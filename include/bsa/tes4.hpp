#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "bsa/detail/common.hpp"

namespace bsa::tes4
{
	enum class archive_flag : std::uint32_t
	{
		none = 0u,

		directory_strings = 1u << 0u,
		file_strings = 1u << 1u,
		compressed = 1u << 2u,
		retain_directory_names = 1u << 3u,
		retain_file_names = 1u << 4u,
		retain_file_name_offsets = 1 << 5u,
		xbox_archive = 1u << 6u,
		retain_strings_during_startup = 1u << 7u,
		embedded_file_names = 1u << 8u,
		xbox_compressed = 1u << 9u
	};

	BSA_MAKE_ALL_ENUM_OPERATORS(archive_flag)

	enum class archive_type : std::uint16_t
	{
		none = 0u,

		meshes = 1u << 0u,
		textures = 1u << 1u,
		menus = 1u << 2u,
		sounds = 1u << 3u,
		voices = 1u << 4u,
		shaders = 1u << 5u,
		trees = 1u << 6u,
		fonts = 1u << 7u,
		misc = 1u << 8u
	};

	BSA_MAKE_ALL_ENUM_OPERATORS(archive_type)

	enum class version : std::uint32_t
	{
		tes4 = 103,  // The Elder Scrolls IV: Oblivion
		fo3 = 104,   // Fallout 3
		tes5 = 104,  // The Elder Scrolls V: Skyrim
		sse = 105,   // The Elder Scrolls V: Skyrim - Special Edition
	};

	namespace detail
	{
		using namespace bsa::detail;
	}

	namespace hashing
	{
		namespace detail = tes4::detail;

		struct hash final
		{
		public:
			std::uint8_t last{ 0 };
			std::uint8_t last2{ 0 };
			std::uint8_t length{ 0 };
			std::uint8_t first{ 0 };
			std::uint32_t crc{ 0 };

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;

			[[nodiscard]] friend auto operator<=>(const hash& a_lhs, const hash& a_rhs) noexcept
				-> std::strong_ordering { return a_lhs.numeric() <=> a_rhs.numeric(); }

			[[nodiscard]] auto numeric() const noexcept
				-> std::uint64_t
			{
				return std::uint64_t{
					std::uint64_t{ last } << 0u * 8u |
					std::uint64_t{ last2 } << 1u * 8u |
					std::uint64_t{ length } << 2u * 8u |
					std::uint64_t{ first } << 3u * 8u |
					std::uint64_t{ crc } << 4u * 8u
				};
			}

		private:
			friend tes4::archive;
			friend tes4::directory;

			void read(
				detail::istream_t& a_in,
				std::endian a_endian);

			void write(
				detail::ostream_t& a_out,
				std::endian a_endian) const noexcept;
		};

		[[nodiscard]] hash hash_directory(std::string& a_path) noexcept;
		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class file final :
		public detail::components::compressed_byte_container
	{
	private:
		friend archive;
		using super = detail::components::compressed_byte_container;

		enum : std::uint32_t
		{
			icompression = 1u << 30u,
			ichecked = 1u << 31u,

			isecondary_archive = 1u << 31u
		};

	public:
		using key = detail::key_t<hashing::hash, hashing::hash_file>;
		using super::clear;

		bool compress(version a_version) noexcept;
		bool decompress(version a_version) noexcept;
	};

	class directory final :
		public detail::components::hashmap<file>
	{
	private:
		friend archive;
		using super = detail::components::hashmap<file>;

	public:
		using key = detail::key_t<hashing::hash, hashing::hash_directory>;
		using super::clear;
	};

	class archive final :
		public detail::components::hashmap<directory, true>
	{
	private:
		using super = detail::components::hashmap<directory, true>;

	public:
		[[nodiscard]] auto archive_flags() const noexcept -> archive_flag { return _flags; }
		void archive_flags(archive_flag a_flags) noexcept { _flags = a_flags; }

		[[nodiscard]] auto archive_types() const noexcept -> archive_type { return _types; }
		void archive_types(archive_type a_types) noexcept { _types = a_types; }

		[[nodiscard]] auto compressed() const noexcept
			-> bool { return test_flag(archive_flag::compressed); }
		[[nodiscard]] auto directory_strings() const noexcept
			-> bool { return test_flag(archive_flag::directory_strings); }
		[[nodiscard]] auto embedded_file_names() const noexcept
			-> bool { return test_flag(archive_flag::embedded_file_names); }
		[[nodiscard]] auto file_strings() const noexcept
			-> bool { return test_flag(archive_flag::file_strings); }
		[[nodiscard]] auto retain_directory_names() const noexcept
			-> bool { return test_flag(archive_flag::retain_directory_names); }
		[[nodiscard]] auto retain_file_name_offsets() const noexcept
			-> bool { return test_flag(archive_flag::retain_file_name_offsets); }
		[[nodiscard]] auto retain_file_names() const noexcept
			-> bool { return test_flag(archive_flag::retain_file_names); }
		[[nodiscard]] auto retain_strings_during_startup() const noexcept
			-> bool { return test_flag(archive_flag::retain_strings_during_startup); }
		[[nodiscard]] auto xbox_archive() const noexcept
			-> bool { return test_flag(archive_flag::xbox_archive); }
		[[nodiscard]] auto xbox_compressed() const noexcept
			-> bool { return test_flag(archive_flag::xbox_compressed); }

		[[nodiscard]] bool fonts() const noexcept { return test_type(archive_type::fonts); }
		[[nodiscard]] bool menus() const noexcept { return test_type(archive_type::menus); }
		[[nodiscard]] bool meshes() const noexcept { return test_type(archive_type::meshes); }
		[[nodiscard]] bool misc() const noexcept { return test_type(archive_type::misc); }
		[[nodiscard]] bool shaders() const noexcept { return test_type(archive_type::shaders); }
		[[nodiscard]] bool sounds() const noexcept { return test_type(archive_type::sounds); }
		[[nodiscard]] bool textures() const noexcept { return test_type(archive_type::textures); }
		[[nodiscard]] bool trees() const noexcept { return test_type(archive_type::trees); }
		[[nodiscard]] bool voices() const noexcept { return test_type(archive_type::voices); }

		void clear() noexcept
		{
			super::clear();
			_flags = archive_flag::none;
			_types = archive_type::none;
		}

		auto read(std::filesystem::path a_path) -> version;
		[[nodiscard]] bool verify_offsets(version a_version) const noexcept;
		void write(std::filesystem::path a_path, version a_version) const;

	private:
		[[nodiscard]] auto make_header(version a_version) const noexcept -> detail::header_t;

		[[nodiscard]] auto read_file_entries(
			directory& a_dir,
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_count,
			std::size_t& a_namesOffset) -> std::optional<std::string_view>;

		void read_file_data(
			file& a_file,
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_size);

		void read_directory(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t& a_namesOffset);

		[[nodiscard]] auto test_flag(archive_flag a_flag) const noexcept
			-> bool { return (_flags & a_flag) != archive_flag::none; }

		[[nodiscard]] auto test_type(archive_type a_type) const noexcept
			-> bool { return (_types & a_type) != archive_type::none; }

		void write_directory_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_names(detail::ostream_t& a_out) const noexcept;

		archive_flag _flags{ archive_flag::none };
		archive_type _types{ archive_type::none };
	};
}
