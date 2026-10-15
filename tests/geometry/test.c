// Plane geometry from RDE (plan 3.6): the drawing core and the canvas had written point-in-polygon, the distance from a
// point to a segment and a polygon's area again and again; they now call rde_math's (rde.h:
// rde_math_point_in_polygon_2d, rde_math_point_segment_distance_2d, rde_math_segment_closest_t_2d,
// rde_math_polygon_signed_area_2d, f32 and f64). This suite keeps the copies as they were, word for word, and checks
// that rde_math's answers are theirs, bit for bit: on random polygons, on whole-number grids (so points fall exactly on
// corners, on edges and on flat edges, the cases an even-odd test is touchy about), on loops that cross themselves (the
// lasso's figure eight), and on segments of no length. Where a copy differed it says how, and that no caller
// reaches the difference.
#include "rde.h"
#include "sim/body.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// A small random generator (xorshift64), so a run is the same every time.
static u64 rng = 0x9E3779B97F4A7C15ull;
static u64 next_u64(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static f64 uniform(f64 lo, f64 hi) { return lo + (hi - lo) * (f64)(next_u64() >> 11) / 9007199254740992.0; }
static i32 whole(i32 lo, i32 hi) { return lo + (i32)(next_u64() % (u64)(hi - lo + 1)); }

// --- the copies, as they were -------------------------------------------------------------------------------------

// drawing/ink/lasso.c's fude_lasso_inside (f32).
static b8 old_lasso_inside(const rde_vec_2F* _poly, u32 _n, rde_vec_2F _p) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1; _i < _n; _j = _i++) {
        const rde_vec_2F _a = _poly[_i];
        const rde_vec_2F _b = _poly[_j];
        if((_a.y > _p.y) != (_b.y > _p.y) && _p.x < (_b.x - _a.x) * (_p.y - _a.y) / (_b.y - _a.y) + _a.x) {
            _in = !_in;
        }
    }
    return _in;
}

// zoom/select.c's fude_zoom_inside: an f32 loop, the point and the sums in f64.
static b8 old_select_inside(const rde_vec_2F* _poly, u32 _n, f64 _x, f64 _y) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        const f64 _xi = _poly[_i].x, _yi = _poly[_i].y, _xj = _poly[_j].x, _yj = _poly[_j].y;
        if((_yi > _y) != (_yj > _y) && _x < (_xj - _xi) * (_y - _yi) / (_yj - _yi) + _xi) {
            _in = !_in;
        }
    }
    return _in;
}

// zoom/fill.c's fude_zoom_fill_inside (and the same loop in render.c, symbolart.c, erase.c, instrument.c and
// sim/body.c's fude_sim_polygon_inside), f64.
static b8 old_fill_inside(const rde_vec_2D* _p, u32 _n, rde_vec_2D _q) {
    b8 _in = false;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        if((_p[_i].y > _q.y) != (_p[_j].y > _q.y) && _q.x < (_p[_j].x - _p[_i].x) * (_q.y - _p[_i].y) / (_p[_j].y - _p[_i].y) + _p[_i].x) {
            _in = !_in;
        }
    }
    return _in;
}

// zoom/erase.c's fude_zoom_dist2_point_segment (and fill.c's fude_zoom_fill_seg_dist2, render.c's stroke cover).
static f64 old_dist2(rde_vec_2D _p, rde_vec_2D _a, rde_vec_2D _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y;
    const f64 _len2 = _dx * _dx + _dy * _dy;
    f64 _t = _len2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _len2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
    return _ex * _ex + _ey * _ey;
}

// zoom/shape.c's fude_zoom_shape_dist_segment (and bucket.c's reach, render.c's shape cover, page_shapes.c's node).
static f64 old_dist(rde_vec_2D _p, rde_vec_2D _a, rde_vec_2D _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    return rde_math_hypot_f64(_a.x + _dx * _t - _p.x, _a.y + _dy * _t - _p.y);
}

// zoom/circuit_wires.c's fzc_near_segment's distance (and circuit_build.c's on a wire, at a junction and on a lead,
// page_circuit.c's, page_shapes.c's guide, page_edit.c's line, snap.c's nearest): t held by min and max, not by
// comparisons, which differs only for a t that is not a number (no finite segment and point give one).
static f64 old_dist_minmax(rde_vec_2D _p, rde_vec_2D _a, rde_vec_2D _b, f64* _t_out) {
    const rde_vec_2D _d = { _b.x - _a.x, _b.y - _a.y };
    const f64 _ll = _d.x * _d.x + _d.y * _d.y;
    const f64 _u = _ll > 0.0 ? rde_math_min_f64(rde_math_max_f64(((_p.x - _a.x) * _d.x + (_p.y - _a.y) * _d.y) / _ll, 0.0), 1.0) : 0.0;
    *_t_out = _u;
    return rde_math_hypot_f64(_a.x + _d.x * _u - _p.x, _a.y + _d.y * _u - _p.y);
}

