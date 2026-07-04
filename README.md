# PNGDecoder

*A PNG decoder built from scratch in C — because trusting `stb_image.h` is easy, understanding the file format isn't.*

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![zlib](https://img.shields.io/badge/zlib-inflate-blue)
![SDL2](https://img.shields.io/badge/SDL2-rendering-yellow?logo=sdl&logoColor=white)
![Status](https://img.shields.io/badge/status-supported-brightgreen)

---

## 🔎 overview/

A command-line C program that reads the raw bytes of a PNG file, parses its chunk structure by hand, decompresses the pixel stream with zlib, undoes the per-scanline filtering byte-by-byte, and hands the reconstructed RGBA buffer to SDL2 for display in a window.

It exists because "just load a PNG" hides a surprising amount of binary-format and algorithmic detail — chunk framing, CRC verification, zlib inflate, and four different scanline reconstruction filters (Sub, Up, Average, Paeth) that each reference neighboring bytes differently. Building it from the spec, rather than reaching for a library, was the point.

**Scope, by design:** this decoder only targets one PNG profile — 8-bit depth, Truecolor with Alpha (RGBA), no interlacing — and rejects anything else outright rather than guessing.

**What I learned:**
- Low-level binary parsing in C: manual endianness swapping, chunk-length-driven buffer walking, and working directly with `malloc`/`memcpy` instead of higher-level abstractions.
- Implementing pixel-level reconstruction algorithms (PNG's Sub/Up/Average/Paeth filters) directly from the specification, including the neighbor-lookup logic each one depends on.
- Medium-complexity binary format parsing: chunk framing, CRC32 integrity checks, and coordinating zlib's `uncompress` with a manually pre-sized output buffer.

**Status:** feature-complete for its declared scope (8-bit RGBA, non-interlaced), with a clear list of follow-up features already scoped out in the source (see [⚙️ technical/](#️-technical)).

---

## 🕹️ functional/

**Tech stack:** C, [zlib](https://zlib.net/) (for `inflate`/CRC32), [SDL2](https://www.libsdl.org/) (for windowing/rendering).

**Requirements:**
- A C compiler targeting Windows/MSVC (the code uses `fopen_s`/`fread_s`, MSVC's safe CRT variants).
- zlib and SDL2 development libraries.
- The repo ships prebuilt `SDL2.dll` and `zlibwapi.dll` alongside a precompiled `png_decoder.exe`, so it can run without rebuilding.

**Setup:**
1. Clone the repo.
2. Either build `png_decoder.c` against zlib + SDL2, or use the included `png_decoder.exe` directly with the bundled DLLs in the same folder.

**Usage:**
The repo includes a test image, `basn6a08.png` (a standard PNGSuite RGBA sample), which matches the decoder's supported profile out of the box.

On success, the program prints the parsed IHDR info (dimensions, bit depth, color type, compression/filter/interlace methods) to the console, then opens a window and blits the decoded image via an SDL2 texture with alpha blending enabled.

**Controls:** none beyond closing the window — the event loop only listens for `SDL_QUIT`.

**Configuration:** none exposed at runtime — supported bit depth (8), color type (RGBA/6), compression method, filter method, and interlace method (none) are all compile-time constants that the decoder validates against and fails fast on mismatch.

---

## ⚙️ technical/

### Architecture

The pipeline runs linearly through `main()`:

1. **File read** — the whole PNG is read into a single heap buffer.
2. **Signature check** — the first 8 bytes are matched against the PNG magic signature.
3. **Chunk walk** — `parse_chunk()` is called in a loop, reading each chunk's length, 4-byte type, data, and CRC in sequence, advancing a shared offset until an `IEND` chunk is hit. Each chunk's CRC32 is recomputed with zlib and compared against the stored value, logging a warning (not aborting) on mismatch.
4. **Chunk dispatch** — a second pass switches on chunk type: `IHDR` is parsed into a dedicated struct with strict validation, `IDAT` payloads are concatenated into one contiguous buffer (multiple `IDAT` chunks are supported), `gAMA` is explicitly skipped, and anything unrecognized aborts the program.
5. **Decompression** — the concatenated `IDAT` data is inflated in one call via zlib's `uncompress()` into a pre-sized buffer (`scanline * height` bytes, where `scanline` includes the leading filter-type byte per row).
6. **Reconstruction** — each scanline's filter-type byte selects one of four defiltering functions (None/Sub/Up/Average/Paeth), each applied per-pixel across the 4 RGBA bytes using neighbor lookups (`recon_a`, `recon_b`, `recon_c`) that mirror the PNG spec's "raw", "prior", and "prior-raw" byte references.
7. **Output** — the per-row filter bytes are stripped, producing a flat RGBA buffer that's uploaded directly to an `SDL_Texture` (`SDL_PIXELFORMAT_RGBA32`) and rendered.

### Key data structures

- `chunk_t` — generic chunk representation (`data_length`, `type`, raw `data` pointer), used identically for every chunk type before dispatch.
- `chunk_IHDR_t` — decoded header fields (width, height, bit depth, color type, compression/filter/interlace methods).
- `png_props_t` — aggregates the parsed `IHDR` and the final pixel buffer, threaded through the pipeline as the single piece of shared state.

### Hardest decision

Settling on the chunk/data-structure split — a single generic `chunk_t` for the raw parsing pass versus type-specific structs (like `chunk_IHDR_t`) for anything that needed structured fields — so the chunk-walking loop could stay type-agnostic while `IHDR` (the one chunk with real fields to validate) still got proper typing.

### Known limitations (roadmap, as noted directly in the source)

- Only `IHDR`, `IDAT`, and `IEND` are actually parsed; `gAMA` is skipped and everything else (`PLTE`, `tEXt`, `zTXt`, `cHRM`, `sRGB`, `tIME`) causes a hard failure instead of being ignored.
- Only one PNG profile is supported: 8-bit depth, Truecolor with Alpha (color type 6). Grayscale, RGB, Indexed-Color, and Grayscale-with-Alpha are all rejected.
- No support for 1/2/4/16-bit depths.
- Multi-`IDAT` concatenation isn't independently verified for correctness beyond "it doesn't crash."
- The Average filter's correctness hasn't been specifically tested — it's implemented per spec but not verified against edge cases.
- No Adam7 interlacing support.
- Limited integrity testing beyond the CRC32 check per chunk (which currently only warns rather than aborting on mismatch).
