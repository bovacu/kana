// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/look.h"

#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/look.h"
#include "drawing/app/toolbar.h"
#include "drawing/app/import.h"
#include "drawing/base/kfile.h"
#include "drawing/base/text.h"
#include "zoom/smooth.h"

// ===========================================================================
// See look.h. The flags, each with what it does. Flags not listed here are described beside their variables below.
//
//   --demo-deep=N   flowers drawn at N depths, each 1024× inside the last, with the camera left at the deepest, so a
//                   shot shows every level at once
//   --zoom=F        the camera's zoom multiplied by F once loaded (0.001 zooms out, 1000 in)
//   --zoom-later=F[@N]  the same a few frames in (at frame N, or 6 when it is left out), after what the first update
//                   brings (a --demo-picture) has landed, or after a demo drew
//   --paper-is=K    the canvas's paper: dots, lines, squares or none (or its number, canvas.h's FUDE_PAPER_)
//   --paper-later=K@N         the same at frame N, after a canvas opened at --canvas's frame, replacing its own paper
//   --pan=X,Y       the page dragged X, Y screen units; before a --zoom, to park the camera on a stroke's edge and
//                   look from inside it
//   --demo-erase    lines crossed and erased with each of the three erasers
//   --gate=POINTS   a canvas of that many points over four depths, for --perf
//   --demo-lasso[=2|3] the left half looped and dragged; with 2 also turned, with 3 turned 37° and held (the
//                   selection's looks)
//   --demo-snap     a rough circle, rectangle and line, each held till it snaps
//   --demo-picture=FILE  a picture brought in as Files would (JPEG, PNG; HEIC on a Mac)
//   --demo-shapes   the Shapes tool's rectangle, ellipse, triangle and line, dragged out
//   --shapes-panel  the Shapes tool's choices, open
//   --smoothing-panel the Smoothing tool's levels, open
//   --demo-smooth   one shaky wave drawn at each smoothing level, top to bottom:
//                   Off, Low, Medium, High, Rope
//   --demo-fill     a hand's closed loop, a rectangle, a figure 8 and an open arc, each tapped with the Fill tool (the
//                   open arc shows the notice)
//   --export=KIND:PATH  at frame 20 (or --export-at's), the view exported to PATH as Export does. KIND is png, svg,
//                   pdf, dxf, print, check, csv, letter, areas, sheets, stl, or mp4 (the zoom video, made over the
//                   frames after)
//   --erase-at=X1,Y1,X2,Y2@N  at frame N, the eraser swept from one point to the other (screen units)
//   --draw-at=X1,Y1,X2,Y2@N   the same with the pen drawing instead
//   --pages=K@N               at frame N, Pages' choice K (0 A4, 1 A5, 2 Letter, 3 square, 4 a page more, 5 the
//                             endless canvas)
//   --instruments=K,K...      those instruments brought out at frame 8 (0 ruler, 1 45° set square, 2 30°/60°, 3
//                             protractor, 4 compass; a kind given twice brings two)
//   --instrument-at=K,X,Y,DEG and that one put there, turned DEG degrees
//   --finger-drag=X1,Y1,X2,Y2,X3,Y3@N  a finger on the instruments from one point through another to a third, from
//                             frame N on
//   --measure=1|2             the pen measures: the tape (1) or a dimension (2), with --draw-at
//   --numpad=TEXT@N           the numpad open at frame N with TEXT in it, on the last line drawn
//   --text=X,Y,S,WORDS@N      a text (S 0) or a sticky note (S 1) saying WORDS ("|" for a line break) at frame N;
//                             with S 4 or 5 it is left being written on the page
//   --calibrate               the true size's calibration at frame 10
//   --mermaid=FILE            a diagram from that file's Mermaid text at frame 15
//   --arrange=K               everything selected at frame 20, then arranged that way (select.h's FUDE_ZOOM_ARRANGE_)
//   --demo-bucket             four lines round a box (a small gap at a corner) and a ring inside, then the Fill tool
//                             tapped inside
//   --demo-layers             the layers' panel, a new layer drawn on, and the first layer hidden
//   --offset=MM               everything selected at frame 20, then offset by MM (selected as --arrange does)
//   --demo-curve              the Curve tool: five points tapped, then the last again, making a wave
//   --demo-curve-open         the Curve tool: three points tapped and left in hand, not finished
//   --switch-topic=N@F        topic N chosen from the Topic menu at frame F (all the last topic's tools put down)
//   --find=WORDS@N            Find for WORDS at frame N
//   --pdf=PATH@N              at frame N, the PDF at PATH picked (Insert → PDF), making a canvas over it
//   --loop-at=X1,Y1,X2,Y2@N   at frame N, the Lasso round that screen box; a PDF's text in it is logged
//   --loop-at=X,Y,R@N         at frame N, a round loop drawn with the pen (up to four; with --saw, a cut)
//   --offset-at=N             at frame N, the selection's Offset pressed (for a board alone, its grain is turned)
//   --joint=K,A,B,C@N         at frame N, a joint put down (K 1 a finger joint, 2 a dovetail; then the length, the
//                             width or the tails, and the depth)
//   --fillet=MM@N             at frame N, two lines meeting in a corner lassoed, then that corner rounded MM mm (0
//                             leaves them lassoed)
//   --example=E@N             at frame N, example E (examples.h) drawn on the canvas open
//   --fps                     the frame rate shown at the top right, as Settings' Display can show it
//   --library=F@N             at frame N, the diagram library's panel open on family F (0 flowchart ... 6 planning)
//   --strip=N                 at frame N, a notebook's pages strip open
//   --strip-do=W,I,J@N        at frame N, on the strip's page I: held with its buttons up (W 0), carried over place J
//                             (1), a page put in before I (2), I deleted (3), or I made J (4)
//   --symbol=ID,DX,DY,TEXT@N  at frame N, the symbol ID (symbol.c's ids) put DX, DY from the middle with TEXT ("|" for
//                             a new line)
//   --value=ID,TEXT,UNIT,WAY@N  at frame N, the first symbol ID lassoed alone and given TEXT in its UNIT (Value's card)
//   --slider=I,PERCENT@N  at frame N, Play's slider I set PERCENT of the way along
//   --make-part@N         at frame N, what the last demo drew lassoed and made a part (Make part)
//   --align=ID,DX,DY@N    at frame N, the first symbol ID lassoed alone and dragged DX, DY points, then held so it
//                         aligns
//   --body=ALL,MATERIAL,FIXED,APPLY@N  at frame N, Make body's card on the lasso (ALL 1 lassoes everything first),
//                         with material MATERIAL (0 wood … 7 foam), FIXED 1 for the ground, and APPLY 1 to apply it
//   --kanban=P@N              at frame N, a Kanban board of preset P (0 Kanban, 1 Scrum, 2 retrospective, 3 week)
//   --kanban-card=P[+][!]@N   at frame N, Insert's Kanban card up with template P in its rows ("+" adds a row; "!"
//                             then presses Insert)
//   --map@N                   at frame N, the map's panel up
//   --area=NAME@N             at frame N, an area named NAME round the view (or round what the lasso holds)
//   --present=S@N             at frame N, presenting, at step S (0 is the first)
//   --laser@N                 at frame N, a laser's trail across the screen while presenting
//   --keep=NAME@N             at frame N, what the lasso holds kept as a piece named NAME (My pieces)
//   --pieces@N                at frame N, My pieces' panel up
//   --custom=I,A[,NAME]@N     at frame N, the library's Custom tile I: A 0 puts it down, 1 opens its card, 2 presses
//                             Edit/Inside (its template), 3 renames it NAME, 4 deletes it (in the panel open, else the
//                             circuits' panel)
//   --inside@N                at frame N, the lasso's Inside pressed (for a custom part alone, its template)
//   --back@N                  at frame N, the page's Back pressed (a panel put away, or a template back where it came
//                             from)
//   --piece-put=I@N           at frame N, piece I put in the middle of the view
//   --stencil=I@N             at frame N, piece I laid as a stencil (an instrument)
//   --canvas=C@N              at frame N, canvas C opened (its notes.h id), as the side panel opens one
//   --canvas-new=NAME@N       at frame N, a new canvas with that name made and opened
//   --audio=W,V@N             at frame N: a made-up recording V seconds long linked to what is there (W 0), its panel
//                             (1), the newest recording at V ms (2), the microphone's chip (3); and for speech: the
//                             model downloaded (4), the newest transcribed (5), written as text (6) or as handwriting
//                             (7), and Live text on (8)
//   --audio-import=WAV@N      at frame N, a recording made of that WAV (16 kHz, mono, 16-bit)
//   --crash-now=N             at frame N, a real crash, leaving its report (--crash-card shows its card in a look
//                             all the same)
//   --home=N                  at frame N, the home screen open (--welcome=N: the app's welcome over it, or the
//                             workshop's in an app that has none)
//   --pen-tap=X,Y@N           at frame N, the pen down and up at X,Y (screen units, centre origin, Y up), with the
//                             tool as it is
//   --pen-drag=X0,Y0,X1,Y1@N  at frame N, the pen down at X0,Y0, moved to X1,Y1 over ten frames, then up
//   --census=N                at frame N, what is alive on the canvas logged: strokes, texts, fills, and shapes by
//                             type (line, arc...)
//   --clear-at=N    at frame N, Clear pressed as the toolbar does
//   --undo-at=N[,M...]  at those frames, Undo pressed (--redo-at presses Redo)
//   --brush-at=X,Y[,erase|mark]  the brush's circle there (screen units), as if the pen hovered, with that tool in
//                   hand
//   --demo-places   two views marked, with the page moved between them, then Places opened
//   --demo-loop     the Lasso's loop drawn with the "pen" over nothing, and lifted
//   --fly-home      at frame 10, a flight home as Reset view does (then Back shows)
//   --depth-levels  at frame 20, the depth's levels opened (use with a deep --zoom)
//   --stress        drawing without pause: a stroke a frame and, now and then, an eraser sweep, an undo, or a zoom in
//                   or out (the kill test's load)
//   --stress-add    the same with strokes and zooms only, so the count only grows
//
// The file's own names start with look_ (LOOK_ for its tables). What the flags ask is read once, by
// fude_zoom_look_start (the readers look_read_*), and kept in the statics below; each frame's look_frame_* parts then
// act on what is due at that frame, always in the same order, so that a shot is the same on every run.
// ===========================================================================

// What the looks act on: the shell's app, deep-zoom page and home screen (fude_zoom_look_start), and whether any
// argument was given (without one, nothing here runs).
RDE_INTERNAL fude_app*       look_app   = NULL;
RDE_INTERNAL fude_zoom_page* look_page  = NULL;
RDE_INTERNAL fude_zoom_home* look_home  = NULL;
RDE_INTERNAL b8              look_given = false;

// The demos' six colours: red, yellow, green, blue, purple, orange.
RDE_INTERNAL const rde_color LOOK_DEMO_COLORS[] = {
    { 231, 76, 60, 255 }, { 241, 196, 15, 255 }, { 46, 204, 113, 255 }, { 52, 152, 219, 255 }, { 155, 89, 182, 255 }, { 230, 126, 34, 255 },
};

// Draws a rose curve of _petals petals in the camera's frame, round _c and _r across, _width points wide on screen,
// as one stroke of 400 points with pressure and time, and puts it in the undo history.
RDE_INTERNAL void look_demo_curve(fude_zoom_scene* _s, fude_zoom_v2 _c, f64 _r, f64 _phase, u32 _petals, rde_color _color, f64 _width) {
    const u32 _n = 400u;
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = rde_math_ldexp_f64(1.0, _q);
    fude_zoom_qpoint _pts[400];
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

// Draws a straight line from _a to _b in the camera's frame, _width points wide on screen, as one stroke of 200
// points, and puts it in the undo history.
RDE_INTERNAL void look_demo_line(fude_zoom_scene* _s, fude_zoom_v2 _a, fude_zoom_v2 _b, rde_color _color, f64 _width) {
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = rde_math_ldexp_f64(1.0, _q);
    fude_zoom_qpoint _pts[200];
    for(u32 _i = 0; _i < 200u; _i++) {
        const f64 _t = (f64)_i / 199.0;
        _pts[_i] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_b.x - _a.x) * _t / _g), (i32)rde_math_round_to_i64_f64((_b.y - _a.y) * _t / _g), 1023u, _i * 4u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_s, _s->camera.frame, _a, _q, _pts, 200u, FUDE_ZOOM_CHANNEL_TIME, _color, (f32)(_width / _s->camera.z), 0, 0, 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}

// Zooms the camera by _factor about the middle of the screen (_half: half the window's size).
RDE_INTERNAL void look_zoom_by(fude_zoom_scene* _s, f64 _factor, fude_zoom_v2 _half) {
    // In steps, as a pinch would, so each settle sees the zoom pass one threshold at most.
    const u32 _steps = (u32)rde_math_ceil_f64(rde_math_abs_f64(rde_math_log2_f64(_factor))) + 1u;
    const f64 _step  = rde_math_pow_f64(_factor, 1.0 / (f64)_steps);
    for(u32 _i = 0; _i < _steps; _i++) {
        fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, _step);
        fude_zoom_camera_settle(_s, _half);
    }
}

