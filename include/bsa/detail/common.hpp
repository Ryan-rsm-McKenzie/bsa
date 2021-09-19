#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <binary_io/any_stream.hpp>
#include <binary_io/span_stream.hpp>
#include <mmio/mmio.hpp>

#include "bsa/fwd.hpp"

#ifndef DOXYGEN

#	if defined(__clang__)
#		define BSA_COMP_CLANG true
#	else
#		define BSA_COMP_CLANG false
#	endif

#	if !BSA_COMP_CLANG && defined(__GNUC__)
#		define BSA_COMP_GNUC true
#	else
#		define BSA_COMP_GNUC false
#	endif

#	if defined(__EDG__)
#		define BSA_COMP_EDG true
#	else
#		define BSA_COMP_EDG false
#	endif

#	if !BSA_COMP_CLANG && !BSA_COMP_EDG && defined(_MSC_VER)
#		define BSA_COMP_MSVC true
#	else
#		define BSA_COMP_MSVC false
#	endif

#	define BSA_MAKE_ENUM_OPERATOR_PAIR(a_type, a_op)                                     \
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

#	define BSA_MAKE_ALL_ENUM_OPERATORS(a_type)                                  \
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

#	if BSA_COMP_GNUC || BSA_COMP_CLANG
#		define BSA_VISIBLE __attribute__((visibility("default")))
#	else
#		define BSA_VISIBLE
#	endif

#endif

namespace bsa
{
#ifndef DOXYGEN
	using namespace std::literals;
#endif

	/// \brief	The copy method to use when reading from in-memory buffers.
	enum class copy_type
	{
		/// Makes a deep copy of the given data (i.e. uses an allocating `std::vector` instead of
		///		a lightweight `std::span`).
		deep,

		/// Make a shallow copy of the given data (i.e. uses a lightweight `std::span` instead of
		///		an allocating `std::vector`).
		shallow
	};

	/// \brief	The file format for a given archive.
	enum class file_format
	{
		tes3,
		tes4,
		fo4
	};

	/// \brief	The base exception type for all `bsa` exceptions.
	class BSA_VISIBLE exception :
		public std::exception
	{
	public:
		/// \brief	Constructs an exception with the given message.
		exception(const char* a_what) noexcept :
			_what(a_what)
		{}

		/// \brief	Obtains the explanation for why the exception was thrown.
		const char* what() const noexcept { return _what; }

	private:
		const char* _what{ nullptr };
	};

#ifdef DOXYGEN
	/// \brief	A doxygen only, detail class.
	/// \details	This is a class that exists solely to de-duplicate documentation.
	///		Do not attempt to use this.
	struct doxygen_detail
	{
	protected:
		/// \brief	Guesses the archive format for a given file.
		///
		/// \exception	binary_io::buffer_exhausted	Thrown when reads index out of bounds.
		/// \exception	bsa::exception	Thrown when parsing errors are encountered.
		///
		/// \remark	This function does not guarantee that the given file constitutes a well-formed
		///		archive of the deduced format. It merely remarks that if the file *were* a
		///		well-formed archive, it would be of the deduced format.
		///
		/// \return	The guessed archive format for the given file, or `std::nullopt` if the file
		///		doesn't match any known format.
		static std::optional<file_format> guess_file_format(
			std::filesystem::path a_path);
	};
#endif

	/// \copydoc bsa::doxygen_detail::guess_file_format()
	///
	/// \param	a_path	The file to guess.
	[[nodiscard]] std::optional<file_format> guess_file_format(
		std::filesystem::path a_path);

	/// \copydoc bsa::doxygen_detail::guess_file_format()
	///
	/// \param	a_src	The buffer to guess.
	[[nodiscard]] std::optional<file_format> guess_file_format(
		std::span<const std::byte> a_src);

	/// \brief	Converts, at most, the first 4 characters of the given string into a 4 byte integer.
	[[nodiscard]] constexpr std::uint32_t make_four_cc(
		std::string_view a_cc) noexcept

	{
		std::uint32_t result = 0;
		for (std::size_t i = 0; i < std::min<std::size_t>(a_cc.size(), 4); ++i) {
			result |= std::uint32_t{ static_cast<unsigned char>(a_cc[i]) }
			          << i * 8u;
		}
		return result;
	}
}

#ifndef DOXYGEN
namespace bsa::detail
{
	using ostream_t = binary_io::any_ostream;

