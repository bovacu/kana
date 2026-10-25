// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGE
#define FUDE_ZOOM_PAGE

#include "rde.h"
#include "drawing/app/extension.h"
#include "drawing/ink/ink.h"
#include "zoom/scene.h"
#include "zoom/render.h"
#include "zoom/rich.h"
#include "zoom/formula.h"
#include "zoom/handmath.h"
#include "zoom/erase.h"
#include "zoom/zfile.h"
#include "zoom/select.h"
#include "zoom/nav.h"
#include "zoom/instrument.h"
#include "zoom/partsform.h"
#include "zoom/kanbanform.h"
#include "zoom/sheetform.h"
#include "zoom/sizeform.h"
#include "zoom/pointform.h"
#include "zoom/choiceform.h"
#include "zoom/circuit.h"
#include "zoom/mech.h"
#include "zoom/mechrun.h"
#include "zoom/coupling.h"
#include "zoom/plot.h"
#include "zoom/valueform.h"
#include "zoom/limitsform.h"
#include "zoom/bodyform.h"
#include "zoom/plan.h"
#include "zoom/repeatform.h"
#include "zoom/map.h"
#include "zoom/piece.h"
#include "zoom/custom.h"
#include "zoom/snap.h"
#include "zoom/numpad.h"
#include "zoom/pdfview.h"
#include "zoom/video.h"
#include "zoom/handfind.h"
#include "zoom/symbol.h"
#include "zoom/display.h"
#include "zoom/product.h"
#include "zoom/pages.h"
#include "zoom/audio.h"
#include "zoom/voice.h"
#include "zoom/transcript.h"

// ===========================================================================
// Page: the deep-zoom page, which the canvas apps use in place of the ink page (extension.h's page kind). The hands
// are the same as everywhere in fude:
//
//   PEN            draws (the toolbar's pen or marker); or erases (its Erase, or the pen's eraser end), in the
//                  eraser's mode (partial, whole strokes, trim), changed by pressing Erase again or E on a computer;
//                  or, with the Lasso, loops things to select them, then drags them, scales them by a corner and
//                  turns them by the knob (select.h)
//   PEN HELD       at a stroke's end, the stroke snaps to the shape it looks like (shape.h), which then follows the
//                  pen (bigger, smaller, turned) until it lifts; one undo brings the stroke back as drawn
//   PEN, SHAPES    with a shape chosen in the toolbar's Shapes (a line, a rectangle, an ellipse, a triangle; filled or
//                  not), the shape is dragged out from where the pen goes down to where it lifts
//   PICTURE        the toolbar's Picture: Photos or Files. What comes in is put in the middle of the screen, selected
//                  (with the Lasso) so it can be moved, scaled and turned at once
//   1 FINGER HELD  the page's menu (Paste, Select all), when fingers do not draw
//   1 FINGER       pans, or draws when the toolbar's hand is on
//   2 FINGERS      pinch-zoom (and pan), as far in or out as anyone likes
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
//
// On a computer, the left button draws, the right drags the page (a right click without dragging opens the page's
// menu), and the wheel zooms where the mouse is. Command or Control with Z is undo, with Shift+Z or Y redo; R is back
// to the start, B Back (to the view the last flight left), E the eraser's mode, P Present, M the map, Backspace or
// Delete deletes the selection, and Escape lets it go. No key clears the canvas, and none acts while a text field has
// the keyboard (page_frame.c).
//
// Where it sits: fude/zoom, the top of the canvas layer. It brings the scene (scene.h), the renderer (render.h), the
// tools and the cards together, and plugs into the app shell through extension.h: FUDE_ZOOM_PAGE_KIND, the toolbar's
// tools (FUDE_ZOOM_TOOLS), Settings' sections, the page's menus and its own widgets. The canvas apps' shell
// (zoom/shell.c, which each canvas app describes itself to) owns the page and calls it.
// Its main types and their lifecycle: the shell owns one fude_zoom_page, made with fude_zoom_page_init and freed with
// fude_zoom_page_destroy. Only one page exists at a time: the extension's hooks, which are handed no page, reach it
// through a pointer page.c keeps, set by fude_zoom_page_init.
// Threads: main thread only. The slow work runs elsewhere and is polled once a frame: handwriting recognition (ML
// Kit), speech to text (listen.h's thread) and a PDF's pages (pdfview.h's worker).
// Files: the canvas's file (zfile.h) is <save dir>/notes/<id>.zoom. Beside it the page keeps <id>.tools (the
// instruments as they lie; its format is described in page_file.c), <id>.pages (pages.h), <id>.words (words.h),
// <id>.cover.png (the home screen's cover) and the recordings, <id>.audio and <id>.rec<N> (audio.h).
// Design:
//   - A stroke is captured by an ink of its own (ink.h, the same pen: its pressure, its pens without pressure,
//     its sampling) in the camera frame's units round where it began, and becomes the canvas's (scene.h) when the pen
//     lifts. Brush colour, size and width come from the app's ink, which the toolbar and Settings set as everywhere.
//   - The canvas's journal is appended about every second while there is anything new, and everything is
//     checkpointed when the canvas is left or the app goes, so a crash loses at most about a second of drawing.
// ===========================================================================

struct fude_app;

#define FUDE_ZOOM_PAGE_FLUSH         1.0     // seconds between journal appends while drawing
// The most fingers followed at once.
#define FUDE_ZOOM_PAGE_FINGERS       5u
#define FUDE_ZOOM_PAGE_LONG_PRESS    0.5     // seconds a still finger is held for the page's menu
#define FUDE_ZOOM_PAGE_HOLD          0.55    // seconds the pen rests at a stroke's end before it snaps to a shape
#define FUDE_ZOOM_PAGE_HOLD_SLOP     3.0f    // ...while moving less than this (screen units)

// The paper's spacing on screen, at least (and under twice that), in points: the dots', and the lines' and squares'
// for each of Settings' Lines & squares sizes.
#define FUDE_ZOOM_PAGE_DOTS          40.0
#define FUDE_ZOOM_PAGE_PAPER_SMALL   28.0
#define FUDE_ZOOM_PAGE_PAPER_MEDIUM  44.0
#define FUDE_ZOOM_PAGE_PAPER_LARGE   72.0

// The canvas's real size: a unit of its home frame (a screen point at ×1) is this many millimetres. Rulers, measures
// and dimensions read in it, and deeper frames in their share of it.
#define FUDE_ZOOM_PAGE_MM_PER_UNIT   1.0
#define FUDE_ZOOM_PAGE_LINE_CHIP     6.0     // seconds a new line's length chip stays

// The instruments as they lie: enough to put them back (fude_zoom_tools_step), and, to tell a move from a re-pin, each
// one's middle on the drawing (in its stuck_frame's units, as it was when the state was taken), which no re-pin moves.
typedef struct {
    fude_zoom_instrument tools[FUDE_ZOOM_INSTRUMENT_MOST];
    u8                   order[FUDE_ZOOM_INSTRUMENT_MOST];
    fude_zoom_v2         middle[FUDE_ZOOM_INSTRUMENT_MOST];
} fude_zoom_tools_state;

// One undo step of the instruments: how they lay before and after it.
typedef struct {
    fude_zoom_tools_state before, after;
    u64                   at;   // how many of the drawing's steps were done before it (counted from its first ever)
} fude_zoom_tools_step;
#define FUDE_ZOOM_TOOLS_STEPS 64u   // the most instrument steps kept (the oldest go)

// A finger on the page.
typedef struct {
    b8         active;
    u64        id;
    rde_vec_2F at;       // where it is now, on screen
    // Where it went down.
    rde_vec_2F start;
} fude_zoom_finger;

#define FUDE_ZOOM_PAGE_BACK   16u    // how many views Back remembers
#define FUDE_ZOOM_PAGE_CRUMBS 8u     // the depth's levels shown (the top and the nearest above)
#define FUDE_ZOOM_PAGE_PLACES 8u     // bookmarks listed (the newest)
#define FUDE_ZOOM_PAGE_LASER  192u   // how many of the laser pointer's last points its trail keeps
// The offset pad's buttons: ◀ X ▶  ▼ Y ▲  ×.
#define FUDE_ZOOM_PAGE_ANCHOR_BUTTONS 7u
#define FUDE_ZOOM_PAGE_STRIP_PICS 24u       // how many pictures of a notebook's pages its strip keeps at once
#define FUDE_ZOOM_PAGE_INLINE_BUTTONS 12u   // writing on the page: its bar's buttons (marks, cut, copy, paste, done)
#define FUDE_ZOOM_PAGE_PREVIEWS 8u         // how many of the arrows' pictures are kept at once
#define FUDE_ZOOM_PAGE_LAYER_ROWS 8u // how many layers are listed
#define FUDE_ZOOM_PAGE_CURVE_NODES FUDE_ZOOM_PATH_NODES   // the most points a curve has (a path's)


// A wire kept by parts being dragged: hidden while they are, and drawn routed to where their pins are going
// (page_circuit.c).
typedef struct {
    u32          wire;
    u32          part[2];     // the parts at its ends (FUDE_ZOOM_NONE: an end on nothing, or on a wire)
    i32          pin[2];
    b8           moving[2];   // whether that end's part is dragged with the lasso
    fude_zoom_v2 at[2];       // where that end was on the screen as the drag began
} fude_zoom_page_drag_wire;

// A recording's transcript (0.1.95, listen.h): what it said, as far as it has been read.
typedef struct {
    u32                  number;   // the recording's number
    fude_zoom_transcript t;
    b8                   wanted;   // to be read to its end (Transcribe pressed, or a recording made with Live text)
} fude_zoom_page_heard;

// A canvas holding the words Find looked for everywhere (everywhere_found).
typedef struct {
    u32 note;            // its id in notes.h
    u32 count;           // how many matches it has
    c8  name[64];
    c8  excerpt[200];    // the line its first match is in
} fude_zoom_page_found;

// A point of the laser pointer's trail while presenting (the page's laser ring).
typedef struct {
    rde_vec_2F at;     // on the screen
    f64        time;   // when it came (the engine's clock)
    b8         lift;   // a new trail starts at it (the pen went down there)
} fude_zoom_page_laser_point;

