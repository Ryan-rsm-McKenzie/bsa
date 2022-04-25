#pragma once

#include <memory>
#include <system_error>
#include <tuple>

#include <binary_io/common.hpp>
#include <reproc++/reproc.hpp>

namespace bsa::detail
{
	class process_in :
		public binary_io::istream_interface<process_in>
	{
	public:
		process_in(reproc::process& a_proc) :
			_proc(a_proc)
		{}

		void seek_absolute(binary_io::streamoff) noexcept { return; }
		void seek_relative(binary_io::streamoff) noexcept { return; }
		binary_io::streamoff tell() const noexcept { return -1; }

		void read_bytes(std::span<std::byte> a_dst)
		{
			std::size_t read = 0;
			std::error_code error;

			auto pos = reinterpret_cast<std::uint8_t*>(std::to_address(a_dst.begin()));
			const auto end = reinterpret_cast<const std::uint8_t*>(std::to_address(a_dst.end()));

			do {
				_proc.poll(reproc::event::out);
				std::tie(read, error) = _proc.read(
					reproc::stream::out,
					pos,
					static_cast<std::size_t>(end - pos));
				pos += read;
			} while (!error && pos != end);

			if (pos != end || error) {
				throw binary_io::buffer_exhausted();
			}
		}

	private:
		reproc::process& _proc;
	};

	class process_out :
		public binary_io::ostream_interface<process_out>
	{
	public:
		process_out(reproc::process& a_proc) :
			_proc(a_proc)
		{}

		void seek_absolute(binary_io::streamoff) noexcept { return; }
		void seek_relative(binary_io::streamoff) noexcept { return; }
		binary_io::streamoff tell() const noexcept { return -1; }

		void write_bytes(std::span<const std::byte> a_src)
		{
			std::size_t written = 0;
			std::error_code error;

			auto pos = reinterpret_cast<const std::uint8_t*>(std::to_address(a_src.begin()));
			const auto end = reinterpret_cast<const std::uint8_t*>(std::to_address(a_src.end()));

			do {
				_proc.poll(reproc::event::in);
				std::tie(written, error) = _proc.write(pos, static_cast<std::size_t>(end - pos));
				pos += written;
			} while (!error && pos != end);

			if (pos != end || error) {
				throw binary_io::buffer_exhausted();
			}
		}

	private:
		reproc::process& _proc;
	};
}