// zoom/mech.c's fzm_on_drawn: an edge from _a (c[j]) to _b (c[i]), t held the other way round (max of min).
static f64 old_dist_mech(rde_vec_2D _l, rde_vec_2D _a, rde_vec_2D _b) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _ll = _dx * _dx + _dy * _dy;
    const f64 _u = _ll > 0.0 ? rde_math_max_f64(0.0, rde_math_min_f64(1.0, ((_l.x - _a.x) * _dx + (_l.y - _a.y) * _dy) / _ll)) : 0.0;
    return rde_math_hypot_f64(_a.x + _dx * _u - _l.x, _a.y + _dy * _u - _l.y);
}

// zoom/fill.c's fude_zoom_fill_reach: the square root of the squared distance (not hypot), less the width there.
static f64 old_fill_reach(rde_vec_2D _p, rde_vec_2D _a, rde_vec_2D _b, f64 _ra, f64 _rb) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _l2 = _dx * _dx + _dy * _dy;
    f64 _t = _l2 > 0.0 ? ((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _l2 : 0.0;
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    const f64 _ex = _a.x + _dx * _t - _p.x, _ey = _a.y + _dy * _t - _p.y;
    return rde_math_sqrt_f64(_ex * _ex + _ey * _ey) - (_ra + (_rb - _ra) * _t);
}

// drawing/ink/ink.c's fude_ink_dist2_to_segment (f32; its error the other way round, squared the same).
static f32 old_ink_dist2(rde_vec_2F _p, rde_vec_2F _a, rde_vec_2F _b) {
    const f32 _abx = _b.x - _a.x;
    const f32 _aby = _b.y - _a.y;
    const f32 _len2 = _abx * _abx + _aby * _aby;
    f32 _t = 0.0f;
    if(_len2 > 0.0f) {
        _t = rde_math_clamp_f32(((_p.x - _a.x) * _abx + (_p.y - _a.y) * _aby) / _len2, 0.0f, 1.0f);
    }
    const f32 _dx = _p.x - (_a.x + _abx * _t);
    const f32 _dy = _p.y - (_a.y + _aby * _t);
    return _dx * _dx + _dy * _dy;
}

// zoom/fill.c's fude_zoom_fill_signed_area and fude_zoom_fill_area (the sum made positive, then halved).
static f64 old_signed_area(const rde_vec_2D* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        _a += _p[_j].x * _p[_i].y - _p[_i].x * _p[_j].y;
    }
    return _a * 0.5;
}
static f64 old_area(const rde_vec_2D* _p, u32 _n) {
    f64 _a = 0.0;
    for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
        _a += _p[_j].x * _p[_i].y - _p[_i].x * _p[_j].y;
    }
    return rde_math_abs_f64(_a) * 0.5;
}

// --- making shapes and points -------------------------------------------------------------------------------------

#define MOST 40u

// A polygon of _n corners: on a whole-number grid of ±_grid when _grid > 0 (corners, edges and flat edges where the
// points fall), else anywhere in ±_size.
static void make_polygon(rde_vec_2D* _p, u32 _n, i32 _grid, f64 _size) {
    for(u32 _i = 0; _i < _n; _i++) {
        _p[_i] = _grid > 0 ? (rde_vec_2D){ (f64)whole(-_grid, _grid), (f64)whole(-_grid, _grid) } : (rde_vec_2D){ uniform(-_size, _size), uniform(-_size, _size) };
    }
}

// A point to test against _p: on the grid (halves too, so edges' middles), at a corner, on an edge, or anywhere.
static rde_vec_2D make_point(const rde_vec_2D* _p, u32 _n, i32 _grid, f64 _size) {
    switch(next_u64() % 4u) {
    case 0:  return _p[next_u64() % _n];   // a corner
    case 1: {                              // on an edge
        const rde_vec_2D _a = _p[next_u64() % _n], _b = _p[next_u64() % _n];
        const f64 _t = _grid > 0 ? 0.5 * (f64)whole(0, 2) : uniform(0.0, 1.0);
        return (rde_vec_2D){ _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };
    }
    case 2:  return _grid > 0 ? (rde_vec_2D){ 0.5 * (f64)whole(-2 * _grid, 2 * _grid), 0.5 * (f64)whole(-2 * _grid, 2 * _grid) } : (rde_vec_2D){ uniform(-_size, _size), uniform(-_size, _size) };
    default: return (rde_vec_2D){ uniform(-1.2 * _size, 1.2 * _size), uniform(-1.2 * _size, 1.2 * _size) };
    }
}

// --- the checks ---------------------------------------------------------------------------------------------------

