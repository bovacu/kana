// The PDF writer (fude/zoom/pdf.c): every file it writes read back here as a
// reader would — the header, the xref (each offset landing on its object), the
// trailer, the page tree, each stream's /Length — and then what the pages say:
// numbers, alpha states, images, text (in Helvetica, and in the app's fonts
// embedded: the glyph ids written, the fonts' programs, widths and ToUnicode
// maps, read back with a reader of its own). It also leaves out.pdf beside the
// suite, pages of everything, to be opened by hand.
#include "zoom/pdf.h"
#include "zoom/mathfont.h"
#include "drawing/base/utf8.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- reading a PDF back ---------------------------------------------------------------------------

static const u8* find(const u8* h, u32 n, const char* s) {
    const u32 k = (u32)strlen(s);
    if(h == NULL || k == 0 || k > n) return NULL;
    for(u32 i = 0; i + k <= n; i++) {
        if(h[i] == (u8)s[0] && memcmp(h + i, s, k) == 0) return h + i;
    }
    return NULL;
}

static u32 count_of(const u8* h, u32 n, const char* s) {
    u32 c = 0;
    const u32 k = (u32)strlen(s);
    for(const u8* p = find(h, n, s); p != NULL; p = find(p + k, n - (u32)(p + k - h), s)) c++;
    return c;
}

static u32 number(const u8** p) {
    u32 v = 0;
    while(**p >= '0' && **p <= '9') { v = v * 10u + (u32)(**p - '0'); (*p)++; }
    return v;
}

#define MAX_OBJECTS 4096u
typedef struct {
    const u8* d;     // the file's first byte
    u32       n;
    u32       size;  // the trailer's /Size
    u32       off[MAX_OBJECTS];
} doc;

#define PARSE(c) do { if(!(c)) { printf("parse failed: %s\n", #c); return false; } } while(0)

// The header, the xref, the trailer: every in-use entry exactly 20 bytes and
// pointing at "N 0 obj".
static b8 parse(doc* D, const u8* d, u32 n) {
    memset(D, 0, sizeof *D);
    D->d = d;
    D->n = n;
    PARSE(n > 64);
    PARSE(memcmp(d, "%PDF-1.4\n%", 10) == 0);
    PARSE(d[10] >= 128 && d[11] >= 128 && d[12] >= 128 && d[13] >= 128 && d[14] == '\n');
    PARSE(memcmp(d + n - 6, "%%EOF\n", 6) == 0);
    const u8* sx = NULL;
    for(u32 i = n - 16; i > 0; i--) {
        if(memcmp(d + i, "startxref\n", 10) == 0) { sx = d + i; break; }
    }
    PARSE(sx != NULL);
    const u8* p = sx + 10;
    const u32 xref = number(&p);
    PARSE(*p == '\n' && p + 7 == d + n);
    PARSE(xref < n);
    p = d + xref;
    PARSE(memcmp(p, "xref\n0 ", 7) == 0);
    p += 7;
    const u32 count = number(&p);
    PARSE(*p == '\n');
    p++;
    PARSE(count >= 2 && count <= MAX_OBJECTS);
    PARSE(p + 20u * count < d + n);
    PARSE(memcmp(p, "0000000000 65535 f\r\n", 20) == 0);
    for(u32 k = 1; k < count; k++) {
        const u8* e = p + 20u * k;
        u32 off = 0;
        for(u32 i = 0; i < 10; i++) {
            PARSE(e[i] >= '0' && e[i] <= '9');
            off = off * 10u + (u32)(e[i] - '0');
        }
        PARSE(memcmp(e + 10, " 00000 n\r\n", 10) == 0);
        char head[32];
        const int h = snprintf(head, sizeof head, "%u 0 obj\n", k);
        PARSE(off < xref && memcmp(d + off, head, (size_t)h) == 0);
        PARSE(off == 15 || d[off - 1] == '\n');
        D->off[k] = off;
    }
    p += 20u * count;
    char tr[96];
    const int t = snprintf(tr, sizeof tr, "trailer\n<< /Size %u /Root 1 0 R >>\n", count);
    PARSE(memcmp(p, tr, (size_t)t) == 0 && p + t == sx);
    D->size = count;
    return true;
}

// Object _k's dictionary (from its "<<" to just past its ">>"), and whether a
// stream follows it.
static const u8* dict(const doc* D, u32 k, u32* len, b8* has_stream) {
    if(k == 0 || k >= D->size) return NULL;
    char head[32];
    const int h = snprintf(head, sizeof head, "%u 0 obj\n", k);
    const u8* b = D->d + D->off[k] + h;
    const u32 rest = D->n - (u32)(b - D->d);
    const u8* end = find(b, rest, ">>\nendobj\n");
    const u8* st  = find(b, rest, ">>\nstream\n");
    if(b[0] != '<' || b[1] != '<') return NULL;
    if(st != NULL && (end == NULL || st < end)) {
        *len = (u32)(st - b) + 2u;
        if(has_stream) *has_stream = true;
        return b;
    }
    if(end == NULL) return NULL;
    *len = (u32)(end - b) + 2u;
    if(has_stream) *has_stream = false;
    return b;
}

static u32 value(const u8* s, u32 n, const char* key) {
    const u8* p = find(s, n, key);
    if(p == NULL) return 0;
    p += strlen(key);
    return number(&p);
}

// Object _k's stream: its /Length exactly the bytes between "stream\n" and
// "\nendstream\nendobj\n". NULL when that does not hold.
static const u8* stream(const doc* D, u32 k, u32* len) {
    u32 dn;
    b8  has = false;
    const u8* d = dict(D, k, &dn, &has);
    if(d == NULL || !has) return NULL;
    const u8* p = find(d, dn, "/Length ");
    if(p == NULL) return NULL;
    p += 8;
    const u32 length = number(&p);
    const u8* data = d + dn + 8;   // ">>" then "\nstream\n"
    if(data + length + 18 > D->d + D->n || memcmp(data + length, "\nendstream\nendobj\n", 18) != 0) return NULL;
    *len = length;
    return data;
}

// Page _i's object (0 the first), through the catalog and the page tree.
static u32 page_object(const doc* D, u32 i) {
    u32 n;
    const u8* cat = dict(D, 1, &n, NULL);
    if(cat == NULL || find(cat, n, "/Type /Catalog") == NULL) return 0;
    const u32 pages = value(cat, n, "/Pages ");
    const u8* tree = dict(D, pages, &n, NULL);
    if(tree == NULL || find(tree, n, "/Type /Pages") == NULL) return 0;
    const u8* kids = find(tree, n, "/Kids [");
    if(kids == NULL) return 0;
    const u8* p = kids + 7;
    for(u32 j = 0;; j++) {
        const u32 obj = number(&p);
        if(memcmp(p, " 0 R", 4) != 0) return 0;
        p += 4;
        if(j == i) return obj;
        if(*p != ' ') return 0;
        p++;
    }
}

static u32 page_count(const doc* D) {
    u32 n;
    const u8* cat = dict(D, 1, &n, NULL);
    if(cat == NULL) return 0;
    const u8* tree = dict(D, value(cat, n, "/Pages "), &n, NULL);
    return tree ? value(tree, n, "/Count ") : 0;
}

static const u8* page_content(const doc* D, u32 i, u32* len) {
    u32 n;
    const u8* page = dict(D, page_object(D, i), &n, NULL);
    if(page == NULL) return NULL;
    return stream(D, value(page, n, "/Contents "), len);
}

// The first object whose dictionary holds _key.
static u32 object_with(const doc* D, const char* key) {
    for(u32 k = 1; k < D->size; k++) {
        u32 n;
        const u8* d = dict(D, k, &n, NULL);
        if(d != NULL && find(d, n, key) != NULL) return k;
    }
    return 0;
}

// Every object well formed: a dictionary, and a stream's /Length exact.
static b8 objects_sound(const doc* D) {
    for(u32 k = 1; k < D->size; k++) {
        u32 n, sn;
        b8  has = false;
        if(dict(D, k, &n, &has) == NULL) { printf("object %u: no dictionary\n", k); return false; }
        if(has && stream(D, k, &sn) == NULL) { printf("object %u: its /Length is wrong\n", k); return false; }
    }
    return true;
}

static b8 same(const u8* p, u32 n, const char* s) {
    if(p != NULL && n == strlen(s) && memcmp(p, s, n) == 0) return true;
    printf("  expected: \"%s\"\n  got:      \"%.*s\"\n", s, p ? (int)n : 4, p ? (const char*)p : "NULL");
    return false;
}

// Finished, parsed and sound.
static b8 finish(fude_zoom_pdf* pdf, fude_bytes* out, doc* D) {
    *out = fude_bytes_new(1024);
    if(!fude_zoom_pdf_finish(pdf, out)) return false;
    return parse(D, out->memory, fude_bytes_size(out)) && objects_sound(D);
}

static doc D;   // big: one, reused

// --- numbers --------------------------------------------------------------------------------------

static void test_numbers(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 200, 100);
    fude_zoom_pdf_move(pdf, 0.1, -0.00004);
    fude_zoom_pdf_line(pdf, 1234567.891, 2.5);
    fude_zoom_pdf_line(pdf, -12.3456, 100.0);
    fude_zoom_pdf_line(pdf, NAN, INFINITY);
    fude_zoom_pdf_line(pdf, 1e20, -1e20);
    fude_zoom_pdf_line(pdf, 1e-20, 0.99999);
    fude_zoom_pdf_line(pdf, -0.0, 3.10);
    fude_zoom_pdf_line(pdf, -0.00006, 0.30000);
    fude_zoom_pdf_curve(pdf, 1, 2, 3, 4, 5, 6);
    fude_zoom_pdf_rect(pdf, 10, 20, 30.5, 40);
    fude_zoom_pdf_close(pdf);
    fude_zoom_pdf_stroke(pdf);
    // A small scale keeps its digits; a shift does not need them.
    fude_zoom_pdf_transform(pdf, 0.000123, 0, -0.0, 0.000123, 1.23456, -7);
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 n;
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "0.1 0 m\n1234567.891 2.5 l\n-12.3456 100 l\n0 0 l\n1000000000 -1000000000 l\n0 1 l\n0 3.1 l\n-0.0001 0.3 l\n"
                     "1 2 3 4 5 6 c\n10 20 30.5 40 re\nh\nS\n0.000123 0 0 0.000123 1.2346 -7 cm\n"));
    CHECK(c != NULL && find(c, n, "e+") == NULL && find(c, n, "e-") == NULL && find(c, n, "-0 ") == NULL);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

