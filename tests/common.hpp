#pragma once

#include <filesystem>
#include <type_traits>

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

[[nodiscard]] inline auto map_file(const std::filesystem::path& a_path)
	-> boost::iostreams::mapped_file_source
{
	return boost::iostreams::mapped_file_source{
		boost::filesystem::path{ a_path.native() }
	};
}

template <class T>
consteval bool assert_nothrowable() noexcept
{
	if constexpr (std::is_default_constructible_v<T>) {
		static_assert(std::is_nothrow_default_constructible_v<T>);
	}

	if constexpr (std::is_copy_constructible_v<T>) {
		static_assert(std::is_nothrow_copy_constructible_v<T>);
	}

	if constexpr (std::is_move_constructible_v<T>) {
		static_assert(std::is_nothrow_move_constructible_v<T>);
	}

	if constexpr (std::is_destructible_v<T>) {
		static_assert(std::is_nothrow_destructible_v<T>);
	}

	if constexpr (std::is_copy_assignable_v<T>) {
		static_assert(sizeof(T) && std::is_nothrow_copy_assignable_v<T>);
	}

	if constexpr (std::is_move_assignable_v<T>) {
		static_assert(sizeof(T) && std::is_nothrow_move_assignable_v<T>);
	}

	return true;
}
