#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/endian/conversion.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/predef.h>

#include "bsa/fwd.hpp"

#define BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, a_op)                                     \
	[[nodiscard]] constexpr a_type operator a_op(a_type a_lhs, a_type a_rhs) noexcept \
	{                                                                                 \
		return static_cast<a_type>(                                                   \
			static_cast<std::underlying_type_t<a_type>>(a_lhs)                        \
				a_op static_cast<std::underlying_type_t<a_type>>(a_rhs));             \
	}                                                                                 \
                                                                                      \
	constexpr a_type& operator a_op##=(a_type& a_lhs, a_type a_rhs) noexcept          \
	{                                                                                 \
		return a_lhs = a_lhs a_op a_rhs;                                              \
	}

#define BSA_MAKE_ALL_ENUM_OPERATORS(a_type)                                  \
	static_assert(std::is_enum_v<a_type>, "\'" #a_type "\' is not an enum"); \
                                                                             \
	[[nodiscard]] constexpr a_type operator~(a_type a_val) noexcept          \
	{                                                                        \
		return static_cast<a_type>(                                          \
			~static_cast<std::underlying_type_t<a_type>>(a_val));            \
	}                                                                        \
                                                                             \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, ^)                                   \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, &)                                   \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, |)                                   \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, <<)                                  \
	BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, >>)

namespace bsa
{
	using namespace std::literals;

	class exception :
		public std::exception
	{
	public:
		exception(const char* a_what) noexcept :
			_what(a_what)
		{}

		const char* what() const noexcept { return _what; }

	private:
		const char* _what{ nullptr };
	};
}

/// \cond
namespace bsa::detail
{
	namespace concepts
	{
		template <class T>
		concept integral =
			std::is_enum_v<T> ||

			std::same_as<T, char> ||

			std::same_as<T, signed char> ||
			std::same_as<T, signed short int> ||
			std::same_as<T, signed int> ||
			std::same_as<T, signed long int> ||
			std::same_as<T, signed long long int> ||

			std::same_as<T, unsigned char> ||
			std::same_as<T, unsigned short int> ||
			std::same_as<T, unsigned int> ||
			std::same_as<T, unsigned long int> ||
			std::same_as<T, unsigned long long int>;
	}

	[[noreturn]] inline void declare_unreachable()
	{
		assert(false);
#if BOOST_COMP_GNUC || BOOST_COMP_CLANG
		__builtin_unreachable();
#elif BOOST_COMP_MSVC || BOOST_COMP_EDG
		__assume(false);
#else
		static_assert(false);
#endif
	}

	void normalize_path(std::string& a_path) noexcept;

	[[nodiscard]] auto read_bstring(detail::istream_t& a_in) -> std::string_view;
	[[nodiscard]] auto read_bzstring(detail::istream_t& a_in) -> std::string_view;
	[[nodiscard]] auto read_wstring(detail::istream_t& a_in) -> std::string_view;
	[[nodiscard]] auto read_zstring(detail::istream_t& a_in) -> std::string_view;

	template <class Enum>
	[[nodiscard]] constexpr auto to_underlying(Enum a_val) noexcept
		-> std::underlying_type_t<Enum>
	{
		static_assert(std::is_enum_v<Enum>, "Input type is not an enumeration");
		return static_cast<std::underlying_type_t<Enum>>(a_val);
	}

	void write_bzstring(detail::ostream_t& a_out, std::string_view a_string) noexcept;
	void write_wstring(detail::ostream_t& a_out, std::string_view a_string) noexcept;
	void write_zstring(detail::ostream_t& a_out, std::string_view a_string) noexcept;

	class istream_t final
	{
	public:
		using stream_type = boost::iostreams::mapped_file_source;

		istream_t(std::filesystem::path a_path);
		istream_t(const istream_t&) = delete;
		istream_t(istream_t&&) = delete;
		~istream_t() noexcept = default;
		istream_t& operator=(const istream_t&) = delete;
		istream_t& operator=(istream_t&&) = delete;

