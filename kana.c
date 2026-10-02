// ===========================================================================
// Kana: Japanese written by hand. The app's shell — what RDE calls (init, the
// events, each frame's update and render, the end), and what it owns.
//
// HOW IT IS PUT TOGETHER (app.h has the whole of it). The page is at the
// bottom (page.h: writing on it, moving it); the screens stack over it in a
// fixed order (app.h: KANA_SCREEN_), and the first one open is on top: it alone
// gets the pointer, the frame and Escape, and its row of buttons shows (row.h).
// The retained UI — the toolbar, the page's menus, the screens' rows, the side
// panel, the word card — is one canvas (ui.h). What is saved, and what leaves
// as a file, is the session's (session.h); a developer's launch flags and
// measures are look.h's.
// ===========================================================================

#include "rde.h"

#include <stdio.h>
#include <string.h>

#include "app/app.h"
#include "app/ui.h"
#include "widgets/kit.h"
#include "app/page.h"
#include "app/session.h"
#include "app/look.h"
#include "widgets/notice.h"
#include "lang/ja/bake.h"
#include "base/theme.h"
#include "base/text.h"
#include "widgets/draw.h"
#include "services/mlkit.h"
#include "handwriting/match.h"
#include "study/marks.h"
#include "study/examlog.h"
#include "study/vocab.h"
#include "study/charnote.h"
#include "services/speech.h"

#define KANA_CONFIG_PATH "./assets/config.rdef"

// A developer's build (debug, RDE_DEBUG): the diagnostics HUD (H) and the raw
// pen samples (M) can be shown, and launch arguments are read (the look flags,
// --perf, the bake). A release build has none of it — it takes no arguments at
// all (main) — unless built with -DKANA_ALLOW_ARGS, to measure a release on the
// device (--perf, COMMANDS.txt). The HUD starts hidden either way, and is not
// a setting: nothing saved shows it.
#if defined(RDE_DEBUG) || defined(KANA_ALLOW_ARGS)
#define KANA_TAKES_ARGS 1
#else
#define KANA_TAKES_ARGS 0
#endif

RDE_INTERNAL rde_window*    window;
RDE_INTERNAL rde_camera     camera;
RDE_INTERNAL kana_kanji_db  kanji_db;
RDE_INTERNAL kana_ink       ink;
RDE_INTERNAL kana_canvas    canvas;
RDE_INTERNAL kana_lasso     lasso;
RDE_INTERNAL kana_notes     notes;
RDE_INTERNAL kana_selection selection;   // Browse's and the chart's ticks (select.h)
RDE_INTERNAL kana_viewer    viewer;
RDE_INTERNAL kana_browse    browse;
RDE_INTERNAL kana_chart     chart;
RDE_INTERNAL kana_practice  practice;
RDE_INTERNAL kana_album     album;
RDE_INTERNAL kana_check     check;
RDE_INTERNAL kana_exam      exam;
RDE_INTERNAL kana_stats     stats;
RDE_INTERNAL kana_scan      scan;
RDE_INTERNAL kana_vocabview vocabview;
RDE_INTERNAL kana_wordexam  wordexam;
RDE_INTERNAL kana_welcome   welcome;
RDE_INTERNAL kana_page_input page;
RDE_INTERNAL kana_ui        ui;
RDE_INTERNAL kana_app       app;

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
RDE_INTERNAL void kana_hud_line(const c8* _text, f32 _x, f32* _y, f32 _gap) {
    kana_draw_text(app.font, app.font_px, _text, _x, *_y, 17.0f, kana_theme_active()->hud);
    *_y -= _gap;
}

