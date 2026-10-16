// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/inktest.h"

// ===========================================================================
// See inktest.h. The code is the developer's --gate as look.c had it, moved here unchanged (the zoom suite checks the
// ink is the same, stroke for stroke and byte for byte).
// ===========================================================================

// The ink's depths: the camera's own and three more, each FUDE_ZOOM_INKTEST_DEEPER times inside the last (the gate's
// four levels, docs/infinite_canvas_design.md §10). The step is a frame's (FUDE_ZOOM_CHILD_SCALE), so each depth's ink
// lies in a frame of its own and the drawing measures the frames as well as the strokes.
#define FUDE_ZOOM_INKTEST_DEPTHS 4u
#define FUDE_ZOOM_INKTEST_DEEPER 1024.0

// The field the strokes are spread over at each depth, in points on the screen round the camera's point: a little
// more than a tablet's screen, so it starts full and a pan finds more ink; each stroke's petals reach
// FUDE_ZOOM_INKTEST_REACH points from its middle, and it is FUDE_ZOOM_INKTEST_WIDTH points wide, as handwriting is.
#define FUDE_ZOOM_INKTEST_FIELD_W 2000.0
#define FUDE_ZOOM_INKTEST_FIELD_H 1500.0
#define FUDE_ZOOM_INKTEST_REACH   40.0
#define FUDE_ZOOM_INKTEST_WIDTH   2.0

// The seed of the ink's numbers, always the same (inktest.h: the same strokes on every run).
#define FUDE_ZOOM_INKTEST_SEED 7u

const rde_color FUDE_ZOOM_INKTEST_COLORS[FUDE_ZOOM_INKTEST_COLOR_COUNT] = {
    { 231, 76, 60, 255 }, { 241, 196, 15, 255 }, { 46, 204, 113, 255 }, { 52, 152, 219, 255 }, { 155, 89, 182, 255 }, { 230, 126, 34, 255 },
};

// See the header. The points are a pen's: kept in the quantum for the camera's zoom (as the pen's are, about a
// sixteenth of a point), the first at 0, 0; the pressure (of 1023: the share of the half-width) swinging between 400
// and 1000 three times round, so the width varies as a hand's does; a point every 4 ms, as a pen sampled at 250 Hz
// gives them.
void fude_zoom_inktest_rose(fude_zoom_scene* _s, fude_zoom_v2 _c, f64 _r, f64 _phase, u32 _petals, rde_color _color, f64 _width) {
    const u32 _n = FUDE_ZOOM_INKTEST_STROKE;
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = rde_math_ldexp_f64(1.0, _q);
    fude_zoom_qpoint _pts[FUDE_ZOOM_INKTEST_STROKE];   // (a stroke's points, a fixed number: on the stack)
    fude_zoom_v2     _first = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)(_n - 1u);
        const f64 _k = _r * (0.55 + 0.45 * rde_math_cos_f64((f64)_petals * _a + _phase));
        const fude_zoom_v2 _p = { _c.x + rde_math_cos_f64(_a) * _k, _c.y + rde_math_sin_f64(_a) * _k };
        if(_i == 0) {
            _first = _p;
        }
        _pts[_i] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_p.x - _first.x) / _g), (i32)rde_math_round_to_i64_f64((_p.y - _first.y) / _g), (u16)(700 + 300 * rde_math_sin_f64(_a * 3.0)), _i * 4u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_s, _s->camera.frame, _first, _q, _pts, _n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME, _color,
                                              (f32)(_width / _s->camera.z), FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}

// See the header. At each depth, its share of the points (what is left over the depths left, so the four add up to
// _points), in strokes spread over the field round the camera's point at that depth; then 1024× in to the next. Two
// numbers of the generator place a stroke, and the second also gives its petals' turn, how many petals it has (3 to
// 7) and its colour. At the end, 1024³ back out: where the camera began.
void fude_zoom_inktest_fill(fude_zoom_scene* _s, u64 _points, fude_zoom_v2 _half) {
    u64 _made = 0;
    u32 _seed = FUDE_ZOOM_INKTEST_SEED;
    for(u32 _l = 0; _l < FUDE_ZOOM_INKTEST_DEPTHS && _made < _points; _l++) {
        const fude_zoom_v2 _c = _s->camera.at;
        const f64 _u = 1.0 / _s->camera.z;
        const u64 _share = (_points - _made) / (FUDE_ZOOM_INKTEST_DEPTHS - _l);
        for(u64 _p = 0; _p < _share; _p += FUDE_ZOOM_INKTEST_STROKE) {
            _seed = _seed * 1664525u + 1013904223u;
            const f64 _x = _c.x + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * FUDE_ZOOM_INKTEST_FIELD_W * _u;
            _seed = _seed * 1664525u + 1013904223u;
            const f64 _y = _c.y + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * FUDE_ZOOM_INKTEST_FIELD_H * _u;
            fude_zoom_inktest_rose(_s, (fude_zoom_v2){ _x, _y }, FUDE_ZOOM_INKTEST_REACH * _u, (f64)(_seed & 255u), 3u + (_seed % 5u),
                                   FUDE_ZOOM_INKTEST_COLORS[_seed % FUDE_ZOOM_INKTEST_COLOR_COUNT], FUDE_ZOOM_INKTEST_WIDTH);
        }
        _made += _share;
        if(_l + 1u < FUDE_ZOOM_INKTEST_DEPTHS) {
            fude_zoom_camera_zoom_by(_s, FUDE_ZOOM_INKTEST_DEEPER, _half);
        }
    }
    fude_zoom_camera_zoom_by(_s, 1.0 / FUDE_ZOOM_INKTEST_DEEPER / FUDE_ZOOM_INKTEST_DEEPER / FUDE_ZOOM_INKTEST_DEEPER, _half);
}

// See the header: a canvas as the side panel's + Canvas makes one, at the top, under the name given.
u32 fude_zoom_inktest_canvas(fude_notes* _notes, const c8* _name) {
    const u32 _id = fude_notes_add(_notes, FUDE_NOTE_CANVAS, 0u, _name);
    fude_notes_open(_notes, _id);
    return _id;
}
