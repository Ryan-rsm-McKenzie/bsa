#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem::hashing
{
	[[nodiscard]] auto sha512(
		std::span<const std::byte> a_data) noexcept
		-> xmem::expected<std::string>;
}