// --- structure ------------------------------------------------------------------------------------

static void test_structure(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    // Outside a page: nothing.
    fude_zoom_pdf_move(pdf, 1, 1);
    fude_zoom_pdf_stroke(pdf);
    fude_zoom_pdf_text(pdf, 1, 1, 10, (rde_color){ 0, 0, 0, 255 }, "nothing");
    fude_zoom_pdf_page_begin(pdf, 210.0 * FUDE_ZOOM_PDF_MM, 297.0 * FUDE_ZOOM_PDF_MM);
    fude_zoom_pdf_rect(pdf, 0, 0, 10, 10);
    fude_zoom_pdf_fill(pdf, false);
    fude_zoom_pdf_page_begin(pdf, 612, 792);   // ends A4 by itself
    fude_zoom_pdf_stroke(pdf);
    fude_zoom_pdf_page_end(pdf);
    fude_zoom_pdf_page_end(pdf);               // twice: nothing
    fude_zoom_pdf_stroke(pdf);                 // between pages: nothing
    fude_zoom_pdf_page_begin(pdf, NAN, 1.0);   // too small: the least PDF allows
    // The file appended after what _out holds; its offsets from its own start.
    fude_bytes out = fude_bytes_new(16);
    fude_put_data(&out, "junk before", 11);
    CHECK(fude_zoom_pdf_finish(pdf, &out));
    CHECK(parse(&D, out.memory + 11, fude_bytes_size(&out) - 11));
    CHECK(objects_sound(&D));
    CHECK(page_count(&D) == 3);
    u32 n;
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "0 0 10 10 re\nf\n"));
    c = page_content(&D, 1, &n);
    CHECK(same(c, n, "S\n"));
    c = page_content(&D, 2, &n);
    CHECK(c != NULL && n == 0);
    const char* boxes[3] = { "/MediaBox [0 0 595.2756 841.8898]", "/MediaBox [0 0 612 792]", "/MediaBox [0 0 3 3]" };
    for(u32 i = 0; i < 3; i++) {
        const u8* page = dict(&D, page_object(&D, i), &n, NULL);
        CHECK(page != NULL && find(page, n, "/Type /Page ") != NULL && find(page, n, boxes[i]) != NULL);
        CHECK(page != NULL && find(page, n, "/Parent 2 0 R") != NULL && find(page, n, "/Resources 3 0 R") != NULL);
    }
    // Nothing used: no states, images or font in the resources.
    const u8* res = dict(&D, 3, &n, NULL);
    CHECK(res != NULL && find(res, n, "/ExtGState") == NULL && find(res, n, "/XObject") == NULL && find(res, n, "/Font") == NULL);
    CHECK(find(out.memory, fude_bytes_size(&out), "/Helvetica") == NULL);
    // Finishing again writes the same file: the writer is left as it was.
    fude_bytes again = fude_bytes_new(16);
    CHECK(fude_zoom_pdf_finish(pdf, &again));
    CHECK(fude_bytes_size(&again) == fude_bytes_size(&out) - 11 && memcmp(again.memory, out.memory + 11, fude_bytes_size(&again)) == 0);
    rde_arr_free(&again);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);

    // No pages: no file, and nothing appended.
    pdf = fude_zoom_pdf_new();
    out = fude_bytes_new(16);
    fude_put_u8(&out, 7);
    CHECK(!fude_zoom_pdf_finish(pdf, &out));
    CHECK(fude_bytes_size(&out) == 1);
    CHECK(!fude_zoom_pdf_finish(NULL, &out));
    CHECK(!fude_zoom_pdf_finish(pdf, NULL));
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    fude_zoom_pdf_free(NULL);
}

// --- graphics state -------------------------------------------------------------------------------

static void test_state(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 0, 0, 0, 255 });       // the default: nothing
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });     // the same: nothing
    fude_zoom_pdf_save(pdf);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 0, 0, 255, 255 });
    fude_zoom_pdf_line_width(pdf, 2.5);
    fude_zoom_pdf_restore(pdf);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });     // red again since the restore: nothing
    fude_zoom_pdf_line_width(pdf, 1.0);                               // the default again: nothing
    fude_zoom_pdf_line_width(pdf, 0.5);
    fude_zoom_pdf_line_width(pdf, 0.5);
    fude_zoom_pdf_line_width(pdf, -3.0);                              // the thinnest
    fude_zoom_pdf_stroke_color(pdf, (rde_color){ 0, 128, 255, 255 });
    fude_zoom_pdf_line_style(pdf, 1, 7);
    const f64 dashes[2] = { 3, 2.25 };
    fude_zoom_pdf_dash(pdf, dashes, 2, 1);
    fude_zoom_pdf_dash(pdf, NULL, 0, 5);
    const f64 bad[2] = { 3, -1 };
    fude_zoom_pdf_dash(pdf, bad, 2, 1);
    const f64 zeros[2] = { 0, 0 };
    fude_zoom_pdf_dash(pdf, zeros, 2, 1);
    fude_zoom_pdf_restore(pdf);                                       // nothing saved: ignored
    fude_zoom_pdf_save(pdf);
    fude_zoom_pdf_save(pdf);                                          // left open: closed at the page's end
    const f64 xy[6] = { 1, 2, 3, 4, 5, 6 };
    fude_zoom_pdf_polyline(pdf, xy, 3, true);
    fude_zoom_pdf_polyline(pdf, xy, 2, false);
    fude_zoom_pdf_fill(pdf, true);
    fude_zoom_pdf_fill_stroke(pdf, false);
    fude_zoom_pdf_fill_stroke(pdf, true);
    fude_zoom_pdf_clip(pdf, false);
    fude_zoom_pdf_clip(pdf, true);
    fude_zoom_pdf_page_end(pdf);
    // A new page starts from the defaults, whatever the last one set.
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });
    fude_zoom_pdf_line_width(pdf, 0.5);
    // A long polyline in blocks: every point there.
    f64* many = (f64*)malloc(sizeof(f64) * 2u * 10000u);
    for(u32 i = 0; i < 10000u; i++) { many[2 * i] = i * 0.37; many[2 * i + 1] = sin(i * 0.01) * 300.0; }
    fude_zoom_pdf_polyline(pdf, many, 10000u, false);
    fude_zoom_pdf_stroke(pdf);
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 n;
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "1 0 0 rg\nq\n0 0 1 rg\n2.5 w\nQ\n0.5 w\n0 w\n0 0.502 1 RG\n1 J 2 j\n[3 2.25] 1 d\n[] 0 d\n[] 0 d\n[] 0 d\n"
                     "q\nq\n1 2 m\n3 4 l\n5 6 l\nh\n1 2 m\n3 4 l\nf*\nB\nB*\nW n\nW* n\nQ\nQ\n"));
    c = page_content(&D, 1, &n);
    CHECK(c != NULL && n > 10000u * 6u);
    const char* start = "1 0 0 rg\n0.5 w\n0 0 m\n0.37 ";
    CHECK(c != NULL && n > strlen(start) && memcmp(c, start, strlen(start)) == 0);
    CHECK(count_of(c, n, " l\n") == 9999u && count_of(c, n, " m\n") == 1u);
    CHECK(c != NULL && n > 2 && memcmp(c + n - 2, "S\n", 2) == 0);
    free(many);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

// --- alpha ----------------------------------------------------------------------------------------

static void test_alpha(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 128 });
    fude_zoom_pdf_rect(pdf, 0, 0, 10, 10);
    fude_zoom_pdf_fill(pdf, false);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 0, 0, 255, 128 });     // the same alpha: the colour only
    fude_zoom_pdf_stroke_color(pdf, (rde_color){ 0, 0, 0, 128 });     // stroke alpha: its own state
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 128 });     // a new page: said again, the same state
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });     // opaque again
    fude_zoom_pdf_save(pdf);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 128 });
    fude_zoom_pdf_restore(pdf);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 255, 0, 0, 255 });     // opaque since the restore: nothing
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 n;
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "1 0 0 rg\n/af128 gs\n0 0 10 10 re\nf\n0 0 1 rg\n/as128 gs\n"));
    c = page_content(&D, 1, &n);
    CHECK(same(c, n, "1 0 0 rg\n/af128 gs\n/af255 gs\nq\n/af128 gs\nQ\n"));
    // Three states for the whole document, each referred to by name.
    CHECK(count_of(out.memory, fude_bytes_size(&out), "/Type /ExtGState") == 3);
    const u8* res = dict(&D, 3, &n, NULL);
    const u32 f128 = value(res, n, "/af128 "), f255 = value(res, n, "/af255 "), s128 = value(res, n, "/as128 ");
    CHECK(res != NULL && find(res, n, "/ExtGState <<") != NULL);
    const u8* s = dict(&D, f128, &n, NULL);
    CHECK(same(s, n, "<< /Type /ExtGState /ca 0.502 >>"));
    s = dict(&D, f255, &n, NULL);
    CHECK(same(s, n, "<< /Type /ExtGState /ca 1 >>"));
    s = dict(&D, s128, &n, NULL);
    CHECK(same(s, n, "<< /Type /ExtGState /CA 0.502 >>"));
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

// --- JPEG -----------------------------------------------------------------------------------------

