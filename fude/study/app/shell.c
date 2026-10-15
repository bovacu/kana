// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/app/shell.h"

#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"
#include "drawing/widgets/notice.h"
#include "drawing/app/readcard.h"
#include "drawing/app/library.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "study/app/study.h"
#include "study/chars/bake.h"
#include "study/screens/chart.h"
#include "drawing/ink/mlkit.h"
#include "study/handwriting/match.h"
#include "study/models/marks.h"
#include "study/models/examlog.h"
#include "study/models/review.h"
#include "study/models/vocab.h"
#include "study/models/charnote.h"
#include "lang/lang.h"

// ===========================================================================
// See shell.h.
//
// Everything the app owns is a static here, made once in fude_shell_init and destroyed in fude_shell_end, so there is
// one app a process. RDE calls the fude_shell_* functions; each frame's update and render go through look.h's timers
// (fude_look_timed_update, fude_look_timed_render), so that --perf can measure them.
// ===========================================================================

#define FUDE_SHELL_CONFIG "./assets/config.rdef"   // where the app runs, beside its assets

// A developer's build (debug, RDE_DEBUG) can show the diagnostics HUD (H) and the raw pen samples (M), and reads launch
// arguments (the look flags, --perf, the bake). A release build has none of it, and takes no arguments at all
// (fude_study_shell_run), unless it is built with -DFUDE_ALLOW_ARGS to measure a release on the device (--perf,
// COMMANDS.txt). The HUD starts hidden either way, and is not a setting: nothing saved shows it.
#if defined(RDE_DEBUG) || defined(FUDE_ALLOW_ARGS)
#define FUDE_SHELL_TAKES_ARGS 1
#else
#define FUDE_SHELL_TAKES_ARGS 0
#endif

RDE_INTERNAL const fude_study_shell_def* shell;   // the app's description (fude_study_shell_run)
RDE_INTERNAL rde_window*     window;
RDE_INTERNAL rde_camera      camera;
RDE_INTERNAL fude_kanji_db   kanji_db;
RDE_INTERNAL fude_ink        ink;
RDE_INTERNAL fude_canvas     canvas;
RDE_INTERNAL fude_lasso      lasso;
RDE_INTERNAL fude_notes      notes;
RDE_INTERNAL fude_selection  selection;   // Browse's and the chart's ticks (select.h)
RDE_INTERNAL fude_viewer     viewer;
RDE_INTERNAL fude_browse     browse;
RDE_INTERNAL fude_chart      chart;
RDE_INTERNAL fude_practice   practice;
RDE_INTERNAL fude_album      album;
RDE_INTERNAL fude_check      check;
RDE_INTERNAL fude_exam       exam;
RDE_INTERNAL fude_stats      stats;
RDE_INTERNAL fude_scan       scan;
RDE_INTERNAL fude_vocabview  vocabview;
RDE_INTERNAL fude_wordexam   wordexam;
RDE_INTERNAL fude_translator translator;
RDE_INTERNAL fude_welcome    welcome;
RDE_INTERNAL fude_readcard   readcard;    // why the app works offline, opened once after the welcome
RDE_INTERNAL fude_library    library;
RDE_INTERNAL fude_page_input page;
RDE_INTERNAL fude_ui         ui;
RDE_INTERNAL fude_study      study;       // the app: the drawing core's part, and the study layer's (study.h)
RDE_INTERNAL fude_app* const app = &study.app;

// Draws the notice on top of everything, after the UI (defined below).
RDE_INTERNAL void fude_shell_render_top(rde_window* _window, f32 _dt);   // the notice, after the UI (below)

// True when the app runs as the offline data bake (--bake, on a computer). Then nothing else is set up, so every
// callback returns at once.
RDE_INTERNAL b8  baking   = false;
RDE_INTERNAL f32 frame_ms = 0.0f;   // smoothed, because the raw value jitters too much to read off a screen

