// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, its file: the canvas's .zoom file, the instruments kept with it (their file and their undo steps),
// saving and closing, a canvas over a PDF, and a notebook's pages (pages.h), opening a canvas included.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
//
// In order: the canvas's path, the instruments' file (<id>.tools) and their undo steps, saving and closing, a canvas
// over a PDF, and a notebook's pages (<id>.pages), with opening a canvas last. The canvas's own file is zfile.h's;
// this module only says when it is flushed and checkpointed.
// ===========================================================================

// --- the canvas's file ------------------------------------------------------------------------

// See page_parts.h. The path is the notes' <id>.kana with its extension made .zoom.
void fude_zoom_page_path(u32 _canvas, c8* _out, usize _size) {
    c8 _kana[RDE_MAX_PATH];
    fude_notes_canvas_path(_canvas, _kana, sizeof(_kana));
    c8* _dot = rde_strrchr(_kana, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.zoom", _kana);
}

// --- the instruments kept with the canvas -------------------------------------------------------
//
// They lie on the drawing (instrument.h), so they go with it: beside its file, <canvas>.tools, written whenever the
// canvas is saved and they changed, and read when it is opened. A header (kfile.h), a count, then for each one stuck
// to the drawing, bottom first: its kind, turn, size, the point of it held, where that is in the drawing (its frame by
// id, as the camera is kept) and its real size (two numbers; a stencil keeps its piece's id in the second). Last, the
// canvas's topic. One the drawing has no frame for any more is left out.

#define FUDE_ZOOM_PAGE_TOOLS_KIND    0x544F4F4Cu   // 'TOOL'
#define FUDE_ZOOM_PAGE_TOOLS_VERSION 1u

// Writes the canvas's .tools path into _out: its .zoom path with the extension made .tools.
RDE_INTERNAL void fude_zoom_page_tools_path(const fude_zoom_page* _page, c8* _out, usize _size) {
    c8 _canvas[RDE_MAX_PATH];
    fude_zoom_page_path(_page->canvas, _canvas, sizeof(_canvas));
    c8* _dot = rde_strrchr(_canvas, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.tools", _canvas);
}

// See page_parts.h. What would be written is hashed, and written only when the hash differs from what was last
// written; a failed write is tried again at the next save.
void fude_zoom_page_tools_save(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    const fude_zoom_instruments* _ins = &_page->instruments;
    fude_bytes _b = fude_bytes_new(512u);
    fude_put_header(&_b, FUDE_ZOOM_PAGE_TOOLS_VERSION, FUDE_ZOOM_PAGE_TOOLS_KIND);
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        const fude_zoom_instrument* _t = &_ins->tools[_ins->order[_i]];
        _n += _t->shown && _t->stuck && _t->stuck_frame < fude_zoom_scene_frame_count(&_page->scene) ? 1u : 0u;
    }
    fude_put_u32(&_b, _n);
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {   // (bottom first: they pile up again as they were)
        const fude_zoom_instrument* _t = &_ins->tools[_ins->order[_i]];
        if(!_t->shown || !_t->stuck || _t->stuck_frame >= fude_zoom_scene_frame_count(&_page->scene)) {
            continue;
        }
        fude_put_u8(&_b, _t->kind);
        fude_put_f64(&_b, _t->angle);
        fude_put_f64(&_b, _t->size);
        fude_put_u32(&_b, (u32)_t->pin);
        fude_put_u64(&_b, fude_zoom_scene_frame(&_page->scene, _t->stuck_frame)->id);
        fude_put_f64(&_b, _t->stuck_point.x);
        fude_put_f64(&_b, _t->stuck_point.y);
        fude_put_f64(&_b, _t->stuck_mm);
        fude_put_f64(&_b, _t->stuck_mm2);
    }
    fude_put_u8(&_b, _page->topic);   // (its topic: the bar's tools for it — topics in the page's code below)
    // Written when it changed only (each save looks: the app may be killed before it hears it is leaving).
    const u64 _hash = rde_hash_64(_b.memory, fude_bytes_size(&_b));
    if(_hash == _page->tools_kept) {
        rde_arr_free(&_b);
        return;
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_tools_path(_page, _path, sizeof(_path));
    if(fude_bytes_write_and_free(&_b, _path, NULL)) {
        _page->tools_kept = _hash;
    }
}

// Reads the canvas's .tools file, if any, putting each instrument out again, and the canvas's topic. One whose frame
// is gone is put out loose, at its turn only; a stencil whose piece is no longer in My pieces is put away.
RDE_INTERNAL void fude_zoom_page_tools_load(fude_zoom_page* _page) {
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_tools_path(_page, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return;   // (none yet: never left with one out)
    }
    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return;
    }
    fude_reader _r = fude_reader_make(_data, _size);
    if(fude_read_header(&_r, FUDE_ZOOM_PAGE_TOOLS_VERSION, FUDE_ZOOM_PAGE_TOOLS_KIND)) {
        const u32 _n = fude_get_u32(&_r);
        for(u32 _i = 0; _i < _n && _i < FUDE_ZOOM_INSTRUMENT_MOST && _r.ok; _i++) {
            const u8  _kind  = fude_get_u8(&_r);
            const f64 _angle = fude_get_f64(&_r);
            const f64 _sz    = fude_get_f64(&_r);
            const i32 _pin   = (i32)fude_get_u32(&_r);
            const u64 _id    = fude_get_u64(&_r);
            const fude_zoom_v2 _at = { fude_get_f64(&_r), fude_get_f64(&_r) };
            const f64 _mm    = fude_get_f64(&_r);
            const f64 _mm2   = fude_get_f64(&_r);
            const u32 _frame = fude_zoom_scene_find_frame(&_page->scene, _id);
            if(!_r.ok || _kind >= FUDE_ZOOM_INSTRUMENT_COUNT) {
                continue;
            }
            const i32 _slot = fude_zoom_instrument_add(&_page->instruments, (FUDE_ZOOM_INSTRUMENT_)_kind, fude_zoom_page_half(_page));
            if(_slot < 0) {
                continue;   // (read past all the same: the topic is after them)
            }
            fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
            if(_frame == FUDE_ZOOM_NONE || !(_mm > 0.0)) {
                _t->angle = _angle;   // (where it lay is gone from the file — one written before its frame was kept: out where a hand puts it, not lost)
                continue;
            }
            _t->angle       = _angle;
            _t->size        = _sz > 0.0 ? _sz : 1.0;
            _t->pin         = _pin;
            _t->stuck_frame = _frame;
            _t->stuck_point = _at;
            _t->stuck_mm    = _mm;
            _t->stuck_mm2   = _mm2;
            _t->stuck       = true;   // (placed by the next follow: on the drawing where it was, as big)
            if(_kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
                // Its lines from its piece again (gone from My pieces: put away).
                fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
                b8 _found = false;
                for(u32 _p = 0; _p < _lib->count && !_found; _p++) {
                    fude_zoom_stencil _st;
                    f64 _side;
                    if(_lib->list[_p].id == (u32)_mm2 && fude_zoom_page_stencil_fill(&_lib->list[_p], &_st, &_side)) {
                        fude_zoom_page_stencil_set(_page, (u32)_slot, &_st);
                        _found = true;
                    }
                }
                if(!_found) {
                    fude_zoom_instrument_remove(&_page->instruments, (u32)_slot);
                }
            }
        }
        // Its topic (a file from before topics: as it is now).
        if(_r.ok && fude_reader_has(&_r, 1u)) {
            const u8 _topic = fude_get_u8(&_r);
            _page->topic = _topic < FUDE_ZOOM_TOPIC_COUNT ? _topic : _page->topic;
        }
    }
    fude_file_free(_data);
}

// --- the instruments' undo ---------------------------------------------------------------------
//
// The instruments' moves are undone with the drawing's steps but kept apart from them: a step of theirs records the
// state before and after, and the drawing's place in its history then (fude_zoom_page_tools_drawing_at), so undo and
// redo take whichever of the two histories' steps is the later or the next.

#define FUDE_ZOOM_PAGE_TOOLS_STILL 1e-4   // two middles nearer than this on the screen (points) are where they were
#define FUDE_ZOOM_PAGE_TOOLS_SIZE  1e-9   // two real sizes this near (of the larger) are the same size

// Copies the instruments' state (all of them and their order) into _out, with where each shown one's middle lies on
// the drawing, in its stuck frame's units: its pinned point there (stuck_point), then the way from that point to its
// middle as it lies on the screen, scaled from the millimetres a point it was last laid at (look_mpp / look, set by
// fude_zoom_page_instruments_follow along with where it is) to the frame's units now. So the middle does not depend on
// which of its points is pinned, nor on the camera having moved since it was laid. One not stuck: its middle on the
// screen (at).
RDE_INTERNAL void fude_zoom_page_tools_take(const fude_zoom_page* _page, fude_zoom_tools_state* _out) {
    rde_memcpy(_out->tools, _page->instruments.tools, sizeof(_out->tools));
    rde_memcpy(_out->order, _page->instruments.order, sizeof(_out->order));
    const f64 _mpp = fude_zoom_page_mm_per_point(_page);
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        const fude_zoom_instrument* _t = &_page->instruments.tools[_i];
        _out->middle[_i] = _t->at;
        if(!_t->shown || !_t->stuck || _t->stuck_frame >= fude_zoom_scene_frame_count(&_page->scene) || !(_mpp > 0.0)) {
            continue;
        }
        const fude_zoom_v2  _pin  = fude_zoom_page_instrument_pin(_page, _i);
        const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _t->stuck_frame));
        const f64 _laid = _t->look > 0.0 && _t->look_mpp > 0.0 ? _t->look_mpp / _t->look / _mpp : 1.0;   // (its mm a point when laid, over now's)
        const fude_zoom_v2 _way = { (_t->at.x - _pin.x) * _laid, (_t->at.y - _pin.y) * _laid };
        // (a way on the screen into the frame's units: the inverse's turn and scale, without its move)
        _out->middle[_i] = (fude_zoom_v2){ _t->stuck_point.x + _back.a * _way.x - _back.b * _way.y, _t->stuck_point.y + _back.b * _way.x + _back.a * _way.y };
    }
}

