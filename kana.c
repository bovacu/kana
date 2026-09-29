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

#include <stdio.h>
#include <string.h>

#include "ink.h"
#include "canvas.h"
#include "toolbar.h"
#include "lasso.h"
#include "save.h"

#define KANA_CONFIG_PATH "./assets/config.rdef"

RDE_INTERNAL rde_window* window;
RDE_INTERNAL rde_camera  camera;
RDE_INTERNAL rde_font*   font;
RDE_INTERNAL kana_ink    ink;
RDE_INTERNAL kana_canvas canvas;
RDE_INTERNAL kana_toolbar toolbar;
RDE_INTERNAL kana_lasso   lasso;

// The pen went down ON the toolbar: it is pressing a button, so nothing it does
// until it lifts may write, erase or pan.
RDE_INTERNAL b8 pen_on_ui = false;
// The pen (or mouse) is down with the Erase tool: its path erases.
RDE_INTERNAL b8 erasing   = false;

RDE_INTERNAL b8  show_samples = false;
RDE_INTERNAL b8  show_hud     = true;

// Frame time, smoothed. The raw value jitters too much to read off a screen.
RDE_INTERNAL f32 frame_ms = 0.0f;

// --- saving --------------------------------------------------------------------
//
// The page and the settings save themselves this long after the last change, and
// never mid-stroke — so an iPadOS kill loses at most this much writing. Also on
// going to the background, but that event reaches on_event through the queue and
// may not be handled before the app is suspended: the autosave is the guarantee.
#define KANA_AUTOSAVE_DELAY 1.0

RDE_INTERNAL c8            document_path[RDE_MAX_PATH];
RDE_INTERNAL c8            settings_path[RDE_MAX_PATH];

// What was last written, and what was last seen (to time the quiet period).
RDE_INTERNAL u32           saved_revision = 0;
RDE_INTERNAL kana_view     saved_view;
RDE_INTERNAL kana_settings saved_settings;
RDE_INTERNAL u32           seen_revision  = 0;
RDE_INTERNAL kana_view     seen_view;
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

// HUD text: dark, on the paper page.
#define KANA_HUD_TEXT_COLOR (rde_color){ 60, 60, 70, 255 }

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
    return _s;
}

RDE_INTERNAL void kana_apply_settings(const kana_settings* _s) {
    toolbar.tool         = _s->tool == KANA_TOOL_ERASE ? KANA_TOOL_ERASE : _s->tool == KANA_TOOL_LASSO ? KANA_TOOL_LASSO : KANA_TOOL_DRAW;
    show_hud             = _s->show_hud;
    ink.brush_scale      = _s->brush_scale == KANA_INK_BRUSH_SCALE_SCREEN ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
    ink.width_mode       = _s->width_mode == KANA_INK_WIDTH_MODE_PRESSURE ? KANA_INK_WIDTH_MODE_PRESSURE : KANA_INK_WIDTH_MODE_CONSTANT;
    ink.color            = _s->color;
    ink.constant_radius  = rde_math_clamp_f32(_s->radius, KANA_TOOLBAR_SIZE_MIN, KANA_TOOLBAR_SIZE_MAX);
    kana_toolbar_set_placement(&toolbar, _s->vertical, _s->toolbar_center);
    kana_toolbar_sync(&toolbar);
}

// Writes whatever changed since the last save (_force: both files regardless).
// Not while a stroke is open — the caller closes it first when it must save now.
RDE_INTERNAL void kana_save_now(b8 _force) {
    if(ink.drawing) {
        return;
    }

    const kana_settings _settings = kana_gather_settings();
    const b8 _doc_dirty  = _force || ink.revision != saved_revision || !kana_view_same(canvas.view, saved_view);
    const b8 _set_dirty  = _force || !kana_settings_equal(&_settings, &saved_settings);

    if(!_doc_dirty && !_set_dirty) {
        return;
    }

    const f64 _start = rde_engine_get_time_now();
    b8        _ok    = true;

    if(_doc_dirty) {
        u32 _bytes = 0;
        if(kana_save_document(document_path, &ink, canvas.view, &_bytes)) {
            saved_revision  = ink.revision;
            saved_view      = canvas.view;
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

    if(ink.revision != seen_revision || !kana_view_same(canvas.view, seen_view) || !kana_settings_equal(&_settings, &seen_settings)) {
        seen_revision    = ink.revision;
        seen_view        = canvas.view;
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
    snprintf(document_path, sizeof(document_path), "%s%s", _dir, KANA_SAVE_DOCUMENT_FILE);
    snprintf(settings_path, sizeof(settings_path), "%s%s", _dir, KANA_SAVE_SETTINGS_FILE);

    kana_settings _settings = kana_gather_settings();
    if(kana_load_settings(settings_path, &_settings) == KANA_LOAD_OK) {
        kana_apply_settings(&_settings);
    }

    const KANA_LOAD_ _doc = kana_load_document(document_path, &ink, &canvas.view);
    if(_doc == KANA_LOAD_OK) {
        snprintf(load_note, sizeof(load_note), "loaded %u strokes", kana_ink_alive_strokes(&ink));
    } else if(_doc == KANA_LOAD_RECOVERED) {
        snprintf(load_note, sizeof(load_note), "PAGE FILE MISSING/DAMAGED - loaded the backup, %u strokes", kana_ink_alive_strokes(&ink));
    } else if(_doc == KANA_LOAD_CORRUPT) {
        snprintf(load_note, sizeof(load_note), "PAGE FILE DAMAGED - kept as %s.bad, starting empty", KANA_SAVE_DOCUMENT_FILE);
    } else {
        snprintf(load_note, sizeof(load_note), "new page");
    }
    rde_log_color(RDE_LOG_COLOR_GREEN, "kana: saves in %s (%s)", _dir, load_note);

    // What is on screen now IS what is saved.
    saved_revision   = seen_revision  = ink.revision;
    saved_view       = seen_view      = canvas.view;
    saved_settings   = seen_settings  = kana_gather_settings();
    last_change_time = rde_engine_get_time_now();
}

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);

    window = _window;
    font   = rde_font_get_default_missing();
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);

    kana_ink_init(&ink);
    kana_canvas_init(&canvas);
    kana_lasso_init(&lasso);
    kana_toolbar_init(&toolbar, _window, &ink, &canvas, &lasso, &show_hud);

    rde_rendering_clear_background_color(KANA_CANVAS_PAGE_COLOR);

    kana_load_saves();

    rde_log_color(RDE_LOG_COLOR_GREEN, "%s",
                  "kana - ink spike. Pen writes, fingers move the page (2-finger tap undo, 3 redo), the toolbar has the rest. Keys: C clear, Z undo, Y redo, M raw samples, H HUD, R reset view, B brush scale.");
}