		template <concepts::integral T>
		[[nodiscard]] T read(std::endian a_endian = std::endian::little)
		{
			const auto load = [&]<std::size_t I>(std::in_place_index_t<I>) {
				const auto bytes = this->read_bytes(sizeof(T));
				return boost::endian::endian_load<
					T,
					sizeof(T),
					static_cast<boost::endian::order>(I)>(
					reinterpret_cast<const unsigned char*>(bytes.data()));
			};

			switch (a_endian) {
			case std::endian::little:
				return load(std::in_place_index<to_underlying(boost::endian::order::little)>);
			case std::endian::big:
				return load(std::in_place_index<to_underlying(boost::endian::order::big)>);
			default:
				declare_unreachable();
			}
		}

		[[nodiscard]] auto read_bytes(std::size_t a_bytes)
			-> std::span<const std::byte>;

		void seek_absolute(std::size_t a_pos) noexcept { _pos = a_pos; }
		void seek_relative(std::ptrdiff_t a_off) noexcept { _pos += a_off; }

		[[nodiscard]] auto tell() const noexcept { return _pos; }
		[[nodiscard]] auto rdbuf() const noexcept -> const stream_type& { return _file; }

		template <concepts::integral T>
		friend istream_t& operator>>(istream_t& a_in, T& a_value)
		{
			a_value = a_in.read<T>();
			return a_in;
		}

		template <class T, std::size_t N>
		friend istream_t& operator>>(
			istream_t& a_in,
			std::array<T, N>& a_value)  //
			requires(sizeof(T) == 1)
		{
			const auto bytes = a_in.read_bytes(a_value.size());
			std::copy(
				bytes.begin(),
				bytes.end(),
				reinterpret_cast<std::byte*>(a_value.data()));
			return a_in;
		}

	private :
		stream_type _file;
		std::size_t _pos{ 0 };
	};

	template <class T>
	struct istream_proxy final
	{
		T d;
		boost::iostreams::mapped_file_source f;
	};

	class ostream_t final
	{
	public:
		ostream_t(std::filesystem::path a_path);
		ostream_t(const ostream_t&) = delete;
		ostream_t(ostream_t&&) = delete;
		~ostream_t() noexcept;
		ostream_t& operator=(const ostream_t&) = delete;
		ostream_t& operator=(ostream_t&&) = delete;

		template <concepts::integral T>
		void write(T a_value, std::endian a_endian = std::endian::little) noexcept
		{
			const auto store = [&]<std::size_t I>(std::in_place_index_t<I>) noexcept {
				std::array<std::byte, sizeof(T)> bytes{};
				boost::endian::endian_store<
					T,
					sizeof(T),
					static_cast<boost::endian::order>(I)>(
					reinterpret_cast<unsigned char*>(bytes.data()),
					a_value);
				this->write_bytes({ bytes.cbegin(), bytes.cend() });
			};

			switch (a_endian) {
			case std::endian::little:
				store(std::in_place_index<to_underlying(boost::endian::order::little)>);
				break;
			case std::endian::big:
				store(std::in_place_index<to_underlying(boost::endian::order::big)>);
				break;
			default:
				declare_unreachable();
			}
		}

		void write_bytes(std::span<const std::byte> a_bytes) noexcept
		{
			std::fwrite(a_bytes.data(), 1, a_bytes.size_bytes(), _file);
		}

		template <concepts::integral T>
		friend ostream_t& operator<<(ostream_t& a_out, T a_value) noexcept
		{
			a_out.write(a_value);
			return a_out;
		}

		template <class T, std::size_t N>
		friend ostream_t& operator<<(
			ostream_t& a_out,
			const std::array<T, N>& a_value) noexcept  //
			requires(sizeof(T) == 1)
		{
			a_out.write_bytes({ //
				reinterpret_cast<const std::byte*>(a_value.data()),
				a_value.size() * sizeof(T) });
			return a_out;
		}

	private :
		std::FILE* _file{ nullptr };
	};

	class restore_point final
	{
	public:
		restore_point(const restore_point&) = delete;

		explicit restore_point(istream_t& a_proxy) noexcept :
			_proxy(a_proxy),
			_pos(a_proxy.tell())
		{}

		~restore_point() noexcept { _proxy.seek_absolute(_pos); }

	private:
		istream_t& _proxy;
		std::size_t _pos;
	};
}
/// \endcond

