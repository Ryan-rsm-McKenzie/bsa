#include "bsa/fo4.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <binary_io/any_stream.hpp>
#include <binary_io/file_stream.hpp>
#include <lz4.h>
#include <lz4hc.h>
#include <zlib.h>

#include <DirectXTex.h>

namespace bsa::fo4
{
	namespace detail
	{
		namespace constants
		{
			namespace
			{
				constexpr auto btdx = make_four_cc("BTDX"sv);

				constexpr std::size_t chunk_header_size_gnrl = 0x10;
				constexpr std::size_t chunk_header_size_dx10 = 0x18;

				constexpr std::size_t chunk_size_gnrl = 0x14;
				constexpr std::size_t chunk_size_dx10 = 0x18;

				constexpr std::size_t chunk_sentinel = 0xBAADF00D;

				constexpr std::uint32_t compression_lz4 = 3;
			}
		}

		namespace
		{
			template <std::size_t MAX_COUNT>
			[[nodiscard]] auto chunk(
				std::span<const DirectX::Image> a_range,
				std::size_t a_chunksz) noexcept
			{
				static_assert(MAX_COUNT > 0);

				std::vector<std::span<const DirectX::Image>> result;
				if (a_range.empty()) {
					return result;
				}

				result.reserve(MAX_COUNT);
				std::size_t start = 0;
				std::size_t size = 0;
				std::size_t i = 0;

				for (; i < a_range.size(); ++i) {
					const auto& image = a_range[i];
					if (size == 0 || size + image.slicePitch < a_chunksz) {
						size += image.slicePitch;
					} else {
						result.push_back(a_range.subspan(start, i - start));
						start = i;
						size = image.slicePitch;
					}
				}

				result.push_back(a_range.subspan(start, i - start));
				if (result.size() > MAX_COUNT) {
					result.erase(result.begin() + MAX_COUNT, result.end());
					const auto pos = static_cast<std::size_t>(result.back().data() - a_range.data());
					result.back() = a_range.subspan(pos);
				}

				return result;
			}

			[[nodiscard]] auto directx_mip_chunk_maximum(
				::DXGI_FORMAT a_fmt,
				std::size_t a_width,
				std::size_t a_height)
				-> std::size_t
			{
				std::size_t pitch = 0;
				std::size_t slice = 0;
				if (const auto result = DirectX::ComputePitch(
						a_fmt,
						a_width,
						a_height,
						pitch,
						slice);
					FAILED(result)) {
					throw bsa::exception("failed to compute dds pitch");
				}

				return slice;
			}

			[[nodiscard]] auto sizeof_header(version a_version) noexcept
				-> std::size_t
			{
				std::size_t size = 0;

				switch (a_version) {
				case version::v3:
					size += sizeof(std::uint32_t);
					[[fallthrough]];
				case version::v2:
					size += sizeof(std::uint64_t);
					[[fallthrough]];
				case version::v1:
					size += 0x18;
					break;
				default:
					detail::declare_unreachable();
				}

				return size;
			}
		}

		class header_t final
		{
		public:
			header_t() noexcept = default;

			header_t(
				const archive::meta_info& a_meta,
				std::size_t a_fileCount,
				std::uint64_t a_stringTableOffset) :
				_version(to_underlying(a_meta.version)),
				_format(to_underlying(a_meta.format)),
				_fileCount(static_cast<std::uint32_t>(a_fileCount)),
				_stringTableOffset(a_stringTableOffset),
				_compression_format(a_meta.compression_format)
			{
				if (a_meta.compression_format == compression_format::lz4 && a_meta.version < version::v3) {
					throw exception("compression format is not valid for the given version");
				}
			}

