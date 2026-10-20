// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SETTINGS
#define FUDE_SETTINGS

#include "rde.h"
#include "drawing/base/save.h"

// ===========================================================================
// Settings file: the tools, the toolbar and the app's choices, kept apart from any one page, saved in the KANA format
// (kfile.h).
//
// Where it sits: drawing/app. The session (session.c) gathers a fude_settings from the app, saves it, and loads and
// applies it at launch; the app's extension adds its own fields (extension.h: settings_gather, settings_apply).
//
// Its main types and their lifecycle: fude_settings is plain data, filled by the caller.
//
// Threads: main thread only.
//
// Files: FUDE_SAVE_SETTINGS_FILE in the save folder (save.h), of kind 'SETT'. It has one chunk, 'PREF', holding these
// fields in this order:
//   u8 tool, u8 vertical, u8 show_hud, u8 brush_scale, u8 width_mode, u8 r g b a (the colour), f32 radius,
//   f32 toolbar_center.x, f32 toolbar_center.y, u8 theme,
//   u8 mlkit (reading with Google ML Kit: 1 on, 0 off),
//   u8 toolbar_minimized (1 when the bar is folded to its grip, 0 when open),
//   u8 paper_size (the size of lines and squares: FUDE_PAPER_SIZE_),
//   u8 language (an RDE_LANGUAGE_, an enum that only grows; 0 when never chosen),
//   u8 finger_writes (1 when one finger writes on a tablet, the toolbar's hand; 0 when only the pen does),
//   u8 pen_ever (1 when a pen has written here),
//   u8 r g b a (the marker's colour), f32 the marker's half-width,
//   u8 cards_read (the must-read cards closed, one bit each: readcard.h),
//   u8 ui_size (the interface's size: FUDE_UI_SIZE_, app.h; 0 for the device's),
//   u8 smoothing (the canvas apps', zoom/smooth.h: its level + 1; 0 when never chosen),
//   f32 eraser_radius, u8 units, f32 true_mm_per_point, f32 print_x, f32 print_y, f32 kerf_mm,
//   u8 show_fps (the canvas apps' frame rate drawn at the top right: 1 on),
//   u8 keep_grain (the canvas apps' fitting: 1 keeps the parts' grain along the board's; 0 lets them turn a quarter),
//   u8 smooth_chosen (1 when the smoothing above was chosen by hand; 0, or a file from before this field, means it
//   never was, and the default, off, applies whatever is stored),
//   u8 mlkit_chosen (always 1: the mlkit byte above is a person's choice. A file from before this field has its mlkit
//   byte read as nothing, and ML Kit is on, its default: the canvas apps wrote 0 there before they had the switch).
//
// Design: new fields go at the END, so an older file just ends sooner and its missing fields keep their defaults. A
// file from before the hand belongs to a pen user, so the pen writes.
// ===========================================================================

#define FUDE_SETTINGS_VERSION   1u
#define FUDE_SAVE_SETTINGS_FILE "settings.kana"   // In the save folder (save.h).
// How many lengths Measurements offers (fude_settings.units): a file holding any other is read as millimetres.
#define FUDE_SETTINGS_UNITS     4u

// The settings, as saved.
RDE_STRUCT {
    u8         tool;             // FUDE_TOOL_
    b8         vertical;
    b8         show_hud;
    u8         brush_scale;      // FUDE_INK_BRUSH_SCALE_
    u8         width_mode;       // FUDE_INK_WIDTH_MODE_
    rde_color  color;
    f32        radius;
    rde_vec_2F toolbar_center;   // In UI canvas units.
    u8         theme;            // FUDE_THEME_
    b8         mlkit;            // Read handwriting with Google ML Kit (mlkit.h).
    b8         toolbar_minimized;   // The bar is folded to its grip.
    u8         paper_size;        // FUDE_PAPER_SIZE_
    u8         language;          // RDE_LANGUAGE_ (0 when never chosen: the device's language).
    b8         finger_writes;     // One finger writes (on a tablet; the toolbar's hand).
    // A pen has been used here; from then on, turning the hand on is the learner's choice.
    b8         pen_ever;
    rde_color  marker_color;      // The marker's colour (ink.h), see-through.
    f32        marker_radius;
    u8         cards_read;        // The must-read cards closed, one bit each (readcard.h).
    u8         ui_size;           // FUDE_UI_SIZE_ (app.h; 0 for the device's).
    u8         smoothing;         // The canvas apps' smoothing (zoom/smooth.h): its level + 1 (0 when never chosen).
    f32        eraser_radius;     // The eraser's reach (ink.h; 0 when never chosen).
    // The canvas apps' lengths, as Measurements offers them (zoom/page_export.c's fude_zoom_page_set_units; not a
    // FUDE_ZOOM_UNIT_): 0 mm, 1 cm, 2 inches, 3 feet and inches; FUDE_SETTINGS_UNITS of them.
    u8         units;
    // The canvas apps' true size: how many millimetres a screen point really is (0 when not calibrated).
    f32        true_mm_per_point;
    // The canvas apps' printer: how long a printed millimetre comes out, across and down (0 when never checked).
    f32        print_x, print_y;
    f32        kerf_mm;           // The saw: how wide its cut is, in millimetres (0 when never set: 3 is used).
    b8         show_fps;          // The canvas apps': the frame rate drawn at the top right (Settings' Display).
    // The canvas apps' fitting of parts into a board: their grain is kept along the board's; otherwise they may turn a
    // quarter.
    b8         keep_grain;
    // The smoothing above was chosen by hand. Otherwise its default (off) applies, because a file from before this
    // field saved a smoothing anyway.
    b8         smooth_chosen;
} fude_settings;

// Writes _settings to the file at _path, atomically (kfile.h). Returns false when it could not be written.
b8         fude_save_settings(const c8* _path, const fude_settings* _settings);
// Reads the file at _path (or its backup: save.h) into _settings and says how the load went. Fields the file does not
// have keep the value _settings came in with, and a file that does not read leaves _settings as it was. Values are
// range-checked; the caller still clamps anything tool-specific.
FUDE_LOAD_ fude_load_settings(const c8* _path, fude_settings* _settings);

// Whether two settings are the same, field by field (floats compared by their bits); the session saves only when
// they differ.
b8         fude_settings_equal(const fude_settings* _a, const fude_settings* _b);

#endif
