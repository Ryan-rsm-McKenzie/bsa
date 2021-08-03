#pragma once

#include <functional>

#include <bsa/bsa.hpp>

using namespace std::literals;

namespace bsa::all {

enum class version : std::uint32_t {
    tes3 = 1,
    tes4 = bsa::detail::to_underlying(bsa::tes4::version::tes4),
    fo3 = bsa::detail::to_underlying(bsa::tes4::version::fo3),
    tes5 = bsa::detail::to_underlying(bsa::tes4::version::tes5),
    sse = bsa::detail::to_underlying(bsa::tes4::version::sse),
    fo4 = bsa::detail::to_underlying(bsa::fo4::format::general),
    fo4dx = bsa::detail::to_underlying(bsa::fo4::format::directx),
};

using underlying_archive = std::variant<bsa::tes3::archive, bsa::tes4::archive, bsa::fo4::archive>;

class archive
{
public:
    archive(const std::filesystem::path &a_path); // Read
    archive(version a_version, bool a_compressed);

    auto read(const std::filesystem::path &a_path) -> version;
    void write(std::filesystem::path a_path);

    void add_file(const std::filesystem::path &a_root, const std::filesystem::path &a_path);
    void add_file(const std::filesystem::path &a_relative, std::vector<std::byte> a_data);

    using iteration_callback
        = std::function<void(const std::filesystem::path &, std::span<const std::byte>)>;
    void iterate_files(const iteration_callback &a_callback, bool skip_compressed = false);

private:
    underlying_archive _archive;
    version _version;
    bool _compressed;
};

} // namespace bsa::all
