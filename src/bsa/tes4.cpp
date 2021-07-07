#include "bsa/tes4.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/endian/conversion.hpp>

#include <lz4frame.h>
#include <lz4hc.h>
#include <zlib.h>

namespace bsa::tes4
{
	namespace detail
	{
		namespace
		{
			namespace constants
			{
				constexpr std::size_t directory_entry_size_x86 = 0x10;
				constexpr std::size_t directory_entry_size_x64 = 0x18;
				constexpr std::size_t file_entry_size = 0x10;
				constexpr std::size_t header_size = 0x24;
			}

			constexpr auto lz4f_decompress_options = []() {
				::LZ4F_decompressOptions_t options = {};
				options.stableDst = true;
				return options;
			}();

			constexpr auto lz4f_preferences = []() noexcept {
				::LZ4F_preferences_t pref = LZ4F_INIT_PREFERENCES;
				pref.compressionLevel = LZ4HC_CLEVEL_DEFAULT;
				pref.autoFlush = 1;
				return pref;
			}();
		}

		class header_t final
		{
		public:
			struct info_t final
			{
				std::uint32_t count{ 0 };
				std::uint32_t blobsz{ 0 };
			};

			header_t() noexcept = default;

			header_t(
				version a_version,
				archive_flag a_flags,
				archive_type a_types,
				info_t a_directories,
				info_t a_files) noexcept :
				_version(to_underlying(a_version)),
				_archiveFlags(to_underlying(a_flags)),
				_directory(a_directories),
				_file(a_files),
				_archiveTypes(to_underlying(a_types))
			{
				this->evaluate_endian();
			}

			friend auto operator>>(
				istream_t& a_in,
				header_t& a_header)
				-> istream_t&
			{
				std::array<std::byte, 4> magic;

				a_in >>
					magic >>
					a_header._version >>
					a_header._directoriesOffset >>
					a_header._archiveFlags >>
					a_header._directory.count >>
					a_header._file.count >>
					a_header._directory.blobsz >>
					a_header._file.blobsz >>
					a_header._archiveTypes;
				a_in.seek_relative(2);

				a_header.evaluate_endian();

				if (magic[0] != std::byte{ 'B' } ||
					magic[1] != std::byte{ 'S' } ||
					magic[2] != std::byte{ 'A' } ||
					magic[3] != std::byte{ '\0' }) {
					throw exception("invalid magic");
				} else if (a_header._version != 103 &&
						   a_header._version != 104 &&
						   a_header._version != 105) {
					throw exception("unsupported version");
				} else if (a_header._directoriesOffset != constants::header_size) {
					throw exception("invalid header size");
				}

				return a_in;
			}

			friend auto operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
				-> ostream_t&
			{
				std::array magic{
					std::byte{ 'B' },
					std::byte{ 'S' },
					std::byte{ 'A' },
					std::byte{ '\0' }
				};

				return a_out
				       << magic
				       << a_header._version
				       << a_header._directoriesOffset
				       << a_header._archiveFlags
				       << a_header._directory.count
				       << a_header._file.count
				       << a_header._directory.blobsz
				       << a_header._file.blobsz
				       << a_header._archiveTypes
				       << std::uint16_t{ 0 };
			}

			[[nodiscard]] auto archive_version() const noexcept -> std::size_t { return _version; }
			[[nodiscard]] auto directories_offset() const noexcept
				-> std::size_t { return _directoriesOffset; }
			[[nodiscard]] auto endian() const noexcept -> std::endian { return _endian; }

			[[nodiscard]] auto directory_count() const noexcept
				-> std::size_t { return _directory.count; }
			[[nodiscard]] auto directory_names_length() const noexcept
				-> std::size_t { return _directory.blobsz; }

			[[nodiscard]] auto file_count() const noexcept
				-> std::size_t { return _file.count; }
			[[nodiscard]] auto file_names_length() const noexcept
				-> std::size_t { return _file.blobsz; }

			[[nodiscard]] auto archive_flags() const noexcept
				-> archive_flag { return archive_flag{ _archiveFlags }; }
			[[nodiscard]] auto archive_types() const noexcept
				-> archive_type { return archive_type{ _archiveTypes }; }

