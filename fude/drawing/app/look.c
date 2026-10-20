// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/look.h"
#include "drawing/base/draw.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/base/clock.h"
#include "drawing/base/redraw.h"
#include "drawing/base/save.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/doc/doc.h"
#include "drawing/app/documents.h"

// ===========================================================================
// See look.h.
//
// fude_look_args reads every core flag into the one fude_look below. What each flag asks for is then done at a fixed
// frame of the shot's sequence (fude_look_frame), chosen so that what it needs is built and laid out by then and so
// that flags given together happen in a sensible order: the language at frame 12, a note's card and Data at 15, the
// paper panel, the view and a data export at 20, a data import and the lasso at 22, the marker at 24, deselecting at
// 25, search, a page and Data's Replace at 26, presses, a swipe and a page turn from 30, trimming the fonts at 40, and
// the document's export at 90. The screenshot is taken at frame 45 or later (fude_look_shot_at), and the app quits
// five frames after it.
// ===========================================================================

// The look's state: the arguments, and what each flag asked for. Text flags point into the arguments, which stay valid
// for the app's life. Numbers that may be 0 start at -1, meaning not given.
RDE_INTERNAL struct {
    i32       argc;
    c8**      argv;
    // What is wanted once the saves are loaded, or some frames in (a shot's sequence).
    const c8* shot;
    u32       frames;
    b8        paper, deselect, trim_fonts, data, data_replace;
    b8        settings_end;   // --settings-end: Settings open, scrolled to its end at frame 20 (the app's sections).
    u32       shot_at;    // --shot-at=N: take the shot at frame N (0: when the sequence says).
    b8        stay;       // --stay: run the sequence without the shot and do not quit, for a screenshot taken outside.
    b8        rtl;        // --rtl: lay the UI out right to left whatever the language, before its strings exist.
    i32       theme;
    const c8* note_card;  // --note-card=folder|canvas|rename: a new note's card at frame 15. Adds to the saves.
    i32       ui_size;    // --ui-size=N: FUDE_UI_SIZE_ (app.h): 1 Small, 2 Medium, 3 Large, 0 the device's.
    const c8* data_export;
    const c8* data_import;
    const c8* press;      // --press=I,J,...: press the top screen's row's buttons in turn, from frame 30.
    f32       swipe;      // --swipe=DX: drag a finger DX across the top screen from its middle, frames 30 to 37.
    i32       language;   // --language=N: switch to the app's Nth language (text.h) at frame 12.
    const c8* doc;        // --doc=PDF: make a new canvas over a copy of it, opened as the saves load.
    i32       book;       // --book=N: open the app's Nth library book as the saves load (-1: none).
    const c8* doc_view;   // --doc-view=ZOOM,X,Y: set the view at frame 20, X,Y in canvas units at the screen's middle.
    const c8* mark;       // --mark=X0,Y0,X1,Y1[;...]: draw marker strokes (canvas units) at frame 24, in its colour.
    const c8* lasso_area; // --lasso-area=X0,Y0,X1,Y1: lasso round that box at frame 22, to select a PDF's text there.
    const c8* doc_search; // --doc-search=WORDS: search the document for them with the document bar at frame 26.
    i32       doc_page;   // --doc-page=N: go to page N with the document bar at frame 26 (0: no).
    const c8* doc_export; // --doc-export=PDF: write the document with its ink to that file at frame 90.
    i32       doc_turn;   // --doc-turn=N: turn the document's page N a quarter clockwise at frame 30 (0: no).
    // --perf: time the frames over this many seconds (0: off).
    f64       perf_seconds, perf_start;
    u32       perf_frames, perf_settle, perf_slow;
    f64       perf_dt_sum, perf_dt_max, perf_update_sum, perf_update_max, perf_render_sum, perf_render_max;
    c8        perf_label[160];
    void    (*perf_note)(struct fude_app* _app, c8* _out, usize _size);
} fude_look = { .theme = -1, .ui_size = -1, .language = -1, .book = -1 };

// Returns the text after "_flag=" when _arg is "_flag=value" (a pointer into _arg), and NULL otherwise.
const c8* fude_look_value(const c8* _arg, const c8* _flag) {
    const usize _n = rde_strlen(_flag);
    return _arg != NULL && rde_strncmp(_arg, _flag, _n) == 0 && _arg[_n] == '=' ? _arg + _n + 1 : NULL;
}

// Whether _arg is exactly _flag.
b8 fude_look_is(const c8* _arg, const c8* _flag) {
    return _arg != NULL && rde_strcmp(_arg, _flag) == 0;
}

