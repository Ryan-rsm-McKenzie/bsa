#include <fstream>

#include "bsa/all.hpp"

namespace bsa::all
{
	namespace detail
	{
		[[nodiscard]] auto read_file(const std::filesystem::path& a_path) -> std::vector<std::byte>
		{
			std::vector<std::byte> data;
			data.resize(std::filesystem::file_size(a_path));

			std::ifstream in{ a_path, std::ios_base::in | std::ios_base::binary };
			in.exceptions(std::ios_base::failbit);
			in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

			return data;
		}

		[[nodiscard]] auto get_archive_identifier(underlying_archive archive) -> std::string_view
		{
			const auto visiter = detail::overload{
				[](bsa::tes3::archive) { return "tes3"; },
				[](bsa::tes4::archive) { return "tes4"; },
				[](bsa::fo4::archive) { return "fo4"; },
			};
			return std::visit(visiter, archive);
		}

		template <typename Version>
		[[nodiscard]] auto archive_version(underlying_archive archive, version a_version) -> Version
		{
			const bool correct = [&] {
				switch (a_version) {
				case bsa::all::version::tes3:
					{
						const bool same = std::same_as<Version, std::uint32_t>;
						return same && std::holds_alternative<bsa::tes3::archive>(archive);
					}
				case bsa::all::version::tes4:
				case bsa::all::version::fo3:
				case bsa::all::version::sse:
					{
						const bool same = std::same_as<Version, bsa::tes4::version>;
						return same && std::holds_alternative<bsa::tes4::archive>(archive);
					}
				case bsa::all::version::fo4:
				case bsa::all::version::fo4dx:
					{
						const bool same = std::same_as<Version, bsa::fo4::format>;
						return same && std::holds_alternative<bsa::fo4::archive>(archive);
					}
				default:
					return false;
				}
			}();

			if (!correct) {
				throw exception("Mismatch between requested version and variant type");
			}

			return static_cast<Version>(bsa::detail::to_underlying(a_version));
		}

		template std::uint32_t archive_version<std::uint32_t>(underlying_archive, version);
		template bsa::tes4::version archive_version<bsa::tes4::version>(underlying_archive, version);
		template bsa::fo4::format archive_version<bsa::fo4::format>(underlying_archive, version);

	}  // namespace detail

	archive::archive(const std::filesystem::path& a_path)
	{
		read(a_path);
	}

	archive::archive(version a_version, bool a_compressed) :
		_version(a_version), _compressed(a_compressed)
	{
		switch (_version) {
		case bsa::all::version::tes3:
			_archive = bsa::tes3::archive{};
			break;
		case bsa::all::version::tes4:
		case bsa::all::version::fo3:
		case bsa::all::version::sse:
			{
				bsa::tes4::archive bsa;
				auto flags = bsa::tes4::archive_flag::directory_strings | bsa::tes4::archive_flag::file_strings;
				if (_compressed) {
					flags |= bsa::tes4::archive_flag::compressed;
				}
				bsa.archive_flags(flags);
				_archive = std::move(bsa);
				break;
			}
		case bsa::all::version::fo4:
		case bsa::all::version::fo4dx:
			_archive = bsa::fo4::archive{};
		}
	}

	auto archive::read(const std::filesystem::path& a_path) -> version
	{
		const auto format = bsa::guess_file_format(a_path).value();

		const auto read = [this, &a_path](auto archive) {
			auto format = archive.read(std::move(a_path));
			_archive = std::move(archive);
			return static_cast<version>(format);
		};

		_version = [&] {
			switch (format) {
			case bsa::file_format::fo4:
				return read(bsa::fo4::archive{});
			case bsa::file_format::tes3:
				{
					bsa::tes3::archive archive;
					archive.read(std::move(a_path));
					_archive = std::move(archive);
					return version::tes3;
				}
			case bsa::file_format::tes4:
				return read(bsa::tes4::archive{});
			default:
				bsa::detail::declare_unreachable();
			}
		}();
		if (_version == version::fo4dx) {
			throw std::runtime_error("unsupported fo4 archive format");
		}
		return _version;
	}

