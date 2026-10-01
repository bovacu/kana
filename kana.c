// ===========================================================================
// kana — milestone 1: the ink spike.
//
// WHAT THIS IS FOR, and it is not "a drawing app". It answers the two questions
// that can invalidate the whole project, and it answers them on real hardware
// because neither can be answered by reading code:
//
//   1. DO PEN EVENTS ARRIVE? RDE exposes SDL3's pen API, but whether an Apple
//      Pencil on an iPad actually produces RDE_EVENT_TYPE_PEN_* is unverified.
//      If it does not, the shared-ink plan is dead and iOS needs a native input
//      path. The HUD says PEN or TOUCH in the largest text on screen.
//
//   2. DOES IT FEEL ATTACHED TO THE NIB? Below roughly 30 ms of lag the ink
//      feels stuck to the pen; above it, it swims and the app feels cheap no
//      matter how good the rest is.
//
// WHAT THE NUMBERS CAN AND CANNOT TELL YOU. Absolute pen-to-photon latency is
// NOT measurable from inside the process — it needs a high-speed camera pointed
// at the glass. What is measurable is the app's own half, and the one number
// worth staring at is INPUT HZ: a pen sampling at 120-240 Hz that arrives here
// at ~60 is being coalesced to the frame rate, and fast strokes will come out as
// visible polygons however low the lag is. Press M and look at the dot spacing
// on a fast stroke — that is the same fact, seen instead of counted.
//
// CONTROLS
//   PEN            writes (or erases, with the Erase tool or the pen's eraser end;
//                  with the Lasso tool, loops strokes to select, then drags them)
//   1 FINGER       pans the page;  2 FINGERS  pinch-zoom
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
//   1-FINGER HOLD  the page's context menu (Paste, Select all)
//   TOOLBAR        everything else
// Keys, for the desktop: C clear, Z undo, Y redo, M raw samples, H HUD, R reset
// view, B brush scale, Backspace/Delete deletes the selection, Esc deselects;
// right-click is the context menu.
// ===========================================================================

#include "rde.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink.h"
#include "canvas.h"
#include "toolbar.h"
#include "text.h"
#include "lasso.h"
#include "save.h"
#include "bake.h"
#include "kanji.h"
#include "exam.h"
#include "stats.h"
#include "examlog.h"
#include "marks.h"
#include "userwords.h"
#include "viewer.h"
#include "theme.h"
#include "draw.h"
#include "recognize.h"

#define KANA_CONFIG_PATH "./assets/config.rdef"

RDE_INTERNAL rde_window* window;
RDE_INTERNAL rde_camera  camera;
// All of Kana's own text (HUD, viewer) uses the UI font — Roboto with Slug, the
// toolbar's — at sizes in screen units, scaled from the size it was loaded at.
// The engine's default atlas font is only the fallback if that failed to load.
RDE_INTERNAL rde_font*   font;
RDE_INTERNAL f32         font_px = 14.0f;
RDE_INTERNAL kana_ink    ink;
RDE_INTERNAL kana_canvas canvas;
RDE_INTERNAL kana_toolbar toolbar;
RDE_INTERNAL kana_lasso   lasso;
RDE_INTERNAL kana_kanji_db kanji_db;
RDE_INTERNAL kana_viewer  viewer;
RDE_INTERNAL kana_browse  browse;
RDE_INTERNAL kana_chart   chart;
RDE_INTERNAL kana_selection selection;   // Browse's and the chart's ticks (select.h)
RDE_INTERNAL kana_exam      exam;        // exams (exam.h), over Browse, the chart and the album
RDE_INTERNAL kana_stats     stats;       // statistics (stats.h)
RDE_INTERNAL kana_practice practice;
RDE_INTERNAL kana_album    album;
RDE_INTERNAL kana_check    check;                         // the lasso's selection, checked (check.h)

// --mlkit-samples=0-27,28-58,...: a developer's measure of ML Kit. Each range of
// strokes of the open canvas is read by ML Kit in turn, and the answers written
// to <save dir>/mlkit_samples.txt (copied off the device to compare).
#define KANA_MLKIT_SAMPLES 32
RDE_INTERNAL u32 mlkit_sample_first[KANA_MLKIT_SAMPLES];
RDE_INTERNAL u32 mlkit_sample_last[KANA_MLKIT_SAMPLES];
RDE_INTERNAL u32 mlkit_sample_count;
RDE_INTERNAL u32 mlkit_sample_next;
RDE_INTERNAL b8  mlkit_sample_asked;
RDE_INTERNAL rde_vec_2F    list_last;                     // where the scenes' pointer last was

// Browse, the chart and Practice follow one pointer at a time: whichever pressed first.
typedef enum { KANA_POINTER_NONE = 0, KANA_POINTER_PEN, KANA_POINTER_FINGER, KANA_POINTER_MOUSE } KANA_POINTER_;
RDE_INTERNAL KANA_POINTER_ browse_pointer = KANA_POINTER_NONE;
RDE_INTERNAL u64           browse_finger  = 0;

// Running as the offline data bake (--bake, desktop): nothing else is set up, so
// every callback returns at once.
RDE_INTERNAL b8 baking = false;
RDE_INTERNAL u8 settings_language = 0;   // the saved language (RDE_LANGUAGE_; 0: never chosen, the device's)
// --paper (desktop looks): the paper panel opened a few frames in.
RDE_INTERNAL b8  look_paper  = false;
RDE_INTERNAL u32 look_frames = 0;
// --shot=FILE: a screenshot of the window once the screen has settled, then quit
// (with the other look flags: a screen, a size, a theme — checked without a device).
RDE_INTERNAL const c8* look_shot        = NULL;
RDE_INTERNAL u32       look_shot_frames = 0;
// Look flags that need the saves (the exams, the marks): acted on once they are loaded.
RDE_INTERNAL b8        look_stats       = false;
RDE_INTERNAL i64       look_kept_exam   = -1;
RDE_INTERNAL f32       look_scroll      = 0.0f;   // --scroll=PX: the open screen scrolled down (Statistics, the album)
RDE_INTERNAL i32       look_theme       = -1;     // --theme=N: shown, not saved
RDE_INTERNAL const c8* look_paste_text  = NULL;   // --paste-text=TEXT: Paste text, as if the clipboard held it, mid-page
RDE_INTERNAL b8        look_deselect    = false;  // --deselect: and nothing left selected after it

// --perf=N: frame times over N seconds (after a second to settle), appended to
// <save dir>/perf.txt with the flags it ran with — to measure a build on the
// device, launched on a screen (--browse, --stats...) from the Mac.
RDE_INTERNAL f64 perf_seconds = 0.0;   // 0: off
RDE_INTERNAL f64 perf_start   = 0.0;   // when measuring began (0: not yet)
RDE_INTERNAL u32 perf_frames  = 0;
RDE_INTERNAL u32 perf_settle  = 0;     // frames before measuring
RDE_INTERNAL u32 perf_slow    = 0;     // frames over 20 ms
RDE_INTERNAL f64 perf_dt_sum  = 0.0, perf_dt_max  = 0.0;
RDE_INTERNAL f64 perf_update_sum = 0.0, perf_update_max = 0.0;   // on_update's time
RDE_INTERNAL f64 perf_render_sum = 0.0, perf_render_max = 0.0;   // on_render's
RDE_INTERNAL c8  perf_label[160];

// The pen went down ON the toolbar: it is pressing a button, so nothing it does
// until it lifts may write, erase or pan.
RDE_INTERNAL b8 pen_on_ui = false;
// The pen (or mouse) is down with the Erase tool: its path erases.
RDE_INTERNAL b8 erasing   = false;

RDE_INTERNAL b8  show_samples = false;
RDE_INTERNAL b8  show_hud     = true;

// Frame time, smoothed. The raw value jitters too much to read off a screen.
RDE_INTERNAL f32 frame_ms = 0.0f;

// The zoom, shown for a moment at the bottom-right whenever it changes.
#define KANA_ZOOM_TOAST_TIME 1.2    // seconds it stays...
#define KANA_ZOOM_TOAST_FADE 0.4    // ...then fades out over this
#define KANA_NOTICE_TIME     2.6    // the toolbar's notice (Copy as text, Paste text) stays...
#define KANA_NOTICE_FADE     0.4    // ...then fades out over this
RDE_INTERNAL f32 zoom_seen     = -1.0f;   // < 0: not seen yet (no toast for the loaded zoom)
RDE_INTERNAL f64 zoom_shown_at = -100.0;

