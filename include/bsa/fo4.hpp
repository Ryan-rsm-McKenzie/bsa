#pragma once

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

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
			inline constexpr auto gnrl = detail::make_file_type({ 'G', 'N', 'R', 'L' });
			inline constexpr auto dx10 = detail::make_file_type({ 'D', 'X', '1', '0' });
		}
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
				hash& a_hash)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&;
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class chunk final :
		public detail::components::compressed_byte_container
	{
	private:
		friend archive;
		using super = detail::components::compressed_byte_container;

	public:
		struct mips_t final
		{
			std::uint16_t first{ 0 };
			std::uint16_t last{ 0 };

			friend auto operator>>(
				detail::istream_t& a_in,
				mips_t& a_mips)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const mips_t& a_mips) noexcept
				-> detail::ostream_t&;
		} mips;

		void clear() noexcept
		{
			super::clear();
			this->mips = mips_t{};
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

			friend auto operator>>(
				detail::istream_t& a_in,
				header_t& a_header)
				-> detail::istream_t&;

			friend auto operator<<(
				detail::ostream_t& a_out,
				const header_t& a_header) noexcept
				-> detail::ostream_t&;
		} header;

		using value_type = container_type::value_type;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using key = detail::key_t<hashing::hash, hashing::hash_file>;

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto operator[](std::size_t a_pos) noexcept
			-> value_type& { return _chunks[a_pos]; }
		[[nodiscard]] auto operator[](std::size_t a_pos) const noexcept
			-> const value_type& { return _chunks[a_pos]; }

		[[nodiscard]] auto begin() noexcept -> iterator { return _chunks.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _chunks.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _chunks.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _chunks.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _chunks.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _chunks.cend(); }

		[[nodiscard]] auto back() noexcept -> value_type& { return _chunks.back(); }
		[[nodiscard]] auto back() const noexcept -> const value_type& { return _chunks.back(); }

		[[nodiscard]] auto capacity() const noexcept -> std::size_t { return _chunks.capacity(); }

		void clear() noexcept
		{
			_chunks.clear();
			this->header = header_t{};
		}

		template <class... Args>
		value_type& emplace_back(Args&&... a_args) noexcept
		{
			return _chunks.emplace_back(std::forward<Args>(a_args)...);
		}

		[[nodiscard]] bool empty() const noexcept { return _chunks.empty(); }
		[[nodiscard]] auto front() noexcept -> value_type& { return _chunks.front(); }
		[[nodiscard]] auto front() const noexcept -> const value_type& { return _chunks.front(); }
		void pop_back() noexcept { _chunks.pop_back(); }
		void push_back(value_type a_value) noexcept { _chunks.push_back(std::move(a_value)); }
		void reserve(std::size_t a_count) noexcept { _chunks.reserve(a_count); }
		void shrink_to_fit() noexcept { _chunks.shrink_to_fit(); }
		[[nodiscard]] auto size() const noexcept -> std::size_t { return _chunks.size(); }

	private:
		container_type _chunks;
	};

	class archive final :
		public detail::components::hashmap<file>
	{
	private:
		using super = detail::components::hashmap<file>;

	public:
		using super::clear;

		auto read(std::filesystem::path a_path) -> format;
		void write(std::filesystem::path a_path, format a_format);

	private:
		[[nodiscard]] auto make_header(format a_format) const noexcept
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
			std::uint64_t& a_dataOffset) noexcept;

		void write_file(
			const file& a_file,
			detail::ostream_t& a_out,
			format a_format,
			std::uint64_t& a_dataOffset) noexcept;
	};
}
