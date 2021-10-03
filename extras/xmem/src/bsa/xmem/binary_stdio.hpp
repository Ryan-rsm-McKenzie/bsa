#pragma once

#include <cstdio>
#include <memory>

#include <binary_io/common.hpp>

namespace bsa::xmem::binary_stdio
{
	class bin :
		public binary_io::istream_interface<bin>
	{
	public:
		void seek_absolute(binary_io::streamoff a_pos) noexcept
		{
			std::fseek(stdout, static_cast<long>(a_pos), SEEK_SET);
		}

		void seek_relative(binary_io::streamoff a_pos) noexcept
		{
			std::fseek(stdout, static_cast<long>(a_pos), SEEK_CUR);
		}

		binary_io::streamoff tell() const noexcept
		{
			return std::ftell(stdout);
		}

		void read_bytes(std::span<std::byte> a_dst)
		{
			std::size_t read = 0;
			auto pos = std::to_address(a_dst.begin());
			const auto end = std::to_address(a_dst.end());

			do {
				read = std::fread(
					pos,
					1,
					static_cast<std::size_t>(end - pos),
					stdin);
				pos += read;
			} while (read != 0 && pos != end);

			if (pos != end) {
				throw binary_io::buffer_exhausted();
			}
		}
	};

	class bout :
		public binary_io::ostream_interface<bout>
	{
	public:
		void seek_absolute(binary_io::streamoff a_pos) noexcept
		{
			std::fseek(stdout, static_cast<long>(a_pos), SEEK_SET);
		}

		void seek_relative(binary_io::streamoff a_pos) noexcept
		{
			std::fseek(stdout, static_cast<long>(a_pos), SEEK_CUR);
		}

		binary_io::streamoff tell() const noexcept
		{
			return std::ftell(stdout);
		}

		void write_bytes(std::span<const std::byte> a_src)
		{
			std::size_t wrote = 0;
			auto pos = std::to_address(a_src.begin());
			const auto end = std::to_address(a_src.end());

			do {
				wrote = std::fwrite(
					pos,
					1,
					static_cast<std::size_t>(end - pos),
					stdout);
				pos += wrote;
			} while (wrote != 0 && pos != end);

			if (pos != end) {
				throw binary_io::buffer_exhausted();
			}
		}
	};
}
