#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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
#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/predef.h>
#pragma warning(pop)

#define BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, a_op)                                     \
	[[nodiscard]] constexpr a_type operator a_op(a_type a_lhs, a_type a_rhs) noexcept \
	{                                                                                 \
		return static_cast<a_type>(                                                   \
			static_cast<std::underlying_type_t<a_type>>(a_lhs)                        \
				a_op static_cast<std::underlying_type_t<a_type>>(a_rhs));             \
	}                                                                                 \
                                                                                      \
	constexpr a_type& operator a_op##=(a_type& a_lhs, a_type a_rhs) noexcept          \
	{                                                                                 \
		return a_lhs = a_lhs a_op a_rhs;                                              \
	}

#define BSA_MAKE_ALL_ENUM_OPERATORS(a_type)                                  \
	static_assert(std::is_enum_v<a_type>, "\'" #a_type "\' is not an enum"); \
                                                                             \
	[[nodiscard]] constexpr a_type operator~(a_type a_val) noexcept          \
	{                                                                        \
		return static_cast<a_type>(                                          \
			~static_cast<std::underlying_type_t<a_type>>(a_val));            \
	}                                                                        \
                                                                             \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, ^);                                  \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, &);                                  \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, |);                                  \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, <<);                                 \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, >>);

namespace bsa::tes4
{
	namespace detail
	{
		class header_t;
		class istream_t;
		class ostream_t;
		class restore_point;
	}

	namespace hashing
	{
		struct hash;
	}

	class archive;
	class directory;
	class file;
	class hash;

	enum class archive_flag : std::uint32_t;
	enum class archive_type : std::uint16_t;
	enum class version : std::uint32_t;

	using namespace std::literals;

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
		namespace concepts
		{
			template <class T>
			concept integral =
				std::same_as<T, std::byte> ||
				std::same_as<T, std::int8_t> ||
				std::same_as<T, std::uint8_t> ||
				std::same_as<T, std::int16_t> ||
				std::same_as<T, std::uint16_t> ||
				std::same_as<T, std::int32_t> ||
				std::same_as<T, std::uint32_t> ||
				std::same_as<T, std::int64_t> ||
				std::same_as<T, std::uint64_t>;
		}

		namespace constants
		{
			inline constexpr std::size_t directory_entry_size_x86 = 0x10;
			inline constexpr std::size_t directory_entry_size_x64 = 0x18;
			inline constexpr std::size_t file_entry_size = 0x10;
			inline constexpr std::size_t header_size = 0x24;
		}

		[[nodiscard]] inline auto crc32(std::span<const std::byte> a_bytes)
			-> std::uint32_t
		{
			constexpr auto constant = std::uint32_t{ 0x1003Fu };
			std::uint32_t crc = 0;
			for (const auto c : a_bytes) {
				crc = static_cast<std::uint8_t>(c) + crc * constant;
			}
			return crc;
		}

		[[noreturn]] inline void declare_unreachable()
		{
			assert(false);
#if BOOST_COMP_GNUC
			__builtin_unreachable();
#elif BOOST_COMP_CLANG
			__builtin_unreachable();
#elif BOOST_COMP_MSVC || BOOST_COMP_EDG
			__assume(false);
#else
			static_assert(false);
#endif
		}
		template <class Enum>
		[[nodiscard]] constexpr auto to_underlying(Enum a_val) noexcept
			-> std::underlying_type_t<Enum>
		{
			static_assert(std::is_enum_v<Enum>, "Input type is not an enumeration");
			return static_cast<std::underlying_type_t<Enum>>(a_val);
		}

		class istream_t final
		{
		public:
			using stream_type = boost::iostreams::mapped_file_source;

			istream_t(std::filesystem::path a_path) noexcept :
				_file(boost::filesystem::path{ a_path.native() })
			{
				assert(_file.is_open());
			}

			istream_t(const istream_t&) = delete;

