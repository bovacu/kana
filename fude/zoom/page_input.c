// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, the hands: a press, a move and a lift handed to the tool they are for, fingers (pinch, pan, taps), a
// flick carrying the view on, and the events (pen, touch, mouse buttons) turned into them. The keys, the mouse's moves
// and its wheel are read once a frame instead (page_frame.c).
//
// One press is followed at a time (the pen's, the mouse's, or a finger's when fingers write): fude_zoom_page_down hands
// it to the first thing that takes it, in order (the panels, a text being written, Play, the marks, the instruments,
// then the tool in hand), and fude_zoom_page_moved and fude_zoom_page_up hand the rest to whatever took it. The
// panels are told which hand it is: all ones for the press, a finger's id plus one for a finger (0 is none). Other
// fingers move the view (fude_zoom_page_finger_*).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

#define FUDE_ZOOM_PAGE_TAP_TIME   0.35    // a multi-finger tap: every finger up within this long (seconds)...
#define FUDE_ZOOM_PAGE_TAP_SLOP   16.0f   // ...none moved further than this (screen units)
#define FUDE_ZOOM_PAGE_TWO_FINGER 0.25    // a second finger this soon after a finger began drawing: it was a gesture
#define FUDE_ZOOM_PAGE_HOVER_QUIET 0.35   // seconds the brush's circle waits after the pen lifts

// Whatever tool is in hand, a press on what the lasso holds (inside its box, on a corner, its knob, a side's handle,
// a path's node) moves, turns, scales or reshapes it, as the lasso would, so there is no going back to the lasso for
// it. The pen with the wire tool on a pin still draws a wire; the eraser rubs out inside it, but its handles are still
// its. A press anywhere else lets it go (the tool does its own there). Returns true when it was the selection's.
RDE_INTERNAL b8 fude_zoom_page_held_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _eraser_end) {
    if(!fude_zoom_select_any(&_page->selection) || fude_zoom_select_busy(&_page->selection)) {
        return false;
    }
    const b8 _erasing = _eraser_end || _page->app->ui->bar.tool == FUDE_TOOL_ERASE;
    if(_page->wire_tool && !_erasing) {
        u32 _o, _pin;
        fude_zoom_v2 _at;
        if(fude_zoom_page_pin_at(_page, _screen, FUDE_ZOOM_PAGE_PIN_REACH, &_o, &_pin, &_at)) {
            return false;   // A wire from this pin.
        }
    }
    const i32 _h = fude_zoom_select_handle_at(&_page->selection, _screen);
    _page->lasso_down_at      = _screen;
    _page->lasso_went         = false;
    _page->lasso_on_selection = _h == 5;
    if(fude_zoom_page_stretch_down(_page, _screen) || (!_erasing && fude_zoom_page_node_down(_page, _screen))) {
        return true;
    }
    if(_h < 0 || (_h == 5 && _erasing)) {
        if(_h < 0) {
            fude_zoom_select_clear(&_page->selection);   // A press off it: let go.
        }
        return false;
    }
    fude_zoom_select_down(&_page->selection, &_page->scene, _screen);
    fude_zoom_page_carry(_page);   // An area moved: what is in it with it.
    return true;
}

