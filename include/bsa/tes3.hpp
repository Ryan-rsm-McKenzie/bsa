#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "bsa/detail/common.hpp"
#include "bsa/fwd.hpp"

namespace bsa::tes3
{
	namespace detail
	{
		using namespace bsa::detail;

		namespace constants
		{
			inline constexpr std::size_t header_size = 0xC;
		}

		class header_t
		{
		public:
			header_t(
				std::uint32_t a_hashOffset,
				std::uint32_t a_fileCount) noexcept :
				_hashOffset(a_hashOffset),
				_fileCount(a_fileCount)
			{}

			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }
			[[nodiscard]] bool good() const noexcept { return _good; }
			[[nodiscard]] auto hash_offset() const noexcept -> std::size_t { return _hashOffset; }

			friend istream_t& operator>>(
				istream_t& a_in,
				header_t& a_header) noexcept
			{
				std::uint32_t magic = 0;
				a_in >>
					magic >>
					a_header._hashOffset >>
					a_header._fileCount;

				if (magic != 0x100) {
					a_header._good = false;
				}

				return a_in;
			}

			friend ostream_t& operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
			{
				a_out
					<< std::uint32_t{ 0x100 }
					<< a_header._hashOffset
					<< a_header._fileCount;
				return a_out;
			}

		private:
			std::uint32_t _hashOffset{ 0 };
			std::uint32_t _fileCount{ 0 };
			bool _good{ true };
		};
	}

	namespace hashing
	{
		struct hash
		{
		public:
			std::uint32_t lo{ 0 };
			std::uint32_t hi{ 0 };
		};
	}

	class file
	{
	public:
	private:
		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied,

			name_count
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		using data_proxy = detail::istream_proxy<std::span<const std::byte>>;
		using name_proxy = detail::istream_proxy<std::string_view>;

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::string,
			name_proxy>
			_name;
		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class archive
	{
	public:
	private:
	};
}
