#include "app/session.h"
#include "app/app.h"
#include "app/ui.h"
#include "app/page.h"
#include "base/save.h"
#include "base/theme.h"
#include "base/text.h"
#include "widgets/notice.h"
#include "services/mlkit.h"
#include "study/marks.h"
#include "study/examlog.h"
#include "study/vocab.h"
#include "study/review.h"
#include "study/charnote.h"
#include "base/backup.h"
#include "study/sheet.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See session.h.
// ===========================================================================

RDE_INTERNAL c8            kana_session_document[RDE_MAX_PATH];   // the open canvas's page
RDE_INTERNAL u32           kana_session_canvas = 0;               // whose page is in the ink
RDE_INTERNAL c8            kana_session_settings_path[RDE_MAX_PATH];
RDE_INTERNAL u8            kana_session_language = 0;             // the saved language (RDE_LANGUAGE_; 0: never chosen, the device's)

// What was last written, and what was last seen (to time the quiet period).
RDE_INTERNAL u32           kana_session_saved_revision = 0;
RDE_INTERNAL kana_view     kana_session_saved_view;
RDE_INTERNAL kana_page     kana_session_saved_page;
RDE_INTERNAL kana_settings kana_session_saved_settings;
RDE_INTERNAL u32           kana_session_seen_revision = 0;
RDE_INTERNAL kana_view     kana_session_seen_view;
RDE_INTERNAL kana_page     kana_session_seen_page;
RDE_INTERNAL kana_settings kana_session_seen_settings;
RDE_INTERNAL f64           kana_session_changed_at = 0.0;

RDE_INTERNAL kana_session_info kana_session_hud = { 0.0, 0.0f, 0u, false, "" };
RDE_INTERNAL c8                kana_session_load_note[160] = "";

kana_session_info kana_session_info_now(void) {
    kana_session_hud.load_note = kana_session_load_note;
    return kana_session_hud;
}

RDE_INTERNAL b8 kana_session_same_page(kana_page _a, kana_page _b) {
    return _a.paper == _b.paper;
}

RDE_INTERNAL b8 kana_session_same_view(kana_view _a, kana_view _b) {
    return memcmp(&_a, &_b, sizeof(kana_view)) == 0;
}

// --- the settings ----------------------------------------------------------------------------

RDE_INTERNAL kana_settings kana_session_gather(const kana_app* _app) {
    const kana_toolbar* _bar = &_app->ui->bar;
    kana_settings       _s;
    memset(&_s, 0, sizeof(_s));
    _s.tool              = (u8)_bar->tool;
    _s.vertical          = _bar->vertical;
    _s.show_hud          = false;   // not a setting any more (the byte stays in the file)
    _s.brush_scale       = (u8)_app->ink->brush_scale;
    _s.width_mode        = (u8)_app->ink->width_mode;
    _s.color             = _app->ink->color;
    _s.radius            = _app->ink->constant_radius;
    _s.toolbar_center    = _bar->center;
    _s.theme             = (u8)kana_theme_index();
    _s.mlkit             = kana_mlkit_enabled();
    _s.toolbar_minimized = _bar->minimized;
    _s.paper_size        = (u8)_app->canvas->paper_size;
    // The language, once one other than the device's is chosen (0 until then: the device's).
    _s.language          = kana_session_language != 0 || kana_text_language() != kana_text_default_language() ? (u8)kana_text_language() : 0u;
    _s.finger_writes     = _app->finger_writes;
    _s.pen_ever          = _app->pen_ever;
    return _s;
}

