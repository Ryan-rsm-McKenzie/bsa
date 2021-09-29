#pragma once

#include <cstdint>
#include <variant>

#include <Windows.h>

#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem::xcompress
{
	namespace detail
	{
		struct compression_context;
		struct decompression_context;
	}

	using compression_context = detail::compression_context*;
	using decompression_context = detail::decompression_context*;

	enum class codec_type : std::int32_t
	{
		codec_default,
		codec_lzx
	};

	enum class flags : std::uint32_t
	{
		none = 0,
		compress_stream = 1u << 0
	};

	::HRESULT WINAPI create_compression_context(
		xcompress::codec_type a_codecType,
		const void* a_codecParams,
		xcompress::flags a_flags,
		xcompress::compression_context* a_context) noexcept;

	::HRESULT compress(
		xcompress::compression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t a_srcSize) noexcept;

	::HRESULT WINAPI compress_stream(
		xcompress::compression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t* a_srcSize) noexcept;

	void WINAPI destroy_compression_context(
		xcompress::compression_context a_context) noexcept;

	::HRESULT WINAPI create_decompression_context(
		xcompress::codec_type a_codecType,
		const void* a_codecParams,
		xcompress::flags a_flags,
		xcompress::decompression_context* a_context) noexcept;

	::HRESULT WINAPI decompress_stream(
		xcompress::decompression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t* a_srcSize) noexcept;

	void WINAPI destroy_decompression_context(
		xcompress::decompression_context a_context) noexcept;

	[[nodiscard]] auto initialize() noexcept -> xmem::expected<std::monostate>;
}