			friend auto operator>>(
				istream_t& a_in,
				header_t& a_header)
				-> istream_t&
			{
				std::uint32_t magic = 0;

				a_in->read(
					magic,
					a_header._version,
					a_header._format,
					a_header._fileCount,
					a_header._stringTableOffset);

				if (magic != constants::btdx) {
					throw exception("invalid magic");
				} else if (a_header._format != constants::gnrl &&
						   a_header._format != constants::dx10) {
					throw exception("invalid format");
				} else {
					switch (a_header._version) {
					case 1:
					case 2:
					case 3:
						break;
					default:
						throw exception("invalid version");
					}
				}

				if (a_header._version >= 2) {
					(void)a_in->read<std::uint64_t>();
				}

				if (a_header._version >= 3) {
					const auto [compression] = a_in->read<std::uint32_t>();
					if (compression == constants::compression_lz4) {
						a_header._compression_format = fo4::compression_format::lz4;
					}
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
					a_header._version,
					a_header._format,
					a_header._fileCount,
					a_header._stringTableOffset);

				if (a_header._version >= 2) {
					a_out.write(std::uint64_t{ 1 });
				}

				if (a_header._version >= 3) {
					std::uint32_t format =
						a_header._compression_format == fo4::compression_format::lz4 ?
							constants::compression_lz4 :
							0;
					a_out.write(format);
				}

				return a_out;
			}

			[[nodiscard]] auto archive_format() const noexcept -> format { return static_cast<format>(_format); }
			[[nodiscard]] auto archive_version() const noexcept -> version { return static_cast<version>(_version); }
			[[nodiscard]] auto compression_format() const noexcept -> fo4::compression_format { return _compression_format; }
			[[nodiscard]] auto file_count() const noexcept -> std::size_t { return _fileCount; }

			[[nodiscard]] auto make_meta() const noexcept
				-> archive::meta_info
			{
				return archive::meta_info{
					.format = this->archive_format(),
					.version = this->archive_version(),
					.compression_format = this->compression_format(),
					.strings = this->string_table_offset() != 0,
				};
			}

			[[nodiscard]] auto string_table_offset() const noexcept
				-> std::uint64_t { return _stringTableOffset; }

		private:
			std::uint32_t _version{ 0 };
			std::uint32_t _format{ 0 };
			std::uint32_t _fileCount{ 0 };
			std::uint64_t _stringTableOffset{ 0 };
			fo4::compression_format _compression_format{ fo4::compression_format::zip };
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

	std::size_t chunk::compress_into_lz4(std::span<std::byte> a_out) const
	{
		assert(!this->compressed());
		assert(a_out.size_bytes() >= this->compress_bound(compression_format::lz4));

		const auto in = this->as_bytes();
		const auto result = ::LZ4_compress_HC(
			reinterpret_cast<const char*>(in.data()),
			reinterpret_cast<char*>(a_out.data()),
			static_cast<int>(in.size_bytes()),
			static_cast<int>(a_out.size_bytes()),
			12);
		if (result == 0) {
			throw bsa::compression_error(bsa::compression_error::library::lz4, result);
		}

		return static_cast<std::size_t>(result);
	}

	std::size_t chunk::compress_into_zlib(
		std::span<std::byte> a_out,
		int a_level,
		int a_windowBits,
		int a_memLevel) const
	{
		assert(!this->compressed());
		assert(a_out.size_bytes() >= this->compress_bound(compression_format::zip));

		::z_stream stream = {};
		if (const auto result = deflateInit2(
				&stream,
				a_level,
				Z_DEFLATED,
				a_windowBits,
				a_memLevel,
				Z_DEFAULT_STRATEGY);
			result != Z_OK) {
			throw bsa::compression_error(bsa::compression_error::library::zlib, result);
		}

		const auto in = this->as_bytes();

		stream.next_out = reinterpret_cast<::Bytef*>(a_out.data());
		stream.avail_out = 0;
		stream.next_in = (z_const ::Bytef*)in.data();
		stream.avail_in = 0;

		auto insz = in.size_bytes();
		auto outsz = a_out.size_bytes();
		int error = Z_OK;
		do {
			if (stream.avail_out == 0) {
				stream.avail_out = static_cast<::uInt>(
					std::min<std::size_t>((std::numeric_limits<::uInt>::max)(), outsz));
				outsz -= stream.avail_out;
			}

			if (stream.avail_in == 0) {
				stream.avail_in = static_cast<::uInt>(
					std::min<std::size_t>((std::numeric_limits<::uInt>::max)(), insz));
				insz -= stream.avail_in;
			}

			error = ::deflate(&stream, insz > 0 ? Z_NO_FLUSH : Z_FINISH);
		} while (error == Z_OK);

		const auto finalsz = stream.total_out;
		::deflateEnd(&stream);

		if (error != Z_STREAM_END) {
			throw bsa::compression_error(bsa::compression_error::library::zlib, error);
		}

		return finalsz;
	}