// Point in polygon, every precision and every caller's way of calling it.
static void test_inside(void) {
    rde_vec_2D p[MOST];
    rde_vec_2F pf[MOST];
    u32 differ_f64 = 0, differ_f32 = 0, differ_mixed = 0, differ_sim = 0, inside = 0, tests = 0;
    for(u32 round = 0; round < 20000u; round++) {
        const u32 n    = 3u + (u32)(next_u64() % (MOST - 2u));
        const i32 grid = round % 2u == 0u ? whole(2, 6) : 0;
        const f64 size = round % 3u == 0u ? 1e5 : 100.0;
        make_polygon(p, n, grid, size);
        for(u32 i = 0; i < n; i++) {
            pf[i] = (rde_vec_2F){ (f32)p[i].x, (f32)p[i].y };
        }
        rde_vec_2D wide[MOST];   // (the f32 loop, exactly, in f64: as select.c's take is handed it)
        for(u32 i = 0; i < n; i++) {
            wide[i] = (rde_vec_2D){ (f64)pf[i].x, (f64)pf[i].y };
        }
        for(u32 k = 0; k < 30u; k++) {
            const rde_vec_2D q  = make_point(p, n, grid, size);
            const rde_vec_2F qf = { (f32)q.x, (f32)q.y };
            const b8 in = rde_math_point_in_polygon_2d_f64(q, p, n);
            differ_f64   += in != old_fill_inside(p, n, q) ? 1u : 0u;
            differ_sim   += fude_sim_polygon_inside(p, n, q) != in ? 1u : 0u;
            differ_f32   += rde_math_point_in_polygon_2d(qf, pf, n) != old_lasso_inside(pf, n, qf) ? 1u : 0u;
            differ_mixed += rde_math_point_in_polygon_2d_f64(q, wide, n) != old_select_inside(pf, n, q.x, q.y) ? 1u : 0u;
            inside       += in ? 1u : 0u;
            tests++;
        }
    }
    CHECK(differ_f64 == 0u && differ_f32 == 0u && differ_mixed == 0u && differ_sim == 0u);
    CHECK(inside > tests / 20u && inside < tests - tests / 20u);   // (both answers well tried)
    // Fewer than 3 corners: never inside (the copies had no such test; no caller hands them fewer than 3: the lasso and
    // the selection take a loop of 3 or more, and every outline, fill ring and instrument has 3 or more).
    const rde_vec_2D two[2] = { { 0.0, 0.0 }, { 10.0, 7.0 } };
    for(u32 i = 0; i < 2000u; i++) {
        const rde_vec_2D q = i % 2u == 0u ? two[i % 4u / 2u] : (rde_vec_2D){ uniform(-1.0, 11.0), uniform(-1.0, 8.0) };
        CHECK(!rde_math_point_in_polygon_2d_f64(q, two, 2u) && !rde_math_point_in_polygon_2d_f64(q, two, 1u) && !rde_math_point_in_polygon_2d_f64(q, two, 0u));
        CHECK(!fude_sim_polygon_inside(two, 2u, q));
    }
}

// The lasso's loops: a figure eight takes what is in each of its lobes, not what lies between them, as the lasso
// always did (even-odd); a square takes its inside, its corners and edges as before.
static void test_lasso_loops(void) {
    // A bow tie: (0,0) → (100,100) → (100,0) → (0,100), its lobes left and right, meeting at (50,50).
    const rde_vec_2F bow[4] = { { 0.0f, 0.0f }, { 100.0f, 100.0f }, { 100.0f, 0.0f }, { 0.0f, 100.0f } };
    CHECK(rde_math_point_in_polygon_2d((rde_vec_2F){ 10.0f, 50.0f }, bow, 4u) && rde_math_point_in_polygon_2d((rde_vec_2F){ 90.0f, 50.0f }, bow, 4u));
    CHECK(!rde_math_point_in_polygon_2d((rde_vec_2F){ 50.0f, 10.0f }, bow, 4u) && !rde_math_point_in_polygon_2d((rde_vec_2F){ 50.0f, 90.0f }, bow, 4u));
    // Every point of a fine grid over it (on its edges and its crossing too) answered as the lasso's own loop did.
    const rde_vec_2F square[4] = { { -3.0f, -3.0f }, { 3.0f, -3.0f }, { 3.0f, 3.0f }, { -3.0f, 3.0f } };
    u32 differ = 0;
    for(i32 y = -8; y <= 108; y++) {
        for(i32 x = -8; x <= 108; x++) {
            const rde_vec_2F q = { (f32)x, (f32)y };
            differ += rde_math_point_in_polygon_2d(q, bow, 4u) != old_lasso_inside(bow, 4u, q) ? 1u : 0u;
            const rde_vec_2F s = { (f32)x * 0.125f - 6.0f, (f32)y * 0.125f - 6.0f };
            differ += rde_math_point_in_polygon_2d(s, square, 4u) != old_lasso_inside(square, 4u, s) ? 1u : 0u;
        }
    }
    CHECK(differ == 0u);
}

