#include "bsa/tes4.hpp"

#include <array>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/nowide/cstdio.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;

namespace
{
	[[nodiscard]] auto hash_directory(std::string_view a_path) noexcept
		-> bsa::tes4::hashing::hash
	{
		std::string t(a_path);
		return bsa::tes4::hashing::hash_directory(t);
	}

	[[nodiscard]] auto hash_file(std::string_view a_path) noexcept
		-> bsa::tes4::hashing::hash
	{
		std::string t(a_path);
		return bsa::tes4::hashing::hash_file(t);
	}

	[[nodiscard]] auto map_file(const std::filesystem::path& a_path)
		-> boost::iostreams::mapped_file_source
	{
		return boost::iostreams::mapped_file_source{
			boost::filesystem::path{ a_path.native() }
		};
	}
}

TEST_CASE("bsa::tes4::hashing", "[tes4.hashing]")
{
	SECTION("validate hash values")
	{
		const auto dhash = [](std::string_view a_path) {
			return hash_directory(a_path).numeric();
		};
		const auto fhash = [](std::string_view a_path) {
			return hash_file(a_path).numeric();
		};

		REQUIRE(dhash("textures/armor/amuletsandrings/elder council"sv) == 0x04BC422C742C696C);
		REQUIRE(dhash("sound/voice/skyrim.esm/maleuniquedbguardian"sv) == 0x594085AC732B616E);
		REQUIRE(dhash("textures/architecture/windhelm"sv) == 0xC1D97EBE741E6C6D);

		REQUIRE(fhash("darkbrotherhood__0007469a_1.fuz"sv) == 0x011F11B0641B5F31);
		REQUIRE(fhash("elder_council_amulet_n.dds"sv) == 0xDC531E2F6516DFEE);
		REQUIRE(fhash("testtoddquest_testtoddhappy_00027fa2_1.mp3"sv) == 0xDE0301EE74265F31);
	}

	SECTION("the empty path \"\" is equivalent to the current path \".\"")
	{
		const auto empty = hash_directory(""sv);
		const auto current = hash_directory("."sv);

		REQUIRE(empty == current);
	}

	SECTION("archive.exe detects file extensions incorrectly")
	{
		// archive.exe uses _splitpath_s under the hood
		const auto gitignore =
			hash_file(".gitignore"sv);  // stem == "", extension == ".gitignore"
		const auto gitmodules =
			hash_file(".gitmodules"sv);  // stem == "", extension == ".gitmodules"

		REQUIRE(gitignore == gitmodules);
		REQUIRE(gitignore.first == '\0');
		REQUIRE(gitignore.last2 == '\0');
		REQUIRE(gitignore.last == '\0');
		REQUIRE(gitignore.length == 0);
		REQUIRE(gitignore.crc == 0);
		REQUIRE(gitignore.numeric() == 0);
	}

	SECTION("root paths are included in hashes")
	{
		const auto h1 = hash_directory("C:\\foo\\bar\\baz"sv);
		const auto h2 = hash_directory("foo/bar/baz"sv);

		REQUIRE(h1 != h2);
	}

	SECTION("directory names longer than 259 characters are equivalent to the empty path")
	{
		const auto looong = hash_directory("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);
		const auto empty = hash_directory(""sv);

		REQUIRE(looong == empty);
	}

	SECTION("file names longer than 259 characters will fail")
	{
		// actually, anything longer than MAX_PATH will crash archive.exe

		const auto good = hash_file("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);
		const auto bad = hash_file("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"sv);

		REQUIRE(good.numeric() != 0);
		REQUIRE(bad.numeric() == 0);
	}

	SECTION("file extensions longer than 14 characters will fail")
	{
		const auto good = hash_file("test.123456789ABCDE"sv);
		const auto bad = hash_file("test.123456789ABCDEF"sv);

		REQUIRE(good.numeric() != 0);
		REQUIRE(bad.numeric() == 0);
	}
}

TEST_CASE("bsa::tes4::directory", "[tes4.directory]")
{
	SECTION("directories start empty")
	{
		const bsa::tes4::directory d{ "root"sv };

		REQUIRE(d.empty());
		REQUIRE(d.size() == 0);
		REQUIRE(d.begin() == d.end());
	}

	SECTION("root paths are included in directory names")
	{
		const bsa::tes4::directory d1{ "C:\\foo\\bar\\baz"sv };
		const bsa::tes4::directory d2{ "foo\\bar\\baz"sv };

		REQUIRE(d1.name() != d2.name());
	}

	SECTION("moving a directory does not modify its hash or name")
	{
		const auto name = "root"sv;
		const auto hash = hash_directory(name);
		bsa::tes4::directory oldd{ name };
		bsa::tes4::directory newd{ std::move(oldd) };

		const auto validate = [&]() {
			REQUIRE(oldd.name() == name);
			REQUIRE(oldd.hash() == hash);
			REQUIRE(newd.name() == name);
			REQUIRE(newd.hash() == hash);
		};

		validate();
		newd = std::move(oldd);
		validate();
	}
}

TEST_CASE("bsa::tes4::file", "[tes4.file]")
{
	SECTION("files start empty")
	{
		const bsa::tes4::file f{ "hello.txt"sv };
		REQUIRE(!f.compressed());
		REQUIRE(f.empty());
		REQUIRE(f.size() == 0);
		REQUIRE(f.as_bytes().size() == 0);
	}

	SECTION("parent directories are not included in file names")
	{
		const bsa::tes4::file f1{ "users/john/test.txt"sv };
		const bsa::tes4::file f2{ "test.txt"sv };

		REQUIRE(f1.filename() == f2.filename());
	}

	SECTION("we can assign and clear the contents of a file")
	{
		auto payload = std::vector<std::byte>(1u << 4);
		bsa::tes4::file f{ "hello.txt"sv };

		f.set_data({ payload.data(), payload.size() });
		REQUIRE(f.size() == payload.size());
		REQUIRE(f.data() == payload.data());
		REQUIRE(f.as_bytes().size() == payload.size());
		REQUIRE(f.as_bytes().data() == payload.data());

		f.clear();
		REQUIRE(f.empty());
	}

	SECTION("moving a file does not modify its hash or name")
	{
		const auto name = "hello.txt"sv;
		const auto hash = hash_file(name);
		bsa::tes4::file oldf{ name };
		bsa::tes4::file newf{ std::move(oldf) };

		const auto validate = [&]() {
			REQUIRE(oldf.filename() == name);
			REQUIRE(oldf.hash() == hash);
			REQUIRE(newf.filename() == name);
			REQUIRE(newf.hash() == hash);
		};

		validate();
		newf = std::move(oldf);
		validate();
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
		REQUIRE(!bsa.read("."sv));
	}

	{
		const auto testArchive = [](std::string_view a_name) {
			const std::filesystem::path root{ "compression_test"sv };

			bsa::tes4::archive bsa;
			const auto version = bsa.read(root / a_name);
			REQUIRE(version);
			REQUIRE(bsa.compressed());

			constexpr std::array files{
				"License.txt"sv,
				"Preview.png"sv,
			};

			for (const auto& name : files) {
				const auto p = root / name;
				REQUIRE(std::filesystem::exists(p));

				const auto read = bsa["."sv][name];
				REQUIRE(read);
				REQUIRE(read->compressed());
				REQUIRE(read->decompressed_size() == std::filesystem::file_size(p));

				bsa::tes4::file original{ ""sv };
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
		};

		SECTION("we can read files compressed in the v104 format")
		{
			testArchive("test_104.bsa"sv);
		}

		SECTION("we can read files compressed in the v105 format")
		{
			testArchive("test_105.bsa"sv);
		}
	}

	SECTION("files can be compressed independently of the archive's compression")
	{
		const std::filesystem::path root{ "compression_mismatch_test"sv };

		bsa::tes4::archive bsa;
		REQUIRE(bsa.read(root / "test.bsa"sv));
		REQUIRE(bsa.compressed());

		constexpr std::array files{
			"License.txt"sv,
			"SampleA.png"sv,
		};

		for (const auto& name : files) {
			const auto p = root / name;
			REQUIRE(std::filesystem::exists(p));

			const auto file = bsa["."sv][name];
			REQUIRE(file);
			REQUIRE(!file->compressed());
			REQUIRE(file->size() == std::filesystem::file_size(p));
		}
	}

	SECTION("we can validate the offsets within an archive (<2gb)")
	{
		bsa::tes4::archive bsa;
		const auto add =
			[&](bsa::tes4::hashing::hash a_hash,
				std::span<const std::byte> a_data) {
				constexpr auto dir = "root"sv;

				bsa::tes4::file f{ a_hash };
				f.set_data(a_data);
				auto it = bsa.find(dir);
				if (it == bsa.end()) {
					it = bsa.insert(bsa::tes4::directory{ dir }).first;
				}
				REQUIRE(it != bsa.end());
				it->insert(std::move(f));
			};

		constexpr auto largesz = std::numeric_limits<std::int32_t>::max();
		const auto plarge = std::make_unique<std::byte[]>(largesz);
		const std::span large{ plarge.get(), largesz };

		const std::array<std::byte, 1u << 4> smallbuf{};
		const std::span small{ smallbuf.data(), smallbuf.size() };

		const auto verify = [&]() {
			return bsa.verify_offsets(bsa::tes4::version::tes4);
		};

		REQUIRE(verify());

		add({ 0 }, small);
		REQUIRE(verify());

		add({ 1 }, large);
		REQUIRE(verify());

		bsa.clear();
		add({ 0 }, large);
		REQUIRE(verify());

		add({ 1 }, small);
		REQUIRE(!verify());
	}
}
