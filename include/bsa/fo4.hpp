#pragma once

#include <array>
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

#include <boost/container/flat_set.hpp>

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
	}

	enum class archive_type : std::uint32_t
	{
		general = detail::make_file_type({ 'G', 'N', 'R', 'L' }),
		directx = detail::make_file_type({ 'D', 'X', '1', '0' }),
	};

	namespace hashing
	{
		struct hash final
		{
			std::uint32_t file{ 0 };
			std::array<char, 4> ext{ '\0', '\0', '\0', '\0' };
			std::uint32_t dir{ 0 };

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;
			[[nodiscard]] friend auto operator<=>(const hash&, const hash&) noexcept
				-> std::strong_ordering = default;
		};
	}

	class file final
	{
	public:
		explicit file(hashing::hash a_hash) noexcept;

		template <detail::concepts::stringable String>
		explicit file(String&& a_path) noexcept;

		file(const file&) noexcept = default;
		file(file&& a_rhs) noexcept;

		~file() noexcept = default;

		file& operator=(const file&) noexcept = default;
		file& operator=(file&& a_rhs) noexcept;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;
		[[nodiscard]] auto data() const noexcept -> const std::byte*;
		[[nodiscard]] auto hash() const noexcept -> const hashing::hash&;
		[[nodiscard]] auto size() const noexcept -> std::size_t;

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
		std::optional<dx10_t> _dx10;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class archive final
	{
	public:
		using key_type = file;
		using key_compare = detail::key_compare_t<key_type, hashing::hash>;

	private:
		using container_type =
			boost::container::flat_set<key_type, key_compare>;

	public:
		using value_type = container_type::value_type;
		using value_compare = container_type::value_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		[[nodiscard]] auto read(std::filesystem::path a_path) noexcept -> std::optional<archive_type>;
		[[nodiscard]] bool write(std::filesystem::path a_path, archive_type a_type) noexcept;

	private:
		container_type _files;
	};
}
