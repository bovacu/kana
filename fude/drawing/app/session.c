// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/session.h"
#include "drawing/base/redraw.h"
#include "drawing/base/crash.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/ink/pagefile.h"
#include "drawing/app/settings.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/base/backup.h"
#include "drawing/ink/mlkit.h"


// ===========================================================================
// See session.h.
//
// Saving works by comparing. The session keeps two copies of what is saved: what was last WRITTEN (the ink's
// revision, the view, what the page shows under the ink as its paper and its turns' revision, the settings, and the
// app's own page's revision) and what was last SEEN. Each frame, what is on screen is compared with what was seen: any
// difference restarts the quiet period. Once nothing has changed for FUDE_SESSION_AUTOSAVE_DELAY, whatever differs
// from what was written is written. Nothing is saved while a stroke is open or the app's own page is busy (and the
// autosave also waits for a lasso drag), so a file never holds half a stroke.
// ===========================================================================

// What the page shows under its ink, as the session follows it: its paper, and its turns' revision (canvas.h), so the
// list of turns itself is never copied.
typedef struct {
    FUDE_PAPER_ paper;
    u32         turns;
} fude_session_shown;

RDE_INTERNAL c8            fude_session_document[RDE_MAX_PATH];   // The open canvas's page file.
RDE_INTERNAL u32           fude_session_canvas = 0;               // The canvas whose page is in the ink.
RDE_INTERNAL c8            fude_session_settings_path[RDE_MAX_PATH];
RDE_INTERNAL u8            fude_session_language = 0;             // Saved RDE_LANGUAGE_ (0: never chosen, device's).

// What was last written, and what was last seen (to time the quiet period).
RDE_INTERNAL u32                fude_session_saved_revision = 0;
RDE_INTERNAL fude_view          fude_session_saved_view;
RDE_INTERNAL fude_session_shown fude_session_saved_page;
RDE_INTERNAL fude_settings      fude_session_saved_settings;
RDE_INTERNAL u32                fude_session_seen_revision = 0;
RDE_INTERNAL fude_view          fude_session_seen_view;
RDE_INTERNAL fude_session_shown fude_session_seen_page;
RDE_INTERNAL fude_settings      fude_session_seen_settings;
RDE_INTERNAL f64                fude_session_changed_at = 0.0;
// The app's own page (extension.h: page_kind), when it has one: its revision last written, and last seen.
RDE_INTERNAL u32           fude_session_kind_saved = 0;
RDE_INTERNAL u32           fude_session_kind_seen  = 0;

// Returns the app's own page, or NULL when the app uses the core's page.
RDE_INTERNAL const fude_page_kind* fude_session_kind(const fude_app* _app) {
    return fude_app_ext(_app)->page_kind;
}

// The last save, for the HUD.
RDE_INTERNAL fude_session_info fude_session_hud = { 0.0, 0.0f, 0u, false, "" };

// Returns Your data's file extension: the app's id and "backup" (name: namebackup). It is in a static buffer,
// rewritten by each call.
RDE_INTERNAL const c8* fude_session_backup_ext(const fude_app* _app) {
    static c8 _ext[80];
    rde_snprintf(_ext, sizeof(_ext), "%sbackup", fude_app_id(_app));
    return _ext;
}
// How the page loaded, in words, for the HUD.
RDE_INTERNAL c8                fude_session_load_note[160] = "";

// Returns the last save and how the page loaded, for the HUD.
fude_session_info fude_session_info_now(void) {
    fude_session_hud.load_note = fude_session_load_note;
    return fude_session_hud;
}

// Returns what the page shows under its ink now, as the session follows it.
RDE_INTERNAL fude_session_shown fude_session_shown_now(const fude_app* _app) {
    return (fude_session_shown){ _app->canvas->page.paper, _app->canvas->page.revision };
}

// Whether the page shows what it showed: the same paper, and no page turned since.
RDE_INTERNAL b8 fude_session_same_page(fude_session_shown _a, fude_session_shown _b) {
    return _a.paper == _b.paper && _a.turns == _b.turns;
}

