#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <Windows.h>

#include <args.hxx>
#include <mmio/mmio.hpp>

#include "bsa/xmem/api.hpp"
#include "bsa/xmem/binary_stdio.hpp"
#include "bsa/xmem/version.hpp"
#include "bsa/xmem/xcompress.hpp"
#include "bsa/xmem/xmem.hpp"

using namespace std::literals;

namespace api = bsa::xmem::api;
namespace binary_stdio = bsa::xmem::binary_stdio;
namespace version = bsa::xmem::version;
namespace xcompress = bsa::xmem::xcompress;
namespace xmem = bsa::xmem;

static_assert(sizeof(void*) == 0x4, "the xmem proxy must be built as a 32-bit application");

namespace
{
	struct options
	{
		enum
		{
			none,
			compress,
			decompress,
			serve,

			total
		};

		template <std::size_t>
		struct subcommand
		{
			std::filesystem::path src;
			std::filesystem::path dst;
		};

		struct serve_command
		{};

		std::variant<
			std::monostate,
			subcommand<compress>,
			subcommand<decompress>,
			serve_command>
			command;

		static_assert(std::variant_size_v<decltype(command)> == total);
	};

	class version_signal :
		public std::exception
	{};

	class version_flag :
		public args::Flag
	{
	private:
		using super = args::Flag;

	public:
		explicit version_flag(args::Group& a_group) :
			super(a_group, "", "Display the version of the program"s, { 'v', "version"s })
		{}

		void ParseValue(const std::vector<std::string>&) override
		{
			throw version_signal();
		}
	};

