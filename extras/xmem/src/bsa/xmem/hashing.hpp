#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace bsa::xmem::hashing
{
	[[nodiscard]] std::string sha512(std::span<const std::byte> a_data);
}