			[[nodiscard]] bool is_open() const noexcept { return _file.is_open(); }

			template <concepts::integral T>
			[[nodiscard]] T read(std::endian a_endian = std::endian::little) noexcept
			{
				using integral_t = typename std::conditional_t<
					std::is_enum_v<T>,
					std::underlying_type<T>,
					std::type_identity<T>>::type;
				auto result = integral_t{ 0 };
				const auto bytes = read_bytes(sizeof(T));

				switch (a_endian) {
				case std::endian::little:
					for (std::size_t i = 0; i < bytes.size(); ++i) {
						result |= static_cast<T>(bytes[i]) << i * 8u;
					}
					break;
				case std::endian::big:
					for (std::ptrdiff_t i = std::ssize(bytes) - 1; i >= 0; --i) {
						result |=
							static_cast<integral_t>(bytes[static_cast<std::size_t>(i)])
							<< static_cast<std::size_t>(i) * 8u;
					}
					break;
				default:
					declare_unreachable();
				}

				return static_cast<T>(result);
			}

			[[nodiscard]] auto read_bytes(std::size_t a_bytes) noexcept
				-> std::span<const std::byte>
			{
				const auto pos = _pos;
				_pos += a_bytes;
				return {
					reinterpret_cast<const std::byte*>(_file.data()) + pos,
					a_bytes
				};
			}

			void seek_absolute(std::size_t a_pos) noexcept { _pos = a_pos; }
			void seek_relative(std::ptrdiff_t a_off) noexcept { _pos += a_off; }

			[[nodiscard]] auto tell() const noexcept { return _pos; }
			[[nodiscard]] auto rdbuf() const noexcept -> stream_type { return _file; }

			template <concepts::integral T>
			friend istream_t& operator>>(istream_t& a_in, T& a_value) noexcept
			{
				a_value = a_in.read<T>();
				return a_in;
			}

			template <std::size_t N>
			friend istream_t& operator>>(
				istream_t& a_in,
				std::array<std::byte, N>& a_value) noexcept
			{
				const auto bytes = a_in.read_bytes(a_value.size());
				std::copy(bytes.begin(), bytes.end(), a_value.begin());
				return a_in;
			}

		private:
			stream_type _file;
			std::size_t _pos{ 0 };
		};

		class ostream_t final
		{
		public:
			ostream_t(std::filesystem::path a_path) noexcept :
				_file(a_path, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary)
			{
				assert(_file.is_open());
			}

			ostream_t(const ostream_t&) = delete;

			[[nodiscard]] bool is_open() const noexcept { return _file.is_open(); }

			template <concepts::integral T>
			void write(T a_value, std::endian a_endian = std::endian::little) noexcept
			{
				using integral_t = typename std::conditional_t<
					std::is_enum_v<T>,
					std::underlying_type<T>,
					std::type_identity<T>>::type;
				const auto value = static_cast<integral_t>(a_value);
				std::array<std::byte, sizeof(T)> bytes;

				switch (a_endian) {
				case std::endian::little:
					for (std::size_t i = 0; i < bytes.size(); ++i) {
						bytes[i] = static_cast<std::byte>((value >> i * 8u) & 0xFFu);
					}
					break;
				case std::endian::big:
					for (std::ptrdiff_t i = std::ssize(bytes) - 1; i >= 0; --i) {
						bytes[static_cast<std::size_t>(i)] =
							static_cast<std::byte>(
								(value >> static_cast<std::size_t>(i) * 8u) &
								0xFFu);
					}
					break;
				default:
					declare_unreachable();
				}

				write_bytes(std::span<const std::byte>{ bytes.data(), bytes.size() });
			}

			void write_bytes(std::span<const std::byte> a_bytes) noexcept
			{
				_file.write(
					reinterpret_cast<const char*>(a_bytes.data()),
					static_cast<std::streamsize>(a_bytes.size()));
			}