// What the flags asked for, filled by the readers (look_read_*) and acted on, frame by frame, by look_frame_*. A
// frame of 0 means the flag was not given.
RDE_INTERNAL u32 look_demo_lasso  = 0;       // --demo-lasso: 1 a loop and a drag; 2 also a turn; 3 a turn held.
// The frames counted since the app started, which the flags' frames refer to.
RDE_INTERNAL u32 look_frames      = 0;
RDE_INTERNAL b8  look_demo_snap   = false;   // --demo-snap: three rough shapes held till they snap
RDE_INTERNAL b8  look_demo_shapes = false;   // --demo-shapes: the Shapes tool's four, dragged out
RDE_INTERNAL i32 look_panel        = -1;     // --shapes-panel, --smoothing-panel: that tool's choices open
RDE_INTERNAL b8  look_demo_smooth  = false;  // --demo-smooth: a shaky wave at each smoothing level
RDE_INTERNAL b8  look_fly_home     = false;  // --fly-home: Reset view's flight at frame 10
RDE_INTERNAL b8  look_demo_places  = false;  // --demo-places: two places marked, Places open
RDE_INTERNAL b8  look_demo_loop    = false;  // --demo-loop: a lasso loop through the pen's own path
RDE_INTERNAL b8  look_demo_fill    = false;  // --demo-fill: things drawn, then tapped with the Fill tool
RDE_INTERNAL b8  look_brush_at     = false;  // --brush-at: the pen hovering there
RDE_INTERNAL i32 look_export       = -1;     // --export: 0 png, 1 svg, to look_export_path
RDE_INTERNAL u32 look_export_at    = 20u;    // --export-at: at that frame
RDE_INTERNAL f32 look_erase_line[4];         // --erase-at: from, to (screen)
RDE_INTERNAL u32 look_erase_frame = 0;       // ...at this frame (0: none).
RDE_INTERNAL FUDE_TOOL_ look_erase_tool = FUDE_TOOL_ERASE;   // --draw-at: the pen instead
RDE_INTERNAL u32 look_pages[2] = { 0, 0 };                   // --pages=K@N: Pages' choice K at frame N
RDE_INTERNAL u32 look_paper_later[2] = { 0, 0 };             // --paper-later=K@N: the canvas's paper K (FUDE_PAPER_) at frame N
RDE_INTERNAL u32 look_clear_frame = 0;       // --clear-at: Clear pressed at this frame (0: none)
RDE_INTERNAL u32 look_undo_frames[2][8];     // --undo-at, --redo-at: Undo, Redo pressed at these frames (0: none)
RDE_INTERNAL c8  look_instruments[16];       // --instruments: the kinds brought out
RDE_INTERNAL f32 look_instrument_at[4][4];   // --instrument-at: kind + 1 (0 when not given), x, y, degrees.
// How many --instrument-at flags were given.
RDE_INTERNAL u32 look_instrument_placed = 0;
RDE_INTERNAL f32 look_finger_drag[6];        // --finger-drag: three points (screen)
// The frame --finger-drag starts at.
RDE_INTERNAL u32 look_finger_frame = 0;
RDE_INTERNAL u8  look_measure = 0;           // --measure: the measuring mode
RDE_INTERNAL c8  look_numpad[64];            // --numpad: its text...
RDE_INTERNAL u32 look_numpad_frame = 0;      // ...and the frame it opens at.
// --text=X,Y,K,WORDS@FRAME, up to eight: each one's argument. K is 0 for a text, 1 for a sticky note, 2 or 3 for a
// formula (which keeps its "|"), 4 or 5 to leave it being written.
RDE_INTERNAL c8  look_texts[8][256];         // See above.
RDE_INTERNAL u32 look_text_count = 0;
RDE_INTERNAL b8  look_calibrate  = false;    // --calibrate
RDE_INTERNAL c8  look_mermaid[1024];         // --mermaid: the file's path.
RDE_INTERNAL i32 look_arrange = -1;          // --arrange: how (FUDE_ZOOM_ARRANGE_; -1: not given).
RDE_INTERNAL b8  look_demo_bucket = false;   // --demo-bucket
// --demo-layers[=K]: with 2, everything is also moved to the new layer; with 3, its name is shown on the card too;
// with 4, it is renamed as well; with 5, the new layer is moved under the first.
RDE_INTERNAL u32 look_demo_layers = 0;       // See above.
RDE_INTERNAL f64 look_offset = 0.0;          // --offset: by how many mm.
RDE_INTERNAL u8  look_demo_curving = 0;      // --demo-curve (1), --demo-curve-open (2)
RDE_INTERNAL u32 look_demo_drive = 0;        // --demo-drive: a drive's demo; with =2, its length typed too.
RDE_INTERNAL b8  look_saw = false;           // --saw: the Saw on from the start
RDE_INTERNAL f32 look_kerf = -1.0f;          // --kerf=MM: the saw's kerf at frame 4
RDE_INTERNAL i32 look_units = -1;            // --units=N: the page's units at frame 4 (0 mm, 1 cm, 2 m, 3 in, 4 ft)
RDE_INTERNAL i32 look_bar_tool = -1;         // --bar-tool=N: the bar's tool N (0 pen, 1 eraser, 2 lasso, 3 marker) chosen at frame 8
RDE_INTERNAL f32 look_tap_at[3];            // --tap-at=X,Y@N: a tap with the Lasso there at frame N.
RDE_INTERNAL b8  look_home_view = false;     // --home-view: the camera at home at 1x, before anything else.
RDE_INTERNAL f32 look_loop_at[4][4];        // --loop-at=X,Y,R@N: a round loop drawn with the pen there at frame N (up to four)
RDE_INTERNAL u32 look_loops = 0;
// --demo-board[=K][@N], at frame N (10 when left out): boards put down. With 1, two put down and one lassoed; 2,
// both lassoed; 3, its name and material asked for; 4, as 1 in walnut and oak.
RDE_INTERNAL i32 look_demo_board = -1;       // See above.
RDE_INTERNAL u32 look_demo_board_at = 10u;
// --census=N: at frame N, what the canvas holds is logged (its shapes by type).
RDE_INTERNAL u32 look_census = 0;
RDE_INTERNAL u32 look_strip = 0;             // --strip=N: a notebook's pages strip open at frame N
RDE_INTERNAL u32 look_strip_do[4][4];        // --strip-do=W,I,J@N, up to four, each kept as W, I, J, N.
RDE_INTERNAL u32 look_strip_do_count = 0;
RDE_INTERNAL i32 look_library = -1;          // --library=F@N: the diagram library's panel open on family F at frame N
RDE_INTERNAL u32 look_library_at = 0;
RDE_INTERNAL c8  look_symbols[8][160];       // --symbol=ID,DX,DY,TEXT@N: a symbol put there at frame N (up to eight)
RDE_INTERNAL u32 look_symbol_at[8];
RDE_INTERNAL u32 look_symbol_count = 0;
RDE_INTERNAL i32 look_kanban = -1;           // --kanban=P@N: a Kanban board of preset P (0 Kanban, 1 Scrum, 2 retro, 3 week) at frame N
RDE_INTERNAL u32 look_kanban_at = 0;
RDE_INTERNAL u32 look_map_at = 0;              // --map@N
RDE_INTERNAL c8  look_area[96];                // --area=NAME@N
RDE_INTERNAL u32 look_area_at = 0;
RDE_INTERNAL i32 look_present = -1;            // --present=S@N
RDE_INTERNAL u32 look_present_at = 0;
RDE_INTERNAL u32 look_laser_at = 0;            // --laser@N
RDE_INTERNAL f32 look_pen_taps[4][3];          // --pen-tap=X,Y@N (up to four)
RDE_INTERNAL u32 look_pen_tap_count = 0;
RDE_INTERNAL u32 look_canvas[2];               // --canvas=C@N
RDE_INTERNAL u32 look_audio_do[8][3];          // --audio=W,V@N, up to eight, each kept as W, V, N.
RDE_INTERNAL u32 look_audio_do_count = 0;
RDE_INTERNAL u32 look_crash_at = 0;            // --crash-now=N: a real crash at frame N, through the whole report.
RDE_INTERNAL c8  look_audio_import[512];      // --audio-import=WAV@N: a recording made of that WAV at frame N
RDE_INTERNAL u32 look_audio_import_at = 0;
RDE_INTERNAL u32 look_home_at = 0;             // --home=N: the home screen open at frame N
RDE_INTERNAL u32 look_welcome_at = 0;          // --welcome=N: ...with its welcome over it (the workshop's, if none)
RDE_INTERNAL c8  look_canvas_new[64];          // --canvas-new=NAME@N: a canvas made so named and opened at frame N
RDE_INTERNAL u32 look_canvas_new_at = 0;
RDE_INTERNAL c8  look_keep[64];                // --keep=NAME@N
RDE_INTERNAL u32 look_keep_at = 0;
RDE_INTERNAL u32 look_pieces_at = 0;           // --pieces@N
RDE_INTERNAL u32 look_piece_put[2];            // --piece-put=I@N
RDE_INTERNAL u32 look_custom[3];               // --custom=I,A[,NAME]@N
RDE_INTERNAL c8  look_custom_name[64];
RDE_INTERNAL u32 look_inside_at = 0;           // --inside@N
RDE_INTERNAL u32 look_back_at = 0;             // --back@N
RDE_INTERNAL u32 look_stencil[2];              // --stencil=I@N
RDE_INTERNAL f32 look_pen_drags[4][5];         // --pen-drag=X0,Y0,X1,Y1@N (up to four)
RDE_INTERNAL u32 look_pen_drag_count = 0;
RDE_INTERNAL i32 look_kanban_card = -1;      // --kanban-card=P[+][!]@N: the Kanban card up at frame N with template P
RDE_INTERNAL u32 look_kanban_card_at = 0;
RDE_INTERNAL c8  look_sheet[96] = { 0 };    // --sheet=PAPER:WAY:SCALE[:W:H][!]@N (or edit@N): a sheet's card up at frame N
RDE_INTERNAL u32 look_sheet_at = 0;
RDE_INTERNAL c8  look_size[96] = { 0 };     // --size-card=W;H;A;X;Y[~][!]@N: the size card up for what the lasso holds at frame N
RDE_INTERNAL u32 look_size_at = 0;
RDE_INTERNAL c8  look_repeat[128] = { 0 };  // --repeat=MODE;...[!]@N: the Repeat card up for what the lasso holds at frame N
RDE_INTERNAL u32 look_repeat_at = 0;
RDE_INTERNAL c8  look_points[6][64];             // --point=MODE:A:B:PRESS@N: a point typed for the Curve tool at frame N (up to six)
RDE_INTERNAL u32 look_point_at[6];
RDE_INTERNAL u32 look_point_count = 0;
// --lines=STEP@N: lines made, extended, trimmed and chamfered (page.h's fude_zoom_page_look_lines).
RDE_INTERNAL u32 look_lines[2];
// --combine=OP@N: shapes made and combined (page.h's fude_zoom_page_look_combine).
RDE_INTERNAL u32 look_combine[2];
RDE_INTERNAL u32 look_circuit[2];                 // --circuit=DEMO@N: a demo circuit (fude_zoom_page_look_circuit).
// --circuit-run@N: the circuit simulated from frame N.
RDE_INTERNAL u32 look_circuit_run = 0;
RDE_INTERNAL u32 look_mech[2];                    // --mech=DEMO@N: a demo mechanism (fude_zoom_page_look_mech).
// --mech-run@N: the mechanism run from frame N.
RDE_INTERNAL u32 look_mech_run = 0;
RDE_INTERNAL c8  look_value[96];                  // --value=ID,TEXT,UNIT,WAY@N: the first symbol ID given a value (Value's card)
RDE_INTERNAL c8  look_limits[96];                 // --limits=ID,PRESET,FIELD,TYPED,UNIT,FINISH@N: the first symbol ID's Limits card
RDE_INTERNAL u32 look_limits_frame = 0;
RDE_INTERNAL u32 look_value_frame = 0;
RDE_INTERNAL u32 look_slider[2];                  // --slider=I,PERCENT@N: Play's slider I set PERCENT along
RDE_INTERNAL f64 look_slider_t = 0.0;
RDE_INTERNAL u32 look_body[5];                    // --body=ALL,MATERIAL,FIXED,APPLY@N: drawings made bodies
RDE_INTERNAL u32 look_make_part = 0;              // --make-part@N: the last demo made a part
RDE_INTERNAL c8  look_align[48];                  // --align=ID,DX,DY@N: a symbol dragged, held
RDE_INTERNAL u32 look_align_frame = 0;
RDE_INTERNAL f32 look_context[3] = { 0 };    // --context=X,Y@N: the page's menu opened at X,Y (screen) at frame N
RDE_INTERNAL u32 look_context_delete = 0;    // --context-delete@N: its Delete pressed at frame N
RDE_INTERNAL u32 look_examples[2] = { 0 };   // --examples=W@N: Insert → Examples, which W (0 all … 3 both), at frame N
RDE_INTERNAL u32 look_example[2] = { 0 };    // --example=E@N: example E drawn on the canvas open, at frame N
RDE_INTERNAL b8  look_fps = false;           // --fps: the frame rate shown
RDE_INTERNAL u32 look_circuit2[2];                // --circuit2=DEMO@N: a second demo circuit (after a part is made)
RDE_INTERNAL u32 look_plan[2][2];                 // --plan=STEP@N (twice at most): a floor plan's demo (fude_zoom_page_look_plan)
RDE_INTERNAL u32 look_plan_count = 0;               // How many --plan flags were given.
RDE_INTERNAL u32 look_shapes_choices[4][2];       // --shapes-choice=I@N: the Shapes panel's choice I pressed at frame N (up to four)
RDE_INTERNAL u32 look_shapes_choice_count = 0;                 // How many --shapes-choice flags were given.
RDE_INTERNAL u32 look_shape_tools[4][2];          // --shape-tool=TYPE@N: the Shapes tool's TYPE (shape.h) in hand at frame N (up to four)
RDE_INTERNAL u32 look_shape_tool_count = 0;
RDE_INTERNAL u32 look_measure_tool[2] = { 0, 0 }; // --measure-tool=N@F: the measure in hand at frame F (1 the tape, 2 the dimension)
RDE_INTERNAL u32 look_topic[2] = { 0, 0 };        // --topic=N@F: the canvas's topic N (page.h's FUDE_ZOOM_TOPIC_) at frame F
RDE_INTERNAL u32 look_switch_topic[2] = { 0, 0 }; // --switch-topic=N@F: topic N chosen from the Topic menu at frame F
RDE_INTERNAL u32 look_line_style[2] = { 0, 0 };   // --line-style=N@F: the Shapes' line style N (shape.h) at frame F
RDE_INTERNAL b8  look_kanban_card_extra = false, look_kanban_card_insert = false;
RDE_INTERNAL u32 look_tidy_at = 0;           // --tidy=N: at frame N, every sticky note lassoed and let go (lined up in a Kanban column)
RDE_INTERNAL f64 look_fillet_mm = -1.0;      // --fillet=MM@N: a corner lassoed at frame N, then rounded MM (0: not).
RDE_INTERNAL u32 look_fillet_at = 0;            // The frame for --fillet (20 when it is left out).
RDE_INTERNAL f32 look_path[16][2];          // --path-at=X,Y;X,Y;...@N: the pen drawn through those points at frame N
RDE_INTERNAL u32 look_path_n = 0, look_path_frame = 0;
RDE_INTERNAL f32 look_pan_later[3];         // --pan-later=X,Y@N: the camera moved X, Y screen points at frame N
RDE_INTERNAL u32 look_handwrite = 0;        // --handwrite=N: every text written out by hand at frame N
RDE_INTERNAL f32 look_grip[3];              // --grip=DX,DY@N: the offset pad's grip dragged at frame N
// --grow=K,BY@N[!]: instrument K's size tab dragged BY points at frame N ("!" leaves it held).
RDE_INTERNAL f32 look_grow[4];              // K, BY, N, and 1 when held.
RDE_INTERNAL f32 look_anchor[4];            // --anchor=K,ACROSS,UP@N: instrument K snapped where it lies at frame N, then moved that many mm
RDE_INTERNAL u32 look_anchor_frame = 0;
// --parts=ROWS@N[!]: Fit parts' card filled at frame N with ROWS ("600 x 300; 200 x 100"); "!" then presses Cut.
RDE_INTERNAL c8  look_parts[256];           // The rows.
RDE_INTERNAL u32 look_parts_frame = 0;
RDE_INTERNAL b8  look_parts_cut = false;
// --fit=LIST@N: the lassoed board's parts fitted at frame N ("@N" alone just presses Fit parts).
RDE_INTERNAL c8  look_fit[256];             // The list.
RDE_INTERNAL u32 look_fit_frame = 0;
// --demo-sculpt[=2|3]: a line, then a bump drawn over its middle with Sculpt on. With 2, Sculpt is off, so the bump
// is a stroke of its own; with 3, Nudge pushes the line's middle up instead.
RDE_INTERNAL u32 look_demo_sculpt = 0;       // See above.
RDE_INTERNAL f64 look_holes = 0.0;           // --holes=MM: a 32 mm hole row that long at frame 10
RDE_INTERNAL u32 look_to_curve = 0;          // --to-curve=N: everything lassoed and To curve at frame N
RDE_INTERNAL u32 look_select_all = 0;        // --select-all=N: everything lassoed at frame N (its row as it shows)
RDE_INTERNAL u32 look_formula_at[2] = { 0, 0 };   // --formula-hand=N, --formula-read=N: formulas written by hand / handwriting read, at frame N
RDE_INTERNAL i32 look_nodes = -1;            // --nodes=STEP (with --demo-curve): the path lassoed, 1 a node dragged, 2 one added, 3 one held,
                                                  // 4 one tapped (its handle shown), 5 and its handle dragged out
