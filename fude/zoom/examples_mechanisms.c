// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples_parts.h"

// ===========================================================================
// See examples.h. The examples, the mechanisms': one function for each (fzx_ and its name), a few with
// helpers of their own just before them, in examples.c's order: first the Strandbeest and the engine with theirs (two
// circles' crossing, Jansen's leg, a link, a belt), then the linkages, the gears, what moves, bodies, what goes wrong
// and the machine (examples.h's FUDE_ZOOM_EXAMPLE_ has the list's order).
//
// Part of the examples: examples_parts.h holds what the modules share and says which module holds what. File-local
// names start with fzx_. Every position is in screen points at ×1 from the view's middle (Y up). Main thread only.
// ===========================================================================

// Two circles' crossing nearest _near (_a's, _ra round; _b's, _rb round); _near itself where they do not cross.
RDE_INTERNAL fude_zoom_v2 fzx_crossing(fude_zoom_v2 _a, f64 _ra, fude_zoom_v2 _b, f64 _rb, fude_zoom_v2 _near) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _d = rde_math_hypot_f64(_dx, _dy);
    if(_d <= 0.0 || _d > _ra + _rb || _d < rde_math_abs_f64(_ra - _rb)) {
        return _near;
    }
    const f64 _along = (_ra * _ra - _rb * _rb + _d * _d) / (2.0 * _d), _h = rde_math_sqrt_f64(rde_math_max_f64(_ra * _ra - _along * _along, 0.0));
    const f64 _mx = _a.x + _along * _dx / _d, _my = _a.y + _along * _dy / _d;
    const fude_zoom_v2 _p = { _mx + _h * _dy / _d, _my - _h * _dx / _d }, _q = { _mx - _h * _dy / _d, _my + _h * _dx / _d };
    return rde_math_hypot_f64(_p.x - _near.x, _p.y - _near.y) <= rde_math_hypot_f64(_q.x - _near.x, _q.y - _near.y) ? _p : _q;
}

// Theo Jansen's leg (his "holy numbers": the bars' lengths), its crank 15 round (0, 0), its fixed pivot at (−38, −7.8):
// its joints with its crank turned _turn (radians, counter-clockwise from the right) — followed there from where they
// are with it at 0, a degree at a time, each the crossing nearest where it was (a linkage does not jump to its other
// way of closing). a: the crank's pin; b over and c under the pivot (on the crank by j and k); d the upper triangle's
// outer corner (the pivot's, b's); f the lower triangle's, from d; g its foot.
typedef struct {
    fude_zoom_v2 a, b, c, d, f, g;
} fzx_jansen_leg;

// Returns the leg's joints with its crank turned _turn (see above).
RDE_INTERNAL fzx_jansen_leg fzx_jansen(f64 _turn) {
    const f64 _lb = 41.5, _lc = 39.3, _ld = 40.1, _le = 55.8, _lf = 39.4, _lg = 36.7, _lh = 65.7, _li = 49.0, _lj = 50.0, _lk = 61.9, _lm = 15.0;
    const fude_zoom_v2 _p = { -38.0, -7.8 };
    fzx_jansen_leg _l = { { 15.0, 0.0 }, { -24.01, 31.27 }, { -26.95, -45.52 }, { -74.79, 8.14 }, { -59.23, -28.05 }, { -43.16, -91.76 } };
    const u32 _steps = (u32)rde_math_max_f64(rde_math_ceil_f64(rde_math_abs_f64(_turn) / FZX_DEG), 1.0);
    for(u32 _s = 1; _s <= _steps; _s++) {
        const f64 _t = _turn * (f64)_s / (f64)_steps;
        _l.a = (fude_zoom_v2){ _lm * rde_math_cos_f64(_t), _lm * rde_math_sin_f64(_t) };
        _l.b = fzx_crossing(_l.a, _lj, _p, _lb, _l.b);
        _l.c = fzx_crossing(_l.a, _lk, _p, _lc, _l.c);
        _l.d = fzx_crossing(_l.b, _le, _p, _ld, _l.d);
        _l.f = fzx_crossing(_l.d, _lf, _l.c, _lg, _l.f);
        _l.g = fzx_crossing(_l.f, _lh, _l.c, _li, _l.g);
    }
    return _l;
}

