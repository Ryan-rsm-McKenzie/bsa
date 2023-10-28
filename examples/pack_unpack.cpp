#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <binary_io/any_stream.hpp>
#include <binary_io/file_stream.hpp>
#include <bsa/bsa.hpp>

using namespace std::literals;

namespace
{
	template <class UnaryFunction>
	void for_each_file(const std::filesystem::path& a_root, UnaryFunction a_func)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(a_root)) {
			if (entry.is_regular_file()) {
				a_func(entry.path());
			}
		}
	}

	template <class... Keys>
	[[nodiscard]] auto virtual_to_local_path(Keys&&... a_keys)
		-> std::string
	{
		std::string local;
		((local += a_keys.name(), local += '/'), ...);
		local.pop_back();

		for (auto& c : local) {
			if (c == '\\' || c == '/') {
				c = std::filesystem::path::preferred_separator;
			}
		}

		return local;
	}

	template <class... Keys>
	[[nodiscard]] auto open_virtual_path(const std::filesystem::path& a_root, Keys&&... a_keys)
		-> binary_io::any_ostream
	{
		const auto relative = virtual_to_local_path(std::forward<Keys>(a_keys)...);
		const auto path = a_root / relative;
		std::filesystem::create_directories(path.parent_path());
		return binary_io::any_ostream{ std::in_place_type<binary_io::file_ostream>, std::move(path) };
	}

	void pack_fo4(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		bsa::fo4::archive ba2;
		for_each_file(
			a_input,
			[&](const std::filesystem::path& a_path) {
				bsa::fo4::file f;
				f.read(
					a_path,
					{
						.format = bsa::fo4::format::general,
						.compression_type = bsa::compression_type::compressed,
					});

				ba2.insert(
					a_path
						.lexically_relative(a_input)
						.lexically_normal()
						.generic_string(),
					std::move(f));
			});
		ba2.write(a_output, { .format = bsa::fo4::format::general });
	}

	void pack_tes3(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		bsa::tes3::archive bsa;
		for_each_file(
			a_input,
			[&](const std::filesystem::path& a_path) {
				bsa::tes3::file f;
				f.read(a_path);

				bsa.insert(
					a_path
						.lexically_relative(a_input)
						.lexically_normal()
						.generic_string(),
					std::move(f));
			});
		bsa.write(a_output);
	}

	void pack_tes4(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		const auto version = bsa::tes4::version::tes4;
		bsa::tes4::archive bsa;
		bsa.archive_flags(
			bsa::tes4::archive_flag::compressed |
			bsa::tes4::archive_flag::directory_strings |
			bsa::tes4::archive_flag::file_strings);
		for_each_file(
			a_input,
			[&](const std::filesystem::path& a_path) {
				bsa::tes4::file f;
				f.read(
					a_path,
					{
						.version = version,
						.compression_type = bsa::compression_type::compressed,
					});

				const auto d = [&]() {
					const auto key =
						a_path
							.parent_path()
							.lexically_relative(a_input)
							.lexically_normal()
							.generic_string();
					if (bsa.find(key) == bsa.end()) {
						bsa.insert(key, bsa::tes4::directory());
					}
					return bsa[key];
				}();

				d->insert(
					a_path
						.filename()
						.lexically_normal()
						.generic_string(),
					std::move(f));
			});
		bsa.write(a_output, version);
	}

	void unpack_fo4(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		bsa::fo4::archive ba2;
		const auto meta = ba2.read(a_input);

		for (auto& [key, file] : ba2) {
			auto out = open_virtual_path(a_output, key);
			file.write(out, { .format = meta.format, .compression_format = meta.compression_format });
		}
	}

	void unpack_tes3(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		bsa::tes3::archive bsa;
		bsa.read(a_input);

		for (const auto& [key, file] : bsa) {
			auto out = open_virtual_path(a_output, key);
			file.write(out);
		}
	}

	void unpack_tes4(
		const std::filesystem::path& a_input,
		const std::filesystem::path& a_output)
	{
		bsa::tes4::archive bsa;
		const auto format = bsa.read(a_input);

		for (auto& dir : bsa) {
			for (auto& file : dir.second) {
				auto out = open_virtual_path(a_output, dir.first, file.first);
				file.second.write(out, { .version = format });
			}
		}
	}

	struct args_t
	{
		bool pack{ false };
		std::filesystem::path input;
		std::filesystem::path output;
		bsa::file_format format{ bsa::file_format::tes4 };
	};

	void print_usage()
	{
		std::cout
			<< "pack_unpack pack <input-directory> <output-archive> {-tes3|-tes4|-fo4}\n"
			<< "pack_unpack unpack <input-archive> <output-directory>\n"
			<< '\n';
	}

	template <class... Args>
	[[noreturn]] void concat_and_throw(Args&&... a_args)
	{
		std::string msg;
		((msg += a_args), ...);
		throw std::runtime_error(msg);
	}

	[[nodiscard]] auto parse_arguments(std::span<const char*> a_args)
		-> args_t
	{
		try {
			if (a_args.size() <= 3) {
				throw std::runtime_error("too few arguments");
			}

			args_t args;
			args.input = a_args[2];
			args.output = a_args[3];
			args.pack = [&]() {
				const auto arg = a_args[1];
				if (arg == "pack"sv) {
					return true;
				} else if (arg == "unpack"sv) {
					return false;
				} else {
					concat_and_throw("unrecognized operation: "sv, arg);
				}
			}();

			const std::size_t expected = [&]() {
				if (args.pack) {
					if (a_args.size() <= 4) {
						throw std::runtime_error("too few arguments");
					}

					const auto arg = a_args[4];
					if (arg == "-tes3"sv) {
						args.format = bsa::file_format::tes3;
					} else if (arg == "-tes4"sv) {
						args.format = bsa::file_format::tes4;
					} else if (arg == "-fo4"sv) {
						args.format = bsa::file_format::fo4;
					} else {
						concat_and_throw("unrecognized format: "sv, arg);
					}

					return 4;
				} else {
					return 3;
				}
			}();

			if (a_args.size() > expected + 1u) {
				throw std::runtime_error("too many arguments");
			}

			return args;
		} catch (const std::runtime_error& a_err) {
			print_usage();
			throw a_err;
		}
	}

	void do_main(int a_argc, const char* a_argv[])
	{
		const auto args = parse_arguments({ a_argv, static_cast<std::size_t>(a_argc) });
		if (args.pack) {
			switch (args.format) {
			case bsa::file_format::fo4:
				pack_fo4(args.input, args.output);
				break;
			case bsa::file_format::tes3:
				pack_tes3(args.input, args.output);
				break;
			case bsa::file_format::tes4:
				pack_tes4(args.input, args.output);
				break;
			default:
				throw std::runtime_error("unhandled format");
			}
		} else {
			const auto format = bsa::guess_file_format(args.input);
			if (!format) {
				throw std::runtime_error("file is not an archive");
			}

			switch (*format) {
			case bsa::file_format::fo4:
				unpack_fo4(args.input, args.output);
				break;
			case bsa::file_format::tes3:
				unpack_tes3(args.input, args.output);
				break;
			case bsa::file_format::tes4:
				unpack_tes4(args.input, args.output);
				break;
			default:
				throw std::runtime_error("unhandled format");
			}
		}
	}
}

#ifndef TESTING
int main(int a_argc, const char* a_argv[])
{
	try {
		do_main(a_argc, a_argv);
		return EXIT_SUCCESS;
	} catch (const std::exception& a_err) {
		std::cerr << a_err.what() << '\n';
		return EXIT_FAILURE;
	}
}
#endif