// --- the HUD (a developer's, H) ------------------------------------------------------------
//
// The HUD's headline, and the reason it exists, is whether pen events arrive at all (PEN, TOUCH ONLY, or nothing).
// Then it shows the pen's pressure and tilt, the input's rate (a pen sampling at 120-240 Hz that arrives here at
// about 60 Hz is being coalesced to the frame rate), the app-side lag, the strokes, the zoom and the last save. The
// absolute pen-to-photon latency cannot be measured from inside the process.

// Draws one of the HUD's lines at _x, *_y, and moves *_y down by _gap.
RDE_INTERNAL void fude_hud_line(const c8* _text, f32 _x, f32* _y, f32 _gap) {
    fude_draw_text(app->font, app->font_px, _text, _x, *_y, 17.0f, fude_theme_active()->hud);
    *_y -= _gap;
}

// Draws the HUD's lines at the top left of the safe area, below the status bar rather than under it.
RDE_INTERNAL void fude_hud_render(rde_window* _window) {
    const rde_vec_2I        _size   = rde_window_get_size(_window);
    const rde_vec_4I        _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const fude_session_info _save   = fude_session_info_now();
    const f32               _x      = -(f32)_size.x * 0.5f + (f32)_insets.x + 20.0f;   // inside the safe area
    f32                     _y      = (f32)_size.y * 0.5f - (f32)_insets.y - 34.0f;
    c8                      _line[192];

    fude_hud_line(ink.pen_seen ? "PEN  (stylus events arriving)" : page.touch_seen ? "TOUCH ONLY  - no pen events!" : "waiting for input...", _x, &_y, 34.0f);
    rde_snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)%s   tilt %.0f / %.0f   pen id %u   %s", (f64)ink.pressure, (f64)ink.pressure_raw,
             !ink.pen_seen ? "" : ink.pressure_live ? " [real]" : " [none - width from speed]", (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id, ink.eraser ? "ERASER" : "tip");
    fude_hud_line(_line, _x, &_y, 28.0f);
    rde_snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms", (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    fude_hud_line(_line, _x, &_y, 28.0f);
    rde_snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end", (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    fude_hud_line(_line, _x, &_y, 28.0f);
    rde_snprintf(_line, sizeof(_line), "%u strokes, %u points   history %u (+%u redo)", fude_ink_alive_strokes(&ink), fude_ink_total_points(&ink), fude_ink_undo_steps(&ink), fude_ink_redo_steps(&ink));
    fude_hud_line(_line, _x, &_y, 28.0f);
    rde_snprintf(_line, sizeof(_line), "zoom %.0f%%   fingers: 1 pan, 2 pinch   tool: %s", (f64)(canvas.view.zoom * 100.0f),
             ui.bar.tool == FUDE_TOOL_ERASE ? "ERASE" : ui.bar.tool == FUDE_TOOL_LASSO ? "LASSO" : "DRAW");
    fude_hud_line(_line, _x, &_y, 28.0f);
    if(_save.save_failed) {
        rde_snprintf(_line, sizeof(_line), "SAVE FAILED - retrying   (%s)", _save.load_note);
    } else if(_save.last_save_time > 0.0) {
        rde_snprintf(_line, sizeof(_line), "saved %.0f s ago: %.1f KB in %.1f ms   (%s)", rde_engine_get_time_now() - _save.last_save_time,
                 (f64)_save.last_save_bytes / 1024.0, (f64)_save.last_save_ms, _save.load_note);
    } else {
        rde_snprintf(_line, sizeof(_line), "not saved yet this run   (%s)", _save.load_note);
    }
    fude_hud_line(_line, _x, &_y, 28.0f);
    rde_snprintf(_line, sizeof(_line), "brush: %s", ink.brush_scale == FUDE_INK_BRUSH_SCALE_PAGE ? "PAGE - same width on the page at any zoom" : "SCREEN - same width on screen while writing");
    fude_hud_line(_line, _x, &_y, 28.0f);
}

// --- init -------------------------------------------------------------------------------------

