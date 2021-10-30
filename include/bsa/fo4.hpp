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
#include <utility>
#include <vector>

#include <binary_io/any_stream.hpp>

#include "bsa/detail/common.hpp"

namespace bsa::fo4
{
#ifndef DOXYGEN
	namespace detail
	{
		using namespace bsa::detail;

		namespace constants
		{
			inline constexpr auto gnrl = make_four_cc("GNRL"sv);
			inline constexpr auto dx10 = make_four_cc("DX10"sv);
		}
	}
#endif

	/// \brief	Represents the file format for an archive.
	enum class format : std::uint32_t
	{
		general = detail::constants::gnrl,
		directx = detail::constants::dx10,
	};

	namespace hashing
	{
		struct hash final
		{
			/// \brief	The file's stem crc.
			std::uint32_t file{ 0 };

			/// \brief	The first 4 bytes of the file's extension.
			std::uint32_t extension{ 0 };

			/// \brief	The file's parent path crc.
			std::uint32_t directory{ 0 };

			/// \name Comparison
			/// @{

			// archives are sorted in whatever order you add files, i.e. not at all
			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;
			[[nodiscard]] friend std::strong_ordering operator<=>(const hash&, const hash&) noexcept = default;

			/// @}

#ifndef DOXYGEN
			friend auto operator>>(
				detail::istream_t& a_in,
				hash& a_hash)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&;
#endif
		};

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

	/// \brief	Represents a chunk of a file within the FO4 virtual filesystem.
	class chunk final :
		public components::compressed_byte_container
	{
	private:
		friend archive;
		friend file;
		using super = components::compressed_byte_container;

	public:
		/// \brief	Unique to \ref format::directx.
		struct mips_t final
		{
			std::uint16_t first{ 0 };
			std::uint16_t last{ 0 };

			/// \name Comparison
			/// @{

			[[nodiscard]] friend constexpr bool operator==(mips_t, mips_t) noexcept = default;

			/// @}

#ifndef DOXYGEN
			friend auto operator>>(
				detail::istream_t& a_in,
				mips_t& a_mips)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const mips_t& a_mips) noexcept
				-> detail::ostream_t&;
#endif
		} mips;

		/// \name Compression
		/// @{

		/// \brief	Compresses the file.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		/// \post	The file will be \ref compressed() "compressed".
		///
		/// \exception	bsa::compression_error	Thrown when any backend compression library errors
		///		are encountered.
		///
		/// \remark	If a compression error is thrown, then the contents are left unchanged.
		void compress();

		/// \brief	Returns an upper bound on the storage size required to compress the file.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		///
		/// \exception	bsa::compression_error	Thrown when any backend compression library errors
		///		are encountered.
		///
		/// \return	Returns the size required to successfully compress the file.
		[[nodiscard]] std::size_t compress_bound() const;

		/// \brief	Compresses the file into the given buffer.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		/// \pre	`a_out` must be \ref compress_bound() "large enough" to compress the
		/// 	file into.
		///
		/// \exception	bsa::compression_error	Thrown when any backend compression library errors
		///		are encountered.
		///
		/// \param	a_out	The buffer to compress the file into.
		/// \return	The final size of the compressed buffer.
		///
		/// \remark	If a compression error is thrown, then the contents of `a_out` are left
		///		in an unspecified state.
		[[nodiscard]] std::size_t compress_into(std::span<std::byte> a_out) const;

		/// @}

		/// \name Decompression
		/// @{

		/// \brief	Decompresses the file.
		///
		/// \pre	The file *must* be \ref compressed() "compressed".
		/// \post	The file will be \ref compressed() "decompressed".
		///
		/// \exception	bsa::compression_error	Thrown when any backend compression library errors
		///		are encountered.
		///
		/// \remark	If a compression error is thrown, then the contents are left unchanged.
		void decompress();

		/// \brief	Decompresses the file into the given buffer.
		///
		/// \pre	The file *must* be \ref compressed() "compressed".
		/// \pre	`a_out` must be \ref decompressed_size() "large enough" to
		/// 	decompress the file into.
		///
		/// \exception	bsa::compression_error	Thrown when any backend compression library errors
		///		are encountered.
		///
		/// \param	a_out	The buffer to decompress the file into.
		///
		/// \remark	If a compression error is thrown, then the contents of `a_out` are left
		///		in an unspecified state.
		void decompress_into(std::span<std::byte> a_out) const;

		/// @}

		/// \name Modifiers
		/// @{

		/// \brief	Clears the data and mips of the chunk.
		void clear() noexcept
		{
			super::clear();
			this->mips = mips_t{};
		}

		/// @}
	};

	/// \brief	Represents a file within the FO4 virtual filesystem.
	class file final
	{
	private:
		using container_type = std::vector<chunk>;