// A 16 by 12 JPEG (red, green / blue, yellow quarters), written by sips.
static const u8 jpeg_16x12[916] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00,
    0xff, 0xe1, 0x00, 0x80, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4d, 0x4d, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04,
    0x01, 0x1a, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x01, 0x1b, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x46, 0x01, 0x28, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x87, 0x69, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0xa0, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x00, 0xa0, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0xa0, 0x03, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xff, 0xed, 0x00, 0x38, 0x50, 0x68, 0x6f, 0x74, 0x6f, 0x73,
    0x68, 0x6f, 0x70, 0x20, 0x33, 0x2e, 0x30, 0x00, 0x38, 0x42, 0x49, 0x4d, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x42, 0x49, 0x4d, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
    0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x0c, 0x00, 0x10, 0x03, 0x01, 0x22,
    0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x1f, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0xff, 0xc4, 0x00, 0xb5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01,
    0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14,
    0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5,
    0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xff, 0xc4, 0x00, 0x1f, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5,
    0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02,
    0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42,
    0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1,
    0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
    0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff,
    0xdb, 0x00, 0x43, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x04, 0x04, 0x06, 0x09, 0x06, 0x06, 0x06, 0x09, 0x0c,
    0x09, 0x09, 0x09, 0x09, 0x0c, 0x0f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0f, 0x12, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x12,
    0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1c, 0x1c,
    0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x04, 0x05, 0x05, 0x07, 0x07, 0x07, 0x0c,
    0x07, 0x07, 0x0c, 0x1d, 0x14, 0x10, 0x14, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
    0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
    0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0xff, 0xdd, 0x00,
    0x04, 0x00, 0x01, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xf0, 0x0f, 0xed,
    0xed, 0x27, 0xfe, 0x7b, 0xff, 0x00, 0xe3, 0xad, 0xfe, 0x15, 0xee, 0x7f, 0xf0, 0xae, 0x3c, 0x67, 0xff, 0x00, 0x40, 0xef,
    0xfc, 0x8d, 0x0f, 0xff, 0x00, 0x17, 0x5f, 0x22, 0xd7, 0xeb, 0x05, 0x78, 0x5e, 0x38, 0xf0, 0xde, 0x17, 0x83, 0x7f, 0xb3,
    0xff, 0x00, 0xb2, 0xe5, 0x29, 0xfb, 0x6f, 0x6b, 0xcd, 0xed, 0x1a, 0x76, 0xe4, 0xf6, 0x76, 0xb7, 0x2a, 0x8f, 0xf3, 0x3b,
    0xde, 0xfd, 0x36, 0xeb, 0xef, 0xe7, 0x78, 0xfa, 0x9c, 0x6d, 0xec, 0xff, 0x00, 0xb5, 0x52, 0x87, 0xb0, 0xbf, 0x2f, 0xb3,
    0xd2, 0xfc, 0xf6, 0xbd, 0xf9, 0x9c, 0xff, 0x00, 0x91, 0x5a, 0xd6, 0xeb, 0xbf, 0x4f, 0xff, 0xd9,
};

// A JPEG's markers as far as a little past its frame header: what is read.
static u32 make_jpeg(u8* b, u8 sof, u8 bits, u16 w, u16 h, u8 comps, b8 adobe, b8 pad) {
    u32 n = 0;
    const u8 soi_app0[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0 };
    memcpy(b, soi_app0, sizeof soi_app0);
    n += sizeof soi_app0;
    if(adobe) {
        const u8 app14[] = { 0xFF, 0xEE, 0x00, 0x0E, 'A', 'd', 'o', 'b', 'e', 0x00, 0x64, 0, 0, 0, 0, 2 };
        memcpy(b + n, app14, sizeof app14);
        n += sizeof app14;
    }
    const u8 dht[] = { 0xFF, 0xC4, 0x00, 0x03, 0x00 };   // tables may come before the frame
    memcpy(b + n, dht, sizeof dht);
    n += sizeof dht;
    if(pad) { b[n++] = 0xFF; b[n++] = 0xFF; }
    b[n++] = 0xFF; b[n++] = sof;
    b[n++] = 0; b[n++] = (u8)(8 + 3 * comps);
    b[n++] = bits;
    b[n++] = (u8)(h >> 8); b[n++] = (u8)h;
    b[n++] = (u8)(w >> 8); b[n++] = (u8)w;
    b[n++] = comps;
    for(u8 i = 0; i < comps; i++) { b[n++] = (u8)(i + 1); b[n++] = 0x11; b[n++] = 0; }
    const u8 sos_eoi[] = { 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x12, 0x34, 0xFF, 0xD9 };
    memcpy(b + n, sos_eoi, sizeof sos_eoi);
    return n + sizeof sos_eoi;
}

static void test_jpeg(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    u8 b[128];
    u32 n;
    CHECK(fude_zoom_pdf_jpeg(pdf, jpeg_16x12, sizeof jpeg_16x12) == 1);
    n = make_jpeg(b, 0xC0, 8, 300, 1000, 1, false, true);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 2);
    n = make_jpeg(b, 0xC2, 8, 64, 48, 4, true, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 3);
    n = make_jpeg(b, 0xC1, 8, 65535, 1, 4, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 4);
    n = make_jpeg(b, 0xC0, 8, 5, 6, 3, true, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 5);
    // What DCTDecode cannot read, or not a JPEG at all: 0, and nothing added.
    n = make_jpeg(b, 0xC3, 8, 5, 6, 3, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 0);      // lossless
    n = make_jpeg(b, 0xC9, 8, 5, 6, 3, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 0);      // arithmetic
    n = make_jpeg(b, 0xC0, 12, 5, 6, 3, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 0);      // 12 bits
    n = make_jpeg(b, 0xC0, 8, 5, 6, 2, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 0);      // two components
    n = make_jpeg(b, 0xC0, 8, 0, 6, 3, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, n) == 0);      // no width
    n = make_jpeg(b, 0xC0, 8, 5, 6, 3, false, false);
    CHECK(fude_zoom_pdf_jpeg(pdf, b, 31) == 0);     // cut inside the frame header
    CHECK(fude_zoom_pdf_jpeg(pdf, b, 2) == 0);
    const u8 scan_first[] = { 0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00 };
    CHECK(fude_zoom_pdf_jpeg(pdf, scan_first, sizeof scan_first) == 0);
    const u8 png[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    CHECK(fude_zoom_pdf_jpeg(pdf, png, sizeof png) == 0);
    const u8 lies[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0xFF, 0xFF, 0x00 };   // a length past the end
    CHECK(fude_zoom_pdf_jpeg(pdf, lies, sizeof lies) == 0);
    CHECK(fude_zoom_pdf_jpeg(pdf, NULL, 10) == 0);
    CHECK(fude_zoom_pdf_jpeg(NULL, jpeg_16x12, sizeof jpeg_16x12) == 0);

    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_image(pdf, 1, 160, 0, 0, 120, 10, 20);
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 len;
    const u8* res = dict(&D, 3, &len, NULL);
    CHECK(res != NULL && find(res, len, "/XObject << /Im1 ") != NULL && find(res, len, "/Im6") == NULL);
    const char* want[5] = {
        "/Width 16 /Height 12 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length 916 >>",
        "/Width 300 /Height 1000 /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /DCTDecode /Length",
        "/Width 64 /Height 48 /ColorSpace /DeviceCMYK /BitsPerComponent 8 /Filter /DCTDecode /Decode [1 0 1 0 1 0 1 0] /Length",
        "/Width 65535 /Height 1 /ColorSpace /DeviceCMYK /BitsPerComponent 8 /Filter /DCTDecode /Length",   // no Adobe: as it is
        "/Width 5 /Height 6 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length",        // Adobe RGB: as it is
    };
    for(u32 i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof name, "/Im%u ", i + 1);
        const u32 obj = value(res, len, name);
        u32 dn;
        const u8* d = dict(&D, obj, &dn, NULL);
        CHECK(d != NULL && find(d, dn, "/Type /XObject /Subtype /Image") != NULL && find(d, dn, want[i]) != NULL);
        if(i == 0) {
            u32 sn;
            const u8* s = stream(&D, obj, &sn);
            CHECK(s != NULL && sn == sizeof jpeg_16x12 && memcmp(s, jpeg_16x12, sn) == 0);
        }
    }
    const u8* c = page_content(&D, 0, &len);
    CHECK(same(c, len, "q\n160 0 0 120 10 20 cm\n/Im1 Do\nQ\n"));
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

// --- RGBA -----------------------------------------------------------------------------------------

static void test_rgba(void) {
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    const u8 opaque[16] = { 10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255, 100, 110, 120, 255 };
    const u8 clear[12]  = { 1, 2, 3, 255, 4, 5, 6, 128, 7, 8, 9, 0 };
    CHECK(fude_zoom_pdf_rgba(pdf, opaque, 2, 2) == 1);
    CHECK(fude_zoom_pdf_rgba(pdf, clear, 3, 1) == 2);
    CHECK(fude_zoom_pdf_rgba(pdf, NULL, 3, 1) == 0);
    CHECK(fude_zoom_pdf_rgba(pdf, clear, 0, 1) == 0);
    CHECK(fude_zoom_pdf_rgba(pdf, clear, 3, 0) == 0);
    CHECK(fude_zoom_pdf_rgba(pdf, clear, 0xFFFFFFFFu, 0xFFFFFFFFu) == 0);   // too big to be: refused before it is read
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_image(pdf, 2, 100, 0, 0, 50, 10, 20);
    fude_zoom_pdf_image(pdf, 3, 1, 0, 0, 1, 0, 0);     // no such image
    fude_zoom_pdf_image(pdf, 0, 1, 0, 0, 1, 0, 0);
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 n;
    const u8* res = dict(&D, 3, &n, NULL);
    const u32 im1 = value(res, n, "/Im1 "), im2 = value(res, n, "/Im2 ");
    u32 dn, sn;
    const u8* d = dict(&D, im1, &dn, NULL);
    CHECK(d != NULL && find(d, dn, "/Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length 12") != NULL);
    CHECK(d != NULL && find(d, dn, "/SMask") == NULL && find(d, dn, "/Filter") == NULL);
    const u8* s = stream(&D, im1, &sn);
    const u8 rgb1[12] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120 };
    CHECK(s != NULL && sn == 12 && memcmp(s, rgb1, 12) == 0);
    d = dict(&D, im2, &dn, NULL);
    const u32 mask = value(d, dn, "/SMask ");
    CHECK(d != NULL && find(d, dn, "/Width 3 /Height 1 /ColorSpace /DeviceRGB") != NULL && mask == im2 + 1);
    s = stream(&D, im2, &sn);
    const u8 rgb2[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    CHECK(s != NULL && sn == 9 && memcmp(s, rgb2, 9) == 0);
    d = dict(&D, mask, &dn, NULL);
    CHECK(same(d, dn, "<< /Type /XObject /Subtype /Image /Width 3 /Height 1 /ColorSpace /DeviceGray /BitsPerComponent 8 /Length 3 >>"));
    s = stream(&D, mask, &sn);
    const u8 alpha[3] = { 255, 128, 0 };
    CHECK(s != NULL && sn == 3 && memcmp(s, alpha, 3) == 0);
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "q\n100 0 0 50 10 20 cm\n/Im2 Do\nQ\n"));
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

// --- text -----------------------------------------------------------------------------------------