RDE_INTERNAL c8  look_find[64];              // --find: the words...
RDE_INTERNAL u32 look_find_frame = 0;        // ...and the frame.
RDE_INTERNAL c8  look_pdf[512];              // --pdf: the PDF...
RDE_INTERNAL u32 look_pdf_frame = 0;         // ...and the frame.
RDE_INTERNAL f32 look_loop[4];               // --loop-at: the box...
RDE_INTERNAL u32 look_loop_frame = 0;        // ...and the frame.
RDE_INTERNAL u32 look_offset_frame = 0;      // --offset-at
RDE_INTERNAL f32 look_joint[4];              // --joint: kind and its three numbers...
RDE_INTERNAL u32 look_joint_frame = 0;       // ...and the frame.
// --export's path, and --brush-at's point (screen units) and tool (-1: as it is).
RDE_INTERNAL c8  look_export_path[1024];
RDE_INTERNAL rde_vec_2F look_brush = { 0.0f, 0.0f };
RDE_INTERNAL i32        look_brush_tool = -1;
RDE_INTERNAL b8  look_depth_levels = false;  // --depth-levels: the depth's levels open at frame 20.
// --stress, and the seed of its random numbers (set from the clock when the flag is given).
RDE_INTERNAL b8  look_stress      = false;
RDE_INTERNAL b8  look_stress_add  = false;   // --stress-add: strokes and zooms only, so the count only grows.
RDE_INTERNAL u32 look_stress_seed = 12345u;
RDE_INTERNAL f64 look_zoom_later  = 0.0;     // --zoom-later: the zoom, at frame look_zoom_frame.
RDE_INTERNAL u32 look_zoom_frame  = 6u;

// A random number from 0 up to 1 for --stress: a linear congruential generator, its top 24 bits.
RDE_INTERNAL f64 look_stress_rnd(void) {
    look_stress_seed = look_stress_seed * 1664525u + 1013904223u;
    return (f64)(look_stress_seed >> 8) / 16777216.0;
}

// A hand's jitter for the demos: the next number of the same generator, from its own seed *_seed, its bits 8 to 15 as
// a share from -0.5 to 0.5 of _span. Not RDE's random: the demos' screenshots depend on these exact numbers. One call
// per statement, so the numbers come in a defined order.
RDE_INTERNAL f32 look_jitter(u32* _seed, f32 _span) {
    *_seed = *_seed * 1664525u + 1013904223u;
    return ((f32)(*_seed >> 8 & 0xFF) / 255.0f - 0.5f) * _span;
}

// --demo-lasso, frame by frame: a loop round the left half at frame 10, a drag at 14, and a turn at 18. It starts
// after the canvas has been drawn, so the lasso knows the frames on screen.
RDE_INTERNAL void look_demo_lasso_frame(void) {
    fude_zoom_selection* _sel = &look_page->selection;
    if(look_frames == 10u) {
        look_app->ui->bar.tool = FUDE_TOOL_LASSO;
        look_page->tool_seen   = (u8)FUDE_TOOL_LASSO;
        const rde_vec_2F _loop[5] = { { -330.0f, 260.0f }, { 20.0f, 260.0f }, { 20.0f, -260.0f }, { -330.0f, -260.0f }, { -330.0f, 255.0f } };
        fude_zoom_select_down(_sel, &look_page->scene, _loop[0]);
        for(u32 _i = 1; _i < 5u; _i++) {
            fude_zoom_select_moved(_sel, _loop[_i]);
        }
        fude_zoom_select_up(_sel, &look_page->scene, &look_page->renderer);
    } else if(look_frames == 14u) {
        fude_zoom_box _b;
        if(fude_zoom_select_box(_sel, &_b)) {
            const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
            fude_zoom_select_down(_sel, &look_page->scene, _c);
            fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + 40.0f, _c.y - 30.0f });
            fude_zoom_select_up(_sel, &look_page->scene, &look_page->renderer);
        }
    } else if(look_frames == 18u && look_demo_lasso >= 2u) {
        fude_zoom_box _b;
        if(fude_zoom_select_box(_sel, &_b)) {
            const rde_vec_2F _knob = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)(_b.max_y + FUDE_ZOOM_SELECT_KNOB) };
            const rde_vec_2F _c    = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
            fude_zoom_select_down(_sel, &look_page->scene, _knob);
            // A sixth of a turn, clockwise; with 3, 37° and still held, so its angle shows.
            const f32 _r = _knob.y - _c.y;
            if(look_demo_lasso >= 3u) {
                fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + _r * rde_math_cos_f32(53.0f * (f32)RDE_PI / 180.0f), _c.y + _r * rde_math_sin_f32(53.0f * (f32)RDE_PI / 180.0f) });
                return;
            }
            fude_zoom_select_moved(_sel, (rde_vec_2F){ _c.x + _r * 0.866f, _c.y + _r * 0.5f });
            fude_zoom_select_up(_sel, &look_page->scene, &look_page->renderer);
        }
    }
}

// --demo-snap: a rough circle, rectangle and line drawn with the "pen", each held at its end until it snaps; the
// circle is then pulled bigger while held. The pen's steps are scripted first and played one a frame.
// A step of the pen: its phase (0 down, 1 moved, 2 up), where (screen units), and how long to hold there (ms).
typedef struct { u8 phase; f32 x, y; u32 hold; } look_pen_step;
// The script (600 steps at most), how many steps it has, the next one, and the time to wait until.
RDE_INTERNAL look_pen_step LOOK_SNAP[600];
RDE_INTERNAL u32                LOOK_SNAP_N    = 0;
RDE_INTERNAL u32                LOOK_SNAP_AT   = 0;
RDE_INTERNAL f64                LOOK_SNAP_WAIT = 0.0;

// Adds a step to the script, while there is room.
RDE_INTERNAL void look_snap_add(u8 _phase, f32 _x, f32 _y, u32 _hold) {
    if(LOOK_SNAP_N < 600u) {
        LOOK_SNAP[LOOK_SNAP_N++] = (look_pen_step){ _phase, _x, _y, _hold };
    }
}

// Writes --demo-snap's script: each shape is drawn with a hand's jitter (look_jitter, up to 2.5 units either way)
// and held 800 ms at its end.
RDE_INTERNAL void look_snap_script(void) {
    u32 _seed = 99u;
    // A circle round (-250, 120), radius 110, ending where it began; held 800 ms, then pulled out.
    for(u32 _i = 0; _i <= 72u; _i++) {
        const f32 _a  = (f32)(2.0 * RDE_PI) * (f32)_i / 72.0f;
        const f32 _jx = look_jitter(&_seed, 5.0f);
        const f32 _jy = look_jitter(&_seed, 5.0f);
        look_snap_add(_i == 0 ? 0u : 1u, -250.0f + rde_math_cos_f32(_a) * 110.0f + _jx, 120.0f + rde_math_sin_f32(_a) * 104.0f + _jy, 0u);
    }
    look_snap_add(1u, -140.0f, 120.0f, 800u);
    for(u32 _i = 1; _i <= 10u; _i++) {
        look_snap_add(1u, -140.0f + 4.0f * (f32)_i, 120.0f + 2.0f * (f32)_i, 0u);
    }
    look_snap_add(2u, 0.0f, 0.0f, 0u);
    // A rectangle, a little off level.
    const f32 _r[5][2] = { { 60.0f, 220.0f }, { 330.0f, 228.0f }, { 326.0f, 60.0f }, { 56.0f, 52.0f }, { 60.0f, 216.0f } };
    for(u32 _k = 0; _k < 4u; _k++) {
        for(u32 _i = 0; _i < 18u; _i++) {
            const f32 _t  = (f32)_i / 18.0f;
            const f32 _jx = look_jitter(&_seed, 5.0f);
            const f32 _jy = look_jitter(&_seed, 5.0f);
            look_snap_add(_k == 0 && _i == 0 ? 0u : 1u, _r[_k][0] + (_r[_k + 1][0] - _r[_k][0]) * _t + _jx, _r[_k][1] + (_r[_k + 1][1] - _r[_k][1]) * _t + _jy, 0u);
        }
    }
    look_snap_add(1u, _r[4][0], _r[4][1], 800u);
    look_snap_add(2u, 0.0f, 0.0f, 0u);
    // A line.
    for(u32 _i = 0; _i <= 30u; _i++) {
        const f32 _jx = look_jitter(&_seed, 5.0f);
        const f32 _jy = look_jitter(&_seed, 5.0f);
        look_snap_add(_i == 0 ? 0u : 1u, -350.0f + 20.0f * (f32)_i + _jx, -150.0f - 4.0f * (f32)_i + _jy, 0u);
    }
    look_snap_add(1u, 250.0f, -270.0f, 800u);
    look_snap_add(2u, 0.0f, 0.0f, 0u);
}

// Plays --demo-snap's script from frame 10, a step a frame, waiting where a step holds.
RDE_INTERNAL void look_demo_snap_frame(void) {
    if(look_frames < 10u || LOOK_SNAP_AT >= LOOK_SNAP_N) {
        return;
    }
    const f64 _now = rde_engine_get_time_now();
    if(_now < LOOK_SNAP_WAIT) {
        return;   // The pen is resting.
    }
    const look_pen_step _st = LOOK_SNAP[LOOK_SNAP_AT++];
    fude_zoom_page_look_pen(look_page, _st.phase, (rde_vec_2F){ _st.x, _st.y });
    if(_st.hold > 0u) {
        LOOK_SNAP_WAIT = _now + (f64)_st.hold / 1000.0;
    }
}

// --demo-shapes at frame 12, and --shapes-panel or --smoothing-panel at frame 20, once the canvas has been drawn.
RDE_INTERNAL void look_demo_shapes_frame(void) {
    if(look_frames == 20u && look_panel >= 0) {
        fude_toolbar_set_tool_open(&look_app->ui->bar, look_panel);
    }
    if(look_frames != 12u || !look_demo_shapes) {
        return;
    }
    const struct { u8 type; b8 filled; f32 x0, y0, x1, y1; } _shapes[4] = {
        { FUDE_ZOOM_SHAPE_RECT,    true,  -380.0f, 260.0f, -120.0f, 80.0f },
        { FUDE_ZOOM_SHAPE_ELLIPSE, false,  -60.0f, 260.0f,  260.0f, 60.0f },
        { FUDE_ZOOM_SHAPE_POLYGON, true,  -380.0f, -40.0f, -120.0f, -280.0f },
        { FUDE_ZOOM_SHAPE_LINE,    false,  -40.0f, -60.0f,  280.0f, -280.0f },
    };
    for(u32 _i = 0; _i < 4u; _i++) {
        look_page->shape_tool   = _shapes[_i].type;
        look_page->shape_filled = _shapes[_i].filled;
        fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ _shapes[_i].x0, _shapes[_i].y0 });
        fude_zoom_page_look_pen(look_page, 1u, (rde_vec_2F){ _shapes[_i].x1, _shapes[_i].y1 });
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ _shapes[_i].x1, _shapes[_i].y1 });
    }
}

// --demo-smooth: the same shaky wave drawn with the "pen" at each smoothing level (smooth.h), top to bottom: Off,
// Low, Medium, High, Rope.
RDE_INTERNAL void look_demo_smooth_frame(void) {
    if(look_frames != 12u) {
        return;
    }
    const u8 _was  = look_page->smooth_level;
    u32      _seed = 4242u;
    for(u32 _l = 0; _l < FUDE_ZOOM_SMOOTH_COUNT; _l++) {
        look_page->smooth_level = (u8)_l;
        const f32  _y0  = 250.0f - 120.0f * (f32)_l;
        rde_vec_2F _pen = { 0.0f, 0.0f };
        // A sample every 1.5 points, as a pen gives drawing briskly, with a hand's tremor (±2.5, about 20 points a
        // wave) and a little noise on top.
        for(u32 _i = 0; _i <= 470u; _i++) {
            const f32 _x = 1.5f * (f32)_i;
            const f32 _jx = look_jitter(&_seed, 2.0f);
            const f32 _jy = look_jitter(&_seed, 2.0f);
            const f32 _tremor = 2.5f * rde_math_sin_f32(_x * 0.31f) + 1.2f * rde_math_sin_f32(_x * 0.53f + 1.0f);
            _pen = (rde_vec_2F){ -400.0f + _x + _jx, _y0 + 30.0f * rde_math_sin_f32(_x * 0.017f) + _tremor + _jy };
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, _pen);
        }
        fude_zoom_page_look_pen(look_page, 2u, _pen);
    }
    look_page->smooth_level = _was;
}

// --demo-fill at frame 12: shapes drawn with the "pen" and the Shapes tool, then tapped with the Fill tool in four
// colours, then partly erased.
RDE_INTERNAL void look_demo_fill_frame(void) {
    if(look_frames != 12u) {
        return;
    }
    u32 _seed = 77u;
    // A loop round (-280, 140), ending a little past where it began.
    for(u32 _i = 0; _i <= 130u; _i++) {
        const f32 _a  = (f32)(2.0 * RDE_PI) * (f32)_i / 120.0f;
        const f32 _jx = look_jitter(&_seed, 3.0f);
        const f32 _jy = look_jitter(&_seed, 3.0f);
        fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ -280.0f + rde_math_cos_f32(_a) * 120.0f + _jx, 140.0f + rde_math_sin_f32(_a) * 95.0f + _jy });
    }
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // A figure 8 round (100, 140).
    for(u32 _i = 0; _i <= 160u; _i++) {
        const f32 _a = (f32)(2.0 * RDE_PI) * (f32)_i / 160.0f, _d = 1.0f + rde_math_sin_f32(_a) * rde_math_sin_f32(_a);
        fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 100.0f + 150.0f * rde_math_cos_f32(_a) / _d, 140.0f + 150.0f * rde_math_sin_f32(_a) * rde_math_cos_f32(_a) / _d });
    }
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // An open arc (a "C") round (200, -170).
    for(u32 _i = 0; _i <= 80u; _i++) {
        const f32 _a = 0.9f + 4.5f * (f32)_i / 80.0f;
        fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 200.0f + rde_math_cos_f32(_a) * 100.0f, -170.0f + rde_math_sin_f32(_a) * 100.0f });
    }
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    // A rectangle with the Shapes tool.
    look_page->shape_tool = FUDE_ZOOM_SHAPE_RECT;
    fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ -400.0f, -80.0f });
    fude_zoom_page_look_pen(look_page, 1u, (rde_vec_2F){ -160.0f, -260.0f });
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ -160.0f, -260.0f });
    look_page->shape_tool = 0;
    // Taps with the Fill tool: the loop, the 8's left lobe, the rectangle, and inside the arc.
    look_page->fill_tool = true;
    const rde_color _was = look_app->ink->color;
    const struct { f32 x, y; rde_color c; } _taps[4] = {
        { -280.0f, 140.0f, { 241, 196, 15, 255 } }, { 30.0f, 140.0f, { 52, 152, 219, 255 } },
        { -280.0f, -170.0f, { 231, 76, 60, 255 } }, { 200.0f, -170.0f, { 46, 204, 113, 255 } },
    };
    for(u32 _i = 0; _i < 4u; _i++) {
        look_app->ink->color = _taps[_i].c;
        fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ _taps[_i].x, _taps[_i].y });
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ _taps[_i].x, _taps[_i].y });
    }
    look_app->ink->color = _was;
    look_page->fill_tool = false;
    // The eraser across the loop and down the rectangle rubs out both the line and the fill there.
    fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_ERASE);
    fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ -440.0f, 150.0f });
    for(u32 _i = 1; _i <= 40u; _i++) {
        fude_zoom_page_look_pen(look_page, 1u, (rde_vec_2F){ -440.0f + 8.0f * (f32)_i, 150.0f - 1.5f * (f32)_i });
    }
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ -280.0f, -60.0f });
    for(u32 _i = 1; _i <= 30u; _i++) {
        fude_zoom_page_look_pen(look_page, 1u, (rde_vec_2F){ -280.0f, -60.0f - 8.0f * (f32)_i });
    }
    fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_DRAW);
}

