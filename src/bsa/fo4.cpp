#include "bsa/fo4.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <binary_io/any_stream.hpp>
#include <binary_io/file_stream.hpp>
#include <zlib.h>

namespace bsa::fo4
{
	namespace detail
	{
		namespace constants
		{
			namespace
			{
				constexpr auto btdx = make_four_cc("BTDX"sv);

				constexpr std::size_t header_size = 0x18;

				constexpr std::size_t chunk_header_size_gnrl = 0x10;
				constexpr std::size_t chunk_header_size_dx10 = 0x18;

				constexpr std::size_t chunk_size_gnrl = 0x14;
				constexpr std::size_t chunk_size_dx10 = 0x18;

				constexpr std::size_t chunk_sentinel = 0xBAADF00D;
			}
		}

		class header_t final
		{
		public:
			header_t() noexcept = default;

			header_t(
				format a_format,
				std::size_t a_fileCount,
				std::uint64_t a_stringTableOffset) noexcept :
				_format(to_underlying(a_format)),
				_fileCount(static_cast<std::uint32_t>(a_fileCount)),
				_stringTableOffset(a_stringTableOffset)
			{}

			friend auto operator>>(
				istream_t& a_in,
				header_t& a_header)
				-> istream_t&
			{
				std::uint32_t magic = 0;
				std::uint32_t version = 0;

				a_in->read(
					magic,
					version,
					a_header._format,
					a_header._fileCount,
					a_header._stringTableOffset);

				if (magic != constants::btdx) {
					throw exception("invalid magic");
				} else if (version != 1) {
					throw exception("invalid version");
				} else if (a_header._format != constants::gnrl &&
						   a_header._format != constants::dx10) {
					throw exception("invalid format");
				}

				return a_in;
			}

			friend auto operator<<(
				ostream_t& a_out,
				const header_t& a_header) noexcept
				-> ostream_t&
			{
				a_out.write(
					constants::btdx,
					std::uint32_t{ 1 },
					a_header._format,
					a_header._fileCount,
					a_header._stringTableOffset);
				return a_out;
			}

			[[nodiscard]] auto archive_format() const noexcept -> std::size_t { return _format; }
			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }
			[[nodiscard]] auto string_table_offset() const noexcept
				-> std::uint64_t { return _stringTableOffset; }

