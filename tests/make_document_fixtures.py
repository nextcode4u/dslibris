# SPDX-License-Identifier: MIT
"""Create tiny, original PDF fixtures without copyrighted books or host tools."""
from pathlib import Path
import sys
import zlib

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)


def stream(data, extra=b'', compressed=False):
    if compressed:
        data = zlib.compress(data)
        extra += b' /Filter /FlateDecode'
    return b'<< /Length ' + str(len(data)).encode() + extra + b' >>\nstream\n' + data + b'\nendstream'


def write(name, body, font=None, extra=None, inherited=False, compressed=False):
    resources = b'/Resources << /Font << /F1 4 0 R >>' + (b' /XObject << /Fm 6 0 R >>' if extra else b'') + b' >>'
    objects = [b'<< /Type /Catalog /Pages 2 0 R >>',
               b'<< /Type /Pages /Kids [3 0 R] /Count 1 ' + (resources if inherited else b'') + b' >>',
               b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 300] /Contents 5 0 R ' + (b'' if inherited else resources) + b' >>',
               font or b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>',
               stream(body, compressed=compressed)]
    if extra:
        objects += extra
    data = b'%PDF-1.4\n'
    offsets = [0]
    for i, obj in enumerate(objects, 1):
        offsets.append(len(data))
        data += f'{i} 0 obj\n'.encode() + obj + b'\nendobj\n'
    xref = len(data)
    data += f'xref\n0 {len(offsets)}\n0000000000 65535 f \n'.encode()
    for offset in offsets[1:]:
        data += f'{offset:010} 00000 n \n'.encode()
    data += f'trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode()
    (root / name).write_bytes(data)


body = b'BT /F1 12 Tf 10 20 Td (Hello \\(reader\\) \\200) Tj 0 -12 Td [(spaced) -150 (words)] TJ ET'
write('simple.pdf', body)
write('compressed.pdf', body, compressed=True)
write('inherited.pdf', body, inherited=True)
write('form.pdf', b'/Fm Do', extra=[stream(b'BT /F1 12 Tf (Form text) Tj ET', b' /Type /XObject /Subtype /Form /BBox [0 0 100 100]')])
cmap = b'''/CIDInit /ProcSet findresource begin 12 dict begin begincmap
1 begincodespacerange <0000> <ffff> endcodespacerange
1 beginbfchar <0001> <03A9> endbfchar
1 beginbfrange <0002> <0003> <0041> endbfrange
1 beginbfrange <0004> <0005> [<00E9> <00660069>] endbfrange
endcmap end end'''
font = b'<< /Type /Font /Subtype /Type0 /BaseFont /Synthetic /Encoding /Identity-H /ToUnicode 6 0 R >>'
write('unicode.pdf', b'BT /F1 12 Tf <00010002000300040005> Tj ET', font, [stream(cmap)])
write('unicode-literal.pdf', b'BT /F1 12 Tf (\\000\\001\\000\\002\\000\\003\\000\\004\\000\\005) Tj ET', font, [stream(cmap)])
write('empty.pdf', b'q Q')
write('bad-encoding.pdf', b'BT /F1 12 Tf (bad) Tj ET', b'<< /Type /Font /Subtype /Type0 /Encoding /Identity-H >>')
write('differences.pdf', b'BT /F1 12 Tf (A) Tj ET', b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding << /BaseEncoding /WinAnsiEncoding /Differences [65 /eacute] >> >>')
write('binary-outside-text.pdf', b'(not text) /Ignored BDC BT /F1 12 Tf (Visible) Tj ET EMC')
write('broken-string.pdf', b'BT /F1 12 Tf (unterminated')
(root / 'invalid.pdf').write_bytes(b'This is not a PDF')

# A page's content array shares text/font state between its streams.
write('split-stream.pdf', b'BT /F1 12 Tf ', extra=[stream(b'(Split streams) Tj ET')])
p = root / 'split-stream.pdf'
data = p.read_bytes().replace(b'/Contents 5 0 R', b'/Contents [5 0 R 6 0 R]')
# Rebuild offsets after editing the page dictionary.
parts = data.split(b'xref\n', 1)[0]
import re
offsets = [m.start() for m in re.finditer(rb'(?m)^\d+ 0 obj\n', parts)]
xref = len(parts)
data = parts + f'xref\n0 {len(offsets)+1}\n0000000000 65535 f \n'.encode()
for offset in offsets:
    data += f'{offset:010} 00000 n \n'.encode()
data += f'trailer\n<< /Size {len(offsets)+1} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode()
p.write_bytes(data)
write('oversized-stream.pdf', b' ' * (128*1024+1), compressed=True)
write('inline-image.pdf', b'BI /W 1 /H 1 /BPC 8 /CS /G ID X EI')
