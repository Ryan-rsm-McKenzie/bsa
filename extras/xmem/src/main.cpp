#include <cstdlib>
#include <iostream>

#include "bsa/xmem/version.hpp"

namespace version = bsa::xmem::version;

static_assert(sizeof(void*) == 0x4, "the xmem proxy must be built as a 32-bit application");

int main()
{
	std::cout << version::full << '\n';

	return EXIT_SUCCESS;
}
