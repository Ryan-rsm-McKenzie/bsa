#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include <bcrypt.h>

#include "bsa/xmem/util.hpp"
#include "bsa/xmem/winapi.hpp"

namespace bsa::xmem::hashing
{
	[[nodiscard]] std::string sha512(std::span<const std::byte> a_data)
	{
		::BCRYPT_ALG_HANDLE algorithm;
		if (const auto status = ::BCryptOpenAlgorithmProvider(
				&algorithm,
				BCRYPT_SHA512_ALGORITHM,
				nullptr,
				0);
			!winapi::nt_success(status)) {
			throw winapi::nt_error("failed to open algorithm provider", status);
		}
		const util::scope_exit delAlgorithm([&]() {
			[[maybe_unused]] const auto success =
				winapi::nt_success(::BCryptCloseAlgorithmProvider(algorithm, 0));
			assert(success);
		});

		::BCRYPT_HASH_HANDLE hash;
		if (const auto status = ::BCryptCreateHash(
				algorithm,
				&hash,
				nullptr,
				0,
				nullptr,
				0,
				0);
			!winapi::nt_success(status)) {
			throw winapi::nt_error("failed to create hash", status);
		}
		const util::scope_exit delHash([&]() {
			[[maybe_unused]] const auto success =
				winapi::nt_success(::BCryptDestroyHash(hash));
			assert(success);
		});

		if (const auto status = ::BCryptHashData(
				hash,
				reinterpret_cast<::PUCHAR>(const_cast<std::byte*>(a_data.data())),  // does not modify contents of buffer
				static_cast<::ULONG>(a_data.size()),
				0);
			!winapi::nt_success(status)) {
			throw winapi::nt_error("failed to hash data", status);
		}

		::DWORD hashLen = 0;
		::ULONG discard = 0;
		if (const auto status = ::BCryptGetProperty(
				hash,
				BCRYPT_HASH_LENGTH,
				reinterpret_cast<::PUCHAR>(&hashLen),
				sizeof(hashLen),
				&discard,
				0);
			!winapi::nt_success(status)) {
			throw winapi::nt_error("failed to get property", status);
		}
		std::vector<::UCHAR> buffer(static_cast<std::size_t>(hashLen));

		if (const auto status = ::BCryptFinishHash(
				hash,
				buffer.data(),
				static_cast<::ULONG>(buffer.size()),
				0);
			!winapi::nt_success(status)) {
			throw winapi::nt_error("failed to finish hash", status);
		}

		std::array<char, 3> buf{};
		std::string result;
		result.reserve(buffer.size() * 2);
		for (const auto byte : buffer) {
			std::snprintf(buf.data(), buf.size(), "%.02hhX", static_cast<unsigned char>(byte));
			result += std::string_view{ buf.data(), 2 };
		}

		return result;
	}
}
