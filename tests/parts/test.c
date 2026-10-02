#include "screens/browse.h"
#include "base/theme.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static f64 g_now = 1000.0; static char g_status[256];
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)c; for(u32 i = 0; i < n; i++) { volatile f32 x = p[i].x + r[i]; (void)x; } }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F a, const rde_vec_2F b, f32 r, u32 p, const rde_color c, rde_shader* sh) { (void)a; (void)b; (void)r; (void)p; (void)c; (void)sh; }
void rde_rendering_2d_draw_circle_border(const rde_vec_2F p, f32 r, f32 t, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)t; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)s; (void)r; (void)c; if(p.y > 300.0f) snprintf(g_status, sizeof g_status, "%s", t); }
void rde_rendering_begin_clipping_rect(const rde_window* w, rde_vec_2I p, rde_vec_2UI s) { (void)w; (void)p; (void)s; }
void rde_rendering_end_clipping_rect(void) {}
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return g_now; }
static const f32 TOP = 566.5f - 184.0f;
static void frame(kana_browse* b) { g_now += 0.016; kana_browse_update(b, 0.016f); kana_browse_render(b, NULL, NULL, 32.0f, TOP, -10000.0f); }
static b8 listed(kana_browse* b, u32 cp) { const u32* l = kana_browse_list(b); for(u32 i = 0; i < kana_browse_count(b); i++) { kana_kanji_info in; kana_kanji_at(b->db, l[i], &in); if(in.codepoint == cp) return true; } return false; }
static i32 part_index(kana_browse* b, u32 cp) { const kana_browse_part* p = b->parts.memory; for(u32 i = 0; i < b->parts.count; i++) if(p[i].codepoint == cp) return (i32)i; return -1; }
// Taps a part on the panel, scrolling the panel until it shows.
static b8 tap_part(kana_browse* b, u32 cp) {
    const i32 want = part_index(b, cp); if(want < 0) return false;
    b->parts_scroller.offset = 0.0f;
    for(int tries = 0; tries < 60; tries++) {
        frame(b);
        const kana_browse_part_hit* h = b->part_hits.memory;
        for(u32 i = 0; i < b->part_hits.count; i++) if((i32)h[i].part == want) {
            const rde_vec_2F at = { h[i].x + h[i].size * 0.5f, b->panel_max.y - (h[i].y + h[i].size * 0.5f - b->parts_scroller.offset) };
            kana_browse_pointer_down(b, at, false, g_now); kana_browse_pointer_up(b, g_now + 0.05); frame(b); return true;
        }
        b->parts_scroller.offset += 150.0f;
    }
    return false;
}

int main(int argc, char** argv) {
    kana_kanji_db db; CHECK(kana_kanji_load(&db, argc > 1 ? argv[1] : "characters.kana"));
    kana_browse b; kana_browse_init(&b, &db);
    CHECK(kana_browse_parts_available(&b));
    const kana_browse_part* p = b.parts.memory;
    printf("%u parts offered; first: U+%04X (%u strokes, %u uses), last %u strokes\n", b.parts.count, p[0].codepoint, p[0].strokes, p[0].uses, p[b.parts.count - 1].strokes);
    CHECK(b.parts.count > 300 && p[0].strokes <= p[b.parts.count - 1].strokes);
    CHECK(part_index(&b, 0x6728) >= 0 && part_index(&b, 0x4EBB) >= 0 && part_index(&b, 0x53E3) >= 0);   // 木 亻 口

    kana_browse_open(&b);
    kana_browse_set_picking(&b, true); frame(&b);
    const u32 all = kana_browse_count(&b);
    CHECK(b.picking && b.part_hits.count > 0 && b.panel_max.y > b.panel_min.y);

    CHECK(tap_part(&b, 0x6728));                                         // 木
    printf("with 木: %u of %u (status '%s')\n", kana_browse_count(&b), all, g_status);
    CHECK(b.picked_count == 1 && kana_browse_count(&b) < all && listed(&b, 0x6728) && listed(&b, 0x6797) && listed(&b, 0x4F11) && !listed(&b, 0x65E5));
    const kana_browse_part* pp = b.parts.memory;
    CHECK(pp[part_index(&b, 0x4EBB)].usable);                            // 亻: 休 has both

    CHECK(tap_part(&b, 0x4EBB));                                         // + 亻
    printf("with 木 and 亻: %u\n", kana_browse_count(&b));
    CHECK(b.picked_count == 2 && listed(&b, 0x4F11) && !listed(&b, 0x6797));

    // A dimmed part cannot be picked.
    i32 dim = -1; pp = b.parts.memory;
    for(u32 i = 0; i < b.parts.count && dim < 0; i++) if(!pp[i].usable) dim = (i32)i;
    CHECK(dim >= 0);
    if(dim >= 0) { const u32 before = b.picked_count; tap_part(&b, pp[dim].codepoint); CHECK(b.picked_count == before); }

    CHECK(tap_part(&b, 0x6728)); CHECK(b.picked_count == 1);           // tap again: unpicked
    kana_browse_clear_parts(&b); frame(&b); CHECK(b.picked_count == 0 && kana_browse_count(&b) == all);
    kana_browse_set_drawing(&b, true); CHECK(!b.picking && b.drawing);
    kana_browse_set_picking(&b, true); CHECK(b.picking && !b.drawing);
    for(int i = 0; i < 100; i++) frame(&b);

    // Select mode (select.h): ticks in the order made, unticking keeps the rest's order, All adds
    // the list after them, None clears; the grid draws its ticks.
    {
        kana_selection sel; kana_selection_init(&sel, db.count);
        CHECK(kana_selection_count(&sel) == 0 && !kana_selection_has(&sel, 5));
        kana_selection_toggle(&sel, 7); kana_selection_toggle(&sel, 3); kana_selection_toggle(&sel, 9);
        CHECK(kana_selection_count(&sel) == 3 && kana_selection_records(&sel)[0] == 7 && kana_selection_records(&sel)[2] == 9);
        kana_selection_toggle(&sel, 3);
        CHECK(kana_selection_count(&sel) == 2 && !kana_selection_has(&sel, 3) && kana_selection_records(&sel)[1] == 9);
        const u32 more[4] = { 9, 11, 7, 12 };
        kana_selection_add(&sel, more, 4);
        CHECK(kana_selection_count(&sel) == 4 && kana_selection_records(&sel)[2] == 11 && kana_selection_records(&sel)[3] == 12);
        kana_selection_toggle(&sel, db.count + 5);   // not a record: nothing
        CHECK(kana_selection_count(&sel) == 4);
        b.selection = &sel; sel.active = true;
        kana_browse_set_filter(&b, KANA_FILTER_ALL); frame(&b);
        kana_selection_add(&sel, kana_browse_list(&b), kana_browse_count(&b));
        CHECK(kana_selection_count(&sel) >= kana_browse_count(&b)); frame(&b);
        kana_selection_clear(&sel);
        CHECK(kana_selection_count(&sel) == 0 && !kana_selection_has(&sel, 7));
        b.selection = NULL; kana_selection_destroy(&sel);
    }

    kana_browse_destroy(&b); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