// The distance from a point to a segment, squared and not, and where along it, in every form the copies had.
static void test_segment_distance(void) {
    u32 differ = 0, zero_length = 0;
    for(u32 round = 0; round < 200000u; round++) {
        const i32 grid = round % 2u == 0u ? whole(1, 8) : 0;
        const f64 size = round % 5u == 0u ? 1e6 : 50.0;
        rde_vec_2D s[3];
        make_polygon(s, 3u, grid, size);
        if(round % 7u == 0u) {
            s[1] = s[0];   // (a segment of no length: measured to its start)
        }
        const rde_vec_2D p = s[2], a = s[0], b = s[1];
        zero_length += a.x == b.x && a.y == b.y ? 1u : 0u;
        const f64 d2 = rde_math_point_segment_distance_sq_2d_f64(p, a, b);
        const f64 d  = rde_math_point_segment_distance_2d_f64(p, a, b);
        const f64 t  = rde_math_segment_closest_t_2d_f64(p, a, b);
        f64 t_old;
        const f64 d_minmax = old_dist_minmax(p, a, b, &t_old);
        differ += d2 != old_dist2(p, a, b) ? 1u : 0u;
        differ += d != old_dist(p, a, b) ? 1u : 0u;
        differ += d != d_minmax || t != t_old ? 1u : 0u;
        differ += d != old_dist_mech(p, a, b) ? 1u : 0u;
        const f64 ra = uniform(0.0, 3.0), rb = uniform(0.0, 3.0);
        differ += rde_math_sqrt_f64(d2) - (ra + (rb - ra) * t) != old_fill_reach(p, a, b, ra, rb) ? 1u : 0u;
        const rde_vec_2F pf = { (f32)p.x, (f32)p.y }, af = { (f32)a.x, (f32)a.y }, bf = { (f32)b.x, (f32)b.y };
        differ += rde_math_point_segment_distance_sq_2d(pf, af, bf) != old_ink_dist2(pf, af, bf) ? 1u : 0u;
    }
    CHECK(differ == 0u && zero_length > 1000u);
    // Along a segment the nearest point's t is the foot's, past either end its end's; the junctions' test
    // (page_circuit.c) reads an unheld t only between 0.02 and 0.98, where holding it changes nothing.
    CHECK(rde_math_segment_closest_t_2d_f64((rde_vec_2D){ 5.0, 3.0 }, (rde_vec_2D){ 0.0, 0.0 }, (rde_vec_2D){ 10.0, 0.0 }) == 0.5);
    CHECK(rde_math_segment_closest_t_2d_f64((rde_vec_2D){ -5.0, 3.0 }, (rde_vec_2D){ 0.0, 0.0 }, (rde_vec_2D){ 10.0, 0.0 }) == 0.0);
    CHECK(rde_math_segment_closest_t_2d_f64((rde_vec_2D){ 15.0, 3.0 }, (rde_vec_2D){ 0.0, 0.0 }, (rde_vec_2D){ 10.0, 0.0 }) == 1.0);
    CHECK(rde_math_point_segment_distance_2d_f64((rde_vec_2D){ 3.0, 4.0 }, (rde_vec_2D){ 0.0, 0.0 }, (rde_vec_2D){ 0.0, 0.0 }) == 5.0);
}

// A polygon's signed area, as the fills worked it out (from the closing edge round, halved at the end); rde_math's is
// 0 for fewer than 3 corners, where no fill asks. (cut.h's fude_zoom_cut_area sums from the first edge round: kept as
// it is, since the order of the sum rounds a flat loop's sign.)
static void test_area(void) {
    rde_vec_2D p[MOST];
    u32 differ = 0;
    for(u32 round = 0; round < 50000u; round++) {
        const u32 n = 3u + (u32)(next_u64() % (MOST - 2u));
        make_polygon(p, n, round % 2u == 0u ? whole(2, 9) : 0, round % 3u == 0u ? 1e4 : 10.0);
        const f64 a = rde_math_polygon_signed_area_2d_f64(p, n);
        differ += a != old_signed_area(p, n) || rde_math_abs_f64(a) != old_area(p, n) ? 1u : 0u;
    }
    CHECK(differ == 0u);
    const rde_vec_2D square[4] = { { 0.0, 0.0 }, { 2.0, 0.0 }, { 2.0, 2.0 }, { 0.0, 2.0 } };
    CHECK(rde_math_polygon_signed_area_2d_f64(square, 4u) == 4.0 && rde_math_polygon_signed_area_2d_f64(square, 2u) == 0.0);
}

int main(void) {
    test_inside();
    test_lasso_loops();
    test_segment_distance();
    test_area();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
