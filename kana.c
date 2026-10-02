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
#include "scan.h"
#include "examlog.h"
#include "marks.h"
#include "vocab.h"
#include "viewer.h"
#include "theme.h"
#include "draw.h"
#include "recognize.h"
#include "backup.h"
#include "sheet.h"
#include "wordcard.h"
#include "vocabview.h"
#include "wordexam.h"
#include "charnote.h"
#include "welcome.h"
#include "review.h"
#include "speech.h"
#include <time.h>

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
RDE_INTERNAL kana_welcome welcome;   // the first time, and from Settings (welcome.h)
RDE_INTERNAL kana_lasso   lasso;
RDE_INTERNAL kana_kanji_db kanji_db;
RDE_INTERNAL kana_viewer  viewer;
RDE_INTERNAL kana_browse  browse;
RDE_INTERNAL kana_chart   chart;
RDE_INTERNAL kana_selection selection;   // Browse's and the chart's ticks (select.h)
RDE_INTERNAL kana_exam      exam;        // exams (exam.h), over Browse, the chart and the album
RDE_INTERNAL kana_stats     stats;       // statistics (stats.h)
RDE_INTERNAL kana_vocabview vocabview;  // the Vocabulary screen (vocabview.h)
RDE_INTERNAL kana_wordexam wordexam;    // word exams (wordexam.h)
RDE_INTERNAL kana_scan      scan;        // text from a photo (scan.h)
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
RDE_INTERNAL i32       look_vocab       = -1;     // --vocab[=N]: the Vocabulary screen (N: on the Nth list)
RDE_INTERNAL b8        look_vocab_sample = false; // --vocab-sample: a few words and a list, when the vocabulary is empty
RDE_INTERNAL const c8* look_note        = NULL;   // --note=TEXT: (with --viewer) the character's note set to TEXT
RDE_INTERNAL i32       look_word_exam   = -1;     // --word-exam[=STAGE]: a word exam of every word (0 its setup, 1 writing, 2 by ear, 3 its results)
RDE_INTERNAL i64       look_kept_exam   = -1;
RDE_INTERNAL f32       look_scroll      = 0.0f;   // --scroll=PX: the open screen scrolled down (Statistics, the album)
RDE_INTERNAL i32       look_theme       = -1;     // --theme=N: shown, not saved
RDE_INTERNAL const c8* look_paste_text  = NULL;   // --paste-text=TEXT: Paste text, as if the clipboard held it, mid-page
RDE_INTERNAL b8        look_deselect    = false;  // --deselect: and nothing left selected after it
RDE_INTERNAL b8        look_trim_fonts  = false;  // --trim-fonts: the fonts trimmed (as on going to the background) just before the shot
RDE_INTERNAL const c8* look_scan_demo   = NULL;   // --scan-demo=PNG: Text from a photo on that image, its lines as given below
RDE_INTERNAL f32       look_scan_turn   = 0.0f;   // --scan-demo-turn=DEG: the image as a camera frame that needs DEG clockwise
RDE_INTERNAL b8        look_scan_live   = false;  // --scan-live: Text from a photo, the camera live (to measure it with --perf)
RDE_INTERNAL b8        look_scan_rows   = false;  // --scan-demo-top-first: and its rows top first, as a camera frame's are
RDE_INTERNAL b8        look_scan_translate = false;  // --scan-demo-translate: and Translate with Google on, translate.h pretending
RDE_INTERNAL const c8* look_translate_probe = NULL; // --translate-probe=xx: ML Kit's translator tried on the device (see kana_translate_probe)
RDE_INTERNAL b8        look_translate_sel = false;  // --translate-selection: the selection (after --paste-text) translated, translate.h pretending
RDE_INTERNAL b8        look_save_sel    = false;  // --save-selection: Save word on the selection (after --paste-text)
RDE_INTERNAL const c8* look_word_card   = NULL;   // --word-card=TEXT: the word card for TEXT, as if the lasso read it
RDE_INTERNAL b8        look_data        = false;  // --data: Settings › Your data
RDE_INTERNAL i32       look_welcome     = -1;     // --welcome[=PAGE]: the welcome, on that page (from 1)
RDE_INTERNAL const c8* look_data_export = NULL;   // --data-export=FILE: everything exported to FILE (no dialog)
RDE_INTERNAL const c8* look_data_import = NULL;   // --data-import=FILE: FILE picked to import (its question shown)...
RDE_INTERNAL b8        look_data_replace = false; // --data-replace: ...and Replace pressed
RDE_INTERNAL const c8* look_sheet       = NULL;   // --sheet=FILE: the viewer's character as a practice sheet at FILE (no dialog)

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
// A pen has written here, ever (saved): until one has, a tablet starts with the
// toolbar's hand on — one finger writes — and the first pen turns it off.
RDE_INTERNAL b8 pen_ever  = false;

// --- writing with a finger (the toolbar's hand, toolbar.finger_writes) -----------------
//
// One finger writes, as the pen does; two or more move the page (canvas.h), as
// fingers always do. Which it is shows a moment later: a finger down is PENDING —
// its points kept — until it moves or rests a moment with a little movement:
// then it writes, from where it landed. A second finger landing first makes them
// the page's gesture (pinch, pan, the two- and three-finger taps). Held still, it
// is the long press: the page's context menu. A finger landing while one writes
// is a resting hand: nothing. A short touch is a dot.
#define KANA_FINGER_POINTS 128u
#define KANA_FINGER_MOVE   6.0f    // screen units from where it landed: it writes
#define KANA_FINGER_WAIT   0.12    // seconds down with some movement: it writes
typedef enum { KANA_FINGER_NONE = 0, KANA_FINGER_PENDING, KANA_FINGER_WRITING, KANA_FINGER_GESTURE, KANA_FINGER_IGNORED } KANA_FINGER_;
RDE_INTERNAL struct {
    u8         state;      // KANA_FINGER_
    u64        id;         // the finger pending, writing or ignored
    rde_vec_2F points[KANA_FINGER_POINTS];
    u32        count;
    f64        since;
    u32        gesture;    // fingers handed to the page's gesture, still down
} finger_ink;

// A developer's build (debug, RDE_DEBUG): the diagnostics HUD (H) and the raw
// pen samples (M) can be shown, and launch arguments are read (the look flags,
// --perf, the bake). A release build has none of it — it takes no arguments at
// all (main) — unless built with -DKANA_ALLOW_ARGS, to measure a release on the
// device (--perf, COMMANDS.txt). The HUD starts hidden either way, and is not
// a setting: nothing saved shows it.
#if defined(RDE_DEBUG)
#define KANA_DEVELOPER 1
#else
#define KANA_DEVELOPER 0
#endif
#if KANA_DEVELOPER || defined(KANA_ALLOW_ARGS)
#define KANA_TAKES_ARGS 1
#else
#define KANA_TAKES_ARGS 0
#endif