// --- saving --------------------------------------------------------------------
//
// The page and the settings save themselves this long after the last change, and
// never mid-stroke — so an iPadOS kill loses at most this much writing. Also on
// going to the background, but that event reaches on_event through the queue and
// may not be handled before the app is suspended: the autosave is the guarantee.
#define KANA_AUTOSAVE_DELAY 1.0

RDE_INTERNAL c8            document_path[RDE_MAX_PATH];   // the open canvas's page
RDE_INTERNAL kana_notes    notes;                         // the canvases and their folders (notes.h)
RDE_INTERNAL u32           current_canvas = 0;            // whose page is in `ink`
RDE_INTERNAL c8            settings_path[RDE_MAX_PATH];

// What was last written, and what was last seen (to time the quiet period).
RDE_INTERNAL u32           saved_revision = 0;
RDE_INTERNAL kana_view     saved_view;
RDE_INTERNAL kana_page     saved_page;
RDE_INTERNAL kana_settings saved_settings;
RDE_INTERNAL u32           seen_revision  = 0;
RDE_INTERNAL kana_view     seen_view;
RDE_INTERNAL kana_page     seen_page;
RDE_INTERNAL kana_settings seen_settings;
RDE_INTERNAL f64           last_change_time = 0.0;

// For the HUD.
RDE_INTERNAL f64           last_save_time  = 0.0;
RDE_INTERNAL f32           last_save_ms    = 0.0f;
RDE_INTERNAL u32           last_save_bytes = 0;
RDE_INTERNAL b8            save_failed     = false;
RDE_INTERNAL c8            load_note[160]  = "";

// Set once a touch has drawn anything, so the HUD can distinguish "no input at
// all" (nothing is reaching the app) from "touch only" (input works, pen does
// not) — which are completely different failures.
RDE_INTERNAL b8  touch_seen = false;

// --- Window pixels to 2D world ------------------------------------------------
//
// Pen positions arrive WINDOW-RELATIVE: top-left origin, Y down. The 2D camera is
// centre-origin with Y up. Every pen position has to make this trip, and getting
// it wrong shows as ink mirrored about the middle of the screen rather than as
// anything subtle.
RDE_INTERNAL rde_vec_2F kana_window_to_world(rde_vec_2F _pixel) {
    const rde_vec_2I _size = rde_window_get_size(window);

    return (rde_vec_2F){
        _pixel.x - (f32)_size.x * 0.5f,
        (f32)_size.y * 0.5f - _pixel.y
    };
}

// Where the pen (or mouse) is on the PAGE. Also hands the ink the current zoom,
// which it needs to store widths in canvas units.
RDE_INTERNAL rde_vec_2F kana_screen_to_canvas(rde_vec_2F _screen) {
    ink.zoom = canvas.view.zoom;
    return kana_canvas_from_screen(&canvas, _screen);
}

// HUD text, in the theme's HUD colour, this many screen units tall.
#define KANA_HUD_TEXT_PX    17.0f

// Touch events report centre-origin with Y DOWN; the screen space is Y up.
RDE_INTERNAL rde_vec_2F kana_touch_to_screen(rde_vec_2I _touch) {
    return (rde_vec_2F){ (f32)_touch.x, -(f32)_touch.y };
}

// B on a keyboard; the toolbar has the same switch.
RDE_INTERNAL void kana_toggle_brush_scale(void) {
    ink.brush_scale = ink.brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    kana_toolbar_sync(&toolbar);
}

// The eraser's reach, in SCREEN units: a fingertip-sized circle at any zoom.
#define KANA_ERASER_RADIUS 10.0f

RDE_INTERNAL void kana_erase_at_screen(rde_vec_2F _screen) {
    kana_ink_erase_at(&ink, kana_canvas_from_screen(&canvas, _screen), KANA_ERASER_RADIUS / canvas.view.zoom);
}

RDE_INTERNAL b8 kana_page_same(kana_page _a, kana_page _b) {
    return _a.paper == _b.paper;
}

RDE_INTERNAL b8 kana_view_same(kana_view _a, kana_view _b) {
    return memcmp(&_a, &_b, sizeof(kana_view)) == 0;
}

RDE_INTERNAL kana_settings kana_gather_settings(void) {
    kana_settings _s;
    memset(&_s, 0, sizeof(_s));
    _s.tool           = (u8)toolbar.tool;
    _s.vertical       = toolbar.vertical;
    _s.show_hud       = show_hud;
    _s.brush_scale    = (u8)ink.brush_scale;
    _s.width_mode     = (u8)ink.width_mode;
    _s.color          = ink.color;
    _s.radius         = ink.constant_radius;
    _s.toolbar_center = toolbar.center;
    _s.theme          = (u8)kana_theme_index();
    _s.mlkit          = kana_mlkit_enabled();
    _s.toolbar_minimized = toolbar.minimized;
    _s.paper_size      = (u8)canvas.paper_size;
    // The language, once one other than the device's is chosen (0 until then: the device's).
    _s.language        = settings_language != 0 || kana_text_language() != kana_text_default_language() ? (u8)kana_text_language() : 0u;
    return _s;
}

