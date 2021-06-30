#include "bsa/tes3.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <string_view>

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include "common.hpp"

#include <catch2/catch.hpp>

using namespace std::literals;

namespace
{
	[[nodiscard]] auto hash_file(std::string_view a_path)
		-> bsa::tes3::hashing::hash
	{
		std::string t(a_path);
		return bsa::tes3::hashing::hash_file(t);
	}
}

static_assert(assert_nothrowable<bsa::tes3::hashing::hash>());
static_assert(assert_nothrowable<bsa::tes3::file>());
static_assert(assert_nothrowable<bsa::tes3::archive>());

TEST_CASE("bsa::tes3::hashing", "[tes3.hashing]")
{
	SECTION("hashes start empty")
	{
		bsa::tes3::hashing::hash h;
		REQUIRE(h.lo == 0);
		REQUIRE(h.hi == 0);
		REQUIRE(h.numeric() == 0);
	}

	SECTION("validate hash values")
	{
		const auto hash = [](std::string_view a_path) noexcept {
			return hash_file(a_path).numeric();
		};

		REQUIRE(hash("meshes/c/artifact_bloodring_01.nif"sv) == 0x1C3C1149920D5F0C);
		REQUIRE(hash("meshes/x/ex_stronghold_pylon00.nif"sv) == 0x20250749ACCCD202);
		REQUIRE(hash("meshes/r/xsteam_centurions.kf"sv) == 0x6E5C0F3125072EA6);
		REQUIRE(hash("textures/tx_rock_cave_mu_01.dds"sv) == 0x58060C2FA3D8F759);
		REQUIRE(hash("meshes/f/furn_ashl_chime_02.nif"sv) == 0x7C3B2F3ABFFC8611);
		REQUIRE(hash("textures/tx_rope_woven.dds"sv) == 0x5865632F0C052C64);
		REQUIRE(hash("icons/a/tx_templar_skirt.dds"sv) == 0x46512A0B60EDA673);
		REQUIRE(hash("icons/m/misc_prongs00.dds"sv) == 0x51715677BBA837D3);
		REQUIRE(hash("meshes/i/in_c_stair_plain_tall_02.nif"sv) == 0x2A324956BF89B1C9);
		REQUIRE(hash("meshes/r/xkwama worker.nif"sv) == 0x6D446E352C3F5A1E);
	}

	SECTION("forward slashes '/' are treated the same as backwards slashes '\\'")
	{
		REQUIRE(hash_file("foo/bar/baz") == hash_file("foo\\bar\\baz"));
	}

	SECTION("hashes are case-insensitive")
	{
		REQUIRE(hash_file("FOO/BAR/BAZ") == hash_file("foo/bar/baz"));
	}

	SECTION("hashes are sorted first by their low value, then their high value")
	{
		bsa::tes3::hashing::hash lhs{ 0, 1 };
		bsa::tes3::hashing::hash rhs{ 1, 0 };
		REQUIRE(lhs < rhs);
	}
}

TEST_CASE("bsa::tes3::file", "[tes3.file]")
{
	SECTION("files start empty")
	{
		bsa::tes3::file f;
		REQUIRE(f.empty());
		REQUIRE(f.size() == 0);
		REQUIRE(f.as_bytes().empty());
	}
}

TEST_CASE("bsa::tes3::archive", "[tes3.archive]")
{
	SECTION("archives start empty")
	{
		bsa::tes3::archive bsa;
		REQUIRE(bsa.empty());
		REQUIRE(bsa.begin() == bsa.end());
		REQUIRE(bsa.size() == 0);
	}

	SECTION("we can read archives")
	{
		const std::filesystem::path root{ "tes3_test"sv };

		bsa::tes3::archive bsa;
		REQUIRE(bsa.read(root / "test.bsa"sv));
		REQUIRE(!bsa.empty());

		constexpr std::array files{
			"characters/character_0000.png"sv,
			"share/License.txt"sv,
		};

		for (const auto& name : files) {
			const auto p = root / name;
			REQUIRE(std::filesystem::exists(p));

			const auto archived = bsa[name];
			REQUIRE(archived);
			REQUIRE(archived->size() == std::filesystem::file_size(p));

			const auto disk = map_file(p);
			REQUIRE(disk.is_open());

			REQUIRE(archived->size() == disk.size());
			REQUIRE(std::memcmp(archived->data(), disk.data(), archived->size()) == 0);
		}
	}
}
