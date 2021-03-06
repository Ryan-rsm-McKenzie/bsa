#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

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

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;

			[[nodiscard]] friend auto operator<=>(const hash& a_lhs, const hash& a_rhs) noexcept
				-> std::strong_ordering { return a_lhs.numeric() <=> a_rhs.numeric(); }

			/// \brief	Obtains the numeric value of the hash used for comparisons.
			[[nodiscard]] auto numeric() const noexcept
				-> std::uint64_t
			{
				return std::uint64_t{
					std::uint64_t{ hi } << 0u * 8u |
					std::uint64_t{ lo } << 4u * 8u
				};
			}

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
		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	/// \brief	Represents a file within the TES:3 virtual filesystem.
	class file final :
		public components::byte_container
	{
	private:
		friend archive;
		using super = components::byte_container;

	public:
		/// \brief	The key used to indentify a file.
		using key = components::key<hashing::hash, hashing::hash_file>;

		/// \brief	Clears the contents of the file.
		using super::clear;
	};

	/// \brief	Represents the TES:3 revision of the bsa format.
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

		/// \brief	Reads the contents of the archive from disk.
		///
		/// \exception	std::system_error	Thrown when filesystem errors are encountered.
		/// \exception	bsa::exception	Thrown when archive parsing errors are encountered.
		///
		/// \remark	If `std::system_error` is thrown, the archive is left unmodified.
		///	\remark	If `bsa::exception` is throw, the archive is left in an unspecified state.
		///		Use \ref clear to return it to a valid state.
		/// \remark	If the function returns successfully, the contents of the archived are replaced
		///		with the contents of the archive on disk.
		///
		/// \param	a_path	The path to the given archive on the native filesystem.
		void read(std::filesystem::path a_path);

		/// \brief	Verifies that offsets within the archive will be valid when written to disk.
		///
		/// \return	Returns `true` is the archive passes validation, `false` otherwise.
		[[nodiscard]] bool verify_offsets() const noexcept;

		/// \brief	Writes the contents of the archive to disk.
		///
		/// \param	a_path	The path to write the archive to on the native filesystem.
		void write(std::filesystem::path a_path) const;

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
