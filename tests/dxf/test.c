// The DXF writer for CNC and laser templates (fude/zoom/dxf): the file read
// back as (code, value) pairs — its sections in order, layers, units, extents,
// polylines, arcs, texts and numbers as written — and each pre-flight count on
// small cases made by hand. Also leaves sample.dxf beside it, for an outside
// reader (ezdxf's audit) to look at.
#include "zoom/dxf.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- reading the file back ---------------------------------------------------------------------

typedef struct { int code; char value[300]; } pair;
typedef struct { pair* p; int n; int ok; } pairs;

// Lines end in "\r\n" (no other CR or LF anywhere); a code line is an integer
// right-aligned in three; every code line has its value line after it.
static pairs parse(const fude_bytes* b) {
    pairs ps = { NULL, 0, 1 };
    const char* s = (const char*)b->memory;
    const u32 size = fude_bytes_size(b);
    int cap = 0, line = 0, code = 0;
    u32 at = 0;
    while(at < size) {
        u32 end = at;
        while(end < size && s[end] != '\r' && s[end] != '\n') end++;
        if(end + 1 >= size || s[end] != '\r' || s[end + 1] != '\n') { ps.ok = 0; break; }
        const u32 len = end - at;
        if(line % 2 == 0) {
            // "  0", " 10", "100": spaces, then digits to the end.
            if(len != 3) { ps.ok = 0; break; }
            u32 k = 0;
            while(k < 3 && s[at + k] == ' ') k++;
            if(k == 3) { ps.ok = 0; break; }
            code = 0;
            for(; k < 3; k++) {
                if(s[at + k] < '0' || s[at + k] > '9') { ps.ok = 0; break; }
                code = code * 10 + (s[at + k] - '0');
            }
            if(!ps.ok) break;
        } else {
            if(len >= sizeof(ps.p[0].value)) { ps.ok = 0; break; }
            if(ps.n == cap) { cap = cap ? cap * 2 : 64; ps.p = (pair*)realloc(ps.p, (size_t)cap * sizeof(pair)); }
            ps.p[ps.n].code = code;
            memcpy(ps.p[ps.n].value, s + at, len);
            ps.p[ps.n].value[len] = 0;
            ps.n++;
        }
        line++;
        at = end + 2;
    }
    if(line % 2 != 0) ps.ok = 0;
    return ps;
}

static void pairs_free(pairs* ps) { free(ps->p); ps->p = NULL; ps->n = 0; }

// The first pair at or after `from` with that code and value (NULL: any). -1: none.
static int find(const pairs* ps, int from, int code, const char* value) {
    for(int i = from < 0 ? 0 : from; i < ps->n; i++) {
        if(ps->p[i].code == code && (value == NULL || strcmp(ps->p[i].value, value) == 0)) return i;
    }
    return -1;
}

// The value of `code` in the record that starts at `at` (until the next 0 or 9). NULL: none.
static const char* field(const pairs* ps, int at, int code) {
    for(int i = at + 1; i < ps->n && ps->p[i].code != 0 && ps->p[i].code != 9; i++) {
        if(ps->p[i].code == code) return ps->p[i].value;
    }
    return NULL;
}

static int field_is(const pairs* ps, int at, int code, const char* want) {
    const char* v = field(ps, at, code);
    if(v == NULL || strcmp(v, want) != 0) {
        printf("  code %d: \"%s\", wanted \"%s\"\n", code, v ? v : "(none)", want);
        return 0;
    }
    return 1;
}

// A header variable's record (the "9 $NAME" pair). -1: none.
static int header_var(const pairs* ps, const char* name) {
    const int h = find(ps, 0, 2, "HEADER");
    const int end = find(ps, h, 0, "ENDSEC");
    const int i = find(ps, h, 9, name);
    return (h < 0 || i < 0 || i > end) ? -1 : i;
}

// The ENTITIES section's record starts, in order, into `at` (room for `max`). How many.
static int entities(const pairs* ps, int* at, int max) {
    const int s = find(ps, 0, 2, "ENTITIES");
    const int end = find(ps, s, 0, "ENDSEC");
    int n = 0;
    if(s < 0 || end < 0) return 0;
    for(int i = s + 1; i < end; i++) {
        if(ps->p[i].code == 0 && n < max) at[n++] = i;
    }
    return n;
}

static pairs finish(const fude_zoom_dxf* d) {
    fude_bytes b = fude_bytes_new(256);
    CHECK(fude_zoom_dxf_finish(d, &b));
    pairs ps = parse(&b);
    CHECK(ps.ok);
    rde_arr_free(&b);
    return ps;
}