// See page_parts.h. The press goes to the first that takes it, in this order: presenting; the diagram library's and
// Play's panels; a text being written; the map's, my pieces', the strip's, Find everywhere's and audio's panels; Play
// (only the parts worked by hand); the marks; a guide pulled from a sheet's ruler; the compass's label; the
// instruments; the Fill, Curve and text tools; what the lasso holds, whatever the tool; then the tool in hand.
// _eraser_end is the pen's other end, which rubs out whatever tool is in hand.
void fude_zoom_page_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser_end) {
    _page->pdf_text_on = false;   // A new press: the lasso's text let go.
    if(!_page->open) {
        return;
    }
    if(_page->presenting) {
        fude_zoom_page_present_pen_down(_page, _screen);   // A tap goes on or back; a drag, the laser.
        return;
    }
    if(fude_zoom_page_library_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_play_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull)) {
        return;   // The diagram library's panel, Play's: their own.
    }
    if(fude_zoom_page_inline_down(_page, _screen)) {
        return;   // A text being written: the caret moved, or it is saved.
    }
    if(fude_zoom_page_map_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_pieces_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull) ||
       fude_zoom_page_strip_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_everywhere_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_audio_down(_page, _screen, 0xFFFFFFFFFFFFFFFFull)) {
        return;   // The map's panel, my pieces', the strip's, Find's, audio's: their own.
    }
    if(_page->play_on) {
        // Playing: nothing is edited (no drawing, no lasso, nothing moved, no menu); a part worked by hand (a switch, a
        // button, a logic input, a board's pin) is, whatever the tool.
        _page->flight.active = false;
        fude_pagemenu_close_context(&_page->app->ui->page);
        b8 _worked = false;
        if(_page->play_circuit && !_eraser_end) {
            _worked = fude_zoom_page_circuit_press(_page, _screen);
        }
        if(!_worked && !_eraser_end && fude_zoom_page_mech_hold(_page, _screen)) {
            _page->mech_finger = 0u;   // The pen's.
        }
        return;
    }
    _page->flight.active = false;   // The hand takes over where it is.
    _page->crumbs_open   = false;
    _page->places_open   = false;
    fude_pagemenu_close_context(&_page->app->ui->page);   // A press anywhere else dismisses it, and still does its job.
    if(fude_zoom_page_mark_press(_page, _screen)) {
        return;
    }
    if(!_eraser_end && _page->app->ui->bar.tool != FUDE_TOOL_ERASE && fude_zoom_page_guide_pull_down(_page, _screen)) {
        return;   // A guide out of a sheet's ruler.
    }
    // The instruments: the pen on one's body or knob moves or turns it; near an edge, a line drawn with the pen (or the
    // marker) goes along it.
    _page->ruled = false;
    if(!_eraser_end && fude_zoom_page_compass_label_down(_page, _screen)) {
        return;   // The compass's radius, to type.
    }
    if(fude_zoom_instruments_any(&_page->instruments) && !_eraser_end) {
        fude_zoom_v2 _on = { _screen.x, _screen.y };
        const FUDE_ZOOM_INSTRUMENT_PEN_ _what = fude_zoom_instruments_pen_down(&_page->instruments, _on, &_on);
        if(_what == FUDE_ZOOM_INSTRUMENT_PEN_HELD) {
            _page->instrument_pen = true;
            return;
        }
        const FUDE_TOOL_ _tool = _page->app->ui->bar.tool;
        if(_what == FUDE_ZOOM_INSTRUMENT_PEN_RULED) {
            if(!_page->fill_tool && _page->shape_tool == 0 && _page->measure_tool == 0 && (_tool == FUDE_TOOL_DRAW || _tool == FUDE_TOOL_MARK)) {
                f64          _along;
                b8           _arc = false;
                fude_zoom_v2 _label;
                fude_zoom_instruments_ruled(&_page->instruments, &_along, &_arc, &_label);
                _page->ruled          = true;
                // A straight edge's line is kept straight (an arc's, a curve's keeps every point); an arc's is kept
                // exact.
                _page->ruled_straight = fude_zoom_instruments_ruled_straight(&_page->instruments) && _tool == FUDE_TOOL_DRAW;
                _page->ruled_round    = fude_zoom_instruments_ruled_round(&_page->instruments) && _tool == FUDE_TOOL_DRAW;
                fude_zoom_page_pin_ruled(_page);   // That edge held on the drawing as it zooms.
                _screen               = fude_zoom_page_ruled_beside(_page, _on);
            } else {
                fude_zoom_instruments_pen_up(&_page->instruments);
            }
        }
    }
    if(_page->fill_tool && !_eraser_end) {
        fude_zoom_page_fill_at(_page, _screen);
        return;
    }
    _page->tape_shown = false;
    if(_page->curve_tool && !_eraser_end && _page->app->ui->bar.tool != FUDE_TOOL_ERASE) {
        fude_zoom_page_curve_tap(_page, _screen);
        return;
    }
    if(_page->text_tool != 0 && !_eraser_end && _page->app->ui->bar.tool != FUDE_TOOL_ERASE) {
        // A held note's side: stretched, not opened; a task's box: ticked; a link: opened. Anywhere else, a text there.
        if(!fude_zoom_page_stretch_down(_page, _screen) && !fude_zoom_page_rich_tap(_page, _screen, false)) {
            fude_zoom_page_text_at(_page, _screen);
        }
        return;
    }
    if(_page->app->ui->bar.tool != FUDE_TOOL_LASSO && fude_zoom_page_held_down(_page, _screen, _eraser_end)) {
        return;   // What the lasso holds, whatever the tool: moved, turned, scaled.
    }
    if(_page->wire_tool && !_eraser_end && _page->app->ui->bar.tool == FUDE_TOOL_DRAW) {
        fude_zoom_page_wire_down(_page, _screen);
        return;
    }
    if(_page->room_tool && !_eraser_end && _page->app->ui->bar.tool == FUDE_TOOL_DRAW) {
        fude_zoom_page_room_at(_page, _screen);   // A room's name and area where it is tapped.
        return;
    }
    if(_page->trim_tool && !_eraser_end && _page->app->ui->bar.tool == FUDE_TOOL_DRAW) {
        fude_zoom_page_trim_at(_page, _screen);   // A line's piece cut away; the pen draws nothing.
        return;
    }
    if(_page->measure_tool != 0 && !_eraser_end && _page->app->ui->bar.tool != FUDE_TOOL_ERASE) {
        fude_zoom_page_measure_down(_page, _screen);
        return;
    }
    if(_page->nudge && !_eraser_end && _page->shape_tool == 0 && _page->app->ui->bar.tool == FUDE_TOOL_DRAW) {
        fude_zoom_page_nudge_down(_page, _screen);   // The pen pushes, not draws.
        return;
    }
    if(_page->app->ui->bar.tool == FUDE_TOOL_ERASE || _eraser_end) {
        fude_zoom_page_erase_down(_page, _screen);
    } else if(_page->shape_tool != 0) {
        fude_zoom_page_shape_down(_page, _screen);
    } else if(_page->app->ui->bar.tool == FUDE_TOOL_LASSO) {
        fude_zoom_box _sb;
        _page->lasso_down_at      = _screen;
        _page->lasso_went         = false;
        _page->lasso_on_selection = fude_zoom_select_box(&_page->selection, &_sb) && _screen.x >= _sb.min_x && _screen.x <= _sb.max_x &&
                                    _screen.y >= _sb.min_y && _screen.y <= _sb.max_y;
        if(!fude_zoom_page_stretch_down(_page, _screen) && !fude_zoom_page_node_down(_page, _screen)) {
            fude_zoom_select_down(&_page->selection, &_page->scene, _screen);
            fude_zoom_page_carry(_page);   // An area moved: what is in it with it.
        }
    } else if(fude_zoom_page_rich_tap(_page, _screen, true)) {
        // A note's task box under the pen: ticked, nothing drawn.
    } else if(fude_zoom_page_link_at(_page, _screen) || fude_zoom_page_audio_linked_at(_page, _screen)) {
        // A note's link under the pen (or ink a recording linked, its panel up): a tap opens it (plays from it); moved
        // away, a stroke after all (begun where it went down).
        _page->link_pending  = true;
        _page->link_audio    = !fude_zoom_page_link_at(_page, _screen);
        _page->link_down     = _screen;
        _page->link_from_pen = _from_pen;
    } else {
        fude_zoom_page_draw_down(_page, _screen, _from_pen);
    }
}