static void test_text(void) {
    // Widths: Helvetica's AFM, H e l l o = 722 + 556 + 222 + 222 + 556.
    CHECK(fabs(fude_zoom_pdf_text_width(12, "Hello") - 2278.0 * 12.0 / 1000.0) < 1e-9);
    CHECK(fabs(fude_zoom_pdf_text_width(10, "\xC3\xA9") - 5.56) < 1e-9);                     // é 556
    CHECK(fabs(fude_zoom_pdf_text_width(10, "\xE2\x80\x94") - 10.0) < 1e-9);                 // — 1000
    CHECK(fabs(fude_zoom_pdf_text_width(10, "\xC2\xB0") - 4.0) < 1e-9);                      // ° 400
    CHECK(fude_zoom_pdf_text_width(10, "\xE2\x89\x88") == fude_zoom_pdf_text_width(10, "?"));  // ≈ drawn as ?
    CHECK(fude_zoom_pdf_text_width(10, "") == 0.0 && fude_zoom_pdf_text_width(10, NULL) == 0.0);
    CHECK(fabs(fude_zoom_pdf_text_width(1000, "W.i") - (944 + 278 + 222)) < 1e-9);
    // What it can write as written: Latin-1 and WinAnsi's extras, a '?' itself; not Japanese, nor ≈.
    CHECK(fude_zoom_pdf_text_writes("\xC3\x89" "chelle 1:1. V\xC3\xA9rifiez le carr\xC3\xA9 : 100 mm de c\xC3\xB4t\xC3\xA9 \xE2\x80\x99 \xE2\x82\xAC ?"));
    CHECK(!fude_zoom_pdf_text_writes("\xE7\xB8\xAE\xE5\xB0\xBA 1:1") && !fude_zoom_pdf_text_writes("a \xE2\x89\x88 b"));   // (縮尺, ≈)
    CHECK(fude_zoom_pdf_text_writes("") && !fude_zoom_pdf_text_writes(NULL));

    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 595, 842);
    // ( ) \ escaped; é ° ≈ € — ’ ½ as WinAnsi has them (≈ it has not).
    fude_zoom_pdf_text(pdf, 72, 700.5, 12, (rde_color){ 255, 0, 0, 255 },
                       "a(b)c\\d \xC3\xA9 \xC2\xB0 \xE2\x89\x88 \xE2\x82\xAC \xE2\x80\x94 \xE2\x80\x99 \xC2\xBD \xC3\x97");
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 0, 0, 0, 255 });       // the text left black as it was: nothing
    fude_zoom_pdf_text(pdf, 0, 0, 8, (rde_color){ 0, 0, 0, 100 }, "x");
    fude_zoom_pdf_text(pdf, 0, 0, 8, (rde_color){ 0, 0, 0, 255 }, "");    // nothing to write
    fude_zoom_pdf_text(pdf, 0, 0, 0, (rde_color){ 0, 0, 0, 255 }, "y");   // no size
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    u32 n;
    const u8* c = page_content(&D, 0, &n);
    CHECK(same(c, n, "q\n1 0 0 rg\nBT\n/F1 12 Tf\n72 700.5 Td\n(a\\(b\\)c\\\\d \xE9 \xB0 ? \x80 \x97 \x92 \xBD \xD7) Tj\nET\nQ\n"
                     "q\n/af100 gs\nBT\n/F1 8 Tf\n0 0 Td\n(x) Tj\nET\nQ\n"));
    const u32 font = object_with(&D, "/BaseFont");
    const u8* f = dict(&D, font, &n, NULL);
    CHECK(same(f, n, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"));
    const u8* res = dict(&D, 3, &n, NULL);
    CHECK(res != NULL && value(res, n, "/Font << /F1 ") == font);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);

    // A long string goes out in blocks, whole.
    pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    c8 longer[3001];
    for(u32 i = 0; i < 3000; i++) longer[i] = (c8)(i % 3 == 0 ? '(' : 'a');
    longer[3000] = 0;
    fude_zoom_pdf_text(pdf, 0, 0, 10, (rde_color){ 0, 0, 0, 255 }, longer);
    CHECK(finish(pdf, &out, &D));
    c = page_content(&D, 0, &n);
    CHECK(count_of(c, n, "\\(") == 1000 && count_of(c, n, "a") == 2000);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);

    // Every length round where a block fills (labels of 380 to 1100 characters), ending in a plain character or an
    // escaped one: the closing ") Tj" always fits after the last (AddressSanitizer stops the suite if it does not),
    // and each label is there whole.
    for(u32 kind = 0; kind < 2u; kind++) {
        pdf = fude_zoom_pdf_new();
        fude_zoom_pdf_page_begin(pdf, 100, 100);
        static c8 label_text[1101];
        u32 as = 0, opens = 0, labels = 0;
        for(u32 len = 380; len <= 1100u; len++) {
            for(u32 i = 0; i < len; i++) label_text[i] = (c8)(kind == 1u && i + 1u == len ? '(' : 'a');
            label_text[len] = 0;
            as += kind == 1u ? len - 1u : len; opens += kind; labels++;
            fude_zoom_pdf_text(pdf, 0, 0, 10, (rde_color){ 0, 0, 0, 255 }, label_text);
        }
        CHECK(finish(pdf, &out, &D));
        c = page_content(&D, 0, &n);
        CHECK(count_of(c, n, "a") == as && count_of(c, n, "\\(") == opens && count_of(c, n, ") Tj\nET\n") == labels);
        rde_arr_free(&out);
        fude_zoom_pdf_free(pdf);
    }
}

// --- the app's fonts, embedded --------------------------------------------------------------------

// The canvas apps' fonts (beside the suite's app, in the canvas app's assets), chained as page_export.c chains them:
// each face's Roboto, then the script font (Noto Sans JP cut down to the language picker's names, CFF), and Phosphor's
// icons behind the regular face. font_files is in the order the writer numbers them (/E1 on: as the chains first name
// them).
#define FONT_DIR FUDE_ROOT "/../sketching/assets/fonts/"
static const char* const font_files[6] = { FONT_DIR "Roboto-Regular.ttf", FONT_DIR "NotoSansJP-Names.otf", FONT_DIR "Phosphor-Regular.ttf",
                                           FONT_DIR "Roboto-Bold.ttf", FONT_DIR "Roboto-Italic.ttf", FONT_DIR "Roboto-BoldItalic.ttf" };
static const char* const font_names[6] = { "Roboto-Regular", "NotoSansJP-Names", "Phosphor-Regular", "Roboto-Bold", "Roboto-Italic", "Roboto-BoldItalic" };
static const int chains[4][4] = { { 0, 1, 2, -1 }, { 3, 1, -1, -1 }, { 4, 1, -1, -1 }, { 5, 1, -1, -1 } };   // (each face's, -1 ending it)

// The fonts set as the app sets them; a face's first file put in place of its own when not NULL (one
// damaged or missing).
static void use_fonts(const char* regular, const char* bold) {
    fude_zoom_pdf_font_files f;
    memset(&f, 0, sizeof f);
    for(u32 face = 0; face < 4; face++) {
        for(u32 i = 0; i < 4 && chains[face][i] >= 0; i++) f.chains[face][i] = font_files[chains[face][i]];
    }
    if(regular != NULL) f.chains[0][0] = regular;
    if(bold != NULL) f.chains[1][0] = bold;
    fude_zoom_pdf_fonts(&f);
}

