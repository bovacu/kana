#include "drawing/app/session.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/base/save.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/base/backup.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See session.h.
// ===========================================================================

RDE_INTERNAL c8            fude_session_document[RDE_MAX_PATH];   // the open canvas's page
RDE_INTERNAL u32           fude_session_canvas = 0;               // whose page is in the ink
RDE_INTERNAL c8            fude_session_settings_path[RDE_MAX_PATH];
RDE_INTERNAL u8            fude_session_language = 0;             // the saved language (RDE_LANGUAGE_; 0: never chosen, the device's)

// What was last written, and what was last seen (to time the quiet period).
RDE_INTERNAL u32           fude_session_saved_revision = 0;
RDE_INTERNAL fude_view     fude_session_saved_view;
RDE_INTERNAL fude_page     fude_session_saved_page;
RDE_INTERNAL fude_settings fude_session_saved_settings;
RDE_INTERNAL u32           fude_session_seen_revision = 0;
RDE_INTERNAL fude_view     fude_session_seen_view;
RDE_INTERNAL fude_page     fude_session_seen_page;
RDE_INTERNAL fude_settings fude_session_seen_settings;
RDE_INTERNAL f64           fude_session_changed_at = 0.0;

RDE_INTERNAL fude_session_info fude_session_hud = { 0.0, 0.0f, 0u, false, "" };

// Your data's file extension: the app's id and "backup" (Kana: kanabackup).
RDE_INTERNAL const c8* fude_session_backup_ext(const fude_app* _app) {
    static c8 _ext[80];
    snprintf(_ext, sizeof(_ext), "%sbackup", fude_app_id(_app));
    return _ext;
}
RDE_INTERNAL c8                fude_session_load_note[160] = "";

fude_session_info fude_session_info_now(void) {
    fude_session_hud.load_note = fude_session_load_note;
    return fude_session_hud;
}

RDE_INTERNAL b8 fude_session_same_page(fude_page _a, fude_page _b) {
    return _a.paper == _b.paper;
}

RDE_INTERNAL b8 fude_session_same_view(fude_view _a, fude_view _b) {
    return memcmp(&_a, &_b, sizeof(fude_view)) == 0;
}

// --- the settings ----------------------------------------------------------------------------

RDE_INTERNAL fude_settings fude_session_gather(const fude_app* _app) {
    const fude_toolbar* _bar = &_app->ui->bar;
    fude_settings       _s;
    memset(&_s, 0, sizeof(_s));
    _s.tool              = (u8)_bar->tool;
    _s.vertical          = _bar->vertical;
    _s.show_hud          = false;   // not a setting any more (the byte stays in the file)
    _s.brush_scale       = (u8)_app->ink->brush_scale;
    _s.width_mode        = (u8)_app->ink->width_mode;
    _s.color             = _app->ink->color;
    _s.radius            = _app->ink->constant_radius;
    _s.toolbar_center    = _bar->center;
    _s.theme             = (u8)fude_theme_index();
    _s.toolbar_minimized = _bar->minimized;
    _s.paper_size        = (u8)_app->canvas->paper_size;
    // The language, once one other than the device's is chosen (0 until then: the device's).
    _s.language          = fude_session_language != 0 || fude_text_language() != fude_text_default_language() ? (u8)fude_text_language() : 0u;
    _s.finger_writes     = _app->finger_writes;
    _s.pen_ever          = _app->pen_ever;
    if(fude_app_ext(_app)->settings_gather != NULL) {
        fude_app_ext(_app)->settings_gather(_app, &_s);   // the app's own (Kana: ML Kit on or off)
    }
    return _s;
}

