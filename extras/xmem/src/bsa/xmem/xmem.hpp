#pragma once

#include <string_view>

#include <tl/expected.hpp>

namespace bsa::xmem
{
	using namespace std::literals;

#define ENUMERATE(F)                            \
	F(hashing_open_algorithm_provider_failure)  \
	F(hashing_create_hash_failure)              \
	F(hashing_hash_data_failure)                \
	F(hashing_get_property_failure)             \
	F(hashing_finish_hash_failure)              \
                                                \
	F(reg_get_value_failure)                    \
                                                \
	F(xcompress_no_proxy_found)                 \
	F(xcompress_unhandled_version)              \
                                                \
	F(api_create_compression_context_failure)   \
	F(api_create_decompression_context_failure) \
	F(api_compress_failure)                     \
	F(api_compress_bound_failure)               \
	F(api_decompress_failure)

	enum class error_code
	{
#define AS_ENUM(a_enum) a_enum,
		ENUMERATE(AS_ENUM)
#undef AS_ENUM
	};

	[[nodiscard]] constexpr auto to_string(error_code a_code) noexcept
		-> std::string_view
	{
#define AS_STRING(a_enum)      \
	case error_code::##a_enum: \
		return #a_enum##sv;

		switch (a_code) {
			ENUMERATE(AS_STRING)
		default:
			return "unknown"sv;
		}

#undef AS_STRING
	}

#undef ENUMERATE

	template <class T>
	using expected = tl::expected<T, xmem::error_code>;

	using tl::unexpected;
}