// See page_parts.h: the move goes to whatever took the press. A pending tap on a link becomes a stroke, begun where it
// went down, once it has gone FUDE_ZOOM_PAGE_LINK_SLOP.
void fude_zoom_page_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(_page->presenting) {
        fude_zoom_page_present_pen_moved(_page, _screen);
        return;
    }
    if(fude_zoom_page_inline_moved(_page, _screen)) {
        return;   // A text being written: selected as far as this.
    }
    if(_page->link_pending) {
        // Down on a link: not a tap once it has gone this far, but a stroke from where it went down.
        if(rde_math_abs_f32(_screen.x - _page->link_down.x) + rde_math_abs_f32(_screen.y - _page->link_down.y) < FUDE_ZOOM_PAGE_LINK_SLOP) {
            return;
        }
        _page->link_pending = false;
        fude_zoom_page_draw_down(_page, _page->link_down, _page->link_from_pen);
    }
    if(fude_zoom_page_library_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_play_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) ||
       fude_zoom_page_map_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_strip_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_everywhere_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_audio_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull) ||
       fude_zoom_page_pieces_moved(_page, _screen, 0xFFFFFFFFFFFFFFFFull)) {
        return;
    }
    if(_page->mech_hold != 0u && _page->mech_finger == 0u) {
        fude_zoom_page_mech_hold_moved(_page, _screen);   // A hand on a mechanism playing: the pen's.
        return;
    }
    if(_page->knob_part != FUDE_ZOOM_NONE && _page->play_finger == 0u) {
        fude_zoom_page_knob_moved(_page, _screen);   // An oscilloscope's knob turned: the pen's.
        return;
    }
    if(_page->stretching) {
        fude_zoom_page_stretch_moved(_page, _screen);
        return;
    }
    if(_page->guide_pulling || _page->wiring) {
        _page->pen_now = _screen;
        return;
    }
    if(_page->instrument_pen) {
        fude_zoom_instruments_pen_moved(&_page->instruments, (fude_zoom_v2){ _screen.x, _screen.y });
        fude_zoom_page_instrument_snap(_page, FUDE_ZOOM_SNAP_RADIUS);
        return;
    }
    if(_page->ruled && _page->drawing) {
        const fude_zoom_v2 _on = fude_zoom_instruments_pen_moved(&_page->instruments, (fude_zoom_v2){ _screen.x, _screen.y });
        fude_zoom_page_ruled_corner(_page);   // Turned onto another edge where they cross: its corner first.
        _screen = fude_zoom_page_ruled_beside(_page, _on);
    }
    if(_page->measuring) {
        fude_zoom_page_measure_moved(_page, _screen);
    } else if(_page->erasing) {
        fude_zoom_page_erase_moved(_page, _screen);
    } else if(_page->drawing) {
        fude_zoom_page_draw_moved(_page, _screen);
    } else if(_page->nudging) {
        fude_zoom_page_nudge_moved(_page, _screen);
    } else if(_page->node_grab >= 0) {
        fude_zoom_page_node_moved(_page, _screen);
    } else if(fude_zoom_select_busy(&_page->selection)) {
        _page->node_last_at = _screen;   // A tap on the line, if it does not go anywhere.
        _page->lasso_went   = _page->lasso_went || rde_math_abs_f32(_screen.x - _page->lasso_down_at.x) + rde_math_abs_f32(_screen.y - _page->lasso_down_at.y) > 6.0f;
        fude_zoom_select_moved(&_page->selection, _screen);
        fude_zoom_page_align(_page);   // Electronics, Mechanisms: level and plumb with the others.
    } else if(_page->shaping) {
        // A line's end is reached from its start: square to things, along them (snap.h).
        _page->snap_from_on = _page->shape_tool == FUDE_ZOOM_SHAPE_LINE || _page->shape_tool == FUDE_ZOOM_SHAPE_GUIDE || _page->shape_tool == FUDE_ZOOM_SHAPE_WALL;
        _page->snap_from    = (fude_zoom_v2){ _page->shape_from.x, _page->shape_from.y };
        _page->pen_now = fude_zoom_page_snapped(_page, _screen);
        _page->snap_from_on = false;
    }
}