	void archive::write(std::filesystem::path a_path)
	{
		const auto writer = detail::overload{
			[&](bsa::tes3::archive& bsa) { bsa.write(a_path); },
			[&](bsa::tes4::archive& bsa) {
				const auto version = detail::archive_version<bsa::tes4::version>(_archive, _version);
				bsa.write(a_path, version);
			},
			[&](bsa::fo4::archive& ba2) {
				const auto version = detail::archive_version<bsa::fo4::format>(_archive, _version);
				ba2.write(a_path, version);
			},
		};

		std::visit(writer, _archive);
	}

	void archive::add_file(const std::filesystem::path& a_root, const std::filesystem::path& a_path)
	{
		const auto relative = a_path.lexically_relative(a_root).lexically_normal();
		const auto data = detail::read_file(a_path);
		add_file(relative, data);
	}

	void archive::add_file(const std::filesystem::path& a_relative, std::vector<std::byte> a_data)
	{
		const auto adder = detail::overload{
			[&](bsa::tes3::archive& bsa) {
				bsa::tes3::file f;
				f.set_data(std::move(a_data));

				bsa.insert(a_relative.lexically_normal().generic_string(), std::move(f));
			},
			[&, this](bsa::tes4::archive& bsa) {
				bsa::tes4::file f;
				const auto version = detail::archive_version<bsa::tes4::version>(_archive, _version);
				f.set_data(std::move(a_data));

				if (_compressed)
					f.compress(version);

				const auto d = [&]() {
					const auto key = a_relative.parent_path().lexically_normal().generic_string();
					if (bsa.find(key) == bsa.end()) {
						bsa.insert(key, bsa::tes4::directory{});
					}
					return bsa[key];
				}();

				d->insert(a_relative.filename().lexically_normal().generic_string(), std::move(f));
			},
			[&, this](bsa::fo4::archive& ba2) {
				assert(detail::archive_version<bsa::fo4::format>(_archive, _version) == bsa::fo4::format::general && "directx ba2 not supported");
				bsa::fo4::file f;
				auto& chunk = f.emplace_back();
				chunk.set_data(std::move(a_data));

				if (_compressed)
					chunk.compress();

				ba2.insert(a_relative.lexically_normal().generic_string(), std::move(f));
			},
		};

		std::visit(adder, _archive);
	}

	void archive::iterate_files(const iteration_callback& a_callback, bool skip_compressed)
	{
		auto visiter = detail::overload{
			[&](bsa::tes3::archive& bsa) {
				for (const auto& [key, file] : bsa) {
					const auto relative = detail::virtual_to_local_path(key);
					const auto bytes = file.as_bytes();
					a_callback(relative, bytes);
				}
			},
			[&](bsa::tes4::archive& bsa) {
				for (auto& dir : bsa) {
					for (auto& file : dir.second) {
						const auto relative = detail::virtual_to_local_path(dir.first, file.first);
						const auto ver = detail::archive_version<bsa::tes4::version>(_archive, _version);

						if (file.second.compressed()) {
							if (skip_compressed) {
								continue;
							}
							file.second.decompress(ver);
						}
						a_callback(relative, file.second.as_bytes());
					}
				}
			},
			[&](bsa::fo4::archive& ba2) {
				for (auto& [key, file] : ba2) {
					const auto relative = detail::virtual_to_local_path(key);

					std::vector<std::byte> bytes;
					for (auto& chunk : file) {
						if (chunk.compressed()) {
							if (skip_compressed) {
								continue;
							}
							chunk.decompress();
						}

						const auto chunk_bytes = chunk.as_bytes();
						bytes.reserve(bytes.size() + chunk_bytes.size());
						bytes.insert(bytes.end(), chunk_bytes.begin(), chunk_bytes.end());
					}
					a_callback(relative, bytes);
				}
			},
		};

		std::visit(visiter, _archive);
	}

	version archive::get_version() const noexcept
	{
		return _version;
	}

	const underlying_archive& archive::get_archive() const noexcept
	{
		return _archive;
	}
}  // namespace bsa::all