// A link (a bar _thick thick) with its holes on _a and _b.
u32 fzx_bar(fzx* _x, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _thick) {
    const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    return fzx_sized(_x, "link", 0.5 * (_a.x + _b.x), 0.5 * (_a.y + _b.y), rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x) / FZX_DEG, _len + _thick, _thick, "");
}

// The Strandbeest's legs: two of Jansen's legs, four times his size, on one motor's two cranks half a turn apart — the
// left one's at 90°, the right one the left one mirrored (its crank at 270°: the mirror of a leg at −90°) —, each on
// its own fixed pivot; a tracer on each foot.
void fzx_strandbeest(fzx* _x) {
    const f64 _k = 4.0, _thick = 20.0;
    fzx_part(_x, "drive motor", 0, 0, 0, "15 rpm 100");
    for(u32 _side = 0; _side < 2u; _side++) {
        const f64 _m = _side == 0u ? 1.0 : -1.0;
        const fzx_jansen_leg _l = fzx_jansen(_side == 0u ? 90.0 * FZX_DEG : -90.0 * FZX_DEG);
        #define FZX_J(_v) ((fude_zoom_v2){ _m * _k * (_v).x, _k * (_v).y })
        const fude_zoom_v2 _o = { 0.0, 0.0 }, _p = FZX_J(((fude_zoom_v2){ -38.0, -7.8 }));
        const fude_zoom_v2 _a = FZX_J(_l.a), _b = FZX_J(_l.b), _c = FZX_J(_l.c), _d = FZX_J(_l.d), _f = FZX_J(_l.f), _g = FZX_J(_l.g);
        #undef FZX_J
        fzx_part(_x, "fixed pivot", _p.x, _p.y - 10.0, 0, "");   // (its hole 10 over its middle)
        fzx_bar(_x, _o, _a, _thick);   // (its crank)
        fzx_bar(_x, _a, _b, _thick);   // j
        fzx_bar(_x, _a, _c, _thick);   // k
        fzx_bar(_x, _p, _b, _thick);   // b
        fzx_bar(_x, _p, _c, _thick);   // c
        fzx_bar(_x, _b, _d, _thick);   // e
        fzx_bar(_x, _p, _d, _thick);   // d
        fzx_bar(_x, _d, _f, _thick);   // f
        fzx_bar(_x, _c, _f, _thick);   // g
        fzx_bar(_x, _f, _g, _thick);   // h
        fzx_bar(_x, _c, _g, _thick);   // i
        fzx_part(_x, "tracer", _g.x, _g.y, 0, "");
    }
    fzx_word(_x, -360, -440, 16.0, 720.0, FUDE_ZOOM_EXAMPLE_WORD_STRANDBEEST);
    fzx_title(_x, -360, 260, FUDE_ZOOM_EXAMPLE_STRANDBEEST);
}

// A belt (or a chain) whose ends' middles are on _a and _b (40 across).
u32 fzx_belt(fzx* _x, const c8* _id, fude_zoom_v2 _a, fude_zoom_v2 _b, const c8* _text) {
    const f64 _len = rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    return fzx_sized(_x, _id, 0.5 * (_a.x + _b.x), 0.5 * (_a.y + _b.y), rde_math_atan2_f64(_b.y - _a.y, _b.x - _a.x) / FZX_DEG, _len + 40.0, 40.0, _text);
}

// Which way a drive motor turns what is on it (+1: counter-clockwise), and so the crank and the cams below.
#define FZX_ENGINE_TURN 1.0