// See page_parts.h. After the tools in hand are told, the lasso's lift: a guide it held let go on a sheet's ruler is
// deleted; what was carried is let go; notes let go in a Kanban column are lined up; a tap that went nowhere opens a
// note's link or takes the thing under it whole; and a loop round a PDF's text takes that text.
void fude_zoom_page_up(fude_zoom_page* _page) {
    if(_page->presenting) {
        fude_zoom_page_present_pen_up(_page);
        return;
    }
    if(fude_zoom_page_inline_up(_page)) {
        return;
    }
    if(_page->link_pending) {
        _page->link_pending = false;
        if(_page->link_audio) {
            fude_zoom_page_audio_tap(_page, _page->link_down);   // A tap on ink a recording linked: played from it.
        } else {
            fude_zoom_page_rich_tap(_page, _page->link_down, false);   // A tap on a note's link: opened.
        }
        return;
    }
    if(fude_zoom_page_library_up(_page, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_play_up(_page, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_map_up(_page, 0xFFFFFFFFFFFFFFFFull) ||
       fude_zoom_page_strip_up(_page, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_everywhere_up(_page, 0xFFFFFFFFFFFFFFFFull) || fude_zoom_page_audio_up(_page, 0xFFFFFFFFFFFFFFFFull) ||
       fude_zoom_page_pieces_up(_page, 0xFFFFFFFFFFFFFFFFull)) {
        return;
    }
    if(_page->stretching) {
        fude_zoom_page_stretch_up(_page);
        return;
    }
    if(_page->guide_pulling) {
        fude_zoom_page_guide_pull_up(_page);
        return;
    }
    if(_page->wiring) {
        fude_zoom_page_wire_up(_page);
        return;
    }
    fude_zoom_page_circuit_release(_page);
    if(_page->mech_hold != 0u && _page->mech_finger == 0u) {
        fude_zoom_page_mech_release(_page);
    }
    if(_page->instrument_pen || _page->ruled) {
        fude_zoom_instruments_pen_up(&_page->instruments);
        _page->instrument_pen = false;
        _page->snap_hit.kind  = FUDE_ZOOM_SNAP_NONE;
    }
    if(_page->measuring) {
        fude_zoom_page_measure_up(_page);
    }
    fude_zoom_page_erase_up(_page);
    fude_zoom_page_draw_up(_page);
    fude_zoom_page_nudge_up(_page);
    _page->ruled = false;
    fude_zoom_page_shape_up(_page);
    const b8 _insert = _page->node_insert;
    const b8 _lassoing = fude_zoom_select_busy(&_page->selection);
    // A loop over a PDF: the box it went round, for its text.
    fude_zoom_box _loop = fude_zoom_box_empty();
    if(_lassoing && _page->selection.grab == FUDE_ZOOM_GRAB_LOOP && fude_zoom_pdfview_is_open(&_page->pdf)) {
        const fude_zoom_sim _back = fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _page->scene.home));
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.loop); _i++) {
            const rde_vec_2F   _s = ((const rde_vec_2F*)_page->selection.loop.memory)[_i];
            const fude_zoom_v2 _h = fude_zoom_sim_apply(_back, (fude_zoom_v2){ _s.x, _s.y });
            _loop = fude_zoom_box_union(_loop, (fude_zoom_box){ _h.x, _h.y, _h.x, _h.y });
        }
    }
    // A guide the lasso holds let go on a sheet's ruler: deleted.
    const b8 _guide_back = _lassoing && _page->selection.grab == FUDE_ZOOM_GRAB_MOVE && _page->lasso_went && rde_arr_length(&_page->selection.picks) == 1u &&
                           fude_zoom_scene_object(&_page->scene, ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object)->channels == FUDE_ZOOM_SHAPE_GUIDE &&
                           fude_zoom_scene_object(&_page->scene, ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object)->kind == FUDE_ZOOM_KIND_SHAPE &&
                           fude_zoom_page_stuck_at(_page, _page->node_last_at) >= 0;
    if(_lassoing) {
        _page->node_insert = false;   // The lasso's lift first: then the new node, on what it holds.
        fude_zoom_select_up(&_page->selection, &_page->scene, &_page->renderer);
        _page->align_from = (rde_vec_2F){ -1e30f, -1e30f };   // The next drag lines up afresh, wherever it starts.
        _page->align_x = _page->align_y = RDE_NAN_F64;
        rde_arr_clear(&_page->align_wires);
        if(_guide_back) {
            fude_zoom_select_delete(&_page->selection, &_page->scene);
        }
        if(_page->carried_keep > 0u) {
            // What was carried let go: the area held.
            fude_zoom_select_keep(&_page->selection, &_page->scene, _page->carried_keep);
            _page->carried_keep = 0;
        }
        _page->node_insert = _insert;
        if(_page->lasso_on_selection && _page->lasso_went) {
            fude_zoom_page_kanban_tidy(_page);   // Sticky notes let go in a Kanban column: lined up in it.
        }
    }
    if(_lassoing && !_page->lasso_went && !_insert && fude_zoom_page_link_at(_page, _page->lasso_down_at)) {
        fude_zoom_page_rich_tap(_page, _page->lasso_down_at, false);   // A tap on a note's link (held or not): opened.
    } else if(_lassoing && !_page->lasso_went && !_page->lasso_on_selection && !_insert) {
        fude_zoom_page_tap_select(_page, _page->lasso_down_at);   // A tap: the thing under it, whole.
    }
    if(_lassoing && _page->lasso_went && _loop.max_x > _loop.min_x && _loop.max_y > _loop.min_y) {
        c8 _some[64];
        _page->pdf_text_box = _loop;
        _page->pdf_text_on  = fude_zoom_page_pdf_text(_page, _loop, _some, sizeof(_some)) > 0u;
    }
    fude_zoom_page_node_up(_page);
}

// --- fingers ---------------------------------------------------------------------------------

// See page_parts.h: the fingers moving the view, not one writing.
u32 fude_zoom_page_finger_count(const fude_zoom_page* _page) {
    u32 _n = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        _n += _page->fingers[_i].active ? 1u : 0u;
    }
    return _n;
}

// Returns the finger on the screen with id _id, or NULL.
RDE_INTERNAL fude_zoom_finger* fude_zoom_page_finger(fude_zoom_page* _page, u64 _id) {
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        if(_page->fingers[_i].active && _page->fingers[_i].id == _id) {
            return &_page->fingers[_i];
        }
    }
    return NULL;
}

// --- a flick: the view carrying on after the hand lets go (as the ink canvas's does: drawing/ink/canvas.c) ---------

#define FUDE_ZOOM_PAGE_FRICTION  3.5    // a flick's slowing, per second (its speed times e^(-3.5 t))
#define FUDE_ZOOM_PAGE_COAST_MIN 20.0   // screen units a second: slower stops

// See page_parts.h. The speed is what moved since the last look, over the time (at least 4 milliseconds, so two
// fingers' moves in one frame are one move), smoothed: 70% the new, 30% the old. After 0.2 seconds held still it
// starts again from 0.
void fude_zoom_page_pan_speed(fude_zoom_page* _page, fude_zoom_v2 _moved) {
    const f64 _now = rde_engine_get_time_now();
    const f64 _dt  = _now - _page->pan_moved_at;
    _page->pan_pending.x += _moved.x;
    _page->pan_pending.y += _moved.y;
    if(_dt < 0.004) {
        return;
    }
    if(_dt < 0.2) {
        _page->pan_velocity.x = _page->pan_velocity.x * 0.3 + _page->pan_pending.x / _dt * 0.7;
        _page->pan_velocity.y = _page->pan_velocity.y * 0.3 + _page->pan_pending.y / _dt * 0.7;
    } else {
        _page->pan_velocity = (fude_zoom_v2){ 0.0, 0.0 };   // Held still a while: from now.
    }
    _page->pan_pending  = (fude_zoom_v2){ 0.0, 0.0 };
    _page->pan_moved_at = _now;
}