// One frame of --stress: mostly a random rose; now and then an eraser sweep, an undo, or a zoom by 8 in or out.
// --stress-add draws and zooms only. It waits while the canvas is closed or someone is drawing or erasing.
RDE_INTERNAL void look_stress_frame(void) {
    fude_zoom_scene* _s = &look_page->scene;
    if(!look_page->open || look_page->drawing || look_page->erasing) {
        return;
    }
    const rde_vec_2I   _size = rde_window_get_size(look_app->window);
    const fude_zoom_v2 _half = { (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
    const f64          _u    = 1.0 / _s->camera.z;
    const f64          _r    = look_stress_rnd();
    const fude_zoom_v2 _at   = { _s->camera.at.x + (look_stress_rnd() - 0.5) * _half.x * 1.6 * _u, _s->camera.at.y + (look_stress_rnd() - 0.5) * _half.y * 1.6 * _u };
    if(_r < 0.82 || (look_stress_add && _r < 0.96)) {
        look_demo_curve(_s, _at, (20.0 + look_stress_rnd() * 60.0) * _u, look_stress_rnd() * 6.0, 3u + (u32)(look_stress_rnd() * 5.0),
                             LOOK_DEMO_COLORS[(u32)(look_stress_rnd() * 6.0) % 6u], 1.0 + look_stress_rnd() * 4.0);
    } else if(_r < 0.92 && !look_stress_add) {
        fude_zoom_eraser _e;
        fude_zoom_eraser_init(&_e);
        fude_zoom_erase_begin(&_e, (FUDE_ZOOM_ERASE_)((u32)(look_stress_rnd() * 3.0) % 3u));
        const fude_zoom_v2 _to = { _at.x + (look_stress_rnd() - 0.5) * 200.0 * _u, _at.y + (look_stress_rnd() - 0.5) * 200.0 * _u };
        fude_zoom_erase_step(_s, &_e, _s->camera.frame, _at, _to, 10.0 * _u, _u);
        fude_zoom_erase_end(_s, &_e, _s->camera.frame, fude_zoom_box_empty());
        fude_zoom_eraser_destroy(&_e);
    } else if(_r < 0.96 && !look_stress_add) {
        fude_zoom_history_undo(_s);
    } else {
        look_zoom_by(_s, look_stress_rnd() < 0.5 ? 8.0 : 0.125, _half);
    }
}

// --- reading the flags, once at launch -------------------------------------------------------

// Reads one argument, _a, when it is one of these flags, and returns whether it was: the demos drawn at launch
// (--demo-deep, --demo-erase, --gate), a picture brought in, the first demos' flags, the tools' panels, topics, texts,
// a joint, loops, a PDF, Find and a drive.
RDE_INTERNAL b8 look_read_demos(const c8* _a) {
    fude_zoom_scene*   _s    = &look_page->scene;
    const rde_vec_2I   _size = rde_window_get_size(look_app->window);
    const fude_zoom_v2 _half = { (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
    const c8* _v;
    if((_v = fude_look_value(_a, "--demo-deep")) != NULL) {
        const u32 _levels = (u32)(i32)rde_strtol(_v, NULL, 10);
        for(u32 _l = 0; _l < _levels; _l++) {
            const fude_zoom_v2 _c = _s->camera.at;
            const f64 _r = 260.0 / _s->camera.z;
            for(u32 _k = 0; _k < 3u; _k++) {
                look_demo_curve(_s, _c, _r * (1.0 - 0.25 * (f64)_k), (f64)_k * 0.7, 5u + _k, LOOK_DEMO_COLORS[(_l + _k) % 6u], 7.0 - 2.0 * (f64)_k);
            }
            if(_l + 1u < _levels) {
                // Off to a petal's tip, then 1024× in, where the next level's flowers are drawn.
                fude_zoom_camera_pan(_s, (fude_zoom_v2){ -_r * 0.9 * _s->camera.z, 0.0 });
                look_zoom_by(_s, 1024.0, _half);
            }
        }
    } else if(fude_look_is(_a, "--demo-erase")) {
        const fude_zoom_v2 _c = _s->camera.at;
        const f64 _u = 1.0 / _s->camera.z;
        fude_zoom_eraser _e;
        fude_zoom_eraser_init(&_e);
        for(u32 _m = 0; _m < 3u; _m++) {
            const f64 _y = _c.y + (200.0 - 200.0 * (f64)_m) * _u;
            look_demo_line(_s, (fude_zoom_v2){ _c.x - 300.0 * _u, _y }, (fude_zoom_v2){ _c.x + 300.0 * _u, _y }, LOOK_DEMO_COLORS[_m], 6.0);
            look_demo_line(_s, (fude_zoom_v2){ _c.x - 100.0 * _u, _y - 60.0 * _u }, (fude_zoom_v2){ _c.x - 100.0 * _u, _y + 60.0 * _u }, LOOK_DEMO_COLORS[3], 4.0);
            look_demo_line(_s, (fude_zoom_v2){ _c.x + 100.0 * _u, _y - 60.0 * _u }, (fude_zoom_v2){ _c.x + 100.0 * _u, _y + 60.0 * _u }, LOOK_DEMO_COLORS[3], 4.0);
            fude_zoom_erase_begin(&_e, (FUDE_ZOOM_ERASE_)_m);
            fude_zoom_erase_step(_s, &_e, _s->camera.frame, (fude_zoom_v2){ _c.x, _y - 30.0 * _u }, (fude_zoom_v2){ _c.x, _y + 30.0 * _u }, 10.0 * _u, _u);
            fude_zoom_erase_end(_s, &_e, _s->camera.frame, fude_zoom_box_empty());
        }
        fude_zoom_eraser_destroy(&_e);
    } else if((_v = fude_look_value(_a, "--gate")) != NULL) {
        // Four depths, the points shared out between them, in pen-like strokes of 400 points; then the camera
        // goes back out to the top.
        const u64 _want = (u64)rde_strtol(_v, NULL, 10);
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
                look_demo_curve(_s, (fude_zoom_v2){ _x, _y }, 40.0 * _u, (f64)(_seed & 255u), 3u + (_seed % 5u), LOOK_DEMO_COLORS[_seed % 6u], 2.0);
            }
            _made += _share;
            if(_l < 3u) {
                look_zoom_by(_s, 1024.0, _half);
            }
        }
        look_zoom_by(_s, 1.0 / 1024.0 / 1024.0 / 1024.0, _half);
    } else if(fude_look_is(_a, "--demo-lasso") || fude_look_value(_a, "--demo-lasso") != NULL) {
        _v = fude_look_value(_a, "--demo-lasso");
        look_demo_lasso = _v != NULL ? (u32)(i32)rde_strtol(_v, NULL, 10) : 1u;
    } else if((_v = fude_look_value(_a, "--demo-picture")) != NULL) {
        // As if Files had brought it; it is taken on the first update.
        const c8* _paths[1] = { _v };
        fude_import_arrived(FUDE_IMPORT_FILES, _paths, 1u);
    } else if(fude_look_is(_a, "--demo-shapes")) {
        look_demo_shapes = true;
    } else if(fude_look_is(_a, "--shapes-panel")) {
        look_panel = 1;   // Topic is first, then General's Shapes, Insert, Smoothing...
    } else if(fude_look_is(_a, "--smoothing-panel")) {
        look_panel = 3;
    } else if((_v = fude_look_value(_a, "--tool-panel")) != NULL) {
        look_panel = (i32)rde_strtol(_v, NULL, 10);   // Any tool's choices (0: Topic; in General, 6: Instruments).
    } else if((_v = fude_look_value(_a, "--topic")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_topic[0], &look_topic[1]);
    } else if((_v = fude_look_value(_a, "--switch-topic")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_switch_topic[0], &look_switch_topic[1]);
    } else if(fude_look_is(_a, "--demo-smooth")) {
        look_demo_smooth = true;
    } else if((_v = fude_look_value(_a, "--text")) != NULL && look_text_count < 8u) {
        rde_snprintf(look_texts[look_text_count++], sizeof(look_texts[0]), "%s", _v);
    } else if((_v = fude_look_value(_a, "--joint")) != NULL) {
        if(rde_sscanf(_v, "%f,%f,%f,%f@%u", &look_joint[0], &look_joint[1], &look_joint[2], &look_joint[3], &look_joint_frame) != 5) {
            look_joint_frame = 0;
        }
    } else if((_v = fude_look_value(_a, "--offset-at")) != NULL) {
        look_offset_frame = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--loop-at")) != NULL) {
        // Its two forms, told apart by how many numbers come before the frame: four, the Lasso round a box; three,
        // a round loop drawn with the pen.
        if(rde_sscanf(_v, "%f,%f,%f,%f@%u", &look_loop[0], &look_loop[1], &look_loop[2], &look_loop[3], &look_loop_frame) != 5) {
            look_loop_frame = 0;
            u32  _frame = 0;
            f32* _l     = look_loop_at[look_loops < 4u ? look_loops : 3u];
            if(rde_sscanf(_v, "%f,%f,%f@%u", &_l[0], &_l[1], &_l[2], &_frame) == 4) {
                _l[3] = (f32)_frame;
                look_loops += look_loops < 4u ? 1u : 0u;
            }
        }
    } else if((_v = fude_look_value(_a, "--pdf")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_pdf, sizeof(look_pdf), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_pdf_frame = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 10u;
    } else if((_v = fude_look_value(_a, "--find")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_find, sizeof(look_find), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_find_frame = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 30u;
    } else if(fude_look_is(_a, "--demo-drive") || fude_look_value(_a, "--demo-drive") != NULL) {
        _v = fude_look_value(_a, "--demo-drive");
        look_demo_drive = _v != NULL ? (u32)(i32)rde_strtol(_v, NULL, 10) : 1u;
    } else {
        return false;
    }
    return true;
}

// Reads one argument, _a, when it is one of these flags, and returns whether it was: a fillet, the library, the
// strip, symbols, tidying, the map, pieces, stencils, custom parts, Inside and Back, audio, a crash, the home screen
// and its welcome, canvases, the pen's taps and drags, the laser, an area and presenting.
RDE_INTERNAL b8 look_read_panels(const c8* _a) {
    const c8* _v;
    if((_v = fude_look_value(_a, "--fillet")) != NULL) {
        look_fillet_mm = rde_strtod(_v, NULL);
        look_fillet_at = rde_strchr(_v, '@') != NULL ? (u32)(i32)rde_strtol(rde_strchr(_v, '@') + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--library")) != NULL) {
        look_library    = (i32)rde_strtol(_v, NULL, 10);
        look_library_at = rde_strchr(_v, '@') != NULL ? (u32)(i32)rde_strtol(rde_strchr(_v, '@') + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--strip-do")) != NULL && look_strip_do_count < 4u) {
        u32* _d = look_strip_do[look_strip_do_count];
        if(rde_sscanf(_v, "%u,%u,%u@%u", &_d[0], &_d[1], &_d[2], &_d[3]) == 4) {
            look_strip_do_count++;
        }
    } else if((_v = fude_look_value(_a, "--strip")) != NULL) {
        look_strip = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--symbol")) != NULL && look_symbol_count < 8u) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_symbols[look_symbol_count], sizeof(look_symbols[0]), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_symbol_at[look_symbol_count] = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
        look_symbol_count++;
    } else if((_v = fude_look_value(_a, "--tidy")) != NULL) {
        look_tidy_at = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if(rde_strncmp(_a, "--map@", 6) == 0) {
        look_map_at = (u32)(i32)rde_strtol(_a + 6, NULL, 10);
    } else if((_v = fude_look_value(_a, "--keep")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_keep, sizeof(look_keep), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_keep_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if(rde_strncmp(_a, "--pieces@", 9) == 0) {
        look_pieces_at = (u32)(i32)rde_strtol(_a + 9, NULL, 10);
    } else if((_v = fude_look_value(_a, "--stencil")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_stencil[0], &look_stencil[1]);
    } else if((_v = fude_look_value(_a, "--piece-put")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_piece_put[0], &look_piece_put[1]);
    } else if((_v = fude_look_value(_a, "--custom")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_sscanf(_v, "%u,%u", &look_custom[0], &look_custom[1]);
        const c8* _n = rde_strchr(_v, ',') != NULL ? rde_strchr(rde_strchr(_v, ',') + 1, ',') : NULL;
        if(_n != NULL && (_at == NULL || _n < _at)) {
            rde_snprintf(look_custom_name, sizeof(look_custom_name), "%.*s", (int)(_at != NULL ? (usize)(_at - _n - 1) : rde_strlen(_n + 1)), _n + 1);
        }
        look_custom[2] = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if(rde_strncmp(_a, "--inside@", 9) == 0) {
        look_inside_at = (u32)(i32)rde_strtol(_a + 9, NULL, 10);
    } else if(rde_strncmp(_a, "--back@", 7) == 0) {
        look_back_at = (u32)(i32)rde_strtol(_a + 7, NULL, 10);
    } else if((_v = fude_look_value(_a, "--audio-import")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_audio_import, sizeof(look_audio_import), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_audio_import_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 10u;
    } else if((_v = fude_look_value(_a, "--audio")) != NULL && look_audio_do_count < 8u) {
        u32* _d = look_audio_do[look_audio_do_count];
        if(rde_sscanf(_v, "%u,%u@%u", &_d[0], &_d[1], &_d[2]) == 3) {
            look_audio_do_count++;
        }
    } else if((_v = fude_look_value(_a, "--crash-now")) != NULL) {
        look_crash_at = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--home")) != NULL) {
        look_home_at = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--welcome")) != NULL) {
        look_welcome_at = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--canvas-new")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_canvas_new, sizeof(look_canvas_new), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_canvas_new_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 5u;
    } else if((_v = fude_look_value(_a, "--canvas")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_canvas[0], &look_canvas[1]);
    } else if((_v = fude_look_value(_a, "--pen-tap")) != NULL) {
        u32 _frame = 0;
        f32* _t = look_pen_taps[look_pen_tap_count < 4u ? look_pen_tap_count : 3u];
        if(rde_sscanf(_v, "%f,%f@%u", &_t[0], &_t[1], &_frame) == 3) {
            _t[2] = (f32)_frame;
            look_pen_tap_count += look_pen_tap_count < 4u ? 1u : 0u;
        }
    } else if((_v = fude_look_value(_a, "--pen-drag")) != NULL) {
        u32 _frame = 0;
        f32* _d = look_pen_drags[look_pen_drag_count < 4u ? look_pen_drag_count : 3u];
        if(rde_sscanf(_v, "%f,%f,%f,%f@%u", &_d[0], &_d[1], &_d[2], &_d[3], &_frame) == 5) {
            _d[4] = (f32)_frame;
            look_pen_drag_count += look_pen_drag_count < 4u ? 1u : 0u;
        }
    } else if(rde_strncmp(_a, "--laser@", 8) == 0) {
        look_laser_at = (u32)(i32)rde_strtol(_a + 8, NULL, 10);
    } else if((_v = fude_look_value(_a, "--area")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_area, sizeof(look_area), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_area_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--present")) != NULL) {
        look_present    = (i32)rde_strtol(_v, NULL, 10);
        look_present_at = rde_strchr(_v, '@') != NULL ? (u32)(i32)rde_strtol(rde_strchr(_v, '@') + 1, NULL, 10) : 20u;
    } else {
        return false;
    }
    return true;
}

// Reads one argument, _a, when it is one of these flags, and returns whether it was: the line style, the measure in
// hand, the Shapes panel's choices, plans, mechanisms, circuits, the cards (Limits, Value, Repeat, Size, Sheet,
// Kanban), the page's menu, examples, the frame rate, Align, Make part, bodies, Play's slider, Combine, lines, points,
// the Shapes tool, the census, boards and Sculpt.
RDE_INTERNAL b8 look_read_cards(const c8* _a) {
    const c8* _v;
    if((_v = fude_look_value(_a, "--line-style")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_line_style[0], &look_line_style[1]);
    } else if((_v = fude_look_value(_a, "--measure-tool")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_measure_tool[0], &look_measure_tool[1]);
    } else if((_v = fude_look_value(_a, "--shapes-choice")) != NULL && look_shapes_choice_count < 4u) {
        if(rde_sscanf(_v, "%u@%u", &look_shapes_choices[look_shapes_choice_count][0], &look_shapes_choices[look_shapes_choice_count][1]) == 2) {
            look_shapes_choice_count++;
        }
    } else if((_v = fude_look_value(_a, "--plan")) != NULL && look_plan_count < 2u) {
        if(rde_sscanf(_v, "%u@%u", &look_plan[look_plan_count][0], &look_plan[look_plan_count][1]) == 2) {
            look_plan_count++;
        }
    } else if((_v = fude_look_value(_a, "--mech")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_mech[0], &look_mech[1]);
    } else if((_v = fude_look_value(_a, "--limits")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        if(_at != NULL) {
            rde_snprintf(look_limits, sizeof(look_limits), "%.*s", (int)(_at - _v), _v);
            look_limits_frame = (u32)(i32)rde_strtol(_at + 1, NULL, 10);
        }
    } else if((_v = fude_look_value(_a, "--value")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        if(_at != NULL) {
            rde_snprintf(look_value, sizeof(look_value), "%.*s", (int)(_at - _v), _v);
            look_value_frame = (u32)(i32)rde_strtol(_at + 1, NULL, 10);
        }
    } else if((_v = fude_look_value(_a, "--context")) != NULL) {
        u32 _frame = 0;
        if(rde_sscanf(_v, "%f,%f@%u", &look_context[0], &look_context[1], &_frame) == 3) {
            look_context[2] = (f32)_frame;
        }
    } else if((_v = fude_look_value(_a, "--examples")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_examples[0], &look_examples[1]);
    } else if(fude_look_is(_a, "--fps")) {
        look_fps = true;
    } else if((_v = fude_look_value(_a, "--example")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_example[0], &look_example[1]);
    } else if(rde_strncmp(_a, "--context-delete@", 17) == 0) {
        look_context_delete = (u32)(i32)rde_strtol(_a + 17, NULL, 10);
    } else if((_v = fude_look_value(_a, "--align")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        if(_at != NULL) {
            rde_snprintf(look_align, sizeof(look_align), "%.*s", (int)(_at - _v), _v);
            look_align_frame = (u32)(i32)rde_strtol(_at + 1, NULL, 10);
        }
    } else if(rde_strncmp(_a, "--make-part@", 12) == 0) {
        look_make_part = (u32)(i32)rde_strtol(_a + 12, NULL, 10);
    } else if((_v = fude_look_value(_a, "--circuit2")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_circuit2[0], &look_circuit2[1]);
    } else if((_v = fude_look_value(_a, "--body")) != NULL) {
        rde_sscanf(_v, "%u,%u,%u,%u@%u", &look_body[0], &look_body[1], &look_body[2], &look_body[3], &look_body[4]);
    } else if((_v = fude_look_value(_a, "--slider")) != NULL) {
        u32 _pc = 0;
        if(rde_sscanf(_v, "%u,%u@%u", &look_slider[0], &_pc, &look_slider[1]) == 3) {
            look_slider_t = (f64)_pc / 100.0;
        }
    } else if(rde_strncmp(_a, "--mech-run@", 11) == 0) {
        look_mech_run = (u32)(i32)rde_strtol(_a + 11, NULL, 10);
    } else if((_v = fude_look_value(_a, "--circuit")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_circuit[0], &look_circuit[1]);
    } else if(rde_strncmp(_a, "--circuit-run@", 14) == 0) {
        look_circuit_run = (u32)(i32)rde_strtol(_a + 14, NULL, 10);
    } else if((_v = fude_look_value(_a, "--combine")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_combine[0], &look_combine[1]);
    } else if((_v = fude_look_value(_a, "--lines")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_lines[0], &look_lines[1]);
    } else if((_v = fude_look_value(_a, "--point")) != NULL && look_point_count < 6u) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_points[look_point_count], sizeof(look_points[0]), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_point_at[look_point_count++] = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--shape-tool")) != NULL) {
        u32* _st = look_shape_tools[look_shape_tool_count < 4u ? look_shape_tool_count : 3u];
        if(rde_sscanf(_v, "%u@%u", &_st[0], &_st[1]) == 2) {
            look_shape_tool_count += look_shape_tool_count < 4u ? 1u : 0u;
        }
    } else if((_v = fude_look_value(_a, "--repeat")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_repeat, sizeof(look_repeat), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_repeat_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--size-card")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_size, sizeof(look_size), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_size_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--sheet")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_sheet, sizeof(look_sheet), "%.*s", (int)(_at != NULL ? (usize)(_at - _v) : rde_strlen(_v)), _v);
        look_sheet_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--kanban-card")) != NULL) {
        look_kanban_card        = (i32)rde_strtol(_v, NULL, 10);
        look_kanban_card_at     = rde_strchr(_v, '@') != NULL ? (u32)(i32)rde_strtol(rde_strchr(_v, '@') + 1, NULL, 10) : 20u;
        look_kanban_card_extra  = rde_strchr(_v, '+') != NULL;
        look_kanban_card_insert = rde_strchr(_v, '!') != NULL;
    } else if((_v = fude_look_value(_a, "--kanban")) != NULL) {
        look_kanban    = (i32)rde_strtol(_v, NULL, 10);
        look_kanban_at = rde_strchr(_v, '@') != NULL ? (u32)(i32)rde_strtol(rde_strchr(_v, '@') + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--census")) != NULL) {
        look_census = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if(fude_look_is(_a, "--demo-board") || fude_look_value(_a, "--demo-board") != NULL) {
        _v = fude_look_value(_a, "--demo-board");
        look_demo_board = _v != NULL ? (i32)rde_strtol(_v, NULL, 10) : 0;
        const c8* _at = _v != NULL ? rde_strchr(_v, '@') : NULL;
        look_demo_board_at = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 10u;
    } else if(fude_look_is(_a, "--demo-sculpt") || fude_look_value(_a, "--demo-sculpt") != NULL) {
        _v = fude_look_value(_a, "--demo-sculpt");
        look_demo_sculpt = _v != NULL ? (u32)(i32)rde_strtol(_v, NULL, 10) : 1u;
    } else {
        return false;
    }
    return true;
}

// Reads one argument, _a, when it is one of these flags, and returns whether it was: a path, a later pan,
// handwriting, the offset pad, an instrument grown or anchored, Fit parts, a tap, the camera at home, the saw, the
// units, the bar's tool, holes, formulas, Select all, To curve, nodes, curves, Offset, layers, the bucket, Arrange,
// Mermaid and the calibration.
RDE_INTERNAL b8 look_read_tools(const c8* _a) {
    const c8* _v;
    if((_v = fude_look_value(_a, "--path-at")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        look_path_frame = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 0u;
        for(const c8* _p = _v; _p != NULL && *_p != 0 && *_p != '@' && look_path_n < 16u;) {
            if(rde_sscanf(_p, "%f,%f", &look_path[look_path_n][0], &look_path[look_path_n][1]) == 2) {
                look_path_n++;
            }
            _p = rde_strchr(_p, ';');
            _p = _p != NULL ? _p + 1 : NULL;
        }
    } else if((_v = fude_look_value(_a, "--pan-later")) != NULL) {
        rde_sscanf(_v, "%f,%f@%f", &look_pan_later[0], &look_pan_later[1], &look_pan_later[2]);
    } else if((_v = fude_look_value(_a, "--handwrite")) != NULL) {
        look_handwrite = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--grip")) != NULL) {
        rde_sscanf(_v, "%f,%f@%f", &look_grip[0], &look_grip[1], &look_grip[2]);
    } else if((_v = fude_look_value(_a, "--grow")) != NULL) {
        rde_sscanf(_v, "%f,%f@%f", &look_grow[0], &look_grow[1], &look_grow[2]);
        look_grow[3] = rde_strchr(_v, '!') != NULL ? 1.0f : 0.0f;
    } else if((_v = fude_look_value(_a, "--anchor")) != NULL) {
        rde_sscanf(_v, "%f,%f,%f@%u", &look_anchor[0], &look_anchor[1], &look_anchor[2], &look_anchor_frame);
    } else if((_v = fude_look_value(_a, "--parts")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        if(_at != NULL) {
            rde_snprintf(look_parts, sizeof(look_parts), "%.*s", (int)(_at - _v), _v);
            look_parts_frame = (u32)(i32)rde_strtol(_at + 1, NULL, 10);
            look_parts_cut   = rde_strchr(_at, '!') != NULL;
        }
    } else if((_v = fude_look_value(_a, "--fit")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        if(_at != NULL) {
            rde_snprintf(look_fit, sizeof(look_fit), "%.*s", (int)(_at - _v), _v);
            look_fit_frame = (u32)(i32)rde_strtol(_at + 1, NULL, 10);
        }
    } else if((_v = fude_look_value(_a, "--tap-at")) != NULL) {
        u32 _frame = 0;
        if(rde_sscanf(_v, "%f,%f@%u", &look_tap_at[0], &look_tap_at[1], &_frame) == 3) {
            look_tap_at[2] = (f32)_frame;
        }
    } else if(fude_look_is(_a, "--home-view")) {
        look_home_view = true;
    } else if((_v = fude_look_value(_a, "--kerf")) != NULL) {
        look_kerf = (f32)rde_strtod(_v, NULL);
    } else if((_v = fude_look_value(_a, "--units")) != NULL) {
        look_units = (i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--bar-tool")) != NULL) {
        look_bar_tool = (i32)rde_strtol(_v, NULL, 10);
    } else if(fude_look_is(_a, "--saw")) {
        look_saw = true;
    } else if((_v = fude_look_value(_a, "--holes")) != NULL) {
        look_holes = rde_strtod(_v, NULL);
    } else if((_v = fude_look_value(_a, "--formula-hand")) != NULL) {
        look_formula_at[0] = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--formula-read")) != NULL) {
        look_formula_at[1] = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--select-all")) != NULL) {
        look_select_all = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--to-curve")) != NULL) {
        look_to_curve = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--nodes")) != NULL) {
        look_nodes = (i32)rde_strtol(_v, NULL, 10);
    } else if(fude_look_is(_a, "--demo-curve")) {
        look_demo_curving = 1u;
    } else if(fude_look_is(_a, "--demo-curve-open")) {
        look_demo_curving = 2u;
    } else if((_v = fude_look_value(_a, "--offset")) != NULL) {
        look_offset = rde_strtod(_v, NULL);
    } else if(fude_look_is(_a, "--demo-layers") || fude_look_value(_a, "--demo-layers") != NULL) {
        _v = fude_look_value(_a, "--demo-layers");
        look_demo_layers = _v != NULL ? (u32)(i32)rde_strtol(_v, NULL, 10) : 1u;
    } else if(fude_look_is(_a, "--demo-bucket")) {
        look_demo_bucket = true;
    } else if((_v = fude_look_value(_a, "--arrange")) != NULL) {
        look_arrange = (i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--mermaid")) != NULL) {
        rde_snprintf(look_mermaid, sizeof(look_mermaid), "%s", _v);
    } else if(fude_look_is(_a, "--calibrate")) {
        look_calibrate = true;
    } else {
        return false;
    }
    return true;
}

// Reads one argument, _a, when it is one of these flags, and returns whether it was: the numpad, the pen's measure,
// a finger's drag, instruments, Clear, Undo and Redo, the eraser's or pen's line, the paper, Pages, exports, the
// brush, the fill, lasso and places demos, flights, the depth's levels, the snap demo, --stress, and what moves the
// camera at launch (--pan, --zoom).
RDE_INTERNAL b8 look_read_view(const c8* _a) {
    fude_zoom_scene*   _s    = &look_page->scene;
    const rde_vec_2I   _size = rde_window_get_size(look_app->window);
    const fude_zoom_v2 _half = { (f64)_size.x * 0.5, (f64)_size.y * 0.5 };
    const c8* _v;
    if((_v = fude_look_value(_a, "--numpad")) != NULL) {
        const c8* _at = rde_strrchr(_v, '@');
        rde_snprintf(look_numpad, sizeof(look_numpad), "%.*s", _at != NULL ? (int)(_at - _v) : (int)rde_strlen(_v), _v);
        look_numpad_frame = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 20u;
    } else if((_v = fude_look_value(_a, "--measure")) != NULL) {
        look_measure = (u8)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--finger-drag")) != NULL) {
        f32* _f = look_finger_drag;
        if(rde_sscanf(_v, "%f,%f,%f,%f,%f,%f@%u", &_f[0], &_f[1], &_f[2], &_f[3], &_f[4], &_f[5], &look_finger_frame) != 7) {
            look_finger_frame = 0;
        }
    } else if((_v = fude_look_value(_a, "--instruments")) != NULL) {
        rde_snprintf(look_instruments, sizeof(look_instruments), "%s", _v);
    } else if((_v = fude_look_value(_a, "--instrument-at")) != NULL && look_instrument_placed < 4u) {
        f32* _p = look_instrument_at[look_instrument_placed];
        // (a kind outside the instruments is dropped here, so the frame that places it can cast it to u32 safely)
        if(rde_sscanf(_v, "%f,%f,%f,%f", &_p[0], &_p[1], &_p[2], &_p[3]) == 4 && _p[0] >= 0.0f && _p[0] < (f32)FUDE_ZOOM_INSTRUMENT_COUNT) {
            _p[0] += 1.0f;
            look_instrument_placed++;
        }
    } else if((_v = fude_look_value(_a, "--clear-at")) != NULL) {
        look_clear_frame = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--undo-at")) != NULL || (_v = fude_look_value(_a, "--redo-at")) != NULL) {
        u32* _f = look_undo_frames[rde_strncmp(_a, "--redo-at", 9) == 0 ? 1 : 0];
        for(u32 _k = 0; _k < 8u && *_v != 0; _k++) {
            _f[_k] = (u32)rde_strtoul(_v, (c8**)&_v, 10);
            if(*_v == ',') {
                _v++;
            }
        }
    } else if((_v = fude_look_value(_a, "--erase-at")) != NULL || (_v = fude_look_value(_a, "--draw-at")) != NULL) {
        look_erase_tool = rde_strncmp(_a, "--draw-at", 9) == 0 ? FUDE_TOOL_DRAW : FUDE_TOOL_ERASE;
        if(rde_sscanf(_v, "%f,%f,%f,%f@%u", &look_erase_line[0], &look_erase_line[1], &look_erase_line[2], &look_erase_line[3], &look_erase_frame) != 5) {
            look_erase_frame = 0;
        }
    } else if((_v = fude_look_value(_a, "--paper-later")) != NULL) {
        look_paper_later[0] = _v[0] >= '0' && _v[0] <= '9' && (u32)(i32)rde_strtol(_v, NULL, 10) < FUDE_PAPER_COUNT ? (u32)(i32)rde_strtol(_v, NULL, 10) :   // A number: canvas.h's FUDE_PAPER_.
                                   rde_strncmp(_v, "lines", 5) == 0 ? FUDE_PAPER_LINES : rde_strncmp(_v, "squares", 7) == 0 ? FUDE_PAPER_SQUARES :
                                   rde_strncmp(_v, "none", 4) == 0 ? FUDE_PAPER_NONE : FUDE_PAPER_DOTS;
        const c8* _at = rde_strchr(_v, '@');
        look_paper_later[1] = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 0u;
    } else if((_v = fude_look_value(_a, "--pages")) != NULL) {
        rde_sscanf(_v, "%u@%u", &look_pages[0], &look_pages[1]);
    } else if((_v = fude_look_value(_a, "--export-at")) != NULL) {
        look_export_at = (u32)(i32)rde_strtol(_v, NULL, 10);
    } else if((_v = fude_look_value(_a, "--export")) != NULL) {
        look_export = rde_strncmp(_v, "svg:", 4) == 0 ? 1 : rde_strncmp(_v, "pdf:", 4) == 0 ? 2 : rde_strncmp(_v, "dxf:", 4) == 0 ? 3 : rde_strncmp(_v, "print:", 6) == 0 ? 4 :
                           rde_strncmp(_v, "check:", 6) == 0 ? 5 : rde_strncmp(_v, "csv:", 4) == 0 ? 6 : rde_strncmp(_v, "letter:", 7) == 0 ? 7 :
                           rde_strncmp(_v, "mp4:", 4) == 0 ? 8 : rde_strncmp(_v, "areas:", 6) == 0 ? 9 : rde_strncmp(_v, "sheets:", 7) == 0 ? 10 : rde_strncmp(_v, "stl:", 4) == 0 ? 11 : 0;
        rde_snprintf(look_export_path, sizeof(look_export_path), "%s", rde_strchr(_v, ':') != NULL ? rde_strchr(_v, ':') + 1 : _v);
    } else if((_v = fude_look_value(_a, "--brush-at")) != NULL) {
        c8 _tool[16] = { 0 };
        look_brush_at   = rde_sscanf(_v, "%f,%f,%15s", &look_brush.x, &look_brush.y, _tool) >= 2;
        look_brush_tool = rde_strcmp(_tool, "erase") == 0 ? (i32)FUDE_TOOL_ERASE : rde_strcmp(_tool, "mark") == 0 ? (i32)FUDE_TOOL_MARK : -1;
    } else if(fude_look_is(_a, "--demo-fill")) {
        look_demo_fill = true;
    } else if(fude_look_is(_a, "--demo-loop")) {
        look_demo_loop = true;
    } else if(fude_look_is(_a, "--demo-places")) {
        look_demo_places = true;
    } else if(fude_look_is(_a, "--fly-home")) {
        look_fly_home = true;
    } else if(fude_look_is(_a, "--depth-levels")) {
        look_depth_levels = true;
    } else if(fude_look_is(_a, "--demo-snap")) {
        look_demo_snap = true;
        look_snap_script();
    } else if(fude_look_is(_a, "--stress") || fude_look_is(_a, "--stress-add")) {
        look_stress      = true;
        look_stress_add  = fude_look_is(_a, "--stress-add");
        look_stress_seed = (u32)rde_engine_get_time_now() * 2654435761u + 7u;
    } else if((_v = fude_look_value(_a, "--paper-is")) != NULL) {
        look_app->canvas->page.paper = _v[0] >= '0' && _v[0] <= '9' && (u32)(i32)rde_strtol(_v, NULL, 10) < FUDE_PAPER_COUNT ? (FUDE_PAPER_)(i32)rde_strtol(_v, NULL, 10) :   // A number: canvas.h's FUDE_PAPER_.
                                       rde_strcmp(_v, "lines") == 0 ? FUDE_PAPER_LINES : rde_strcmp(_v, "squares") == 0 ? FUDE_PAPER_SQUARES : rde_strcmp(_v, "none") == 0 ? FUDE_PAPER_NONE : FUDE_PAPER_DOTS;
    } else if((_v = fude_look_value(_a, "--pan")) != NULL) {
        f64 _x = 0.0, _y = 0.0;
        rde_sscanf(_v, "%lf,%lf", &_x, &_y);
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _x, _y });
    } else if((_v = fude_look_value(_a, "--zoom-later")) != NULL) {
        look_zoom_later = rde_strtod(_v, NULL);
        const c8* _at = rde_strchr(_v, '@');
        look_zoom_frame = _at != NULL ? (u32)(i32)rde_strtol(_at + 1, NULL, 10) : 6u;
    } else if((_v = fude_look_value(_a, "--zoom")) != NULL) {
        look_zoom_by(_s, rde_strtod(_v, NULL), _half);
    } else {
        return false;
    }
    return true;
}

// --- each frame ------------------------------------------------------------------------------

// The demos played frame by frame (--stress, --demo-lasso, --demo-snap, the Shapes, smoothing and
// fill demos), the instruments brought out, a finger's drag, the texts, the bucket's and the layers' demos.
RDE_INTERNAL void look_frame_demos(void) {
    if(look_stress) {
        look_stress_frame();
    }
    if(look_demo_lasso > 0u) {
        look_demo_lasso_frame();
    }
    if(look_demo_snap) {
        look_demo_snap_frame();
    }
    if(look_demo_shapes || look_panel >= 0) {
        look_demo_shapes_frame();
    }
    if(look_demo_smooth) {
        look_demo_smooth_frame();
    }
    if(look_demo_fill) {
        look_demo_fill_frame();
    }
    if(look_instruments[0] != 0 && look_frames == 7u) {
        // Instruments left out from before are put away first, so only the ones asked for show.
        for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
            if(look_page->instruments.tools[_k].shown) {
                fude_zoom_instrument_remove(&look_page->instruments, _k);
            }
        }
    }
    if(look_instruments[0] != 0 && look_frames == 8u) {
        for(const c8* _c = look_instruments; *_c != 0; _c++) {
            if(*_c >= '0' && *_c < '0' + (c8)FUDE_ZOOM_INSTRUMENT_COUNT) {
                fude_zoom_instrument_add(&look_page->instruments, (FUDE_ZOOM_INSTRUMENT_)(*_c - '0'), fude_zoom_page_half(look_page));
            }
        }
        // Each --instrument-at places the first instrument of its kind not placed yet, so two rulers can both be
        // placed.
        b8 _placed[FUDE_ZOOM_INSTRUMENT_MOST] = { 0 };
        for(u32 _i = 0; _i < look_instrument_placed; _i++) {
            const f32* _p = look_instrument_at[_i];
            for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
                fude_zoom_instrument* _t = &look_page->instruments.tools[_k];
                if(_t->shown && !_placed[_k] && _t->kind == (u8)((u32)_p[0] - 1u)) {
                    _t->at      = (fude_zoom_v2){ _p[1], _p[2] };
                    _t->angle   = (f64)_p[3] * RDE_PI / 180.0;
                    _t->stuck   = false;   // It lies there on the drawing from now on.
                    _placed[_k] = true;
                    break;
                }
            }
        }
    }
    if(look_finger_frame > 0 && look_frames >= look_finger_frame && look_frames <= look_finger_frame + 21u) {
        // Down, ten steps to the second point, ten to the third, then up: a frame each.
        const u32 _k = look_frames - look_finger_frame;
        const f32* _f = look_finger_drag;
        const f32  _t = _k <= 10u ? (f32)_k / 10.0f : (f32)(_k - 10u) / 10.0f;
        const f32  _x = _k <= 10u ? _f[0] + (_f[2] - _f[0]) * _t : _f[2] + (_f[4] - _f[2]) * rde_math_min_f32(_t, 1.0f);
        const f32  _y = _k <= 10u ? _f[1] + (_f[3] - _f[1]) * _t : _f[3] + (_f[5] - _f[3]) * rde_math_min_f32(_t, 1.0f);
        fude_zoom_page_look_finger(look_page, _k == 0u ? 0u : (_k <= 20u ? 1u : 2u), (fude_zoom_v2){ _x, _y });   // As a finger.
    }
    for(u32 _i = 0; _i < look_text_count; _i++) {
        f32 _x = 0.0f, _y = 0.0f;
        u32 _sticky = 0;
        c8  _words[1024] = { 0 };
        const c8* _at = rde_strrchr(look_texts[_i], '@');
        if(rde_sscanf(look_texts[_i], "%f,%f,%u,", &_x, &_y, &_sticky) == 3 && _at != NULL && (u32)(i32)rde_strtol(_at + 1, NULL, 10) == look_frames) {
            const c8* _w = look_texts[_i];
            for(u32 _c = 0; _c < 3u && _w != NULL; _c++) {
                _w = rde_strchr(_w, ',');
                _w = _w != NULL ? _w + 1 : NULL;
            }
            if(_w != NULL) {
                rde_snprintf(_words, sizeof(_words), "%.*s", (int)(_at - _w), _w);
                const b8 _from_file = rde_strncmp(_words, "file:", 5) == 0;
                if(_from_file) {   // "file:PATH": the words come from a file.
                    // A file is for words the engine would take for its own options (an = in a formula), or a note's
                    // table, which needs its own line breaks.
                    c8 _file[RDE_MAX_PATH];
                    rde_snprintf(_file, sizeof(_file), "%s", _words + 5);
                    fude_file_read_text(_file, _words, sizeof(_words));
                    while(_words[0] != 0 && _words[rde_strlen(_words) - 1u] == '\n') {
                        _words[rde_strlen(_words) - 1u] = 0;
                    }
                }
                for(c8* _c = _words; *_c != 0 && _sticky != 2u && _sticky != 3u && !_from_file; _c++) {   // A formula keeps its |.
                    if(*_c == '|') { *_c = '\n'; }
                }
                fude_zoom_page_look_text(look_page, (rde_vec_2F){ _x, _y }, (u8)_sticky, _words);
            }
        }
    }
    if(look_demo_bucket && look_frames == 12u) {
        // Four hand-drawn strokes, each separate, round a box; the last stops short of the first.
        const f32 _box[5][2] = { { -200, -150 }, { 200, -150 }, { 200, 150 }, { -200, 150 }, { -200, -142 } };
        for(u32 _side = 0; _side < 4u; _side++) {
            for(u32 _i = 0; _i <= 20u; _i++) {
                const f32 _t = (f32)_i / 20.0f;
                const f32 _x = _box[_side][0] + (_box[_side + 1u][0] - _box[_side][0]) * _t + 3.0f * rde_math_sin_f32(_t * 9.0f);
                const f32 _y = _box[_side][1] + (_box[_side + 1u][1] - _box[_side][1]) * _t + 3.0f * rde_math_cos_f32(_t * 7.0f);
                fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ _x, _y });
            }
            fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
        }
        for(u32 _i = 0; _i <= 40u; _i++) {
            const f32 _a = (f32)(2.0 * RDE_PI) * (f32)_i / 40.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 80.0f + rde_math_cos_f32(_a) * 45.0f, 40.0f + rde_math_sin_f32(_a) * 45.0f });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_demo_layers && look_frames == 18u) {
        fude_zoom_page_look_layers(look_page, 0u, 0u);
        fude_zoom_page_look_layers(look_page, 1u, 0u);
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _a = (f32)(2.0 * RDE_PI) * (f32)_i / 30.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 150.0f + rde_math_cos_f32(_a) * 90.0f, -120.0f + rde_math_sin_f32(_a) * 60.0f });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_demo_layers == 5u && look_frames == 22u) {
        fude_zoom_page_look_layers(look_page, 7u, 0u);   // The new layer (on top, listed first) moved under the first.
    }
    if(look_demo_layers == 1u && look_frames == 22u) {
        fude_zoom_page_look_layers(look_page, 2u, 0u);   // The first layer hidden.
    }
    if(look_demo_layers >= 2u && look_demo_layers <= 4u && look_frames == 22u) {
        fude_zoom_page_look_layers(look_page, 4u, 1u);   // Everything moved onto the second.
    }
    if(look_demo_layers >= 3u && look_demo_layers <= 4u && look_frames == 26u) {
        fude_zoom_page_look_layers(look_page, 0u, 0u);
        fude_zoom_page_look_layers(look_page, 5u, 1u);   // Chosen,
        fude_zoom_page_look_layers(look_page, 5u, 1u);   // then its name shown on the card.
    }
    if(look_demo_layers == 4u && look_frames == 30u) {
        fude_zoom_page_look_rename(look_page, "Cortes CNC");   // And renamed to this.
    }
    if(look_demo_bucket && look_frames == 16u) {
        look_page->fill_tool = true;
        fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ -60.0f, -40.0f });
        look_page->fill_tool = false;
    }
}

// The tools at their frames: a joint, the grain, the lasso's box, a PDF, Find, Sculpt and Nudge, the pen's
// loops, a tap, the camera at home, the saw, the units, the bar's tool, holes, formulas, Select all, To curve,
// nodes, a path, a later pan, handwriting, the offset pad, an instrument grown or anchored, Fit parts and a fillet.
RDE_INTERNAL void look_frame_tools(void) {
    if(look_joint_frame > 0 && look_frames == look_joint_frame) {
        fude_zoom_page_look_joint(look_page, (u8)look_joint[0], look_joint[1], look_joint[2], look_joint[3]);
    }
    if(look_offset_frame > 0 && look_frames == look_offset_frame) {
        fude_zoom_page_look_grain(look_page);
    }
    if(look_loop_frame > 0 && look_frames == look_loop_frame) {
        fude_zoom_page_look_loop(look_page, (rde_vec_2F){ look_loop[0], look_loop[1] }, (rde_vec_2F){ look_loop[2], look_loop[3] });
    }
    if(look_pdf_frame > 0 && look_frames == look_pdf_frame) {
        const c8* _paths[1] = { look_pdf };
        FUDE_ZOOM_PAGE_KIND.imported(look_app, 0u, _paths, 1u);   // As Files would hand it over.
    }
    if(look_find_frame > 0 && look_frames == look_find_frame) {
        fude_zoom_page_look_find(look_page, look_find);
    }
    if(look_demo_sculpt > 0 && look_frames == 10u) {
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_DRAW);
        look_page->tool_seen      = (u8)FUDE_TOOL_DRAW;
        look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
        look_page->smooth_level   = 0u;
        for(u32 _i = 0; _i <= 40u; _i++) {
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ -300.0f + 15.0f * (f32)_i, -40.0f });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_demo_sculpt == 3u && look_frames == 14u) {
        // Nudge: the pen goes from under the line's middle up through it, pushing it along.
        look_page->nudge = true;
        for(u32 _i = 0; _i <= 30u; _i++) {
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 0.0f, -70.0f + 4.0f * (f32)_i });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_demo_sculpt > 0 && look_demo_sculpt < 3u && look_frames == 14u) {
        look_page->sculpt = look_demo_sculpt == 1u;
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _t = (f32)_i / 30.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ -100.0f + 200.0f * _t, -40.0f + 110.0f * rde_math_sin_f32((f32)RDE_PI * _t) });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    for(u32 _l = 0; _l < look_loops; _l++) {
        const f32* _at = look_loop_at[_l];
        if(look_frames != (u32)_at[3]) {
            continue;
        }
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_DRAW);
        look_page->tool_seen      = (u8)FUDE_TOOL_DRAW;
        look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
        for(u32 _i = 0; _i <= 48u; _i++) {
            const f32 _a = (f32)(2.0 * RDE_PI) * (f32)_i / 48.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ _at[0] + rde_math_cos_f32(_a) * _at[2], _at[1] + rde_math_sin_f32(_a) * _at[2] });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_tap_at[2] > 0.0f && look_frames == (u32)look_tap_at[2]) {
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_LASSO);
        look_page->tool_seen      = (u8)FUDE_TOOL_LASSO;
        look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
        fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ look_tap_at[0], look_tap_at[1] });
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ look_tap_at[0], look_tap_at[1] });
    }
    if(look_home_view && look_frames == 2u) {
        fude_zoom_camera_look_at(&look_page->scene, look_page->scene.home != FUDE_ZOOM_NONE ? look_page->scene.home : look_page->scene.root, (fude_zoom_v2){ 0.0, 0.0 }, 1.0);
    }
    if(look_saw && look_frames == 4u) {
        look_page->saw = true;
        fude_toolbar_refresh(&look_app->ui->bar);
    }
    if(look_kerf >= 0.0f && look_frames == 4u) {
        look_page->kerf_mm = look_kerf;
    }
    if(look_units >= 0 && look_units < (i32)FUDE_ZOOM_UNIT_COUNT && look_frames == 4u) {
        look_page->units.unit = (FUDE_ZOOM_UNIT_)look_units;
    }
    if(look_bar_tool >= 0 && look_frames == 8u) {
        fude_toolbar_set_tool(&look_app->ui->bar, (FUDE_TOOL_)look_bar_tool);
    }
    if(look_holes > 0.0 && look_frames == 10u) {
        fude_zoom_page_look_holes(look_page, look_holes);
    }
    for(u8 _k = 0; _k < 2u; _k++) {
        if(look_formula_at[_k] > 0u && look_frames == look_formula_at[_k]) {
            fude_zoom_page_look_formula(look_page, _k);
        }
    }
    if(look_select_all > 0 && look_frames == look_select_all) {
        FUDE_ZOOM_PAGE_KIND.command(look_app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_to_curve > 0 && look_frames == look_to_curve) {
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_LASSO);
        fude_zoom_page_look_to_curve(look_page);
    }
    if(look_nodes >= 0 && look_frames == 18u) {
        fude_zoom_page_look_nodes(look_page, 0u);
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_LASSO);
    }
    if(look_nodes >= 1 && look_frames == 20u) {
        fude_zoom_page_look_nodes(look_page, (u8)look_nodes);
    }
    if(look_path_frame > 0u && look_frames == look_path_frame && look_path_n >= 2u) {
        // Down at the first, ten steps to each next, up at the last.
        fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ look_path[0][0], look_path[0][1] });
        for(u32 _i = 1; _i < look_path_n; _i++) {
            for(u32 _s = 1; _s <= 10u; _s++) {
                const f32 _t = (f32)_s / 10.0f;
                fude_zoom_page_look_pen(look_page, 1u, (rde_vec_2F){ look_path[_i - 1u][0] + (look_path[_i][0] - look_path[_i - 1u][0]) * _t,
                                                                  look_path[_i - 1u][1] + (look_path[_i][1] - look_path[_i - 1u][1]) * _t });
            }
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_pan_later[2] > 0.0f && look_frames == (u32)look_pan_later[2]) {
        fude_zoom_camera_pan(&look_page->scene, (fude_zoom_v2){ look_pan_later[0], look_pan_later[1] });
    }
    if(look_handwrite > 0u && look_frames == look_handwrite) {
        fude_zoom_page_look_handwrite(look_page);
    }
    if(look_grip[2] > 0.0f && look_frames == (u32)look_grip[2]) {
        fude_zoom_page_look_grip(look_page, look_grip[0], look_grip[1]);
    }
    if(look_grow[2] > 0.0f && look_frames == (u32)look_grow[2]) {
        fude_zoom_page_look_grow(look_page, (u32)look_grow[0], (f64)look_grow[1], look_grow[3] > 0.5f);
    }
    if(look_anchor_frame > 0u && look_frames == look_anchor_frame) {
        fude_zoom_page_look_anchor(look_page, (u32)look_anchor[0], (f64)look_anchor[1], (f64)look_anchor[2]);
    }
    if(look_parts_frame > 0u && look_frames == look_parts_frame) {
        fude_zoom_parts_look(&look_page->parts_form, look_parts, look_parts_cut);
    }
    if(look_fit_frame > 0u && look_frames == look_fit_frame) {
        fude_zoom_page_look_fit(look_page, look_fit);
    }
    if(look_fillet_mm >= 0.0 && look_frames == look_fillet_at) {
        fude_zoom_page_look_fillet(look_page, look_fillet_mm);
    }
}