// --- the file ----------------------------------------------------------------------------------

static void test_structure(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    const u32 cut = fude_zoom_dxf_layer(d, "cut", 1);
    fude_zoom_dxf_line(d, cut, 0, 0, 10, 0);
    pairs ps = finish(d);

    // HEADER, TABLES, BLOCKS, ENTITIES, each closed before the next opens; EOF last.
    const char* order[] = { "HEADER", "TABLES", "BLOCKS", "ENTITIES" };
    int at = 0;
    for(int k = 0; k < 4; k++) {
        const int s = find(&ps, at, 0, "SECTION");
        CHECK(s >= 0 && s + 1 < ps.n && ps.p[s + 1].code == 2 && strcmp(ps.p[s + 1].value, order[k]) == 0);
        const int e = find(&ps, s, 0, "ENDSEC");
        const int next = find(&ps, s + 1, 0, "SECTION");
        CHECK(e > s && (next < 0 || e < next));
        at = e;
    }
    CHECK(find(&ps, at + 1, 0, "SECTION") < 0);
    CHECK(ps.n > 0 && ps.p[ps.n - 1].code == 0 && strcmp(ps.p[ps.n - 1].value, "EOF") == 0);
    CHECK(find(&ps, 0, 0, "EOF") == ps.n - 1);

    // Version and units.
    int v = header_var(&ps, "$ACADVER");
    CHECK(v >= 0 && field_is(&ps, v, 1, "AC1009"));
    v = header_var(&ps, "$INSUNITS");
    CHECK(v >= 0 && field_is(&ps, v, 70, "4"));
    v = header_var(&ps, "$MEASUREMENT");
    CHECK(v >= 0 && field_is(&ps, v, 70, "1"));

    // Tables: the CONTINUOUS line type, the STANDARD style; BLOCKS empty.
    const int lt = find(&ps, find(&ps, 0, 2, "TABLES"), 2, "LTYPE");
    CHECK(lt >= 0 && find(&ps, lt, 2, "CONTINUOUS") > lt);
    CHECK(find(&ps, 0, 2, "STANDARD") > find(&ps, 0, 2, "STYLE"));
    const int blocks = find(&ps, 0, 2, "BLOCKS");
    CHECK(blocks >= 0 && ps.p[blocks + 1].code == 0 && strcmp(ps.p[blocks + 1].value, "ENDSEC") == 0);

    // The line, its layer.
    int e[8];
    CHECK(entities(&ps, e, 8) == 1);
    CHECK(strcmp(ps.p[e[0]].value, "LINE") == 0);
    CHECK(field_is(&ps, e[0], 8, "cut"));
    CHECK(field_is(&ps, e[0], 11, "10"));
    CHECK(field_is(&ps, e[0], 31, "0"));
    pairs_free(&ps);

    // Appended after what _out held; the builder can be finished again.
    fude_bytes a = fude_bytes_new(16), b = fude_bytes_new(16);
    fude_put_data(&a, "xyz", 3);
    CHECK(fude_zoom_dxf_finish(d, &a));
    CHECK(fude_zoom_dxf_finish(d, &b));
    CHECK(fude_bytes_size(&a) == fude_bytes_size(&b) + 3);
    CHECK(memcmp(a.memory, "xyz  0\r\nSECTION\r\n", 17) == 0);
    CHECK(memcmp(a.memory + 3, b.memory, fude_bytes_size(&b)) == 0);
    CHECK(!fude_zoom_dxf_finish(NULL, &a));
    CHECK(!fude_zoom_dxf_finish(d, NULL));
    CHECK(fude_bytes_size(&a) == fude_bytes_size(&b) + 3);
    rde_arr_free(&a);
    rde_arr_free(&b);
    fude_zoom_dxf_free(d);
    fude_zoom_dxf_free(NULL);
}

