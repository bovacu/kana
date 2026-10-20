// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/redraw.h"

// ===========================================================================
// See redraw.h: the rule is there, and fude_redraw_end_frame_at is the rule; the check is fude_redraw_check_step and
// fude_redraw_check_taken.
//
// A frame's reports gather between two frame ends: what the update reports, then what the render after it reports
// (a render runs after its frame's end, so what it reports is the next frame's). A timer is kept as an absolute time,
// on the engine's clock when it is asked (a render's ask is not made late by the frame it waits for), until it is due,
// so a timer asked once (a hold's end) is not lost on the frames between.
// ===========================================================================

// The module's state (one per process). Times are on the clock fude_redraw_end_frame_at is given.
RDE_INTERNAL struct {
    b8        continuous;     // rule 1: every frame drawn
    b8        changed;        // a change reported since the last frame's end
    f64       due_at;         // the soonest timer still to come (< 0: none)
    u32       quiet_frames;   // frames ended since the last one with a change (0: the last one changed)
    f64       changed_at;     // when the last change was seen
    rde_mutex lock;           // guards requested, which other threads set (fude_redraw_request)
    b8        requested;      // a fude_redraw_request since the last frame's end
} fude_redraw = { .due_at = -1.0, .quiet_frames = UINT32_MAX, .changed_at = -1.0e9 };

// The check's state (redraw.h: THE CHECK). The picture is the one kept: what the screen shows, as last taken.
RDE_INTERNAL struct {
    b8                 on;
    b8                 reference_due;   // a drawn frame put up a new picture: the next undrawn frame is the reference
    f64                probe_at;        // when the next probe is drawn (< 0: none planned)
    FUDE_REDRAW_CHECK_ taking;          // what the picture asked for is for
    b8                 awaiting;        // ...and it is not delivered yet (no frame drawn since: a minimised window)
    const c8*          shown;           // ...what showed then (a name kept by the shell: a literal)
    f64                asked_at;        // ...and when
    rde_arr TYPE(u8)   picture;         // the picture kept, 4 bytes a pixel, on the std allocator
    u32                w, h;            // its size in pixels
} fude_redraw_check = { .on = FUDE_REDRAW_CHECK, .reference_due = true, .probe_at = -1.0 };

// Makes the mutex once (a second start, as the suite does, keeps it), and counts the start as a change.
void fude_redraw_start(void) {
    if(fude_redraw.lock == NULL) {
        fude_redraw.lock = rde_mutex_create();
    }
    fude_redraw.changed = true;   // the first frames show the app as it starts
}

// Sets rule 1.
void fude_redraw_set_continuous(b8 _continuous) {
    fude_redraw.continuous = _continuous;
}

// Rule 1, as set.
b8 fude_redraw_continuous(void) {
    return fude_redraw.continuous;
}

// Turns the check on or off; a picture kept is kept (the next probe compares with it only if it is still the screen's).
void fude_redraw_set_check(b8 _on) {
    fude_redraw_check.on            = _on;
    fude_redraw_check.reference_due = true;
    fude_redraw_check.probe_at      = -1.0;
}

// Notes a change for the frame now ending (or the next, from a render).
void fude_redraw_changed(void) {
    fude_redraw.changed = true;
}

// Keeps _at (on the clock fude_redraw_end_frame_at is given) when it is sooner than the timer kept.
RDE_INTERNAL void fude_redraw_due(f64 _at) {
    if(fude_redraw.due_at < 0.0 || _at < fude_redraw.due_at) {
        fude_redraw.due_at = _at;
    }
}

// Keeps the soonest time asked for, from now on the engine's clock; a time already past is the next frame.
void fude_redraw_after(f64 _seconds) {
    fude_redraw_due(rde_engine_get_time_now() + (_seconds > 0.0 ? _seconds : 0.0));
}