// Returns where instrument _t's middle _middle (fude_zoom_page_tools_take's) is on the screen as the camera is now: one
// not stuck (or stuck in a frame no longer there) where it was on the screen.
RDE_INTERNAL fude_zoom_v2 fude_zoom_page_tools_middle_now(const fude_zoom_page* _page, const fude_zoom_instrument* _t, fude_zoom_v2 _middle) {
    if(!_t->stuck || _t->stuck_frame >= fude_zoom_scene_frame_count(&_page->scene)) {
        return _middle;
    }
    return fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _t->stuck_frame), _middle);
}

// Returns whether two real sizes are the same up to rounding (FUDE_ZOOM_PAGE_TOOLS_SIZE of the larger): laid again
// without being changed (a re-pin lays it again), a size is worked out again and may differ in its last bits.
RDE_INTERNAL b8 fude_zoom_page_tools_same_size(f64 _x, f64 _y) {
    return _x == _y || rde_math_abs_f64(_x - _y) <= FUDE_ZOOM_PAGE_TOOLS_SIZE * rde_math_max_f64(rde_math_abs_f64(_x), rde_math_abs_f64(_y));
}

// Returns whether two states lie the same on the drawing: each shown one's kind, serial and turn, whether it is stuck,
// its real sizes, and where its middle lies (each state's middle put on the screen as the camera is now, within
// FUDE_ZOOM_PAGE_TOOLS_STILL). Which of its points is pinned is not compared, nor where that point lies (stuck_point,
// stuck_frame): pinned by another point it has not moved, and that alone is no step of its own.
RDE_INTERNAL b8 fude_zoom_page_tools_same(const fude_zoom_page* _page, const fude_zoom_tools_state* _a, const fude_zoom_tools_state* _b) {
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        const fude_zoom_instrument* _x = &_a->tools[_i];
        const fude_zoom_instrument* _y = &_b->tools[_i];
        if(_x->shown != _y->shown) {
            return false;
        }
        if(!_x->shown) {
            continue;
        }
        // (a stencil's second number is its piece's id, not a size: exactly)
        const b8 _mm2 = _x->kind == FUDE_ZOOM_INSTRUMENT_STENCIL ? _x->stuck_mm2 == _y->stuck_mm2 : fude_zoom_page_tools_same_size(_x->stuck_mm2, _y->stuck_mm2);
        if(_x->kind != _y->kind || _x->serial != _y->serial || _x->angle != _y->angle || _x->stuck != _y->stuck ||
           !fude_zoom_page_tools_same_size(_x->stuck_mm, _y->stuck_mm) || !_mm2) {
            return false;
        }
        const fude_zoom_v2 _mx = fude_zoom_page_tools_middle_now(_page, _x, _a->middle[_i]), _my = fude_zoom_page_tools_middle_now(_page, _y, _b->middle[_i]);
        if(!(rde_math_hypot_f64(_mx.x - _my.x, _mx.y - _my.y) <= FUDE_ZOOM_PAGE_TOOLS_STILL)) {
            return false;
        }
    }
    return true;
}

