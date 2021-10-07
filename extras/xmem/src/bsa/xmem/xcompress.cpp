#include "xcompress.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <Windows.h>

#include <mmio/mmio.hpp>
#include <xbyak/xbyak.h>

#include "bsa/xmem/expected.hpp"
#include "bsa/xmem/hashing.hpp"
#include "bsa/xmem/util.hpp"
#include "bsa/xmem/winapi.hpp"
#include "bsa/xmem/xmem.hpp"

namespace bsa::xmem::xcompress
{
	using namespace std::literals;

	namespace relocatable
	{
		namespace
		{
			void* create_compression_context = nullptr;
			void* compress_stream = nullptr;
			void* destroy_compression_context = nullptr;

			void* create_decompression_context = nullptr;
			void* decompress_stream = nullptr;
			void* destroy_decompression_context = nullptr;

			void* do_reset_compression_context = nullptr;

			void* compress_internals[3] = {};
		}
	}

	namespace detail
	{
		namespace
		{
			struct s_do_compress :
				Xbyak::CodeGenerator
			{
				s_do_compress() noexcept
				{
					assert(relocatable::do_reset_compression_context != nullptr);
					assert(relocatable::compress_internals[0] != nullptr);
					assert(relocatable::compress_internals[1] != nullptr);
					assert(relocatable::compress_internals[2] != nullptr);

					push(ebp);
					mov(ebp, esp);
					sub(esp, 0x18);
					mov(eax, ptr[ebp + 0x8]);
					push(ebx);
					push(esi);
					xor_(ebx, ebx);
					test(byte[eax + 0x8], 0x1);
					push(edi);
					lea(esi, ptr[eax + 0x10]);
					mov(ptr[ebp - 0x4], ebx);
					jnz("loc_1000478D");
					push(eax);
					mov(eax, reinterpret_cast<std::uint32_t>(relocatable::do_reset_compression_context));  // added
					call(eax);
					pop(ecx);

					L("loc_1000478D");
					mov(ecx, ptr[ebp + 0x0C]);
					mov(eax, ptr[ebp + 0x18]);
					cmp(eax, ebx);
					mov(edi, ptr[ebp + 0x14]);
					mov(ptr[ebp - 0x18], ecx);
					mov(ecx, ptr[ebp + 0x10]);
					mov(ecx, ptr[ecx]);
					mov(ptr[ebp - 0x14], ecx);
					lea(ecx, ptr[ebp - 0x18]);
					mov(ptr[ebp + 0x18], eax);
					mov(ptr[ebp + 0x8], ebx);
					mov(ptr[ebp - 0x10], ebx);
					mov(ptr[ebp - 0xC], ebx);
					mov(ptr[ebp - 0x8], ebx);
					mov(dword[esi + 0x4340], reinterpret_cast<std::uint32_t>(relocatable::compress_internals[0]));
					mov(ptr[esi + 0x4330], ecx);
					jbe("loc_10004800");
					mov(ebx, 0x8000);

					L("loc_100047CC");
					mov(eax, ptr[ebp + 0x18]);
					cmp(eax, ebx);
					mov(ptr[ebp + 0x14], eax);
					jb("loc_100047D9");
					mov(ptr[ebp + 0x14], ebx);

					L("loc_100047D9");
					inc(dword[ebp + 0x8]);
					cmp(eax, ebx);
					ja("loc_100047E6");
					mov(eax, ptr[ebp + 0x8]);
					mov(ptr[ebp - 0x8], eax);

					L("loc_100047E6");
					push(0);
					lea(eax, ptr[ebp + 0xC]);
					push(eax);
					push(dword[ebp + 0x14]);
					push(edi);
					push(esi);
					mov(eax, reinterpret_cast<std::uint32_t>(relocatable::compress_internals[1]));  // added
					call(eax);
					mov(eax, ptr[ebp + 0x14]);
					add(edi, eax);
					sub(ptr[ebp + 0x18], eax);
					jnz("loc_100047CC");

					L("loc_10004800");
					push(esi);
					mov(eax, reinterpret_cast<std::uint32_t>(relocatable::compress_internals[2]));  // added
					call(eax);
					mov(eax, ptr[ebp - 0x10]);
					cmp(eax, ptr[ebp - 0x14]);
					mov(ecx, ptr[ebp + 0x10]);
					pop(edi);
					pop(esi);
					mov(ptr[ecx], eax);
					pop(ebx);
					jbe("loc_1000481D");
					mov(dword[ebp - 0x4], 0x6);

					L("loc_1000481D");
					mov(eax, ptr[ebp - 0x4]);
					leave();
					ret();

					this->ready();
				}
			};