namespace bsa::concepts
{
	template <class T>
	concept stringable =
		std::constructible_from<std::string, T>;
}

namespace bsa::components
{
	/// \brief	A basic byte storage container.
	/// \details	Primarily stores non-allocating, immutable views into externally backed data,
	///		but is capable of managing its data's lifetime as a convenience.
	class basic_byte_container
	{
	public:
		basic_byte_container() noexcept = default;
		basic_byte_container(const basic_byte_container&) noexcept = default;
		basic_byte_container(basic_byte_container&&) noexcept = default;
		~basic_byte_container() noexcept = default;
		basic_byte_container& operator=(const basic_byte_container&) noexcept = default;
		basic_byte_container& operator=(basic_byte_container&&) noexcept = default;

		/// \brief	Retrieves an immutable view into the underlying bytes.
		auto as_bytes() const noexcept -> std::span<const std::byte>;

		/// \brief	Retrieves an immutable pointer to the underlying bytes.
		[[nodiscard]] auto data() const noexcept
			-> const std::byte* { return as_bytes().data(); }

		/// \brief	Checks if the underlying byte container is empty.
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

		/// \brief	Returns the size of the underlying byte container.
		[[nodiscard]] auto size() const noexcept
			-> std::size_t { return as_bytes().size(); }

	private:
		friend compressed_byte_container;
		friend byte_container;

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		using data_proxy = detail::istream_proxy<std::span<const std::byte>>;

		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;

		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	/// \brief	A byte storage container without compression support.
	class byte_container :
		public basic_byte_container
	{
	public:
		/// \brief	Assigns the underlying container to be a non-owning view into the given data.
		///
		/// \param	a_data	The data to store a view to.
		void set_data(std::span<const std::byte> a_data) noexcept
		{
			_data.emplace<data_view>(a_data);
		}

		/// \brief	Assigns the underlying container to be an owning view into the given data.
		///
		/// \param	a_data	The data to take ownership of.
		void set_data(std::vector<std::byte> a_data) noexcept
		{
			_data.emplace<data_owner>(std::move(a_data));
		}

	protected:
		void clear() noexcept { _data.emplace<data_view>(); }

		void set_data(
			std::span<const std::byte> a_data,
			const detail::istream_t& a_in) noexcept
		{
			_data.emplace<data_proxied>(a_data, a_in.rdbuf());
		}
	};

	/// \brief	A byte storage container with compression support.
	class compressed_byte_container :
		public basic_byte_container
	{
	public:
		/// \brief	Checks if the underlying bytes are compressed.
		[[nodiscard]] bool compressed() const noexcept { return _decompsz.has_value(); }

		/// \brief	Retrieves the decompressed size of the compressed storage.
		/// \details	Only valid if the container *is* compressed.
		[[nodiscard]] auto decompressed_size() const noexcept
			-> std::size_t
		{
			assert(this->compressed());
			return *_decompsz;
		}

		/// \copydoc byte_container::set_data(std::span<const std::byte>)
		///
		/// \param	a_decompressedSize	The decompressed size of the data,
		///		if the given data is compressed.
		void set_data(
			std::span<const std::byte> a_data,
			std::optional<std::size_t> a_decompressedSize = std::nullopt) noexcept
		{
			_data.emplace<data_view>(a_data);
			_decompsz = a_decompressedSize;
		}

		/// \copydoc byte_container::set_data(std::vector<std::byte>)
		///
		/// \param	a_decompressedSize	The decompressed size of the data,
		///		if the given data is compressed.
		void set_data(
			std::vector<std::byte> a_data,
			std::optional<std::size_t> a_decompressedSize = std::nullopt) noexcept
		{
			_data.emplace<data_owner>(std::move(a_data));
			_decompsz = a_decompressedSize;
		}

	protected:
		void clear() noexcept
		{
			_data.emplace<data_view>();
			_decompsz.reset();
		}

		void set_data(
			std::span<const std::byte> a_data,
			const detail::istream_t& a_in,
			std::optional<std::size_t> a_decompressedSize = std::nullopt) noexcept
		{
			_data.emplace<data_proxied>(a_data, a_in.rdbuf());
			_decompsz = a_decompressedSize;
		}

	private:
		std::optional<std::size_t> _decompsz;
	};