// Puts the screens into the app's table, in the order they stack (FUDE_STUDY_SCREEN_).
RDE_INTERNAL void fude_shell_place_screens(void) {
    const struct { FUDE_STUDY_SCREEN_ id; const fude_screen* vt; any self; } _screens[] = {
        { FUDE_STUDY_SCREEN_READCARD,   &FUDE_READCARD_SCREEN,   &readcard },
        { FUDE_STUDY_SCREEN_WELCOME,    &FUDE_WELCOME_SCREEN,    &welcome },
        { FUDE_STUDY_SCREEN_PRACTICE,   &FUDE_PRACTICE_SCREEN,   &practice },
        { FUDE_STUDY_SCREEN_VIEWER,     &FUDE_VIEWER_SCREEN,     &viewer },
        { FUDE_STUDY_SCREEN_SCAN,       &FUDE_SCAN_SCREEN,       &scan },
        { FUDE_STUDY_SCREEN_WORDEXAM,   &FUDE_WORDEXAM_SCREEN,   &wordexam },
        { FUDE_STUDY_SCREEN_EXAM,       &FUDE_EXAM_SCREEN,       &exam },
        { FUDE_STUDY_SCREEN_STATS,      &FUDE_STATS_SCREEN,      &stats },
        { FUDE_STUDY_SCREEN_TRANSLATOR, &FUDE_TRANSLATOR_SCREEN, &translator },
        { FUDE_STUDY_SCREEN_VOCAB,      &FUDE_VOCAB_SCREEN,      &vocabview },
        { FUDE_STUDY_SCREEN_CHECK,      &FUDE_CHECK_SCREEN,      &check },
        { FUDE_STUDY_SCREEN_LIBRARY,    &FUDE_LIBRARY_SCREEN,    &library },
        { FUDE_STUDY_SCREEN_ALBUM,      &FUDE_ALBUM_SCREEN,      &album },
        { FUDE_STUDY_SCREEN_CHART,      &FUDE_CHART_SCREEN,      &chart },
        { FUDE_STUDY_SCREEN_BROWSE,     &FUDE_BROWSE_SCREEN,     &browse },
    };
    for(u32 _i = 0; _i < sizeof(_screens) / sizeof(_screens[0]); _i++) {
        app->screens[_screens[_i].id] = (fude_screen_slot){ _screens[_i].vt, _screens[_i].self };
    }
    app->screen_count = FUDE_STUDY_SCREEN_COUNT;
}

