#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <binary_io/any_stream.hpp>

#include "bsa/detail/common.hpp"

namespace bsa::tes4
{
	/// \brief	Archive flags can impact the layout of an archive, or how it is read.
	enum class archive_flag : std::uint32_t
	{
		none = 0u,

		/// \brief	Includes directory paths within the archive.
		/// \remark	archive.exe does not let you write archives without this flag set.
		/// \remark	This includes only the parent path of all files, and not filenames.
		directory_strings = 1u << 0u,

		/// \brief	Includes filenames within the archive.
		/// \remark	archive.exe does not let you write archives without this flag set.
		/// \remark	This includes only the filename of all files, and not the parent path.
		file_strings = 1u << 1u,

		/// \brief	Compresses the data within the archive.
		/// \remark	The v103 format uses zlib.
		/// \remark	The v104 format uses xmem and zlib.
		/// \remark	The v105 format uses lz4.
		compressed = 1u << 2u,

		/// \brief	Impacts runtime parsing.
		retain_directory_names = 1u << 3u,

		/// \brief	Impacts runtime parsing.
		retain_file_names = 1u << 4u,

		/// \brief	Impacts runtime parsing.
		retain_file_name_offsets = 1 << 5u,

		/// \brief	Writes the archive in the xbox (big-endian) format.
		/// \remark	This flag affects the sort order of files on disk.
		/// \remark	Only the crc hash is actually written in big-endian format.
		xbox_archive = 1u << 6u,

		/// \brief	Impacts runtime parsing.
		retain_strings_during_startup = 1u << 7u,

		/// \brief	Writes the full (virtual) path of a file next to the data blob.
		/// \remark	This flag has a different meaning in the v103 format.
		embedded_file_names = 1u << 8u,

		/// \brief	Uses the xmem codec from XNA 4.0 to compress the archive.
		/// \remark	This flag requires \ref archive_flag::compressed to be set as well.
		/// \remark	This flag is unused in SSE.
		xbox_compressed = 1u << 9u
	};

#ifndef DOXYGEN
	BSA_MAKE_ALL_ENUM_OPERATORS(archive_flag)
#endif

	/// \brief	Specifies file types contained within an archive.
	/// \remarks	It's not apparent if the game actually uses these flags for anything.
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

	/// \brief	Specifies the codec to use when performing compression/decompression actions on files.
	enum class compression_codec
	{
		/// \brief	The default compression codec.
		normal,

		/// \brief	The compression codec used for xbox archives.
		xmem
	};

#ifndef DOXYGEN
	BSA_MAKE_ALL_ENUM_OPERATORS(archive_type)
#endif

	/// \brief	The archive version.
	/// \remark	Each version has an impact on the abi of the TES4 archive file format.
	enum class version : std::uint32_t
	{
		/// \brief	The Elder Scrolls IV: Oblivion
		tes4 = 103,

		/// \brief	Fallout 3
		fo3 = 104,

		/// \brief	The Elder Scrolls V: Skyrim
		tes5 = 104,

		/// \brief	The Elder Scrolls V: Skyrim - Special Edition
		sse = 105,
	};

#ifndef DOXYGEN
	namespace detail
	{
		using namespace bsa::detail;
	}
#endif

	namespace hashing
	{
		/// \copybrief bsa::tes3::hashing::hash
		struct hash final
		{
		public:
			/// \brief	The last character of the path (directory) or stem (file).
			std::uint8_t last{ 0 };

			/// \brief	The second to last character of the path (directory) or stem (file).
			std::uint8_t last2{ 0 };

			/// \brief	The length of the path (directory) or stem (file).
			std::uint8_t length{ 0 };

			/// \brief	The first character of the path (directory) or stem (file).
			std::uint8_t first{ 0 };

			std::uint32_t crc{ 0 };

			/// \name Comparison
			/// @{

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;

			[[nodiscard]] friend std::strong_ordering operator<=>(
				const hash& a_lhs,
				const hash& a_rhs) noexcept
			{
				return a_lhs.numeric() <=> a_rhs.numeric();
			}

			/// @}

			/// \name Observers
			/// @{

			/// \copybrief bsa::tes3::hashing::hash
			[[nodiscard]] std::uint64_t numeric() const noexcept
			{
				return std::uint64_t{
					std::uint64_t{ last } << 0u * 8u |
					std::uint64_t{ last2 } << 1u * 8u |
					std::uint64_t{ length } << 2u * 8u |
					std::uint64_t{ first } << 3u * 8u |
					std::uint64_t{ crc } << 4u * 8u
				};
			}

			/// @}

		private:
#ifndef DOXYGEN
			friend tes4::archive;
			friend tes4::directory;
#endif