RDE_INTERNAL void kana_session_apply(kana_app* _app, const kana_settings* _s) {
    kana_toolbar* _bar = &_app->ui->bar;
    _bar->tool                   = _s->tool == KANA_TOOL_ERASE ? KANA_TOOL_ERASE : _s->tool == KANA_TOOL_LASSO ? KANA_TOOL_LASSO : KANA_TOOL_DRAW;
    _app->ink->brush_scale       = _s->brush_scale == KANA_INK_BRUSH_SCALE_SCREEN ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    _app->ink->width_mode        = _s->width_mode == KANA_INK_WIDTH_MODE_PRESSURE ? KANA_INK_WIDTH_MODE_PRESSURE : KANA_INK_WIDTH_MODE_CONSTANT;
    _app->ink->color             = _s->color;
    _app->ink->constant_radius   = rde_math_clamp_f32(_s->radius, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
    kana_theme_set((KANA_THEME_)_s->theme);
    kana_mlkit_set_enabled(_s->mlkit);
    _app->canvas->paper_size     = _s->paper_size < KANA_PAPER_SIZE_COUNT ? (KANA_PAPER_SIZE_)_s->paper_size : KANA_PAPER_MEDIUM;
    kana_session_language        = _s->language;
    _app->finger_writes          = _s->finger_writes;
    _app->pen_ever               = _s->pen_ever;
    if(_s->language != 0 && (RDE_LANGUAGE_)_s->language != kana_text_language()) {
        kana_text_set_language((RDE_LANGUAGE_)_s->language);   // the UI follows next frame
    }
    kana_toolbar_set_placement(_bar, _s->vertical, _s->toolbar_center, _s->toolbar_minimized);
    kana_ui_apply_theme(_app->ui);   // the theme, and what the tools show
}

void kana_session_settings_seen(kana_app* _app) {
    kana_session_saved_settings = kana_session_seen_settings = kana_session_gather(_app);
}

// --- saving -----------------------------------------------------------------------------------

void kana_session_save_now(kana_app* _app, b8 _force) {
    if(_app->ink->drawing) {
        return;
    }
    const kana_settings _settings = kana_session_gather(_app);
    const b8 _doc_dirty = _force || _app->ink->revision != kana_session_saved_revision || !kana_session_same_view(_app->canvas->view, kana_session_saved_view) ||
                          !kana_session_same_page(_app->canvas->page, kana_session_saved_page);
    const b8 _set_dirty = _force || !kana_settings_equal(&_settings, &kana_session_saved_settings);
    if(!_doc_dirty && !_set_dirty) {
        return;
    }

    const f64 _start = rde_engine_get_time_now();
    b8        _ok    = true;
    if(_doc_dirty) {
        u32 _bytes = 0;
        if(kana_save_document(kana_session_document, _app->ink, _app->canvas->view, _app->canvas->page, &_bytes)) {
            kana_session_saved_revision     = _app->ink->revision;
            kana_session_saved_view         = _app->canvas->view;
            kana_session_saved_page         = _app->canvas->page;
            kana_session_hud.last_save_bytes = _bytes;
        } else {
            _ok = false;
        }
    }
    if(_set_dirty) {
        if(kana_save_settings(kana_session_settings_path, &_settings)) {
            kana_session_saved_settings = _settings;
        } else {
            _ok = false;
        }
    }
    const f64 _now = rde_engine_get_time_now();
    kana_session_hud.last_save_ms   = (f32)((_now - _start) * 1000.0);
    kana_session_hud.last_save_time = _now;
    kana_session_hud.save_failed    = !_ok;
    // A failure retries after another quiet period rather than every frame.
    if(!_ok) {
        kana_session_changed_at = _now;
    }
}

// Once a frame: note any change, and save once things have been quiet a while.
RDE_INTERNAL void kana_session_autosave(kana_app* _app) {
    const f64           _now      = rde_engine_get_time_now();
    const kana_settings _settings = kana_session_gather(_app);
    if(_app->ink->revision != kana_session_seen_revision || !kana_session_same_view(_app->canvas->view, kana_session_seen_view) ||
       !kana_session_same_page(_app->canvas->page, kana_session_seen_page) || !kana_settings_equal(&_settings, &kana_session_seen_settings)) {
        kana_session_seen_revision = _app->ink->revision;
        kana_session_seen_view     = _app->canvas->view;
        kana_session_seen_page     = _app->canvas->page;
        kana_session_seen_settings = _settings;
        kana_session_changed_at    = _now;
        return;
    }
    if(!_app->ink->drawing && !kana_lasso_busy(_app->lasso) && _now - kana_session_changed_at >= KANA_SESSION_AUTOSAVE_DELAY) {
        kana_session_save_now(_app, false);
    }
}

void kana_session_save_on_exit(kana_app* _app) {
    kana_page_let_go(_app->page);
    kana_session_save_now(_app, false);
}

// What is on screen now IS what is saved.
RDE_INTERNAL void kana_session_on_screen_is_saved(kana_app* _app) {
    kana_session_saved_revision = kana_session_seen_revision = _app->ink->revision;
    kana_session_saved_view     = kana_session_seen_view     = _app->canvas->view;
    kana_session_saved_page     = kana_session_seen_page     = _app->canvas->page;
    kana_session_changed_at     = rde_engine_get_time_now();
}

// --- loading ---------------------------------------------------------------------------------

void kana_session_load(kana_app* _app) {
    const c8* _dir = kana_save_dir();
    snprintf(kana_session_settings_path, sizeof(kana_session_settings_path), "%s%s", _dir, KANA_SAVE_SETTINGS_FILE);

    // The canvases (the first time, the old page.kana becomes the first one).
    kana_notes_load(_app->notes);
    kana_session_canvas = _app->notes->open;
    kana_notes_canvas_path(kana_session_canvas, kana_session_document, sizeof(kana_session_document));

    // The study files: marks, exams, words, reviews, characters' notes.
    static const struct { const c8* name; void (*open)(const c8*); } _files[] = {
        { "marks.kana", kana_marks_open }, { "exams.kana", kana_examlog_open }, { "words.kana", kana_vocab_open },
        { "reviews.kana", kana_reviews_open }, { "charnotes.kana", kana_charnotes_open },
    };
    for(u32 _i = 0; _i < sizeof(_files) / sizeof(_files[0]); _i++) {
        c8 _path[RDE_MAX_PATH];
        snprintf(_path, sizeof(_path), "%s%s", _dir, _files[_i].name);
        _files[_i].open(_path);
    }

    kana_settings    _settings = kana_session_gather(_app);
    const KANA_LOAD_ _loaded   = kana_load_settings(kana_session_settings_path, &_settings);
    if(_loaded == KANA_LOAD_OK) {
        kana_session_apply(_app, &_settings);
    } else if(_loaded == KANA_LOAD_MISSING) {
        kana_welcome_open(_app->welcome);   // the very first time: what Kana is, and its gestures
        // The very first time: the theme the device is in — Night when it is
        // dark, Paper otherwise, as the launch screen was — and the settings
        // saved at once (below), so from now on the theme is the learner's.
        kana_theme_set(rde_engine_get_system_theme() == RDE_SYSTEM_THEME_DARK ? KANA_THEME_NIGHT : KANA_THEME_PAPER);
        kana_ui_apply_theme(_app->ui);
    }

    const KANA_LOAD_ _doc = kana_load_document(kana_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page);
    if(_doc == KANA_LOAD_OK) {
        snprintf(kana_session_load_note, sizeof(kana_session_load_note), "loaded %u strokes", kana_ink_alive_strokes(_app->ink));
    } else if(_doc == KANA_LOAD_RECOVERED) {
        snprintf(kana_session_load_note, sizeof(kana_session_load_note), "PAGE FILE MISSING/DAMAGED - loaded the backup, %u strokes", kana_ink_alive_strokes(_app->ink));
    } else if(_doc == KANA_LOAD_CORRUPT) {
        snprintf(kana_session_load_note, sizeof(kana_session_load_note), "PAGE FILE DAMAGED - kept as .bad, starting empty");
    } else {
        snprintf(kana_session_load_note, sizeof(kana_session_load_note), "new page");
    }
    rde_log_color(RDE_LOG_COLOR_GREEN, "kana: saves in %s (%s)", _dir, kana_session_load_note);

    kana_session_on_screen_is_saved(_app);
    kana_session_settings_seen(_app);
    if(_loaded == KANA_LOAD_MISSING && !kana_save_settings(kana_session_settings_path, &kana_session_saved_settings)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not write the first settings (%s)", kana_session_settings_path);
    }
}

// The page let go and emptied, the brush kept (a setting, not the page's).
RDE_INTERNAL void kana_session_empty_page(kana_app* _app) {
    kana_page_let_go(_app->page);
    kana_canvas_release_fingers(_app->canvas);
    kana_lasso_clear(_app->lasso, _app->ink);
    const KANA_INK_BRUSH_SCALE_ _scale  = _app->ink->brush_scale;
    const KANA_INK_WIDTH_MODE_  _width  = _app->ink->width_mode;
    const rde_color             _color  = _app->ink->color;
    const f32                   _radius = _app->ink->constant_radius;
    kana_ink_destroy(_app->ink);
    kana_ink_init(_app->ink);
    _app->ink->brush_scale     = _scale;
    _app->ink->width_mode      = _width;
    _app->ink->color           = _color;
    _app->ink->constant_radius = _radius;
    kana_canvas_reset_view(_app->canvas);
    _app->canvas->page = (kana_page){ 0 };   // the next page's own, or nothing (a new canvas)
    kana_page_zoom_seen(_app->page);         // the loaded zoom is not a change to show
}

// The canvas opened in the side panel (notes.open changed): the page leaving is
// saved — unless it was just deleted — and the new one loaded, with its own view
// and a fresh history. The lasso's clipboard carries over (copy on one canvas,
// paste on another).
RDE_INTERNAL void kana_session_switch_canvas(kana_app* _app) {
    kana_page_let_go(_app->page);
    if(kana_notes_find(_app->notes, kana_session_canvas) != NULL) {
        kana_session_save_now(_app, false);
    }
    kana_session_empty_page(_app);
    kana_session_canvas = _app->notes->open;
    kana_notes_canvas_path(kana_session_canvas, kana_session_document, sizeof(kana_session_document));
    kana_load_document(kana_session_document, _app->ink, &_app->canvas->view, &_app->canvas->page);
    kana_session_on_screen_is_saved(_app);
}

// After an import put the backup's files in the save folder: everything read
// again from them. Every screen is closed first (each holds what it read before).
RDE_INTERNAL void kana_session_reload(kana_app* _app) {
    kana_app_close_all(_app);
    kana_session_empty_page(_app);
    kana_ink_destroy(_app->ink);
    kana_ink_init(_app->ink);
    kana_session_load(_app);
    _app->ui->side._notes_built = false;   // the side panel's list, from the new notes
    kana_ui_apply_theme(_app->ui);
}

// --- Your data ---------------------------------------------------------------------------------

void kana_session_export_to(kana_app* _app, const c8* _path, b8 _share) {
    kana_backup_info _info;
    c8               _line[400];
    if(!kana_backup_export(kana_save_dir(), _path, &_info) || (_share && !rde_mobile_share_file(_path, "application/octet-stream", "Kana backup"))) {
        kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_share) {
        KANA_TEXTF(_line, KANA_TEXT_DATA_EXPORT_READY, KANA_TN(_info.files));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_DATA_EXPORT_SAVED, KANA_TN(_info.files), KANA_TS(_path));
    }
    kana_side_data_message(_app->ui, _line, false);
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana: exported %u files (%llu bytes) to %s", _info.files, (unsigned long long)_info.bytes, _path);
}

