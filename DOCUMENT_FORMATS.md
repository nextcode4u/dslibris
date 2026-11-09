# Document support in this fork

The optional BlocksDS build retains DSLibris's two-screen, rotated book layout,
font settings, page controls, bookmarks and saved reading positions. It adds the
following formats to the library and to launcher `argv[1]` selection. Matching is
case-insensitive; directory names resembling books are skipped.

| Files | Reading behavior |
| --- | --- |
| `.epub`, `.kepub` | Original EPUB loader, metadata and book indexing; `.kepub.epub` also matches |
| `.txt`, `.text` | UTF-8 and BOM-marked UTF-16LE/BE text, CR/LF/CRLF newlines, blank lines and tab indentation |
| `.md`, `.markdown` | CommonMark text, headings, bold/italic, lists, code blocks, link labels, image alt text and simple table text |
| `.pdf` | Extracted text reflowed into reader pages; no original page layout or image rendering |
| `.html`, `.htm`, `.xhtml` | Local text, headings, basic emphasis, lists, code blocks and HTML entities |
| `.fb2`, `.fb2.zip`, `.fbz` | FictionBook body text, headings, emphasis, verse and notes; ZIP entries are read without extraction |
| `.rtf` | Rich Text Format paragraphs, direct bold/italic, Unicode and Windows-1252/1251 text |
| `.pdb`, `.prc`, `.mobi`, `.azw`, `.azw3` | PalmDOC or unencrypted Mobipocket/KF8 text, selected by container contents |
| `.docx`, `.odt` | Main document text and simple table rows from the Office/OpenDocument ZIP |
| `.opf`, `.oeb` | Unpacked OPF/OEB package, with local chapters read in spine order |
| `.oebzip` | ZIP containing one OPF package and its local chapters |
| `.htmlz` | One top-level HTML/XHTML document inside ZIP |
| `.txtz` | One TXT/TEXT or Markdown document inside ZIP |
| `.pml`, `.pmlz` | Palm Markup text, basic emphasis, headings, link labels and notes |
| `.tcr` | Dictionary-compressed text, reflowed after bounded decompression |

For example, a launcher can pass `sd:/media/Books/Notes.md` or
`sd:/media/Books/Manual.pdf` as one argument to `dslibris-blocksds.nds`.
No argument opens the normal library. Unsupported extensions are not listed.
See [ARGUMENT_LAUNCH.md](ARGUMENT_LAUNCH.md) for the existing mount/path behavior.

## TXT and Markdown

TXT preserves explicit line breaks and wraps long lines/words to the screen.
A BOM selects UTF-16LE/BE; otherwise the input is UTF-8. Legacy code pages are
not guessed. Invalid Unicode is replaced with a replacement character; input bytes cannot inject the reader's formatting controls.

Markdown uses MD4C. Links display their labels and images display alt text;
there is no web browser, image loader or syntax highlighting. Fenced code preserves
indentation but uses the selected reading font. Tables are flattened into rows
with separators. Numeric Unicode entities and MD4C's complete named HTML entity table are decoded;
unknown names remain literal. Raw HTML has no rendering
or execution support. This is a book-style rendering of Markdown rather than a
pixel-equivalent HTML preview.

## HTML, FictionBook, RTF and office documents

HTML/XHTML reading is a bounded text extractor, not a browser. It preserves
headings, direct bold/italic, paragraphs, simple list markers and preformatted
text. HTML entities and CDATA are decoded; comments, head metadata, scripts and
styles are omitted. Image alt labels can appear but images, CSS, JavaScript and
linked resources are never loaded. UTF-8/UTF-16 BOMs and common declared UTF-8,
Windows-1252/1251 and ISO-8859-1 encodings are supported. Unrecognized encodings
are rejected. Malformed markup can be rejected rather than repaired like a browser.

FictionBook reads body sections, headings, emphasis, poems and notes in document
order. Embedded base64 binaries and description metadata are not rendered. Library
labels use the filename. FB2 ZIPs must contain exactly one `.fb2` document; no
archive entries are extracted to the SD card. Ambiguous archives, encryption,
XML DTDs and damaged ZIP CRCs are rejected.

