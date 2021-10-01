#include "bsa/xmem/util.hpp"

#include <catch2/catch.hpp>

namespace util = bsa::xmem::util;

TEST_CASE("validate pointer adjuster", "[src]")
{
	const auto check = [](std::uintptr_t a_base,
						   std::ptrdiff_t a_offset,
						   std::uintptr_t a_expected) {
		const void* const ptr = reinterpret_cast<void*>(a_base);
		const auto result = reinterpret_cast<std::uintptr_t>(util::adjust_pointer<void>(ptr, a_offset));
		REQUIRE(result == a_expected);
	};

	check(100, +10, 110);
	check(100, -10, 90);
}