// The panels and screens at their frames: the strip, the library, Kanban, an area, the map, presenting, the
// laser, pieces, custom parts, audio, a crash, the home screen and its welcome, canvases, the pen's taps and drags,
// Kanban's card, the Shapes tool, Repeat, plans, mechanisms and circuits.
RDE_INTERNAL void look_frame_panels(void) {
    for(u32 _k = 0; _k < look_strip_do_count; _k++) {
        if(look_frames == look_strip_do[_k][3]) {
            fude_zoom_page_look_strip_do(look_page, (u8)look_strip_do[_k][0], look_strip_do[_k][1], look_strip_do[_k][2]);
        }
    }
    if(look_strip > 0u && look_frames == look_strip) {
        fude_zoom_page_look_strip(look_page);
    }
    if(look_library >= 0 && look_frames == look_library_at) {
        fude_zoom_page_look_library(look_page, (u8)look_library);
    }
    if(look_kanban >= 0 && look_frames == look_kanban_at) {
        fude_zoom_page_look_kanban(look_page, (u32)look_kanban);
    }
    if(look_area_at > 0u && look_frames == look_area_at) {
        fude_zoom_page_look_area(look_page, look_area);
    }
    if(look_map_at > 0u && look_frames == look_map_at) {
        fude_zoom_page_look_map(look_page);
    }
    if(look_present >= 0 && look_frames == look_present_at) {
        fude_zoom_page_look_present(look_page, (u32)look_present);
    }
    if(look_laser_at > 0u && look_frames == look_laser_at) {
        fude_zoom_page_look_laser(look_page);
    }
    if(look_keep_at > 0u && look_frames == look_keep_at) {
        fude_zoom_page_look_keep(look_page, look_keep);
    }
    if(look_pieces_at > 0u && look_frames == look_pieces_at) {
        fude_zoom_page_look_pieces(look_page);
    }
    if(look_stencil[1] > 0u && look_frames == look_stencil[1]) {
        fude_zoom_page_look_stencil(look_page, look_stencil[0]);
    }
    if(look_piece_put[1] > 0u && look_frames == look_piece_put[1]) {
        fude_zoom_page_look_piece_put(look_page, look_piece_put[0]);
    }
    if(look_custom[2] > 0u && look_frames == look_custom[2]) {
        fude_zoom_page_look_custom(look_page, look_custom[0], look_custom[1], look_custom_name);
    }
    if(look_inside_at > 0u && look_frames == look_inside_at) {
        fude_zoom_page_look_inside(look_page);
    }
    if(look_back_at > 0u && look_frames == look_back_at) {
        fude_zoom_page_look_back(look_page);
    }
    if(look_audio_import_at > 0u && look_frames == look_audio_import_at) {
        fude_zoom_page_look_audio_import(look_page, look_audio_import);
    }
    for(u32 _k = 0; _k < look_audio_do_count; _k++) {
        if(look_frames == look_audio_do[_k][2]) {
            fude_zoom_page_look_audio(look_page, (u8)look_audio_do[_k][0], look_audio_do[_k][1]);
        }
    }
    if(look_crash_at > 0u && look_frames == look_crash_at) {
        rde_crash_note("a look crashing on purpose");
        volatile int* _nowhere = NULL;
        *_nowhere = 1;   // A SIGSEGV.
    }
    if(look_home_at > 0u && look_frames == look_home_at) {
        fude_zoom_page_cover_now(look_page);
        fude_zoom_home_open(look_home);
    }
    if(look_welcome_at > 0u && look_frames == look_welcome_at) {
        // An app with no welcome shows the workshop's, so that the welcome can be looked at in any canvas app.
        if(look_home->welcome_title == FUDE_TEXT_COUNT) {
            look_home->welcome_title    = FUDE_TEXT_ZOOM_WELCOME_TITLE;
            look_home->welcome_lines[0] = FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_EXAMPLES;
            look_home->welcome_lines[1] = FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_BUILD;
            look_home->welcome_lines[2] = FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_DRAW;
            look_home->welcome_first    = FUDE_TEXT_ZOOM_WELCOME_EXAMPLES;
        }
        fude_zoom_home_show_welcome(look_home);
    }
    if(look_canvas_new_at > 0u && look_frames == look_canvas_new_at) {
        const u32 _id = fude_notes_add(look_app->notes, FUDE_NOTE_CANVAS, 0u, look_canvas_new);
        if(_id != 0u) {
            look_app->notes->open = _id;   // The session switches to it when it next looks.
        }
    }
    if(look_canvas[1] > 0u && look_frames == look_canvas[1]) {
        look_app->notes->open = look_canvas[0];   // The session switches to it when it next looks.
    }
    for(u32 _ti = 0; _ti < look_pen_tap_count; _ti++) {
        const f32* _pt = look_pen_taps[_ti];
        if(_pt[2] > 0.0f && look_frames == (u32)_pt[2]) {
            fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ _pt[0], _pt[1] });
            fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ _pt[0], _pt[1] });
        }
    }
    for(u32 _di = 0; _di < look_pen_drag_count; _di++) {
        const f32* _pd = look_pen_drags[_di];
        if(_pd[4] > 0.0f && look_frames >= (u32)_pd[4] && look_frames <= (u32)_pd[4] + 11u) {
            const u32 _k = look_frames - (u32)_pd[4];
            const f32 _t = (f32)(_k < 10u ? _k : 10u) / 10.0f;
            const rde_vec_2F _p = { _pd[0] + (_pd[2] - _pd[0]) * _t, _pd[1] + (_pd[3] - _pd[1]) * _t };
            fude_zoom_page_look_pen(look_page, _k == 0u ? 0u : (_k <= 10u ? 1u : 2u), _p);
        }
    }
    if(look_kanban_card >= 0 && look_frames == look_kanban_card_at) {
        fude_zoom_page_look_kanban_card(look_page, (u32)look_kanban_card, look_kanban_card_extra, look_kanban_card_insert);
    }
    for(u32 _si = 0; _si < look_shape_tool_count; _si++) {
        if(look_shape_tools[_si][1] > 0u && look_frames == look_shape_tools[_si][1]) {
            fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_DRAW);
            look_page->tool_seen      = (u8)FUDE_TOOL_DRAW;
            look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
            look_page->shape_tool     = (u8)look_shape_tools[_si][0];
            look_page->measure_tool   = 0;
        }
    }
    if(look_repeat_at > 0u && look_frames == look_repeat_at) {
        fude_zoom_page_look_repeat(look_page, look_repeat);
    }
    for(u32 _ci = 0; _ci < look_shapes_choice_count; _ci++) {
        if(look_frames == look_shapes_choices[_ci][1]) {
            fude_zoom_page_look_shapes_choice(look_page, look_shapes_choices[_ci][0]);
        }
    }
    for(u32 _pl = 0; _pl < look_plan_count; _pl++) {
        if(look_frames == look_plan[_pl][1]) {
            fude_zoom_page_look_plan(look_page, look_plan[_pl][0]);
        }
    }
    if(look_mech[1] > 0u && look_frames == look_mech[1]) {
        fude_zoom_page_look_mech(look_page, look_mech[0]);
    }
    if(look_mech_run > 0u && look_frames == look_mech_run) {
        fude_zoom_page_look_mech_run(look_page);
    }
    if(look_circuit[1] > 0u && look_frames == look_circuit[1]) {
        fude_zoom_page_look_circuit(look_page, look_circuit[0]);
    }
    if(look_circuit_run > 0u && look_frames == look_circuit_run) {
        fude_zoom_page_look_circuit_run(look_page);
    }
}

