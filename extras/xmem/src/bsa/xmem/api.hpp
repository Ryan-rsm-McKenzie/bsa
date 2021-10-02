#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include <Windows.h>

#include "bsa/xmem/expected.hpp"
#include "bsa/xmem/winapi.hpp"
#include "bsa/xmem/xcompress.hpp"
#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem::api
{
	template <
		class T,
		auto Destroyer>
	class context_wrapper
	{
	public:
		using value_type = T;

		context_wrapper(const volatile context_wrapper&) = delete;
		context_wrapper(context_wrapper&& a_rhs) noexcept :
			_context(std::exchange(a_rhs._context, nullptr))
		{}

		explicit context_wrapper(value_type a_context) noexcept :
			_context(a_context)
		{}

		~context_wrapper() noexcept
		{
			if (_context) {
				Destroyer(std::exchange(_context, nullptr));
			}
		}

		context_wrapper& operator=(const volatile context_wrapper&) = delete;
		context_wrapper& operator=(context_wrapper&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_context = std::exchange(a_rhs._context, nullptr);
			}
			return *this;
		}

		[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
		[[nodiscard]] value_type get() const noexcept { return _context; }
		[[nodiscard]] bool has_value() const noexcept { return static_cast<bool>(_context); }

	private:
		value_type _context = {};
	};

	using compression_context = context_wrapper<xcompress::compression_context, &xcompress::destroy_compression_context>;
	using decompression_context = context_wrapper<xcompress::decompression_context, &xcompress::destroy_decompression_context>;

	[[nodiscard]] inline auto create_compression_context() noexcept
		-> xmem::expected<api::compression_context>
	{
		xcompress::compression_context compressor = {};
		const auto result = xcompress::create_compression_context(
			xcompress::codec_type::codec_default,
			nullptr,
			xcompress::flags::none,
			&compressor);
		if (winapi::hresult_success(result)) {
			return api::compression_context{ compressor };
		} else {
			return xmem::unexpected(xmem::error_code::api_create_compression_context_failure);
		}
	}

	[[nodiscard]] inline auto create_decompression_context() noexcept
		-> xmem::expected<api::decompression_context>
	{
		xcompress::decompression_context decompressor = {};
		const auto result = xcompress::create_decompression_context(
			xcompress::codec_type::codec_default,
			nullptr,
			xcompress::flags::compress_stream,
			&decompressor);
		if (winapi::hresult_success(result)) {
			return api::decompression_context{ decompressor };
		} else {
			return xmem::unexpected(xmem::error_code::api_create_decompression_context_failure);
		}
	}

	[[nodiscard]] inline auto compress(
		const api::compression_context& a_context,
		std::span<const std::byte> a_in,
		std::span<std::byte> a_out) noexcept
		-> xmem::expected<std::size_t>
	{
		assert(a_context);

		std::uint32_t outsz = a_out.size_bytes();
		const auto result = xcompress::compress(
			a_context.get(),
			a_out.data(),
			&outsz,
			a_in.data(),
			a_in.size_bytes());
		if (winapi::hresult_success(result)) {
			return outsz;
		} else {
			return xmem::unexpected(xmem::error_code::api_compress_failure);
		}
	}

	[[nodiscard]] inline auto compress_bound(
		const api::compression_context& a_context,
		std::span<const std::byte> a_data) noexcept
		-> xmem::expected<std::size_t>
	{
		assert(a_context);

		std::uint32_t outsz = 0;
		const auto result = xcompress::compress(
			a_context.get(),
			nullptr,
			&outsz,
			a_data.data(),
			a_data.size_bytes());
		if (winapi::hresult_success(result) || result == 0x81DE2001) {
			return outsz;
		} else {
			return xmem::unexpected(xmem::error_code::api_compress_bound_failure);
		}
	}

	[[nodiscard]] inline auto decompress(
		const api::decompression_context& a_context,
		std::span<const std::byte> a_in,
		std::span<std::byte> a_out) noexcept
		-> xmem::expected<std::size_t>
	{
		assert(a_context);

		std::uint32_t insz = 0;
		const std::byte* inptr = a_in.data();
		std::uint32_t outsz = 0;
		std::byte* outptr = a_out.data();
		::HRESULT result = S_OK;
		const auto streamOK = [&]() {
			return winapi::hresult_success(result) && outsz != 0;
		};

		do {
			inptr += insz;
			insz = static_cast<std::size_t>(std::to_address(a_in.end()) - inptr);
			outptr += outsz;
			outsz = static_cast<std::size_t>(std::to_address(a_out.end()) - outptr);
			result = xcompress::decompress_stream(
				a_context.get(),
				outptr,
				&outsz,
				inptr,
				&insz);
		} while (streamOK() && inptr != std::to_address(a_in.end()));

		if (winapi::hresult_success(result)) {
			return static_cast<std::size_t>((outptr + outsz) - a_out.data());
		} else {
			return xmem::unexpected(xmem::error_code::api_decompress_failure);
		}
	}
}
