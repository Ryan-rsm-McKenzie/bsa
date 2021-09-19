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

			// archives are sorted in whatever order you add files, i.e. not at all
			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;
			[[nodiscard]] friend std::strong_ordering operator<=>(const hash&, const hash&) noexcept = default;

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

	/// \brief	Represents a chunk of a file within the FO:4 virtual filesystem.
	class chunk final :
		public components::compressed_byte_container
	{
	private:
		friend archive;
		using super = components::compressed_byte_container;

	public:
		/// \brief	Unique to \ref format::directx.
		struct mips_t final
		{
			std::uint16_t first{ 0 };
			std::uint16_t last{ 0 };

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

		/// \brief	Clears the data and mips of the chunk.
		void clear() noexcept
		{
			super::clear();
			this->mips = mips_t{};
		}

		/// \brief	Compresses the file.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		/// \post	The file will be \ref compressed() "compressed".
		///
		/// \return	Returns `true` if compression succeeded, `false` otherwise.
		bool compress() noexcept;

		/// \brief	Returns an upper bound on the storage size required to compress the file.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		///
		/// \return	Returns the size required to successfully compress the file.
		[[nodiscard]] std::size_t compress_bound() const noexcept;

		/// \brief	Compresses the file into the given buffer.
		///
		/// \pre	The file must *not* be \ref compressed() "compressed".
		/// \pre	`a_out` must be \ref compress_bound() "large enough" to compress the
		/// 	file into.
		///
		/// \param	a_out	The buffer to compress the file into.
		/// \return	The final size of the compressed buffer, or `std::nullopt` if
		/// 	compression failed.
		[[nodiscard]] std::optional<std::size_t> compress_into(std::span<std::byte> a_out) noexcept;

		/// \brief	Decompresses the file.
		///
		/// \pre	The file *must* be \ref compressed() "compressed".
		/// \post	The file will be \ref compressed() "decompressed".
		///
		/// \return Returns `true` if decompression succeeded, `false` otherwise.
		bool decompress() noexcept;

		/// \brief	Decompresses the file into the given buffer.
		///
		/// \pre	The file *must* be \ref compressed() "compressed".
		/// \pre	`a_out` must be \ref decompressed_size() "large enough" to
		/// 	decompress the file into.
		///
		/// \param	a_out	The buffer to decompress the file into.
		/// \return	Returns `true` if decompression succeeded, `false` otherwise.
		bool decompress_into(std::span<std::byte> a_out) noexcept;
	};

	/// \brief	Represents a file within the FO:4 virtual filesystem.
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

#ifdef DOXYGEN
		using value_type = chunk;
#else
		using value_type = container_type::value_type;
#endif
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		/// \brief	The key used to indentify a file.
		using key = components::key<hashing::hash, hashing::hash_file_in_place>;

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

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

		/// \brief	Returns a reference to the \ref chunk at the back of the file.
		///
		/// \pre	The container must *not* be empty.
		[[nodiscard]] value_type& back() noexcept { return _chunks.back(); }
		/// \copydoc file::back
		[[nodiscard]] const value_type& back() const noexcept { return _chunks.back(); }

		/// \brief	Returns the number of chunks the file can store without reallocating.
		[[nodiscard]] std::size_t capacity() const noexcept { return _chunks.capacity(); }

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

		/// \brief	Checks if the file is empty.
		[[nodiscard]] bool empty() const noexcept { return _chunks.empty(); }

		/// \brief	Returns a reference to the \ref chunk at the front of the file.
		///
		/// \pre	The file must *not* be empty.
		[[nodiscard]] value_type& front() noexcept { return _chunks.front(); }
		/// \copydoc file::front
		[[nodiscard]] const value_type& front() const noexcept { return _chunks.front(); }

		/// \brief	Removes a \ref chunk from the file.
		void pop_back() noexcept { _chunks.pop_back(); }
		/// \brief	Appends a \ref chunk to the file.
		void push_back(value_type a_value) noexcept { _chunks.push_back(std::move(a_value)); }
		/// \brief	Reserves storage for `a_count` chunks.
		void reserve(std::size_t a_count) noexcept { _chunks.reserve(a_count); }

		/// \brief	Shrinks the file's capacity to its size.
		void shrink_to_fit() noexcept { _chunks.shrink_to_fit(); }
		/// \brief	Returns the number of chunks in the file.
		[[nodiscard]] std::size_t size() const noexcept { return _chunks.size(); }

	private:
		container_type _chunks;
	};

	/// \brief	Represents the FO:4 revision of the ba2 format.
	class archive final :
		public components::hashmap<file>
	{
	private:
		using super = components::hashmap<file>;

	public:
#ifdef DOXYGEN
		/// \brief	Clears the contents of the archive.
		void clear() noexcept;
#else
		using super::clear;
#endif

		/// \copydoc bsa::tes3::archive::read(std::filesystem::path)
		///
		/// \return	The format of the archive that was read.
		format read(std::filesystem::path a_path);

		/// \copydoc bsa::tes3::archive::read(std::span<const std::byte>, copy_type)
		///
		/// \return	The format of the archive that was read.
		format read(
			std::span<const std::byte> a_src,
			copy_type a_copy = copy_type::deep);

		/// \copydoc bsa::tes3::archive::write(std::filesystem::path) const
		/// \copydoc bsa::fo4::archive::write(format, bool) const
		void write(
			std::filesystem::path a_path,
			format a_format,
			bool a_strings = true) const;

		/// \copydoc bsa::tes3::archive::write(binary_io::any_ostream&) const
		/// \copydoc bsa::fo4::archive::write(format, bool) const
		void write(
			binary_io::any_ostream& a_dst,
			format a_format,
			bool a_strings = true) const;

#ifdef DOXYGEN
	protected:
		/// \param	a_format	The format to write the archive in.
		/// \param	a_strings	Controls whether the string table is written or not.
		void write(format a_format, bool a_strings) const;
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
