// What draw.c keeps of texts (fude/drawing/base/draw.c): the wrapped layouts and the balanced widths, each kept in an
// rde_lru by the whole of what it is made from (the font and its size, the size drawn at, the width, every byte of the
// text). Every answer, asked again in a random order over many more texts and widths than are kept (so kept ones are
// let go and made again, and others found), is what it was when first made; two texts of one length that break
// differently never share a layout; lines are no wider than their width and hold every word; a balanced width keeps
// its text's number of lines; a text of any length is laid out and drawn whole, every line of it; a text drawn whole on
// several lines keeps the size it fits at. The tests have no font: widths are draw.c's estimates (0.6 of the size for
// a Latin character, 1.3 for a CJK one), the same each time.
#include "drawing/base/draw.h"
#include "drawing/base/utf8.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static u64 seed = 0x243F6A8885A308D3ull;
static u32 next(void) {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return (u32)(seed >> 16);
}

#define TEXTS  90u   // texts × WIDTHS layouts asked for: far more than draw.c keeps (64)
#define WIDTHS 3u
#define LINES  32u   // the most lines of a layout these checks ask for
#define PX     10.0f

// What fude_draw_text hands the engine (no font draws anything here): how many texts, and the longest one's length.
static u32   drawn = 0;
static usize drawn_longest = 0;
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) {
    (void)f; (void)p; (void)s; (void)r; (void)c;
    drawn++;
    drawn_longest = strlen(t) > drawn_longest ? strlen(t) : drawn_longest;
}

// A layout as fude_draw_text_wrap_spans gives it.
typedef struct { u32 n; u32 from[LINES], to[LINES]; } spans;

static spans layout(rde_font* f, const c8* t, f32 w) {
    spans s;
    memset(&s, 0, sizeof s);
    s.n = fude_draw_text_wrap_spans(f, 20.0f, t, PX, w, s.from, s.to, LINES);
    return s;
}

static b8 same(const spans* a, const spans* b) {
    if(a->n != b->n) return false;
    for(u32 i = 0; i < a->n; i++) {
        if(a->from[i] != b->from[i] || a->to[i] != b->to[i]) return false;
    }
    return true;
}

// The estimated width of bytes [from, to) of t, as the tests measure (no font).
static f32 estimate(const c8* t, u32 from, u32 to) {
    f32 w = 0.0f;
    const c8* p = t + from;
    while((u32)(p - t) < to) {
        const u32 cp = fude_utf8_next(&p);
        w += (cp >= 0x2E80u ? 1.3f : 0.6f) * PX;
    }
    return w;
}

// The engine's measure (tests/support/rich.c's measures nothing, so widths are estimates): for the font measured below,
// a width from the text's bytes, counted; for any other, nothing, as before.
static u32 measured = 0;
static u8  measured_font;
rde_vec_2F rde_rich_text_measure(const c8* m, rde_font* f, f32 s, f32 w, b8 wrap) {
    (void)s; (void)w; (void)wrap;
    if(f != (rde_font*)(any)&measured_font) return (rde_vec_2F){ 0.0f, 0.0f };
    measured++;
    u32 sum = 0;
    for(const c8* c = m; *c != 0; c++) sum += (u8)*c;
    return (rde_vec_2F){ (f32)strlen(m) * 11.0f + (f32)(sum % 7u), 20.0f };
}

