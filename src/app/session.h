#ifndef KANA_SESSION
#define KANA_SESSION

#include "rde.h"

// ===========================================================================
// What is kept between launches (save.h's files): the page open and the
// settings, written a moment after the last change (never mid-stroke) and on
// leaving the app — so an iPadOS kill loses at most that moment; the canvases
// switched in the side panel; the study files opened (marks, exams, words,
// reviews, notes). And what leaves the app as a file: Your data's export and
// import (backup.h), a practice sheet (sheet.h) — shared on a tablet, saved
// where chosen on a computer.
// ===========================================================================

struct kana_app;

// The page and the settings save themselves this long after the last change.
#define KANA_SESSION_AUTOSAVE_DELAY 1.0

// For the HUD: the last save, and how the page loaded.
typedef struct {
    f64       last_save_time;    // engine clock (0: not this run)
    f32       last_save_ms;
    u32       last_save_bytes;
    b8        save_failed;
    const c8* load_note;
} kana_session_info;

// Everything read: the canvases, the study files, the settings, the open page.
// The very first time, the welcome opens and the theme follows the device's.
void kana_session_load(struct kana_app* _app);
// Once a frame: a change noted, and saved once things have been quiet a while;
// a canvas chosen in the side panel opened; Your data's requests; a sheet asked for.
void kana_session_update(struct kana_app* _app);
// Whatever changed, written now (_force: both files regardless). Not while a
// stroke is open.
void kana_session_save_now(struct kana_app* _app, b8 _force);
// The app is going away (background, terminate, quit): what the pen was doing
// finished, everything written now.
void kana_session_save_on_exit(struct kana_app* _app);
// The settings on screen taken as the saved ones (a look's theme: not a change to save).
void kana_session_settings_seen(struct kana_app* _app);
kana_session_info kana_session_info_now(void);

// Your data: everything into one file at _path (_share: then to the system's
// share sheet); a file picked to import, inspected and its question asked.
void kana_session_export_to(struct kana_app* _app, const c8* _path, b8 _share);
void kana_session_import_pick(struct kana_app* _app, const c8* _path);
// A practice sheet of _records at _path (_cut: more were asked for than fit;
// _share: then to the share sheet).
void kana_session_sheet(struct kana_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share);

#endif