RDE_INTERNAL b8  show_samples = false;
RDE_INTERNAL b8  show_hud     = false;

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
    _s.show_hud       = false;   // not a setting any more (the byte stays in the file)
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
    _s.finger_writes   = toolbar.finger_writes;
    _s.pen_ever        = pen_ever;
    return _s;
}

RDE_INTERNAL void kana_apply_settings(const kana_settings* _s) {
    toolbar.tool         = _s->tool == KANA_TOOL_ERASE ? KANA_TOOL_ERASE : _s->tool == KANA_TOOL_LASSO ? KANA_TOOL_LASSO : KANA_TOOL_DRAW;
    ink.brush_scale      = _s->brush_scale == KANA_INK_BRUSH_SCALE_SCREEN ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    ink.width_mode       = _s->width_mode == KANA_INK_WIDTH_MODE_PRESSURE ? KANA_INK_WIDTH_MODE_PRESSURE : KANA_INK_WIDTH_MODE_CONSTANT;
    ink.color            = _s->color;
    ink.constant_radius  = rde_math_clamp_f32(_s->radius, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
    kana_theme_set((KANA_THEME_)_s->theme);
    kana_mlkit_set_enabled(_s->mlkit);
    canvas.paper_size   = _s->paper_size < KANA_PAPER_SIZE_COUNT ? (KANA_PAPER_SIZE_)_s->paper_size : KANA_PAPER_MEDIUM;
    settings_language   = _s->language;
    toolbar.finger_writes = _s->finger_writes;
    pen_ever            = _s->pen_ever;
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
    kana_vocab_open(_path);
    snprintf(_path, sizeof(_path), "%sreviews.kana", _dir);
    kana_reviews_open(_path);
    snprintf(_path, sizeof(_path), "%scharnotes.kana", _dir);
    kana_charnotes_open(_path);

    kana_settings    _settings = kana_gather_settings();
    const KANA_LOAD_ _loaded   = kana_load_settings(settings_path, &_settings);
    if(_loaded == KANA_LOAD_OK) {
        kana_apply_settings(&_settings);
    } else if(_loaded == KANA_LOAD_MISSING) {
        kana_welcome_open(&welcome);   // the very first time: what Kana is, and its gestures
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

// --- Your data (backup.h, Settings › Your data) -------------------------------------------

// After an import put the backup's files in the save folder: everything read
// again from them — the canvases, marks, exams, words, settings and the open
// page. Every screen is closed first (each holds what it read before), and the
// page is let go as on a canvas switch.
RDE_INTERNAL void kana_reload_saves(void) {
    if(practice.open) { kana_practice_close(&practice); }
    if(viewer.open)   { kana_viewer_close(&viewer); }
    if(browse.open)   { kana_browse_close(&browse); }
    if(chart.open)    { kana_chart_close(&chart); }
    if(album.open)    { kana_album_close(&album); }
    if(check.open)    { kana_check_close(&check); }
    if(exam.open)     { kana_exam_close(&exam); }
    if(stats.open)    { kana_stats_close(&stats); }
    if(scan.open)     { kana_scan_close(&scan); }
    if(wordexam.open) { kana_wordexam_close(&wordexam); }
    if(vocabview.open) { kana_vocabview_close(&vocabview); }
    kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
    kana_ink_erase_end(&ink);
    kana_ink_end(&ink);
    pen_on_ui = false;
    erasing   = false;
    kana_canvas_release_fingers(&canvas);
    kana_lasso_clear(&lasso, &ink);
    kana_ink_destroy(&ink);
    kana_ink_init(&ink);
    kana_canvas_reset_view(&canvas);
    canvas.page = (kana_page){ 0 };
    kana_load_saves();
    toolbar.side._notes_built = false;   // the side panel's list, from the new notes
    kana_toolbar_sync(&toolbar);
    zoom_seen = -1.0f;
}

// Everything into one file at _path: shared (the system's sheet: Files, iCloud
// Drive, a message...) on a phone or tablet, or already where the learner chose.
RDE_INTERNAL void kana_data_export_to(const c8* _path, b8 _share) {
    kana_backup_info _info;
    c8               _line[400];
    if(!kana_backup_export(kana_save_dir(), _path, &_info) || (_share && !rde_mobile_share_file(_path, "application/octet-stream", "Kana backup"))) {
        kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_share) {
        KANA_TEXTF(_line, KANA_TEXT_DATA_EXPORT_READY, KANA_TN(_info.files));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_DATA_EXPORT_SAVED, KANA_TN(_info.files), KANA_TS(_path));
    }
    kana_side_data_message(&toolbar, _line, false);
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana: exported %u files (%llu bytes) to %s", _info.files, (unsigned long long)_info.bytes, _path);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen (rde_dialog_save_file).
RDE_INTERNAL void kana_data_on_save_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter); RDE_UNUSED(_user_data);
    if(_paths == NULL) {
        kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_EXPORT_FAILED), true);
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8          _path[RDE_MAX_PATH];
    const c8*   _ext = "." KANA_BACKUP_EXTENSION;
    const usize _n   = strlen(_paths[0]);
    const b8    _has = _n >= strlen(_ext) && strcmp(_paths[0] + _n - strlen(_ext), _ext) == 0;
    snprintf(_path, sizeof(_path), "%s%s", _paths[0], _has ? "" : _ext);
    kana_data_export_to(_path, false);
}
#endif

// A file picked to import (rde_dialog_open_file): read, checked whole, and the
// question asked — nothing changes until Replace.
RDE_INTERNAL void kana_data_on_open_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter); RDE_UNUSED(_user_data);
    if(_paths == NULL) {
        kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    rde_memory_allocator* _a    = rde_memory_allocator_get_default_std();
    usize                 _size = 0;
    u8*                   _data = rde_file_read_uri(_paths[0], &_size, _a);
    if(_data == NULL) {
        kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_CANT_OPEN), true);
        return;
    }
    kana_backup_info _info;
    if(!kana_backup_inspect(_data, _size, &_info)) {
        _a->free(_a->allocator, _data);
        kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_NOT_BACKUP), true);
        return;
    }
    c8 _when[64];
    c8 _question[400];
    kana_text_date_time(_when, sizeof(_when), _info.created);
    KANA_TEXTF(_question, KANA_TEXT_DATA_CONFIRM, KANA_TS(_when), KANA_TN(_info.canvases));
    kana_side_data_confirm(&toolbar, _data, _size, _question);
}

// Deletes what an earlier export left in the outbox.
RDE_INTERNAL b8 kana_data_outbox_entry(const c8* _path, b8 _is_dir, any _user_data) {
    RDE_UNUSED(_user_data);
    if(!_is_dir) {
        rde_file_delete(_path);
    }
    return true;
}

