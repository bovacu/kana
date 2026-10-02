#ifndef FUDE_SAVE
#define FUDE_SAVE

#include "rde.h"
#include "drawing/ink/ink.h"
#include "drawing/ink/canvas.h"

// ===========================================================================
// Saving and loading.
//
// FORMAT: one small tagged binary layout for every Kana file.
//
//   header   "KANA"  u32 version  u32 kind                        (12 bytes)
//   chunk*   u32 tag  u32 size  payload[size]
//
// Every number is little-endian, written byte by byte so the format does not
// depend on the machine; floats as their IEEE-754 bits. Readers SKIP chunk tags
// they do not know, and within a chunk read only the fields it has. Arrays carry
// their record size, so a record can grow new fields at its end. New data goes in
// new chunks or at the END of a record: older files keep loading, and older
// builds keep reading newer files.
//
// WRITES ARE ATOMIC: the file is written to <name>.tmp, the previous version is
// renamed to <name>.bak, then .tmp is renamed into place. A kill at any point
// leaves the old file or the new one, never half of one. Loading falls back to
// .bak when the main file is missing or damaged. A file that fails to parse is
// renamed to <name>.bad and never overwritten: a reader bug must not be able to
// destroy a page by autosaving an empty one over it.
//
// DOCUMENT ('DOC '): the page.
//   'VIEW'  f32 offset.x, offset.y, zoom
//   'PAGE'  u8 paper (FUDE_PAPER_: 0 dots, 1 squares, 2 lines, 3 none)
//           (optional: a file from before it has none — dots)
//   'STRK'  u32 count, u32 record size, then per stroke:
//           u32 point_count, u8 r g b a, u8 flags (bit 0: from_pen; bit 1: the
//           marker's, see-through under the other ink), 3 reserved
//           (r g b a all 0: the theme's ink; 30 30 36 255, from before themes,
//           is read as that too)
//   'PNTS'  u32 count, u32 record size, then every point of those strokes in
//           order: f32 x, y, pressure, radius, time   (canvas units, seconds)
// Only ALIVE strokes are written, and not the undo history: a loaded page is
// where the history starts, as in most drawing apps.
//
// SETTINGS ('SETT'): the tools and the toolbar, apart from any one page.
//   'PREF'  u8 tool, vertical, show_hud, brush_scale, width_mode, u8 r g b a,
//           f32 radius, f32 toolbar_center.x, toolbar_center.y, u8 theme,
//           u8 mlkit (reading with Google ML Kit: 1 on, 0 off),
//           u8 toolbar_minimized (the bar folded to its grip: 1, open: 0),
//           u8 paper_size (lines' and squares' size: FUDE_PAPER_SIZE_),
//           u8 language (RDE_LANGUAGE_: RDE's enum only grows; 0: never chosen),
//           u8 finger_writes (1: one finger writes on a tablet — the toolbar's
//           hand; 0: only the pen), u8 pen_ever (1: a pen has written here),
//           u8 r g b a (the marker's colour), f32 the marker's half-width
//           (new fields go at the END: an older file just ends sooner; one from
//           before the hand is a pen user's: the pen writes)
// ===========================================================================

#define FUDE_SAVE_VERSION        1u
#define FUDE_SAVE_DOCUMENT_FILE  "page.kana"
#define FUDE_SAVE_SETTINGS_FILE  "settings.kana"

typedef enum {
    FUDE_LOAD_OK = 0,
    FUDE_LOAD_RECOVERED,  // the main file was missing or damaged; the .bak loaded
    FUDE_LOAD_MISSING,    // nothing saved yet
    FUDE_LOAD_CORRUPT     // there were files and none parsed; set aside as .bad
} FUDE_LOAD_;

RDE_STRUCT {
    u8         tool;             // FUDE_TOOL_
    b8         vertical;
    b8         show_hud;
    u8         brush_scale;      // FUDE_INK_BRUSH_SCALE_
    u8         width_mode;       // FUDE_INK_WIDTH_MODE_
    rde_color  color;
    f32        radius;
    rde_vec_2F toolbar_center;   // UI canvas units
    u8         theme;            // FUDE_THEME_
    b8         mlkit;            // read handwriting with Google ML Kit (mlkit.h)
    b8         toolbar_minimized;   // the bar folded to its grip
    u8         paper_size;        // FUDE_PAPER_SIZE_
    u8         language;          // RDE_LANGUAGE_ (0: never chosen — the device's)
    b8         finger_writes;     // one finger writes (a tablet; the toolbar's hand)
    b8         pen_ever;          // a pen has been used here (then the hand is the learner's to turn on)
    rde_color  marker_color;      // the marker's (ink.h), see-through
    f32        marker_radius;
} fude_settings;

// The folder saves live in, created if missing, ending in '/'. iOS: the app's
// Application Support — private to the app and included in device backups —
// in a folder of its own (the app's id, info.h: the session sets it as it loads,
// before anything is saved or read). Desktop: ./saves/ under the working directory.
const c8*  fude_save_dir(void);
void       fude_save_set_folder(const c8* _name);

// _out_bytes (may be NULL) receives the file size.
b8         fude_save_document(const c8* _path, const fude_ink* _ink, fude_view _view, fude_page _page, u32* _out_bytes);
// Loads into an EMPTY ink (fresh from fude_ink_init). On MISSING or CORRUPT,
// _ink, _view and _page are untouched.
FUDE_LOAD_ fude_load_document(const c8* _path, fude_ink* _ink, fude_view* _view, fude_page* _page);

b8         fude_save_settings(const c8* _path, const fude_settings* _settings);
// Fields the file does not have keep the value _settings came in with. Values
// are range-checked; the caller still clamps anything tool-specific.
FUDE_LOAD_ fude_load_settings(const c8* _path, fude_settings* _settings);

b8         fude_settings_equal(const fude_settings* _a, const fude_settings* _b);

#endif
