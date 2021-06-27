#pragma once

#include <cstdint>

namespace bsa
{
	namespace detail
	{
		class istream_t;
		class ostream_t;
		class restore_point;

		template <class, bool>
		class index_t;

		template <class T>
		struct istream_proxy;

		template <class>
		struct key_compare_t;
	}

	namespace tes4
	{
		namespace detail
		{
			class header_t;
		}

		namespace hashing
		{
			struct hash;
		}

		class archive;
		class directory;
		class file;

		enum class archive_flag : std::uint32_t;
		enum class archive_type : std::uint16_t;
		enum class version : std::uint32_t;
	}
}
