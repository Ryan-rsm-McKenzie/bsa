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
				header_t& a_header) noexcept
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
					a_header._good = false;
				} else if (a_header._version != 103 &&
						   a_header._version != 104 &&
						   a_header._version != 105) {
					a_header._good = false;
				} else if (a_header._directoriesOffset != constants::header_size) {
					a_header._good = false;
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

			[[nodiscard]] bool good() const noexcept { return _good; }

			[[nodiscard]] auto directories_offset() const noexcept
				-> std::size_t { return _directoriesOffset; }
			[[nodiscard]] auto endian() const noexcept -> std::endian { return _endian; }
			[[nodiscard]] auto archive_version() const noexcept -> std::size_t { return _version; }

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
				-> bool { return test_flag(archive_flag::xbox_compressed); }

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
			bool _good{ true };
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
			std::endian a_endian) noexcept
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

	bool file::compress(version a_version) noexcept
	{
		assert(!this->compressed());

		const auto in = this->as_bytes();
		std::vector<std::byte> out;

		switch (detail::to_underlying(a_version)) {
		case 103:
		case 104:
			{
				auto outsz = ::compressBound(static_cast<::uLong>(in.size()));
				out.resize(outsz);

				const auto result = ::compress(
					reinterpret_cast<::Byte*>(out.data()),
					&outsz,
					reinterpret_cast<const ::Byte*>(in.data()),
					static_cast<::uLong>(in.size_bytes()));
				if (result == Z_OK) {
					out.resize(outsz);
					out.shrink_to_fit();
				} else {
					return false;
				}
			}
			break;
		case 105:
			{
				::LZ4F_preferences_t pref = LZ4F_INIT_PREFERENCES;
				pref.compressionLevel = LZ4HC_CLEVEL_DEFAULT;
				pref.autoFlush = 1;
				out.resize(::LZ4F_compressFrameBound(in.size(), &pref));

				const auto result = ::LZ4F_compressFrame(
					out.data(),
					out.size(),
					in.data(),
					in.size(),
					&pref);
				if (!::LZ4F_isError(result)) {
					out.resize(result);
					out.shrink_to_fit();
				} else {
					return false;
				}
			}
			break;
		default:
			detail::declare_unreachable();
		}

		this->set_data(std::move(out), in.size_bytes());

		assert(this->compressed());
		return true;
	}

	bool file::decompress(version a_version) noexcept
	{
		assert(this->compressed());

		const auto in = this->as_bytes();
		std::vector<std::byte> out;
		out.resize(this->decompressed_size());

		switch (detail::to_underlying(a_version)) {
		case 103:
		case 104:
			{
				auto outsz = static_cast<::uLong>(out.size());

				const auto result = ::uncompress(
					reinterpret_cast<::Byte*>(out.data()),
					&outsz,
					reinterpret_cast<const ::Byte*>(in.data()),
					static_cast<::uLong>(in.size_bytes()));
				if (result == Z_OK) {
					assert(static_cast<std::size_t>(outsz) == this->decompressed_size());
				} else {
					return false;
				}
			}
			break;
		case 105:
			{
				::LZ4F_dctx* pdctx = nullptr;
				if (::LZ4F_createDecompressionContext(&pdctx, LZ4F_VERSION) != 0) {
					return false;
				}
				std::unique_ptr<::LZ4F_dctx, decltype(&::LZ4F_freeDecompressionContext)> dctx{
					pdctx,
					::LZ4F_freeDecompressionContext
				};

				std::size_t insz = 0;
				const std::byte* inptr = in.data();
				std::size_t outsz = 0;
				std::byte* outptr = out.data();
				const auto options = []() {
					::LZ4F_decompressOptions_t options = {};
					options.stableDst = true;
					return options;
				}();
				std::size_t result = 0;
				do {
					inptr += insz;
					insz = static_cast<std::size_t>(std::to_address(in.end()) - inptr);
					outptr += outsz;
					outsz = static_cast<std::size_t>(std::to_address(out.end()) - outptr);
					result = ::LZ4F_decompress(
						dctx.get(),
						outptr,
						&outsz,
						inptr,
						&insz,
						&options);
				} while (result != 0 && !::LZ4F_isError(result));

				if (!::LZ4F_isError(result)) {
					assert(outptr + outsz == std::to_address(out.end()));
				} else {
					return false;
				}
			}
			break;
		default:
			detail::declare_unreachable();
		}

		this->set_data(std::move(out));

		assert(!this->compressed());
		return true;
	}

	void file::read_data(
		detail::istream_t& a_in,
		const detail::header_t& a_header,
		std::size_t a_size) noexcept
	{
		std::optional<std::size_t> decompsz;
		const bool compressed =
			a_size & icompression ?
                !a_header.compressed() :
                a_header.compressed();
		if (compressed) {
			std::uint32_t size = 0;
			a_in >> size;
			decompsz = size;
			a_size -= 4;
		}
		a_size &= ~(ichecked | icompression);

		this->set_data(a_in.read_bytes(a_size), a_in, decompsz);
	}

	void file::write_data(detail::ostream_t& a_out) const noexcept
	{
		if (this->compressed()) {
			a_out << static_cast<std::uint32_t>(this->decompressed_size());
		}

		a_out.write_bytes(as_bytes());
	}

	void directory::read_file_names(detail::istream_t& a_in) noexcept
	{
		for ([[maybe_unused]] auto& [key, file] : *this) {
			const_cast<file::key&>(key).set_name(
				detail::read_zstring(a_in),
				a_in);
		}
	}

	auto directory::read_files(
		detail::istream_t& a_in,
		const detail::header_t& a_header,
		std::size_t a_count) noexcept
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

			const auto fname = [&]() noexcept -> std::string_view {
				if (a_header.embedded_file_names()) {
					auto name = detail::read_bstring(a_in);
					const auto pos = name.find_last_of("\\/"sv);
					if (pos != std::string_view::npos) {
						if (!dirname) {
							dirname = name.substr(0, pos);
						}
						name = name.substr(pos + 1);
					}

					size -= static_cast<std::uint32_t>(name.length() + 1u);
					return name;
				} else {
					return {};
				}
			}();

			[[maybe_unused]] const auto [it, success] =
				this->emplace(
					std::piecewise_construct,
					std::forward_as_tuple(hash, fname, a_in),
					std::forward_as_tuple());
			assert(success);

			it->second.read_data(a_in, a_header, size);
		}

		return dirname;
	}

	void directory::write_file_data(
		detail::ostream_t& a_out,
		const detail::header_t& a_header,
		std::string_view a_dirname) const noexcept
	{
		std::span dirbytes{
			reinterpret_cast<const std::byte*>(a_dirname.data()),
			a_dirname.size()
		};

		for (const auto& [key, file] : *this) {
			if (a_header.embedded_file_names()) {
				const auto fname = key.name();
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

			file.write_data(a_out);
		}
	}

	void directory::write_file_entries(
		detail::ostream_t& a_out,
		const detail::header_t& a_header,
		std::uint32_t& a_dataOffset) const noexcept
	{
		for (const auto& [key, file] : *this) {
			key.hash().write(a_out, a_header.endian());
			const auto fsize = file.size();
			if (!!a_header.compressed() != !!file.compressed()) {
				a_out << (static_cast<std::uint32_t>(fsize) | file::icompression);
			} else {
				a_out << static_cast<std::uint32_t>(fsize);
			}
			a_out << a_dataOffset;

			if (a_header.embedded_file_names()) {
				a_dataOffset += static_cast<std::uint32_t>(
					key.name().length() +
					1u);  // prefixed byte length
			}
			if (file.compressed()) {
				a_dataOffset += 4;
			}
			a_dataOffset += static_cast<std::uint32_t>(fsize);
		}
	}

	void directory::write_file_names(detail::ostream_t& a_out) const noexcept
	{
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			detail::write_zstring(a_out, key.name());
		}
	}

	auto archive::read(std::filesystem::path a_path) noexcept
		-> std::optional<version>
	{
		detail::istream_t in{ std::move(a_path) };
		if (!in.is_open()) {
			return std::nullopt;
		}

		const auto header = [&]() noexcept {
			detail::header_t result;
			in >> result;
			return result;
		}();
		if (!header.good()) {
			return std::nullopt;
		}

		this->clear();

		_flags = header.archive_flags();
		_types = header.archive_types();

		in.seek_absolute(header.directories_offset());
		for (std::size_t i = 0; i < header.directory_count(); ++i) {
			this->read_directory(in, header);
		}

		if (header.file_strings() && !header.embedded_file_names()) {
			this->read_file_names(in, header);
		}

		return { static_cast<version>(header.archive_version()) };
	}

	[[nodiscard]] bool archive::verify_offsets(version a_version) const noexcept
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

	bool archive::write(std::filesystem::path a_path, version a_version) const noexcept
	{
		detail::ostream_t out{ std::move(a_path) };
		if (!out.is_open()) {
			return false;
		}

		const auto header = this->make_header(a_version);
		out << header;

		this->write_directory_entries(out, header);
		this->write_file_entries(out, header);
		if (header.file_strings()) {
			this->write_file_names(out);
		}
		this->write_file_data(out, header);

		return true;
	}

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

	void archive::read_file_names(
		detail::istream_t& a_in,
		const detail::header_t& a_header) noexcept
	{
		a_in.seek_absolute(detail::offsetof_file_strings(a_header));
		for ([[maybe_unused]] auto& [key, dir] : *this) {
			dir.read_file_names(a_in);
		}
	}

	void archive::read_directory(
		detail::istream_t& a_in,
		const detail::header_t& a_header) noexcept
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

		const detail::restore_point _{ a_in };
		a_in.seek_absolute(offset - a_header.file_names_length());

		const auto name =
			a_header.directory_strings() ? detail::read_bzstring(a_in) : ""sv;

		directory d;
		const auto backup = d.read_files(a_in, a_header, count);
		[[maybe_unused]] const auto [it, success] =
			this->emplace(
				std::piecewise_construct,
				std::forward_as_tuple(hash, backup.value_or(name), a_in),
				std::forward_as_tuple(std::move(d)));
		assert(success);
	}

	void archive::write_directory_entries(
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		auto offset = static_cast<std::uint32_t>(detail::offsetof_file_entries(a_header));
		offset += static_cast<std::uint32_t>(a_header.file_names_length());

		for (const auto& [key, dir] : *this) {
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
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		for (const auto& [key, dir] : *this) {
			dir.write_file_data(a_out, a_header, key.name());
		}
	}

	void archive::write_file_entries(
		detail::ostream_t& a_out,
		const detail::header_t& a_header) const noexcept
	{
		auto offset = static_cast<std::uint32_t>(detail::offsetof_file_data(a_header));
		for ([[maybe_unused]] const auto& [key, dir] : *this) {
			if (a_header.directory_strings()) {
				detail::write_bzstring(a_out, key.name());
			}

			dir.write_file_entries(a_out, a_header, offset);
		}
	}

	void archive::write_file_names(detail::ostream_t& a_out) const noexcept
	{
		for ([[maybe_unused]] const auto& [key, dir] : *this) {
			dir.write_file_names(a_out);
		}
	}
}
