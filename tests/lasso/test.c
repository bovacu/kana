#include "drawing/ink/lasso.h"
#include "drawing/ink/canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static f64 fake_now = 1.0;
f64  rde_engine_get_time_now(void) { return fake_now; }
static u32 drawn = 0;
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)c; if(n) drawn++; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F c, const rde_vec_2F s, const rde_color col) { (void)c; (void)s; (void)col; }

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// Horizontal stroke from (x0, y) to (x0 + 5*(n-1), y).
static void stroke(fude_ink* k, f32 x0, f32 y, u32 n) {
    fude_ink_begin(k, (rde_vec_2F){ x0, y }, true, false);
    for(u32 i = 1; i < n; i++) { k->sample_time += 0.004; fude_ink_extend(k, (rde_vec_2F){ x0 + 5.0f * (f32)i, y }); }
    fude_ink_end(k);
}
// A rectangular loop, drawn as the pen would: down, moves, up.
static void loop(fude_lasso* l, fude_ink* k, f32 x0, f32 y0, f32 x1, f32 y1) {
    fude_lasso_pen_down(l, k, (rde_vec_2F){ x0, y0 }, 1.0f);
    for(f32 x = x0; x <= x1; x += 4) fude_lasso_pen_moved(l, k, (rde_vec_2F){ x, y0 }, 1.0f);
    for(f32 y = y0; y <= y1; y += 4) fude_lasso_pen_moved(l, k, (rde_vec_2F){ x1, y }, 1.0f);
    for(f32 x = x1; x >= x0; x -= 4) fude_lasso_pen_moved(l, k, (rde_vec_2F){ x, y1 }, 1.0f);
    fude_lasso_pen_up(l, k, 1.0f);
}
static rde_vec_2F first_point(const fude_ink* k, u32 s) { return fude_ink_stroke_points(k, fude_ink_stroke_at(k, s))[0].position; }

