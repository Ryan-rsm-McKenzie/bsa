#pragma once

#include <nonstd/expected.hpp>

#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem
{
	template <class T>
	using expected = nonstd::expected<T, xmem::error_code>;

#ifdef __EDG__
	template <class E>
	[[nodiscard]] auto unexpected(E) noexcept -> nonstd::unexpected_type<E>;
#else
	using nonstd::unexpected;
#endif

#define WRAP_EXPECTED(a_value)                        \
	do {                                              \
		if (!a_value) {                               \
			return xmem::unexpected(a_value.error()); \
		}                                             \
	} while (false)

#define UNWRAP_EXPECTED(a_value)    \
	do {                            \
		if (!a_value) {             \
			return a_value.error(); \
		}                           \
	} while (false)
}