			template <concepts::integral T>
			friend ostream_t& operator<<(ostream_t& a_out, T a_value) noexcept
			{
				a_out.write(a_value);
				return a_out;
			}

			template <std::size_t N>
			friend ostream_t& operator<<(
				ostream_t& a_out,
				const std::array<std::byte, N>& a_value) noexcept
			{
				a_out.write_bytes({ a_value.data(), a_value.size() });
				return a_out;
			}

		private:
			std::ofstream _file;
		};

		class restore_point final
		{
		public:
			restore_point(const restore_point&) = delete;

			explicit restore_point(istream_t& a_proxy) noexcept :
				_proxy(a_proxy),
				_pos(a_proxy.tell())
			{}

			~restore_point() noexcept { _proxy.seek_absolute(_pos); }

		private:
			istream_t& _proxy;
			std::size_t _pos;
		};

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

			friend istream_t& operator>>(istream_t& a_in, header_t& a_value) noexcept
			{
				std::array<std::byte, 4> magic;

				a_in >>
					magic >>
					a_value._version >>
					a_value._directoriesOffset >>
					a_value._archiveFlags >>
					a_value._directory.count >>
					a_value._file.count >>
					a_value._directory.blobsz >>
					a_value._file.blobsz >>
					a_value._archiveTypes;
				a_in.seek_relative(2);

				a_value.evaluate_endian();

				assert(magic[0] == std::byte{ u8'B' } &&
					   magic[1] == std::byte{ u8'S' } &&
					   magic[2] == std::byte{ u8'A' } &&
					   magic[3] == std::byte{ u8'\0' });
				assert(a_value._version == 103 ||
					   a_value._version == 104 ||
					   a_value._version == 105);
				assert(a_value._directoriesOffset == constants::header_size);

				return a_in;
			}

			friend ostream_t& operator<<(ostream_t& a_out, const header_t& a_value) noexcept
			{
				std::array magic{
					std::byte{ u8'B' },
					std::byte{ u8'S' },
					std::byte{ u8'A' },
					std::byte{ u8'\0' }
				};

				a_out
					<< magic
					<< a_value._version
					<< a_value._directoriesOffset
					<< a_value._archiveFlags
					<< a_value._directory.count
					<< a_value._file.count
					<< a_value._directory.blobsz
					<< a_value._file.blobsz
					<< a_value._archiveTypes
					<< std::uint16_t{ 0 };

				return a_out;
			}

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
			[[nodiscard]] bool embedded_file_names() const noexcept { return test_flag(archive_flag::embedded_file_names); }
			[[nodiscard]] bool file_strings() const noexcept { return test_flag(archive_flag::file_strings); }
			[[nodiscard]] bool retain_directory_names() const noexcept { return test_flag(archive_flag::retain_directory_names); }
			[[nodiscard]] bool retain_file_name_offsets() const noexcept { return test_flag(archive_flag::retain_file_name_offsets); }
			[[nodiscard]] bool retain_file_names() const noexcept { return test_flag(archive_flag::retain_file_names); }
			[[nodiscard]] bool retain_strings_during_startup() const noexcept { return test_flag(archive_flag::retain_strings_during_startup); }
			[[nodiscard]] bool xbox_archive() const noexcept { return test_flag(archive_flag::xbox_archive); }
			[[nodiscard]] bool xbox_compressed() const noexcept { return test_flag(archive_flag::xbox_compressed); }

		private:
			[[nodiscard]] bool test_flag(archive_flag a_flag) const noexcept
			{
				return (_archiveFlags & to_underlying(a_flag)) != 0;
			}

			void evaluate_endian() noexcept
			{
				_endian =
					xbox_archive() ?
						std::endian::big :
                        std::endian::little;
			}