// Sets the request under the mutex (the main thread reads it at the frame's end), then wakes RDE, which is
// thread-safe. Before fude_redraw_start there is no mutex and no other thread yet, so the flag is set alone.
void fude_redraw_request(void) {
    if(fude_redraw.lock != NULL) {
        rde_mutex_lock(fude_redraw.lock);
    }
    fude_redraw.requested = true;
    if(fude_redraw.lock != NULL) {
        rde_mutex_unlock(fude_redraw.lock);
    }
    rde_engine_request_frame();
}

// When the rule last saw a change.
f64 fude_redraw_last_change(void) {
    return fude_redraw.changed_at;
}

// The rule (redraw.h): gathers the frame's reports into a change and a due timer, decides, and when the frame is
// unchanged works out how long it may sleep: until the timer, or with none until something happens.
fude_redraw_verdict fude_redraw_end_frame_at(f64 _now, f32 _ui_wake) {
    b8 _requested = false;
    if(fude_redraw.lock != NULL) {
        rde_mutex_lock(fude_redraw.lock);
    }
    _requested             = fude_redraw.requested;
    fude_redraw.requested  = false;
    if(fude_redraw.lock != NULL) {
        rde_mutex_unlock(fude_redraw.lock);
    }

    // RDE's UI: 0 moved this frame, above 0 a timer (a caret's blink), below 0 nothing.
    const b8 _changed = fude_redraw.changed || _requested || _ui_wake == 0.0f;
    if(_ui_wake > 0.0f) {
        fude_redraw_due(_now + (f64)_ui_wake);
    }
    const b8 _due = fude_redraw.due_at >= 0.0 && _now >= fude_redraw.due_at - FUDE_REDRAW_EARLY;
    if(_due) {
        fude_redraw.due_at = -1.0;   // its one frame is this one; a source still waiting asks again
    }

    if(_changed) {
        fude_redraw.quiet_frames = 0u;
        fude_redraw.changed_at   = _now;
    } else if(fude_redraw.quiet_frames < UINT32_MAX) {
        fude_redraw.quiet_frames++;
    }
    const b8 _grace = fude_redraw.quiet_frames < FUDE_REDRAW_GRACE_FRAMES || _now - fude_redraw.changed_at < FUDE_REDRAW_GRACE_SECONDS;

    fude_redraw_verdict _v = { fude_redraw.continuous || _grace || _due, -1.0 };
    if(!_v.draw && fude_redraw.due_at >= 0.0) {
        _v.sleep = fude_redraw.due_at - _now;
    }

    // The next frame's reports start afresh (a timer is kept until it is due).
    fude_redraw.changed = false;
    return _v;
}

// The check's plan (redraw.h). A probe is planned a period after the reference, and again a period after each probe,
// for as long as the screen stays still.
FUDE_REDRAW_CHECK_ fude_redraw_check_step(b8 _draw, f64 _now) {
    if(!fude_redraw_check.on || fude_redraw.continuous) {
        return FUDE_REDRAW_CHECK_NONE;
    }
    if(_draw) {
        fude_redraw_check.reference_due = true;
        fude_redraw_check.probe_at      = -1.0;
        return FUDE_REDRAW_CHECK_NONE;
    }
    if(fude_redraw_check.awaiting) {
        return FUDE_REDRAW_CHECK_NONE;   // (one picture at a time: RDE takes one petition)
    }
    if(fude_redraw_check.reference_due) {
        fude_redraw_check.reference_due = false;
        fude_redraw_check.probe_at      = _now + FUDE_REDRAW_CHECK_SECONDS;
        return FUDE_REDRAW_CHECK_REFERENCE;
    }
    if(fude_redraw_check.probe_at >= 0.0 && _now >= fude_redraw_check.probe_at - FUDE_REDRAW_EARLY) {
        fude_redraw_check.probe_at = _now + FUDE_REDRAW_CHECK_SECONDS;
        return FUDE_REDRAW_CHECK_PROBE;
    }
    return FUDE_REDRAW_CHECK_NONE;
}