// A four-stroke engine, its cylinder upright: a starter motor turning a crank (50) and a 30-tooth gear; a rod (170) up
// to the piston, a slider on a rail between the cylinder's walls (its top at the start: the intake stroke's beginning).
// The gear turns a 30-tooth gear either side of it (out of the cylinder's way), each with a small sprocket on it and a
// belt up to a camshaft's sprocket twice its size: the camshafts at half the crank's speed, as a four-stroke's are,
// each with a cam on it opening a valve (a follower toward the cylinder's head) — the left one (intake) as the piston
// first goes down, the right one (exhaust) as it last comes up: its lobe pointing at its valve a quarter turn of the
// crank after the start, and three and a half turns. A tracer on the rod's middle.
void fzx_engine(fzx* _x) {
    const f64 _r = 50.0, _rod = 170.0;
    fzx_part(_x, "drive motor", 0, 0, 0, "30 rpm 200");
    fzx_part(_x, "gear 30T", 0, 0, 0, "");
    const fude_zoom_v2 _o = { 0.0, 0.0 }, _a = { 0.0, _r }, _w = { 0.0, _r + _rod };
    fzx_bar(_x, _o, _a, 18.0);
    fzx_bar(_x, _a, _w, 16.0);
    fzx_part(_x, "tracer", 0.0, _r + 0.5 * _rod, 0, "");
    // (as long as the piston goes, and its own width: what it slides between)
    fzx_sized(_x, "rail", 0, 170, 90, 220, 16, "");
    fzx_sized(_x, "slider", _w.x, _w.y, 0, 100, 60, "");
    for(u32 _side = 0; _side < 2u; _side++) {
        const f64 _m = _side == 0u ? -1.0 : 1.0;
        fzx_sized(_x, "wall", _m * 62.0, 220, 90, 220, 20, "");   // (the cylinder's walls)
        // (the side gear, its sprocket, the belt up to the camshaft's)
        const fude_zoom_v2 _g = { _m * 150.0, 0.0 }, _s = { _m * 150.0, 520.0 };
        fzx_part(_x, "gear 30T", _g.x, _g.y, 0, "");
        fzx_sized(_x, "sprocket", _g.x, _g.y, 0, 60, 60, "");
        fzx_sized(_x, "sprocket", _s.x, _s.y, 0, 120, 120, "");
        fzx_belt(_x, "belt", _g, _s, "");
        // (its valve: a follower from the camshaft toward the head, its roller on the cam)
        const fude_zoom_v2 _seat = { _m * 25.0, 310.0 };
        const f64 _ul = rde_math_hypot_f64(_seat.x - _s.x, _seat.y - _s.y);
        const fude_zoom_v2 _u = { (_seat.x - _s.x) / _ul, (_seat.y - _s.y) / _ul };
        fzx_sized(_x, "follower", _s.x + 107.0 * _u.x, _s.y + 107.0 * _u.y, rde_math_atan2_f64(-_u.y, -_u.x) / FZX_DEG, 160, 30, "");
        // (its cam: the camshaft turns -1/2 as the crank — the side gear turns it the other way —; its lobe on its
        // valve after the crank's quarter turn (intake), or its three and a half (exhaust))
        const f64 _lobe = rde_math_atan2_f64(_u.y, _u.x) / FZX_DEG + FZX_ENGINE_TURN * 0.5 * (_side == 0u ? 90.0 : 630.0);
        fzx_sized(_x, "cam", _s.x + 0.285 * 40.0 * rde_math_cos_f64(_lobe * FZX_DEG), _s.y + 0.285 * 40.0 * rde_math_sin_f64(_lobe * FZX_DEG), _lobe, 80, 80, "");
    }
    fzx_word(_x, -360, -180, 16.0, 720.0, FUDE_ZOOM_EXAMPLE_WORD_ENGINE);
    fzx_title(_x, -360, 680, FUDE_ZOOM_EXAMPLE_ENGINE);
}

// --- mechanisms ----------------------------------------------------------------------------------------------------

// A crank and rocker on a motor turning at _rpm: ground 110 (motor at -55, pivot at 55), crank 40, coupler 120, rocker
// 100 (Grashof: the crank turns round).
RDE_INTERNAL void fzx_crank_rocker(fzx* _x, const c8* _rpm) {
    fzx_part(_x, "drive motor", -55, 0, 0, _rpm);
    fzx_part(_x, "fixed pivot", 55, -10, 0, "");
    fzx_sized(_x, "link", -35, 0, 0, 40 + 24, 24, "");
    fzx_sized(_x, "link", (-15 + 51.43) * 0.5, 99.9 * 0.5, rde_math_atan2_f64(99.9, 66.43) / FZX_DEG, 120 + 24, 24, "");
    fzx_sized(_x, "link", (55 + 51.43) * 0.5, 99.9 * 0.5, rde_math_atan2_f64(99.9, -3.57) / FZX_DEG, 100 + 24, 24, "");
}

// A crank and rocker on a 30 rpm motor.
void fzx_crank(fzx* _x) {
    fzx_crank_rocker(_x, "30 rpm");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_CRANK);
}