// Where the drawing's history is: its steps done, counted from its first ever.
RDE_INTERNAL u64 fude_zoom_page_tools_drawing_at(const fude_zoom_page* _page) {
    return _page->scene.history_dropped + _page->scene.action_count;
}

// See page_parts.h: once a frame (and before an undo or redo), the instruments left changed since the last step, with
// no hand on them and nothing drawn along them, make a step of their own. A drawing step since one of theirs was
// undone drops their redo, as the drawing's own redo is dropped; at most FUDE_ZOOM_TOOLS_STEPS are kept, the oldest
// going first.
void fude_zoom_page_tools_history(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    // A drawing step since one of theirs was undone: that one cannot come back (as the drawing's own).
    if(_page->scene.history_pushes != _page->tools_seen) {
        _page->tools_seen = _page->scene.history_pushes;
        rde_arr_resize(&_page->tools_steps, _page->tools_done);
    }
    const fude_zoom_instruments* _ins = &_page->instruments;
    if(_ins->ruled >= 0) {
        _page->tools_absorb = true;   // (drawn along: a compass turning with the pen is no step)
        return;
    }
    if(_ins->held >= 0 || _ins->hands > 0u || _ins->growing || _ins->closing_hand != 0u) {
        return;
    }
    fude_zoom_tools_state _now;
    fude_zoom_page_tools_take(_page, &_now);
    if(!_page->tools_then_set || _page->tools_absorb) {
        _page->tools_then     = _now;
        _page->tools_then_set = true;
        _page->tools_absorb   = false;
        return;
    }
    if(fude_zoom_page_tools_same(_page, &_page->tools_then, &_now)) {
        _page->tools_then = _now;   // (re-pinned only)
        return;
    }
    rde_arr_resize(&_page->tools_steps, _page->tools_done);
    if(_page->tools_done >= FUDE_ZOOM_TOOLS_STEPS) {
        rde_arr_remove(&_page->tools_steps, 0);   // (the oldest goes)
        _page->tools_done--;
    }
    const fude_zoom_tools_step _step = { _page->tools_then, _now, fude_zoom_page_tools_drawing_at(_page) };
    rde_arr_add(&_page->tools_steps, (any)&_step);
    _page->tools_done++;
    _page->tools_then = _now;
}

