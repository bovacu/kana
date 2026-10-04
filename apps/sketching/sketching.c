// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Sketching (a working name): the deep-zoom drawing app — zoom in or out as far
// as anyone likes and draw at every depth (docs/infinite_canvas_design.md).
// The drawing core's shell (fude/drawing: the toolbar, the side panel's
// canvases, Settings, FAQ & Contact, About) over the deep-zoom page
// (fude/zoom/page.h) in place of the ink page.
//
// The app's shell: what RDE calls (init, the events, each frame's update and
// render, the end), and what it owns. The core still has its ink, its page and
// its canvas — the ink as the brush's settings the toolbar and Settings set;
// the page and canvas idle — and the extension's page kind hands Undo, Redo,
// Clear and the saving of a canvas to the deep-zoom page.
// ===========================================================================

#include "rde.h"

#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/row.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "zoom/page.h"
#include "zoom/smooth.h"
#include "drawing/widgets/toolbar.h"
#include "drawing/doc/import.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SKETCHING_CONFIG_PATH "./assets/config.rdef"

// A developer's build (debug, RDE_DEBUG) reads launch arguments (look.h); a
// release takes none at all (main).
#if defined(RDE_DEBUG)
#define SKETCHING_TAKES_ARGS 1
#else
#define SKETCHING_TAKES_ARGS 0
#endif

RDE_INTERNAL const fude_app_info SKETCHING_INFO = {
    .name = SKETCHING_NAME, .version = SKETCHING_VERSION, .store_id = SKETCHING_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Names.otf", .credits = FUDE_TEXT_COUNT,
    .licences = { { FUDE_TEXT_LICENCE_FONTS, { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } } },
    .licence_count = 1u,
};

RDE_INTERNAL rde_window*     window;
RDE_INTERNAL rde_camera      camera;
RDE_INTERNAL void sketching_render_top(rde_window* _window, f32 _dt);   // the notice, after the UI (below)
RDE_INTERNAL fude_ink        ink;
RDE_INTERNAL fude_canvas     canvas;
RDE_INTERNAL fude_lasso      lasso;
RDE_INTERNAL fude_notes      notes;
RDE_INTERNAL fude_page_input page;
RDE_INTERNAL fude_ui         ui;
RDE_INTERNAL fude_app        app;
RDE_INTERNAL fude_zoom_page  zoom;

// The first launch: brushes keep their size on the screen at any zoom (the
// research's first idea) — a stroke drawn zoomed in is finer on the canvas.
RDE_INTERNAL void sketching_first_launch(fude_app* _app) {
    _app->ink->brush_scale = FUDE_INK_BRUSH_SCALE_SCREEN;
}

RDE_INTERNAL const fude_extension SKETCHING_EXTENSION = {
    .page_kind       = &FUDE_ZOOM_PAGE_KIND,
    .tools           = FUDE_ZOOM_TOOLS,
    .tool_count      = 5u,
    .brush_on_screen = true,
    .settings_gather = fude_zoom_page_settings_gather,
    .settings_apply  = fude_zoom_page_settings_apply,
    .ui_build        = fude_zoom_page_ui_build,
    .ui_forget       = fude_zoom_page_ui_forget,
    .ui_update       = fude_zoom_page_ui_update,
    .ui_restyle      = fude_zoom_page_ui_restyle,
    .ui_hit          = fude_zoom_page_ui_hit,
    .first_launch    = sketching_first_launch,
};

// --- a developer's looks (debug builds only) ------------------------------------------------------
//
//   --demo-deep=N   flowers drawn at N depths, each 1024× inside the last, the
//                   camera left at the deepest (a shot shows every level at once)
//   --zoom=F        the camera's zoom times F, once loaded (0.001: out, 1000: in)
//   --zoom-later=F[@N]  the same a few frames in (at frame N: 6 when left out),
//                   after what the first update brings (a --demo-picture) has
//                   landed, or after a demo drew
//   --paper-is=K    the canvas's paper: dots, lines, squares or none
//   --pan=X,Y       the page dragged X, Y (screen units) — before a --zoom, to
//                   park the camera on a stroke's edge and look from inside it
//   --demo-erase    lines crossed and erased with each of the three erasers
//   --gate=POINTS   a canvas of that many points over four depths, for --perf
//   --demo-lasso[=2] the left half looped, dragged (and with 2, turned) — the selection's looks
//   --demo-snap     a rough circle, rectangle and line, each held till it snaps
//   --demo-picture=FILE  a picture brought in as Files would (JPEG, PNG; HEIC on a Mac)
//   --demo-shapes   the Shapes tool's rectangle, ellipse, triangle and line, dragged out
//   --shapes-panel  the Shapes tool's choices, open
//   --smoothing-panel the Smoothing tool's levels, open
//   --demo-smooth   one shaky wave drawn at each smoothing level, top to bottom:
//                   Off, Low, Medium, High, Rope
//   --demo-fill     a hand's closed loop, a rectangle, a figure 8 and an open arc,
//                   each tapped with the Fill tool (the arc: the notice)
//   --export=png:PATH or svg:PATH  at frame 20, the view exported there (as Export does)
//   --erase-at=X1,Y1,X2,Y2@N  at frame N, the eraser swept from one point to the other (screen)
//   --draw-at=X1,Y1,X2,Y2@N   ...the pen drawn instead
//   --clear-at=N    at frame N, Clear pressed (as the toolbar does)
//   --brush-at=X,Y[,erase|mark]  the brush's circle there, as if the pen hovered
//                   (screen units), with that tool in hand
//   --demo-places   two views marked (the page moved between), then Places opened
//   --demo-loop     the Lasso's loop drawn with the "pen" over nothing, and lifted
//   --fly-home      at frame 10, flown home as Reset view does (then Back shows)
//   --depth-levels  at frame 20, the depth's levels opened (with a deep --zoom)
//   --stress        drawing without pause: a stroke a frame, now and then an
//                   eraser sweep, an undo, a zoom in or out (the kill test's)
//   --stress-add    the same with strokes and zooms only: the count only grows

