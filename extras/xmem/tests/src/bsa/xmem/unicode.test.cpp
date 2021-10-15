#include "bsa/xmem/unicode.hpp"

#include <cstring>
#include <string>

#include <catch2/catch.hpp>

namespace unicode = bsa::xmem::unicode;

using namespace std::literals;

TEST_CASE("validate unicode conversions", "[src]")
{
#define TEST_STRING(a_string)                                                    \
	do {                                                                         \
		const auto utf8 = u8##a_string##sv;                                      \
		const auto utf16 = u##a_string##sv;                                      \
		const auto result = unicode::utf16_to_utf8(                              \
			{ reinterpret_cast<const wchar_t*>(utf16.data()), utf16.length() }); \
		REQUIRE(result);                                                         \
		REQUIRE(utf8.length() == result->length());                              \
		REQUIRE(std::memcmp(utf8.data(), result->data(), utf8.length()) == 0);   \
	} while (false)

	TEST_STRING("ⅶ⎷⳨⿿⤟♾┱ⲟ✂✟⯛⑉⟿⸕℧ℶ⑉⭣Ⱬ⩬");
	TEST_STRING("⺛⍽⡐⸓┦⍿⋴⤏ⲩ⷗⮇⢥ⶻ⻍⇉⢝⊛⇞❩⾼");
	TEST_STRING("⢑₁≧Ⲍ⋱⻪ⶰ℄↏∣⠠⮠♅⋒␫ⰹⱻ◸⧁⿑");
	TEST_STRING("╔⢟⚨⌸⧏⸪⾰Ⱌⷛ⍬⭳⬾⒧␲⟌℁⻜⢝ℴ➅");
	TEST_STRING("₢⮐⭪⭗ⶁ╌⮛⥦⚟┉⁺⇞ↀ〉₤⡻ℨ⢇₽₉");
}