// The hand let go of a pan: moving still (in the last 0.08 seconds, faster than four times FUDE_ZOOM_PAGE_COAST_MIN
// either way), it carries on.
RDE_INTERNAL void fude_zoom_page_pan_let_go(fude_zoom_page* _page) {
    const fude_zoom_v2 _v = _page->pan_velocity;
    _page->coasting = rde_engine_get_time_now() - _page->pan_moved_at < 0.08 &&
                      (rde_math_abs_f64(_v.x) > FUDE_ZOOM_PAGE_COAST_MIN * 4.0 || rde_math_abs_f64(_v.y) > FUDE_ZOOM_PAGE_COAST_MIN * 4.0);
}

// See page_parts.h. A frame's time is taken as 0.05 seconds at most. It stops once slower than FUDE_ZOOM_PAGE_COAST_MIN
// both ways.
void fude_zoom_page_coast(fude_zoom_page* _page) {
    if(!_page->coasting) {
        return;
    }
    if(fude_zoom_page_finger_count(_page) > 0 || _page->drawing || _page->erasing || _page->flight.active || _page->mouse_panning) {
        _page->coasting = false;
        return;
    }
    fude_redraw_changed();   // (render on demand: the flick moves the view every frame)
    const f64 _dt = rde_math_min_f64((f64)rde_engine_get_delta(), 0.05);
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ _page->pan_velocity.x * _dt, _page->pan_velocity.y * _dt });
    fude_zoom_camera_settle(&_page->scene, fude_zoom_page_half(_page));
    const f64 _keep = rde_math_exp_f64(-FUDE_ZOOM_PAGE_FRICTION * _dt);
    _page->pan_velocity.x *= _keep;
    _page->pan_velocity.y *= _keep;
    if(rde_math_abs_f64(_page->pan_velocity.x) < FUDE_ZOOM_PAGE_COAST_MIN && rde_math_abs_f64(_page->pan_velocity.y) < FUDE_ZOOM_PAGE_COAST_MIN) {
        _page->coasting = false;
    }
}

// A finger down that moves the view (unless a panel takes it): a flight, a flick, the depth's crumbs, Places and the
// context menu all end. The first one down starts a tap's count and clock. A finger past FUDE_ZOOM_PAGE_FINGERS is
// not kept.
RDE_INTERNAL void fude_zoom_page_finger_down(fude_zoom_page* _page, u64 _id, rde_vec_2F _at) {
    if(fude_zoom_page_library_down(_page, _at, _id + 1u) || fude_zoom_page_play_down(_page, _at, _id + 1u) || fude_zoom_page_map_down(_page, _at, _id + 1u) ||
       fude_zoom_page_strip_down(_page, _at, _id + 1u) || fude_zoom_page_everywhere_down(_page, _at, _id + 1u) || fude_zoom_page_audio_down(_page, _at, _id + 1u) ||
       fude_zoom_page_pieces_down(_page, _at, _id + 1u)) {
        return;   // The panels': their own.
    }
    _page->flight.active = false;
    _page->coasting      = false;
    _page->crumbs_open   = false;
    _page->places_open   = false;
    fude_pagemenu_close_context(&_page->app->ui->page);
    if(fude_zoom_page_finger_count(_page) == 0) {
        _page->tap_start    = rde_engine_get_time_now();
        _page->tap_fingers  = 0;
        _page->tap_spoiled  = false;
        _page->long_pressed = false;
        _page->pan_velocity = (fude_zoom_v2){ 0.0, 0.0 };
        _page->pan_pending  = (fude_zoom_v2){ 0.0, 0.0 };
        _page->pan_moved_at = _page->tap_start;
    }
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS; _i++) {
        if(!_page->fingers[_i].active) {
            _page->fingers[_i] = (fude_zoom_finger){ true, _id, _at, _at };
            break;
        }
    }
    const u32 _n = fude_zoom_page_finger_count(_page);
    _page->tap_fingers = _n > _page->tap_fingers ? _n : _page->tap_fingers;
}

// The first two fingers move the camera: one pans, two pan by their middle and pinch round it. A finger that has gone
// further than FUDE_ZOOM_PAGE_TAP_SLOP spoils the tap.
RDE_INTERNAL void fude_zoom_page_finger_moved(fude_zoom_page* _page, u64 _id, rde_vec_2F _at) {
    if(fude_zoom_page_library_moved(_page, _at, _id + 1u) || fude_zoom_page_play_moved(_page, _at, _id + 1u) || fude_zoom_page_map_moved(_page, _at, _id + 1u) ||
       fude_zoom_page_strip_moved(_page, _at, _id + 1u) || fude_zoom_page_everywhere_moved(_page, _at, _id + 1u) || fude_zoom_page_audio_moved(_page, _at, _id + 1u) ||
       fude_zoom_page_pieces_moved(_page, _at, _id + 1u)) {
        return;
    }
    fude_zoom_finger* _f = fude_zoom_page_finger(_page, _id);
    if(_f == NULL) {
        return;
    }
    const f32 _dx = _at.x - _f->start.x, _dy = _at.y - _f->start.y;
    if(_dx * _dx + _dy * _dy > FUDE_ZOOM_PAGE_TAP_SLOP * FUDE_ZOOM_PAGE_TAP_SLOP) {
        _page->tap_spoiled = true;
    }
    fude_zoom_finger* _two[2] = { NULL, NULL };
    u32 _k = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_PAGE_FINGERS && _k < 2u; _i++) {
        if(_page->fingers[_i].active) {
            _two[_k++] = &_page->fingers[_i];
        }
    }
    fude_zoom_scene* _s = &_page->scene;
    if(_k == 2u && (_f == _two[0] || _f == _two[1])) {
        fude_zoom_finger* _other = _f == _two[0] ? _two[1] : _two[0];
        const fude_zoom_v2 _old_mid = { (_f->at.x + _other->at.x) * 0.5, (_f->at.y + _other->at.y) * 0.5 };
        const fude_zoom_v2 _new_mid = { (_at.x + _other->at.x) * 0.5, (_at.y + _other->at.y) * 0.5 };
        const f64 _old_d = rde_math_hypot_f64(_f->at.x - _other->at.x, _f->at.y - _other->at.y);
        const f64 _new_d = rde_math_hypot_f64(_at.x - _other->at.x, _at.y - _other->at.y);
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _new_mid.x - _old_mid.x, _new_mid.y - _old_mid.y });
        if(_old_d > 1.0 && _new_d > 1.0) {
            fude_zoom_camera_zoom_at(_s, _new_mid, _new_d / _old_d);
        }
        fude_zoom_page_pan_speed(_page, (fude_zoom_v2){ _new_mid.x - _old_mid.x, _new_mid.y - _old_mid.y });
    } else if(_k == 1u) {
        fude_zoom_camera_pan(_s, (fude_zoom_v2){ _at.x - _f->at.x, _at.y - _f->at.y });
        fude_zoom_page_pan_speed(_page, (fude_zoom_v2){ _at.x - _f->at.x, _at.y - _f->at.y });
    }
    _f->at = _at;
    fude_zoom_camera_settle(_s, fude_zoom_page_half(_page));
}