#if defined(RDE_DEBUG)
#include <math.h>

RDE_INTERNAL const rde_color SKETCHING_DEMO_COLORS[] = {
    { 231, 76, 60, 255 }, { 241, 196, 15, 255 }, { 46, 204, 113, 255 }, { 52, 152, 219, 255 }, { 155, 89, 182, 255 }, { 230, 126, 34, 255 },
};

// A curve in the camera's frame: a rose round _c, _r across, _width pt wide on screen.
RDE_INTERNAL void sketching_demo_curve(fude_zoom_scene* _s, fude_zoom_v2 _c, f64 _r, f64 _phase, u32 _petals, rde_color _color, f64 _width) {
    const u32 _n = 400u;
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = ldexp(1.0, _q);
    fude_zoom_qpoint _pts[400];
    fude_zoom_v2     _first = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _a = 6.283185307179586 * (f64)_i / (f64)(_n - 1u);
        const f64 _k = _r * (0.55 + 0.45 * cos((f64)_petals * _a + _phase));
        const fude_zoom_v2 _p = { _c.x + cos(_a) * _k, _c.y + sin(_a) * _k };
        if(_i == 0) {
            _first = _p;
        }
        _pts[_i] = (fude_zoom_qpoint){ (i32)llround((_p.x - _first.x) / _g), (i32)llround((_p.y - _first.y) / _g), (u16)(700 + 300 * sin(_a * 3.0)), _i * 4u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_s, _s->camera.frame, _first, _q, _pts, _n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME, _color,
                                              (f32)(_width / _s->camera.z), FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}

RDE_INTERNAL void sketching_demo_line(fude_zoom_scene* _s, fude_zoom_v2 _a, fude_zoom_v2 _b, rde_color _color, f64 _width) {
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = ldexp(1.0, _q);
    fude_zoom_qpoint _pts[200];
    for(u32 _i = 0; _i < 200u; _i++) {
        const f64 _t = (f64)_i / 199.0;
        _pts[_i] = (fude_zoom_qpoint){ (i32)llround((_b.x - _a.x) * _t / _g), (i32)llround((_b.y - _a.y) * _t / _g), 1023u, _i * 4u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_s, _s->camera.frame, _a, _q, _pts, 200u, FUDE_ZOOM_CHANNEL_TIME, _color, (f32)(_width / _s->camera.z), 0, 0, 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}

RDE_INTERNAL void sketching_zoom_by(fude_zoom_scene* _s, f64 _factor, fude_zoom_v2 _half) {
    // In steps, as a pinch would: each settle sees the zoom pass one threshold at most.
    const u32 _steps = (u32)ceil(fabs(log2(_factor))) + 1u;
    const f64 _step  = pow(_factor, 1.0 / (f64)_steps);
    for(u32 _i = 0; _i < _steps; _i++) {
        fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, _step);
        fude_zoom_camera_settle(_s, _half);
    }
}

RDE_INTERNAL u32 sketching_demo_lasso  = 0;       // --demo-lasso: 1 a loop and a drag; 2 and a turn
RDE_INTERNAL u32 sketching_frames      = 0;
RDE_INTERNAL b8  sketching_demo_snap   = false;   // --demo-snap: three rough shapes held till they snap
RDE_INTERNAL b8  sketching_demo_shapes = false;   // --demo-shapes: the Shapes tool's four, dragged out
RDE_INTERNAL i32 sketching_panel        = -1;     // --shapes-panel, --smoothing-panel: that tool's choices open
RDE_INTERNAL b8  sketching_demo_smooth  = false;  // --demo-smooth: a shaky wave at each smoothing level
RDE_INTERNAL b8  sketching_fly_home     = false;  // --fly-home: Reset view's flight at frame 10
RDE_INTERNAL b8  sketching_demo_places  = false;  // --demo-places: two places marked, Places open
RDE_INTERNAL b8  sketching_demo_loop    = false;  // --demo-loop: a lasso loop through the pen's own path
RDE_INTERNAL b8  sketching_demo_fill    = false;  // --demo-fill: things drawn, then tapped with the Fill tool
RDE_INTERNAL b8  sketching_brush_at     = false;  // --brush-at: the pen hovering there
RDE_INTERNAL i32 sketching_export       = -1;     // --export: 0 png, 1 svg, to sketching_export_path
RDE_INTERNAL f32 sketching_erase_line[4];         // --erase-at: from, to (screen)
RDE_INTERNAL u32 sketching_erase_frame = 0;       // ...at this frame (0: none)
RDE_INTERNAL FUDE_TOOL_ sketching_erase_tool = FUDE_TOOL_ERASE;   // --draw-at: the pen instead
RDE_INTERNAL u32 sketching_clear_frame = 0;       // --clear-at: Clear pressed at this frame (0: none)
RDE_INTERNAL c8  sketching_export_path[1024];
RDE_INTERNAL rde_vec_2F sketching_brush = { 0.0f, 0.0f };
RDE_INTERNAL i32        sketching_brush_tool = -1;
RDE_INTERNAL b8  sketching_depth_levels = false;  // --depth-levels: the depth's levels open at frame 20
RDE_INTERNAL b8  sketching_stress      = false;
RDE_INTERNAL b8  sketching_stress_add  = false;   // --stress-add: strokes and zooms only (the count only grows)
RDE_INTERNAL u32 sketching_stress_seed = 12345u;
RDE_INTERNAL f64 sketching_zoom_later  = 0.0;     // --zoom-later: the zoom, at frame sketching_zoom_frame
RDE_INTERNAL u32 sketching_zoom_frame  = 6u;

RDE_INTERNAL f64 sketching_stress_rnd(void) {
    sketching_stress_seed = sketching_stress_seed * 1664525u + 1013904223u;
    return (f64)(sketching_stress_seed >> 8) / 16777216.0;
}

// --demo-lasso, frame by frame (after the canvas has been drawn, so the lasso
// knows the frames on screen): a loop round the left half, a drag, a turn.
RDE_INTERNAL void sketching_demo_lasso_frame(void) {
    fude_zoom_selection* _sel = &zoom.selection;
    if(sketching_frames == 10u) {
        app.ui->bar.tool = FUDE_TOOL_LASSO;
        zoom.tool_seen   = (u8)FUDE_TOOL_LASSO;
        const rde_vec_2F _loop[5] = { { -330.0f, 260.0f }, { 20.0f, 260.0f }, { 20.0f, -260.0f }, { -330.0f, -260.0f }, { -330.0f, 255.0f } };
        fude_zoom_select_down(_sel, &zoom.scene, &zoom.renderer, _loop[0]);
        for(u32 _i = 1; _i < 5u; _i++) {
            fude_zoom_select_moved(_sel, _loop[_i]);
        }
        fude_zoom_select_up(_sel, &zoom.scene, &zoom.renderer);
    } else if(sketching_frames == 14u) {
        fude_zoom_box _b;
        if(fude_zoom_select_box(_sel, &_b)) {
            const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
            fude_zoom_select_down(_sel, &zoom.scene, &zoom.renderer, _c);
            fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + 40.0f, _c.y - 30.0f });
            fude_zoom_select_up(_sel, &zoom.scene, &zoom.renderer);
        }
    } else if(sketching_frames == 18u && sketching_demo_lasso >= 2u) {
        fude_zoom_box _b;
        if(fude_zoom_select_box(_sel, &_b)) {
            const rde_vec_2F _knob = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)(_b.max_y + FUDE_ZOOM_SELECT_KNOB) };
            const rde_vec_2F _c    = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
            fude_zoom_select_down(_sel, &zoom.scene, &zoom.renderer, _knob);
            // A sixth of a turn, clockwise.
            const f32 _r = _knob.y - _c.y;
            fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + _r * 0.866f, _c.y + _r * 0.5f });
            fude_zoom_select_up(_sel, &zoom.scene, &zoom.renderer);
        }
    }
}