			std::int32_t __cdecl do_compress(
				xcompress::compression_context a_context,
				void* a_destination,
				std::uint32_t* a_destSize,
				const void* a_source,
				std::uint32_t a_srcSize) noexcept
			{
				static s_do_compress code;
				return code.getCode<decltype(&detail::do_compress)>()(
					a_context,
					a_destination,
					a_destSize,
					a_source,
					a_srcSize);
			}

			::HRESULT internal_code_to_hresult(
				std::int32_t a_code) noexcept
			{
				switch (a_code) {
				case 0:
					return S_OK;
				case 1:
					return E_OUTOFMEMORY;
				case 6:
				case 7:
					return 0x81DE2001;
				default:
					return E_FAIL;
				}
			}
		}

		struct xna_candidate_t final
		{
			std::wstring_view version;
			std::string_view hash;
		};

		struct xna_result_t final
		{
			::HMODULE dll{ nullptr };
			std::wstring_view version;
		};

		[[nodiscard]] auto find_bundled_xna(
			std::span<const xna_candidate_t> a_frameworks) noexcept
			-> xmem::expected<xna_result_t>
		{
			const auto cwd = xmem::current_executable_directory();
			if (cwd) {
				try {
					const auto path = *cwd / u8"XnaNative.dll"sv;
					const mmio::mapped_file_source dll{ path };
					const auto hash = hashing::sha512(std::span{ dll.data(), dll.size() });
					WRAP_EXPECTED(hash);

					const auto it =
						std::find_if(
							a_frameworks.begin(),
							a_frameworks.end(),
							[&](const auto& a_elem) noexcept {
								return a_elem.hash == *hash;
							});
					if (it != a_frameworks.end()) {
						const auto handle = ::LoadLibraryW(path.c_str());
						if (handle) {
							return xna_result_t{ handle, it->version };
						} else {
							return xmem::unexpected(xmem::error_code::load_library_failure);
						}
					}
				} catch (const std::system_error&) {}
			}

			return xmem::unexpected(xmem::error_code::ok);
		}

		[[nodiscard]] auto find_installed_xna(
			std::span<const xna_candidate_t> a_frameworks) noexcept
			-> xmem::expected<xna_result_t>
		{
			for (const auto& [version, expectedSHA] : a_frameworks) {
				std::wstring subkey(LR"(SOFTWARE\WOW6432Node\Microsoft\XNA\Framework\)"sv);
				subkey += version;

				const auto getValue = [&](void* a_dst, ::DWORD& a_dstSz) noexcept {
					return ::RegGetValueW(
						HKEY_LOCAL_MACHINE,
						subkey.c_str(),
						L"NativeLibraryPath",
						RRF_RT_REG_SZ,
						nullptr,
						a_dst,
						&a_dstSz);
				};

				::DWORD size = 0;
				auto status = getValue(nullptr, size);
				if (status == ERROR_SUCCESS) {
					std::vector<wchar_t> dst(size / sizeof(wchar_t));
					status = getValue(dst.data(), size);
					if (status != ERROR_SUCCESS) {
						return xmem::unexpected(xmem::error_code::reg_get_value_failure);
					}

					std::filesystem::path path{ dst.data(), dst.data() + (size / sizeof(wchar_t)) - 1 };
					path /= L"XnaNative.dll"sv;
					mmio::mapped_file_source file{ path };
					const auto readSHA = hashing::sha512(std::span{ file.data(), file.size() });
					WRAP_EXPECTED(readSHA);

					if (*readSHA == expectedSHA) {
						const auto lib = ::LoadLibraryW(path.c_str());
						if (lib != nullptr) {
							return xna_result_t{ lib, version };
						} else {
							return xmem::unexpected(xmem::error_code::load_library_failure);
						}
					}
				} else if (status != ERROR_FILE_NOT_FOUND) {
					return xmem::unexpected(xmem::error_code::reg_get_value_failure);
				}
			}

			return xmem::unexpected(xmem::error_code::ok);
		}

