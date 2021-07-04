#include "bsa/fo4.hpp"

#include <string>
#include <string_view>

#include "utility.hpp"

#include <catch2/catch.hpp>

using namespace std::literals;

namespace
{
	[[nodiscard]] auto hash_file(std::string_view a_path) noexcept
		-> bsa::fo4::hashing::hash
	{
		std::string t(a_path);
		return bsa::fo4::hashing::hash_file(t);
	}
}

static_assert(assert_nothrowable<bsa::fo4::hashing::hash>());
static_assert(assert_nothrowable<bsa::fo4::chunk>());
static_assert(assert_nothrowable<bsa::fo4::file>());
static_assert(assert_nothrowable<bsa::fo4::archive>());

TEST_CASE("bsa::fo4::hashing", "[fo4.hashing]")
{
	SECTION("validate hash values")
	{
		using hash_t = bsa::fo4::hashing::hash;
		const auto h = [](std::string_view a_path) noexcept {
			return hash_file(a_path);
		};

		REQUIRE(h(R"(Sound\Voice\Fallout4.esm\RobotMrHandy\María_M.fuz)"sv) == hash_t{ 0x7644F04B, 0x007A7566, 0x8A9C014E });
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