	/// \brief	Establishes a basic mapping between a \ref key and its
	///		associated files.
	///
	/// \tparam	The `mapped_type`
	template <class T, bool RECURSE>
	class hashmap
	{
	private:
		using container_type =
			std::map<typename T::key, T>;

	public:
		using key_type = typename container_type::key_type;
		using mapped_type = typename container_type::mapped_type;
		using value_type = typename container_type::value_type;
		using key_compare = typename container_type::key_compare;
		using iterator = typename container_type::iterator;
		using const_iterator = typename container_type::const_iterator;

		/// \brief	A proxy value used to facilitate the usage/chaining of \ref hashmap::operator[]
		///		in an intuitive manner.
		template <class U>
		class index_t final
		{
		public:
			using value_type = U;
			using pointer = value_type*;
			using reference = value_type&;

			/// \brief	Constructs an empty index.
			index_t() noexcept = default;

			/// \brief	Indexes the stored index with the given key.
			/// \remark	Indexing an empty index is valid, and will simply yield another empty index.
			template <class K>
			[[nodiscard]] auto operator[](K&& a_key) const noexcept  //
				requires(RECURSE)
			{
				using result_t = decltype((**this)[std::forward<K>(a_key)]);
				return (*this) ?
                           (**this)[std::forward<K>(a_key)] :
                           result_t{};
			}

			/// \brief	Checks if the current index is valid.
			[[nodiscard]] explicit operator bool() const noexcept { return _proxy != nullptr; }

			/// \brief	Obtains a reference to the currently held index.
			/// \pre	The current index *must* be valid.
			[[nodiscard]] auto operator*() const noexcept -> reference
			{
				assert(*this);
				return *_proxy;
			}

			/// \brief	Obtains a pointer to the currently held index.
			/// \remark	The current index does *not* need to be valid.
			[[nodiscard]] auto operator->() const noexcept -> pointer { return _proxy; }

		private:
			friend hashmap;

			explicit index_t(value_type& a_value) noexcept :
				_proxy(&a_value)
			{}

			value_type* _proxy{ nullptr };
		};

		using index = index_t<mapped_type>;
		using const_index = index_t<const mapped_type>;

		hashmap() noexcept = default;
		hashmap(const hashmap&) noexcept = default;
		hashmap(hashmap&&) noexcept = default;
		~hashmap() noexcept = default;
		hashmap& operator=(const hashmap&) noexcept = default;
		hashmap& operator=(hashmap&&) noexcept = default;

		/// \brief	Obtains a proxy to the underlying `mapped_type`. The validity of the
		///		proxy depends on the presence of the key within the container.
		[[nodiscard]] auto operator[](const key_type& a_key) noexcept
			-> index
		{
			const auto it = _map.find(a_key);
			return it != _map.end() ? index{ it->second } : index{};
		}

		/// \copybrief hashmap::operator[]
		[[nodiscard]] auto operator[](const key_type& a_key) const noexcept
			-> const_index
		{
			const auto it = _map.find(a_key);
			return it != _map.end() ? const_index{ it->second } : const_index{};
		}

		/// \brief	Obtains an interator to the beginning of the container.
		[[nodiscard]] auto begin() noexcept -> iterator { return _map.begin(); }
		/// \copybrief hashmap::begin
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _map.begin(); }
		/// \copybrief hashmap::begin
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _map.cbegin(); }

