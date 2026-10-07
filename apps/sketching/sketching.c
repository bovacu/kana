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
#include "zoom/themes.h"
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
    .licences = { { FUDE_TEXT_LICENCE_FONTS, { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
                  { FUDE_TEXT_ZOOM_LICENCE_HAND, { "assets/fonts/LICENSE-Hershey.txt", NULL, NULL } } },   // (Handwriting's letters)
    .licence_count = 2u,
    .turns = true,   // (more room to draw, landscape: Borja asked, 2026-10-05)
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
    .tool_count      = FUDE_EXTENSION_TOOLS,   // (Topic, then the topic's own: page.c's topics)
    .brush_on_screen = false,   // the bar's Page/Screen: on the page, a width on the canvas itself (what instruments want)
    .selection_row   = &FUDE_ZOOM_SELECTION_ROW,
    .selection_faces = fude_zoom_page_selection_faces,
    .context_row     = &FUDE_ZOOM_CONTEXT_ROW,
    .context_faces   = fude_zoom_page_context_faces,
    .sections        = &FUDE_ZOOM_PAGE_MEASUREMENTS,
    .section_count   = 1u,
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
//   --demo-lasso[=2|3] the left half looped, dragged (and with 2, turned; 3, turned 37° and held) — the selection's looks
//   --demo-snap     a rough circle, rectangle and line, each held till it snaps
//   --demo-picture=FILE  a picture brought in as Files would (JPEG, PNG; HEIC on a Mac)
//   --demo-shapes   the Shapes tool's rectangle, ellipse, triangle and line, dragged out
//   --shapes-panel  the Shapes tool's choices, open
//   --smoothing-panel the Smoothing tool's levels, open
//   --demo-smooth   one shaky wave drawn at each smoothing level, top to bottom:
//                   Off, Low, Medium, High, Rope
//   --demo-fill     a hand's closed loop, a rectangle, a figure 8 and an open arc,
//                   each tapped with the Fill tool (the arc: the notice)
//   --export=png:PATH or svg:PATH  at frame 20, the view exported there (as Export does; mp4:PATH the zoom video, made over the frames after)
//   --erase-at=X1,Y1,X2,Y2@N  at frame N, the eraser swept from one point to the other (screen)
//   --draw-at=X1,Y1,X2,Y2@N   ...the pen drawn instead
//   --instruments=K,K...      those instruments out at frame 8 (0 ruler, 1 45° set square, 2 30°/60°, 3 protractor, 4 compass; a kind twice: two)
//   --instrument-at=K,X,Y,DEG ...and that one put there, turned so
//   --finger-drag=X1,Y1,X2,Y2,X3,Y3@N  a finger on the instruments from one point through another to a third (frames N on)
//   --measure=1|2             the pen measures: the tape (1) or a dimension (2) (with --draw-at)
//   --numpad=TEXT@N           the numpad open at frame N with TEXT in it (on the last line drawn)
//   --text=X,Y,S,WORDS@N      a text (S 0) or a sticky note (S 1) saying WORDS (| a line break) at frame N
//   --calibrate               the true size's calibration at frame 10
//   --mermaid=FILE            a diagram from that file's Mermaid text at frame 15
//   --arrange=K               everything selected at frame 20, then arranged so (select.h's FUDE_ZOOM_ARRANGE_)
//   --demo-bucket             four lines round a box (a small gap at a corner), a ring inside, the Fill tool tapped inside
//   --demo-layers             the layers' panel, a new layer drawn on, the first layer hidden
//   --offset=MM               everything selected at frame 20, then offset by MM (with --arrange's selecting)
//   --demo-curve              the Curve tool: five points tapped, the last again (a wave)
//   --find=WORDS@N            Find for WORDS at frame N
//   --pdf=PATH@N              at frame N, the PDF at PATH picked (Insert → PDF): a canvas over it
//   --loop-at=X1,Y1,X2,Y2@N   at frame N, the Lasso round that screen box (a PDF's text in it: logged)
//   --offset-at=N             at frame N, the selection's Offset pressed (a board alone: its grain turned)
//   --joint=K,A,B,C@N         at frame N, a joint put down (K 1 a finger joint, 2 a dovetail; length, width or tails, depth)
//   --fillet=MM@N             at frame N, two lines meeting in a corner lassoed, then that corner rounded MM mm (0: left lassoed)
//   --library=F@N             at frame N, the diagram library's panel open on family F (0 flowchart ... 6 planning)
//   --symbol=ID,DX,DY,TEXT@N  at frame N, the symbol ID (symbol.c's ids) put DX, DY from the middle with TEXT ("|" a new line)
//   --value=ID,TEXT,UNIT,WAY@N  at frame N, the first symbol ID lassoed alone and given TEXT in its UNIT (Value's card)
//   --slider=I,PERCENT@N  at frame N, Play's slider I set PERCENT of the way along
//   --make-part@N         at frame N, what the last demo drew lassoed and made a part (Make part)
//   --align=ID,DX,DY@N    at frame N, the first symbol ID lassoed alone and dragged DX, DY points, held (aligned)
//   --body=ALL,MATERIAL,FIXED,APPLY@N  at frame N, Make body's card on the lasso (ALL 1: everything lassoed first),
//                         material MATERIAL (0 wood…7 foam), FIXED 1 the ground, APPLY 1 applied
//   --kanban=P@N              at frame N, a Kanban board of preset P (0 Kanban, 1 Scrum, 2 retrospective, 3 week)
//   --kanban-card=P[+][!]@N   at frame N, Insert's Kanban card up with template P in its rows (+: a row more; !: then Insert)
//   --map@N                   at frame N, the map's panel up
//   --area=NAME@N             at frame N, an area named NAME round the view (or what the lasso holds)
//   --present=S@N             at frame N, presenting, at step S (0 the first)
//   --laser@N                 at frame N, a laser's trail across the screen (presenting)
//   --keep=NAME@N             at frame N, what the lasso holds kept as a piece NAME (My pieces)
//   --pieces@N                at frame N, My pieces' panel up
//   --piece-put=I@N           at frame N, piece I put in the middle of the view
//   --stencil=I@N             at frame N, piece I laid as a stencil (an instrument)
//   --canvas=C@N              at frame N, canvas C opened (its notes.h id), as the side panel opens one
//   --pen-tap=X,Y@N           at frame N, the pen down and up at X,Y (the screen's, centre origin, Y up), the tool as it is
//   --pen-drag=X0,Y0,X1,Y1@N  at frame N, the pen down at X0,Y0, moved to X1,Y1 over ten frames, up
//   --census=N                at frame N, what is alive on the canvas logged: strokes, shapes by type (line, arc...), texts
//   --clear-at=N    at frame N, Clear pressed (as the toolbar does)
//   --undo-at=N[,M...]  at those frames, Undo pressed (--redo-at: Redo)
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
RDE_INTERNAL u32 sketching_export_at    = 20u;    // --export-at: at that frame
RDE_INTERNAL f32 sketching_erase_line[4];         // --erase-at: from, to (screen)
RDE_INTERNAL u32 sketching_erase_frame = 0;       // ...at this frame (0: none)
RDE_INTERNAL FUDE_TOOL_ sketching_erase_tool = FUDE_TOOL_ERASE;   // --draw-at: the pen instead
RDE_INTERNAL u32 sketching_clear_frame = 0;       // --clear-at: Clear pressed at this frame (0: none)
RDE_INTERNAL u32 sketching_undo_frames[2][8];     // --undo-at, --redo-at: Undo, Redo pressed at these frames (0: none)
RDE_INTERNAL c8  sketching_instruments[16];       // --instruments: the kinds brought out
RDE_INTERNAL f32 sketching_instrument_at[4][4];   // --instrument-at: kind (+1: given), x, y, degrees
RDE_INTERNAL u32 sketching_instrument_placed = 0;
RDE_INTERNAL f32 sketching_finger_drag[6];        // --finger-drag: three points (screen)
RDE_INTERNAL u32 sketching_finger_frame = 0;
RDE_INTERNAL u8  sketching_measure = 0;           // --measure: the measuring mode
RDE_INTERNAL c8  sketching_numpad[64];            // --numpad: its text...
RDE_INTERNAL u32 sketching_numpad_frame = 0;      // ...and when
RDE_INTERNAL c8  sketching_texts[4][160];         // --text: each one's argument
RDE_INTERNAL u32 sketching_text_count = 0;
RDE_INTERNAL b8  sketching_calibrate  = false;    // --calibrate
RDE_INTERNAL c8  sketching_mermaid[1024];         // --mermaid: the file
RDE_INTERNAL i32 sketching_arrange = -1;          // --arrange: how
RDE_INTERNAL b8  sketching_demo_bucket = false;   // --demo-bucket
RDE_INTERNAL u32 sketching_demo_layers = 0;       // --demo-layers[=2 everything moved to the new layer, =3 and its name on the card, =4 renamed, =5 the new layer moved under the first]
RDE_INTERNAL f64 sketching_offset = 0.0;          // --offset
RDE_INTERNAL b8  sketching_demo_curving = false;  // --demo-curve
RDE_INTERNAL u32 sketching_demo_drive = 0;        // --demo-drive[=2: and its length typed]
RDE_INTERNAL b8  sketching_saw = false;           // --saw: the Saw on from the start
RDE_INTERNAL f32 sketching_kerf = -1.0f;          // --kerf=MM: the saw's kerf at frame 4
RDE_INTERNAL i32 sketching_units = -1;            // --units=N: the page's units at frame 4 (0 mm, 1 cm, 2 m, 3 in, 4 ft)
RDE_INTERNAL i32 sketching_bar_tool = -1;         // --bar-tool=N: the bar's tool N (0 pen, 1 eraser, 2 lasso, 3 marker) chosen at frame 8
RDE_INTERNAL f32 sketching_tap_at[3];            // --tap-at=X,Y@N: a tap with the Lasso there at frame N
RDE_INTERNAL b8  sketching_home_view = false;     // --home-view: the camera at home, x1, before anything else (frame 2)
RDE_INTERNAL f32 sketching_loop_at[4][4];        // --loop-at=X,Y,R@N: a round loop drawn with the pen there at frame N (up to four)
RDE_INTERNAL u32 sketching_loops = 0;
RDE_INTERNAL i32 sketching_demo_board = -1;       // --demo-board[=1: two put down, one lassoed; 2: both lassoed; 3: its name and material asked; 4: as 1, walnut and oak][@N: at frame N]
RDE_INTERNAL u32 sketching_demo_board_at = 10u;
RDE_INTERNAL u32 sketching_census = 0;
RDE_INTERNAL i32 sketching_library = -1;          // --library=F@N: the diagram library's panel open on family F at frame N
RDE_INTERNAL u32 sketching_library_at = 0;
RDE_INTERNAL c8  sketching_symbols[8][160];       // --symbol=ID,DX,DY,TEXT@N: a symbol put there at frame N (up to eight)
RDE_INTERNAL u32 sketching_symbol_at[8];
RDE_INTERNAL u32 sketching_symbol_count = 0;
RDE_INTERNAL i32 sketching_kanban = -1;           // --kanban=P@N: a Kanban board of preset P (0 Kanban, 1 Scrum, 2 retro, 3 week) at frame N
RDE_INTERNAL u32 sketching_kanban_at = 0;
RDE_INTERNAL u32 sketching_map_at = 0;              // --map@N
RDE_INTERNAL c8  sketching_area[96];                // --area=NAME@N
RDE_INTERNAL u32 sketching_area_at = 0;
RDE_INTERNAL i32 sketching_present = -1;            // --present=S@N
RDE_INTERNAL u32 sketching_present_at = 0;
RDE_INTERNAL u32 sketching_laser_at = 0;            // --laser@N
RDE_INTERNAL f32 sketching_pen_taps[4][3];          // --pen-tap=X,Y@N (up to four)
RDE_INTERNAL u32 sketching_pen_tap_count = 0;
RDE_INTERNAL u32 sketching_canvas[2];               // --canvas=C@N
RDE_INTERNAL c8  sketching_keep[64];                // --keep=NAME@N
RDE_INTERNAL u32 sketching_keep_at = 0;
RDE_INTERNAL u32 sketching_pieces_at = 0;           // --pieces@N
RDE_INTERNAL u32 sketching_piece_put[2];            // --piece-put=I@N
RDE_INTERNAL u32 sketching_stencil[2];              // --stencil=I@N
RDE_INTERNAL f32 sketching_pen_drags[4][5];         // --pen-drag=X0,Y0,X1,Y1@N (up to four)
RDE_INTERNAL u32 sketching_pen_drag_count = 0;
RDE_INTERNAL i32 sketching_kanban_card = -1;      // --kanban-card=P[+][!]@N: the Kanban card up at frame N with template P
RDE_INTERNAL u32 sketching_kanban_card_at = 0;
RDE_INTERNAL c8  sketching_sheet[96] = { 0 };    // --sheet=PAPER:WAY:SCALE[:W:H][!]@N (or edit@N): a sheet's card up at frame N
RDE_INTERNAL u32 sketching_sheet_at = 0;
RDE_INTERNAL c8  sketching_size[96] = { 0 };     // --size-card=W;H;A;X;Y[~][!]@N: the size card up for what the lasso holds at frame N
RDE_INTERNAL u32 sketching_size_at = 0;
RDE_INTERNAL c8  sketching_repeat[128] = { 0 };  // --repeat=MODE;...[!]@N: the Repeat card up for what the lasso holds at frame N
RDE_INTERNAL u32 sketching_repeat_at = 0;
RDE_INTERNAL c8  sketching_points[6][64];             // --point=MODE:A:B:PRESS@N: a point typed for the Curve tool at frame N (up to six)
RDE_INTERNAL u32 sketching_point_at[6];
RDE_INTERNAL u32 sketching_point_count = 0;
RDE_INTERNAL u32 sketching_lines[2];
RDE_INTERNAL u32 sketching_combine[2];
RDE_INTERNAL u32 sketching_circuit[2];                 // --circuit=DEMO@N: a demo circuit (page.c's look_circuit)
RDE_INTERNAL u32 sketching_circuit_run = 0;
RDE_INTERNAL u32 sketching_mech[2];                    // --mech=DEMO@N: a demo mechanism (page.c's look_mech)
RDE_INTERNAL u32 sketching_mech_run = 0;
RDE_INTERNAL c8  sketching_value[96];                  // --value=ID,TEXT,UNIT,WAY@N: the first symbol ID given a value (Value's card)
RDE_INTERNAL u32 sketching_value_frame = 0;
RDE_INTERNAL u32 sketching_slider[2];                  // --slider=I,PERCENT@N: Play's slider I set PERCENT along
RDE_INTERNAL f64 sketching_slider_t = 0.0;
RDE_INTERNAL u32 sketching_body[5];                    // --body=ALL,MATERIAL,FIXED,APPLY@N: drawings made bodies
RDE_INTERNAL u32 sketching_make_part = 0;              // --make-part@N: the last demo made a part
RDE_INTERNAL c8  sketching_align[48];                  // --align=ID,DX,DY@N: a symbol dragged, held
RDE_INTERNAL u32 sketching_align_frame = 0;
RDE_INTERNAL f32 sketching_context[3] = { 0 };    // --context=X,Y@N: the page's menu opened at X,Y (screen) at frame N
RDE_INTERNAL u32 sketching_context_delete = 0;    // --context-delete@N: its Delete pressed at frame N
RDE_INTERNAL u32 sketching_circuit2[2];                // --circuit2=DEMO@N: a second demo circuit (after a part is made)
RDE_INTERNAL u32 sketching_plan[2][2];                 // --plan=STEP@N (twice at most): a floor plan's demo (page.c's look_plan)
RDE_INTERNAL u32 sketching_plan_count = 0;               // --mech-run@N: run from frame N            // --circuit-run@N: simulated from frame N
RDE_INTERNAL u32 sketching_shapes_choices[4][2];       // --shapes-choice=I@N: the Shapes panel's choice I pressed at frame N (up to four)
RDE_INTERNAL u32 sketching_shapes_choice_count = 0;                 // --combine=OP@N: shapes made and combined (page.c's look_combine)                   // --lines=STEP@N: lines made, extended, trimmed, chamfered (page.c's look_lines)
RDE_INTERNAL u32 sketching_shape_tools[4][2];          // --shape-tool=TYPE@N: the Shapes tool's TYPE (shape.h) in hand at frame N (up to four)
RDE_INTERNAL u32 sketching_shape_tool_count = 0;
RDE_INTERNAL u32 sketching_measure_tool[2] = { 0, 0 }; // --measure-tool=N@F: the measure in hand at frame F (1 the tape, 2 the dimension)
RDE_INTERNAL u32 sketching_topic[2] = { 0, 0 };        // --topic=N@F: the canvas's topic N (page.h's FUDE_ZOOM_TOPIC_) at frame F
RDE_INTERNAL u32 sketching_line_style[2] = { 0, 0 };   // --line-style=N@F: the Shapes' line style N (shape.h) at frame F
RDE_INTERNAL b8  sketching_kanban_card_extra = false, sketching_kanban_card_insert = false;
RDE_INTERNAL u32 sketching_tidy_at = 0;           // --tidy=N: at frame N, every sticky note lassoed and let go (lined up in a Kanban column)
RDE_INTERNAL f64 sketching_fillet_mm = -1.0;      // --fillet=MM@N: two lines in a corner lassoed at frame N, then filleted MM round (0: not)
RDE_INTERNAL u32 sketching_fillet_at = 0;            // --census=N: at frame N, what the canvas holds logged (its shapes by type)
RDE_INTERNAL f32 sketching_path[16][2];          // --path-at=X,Y;X,Y;...@N: the pen drawn through those points at frame N
RDE_INTERNAL u32 sketching_path_n = 0, sketching_path_frame = 0;
RDE_INTERNAL f32 sketching_pan_later[3];         // --pan-later=X,Y@N: the camera moved X, Y screen points at frame N
RDE_INTERNAL u32 sketching_handwrite = 0;        // --handwrite=N: every text written out by hand at frame N
RDE_INTERNAL f32 sketching_grip[3];              // --grip=DX,DY@N: the offset pad's grip dragged at frame N
RDE_INTERNAL f32 sketching_grow[4];              // --grow=K,BY@N[!]: instrument K's size tab dragged BY points at frame N ("!": still held)
RDE_INTERNAL f32 sketching_anchor[4];            // --anchor=K,ACROSS,UP@N: instrument K snapped where it lies at frame N, then moved that many mm
RDE_INTERNAL u32 sketching_anchor_frame = 0;
RDE_INTERNAL c8  sketching_parts[256];           // --parts=ROWS@N[!]: Fit parts' card filled at frame N ("600 x 300; 200 x 100"; "!": then Cut)
RDE_INTERNAL u32 sketching_parts_frame = 0;
RDE_INTERNAL b8  sketching_parts_cut = false;
RDE_INTERNAL c8  sketching_fit[256];             // --fit=LIST@N: the lassoed board's parts fitted at frame N ("@N" alone: Fit parts pressed)
RDE_INTERNAL u32 sketching_fit_frame = 0;
RDE_INTERNAL u32 sketching_demo_sculpt = 0;       // --demo-sculpt[=2|3]: a line, then a bump drawn over its middle with Sculpt on (2: off: a stroke of its own; 3: Nudge pushing its middle up)
RDE_INTERNAL f64 sketching_holes = 0.0;           // --holes=MM: a 32 mm hole row that long at frame 10
RDE_INTERNAL u32 sketching_to_curve = 0;          // --to-curve=N: everything lassoed and To curve at frame N
RDE_INTERNAL i32 sketching_nodes = -1;            // --nodes=STEP (with --demo-curve): the path lassoed, 1 a node dragged, 2 one added, 3 one held,
                                                  // 4 one tapped (its handle shown), 5 and its handle dragged out
RDE_INTERNAL c8  sketching_find[64];              // --find: the words...
RDE_INTERNAL u32 sketching_find_frame = 0;        // ...and when
RDE_INTERNAL c8  sketching_pdf[512];              // --pdf: the PDF...
RDE_INTERNAL u32 sketching_pdf_frame = 0;         // ...and when
RDE_INTERNAL f32 sketching_loop[4];               // --loop-at: the box...
RDE_INTERNAL u32 sketching_loop_frame = 0;        // ...and when
RDE_INTERNAL u32 sketching_offset_frame = 0;      // --offset-at
RDE_INTERNAL f32 sketching_joint[4];              // --joint: kind and its three numbers...
RDE_INTERNAL u32 sketching_joint_frame = 0;       // ...and when
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
            // A sixth of a turn, clockwise (3: 37°, and still held: its angle shown).
            const f32 _r = _knob.y - _c.y;
            if(sketching_demo_lasso >= 3u) {
                fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + _r * cosf(53.0f * 3.14159265f / 180.0f), _c.y + _r * sinf(53.0f * 3.14159265f / 180.0f) });
                return;
            }
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
            sketching_panel = 1;   // (Topic the first: General's Shapes, Insert, Smoothing...)
        } else if(fude_look_is(_argv[_i], "--smoothing-panel")) {
            sketching_panel = 3;
        } else if((_v = fude_look_value(_argv[_i], "--tool-panel")) != NULL) {
            sketching_panel = atoi(_v);   // any app tool's choices open (0: Topic; in General 6: Instruments)
        } else if((_v = fude_look_value(_argv[_i], "--topic")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_topic[0], &sketching_topic[1]);
        } else if(fude_look_is(_argv[_i], "--demo-smooth")) {
            sketching_demo_smooth = true;
        } else if((_v = fude_look_value(_argv[_i], "--text")) != NULL && sketching_text_count < 4u) {
            snprintf(sketching_texts[sketching_text_count++], sizeof(sketching_texts[0]), "%s", _v);
        } else if((_v = fude_look_value(_argv[_i], "--joint")) != NULL) {
            if(sscanf(_v, "%f,%f,%f,%f@%u", &sketching_joint[0], &sketching_joint[1], &sketching_joint[2], &sketching_joint[3], &sketching_joint_frame) != 5) {
                sketching_joint_frame = 0;
            }
        } else if((_v = fude_look_value(_argv[_i], "--offset-at")) != NULL) {
            sketching_offset_frame = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--loop-at")) != NULL) {
            if(sscanf(_v, "%f,%f,%f,%f@%u", &sketching_loop[0], &sketching_loop[1], &sketching_loop[2], &sketching_loop[3], &sketching_loop_frame) != 5) {
                sketching_loop_frame = 0;
            }
        } else if((_v = fude_look_value(_argv[_i], "--pdf")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_pdf, sizeof(sketching_pdf), "%.*s", _at != NULL ? (int)(_at - _v) : (int)strlen(_v), _v);
            sketching_pdf_frame = _at != NULL ? (u32)atoi(_at + 1) : 10u;
        } else if((_v = fude_look_value(_argv[_i], "--find")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_find, sizeof(sketching_find), "%.*s", _at != NULL ? (int)(_at - _v) : (int)strlen(_v), _v);
            sketching_find_frame = _at != NULL ? (u32)atoi(_at + 1) : 30u;
        } else if(fude_look_is(_argv[_i], "--demo-drive") || fude_look_value(_argv[_i], "--demo-drive") != NULL) {
            _v = fude_look_value(_argv[_i], "--demo-drive");
            sketching_demo_drive = _v != NULL ? (u32)atoi(_v) : 1u;
        } else if((_v = fude_look_value(_argv[_i], "--fillet")) != NULL) {
            sketching_fillet_mm = atof(_v);
            sketching_fillet_at = strchr(_v, '@') != NULL ? (u32)atoi(strchr(_v, '@') + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--library")) != NULL) {
            sketching_library    = atoi(_v);
            sketching_library_at = strchr(_v, '@') != NULL ? (u32)atoi(strchr(_v, '@') + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--symbol")) != NULL && sketching_symbol_count < 8u) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_symbols[sketching_symbol_count], sizeof(sketching_symbols[0]), "%.*s", _at != NULL ? (int)(_at - _v) : (int)strlen(_v), _v);
            sketching_symbol_at[sketching_symbol_count] = _at != NULL ? (u32)atoi(_at + 1) : 20u;
            sketching_symbol_count++;
        } else if((_v = fude_look_value(_argv[_i], "--tidy")) != NULL) {
            sketching_tidy_at = (u32)atoi(_v);
        } else if(strncmp(_argv[_i], "--map@", 6) == 0) {
            sketching_map_at = (u32)atoi(_argv[_i] + 6);
        } else if((_v = fude_look_value(_argv[_i], "--keep")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_keep, sizeof(sketching_keep), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_keep_at = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if(strncmp(_argv[_i], "--pieces@", 9) == 0) {
            sketching_pieces_at = (u32)atoi(_argv[_i] + 9);
        } else if((_v = fude_look_value(_argv[_i], "--stencil")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_stencil[0], &sketching_stencil[1]);
        } else if((_v = fude_look_value(_argv[_i], "--piece-put")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_piece_put[0], &sketching_piece_put[1]);
        } else if((_v = fude_look_value(_argv[_i], "--canvas")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_canvas[0], &sketching_canvas[1]);
        } else if((_v = fude_look_value(_argv[_i], "--pen-tap")) != NULL) {
            u32 _frame = 0;
            f32* _t = sketching_pen_taps[sketching_pen_tap_count < 4u ? sketching_pen_tap_count : 3u];
            if(sscanf(_v, "%f,%f@%u", &_t[0], &_t[1], &_frame) == 3) {
                _t[2] = (f32)_frame;
                sketching_pen_tap_count += sketching_pen_tap_count < 4u ? 1u : 0u;
            }
        } else if((_v = fude_look_value(_argv[_i], "--pen-drag")) != NULL) {
            u32 _frame = 0;
            f32* _d = sketching_pen_drags[sketching_pen_drag_count < 4u ? sketching_pen_drag_count : 3u];
            if(sscanf(_v, "%f,%f,%f,%f@%u", &_d[0], &_d[1], &_d[2], &_d[3], &_frame) == 5) {
                _d[4] = (f32)_frame;
                sketching_pen_drag_count += sketching_pen_drag_count < 4u ? 1u : 0u;
            }
        } else if(strncmp(_argv[_i], "--laser@", 8) == 0) {
            sketching_laser_at = (u32)atoi(_argv[_i] + 8);
        } else if((_v = fude_look_value(_argv[_i], "--area")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_area, sizeof(sketching_area), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_area_at = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--present")) != NULL) {
            sketching_present    = atoi(_v);
            sketching_present_at = strchr(_v, '@') != NULL ? (u32)atoi(strchr(_v, '@') + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--line-style")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_line_style[0], &sketching_line_style[1]);
        } else if((_v = fude_look_value(_argv[_i], "--measure-tool")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_measure_tool[0], &sketching_measure_tool[1]);
        } else if((_v = fude_look_value(_argv[_i], "--shapes-choice")) != NULL && sketching_shapes_choice_count < 4u) {
            if(sscanf(_v, "%u@%u", &sketching_shapes_choices[sketching_shapes_choice_count][0], &sketching_shapes_choices[sketching_shapes_choice_count][1]) == 2) {
                sketching_shapes_choice_count++;
            }
        } else if((_v = fude_look_value(_argv[_i], "--plan")) != NULL && sketching_plan_count < 2u) {
            if(sscanf(_v, "%u@%u", &sketching_plan[sketching_plan_count][0], &sketching_plan[sketching_plan_count][1]) == 2) {
                sketching_plan_count++;
            }
        } else if((_v = fude_look_value(_argv[_i], "--mech")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_mech[0], &sketching_mech[1]);
        } else if((_v = fude_look_value(_argv[_i], "--value")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            if(_at != NULL) {
                snprintf(sketching_value, sizeof(sketching_value), "%.*s", (int)(_at - _v), _v);
                sketching_value_frame = (u32)atoi(_at + 1);
            }
        } else if((_v = fude_look_value(_argv[_i], "--context")) != NULL) {
            u32 _frame = 0;
            if(sscanf(_v, "%f,%f@%u", &sketching_context[0], &sketching_context[1], &_frame) == 3) {
                sketching_context[2] = (f32)_frame;
            }
        } else if(strncmp(_argv[_i], "--context-delete@", 17) == 0) {
            sketching_context_delete = (u32)atoi(_argv[_i] + 17);
        } else if((_v = fude_look_value(_argv[_i], "--align")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            if(_at != NULL) {
                snprintf(sketching_align, sizeof(sketching_align), "%.*s", (int)(_at - _v), _v);
                sketching_align_frame = (u32)atoi(_at + 1);
            }
        } else if(strncmp(_argv[_i], "--make-part@", 12) == 0) {
            sketching_make_part = (u32)atoi(_argv[_i] + 12);
        } else if((_v = fude_look_value(_argv[_i], "--circuit2")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_circuit2[0], &sketching_circuit2[1]);
        } else if((_v = fude_look_value(_argv[_i], "--body")) != NULL) {
            sscanf(_v, "%u,%u,%u,%u@%u", &sketching_body[0], &sketching_body[1], &sketching_body[2], &sketching_body[3], &sketching_body[4]);
        } else if((_v = fude_look_value(_argv[_i], "--slider")) != NULL) {
            u32 _pc = 0;
            if(sscanf(_v, "%u,%u@%u", &sketching_slider[0], &_pc, &sketching_slider[1]) == 3) {
                sketching_slider_t = (f64)_pc / 100.0;
            }
        } else if(strncmp(_argv[_i], "--mech-run@", 11) == 0) {
            sketching_mech_run = (u32)atoi(_argv[_i] + 11);
        } else if((_v = fude_look_value(_argv[_i], "--circuit")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_circuit[0], &sketching_circuit[1]);
        } else if(strncmp(_argv[_i], "--circuit-run@", 14) == 0) {
            sketching_circuit_run = (u32)atoi(_argv[_i] + 14);
        } else if((_v = fude_look_value(_argv[_i], "--combine")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_combine[0], &sketching_combine[1]);
        } else if((_v = fude_look_value(_argv[_i], "--lines")) != NULL) {
            sscanf(_v, "%u@%u", &sketching_lines[0], &sketching_lines[1]);
        } else if((_v = fude_look_value(_argv[_i], "--point")) != NULL && sketching_point_count < 6u) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_points[sketching_point_count], sizeof(sketching_points[0]), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_point_at[sketching_point_count++] = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--shape-tool")) != NULL) {
            u32* _st = sketching_shape_tools[sketching_shape_tool_count < 4u ? sketching_shape_tool_count : 3u];
            if(sscanf(_v, "%u@%u", &_st[0], &_st[1]) == 2) {
                sketching_shape_tool_count += sketching_shape_tool_count < 4u ? 1u : 0u;
            }
        } else if((_v = fude_look_value(_argv[_i], "--repeat")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_repeat, sizeof(sketching_repeat), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_repeat_at = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--size-card")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_size, sizeof(sketching_size), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_size_at = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--sheet")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_sheet, sizeof(sketching_sheet), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : strlen(_v)), _v);
            sketching_sheet_at = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--kanban-card")) != NULL) {
            sketching_kanban_card        = atoi(_v);
            sketching_kanban_card_at     = strchr(_v, '@') != NULL ? (u32)atoi(strchr(_v, '@') + 1) : 20u;
            sketching_kanban_card_extra  = strchr(_v, '+') != NULL;
            sketching_kanban_card_insert = strchr(_v, '!') != NULL;
        } else if((_v = fude_look_value(_argv[_i], "--kanban")) != NULL) {
            sketching_kanban    = atoi(_v);
            sketching_kanban_at = strchr(_v, '@') != NULL ? (u32)atoi(strchr(_v, '@') + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--census")) != NULL) {
            sketching_census = (u32)atoi(_v);
        } else if(fude_look_is(_argv[_i], "--demo-board") || fude_look_value(_argv[_i], "--demo-board") != NULL) {
            _v = fude_look_value(_argv[_i], "--demo-board");
            sketching_demo_board = _v != NULL ? atoi(_v) : 0;
            const c8* _at = _v != NULL ? strchr(_v, '@') : NULL;
            sketching_demo_board_at = _at != NULL ? (u32)atoi(_at + 1) : 10u;
        } else if(fude_look_is(_argv[_i], "--demo-sculpt") || fude_look_value(_argv[_i], "--demo-sculpt") != NULL) {
            _v = fude_look_value(_argv[_i], "--demo-sculpt");
            sketching_demo_sculpt = _v != NULL ? (u32)atoi(_v) : 1u;
        } else if((_v = fude_look_value(_argv[_i], "--loop-at")) != NULL) {
            u32 _frame = 0;
            f32* _l = sketching_loop_at[sketching_loops < 4u ? sketching_loops : 3u];
            if(sscanf(_v, "%f,%f,%f@%u", &_l[0], &_l[1], &_l[2], &_frame) == 4) {
                _l[3] = (f32)_frame;
                sketching_loops += sketching_loops < 4u ? 1u : 0u;
            }
        } else if((_v = fude_look_value(_argv[_i], "--path-at")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            sketching_path_frame = _at != NULL ? (u32)atoi(_at + 1) : 0u;
            for(const c8* _p = _v; _p != NULL && *_p != 0 && *_p != '@' && sketching_path_n < 16u;) {
                if(sscanf(_p, "%f,%f", &sketching_path[sketching_path_n][0], &sketching_path[sketching_path_n][1]) == 2) {
                    sketching_path_n++;
                }
                _p = strchr(_p, ';');
                _p = _p != NULL ? _p + 1 : NULL;
            }
        } else if((_v = fude_look_value(_argv[_i], "--pan-later")) != NULL) {
            sscanf(_v, "%f,%f@%f", &sketching_pan_later[0], &sketching_pan_later[1], &sketching_pan_later[2]);
        } else if((_v = fude_look_value(_argv[_i], "--handwrite")) != NULL) {
            sketching_handwrite = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--grip")) != NULL) {
            sscanf(_v, "%f,%f@%f", &sketching_grip[0], &sketching_grip[1], &sketching_grip[2]);
        } else if((_v = fude_look_value(_argv[_i], "--grow")) != NULL) {
            sscanf(_v, "%f,%f@%f", &sketching_grow[0], &sketching_grow[1], &sketching_grow[2]);
            sketching_grow[3] = strchr(_v, '!') != NULL ? 1.0f : 0.0f;
        } else if((_v = fude_look_value(_argv[_i], "--anchor")) != NULL) {
            sscanf(_v, "%f,%f,%f@%u", &sketching_anchor[0], &sketching_anchor[1], &sketching_anchor[2], &sketching_anchor_frame);
        } else if((_v = fude_look_value(_argv[_i], "--parts")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            if(_at != NULL) {
                snprintf(sketching_parts, sizeof(sketching_parts), "%.*s", (int)(_at - _v), _v);
                sketching_parts_frame = (u32)atoi(_at + 1);
                sketching_parts_cut   = strchr(_at, '!') != NULL;
            }
        } else if((_v = fude_look_value(_argv[_i], "--fit")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            if(_at != NULL) {
                snprintf(sketching_fit, sizeof(sketching_fit), "%.*s", (int)(_at - _v), _v);
                sketching_fit_frame = (u32)atoi(_at + 1);
            }
        } else if((_v = fude_look_value(_argv[_i], "--tap-at")) != NULL) {
            u32 _frame = 0;
            if(sscanf(_v, "%f,%f@%u", &sketching_tap_at[0], &sketching_tap_at[1], &_frame) == 3) {
                sketching_tap_at[2] = (f32)_frame;
            }
        } else if(fude_look_is(_argv[_i], "--home-view")) {
            sketching_home_view = true;
        } else if((_v = fude_look_value(_argv[_i], "--kerf")) != NULL) {
            sketching_kerf = (f32)atof(_v);
        } else if((_v = fude_look_value(_argv[_i], "--units")) != NULL) {
            sketching_units = atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--bar-tool")) != NULL) {
            sketching_bar_tool = atoi(_v);
        } else if(fude_look_is(_argv[_i], "--saw")) {
            sketching_saw = true;
        } else if((_v = fude_look_value(_argv[_i], "--holes")) != NULL) {
            sketching_holes = atof(_v);
        } else if((_v = fude_look_value(_argv[_i], "--to-curve")) != NULL) {
            sketching_to_curve = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--nodes")) != NULL) {
            sketching_nodes = atoi(_v);
        } else if(fude_look_is(_argv[_i], "--demo-curve")) {
            sketching_demo_curving = true;
        } else if((_v = fude_look_value(_argv[_i], "--offset")) != NULL) {
            sketching_offset = atof(_v);
        } else if(fude_look_is(_argv[_i], "--demo-layers") || fude_look_value(_argv[_i], "--demo-layers") != NULL) {
            _v = fude_look_value(_argv[_i], "--demo-layers");
            sketching_demo_layers = _v != NULL ? (u32)atoi(_v) : 1u;
        } else if(fude_look_is(_argv[_i], "--demo-bucket")) {
            sketching_demo_bucket = true;
        } else if((_v = fude_look_value(_argv[_i], "--arrange")) != NULL) {
            sketching_arrange = atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--mermaid")) != NULL) {
            snprintf(sketching_mermaid, sizeof(sketching_mermaid), "%s", _v);
        } else if(fude_look_is(_argv[_i], "--calibrate")) {
            sketching_calibrate = true;
        } else if((_v = fude_look_value(_argv[_i], "--numpad")) != NULL) {
            const c8* _at = strrchr(_v, '@');
            snprintf(sketching_numpad, sizeof(sketching_numpad), "%.*s", _at != NULL ? (int)(_at - _v) : (int)strlen(_v), _v);
            sketching_numpad_frame = _at != NULL ? (u32)atoi(_at + 1) : 20u;
        } else if((_v = fude_look_value(_argv[_i], "--measure")) != NULL) {
            sketching_measure = (u8)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--finger-drag")) != NULL) {
            f32* _f = sketching_finger_drag;
            if(sscanf(_v, "%f,%f,%f,%f,%f,%f@%u", &_f[0], &_f[1], &_f[2], &_f[3], &_f[4], &_f[5], &sketching_finger_frame) != 7) {
                sketching_finger_frame = 0;
            }
        } else if((_v = fude_look_value(_argv[_i], "--instruments")) != NULL) {
            snprintf(sketching_instruments, sizeof(sketching_instruments), "%s", _v);
        } else if((_v = fude_look_value(_argv[_i], "--instrument-at")) != NULL && sketching_instrument_placed < 4u) {
            f32* _p = sketching_instrument_at[sketching_instrument_placed];
            if(sscanf(_v, "%f,%f,%f,%f", &_p[0], &_p[1], &_p[2], &_p[3]) == 4) {
                _p[0] += 1.0f;
                sketching_instrument_placed++;
            }
        } else if((_v = fude_look_value(_argv[_i], "--clear-at")) != NULL) {
            sketching_clear_frame = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--undo-at")) != NULL || (_v = fude_look_value(_argv[_i], "--redo-at")) != NULL) {
            u32* _f = sketching_undo_frames[strncmp(_argv[_i], "--redo-at", 9) == 0 ? 1 : 0];
            for(u32 _k = 0; _k < 8u && *_v != 0; _k++) {
                _f[_k] = (u32)strtoul(_v, (c8**)&_v, 10);
                if(*_v == ',') {
                    _v++;
                }
            }
        } else if((_v = fude_look_value(_argv[_i], "--erase-at")) != NULL || (_v = fude_look_value(_argv[_i], "--draw-at")) != NULL) {
            sketching_erase_tool = strncmp(_argv[_i], "--draw-at", 9) == 0 ? FUDE_TOOL_DRAW : FUDE_TOOL_ERASE;
            if(sscanf(_v, "%f,%f,%f,%f@%u", &sketching_erase_line[0], &sketching_erase_line[1], &sketching_erase_line[2], &sketching_erase_line[3], &sketching_erase_frame) != 5) {
                sketching_erase_frame = 0;
            }
        } else if((_v = fude_look_value(_argv[_i], "--export-at")) != NULL) {
            sketching_export_at = (u32)atoi(_v);
        } else if((_v = fude_look_value(_argv[_i], "--export")) != NULL) {
            sketching_export = strncmp(_v, "svg:", 4) == 0 ? 1 : strncmp(_v, "pdf:", 4) == 0 ? 2 : strncmp(_v, "dxf:", 4) == 0 ? 3 : strncmp(_v, "print:", 6) == 0 ? 4 :
                               strncmp(_v, "check:", 6) == 0 ? 5 : strncmp(_v, "csv:", 4) == 0 ? 6 : strncmp(_v, "letter:", 7) == 0 ? 7 :
                               strncmp(_v, "mp4:", 4) == 0 ? 8 : strncmp(_v, "areas:", 6) == 0 ? 9 : strncmp(_v, "sheets:", 7) == 0 ? 10 : strncmp(_v, "stl:", 4) == 0 ? 11 : 0;
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
    fude_text_drop_taught_language();   // a drawing app teaches no language: no fourth (Japanese) in it
    fude_text_set_language(fude_text_default_language());
    fude_zoom_themes_use();      // the workshop's themes in place of the language apps' (Paper and Night kept)
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
    fude_zoom_page_topic_follow(&zoom, &ui);   // (a topic chosen: the bar's tools for it)
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
    if(sketching_instruments[0] != 0 && sketching_frames == 7u) {
        // (Those left out from before put away first: only the ones asked for.)
        for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
            if(zoom.instruments.tools[_k].shown) {
                fude_zoom_instrument_remove(&zoom.instruments, _k);
            }
        }
    }
    if(sketching_instruments[0] != 0 && sketching_frames == 8u) {
        for(const c8* _c = sketching_instruments; *_c != 0; _c++) {
            if(*_c >= '0' && *_c < '0' + (c8)FUDE_ZOOM_INSTRUMENT_COUNT) {
                fude_zoom_instrument_add(&zoom.instruments, (FUDE_ZOOM_INSTRUMENT_)(*_c - '0'), fude_zoom_page_half(&zoom));
            }
        }
        // Each placed: the first of its kind not placed yet (two rulers placed: both).
        b8 _placed[FUDE_ZOOM_INSTRUMENT_MOST] = { 0 };
        for(u32 _i = 0; _i < sketching_instrument_placed; _i++) {
            const f32* _p = sketching_instrument_at[_i];
            for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
                fude_zoom_instrument* _t = &zoom.instruments.tools[_k];
                if(_t->shown && !_placed[_k] && _t->kind == (u8)((u32)_p[0] - 1u)) {
                    _t->at      = (fude_zoom_v2){ _p[1], _p[2] };
                    _t->angle   = (f64)_p[3] * 3.14159265358979323846 / 180.0;
                    _t->stuck   = false;   // (lying there on the drawing from now)
                    _placed[_k] = true;
                    break;
                }
            }
        }
    }
    if(sketching_finger_frame > 0 && sketching_frames >= sketching_finger_frame && sketching_frames <= sketching_finger_frame + 21u) {
        // Down, ten steps to the second point, ten to the third, up: a frame each.
        const u32 _k = sketching_frames - sketching_finger_frame;
        const f32* _f = sketching_finger_drag;
        const f32  _t = _k <= 10u ? (f32)_k / 10.0f : (f32)(_k - 10u) / 10.0f;
        const f32  _x = _k <= 10u ? _f[0] + (_f[2] - _f[0]) * _t : _f[2] + (_f[4] - _f[2]) * fminf(_t, 1.0f);
        const f32  _y = _k <= 10u ? _f[1] + (_f[3] - _f[1]) * _t : _f[3] + (_f[5] - _f[3]) * fminf(_t, 1.0f);
        fude_zoom_page_look_finger(&zoom, _k == 0u ? 0u : (_k <= 20u ? 1u : 2u), (fude_zoom_v2){ _x, _y });   // (as the page takes a finger)
    }
    for(u32 _i = 0; _i < sketching_text_count; _i++) {
        f32 _x = 0.0f, _y = 0.0f;
        u32 _sticky = 0, _frame = 0;
        c8  _words[160] = { 0 };
        const c8* _at = strrchr(sketching_texts[_i], '@');
        if(sscanf(sketching_texts[_i], "%f,%f,%u,", &_x, &_y, &_sticky) == 3 && _at != NULL && (u32)atoi(_at + 1) == sketching_frames) {
            const c8* _w = sketching_texts[_i];
            for(u32 _c = 0; _c < 3u && _w != NULL; _c++) {
                _w = strchr(_w, ',');
                _w = _w != NULL ? _w + 1 : NULL;
            }
            if(_w != NULL) {
                snprintf(_words, sizeof(_words), "%.*s", (int)(_at - _w), _w);
                for(c8* _c = _words; *_c != 0; _c++) {
                    if(*_c == '|') { *_c = '\n'; }
                }
                fude_zoom_page_look_text(&zoom, (rde_vec_2F){ _x, _y }, _sticky != 0, _words);
            }
        }
        RDE_UNUSED(_frame);
    }
    if(sketching_demo_bucket && sketching_frames == 12u) {
        // Four strokes of a hand's, each its own, round a box; the last stops short of the first.
        const f32 _box[5][2] = { { -200, -150 }, { 200, -150 }, { 200, 150 }, { -200, 150 }, { -200, -142 } };
        for(u32 _side = 0; _side < 4u; _side++) {
            for(u32 _i = 0; _i <= 20u; _i++) {
                const f32 _t = (f32)_i / 20.0f;
                const f32 _x = _box[_side][0] + (_box[_side + 1u][0] - _box[_side][0]) * _t + 3.0f * sinf(_t * 9.0f);
                const f32 _y = _box[_side][1] + (_box[_side + 1u][1] - _box[_side][1]) * _t + 3.0f * cosf(_t * 7.0f);
                fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ _x, _y });
            }
            fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
        }
        for(u32 _i = 0; _i <= 40u; _i++) {
            const f32 _a = 6.2832f * (f32)_i / 40.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 80.0f + cosf(_a) * 45.0f, 40.0f + sinf(_a) * 45.0f });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_demo_layers && sketching_frames == 18u) {
        fude_zoom_page_look_layers(&zoom, 0u, 0u);
        fude_zoom_page_look_layers(&zoom, 1u, 0u);
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _a = 6.2832f * (f32)_i / 30.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 150.0f + cosf(_a) * 90.0f, -120.0f + sinf(_a) * 60.0f });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_demo_layers == 5u && sketching_frames == 22u) {
        fude_zoom_page_look_layers(&zoom, 7u, 0u);   // the new layer (on top, listed first) moved under the first
    }
    if(sketching_demo_layers == 1u && sketching_frames == 22u) {
        fude_zoom_page_look_layers(&zoom, 2u, 0u);   // the first layer hidden
    }
    if(sketching_demo_layers >= 2u && sketching_demo_layers <= 4u && sketching_frames == 22u) {
        fude_zoom_page_look_layers(&zoom, 4u, 1u);   // everything onto the second
    }
    if(sketching_demo_layers >= 3u && sketching_demo_layers <= 4u && sketching_frames == 26u) {
        fude_zoom_page_look_layers(&zoom, 0u, 0u);
        fude_zoom_page_look_layers(&zoom, 5u, 1u);   // chosen,
        fude_zoom_page_look_layers(&zoom, 5u, 1u);   // then its name on the card
    }
    if(sketching_demo_layers == 4u && sketching_frames == 30u) {
        fude_zoom_page_look_rename(&zoom, "Cortes CNC");   // and saved as
    }
    if(sketching_demo_bucket && sketching_frames == 16u) {
        zoom.fill_tool = true;
        fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ -60.0f, -40.0f });
        zoom.fill_tool = false;
    }
    if(sketching_joint_frame > 0 && sketching_frames == sketching_joint_frame) {
        fude_zoom_page_look_joint(&zoom, (u8)sketching_joint[0], sketching_joint[1], sketching_joint[2], sketching_joint[3]);
    }
    if(sketching_offset_frame > 0 && sketching_frames == sketching_offset_frame) {
        fude_zoom_page_look_grain(&zoom);
    }
    if(sketching_loop_frame > 0 && sketching_frames == sketching_loop_frame) {
        fude_zoom_page_look_loop(&zoom, (rde_vec_2F){ sketching_loop[0], sketching_loop[1] }, (rde_vec_2F){ sketching_loop[2], sketching_loop[3] });
    }
    if(sketching_pdf_frame > 0 && sketching_frames == sketching_pdf_frame) {
        const c8* _paths[1] = { sketching_pdf };
        FUDE_ZOOM_PAGE_KIND.imported(&app, 0u, _paths, 1u);   // (as Files would hand it over)
    }
    if(sketching_find_frame > 0 && sketching_frames == sketching_find_frame) {
        fude_zoom_page_look_find(&zoom, sketching_find);
    }
    if(sketching_demo_sculpt > 0 && sketching_frames == 10u) {
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_DRAW);
        zoom.tool_seen      = (u8)FUDE_TOOL_DRAW;
        zoom.tool_taps_seen = ui.bar.tool_taps;
        zoom.smooth_level   = 0u;
        for(u32 _i = 0; _i <= 40u; _i++) {
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ -300.0f + 15.0f * (f32)_i, -40.0f });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_demo_sculpt == 3u && sketching_frames == 14u) {
        // Nudge: the pen from under the line's middle up through it, pushing it along.
        zoom.nudge = true;
        for(u32 _i = 0; _i <= 30u; _i++) {
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ 0.0f, -70.0f + 4.0f * (f32)_i });
        }
        if(sketching_frames == 14u) {
            fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
        }
    }
    if(sketching_demo_sculpt > 0 && sketching_demo_sculpt < 3u && sketching_frames == 14u) {
        zoom.sculpt = sketching_demo_sculpt == 1u;
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _t = (f32)_i / 30.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ -100.0f + 200.0f * _t, -40.0f + 110.0f * sinf(3.14159265f * _t) });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    for(u32 _l = 0; _l < sketching_loops; _l++) {
        const f32* _at = sketching_loop_at[_l];
        if(sketching_frames != (u32)_at[3]) {
            continue;
        }
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_DRAW);
        zoom.tool_seen      = (u8)FUDE_TOOL_DRAW;
        zoom.tool_taps_seen = ui.bar.tool_taps;
        for(u32 _i = 0; _i <= 48u; _i++) {
            const f32 _a = 6.2831853f * (f32)_i / 48.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ _at[0] + cosf(_a) * _at[2], _at[1] + sinf(_a) * _at[2] });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_tap_at[2] > 0.0f && sketching_frames == (u32)sketching_tap_at[2]) {
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_LASSO);
        zoom.tool_seen      = (u8)FUDE_TOOL_LASSO;
        zoom.tool_taps_seen = ui.bar.tool_taps;
        fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ sketching_tap_at[0], sketching_tap_at[1] });
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ sketching_tap_at[0], sketching_tap_at[1] });
    }
    if(sketching_home_view && sketching_frames == 2u) {
        fude_zoom_camera_look_at(&zoom.scene, zoom.scene.home != FUDE_ZOOM_NONE ? zoom.scene.home : zoom.scene.root, (fude_zoom_v2){ 0.0, 0.0 }, 1.0);
    }
    if(sketching_saw && sketching_frames == 4u) {
        zoom.saw = true;
        fude_toolbar_refresh(&ui.bar);
    }
    if(sketching_kerf >= 0.0f && sketching_frames == 4u) {
        zoom.kerf_mm = sketching_kerf;
    }
    if(sketching_units >= 0 && sketching_units < (i32)FUDE_ZOOM_UNIT_COUNT && sketching_frames == 4u) {
        zoom.units.unit = (FUDE_ZOOM_UNIT_)sketching_units;
    }
    if(sketching_bar_tool >= 0 && sketching_frames == 8u) {
        fude_toolbar_set_tool(&ui.bar, (FUDE_TOOL_)sketching_bar_tool);
    }
    if(sketching_holes > 0.0 && sketching_frames == 10u) {
        fude_zoom_page_look_holes(&zoom, sketching_holes);
    }
    if(sketching_to_curve > 0 && sketching_frames == sketching_to_curve) {
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_LASSO);
        fude_zoom_page_look_to_curve(&zoom);
    }
    if(sketching_nodes >= 0 && sketching_frames == 18u) {
        fude_zoom_page_look_nodes(&zoom, 0u);
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_LASSO);
    }
    if(sketching_nodes >= 1 && sketching_frames == 20u) {
        fude_zoom_page_look_nodes(&zoom, (u8)sketching_nodes);
    }
    if(sketching_path_frame > 0u && sketching_frames == sketching_path_frame && sketching_path_n >= 2u) {
        // Down at the first, ten steps to each next, up at the last.
        fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ sketching_path[0][0], sketching_path[0][1] });
        for(u32 _i = 1; _i < sketching_path_n; _i++) {
            for(u32 _s = 1; _s <= 10u; _s++) {
                const f32 _t = (f32)_s / 10.0f;
                fude_zoom_page_look_pen(&zoom, 1u, (rde_vec_2F){ sketching_path[_i - 1u][0] + (sketching_path[_i][0] - sketching_path[_i - 1u][0]) * _t,
                                                                  sketching_path[_i - 1u][1] + (sketching_path[_i][1] - sketching_path[_i - 1u][1]) * _t });
            }
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_pan_later[2] > 0.0f && sketching_frames == (u32)sketching_pan_later[2]) {
        fude_zoom_camera_pan(&zoom.scene, (fude_zoom_v2){ sketching_pan_later[0], sketching_pan_later[1] });
    }
    if(sketching_handwrite > 0u && sketching_frames == sketching_handwrite) {
        fude_zoom_page_look_handwrite(&zoom);
    }
    if(sketching_grip[2] > 0.0f && sketching_frames == (u32)sketching_grip[2]) {
        fude_zoom_page_look_grip(&zoom, sketching_grip[0], sketching_grip[1]);
    }
    if(sketching_grow[2] > 0.0f && sketching_frames == (u32)sketching_grow[2]) {
        fude_zoom_page_look_grow(&zoom, (u32)sketching_grow[0], (f64)sketching_grow[1], sketching_grow[3] > 0.5f);
    }
    if(sketching_anchor_frame > 0u && sketching_frames == sketching_anchor_frame) {
        fude_zoom_page_look_anchor(&zoom, (u32)sketching_anchor[0], (f64)sketching_anchor[1], (f64)sketching_anchor[2]);
    }
    if(sketching_parts_frame > 0u && sketching_frames == sketching_parts_frame) {
        fude_zoom_parts_look(&zoom.parts_form, sketching_parts, sketching_parts_cut);
    }
    if(sketching_fit_frame > 0u && sketching_frames == sketching_fit_frame) {
        fude_zoom_page_look_fit(&zoom, sketching_fit);
    }
    if(sketching_fillet_mm >= 0.0 && sketching_frames == sketching_fillet_at) {
        fude_zoom_page_look_fillet(&zoom, sketching_fillet_mm);
    }
    if(sketching_library >= 0 && sketching_frames == sketching_library_at) {
        fude_zoom_page_look_library(&zoom, (u8)sketching_library);
    }
    if(sketching_kanban >= 0 && sketching_frames == sketching_kanban_at) {
        fude_zoom_page_look_kanban(&zoom, (u32)sketching_kanban);
    }
    if(sketching_area_at > 0u && sketching_frames == sketching_area_at) {
        fude_zoom_page_look_area(&zoom, sketching_area);
    }
    if(sketching_map_at > 0u && sketching_frames == sketching_map_at) {
        fude_zoom_page_look_map(&zoom);
    }
    if(sketching_present >= 0 && sketching_frames == sketching_present_at) {
        fude_zoom_page_look_present(&zoom, (u32)sketching_present);
    }
    if(sketching_laser_at > 0u && sketching_frames == sketching_laser_at) {
        fude_zoom_page_look_laser(&zoom);
    }
    if(sketching_keep_at > 0u && sketching_frames == sketching_keep_at) {
        fude_zoom_page_look_keep(&zoom, sketching_keep);
    }
    if(sketching_pieces_at > 0u && sketching_frames == sketching_pieces_at) {
        fude_zoom_page_look_pieces(&zoom);
    }
    if(sketching_stencil[1] > 0u && sketching_frames == sketching_stencil[1]) {
        fude_zoom_page_look_stencil(&zoom, sketching_stencil[0]);
    }
    if(sketching_piece_put[1] > 0u && sketching_frames == sketching_piece_put[1]) {
        fude_zoom_page_look_piece_put(&zoom, sketching_piece_put[0]);
    }
    if(sketching_canvas[1] > 0u && sketching_frames == sketching_canvas[1]) {
        notes.open = sketching_canvas[0];   // (switched to by the session's next look)
    }
    for(u32 _ti = 0; _ti < sketching_pen_tap_count; _ti++) {
        const f32* _pt = sketching_pen_taps[_ti];
        if(_pt[2] > 0.0f && sketching_frames == (u32)_pt[2]) {
            fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ _pt[0], _pt[1] });
            fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ _pt[0], _pt[1] });
        }
    }
    for(u32 _di = 0; _di < sketching_pen_drag_count; _di++) {
        const f32* _pd = sketching_pen_drags[_di];
        if(_pd[4] > 0.0f && sketching_frames >= (u32)_pd[4] && sketching_frames <= (u32)_pd[4] + 11u) {
            const u32 _k = sketching_frames - (u32)_pd[4];
            const f32 _t = (f32)(_k < 10u ? _k : 10u) / 10.0f;
            const rde_vec_2F _p = { _pd[0] + (_pd[2] - _pd[0]) * _t, _pd[1] + (_pd[3] - _pd[1]) * _t };
            fude_zoom_page_look_pen(&zoom, _k == 0u ? 0u : (_k <= 10u ? 1u : 2u), _p);
        }
    }
    if(sketching_kanban_card >= 0 && sketching_frames == sketching_kanban_card_at) {
        fude_zoom_page_look_kanban_card(&zoom, (u32)sketching_kanban_card, sketching_kanban_card_extra, sketching_kanban_card_insert);
    }
    for(u32 _si = 0; _si < sketching_shape_tool_count; _si++) {
        if(sketching_shape_tools[_si][1] > 0u && sketching_frames == sketching_shape_tools[_si][1]) {
            fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_DRAW);
            zoom.tool_seen      = (u8)FUDE_TOOL_DRAW;
            zoom.tool_taps_seen = ui.bar.tool_taps;
            zoom.shape_tool     = (u8)sketching_shape_tools[_si][0];
            zoom.measure_tool   = 0;
        }
    }
    if(sketching_repeat_at > 0u && sketching_frames == sketching_repeat_at) {
        fude_zoom_page_look_repeat(&zoom, sketching_repeat);
    }
    for(u32 _ci = 0; _ci < sketching_shapes_choice_count; _ci++) {
        if(sketching_frames == sketching_shapes_choices[_ci][1]) {
            fude_zoom_page_look_shapes_choice(&zoom, sketching_shapes_choices[_ci][0]);
        }
    }
    for(u32 _pl = 0; _pl < sketching_plan_count; _pl++) {
        if(sketching_frames == sketching_plan[_pl][1]) {
            fude_zoom_page_look_plan(&zoom, sketching_plan[_pl][0]);
        }
    }
    if(sketching_mech[1] > 0u && sketching_frames == sketching_mech[1]) {
        fude_zoom_page_look_mech(&zoom, sketching_mech[0]);
    }
    if(sketching_mech_run > 0u && sketching_frames == sketching_mech_run) {
        fude_zoom_page_look_mech_run(&zoom);
    }
    if(sketching_circuit[1] > 0u && sketching_frames == sketching_circuit[1]) {
        fude_zoom_page_look_circuit(&zoom, sketching_circuit[0]);
    }
    if(sketching_circuit_run > 0u && sketching_frames == sketching_circuit_run) {
        fude_zoom_page_look_circuit_run(&zoom);
    }
    if(sketching_value_frame > 0u && sketching_frames == sketching_value_frame) {
        c8 _id[48] = { 0 }, _typed[24] = { 0 };
        u32 _unit = 0, _way = 0;
        const c8* _c1 = strchr(sketching_value, ',');
        const c8* _c2 = _c1 != NULL ? strchr(_c1 + 1, ',') : NULL;
        if(_c2 != NULL) {
            snprintf(_id, sizeof(_id), "%.*s", (int)(_c1 - sketching_value), sketching_value);
            snprintf(_typed, sizeof(_typed), "%.*s", (int)(_c2 - _c1 - 1), _c1 + 1);
            sscanf(_c2 + 1, "%u,%u", &_unit, &_way);
            fude_zoom_page_look_value(&zoom, _id, _typed, _unit, _way);
        }
    }
    if(sketching_align_frame > 0u && sketching_frames == sketching_align_frame) {
        c8 _id[40] = { 0 };
        f64 _dx = 0.0, _dy = 0.0;
        const c8* _c1 = strchr(sketching_align, ',');
        if(_c1 != NULL && sscanf(_c1 + 1, "%lf,%lf", &_dx, &_dy) == 2) {
            snprintf(_id, sizeof(_id), "%.*s", (int)(_c1 - sketching_align), sketching_align);
            fude_zoom_page_look_align(&zoom, _id, _dx, _dy);
        }
    }
    if(sketching_context[2] > 0.0f && sketching_frames == (u32)sketching_context[2]) {
        fude_zoom_page_look_context(&zoom, (rde_vec_2F){ sketching_context[0], sketching_context[1] }, false);
    }
    if(sketching_context_delete > 0u && sketching_frames == sketching_context_delete) {
        fude_zoom_page_look_context(&zoom, (rde_vec_2F){ 0.0f, 0.0f }, true);
    }
    if(sketching_make_part > 0u && sketching_frames == sketching_make_part) {
        fude_zoom_page_look_make_part(&zoom);
    }
    if(sketching_circuit2[1] > 0u && sketching_frames == sketching_circuit2[1]) {
        fude_zoom_page_look_circuit(&zoom, sketching_circuit2[0]);
    }
    if(sketching_body[4] > 0u && sketching_frames == sketching_body[4]) {
        fude_zoom_page_look_body(&zoom, sketching_body[0] != 0u, sketching_body[1], sketching_body[2] != 0u, sketching_body[3] != 0u);
    }
    if(sketching_slider[1] > 0u && sketching_frames == sketching_slider[1]) {
        fude_zoom_page_look_slider(&zoom, sketching_slider[0], sketching_slider_t);
    }
    if(sketching_combine[1] > 0u && sketching_frames == sketching_combine[1]) {
        fude_zoom_page_look_combine(&zoom, sketching_combine[0]);
    }
    if(sketching_lines[1] > 0u && sketching_frames == sketching_lines[1]) {
        fude_zoom_page_look_lines(&zoom, sketching_lines[0]);
    }
    for(u32 _pi = 0; _pi < sketching_point_count; _pi++) {
        if(sketching_frames == sketching_point_at[_pi]) {
            fude_zoom_page_look_point(&zoom, sketching_points[_pi]);
        }
    }
    if(sketching_topic[1] > 0u && sketching_frames == sketching_topic[1]) {
        zoom.topic = (u8)sketching_topic[0];
    }
    if(sketching_line_style[1] > 0u && sketching_frames == sketching_line_style[1]) {
        zoom.line_style = (u8)sketching_line_style[0];
    }
    if(sketching_measure_tool[1] > 0u && sketching_frames == sketching_measure_tool[1]) {
        fude_toolbar_set_tool(&ui.bar, FUDE_TOOL_DRAW);
        zoom.tool_seen      = (u8)FUDE_TOOL_DRAW;
        zoom.tool_taps_seen = ui.bar.tool_taps;
        zoom.shape_tool     = 0;
        zoom.measure_tool   = (u8)sketching_measure_tool[0];
    }
    if(sketching_size_at > 0u && sketching_frames == sketching_size_at) {
        fude_zoom_page_look_size(&zoom, sketching_size);
    }
    if(sketching_sheet_at > 0u && sketching_frames == sketching_sheet_at) {
        fude_zoom_page_look_sheet(&zoom, sketching_sheet);
    }
    if(sketching_tidy_at > 0 && sketching_frames == sketching_tidy_at) {
        fude_zoom_page_look_tidy(&zoom);
    }
    for(u32 _k = 0; _k < sketching_symbol_count; _k++) {
        if(sketching_frames == sketching_symbol_at[_k]) {
            c8 _id[64] = { 0 };
            f64 _dx = 0.0, _dy = 0.0;
            const c8* _p = sketching_symbols[_k];
            const c8* _c1 = strchr(_p, ',');
            const c8* _c2 = _c1 != NULL ? strchr(_c1 + 1, ',') : NULL;
            const c8* _c3 = _c2 != NULL ? strchr(_c2 + 1, ',') : NULL;
            snprintf(_id, sizeof(_id), "%.*s", _c1 != NULL ? (int)(_c1 - _p) : (int)strlen(_p), _p);
            _dx = _c1 != NULL ? atof(_c1 + 1) : 0.0;
            _dy = _c2 != NULL ? atof(_c2 + 1) : 0.0;
            fude_zoom_page_look_symbol(&zoom, _id, _c3 != NULL ? _c3 + 1 : "", _dx, _dy);
        }
    }
    if(sketching_census > 0 && sketching_frames == sketching_census) {
        static const c8* const _types[] = { "?", "line", "rect", "ellipse", "polygon", "dimension", "arrow", "board", "path", "arc", "symbol", "sheet" };
        u32 _strokes = 0, _texts = 0, _fills = 0, _shapes[sizeof(_types) / sizeof(_types[0])] = { 0 };
        for(u32 _i = 0; _i < fude_zoom_scene_object_count(&zoom.scene); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(&zoom.scene, _i);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            _strokes += _o->kind == FUDE_ZOOM_KIND_STROKE ? 1u : 0u;
            _texts   += _o->kind == FUDE_ZOOM_KIND_TEXT ? 1u : 0u;
            _fills   += _o->kind == FUDE_ZOOM_KIND_FILL ? 1u : 0u;
            if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                rde_log_color(RDE_LOG_COLOR_GREEN, "sketching: census: stroke %u: %u points, box %.1f %.1f %.1f %.1f, flags %u", _i, _o->count,
                              _o->box.min_x, _o->box.min_y, _o->box.max_x, _o->box.max_y, (u32)_o->flags);
                fude_zoom_qpoint _q[48];
                if(_o->count <= 48u && fude_zoom_scene_points(&zoom.scene, _i, _q)) {
                    for(u32 _p = 0; _p < _o->count; _p++) {
                        const fude_zoom_v2 _at = fude_zoom_scene_point_at(_o, &_q[_p]);
                        rde_log_color(RDE_LOG_COLOR_GREEN, "sketching: census:   %.2f %.2f r %.2f", _at.x, _at.y, (f64)fude_zoom_scene_radius_at(_o, &_q[_p]));
                    }
                }
            }
            if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                _shapes[_o->channels < sizeof(_types) / sizeof(_types[0]) ? _o->channels : 0u]++;
            }
        }
        c8 _said[256];
        usize _at = (usize)snprintf(_said, sizeof(_said), "%u strokes, %u texts, %u fills", _strokes, _texts, _fills);
        for(u32 _t = 1; _t < sizeof(_types) / sizeof(_types[0]) && _at < sizeof(_said); _t++) {
            if(_shapes[_t] > 0) {
                _at += (usize)snprintf(_said + _at, sizeof(_said) - _at, ", %u %s", _shapes[_t], _types[_t]);
            }
        }
        rde_log_color(RDE_LOG_COLOR_GREEN, "sketching: census: %s", _said);
    }
    if(sketching_demo_board >= 0 && sketching_frames == sketching_demo_board_at) {
        fude_zoom_page_look_board(&zoom, (u8)sketching_demo_board);
    }
    if(sketching_demo_drive >= 1u && sketching_frames == 10u) {
        fude_zoom_page_look_drive(&zoom, 0u);
    }
    if(sketching_demo_drive >= 2u && sketching_frames == 14u) {
        fude_zoom_page_look_drive(&zoom, 1u);
    }
    if(sketching_demo_curving && sketching_frames == 14u) {
        zoom.curve_tool = true;
        const f32 _pts[6][2] = { { -300, 0 }, { -150, 150 }, { 0, -100 }, { 150, 150 }, { 300, 0 }, { 300, 0 } };
        for(u32 _i = 0; _i < 6u; _i++) {
            fude_zoom_page_look_pen(&zoom, 0u, (rde_vec_2F){ _pts[_i][0], _pts[_i][1] });
            fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ _pts[_i][0], _pts[_i][1] });
        }
        zoom.curve_tool = false;
    }
    if(sketching_offset != 0.0 && sketching_frames == 20u) {
        FUDE_ZOOM_PAGE_KIND.command(&app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f });
        fude_zoom_page_look_offset(&zoom, sketching_offset);
        FUDE_ZOOM_PAGE_KIND.command(&app, FUDE_PAGE_CMD_DESELECT, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_arrange >= 0 && sketching_frames == 20u) {
        FUDE_ZOOM_PAGE_KIND.command(&app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_arrange >= 0 && sketching_frames == 22u) {
        fude_zoom_select_arrange(&zoom.selection, &zoom.scene, (FUDE_ZOOM_ARRANGE_)sketching_arrange);
    }
    if(sketching_mermaid[0] != 0 && sketching_frames == 15u) {
        FILE* _f = fopen(sketching_mermaid, "rb");
        if(_f != NULL) {
            static c8 _text[16384];
            const usize _n = fread(_text, 1, sizeof(_text) - 1u, _f);
            _text[_n] = 0;
            fclose(_f);
            fude_zoom_page_look_mermaid(&zoom, _text);
        }
    }
    if(sketching_calibrate && sketching_frames == 10u) {
        fude_zoom_page_look_calibrate(&zoom);
    }
    if(sketching_numpad_frame > 0 && sketching_frames == sketching_numpad_frame) {
        fude_zoom_page_look_numpad(&zoom, sketching_numpad);
    }
    if(sketching_clear_frame > 0 && sketching_frames == sketching_clear_frame) {
        FUDE_ZOOM_PAGE_KIND.clear(&app);
    }
    for(u32 _k = 0; _k < 8u; _k++) {
        if(sketching_undo_frames[0][_k] > 0 && sketching_frames == sketching_undo_frames[0][_k]) {
            FUDE_ZOOM_PAGE_KIND.undo(&app);
        }
        if(sketching_undo_frames[1][_k] > 0 && sketching_frames == sketching_undo_frames[1][_k]) {
            FUDE_ZOOM_PAGE_KIND.redo(&app);
        }
    }
    if(sketching_erase_frame > 0 && sketching_frames == sketching_erase_frame) {
        fude_toolbar_set_tool(&ui.bar, sketching_erase_tool);
        zoom.tool_seen      = (u8)sketching_erase_tool;
        zoom.tool_taps_seen = ui.bar.tool_taps;
        zoom.measure_tool   = sketching_measure;
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _t = (f32)_i / 30.0f;
            fude_zoom_page_look_pen(&zoom, _i == 0 ? 0u : 1u, (rde_vec_2F){ sketching_erase_line[0] + (sketching_erase_line[2] - sketching_erase_line[0]) * _t,
                                                                          sketching_erase_line[1] + (sketching_erase_line[3] - sketching_erase_line[1]) * _t });
        }
        fude_zoom_page_look_pen(&zoom, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(sketching_export >= 0 && sketching_frames == sketching_export_at) {
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
