#include "utility.hpp"

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

#include <catch2/catch.hpp>

#include "bsa/all.hpp"

TEST_CASE("bsa::all::detail", "[all.detail]")
{
	SECTION("get_identifier")
	{
		REQUIRE(bsa::all::detail::get_archive_identifier(bsa::tes3::archive{}) == "tes3");
		REQUIRE(bsa::all::detail::get_archive_identifier(bsa::tes4::archive{}) == "tes4");
		REQUIRE(bsa::all::detail::get_archive_identifier(bsa::fo4::archive{}) == "fo4");
	}

	SECTION("archive_version correctly handles errors")
	{
		using namespace bsa;
		using bsa::all::detail::archive_version;

		auto test_throw_wrong_version = [](auto archive, auto version) {
			REQUIRE_THROWS(archive_version<std::uint32_t>(archive, version));
			REQUIRE_THROWS(archive_version<tes4::version>(archive, version));
			REQUIRE_THROWS(archive_version<fo4::format>(archive, version));
		};

		SECTION("tes3 archive")
		{
			all::underlying_archive archive = tes3::archive{};
			all::version version = all::version::tes3;

			REQUIRE_NOTHROW(archive_version<std::uint32_t>(archive, version));
			REQUIRE_THROWS(archive_version<bsa::tes4::version>(archive, version));
			REQUIRE_THROWS(archive_version<bsa::fo4::format>(archive, version));

			REQUIRE(archive_version<std::uint32_t>(archive, version) == detail::to_underlying(all::version::tes3));

			test_throw_wrong_version(archive, all::version::tes4);
			test_throw_wrong_version(archive, all::version::tes5);
			test_throw_wrong_version(archive, all::version::sse);
			test_throw_wrong_version(archive, all::version::fo4);
		}

		SECTION("tes4 archive")
		{
			all::underlying_archive archive = tes4::archive{};
			all::version version = all::version::tes4;

			REQUIRE_THROWS(archive_version<std::uint32_t>(archive, version));
			REQUIRE_NOTHROW(archive_version<tes4::version>(archive, version));
			REQUIRE_THROWS(archive_version<bsa::fo4::format>(archive, version));

			REQUIRE(archive_version<tes4::version>(archive, version) == tes4::version::tes4);

			test_throw_wrong_version(archive, all::version::tes3);
			test_throw_wrong_version(archive, all::version::fo4);
			test_throw_wrong_version(archive, all::version::fo4dx);
		}

		SECTION("fo4 archive")
		{
			all::underlying_archive archive = fo4::archive{};
			all::version version = all::version::fo4;

			REQUIRE_THROWS(archive_version<std::uint32_t>(archive, version));
			REQUIRE_THROWS(archive_version<bsa::tes4::version>(archive, version));
			REQUIRE_NOTHROW(archive_version<fo4::format>(archive, version));

			REQUIRE(archive_version<fo4::format>(archive, version) == fo4::format::general);

			test_throw_wrong_version(archive, all::version::tes3);
			test_throw_wrong_version(archive, all::version::tes4);
			test_throw_wrong_version(archive, all::version::tes5);
			test_throw_wrong_version(archive, all::version::sse);
		}
	}
}

TEST_CASE("bsa::all::archive", "all.archive")
{
	SECTION("Archive is correctly initialized by write constructor")
	{
		auto archive = bsa::all::archive(bsa::all::version::tes3, true);
		REQUIRE(std::holds_alternative<bsa::tes3::archive>(archive.get_archive()));

		archive = bsa::all::archive(bsa::all::version::tes4, true);
		REQUIRE(std::holds_alternative<bsa::tes4::archive>(archive.get_archive()));

		archive = bsa::all::archive(bsa::all::version::fo4, true);
		REQUIRE(std::holds_alternative<bsa::fo4::archive>(archive.get_archive()));
	}
}