static u8* read_file(const char* path, u32* n) {
    *n = 0;
    FILE* f = fopen(path, "rb");
    if(f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long k = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8* d = (u8*)malloc((size_t)k + 1u);
    *n = (u32)fread(d, 1, (size_t)k, f);
    fclose(f);
    return d;
}

static void write_file(const char* path, const u8* d, u32 n) {
    FILE* f = fopen(path, "wb");
    if(f != NULL) { fwrite(d, 1, n, f); fclose(f); }
}

static u32 be16(const u8* p) { return (u32)p[0] << 8 | p[1]; }
static u32 be32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static u32 hex(const u8* p, u32 digits) {
    u32 v = 0;
    for(u32 i = 0; i < digits; i++) v = v * 16u + (u32)(p[i] <= '9' ? p[i] - '0' : p[i] - 'A' + 10);
    return v;
}

// TrueType's check sum: the bytes as big-endian words, added up, the last word padded with zeros.
static u32 sum_words(const u8* p, u32 n) {
    u32 s = 0;
    for(u32 i = 0; i < n; i += 4) {
        u32 w = 0;
        for(u32 k = 0; k < 4; k++) w = w << 8 | (i + k < n ? p[i + k] : 0u);
        s += w;
    }
    return s;
}

// A font as this suite reads it, the plain way, to check the writer against: where its tables are, its Windows
// Unicode character map (format 4: what these fonts have), its advances and its glyphs' bytes.
typedef struct {
    u8* d;
    u32 n;
    u32 head, hhea, maxp, hmtx, loca, glyf, cmap4, cff, cff_len;
    u32 upem, glyphs, hmetrics, long_loca;
} ttf;

static u32 ttf_table(const u8* d, const char* tag, u32* len) {
    for(u32 i = 0; i < be16(d + 4); i++) {
        const u8* r = d + 12 + 16 * i;
        if(memcmp(r, tag, 4) == 0) {
            if(len != NULL) *len = be32(r + 12);
            return be32(r + 8);
        }
    }
    return 0;
}

static b8 ttf_open(ttf* t, const char* path) {
    memset(t, 0, sizeof *t);
    t->d = read_file(path, &t->n);
    if(t->d == NULL || t->n < 12) return false;
    t->head = ttf_table(t->d, "head", NULL); t->hhea = ttf_table(t->d, "hhea", NULL); t->maxp = ttf_table(t->d, "maxp", NULL);
    t->hmtx = ttf_table(t->d, "hmtx", NULL); t->loca = ttf_table(t->d, "loca", NULL); t->glyf = ttf_table(t->d, "glyf", NULL);
    t->cff  = ttf_table(t->d, "CFF ", &t->cff_len);
    const u32 cmap = ttf_table(t->d, "cmap", NULL);
    for(u32 i = 0; cmap != 0 && i < be16(t->d + cmap + 2); i++) {
        const u8* r = t->d + cmap + 4 + 8 * i;
        const u32 sub = cmap + be32(r + 4);
        if(be16(r) == 3 && be16(r + 2) == 1 && be16(t->d + sub) == 4) t->cmap4 = sub;
    }
    t->upem = be16(t->d + t->head + 18); t->long_loca = be16(t->d + t->head + 50);
    t->glyphs = be16(t->d + t->maxp + 4); t->hmetrics = be16(t->d + t->hhea + 34);
    return t->cmap4 != 0 && t->hmtx != 0 && t->upem != 0;
}

static u32 ttf_glyph(const ttf* t, u32 cp) {
    if(cp > 0xFFFF) return 0;
    const u8* s = t->d + t->cmap4;
    const u32 segs = be16(s + 6) / 2;
    const u8 *ends = s + 14, *starts = ends + 2 * segs + 2, *deltas = starts + 2 * segs, *ranges = deltas + 2 * segs;
    for(u32 i = 0; i < segs; i++) {
        if(be16(ends + 2 * i) < cp) continue;
        const u32 start = be16(starts + 2 * i), delta = be16(deltas + 2 * i), range = be16(ranges + 2 * i);
        if(cp < start) return 0;
        if(range == 0) return (cp + delta) & 0xFFFF;
        const u32 g = be16(ranges + 2 * i + range + 2 * (cp - start));
        return g != 0 ? (g + delta) & 0xFFFF : 0;
    }
    return 0;
}

static u32 ttf_advance(const ttf* t, u32 g) { return be16(t->d + t->hmtx + 4 * (g < t->hmetrics ? g : t->hmetrics - 1)); }

// Glyph _g's bytes in glyf (*at), and how many.
static u32 ttf_glyph_bytes(const ttf* t, u32 g, const u8** at) {
    u32 a, b;
    if(t->long_loca) { a = be32(t->d + t->loca + 4 * g); b = be32(t->d + t->loca + 4 * g + 4); }
    else             { a = 2 * be16(t->d + t->loca + 2 * g); b = 2 * be16(t->d + t->loca + 2 * g + 2); }
    *at = t->d + t->glyf + a;
    return b > a ? b - a : 0;
}

static ttf fonts_read[6];   // font_files, read by ttf_open

// NotoSansJP-Names's CIDs (its CFF's charset, format 0, read by hand): glyph 1 (日) 0x4EFC, 2 (本) 0x5116, 3 (語) 0x93E4.
static u32 jp_cid(u32 g) { return g == 1 ? 0x4EFC : g == 2 ? 0x5116 : g == 3 ? 0x93E4 : 0; }
static u32 jp_glyph(u32 cid) { return cid == 0x4EFC ? 1 : cid == 0x5116 ? 2 : cid == 0x93E4 ? 3 : 0; }

// The characters these tests write that the screen draws as nothing (a zero-width space, joiner, emoji selector).
static b8 hidden(u32 cp) { return cp == 0x200B || cp == 0x200D || cp == 0xFE0F; }

// What the writer must write for character _cp in face _face, by the chain: the font (its place in font_files) and its
// code (the glyph id, or a CID in the CFF one); the chain's first font and code 0 when none has it. False for a hidden
// character, which is not written.
static b8 expect(u32 face, u32 cp, u32* font, u32* code) {
    if(hidden(cp)) return false;
    for(u32 i = 0; i < 4 && chains[face][i] >= 0; i++) {
        const ttf* t = &fonts_read[chains[face][i]];
        const u32 g = ttf_glyph(t, cp);
        if(g != 0) {
            *font = (u32)chains[face][i];
            *code = t->cff != 0 ? jp_cid(g) : g;
            return true;
        }
    }
    *font = (u32)chains[face][0];
    *code = 0;
    return true;
}

// The width of _utf8 at _size in face _face, from the fonts' own advances.
static f64 width_of(u32 face, const char* utf8, f64 size) {
    f64 ems = 0;
    for(u32 cp = fude_utf8_next(&utf8); cp != 0; cp = fude_utf8_next(&utf8)) {
        u32 font, code;
        if(!expect(face, cp, &font, &code)) continue;
        const ttf* t = &fonts_read[font];
        ems += (f64)ttf_advance(t, t->cff != 0 ? jp_glyph(code) : code) / (f64)t->upem;
    }
    return ems * size;
}

// One glyph as a page writes it: its font (/E<n>'s n - 1: its place in font_files), its code and the character it
// stands for (expected, or read back through the font's ToUnicode map).
typedef struct { u32 font, code, unicode; } glyph_of;

// Appends what _face's text _utf8 must write to _out (at _at), as expect() says; returns the new count.
static u32 expected(u32 face, const char* utf8, glyph_of* out, u32 at, u32 most) {
    for(u32 cp = fude_utf8_next(&utf8); cp != 0; cp = fude_utf8_next(&utf8)) {
        glyph_of g;
        if(!expect(face, cp, &g.font, &g.code)) continue;
        g.unicode = g.code == 0 ? 0xFFFD : cp;
        if(at < most) out[at] = g;
        at++;
    }
    return at;
}

// The glyphs page _page writes in the embedded fonts, in order: each hex string's codes in the font its last Tf set.
static u32 written(const doc* D, u32 page, glyph_of* out, u32 most) {
    u32 n, k = 0, font = 0;
    const u8* c = page_content(D, page, &n);
    if(c == NULL) return 0;
    for(u32 i = 0; i < n; i++) {
        if(c[i] == '/' && i + 1 < n && c[i + 1] == 'E') {
            const u8* p = c + i + 2;
            font = number(&p) - 1u;
        } else if(c[i] == '<') {
            u32 j = i + 1;
            for(; j + 4 <= n && c[j] != '>'; j += 4, k++) {
                if(k < most) { out[k].font = font; out[k].code = hex(c + j, 4); out[k].unicode = 0; }
            }
            i = j;
        }
    }
    return k;
}

// Font /E<n>'s objects, from the resources: its Type 0 font, its CIDFont, its descriptor, its program, its ToUnicode.
typedef struct { u32 type0, cid, descriptor, program, unicode; } font_objects;

static b8 objects_of(const doc* D, u32 n, font_objects* o) {
    u32 len;
    char key[16];
    snprintf(key, sizeof key, "/E%u ", n);
    memset(o, 0, sizeof *o);
    const u8* res = dict(D, 3, &len, NULL);
    if(res == NULL || find(res, len, key) == NULL) return false;
    o->type0 = value(res, len, key);
    const u8* t = dict(D, o->type0, &len, NULL);
    if(t == NULL) return false;
    o->cid     = value(t, len, "/DescendantFonts [");
    o->unicode = value(t, len, "/ToUnicode ");
    const u8* c = dict(D, o->cid, &len, NULL);
    if(c == NULL) return false;
    o->descriptor = value(c, len, "/FontDescriptor ");
    const u8* d = dict(D, o->descriptor, &len, NULL);
    if(d == NULL) return false;
    o->program = find(d, len, "/FontFile2 ") != NULL ? value(d, len, "/FontFile2 ") : value(d, len, "/FontFile3 ");
    return o->program != 0 && o->unicode != 0;
}

// A ToUnicode CMap's bfchar entries into map (65536 code points by code; 0 where it says nothing). Returns how many,
// or 0 when a line is not "<code> <unit>" or "<code> <high low>".
static u32 unicode_map(const doc* D, u32 obj, u32* map) {
    u32 n, count = 0;
    memset(map, 0, sizeof(u32) * 65536u);
    const u8* s = stream(D, obj, &n);
    const u8* p = s != NULL ? find(s, n, "endcodespacerange") : NULL;
    while(p != NULL && (p = find(p, n - (u32)(p - s), "<")) != NULL) {
        if(p + 8 > s + n || p[5] != '>' || p[6] != ' ' || p[7] != '<') return 0;
        const u32 code = hex(p + 1, 4);
        const u8* q = p + 8;
        u32 digits = 0;
        while(q + digits < s + n && q[digits] != '>') digits++;
        if(digits != 4 && digits != 8) return 0;
        u32 u = hex(q, 4);
        if(digits == 8) u = 0x10000u + ((u - 0xD800u) << 10) + (hex(q + 4, 4) - 0xDC00u);
        map[code] = u;
        count++;
        p = q + digits;
    }
    return count;
}

// The CIDFont's widths (W) for code: what its "first [w w w]" runs say, or -1.
static f64 width_in(const doc* D, u32 cid_obj, u32 code) {
    u32 len;
    const u8* c = dict(D, cid_obj, &len, NULL);
    const u8* w = c != NULL ? find(c, len, "/W [") : NULL;
    if(w == NULL) return -1;
    const char* p = (const char*)w + 4;
    while(*p != ']') {
        char* end;
        const u32 first = (u32)strtoul(p, &end, 10);
        p = end;
        while(*p == ' ') p++;
        if(*p != '[') return -1;
        p++;
        for(u32 k = first; *p != ']'; k++) {
            const f64 v = strtod(p, &end);
            if(end == p) return -1;
            if(k == code) return v;
            p = end;
            while(*p == ' ') p++;
        }
        p++;
        while(*p == ' ') p++;
    }
    return -1;
}

// A TrueType program as the writer cuts it down: its words add up to TrueType's sum, each table's check sum is right,
// a reader's tables are there and the shaping ones are not, it has the original's glyphs, each glyph kept (glyph 0,
// the used ones, and the parts of a kept composite) is the original's bytes, and every other glyph is empty.
static void check_program(const u8* p, u32 n, const ttf* t, const u8* used) {
    CHECK(n > 12 && be32(p) == 0x00010000u && sum_words(p, n) == 0xB1B0AFBAu);
    const u32 tables = be16(p + 4);
    u32 head = 0, loca = 0, glyf = 0, maxp = 0;
    for(u32 i = 0; i < tables; i++) {
        const u8* r = p + 12 + 16 * i;
        const u32 at = be32(r + 8), len = be32(r + 12);
        CHECK(at % 4 == 0 && at + len <= n && (i == 0 || memcmp(r - 16, r, 4) < 0));   // (on 4 bytes, by tag)
        u32 sum = sum_words(p + at, len);
        if(memcmp(r, "head", 4) == 0) { head = at; sum -= be32(p + at + 8); }   // (its adjustment counted as 0)
        CHECK(sum == be32(r + 4));
        if(memcmp(r, "loca", 4) == 0) loca = at;
        if(memcmp(r, "glyf", 4) == 0) glyf = at;
        if(memcmp(r, "maxp", 4) == 0) maxp = at;
        CHECK(memcmp(r, "GSUB", 4) != 0 && memcmp(r, "GPOS", 4) != 0);
    }
    CHECK(head != 0 && loca != 0 && glyf != 0 && maxp != 0 && ttf_table(p, "hmtx", NULL) != 0 && ttf_table(p, "hhea", NULL) != 0);
    if(head == 0 || loca == 0 || glyf == 0 || maxp == 0) return;
    CHECK(be16(p + maxp + 4) == t->glyphs);
    // (what must be kept: the used glyphs, glyph 0, and the parts of each, read by mathfont.h's reader)
    fude_zoom_mfont m;
    CHECK(fude_zoom_mfont_load(&m, t->d, t->n));
    u8* keep = (u8*)calloc(t->glyphs, 1);
    keep[0] = 1;
    for(u32 g = 0; g < t->glyphs; g++) keep[g] |= used[g];
    for(u32 pass = 0; pass < 4; pass++) {
        for(u32 g = 0; g < t->glyphs; g++) {
            u32 parts[64];
            const u32 k = keep[g] ? fude_zoom_mfont_parts(&m, g, parts, 64) : 0;
            for(u32 i = 0; i < k; i++) if(parts[i] < t->glyphs) keep[parts[i]] = 1;
        }
    }
    const b8 is_short = be16(p + head + 50) == 0;
    u32 kept = 0, emptied = 0;
    for(u32 g = 0; g < t->glyphs; g++) {
        const u32 a = is_short ? 2 * be16(p + loca + 2 * g) : be32(p + loca + 4 * g);
        const u32 b = is_short ? 2 * be16(p + loca + 2 * g + 2) : be32(p + loca + 4 * g + 4);
        const u8* was;
        const u32 len = ttf_glyph_bytes(t, g, &was);
        if(keep[g]) {
            CHECK(b - a >= len && b - a < len + 4 && memcmp(p + glyf + a, was, len) == 0);
            kept++;
        } else {
            CHECK(a == b);
            emptied += len > 0;
        }
    }
    CHECK(kept > 1 && emptied > 0);
    free(keep);
}

static void test_fonts(void) {
    for(u32 i = 0; i < 6; i++) CHECK(ttf_open(&fonts_read[i], font_files[i]));
    const ttf* roboto = &fonts_read[0];
    const ttf* names  = &fonts_read[1];

    // mathfont.h's readers, which the writer embeds through: a CFF font loads (its character map and advances, no
    // glyf), its CIDs from its charset; a TrueType font's tables, a composite glyph's parts.
    fude_zoom_mfont m;
    rde_arr cids = rde_arr_new(sizeof(u16), rde_memory_allocator_get_default_std());
    CHECK(fude_zoom_mfont_load(&m, names->d, names->n) && m.cff != 0 && m.glyf == 0 && fude_zoom_mfont_glyph(&m, 0x672C) == 2);
    CHECK(fude_zoom_mfont_cids(&m, &cids) && rde_arr_length(&cids) == 4);
    const u16* cid = (const u16*)cids.memory;
    CHECK(cid[0] == 0 && cid[1] == jp_cid(1) && cid[2] == jp_cid(2) && cid[3] == jp_cid(3));
    u32 at, len, parts[8];
    i32 box[4];
    CHECK(!fude_zoom_mfont_glyph_data(&m, 1, &at, &len) && !fude_zoom_mfont_box(&m, 1, box) && fude_zoom_mfont_parts(&m, 1, parts, 8) == 0);
    CHECK(fude_zoom_mfont_load(&m, roboto->d, roboto->n) && m.cff == 0);
    CHECK(!fude_zoom_mfont_cids(&m, &cids) && rde_arr_length(&cids) == 4);   // (no CFF: left alone)
    const u32 e = fude_zoom_mfont_glyph(&m, 'e'), e_acute = fude_zoom_mfont_glyph(&m, 0xE9);
    CHECK(fude_zoom_mfont_parts(&m, e_acute, parts, 8) == 2 && (parts[0] == e || parts[1] == e) && fude_zoom_mfont_parts(&m, e, parts, 8) == 0);
    const u8* e_bytes;
    CHECK(fude_zoom_mfont_glyph_data(&m, e, &at, &len) && len == ttf_glyph_bytes(roboto, e, &e_bytes) && roboto->d + at == e_bytes);
    u32 cvt = 0;
    CHECK(fude_zoom_mfont_table(&m, 0x63767420u, &cvt) == ttf_table(roboto->d, "cvt ", NULL) && cvt > 0 && fude_zoom_mfont_table(&m, 0x58585858u, NULL) == 0);
    CHECK(fude_zoom_mfont_u16(&m, roboto->head + 18) == roboto->upem && fude_zoom_mfont_u32(&m, roboto->n) == 0);   // (past the end: 0)
    rde_arr_free(&cids);

    static const char* const lines[8] = {
        "Greek: \xCE\x9A\xCE\xB1\xCE\xBB\xCE\xB7\xCE\xBC\xCE\xAD\xCF\x81\xCE\xB1 \xCE\xBA\xCF\x8C\xCF\x83\xCE\xBC\xCE\xB5",              // Καλημέρα κόσμε
        "Cyrillic: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82, \xD0\xBC\xD0\xB8\xD1\x80! \xD0\x81\xD0\xB6",                      // Привет, мир! Ёж
        "Polish: Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84 \xC5\x81\xC3\xB3" "d\xC5\xBA",       // Zażółć gęślą jaźń Łódź
        "Turkish: \xC4\xB0stanbul \xC4\xB1l\xC4\xB1k \xC4\x9F \xC5\x9F \xC3\xA7 \xC3\xB6 \xC3\xBC \xC4\x9E \xC5\x9E",                  // İstanbul ılık ğ ş ç ö ü Ğ Ş
        "Czech: P\xC5\x99\xC3\xADli\xC5\xA1 \xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD k\xC5\xAF\xC5\x88 \xC5\x98",                          // Příliš žluťoučký kůň Ř
        "\xCE\xA9 \xD0\x96 \xC5\x82 \xE2\x80\x94 \xE2\x80\x9Cq\xE2\x80\x9D \xE2\x82\xAC \xE2\x89\x88",                                  // Ω Ж ł — “q” € ≈
        "\xC4\xB1 \xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xE4\xB8\xAD",                                                         // ı é 日本語 中 (in bold italic)
        "a\xE2\x80\x8B" "b \xEE\x80\x90 \xF0\x9F\x98\x80\xEF\xB8\x8F \xE6\x97\xA5 \xE0\xA4\x95",                                        // a(ZWSP)b, an icon, 😀(VS16), 日, क
    };
    static const u8 faces[8] = { 0, 0, 0, 0, 1, 2, 3, 0 };
    use_fonts(NULL, NULL);
    // What prints as written: the four scripts and Japanese's three names; not 中, 😀 or क; hidden ones count.
    for(u32 i = 0; i < 4; i++) CHECK(fude_zoom_pdf_text_writes(lines[i]));
    CHECK(fude_zoom_pdf_text_writes("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xEE\x80\x90 a\xE2\x80\x8B" "b"));
    CHECK(!fude_zoom_pdf_text_writes("a\xE4\xB8\xAD") && !fude_zoom_pdf_text_writes("\xF0\x9F\x98\x80") && !fude_zoom_pdf_text_writes(NULL));
    CHECK(!fude_zoom_pdf_text_writes(lines[6]) && fude_zoom_pdf_text_writes(""));
    // Widths: each character's advance in the font it is written in, its face's own, without kerning.
    for(u32 i = 0; i < 8; i++) {
        CHECK(fabs(fude_zoom_pdf_text_width_face(13, lines[i], faces[i]) - width_of(faces[i], lines[i], 13)) < 1e-9);
    }
    CHECK(fabs(fude_zoom_pdf_text_width(12, "Hello") - width_of(0, "Hello", 12)) < 1e-9 && width_of(1, "Hello", 12) > width_of(0, "Hello", 12) + 0.1);
    CHECK(fude_zoom_pdf_text_width(10, "") == 0.0 && fude_zoom_pdf_text_width(10, NULL) == 0.0 && fude_zoom_pdf_text_width(10, "\xE2\x80\x8B") == 0.0);

    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 595, 842);
    for(u32 i = 0; i < 8; i++) fude_zoom_pdf_text_face(pdf, 72, 760 - 30 * i, 14, (rde_color){ 0, 0, 0, 255 }, lines[i], faces[i]);
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    // The first line exactly: in /E1 (Roboto), each character its glyph id.
    {
        static glyph_of g[64];
        const u32 k = expected(0, lines[0], g, 0, 64);
        char want[512];
        int w = snprintf(want, sizeof want, "q\nBT\n/E1 14 Tf\n72 760 Td\n<");
        for(u32 i = 0; i < k; i++) w += snprintf(want + w, sizeof want - (size_t)w, "%04X", g[i].code);
        snprintf(want + w, sizeof want - (size_t)w, "> Tj\nET\nQ\n");
        u32 n;
        const u8* c = page_content(&D, 0, &n);
        CHECK(c != NULL && n > strlen(want) && memcmp(c, want, strlen(want)) == 0);
        CHECK(g[0].code == ttf_glyph(roboto, 'G') && g[0].code != 0);
    }
    // Every glyph as the chains say, and each read back through its font's ToUnicode map as its character.
    static glyph_of want[1024], got[1024];
    u32 nw = 0;
    for(u32 i = 0; i < 8; i++) nw = expected(faces[i], lines[i], want, nw, 1024);
    const u32 ng = written(&D, 0, got, 1024);
    CHECK(ng == nw);
    u32* maps[6] = { NULL };
    u8*  used[6] = { NULL };
    for(u32 i = 0; i < ng && i < nw; i++) {
        CHECK(got[i].font == want[i].font && got[i].code == want[i].code);
        const u32 f = got[i].font < 6 ? got[i].font : 0;
        if(maps[f] == NULL) {
            font_objects o;
            maps[f] = (u32*)malloc(sizeof(u32) * 65536u);
            used[f] = (u8*)calloc(65536u, 1);
            CHECK(objects_of(&D, f + 1, &o) && unicode_map(&D, o.unicode, maps[f]) > 0);
        }
        CHECK(maps[f][got[i].code & 0xFFFF] == want[i].unicode);
        used[f][fonts_read[f].cff != 0 ? jp_glyph(got[i].code) : (got[i].code & 0xFFFF)] = 1;
    }
    CHECK(maps[0] && maps[1] && maps[2] && maps[3] && maps[4] && maps[5]);   // (every font written in)
    // Each font embedded: a Type 0 font over its CIDFont, named as a subset of its file, its widths its advances, its
    // program the font cut down (TrueType) or its CFF whole, and its map one entry a glyph used.
    for(u32 f = 0; f < 6; f++) {
        font_objects o;
        if(maps[f] == NULL || !objects_of(&D, f + 1, &o)) { CHECK(false); continue; }
        const ttf* t = &fonts_read[f];
        const u8* t0 = dict(&D, o.type0, &len, NULL);
        CHECK(t0 != NULL && find(t0, len, "/Subtype /Type0") != NULL && find(t0, len, "/Encoding /Identity-H") != NULL);
        const u8* name = t0 != NULL ? find(t0, len, "/BaseFont /") : NULL;
        CHECK(name != NULL && name[17] == '+' && memcmp(name + 18, font_names[f], strlen(font_names[f])) == 0 && name[18 + strlen(font_names[f])] == ' ');
        for(u32 i = 11; name != NULL && i < 17; i++) CHECK(name[i] >= 'A' && name[i] <= 'Z');
        const u8* cf = dict(&D, o.cid, &len, NULL);
        CHECK(cf != NULL && find(cf, len, "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >>") != NULL);
        CHECK(cf != NULL && find(cf, len, t->cff ? "/Subtype /CIDFontType0 " : "/Subtype /CIDFontType2 ") != NULL);
        CHECK(cf != NULL && (find(cf, len, "/CIDToGIDMap /Identity") != NULL) == (t->cff == 0));
        const u8* fd = dict(&D, o.descriptor, &len, NULL);
        CHECK(fd != NULL && find(fd, len, "/Type /FontDescriptor") != NULL && find(fd, len, f == 4 || f == 5 ? "/Flags 68 " : "/Flags 4 ") != NULL);
        u32 entries = 0;
        for(u32 g = 0; g < 65536u; g++) {
            if(used[f][g] == 0) continue;
            entries++;
            const u32 code = t->cff != 0 ? jp_cid(g) : g;
            CHECK(fabs(width_in(&D, o.cid, code) - (f64)ttf_advance(t, g) * 1000.0 / t->upem) < 0.0051);
        }
        CHECK(unicode_map(&D, o.unicode, maps[f]) == entries);
        u32 pn;
        const u8* prog = stream(&D, o.program, &pn);
        const u8* pd   = dict(&D, o.program, &len, NULL);
        CHECK(prog != NULL && pd != NULL);
        if(prog == NULL || pd == NULL) continue;
        if(t->cff != 0) {
            CHECK(find(pd, len, "/Subtype /CIDFontType0C") != NULL && pn == t->cff_len && memcmp(prog, t->d + t->cff, pn) == 0);
        } else {
            CHECK(value(pd, len, "/Length1 ") == pn && pn < t->n / 3);
            check_program(prog, pn, t, used[f]);
        }
        printf("  %s: %u glyphs used, embedded in %u bytes (the file: %u)\n", font_names[f], entries, pn, t->n);
    }
    // The same document finished again: the same file.
    fude_bytes again = fude_bytes_new(16);
    CHECK(fude_zoom_pdf_finish(pdf, &again) && fude_bytes_size(&again) == fude_bytes_size(&out) && memcmp(again.memory, out.memory, fude_bytes_size(&out)) == 0);
    rde_arr_free(&again);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    for(u32 f = 0; f < 6; f++) { free(maps[f]); free(used[f]); }
    const rde_color black = { 0, 0, 0, 255 };
    u32 n;
    const u8* c;

    // A face whose own font cannot be read is written in the regular face's fonts.
    use_fonts(NULL, "nowhere/Missing-Bold.ttf");
    pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_text_face(pdf, 0, 0, 10, black, "Bold", 1);
    CHECK(finish(pdf, &out, &D));
    c = page_content(&D, 0, &n);
    CHECK(c != NULL && find(c, n, "/E1 10 Tf") != NULL && find(c, n, "/E4") == NULL && find(c, n, "/F") == NULL);
    CHECK(fabs(fude_zoom_pdf_text_width_face(10, "Bold", 1) - width_of(0, "Bold", 10)) < 1e-9);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    // The regular face's first unreadable: its text is Helvetica's (and its widths and what it writes), the bold's
    // still Roboto Bold (the fourth file named: /E4).
    use_fonts("nowhere/Missing.ttf", NULL);
    CHECK(fabs(fude_zoom_pdf_text_width(10, "Hi") - (722 + 222) / 100.0) < 1e-9 && !fude_zoom_pdf_text_writes("\xCE\xA9"));
    pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_text_face(pdf, 0, 0, 10, black, "Hi", 0);
    fude_zoom_pdf_text_face(pdf, 0, 0, 10, black, "Hi", 1);
    CHECK(finish(pdf, &out, &D));
    c = page_content(&D, 0, &n);
    {
        char hi[128];
        snprintf(hi, sizeof hi, "q\nBT\n/F1 10 Tf\n0 0 Td\n(Hi) Tj\nET\nQ\nq\nBT\n/E4 10 Tf\n0 0 Td\n<%04X%04X> Tj\nET\nQ\n", ttf_glyph(&fonts_read[3], 'H'),
                 ttf_glyph(&fonts_read[3], 'i'));
        CHECK(same(c, n, hi));
    }
    CHECK(object_with(&D, "/BaseFont /Helvetica ") != 0 && object_with(&D, "/Subtype /CIDFontType2") != 0);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    // A document keeps the fonts set when it was made, whatever is set after; with none set, Helvetica as before.
    use_fonts(NULL, NULL);
    pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_fonts(NULL);
    fude_zoom_pdf_page_begin(pdf, 100, 100);
    fude_zoom_pdf_text(pdf, 0, 0, 10, black, "\xCE\xA9");
    fude_zoom_pdf* plain = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(plain, 100, 100);
    fude_zoom_pdf_text(plain, 0, 0, 10, black, "\xCE\xA9");
    CHECK(finish(pdf, &out, &D));
    c = page_content(&D, 0, &n);
    CHECK(c != NULL && find(c, n, "/E1 10 Tf") != NULL);
    rde_arr_free(&out);
    CHECK(finish(plain, &out, &D));
    c = page_content(&D, 0, &n);
    CHECK(same(c, n, "q\nBT\n/F1 10 Tf\n0 0 Td\n(?) Tj\nET\nQ\n"));
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    fude_zoom_pdf_free(plain);

    // A damaged font, cut short anywhere or with bytes overwritten (Roboto, and the CFF one): text written in it never
    // reads past its bytes (AddressSanitizer stops the suite if it does), and the file is still sound.
    srand(7);
    for(u32 round = 0; round < 90; round++) {
        const b8   cff = round % 3 == 2;
        const ttf* src = cff ? names : roboto;
        u32 size = src->n;
        u8* d = (u8*)malloc(size);
        memcpy(d, src->d, size);
        if(round < 45) size = (u32)rand() % size;
        else for(u32 k = 0; k < 200; k++) d[(u32)rand() % size] = (u8)rand();
        write_file(cff ? "damaged.otf" : "damaged.ttf", d, size);
        free(d);
        if(cff) {
            fude_zoom_pdf_font_files f;
            memset(&f, 0, sizeof f);
            f.chains[0][0] = font_files[0];
            f.chains[0][1] = "damaged.otf";
            fude_zoom_pdf_fonts(&f);
        } else {
            use_fonts("damaged.ttf", round % 3 == 1 ? "damaged.ttf" : NULL);
        }
        pdf = fude_zoom_pdf_new();
        fude_zoom_pdf_page_begin(pdf, 200, 200);
        for(u32 i = 0; i < 8; i++) fude_zoom_pdf_text_face(pdf, 10, 10 + 20 * i, 10, black, lines[i], faces[i]);
        CHECK(fude_zoom_pdf_text_width(10, lines[3]) >= 0.0);
        CHECK(finish(pdf, &out, &D));
        rde_arr_free(&out);
        fude_zoom_pdf_free(pdf);
    }
    remove("damaged.ttf");
    remove("damaged.otf");

    // Random lines of every kind of character above (one of each that shares a glyph with another: Ω and the ohm sign
    // copy back as the first written): each glyph as the chains say, each read back as written, each width the sum.
    use_fonts(NULL, NULL);
    static const u32 pool_all[] = { 'A', 'z', '0', ' ', '(', ')', '\\', '<', '>', 0xE9, 0xDF, 0xC5, 0x141, 0x142, 0x130, 0x131, 0x15F, 0x11F,
                                    0x159, 0x16F, 0x391, 0x3B1, 0x3C9, 0x3A9, 0x2126, 0x410, 0x44F, 0x451, 0x2014, 0x20AC, 0x2248, 0x65E5,
                                    0x672C, 0x8A9E, 0x4E2D, 0xE010, 0xE016, 0x1F600, 0x915, 0x200B, 0xFE0F, 0xFFFD };
    u32 pool[64], np = 0;
    for(u32 i = 0; i < sizeof pool_all / sizeof pool_all[0]; i++) {
        u32 f, code, f2, code2;
        b8  twin = false;
        for(u32 j = 0; j < np && expect(0, pool_all[i], &f, &code) && code != 0; j++) {
            twin = twin || (expect(0, pool[j], &f2, &code2) && f2 == f && code2 == code);
        }
        if(!twin) pool[np++] = pool_all[i];
    }
    CHECK(np == sizeof pool_all / sizeof pool_all[0] - 1);   // (only the ohm sign: Ω's twin)
    srand(11);
    pdf = fude_zoom_pdf_new();
    fude_zoom_pdf_page_begin(pdf, 595, 842);
    enum { LINES = 300, LONGEST = 60, MOST = LINES * LONGEST };   // (characters at most: every line its longest)
    static glyph_of rw[MOST], rg[MOST];
    u32 nrw = 0;
    for(u32 line = 0; line < LINES; line++) {
        c8  s[LONGEST * 4 + 8];
        u32 bytes = 0;
        const u32 count = 1u + (u32)rand() % LONGEST;
        for(u32 k = 0; k < count; k++) bytes += fude_utf8_put(pool[(u32)rand() % np], s + bytes);
        s[bytes] = 0;
        fude_zoom_pdf_text(pdf, 10, 830 - (f64)(line % 80) * 10, 8, black, s);
        nrw = expected(0, s, rw, nrw, MOST);
        CHECK(fabs(fude_zoom_pdf_text_width(8, s) - width_of(0, s, 8)) < 1e-9);
    }
    CHECK(finish(pdf, &out, &D));
    const u32 nrg = written(&D, 0, rg, MOST);
    CHECK(nrg == nrw && nrw <= MOST);
    u32* rmaps[6] = { NULL };
    for(u32 i = 0; i < nrg && i < nrw; i++) {
        CHECK(rg[i].font == rw[i].font && rg[i].code == rw[i].code);
        const u32 f = rg[i].font < 6 ? rg[i].font : 0;
        if(rmaps[f] == NULL) {
            font_objects o;
            rmaps[f] = (u32*)malloc(sizeof(u32) * 65536u);
            CHECK(objects_of(&D, f + 1, &o) && unicode_map(&D, o.unicode, rmaps[f]) > 0);
        }
        CHECK(rmaps[f][rg[i].code & 0xFFFF] == rw[i].unicode);
    }
    for(u32 f = 0; f < 6; f++) free(rmaps[f]);
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    fude_zoom_pdf_fonts(NULL);
    for(u32 i = 0; i < 6; i++) free(fonts_read[i].d);
}