// Puts the instruments as _state has them, with nothing in hand, and makes that the state the next step starts from.
RDE_INTERNAL void fude_zoom_page_tools_put(fude_zoom_page* _page, const fude_zoom_tools_state* _state) {
    fude_zoom_instruments* _ins = &_page->instruments;
    rde_memcpy(_ins->tools, _state->tools, sizeof(_ins->tools));
    rde_memcpy(_ins->order, _state->order, sizeof(_ins->order));
    _ins->held   = -1;
    _ins->hands  = 0u;
    _ins->ruled  = -1;
    _ins->docked = -1;
    if(_page->anchor_tool >= 0 && !_ins->tools[_page->anchor_tool].shown) {
        _page->anchor_tool = -1;
    }
    _page->tools_then     = *_state;
    _page->tools_then_set = true;
}

// See page_parts.h. A step taken at the drawing's present place counts as the latest.
b8 fude_zoom_page_tools_undo(fude_zoom_page* _page) {
    fude_zoom_page_tools_history(_page);
    if(_page->tools_done == 0u) {
        return false;
    }
    const fude_zoom_tools_step* _st = &((const fude_zoom_tools_step*)_page->tools_steps.memory)[_page->tools_done - 1u];
    if(_st->at < fude_zoom_page_tools_drawing_at(_page)) {
        return false;   // (the drawing's step is later)
    }
    fude_zoom_page_tools_put(_page, &_st->before);
    _page->tools_done--;
    return true;
}

// See page_parts.h.
b8 fude_zoom_page_tools_redo(fude_zoom_page* _page) {
    fude_zoom_page_tools_history(_page);
    if(_page->tools_done >= (u32)rde_arr_length(&_page->tools_steps)) {
        return false;
    }
    const fude_zoom_tools_step* _st = &((const fude_zoom_tools_step*)_page->tools_steps.memory)[_page->tools_done];
    if(_st->at > fude_zoom_page_tools_drawing_at(_page)) {
        return false;   // (a drawing's step comes first)
    }
    fude_zoom_page_tools_put(_page, &_st->after);
    _page->tools_done++;
    return true;
}

// See page_parts.h: the frames the instruments lie in kept by the file (and, leaving, the camera's), since an empty
// one zoomed into is not written, and what names it would find nothing when the canvas opens again.
void fude_zoom_page_keep_frames(fude_zoom_page* _page, b8 _camera) {
    fude_zoom_scene* _s = &_page->scene;
    for(u32 _i = 0; _i < FUDE_ZOOM_INSTRUMENT_MOST; _i++) {
        const fude_zoom_instrument* _t = &_page->instruments.tools[_i];
        if(_t->shown && _t->stuck && _t->stuck_frame < fude_zoom_scene_frame_count(_s)) {
            fude_zoom_scene_keep_frame(_s, _t->stuck_frame);
        }
    }
    if(_camera) {
        fude_zoom_scene_keep_frame(_s, _s->camera.frame);
    }
}