RDE_INTERNAL void kana_hud_render(rde_window* _window) {
    const rde_vec_2I        _size   = rde_window_get_size(_window);
    const rde_vec_4I        _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const kana_session_info _save   = kana_session_info_now();
    const f32               _x      = -(f32)_size.x * 0.5f + (f32)_insets.x + 20.0f;   // below the status bar, not under it
    f32                     _y      = (f32)_size.y * 0.5f - (f32)_insets.y - 34.0f;
    c8                      _line[192];

    kana_hud_line(ink.pen_seen ? "PEN  (stylus events arriving)" : page.touch_seen ? "TOUCH ONLY  - no pen events!" : "waiting for input...", _x, &_y, 34.0f);
    snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)%s   tilt %.0f / %.0f   pen id %u   %s", (f64)ink.pressure, (f64)ink.pressure_raw,
             !ink.pen_seen ? "" : ink.pressure_live ? " [real]" : " [none - width from speed]", (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id, ink.eraser ? "ERASER" : "tip");
    kana_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms", (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    kana_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end", (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    kana_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "%u strokes, %u points   history %u (+%u redo)", kana_ink_alive_strokes(&ink), kana_ink_total_points(&ink), kana_ink_undo_steps(&ink), kana_ink_redo_steps(&ink));
    kana_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "zoom %.0f%%   fingers: 1 pan, 2 pinch   tool: %s", (f64)(canvas.view.zoom * 100.0f),
             ui.bar.tool == KANA_TOOL_ERASE ? "ERASE" : ui.bar.tool == KANA_TOOL_LASSO ? "LASSO" : "DRAW");
    kana_hud_line(_line, _x, &_y, 28.0f);
    if(_save.save_failed) {
        snprintf(_line, sizeof(_line), "SAVE FAILED - retrying   (%s)", _save.load_note);
    } else if(_save.last_save_time > 0.0) {
        snprintf(_line, sizeof(_line), "saved %.0f s ago: %.1f KB in %.1f ms   (%s)", rde_engine_get_time_now() - _save.last_save_time,
                 (f64)_save.last_save_bytes / 1024.0, (f64)_save.last_save_ms, _save.load_note);
    } else {
        snprintf(_line, sizeof(_line), "not saved yet this run   (%s)", _save.load_note);
    }
    kana_hud_line(_line, _x, &_y, 28.0f);
    snprintf(_line, sizeof(_line), "brush: %s", ink.brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? "PAGE - same width on the page at any zoom" : "SCREEN - same width on screen while writing");
    kana_hud_line(_line, _x, &_y, 28.0f);
}

// --- init -------------------------------------------------------------------------------------