// A chosen path with _ext added when the learner left it out.
RDE_INTERNAL void kana_session_with_extension(c8* _out, usize _size, const c8* _path, const c8* _ext, const c8* _ext_upper) {
    const usize _n   = strlen(_path);
    const usize _e   = strlen(_ext);
    const b8    _has = _n >= _e && (strcmp(_path + _n - _e, _ext) == 0 || (_ext_upper != NULL && strcmp(_path + _n - _e, _ext_upper) == 0));
    snprintf(_out, _size, "%s%s", _path, _has ? "" : _ext);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen (rde_dialog_save_file).
RDE_INTERNAL void kana_session_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    kana_app* _app = (kana_app*)_user_data;
    if(_paths == NULL) {
        kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8 _path[RDE_MAX_PATH];
    kana_session_with_extension(_path, sizeof(_path), _paths[0], "." KANA_BACKUP_EXTENSION, NULL);
    kana_session_export_to(_app, _path, false);
}
#endif

// A file picked to import: read, checked whole, and the question asked —
// nothing changes until Replace.
void kana_session_import_pick(kana_app* _app, const c8* _path) {
    rde_memory_allocator* _a    = rde_memory_allocator_get_default_std();
    usize                 _size = 0;
    u8*                   _data = rde_file_read_uri(_path, &_size, _a);
    if(_data == NULL) {
        kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    kana_backup_info _info;
    if(!kana_backup_inspect(_data, _size, &_info)) {
        _a->free(_a->allocator, _data);
        kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_NOT_BACKUP), true);
        return;
    }
    c8 _when[64];
    c8 _question[400];
    kana_text_date_time(_when, sizeof(_when), _info.created);
    KANA_TEXTF(_question, KANA_TEXT_DATA_CONFIRM, KANA_TS(_when), KANA_TN(_info.canvases));
    kana_side_data_confirm(_app->ui, _data, _size, _question);
}

