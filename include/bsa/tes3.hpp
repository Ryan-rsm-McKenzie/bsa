#pragma once

#include <cassert>
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

		namespace constants
		{
			inline constexpr std::size_t file_entry_size = 0x8;
			inline constexpr std::size_t hash_size = 0x8;
			inline constexpr std::size_t header_size = 0xC;
		}

		class header_t final
		{
		public:
			header_t() noexcept = default;

			header_t(
				std::uint32_t a_hashOffset,
				std::uint32_t a_fileCount) noexcept :
				_hashOffset(a_hashOffset),
				_fileCount(a_fileCount)
			{}

			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }
			[[nodiscard]] bool good() const noexcept { return _good; }
			[[nodiscard]] auto hash_offset() const noexcept -> std::size_t { return _hashOffset; }

			friend istream_t& operator>>(
				istream_t& a_in,
				header_t& a_header) noexcept
			{
				std::uint32_t magic = 0;
				a_in >>
					magic >>
					a_header._hashOffset >>
					a_header._fileCount;

				if (magic != 0x100) {
					a_header._good = false;
				}

				return a_in;
			}

			friend ostream_t& operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
			{
				a_out
					<< std::uint32_t{ 0x100 }
					<< a_header._hashOffset
					<< a_header._fileCount;
				return a_out;
			}

		private:
			std::uint32_t _hashOffset{ 0 };
			std::uint32_t _fileCount{ 0 };
			bool _good{ true };
		};
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
				-> std::strong_ordering
			{
				return a_lhs.lo != a_rhs.lo ?
                           a_lhs.lo <=> a_rhs.lo :
                           a_lhs.hi <=> a_rhs.hi;
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
	}

	namespace detail
	{
		template <class T>
		using index_t = bsa::detail::index_t<T, false, hashing::hash>;
	}

	class file final
	{
	public:
		file(hashing::hash a_hash) noexcept;

		template <detail::concepts::stringable String>
		file(String&& a_path) noexcept;

		file(const file&) noexcept = default;
		file(file&& a_rhs) noexcept;

		~file() noexcept = default;

		file& operator=(const file&) noexcept = default;
		file& operator=(file&&) noexcept = default;

		[[nodiscard]] auto as_bytes() const noexcept -> std::span<const std::byte>;
		[[nodiscard]] auto data() const noexcept -> const std::byte* { return as_bytes().data(); }
		[[nodiscard]] bool empty() const noexcept { return size() == 0; }
		[[nodiscard]] auto filename() const noexcept -> std::string_view;
		[[nodiscard]] auto hash() const noexcept -> const hashing::hash& { return _hash; }
		void set_data(std::span<const std::byte> a_data) noexcept;
		void set_data(std::vector<std::byte> a_data) noexcept;
		[[nodiscard]] auto size() const noexcept -> std::size_t { return as_bytes().size(); }

	protected:
		friend archive;

		void read(
			detail::istream_t& a_in,
			std::size_t a_dataOffset) noexcept
		{
			std::uint32_t size = 0;
			std::uint32_t offset = 0;
			a_in >> size >> offset;

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(a_dataOffset + offset);
			_data.emplace<data_proxied>(a_in.read_bytes(size), a_in.rdbuf());
		}

		void read_filename(
			detail::istream_t& a_in,
			std::size_t a_namesOffset) noexcept
		{
			std::uint32_t offset = 0;
			a_in >> offset;

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(a_namesOffset + offset);
			_name.emplace<name_proxied>(
				reinterpret_cast<const char*>(a_in.read_bytes(1).data()),  // zstring
				a_in.rdbuf());
		}

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

	namespace detail
	{
		[[nodiscard]] auto offsetof_file_entries(const detail::header_t&) noexcept
			-> std::size_t { return constants::header_size; }

		[[nodiscard]] auto offsetof_name_offsets(const detail::header_t& a_header) noexcept
			-> std::size_t
		{
			return offsetof_file_entries(a_header) +
			       a_header.file_count() * constants::file_entry_size;
		}

		[[nodiscard]] auto offsetof_names(const detail::header_t& a_header) noexcept
			-> std::size_t
		{
			return offsetof_name_offsets(a_header) +
			       a_header.file_count() * 4u;
		}

		[[nodiscard]] auto offsetof_hashes(const detail::header_t& a_header) noexcept
			-> std::size_t { return a_header.hash_offset() - constants::header_size; }

		[[nodiscard]] auto offsetof_file_data(const detail::header_t& a_header) noexcept
			-> std::size_t
		{
			return offsetof_hashes(a_header) +
			       a_header.file_count() * constants::hash_size;
		}
	}

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

		[[nodiscard]] auto operator[](hashing::hash a_hash) noexcept -> index;
		[[nodiscard]] auto operator[](hashing::hash a_hash) const noexcept -> const_index;

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) noexcept -> index;

		template <detail::concepts::stringable String>
		[[nodiscard]] auto operator[](String&& a_path) const noexcept -> const_index;

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
		bool erase(String&& a_path) noexcept;

		[[nodiscard]] auto find(hashing::hash a_hash) noexcept -> iterator;
		[[nodiscard]] auto find(hashing::hash a_hash) const noexcept -> const_iterator;

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) noexcept -> iterator;

		template <detail::concepts::stringable String>
		[[nodiscard]] auto find(String&& a_path) const noexcept -> const_iterator;

		auto insert(file a_file) noexcept -> std::pair<iterator, bool>;

		bool read(std::filesystem::path a_path) noexcept
		{
			detail::istream_t in{ std::move(a_path) };
			if (!in.is_open()) {
				return false;
			}

			const auto header = [&]() noexcept {
				detail::header_t header;
				in >> header;
				return header;
			}();
			if (!header.good()) {
				return false;
			}

			clear();

			const auto dataOffset = detail::offsetof_file_data(header);
			_files.reserve(header.file_count());
			for (std::size_t i = 0; i < header.file_count(); ++i) {
				read_file(in, header, i, dataOffset);
			}

			const auto nameOffset = detail::offsetof_names(header);
			for (auto& file : _files) {
				file.read_filename(in, nameOffset);
			}
		}

		[[nodiscard]] auto size() const noexcept -> std::size_t { return _files.size(); }

		[[nodiscard]] bool verify_offsets() const noexcept;

		bool write(std::filesystem::path a_path) const noexcept;

	private:
		void read_file(
			detail::istream_t& a_in,
			const detail::header_t& a_header,
			std::size_t a_idx,
			std::size_t a_dataOffset) noexcept
		{
			const auto hash = [&]() noexcept {
				const detail::restore_point _{ a_in };
				a_in.seek_absolute(detail::offsetof_hashes(a_header));
				a_in.seek_relative(detail::constants::hash_size * a_idx);
				hashing::hash h;
				a_in >> h;
				return h;
			}();

			[[maybe_unused]] const auto [it, success] = _files.emplace(hash);
			assert(success);

			it->read(a_in, a_dataOffset);
		}

		container_type _files;
	};
}
