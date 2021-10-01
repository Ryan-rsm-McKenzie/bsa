#include "bsa/xmem/winapi.hpp"

#include <Windows.h>

#include <catch2/catch.hpp>

namespace winapi = bsa::xmem::winapi;

TEST_CASE("validate winapi wrappers", "[src]")
{
	SECTION("HRESULT")
	{
		REQUIRE(winapi::hresult_success(S_OK));
		REQUIRE(!winapi::hresult_success(E_FAIL));
	}

	SECTION("NTSTATUS")
	{
		constexpr auto status_success = static_cast<::NTSTATUS>(0x00000000);
		REQUIRE(winapi::nt_success(status_success));

		constexpr auto status_object_name_exists = static_cast<::NTSTATUS>(0x40000000);
		REQUIRE(winapi::nt_success(status_object_name_exists));

		constexpr auto status_guard_page_violation = static_cast<::NTSTATUS>(0x80000001);
		REQUIRE(!winapi::nt_success(status_guard_page_violation));

		constexpr auto status_unsuccessful = static_cast<::NTSTATUS>(0xC0000001);
		REQUIRE(!winapi::nt_success(status_unsuccessful));
	}
}