RDE_INTERNAL void kana_apply_settings(const kana_settings* _s) {
    toolbar.tool         = _s->tool == KANA_TOOL_ERASE ? KANA_TOOL_ERASE : _s->tool == KANA_TOOL_LASSO ? KANA_TOOL_LASSO : KANA_TOOL_DRAW;
    show_hud             = _s->show_hud;
    ink.brush_scale      = _s->brush_scale == KANA_INK_BRUSH_SCALE_SCREEN ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    ink.width_mode       = _s->width_mode == KANA_INK_WIDTH_MODE_PRESSURE ? KANA_INK_WIDTH_MODE_PRESSURE : KANA_INK_WIDTH_MODE_CONSTANT;
    ink.color            = _s->color;
    ink.constant_radius  = rde_math_clamp_f32(_s->radius, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
    kana_theme_set((KANA_THEME_)_s->theme);
    kana_mlkit_set_enabled(_s->mlkit);
    canvas.paper_size   = _s->paper_size < KANA_PAPER_SIZE_COUNT ? (KANA_PAPER_SIZE_)_s->paper_size : KANA_PAPER_MEDIUM;
    settings_language   = _s->language;
    if(_s->language != 0 && (RDE_LANGUAGE_)_s->language != kana_text_language()) {
        kana_text_set_language((RDE_LANGUAGE_)_s->language);   // the UI follows next frame
    }
    kana_toolbar_set_placement(&toolbar, _s->vertical, _s->toolbar_center, _s->toolbar_minimized);
    kana_toolbar_sync(&toolbar);   // also restyles it in the theme
}

// Writes whatever changed since the last save (_force: both files regardless).
// Not while a stroke is open — the caller closes it first when it must save now.
RDE_INTERNAL void kana_save_now(b8 _force) {
    if(ink.drawing) {
        return;
    }

    const kana_settings _settings = kana_gather_settings();
    const b8 _doc_dirty  = _force || ink.revision != saved_revision || !kana_view_same(canvas.view, saved_view) || !kana_page_same(canvas.page, saved_page);
    const b8 _set_dirty  = _force || !kana_settings_equal(&_settings, &saved_settings);

    if(!_doc_dirty && !_set_dirty) {
        return;
    }

    const f64 _start = rde_engine_get_time_now();
    b8        _ok    = true;

    if(_doc_dirty) {
        u32 _bytes = 0;
        if(kana_save_document(document_path, &ink, canvas.view, canvas.page, &_bytes)) {
            saved_revision  = ink.revision;
            saved_view      = canvas.view;
            saved_page      = canvas.page;
            last_save_bytes = _bytes;
        } else {
            _ok = false;
        }
    }

    if(_set_dirty) {
        if(kana_save_settings(settings_path, &_settings)) {
            saved_settings = _settings;
        } else {
            _ok = false;
        }
    }

    const f64 _now = rde_engine_get_time_now();
    last_save_ms   = (f32)((_now - _start) * 1000.0);
    last_save_time = _now;
    save_failed    = !_ok;

    // A failure retries after another quiet period rather than every frame.
    if(!_ok) {
        last_change_time = _now;
    }
}

// Once a frame: note any change, and save once things have been quiet a while.
RDE_INTERNAL void kana_autosave(void) {
    const f64           _now      = rde_engine_get_time_now();
    const kana_settings _settings = kana_gather_settings();

    if(ink.revision != seen_revision || !kana_view_same(canvas.view, seen_view) || !kana_page_same(canvas.page, seen_page) ||
       !kana_settings_equal(&_settings, &seen_settings)) {
        seen_revision    = ink.revision;
        seen_view        = canvas.view;
        seen_page        = canvas.page;
        seen_settings    = _settings;
        last_change_time = _now;
        return;
    }

    if(!ink.drawing && !kana_lasso_busy(&lasso) && _now - last_change_time >= KANA_AUTOSAVE_DELAY) {
        kana_save_now(false);
    }
}

// The app is going away (background, terminate, quit): finish what the pen was
// doing and write everything now.
RDE_INTERNAL void kana_save_on_exit(void) {
    kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
    kana_ink_erase_end(&ink);
    kana_ink_end(&ink);
    pen_on_ui = false;
    erasing   = false;
    kana_save_now(false);
}

RDE_INTERNAL void kana_load_saves(void) {
    const c8* _dir = kana_save_dir();
    snprintf(settings_path, sizeof(settings_path), "%s%s", _dir, KANA_SAVE_SETTINGS_FILE);

    // The canvases (the first time, the old page.kana becomes the first one).
    kana_notes_load(&notes);
    current_canvas = notes.open;
    kana_notes_canvas_path(current_canvas, document_path, sizeof(document_path));

    // The study marks and every exam (marks.h, examlog.h).
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%smarks.kana", _dir);
    kana_marks_open(_path);
    snprintf(_path, sizeof(_path), "%sexams.kana", _dir);
    kana_examlog_open(_path);
    snprintf(_path, sizeof(_path), "%swords.kana", _dir);
    kana_userwords_open(_path);

    kana_settings    _settings = kana_gather_settings();
    const KANA_LOAD_ _loaded   = kana_load_settings(settings_path, &_settings);
    if(_loaded == KANA_LOAD_OK) {
        kana_apply_settings(&_settings);
    } else if(_loaded == KANA_LOAD_MISSING) {
        // The very first time: the theme the device is in — Night when it is
        // dark, Paper otherwise, as the launch screen was — and the settings
        // saved at once (below), so from now on the theme is the learner's.
        kana_theme_set(rde_engine_get_system_theme() == RDE_SYSTEM_THEME_DARK ? KANA_THEME_NIGHT : KANA_THEME_PAPER);
        kana_toolbar_sync(&toolbar);
    }

    const KANA_LOAD_ _doc = kana_load_document(document_path, &ink, &canvas.view, &canvas.page);
    if(_doc == KANA_LOAD_OK) {
        snprintf(load_note, sizeof(load_note), "loaded %u strokes", kana_ink_alive_strokes(&ink));
    } else if(_doc == KANA_LOAD_RECOVERED) {
        snprintf(load_note, sizeof(load_note), "PAGE FILE MISSING/DAMAGED - loaded the backup, %u strokes", kana_ink_alive_strokes(&ink));
    } else if(_doc == KANA_LOAD_CORRUPT) {
        snprintf(load_note, sizeof(load_note), "PAGE FILE DAMAGED - kept as .bad, starting empty");
    } else {
        snprintf(load_note, sizeof(load_note), "new page");
    }
    rde_log_color(RDE_LOG_COLOR_GREEN, "kana: saves in %s (%s)", _dir, load_note);

    // What is on screen now IS what is saved.
    saved_revision   = seen_revision  = ink.revision;
    saved_view       = seen_view      = canvas.view;
    saved_page       = seen_page      = canvas.page;
    saved_settings   = seen_settings  = kana_gather_settings();
    last_change_time = rde_engine_get_time_now();
    if(_loaded == KANA_LOAD_MISSING && !kana_save_settings(settings_path, &saved_settings)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not write the first settings (%s)", settings_path);
    }
}

// The canvas opened in the side panel (notes.open changed): the page leaving is
// saved — unless it was just deleted — and the new one loaded, with its own view
// and a fresh history. The brush is a setting, not the page's: it carries over;
// so does the lasso's clipboard (copy on one canvas, paste on another).
RDE_INTERNAL void kana_switch_canvas(void) {
    kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
    kana_ink_erase_end(&ink);
    kana_ink_end(&ink);
    pen_on_ui = false;
    erasing   = false;
    kana_canvas_release_fingers(&canvas);
    if(kana_notes_find(&notes, current_canvas) != NULL) {
        kana_save_now(false);
    }
    kana_lasso_clear(&lasso, &ink);

    const KANA_INK_BRUSH_SCALE_ _scale  = ink.brush_scale;
    const KANA_INK_WIDTH_MODE_  _width  = ink.width_mode;
    const rde_color             _color  = ink.color;
    const f32                   _radius = ink.constant_radius;
    kana_ink_destroy(&ink);
    kana_ink_init(&ink);
    ink.brush_scale     = _scale;
    ink.width_mode      = _width;
    ink.color           = _color;
    ink.constant_radius = _radius;

    kana_canvas_reset_view(&canvas);
    canvas.page    = (kana_page){ 0 };   // the next page's own, or nothing (a new canvas)
    current_canvas = notes.open;
    kana_notes_canvas_path(current_canvas, document_path, sizeof(document_path));
    kana_load_document(document_path, &ink, &canvas.view, &canvas.page);

    saved_revision   = seen_revision = ink.revision;
    saved_view       = seen_view     = canvas.view;
    saved_page       = seen_page     = canvas.page;
    last_change_time = rde_engine_get_time_now();
    zoom_seen        = -1.0f;   // the loaded zoom is not a change to show
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
    font   = rde_font_get_default_missing();
    // The words first: the UI is built in them. The device's language when Kana
    // speaks it; a saved choice replaces it (kana_apply_settings).
    kana_text_set_language(kana_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);

    kana_ink_init(&ink);
    kana_canvas_init(&canvas);
    kana_lasso_init(&lasso);

    // The baked character data (see bake.h). Without it the app still draws; the
    // Kanji button just stays greyed out.
    const b8 _have_kanji = kana_kanji_load(&kanji_db, KANA_KANJI_FILE);
    kana_viewer_init(&viewer, _have_kanji ? &kanji_db : NULL);
    kana_browse_init(&browse, _have_kanji ? &kanji_db : NULL);
    kana_chart_init(&chart, _have_kanji ? &kanji_db : NULL);
    kana_selection_init(&selection, _have_kanji ? kanji_db.count : 0u);
    kana_exam_init(&exam, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    kana_stats_init(&stats, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    browse.selection = &selection;
    chart.selection  = &selection;
    kana_practice_init(&practice, _have_kanji ? &kanji_db : NULL);
    kana_album_init(&album, _have_kanji ? &kanji_db : NULL);
    kana_notes_init(&notes);
    kana_check_init(&check, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    if(_have_kanji) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "kana: %u characters loaded", kanji_db.count);
    }

    kana_toolbar_init(&toolbar, _window, &ink, &canvas, &lasso, &viewer, &browse, &chart, &practice, &album, &notes, &check, &show_hud);
    toolbar.selection = &selection;
    toolbar.exam      = &exam;
    toolbar.stats     = &stats;
    if(toolbar.font != NULL) {
        font    = toolbar.font;
        font_px = (f32)KANA_TOOLBAR_FONT_SIZE;
    }
    kana_draw_set_icon_fill(toolbar.font_icons_fill, (f32)KANA_TOOLBAR_FONT_SIZE);   // the screens' filled icons (the marks)

    // Development: --browse / --kana open Browse / the kana chart at start;
    // --viewer=6728 the viewer on that code point (hex); --practice=6728 Practice,
    // --guided=6728 guided; --settings Settings; --licences=3 Settings and
    // Licences on document 3.
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--mlkit-samples=", 16) == 0) {
            const c8* _p = _argv[_i] + 16;
            while(*_p != 0 && mlkit_sample_count < KANA_MLKIT_SAMPLES) {
                c8* _end = NULL;
                mlkit_sample_first[mlkit_sample_count] = (u32)strtoul(_p, &_end, 10);
                mlkit_sample_last[mlkit_sample_count]  = *_end == '-' ? (u32)strtoul(_end + 1, &_end, 10) : mlkit_sample_first[mlkit_sample_count];
                mlkit_sample_count++;
                _p = *_end == ',' ? _end + 1 : _end;
                if(*_end == 0) { break; }
            }
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--settings") == 0) {
            kana_side_open_settings(&toolbar, -1);
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--licences=", 11) == 0) {
            kana_side_open_settings(&toolbar, (i32)strtol(_argv[_i] + 11, NULL, 10));
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--album") == 0) {
            kana_album_open(&album);
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--album-exams") == 0) {
            kana_album_open(&album);
            kana_album_set_view(&album, KANA_ALBUM_VIEW_EXAMS);
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--album-page=", 13) == 0) {
            kana_album_open(&album);
            kana_album_open_page(&album, (u32)strtoul(_argv[_i] + 13, NULL, 16));
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--kept-exam=", 12) == 0) {
            kana_album_open(&album);
            kana_album_set_view(&album, KANA_ALBUM_VIEW_EXAMS);
            look_kept_exam = (i64)strtoul(_argv[_i] + 12, NULL, 10);
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--browse") == 0) {
            kana_browse_open(&browse);
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--kana") == 0) {
            kana_chart_open(&chart);
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--guided=", 9) == 0) {
            u32 _record = 0;
            if(_have_kanji && kana_kanji_find_index(&kanji_db, (u32)strtoul(_argv[_i] + 9, NULL, 16), &_record)) {
                kana_practice_open(&practice, _record);
                kana_practice_set_guided(&practice, true);
            }
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--practice=", 11) == 0) {
            u32 _record = 0;
            if(_have_kanji && kana_kanji_find_index(&kanji_db, (u32)strtoul(_argv[_i] + 11, NULL, 16), &_record)) {
                kana_practice_open(&practice, _record);
            }
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--viewer=", 9) == 0) {
            kana_viewer_show_codepoint(&viewer, (u32)strtoul(_argv[_i] + 9, NULL, 16));
        }
        // Looks, for the desktop: --size=744x1133 the window (an iPad mini's
        // points), --theme=2 a theme, --side / --paper / --stats / --exam /
        // --exam-start (an N5 exam, writing) / --album-exams / --album-page=HEX /
        // --kept-exam=N a screen, --shot=FILE a screenshot of it (then quit).
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--size=", 7) == 0) {
            c8*       _end = NULL;
            const i32 _w   = (i32)strtol(_argv[_i] + 7, &_end, 10);
            const i32 _h   = _end != NULL && *_end == 'x' ? (i32)strtol(_end + 1, NULL, 10) : 0;
            if(_w > 0 && _h > 0) {
                rde_window_set_size(_window, (rde_vec_2I){ _w, _h });
            }
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--theme=", 8) == 0) {
            look_theme = (i32)strtol(_argv[_i] + 8, NULL, 10);
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--side") == 0) {
            toolbar.side.open = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--paper") == 0) {
            look_paper = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--shot=", 7) == 0) {
            look_shot = _argv[_i] + 7;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--paste-text=", 13) == 0) {
            // "\\n" for a line break: RDE reads its arguments as config lines.
            static c8 _pasted[512];
            usize     _n = 0;
            for(const c8* _p = _argv[_i] + 13; *_p != 0 && _n + 1u < sizeof(_pasted); _p++) {
                if(_p[0] == '\\' && _p[1] == 'n') { _pasted[_n++] = '\n'; _p++; }
                else                               { _pasted[_n++] = *_p; }
            }
            _pasted[_n]     = 0;
            look_paste_text = _pasted;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--deselect") == 0) {
            look_deselect = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--scroll=", 9) == 0) {
            look_scroll = strtof(_argv[_i] + 9, NULL);
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--perf=", 7) == 0) {
            perf_seconds = strtod(_argv[_i] + 7, NULL);
            for(i32 _k = 1; _k < _argc; _k++) {
                if(_argv[_k] != NULL && strncmp(_argv[_k], "--perf=", 7) != 0 && strlen(perf_label) + strlen(_argv[_k]) + 2u < sizeof(perf_label)) {
                    strcat(perf_label, _argv[_k]);
                    strcat(perf_label, " ");
                }
            }
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--stats") == 0) {
            look_stats = true;
        }
        if(_argv[_i] != NULL && (strcmp(_argv[_i], "--exam") == 0 || strcmp(_argv[_i], "--exam-start") == 0)) {
            kana_exam_open(&exam);
            if(strcmp(_argv[_i], "--exam-start") == 0) {
                exam.source = KANA_EXAM_SOURCE_N5;
                kana_exam_preview(&exam);
                kana_exam_start(&exam);
            }
        }
    }

    kana_load_saves();
    if(look_theme >= 0) {
        kana_theme_set((KANA_THEME_)look_theme);
        kana_toolbar_sync(&toolbar);
        saved_settings = seen_settings = kana_gather_settings();   // a look, not a change to save
    }
    if(look_stats) {
        kana_stats_open(&stats);
    }
    if(look_kept_exam >= 0) {
        kana_exam_open_kept(&exam, (u32)look_kept_exam);
    }
    kana_mlkit_prepare();   // after the settings: when ML Kit is on, its model downloads the first time (iOS; nothing elsewhere)

    rde_log_color(RDE_LOG_COLOR_GREEN, "%s",
                  "kana - ink spike. Pen writes, fingers move the page (2-finger tap undo, 3 redo), the toolbar has the rest. Keys: C clear, Z undo, Y redo, M raw samples, H HUD, R reset view, B brush scale.");
}