// By hand: a crank and rocker on a hand crank (its handle 21 out: the crank; coupler 120, rocker 100, ground 89 — it
// turns round), a tracer on its coupler; beside it a pendulum, its weight to take hold of and drag.
void fzx_by_hand(fzx* _x) {
    fzx_part(_x, "hand crank", -55, 0, 0, "");
    fzx_part(_x, "fixed pivot", 55, -10, 0, "");
    const f64 _cx = 35.22, _cy = 98.02;   // (where the coupler meets the rocker)
    fzx_sized(_x, "link", (-34.0 + _cx) * 0.5, _cy * 0.5, rde_math_atan2_f64(_cy, _cx + 34.0) / FZX_DEG, 120 + 24, 24, "");
    fzx_sized(_x, "link", (55.0 + _cx) * 0.5, _cy * 0.5, rde_math_atan2_f64(_cy, _cx - 55.0) / FZX_DEG, 100 + 24, 24, "");
    fzx_part(_x, "tracer", (-34.0 + _cx) * 0.5, _cy * 0.5, 0, "");
    fzx_word(_x, -140, -70, 16.0, 220.0, FUDE_ZOOM_EXAMPLE_WORD_TURN_CRANK);
    // (the pendulum)
    fzx_part(_x, "fixed pivot", 250, 90, 0, "");
    fzx_sized(_x, "link", 250, 100 - 60, -90, 144, 24, "");
    fzx_part(_x, "weight", 250, 100 - 120, 0, "1 kg");
    fzx_word(_x, 200, -70, 16.0, 180.0, FUDE_ZOOM_EXAMPLE_WORD_DRAG_WEIGHT);
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_BY_HAND);
}

// The crank and rocker (at 20 rpm) with a tracer on its crank's end (a circle) and one on its coupler's middle (its
// coupler curve: the shape a linkage is chosen for).
void fzx_coupler_curve(fzx* _x) {
    fzx_crank_rocker(_x, "20 rpm");
    fzx_part(_x, "tracer", -15, 0, 0, "");
    fzx_part(_x, "tracer", (-15 + 51.43) * 0.5, 99.9 * 0.5, 0, "");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_COUPLER_CURVE);
}

// A drive motor (30 rpm) with a small sprocket on it, a chain round it and a sprocket twice as big — half as fast, the
// same way; under it, the same motor with a crossed belt to a sprocket as big — as fast, the other way.
void fzx_belts(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 0.0 : -220.0;
        fzx_part(_x, "drive motor", -300, _y, 0, "30 rpm");
        fzx_sized(_x, "sprocket", -300, _y, 0, 60, 60, "");
        fzx_sized(_x, "sprocket", -50, _y, 0, _k == 0u ? 120 : 60, _k == 0u ? 120 : 60, "");
        // (its ends' middles 250 apart: the two axles)
        fzx_sized(_x, _k == 0u ? "chain" : "belt", -175, _y, 0, 290, 40, _k == 0u ? "" : "crossed");
    }
    fzx_title(_x, -360, 160, FUDE_ZOOM_EXAMPLE_BELTS);
}

// An eccentric cam on a drive motor (15 rpm: its hole, off its middle, on the shaft), a follower over it pointing down
// — its roller pressed onto the cam, rising and falling its eccentricity either way.
void fzx_cam(fzx* _x) {
    fzx_part(_x, "drive motor", 0, 0, 0, "15 rpm");
    fzx_sized(_x, "cam", 0.285 * 40.0, 0, 0, 80, 80, "");
    fzx_sized(_x, "follower", 0, 92, -90, 120, 30, "");
    fzx_title(_x, -150, 230, FUDE_ZOOM_EXAMPLE_CAM);
}