// Whether a screenshot is being taken, so the real mouse is ignored.
b8 fude_look_owns_pointer(void) {
    return fude_look.shot != NULL;
}

// Whether --rtl was given.
b8 fude_look_rtl(void) {
    return fude_look.rtl;
}

// The frame of the shot's sequence, counted while --shot or --stay runs; 0 otherwise.
u32 fude_look_shot_frame(void) {
    return fude_look.shot != NULL || fude_look.stay ? fude_look.frames : 0u;
}

// Sets the function that writes the app's own part of --perf's summary line.
void fude_look_perf_note(void (*_note)(struct fude_app* _app, c8* _out, usize _size)) {
    fude_look.perf_note = _note;
}

// Whether any argument was given; a release is launched with none.
b8 fude_look_given(void) {
    return fude_look.argc > 1;
}

// Whether _flag is one of the arguments, exactly.
b8 fude_look_has(const c8* _flag) {
    for(i32 _i = 1; _i < fude_look.argc; _i++) {
        if(fude_look_is(fude_look.argv[_i], _flag)) {
            return true;
        }
    }
    return false;
}

// Keeps the arguments and reads the core's flags from them. Unknown flags are left for the app's own reader.
void fude_look_args(i32 _argc, c8** _argv) {
    fude_look.argc = _argc;
    fude_look.argv = _argv;
    // Render on demand (redraw.h): a launch with flags is a developer's look, whose sequences count frames and whose
    // --perf and --shot must measure and look exactly as before, so every frame is drawn (--continuous says so on its
    // own); --on-demand keeps render on demand all the same, to try it with flags.
    fude_redraw_set_continuous(_argc > 1 && !fude_look_has("--on-demand"));
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _a = _argv[_i];
        const c8* _v;
        if(fude_look_is(_a, "--paper"))               { fude_look.paper = true; }
        if(fude_look_is(_a, "--deselect"))            { fude_look.deselect = true; }
        if(fude_look_is(_a, "--trim-fonts"))          { fude_look.trim_fonts = true; }
        if(fude_look_is(_a, "--data"))                { fude_look.data = true; }
        if(fude_look_is(_a, "--data-replace"))        { fude_look.data_replace = true; }
        if(fude_look_is(_a, "--stay"))                { fude_look.stay = true; }
        if((_v = fude_look_value(_a, "--theme")) != NULL)          { fude_look.theme = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--ui-size")) != NULL)        { fude_look.ui_size = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--note-card")) != NULL)      { fude_look.note_card = _v; }
        if((_v = fude_look_value(_a, "--shot")) != NULL)           { fude_look.shot = _v; fude_clock_use_frames(); }   // Same picture every run (clock.h).
        if((_v = fude_look_value(_a, "--shot-at")) != NULL)        { fude_look.shot_at = (u32)(i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--data-export")) != NULL)    { fude_look.data_export = _v; }
        if((_v = fude_look_value(_a, "--data-import")) != NULL)    { fude_look.data_import = _v; }
        if((_v = fude_look_value(_a, "--press")) != NULL)          { fude_look.press = _v; }
        if((_v = fude_look_value(_a, "--swipe")) != NULL)          { fude_look.swipe = rde_strtof(_v, NULL); }
        if((_v = fude_look_value(_a, "--language")) != NULL)       { fude_look.language = (i32)rde_strtol(_v, NULL, 10); }
        if(fude_look_is(_a, "--rtl"))                             { fude_look.rtl = true; }
        if((_v = fude_look_value(_a, "--doc")) != NULL)            { fude_look.doc = _v; }
        if((_v = fude_look_value(_a, "--doc-view")) != NULL)       { fude_look.doc_view = _v; }
        if((_v = fude_look_value(_a, "--book")) != NULL)           { fude_look.book = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--mark")) != NULL)           { fude_look.mark = _v; }
        if((_v = fude_look_value(_a, "--lasso-area")) != NULL)     { fude_look.lasso_area = _v; }
        if((_v = fude_look_value(_a, "--doc-search")) != NULL)     { fude_look.doc_search = _v; }
        if((_v = fude_look_value(_a, "--doc-page")) != NULL)       { fude_look.doc_page = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--doc-export")) != NULL)     { fude_look.doc_export = _v; }
        if((_v = fude_look_value(_a, "--doc-turn")) != NULL)       { fude_look.doc_turn = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--perf")) != NULL) {
            // The summary line starts with every other argument, so each run says what it measured.
            fude_look.perf_seconds = rde_strtod(_v, NULL);
            for(i32 _k = 1; _k < _argc; _k++) {
                if(_argv[_k] != NULL && rde_strncmp(_argv[_k], "--perf=", 7) != 0 && rde_strlen(fude_look.perf_label) + rde_strlen(_argv[_k]) + 2u < sizeof(fude_look.perf_label)) {
                    rde_strcat(fude_look.perf_label, sizeof(fude_look.perf_label), _argv[_k]);
                    rde_strcat(fude_look.perf_label, sizeof(fude_look.perf_label), " ");
                }
            }
        }
    }
}

// Does what the flags ask for before the saves load: opens Settings, the side panel, the FAQ or a licence, checks the
// text drawing, and sets the window's size.
void fude_look_start(fude_app* _app) {
    for(i32 _i = 1; _i < fude_look.argc; _i++) {
        const c8* _a = fude_look.argv[_i];
        const c8* _v;
        if(fude_look_is(_a, "--settings"))   { fude_side_open_settings(_app->ui, -1); }
        if(fude_look_is(_a, "--settings-end")) { fude_side_open_settings(_app->ui, -1); fude_look.settings_end = true; }
        if(fude_look_is(_a, "--side"))       { _app->ui->side.open = true; }
        if(fude_look_is(_a, "--faq"))        { _app->ui->side.open = true; _app->ui->side.faq_open = true; }   // FAQ & Contact.
        if(fude_look_is(_a, "--text-check")) { fude_draw_text_check(_app->window); }
        if((_v = fude_look_value(_a, "--licences")) != NULL)   { fude_side_open_settings(_app->ui, (i32)rde_strtol(_v, NULL, 10)); }
        if((_v = fude_look_value(_a, "--size")) != NULL) {
            c8*       _end = NULL;
            const i32 _w   = (i32)rde_strtol(_v, &_end, 10);
            const i32 _h   = _end != NULL && *_end == 'x' ? (i32)rde_strtol(_end + 1, NULL, 10) : 0;
            if(_w > 0 && _h > 0) {
                // The size is the device's in its points, so the window is set at scale 1 and then the interface's
                // scale (app.h) is applied on top.
                rde_window_set_ui_scale(_app->window, 1.0f);
                rde_window_set_size(_app->window, (rde_vec_2I){ _w, _h });
                fude_app_apply_ui_size(_app);
            }
        }
    }
}

// Does what the flags ask for once the saves are loaded: opens a document or a book, and sets the theme and the
// interface's size. The theme and the size are only shown, never saved.
void fude_look_loaded(fude_app* _app) {
    if(fude_look.doc != NULL && fude_documents_new_canvas(_app, FUDE_NOTE_DOCUMENT_OWN, fude_look.doc, "Document", 0u) == 0u) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "look: --doc %s: no canvas made", fude_look.doc);
    }
    if(fude_look.book >= 0 && (u32)fude_look.book < fude_app_ext(_app)->library_count) {
        fude_documents_open_book(_app, &fude_app_ext(_app)->library[fude_look.book], fude_text(FUDE_TEXT_LIBRARY_FOLDER));
    }
    if(fude_look.theme >= 0) {
        fude_theme_set((FUDE_THEME_)fude_look.theme);
        fude_ui_apply_theme(_app->ui);
        fude_session_settings_seen(_app);   // A look, not a change to save.
    }
    if(fude_look.ui_size >= 0 && fude_look.ui_size < (i32)FUDE_UI_SIZE_COUNT) {
        _app->ui_size = (u8)fude_look.ui_size;
        fude_app_apply_ui_size(_app);
        fude_session_settings_seen(_app);   // A look, not a change to save.
    }
}

