#include "drawing/app/look.h"
#include "drawing/widgets/draw.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/base/save.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/doc/doc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See look.h.
// ===========================================================================

RDE_INTERNAL struct {
    i32       argc;
    c8**      argv;
    // Wanted once the saves are loaded, or some frames in (a shot's sequence).
    const c8* shot;
    u32       frames;
    b8        paper, deselect, trim_fonts, data, data_replace;
    b8        stay;       // --stay: a shot's sequence without the shot, and no quitting (a screenshot taken from outside: the Simulator's)
    i32       theme;
    const c8* note_card;  // --note-card=folder|canvas|rename: a new note's card at frame 15 (in the saves: a look for scratch saves)
    i32       ui_size;    // --ui-size=N: the interface's (FUDE_UI_SIZE_, app.h: 1 Small, 2 Medium, 3 Large; 0 the device's)
    const c8* data_export;
    const c8* data_import;
    const c8* press;      // --press=I,J,...: the top screen's row's buttons pressed in turn, from frame 30
    f32       swipe;      // --swipe=DX: a finger dragged DX across the screen on top (from its middle), frames 30-36
    i32       language;   // --language=N: the app's Nth language (text.h), chosen at frame 12
    const c8* doc;        // --doc=PDF: a new canvas over a copy of it, opened as the saves load
    i32       book;       // --book=N: the app's Nth book (its library), opened as the saves load (-1: no)
    const c8* doc_view;   // --doc-view=ZOOM,X,Y: the view at frame 20 (canvas units at the middle of the screen)
    const c8* mark;       // --mark=X0,Y0,X1,Y1[;...]: marker strokes (canvas units) at frame 24, in the marker's colour
    const c8* lasso_area; // --lasso-area=X0,Y0,X1,Y1: the lasso's loop round that box at frame 22 (a PDF's text: its area)
    const c8* doc_search; // --doc-search=WORDS: the document bar's Search for them, frame 26
    i32       doc_page;   // --doc-page=N: the document bar's page N gone to, frame 26 (0: no)
    const c8* doc_export; // --doc-export=PDF: the document written there with its ink, frame 90
    i32       doc_turn;   // --doc-turn=N: its page N turned a quarter clockwise, frame 30 (0: no)
    // --perf: frame times over this many seconds (0: off).
    f64       perf_seconds, perf_start;
    u32       perf_frames, perf_settle, perf_slow;
    f64       perf_dt_sum, perf_dt_max, perf_update_sum, perf_update_max, perf_render_sum, perf_render_max;
    c8        perf_label[160];
    void    (*perf_note)(struct fude_app* _app, c8* _out, usize _size);
} fude_look = { .theme = -1, .ui_size = -1, .language = -1, .book = -1 };

const c8* fude_look_value(const c8* _arg, const c8* _flag) {
    const usize _n = strlen(_flag);
    return _arg != NULL && strncmp(_arg, _flag, _n) == 0 && _arg[_n] == '=' ? _arg + _n + 1 : NULL;
}

b8 fude_look_is(const c8* _arg, const c8* _flag) {
    return _arg != NULL && strcmp(_arg, _flag) == 0;
}

u32 fude_look_shot_frame(void) {
    return fude_look.shot != NULL || fude_look.stay ? fude_look.frames : 0u;
}

void fude_look_perf_note(void (*_note)(struct fude_app* _app, c8* _out, usize _size)) {
    fude_look.perf_note = _note;
}

