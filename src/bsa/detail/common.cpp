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
#include <utility>

#if BSA_OS_WINDOWS
#	define WIN32_LEAN_AND_MEAN

#	define NOGDICAPMASKS
#	define NOVIRTUALKEYCODES
#	define NOWINMESSAGES
#	define NOWINSTYLES
#	define NOSYSMETRICS
#	define NOMENUS
#	define NOICONS
#	define NOKEYSTATES
#	define NOSYSCOMMANDS
#	define NORASTEROPS
#	define NOSHOWWINDOW
#	define OEMRESOURCE
#	define NOATOM
#	define NOCLIPBOARD
#	define NOCOLOR
#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOGDI
#	define NOKERNEL
#	define NOUSER
#	define NONLS
#	define NOMB
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
#	define NOMSG
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
#	define NOWH
#	define NOWINOFFSETS
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX

#	include <Windows.h>
#else
#	include <sys/mman.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <sys/stat.h>
#endif

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

			(void)::_wfopen_s(&result, a_path.c_str(), mode.c_str());
			return result;
#else
			return std::fopen(a_path.c_str(), a_mode);
#endif
		}
	}

	namespace mmio
	{
		file::file() noexcept
		{
#if BSA_OS_WINDOWS
			this->_file = INVALID_HANDLE_VALUE;
#else
			this->_view = MAP_FAILED;
#endif
		}

		void file::close() noexcept
		{
#if BSA_OS_WINDOWS
			if (this->_view != nullptr) {
				[[maybe_unused]] const auto success = ::UnmapViewOfFile(this->_view);
				assert(success != 0);
				this->_view = nullptr;
			}

			if (this->_mapping != nullptr) {
				[[maybe_unused]] const auto success = ::CloseHandle(this->_mapping);
				assert(success != 0);
				this->_mapping = nullptr;
			}

			if (this->_file != INVALID_HANDLE_VALUE) {
				[[maybe_unused]] const auto success = ::CloseHandle(this->_file);
				assert(success != 0);
				this->_file = INVALID_HANDLE_VALUE;
				this->_size = 0;
			}
#else
			if (this->_view != MAP_FAILED) {
				[[maybe_unused]] const auto success = ::munmap(this->_view);
				assert(success == 0);
				this->_view = MAP_FAILED;
			}

			if (this->_file != -1) {
				[[maybe_unused]] const auto success = ::close(this->_file);
				assert(success == 0);
				this->_file = -1;
				this->_size = 0;
			}
#endif
		}

		bool file::is_open() const noexcept
		{
#if BSA_OS_WINDOWS
			return this->_view != nullptr;
#else
			return this->_view != MAP_FAILED;
#endif
		}

		bool file::open(std::filesystem::path a_path) noexcept
		{
			this->close();
			if (this->do_open(a_path.c_str())) {
				return true;
			} else {
				this->close();
				return false;
			}
		}

		void file::do_move(file&& a_rhs) noexcept
		{
#if BSA_OS_WINDOWS
			this->_file = std::exchange(a_rhs._file, INVALID_HANDLE_VALUE);
			this->_mapping = std::exchange(a_rhs._mapping, nullptr);
			this->_view = std::exchange(a_rhs._view, nullptr);
#else
			this->_file = std::exchange(a_rhs._file, -1);
			this->_view = std::exchange(a_rhs._view, MAP_FAILED);
#endif
			this->_size = std::exchange(a_rhs._size, 0);
		}

#if BSA_OS_WINDOWS
		bool file::do_open(const wchar_t* a_path) noexcept
		{
			this->_file = ::CreateFileW(
				a_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_READONLY,
				nullptr);
			if (this->_file == INVALID_HANDLE_VALUE) {
				return false;
			}

			::LARGE_INTEGER size = {};
			if (::GetFileSizeEx(this->_file, &size) == 0) {
				return false;
			}
			this->_size = static_cast<std::size_t>(size.QuadPart);

			this->_mapping = ::CreateFileMappingW(
				this->_file,
				nullptr,
				PAGE_READONLY,
				0,
				0,
				nullptr);
			if (this->_mapping == nullptr) {
				return false;
			}

			this->_view = ::MapViewOfFile(
				this->_mapping,
				FILE_MAP_READ,
				0,
				0,
				0);
			if (this->_view == nullptr) {
				return false;
			}

			return true;
		}
#else
		bool file::do_open(const char* a_path) noexcept
		{
			this->_file = ::open(
				a_path,
				O_RDONLY);
			if (this->_file == -1) {
				return false;
			}

			::stat s = {};
			if (::fstat(this->_file, &s) == -1) {
				return false;
			}
			this->_size = static_cast<std::size_t>(s.st_size);

			this->_view = ::mmap(
				nullptr,
				this->_size,
				PROT_READ,
				MAP_SHARED,
				this->_file,
				0);
			if (this->_view == MAP_FAILED) {
				return false;
			}

			return true;
		}
#endif
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
		_file = std::make_shared<stream_type>();
		_file->open(std::move(a_path));
		if (!_file->is_open()) {
			throw std::system_error{
				std::error_code{ errno, std::generic_category() },
				"failed to open file"
			};
		}
	}

	auto istream_t::read_bytes(std::size_t a_bytes)
		-> std::span<const std::byte>
	{
		if (_pos + a_bytes > _file->size()) {
			throw exception("file read out of range");
		}

		const auto pos = _pos;
		_pos += a_bytes;
		return {
			reinterpret_cast<const std::byte*>(_file->data()) + pos,
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
