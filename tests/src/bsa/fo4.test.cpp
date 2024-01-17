#include "utility.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <DirectXTex.h>

#include "catch2.hpp"
#include <binary_io/any_stream.hpp>
#include <binary_io/memory_stream.hpp>
#include <mmio/mmio.hpp>

#include "bsa/fo4.hpp"

namespace
{
#define COMPARE(a_field) a_lhs.a_field == a_rhs.a_field

	struct DDS_PIXELFORMAT
	{
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwFourCC;
		DWORD dwRGBBitCount;
		DWORD dwRBitMask;
		DWORD dwGBitMask;
		DWORD dwBBitMask;
		DWORD dwABitMask;

		[[nodiscard]] friend bool operator==(DDS_PIXELFORMAT, DDS_PIXELFORMAT) noexcept = default;
	};

	struct DDS_HEADER
	{
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwHeight;
		DWORD dwWidth;
		DWORD dwPitchOrLinearSize;
		DWORD dwDepth;
		DWORD dwMipMapCount;
		DWORD dwReserved1[11];
		DDS_PIXELFORMAT ddspf;
		DWORD dwCaps;
		DWORD dwCaps2;
		DWORD dwCaps3;
		DWORD dwCaps4;
		DWORD dwReserved2;

		[[nodiscard]] friend bool operator==(
			DDS_HEADER a_lhs,
			DDS_HEADER a_rhs) noexcept
		{
			return COMPARE(dwSize) &&
			       COMPARE(dwFlags) &&
			       COMPARE(dwHeight) &&
			       COMPARE(dwWidth) &&
			       COMPARE(dwPitchOrLinearSize) &&
			       COMPARE(dwDepth) &&
			       COMPARE(dwMipMapCount) &&
			       COMPARE(ddspf) &&
			       COMPARE(dwCaps) &&
			       COMPARE(dwCaps2) &&
			       COMPARE(dwCaps3) &&
			       COMPARE(dwCaps4);
		}
	};

	struct DDS_HEADER_DXT10
	{
		DXGI_FORMAT dxgiFormat;
		UINT resourceDimension;
		UINT miscFlag;
		UINT arraySize;
		UINT miscFlags2;  // formerly reserved

		[[nodiscard]] friend bool operator==(
			DDS_HEADER_DXT10 a_lhs,
			DDS_HEADER_DXT10 a_rhs) noexcept
		{
			return COMPARE(dxgiFormat) &&
			       COMPARE(resourceDimension) &&
			       COMPARE(miscFlag) &&
			       COMPARE(arraySize);
		}
	};

	struct dds9_header_t
	{
		DWORD dwMagic;
		DDS_HEADER header;

		[[nodiscard]] friend bool operator==(dds9_header_t, dds9_header_t) noexcept = default;
	};

	struct dds10_header_t
	{
		dds9_header_t header9;
		DDS_HEADER_DXT10 header10;

		[[nodiscard]] friend bool operator==(dds10_header_t, dds10_header_t) noexcept = default;
	};

#undef COMPARE
}

static_assert(assert_nothrowable<bsa::fo4::hashing::hash>());
static_assert(assert_nothrowable<bsa::fo4::chunk>());
static_assert(assert_nothrowable<bsa::fo4::file>());
static_assert(assert_nothrowable<bsa::fo4::file::key, false>());
static_assert(assert_nothrowable<bsa::fo4::archive>());