#if defined(RDE_PLATFORM_MOBILE)
// A file to share goes in the outbox (what an earlier one left there let go):
// its path, _name in it.
RDE_INTERNAL void kana_outbox_path(c8* _out, usize _size, const c8* _name) {
    c8 _outbox[RDE_MAX_PATH];
    snprintf(_outbox, sizeof(_outbox), "%s%s/", kana_save_dir(), KANA_BACKUP_OUTBOX);
    if(rde_file_dir_exists(_outbox)) {
        rde_file_crawl_dir_recursively(_outbox, kana_data_outbox_entry, NULL, 0, NULL);
    }
    snprintf(_out, _size, "%s%s", _outbox, _name);
    rde_file_create_missing_dirs(_out);
}
#endif

// Today, for a file's name: 2026-10-01.
RDE_INTERNAL void kana_file_date(c8* _out, usize _size) {
    const time_t     _now = time(NULL);
    const struct tm* _tm  = localtime(&_now);
    strftime(_out, _size, "%Y-%m-%d", _tm);
}

// --- Practice sheets (sheet.h) -----------------------------------------------------------

#if !defined(RDE_PLATFORM_MOBILE)
RDE_INTERNAL u32 sheet_records[KANA_SHEET_MAX];   // the desktop: what the save dialog's answer is for
RDE_INTERNAL u32 sheet_count;
RDE_INTERNAL b8  sheet_cut;                       // ...more were asked for
#endif

