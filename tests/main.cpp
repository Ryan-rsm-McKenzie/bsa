#include "bsa/tes4.hpp"

#include <iterator>

#pragma warning(push)
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#pragma warning(pop)

TEST_CASE("bsa::tes4::archive", "[main]")
{
	bsa::tes4::archive bsa;

	REQUIRE(bsa.empty());
	REQUIRE(bsa.size() == 0);
	REQUIRE(std::distance(bsa.begin(), bsa.end()) == std::ssize(bsa));

	REQUIRE(bsa.archive_flags() == bsa::tes4::archive_flag::none);
	REQUIRE(bsa.archive_types() == bsa::tes4::archive_type::none);

	REQUIRE(!bsa.compressed());
	REQUIRE(!bsa.directory_strings());
	REQUIRE(!bsa.embedded_file_names());
	REQUIRE(!bsa.file_strings());
	REQUIRE(!bsa.retain_directory_names());
	REQUIRE(!bsa.retain_file_name_offsets());
	REQUIRE(!bsa.retain_file_names());
	REQUIRE(!bsa.retain_strings_during_startup());
	REQUIRE(!bsa.xbox_archive());
	REQUIRE(!bsa.xbox_compressed());

	REQUIRE(!bsa.fonts());
	REQUIRE(!bsa.menus());
	REQUIRE(!bsa.meshes());
	REQUIRE(!bsa.misc());
	REQUIRE(!bsa.shaders());
	REQUIRE(!bsa.sounds());
	REQUIRE(!bsa.textures());
	REQUIRE(!bsa.trees());
	REQUIRE(!bsa.voices());
}
