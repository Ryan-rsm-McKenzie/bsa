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

#include <boost/container/flat_set.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

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

		protected:
			friend class tes4::archive;
			friend class tes4::directory;

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
				return (**this)[a_hash];
			}

			template <concepts::stringable String>
			[[nodiscard]] auto operator[](String&& a_path) const noexcept  //
				requires(RECURSE)
			{
				return (**this)[std::forward<String>(a_path)];
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

		template <class T>
		struct key_compare_t
		{
			using is_transparent = int;

			[[nodiscard]] auto operator()(
				const T& a_lhs,
				const T& a_rhs) const noexcept
				-> bool { return a_lhs.hash() < a_rhs.hash(); }

			[[nodiscard]] auto operator()(
				const T& a_lhs,
				const hashing::hash& a_rhs) const noexcept
				-> bool { return a_lhs.hash() < a_rhs; }

			[[nodiscard]] auto operator()(
				const hashing::hash& a_lhs,
				const T& a_rhs) const noexcept
				-> bool { return a_lhs < a_rhs.hash(); }
		};
	}

	class file final
	{
	public:
		explicit file(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		template <detail::concepts::stringable String>
		explicit file(String&& a_path) noexcept
		{
			_name.emplace<name_owner>(std::forward<String>(a_path));
			_hash = hashing::hash_file(*std::get_if<name_owner>(&_name));
		}

		file(const file&) noexcept = default;
		file(file&& a_rhs) noexcept :
			_hash(a_rhs._hash),
			_name(a_rhs._name),
			_data(std::move(a_rhs._data)),
			_decompsz(a_rhs._decompsz)
		{
			a_rhs.clear();
		}

		~file() noexcept = default;

		file& operator=(const file&) noexcept = default;
		file& operator=(file&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_hash = a_rhs._hash;
				_name = a_rhs._name;
				_data = std::move(a_rhs._data);
				_decompsz = a_rhs._decompsz;

				a_rhs.clear();
			}
			return *this;
		}

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;

		void clear() noexcept;

		bool compress(version a_version) noexcept;

		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }

		[[nodiscard]] auto data() const noexcept -> const std::byte*;

		bool decompress(version a_version) noexcept;

		[[nodiscard]] auto decompressed_size() const noexcept
			-> std::size_t { return _decompsz ? *_decompsz : size(); }

		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

		[[nodiscard]] auto filename() const noexcept -> std::string_view;

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
			-> std::optional<std::string_view>;

		void read_filename(detail::istream_t& a_in) noexcept;

		void write_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::string_view a_dirname) const noexcept;

		void write_filename(detail::ostream_t& a_out) const noexcept;

	private:
		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied,

			name_count
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		struct name_proxy final
		{
			std::string_view n;
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
			std::string,
			name_proxy>
			_name;
		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;
		std::optional<std::size_t> _decompsz;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class directory final
	{
	public:
		using key_type = file;
		using key_compare = detail::key_compare_t<key_type>;

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

		template <detail::concepts::stringable String>
		explicit directory(String&& a_path) noexcept
		{
			_name.emplace<name_owner>(std::forward<String>(a_path));
			_hash = hashing::hash_directory(*std::get_if<name_owner>(&_name));
		}

		directory(const directory&) noexcept = default;
		directory(directory&& a_rhs) noexcept :
			_hash(a_rhs._hash),
			_name(a_rhs._name),
			_files(std::move(a_rhs._files))
		{
			a_rhs.clear();
		}

		~directory() noexcept = default;

		directory& operator=(const directory&) noexcept = default;
		directory& operator=(directory&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_hash = a_rhs._hash;
				_name = a_rhs._name;
				_files = std::move(a_rhs._files);

				a_rhs.clear();
			}
			return *this;
		}

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

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) noexcept
			-> index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_file(path)];
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) const noexcept
			-> const_index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_file(path)];
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		void clear() noexcept { _files.clear(); }

		[[nodiscard]] bool empty() const noexcept { return _files.empty(); }

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept
			-> iterator { return _files.find(a_hash); }

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator { return _files.find(a_hash); }

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) noexcept
			-> iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_file(path));
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) const noexcept
			-> const_iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_file(path));
		}

		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }

		auto insert(file a_file) noexcept
			-> std::pair<iterator, bool> { return _files.insert(std::move(a_file)); }

		[[nodiscard]] auto name() const noexcept -> std::string_view;

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
			name_owner,
			name_proxied,

			name_count
		};

		struct name_proxy final
		{
			std::string_view n;
			boost::iostreams::mapped_file_source f;
		};

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::string,
			name_proxy>
			_name;
		container_type _files;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
	};

	class archive final
	{
	public:
		using key_type = directory;
		using key_compare = detail::key_compare_t<key_type>;

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
		archive(const archive&) noexcept = default;
		archive(archive&&) noexcept = default;
		~archive() noexcept = default;
		archive& operator=(const archive&) noexcept = default;
		archive& operator=(archive&&) noexcept = default;

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

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) noexcept
			-> index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_directory(path)];
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) const noexcept
			-> const_index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_directory(path)];
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

		bool erase(hashing::hash a_hash) noexcept;

		template <detail::concepts::stringable String>
		bool erase(String&& a_path) noexcept
		{
			std::string path(std::forward<String>(a_path));
			return erase(hashing::hash_directory(path));
		}

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept
			-> iterator { return _directories.find(a_hash); }

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator { return _directories.find(a_hash); }

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) noexcept
			-> iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_directory(path));
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) const noexcept
			-> const_iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_directory(path));
		}

		auto insert(directory a_directory) noexcept
			-> std::pair<iterator, bool> { return _directories.insert(std::move(a_directory)); }

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
