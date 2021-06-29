#pragma once

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/container/set.hpp>
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
	}

	class chunk final
	{
	public:
		chunk() noexcept = default;
		chunk(const chunk&) noexcept = default;
		chunk(chunk&&) noexcept = default;
		~chunk() noexcept = default;
		chunk& operator=(const chunk&) noexcept = default;
		chunk& operator=(chunk&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;
		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }
		[[nodiscard]] auto data() const noexcept -> const std::byte* { return as_bytes().data(); }
		[[nodiscard]] auto size() const noexcept -> std::size_t { return as_bytes().size(); }

	protected:
		friend class file;

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
			if (compressedSize != 0) {
				_decompsz = decompressedSize;
				size = compressedSize;
			} else {
				size = decompressedSize;
			}

			if (a_format == format::directx) {
				auto& mip = _mip.emplace(mip_t{});
				a_in >>
					mip.first >>
					mip.last;
			}

			std::uint32_t sentinel = 0;
			a_in >> sentinel;
			assert(sentinel == 0xBAADF00D);

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(dataFileOffset);
			_data.emplace<data_proxied>(
				a_in.read_bytes(size),
				a_in.rdbuf());
		}

	private:
		struct mip_t final
		{
			std::uint16_t first{ 0 };
			std::uint16_t last{ 0 };
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		using data_proxy = detail::istream_proxy<std::span<const std::byte>>;

		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;
		std::optional<std::size_t> _decompsz;
		std::optional<mip_t> _mip;

		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class file final
	{
	private:
		using container_type = boost::container::small_vector<chunk, 1>;

	public:
		using value_type = container_type::value_type;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		explicit file(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		template <detail::concepts::stringable String>
		explicit file(String&& a_path) noexcept;

		file(const file&) noexcept = default;
		file(file&& a_rhs) noexcept :
			_hash(a_rhs._hash),
			_name(a_rhs._name),
			_chunks(std::move(a_rhs._chunks)),
			_dx10(a_rhs._dx10)
		{
			a_rhs.clear();
		}

		~file() noexcept = default;

		file& operator=(const file&) noexcept = default;
		file& operator=(file&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_hash = a_rhs._hash;
				_name = a_rhs._name;
				_chunks = std::move(a_rhs._chunks);
				_dx10 = a_rhs._dx10;

				a_rhs.clear();
			}
			return *this;
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _chunks.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _chunks.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _chunks.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _chunks.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _chunks.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _chunks.cend(); }

		void clear() noexcept
		{
			_chunks.clear();
			_dx10 = dx10_t{};
		}

		[[nodiscard]] bool empty() const noexcept { return _chunks.empty(); }
		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }
		[[nodiscard]] auto size() const noexcept -> std::size_t { return _chunks.size(); }

	protected:
		friend class archive;

		void read_chunk(
			detail::istream_t& a_in,
			format a_format) noexcept
		{
			std::uint8_t count = 0;

			a_in.seek_relative(1u);  // skip mod index
			a_in >> count;
			a_in.seek_relative(2u);  // skip unknown

			if (a_format == format::directx) {
				auto& dx = _dx10.emplace(dx10_t{});
				a_in >>
					dx.height >>
					dx.width >>
					dx.mipCount >>
					dx.format >>
					dx.flags >>
					dx.tileMode;
			}

			_chunks.reserve(count);
			for (std::size_t i = 0; i < count; ++i) {
				auto& chunk = _chunks.emplace_back();
				chunk.read(a_in, a_format);
			}
		}

		void read_name(detail::istream_t& a_in) noexcept
		{
			std::uint16_t len = 0;
			a_in >> len;
			std::string_view name{
				reinterpret_cast<const char*>(a_in.read_bytes(len).data()),
				len
			};
			_name.emplace<name_proxied>(name, a_in.rdbuf());
		}

	private:
		struct dx10_t final
		{
			std::uint16_t height{ 0 };
			std::uint16_t width{ 0 };
			std::uint8_t mipCount{ 0 };
			std::uint8_t format{ 0 };
			std::uint8_t flags{ 0 };
			std::uint8_t tileMode{ 0 };
		};

		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied,

			name_count
		};

		using name_proxy = detail::istream_proxy<std::string_view>;

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::string,
			name_proxy>
			_name;
		container_type _chunks;
		std::optional<dx10_t> _dx10;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
	};

	class archive final
	{
	public:
		using key_type = file;
		using key_compare = detail::key_compare_t<key_type, hashing::hash>;

	private:
		using container_type =
			boost::container::set<key_type, key_compare>;

	public:
		using value_type = container_type::value_type;
		using value_compare = container_type::value_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		archive() noexcept = default;
		archive(const archive&) noexcept = default;
		archive(archive&&) noexcept = default;
		~archive() noexcept = default;
		archive& operator=(const archive&) noexcept = default;
		archive& operator=(archive&&) noexcept = default;

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		void clear() noexcept { _files.clear(); }

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
				hashing::hash h;
				in >> h;
				[[maybe_unused]] const auto [it, success] = _files.emplace(h);
				assert(success);

				it->read_chunk(in, fmt);
			}

			in.seek_absolute(header.string_table_offset());
			for (auto& file : _files) {
				file.read_name(in);
			}

			return { fmt };
		}

		[[nodiscard]] bool write(
			std::filesystem::path a_path,
			format a_format) noexcept;

	private:
		container_type _files;
	};
}
