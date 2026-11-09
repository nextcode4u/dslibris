#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the BlocksDS reader, fonts, matching source and redistribution notices."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import zipfile

from prepare_dependencies import fetch

ROOT = Path(__file__).resolve().parents[1]
FONTS = ['LiberationSerif-Regular.ttf', 'LiberationSerif-Bold.ttf',
         'LiberationSerif-Italic.ttf', 'LiberationSerif-BoldItalic.ttf',
         'LiberationSans-Regular.ttf']


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def write_zip(path, files):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)


def write_source(path, files):
    # A fixed tar timestamp makes the source layout independent of local checkout times.
    with tarfile.open(path, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
        for name, data in sorted(files.items()):
            info = tarfile.TarInfo('DSLibris/' + name)
            info.size, info.mode, info.mtime = len(data), 0o644, 0
            archive.addfile(info, io.BytesIO(data))


def build(tag, skip_build=False):
    if not tag or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in tag):
        raise ValueError('Use a simple version tag')
    dirty = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
    if dirty.strip():
        raise ValueError('Commit source changes before preparing a versioned release')
    sdk = Path(os.environ.get('BLOCKSDS', '/opt/blocksds/core'))
    toolchain = Path(os.environ.get('WONDERFUL_TOOLCHAIN', '/opt/wonderful'))
    sdk_version = (sdk/'version.txt').read_text().strip().removeprefix('v')
    compiler = toolchain/'toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-gcc'
    gcc_version = subprocess.check_output([str(compiler), '-dumpfullversion'], text=True).strip()
    if sdk_version != '1.25.0':
        raise ValueError('Review SDK/runtime source pins before packaging another SDK version')
    if not skip_build:
        subprocess.run(['make', '-f', 'Makefile.blocksds', '-j4'], cwd=ROOT, check=True)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    cache, expanded = ROOT/'build/downloads', ROOT/'build/release-deps'
    expanded.mkdir(parents=True, exist_ok=True)
    manifests = {**json.loads((ROOT/'ports/dependencies.json').read_text()),
                 **json.loads((ROOT/'ports/runtime-dependencies.json').read_text())}
    folders = {name: fetch(item, cache, expanded) for name, item in manifests.items()}
    notices = {name: (ROOT/name).read_bytes() for name in ('COPYING', 'COPYING.GPL3', 'ARGUMENT_LAUNCH_LICENSE.md')}
    notices.update({p.name: p.read_bytes() for p in (ROOT/'ports/notices').iterdir() if p.is_file()})
    for name, item in manifests.items():
        for notice in item['notices']:
            content = (folders[name]/notice).read_bytes()
            # libmobi 0.12 ships an empty AUTHORS placeholder, not a notice.
            if Path(notice).name == 'AUTHORS' and not content.strip():
                continue
            if not content.strip():
                raise ValueError('Empty required runtime notice: '+name+'/'+notice)
            notices[name+'-'+Path(notice).name] = content
    for notice in (folders['blocksds-sdk']/'sys/crts').glob('COPYING.*'):
        notices['blocksds-crts-'+notice.name] = notice.read_bytes()
    notices['ATTRIBUTION.md'] = (
        '# DSLibris BlocksDS reader\n\nOriginal reader: Ray Haleblian and upstream contributors.\n'
        'Fork adapters: nextcode4u. Combined BlocksDS build: GPL-3.0-or-later.\n'
        'Includes FreeType Project software (https://freetype.org).\n'
        'Liberation Fonts 2.1.5 retain their SIL Open Font License and reserved names.\n'
        'Dependency terms remain separate; preserve this entire notice collection.\n'
        'Matching source and dependency archives accompany this release.\n'
        'Toolchain runtime provenance: ports/README.md in source.\n').encode()
    binary = (ROOT/'dslibris-blocksds.nds').read_bytes()
    runtime = {'_nds/dsi-frontend/plugins/apps/dslibris-blocksds.nds': binary}
    runtime.update({'data/dslibris/font/'+name: (folders['liberation-fonts']/name).read_bytes() for name in FONTS})
    source = {}
    tracked = subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode().split('\0')
    for name in filter(None, tracked):
        # Old prebuilt distribution assets are not inputs to the BlocksDS build.
        if name.startswith(('test/', 'portlibs/', 'etc/filesystem/')) or Path(name).suffix.lower() in {'.ttf','.otf','.epub','.dldi','.a','.nds','.elf'}:
            continue
        source[name] = (ROOT/name).read_bytes()
    for item in manifests.values():
        source['dependencies/'+item['archive']] = (cache/item['archive']).read_bytes()
    source.update({'release-notices/'+name: data for name,data in notices.items()})
    source['dependencies/README.md'] = (
        '# Matching dependency source\n\nCopy the archives into build/downloads/ before rebuilding.\n'
        'tools/prepare_dependencies.py verifies hashes and applies the recorded embedded adaptations.\n'
        'Use the pinned BlocksDS SDK/libnds and Wonderful package versions from BUILD-INFO.json.\n'
        'Runtime sources/fonts are pinned in ports/runtime-dependencies.json.\n').encode()
    info = {'tag':tag,'commit':commit,'target':'Makefile.blocksds','blocksds':sdk_version,'gcc':gcc_version,
            'freetype':'2.14.3','libpng':'1.6.59','zlib':'1.3.2','dependencies':manifests,
            'runtime_files':{name:checksum(data) for name,data in sorted(runtime.items())}}
    encoded = (json.dumps(info,indent=2)+'\n').encode()
    source['BUILD-INFO.json'] = encoded
    output = ROOT/'dist'/tag
    output.mkdir(parents=True,exist_ok=True)
    write_source(output/f'DSLibris-{tag}-source.tar.gz',source)
    write_zip(output/f'DSLibris-{tag}.zip', {**runtime, **{'licenses/'+n:d for n,d in notices.items()}})
    (output/'dslibris-blocksds.nds').write_bytes(binary)
    (output/'BUILD-INFO.json').write_bytes(encoded)
    files = sorted(p for p in output.iterdir() if p.is_file() and p.name != 'SHA256SUMS')
    (output/'SHA256SUMS').write_text(''.join(checksum(p.read_bytes())+'  '+p.name+'\n' for p in files))
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--skip-build', action='store_true')
    args = parser.parse_args()
    build(args.tag, args.skip_build)