			[[nodiscard]] auto compressed() const noexcept
				-> bool { return test_flag(archive_flag::compressed); }
			[[nodiscard]] auto directory_strings() const noexcept
				-> bool { return test_flag(archive_flag::directory_strings); }
			[[nodiscard]] auto embedded_file_names() const noexcept
				-> bool { return _version > 103 && test_flag(archive_flag::embedded_file_names); }
			[[nodiscard]] auto file_strings() const noexcept
				-> bool { return test_flag(archive_flag::file_strings); }
			[[nodiscard]] auto xbox_archive() const noexcept
				-> bool { return test_flag(archive_flag::xbox_archive); }
			[[nodiscard]] auto xbox_compressed() const noexcept
				-> bool { return _version > 103 && test_flag(archive_flag::xbox_compressed); }

		private:
			[[nodiscard]] bool test_flag(archive_flag a_flag) const noexcept
			{
				return (_archiveFlags & to_underlying(a_flag)) != 0;
			}

			void evaluate_endian() noexcept
			{
				_endian =
					this->xbox_archive() ?
                        std::endian::big :
                        std::endian::little;
			}

			std::uint32_t _version{ 0 };
			std::uint32_t _directoriesOffset{ constants::header_size };
			std::uint32_t _archiveFlags{ 0 };
			info_t _directory;
			info_t _file;
			std::endian _endian{ std::endian::little };
			std::uint16_t _archiveTypes{ 0 };
		};

		namespace
		{
			[[nodiscard]] auto offsetof_directory_entries(
				const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				return a_header.directories_offset();
			}

			[[nodiscard]] auto offsetof_file_entries(
				const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				const auto dirsz = [](std::size_t a_version) noexcept {
					switch (a_version) {
					case 103:
					case 104:
						return constants::directory_entry_size_x86;
					case 105:
						return constants::directory_entry_size_x64;
					default:
						declare_unreachable();
					}
				}(a_header.archive_version());

				return offsetof_directory_entries(a_header) +
				       dirsz * a_header.directory_count();
			}

			[[nodiscard]] auto offsetof_file_strings(
				const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				const auto dirStrSz =
					a_header.directory_strings() ?
                        // include prefixed byte length
                        a_header.directory_names_length() + a_header.directory_count() :
                        0;

				return offsetof_file_entries(a_header) +
				       dirStrSz +
				       a_header.file_count() * constants::file_entry_size;
			}

			[[nodiscard]] auto offsetof_file_data(
				const detail::header_t& a_header) noexcept
				-> std::size_t
			{
				return offsetof_file_strings(a_header) +
				       a_header.file_names_length();
			}
		}
	}

	namespace hashing
	{
		namespace
		{
			[[nodiscard]] auto crc32(std::span<const std::byte> a_bytes)
				-> std::uint32_t
			{
				constexpr auto constant = std::uint32_t{ 0x1003Fu };
				std::uint32_t crc = 0;
				for (const auto c : a_bytes) {
					crc = static_cast<std::uint8_t>(c) + crc * constant;
				}
				return crc;
			}

			[[nodiscard]] constexpr auto make_file_extension(std::string_view a_extension) noexcept
				-> std::uint32_t
			{
				std::uint32_t ext = 0;
				for (std::size_t i = 0; i < std::min<std::size_t>(a_extension.size(), 4u); ++i) {
					ext |= std::uint32_t{ static_cast<unsigned char>(a_extension[i]) } << i * 8;
				}
				return ext;
			}
		}

		void hash::read(
			detail::istream_t& a_in,
			std::endian a_endian)
		{
			a_in >>
				last >>
				last2 >>
				length >>
				first;
			crc = a_in.read<decltype(crc)>(a_endian);
		}

		void hash::write(
			detail::ostream_t& a_out,
			std::endian a_endian) const noexcept
		{
			a_out << last
				  << last2
				  << length
				  << first;
			a_out.write(crc, a_endian);
		}

		hash hash_directory(std::string& a_path) noexcept
		{
			detail::normalize_path(a_path);
			const std::span<const std::byte> view{
				reinterpret_cast<const std::byte*>(a_path.data()),
				a_path.size()
			};

			hash h;

			switch (std::min<std::size_t>(view.size(), 3)) {
			case 3:
				h.last2 = static_cast<std::uint8_t>(*(view.end() - 2));
				[[fallthrough]];
			case 2:
			case 1:
				h.last = static_cast<std::uint8_t>(view.back());
				h.first = static_cast<std::uint8_t>(view.front());
				[[fallthrough]];
			default:
				break;
			}

			h.length = static_cast<std::uint8_t>(view.size());
			if (h.length > 3) {
				// skip first and last two chars -> already processed
				h.crc = crc32(view.subspan(1, view.size() - 3));
			}

			return h;
		}

