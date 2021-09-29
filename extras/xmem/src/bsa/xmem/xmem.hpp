#pragma once

#include <tl/expected.hpp>

namespace bsa::xmem
{
	enum class error_code
	{
		hashing_open_algorithm_provider_failure,
		hashing_create_hash_failure,
		hashing_hash_data_failure,
		hashing_get_property_failure,
		hashing_finish_hash_failure,

		reg_get_value_failure,

		xcompress_no_proxy_found,
		xcompress_unhandled_version,

		api_compress_failure,
		api_compress_bound_failure,
		api_decompress_failure,
	};

	template <class T>
	using expected = tl::expected<T, xmem::error_code>;

	using tl::unexpected;
}
