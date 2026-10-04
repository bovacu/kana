// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Hanzi: Chinese written by hand. The app's shell — what RDE calls (init, the
// events, each frame's update and render, the end), and what it owns.
//
// HOW IT IS PUT TOGETHER (app.h has the whole of it). The page is at the
// bottom (page.h: writing on it, moving it); the screens stack over it in a
// fixed order (hanzi_app.h: HANZI_SCREEN_), and the first one open is on top: it alone
// gets the pointer, the frame and Escape, and its row of buttons shows (row.h).
// The retained UI — the toolbar, the page's menus, the screens' rows, the side
// panel, the word card — is one canvas (ui.h). What is saved, and what leaves
// as a file, is the session's (session.h); a developer's launch flags and
// measures are look.h's.
// ===========================================================================

#include "rde.h"

#include <stdio.h>
#include <string.h>

#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"
#include "drawing/widgets/notice.h"
#include "lang/zh/bake.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "study/services/mlkit.h"
#include "study/handwriting/match.h"
#include "lang/lang.h"
#include "study/models/marks.h"
#include "study/models/examlog.h"
#include "study/models/vocab.h"
#include "study/models/charnote.h"

#include "hanzi_app.h"

#define HANZI_CONFIG_PATH "./assets/config.rdef"


// A developer's build (debug, RDE_DEBUG): the diagnostics HUD (H) and the raw
// pen samples (M) can be shown, and launch arguments are read (the look flags,
// --perf, the bake). A release build has none of it — it takes no arguments at
// all (main) — unless built with -DHANZI_ALLOW_ARGS, to measure a release on the
// device (--perf, COMMANDS.txt). The HUD starts hidden either way, and is not
// a setting: nothing saved shows it.
#if defined(RDE_DEBUG) || defined(HANZI_ALLOW_ARGS)
#define HANZI_TAKES_ARGS 1
#else
#define HANZI_TAKES_ARGS 0
#endif

RDE_INTERNAL rde_window*    window;
RDE_INTERNAL rde_camera     camera;
RDE_INTERNAL void fude_render_top(rde_window* _window, f32 _dt);   // the notice, after the UI (below)
RDE_INTERNAL fude_kanji_db  kanji_db;
RDE_INTERNAL fude_ink       ink;
RDE_INTERNAL fude_canvas    canvas;
RDE_INTERNAL fude_lasso     lasso;
RDE_INTERNAL fude_notes     notes;
RDE_INTERNAL fude_selection selection;   // Browse's ticks (select.h)
RDE_INTERNAL fude_viewer    viewer;
RDE_INTERNAL fude_browse    browse;
RDE_INTERNAL fude_practice  practice;
RDE_INTERNAL fude_album     album;
RDE_INTERNAL fude_check     check;
RDE_INTERNAL fude_exam      exam;
RDE_INTERNAL fude_stats     stats;
RDE_INTERNAL fude_scan      scan;
RDE_INTERNAL fude_vocabview vocabview;
RDE_INTERNAL fude_wordexam  wordexam;
RDE_INTERNAL fude_translator translator;
RDE_INTERNAL fude_welcome   welcome;
RDE_INTERNAL fude_readcard  readcard;   // why the app works offline, once after the welcome
RDE_INTERNAL fude_library   library;
RDE_INTERNAL fude_page_input page;
RDE_INTERNAL fude_ui        ui;
RDE_INTERNAL hanzi_app      hanzi;   // the app: the drawing core's, the study layer's, Hanzi's own (hanzi_app.h)
RDE_INTERNAL fude_app* const app = &hanzi.study.app;

// Running as the offline data bake (--bake, desktop): nothing else is set up, so
// every callback returns at once.
RDE_INTERNAL b8  baking   = false;
RDE_INTERNAL f32 frame_ms = 0.0f;   // smoothed: the raw value jitters too much to read off a screen

// --- the HUD (a developer's, H) ------------------------------------------------------------

