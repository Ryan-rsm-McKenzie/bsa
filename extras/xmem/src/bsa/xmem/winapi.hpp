#pragma once

#include <cassert>
#include <utility>

#include <Windows.h>

namespace bsa::xmem::winapi
{
	class hmodule_wrapper
	{
	public:
		using value_type = ::HMODULE;

		hmodule_wrapper() noexcept :
			hmodule_wrapper(nullptr)
		{}

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

		[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
		[[nodiscard]] value_type get() const noexcept { return _module; }
		[[nodiscard]] bool has_value() const noexcept { return _module != nullptr; }

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