		hash hash_file(std::string& a_path) noexcept
		{
			constexpr std::array lut{
				make_file_extension(""sv),
				make_file_extension(".nif"sv),
				make_file_extension(".kf"sv),
				make_file_extension(".dds"sv),
				make_file_extension(".wav"sv),
				make_file_extension(".adp"sv),
			};

			detail::normalize_path(a_path);
			if (const auto pos = a_path.find_last_of('\\'); pos != std::string::npos) {
				a_path = a_path.substr(pos + 1);
			}
			const std::string_view pview{ a_path };

			const auto [stem, extension] = [&]() noexcept
				-> std::pair<std::string_view, std::string_view> {
				const auto split = pview.find_last_of('.');
				if (split != std::string_view::npos) {
					return {
						pview.substr(0, split),
						pview.substr(split)
					};
				} else {
					return {
						pview,
						""sv
					};
				}
			}();

			if (!stem.empty() &&
				stem.length() < 260 &&
				extension.length() < 16) {
				auto h = [&]() noexcept {
					std::string temp{ stem };
					return hash_directory(temp);
				}();
				h.crc += crc32({ //
					reinterpret_cast<const std::byte*>(extension.data()),
					extension.size() });

				const auto it = std::find(
					lut.begin(),
					lut.end(),
					make_file_extension(extension));
				if (it != lut.end()) {
					const auto i = static_cast<std::uint8_t>(it - lut.begin());
					h.first += 32u * (i & 0xFCu);
					h.last += (i & 0xFEu) << 6u;
					h.last2 += i << 7u;
				}

				return h;
			} else {
				return {};
			}
		}
	}

	auto file::compress_into_lz4(std::span<std::byte> a_out) noexcept
		-> std::optional<std::size_t>
	{
		assert(!this->compressed());
		assert(a_out.size_bytes() >= this->compress_bound(version::sse));

		const auto in = this->as_bytes();

		const auto result = ::LZ4F_compressFrame(
			a_out.data(),
			a_out.size_bytes(),
			in.data(),
			in.size_bytes(),
			&detail::lz4f_preferences);
		if (!::LZ4F_isError(result)) {
			return result;
		} else {
			return std::nullopt;
		}
	}

	auto file::compress_into_zlib(std::span<std::byte> a_out) noexcept
		-> std::optional<std::size_t>
	{
		assert(!this->compressed());
		assert(a_out.size_bytes() >= this->compress_bound(version::tes4));

		const auto in = this->as_bytes();
		auto outsz = static_cast<::uLong>(a_out.size_bytes());

		const auto result = ::compress(
			reinterpret_cast<::Byte*>(a_out.data()),
			&outsz,
			reinterpret_cast<const ::Byte*>(in.data()),
			static_cast<::uLong>(in.size_bytes()));
		if (result == Z_OK) {
			return static_cast<std::size_t>(outsz);
		} else {
			return std::nullopt;
		}
	}

	bool file::decompress_into_lz4(std::span<std::byte> a_out) noexcept
	{
		assert(this->compressed());
		assert(a_out.size_bytes() >= this->decompressed_size());

		::LZ4F_dctx* pdctx = nullptr;
		if (::LZ4F_createDecompressionContext(&pdctx, LZ4F_VERSION) != 0) {
			return false;
		}
		std::unique_ptr<::LZ4F_dctx, decltype(&::LZ4F_freeDecompressionContext)> dctx{
			pdctx,
			::LZ4F_freeDecompressionContext
		};

		const auto in = this->as_bytes();

		std::size_t insz = 0;
		const std::byte* inptr = in.data();
		std::size_t outsz = 0;
		std::byte* outptr = a_out.data();
		std::size_t result = 0;
		do {
			inptr += insz;
			insz = static_cast<std::size_t>(std::to_address(in.end()) - inptr);
			outptr += outsz;
			outsz = static_cast<std::size_t>(std::to_address(a_out.end()) - outptr);
			result = ::LZ4F_decompress(
				dctx.get(),
				outptr,
				&outsz,
				inptr,
				&insz,
				&detail::lz4f_decompress_options);
		} while (result != 0 && !::LZ4F_isError(result));

		const auto totalsz = static_cast<std::size_t>(
			(outptr + outsz) - std::to_address(a_out.begin()));
		if (!::LZ4F_isError(result) && totalsz == this->decompressed_size()) {
			return true;
		} else {
			return false;
		}
	}