// See page_parts.h. The journal is flushed (zfile.h), the instruments written when they changed and, leaving, a
// checkpoint made when anything was drawn or the camera moved since the last.
b8 fude_zoom_page_save_now(fude_zoom_page* _page, b8 _leaving) {
    if(!_page->open) {
        return true;
    }
    fude_zoom_page_keep_frames(_page, _leaving);   // (on file before what names them)
    b8 _ok = fude_zoom_file_flush(&_page->file, &_page->scene);
    // Leaving: a checkpoint for what was drawn since the last, and for where the camera is now, which only a checkpoint
    // keeps (the app opens where it was left, even after only looking round).
    const fude_zoom_camera* _cam = &_page->scene.camera;
    const b8 _moved = _cam->frame != _page->camera_kept.frame || _cam->at.x != _page->camera_kept.at.x ||
                      _cam->at.y != _page->camera_kept.at.y || _cam->z != _page->camera_kept.z;
    fude_zoom_page_tools_save(_page);   // (the instruments out, with it: when they changed)
    if(_ok && _leaving && _page->file.size > 0 && (_page->file.journal_bytes > 0 || _moved)) {
        _ok = fude_zoom_file_checkpoint(&_page->file, &_page->scene);
        if(_ok) {
            _page->camera_kept = *_cam;
        }
    }
    _page->flushed_at = rde_engine_get_time_now();
    return _ok;
}

// See page_parts.h: a push let go of without keeping it (a pinch after all, the canvas closing), the lines as they
// were, shown again.
void fude_zoom_page_nudge_cancel(fude_zoom_page* _page) {
    if(!_page->nudging) {
        return;
    }
    _page->nudging = false;
    _page->renderer.lifted = &_page->selection.lifted;
    rde_arr_clear(&_page->nudge_items);
    rde_arr_clear(&_page->nudge_points);
    rde_arr_clear(&_page->nudge_radii);
    rde_arr_clear(&_page->nudge_handles);
    if(rde_arr_length(&_page->nudge_lifted) > 0) {
        rde_memset(_page->nudge_lifted.memory, 0, rde_arr_length(&_page->nudge_lifted));
    }
}

// What the page keeps of the closing canvas, let go: each names one of its frames by slot (a flight's ends, the marks,
// the depth's levels, the frames the last draw went into, a tape left showing, a curve's points) or one of its objects
// by index (Places' rows, the line's chip, what the page's menu opened on). The next canvas gives those numbers to
// other things, or has fewer: a flight carried on, or a mark, level or place tapped, would put the camera in a frame
// its scene does not have. Back's list names frames by id, which outlive slots, but ids are a canvas's own too (each
// scene's counter starts again), so Back would fly to whatever has the same id there.
RDE_INTERNAL void fude_zoom_page_forget_views(fude_zoom_page* _page) {
    _page->flight.active    = false;
    _page->back_count       = 0;
    _page->mark_count       = 0;
    _page->marks_for        = (fude_zoom_camera){ FUDE_ZOOM_NONE, { 0.0, 0.0 }, 0.0 };   // (found afresh on the next)
    _page->marks_at         = 0.0;
    _page->crumbs_open      = false;
    _page->crumb_count      = 0;
    _page->places_open      = false;
    _page->place_count      = 0;
    _page->place_here       = -1;
    _page->tape_shown       = false;
    _page->curve_count      = 0;
    _page->line_chip_until  = 0.0;
    _page->line_chip_object = FUDE_ZOOM_NONE;
    _page->context_target   = FUDE_ZOOM_NONE;
    _page->context_held     = false;
    rde_arr_clear(&_page->renderer.visible);   // (the pen is mapped through nothing until the new canvas is drawn)
}

// See page_parts.h. What is under way is finished or let go before the save, so it is saved with the canvas; the words
// and recordings are kept after it, and the scene freed last.
void fude_zoom_page_close(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    fude_zoom_page_play_stop(_page);    // (what played is this canvas's)
    fude_zoom_page_template_leave(_page);   // (a template's: kept as its piece, or its part made again)
    fude_zoom_page_nudge_cancel(_page);
    fude_zoom_page_video_drop(_page);   // (a zoom video under way: let go)
    fude_zoom_page_inline_end(_page);   // (a text being written on the page: saved with it)
    fude_zoom_page_save_now(_page, true);
    fude_zoom_page_words_keep(_page);   // (its words beside it: Find through every canvas)
    fude_zoom_page_audio_close(_page);  // (a recording being made: kept)
    fude_zoom_render_trim(&_page->renderer);
    fude_zoom_page_previews_free(_page);
    fude_zoom_page_present_end(_page);
    fude_zoom_page_map_free(_page);
    _page->map_open         = false;
    _page->map_contents_rev = 0xFFFFFFFFu;
    _page->junctions_valid  = false;    // (the next canvas's revisions start again: its dots found afresh)
    _page->junctions_looked = 0u;       // (...and its objects looked through for a wire from its first)
    _page->junctions_wired  = false;
    rde_arr_clear(&_page->attributes);  // (...and for its attributes)
    _page->attributes_looked = 0u;
    fude_zoom_page_strip_free(_page);   // (its pages' pictures: this canvas's)
    _page->strip_open       = false;
    _page->strip_pick       = FUDE_ZOOM_NONE;
    _page->strip_lifted     = false;
    fude_zoom_page_forget_views(_page);
    fude_zoom_pdfview_close(&_page->pdf);
    fude_zoom_scene_destroy(&_page->scene);
    _page->open = false;
}