// A pointer for the viewer (over the others), Check, Browse, the chart or the
// album, whichever is open.
RDE_INTERNAL void kana_list_down(rde_vec_2F _screen, b8 _pen, f64 _now) {
    list_last = _screen;
    if(viewer.open)     { kana_viewer_pointer_down(&viewer, _screen, _now); }
    else if(exam.open)  { kana_exam_pointer_down(&exam, _screen, _pen, _now); }
    else if(stats.open) { kana_stats_pointer_down(&stats, _screen, _now); }
    else if(check.open) { kana_check_pointer_down(&check, _screen, _now); }
    else if(album.open) { kana_album_pointer_down(&album, _screen, _now); }
    else if(chart.open) { kana_chart_pointer_down(&chart, _screen, _now); }
    else                { kana_browse_pointer_down(&browse, _screen, _pen, _now); }
}
RDE_INTERNAL void kana_list_moved(rde_vec_2F _screen, f64 _now) {
    list_last = _screen;
    if(viewer.open)     { kana_viewer_pointer_moved(&viewer, _screen, _now); }
    else if(exam.open)  { kana_exam_pointer_moved(&exam, _screen, _now); }
    else if(stats.open) { kana_stats_pointer_moved(&stats, _screen, _now); }
    else if(check.open) { kana_check_pointer_moved(&check, _screen, _now); }
    else if(album.open) { kana_album_pointer_moved(&album, _screen, _now); }
    else if(chart.open) { kana_chart_pointer_moved(&chart, _screen, _now); }
    else                { kana_browse_pointer_moved(&browse, _screen, _now); }
}
RDE_INTERNAL void kana_list_up(f64 _now) {
    if(viewer.open)     { kana_viewer_pointer_up(&viewer, _now); }
    else if(exam.open)  { kana_exam_pointer_up(&exam, _now); }
    else if(stats.open) { kana_stats_pointer_up(&stats, _now); }
    else if(check.open) { kana_check_pointer_up(&check, _now); }
    else if(album.open) { kana_album_pointer_up(&album, _now); }
    else if(chart.open) { kana_chart_pointer_up(&chart, _now); }
    else                { kana_browse_pointer_up(&browse, _now); }
}