// _records as a sheet at _path: shared (the system's sheet, which prints) on a
// phone or tablet, or already where the learner chose. A note when _cut (more
// were asked for than a sheet takes).
RDE_INTERNAL void kana_sheet_make(const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share) {
    kana_sheet_info _info;
    if(!kana_sheet_write(&kanji_db, _records, _count, _path, &_info) || (_share && !rde_mobile_share_file(_path, "application/pdf", kana_text(KANA_TEXT_SHEET_TITLE)))) {
        kana_toolbar_notice(&toolbar, kana_text(KANA_TEXT_SHEET_FAILED));
        return;
    }
    c8 _line[400];
    if(_cut) {
        KANA_TEXTF(_line, KANA_TEXT_SHEET_FIRST_N, KANA_TN(KANA_SHEET_MAX));
        kana_toolbar_notice(&toolbar, _line);
    } else if(!_share) {
        KANA_TEXTF(_line, KANA_TEXT_SHEET_SAVED, KANA_TS(_path));
        kana_toolbar_notice(&toolbar, _line);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana: practice sheet, %u characters on %u pages (%u bytes): %s", _info.characters, _info.pages, _info.bytes, _path);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen (rde_dialog_save_file).
RDE_INTERNAL void kana_sheet_on_save_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter); RDE_UNUSED(_user_data);
    if(_paths == NULL) {
        kana_toolbar_notice(&toolbar, kana_text(KANA_TEXT_SHEET_FAILED));
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8          _path[RDE_MAX_PATH];
    const usize _n   = strlen(_paths[0]);
    const b8    _has = _n >= 4u && (strcmp(_paths[0] + _n - 4u, ".pdf") == 0 || strcmp(_paths[0] + _n - 4u, ".PDF") == 0);
    snprintf(_path, sizeof(_path), "%s%s", _paths[0], _has ? "" : ".pdf");
    kana_sheet_make(sheet_records, sheet_count, sheet_cut, _path, false);
}
#endif

// Once a frame: a sheet asked for — to share, or (the desktop) where to save it.
RDE_INTERNAL void kana_sheet_update(void) {
    const u32* _records = NULL;
    const u32  _count   = kana_toolbar_take_sheet(&toolbar, &_records);
    if(_count == 0u) {
        return;
    }
#if defined(RDE_PLATFORM_MOBILE)
    c8 _date[32];
    c8 _name[160];
    c8 _path[RDE_MAX_PATH];
    kana_file_date(_date, sizeof(_date));
    snprintf(_name, sizeof(_name), "%s %s.pdf", kana_text(KANA_TEXT_SHEET_TITLE), _date);
    kana_outbox_path(_path, sizeof(_path), _name);
    kana_sheet_make(_records, _count, _count > KANA_SHEET_MAX, _path, true);
#else
    sheet_count = 0;
    for(u32 _i = 0; _i < _count && _i < KANA_SHEET_MAX; _i++) {
        sheet_records[sheet_count++] = _records[_i];
    }
    sheet_cut = _count > KANA_SHEET_MAX;
    static const rde_dialog_filter _filter = { "PDF", "pdf" };
    rde_dialog_save_file(window, &_filter, 1, NULL, kana_sheet_on_save_path, NULL);
#endif
}

// Once a frame: what Your data asked for.
RDE_INTERNAL void kana_data_update(void) {
    switch(kana_side_take_data_request(&toolbar)) {
        case KANA_SIDE_DATA_EXPORT: {
            kana_save_now(false);   // what is on screen, in the files first
#if defined(RDE_PLATFORM_MOBILE)
            // Written to the outbox, then shared from there.
            c8 _date[32];
            c8 _name[96];
            c8 _path[RDE_MAX_PATH];
            kana_file_date(_date, sizeof(_date));
            snprintf(_name, sizeof(_name), "Kana backup %s.%s", _date, KANA_BACKUP_EXTENSION);
            kana_outbox_path(_path, sizeof(_path), _name);
            kana_data_export_to(_path, true);
#else
            static const rde_dialog_filter _filter = { "Kana backup", KANA_BACKUP_EXTENSION };
            rde_dialog_save_file(window, &_filter, 1, NULL, kana_data_on_save_path, NULL);
#endif
        } break;
        case KANA_SIDE_DATA_IMPORT: {
            static const rde_dialog_filter _filter = { "Kana backup", KANA_BACKUP_EXTENSION };
            rde_dialog_open_file(window, &_filter, 1, NULL, false, kana_data_on_open_path, NULL);
        } break;
        case KANA_SIDE_DATA_REPLACE: {
            kana_save_now(false);   // so what is set aside is what was on screen
            kana_backup_info _info;
            const b8 _whole = kana_backup_inspect(toolbar.side.data_backup, toolbar.side.data_backup_size, &_info);
            const b8 _ok    = _whole && kana_backup_restore(toolbar.side.data_backup, toolbar.side.data_backup_size, kana_save_dir());
            kana_side_data_done(&toolbar);
            if(_ok) {
                kana_reload_saves();
                c8 _when[64];
                c8 _line[400];
                kana_text_date_time(_when, sizeof(_when), _info.created);
                KANA_TEXTF(_line, KANA_TEXT_DATA_IMPORTED, KANA_TS(_when));
                kana_side_data_message(&toolbar, _line, false);
                rde_log_level(RDE_LOG_LEVEL_INFO, "kana: imported %u files from a backup made %s", _info.files, _when);
            } else {
                kana_side_data_message(&toolbar, kana_text(KANA_TEXT_DATA_IMPORT_FAILED), true);
            }
        } break;
        default: break;
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
    kana_vocabview_init(&vocabview, _have_kanji ? &kanji_db : NULL);
    kana_wordexam_init(&wordexam, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    kana_stats_init(&stats, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    kana_scan_init(&scan);
    scan.db = _have_kanji ? &kanji_db : NULL;   // the lines' words (wordsplit.h)
    browse.selection = &selection;
    chart.selection  = &selection;
    kana_practice_init(&practice, _have_kanji ? &kanji_db : NULL);
    kana_album_init(&album, _have_kanji ? &kanji_db : NULL);
    kana_notes_init(&notes);
    kana_check_init(&check, _have_kanji ? &kanji_db : NULL, &browse.catalog);
    if(_have_kanji) {
        rde_log_color(RDE_LOG_COLOR_GREEN, "kana: %u characters loaded", kanji_db.count);
    }

    // --scan-demo-translate: translate.h pretends (debug builds), so the scan
    // screen has its Translate with Google — known before the toolbar is built.
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--scan-demo-translate") == 0) {
            kana_translate_demo(true);
            look_scan_translate = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--save-selection") == 0) {
            look_save_sel = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--word-card=", 12) == 0) {
            look_word_card = _argv[_i] + 12;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--translate-selection") == 0) {
            kana_translate_demo(true);
            look_translate_sel = true;
        }
    }
    kana_toolbar_init(&toolbar, _window, &ink, &canvas, &lasso, &viewer, &browse, &chart, &practice, &album, &notes, &check, &show_hud);
#if defined(RDE_PLATFORM_MOBILE)
    toolbar.finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    toolbar.selection = &selection;
    toolbar.exam      = &exam;
    toolbar.vocab     = &vocabview;
    toolbar.wordexam  = &wordexam;
    toolbar.stats     = &stats;
    toolbar.scan      = &scan;
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
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--scan-demo=", 12) == 0) {
            look_scan_demo = _argv[_i] + 12;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--scan-demo-turn=", 17) == 0) {
            look_scan_turn = strtof(_argv[_i] + 17, NULL);
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--scan-demo-top-first") == 0) {
            look_scan_rows = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--scan-live") == 0) {
            look_scan_live = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--deselect") == 0) {
            look_deselect = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--trim-fonts") == 0) {
            look_trim_fonts = true;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--data") == 0) {
            look_data = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--welcome", 9) == 0) {
            look_welcome = _argv[_i][9] == '=' ? (i32)strtol(_argv[_i] + 10, NULL, 10) - 1 : 0;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--data-export=", 14) == 0) {
            look_data_export = _argv[_i] + 14;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--data-import=", 14) == 0) {
            look_data_import = _argv[_i] + 14;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--sheet=", 8) == 0) {
            look_sheet = _argv[_i] + 8;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--data-replace") == 0) {
            look_data_replace = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--translate-probe=", 18) == 0) {
            look_translate_probe = _argv[_i] + 18;
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
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--vocab", 7) == 0) {
            look_vocab = _argv[_i][7] == '=' ? (i32)strtol(_argv[_i] + 8, NULL, 10) : 0;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--note=", 7) == 0) {
            look_note = _argv[_i] + 7;
        }
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--vocab-sample") == 0) {
            look_vocab_sample = true;
        }
        if(_argv[_i] != NULL && strncmp(_argv[_i], "--word-exam", 11) == 0) {
            look_word_exam = _argv[_i][11] == '=' ? (i32)strtol(_argv[_i] + 12, NULL, 10) : 0;
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
    if(look_note != NULL && viewer.open) {
        kana_charnote_set(kana_viewer_codepoint(&viewer), look_note);
    }
    if(look_vocab_sample && kana_vocab_count() == 0u) {
        static const c8* const _sample[][3] = {
            { "日本語", "にほんご", "Japanese (language)" }, { "勉強", "べんきょう", "study" }, { "食べる", "たべる", "to eat" }, { "東京", "とうきょう", "Tokyo" },
            { "本", "ほん", "book; volume; script" }, { "先生", "せんせい", "teacher; instructor; master" }, { "学生", "がくせい", "student" }, { "読む", "よむ", "to read" },
        };
        const u32 _list = kana_vocab_list_add("Lesson 1");
        for(u32 _w = 0; _w < sizeof(_sample) / sizeof(_sample[0]); _w++) {
            const u32 _id = kana_vocab_add(_sample[_w][0], _sample[_w][1], _sample[_w][2], 0u);
            if(_w % 2u == 0u) {
                kana_vocab_set_in_list(_list, _id, true);
            }
        }
        kana_vocab_list_add("Food");
    }
    if(look_word_exam >= 0) {
        const u32 _n   = kana_vocab_count();
        u32*      _ids = (u32*)rde_malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
        for(u32 _i = 0; _i < _n; _i++) {
            _ids[_i] = kana_vocab_at(_i)->id;
        }
        kana_wordexam_open(&wordexam, _ids, _n, kana_text(KANA_TEXT_VOCAB));
        rde_free(_ids);
        if(look_word_exam >= 1) {
            wordexam.by = look_word_exam == 2 ? KANA_WORDEXAM_BY_EAR : KANA_WORDEXAM_BY_MEANING;
            kana_wordexam_start(&wordexam);
        }
        for(u32 _i = 0; look_word_exam == 3 && _i < KANA_WORDEXAM_MAX && wordexam.stage == KANA_WORDEXAM_WRITING; _i++) {
            kana_wordexam_next(&wordexam);   // nothing written: the results, every word wrong
        }
    }
    if(look_vocab >= 0) {
        kana_vocabview_open(&vocabview);
        kana_vocabview_show_list(&vocabview, look_vocab > 0 ? kana_vocab_list_at((u32)look_vocab - 1u) : 0u);
    }
    if(look_kept_exam >= 0) {
        kana_exam_open_kept(&exam, (u32)look_kept_exam);
    }
    if(look_scan_live) {
        kana_scan_open(&scan, window, kana_canvas_from_screen(&canvas, (rde_vec_2F){ 0.0f, 0.0f }));
        kana_scan_camera(&scan);
    }
    if(look_scan_demo != NULL) {
        // A PNG (its size read from its header) with four lines across it, the
        // second left out: the screen as a photo read would show it.
        kana_scan_open(&scan, window, kana_canvas_from_screen(&canvas, (rde_vec_2F){ 0.0f, 0.0f }));   // no such file: the screen empty
        FILE* _f = fopen(look_scan_demo, "rb");
        if(_f != NULL) {
            static u8 _png[8u << 20];
            const usize _n = fread(_png, 1, sizeof(_png), _f);
            fclose(_f);
            static kana_textscan_line _demo[4];
            const c8* const _texts[4] = { "今日は、日本語を", "勉強します。", "Hello 漢字", "とかな" };
            for(u32 _l = 0; _l < 4u; _l++) {
                snprintf(_demo[_l].text, sizeof(_demo[_l].text), "%s", _texts[_l]);
                const f32 _y0 = 288.0f + 90.0f * (f32)_l, _y1 = _y0 + 64.0f;
                _demo[_l].corners[0] = (rde_vec_2F){ 86.0f, _y0 };
                _demo[_l].corners[1] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y0 };
                _demo[_l].corners[2] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y1 };
                _demo[_l].corners[3] = (rde_vec_2F){ 86.0f, _y1 };
                _demo[_l].block      = _l < 2u ? 0u : 1u;
            }
            const kana_textscan_result _r = { _png, _n, ((u32)_png[16] << 24) | ((u32)_png[17] << 16) | ((u32)_png[18] << 8) | _png[19],
                                              ((u32)_png[20] << 24) | ((u32)_png[21] << 16) | ((u32)_png[22] << 8) | _png[23], _demo, 4u };
            kana_scan_show(&scan, &_r);
            ((kana_scan_line*)scan.lines.memory)[1].kept = false;
            if(look_scan_translate) {
                kana_scan_translate(&scan);
            }
            scan.shown_top_first = look_scan_rows;
            if(look_scan_turn != 0.0f) {
                // The file holds the picture turned the other way: shown as a frame is.
                scan.shown_rotation = look_scan_turn;
                if(fmodf(fabsf(look_scan_turn), 180.0f) > 45.0f) {
                    const u32 _w = scan.picture_w;
                    scan.picture_w = scan.picture_h;
                    scan.picture_h = _w;
                }
            }
        }
    }
    kana_mlkit_prepare();   // after the settings: when ML Kit is on, its model downloads the first time (iOS; nothing elsewhere)

    rde_log_color(RDE_LOG_COLOR_GREEN, "%s",
                  "kana - ink spike. Pen writes, fingers move the page (2-finger tap undo, 3 redo), the toolbar has the rest. Keys: C clear, Z undo, Y redo, M raw samples, H HUD, R reset view, B brush scale.");
}