void fude_look_args(i32 _argc, c8** _argv) {
    fude_look.argc = _argc;
    fude_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _a = _argv[_i];
        const c8* _v;
        if(fude_look_is(_a, "--paper"))               { fude_look.paper = true; }
        if(fude_look_is(_a, "--deselect"))            { fude_look.deselect = true; }
        if(fude_look_is(_a, "--trim-fonts"))          { fude_look.trim_fonts = true; }
        if(fude_look_is(_a, "--data"))                { fude_look.data = true; }
        if(fude_look_is(_a, "--data-replace"))        { fude_look.data_replace = true; }
        if(fude_look_is(_a, "--stay"))                { fude_look.stay = true; }
        if((_v = fude_look_value(_a, "--theme")) != NULL)          { fude_look.theme = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--ui-size")) != NULL)        { fude_look.ui_size = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--note-card")) != NULL)      { fude_look.note_card = _v; }
        if((_v = fude_look_value(_a, "--shot")) != NULL)           { fude_look.shot = _v; }
        if((_v = fude_look_value(_a, "--data-export")) != NULL)    { fude_look.data_export = _v; }
        if((_v = fude_look_value(_a, "--data-import")) != NULL)    { fude_look.data_import = _v; }
        if((_v = fude_look_value(_a, "--press")) != NULL)          { fude_look.press = _v; }
        if((_v = fude_look_value(_a, "--swipe")) != NULL)          { fude_look.swipe = strtof(_v, NULL); }
        if((_v = fude_look_value(_a, "--language")) != NULL)       { fude_look.language = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--doc")) != NULL)            { fude_look.doc = _v; }
        if((_v = fude_look_value(_a, "--doc-view")) != NULL)       { fude_look.doc_view = _v; }
        if((_v = fude_look_value(_a, "--book")) != NULL)           { fude_look.book = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--mark")) != NULL)           { fude_look.mark = _v; }
        if((_v = fude_look_value(_a, "--lasso-area")) != NULL)     { fude_look.lasso_area = _v; }
        if((_v = fude_look_value(_a, "--doc-search")) != NULL)     { fude_look.doc_search = _v; }
        if((_v = fude_look_value(_a, "--doc-page")) != NULL)       { fude_look.doc_page = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--doc-export")) != NULL)     { fude_look.doc_export = _v; }
        if((_v = fude_look_value(_a, "--doc-turn")) != NULL)       { fude_look.doc_turn = (i32)strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--perf")) != NULL) {
            fude_look.perf_seconds = strtod(_v, NULL);
            for(i32 _k = 1; _k < _argc; _k++) {
                if(_argv[_k] != NULL && strncmp(_argv[_k], "--perf=", 7) != 0 && strlen(fude_look.perf_label) + strlen(_argv[_k]) + 2u < sizeof(fude_look.perf_label)) {
                    strcat(fude_look.perf_label, _argv[_k]);
                    strcat(fude_look.perf_label, " ");
                }
            }
        }
    }
}

void fude_look_start(fude_app* _app) {
    for(i32 _i = 1; _i < fude_look.argc; _i++) {
        const c8* _a = fude_look.argv[_i];
        const c8* _v;
        if(fude_look_is(_a, "--settings"))   { fude_side_open_settings(_app->ui, -1); }
        if(fude_look_is(_a, "--side"))       { _app->ui->side.open = true; }
        if(fude_look_is(_a, "--text-check")) { fude_draw_text_check(_app->window); }
        if((_v = fude_look_value(_a, "--licences")) != NULL)   { fude_side_open_settings(_app->ui, (i32)strtol(_v, NULL, 10)); }
        if((_v = fude_look_value(_a, "--size")) != NULL) {
            c8*       _end = NULL;
            const i32 _w   = (i32)strtol(_v, &_end, 10);
            const i32 _h   = _end != NULL && *_end == 'x' ? (i32)strtol(_end + 1, NULL, 10) : 0;
            if(_w > 0 && _h > 0) {
                // The device's size in its points; the interface's scale on top (app.h).
                rde_window_set_ui_scale(_app->window, 1.0f);
                rde_window_set_size(_app->window, (rde_vec_2I){ _w, _h });
                fude_app_apply_ui_size(_app);
            }
        }
    }
}

void fude_look_loaded(fude_app* _app) {
    if(fude_look.doc != NULL && fude_doc_new_canvas(_app, FUDE_NOTE_DOCUMENT_OWN, fude_look.doc, "Document", 0u) == 0u) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "look: --doc %s: no canvas made", fude_look.doc);
    }
    if(fude_look.book >= 0 && (u32)fude_look.book < fude_app_ext(_app)->library_count) {
        fude_doc_open_book(_app, &fude_app_ext(_app)->library[fude_look.book], fude_text(FUDE_TEXT_LIBRARY_FOLDER));
    }
    if(fude_look.theme >= 0) {
        fude_theme_set((FUDE_THEME_)fude_look.theme);
        fude_ui_apply_theme(_app->ui);
        fude_session_settings_seen(_app);   // a look, not a change to save
    }
    if(fude_look.ui_size >= 0 && fude_look.ui_size < (i32)FUDE_UI_SIZE_COUNT) {
        _app->ui_size = (u8)fude_look.ui_size;
        fude_app_apply_ui_size(_app);
        fude_session_settings_seen(_app);   // a look, not a change to save
    }
}