// Practice: only the pen (and the desktop mouse) writes; a resting hand does nothing.
RDE_INTERNAL void kana_practice_event(rde_event* _event) {
    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _screen = kana_window_to_world(_event->data.pen_event_data.position);
            if(browse_pointer == KANA_POINTER_NONE && !kana_toolbar_hit(&toolbar, _screen)) {
                browse_pointer = KANA_POINTER_PEN;
                kana_practice_pen_down(&practice, _screen);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            if(browse_pointer == KANA_POINTER_PEN) {
                kana_practice_pen_moved(&practice, kana_window_to_world(_event->data.pen_event_data.position));
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            if(browse_pointer == KANA_POINTER_PEN) {
                kana_practice_pen_up(&practice);
                browse_pointer = KANA_POINTER_NONE;
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m      = rde_input_mouse_get_position(window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && browse_pointer == KANA_POINTER_NONE &&
               !_event->handled && !kana_toolbar_hit(&toolbar, _screen)) {
                browse_pointer = KANA_POINTER_MOUSE;
                kana_practice_pen_down(&practice, _screen);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && browse_pointer == KANA_POINTER_MOUSE) {
                kana_practice_pen_up(&practice);
                browse_pointer = KANA_POINTER_NONE;
            }
        } break;
#endif

        default: break;
    }
}

// Browse's (or the chart's) input: one pointer — pen, finger or (desktop) mouse —
// to the grid and the drawing pad. Presses on the bars are the UI's.
RDE_INTERNAL void kana_browse_event(rde_event* _event) {
    const f64 _now = rde_engine_get_time_now();

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _screen = kana_window_to_world(_event->data.pen_event_data.position);
            if(browse_pointer == KANA_POINTER_NONE && !kana_toolbar_hit(&toolbar, _screen)) {
                browse_pointer = KANA_POINTER_PEN;
                kana_list_down(_screen, true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            if(browse_pointer == KANA_POINTER_PEN) {
                kana_list_moved(kana_window_to_world(_event->data.pen_event_data.position), _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            if(browse_pointer == KANA_POINTER_PEN) {
                kana_list_up(_now);
                browse_pointer = KANA_POINTER_NONE;
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            const rde_vec_2F        _pos   = kana_touch_to_screen(_touch->init_touch_position);
            if(!_touch->from_pen && browse_pointer == KANA_POINTER_NONE && !kana_toolbar_hit(&toolbar, _pos)) {
                browse_pointer = KANA_POINTER_FINGER;
                browse_finger  = _touch->finger_id;
                kana_list_down(_pos, false, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && browse_pointer == KANA_POINTER_FINGER && _touch->finger_id == browse_finger) {
                kana_list_moved(kana_touch_to_screen(_touch->moved_touch_position), _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && browse_pointer == KANA_POINTER_FINGER && _touch->finger_id == browse_finger) {
                kana_list_up(_now);
                browse_pointer = KANA_POINTER_NONE;
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m      = rde_input_mouse_get_position(window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && browse_pointer == KANA_POINTER_NONE &&
               !_event->handled && !kana_toolbar_hit(&toolbar, _screen)) {
                browse_pointer = KANA_POINTER_MOUSE;
                kana_list_down(_screen, true, _now);   // the mouse draws on the pad, like the pen
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && browse_pointer == KANA_POINTER_MOUSE) {
                kana_list_up(_now);
                browse_pointer = KANA_POINTER_NONE;
            }
        } break;
#endif

        default: break;
    }
}

void on_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);

    if(baking || _event == NULL) {
        return;
    }

    // The screens have the whole screen: nothing reaches the page (their buttons
    // are UI and have had the event already). Leaving the app still saves. The
    // top screen gets the pointer.
    if(practice.open || viewer.open || browse.open || chart.open || album.open || check.open || exam.open || stats.open) {
        if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
           _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
            kana_save_on_exit();
        }
        if(practice.open) {
            kana_practice_event(_event);
        } else {
            kana_browse_event(_event);   // the viewer, Check, Browse, the chart or the album, whichever is on top
        }
        return;
    }

    switch(_event->type) {
        // --- pen ---------------------------------------------------------
        //
        // PEN_AXIS carries pressure and tilt and NOTHING else carries them, so
        // it has to be folded into the live state before any move is sampled.
        case RDE_EVENT_TYPE_PEN_AXIS: {
            kana_ink_pen_axis(&ink, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_event_pen* _pen = &_event->data.pen_event_data;

            const rde_vec_2F     _screen = kana_window_to_world(_pen->position);

            ink.pen_seen    = true;
            ink.eraser      = _pen->eraser;
            ink.sample_time = (f64)_event->time_stamp * 1e-9; // SDL event time, ns

            // On the toolbar the pen is pressing a button — the UI gets it as a
            // synthetic mouse click right after this event — so it must not also
            // write underneath.
            if(kana_toolbar_hit(&toolbar, _screen)) {
                pen_on_ui = true;
                break;
            }

            // A press anywhere else dismisses the context menu, and still does its job.
            kana_toolbar_close_context_menu(&toolbar);

            // Writing takes over from any finger gesture in progress.
            kana_canvas_release_fingers(&canvas);

            // The Erase tool, or a pen's own eraser end.
            if(toolbar.tool == KANA_TOOL_ERASE || _pen->eraser) {
                erasing = true;
                kana_erase_at_screen(_screen);
                break;
            }

            if(toolbar.tool == KANA_TOOL_LASSO) {
                kana_lasso_pen_down(&lasso, &ink, kana_screen_to_canvas(_screen), canvas.view.zoom);
                break;
            }

            kana_ink_begin(&ink, kana_screen_to_canvas(_screen), true, false);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            const rde_event_pen* _pen = &_event->data.pen_event_data;

            ink.pen_seen = true;

            if(pen_on_ui) {
                break;
            }

            if(erasing) {
                kana_erase_at_screen(kana_window_to_world(_pen->position));
                break;
            }

            if(kana_lasso_busy(&lasso)) {
                kana_lasso_pen_moved(&lasso, &ink, kana_canvas_from_screen(&canvas, kana_window_to_world(_pen->position)), canvas.view.zoom);
                break;
            }

            // Only while the tip is down. A pen hovering in proximity still
            // reports motion, and inking on hover would be a mess.
            if(ink.drawing) {
                ink.sample_time = (f64)_event->time_stamp * 1e-9; // SDL event time, ns
                kana_ink_extend(&ink, kana_screen_to_canvas(kana_window_to_world(_pen->position)));
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            ink.pen_seen = true;
            pen_on_ui    = false;
            erasing      = false;
            kana_ink_erase_end(&ink);   // the whole swipe is one undo
            kana_ink_end(&ink);
            kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
        } break;

        // Leaving: save now. See KANA_AUTOSAVE_DELAY for why this is not the only save.
        case RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND:
        case RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND:
        case RDE_EVENT_TYPE_MOBILE_TERMINATING: {
            kana_save_on_exit();
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_IN:
        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            ink.pen_seen = true;
        } break;

        // --- touch: moves the page, NEVER draws ----------------------------
        //
        // By design only the pen draws; a finger never does. Fingers move the
        // page instead (see canvas.h): one pans, two pinch-zoom. They also feed
        // the HUD's TOUCH ONLY diagnosis ("the pen is not supported" vs "nothing
        // is reaching the app").
        //
        // SDL echoes every pen contact as a synthetic touch too (on by default),
        // sent right after PEN_DOWN; the engine flags those `from_pen` and they
        // are skipped — otherwise writing would drag the page along. A finger
        // landing while the pen is writing is a resting palm, not a gesture, so
        // it is never picked up.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }

            touch_seen = true;
            const rde_vec_2F _pos = kana_touch_to_screen(_touch->init_touch_position);

            // A finger on the toolbar is using it, not moving the page. One landing
            // while the pen writes or erases is a resting palm.
            if(kana_toolbar_hit(&toolbar, _pos)) {
                break;
            }
            kana_toolbar_close_context_menu(&toolbar);
            if(!ink.drawing && !erasing && !kana_lasso_busy(&lasso)) {
                kana_canvas_finger_down(&canvas, _touch->finger_id, _pos);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen || ink.drawing) {
                break;
            }

            kana_canvas_finger_moved(&canvas, _touch->finger_id, kana_touch_to_screen(_touch->moved_touch_position));
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }

            // Two fingers tapped: undo. Three: redo.
            const KANA_CANVAS_TAP_ _tap = kana_canvas_finger_up(&canvas, _touch->finger_id);
            if(_tap == KANA_CANVAS_TAP_TWO) {
                kana_ink_undo(&ink);
            } else if(_tap == KANA_CANVAS_TAP_THREE) {
                kana_ink_redo(&ink);
            }
        } break;

        // --- mouse, so the spike is usable on the desktop while developing ---
        //
        // Desktop only. On mobile SDL synthesizes mouse events from the pen AND from
        // touch (SDL_HINT_PEN_MOUSE_EVENTS / TOUCH_MOUSE_EVENTS, on by default), so
        // this branch would open yet another stroke on every pen-down.
#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            // The mouse reports CENTRE-ORIGIN already, unlike pen and touch.
            const rde_vec_2I _m      = rde_input_mouse_get_position(window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };

            // The UI already took this click (a button), or it landed on the bar.
            if(_event->handled || kana_toolbar_hit(&toolbar, _screen)) {
                break;
            }

            // Right-click stands in for the long press.
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                kana_toolbar_open_context_menu(&toolbar, _screen, kana_canvas_from_screen(&canvas, _screen));
                break;
            }

            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                kana_toolbar_close_context_menu(&toolbar);

                if(toolbar.tool == KANA_TOOL_ERASE) {
                    erasing = true;
                    kana_erase_at_screen(_screen);
                    break;
                }

                if(toolbar.tool == KANA_TOOL_LASSO) {
                    kana_lasso_pen_down(&lasso, &ink, kana_screen_to_canvas(_screen), canvas.view.zoom);
                    break;
                }

                kana_ink_begin(&ink, kana_screen_to_canvas(_screen), false, false);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                erasing = false;
                kana_ink_erase_end(&ink);
                kana_ink_end(&ink);
                kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
            }
        } break;
#endif

        default: break;
    }
}

void on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

// --mlkit-samples: the next range to ML Kit (recognize.h), and its answer written down.
RDE_INTERNAL void kana_mlkit_samples_update(void) {
    static kana_recognition _reading;
    static f64              _asked_at;
    if(mlkit_sample_next >= mlkit_sample_count) {
        return;
    }
    if(kana_mlkit_state() == KANA_MLKIT_FAILED) {
        kana_mlkit_prepare();
    }
    if(!mlkit_sample_asked) {
        kana_ink _one;
        kana_ink_init(&_one);
        for(u32 _s = mlkit_sample_first[mlkit_sample_next]; _s <= mlkit_sample_last[mlkit_sample_next] && _s < kana_ink_stroke_count(&ink); _s++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(&ink, _s);
            kana_ink_add_loaded_stroke(&_one, kana_ink_stroke_points(&ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        }
        mlkit_sample_asked = kana_recognize_start(&_reading, &_one);
        _asked_at          = rde_engine_get_time_now();
        kana_ink_destroy(&_one);
        return;
    }
    if(kana_recognize_poll(&_reading)) {
        c8 _path[512];
        snprintf(_path, sizeof(_path), "%smlkit_samples.txt", kana_save_dir());
        const f64 _ms   = (rde_engine_get_time_now() - _asked_at) * 1000.0;
        FILE*     _file = fopen(_path, mlkit_sample_next == 0 ? "wb" : "ab");
        if(_file != NULL) {
            fprintf(_file, "strokes %u-%u (%.0f ms):\n", mlkit_sample_first[mlkit_sample_next], mlkit_sample_last[mlkit_sample_next], _ms);
            for(u32 _l = 0; _l < _reading.line_count; _l++) {
                fprintf(_file, "%s\n", _reading.lines[_l]);
            }
            fprintf(_file, "\n");
            fclose(_file);
        }
        rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit, strokes %u-%u (%.0f ms): %s", mlkit_sample_first[mlkit_sample_next], mlkit_sample_last[mlkit_sample_next], _ms,
                      _reading.line_count > 0 ? _reading.lines[0] : "");
        mlkit_sample_next++;
        mlkit_sample_asked = false;
    }
}

// The data's code for a language ("es"...; NULL: English — the rest have no meanings of their own).
RDE_INTERNAL const c8* kana_data_language(RDE_LANGUAGE_ _language) {
    switch(_language) {
        case RDE_LANGUAGE_ES_ES: return "es";
        case RDE_LANGUAGE_PT_BR: return "pt";
        case RDE_LANGUAGE_FR_FR: return "fr";
        default:                 return NULL;
    }
}

// After a change of language (text.h): the UI built again in it, and the
// meanings shown, searched and listed in it (English where it has none).
RDE_INTERNAL void kana_follow_language(void) {
    static u32 _seen = 0;
    kana_toolbar_follow_language(&toolbar);
    if(_seen == kana_text_revision()) {
        return;
    }
    _seen = kana_text_revision();
    if(kanji_db._file != NULL) {
        kana_kanji_set_language(&kanji_db, kana_data_language(kana_text_language()));
        kana_browse_language_changed(&browse);
        viewer.rows_for = UINT32_MAX;   // its word rows keep copies
    }
}

RDE_INTERNAL void kana_update(f32 _dt) {
    if(baking) {
        return;
    }
    kana_follow_language();   // a language chosen: the UI and the meanings in it
    // --paper: the paper panel, open once the bar has been laid out.
    if(look_paper && ++look_frames == 20u) {
        kana_toolbar_open_paper(&toolbar);
        look_paper = false;
    }
    if(look_shot != NULL) {
        ++look_shot_frames;
        if(look_shot_frames == 20u && look_paste_text != NULL) {
            kana_toolbar_paste_text(&toolbar, look_paste_text, kana_canvas_from_screen(&canvas, (rde_vec_2F){ 0.0f, 0.0f }));
        }
        if(look_shot_frames == 25u && look_deselect) {
            kana_lasso_clear(&lasso, &ink);
        }
        if(look_shot_frames == 30u && look_scroll > 0.0f) {
            stats.scroller.offset       = look_scroll;
            album.scroller.offset       = look_scroll;
            album.page_scroller.offset  = look_scroll;
        }
        if(look_shot_frames == 45u) {
            rde_window_take_screenshot(window, (rde_vec_2I){ 0, 0 }, (rde_vec_2I){ 0, 0 }, look_shot, NULL);   // the next frame, whole
        } else if(look_shot_frames == 50u) {
            rde_engine_set_running(false);
        }
    }
    if(notes.open != current_canvas) {
        kana_switch_canvas();   // chosen (or made, or its canvas deleted) in the side panel
    }
    kana_mlkit_samples_update();

    kana_ink_frame_begin(&ink);

    // Smoothed hard: an unsmoothed frame time is unreadable on screen.
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

    // Practice is over everything else.
    if(practice.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_practice_pen_moved(&practice, (rde_vec_2F){ (f32)_m.x, (f32)_m.y });
        }
#endif
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_practice_close(&practice); }
        kana_practice_update(&practice);   // guided: a finished step moves on
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    // The viewer's keys, for the desktop; the page's are off while it is open. A
    // tapped example word practises its kanji.
    if(viewer.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_viewer_update(&viewer, _dt);
        u32       _kanji[KANA_VIEWER_WORD_KANJI];
        const u32 _n = kana_viewer_take_word(&viewer, _kanji, KANA_VIEWER_WORD_KANJI);
        if(_n > 0) {
            kana_practice_open_set(&practice, _kanji, _n);
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_RIGHT)) { kana_viewer_next(&viewer); }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_LEFT))  { kana_viewer_prev(&viewer); }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_SPACE)) { kana_viewer_replay(&viewer); }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_viewer_close(&viewer); }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    // Statistics: a tapped character opens the viewer, walking its list.
    if(stats.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_stats_update(&stats, _dt);
        const u32* _records;
        u32        _count, _position;
        if(kana_stats_take_tap(&stats, &_records, &_count, &_position)) {
            kana_viewer_show(&viewer, _records, _count, _position);
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_stats_close(&stats); }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    // An exam: its answers read as it goes; a tapped result opens the viewer on it,
    // walking the exam's characters.
    if(exam.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_exam_update(&exam, _dt);
        u32 _tapped;
        if(kana_exam_take_tap(&exam, &_tapped)) {
            u32       _asked[KANA_EXAM_MAX];
            const u32 _n = kana_exam_asked(&exam, _asked, KANA_EXAM_MAX);
            for(u32 _i = 0; _i < _n; _i++) {
                if(_asked[_i] == _tapped) {
                    kana_viewer_show(&viewer, _asked, _n, _i);
                    break;
                }
            }
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_exam_close(&exam); }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    if(check.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_check_close(&check); }
        kana_check_update(&check);   // reads the selection once the screen has shown
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    if(album.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_album_update(&album, _dt);
        u32 _kept;
        if(kana_album_take_exam(&album, &_kept)) {
            kana_exam_open_kept(&exam, _kept);   // its results, over the album
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) {
            if(album.page_open) { kana_album_close_page(&album); } else { kana_album_close(&album); }
        }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    if(browse.open || chart.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {   // the mouse is polled, like on the page
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        // A tapped character opens the viewer, walking the grid's (or chart's)
        // list; in Select mode it is ticked (or unticked) instead.
        u32 _position = 0;
        if(browse.open) {
            kana_browse_update(&browse, _dt);
            if(kana_browse_take_tap(&browse, &_position)) {
                if(selection.active) { kana_selection_toggle(&selection, kana_browse_list(&browse)[_position]); }
                else                 { kana_viewer_show(&viewer, kana_browse_list(&browse), kana_browse_count(&browse), _position); }
            }
        } else {
            kana_chart_update(&chart, _dt);
            if(kana_chart_take_tap(&chart, &_position)) {
                if(selection.active) { kana_selection_toggle(&selection, kana_chart_list(&chart)[_position]); }
                else                 { kana_viewer_show(&viewer, kana_chart_list(&chart), kana_chart_count(&chart), _position); }
            }
        }

        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) {
            kana_browse_close(&browse);
            kana_chart_close(&chart);
        }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_C)) {
        kana_ink_clear(&ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_Z)) {
        kana_ink_undo(&ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_Y)) {
        kana_ink_redo(&ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_DELETE)) {
        kana_lasso_delete(&lasso, &ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) {
        kana_lasso_clear(&lasso, &ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_M)) {
        show_samples = !show_samples;
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_H)) {
        show_hud = !show_hud;
        kana_toolbar_sync(&toolbar);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_R)) {
        kana_canvas_reset_view(&canvas);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_B)) {
        kana_toggle_brush_scale();
    }

    // Desktop dragging, so the thing can be exercised without a tablet in hand.
    // Not on mobile, where the "mouse" is SDL's echo of the pen (see on_event).
