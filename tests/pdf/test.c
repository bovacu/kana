// The PDF writer (fude/zoom/pdf.c): every file it writes read back here as a
// reader would — the header, the xref (each offset landing on its object), the
// trailer, the page tree, each stream's /Length — and then what the pages say:
// numbers, alpha states, images, text. It also leaves out.pdf beside the suite,
// a page of everything, to be opened by hand.
#include "zoom/pdf.h"
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
    fude_bytes out;
    CHECK(finish(pdf, &out, &D));
    CHECK(page_count(&D) == 2);
    FILE* f = fopen("out.pdf", "wb");
    if(f != NULL) {
        fwrite(out.memory, 1, fude_bytes_size(&out), f);
        fclose(f);
    }
    rde_arr_free(&out);
    fude_zoom_pdf_free(pdf);
}

int main(void) {
    test_numbers();
    test_structure();
    test_state();
    test_alpha();
    test_jpeg();
    test_rgba();
    test_text();
    write_sample();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