// A finger up. When it is the last, a pan carries on as a flick, or a quick tap of fingers that hardly moved does what
// their count says.
RDE_INTERNAL void fude_zoom_page_finger_up(fude_zoom_page* _page, u64 _id) {
    if(fude_zoom_page_library_up(_page, _id + 1u) || fude_zoom_page_play_up(_page, _id + 1u) || fude_zoom_page_map_up(_page, _id + 1u) ||
       fude_zoom_page_strip_up(_page, _id + 1u) || fude_zoom_page_everywhere_up(_page, _id + 1u) || fude_zoom_page_audio_up(_page, _id + 1u) ||
       fude_zoom_page_pieces_up(_page, _id + 1u)) {
        return;
    }
    fude_zoom_finger* _f = fude_zoom_page_finger(_page, _id);
    if(_f == NULL) {
        return;
    }
    _f->active = false;
    if(fude_zoom_page_finger_count(_page) > 0) {
        return;
    }
    if(_page->tap_spoiled && !_page->long_pressed) {
        fude_zoom_page_pan_let_go(_page);   // A flick: the view carries on.
    }
    // The last one up: a quick tap of one on a mark goes there; of two is undo, of three redo.
    if(!_page->tap_spoiled && rde_engine_get_time_now() - _page->tap_start <= FUDE_ZOOM_PAGE_TAP_TIME) {
        if(_page->presenting) {
            if(_page->tap_fingers == 1u) {
                fude_zoom_page_present_tap(_page, _f->start);   // Presenting: on or back; nothing undone.
            }
        } else if(_page->play_on) {
            // Playing: nothing undone or done again by fingers' taps.
        } else if(_page->tap_fingers == 1u && _page->inline_on) {
            fude_zoom_page_inline_down(_page, _f->start);   // A text being written: its caret there, or it is saved.
            fude_zoom_page_inline_up(_page);
        } else if(_page->tap_fingers == 1u && fude_zoom_page_audio_tap(_page, _f->start)) {
            // Ink a recording linked, its panel up: played from where it was written.
        } else if(_page->tap_fingers == 1u) {
            // A note's task ticked, its link opened; else a mark pressed.
            if(!fude_zoom_page_rich_tap(_page, _f->start, false)) {
                fude_zoom_page_mark_press(_page, _f->start);
            }
        } else if(_page->tap_fingers == 2u) {
            fude_zoom_page_undo(_page);
        } else if(_page->tap_fingers == 3u) {
            fude_zoom_page_redo(_page);
        }
        fude_ui_update(_page->app->ui);
    }
}

// --- events ----------------------------------------------------------------------------------

// Lets go of what fingers were doing on the page: a finger's stroke ends (kept, as a lift keeps it) and the view's
// fingers are dropped, with no tap or flick. When the pen comes down, and when the system takes the fingers away.
RDE_INTERNAL void fude_zoom_page_fingers_let_go(fude_zoom_page* _page) {
    if(_page->finger_drawing) {
        fude_zoom_page_up(_page);
        _page->finger_drawing = false;
    }
    rde_memset(_page->fingers, 0, sizeof(_page->fingers));
}

// A pen's press (fude_zoom_page_event's): the offset pad's grip, the UI, or the page, which a finger drawing or
// moving it gives up to the pen.
RDE_INTERNAL void fude_zoom_page_event_pen_down(fude_zoom_page* _page, rde_event* _event, fude_app* _app, fude_ui* _ui, fude_ink* _cap) {
    const rde_event_pen* _pen    = &_event->data.pen_event_data;
    const rde_vec_2F     _screen = _pen->position;
    fude_page_pen_came(_app->page);
    _cap->pen_seen    = true;
    _cap->eraser      = _pen->eraser;
    _cap->sample_time = (f64)_event->time_stamp * 1e-9;
    if(fude_zoom_page_grip_down(_page, _screen, FUDE_ZOOM_PAGE_HAND_PEN)) {
        _page->pen_on_ui = true;   // The pad's: its moves are the grip's.
        return;
    }
    if(fude_ui_hit(_ui, _screen)) {
        _page->pen_on_ui = true;
        return;
    }
    fude_toolbar_close_panels(&_ui->bar);   // A press on the page closes the bar's panels.
    fude_zoom_page_fingers_let_go(_page);   // The pen takes over from a finger drawing or moving the page.
    fude_zoom_page_down(_page, _screen, true, _pen->eraser);
}

// A pen's move (fude_zoom_page_event's): where it hovers, eased; then to the grip, or to the page unless it was
// pressed on the UI.
RDE_INTERNAL void fude_zoom_page_event_pen_moved(fude_zoom_page* _page, rde_event* _event, fude_ui* _ui, fude_ink* _cap) {
    _cap->pen_seen = true;
    {
        // Eased toward the pen while it hovers near where it was (a pen leaving the glass reads unsteadily);
        // a jump of 30 points or more is followed at once.
        const rde_vec_2F _at = _event->data.pen_event_data.position;
        const f32 _dx = _at.x - _page->hover_at.x, _dy = _at.y - _page->hover_at.y;
        const b8  _near = _page->hover_on && !_event->data.pen_event_data.down && _dx * _dx + _dy * _dy < 900.0f;
        _page->hover_at     = _near ? (rde_vec_2F){ _page->hover_at.x + _dx * 0.35f, _page->hover_at.y + _dy * 0.35f } : _at;
        _page->hover_eraser = _event->data.pen_event_data.eraser;
        _page->hover_on     = !_page->pen_on_ui && !fude_ui_hit(_ui, _at);
    }
    if(fude_zoom_page_grip_moved(_page, _event->data.pen_event_data.position, FUDE_ZOOM_PAGE_HAND_PEN)) {
        return;
    }
    if(_page->pen_on_ui) {
        return;
    }
    _cap->sample_time = (f64)_event->time_stamp * 1e-9;
    fude_zoom_page_moved(_page, _event->data.pen_event_data.position);
}

