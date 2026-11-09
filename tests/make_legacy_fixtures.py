# SPDX-License-Identifier: MIT
"""Original Palm Markup and TCR dictionary examples, not redistributed books."""
from pathlib import Path
import sys
import zipfile
root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
pml = r'''\xPML START\x
Plain \bbold \iand italic\i\b. Caf\a233 \U03A9 \UD83C\UDF19.
\vHIDDEN COMMENT\v\m="ignored.png"\C0="HIDDEN TOC"
\q="https://never-fetch.invalid"link label\q
\X1Subheading\X1\SpSuper\Sp and \SbSub\Sb.
\FN="note"Footnote text\FN\SB="aside"Sidebar text\SB
Escaped \\ end.\pPML END'''
(root / 'Book.pml').write_bytes(pml.encode('cp1252'))
(root / 'Legacy.pml').write_bytes(b'Caf\xe9')
(root / 'Utf8.pml').write_bytes(b'\xef\xbb\xbf'+'UTF8 Ω'.encode())
for name, text in [('BadUnicode.pml',r'\U00xz'),('Trailing.pml','trailing\\'),('Unknown.pml',r'\ztext'),('Comment.pml',r'\vno close'),('BadParameter.pml',r'\m="no close')]:
    (root/name).write_text(text)
with zipfile.ZipFile(root / 'Book.pmlz','w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('book.pml',pml.encode('cp1252'))
    z.writestr('images/ignored.png',b'not decoded')

def tcr(name, dictionary, payload):
    entries = [dictionary.get(i,b'') for i in range(256)]
    (root/name).write_bytes(b'!!8-Bit!!'+b''.join(bytes([len(entry)])+entry for entry in entries)+payload)
tcr('Book.tcr',{0:b'TCR START\r\n',1:b'Caf\xe9\r\n',2:b'TCR END'},b'\0\1\2')
tcr('Utf8.tcr',{0:'Unicode Ω 🌙'.encode()},b'\0')
tcr('Long.tcr',{0:b'x'*255},b'\0'*64)
(root/'Broken.tcr').write_bytes(b'bad header')
(root/'Truncated.tcr').write_bytes(b'!!8-Bit!!\x04abc')