// The frame the shot is taken at: 45, or after the last press has settled (a
// document: 100, its pages drawn by then).
RDE_INTERNAL u32 fude_look_shot_at(void) {
    if(fude_look.doc != NULL || fude_look.book >= 0) {
        return 100u;
    }
    if(fude_look.mark != NULL) {
        return 60u;
    }
    u32 _presses = 0;
    for(const c8* _p = fude_look.press; _p != NULL && *_p != 0; _p = strchr(_p, ',') != NULL ? strchr(_p, ',') + 1 : NULL) {
        _presses++;
    }
    const u32 _last = 30u + 4u * _presses + 10u;
    return _presses > 0 && _last > 45u ? _last : 45u;
}

void fude_look_frame(fude_app* _app) {
    // --paper: the paper panel, open once the bar has been laid out.
    if(fude_look.paper && fude_look.frames == 20u) {
        fude_toolbar_set_paper_open(&_app->ui->bar, true);
        fude_look.paper = false;
    }
    if(fude_look.shot == NULL && !fude_look.stay) {
        fude_look.frames += fude_look.paper ? 1u : 0u;
        return;
    }
    const u32 _frame = ++fude_look.frames;
    if(_frame == 12u && fude_look.language >= 0 && (u32)fude_look.language < FUDE_TEXT_LANGUAGES) {
        fude_text_set_language(FUDE_TEXT_LANGUAGE_LIST[fude_look.language].language);
    }
    // --press: one button every four frames from frame 30 (the UI catching up between).
    if(fude_look.press != NULL && _frame >= 30u && (_frame - 30u) % 4u == 0u) {
        const c8* _p = fude_look.press;
        for(u32 _k = 0; _k < (_frame - 30u) / 4u && _p != NULL; _k++) {
            _p = strchr(_p, ',');
            _p = _p != NULL ? _p + 1 : NULL;
        }
        if(_p != NULL && *_p != 0 && !fude_ui_press(_app->ui, (u32)strtoul(_p, NULL, 10))) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "look: --press %s: no such button on the screen on top", _p);
        }
    }
    // --swipe: down in the middle, six moves across, up — the screen's own pointer.
    const fude_screen_slot* _top = fude_app_top(_app);
    if(fude_look.swipe != 0.0f && _top != NULL && _frame >= 30u && _frame <= 37u) {
        const f64        _now = rde_engine_get_time_now();
        const rde_vec_2F _at  = { fude_look.swipe * (f32)(_frame - 30u) / 6.0f, 0.0f };
        if(_frame == 30u)      { _top->vt->pointer_down(_top->self, _at, false, _now); }
        else if(_frame <= 36u) { _top->vt->pointer_moved(_top->self, _at, _now); }
        else                   { _top->vt->pointer_up(_top->self, _at, _now); }
    }
    if(_frame == 15u && fude_look.note_card != NULL) {
        const b8  _canvas = strcmp(fude_look.note_card, "canvas") == 0;
        const u32 _note   = fude_notes_add(_app->notes, _canvas ? FUDE_NOTE_CANVAS : FUDE_NOTE_FOLDER, 0u, _canvas ? "Canvas 2" : "Folder 1");
        fude_side_look_card(_app->ui, _note, strcmp(fude_look.note_card, "rename") == 0 ? FUDE_SIDE_CARD_RENAME : FUDE_SIDE_CARD_ACTIONS);
    }
    if(_frame == 15u && fude_look.data) {
        fude_side_open_settings(_app->ui, -1);
        _app->ui->side.data_open = true;
    }
    if(_frame == 20u && fude_look.data_export != NULL) {
        fude_session_export_to(_app, fude_look.data_export, false);
    }
    if(_frame == 22u && fude_look.data_import != NULL) {
        fude_session_import_pick(_app, fude_look.data_import);
    }
    if(_frame == 20u && fude_look.doc_view != NULL) {
        f32 _z = 1.0f, _x = 0.0f, _y = 0.0f;
        if(sscanf(fude_look.doc_view, "%f,%f,%f", &_z, &_x, &_y) >= 1 && _z > 0.0f) {
            _app->canvas->view.zoom   = _z;
            _app->canvas->view.offset = (rde_vec_2F){ -_x * _z, -_y * _z };
        }
    }
    if(_frame == 24u && fude_look.mark != NULL) {
        // Each along a line of text, as a slow hand marks it: many samples close
        // together, a little unsteady.
        for(const c8* _m = fude_look.mark; _m != NULL && *_m != 0; _m = strchr(_m, ';') != NULL ? strchr(_m, ';') + 1 : NULL) {
            f32 _x0, _y0, _x1, _y1;
            if(sscanf(_m, "%f,%f,%f,%f", &_x0, &_y0, &_x1, &_y1) != 4) {
                continue;
            }
            _app->ink->marking = true;
            for(u32 _k = 0; _k <= 120u; _k++) {
                const f32        _t = (f32)_k / 120.0f;
                const f32        _w = 1.2f * sinf((f32)_k * 2.3f) + 0.8f * sinf((f32)_k * 5.1f);   // the hand's tremor
                const rde_vec_2F _p = { _x0 + (_x1 - _x0) * _t + 0.6f * _w, _y0 + (_y1 - _y0) * _t + _w };
                if(_k == 0u) { fude_ink_begin(_app->ink, _p, true, false); }
                else         { fude_ink_extend(_app->ink, _p); }
            }
            fude_ink_end(_app->ink);
            _app->ink->marking = false;
        }
    }
    if(_frame == 22u && fude_look.lasso_area != NULL) {
        f32 _x0, _y0, _x1, _y1;
        if(sscanf(fude_look.lasso_area, "%f,%f,%f,%f", &_x0, &_y0, &_x1, &_y1) == 4) {
            fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
            const rde_vec_2F _round[5] = { { _x0, _y0 }, { _x1, _y0 }, { _x1, _y1 }, { _x0, _y1 }, { _x0, _y0 } };
            fude_lasso_pen_down(_app->lasso, _app->ink, _round[0], _app->canvas->view.zoom);
            for(u32 _k = 1; _k < 5u; _k++) {
                fude_lasso_pen_moved(_app->lasso, _app->ink, _round[_k], _app->canvas->view.zoom);
            }
            fude_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
            fude_page_lasso_text(_app->page);
        }
    }
    if(_frame == 26u && fude_look.doc_search != NULL) {
        fude_docbar_search(&_app->ui->docbar, fude_look.doc_search);
    }
    if(_frame == 30u && fude_look.doc_turn > 0) {
        fude_doc_turn_page(&_app->page->doc, _app, (u32)(fude_look.doc_turn - 1));
    }
    if(_frame == 90u && fude_look.doc_export != NULL) {
        rde_log_level(RDE_LOG_LEVEL_INFO, "look: --doc-export %s: %s", fude_look.doc_export,
                      fude_doc_export(&_app->page->doc, _app->ink, fude_look.doc_export) ? "written" : "NOT written");
    }
    if(_frame == 26u && fude_look.doc_page > 0) {
        fude_doc_go_to_page(&_app->page->doc, _app->canvas, (u32)(fude_look.doc_page - 1));
    }
    if(_frame == 25u && fude_look.deselect) {
        fude_lasso_clear(_app->lasso, _app->ink);
    }
    if(_frame == 26u && fude_look.data_replace) {
        _app->ui->side.data_request = FUDE_SIDE_DATA_REPLACE;
    }
    if(_frame == 40u && fude_look.trim_fonts) {
        fude_ui_trim_fonts(_app->ui);   // the shot then shows every glyph uploaded again
    }
    if(fude_look.shot == NULL) {
        return;   // --stay: the app carries on
    }
    if(_frame == fude_look_shot_at()) {
        rde_window_take_screenshot(_app->window, (rde_vec_2I){ 0, 0 }, (rde_vec_2I){ 0, 0 }, fude_look.shot, NULL);   // the next frame, whole
    } else if(_frame == fude_look_shot_at() + 5u) {
        rde_engine_set_running(false);
    }
}

