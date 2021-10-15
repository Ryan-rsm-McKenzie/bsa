#pragma once

#include <string>
#include <string_view>

#include "bsa/xmem/expected.hpp"
#include "bsa/xmem/xmem.hpp"

#include <Windows.h>

namespace bsa::xmem::unicode
{
	[[nodiscard]] inline auto utf16_to_utf8(std::wstring_view a_in)
		-> xmem::expected<std::string>
	{
		const auto cvt = [&](char* a_dst, std::size_t a_length) {
			return ::WideCharToMultiByte(
				CP_UTF8,
				0,
				a_in.data(),
				a_in.length(),
				a_dst,
				static_cast<int>(a_length),
				nullptr,
				nullptr);
		};

		const auto len = cvt(nullptr, 0);
		if (len == 0) {
			return xmem::unexpected(error_code::unicode_conversion_failure);
		}

		std::string out(len, '\0');
		if (cvt(out.data(), out.length()) == 0) {
			return xmem::unexpected(error_code::unicode_conversion_failure);
		}

		return out;
	}
}