// THE HEADLINE, and the reason the HUD exists: whether pen events arrive at all
// (PEN, TOUCH ONLY, or nothing), the pen's pressure and tilt, the input's rate
// (a pen sampling at 120-240 Hz that arrives here at ~60 is coalesced to the
// frame rate), the app-side lag, the strokes, the zoom, the last save. Absolute
// pen-to-photon latency is NOT measurable from inside the process.
RDE_INTERNAL void fude_hud_line(const c8* _text, f32 _x, f32* _y, f32 _gap) {
    fude_draw_text(app->font, app->font_px, _text, _x, *_y, 17.0f, fude_theme_active()->hud);
    *_y -= _gap;
}

RDE_INTERNAL void fude_hud_render(rde_window* _window) {
    const rde_vec_2I        _size   = rde_window_get_size(_window);
    const rde_vec_4I        _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const fude_session_info _save   = fude_session_info_now();
    const f32               _x      = -(f32)_size.x * 0.5f + (f32)_insets.x + 20.0f;   // below the status bar, not under it
    f32                     _y      = (f32)_size.y * 0.5f - (f32)_insets.y - 34.0f;
    c8                      _line[192];

    fude_hud_line(ink.pen_seen ? "PEN  (stylus events arriving)" : page.touch_seen ? "TOUCH ONLY  - no pen events!" : "waiting for input...", _x, &_y, 34.0f);
    snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)%s   tilt %.0f / %.0f   pen id %u   %s", (f64)ink.pressure, (f64)ink.pressure_raw,
             !ink.pen_seen ? "" : ink.pressure_live ? " [real]" : " [none - width from speed]", (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id, ink.eraser ? "ERASER" : "tip");
    fude_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms", (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    fude_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end", (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    fude_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "%u strokes, %u points   history %u (+%u redo)", fude_ink_alive_strokes(&ink), fude_ink_total_points(&ink), fude_ink_undo_steps(&ink), fude_ink_redo_steps(&ink));
    fude_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "zoom %.0f%%   fingers: 1 pan, 2 pinch   tool: %s", (f64)(canvas.view.zoom * 100.0f),
             ui.bar.tool == FUDE_TOOL_ERASE ? "ERASE" : ui.bar.tool == FUDE_TOOL_LASSO ? "LASSO" : "DRAW");
    fude_hud_line(_line, _x, &_y, 28.0f);
    if(_save.save_failed) {
        snprintf(_line, sizeof(_line), "SAVE FAILED - retrying   (%s)", _save.load_note);
    } else if(_save.last_save_time > 0.0) {
        snprintf(_line, sizeof(_line), "saved %.0f s ago: %.1f KB in %.1f ms   (%s)", rde_engine_get_time_now() - _save.last_save_time,
                 (f64)_save.last_save_bytes / 1024.0, (f64)_save.last_save_ms, _save.load_note);
    } else {
        snprintf(_line, sizeof(_line), "not saved yet this run   (%s)", _save.load_note);
    }
    fude_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "brush: %s", ink.brush_scale == FUDE_INK_BRUSH_SCALE_PAGE ? "PAGE - same width on the page at any zoom" : "SCREEN - same width on screen while writing");
    fude_hud_line(_line, _x, &_y, 28.0f);
}

// --- init -------------------------------------------------------------------------------------

