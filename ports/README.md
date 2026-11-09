# Dependency builds

`tools/prepare_dependencies.py` downloads the sources pinned by
[dependencies.json](dependencies.json), verifies SHA-256 hashes and builds them
under ignored `build/deps/`. `--host` uses a separate `build/deps-host/` for native
tests. No dependency binaries or download archives are checked in.

| Dependency | Purpose | Terms retained |
| --- | --- | --- |
| [Expat 2.10.0](https://github.com/libexpat/libexpat/tree/R_2_10_0) | EPUB and preferences XML | MIT, upstream `COPYING` |
| [MD4C 0.6.0](https://github.com/mity/md4c/tree/v0.6.0) | CommonMark parsing and named HTML entities | MIT, upstream `LICENSE.md` |
| [PDFio 1.6.6](https://github.com/michaelrsweet/pdfio/tree/v1.6.6) | PDF objects, compression and text streams | Apache-2.0, upstream `LICENSE` and `NOTICE`, including its GPLv2 combination exception |
| [libmobi 0.12](https://github.com/bfabiszewski/libmobi/tree/v0.12) | PalmDOC, MOBI and KF8 text/decompression | LGPL-3.0-or-later, upstream `COPYING` and `AUTHORS` |
| [zlib/MiniZip 1.3.2](https://github.com/madler/zlib/tree/v1.3.2) | Headers for upstream's existing MiniZip source | zlib/MiniZip notices |

FreeType 2.14.3, zlib 1.3.2 and libpng 1.6.59 were supplied by Wonderful packages
in the tested BlocksDS build. The older `portlibs/nds/` FreeType archive is used
only by the unchanged devkitARM build, not by `Makefile.blocksds`.

The script copies dependency notices into `build/deps/licenses/` (or the host
counterpart). A binary distributor must include those notices, the reader's GPL
and fork notices (including [GPLv3](../COPYING.GPL3) for the BlocksDS combined build), and the toolchain/FreeType/zlib/libpng notices, and provide the
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

## Mobipocket text-only build

libmobi is built without encryption support or XML/OPF writing. The script skips
`mobi_reconstruct_resources()` in its downloaded `parse_rawml.c`: this reader does
not need decompressed images, audio or embedded fonts. KF8 skeleton/fragment
reconstruction remains enabled; TOC, dictionary and link reconstruction are disabled
at the call site. The exact source archive/hash and this repeatable adaptation are
part of the build recipe. No upstream library source is replaced in the repository.

libmobi retains LGPL-3.0-or-later. The original reader grants GPL-2.0-or-later;
the combined BlocksDS executable uses that later-version option under
GPL-3.0-or-later. Preserve both original notices and dependency notices. Distributing
a static binary also requires corresponding source/build materials sufficient to
rebuild and relink it, including these dependency sources and adaptations; a source
URL alone is not a substitute for fulfilling those terms. The checksum-pinned
script prepares those sources locally. The legacy EPUB-only target does not link
libmobi. New adapter files retain their separate MIT grant.

## Additional format references

The OPF/OEB reader follows manifest IDs and spine order described by the
[EPUB package specification](https://www.w3.org/TR/epub-33/#sec-package-doc).
TCR dictionary framing and PML/HTMLZ conventions were checked against calibre's
[TCR implementation](https://github.com/kovidgoyal/calibre/blob/master/src/calibre/ebooks/compression/tcr.py),
[PML implementation](https://github.com/kovidgoyal/calibre/blob/master/src/calibre/ebooks/pml/pmlconverter.py)
and [HTMLZ input](https://github.com/kovidgoyal/calibre/blob/master/src/calibre/ebooks/conversion/plugins/htmlz_input.py).
These are interoperability references, not linked/vendored dependencies. The bounded
reader adapters and authored fixtures here are separate MIT-licensed code. No
additional library is needed for these formats; ZIP and XML use existing dependencies.