	[[noreturn]] inline void declare_unreachable()
	{
		assert(false);
#	if BSA_COMP_GNUC || BSA_COMP_CLANG
		__builtin_unreachable();
#	elif BSA_COMP_MSVC || BSA_COMP_EDG
		__assume(false);
#	else
		static_assert(false);
#	endif
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
		using stream_type = binary_io::span_istream;
		using file_type = mmio::mapped_file_source;

		istream_t(std::filesystem::path a_path);
		istream_t(std::span<const std::byte> a_bytes, copy_type a_copy) noexcept;

		istream_t(const volatile istream_t&) = delete;
		istream_t& operator=(const volatile istream_t&) = delete;

		[[nodiscard]] auto operator*() noexcept
			-> stream_type& { return _stream; }
		[[nodiscard]] auto operator*() const noexcept
			-> const stream_type& { return _stream; }

		[[nodiscard]] auto operator->() noexcept
			-> stream_type* { return &_stream; }
		[[nodiscard]] auto operator->() const noexcept
			-> const stream_type* { return &_stream; }

		[[nodiscard]] bool deep_copy() const noexcept { return _copy == copy_type::deep; }
		[[nodiscard]] auto file() const noexcept { return _file; }
		[[nodiscard]] bool has_file() const noexcept { return _file != nullptr; }
		[[nodiscard]] bool shallow_copy() const noexcept { return _copy == copy_type::shallow; }

	private:
		std::shared_ptr<file_type> _file;
		stream_type _stream;
		copy_type _copy{ copy_type::deep };
	};

	template <class T>
	struct istream_proxy final
	{
		T d;
		std::shared_ptr<istream_t::file_type> f;
	};

	class restore_point final
	{
	public:
		restore_point(const restore_point&) = delete;

		explicit restore_point(istream_t& a_proxy) noexcept :
			_proxy(a_proxy),
			_pos(a_proxy->tell())
		{}

		~restore_point() noexcept { _proxy->seek_absolute(_pos); }

	private:
		istream_t& _proxy;
		std::size_t _pos;
	};
}
#endif

namespace bsa::concepts
{
#ifdef DOXYGEN
	/// \brief	Defines a type that can be used to construct `std::string`
	struct stringable
	{};
#else
	template <class T>
	concept stringable =
		std::constructible_from<std::string, T>;
#endif
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
		std::span<const std::byte> as_bytes() const noexcept;

		/// \brief	Retrieves an immutable pointer to the underlying bytes.
		[[nodiscard]] const std::byte* data() const noexcept { return as_bytes().data(); }

		/// \brief	Checks if the underlying byte container is empty.
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

		/// \brief	Returns the size of the underlying byte container.
		[[nodiscard]] std::size_t size() const noexcept { return as_bytes().size(); }

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

#ifndef DOXYGEN
	protected:
		void clear() noexcept { _data.emplace<data_view>(); }

		void set_data(
			std::span<const std::byte> a_data,
			const detail::istream_t& a_in) noexcept
		{
			if (a_in.has_file() && a_in.shallow_copy()) {
				_data.emplace<data_proxied>(a_data, a_in.file());
			} else {
				if (a_in.deep_copy()) {
					_data.emplace<data_owner>(a_data.begin(), a_data.end());
				} else {
					_data.emplace<data_view>(a_data);
				}
			}
		}
#endif
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
		[[nodiscard]] std::size_t decompressed_size() const noexcept
		{
			assert(this->compressed());
			return *_decompsz;
		}

		/// \copydoc bsa::components::byte_container::set_data(std::span<const std::byte>)
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

		/// \copydoc bsa::components::byte_container::set_data(std::vector<std::byte>)
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

#ifndef DOXYGEN
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
			if (a_in.has_file() && a_in.shallow_copy()) {
				_data.emplace<data_proxied>(a_data, a_in.file());
			} else {
				if (a_in.deep_copy()) {
					_data.emplace<data_owner>(a_data.begin(), a_data.end());
				} else {
					_data.emplace<data_view>(a_data);
				}
			}
			_decompsz = a_decompressedSize;
		}
