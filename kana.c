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
//   PEN / FINGER   draw
//   C              clear
//   M              show every raw input sample as a dot
//   H              hide the HUD (for photographing the nib-to-ink gap)
// ===========================================================================

#include "rde.h"

#include <stdio.h>
#include <string.h>

#include "ink.h"

#define KANA_CONFIG_PATH "./assets/config.rdef"

RDE_INTERNAL rde_window* window;
RDE_INTERNAL rde_camera  camera;
RDE_INTERNAL rde_font*   font;
RDE_INTERNAL kana_ink    ink;

RDE_INTERNAL b8  show_samples = false;
RDE_INTERNAL b8  show_hud     = true;

// Frame time, smoothed. The raw value jitters too much to read off a screen.
RDE_INTERNAL f32 frame_ms = 0.0f;

// Set once a touch has drawn anything, so the HUD can distinguish "no input at
// all" (nothing is reaching the app) from "touch only" (input works, pen does
// not) — which are completely different failures.
RDE_INTERNAL b8  touch_seen = false;

// --- Window pixels to 2D world ------------------------------------------------
//
// Pen and touch positions arrive WINDOW-RELATIVE: top-left origin, Y down. The
// 2D camera is centre-origin with Y up. Every input position has to make this
// trip, and getting it wrong shows as ink mirrored about the middle of the
// screen rather than as anything subtle.
RDE_INTERNAL rde_vec_2F kana_window_to_world(rde_vec_2F _pixel) {
    const rde_vec_2I _size = rde_window_get_size(window);

    return (rde_vec_2F){
        _pixel.x - (f32)_size.x * 0.5f,
        (f32)_size.y * 0.5f - _pixel.y
    };
}

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);

    window = _window;
    font   = rde_font_get_default_missing();
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);

    kana_ink_init(&ink);

    rde_log_color(RDE_LOG_COLOR_GREEN, "%s",
                  "kana - ink spike. Draw with the pen. C clears, M shows raw samples, H hides the HUD.");
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

            ink.pen_seen = true;
            ink.eraser   = _pen->eraser;

            kana_ink_begin(&ink, kana_window_to_world(_pen->position), true, _pen->eraser);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            const rde_event_pen* _pen = &_event->data.pen_event_data;

            ink.pen_seen = true;

            // Only while the tip is down. A pen hovering in proximity still
            // reports motion, and inking on hover would be a mess.
            if(ink.drawing) {
                kana_ink_extend(&ink, kana_window_to_world(_pen->position));
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            ink.pen_seen = true;
            kana_ink_end(&ink);
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_IN:
        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            ink.pen_seen = true;
        } break;

        // --- touch, as the fallback --------------------------------------
        //
        // Present ONLY to tell "the pen is not supported" apart from "nothing is
        // reaching the app". If the finished app ever ships finger drawing it
        // should be a deliberate choice, not this.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            touch_seen = true;

            kana_ink_begin(&ink, kana_window_to_world((rde_vec_2F){
                (f32)_touch->init_touch_position.x, (f32)_touch->init_touch_position.y }), false, false);
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            touch_seen = true;

            kana_ink_extend(&ink, kana_window_to_world((rde_vec_2F){
                (f32)_touch->moved_touch_position.x, (f32)_touch->moved_touch_position.y }));
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            touch_seen = true;
            kana_ink_end(&ink);
        } break;

        // --- mouse, so the spike is usable on the desktop while developing ---
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                const rde_vec_2I _m = rde_input_mouse_get_position(window);
                const rde_vec_2I _size = rde_window_get_size(window);

                // The mouse reports CENTRE-ORIGIN already, unlike pen and touch.
                RDE_UNUSED(_size);
                kana_ink_begin(&ink, (rde_vec_2F){ (f32)_m.x, (f32)_m.y }, false, false);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                kana_ink_end(&ink);
            }
        } break;

        default: break;
    }
}

void on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

void on_update(f32 _dt) {
    kana_ink_frame_begin(&ink, rde_engine_get_time_now());

    // Smoothed hard: an unsmoothed frame time is unreadable on screen.
    frame_ms += ((_dt * 1000.0f) - frame_ms) * 0.1f;

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_C)) {
        kana_ink_clear(&ink);
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_M)) {
        show_samples = !show_samples;
    }

    if(rde_input_key_is_just_pressed(window, RDE_KEYBOARD_KEY_H)) {
        show_hud = !show_hud;
    }

    // Desktop dragging, so the thing can be exercised without a tablet in hand.
    if(ink.drawing && rde_input_mouse_is_button_pressed(window, RDE_MOUSE_BUTTON_LEFT)) {
        const rde_vec_2I _m = rde_input_mouse_get_position(window);
        kana_ink_extend(&ink, (rde_vec_2F){ (f32)_m.x, (f32)_m.y });
    }
}

void on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

RDE_INTERNAL void kana_draw_hud(rde_window* _window) {
    const rde_vec_2I _size = rde_window_get_size(_window);
    const f32 _x = -(f32)_size.x * 0.5f + 20.0f;
    f32       _y =  (f32)_size.y * 0.5f - 34.0f;

    c8 _line[192];

    // THE HEADLINE, and the reason this spike exists. Answers open question 1 at
    // a glance, from across the room, without reading a log.
    const c8* _source = ink.pen_seen  ? "PEN  (stylus events arriving)"
                      : touch_seen    ? "TOUCH ONLY  - no pen events!"
                                      : "waiting for input...";

    rde_rendering_2d_draw_text(font, _source, (rde_vec_3F){ _x, _y, 0.0f });
    _y -= 34.0f;

    snprintf(_line, sizeof(_line), "pressure %.2f (raw %.2f)   tilt %.0f / %.0f   pen id %u   %s",
             (f64)ink.pressure, (f64)ink.pressure_raw,
             (f64)ink.tilt.x, (f64)ink.tilt.y, ink.pen_id,
             ink.eraser ? "ERASER" : "tip");
    rde_rendering_2d_draw_text(font, _line, (rde_vec_3F){ _x, _y, 0.0f });
    _y -= 28.0f;

    // The number that decides whether fast strokes can look smooth.
    snprintf(_line, sizeof(_line), "input %.0f Hz   %u samples this frame   frame %.1f ms",
             (f64)ink.sample_hz, ink.samples_this_frame, (f64)frame_ms);
    rde_rendering_2d_draw_text(font, _line, (rde_vec_3F){ _x, _y, 0.0f });
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "app-side lag %.1f ms (peak %.1f)   -- NOT end-to-end, see the header",
             (f64)ink.stale_ms, (f64)ink.stale_ms_max);
    rde_rendering_2d_draw_text(font, _line, (rde_vec_3F){ _x, _y, 0.0f });
    _y -= 28.0f;

    snprintf(_line, sizeof(_line), "%u strokes, %u points%s%s",
             ink.stroke_count, kana_ink_total_points(&ink),
             ink.dropped_points  ? "   POINT LIMIT HIT"  : "",
             ink.dropped_strokes ? "   STROKE LIMIT HIT" : "");
    rde_rendering_2d_draw_text(font, _line, (rde_vec_3F){ _x, _y, 0.0f });
    _y -= 28.0f;

    rde_rendering_2d_draw_text(font, "C clear    M raw samples    H hide this",
                               (rde_vec_3F){ _x, _y, 0.0f });
}

void on_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);

    rde_rendering_2d_begin_drawing(_window, &camera);
    {
        kana_ink_render(&ink, rde_engine_get_time_now(), show_samples);

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