// The screens, in the order they stack (hanzi_app.h: HANZI_SCREEN_).
RDE_INTERNAL void fude_screens_place(void) {
    const struct { HANZI_SCREEN_ id; const fude_screen* vt; void* self; } _screens[] = {
        { HANZI_SCREEN_READCARD, &FUDE_READCARD_SCREEN, &readcard },
        { HANZI_SCREEN_WELCOME,  &FUDE_WELCOME_SCREEN,  &welcome },
        { HANZI_SCREEN_PRACTICE, &FUDE_PRACTICE_SCREEN, &practice },
        { HANZI_SCREEN_VIEWER,   &FUDE_VIEWER_SCREEN,   &viewer },
        { HANZI_SCREEN_SCAN,     &FUDE_SCAN_SCREEN,     &scan },
        { HANZI_SCREEN_WORDEXAM, &FUDE_WORDEXAM_SCREEN, &wordexam },
        { HANZI_SCREEN_EXAM,     &FUDE_EXAM_SCREEN,     &exam },
        { HANZI_SCREEN_STATS,    &FUDE_STATS_SCREEN,    &stats },
        { HANZI_SCREEN_TRANSLATOR, &FUDE_TRANSLATOR_SCREEN, &translator },
        { HANZI_SCREEN_VOCAB,    &FUDE_VOCAB_SCREEN,    &vocabview },
        { HANZI_SCREEN_CHECK,    &FUDE_CHECK_SCREEN,    &check },
        { HANZI_SCREEN_LIBRARY,  &FUDE_LIBRARY_SCREEN,  &library },
        { HANZI_SCREEN_ALBUM,    &FUDE_ALBUM_SCREEN,    &album },
        { HANZI_SCREEN_BROWSE,   &FUDE_BROWSE_SCREEN,   &browse },
    };
    for(u32 _i = 0; _i < sizeof(_screens) / sizeof(_screens[0]); _i++) {
        app->screens[_screens[_i].id] = (fude_screen_slot){ _screens[_i].vt, _screens[_i].self };
    }
    app->screen_count = HANZI_SCREEN_COUNT;
}

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    if(fude_bake_requested(_argc, _argv)) {
        baking = true;
        const i32 _rc = fude_bake_run(_argc, _argv);
        rde_log_level(_rc == 0 ? RDE_LOG_LEVEL_INFO : RDE_LOG_LEVEL_ERROR, "bake %s", _rc == 0 ? "finished" : "FAILED");
        // Not rde_engine_destroy_engine: rde_run carries on after init_func.
        rde_engine_set_running(false);
        return;
    }

    window = _window;
    fude_app_window(_window);    // its units (dp on Android): before anything reads its size
    // The words first: the UI is built in them. The device's language when Hanzi
    // speaks it; a saved choice replaces it (session.h).
    {   // its fourth UI language: the one it teaches (lang.h), when its strings have it
        const c8*           _name;
        const c8*           _flag;
        const RDE_LANGUAGE_ _taught = fude_lang_ui_language(&_name, &_flag);
        fude_text_set_taught_language(_taught, _name, _flag);
    }
    fude_text_set_language(fude_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    rde_engine_set_top_overlay_render(fude_render_top);
    fude_look_args(_argc, _argv);
    hanzi_look_args(_argc, _argv);

    // The page.
    fude_ink_init(&ink);
    fude_canvas_init(&canvas);
    fude_lasso_init(&lasso);
    fude_notes_init(&notes);

    // The baked character data (see bake.h). Without it the app still draws; the
    // screens that need it just stay empty.
    const b8             _have = fude_kanji_load(&kanji_db, FUDE_KANJI_FILE);
    const fude_kanji_db* _db   = _have ? &kanji_db : NULL;
    fude_viewer_init(&viewer, _db);
    fude_browse_init(&browse, _db);
    fude_selection_init(&selection, _have ? kanji_db.count : 0u);
    fude_exam_init(&exam, _db, &browse.catalog);
    fude_vocabview_init(&vocabview, _db);
    fude_translator_init(&translator, _db);
    fude_wordexam_init(&wordexam, _db, &browse.catalog);
    fude_stats_init(&stats, _db, &browse.catalog);
    fude_scan_init(&scan);
    scan.db          = _db;   // the lines' words (wordsplit.h)
    browse.selection = &selection;
    fude_practice_init(&practice, _db);
    fude_album_init(&album, _db);
    fude_check_init(&check, _db, &browse.catalog);
    if(_have) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "hanzi: %u characters loaded", kanji_db.count);
    }

    // The app: what it holds, the screens in order, the page, the UI over it all.
    hanzi = (hanzi_app){
        .study = {
            .app = {
                .info = &HANZI_INFO, .ext = &HANZI_EXTENSION, .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f,
                .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
            },
            .db = _have ? &kanji_db : NULL, .catalog = &browse.catalog,
            .viewer = &viewer, .browse = &browse, .practice = &practice, .album = &album, .check = &check, .exam = &exam,
            .stats = &stats, .scan = &scan, .vocab = &vocabview, .wordexam = &wordexam, .translator = &translator, .selection = &selection,
        },
        .welcome = &welcome, .library = &library,
    };
    fude_library_init(&library, app);
    fude_app_start(app);         // its save folder, its window's title: before anything reads a save