#if !defined(RDE_PLATFORM_MOBILE)
    if(rde_input_mouse_is_button_pressed(window, RDE_MOUSE_BUTTON_LEFT)) {
        const rde_vec_2I _m      = rde_input_mouse_get_position(window);
        const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };

        if(erasing) {
            kana_erase_at_screen(_screen);
        } else if(kana_lasso_busy(&lasso)) {
            kana_lasso_pen_moved(&lasso, &ink, kana_canvas_from_screen(&canvas, _screen), canvas.view.zoom);
        } else if(ink.drawing) {
            kana_ink_extend(&ink, kana_screen_to_canvas(_screen));
        }
    }
#endif

    // A finger held still: the page's context menu. Polled, because a finger
    // that doesn't move sends no events.
    rde_vec_2F _press;
    if(kana_canvas_long_press(&canvas, &_press)) {
        kana_toolbar_open_context_menu(&toolbar, _press, kana_canvas_from_screen(&canvas, _press));
    }

    kana_toolbar_update(&toolbar);
    kana_autosave();
}

void on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

RDE_INTERNAL void kana_hud_text(const c8* _text, f32 _x, f32 _y) {
    kana_draw_text(font, font_px, _text, _x, _y, KANA_HUD_TEXT_PX, kana_theme_active()->hud);
}