// --- a page to look at -----------------------------------------------------------------------------

// A circle as four cubic arcs.
static void circle(fude_zoom_pdf* pdf, f64 x, f64 y, f64 r) {
    const f64 k = 0.5522847498 * r;
    fude_zoom_pdf_move(pdf, x + r, y);
    fude_zoom_pdf_curve(pdf, x + r, y + k, x + k, y + r, x, y + r);
    fude_zoom_pdf_curve(pdf, x - k, y + r, x - r, y + k, x - r, y);
    fude_zoom_pdf_curve(pdf, x - r, y - k, x - k, y - r, x, y - r);
    fude_zoom_pdf_curve(pdf, x + k, y - r, x + r, y - k, x + r, y);
    fude_zoom_pdf_close(pdf);
}

static void cross(fude_zoom_pdf* pdf, f64 x, f64 y) {
    fude_zoom_pdf_move(pdf, x - 10, y); fude_zoom_pdf_line(pdf, x + 10, y);
    fude_zoom_pdf_move(pdf, x, y - 10); fude_zoom_pdf_line(pdf, x, y + 10);
    fude_zoom_pdf_stroke(pdf);
    circle(pdf, x, y, 5);
    fude_zoom_pdf_stroke(pdf);
}

static void label(fude_zoom_pdf* pdf, f64 cx, f64 y, f64 size, const c8* s) {
    fude_zoom_pdf_text(pdf, cx - fude_zoom_pdf_text_width(size, s) / 2.0, y, size, (rde_color){ 20, 20, 20, 255 }, s);
}

