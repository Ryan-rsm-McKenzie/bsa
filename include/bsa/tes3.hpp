#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>

#include <binary_io/any_stream.hpp>

#include "bsa/detail/common.hpp"

namespace bsa::tes3
{
#ifndef DOXYGEN
	namespace detail
	{
		using namespace bsa::detail;
	}
#endif

	namespace hashing
	{
		/// \brief	The underlying hash object used to uniquely identify objects within the archive.
		struct hash final
		{
		public:
			std::uint32_t lo{ 0 };
			std::uint32_t hi{ 0 };

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

			/// \brief	Obtains the numeric value of the hash used for comparisons.
			[[nodiscard]] std::uint64_t numeric() const noexcept
			{
				return std::uint64_t{
					std::uint64_t{ hi } << 0u * 8u |
					std::uint64_t{ lo } << 4u * 8u
				};
			}

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

		/// \brief	Produces a hash using the given path.
		/// \remark	The path is normalized in place. After the function returns,
		///		the path contains the string that would be stored on disk.
		[[nodiscard]] hash hash_file_in_place(std::string& a_path) noexcept;

		/// \copybrief	hash_file_in_place()
		/// \remark	See also \ref bsa::concepts::stringable.
		template <concepts::stringable String>
		[[nodiscard]] hash hash_file(String&& a_path) noexcept
		{
			std::string str(std::forward<String>(a_path));
			return hash_file_in_place(str);
		}
	}

	/// \brief	Represents a file within the TES3 virtual filesystem.
	class file final :
		public components::byte_container
	{
	private:
		friend archive;
		using super = components::byte_container;

	public:
		/// \name Member types
		/// @{

		/// \brief	The key used to indentify a file.
		using key = components::key<hashing::hash, hashing::hash_file_in_place>;

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

		/// \copydoc bsa::doxygen_detail::read
		///
		/// \param	a_source	Where/how to read the given file.
		void read(read_source a_source);

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::doxygen_detail::write
		///
		/// \param	a_sink	Where/how to write the given file.
		void write(write_sink a_sink) const;

		/// @}
	};

	/// \brief	Represents the TES3 revision of the bsa format.
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

		/// \copydoc bsa::doxygen_detail::read
		///
		/// \exception	bsa::exception	Thrown when archive parsing errors are encountered.
		///
		/// \param	a_source	Where/how to read the given archive.
		void read(read_source a_source);

		/// @}

		/// \name Validation
		/// @{

		/// \brief	Verifies that offsets within the archive will be valid when written to disk.
		///
		/// \return	Returns `true` is the archive passes validation, `false` otherwise.
		[[nodiscard]] bool verify_offsets() const noexcept;

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc bsa::doxygen_detail::write
		///
		/// \param	a_sink	Where/how to write the given archive.
		void write(write_sink a_sink) const;

		/// @}

	private:
		struct offsets_t;

		[[nodiscard]] auto make_header() const noexcept -> detail::header_t;

		void read_file(
			detail::istream_t& a_in,
			const offsets_t& a_offsets,
			std::size_t a_idx);

		void write_file_entries(detail::ostream_t& a_out) const noexcept;
		void write_file_name_offsets(detail::ostream_t& a_out) const noexcept;
		void write_file_names(detail::ostream_t& a_out) const noexcept;
		void write_file_hashes(detail::ostream_t& a_out) const noexcept;
		void write_file_data(detail::ostream_t& a_out) const noexcept;
	};
}