// Two ratchet wheels on their axles, a weight on a rope tied to each one's right (pulling it clockwise): the left one's
// pawl (along its top, its point on its teeth) holds it; the right one, with none, runs back. Dragged round
// counter-clockwise, the left one clicks past its pawl and stays.
void fzx_ratchet(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _cx = _k == 0u ? -150.0 : 150.0;
        fzx_sized(_x, "ratchet", _cx, 0, 0, 80, 80, "");
        if(_k == 0u) {
            // (its point 33 over the wheel's middle: between its roots and tips)
            fzx_sized(_x, "pawl", _cx + 34.0, 33, 180, 70, 16, "");
        }
        fzx_sized(_x, "rope", _cx + 36.0, -59, 90, 120, 12, "");
        fzx_part(_x, "weight", _cx + 36.0, -130, 0, "1 kg");
    }
    fzx_word(_x, -260, -190, 16.0, 520.0, FUDE_ZOOM_EXAMPLE_WORD_RATCHET_DRAG);
    fzx_title(_x, -260, 120, FUDE_ZOOM_EXAMPLE_RATCHET);
}

// Two weights (1 kg) on springs (stiffness 4: 4 Hz) from fixed pivots: the left one bounces on and on; the right one
// has a damper (3) under it, to a pivot on the ground (in line with its spring: a shock absorber) — it settles in a
// bounce.
void fzx_damper(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _cx = _k == 0u ? -150.0 : 150.0;
        fzx_part(_x, "fixed pivot", _cx, 200, 0, "");
        // (its ends 108 apart: the pivot's hole, the weight's middle)
        fzx_sized(_x, "spring", _cx, 156, 90, 120, 24, "4");
        fzx_part(_x, "weight", _cx, 102, 0, "1 kg");
    }
    // (from the weight's middle down to a pivot's hole under it: 108 long)
    fzx_sized(_x, "damper", 150, 48, 90, 120, 24, "3");
    fzx_part(_x, "fixed pivot", 150, -16, 0, "");
    fzx_title(_x, -230, 300, FUDE_ZOOM_EXAMPLE_DAMPER);
}

// A worm (60 rpm) under a 30-tooth gear on its own axle (its pitch circle on the worm's): the gear a tooth a turn of it
// — 2 rpm —, a 10-tooth gear beside it three times as fast; a weight on a rope from the big gear's left, lowered as it
// turns, never let fall: a gear cannot turn a worm.
void fzx_worm(fzx* _x) {
    fzx_sized(_x, "worm", 0, 0, 0, 120, 40, "60 rpm");
    fzx_part(_x, "gear 30T", 0, 84, 0, "");
    fzx_part(_x, "gear 10T", 100, 84, 0, "");
    fzx_sized(_x, "rope", -72, 15, 90, 140, 12, "");
    fzx_part(_x, "weight", -72, -65, 0, "1 kg");
    fzx_title(_x, -150, 230, FUDE_ZOOM_EXAMPLE_WORM);
}

// A 20-tooth gear on a 20 rpm motor turning a 40-tooth one, and a 10-tooth one on that.
void fzx_gear_train(fzx* _x) {
    fzx_part(_x, "drive motor", -100, 0, 0, "20 rpm");
    fzx_part(_x, "gear 20T", -100, 0, 0, "");
    fzx_part(_x, "gear 40T", 50, 0, 0, "");
    fzx_part(_x, "gear 10T", 50, 125, 0, "");
    fzx_title(_x, -200, 260, FUDE_ZOOM_EXAMPLE_GEAR_TRAIN);
}

// A pendulum let go from 45° (a 1 kg weight on a link from a fixed pivot), and a 2 kg weight on a spring, over a wall.
void fzx_pendulum(fzx* _x) {
    fzx_part(_x, "fixed pivot", -100, 140, 0, "");
    const f64 _a = -45.0 * FZX_DEG;   // (let go from 45°)
    fzx_sized(_x, "link", -100 + rde_math_sin_f64(-_a) * 48, 150 - rde_math_cos_f64(_a) * 48, -45, 120, 24, "");
    fzx_part(_x, "weight", -100 + rde_math_sin_f64(-_a) * 96, 150 - rde_math_cos_f64(_a) * 96, 0, "1 kg");
    fzx_part(_x, "fixed pivot", 150, 140, 0, "");
    fzx_part(_x, "spring", 150, 150 - 45, -90, "");
    fzx_part(_x, "weight", 150, 150 - 90, 0, "2 kg");
    fzx_sized(_x, "wall", 0, -120, 0, 600, 20, "");
    fzx_title(_x, -280, 260, FUDE_ZOOM_EXAMPLE_PENDULUM);
}

