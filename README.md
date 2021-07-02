# `bsa`
[![Main CI](https://github.com/Ryan-rsm-McKenzie/bsa/actions/workflows/main_ci.yml/badge.svg)](https://github.com/Ryan-rsm-McKenzie/bsa/actions/workflows/main_ci.yml)

### Reading
```cpp
#include <bsa/tes4.hpp>
#include <cstdio>

std::filesystem::path oblivion{ "path/to/oblivion" };
bsa::tes4::archive bsa;
bsa.read(oblivion / "Data/Oblivion - Voices2.bsa");
const auto file = bsa["sound/voice/oblivion.esm/imperial/m"]["testtoddquest_testtoddhappy_00027fa2_1.mp3"];
if (file) {
	const auto bytes = file->as_bytes();
	const auto out = std::fopen("happy.mp3", "wb");
	std::fwrite(bytes.data(), 1, bytes.size_bytes(), out);
	std::fclose(out);
}
```

### Writing
```cpp
#include <bsa/tes4.hpp>
#include <cstddef>
#include <utility>

const char payload[] = { "Hello world!\n" };
bsa::tes4::file f;
f.set_data({ reinterpret_cast<const std::byte*>(payload), sizeof(payload) - 1 });

bsa::tes4::directory d;
d.insert("hello.txt", std::move(f));

bsa::tes4::archive bsa;
bsa.insert("misc", std::move(d));
bsa.archive_flags(bsa::tes4::archive_flag::file_strings | bsa::tes4::archive_flag::directory_strings);
bsa.archive_types(bsa::tes4::archive_type::misc);

bsa.write("example.bsa", bsa::tes4::version::sse);
```

## Important Notes

- This library is a work in progress.
- `bsa` uses copy-on-write semantics. Under the hood, it memory maps input files and stores no-copy views into that data, so that the resulting memory profile is very low. As a consequence, this means that archives you wish to read from must be stored on disk. `bsa` can not read from arbitrary input streams.
- If the `hash` of one `file` compares equal to the `hash` of another `file`, then they _are_ equal. It doesn't matter if they have different file names, or if they store different data blobs. The game engine uniquely identifies `file`'s based on their `hash` alone.
- UTF-8 inputs are not well formed. The game engine has a crippling bug where extended ascii characters can index out-of-bounds, producing unreproducible hashes. It is the user's job to ensure they aren't attempting to store paths which contain such characters. The game engine will accept them, but it will never be able to reproducibly locate them.
- The game engine normalizes paths to use the `\` character instead of the standard `/`. As such, users should be aware that file paths retrieved from the virtual file system may not constitute valid paths on their native file system.
- Avoid writing file paths which are close to the limit of `MAX_PATH`. Bethesda uses fixed buffers everywhere with no input validation, so they will most likely crash the game.
- Make sure to lexically normalize your paths before you pass them. Bethesda uses *really* basic file extension detection methods, and `bsa` replicates those methods.
- Files can not be split into more than 4 chunks inside a ba2. Bethesda uses a fixed buffer to store the chunks, and exceeding that limit will likely crash the game.