// --demo-snap: a rough circle, rectangle and line drawn with the "pen", each
// held at its end until it snaps; the circle then pulled bigger while held.
typedef struct { u8 phase; f32 x, y; u32 hold; } sketching_pen_step;
RDE_INTERNAL sketching_pen_step SKETCHING_SNAP[600];
RDE_INTERNAL u32                SKETCHING_SNAP_N    = 0;
RDE_INTERNAL u32                SKETCHING_SNAP_AT   = 0;
RDE_INTERNAL f64                SKETCHING_SNAP_WAIT = 0.0;

RDE_INTERNAL void sketching_snap_add(u8 _phase, f32 _x, f32 _y, u32 _hold) {
    if(SKETCHING_SNAP_N < 600u) {
        SKETCHING_SNAP[SKETCHING_SNAP_N++] = (sketching_pen_step){ _phase, _x, _y, _hold };
    }
}

RDE_INTERNAL void sketching_snap_script(void) {
    u32 _seed = 99u;
    #define SKETCHING_JIT() (((f32)((_seed = _seed * 1664525u + 1013904223u) >> 8 & 0xFF) / 255.0f - 0.5f) * 5.0f)
    // A circle round (-250, 120), radius 110, ending where it began; held 800 ms; pulled out.
    for(u32 _i = 0; _i <= 72u; _i++) {
        const f32 _a = 6.2831853f * (f32)_i / 72.0f;
        sketching_snap_add(_i == 0 ? 0u : 1u, -250.0f + cosf(_a) * 110.0f + SKETCHING_JIT(), 120.0f + sinf(_a) * 104.0f + SKETCHING_JIT(), 0u);
    }
    sketching_snap_add(1u, -140.0f, 120.0f, 800u);
    for(u32 _i = 1; _i <= 10u; _i++) {
        sketching_snap_add(1u, -140.0f + 4.0f * (f32)_i, 120.0f + 2.0f * (f32)_i, 0u);
    }
    sketching_snap_add(2u, 0.0f, 0.0f, 0u);
    // A rectangle, a little off level.
    const f32 _r[5][2] = { { 60.0f, 220.0f }, { 330.0f, 228.0f }, { 326.0f, 60.0f }, { 56.0f, 52.0f }, { 60.0f, 216.0f } };
    for(u32 _k = 0; _k < 4u; _k++) {
        for(u32 _i = 0; _i < 18u; _i++) {
            const f32 _t = (f32)_i / 18.0f;
            sketching_snap_add(_k == 0 && _i == 0 ? 0u : 1u, _r[_k][0] + (_r[_k + 1][0] - _r[_k][0]) * _t + SKETCHING_JIT(), _r[_k][1] + (_r[_k + 1][1] - _r[_k][1]) * _t + SKETCHING_JIT(), 0u);
        }
    }
    sketching_snap_add(1u, _r[4][0], _r[4][1], 800u);
    sketching_snap_add(2u, 0.0f, 0.0f, 0u);
    // A line.
    for(u32 _i = 0; _i <= 30u; _i++) {
        sketching_snap_add(_i == 0 ? 0u : 1u, -350.0f + 20.0f * (f32)_i + SKETCHING_JIT(), -150.0f - 4.0f * (f32)_i + SKETCHING_JIT(), 0u);
    }
    sketching_snap_add(1u, 250.0f, -270.0f, 800u);
    sketching_snap_add(2u, 0.0f, 0.0f, 0u);
    #undef SKETCHING_JIT
}