void on_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);

    if(_event == NULL) {
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

void on_update(f32 _dt) {
    kana_ink_frame_begin(&ink);

    // Smoothed hard: an unsmoothed frame time is unreadable on screen.
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

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

RDE_INTERNAL void kana_draw_text(const c8* _text, f32 _x, f32 _y) {
    rde_rendering_2d_draw_text_1(font, _text, (rde_vec_3F){ _x, _y, 0.0f }, KANA_HUD_TEXT_COLOR);
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

    kana_draw_text(_source, _x, _y);
    _y -= 34.0f;

    // Whether this pen has real pressure, or its ink width comes from speed.
    const c8* _pressure_mode = !ink.pen_seen      ? ""
                             : ink.pressure_live  ? " [real]"
                                                  : " [none - width from speed]";

    snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)%s   tilt %.0f / %.0f   pen id %u   %s",
             (f64)ink.pressure, (f64)ink.pressure_raw, _pressure_mode,
             (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id,
             ink.eraser ? "ERASER" : "tip");
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    // The number that decides whether fast strokes can look smooth.
    snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms",
             (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end, see the header",
             (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "%u strokes, %u points   history %u (+%u redo)",
             kana_ink_alive_strokes(&ink), kana_ink_total_points(&ink),
             kana_ink_undo_steps(&ink), kana_ink_redo_steps(&ink));
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "zoom %.0f%%   fingers: 1 pan, 2 pinch   tool: %s",
             (f64)(canvas.view.zoom * 100.0f),
             toolbar.tool == KANA_TOOL_ERASE ? "ERASE" : toolbar.tool == KANA_TOOL_LASSO ? "LASSO" : "DRAW");
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    if(save_failed) {
        snprintf(_line, sizeof(_line), "SAVE FAILED - retrying   (%s)", load_note);
    } else if(last_save_time > 0.0) {
        snprintf(_line, sizeof(_line), "saved %.0f s ago: %.1f KB in %.1f ms   (%s)",
                 rde_engine_get_time_now() - last_save_time, (f64)last_save_bytes / 1024.0, (f64)last_save_ms, load_note);
    } else {
        snprintf(_line, sizeof(_line), "not saved yet this run   (%s)", load_note);
    }
    kana_draw_text(_line, _x, _y);
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "brush: %s",
             ink.brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? "PAGE - same width on the page at any zoom"
                                                          : "SCREEN - same width on screen while writing");
    kana_draw_text(_line, _x, _y);
}

void on_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);

    rde_rendering_2d_begin_drawing(_window, &camera);
    {
        const rde_vec_2I _size = rde_window_get_size(_window);
        kana_canvas_draw_grid(&canvas, _size);
        kana_lasso_render_under(&lasso, &ink, canvas.view.offset, canvas.view.zoom);
        kana_ink_render(&ink, canvas.view.offset, canvas.view.zoom, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f },
                        rde_engine_get_time_now(), show_samples);
        kana_lasso_render_over(&lasso, &ink, canvas.view.offset, canvas.view.zoom);

        if(show_hud && font != NULL) {
            kana_draw_hud(_window);
        }
    }
    rde_rendering_2d_end_drawing();
}

void on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

void end_func(void) {
    kana_save_on_exit();
    kana_toolbar_destroy(&toolbar);
    kana_lasso_destroy(&lasso);
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
