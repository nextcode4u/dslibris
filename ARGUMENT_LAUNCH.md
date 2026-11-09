# Opening an EPUB from a launcher

This fork adds optional `argv[1]` selection to [dslibris](https://github.com/rhaleblian/dslibris),
the upstream EPUB reader by its original contributors. It does not replace the reader,
EPUB parser or reading interface.

A launcher passes the reader executable as `argv[0]` and one book path as `argv[1]`:

```text
argv[0] = sd:/apps/dslibris.nds
argv[1] = sd:/media/Books/My Book.epub
```

`.epub` is matched without case sensitivity. Keep the entire filename in one argv
entry; spaces do not require quoting inside that entry. Standard `sd:/`, `fat:/`,
absolute and relative paths are accepted when available through the mounted
filesystem. An explicit device prefix selects that card before fonts/preferences
are loaded. Relative paths use the initial filesystem working directory.

The requested book's directory becomes the library, and the normal book indexing
and opening path selects it ahead of the saved last-opened book. Uppercase `.EPUB`
files can also be selected even though upstream's library scanner matches lowercase
extensions. The working directory is retained for upstream resource lookups.
Provide upstream's normal fonts/resources and preferences as usual.

No argument, an unsupported extension or an unreadable/empty file leaves the normal
library startup in place, including its existing no-library/no-books error behavior.
The adapter rejects parent paths of 512 bytes or more and basenames of 256 bytes or
more rather than truncating them. Only the first content argument is used.

## Building

Use the original Makefile and upstream README's devkitPro setup. Run `make` from
the repository root, not `make -C` from another working directory: the existing
Makefile uses `PWD` for its bundled FreeType paths.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
make -j4
```

Required devkitPro libraries include libnds/libfat, nds-libexpat, nds-zlib,
nds-bzip2 and nds-libpng; upstream also supplies its FreeType port. The output name
comes from the checkout directory (for example `DSLibris.nds`). The original
Makefile is unchanged. A full devkitARM/libnds 2 build and host tests passed locally;
console launch/reading compatibility still needs testing.

## Source changes

`source/argv_launch.cpp` owns book selection and `include/argv_path.h` owns bounded
path handling. Existing entry points only pass the optional path and choose the
selected book. This branch started from upstream commit
`1fbd100ff81b5c677778a4ffd09eb86fded218c1`.

## Keeping up with upstream

The `argv-launch` branch contains the fork additions; `main` is kept as an upstream
baseline. Startup adapters and host tests are separate files, and the original
loading routines remain responsible for file decoding and emulation. The small
hooks in existing source are the expected merge points.

```sh
git remote add upstream https://github.com/rhaleblian/dslibris.git  # Once, if not already configured.
git fetch upstream
git switch argv-launch
git merge upstream/main
sh tests/run-argv-tests.sh
```

Rebuild after merging. If upstream adds native argument support, retire the adapter
rather than layering a second startup loader over it.

## Validation and licensing

Run `sh tests/run-argv-tests.sh` with a C/C++ compiler supporting AddressSanitizer and
UndefinedBehaviorSanitizer. These tests exercise the actual adapter against small
host API doubles and synthetic files: path bounds, spaces, extension matching,
missing files and one-shot selection. They do not run a DS CPU or prove that a
game/book works on a console.

The source and assets inherited from upstream retain their original authors,
credits and terms. New integration files are MIT-licensed as described in
[ARGUMENT_LAUNCH_LICENSE.md](ARGUMENT_LAUNCH_LICENSE.md); this does not relicense the
application or its dependencies. No game, book or separately supplied BIOS is
added by this fork. No binary release has been published for this change.
