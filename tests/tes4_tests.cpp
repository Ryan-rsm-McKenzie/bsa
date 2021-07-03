#include "bsa/tes4.hpp"

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

#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/regex.hpp>

#include "common.hpp"

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
}

static_assert(assert_nothrowable<bsa::tes4::hashing::hash>());
static_assert(assert_nothrowable<bsa::tes4::file>());
static_assert(assert_nothrowable<bsa::tes4::directory>());
static_assert(assert_nothrowable<bsa::tes4::archive>());

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
		REQUIRE(fhash("María_F.fuz"sv) == 0x6434BBA36D085F66);
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

	SECTION("root paths are included in directory names")
	{
		const auto h1 = hash_directory("C:\\foo\\bar\\baz"sv);
		const auto h2 = hash_directory("foo\\bar\\baz"sv);

		REQUIRE(h1 != h2);
	}

	SECTION("parent directories are not included in file names")
	{
		const auto h1 = hash_file("users/john/test.txt"sv);
		const auto h2 = hash_file("test.txt"sv);

		REQUIRE(h1 == h2);
	}
}

TEST_CASE("bsa::tes4::directory", "[tes4.directory]")
{
	SECTION("directories start empty")
	{
		const bsa::tes4::directory d;

		REQUIRE(d.empty());
		REQUIRE(d.size() == 0);
		REQUIRE(d.begin() == d.end());
	}
}