static void write_sample(void) {
    use_fonts(NULL, NULL);   // (its text in the app's fonts, as the app writes it)
    fude_zoom_pdf* pdf = fude_zoom_pdf_new();
    const f64 W = 210.0 * FUDE_ZOOM_PDF_MM, H = 297.0 * FUDE_ZOOM_PDF_MM;
    const u32 jpeg = fude_zoom_pdf_jpeg(pdf, jpeg_16x12, sizeof jpeg_16x12);
    // A disc fading out to its edge: opaque middle, clear rim.
    u8* px = (u8*)malloc(64 * 64 * 4);
    for(u32 y = 0; y < 64; y++) {
        for(u32 x = 0; x < 64; x++) {
            const f64 d = sqrt((x - 31.5) * (x - 31.5) + (y - 31.5) * (y - 31.5)) / 32.0;
            u8* p = px + (y * 64 + x) * 4;
            p[0] = (u8)(x * 4); p[1] = 40; p[2] = (u8)(255 - y * 4);
            p[3] = (u8)(d >= 1.0 ? 0 : (d < 0.5 ? 255 : (1.0 - d) * 2.0 * 255.0));
        }
    }
    const u32 disc = fude_zoom_pdf_rgba(pdf, px, 64, 64);
    free(px);

    fude_zoom_pdf_page_begin(pdf, W, H);
    label(pdf, W / 2, H - 60, 20, "PDF writer \xE2\x80\x94 sample page");
    label(pdf, W / 2, H - 82, 11, "Accents \xC3\xA9 \xC3\xB1 \xC3\xBC, 90\xC2\xB0, 3 \xC3\x97 4, \xC2\xBD \xC2\xBC \xC2\xBE, 5 \xE2\x82\xAC, \xE2\x80\x9Cquoted\xE2\x80\x9D \xE2\x80\xA2 (parens) \\ \xE2\x89\x88");
    // Alpha: an opaque red square under a half-clear blue one.
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 220, 30, 30, 255 });
    fude_zoom_pdf_rect(pdf, 60, 560, 120, 120);
    fude_zoom_pdf_fill(pdf, false);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 30, 60, 230, 128 });
    fude_zoom_pdf_rect(pdf, 120, 600, 120, 120);
    fude_zoom_pdf_fill(pdf, false);
    // A thick round-capped green ring, half clear, and a dashed line.
    fude_zoom_pdf_stroke_color(pdf, (rde_color){ 20, 160, 60, 160 });
    fude_zoom_pdf_line_width(pdf, 8);
    fude_zoom_pdf_line_style(pdf, 1, 1);
    circle(pdf, 360, 640, 60);
    fude_zoom_pdf_stroke(pdf);
    fude_zoom_pdf_stroke_color(pdf, (rde_color){ 0, 0, 0, 255 });
    fude_zoom_pdf_line_width(pdf, 2);
    fude_zoom_pdf_line_style(pdf, 0, 0);
    const f64 dash[2] = { 10, 5 };
    fude_zoom_pdf_dash(pdf, dash, 2, 0);
    fude_zoom_pdf_move(pdf, 60, 540);
    fude_zoom_pdf_line(pdf, W - 60, 540);
    fude_zoom_pdf_stroke(pdf);
    fude_zoom_pdf_dash(pdf, NULL, 0, 0);
    // An even-odd star: its middle stays empty.
    f64 star[10];
    for(u32 i = 0; i < 5; i++) {
        const f64 a = 1.5707963 + i * 2.0 * 2.0 * 3.14159265 / 5.0;
        star[2 * i] = 480 + cos(a) * 60; star[2 * i + 1] = 640 + sin(a) * 60;
    }
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 240, 170, 0, 255 });
    fude_zoom_pdf_polyline(pdf, star, 5, true);
    fude_zoom_pdf_fill_stroke(pdf, true);
    // The JPEG, large; the disc over a checkerboard to show its alpha.
    fude_zoom_pdf_image(pdf, jpeg, 160, 0, 0, 120, 60, 380);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 0, 0, 0, 255 });
    for(u32 y = 0; y < 8; y++) for(u32 x = 0; x < 8; x++) if((x + y) % 2 == 0) fude_zoom_pdf_rect(pdf, 260 + x * 15, 380 + y * 15, 15, 15);
    fude_zoom_pdf_fill(pdf, false);
    fude_zoom_pdf_image(pdf, disc, 120, 0, 0, 120, 260, 380);
    // A clip: stripes seen through a circle.
    fude_zoom_pdf_save(pdf);
    circle(pdf, 460, 440, 55);
    fude_zoom_pdf_clip(pdf, false);
    fude_zoom_pdf_fill_color(pdf, (rde_color){ 120, 0, 160, 255 });
    for(u32 i = 0; i < 12; i++) fude_zoom_pdf_rect(pdf, 400, 380 + i * 10, 120, 5);
    fude_zoom_pdf_fill(pdf, false);
    fude_zoom_pdf_restore(pdf);
    // The 100 mm test square, a transform turning its label.
    fude_zoom_pdf_line_width(pdf, 0.5);
    fude_zoom_pdf_rect(pdf, 60, 60, 100 * FUDE_ZOOM_PDF_MM, 100 * FUDE_ZOOM_PDF_MM);
    fude_zoom_pdf_stroke(pdf);
    label(pdf, 60 + 50 * FUDE_ZOOM_PDF_MM, 60 + 50 * FUDE_ZOOM_PDF_MM, 14, "100 mm");
    fude_zoom_pdf_save(pdf);
    fude_zoom_pdf_transform(pdf, 0, 1, -1, 0, 60 + 100 * FUDE_ZOOM_PDF_MM + 16, 60 + 50 * FUDE_ZOOM_PDF_MM);
    label(pdf, 0, 0, 10, "turned 90\xC2\xB0");
    fude_zoom_pdf_restore(pdf);
    // Registration crosses.
    cross(pdf, 30, 30); cross(pdf, W - 30, 30); cross(pdf, 30, H - 30); cross(pdf, W - 30, H - 30);
    fude_zoom_pdf_page_begin(pdf, 612, 792);
    fude_zoom_pdf_rect(pdf, 36, 36, 612 - 72, 792 - 72);
    fude_zoom_pdf_stroke(pdf);
    label(pdf, 306, 396, 24, "Page 2: Letter");
    // A page of scripts in every face: what the app's fonts draw (Japanese's three names, an icon), and what none has
    // (中, 😀, क: the missing-glyph box, as on the screen).
    fude_zoom_pdf_page_begin(pdf, W, H);
    static const char* const scripts[] = {
        "Greek: \xCE\x9A\xCE\xB1\xCE\xBB\xCE\xB7\xCE\xBC\xCE\xAD\xCF\x81\xCE\xB1 \xCE\xBA\xCF\x8C\xCF\x83\xCE\xBC\xCE\xB5",
        "Cyrillic: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82, \xD0\xBC\xD0\xB8\xD1\x80!",
        "Polish: Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84",
        "Turkish: \xC4\xB0stanbul \xC4\xB1l\xC4\xB1k \xC4\x9F \xC5\x9F",
        "Czech: P\xC5\x99\xC3\xADli\xC5\xA1 \xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD k\xC5\xAF\xC5\x88",
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xEE\x80\x90 \xE2\x89\x88 \xE2\x82\xAC | \xE4\xB8\xAD \xF0\x9F\x98\x80 \xE0\xA4\x95",
    };
    for(u32 face = 0; face < 4; face++) {
        for(u32 i = 0; i < 6; i++) {
            fude_zoom_pdf_text_face(pdf, 60, H - 70 - (f64)(face * 6 + i) * 30, 16, (rde_color){ 20, 20, 20, 255 }, scripts[i], (u8)face);
        }
    }
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    CHECK(page_count(&D) == 3);
    FILE* f = fopen("out.pdf", "wb");
    if(f != NULL) {
        fwrite(out.memory, 1, fude_bytes_size(&out), f);
        fclose(f);
    }
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
    fude_zoom_pdf_fonts(NULL);
}

int main(void) {
    test_numbers();
    test_structure();
    test_state();
    test_alpha();
    test_jpeg();
    test_rgba();
    test_text();
    test_fonts();
    write_sample();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
