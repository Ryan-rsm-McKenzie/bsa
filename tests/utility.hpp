#pragma once

#if defined(_WIN32)
#	define NOGDI         // workaround for old windows kits + new preprocessor
#	include <Windows.h>  // ensure windows.h compatibility
#endif

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <binary_io/any_stream.hpp>
#include <binary_io/common.hpp>
#include <binary_io/memory_stream.hpp>
#include <catch2/catch.hpp>
#include <mmio/mmio.hpp>

#include <bsa/detail/common.hpp>

using namespace std::literals;

template <class T, bool DefaultConstructible = true>
consteval bool assert_nothrowable() noexcept
{
	if constexpr (DefaultConstructible) {
		static_assert(std::is_nothrow_default_constructible_v<T>);
	}

	static_assert(std::is_nothrow_copy_constructible_v<T>);
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);
	static_assert(std::is_nothrow_copy_assignable_v<T>);
	static_assert(std::is_nothrow_move_assignable_v<T>);

	return true;
}

[[nodiscard]] inline auto map_file(std::filesystem::path a_path)
	-> mmio::mapped_file_source
{
	mmio::mapped_file_source stream;
	stream.open(std::move(a_path));
	REQUIRE(stream.is_open());
	return stream;
}

[[nodiscard]] inline auto map_files(
	std::filesystem::path a_root,
	std::span<const std::string_view> a_paths)
	-> std::vector<std::pair<std::string_view, mmio::mapped_file_source>>
{
	std::vector<std::pair<std::string_view, mmio::mapped_file_source>> result;
	for (const auto& path : a_paths) {
		result.emplace_back(path, map_file(a_root / path));
	}
	return result;
}

[[nodiscard]] inline auto open_file(
	std::filesystem::path a_path,
	const char* a_mode)
{
	const auto close = [](std::FILE* a_file) noexcept {
		std::fclose(a_file);
	};

	if (a_path.has_parent_path()) {
		std::filesystem::create_directories(a_path.parent_path());
	}

	auto result = std::unique_ptr<std::FILE, decltype(close)>{
		std::fopen(a_path.string().c_str(), a_mode),
		close
	};
	return result;
}

inline auto simple_normalize(std::string_view a_path) noexcept
	-> std::string
{
	std::string result(a_path);
	for (auto& c : result) {
		c = c == '/' ? '\\' : static_cast<char>(std::tolower(c));
	}
	return result;
}

inline auto make_substr_matcher(std::string_view a_str) noexcept
{
	std::string pattern;
	pattern += ".*\\b"sv;
	pattern += a_str;
	pattern += "\\b.*"sv;
	return Catch::Matchers::Matches(pattern, Catch::CaseSensitive::No);
}

[[nodiscard]] inline auto split_string(std::string_view a_str, char a_delim)
	-> std::pair<std::string_view, std::string_view>
{
	const auto pos = a_str.find_first_of(a_delim);
	return {
		a_str.substr(0, pos),
		a_str.substr(pos + 1)
	};
}

template <class Archive>
void test_in_memory_buffer(
	std::string_view a_archiveName,
	std::function<void(Archive&, std::span<const std::pair<std::string_view, mmio::mapped_file_source>>)> a_addFiles,
	std::function<void(Archive&, std::filesystem::path)> a_writeDisk,
	std::function<void(Archive&, binary_io::any_ostream&)> a_writeMemory,
	std::function<void(Archive&, std::span<const std::byte>, bsa::copy_type)> a_readMemory)
{
	const std::filesystem::path root{ "in_memory_test"sv };
	const std::filesystem::path archivePath = root / a_archiveName;
	const auto map_disk = [&]() {
		return map_file(archivePath);
	};

	{
		constexpr std::array paths{
			"Background/background_tilemap.png"sv,
			"Characters/character_0008.png"sv,
			"Share/License.txt"sv,
			"Tilemap/characters.png"sv,
			"Tiles/tile_0007.png"sv,
		};

		const auto files = map_files(root / "data"sv, std::span{ paths });

		Archive archive;
		a_addFiles(archive, std::span{ files });
		a_writeDisk(archive, archivePath);
	}

	for (auto copyType : { bsa::copy_type::shallow, bsa::copy_type::deep }) {
		auto disk = map_disk();
		Archive archive;
		a_readMemory(archive, std::span{ disk.data(), disk.size() }, copyType);

		if (copyType == bsa::copy_type::deep) {
			disk.close();  // close stream to assert deep copies are actually being made
		}
		binary_io::any_ostream anyOS{ std::in_place_type<binary_io::memory_ostream> };
		a_writeMemory(archive, anyOS);

		const auto& memOS = anyOS.get<binary_io::memory_ostream>().rdbuf();
		const auto memory = std::span{ memOS.data(), memOS.size() };

		if (!disk.is_open()) {
			disk = map_disk();
		}
		REQUIRE(disk.size() == memory.size());
		REQUIRE(std::memcmp(disk.data(), memory.data(), disk.size()) == 0);
	}
}