// Whether two views are the same, byte for byte.
RDE_INTERNAL b8 fude_session_same_view(fude_view _a, fude_view _b) {
    return rde_memcmp(&_a, &_b, sizeof(fude_view)) == 0;
}

// --- the settings ----------------------------------------------------------------------------

// Returns the settings in effect now, gathered from the toolbar, the ink, the canvas, the app and the app's
// extension.
RDE_INTERNAL fude_settings fude_session_gather(const fude_app* _app) {
    const fude_toolbar* _bar = &_app->ui->bar;
    fude_settings       _s;
    rde_memset(&_s, 0, sizeof(_s));
    _s.tool              = (u8)_bar->tool;
    _s.vertical          = _bar->vertical;
    _s.show_hud          = false;   // No longer a setting, but the byte stays in the file.
    _s.brush_scale       = (u8)_app->ink->brush_scale;
    _s.width_mode        = (u8)_app->ink->width_mode;
    _s.color             = _app->ink->color;
    _s.radius            = _app->ink->constant_radius;
    _s.marker_color      = _app->ink->marker_color;
    _s.marker_radius     = _app->ink->marker_radius;
    _s.eraser_radius     = _app->ink->eraser_radius;
    _s.toolbar_center    = _bar->center;
    _s.theme             = (u8)fude_theme_index();
    _s.toolbar_minimized = _bar->minimized;
    _s.paper_size        = (u8)_app->canvas->paper_size;
    // The language, once one other than the device's has been chosen; 0 until then, meaning the device's.
    _s.language          = fude_session_language != 0 || fude_text_language() != fude_text_default_language() ? (u8)fude_text_language() : 0u;
    _s.finger_writes     = _app->finger_writes;
    _s.pen_ever          = _app->pen_ever;
    _s.cards_read        = _app->cards_read;
    _s.ui_size           = _app->ui_size;
    _s.mlkit             = fude_mlkit_enabled();
    if(fude_app_ext(_app)->settings_gather != NULL) {
        fude_app_ext(_app)->settings_gather(_app, &_s);   // The app's own.
    }
    return _s;
}

// Applies loaded settings to the toolbar, the ink, the theme, the canvas and the app, clamping each to what the tools
// allow, then lets the app's extension apply its own.
RDE_INTERNAL void fude_session_apply(fude_app* _app, const fude_settings* _s) {
    fude_toolbar* _bar = &_app->ui->bar;
    _bar->tool                   = _s->tool == FUDE_TOOL_ERASE ? FUDE_TOOL_ERASE : _s->tool == FUDE_TOOL_LASSO ? FUDE_TOOL_LASSO : _s->tool == FUDE_TOOL_MARK ? FUDE_TOOL_MARK : FUDE_TOOL_DRAW;
    _app->ink->brush_scale       = _s->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN || fude_app_ext(_app)->brush_on_screen ? FUDE_INK_BRUSH_SCALE_SCREEN : FUDE_INK_BRUSH_SCALE_PAGE;
    _app->ink->width_mode        = _s->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE ? FUDE_INK_WIDTH_MODE_PRESSURE : FUDE_INK_WIDTH_MODE_CONSTANT;
    _app->ink->color             = _s->color;
    _app->ink->constant_radius   = rde_math_clamp_f32(_s->radius, FUDE_TOOLBAR_SIZE_MIN, FUDE_TOOLBAR_SIZE_MAX);
    if(_s->marker_color.a > 0u) {   // A file from before the marker keeps the ink's defaults.
        _app->ink->marker_color  = _s->marker_color;
        _app->ink->marker_radius = rde_math_clamp_f32(_s->marker_radius, FUDE_INK_MARKER_MIN, FUDE_INK_MARKER_MAX);
    }
    _app->ink->eraser_radius     = _s->eraser_radius > 0.0f ? rde_math_clamp_f32(_s->eraser_radius, FUDE_INK_ERASER_MIN, FUDE_INK_ERASER_MAX) : FUDE_INK_ERASER_RADIUS;
    fude_theme_set((FUDE_THEME_)_s->theme);
    fude_mlkit_set_enabled(_s->mlkit);   // (every ML Kit service obeys it: mlkit.h)
    if(fude_app_ext(_app)->settings_apply != NULL) {
        fude_app_ext(_app)->settings_apply(_app, _s);
    }
    _app->canvas->paper_size     = _s->paper_size < FUDE_PAPER_SIZE_COUNT ? (FUDE_PAPER_SIZE_)_s->paper_size : FUDE_PAPER_MEDIUM;
    // A language the app no longer offers falls back to the device's (the language it taught before another took
    // its place, for example).
    fude_session_language        = fude_text_language_offered((RDE_LANGUAGE_)_s->language) ? _s->language : 0u;
    _app->finger_writes          = _s->finger_writes;
    _app->pen_ever               = _s->pen_ever;
    _app->cards_read             = _s->cards_read;
    _app->ui_size                = _s->ui_size;
    fude_app_apply_ui_size(_app);   // So the first frame is laid out for it.
    if(fude_session_language != 0 && (RDE_LANGUAGE_)fude_session_language != fude_text_language()) {
        fude_text_set_language((RDE_LANGUAGE_)fude_session_language);   // The UI follows next frame.
    }
    fude_toolbar_set_placement(_bar, _s->vertical, _s->toolbar_center, _s->toolbar_minimized);
    fude_ui_apply_theme(_app->ui);   // The theme, and what the tools show.
}

