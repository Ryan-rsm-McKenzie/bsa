#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <Windows.h>

#include "bsa/xmem/winapi.hpp"
#include "bsa/xmem/xcompress.hpp"
#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem::api
{
	[[nodiscard]] inline auto compress(
		xcompress::compression_context a_context,
		std::span<const std::byte> a_in,
		std::span<std::byte> a_out) noexcept
		-> xmem::expected<std::size_t>
	{
		std::uint32_t outsz = a_out.size_bytes();
		const auto result = xcompress::compress(
			a_context,
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
		xcompress::compression_context a_context,
		std::span<const std::byte> a_data) noexcept
		-> xmem::expected<std::size_t>
	{
		std::uint32_t outsz = 0;
		const auto result = xcompress::compress(
			a_context,
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
		xcompress::decompression_context a_context,
		std::span<const std::byte> a_in,
		std::span<std::byte> a_out) noexcept
		-> xmem::expected<std::size_t>
	{
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
				a_context,
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
