#include "bsa/detail/common.hpp"

#include <cassert>
#include <exception>

#pragma warning(push)
#include <boost/filesystem/path.hpp>
#include <boost/nowide/cstdio.hpp>
#pragma warning(pop)

namespace bsa::detail
{
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
