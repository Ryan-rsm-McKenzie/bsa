#include "utility.hpp"

#include <array>
#include <filesystem>
#include <system_error>
#include <utility>

#include "catch2.hpp"

#include "bsa/detail/common.hpp"

TEST_CASE("bsa::functional", "[src][common]")
{
	SECTION("guess_file_format")
	{
		const std::filesystem::path root{ "common_guess_test"sv };

#define FORMAT(a_fmt, a_ext) std::make_pair(#a_fmt "." #a_ext##sv, bsa::file_format::a_fmt)
		std::array formats{
			FORMAT(fo4, ba2),
			FORMAT(tes3, bsa),
			FORMAT(tes4, bsa),
		};
#undef FORMAT

		for (const auto& [name, type] : formats) {
			const auto guessed = bsa::guess_file_format(root / name);
			REQUIRE(guessed);
			REQUIRE(*guessed == type);
		}

		REQUIRE(!bsa::guess_file_format(root / "data/misc/example.txt"sv));
		REQUIRE_THROWS_AS(bsa::guess_file_format(root / "foo.bar"sv), std::system_error);
	}

	SECTION("make_four_cc")
	{
		REQUIRE(bsa::make_four_cc(""sv) == 0x00000000);
		REQUIRE(bsa::make_four_cc("A"sv) == 0x00000041);
		REQUIRE(bsa::make_four_cc("AB"sv) == 0x00004241);
		REQUIRE(bsa::make_four_cc("ABC"sv) == 0x00434241);
		REQUIRE(bsa::make_four_cc("ABCD"sv) == 0x44434241);
		REQUIRE(bsa::make_four_cc("ABCDE"sv) == 0x44434241);
	}
}