// The zoom as a percentage, in a pill at the bottom-right, while it is changing
// and for a moment after.
RDE_INTERNAL void kana_draw_zoom_toast(rde_window* _window) {
    const f64 _now = rde_engine_get_time_now();
    if(zoom_seen < 0.0f) {
        zoom_seen = canvas.view.zoom;
    } else if(fabsf(canvas.view.zoom - zoom_seen) > 1e-5f) {
        zoom_seen     = canvas.view.zoom;
        zoom_shown_at = _now;
    }

    const f64 _age = _now - zoom_shown_at;
    if(_age >= KANA_ZOOM_TOAST_TIME + KANA_ZOOM_TOAST_FADE || font == NULL) {
        return;
    }
    const f32 _fade = _age <= KANA_ZOOM_TOAST_TIME ? 1.0f : 1.0f - (f32)((_age - KANA_ZOOM_TOAST_TIME) / KANA_ZOOM_TOAST_FADE);

    // The text, measured (once per new value): the pill fits it, and it sits in
    // the middle — across by its width, up and down by its digits' height.
    static c8         _shown[16] = "";
    static rde_vec_2F _measured  = { 0.0f, 0.0f };
    c8 _text[16];
    snprintf(_text, sizeof(_text), "%.0f%%", (f64)(canvas.view.zoom * 100.0f));
    const f32 _px    = 20.0f;
    const f32 _scale = _px / font_px;
    if(strcmp(_text, _shown) != 0) {
        snprintf(_shown, sizeof(_shown), "%s", _text);
        _measured = rde_rich_text_measure(_text, font, _scale, 10000.0f, false);
    }
    const f32 _em     = _measured.y / 1.17f;          // the line is ~1.17 em in Roboto
    const f32 _digits = _em * 0.711f;                 // Roboto's digit (cap) height, in em

    const kana_theme* _t      = kana_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _safe   = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const rde_vec_2F  _pill   = { fmaxf(72.0f, _measured.x + 32.0f), fmaxf(40.0f, _digits + 26.0f) };
    const rde_vec_2F  _center = { (f32)_size.x * 0.5f - (f32)_safe.z - 16.0f - _pill.x * 0.5f, -(f32)_size.y * 0.5f + (f32)_safe.w + 16.0f + _pill.y * 0.5f };

    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_rendering_2d_draw_rounded_rectangle(_center, _pill, 1.0f, 8u, _back, NULL);

    rde_color _ink = _t->button_text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    // The position is where the text starts, on its baseline.
    kana_draw_text(font, font_px, _text, _center.x - _measured.x * 0.5f, _center.y - _digits * 0.5f, _px, _ink);
}

// The toolbar's notice — what Copy as text copied, what Paste text left out — in
// a pill at the bottom, in the middle, a moment.
RDE_INTERNAL void kana_draw_notice(rde_window* _window) {
    if(toolbar.notice_at <= 0.0 || toolbar.notice[0] == 0 || font == NULL) {
        return;
    }
    const f64 _age = rde_engine_get_time_now() - toolbar.notice_at;
    if(_age >= KANA_NOTICE_TIME + KANA_NOTICE_FADE) {
        toolbar.notice_at = 0.0;
        return;
    }
    const f32         _fade = _age <= KANA_NOTICE_TIME ? 1.0f : 1.0f - (f32)((_age - KANA_NOTICE_TIME) / KANA_NOTICE_FADE);
    const kana_theme* _t    = kana_theme_active();
    const rde_vec_2I  _size = rde_window_get_size(_window);
    const rde_vec_4I  _safe = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _px   = kana_draw_text_px_to_fit(font, font_px, toolbar.notice, 17.0f, (f32)_size.x * 0.85f - 40.0f, 0.6f);
    const f32         _w    = kana_draw_text_width(font, font_px, toolbar.notice, _px);
    const rde_vec_2F  _pill = { _w + 40.0f, _px + 26.0f };
    const rde_vec_2F  _c    = { 0.0f, -(f32)_size.y * 0.5f + (f32)_safe.w + 72.0f + _pill.y * 0.5f };

    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_color _edge = _t->outline;
    _edge.a         = (u8)((f32)_edge.a * _fade);
    kana_draw_card((rde_vec_2F){ _c.x - _pill.x * 0.5f, _c.y - _pill.y * 0.5f }, (rde_vec_2F){ _c.x + _pill.x * 0.5f, _c.y + _pill.y * 0.5f }, _pill.y * 0.5f, _back, _edge);
    rde_color _ink = _t->text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    kana_draw_text(font, font_px, toolbar.notice, _c.x - _w * 0.5f, _c.y - _px * 0.36f, _px, _ink);
}