#if defined(RDE_PLATFORM_MOBILE)
    app->finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    fude_screens_place();
    welcome.card       = &readcard;          // closed, the welcome opens it the first time
    hanzi.study.welcome  = &welcome;           // and the voice card, from the side panel (study.h)
    welcome.cards_read = &app->cards_read;
    fude_study_init(&hanzi.study);
    fude_page_init(&page, app);
    fude_ui_init(&ui, app);
    if(ui.font != NULL) {
        app->font    = ui.font;   // the screens' text: the UI font, in screen units, scaled from the size it was loaded at
        app->font_px = (f32)FUDE_KIT_FONT_SIZE;
    }
    fude_glyph_set_text_font(app->font, app->font_px);   // what the data has no strokes for (pinyin's letters)

    fude_look_start(app);
    hanzi_look_start(app);
    fude_session_load(app);
    fude_look_loaded(app);
    hanzi_look_loaded(app);
    fude_mlkit_prepare();   // after the settings: when ML Kit is on, its model downloads the first time (iOS; nothing elsewhere)
    rde_log_color(RDE_LOG_COLOR_GREEN, "%s", "hanzi: the pen writes, fingers move the page; the toolbar has the rest.");
}

// --- events --------------------------------------------------------------------------------------

void on_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);
    if(baking || _event == NULL) {
        return;
    }
    // Out of sight, or the OS short of memory: the fonts' glyph memory goes back
    // to what they started with (each glyph uploads again when next drawn). An
    // event comes before the frame draws, as rde_font_trim needs.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        fude_ui_trim_fonts(&ui);
    }
    // Leaving: save now. The autosave (session.h) is the guarantee — this event
    // reaches here through the queue and may not be handled before the app is
    // suspended. The camera stops: what was in view stays.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        fude_session_save_on_exit(app);
        if(scan.open) {
            fude_scan_pause(&scan);
        }
    }
    // A screen on top has the pointer (its buttons are UI and have had the event
    // already); the page has it otherwise.
    if(fude_app_top(app) != NULL) {
        if(page.finger_state != 0u || page.erasing || page.pen_on_ui || ink.drawing) {
            fude_page_let_go(&page);   // a screen over the page: what was writing there ends
        }
        fude_app_screen_event(app, _event);
    } else {
        fude_page_event(&page, _event);
    }
}

// --- each frame -------------------------------------------------------------------------------

RDE_INTERNAL void fude_update(f32 _dt) {
    if(baking) {
        return;
    }
    fude_look_frame(app);
    hanzi_look_frame(app);
    // A language chosen: the UI and the meanings in it (English where it has none).
    static u32 _language_seen = 0;
    fude_ui_follow_language(&ui);
    if(_language_seen != fude_text_revision()) {
        _language_seen = fude_text_revision();
        if(hanzi.study.db != NULL) {
            const RDE_LANGUAGE_ _l = fude_text_language();
            fude_kanji_set_language(&kanji_db, _l == RDE_LANGUAGE_ES_ES ? "es" : _l == RDE_LANGUAGE_PT_BR ? "pt" : _l == RDE_LANGUAGE_FR_FR ? "fr" : NULL);
            fude_browse_language_changed(&browse);
            viewer.rows_for = UINT32_MAX;   // its word rows keep copies
        }
    }
#if defined(RDE_PLATFORM_IOS)
    // Apple Pencil's double tap: asked for once the app's view is there (a few
    // frames at most after launch).
    static b8  _pencil_taps  = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
    }
