// sheet.h: sheets written for 1, 3, 7 and 200+ characters (and in Japanese and
// Spanish), each checked as a PDF: header, every xref offset on its object, the
// trailer's startxref on "xref", the page count. argv[1]: the data.
#include "study/models/sheet.h"
#include "drawing/base/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static fude_kanji_db db;
static const char* OUT = "sheets";

static char* slurp(const char* path, long* size) {
    FILE* f = fopen(path, "rb"); if(!f) return NULL;
    fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
    char* d = malloc(*size + 1); fread(d, 1, *size, f); fclose(f); d[*size] = 0; return d;
}

// The file's structure; how many pages it says it has.
static int check_pdf(const char* path) {
    long n = 0; char* d = slurp(path, &n);
    CHECK(d != NULL && n > 100);
    if(!d) return -1;
    CHECK(memcmp(d, "%PDF-1.4\n", 9) == 0);
    char* sx = NULL; for(char* p = strstr(d, "startxref"); p; p = strstr(p + 1, "startxref")) sx = p;
    CHECK(sx != NULL);
    long xref = sx ? atol(sx + 10) : 0;
    CHECK(xref > 0 && xref < n && memcmp(d + xref, "xref\n", 5) == 0);
    int objects = 0; sscanf(d + xref + 5, "0 %d", &objects);
    char* line = strchr(d + xref + 5, '\n') + 1;   // past "0 N"
    line += 20;                                     // the free entry
    for(int o = 1; o < objects; o++, line += 20) {
        long off = atol(line);
        char want[32]; snprintf(want, sizeof want, "%d 0 obj\n", o);
        if(memcmp(d + off, want, strlen(want)) != 0) { printf("FAIL %s: object %d not at %ld\n", path, o, off); fails++; break; }
        CHECK(line[17] == 'n' && line[19] == '\n');
    }
    // Each stream's /Length is its bytes.
    for(char* s = strstr(d, "/Length "); s; s = strstr(s + 1, "/Length ")) {
        long len = atol(s + 8);
        char* st = strstr(s, "stream\n") + 7;
        if(memcmp(st + len, "\nendstream", 10) != 0) { printf("FAIL %s: a stream's length\n", path); fails++; break; }
    }
    CHECK(strstr(d, "%%EOF") != NULL);
    int pages = 0; char* c = strstr(d, "/Type /Pages /Count "); if(c) pages = atoi(c + 20);
    free(d);
    return pages;
}

static u32 recs(const char* text, u32* out, u32 max) {
    u32 n = 0; const c8* p = text;
    for(u32 cp = fude_utf8_next(&p); cp && n < max; cp = fude_utf8_next(&p)) { u32 r; if(fude_kanji_find_index(&db, cp, &r)) out[n++] = r; }
    return n;
}

int main(int argc, char** argv) {
    CHECK(argc > 1 && fude_kanji_load(&db, argv[1]));
    CHECK(fude_text_set_language(RDE_LANGUAGE_EN_US));
    mkdir(OUT, 0755);
    u32 r[400]; fude_sheet_info info; char path[256];
    const char* sets[] = { "木", "日本語", "あいうえおかき", "鬱" };
    const u32 want_pages[] = { 1, 1, 2, 1 };
    for(u32 s = 0; s < 4; s++) {
        const u32 n = recs(sets[s], r, 400);
        snprintf(path, sizeof path, "%s/sheet%u.pdf", OUT, s);
        CHECK(fude_sheet_write(&db, r, n, path, &info));
        CHECK(info.characters == n && info.pages == want_pages[s]);
        CHECK(check_pdf(path) == (int)info.pages);
        printf("%s: %u characters, %u pages, %u bytes\n", sets[s], info.characters, info.pages, info.bytes);
    }
    // Many: the first FUDE_SHEET_MAX, six a page.
    u32 all[400]; for(u32 i = 0; i < 400; i++) all[i] = i;
    CHECK(fude_sheet_write(&db, all, 400, "sheets/many.pdf", &info));
    CHECK(info.characters == FUDE_SHEET_MAX && info.pages == (FUDE_SHEET_MAX + 5) / 6);
    CHECK(check_pdf("sheets/many.pdf") == (int)info.pages);
    printf("many: %u characters, %u pages, %u bytes\n", info.characters, info.pages, info.bytes);
    // Other languages: the texts change, the file stays whole.
    CHECK(fude_text_set_language(RDE_LANGUAGE_JA_JP));
    u32 n = recs("練習", r, 8);
    CHECK(fude_sheet_write(&db, r, n, "sheets/ja.pdf", &info) && check_pdf("sheets/ja.pdf") == 1);
    CHECK(fude_text_set_language(RDE_LANGUAGE_FR_FR));
    fude_kanji_set_language(&db, "fr");
    CHECK(fude_sheet_write(&db, r, n, "sheets/fr.pdf", &info) && check_pdf("sheets/fr.pdf") == 1);
    // Nothing to draw, or nowhere to write: false, no file.
    CHECK(!fude_sheet_write(&db, r, 0, "sheets/none.pdf", &info));
    CHECK(!fude_sheet_write(&db, r, n, "/nonexistent/dir/x.pdf", &info));
    fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