RDE_INTERNAL void kana_session_on_import_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    kana_app* _app = (kana_app*)_user_data;
    if(_paths == NULL) {
        kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    if(_count > 0) {
        kana_session_import_pick(_app, _paths[0]);
    }
}

#if defined(RDE_PLATFORM_MOBILE)
// Deletes what an earlier share left in the outbox.
RDE_INTERNAL b8 kana_session_outbox_entry(const c8* _path, b8 _is_dir, any _user_data) {
    RDE_UNUSED(_user_data);
    if(!_is_dir) {
        rde_file_delete(_path);
    }
    return true;
}

// A file to share goes in the outbox (what an earlier one left there let go):
// its path, _name in it, _date's day in its name ("Kana backup 2026-10-01.kanabackup").
RDE_INTERNAL void kana_session_outbox_path(c8* _out, usize _size, const c8* _stem, const c8* _ext) {
    c8 _outbox[RDE_MAX_PATH];
    c8 _date[32];
    const time_t _now = time(NULL);
    strftime(_date, sizeof(_date), "%Y-%m-%d", localtime(&_now));
    snprintf(_outbox, sizeof(_outbox), "%s%s/", kana_save_dir(), KANA_BACKUP_OUTBOX);
    if(rde_file_dir_exists(_outbox)) {
        rde_file_crawl_dir_recursively(_outbox, kana_session_outbox_entry, NULL, 0, NULL);
    }
    snprintf(_out, _size, "%s%s %s.%s", _outbox, _stem, _date, _ext);
    rde_file_create_missing_dirs(_out);
}
#endif

