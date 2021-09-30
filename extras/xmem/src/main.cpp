#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
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
#include "bsa/xmem/version.hpp"
#include "bsa/xmem/xcompress.hpp"
#include "bsa/xmem/xmem.hpp"

using namespace std::literals;

namespace api = bsa::xmem::api;
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
			decompress
		};

		template <std::size_t>
		struct subcommand
		{
			std::filesystem::path src;
			std::filesystem::path dst;
		};

		std::variant<
			std::monostate,
			subcommand<compress>,
			subcommand<decompress>>
			command;
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
		-> std::optional<options>
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

		try {
			p.ParseArgs(a_args);
		} catch (const args::Help&) {
			std::cout << p << '\n';
			return std::nullopt;
		} catch (const args::ParseError& a_err) {
			std::cout << a_err.what() << '\n'
					  << p << '\n';
			return std::nullopt;
		} catch (const args::ValidationError&) {
			std::cout << p << '\n';
			return std::nullopt;
		} catch (const version_signal&) {
			std::cout << "xmem v"sv << version::full << '\n';
			return std::nullopt;
		} catch (const std::exception& a_err) {
			std::cout << a_err.what() << '\n';
			return std::nullopt;
		} catch (...) {
			std::cout << p << '\n';
			return std::nullopt;
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

	[[nodiscard]] auto do_compress(
		std::filesystem::path a_src,
		std::filesystem::path a_dst) noexcept
		-> xmem::expected<std::monostate>
	{
		const auto context = api::create_compression_context();
		if (!context) {
			return xmem::unexpected(context.error());
		}

		const mmio::mapped_file_source in{ std::move(a_src) };
		const auto size = api::compress_bound(context->get(), std::span{ in.data(), in.size() });
		if (!size) {
			return xmem::unexpected(size.error());
		}

		std::vector<std::byte> dst(*size);
		const auto realsz = api::compress(context->get(), std::span{ in.data(), in.size() }, dst);
		if (!realsz) {
			return xmem::unexpected(realsz.error());
		}

		dst.resize(*realsz);
		const auto out = open_file(a_dst.c_str(), L"wb");
		std::fwrite(dst.data(), 1, dst.size(), out.get());

		return std::monostate{};
	}

	[[nodiscard]] auto do_decompress(
		std::filesystem::path a_src,
		std::filesystem::path a_dst) noexcept
		-> xmem::expected<std::monostate>
	{
		const auto context = api::create_decompression_context();
		if (!context) {
			return xmem::unexpected(context.error());
		}

		const mmio::mapped_file_source in{ std::move(a_src) };
		std::vector<std::byte> dst(in.size() * 2);
		const auto realsz = api::decompress(context->get(), std::span{ in.data(), in.size() }, dst);
		if (!realsz) {
			return xmem::unexpected(realsz.error());
		}

		dst.resize(*realsz);
		const auto out = open_file(a_dst.c_str(), L"wb");
		std::fwrite(dst.data(), 1, dst.size(), out.get());

		return std::monostate{};
	}
}

int do_main(std::span<const std::string> a_args)
{
	const auto options = parse_commands(a_args);
	if (!options) {
		return EXIT_FAILURE;
	}

	if (!xcompress::initialize()) {
		return EXIT_FAILURE;
	}

#define CASE(a_method)                                                              \
	case options::##a_method:                                                       \
		{                                                                           \
			const auto& cmd = *std::get_if<options::##a_method>(&options->command); \
			const auto result = do_##a_method(                                      \
				std::move(cmd.src),                                                 \
				std::move(cmd.dst));                                                \
			if (!result) {                                                          \
				std::cout                                                           \
					<< "failed to " #a_method " file with error: "                  \
					<< xmem::to_string(result.error())                              \
					<< '\n';                                                        \
				return EXIT_FAILURE;                                                \
			}                                                                       \
		}                                                                           \
		break

	switch (options->command.index()) {
		CASE(compress);
		CASE(decompress);
	default:
		return EXIT_FAILURE;
	}

#undef CASE

	return EXIT_SUCCESS;
}

#ifndef TESTING
int main(int a_argc, char* a_argv[])
{
	const std::vector<std::string> args(a_argv, a_argv + a_argc);
	return do_main(args);
}
#endif