// The widths measured by the engine (draw.c's exact widths: an rde_lru of 1536, keyed by font, its size and the whole
// text): each is what the engine measures, measured once; past 1536 texts, the one used longest ago is measured again
// and the ones a screen draws every frame never are (they were, every time the map filled and was emptied); another
// font size is a width of its own; a text past the key's room is measured each time, right still. Counted: 300 texts
// a frame and 50 new ones each frame, over 50 frames.
static void test_exact_widths(void) {
    rde_font* font = (rde_font*)(any)&measured_font;
    static c8 texts[3000][40];
    for(u32 i = 0; i < 3000u; i++) snprintf(texts[i], sizeof texts[i], "a label %u, its words %u", i, i * 31u);
    u32 sum0 = 0;
    for(const c8* c = texts[0]; *c != 0; c++) sum0 += (u8)*c;
    measured = 0;
    const f32 w0 = fude_draw_text_width(font, 20.0f, texts[0], 10.0f);
    CHECK(measured == 1u && w0 == ((f32)strlen(texts[0]) * 11.0f + (f32)(sum0 % 7u)) / 20.0f * 10.0f);
    CHECK(fude_draw_text_width(font, 20.0f, texts[0], 10.0f) == w0 && measured == 1u);
    CHECK(fude_draw_text_width(font, 40.0f, texts[0], 10.0f) == w0 * 0.5f && measured == 2u);   // (the font at another size)
    // 300 drawn every frame, 50 new ones each frame: 2,800 texts in all, the 300 measured once.
    measured = 0;
    u32 wrong = 0;
    for(u32 f = 0; f < 50u; f++) {
        for(u32 i = 0; i < 300u; i++) {
            const f32 w = fude_draw_text_width(font, 20.0f, texts[i], 10.0f);
            u32 sum = 0;
            for(const c8* c = texts[i]; *c != 0; c++) sum += (u8)*c;
            wrong += w == ((f32)strlen(texts[i]) * 11.0f + (f32)(sum % 7u)) / 20.0f * 10.0f ? 0u : 1u;
        }
        for(u32 i = 0; i < 50u; i++) fude_draw_text_width(font, 20.0f, texts[300u + f * 50u + i], 10.0f);
    }
    // (what the map emptied whenever it held 1536 measured, run over the same texts: those kept, by a mark each)
    static u8 old_kept[3000];
    memset(old_kept, 0, sizeof old_kept);
    u32 old_count = 1u, old_measured = 0;
    old_kept[0] = 1u;
    for(u32 f = 0; f < 50u; f++) {
        for(u32 i = 0; i < 350u; i++) {
            const u32 t = i < 300u ? i : 300u + f * 50u + (i - 300u);
            if(old_kept[t]) continue;
            old_measured++;
            if(old_count >= 1536u) { memset(old_kept, 0, sizeof old_kept); old_count = 0; }
            old_kept[t] = 1u;
            old_count++;
        }
    }
    printf("  300 texts a frame and 50 new ones, 50 frames: %u measured (each once), %u when the map was emptied at 1536, %u wrong\n", measured,
           old_measured, wrong);
    CHECK(wrong == 0u && measured == 300u - 1u + 50u * 50u);   // (texts[0] at 20 was kept from above)
    // A text past the key's room (1 KB): measured each time, its width right.
    static c8 long_text[2000];
    memset(long_text, 'm', sizeof long_text - 1u);
    long_text[sizeof long_text - 1u] = 0;
    measured = 0;
    const f32 wl = fude_draw_text_width(font, 20.0f, long_text, 10.0f);
    CHECK(fude_draw_text_width(font, 20.0f, long_text, 10.0f) == wl && measured == 2u && wl == (1999.0f * 11.0f + (f32)((1999u * (u32)'m') % 7u)) / 20.0f * 10.0f);
}

// What the canvas's plain texts cost draw.c's layouts, timed (the renderer keeps their lines itself now: render_text.c,
// tests/zoom's test_render_plain_texts). Drawn at one size frame after frame, 60 texts stay kept in the 64 layouts,
// while 100 are each laid out again every frame, each pushing out the one needed next; over a zoom each is laid out
// every frame however many there are, its size on the screen being in the key.
static void test_canvas_texts(void) {
    rde_font* font = (rde_font*)(any)&measured_font;
    static c8 notes[100][160];
    for(u32 i = 0; i < 100u; i++) snprintf(notes[i], sizeof notes[i], "Note %u: a plain text on the canvas, some words long, wrapped to its box at its size (%u)", i, i * 13u);
    f64 ms[3];
    for(u32 run = 0; run < 3u; run++) {
        const u32 n = run == 0u ? 60u : 100u;
        for(u32 i = 0; i < n; i++) fude_draw_text_wrap_lines(font, 20.0f, notes[i], PX, 200.0f);   // (the widths measured once)
        const clock_t c0 = clock();
        for(u32 f = 0; f < 30u; f++) {
            const f32 k = run == 2u ? (f32)pow(1.03, (f64)(f + 1u)) : 1.0f;
            for(u32 i = 0; i < n; i++) fude_draw_text_wrap_lines(font, 20.0f, notes[i], PX * k, 200.0f * k);
        }
        ms[run] = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
    }
    printf("  canvas texts through draw.c's layouts, 30 frames: 60 still %.2f ms, 100 still %.2f ms, 100 over a zoom %.2f ms\n", ms[0], ms[1], ms[2]);
}