		/// \brief	Obtains an iterator to the end of the container.
		[[nodiscard]] auto end() noexcept -> iterator { return _map.end(); }
		/// \copybrief hashmap::end
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _map.end(); }
		/// \copybrief hashmap::end
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _map.cend(); }

		/// \brief	Checks if the container is empty.
		[[nodiscard]] bool empty() const noexcept { return _map.empty(); }

		/// \brief	Erases any element with the given key from the container.
		///
		/// \return	Returns `true` if the element was successfully deleted, `false` otherwise.
		bool erase(const key_type& a_key) noexcept
		{
			const auto it = _map.find(a_key);
			if (it != _map.end()) {
				_map.erase(it);
				return true;
			} else {
				return false;
			}
		}

		/// \brief	Finds a `value_type` with the given key within the container.
		[[nodiscard]] auto find(const key_type& a_key) noexcept
			-> iterator { return _map.find(a_key); }

		/// \copybrief hashmap::find(const key_type&) const noexcept
		[[nodiscard]] auto find(const key_type& a_key) const noexcept
			-> const_iterator { return _map.find(a_key); }

		/// \brief	Inserts `a_value` into the container with the given `a_key`.
		///
		/// \param	a_key	The key of the `value_type`.
		/// \param	a_value The value of the `value_type`.
		/// \return	Returns an `iterator` to the position at which the given `value_type`
		///		was inserted, and a `bool` to indicate if the insertion was successful.
		auto insert(key_type a_key, mapped_type a_value) noexcept
			-> std::pair<iterator, bool>
		{
			return _map.emplace(std::move(a_key), std::move(a_value));
		}

		/// \brief	Returns the number of elements in the container.
		[[nodiscard]] auto size() const noexcept -> std::size_t { return _map.size(); }

	protected:
		void clear() { _map.clear(); }

	private:
		container_type _map;
	};

	/// \brief	A generic key used to uniquely identify an object inside the virtual filesystem.
	///
	/// \tparam	Hash	The hash type used as the underlying key.
	/// \tparam	Hasher	The function used to generate the hash.
	template <class Hash, hasher_t<Hash> Hasher>
	class key final
	{
	public:
		/// \brief	The underlying hash type.
		using hash_type = Hash;

		key() = delete;

		/// \brief	Construct a key using a raw hash.
		///
		/// \param	a_hash	The raw hash that identifies the key.
		key(hash_type a_hash) noexcept :
			_hash(a_hash)
		{}

		/// \brief	Construct a key using a string-like object.
		///
		/// \param	a_string	The string-like object used to generate the underlying hash.
		template <concepts::stringable String>
		key(String&& a_string) noexcept
		{
			_name.emplace<name_owner>(std::forward<String>(a_string));
			_hash = Hasher(*std::get_if<name_owner>(&_name));
		}

		key(const key&) noexcept = default;
		key(key&&) noexcept = default;
		~key() noexcept = default;
		key& operator=(const key&) noexcept = default;
		key& operator=(key&&) noexcept = default;

		/// \brief	Retrieve a reference to the underlying hash.
		[[nodiscard]] auto hash() const noexcept -> const hash_type& { return _hash; }

		/// \brief	Retrieve the name that generated the underlying hash.
		[[nodiscard]] auto name() const noexcept
			-> std::string_view
		{
			switch (_name.index()) {
			case name_null:
				return {};
			case name_owner:
				return *std::get_if<name_owner>(&_name);
			case name_proxied:
				return std::get_if<name_proxied>(&_name)->d;
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] friend auto operator==(
			const key& a_lhs,
			const key& a_rhs) noexcept
			-> bool { return a_lhs.hash() == a_rhs.hash(); }

		[[nodiscard]] friend auto operator==(
			const key& a_lhs,
			const hash_type& a_rhs) noexcept
			-> bool { return a_lhs.hash() == a_rhs; }

		[[nodiscard]] friend auto operator<=>(
			const key& a_lhs,
			const key& a_rhs) noexcept
			-> std::strong_ordering { return a_lhs.hash() <=> a_rhs.hash(); }

		[[nodiscard]] friend auto operator<=>(
			const key& a_lhs,
			const hash_type& a_rhs) noexcept
			-> std::strong_ordering { return a_lhs.hash() <=> a_rhs; }

	private:
		friend fo4::archive;
		friend tes3::archive;
		friend tes4::archive;

		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied,

			name_count
		};

		using name_proxy = detail::istream_proxy<std::string_view>;

		key(hash_type a_hash,
			std::string_view a_name,
			const detail::istream_t& a_in) noexcept :
			_hash(a_hash),
			_name(std::in_place_index<name_proxied>, a_name, a_in.rdbuf())
		{}

		hash_type _hash;
		std::variant<
			std::monostate,
			std::string,
			name_proxy>
			_name;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
	};
}
