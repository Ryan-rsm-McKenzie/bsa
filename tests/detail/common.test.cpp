#include "utility.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#include <boost/nowide/cstdio.hpp>
#include <catch2/catch.hpp>

#include "bsa/detail/common.hpp"

using namespace std::literals;

namespace
{
	[[nodiscard]] auto open_file(
		std::filesystem::path a_path,
		const char* a_mode)
	{
		const auto close = [](std::FILE* a_file) noexcept {
			std::fclose(a_file);
		};

		std::filesystem::create_directories(a_path.parent_path());
		auto result = std::unique_ptr<std::FILE, decltype(close)>{
			boost::nowide::fopen(
				reinterpret_cast<const char*>(a_path.u8string().c_str()),
				a_mode),
			close
		};
		REQUIRE(result);
		return result;
	}
}

TEST_CASE("bsa::detail::iostream_t", "[bsa.io]")
{
	const std::filesystem::path root{ "common_io_test"sv };

	const char rawData[] =
		"\x01"
		"\x01\x02"
		"\x01\x02\x03\x04"
		"\x01\x02\x03\x04\x05\x06\x07\x08";
	std::span data{
		reinterpret_cast<const std::byte*>(rawData),
		sizeof(rawData) - 1
	};

	auto f = open_file(root / "in.bin"sv, "wb");
	std::fwrite(data.data(), 1, data.size_bytes(), f.get());
	f.reset();

	{
		const auto path = root / "in.bin"sv;

		SECTION("we can read data in little endian format")
		{
			bsa::detail::istream_t in{ path };

			REQUIRE(in.read<std::uint8_t>(std::endian::little) == 0x01);
			REQUIRE(in.read<std::uint16_t>(std::endian::little) == 0x0201);
			REQUIRE(in.read<std::uint32_t>(std::endian::little) == 0x04030201);
			REQUIRE(in.read<std::uint64_t>(std::endian::little) == 0x0807060504030201);
		}

		SECTION("we can read data in big endian format")
		{
			bsa::detail::istream_t in{ path };

			REQUIRE(in.read<std::uint8_t>(std::endian::big) == 0x01);
			REQUIRE(in.read<std::uint16_t>(std::endian::big) == 0x0102);
			REQUIRE(in.read<std::uint32_t>(std::endian::big) == 0x01020304);
			REQUIRE(in.read<std::uint64_t>(std::endian::big) == 0x0102030405060708);
		}
	}

	{
		const auto path = root / "out.bin"sv;
		const auto verify = [&]() {
			const auto in = map_file(path);
			REQUIRE(in.is_open());
			REQUIRE(in.size() == data.size_bytes());
			REQUIRE(std::memcmp(in.data(), data.data(), in.size()) == 0);
		};

		SECTION("we can write data in little endian format")
		{
			{
				bsa::detail::ostream_t out{ path };

				out.write<std::uint8_t>(0x01, std::endian::little);
				out.write<std::uint16_t>(0x0201, std::endian::little);
				out.write<std::uint32_t>(0x04030201, std::endian::little);
				out.write<std::uint64_t>(0x0807060504030201, std::endian::little);
			}

			verify();
		}

		SECTION("we can write data in big endian format")
		{
			{
				bsa::detail::ostream_t out{ path };

				out.write<std::uint8_t>(0x01, std::endian::big);
				out.write<std::uint16_t>(0x0102, std::endian::big);
				out.write<std::uint32_t>(0x01020304, std::endian::big);
				out.write<std::uint64_t>(0x0102030405060708, std::endian::big);
			}

			verify();
		}
	}
}
