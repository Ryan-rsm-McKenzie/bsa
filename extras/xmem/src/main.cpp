#include <cstdlib>

static_assert(sizeof(void*) == 0x4, "the xmem proxy must be built as a 32-bit application");

int main()
{
	return EXIT_SUCCESS;
}