	void chunk::decompress_into_lz4(std::span<std::byte> a_out) const
	{
		assert(this->compressed());
		assert(a_out.size_bytes() >= this->decompressed_size());

		const auto in = this->as_bytes();
		const auto result = ::LZ4_decompress_safe(
			reinterpret_cast<const char*>(in.data()),
			reinterpret_cast<char*>(a_out.data()),
			static_cast<int>(in.size_bytes()),
			static_cast<int>(a_out.size_bytes()));

		if (result < 0) {
			throw bsa::compression_error(bsa::compression_error::library::lz4, result);
		}

		const auto outsz = static_cast<std::size_t>(result);
		if (outsz != this->decompressed_size()) {
			throw bsa::compression_error(detail::error_code::decompress_size_mismatch);
		}
	}

	void chunk::decompress_into_zlib(std::span<std::byte> a_out) const
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
		if (result != Z_OK) {
			throw bsa::compression_error(bsa::compression_error::library::zlib, result);
		}

		if (outsz != this->decompressed_size()) {
			throw bsa::compression_error(detail::error_code::decompress_size_mismatch);
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

	void chunk::compress(const compression_params& a_params)
	{
		std::vector<std::byte> out;
		out.resize(this->compress_bound(a_params.compression_format));

		const auto outsz = this->compress_into({ out.data(), out.size() }, a_params);
		out.resize(outsz);
		out.shrink_to_fit();
		this->set_data(std::move(out), this->size());

		assert(this->compressed());
	}

	auto chunk::compress_bound(compression_format a_format) const
		-> std::size_t
	{
		assert(!this->compressed());
		switch (a_format) {
		case compression_format::zip:
			return ::compressBound(static_cast<::uLong>(this->size()));
		case compression_format::lz4:
			return ::LZ4_compressBound(static_cast<int>(this->size()));
		default:
			detail::declare_unreachable();
		}
	}

	auto chunk::compress_into(
		std::span<std::byte> a_out,
		const compression_params& a_params) const
		-> std::size_t
	{
		switch (a_params.compression_format) {
		case compression_format::zip:
			switch (a_params.compression_level) {
			case compression_level::fo4:
				return this->compress_into_zlib(a_out, Z_DEFAULT_COMPRESSION, MAX_WBITS, 8);
			case compression_level::fo4_xbox:
				return this->compress_into_zlib(a_out, Z_BEST_COMPRESSION, 12, 8);
			case compression_level::starfield:
				return this->compress_into_zlib(a_out, Z_BEST_COMPRESSION, MAX_WBITS, MAX_MEM_LEVEL);
			default:
				detail::declare_unreachable();
			}
		case compression_format::lz4:
			return this->compress_into_lz4(a_out);
		default:
			detail::declare_unreachable();
		}
	}

	void chunk::decompress(compression_format a_format)
	{
		std::vector<std::byte> out;
		out.resize(this->decompressed_size());
		this->decompress_into({ out.data(), out.size() }, a_format);
		this->set_data(std::move(out));

		assert(!this->compressed());
	}

	void chunk::decompress_into(
		std::span<std::byte> a_out,
		compression_format a_format) const
	{
		switch (a_format) {
		case compression_format::zip:
			this->decompress_into_zlib(a_out);
			break;
		case compression_format::lz4:
			this->decompress_into_lz4(a_out);
			break;
		default:
			detail::declare_unreachable();
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

	void file::read(
		read_source a_source,
		const read_params& a_params)
	{
		auto& in = a_source.stream();
		switch (a_params.format) {
		case format::general:
			this->read_general(
				in,
				a_params.compression_format,
				a_params.compression_level,
				a_params.compression_type);
			break;
		case format::directx:
			this->read_directx(
				in,
				a_params.mip_chunk_width,
				a_params.mip_chunk_height,
				a_params.compression_format,
				a_params.compression_level,
				a_params.compression_type);
			break;
		default:
			detail::declare_unreachable();
		}
	}

	void file::write(
		write_sink a_sink,
		const write_params& a_params) const
	{
		auto& out = a_sink.stream();
		switch (a_params.format) {
		case format::general:
			this->write_general(out, a_params.compression_format);
			break;
		case format::directx:
			this->write_directx(out, a_params.compression_format);
			break;
		default:
			detail::declare_unreachable();
		}
	}

	void file::read_directx(
		detail::istream_t& a_in,
		std::size_t a_mipChunkWidth,
		std::size_t a_mipChunkHeight,
		compression_format a_format,
		compression_level a_level,
		compression_type a_type)
	{
		DirectX::ScratchImage scratch;
		const auto in = a_in->rdbuf();
		if (const auto result = DirectX::LoadFromDDSMemory(
				in.data(),
				in.size_bytes(),
				DirectX::DDS_FLAGS::DDS_FLAGS_NONE,
				nullptr,
				scratch);
			FAILED(result)) {
			throw bsa::exception("failed to load dds from memory");
		}

		this->clear();
		this->reserve(4u);

		auto& meta = scratch.GetMetadata();
		this->header.height = static_cast<std::uint16_t>(meta.height);
		this->header.width = static_cast<std::uint16_t>(meta.width);
		this->header.mip_count = static_cast<std::uint8_t>(meta.mipLevels);
		this->header.format = static_cast<std::uint8_t>(meta.format);
		this->header.flags = meta.IsCubemap() ? 1u : 0u;
		this->header.tile_mode = 8u;

		const std::span images{ scratch.GetImages(), scratch.GetImageCount() };
		const auto addChunk = [&](std::span<const DirectX::Image> a_splice) {
			assert(!a_splice.empty());

			std::vector<std::byte> bytes;
			for (const auto& image : a_splice) {
				// dxtex always allocates internally, so we're forced to allocate,
				//	even for mmapped files
				const auto pixels = reinterpret_cast<std::byte*>(image.pixels);
				bytes.insert(bytes.end(), pixels, pixels + image.slicePitch);
			}

			const auto mipIdx = [&](const DirectX::Image& a_image) noexcept {
				return static_cast<std::uint16_t>(
					(std::min<std::size_t>)(  //
						&a_image - scratch.GetImages(),
						this->header.mip_count - 1u));
			};

			auto& chunk = this->emplace_back();
			chunk.mips.first = mipIdx(a_splice.front());
			chunk.mips.last = mipIdx(a_splice.back());
			chunk.set_data(std::move(bytes));
			if (a_type == compression_type::compressed) {
				chunk.compress({ .compression_format = a_format, .compression_level = a_level });
			}
		};

		if ((this->header.flags & 1u) != 0) {  // don't chunk cubemaps
			addChunk(images);
		} else {
			const auto splices = detail::chunk<4>(
				images,
				detail::directx_mip_chunk_maximum(meta.format, a_mipChunkWidth, a_mipChunkHeight));
			std::for_each(splices.begin(), splices.end(), addChunk);
		}
	}

	void file::read_general(
		detail::istream_t& a_in,
		compression_format a_format,
		compression_level a_level,
		compression_type a_type)
	{
		this->clear();

		auto& chunk = this->emplace_back();
		chunk.set_data(a_in->rdbuf(), a_in);
		if (a_type == compression_type::compressed) {
			chunk.compress({ .compression_format = a_format, .compression_level = a_level });
		}
	}

	void file::write_directx(
		detail::ostream_t& a_out,
		compression_format a_format) const
	{
		const DirectX::TexMetadata meta{
			.width = this->header.width,
			.height = this->header.height,
			.depth = 1,
			.arraySize = 1,
			.mipLevels = this->header.mip_count,
			.miscFlags = (this->header.flags & 1u) != 0 ? std::uint32_t{ DirectX::TEX_MISC_FLAG::TEX_MISC_TEXTURECUBE } : 0u,
			.miscFlags2 = 0,
			.format = static_cast<::DXGI_FORMAT>(this->header.format),
			.dimension = DirectX::TEX_DIMENSION_TEXTURE2D,
		};

		std::size_t required = 0;
		if (const auto result = DirectX::EncodeDDSHeader(meta, DirectX::DDS_FLAGS_NONE, nullptr, 0, required);
			FAILED(result)) {
			throw bsa::exception("failed to encode dds header");
		}

		DirectX::Blob blob;
		blob.Initialize(required);

		if (const auto result = DirectX::EncodeDDSHeader(
				meta,
				DirectX::DDS_FLAGS::DDS_FLAGS_NONE,
				blob.GetBufferPointer(),
				blob.GetBufferSize(),
				required);
			FAILED(result)) {
			throw bsa::exception("failed to encode dds header");
		}

		a_out.write_bytes({ //
			static_cast<const std::byte*>(blob.GetBufferPointer()),
			blob.GetBufferSize() });
		std::vector<std::byte> buffer;
		for (const auto& chunk : *this) {
			if (chunk.compressed()) {
				buffer.resize(chunk.decompressed_size());
				chunk.decompress_into(buffer, a_format);
				a_out.write_bytes(buffer);
			} else {
				a_out.write_bytes(chunk.as_bytes());
			}
		}
	}

	void file::write_general(
		detail::ostream_t& a_out,
		compression_format a_format) const
	{
		std::vector<std::byte> buffer;
		for (const auto& chunk : *this) {
			if (chunk.compressed()) {
				buffer.resize(chunk.decompressed_size());
				chunk.decompress_into(buffer, a_format);
				a_out.write_bytes(buffer);
			} else {
				a_out.write_bytes(chunk.as_bytes());
			}
		}
	}

	auto archive::read(read_source a_source)
		-> meta_info
	{
		auto& in = a_source.stream();
		const auto header = [&]() {
			detail::header_t result;
			in >> result;
			return result;
		}();

		this->clear();
		for (std::size_t i = 0, strpos = header.string_table_offset();
			 i < header.file_count();
			 ++i) {
			hashing::hash hash;
			in >> hash;

			const auto name = [&]() {
				if (strpos != 0) {
					const detail::restore_point _{ in };
					in->seek_absolute(strpos);
					const auto name = detail::read_wstring(in);
					strpos = in->tell();
					return name;
				} else {
					return ""sv;
				}
			}();

			[[maybe_unused]] const auto [it, success] =
				this->insert(
					key_type{ hash, name, in },
					mapped_type{});
			assert(success);

			this->read_file(it->second, in, header.archive_format());
		}

		return header.make_meta();
	}

	void archive::write(
		write_sink a_sink,
		const meta_info& a_meta) const
	{
		auto& out = a_sink.stream();

		auto [header, dataOffset] = make_header(a_meta);
		out << header;

		for (const auto& [key, file] : *this) {
			out << key.hash();
			this->write_file(file, out, a_meta.format, dataOffset);
		}

		for (const auto& file : *this) {
			for (const auto& chunk : file.second) {
				out.write_bytes(chunk.as_bytes());
			}
		}

		if (a_meta.strings) {
			for ([[maybe_unused]] const auto& [key, file] : *this) {
				detail::write_wstring(out, key.name());
			}
		}
	}

	auto archive::make_header(const meta_info& a_meta) const
		-> std::pair<detail::header_t, std::uint64_t>
	{
		const auto inspect = [&](auto a_gnrl, auto a_dx10) noexcept {
			switch (a_meta.format) {
			case format::general:
				return a_gnrl();
			case format::directx:
				return a_dx10();
			default:
				detail::declare_unreachable();
			}
		};

		std::uint64_t dataOffset =
			detail::sizeof_header(a_meta.version) +
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
				a_meta,
				this->size(),
				a_meta.strings ? dataOffset + dataSize : 0u },
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
