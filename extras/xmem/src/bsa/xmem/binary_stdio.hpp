#pragma once

#include <memory>

#include <Windows.h>

#include <binary_io/common.hpp>

namespace bsa::xmem::binary_stdio
{
	class bin :
		public binary_io::istream_interface<bin>
	{
	public:
		void seek_absolute(binary_io::streamoff) noexcept { return; }
		void seek_relative(binary_io::streamoff) noexcept { return; }
		binary_io::streamoff tell() const noexcept { return -1; }

		void read_bytes(std::span<std::byte> a_dst)
		{
			::BOOL success = TRUE;
			::DWORD read = 0;
			auto pos = std::to_address(a_dst.begin());
			const auto end = std::to_address(a_dst.end());
			const auto handle = ::GetStdHandle(STD_INPUT_HANDLE);

			do {
				success = ::ReadFile(
					handle,
					pos,
					static_cast<::DWORD>(end - pos),
					&read,
					nullptr);
				pos += read;
			} while (success && read != 0 && pos != end);

			if (pos != end) {
				throw binary_io::buffer_exhausted();
			}
		}
	};

	class bout :
		public binary_io::ostream_interface<bout>
	{
	public:
		void seek_absolute(binary_io::streamoff) noexcept { return; }
		void seek_relative(binary_io::streamoff) noexcept { return; }
		binary_io::streamoff tell() const noexcept { return -1; }

		void write_bytes(std::span<const std::byte> a_src)
		{
			::BOOL success = TRUE;
			::DWORD written = 0;
			auto pos = std::to_address(a_src.begin());
			const auto end = std::to_address(a_src.end());
			const auto handle = ::GetStdHandle(STD_OUTPUT_HANDLE);

			do {
				success = ::WriteFile(
					handle,
					pos,
					static_cast<::DWORD>(end - pos),
					&written,
					nullptr);
				pos += written;
			} while (success && written != 0 && pos != end);

			if (pos != end) {
				throw binary_io::buffer_exhausted();
			}
		}
	};
}