// RDE's init: runs the data bake instead when --bake asks for it. Otherwise it reads the launch flags, loads the
// character data, sets up the page, the screens, the app and its UI, then loads the saves.
RDE_INTERNAL void fude_shell_init(i32 _argc, c8** _argv, rde_window* _window) {
    if(fude_bake_requested(_argc, _argv)) {
        baking = true;
        const i32 _rc = fude_bake_run(_argc, _argv);
        rde_log_level(_rc == 0 ? RDE_LOG_LEVEL_INFO : RDE_LOG_LEVEL_ERROR, "bake %s", _rc == 0 ? "finished" : "FAILED");
        // Not rde_engine_destroy_engine, because rde_run carries on after its init returns.
        rde_engine_set_running(false);
        return;
    }

    window = _window;
    fude_app_window(_window);    // its units (dp on Android), set before anything reads its size
    // The words come first, because the UI is built in them. The device's language is used when the app speaks it, and
    // a saved choice replaces it later (session.h).
    {   // the fourth UI language is the one the app teaches (lang.h), when its strings have it
        const c8*           _name;
        const c8*           _flag;
        const RDE_LANGUAGE_ _taught = fude_lang_ui_language(&_name, &_flag);
        fude_text_set_taught_language(_taught, _name, _flag);
    }
    fude_text_set_language(fude_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    rde_engine_set_top_overlay_render(fude_shell_render_top);
    fude_look_args(_argc, _argv);
    fude_study_look_args(_argc, _argv);

    // The page.
    fude_ink_init(&ink);
    fude_canvas_init(&canvas);
    fude_lasso_init(&lasso);
    fude_notes_init(&notes);

    // The baked character data (see bake.h). Without it the app still draws; the screens that need it just stay empty.
    const b8       _have = fude_kanji_load(&kanji_db, FUDE_KANJI_FILE);
    fude_kanji_db* _db   = _have ? &kanji_db : NULL;   // (fude_study.db holds it as it is; the screens take it const)
    fude_viewer_init(&viewer, _db);
    fude_browse_init(&browse, _db);
    fude_chart_init(&chart, _db);
    fude_selection_init(&selection, _have ? kanji_db.count : 0u);
    fude_exam_init(&exam, _db, &browse.catalog);
    fude_vocabview_init(&vocabview, _db);
    fude_translator_init(&translator, _db);
    fude_wordexam_init(&wordexam, _db, &browse.catalog);
    fude_stats_init(&stats, _db, &browse.catalog);
    fude_scan_init(&scan);
    scan.db          = _db;   // for the words in the lines read (wordsplit.h)
    browse.selection = &selection;
    chart.selection  = &selection;
    fude_practice_init(&practice, _db);
    fude_album_init(&album, _db);
    fude_check_init(&check, _db, &browse.catalog);
    if(_have) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "%s: %u characters loaded", shell->info->name, kanji_db.count);
    }

    // The app: what it holds, the screens in order, the page, and the UI over it all.
    study = (fude_study){
        .app = {
            .info = shell->info, .ext = shell->ext, .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f,
            .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
        },
        .db = _db, .catalog = &browse.catalog,
        .viewer = &viewer, .browse = &browse, .chart = &chart, .practice = &practice, .album = &album, .check = &check, .exam = &exam,
        .stats = &stats, .scan = &scan, .vocab = &vocabview, .wordexam = &wordexam, .translator = &translator,
        .library = &library, .welcome = &welcome, .selection = &selection,
    };
    fude_library_init(&library, app);
    fude_app_start(app);         // its save folder and its window's title, set before anything reads a save
#if defined(RDE_PLATFORM_MOBILE)
    app->finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    fude_shell_place_screens();
    welcome.card       = &readcard;   // once closed, the welcome opens it the first time
    welcome.cards_read = &app->cards_read;
    fude_study_init(&study);
    fude_page_init(&page, app);
    fude_ui_init(&ui, app);
    if(ui.font != NULL) {
        // The screens' text uses the UI font, in screen units, scaled from the size it was loaded at.
        app->font    = ui.font;   // the UI font, for the screens' text
        app->font_px = (f32)FUDE_KIT_FONT_SIZE;
    }
    fude_glyph_set_text_font(app->font, app->font_px);   // for what the data has no strokes for (pinyin's letters)

    fude_look_start(app);
    fude_study_look_start(app);
    fude_session_load(app);
    fude_look_loaded(app);
    fude_study_look_loaded(app);
    fude_mlkit_set_model(fude_lang_ink_model());   // the handwriting of the language taught
    // After the settings are loaded: when ML Kit is on, its model downloads the first time, on iOS and Android (where
    // ML Kit is not in the build, the desktop's and the iOS Simulator's, nothing happens).
    fude_mlkit_prepare();   // after the settings
    rde_log_color(RDE_LOG_COLOR_GREEN, "%s: the pen writes, fingers move the page; the toolbar has the rest.", shell->info->name);
}

// --- events --------------------------------------------------------------------------------------