		private:
			std::uint32_t _format{ 0 };
			std::uint32_t _fileCount{ 0 };
			std::uint64_t _stringTableOffset{ 0 };
		};
	}

	namespace hashing
	{
		namespace
		{
			struct split_t
			{
				std::string_view parent;
				std::string_view stem;
				std::string_view extension;
			};

			[[nodiscard]] auto split_path(std::string_view a_path) noexcept
				-> split_t
			{
				const auto find = [&](char a_ch) noexcept {
					const auto pos = a_path.find_last_of(a_ch);
					return pos != std::string_view::npos ?
                               std::optional{ pos } :
                               std::nullopt;
				};

				split_t result;
				const auto pstem = find('\\');
				const auto pextension = find('.');

				if (pstem) {
					result.parent = a_path.substr(0, *pstem);
				}

				if (pextension) {
					result.extension = a_path.substr(*pextension + 1);  // don't include '.'
				}

				const auto first = pstem ? *pstem + 1 : 0;
				const auto last = pextension ?
                                      *pextension - first :
                                      pextension.value_or(std::string_view::npos);
				result.stem = a_path.substr(first, last);

				return result;
			}

			[[nodiscard]] auto crc32(std::string_view a_string) noexcept
				-> std::uint32_t
			{
				constexpr std::array<std::uint32_t, 256> lut = {
					0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
					0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
					0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
					0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
					0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
					0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
					0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
					0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
					0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
					0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
					0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
					0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
					0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
					0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
					0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
					0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
					0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
					0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
					0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
					0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
					0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
					0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
					0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
					0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
					0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
					0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
					0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
					0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
					0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
					0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
					0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
					0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
				};

				std::uint32_t result = 0;
				for (const auto c : a_string) {
					result = (result >> 8u) ^ lut[(result ^ static_cast<unsigned char>(c)) & 0xFFu];
				}
				return result;
			}
		}

		auto operator>>(
			detail::istream_t& a_in,
			hash& a_hash)
			-> detail::istream_t&
		{
			a_in->read(a_hash.file, a_hash.extension, a_hash.directory);
			return a_in;
		}

		auto operator<<(
			detail::ostream_t& a_out,
			const hash& a_hash) noexcept
			-> detail::ostream_t&
		{
			a_out.write(a_hash.file, a_hash.extension, a_hash.directory);
			return a_out;
		}

		hash hash_file_in_place(std::string& a_path) noexcept
		{
			detail::normalize_path(a_path);
			const auto pieces = split_path(a_path);

			hash h;
			h.directory = crc32(pieces.parent);
			h.file = crc32(pieces.stem);

			const auto len = std::min<std::size_t>(pieces.extension.length(), 4u);
			for (std::size_t i = 0; i < len; ++i) {
				h.extension |=
					std::uint32_t{ static_cast<unsigned char>(pieces.extension[i]) }
					<< i * 8u;
			}

			return h;
		}
	}

	auto operator>>(
		detail::istream_t& a_in,
		chunk::mips_t& a_mips)
		-> detail::istream_t&
	{
		a_in->read(a_mips.first, a_mips.last);
		return a_in;
	}

	auto operator<<(
		detail::ostream_t& a_out,
		const chunk::mips_t& a_mips) noexcept
		-> detail::ostream_t&
	{
		a_out.write(a_mips.first, a_mips.last);
		return a_out;
	}

	bool chunk::compress() noexcept
	{
		std::vector<std::byte> out;
		out.resize(this->compress_bound());

		const auto outsz = this->compress_into({ out.data(), out.size() });
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

	auto chunk::compress_bound() const noexcept
		-> std::size_t
	{
		assert(!this->compressed());
		return ::compressBound(static_cast<::uLong>(this->size()));
	}

	auto chunk::compress_into(std::span<std::byte> a_out) noexcept
		-> std::optional<std::size_t>
	{
		assert(!this->compressed());
		assert(a_out.size_bytes() >= this->compress_bound());

		const auto in = this->as_bytes();
		auto outsz = static_cast<::uLong>(a_out.size());

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

	bool chunk::decompress() noexcept
	{
		std::vector<std::byte> out;
		out.resize(this->decompressed_size());
		if (this->decompress_into({ out.data(), out.size() })) {
			this->set_data(std::move(out));

			assert(!this->compressed());
			return true;
		} else {
			assert(this->compressed());
			return false;
		}
	}

	bool chunk::decompress_into(std::span<std::byte> a_out) noexcept
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

	auto operator>>(
		detail::istream_t& a_in,
		file::header_t& a_header)
		-> detail::istream_t&
	{
		a_in->read(
			a_header.height,
			a_header.width,
			a_header.mip_count,
			a_header.format,
			a_header.flags,
			a_header.tile_mode);
		return a_in;
	}

	auto operator<<(
		detail::ostream_t& a_out,
		const file::header_t& a_header) noexcept
		-> detail::ostream_t&
	{
		a_out.write(
			a_header.height,
			a_header.width,
			a_header.mip_count,
			a_header.format,
			a_header.flags,
			a_header.tile_mode);
		return a_out;
	}

	auto archive::read(std::filesystem::path a_path)
		-> format
	{
		detail::istream_t in{ std::move(a_path) };
		return this->do_read(in);
	}

	auto archive::read(
		std::span<const std::byte> a_src,
		copy_type a_copy)
		-> format
	{
		detail::istream_t in{ a_src, a_copy };
		return this->do_read(in);
	}

	void archive::write(
		std::filesystem::path a_path,
		format a_format,
		bool a_strings) const
	{
		binary_io::any_ostream out{ std::in_place_type<binary_io::file_ostream>, std::move(a_path) };
		this->do_write(out, a_format, a_strings);
	}

	void archive::write(
		binary_io::any_ostream& a_dst,
		format a_format,
		bool a_strings) const
	{
		this->do_write(a_dst, a_format, a_strings);
	}

	auto archive::do_read(detail::istream_t& a_in)
		-> format
	{
		const auto header = [&]() {
			detail::header_t result;
			a_in >> result;
			return result;
		}();

		this->clear();
		const auto fmt = static_cast<format>(header.archive_format());

		for (std::size_t i = 0, strpos = header.string_table_offset();
			 i < header.file_count();
			 ++i) {
			hashing::hash hash;
			a_in >> hash;

			const auto name = [&]() {
				if (strpos != 0) {
					const detail::restore_point _{ a_in };
					a_in->seek_absolute(strpos);
					const auto name = detail::read_wstring(a_in);
					strpos = a_in->tell();
					return name;
				} else {
					return ""sv;
				}
			}();

			[[maybe_unused]] const auto [it, success] =
				this->insert(
					key_type{ hash, name, a_in },
					mapped_type{});
			assert(success);

			this->read_file(it->second, a_in, fmt);
		}

		return fmt;
	}

	void archive::do_write(
		detail::ostream_t& a_out,
		format a_format,
		bool a_strings) const
	{
		auto [header, dataOffset] = make_header(a_format, a_strings);
		a_out << header;

		for (const auto& [key, file] : *this) {
			a_out << key.hash();
			this->write_file(file, a_out, a_format, dataOffset);
		}

		for (const auto& file : *this) {
			for (const auto& chunk : file.second) {
				a_out.write_bytes(chunk.as_bytes());
			}
		}

		if (a_strings) {
			for ([[maybe_unused]] const auto& [key, file] : *this) {
				detail::write_wstring(a_out, key.name());
			}
		}
	}

	auto archive::make_header(
		format a_format,
		bool a_strings) const noexcept
		-> std::pair<detail::header_t, std::uint64_t>
	{
		const auto inspect = [&](auto a_gnrl, auto a_dx10) noexcept {
			switch (a_format) {
			case format::general:
				return a_gnrl();
			case format::directx:
				return a_dx10();
			default:
				detail::declare_unreachable();
			}
		};

		std::uint64_t dataOffset =
			detail::constants::header_size +
			inspect(
				[]() noexcept { return detail::constants::chunk_header_size_gnrl; },
				[]() noexcept { return detail::constants::chunk_header_size_dx10; }) *
				this->size();
		std::uint64_t dataSize = 0;
		for ([[maybe_unused]] const auto& [key, file] : *this) {
			dataOffset +=
				inspect(
					[]() noexcept { return detail::constants::chunk_size_gnrl; },
					[]() noexcept { return detail::constants::chunk_size_dx10; }) *
				file.size();
			for (const auto& chunk : file) {
				dataSize += chunk.size();
			}
		}

		return {
			detail::header_t{
				a_format,
				this->size(),
				a_strings ? dataOffset + dataSize : 0u },
			dataOffset
		};
	}

	void archive::read_chunk(
		chunk& a_chunk,
		detail::istream_t& a_in,
		format a_format)
	{
		const auto [dataFileOffset, compressedSize, decompressedSize] =
			a_in->read<std::uint64_t, std::uint32_t, std::uint32_t>();

		std::size_t size = 0;
		std::optional<std::size_t> decompsz;
		if (compressedSize != 0) {
			decompsz = decompressedSize;
			size = compressedSize;
		} else {
			size = decompressedSize;
		}

		if (a_format == format::directx) {
			a_in >> a_chunk.mips;
		}

		const auto [sentinel] = a_in->read<std::uint32_t>();
		if (sentinel != detail::constants::chunk_sentinel) {
			throw exception("invalid chunk sentinel");
		}

		const detail::restore_point _{ a_in };
		a_in->seek_absolute(dataFileOffset);
		a_chunk.set_data(
			a_in->read_bytes(size),
			a_in,
			decompsz);
	}

	void archive::read_file(
		file& a_file,
		detail::istream_t& a_in,
		format a_format)
	{
		a_in->seek_relative(1u);  // skip mod index
		const auto [count, hdrsz] = a_in->read<std::uint8_t, std::uint16_t>();

		switch (a_format) {
		case format::general:
			if (hdrsz != detail::constants::chunk_header_size_gnrl) {
				throw exception("invalid chunk header size");
			}
			break;
		case format::directx:
			if (hdrsz != detail::constants::chunk_header_size_dx10) {
				throw exception("invalid chunk header size");
			}
			a_in >> a_file.header;
			break;
		default:
			detail::declare_unreachable();
		}

		a_file.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			this->read_chunk(
				a_file.emplace_back(),
				a_in,
				a_format);
		}
	}

	void archive::write_chunk(
		const chunk& a_chunk,
		detail::ostream_t& a_out,
		format a_format,
		std::uint64_t& a_dataOffset) const noexcept
	{
		const auto size = a_chunk.size();
		a_out.write(
			a_dataOffset,
			static_cast<std::uint32_t>(a_chunk.compressed() ? size : 0u),
			static_cast<std::uint32_t>(
				a_chunk.compressed() ? a_chunk.decompressed_size() : size));
		a_dataOffset += size;

		if (a_format == format::directx) {
			a_out << a_chunk.mips;
		}

		a_out.write<std::uint32_t>(detail::constants::chunk_sentinel);
	}

	void archive::write_file(
		const file& a_file,
		detail::ostream_t& a_out,
		format a_format,
		std::uint64_t& a_dataOffset) const noexcept
	{
		a_out.write(
			std::byte{ 0 },  // skip mod index
			static_cast<std::uint8_t>(a_file.size()));
		switch (a_format) {
		case format::general:
			a_out.write<std::uint16_t>(detail::constants::chunk_header_size_gnrl);
			break;
		case format::directx:
			a_out.write<std::uint16_t>(detail::constants::chunk_header_size_dx10);
			a_out << a_file.header;
			break;
		default:
			detail::declare_unreachable();
		}

		for (const auto& chunk : a_file) {
			this->write_chunk(chunk, a_out, a_format, a_dataOffset);
		}
	}
}