		[[nodiscard]] auto find_xcompress_proxy() noexcept
			-> xmem::expected<xna_result_t>
		{
			constexpr std::array frameworks = {
				xna_candidate_t{ L"v3.0"sv, "31B27A5924FE35A11ADDCBFC97C30E074A20966CE9E7D4031165CCD6CA5A75254CB1E76F830921FCAD6D07473214F9D75B555C689EBC0A196E9CF948CAB4E37F"sv },  // 3.0
				xna_candidate_t{ L"v3.1"sv, "001F7B6AC90344405D04361AC699F1C13706587352BA7CBD9FB08F1803AD3EFD99E615863E6E27B5429B784FE1B4FF93E34BB184FB568B093B0C3108D8C88EE3"sv },  // 3.1
				xna_candidate_t{ L"v4.0"sv, "A53443BE19E7DC52B0E7AE8FEF2E00967E4FF9207D2813D559ED65F5F1E7A59AF878D45C68C21CAEB898F220CF3C211489CB4BAD9F3455557B3408F0F3FF9454"sv },  // 4.0
				xna_candidate_t{ L"v4.0"sv, "014E1C057F17BC625FEB4EAC03C17508C36836EF8C5C09E48D67ECEBA8614A6B36743AF9D281B999F4768D75BAAF1B5FEFCB5F0B1E0519E708F732F9E68C972D"sv },  // 4.0 Refresh
			};

			auto bundled = find_bundled_xna(frameworks);
			if (bundled.has_value() || bundled.error() != xmem::error_code::ok) {
				return bundled;
			}

			auto installed = find_installed_xna(frameworks);
			if (installed.has_value() || installed.error() != xmem::error_code::ok) {
				return installed;
			}

			return xmem::unexpected(xmem::error_code::xcompress_no_proxy_found);
		}
	}

	::HRESULT WINAPI create_compression_context(
		xcompress::codec_type a_codecType,
		const void* a_codecParams,
		xcompress::flags a_flags,
		xcompress::compression_context* a_context) noexcept
	{
		assert(relocatable::create_compression_context != nullptr);
		return reinterpret_cast<decltype(&xcompress::create_compression_context)>(
			relocatable::create_compression_context)(
			a_codecType,
			a_codecParams,
			a_flags,
			a_context);
	}

	::HRESULT compress(
		xcompress::compression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t a_srcSize) noexcept
	{
		::HRESULT result = E_FAIL;
		if (*util::adjust_pointer<std::uint32_t>(a_context, +0x4) == 1) {
			if (a_srcSize > 0) {
				const auto code = detail::do_compress(
					a_context,
					a_destination,
					a_destSize,
					a_source,
					a_srcSize);
				result = detail::internal_code_to_hresult(code);
			} else {
				*a_destSize = 0;
				result = S_OK;
			}
		}
		return result;
	}

	::HRESULT WINAPI compress_stream(
		xcompress::compression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t* a_srcSize) noexcept
	{
		assert(relocatable::compress_stream != nullptr);
		return reinterpret_cast<decltype(&xcompress::compress_stream)>(
			relocatable::compress_stream)(
			a_context,
			a_destination,
			a_destSize,
			a_source,
			a_srcSize);
	}

	void WINAPI destroy_compression_context(
		xcompress::compression_context a_context) noexcept
	{
		assert(relocatable::destroy_compression_context != nullptr);
		return reinterpret_cast<decltype(&xcompress::destroy_compression_context)>(
			relocatable::destroy_compression_context)(
			a_context);
	}