// The screens, in the order they stack (app.h: KANA_SCREEN_).
RDE_INTERNAL void kana_screens_place(void) {
    const struct { KANA_SCREEN_ id; const kana_screen* vt; void* self; } _screens[] = {
        { KANA_SCREEN_WELCOME,  &KANA_WELCOME_SCREEN,  &welcome },
        { KANA_SCREEN_PRACTICE, &KANA_PRACTICE_SCREEN, &practice },
        { KANA_SCREEN_VIEWER,   &KANA_VIEWER_SCREEN,   &viewer },
        { KANA_SCREEN_SCAN,     &KANA_SCAN_SCREEN,     &scan },
        { KANA_SCREEN_WORDEXAM, &KANA_WORDEXAM_SCREEN, &wordexam },
        { KANA_SCREEN_EXAM,     &KANA_EXAM_SCREEN,     &exam },
        { KANA_SCREEN_STATS,    &KANA_STATS_SCREEN,    &stats },
        { KANA_SCREEN_VOCAB,    &KANA_VOCAB_SCREEN,    &vocabview },
        { KANA_SCREEN_CHECK,    &KANA_CHECK_SCREEN,    &check },
        { KANA_SCREEN_ALBUM,    &KANA_ALBUM_SCREEN,    &album },
        { KANA_SCREEN_CHART,    &KANA_CHART_SCREEN,    &chart },
        { KANA_SCREEN_BROWSE,   &KANA_BROWSE_SCREEN,   &browse },
    };
    for(u32 _i = 0; _i < sizeof(_screens) / sizeof(_screens[0]); _i++) {
        app.screens[_screens[_i].id] = (kana_screen_slot){ _screens[_i].vt, _screens[_i].self };
    }
}

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    if(kana_bake_requested(_argc, _argv)) {
        baking = true;
        const i32 _rc = kana_bake_run(_argc, _argv);
        rde_log_level(_rc == 0 ? RDE_LOG_LEVEL_INFO : RDE_LOG_LEVEL_ERROR, "bake %s", _rc == 0 ? "finished" : "FAILED");
        // Not rde_engine_destroy_engine: rde_run carries on after init_func.
        rde_engine_set_running(false);
        return;
    }

    window = _window;
    // The words first: the UI is built in them. The device's language when Kana
    // speaks it; a saved choice replaces it (session.h).
    kana_text_set_language(kana_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    kana_look_args(_argc, _argv);

    // The page.
    kana_ink_init(&ink);
    kana_canvas_init(&canvas);
    kana_lasso_init(&lasso);
    kana_notes_init(&notes);

    // The baked character data (see bake.h). Without it the app still draws; the
    // screens that need it just stay empty.
    const b8             _have = kana_kanji_load(&kanji_db, KANA_KANJI_FILE);
    const kana_kanji_db* _db   = _have ? &kanji_db : NULL;
    kana_viewer_init(&viewer, _db);
    kana_browse_init(&browse, _db);
    kana_chart_init(&chart, _db);
    kana_selection_init(&selection, _have ? kanji_db.count : 0u);
    kana_exam_init(&exam, _db, &browse.catalog);
    kana_vocabview_init(&vocabview, _db);
    kana_wordexam_init(&wordexam, _db, &browse.catalog);
    kana_stats_init(&stats, _db, &browse.catalog);
    kana_scan_init(&scan);
    scan.db          = _db;   // the lines' words (wordsplit.h)
    browse.selection = &selection;
    chart.selection  = &selection;
    kana_practice_init(&practice, _db);
    kana_album_init(&album, _db);
    kana_check_init(&check, _db, &browse.catalog);
    if(_have) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "kana: %u characters loaded", kanji_db.count);
    }

    // The app: what it holds, the screens in order, the page, the UI over it all.
    app = (kana_app){
        .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f, .db = _have ? &kanji_db : NULL, .catalog = &browse.catalog,
        .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
        .viewer = &viewer, .browse = &browse, .chart = &chart, .practice = &practice, .album = &album, .check = &check, .exam = &exam,
        .stats = &stats, .scan = &scan, .vocab = &vocabview, .wordexam = &wordexam, .welcome = &welcome, .selection = &selection,
    };
#if defined(RDE_PLATFORM_MOBILE)
    app.finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    kana_screens_place();
    kana_page_init(&page, &app);
    kana_ui_init(&ui, &app);
    if(ui.font != NULL) {
        app.font    = ui.font;   // the screens' text: the UI font, in screen units, scaled from the size it was loaded at
        app.font_px = (f32)KANA_KIT_FONT_SIZE;
    }

    kana_look_start(&app);
    kana_session_load(&app);
    kana_look_loaded(&app);
    kana_mlkit_prepare();   // after the settings: when ML Kit is on, its model downloads the first time (iOS; nothing elsewhere)
    rde_log_color(RDE_LOG_COLOR_GREEN, "%s", "kana: the pen writes, fingers move the page; the toolbar has the rest.");
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
        kana_ui_trim_fonts(&ui);
    }
    // Leaving: save now. The autosave (session.h) is the guarantee — this event
    // reaches here through the queue and may not be handled before the app is
    // suspended. The camera stops: what was in view stays.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        kana_session_save_on_exit(&app);
        if(scan.open) {
            kana_scan_pause(&scan);
        }
    }
    // A screen on top has the pointer (its buttons are UI and have had the event
    // already); the page has it otherwise.
    if(kana_app_top(&app) != NULL) {
        if(page.finger_state != 0u || page.erasing || page.pen_on_ui || ink.drawing) {
            kana_page_let_go(&page);   // a screen over the page: what was writing there ends
        }
        kana_app_screen_event(&app, _event);
    } else {
        kana_page_event(&page, _event);
    }
}

