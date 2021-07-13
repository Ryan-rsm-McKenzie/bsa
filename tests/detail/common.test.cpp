#include "utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

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
			bsa::detail::unicode::fopen(a_path, a_mode),
			close
		};
		REQUIRE(result);
		return result;
	}
}

TEST_CASE("bsa::detail::endian", "[bsa.endian]")
{
	const auto test = []<class T>(std::in_place_type_t<T>, std::size_t a_little, std::size_t a_big) {
		const char payload[] = "\x01\x02\x03\x04\x05\x06\x07\x08";
		std::array<char, 8> buffer{};

		const auto readable = std::as_bytes(std::span{ payload }).subspan<0, sizeof(T)>();
		const auto writable = std::as_writable_bytes(std::span{ buffer }).subspan<0, sizeof(T)>();

		SECTION("reverse")
		{
			REQUIRE(a_little == bsa::detail::endian::reverse(static_cast<T>(a_big)));
		}

		SECTION("load little-endian")
		{
			const auto i = bsa::detail::endian::load<std::endian::little, T>(readable);
			REQUIRE(i == a_little);
		}

		SECTION("load big-endian")
		{
			const auto i = bsa::detail::endian::load<std::endian::big, T>(readable);
			REQUIRE(i == a_big);
		}

		SECTION("store little-endian")
		{
			bsa::detail::endian::store<std::endian::little>(writable, static_cast<T>(a_little));
			REQUIRE(std::ranges::equal(readable, writable));
		}

		SECTION("store big-endian")
		{
			bsa::detail::endian::store<std::endian::big>(writable, static_cast<T>(a_big));
			REQUIRE(std::ranges::equal(readable, writable));
		}
	};

	SECTION("1 byte")
	{
		test(std::in_place_type<std::uint8_t>, 0x01, 0x01);
	}

	SECTION("2 bytes")
	{
		test(std::in_place_type<std::uint16_t>, 0x0201, 0x0102);
	}

	SECTION("4 bytes")
	{
		test(std::in_place_type<std::uint32_t>, 0x04030201, 0x01020304);
	}

	SECTION("8 bytes")
	{
		test(std::in_place_type<std::uint64_t>, 0x0807060504030201, 0x0102030405060708);
	}
}

TEST_CASE("bsa::detail::unicode", "[bsa.unicode]")
{
	const std::filesystem::path root{ "common_unicode_test"sv };
	const std::filesystem::path path = root / u8"\u1E9E"sv;
	const char payload[] = "hello world!\n";

	std::filesystem::create_directory(root);

	auto f = bsa::detail::unicode::fopen(path, "wb");
	REQUIRE(f != nullptr);
	REQUIRE(std::fwrite(payload, 1, sizeof(payload) - 1, f) == sizeof(payload) - 1);
	REQUIRE(std::fclose(f) == 0);

	f = bsa::detail::unicode::fopen(path, "rb");
	REQUIRE(f != nullptr);
	std::array<char, sizeof(payload) - 1> buf{};
	REQUIRE(std::fread(buf.data(), 1, buf.size(), f) == buf.size());
	REQUIRE(std::memcmp(payload, buf.data(), buf.size()) == 0);
	REQUIRE(std::fclose(f) == 0);
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
