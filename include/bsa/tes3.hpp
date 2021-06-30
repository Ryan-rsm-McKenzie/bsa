#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "bsa/detail/common.hpp"

namespace bsa::tes3
{
	namespace detail
	{
		using namespace bsa::detail;
	}

	namespace hashing
	{
		struct hash final
		{
		public:
			std::uint32_t lo{ 0 };
			std::uint32_t hi{ 0 };

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;

			[[nodiscard]] friend auto operator<=>(const hash& a_lhs, const hash& a_rhs) noexcept
				-> std::strong_ordering { return a_lhs.numeric() <=> a_rhs.numeric(); }

			[[nodiscard]] auto numeric() const noexcept
				-> std::uint64_t
			{
				return std::uint64_t{
					std::uint64_t{ hi } << 0u * 8u |
					std::uint64_t{ lo } << 4u * 8u
				};
			}

			friend auto operator>>(
				detail::istream_t& a_in,
				hash& a_hash) noexcept
				-> detail::istream_t&
			{
				return a_in >>
				       a_hash.lo >>
				       a_hash.hi;
			}

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&
			{
				return a_out
				       << a_hash.lo
				       << a_hash.hi;
			}
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class file final :
		public detail::components::byte_container
	{
	private:
		using super = detail::components::byte_container;

	public:
		using key = detail::key_t<hashing::hash, hashing::hash_file>;

		using super::clear;

	private:
		friend archive;

		void read(
			detail::istream_t& a_in,
			std::size_t a_dataOffset) noexcept;
	};

	class archive final :
		public detail::components::hashmap<file>
	{
	private:
		using super = detail::components::hashmap<file>;

	public:
		using super::clear;

		bool read(std::filesystem::path a_path) noexcept;

		[[nodiscard]] bool verify_offsets() const noexcept;

		bool write(std::filesystem::path a_path) const noexcept;

	private:
		struct offsets_t;

		[[nodiscard]] auto make_header() const noexcept -> detail::header_t;

		void read_file(
			detail::istream_t& a_in,
			const offsets_t& a_offsets,
			std::size_t a_idx) noexcept;

		void write_file_entries(detail::ostream_t& a_out) const noexcept;
		void write_file_name_offsets(detail::ostream_t& a_out) const noexcept;
		void write_file_names(detail::ostream_t& a_out) const noexcept;
		void write_file_hashes(detail::ostream_t& a_out) const noexcept;
		void write_file_data(detail::ostream_t& a_out) const noexcept;
	};
}