			void read(
				detail::istream_t& a_in,
				std::endian a_endian);

			void write(
				detail::ostream_t& a_out,
				std::endian a_endian) const noexcept;
		};

		/// \copydoc bsa::tes3::hashing::hash_file_in_place()
		[[nodiscard]] hash hash_directory_in_place(std::string& a_path) noexcept;

		/// \copydoc bsa::tes3::hashing::hash_file()
		template <concepts::stringable String>
		[[nodiscard]] hash hash_directory(String&& a_path) noexcept
		{
			std::string str(std::forward<String>(a_path));
			return hash_directory_in_place(str);
		}

		/// \copydoc bsa::tes3::hashing::hash_file_in_place()
		[[nodiscard]] hash hash_file_in_place(std::string& a_path) noexcept;

		/// \copydoc bsa::tes3::hashing::hash_file()
		template <concepts::stringable String>
		[[nodiscard]] hash hash_file(String&& a_path) noexcept
		{
			std::string str(std::forward<String>(a_path));
			return hash_file_in_place(str);
		}
	}

	/// \brief	Represents a file within the TES4 virtual filesystem.
	class file final :
		public components::compressed_byte_container
	{
	private:
		friend archive;
		using super = components::compressed_byte_container;

	public:
		/// \name Member types
		/// @{

		/// \brief	The key used to indentify a file.
		using key = components::key<hashing::hash, hashing::hash_file_in_place>;

		/// @}

		/// \name Compression
		/// @{

		/// \copydoc bsa::fo4::chunk::compress
		///
		/// \param	a_version	The version to compress the file for.
		/// \param	a_codec	The codec to use.
		void compress(
			version a_version,
			compression_codec a_codec = compression_codec::normal);

		/// \copydoc bsa::fo4::chunk::compress_bound
		///
		/// \param	a_version	The version the file would be compressed for.
		/// \param	a_codec	The codec to use.
		[[nodiscard]] std::size_t compress_bound(
			version a_version,
			compression_codec a_codec = compression_codec::normal) const;

		/// \copydoc bsa::fo4::chunk::compress_into
		///
		/// \param	a_version	The version to compress the file for.
		/// \param	a_codec	The codec to use.
		[[nodiscard]] std::size_t compress_into(
			version a_version,
			std::span<std::byte> a_out,
			compression_codec a_codec = compression_codec::normal) const;

		/// @}

		/// \name Decompression
		/// @{

		/// \copydoc bsa::fo4::chunk::decompress
		///
		/// \param	a_version	The version to decompress the file for.
		/// \param	a_codec	The codec to use.
		void decompress(
			version a_version,
			compression_codec a_codec = compression_codec::normal);

		/// \copydoc bsa::fo4::chunk::decompress_into
		///
		/// \param	a_version	The version to decompress the file for.
		/// \param	a_codec	The codec to use.
		void decompress_into(
			version a_version,
			std::span<std::byte> a_out,
			compression_codec a_codec = compression_codec::normal) const;

		/// @}

		/// \name Modifiers
		/// @{

#ifdef DOXYGEN
		/// \brief	Clears the contents of the file.
		void clear() noexcept;
#else
		using super::clear;
#endif

		/// @}

		/// \name Reading
		/// @{

		/// \copydoc bsa::tes3::file::read(std::filesystem::path)
		/// \copydoc bsa::tes4::file::doxygen_read
		void read(
			std::filesystem::path a_path,
			version a_version,
			compression_codec a_codec = compression_codec::normal,
			compression_type a_compression = compression_type::decompressed);

		/// \copydoc bsa::tes3::file::read(std::span<const std::byte>, copy_type)
		/// \copydoc bsa::tes4::file::doxygen_read
		void read(
			std::span<const std::byte> a_src,
			version a_version,
			compression_codec a_codec = compression_codec::normal,
			compression_type a_compression = compression_type::decompressed,
			copy_type a_copy = copy_type::deep);

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::tes3::file::write(std::filesystem::path) const
		/// \copydoc bsa::tes4::file::doxygen_write
		void write(
			std::filesystem::path a_path,
			version a_version,
			compression_codec a_codec = compression_codec::normal) const;

		/// \copydoc bsa::tes3::file::write(binary_io::any_ostream&) const
		/// \copydoc bsa::tes4::file::doxygen_write
		void write(
			binary_io::any_ostream& a_dst,
			version a_version,
			compression_codec a_codec = compression_codec::normal) const;

		/// @}

#ifdef DOXYGEN
	protected:
		/// \name Doxygen only
		/// @{

		/// \param	a_version	The version to compress the file for.
		/// \param	a_codec	The codec to use.
		/// \param	a_compression	The resulting compression of the file read.
		void doxygen_read(
			version a_version,
			compression_codec a_codec = compression_codec::normal,
			compression_type a_compression = compression_type::decompressed);

		/// \param	a_version	The version to decompress the file for.
		/// \param	a_codec	The codec to use.
		void doxygen_write(
			version a_version,
			compression_codec a_codec = compression_codec::normal) const;

		/// @}
#endif

	private:
		enum : std::uint32_t
		{
			icompression = 1u << 30u,
			ichecked = 1u << 31u,

			isecondary_archive = 1u << 31u
		};

		[[nodiscard]] auto compress_bound_xmem() const -> std::size_t;

		[[nodiscard]] auto compress_into_lz4(std::span<std::byte> a_out) const -> std::size_t;
		[[nodiscard]] auto compress_into_xmem(std::span<std::byte> a_out) const -> std::size_t;
		[[nodiscard]] auto compress_into_zlib(std::span<std::byte> a_out) const -> std::size_t;

		void decompress_into_lz4(std::span<std::byte> a_out) const;
		void decompress_into_xmem(std::span<std::byte> a_out) const;
		void decompress_into_zlib(std::span<std::byte> a_out) const;

		void do_read(
			detail::istream_t& a_in,
			version a_version,
			compression_codec a_codec,
			compression_type a_compression);
		void do_write(
			detail::ostream_t& a_out,
			version a_version,
			compression_codec a_codec) const;
	};

