#ifndef KANA_SAVE
#define KANA_SAVE

#include "rde.h"
#include "ink.h"
#include "canvas.h"

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
//   'STRK'  u32 count, u32 record size, then per stroke:
//           u32 point_count, u8 r g b a, u8 flags (bit 0: from_pen), 3 reserved
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
//           u8 mlkit (reading with Google ML Kit: 1 on, 0 off)
//           (new fields go at the END: an older file just ends sooner)
// ===========================================================================

#define KANA_SAVE_VERSION        1u
#define KANA_SAVE_DOCUMENT_FILE  "page.kana"
#define KANA_SAVE_SETTINGS_FILE  "settings.kana"

typedef enum {
    KANA_LOAD_OK = 0,
    KANA_LOAD_RECOVERED,  // the main file was missing or damaged; the .bak loaded
    KANA_LOAD_MISSING,    // nothing saved yet
    KANA_LOAD_CORRUPT     // there were files and none parsed; set aside as .bad
} KANA_LOAD_;

RDE_STRUCT {
    u8         tool;             // KANA_TOOL_
    b8         vertical;
    b8         show_hud;
    u8         brush_scale;      // KANA_INK_BRUSH_SCALE_
    u8         width_mode;       // KANA_INK_WIDTH_MODE_
    rde_color  color;
    f32        radius;
    rde_vec_2F toolbar_center;   // UI canvas units
    u8         theme;            // KANA_THEME_
    b8         mlkit;            // read handwriting with Google ML Kit (mlkit.h)
} kana_settings;

// The folder saves live in, created if missing, ending in '/'. iOS: the app's
// Application Support — private to the app and included in device backups.
// Desktop: ./saves/ under the working directory.
const c8*  kana_save_dir(void);

// _out_bytes (may be NULL) receives the file size.
b8         kana_save_document(const c8* _path, const kana_ink* _ink, kana_view _view, u32* _out_bytes);
// Loads into an EMPTY ink (fresh from kana_ink_init). On MISSING or CORRUPT,
// _ink and _view are untouched.
KANA_LOAD_ kana_load_document(const c8* _path, kana_ink* _ink, kana_view* _view);

b8         kana_save_settings(const c8* _path, const kana_settings* _settings);
// Fields the file does not have keep the value _settings came in with. Values
// are range-checked; the caller still clamps anything tool-specific.
KANA_LOAD_ kana_load_settings(const c8* _path, kana_settings* _settings);

b8         kana_settings_equal(const kana_settings* _a, const kana_settings* _b);

#endif
