#pragma once

#include <cassert>
#include <exception>
#include <utility>

#include <Windows.h>

namespace bsa::xmem::winapi
{
	template <class T>
	class win32_error :
		public std::exception
	{
	public:
		using value_type = T;

		win32_error(
			const char* a_what,
			value_type a_error) noexcept :
			_what(a_what),
			_error(a_error)
		{}

		const char* what() const noexcept override { return _what; }

		[[nodiscard]] value_type error_code() const noexcept { return _error; }

	private:
		const char* _what = nullptr;
		value_type _error = {};
	};

	using hresult_error = win32_error<::HRESULT>;
	using lstatus_error = win32_error<::LSTATUS>;
	using nt_error = win32_error<::NTSTATUS>;

	class hmodule_wrapper
	{
	public:
		using value_type = ::HMODULE;

		explicit hmodule_wrapper(value_type a_module) noexcept :
			_module(a_module)
		{}

		hmodule_wrapper(const hmodule_wrapper&) = delete;
		hmodule_wrapper(hmodule_wrapper&& a_rhs) noexcept :
			_module(std::exchange(a_rhs._module, nullptr))
		{}

		~hmodule_wrapper() noexcept
		{
			if (_module) {
				[[maybe_unused]] const auto result = ::FreeLibrary(
					std::exchange(_module, nullptr));
				assert(result != 0);
			}
		}

		hmodule_wrapper& operator=(const hmodule_wrapper&) = delete;
		hmodule_wrapper& operator=(hmodule_wrapper&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_module = std::exchange(a_rhs._module, nullptr);
			}
			return *this;
		}

		[[nodiscard]] explicit operator bool() const noexcept { return _module != nullptr; }
		[[nodiscard]] value_type get() const noexcept { return _module; }

	private:
		value_type _module{ nullptr };
	};

	[[nodiscard]] inline bool hresult_success(::HRESULT a_result) noexcept
	{
		return HRESULT_SEVERITY(a_result) == 0;
	}

	[[nodiscard]] inline bool nt_success(::NTSTATUS a_status) noexcept
	{
		return a_status >= 0;
	}
}