// Takes the settings now as both written and seen, so they are not a change to save.
void fude_session_settings_seen(fude_app* _app) {
    fude_session_saved_settings = fude_session_seen_settings = fude_session_gather(_app);
}

// --- saving -----------------------------------------------------------------------------------

// Writes what differs from what was last written (everything when _force): the page file, the settings and the app's
// own page. Each part's "written" copy moves on only when it was written, so a failed part is tried again. Records
// the time and size for the HUD.
void fude_session_save_now(fude_app* _app, b8 _force) {
    const fude_page_kind* _kind = fude_session_kind(_app);
    if(_app->ink->drawing || (_kind != NULL && _kind->busy(_app))) {
        return;
    }
    const fude_settings _settings = fude_session_gather(_app);
    const b8 _doc_dirty = _force || _app->ink->revision != fude_session_saved_revision || !fude_session_same_view(_app->canvas->view, fude_session_saved_view) ||
                          !fude_session_same_page(fude_session_shown_now(_app), fude_session_saved_page);
    const b8 _set_dirty  = _force || !fude_settings_equal(&_settings, &fude_session_saved_settings);
    const b8 _kind_dirty = _kind != NULL && (_force || _kind->revision(_app) != fude_session_kind_saved);
    if(!_doc_dirty && !_set_dirty && !_kind_dirty) {
        return;
    }

    const f64 _start = rde_engine_get_time_now();
    b8        _ok    = true;
    if(_doc_dirty) {
        u32 _bytes = 0;
        if(fude_save_document(fude_session_document, _app->ink, _app->canvas->view, _app->canvas->page, &_bytes)) {
            fude_session_saved_revision     = _app->ink->revision;
            fude_session_saved_view         = _app->canvas->view;
            fude_session_saved_page         = fude_session_shown_now(_app);
            fude_session_hud.last_save_bytes = _bytes;
        } else {
            _ok = false;
        }
    }
    if(_set_dirty) {
        if(fude_save_settings(fude_session_settings_path, &_settings)) {
            fude_session_saved_settings = _settings;
        } else {
            _ok = false;
        }
    }
    if(_kind_dirty) {
        const u32 _revision = _kind->revision(_app);
        if(_kind->save(_app, _force)) {
            fude_session_kind_saved = _revision;
        } else {
            _ok = false;
        }
    }
    const f64 _now = rde_engine_get_time_now();
    fude_session_hud.last_save_ms   = (f32)((_now - _start) * 1000.0);
    fude_session_hud.last_save_time = _now;
    fude_session_hud.save_failed    = !_ok;
    // A failure retries after another quiet period rather than every frame.
    if(!_ok) {
        fude_session_changed_at = _now;
    }
}