static void test_layers(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    const u32 cut = fude_zoom_dxf_layer(d, "cut", 1);
    CHECK(cut == 1);
    CHECK(fude_zoom_dxf_layer(d, "CUT", 3) == cut);                  // without case; first colour kept
    CHECK(fude_zoom_dxf_layer(d, "cut", 1) == cut);
    const u32 score = fude_zoom_dxf_layer(d, "score", 5);
    CHECK(score == 2);
    const u32 odd = fude_zoom_dxf_layer(d, "en grave/\xC3\xA9", 4);   // é: one character, one '_'
    CHECK(odd == 3);
    CHECK(fude_zoom_dxf_layer(d, "en_grave__", 9) == odd);
    const u32 lng = fude_zoom_dxf_layer(d, "abcdefghijklmnopqrstuvwxyz0123456789", 6);
    CHECK(lng == 4);
    CHECK(fude_zoom_dxf_layer(d, "abcdefghijklmnopqrstuvwxyz01234", 6) == lng);   // the first 31
    CHECK(fude_zoom_dxf_layer(d, "0", 2) == 0);
    CHECK(fude_zoom_dxf_layer(d, "", 2) == 0);
    CHECK(fude_zoom_dxf_layer(d, NULL, 2) == 0);
    const u32 x = fude_zoom_dxf_layer(d, "drill", 0);                  // 0 is no layer colour: 7
    CHECK(x == 5);
    CHECK(fude_zoom_dxf_layer(NULL, "cut", 1) == 0);
    fude_zoom_dxf_line(d, score, 0, 0, 1, 1);
    fude_zoom_dxf_line(d, 99, 0, 0, 1, 1);                            // not a layer: "0"
    fude_zoom_dxf_line(d, odd, 0, 0, 1, 1);
    pairs ps = finish(d);

    const int table = find(&ps, find(&ps, 0, 2, "TABLES"), 2, "LAYER");
    const int end = find(&ps, table, 0, "ENDTAB");
    CHECK(table >= 0 && field_is(&ps, table, 70, "6"));
    const char* names[] = { "0", "cut", "score", "en_grave__", "abcdefghijklmnopqrstuvwxyz01234", "drill" };
    const char* colours[] = { "7", "1", "5", "4", "6", "7" };
    int n = 0;
    for(int i = find(&ps, table, 0, "LAYER"); i >= 0 && i < end; i = find(&ps, i + 1, 0, "LAYER")) {
        if(n < 6) {
            CHECK(field_is(&ps, i, 2, names[n]));
            CHECK(field_is(&ps, i, 62, colours[n]));
            CHECK(field_is(&ps, i, 6, "CONTINUOUS"));
            CHECK(field_is(&ps, i, 70, "0"));
        }
        n++;
    }
    CHECK(n == 6);
    int e[8];
    CHECK(entities(&ps, e, 8) == 3);
    CHECK(field_is(&ps, e[0], 8, "score"));
    CHECK(field_is(&ps, e[1], 8, "0"));
    CHECK(field_is(&ps, e[2], 8, "en_grave__"));
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

static void test_extents(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    fude_zoom_dxf_line(d, 0, -5, 2, 10, 3);
    fude_zoom_dxf_circle(d, 0, 20, 0, 5);
    fude_zoom_dxf_text(d, 0, 3, 7, 2.5, 0, "A");          // its insertion point only
    pairs ps = finish(d);
    int v = header_var(&ps, "$EXTMIN");
    CHECK(v >= 0 && field_is(&ps, v, 10, "-5") && field_is(&ps, v, 20, "-5") && field_is(&ps, v, 30, "0"));
    v = header_var(&ps, "$EXTMAX");
    CHECK(v >= 0 && field_is(&ps, v, 10, "25") && field_is(&ps, v, 20, "7") && field_is(&ps, v, 30, "0"));
    pairs_free(&ps);
    fude_zoom_dxf_free(d);

    // An arc's box: its ends, and the top of the circle it passes over.
    d = fude_zoom_dxf_new();
    fude_zoom_dxf_arc(d, 0, 0, 0, 10, 45, 135);
    ps = finish(d);
    v = header_var(&ps, "$EXTMIN");
    CHECK(v >= 0 && field_is(&ps, v, 10, "-7.071068") && field_is(&ps, v, 20, "7.071068"));
    v = header_var(&ps, "$EXTMAX");
    CHECK(v >= 0 && field_is(&ps, v, 10, "7.071068") && field_is(&ps, v, 20, "10"));
    pairs_free(&ps);
    fude_zoom_dxf_free(d);

    // Round past 0 degrees: the right of the circle, not the left.
    d = fude_zoom_dxf_new();
    fude_zoom_dxf_arc(d, 0, 100, 0, 10, 300, 30);
    ps = finish(d);
    v = header_var(&ps, "$EXTMIN");
    CHECK(v >= 0 && field_is(&ps, v, 10, "105") && field_is(&ps, v, 20, "-8.660254"));
    v = header_var(&ps, "$EXTMAX");
    CHECK(v >= 0 && field_is(&ps, v, 10, "110") && field_is(&ps, v, 20, "5"));
    pairs_free(&ps);
    fude_zoom_dxf_free(d);

    // Nothing at all: zeros, not infinities.
    d = fude_zoom_dxf_new();
    ps = finish(d);
    v = header_var(&ps, "$EXTMIN");
    CHECK(v >= 0 && field_is(&ps, v, 10, "0") && field_is(&ps, v, 20, "0"));
    v = header_var(&ps, "$EXTMAX");
    CHECK(v >= 0 && field_is(&ps, v, 10, "0") && field_is(&ps, v, 20, "0"));
    int e[4];
    CHECK(entities(&ps, e, 4) == 0);
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

static void test_polylines(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    const u32 cut = fude_zoom_dxf_layer(d, "cut", 1);
    const f64 square[] = { 0, 0, 10, 0, 10, 10, 0, 10 };
    const f64 repeat[] = { 0, 0, 10, 0, 10, 10, 0, 10, 0, 0 };
    const f64 open[] = { 0, 0, 5, 5, 10, 0 };
    fude_zoom_dxf_polyline(d, cut, square, 4, true);
    fude_zoom_dxf_polyline(d, cut, repeat, 5, true);        // the repeat dropped
    fude_zoom_dxf_polyline(d, cut, open, 3, false);
    fude_zoom_dxf_polyline(d, cut, open, 1, false);         // nothing to cut
    fude_zoom_dxf_polyline(d, cut, NULL, 3, false);
    fude_zoom_dxf_polyline(d, cut, open, 0, true);
    CHECK(fude_zoom_dxf_preflight(d, 0.05).entities == 3);
    pairs ps = finish(d);
    int e[64];
    const int n = entities(&ps, e, 64);
    // POLYLINE, 4 VERTEX, SEQEND; again; POLYLINE, 3 VERTEX, SEQEND.
    CHECK(n == 6 + 6 + 5);
    if(n == 17) {
        const int starts[] = { 0, 6, 12 };
        const int counts[] = { 4, 4, 3 };
        const char* flags[] = { "1", "1", "0" };
        for(int k = 0; k < 3; k++) {
            const int p = e[starts[k]];
            CHECK(strcmp(ps.p[p].value, "POLYLINE") == 0);
            CHECK(field_is(&ps, p, 66, "1"));
            CHECK(field_is(&ps, p, 70, flags[k]));
            CHECK(field_is(&ps, p, 8, "cut"));
            for(int j = 1; j <= counts[k]; j++) {
                CHECK(strcmp(ps.p[e[starts[k] + j]].value, "VERTEX") == 0);
                CHECK(field_is(&ps, e[starts[k] + j], 8, "cut"));
            }
            CHECK(strcmp(ps.p[e[starts[k] + counts[k] + 1]].value, "SEQEND") == 0);
        }
        CHECK(field_is(&ps, e[3], 10, "10") && field_is(&ps, e[3], 20, "10") && field_is(&ps, e[3], 30, "0"));
        CHECK(field_is(&ps, e[10], 10, "0") && field_is(&ps, e[10], 20, "10"));   // the second's last: (0, 10)
    }
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

static void test_arcs(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    fude_zoom_dxf_arc(d, 0, 1, 2, 3, -90, 90);      // the right half
    fude_zoom_dxf_arc(d, 0, 0, 0, 3, 350, 10);      // 20 degrees over 0
    fude_zoom_dxf_arc(d, 0, 0, 0, 3, 400, 450);
    fude_zoom_dxf_arc(d, 0, 0, 0, -4, 0, 90);       // a negative radius is its size
    fude_zoom_dxf_arc(d, 0, 5, 6, 2, 0, 360);       // a whole turn: a circle
    fude_zoom_dxf_arc(d, 0, 5, 6, 2, 90, -270);
    fude_zoom_dxf_arc(d, 0, 5, 6, 2, 30, 30);       // no sweep: nothing
    fude_zoom_dxf_arc(d, 0, 5, 6, 2, 30, 30.000001);  // too little to tell from a whole turn at six decimals
    pairs ps = finish(d);
    int e[16];
    CHECK(entities(&ps, e, 16) == 6);
    CHECK(strcmp(ps.p[e[0]].value, "ARC") == 0);
    CHECK(field_is(&ps, e[0], 10, "1") && field_is(&ps, e[0], 20, "2") && field_is(&ps, e[0], 40, "3"));
    CHECK(field_is(&ps, e[0], 50, "270") && field_is(&ps, e[0], 51, "90"));
    CHECK(field_is(&ps, e[1], 50, "350") && field_is(&ps, e[1], 51, "10"));
    CHECK(field_is(&ps, e[2], 50, "40") && field_is(&ps, e[2], 51, "90"));
    CHECK(field_is(&ps, e[3], 40, "4"));
    CHECK(strcmp(ps.p[e[4]].value, "CIRCLE") == 0 && field_is(&ps, e[4], 40, "2") && field_is(&ps, e[4], 10, "5"));
    CHECK(strcmp(ps.p[e[5]].value, "CIRCLE") == 0);
    CHECK(field(&ps, e[4], 50) == NULL);
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

static void test_text(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    const u32 eng = fude_zoom_dxf_layer(d, "engrave", 3);
    fude_zoom_dxf_text(d, eng, 1, 2, 2.5, 90, "Ok 50% \xC3\xA9 \xE6\x97\xA5 a%%b");
    fude_zoom_dxf_text(d, eng, 0, 0, 0, -90, "\\U+0041 \\x\tz\n\xF0\x9F\x98\x80");
    fude_zoom_dxf_text(d, eng, 0, 0, 3, 0, "");                 // nothing to engrave
    fude_zoom_dxf_text(d, eng, 0, 0, 3, 0, NULL);
    char longest[401];
    memset(longest, 'x', 400);
    longest[400] = 0;
    fude_zoom_dxf_text(d, eng, 0, 0, 3, 0, longest);
    char accents[201];
    for(int i = 0; i < 100; i++) { accents[i * 2] = (char)0xC3; accents[i * 2 + 1] = (char)0xA9; }
    accents[200] = 0;
    fude_zoom_dxf_text(d, eng, 0, 0, 3, 0, accents);
    pairs ps = finish(d);
    int e[8];
    CHECK(entities(&ps, e, 8) == 4);
    CHECK(strcmp(ps.p[e[0]].value, "TEXT") == 0);
    CHECK(field_is(&ps, e[0], 8, "engrave"));
    CHECK(field_is(&ps, e[0], 1, "Ok 50% \\U+00E9 \\U+65E5 a%%%%b"));
    CHECK(field_is(&ps, e[0], 40, "2.5") && field_is(&ps, e[0], 50, "90"));
    CHECK(field_is(&ps, e[0], 10, "1") && field_is(&ps, e[0], 20, "2"));
    // A backslash that would start an escape, controls as spaces, past U+FFFF a pair.
    CHECK(field_is(&ps, e[1], 1, "\\U+005CU+0041 \\x z \\U+D83D\\U+DE00"));
    CHECK(field_is(&ps, e[1], 40, "1") && field_is(&ps, e[1], 50, "270"));
    // At most 255 bytes, cut at a whole character: 36 escapes of 7.
    const char* t = field(&ps, e[2], 1);
    CHECK(t != NULL && strlen(t) == 255 && t[0] == 'x');
    t = field(&ps, e[3], 1);
    CHECK(t != NULL && strlen(t) == 36 * 7 && strncmp(t + 35 * 7, "\\U+00E9", 7) == 0);
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

static void test_numbers(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    fude_zoom_dxf_line(d, 0, 1.5, 2.0, 0.1234567, -3.25);
    fude_zoom_dxf_line(d, 0, NAN, INFINITY, -0.0000001, 1e20);
    fude_zoom_dxf_line(d, 0, 0.00005, -0.5, 123456.789, -0.0);
    fude_zoom_dxf_line(d, 0, 0.1, 1e-7, -1e9, 999999999.9999999);
    fude_zoom_dxf_circle(d, 0, 0, 0, NAN);
    pairs ps = finish(d);
    int e[8];
    CHECK(entities(&ps, e, 8) == 5);
    CHECK(field_is(&ps, e[0], 10, "1.5") && field_is(&ps, e[0], 20, "2"));
    CHECK(field_is(&ps, e[0], 11, "0.123457") && field_is(&ps, e[0], 21, "-3.25"));
    CHECK(field_is(&ps, e[1], 10, "0") && field_is(&ps, e[1], 20, "0"));
    CHECK(field_is(&ps, e[1], 11, "0") && field_is(&ps, e[1], 21, "100000000000000000000"));
    CHECK(field_is(&ps, e[2], 10, "0.00005") && field_is(&ps, e[2], 20, "-0.5"));
    CHECK(field_is(&ps, e[2], 11, "123456.789") && field_is(&ps, e[2], 21, "0"));
    CHECK(field_is(&ps, e[3], 10, "0.1") && field_is(&ps, e[3], 20, "0"));
    CHECK(field_is(&ps, e[3], 11, "-1000000000") && field_is(&ps, e[3], 21, "1000000000"));
    CHECK(field_is(&ps, e[4], 40, "0"));
    // Every real (codes 10 to 59) in the file: digits, one point at most, no
    // trailing zero after it, no exponent, a sign only in front, never "-0".
    int reals = 0;
    for(int i = 0; i < ps.n; i++) {
        if(ps.p[i].code < 10 || ps.p[i].code > 59) continue;
        const char* v = ps.p[i].value;
        const size_t len = strlen(v);
        int good = len > 0 && strspn(v + (v[0] == '-'), "0123456789.") == len - (v[0] == '-');
        const char* dot = strchr(v, '.');
        if(dot != NULL) good = good && strchr(dot + 1, '.') == NULL && v[len - 1] != '0' && v[len - 1] != '.';
        good = good && strcmp(v, "-0") != 0;
        if(!good) printf("  bad number \"%s\" at code %d\n", v, ps.p[i].code);
        CHECK(good);
        reals++;
    }
    CHECK(reals > 20);
    pairs_free(&ps);
    fude_zoom_dxf_free(d);
}

// --- pre-flight --------------------------------------------------------------------------------

static void square_of_lines(fude_zoom_dxf* d, u32 layer, f64 x, f64 y, f64 s, f64 gap) {
    fude_zoom_dxf_line(d, layer, x, y, x + s, y);
    fude_zoom_dxf_line(d, layer, x + s, y, x + s, y + s);
    fude_zoom_dxf_line(d, layer, x + s, y + s + gap, x, y + s);   // runs the other way round, a gap at its start
    fude_zoom_dxf_line(d, layer, x, y, x, y + s);
}

static void test_preflight(void) {
    const f64 tiny = 0.05;
    fude_zoom_dxf_check c;

    // Four lines round a square: closed. A lone line: open.
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    square_of_lines(d, 0, 0, 0, 10, 0);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.entities == 4 && c.open_paths == 0 && c.duplicates == 0 && c.tiny_segments == 0);
    fude_zoom_dxf_line(d, 0, 50, 50, 60, 50);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.entities == 5 && c.open_paths == 1 && c.duplicates == 0);
    // Drawn back over itself: a duplicate, still one open path, not a loop.
    fude_zoom_dxf_line(d, 0, 60, 50.01, 50.02, 50);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.entities == 6 && c.open_paths == 1 && c.duplicates == 1);
    // The same on another layer is another operation, not a duplicate.
    const u32 score = fude_zoom_dxf_layer(d, "score", 2);
    fude_zoom_dxf_line(d, score, 50, 50, 60, 50);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 1 && c.open_paths == 2);
    // Exactly: a tolerance of 0 only takes the same numbers (the line back
    // is no longer the same, nor do its ends meet the first's).
    c = fude_zoom_dxf_preflight(d, 0);
    CHECK(c.duplicates == 0 && c.open_paths == 3 && c.tiny_segments == 0);
    fude_zoom_dxf_free(d);

    // Corners a little apart: within the tolerance closed, past it one open path.
    d = fude_zoom_dxf_new();
    square_of_lines(d, 0, 0, 0, 10, 0.02);
    CHECK(fude_zoom_dxf_preflight(d, tiny).open_paths == 0);
    square_of_lines(d, 0, 20, 0, 10, 0.1);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 1 && c.duplicates == 0 && c.entities == 8);
    fude_zoom_dxf_free(d);

    // Three lines in a row: one open path. Three meeting at a point: one.
    // Two apart: two. A tiny one: a tiny segment.
    d = fude_zoom_dxf_new();
    fude_zoom_dxf_line(d, 0, 0, 0, 10, 0);
    fude_zoom_dxf_line(d, 0, 10, 0, 10, 10);
    fude_zoom_dxf_line(d, 0, 20, 10, 10, 10);
    CHECK(fude_zoom_dxf_preflight(d, tiny).open_paths == 1);
    fude_zoom_dxf_line(d, 0, 100, 100, 110, 100);
    fude_zoom_dxf_line(d, 0, 100, 100, 90, 110);
    fude_zoom_dxf_line(d, 0, 100, 100, 90, 90);
    CHECK(fude_zoom_dxf_preflight(d, tiny).open_paths == 2);
    fude_zoom_dxf_line(d, 0, 200, 0, 200.01, 0);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 3 && c.tiny_segments == 1);
    fude_zoom_dxf_free(d);

    // Polylines: open with ends apart is open; ends meeting, or closed, is not.
    d = fude_zoom_dxf_new();
    const f64 vee[] = { 0, 0, 5, 5, 10, 0 };
    const f64 loop[] = { 0, 0, 10, 0, 10, 10, 0.03, 0 };
    const f64 square[] = { 0, 0, 10, 0, 10, 10, 0, 10 };
    const f64 shifted[] = { 10, 10.01, 10, 0, 0, 0, 0, 10 };   // the square from another corner, the other way
    const f64 back[] = { 10, 0, 5, 5, 0, 0 };
    fude_zoom_dxf_polyline(d, 0, vee, 3, false);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 1 && c.entities == 1);
    fude_zoom_dxf_polyline(d, 0, loop, 4, false);
    fude_zoom_dxf_polyline(d, 0, square, 4, true);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 1 && c.duplicates == 0 && c.tiny_segments == 0);   // the loop's gap is no side of it
    fude_zoom_dxf_polyline(d, 0, shifted, 4, true);
    fude_zoom_dxf_polyline(d, 0, back, 3, false);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 2 && c.open_paths == 1 && c.entities == 5);
    fude_zoom_dxf_free(d);
    // A speck's ends meet, but it never left them: still open.
    d = fude_zoom_dxf_new();
    const f64 speck[] = { 0, 0, 0.02, 0, 0.01, 0.01 };
    fude_zoom_dxf_polyline(d, 0, speck, 3, false);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 1 && c.tiny_segments == 2);
    fude_zoom_dxf_free(d);
    // A closed one is not the same as an open one through the same points.
    d = fude_zoom_dxf_new();
    fude_zoom_dxf_polyline(d, 0, square, 4, true);
    fude_zoom_dxf_polyline(d, 0, square, 4, false);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 0 && c.open_paths == 1);
    fude_zoom_dxf_free(d);

    // A two-point polyline is a line for duplicates; a closed polyline's
    // repeated last point is no tiny closing side.
    d = fude_zoom_dxf_new();
    const f64 two[] = { 10, 0, 0, 0 };
    const f64 shut[] = { 0, 0, 10, 0, 10, 10, 0, 0 };
    const f64 stutter[] = { 0, 0, 10, 0, 10, 0.001, 10, 10 };
    fude_zoom_dxf_line(d, 0, 0, 0, 10, 0);
    fude_zoom_dxf_polyline(d, 0, two, 2, false);
    fude_zoom_dxf_polyline(d, 0, shut, 4, true);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 1 && c.tiny_segments == 0 && c.open_paths == 1);
    fude_zoom_dxf_polyline(d, 0, stutter, 4, false);
    CHECK(fude_zoom_dxf_preflight(d, tiny).tiny_segments == 1);
    fude_zoom_dxf_free(d);

    // Arcs join contours: a slot of two lines and two half circles is closed;
    // a lone arc is open; one all but round is closed by its own ends.
    d = fude_zoom_dxf_new();
    fude_zoom_dxf_line(d, 0, 0, 0, 10, 0);
    fude_zoom_dxf_arc(d, 0, 10, 5, 5, 270, 90);
    fude_zoom_dxf_line(d, 0, 10, 10, 0, 10);
    fude_zoom_dxf_arc(d, 0, 0, 5, 5, 90, 270);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.open_paths == 0 && c.entities == 4 && c.tiny_segments == 0);
    fude_zoom_dxf_arc(d, 0, 50, 50, 5, 0, 90);
    CHECK(fude_zoom_dxf_preflight(d, tiny).open_paths == 1);
    fude_zoom_dxf_arc(d, 0, 100, 0, 1, 10, 369.99);
    CHECK(fude_zoom_dxf_preflight(d, tiny).open_paths == 1);
    // The same arc again (its angles a turn round), and one the other way round the circle.
    fude_zoom_dxf_arc(d, 0, 50, 50, 5, 360, 450);
    fude_zoom_dxf_arc(d, 0, 50, 50, 5, 90, 0);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 1 && c.open_paths == 0);   // the quarter and three quarters close a circle
    // Tiny arcs and circles by their length.
    fude_zoom_dxf_arc(d, 0, 200, 0, 1, 0, 0.5);
    fude_zoom_dxf_circle(d, 0, 300, 0, 0.005);
    fude_zoom_dxf_circle(d, 0, 300, 0, 0.5);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.tiny_segments == 2 && c.open_paths == 1);
    fude_zoom_dxf_free(d);

    // Circles and texts as duplicates.
    d = fude_zoom_dxf_new();
    const u32 drill = fude_zoom_dxf_layer(d, "drill", 4);
    const u32 eng = fude_zoom_dxf_layer(d, "engrave", 3);
    fude_zoom_dxf_circle(d, drill, 0, 0, 3);
    fude_zoom_dxf_circle(d, drill, 0.01, 0, 3.02);
    fude_zoom_dxf_circle(d, drill, 0, 0, 3.2);
    fude_zoom_dxf_circle(d, eng, 0, 0, 3);
    fude_zoom_dxf_text(d, eng, 5, 5, 3, 0, "A1");
    fude_zoom_dxf_text(d, eng, 5, 5, 3, 360, "A1");
    fude_zoom_dxf_text(d, eng, 5, 5, 3, 0, "A2");
    fude_zoom_dxf_text(d, eng, 5, 5, 3, 90, "A1");
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.duplicates == 2 && c.open_paths == 0 && c.tiny_segments == 0 && c.entities == 8);
    fude_zoom_dxf_free(d);

    // Nothing, and no drawing.
    d = fude_zoom_dxf_new();
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.entities == 0 && c.open_paths == 0 && c.duplicates == 0 && c.tiny_segments == 0);
    fude_zoom_dxf_free(d);
    c = fude_zoom_dxf_preflight(NULL, tiny);
    CHECK(c.entities == 0);
    // Many pieces: the sweep finds the same answer as the hand count.
    d = fude_zoom_dxf_new();
    for(int i = 0; i < 300; i++) square_of_lines(d, 0, (i % 20) * 15.0, (i / 20) * 15.0, 10, 0);
    for(int i = 0; i < 50; i++) fude_zoom_dxf_line(d, 0, i * 3.0, -20, i * 3.0 + 1, -25);
    for(int i = 0; i < 10; i++) square_of_lines(d, 0, (i % 20) * 15.0, 0, 10, 0);
    c = fude_zoom_dxf_preflight(d, tiny);
    CHECK(c.entities == 1290 && c.open_paths == 50 && c.duplicates == 40 && c.tiny_segments == 0);
    fude_zoom_dxf_free(d);
}