// The cards and topics at their frames: Limits, Value, Align, the page's menu, the frame rate, examples, Make
// part, bodies, Play's slider, Combine, lines, points, topics, the line style, the measures, Size, Sheet, tidying
// and symbols.
RDE_INTERNAL void look_frame_cards(void) {
    if(look_limits_frame > 0u && look_frames == look_limits_frame) {
        c8 _id[48] = { 0 }, _typed[24] = { 0 };
        i32 _preset = -1;
        u32 _field = 0, _unit = 0, _finish = 0;
        const c8* _c1 = rde_strchr(look_limits, ',');
        if(_c1 != NULL && rde_sscanf(_c1 + 1, "%d,%u,%23[^,],%u,%u", &_preset, &_field, _typed, &_unit, &_finish) >= 2) {
            rde_snprintf(_id, sizeof(_id), "%.*s", (int)(_c1 - look_limits), look_limits);
            fude_zoom_page_look_limits(look_page, _id, _preset, _field, rde_strcmp(_typed, "-") == 0 ? "" : _typed, _unit, _finish);
        }
    }
    if(look_value_frame > 0u && look_frames == look_value_frame) {
        c8 _id[48] = { 0 }, _typed[24] = { 0 };
        u32 _unit = 0, _way = 0;
        const c8* _c1 = rde_strchr(look_value, ',');
        const c8* _c2 = _c1 != NULL ? rde_strchr(_c1 + 1, ',') : NULL;
        if(_c2 != NULL) {
            rde_snprintf(_id, sizeof(_id), "%.*s", (int)(_c1 - look_value), look_value);
            rde_snprintf(_typed, sizeof(_typed), "%.*s", (int)(_c2 - _c1 - 1), _c1 + 1);
            rde_sscanf(_c2 + 1, "%u,%u", &_unit, &_way);
            fude_zoom_page_look_value(look_page, _id, _typed, _unit, _way);
        }
    }
    if(look_align_frame > 0u && look_frames == look_align_frame) {
        c8 _id[40] = { 0 };
        f64 _dx = 0.0, _dy = 0.0;
        const c8* _c1 = rde_strchr(look_align, ',');
        if(_c1 != NULL && rde_sscanf(_c1 + 1, "%lf,%lf", &_dx, &_dy) == 2) {
            rde_snprintf(_id, sizeof(_id), "%.*s", (int)(_c1 - look_align), look_align);
            fude_zoom_page_look_align(look_page, _id, _dx, _dy);
        }
    }
    if(look_context[2] > 0.0f && look_frames == (u32)look_context[2]) {
        fude_zoom_page_look_context(look_page, (rde_vec_2F){ look_context[0], look_context[1] }, false);
    }
    if(look_fps && look_frames == 2u) {
        fude_zoom_page_look_fps(look_page, true);
    }
    if(look_example[1] > 0u && look_frames == look_example[1]) {
        fude_zoom_page_look_draw_example(look_page, look_example[0]);
    }
    if(look_examples[1] > 0u && look_frames == look_examples[1]) {
        fude_zoom_page_look_examples(look_page, look_examples[0]);
    }
    if(look_context_delete > 0u && look_frames == look_context_delete) {
        fude_zoom_page_look_context(look_page, (rde_vec_2F){ 0.0f, 0.0f }, true);
    }
    if(look_make_part > 0u && look_frames == look_make_part) {
        fude_zoom_page_look_make_part(look_page);
    }
    if(look_circuit2[1] > 0u && look_frames == look_circuit2[1]) {
        fude_zoom_page_look_circuit(look_page, look_circuit2[0]);
    }
    if(look_body[4] > 0u && look_frames == look_body[4]) {
        fude_zoom_page_look_body(look_page, look_body[0] != 0u, look_body[1], look_body[2] != 0u, look_body[3] != 0u);
    }
    if(look_slider[1] > 0u && look_frames == look_slider[1]) {
        fude_zoom_page_look_slider(look_page, look_slider[0], look_slider_t);
    }
    if(look_combine[1] > 0u && look_frames == look_combine[1]) {
        fude_zoom_page_look_combine(look_page, look_combine[0]);
    }
    if(look_lines[1] > 0u && look_frames == look_lines[1]) {
        fude_zoom_page_look_lines(look_page, look_lines[0]);
    }
    for(u32 _pi = 0; _pi < look_point_count; _pi++) {
        if(look_frames == look_point_at[_pi]) {
            fude_zoom_page_look_point(look_page, look_points[_pi]);
        }
    }
    if(look_topic[1] > 0u && look_frames == look_topic[1]) {
        look_page->topic = (u8)look_topic[0];
    }
    if(look_switch_topic[1] > 0u && look_frames == look_switch_topic[1]) {
        fude_zoom_page_look_topic(look_page, look_switch_topic[0]);
    }
    if(look_line_style[1] > 0u && look_frames == look_line_style[1]) {
        look_page->line_style = (u8)look_line_style[0];
    }
    if(look_measure_tool[1] > 0u && look_frames == look_measure_tool[1]) {
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_DRAW);
        look_page->tool_seen      = (u8)FUDE_TOOL_DRAW;
        look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
        look_page->shape_tool     = 0;
        look_page->measure_tool   = (u8)look_measure_tool[0];
    }
    if(look_size_at > 0u && look_frames == look_size_at) {
        fude_zoom_page_look_size(look_page, look_size);
    }
    if(look_sheet_at > 0u && look_frames == look_sheet_at) {
        fude_zoom_page_look_sheet(look_page, look_sheet);
    }
    if(look_tidy_at > 0 && look_frames == look_tidy_at) {
        fude_zoom_page_look_tidy(look_page);
    }
    for(u32 _k = 0; _k < look_symbol_count; _k++) {
        if(look_frames == look_symbol_at[_k]) {
            c8 _id[64] = { 0 };
            f64 _dx = 0.0, _dy = 0.0;
            const c8* _p = look_symbols[_k];
            const c8* _c1 = rde_strchr(_p, ',');
            const c8* _c2 = _c1 != NULL ? rde_strchr(_c1 + 1, ',') : NULL;
            const c8* _c3 = _c2 != NULL ? rde_strchr(_c2 + 1, ',') : NULL;
            rde_snprintf(_id, sizeof(_id), "%.*s", _c1 != NULL ? (int)(_c1 - _p) : (int)rde_strlen(_p), _p);
            _dx = _c1 != NULL ? rde_strtod(_c1 + 1, NULL) : 0.0;
            _dy = _c2 != NULL ? rde_strtod(_c2 + 1, NULL) : 0.0;
            fude_zoom_page_look_symbol(look_page, _id, _c3 != NULL ? _c3 + 1 : "", _dx, _dy);
        }
    }
}

