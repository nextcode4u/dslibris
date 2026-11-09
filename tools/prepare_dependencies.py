#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fetch pinned dependency source and build local libraries for BlocksDS or tests."""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def fetch(item, cache, destination):
    archive = cache / item['archive']
    if not archive.exists():
        temporary = archive.with_suffix('.part')
        with urllib.request.urlopen(item['url']) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output)
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != item['sha256']:
        raise RuntimeError('Checksum mismatch: ' + str(archive))
    folder = destination / item['directory']
    if not folder.exists():
        with tarfile.open(archive) as source:
            for entry in source.getmembers():
                target = (destination / entry.name).resolve()
                if not target.is_relative_to(destination.resolve()) or entry.issym() or entry.islnk():
                    raise RuntimeError('Unsafe archive member: ' + entry.name)
            source.extractall(destination)
    return folder


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', action='store_true', help='Build native test libraries instead of ARM libraries')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    sdk = Path(os.environ.get('BLOCKSDS', '/opt/blocksds/core'))
    toolchain = Path(os.environ.get('WONDERFUL_TOOLCHAIN', '/opt/wonderful'))
    output = ROOT / 'build' / ('deps-host' if args.host else 'deps')
    cache = ROOT / 'build/downloads'
    output.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    (output / '.ready').unlink(missing_ok=True)
    manifest = json.loads((ROOT / 'ports/dependencies.json').read_text())
    sources = {name: fetch(item, cache, output) for name, item in manifest.items()}
    includes, libraries, notices = output / 'include', output / 'lib', output / 'licenses'
    for folder in (includes, libraries, notices):
        folder.mkdir(exist_ok=True)
    cc = os.environ.get('CC', 'cc') if args.host else str(toolchain / 'toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-gcc')
    ar = os.environ.get('AR', 'ar') if args.host else str(toolchain / 'toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-ar')
    flags = ['-std=gnu11', '-O2', '-ffunction-sections', '-fdata-sections', '-D_POSIX_C_SOURCE=200809L', '-D_DEFAULT_SOURCE', '-DHAVE_TIMEGM=1']
    if not args.host:
        flags += ['-mthumb', '-mcpu=arm946e-s+nofp', '-specs=' + str(sdk / 'sys/crts/ds_arm9.specs')]
    expat = sources['expat']
    shutil.copyfile(ROOT / 'ports/expat_config.h', expat / 'expat_config.h')
    pdfio = sources['pdfio']
    header = pdfio / 'pdfio-private.h'
    # Upstream reserves a 128 KiB token on the stack. Bound object strings on DS.
    text = header.read_text().replace('PDFIO_MAX_STRING\t131072', 'PDFIO_MAX_STRING\t4096')
    header.write_text(text)
    file_source = pdfio / 'pdfio-file.c'
    file_source.write_text(re.sub(r'(?:\(uint64_t\))*obj->offset >>', '((uint64_t)obj->offset) >>', file_source.read_text()))
    units = {
        'expat': [expat / 'lib' / (name + '.c') for name in ('xcs', 'xmlparse', 'xmlrole', 'xmltok')],
        'md4c': [sources['md4c'] / 'src/md4c.c'],
        'pdfio': sorted(pdfio.glob('pdfio-*.c')) + [pdfio / 'ttf.c', ROOT / 'ports/pdfio_compat.c'],
    }
    for name, files in units.items():
        for notice in manifest[name]['notices']:
            shutil.copyfile(sources[name] / notice, notices / (name + '-' + Path(notice).name))
        objects = []
        def compile_one(pair):
            source, target = pair
            extra = ['-Dlseek=reader_pdf_lseek'] if name == 'pdfio' and source.name != 'pdfio_compat.c' else []
            subprocess.run([cc, *flags, *extra, '-I' + str(expat), '-I' + str(expat / 'lib'), '-c', str(source), '-o', str(target)], check=True)
        for source in files:
            obj = output / (name + '-' + source.stem + '.o')
            objects.append((source, obj))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            list(pool.map(compile_one, objects))
        archive = libraries / ('lib' + name + '.a')
        archive.unlink(missing_ok=True)
        subprocess.run([ar, 'rcs', str(archive), *[str(obj) for _, obj in objects]], check=True)
    example = (pdfio / 'examples/pdf2text.c').read_text()
    start = example.index('typedef struct name_map_s')
    end = example.index('\n};', example.index('static name_map_t', start)) + 3
    glyphs = example[start:end].replace('static name_map_t', 'static const name_map_t')
    (includes / 'pdfio_glyph_names.h').write_text('// Derived from PDFio pdf2text.c; Copyright 2022-2025 Michael R Sweet.\n// Apache-2.0; see build/deps/licenses/pdfio-LICENSE and pdfio-NOTICE.\n' + glyphs + '\n')
    headers = [expat / 'lib/expat.h', expat / 'lib/expat_external.h', sources['md4c'] / 'src/md4c.h', pdfio / 'pdfio.h']
    headers += [sources['zlib'] / 'contrib/minizip' / name for name in ('ioapi.h', 'unzip.h', 'ints.h')]
    for header in headers:
        shutil.copyfile(header, includes / header.name)
    for notice in manifest['zlib']['notices']:
        shutil.copyfile(sources['zlib'] / notice, notices / ('zlib-' + Path(notice).name))
    (output / '.ready').write_text('Dependencies built from ports/dependencies.json\n')
    print('Prepared', output)


if __name__ == '__main__':
    main()