RDE_INTERNAL void sketching_demo_snap_frame(void) {
    if(sketching_frames < 10u || SKETCHING_SNAP_AT >= SKETCHING_SNAP_N) {
        return;
    }
    const f64 _now = rde_engine_get_time_now();
    if(_now < SKETCHING_SNAP_WAIT) {
        return;   // the pen resting
    }
    const sketching_pen_step _st = SKETCHING_SNAP[SKETCHING_SNAP_AT++];
    fude_zoom_page_look_pen(&zoom, _st.phase, (rde_vec_2F){ _st.x, _st.y });
    if(_st.hold > 0u) {
        SKETCHING_SNAP_WAIT = _now + (f64)_st.hold / 1000.0;
    }
}

// --demo-shapes, and --shapes-panel or --smoothing-panel, once the canvas has been drawn.
RDE_INTERNAL void sketching_demo_shapes_frame(void) {
    if(sketching_frames == 20u && sketching_panel >= 0) {
        fude_toolbar_set_tool_open(&ui.bar, sketching_panel);
    }
    if(sketching_frames != 12u || !sketching_demo_shapes) {
        return;
    }
    const struct { u8 type; b8 filled; f32 x0, y0, x1, y1; } _shapes[4] = {
        { FUDE_ZOOM_SHAPE_RECT,    true,  -380.0f, 260.0f, -120.0f, 80.0f },
        { FUDE_ZOOM_SHAPE_ELLIPSE, false,  -60.0f, 260.0f,  260.0f, 60.0f },
        { FUDE_ZOOM_SHAPE_POLYGON, true,  -380.0f, -40.0f, -120.0f, -280.0f },
        { FUDE_ZOOM_SHAPE_LINE,    false,  -40.0f, -60.0f,  280.0f, -280.0f },
    };
    for(u32 _i = 0; _i < 4u; _i++) {
        zoom.shape_tool   = _shapes[_i].type;
        zoom.shape_filled = _shapes[_i].filled;
        fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ _shapes[_i].x0, _shapes[_i].y0 });
        fude_zoom_page_look_pen(&zoom, 1u, (rde_vec_2F){ _shapes[_i].x1, _shapes[_i].y1 });
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ _shapes[_i].x1, _shapes[_i].y1 });
    }
}

// --demo-smooth: the same shaky wave drawn with the "pen" at each level, top to
// bottom (smooth.h): Off, Low, Medium, High, Rope.
RDE_INTERNAL void sketching_demo_smooth_frame(void) {
    if(sketching_frames != 12u) {
        return;
    }
    const u8 _was  = zoom.smooth_level;
    u32      _seed = 4242u;
    for(u32 _l = 0; _l < FUDE_ZOOM_SMOOTH_COUNT; _l++) {
        zoom.smooth_level = (u8)_l;
        const f32  _y0  = 250.0f - 120.0f * (f32)_l;
        rde_vec_2F _pen = { 0.0f, 0.0f };
        // A sample every 1.5 points (a pen's, drawing briskly); a hand's tremor
        // (±2.5, some 20 points a wave) and a little noise on top.
        for(u32 _i = 0; _i <= 470u; _i++) {
            const f32 _x = 1.5f * (f32)_i;
            _seed = _seed * 1664525u + 1013904223u;
            const f32 _jx = ((f32)(_seed >> 8 & 0xFF) / 255.0f - 0.5f) * 2.0f;
            _seed = _seed * 1664525u + 1013904223u;
            const f32 _jy = ((f32)(_seed >> 8 & 0xFF) / 255.0f - 0.5f) * 2.0f;
            const f32 _tremor = 2.5f * sinf(_x * 0.31f) + 1.2f * sinf(_x * 0.53f + 1.0f);
            _pen = (rde_vec_2F){ -400.0f + _x + _jx, _y0 + 30.0f * sinf(_x * 0.017f) + _tremor + _jy };
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, _pen);
        }
        fude_zoom_page_look_pen(&zoom, 2u, _pen);
    }
    zoom.smooth_level = _was;
}