// --- a canvas over a PDF (pdfview.h) --------------------------------------------------------------

// A canvas over a PDF opened new: the first page's width across the screen (94% of it), its top at the top.
RDE_INTERNAL void fude_zoom_page_pdf_first_view(fude_zoom_page* _page) {
    fude_zoom_scene*    _s     = &_page->scene;
    const fude_zoom_v2  _half  = fude_zoom_page_half(_page);
    const fude_zoom_box _first = fude_zoom_pdfview_pages(&_page->pdf)[0].box;
    const f64 _z = _half.x * 2.0 * 0.94 / rde_math_max_f64(_first.max_x - _first.min_x, 1e-9);   // screen points to a home unit
    fude_zoom_camera_look_at(_s, _s->home, (fude_zoom_v2){ 0.0, _first.max_y - (_half.y - 16.0) / _z }, _z);
    fude_zoom_camera_settle(_s, _half);
}

// See page_parts.h. The canvas is as big as its pages (_b, home units: a PDF's, a notebook's): the view is kept over
// them, never past their edges (what is narrower or shorter than the screen, in its middle), and never further out
// than all of them fit (a PDF's) or, _least over 0, than their width is _least of the screen's (a notebook's: out over
// as many pages as that shows; one fits the screen as it opens). Two passes: the zoom first, then the pan, each
// settling the camera.
void fude_zoom_page_keep_view_over(fude_zoom_page* _page, fude_zoom_box _b, f64 _least) {
    fude_zoom_scene*    _s    = &_page->scene;
    const fude_zoom_v2  _half = fude_zoom_page_half(_page);
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        const fude_zoom_sim _home = fude_zoom_page_frame_sim(_page, _s->home);
        const fude_zoom_v2  _a    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _b.min_x, _b.min_y });
        const fude_zoom_v2  _c    = fude_zoom_sim_apply(_home, (fude_zoom_v2){ _b.max_x, _b.max_y });
        const f64 _x0 = rde_math_min_f64(_a.x, _c.x), _x1 = rde_math_max_f64(_a.x, _c.x), _y0 = rde_math_min_f64(_a.y, _c.y), _y1 = rde_math_max_f64(_a.y, _c.y);
        const f64 _w = _x1 - _x0, _h = _y1 - _y0;
        if(_pass == 0u) {
            // Further out than that: in again, round the middle of the screen.
            if(_least > 0.0) {
                if(_w < _half.x * 2.0 * _least - 0.5) {
                    fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, _half.x * 2.0 * _least / rde_math_max_f64(_w, 1e-9));
                    fude_zoom_camera_settle(_s, _half);
                }
            } else if(_w < _half.x * 2.0 - 0.5 && _h < _half.y * 2.0 - 0.5) {
                fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, rde_math_min_f64(_half.x * 2.0 / rde_math_max_f64(_w, 1e-9), _half.y * 2.0 / rde_math_max_f64(_h, 1e-9)));
                fude_zoom_camera_settle(_s, _half);
            }
            continue;
        }
        f64 _dx = 0.0, _dy = 0.0;
        if(_w <= _half.x * 2.0)    { _dx = -(_x0 + _x1) * 0.5; }
        else if(_x0 > -_half.x)    { _dx = -_half.x - _x0; }
        else if(_x1 < _half.x)     { _dx = _half.x - _x1; }
        if(_h <= _half.y * 2.0)    { _dy = -(_y0 + _y1) * 0.5; }
        else if(_y0 > -_half.y)    { _dy = -_half.y - _y0; }
        else if(_y1 < _half.y)     { _dy = _half.y - _y1; }
        if(rde_math_abs_f64(_dx) > 1e-6 || rde_math_abs_f64(_dy) > 1e-6) {
            fude_zoom_camera_pan(_s, (fude_zoom_v2){ _dx, _dy });
            fude_zoom_camera_settle(_s, _half);
        }
        // A flick against an edge: stopped that way.
        _page->pan_velocity.x = rde_math_abs_f64(_dx) > 0.5 ? 0.0 : _page->pan_velocity.x;
        _page->pan_velocity.y = rde_math_abs_f64(_dy) > 0.5 ? 0.0 : _page->pan_velocity.y;
    }
}

// --- a notebook's pages (pages.h) ------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_PAGES_LEAST 0.2   // a notebook zoomed out until its pages are this much of the screen's width

