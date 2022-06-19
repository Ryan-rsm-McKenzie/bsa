#include "bsa/xmem/hashing.hpp"

#include <span>
#include <string_view>

#include "catch2.hpp"

using namespace std::literals;

namespace hashing = bsa::xmem::hashing;

TEST_CASE("assert hashing correctness", "[src]")
{
	SECTION("SHA512")
	{
		const auto check = [](std::string_view a_in, std::string_view a_out) {
			const auto sha = hashing::sha512(std::as_bytes(std::span{ a_in }));
			REQUIRE(sha);
			REQUIRE(sha == a_out);
		};

		check(
			"hello world!"sv,
			"DB9B1CD3262DEE37756A09B9064973589847CAA8E53D31A9D142EA2701B1B28ABD97838BB9A27068BA305DC8D04A45A1FCF079DE54D607666996B3CC54F6B67C"sv);
		check(
			"the quick brown fox"sv,
			"D9D380F29B97AD6A1D92E987D83FA5A02653301E1006DD2BCD51AFA59A9147E9CAEDAF89521ABC0F0B682ADCD47FB512B8343C834A32F326FE9BEF00542CE887"sv);
		check(
			"I can go no further. You alone must stand against the Prince of Destruction and his Mortal Servants."sv,
			"956B6161C8102EE24DF0C059D9EDA45465D9951EC364E6CA94C61B27322776215E70139D6EBB3200DF9772B3E6DC236931A98457F39A86C819FA42E29445BE79"sv);
		check(
			"Do you get to the cloud district very often? Oh, what am I saying? Of course you don't."sv,
			"7E3F039B11BC69165FC4E83AA166BC2B27C060CB3123ED5392585FEB7C412F31463CE022C70D040F07A7F82490480B68FFC1D1492C003F6F235555611D7F7859"sv);
	}
}