RTF supports groups, direct bold/italic, Unicode escapes (including surrogate
pairs), paragraph/tab controls, Windows-1252 and Windows-1251 ANSI text. It omits
font/color tables, picture/object data, metadata and ignorable destinations.
Fonts, colors, positioning, complex fields and full word-processor layout are not
reproduced. Unsupported ANSI code pages are rejected.

DOCX reads `word/document.xml`, including direct bold/italic run properties. ODT
reads `content.xml`, with headings, paragraphs, list markers and explicit spacing.
Both flatten table cells and omit pictures, styling resources, headers, footers
and separate notes/attachments. They do not execute macros or fetch links. Binary
`.doc` is a different format and is not accepted as DOCX.

These formats share the existing page/font/bookmark UI. A filename is not proof
of valid content: parsing validates the underlying markup/container and can return
a readable error. XML depth is capped at 128; archive directory scans at 4096
entries; decoded XML also has the normal input-byte limit. HTML tags are capped
at 4 KiB and nesting at 128, while comments and text are streamed. RTF nesting is
capped at 128. FB2/office DTDs are rejected and no external XML entity resolver is enabled.

## OPF/OEB, zipped text and older text formats

Open the `.opf` package with its chapter directories beside it, for example:

```text
Book/
  content.opf
  Text/
    chapter1.xhtml
    chapter2.xhtml
```

OPF/OEB manifest IDs are resolved in spine order, rather than filename or manifest
order. `linear="no"` entries are omitted. HTML/XHTML, OEB1 HTML and plain-text spine
items are supported; an unsupported spine media type reports an error. Metadata-only
OPF sidecars have no book content and report no readable text. `.oeb` accepts the
same package XML; `.oebzip` accepts a ZIP containing exactly one `.opf` plus its
chapter files. These are text readers, not full CSS/layout engines.

Chapter references can contain percent-encoded filenames and fragments. They must
remain under the OPF's directory: absolute paths, remote URLs, queries and escaping
`..` paths are rejected. File paths are checked lexically, as used on FAT SD cards;
do not use desktop symlinks to reference files outside the package. The reader never
fetches web resources. Older OEB public DOCTYPE declarations are accepted without
loading their DTD; internal subsets and `xml:base` overrides are unsupported.

HTMLZ must contain one top-level `.html`, `.htm` or `.xhtml`; TXTZ must contain one
`.txt`, `.text`, `.md` or `.markdown`. PMLZ must contain one `.pml`. Other resources
and metadata are ignored. Multiple matching documents are rejected instead of
silently omitting part of a book. Markdown inside TXTZ uses the normal Markdown
renderer. UTF-16 TXT and declared HTML encodings behave as in standalone files.
ZIP entries are CRC checked and read directly, without extracting files to SD.

PML supports basic bold/italic, chapter headings, paragraphs/page-break separators,
Unicode and decimal character escapes, visible link/footnote/sidebar text and escaped
backslashes. Comments, TOC labels and image references are omitted. Alignment,
font-size changes, images and interactive links are not reproduced. Input defaults
to Windows-1252; a BOM can select UTF-8/UTF-16. Unknown controls are rejected. This
does not add eReader/Plucker support to generic `.pdb` containers.

TCR reads its 256-entry substitution dictionary and bounded text stream. Since TCR
has no charset field, valid UTF-8 is preferred and other byte streams are interpreted
as Windows-1252. A BOM can select UTF-16 after decompression. Other legacy encodings
need conversion. The implementation uses the existing fonts/pages/bookmarks.

OPF XML is capped at 256 KiB, nesting at 64 and manifest/spine entries at 1024.
Archive directory scans are capped at 4096 entries and 128 KiB of stored names.
Markdown inside TXTZ retains the standalone Markdown source limit. A decoded
chapter/archive text entry, and TCR's expanded text, cannot exceed the page-data byte budget (384 KiB on
DS, 2 MiB on DSi). Package XML and all chapters together must also fit the normal
input-byte budget. These are conservative guards, not peak-memory measurements.

