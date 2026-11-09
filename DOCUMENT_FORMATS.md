# Document support in this fork

The optional BlocksDS build retains DSLibris's two-screen, rotated book layout,
font settings, page controls, bookmarks and saved reading positions. It adds the
following formats to the library and to launcher `argv[1]` selection. Matching is
case-insensitive; directory names resembling books are skipped.

| Files | Reading behavior |
| --- | --- |
| `.epub` | Original EPUB loader, metadata and book indexing |
| `.txt` | UTF-8 text, optional UTF-8 BOM, CR/LF/CRLF newlines, blank lines and tab indentation |
| `.md`, `.markdown` | CommonMark text, headings, bold/italic, lists, code blocks, link labels, image alt text and simple table text |
| `.pdf` | Extracted text reflowed into reader pages; no original page layout or image rendering |

For example, a launcher can pass `sd:/media/Books/Notes.md` or
`sd:/media/Books/Manual.pdf` as one argument to `dslibris-blocksds.nds`.
No argument opens the normal library. Unsupported extensions are not listed.
See [ARGUMENT_LAUNCH.md](ARGUMENT_LAUNCH.md) for the existing mount/path behavior.

## TXT and Markdown

TXT preserves explicit line breaks and wraps long lines/words to the screen.
It does not autodetect legacy code pages or UTF-16. Invalid UTF-8 is replaced with
a replacement character; input bytes cannot inject the reader's formatting controls.

Markdown uses MD4C. Links display their labels and images display alt text;
there is no web browser, image loader or syntax highlighting. Fenced code preserves
indentation but uses the selected reading font. Tables are flattened into rows
with separators. Numeric Unicode entities and common named XML/HTML entities are
decoded; other named entities can remain spelled out. Raw HTML has no rendering
or execution support. This is a book-style rendering of Markdown rather than a
pixel-equivalent HTML preview.

## PDF scope

PDFio reads the PDF structure and decoded streams. The reader extracts text from
text operators, justified arrays and nested Form XObjects. It supports WinAnsi,
limited StandardEncoding, font Differences glyph names and ToUnicode `bfchar` /
`bfrange` maps, including multi-character mappings and binary CID strings. Page
content arrays preserve text/font state across streams.

Reading order follows the PDF content order; columns, rotated text, tables and
positioned text can reflow imperfectly. Images, vector graphics, original fonts,
PDF annotations and interactive links are not displayed. There is no OCR:
image-only/scanned documents report that no readable text layer was found.
Encrypted PDFs, inline binary images and unsupported font encodings are rejected.
MacRoman/MacExpert fonts without ToUnicode maps are not currently decoded.

Limits prevent silently truncating a document:

| Bound | DS | DSi |
| --- | --- | --- |
| TXT/PDF input bytes | 2 MiB | 8 MiB |
| Markdown source (loaded for MD4C) | 256 KiB | 1 MiB |
| Generated page data | 384 KiB | 2 MiB |
| Reader page count | 1024 | 4096 |

A PDF page's combined decoded content streams, and each CMap/Form stream, are
limited to 128 KiB. ToUnicode maps hold up to 8192 mappings per current font, Form
nesting to four levels, PDF operand data to 64 KiB and object strings to 4 KiB.
The PDF decompressed-content/CMap processing budget equals the input-byte limit.
The limits are conservative guards, not measured peak-RAM guarantees: fonts,
PDF metadata, the runtime and other reader state consume additional memory.

Pages are still retained in RAM, as in the upstream reader. Very large books need
a future disk-backed page cache rather than merely raising these limits. Bookmark
and resume page numbers depend on the current font/layout settings; PDF source-page
numbers are not the same as the reader's reflowed page numbers.

## Building and checking

Use the BlocksDS instructions in [README.md](README.md). Format backends and layout
are kept in `source/document/`; `document_book.cpp` adapts their output to the
existing `Book`/`Page` interface. The original devkitARM Makefile is retained for
upstream compatibility and builds the EPUB reader without these optional backends.

Run:

```sh
sh tests/run-core-tests.sh
sh tests/run-argv-tests.sh
sh tests/run-document-tests.sh
```

The document tests build native dependency copies when needed and generate original
PDF fixtures locally. They check TXT newlines, malformed UTF-8, long words, page and
style boundaries, Markdown, compressed and split PDF streams, font encodings,
Unicode ranges/ligatures, nested Forms, limits and failure cases. Reader code runs
under AddressSanitizer/UndefinedBehaviorSanitizer; third-party libraries use their
normal builds. These are not physical-hardware tests.

Local melonDS checks covered EPUB, TXT, Markdown and text/Unicode PDF reading in
DS mode, plus Gloam-to-reader argv launches for EPUB, TXT, Markdown and compressed
PDF text in DSi mode, using separate test SD images. Physical DS/DSi timing, SD-card behavior,
large-document peak memory and broader real-world PDF compatibility remain unverified.