TEST_CASE("bsa::tes4::file", "[tes4.file]")
{
	SECTION("files start empty")
	{
		const bsa::tes4::file f;
		REQUIRE(!f.compressed());
		REQUIRE(f.empty());
		REQUIRE(f.size() == 0);
		REQUIRE(f.as_bytes().size() == 0);
	}

	SECTION("we can assign and clear the contents of a file")
	{
		auto payload = std::vector<std::byte>(1u << 4);
		bsa::tes4::file f;

		f.set_data({ payload.data(), payload.size() });
		REQUIRE(f.size() == payload.size());
		REQUIRE(f.data() == payload.data());
		REQUIRE(f.as_bytes().size() == payload.size());
		REQUIRE(f.as_bytes().data() == payload.data());

		f.clear();
		REQUIRE(f.empty());
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
		try {
			bsa::tes4::archive bsa;
			bsa.read("."sv);
			REQUIRE(false);
		} catch (const std::system_error&) {
			REQUIRE(true);
		}
	}

	SECTION("attempting to write to an invalid location will fail")
	{
		try {
			bsa::tes4::archive bsa;
			bsa.write("."sv, bsa::tes4::version::tes5);
			REQUIRE(false);
		} catch (const std::system_error&) {
			REQUIRE(true);
		}
	}

	{
		const auto testArchive = [](std::string_view a_name) {
			const std::filesystem::path root{ "tes4_compression_test"sv };

			bsa::tes4::archive bsa;
			const auto version = bsa.read(root / a_name);
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

				bsa::tes4::file original;
				const auto origsrc = map_file(p);
				original.set_data({ reinterpret_cast<const std::byte*>(origsrc.data()), origsrc.size() });
				REQUIRE(original.compress(version));

				REQUIRE(read->size() == original.size());
				REQUIRE(read->decompressed_size() == original.decompressed_size());
				REQUIRE(std::memcmp(read->data(), original.data(), original.size()) == 0);

				REQUIRE(read->decompress(version));
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
		const std::filesystem::path root{ "tes4_compression_mismatch_test"sv };

		bsa::tes4::archive bsa;
		bsa.read(root / "test.bsa"sv);
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

				bsa::tes4::file f;
				f.set_data(a_data);
				auto it = bsa.find(dir);
				if (it == bsa.end()) {
					it = bsa.insert(dir, bsa::tes4::directory{}).first;
				}
				REQUIRE(it != bsa.end());
				it->second.insert(a_hash, std::move(f));
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

	SECTION("we can write archives with a variety of flags")
	{
		const std::filesystem::path root{ "tes4_flags_test"sv };
		const std::filesystem::path outPath = root / "out.bsa"sv;

		struct info_t
		{
			struct pair_t
			{
				std::uint64_t hash;
				std::string_view name;
			};

			consteval info_t(
				std::uint64_t a_dhash,
				std::string_view a_dname,
				std::uint64_t a_fhash,
				std::string_view a_fname) noexcept :
				directory{ a_dhash, a_dname },
				file{ a_fhash, a_fname }
			{}

			pair_t directory;
			pair_t file;
		};

		constexpr std::array index{
			info_t{ 0x006819F973057265, "Share"sv, 0xDC415D456C077365, "License.txt"sv },
			info_t{ 0x00691A4374056573, "Tiles"sv, 0xDDE285B874093030, "tile_0000.png"sv },
			info_t{ 0x0E09AFBA620A6E64, "Background"sv, 0xC41A947762116F6D, "background_bottom.png"sv },
			info_t{ 0x4ADF420B74076170, "Tilemap"sv, 0x0D9BA627630A7273, "characters.png"sv },
			info_t{ 0x6A326CD4630B2033, "Construct 3"sv, 0xC7EDDCEA72066D65, "Readme.txt"sv },
			info_t{ 0x79CD3FEC630A7273, "Characters"sv, 0xD0E4FC14630E3030, "character_0000.png"sv },
		};

		std::vector<boost::iostreams::mapped_file_source> mmapped;
		bsa::tes4::archive in;
		for (const auto& [dir, file] : index) {
			const auto data = map_file(root / "data"sv / dir.name / file.name);
			REQUIRE(data.is_open());
			bsa::tes4::file f;
			f.set_data({ //
				reinterpret_cast<const std::byte*>(data.data()),
				data.size() });
			mmapped.push_back(std::move(data));

			bsa::tes4::directory d;
			REQUIRE(d.insert(file.name, std::move(f)).second);

			REQUIRE(in.insert(dir.name, std::move(d)).second);
		}

		constexpr std::array versions{
			bsa::tes4::version::tes4,
			bsa::tes4::version::tes5,
			bsa::tes4::version::sse,
		};
		constexpr std::array flags{
			bsa::tes4::archive_flag::directory_strings,
			bsa::tes4::archive_flag::file_strings,
			bsa::tes4::archive_flag::compressed,
			bsa::tes4::archive_flag::embedded_file_names,
		};

		for (const auto version : versions) {
			for (std::size_t i = 0; i < flags.size(); ++i) {
				auto af = flags[i];
				for (std::size_t j = i; j < flags.size(); ++j) {
					af |= flags[j];
					in.archive_flags(af);
					in.write(outPath, version);

					bsa::tes4::archive out;
					REQUIRE(out.read(outPath) == version);
					REQUIRE(out.size() == index.size());
					for (std::size_t idx = 0; idx < index.size(); ++idx) {
						const auto& [dir, file] = index[idx];
						const auto& mapped = mmapped[idx];

						REQUIRE(out[dir.name][file.name]);

						const auto d = out.find(dir.name);
						REQUIRE(d != out.end());
						REQUIRE(d->first.hash().numeric() == dir.hash);
						REQUIRE(d->second.size() == 1);

						const auto f = d->second.find(file.name);
						REQUIRE(f != d->second.end());
						REQUIRE(f->first.hash().numeric() == file.hash);
						if (f->second.compressed()) {
							REQUIRE(f->second.decompress(version));
						}
						REQUIRE(f->second.size() == mapped.size());
						REQUIRE(std::memcmp(f->second.data(), mapped.data(), mapped.size()) == 0);
					}
				}
			}
		}
	}

	SECTION("archives will bail on malformed inputs")
	{
		const std::filesystem::path root{ "tes4_invalid_test"sv };
		constexpr std::array types{
			"magic"sv,
			"version"sv,
			"size"sv,
			"range"sv,
		};

		for (const auto& type : types) {
			try {
				std::string filename;
				filename += "invalid_"sv;
				filename += type;
				filename += ".bsa"sv;

				bsa::tes4::archive bsa;
				bsa.read(root / filename);

				REQUIRE(false);
			} catch (bsa::exception& a_err) {
				std::string fmt;
				fmt += "\\b"sv;
				fmt += type;
				fmt += "\\b"sv;

				boost::regex pattern{
					fmt.c_str(),
					boost::regex_constants::ECMAScript | boost::regex_constants::icase
				};

				REQUIRE(boost::regex_search(a_err.what(), pattern));
			}
		}
	}
}