RDE_INTERNAL void fude_session_apply(fude_app* _app, const fude_settings* _s) {
    fude_toolbar* _bar = &_app->ui->bar;
    _bar->tool                   = _s->tool == FUDE_TOOL_ERASE ? FUDE_TOOL_ERASE : _s->tool == FUDE_TOOL_LASSO ? FUDE_TOOL_LASSO : FUDE_TOOL_DRAW;
    _app->ink->brush_scale       = _s->brush_scale == FUDE_INK_BRUSH_SCALE_SCREEN ? FUDE_INK_BRUSH_SCALE_SCREEN : FUDE_INK_BRUSH_SCALE_PAGE;
    _app->ink->width_mode        = _s->width_mode == FUDE_INK_WIDTH_MODE_PRESSURE ? FUDE_INK_WIDTH_MODE_PRESSURE : FUDE_INK_WIDTH_MODE_CONSTANT;
    _app->ink->color             = _s->color;
    _app->ink->constant_radius   = rde_math_clamp_f32(_s->radius, FUDE_TOOLBAR_SIZE_MIN, FUDE_TOOLBAR_SIZE_MAX);
    fude_theme_set((FUDE_THEME_)_s->theme);
    if(fude_app_ext(_app)->settings_apply != NULL) {
        fude_app_ext(_app)->settings_apply(_app, _s);
    }
    _app->canvas->paper_size     = _s->paper_size < FUDE_PAPER_SIZE_COUNT ? (FUDE_PAPER_SIZE_)_s->paper_size : FUDE_PAPER_MEDIUM;
    fude_session_language        = _s->language;
    _app->finger_writes          = _s->finger_writes;
    _app->pen_ever               = _s->pen_ever;
    if(_s->language != 0 && (RDE_LANGUAGE_)_s->language != fude_text_language()) {
        fude_text_set_language((RDE_LANGUAGE_)_s->language);   // the UI follows next frame
    }
    fude_toolbar_set_placement(_bar, _s->vertical, _s->toolbar_center, _s->toolbar_minimized);
    fude_ui_apply_theme(_app->ui);   // the theme, and what the tools show
}

void fude_session_settings_seen(fude_app* _app) {
    fude_session_saved_settings = fude_session_seen_settings = fude_session_gather(_app);
}

// --- saving -----------------------------------------------------------------------------------