// --census: what is alive on the canvas, logged at its frame.
RDE_INTERNAL void look_frame_census(void) {
    if(look_census > 0 && look_frames == look_census) {
        static const c8* const _types[] = { "?", "line", "rect", "ellipse", "polygon", "dimension", "arrow", "board", "path", "arc", "symbol", "sheet" };
        u32 _strokes = 0, _texts = 0, _fills = 0, _shapes[sizeof(_types) / sizeof(_types[0])] = { 0 };
        for(u32 _i = 0; _i < fude_zoom_scene_object_count(&look_page->scene); _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(&look_page->scene, _i);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                continue;
            }
            _strokes += _o->kind == FUDE_ZOOM_KIND_STROKE ? 1u : 0u;
            _texts   += _o->kind == FUDE_ZOOM_KIND_TEXT ? 1u : 0u;
            _fills   += _o->kind == FUDE_ZOOM_KIND_FILL ? 1u : 0u;
            if(_o->kind == FUDE_ZOOM_KIND_STROKE) {
                rde_log_color(RDE_LOG_COLOR_GREEN, "census: stroke %u: %u points, box %.1f %.1f %.1f %.1f, flags %u", _i, _o->count,
                              _o->box.min_x, _o->box.min_y, _o->box.max_x, _o->box.max_y, (u32)_o->flags);
                fude_zoom_qpoint _q[48];
                if(_o->count <= 48u && fude_zoom_scene_points(&look_page->scene, _i, _q)) {
                    for(u32 _p = 0; _p < _o->count; _p++) {
                        const fude_zoom_v2 _at = fude_zoom_scene_point_at(_o, &_q[_p]);
                        rde_log_color(RDE_LOG_COLOR_GREEN, "census:   %.2f %.2f r %.2f", _at.x, _at.y, (f64)fude_zoom_scene_radius_at(_o, &_q[_p]));
                    }
                }
            }
            if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                _shapes[_o->channels < sizeof(_types) / sizeof(_types[0]) ? _o->channels : 0u]++;
            }
        }
        c8 _said[256];
        usize _at = (usize)rde_snprintf(_said, sizeof(_said), "%u strokes, %u texts, %u fills", _strokes, _texts, _fills);
        for(u32 _t = 1; _t < sizeof(_types) / sizeof(_types[0]) && _at < sizeof(_said); _t++) {
            if(_shapes[_t] > 0) {
                _at += (usize)rde_snprintf(_said + _at, sizeof(_said) - _at, ", %u %s", _shapes[_t], _types[_t]);
            }
        }
        rde_log_color(RDE_LOG_COLOR_GREEN, "census: %s", _said);
    }
}

