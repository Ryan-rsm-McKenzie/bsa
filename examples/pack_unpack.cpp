#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <bsa/all.hpp>

using namespace std::literals;

namespace {
template<class UnaryFunction>
void for_each_file(const std::filesystem::path &a_root, UnaryFunction a_func)
{
    for (const auto &entry : std::filesystem::recursive_directory_iterator(a_root)) {
        if (entry.is_regular_file()) {
            a_func(entry.path());
        }
    }
}

template<class... Keys>
[[nodiscard]] auto open_virtual_path(const std::filesystem::path &a_root,
                                     const std::filesystem::path &relative) -> std::ofstream
{
    const auto path = a_root / relative;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out{path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};
    out.exceptions(std::ios_base::failbit);
    return out;
}

struct args_t
{
    bool pack{false};
    std::filesystem::path input;
    std::filesystem::path output;
    bsa::all::version format{bsa::all::version::tes4};
};

void print_usage()
{
    std::cout << "pack_unpack pack <input-directory> <output-archive> {-tes3|-tes4|-fo4}\n"
              << "pack_unpack unpack <input-archive> <output-directory>\n"
              << '\n';
}

template<class... Args>
[[noreturn]] void concat_and_throw(Args &&...a_args)
{
    std::string msg;
    ((msg += a_args), ...);
    throw std::runtime_error(msg);
}

[[nodiscard]] auto parse_arguments(std::span<const char *> a_args) -> args_t
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
                    args.format = bsa::all::version::tes3;
                } else if (arg == "-tes4"sv) {
                    args.format = bsa::all::version::tes4;
                } else if (arg == "-tes5"sv) {
                    args.format = bsa::all::version::tes5;
                } else if (arg == "-sse"sv) {
                    args.format = bsa::all::version::sse;
                } else if (arg == "-fo3"sv) {
                    args.format = bsa::all::version::fo3;
                } else if (arg == "-fo4"sv) {
                    args.format = bsa::all::version::fo4;
                } else if (arg == "-fo4dx"sv) {
                    args.format = bsa::all::version::fo4dx;
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
    } catch (const std::runtime_error &a_err) {
        print_usage();
        throw a_err;
    }
}

void do_main(int a_argc, const char *a_argv[])
{
    const auto args = parse_arguments({a_argv, static_cast<std::size_t>(a_argc)});
    if (args.pack) {
        bsa::all::archive arch(args.format, true);
        for_each_file(args.input, [&](const std::filesystem::path &path) {
            arch.add_file(args.input, path);
        });
        arch.write(args.output);
    } else {
        bsa::all::archive arch(args.input);
        arch.iterate_files(
            [&](const std::filesystem::path &relative, std::span<const std::byte> data) {
                auto out = open_virtual_path(args.output, relative);
                out.write(reinterpret_cast<const char *>(data.data()), data.size());
            });
    }
}

} // namespace

#ifndef TESTING
int main(int a_argc, const char *a_argv[])
{
    try {
        do_main(a_argc, a_argv);
        return EXIT_SUCCESS;
    } catch (const std::exception &a_err) {
        std::cerr << a_err.what() << '\n';
        return EXIT_FAILURE;
    }
}
#endif