// --- --perf ---------------------------------------------------------------------------------

// The time is up: the summary, once.
RDE_INTERNAL void fude_look_perf_write(fude_app* _app, f64 _now) {
    const f64 _n    = fude_look.perf_frames > 0 ? (f64)fude_look.perf_frames : 1.0;
    const f64 _span = _now - fude_look.perf_start;
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%sperf.txt", fude_save_dir());
    FILE* _f = fopen(_path, "a");
    if(_f != NULL) {
        fprintf(_f, "%s| %u frames in %.1f s: %.1f fps, frame %.2f ms avg, %.2f ms worst, %u over 20 ms | update %.2f ms avg, %.2f worst | render %.2f ms avg, %.2f worst",
                fude_look.perf_label, fude_look.perf_frames, _span, _n / _span, 1000.0 * fude_look.perf_dt_sum / _n, 1000.0 * fude_look.perf_dt_max, fude_look.perf_slow,
                1000.0 * fude_look.perf_update_sum / _n, 1000.0 * fude_look.perf_update_max, 1000.0 * fude_look.perf_render_sum / _n, 1000.0 * fude_look.perf_render_max);
        if(fude_look.perf_note != NULL) {   // the app's own (Kana: the camera's frames)
            c8 _note[256] = "";
            fude_look.perf_note(_app, _note, sizeof(_note));
            fprintf(_f, "%s", _note);
        }
        fprintf(_f, "\n");
        fclose(_f);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "perf: %u frames, %.2f ms avg, %.2f ms worst (written to %s)", fude_look.perf_frames,
                  1000.0 * fude_look.perf_dt_sum / _n, 1000.0 * fude_look.perf_dt_max, _path);
    fude_look.perf_seconds = 0.0;
}

