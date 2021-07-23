#include "pack_unpack.cpp"

#include "utility.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>

#include "bsa/bsa.hpp"

using namespace std::literals;

namespace
{
	[[nodiscard]] auto ci_compare_three_way(
		std::string_view a_lhs,
		std::string_view a_rhs) noexcept
		-> std::strong_ordering
	{
		for (std::size_t i = 0; i < (std::min)(a_lhs.size(), a_rhs.size()); ++i) {
			const auto l = std::tolower(a_lhs[i]);
			const auto r = std::tolower(a_rhs[i]);
			if (l != r) {
				return l <=> r;
			}
		}

		return a_lhs.length() <=> a_rhs.length();
	}

	struct ci_sort
	{
		using is_transparent = int;

		[[nodiscard]] bool operator()(
			std::string_view a_lhs,
			std::string_view a_rhs) const noexcept
		{
			return ci_compare_three_way(a_lhs, a_rhs) < 0;
		}
	};

	[[nodiscard]] auto get_files_from_directory(const std::filesystem::path& a_dir)
		-> std::set<std::string, ci_sort>
	{
		std::set<std::string, ci_sort> result;
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ a_dir }) {
			if (entry.is_regular_file()) {
				result.insert(
					entry
						.path()
						.lexically_relative(a_dir)
						.lexically_normal()
						.generic_string());
			}
		}

		return result;
	}
}

TEST_CASE("pack_unpack", "[bsa.examples.pack_unpack]")
{
	constexpr char prog[] = "pack_unpack";
	const std::filesystem::path root{ "examples_pack_unpack_test"sv };
	const auto datadir = (root / "data"sv).string();

	const auto invokeMain = [&](auto... a_args) {
		std::array args{ prog, a_args... };
		do_main(static_cast<int>(args.size()), args.data());
	};

	const auto checkUnpack = [](const std::filesystem::path& a_old,
								 const std::filesystem::path& a_new) {
		const auto oldf = get_files_from_directory(a_old);
		const auto newf = get_files_from_directory(a_new);
		REQUIRE(std::equal(
			oldf.begin(),
			oldf.end(),
			newf.begin(),
			newf.end(),
			[](auto&& a_lhs, auto&& a_rhs) { return ci_compare_three_way(a_lhs, a_rhs) == 0; }));

		for (auto lhs = oldf.begin(), rhs = newf.begin();
			 lhs != oldf.end() && rhs != newf.end();
			 ++lhs, ++rhs) {
			const auto lmap = map_file(a_old / *lhs);
			const auto rmap = map_file(a_new / *rhs);

			REQUIRE(lmap.is_open());
			REQUIRE(rmap.is_open());

			REQUIRE(lmap.size() == rmap.size());
			REQUIRE(std::memcmp(lmap.data(), rmap.data(), lmap.size()) == 0);
		}
	};

	const auto test = [&](std::string_view a_format,
						  std::string_view a_extension,
						  auto a_verifyPack) {
		const auto filename = [&]() {
			auto result = root;
			result /= a_format;
			result += '.';
			result += a_extension;
			return result.string();
		}();

		SECTION("packing")
		{
			const auto format = [&]() {
				std::string result;
				result += '-';
				result += a_format;
				return result;
			}();

			std::filesystem::remove(filename);
			invokeMain("pack", datadir.c_str(), filename.c_str(), format.c_str());
			a_verifyPack(filename);
		}

		SECTION("unpacking")
		{
			const auto outdir = (root / a_format).string();
			std::filesystem::remove_all(outdir);
			invokeMain("unpack", filename.c_str(), outdir.c_str());
			checkUnpack(datadir, outdir);
		}
	};

	SECTION("tes3")
	{
		test(
			"tes3"sv,
			"bsa"sv,
			[&](std::string_view a_outfile) {
				bsa::tes3::archive bsa;
				bsa.read(a_outfile);

				const auto files = get_files_from_directory(datadir);
				REQUIRE(bsa.size() == files.size());
				for (const auto& file : files) {
					REQUIRE(bsa[file]);
				}
			});
	}

	SECTION("tes4")
	{
		test(
			"tes4"sv,
			"bsa"sv,
			[&](std::string_view a_outfile) {
				bsa::tes4::archive bsa;
				bsa.read(a_outfile);

				const auto files = get_files_from_directory(datadir);
				const auto filecount = [&]() {
					std::size_t result = 0;
					for (const auto& dir : bsa) {
						result += dir.second.size();
					}
					return result;
				}();
				REQUIRE(filecount == files.size());

				for (const auto& file : files) {
					const auto [dir, filename] = [&]() -> std::pair<std::string_view, std::string_view> {
						const std::string_view view = file;
						const auto pos = view.find_last_of('/');
						if (pos != std::string_view::npos) {
							return {
								view.substr(0, pos),
								view.substr(pos + 1)
							};
						} else {
							return { ""sv, view };
						}
					}();

					REQUIRE(bsa[dir][filename]);
				}
			});
	}

	SECTION("fo4")
	{
		test(
			"fo4"sv,
			"ba2"sv,
			[&](std::string_view a_outfile) {
				bsa::fo4::archive ba2;
				ba2.read(a_outfile);

				const auto files = get_files_from_directory(datadir);
				REQUIRE(ba2.size() == files.size());
				for (const auto& file : files) {
					REQUIRE(ba2[file]);
				}
			});
	}

	SECTION("input validation")
	{
		REQUIRE_THROWS_WITH(
			invokeMain("pack"),
			make_substr_matcher("too few"sv));
		REQUIRE_THROWS_WITH(
			invokeMain("pack", "foo", "bar", "-tes4", "baz"),
			make_substr_matcher("too many"sv));
		REQUIRE_THROWS_WITH(
			invokeMain("foo", "bar", "baz"),
			make_substr_matcher("unrecognized operation"sv));
		REQUIRE_THROWS_WITH(
			invokeMain("pack", "foo", "bar", "-baz"),
			make_substr_matcher("unrecognized format"sv));
	}
}
