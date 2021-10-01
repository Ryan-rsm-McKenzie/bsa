#pragma once

#include <cstdio>

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
			if (std::fread(
					a_dst.data(),
					1,
					a_dst.size_bytes(),
					stdin) != a_dst.size_bytes()) {
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
			if (std::fwrite(
					a_src.data(),
					1,
					a_src.size_bytes(),
					stdout) != a_src.size_bytes()) {
				throw binary_io::buffer_exhausted();
			}
		}
	};
}
