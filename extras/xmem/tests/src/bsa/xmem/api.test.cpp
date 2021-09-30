#include "bsa/xmem/api.hpp"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>
#include <mmio/mmio.hpp>

#include "bsa/xmem/winapi.hpp"
#include "bsa/xmem/xcompress.hpp"

using namespace std::literals;

namespace api = bsa::xmem::api;
namespace winapi = bsa::xmem::winapi;
namespace xcompress = bsa::xmem::xcompress;

TEST_CASE("assert roundtrip compression works", "[src]")
{
	REQUIRE(xcompress::initialize());

	const std::filesystem::path root{ "roundtrip_test"sv };
	const auto validate = [](std::filesystem::path a_expected, std::span<const std::byte> a_got) {
		const mmio::mapped_file_source expected{ std::move(a_expected) };
		REQUIRE(expected.is_open());
		REQUIRE(expected.size() == a_got.size());
		REQUIRE(std::memcmp(expected.data(), a_got.data(), a_got.size()) == 0);
	};

	SECTION("compression")
	{
		const auto infile = root / "decompressed.png"sv;
		const auto outfile = root / "compressed.bin"sv;

		const auto compressor = api::create_compression_context();
		REQUIRE(compressor);

		const mmio::mapped_file_source in{ infile };
		REQUIRE(in.is_open());
		auto bound = api::compress_bound(compressor->get(), std::span{ in.data(), in.size() });
		REQUIRE(bound.has_value());
		std::vector<std::byte> out(*bound);
		const auto realsz = api::compress(compressor->get(), std::span{ in.data(), in.size() }, out);
		REQUIRE(realsz.has_value());
		out.resize(*realsz);

		validate(outfile, out);
	}

	SECTION("decompression")
	{
		const auto infile = root / "compressed.bin"sv;
		const auto outfile = root / "decompressed.png"sv;

		const auto decompressor = api::create_decompression_context();
		REQUIRE(decompressor);

		const mmio::mapped_file_source in{ infile };
		REQUIRE(in.is_open());
		std::vector<std::byte> out(in.size() * 2);
		const auto outsz = api::decompress(decompressor->get(), std::span{ in.data(), in.size() }, out);
		REQUIRE(outsz.has_value());
		out.resize(*outsz);

		validate(outfile, out);
	}
}