// The rest at their frames, in this order: a board, a drive, a curve, Offset, Arrange, Mermaid, the
// calibration, the numpad, Clear, Undo and Redo, the paper, Pages, the eraser's or pen's line, an export, the
// brush, the lasso's loop, places, flights, the depth's levels, and a later zoom.
RDE_INTERNAL void look_frame_last(void) {
    if(look_demo_board >= 0 && look_frames == look_demo_board_at) {
        fude_zoom_page_look_board(look_page, (u8)look_demo_board);
    }
    if(look_demo_drive >= 1u && look_frames == 10u) {
        fude_zoom_page_look_drive(look_page, 0u);
    }
    if(look_demo_drive >= 2u && look_frames == 14u) {
        fude_zoom_page_look_drive(look_page, 1u);
    }
    if(look_demo_curving != 0u && look_frames == 14u) {
        look_page->curve_tool = true;
        const f32 _pts[6][2] = { { -300, 0 }, { -150, 150 }, { 0, -100 }, { 150, 150 }, { 300, 0 }, { 300, 0 } };
        const u32 _taps = look_demo_curving == 2u ? 3u : 6u;
        for(u32 _i = 0; _i < _taps; _i++) {
            fude_zoom_page_look_pen(look_page, 0u, (rde_vec_2F){ _pts[_i][0], _pts[_i][1] });
            fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ _pts[_i][0], _pts[_i][1] });
        }
        look_page->curve_tool = look_demo_curving == 2u;   // Left open: still in hand.
    }
    if(look_offset != 0.0 && look_frames == 20u) {
        FUDE_ZOOM_PAGE_KIND.command(look_app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f });
        fude_zoom_page_look_offset(look_page, look_offset);
        FUDE_ZOOM_PAGE_KIND.command(look_app, FUDE_PAGE_CMD_DESELECT, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_arrange >= 0 && look_frames == 20u) {
        FUDE_ZOOM_PAGE_KIND.command(look_app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_arrange >= 0 && look_frames == 22u) {
        fude_zoom_select_arrange(&look_page->selection, &look_page->scene, (FUDE_ZOOM_ARRANGE_)look_arrange);
    }
    if(look_mermaid[0] != 0 && look_frames == 15u) {
        static c8 _text[16384];
        if(fude_file_read_text(look_mermaid, _text, sizeof(_text)) > 0u) {
            fude_zoom_page_look_mermaid(look_page, _text);
        }
    }
    if(look_calibrate && look_frames == 10u) {
        fude_zoom_page_look_calibrate(look_page);
    }
    if(look_numpad_frame > 0 && look_frames == look_numpad_frame) {
        fude_zoom_page_look_numpad(look_page, look_numpad);
    }
    if(look_clear_frame > 0 && look_frames == look_clear_frame) {
        FUDE_ZOOM_PAGE_KIND.clear(look_app);
    }
    for(u32 _k = 0; _k < 8u; _k++) {
        if(look_undo_frames[0][_k] > 0 && look_frames == look_undo_frames[0][_k]) {
            FUDE_ZOOM_PAGE_KIND.undo(look_app);
        }
        if(look_undo_frames[1][_k] > 0 && look_frames == look_undo_frames[1][_k]) {
            FUDE_ZOOM_PAGE_KIND.redo(look_app);
        }
    }
    if(look_paper_later[1] > 0u && look_frames == look_paper_later[1]) {
        look_app->canvas->page.paper = (FUDE_PAPER_)look_paper_later[0];
    }
    if(look_pages[1] > 0u && look_frames == look_pages[1]) {
        fude_zoom_page_look_pages(look_page, look_pages[0]);
    }
    if(look_erase_frame > 0 && look_frames == look_erase_frame) {
        fude_toolbar_set_tool(&look_app->ui->bar, look_erase_tool);
        look_page->tool_seen      = (u8)look_erase_tool;
        look_page->tool_taps_seen = look_app->ui->bar.tool_taps;
        look_page->measure_tool   = look_measure;
        for(u32 _i = 0; _i <= 30u; _i++) {
            const f32 _t = (f32)_i / 30.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ look_erase_line[0] + (look_erase_line[2] - look_erase_line[0]) * _t,
                                                                          look_erase_line[1] + (look_erase_line[3] - look_erase_line[1]) * _t });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_export >= 0 && look_frames == look_export_at) {
        fude_zoom_page_look_export(look_page, (u8)look_export, look_export_path);
    }
    if(look_brush_at) {
        look_page->hover_at = look_brush;
        look_page->hover_on = true;
        if(look_brush_tool >= 0 && look_frames == 15u) {
            fude_toolbar_set_tool(&look_app->ui->bar, (FUDE_TOOL_)look_brush_tool);
        }
    }
    if(look_demo_loop && look_frames == 12u) {
        fude_toolbar_set_tool(&look_app->ui->bar, FUDE_TOOL_LASSO);
        look_page->tool_seen = (u8)FUDE_TOOL_LASSO;
        for(u32 _i = 0; _i <= 40u; _i++) {
            const f32 _a = 3.0f * (f32)_i / 40.0f;
            fude_zoom_page_look_pen(look_page, _i == 0 ? 0u : 1u, (rde_vec_2F){ 100.0f + rde_math_cos_f32(_a) * 150.0f, -100.0f + rde_math_sin_f32(_a) * 150.0f });
        }
        fude_zoom_page_look_pen(look_page, 2u, (rde_vec_2F){ 0.0f, 0.0f });
    }
    if(look_demo_places) {
        if(look_frames == 10u || look_frames == 14u) {
            fude_zoom_page_look_nav(look_page, 3u);
        } else if(look_frames == 12u) {
            fude_zoom_camera_zoom_at(&look_page->scene, (fude_zoom_v2){ 0.0, 0.0 }, 20.0);
            fude_zoom_camera_settle(&look_page->scene, (fude_zoom_v2){ 512.0, 384.0 });
        } else if(look_frames == 20u) {
            fude_zoom_page_look_nav(look_page, 4u);
        }
    }
    if(look_fly_home && look_frames == 10u) {
        fude_zoom_page_look_nav(look_page, 0u);
    }
    if(look_depth_levels && look_frames == 20u) {
        fude_zoom_page_look_nav(look_page, 1u);
    }
    if(look_frames == look_zoom_frame && look_zoom_later > 0.0) {
        const rde_vec_2I _size = rde_window_get_size(look_app->window);
        look_zoom_by(&look_page->scene, look_zoom_later, (fude_zoom_v2){ (f64)_size.x * 0.5, (f64)_size.y * 0.5 });
    }
}

// --- what the shell calls ---------------------------------------------------------------------

// See the header. Each argument is read by the first reader that knows it, in this order, as one chain would.
void fude_zoom_look_start(fude_app* _app, fude_zoom_page* _page, fude_zoom_home* _home, i32 _argc, c8** _argv) {
    look_app   = _app;
    look_page  = _page;
    look_home  = _home;
    look_given = _argc > 1;
    for(i32 _i = 1; _i < _argc; _i++) {
        (void)(look_read_demos(_argv[_i]) || look_read_panels(_argv[_i]) || look_read_cards(_argv[_i]) || look_read_tools(_argv[_i]) ||
               look_read_view(_argv[_i]));
    }
}

// See the header.
void fude_zoom_look_frame(void) {
    if(!look_given) {
        return;
    }
    look_frames++;
    look_frame_demos();
    look_frame_tools();
    look_frame_panels();
    look_frame_cards();
    look_frame_census();
    look_frame_last();
}