	::HRESULT WINAPI create_decompression_context(
		xcompress::codec_type a_codecType,
		const void* a_codecParams,
		xcompress::flags a_flags,
		xcompress::decompression_context* a_context) noexcept
	{
		assert(relocatable::create_decompression_context != nullptr);
		return reinterpret_cast<decltype(&xcompress::create_decompression_context)>(
			relocatable::create_decompression_context)(
			a_codecType,
			a_codecParams,
			a_flags,
			a_context);
	}

	::HRESULT WINAPI decompress_stream(
		xcompress::decompression_context a_context,
		void* a_destination,
		std::uint32_t* a_destSize,
		const void* a_source,
		std::uint32_t* a_srcSize) noexcept
	{
		assert(relocatable::decompress_stream != nullptr);
		return reinterpret_cast<decltype(&xcompress::decompress_stream)>(
			relocatable::decompress_stream)(
			a_context,
			a_destination,
			a_destSize,
			a_source,
			a_srcSize);
	}

	void WINAPI destroy_decompression_context(
		xcompress::decompression_context a_context) noexcept
	{
		assert(relocatable::destroy_decompression_context != nullptr);
		return reinterpret_cast<decltype(&xcompress::destroy_decompression_context)>(
			relocatable::destroy_decompression_context)(
			a_context);
	}

	auto initialize() noexcept
		-> xmem::error_code
	{
		static const auto holder = []() -> xmem::expected<winapi::hmodule_wrapper> {
			const auto found = detail::find_xcompress_proxy();
			if (!found) {
				return xmem::unexpected(found.error());
			}

			const auto [lib, version] = *found;
			assert(lib != nullptr);
			const auto getProc = [&](void*& a_dst, std::uintptr_t a_offset) {
				a_dst = util::adjust_pointer<void>(lib, a_offset);
				assert(a_dst != nullptr);
			};

			if (version == L"v3.0"sv) {
				getProc(relocatable::create_compression_context, 0x0018D303);
				getProc(relocatable::compress_stream, 0x0018D293);
				getProc(relocatable::destroy_compression_context, 0x0018D2DF);

				getProc(relocatable::create_decompression_context, 0x0018D3DA);
				getProc(relocatable::decompress_stream, 0x0018D36A);
				getProc(relocatable::destroy_decompression_context, 0x0018D3B6);

				getProc(relocatable::do_reset_compression_context, 0x0018D673);

				getProc(relocatable::compress_internals[0], 0x0018D58B);
				getProc(relocatable::compress_internals[1], 0x0018DF41);
				getProc(relocatable::compress_internals[2], 0x0018DF8C);
			} else if (version == L"v3.1"sv) {
				getProc(relocatable::create_compression_context, 0x001963F1);
				getProc(relocatable::compress_stream, 0x0019633F);
				getProc(relocatable::destroy_compression_context, 0x001963CB);

				getProc(relocatable::create_decompression_context, 0x001964DB);
				getProc(relocatable::decompress_stream, 0x0019645F);
				getProc(relocatable::destroy_decompression_context, 0x001964B5);

				getProc(relocatable::do_reset_compression_context, 0x001967A2);

				getProc(relocatable::compress_internals[0], 0x001966B0);
				getProc(relocatable::compress_internals[1], 0x001970B3);
				getProc(relocatable::compress_internals[2], 0x00197103);
			} else if (version == L"v4.0"sv) {  // same offsets for regular and refresh
				getProc(relocatable::create_compression_context, 0x00197933);
				getProc(relocatable::compress_stream, 0x00197881);
				getProc(relocatable::destroy_compression_context, 0x0019790D);

				getProc(relocatable::create_decompression_context, 0x00197A1D);
				getProc(relocatable::decompress_stream, 0x001979A1);
				getProc(relocatable::destroy_decompression_context, 0x001979F7);

				getProc(relocatable::do_reset_compression_context, 0x00197CF2);

				getProc(relocatable::compress_internals[0], 0x00197C00);
				getProc(relocatable::compress_internals[1], 0x00198603);
				getProc(relocatable::compress_internals[2], 0x00198653);
			} else {
				return xmem::unexpected(xmem::error_code::xcompress_unhandled_version);
			}

			return winapi::hmodule_wrapper{ lib };
		}();

		return holder ? xmem::error_code::ok : holder.error();
	}
}
