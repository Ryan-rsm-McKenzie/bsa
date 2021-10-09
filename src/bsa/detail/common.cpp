#include "bsa/detail/common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include <lz4frame.h>
#include <zlib.h>

#ifdef BSA_SUPPORT_XMEM
#	include "bsa/xmem/xmem.hpp"
#endif

namespace bsa
{
	namespace
	{
		[[nodiscard]] auto guess_file_format(detail::istream_t& a_in)
			-> std::optional<file_format>
		{
			switch (std::get<0>(a_in->read<std::uint32_t>())) {
			case 0x100:
				return file_format::tes3;
			case make_four_cc("BSA"sv):
				return file_format::tes4;
			case make_four_cc("BTDX"sv):
				return file_format::fo4;
			default:
				return std::nullopt;
			}
		}
	}

	auto guess_file_format(std::filesystem::path a_path)
		-> std::optional<file_format>
	{
		detail::istream_t in{ std::move(a_path) };
		return guess_file_format(in);
	}

	auto guess_file_format(std::span<const std::byte> a_src)
		-> std::optional<file_format>
	{
		detail::istream_t in{ a_src, copy_type::shallow };
		return guess_file_format(in);
	}
}

namespace bsa::detail
{
	namespace
	{
		[[nodiscard]] char mapchar(char a_ch) noexcept
		{
			constexpr auto lut = []() noexcept {
				std::array<char, (std::numeric_limits<unsigned char>::max)() + 1> map{};
				for (std::size_t i = 0; i < map.size(); ++i) {
					map[i] = static_cast<char>(i);
				}

				map[static_cast<std::size_t>('/')] = '\\';

				constexpr auto offset = char{ 'a' - 'A' };
				for (std::size_t i = 'A'; i <= 'Z'; ++i) {
					map[i] = static_cast<char>(i) + offset;
				}

				return map;
			}();

			return lut[static_cast<unsigned char>(a_ch)];
		}
	}

	void normalize_path(std::string& a_path) noexcept
	{
		for (auto& c : a_path) {
			c = mapchar(c);
		}

		while (!a_path.empty() && a_path.back() == '\\') {
			a_path.pop_back();
		}

		while (!a_path.empty() && a_path.front() == '\\') {
			a_path.erase(a_path.begin());
		}

		if (a_path.empty() || a_path.size() >= 260) {
			a_path = '.';
		}
	}

	auto read_bstring(detail::istream_t& a_in)
		-> std::string_view
	{
		const auto [len] = a_in->read<std::uint8_t>();
		return {
			reinterpret_cast<const char*>(a_in->read_bytes(len).data()),
			len
		};
	}

	auto read_bzstring(detail::istream_t& a_in)
		-> std::string_view
	{
		const auto [len] = a_in->read<std::uint8_t>();
		return {
			reinterpret_cast<const char*>(a_in->read_bytes(len).data()),
			len - 1u  // skip null terminator
		};
	}

	auto read_wstring(detail::istream_t& a_in)
		-> std::string_view
	{
		const auto [len] = a_in->read<std::uint16_t>();
		return {
			reinterpret_cast<const char*>(a_in->read_bytes(len).data()),
			len
		};
	}

	auto read_zstring(detail::istream_t& a_in)
		-> std::string_view
	{
		const std::string_view result{
			reinterpret_cast<const char*>(a_in->read_bytes(1u).data())
		};
		a_in->seek_relative(result.length());  // include null terminator
		return result;
	}

	void write_bzstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out.write(static_cast<std::uint8_t>(a_string.length() + 1u));  // include null terminator
		write_zstring(a_out, a_string);
	}

	void write_wstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out.write(static_cast<std::uint16_t>(a_string.length()));
		a_out.write_bytes({ //
			reinterpret_cast<const std::byte*>(a_string.data()),
			a_string.length() });
	}

	void write_zstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out.write_bytes({ //
			reinterpret_cast<const std::byte*>(a_string.data()),
			a_string.length() });
		a_out.write(std::byte{ '\0' });
	}

	istream_t::istream_t(std::filesystem::path a_path) :
		_file(std::make_shared<file_type>(std::move(a_path))),
		_stream({ _file->data(), _file->size() }),
		_copy(copy_type::shallow)
	{
		_stream.endian(std::endian::little);
	}

	istream_t::istream_t(std::span<const std::byte> a_bytes, copy_type a_copy) noexcept :
		_stream(a_bytes),
		_copy(a_copy)
	{
		_stream.endian(std::endian::little);
	}
}

namespace bsa
{
	compression_error::compression_error(
		library a_library,
		std::size_t a_code) noexcept :
		_lib(a_library)
	{
		switch (_lib) {
		case library::internal:
			_what = detail::to_string(static_cast<detail::error_code>(a_code));
			break;
		case library::zlib:
			_what = ::zError(static_cast<int>(a_code));
			break;
		case library::lz4:
			_what = ::LZ4F_getErrorName(static_cast<::LZ4F_errorCode_t>(a_code));
			break;
		case library::xmem:
#ifdef BSA_SUPPORT_XMEM
			_what = xmem::to_string(static_cast<xmem::error_code>(a_code));
#else
			_what = "undecoded xmem error"sv;
#endif
			break;
		default:
			detail::declare_unreachable();
		}
	}
}

namespace bsa::components
{
	auto basic_byte_container::as_bytes() const noexcept
		-> std::span<const std::byte>
	{
		switch (_data.index()) {
		case data_view:
			return *std::get_if<data_view>(&_data);
		case data_owner:
			{
				const auto& owner = *std::get_if<data_owner>(&_data);
				return {
					owner.data(),
					owner.size()
				};
			}
		case data_proxied:
			return std::get_if<data_proxied>(&_data)->d;
		default:
			detail::declare_unreachable();
		}
	}
}
