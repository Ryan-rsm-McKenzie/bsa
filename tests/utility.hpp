#pragma once

#include <boost/predef.h>
#if BOOST_OS_WINDOWS
#	define NOGDI
#	include <Windows.h>  // ensure windows.h compatibility
#endif

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

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

[[nodiscard]] inline auto map_file(const std::filesystem::path& a_path)
	-> boost::iostreams::mapped_file_source
{
	return boost::iostreams::mapped_file_source{
		boost::filesystem::path{ a_path.native() }
	};
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
