#ifndef FUDE_SESSION
#define FUDE_SESSION

#include "rde.h"

// ===========================================================================
// What is kept between launches (save.h's files): the page open and the
// settings, written a moment after the last change (never mid-stroke) and on
// leaving the app — so an iPadOS kill loses at most that moment; the canvases
// switched in the side panel; the app's own files opened (extension.h: Kana's
// marks, exams, words, reviews, notes). And what leaves the app as a file:
// Your data's export and import (backup.h) — shared on a tablet, saved where
// chosen on a computer (and the app's own the same way: Kana's practice sheets).
// ===========================================================================

struct fude_app;

// The page and the settings save themselves this long after the last change.
#define FUDE_SESSION_AUTOSAVE_DELAY 1.0

// For the HUD: the last save, and how the page loaded.
typedef struct {
    f64       last_save_time;    // engine clock (0: not this run)
    f32       last_save_ms;
    u32       last_save_bytes;
    b8        save_failed;
    const c8* load_note;
} fude_session_info;

// Everything read: the canvases, the app's own files, the settings, the open
// page. The very first time, the app's first launch (Kana: the welcome), and the
// theme follows the device's.
void fude_session_load(struct fude_app* _app);
// Once a frame: a change noted, and saved once things have been quiet a while;
// a canvas chosen in the side panel opened; Your data's requests.
void fude_session_update(struct fude_app* _app);
// Whatever changed, written now (_force: both files regardless). Not while a
// stroke is open.
void fude_session_save_now(struct fude_app* _app, b8 _force);
// The app is going away (background, terminate, quit): what the pen was doing
// finished, everything written now.
void fude_session_save_on_exit(struct fude_app* _app);
// The settings on screen taken as the saved ones (a look's theme: not a change to save).
void fude_session_settings_seen(struct fude_app* _app);
fude_session_info fude_session_info_now(void);

// Your data: everything into one file at _path (_share: then to the system's
// share sheet); a file picked to import, inspected and its question asked.
void fude_session_export_to(struct fude_app* _app, const c8* _path, b8 _share);
void fude_session_import_pick(struct fude_app* _app, const c8* _path);
// A file to share goes in the outbox in the save folder (what an earlier one
// left there let go): its path, _stem and today's date in its name, _ext its
// extension ("Kana backup 2026-10-01.kanabackup").
void fude_session_outbox_path(c8* _out, usize _size, const c8* _stem, const c8* _ext);
// A chosen path with _ext (".pdf") added when it was left out (_ext_upper: that
// too counts; may be NULL).
void fude_session_with_extension(c8* _out, usize _size, const c8* _path, const c8* _ext, const c8* _ext_upper);

#endif
