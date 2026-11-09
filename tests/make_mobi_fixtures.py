# SPDX-License-Identifier: MIT
"""Author minimal PalmDOC/MOBI fixtures, without third-party book content."""
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)

def book(name, text, mobi=False, compressed=False, encrypted=False, encoding=65001, kf8=False):
    payload = text.encode('utf-8' if encoding == 65001 else 'cp1252')
    record0 = struct.pack('>HHIHHHH', 2 if compressed else 1, 0, len(payload), 1, 4096, int(encrypted), 0)
    if mobi:
        # Short, valid MOBI v3 header; optional newer indices are absent.
        if kf8:
            header8 = bytearray(b'\xff' * 264)
            header8[:24] = b'MOBI' + struct.pack('>IIIII', 264, 2, encoding, 42, 8)
            for offset in (68, 72, 76, 80, 84, 100, 108, 112, 180):
                struct.pack_into('>I', header8, offset, 0)
            struct.pack_into('>H', header8, 226, 0)  # no trailing text-record flags
            record0 += header8
        else:
            record0 += b'MOBI' + struct.pack('>IIIII', 24, 2, encoding, 42, 3)
    data = payload
    if compressed:
        # PalmDOC literal runs (up to eight bytes), including non-ASCII bytes.
        data = b''.join(bytes([len(payload[i:i+8])]) + payload[i:i+8] for i in range(0, len(payload), 8))
    header = bytearray(78)
    header[:16] = b'Original fixture'
    header[60:68] = b'BOOKMOBI' if mobi else b'TEXtREAd'
    struct.pack_into('>H', header, 76, 2)
    start = 78+16+2
    table = struct.pack('>IIII', start, 0, start+len(record0), 1)
    (root / name).write_bytes(header+table+b'\0\0'+record0+data)

book('Palm.pdb', 'PALMDOC START\r\nCafé text.\r\nPALMDOC END', encoding=1252)
book('Compressed.prc', 'PALMDOC COMPRESSED Café', compressed=True, encoding=1252)
html = '<html><head><title>HIDDEN</title></head><body><h1>MOBI START</h1><p>Hello <b>bold</b> Ω.</p><p>MOBI END</p></body></html>'
book('Book.mobi', html, mobi=True)
book('Book.azw', html, mobi=True, compressed=True)
book('Legacy.mobi', '<meta charset="windows-1252"><p>Café smart “quotes”.</p>', mobi=True, encoding=1252)
book('Encrypted.azw', html, mobi=True, encrypted=True)
(root / 'Broken.mobi').write_bytes(b'not a book')
# Offset outside the file must fail before dependency parsing.
bad = bytearray((root / 'Book.mobi').read_bytes())
struct.pack_into('>I', bad, 86, 0x7fffffff)
(root / 'BadOffset.mobi').write_bytes(bad)

book('Book.azw3', html, mobi=True, kf8=True)
book('Legacy.azw3', '<meta charset="windows-1252"><p>Café smart “quotes”.</p>', mobi=True, kf8=True, encoding=1252)