// --- each frame -------------------------------------------------------------------------------

RDE_INTERNAL void kana_update(f32 _dt) {
    if(baking) {
        return;
    }
    kana_look_frame(&app);
    // A language chosen: the UI and the meanings in it (English where it has none).
    static u32 _language_seen = 0;
    kana_ui_follow_language(&ui);
    if(_language_seen != kana_text_revision()) {
        _language_seen = kana_text_revision();
        if(app.db != NULL) {
            const RDE_LANGUAGE_ _l = kana_text_language();
            kana_kanji_set_language(&kanji_db, _l == RDE_LANGUAGE_ES_ES ? "es" : _l == RDE_LANGUAGE_PT_BR ? "pt" : _l == RDE_LANGUAGE_FR_FR ? "fr" : NULL);
            kana_browse_language_changed(&browse);
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
    if(ui.side.welcome_request) {   // Settings › the welcome again
        ui.side.welcome_request = false;
        kana_welcome_open(&welcome);
    }
    if(kana_speech_take_hint()) {
        kana_notice_show(kana_text(KANA_TEXT_SPEECH_BETTER_VOICE));   // the basic voice spoke: where better ones are
    }

    kana_ink_frame_begin(&ink);
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

    // The screen on top has the frame; the page, when there is none.
    if(!kana_app_update(&app, _dt)) {
        kana_page_update(&page);
    }
    kana_ui_update(&ui);
    kana_session_update(&app);
}

RDE_INTERNAL void kana_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(baking) {
        return;
    }
    // EVERY frame: the engine resets the clear colour at the start of each one,
    // and a frame nobody sets it for clears to black.
    rde_rendering_clear_background_color(kana_theme_active()->page);
    rde_rendering_2d_begin_drawing(_window, &camera);
    if(!kana_app_render(&app)) {
        kana_page_render(&page, _window);
        if(page.show_hud && app.font != NULL) {
            kana_hud_render(_window);
        }
    }
    kana_app_render_overlays(&app);   // the welcome, over everything
    // The notice: over the page's bottom, or over a screen's row.
    const KANA_SCREEN_ _top = kana_app_top_id(&app);
    kana_notice_render(_window, app.font, app.font_px, _top == KANA_SCREEN_COUNT ? 72.0f : 8.0f + kana_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

void on_update(f32 _dt) {
    kana_look_timed_update(kana_update, _dt);
}

void on_render(rde_window* _window, f32 _dt) {
    kana_look_timed_render(&app, kana_render, _window, _dt);
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
    kana_session_save_on_exit(&app);
    kana_ui_destroy(&ui);
    kana_lasso_destroy(&lasso);
    kana_viewer_destroy(&viewer);
    kana_browse_destroy(&browse);
    kana_chart_destroy(&chart);
    kana_selection_destroy(&selection);
    kana_exam_destroy(&exam);
    kana_wordexam_destroy(&wordexam);
    kana_vocabview_destroy(&vocabview);
    kana_stats_destroy(&stats);
    kana_scan_destroy(&scan);
    kana_marks_close();
    kana_examlog_close();
    kana_vocab_close();
    kana_charnotes_close();
    kana_practice_destroy(&practice);
    kana_album_destroy(&album);
    kana_notes_destroy(&notes);
    kana_check_destroy(&check);
    kana_match_release();
    kana_kanji_unload(&kanji_db);
    kana_ink_destroy(&ink);
}

int main(i32 _argc, c8* _argv[]) {
#if !KANA_TAKES_ARGS
    // A release: whatever it is launched with is ignored — by Kana and by RDE
    // (which reads arguments as config overrides). Only the program's name.
    _argc = _argc > 1 ? 1 : _argc;
#endif
    return rde_run(_argc, _argv, KANA_CONFIG_PATH, init_func, on_event, on_fixed_update, on_update, on_late_update, on_render, on_crash, end_func);
}
