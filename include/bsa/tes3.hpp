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
				hash& a_hash)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&;
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class file final :
		public components::byte_container
	{
	private:
		friend archive;
		using super = components::byte_container;

	public:
		using key = components::key<hashing::hash, hashing::hash_file>;
		using super::clear;
	};

	class archive final :
		public components::hashmap<file>
	{
	private:
		using super = components::hashmap<file>;

	public:
		using super::clear;

		void read(std::filesystem::path a_path);
		[[nodiscard]] bool verify_offsets() const noexcept;
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