// --demo-fill: drawn with the "pen" (and the Shapes tool), then tapped with the
// Fill tool in three colours.
RDE_INTERNAL void sketching_demo_fill_frame(void) {
    if(sketching_frames != 12u) {
        return;
    }
    u32 _seed = 77u;
    #define SKETCHING_FILL_JIT() (((f32)((_seed = _seed * 1664525u + 1013904223u) >> 8 & 0xFF) / 255.0f - 0.5f) * 3.0f)
    // A loop round (-280, 140), ending a little past where it began.
    for(u32 _i = 0; _i <= 130u; _i++) {
        const f32 _a = 6.2831853f * (f32)_i / 120.0f;
        fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ -280.0f + cosf(_a) * 120.0f + SKETCHING_FILL_JIT(), 140.0f + sinf(_a) * 95.0f + SKETCHING_FILL_JIT() });
    }
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // A figure 8 round (100, 140).
    for(u32 _i = 0; _i <= 160u; _i++) {
        const f32 _a = 6.2831853f * (f32)_i / 160.0f, _d = 1.0f + sinf(_a) * sinf(_a);
        fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 100.0f + 150.0f * cosf(_a) / _d, 140.0f + 150.0f * sinf(_a) * cosf(_a) / _d });
    }
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // An open arc (a "C") round (200, -170).
    for(u32 _i = 0; _i <= 80u; _i++) {
        const f32 _a = 0.9f + 4.5f * (f32)_i / 80.0f;
        fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 200.0f + cosf(_a) * 100.0f, -170.0f + sinf(_a) * 100.0f });
    }
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // A rectangle with the Shapes tool.
    zoom.shape_tool = FUDE_ZOOM_SHAPE_RECT;
    fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ -400.0f, -80.0f });
    fude_zoom_page_look_pen(&zoom, 1u, (rde_vec_2F){ -160.0f, -260.0f });
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ -160.0f, -260.0f });
    zoom.shape_tool = 0;
    // Tapped with the Fill tool: the loop, the 8's left lobe, the rectangle, inside the arc.
    zoom.fill_tool = true;
    const rde_color _was = ink.color;
    const struct { f32 x, y; rde_color c; } _taps[4] = {
        { -280.0f, 140.0f, { 241, 196, 15, 255 } }, { 30.0f, 140.0f, { 52, 152, 219, 255 } },
        { -280.0f, -170.0f, { 231, 76, 60, 255 } }, { 200.0f, -170.0f, { 46, 204, 113, 255 } },
    };
    for(u32 _i = 0; _i < 4u; _i++) {
        ink.color = _taps[_i].c;
        fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ _taps[_i].x, _taps[_i].y });
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ _taps[_i].x, _taps[_i].y });
    }
    ink.color = _was;
    zoom.fill_tool = false;
    // The eraser across the loop and down the rectangle: rubbed out there, line and fill.
    fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_ERASE);
    fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ -440.0f, 150.0f });
    for(u32 _i = 1; _i <= 40u; _i++) {
        fude_zoom_page_look_pen(&zoom, 1u, (rde_vec_2F){ -440.0f + 8.0f * (f32)_i, 150.0f - 1.5f * (f32)_i });
    }
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ -280.0f, -60.0f });
    for(u32 _i = 1; _i <= 30u; _i++) {
        fude_zoom_page_look_pen(&zoom, 1u, (rde_vec_2F){ -280.0f, -60.0f - 8.0f * (f32)_i });
    }
    fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_DRAW);
    #undef SKETCHING_FILL_JIT
}