// Called once a frame: notes any change (which restarts the quiet period), and saves once nothing has changed for
// FUDE_SESSION_AUTOSAVE_DELAY and nothing is being drawn or dragged.
RDE_INTERNAL void fude_session_autosave(fude_app* _app) {
    const f64             _now      = rde_engine_get_time_now();
    const fude_settings   _settings = fude_session_gather(_app);
    const fude_page_kind* _kind     = fude_session_kind(_app);
    const u32             _kind_rev = _kind != NULL ? _kind->revision(_app) : 0u;
    if(_app->ink->revision != fude_session_seen_revision || !fude_session_same_view(_app->canvas->view, fude_session_seen_view) ||
       !fude_session_same_page(fude_session_shown_now(_app), fude_session_seen_page) || !fude_settings_equal(&_settings, &fude_session_seen_settings) ||
       _kind_rev != fude_session_kind_seen) {
        fude_session_kind_seen     = _kind_rev;
        fude_session_seen_revision = _app->ink->revision;
        fude_session_seen_view     = _app->canvas->view;
        fude_session_seen_page     = fude_session_shown_now(_app);
        fude_session_seen_settings = _settings;
        fude_session_changed_at    = _now;
        fude_redraw_after(FUDE_SESSION_AUTOSAVE_DELAY);   // (render on demand: the save is due then, with no event)
        return;
    }
    if(!_app->ink->drawing && !fude_lasso_busy(_app->lasso) && (_kind == NULL || !_kind->busy(_app)) && _now - fude_session_changed_at >= FUDE_SESSION_AUTOSAVE_DELAY) {
        fude_session_save_now(_app, false);
    }
    // Render on demand: a save still due (one that failed waits another quiet period: fude_session_save_now) is a
    // timer. One held back by a stroke or a drag needs none: what holds it ends with an event.
    const f64 _since = rde_engine_get_time_now() - fude_session_changed_at;
    if(_since < FUDE_SESSION_AUTOSAVE_DELAY) {
        fude_redraw_after(FUDE_SESSION_AUTOSAVE_DELAY - _since);
    }
}

// The app is going away: marks the session as ended cleanly, finishes what the pen was doing, and writes everything.
void fude_session_save_on_exit(fude_app* _app) {
    fude_crash_session(false);   // The app is off the screen, so being killed now is not a crash.
    fude_page_let_go(_app->page);
    fude_session_save_now(_app, false);
    // Write all of the app's own page now, whatever its revision says.
    const fude_page_kind* _kind = fude_session_kind(_app);
    if(_kind != NULL && _kind->save(_app, true)) {
        fude_session_kind_saved = _kind->revision(_app);
    }
}

// Takes what is on screen now as both written and seen: it IS what is saved, just after a load.
RDE_INTERNAL void fude_session_on_screen_is_saved(fude_app* _app) {
    const fude_page_kind* _kind = fude_session_kind(_app);
    fude_session_kind_saved     = fude_session_kind_seen     = _kind != NULL ? _kind->revision(_app) : 0u;
    fude_session_saved_revision = fude_session_seen_revision = _app->ink->revision;
    fude_session_saved_view     = fude_session_seen_view     = _app->canvas->view;
    fude_session_saved_page     = fude_session_seen_page     = fude_session_shown_now(_app);
    fude_session_changed_at     = rde_engine_get_time_now();
}

// --- loading ---------------------------------------------------------------------------------