// A pointer for the viewer (over the others), Check, Browse, the chart or the
// album, whichever is open.
RDE_INTERNAL void kana_list_down(rde_vec_2F _screen, b8 _pen, f64 _now) {
    list_last = _screen;
    if(welcome.open)    { kana_welcome_pointer_down(&welcome, _screen); }   // over everything
    else if(viewer.open) { kana_viewer_pointer_down(&viewer, _screen, _now); }
    else if(scan.open)  { kana_scan_pointer_down(&scan, _screen, _now); }
    else if(exam.open)  { kana_exam_pointer_down(&exam, _screen, _pen, _now); }
    else if(wordexam.open) { kana_wordexam_pointer_down(&wordexam, _screen, _pen, _now); }
    else if(stats.open) { kana_stats_pointer_down(&stats, _screen, _now); }
    else if(vocabview.open) { kana_vocabview_pointer_down(&vocabview, _screen, _now); }
    else if(check.open) { kana_check_pointer_down(&check, _screen, _now); }
    else if(album.open) { kana_album_pointer_down(&album, _screen, _now); }
    else if(chart.open) { kana_chart_pointer_down(&chart, _screen, _now); }
    else                { kana_browse_pointer_down(&browse, _screen, _pen, _now); }
}
RDE_INTERNAL void kana_list_moved(rde_vec_2F _screen, f64 _now) {
    list_last = _screen;
    if(welcome.open)    { }
    else if(viewer.open) { kana_viewer_pointer_moved(&viewer, _screen, _now); }
    else if(scan.open)  { kana_scan_pointer_moved(&scan, _screen, _now); }
    else if(exam.open)  { kana_exam_pointer_moved(&exam, _screen, _now); }
    else if(wordexam.open) { kana_wordexam_pointer_moved(&wordexam, _screen, _now); }
    else if(stats.open) { kana_stats_pointer_moved(&stats, _screen, _now); }
    else if(vocabview.open) { kana_vocabview_pointer_moved(&vocabview, _screen, _now); }
    else if(check.open) { kana_check_pointer_moved(&check, _screen, _now); }
    else if(album.open) { kana_album_pointer_moved(&album, _screen, _now); }
    else if(chart.open) { kana_chart_pointer_moved(&chart, _screen, _now); }
    else                { kana_browse_pointer_moved(&browse, _screen, _now); }
}
RDE_INTERNAL void kana_list_up(f64 _now) {
    if(welcome.open)    { kana_welcome_pointer_up(&welcome, list_last); }
    else if(viewer.open) { kana_viewer_pointer_up(&viewer, _now); }
    else if(scan.open)  { kana_scan_pointer_up(&scan, _now); }
    else if(exam.open)  { kana_exam_pointer_up(&exam, _now); }
    else if(wordexam.open) { kana_wordexam_pointer_up(&wordexam, _now); }
    else if(stats.open) { kana_stats_pointer_up(&stats, _now); }
    else if(vocabview.open) { kana_vocabview_pointer_up(&vocabview, _now); }
    else if(check.open) { kana_check_pointer_up(&check, _now); }
    else if(album.open) { kana_album_pointer_up(&album, _now); }
    else if(chart.open) { kana_chart_pointer_up(&chart, _now); }
    else                { kana_browse_pointer_up(&browse, _now); }
}

// --- writing on the page: the pen's, and a writing finger's ---------------------------

RDE_INTERNAL void kana_page_write_down(rde_vec_2F _screen, b8 _from_pen, b8 _eraser) {
    kana_toolbar_close_context_menu(&toolbar);   // a press anywhere else dismisses it, and still does its job
    kana_canvas_release_fingers(&canvas);        // writing takes over from any finger gesture in progress
    if(toolbar.tool == KANA_TOOL_ERASE || _eraser) {
        erasing = true;   // the Erase tool, or a pen's own eraser end
        kana_erase_at_screen(_screen);
        return;
    }
    if(toolbar.tool == KANA_TOOL_LASSO) {
        kana_lasso_pen_down(&lasso, &ink, kana_screen_to_canvas(_screen), canvas.view.zoom);
        return;
    }
    kana_ink_begin(&ink, kana_screen_to_canvas(_screen), _from_pen, false);
}

RDE_INTERNAL void kana_page_write_moved(rde_vec_2F _screen) {
    if(erasing) {
        kana_erase_at_screen(_screen);
    } else if(kana_lasso_busy(&lasso)) {
        kana_lasso_pen_moved(&lasso, &ink, kana_canvas_from_screen(&canvas, _screen), canvas.view.zoom);
    } else if(ink.drawing) {
        kana_ink_extend(&ink, kana_screen_to_canvas(_screen));
    }
}