	[[nodiscard]] auto parse_commands(
		std::span<const std::string> a_args) noexcept
		-> xmem::expected<options>
	{
		args::ArgumentParser p{ "xmem v"s.append(version::full) };
		p.helpParams.descriptionindent = 0;
		p.helpParams.helpindent = 30;
		p.helpParams.progindent = 0;
		p.helpParams.proglineCommand = "[subcommand]"s;
		p.helpParams.proglineOptions = "[options]"s;
		p.helpParams.showTerminator = false;

		p.Prog("USAGE: xmem"s);

		args::Group super{ p, ""s, args::Group::Validators::AtMostOne };
		args::HelpFlag help{ super, ""s, "Print this options list"s, { 'h', "help"s } };
		version_flag version{ super };

		args::Command compress{ super, "compress"s, "Compresses the file <src> and writes the result to <dst>"s };
		compress.Add(help);
		args::Positional<std::string> compressSrc{ compress, "<src>"s, "The source file to compress"s, args::Options::Required };
		args::Positional<std::string> compressDst{ compress, "<dst>"s, "The destination to write the result to"s, args::Options::Required };

		args::Command decompress{ super, "decompress"s, "Decompresses the file <src> and writes the result to <dst>"s };
		decompress.Add(help);
		args::Positional<std::string> decompressSrc{ decompress, "<src>"s, "The source file to decompress"s, args::Options::Required };
		args::Positional<std::string> decompressDst{ decompress, "<dst>"s, "The destination to write the result to"s, args::Options::Required };

		args::Command serve{ super, "serve"s, "Starts the process in server mode, where it listens for requests over stdin"s };
		serve.Add(help);

		try {
			p.ParseArgs(a_args);
		} catch (const args::Help&) {
			std::cout << p << '\n';
			return xmem::unexpected(xmem::error_code::ok);
		} catch (const args::ParseError& a_err) {
			std::cout << a_err.what() << '\n'
					  << p << '\n';
			return xmem::unexpected(xmem::error_code::exit_failure);
		} catch (const args::ValidationError&) {
			std::cout << p << '\n';
			return xmem::unexpected(xmem::error_code::ok);
		} catch (const version_signal&) {
			std::cout << "xmem v"sv << version::full << '\n';
			return xmem::unexpected(xmem::error_code::ok);
		} catch (const std::exception& a_err) {
			std::cout << a_err.what() << '\n';
			return xmem::unexpected(xmem::error_code::exit_failure);
		} catch (...) {
			std::cout << p << '\n';
			return xmem::unexpected(xmem::error_code::exit_failure);
		}

		options o;
#define MAYBE_SET(a_value)                                                             \
	if (a_value##.Matched()) {                                                         \
		assert(o.command.index() == options::none);                                    \
		assert(a_value##Src.Matched());                                                \
		assert(a_value##Dst.Matched());                                                \
		o.command.emplace<options::##a_value>(a_value##Src.Get(), a_value##Dst.Get()); \
	}

		MAYBE_SET(compress);
		MAYBE_SET(decompress);

		if (serve.Matched()) {
			assert(o.command.index() == options::none);
			o.command.emplace<options::serve>();
		}

#undef MAYBE_SET

		return o;
	}

	[[nodiscard]] auto open_file(
		const wchar_t* a_filename,
		const wchar_t* a_mode) noexcept
	{
		const auto close = [](std::FILE* a_file) {
			if (a_file) {
				std::fclose(a_file);
			}
		};

		std::FILE* f = nullptr;
		::_wfopen_s(&f, a_filename, a_mode);

		return std::unique_ptr<std::FILE, decltype(close)>{ f, close };
	}

	[[nodiscard]] auto map_file(
		std::filesystem::path a_path) noexcept
		-> xmem::expected<mmio::mapped_file_source>
	{
		mmio::mapped_file_source file;
		if (file.open(std::move(a_path))) {
			return file;
		} else {
			return xmem::unexpected(xmem::error_code::open_file_failure);
		}
	}

#define CHECK_EXPECTED(a_value)       \
	do {                              \
		if (!a_value) {               \
			return a_value##.error(); \
		}                             \
	} while (false)

	[[nodiscard]] auto do_compress(
		std::filesystem::path a_src,
		std::filesystem::path a_dst) noexcept
		-> xmem::error_code
	{
		const auto context = api::create_compression_context();
		CHECK_EXPECTED(context);

		const auto in = map_file(std::move(a_src));
		CHECK_EXPECTED(in);

		const auto size = api::compress_bound(*context, std::span{ in->data(), in->size() });
		CHECK_EXPECTED(size);

		std::vector<std::byte> dst(*size);
		const auto realsz = api::compress(*context, std::span{ in->data(), in->size() }, dst);
		CHECK_EXPECTED(realsz);

		dst.resize(*realsz);
		const auto out = open_file(a_dst.c_str(), L"wb");
		std::fwrite(dst.data(), 1, dst.size(), out.get());

		return xmem::error_code::ok;
	}

	[[nodiscard]] auto do_decompress(
		std::filesystem::path a_src,
		std::filesystem::path a_dst) noexcept
		-> xmem::error_code
	{
		const auto context = api::create_decompression_context();
		CHECK_EXPECTED(context);

		const auto in = map_file(std::move(a_src));
		CHECK_EXPECTED(in);

		std::vector<std::byte> dst(in->size() * 2);
		const auto realsz = api::decompress(*context, std::span{ in->data(), in->size() }, dst);
		CHECK_EXPECTED(realsz);

		dst.resize(*realsz);
		const auto out = open_file(a_dst.c_str(), L"wb");
		std::fwrite(dst.data(), 1, dst.size(), out.get());

		return xmem::error_code::ok;
	}

	[[nodiscard]] auto serve_compress() noexcept
		-> xmem::error_code
	{
		binary_stdio::bin in;
		xmem::compress_request request;
		in >> request;

		const auto context = api::create_compression_context();
		CHECK_EXPECTED(context);

		std::vector<std::byte> bytes(request.bound);
		const auto realsz = api::compress(*context, request.data.as_bytes(), bytes);
		CHECK_EXPECTED(realsz);
		bytes.resize(*realsz);

		binary_stdio::bout out;
		out << xmem::response_header{};
		out << xmem::compress_response{ std::move(bytes) };

		return xmem::error_code::ok;
	}

	[[nodiscard]] auto serve_compress_bound() noexcept
		-> xmem::error_code
	{
		binary_stdio::bin in;
		xmem::compress_bound_request request;
		in >> request;

		const auto context = api::create_compression_context();
		CHECK_EXPECTED(context);

		const auto bound = api::compress_bound(*context, request.data.as_bytes());
		CHECK_EXPECTED(bound);

		binary_stdio::bout out;
		out << xmem::response_header{};
		out << xmem::compress_bound_response{ *bound };

		return xmem::error_code::ok;
	}

	[[nodiscard]] auto serve_decompress() noexcept
	{
		binary_stdio::bin in;
		xmem::decompress_request request;
		in >> request;

		const auto context = api::create_decompression_context();
		CHECK_EXPECTED(context);

		std::vector<std::byte> bytes(request.original_size);
		const auto size = api::decompress(*context, request.data.as_bytes(), bytes);
		CHECK_EXPECTED(size);

		if (*size != request.original_size) {
			return xmem::error_code::serve_decompress_size_mismatch;
		}

		binary_stdio::bout out;
		out << xmem::response_header{};
		out << xmem::decompress_response{ std::move(bytes) };

		return xmem::error_code::ok;
	}

	[[nodiscard]] auto do_serve() noexcept
		-> xmem::error_code
	{
		if (const auto init = xcompress::initialize(); init != xmem::error_code::ok) {
			return init;
		}

		xmem::request_header header;
		binary_stdio::bin in;
		auto ec = xmem::error_code::ok;

		for (;;) {
			in >> header;
			switch (header.type) {
			case xmem::request_type::exit:
				return xmem::error_code::ok;
			case xmem::request_type::compress:
				ec = serve_compress();
				break;
			case xmem::request_type::compress_bound:
				ec = serve_compress_bound();
				break;
			case xmem::request_type::decompress:
				ec = serve_decompress();
				break;
			default:
				ec = xmem::error_code::serve_unhandled_request;
				break;
			}

			if (ec != xmem::error_code::ok) {
				binary_stdio::bout out;
				out << xmem::response_header{ ec };
				ec = xmem::error_code::ok;
			}
		}
	}
}

xmem::error_code do_main(
	std::span<const std::string> a_args) noexcept
{
	const auto options = parse_commands(a_args);
	CHECK_EXPECTED(options);

	if (const auto init = xcompress::initialize(); init != xmem::error_code::ok) {
		return init;
	}

#define CASE(a_method)                                                              \
	case options::##a_method:                                                       \
		{                                                                           \
			const auto& cmd = *std::get_if<options::##a_method>(&options->command); \
			const auto result = do_##a_method(                                      \
				std::move(cmd.src),                                                 \
				std::move(cmd.dst));                                                \
			if (result != xmem::error_code::ok) {                                   \
				std::cout                                                           \
					<< "failed to " #a_method " file with error: "                  \
					<< xmem::to_string(result)                                      \
					<< '\n';                                                        \
				return xmem::error_code::exit_failure;                              \
			}                                                                       \
		}                                                                           \
		break

	switch (options->command.index()) {
		CASE(compress);
		CASE(decompress);
	case options::serve:
		if (const auto result = do_serve(); result != xmem::error_code::ok) {
			return result;
		}
		break;
	default:
		return xmem::error_code::exit_failure;
	}

#undef CASE

	return xmem::error_code::ok;
}

#ifndef TESTING
int main(int a_argc, char* a_argv[])
{
	const std::vector<std::string> args(a_argv, a_argv + a_argc);
	return static_cast<int>(do_main(args));
}
#endif