// Notes how the open canvas's page file loaded (_doc), for the HUD, and logs it when the file was damaged: read from
// its backup, or set aside with the page started empty. No load problem shows a notice in the app, so the log is where
// this one is told; it is logged once, as the page loads (at launch, or as another canvas opens).
RDE_INTERNAL void fude_session_page_loaded(const fude_app* _app, FUDE_LOAD_ _doc) {
    if(_doc == FUDE_LOAD_OK) {
        rde_snprintf(fude_session_load_note, sizeof(fude_session_load_note), "loaded %u strokes", fude_ink_alive_strokes(_app->ink));
    } else if(_doc == FUDE_LOAD_RECOVERED) {
        rde_snprintf(fude_session_load_note, sizeof(fude_session_load_note), "PAGE FILE MISSING/DAMAGED - loaded the backup, %u strokes", fude_ink_alive_strokes(_app->ink));
    } else if(_doc == FUDE_LOAD_CORRUPT) {
        rde_snprintf(fude_session_load_note, sizeof(fude_session_load_note), "PAGE FILE DAMAGED - kept as .bad, starting empty");
    } else {
        rde_snprintf(fude_session_load_note, sizeof(fude_session_load_note), "new page");
    }
    if(_doc == FUDE_LOAD_RECOVERED || _doc == FUDE_LOAD_CORRUPT) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "session: canvas %u's page (%s): %s", fude_session_canvas, fude_session_document, fude_session_load_note);
    }
}

// Reads everything at launch, or again after a backup was imported. See the header.
void fude_session_load(fude_app* _app) {
    fude_save_set_folder(fude_app_id(_app));   // On a device, the app's own folder (fude_app_start already set it).
    const c8* _dir = fude_save_dir();
    rde_snprintf(fude_session_settings_path, sizeof(fude_session_settings_path), "%s%s", _dir, FUDE_SAVE_SETTINGS_FILE);

    // The canvases. The first time, the old page.kana becomes the first one.
    fude_notes_load(_app->notes);
    fude_session_canvas = _app->notes->open;
    fude_notes_canvas_path(fude_session_canvas, fude_session_document, sizeof(fude_session_document));

    // The app's own files (a study app's, for example: marks, exams, words, reviews, characters' notes).
    if(fude_app_ext(_app)->session_open != NULL) {
        fude_app_ext(_app)->session_open(_app, _dir);
    }

    fude_settings    _settings = fude_session_gather(_app);
    const FUDE_LOAD_ _loaded   = fude_load_settings(fude_session_settings_path, &_settings);
    if(_loaded == FUDE_LOAD_OK || _loaded == FUDE_LOAD_RECOVERED) {
        fude_session_apply(_app, &_settings);
    } else if(_loaded == FUDE_LOAD_MISSING) {
        if(fude_app_ext(_app)->first_launch != NULL) {
            fude_app_ext(_app)->first_launch(_app);   // Such as a study app's welcome: what it is, and its gestures.
        }
        // The very first time, take the theme the device is in: Night when it is dark, Paper otherwise, as the launch
        // screen was. The settings are saved at once (below), so from then on the theme is the user's.
        fude_theme_set(rde_engine_get_system_theme() == RDE_SYSTEM_THEME_DARK ? FUDE_THEME_NIGHT : FUDE_THEME_PAPER);
        fude_ui_apply_theme(_app->ui);
    }

    fude_session_page_loaded(_app, fude_load_document(fude_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page));
    rde_log_color(RDE_LOG_COLOR_GREEN, "session: saves in %s (%s)", _dir, fude_session_load_note);
    if(fude_session_kind(_app) != NULL) {
        fude_session_kind(_app)->open(_app, fude_session_canvas);
    }

    fude_session_on_screen_is_saved(_app);
    fude_session_settings_seen(_app);
    if(_loaded == FUDE_LOAD_MISSING && !fude_save_settings(fude_session_settings_path, &fude_session_saved_settings)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "session: could not write the first settings (%s)", fude_session_settings_path);
    }
}

// Lets go of the page and empties it, with a reset view and the default page (dots, no page turned). The brush, the
// marker and the eraser are settings, not the page's, and stay as they are (ink.h: fude_ink_clear_page).
RDE_INTERNAL void fude_session_empty_page(fude_app* _app) {
    fude_page_let_go(_app->page);
    fude_canvas_release_fingers(_app->canvas);
    fude_lasso_clear(_app->lasso, _app->ink);
    fude_ink_clear_page(_app->ink);
    fude_canvas_reset_view(_app->canvas);
    fude_page_reset(&_app->canvas->page);    // Until the next page's own loads (a new canvas has none).
    fude_page_zoom_seen(_app->page);         // The loaded zoom is not a change to show.
}