// A pulley with a 1 kg weight down one side and a 2 kg crate down the other, over a wall; and a rope pendulum.
void fzx_pulleys(fzx* _x) {
    // A pulley (its groove 31 round), a rope down each side: a 1 kg weight on one, a 2 kg crate on the other; and a
    // rope pendulum from a pivot.
    fzx_sized(_x, "pulley", 0, 100, 0, 80, 80, "");
    fzx_sized(_x, "rope", -31.2, 25, 90, 158, 12, "");
    fzx_sized(_x, "rope", 31.2, 25, 90, 158, 12, "");
    fzx_part(_x, "weight", -31.2, -62, 0, "1 kg");
    fzx_part(_x, "crate", 31.2, -68, 0, "2 kg");
    fzx_sized(_x, "wall", 0, -220, 0, 400, 20, "");
    fzx_part(_x, "fixed pivot", 150, 90, 0, "");
    fzx_sized(_x, "rope", 185, 60, rde_math_atan2_f64(-80.0, 70.0) / FZX_DEG, 106.3 / 0.95, 12, "");
    fzx_part(_x, "weight", 220, 20, 0, "1 kg");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_PULLEYS);
}

// A slider-crank: a 60 rpm motor turning a crank 40 long, a rod 140 to a slider on a rail.
void fzx_piston(fzx* _x) {
    // A motor turning a crank 40 long, a rod 140 to a slider on a rail.
    fzx_part(_x, "drive motor", -120, 0, 0, "60 rpm");
    fzx_sized(_x, "link", -120 + 20, 0, 0, 40 + 24, 24, "");
    fzx_sized(_x, "link", -80 + 70, 0, 0, 140 + 24, 24, "");
    fzx_sized(_x, "rail", 100, 0, 0, 300, 16, "");
    fzx_part(_x, "slider", 60, 0, 0, "");
    fzx_title(_x, -200, 160, FUDE_ZOOM_EXAMPLE_PISTON);
}

// A rack under a 20-tooth gear on a 10 rpm motor.
void fzx_rack(fzx* _x) {
    // A 20-tooth gear (its pitch circle 50 round) on a motor, a rack under it.
    fzx_part(_x, "drive motor", 0, 60, 0, "10 rpm");
    fzx_part(_x, "gear 20T", 0, 60, 0, "");
    fzx_sized(_x, "rack", 0, 60 - 50 - 5, 0, 300, 30, "");
    fzx_title(_x, -160, 220, FUDE_ZOOM_EXAMPLE_RACK);
}

// A 30 rpm motor turning four 20-tooth gears in an L, a rack turned over on the last.
void fzx_gears_rack(fzx* _x) {
    // A motor turning a gear, three more on in an L (each drawn turned anyhow), a rack turned over on the last.
    fzx_part(_x, "drive motor", -150, -50, 0, "30 rpm");
    fzx_part(_x, "gear 20T", -150, -50, 17, "");
    fzx_part(_x, "gear 20T", -50, -50, 63, "");
    fzx_part(_x, "gear 20T", 50, -50, -23, "");
    fzx_part(_x, "gear 20T", 50, 50, 115, "");
    fzx_sized(_x, "rack", 110, 105, 180, 400, 30, "");
    fzx_title(_x, -230, 220, FUDE_ZOOM_EXAMPLE_GEARS_RACK);
}