// A finger down (fude_zoom_page_event's), to the first that takes it: the grip, the UI, a part worked by hand
// or a mechanism held while playing, an instrument, a pinch begun just after a stroke, the press when fingers
// write; else the view moved. The pen's echoes and a palm resting while the pen draws are left out.
RDE_INTERNAL void fude_zoom_page_event_touch_down(fude_zoom_page* _page, rde_event* _event, fude_app* _app, fude_ui* _ui, fude_ink* _cap) {
    const rde_event_mobile* _touch = &_event->data.mobile_event_data;
    if(!_touch->from_pen) {
        _page->hover_on = false;   // A finger: no pen hovering (nor where it last was).
    }
    if(_touch->from_pen || ((_page->drawing || _page->erasing || fude_zoom_select_busy(&_page->selection)) && !_page->finger_drawing)) {
        return;
    }
    const rde_vec_2F _at = { (f32)_touch->init_touch_position.x, (f32)_touch->init_touch_position.y };   // (screen space)
    if(fude_zoom_page_grip_down(_page, _at, FUDE_ZOOM_PAGE_HAND_FINGER + _touch->finger_id)) {
        return;
    }
    if(fude_ui_hit(_ui, _at)) {
        return;
    }
    fude_toolbar_close_panels(&_ui->bar);
    // Playing: a first finger on a part worked by hand works it (pressed while it is down); elsewhere it moves
    // the view, as ever. (With fingers writing, the press goes as the pen's does.)
    if(_page->play_on && _page->play_circuit && !_app->finger_writes && fude_zoom_page_finger_count(_page) == 0 &&
       fude_zoom_page_circuit_press(_page, _at)) {
        _page->play_finger = _touch->finger_id + 1u;
        return;
    }
    // A mechanism playing: a first finger on a crank turns it, on a part that moves takes hold of it.
    if(_page->play_on && _page->play_mech && !_app->finger_writes && fude_zoom_page_finger_count(_page) == 0 && _page->mech_hold == 0u &&
       fude_zoom_page_mech_hold(_page, _at)) {
        _page->mech_finger = _touch->finger_id + 1u;
        return;
    }
    // An instrument under a first finger (or a second by the one held) is the finger's to move.
    if(((fude_zoom_page_finger_count(_page) == 0 && !_page->finger_drawing) || _page->instruments.held >= 0) &&
       fude_zoom_instruments_finger_down(&_page->instruments, _touch->finger_id, (fude_zoom_v2){ _at.x, _at.y })) {
        return;
    }
    if(_page->finger_drawing) {
        // A second finger soon after the first began: it was a pinch, not a stroke. The stroke goes, and the
        // two move the page.
        if(rde_engine_get_time_now() - _page->stroke_began <= FUDE_ZOOM_PAGE_TWO_FINGER) {
            fude_zoom_page_draw_cancel(_page);
            fude_zoom_select_abort(&_page->selection, &_page->scene);   // The lasso's loop or drag too,
            _page->shaping      = false;                                 // a shape being dragged out,
            _page->link_pending = false;                                 // a link's tap.
            _page->erasing = false;
            _page->finger_drawing = false;
            fude_zoom_page_finger_down(_page, _page->writer, _page->erase_last);
            fude_zoom_page_finger_down(_page, _touch->finger_id, _at);
        }
        return;
    }
    if(_app->finger_writes && fude_zoom_page_finger_count(_page) == 0) {
        _page->finger_drawing = true;
        _page->writer         = _touch->finger_id;
        _page->erase_last     = _at;
        _cap->eraser          = false;
        _cap->sample_time     = rde_engine_get_time_now();
        _page->stroke_began   = _cap->sample_time;   // Whatever it begins (a stroke, a lasso, a shape), a
                                                     // second finger soon after makes it a pinch.
        fude_zoom_page_down(_page, _at, false, false);
        return;
    }
    fude_zoom_page_finger_down(_page, _touch->finger_id, _at);
}

// A finger moved (fude_zoom_page_event's), to what it holds: a mechanism, a knob, the grip, an instrument, the
// stroke it writes; else the view.
RDE_INTERNAL void fude_zoom_page_event_touch_moved(fude_zoom_page* _page, rde_event* _event, fude_ink* _cap) {
    const rde_event_mobile* _touch = &_event->data.mobile_event_data;
    if(_touch->from_pen) {
        return;
    }
    const rde_vec_2F _at = { (f32)_touch->moved_touch_position.x, (f32)_touch->moved_touch_position.y };
    if(_page->mech_hold != 0u && _page->mech_finger == _touch->finger_id + 1u) {
        fude_zoom_page_mech_hold_moved(_page, _at);
        return;
    }
    if(_page->knob_part != FUDE_ZOOM_NONE && _page->play_finger == _touch->finger_id + 1u) {
        fude_zoom_page_knob_moved(_page, _at);
        return;
    }
    if(fude_zoom_page_grip_moved(_page, _at, FUDE_ZOOM_PAGE_HAND_FINGER + _touch->finger_id)) {
        return;
    }
    if(fude_zoom_instruments_finger_moved(&_page->instruments, _touch->finger_id, (fude_zoom_v2){ _at.x, _at.y })) {
        fude_zoom_page_instrument_snap(_page, FUDE_ZOOM_SNAP_RADIUS * 1.6);   // A finger is broader than a pen.
        return;
    }
    if(_page->finger_drawing && _touch->finger_id == _page->writer) {
        _cap->sample_time = rde_engine_get_time_now();
        fude_zoom_page_moved(_page, _at);
        if(!_page->erasing) {
            _page->erase_last = _at;
        }
        return;
    }
    fude_zoom_page_finger_moved(_page, _touch->finger_id, _at);
}

