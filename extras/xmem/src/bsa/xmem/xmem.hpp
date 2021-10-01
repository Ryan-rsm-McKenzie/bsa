#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include <binary_io/common.hpp>
#include <tl/expected.hpp>

namespace bsa::xmem
{
	using namespace std::literals;

#define ENUMERATE(F)                            \
	F(exit_failure, = -1)                       \
                                                \
	F(ok, = 0)                                  \
                                                \
	F(open_file_failure)                        \
                                                \
	F(hashing_open_algorithm_provider_failure)  \
	F(hashing_create_hash_failure)              \
	F(hashing_hash_data_failure)                \
	F(hashing_get_property_failure)             \
	F(hashing_finish_hash_failure)              \
                                                \
	F(reg_get_value_failure)                    \
                                                \
	F(xcompress_no_proxy_found)                 \
	F(xcompress_unhandled_version)              \
                                                \
	F(api_create_compression_context_failure)   \
	F(api_create_decompression_context_failure) \
	F(api_compress_failure)                     \
	F(api_compress_bound_failure)               \
	F(api_decompress_failure)                   \
                                                \
	F(serve_unhandled_request)                  \
	F(serve_decompress_size_mismatch)

	enum class error_code : std::int8_t
	{
#define AS_ENUM(a_enum, ...) a_enum __VA_ARGS__,
		ENUMERATE(AS_ENUM)
#undef AS_ENUM
	};

	[[nodiscard]] constexpr auto to_string(error_code a_code) noexcept
		-> std::string_view
	{
#define AS_STRING(a_enum, ...) \
	case error_code::##a_enum: \
		return #a_enum##sv;

		switch (a_code) {
			ENUMERATE(AS_STRING)
		default:
			return "unknown"sv;
		}

#undef AS_STRING
	}

#undef ENUMERATE

	template <class T>
	using expected = tl::expected<T, xmem::error_code>;

	using tl::unexpected;

	struct byte_container
	{
	public:
		byte_container() noexcept = default;

		byte_container(std::span<const std::byte> a_bytes) noexcept :
			_data(std::in_place_index<shallow>, a_bytes)
		{}

		byte_container(std::vector<std::byte> a_bytes) noexcept :
			_data(std::in_place_index<deep>, std::move(a_bytes))
		{}

		[[nodiscard]] auto as_bytes() const noexcept
			-> std::span<const std::byte>
		{
			if (_data.index() == shallow) {
				return *std::get_if<shallow>(&_data);
			} else {
				return *std::get_if<deep>(&_data);
			}
		}

		[[nodiscard]] auto as_vector() && noexcept
			-> std::vector<std::byte>
		{
			if (_data.index() == shallow) {
				auto& bytes = *std::get_if<shallow>(&_data);
				return std::vector(bytes.begin(), bytes.end());
			} else {
				return std::move(*std::get_if<deep>(&_data));
			}
		}

		void emplace(std::span<const std::byte> a_bytes) noexcept
		{
			_data.emplace<shallow>(a_bytes);
		}

		void emplace(std::vector<std::byte> a_bytes) noexcept
		{
			_data.emplace<deep>(std::move(a_bytes));
		}

	private:
		enum
		{
			shallow,
			deep,

			total
		};

		std::variant<
			std::span<const std::byte>,
			std::vector<std::byte>>
			_data;

		static_assert(std::variant_size_v<decltype(_data)> == total);
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		byte_container& a_container) noexcept
	{
		auto& stream = static_cast<T&>(a_stream);

		const auto [size] = a_stream.read<std::uint32_t>();
		std::vector<std::byte> bytes(size);
		stream.read_bytes(bytes);
		a_container.emplace(std::move(bytes));

		return stream;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const byte_container& a_container) noexcept
	{
		auto& stream = static_cast<T&>(a_stream);

		const auto bytes = a_container.as_bytes();
		stream.write(static_cast<std::uint32_t>(bytes.size_bytes()));
		stream.write_bytes(bytes);

		return stream;
	}

	enum class request_type : std::uint8_t
	{
		none,

		exit,
		compress,
		compress_bound,
		decompress,
	};

	struct request_header
	{
		request_type type{ request_type::none };
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		request_header& a_request) noexcept
	{
		return static_cast<T&>(a_stream) >> a_request.type;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const request_header& a_request) noexcept
	{
		return static_cast<T&>(a_stream) << a_request.type;
	}

	struct response_header
	{
		error_code error{ error_code::ok };
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		response_header& a_response) noexcept
	{
		return static_cast<T&>(a_stream) >> a_response.error;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const response_header& a_response) noexcept
	{
		return static_cast<T&>(a_stream) << a_response.error;
	}

	template <request_type E>
	struct generic_request
	{
		static constexpr auto expected_request = E;

		byte_container data;
	};

	template <class T, request_type E>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		generic_request<E>& a_request) noexcept
	{
		return static_cast<T&>(a_stream) >> a_request.data;
	}

	template <class T, request_type E>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const generic_request<E>& a_request) noexcept
	{
		return static_cast<T&>(a_stream) << a_request.data;
	}

	template <request_type>
	struct generic_response
	{
		byte_container data;
	};

	template <class T, request_type E>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		generic_response<E>& a_response) noexcept
	{
		return static_cast<T&>(a_stream) >> a_response.data;
	}

	template <class T, request_type E>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const generic_response<E>& a_response) noexcept
	{
		return static_cast<T&>(a_stream) << a_response.data;
	}

	using compress_bound_request = generic_request<request_type::compress_bound>;

	using compress_response = generic_response<request_type::compress>;
	using decompress_response = generic_response<request_type::decompress>;

	struct compress_request
	{
		static constexpr auto expected_request = request_type::compress;

		std::uint32_t bound{ 0 };
		byte_container data;
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		compress_request& a_request) noexcept
	{
		return static_cast<T&>(a_stream) >>
		       a_request.bound >>
		       a_request.data;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const compress_request& a_request) noexcept
	{
		return static_cast<T&>(a_stream)
		       << a_request.bound
		       << a_request.data;
	}

	struct decompress_request
	{
		static constexpr auto expected_request = request_type::decompress;

		std::uint32_t original_size{ 0 };
		byte_container data;
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		decompress_request& a_request) noexcept
	{
		return static_cast<T&>(a_stream) >>
		       a_request.original_size >>
		       a_request.data;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const decompress_request& a_request) noexcept
	{
		return static_cast<T&>(a_stream)
		       << a_request.original_size
		       << a_request.data;
	}

	struct compress_bound_response
	{
		std::uint32_t bound{ 0 };
	};

	template <class T>
	decltype(auto) operator>>(
		binary_io::istream_interface<T>& a_stream,
		compress_bound_response& a_response) noexcept
	{
		return static_cast<T&>(a_stream) >> a_response.bound;
	}

	template <class T>
	decltype(auto) operator<<(
		binary_io::ostream_interface<T>& a_stream,
		const compress_bound_response& a_response) noexcept
	{
		return static_cast<T&>(a_stream) << a_response.bound;
	}
}