RDE_INTERNAL void kana_page_write_up(void) {
    erasing = false;
    kana_ink_erase_end(&ink);   // the whole swipe is one undo
    kana_ink_end(&ink);
    kana_lasso_pen_up(&lasso, &ink, canvas.view.zoom);
}

// The pending finger writes: from where it landed, through where it has been.
RDE_INTERNAL void kana_finger_start_writing(void) {
    ink.eraser      = false;
    ink.sample_time = rde_engine_get_time_now();
    kana_page_write_down(finger_ink.points[0], false, false);
    for(u32 _i = 1; _i < finger_ink.count; _i++) {
        kana_page_write_moved(finger_ink.points[_i]);
    }
    finger_ink.state = KANA_FINGER_WRITING;
}

RDE_INTERNAL f32 kana_finger_moved_from_start(rde_vec_2F _p) {
    const rde_vec_2F _d = { _p.x - finger_ink.points[0].x, _p.y - finger_ink.points[0].y };
    return sqrtf(_d.x * _d.x + _d.y * _d.y);
}

RDE_INTERNAL void kana_finger_down(u64 _id, rde_vec_2F _screen) {
    if(finger_ink.state == KANA_FINGER_WRITING || finger_ink.state == KANA_FINGER_IGNORED) {
        return;   // a resting hand
    }
    if(finger_ink.state == KANA_FINGER_PENDING) {
        // A second finger before the first wrote: both are the page's gesture.
        kana_canvas_finger_down(&canvas, finger_ink.id, finger_ink.points[0]);
        if(finger_ink.count > 1) {
            kana_canvas_finger_moved(&canvas, finger_ink.id, finger_ink.points[finger_ink.count - 1]);
        }
        kana_canvas_finger_down(&canvas, _id, _screen);
        finger_ink.state   = KANA_FINGER_GESTURE;
        finger_ink.gesture = 2;
        return;
    }
    if(finger_ink.state == KANA_FINGER_GESTURE) {
        kana_canvas_finger_down(&canvas, _id, _screen);
        finger_ink.gesture++;
        return;
    }
    kana_toolbar_close_context_menu(&toolbar);
    finger_ink.state     = KANA_FINGER_PENDING;
    finger_ink.id        = _id;
    finger_ink.points[0] = _screen;
    finger_ink.count     = 1;
    finger_ink.since     = rde_engine_get_time_now();
}

RDE_INTERNAL void kana_finger_moved(u64 _id, rde_vec_2F _screen) {
    if(finger_ink.state == KANA_FINGER_GESTURE) {
        kana_canvas_finger_moved(&canvas, _id, _screen);
        return;
    }
    if(_id != finger_ink.id) {
        return;
    }
    if(finger_ink.state == KANA_FINGER_PENDING) {
        if(finger_ink.count < KANA_FINGER_POINTS) {
            finger_ink.points[finger_ink.count++] = _screen;
        } else {
            finger_ink.points[KANA_FINGER_POINTS - 1u] = _screen;
        }
        if(kana_finger_moved_from_start(_screen) > KANA_FINGER_MOVE) {
            kana_finger_start_writing();
        }
    } else if(finger_ink.state == KANA_FINGER_WRITING) {
        ink.sample_time = rde_engine_get_time_now();
        kana_page_write_moved(_screen);
    }
}

RDE_INTERNAL void kana_finger_up(u64 _id) {
    if(finger_ink.state == KANA_FINGER_GESTURE) {
        // Two fingers tapped: undo. Three: redo.
        const KANA_CANVAS_TAP_ _tap = kana_canvas_finger_up(&canvas, _id);
        if(_tap == KANA_CANVAS_TAP_TWO) {
            kana_ink_undo(&ink);
        } else if(_tap == KANA_CANVAS_TAP_THREE) {
            kana_ink_redo(&ink);
        }
        finger_ink.gesture = finger_ink.gesture > 0 ? finger_ink.gesture - 1u : 0u;
        if(finger_ink.gesture == 0) {
            finger_ink.state = KANA_FINGER_NONE;
        }
        return;
    }
    if(_id != finger_ink.id) {
        return;
    }
    if(finger_ink.state == KANA_FINGER_PENDING) {
        kana_finger_start_writing();   // a short touch: a dot
    }
    if(finger_ink.state == KANA_FINGER_WRITING) {
        kana_page_write_up();
    }
    finger_ink.state = KANA_FINGER_NONE;
}

// Once a frame (a finger at rest sends nothing): a pending finger that rested a
// moment with a little movement writes; one held quite still is the long press.
RDE_INTERNAL void kana_finger_poll(void) {
    if(finger_ink.state != KANA_FINGER_PENDING) {
        return;
    }
    const f64 _held  = rde_engine_get_time_now() - finger_ink.since;
    const f32 _moved = kana_finger_moved_from_start(finger_ink.points[finger_ink.count - 1]);
    if(_held >= KANA_CANVAS_LONG_PRESS_TIME && _moved < KANA_FINGER_MOVE) {
        kana_toolbar_open_context_menu(&toolbar, finger_ink.points[0], kana_canvas_from_screen(&canvas, finger_ink.points[0]));
        finger_ink.state = KANA_FINGER_IGNORED;   // until it lifts
    } else if(_held >= KANA_FINGER_WAIT && _moved > 1.5f) {
        kana_finger_start_writing();
    }
}

// Lets a finger's writing go (a pen came down, or a screen opened over the page).
RDE_INTERNAL void kana_finger_forget(void) {
    if(finger_ink.state == KANA_FINGER_WRITING) {
        kana_page_write_up();
    }
    if(finger_ink.state == KANA_FINGER_GESTURE) {
        kana_canvas_release_fingers(&canvas);
    }
    memset(&finger_ink, 0, sizeof(finger_ink));
}

// A pen came down, anywhere: the pen writes from now on (the hand goes off).
RDE_INTERNAL void kana_pen_came(void) {
    pen_ever = true;
    if(toolbar.finger_writes) {
        kana_finger_forget();
        kana_toolbar_set_finger_writes(&toolbar, false);
    }
}