	bool file::decompress_into_zlib(std::span<std::byte> a_out) noexcept
	{
		assert(this->compressed());
		assert(a_out.size_bytes() >= this->decompressed_size());

		const auto in = this->as_bytes();
		auto outsz = static_cast<::uLong>(a_out.size_bytes());

		const auto result = ::uncompress(
			reinterpret_cast<::Byte*>(a_out.data()),
			&outsz,
			reinterpret_cast<const ::Byte*>(in.data()),
			static_cast<::uLong>(in.size_bytes()));
		if (result == Z_OK && outsz == this->decompressed_size()) {
			return true;
		} else {
			return false;
		}
	}

	bool file::compress(version a_version) noexcept
	{
		std::vector<std::byte> out;
		out.resize(this->compress_bound(a_version));

		const auto outsz = this->compress_into(a_version, { out.data(), out.size() });
		if (outsz) {
			out.resize(*outsz);
			out.shrink_to_fit();
			this->set_data(std::move(out), this->size());

			assert(this->compressed());
			return true;
		} else {
			assert(!this->compressed());
			return false;
		}
	}

	auto file::compress_bound(version a_version) const noexcept
		-> std::size_t
	{
		switch (detail::to_underlying(a_version)) {
		case 103:
		case 104:
			return ::compressBound(static_cast<::uLong>(this->size()));
		case 105:
			return ::LZ4F_compressFrameBound(this->size(), &detail::lz4f_preferences);
		default:
			detail::declare_unreachable();
		}
	}

	auto file::compress_into(
		version a_version,
		std::span<std::byte> a_out) noexcept
		-> std::optional<std::size_t>
	{
		switch (detail::to_underlying(a_version)) {
		case 103:
		case 104:
			return this->compress_into_zlib(a_out);
		case 105:
			return this->compress_into_lz4(a_out);
		default:
			detail::declare_unreachable();
		}
	}

	bool file::decompress(version a_version) noexcept
	{
		std::vector<std::byte> out;
		out.resize(this->decompressed_size());
		if (this->decompress_into(a_version, { out.data(), out.size() })) {
			this->set_data(std::move(out));

			assert(!this->compressed());
			return true;
		} else {
			assert(this->compressed());
			return false;
		}
	}

	bool file::decompress_into(
		version a_version,
		std::span<std::byte> a_out) noexcept
	{
		switch (detail::to_underlying(a_version)) {
		case 103:
		case 104:
			return this->decompress_into_zlib(a_out);
		case 105:
			return this->decompress_into_lz4(a_out);
		default:
			detail::declare_unreachable();
		}
	}

	auto archive::read(std::filesystem::path a_path)
		-> version
	{
		detail::istream_t in{ std::move(a_path) };
		const auto header = [&]() {
			detail::header_t result;
			in >> result;
			return result;
		}();

		this->clear();

		_flags = header.archive_flags();
		_types = header.archive_types();

		std::size_t namesOffset = detail::offsetof_file_strings(header);
		in.seek_absolute(header.directories_offset());
		for (std::size_t i = 0; i < header.directory_count(); ++i) {
			this->read_directory(in, header, namesOffset);
		}

		return static_cast<version>(header.archive_version());
	}

	bool archive::verify_offsets(version a_version) const noexcept
	{
		const auto header = this->make_header(a_version);
		auto offset = detail::offsetof_file_data(header);

		std::size_t last = 0;
		for (const auto& dir : *this) {
			for (const auto& [key, file] : dir.second) {
				last = key.name().length() +
				       1u;  // prefixed byte length
				if (file.compressed()) {
					last += 4u;
				}
				last += file.size();
				offset += last;
			}
		}

		offset -= last;
		return offset <= std::numeric_limits<std::int32_t>::max();
	}

	void archive::write(std::filesystem::path a_path, version a_version) const
	{
		detail::ostream_t out{ std::move(a_path) };
		const auto header = this->make_header(a_version);
		out << header;

		const auto intermediate = sort_for_write(header.xbox_archive());

		this->write_directory_entries(intermediate, out, header);
		this->write_file_entries(intermediate, out, header);
		if (header.file_strings()) {
			this->write_file_names(intermediate, out);
		}
		this->write_file_data(intermediate, out, header);
	}

	struct archive::xbox_sort_t final
	{
		// i legitimately have no idea how they sort hashes in the xbox format
		// it simply defies all reason