int main(void) {
    // Texts of random words: Latin letters, or (one in five) kana, so each wraps its own way.
    static c8 texts[TEXTS][256];
    for(u32 i = 0; i < TEXTS; i++) {
        c8* p = texts[i];
        const b8 kana = i % 5u == 4u;
        const u32 words = 2u + next() % (kana ? 5u : 10u);
        for(u32 w = 0; w < words; w++) {
            const u32 len = 1u + next() % 9u;
            for(u32 k = 0; k < len; k++) {
                if(kana) { memcpy(p, "\xE3\x81\x8B", 3); p += 3; }   // か
                else     { *p++ = (c8)('a' + next() % 26u); }
            }
            if(w + 1u < words) *p++ = ' ';
        }
        *p = 0;
    }
    const f32 widths[WIDTHS] = { 60.0f, 110.0f, 250.0f };
    static spans first[TEXTS][WIDTHS];
    for(u32 i = 0; i < TEXTS; i++) {
        for(u32 k = 0; k < WIDTHS; k++) {
            first[i][k] = layout(NULL, texts[i], widths[k]);   // (each made now: none asked for before)
        }
    }
    // Each line no wider than its width, in order, and together every letter of the text.
    for(u32 i = 0; i < TEXTS; i++) {
        for(u32 k = 0; k < WIDTHS; k++) {
            const spans* s = &first[i][k];
            u32 letters = 0;
            b8 ok = s->n > 0u;
            for(u32 j = 0; j < s->n; j++) {
                ok = ok && s->from[j] < s->to[j] && (j == 0u || s->to[j - 1u] <= s->from[j]) && estimate(texts[i], s->from[j], s->to[j]) <= widths[k] + 1e-3f;
                for(u32 b = s->from[j]; b < s->to[j]; b++) letters += texts[i][b] != ' ' ? 1u : 0u;
            }
            u32 all = 0;
            for(const c8* c = texts[i]; *c != 0; c++) all += *c != ' ' ? 1u : 0u;
            if(!ok || (s->n < LINES && letters != all)) printf("  text %u at %.0f: %u lines, %u of %u letters\n", i, (double)widths[k], s->n, letters, all);
            CHECK(ok && (s->n == LINES || letters == all));
        }
    }
    // Asked again in a random order: let go and made again, or found, each is what it was.
    u32 differ = 0;
    for(u32 r = 0; r < 4000u; r++) {
        const u32 i = next() % TEXTS, k = next() % WIDTHS;
        const spans s = layout(NULL, texts[i], widths[k]);
        differ += same(&s, &first[i][k]) ? 0u : 1u;
    }
    CHECK(differ == 0u);
    // Two texts of one length that break differently, asked for in turn: each its own layout ("aaaa bbbb" / "cccc";
    // "aaaaaaaa" / "bb cc"), never the other's.
    for(u32 r = 0; r < 6u; r++) {
        const spans a = layout(NULL, "aaaa bbbb cccc", 54.0f), b = layout(NULL, "aaaaaaaa bb cc", 54.0f);
        CHECK(a.n == 2u && a.from[0] == 0u && a.to[0] == 9u && a.from[1] == 10u && a.to[1] == 14u);
        CHECK(b.n == 2u && b.from[0] == 0u && b.to[0] == 8u && b.from[1] == 9u && b.to[1] == 14u);
    }
    // Balanced widths (they need a font: any pointer, as the tests' measuring stands in for it): no wider than the
    // width, the same number of lines, and asked again in a random order over more than are kept (32), the same.
    rde_font* font = (rde_font*)&seed;   // (never read)
    static f32 balanced[TEXTS][2];
    for(u32 i = 0; i < TEXTS; i++) {
        for(u32 k = 0; k < 2u; k++) {
            const f32 w = widths[k];
            balanced[i][k] = fude_draw_text_balanced_width(font, 20.0f, texts[i], PX, w);
            CHECK(balanced[i][k] <= w && balanced[i][k] > 0.0f);
            CHECK(fude_draw_text_wrap_lines(font, 20.0f, texts[i], PX, balanced[i][k]) == fude_draw_text_wrap_lines(font, 20.0f, texts[i], PX, w));
        }
    }
    differ = 0;
    for(u32 r = 0; r < 2000u; r++) {
        const u32 i = next() % TEXTS, k = next() % 2u;
        differ += fude_draw_text_balanced_width(font, 20.0f, texts[i], PX, widths[k]) == balanced[i][k] ? 0u : 1u;
    }
    CHECK(differ == 0u);
    // No line is left out, however many: a hundred lines are a hundred (a layout once kept 32), and each is drawn.
    {
        static c8 many[400];
        c8* p = many;
        for(u32 i = 0; i < 100u; i++) { *p++ = (c8)('a' + i % 26u); *p++ = '\n'; }
        *p = 0;
        CHECK(fude_draw_text_wrap_lines(font, 20.0f, many, PX, 300.0f) == 100u);
        drawn = 0;
        CHECK(fude_draw_text_wrap(font, 20.0f, many, 0.0f, 0.0f, PX, 300.0f, 12.0f, (rde_color){ 0, 0, 0, 255 }) == 100u && drawn == 100u);
        u32 from[120], to[120];
        CHECK(fude_draw_text_wrap_spans(font, 20.0f, many, PX, 300.0f, from, to, 120u) == 100u && from[99] == 198u && to[99] == 199u);
    }
    // A line longer than 511 bytes is drawn whole (it was cut there), and a text past 65,535 bytes still wraps (it got
    // no lines at all): 2,000 bytes of one word on a line wide enough, then 70,000 bytes of words.
    {
        static c8 word[2001];
        memset(word, 'x', 2000u);
        word[2000] = 0;
        drawn = 0; drawn_longest = 0;
        CHECK(fude_draw_text_wrap(font, 20.0f, word, 0.0f, 0.0f, PX, 2000.0f * 0.6f * PX + 10.0f, 12.0f, (rde_color){ 0, 0, 0, 255 }) == 1u);
        CHECK(drawn == 1u && drawn_longest == 2000u);
        static c8 huge[70001];
        for(u32 i = 0; i < 70000u; i++) huge[i] = i % 10u == 9u ? ' ' : 'y';
        huge[70000] = 0;
        const u32 n = fude_draw_text_wrap_lines(font, 20.0f, huge, PX, 600.0f);
        CHECK(n >= 700u);   // 7,000 words of 9 letters (54 units each), at most 10 to a 600-unit line
        u32 first_from = 1, first_to = 0;
        CHECK(fude_draw_text_wrap_spans(font, 20.0f, huge, PX, 600.0f, &first_from, &first_to, 1u) == 1u && first_from == 0u && first_to > 0u);
    }
    // A text drawn whole on several lines: the size it fits at is kept; asked again in a random order over more texts
    // than are kept (32), it is the same, and never larger than asked for.
    {
        static f32 whole[TEXTS];
        for(u32 i = 0; i < TEXTS; i++) {
            whole[i] = fude_draw_text_whole(font, 20.0f, texts[i], 0.0f, 0.0f, 3.0f * PX, 60.0f, 2u, (rde_color){ 0, 0, 0, 255 });
            CHECK(whole[i] <= 3.0f * PX && whole[i] > 0.0f);
        }
        differ = 0;
        for(u32 r = 0; r < 1000u; r++) {
            const u32 i = next() % TEXTS;
            differ += fude_draw_text_whole(font, 20.0f, texts[i], 0.0f, 0.0f, 3.0f * PX, 60.0f, 2u, (rde_color){ 0, 0, 0, 255 }) == whole[i] ? 0u : 1u;
        }
        CHECK(differ == 0u);
    }
    test_exact_widths();
    test_canvas_texts();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