int main(void) {
    fude_ink k; fude_ink_init(&k);
    fude_lasso l; fude_lasso_init(&l);

    stroke(&k, 0, 0, 11);      // s0: x 0..50,  y 0
    stroke(&k, 0, 100, 11);    // s1: x 0..50,  y 100
    stroke(&k, 40, 50, 11);    // s2: x 40..90, y 50  (straddles x = 60)
    CHECK(fude_ink_stroke_at(&k, 0)->bounds_min.x == -3.0f && fude_ink_stroke_at(&k, 0)->bounds_max.x == 53.0f);

    // Loop around s0 and the left 3 of s2's 11 points: only s0.
    loop(&l, &k, -10, -10, 52, 60);
    CHECK(fude_lasso_count(&l) == 1 && ((u32*)l.selected.memory)[0] == 0);
    // Loop around s0 and most of s2: both. (Deselect first: a press this close to
    // the selection would grab it.)
    fude_lasso_clear(&l, &k);
    loop(&l, &k, -10, -10, 80, 60);
    CHECK(fude_lasso_count(&l) == 2);
    CHECK(!fude_lasso_busy(&l));

    // Tap on s1 picks it alone.
    fude_lasso_pen_down(&l, &k, (rde_vec_2F){ 200, 200 }, 1.0f); fude_lasso_pen_up(&l, &k, 1.0f);   // tap on nothing: deselect
    CHECK(fude_lasso_count(&l) == 0);
    fude_lasso_pen_down(&l, &k, (rde_vec_2F){ 25, 102 }, 1.0f); fude_lasso_pen_up(&l, &k, 1.0f);
    CHECK(fude_lasso_count(&l) == 1 && ((u32*)l.selected.memory)[0] == 1);

    // Drag it: pen down inside the box, move, up. One history step.
    const u32 steps = fude_ink_undo_steps(&k);
    fude_lasso_pen_down(&l, &k, (rde_vec_2F){ 25, 100 }, 1.0f);
    CHECK(l.state == FUDE_LASSO_MOVING);
    fude_lasso_pen_moved(&l, &k, (rde_vec_2F){ 35, 110 }, 1.0f);
    fude_lasso_pen_moved(&l, &k, (rde_vec_2F){ 125, 300 }, 1.0f);
    fude_lasso_pen_up(&l, &k, 1.0f);
    CHECK(fude_ink_undo_steps(&k) == steps + 1);
    CHECK(first_point(&k, 1).x == 100.0f && first_point(&k, 1).y == 300.0f);
    CHECK(fude_ink_stroke_at(&k, 1)->bounds_min.y == 297.0f);
    { rde_vec_2F mn, mx; CHECK(fude_lasso_bounds(&l, &k, &mn, &mx) && mn.x == 97.0f); }
    // Undo puts it back, and the selection box follows; redo moves it again.
    CHECK(fude_ink_undo(&k)); CHECK(first_point(&k, 1).x == 0.0f && first_point(&k, 1).y == 100.0f);
    { rde_vec_2F mn, mx; CHECK(fude_lasso_bounds(&l, &k, &mn, &mx) && mn.x == -3.0f); }
    CHECK(fude_ink_redo(&k)); CHECK(first_point(&k, 1).x == 100.0f);

    // A press that doesn't move leaves no history.
    const u32 steps2 = fude_ink_undo_steps(&k);
    fude_lasso_pen_down(&l, &k, (rde_vec_2F){ 110, 300 }, 1.0f); fude_lasso_pen_up(&l, &k, 1.0f);
    CHECK(fude_ink_undo_steps(&k) == steps2 && fude_ink_redo_steps(&k) == 0 && fude_lasso_count(&l) == 1);

    // Delete: one step, undo brings it back where it was.
    fude_lasso_delete(&l, &k);
    CHECK(fude_ink_alive_strokes(&k) == 2 && fude_lasso_count(&l) == 0 && fude_ink_undo_steps(&k) == steps2 + 1);
    CHECK(fude_ink_undo(&k)); CHECK(fude_ink_alive_strokes(&k) == 3 && first_point(&k, 1).x == 100.0f);

    // Selection follows undo of a WRITE: select s2, undo writes until it dies.
    loop(&l, &k, 30, 40, 100, 60); CHECK(fude_lasso_count(&l) == 1);
    while(fude_ink_stroke_at(&k, 2)->alive) CHECK(fude_ink_undo(&k));
    fude_lasso_sync(&l, &k); CHECK(fude_lasso_count(&l) == 0);
    // ...and a new stroke then truncates the dead tail: invariants hold, no stale ids.
    stroke(&k, 0, 500, 3);
    fude_lasso_sync(&l, &k); CHECK(fude_lasso_count(&l) == 0);
    { u32 next = 0; for(u32 s = 0; s < fude_ink_stroke_count(&k); s++) { CHECK(fude_ink_stroke_at(&k, s)->first_point == next); next += fude_ink_stroke_at(&k, s)->point_count; } CHECK(next == k.points.count); }

    // Undo a move after other edits; then an unrelated move; history consistent.
    loop(&l, &k, -10, 490, 20, 510); CHECK(fude_lasso_count(&l) == 1);
    fude_lasso_pen_down(&l, &k, (rde_vec_2F){ 5, 500 }, 1.0f); fude_lasso_pen_moved(&l, &k, (rde_vec_2F){ 5, 520 }, 1.0f); fude_lasso_pen_up(&l, &k, 1.0f);
    const u32 last = fude_ink_stroke_count(&k) - 1;
    CHECK(first_point(&k, last).y == 520.0f);
    CHECK(fude_ink_undo(&k)); CHECK(first_point(&k, last).y == 500.0f);
    CHECK(fude_ink_redo(&k)); CHECK(first_point(&k, last).y == 520.0f);

    // Clearing deselects via sync; rendering culls off-screen strokes.
    drawn = 0; fude_ink_render(&k, (rde_vec_2F){ 0, 0 }, 1.0f, (rde_vec_2F){ 60, 60 }, 2.0, false);
    CHECK(drawn == 1);   // only s0 (y = 0) is within +-60 of the origin
    fude_ink_clear(&k); fude_lasso_sync(&l, &k); CHECK(fude_lasso_count(&l) == 0);
    drawn = 0; fude_lasso_render_under(&l, &k, (rde_vec_2F){0,0}, 1.0f); CHECK(drawn == 0);

    // ------------------------------------------------------------ clipboard
    {
        fude_ink c; fude_ink_init(&c); fude_lasso m; fude_lasso_init(&m);
        stroke(&c, 0, 0, 5);       // x 0..20
        stroke(&c, 0, 10, 5);      // x 0..20, y 10
        CHECK(!fude_lasso_can_paste(&m));
        fude_lasso_select_all(&m, &c); CHECK(fude_lasso_count(&m) == 2);
        fude_lasso_copy(&m, &c); CHECK(fude_lasso_can_paste(&m)); CHECK(fude_lasso_count(&m) == 2);
        const u32 before = fude_ink_undo_steps(&c);

        // Paste centred at (500, 500): the copy's box centre lands there, selected.
        fude_lasso_paste(&m, &c, (rde_vec_2F){ 500, 500 });
        CHECK(fude_ink_alive_strokes(&c) == 4 && fude_lasso_count(&m) == 2);
        CHECK(((u32*)m.selected.memory)[0] == 2 && ((u32*)m.selected.memory)[1] == 3);
        { rde_vec_2F mn, mx; CHECK(fude_lasso_bounds(&m, &c, &mn, &mx)); CHECK((mn.x + mx.x) * 0.5f == 500.0f && (mn.y + mx.y) * 0.5f == 500.0f); }
        CHECK(first_point(&c, 2).x == 490.0f && first_point(&c, 2).y == 495.0f);   // (0,0) - centre (10,5) + (500,500)
        CHECK(fude_ink_stroke_at(&c, 3)->from_pen);
        CHECK(fude_ink_undo_steps(&c) == before + 1);                            // one step for the whole paste
        CHECK(fude_ink_undo(&c)); CHECK(fude_ink_alive_strokes(&c) == 2);
        fude_lasso_sync(&m, &c); CHECK(fude_lasso_count(&m) == 0);
        CHECK(fude_ink_redo(&c)); CHECK(fude_ink_alive_strokes(&c) == 4);

        // Duplicate: offset down-right on screen (zoom 2 -> 10 canvas units), selected, clipboard untouched.
        fude_lasso_clear(&m, &c); fude_lasso_pen_down(&m, &c, (rde_vec_2F){ 2, 0 }, 1.0f); fude_lasso_pen_up(&m, &c, 1.0f);  // deselect, then tap-select stroke 0
        CHECK(fude_lasso_count(&m) == 1 && ((u32*)m.selected.memory)[0] == 0);
        fude_lasso_duplicate(&m, &c, 2.0f);
        CHECK(fude_ink_alive_strokes(&c) == 5 && fude_lasso_count(&m) == 1 && ((u32*)m.selected.memory)[0] == 4);
        CHECK(first_point(&c, 4).x == 10.0f && first_point(&c, 4).y == -10.0f);
        CHECK(rde_arr_length(&m.clipboard.strokes) == 2);                        // still the two-stroke copy

        // Cut: copies, deletes in one step, deselects; undo brings it back.
        const u32 steps = fude_ink_undo_steps(&c);
        fude_lasso_cut(&m, &c);
        CHECK(fude_ink_alive_strokes(&c) == 4 && fude_lasso_count(&m) == 0 && rde_arr_length(&m.clipboard.strokes) == 1);
        CHECK(fude_ink_undo_steps(&c) == steps + 1);
        CHECK(fude_ink_undo(&c) && fude_ink_alive_strokes(&c) == 5);

        // Paste after undos drops the redo branch (a new edit), history stays consistent.
        CHECK(fude_ink_undo(&c));                                                 // undo the duplicate
        fude_lasso_paste(&m, &c, (rde_vec_2F){ -100, -100 });
        CHECK(fude_ink_redo_steps(&c) == 0 && fude_ink_alive_strokes(&c) == 5);
        { u32 next = 0; for(u32 s2 = 0; s2 < fude_ink_stroke_count(&c); s2++) { CHECK(fude_ink_stroke_at(&c, s2)->first_point == next); next += fude_ink_stroke_at(&c, s2)->point_count; } CHECK(next == c.points.count); }

        fude_lasso_destroy(&m); fude_ink_destroy(&c);
    }

    // ------------------------------------------------------------ long press
    {
        fude_canvas cv; fude_canvas_init(&cv); rde_vec_2F at;
        fake_now = 10.0;
        fude_canvas_finger_down(&cv, 7, (rde_vec_2F){ 100, 50 });
        fake_now = 10.3; CHECK(!fude_canvas_long_press(&cv, &at));
        fude_canvas_finger_moved(&cv, 7, (rde_vec_2F){ 103, 52 });               // a wobble, under the slop
        fake_now = 10.6; CHECK(fude_canvas_long_press(&cv, &at)); CHECK(at.x == 103.0f && at.y == 52.0f);
        CHECK(cv.view.offset.x == 0.0f && cv.view.offset.y == 0.0f);             // the wobble's pan put back
        fake_now = 10.7; CHECK(!fude_canvas_long_press(&cv, &at));               // once per gesture
        fude_canvas_finger_moved(&cv, 7, (rde_vec_2F){ 300, 52 });               // no longer pans
        CHECK(cv.view.offset.x == 0.0f);
        CHECK(fude_canvas_finger_up(&cv, 7) == FUDE_CANVAS_TAP_NONE);

        // Moving past the slop: never a long press.
        fake_now = 20.0; fude_canvas_finger_down(&cv, 8, (rde_vec_2F){ 0, 0 });
        fude_canvas_finger_moved(&cv, 8, (rde_vec_2F){ 40, 0 });
        fake_now = 21.0; CHECK(!fude_canvas_long_press(&cv, &at));
        fude_canvas_finger_up(&cv, 8);

        // Two fingers held: not a long press either.
        fake_now = 30.0; fude_canvas_finger_down(&cv, 9, (rde_vec_2F){ 0, 0 }); fude_canvas_finger_down(&cv, 10, (rde_vec_2F){ 50, 0 });
        fude_canvas_finger_up(&cv, 10);
        fake_now = 31.0; CHECK(!fude_canvas_long_press(&cv, &at));
        fude_canvas_finger_up(&cv, 9);
    }

    fude_lasso_destroy(&l); fude_ink_destroy(&k);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