		template <class T>
		[[nodiscard]] bool operator()(
			const T& a_lhs,
			const T& a_rhs) const noexcept
		{
			return key_for(a_lhs) < key_for(a_rhs);
		}

		[[nodiscard]] static auto key_for(
			const intermediate_t::value_type& a_value) noexcept
			-> std::uint64_t
		{
			return boost::endian::endian_reverse(
				a_value.first->first.hash().numeric());
		}

		[[nodiscard]] static auto key_for(
			const mapped_type::value_type* a_value) noexcept
			-> std::uint64_t
		{
			return boost::endian::endian_reverse(
				a_value->first.hash().numeric());
		}
	};

	auto archive::make_header(version a_version) const noexcept
		-> detail::header_t
	{
		detail::header_t::info_t files;
		detail::header_t::info_t dirs;

		for (const auto& dir : *this) {
			dirs.count += 1;

			if (this->directory_strings()) {
				dirs.blobsz += static_cast<std::uint32_t>(
					dir.first.name().length() +
					1u);  // null terminator
			}

			for (const auto& file : dir.second) {
				files.count += 1;

				if (this->file_strings()) {
					files.blobsz += static_cast<std::uint32_t>(
						file.first.name().length() +
						1u);  // null terminator
				}
			}
		}

		return {
			a_version,
			_flags,
			_types,
			dirs,
			files
		};
	}

	auto archive::read_file_entries(
		directory& a_dir,
		detail::istream_t& a_in,
		const detail::header_t& a_header,
		std::size_t a_count,
		std::size_t& a_namesOffset)
		-> std::optional<std::string_view>
	{
		std::optional<std::string_view> dirname;

		for (std::size_t i = 0; i < a_count; ++i) {
			hashing::hash hash;
			hash.read(a_in, a_header.endian());

			std::uint32_t size = 0;
			std::uint32_t offset = 0;
			a_in >> size >> offset;

			const detail::restore_point _{ a_in };
			a_in.seek_absolute(offset & ~file::isecondary_archive);

			const auto fname = [&]() -> std::string_view {
				if (a_header.embedded_file_names()) {
					auto name = detail::read_bstring(a_in);
					size -= static_cast<std::uint32_t>(name.length() + 1u);
					const auto pos = name.find_last_of("\\/"sv);
					if (pos != std::string_view::npos) {
						if (!dirname) {
							dirname = name.substr(0, pos);
						}
						name = name.substr(pos + 1);
					}

					return name;
				} else if (a_header.file_strings()) {
					const detail::restore_point r{ a_in };
					a_in.seek_absolute(a_namesOffset);
					const auto name = detail::read_zstring(a_in);
					a_namesOffset = a_in.tell();
					return name;
				} else {
					return {};
				}
			}();

			[[maybe_unused]] const auto [it, success] =
				a_dir.insert(
					directory::key_type{ hash, fname, a_in },
					directory::mapped_type{});
			assert(success);

			this->read_file_data(it->second, a_in, a_header, size);
		}

		return dirname;
	}

	void archive::read_file_data(
		file& a_file,
		detail::istream_t& a_in,
		const detail::header_t& a_header,
		std::size_t a_size)
	{
		std::optional<std::size_t> decompsz;
		const bool compressed =
			a_size & file::icompression ?
                !a_header.compressed() :
                a_header.compressed();
		if (compressed) {
			std::uint32_t size = 0;
			a_in >> size;
			decompsz = size;
			a_size -= 4;
		}
		a_size &= ~(file::ichecked | file::icompression);

		a_file.set_data(a_in.read_bytes(a_size), a_in, decompsz);
	}

	void archive::read_directory(
		detail::istream_t& a_in,
		const detail::header_t& a_header,
		std::size_t& a_namesOffset)
	{
		hashing::hash hash;
		hash.read(a_in, a_header.endian());

		std::uint32_t count = 0;
		a_in >> count;

		std::uint32_t offset = 0;
		switch (a_header.archive_version()) {
		case 103:
		case 104:
			a_in >> offset;
			break;
		case 105:
			a_in.seek_relative(4u);
			a_in >> offset;
			a_in.seek_relative(4u);
			break;
		default:
			detail::declare_unreachable();
		}

		if (a_header.file_strings()) {
			offset -= a_header.file_names_length();
		}

		const detail::restore_point _{ a_in };
		a_in.seek_absolute(offset);

		const auto name =
			a_header.directory_strings() ? detail::read_bzstring(a_in) : ""sv;

		directory d;
		const auto backup = this->read_file_entries(d, a_in, a_header, count, a_namesOffset);
		[[maybe_unused]] const auto [it, success] =
			this->insert(
				key_type{ hash, backup.value_or(name), a_in },
				std::move(d));
		assert(success);
	}

