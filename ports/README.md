# Dependency builds

`tools/prepare_dependencies.py` downloads the sources pinned by
[dependencies.json](dependencies.json), verifies SHA-256 hashes and builds them
under ignored `build/deps/`. `--host` uses a separate `build/deps-host/` for native
tests. No dependency binaries or download archives are checked in.

| Dependency | Purpose | Terms retained |
| --- | --- | --- |
| [Expat 2.10.0](https://github.com/libexpat/libexpat/tree/R_2_10_0) | EPUB and preferences XML | MIT, upstream `COPYING` |
| [MD4C 0.6.0](https://github.com/mity/md4c/tree/v0.6.0) | CommonMark parsing | MIT, upstream `LICENSE.md` |
| [PDFio 1.6.6](https://github.com/michaelrsweet/pdfio/tree/v1.6.6) | PDF objects, compression and text streams | Apache-2.0, upstream `LICENSE` and `NOTICE`, including its GPLv2 combination exception |
| [zlib/MiniZip 1.3.2](https://github.com/madler/zlib/tree/v1.3.2) | Headers for upstream's existing MiniZip source | zlib/MiniZip notices |

FreeType 2.14.3, zlib 1.3.2 and libpng 1.6.59 were supplied by Wonderful packages
in the tested BlocksDS build. The older `portlibs/nds/` FreeType archive is used
only by the unchanged devkitARM build, not by `Makefile.blocksds`.

The script copies dependency notices into `build/deps/licenses/` (or the host
counterpart). A binary distributor must include those notices, the reader's GPL
and fork notices, and the toolchain/FreeType/zlib/libpng notices, and provide the
corresponding reader source under its GPL terms. Source downloads and generated
headers remain available in the build directory for inspection.

## Embedded adaptations

The build script makes two repeatable changes in its downloaded PDFio copy:

- Reduce the internal object-string token limit from 128 KiB to 4 KiB. The original
  token sits on the stack; this bound is important on DS. PDFs needing longer
  object strings can fail to open. Content strings use the separate reader lexer.
- Cast PDF writer offsets to `uint64_t` before shifts beyond 31 bits, avoiding
  undefined behavior on the SDK's 32-bit `off_t`. The reader does not write PDFs.

PDFio's seek calls also use [pdfio_compat.c](pdfio_compat.c), which clamps its
end-of-file probe to byte zero for files smaller than 1023 bytes. This avoids
relying on a FAT driver returning exactly POSIX `EINVAL` for a seek before EOF's
negative bound. The shim does not modify the SDK or emulator.

The glyph-name map is generated from PDFio's `examples/pdf2text.c`, with its
copyright and Apache notice retained. The extractor itself is separate reader
code; it retains binary strings (including CID-font NUL bytes), reads ToUnicode
maps and handles nested Form text.

Expat receives a small little-endian configuration. The DS has no OS entropy
provider, so it uses Expat's documented `XML_POOR_ENTROPY` fallback. No external
entity resolver is installed. This reader is intended for local documents; this
configuration is not a hardened network XML service.
