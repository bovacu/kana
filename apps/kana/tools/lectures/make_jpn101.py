# Kana's edition of "First Year Japanese I - Supplemental material and worksheets"
# (Yoko Sato, Mt Hood Community College, CC BY 4.0), for the Lectures screen:
#   python3 make_jpn101.py SOURCE.pdf OUT.pdf
# SOURCE: https://www.mhcc.edu/Japanese/JPN101_Supplement_SatoOER.pdf (76 pages).
# Needs pypdf (pip install pypdf).
#
# What changes, and why: the cover's photograph and its credit page (pages 1-2)
# and the worksheet with illustrations (page 21) are stock pictures from
# photo-ac.com and ac-illust.com, not under the book's licence: they are left
# out. A plain title page takes the cover's place, with the credit, the licence
# and these changes, as CC BY 4.0 asks. Everything else is the author's, as it was
# (the page numbers printed on the pages are the original's).
import sys
from pypdf import PdfReader, PdfWriter

LEFT_OUT = {1, 2, 21}   # pages, from 1

def text(s):
    return s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

def title_page():
    # One US Letter page in the PDF's own fonts (nothing embedded).
    lines = [
        ('F1', 30, 72, 640, 'First Year Japanese I'),
        ('F1', 22, 72, 604, 'JPN101 - Supplemental material and worksheets'),
        ('F2', 16, 72, 560, 'Yoko Sato, Mt Hood Community College'),
        ('F2', 12, 72, 470, '"First Year Japanese I - Supplemental material and worksheets" by Yoko Sato,'),
        ('F2', 12, 72, 454, 'Mt Hood Community College, is licensed under CC BY 4.0:'),
        ('F2', 12, 72, 438, 'https://creativecommons.org/licenses/by/4.0/'),
        ('F2', 12, 72, 410, 'Original: https://www.mhcc.edu/Japanese/JPN101_Supplement_SatoOER.pdf'),
        ('F1', 12, 72, 370, 'Changes made for Kana'),
        ('F2', 12, 72, 352, 'Left out: the cover photograph and its credit (pages 1-2) and the worksheet on'),
        ('F2', 12, 72, 336, 'page 21, whose pictures are stock images not under this licence. This title page'),
        ('F2', 12, 72, 320, 'replaces the cover. Everything else is as the author made it; the page numbers'),
        ('F2', 12, 72, 304, 'printed on the pages are the original\'s.'),
        ('F2', 12, 72, 264, 'This edition is under CC BY 4.0 too.'),
    ]
    content = 'BT\n' + ''.join('/%s %d Tf 1 0 0 1 %d %d Tm (%s) Tj\n' % (f, size, x, y, text(s)) for f, size, x, y, s in lines) + 'ET\n'
    objects = [
        '<< /Type /Catalog /Pages 2 0 R >>',
        '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
        '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >> /Contents 6 0 R >>',
        '<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>',
        '<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>',
        '<< /Length %d >>\nstream\n%sendstream' % (len(content), content),
    ]
    out = b'%PDF-1.4\n'
    offsets = []
    for i, o in enumerate(objects):
        offsets.append(len(out))
        out += ('%d 0 obj\n%s\nendobj\n' % (i + 1, o)).encode('latin-1')
    xref = len(out)
    out += ('xref\n0 %d\n0000000000 65535 f \n' % (len(objects) + 1)).encode()
    out += ''.join('%010d 00000 n \n' % o for o in offsets).encode()
    out += ('trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objects) + 1, xref)).encode()
    return out

def main(source, out):
    import io
    reader = PdfReader(source)
    writer = PdfWriter()
    writer.add_page(PdfReader(io.BytesIO(title_page())).pages[0])
    for i, page in enumerate(reader.pages):
        if i + 1 not in LEFT_OUT:
            writer.add_page(page)
    writer.add_metadata({'/Title': 'First Year Japanese I - Supplemental material and worksheets (Kana edition)', '/Author': 'Yoko Sato',
                         '/Subject': 'CC BY 4.0. Changed for Kana: pages 1, 2 and 21 left out, a title page added.'})
    writer.compress_identical_objects(remove_duplicates=True, remove_unreferenced=True)
    with open(out, 'wb') as f:
        writer.write(f)
    print('%s: %d pages' % (out, len(writer.pages)))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
