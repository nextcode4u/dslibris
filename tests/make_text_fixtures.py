# SPDX-License-Identifier: MIT
"""Original small text/document fixtures; no user books or office tools required."""
from pathlib import Path
import struct
import sys
import zipfile

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)

def package(name, members, compression=zipfile.ZIP_DEFLATED):
    with zipfile.ZipFile(root / name, 'w', compression=compression) as archive:
        for path, text in members.items():
            archive.writestr(path, text)

html = '''<!DOCTYPE html><html><head><meta charset="utf-8"><title>HIDDEN TITLE</title>
<style>HIDDEN CSS</style><script>if (a<b) { alert('HIDDEN SCRIPT'); }</script></head>
<body><h1>HTML HEADING</h1><p>Hello <b>bold</b> <i>italic</i> &copy; &#937; &hellip;.</p>
<!----><!-- HIDDEN COMMENT -->
<p><![CDATA[<escaped>&ready Ω]]></p><p>Second<br>line <img alt="picture label" src="never-open.png"></p>
<pre>  code\n  next</pre></body></html>'''
(root / 'Page.html').write_text(html)
(root / 'Page.xhtml').write_text(html.replace('<br>', '<br/>').replace('src="never-open.png">', 'src="never-open.png"/>'))
(root / 'Page.htm').write_text('<p>First<p>Second<br>Third &amp; &notAnEntity;')
(root / 'Legacy.html').write_bytes('<meta charset="windows-1251"><p>Привет</p>'.encode('cp1251'))
(root / 'Utf16.html').write_text('<h1>UTF16 Ω</h1>', encoding='utf-16')
(root / 'Unknown.html').write_text('<meta charset="shift_jis"><p>text</p>')
(root / 'Deep.html').write_text('<div>' * 129 + 'deep')
(root / 'Comment.html').write_text('<!--' + ('x' * 10000) + '--><p>After comment</p>')

fb2 = '''<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
<description><title-info><book-title>HIDDEN META</book-title></title-info></description>
<body><section><title><p>FB2 HEADING</p></title>
<p>Hello <strong>bold</strong> <emphasis>italic</emphasis> Ω.</p>
<empty-line/><poem><stanza><v>Verse one</v><v>Verse two</v></stanza></poem>
</section></body><binary id="cover">HIDDEN IMAGE</binary></FictionBook>'''
(root / 'Book.fb2').write_text(fb2)
(root / 'Legacy.fb2').write_bytes(fb2.replace('UTF-8', 'windows-1251').replace('FB2 HEADING', 'Привет').replace('Ω', 'Я').encode('cp1251'))
(root / 'Entity.fb2').write_text('''<!DOCTYPE FictionBook [<!ENTITY bad SYSTEM "file:///etc/passwd">]><FictionBook><body><p>&bad;</p></body></FictionBook>''')
(root / 'Wrong.fb2').write_text('<other><body>not FictionBook</body></other>')
package('Book.fb2.zip', {'novel.fb2': fb2, '../../never-created.txt': 'Not extracted'})
package('Book.fbz', {'novel.fb2': fb2})
package('Ambiguous.fbz', {'one.fb2': fb2, 'two.fb2': fb2})
package('BadCRC.fbz', {'book.fb2': fb2}, zipfile.ZIP_STORED)
p = root / 'BadCRC.fbz'
data = bytearray(p.read_bytes())
start = 30 + struct.unpack_from('<H', data, 26)[0] + struct.unpack_from('<H', data, 28)[0]
data[start + fb2.index('FB2 HEADING')] ^= 1
p.write_bytes(data)

word = '''<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>
<w:p><w:r><w:t>DOCX START </w:t></w:r><w:r><w:rPr><w:b/></w:rPr><w:t>bold</w:t></w:r><w:r><w:t> end.</w:t></w:r></w:p>
<w:p><w:r><w:t>Second Ω</w:t><w:tab/><w:t>tab</w:t><w:br/><w:t>new line</w:t></w:r>
<w:del><w:r><w:t>HIDDEN DELETE</w:t></w:r></w:del></w:p></w:body></w:document>'''
package('Book.docx', {'word/document.xml': word, 'word/media/ignored.png': b'not decoded'})
package('Missing.docx', {'not-a-document.xml': word})
odt = '''<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0">
<office:automatic-styles/><office:body><office:text><text:h>ODT HEADING</text:h>
<text:p>Hello <text:span>world</text:span> Ω.</text:p><text:p><text:s text:c="3"/>Indented<text:line-break/>Second line</text:p>
</office:text></office:body></office:document-content>'''
package('Book.odt', {'mimetype': 'application/vnd.oasis.opendocument.text', 'content.xml': odt})

rtf = r'''{\rtf1\ansi\ansicpg1252{\fonttbl{\f0 HIDDEN FONT;}}{\info HIDDEN INFO}
RTF START\par Plain {\b bold {\i both}} normal.\par Caf\'e9 \u937? \u-10180?\u-8423?\par
{\*\unknown HIDDEN UNKNOWN}{\pict\bin4 {\}x}{\field{\*\fldinst HIDDEN FIELD}{\fldrslt link label}}
Escaped \\ \{ \}\par{\ansicpg1251 \'cf\'f0\'e8\'e2\'e5\'f2} \'e9}'''
(root / 'Book.rtf').write_text(rtf)
(root / 'Broken.rtf').write_text(r'{\rtf1 missing close')
(root / 'BadHex.rtf').write_text(r"{\rtf1 \'zz}")
(root / 'BadEncoding.rtf').write_text(r'{\rtf1\ansicpg932 text}')
(root / 'BadBin.rtf').write_text(r'{\rtf1\bin12 xxx}')
(root / 'Plain.rtf').write_text('Not RTF')

p = root / 'Book.fbz'
data = bytearray(p.read_bytes())
struct.pack_into('<H', data, 6, struct.unpack_from('<H', data, 6)[0] | 1)
central = data.index(b'PK\x01\x02')
struct.pack_into('<H', data, central + 8, struct.unpack_from('<H', data, central + 8)[0] | 1)
(root / 'Encrypted.fbz').write_bytes(data)