// Practice: the pen (and the desktop mouse) writes — and, with the hand on, one
// finger; otherwise a resting hand does nothing.
RDE_INTERNAL void kana_practice_event(rde_event* _event) {
    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _screen = kana_window_to_world(_event->data.pen_event_data.position);
            kana_pen_came();
            if(browse_pointer == KANA_POINTER_FINGER) {
                kana_practice_pen_up(&practice);   // the pen takes over from a writing finger
                browse_pointer = KANA_POINTER_NONE;
            }
            if(browse_pointer == KANA_POINTER_NONE && !kana_toolbar_hit(&toolbar, _screen)) {
                browse_pointer = KANA_POINTER_PEN;
                kana_practice_pen_down(&practice, _screen);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            const rde_vec_2F        _pos   = kana_touch_to_screen(_touch->init_touch_position);
            if(toolbar.finger_writes && !_touch->from_pen && browse_pointer == KANA_POINTER_NONE && !kana_toolbar_hit(&toolbar, _pos)) {
                browse_pointer = KANA_POINTER_FINGER;
                browse_finger  = _touch->finger_id;
                kana_practice_pen_down(&practice, _pos);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && browse_pointer == KANA_POINTER_FINGER && _touch->finger_id == browse_finger) {
                kana_practice_pen_moved(&practice, kana_touch_to_screen(_touch->moved_touch_position));
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && browse_pointer == KANA_POINTER_FINGER && _touch->finger_id == browse_finger) {
                kana_practice_pen_up(&practice);
                browse_pointer = KANA_POINTER_NONE;
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
            kana_pen_came();
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
                kana_list_down(_pos, toolbar.finger_writes, _now);   // the hand on: it writes where a pen would
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

    // Out of sight, or the OS short of memory: the fonts' glyph memory goes back
    // to what they started with (each glyph uploads again when next drawn). An
    // event comes before the frame draws, as rde_font_trim needs.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        kana_toolbar_trim_fonts(&toolbar);
    }

    // The screens have the whole screen: nothing reaches the page (their buttons
    // are UI and have had the event already). Leaving the app still saves. The
    // top screen gets the pointer.
    if(welcome.open || practice.open || viewer.open || browse.open || chart.open || album.open || check.open || exam.open || stats.open || scan.open ||
       vocabview.open || wordexam.open) {
        if(finger_ink.state != KANA_FINGER_NONE) {
            kana_finger_forget();   // a screen over the page: a finger's writing there ends
        }
        if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
           _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
            kana_save_on_exit();
            kana_scan_pause(&scan);   // the camera stops: what was in view stays
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

            kana_pen_came();
            ink.pen_seen    = true;
            ink.eraser      = _pen->eraser;
            ink.sample_time = (f64)_event->time_stamp * 1e-9; // SDL event time, ns

            // On the toolbar the pen is pressing a button — the UI gets it as a
            // synthetic mouse click right after this event — so it must not also
            // write underneath. (Translate with Google's card is drawn by Kana:
            // its speaker answers here.)
            kana_toolbar_card_press(&toolbar, _screen);
            if(kana_toolbar_hit(&toolbar, _screen)) {
                pen_on_ui = true;
                break;
            }

            kana_page_write_down(_screen, true, _pen->eraser);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            const rde_event_pen* _pen = &_event->data.pen_event_data;

            ink.pen_seen = true;

            if(pen_on_ui) {
                break;
            }

            // Only while the tip is down (kana_page_write_moved draws only then): a
            // pen hovering in proximity still reports motion, and inking on hover
            // would be a mess.
            ink.sample_time = (f64)_event->time_stamp * 1e-9; // SDL event time, ns
            kana_page_write_moved(kana_window_to_world(_pen->position));
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            ink.pen_seen = true;
            pen_on_ui    = false;
            kana_page_write_up();
        } break;

        // Apple Pencil's double tap: what the learner set it to do (the system's
        // Apple Pencil settings) — the eraser and back, most often.
        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            ink.pen_seen = true;
            kana_toolbar_pen_double_tap(&toolbar, _event->data.pen_event_data.tap_action);
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
            if(!ink.drawing && !erasing) {
                kana_toolbar_card_press(&toolbar, _pos);   // the translation card's speaker
            }
            if(kana_toolbar_hit(&toolbar, _pos)) {
                break;
            }
            if(toolbar.finger_writes) {
                kana_finger_down(_touch->finger_id, _pos);   // one finger writes; two move the page
                break;
            }
            kana_toolbar_close_context_menu(&toolbar);
            if(!ink.drawing && !erasing && !kana_lasso_busy(&lasso)) {
                kana_canvas_finger_down(&canvas, _touch->finger_id, _pos);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(finger_ink.state != KANA_FINGER_NONE) {
                kana_finger_moved(_touch->finger_id, kana_touch_to_screen(_touch->moved_touch_position));
                break;
            }
            if(ink.drawing) {
                break;
            }

            kana_canvas_finger_moved(&canvas, _touch->finger_id, kana_touch_to_screen(_touch->moved_touch_position));
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(finger_ink.state != KANA_FINGER_NONE) {
                kana_finger_up(_touch->finger_id);
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
            if(!_event->handled) {
                kana_toolbar_card_press(&toolbar, _screen);   // the translation card's speaker
            }
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

// --translate-probe=xx: Japanese into xx with ML Kit's translator, on the device
// itself — its models downloaded when missing, then a few sentences — written to
// <save dir>/translate.txt with the times. Three minutes at most. The app then
// carries on as usual: an iOS app cannot quit itself (stopping RDE's loop only
// freezes it on screen).
RDE_INTERNAL void kana_translate_probe(void) {
    static const c8* const _sentences[4] = { "今日は日本語を勉強します。", "駅はどこですか？", "この本はとても面白かったです。", "憂鬱" };
    static f64 _start, _ready_at, _sent_at;
    static u32 _tickets[4], _got;
    static b8  _sent;
    static c8  _answers[4][KANA_TRANSLATE_TEXT];
    static f64 _took[4];
    const f64 _now = rde_engine_get_time_now();
    if(_start == 0.0) {
        _start = _now;
        kana_translate_prepare(look_translate_probe);
    }
    const KANA_TRANSLATE_STATE_ _state = kana_translate_state(look_translate_probe);
    if(_state == KANA_TRANSLATE_READY && !_sent) {
        _sent     = true;
        _ready_at = _sent_at = _now;
        for(u32 _i = 0; _i < 4u; _i++) {
            _tickets[_i] = kana_translate_text(_sentences[_i], look_translate_probe);
        }
    }
    u32 _ticket;
    c8  _out[KANA_TRANSLATE_TEXT];
    while(kana_translate_poll(&_ticket, _out, sizeof(_out))) {
        for(u32 _i = 0; _i < 4u; _i++) {
            if(_tickets[_i] != 0u && _tickets[_i] == _ticket) {
                snprintf(_answers[_i], sizeof(_answers[_i]), "%s", _out);
                _took[_i] = _now - _sent_at;
                _got++;
            }
        }
    }
    if((_sent && _got == 4u) || _state == KANA_TRANSLATE_FAILED || _state == KANA_TRANSLATE_UNAVAILABLE || _now - _start > 180.0) {
        c8 _path[RDE_MAX_PATH];
        snprintf(_path, sizeof(_path), "%stranslate.txt", kana_save_dir());
        FILE* _f = fopen(_path, "a");
        if(_f != NULL) {
            fprintf(_f, "ja -> %s: state %d, ready after %.1f s, %u of 4 answered\n", look_translate_probe, (i32)_state,
                    _ready_at > 0.0 ? _ready_at - _start : -1.0, _got);
            for(u32 _i = 0; _i < 4u; _i++) {
                fprintf(_f, "  %s -> %s (%.0f ms)\n", _sentences[_i], _answers[_i], 1000.0 * _took[_i]);
            }
            fclose(_f);
        }
        rde_log_level(RDE_LOG_LEVEL_INFO, "kana translate probe: written to %s", _path);
        look_translate_probe = NULL;
    }
}

RDE_INTERNAL void kana_update(f32 _dt) {
    if(baking) {
        return;
    }
    if(look_translate_probe != NULL) {
        kana_translate_probe();
    }
    kana_follow_language();   // a language chosen: the UI and the meanings in it
#if defined(RDE_PLATFORM_IOS)
    // Apple Pencil's double tap: asked for once the app's view is there (a few
    // frames at most after launch).
    static b8  _pencil_taps = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
    }
#endif
    kana_data_update();       // Settings › Your data: export, import
    kana_sheet_update();      // a practice sheet: shared, or saved where chosen
    if(toolbar.side.welcome_request) {
        toolbar.side.welcome_request = false;
        kana_welcome_open(&welcome);
    }
    toolbar.covered = welcome.open;   // the bar and the menu hide under it
    if(kana_speech_take_hint()) {
        kana_toolbar_notice(&toolbar, kana_text(KANA_TEXT_SPEECH_BETTER_VOICE));   // the basic voice spoke: where better ones are
    }
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
        if(look_shot_frames == 28u && look_translate_sel) {
            kana_toolbar_translate_selection(&toolbar);
        }
        if(look_shot_frames == 28u && look_save_sel) {
            kana_toolbar_save_selection(&toolbar);
        }
        if(look_shot_frames == 28u && look_word_card != NULL) {
            kana_wordcard_ask_read(&kanji_db, look_word_card);
        }
        if(look_shot_frames == 10u && look_welcome >= 0) {
            kana_welcome_open(&welcome);
            welcome.page = (u32)look_welcome < KANA_WELCOME_PAGES ? (u32)look_welcome : 0u;
        }
        if(look_shot_frames == 15u && look_data) {
            kana_side_open_settings(&toolbar, -1);
            toolbar.side.data_open = true;
        }
        if(look_shot_frames == 20u && look_data_export != NULL) {
            kana_data_export_to(look_data_export, false);
        }
        if(look_shot_frames == 20u && look_sheet != NULL && viewer.open && rde_arr_length(&viewer.list) > 0) {
            kana_sheet_make(&((const u32*)viewer.list.memory)[viewer.position], 1u, false, look_sheet, false);
        }
        if(look_shot_frames == 22u && look_data_import != NULL) {
            const c8* const _picked[2] = { look_data_import, NULL };
            kana_data_on_open_path(_picked, 1u, -1, NULL);
        }
        if(look_shot_frames == 26u && look_data_replace) {
            toolbar.side.data_request = KANA_SIDE_DATA_REPLACE;
        }
        if(look_shot_frames == 40u && look_trim_fonts) {
            kana_toolbar_trim_fonts(&toolbar);   // the shot then shows every glyph uploaded again
        }
        if(look_shot_frames == 30u && look_scroll > 0.0f) {
            stats.scroller.offset       = look_scroll;
            album.scroller.offset       = look_scroll;
            album.page_scroller.offset  = look_scroll;
            scan.taps.offset            = look_scroll;   // the panel of translations
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

    // Text from a photo: the photo read, and the lines kept written on the page
    // (Paste text's way) where its menu was opened.
    if(scan.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_scan_update(&scan, _dt);
        static c8 _lines[KANA_TEXTSCAN_LINES * 64u];
        if(kana_scan_take_text(&scan, _lines, sizeof(_lines))) {
            const rde_vec_2F _at = scan.canvas_at;
            kana_scan_close(&scan);
            kana_toolbar_paste_text(&toolbar, _lines, _at);
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_scan_close(&scan); }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    // A word exam: its answers read as it goes; a tapped result opens the word card.
    if(wordexam.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_wordexam_update(&wordexam, _dt);
        u32 _word;
        if(kana_wordexam_take_tap(&wordexam, &_word)) {
            kana_wordcard_ask_saved(_word);
        }
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_wordexam_close(&wordexam); }
        kana_toolbar_update(&toolbar);
        kana_autosave();
        return;
    }

    // The Vocabulary screen.
    if(vocabview.open && !stats.open) {
#if !defined(RDE_PLATFORM_MOBILE)
        if(browse_pointer == KANA_POINTER_MOUSE) {
            const rde_vec_2I _m = rde_input_mouse_get_position(window);
            kana_list_moved((rde_vec_2F){ (f32)_m.x, (f32)_m.y }, rde_engine_get_time_now());
        }
#endif
        kana_vocabview_update(&vocabview, _dt);
        if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_ESCAPE)) { kana_vocabview_close(&vocabview); }
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

#if KANA_DEVELOPER
    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_M)) {
        show_samples = !show_samples;
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_H)) {
        show_hud = !show_hud;
        kana_toolbar_sync(&toolbar);
    }
#endif

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

    // A writing finger's moment to decide, and its long press (polled: a finger
    // at rest sends nothing).
    kana_finger_poll();

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
    } else if(scan.open) {
        kana_scan_render(&scan, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.scan_menu.size.y + 24.0f);
    } else if(wordexam.open) {
        kana_wordexam_render(&wordexam, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.vocab_menu.size.y + 24.0f);
    } else if(stats.open) {
        kana_stats_render(&stats, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.stats_menu.size.y + 24.0f);
    } else if(vocabview.open) {
        kana_vocabview_render(&vocabview, _window, font, font_px, _hh - (f32)_safe.y - 8.0f, -_hh + (f32)_safe.w + toolbar.vocab_menu.size.y + 24.0f);
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
        kana_toolbar_render_translation(&toolbar, _window);   // Translate with Google, by the selection
        kana_draw_notice(_window);
    }
    kana_welcome_render(&welcome, _window, font, font_px);   // over everything
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
        fprintf(_f, "%s| %u frames in %.1f s: %.1f fps, frame %.2f ms avg, %.2f ms worst, %u over 20 ms | update %.2f ms avg, %.2f worst | render %.2f ms avg, %.2f worst",
                perf_label, perf_frames, _span, _n / _span, 1000.0 * perf_dt_sum / _n, 1000.0 * perf_dt_max, perf_slow,
                1000.0 * perf_update_sum / _n, 1000.0 * perf_update_max, 1000.0 * perf_render_sum / _n, 1000.0 * perf_render_max);
        if(scan.open) {   // the camera: frames shown, and ML Kit's reads of them
            fprintf(_f, " | camera %u frames shown, %u read, %.0f ms a read", scan.frames_shown, scan.frames_read,
                    scan.frames_read > 0 ? 1000.0 * scan.read_seconds / (f64)scan.frames_read : 0.0);
        }
        fprintf(_f, "\n");
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
