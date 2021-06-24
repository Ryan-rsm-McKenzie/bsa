#include "bsa/tes4.hpp"

#include <cstring>
#include <exception>
#include <iterator>
#include <string_view>

#pragma warning(push)
#include <catch2/catch.hpp>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/nowide/cstdio.hpp>
#pragma warning(pop)

using namespace std::literals;

[[nodiscard]] auto fopen_path(std::filesystem::path a_path, const char* a_mode) noexcept
	-> std::FILE*
{
	return boost::nowide::fopen(
		reinterpret_cast<const char*>(a_path.u8string().data()),
		a_mode);
};

[[nodiscard]] auto hash_directory(std::filesystem::path a_path) noexcept
	-> bsa::tes4::hashing::hash
{
	return bsa::tes4::hashing::hash_directory(a_path);
}

[[nodiscard]] auto hash_file(std::filesystem::path a_path) noexcept
	-> bsa::tes4::hashing::hash
{
	return bsa::tes4::hashing::hash_file(a_path);
}

[[nodiscard]] auto map_file(const std::filesystem::path& a_path)
	-> boost::iostreams::mapped_file_source
{
	return boost::iostreams::mapped_file_source{
		boost::filesystem::path{ a_path.native() }
	};
};

TEST_CASE("bsa::tes4::hashing", "[tes4.hashing]")
{
	SECTION("validate hash values")
	{
		const auto h = hash_file(u8"testtoddquest_testtoddhappy_00027fa2_1.mp3"sv);

		REQUIRE(h.numeric() == 0xDE0301EE74265F31);
	}

	SECTION("the empty path \"\" is equivalent to the current path \".\"")
	{
		const auto empty = hash_directory(u8""sv);
		const auto current = hash_directory(u8"."sv);

		REQUIRE(empty == current);
	}

	SECTION("archive.exe detects file extensions incorrectly")
	{
		// archive.exe uses _splitpath_s under the hood
		const auto gitignore =
			hash_file(u8".gitignore"sv);  // stem == "", extension == ".gitignore"
		const auto gitmodules =
			hash_file(u8".gitmodules"sv);  // stem == "", extension == ".gitmodules"

		REQUIRE(gitignore == gitmodules);
		REQUIRE(gitignore.first == '\0');
		REQUIRE(gitignore.last2 == '\0');
		REQUIRE(gitignore.last == '\0');
		REQUIRE(gitignore.length == 0);
		REQUIRE(gitignore.crc == 0);
		REQUIRE(gitignore.numeric() == 0);
	}

	SECTION("drive letters are included in hashes")
	{
		const auto h1 = hash_directory(u8"C:\\foo\\bar\\baz"sv);
		const auto h2 = hash_directory(u8"foo\\bar\\baz"sv);

		REQUIRE(h1 != h2);
	}

	SECTION("directory names longer than 259 characters are equivalent to the empty path")
	{
		const auto looong = hash_directory(u8"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);
		const auto empty = hash_directory(u8""sv);

		REQUIRE(looong == empty);
	}

	SECTION("file names longer than 259 characters will fail")
	{
		// actually, anything longer than MAX_PATH will crash archive.exe

		const auto good = hash_file(u8"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);
		const auto bad = hash_file(u8"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);

		REQUIRE(good.numeric() != 0);
		REQUIRE(bad.numeric() == 0);
	}

	SECTION("file extensions longer than 14 characters will fail")
	{
		const auto good = hash_file(u8"test.123456789ABCDE"sv);
		const auto bad = hash_file(u8"test.123456789ABCDEF"sv);

		REQUIRE(good.numeric() != 0);
		REQUIRE(bad.numeric() == 0);
	}
}

TEST_CASE("bsa::tes4::directory", "[tes4.directory]")
{
	SECTION("drive letters are included in directory names")
	{
		const bsa::tes4::directory d1{ u8"C:\\foo\\bar\\baz"sv };
		const bsa::tes4::directory d2{ u8"foo\\bar\\baz"sv };

		REQUIRE(d1.name() != d2.name());
	}
}

TEST_CASE("bsa::tes4::file", "[tes4.file]")
{
	SECTION("parent directories are not included in file names")
	{
		const bsa::tes4::file f1{ u8"C:\\users\\john\\test.txt"sv };
		const bsa::tes4::file f2{ u8"test.txt"sv };

		REQUIRE(f1.filename() == f2.filename());
	}
}

TEST_CASE("bsa::tes4::archive", "[tes4.archive]")
{
	SECTION("archives start empty")
	{
		bsa::tes4::archive bsa;

		REQUIRE(bsa.empty());
		REQUIRE(bsa.size() == 0);
		REQUIRE(std::distance(bsa.begin(), bsa.end()) == std::ssize(bsa));

		REQUIRE(bsa.archive_flags() == bsa::tes4::archive_flag::none);
		REQUIRE(bsa.archive_types() == bsa::tes4::archive_type::none);

		REQUIRE(!bsa.compressed());
		REQUIRE(!bsa.directory_strings());
		REQUIRE(!bsa.embedded_file_names());
		REQUIRE(!bsa.file_strings());
		REQUIRE(!bsa.retain_directory_names());
		REQUIRE(!bsa.retain_file_name_offsets());
		REQUIRE(!bsa.retain_file_names());
		REQUIRE(!bsa.retain_strings_during_startup());
		REQUIRE(!bsa.xbox_archive());
		REQUIRE(!bsa.xbox_compressed());

		REQUIRE(!bsa.fonts());
		REQUIRE(!bsa.menus());
		REQUIRE(!bsa.meshes());
		REQUIRE(!bsa.misc());
		REQUIRE(!bsa.shaders());
		REQUIRE(!bsa.sounds());
		REQUIRE(!bsa.textures());
		REQUIRE(!bsa.trees());
		REQUIRE(!bsa.voices());
	}

	SECTION("attempting to read an invalid file will fail")
	{
		bsa::tes4::archive bsa;
		REQUIRE(!bsa.read(u8"."sv));
	}

	SECTION("files can be compressed within an archive")
	{
		const std::filesystem::path root{ u8"compression_test"sv };

		bsa::tes4::archive bsa;
		const auto version = bsa.read(root / u8"test.bsa"sv);
		REQUIRE(version);
		REQUIRE(bsa.compressed());

		constexpr std::array files{
			u8"License.txt"sv,
			u8"Preview.png"sv,
		};

		for (const auto& name : files) {
			const auto p = root / name;
			REQUIRE(std::filesystem::exists(p));

			const auto read = bsa[u8"."sv][name];
			REQUIRE(read);
			REQUIRE(read->compressed());
			REQUIRE(read->decompressed_size() == std::filesystem::file_size(p));

			bsa::tes4::file original{ "" };
			const auto origsrc = map_file(p);
			original.set_data({ reinterpret_cast<const std::byte*>(origsrc.data()), origsrc.size() });
			REQUIRE(original.compress(*version));

			REQUIRE(read->size() == original.size());
			REQUIRE(read->decompressed_size() == original.decompressed_size());
			REQUIRE(std::memcmp(read->data(), original.data(), original.size()) == 0);

			REQUIRE(read->decompress(*version));
			REQUIRE(read->size() == origsrc.size());
			REQUIRE(std::memcmp(read->data(), origsrc.data(), origsrc.size()) == 0);
		}
	}

	SECTION("files can be compressed independently of the archive's compression")
	{
		const std::filesystem::path root{ u8"compression_mismatch_test"sv };

		bsa::tes4::archive bsa;
		REQUIRE(bsa.read(root / u8"test.bsa"sv));
		REQUIRE(bsa.compressed());

		constexpr std::array files{
			u8"License.txt"sv,
			u8"SampleA.png"sv,
		};

		for (const auto& name : files) {
			const auto p = root / name;
			REQUIRE(std::filesystem::exists(p));

			const auto file = bsa[u8"."sv][name];
			REQUIRE(file);
			REQUIRE(!file->compressed());
			REQUIRE(file->size() == std::filesystem::file_size(p));
		}
	}
}
