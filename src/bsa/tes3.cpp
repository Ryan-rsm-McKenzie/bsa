#include "bsa/tes3.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace bsa::tes3
{
	namespace detail
	{
		class header_t final
		{
		public:
			header_t() noexcept = default;

			header_t(
				std::uint32_t a_hashOffset,
				std::uint32_t a_fileCount) noexcept :
				_hashOffset(a_hashOffset),
				_fileCount(a_fileCount)
			{}

			friend auto operator>>(
				istream_t& a_in,
				header_t& a_header)
				-> istream_t&
			{
				std::uint32_t magic = 0;
				a_in >>
					magic >>
					a_header._hashOffset >>
					a_header._fileCount;

				if (magic != 0x100) {
					throw exception("invalid magic");
				}

				return a_in;
			}

			friend auto operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
				-> ostream_t&
			{
				return a_out
				       << std::uint32_t{ 0x100 }
				       << a_header._hashOffset
				       << a_header._fileCount;
			}

			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }
			[[nodiscard]] auto hash_offset() const noexcept -> std::size_t { return _hashOffset; }

		private:
			std::uint32_t _hashOffset{ 0 };
			std::uint32_t _fileCount{ 0 };
		};

		namespace
		{
			namespace constants
			{
				inline constexpr std::size_t file_entry_size = 0x8;
				inline constexpr std::size_t hash_size = 0x8;
				inline constexpr std::size_t header_size = 0xC;
			}

			[[nodiscard]] inline auto offsetof_file_entries(const detail::header_t&) noexcept
				-> std::size_t { return constants::header_size; }

			[[nodiscard]] inline auto offsetof_name_offsets(const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				return offsetof_file_entries(a_header) +
				       a_header.file_count() * constants::file_entry_size;
			}

			[[nodiscard]] inline auto offsetof_names(const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				return offsetof_name_offsets(a_header) +
				       a_header.file_count() * 4u;
			}

			[[nodiscard]] inline auto offsetof_hashes(const detail::header_t& a_header) noexcept
				-> std::size_t { return a_header.hash_offset() + constants::header_size; }

			[[nodiscard]] inline auto offsetof_file_data(const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				return offsetof_hashes(a_header) +
				       a_header.file_count() * constants::hash_size;
			}
		}
	}

	namespace hashing
	{
		auto operator>>(
			detail::istream_t& a_in,
			hash& a_hash)
			-> detail::istream_t&
		{
			return a_in >>
			       a_hash.lo >>
			       a_hash.hi;
		}

		auto operator<<(
			detail::ostream_t& a_out,
			const hash& a_hash) noexcept
			-> detail::ostream_t&
		{
			return a_out
			       << a_hash.lo
			       << a_hash.hi;
		}

		hash hash_file(std::string& a_path) noexcept
		{
			detail::normalize_path(a_path);
			hash h;

			const std::size_t midpoint = a_path.length() / 2u;
			std::size_t i = 0;
			for (; i < midpoint; ++i) {
				// rotate between first 4 bytes
				h.lo ^= std::uint32_t{ static_cast<unsigned char>(a_path[i]) }
				        << ((i % 4u) * 8u);
			}

			for (std::uint32_t rot = 0; i < a_path.length(); ++i) {
				// rotate between last 4 bytes
				rot = std::uint32_t{ static_cast<unsigned char>(a_path[i]) }
				      << (((i - midpoint) % 4u) * 8u);
				h.hi = std::rotr(h.hi ^ rot, static_cast<int>(rot));
			}

			return h;
		}
	}

	struct archive::offsets_t final
	{
		std::size_t hashes{ 0 };
		std::size_t nameOffsets{ 0 };
		std::size_t names{ 0 };
		std::size_t fileData{ 0 };

		friend offsets_t& operator+=(
			offsets_t& a_lhs,
			const offsets_t& a_rhs) noexcept
		{
			a_lhs.hashes += a_rhs.hashes;
			a_lhs.nameOffsets += a_rhs.nameOffsets;
			a_lhs.names += a_rhs.names;
			a_lhs.fileData += a_rhs.fileData;
			return a_lhs;
		}

		friend offsets_t& operator-=(
			offsets_t& a_lhs,
			const offsets_t& a_rhs) noexcept
		{
			a_lhs.hashes -= a_rhs.hashes;
			a_lhs.nameOffsets -= a_rhs.nameOffsets;
			a_lhs.names -= a_rhs.names;
			a_lhs.fileData -= a_rhs.fileData;
			return a_lhs;
		}
	};

	void archive::read(std::filesystem::path a_path)
	{
		detail::istream_t in{ std::move(a_path) };
		const auto header = [&]() {
			detail::header_t result;
			in >> result;
			return result;
		}();

		this->clear();

		const offsets_t offsets{
			detail::offsetof_hashes(header),
			detail::offsetof_name_offsets(header),
			detail::offsetof_names(header),
			detail::offsetof_file_data(header)
		};

		for (std::size_t i = 0; i < header.file_count(); ++i) {
			this->read_file(in, offsets, i);
		}
	}

	bool archive::verify_offsets() const noexcept
	{
		offsets_t total;
		offsets_t last;

		for (const auto& [key, file] : *this) {
			last.nameOffsets += key.name().length() +
			                    1u;  // include null terminator
			last.fileData += file.size();

			total += last;
		}

		total.hashes =
			detail::constants::header_size +
			(detail::constants::file_entry_size + 4u) * this->size() +
			total.nameOffsets;
		total -= last;

		const std::array offsets{
			total.nameOffsets,
			total.hashes,
			total.fileData
		};
		for (const auto offset : offsets) {
			if (offset > std::numeric_limits<std::uint32_t>::max()) {
				return false;
			}
		}
		return true;
	}

	void archive::write(std::filesystem::path a_path) const
	{
		detail::ostream_t out{ std::move(a_path) };
		out << this->make_header();

		this->write_file_entries(out);
		this->write_file_name_offsets(out);
		this->write_file_names(out);
		this->write_file_hashes(out);
		this->write_file_data(out);
	}

	auto archive::make_header() const noexcept
		-> detail::header_t
	{
		std::size_t offset =
			(detail::constants::file_entry_size + 4u) * this->size();
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			offset += key.name().length() +
			          1u;  // include null terminator
		}

		return {
			static_cast<std::uint32_t>(offset),
			static_cast<std::uint32_t>(this->size())
		};
	}

	void archive::read_file(
		detail::istream_t& a_in,
		const offsets_t& a_offsets,
		std::size_t a_idx)
	{
		const auto hash = [&]() {
			const detail::restore_point _{ a_in };
			a_in.seek_absolute(a_offsets.hashes);
			a_in.seek_relative(detail::constants::hash_size * a_idx);
			hashing::hash h;
			a_in >> h;
			return h;
		}();
		const auto name = [&]() {
			const detail::restore_point _{ a_in };
			a_in.seek_absolute(a_offsets.nameOffsets);
			a_in.seek_relative(4u * a_idx);
			std::uint32_t offset = 0;
			a_in >> offset;
			a_in.seek_absolute(a_offsets.names + offset);
			return detail::read_zstring(a_in);
		}();

		[[maybe_unused]] const auto [it, success] =
			this->insert(
				key_type{ hash, name, a_in },
				mapped_type{});
		assert(success);

		std::uint32_t size = 0;
		std::uint32_t offset = 0;
		a_in >> size >> offset;

		const detail::restore_point _{ a_in };
		a_in.seek_absolute(a_offsets.fileData + offset);
		it->second.set_data(a_in.read_bytes(size), a_in);
	}

	void archive::write_file_entries(detail::ostream_t& a_out) const noexcept
	{
		std::uint32_t offset = 0;
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			const auto size = static_cast<std::uint32_t>(file.size());
			a_out << size << offset;
			offset += size;
		}
	}

	void archive::write_file_name_offsets(detail::ostream_t& a_out) const noexcept
	{
		std::uint32_t offset = 0;
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			a_out << offset;
			offset += static_cast<std::uint32_t>(
				key.name().length() +
				1u);  // include null terminator
		}
	}

	void archive::write_file_names(detail::ostream_t& a_out) const noexcept
	{
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			detail::write_zstring(a_out, key.name());
		}
	}

	void archive::write_file_hashes(detail::ostream_t& a_out) const noexcept
	{
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			a_out << key.hash();
		}
	}

	void archive::write_file_data(detail::ostream_t& a_out) const noexcept
	{
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			a_out.write_bytes(file.as_bytes());
		}
	}
}