#endif
    fude_study_update(app);   // Select mode's end; a better voice offered

    fude_ink_frame_begin(&ink);
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

    // The screen on top has the frame; the page, when there is none.
    if(!fude_app_update(app, _dt)) {
        fude_page_update(&page);
    }
    fude_ui_update(&ui);
    fude_session_update(app);
    fude_study_sheet_update(app);
}

RDE_INTERNAL void fude_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(baking) {
        return;
    }
    // EVERY frame: the engine resets the clear colour at the start of each one,
    // and a frame nobody sets it for clears to black.
    rde_rendering_clear_background_color(fude_theme_active()->page);
    rde_rendering_2d_begin_drawing(_window, &camera);
    if(!fude_app_render(app)) {
        fude_page_render(&page, _window);
        if(page.show_hud && app->font != NULL) {
            fude_hud_render(_window);
        }
    }
    fude_app_render_overlays(app);   // the welcome, over everything
    rde_rendering_2d_end_drawing();
}

// After every UI canvas too (the side panel is the engine's UI): the notice, over
// the page's bottom or a screen's row, on top of all.
RDE_INTERNAL void fude_render_top(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(baking || app->font == NULL) {
        return;
    }
    rde_rendering_2d_begin_drawing(_window, &camera);
    fude_notice_render(_window, app->font, app->font_px, fude_app_top(app) == NULL ? 72.0f : 8.0f + fude_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

void on_update(f32 _dt) {
    fude_look_timed_update(fude_update, _dt);
}

void on_render(rde_window* _window, f32 _dt) {
    fude_look_timed_render(app, fude_render, _window, _dt);
}

void on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

void on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

void on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

void end_func(void) {
    if(baking) {
        return;
    }
    fude_session_save_on_exit(app);
    fude_ui_destroy(&ui);
    fude_study_destroy(&hanzi.study);
    fude_lasso_destroy(&lasso);
    fude_viewer_destroy(&viewer);
    fude_browse_destroy(&browse);
    fude_selection_destroy(&selection);
    fude_exam_destroy(&exam);
    fude_wordexam_destroy(&wordexam);
    fude_vocabview_destroy(&vocabview);
    fude_translator_destroy(&translator);
    fude_library_destroy(&library);
    fude_stats_destroy(&stats);
    fude_scan_destroy(&scan);
    fude_marks_close();
    fude_examlog_close();
    fude_vocab_close();
    fude_charnotes_close();
    fude_practice_destroy(&practice);
    fude_album_destroy(&album);
    fude_notes_destroy(&notes);
    fude_check_destroy(&check);
    fude_match_release();
    fude_kanji_unload(&kanji_db);
    fude_ink_destroy(&ink);
}

int main(i32 _argc, c8* _argv[]) {
#if !HANZI_TAKES_ARGS
    // A release: whatever it is launched with is ignored — by Hanzi and by RDE
    // (which reads arguments as config overrides). Only the program's name.
    _argc = _argc > 1 ? 1 : _argc;
#endif
    // The bake runs from the project root (its sources are data/raw/...), where the
    // app's config is under apps/hanzi/: its memory sizes hold the data files whole.
    const c8* _config = fude_bake_requested(_argc, _argv) ? "apps/hanzi/assets/config.rdef" : HANZI_CONFIG_PATH;
    return rde_run(_argc, _argv, _config, init_func, on_event, on_fixed_update, on_update, on_late_update, on_render, on_crash, end_func);
}