// Returns the frame the shot is taken at: the one --shot-at gives; 100 with a document or a book, since its pages are
// drawn by then; 60 with marker strokes; else 45, or 14 frames after the last press when that is later.
RDE_INTERNAL u32 fude_look_shot_at(void) {
    if(fude_look.shot_at > 0u) {
        return fude_look.shot_at;
    }
    if(fude_look.doc != NULL || fude_look.book >= 0) {
        return 100u;
    }
    if(fude_look.mark != NULL) {
        return 60u;
    }
    u32 _presses = 0;
    for(const c8* _p = fude_look.press; _p != NULL && *_p != 0; _p = rde_strchr(_p, ',') != NULL ? rde_strchr(_p, ',') + 1 : NULL) {
        _presses++;
    }
    const u32 _last = 30u + 4u * _presses + 10u;
    return _presses > 0 && _last > 45u ? _last : 45u;
}

// Moves the look on by a frame: counts it, does what the flags ask for at this frame, and takes the shot or quits.
void fude_look_frame(fude_app* _app) {
    fude_clock_frame();   // During a screenshot the clock counts frames, not time (clock.h).
    fude_draw_frame();    // The texts' kept shapes: what a frame draws is not let go during it (draw.h).
    // --paper: open the paper panel at frame 20, once the toolbar has been laid out. Without a shot, the frames are
    // counted only for this.
    if(fude_look.paper && fude_look.frames == 20u) {
        fude_toolbar_set_paper_open(&_app->ui->bar, true);
        fude_look.paper = false;
    }
    if(fude_look.shot == NULL && !fude_look.stay) {
        fude_look.frames += fude_look.paper ? 1u : 0u;
        return;
    }
    const u32 _frame = ++fude_look.frames;
    // --settings-end: scrolled as far down as it goes (the scroll is held to the content's end), once laid out and
    // after --language's switch, which builds Settings again from its top.
    if(_frame == 20u && fude_look.settings_end) {
        rde_ui_scroll_area_set_scroll(_app->ui->side.settings_body, (rde_vec_2F){ 0.0f, 1.0e9f });
    }
    if(_frame == 12u && fude_look.language >= 0 && (u32)fude_look.language < FUDE_TEXT_LANGUAGES) {
        fude_text_set_language(FUDE_TEXT_LANGUAGE_LIST[fude_look.language].language);
    }
    // --press: one button every four frames from frame 30, so the UI catches up between presses. The press for this
    // frame is the k-th number in the list.
    if(fude_look.press != NULL && _frame >= 30u && (_frame - 30u) % 4u == 0u) {
        const c8* _p = fude_look.press;
        for(u32 _k = 0; _k < (_frame - 30u) / 4u && _p != NULL; _k++) {
            _p = rde_strchr(_p, ',');
            _p = _p != NULL ? _p + 1 : NULL;
        }
        if(_p != NULL && *_p != 0 && !fude_ui_press(_app->ui, (u32)rde_strtoul(_p, NULL, 10))) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "look: --press %s: no such button on the screen on top", _p);
        }
    }
    // --swipe: down in the middle at frame 30, six moves across, then up at frame 37, through the screen's own pointer
    // functions (none for a screen that takes no pointer).
    const fude_screen_slot* _top = fude_app_top(_app);
    if(fude_look.swipe != 0.0f && _top != NULL && _top->vt->pointer_down != NULL && _frame >= 30u && _frame <= 37u) {
        const f64        _now = rde_engine_get_time_now();
        const rde_vec_2F _at  = { fude_look.swipe * (f32)(_frame - 30u) / 6.0f, 0.0f };
        if(_frame == 30u)      { _top->vt->pointer_down(_top->self, _at, false, _now); }
        else if(_frame <= 36u) { _top->vt->pointer_moved(_top->self, _at, _now); }
        else                   { _top->vt->pointer_up(_top->self, _at, _now); }
    }
    if(_frame == 15u && fude_look.note_card != NULL) {
        const b8  _canvas = rde_strcmp(fude_look.note_card, "canvas") == 0;
        const u32 _note   = fude_notes_add(_app->notes, _canvas ? FUDE_NOTE_CANVAS : FUDE_NOTE_FOLDER, 0u, _canvas ? "Canvas 2" : "Folder 1");
        fude_side_look_card(_app->ui, _note, rde_strcmp(fude_look.note_card, "rename") == 0 ? FUDE_SIDE_CARD_RENAME : FUDE_SIDE_CARD_ACTIONS);
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
        if(rde_sscanf(fude_look.doc_view, "%f,%f,%f", &_z, &_x, &_y) >= 1 && _z > 0.0f) {
            _app->canvas->view.zoom   = _z;
            _app->canvas->view.offset = (rde_vec_2F){ -_x * _z, -_y * _z };
        }
    }
    if(_frame == 24u && fude_look.mark != NULL) {
        // Draw each stroke along its line of text the way a slow hand marks it: 121 samples close together, a little
        // unsteady.
        for(const c8* _m = fude_look.mark; _m != NULL && *_m != 0; _m = rde_strchr(_m, ';') != NULL ? rde_strchr(_m, ';') + 1 : NULL) {
            f32 _x0, _y0, _x1, _y1;
            if(rde_sscanf(_m, "%f,%f,%f,%f", &_x0, &_y0, &_x1, &_y1) != 4) {
                continue;
            }
            _app->ink->marking = true;
            for(u32 _k = 0; _k <= 120u; _k++) {
                const f32        _t = (f32)_k / 120.0f;
                const f32        _w = 1.2f * rde_math_sin_f32((f32)_k * 2.3f) + 0.8f * rde_math_sin_f32((f32)_k * 5.1f);   // The hand's tremor.
                const rde_vec_2F _p = { _x0 + (_x1 - _x0) * _t + 0.6f * _w, _y0 + (_y1 - _y0) * _t + _w };
                if(_k == 0u) { fude_ink_begin(_app->ink, _p, true); }
                else         { fude_ink_extend(_app->ink, _p); }
            }
            fude_ink_end(_app->ink);
            _app->ink->marking = false;
        }
    }
    if(_frame == 22u && fude_look.lasso_area != NULL) {
        f32 _x0, _y0, _x1, _y1;
        if(rde_sscanf(fude_look.lasso_area, "%f,%f,%f,%f", &_x0, &_y0, &_x1, &_y1) == 4) {
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
        fude_doc_turn_page(&_app->page->doc, _app->canvas, _app->ink, (u32)(fude_look.doc_turn - 1));
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
        fude_ui_trim_fonts(_app->ui);   // The shot then shows that every glyph is uploaded again.
    }
    if(fude_look.shot == NULL) {
        return;   // --stay: no shot, and the app carries on.
    }
    if(_frame == fude_look_shot_at()) {
        rde_window_take_screenshot(_app->window, (rde_vec_2I){ 0, 0 }, (rde_vec_2I){ 0, 0 }, fude_look.shot, NULL);   // The next frame, whole.
    } else if(_frame == fude_look_shot_at() + 5u) {
        rde_engine_set_running(false);
    }
}

