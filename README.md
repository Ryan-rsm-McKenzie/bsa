[![Main CI](https://github.com/Ryan-rsm-McKenzie/bsa/actions/workflows/cmake.yml/badge.svg)](https://github.com/Ryan-rsm-McKenzie/bsa/actions/workflows/cmake.yml)

# Important Notes

- This library is a work in progress.
- `bsa` uses copy-on-write semantics. Under the hood, it memory maps input files and stores no-copy views into that data, so that the resulting memory profile is very low. As a consequence, this means that archives you wish to read from must be stored on disk. `bsa` can not read from arbitrary input streams.
- `file`'s and `directory`'s have immutable path names. If you wish to change the name, you must create a new instance. This is a consequence of how these paths uniquely identify these instances.
- If the `hash` of one `file` compares equal to the `hash` of another `file`, then they _are_ equal. It doesn't matter if they have different file names, or if they store different data blobs. The game engine uniquely identifies `file`'s based on their `hash` alone.
- The interface accepts and reports utf-8 strings, but that does not mean utf-8 paths are well formed. The game engine has a crippling bug where extended ascii characters can index out-of-bounds, producing unreproducible hashes. It is the user's job to ensure they aren't attempting to store paths which contain such characters. The game engine will accept them, but it will never be able to reproducibly locate them.