// Once a frame: what Your data asked for.
RDE_INTERNAL void kana_session_data_update(kana_app* _app) {
    switch(kana_side_take_data_request(_app->ui)) {
        case KANA_SIDE_DATA_EXPORT: {
            kana_session_save_now(_app, false);   // what is on screen, in the files first
#if defined(RDE_PLATFORM_MOBILE)
            // Written to the outbox, then shared from there.
            c8 _path[RDE_MAX_PATH];
            kana_session_outbox_path(_path, sizeof(_path), "Kana backup", KANA_BACKUP_EXTENSION);
            kana_session_export_to(_app, _path, true);
#else
            static const rde_dialog_filter _filter = { "Kana backup", KANA_BACKUP_EXTENSION };
            rde_dialog_save_file(_app->window, &_filter, 1, NULL, kana_session_on_export_path, _app);
#endif
        } break;
        case KANA_SIDE_DATA_IMPORT: {
            static const rde_dialog_filter _filter = { "Kana backup", KANA_BACKUP_EXTENSION };
            rde_dialog_open_file(_app->window, &_filter, 1, NULL, false, kana_session_on_import_path, _app);
        } break;
        case KANA_SIDE_DATA_REPLACE: {
            kana_session_save_now(_app, false);   // so what is set aside is what was on screen
            kana_side*       _side = &_app->ui->side;
            kana_backup_info _info;
            const b8 _whole = kana_backup_inspect(_side->data_backup, _side->data_backup_size, &_info);
            const b8 _ok    = _whole && kana_backup_restore(_side->data_backup, _side->data_backup_size, kana_save_dir());
            kana_side_data_done(_app->ui);
            if(_ok) {
                kana_session_reload(_app);
                c8 _when[64];
                c8 _line[400];
                kana_text_date_time(_when, sizeof(_when), _info.created);
                KANA_TEXTF(_line, KANA_TEXT_DATA_IMPORTED, KANA_TS(_when));
                kana_side_data_message(_app->ui, _line, false);
                rde_log_level(RDE_LOG_LEVEL_INFO, "kana: imported %u files from a backup made %s", _info.files, _when);
            } else {
                kana_side_data_message(_app->ui, kana_text(KANA_TEXT_DATA_IMPORT_FAILED), true);
            }
        } break;
        default: break;
    }
}