// Writes the canvas's .pages path into _out: its .zoom path with the extension made .pages.
RDE_INTERNAL void fude_zoom_page_pages_path(const fude_zoom_page* _page, c8* _out, usize _size) {
    c8 _canvas[RDE_MAX_PATH];
    fude_zoom_page_path(_page->canvas, _canvas, sizeof(_canvas));
    c8* _dot = rde_strrchr(_canvas, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.pages", _canvas);
}

// See page_parts.h.
void fude_zoom_page_pages_save(fude_zoom_page* _page) {
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_pages_path(_page, _path, sizeof(_path));
    fude_zoom_pages_save(&_page->pages, _path);
}

// A step's tag (scene.h' fude_zoom_action.tag) saying a notebook's pages went up (or down) by _delta: undone, they go
// back; redone, again. 0: no change (no tag).
#define FUDE_ZOOM_PAGE_TAG_PAGES 0x50470000u   // 'PG' in its top half, the change in its bottom half (i16)

// See page_parts.h.
u32 fude_zoom_page_pages_tag(i32 _delta) {
    return _delta == 0 ? 0u : FUDE_ZOOM_PAGE_TAG_PAGES | (u32)(u16)(i16)_delta;
}

// See page_parts.h: a step undone (_undo) or redone, its pages' change, if it had one, undone or done again, and
// kept. The count stays between 1 and FUDE_ZOOM_PAGES_MOST, and never below what is written on needs.
void fude_zoom_page_pages_step(fude_zoom_page* _page, u32 _tag, b8 _undo) {
    if((_tag & 0xFFFF0000u) != FUDE_ZOOM_PAGE_TAG_PAGES || !_page->pages.on) {
        return;
    }
    const i32 _delta = (i32)(i16)(u16)(_tag & 0xFFFFu);
    const i64 _count = (i64)_page->pages.count + (_undo ? -(i64)_delta : (i64)_delta);
    _page->pages.count = _count < 1 ? 1u : _count > (i64)FUDE_ZOOM_PAGES_MOST ? FUDE_ZOOM_PAGES_MOST : (u32)_count;
    fude_zoom_pages_grow(&_page->pages, &_page->scene);   // (never fewer than what is written on needs)
    fude_zoom_page_pages_save(_page);
    _page->pages_rev = 0xFFFFFFFFu;
}

// See page_parts.h: a notebook's pages rearranged (pages.h: a blank one put in before _i, _i taken out, _i made _j),
// what is on them with them, one undo step tagged with the change in the page count; kept. False: nothing done.
b8 fude_zoom_page_pages_do(fude_zoom_page* _page, u8 _do, u32 _i, u32 _j) {
    fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(u32)             _died   = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u32)             _moved  = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(fude_zoom_place) _before = rde_arr_new(sizeof(fude_zoom_place), _heap);
    rde_arr TYPE(fude_zoom_place) _after  = rde_arr_new(sizeof(fude_zoom_place), _heap);
    const u32 _was  = _page->pages.count;
    const b8  _done = fude_zoom_pages_rearrange(&_page->pages, _s, _do, _i, _j, &_died, &_moved, &_before, &_after);
    if(_done) {
        // (one step: what moved, what was let go, and the pages added or taken out — undone, all of it)
        fude_zoom_history_push_tagged(_s, _s->home, fude_zoom_pages_bounds(&_page->pages), (const u32*)_died.memory, (u32)rde_arr_length(&_died), NULL, 0u,
                                      (const u32*)_moved.memory, (const fude_zoom_place*)_before.memory, (const fude_zoom_place*)_after.memory,
                                      (u32)rde_arr_length(&_moved), fude_zoom_page_pages_tag((i32)_page->pages.count - (i32)_was));
    }
    rde_arr_free(&_died);
    rde_arr_free(&_moved);
    rde_arr_free(&_before);
    rde_arr_free(&_after);
    if(_done) {
        fude_zoom_pages_grow(&_page->pages, _s);   // (one blank one past the last written on, still)
        fude_zoom_page_pages_save(_page);
    }
    return _done;
}

// See page_parts.h. A canvas over a PDF is not one, even with pages of its own.
b8 fude_zoom_page_paged(const fude_zoom_page* _page) {
    return _page->pages.on && !fude_zoom_pdfview_is_open(&_page->pdf);
}

// See page_parts.h. The page's width is 94% of the screen's, its top 16 points below the screen's.
void fude_zoom_page_pages_first_view(fude_zoom_page* _page) {
    fude_zoom_scene*    _s     = &_page->scene;
    const fude_zoom_v2  _half  = fude_zoom_page_half(_page);
    const fude_zoom_box _first = fude_zoom_pages_box(&_page->pages, 0u);
    const f64 _z = _half.x * 2.0 * 0.94 / rde_math_max_f64(_first.max_x - _first.min_x, 1e-9);
    fude_zoom_camera_look_at(_s, _s->home, (fude_zoom_v2){ 0.0, _first.max_y - (_half.y - 16.0) / _z }, _z);
    fude_zoom_camera_settle(_s, _half);
}

