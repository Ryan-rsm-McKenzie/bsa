#pragma once

#include <cstdint>

namespace bsa
{
	/// \cond
	namespace detail
	{
		class istream_t;
		class ostream_t;
		class restore_point;

		template <class T>
		struct istream_proxy;
	}
	/// \endcond

	namespace components
	{
		class basic_byte_container;
		class byte_container;
		class compressed_byte_container;

		template <class, bool = false>
		class hashmap;

		template <class Hash>
		using hasher_t = Hash (*)(std::string&) noexcept;

		template <class Hash, hasher_t<Hash>>
		class key;
	}

	namespace fo4
	{
		namespace detail
		{
			class header_t;
		}

		namespace hashing
		{
			struct hash;
		}

		enum class format : std::uint32_t;

		class archive;
		class chunk;
		class file;
	}

	namespace tes3
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
		class file;
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