TEST_CASE("bsa::fo4::hashing", "[src][fo4][hashing]")
{
	SECTION("hashes start empty")
	{
		const bsa::fo4::hashing::hash hash;
		REQUIRE(hash.file == 0);
		REQUIRE(hash.extension == 0);
		REQUIRE(hash.directory == 0);
	}

	SECTION("validate hash values")
	{
		using hash_t = bsa::fo4::hashing::hash;
		const auto h = [](std::string_view a_path) noexcept {
			return bsa::fo4::hashing::hash_file(a_path);
		};

		REQUIRE(h("Sound\\Voice\\Fallout4.esm\\RobotMrHandy\\Mar"
				  "\xED"
				  "a_M.fuz"sv) == hash_t{ 0xC9FB26F9, 0x007A7566, 0x8A9C014E });
		REQUIRE(h(R"(Strings\ccBGSFO4001-PipBoy(Black)_en.DLSTRINGS)"sv) == hash_t{ 0x1985075C, 0x74736C64, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4001\AnimObjects\PipBoy\PipBoy02(Black)_d.DDS)"sv) == hash_t{ 0x69E1E82C, 0x00736464, 0x23157A84 });
		REQUIRE(h(R"(Materials\CreationClub\BGSFO4003\AnimObjects\PipBoy\PipBoyLabels01(Camo01).BGSM)"sv) == hash_t{ 0x0785843B, 0x6D736762, 0x818374CC });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4003\AnimObjects\PipBoy\PipBoy02(Camo01)_d.DDS)"sv) == hash_t{ 0xF2D2F9A7, 0x00736464, 0xE9DB0C08 });
		REQUIRE(h(R"(Strings\ccBGSFO4004-PipBoy(Camo02)_esmx.DLSTRINGS)"sv) == hash_t{ 0xC26B77C1, 0x74736C64, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4004\AnimObjects\PipBoy\PipBoyLabels01(Camo02)_d.DDS)"sv) == hash_t{ 0xB32EE4B0, 0x00736464, 0x089FAA9B });
		REQUIRE(h(R"(Strings\ccBGSFO4006-PipBoy(Chrome)_es.STRINGS)"sv) == hash_t{ 0xA94A4503, 0x69727473, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4006\AnimObjects\PipBoy\PipBoy01(Chrome)_s.DDS)"sv) == hash_t{ 0xE2D67EE2, 0x00736464, 0xC251DC17 });
		REQUIRE(h(R"(Meshes\CreationClub\BGSFO4016\Clothes\Prey\MorganSpaceSuit_M_First.nif)"sv) == hash_t{ 0x212E5DAD, 0x0066696E, 0x741DAAC0 });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4016\Clothes\Prey\Morgan_Male_Body_s.DDS)"sv) == hash_t{ 0x9C672F34, 0x00736464, 0x1D5F0EDF });
		REQUIRE(h(R"(Strings\ccBGSFO4018-GaussRiflePrototype_ru.STRINGS)"sv) == hash_t{ 0x5198717F, 0x69727473, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4018\Weapons\GaussRiflePrototype\Barrel02_s.DDS)"sv) == hash_t{ 0x2C98BAA2, 0x00736464, 0x8D59E9EA });
		REQUIRE(h(R"(Strings\ccBGSFO4019-ChineseStealthArmor_esmx.DLSTRINGS)"sv) == hash_t{ 0xDDF2A35F, 0x74736C64, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4019\Armor\ChineseStealthArmor\ChineseStealthArmor01_d.DDS)"sv) == hash_t{ 0x03C2AA10, 0x00736464, 0x71ED2818 });
		REQUIRE(h(R"(Materials\CreationClub\BGSFO4020\Actors\PowerArmor\T45helmet01(Black).BGSM)"sv) == hash_t{ 0xF56D31C0, 0x6D736762, 0x28A143A5 });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4020\Actors\PowerArmor\T51\Black\T51Helmet01(Black)_d.DDS)"sv) == hash_t{ 0x3192919D, 0x00736464, 0xA56D1E61 });
		REQUIRE(h(R"(Materials\CreationClub\BGSFO4038\Actors\PowerArmor\HorsePAHelmet.BGSM)"sv) == hash_t{ 0xE90B72CC, 0x6D736762, 0x44676566 });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4038\Actors\PowerArmor\HorsePATorso_teal_d.DDS)"sv) == hash_t{ 0x0A6251B3, 0x00736464, 0xC1AC59B4 });
		REQUIRE(h(R"(Strings\ccBGSFO4044-HellfirePowerArmor_en.DLSTRINGS)"sv) == hash_t{ 0x3E5C1E5E, 0x74736C64, 0x29F6B58B });
		REQUIRE(h(R"(Textures\CreationClub\BGSFO4044\Actors\PowerArmor\HellfirePAHelmet_Institute_d.DDS)"sv) == hash_t{ 0x0F221EAF, 0x00736464, 0xC021EF40 });
		REQUIRE(h(R"(Meshes\Weapons\HandmadeShotgun\HandmadeShotgun_GlowSights.nif)"sv) == hash_t{ 0x4E080CE2, 0x0066696E, 0xCCD47ECF });
		REQUIRE(h(R"(Textures\Weapons\HandmadeShotgun\HandmadeShotgun_Barrels_GhoulSlayer_d.DDS)"sv) == hash_t{ 0xBBFC484C, 0x00736464, 0xCEAE4154 });
		REQUIRE(h(R"(Materials\CreationClub\FSVFO4001\Clothes\MilitaryBackpack\BackpackPatch_NCR02.bgsm)"sv) == hash_t{ 0x90EB78B9, 0x6D736762, 0xDA685DF4 });
		REQUIRE(h(R"(Textures\CreationClub\FSVFO4001\Clothes\MilitaryBackpack\Button_SunsetSars_d.DDS)"sv) == hash_t{ 0xC25F8604, 0x00736464, 0xD1CE178D });
		REQUIRE(h(R"(Materials\CreationClub\FSVFO4002\Furniture\MidCenturyModern01\BedSpread01.bgsm)"sv) == hash_t{ 0xA5AAE799, 0x6D736762, 0xBECD0DEF });
		REQUIRE(h(R"(Textures\CreationClub\FSVFO4002\Furniture\MidCenturyModern01\Bed01_n.DDS)"sv) == hash_t{ 0x6A09686A, 0x00736464, 0xBA782808 });
		REQUIRE(h(R"(Sound\FX\DLC03\NPC\Gulper\NPC_Gulper_Foot_Walk_02.xwm)"sv) == hash_t{ 0xFE001981, 0x006D7778, 0xE7FBD6C4 });
		REQUIRE(h(R"(Textures\Terrain\DLC03FarHarbor\DLC03FarHarbor.4.-69.41.DDS)"sv) == hash_t{ 0x36BACD03, 0x00736464, 0x8184624D });
		REQUIRE(h(R"(Sound\Voice\DLCCoast.esm\PlayerVoiceFemale01\00043FFC_1.fuz)"sv) == hash_t{ 0x339EFB3F, 0x007A7566, 0x3A5289D4 });
		REQUIRE(h(R"(Meshes\PreCombined\DLCNukaWorld.esm\0000F616_17EAC297_OC.NIF)"sv) == hash_t{ 0xD4AD97F7, 0x0066696E, 0x0787B7E9 });
		REQUIRE(h(R"(Textures\Terrain\NukaWorld\NukaWorld.4.-28.28_msn.DDS)"sv) == hash_t{ 0x86C13103, 0x00736464, 0x26C08933 });
		REQUIRE(h(R"(Sound\Voice\DLCNukaWorld.esm\DLC04NPCMJohnCalebBradberton\00044D5E_1.fuz)"sv) == hash_t{ 0x896E4419, 0x007A7566, 0xD6575CD6 });
		REQUIRE(h(R"(Meshes\SCOL\DLCRobot.esm\CM00007BD8.NIF)"sv) == hash_t{ 0x103559EF, 0x0066696E, 0xF584B7C4 });
		REQUIRE(h(R"(Textures\DLC01\SetDressing\Rubble\Robottrashpilesnorust_s.DDS)"sv) == hash_t{ 0xC7AF7106, 0x00736464, 0x5FD1A1B0 });
		REQUIRE(h(R"(Sound\Voice\DLCRobot.esm\DLC01RobotCompanionFemaleProcessed\00001460_1.fuz)"sv) == hash_t{ 0x6D3D7DC7, 0x007A7566, 0xB2B47CAD });
		REQUIRE(h(R"(Materials\DLC02\SetDressing\Workshop\NeonSignage\NeonLetterKit01-Orange-5.BGEM)"sv) == hash_t{ 0x21D59551, 0x6D656762, 0x926F0C27 });
		REQUIRE(h(R"(Textures\DLC02\SetDressing\Workshop\Traps\DLC02_SpringTrap01_s.DDS)"sv) == hash_t{ 0x02BE99A4, 0x00736464, 0xF03CA2DF });
		REQUIRE(h(R"(Sound\FX\DLC05\PHY\BallTrack\PHY_Metal_BallTrack_SteelBall_Wood_H_03.xwm)"sv) == hash_t{ 0x33AABE0C, 0x006D7778, 0x07AA294C });
		REQUIRE(h(R"(Textures\DLC05\Effects\PaintBalls\ImpactDecalPaintSplatters01Red_d.DDS)"sv) == hash_t{ 0x6327DF24, 0x00736464, 0xFB5FB431 });
		REQUIRE(h(R"(Meshes\SCOL\DLCworkshop03.esm\CM00001091.NIF)"sv) == hash_t{ 0x2CAF6750, 0x0066696E, 0xABA83647 });
		REQUIRE(h(R"(Textures\DLC06\Interiors\Vault\DLC06VltSignWelcome88_01_d.DDS)"sv) == hash_t{ 0x825BD732, 0x00736464, 0xAE76DDEF });
		REQUIRE(h(R"(Sound\Voice\DLCworkshop03.esm\FemaleEvenToned\00005232_1.fuz)"sv) == hash_t{ 0x4DB6EE2D, 0x007A7566, 0xDA9F7ABC });
		REQUIRE(h(R"(Meshes\AnimTextData\DynamicIdleData\5693375383928345500.txt)"sv) == hash_t{ 0x997FC17A, 0x00747874, 0xFD345C50 });
		REQUIRE(h(R"(Interface\Pipboy_StatsPage.swf)"sv) == hash_t{ 0x2F26E4D0, 0x00667773, 0xD2FDF873 });
		REQUIRE(h(R"(Materials\Landscape\Grass\BeachGrass01.BGSM)"sv) == hash_t{ 0xB023CE22, 0x6D736762, 0x941D851F });
		REQUIRE(h(R"(Meshes\Actors\Character\FaceGenData\FaceGeom\Fallout4.esm\000B3EC7.NIF)"sv) == hash_t{ 0x90C91640, 0x0066696E, 0x067FA81E });
		REQUIRE(h(R"(Meshes\PreCombined\0000E069_7831AAC9_OC.NIF)"sv) == hash_t{ 0x5F0B19DF, 0x0066696E, 0xE659D075 });
		REQUIRE(h(R"(scripts\MinRadiantOwnedBuildResourceScript.pex)"sv) == hash_t{ 0xA2DAD4FD, 0x00786570, 0x40724840 });
		REQUIRE(h(R"(Meshes\debris\roundrock2_dirt.nif)"sv) == hash_t{ 0x1E47A158, 0x0066696E, 0xF55EC6BA });
		REQUIRE(h(R"(ShadersFX\Shaders011.fxp)"sv) == hash_t{ 0x883415D8, 0x00707866, 0xDFAE3D0F });
		REQUIRE(h(R"(Sound\FX\FX\Bullet\Impact\xxx\FX_Bullet_Impact_Dirt_04.xwm)"sv) == hash_t{ 0xFFAD9A14, 0x006D7778, 0xCBA20EB7 });
		REQUIRE(h(R"(Textures\Effects\ColorBlackZeroAlphaUtility.DDS)"sv) == hash_t{ 0xF912F225, 0x00736464, 0xEA3C9738 });
		REQUIRE(h(R"(Textures\interiors\Building\BldWindow01_s.DDS)"sv) == hash_t{ 0x6ECA4F0C, 0x00736464, 0x5A3A7C7A });
		REQUIRE(h(R"(Textures\Terrain\Commonwealth\Commonwealth.4.-8.12_msn.DDS)"sv) == hash_t{ 0x55E37BD8, 0x00736464, 0x4409E1A9 });
		REQUIRE(h(R"(Textures\Clothes\Nat\Nats_Outfit_s.DDS)"sv) == hash_t{ 0x692FFE7D, 0x00736464, 0x3F5BEDF1 });
		REQUIRE(h(R"(Textures\Interface\Newspaper\Newspaper_s.DDS)"sv) == hash_t{ 0xFAC17C6C, 0x00736464, 0x58B9C5A4 });
		REQUIRE(h(R"(Textures\Actors\Character\FaceCustomization\Fallout4.esm\00110043_s.DDS)"sv) == hash_t{ 0x09A155E6, 0x00736464, 0x9C7DFA7A });
		REQUIRE(h(R"(Textures\Terrain\Commonwealth\Commonwealth.4.-48.-60.DDS)"sv) == hash_t{ 0x182C2446, 0x00736464, 0x4409E1A9 });
		REQUIRE(h(R"(Textures\Terrain\Commonwealth\Commonwealth.4.-80.8_msn.DDS)"sv) == hash_t{ 0xDA3234A4, 0x00736464, 0x4409E1A9 });
		REQUIRE(h(R"(Textures\Terrain\SanctuaryHillsWorld\SanctuaryHillsWorld.4.-36.40.DDS)"sv) == hash_t{ 0xDD27070A, 0x00736464, 0x49AAA5E1 });
		REQUIRE(h(R"(Textures\Terrain\SanctuaryHillsWorld\SanctuaryHillsWorld.4.76.-24.DDS)"sv) == hash_t{ 0x71560B31, 0x00736464, 0x49AAA5E1 });
		REQUIRE(h(R"(Sound\Voice\Fallout4.esm\NPCMTravisMiles\000A6032_1.fuz)"sv) == hash_t{ 0x34402DE0, 0x007A7566, 0xF186D761 });
	}
}

TEST_CASE("bsa::fo4::chunk", "[src][fo4][vfs]")
{
	SECTION("files start empty")
	{
		const bsa::fo4::chunk chunk;
		REQUIRE(chunk.empty());
		REQUIRE(!chunk.compressed());
		REQUIRE(chunk.size() == 0);
		REQUIRE(chunk.mips.first == 0);
		REQUIRE(chunk.mips.last == 0);
	}
}

TEST_CASE("bsa::fo4::file", "[src][fo4][vfs]")
{
	SECTION("files start empty")
	{
		const bsa::fo4::file file;
		REQUIRE(file.empty());
		REQUIRE(file.begin() == file.end());
		REQUIRE(file.size() == 0);
		REQUIRE(file.header.height == 0);
		REQUIRE(file.header.width == 0);
		REQUIRE(file.header.mip_count == 0);
		REQUIRE(file.header.format == 0);
		REQUIRE(file.header.flags == 0);
		REQUIRE(file.header.tile_mode == 0);
	}
}

TEST_CASE("bsa::fo4::archive", "[src][fo4][archive]")
{
	SECTION("archives start empty")
	{
		const bsa::fo4::archive ba2;
		REQUIRE(ba2.empty());
		REQUIRE(ba2.begin() == ba2.end());
		REQUIRE(ba2.size() == 0);
	}

	SECTION("we can read/write texture archives")
	{
		const std::filesystem::path root{ "fo4_dds_test"sv };
		const std::filesystem::path inPath = root / "in.ba2"sv;

		bsa::fo4::archive ba2;
		const auto meta = ba2.read(inPath);
		REQUIRE(meta.format == bsa::fo4::format::directx);
		REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);
		REQUIRE(ba2.size() == 1);

		const auto file = ba2["Fence006_1K_Roughness.dds"sv];
		REQUIRE(file);
		REQUIRE(file->size() == 3);
		REQUIRE(file->header.height == 1024);
		REQUIRE(file->header.width == 1024);
		REQUIRE(file->header.mip_count == 11);
		REQUIRE(file->header.format == 98);
		REQUIRE(file->header.flags == 0);
		REQUIRE(file->header.tile_mode == 8);

		REQUIRE((*file)[0].size() == 0x100'000);
		REQUIRE((*file)[0].mips.first == 0);
		REQUIRE((*file)[0].mips.last == 0);

		REQUIRE((*file)[1].size() == 0x40'000);
		REQUIRE((*file)[1].mips.first == 1);
		REQUIRE((*file)[1].mips.last == 1);

		REQUIRE((*file)[2].size() == 0x15'570);
		REQUIRE((*file)[2].mips.first == 2);
		REQUIRE((*file)[2].mips.last == 10);

		compare_to_master_copy(
			inPath,
			[&](binary_io::any_ostream& a_os) {
				ba2.write(a_os, { .format = bsa::fo4::format::directx });
			});
	}

	SECTION("we can write archives")
	{
		const std::filesystem::path root{ "fo4_write_test"sv };

		struct info_t
		{
			consteval info_t(
				std::uint32_t a_file,
				std::string_view a_extension,
				std::uint32_t a_directory,
				std::string_view a_path) noexcept :
				hash{ a_file, bsa::make_four_cc(a_extension), a_directory },
				path(a_path)
			{}

			bsa::fo4::hashing::hash hash;
			std::string_view path;
		};

		constexpr std::array index{
			info_t{ 0x35B94567, "png"sv, 0x5FE2DC26, "Background/background_tilemap.png"sv },
			info_t{ 0x53D5F897, "png"sv, 0xD9A32978, "Characters/character_0003.png"sv },
			info_t{ 0x36F72750, "txt"sv, 0x60648919, "Construct 3/Readme.txt"sv },
			info_t{ 0xCA042B67, "txt"sv, 0x29246A47, "Share/License.txt"sv },
			info_t{ 0xDA3773A6, "png"sv, 0x0B0A447E, "Tilemap/tiles.png"sv },
			info_t{ 0x785183FF, "png"sv, 0xDA3773A6, "Tiles/tile_0003.png"sv },
		};

		std::vector<mmio::mapped_file_source> mmapped;
		bsa::fo4::archive in;
		for (const auto& file : index) {
			const auto& data = mmapped.emplace_back(
				map_file(root / "data"sv / file.path));
			REQUIRE(data.is_open());
			bsa::fo4::chunk c;
			c.set_data({ //
				reinterpret_cast<const std::byte*>(data.data()),
				data.size() });

			bsa::fo4::file f;
			f.push_back(std::move(c));

			REQUIRE(in.insert(file.path, std::move(f)).second);
		}

		const auto test = [&](bool a_strings) {
			binary_io::any_ostream os{ std::in_place_type<binary_io::memory_ostream> };

			in.write(os, { .format = bsa::fo4::format::general, .strings = a_strings });

			bsa::fo4::archive out;
			const auto meta = out.read({ os.get<binary_io::memory_ostream>().rdbuf() });
			REQUIRE(meta.format == bsa::fo4::format::general);
			REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);
			REQUIRE(out.size() == index.size());
			for (std::size_t idx = 0; idx < index.size(); ++idx) {
				const auto& file = index[idx];
				const auto& mapped = mmapped[idx];

				REQUIRE(out[file.path]);

				const auto f = out.find(file.path);
				REQUIRE(f != out.end());
				REQUIRE(f->first.hash() == file.hash);
				if (a_strings) {
					REQUIRE(f->first.name() == simple_normalize(file.path));
				}
				REQUIRE(f->second.size() == 1);

				auto& c = f->second.front();
				if (c.compressed()) {
					REQUIRE(c.decompressed_size() == mapped.size());
					c.decompress(meta.compression_format);
				}
				assert_byte_equality(c.as_bytes(), std::span{ mapped.data(), mapped.size() });
			}
		};

		test(true);
		test(false);

		for (auto& f : in) {
			for (auto& c : f.second) {
				c.compress({});
			}
		}

		test(true);
		test(false);
	}

	SECTION("archives will bail on malformed inputs")
	{
		const std::filesystem::path root{ "fo4_invalid_test"sv };
		constexpr std::array types{
			"format"sv,
			"magic"sv,
			"exhausted"sv,
			"sentinel"sv,
			"size"sv,
			"version"sv,
		};

		for (const auto& type : types) {
			std::string filename;
			filename += "invalid_"sv;
			filename += type;
			filename += ".ba2"sv;

			bsa::fo4::archive ba2;
			REQUIRE_THROWS_WITH(
				ba2.read(root / filename),
				make_substr_matcher(type));
		}
	}

	SECTION("archives do not have to contain a string table")
	{
		const std::filesystem::path root{ "fo4_missing_string_table_test"sv };
		const auto inPath = root / "in.ba2"sv;
		const auto filename = "misc/example.txt"sv;

		bsa::fo4::archive ba2;
		const auto meta = ba2.read(inPath);
		REQUIRE(meta.format == bsa::fo4::format::general);
		REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);
		const auto f = ba2[filename];
		REQUIRE(f);
		REQUIRE(f->size() == 1);

		const auto mapped = map_file(root / "data"sv / filename);
		const auto& c = f->front();

		REQUIRE(!c.compressed());
		assert_byte_equality(c.as_bytes(), std::span{ mapped.data(), mapped.size() });

		compare_to_master_copy(
			inPath,
			[&](binary_io::any_ostream& a_os) {
				ba2.write(a_os, { .format = bsa::fo4::format::general, .strings = false });
			});
	}

	SECTION("archives can contain compressed files")
	{
		const std::filesystem::path root{ "fo4_compression_test"sv };
		const std::array archives{
			std::make_pair("normal.ba2"sv, bsa::fo4::compression_level::fo4),
			std::make_pair("xbox.ba2"sv, bsa::fo4::compression_level::fo4_xbox),
		};

		for (const auto& [archive, compression] : archives) {
			bsa::fo4::archive ba2;
			const auto meta = ba2.read(root / archive);
			REQUIRE(meta.format == bsa::fo4::format::general);
			REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "data"sv)) {
				if (entry.is_regular_file()) {
					const auto p = std::filesystem::relative(entry.path(), root / "data"sv);
					const auto arch = ba2[p.string()];
					REQUIRE(arch);
					REQUIRE(arch->size() == 1);
					auto archC = arch->front();
					REQUIRE(archC.compressed());

					const auto disk = map_file(entry.path());

					bsa::fo4::chunk diskC;
					diskC.set_data({ //
						reinterpret_cast<const std::byte*>(disk.data()),
						disk.size() });
					diskC.compress({ .compression_level = compression });
					assert_byte_equality(archC.as_bytes(), diskC.as_bytes());

					archC.decompress(meta.compression_format);
					REQUIRE(!archC.compressed());
					assert_byte_equality(archC.as_bytes(), std::span{ disk.data(), disk.size() });
				}
			}
		}
	}

	SECTION("we can read/write archives without touching the disk")
	{
		test_in_memory_buffer<bsa::fo4::archive>(
			"fo4.ba2"sv,
			[](
				bsa::fo4::archive& a_archive,
				std::span<const std::pair<std::string_view, mmio::mapped_file_source>> a_files) {
				for (const auto& [path, file] : a_files) {
					bsa::fo4::file f;
					f.emplace_back()
						.set_data(std::span{ file.data(), file.size() });
					a_archive.insert(path, std::move(f));
				}
			},
			[](bsa::fo4::archive& a_archive, std::filesystem::path a_dst) {
				a_archive.write(a_dst, { .format = bsa::fo4::format::general });
			},
			[](bsa::fo4::archive& a_archive, binary_io::any_ostream& a_dst) {
				a_archive.write(a_dst, { .format = bsa::fo4::format::general });
			},
			[](bsa::fo4::archive& a_archive, std::span<const std::byte> a_src, bsa::copy_type a_type) {
				const auto meta = a_archive.read({ a_src, a_type });
				REQUIRE(meta.format == bsa::fo4::format::general);
				REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);
			});
	}

	SECTION("we can apply the correct chunking strategy for texture files")
	{
		const std::filesystem::path root{ "fo4_chunk_test"sv };
		const auto filename = "test.dds"sv;

		bsa::fo4::file f;
		f.read(
			root / filename,
			{
				.format = bsa::fo4::format::directx,
				.compression_type = bsa::compression_type::compressed,
			});

		REQUIRE(f.header.mip_count == 11);
		REQUIRE(f.header.format == ::DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM);
		REQUIRE(f.size() == 3);

		REQUIRE(f[0].mips.first == 0);
		REQUIRE(f[0].mips.last == 0);
		REQUIRE(f[0].decompressed_size() == 0x8'0000);

		REQUIRE(f[1].mips.first == 1);
		REQUIRE(f[1].mips.last == 1);
		REQUIRE(f[1].decompressed_size() == 0x2'0000);

		REQUIRE(f[2].mips.first == 2);
		REQUIRE(f[2].mips.last == 10);
		REQUIRE(f[2].decompressed_size() == 0xAAB8);

		bsa::fo4::archive ba2;
		REQUIRE(ba2.insert(filename, std::move(f)).second);

		compare_to_master_copy(
			root / "in.ba2"sv,
			[&](binary_io::any_ostream& a_os) {
				ba2.write(a_os, { .format = bsa::fo4::format::directx });
			});
	}

	SECTION("we can create texture archives using cubemaps")
	{
		const std::filesystem::path root{ "fo4_cubemap_test"sv };
		const auto filename = "blacksky_e.dds"sv;

		bsa::fo4::file f;
		f.read(
			root / filename,
			{
				.format = bsa::fo4::format::directx,
				.compression_type = bsa::compression_type::compressed,
			});
		REQUIRE(f.header.mip_count == 10);
		REQUIRE(f.header.flags == 1);
		REQUIRE(f.header.tile_mode == 8);
		REQUIRE(f.size() == 1);
		REQUIRE(f[0].mips.first == 0);
		REQUIRE(f[0].mips.last == 9);
		REQUIRE(f[0].decompressed_size() == 0x20'00A0);

		bsa::fo4::archive ba2;
		REQUIRE(ba2.insert(filename, std::move(f)).second);

		compare_to_master_copy(
			root / "in.ba2"sv,
			[&](binary_io::any_ostream& a_os) {
				ba2.write(a_os, { .format = bsa::fo4::format::directx });
			});
	}

	SECTION("we can pack/unpack archives written in the directx format")
	{
		const std::filesystem::path root{ "fo4_dds_test"sv };
		const auto filename = "Fence006_1K_Roughness.dds"sv;

		bsa::fo4::archive ba2;
		const auto meta = ba2.read(root / "in.ba2"sv);
		REQUIRE(meta.format == bsa::fo4::format::directx);
		REQUIRE(meta.compression_format == bsa::fo4::compression_format::zip);

		const auto archived = ba2[filename];
		REQUIRE(archived);
		REQUIRE(!archived->empty());
		for (const auto& chunk : *archived) {
			REQUIRE(!chunk.compressed());
		}

		{
			bsa::fo4::file copy;
			copy.read(root / filename, { .format = bsa::fo4::format::directx });
			REQUIRE(copy.header == archived->header);
			REQUIRE(copy.size() == archived->size());
			for (std::size_t i = 0; i < copy.size(); ++i) {
				const auto& chunk1 = copy[i];
				const auto& chunk2 = (*archived)[i];
				REQUIRE(chunk1.mips == chunk2.mips);
				assert_byte_equality(chunk1.as_bytes(), chunk2.as_bytes());
			}
		}

		compare_to_master_copy(
			root / filename,
			[&](binary_io::any_ostream& a_os) {
				archived->write(a_os, { .format = bsa::fo4::format::directx });
			},
			[](std::span<const std::byte> a_lhs, std::span<const std::byte> a_rhs) {
				REQUIRE(a_lhs.size() == a_rhs.size());
				const auto lhs = reinterpret_cast<const dds10_header_t*>(a_lhs.data());
				const auto rhs = reinterpret_cast<const dds10_header_t*>(a_rhs.data());
				REQUIRE(*lhs == *rhs);
				REQUIRE(std::memcmp(lhs + 1, rhs + 1, a_lhs.size() - sizeof(dds10_header_t)) == 0);
			});
	}

	SECTION("we can read/write directx files")
	{
		const std::filesystem::path root{ "fo4_dx9_test"sv };
		const auto files = { "dx9.dds"sv, "blacksky_e.dds"sv, "bleakfallscube_e.dds"sv };
		for (const auto& filename : files) {
			bsa::fo4::file file;
			file.read(root / filename, { .format = bsa::fo4::format::directx });
			compare_to_master_copy(
				root / filename,
				[&](binary_io::any_ostream& a_os) {
					file.write(a_os, { .format = bsa::fo4::format::directx });
				});
		}
	}
}
