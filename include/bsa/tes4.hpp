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

#pragma warning(push)
#include <boost/container/flat_set.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#pragma warning(pop)

#include "bsa/detail/common.hpp"
#include "bsa/fwd.hpp"

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

	BSA_MAKE_ALL_ENUM_OPERATORS(archive_flag);

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

	BSA_MAKE_ALL_ENUM_OPERATORS(archive_type);

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

		namespace constants
		{
			inline constexpr std::size_t directory_entry_size_x86 = 0x10;
			inline constexpr std::size_t directory_entry_size_x64 = 0x18;
			inline constexpr std::size_t file_entry_size = 0x10;
			inline constexpr std::size_t header_size = 0x24;
		}

		class header_t final
		{
		public:
			struct info_t final
			{
				std::uint32_t count{ 0 };
				std::uint32_t blobsz{ 0 };
			};

			header_t() noexcept = default;

			header_t(
				version a_version,
				archive_flag a_flags,
				archive_type a_types,
				info_t a_directories,
				info_t a_files) noexcept :
				_version(to_underlying(a_version)),
				_archiveFlags(to_underlying(a_flags)),
				_directory(a_directories),
				_file(a_files),
				_archiveTypes(to_underlying(a_types))
			{
				evaluate_endian();
			}

			friend istream_t& operator>>(istream_t& a_in, header_t& a_value) noexcept;
			friend ostream_t& operator<<(ostream_t& a_out, const header_t& a_value) noexcept;

			[[nodiscard]] bool good() const noexcept { return _good; }

			[[nodiscard]] auto directories_offset() const noexcept -> std::size_t { return _directoriesOffset; }
			[[nodiscard]] auto endian() const noexcept -> std::endian { return _endian; }
			[[nodiscard]] auto version() const noexcept -> std::size_t { return _version; }

			[[nodiscard]] auto directory_count() const noexcept -> std::size_t { return _directory.count; }
			[[nodiscard]] auto directory_names_length() const noexcept -> std::size_t { return _directory.blobsz; }

			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _file.count; }
			[[nodiscard]] auto file_names_length() const noexcept -> std::size_t { return _file.blobsz; }

			[[nodiscard]] auto archive_flags() const noexcept -> archive_flag { return archive_flag{ _archiveFlags }; }
			[[nodiscard]] auto archive_types() const noexcept -> archive_type { return archive_type{ _archiveTypes }; }

			[[nodiscard]] bool compressed() const noexcept { return test_flag(archive_flag::compressed); }
			[[nodiscard]] bool directory_strings() const noexcept { return test_flag(archive_flag::directory_strings); }
			[[nodiscard]] bool embedded_file_names() const noexcept { return _version > 103 && test_flag(archive_flag::embedded_file_names); }
			[[nodiscard]] bool file_strings() const noexcept { return test_flag(archive_flag::file_strings); }
			[[nodiscard]] bool xbox_archive() const noexcept { return test_flag(archive_flag::xbox_archive); }
			[[nodiscard]] bool xbox_compressed() const noexcept { return test_flag(archive_flag::xbox_compressed); }

		private:
			[[nodiscard]] bool test_flag(archive_flag a_flag) const noexcept
			{
				return (_archiveFlags & to_underlying(a_flag)) != 0;
			}

			void evaluate_endian() noexcept;

			std::uint32_t _version{ 0 };
			std::uint32_t _directoriesOffset{ constants::header_size };
			std::uint32_t _archiveFlags{ 0 };
			info_t _directory;
			info_t _file;
			std::endian _endian{ std::endian::little };
			std::uint16_t _archiveTypes{ 0 };
			bool _good{ true };
		};
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
				-> std::strong_ordering
			{
				return a_lhs.numeric() <=> a_rhs.numeric();
			}

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

		protected:
			friend class archive;
			friend class directory;

			void read(
				detail::istream_t& a_in,
				std::endian a_endian) noexcept;

			void write(
				detail::ostream_t& a_out,
				std::endian a_endian) const noexcept;
		};

		[[nodiscard]] hash hash_directory(std::filesystem::path& a_path) noexcept;
		[[nodiscard]] hash hash_file(std::filesystem::path& a_path) noexcept;
	}

	namespace detail
	{
		template <class T, bool RECURSE>
		class index_t final
		{
		public:
			using value_type = T;
			using pointer = T*;
			using reference = T&;

			index_t() noexcept = default;

			[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept  //
				requires(RECURSE)
			{
				assert(_proxy != nullptr);
				return (*_proxy)[a_hash];
			}

			[[nodiscard]] auto operator[](std::filesystem::path a_path) const noexcept  //
				requires(RECURSE)
			{
				return (**this)[std::move(a_path)];
			}

			[[nodiscard]] explicit operator bool() const noexcept { return _proxy != nullptr; }

			[[nodiscard]] auto operator*() const noexcept -> reference
			{
				assert(*this);
				return *_proxy;
			}

			[[nodiscard]] auto operator->() const noexcept -> pointer { return _proxy; }

		protected:
			friend class tes4::archive;
			friend class tes4::directory;

			explicit index_t(reference a_value) noexcept :
				_proxy(&a_value)
			{}

		private:
			pointer _proxy{ nullptr };
		};
	}

	class file final
	{
	public:
		explicit file(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		explicit file(std::filesystem::path a_path) noexcept
		{
			_hash = hashing::hash_file(a_path);
			_name.emplace<name_owner>(a_path.u8string());
		}

		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;

		bool compress(version a_version) noexcept;

		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }

		[[nodiscard]] auto data() const noexcept -> const std::byte*;

		bool decompress(version a_version) noexcept;

		[[nodiscard]] auto decompressed_size() const noexcept
			-> std::size_t
		{
			return _decompsz ? *_decompsz : size();
		}

		[[nodiscard]] auto filename() const noexcept -> std::u8string_view;

		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }

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

		[[nodiscard]] auto size() const noexcept -> std::size_t;

	protected:
		friend class directory;

		enum : std::uint32_t
		{
			icompression = 1u << 30u,
			ichecked = 1u << 31u,

			isecondary_archive = 1u << 31u
		};

		[[nodiscard]] auto read_data(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_size,
			std::size_t a_offset) noexcept
			-> std::optional<std::u8string_view>;

		void read_filename(detail::istream_t& a_in) noexcept;

		void write_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::u8string_view a_dirname) const noexcept;

		void write_filename(detail::ostream_t& a_out) const noexcept;

	private:
		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied
		};

		struct name_proxy final
		{
			std::u8string_view n;
			boost::iostreams::mapped_file_source f;
		};

		struct data_proxy final
		{
			std::span<const std::byte> d;
			boost::iostreams::mapped_file_source f;
		};

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::u8string,
			name_proxy>
			_name;
		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;
		std::optional<std::size_t> _decompsz;
	};

	class directory final
	{
	public:
		using key_type = file;

		struct key_compare
		{
			using is_transparent = int;

			[[nodiscard]] bool operator()(
				const key_type& a_lhs,
				const key_type& a_rhs) const noexcept
			{
				return a_lhs.hash() < a_rhs.hash();
			}

			[[nodiscard]] bool operator()(
				const key_type& a_lhs,
				const hashing::hash& a_rhs) const noexcept
			{
				return a_lhs.hash() < a_rhs;
			}

			[[nodiscard]] bool operator()(
				const hashing::hash& a_lhs,
				const key_type& a_rhs) const noexcept
			{
				return a_lhs < a_rhs.hash();
			}
		};

	private:
		using container_type =
			boost::container::flat_set<key_type, key_compare>;

	public:
		using value_type = container_type::value_type;
		using value_compare = container_type::value_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<value_type, false>;
		using const_index = detail::index_t<const value_type, false>;

		explicit directory(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		explicit directory(std::filesystem::path a_path) noexcept
		{
			_hash = hashing::hash_directory(a_path);
			_name.emplace<name_owner>(a_path.u8string());
		}

		directory(const directory&) noexcept = default;
		directory(directory&&) noexcept = default;
		~directory() noexcept = default;
		directory& operator=(const directory&) noexcept = default;
		directory& operator=(directory&&) noexcept = default;

		[[nodiscard]] auto operator[](hashing::hash a_hash) noexcept
			-> index
		{
			const auto it = _files.find(a_hash);
			return it != _files.end() ? index{ *it } : index{};
		}

		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
			-> const_index
		{
			const auto it = _files.find(a_hash);
			return it != _files.end() ? const_index{ *it } : const_index{};
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_path) noexcept
			-> index
		{
			return (*this)[hashing::hash_file(a_path)];
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_path) const noexcept
			-> const_index
		{
			return (*this)[hashing::hash_file(a_path)];
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept
			-> iterator
		{
			return _files.find(a_hash);
		}

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator
		{
			return _files.find(a_hash);
		}

		[[nodiscard]] auto find(std::filesystem::path a_path) noexcept
			-> iterator
		{
			return find(hashing::hash_file(a_path));
		}

		[[nodiscard]] auto find(std::filesystem::path a_path) const noexcept
			-> const_iterator
		{
			return find(hashing::hash_file(a_path));
		}

		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }

		auto insert(file a_file) noexcept
			-> std::pair<iterator, bool>
		{
			return _files.insert(std::move(a_file));
		}

		[[nodiscard]] auto name() const noexcept -> std::u8string_view;

		void reserve(std::size_t a_count) noexcept { _files.reserve(a_count); }

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _files.size(); }

	protected:
		friend class archive;

		void read_file_names(detail::istream_t& a_in) noexcept;

		void read_files(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_count) noexcept;

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::uint32_t& a_dataOffset) const noexcept;

		void write_file_names(detail::ostream_t& a_out) const noexcept;

	private:
		enum : std::size_t
		{
			name_null,
			name_view,
			name_owner,
			name_proxied
		};

		struct name_proxy final
		{
			std::u8string_view n;
			boost::iostreams::mapped_file_source f;
		};

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::u8string_view,
			std::u8string,
			name_proxy>
			_name;
		container_type _files;
	};

	class archive final
	{
	public:
		using key_type = directory;

		struct key_compare
		{
			using is_transparent = int;

			[[nodiscard]] bool operator()(
				const key_type& a_lhs,
				const key_type& a_rhs) const noexcept
			{
				return a_lhs.hash() < a_rhs.hash();
			}

			[[nodiscard]] bool operator()(
				const key_type& a_lhs,
				const hashing::hash& a_rhs) const noexcept
			{
				return a_lhs.hash() < a_rhs;
			}

			[[nodiscard]] bool operator()(
				const hashing::hash& a_lhs,
				const key_type& a_rhs) const noexcept
			{
				return a_lhs < a_rhs.hash();
			}
		};

	private:
		using container_type =
			boost::container::flat_set<key_type, key_compare>;

	public:
		using value_type = container_type::value_type;
		using value_compare = container_type::value_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<value_type, true>;
		using const_index = detail::index_t<const value_type, true>;

		archive() noexcept = default;

		[[nodiscard]] auto operator[](hashing::hash a_hash) noexcept
			-> index
		{
			const auto it = _directories.find(a_hash);
			return it != _directories.end() ? index{ *it } : index{};
		}

		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
			-> const_index
		{
			const auto it = _directories.find(a_hash);
			return it != _directories.end() ? const_index{ *it } : const_index{};
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_path) noexcept
			-> index
		{
			return (*this)[hashing::hash_directory(a_path)];
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_path) const noexcept
			-> const_index
		{
			return (*this)[hashing::hash_directory(a_path)];
		}

		[[nodiscard]] auto archive_flags() const noexcept -> archive_flag { return _flags; }
		void archive_flags(archive_flag a_flags) noexcept { _flags = a_flags; }

		[[nodiscard]] auto archive_types() const noexcept -> archive_type { return _types; }
		void archive_types(archive_type a_types) noexcept { _types = a_types; }

		[[nodiscard]] bool compressed() const noexcept { return test_flag(archive_flag::compressed); }
		[[nodiscard]] bool directory_strings() const noexcept { return test_flag(archive_flag::directory_strings); }
		[[nodiscard]] bool embedded_file_names() const noexcept { return test_flag(archive_flag::embedded_file_names); }
		[[nodiscard]] bool file_strings() const noexcept { return test_flag(archive_flag::file_strings); }
		[[nodiscard]] bool retain_directory_names() const noexcept { return test_flag(archive_flag::retain_directory_names); }
		[[nodiscard]] bool retain_file_name_offsets() const noexcept { return test_flag(archive_flag::retain_file_name_offsets); }
		[[nodiscard]] bool retain_file_names() const noexcept { return test_flag(archive_flag::retain_file_names); }
		[[nodiscard]] bool retain_strings_during_startup() const noexcept { return test_flag(archive_flag::retain_strings_during_startup); }
		[[nodiscard]] bool xbox_archive() const noexcept { return test_flag(archive_flag::xbox_archive); }
		[[nodiscard]] bool xbox_compressed() const noexcept { return test_flag(archive_flag::xbox_compressed); }

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

		bool erase(hashing::hash a_hash) noexcept;

		bool erase(std::filesystem::path a_path) noexcept
		{
			return erase(hashing::hash_directory(a_path));
		}

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept
			-> iterator
		{
			return _directories.find(a_hash);
		}

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator
		{
			return _directories.find(a_hash);
		}

		[[nodiscard]] auto find(std::filesystem::path a_path) noexcept
			-> iterator
		{
			return find(hashing::hash_directory(a_path));
		}

		[[nodiscard]] auto find(std::filesystem::path a_path) const noexcept
			-> const_iterator
		{
			return find(hashing::hash_directory(a_path));
		}

		auto insert(directory a_directory) noexcept
			-> std::pair<iterator, bool>
		{
			return _directories.insert(std::move(a_directory));
		}

		auto read(std::filesystem::path a_path) noexcept -> std::optional<version>;

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _directories.size(); }

		bool write(std::filesystem::path a_path, version a_version) const noexcept;

	private:
		void read_file_names(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept;

		void read_directory(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept;

		void write_directory_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		[[nodiscard]] bool test_flag(archive_flag a_flag) const noexcept
		{
			return (_flags & a_flag) != archive_flag::none;
		}

		[[nodiscard]] bool test_type(archive_type a_type) const noexcept
		{
			return (_types & a_type) != archive_type::none;
		}

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
