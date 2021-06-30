#pragma once

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <boost/container/small_vector.hpp>

#include "bsa/detail/common.hpp"

namespace bsa::fo4
{
	namespace detail
	{
		using namespace bsa::detail;

		[[nodiscard]] consteval auto make_file_type(
			std::array<char, 4> a_type) noexcept
			-> std::uint32_t
		{
			std::uint32_t result = 0;
			for (std::size_t i = 0; i < a_type.size(); ++i) {
				result |= std::uint32_t{ static_cast<unsigned char>(a_type[i]) }
				          << i * 8u;
			}
			return result;
		}

		namespace constants
		{
			inline constexpr auto btdx = make_file_type({ 'B', 'T', 'D', 'X' });
			inline constexpr auto gnrl = detail::make_file_type({ 'G', 'N', 'R', 'L' });
			inline constexpr auto dx10 = detail::make_file_type({ 'D', 'X', '1', '0' });
		}

		class header_t final
		{
		public:
			header_t() noexcept = default;

			[[nodiscard]] auto archive_format() const noexcept -> std::size_t { return _format; }
			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }
			[[nodiscard]] bool good() const noexcept { return _good; }
			[[nodiscard]] auto string_table_offset() const noexcept
				-> std::uint64_t { return _stringTableOffset; }

			friend auto operator>>(
				istream_t& a_in,
				header_t& a_header) noexcept
				-> istream_t&
			{
				std::uint32_t magic = 0;
				std::uint32_t version = 0;

				a_in >>
					magic >>
					version >>
					a_header._format >>
					a_header._fileCount >>
					a_header._stringTableOffset;

				if (magic != constants::btdx) {
					a_header._good = false;
				} else if (version != 1) {
					a_header._good = false;
				}

				return a_in;
			}

			friend auto operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
				-> ostream_t&
			{
				return a_out
				       << constants::btdx
				       << std::uint32_t{ 1 }
				       << a_header._format
				       << a_header._fileCount
				       << a_header._stringTableOffset;
			}

		private:
			std::uint32_t _format{ 0 };
			std::uint32_t _fileCount{ 0 };
			std::uint64_t _stringTableOffset{ 0 };
			bool _good{ true };
		};
	}

	enum class format : std::uint32_t
	{
		general = detail::constants::gnrl,
		directx = detail::constants::dx10,
	};

	namespace hashing
	{
		struct hash final
		{
			std::uint32_t file{ 0 };
			std::uint32_t ext{ 0 };
			std::uint32_t dir{ 0 };

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;
			[[nodiscard]] friend auto operator<=>(const hash&, const hash&) noexcept
				-> std::strong_ordering = default;

			friend auto operator>>(
				detail::istream_t& a_in,
				hash& a_hash) noexcept
				-> detail::istream_t&
			{
				return a_in >>
				       a_hash.file >>
				       a_hash.ext >>
				       a_hash.dir;
			}

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&
			{
				return a_out
				       << a_hash.file
				       << a_hash.ext
				       << a_hash.dir;
			}
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class chunk final :
		public detail::components::compressed_byte_container
	{
	public:
		struct mips_t final
		{
			std::uint16_t first{ 0 };
			std::uint16_t last{ 0 };
		};

		mips_t mips;

	private:
		friend file;

		void read(
			detail::istream_t& a_in,
			format a_format) noexcept
		{
			std::uint64_t dataFileOffset = 0;
			std::uint32_t compressedSize = 0;
			std::uint32_t decompressedSize = 0;
			a_in >>
				dataFileOffset >>
				compressedSize >>
				decompressedSize;

			std::size_t size = 0;
			std::optional<std::size_t> decompsz;
			if (compressedSize != 0) {
				decompsz = decompressedSize;
				size = compressedSize;
			} else {
				size = decompressedSize;
			}

			if (a_format == format::directx) {
				a_in >>
					mips.first >>
					mips.last;
			}

			std::uint32_t sentinel = 0;
			a_in >> sentinel;
			assert(sentinel == 0xBAADF00D);

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(dataFileOffset);
			set_data(
				a_in.read_bytes(size),
				a_in,
				decompsz);
		}
	};

	class file final
	{
	private:
		using container_type = boost::container::small_vector<chunk, 1>;

	public:
		struct header_t final
		{
			std::uint16_t height{ 0 };
			std::uint16_t width{ 0 };
			std::uint8_t mipCount{ 0 };
			std::uint8_t format{ 0 };
			std::uint8_t flags{ 0 };
			std::uint8_t tileMode{ 0 };
		};

		using value_type = container_type::value_type;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using key = detail::key_t<hashing::hash, hashing::hash_file>;

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = delete;
		file& operator=(file&&) noexcept = delete;

		[[nodiscard]] auto begin() noexcept -> iterator { return _chunks.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _chunks.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _chunks.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _chunks.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _chunks.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _chunks.cend(); }

		void clear() noexcept
		{
			_chunks.clear();
			header = header_t{};
		}

		[[nodiscard]] bool empty() const noexcept { return _chunks.empty(); }
		[[nodiscard]] auto size() const noexcept -> std::size_t { return _chunks.size(); }

		header_t header;

	private:
		friend archive;

		void read_chunk(
			detail::istream_t& a_in,
			format a_format) noexcept
		{
			std::uint8_t count = 0;

			a_in.seek_relative(1u);  // skip mod index
			a_in >> count;
			a_in.seek_relative(2u);  // skip unknown

			if (a_format == format::directx) {
				a_in >>
					header.height >>
					header.width >>
					header.mipCount >>
					header.format >>
					header.flags >>
					header.tileMode;
			}

			_chunks.reserve(count);
			for (std::size_t i = 0; i < count; ++i) {
				auto& chunk = _chunks.emplace_back();
				chunk.read(a_in, a_format);
			}
		}

		container_type _chunks;
	};

	class archive final :
		public detail::components::hashmap<file>
	{
	private:
		using super = detail::components::hashmap<file>;

	public:
		using super::clear;

		[[nodiscard]] auto read(std::filesystem::path a_path) noexcept
			-> std::optional<format>
		{
			detail::istream_t in{ std::move(a_path) };
			if (!in.is_open()) {
				return std::nullopt;
			}

			const auto header = [&]() {
				detail::header_t header;
				in >> header;
				return header;
			}();
			if (!header.good()) {
				return std::nullopt;
			}

			clear();
			const auto fmt = static_cast<format>(header.archive_format());

			for (std::size_t i = 0; i < header.file_count(); ++i) {
				hashing::hash hash;
				in >> hash;

				const auto name = [&]() {
					const detail::restore_point _{ in };
					in.seek_absolute(header.string_table_offset());

					std::uint16_t len = 0;
					in >> len;
					return std::string_view{
						reinterpret_cast<const char*>(in.read_bytes(len).data()),
						len
					};
				}();

				[[maybe_unused]] const auto [it, success] =
					this->emplace(
						std::piecewise_construct,
						std::forward_as_tuple(hash, name, in),
						std::forward_as_tuple());
				assert(success);

				it->second.read_chunk(in, fmt);
			}

			return { fmt };
		}

		[[nodiscard]] bool write(
			std::filesystem::path a_path,
			format a_format) noexcept;
	};
}
