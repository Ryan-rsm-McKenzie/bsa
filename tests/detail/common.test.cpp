#include "utility.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <catch2/catch.hpp>

#include "bsa/detail/common.hpp"

TEST_CASE("bsa::functional", "[bsa.functional]")
{
	SECTION("guess_file_format")
	{
		const std::filesystem::path root{ "common_guess_test"sv };

#define FORMAT(a_fmt, a_ext) std::make_pair(#a_fmt "." #a_ext##sv, bsa::file_format::a_fmt)
		std::array formats{
			FORMAT(fo4, ba2),
			FORMAT(tes3, bsa),
			FORMAT(tes4, bsa),
		};
#undef FORMAT

		for (const auto& [name, type] : formats) {
			const auto guessed = bsa::guess_file_format(root / name);
			REQUIRE(guessed);
			REQUIRE(*guessed == type);
		}

		REQUIRE(!bsa::guess_file_format(root / "data/misc/example.txt"sv));
		REQUIRE_THROWS_AS(bsa::guess_file_format(root / "foo.bar"sv), std::system_error);
	}

	SECTION("make_four_cc")
	{
		REQUIRE(bsa::make_four_cc(""sv) == 0x00000000);
		REQUIRE(bsa::make_four_cc("A"sv) == 0x00000041);
		REQUIRE(bsa::make_four_cc("AB"sv) == 0x00004241);
		REQUIRE(bsa::make_four_cc("ABC"sv) == 0x00434241);
		REQUIRE(bsa::make_four_cc("ABCD"sv) == 0x44434241);
		REQUIRE(bsa::make_four_cc("ABCDE"sv) == 0x44434241);
	}
}

TEST_CASE("bsa::detail::endian", "[bsa.endian]")
{
	const auto test = []<class T>(std::in_place_type_t<T>, std::size_t a_little, std::size_t a_big) {
		// test against unaligned memory
		const char payload[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08";
		std::array<char, sizeof(payload) - 1> buffer{};

		const auto readable = std::as_bytes(std::span{ payload }).subspan<1, sizeof(T)>();
		const auto writable = std::as_writable_bytes(std::span{ buffer }).subspan<1, sizeof(T)>();

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
	SECTION("we can open files with utf-8 characters")
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
}

TEST_CASE("bsa::detail::mmio", "[bsa.mmio]")
{
	SECTION("we can perform read-only memory-mapped io")
	{
		const std::filesystem::path root{ "common_mmio_test"sv };
		const std::filesystem::path path = root / "out.txt"sv;

		const char payload[] = "The quick brown fox jumps over the lazy dog\n";
		const std::size_t size = sizeof(payload) - 1;

		{
			const auto f = open_file(path, "wb");
			REQUIRE(f != nullptr);
			REQUIRE(std::fwrite(payload, 1, size, f.get()) == size);
		}

		{
			const auto isOpen = [](const bsa::detail::mmio::file& a_file) {
				return a_file.is_open() &&
				       a_file.data() != nullptr &&
				       a_file.size() != 0;
			};
			const auto isClosed = [](const bsa::detail::mmio::file& a_file) {
				return !a_file.is_open() &&
				       a_file.data() == nullptr &&
				       a_file.size() == 0;
			};

			bsa::detail::mmio::file map;
			REQUIRE(isClosed(map));

			REQUIRE(map.open(path));
			REQUIRE(isOpen(map));

			bsa::detail::mmio::file map2{ std::move(map) };
			REQUIRE(isClosed(map));
			REQUIRE(isOpen(map2));

			map = std::move(map2);
			REQUIRE(isOpen(map));
			REQUIRE(isClosed(map2));

			REQUIRE(map.size() == size);
			REQUIRE(std::memcmp(map.data(), payload, size) == 0);
		}
	}
}

TEST_CASE("bsa::detail::iostream_t", "[bsa.io]")
{
	const std::filesystem::path root{ "common_io_test"sv };

	SECTION("we can read/write binary data")
	{
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
}