// Opens the canvas chosen in the side panel (notes.open changed). The page being left is saved, unless its canvas was
// just deleted, and the new one is loaded with its own view and a fresh history. The lasso's clipboard carries over,
// so the user can copy on one canvas and paste on another.
RDE_INTERNAL void fude_session_switch_canvas(fude_app* _app) {
    rde_crash_note("canvas switched");   // (for a crash report's notes: crash.h)
    fude_page_let_go(_app->page);
    const fude_page_kind* _kind = fude_session_kind(_app);
    if(fude_notes_find(_app->notes, fude_session_canvas) != NULL) {
        fude_session_save_now(_app, false);
        if(_kind != NULL) {
            _kind->save(_app, true);   // The canvas being left is written whole.
        }
    }
    fude_session_empty_page(_app);
    fude_session_canvas = _app->notes->open;
    fude_notes_canvas_path(fude_session_canvas, fude_session_document, sizeof(fude_session_document));
    fude_session_page_loaded(_app, fude_load_document(fude_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page));
    if(_kind != NULL) {
        _kind->open(_app, fude_session_canvas);
    }
    fude_session_on_screen_is_saved(_app);
}

// After an import has put the backup's files in the save folder, reads everything again from them. Every screen is
// closed first, since each holds what it read before.
RDE_INTERNAL void fude_session_reload(fude_app* _app) {
    fude_app_close_all(_app);
    fude_session_empty_page(_app);
    fude_ink_destroy(_app->ink);
    fude_ink_init(_app->ink);
    fude_session_load(_app);
    _app->ui->side._notes_built = false;   // Rebuild the side panel's list from the new notes.
    fude_ui_apply_theme(_app->ui);
}

// --- Your data ---------------------------------------------------------------------------------

// Writes the backup to _path, shares it when _share, and says on Your data's card how it went.
void fude_session_export_to(fude_app* _app, const c8* _path, b8 _share) {
    fude_backup_info _info;
    c8               _line[512];
    // Write the app's own page whole first, so the backup holds all of it.
    if(fude_session_kind(_app) != NULL) {
        fude_session_kind(_app)->save(_app, true);
    }
    rde_crash_note("data export");   // (for a crash report's notes: crash.h)
    c8 _what[128];
    FUDE_TEXTF(_what, FUDE_TEXT_DATA_BACKUP_NAME, FUDE_TS(_app->info->name));
    if(!fude_backup_export(fude_save_dir(), _path, _app->info->version, &_info) || (_share && !rde_mobile_share_file(_path, "application/octet-stream", _what))) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_share) {
        FUDE_TEXTF(_line, FUDE_TEXT_DATA_EXPORT_READY, FUDE_TN(_info.files));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_DATA_EXPORT_SAVED, FUDE_TN(_info.files), FUDE_TS(_path));
    }
    fude_side_data_message(_app->ui, _line, false);
    rde_log_level(RDE_LOG_LEVEL_INFO, "session: exported %u files (%llu bytes) to %s", _info.files, (unsigned long long)_info.bytes, _path);
}

// Writes _path into _out with _ext added, unless it already ends in _ext or _ext_upper. See the header.
void fude_session_with_extension(c8* _out, usize _size, const c8* _path, const c8* _ext, const c8* _ext_upper) {
    const usize _n   = rde_strlen(_path);
    const usize _e   = rde_strlen(_ext);
    const b8    _has = _n >= _e && (rde_strcmp(_path + _n - _e, _ext) == 0 || (_ext_upper != NULL && rde_strcmp(_path + _n - _e, _ext_upper) == 0));
    rde_snprintf(_out, _size, "%s%s", _path, _has ? "" : _ext);
}