## Other formats and conversion

For formats outside the native readers—such as LIT, LRF, CHM, RB, SNB,
eReader/Plucker/zTXT PDB or DjVu with an embedded text layer—use desktop conversion to an unprotected EPUB or UTF-8 TXT.
[calibre's supported-format list](https://manual.calibre-ebook.com/faq.html#what-formats-does-calibre-support-conversion-to-from)
and [conversion command](https://manual.calibre-ebook.com/generated/en/ebook-convert.html)
describe the available input plugins. For example, with calibre installed:

```sh
ebook-convert "Manual.chm" "Manual.epub"
```

Copy the converted book to the SD card's book directory, or launch its path via
argv. Conversion quality depends on the source format; it is a desktop preparation
step, not native DS support. No converter or extra runtime is required on the SD.
Do not rename unsupported files to a supported extension. DRM requires an
unprotected source; this reader does not remove it. Scanned documents and comic
archives need OCR or a separate image/page reader to become readable text.

## PalmDOC and Mobipocket/Kindle

libmobi decodes uncompressed, PalmDOC-compressed and Huff/CDIC-compressed text.
PalmDOC (`TEXt/REAd`) uses Windows-1252; MOBI uses its declared UTF-8 or Windows-1252.
KF8 reconstructs the HTML skeleton/fragment stream before text reflow. Dual-format
books use libmobi's KF8 selection. Images, CSS, embedded fonts, multimedia, original
page layout, interactive links and dictionary lookups are not provided. Library
labels use the filename. PDB/PRC are containers, so unrelated Palm applications or
ebook encodings are not accepted just because their extension matches.

DRM/encrypted books are rejected; no keys or decryption code are included. Kindle
KFX, Print Replica/AZW4, DjVu, CBZ/CBR, binary DOC, CHM and DRM-protected EPUB are
not new supported formats. Convert these to an unprotected, supported text format
first; image-only books need a different image/page renderer.

The record table is checked before loading; at most 4096 records are accepted.
Compressed records and reconstructed text coexist in memory, so these formats
use tighter file limits. Reconstructed raw text is capped at twice the page-data
budget, up to 2 MiB, and actual allocations can still fail below those limits.

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
| Text/PDF/HTML/XML/RTF input bytes | 2 MiB | 8 MiB |
| PalmDOC/MOBI compressed file | 256 KiB | 1 MiB |
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
Unicode ranges/ligatures, nested Forms, HTML/RTF text, FB2 and office ZIPs,
Unicode encodings, entity/DTD handling, CRC checks, PalmDOC/MOBI compression,
legacy encodings, encrypted-file rejection, OPF spine order/path bounds, OEB and text archives,
PML controls, TCR expansion, limits and failure cases. Reader code runs
under AddressSanitizer/UndefinedBehaviorSanitizer; third-party libraries use their
normal builds. These are not physical-hardware tests.

Local melonDS checks covered EPUB, TXT, Markdown and text/Unicode PDF reading in
DS mode, plus Gloam-to-reader argv launches for EPUB, TXT, Markdown and compressed
PDF text in DSi mode. Additional DSi-mode Gloam argv checks opened small authored
HTML, zipped FB2, RTF, DOCX, ODT, PalmDOC, MOBI and KF8/AZW3 files, including
Windows-1252 KF8 text. OPF/OEB ZIP, HTMLZ, Markdown-in-TXTZ, PMLZ, TCR
and `.kepub` argv launches were also exercised with local fixtures. A libmobi Huff/CDIC sample opened and allowed page turns
and bookmarking; encrypted samples were rejected. A 300-book library check
covered navigation beyond entry 255, XML-sensitive filenames, immediate bookmark
saving and page/bookmark restoration after restarting. Native checks also exercised
libmobi's KF8 skeleton/fragment samples. These used separate test SD images;
fixtures, screenshots and third-party sample books are not repository contents.
Physical DS/DSi timing, SD-card behavior,
large-document peak memory and broader real-world PDF compatibility remain unverified.