// --- --perf ---------------------------------------------------------------------------------

// The time is up: appends the summary line to <save dir>/perf.txt and logs it, once (perf_seconds is set to 0, which
// turns the timing off). The line holds the frame count, the frames per second, and the average and worst times of a
// frame (with how many took over 20 ms), of the update and of the render, then the app's own note.
RDE_INTERNAL void fude_look_perf_write(fude_app* _app, f64 _now) {
    const f64 _n    = fude_look.perf_frames > 0 ? (f64)fude_look.perf_frames : 1.0;
    const f64 _span = _now - fude_look.perf_start;
    c8 _line[1024];
    c8 _note[256] = "";
    if(fude_look.perf_note != NULL) {   // The app's own, such as a study app's camera frames.
        fude_look.perf_note(_app, _note, sizeof(_note));
    }
    rde_snprintf(_line, sizeof(_line), "%s| %u frames in %.1f s: %.1f fps, frame %.2f ms avg, %.2f ms worst, %u over 20 ms | update %.2f ms avg, %.2f worst | render %.2f ms avg, %.2f worst%s",
                 fude_look.perf_label, fude_look.perf_frames, _span, _n / _span, 1000.0 * fude_look.perf_dt_sum / _n, 1000.0 * fude_look.perf_dt_max, fude_look.perf_slow,
                 1000.0 * fude_look.perf_update_sum / _n, 1000.0 * fude_look.perf_update_max, 1000.0 * fude_look.perf_render_sum / _n, 1000.0 * fude_look.perf_render_max, _note);
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%sperf.txt", fude_save_dir());
    RDE_TRY({
        rde_file* _f = rde_file_open(_path, RDE_FILE_MODE_APPEND);
        if(_f != NULL && !rde_failed()) {
            rde_file_write_fmt(_f, "%s\n", _line);
            rde_file_close(_f);
        }
    });
    // The whole line in the log too: a release on Android keeps its save folder to itself (perf.txt cannot be copied
    // back from it), so the log (adb logcat) is where a measuring run's numbers are read there.
    rde_log_level(RDE_LOG_LEVEL_INFO, "perf: %s (written to %s)", _line, _path);
    fude_look.perf_seconds = 0.0;
}

// Runs _update, timing it under --perf. The first 60 frames, about a second, let the app settle and are not counted.
void fude_look_timed_update(void (*_update)(f32), f32 _dt) {
    if(fude_look.perf_seconds <= 0.0) {
        _update(_dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    if(fude_look.perf_start <= 0.0) {
        fude_look.perf_start = fude_look.perf_settle++ >= 60u ? _t0 : 0.0;   // A second to settle first.
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

// Runs _render, timing it under --perf, and writes the summary once the seconds asked for have passed.
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