	/// \brief	Represents a directory within the TES4 virtual filesystem.
	class directory final :
		public components::hashmap<file>
	{
	private:
		friend archive;
		using super = components::hashmap<file>;

	public:
		/// \name Member types
		/// @{

		/// \brief	The key used to indentify a directory.
		using key = components::key<hashing::hash, hashing::hash_directory_in_place>;

		/// @}

		/// \name Modifiers
		/// @{

#ifdef DOXYGEN
		/// \brief	Clears the contents of the directory.
		void clear() noexcept;
#else
		using super::clear;
#endif

		/// @}
	};

	/// \brief	Represents the TES4 revision of the bsa format.
	class archive final :
		public components::hashmap<directory, true>
	{
	private:
		using super = components::hashmap<directory, true>;

	public:
		/// \name Archive flags
		/// @{

		/// \brief	Retrieves the current archive flags.
		[[nodiscard]] archive_flag archive_flags() const noexcept { return _flags; }
		/// \brief	Sets the current archive flags.
		void archive_flags(archive_flag a_flags) noexcept { _flags = a_flags; }

		/// \brief	Checks if \ref archive_flag::compressed is set.
		[[nodiscard]] bool compressed() const noexcept { return test_flag(archive_flag::compressed); }
		/// \brief	Checks if \ref archive_flag::directory_strings is set.
		[[nodiscard]] bool directory_strings() const noexcept { return test_flag(archive_flag::directory_strings); }
		/// \brief	Checks if \ref archive_flag::embedded_file_names is set.
		[[nodiscard]] bool embedded_file_names() const noexcept { return test_flag(archive_flag::embedded_file_names); }
		/// \brief	Checks if \ref archive_flag::file_strings is set.
		[[nodiscard]] bool file_strings() const noexcept { return test_flag(archive_flag::file_strings); }
		/// \brief	Checks if \ref archive_flag::retain_directory_names is set.
		[[nodiscard]] bool retain_directory_names() const noexcept { return test_flag(archive_flag::retain_directory_names); }
		/// \brief	Checks if \ref archive_flag::retain_file_name_offsets is set.
		[[nodiscard]] bool retain_file_name_offsets() const noexcept { return test_flag(archive_flag::retain_file_name_offsets); }
		/// \brief	Checks if \ref archive_flag::retain_file_names is set.
		[[nodiscard]] bool retain_file_names() const noexcept { return test_flag(archive_flag::retain_file_names); }
		/// \brief	Checks if \ref archive_flag::retain_strings_during_startup is set.
		[[nodiscard]] bool retain_strings_during_startup() const noexcept { return test_flag(archive_flag::retain_strings_during_startup); }
		/// \brief	Checks if \ref archive_flag::xbox_archive is set.
		[[nodiscard]] bool xbox_archive() const noexcept { return test_flag(archive_flag::xbox_archive); }
		/// \brief	Checks if \ref archive_flag::xbox_compressed is set.
		[[nodiscard]] bool xbox_compressed() const noexcept { return test_flag(archive_flag::xbox_compressed); }

		/// @}

		/// \name Archive types
		/// @{

		/// \brief	Retrieves the current archive types.
		[[nodiscard]] archive_type archive_types() const noexcept { return _types; }
		/// \brief	Sets the current archive types.
		void archive_types(archive_type a_types) noexcept { _types = a_types; }

		/// \brief	Checks if \ref archive_type::fonts is set.
		[[nodiscard]] bool fonts() const noexcept { return test_type(archive_type::fonts); }
		/// \brief	Checks if \ref archive_type::menus is set.
		[[nodiscard]] bool menus() const noexcept { return test_type(archive_type::menus); }
		/// \brief	Checks if \ref archive_type::meshes is set.
		[[nodiscard]] bool meshes() const noexcept { return test_type(archive_type::meshes); }
		/// \brief	Checks if \ref archive_type::misc is set.
		[[nodiscard]] bool misc() const noexcept { return test_type(archive_type::misc); }
		/// \brief	Checks if \ref archive_type::shaders is set.
		[[nodiscard]] bool shaders() const noexcept { return test_type(archive_type::shaders); }
		/// \brief	Checks if \ref archive_type::sounds is set.
		[[nodiscard]] bool sounds() const noexcept { return test_type(archive_type::sounds); }
		/// \brief	Checks if \ref archive_type::textures is set.
		[[nodiscard]] bool textures() const noexcept { return test_type(archive_type::textures); }
		/// \brief	Checks if \ref archive_type::trees is set.
		[[nodiscard]] bool trees() const noexcept { return test_type(archive_type::trees); }
		/// \brief	Checks if \ref archive_type::voices is set.
		[[nodiscard]] bool voices() const noexcept { return test_type(archive_type::voices); }

		/// @}

		/// \name Modifiers
		/// @{

		/// \brief	Clears the contents, flags, and file types of the archive.
		void clear() noexcept
		{
			super::clear();
			_flags = archive_flag::none;
			_types = archive_type::none;
		}

		/// @}

		/// \name Reading
		/// @{

		/// \copydoc bsa::tes3::archive::read(std::filesystem::path)
		/// \copydoc bsa::tes4::archive::doxygen_read
		version read(std::filesystem::path a_path);

		/// \copydoc bsa::tes3::archive::read(std::span<const std::byte>, copy_type)
		/// \copydoc bsa::tes4::archive::doxygen_read
		version read(
			std::span<const std::byte> a_src,
			copy_type a_copy = copy_type::deep);

		/// @}

		/// \name Verification
		/// @{

		/// \copydoc bsa::tes3::archive::verify_offsets
		///
		/// \param	a_version	The version format to check for.
		[[nodiscard]] bool verify_offsets(version a_version) const noexcept;

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::tes3::archive::write(std::filesystem::path) const
		/// \copydoc bsa::tes4::archive::doxygen_write
		void write(std::filesystem::path a_path, version a_version) const;

		/// \copydoc bsa::tes3::archive::write(binary_io::any_ostream&) const
		/// \copydoc bsa::tes4::archive::doxygen_write
		void write(binary_io::any_ostream& a_dst, version a_version) const;

		/// @}

#ifdef DOXYGEN
	protected:
		/// \name Doxygen only
		/// @{

		/// \return	The version of the archive that was read.
		version doxygen_read();

		/// \param	a_version The version format to write the archive in.
		void doxygen_write(version a_version) const;

		/// @}
#endif

	private:
		using intermediate_t =
			std::vector<
				std::pair<
					const value_type*,
					std::vector<const mapped_type::value_type*>>>;

		struct xbox_sort_t;

		[[nodiscard]] auto do_read(detail::istream_t& a_in) -> version;

		void do_write(detail::ostream_t& a_out, version a_version) const;

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
			std::size_t& a_filesOffset,
			std::size_t& a_namesOffset);

		[[nodiscard]] auto sort_for_write(bool a_xbox) const noexcept -> intermediate_t;

		[[nodiscard]] auto test_flag(archive_flag a_flag) const noexcept
			-> bool { return (_flags & a_flag) != archive_flag::none; }

		[[nodiscard]] auto test_type(archive_type a_type) const noexcept
			-> bool { return (_types & a_type) != archive_type::none; }

		void write_directory_entries(
			const intermediate_t& a_intermediate,
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_data(
			const intermediate_t& a_intermediate,
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_entries(
			const intermediate_t& a_intermediate,
			detail::ostream_t& a_out,
			const detail::header_t& a_header) const noexcept;

		void write_file_names(
			const intermediate_t& a_intermediate,
			detail::ostream_t& a_out) const noexcept;

		archive_flag _flags{ archive_flag::none };
		archive_type _types{ archive_type::none };
	};
}
