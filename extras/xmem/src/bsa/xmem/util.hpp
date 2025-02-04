#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace bsa::xmem::util
{
	template <class EF>                                        //
		requires(std::invocable<std::remove_reference_t<EF>>)  //
	class scope_exit
	{
	public:
		// 1)
		template <class Fn>
		explicit scope_exit(Fn&& a_fn)  //
			noexcept(std::is_nothrow_constructible_v<EF, Fn> ||
					 std::is_nothrow_constructible_v<EF, Fn&>)  //
			requires(!std::is_same_v<std::remove_cvref_t<Fn>, scope_exit> &&
					 std::is_constructible_v<EF, Fn>)
		{
			static_assert(std::invocable<Fn>);

			if constexpr (!std::is_lvalue_reference_v<Fn> &&
						  std::is_nothrow_constructible_v<EF, Fn>) {
				_fn.emplace(std::forward<Fn>(a_fn));
			} else {
				_fn.emplace(a_fn);
			}
		}

		// 2)
		scope_exit(scope_exit&& a_rhs)  //
			noexcept(std::is_nothrow_move_constructible_v<EF> ||
					 std::is_nothrow_copy_constructible_v<EF>)  //
			requires(std::is_nothrow_move_constructible_v<EF> ||
					 std::is_copy_constructible_v<EF>)
		{
			static_assert(!(std::is_nothrow_move_constructible_v<EF> && !std::is_move_constructible_v<EF>));
			static_assert(!(!std::is_nothrow_move_constructible_v<EF> && !std::is_copy_constructible_v<EF>));

			if (a_rhs.active()) {
				if constexpr (std::is_nothrow_move_constructible_v<EF>) {
					_fn.emplace(std::forward<EF>(*a_rhs._fn));
				} else {
					_fn.emplace(a_rhs._fn);
				}
				a_rhs.release();
			}
		}

		// 3)
		scope_exit(const scope_exit&) = delete;

		~scope_exit() noexcept
		{
			if (_fn.has_value()) {
				(*_fn)();
			}
		}

		void release() noexcept { _fn.reset(); }

	private:
		[[nodiscard]] bool active() const noexcept { return _fn.has_value(); }

		std::optional<std::remove_reference_t<EF>> _fn;
	};

	template <class EF>
	scope_exit(EF) -> scope_exit<EF>;

	template <class T, class U>
	[[nodiscard]] auto adjust_pointer(U* a_ptr, std::ptrdiff_t a_adjust) noexcept
	{
		const auto addr = a_ptr ? reinterpret_cast<std::uintptr_t>(a_ptr) + a_adjust : 0;
		if constexpr (std::is_const_v<U> && std::is_volatile_v<U>) {
			return reinterpret_cast<std::add_cv_t<T>*>(addr);
		} else if constexpr (std::is_const_v<U>) {
			return reinterpret_cast<std::add_const_t<T>*>(addr);
		} else if constexpr (std::is_volatile_v<U>) {
			return reinterpret_cast<std::add_volatile_t<T>*>(addr);
		} else {
			return reinterpret_cast<T*>(addr);
		}
	}
}
