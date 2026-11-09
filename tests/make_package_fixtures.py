# SPDX-License-Identifier: MIT
"""Original OPF/OEB and zipped text fixtures, including damaged containers."""
from pathlib import Path
import sys
import zipfile

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
package = '''<package xmlns="http://www.idpf.org/2007/opf" version="2.0">
<metadata><title>IGNORED METADATA</title></metadata><manifest>
<item id="second" href="Text/second.html" media-type="text/html"/>
<item id="first" href="Text/first%20chapter.xhtml#begin" media-type="application/xhtml+xml"/>
<item id="extra" href="Text/extra.txt" media-type="text/plain"/>
<item id="notes" href="missing-notes.html" media-type="text/html"/>
</manifest><spine><itemref idref="first"/><itemref idref="second"/>
<itemref idref="extra"/><itemref idref="notes" linear="no"/></spine></package>'''
chapters = {
    'Text/first chapter.xhtml': '<html><head><title>HIDDEN</title></head><body><h1>FIRST CHAPTER</h1><p>Hello <b>bold</b> Ω.</p></body></html>',
    'Text/second.html': '<meta charset="windows-1252"><p>SECOND CHAPTER Café.</p>'.encode('cp1252'),
    'Text/extra.txt': 'THIRD CHAPTER\nPlain text.',
}
for filename in ('Book.opf', 'Book.oeb'):
    (root / filename).write_text(package)
for filename, data in chapters.items():
    path = root / filename
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data if isinstance(data, bytes) else data.encode('utf-8'))

def archive(filename, members):
    with zipfile.ZipFile(root / filename, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in members.items():
            z.writestr(name, data)

archive('Book.oebzip', {'OEB/book.opf': package, **{'OEB/'+name: data for name, data in chapters.items()}, 'unused/image.png': b'not decoded'})
archive('Book.htmlz', {'index.html': '<h1>HTMLZ START</h1><p>Hello &copy; Ω.</p>', 'metadata.opf': '<metadata/>', 'image.png': b'ignored'})
archive('Legacy.htmlz', {'index.htm': '<meta charset="windows-1252"><p>Café</p>'.encode('cp1252')})
archive('Book.txtz', {'book.txt': 'TXTZ START\r\nCafé Ω.\nTXTZ END'})
archive('Utf16.txtz', {'book.txt': 'UTF16 Ω'.encode('utf-16')})
archive('Ambiguous.htmlz', {'a.html': '<p>One', 'b.html': '<p>Two'})
archive('Ambiguous.txtz', {'a.txt': 'One', 'b.txt': 'Two'})
archive('Empty.htmlz', {'image.png': b'no text'})
archive('Multiple.oebzip', {'a.opf': package, 'b.opf': package})
for name, href in [('Remote', 'https://example.invalid/book.html'), ('Escape', '../secret.html'), ('EncodedEscape', '%2e%2e/secret.html'), ('Absolute', '/secret.html'), ('Backslash', '..%5csecret.html'), ('Query', 'book.html?file=other')]:
    (root / (name+'.opf')).write_text(package.replace('Text/first%20chapter.xhtml#begin', href))
(root / 'Duplicate.opf').write_text(package.replace('id="second"', 'id="first"'))
(root / 'MissingRef.opf').write_text(package.replace('idref="first"', 'idref="unknown"'))
(root / 'MissingFile.opf').write_text(package.replace('Text/first%20chapter.xhtml#begin', 'absent.html'))
(root / 'WrongMedia.opf').write_text(package.replace('application/xhtml+xml', 'image/svg+xml'))
(root / 'Metadata.opf').write_text('<package><metadata><title>Not a book</title></metadata></package>')
(root / 'WrongRoot.opf').write_text('<html><p>Not a package</p></html>')
(root / 'DTD.opf').write_text('<!DOCTYPE package SYSTEM "never-open.dtd">'+package)
(root / 'Entity.opf').write_text('<!DOCTYPE package [<!ENTITY test SYSTEM "never-open.txt">]>'+package)
(root / 'Base.opf').write_text(package.replace('<manifest>', '<manifest xml:base="elsewhere/">'))
# Corrupt selected entry CRC in the central directory, without relying on zlib corruption.
import struct
data = bytearray((root / 'Book.txtz').read_bytes())
central = data.index(b'PK\x01\x02')
struct.pack_into('<I', data, central+16, 1234)
(root / 'BadCRC.txtz').write_bytes(data)
data = bytearray((root / 'Book.txtz').read_bytes())
central = data.index(b'PK\x01\x02')
struct.pack_into('<H', data, 6, 1)
struct.pack_into('<H', data, central+8, 1)
(root / 'Encrypted.txtz').write_bytes(data)

archive('Markdown.txtz', {'index.md': '# MARKDOWN ZIP\n\n**bold** text Ω.'})

archive('LargeMarkdown.txtz', {'book.md': 'x'*(256*1024+1)})
archive('BigDirectory.htmlz', {'index.html': '<p>Readable</p>', **{'x'*440+str(i)+'.bin': b'' for i in range(300)}})
