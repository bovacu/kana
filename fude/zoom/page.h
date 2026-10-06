// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGE
#define FUDE_ZOOM_PAGE

#include "rde.h"
#include "drawing/app/extension.h"
#include "drawing/ink/ink.h"
#include "zoom/scene.h"
#include "zoom/render.h"
#include "zoom/erase.h"
#include "zoom/zfile.h"
#include "zoom/select.h"
#include "zoom/nav.h"
#include "zoom/instrument.h"
#include "zoom/partsform.h"
#include "zoom/kanbanform.h"
#include "zoom/sheetform.h"
#include "zoom/sizeform.h"
#include "zoom/repeatform.h"
#include "zoom/map.h"
#include "zoom/piece.h"
#include "zoom/snap.h"
#include "zoom/numpad.h"
#include "zoom/pdfview.h"
#include "zoom/video.h"
#include "zoom/handfind.h"
#include "zoom/symbol.h"

// ===========================================================================
// The deep-zoom page: Sketching's, in place of the ink page (extension.h's
// page kind). The same hands as everywhere in fude:
//
//   PEN            draws (the toolbar's pen or marker), or erases (its Erase, or
//                  the pen's eraser end) — the eraser's mode: Erase pressed
//                  again, or E on a computer (partial, whole strokes, trim) —
//                  or, with the Lasso, loops things to select them, then drags
//                  them, scales them by a corner, turns them by the knob (select.h)
//   PEN HELD       at a stroke's end: it snaps to the shape it looks like (shape.h),
//                  which then follows the pen — bigger, smaller, turned — until it
//                  lifts; one undo brings the stroke back as drawn
//   PEN, SHAPES    with a shape chosen in the toolbar's Shapes (a line, a rectangle,
//                  an ellipse, a triangle; filled or not): dragged out from where
//                  it goes down to where it lifts
//   PICTURE        the toolbar's Picture: Photos or Files; what comes in is put in
//                  the middle of the screen, selected (with the Lasso) to be moved,
//                  scaled and turned at once
//   1 FINGER HELD  the page's menu (Paste, Select all), when fingers do not draw
//   1 FINGER       pans — or, with the toolbar's hand on, draws
//   2 FINGERS      pinch-zoom (and pan), as far in or out as anyone likes
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
// On a computer: the left button draws, the right drags the page (a right
// click without dragging: the page's menu), the wheel zooms where the mouse is;
// Z undo, Y redo, C clear, R back to the start, E the eraser's mode, Backspace
// or Delete the selection, Escape lets it go.
//
// A stroke is captured by an ink of its own (ink.h, exactly Kana's pen: its
// pressure, its pens without pressure, its sampling) in the camera frame's
// units round where it began, and becomes the canvas's (scene.h) when the pen
// lifts. Brush colour, size and width come from the app's ink, which the
// toolbar and Settings set as everywhere.
//
// The canvas's file (zfile.h) is <save dir>/notes/<id>.zoom; its journal is
// appended about every second while there is anything new, and everything is
// checkpointed when the canvas is left or the app goes.
// ===========================================================================

struct fude_app;

#define FUDE_ZOOM_PAGE_FLUSH         1.0     // seconds between journal appends while drawing
#define FUDE_ZOOM_PAGE_FINGERS       5u
#define FUDE_ZOOM_PAGE_LONG_PRESS    0.5     // seconds a still finger is held for the page's menu
#define FUDE_ZOOM_PAGE_HOLD          0.55    // seconds the pen rests at a stroke's end before it snaps to a shape
#define FUDE_ZOOM_PAGE_HOLD_SLOP     3.0f    // ...moving less than this (screen units)

// The paper's spacing on screen, at least (and under twice that): the dots', and
// lines' and squares' by Settings' Lines & squares size.
#define FUDE_ZOOM_PAGE_DOTS          40.0
#define FUDE_ZOOM_PAGE_PAPER_SMALL   28.0
#define FUDE_ZOOM_PAGE_PAPER_MEDIUM  44.0
#define FUDE_ZOOM_PAGE_PAPER_LARGE   72.0

// The canvas's real size: a unit of its home frame (a screen point at ×1) is
// this many millimetres — rulers, measures and dimensions read in it, and
// deeper frames in their share of it.
#define FUDE_ZOOM_PAGE_MM_PER_UNIT   1.0
#define FUDE_ZOOM_PAGE_LINE_CHIP     6.0     // seconds a new line's length chip stays

// The instruments as they lie (fude_zoom_page_tools_step): enough to put them back.
typedef struct {
    fude_zoom_instrument tools[FUDE_ZOOM_INSTRUMENT_MOST];
    u8                   order[FUDE_ZOOM_INSTRUMENT_MOST];
} fude_zoom_tools_state;

typedef struct {
    fude_zoom_tools_state before, after;
    u64                   at;   // the drawing's steps done before it (counted from its first ever)
} fude_zoom_tools_step;
#define FUDE_ZOOM_TOOLS_STEPS 64u   // the instruments' steps kept, at most (the oldest go)

typedef struct {
    b8         active;
    u64        id;
    rde_vec_2F at;       // screen
    rde_vec_2F start;
} fude_zoom_finger;