// Drawings made bodies: a fixed ramp, a wooden triangle, a rubber L, a steel ball and an aluminium bar pinned at one
// end (a pendulum).
void fzx_bodies(fzx* _x) {
    // A ramp drawn as a line (the ground), a pen-drawn triangle of wood, a steel ball, a rubber L, and a bar pinned at
    // one end (a pendulum).
    const fude_zoom_v2 _ramp[4] = { { -330, 130 }, { -40, -60 }, { 330, -60 }, { 330, 10 } };
    const fude_zoom_v2 _tri[3]  = { { -205, 175 }, { -150, 175 }, { -178, 225 } };
    const fude_zoom_v2 _ell[6]  = { { 150, 110 }, { 230, 110 }, { 230, 130 }, { 175, 130 }, { 175, 170 }, { 150, 170 } };
    u32 _bodies[5];
    _bodies[0] = fzx_path(_x, _ramp, 4u, false, 4.0);
    _bodies[1] = fzx_path(_x, _tri, 3u, true, 4.0);
    _bodies[2] = fzx_path(_x, _ell, 6u, true, 4.0);
    const f64 _ball[2] = { 26.0 * _x->p.u, 26.0 * _x->p.u };
    _bodies[3] = fude_zoom_scene_add_shape(_x->p.s, _x->p.frame, (fude_zoom_place){ fzx_at(_x, -265, 200), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, _ball, 2u, _x->p.ink,
                                           (f32)(1.5 * _x->p.u), 0, 0);
    const f64 _bar[3] = { 70.0 * _x->p.u, 8.0 * _x->p.u, 0.0 };
    _bodies[4] = fude_zoom_scene_add_shape(_x->p.s, _x->p.frame, (fude_zoom_place){ fzx_at(_x, 40, 240), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, _bar, 3u, _x->p.ink,
                                           (f32)(1.5 * _x->p.u), 0, 0);
    rde_arr_add(_x->p.born, (any)&_bodies[3]);
    rde_arr_add(_x->p.born, (any)&_bodies[4]);
    // (the ramp's: it is fixed whatever; wood, rubber, steel, aluminium)
    const u32 _material[5] = { 0u, 0u, 3u, 1u, 2u };
    for(u32 _i = 0; _i < 5u; _i++) {
        if(_bodies[_i] == FUDE_ZOOM_NONE) {
            continue;
        }
        const fude_zoom_body_props _bp = { _material[_i], _i == 0u, 0.0, -1.0, -1.0 };
        const u32 _made = fude_zoom_props_add_body(_x->p.s, _bodies[_i], &_bp);
        rde_arr_add(_x->p.born, (any)&_made);
    }
    fzx_part(_x, "pin", -26, 240, 0, "");
    fzx_title(_x, -330, 320, FUDE_ZOOM_EXAMPLE_BODIES);
}

// Four blocks alike — ice, wood, rubber, steel — let go on four wooden ramps alike (31°: ice slides away, steel and
// wood after it more slowly, rubber holds where it is); three balls of rubber, glass, foam dropped on an ice floor
// (rubber bounces back up, glass and foam hardly). Each its material's name by it.
void fzx_materials(fzx* _x) {
    static const u32 _blocks[4] = { 6u, 0u, 3u, 1u };   // (sim/body.h's: ice, wood, rubber, steel)
    // (down the slope)
    const f64 _dx = 250.0 / rde_math_hypot_f64(250.0, 150.0), _dy = -150.0 / rde_math_hypot_f64(250.0, 150.0);
    for(u32 _k = 0; _k < 4u; _k++) {
        const f64 _o = 330.0 - 160.0 * (f64)_k;
        const fude_zoom_v2 _ramp[4] = { { -420, _o + 150 }, { -170, _o }, { 330, _o }, { 330, _o + 40 } };
        fzx_drawn(_x, _ramp, 4u, false, 0u, true);
        // (a block 36 a side on the slope, 40 down from its top, a point off it)
        const fude_zoom_v2 _p0 = { -420.0 + _dx * 40.0 - _dy * 1.0, _o + 150.0 + _dy * 40.0 + _dx * 1.0 };
        const fude_zoom_v2 _sq[4] = { _p0, { _p0.x + _dx * 36.0, _p0.y + _dy * 36.0 }, { _p0.x + _dx * 36.0 - _dy * 36.0, _p0.y + _dy * 36.0 + _dx * 36.0 },
                                     { _p0.x - _dy * 36.0, _p0.y + _dx * 36.0 } };
        fzx_drawn(_x, _sq, 4u, true, _blocks[_k], false);
        fzx_word(_x, -570, _o + 190, 20.0, 150.0, FUDE_ZOOM_EXAMPLE_WORD_MATERIAL + _blocks[_k]);
    }
    static const u32 _balls[3] = { 3u, 5u, 7u };   // (rubber, glass, foam)
    // (a tray: a ball that rolls stays on it)
    const fude_zoom_v2 _floor[4] = { { 450, -250 }, { 450, -330 }, { 900, -330 }, { 900, -250 } };
    fzx_drawn(_x, _floor, 4u, false, 6u, true);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _bx = 520.0 + 160.0 * (f64)_k;
        fzx_ball(_x, _bx, -100, 22, _balls[_k]);
        fzx_word(_x, _bx - 45, -35, 20.0, 150.0, FUDE_ZOOM_EXAMPLE_WORD_MATERIAL + _balls[_k]);
    }
    fzx_title(_x, -560, 600, FUDE_ZOOM_EXAMPLE_MATERIALS);
}

// Weights on links hung from pivots — 1 kg, 50 kg (490 N: its pin takes 980), 500 kg (4.9 kN: its pin shears, the link
// and the weight fall) — and on ropes — 100 kg (a 2 kN rope holds 980 N), 300 kg (2.9 kN: it snaps). A floor below.
void fzx_too_heavy(fzx* _x) {
    static const c8* const _links[3] = { "1 kg", "50 kg", "500 kg" };
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _px = -420.0 + 160.0 * (f64)_k;
        fzx_part(_x, "fixed pivot", _px, 140, 0, "");   // (its hole 10 over its middle)
        fzx_sized(_x, "link", _px, 150 - 48, -90, 120, 24, "");
        fzx_part(_x, "weight", _px, 150 - 96, 0, _links[_k]);
    }
    static const c8* const _ropes[2] = { "100 kg", "300 kg" };
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _px = 80.0 + 180.0 * (f64)_k;
        fzx_part(_x, "fixed pivot", _px, 140, 0, "");
        fzx_sized(_x, "rope", _px, 95, -90, 110.0 / 0.95, 12, "");
        fzx_part(_x, "weight", _px, 40, 0, _ropes[_k]);
    }
    fzx_sized(_x, "wall", -80, -200, 0, 760, 20, "");
    fzx_title(_x, -460, 300, FUDE_ZOOM_EXAMPLE_TOO_HEAVY);
}