	auto archive::sort_for_write(bool a_xbox) const noexcept
		-> intermediate_t
	{
		intermediate_t result;
		result.reserve(this->size());
		for (const auto& dir : *this) {
			auto& elem = result.emplace_back();
			elem.first = &dir;
			elem.second.reserve(dir.second.size());
			for (const auto& file : dir.second) {
				elem.second.push_back(&file);
			}

			if (a_xbox) {
				std::sort(elem.second.begin(), elem.second.end(), xbox_sort_t{});
			}
		}

		if (a_xbox) {
			std::sort(result.begin(), result.end(), xbox_sort_t{});
		}

		return result;
	}

	void archive::write_directory_entries(
		const intermediate_t& a_intermediate,
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		auto offset = static_cast<std::uint32_t>(detail::offsetof_file_entries(a_header));
		offset += static_cast<std::uint32_t>(a_header.file_names_length());

		for (const auto& elem : a_intermediate) {
			const auto& [key, dir] = *elem.first;
			key.hash().write(a_out, a_header.endian());
			a_out << static_cast<std::uint32_t>(dir.size());

			switch (a_header.archive_version()) {
			case 103:
			case 104:
				a_out << offset;
				break;
			case 105:
				a_out
					<< std::uint32_t{ 0 }
					<< offset
					<< std::uint32_t{ 0 };
				break;
			default:
				detail::declare_unreachable();
			}

			if (a_header.directory_strings()) {
				offset += static_cast<std::uint32_t>(
					key.name().length() +
					1u +  // prefixed byte length
					1u);  // null terminator
			}

			offset += static_cast<std::uint32_t>(
				detail::constants::file_entry_size *
				dir.size());
		}
	}

	void archive::write_file_data(
		const intermediate_t& a_intermediate,
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		for (const auto& elem : a_intermediate) {
			const auto& dir = *elem.first;
			const auto dirname = dir.first.name();
			std::span dirbytes{
				reinterpret_cast<const std::byte*>(dirname.data()),
				dirname.size()
			};

			for (const auto file : elem.second) {
				if (a_header.embedded_file_names()) {
					const auto fname = file->first.name();
					const auto len = dirbytes.size() +
					                 1u +  // directory separator
					                 fname.size();
					a_out << static_cast<std::uint8_t>(len);
					a_out.write_bytes(dirbytes);
					a_out << std::byte{ '\\' };
					a_out.write_bytes({ //
						reinterpret_cast<const std::byte*>(fname.data()),
						fname.size() });
				}

				if (file->second.compressed()) {
					a_out << static_cast<std::uint32_t>(file->second.decompressed_size());
				}

				a_out.write_bytes(file->second.as_bytes());
			}
		}
	}

	void archive::write_file_entries(
		const intermediate_t& a_intermediate,
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		auto offset = static_cast<std::uint32_t>(detail::offsetof_file_data(a_header));
		for (const auto& elem : a_intermediate) {
			const auto& dir = *elem.first;
			if (a_header.directory_strings()) {
				detail::write_bzstring(a_out, dir.first.name());
			}

			for (const auto file : elem.second) {
				file->first.hash().write(a_out, a_header.endian());
				auto fsize = file->second.size();

				if (!!a_header.compressed() != !!file->second.compressed()) {
					fsize |= file::icompression;
				}

				if (a_header.embedded_file_names()) {
					fsize += static_cast<std::uint32_t>(
						1u +  // prefixed byte length
						dir.first.name().length() +
						1u +  // directory separator
						file->first.name().length());
				}

				if (file->second.compressed()) {
					fsize += 4u;
				}

				a_out << static_cast<std::uint32_t>(fsize);
				a_out << offset;
				offset += static_cast<std::uint32_t>(fsize & ~file::icompression);
			}
		}
	}

	void archive::write_file_names(
		const intermediate_t& a_intermediate,
		detail::ostream_t& a_out) const noexcept
	{
		for (const auto& elem : a_intermediate) {
			for (const auto file : elem.second) {
				detail::write_zstring(a_out, file->first.name());
			}
		}
	}
}