// --- practice sheets -----------------------------------------------------------------------------

void kana_session_sheet(kana_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share) {
    kana_sheet_info _info;
    if(!kana_sheet_write(_app->db, _records, _count, _path, &_info) || (_share && !rde_mobile_share_file(_path, "application/pdf", kana_text(KANA_TEXT_SHEET_TITLE)))) {
        kana_notice_show(kana_text(KANA_TEXT_SHEET_FAILED));
        return;
    }
    c8 _line[400];
    if(_cut) {
        KANA_TEXTF(_line, KANA_TEXT_SHEET_FIRST_N, KANA_TN(KANA_SHEET_MAX));
        kana_notice_show(_line);
    } else if(!_share) {
        KANA_TEXTF(_line, KANA_TEXT_SHEET_SAVED, KANA_TS(_path));
        kana_notice_show(_line);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana: practice sheet, %u characters on %u pages (%u bytes): %s", _info.characters, _info.pages, _info.bytes, _path);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen; the sheet asked for kept till then.
RDE_INTERNAL u32 kana_session_sheet_records[KANA_SHEET_MAX];
RDE_INTERNAL u32 kana_session_sheet_count;
RDE_INTERNAL b8  kana_session_sheet_cut;

RDE_INTERNAL void kana_session_on_sheet_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    kana_app* _app = (kana_app*)_user_data;
    if(_paths == NULL) {
        kana_notice_show(kana_text(KANA_TEXT_SHEET_FAILED));
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8 _path[RDE_MAX_PATH];
    kana_session_with_extension(_path, sizeof(_path), _paths[0], ".pdf", ".PDF");
    kana_session_sheet(_app, kana_session_sheet_records, kana_session_sheet_count, kana_session_sheet_cut, _path, false);
}
#endif

// Once a frame: a sheet asked for (kana_app_sheet) — shared, or (the desktop)
// where to save it asked.
RDE_INTERNAL void kana_session_sheet_update(kana_app* _app) {
    const u32 _count = _app->sheet_count;
    if(_count == 0u) {
        return;
    }
    _app->sheet_count = 0u;
    const b8  _cut  = _count > KANA_SHEET_MAX;
    const u32 _keep = _cut ? KANA_SHEET_MAX : _count;
#if defined(RDE_PLATFORM_MOBILE)
    c8 _path[RDE_MAX_PATH];
    kana_session_outbox_path(_path, sizeof(_path), kana_text(KANA_TEXT_SHEET_TITLE), "pdf");
    kana_session_sheet(_app, _app->sheet, _keep, _cut, _path, true);
#else
    memcpy(kana_session_sheet_records, _app->sheet, sizeof(u32) * _keep);
    kana_session_sheet_count = _keep;
    kana_session_sheet_cut   = _cut;
    static const rde_dialog_filter _filter = { "PDF", "pdf" };
    rde_dialog_save_file(_app->window, &_filter, 1, NULL, kana_session_on_sheet_path, _app);
#endif
}

// --- each frame ---------------------------------------------------------------------------------

void kana_session_update(kana_app* _app) {
    if(_app->notes->open != kana_session_canvas) {
        kana_session_switch_canvas(_app);   // chosen (or made, or its canvas deleted) in the side panel
    }
    kana_session_data_update(_app);
    kana_session_sheet_update(_app);
    kana_session_autosave(_app);
}