RDE_INTERNAL void kana_draw_hud(rde_window* _window) {
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    // Below the status bar, not under it.
    const f32 _x = -(f32)_size.x * 0.5f + (f32)_insets.x + 20.0f;
    f32       _y =  (f32)_size.y * 0.5f - (f32)_insets.y - 34.0f;

    c8 _line[192];

    // THE HEADLINE, and the reason this spike exists. Answers open question 1 at
    // a glance, from across the room, without reading a log.
    const c8* _source = ink.pen_seen  ? "PEN  (stylus events arriving)"
                      : touch_seen    ? "TOUCH ONLY  - no pen events!"
                                      : "waiting for input...";

    kana_hud_text(_source, _x, _y);
    _y -= 34.0f;

    // Whether this pen has real pressure, or its ink width comes from speed.
    const c8* _pressure_mode = !ink.pen_seen      ? ""
                             : ink.pressure_live  ? " [real]"
                                                  : " [none - width from speed]";

    snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)%s   tilt %.0f / %.0f   pen id %u   %s",
             (f64)ink.pressure, (f64)ink.pressure_raw, _pressure_mode,
             (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id,
             ink.eraser ? "ERASER" : "tip");
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    // The number that decides whether fast strokes can look smooth.
    snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms",
             (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end, see the header",
             (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "%u strokes, %u points   history %u (+%u redo)",
             kana_ink_alive_strokes(&ink), kana_ink_total_points(&ink),
             kana_ink_undo_steps(&ink), kana_ink_redo_steps(&ink));
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "zoom %.0f%%   fingers: 1 pan, 2 pinch   tool: %s",
             (f64)(canvas.view.zoom * 100.0f),
             toolbar.tool == KANA_TOOL_ERASE ? "ERASE" : toolbar.tool == KANA_TOOL_LASSO ? "LASSO" : "DRAW");
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    if(save_failed) {
        snprintf(_line, sizeof(_line), "SAVE FAILED - retrying   (%s)", load_note);
    } else if(last_save_time > 0.0) {
        snprintf(_line, sizeof(_line), "saved %.0f s ago: %.1f KB in %.1f ms   (%s)",
                 rde_engine_get_time_now() - last_save_time, (f64)last_save_bytes / 1024.0, (f64)last_save_ms, load_note);
    } else {
        snprintf(_line, sizeof(_line), "not saved yet this run   (%s)", load_note);
    }
    kana_hud_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "brush: %s",
             ink.brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? "PAGE - same width on the page at any zoom"
                                                          : "SCREEN - same width on screen while writing");
    kana_hud_text(_line, _x, _y);
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
    const rde_vec_4I _safe = rde_window_get_safe_area_insets(_window);
    const f32        _hh   = (f32)rde_window_get_size(_window).y * 0.5f;
    if(practice.open) {
        kana_practice_render(&practice, _window, font, font_px, _hh - (f32)_safe.y - 8.0f,
                             -_hh + (f32)_safe.w + toolbar.practice_menu.size.y + 24.0f);
    } else if(viewer.open) {
        kana_viewer_render(&viewer, _window, font, font_px, toolbar.viewer_menu.size.y + 16.0f);
    } else if(stats.open) {
        kana_stats_render(&stats, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.stats_menu.size.y + 24.0f);
    } else if(exam.open) {
        kana_exam_render(&exam, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.exam_menu.size.y + 24.0f);
    } else if(check.open) {
        kana_check_render(&check, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.check_menu.size.y + 24.0f);
    } else if(album.open) {
        kana_album_render(&album, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.album_menu.size.y + 24.0f);
    } else if(browse.open) {
        kana_browse_render(&browse, _window, font, font_px, _hh - toolbar.browse_bar_height, -_hh + (f32)_safe.w + toolbar.browse_menu.size.y + 24.0f);
    } else if(chart.open) {
        kana_chart_render(&chart, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.chart_menu.size.y + 24.0f);
    } else {
        const rde_vec_2I _size = rde_window_get_size(_window);
        kana_canvas_draw_grid(&canvas, _size);
        kana_lasso_render_under(&lasso, &ink, canvas.view.offset, canvas.view.zoom);
        kana_ink_render(&ink, canvas.view.offset, canvas.view.zoom, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f },
                        rde_engine_get_time_now(), show_samples);
        kana_lasso_render_over(&lasso, &ink, canvas.view.offset, canvas.view.zoom);

        if(show_hud && font != NULL) {
            kana_draw_hud(_window);
        }
        kana_draw_zoom_toast(_window);
        kana_draw_notice(_window);
    }
    rde_rendering_2d_end_drawing();
}

// --perf: the time is up — the summary, once.
RDE_INTERNAL void kana_perf_write(f64 _now) {
    const f64 _n    = perf_frames > 0 ? (f64)perf_frames : 1.0;
    const f64 _span = _now - perf_start;
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%sperf.txt", kana_save_dir());
    FILE* _f = fopen(_path, "a");
    if(_f != NULL) {
        fprintf(_f, "%s| %u frames in %.1f s: %.1f fps, frame %.2f ms avg, %.2f ms worst, %u over 20 ms | update %.2f ms avg, %.2f worst | render %.2f ms avg, %.2f worst\n",
                perf_label, perf_frames, _span, _n / _span, 1000.0 * perf_dt_sum / _n, 1000.0 * perf_dt_max, perf_slow,
                1000.0 * perf_update_sum / _n, 1000.0 * perf_update_max, 1000.0 * perf_render_sum / _n, 1000.0 * perf_render_max);
        fclose(_f);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana perf: %u frames, %.2f ms avg, %.2f ms worst (written to %s)", perf_frames, 1000.0 * perf_dt_sum / _n, 1000.0 * perf_dt_max, _path);
    perf_seconds = 0.0;
}

// The engine's update and render: Kana's, timed when --perf asks.
void on_update(f32 _dt) {
    if(perf_seconds <= 0.0) {
        kana_update(_dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    if(perf_start <= 0.0) {
        perf_start = perf_settle++ >= 60u ? _t0 : 0.0;   // a second to settle first
    } else {
        perf_frames++;
        perf_dt_sum += (f64)_dt;
        perf_dt_max  = (f64)_dt > perf_dt_max ? (f64)_dt : perf_dt_max;
        perf_slow   += _dt > 0.020f ? 1u : 0u;
    }
    kana_update(_dt);
    if(perf_start > 0.0 && perf_frames > 0) {
        const f64 _t = rde_engine_get_time_now() - _t0;
        perf_update_sum += _t;
        perf_update_max  = _t > perf_update_max ? _t : perf_update_max;
    }
}

void on_render(rde_window* _window, f32 _dt) {
    if(perf_seconds <= 0.0) {
        kana_render(_window, _dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    kana_render(_window, _dt);
    if(perf_start > 0.0 && perf_frames > 0) {
        const f64 _now = rde_engine_get_time_now();
        perf_render_sum += _now - _t0;
        perf_render_max  = _now - _t0 > perf_render_max ? _now - _t0 : perf_render_max;
        if(_now - perf_start >= perf_seconds) {
            kana_perf_write(_now);
        }
    }
}

void on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

void end_func(void) {
    if(baking) {
        return;
    }
    kana_save_on_exit();
    kana_toolbar_destroy(&toolbar);
    kana_lasso_destroy(&lasso);
    kana_viewer_destroy(&viewer);
    kana_browse_destroy(&browse);
    kana_chart_destroy(&chart);
    kana_selection_destroy(&selection);
    kana_exam_destroy(&exam);
    kana_stats_destroy(&stats);
    kana_marks_close();
    kana_examlog_close();
    kana_userwords_close();
    kana_practice_destroy(&practice);
    kana_album_destroy(&album);
    kana_notes_destroy(&notes);
    kana_check_destroy(&check);
    kana_match_release();
    kana_kanji_unload(&kanji_db);
    kana_ink_destroy(&ink);
}

int main(i32 _argc, c8* _argv[]) {
    return rde_run(
        _argc,
        _argv,
        KANA_CONFIG_PATH,
        init_func,
        on_event,
        on_fixed_update,
        on_update,
        on_late_update,
        on_render,
        on_crash,
        end_func
    );
}
