#pragma once

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/map.hpp>

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
				std::endian a_endian) noexcept;

			void write(
				detail::ostream_t& a_out,
				std::endian a_endian) const noexcept;
		};

		[[nodiscard]] hash hash_directory(std::string& a_path) noexcept;
		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class file final
	{
	public:
		using key = detail::key_t<hashing::hash, hashing::hash_file>;

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;

		void clear() noexcept;

		bool compress(version a_version) noexcept;

		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }

		[[nodiscard]] auto data() const noexcept -> const std::byte* { return as_bytes().data(); }

		bool decompress(version a_version) noexcept;

		[[nodiscard]] auto decompressed_size() const noexcept
			-> std::size_t { return _decompsz ? *_decompsz : size(); }

		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

		void set_data(
			std::span<const std::byte> a_data,
			std::optional<std::size_t> a_decompressedSize = std::nullopt) noexcept
		{
			_data.emplace<data_view>(a_data);
			_decompsz = a_decompressedSize;
		}

		void set_data(
			std::vector<std::byte> a_data,
			std::optional<std::size_t> a_decompressedSize = std::nullopt) noexcept
		{
			_data.emplace<data_owner>(std::move(a_data));
			_decompsz = a_decompressedSize;
		}

		[[nodiscard]] auto size() const noexcept -> std::size_t { return as_bytes().size(); }

	private:
		friend directory;

		enum : std::uint32_t
		{
			icompression = 1u << 30u,
			ichecked = 1u << 31u,

			isecondary_archive = 1u << 31u
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		using data_proxy = detail::istream_proxy<std::span<const std::byte>>;

		void read_data(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_size) noexcept;

		void write_data(detail::ostream_t& a_out) const noexcept;

		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;
		std::optional<std::size_t> _decompsz;

		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class directory final
	{
	private:
		using container_type =
			boost::container::map<file::key, file>;

	public:
		using key_type = container_type::key_type;
		using mapped_type = container_type::mapped_type;
		using value_type = container_type::value_type;
		using key_compare = container_type::key_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<mapped_type, false>;
		using const_index = detail::index_t<const mapped_type, false>;

		using key = detail::key_t<hashing::hash, hashing::hash_directory>;

		directory() noexcept = default;
		directory(const directory&) noexcept = default;
		directory(directory&&) noexcept = default;
		~directory() noexcept = default;
		directory& operator=(const directory&) noexcept = default;
		directory& operator=(directory&&) noexcept = default;

		[[nodiscard]] auto operator[](const key_type& a_key) noexcept
			-> index
		{
			const auto it = _files.find(a_key);
			return it != _files.end() ? index{ it->second } : index{};
		}

		[[nodiscard]] auto operator[](const key_type& a_key) const noexcept
			-> const_index
		{
			const auto it = _files.find(a_key);
			return it != _files.end() ? const_index{ it->second } : const_index{};
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		void clear() noexcept { _files.clear(); }

		[[nodiscard]] bool empty() const noexcept { return _files.empty(); }

		bool erase(const key_type& a_key) noexcept;

		[[nodiscard]] auto find(const key_type& a_key) noexcept
			-> iterator { return _files.find(a_key); }

		[[nodiscard]] auto find(const key_type& a_key) const noexcept
			-> const_iterator { return _files.find(a_key); }

		auto insert(key_type a_key, mapped_type a_file) noexcept
			-> std::pair<iterator, bool>
		{
			return _files.emplace(std::move(a_key), std::move(a_file));
		}

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _files.size(); }

	private:
		friend archive;

		void read_file_names(detail::istream_t& a_in) noexcept;

		[[nodiscard]] auto read_files(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_count) noexcept -> std::optional<std::string_view>;

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::string_view a_dirname) const noexcept;

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::uint32_t& a_dataOffset) const noexcept;

		void write_file_names(detail::ostream_t& a_out) const noexcept;

		container_type _files;
	};

	class archive final
	{
	private:
		using container_type =
			boost::container::map<directory::key, directory>;

	public:
		using key_type = container_type::key_type;
		using mapped_type = container_type::mapped_type;
		using value_type = container_type::value_type;
		using key_compare = container_type::key_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<mapped_type, true>;
		using const_index = detail::index_t<const mapped_type, true>;

		archive() noexcept = default;
		archive(const archive&) noexcept = default;
		archive(archive&&) noexcept = default;
		~archive() noexcept = default;
		archive& operator=(const archive&) noexcept = default;
		archive& operator=(archive&&) noexcept = default;

		[[nodiscard]] auto operator[](const key_type& a_key) noexcept
			-> index
		{
			const auto it = _directories.find(a_key);
			return it != _directories.end() ? index{ it->second } : index{};
		}

		[[nodiscard]] auto operator[](const key_type& a_key) const noexcept
			-> const_index
		{
			const auto it = _directories.find(a_key);
			return it != _directories.end() ? const_index{ it->second } : const_index{};
		}

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

		[[nodiscard]] auto begin() noexcept -> iterator { return _directories.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _directories.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _directories.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _directories.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _directories.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _directories.cend(); }

		void clear() noexcept
		{
			_directories.clear();
			_flags = archive_flag::none;
			_types = archive_type::none;
		}

		[[nodiscard]] bool empty() const noexcept { return _directories.empty(); }

		bool erase(const key_type& a_key) noexcept;

		[[nodiscard]] auto find(const key_type& a_key) noexcept
			-> iterator { return _directories.find(a_key); }

		[[nodiscard]] auto find(const key_type& a_key) const noexcept
			-> const_iterator { return _directories.find(a_key); }

		auto insert(key_type a_key, mapped_type a_directory) noexcept
			-> std::pair<iterator, bool>
		{
			return _directories.emplace(std::move(a_key), std::move(a_directory));
		}

		auto read(std::filesystem::path a_path) noexcept -> std::optional<version>;

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _directories.size(); }

		[[nodiscard]] bool verify_offsets(version a_version) const noexcept;

		bool write(std::filesystem::path a_path, version a_version) const noexcept;

	private:
		[[nodiscard]] auto make_header(version a_version) const noexcept -> detail::header_t;

		void read_file_names(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept;

		void read_directory(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept;

		void write_directory_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		[[nodiscard]] auto test_flag(archive_flag a_flag) const noexcept
			-> bool { return (_flags & a_flag) != archive_flag::none; }

		[[nodiscard]] auto test_type(archive_type a_type) const noexcept
			-> bool { return (_types & a_type) != archive_type::none; }

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_names(detail::ostream_t& a_out) const noexcept;

		container_type _directories;
		archive_flag _flags{ archive_flag::none };
		archive_type _types{ archive_type::none };
	};
}
