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

#include <boost/container/map.hpp>

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

			friend auto operator>>(
				detail::istream_t& a_in,
				hash& a_hash) noexcept
				-> detail::istream_t&
			{
				return a_in >>
				       a_hash.lo >>
				       a_hash.hi;
			}

			friend auto operator<<(
				detail::ostream_t& a_out,
				const hash& a_hash) noexcept
				-> detail::ostream_t&
			{
				return a_out
				       << a_hash.lo
				       << a_hash.hi;
			}
		};

		[[nodiscard]] hash hash_file(std::string& a_path) noexcept;
	}

	class file final
	{
	public:
		using key = detail::key_t<hashing::hash, hashing::hash_file>;

		file() noexcept = default;
		file(const file&) noexcept = default;
		file(file&&) noexcept = default;
		~file() noexcept = default;
		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;
		void clear() noexcept { _data.emplace<data_view>(); }
		[[nodiscard]] auto data() const noexcept -> const std::byte* { return as_bytes().data(); }
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }

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
			std::size_t a_dataOffset) noexcept;

	private:
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

	class archive final
	{
	private:
		using container_type =
			boost::container::map<file::key, file>;

	public:
		using key_type = container_type::key_type;
		using mapped_type = container_type::mapped_type;
		using value_type = container_type::value_type;
		using key_compare = container_type::key_compare;
		using iterator = container_type::iterator;
		using const_iterator = container_type::const_iterator;

		using index = detail::index_t<mapped_type, false>;
		using const_index = detail::index_t<const mapped_type, false>;

		archive() noexcept = default;
		archive(const archive&) noexcept = default;
		archive(archive&&) noexcept = default;
		~archive() noexcept = default;
		archive& operator=(const archive&) noexcept = default;
		archive& operator=(archive&&) noexcept = default;

		[[nodiscard]] auto operator[](const key_type& a_key) noexcept
			-> index
		{
			const auto it = _files.find(a_key);
			return it != _files.end() ? index{ it->second } : index{};
		}

		[[nodiscard]] auto operator[](const key_type& a_key) const noexcept
			-> const_index
		{
			const auto it = _files.find(a_key);
			return it != _files.end() ? const_index{ it->second } : const_index{};
		}

		[[nodiscard]] auto begin() noexcept -> iterator { return _files.begin(); }
		[[nodiscard]] auto begin() const noexcept -> const_iterator { return _files.begin(); }
		[[nodiscard]] auto cbegin() const noexcept -> const_iterator { return _files.cbegin(); }

		[[nodiscard]] auto end() noexcept -> iterator { return _files.end(); }
		[[nodiscard]] auto end() const noexcept -> const_iterator { return _files.end(); }
		[[nodiscard]] auto cend() const noexcept -> const_iterator { return _files.cend(); }

		void clear() noexcept { _files.clear(); }

		[[nodiscard]] bool empty() const noexcept { return _files.empty(); }

		bool erase(const key_type& a_key) noexcept;

		[[nodiscard]] auto find(const key_type& a_key) noexcept
			-> iterator { return _files.find(a_key); }

		[[nodiscard]] auto find(const key_type& a_key) const noexcept
			-> const_iterator { return _files.find(a_key); }

		auto insert(key_type a_key, mapped_type a_file) noexcept
			-> std::pair<iterator, bool>
		{
			return _files.emplace(std::move(a_key), std::move(a_file));
		}

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