// The time to the planned probe.
f64 fude_redraw_check_wake(f64 _now) {
    if(!fude_redraw_check.on || fude_redraw.continuous || fude_redraw_check.probe_at < 0.0) {
        return -1.0;
    }
    return fude_redraw_check.probe_at > _now ? fude_redraw_check.probe_at - _now : 0.0;
}

// Counts the pixels that differ and boxes them. A row the same whole is passed at once; most of a still screen is.
fude_redraw_difference fude_redraw_compare(const u8* _a, const u8* _b, u32 _w, u32 _h) {
    fude_redraw_difference _d = { 0u, 0u, 0u, 0u, 0u };
    u32 _x0 = UINT32_MAX, _y0 = UINT32_MAX, _x1 = 0u, _y1 = 0u;
    const usize _row = (usize)_w * 4u;
    for(u32 _y = 0; _y < _h; _y++) {
        const u8* _ra = _a + (usize)_y * _row;
        const u8* _rb = _b + (usize)_y * _row;
        if(rde_memcmp(_ra, _rb, _row) == 0) {
            continue;
        }
        for(u32 _x = 0; _x < _w; _x++) {
            if(rde_memcmp(_ra + (usize)_x * 4u, _rb + (usize)_x * 4u, 4u) != 0) {
                _d.pixels++;
                _x0 = _x < _x0 ? _x : _x0;
                _x1 = _x > _x1 ? _x : _x1;
                _y0 = _y < _y0 ? _y : _y0;
                _y1 = _y;
            }
        }
    }
    if(_d.pixels > 0u) {
        _d.x = _x0;
        _d.y = _y0;
        _d.w = _x1 - _x0 + 1u;
        _d.h = _y1 - _y0 + 1u;
    }
    return _d;
}

// Writes the box _d of picture _pic (_w pixels wide) as a PNG at _path, in the working folder (a developer's build
// runs from its own), so the warning's two pictures can be looked at. A file that cannot be written is logged.
RDE_INTERNAL void fude_redraw_check_write(const u8* _pic, u32 _w, fude_redraw_difference _d, const c8* _path) {
    rde_memory_allocator* _std  = rde_memory_allocator_get_default_std();
    rde_arr TYPE(u8)      _crop = rde_arr_new(sizeof(u8), _std);
    rde_arr_resize(&_crop, (usize)_d.w * _d.h * 4u);
    for(u32 _y = 0; _y < _d.h; _y++) {
        rde_memcpy((u8*)_crop.memory + (usize)_y * _d.w * 4u, _pic + ((usize)(_d.y + _y) * _w + _d.x) * 4u, (usize)_d.w * 4u);
    }
    usize _size = 0;
    u8*   _png  = rde_image_encode_png((const u8*)_crop.memory, _d.w, _d.h, &_size, _std);
    b8    _ok   = false;
    if(_png != NULL) {
        RDE_TRY({
            rde_file* _f = rde_file_open(_path, RDE_FILE_MODE_WRITE_BYTES);
            if(_f != NULL && !rde_failed()) {
                rde_file_write(_f, (const c8*)_png, _size);
                rde_file_close(_f);
                _ok = true;
            }
        });
        _std->free(_std->allocator, _png);
    }
    if(!_ok) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "redraw check: could not write %s", _path);
    }
    rde_arr_free(&_crop);
}

// The check's picture asked for (redraw.h): what, of what, and when; no other is planned until it is delivered.
void fude_redraw_check_asked(FUDE_REDRAW_CHECK_ _what, const c8* _shown, f64 _now) {
    fude_redraw_check.taking   = _what;
    fude_redraw_check.shown    = _shown != NULL ? _shown : "(unnamed)";
    fude_redraw_check.asked_at = _now;
    fude_redraw_check.awaiting = true;
}