void fude_session_save_now(fude_app* _app, b8 _force) {
    if(_app->ink->drawing) {
        return;
    }
    const fude_settings _settings = fude_session_gather(_app);
    const b8 _doc_dirty = _force || _app->ink->revision != fude_session_saved_revision || !fude_session_same_view(_app->canvas->view, fude_session_saved_view) ||
                          !fude_session_same_page(_app->canvas->page, fude_session_saved_page);
    const b8 _set_dirty = _force || !fude_settings_equal(&_settings, &fude_session_saved_settings);
    if(!_doc_dirty && !_set_dirty) {
        return;
    }

    const f64 _start = rde_engine_get_time_now();
    b8        _ok    = true;
    if(_doc_dirty) {
        u32 _bytes = 0;
        if(fude_save_document(fude_session_document, _app->ink, _app->canvas->view, _app->canvas->page, &_bytes)) {
            fude_session_saved_revision     = _app->ink->revision;
            fude_session_saved_view         = _app->canvas->view;
            fude_session_saved_page         = _app->canvas->page;
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
    const f64 _now = rde_engine_get_time_now();
    fude_session_hud.last_save_ms   = (f32)((_now - _start) * 1000.0);
    fude_session_hud.last_save_time = _now;
    fude_session_hud.save_failed    = !_ok;
    // A failure retries after another quiet period rather than every frame.
    if(!_ok) {
        fude_session_changed_at = _now;
    }
}

// Once a frame: note any change, and save once things have been quiet a while.
RDE_INTERNAL void fude_session_autosave(fude_app* _app) {
    const f64           _now      = rde_engine_get_time_now();
    const fude_settings _settings = fude_session_gather(_app);
    if(_app->ink->revision != fude_session_seen_revision || !fude_session_same_view(_app->canvas->view, fude_session_seen_view) ||
       !fude_session_same_page(_app->canvas->page, fude_session_seen_page) || !fude_settings_equal(&_settings, &fude_session_seen_settings)) {
        fude_session_seen_revision = _app->ink->revision;
        fude_session_seen_view     = _app->canvas->view;
        fude_session_seen_page     = _app->canvas->page;
        fude_session_seen_settings = _settings;
        fude_session_changed_at    = _now;
        return;
    }
    if(!_app->ink->drawing && !fude_lasso_busy(_app->lasso) && _now - fude_session_changed_at >= FUDE_SESSION_AUTOSAVE_DELAY) {
        fude_session_save_now(_app, false);
    }
}

void fude_session_save_on_exit(fude_app* _app) {
    fude_page_let_go(_app->page);
    fude_session_save_now(_app, false);
}

// What is on screen now IS what is saved.
RDE_INTERNAL void fude_session_on_screen_is_saved(fude_app* _app) {
    fude_session_saved_revision = fude_session_seen_revision = _app->ink->revision;
    fude_session_saved_view     = fude_session_seen_view     = _app->canvas->view;
    fude_session_saved_page     = fude_session_seen_page     = _app->canvas->page;
    fude_session_changed_at     = rde_engine_get_time_now();
}

// --- loading ---------------------------------------------------------------------------------

void fude_session_load(fude_app* _app) {
    fude_save_set_folder(fude_app_id(_app));   // a device's: the app's own folder (fude_app_start has set it already)
    const c8* _dir = fude_save_dir();
    snprintf(fude_session_settings_path, sizeof(fude_session_settings_path), "%s%s", _dir, FUDE_SAVE_SETTINGS_FILE);

    // The canvases (the first time, the old page.kana becomes the first one).
    fude_notes_load(_app->notes);
    fude_session_canvas = _app->notes->open;
    fude_notes_canvas_path(fude_session_canvas, fude_session_document, sizeof(fude_session_document));

    // The app's own files (Kana: marks, exams, words, reviews, characters' notes).
    if(fude_app_ext(_app)->session_open != NULL) {
        fude_app_ext(_app)->session_open(_app, _dir);
    }

    fude_settings    _settings = fude_session_gather(_app);
    const FUDE_LOAD_ _loaded   = fude_load_settings(fude_session_settings_path, &_settings);
    if(_loaded == FUDE_LOAD_OK) {
        fude_session_apply(_app, &_settings);
    } else if(_loaded == FUDE_LOAD_MISSING) {
        if(fude_app_ext(_app)->first_launch != NULL) {
            fude_app_ext(_app)->first_launch(_app);   // the very first time (Kana: what it is, and its gestures)
        }
        // The very first time: the theme the device is in — Night when it is
        // dark, Paper otherwise, as the launch screen was — and the settings
        // saved at once (below), so from now on the theme is the learner's.
        fude_theme_set(rde_engine_get_system_theme() == RDE_SYSTEM_THEME_DARK ? FUDE_THEME_NIGHT : FUDE_THEME_PAPER);
        fude_ui_apply_theme(_app->ui);
    }

    const FUDE_LOAD_ _doc = fude_load_document(fude_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page);
    if(_doc == FUDE_LOAD_OK) {
        snprintf(fude_session_load_note, sizeof(fude_session_load_note), "loaded %u strokes", fude_ink_alive_strokes(_app->ink));
    } else if(_doc == FUDE_LOAD_RECOVERED) {
        snprintf(fude_session_load_note, sizeof(fude_session_load_note), "PAGE FILE MISSING/DAMAGED - loaded the backup, %u strokes", fude_ink_alive_strokes(_app->ink));
    } else if(_doc == FUDE_LOAD_CORRUPT) {
        snprintf(fude_session_load_note, sizeof(fude_session_load_note), "PAGE FILE DAMAGED - kept as .bad, starting empty");
    } else {
        snprintf(fude_session_load_note, sizeof(fude_session_load_note), "new page");
    }
    rde_log_color(RDE_LOG_COLOR_GREEN, "fude: saves in %s (%s)", _dir, fude_session_load_note);

    fude_session_on_screen_is_saved(_app);
    fude_session_settings_seen(_app);
    if(_loaded == FUDE_LOAD_MISSING && !fude_save_settings(fude_session_settings_path, &fude_session_saved_settings)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: could not write the first settings (%s)", fude_session_settings_path);
    }
}

// The page let go and emptied, the brush kept (a setting, not the page's).
RDE_INTERNAL void fude_session_empty_page(fude_app* _app) {
    fude_page_let_go(_app->page);
    fude_canvas_release_fingers(_app->canvas);
    fude_lasso_clear(_app->lasso, _app->ink);
    const FUDE_INK_BRUSH_SCALE_ _scale  = _app->ink->brush_scale;
    const FUDE_INK_WIDTH_MODE_  _width  = _app->ink->width_mode;
    const rde_color             _color  = _app->ink->color;
    const f32                   _radius = _app->ink->constant_radius;
    fude_ink_destroy(_app->ink);
    fude_ink_init(_app->ink);
    _app->ink->brush_scale     = _scale;
    _app->ink->width_mode      = _width;
    _app->ink->color           = _color;
    _app->ink->constant_radius = _radius;
    fude_canvas_reset_view(_app->canvas);
    _app->canvas->page = (fude_page){ 0 };   // the next page's own, or nothing (a new canvas)
    fude_page_zoom_seen(_app->page);         // the loaded zoom is not a change to show
}

// The canvas opened in the side panel (notes.open changed): the page leaving is
// saved — unless it was just deleted — and the new one loaded, with its own view
// and a fresh history. The lasso's clipboard carries over (copy on one canvas,
// paste on another).
RDE_INTERNAL void fude_session_switch_canvas(fude_app* _app) {
    fude_page_let_go(_app->page);
    if(fude_notes_find(_app->notes, fude_session_canvas) != NULL) {
        fude_session_save_now(_app, false);
    }
    fude_session_empty_page(_app);
    fude_session_canvas = _app->notes->open;
    fude_notes_canvas_path(fude_session_canvas, fude_session_document, sizeof(fude_session_document));
    fude_load_document(fude_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page);
    fude_session_on_screen_is_saved(_app);
}

// After an import put the backup's files in the save folder: everything read
// again from them. Every screen is closed first (each holds what it read before).
RDE_INTERNAL void fude_session_reload(fude_app* _app) {
    fude_app_close_all(_app);
    fude_session_empty_page(_app);
    fude_ink_destroy(_app->ink);
    fude_ink_init(_app->ink);
    fude_session_load(_app);
    _app->ui->side._notes_built = false;   // the side panel's list, from the new notes
    fude_ui_apply_theme(_app->ui);
}

// --- Your data ---------------------------------------------------------------------------------

void fude_session_export_to(fude_app* _app, const c8* _path, b8 _share) {
    fude_backup_info _info;
    c8               _line[400];
    c8 _what[64];
    snprintf(_what, sizeof(_what), "%s backup", _app->info->name);
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
    rde_log_level(RDE_LOG_LEVEL_INFO, "fude: exported %u files (%llu bytes) to %s", _info.files, (unsigned long long)_info.bytes, _path);
}

void fude_session_with_extension(c8* _out, usize _size, const c8* _path, const c8* _ext, const c8* _ext_upper) {
    const usize _n   = strlen(_path);
    const usize _e   = strlen(_ext);
    const b8    _has = _n >= _e && (strcmp(_path + _n - _e, _ext) == 0 || (_ext_upper != NULL && strcmp(_path + _n - _e, _ext_upper) == 0));
    snprintf(_out, _size, "%s%s", _path, _has ? "" : _ext);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen (rde_dialog_save_file).
RDE_INTERNAL void fude_session_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    if(_paths == NULL) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8 _path[RDE_MAX_PATH];
    c8 _ext[80];
    snprintf(_ext, sizeof(_ext), ".%s", fude_session_backup_ext(_app));
    fude_session_with_extension(_path, sizeof(_path), _paths[0], _ext, NULL);
    fude_session_export_to(_app, _path, false);
}
#endif

// A file picked to import: read, checked whole, and the question asked —
// nothing changes until Replace.
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
    c8 _question[400];
    fude_text_date_time(_when, sizeof(_when), _info.created);
    FUDE_TEXTF(_question, FUDE_TEXT_DATA_CONFIRM, FUDE_TS(_when), FUDE_TN(_info.canvases));
    fude_side_data_confirm(_app->ui, _data, _size, _question);
}

RDE_INTERNAL void fude_session_on_import_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    if(_paths == NULL) {
        fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    if(_count > 0) {
        fude_session_import_pick(_app, _paths[0]);
    }
}

// Deletes what an earlier share left in the outbox.
RDE_INTERNAL b8 fude_session_outbox_entry(const c8* _path, b8 _is_dir, any _user_data) {
    RDE_UNUSED(_user_data);
    if(!_is_dir) {
        rde_file_delete(_path);
    }
    return true;
}

void fude_session_outbox_path(c8* _out, usize _size, const c8* _stem, const c8* _ext) {
    c8 _outbox[RDE_MAX_PATH];
    c8 _date[32];
    const time_t _now = time(NULL);
    strftime(_date, sizeof(_date), "%Y-%m-%d", localtime(&_now));
    snprintf(_outbox, sizeof(_outbox), "%s%s/", fude_save_dir(), FUDE_BACKUP_OUTBOX);
    if(rde_file_dir_exists(_outbox)) {
        rde_file_crawl_dir_recursively(_outbox, fude_session_outbox_entry, NULL, 0, NULL);
    }
    snprintf(_out, _size, "%s%s %s.%s", _outbox, _stem, _date, _ext);
    rde_file_create_missing_dirs(_out);
}

// Your data's file for the system's dialogs ("Kana backup", *.kanabackup):
// kept, as a dialog reads it until it answers.
RDE_INTERNAL const rde_dialog_filter* fude_session_backup_filter(const fude_app* _app) {
    static c8                _name[64];
    static rde_dialog_filter _filter;
    snprintf(_name, sizeof(_name), "%s backup", _app->info->name);
    _filter = (rde_dialog_filter){ _name, fude_session_backup_ext(_app) };
    return &_filter;
}

// Once a frame: what Your data asked for.
RDE_INTERNAL void fude_session_data_update(fude_app* _app) {
    switch(fude_side_take_data_request(_app->ui)) {
        case FUDE_SIDE_DATA_EXPORT: {
            fude_session_save_now(_app, false);   // what is on screen, in the files first
#if defined(RDE_PLATFORM_MOBILE)
            // Written to the outbox, then shared from there.
            c8 _path[RDE_MAX_PATH];
            fude_session_outbox_path(_path, sizeof(_path), fude_session_backup_filter(_app)->name, fude_session_backup_ext(_app));
            fude_session_export_to(_app, _path, true);
#else
            rde_dialog_save_file(_app->window, fude_session_backup_filter(_app), 1, NULL, fude_session_on_export_path, _app);
#endif
        } break;
        case FUDE_SIDE_DATA_IMPORT: {
            rde_dialog_open_file(_app->window, fude_session_backup_filter(_app), 1, NULL, false, fude_session_on_import_path, _app);
        } break;
        case FUDE_SIDE_DATA_REPLACE: {
            fude_session_save_now(_app, false);   // so what is set aside is what was on screen
            fude_side*       _side = &_app->ui->side;
            fude_backup_info _info;
            const b8 _whole = fude_backup_inspect(_side->data_backup, _side->data_backup_size, &_info);
            const b8 _ok    = _whole && fude_backup_restore(_side->data_backup, _side->data_backup_size, fude_save_dir());
            fude_side_data_done(_app->ui);
            if(_ok) {
                fude_session_reload(_app);
                c8 _when[64];
                c8 _line[400];
                fude_text_date_time(_when, sizeof(_when), _info.created);
                FUDE_TEXTF(_line, FUDE_TEXT_DATA_IMPORTED, FUDE_TS(_when));
                fude_side_data_message(_app->ui, _line, false);
                rde_log_level(RDE_LOG_LEVEL_INFO, "fude: imported %u files from a backup made %s", _info.files, _when);
            } else {
                fude_side_data_message(_app->ui, fude_text(FUDE_TEXT_DATA_IMPORT_FAILED), true);
            }
        } break;
        default: break;
    }
}

// --- each frame ---------------------------------------------------------------------------------

void fude_session_update(fude_app* _app) {
    if(_app->notes->open != fude_session_canvas) {
        fude_session_switch_canvas(_app);   // chosen (or made, or its canvas deleted) in the side panel
    }
    fude_session_data_update(_app);
    fude_session_autosave(_app);
}
