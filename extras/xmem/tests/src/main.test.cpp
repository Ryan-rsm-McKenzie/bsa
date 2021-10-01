#include "../../src/main.cpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include <catch2/catch.hpp>
#include <mmio/mmio.hpp>

#include "bsa/xmem/xmem.hpp"

using namespace std::literals;

TEST_CASE("assert cli commands work", "[src]")
{
	const std::filesystem::path root{ "roundtrip_test"sv };
	const auto validate = [](std::filesystem::path a_expected, std::filesystem::path a_got) {
		const mmio::mapped_file_source expected{ std::move(a_expected) };
		const mmio::mapped_file_source got{ std::move(a_got) };
		REQUIRE(expected.is_open());
		REQUIRE(expected.size() == got.size());
		REQUIRE(std::memcmp(expected.data(), got.data(), got.size()) == 0);
	};

	SECTION("compress")
	{
		const auto infile = root / "decompressed.png"sv;
		const auto wantfile = root / "compressed.bin"sv;
		const auto outfile = root / "output.bin"sv;

		if (std::filesystem::exists(outfile)) {
			REQUIRE(std::filesystem::remove(outfile));
		}

		const std::array args{
			"compress"s,
			infile.string(),
			outfile.string()
		};

		REQUIRE(do_main(args) == bsa::xmem::error_code::ok);
		validate(wantfile, outfile);
	}

	SECTION("decompress")
	{
		const auto infile = root / "compressed.bin"sv;
		const auto wantfile = root / "decompressed.png"sv;
		const auto outfile = root / "output.png"sv;

		if (std::filesystem::exists(outfile)) {
			REQUIRE(std::filesystem::remove(outfile));
		}

		const std::array args{
			"decompress"s,
			infile.string(),
			outfile.string()
		};

		REQUIRE(do_main(args) == bsa::xmem::error_code::ok);
		validate(wantfile, outfile);
	}
}
