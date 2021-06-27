#include "bsa/tes3.hpp"

#include <bit>
#include <cstddef>
#include <string>

namespace bsa::tes3
{
	namespace hashing
	{
		hash hash_file(std::string& a_path) noexcept
		{
			detail::normalize_directory(a_path);
			hash h;

			const std::size_t midpoint = a_path.length() / 2u;
			std::size_t i = 0;
			for (; i < midpoint; ++i) {
				// rotate between first 4 bytes
				h.lo ^= std::uint32_t{ static_cast<unsigned char>(a_path[i]) }
				        << ((i % 4u) * 8u);
			}

			for (std::uint32_t rot = 0; i < a_path.length(); ++i) {
				// rotate between last 4 bytes
				rot = std::uint32_t{ static_cast<unsigned char>(a_path[i]) }
				      << (((i - midpoint) % 4u) * 8u);
				h.hi = std::rotr(h.hi ^ rot, static_cast<int>(rot));
			}

			return h;
		}
	}
}