// The picture delivered (redraw.h): a probe's compared with the one kept — when nothing was reported changed since it
// was asked (a probe asked while the window was minimised is answered by the frame drawn after its restore, which may
// show changes reported meanwhile) — and a difference warned of, its box written out as it was and as it is; either
// way the picture is kept, as what the screen shows now (so one unreported change is told once, and one that keeps
// moving every probe). _bytes are RDE's, valid during the call.
fude_redraw_difference fude_redraw_check_delivered(const u8* _bytes, u32 _w, u32 _h) {
    fude_redraw_difference _warned = { 0 };
    fude_redraw_check.awaiting = false;
    const usize _size = (usize)_w * (usize)_h * 4u;
    if(fude_redraw_check.taking == FUDE_REDRAW_CHECK_PROBE && fude_redraw.changed_at <= fude_redraw_check.asked_at &&
       fude_redraw_check.w == _w && fude_redraw_check.h == _h &&
       rde_arr_is_inited(&fude_redraw_check.picture) && rde_arr_length(&fude_redraw_check.picture) == _size) {
        const fude_redraw_difference _d = fude_redraw_compare((const u8*)fude_redraw_check.picture.memory, _bytes, _w, _h);
        if(_d.pixels > 0u) {
            rde_log_level(RDE_LOG_LEVEL_WARNING,
                          "redraw check: the screen changed and nothing reported it (drawing/base/redraw.h): %s, %u pixels in %u x %u at (%u, %u) "
                          "(window pixels, from the top left), %.2f s after the last change reported; the box as it was and as it is: "
                          "redraw_check_before.png, redraw_check_after.png",
                          fude_redraw_check.shown, _d.pixels, _d.w, _d.h, _d.x, _d.y, fude_redraw_check.asked_at - fude_redraw.changed_at);
            fude_redraw_check_write((const u8*)fude_redraw_check.picture.memory, _w, _d, "redraw_check_before.png");
            fude_redraw_check_write(_bytes, _w, _d, "redraw_check_after.png");
            _warned = _d;
        }
    }
    if(!rde_arr_is_inited(&fude_redraw_check.picture)) {
        fude_redraw_check.picture = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    }
    rde_arr_resize(&fude_redraw_check.picture, _size);
    rde_memcpy(fude_redraw_check.picture.memory, _bytes, _size);
    fude_redraw_check.w = _w;
    fude_redraw_check.h = _h;
    return _warned;
}

// RDE's screenshot callback for the check (fude_redraw_frame_end asked for it).
RDE_INTERNAL void fude_redraw_check_taken(u8* _bytes, u32 _w, u32 _h) {
    (void)fude_redraw_check_delivered(_bytes, _w, _h);
}

// Hands the rule's verdict to RDE: render on demand on unless every frame is drawn, and an unchanged frame with how
// long it may sleep (no longer than to the check's next probe). A frame the check takes is drawn, and its picture
// asked for. Continuous never calls rde_engine_frame_unchanged, so RDE runs exactly as it did before.
void fude_redraw_frame_end(rde_window* _window, const c8* _shown) {
    const f64                 _now   = rde_engine_get_time_now();
    const fude_redraw_verdict _v     = fude_redraw_end_frame_at(_now, rde_ui_get_wake_after());
    const FUDE_REDRAW_CHECK_  _check = fude_redraw_check_step(_v.draw, _now);
    rde_engine_set_render_on_demand(!fude_redraw.continuous);
    if(_check != FUDE_REDRAW_CHECK_NONE && _window != NULL) {
        fude_redraw_check_asked(_check, _shown, _now);
        rde_window_take_screenshot_bytes(_window, (rde_vec_2I){ 0, 0 }, (rde_vec_2I){ 0, 0 }, fude_redraw_check_taken, rde_memory_allocator_get_default_std());   // (the whole window)
        return;
    }
    if(!_v.draw) {
        rde_engine_frame_unchanged();
        f64       _sleep = _v.sleep;
        const f64 _probe = fude_redraw_check_wake(_now);
        if(_probe >= 0.0 && (_sleep < 0.0 || _probe < _sleep)) {
            _sleep = _probe;
        }
        if(_sleep >= 0.0) {
            rde_engine_wake_after((f32)_sleep);
        }
    }
}