	public:
		/// \brief	Unique to \ref format::directx.
		struct header_t final
		{
			std::uint16_t height{ 0 };
			std::uint16_t width{ 0 };
			std::uint8_t mip_count{ 0 };
			std::uint8_t format{ 0 };
			std::uint8_t flags{ 0 };
			std::uint8_t tile_mode{ 0 };

			/// \name Comparison
			/// @{

			[[nodiscard]] friend constexpr bool operator==(header_t, header_t) noexcept = default;

			/// @}

#ifndef DOXYGEN
			friend auto operator>>(
				detail::istream_t& a_in,
				header_t& a_header)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const header_t& a_header) noexcept
				-> detail::ostream_t&;
#endif
		} header;

		/// \name Member types
		/// @{

#ifdef DOXYGEN
		using value_type = chunk;
#else
		using value_type = container_type::value_type;
#endif
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		/// \brief	The key used to indentify a file.
		using key = components::key<hashing::hash, hashing::hash_file_in_place>;

		/// @}

		/// \name Assignment
		/// @{

		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		/// @}

		/// \name Capacity
		/// @{

		/// \brief	Returns the number of chunks the file can store without reallocating.
		[[nodiscard]] std::size_t capacity() const noexcept { return _chunks.capacity(); }

		/// \brief	Checks if the file is empty.
		[[nodiscard]] bool empty() const noexcept { return _chunks.empty(); }

		/// \brief	Reserves storage for `a_count` chunks.
		void reserve(std::size_t a_count) noexcept { _chunks.reserve(a_count); }

		/// \brief	Shrinks the file's capacity to its size.
		void shrink_to_fit() noexcept { _chunks.shrink_to_fit(); }

		/// \brief	Returns the number of chunks in the file.
		[[nodiscard]] std::size_t size() const noexcept { return _chunks.size(); }

		/// @}

		/// \name Constructors
		/// @{

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;

		/// @}

		/// \name Destructors
		/// @{

		~file() noexcept = default;

		/// @}

		/// \name Element access
		/// @{

		/// \brief	Returns the \ref chunk at the given position.
		///
		/// \pre	`a_pos` *must* be less than the current size.
		[[nodiscard]] value_type& operator[](std::size_t a_pos) noexcept
		{
			return _chunks[a_pos];
		}

		/// \copydoc file::operator[]
		[[nodiscard]] const value_type& operator[](std::size_t a_pos) const noexcept
		{
			return _chunks[a_pos];
		}

		/// \brief	Returns a reference to the \ref chunk at the back of the file.
		///
		/// \pre	The container must *not* be empty.
		[[nodiscard]] value_type& back() noexcept { return _chunks.back(); }
		/// \copydoc file::back
		[[nodiscard]] const value_type& back() const noexcept { return _chunks.back(); }

		/// \brief	Returns a reference to the \ref chunk at the front of the file.
		///
		/// \pre	The file must *not* be empty.
		[[nodiscard]] value_type& front() noexcept { return _chunks.front(); }
		/// \copydoc file::front
		[[nodiscard]] const value_type& front() const noexcept { return _chunks.front(); }

		/// @}

		/// \name Iterators
		/// @{

		/// \brief	Returns an iterator to the beginning of the file.
		[[nodiscard]] iterator begin() noexcept { return _chunks.begin(); }
		/// \copybrief file::begin
		[[nodiscard]] const_iterator begin() const noexcept { return _chunks.begin(); }
		/// \copybrief file::begin
		[[nodiscard]] const_iterator cbegin() const noexcept { return _chunks.cbegin(); }

		/// \brief	Returns an iterator to the end of the file.
		[[nodiscard]] iterator end() noexcept { return _chunks.end(); }
		/// \copybrief file::end
		[[nodiscard]] const_iterator end() const noexcept { return _chunks.end(); }
		/// \copybrief file::end
		[[nodiscard]] const_iterator cend() const noexcept { return _chunks.cend(); }

		/// @}

		/// \name Modifiers
		/// @{

		/// \brief	Clears the chunks and header of the file.
		void clear() noexcept
		{
			_chunks.clear();
			this->header = header_t{};
		}

		/// \brief	Constructs a \ref chunk in-place, inside the file.
		template <class... Args>
		value_type& emplace_back(Args&&... a_args) noexcept
		{
			return _chunks.emplace_back(std::forward<Args>(a_args)...);
		}

		/// \brief	Removes a \ref chunk from the file.
		void pop_back() noexcept { _chunks.pop_back(); }
		/// \brief	Appends a \ref chunk to the file.
		void push_back(value_type a_value) noexcept { _chunks.push_back(std::move(a_value)); }

		/// @}

		/// \name Reading
		/// @{

