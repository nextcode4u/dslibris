# dslibris

[![C/C++ CI](https://github.com/rhaleblian/dslibris/actions/workflows/build.yml/badge.svg)](https://github.com/rhaleblian/dslibris/actions/workflows/build.yml)

**dslibris** is an
[EPUB](https://en.wikipedia.org/wiki/EPUB)
ebook reader for the Nintendo DS.

![Browser](etc/sample/browser.png)

![Quickstart](etc/sample/quickstart.png)

![A sample page](etc/sample/iliad.png)


## This fork

This is nextcode4u's fork of [rhaleblian/dslibris](https://github.com/rhaleblian/dslibris).
The original reader and its two-screen design are the work of the upstream authors.
Fork additions provide launcher arguments, a current BlocksDS build, and TXT,
Markdown, reflowed PDF, HTML/XHTML, FictionBook, RTF, DOCX, ODT, PalmDOC, DRM-free Mobipocket/Kindle, OPF/OEB packages, HTMLZ/TXTZ, PML/PMLZ and
TCR text alongside EPUB (including `.kepub` containers).

- [Document formats and limits](DOCUMENT_FORMATS.md)
- [Launcher arguments and upstream merges](ARGUMENT_LAUNCH.md)
- [Dependency versions, licenses and embedded adaptations](ports/README.md)

### Build with BlocksDS

Install [current BlocksDS/Wonderful](https://blocksds.skylyrac.net/docs/) and these optional packages:

```sh
wf-pacman -Syu
wf-pacman -S toolchain-gcc-arm-none-eabi-freetype toolchain-gcc-arm-none-eabi-zlib toolchain-gcc-arm-none-eabi-libpng16
make -f Makefile.blocksds -j4
```

Python 3.9+ is required. The first build downloads checksum-pinned Expat, MD4C,
PDFio, libmobi and MiniZip headers, then builds the required libraries under ignored
`build/deps/`. SDK paths default to `/opt/blocksds/core` and `/opt/wonderful`; override
`BLOCKSDS` and `WONDERFUL_TOOLCHAIN` for another installation. On Windows, use WSL
or a supported BlocksDS development environment.

The result is `dslibris-blocksds.nds`. The add-on installation uses
`data/dslibris/font/` and `media/books/`; new settings live in
`data/dslibris/dslibris.xml`. An existing root `dslibris.xml` remains in use when
there is no managed settings file, preserving its reading state and legacy fonts.
Without the managed layout, keep the normal `Font/` and `Book/` folders
from the upstream installation, or use the existing preference settings/argv path
to select other folders. The reader expects its normal fonts; this is an executable
build rather than a replacement SD-card layout.

The upstream devkitARM build below is retained as the EPUB-only compatibility build.
The BlocksDS route uses current FreeType rather than the historical archive in
`portlibs/nds/`. Tested locally with BlocksDS 1.25 and Wonderful GCC 16.2; native tests
and DS-mode emulator checks are described in the document guide.

### Preparing a reader release

```sh
python3 tools/build_release.py --tag v1.6.0-blocksds-argv.1
```

Commit changes first. The script builds the ROM, verifies the pinned font/runtime
sources, and writes ignored `dist/<tag>/` packages: runtime ZIP with the five
Liberation Fonts, standalone NDS, matching source/dependency archives, notices,
build metadata and checksums. It does not publish a GitHub release. The source
archive omits obsolete prebuilt libraries/fonts/books from the upstream legacy
SD distribution; they are not BlocksDS build inputs. Exact font/SDK/library source
pins are in `ports/runtime-dependencies.json`.

### Licensing

The original application remains GPL-2.0-or-later; [COPYING](COPYING) contains the
GPLv2 text. The BlocksDS binary links LGPL-3.0-or-later libmobi and uses the
original application’s later-version option to combine under GPL-3.0-or-later;
[COPYING.GPL3](COPYING.GPL3) supplies that text. This does not remove the original
GPL-2.0-or-later grant from upstream files or the EPUB-only build. New integration files have the limited MIT grant in
[ARGUMENT_LAUNCH_LICENSE.md](ARGUMENT_LAUNCH_LICENSE.md). Dependencies retain their
own terms and notices; PDFio's GPLv2 combination exception is preserved. Include
the required licenses and corresponding source information with any binary release.

## Installation

See the
[Quickstart](https://github.com/rhaleblian/dslibris/wiki/User:-Quickstart)
page in the Wiki.

## Development

### Prerequisites

Install devkitPro's toolchain for ARM, aka `devkitARM`.
Then install the required devkitARM packages:

    sudo dkp-pacman -S $(cat pacman.txt)

### Building

Assure that devkitARM is available to your shell:

    . /opt/devkitpro/devkitarm.sh

then

    make

`dslibris.nds` should show up in the top directory.

## See Also

Blog: <http://ray.haleblian.com/wordpress/dslibris-an-ebook-reader-for-the-nintendo-ds/>

Development Tools: <http://devkitpro.org>

EPUB/W3C/IDPF: [http://idpf.org](https://idpf.org/)
