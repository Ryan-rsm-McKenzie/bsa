#pragma once

#if defined(_WIN32)
#	define NOGDI         // workaround for old windows kits + new preprocessor
#	include <Windows.h>  // ensure windows.h compatibility
#endif

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

#include <bsa/detail/common.hpp>

using namespace std::literals;

template <class T>
consteval bool assert_nothrowable() noexcept
{
	static_assert(std::is_nothrow_default_constructible_v<T>);
	static_assert(std::is_nothrow_copy_constructible_v<T>);
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);
	static_assert(std::is_nothrow_copy_assignable_v<T>);
	static_assert(std::is_nothrow_move_assignable_v<T>);

	return true;
}

[[nodiscard]] inline auto map_file(std::filesystem::path a_path)
	-> bsa::detail::istream_t::stream_type
{
	bsa::detail::istream_t::stream_type stream;
	stream.open(std::move(a_path));
	return stream;
}

[[nodiscard]] inline auto open_file(
	std::filesystem::path a_path,
	const char* a_mode)
{
	const auto close = [](std::FILE* a_file) noexcept {
		std::fclose(a_file);
	};

	std::filesystem::create_directories(a_path.parent_path());
	auto result = std::unique_ptr<std::FILE, decltype(close)>{
		bsa::detail::unicode::fopen(a_path, a_mode),
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
