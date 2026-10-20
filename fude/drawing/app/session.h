// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SESSION
#define FUDE_SESSION

#include "rde.h"

// ===========================================================================
// Session: what is kept between launches (save.h's files) and what leaves the app as a file. The open page and the
// settings are written a moment after the last change (never mid-stroke) and on leaving the app, so an iPadOS kill
// loses at most that moment. Canvases switched in the side panel are saved and loaded here, and the app's own files are
// opened here too (extension.h: a study app's marks, exams, words, reviews, notes). Your data's export and import
// (backup.h) are shared on a tablet and saved where chosen on a computer (and the app's own files go the same way: a
// study app's practice sheets).
//
// Where it sits: drawing/app. The shell calls it at launch, each frame and on leaving; the side panel's Your data and
// the document bar use its file helpers.
//
// Its state: one session per process, kept in session.c's statics; there is nothing to create or free.
//
// Threads: main thread only.
//
// Files: the open canvas's page file (pagefile.h), the settings file (settings.h), the notes' index (notes.h), the
// app's own files through its extension, and the backups and the outbox in the save folder (backup.h).
// ===========================================================================

struct fude_app;

// The page and the settings save themselves this many seconds after the last change.
#define FUDE_SESSION_AUTOSAVE_DELAY 1.0

// For the HUD: the last save, and how the page loaded.
typedef struct {
    f64       last_save_time;    // On the engine clock (0 when nothing was saved this run).
    f32       last_save_ms;
    u32       last_save_bytes;
    b8        save_failed;
    const c8* load_note;         // How the page loaded, in words; session.c's own string, valid for the app's life.
} fude_session_info;

// Reads everything at launch: the canvases, the app's own files, the settings, and the open page. The very first time
// (no settings yet) it runs the app's first launch (a study app: the welcome), takes the device's theme, and saves the
// settings at once, so from then on the theme is the learner's.
void fude_session_load(struct fude_app* _app);
// Called once a frame: opens a canvas chosen in the side panel (saving the one left), does what Your data asked for,
// notes any change, and saves once things have been quiet for FUDE_SESSION_AUTOSAVE_DELAY.
void fude_session_update(struct fude_app* _app);
// Writes whatever changed now: the page, the settings, and the app's own page (extension.h: page_kind). _force writes
// them all regardless. It does nothing while a stroke is open; a failed save is tried again after another quiet
// period.
void fude_session_save_now(struct fude_app* _app, b8 _force);
// For when the app is going away (background, terminate, quit): finishes what the pen was doing, writes everything
// now, and marks the session as ended cleanly for the crash reports (crash.h).
void fude_session_save_on_exit(struct fude_app* _app);
// Takes the settings in effect now as the saved ones, so they are not a change to save (a look's theme).
void fude_session_settings_seen(struct fude_app* _app);
// The HUD's view of the last save and of how the page loaded.
fude_session_info fude_session_info_now(void);

// Your data. fude_session_export_to writes everything into one backup file at _path, then hands it to the system's
// share sheet when _share; the card says how it went. fude_session_import_pick reads the file at _path (a path or an
// Android content URI), checks that it is a whole backup, and asks the card's question; nothing changes until Replace.
void fude_session_export_to(struct fude_app* _app, const c8* _path, b8 _share);
void fude_session_import_pick(struct fude_app* _app, const c8* _path);
// Writes into _out the path for a file to share, in the outbox in the save folder, deleting what earlier shares left
// there. Its name is _stem and today's date, with the extension _ext ("<Name> backup 2026-10-01.<id>backup").
void fude_session_outbox_path(c8* _out, usize _size, const c8* _stem, const c8* _ext);
// Writes into _out the chosen _path with _ext (".pdf") added when it was left out. _ext_upper (may be NULL) is another
// ending that counts as having it (".PDF").
void fude_session_with_extension(c8* _out, usize _size, const c8* _path, const c8* _ext, const c8* _ext_upper);

#endif