// 10 kg at the end of an arm (126 mm) on a drive motor: one of 2 N·m (12.4 needed to lift it out level: it hangs, the
// motor held still — jammed, said); one of 30 N·m swinging it round and round.
void fzx_motor_torque(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _px = _k == 0u ? -250.0 : 200.0;
        fzx_part(_x, "drive motor", _px, 0, 0, _k == 0u ? "30 rpm 2Nm" : "30 rpm 30Nm");
        fzx_sized(_x, "link", _px + 63, 0, 0, 150, 24, "");
        fzx_part(_x, "weight", _px + 126, 0, 0, "10 kg");
    }
    fzx_title(_x, -300, 220, FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE);
}

// Nine gears of four sizes (a 5-point module: pitch radii 25, 50, 75, 100) on a motor along a line, one up and one down
// off it, each turning a rack.
void fzx_gearbox(fzx* _x) {
    fzx_part(_x, "drive motor", -450, 0, 0, "30 rpm");
    static const struct { const c8* id; f64 x, y, turn; } _g[9] = {
        { "gear 10T", -450, 0, 0 }, { "gear 30T", -350, 0, 10 }, { "gear 20T", -225, 0, 33 }, { "gear 40T", -75, 0, 5 }, { "gear 10T", 50, 0, 20 },
        { "gear 30T", 150, 0, 3 }, { "gear 20T", 275, 0, 71 }, { "gear 20T", -75, 150, 44 }, { "gear 10T", 150, -100, 12 },
    };
    for(u32 _i = 0; _i < 9u; _i++) {
        fzx_part(_x, _g[_i].id, _g[_i].x, _g[_i].y, _g[_i].turn, "");
    }
    // (turned over on the top one: its pitch line at its rim)
    fzx_sized(_x, "rack", -15, 150 + 50 + 5, 180, 400, 30, "");
    fzx_sized(_x, "rack", 150, -100 - 25 - 5, 0, 300, 30, "");    // (under the bottom one)
    fzx_title(_x, -500, 330, FUDE_ZOOM_EXAMPLE_GEARBOX);
}

// Six of them on one canvas, every one running at once.
void fzx_machine(fzx* _x) {
    void (*const _each[6])(fzx*) = { fzx_crank, fzx_pulleys, fzx_pendulum, fzx_piston, fzx_gears_rack, fzx_bodies };
    const f64 _at[6][2] = { { -850, 450 }, { 0, 450 }, { 850, 450 }, { -850, -450 }, { 0, -450 }, { 850, -450 } };
    for(u32 _i = 0; _i < 6u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -1150, 850, FUDE_ZOOM_EXAMPLE_MACHINE);
}