// The page: the open canvas and everything the page keeps between frames. Its fields are grouped by what they serve;
// most belong to one page_*.c module alone (page_parts.h says which module holds what).
typedef struct fude_zoom_page {
    struct fude_app*   app;
    fude_zoom_scene    scene;
    fude_zoom_renderer renderer;
    fude_zoom_formulas formulas;     // the maths font and the formulas set with it (the renderer draws them)
    fude_zoom_handmath_chars hand_chars;   // its glyphs' characters, for a formula written out by hand (handmath.h)
    fude_zoom_eraser   eraser;
    fude_zoom_file     file;
    fude_ink           capture;      // the stroke being drawn
    fude_zoom_selection selection;   // the lasso's selection
    u8                 tool_seen;    // the toolbar's tool last frame (choosing another one lets the selection go)
    b8                 long_pressed; // this finger gesture has already opened the menu
    u32                canvas;       // the canvas open (its notes.h id; 0: none yet)
    b8                 open;
    u32                device;
    u8                 erase_mode;   // FUDE_ZOOM_ERASE_
    // The toolbar's counts of taps on Erase and on its tools as last seen, so a new tap is noticed.
    u32                erase_taps_seen;
    u32                tool_taps_seen;
    // The Shapes tool: the shape chosen (0 for none, when the bar's own tool is in hand), whether it is filled, and the
    // one being dragged out (on screen).
    u8                 shape_tool;   // FUDE_ZOOM_SHAPE_ (a triangle is a POLYGON)
    b8                 shape_filled;
    u8                 line_style;     // the Shapes' line: shape.h's FUDE_ZOOM_LINE_ (solid, dashed, a centre line)
    u8                 hatch;          // ...a closed shape's hatching (shape.h's FUDE_ZOOM_HATCH_)
    f64                pen_mm;         // ...its pen's width on paper in mm (× N on a 1:N sheet; 0: the brush's width)
    u8                 topic;          // what the canvas is for, setting the bar's tools (FUDE_ZOOM_TOPIC_; in .tools)
    f64                stl_mm;         // 3D export: how tall non-board parts stand (0: not asked yet, so 3 mm)
    b8                 fill_tool;    // the Fill tool is in hand: a tap fills what is closed under it (fill.h)
    b8                 shaping;
    rde_vec_2F         shape_from;
    // A guide being pulled out of a sheet's ruler stuck to the screen's edge, with the pen at pen_now: from which
    // sheet, and from its top ruler (a guide along its X) or else its left one (along its Y).
    b8                 guide_pulling;   // a guide is being pulled out
    u32                guide_sheet;     // ...from that sheet
    u8                 guide_top;       // ...from its top ruler, else its left one

    // The pen (or the mouse, or a writing finger) is down.
    b8                 drawing;
    b8                 erasing;
    b8                 pen_on_ui;
    u32                draw_frame;
    fude_zoom_v2       origin;       // where the stroke began, in that frame's units
    f64                draw_z;
    rde_vec_2F         erase_last;   // on screen
    u64                writer;       // the finger drawing (when the hand is on)
    u64                play_finger;  // the finger working a part while it plays (its id + 1; 0: none)
    b8                 finger_drawing;
    f64                stroke_began;
    // Hold to snap: when the pen last moved, and the shape it snapped to.
    f64                moved_at;
    rde_vec_2F         moved_from;   // on screen
    b8                 snapped;
    fude_zoom_shape_fit fit;         // in screen units, as recognized
    rde_vec_2F         snap_pen;     // where the pen was when it snapped
    rde_vec_2F         pen_now;      // ...and where it is now
    // The brush's circle (its size, where it would draw), shown while the pen (or the mouse) hovers, for its eraser end
    // or not.
    rde_vec_2F         hover_at;     // on screen, eased (a pen leaving the glass reads unsteadily)
    b8                 hover_on;
    b8                 hover_eraser;
    f64                hover_quiet;  // not shown before this time (the pen has just lifted)
    rde_arr TYPE(fude_zoom_v2) snap_scratch;
    // Smoothing (smooth.h): the level for new strokes (FUDE_ZOOM_SMOOTH_, a setting); the stroke's points as the pen
    // gave them (from origin, in the frame's units; the capture holds them smoothed); its first point not yet settled;
    // and the rope's tip.
    u8                 smooth_level;
    u8                 stroke_smooth;   // the stroke's own level (none along an instrument)
    rde_arr TYPE(fude_zoom_v2) raw;
    rde_arr TYPE(fude_zoom_v2) smoothed;
    u32                smooth_from;
    fude_zoom_v2       rope_tip;

    // The instruments (instrument.h): the ruler, the set squares and the protractor; whether the pen is holding one, or
    // drawing along one's edge.
    fude_zoom_instruments instruments;
    fude_zoom_pdfview  pdf;             // a canvas over a PDF: its pages under the ink (pdfview.h; none: a free canvas)
    fude_zoom_pages    pages;           // a notebook: its pages one under the other (pages.h; off: the endless canvas)
    u32                pages_rev;       // the scene's revision the page count was last brought up to date for
    b8                 instrument_pen;
    b8                 ruled;
    // Measuring (the Instruments' Tape measure and Dimension): which one (0 none, 1 the tape, 2 a dimension), its two
    // ends in the frame it measures in, the tape left showing after the pen lifts, and what the pen snapped to (snap.h;
    // the Shapes tool's ends snap too). Then how lengths are written.
    u8                 measure_tool;
    b8                 measuring;
    b8                 tape_shown;
    u32                measure_frame;
    fude_zoom_v2       measure_a, measure_b;
    fude_zoom_snap     snap_hit;
    fude_zoom_v2       snap_from;      // where a line being drawn or measured starts (screen; snap.h's PERP, PARALLEL)
    fude_zoom_snap     measure_from_hit;   // what a measure began on (a dimension drawn from it follows it: connect.h)
    fude_zoom_v2       measure_raw;        // where the pen came down to measure, before it snapped (on screen)
    b8                 snap_from_on;
    fude_zoom_units_style units;
    b8                 ruled_straight;  // the pen is along a straight edge (its line becomes a LINE shape)
    b8                 ruled_round;     // ...or a round one (its line becomes an ARC, or a whole circle)
    // A line just drawn shows its length on a chip by its end for a moment; a tap on the chip opens the numpad, to give
    // the length exactly (numpad.h).
    fude_zoom_numpad   numpad;
    // Each arrow's preview (the marks on an empty screen): what it points at, drawn once into a small picture of its
    // own (in fude_zoom_page_render_offscreen, one a frame) and then shown from it, rather than drawn again each frame.
    // It is kept while the same thing is pointed at and the canvas is as it was.
    rde_render_texture* previews[FUDE_ZOOM_PAGE_PREVIEWS];
    u32                 preview_frame[FUDE_ZOOM_PAGE_PREVIEWS];
    fude_zoom_box       preview_box[FUDE_ZOOM_PAGE_PREVIEWS];
    u32                 preview_rev[FUDE_ZOOM_PAGE_PREVIEWS];
    b8                  preview_ready[FUDE_ZOOM_PAGE_PREVIEWS];
    i8                  mark_preview[FUDE_ZOOM_NAV_MARKS];   // each mark's preview (-1: none)
    // When the canvas is left with the camera somewhere other than camera_kept, one more checkpoint keeps where it is.
    fude_zoom_camera   camera_kept;     // the camera as the file has it (its last checkpoint's)
    u64                tools_kept;      // the instruments file as last written (a hash of its bytes; 0: not yet)
    // The instruments' undo, beside the drawing's (in memory only): a step each time they were left changed (taken out,
    // put away, moved, turned, made bigger, nudged), holding what they were and became, and where it falls among the
    // drawing's steps (fude_zoom_tools_step).
    rde_arr            tools_steps;
    u32                tools_done;      // the steps done (the rest were undone, and can be redone)
    fude_zoom_tools_state tools_then;   // the instruments as the last step left them
    b8                 tools_then_set;
    b8                 tools_absorb;    // drawing along one: what that changes (a compass turned) is not a step
    u64                tools_seen;      // the drawing's pushes when last looked at; a push since stops their redo
    fude_zoom_parts_form parts_form;    // Fit parts' card: a board's parts, their sizes typed
    fude_zoom_kanban_form kanban_form;  // Insert's Kanban board: its columns typed
    fude_zoom_sheet_form  sheet_form;   // Insert's Measured sheet (or a sheet's size tapped): its paper, scale and size
    u32                   sheet_editing;       // the sheet being changed (FUDE_ZOOM_NONE: a new one)
    f64                   sheet_last[4];       // the last one made: width, height (mm), scale, bits (width 0: none yet)
    fude_zoom_size_form   size_form;    // makes what the lasso holds an exact size (its size chip tapped)
    fude_zoom_point_form  point_form;   // a point typed for the Curve tool (its Type a point chip tapped)
    fude_zoom_choice_form choice_form;  // a choice asked (Combine's: join, cut out, overlap)
    u64                   combine_key;  // what the lasso held (fude_zoom_page_held_key) when combine_offered was worked out
    b8                    combine_offered; // ...and whether Combine was offered for it (page_edit.c)
    fude_zoom_value_form  value_form;   // a part's value asked (Value: a resistor's, a source's, a motor's...)
    fude_zoom_body_form   body_form;    // a drawing made a body (Make body: its material, moving or fixed)
    fude_zoom_limits_form limits_form;  // a part's limits asked (Limits: what it takes at most, limits.h)
    u32                   limits_object; // the part the limits card is for
    u32                   value_object; // the part the value card is for
    u8                    point_mode;   // how the last point was typed (FUDE_ZOOM_POINT_)
    fude_zoom_repeat_form repeat_form;  // the lasso's Repeat: copies in a row, round a point, mirrored
    b8                    size_chip;    // the chip shows the lasso's size (not a line's, a board's or a sheet's)
    u64                   size_key;     // what the lasso held when size_said was worked out
    c8                    size_said[2u * FUDE_ZOOM_UNITS_TEXT + 8u];
    // The line chip itself: its button, the object it is for, until when it shows, and what it says.
    rde_ui_button*     line_chip;
    u32                line_chip_object;
    f64                line_chip_until;
    b8                 shown_line_chip;
    rde_vec_2F         line_chip_center;
    c8                 line_chip_said[3u * FUDE_ZOOM_UNITS_TEXT + 96u];   // (room for a board's sizes and material)
    // Text (the Insert tool's Text and Sticky note): which one (0 none, 1 a text, 2 a note); its card (a field for the
    // keyboard, Cancel, Save); and what it is for: a new one where the pen went down (on screen), or the one tapped.
    u8                 text_tool;
    fude_kit_modal     text_card;
    rde_vec_2F         text_card_at;    // where the card was put (kit units) and how big: the Formula preview's room
    rde_vec_2F         text_card_size;
    rde_ui_text_editor* text_field;
    rde_ui_button*     text_save;
    rde_ui_button*     text_cancel;
    c8                 context_link[1024];   // the note's link the page's menu opened on (Open link; empty: none)
    // Writing on the page (0.1.90): a text or a sticky note written where it is rather than on a card. A hidden field
    // takes the keyboard (and its words), the words are drawn on the page as they are written (rich.h's lay_raw), with
    // a caret, the selection and a bar of marks over them.
    b8                  inline_on;
    rde_ui_text_editor* inline_field;
    rde_ui_text_editor* card_field;        // the card's own field (text_field is one or the other)
    rde_ui_image*       inline_bar;
    rde_ui_button*      inline_buttons[FUDE_ZOOM_PAGE_INLINE_BUTTONS];
    b8                  inline_bar_shown;
    // Where the bar was put (UI canvas, origin bottom left), so a touch on it is not taken as the page's.
    rde_vec_2F          inline_bar_at, inline_bar_size;   // where the bar was put
    fude_zoom_rich      inline_rich;       // its words as written, laid out (in its own units)
    b8                  inline_rich_made;
    u32                 inline_frame;
    fude_zoom_sim       inline_sim;        // from its own units (top left at 0, 0, Y up) to its frame's
    f64                 inline_size;       // its letters' size (in its own units)
    f64                 inline_w, inline_h;   // a note's size (own units; 0, 0 for a text, as long as its lines)
    rde_color           inline_ink, inline_paper;
    fude_zoom_box       inline_box;        // where it is on the screen (as of the last frame)
    rde_vec_2F          inline_origin;     // its words' top left on screen; one of its units is inline_k points
    f64                 inline_k;
    f64                 inline_started;
    b8                  inline_selecting;  // a hand on its words: selecting from inline_anchor to under it
    u32                 inline_anchor;
    // The pen down on a note's link: let go where it went down, the link opens; moved, it draws. When it is not on a
    // link but on ink a recording is linked to (its panel up), a tap plays from there.
    b8                 link_pending;      // the pen is down on a note's link
    b8                 link_audio;      // ...or on ink linked to a recording
    b8                 link_from_pen;
    rde_vec_2F         link_down;
    // The card's buttons that put a note's marks in, one tap each (rich.h: bold, italic, a heading, lists, a table, a
    // link).
    rde_ui_button*     text_formats[FUDE_ZOOM_RICH_DOS];   // a note's marks
    u32                text_object;    // the text being changed (FUDE_ZOOM_NONE: a new one)
    u32                text_frame;
    fude_zoom_v2       text_at;        // a new one's top left, in its frame's units
    u8                 text_style;
    rde_vec_2F         text_laid_out;
    // Measurements (Settings' section): the units lengths are written in (0 mm, 1 cm, 2 inches, 3 feet and inches), and
    // the screen's true size, the millimetres a point really is (0: not calibrated); then the calibration under way: a
    // bank card's outline at the size being tried, and its buttons.
    u8                 units_choice;
    f32                true_mm_per_point;
    f32                print_x, print_y;      // the printer's factors (a setting; 0: never checked)
    f64                print_measured_x;      // the check's first line, kept while the second is typed
    // A board being made or changed (Insert's Board, or a lassoed board's chip): its sizes as typed, one at a time,
    // then its name.
    u32                board_editing;         // the board being changed (FUDE_ZOOM_NONE: a new one)
    f64                board_mm[3];           // length, width, thickness
    u8                 board_grain;
    u8                 board_material;   // FUDE_ZOOM_MATERIAL_ (a new board gets the last one's)
    f64                fillet_mm;        // the last fillet's radius (0: none yet)
    f64                chamfer_mm;       // the last chamfer's size (-1: none yet)
    b8                 trim_tool;        // the Instruments' Trim: a tap takes a line's piece between crossings
    const c8*          material_words[FUDE_ZOOM_MATERIAL_COUNT];
    rde_ui_button*     material_chips[FUDE_ZOOM_MATERIAL_COUNT];   // on the text card, when naming a board
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
    // Layers (scene.h; the toolbar's Layers): the panel, with a row for each layer (its name and how many things it
    // holds; a tap draws on it), its eye and its lock, then New layer; and the layers as the panel lists them.
    b8                 layers_open, shown_layers, layers_dirty;   // (dirty: the rows are laid out again)
    rde_vec_2F         layers_bar_at, layers_bar_size;   // the toolbar as it was when the panel last stepped round it
    rde_ui_button*     layer_rows[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_eyes[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_locks[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_ups[FUDE_ZOOM_PAGE_LAYER_ROWS];     // each moves its layer over the one above it
    rde_ui_button*     layer_downs[FUDE_ZOOM_PAGE_LAYER_ROWS];
    rde_ui_button*     layer_drops[FUDE_ZOOM_PAGE_LAYER_ROWS];   // each one's delete
    f32                layers_left, layers_top;   // where the panel was put (its top left)
    rde_ui_button*     layer_add;
    u32                layer_count;
    u16                layer_index[FUDE_ZOOM_PAGE_LAYER_ROWS];
    u8                 layer_flags[FUDE_ZOOM_PAGE_LAYER_ROWS];
    u32                layer_object[FUDE_ZOOM_PAGE_LAYER_ROWS];   // each row's LAYER object (FUDE_ZOOM_NONE: none yet)
    c8                 layer_names[FUDE_ZOOM_PAGE_LAYER_ROWS][40];
    u32                layers_seen;     // the scene's revision the layers were last worked out at
    b8                 layer_moving;    // the lasso's To layer: the next row tapped (or New layer) takes the selection
    f64                text_opened_at;  // when the text card came up
    u32                layer_renaming;  // the layer row the text card renames
    u32                layers_mask_seen;   // the layers' hidden and locked bits as last worked out
    // The Curve tool (Shapes): the points tapped so far (in the frame's units), with a smooth curve through them
    // (Catmull-Rom). Tapping the last point again finishes it; tapping the first closes it.
    b8                 curve_tool;
    fude_zoom_v2       curve_nodes[FUDE_ZOOM_PAGE_CURVE_NODES];
    u32                curve_count;
    u32                curve_frame;
    u64                curve_corners;   // one bit per point: a corner, where the line turns (typed)
    // A lassoed path's or polygon's nodes (shape.h): the one held and where it is (on screen), the first tap of a
    // double tap, and a tap on its line waiting to become a new node (if the pen lifts where it went down).
    i32                node_grab;
    u32                node_object;
    rde_vec_2F         node_at, node_down_at, node_last_at;
    f64                node_tap_time;
    // The node last tapped (pressed and let go without moving), its path, and when.
    i32                node_tap_index;  // the node last tapped
    u32                node_tap_object;
    f64                node_down_time;  // when the node held was pressed
    b8                 node_insert;
    // The path's node last touched shows its handle (shape.h: a Bezier handle, dragged out either way), which is held
    // by one of its two ends: node_grab's, 1 for the way out, -1 for the way in.
    i32                node_focus;
    u32                node_focus_object;
    i8                 node_handle;
    rde_vec_2F         lasso_down_at;    // the Lasso's press: a tap (that went nowhere) selects the thing under it
    b8                 lasso_went;
    b8                 lasso_on_selection;
    b8                 sculpt;          // the Smoothing tool's Sculpt: a stroke begun on a line redoes that stretch
    b8                 saw;             // the Instruments' Saw: what is drawn cuts the boards it crosses (cut.h)
    u32                compass_typing;   // the compass whose radius is on the numpad (its slot; the serial says which)
    u32                compass_serial;
    u8                 compass_step;      // an ellipse template's: 0 its width on the numpad, 1 its height
    f32                kerf_mm;         // how wide the saw cuts, in millimetres (a setting; 0: never set)
    b8                 keep_grain;      // fitting parts into a board keeps their grain along the board's (a setting)
    b8                 smooth_chosen;   // the smoothing was chosen by hand (a setting; else the default, off)
    b8                 rich_fonts_tried;   // a note's bold and italic fonts loaded, or tried (for the renderer)
    rde_arr            paper_lines;        // the canvas apps' own papers' lines this frame (paper.h)
    // A notebook's pages strip (0.1.86): its chip ("3 / 12": the page the view is on, of how many), which opens it, and
    // the strip itself: a picture of each page, the one you are on lit, a tap flying to it, dragged along.
    rde_ui_button*      pages_button;
    b8                  shown_pages;
    b8                  laid_pages;        // (the row at the bottom left is laid out with the chip in it)
    c8                  pages_said[24];
    rde_vec_2F          pages_center;
    f64                 pages_pressed_at;
    b8                  strip_open;
    f64                 strip_scroll;      // how far along it, in points
    u64                 strip_hand;
    rde_vec_2F          strip_down;
    f64                 strip_scroll_down;
    b8                  strip_dragged;
    // Find through every canvas (0.1.92, words.h): a few canvases are looked through each frame, and those holding the
    // words are listed in a panel where the strip goes. A tap opens one and carries Find on in it.
    b8                  everywhere_on;          // still looking
    b8                  everywhere_shown;       // its panel is up
    c8                  everywhere_query[128];
    u32                 everywhere_next;        // the next of the notes to look through
    u32                 everywhere_done, everywhere_total;   // canvases looked through, of how many
    rde_arr             everywhere_found;       // fude_zoom_page_found, the most matches first
    f64                 everywhere_scroll;
    u64                 everywhere_hand;
    rde_vec_2F          everywhere_down;
    f64                 everywhere_scroll_down;
    b8                  everywhere_dragged;
    c8                  find_after[128];        // a canvas opened from it: its words are found once it is up...
    u32                 find_after_frames;      // ...this many frames on
    // Its cover (0.1.92): a picture of the canvas kept beside it (<id>.cover.png) for the home screen, a notebook's
    // first page or else the view. It is drawn again a few seconds after the canvas changed (or when asked), and read
    // back the frame after.
    rde_render_texture* cover_target;
    u32                 cover_canvas;     // the canvas the next three fields are about
    u32                 cover_rev;        // the drawing's revision last seen
    f64                 cover_changed_at;
    b8                  cover_due;        // changed since its cover was drawn
    b8                  cover_now;        // wanted at once (the home screen opening, or no cover yet)
    u8                  cover_stage;      // 1: drawn this frame, to be read back the next
    u32                 cover_stamp;      // goes up as each cover is written...
    u32                 cover_note;       // ...and this is the canvas it was written for
    // The home button (beside the side panel's), where the app has a home screen (on_home).
    rde_ui_button*      home_button;
    b8                  shown_home;
    rde_vec_2F          home_center;
    void              (*on_home)(struct fude_app* _app);
    // The next new canvas opened is made a notebook of this size (pages.h's FUDE_ZOOM_PAGES_ + 1; 0: none).
    u8                  notebook_next;    // the next new canvas's notebook size
    // Audio synced to ink (0.1.93: audio.h, voice.h): the open canvas's recordings and what each thing was written
    // during; one being made, and one played (the ink faint past where it is, with Replay ink); the chips and the
    // panel.
    fude_zoom_audio          audio;
    fude_zoom_voice_recorder recorder;
    u32                      audio_recording;     // the one being made (its number; 0: none)
    f64                      audio_started_at;    // when it began (the engine's clock)
    f64                      audio_saved_at;
    u64                      audio_pushes_seen;   // the history's steps linked to recordings so far
    f64                      audio_down_linked;   // the pen-down (stroke_began) the last step was linked to
    fude_zoom_voice_player   player;
    u32                      audio_playing;       // the one in the player (its number; 0: none)
    b8                       audio_panel;
    b8                       audio_replay;
    rde_arr                  audio_faint;         // one u8 per object: the renderer's faint
    u32                      audio_faint_at;      // the position it was worked out for (ms; FUDE_ZOOM_NONE: again)
    u8                       audio_asked;         // microphone: 0 not asked now, 1 asked, 2 granted, 3 refused
    // Speech to text (0.1.95: listen.h): the recordings' transcripts (each loaded when first shown), the piece being
    // read, Live text (the recording being made, read as it goes), and the model's download.
    rde_arr                  heard;               // fude_zoom_page_heard
    u32                      heard_reading;       // the recording a piece of which is being read (0: none)
    u32                      heard_piece_end;     // ...up to this sample
    b8                       heard_live;
    b8                       heard_downloading;
    // A Transcribe (its recording) or Live text (FUDE_ZOOM_NONE) pressed once, which asked for the model's download.
    u32                      heard_ask;           // what asked for the download
    f64                      heard_checked_at;    // (the recording being made: its file's length looked at)
    u32                      heard_available;     // ...and the samples it held then
    f64                      heard_said_at;       // when the live caption's last words came
    u32                      audio_armed;         // a recording whose Delete was pressed once (0: none)
    u64                      audio_hand;          // the touch on the panel
    rde_vec_2F               audio_down;
    b8                       audio_scrubbing;     // its bar is being dragged
    f32                      audio_scrub;         // ...to there (0 to 1)
    f64                      audio_scroll;
    rde_ui_button*           mic_button;
    b8                       shown_mic;
    rde_vec_2F               mic_center;
    c8                       mic_said[24];
    f64                      mic_pressed_at;
    rde_ui_button*           listen_button;
    b8                       shown_listen;
    rde_vec_2F               listen_center;
    f64                      listen_pressed_at;
    b8                       laid_audio;          // (the row is laid out with the microphone's chip in it)
    b8                       laid_listen;         // (the row is laid out with the recordings' chip in it)
    b8                       laid_recording;      // (...with the microphone's chip wide, showing a recording's time)
    // The strip's hand: held this long without moving, the page under it is lifted, then dragged to its new place.
    f64                 strip_down_at;     // when the hand went down (the engine's clock)
    rde_vec_2F          strip_at;          // where the hand is now
    b8                  strip_lifted;      // a page is held, being dragged to its new place
    u32                 strip_pick;        // the page held (once let go, its buttons over it); FUDE_ZOOM_NONE: none
    u32                 strip_drop;        // where it would go, if let go now
    // The pages' pictures, one for each slot of strip_lru, which knows whose each is (its page's index, a u32) and
    // which was shown longest ago (the one drawn again into, past FUDE_ZOOM_PAGE_STRIP_PICS); each drawn at the scene's
    // revision strip_pic_rev, and ready once drawn.
    rde_render_texture* strip_pics[FUDE_ZOOM_PAGE_STRIP_PICS];
    u32                 strip_pic_rev[FUDE_ZOOM_PAGE_STRIP_PICS];
    b8                  strip_pic_ready[FUDE_ZOOM_PAGE_STRIP_PICS];
    rde_lru             strip_lru;
    rde_ui_label*      grain_label;     // Fit parts' grain: its row in Settings
    rde_ui_button*     grain_buttons[2];   // (0: may turn, 1: keep the grain)
    rde_ui_label*      kerf_label;
    rde_ui_button*     kerf_button;
    rde_ui_button*     kerf_chip;       // the saw's kerf, shown while the Saw is on (a tap types it)
    // Settings' Display: the frame rate drawn at the top right (a setting), counted over half a second at a time.
    b8                 show_fps;
    rde_ui_label*      display_header;
    rde_ui_label*      fps_label;
    rde_ui_button*     fps_buttons[2];  // Off, On
    u32                fps_frames;      // frames drawn since fps_since
    f64                fps_since;
    f64                fps;             // as last counted (0: not yet)
    // ...and its test canvas (inktest.h): the row, and the canvas its Make made, filled with the test ink once it is
    // the canvas open (fude_zoom_page_inktest_follow); 0 when none is waiting.
    rde_ui_label*      inktest_label;
    rde_ui_button*     inktest_button;
    u32                inktest_canvas;
    // ...and where the frames' time goes, written to the log every 2 s while the frame rate shows
    // (fude_zoom_page_perf_frame): the app's update and render, Play's step within the update, and the page's drawing
    // within the render, in seconds.
    f64                perf_since, perf_update, perf_render, perf_play, perf_draw;
    u32                perf_frames;
    u32                fit_stock;       // the board a list of parts is being typed for (Fit parts)
    // An instrument snapped by one of its points (instrument.h: its keys) and let go: that point in the drawing, and
    // how far across and up the offset pad (◀ X ▶ ▼ Y ▲ ×, at the bottom middle) has moved it from there.
    i32                anchor_tool;     // its slot (-1: none)
    u32                anchor_serial;   // ...and which instrument it was (instrument.h)
    u32                anchor_key;
    u32                anchor_frame;
    fude_zoom_v2       anchor_point;    // in anchor_frame's units
    f64                anchor_mm[2];    // across (+ right), up (+ up)
    u8                 anchor_typing;   // the length on the numpad (0 across, 1 up)
    // Dragged by one hand while the pad is up, it slides across or up only (the way the hand first went; -1 until that
    // is clear), from the lengths it had when taken, in the arrows' steps.
    b8                 anchor_sliding;
    i32                anchor_slide_axis;
    fude_zoom_v2       anchor_slide_grab;   // where the hand took it
    f64                anchor_slide_mm[2];
    rde_ui_button*     anchor_buttons[FUDE_ZOOM_PAGE_ANCHOR_BUTTONS];
    rde_ui_button*     anchor_grip;     // the pad's left end: dragged, the pad moves (and stays where it is left)
    rde_vec_2F         anchor_grip_at;
    b8                 anchor_custom;   // the pad was put somewhere by a hand (else it is at the bottom middle)
    rde_vec_2F         anchor_custom_at;
    b8                 anchor_dragging;
    u64                anchor_drag_hand;
    rde_vec_2F         anchor_drag_from, anchor_from;
    b8                 shown_anchor;
    c8                 anchor_said[2][64];
    rde_vec_2F         anchor_center;
    f32                anchor_w;
    f32                anchor_screen_w, anchor_bar_x;   // (the pad is placed again when these change)
    b8                 shown_kerf_chip;
    rde_vec_2F         kerf_chip_center;
    c8                 kerf_chip_said[96];
    rde_ui_button*     type_chip;       // shown while the Curve tool is on: Type a point (a tap opens the point card)
    b8                 shown_type_chip;
    rde_vec_2F         type_chip_center;
    // The Smoothing tool's Nudge: the pen pushes the pen lines near it. While the pen is down, each pushed line is a
    // copy of its points and the original is hidden (nudge_lifted is the renderer's lifted while nudging).
    b8                 nudge, nudging;
    u32                nudge_frame;
    fude_zoom_v2       nudge_at;          // the pen, in frame units
    rde_arr            nudge_items;       // fude_zoom_nudged
    rde_arr            nudge_points;      // their points, in frame units, one line after another
    rde_arr            nudge_radii;       // ...and each one's half-width (f32, frame units: its pressure kept)
    rde_arr            nudge_handles;     // a path's nodes' handles (frame units), in step with nudge_points
    rde_arr            nudge_lifted;      // one u8 per object
    // Find: its button by the depth; what was last looked for, and which of its matches was last flown to.
    rde_ui_button*     find_button;
    rde_vec_2F         find_center;
    b8                 shown_find;
    c8                 find_query[128];
    u32                find_next;
    u32                find_pdf_base;   // the texts' matches come before a PDF's: how many there are
    // The lasso's loop over a PDF's own text (pdfview.h): its box (in home frame units), while the selection's row
    // offers it (Copy text), on its own when the loop took no ink.
    b8                 pdf_text_on;
    fude_zoom_box      pdf_text_box;
    // A joint being asked for (Insert's Finger joint, Dovetail): which one (1, 2), the question it is at, and its
    // length, finger width or tails, and depth so far (mm; the tails are a count).
    u8                 joint_kind;
    u8                 joint_step;
    f64                joint_mm[3];
    // Handwriting read as text (the lasso's To text, through Google ML Kit where there is one): what is being read (the
    // strokes by their ids, which stay theirs whatever else changes; their box on the canvas, in the units of
    // read_frame, the camera's frame when the reading began, and read_z, the camera's zoom then, by which the text's
    // size is worked out, so the view may move before the answer comes; their colour) while the answer is awaited.
    // read_stale: the canvas was left meanwhile, so the answer is dropped as it comes (ML Kit's one reading is still
    // under way, so `reading` stays set until then and nothing else is handed to it).
    b8                 reading;
    b8                 read_stale;
    rde_arr TYPE(fude_zoom_id) read_strokes;
    fude_zoom_box      read_box;
    u32                read_frame;
    f64                read_z;
    rde_color          read_color;
    b8                 read_wanted;       // To text asked for while a line was being read: done after it
    // The diagram library (symbol.h): its panel open, the family shown, how far its grid is scrolled, the hand on it (a
    // tap or a drag through it; the press that put it away, and its moves too), and the symbol whose text the text card
    // takes. The fields after library_tabs come further down, after the building plans.
    b8                 library_open;
    u8                 library_family;
    u32                library_tabs;   // the tabs it shows (bits: the families', the connectors'), as opened
    // Electronics (circuit.h): the circuit Play worked out (from what the lasso held) and simulates; and the wire pen.
    fude_zoom_circuit  circuit;
    f64                circuit_drawn_at; // Play's time when the current's dots last moved on (they move by Play's time since)
    b8                 circuit_failed;  // it was said that the circuit could not be solved (said once, until it can)
    u32                circuit_held;    // a push button held down (its part in the circuit; FUDE_ZOOM_NONE: none)
    f64                circuit_held_at; // ...since when (the circuit's time)
    u8                 mech_hold;       // a hand on a mechanism playing: 0 none, 1 a part held, 2 a hand crank turned
    u32                mech_hold_crank; // ...the crank (its index in the running world)
    f64                mech_hold_angle; // ...the hand's angle round it (radians, every turn counted)
    f64                mech_hold_offset;// ...the crank's turn less the hand's, when it was taken
    u32                mech_finger;     // ...the finger on it (its id + 1; 0: the pen)
    b8                 circuit_letting; // ...let go too soon: let up once it has been down long enough to be seen
    u32                knob_part;       // an oscilloscope's knob held (its part in the circuit; FUDE_ZOOM_NONE: none)
    u32                knob;            // ...which knob (display.h's)
    i32                knob_way;        // ...which half it was taken on (a tap's way: +1, -1)
    rde_vec_2F         knob_from;       // ...where the hand took it, less the steps it has turned it (on screen)
    f32                knob_step;       // ...how far a step is (screen points)
    b8                 knob_dragged;    // ...dragged (else, when let go, a tap)
    b8                 wire_tool;       // the pen draws wires
    b8                 wiring;          // ...one is being drawn, from:
    u32                wire_from;       // a part (FUDE_ZOOM_NONE: a point)
    i32                wire_from_pin;
    fude_zoom_v2       wire_start;      // (on screen)
    // Mechanisms (mech.h): the plan Play worked out, and its world running on its own (mechrun.h: the view only reads
    // it); and what the canvas leaves for it to draw moved.
    fude_zoom_mech_plan   mech;
    fude_zoom_mech_world  mech_run;
    fude_zoom_coupling    coupling;         // ...and the circuit with it (motors' shafts, buttons its parts press)
    b8                    play_coupled;
    rde_arr TYPE(u8)      mech_lifted;
    // What moves in a symbol as it plays (a solenoid's plunger), for the renderer (render.h's moved).
    rde_arr TYPE(f32)     play_moved;         // (render.h's moved)
    rde_audio_stream*     sound;              // (a speaker's sound: rde's audio stream, open while one plays)
    b8                    play_fine;          // (a speaker or an oscilloscope playing: more steps a frame)
    rde_arr TYPE(u8)      circuit_labelled;   // (scratch: each node's voltage written once while Play draws)
    // Scratch for drawing a mechanism as it plays (page_mech.c), kept so that a frame allocates nothing: a tracer's
    // path on the screen and its stretches there (mechrun.h's fude_zoom_mech_trail_runs), its stroke's half width at
    // each point (the same at every one: grown as needed, never changed), and a belt's loop.
    rde_arr TYPE(rde_vec_2F)   mech_trail;
    rde_arr TYPE(u32)          mech_trail_runs;
    rde_arr TYPE(f32)          mech_trail_radii;
    rde_arr TYPE(fude_zoom_v2) mech_belt_loop;
    // Lining up as it moves (Electronics, Mechanisms): the points of what the lasso holds that line up (a part's pins,
    // a mechanism part's holes and middle) and the others' points, on the screen as the drag began; and what it lines
    // up with now (a level line's y, a plumb line's x; NAN: none).
    rde_arr TYPE(fude_zoom_v2) align_movers;
    rde_arr TYPE(fude_zoom_v2) align_targets;
    rde_vec_2F            align_from;
    f64                   align_x, align_y;
    fude_zoom_v2          align_origin;     // a grid's origin on screen (Electronics: the parts' own grid)
    f64                   align_grid;       // ...its step (0: no grid)
    rde_arr TYPE(fude_zoom_page_drag_wire) align_wires;   // the wires the dragged parts keep, drawn routed to them live
    // Play (the lasso's Play, or the Circuit's and Motion's): what the lasso held (its circuit, its mechanism, its
    // graphs' variables) simulated on its own (the canvas is read to build them, never written), with the view drawing
    // what they are now; and its panel at the bottom (Pause or Go on, Back to the start, Stop; a graph's variables'
    // sliders).
    b8                    play_on;          // playing, or paused
    b8                    play_running;
    b8                    play_circuit, play_mech, play_plots;
    rde_arr TYPE(u8)      play_scope;       // one byte per object: played (one made again for it is too)
    f64                   play_clock;       // the screen's time it last stepped at
    f64                   play_time;        // seconds played
    f64                   play_sim_span, play_real_span;   // (the circuit's seconds against the clock's)
    // The circuit's seconds per second: 1 in time; less is slow motion, when it is too fast to keep up.
    f64                   play_speed;       // the speed it reaches
    f64                   play_rate;        // the speed asked (its chip: 1, ½, ¼, a tenth; kept between Plays)
    b8                    play_step_once;   // Step pressed: a 60th of a second (at its rate) on, then paused
    b8                    play_slow_told;   // (said it is slow, until it keeps up again)
    u32                   play_seen;        // the canvas's revision the circuit was built at (if edited: rebuilt)
    u64                   play_what_key;    // what the lasso held (fude_zoom_page_held_key) when play_what_was was worked out
    u32                   play_what_parts;  // ...the custom parts known then (fude_zoom_logic_revision)
    u8                    play_what_was;    // ...and what of it plays then (fude_zoom_page_play_what, page_mech.c)
    fude_zoom_plot_play   plot_play;        // graphs: their time, and what the sliders hold
    fude_zoom_plot_var    play_vars[4];     // the sliders (the played graphs' variables)
    u32                   play_var_count;
    u32                   play_sensors[4];  // the circuit's sensors (an LDR, a thermistor) with a slider each
    u32                   play_sensor_count;
    i32                   play_slider;      // the slider held (-1: none)
    u64                   play_hand;        // the hand on the panel (0: none)
    rde_vec_2F            play_down;
    // Building plans (plan.h): the Plan tool's wall thickness (mm), and Room in hand (a tap labels the room there).
    f64                   wall_mm;
    b8                    room_tool;
    // More of the diagram library's state (see library_open above).
    f64                library_scroll, library_from;
    rde_vec_2F         library_down;
    u64                library_hand, library_gone;
    b8                 library_dragged;
    u32                symbol_editing;
    u32                connector_heads;   // what a new connector's ends are (symbol.h: the library's Connectors tab)
    // The map (map.h): its button by Find, and its panel over the bottom left. The panel shows the canvas from above as
    // a picture drawn off screen (again when what is drawn, its shape or the view's place on it changes), with the
    // areas, the places and the view drawn over it; and the hand on it (a tap flies; a drag moves the view).
    rde_ui_button*      map_button;
    rde_vec_2F          map_center;
    b8                  shown_map, map_open;
    f64                 map_pressed_at;      // (a tap on a tablet can come twice: as the touch and as the mouse)
    f64                 map_aspect;          // its height over its width (as what it shows was when it opened)
    fude_zoom_box       map_world;           // what it shows, in root units
    f64                 map_world_at;
    fude_zoom_box       map_contents;        // what is drawn, in root units...
    u32                 map_contents_rev;    // ...as of this revision of the scene
    rde_render_texture* map_picture;
    u32                 map_pw, map_ph;      // its size in pixels
    b8                  map_stale, map_ready;
    f64                 map_drawn_at;
    rde_color           map_paper;           // the theme's page colour it was drawn on
    u64                 map_hand, map_gone;
    rde_vec_2F          map_down;
    b8                  map_dragged, map_on_view;
    fude_zoom_v2        map_grab, map_grab_view;   // the hand's point when it went down; the view's middle then
    u32                 map_label_n;               // the areas' names as drawn: their boxes on the screen
    fude_zoom_box       map_label_box[FUDE_ZOOM_MAP_AREAS];
    u32                 map_label_object[FUDE_ZOOM_MAP_AREAS];
    u32                 map_pin_n;                 // the places' pins as drawn
    rde_vec_2F          map_pin_at[FUDE_ZOOM_MAP_PLACES];
    u32                 map_pin_object[FUDE_ZOOM_MAP_PLACES];
    // My pieces (piece.h): the list (read the first time it is wanted), its panel (like the diagram library's: a tile
    // for each piece, its picture drawn from its lines and made once), the hand on it, and the × waiting for its second
    // tap.
    fude_zoom_pieces    pieces;
    // Insert → Examples being made (examples.h): those left to make, the canvas made for the next (drawn once it is
    // open), the folder they go in, the first one made (opened at the end), and how many were made.
    rde_arr TYPE(u32)   examples_todo;
    u32                 examples_waiting;
    u32                 examples_folder;
    u32                 examples_first;
    u32                 examples_made;
    // Custom (custom.h): the library's tab of what is kept. Its tiles as listed now (a piece's place in My pieces, or
    // FUDE_ZOOM_PAGE_CUSTOM_PART | a custom part's place in the logic library); when the hand went down on the panel
    // (held there, it opens a tile's card); and the tile the card is for. Then the template canvases: which is whose
    // (read the first time wanted); the one open (whose it is, the canvas it was opened from for Back, and the revision
    // it was filled or opened at: if it is left changed, what it holds is kept); and the one being made (filled once it
    // is open).
    rde_arr TYPE(u32)   custom_items;
    // The diagram library's tiles for each symbol family: their kinds, family after family, each in the symbols'
    // table's order (an area left out: Insert's own); family f's from library_first[f] up to library_first[f + 1].
    // Made once, at init (fude_zoom_page_library_make): the symbols' table never changes, and the panel reads its tiles
    // each frame.
    rde_arr TYPE(u32)   library_kinds;
    u32                 library_first[FUDE_ZOOM_SYMBOL_FAMILIES + 1u];
    f64                 library_down_at;
    u8                  custom_kind;
    c8                  custom_key[FUDE_ZOOM_CUSTOM_KEY];
    fude_zoom_templates templates;
    b8                  templates_ready;
    b8                  template_on;
    u8                  template_kind;
    c8                  template_key[FUDE_ZOOM_CUSTOM_KEY];
    u32                 template_return, template_return_for;
    u32                 template_seen;
    u32                 template_making, template_making_return;
    u8                  template_making_kind;
    c8                  template_making_key[FUDE_ZOOM_CUSTOM_KEY];
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
    // A container's contents carried by the lasso's drag: how many of the selection's own picks to keep after (0:
    // none).
    u32                 carried_keep;        // the picks to keep after a carry
    // A lone rectangle, ellipse or diagram shape (an area) held by one of its sides' handles is stretched that way
    // only, the side across from it kept where it is. It is drawn as it will be while held, and made so where the pen
    // lifts (one undo step).
    b8                  stretching;
    u32                 stretch_object;
    u8                  stretch_side;        // 0 right, 1 top, 2 left, 3 bottom (its own)
    f64                 stretch_half[2];     // its half sizes when taken (its own units)
    f64                 stretch_to;          // where that side goes, along its way (its own units, from its middle)
    // Present: the areas in the order they were made (or, with none, the places by number), each framed in turn with
    // nothing else on the screen. A tap on the right goes on, on the left goes back. The pen (or the mouse dragged) is
    // a laser pointer whose trail fades, and the pill at the bottom (◀ 2 / 5 ▶ ×) shows for a while after a tap.
    b8                  presenting, present_areas;
    u32                 present_step, present_count;
    u32                 present_objects[FUDE_ZOOM_MAP_AREAS];
    f64                 present_touched_at;
    b8                  present_pen_down, present_pen_moved;
    rde_vec_2F          present_pen_at;
    f64                 present_pen_time;
    // The trail's last FUDE_ZOOM_PAGE_LASER points, oldest first (fude_zoom_page_laser_point; a new one pushes the
    // oldest out), made with the page.
    rde_ring_buffer     laser;
    // Handwriting for Find (handfind.h): the canvas's lines of writing, each read once (as To text reads them) in the
    // background once Find has been asked for on this canvas, and what each says, by its key.
    b8                 hand_on;
    u32                hand_revision;     // the scene's revision when its lines were found
    f64                hand_found_at;
    rde_arr            hand_lines;        // fude_zoom_hand_line
    rde_arr            hand_strokes;      // u32
    rde_hash_map       hand_said;         // a line's key (u64) to what it says (page_find.c's fude_zoom_page_hand_said)
    u64                hand_asking;       // the line being read now (its key; 0: none)
    b8                 hand_asking_stale; // ...read on a canvas left since: its answer let go as it comes (ids, and so
                                          // keys, are a canvas's own: another canvas's line could have the same key)

    // The fingers moving the page.
    fude_zoom_finger   fingers[FUDE_ZOOM_PAGE_FINGERS];
    f64                tap_start;
    u32                tap_fingers;
    b8                 tap_spoiled;

    // A computer's right-button drag (without a drag, it opens the page's menu).
    b8                 mouse_panning;
    // A flick (as on the ink page): how fast the hand pans (screen units a second, with what it moved since the last
    // look waiting), and the view carrying on once let go.
    fude_zoom_v2       pan_velocity;
    fude_zoom_v2       pan_pending;
    f64                pan_moved_at;
    b8                 coasting;
    rde_vec_2F         mouse_last;
    rde_vec_2F         mouse_down;
    b8                 mouse_dragged;

    // When the journal was last written to the file, and the frames the tools may change (render.h's
    // fude_zoom_render_editable, as last asked).
    f64                flushed_at;
    rde_arr TYPE(fude_zoom_visible) editable;
    rde_arr TYPE(f64)               play_sizes;   // (Play's tags: the parts' sizes on the screen, each frame)
    // Where wires join, dots on the screen (page_circuit.c): a wire's loose ends found lying on another wire, and the
    // dots' radius, as last worked out; kept while what they were worked out from is the same (the scene's revision
    // and hidden layers, the frames drawn and where: the editable list, and the screen's half size), and dropped as a
    // canvas closes (a new one's revisions start again).
    rde_arr TYPE(fude_zoom_v2)      junctions;
    rde_arr TYPE(fude_zoom_visible) junctions_view;
    u32                             junctions_rev;
    u32                             junctions_hidden;
    fude_zoom_v2                    junctions_half;
    f32                             junctions_dot;
    b8                              junctions_valid;
    // Whether the canvas has had a wire (none: no joins to look for), from its objects looked through so far, once
    // each as they are made (junctions_looked of them).
    u32                             junctions_looked;
    b8                              junctions_wired;
    // Scratch for finding them (fude_zoom_page_junctions_find), kept so that a look allocates nothing: the objects in a
    // frame on the screen (the pins' marks use it too, each frame the Wire tool is in hand), a wire's points, the
    // loose ends and the wires' segments on the screen.
    rde_arr TYPE(u32)               circuit_found;
    rde_arr TYPE(fude_zoom_v2)      junctions_points;
    rde_arr TYPE(fude_zoom_v2)      junctions_ends;
    rde_arr TYPE(fude_zoom_v2)      junctions_lines;
    // The canvas's attributes (props.h: its PROPS shapes, alive or not), from its objects looked through so far
    // (attributes_looked of them), each once as it is made (props.h's fude_zoom_props_gather): what the logic probes'
    // lights and the bodies' marks walk each frame, instead of every object. Dropped as a canvas closes.
    rde_arr TYPE(u32)               attributes;
    u32                             attributes_looked;
    u32                context_target;   // what the page's menu opened on (its Delete's), or FUDE_ZOOM_NONE
    b8                 context_held;     // ...what the lasso holds (the menu opened inside its box)

    // Getting around (nav.h): the flight under way; where Back goes (the views flights left, newest last, their frames
    // by id, which outlive slots); and the marks for an empty screen, with the camera they were found for.
    fude_zoom_flight   flight;
    struct { fude_zoom_id frame; fude_zoom_v2 at; f64 z; } back[FUDE_ZOOM_PAGE_BACK];
    u32                back_count;
    fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
    u32                mark_count;
    fude_zoom_camera   marks_for;
    f64                marks_at;
    // ...and their widgets (the extension's own: ui.h): Back beside the menu, the depth at the bottom left, and its
    // levels over it once tapped.
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
    // Places (the bookmarks: scene.h): its button by the depth, and its list over it once tapped: first Mark this view
    // (or Unmark), then the places.
    rde_ui_button*     places_button;
    rde_ui_button*     place_rows[FUDE_ZOOM_PAGE_PLACES + 1u];
    rde_ui_button*     place_drops[FUDE_ZOOM_PAGE_PLACES];   // the × beside each place, which lets it go
    u32                place_marks[FUDE_ZOOM_PAGE_PLACES];   // each row's bookmark (its object)
    u32                place_count;     // how many rows of places are shown
    i32                place_here;      // the bookmark the view is at (-1: none); row 0 unmarks it
    b8                 places_open, shown_places, shown_place_rows;
    rde_vec_2F         places_center, place_size;

    // Export (export.h): a PNG under way: its file chosen (stage 1), then the view drawn into the target (stage 2),
    // read back the frame after, once the GPU is done with it.
    u8                 export_stage;
    b8                 export_share;      // to the system's share sheet (a tablet's), not to a chosen file
    c8                 export_path[1024];
    rde_render_texture* export_target;
    // Exporting what the lasso holds: what is kept (a byte per object), the rest hidden (the PNG renderer's lifted),
    // and the view framing it, which the PNG is drawn from the frame after.
    rde_arr            export_keep;
    rde_arr            export_hidden;
    b8                 export_held;
    fude_zoom_camera   export_view;
    // The zoom video (video.h): the camera flown from the whole canvas down to the view on screen. Each frame is drawn
    // off screen (export_stage 10), then read back and handed to the encoder the frame after (11), until the last; then
    // it is shared or saved.
    fude_zoom_video*   video;
    fude_zoom_flight   video_flight;     // (on its own clock: a frame's time, from 0)
    fude_zoom_camera   video_from, video_to;
    u32                video_frame, video_frames;   // the next to draw, and how many in all
    u32                video_w, video_h;
    f64                video_scale;      // its pixels per screen point
} fude_zoom_page;

// The page's hooks for the extension (extension.h: .page_kind): undo, redo, clear, reset view, the session's open and
// save, the selection's commands, imported pictures and Back. Its tools for the toolbar (FUDE_ZOOM_TOOLS) and its
// settings hooks are below.
extern const fude_page_kind      FUDE_ZOOM_PAGE_KIND;
// The canvas apps' sections of Settings (extension.h: .sections): Measurements (the units, the screen's true size),
// the core's Handwriting (Google ML Kit on or off), then Display (the frame rate, in every build: a person can see how
// smoothly their device draws a big canvas).
#define FUDE_ZOOM_PAGE_SECTION_COUNT 3u
extern const fude_extension_section FUDE_ZOOM_PAGE_SECTIONS[FUDE_ZOOM_PAGE_SECTION_COUNT];
// The handwriting model for the language the app is shown in (ML Kit's tag, for drawing/ink/mlkit.h): a canvas app's
// writing is in its reader's language, where a study app's is the language it teaches. The shell sets it at launch.
const c8* fude_zoom_ink_model(void);
// The row over the lasso's selection (extension.h: .selection_row): the core's Cut, Copy, Duplicate and Delete, with
// the canvas's own between them (Play, Value, Limits, Make body, Make part, Inside, Repeat, To text, the formula
// buttons, Edit text, Offset, To curve, Chamfer, Combine, Constrain, To layer, Keep as a piece).
extern const fude_row_def FUDE_ZOOM_SELECTION_ROW;
// Sets how that row shows now (extension.h: .selection_faces). For example, with boards held, To curve becomes Fit
// parts (one board: a list of parts typed for it) or Fit into the largest.
void fude_zoom_page_selection_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
// The page's menu at a long press or a right click (extension.h: .context_row): Paste, Select all, and Delete when it
// opened on something (what the lasso holds, if opened inside its box; otherwise the thing under it).
extern const fude_row_def FUDE_ZOOM_CONTEXT_ROW;
// Sets how that menu shows now (extension.h: .context_faces).
void fude_zoom_page_context_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
// The toolbar's tools (extension.h: .tools): Topic, then the topic's own (product.h), chosen from the twelve
// (page_topics.c's FUDE_ZOOM_BASE_TOOLS): Shapes, Insert, Smoothing, Fill, Arrange, Instruments, Layers, Export,
// Circuit, Motion, Plan and Pages. The slots no topic fills are left off the bar.
extern fude_extension_tool FUDE_ZOOM_TOOLS[FUDE_EXTENSION_TOOLS];   // Topic, then the topic's own
// Fills the bar again for the canvas's topic when it has changed (rebuilt whole with fude_ui_rebuild_now): a topic
// chosen, or a canvas opened in another. Call it once a frame, before the UI is updated.
void fude_zoom_page_topic_follow(fude_zoom_page* _page, struct fude_ui* _ui);

// The longest side, in pixels, a picture is brought down to as it comes in.
#define FUDE_ZOOM_PAGE_PICTURE_PX 4096u

// Move the page's settings into the settings file and out of it (extension.h: .settings_gather, .settings_apply):
// smoothing, units, the screen's true size, the printer's factors, the saw's kerf, the frame rate shown and keeping the
// grain.
void fude_zoom_page_settings_gather(const struct fude_app* _app, fude_settings* _settings);
void fude_zoom_page_settings_apply(struct fude_app* _app, const fude_settings* _settings);
// Its widgets over the page (extension.h: .ui_build, .ui_forget, .ui_update, .ui_restyle, .ui_hit): Back, the depth and
// its levels, Places, Find, the map, the chips and the pads. ui_build makes them (again when the language changes);
// ui_forget lets go of them before the UI canvas goes; ui_update places and shows them once a frame; ui_restyle
// restyles them; ui_hit returns true when a point is on one of them, or a card covers the screen, so the page does not
// take it.
void fude_zoom_page_ui_build(struct fude_ui* _ui, rde_ui_node* _root);
void fude_zoom_page_ui_forget(struct fude_ui* _ui);
void fude_zoom_page_ui_update(struct fude_ui* _ui, b8 _full);
void fude_zoom_page_ui_restyle(struct fude_ui* _ui);
b8   fude_zoom_page_ui_hit(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

// Sets up the page for _app (zeroed, its parts made, its fonts and the symbols' art loaded) and makes it the page the
// extension's hooks reach. No canvas is open until the session opens one.
void fude_zoom_page_init(fude_zoom_page* _page, struct fude_app* _app);
// Returns the screen's half size in points: how far the page reaches each way from its middle.
fude_zoom_v2 fude_zoom_page_half(const fude_zoom_page* _page);
// Closes the canvas (saving it) and frees everything the page holds; the hooks no longer reach it.
void fude_zoom_page_destroy(fude_zoom_page* _page);
// Handles a pen, touch or mouse event while no screen is on top. Keys are not events here: fude_zoom_page_update reads
// them once a frame.
void fude_zoom_page_event(fude_zoom_page* _page, rde_event* _event);
// Called once a frame while no screen is on top: flights, flicks, Play, saving, covers, Find and the rest move on.
void fude_zoom_page_update(fude_zoom_page* _page);
// Draws the page: its paper, the canvas and the stroke being drawn, and everything over them. Call it inside a 2D
// drawing block.
void fude_zoom_page_render(fude_zoom_page* _page, rde_window* _window);
// The system is short of memory: lets go of what can be made again (the renderer's caches, the strip's and the arrows'
// pictures, the map's picture, the note fonts' glyphs).
void fude_zoom_page_trim(fude_zoom_page* _page);
// A developer's looks (look.h): each function below named fude_zoom_page_look_* puts the page into a state as a hand
// would, for a screenshot or a check. The canvas apps' looks (zoom/look.c) call them from their launch flags, which
// only a developer's build reads.
//
// fude_zoom_page_look_pen: the pen as if it went down (_phase 0), moved (1) or lifted (2) at _screen.
void fude_zoom_page_look_pen(fude_zoom_page* _page, u8 _phase, rde_vec_2F _screen);
// Getting around: flown home as Reset view does (0), the depth's levels opened (1), Back (2), the view marked (3),
// Places opened (4).
void fude_zoom_page_look_nav(fude_zoom_page* _page, u8 _what);
// The layers: their panel opened (0), a new layer (1), row _row's eye pressed (2); everything lassoed and moved to row
// _row's layer (4); row _row tapped for its name (5); the row moved down a place (7); any other value presses its lock
// (3).
void fude_zoom_page_look_layers(fude_zoom_page* _page, u8 _what, u32 _row);
// The text card's words replaced by _name, then saved (renaming what the card is for).
void fude_zoom_page_look_rename(fude_zoom_page* _page, const c8* _name);
// _step 0: a line 300 points long with its dimension over it, a square on its end and a line up from its end (one undo
// step), the dimension's chip shown. Any other step: the dimension's line typed half as long again.
void fude_zoom_page_look_drive(fude_zoom_page* _page, u8 _step);
// A board: its first size asked (_step 0); its sizes typed and its name asked, oak chosen (3); otherwise a side and a
// top made side by side, the side lassoed (with _step 2, both), and with _step 4 the side walnut and the top oak.
void fude_zoom_page_look_board(fude_zoom_page* _page, u8 _step);
// Two lines 60 mm long meeting in a corner in the middle of the view, lassoed; then, when _mm > 0, that corner filleted
// _mm round.
void fude_zoom_page_look_fillet(fude_zoom_page* _page, f64 _mm);
// The diagram library's panel opened on family _family.
void fude_zoom_page_look_library(fude_zoom_page* _page, u8 _family);
// Returns how far above the screen's bottom a notice should sit to be clear of the page's panels there (the strip's,
// Find's); a little when none is up.
f32  fude_zoom_page_notice_above(const fude_zoom_page* _page);
// The app is going to the background: a recording being made is stopped (and kept), and one playing is paused.
void fude_zoom_page_audio_pause(fude_zoom_page* _page);
// Makes the next new canvas opened (the home screen's New notebook) a notebook of pages of size _size (pages.h's
// FUDE_ZOOM_PAGES_).
void fude_zoom_page_notebook_next(fude_zoom_page* _page, u8 _size);
// Asks for the canvas's cover to be written in the next frames (the home screen is opening, and should show it as it is
// now).
void fude_zoom_page_cover_now(fude_zoom_page* _page);
// Writes into _out the path of canvas _canvas's cover: <id>.cover.png beside its file.
void fude_zoom_page_cover_path(u32 _canvas, c8* _out, usize _size);
// A notebook's pages strip opened.
void fude_zoom_page_look_strip(fude_zoom_page* _page);   // (a notebook's pages strip opened)
// Audio: 0 a made-up recording _value seconds long (a tone), with what is on the canvas linked over it; 1 its panel; 2
// the newest recording at _value ms; 3 the microphone's chip pressed; 4 the speech model downloaded; 5 the newest
// recording transcribed; 6 written out as text; 7 written out as handwriting; 8 Live text on.
void fude_zoom_page_look_audio(fude_zoom_page* _page, u8 _what, u32 _value);
// A WAV file (16 kHz, mono, 16-bit) made a recording of the canvas, as if it had been recorded here.
void fude_zoom_page_look_audio_import(fude_zoom_page* _page, const c8* _wav);   // (16 kHz mono 16-bit WAV)
// The strip's pages: page _i held, its buttons up (0); carried over place _j (1); a page put in before _i (2), page _i
// deleted (3), or page _i made page _j (4).
void fude_zoom_page_look_strip_do(fude_zoom_page* _page, u8 _what, u32 _i, u32 _j);   // (_what: see above)
// Returns whether the canvas open is a notebook of pages (pages.h). False when no canvas is open.
b8   fude_zoom_page_is_notebook(const fude_zoom_page* _page);
// A diagram symbol (symbol.h's id _id) put _dx, _dy screen points from the view's middle, with _text on it ('|' for a
// line break, '~' for '=').
void fude_zoom_page_look_symbol(fude_zoom_page* _page, const c8* _id, const c8* _text, f64 _dx, f64 _dy);
// A Kanban board of preset _preset put down.
void fude_zoom_page_look_kanban(fude_zoom_page* _page, u32 _preset);
// Insert's Kanban board card up with template _preset in its rows (_extra: a row more), then, with _insert, Insert
// pressed.
void fude_zoom_page_look_kanban_card(fude_zoom_page* _page, u32 _preset, b8 _extra, b8 _insert);
// The map's panel opened; fude_zoom_page_look_area makes an area named _name (round what the lasso holds, or else the
// view).
void fude_zoom_page_look_map(fude_zoom_page* _page);
void fude_zoom_page_look_area(fude_zoom_page* _page, const c8* _name);
// A measured sheet's card up from _spec, "PAPER:WAY:SCALE[:W:H][!]": a paper's index or -1, 1 for landscape, a scale's
// index, the sizes typed, and ! for Insert pressed. "edit" opens the card of the last sheet, lassoed.
void fude_zoom_page_look_sheet(fude_zoom_page* _page, const c8* _spec);
// Lines to trim (page_look.c's look_lines): made, and the first lassoed; then, up to _step, 1 extended, 2 trimmed, 3
// chamfered.
void fude_zoom_page_look_lines(fude_zoom_page* _page, u32 _step);
// A demo circuit (page_look.c's look_circuit: 1 an LED and a switch, 2 an Arduino blinking, 3 a 555, 4 a breadboard, 5
// a full adder of gates, 6 a 74HC161 counting, 7 a 4-bit adder of full adders), with the topic Electronics;
// fude_zoom_page_look_circuit_run lassoes everything and plays it.
void fude_zoom_page_look_circuit(fude_zoom_page* _page, u32 _demo);
void fude_zoom_page_look_circuit_run(fude_zoom_page* _page);
// A demo mechanism (page_look.c's look_mech: 1 a crank and rocker, 2 a gear train, 3 a pendulum and a spring, 4 a
// pulley, 5 a slider-crank, 6 a rack and pinion, 7 drawings made bodies, 8 a gear train on a rack), with the topic
// Mechanisms; fude_zoom_page_look_mech_run lassoes everything and plays it.
void fude_zoom_page_look_mech(fude_zoom_page* _page, u32 _demo);
void fude_zoom_page_look_mech_run(fude_zoom_page* _page);
// A floor plan (page_look.c's look_plan): a room with a door, a window, a bed, a socket and a light (_step 1), then
// labelled (2).
void fude_zoom_page_look_plan(fude_zoom_page* _page, u32 _step);
// Makes a part of the circuit the lasso holds (Make part: logic.h), keeps it in My parts, and puts one of it beside the
// circuit.
void fude_zoom_page_make_part(fude_zoom_page* _page);
// What the last demo drew made a part (a developer's look).
void fude_zoom_page_look_make_part(fude_zoom_page* _page);
// The first symbol _id lassoed alone and dragged _dx, _dy screen points, held there (its alignment lines showing).
void fude_zoom_page_look_align(fude_zoom_page* _page, const c8* _id, f64 _dx, f64 _dy);
// The page's menu opened at _screen as a long press opens it; or, with _delete, its Delete pressed.
void fude_zoom_page_look_context(fude_zoom_page* _page, rde_vec_2F _screen, b8 _delete);
// Insert → Examples as chosen on its card (_which: 0 all, 1 the electronics', 2 the mechanisms', 3 those of both).
void fude_zoom_page_look_examples(fude_zoom_page* _page, u32 _which);
// Make body's card up for what the lasso holds (everything, with _all), material _material, fixed or not, and with
// _apply applied.
void fude_zoom_page_look_body(fude_zoom_page* _page, b8 _all, u32 _material, b8 _fixed, b8 _apply);
// The first symbol _id lassoed alone and given a value on Value's card: _typed in unit _unit, way _way.
void fude_zoom_page_look_value(fude_zoom_page* _page, const c8* _id, const c8* _typed, u32 _unit, u32 _way);
// The first symbol _id lassoed alone, then Limits: real part _preset chosen (-1: none), field _field typed _typed
// (empty: none) in unit _unit (1: thousandths), then _finish: 0 the card left up, 1 Apply, 2 Typical and Apply.
void fude_zoom_page_look_limits(fude_zoom_page* _page, const c8* _id, i32 _preset, u32 _field, const c8* _typed, u32 _unit, u32 _finish);
// Play's slider _slider set _t (0 to 1) of the way along.
void fude_zoom_page_look_slider(fude_zoom_page* _page, u32 _slider, f64 _t);
// The Shapes panel's choice _index pressed.
void fude_zoom_page_look_shapes_choice(fude_zoom_page* _page, u32 _index);
// Shapes to combine (page_look.c's look_combine: a rectangle and a circle, lassoed): _op 0 join, 1 cut out, 2 overlap,
// 9 Combine's card; 10 + _op the same with the rectangle filled.
void fude_zoom_page_look_combine(fude_zoom_page* _page, u32 _op);
// A point typed for the Curve tool from _spec, "MODE:A:B:PRESS": the Curve tool taken, the card opened if it is not up;
// PRESS 0 nothing, 1 Add point, 2 Finish.
void fude_zoom_page_look_point(fude_zoom_page* _page, const c8* _spec);
// The size card up for what the lasso holds, its fields typed from _spec, "W;H;ANGLE;X;Y" (empty ones left; ~ lets its
// proportions go; ! presses Apply).
void fude_zoom_page_look_size(fude_zoom_page* _page, const c8* _spec);
// The lasso's Repeat card up from _spec, "MODE;COPIES;ACROSS;UP;COPIES;ANGLE;CX;CY;LINE;AXIS;KEEP" (empty ones left; !
// presses Apply).
void fude_zoom_page_look_repeat(fude_zoom_page* _page, const c8* _spec);
// Present started, at step _step; fude_zoom_page_look_laser draws a laser pointer's trail across the screen.
void fude_zoom_page_look_present(fude_zoom_page* _page, u32 _step);
void fude_zoom_page_look_laser(fude_zoom_page* _page);
// What the lasso holds kept as piece _name; My pieces' panel up; piece _i put in the middle of the view.
void fude_zoom_page_look_keep(fude_zoom_page* _page, const c8* _name);
void fude_zoom_page_look_pieces(fude_zoom_page* _page);
void fude_zoom_page_look_piece_put(fude_zoom_page* _page, u32 _i);
// Example _example drawn on the canvas open, as Insert → Examples draws each on its own (the view zoomed out to all of
// it).
void fude_zoom_page_look_draw_example(fude_zoom_page* _page, u32 _example);
// Tells the page that a frame's update and render took _update and _render seconds (the whole app's). While the frame
// rate is shown, where the time goes is written to the log every 2 s.
void fude_zoom_page_perf_frame(fude_zoom_page* _page, f64 _update, f64 _render);
// The frame rate shown at the top right (Settings' Display), or not.
void fude_zoom_page_look_fps(fude_zoom_page* _page, b8 _on);
// Settings' Display's Make pressed: the test canvas made and opened, and filled with the test ink the frame after.
void fude_zoom_page_look_inktest(fude_zoom_page* _page);
// The library's Custom tile _i (in the panel as it is open; else the circuits'): _action 0 put down, 1 its card, 2 Edit
// or Inside (its template), 3 renamed _name, 4 deleted. fude_zoom_page_look_inside presses the lasso's Inside (a custom
// part alone); fude_zoom_page_look_back presses the page's Back.
void fude_zoom_page_look_custom(fude_zoom_page* _page, u32 _i, u32 _action, const c8* _name);
void fude_zoom_page_look_inside(fude_zoom_page* _page);
void fude_zoom_page_look_back(fude_zoom_page* _page);
// Piece _i laid as a stencil.
void fude_zoom_page_look_stencil(fude_zoom_page* _page, u32 _i);
// Every sticky note lassoed and let go where it is (those in a Kanban column lined up in it).
void fude_zoom_page_look_tidy(fude_zoom_page* _page);
// The lassoed board's parts fitted from _text ("Side 600 x 300 x2; Top 800 x 300"); with "", or not one board lassoed,
// Fit parts pressed.
void fude_zoom_page_look_fit(fude_zoom_page* _page, const c8* _text);
// Every text selected, then Handwriting pressed.
void fude_zoom_page_look_handwrite(fude_zoom_page* _page);
// The offset pad's grip dragged _dx, _dy (UI units); fude_zoom_page_look_grow drags instrument _kind's size tab _by
// along it (_hold: not let go).
void fude_zoom_page_look_grip(fude_zoom_page* _page, f32 _dx, f32 _dy);
void fude_zoom_page_look_grow(fude_zoom_page* _page, u32 _kind, f64 _by, b8 _hold);
// Instrument _kind snapped where it lies (as if let go there), then moved _across and _up millimetres by the offset
// pad.
void fude_zoom_page_look_anchor(fude_zoom_page* _page, u32 _kind, f64 _across, f64 _up);
// A finger on the instruments as the page takes one (0 down; 1 moved, snapped or slid as a hand's; 2 up), at _at (the
// instruments' units).
void fude_zoom_page_look_finger(fude_zoom_page* _page, u32 _phase, fude_zoom_v2 _at);
// The Lasso taken and a loop drawn round the screen box _a to _b (as the pen would); a PDF's text found in it is
// written to the log.
void fude_zoom_page_look_loop(fude_zoom_page* _page, rde_vec_2F _a, rde_vec_2F _b);
// Another topic chosen from the Topic menu (FUDE_ZOOM_TOPIC_), as a tap on it chooses it.
void fude_zoom_page_look_topic(fude_zoom_page* _page, u32 _topic);
// The selection's Offset pressed (with a board lassoed alone, this turns its grain).
void fude_zoom_page_look_grain(fude_zoom_page* _page);
// A joint put down (_kind 1 a finger joint, 2 a dovetail): its length, finger width or tails, and depth (mm).
// fude_zoom_page_look_nodes works a lassoed path's nodes (0 lassoed, 1 a node dragged, 2 a node added, 3 held, 4
// tapped, 5 its handle dragged); fude_zoom_page_look_to_curve lassoes everything and presses To curve;
// fude_zoom_page_look_holes puts a hole row _mm long, as its numpad does.
void fude_zoom_page_look_joint(fude_zoom_page* _page, u8 _kind, f64 _a, f64 _b, f64 _c);
void fude_zoom_page_look_nodes(fude_zoom_page* _page, u8 _step);
void fude_zoom_page_look_to_curve(fude_zoom_page* _page);
void fude_zoom_page_look_holes(fude_zoom_page* _page, f64 _mm);
// What the lasso holds offset by _mm (outwards; inwards when negative), as Offset's numpad does.
void fude_zoom_page_look_offset(fude_zoom_page* _page, f64 _mm);
// Find for _words, as its card does.
void fude_zoom_page_look_find(fude_zoom_page* _page, const c8* _words);
// A diagram made from Mermaid text _text, as the Insert tool's card does.
void fude_zoom_page_look_mermaid(fude_zoom_page* _page, const c8* _text);
// The true size's calibration begun, as Settings' Calibrate does.
void fude_zoom_page_look_calibrate(fude_zoom_page* _page);
// Draws over the UI, after it (the shell's top overlay, its drawing begun): the Formula card's formula set as it is
// typed.
void fude_zoom_page_render_top(fude_zoom_page* _page);

// Everything lassoed, then its formulas written out by hand (_what 0), or its handwriting read as a formula (1).
void fude_zoom_page_look_formula(fude_zoom_page* _page, u8 _what);

// A text put down at _screen saying _words, as the card's Save does. _kind 0 a text, 1 a sticky note, 2 a formula; 3 a
// formula's card left open; 4 and 5 a text and a note written on the page, left open.
void fude_zoom_page_look_text(fude_zoom_page* _page, rde_vec_2F _screen, u8 _kind, const c8* _words);   // (see above)
// The numpad opened with _text in it, on the last line drawn (as its length chip opens it).
void fude_zoom_page_look_numpad(fude_zoom_page* _page, const c8* _text);
// Export into _path in format _format: 0 PNG, 1 SVG, 2 PDF, 3 DXF, 4 tiles on A4, 5 the printer's check page, 6 the cut
// list, 7 tiles on Letter, 8 the zoom video, 9 the areas as a PDF, 10 the sheets as a PDF, 11 STL, 12 the parts list.
void fude_zoom_page_look_export(fude_zoom_page* _page, u8 _format, const c8* _path);
// Pages' choice _choice, as the Pages tool makes it: a size (pages.h's FUDE_ZOOM_PAGES_) makes the canvas a notebook;
// FUDE_ZOOM_PAGES_SIZES adds a page; anything past it goes back to the endless canvas.
void fude_zoom_page_look_pages(fude_zoom_page* _page, u32 _choice);
// Draws what the page draws off screen, in the frame's render before the page's own drawing (the shell's): its cover,
// the map's picture, a strip page's picture, a zoom video's frame and a PNG export's view, when each is wanted.
void fude_zoom_page_render_offscreen(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);

#endif