#if !defined(RDE_PLATFORM_MOBILE)
// On a computer, the save dialog's callback (rde_dialog_save_file): exports to the chosen path, with the backup's
// extension added when it was left out. _paths is NULL when the dialog failed, and empty when cancelled.
RDE_INTERNAL void fude_session_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    fude_redraw_request();   // (render on demand: a dialog's answer may come while the app sleeps, with no event)
    if(_paths == NULL) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_count == 0) {
        return;   // Cancelled.
    }
    c8 _path[RDE_MAX_PATH];
    c8 _ext[80];
    rde_snprintf(_ext, sizeof(_ext), ".%s", fude_session_backup_ext(_app));
    fude_session_with_extension(_path, sizeof(_path), _paths[0], _ext, NULL);
    fude_session_export_to(_app, _path, false);
}
#endif

// Reads the file picked to import, checks that it is a whole backup, and asks on Your data's card whether to replace
// everything with it (saying when it was made and how many canvases it holds). Nothing changes until Replace. The
// card keeps the bytes read. The file is read whole: Android hands a picked file over as a content address that RDE
// reads whole (rde_file_read_uri), and the checksum, over every byte, is checked before anything changes. A backup is
// the save folder's own files (no crash report, no downloaded model: backup.h), so it is the size of what the
// learner made.
void fude_session_import_pick(fude_app* _app, const c8* _path) {
    rde_memory_allocator* _a    = rde_memory_allocator_get_default_std();
    usize                 _size = 0;
    u8*                   _data = rde_file_read_uri(_path, &_size, _a);
    if(_data == NULL) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    fude_backup_info _info;
    if(!fude_backup_inspect(_data, _size, &_info)) {
        _a->free(_a->allocator, _data);
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_NOT_BACKUP), true);
        return;
    }
    c8 _when[64];
    c8 _question[512];
    fude_text_date_time(_when, sizeof(_when), _info.created);
    FUDE_TEXTF(_question, FUDE_TEXT_DATA_CONFIRM, FUDE_TS(_when), FUDE_TN(_info.canvases));
    fude_side_data_confirm(_app->ui, _data, _size, _question);
}

// The open dialog's callback: reads the file picked. _paths is NULL when the dialog failed, and empty when cancelled.
RDE_INTERNAL void fude_session_on_import_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    fude_redraw_request();   // (render on demand: a dialog's answer may come while the app sleeps, with no event)
    if(_paths == NULL) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    if(_count > 0) {
        fude_session_import_pick(_app, _paths[0]);
    }
}

// Called for each entry of the outbox: deletes a file an earlier share left there. Returns true to go on.
RDE_INTERNAL b8 fude_session_outbox_entry(const c8* _path, b8 _is_dir, any _user_data) {
    RDE_UNUSED(_user_data);
    if(!_is_dir) {
        rde_file_delete(_path);
    }
    return true;
}

// Empties the outbox and writes into _out the path of a new file there, named "<_stem> <YYYY-MM-DD>.<_ext>" by today's
// local date. Makes the outbox folder when it is missing.
void fude_session_outbox_path(c8* _out, usize _size, const c8* _stem, const c8* _ext) {
    c8 _outbox[RDE_MAX_PATH];
    c8 _date[32];
    const rde_date_components _today = rde_date_local_components(rde_date_now_unix_seconds());
    rde_snprintf(_date, sizeof(_date), "%04d-%02d-%02d", (i32)_today.year, (i32)_today.month, (i32)_today.day);
    rde_snprintf(_outbox, sizeof(_outbox), "%s%s/", fude_save_dir(), FUDE_BACKUP_OUTBOX);
    if(rde_file_dir_exists(_outbox)) {
        rde_file_crawl_dir_recursively(_outbox, fude_session_outbox_entry, NULL, 0, NULL);
    }
    rde_snprintf(_out, _size, "%s%s %s.%s", _outbox, _stem, _date, _ext);
    rde_file_create_missing_dirs(_out);
}

