#include "bsa/detail/common.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <system_error>

#include <boost/filesystem/path.hpp>

namespace bsa::detail
{
	namespace
	{
		[[nodiscard]] char mapchar(char a_ch) noexcept
		{
			constexpr auto lut = []() noexcept {
				std::array<char, std::numeric_limits<unsigned char>::max() + 1> map{};
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

	namespace unicode
	{
		auto fopen(std::filesystem::path a_path, const char* a_mode)
			-> std::FILE*
		{
#if BSA_OS_WINDOWS
			std::FILE* result = nullptr;

			// a_mode is basic ASCII which means it's valid utf-16 with a simple cast
			std::wstring mode;
			mode.resize(std::strlen(a_mode));
			std::copy(a_mode, a_mode + mode.size(), mode.begin());

			(void)::_wfopen_s(
				&result,
				a_path.native().c_str(),
				mode.c_str());

			return result;
#else
			return std::fopen(
				reinterpret_cast<const char*>(a_path.u8string().c_str()),
				a_mode);
#endif
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
		std::uint8_t len = 0;
		a_in >> len;
		return {
			reinterpret_cast<const char*>(a_in.read_bytes(len).data()),
			len
		};
	}

	auto read_bzstring(detail::istream_t& a_in)
		-> std::string_view
	{
		std::uint8_t len = 0;
		a_in >> len;
		return {
			reinterpret_cast<const char*>(a_in.read_bytes(len).data()),
			len - 1u  // skip null terminator
		};
	}

	auto read_wstring(detail::istream_t& a_in)
		-> std::string_view
	{
		std::uint16_t len = 0;
		a_in >> len;
		return {
			reinterpret_cast<const char*>(a_in.read_bytes(len).data()),
			len
		};
	}

	auto read_zstring(detail::istream_t& a_in)
		-> std::string_view
	{
		const std::string_view result{
			reinterpret_cast<const char*>(a_in.read_bytes(1u).data())
		};
		a_in.seek_relative(result.length());  // include null terminator
		return result;
	}

	void write_bzstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out << static_cast<std::uint8_t>(a_string.length() + 1u);  // include null terminator
		write_zstring(a_out, a_string);
	}

	void write_wstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out << static_cast<std::uint16_t>(a_string.length());
		a_out.write_bytes({ //
			reinterpret_cast<const std::byte*>(a_string.data()),
			a_string.length() });
	}

	void write_zstring(detail::ostream_t& a_out, std::string_view a_string) noexcept
	{
		a_out.write_bytes({ //
			reinterpret_cast<const std::byte*>(a_string.data()),
			a_string.length() });
		a_out << std::byte{ '\0' };
	}

	istream_t::istream_t(std::filesystem::path a_path)
	{
		_file.open(boost::filesystem::path{ a_path.native() });
		if (!_file.is_open()) {  // boost should throw an exception on failure
			throw std::system_error{
				std::error_code{ errno, std::generic_category() },
				"failed to open file"
			};
		}
	}

	auto istream_t::read_bytes(std::size_t a_bytes)
		-> std::span<const std::byte>
	{
		if (_pos + a_bytes > _file.size()) {
			throw exception("file read out of range");
		}

		const auto pos = _pos;
		_pos += a_bytes;
		return {
			reinterpret_cast<const std::byte*>(_file.data()) + pos,
			a_bytes
		};
	}

	ostream_t::ostream_t(std::filesystem::path a_path)
	{
		_file = unicode::fopen(
			reinterpret_cast<const char*>(a_path.u8string().data()),
			"wb");
		if (_file == nullptr) {
			throw std::system_error{
				std::error_code{ errno, std::generic_category() },
				"failed to open file"
			};
		}
	}

	ostream_t::~ostream_t() noexcept
	{
		if (_file) {
			[[maybe_unused]] const auto result = std::fclose(_file);
			assert(result == 0);
			_file = nullptr;
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