// One frame of --stress.
RDE_INTERNAL void sketching_stress_frame(void) {
    fude_zoom_scene* _s = &zoom.scene;
    if(!zoom.open || zoom.drawing || zoom.erasing) {
        return;
    }
    const rde_vec_2I   _size = rde_window_get_size(window);
    const fude_zoom_v2 _half = { (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
    const f64          _u    = 1.0 / _s->camera.z;
    const f64          _r    = sketching_stress_rnd();
    const fude_zoom_v2 _at   = { _s->camera.at.x + (sketching_stress_rnd() - 0.5) * _half.x * 1.6 * _u, _s->camera.at.y + (sketching_stress_rnd() - 0.5) * _half.y * 1.6 * _u };
    if(_r < 0.82 || (sketching_stress_add && _r < 0.96)) {
        sketching_demo_curve(_s, _at, (20.0 + sketching_stress_rnd() * 60.0) * _u, sketching_stress_rnd() * 6.0, 3u + (u32)(sketching_stress_rnd() * 5.0),
                             SKETCHING_DEMO_COLORS[(u32)(sketching_stress_rnd() * 6.0) % 6u], 1.0 + sketching_stress_rnd() * 4.0);
    } else if(_r < 0.92 && !sketching_stress_add) {
        fude_zoom_eraser _e;
        fude_zoom_eraser_init(&_e);
        fude_zoom_erase_begin(&_e, (FUDE_ZOOM_ERASE_)((u32)(sketching_stress_rnd() * 3.0) % 3u));
        const fude_zoom_v2 _to = { _at.x + (sketching_stress_rnd() - 0.5) * 200.0 * _u, _at.y + (sketching_stress_rnd() - 0.5) * 200.0 * _u };
        fude_zoom_erase_step(_s, &_e, _s->camera.frame, _at, _to, 10.0 * _u, _u);
        fude_zoom_erase_end(_s, &_e, _s->camera.frame, fude_zoom_box_empty());
        fude_zoom_eraser_destroy(&_e);
    } else if(_r < 0.96 && !sketching_stress_add) {
        fude_zoom_history_undo(_s);
    } else {
        sketching_zoom_by(_s, sketching_stress_rnd() < 0.5 ? 8.0 : 0.125, _half);
    }
}

RDE_INTERNAL void sketching_looks(i32 _argc, c8** _argv) {
    fude_zoom_scene* _s    = &zoom.scene;
    const rde_vec_2I _size = rde_window_get_size(window);
    const fude_zoom_v2 _half = { (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _v;
        if((_v = fude_look_value(_argv[_i], "--demo-deep")) != NULL) {
            const u32 _levels = (u32)atoi(_v);
            for(u32 _l = 0; _l < _levels; _l++) {
                const fude_zoom_v2 _c = _s->camera.at;
                const f64 _r = 260.0 / _s->camera.z;
                for(u32 _k = 0; _k < 3u; _k++) {
                    sketching_demo_curve(_s, _c, _r * (1.0 - 0.25 * (f64)_k), (f64)_k * 0.7, 5u + _k, SKETCHING_DEMO_COLORS[(_l + _k) % 6u], 7.0 - 2.0 * (f64)_k);
                }
                if(_l + 1u < _levels) {
                    // Off to a petal's tip, then 1024× in.
                    fude_zoom_camera_pan(_s, (fude_zoom_v2){ -_r * 0.9 * _s->camera.z, 0.0 });
                    sketching_zoom_by(_s, 1024.0, _half);
                }
            }
        } else if(fude_look_is(_argv[_i], "--demo-erase")) {
            const fude_zoom_v2 _c = _s->camera.at;
            const f64 _u = 1.0 / _s->camera.z;
            fude_zoom_eraser _e;
            fude_zoom_eraser_init(&_e);
            for(u32 _m = 0; _m < 3u; _m++) {
                const f64 _y = _c.y + (200.0 - 200.0 * (f64)_m) * _u;
                sketching_demo_line(_s, (fude_zoom_v2){ _c.x - 300.0 * _u, _y }, (fude_zoom_v2){ _c.x + 300.0 * _u, _y }, SKETCHING_DEMO_COLORS[_m], 6.0);
                sketching_demo_line(_s, (fude_zoom_v2){ _c.x - 100.0 * _u, _y - 60.0 * _u }, (fude_zoom_v2){ _c.x - 100.0 * _u, _y + 60.0 * _u }, SKETCHING_DEMO_COLORS[3], 4.0);
                sketching_demo_line(_s, (fude_zoom_v2){ _c.x + 100.0 * _u, _y - 60.0 * _u }, (fude_zoom_v2){ _c.x + 100.0 * _u, _y + 60.0 * _u }, SKETCHING_DEMO_COLORS[3], 4.0);
                fude_zoom_erase_begin(&_e, (FUDE_ZOOM_ERASE_)_m);
                fude_zoom_erase_step(_s, &_e, _s->camera.frame, (fude_zoom_v2){ _c.x, _y - 30.0 * _u }, (fude_zoom_v2){ _c.x, _y + 30.0 * _u }, 10.0 * _u, _u);
                fude_zoom_erase_end(_s, &_e, _s->camera.frame, fude_zoom_box_empty());
            }
            fude_zoom_eraser_destroy(&_e);
        } else if((_v = fude_look_value(_argv[_i], "--gate")) != NULL) {
            // Four depths, the points shared out, pen-like strokes of 400.
            const u64 _want = (u64)atoll(_v);
            u64 _made = 0;
            u32 _seed = 7u;
            for(u32 _l = 0; _l < 4u && _made < _want; _l++) {
                const fude_zoom_v2 _c = _s->camera.at;
                const f64 _u = 1.0 / _s->camera.z;
                const u64 _share = (_want - _made) / (4u - _l);
                for(u64 _p = 0; _p < _share; _p += 400u) {
                    _seed = _seed * 1664525u + 1013904223u;
                    const f64 _x = _c.x + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 2000.0 * _u;
                    _seed = _seed * 1664525u + 1013904223u;
                    const f64 _y = _c.y + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 1500.0 * _u;
                    sketching_demo_curve(_s, (fude_zoom_v2){ _x, _y }, 40.0 * _u, (f64)(_seed & 255u), 3u + (_seed % 5u), SKETCHING_DEMO_COLORS[_seed % 6u], 2.0);
                }
                _made += _share;
                if(_l < 3u) {
                    sketching_zoom_by(_s, 1024.0, _half);
                }
            }
            sketching_zoom_by(_s, 1.0 / 1024.0 / 1024.0 / 1024.0, _half);
        } else if(fude_look_is(_argv[_i], "--demo-lasso") || fude_look_value(_argv[_i], "--demo-lasso") != NULL) {
            _v = fude_look_value(_argv[_i], "--demo-lasso");
            sketching_demo_lasso = _v != NULL ? (u32)atoi(_v) : 1u;
        } else if((_v = fude_look_value(_argv[_i], "--demo-picture")) != NULL) {
            // As if Files had brought it (taken on the first update).
            const c8* _paths[1] = { _v };
            fude_import_arrived(FUDE_IMPORT_FILES, _paths, 1u);
        } else if(fude_look_is(_argv[_i], "--demo-shapes")) {
            sketching_demo_shapes = true;
        } else if(fude_look_is(_argv[_i], "--shapes-panel")) {
            sketching_panel = 0;
        } else if(fude_look_is(_argv[_i], "--smoothing-panel")) {
            sketching_panel = 2;
        } else if(fude_look_is(_argv[_i], "--demo-smooth")) {
            sketching_demo_smooth = true;
        } else if((_v = fude_look_value(_argv[_i], "--clear-at")) != NULL) {
            sketching_clear_frame = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--erase-at")) != NULL || (_v = fude_look_value(_argv[_i], "--draw-at")) != NULL) {
            sketching_erase_tool = strncmp(_argv[_i], "--draw-at", 9) == 0 ? FUDE_TOOL_DRAW : FUDE_TOOL_ERASE;
            if(sscanf(_v, "%f,%f,%f,%f@%u", &sketching_erase_line[0], &sketching_erase_line[1], &sketching_erase_line[2], &sketching_erase_line[3], &sketching_erase_frame) != 5) {
                sketching_erase_frame = 0;
            }
        } else if((_v = fude_look_value(_argv[_i], "--export")) != NULL) {
            sketching_export = strncmp(_v, "svg:", 4) == 0 ? 1 : 0;
            snprintf(sketching_export_path, sizeof(sketching_export_path), "%s", strchr(_v, ':') != NULL ? strchr(_v, ':') + 1 : _v);
        } else if((_v = fude_look_value(_argv[_i], "--brush-at")) != NULL) {
            c8 _tool[16] = { 0 };
            sketching_brush_at   = sscanf(_v, "%f,%f,%15s", &sketching_brush.x, &sketching_brush.y, _tool) >= 2;
            sketching_brush_tool = strcmp(_tool, "erase") == 0 ? (i32)FUDE_TOOL_ERASE : strcmp(_tool, "mark") == 0 ? (i32)FUDE_TOOL_MARK : -1;
        } else if(fude_look_is(_argv[_i], "--demo-fill")) {
            sketching_demo_fill = true;
        } else if(fude_look_is(_argv[_i], "--demo-loop")) {
            sketching_demo_loop = true;
        } else if(fude_look_is(_argv[_i], "--demo-places")) {
            sketching_demo_places = true;
        } else if(fude_look_is(_argv[_i], "--fly-home")) {
            sketching_fly_home = true;
        } else if(fude_look_is(_argv[_i], "--depth-levels")) {
            sketching_depth_levels = true;
        } else if(fude_look_is(_argv[_i], "--demo-snap")) {
            sketching_demo_snap = true;
            sketching_snap_script();
        } else if(fude_look_is(_argv[_i], "--stress") || fude_look_is(_argv[_i], "--stress-add")) {
            sketching_stress      = true;
            sketching_stress_add  = fude_look_is(_argv[_i], "--stress-add");
            sketching_stress_seed = (u32)rde_engine_get_time_now() * 2654435761u + 7u;
        } else if((_v = fude_look_value(_argv[_i], "--paper-is")) != NULL) {
            canvas.page.paper = strcmp(_v, "lines") == 0 ? FUDE_PAPER_LINES : strcmp(_v, "squares") == 0 ? FUDE_PAPER_SQUARES : strcmp(_v, "none") == 0 ? FUDE_PAPER_NONE : FUDE_PAPER_DOTS;
        } else if((_v = fude_look_value(_argv[_i], "--pan")) != NULL) {
            f64 _x = 0.0, _y = 0.0;
            sscanf(_v, "%lf,%lf", &_x, &_y);
            fude_zoom_camera_pan(_s, (fude_zoom_v2){ _x, _y });
        } else if((_v = fude_look_value(_argv[_i], "--zoom-later")) != NULL) {
            sketching_zoom_later = atof(_v);
            const c8* _at = strchr(_v, '@');
            sketching_zoom_frame = _at != NULL ? (u32)atoi(_at + 1) : 6u;
        } else if((_v = fude_look_value(_argv[_i], "--zoom")) != NULL) {
            sketching_zoom_by(_s, atof(_v), _half);
        }
    }
}
#endif

// --- init -------------------------------------------------------------------------------------

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    window = _window;
    fude_app_window(_window);    // its units (dp on Android): before anything reads its size
    fude_text_set_language(fude_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    rde_engine_set_top_overlay_render(sketching_render_top);
    fude_look_args(_argc, _argv);

    fude_ink_init(&ink);
    ink.brush_scale = FUDE_INK_BRUSH_SCALE_SCREEN;   // until the settings say otherwise
    fude_canvas_init(&canvas);
    fude_lasso_init(&lasso);
    fude_notes_init(&notes);

    app = (fude_app){
        .info = &SKETCHING_INFO, .ext = &SKETCHING_EXTENSION, .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f,
        .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
    };
    fude_app_start(&app);       // its save folder, its window's title: before anything reads a save
#if defined(RDE_PLATFORM_MOBILE)
    app.finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    fude_page_init(&page, &app);
    fude_zoom_page_init(&zoom, &app);
    fude_ui_init(&ui, &app);
    if(ui.font != NULL) {
        app.font    = ui.font;
        app.font_px = (f32)FUDE_KIT_FONT_SIZE;
    }

    fude_look_start(&app);
    fude_session_load(&app);    // opens the canvas on the deep-zoom page too (the page kind)
    fude_look_loaded(&app);
#if defined(RDE_DEBUG)
    sketching_looks(_argc, _argv);
#endif
}

// --- events --------------------------------------------------------------------------------------

void on_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);
    if(_event == NULL) {
        return;
    }
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        fude_ui_trim_fonts(&ui);
        fude_zoom_page_trim(&zoom);
    }
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        fude_session_save_on_exit(&app);
    }
    if(fude_app_top(&app) == NULL) {
        fude_zoom_page_event(&zoom, _event);
    }
}