// RDE's events: the screen on top gets the pointer, or the page when there is none.
RDE_INTERNAL void fude_shell_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);
    if(baking || _event == NULL) {
        return;
    }
    // When the app goes out of sight, or the OS is short of memory, the fonts' glyph memory goes back to what it
    // started with (each glyph uploads again when it is next drawn). An event comes before the frame draws, as
    // rde_font_trim needs.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        fude_ui_trim_fonts(&ui);
    }
    // When the app leaves, it saves now. The autosave (session.h) is the guarantee, because this event reaches here
    // through the queue and may not be handled before the app is suspended. The camera stops, and what was in view
    // stays.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        fude_session_save_on_exit(app);
        if(scan.open) {
            fude_scan_pause(&scan);
        }
    }
    // A screen on top has the pointer (its buttons are UI and have had the event already); otherwise the page has it.
    if(fude_app_top(app) != NULL) {
        if(page.finger_state != 0u || page.erasing || page.pen_on_ui || ink.drawing) {
            fude_page_let_go(&page);   // a screen is over the page, so whatever was writing there ends
        }
        fude_app_screen_event(app, _event);
    } else {
        fude_page_event(&page, _event);
    }
}

// --- each frame -------------------------------------------------------------------------------

// Once a frame: the look flags' frame, the language, the study's update, then the screen on top or the page, the UI,
// the session's autosave and the sheet asked for.
RDE_INTERNAL void fude_shell_update(f32 _dt) {
    if(baking) {
        return;
    }
    fude_look_frame(app);
    fude_study_look_frame(app);
    // When a language is chosen, the UI follows, and so do the meanings in it (English where the data has none in that
    // language).
    static u32 _language_seen = 0;
    fude_ui_follow_language(&ui);
    if(_language_seen != fude_text_revision()) {
        _language_seen = fude_text_revision();
        if(study.db != NULL) {
            const RDE_LANGUAGE_ _l = fude_text_language();
            fude_kanji_set_language(&kanji_db, _l == RDE_LANGUAGE_ES_ES ? "es" : _l == RDE_LANGUAGE_PT_BR ? "pt" : _l == RDE_LANGUAGE_FR_FR ? "fr" : NULL);
            fude_browse_language_changed(&browse);
            viewer.rows_for = UINT32_MAX;   // its word rows keep copies of the meanings, so they are made again
        }
    }
#if defined(RDE_PLATFORM_IOS)
    // Apple Pencil's double tap is asked for once the app's view is there, which takes a few frames at most after
    // launch. It is tried for 120 frames.
    static b8  _pencil_taps  = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
    }
#endif
    fude_study_update(app);   // Select mode's end, and a better voice offered

    fude_ink_frame_begin(&ink);
    // The frame time, smoothed for the HUD.
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

    // The screen on top has the frame, or the page when there is none.
    if(!fude_app_update(app, _dt)) {
        fude_page_update(&page);
    }
    fude_ui_update(&ui);
    fude_session_update(app);
    fude_study_sheet_update(app);
}

