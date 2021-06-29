#include "bsa/detail/common.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <exception>
#include <limits>
#include <string>

#include <boost/filesystem/path.hpp>
#include <boost/nowide/cstdio.hpp>

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

	void normalize_directory(std::string& a_path) noexcept
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

	istream_t::istream_t(std::filesystem::path a_path) noexcept
	{
		try {
			_file.open(boost::filesystem::path{ a_path.native() });
		} catch (const std::exception&) {}
	}

	auto istream_t::read_bytes(std::size_t a_bytes) noexcept
		-> std::span<const std::byte>
	{
		assert(_pos + a_bytes <= _file.size());

		const auto pos = _pos;
		_pos += a_bytes;
		return {
			reinterpret_cast<const std::byte*>(_file.data()) + pos,
			a_bytes
		};
	}

	ostream_t::ostream_t(std::filesystem::path a_path) noexcept
	{
		_file = boost::nowide::fopen(
			reinterpret_cast<const char*>(a_path.u8string().data()),
			"wb");
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