// --- each frame -------------------------------------------------------------------------------

RDE_INTERNAL void sketching_update(f32 _dt) {
    fude_look_frame(&app);
    fude_ui_follow_language(&ui);
#if defined(RDE_PLATFORM_IOS)
    static b8  _pencil_taps  = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
    }
#endif
    fude_ink_frame_begin(&ink);
    fude_ink_frame_begin(&zoom.capture);
    if(!fude_app_update(&app, _dt)) {
        fude_zoom_page_update(&zoom);
    }
#if defined(RDE_DEBUG)
    sketching_frames++;
    if(sketching_stress) {
        sketching_stress_frame();
    }
    if(sketching_demo_lasso > 0u) {
        sketching_demo_lasso_frame();
    }
    if(sketching_demo_snap) {
        sketching_demo_snap_frame();
    }
    if(sketching_demo_shapes || sketching_panel >= 0) {
        sketching_demo_shapes_frame();
    }
    if(sketching_demo_smooth) {
        sketching_demo_smooth_frame();
    }
    if(sketching_demo_fill) {
        sketching_demo_fill_frame();
    }
    if(sketching_clear_frame > 0 && sketching_frames == sketching_clear_frame) {
        FUDE_ZOOM_PAGE_KIND.clear(&app);
    }
    if(sketching_erase_frame > 0 && sketching_frames == sketching_erase_frame) {
        fude_toolbar_set_tool(&ui.bar, sketching_erase_tool);
        zoom.tool_seen = (u8)sketching_erase_tool;
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _t = (f32)_i / 30.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ sketching_erase_line[0] + (sketching_erase_line[2] - sketching_erase_line[0]) * _t,
                                                                          sketching_erase_line[1] + (sketching_erase_line[3] - sketching_erase_line[1]) * _t });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_export >= 0 && sketching_frames == 20u) {
        fude_zoom_page_look_export(&zoom, (u8)sketching_export, sketching_export_path);
    }
    if(sketching_brush_at) {
        zoom.hover_at = sketching_brush;
        zoom.hover_on = true;
        if(sketching_brush_tool >= 0 && sketching_frames == 15u) {
            fude_toolbar_set_tool(&ui.bar, (FUDE_TOOL_)sketching_brush_tool);
        }
    }
    if(sketching_demo_loop && sketching_frames == 12u) {
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_LASSO);
        zoom.tool_seen = (u8)FUDE_TOOL_LASSO;
        for(u32 _i = 0; _i <= 40u; _i++) {
            const f32 _a = 3.0f * (f32)_i / 40.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 100.0f + cosf(_a) * 150.0f, -100.0f + sinf(_a) * 150.0f });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_demo_places) {
        if(sketching_frames == 10u || sketching_frames == 14u) {
            fude_zoom_page_look_nav(&zoom, 3u);
        } else if(sketching_frames == 12u) {
            fude_zoom_camera_zoom_at(&zoom.scene, (fude_zoom_v2){ 0.0, 0.0 }, 20.0);
            fude_zoom_camera_settle(&zoom.scene, (fude_zoom_v2){ 512.0, 384.0 });
        } else if(sketching_frames == 20u) {
            fude_zoom_page_look_nav(&zoom, 4u);
        }
    }
    if(sketching_fly_home && sketching_frames == 10u) {
        fude_zoom_page_look_nav(&zoom, 0u);
    }
    if(sketching_depth_levels && sketching_frames == 20u) {
        fude_zoom_page_look_nav(&zoom, 1u);
    }
    if(sketching_frames == sketching_zoom_frame && sketching_zoom_later > 0.0) {
        const rde_vec_2I _size = rde_window_get_size(window);
        sketching_zoom_by(&zoom.scene, sketching_zoom_later, (fude_zoom_v2){ (f64)_size.x * 0.5, (f64)_size.y * 0.5 });
    }