// A finger up (fude_zoom_page_event's), to what it held: the grip, a push button, a mechanism, an instrument,
// the stroke it wrote; else the view's fingers (a tap among them).
RDE_INTERNAL void fude_zoom_page_event_touch_up(fude_zoom_page* _page, rde_event* _event) {
    const rde_event_mobile* _touch = &_event->data.mobile_event_data;
    if(_touch->from_pen) {
        return;
    }
    if(fude_zoom_page_grip_up(_page, FUDE_ZOOM_PAGE_HAND_FINGER + _touch->finger_id)) {
        return;
    }
    if(_page->play_finger != 0u && _page->play_finger == _touch->finger_id + 1u) {
        fude_zoom_page_circuit_release(_page);   // A push button let go.
        _page->play_finger = 0u;
        return;
    }
    if(_page->mech_hold != 0u && _page->mech_finger == _touch->finger_id + 1u) {
        fude_zoom_page_mech_release(_page);
        return;
    }
    if(fude_zoom_instruments_finger_up(&_page->instruments, _touch->finger_id)) {
        _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
        return;
    }
    if(_page->finger_drawing && _touch->finger_id == _page->writer) {
        fude_zoom_page_up(_page);
        _page->finger_drawing = false;
        return;
    }
    fude_zoom_page_finger_up(_page, _touch->finger_id);
}

#if !defined(RDE_PLATFORM_MOBILE)
// A mouse button pressed (fude_zoom_page_event's; there is no mouse on a phone or a tablet): the left one on the
// grip, or pressing as the pen does; the right one panning the page (a click without a drag: the context menu).
RDE_INTERNAL void fude_zoom_page_event_mouse_down(fude_zoom_page* _page, rde_event* _event, fude_app* _app, fude_ui* _ui, fude_ink* _cap) {
    const rde_vec_2I _m      = rde_input_mouse_get_position(_app->window);
    const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
    if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && fude_zoom_page_grip_down(_page, _screen, FUDE_ZOOM_PAGE_HAND_MOUSE)) {
        return;
    }
    if(_event->handled || fude_ui_hit(_ui, _screen)) {
        return;
    }
    fude_toolbar_close_panels(&_ui->bar);
    if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
        _page->mouse_panning = true;
        _page->mouse_last    = _screen;
        _page->mouse_down    = _screen;
        _page->mouse_dragged = false;
        _page->coasting      = false;
        _page->pan_velocity  = (fude_zoom_v2){ 0.0, 0.0 };
        _page->pan_pending   = (fude_zoom_v2){ 0.0, 0.0 };
        _page->pan_moved_at  = rde_engine_get_time_now();
    } else if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
        _cap->sample_time = rde_engine_get_time_now();
        fude_zoom_page_down(_page, _screen, false, false);
    }
}
#endif

// See page.h. The pen goes to the offset pad's grip, the UI, or the page (fude_zoom_page_down and the rest). A finger
// goes to the grip, the UI, a part worked by hand while playing, an instrument, the press when fingers write (unless a
// second finger soon after makes it a pinch), or else moves the view; fingers the system takes away let go of the page,
// as when the pen comes down. The mouse's left button presses as the pen does and its right button pans (a click
// without a drag opens the context menu). Key events are not handled here.
// The longer kinds of event are steps of their own above (fude_zoom_page_event_*), handed the app, the UI and the
// capture they use.
void fude_zoom_page_event(fude_zoom_page* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;
    fude_ink* _cap = &_page->capture;

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_AXIS: {
            fude_ink_pen_axis(_cap, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            fude_zoom_page_event_pen_down(_page, _event, _app, _ui, _cap);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            fude_zoom_page_event_pen_moved(_page, _event, _ui, _cap);
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            fude_zoom_page_grip_up(_page, FUDE_ZOOM_PAGE_HAND_PEN);
            _cap->pen_seen    = true;
            _page->pen_on_ui  = false;
            _page->hover_quiet = rde_engine_get_time_now() + FUDE_ZOOM_PAGE_HOVER_QUIET;
            if(!_page->finger_drawing) {
                fude_zoom_page_up(_page);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            _page->hover_on = false;
        } break;

        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            fude_toolbar_pen_double_tap(&_ui->bar, _event->data.pen_event_data.tap_action);
        } break;

        // SDL echoes every pen contact as a touch (from_pen): skipped. A finger landing while the pen draws is a
        // resting palm.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            fude_zoom_page_event_touch_down(_page, _event, _app, _ui, _cap);
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            fude_zoom_page_event_touch_moved(_page, _event, _cap);
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            fude_zoom_page_event_touch_up(_page, _event);
        } break;

        // The system took the fingers away (a palm the pen rejects, an edge gesture): they will never come up, so what
        // they did on the page is let go, as when the pen comes down, and the next finger never pinches against one
        // that is gone. Android cancels every finger of a gesture at once.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED: {
            if(!_event->data.mobile_event_data.from_pen) {
                fude_zoom_page_fingers_let_go(_page);
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            fude_zoom_page_event_mouse_down(_page, _event, _app, _ui, _cap);
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                if(_page->mouse_panning && !_page->mouse_dragged && !_page->play_on) {
                    fude_zoom_page_context_open(_page, _page->mouse_down);   // Where Paste lands: the screen.
                }
                _page->mouse_panning = false;
                if(_page->mouse_dragged) {
                    fude_zoom_page_pan_let_go(_page);   // A flick of the mouse: the view carries on.
                }
            } else if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                if(!fude_zoom_page_grip_up(_page, FUDE_ZOOM_PAGE_HAND_MOUSE)) {
                    fude_zoom_page_up(_page);
                }
            }
        } break;
#endif

        default: break;
    }
}