			std::uint32_t _version{ 0 };
			std::uint32_t _directoriesOffset{ constants::header_size };
			std::uint32_t _archiveFlags{ 0 };
			info_t _directory;
			info_t _file;
			std::endian _endian{ std::endian::little };
			std::uint16_t _archiveTypes{ 0 };
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
				const auto numeric = [](const hash& a_hash) noexcept {
					return std::uint64_t{
						std::uint64_t{ a_hash.last } << 0u * 8u |
						std::uint64_t{ a_hash.last2 } << 1u * 8u |
						std::uint64_t{ a_hash.length } << 2u * 8u |
						std::uint64_t{ a_hash.first } << 3u * 8u |
						std::uint64_t{ a_hash.crc } << 4u * 8u
					};
				};

				return numeric(a_lhs) <=> numeric(a_rhs);
			}

		protected:
			friend class archive;
			friend class directory;

			void read(
				detail::istream_t& a_in,
				std::endian a_endian) noexcept
			{
				a_in >>
					last >>
					last2 >>
					length >>
					first;
				crc = a_in.read<decltype(crc)>(a_endian);
			}

			void write(
				detail::ostream_t& a_out,
				std::endian a_endian) const noexcept
			{
				a_out << last
					  << last2
					  << length
					  << first;
				a_out.write(crc, a_endian);
			}
		};

		[[nodiscard]] hash hash_directory(std::filesystem::path a_path) noexcept;
		[[nodiscard]] hash hash_file(std::filesystem::path a_path) noexcept;
	}

	class file final
	{
	public:
		explicit file(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		explicit file(std::u8string a_name) noexcept :
			_hash(hashing::hash_file(a_name)),
			_name(std::in_place_index<name_owner>, std::move(a_name))
		{}

		explicit file(std::u8string_view a_name) noexcept :
			_hash(hashing::hash_file(a_name)),
			_name(std::in_place_index<name_view>, a_name)
		{}

		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept
			-> std::span<const std::byte>
		{
			switch (_data.index()) {
			case data_view:
				return std::get<data_view>(_data);
			case data_owner:
				{
					auto& owner = std::get<data_owner>(_data);
					return {
						owner.data(),
						owner.size()
					};
				}
			case data_proxied:
				return std::get<data_proxied>(_data).d;
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] auto data() const noexcept
			-> const std::byte*
		{
			switch (_data.index()) {
			case data_view:
				return std::get<data_view>(_data).data();
			case data_owner:
				return std::get<data_owner>(_data).data();
			case data_proxied:
				return std::get<data_proxied>(_data).d.data();
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }

		[[nodiscard]] auto filename() const noexcept
			-> std::u8string_view
		{
			switch (_name.index()) {
			case name_null:
				return {};
			case name_view:
				return std::get<name_view>(_name);
			case name_owner:
				return std::get<name_owner>(_name);
			case name_proxied:
				return std::get<name_proxied>(_name).n;
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }

		[[nodiscard]] auto size() const noexcept
			-> std::size_t
		{
			switch (_data.index()) {
			case data_view:
				return std::get<data_view>(_data).size();
			case data_owner:
				return std::get<data_owner>(_data).size();
			case data_proxied:
				return std::get<data_proxied>(_data).d.size();
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] auto uncompressed_size() const noexcept
			-> std::size_t
		{
			return _decompsz ? *_decompsz : size();
		}

	protected:
		friend class directory;

		enum : std::uint32_t
		{
			icompression = 1u << 30u,
			ichecked = 1u << 31u
		};

		[[nodiscard]] auto read_data(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_size,
			std::size_t a_offset) noexcept
			-> std::optional<std::u8string_view>
		{
			std::optional<std::u8string_view> dirname;

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(a_offset);

			if (a_header.embedded_file_names()) {  // bstring
				std::uint8_t len = 0;
				a_in >> len;
				const auto bytes = a_in.read_bytes(len);

				if (_name.index() == name_null) {
					std::u8string_view name{
						reinterpret_cast<const char8_t*>(bytes.data()),
						len
					};
					const auto pos = name.find_last_of(u8'\\');
					if (pos != std::u8string_view::npos) {
						dirname = name.substr(0, pos);
						name = name.substr(pos + 1);
					}
					_name.emplace<name_proxied>(name, a_in.rdbuf());
				}

				a_size -= static_cast<std::size_t>(len) + 1;
			}

			const bool compressed =
				a_size & icompression ?
					!a_header.compressed() :
                    a_header.compressed();
			if (compressed) {
				std::uint32_t size = 0;
				a_in >> size;
				_decompsz = size;
				a_size -= sizeof(size);
			}

			_data.emplace<data_proxied>(a_in.read_bytes(a_size), a_in.rdbuf());
			return dirname;
		}

		void read_filename(detail::istream_t& a_in) noexcept
		{
			// zstring
			const std::u8string_view name{
				reinterpret_cast<const char8_t*>(a_in.read_bytes(1).data())
			};
			a_in.seek_relative(name.length());
			_name.emplace<name_proxied>(name, a_in.rdbuf());
		}

		void write_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::u8string_view a_dirname) const noexcept
		{
			if (a_header.embedded_file_names()) {
				const auto writeStr = [&](std::u8string_view a_str) noexcept {
					a_out.write_bytes(
						{ reinterpret_cast<const std::byte*>(a_str.data()), a_str.length() });
				};

				const auto myname = filename();
				a_out << static_cast<std::uint8_t>(
					a_dirname.length() +
					1u +  // directory separator
					myname.length());
				writeStr(a_dirname);
				a_out << std::byte{ u8'\\' };
				writeStr(myname);
			}

			if (compressed()) {
				a_out << static_cast<std::uint32_t>(*_decompsz);
			}

			a_out.write_bytes(as_bytes());
		}

		void write_filename(detail::ostream_t& a_out) const noexcept
		{
			const auto name = filename();
			a_out.write_bytes(
				{ reinterpret_cast<const std::byte*>(name.data()), name.length() });
			a_out << std::byte{ u8'\0' };
		}

	private:
		enum : std::size_t
		{
			name_null,
			name_view,
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
			std::u8string_view,
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
		using iterator = container_type::const_iterator;
		using const_iterator = container_type::const_iterator;

		class index_t final
		{
		public:
			index_t() noexcept = default;

			[[nodiscard]] explicit operator bool() const noexcept { return _proxy != nullptr; }
			[[nodiscard]] auto operator*() const noexcept -> const file& { return *_proxy; }
			[[nodiscard]] auto operator->() const noexcept -> const file* { return _proxy; }

		protected:
			friend class archive;
			friend class directory;

			explicit index_t(const file& a_value) noexcept :
				_proxy(&a_value)
			{}

		private:
			const file* _proxy{ nullptr };
		};

		explicit directory(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		explicit directory(std::u8string a_filename) noexcept :
			_hash(hashing::hash_directory(a_filename)),
			_name(std::in_place_index<name_owner>, std::move(a_filename))
		{}

		explicit directory(std::u8string_view a_filename) noexcept :
			_hash(hashing::hash_directory(a_filename)),
			_name(std::in_place_index<name_view>, a_filename)
		{}

		directory(const directory&) noexcept = default;
		directory(directory&&) noexcept = default;
		~directory() noexcept = default;
		directory& operator=(const directory&) noexcept = default;
		directory& operator=(directory&&) noexcept = default;

		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
			-> index_t
		{
			const auto it = _files.find(a_hash);
			return it != _files.end() ? index_t{ *it } : index_t{};
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_filename) const noexcept
			-> index_t
		{
			return (*this)[hashing::hash_file(std::move(a_filename))];
		}

		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator
		{
			return _files.find(a_hash);
		}

		[[nodiscard]] auto find(std::filesystem::path a_filename) const noexcept
			-> const_iterator
		{
			return find(hashing::hash_file(std::move(a_filename)));
		}

		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }

		auto insert(file a_file) noexcept
			-> std::pair<const_iterator, bool>
		{
			return _files.insert(std::move(a_file));
		}

		[[nodiscard]] auto name() const noexcept
			-> std::u8string_view
		{
			switch (_name.index()) {
			case name_null:
				return {};
			case name_view:
				return std::get<name_view>(_name);
			case name_owner:
				return std::get<name_owner>(_name);
			case name_proxied:
				return std::get<name_proxied>(_name).n;
			default:
				detail::declare_unreachable();
			}
		}

		void reserve(std::size_t a_count) noexcept { _files.reserve(a_count); }

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _files.size(); }

	protected:
		friend class archive;

		void read_files(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_count) noexcept
		{
			if (a_header.directory_strings()) {  // bzstring
				std::uint8_t len = 0;
				a_in >> len;
				const std::u8string_view name{
					reinterpret_cast<const char8_t*>(a_in.read_bytes(len).data()),
					len - 1u  // skip null terminator
				};

				_name.emplace<name_proxied>(name, a_in.rdbuf());
			}

			_files.reserve(a_count);
			for (std::size_t i = 0; i < a_count; ++i) {
				hashing::hash h;
				h.read(a_in, a_header.endian());

				std::uint32_t size = 0;
				std::uint32_t offset = 0;
				a_in >> size >> offset;

				file f{ h };
				const auto dirname = f.read_data(a_in, a_header, size, offset);
				if (_name.index() == name_null && dirname) {
					_name.emplace<name_proxied>(*dirname, a_in.rdbuf());
				}

				[[maybe_unused]] const auto [it, success] = _files.insert(std::move(f));
				assert(success);
			}
		}

		void read_file_names(detail::istream_t& a_in) noexcept
		{
			for (auto& file : _files) {
				file.read_filename(a_in);
			}
		}

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept
		{
			const auto myname = name();
			for (const auto& file : _files) {
				file.write_data(a_out, a_header, myname);
			}
		}

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::uint32_t& a_dataOffset) const noexcept
		{
			if (a_header.directory_strings()) {  // bzstring
				const auto myname = name();
				a_out << static_cast<std::uint8_t>(myname.length() + 1u);  // include null terminator
				a_out.write_bytes(
					{ reinterpret_cast<const std::byte*>(myname.data()), myname.size() });
				a_out << std::byte{ u8'\0' };
			}

			for (const auto& file : _files) {
				file.hash().write(a_out, a_header.endian());
				const auto fsize = file.size();
				if (!!a_header.compressed() != !!file.compressed()) {
					a_out << (static_cast<std::uint32_t>(fsize) | file::icompression);
				} else {
					a_out << static_cast<std::uint32_t>(fsize);
				}
				a_out << a_dataOffset;
				a_dataOffset += static_cast<std::uint32_t>(file.filename().length() +
														   1u);  // prefixed byte length
				if (file.compressed()) {
					a_dataOffset += static_cast<std::uint32_t>(file.uncompressed_size());
				}
				a_dataOffset += static_cast<std::uint32_t>(fsize);
			}
		}

		void write_file_names(detail::ostream_t& a_out) const noexcept
		{
			for (const auto& file : _files) {
				file.write_filename(a_out);
			}
		}

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
		using iterator = container_type::const_iterator;
		using const_iterator = container_type::const_iterator;

		class index_t final
		{
		public:
			index_t() noexcept = default;

			[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
				-> directory::index_t
			{
				return _proxy ? (*_proxy)[a_hash] : directory::index_t{};
			}

			[[nodiscard]] auto operator[](std::filesystem::path a_path) const noexcept
				-> directory::index_t
			{
				return _proxy ? (*_proxy)[std::move(a_path)] : directory::index_t{};
			}

			[[nodiscard]] explicit operator bool() const noexcept { return _proxy != nullptr; }
			[[nodiscard]] auto operator*() const noexcept -> const directory& { return *_proxy; }
			[[nodiscard]] auto operator->() const noexcept -> const directory* { return _proxy; }

		protected:
			friend class archive;

			explicit index_t(const directory& a_value) noexcept :
				_proxy(&a_value)
			{}

		private:
			const directory* _proxy{ nullptr };
		};

		archive() noexcept = default;

		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
			-> index_t
		{
			const auto it = _directories.find(a_hash);
			return it != _directories.end() ? index_t{ *it } : index_t{};
		}

		[[nodiscard]] auto operator[](std::filesystem::path a_path) const noexcept
			-> index_t
		{
			return (*this)[hashing::hash_directory(std::move(a_path))];
		}

		[[nodiscard]] auto archive_flags() const noexcept -> archive_flag { return _flags; }
		void archive_flags(archive_flag a_flags) noexcept { _flags = a_flags; }

		[[nodiscard]] auto archive_types() const noexcept -> archive_type { return _types; }
		void archive_types(archive_type a_types) noexcept { _types = a_types; }

		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _directories.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _directories.cbegin(); }

		[[nodiscard]] auto end() const noexcept -> const_iterator { return _directories.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _directories.cend(); }

		void clear() noexcept
		{
			_directories.clear();
			_flags = archive_flag::none;
			_types = archive_type::none;
		}

		bool erase(hashing::hash a_hash) noexcept
		{
			const auto it = _directories.find(a_hash);
			if (it != _directories.end()) {
				_directories.erase(it);
				return true;
			} else {
				return false;
			}
		}

		bool erase(std::filesystem::path a_path) noexcept
		{
			return erase(hashing::hash_directory(std::move(a_path)));
		}

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator
		{
			return _directories.find(a_hash);
		}

		[[nodiscard]] auto find(std::filesystem::path a_path) const noexcept
			-> const_iterator
		{
			return find(hashing::hash_directory(std::move(a_path)));
		}

		auto insert(directory a_directory) noexcept
			-> std::pair<const_iterator, bool>
		{
			return _directories.insert(std::move(a_directory));
		}

		void read(std::filesystem::path a_path) noexcept
		{
			clear();
			detail::istream_t in{ std::move(a_path) };
			const auto header = [&]() noexcept {
				detail::header_t result;
				in >> result;
				return result;
			}();

			_flags = header.archive_flags();
			_types = header.archive_types();

			in.seek_absolute(header.directories_offset());
			_directories.reserve(header.directory_count());
			for (std::size_t i = 0; i < header.directory_count(); ++i) {
				read_directory(in, header);
			}

			if (header.file_strings() && !header.embedded_file_names()) {
				read_file_names(in, header);
			}
		}

		void write(std::filesystem::path a_path, version a_version) const noexcept
		{
			detail::ostream_t out{ std::move(a_path) };
			const auto header = [&]() noexcept -> detail::header_t {
				detail::header_t::info_t files;
				detail::header_t::info_t dirs;

				for (const auto& dir : _directories) {
					dirs.count += 1;
					dirs.blobsz += static_cast<std::uint32_t>(
						dir.name().length() +
						1u +  // prefixed byte length
						1u);  // null terminator

					for (const auto& file : dir) {
						files.count += 1;
						files.blobsz += static_cast<std::uint32_t>(
							file.filename().length() +
							1u);  // prefixed byte length
					}
				}

				return {
					a_version,
					_flags,
					_types,
					dirs,
					files
				};
			}();
			out << header;

			auto dataOffset = write_directory_entries(out, header);
			write_file_entries(out, header, dataOffset);
			if (header.file_strings()) {
				write_file_names(out);
			}
			write_file_data(out, header);
		}

	private:
		void read_directory(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept
		{
			hashing::hash hash;
			hash.read(a_in, a_header.endian());
			directory dir{ hash };

			std::uint32_t count = 0;
			a_in >> count;

			std::uint32_t offset = 0;
			switch (a_header.version()) {
			case 103:
			case 104:
				a_in >> offset;
				break;
			case 105:
				a_in.seek_relative(4);
				a_in >> offset;
				a_in.seek_relative(4);
				break;
			default:
				detail::declare_unreachable();
			}

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(offset - a_header.file_names_length());
			dir.read_files(a_in, a_header, count);

			[[maybe_unused]] const auto [it, success] = _directories.insert(std::move(dir));
			assert(success);
		}

		void read_file_names(
			detail::istream_t& a_in,
			const detail::header_t& a_header) noexcept
		{
			const auto dirsz = [&]() noexcept {
				switch (a_header.version()) {
				case 103:
				case 104:
					return detail::constants::directory_entry_size_x86;
				case 105:
					return detail::constants::directory_entry_size_x64;
				default:
					detail::declare_unreachable();
				}
			}();

			std::uint32_t offset = 0;
			offset += static_cast<std::uint32_t>(a_header.directories_offset());
			offset += static_cast<std::uint32_t>(dirsz * a_header.directory_count());
			offset += static_cast<std::uint32_t>(
				detail::constants::file_entry_size *
				a_header.file_count());
			for (const auto& dir : _directories) {
				for (const auto& file : dir) {
					offset += static_cast<std::uint32_t>(file.filename().size() +
														 1u +  // prefixed byte length
														 1u);  // null terminator
				}
			}

			a_in.seek_absolute(offset);
			for (auto& dir : _directories) {
				dir.read_file_names(a_in);
			}
		}

		[[nodiscard]] auto write_directory_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept
			-> std::uint32_t
		{
			const auto match = [&](auto&& a_x86, auto&& a_x64) noexcept {
				switch (a_header.version()) {
				case 103:
				case 104:
					return a_x86();
				case 105:
					return a_x64();
				default:
					detail::declare_unreachable();
				}
			};

			const auto dirsz = match(
				[]() noexcept { return detail::constants::directory_entry_size_x86; },
				[]() noexcept { return detail::constants::directory_entry_size_x64; });

			std::uint32_t offset = 0;
			offset += static_cast<std::uint32_t>(a_header.directories_offset());
			offset += static_cast<std::uint32_t>(dirsz * a_header.directory_count());
			offset += static_cast<std::uint32_t>(a_header.file_names_length());

			for (const auto& dir : _directories) {
				dir.hash().write(a_out, a_header.endian());
				a_out << static_cast<std::uint32_t>(dir.size());

				match(
					[&]() noexcept { a_out << offset; },
					[&]() noexcept {
						a_out
							<< std::uint32_t{ 0 }
							<< offset
							<< std::uint32_t{ 0 };
					});

				if (a_header.directory_strings()) {
					offset += static_cast<std::uint32_t>(dir.name().length() +
														 1u +  // prefixed byte length
														 1u);  // null terminator
				}

				offset += static_cast<std::uint32_t>(
					detail::constants::file_entry_size *
					dir.size());
			}

			return offset;
		}

		void write_file_data(
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept
		{
			for (const auto& dir : _directories) {
				dir.write_file_data(a_out, a_header);
			}
		}

		void write_file_entries(
			detail::ostream_t& a_out,
			const detail::header_t& a_header,
			std::uint32_t a_dataOffset) const noexcept
		{
			for (const auto& dir : _directories) {
				dir.write_file_entries(a_out, a_header, a_dataOffset);
			}
		}

		void write_file_names(detail::ostream_t& a_out) const noexcept
		{
			for (const auto& dir : _directories) {
				dir.write_file_names(a_out);
			}
		}

		container_type _directories;
		archive_flag _flags{ archive_flag::none };
		archive_type _types{ archive_type::none };
	};
}

#undef BSA_MAKE_ALL_ENUM_OPERATORS
#undef BSA_MAKE_ENUM_OPERATOR_PAIR