		/// \copydoc bsa::tes3::file::read(std::filesystem::path)
		/// \copydoc bsa::fo4::file::doxygen_read
		void read(
			std::filesystem::path a_path,
			format a_format,
			std::size_t a_mipChunkMax = 512u * 512u,
			compression_type a_compression = compression_type::decompressed);

		/// \copydoc bsa::tes3::file::read(std::span<const std::byte>, copy_type)
		/// \copydoc bsa::fo4::file::doxygen_read
		void read(
			std::span<const std::byte> a_src,
			format a_format,
			std::size_t a_mipChunkMax = 512u * 512u,
			compression_type a_compression = compression_type::decompressed,
			copy_type a_copy = copy_type::deep);

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::tes3::file::write(std::filesystem::path) const
		/// \copydoc bsa::fo4::file::doxygen_write
		void write(
			std::filesystem::path a_path,
			format a_format) const;

		/// \copydoc bsa::tes3::file::write(binary_io::any_ostream&) const
		/// \copydoc bsa::fo4::file::doxygen_write
		void write(
			binary_io::any_ostream& a_dst,
			format a_format) const;

		/// @}

#ifdef DOXYGEN
	protected:
		/// \name Doxygen only
		/// @{

		/// \param	a_format	The format to read the file as.
		/// \param	a_mipChunkMax	The maxiumum size to restrict a single mip chunk to.
		/// \param	a_compression	The resulting compression of the file read.
		void doxygen_read(
			format a_format,
			std::size_t a_mipChunkMax = 512u * 512u,
			compression_type a_compression = compression_type::decompressed);

		/// \param	a_format	The format to write the file as.
		void doxygen_write(format a_format) const;

		/// @}
#endif

	private:
		void do_read(
			detail::istream_t& a_in,
			format a_format,
			std::size_t a_mipChunkMax,
			compression_type a_compression);
		void do_write(
			detail::ostream_t& a_out,
			format a_format) const;

		void read_directx(
			detail::istream_t& a_in,
			std::size_t a_mipChunkMax,
			compression_type a_compression);
		void read_general(
			detail::istream_t& a_in,
			compression_type a_compression);

		void write_directx(detail::ostream_t& a_out) const;
		void write_general(detail::ostream_t& a_out) const;

		container_type _chunks;
	};

	/// \brief	Represents the FO4 revision of the ba2 format.
	class archive final :
		public components::hashmap<file>
	{
	private:
		using super = components::hashmap<file>;

	public:
		/// \name Modifiers
		/// @{

#ifdef DOXYGEN
		/// \brief	Clears the contents of the archive.
		void clear() noexcept;
#else
		using super::clear;
#endif

		/// @}

		/// \name Reading
		/// @{

		/// \copydoc bsa::tes3::archive::read(std::filesystem::path)
		/// \copydoc bsa::fo4::archive::doxygen_read
		format read(std::filesystem::path a_path);

		/// \copydoc bsa::tes3::archive::read(std::span<const std::byte>, copy_type)
		/// \copydoc bsa::fo4::archive::doxygen_read
		format read(
			std::span<const std::byte> a_src,
			copy_type a_copy = copy_type::deep);

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::tes3::archive::write(std::filesystem::path) const
		/// \copydoc bsa::fo4::archive::doxygen_write
		void write(
			std::filesystem::path a_path,
			format a_format,
			bool a_strings = true) const;

		/// \copydoc bsa::tes3::archive::write(binary_io::any_ostream&) const
		/// \copydoc bsa::fo4::archive::doxygen_write
		void write(
			binary_io::any_ostream& a_dst,
			format a_format,
			bool a_strings = true) const;

		/// @}

#ifdef DOXYGEN
	protected:
		/// \name Doxygen only
		/// @{

		/// \return	The format of the archive that was read.
		format doxygen_read();

		/// \param	a_format	The format to write the archive in.
		/// \param	a_strings	Controls whether the string table is written or not.
		void doxygen_write(format a_format, bool a_strings) const;

		/// @}
#endif

	private:
		[[nodiscard]] auto do_read(detail::istream_t& a_in) -> format;

		void do_write(
			detail::ostream_t& a_out,
			format a_format,
			bool a_strings) const;

		[[nodiscard]] auto make_header(
			format a_format,
			bool a_strings) const noexcept
			-> std::pair<detail::header_t, std::uint64_t>;

		void read_chunk(
			chunk& a_chunk,
			detail::istream_t& a_in,
			format a_format);

		void read_file(
			file& a_file,
			detail::istream_t& a_in,
			format a_format);

		void write_chunk(
			const chunk& a_chunk,
			detail::ostream_t& a_out,
			format a_format,
			std::uint64_t& a_dataOffset) const noexcept;

		void write_file(
			const file& a_file,
			detail::ostream_t& a_out,
			format a_format,
			std::uint64_t& a_dataOffset) const noexcept;
	};
}
