#include "utility.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch.hpp>

#include "bsa/tes3.hpp"

namespace
{
	[[nodiscard]] auto hash_file(std::string_view a_path) noexcept
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
		const bsa::tes3::hashing::hash h;
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
		const bsa::tes3::hashing::hash lhs{ 0, 1 };
		const bsa::tes3::hashing::hash rhs{ 1, 0 };
		REQUIRE(lhs < rhs);
	}
}

TEST_CASE("bsa::tes3::file", "[tes3.file]")
{
	SECTION("files start empty")
	{
		const bsa::tes3::file f;
		REQUIRE(f.empty());
		REQUIRE(f.size() == 0);
		REQUIRE(f.as_bytes().empty());
	}
}

TEST_CASE("bsa::tes3::archive", "[tes3.archive]")
{
	SECTION("archives start empty")
	{
		const bsa::tes3::archive bsa;
		REQUIRE(bsa.empty());
		REQUIRE(bsa.begin() == bsa.end());
		REQUIRE(bsa.size() == 0);
	}

	SECTION("we can read archives")
	{
		const std::filesystem::path root{ "tes3_read_test"sv };

		bsa::tes3::archive bsa;
		bsa.read(root / "test.bsa"sv);
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

	SECTION("we can write archives")
	{
		const std::filesystem::path root{ "tes3_write_test"sv };
		const std::filesystem::path outPath = root / "out.bsa"sv;

		struct info_t
		{
			consteval info_t(
				std::uint32_t a_lo,
				std::uint32_t a_hi,
				std::string_view a_path) noexcept :
				hash{ a_lo, a_hi },
				path(a_path)
			{}

			bsa::tes3::hashing::hash hash;
			std::string_view path;
		};

		constexpr std::array index{
			info_t{ 0x0C18356B, 0xA578DB74, "Tiles/tile_0001.png"sv },
			info_t{ 0x1B0D3416, 0xF5D5F30E, "Share/License.txt"sv },
			info_t{ 0x1B3B140A, 0x07B36E53, "Background/background_middle.png"sv },
			info_t{ 0x29505413, 0x1EB4CED7, "Construct 3/Pixel Platformer.c3p"sv },
			info_t{ 0x4B7D031B, 0xD4701AD4, "Tilemap/characters_packed.png"sv },
			info_t{ 0x74491918, 0x2BEBCD0A, "Characters/character_0001.png"sv },
		};

		std::vector<bsa::detail::istream_t::stream_type> mmapped;
		bsa::tes3::archive in;
		for (const auto& file : index) {
			const auto& data = mmapped.emplace_back(
				map_file(root / "data"sv / file.path));
			REQUIRE(data.is_open());
			bsa::tes3::file f;
			f.set_data({ //
				reinterpret_cast<const std::byte*>(data.data()),
				data.size() });

			REQUIRE(in.insert(file.path, std::move(f)).second);
		}

		in.write(outPath);

		bsa::tes3::archive out;
		out.read(outPath);
		REQUIRE(out.size() == index.size());
		for (std::size_t idx = 0; idx < index.size(); ++idx) {
			const auto& file = index[idx];
			const auto& mapped = mmapped[idx];

			REQUIRE(out[file.path]);

			const auto f = out.find(file.path);
			REQUIRE(f != out.end());
			REQUIRE(f->first.hash() == file.hash);
			REQUIRE(f->first.name() == simple_normalize(file.path));
			REQUIRE(f->second.size() == mapped.size());
			REQUIRE(std::memcmp(f->second.data(), mapped.data(), mapped.size()) == 0);
		}
	}

	SECTION("archives will bail on malformed inputs")
	{
		const std::filesystem::path root{ "tes3_invalid_test"sv };
		constexpr std::array types{
			"magic",
			"range",
		};

		for (const std::string type : types) {
			std::string filename;
			filename += "invalid_"sv;
			filename += type;
			filename += ".bsa"sv;

			bsa::tes3::archive bsa;
			REQUIRE_THROWS_WITH(
				bsa.read(root / filename),
				Catch::Matchers::Matches(".*\\b"s + type + "\\b.*"s, Catch::CaseSensitive::No));
		}
	}

	SECTION("we can validate the offsets within an archive (<4gb)")
	{
		bsa::tes3::archive bsa;
		const auto add =
			[&](bsa::tes3::hashing::hash a_hash,
				std::span<const std::byte> a_data) {
				constexpr auto dir = "root"sv;

				bsa::tes3::file f;
				f.set_data(a_data);

				REQUIRE(bsa.insert(a_hash, std::move(f)).second);
			};

		constexpr auto largesz = static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()) + 1u;
		const auto plarge = std::make_unique<std::byte[]>(largesz);
		const std::span large{ plarge.get(), largesz };

		const std::array<std::byte, 1u << 4> littlebuf{};
		const std::span little{ littlebuf.data(), littlebuf.size() };

		const auto verify = [&]() {
			return bsa.verify_offsets();
		};

		REQUIRE(verify());

		add({ 0 }, little);
		REQUIRE(verify());

		add({ 1 }, large);
		REQUIRE(verify());

		bsa.clear();
		add({ 0 }, large);
		REQUIRE(verify());

		add({ 1 }, little);
		REQUIRE(!verify());
	}
}