#endif
    fude_ui_update(&ui);
    fude_session_update(&app);
}

RDE_INTERNAL void sketching_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    fude_zoom_page_render_offscreen(&zoom, _window, &camera);   // a PNG export's view, when one is under way
    rde_rendering_clear_background_color(fude_theme_active()->page);
    rde_rendering_2d_begin_drawing(_window, &camera);
    if(!fude_app_render(&app)) {
        fude_zoom_page_render(&zoom, _window);
    }
    fude_app_render_overlays(&app);
    rde_rendering_2d_end_drawing();
}

RDE_INTERNAL void sketching_render_top(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(app.font == NULL) {
        return;
    }
    rde_rendering_2d_begin_drawing(_window, &camera);
    fude_notice_render(_window, app.font, app.font_px, fude_app_top(&app) == NULL ? 72.0f : 8.0f + fude_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

void on_update(f32 _dt) {
    fude_look_timed_update(sketching_update, _dt);
}

void on_render(rde_window* _window, f32 _dt) {
    fude_look_timed_render(&app, sketching_render, _window, _dt);
}

void on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

void on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

void on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

void end_func(void) {
    fude_session_save_on_exit(&app);
    fude_zoom_page_destroy(&zoom);
    fude_ui_destroy(&ui);
    fude_lasso_destroy(&lasso);
    fude_notes_destroy(&notes);
    fude_ink_destroy(&ink);
}

int main(i32 _argc, c8* _argv[]) {
#if !SKETCHING_TAKES_ARGS
    _argc = _argc > 1 ? 1 : _argc;
#endif
    return rde_run(_argc, _argv, SKETCHING_CONFIG_PATH, init_func, on_event, on_fixed_update, on_update, on_late_update, on_render, on_crash, end_func);
}