// See page_parts.h: a notebook's view kept over its pages (as a PDF's), and, once the drawing changed, as many pages as
// it needs: one past the last written on.
void fude_zoom_page_pages_follow(fude_zoom_page* _page) {
    if(!fude_zoom_page_paged(_page)) {
        return;
    }
    if(_page->scene.revision != _page->pages_rev) {
        _page->pages_rev = _page->scene.revision;
        if(fude_zoom_pages_grow(&_page->pages, &_page->scene)) {
            fude_zoom_page_pages_save(_page);
        }
    }
    fude_zoom_page_keep_view_over(_page, fude_zoom_pages_bounds(&_page->pages), FUDE_ZOOM_PAGE_PAGES_LEAST);
}

// See page_parts.h. The canvas open before is closed (and saved) first. A new canvas gets its first view from what it
// is: a notebook's first page, or its PDF's.
void fude_zoom_page_open(fude_zoom_page* _page, u32 _canvas) {
    fude_zoom_page_close(_page);
    fude_zoom_page_hand_forget(_page);   // (another canvas's handwriting)
    fude_zoom_scene_init(&_page->scene, _page->device);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_path(_canvas, _path, sizeof(_path));
    const f64 _began = rde_engine_get_time_now();   // (how long the reading took, logged: a big canvas's cost to open)
    const FUDE_LOAD_ _r = fude_zoom_file_open(&_page->file, _path, &_page->scene);
    const f64 _took = rde_engine_get_time_now() - _began;
    u64 _points = 0;
    const u32 _strokes = fude_zoom_scene_alive_strokes(&_page->scene, &_points);
    rde_log_color(RDE_LOG_COLOR_GREEN, "canvas: %s (%s in %.1f ms): %u strokes, %llu points, %u frames", _path,
                  _r == FUDE_LOAD_OK ? "loaded" : _r == FUDE_LOAD_MISSING ? "new" : _r == FUDE_LOAD_RECOVERED ? "RECOVERED from the backup" : "DAMAGED, kept as .bad",
                  1000.0 * _took, _strokes, (unsigned long long)_points, fude_zoom_scene_frame_count(&_page->scene));
    fude_zoom_select_clear(&_page->selection);
    _page->combine_key     = 0u;      // (the lasso's row's kept answers were the last canvas's: worked out again)
    _page->combine_offered = false;
    _page->play_what_key   = 0u;
    _page->play_what_parts = 0u;
    _page->play_what_was   = 0u;
    _page->camera_kept = _page->scene.camera;   // (what the file has)
    _page->canvas     = _canvas;
    _page->open       = true;
    fude_zoom_instruments_init(&_page->instruments);
    _page->anchor_tool = -1;
    _page->tools_kept = 0;
    fude_zoom_page_tools_load(_page);           // the instruments it was left with
    rde_arr_clear(&_page->tools_steps);         // (their undo: this canvas's, from now)
    _page->tools_done     = 0;
    _page->tools_then_set = false;
    _page->tools_absorb   = false;
    _page->tools_seen     = _page->scene.history_pushes;
    // A notebook (pages.h: its pages beside it); new to its pages, its first one in view.
    c8 _pages[RDE_MAX_PATH];
    fude_zoom_page_pages_path(_page, _pages, sizeof(_pages));
    if(fude_zoom_pages_load(&_page->pages, _pages) && _r == FUDE_LOAD_MISSING) {
        fude_zoom_page_pages_first_view(_page);
    }
    if(_page->notebook_next != 0u && _r == FUDE_LOAD_MISSING && !_page->pages.on) {
        // (made as a notebook: the home screen's New notebook)
        fude_zoom_pages_size(&_page->pages, (u8)(_page->notebook_next - 1u));
        _page->pages.count = 1u;
        fude_zoom_page_pages_save(_page);
        fude_zoom_page_pages_first_view(_page);
    }
    _page->notebook_next = 0u;
    _page->pages_rev = 0xFFFFFFFFu;
    // A canvas over a PDF (its own, notes.h): its pages under the ink; new, the first one's width across the screen,
    // its top at the top.
    const fude_note* _note = fude_notes_find(_page->app->notes, _canvas);
    if(_note != NULL && _note->document == FUDE_NOTE_DOCUMENT_OWN) {
        c8 _pdf[RDE_MAX_PATH];
        fude_notes_document_path(_canvas, _pdf, sizeof(_pdf));
        if(fude_zoom_pdfview_open(&_page->pdf, _pdf) && _r == FUDE_LOAD_MISSING) {
            fude_zoom_page_pdf_first_view(_page);
        }
    }
    _page->flushed_at = rde_engine_get_time_now();
    _page->drawing = _page->erasing = _page->finger_drawing = false;
    fude_zoom_page_audio_open(_page);   // (its recordings)
    fude_zoom_page_template_opened(_page, true);   // (a piece's or a custom part's template: said, kept when left)
}