// Draws the frame: the screen on top, or the page (and the HUD when it shows), then the overlays.
RDE_INTERNAL void fude_shell_render_frame(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(baking) {
        return;
    }
    // The background is set EVERY frame: the engine resets the clear colour at the start of each one, and a frame
    // nobody sets it for clears to black.
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

// Drawn after every UI canvas too (the side panel is the engine's UI): the notice, over the page's bottom or a
// screen's row, on top of all.
RDE_INTERNAL void fude_shell_render_top(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(baking || app->font == NULL) {
        return;
    }
    rde_rendering_2d_begin_drawing(_window, &camera);
    fude_notice_render(_window, app->font, app->font_px, fude_app_top(app) == NULL ? 72.0f : 8.0f + fude_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

// RDE's update, timed by look.h for --perf.
RDE_INTERNAL void fude_shell_on_update(f32 _dt) {
    fude_look_timed_update(fude_shell_update, _dt);
}

// RDE's render, timed by look.h for --perf.
RDE_INTERNAL void fude_shell_on_render(rde_window* _window, f32 _dt) {
    fude_look_timed_render(app, fude_shell_render_frame, _window, _dt);
}

// RDE's fixed update and late update: the app has nothing to do in them.
RDE_INTERNAL void fude_shell_on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

RDE_INTERNAL void fude_shell_on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

// RDE's crash handler: logs the error and its call stack.
RDE_INTERNAL void fude_shell_on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

// --- the end ----------------------------------------------------------------------------------

// RDE's end: saves, then lets go of everything, in about the reverse order it was made.
RDE_INTERNAL void fude_shell_end(void) {
    if(baking) {
        return;
    }
    fude_session_save_on_exit(app);
    fude_ui_destroy(&ui);
    fude_study_destroy(&study);
    fude_lasso_destroy(&lasso);
    fude_viewer_destroy(&viewer);
    fude_browse_destroy(&browse);
    fude_chart_destroy(&chart);
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
    fude_reviews_close();
    fude_vocab_close();
    fude_charnotes_close();
    fude_practice_destroy(&practice);
    fude_album_destroy(&album);
    fude_page_destroy(&page);
    fude_notes_destroy(&notes);
    fude_check_destroy(&check);
    fude_match_release();
    fude_kanji_unload(&kanji_db);
    fude_ink_destroy(&ink);
}

i32 fude_study_shell_run(const fude_study_shell_def* _def, i32 _argc, c8** _argv) {
    shell = _def;
#if !FUDE_SHELL_TAKES_ARGS
    // A release ignores whatever it is launched with, both in the app and in RDE (which reads arguments as config
    // overrides). Only the program's name is kept.
    _argc = _argc > 1 ? 1 : _argc;
#endif
    // The bake runs from the project root (its sources are in data/raw/...), where the app's config is under
    // apps/<app>/; that config's memory sizes hold the data files whole.
    const c8* _config = fude_bake_requested(_argc, _argv) ? _def->bake_config : FUDE_SHELL_CONFIG;
    return rde_run(_argc, _argv, _config, fude_shell_init, fude_shell_event, fude_shell_on_fixed_update, fude_shell_on_update,
                   fude_shell_on_late_update, fude_shell_on_render, fude_shell_on_crash, fude_shell_end);
}

// --- the side panel's Study section -----------------------------------------------------------

// Opens a side panel screen (see the header), first letting go of the lasso's selection. Browse and the chart are one
// or the other: opening one closes the other.
void fude_study_open(fude_app* _app, FUDE_STUDY_SCREEN_ _screen) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case FUDE_STUDY_SCREEN_BROWSE:  fude_chart_close(_study->chart);   fude_browse_open(_study->browse); break;
        case FUDE_STUDY_SCREEN_CHART:   fude_browse_close(_study->browse); fude_chart_open(_study->chart);   break;
        case FUDE_STUDY_SCREEN_EXAM:    fude_exam_open(_study->exam);       break;
        case FUDE_STUDY_SCREEN_VOCAB:   fude_vocabview_open(_study->vocab); break;
        case FUDE_STUDY_SCREEN_STATS:   fude_stats_open(_study->stats);     break;
        case FUDE_STUDY_SCREEN_ALBUM:   fude_album_open(_study->album);     break;
        case FUDE_STUDY_SCREEN_LIBRARY: fude_library_open(_study->library); break;
        default: break;
    }
}

// The side panel's entry: opens _screen and closes the panel.
b8 fude_study_nav_open(fude_app* _app, u32 _screen) {
    fude_study_open(_app, (FUDE_STUDY_SCREEN_)_screen);
    return true;
}

// The side panel's Reviews entry; the panel stays open when none are due.
b8 fude_study_nav_review(fude_app* _app, u32 _arg) {
    RDE_UNUSED(_arg);
    return fude_study_review(_app);
}

// True when the character data is loaded.
b8 fude_study_nav_has_data(const fude_app* _app) {
    return fude_browse_available(FUDE_STUDY(_app)->browse);
}

// True when the language taught has a chart of letters.
b8 fude_study_nav_has_chart(const fude_app* _app) {
    return fude_chart_count(FUDE_STUDY(_app)->chart) > 0u;
}

// True when the album has character data to show.
b8 fude_study_nav_has_album(const fude_app* _app) {
    return FUDE_STUDY(_app)->album->db != NULL;
}
