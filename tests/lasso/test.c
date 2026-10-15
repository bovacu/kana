#include "drawing/ink/lasso.h"
#include "drawing/ink/canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static f64 fake_now = 1.0;
f64  rde_engine_get_time_now(void) { return fake_now; }
static u32 drawn = 0;
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)c; if(n) drawn++; }
// The circles drawn (the ink's sample dots among them): how many, and where the first 256 are.
static u32 dots = 0;
static rde_vec_2F dot_at[256];
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)r; (void)s; (void)c; (void)sh; if(dots < 256u) dot_at[dots] = p; dots++; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F c, const rde_vec_2F s, const rde_color col) { (void)c; (void)s; (void)col; }

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// Horizontal stroke from (x0, y) to (x0 + 5*(n-1), y).
static void stroke(fude_ink* k, f32 x0, f32 y, u32 n) {
    fude_ink_begin(k, (rde_vec_2F){ x0, y }, true);
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
// A page turned under the ink (fude_ink_remap's _map): every point 1000 to the right.
static void shift_right(any user, u32 stroke, fude_ink_point* p) { (void)user; (void)stroke; p->position.x += 1000.0f; }

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

    // A page turned after an erase: undoing the erase brings the stroke back where the page now is.
    { fude_ink t; fude_ink_init(&t);
      stroke(&t, 0, 0, 11); stroke(&t, 0, 100, 11);
      fude_ink_erase_at(&t, (rde_vec_2F){ 25.0f, 100.0f }, 2.0f); fude_ink_erase_end(&t);
      CHECK(fude_ink_alive_strokes(&t) == 1);
      fude_ink_remap(&t, shift_right, NULL);
      CHECK(fude_ink_undo(&t) && fude_ink_alive_strokes(&t) == 2);
      CHECK(first_point(&t, 0).x == 1000.0f && first_point(&t, 1).x == 1000.0f && fude_ink_stroke_at(&t, 1)->bounds_min.x > 990.0f);
      fude_ink_destroy(&t); }

    // A tap on the selection keeps what was undone; a drag that moves drops it, and is still one undoable step.
    { fude_ink t; fude_ink_init(&t); fude_lasso s; fude_lasso_init(&s);
      stroke(&t, 0, 0, 11); stroke(&t, 0, 100, 11); stroke(&t, 0, 200, 11);
      fude_ink_erase_at(&t, (rde_vec_2F){ 25.0f, 100.0f }, 2.0f); fude_ink_erase_end(&t);
      CHECK(fude_ink_undo(&t) && fude_ink_undo(&t));                             // the erase and s2: a branch with targets
      CHECK(fude_ink_redo_steps(&t) == 2 && fude_ink_alive_strokes(&t) == 2);
      fude_lasso_pen_down(&s, &t, (rde_vec_2F){ 25, 2 }, 1.0f); fude_lasso_pen_up(&s, &t, 1.0f);   // tap-select s0
      CHECK(fude_lasso_count(&s) == 1 && ((u32*)s.selected.memory)[0] == 0);
      fude_lasso_pen_down(&s, &t, (rde_vec_2F){ 25, 0 }, 1.0f);
      CHECK(s.state == FUDE_LASSO_MOVING && fude_ink_can_redo(&t));             // pressed, not moved: redo still offered
      fude_lasso_pen_up(&s, &t, 1.0f);
      CHECK(fude_ink_redo_steps(&t) == 2 && fude_ink_undo_steps(&t) == 2 && rde_arr_length(&t.targets) == 1);
      CHECK(fude_ink_redo(&t) && fude_ink_redo(&t) && fude_ink_alive_strokes(&t) == 2 && !fude_ink_stroke_at(&t, 1)->alive);
      CHECK(fude_ink_undo(&t) && fude_ink_undo(&t) && fude_ink_redo_steps(&t) == 2);
      // Now a drag: redo greys out once it has moved, and the branch goes as it ends.
      fude_lasso_pen_down(&s, &t, (rde_vec_2F){ 25, 0 }, 1.0f);
      fude_lasso_pen_moved(&s, &t, (rde_vec_2F){ 25, 50 }, 1.0f);
      CHECK(!fude_ink_can_redo(&t));
      fude_lasso_pen_up(&s, &t, 1.0f);
      CHECK(fude_ink_redo_steps(&t) == 0 && fude_ink_undo_steps(&t) == 3 && fude_ink_stroke_count(&t) == 2);
      CHECK(rde_arr_length(&t.targets) == 1 && ((u32*)t.targets.memory)[0] == 0);   // the erase's target gone, the move's kept
      CHECK(first_point(&t, 0).y == 50.0f && fude_ink_alive_strokes(&t) == 2);
      CHECK(fude_ink_undo(&t) && first_point(&t, 0).y == 0.0f);
      CHECK(fude_ink_redo(&t) && first_point(&t, 0).y == 50.0f);
      { u32 next = 0; for(u32 s2 = 0; s2 < fude_ink_stroke_count(&t); s2++) { CHECK(fude_ink_stroke_at(&t, s2)->first_point == next); next += fude_ink_stroke_at(&t, s2)->point_count; } CHECK(next == t.points.count); }
      fude_lasso_destroy(&s); fude_ink_destroy(&t); }

    // Another canvas: the page empties, and every setting is kept (the marker's and the eraser's were once lost).
    { fude_ink t; fude_ink_init(&t);
      t.marker_color = (rde_color){ 10, 20, 30, 90 }; t.marker_radius = 7.5f; t.eraser_radius = 33.0f;
      t.color = (rde_color){ 1, 2, 3, 255 }; t.constant_radius = 2.25f; t.width_mode = FUDE_INK_WIDTH_MODE_PRESSURE; t.pressure_live = true;
      stroke(&t, 0, 0, 11);
      fude_ink_clear_page(&t);
      CHECK(fude_ink_stroke_count(&t) == 0u && fude_ink_undo_steps(&t) == 0u);
      CHECK(t.marker_color.r == 10 && t.marker_color.a == 90 && t.marker_radius == 7.5f && t.eraser_radius == 33.0f);
      CHECK(t.color.b == 3 && t.constant_radius == 2.25f && t.width_mode == FUDE_INK_WIDTH_MODE_PRESSURE && t.pressure_live);
      stroke(&t, 0, 0, 11); CHECK(fude_ink_alive_strokes(&t) == 1u);   // and it still writes
      fude_ink_destroy(&t); }

    // A loop that crosses itself (a figure eight: its lobes left and right, crossing at (50, 50)) takes what is in each
    // lobe and not what lies between them: the even-odd rule, which the lasso's inside test has always kept (it is
    // rde_math's point in polygon now, plan 3.6).
    { fude_ink e; fude_ink_init(&e); fude_lasso b; fude_lasso_init(&b);
      stroke(&e, 5, 50, 3);      // s0: x 5..15, y 50: in the left lobe
      stroke(&e, 85, 50, 3);     // s1: x 85..95, y 50: in the right lobe
      stroke(&e, 45, 10, 3);     // s2: x 45..55, y 10: under the crossing, in neither
      const rde_vec_2F corners[5] = { { 0, 0 }, { 100, 100 }, { 100, 0 }, { 0, 100 }, { 0, 0 } };
      fude_lasso_pen_down(&b, &e, corners[0], 1.0f);
      for(u32 c = 0; c + 1u < 5u; c++) {
          for(u32 s = 1; s <= 25u; s++) {
              const f32 t = (f32)s / 25.0f;
              fude_lasso_pen_moved(&b, &e, (rde_vec_2F){ corners[c].x + (corners[c + 1u].x - corners[c].x) * t, corners[c].y + (corners[c + 1u].y - corners[c].y) * t }, 1.0f);
          }
      }
      fude_lasso_pen_up(&b, &e, 1.0f);
      b8 left = false, right = false, between = false;
      for(u32 i = 0; i < fude_lasso_count(&b); i++) {
          const u32 id = ((const u32*)b.selected.memory)[i];
          left = left || id == 0u; right = right || id == 1u; between = between || id == 2u;
      }
      CHECK(fude_lasso_count(&b) == 2 && left && right && !between);
      fude_lasso_destroy(&b); fude_ink_destroy(&e); }

    fude_lasso_destroy(&l); fude_ink_destroy(&k);
    // The sample dots (a developer's diagnostic): one where each point of each stroke is, the marker's too, though the
    // marker draws fewer of its points than it has. A pen's stroke far off is drawn a frame before, so what the drawing
    // left in its scratch array is not where any of the marker's points are.
    { fude_ink m; fude_ink_init(&m);
      stroke(&m, 10000, 0, 30);
      m.marking = true;
      fude_ink_begin(&m, (rde_vec_2F){ 0, 0 }, true);
      for(u32 i = 1; i < 30u; i++) { m.sample_time += 0.004; fude_ink_extend(&m, (rde_vec_2F){ 2.0f * (f32)i, 0 }); }
      fude_ink_end(&m);
      const fude_ink_stroke* mark = fude_ink_stroke_at(&m, 1);
      CHECK(mark->marker && mark->point_count == 30u);
      const rde_vec_2F half = { 1e6f, 1e6f };
      fude_ink_render(&m, (rde_vec_2F){ 0, 0 }, 1.0f, half, 1.0, false);
      dots = 0;
      fude_ink_render(&m, (rde_vec_2F){ 0, 0 }, 1.0f, half, 1.0, true);
      CHECK(dots == 60u);
      u32 wrong = 0;
      for(u32 i = 0; i < 30u && i < dots; i++) wrong += dot_at[i].x == 2.0f * (f32)i && dot_at[i].y == 0.0f ? 0u : 1u;            // the marker's, first
      for(u32 i = 0; i < 30u && 30u + i < dots; i++) wrong += dot_at[30u + i].x == 10000.0f + 5.0f * (f32)i ? 0u : 1u;            // then the pen's
      CHECK(wrong == 0u);
      fude_ink_destroy(&m); }
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
