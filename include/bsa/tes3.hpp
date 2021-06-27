#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/flat_set.hpp>

#include "bsa/detail/common.hpp"

namespace bsa::tes3
{
	namespace detail
	{
		using namespace bsa::detail;
	}

	namespace hashing
	{
		struct hash final
		{
		public:
			std::uint32_t lo{ 0 };
			std::uint32_t hi{ 0 };

			[[nodiscard]] friend bool operator==(const hash&, const hash&) noexcept = default;

			[[nodiscard]] friend auto operator<=>(const hash& a_lhs, const hash& a_rhs) noexcept
				-> std::strong_ordering { return a_lhs.numeric() <=> a_rhs.numeric(); }

			[[nodiscard]] auto numeric() const noexcept
				-> std::uint64_t
			{
				return std::uint64_t{
					std::uint64_t{ hi } << 0u * 8u |
					std::uint64_t{ lo } << 4u * 8u
				};
			}

		protected:
			friend tes3::archive;

			friend detail::istream_t& operator>>(
				detail::istream_t& a_in,
				hash& a_hash) noexcept
			{
				a_in >>
					a_hash.lo >>
					a_hash.hi;
				return a_in;
			}

			friend detail::ostream_t& operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
			{
				a_out
					<< a_hash.lo
					<< a_hash.hi;
				return a_out;
			}
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	namespace detail
	{
		template <class T>
		using index_t = bsa::detail::index_t<T, false, hashing::hash>;
	}

	class file final
	{
	public:
		explicit file(hashing::hash a_hash) noexcept :
			_hash(a_hash)
		{}

		template <detail::concepts::stringable String>
		explicit file(String&& a_path) noexcept;

		file(const file&) noexcept = default;
		file(file&& a_rhs) noexcept :
			_hash(a_rhs._hash),
			_name(a_rhs._name),
			_data(std::move(a_rhs._data))
		{
			a_rhs.clear();
		}

		~file() noexcept = default;

		file& operator=(const file&) noexcept = default;
		file& operator=(file&& a_rhs) noexcept
		{
			if (this != &a_rhs) {
				_hash = a_rhs._hash;
				_name = a_rhs._name;
				_data = std::move(a_rhs._data);

				a_rhs.clear();
			}
			return *this;
		}

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;
		void clear() noexcept { _data.emplace<data_view>(); }
		[[nodiscard]] auto data() const noexcept -> const std::byte* { return as_bytes().data(); }
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }
		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }
		[[nodiscard]] auto name() const noexcept -> std::string_view;

		void set_data(std::span<const std::byte> a_data) noexcept
		{
			_data.emplace<data_view>(a_data);
		}

		void set_data(std::vector<std::byte> a_data) noexcept
		{
			_data.emplace<data_owner>(std::move(a_data));
		}

		[[nodiscard]] auto size() const noexcept -> std::size_t { return as_bytes().size(); }

	protected:
		friend archive;

		void read(
			detail::istream_t& a_in,
			std::size_t a_nameOffset,
			std::size_t a_dataOffset) noexcept;

	private:
		enum : std::size_t
		{
			name_null,
			name_owner,
			name_proxied,

			name_count
		};

		enum : std::size_t
		{
			data_view,
			data_owner,
			data_proxied,

			data_count
		};

		using data_proxy = detail::istream_proxy<std::span<const std::byte>>;
		using name_proxy = detail::istream_proxy<std::string_view>;

		hashing::hash _hash;
		std::variant<
			std::monostate,
			std::string,
			name_proxy>
			_name;
		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>,
			data_proxy>
			_data;

		static_assert(name_count == std::variant_size_v<decltype(_name)>);
		static_assert(data_count == std::variant_size_v<decltype(_data)>);
	};

	class archive final
	{
	public:
		using key_type = file;
		using key_compare = detail::key_compare_t<key_type, hashing::hash>;

	private:
		using container_type =
			boost::container::flat_set<key_type, key_compare>;

	public:
		using value_type = container_type::value_type;
		using value_compare = container_type::value_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<value_type>;
		using const_index = detail::index_t<const value_type>;

		archive() noexcept = default;
		archive(const archive&) noexcept = default;
		archive(archive&&) noexcept = default;
		~archive() noexcept = default;
		archive& operator=(const archive&) noexcept = default;
		archive& operator=(archive&&) noexcept = default;

		[[nodiscard]] auto operator[](hashing::hash a_hash) noexcept
			-> index
		{
			const auto it = _files.find(a_hash);
			return it != _files.end() ? index{ *it } : index{};
		}

		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept
			-> const_index
		{
			const auto it = _files.find(a_hash);
			return it != _files.end() ? const_index{ *it } : const_index{};
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) noexcept
			-> index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_file(path)];
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) const noexcept
			-> const_index
		{
			std::string path(std::forward<String>(a_path));
			return (*this)[hashing::hash_file(path)];
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		void clear() noexcept { _files.clear(); }

		[[nodiscard]] bool empty() const noexcept { return _files.empty(); }

		bool erase(hashing::hash a_hash) noexcept;

		template <detail::concepts::stringable String>
		bool erase(String&& a_path) noexcept
		{
			std::string path(std::forward<String>(a_path));
			return erase(hashing::hash_file(path));
		}

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept
			-> iterator { return _files.find(a_hash); }

		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept
			-> const_iterator { return _files.find(a_hash); }

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) noexcept
			-> iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_file(path));
		}

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) const noexcept
			-> const_iterator
		{
			std::string path(std::forward<String>(a_path));
			return find(hashing::hash_file(path));
		}

		auto insert(file a_file) noexcept
			-> std::pair<iterator, bool> { return _files.insert(std::move(a_file)); }

		bool read(std::filesystem::path a_path) noexcept;

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _files.size(); }

		[[nodiscard]] bool verify_offsets() const noexcept;

		bool write(std::filesystem::path a_path) const noexcept;

	private:
		struct offsets_t;

		[[nodiscard]] auto make_header() const noexcept -> detail::header_t;

		void read_file(
			detail::istream_t& a_in,
			const offsets_t& a_offsets,
			std::size_t a_idx) noexcept;

		void write_file_entries(detail::ostream_t& a_out) const noexcept;
		void write_file_name_offsets(detail::ostream_t& a_out) const noexcept;
		void write_file_names(detail::ostream_t& a_out) const noexcept;
		void write_file_hashes(detail::ostream_t& a_out) const noexcept;
		void write_file_data(detail::ostream_t& a_out) const noexcept;

		container_type _files;
	};
}
