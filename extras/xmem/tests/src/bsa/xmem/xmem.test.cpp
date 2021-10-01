#include "bsa/xmem/xmem.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <binary_io/binary_io.hpp>
#include <catch2/catch.hpp>

using namespace std::literals;

namespace xmem = bsa::xmem;

TEST_CASE("validate xmem ipc protocols", "[src]")
{
	const auto number = 42u;
	const std::string_view data{ "\x01\x02\x03\x04\0x05\0x06\0x07"sv };
	const auto bytes = std::as_bytes(std::span{ data });

	const auto validateBytes = [&](const xmem::byte_container& a_container) {
		REQUIRE(a_container.as_bytes().size() == bytes.size());
		REQUIRE(std::equal(bytes.begin(), bytes.end(), a_container.as_bytes().begin()));
	};

	const auto validate = []<class T>(const T& a_original, auto a_validator) {
		a_validator(a_original);

		binary_io::memory_ostream out;
		out << a_original;

		binary_io::span_istream in{ out.rdbuf() };
		T copy = {};
		in >> copy;

		REQUIRE(in.tell() == out.tell());
		a_validator(copy);
	};

	SECTION("headers")
	{
		const auto request = xmem::request_type::exit;
		validate(
			xmem::request_header{ request },
			[&](const xmem::request_header& a_request) {
				REQUIRE(a_request.type == request);
			});

		const auto response = xmem::error_code::serve_unhandled_request;
		validate(
			xmem::response_header{ response },
			[&](const xmem::response_header& a_response) {
				REQUIRE(a_response.error == response);
			});
	}

	SECTION("compress")
	{
		validate(
			xmem::compress_request{ number, bytes },
			[&](const xmem::compress_request& a_request) {
				REQUIRE(a_request.bound == number);
				validateBytes(a_request.data);
			});

		validate(
			xmem::compress_response{ bytes },
			[&](const xmem::compress_response& a_response) {
				validateBytes(a_response.data);
			});
	}

	SECTION("compress_bound")
	{
		validate(
			xmem::compress_bound_request{ bytes },
			[&](const xmem::compress_bound_request& a_request) {
				validateBytes(a_request.data);
			});

		validate(
			xmem::compress_bound_response{ number },
			[&](const xmem::compress_bound_response& a_response) {
				REQUIRE(a_response.bound == number);
			});
	}

	SECTION("decompress")
	{
		validate(
			xmem::decompress_request{ number, bytes },
			[&](const xmem::decompress_request& a_request) {
				REQUIRE(a_request.original_size == number);
				validateBytes(a_request.data);
			});

		validate(
			xmem::decompress_response{ bytes },
			[&](const xmem::decompress_response& a_response) {
				validateBytes(a_response.data);
			});
	}
}