#endif

	private:
		std::optional<std::size_t> _decompsz;
	};

	/// \brief	Establishes a basic mapping between a \ref key and its
	///		associated files.
	///
	/// \tparam	T	The `mapped_type`.
	/// \tparam	RECURSE	Determines if indexing via `operator[]` is a recursive action.
	template <class T, bool RECURSE>
	class hashmap
	{
	private:
		using container_type =
			std::map<typename T::key, T>;

	public:
#ifdef DOXYGEN
		using key_type = typename T::key;
		using mapped_type = T;
		using value_type = std::pair<const key_type, mapped_type>;
#else
		using key_type = typename container_type::key_type;
		using mapped_type = typename container_type::mapped_type;
		using value_type = typename container_type::value_type;
#endif
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
			[[nodiscard]] reference operator*() const noexcept
			{
				assert(*this);
				return *_proxy;
			}

			/// \brief	Obtains a pointer to the currently held index.
			/// \remark	The current index does *not* need to be valid.
			[[nodiscard]] pointer operator->() const noexcept { return _proxy; }

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
		[[nodiscard]] index operator[](const key_type& a_key) noexcept
		{
			const auto it = _map.find(a_key);
			return it != _map.end() ? index{ it->second } : index{};
		}

		/// \copybrief operator[]()
		[[nodiscard]] const_index operator[](const key_type& a_key) const noexcept
		{
			const auto it = _map.find(a_key);
			return it != _map.end() ? const_index{ it->second } : const_index{};
		}

		/// \brief	Obtains an interator to the beginning of the container.
		[[nodiscard]] iterator begin() noexcept { return _map.begin(); }
		/// \copybrief begin()
		[[nodiscard]] const_iterator begin() const noexcept { return _map.begin(); }
		/// \copybrief begin()
		[[nodiscard]] const_iterator cbegin() const noexcept { return _map.cbegin(); }

		/// \brief	Obtains an iterator to the end of the container.
		[[nodiscard]] iterator end() noexcept { return _map.end(); }
		/// \copybrief end()
		[[nodiscard]] const_iterator end() const noexcept { return _map.end(); }
		/// \copybrief end()
		[[nodiscard]] const_iterator cend() const noexcept { return _map.cend(); }

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
		[[nodiscard]] iterator find(const key_type& a_key) noexcept { return _map.find(a_key); }

		/// \copybrief find()
		[[nodiscard]] const_iterator find(const key_type& a_key) const noexcept { return _map.find(a_key); }

		/// \brief	Inserts `a_value` into the container with the given `a_key`.
		///
		/// \param	a_key	The key of the `value_type`.
		/// \param	a_value The value of the `value_type`.
		/// \return	Returns an `iterator` to the position at which the given `value_type`
		///		was inserted, and a `bool` to indicate if the insertion was successful.
		std::pair<iterator, bool> insert(
			key_type a_key,
			mapped_type a_value) noexcept
		{
			return _map.emplace(std::move(a_key), std::move(a_value));
		}

		/// \brief	Returns the number of elements in the container.
		[[nodiscard]] std::size_t size() const noexcept { return _map.size(); }

#ifndef DOXYGEN
	protected:
		void clear() noexcept { _map.clear(); }
#endif

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
		[[nodiscard]] const hash_type& hash() const noexcept { return _hash; }

		/// \brief	Retrieve the name that generated the underlying hash.
		[[nodiscard]] std::string_view name() const noexcept
		{
			switch (_name.index()) {
			case name_view:
				return *std::get_if<name_view>(&_name);
			case name_owner:
				return *std::get_if<name_owner>(&_name);
			case name_proxied:
				return std::get_if<name_proxied>(&_name)->d;
			default:
				detail::declare_unreachable();
			}
		}

		[[nodiscard]] friend bool operator==(
			const key& a_lhs,
			const key& a_rhs) noexcept
		{
			return a_lhs.hash() == a_rhs.hash();
		}

		[[nodiscard]] friend bool operator==(
			const key& a_lhs,
			const hash_type& a_rhs) noexcept
		{
			return a_lhs.hash() == a_rhs;
		}

		[[nodiscard]] friend std::strong_ordering operator<=>(
			const key& a_lhs,
			const key& a_rhs) noexcept
		{
			return a_lhs.hash() <=> a_rhs.hash();
		}

		[[nodiscard]] friend std::strong_ordering operator<=>(
			const key& a_lhs,
			const hash_type& a_rhs) noexcept
		{
			return a_lhs.hash() <=> a_rhs;
		}

	private:
#ifndef DOXYGEN
		friend fo4::archive;
		friend tes3::archive;
		friend tes4::archive;
#endif

		enum : std::size_t
		{
			name_view,
			name_owner,
			name_proxied,

			name_count
		};

		using name_proxy = detail::istream_proxy<std::string_view>;

		key(hash_type a_hash,
			std::string_view a_name,
			const detail::istream_t& a_in) noexcept :
			_hash(a_hash)
		{
			if (a_in.has_file() && a_in.shallow_copy()) {
				_name.emplace<name_proxied>(a_name, a_in.file());
			} else {
				if (a_in.deep_copy()) {
					_name.emplace<name_owner>(a_name);
				} else {
					_name.emplace<name_view>(a_name);
				}
			}
		}

		hash_type _hash;
		std::variant<
			std::string_view,
			std::string,
			name_proxy>
			_name;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
	};
}