// A template with a bit of everything, for an outside reader to audit.
static void write_sample(void) {
    fude_zoom_dxf* d = fude_zoom_dxf_new();
    const u32 cut = fude_zoom_dxf_layer(d, "cut", 1);
    const u32 score = fude_zoom_dxf_layer(d, "score", 5);
    const u32 eng = fude_zoom_dxf_layer(d, "engrave", 3);
    const u32 drill = fude_zoom_dxf_layer(d, "drill", 4);
    const f64 outline[] = { 0, 0, 200, 0, 200, 120, 0, 120 };
    fude_zoom_dxf_polyline(d, cut, outline, 4, true);
    fude_zoom_dxf_line(d, score, 100, 0, 100, 120);
    fude_zoom_dxf_line(d, cut, 20, 20, 60, 20);
    fude_zoom_dxf_arc(d, cut, 60, 30, 10, 270, 90);
    fude_zoom_dxf_line(d, cut, 60, 40, 20, 40);
    fude_zoom_dxf_arc(d, cut, 20, 30, 10, 90, 270);
    for(int i = 0; i < 4; i++) fude_zoom_dxf_circle(d, drill, 10 + (i % 2) * 180, 10 + (i / 2) * 100, 2.5);
    fude_zoom_dxf_text(d, eng, 120, 60, 8, 0, "Shelf 50% \xC3\xA9");
    fude_zoom_dxf_text(d, eng, 150, 20, 5, 90, "\xE6\x97\xA5\xE6\x9C\xAC");
    fude_bytes b = fude_bytes_new(4096);
    CHECK(fude_zoom_dxf_finish(d, &b));
    FILE* f = fopen("sample.dxf", "wb");
    CHECK(f != NULL);
    if(f != NULL) {
        CHECK(fwrite(b.memory, 1, fude_bytes_size(&b), f) == fude_bytes_size(&b));
        fclose(f);
    }
    rde_arr_free(&b);
    fude_zoom_dxf_free(d);
}

int main(void) {
    test_structure();
    test_layers();
    test_extents();
    test_polylines();
    test_arcs();
    test_text();
    test_numbers();
    test_preflight();
    write_sample();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