// Returns Your data's file type for the system's dialogs ("<Name> backup", *.<id>backup). It is kept in
// statics, because a dialog reads it until it answers.
RDE_INTERNAL const rde_dialog_filter* fude_session_backup_filter(const fude_app* _app) {
    static c8                _name[128];
    static rde_dialog_filter _filter;
    FUDE_TEXTF(_name, FUDE_TEXT_DATA_BACKUP_NAME, FUDE_TS(_app->info->name));
    _filter = (rde_dialog_filter){ _name, fude_session_backup_ext(_app) };
    return &_filter;
}

// Points *_out at the file types an import opens and returns how many: the app's own backups, and those of the app it
// can take from (info.h: imports, for an app split from another). Kept in statics, like the one above.
RDE_INTERNAL u32 fude_session_import_filters(const fude_app* _app, const rde_dialog_filter** _out) {
    static rde_dialog_filter _filters[2];
    static c8                _name[128], _ext[80];
    _filters[0] = *fude_session_backup_filter(_app);
    *_out = _filters;
    if(_app->info->imports == NULL || _app->info->imports[0] == 0) {
        return 1u;
    }
    FUDE_TEXTF(_name, FUDE_TEXT_DATA_BACKUP_NAME, FUDE_TS(_app->info->imports));
    rde_snprintf(_ext, sizeof(_ext), "%sbackup", _app->info->imports);
    _filters[1] = (rde_dialog_filter){ _name, _ext };
    return 2u;
}

// Called once a frame: does what Your data asked for (export, import, or replace with the backup picked).
RDE_INTERNAL void fude_session_data_update(fude_app* _app) {
    switch(fude_side_take_data_request(_app->ui)) {
        case FUDE_SIDE_DATA_EXPORT: {
            fude_session_save_now(_app, false);   // Write what is on screen to the files first.
#if defined(RDE_PLATFORM_MOBILE)
            // Write it to the outbox, then share it from there.
            c8 _path[RDE_MAX_PATH];
            fude_session_outbox_path(_path, sizeof(_path), fude_session_backup_filter(_app)->name, fude_session_backup_ext(_app));
            fude_session_export_to(_app, _path, true);
#else
            rde_dialog_save_file(_app->window, fude_session_backup_filter(_app), 1, NULL, fude_session_on_export_path, _app);
#endif
        } break;
        case FUDE_SIDE_DATA_IMPORT: {
            const rde_dialog_filter* _filters = NULL;
            const u32 _count = fude_session_import_filters(_app, &_filters);
            rde_dialog_open_file(_app->window, _filters, _count, NULL, false, fude_session_on_import_path, _app);
        } break;
        case FUDE_SIDE_DATA_REPLACE: {
            rde_crash_note("data import");   // (for a crash report's notes: crash.h)
            fude_session_save_now(_app, false);   // So what is set aside is what was on screen.
            fude_side*       _side = &_app->ui->side;
            fude_backup_info _info;
            const b8 _whole = fude_backup_inspect(_side->data_backup, _side->data_backup_size, &_info);
            const b8 _ok    = _whole && fude_backup_restore(_side->data_backup, _side->data_backup_size, fude_save_dir());
            fude_side_data_done(_app->ui);
            if(_ok) {
                fude_session_reload(_app);
                c8 _when[64];
                c8 _line[512];
                fude_text_date_time(_when, sizeof(_when), _info.created);
                FUDE_TEXTF(_line, FUDE_TEXT_DATA_IMPORTED, FUDE_TS(_when));
                fude_side_data_message(_app->ui, _line, false);
                rde_log_level(RDE_LOG_LEVEL_INFO, "session: imported %u files from a backup made %s", _info.files, _when);
            } else {
                fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_IMPORT_FAILED), true);
            }
        } break;
        default: break;
    }
}

// --- each frame ---------------------------------------------------------------------------------

// Runs the session's part of the frame: a canvas switch, Your data's requests, and the autosave.
void fude_session_update(fude_app* _app) {
    fude_crash_session(true);   // On the screen again, so mark the session running (once).
    if(_app->notes->open != fude_session_canvas) {
        fude_session_switch_canvas(_app);   // Chosen, made, or its canvas deleted in the side panel.
    }
    fude_session_data_update(_app);
    fude_session_autosave(_app);
}