void fude_look_timed_update(void (*_update)(f32), f32 _dt) {
    if(fude_look.perf_seconds <= 0.0) {
        _update(_dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    if(fude_look.perf_start <= 0.0) {
        fude_look.perf_start = fude_look.perf_settle++ >= 60u ? _t0 : 0.0;   // a second to settle first
    } else {
        fude_look.perf_frames++;
        fude_look.perf_dt_sum += (f64)_dt;
        fude_look.perf_dt_max  = (f64)_dt > fude_look.perf_dt_max ? (f64)_dt : fude_look.perf_dt_max;
        fude_look.perf_slow   += _dt > 0.020f ? 1u : 0u;
    }
    _update(_dt);
    if(fude_look.perf_start > 0.0 && fude_look.perf_frames > 0) {
        const f64 _t = rde_engine_get_time_now() - _t0;
        fude_look.perf_update_sum += _t;
        fude_look.perf_update_max  = _t > fude_look.perf_update_max ? _t : fude_look.perf_update_max;
    }
}

void fude_look_timed_render(fude_app* _app, void (*_render)(rde_window*, f32), rde_window* _window, f32 _dt) {
    if(fude_look.perf_seconds <= 0.0) {
        _render(_window, _dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    _render(_window, _dt);
    if(fude_look.perf_start > 0.0 && fude_look.perf_frames > 0) {
        const f64 _now = rde_engine_get_time_now();
        fude_look.perf_render_sum += _now - _t0;
        fude_look.perf_render_max  = _now - _t0 > fude_look.perf_render_max ? _now - _t0 : fude_look.perf_render_max;
        if(_now - fude_look.perf_start >= fude_look.perf_seconds) {
            fude_look_perf_write(_app, _now);
        }
    }
}