#define FUDE_ZOOM_PAGE_BACK   16u    // views Back remembers
#define FUDE_ZOOM_PAGE_CRUMBS 8u     // the depth's levels shown (the top and the nearest above)
#define FUDE_ZOOM_PAGE_PLACES 8u     // bookmarks listed (the newest)
#define FUDE_ZOOM_PAGE_LASER  192u   // the laser pointer's trail: its last points kept
#define FUDE_ZOOM_PAGE_ANCHOR_BUTTONS 7u
#define FUDE_ZOOM_PAGE_PREVIEWS 8u         // the arrows' pictures kept at once   // the offset pad's: ◀ X ▶  ▼ Y ▲  ×
#define FUDE_ZOOM_PAGE_LAYER_ROWS 8u // layers listed
#define FUDE_ZOOM_PAGE_CURVE_NODES FUDE_ZOOM_PATH_NODES   // a curve's points, at most (a path's)

typedef struct fude_zoom_page {
    struct fude_app*   app;
    fude_zoom_scene    scene;
    fude_zoom_renderer renderer;
    fude_zoom_eraser   eraser;
    fude_zoom_file     file;
    fude_ink           capture;      // the stroke being drawn
    fude_zoom_selection selection;   // the lasso's
    u8                 tool_seen;    // the toolbar's tool last frame (another one lets the selection go)
    b8                 long_pressed; // this finger gesture already opened the menu
    u32                canvas;       // the canvas open (notes.h id; 0: none yet)
    b8                 open;
    u32                device;
    u8                 erase_mode;   // FUDE_ZOOM_ERASE_
    u32                erase_taps_seen;
    u32                tool_taps_seen;
    // The Shapes tool: the shape chosen (0: none — the bar's tool is in hand),
    // filled or not, and the one being dragged out (screen).
    u8                 shape_tool;   // FUDE_ZOOM_SHAPE_ (a triangle is a POLYGON)
    b8                 shape_filled;
    u8                 line_style;     // the Shapes' line: shape.h's FUDE_ZOOM_LINE_ (solid, dashed, a centre line)
    f64                stl_mm;         // Export's 3D model: how tall what is not a board stands (0: not asked yet: 3 mm)
    b8                 fill_tool;    // the Fill tool in hand: a tap fills what is closed under it (fill.h)
    b8                 shaping;
    rde_vec_2F         shape_from;

    // The pen (or mouse, or a writing finger) down.
    b8                 drawing;
    b8                 erasing;
    b8                 pen_on_ui;
    u32                draw_frame;
    fude_zoom_v2       origin;       // where the stroke began, that frame's units
    f64                draw_z;
    rde_vec_2F         erase_last;   // screen
    u64                writer;       // the finger drawing (with the hand on)
    b8                 finger_drawing;
    f64                stroke_began;
    // Hold to snap: when the pen last moved, and the shape it snapped to.
    f64                moved_at;
    rde_vec_2F         moved_from;   // screen
    b8                 snapped;
    fude_zoom_shape_fit fit;         // screen units, as recognized
    rde_vec_2F         snap_pen;     // where the pen was when it snapped
    rde_vec_2F         pen_now;      // and now
    // The brush's circle (its size, where it would draw): the pen hovering
    // (or the mouse), its eraser end or not.
    rde_vec_2F         hover_at;     // screen (eased: a pen leaving the glass reads unsteadily)
    b8                 hover_on;
    b8                 hover_eraser;
    f64                hover_quiet;  // not shown before this (the pen just lifted)
    rde_arr TYPE(fude_zoom_v2) snap_scratch;
    // Smoothing (smooth.h): the level for new strokes (FUDE_ZOOM_SMOOTH_, a
    // setting), and the stroke's points as the pen gave them (from origin, the
    // frame's units: the capture holds them smoothed), its first point still
    // unsettled, and the rope's tip.
    u8                 smooth_level;
    u8                 stroke_smooth;   // the stroke's own (none along an instrument)
    rde_arr TYPE(fude_zoom_v2) raw;
    rde_arr TYPE(fude_zoom_v2) smoothed;
    u32                smooth_from;
    fude_zoom_v2       rope_tip;

    // The instruments (instrument.h): the ruler, the set squares and the
    // protractor; the pen holding one, or drawing along one's edge.
    fude_zoom_instruments instruments;
    fude_zoom_pdfview  pdf;             // a canvas over a PDF: its pages under the ink (pdfview.h; none open: a free canvas)
    b8                 instrument_pen;
    b8                 ruled;
    // Measuring (the Instruments' Tape measure and Dimension): which (0 none,
    // 1 the tape, 2 a dimension), its two ends in the frame it measures in, the
    // tape left showing after the pen lifts, and what the pen snapped to
    // (snap.h; the Shapes tool's ends snap too). How lengths are written.
    u8                 measure_tool;
    b8                 measuring;
    b8                 tape_shown;
    u32                measure_frame;
    fude_zoom_v2       measure_a, measure_b;
    fude_zoom_snap     snap_hit;
    fude_zoom_v2       snap_from;      // the point a line being drawn or measured comes from (screen; snap.h's PERP, PARALLEL)
    fude_zoom_snap     measure_from_hit;   // what a measure began on (a dimension drawn from it follows it: connect.h)
    fude_zoom_v2       measure_raw;        // where the pen came down to measure, before it snapped (screen)
    b8                 snap_from_on;
    fude_zoom_units_style units;
    b8                 ruled_straight;  // the pen along a straight edge (its line becomes a LINE shape)
    b8                 ruled_round;     // ...along a round one (its line becomes an ARC, or a circle all the way round)
    // A line just drawn: its length on a chip by its end for a moment, which
    // a tap opens the numpad on, to give it exactly (numpad.h).
    fude_zoom_numpad   numpad;
    // An arrow's preview (an empty screen's marks): what it points at, drawn once into a small picture
    // of its own (fude_zoom_page_render_offscreen, one a frame), then shown by it — not drawn again
    // each frame. Kept while the same thing is pointed at and the canvas is as it was.
    rde_render_texture* previews[FUDE_ZOOM_PAGE_PREVIEWS];
    u32                 preview_frame[FUDE_ZOOM_PAGE_PREVIEWS];
    fude_zoom_box       preview_box[FUDE_ZOOM_PAGE_PREVIEWS];
    u32                 preview_rev[FUDE_ZOOM_PAGE_PREVIEWS];
    b8                  preview_ready[FUDE_ZOOM_PAGE_PREVIEWS];
    i8                  mark_preview[FUDE_ZOOM_NAV_MARKS];   // each mark's (-1: none)
    fude_zoom_camera   camera_kept;     // the camera as the file has it (its last checkpoint's): leaving elsewhere, one more
    u64                tools_kept;      // the instruments file as last written (its bytes' hash; 0: not yet)
    // The instruments' undo, beside the drawing's (in memory only): a step each time they were
    // left changed — taken out, put away, moved, turned, made bigger, nudged — what they were
    // and became, and where it falls among the drawing's steps (fude_zoom_tools_step).
    rde_arr            tools_steps;
    u32                tools_done;      // the steps done (the rest: undone, to redo)
    fude_zoom_tools_state tools_then;   // the instruments as the last step left them
    b8                 tools_then_set;
    b8                 tools_absorb;    // drawn along: what that changed (a compass turned with the pen) is no step
    u64                tools_seen;      // the drawing's pushes when looked last (one since: no redo of an instrument step)
    fude_zoom_parts_form parts_form;    // Fit parts' card: a board's parts, their sizes typed
    fude_zoom_kanban_form kanban_form;  // Insert's Kanban board: its columns typed
    fude_zoom_sheet_form  sheet_form;   // Insert's Measured sheet (or a sheet's size tapped): its paper, scale and size
    u32                   sheet_editing;       // the sheet changed (FUDE_ZOOM_NONE: a new one)
    f64                   sheet_last[4];       // the last one made: width, height (mm), scale, bits (0 width: none yet)
    fude_zoom_size_form   size_form;    // what the lasso holds made an exact size (its size chip tapped)
    fude_zoom_repeat_form repeat_form;  // the lasso's Repeat: copies in a row, round a point, mirrored
    b8                    size_chip;    // the chip says the lasso's size (not a line's, a board's, a sheet's)
    u64                   size_key;     // what the lasso held when size_said was worked out
    c8                    size_said[2u * FUDE_ZOOM_UNITS_TEXT + 8u];
    rde_ui_button*     line_chip;
    u32                line_chip_object;
    f64                line_chip_until;
    b8                 shown_line_chip;
    rde_vec_2F         line_chip_center;
    c8                 line_chip_said[3u * FUDE_ZOOM_UNITS_TEXT + 96u];   // (a board's three sizes, its material)
    // Text (the Insert tool's Text and Sticky note): which (0 none, 1 a text,
    // 2 a note); its card — a field (the keyboard's), Cancel, Save — and what it
    // is for: a new one where the pen went down (screen), or the one tapped.
    u8                 text_tool;
    fude_kit_modal     text_card;
    rde_ui_text_editor* text_field;
    rde_ui_button*     text_save;
    rde_ui_button*     text_cancel;
    u32                text_object;    // FUDE_ZOOM_NONE: a new one
    u32                text_frame;
    fude_zoom_v2       text_at;        // a new one's top left, its frame's units
    u8                 text_style;
    rde_vec_2F         text_laid_out;
    // Measurements (Settings' section): the units lengths are written in
    // (0 mm, 1 cm, 2 inches, 3 feet and inches), and the screen's true size —
    // millimetres a point really is (0: not calibrated) — with the calibration
    // under way: a bank card's outline at the size being tried, its buttons.
    u8                 units_choice;
    f32                true_mm_per_point;
    f32                print_x, print_y;      // the printer's factors (settings; 0: never checked)
    f64                print_measured_x;      // the check's first line, while the second is typed
    // A board being made or changed (Insert's Board; a lassoed board's chip): its sizes as typed, one at a time, then its name.
    u32                board_editing;         // the board changed (FUDE_ZOOM_NONE: a new one)
    f64                board_mm[3];           // length, width, thickness
    u8                 board_grain;
    u8                 board_material;   // FUDE_ZOOM_MATERIAL_ (a new board: the last one's)
    f64                fillet_mm;        // the last fillet's radius (0: none yet)
    const c8*          material_words[FUDE_ZOOM_MATERIAL_COUNT];
    rde_ui_button*     material_chips[FUDE_ZOOM_MATERIAL_COUNT];   // on the text card, naming a board
    b8                 calibrating;
    f64                calibrate_mm;
    rde_ui_label*      measure_header;
    rde_ui_label*      units_label;
    rde_ui_button*     units_buttons[4];
    rde_ui_label*      true_label;
    rde_ui_button*     calibrate_button;
    rde_ui_label*      printer_label;
    rde_ui_button*     printer_button;
    rde_ui_button*     calibrate_keys[6];   // much smaller, smaller, bigger, much bigger, Cancel, Save
    b8                 shown_calibrate;
    // Layers (scene.h; the toolbar's Layers): the panel — a row a layer, its
    // name and how many things it holds (a tap draws on it), its eye and its
    // lock, then New layer — and the layers as it lists them.
    b8                 layers_open, shown_layers, layers_dirty;   // (dirty: the rows laid out again)
    rde_vec_2F         layers_bar_at, layers_bar_size;   // the toolbar where the panel last stepped round it
    rde_ui_button*     layer_rows[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_eyes[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_locks[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_ups[FUDE_ZOOM_PAGE_LAYER_ROWS];     // moved over the one above it
    rde_ui_button*     layer_downs[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_drops[FUDE_ZOOM_PAGE_LAYER_ROWS];   // each one's delete
    f32                layers_left, layers_top;   // where the panel was put (its top left)
    rde_ui_button*     layer_add;
    u32                layer_count;
    u16                layer_index[FUDE_ZOOM_PAGE_LAYER_ROWS];
    u8                 layer_flags[FUDE_ZOOM_PAGE_LAYER_ROWS];
    u32                layer_object[FUDE_ZOOM_PAGE_LAYER_ROWS];   // its LAYER object (FUDE_ZOOM_NONE: none yet)
    c8                 layer_names[FUDE_ZOOM_PAGE_LAYER_ROWS][40];
    u32                layers_seen;     // the scene's revision the layers were last worked out at
    b8                 layer_moving;    // the lasso's To layer: the next row tapped (or New layer) takes the selection
    f64                text_opened_at;  // when the text card came up
    u32                layer_renaming;  // the row the text card renames
    u32                layers_mask_seen;   // ...and their hidden and locked bits
    // Handwriting read as text (the lasso's To text: Google ML Kit, where
    // there is one): what is being read — the strokes, their box on the
    // screen, their colour — while the answer is awaited.
    // The Curve tool (Shapes): the points tapped so far (the frame's units),
    // a smooth curve through them (Catmull-Rom) — the last tapped again
    // finishes it, the first closes it.
    b8                 curve_tool;
    fude_zoom_v2       curve_nodes[FUDE_ZOOM_PAGE_CURVE_NODES];
    u32                curve_count;
    u32                curve_frame;
    // A lassoed path's or polygon's nodes (shape.h): one held and where it is (screen), a double tap's
    // first, and a tap on its line waiting to be a new node (if the pen lifts where it went down).
    i32                node_grab;
    u32                node_object;
    rde_vec_2F         node_at, node_down_at, node_last_at;
    f64                node_tap_time;
    i32                node_tap_index;  // the node last tapped (a tap: pressed and let go without moving), its path, and when
    u32                node_tap_object;
    f64                node_down_time;  // when the node held was pressed
    b8                 node_insert;
    // A path's node last touched shows its handle (shape.h: a Bezier handle, dragged out
    // either way); held by one of its two ends: node_grab's, 1 the way out, -1 the way in.
    i32                node_focus;
    u32                node_focus_object;
    i8                 node_handle;
    rde_vec_2F         lasso_down_at;    // the Lasso's press: a tap (it went nowhere) selects the thing under it
    b8                 lasso_went;
    b8                 lasso_on_selection;
    b8                 sculpt;          // the Smoothing tool's Sculpt: a stroke begun on a line redoes that stretch of it
    b8                 saw;             // the Instruments' Saw: what is drawn cuts the boards it crosses (cut.h), not drawn
    u32                compass_typing;   // the compass whose radius is on the numpad (its slot, and which it was)
    u32                compass_serial;
    u8                 compass_step;      // an ellipse template's: 0 its width on the numpad, 1 its height
    f32                kerf_mm;         // the saw's cut, millimetres wide (settings; 0: never set)
    rde_ui_label*      kerf_label;
    rde_ui_button*     kerf_button;
    rde_ui_button*     kerf_chip;       // the saw's kerf, shown while the Saw is on (tapped: typed)
    u32                fit_stock;       // the board a list of parts is being typed for (the selection's Fit parts)
    // An instrument snapped by one of its points (instrument.h: its keys) and let go:
    // that point in the drawing, and how far across and up the offset pad has moved
    // it from there (◀ X ▶  ▼ Y ▲  ×, bottom middle).
    i32                anchor_tool;     // its slot (-1: none)
    u32                anchor_serial;   // ...and which it was (instrument.h)
    u32                anchor_key;
    u32                anchor_frame;
    fude_zoom_v2       anchor_point;    // in anchor_frame's units
    f64                anchor_mm[2];    // across (+ right), up (+ up)
    u8                 anchor_typing;   // the one on the numpad (0 across, 1 up)
    // Dragged by one hand while the pad is up: slid across or up only (the way the hand
    // first went; -1: not yet clear), from the lengths it had when taken, in the arrows' steps.
    b8                 anchor_sliding;
    i32                anchor_slide_axis;
    fude_zoom_v2       anchor_slide_grab;   // the hand where it took it
    f64                anchor_slide_mm[2];
    rde_ui_button*     anchor_buttons[FUDE_ZOOM_PAGE_ANCHOR_BUTTONS];
    rde_ui_button*     anchor_grip;     // its left end: dragged, the pad moves (and stays where it is left)
    rde_vec_2F         anchor_grip_at;
    b8                 anchor_custom;   // put somewhere by a hand (else bottom middle)
    rde_vec_2F         anchor_custom_at;
    b8                 anchor_dragging;
    u64                anchor_drag_hand;
    rde_vec_2F         anchor_drag_from, anchor_from;
    b8                 shown_anchor;
    c8                 anchor_said[2][64];
    rde_vec_2F         anchor_center;
    f32                anchor_w;
    f32                anchor_screen_w, anchor_bar_x;   // (placed again when these change)
    b8                 shown_kerf_chip;
    rde_vec_2F         kerf_chip_center;
    c8                 kerf_chip_said[96];
    // ...its Nudge: the pen pushes the pen lines near it (each pushed one a copy of its points while the pen is down,
    // the original hidden: nudge_lifted is the renderer's lifted while nudging).
    b8                 nudge, nudging;
    u32                nudge_frame;
    fude_zoom_v2       nudge_at;          // the pen, frame units
    rde_arr            nudge_items;       // fude_zoom_nudged
    rde_arr            nudge_points;      // their points, frame units, one after another
    rde_arr            nudge_radii;       // ...and each one's half-width (f32, frame units: its pressure kept)
    rde_arr            nudge_handles;     // a path's: its nodes' handles (frame units), as its nodes are in nudge_points
    rde_arr            nudge_lifted;      // u8 an object
    // Find: its button by the depth; what was last looked for and which of
    // its matches was last flown to.
    rde_ui_button*     find_button;
    rde_vec_2F         find_center;
    b8                 shown_find;
    c8                 find_query[128];
    u32                find_next;
    u32                find_pdf_base;   // the texts' matches before a PDF's (the PDF's come after them)
    // The lasso's loop over a PDF's own text (pdfview.h): its box (home frame units), while
    // the selection's row offers it (Copy text) — on its own when the loop took no ink.
    b8                 pdf_text_on;
    fude_zoom_box      pdf_text_box;
    // A joint being asked for (Insert's Finger joint, Dovetail): which (1, 2), the question
    // it is at, and its length, finger width or tails, depth so far (mm; tails: a count).
    u8                 joint_kind;
    u8                 joint_step;
    f64                joint_mm[3];
    b8                 reading;
    rde_arr TYPE(u32)  read_strokes;
    fude_zoom_box      read_box;
    rde_color          read_color;
    b8                 read_wanted;       // To text asked for while a line of writing was being read: after it
    // Handwriting for Find (handfind.h): the canvas's lines of writing, each read once (as To
    // text reads) in the background once Find has been asked for on it; what each says, by its key.
    // The diagram library (symbol.h): its panel open, the family shown, how far its grid is scrolled, the
    // hand on it (a tap or a drag through it; the press that put it away, its moves too); the symbol whose
    // text the text card takes.
    b8                 library_open;
    u8                 library_family;
    f64                library_scroll, library_from;
    rde_vec_2F         library_down;
    u64                library_hand, library_gone;
    b8                 library_dragged;
    u32                symbol_editing;
    u32                connector_heads;   // what a new connector's ends are (symbol.h: the library's Connectors tab)
    // The map (map.h): its button by Find, its panel over the bottom left — the canvas from above as a picture
    // drawn off screen (again when what is drawn, its shape or the view's place on it changes), the areas, the
    // places and the view drawn over it — and the hand on it (a tap flies; a drag moves the view).
    rde_ui_button*      map_button;
    rde_vec_2F          map_center;
    b8                  shown_map, map_open;
    f64                 map_pressed_at;      // (a tap on a tablet can come twice: as the touch and as the mouse)
    f64                 map_aspect;          // its map's height over its width (as what it shows was when it opened)
    fude_zoom_box       map_world;           // what it shows, root units
    f64                 map_world_at;
    fude_zoom_box       map_contents;        // what is drawn, root units...
    u32                 map_contents_rev;    // ...as of this revision of the scene
    rde_render_texture* map_picture;
    u32                 map_pw, map_ph;      // its pixels
    b8                  map_stale, map_ready;
    f64                 map_drawn_at;
    rde_color           map_paper;           // the theme's page it was drawn on
    u64                 map_hand, map_gone;
    rde_vec_2F          map_down;
    b8                  map_dragged, map_on_view;
    fude_zoom_v2        map_grab, map_grab_view;   // the hand's point on it when it went down, the view's middle then (root units)
    u32                 map_label_n;               // the areas' names as drawn (to tap): their boxes on the screen
    fude_zoom_box       map_label_box[FUDE_ZOOM_MAP_AREAS];
    u32                 map_label_object[FUDE_ZOOM_MAP_AREAS];
    u32                 map_pin_n;                 // the places' pins as drawn
    rde_vec_2F          map_pin_at[FUDE_ZOOM_MAP_PLACES];
    u32                 map_pin_object[FUDE_ZOOM_MAP_PLACES];
    // Present: the areas in the order they were made (or, none, the places by number), each framed in turn
    // with nothing else on the screen; a tap on the right goes on, on the left back; the pen (or the mouse
    // dragged) is a laser pointer — its trail fading — and the pill at the bottom (◀ 2 / 5 ▶ ×) shows a while
    // after a tap.
    // A lone rectangle, ellipse or diagram shape (an area) held by one of its sides' handles: stretched that way
    // only — the side across it kept where it is — drawn as it will be while held, made so where the pen lifts
    // (one undo step).
    // My pieces (piece.h): the list (read the first time it is wanted), its panel (as the diagram library's: a
    // tile a piece, its picture drawn from its lines, made once), the hand on it, the × waiting for its second tap.
    fude_zoom_pieces    pieces;
    b8                  pieces_open, pieces_dragged;
    b8                  pieces_stencil;      // opened from the Instruments: a tap on a piece lays it as a stencil
    f64                 pieces_scroll, pieces_from;
    rde_vec_2F          pieces_down;
    u64                 pieces_hand, pieces_gone;
    i32                 piece_armed;
    f64                 piece_armed_at;
    rde_arr             piece_points[FUDE_ZOOM_PIECES];
    rde_arr             piece_lines[FUDE_ZOOM_PIECES];
    b8                  piece_lines_ready[FUDE_ZOOM_PIECES];
    u32                 carried_keep;        // a container's contents carried by the lasso's drag: its picks to keep after (0: none)
    b8                  stretching;
    u32                 stretch_object;
    u8                  stretch_side;        // 0 right, 1 top, 2 left, 3 bottom (its own)
    f64                 stretch_half[2];     // its half sizes as taken (its own units)
    f64                 stretch_to;          // where that side goes, along its way (its own units, from its middle)
    b8                  presenting, present_areas;
    u32                 present_step, present_count;
    u32                 present_objects[FUDE_ZOOM_MAP_AREAS];
    f64                 present_touched_at;
    b8                  present_pen_down, present_pen_moved;
    rde_vec_2F          present_pen_at;
    f64                 present_pen_time;
    rde_vec_2F          laser_at[FUDE_ZOOM_PAGE_LASER];
    f64                 laser_time[FUDE_ZOOM_PAGE_LASER];
    b8                  laser_lift[FUDE_ZOOM_PAGE_LASER];   // a new trail from this point (not joined to the one before)
    u32                 laser_head, laser_count;
    b8                 hand_on;
    u32                hand_revision;     // the scene's when its lines were found
    f64                hand_found_at;
    rde_arr            hand_lines;        // fude_zoom_hand_line
    rde_arr            hand_strokes;      // u32
    rde_arr            hand_said;         // fude_zoom_page_hand_said
    u64                hand_asking;       // the line being read now (its key; 0: none)

    // The fingers moving the page.
    fude_zoom_finger   fingers[FUDE_ZOOM_PAGE_FINGERS];
    f64                tap_start;
    u32                tap_fingers;
    b8                 tap_spoiled;

    // A computer's right-button drag (no drag: the page's menu).
    b8                 mouse_panning;
    // A flick (as Kana's canvas): how fast the hand pans (screen units a second, what
    // it moved since the last look at it waiting), and the view carrying on once let go.
    fude_zoom_v2       pan_velocity;
    fude_zoom_v2       pan_pending;
    f64                pan_moved_at;
    b8                 coasting;
    rde_vec_2F         mouse_last;
    rde_vec_2F         mouse_down;
    b8                 mouse_dragged;

    f64                flushed_at;
    rde_arr TYPE(fude_zoom_visible) editable;

    // Getting around (nav.h): the flight under way; where Back goes (the views
    // flights left, newest last — their frames by id, which outlive slots); the
    // marks for an empty screen, and the camera they were found for.
    fude_zoom_flight   flight;
    struct { fude_zoom_id frame; fude_zoom_v2 at; f64 z; } back[FUDE_ZOOM_PAGE_BACK];
    u32                back_count;
    fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
    u32                mark_count;
    fude_zoom_camera   marks_for;
    f64                marks_at;
    // ...and their widgets (the extension's own: ui.h): Back beside the menu,
    // the depth at the bottom left, its levels over it once tapped.
    rde_ui_button*     back_button;
    rde_ui_button*     depth_button;
    rde_ui_button*     crumbs[FUDE_ZOOM_PAGE_CRUMBS];
    fude_zoom_camera   crumb_views[FUDE_ZOOM_PAGE_CRUMBS];
    u32                crumb_count;
    b8                 crumbs_open;
    c8                 depth_said[32];
    rde_vec_2F         back_center, depth_center, crumb_size;
    b8                 shown_back, shown_depth, shown_crumbs;
    rde_vec_2F         laid_out;
    // Places (the bookmarks: scene.h): its button by the depth, and its list
    // over it once tapped — first Mark this view (or Unmark), then the places.
    rde_ui_button*     places_button;
    rde_ui_button*     place_rows[FUDE_ZOOM_PAGE_PLACES + 1u];
    rde_ui_button*     place_drops[FUDE_ZOOM_PAGE_PLACES];   // the × beside each place: let it go
    u32                place_marks[FUDE_ZOOM_PAGE_PLACES];   // each row's bookmark (scene.marks)
    u32                place_count;     // rows of places shown
    i32                place_here;      // the bookmark the view is at (-1: none): row 0 unmarks it
    b8                 places_open, shown_places, shown_place_rows;
    rde_vec_2F         places_center, place_size;

    // Export (export.h): a PNG under way — its file chosen (1), the view drawn
    // into the target (2: read back the frame after, the GPU done with it).
    u8                 export_stage;
    b8                 export_share;      // to the system's share sheet (a tablet's), not a file chosen
    c8                 export_path[1024];
    rde_render_texture* export_target;
    // Exporting what the lasso holds: what is kept (a byte an object), the rest hidden (the
    // PNG's renderer's lifted), and the view framing it — the PNG's, drawn the frame after.
    rde_arr            export_keep;
    rde_arr            export_hidden;
    b8                 export_held;
    fude_zoom_camera   export_view;
    // The zoom video (video.h): the camera flown from the whole canvas down to the view on
    // screen, a frame drawn off screen (export_stage 10) and read back and handed to the
    // encoder the frame after (11), until the last; then shared or saved.
    fude_zoom_video*   video;
    fude_zoom_flight   video_flight;     // (on its own clock: a frame's time, from 0)
    fude_zoom_camera   video_from, video_to;
    u32                video_frame, video_frames;   // the next to draw, how many in all
    u32                video_w, video_h;
    f64                video_scale;      // its pixels a screen point
} fude_zoom_page;

// The page's hooks for the extension (extension.h: .page_kind), and its tools
// for the toolbar (extension.h: .tools): Shapes, Picture and Smoothing, each
// with its choices, and Fill; its settings (extension.h: .settings_gather, .settings_apply).
extern const fude_page_kind      FUDE_ZOOM_PAGE_KIND;
// Settings' Measurements (extension.h: .sections): the units, and calibrating the true size.
extern const fude_extension_section FUDE_ZOOM_PAGE_MEASUREMENTS;
// The row over the lasso's selection (extension.h: .selection_row): the core's,
// and To text where handwriting can be read.
extern const fude_row_def FUDE_ZOOM_SELECTION_ROW;
// How that row shows now (extension.h: .selection_faces): boards held, To curve is
// Fit parts (one board: a list of parts typed for it) or Fit into the largest.
void fude_zoom_page_selection_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
extern const fude_extension_tool FUDE_ZOOM_TOOLS[8];   // Shapes, Insert, Smoothing, Fill, Arrange, Instruments, Layers, Export

// A picture's longer side, at most, as it comes in (pixels).
#define FUDE_ZOOM_PAGE_PICTURE_PX 4096u

void fude_zoom_page_settings_gather(const struct fude_app* _app, fude_settings* _settings);
void fude_zoom_page_settings_apply(struct fude_app* _app, const fude_settings* _settings);
// Its widgets over the page (extension.h: .ui_build, .ui_forget, .ui_update,
// .ui_restyle, .ui_hit): Back and the depth.
void fude_zoom_page_ui_build(struct fude_ui* _ui, rde_ui_node* _root);
void fude_zoom_page_ui_forget(struct fude_ui* _ui);
void fude_zoom_page_ui_update(struct fude_ui* _ui, b8 _full);
void fude_zoom_page_ui_restyle(struct fude_ui* _ui);
b8   fude_zoom_page_ui_hit(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

void fude_zoom_page_init(fude_zoom_page* _page, struct fude_app* _app);
// The screen's half-size (points): where the page's screen ends each way from its middle.
fude_zoom_v2 fude_zoom_page_half(const fude_zoom_page* _page);
void fude_zoom_page_destroy(fude_zoom_page* _page);
// A pen, touch or mouse event while no screen is on top.
void fude_zoom_page_event(fude_zoom_page* _page, rde_event* _event);
// Once a frame while no screen is on top.
void fude_zoom_page_update(fude_zoom_page* _page);
// The page: its paper, the canvas, the stroke being drawn. Inside a 2D drawing block.
void fude_zoom_page_render(fude_zoom_page* _page, rde_window* _window);
// The OS short of memory: the decoded strokes let go.
void fude_zoom_page_trim(fude_zoom_page* _page);
// A developer's looks: the pen as if it went down (0), moved (1), lifted (2) at
// _screen — what the pen's events do.
void fude_zoom_page_look_pen(fude_zoom_page* _page, u8 _phase, rde_vec_2F _screen);
// ...and the getting around: flown home as Reset view does (0), the depth's
// levels opened (1), Back (2), the view marked (3), Places opened (4).
void fude_zoom_page_look_nav(fude_zoom_page* _page, u8 _what);
// ...and the layers: their panel opened (0), a new layer (1), a row's eye (2) or lock (3) pressed.
void fude_zoom_page_look_layers(fude_zoom_page* _page, u8 _what, u32 _row);
void fude_zoom_page_look_rename(fude_zoom_page* _page, const c8* _name);
void fude_zoom_page_look_drive(fude_zoom_page* _page, u8 _step);
void fude_zoom_page_look_board(fude_zoom_page* _page, u8 _step);
// Two lines in a corner in the middle of the view, lassoed; and (_mm > 0) that corner filleted _mm round.
void fude_zoom_page_look_fillet(fude_zoom_page* _page, f64 _mm);
// The diagram library's panel open on family _family; a symbol (symbol.h' id) put _dx, _dy (screen points)
// from the view's middle with _text ("|": a line break); a Kanban board of preset _preset.
void fude_zoom_page_look_library(fude_zoom_page* _page, u8 _family);
void fude_zoom_page_look_symbol(fude_zoom_page* _page, const c8* _id, const c8* _text, f64 _dx, f64 _dy);
void fude_zoom_page_look_kanban(fude_zoom_page* _page, u32 _preset);
// Insert's Kanban board's card up, template _preset in its rows (_extra: a row more), then (_insert) Insert.
void fude_zoom_page_look_kanban_card(fude_zoom_page* _page, u32 _preset, b8 _extra, b8 _insert);
// The map's panel up; an area named _name made (round what the lasso holds, else the view); presenting, at step
// _step; a laser's trail across the screen (presenting).
void fude_zoom_page_look_map(fude_zoom_page* _page);
void fude_zoom_page_look_area(fude_zoom_page* _page, const c8* _name);
// A sheet's card up ("PAPER:WAY:SCALE[:W:H][!]": a paper's index or -1, 1 landscape, a scale's index, sizes typed, ! Insert
// pressed), or ("edit") the last sheet's, lassoed.
void fude_zoom_page_look_sheet(fude_zoom_page* _page, const c8* _spec);
// The size card up for what the lasso holds, fields typed ("W;H;ANGLE;X;Y": empty ones left; ~ its proportions let go;
// ! Apply pressed).
void fude_zoom_page_look_size(fude_zoom_page* _page, const c8* _spec);
// The lasso's Repeat card up ("MODE;COPIES;ACROSS;UP;COPIES;ANGLE;CX;CY;LINE;AXIS;KEEP", empty ones left; ! Apply pressed).
void fude_zoom_page_look_repeat(fude_zoom_page* _page, const c8* _spec);
void fude_zoom_page_look_present(fude_zoom_page* _page, u32 _step);
void fude_zoom_page_look_laser(fude_zoom_page* _page);
// What the lasso holds kept as piece _name; My pieces' panel up; piece _i put in the middle of the view.
void fude_zoom_page_look_keep(fude_zoom_page* _page, const c8* _name);
void fude_zoom_page_look_pieces(fude_zoom_page* _page);
void fude_zoom_page_look_piece_put(fude_zoom_page* _page, u32 _i);
// Piece _i laid as a stencil.
void fude_zoom_page_look_stencil(fude_zoom_page* _page, u32 _i);
// Every sticky note lassoed and let go where it is (those in a Kanban column lined up in it).
void fude_zoom_page_look_tidy(fude_zoom_page* _page);
// The lassoed board's parts fitted from _text ("Side 600 x 300 x2; Top 800 x 300"; "": Fit parts pressed).
void fude_zoom_page_look_fit(fude_zoom_page* _page, const c8* _text);
// The offset pad's grip dragged _dx, _dy (UI units); instrument _kind's size tab dragged _by along it (_hold: not let go).
// Every text selected, then Handwriting pressed.
void fude_zoom_page_look_handwrite(fude_zoom_page* _page);
void fude_zoom_page_look_grip(fude_zoom_page* _page, f32 _dx, f32 _dy);
void fude_zoom_page_look_grow(fude_zoom_page* _page, u32 _kind, f64 _by, b8 _hold);
// Instrument _kind snapped where it lies (as if let go there), then moved _across and _up millimetres by the offset pad.
void fude_zoom_page_look_anchor(fude_zoom_page* _page, u32 _kind, f64 _across, f64 _up);
// A finger on the instruments as the page takes one (0 down, 1 moved — snapped or slid as a
// hand's — 2 up), at that point (the instruments' units).
void fude_zoom_page_look_finger(fude_zoom_page* _page, u32 _phase, fude_zoom_v2 _at);
// The Lasso taken and a loop drawn round the screen box _a-_b (as the pen would); a PDF's
// text found in it written to the log.
void fude_zoom_page_look_loop(fude_zoom_page* _page, rde_vec_2F _a, rde_vec_2F _b);
// The selection's Offset pressed (a board lassoed alone: its grain turned).
void fude_zoom_page_look_grain(fude_zoom_page* _page);
// A joint put down (1 a finger joint, 2 a dovetail): length, finger width or tails, depth (mm).
void fude_zoom_page_look_joint(fude_zoom_page* _page, u8 _kind, f64 _a, f64 _b, f64 _c);
void fude_zoom_page_look_nodes(fude_zoom_page* _page, u8 _step);
void fude_zoom_page_look_to_curve(fude_zoom_page* _page);
void fude_zoom_page_look_holes(fude_zoom_page* _page, f64 _mm);
// ...and what the lasso holds offset by _mm (out; in when negative), as Offset's numpad does.
void fude_zoom_page_look_offset(fude_zoom_page* _page, f64 _mm);
// ...and Find for _words, as its card does.
void fude_zoom_page_look_find(fude_zoom_page* _page, const c8* _words);
// ...and a diagram made from Mermaid text, as the Insert tool's card does.
void fude_zoom_page_look_mermaid(fude_zoom_page* _page, const c8* _text);
// ...and the true size's calibration begun, as Settings' Calibrate does.
void fude_zoom_page_look_calibrate(fude_zoom_page* _page);
// ...and a text (or a sticky note: _sticky) put down at _screen saying _words, as the card's Save does.
void fude_zoom_page_look_text(fude_zoom_page* _page, rde_vec_2F _screen, b8 _sticky, const c8* _words);
// ...and the numpad opened with _text in it, on the last line drawn (as its length chip opens it).
void fude_zoom_page_look_numpad(fude_zoom_page* _page, const c8* _text);
// ...and Export into _path: PNG (0), SVG (1), PDF (2) or DXF (3).
void fude_zoom_page_look_export(fude_zoom_page* _page, u8 _format, const c8* _path);
// A PNG export's view drawn off screen, when one is under way: in the frame's
// render, before the page's own drawing (the shell's).
void fude_zoom_page_render_offscreen(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);

#endif
