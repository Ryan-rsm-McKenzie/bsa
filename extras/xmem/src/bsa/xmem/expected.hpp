#pragma once

#include <nonstd/expected.hpp>

#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem
{
	template <class T>
	using expected = nonstd::expected<T, xmem::error_code>;

	using nonstd::unexpected;
}
