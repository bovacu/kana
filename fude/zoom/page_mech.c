// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, Play and mechanisms: a hand on a mechanism playing (mechrun.h), Play (what the lasso holds, simulated on
// its own: its panel, its steps, a speaker's sound), mechanisms drawn as they move, bodies and a part's limits.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// Play's Pause, defined with Play below.
RDE_INTERNAL void fude_zoom_page_play_pause(fude_zoom_page* _page, b8 _pause);

// --- a hand on a mechanism playing (mechrun.h) ---

// Screen point _screen in the home frame's units.
RDE_INTERNAL fude_zoom_v2 fude_zoom_page_home_at(const fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_scene* _s = &_page->scene;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    return fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _home)), (fude_zoom_v2){ _screen.x, _screen.y });
}

// Playing, a hand put on the mechanism: on a hand crank — it turns as the hand goes round it —, else on a part that
// moves — it follows the hand. True: one taken.
b8 fude_zoom_page_mech_hold(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_mech_world* _w = &_page->mech_run;
    if(!_page->play_on || !_page->play_mech || !fude_zoom_mech_world_on(_w)) {
        return false;
    }
    const fude_zoom_v2 _at = fude_zoom_page_home_at(_page, _screen);
    const u32 _c = fude_zoom_mech_world_crank_at(_w, _at);
    if(_c != FUDE_ZOOM_NONE) {
        const fude_zoom_v2 _m = fude_zoom_mech_world_crank_middle(_w, _c);
        _page->mech_hold        = 2u;
        _page->mech_hold_crank  = _c;
        _page->mech_hold_angle  = rde_math_atan2_f64(_at.y - _m.y, _at.x - _m.x);
        _page->mech_hold_offset = fude_zoom_mech_world_crank_angle(_w, _c) - _page->mech_hold_angle;
        fude_zoom_mech_world_crank_hold(_w, _c, fude_zoom_mech_world_crank_angle(_w, _c));   // (held where it is)
        return true;
    }
    if(fude_zoom_mech_world_grab(_w, _at)) {
        _page->mech_hold = 1u;
        return true;
    }
    return false;
}

// See page_parts.h. A crank turns as far as the hand has gone round its middle, every turn counted: each move's angle
// is taken the short way round and added up.
void fude_zoom_page_mech_hold_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_v2 _at = fude_zoom_page_home_at(_page, _screen);
    if(_page->mech_hold == 1u) {
        fude_zoom_mech_world_drag(_w, _at);
    } else if(_page->mech_hold == 2u) {
        const fude_zoom_v2 _m = fude_zoom_mech_world_crank_middle(_w, _page->mech_hold_crank);
        f64 _d = rde_math_atan2_f64(_at.y - _m.y, _at.x - _m.x) - _page->mech_hold_angle;
        _d -= 2.0 * RDE_PI * rde_math_floor_f64((_d + RDE_PI) / (2.0 * RDE_PI));
        _page->mech_hold_angle += _d;
        fude_zoom_mech_world_crank_hold(_w, _page->mech_hold_crank, _page->mech_hold_angle + _page->mech_hold_offset);
    }
}

// See page_parts.h: the part held, or the crank, free again.
void fude_zoom_page_mech_release(fude_zoom_page* _page) {
    if(_page->mech_hold == 1u) {
        fude_zoom_mech_world_let_go(&_page->mech_run);
    } else if(_page->mech_hold == 2u) {
        fude_zoom_mech_world_crank_let_go(&_page->mech_run, _page->mech_hold_crank);
    }
    _page->mech_hold   = 0u;
    _page->mech_finger = 0u;
}

// Puts the button held by hand up now (bit 0 of its switch_on cleared) and forgets it. Put up, it jumps, as it did
// when pressed (fude_zoom_circuit_tap): the next step backward Euler, and short.
RDE_INTERNAL void fude_zoom_page_circuit_let_go(fude_zoom_page* _page) {
    if(_page->circuit_held == FUDE_ZOOM_NONE) {
        return;
    }
    const u32 _i = fude_zoom_page_circuit_part(_page, _page->circuit_held);
    fude_zoom_circuit_part* _p = _i != FUDE_ZOOM_NONE ? &((fude_zoom_circuit_part*)_page->circuit.parts.memory)[_i] : NULL;
    if(_p != NULL && (_p->switch_on & 1ull)) {
        _p->switch_on &= ~1ull;
        fude_zoom_circuit_jump(&_page->circuit);
    }
    _page->circuit_held    = FUDE_ZOOM_NONE;
    _page->circuit_letting = false;
}

// How long a button held must have been down, in the circuit's seconds, before letting go puts it up: a quick tap's
// down and up can come in one frame, and then it was never down while the circuit stepped, so what it drives would
// never have seen it pressed.
#define FUDE_ZOOM_PAGE_PRESS_LEAST 0.1

// An oscilloscope's knob held, the hand moved: turned a step clockwise for each step's length it has gone right or up
// (back, left or down) — what it is set to said as it changes; past its end, the hand's way back to it from where it
// stops.
void fude_zoom_page_knob_moved(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(_page->knob_part == FUDE_ZOOM_NONE) {
        return;
    }
    const f32 _dx = _screen.x - _page->knob_from.x, _dy = _screen.y - _page->knob_from.y;   // (the screen's y up)
    if(!_page->knob_dragged && rde_math_hypot_f32(_dx, _dy) < FUDE_ZOOM_PAGE_KNOB_STILL) {
        return;
    }
    _page->knob_dragged = true;
    c8 _say[64];
    b8 _said = false;
    for(u32 _guard = 0; _guard < 64u; _guard++) {
        const f32 _along = _screen.x - _page->knob_from.x + _screen.y - _page->knob_from.y;
        const i32 _way = _along >= _page->knob_step ? 1 : (_along <= -_page->knob_step ? -1 : 0);
        if(_way == 0) {
            break;
        }
        const fude_zoom_circuit_part* _q = &((const fude_zoom_circuit_part*)_page->circuit.parts.memory)[_page->knob_part];
        const fude_zoom_scope _was = fude_zoom_display_scope_of(_q);
        fude_zoom_circuit_scope_turn(&_page->circuit, _page->knob_part, _page->knob, _way, _say, sizeof(_say));
        const fude_zoom_scope _now = fude_zoom_display_scope_of(_q);
        _said = _said || rde_math_abs_f64(fude_zoom_scope_knob_round(&_now, _page->knob) - fude_zoom_scope_knob_round(&_was, _page->knob)) > 1e-12;
        _page->knob_from.x += (f32)_way * _page->knob_step;   // (a step's way used; at its end too — back from there at once)
    }
    if(_said) {
        fude_notice_show(_say);
    }
}

// The knob let go: when it was not dragged it was a tap, and turns one step its way (knob_way).
RDE_INTERNAL void fude_zoom_page_knob_release(fude_zoom_page* _page) {
    if(_page->knob_part == FUDE_ZOOM_NONE) {
        return;
    }
    c8 _say[64];
    if(!_page->knob_dragged && fude_zoom_circuit_scope_turn(&_page->circuit, _page->knob_part, _page->knob, _page->knob_way, _say, sizeof(_say))) {
        fude_notice_show(_say);
    }
    _page->knob_part = FUDE_ZOOM_NONE;
}

// See page_parts.h. While Play runs, a button down for less than FUDE_ZOOM_PAGE_PRESS_LEAST of the circuit's time is
// put up later, once it has been (fude_zoom_page_play_step).
void fude_zoom_page_circuit_release(fude_zoom_page* _page) {
    fude_zoom_page_knob_release(_page);
    if(_page->circuit_held == FUDE_ZOOM_NONE) {
        return;
    }
    if(_page->play_running && _page->circuit.time < _page->circuit_held_at + FUDE_ZOOM_PAGE_PRESS_LEAST) {
        _page->circuit_letting = true;   // (up in play_step, once it has been seen)
        return;
    }
    fude_zoom_page_circuit_let_go(_page);
}

// --- Play: what the lasso holds, simulated on its own ---------------------------------------------------------
//
// Pressed on the lasso's row (or the Circuit's and Motion's Play): what it holds — a circuit's parts (and the wires
// joined to them), a mechanism's, graphs with variables — is built into its own simulations (circuit.h's circuit,
// mechrun.h's world, plot.h's time and sliders) that run on their own, as a game's world does: the canvas is read to
// build them and never written while they run, and the view draws what they are now over it (current along the
// wires, LEDs lit, the parts that move where they have moved, the graphs as their variables go). Its panel at the
// bottom: Pause (Go on), Back to the start, Stop; a played graph's variables, a slider each. A circuit's parts edited
// while it plays (a value, a wire drawn to them) are built in again, its state kept; a mechanism's are taken on Back
// to the start.

#define FUDE_ZOOM_PAGE_PLAY_W      460.0   // the panel, its widest (points)
#define FUDE_ZOOM_PAGE_PLAY_ROW    48.0    // its buttons' row
#define FUDE_ZOOM_PAGE_PLAY_SLIDER 40.0    // a slider's row
#define FUDE_ZOOM_PAGE_PLAY_PAD    10.0    // round its edge

// What object _o is to Play: 1 a circuit's part, 2 a mechanism's, 4 a graph with variables, 8 a wire (0: nothing).
RDE_INTERNAL u8 fude_zoom_page_play_kind(const fude_zoom_page* _page, u32 _o) {
    const fude_zoom_scene* _s = &_page->scene;
    if(_o >= fude_zoom_scene_object_count(_s)) {
        return 0u;
    }
    const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
    if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE) {
        return 0u;
    }
    if(_ob->channels == FUDE_ZOOM_SHAPE_WIRE) {
        return 8u;
    }
    if(_ob->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return 0u;
    }
    if(fude_zoom_part_of(_s, _o) != NULL) {
        return 1u;
    }
    if(fude_zoom_mech_of(_s, _o) != NULL) {
        return 2u;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const fude_zoom_symbol_info* _info = _count >= 4u ? fude_zoom_symbol_info_of(fude_zoom_symbol_kind_of(_n, _count)) : NULL;
    if(_info != NULL && rde_strcmp(_info->id, "graph") == 0) {
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
        fude_zoom_plot_var _v[1];
        return fude_zoom_plot_vars(_info->id, _text, _v, 1u) > 0u ? 4u : 0u;
    }
    return 0u;
}

// Drawings made bodies (props.h) that play as a mechanism's: one in _scope (a byte an object: it, or its properties, in
// it) — or, _scope NULL, among the lasso's picks (looked through for each body on the canvas: few). The whole scene is
// gone through for the bodies: the row's face asks through fude_zoom_page_play_what, which keeps its answer.
RDE_INTERNAL b8 fude_zoom_page_play_drawn(const fude_zoom_page* _page, const u8* _scope) {
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_pick*  _p = (const fude_zoom_pick*)_page->selection.picks.memory;
    const u32 _np = (u32)rde_arr_length(&_page->selection.picks);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        if(fude_zoom_props_kind(_s, _i) != FUDE_ZOOM_PROPS_BODY) {
            continue;
        }
        const u32 _t = fude_zoom_props_target(_s, _i);
        if(_t == FUDE_ZOOM_NONE) {
            continue;
        }
        if(_scope != NULL && (_scope[_i] || _scope[_t])) {
            return true;
        }
        for(u32 _k = 0; _scope == NULL && _k < _np; _k++) {
            if(_p[_k].object == _i || _p[_k].object == _t) {
                return true;
            }
        }
    }
    return false;
}

// See page_parts.h. Worked out again only when fude_zoom_page_held_key or the custom parts known change (a part learnt,
// renamed or forgotten changes which symbols are parts, the scene unchanged): the row's face asks each frame, and the
// drawings made bodies are looked for through the whole scene.
u8 fude_zoom_page_play_what(fude_zoom_page* _page) {
    if(rde_arr_length(&_page->selection.picks) == 0u) {
        return 0u;
    }
    const u64 _key   = fude_zoom_page_held_key(_page);
    const u32 _parts = fude_zoom_logic_revision();
    if(_key == _page->play_what_key && _parts == _page->play_what_parts) {
        return _page->play_what_was;
    }
    u8 _what = 0u;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        _what |= fude_zoom_page_play_kind(_page, ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object);
    }
    _what = (u8)(_what | (!(_what & 2u) && fude_zoom_page_play_drawn(_page, NULL) ? 2u : 0u));
    _page->play_what_key   = _key;
    _page->play_what_parts = _parts;
    _page->play_what_was   = _what;
    return _what;
}

// --- a speaker's sound -------------------------------------------------------------------------------------------
//
// While a circuit plays with a speaker in it, its sound (circuit.h: the speakers' currents, sampled as the circuit
// steps) is played as it is made — rde's audio stream on the default output, mono at the device's rate, a tenth of a
// second ahead at most. Only at ×1: slower or faster, its pitch would be wrong — silent then.
#define FUDE_ZOOM_PAGE_SOUND_AHEAD 0.12   // seconds queued at most (what is made past it let go: a step behind is a step late)
// Steps a frame at most while a speaker is heard or an oscilloscope watched (else Play's 400): a 555 at a few hundred
// hertz cuts its step at each edge, some 30 000 steps a second — under 400 a frame it falls behind, and what is heard
// stutters —; an oscilloscope's steps are no longer than its samples are apart (at 1 ms a division, 40 000 a second).
// (A few milliseconds a frame.)
#define FUDE_ZOOM_PAGE_FINE_STEPS 2000u

// See page_parts.h. The circuit stops sampling its speakers too.
void fude_zoom_page_sound_off(fude_zoom_page* _page) {
    if(_page->sound != NULL) {
        rde_audio_stream_close(_page->sound);
        _page->sound = NULL;
    }
    fude_zoom_circuit_sound(&_page->circuit, 0.0);
}

// As Play starts: a stream opened when a speaker plays (closed when none does), and play_fine set when a speaker or an
// oscilloscope is in the circuit (FUDE_ZOOM_PAGE_FINE_STEPS).
RDE_INTERNAL void fude_zoom_page_sound_on(fude_zoom_page* _page) {
    b8 _speaker = false, _scope = false;
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
    for(u32 _i = 0; _page->play_circuit && _i < (u32)rde_arr_length(&_page->circuit.parts); _i++) {
        _speaker = _speaker || _p[_i].part->model == FUDE_ZOOM_MODEL_SPEAKER;
        _scope   = _scope || _p[_i].part->model == FUDE_ZOOM_MODEL_SCOPE;
    }
    _page->play_fine = _speaker || _scope;
    if(!_speaker) {
        fude_zoom_page_sound_off(_page);
        return;
    }
    if(_page->sound == NULL) {
        _page->sound = rde_audio_stream_open(48000u, 1u, 48000u / 4u, NULL);
    }
    fude_zoom_circuit_sound(&_page->circuit, _page->sound != NULL ? (f64)rde_audio_stream_get_sample_rate(_page->sound) : 0.0);
}

// Each frame: what the circuit made, queued (at ×1, running), the stream started once a little is; else let go, silent.
RDE_INTERNAL void fude_zoom_page_sound_feed(fude_zoom_page* _page) {
    fude_zoom_circuit* _c = &_page->circuit;
    if(_page->sound == NULL) {
        rde_arr_clear(&_c->audio);
        return;
    }
    const u32 _rate = rde_audio_stream_get_sample_rate(_page->sound);
    if(!_page->play_running || rde_math_abs_f64(_page->play_rate - 1.0) > 1e-9 || _rate == 0u) {
        rde_arr_clear(&_c->audio);
        rde_audio_stream_stop(_page->sound);
        return;
    }
    const u32 _most = (u32)(FUDE_ZOOM_PAGE_SOUND_AHEAD * (f64)_rate), _queued = rde_audio_stream_queued_frames(_page->sound);
    const u32 _n = (u32)rde_arr_length(&_c->audio), _room = _most > _queued ? _most - _queued : 0u;
    if(_n > 0u && _room > 0u) {
        // (more than there is room for: the newest kept — what is late is heard late, not at all)
        const u32 _take = _n < _room ? _n : _room;
        rde_audio_stream_push(_page->sound, (const f32*)_c->audio.memory + (_n - _take), _take);
    }
    rde_arr_clear(&_c->audio);
    if(rde_audio_stream_queued_frames(_page->sound) > _rate / 30u) {
        rde_audio_stream_start(_page->sound);
    }
}

// See page_parts.h. Everything as drawn again: nothing playing, what was built for it let go, the speaker silent.
void fude_zoom_page_play_stop(fude_zoom_page* _page) {
    _page->mech_hold    = 0u;
    _page->mech_finger  = 0u;
    _page->play_on      = false;
    _page->play_running = false;
    _page->play_circuit = false;
    _page->play_fine    = false;
    _page->play_mech    = false;
    _page->play_plots   = false;
    _page->play_slider  = -1;
    _page->play_hand    = 0u;
    _page->circuit_held = FUDE_ZOOM_NONE;
    _page->knob_part    = FUDE_ZOOM_NONE;
    _page->circuit_letting = false;
    _page->circuit_failed = false;
    _page->play_coupled = false;
    fude_zoom_coupling_clear(&_page->coupling, &_page->circuit);
    fude_zoom_mech_world_stop(&_page->mech_run);
    rde_arr_clear(&_page->mech_lifted);
    rde_arr_clear(&_page->circuit.parts);
    rde_arr_clear(&_page->circuit.wires);
    fude_zoom_page_sound_off(_page);
}

// Built from its scope, from the start (its circuit, its mechanism, its graphs' sliders), and running. False: nothing
// in it plays.
RDE_INTERNAL b8 fude_zoom_page_play_build(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _n = fude_zoom_scene_object_count(_s);
    while((u32)rde_arr_length(&_page->play_scope) < _n) {
        const u8 _no = 0u;
        rde_arr_add(&_page->play_scope, (any)&_no);
    }
    const u8* _scope = (const u8*)_page->play_scope.memory;
    u8 _what = 0u;
    for(u32 _i = 0; _i < _n; _i++) {
        _what |= _scope[_i] ? fude_zoom_page_play_kind(_page, _i) : 0u;
    }
    _what |= !(_what & 2u) && fude_zoom_page_play_drawn(_page, _scope) ? 2u : 0u;   // (drawings made bodies alone: a mechanism)
    fude_zoom_coupling_clear(&_page->coupling, &_page->circuit);
    fude_zoom_mech_world_stop(&_page->mech_run);
    rde_arr_clear(&_page->mech_lifted);
    rde_arr_clear(&_page->circuit.parts);
    rde_arr_clear(&_page->circuit.wires);
    fude_zoom_logic_forget(_page->circuit.logic);   // (from the start: nothing its logic held carried over)
    _page->play_circuit = (_what & 1u) && fude_zoom_circuit_build_in(&_page->circuit, _s, _scope) > 0u;
    if(_page->play_circuit) {
        // (from the start: nothing charged, each part as its text says — built again now, not when the canvas next
        // changes: a canvas left alone while it plays still runs)
        fude_zoom_circuit_reset(&_page->circuit);
        fude_zoom_circuit_build_in(&_page->circuit, _s, _scope);
        _page->play_seen = _s->revision;
    }
    _page->play_mech = (_what & 2u) && fude_zoom_mech_plan_build(&_page->mech, _s, _scope) > 0u && fude_zoom_mech_world_start(&_page->mech_run, &_page->mech);
    if(_page->play_mech) {
        // (what the world draws itself while it runs — the parts that move, springs, ropes, pulleys —, the canvas not)
        u8* _l = (u8*)rde_arr_add_n(&_page->mech_lifted, _n);
        rde_memset(_l, 0, _n);
        const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_page->mech.bodies.memory;
        rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_page->mech_run.bodies.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->mech.bodies); _i++) {
            const u8 _k = _b[_i].part->kind;
            if(((_pb[_i] != NULL && !_b[_i].fixed) || _k == FUDE_ZOOM_MECH_SPRING || _k == FUDE_ZOOM_MECH_ROPE || _k == FUDE_ZOOM_MECH_PULLEY ||
                _k == FUDE_ZOOM_MECH_TRACER || _k == FUDE_ZOOM_MECH_BELT || _k == FUDE_ZOOM_MECH_DAMPER) && _k != FUDE_ZOOM_MECH_PLUNGER && _k != FUDE_ZOOM_MECH_FOLLOWER) {
                _l[_b[_i].object] = 1u;   // (a solenoid's plunger, a follower: their symbols' own, drawn where they are: render.h's moved)
            }
        }
    }
    // Both: stepped together, each told what the other did (a motor turning a gear, a rack pressing a button).
    _page->play_coupled = _page->play_circuit && _page->play_mech &&
                          fude_zoom_coupling_build(&_page->coupling, &_page->circuit, &_page->mech_run, _s) > 0u;
    // Graphs: their variables, a slider each (four at most, the first graph's first), from the start.
    // Sensors (an LDR's light, a thermistor's temperature): a slider each, four at most.
    _page->play_sensor_count = 0u;
    fude_zoom_page_sound_on(_page);
    if(_page->play_circuit) {
        const fude_zoom_circuit_part* _cp = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->circuit.parts) && _page->play_sensor_count < 4u; _i++) {
            if(fude_zoom_circuit_lit(_cp[_i].part->model) || _cp[_i].part->model == FUDE_ZOOM_MODEL_THERMISTOR) {
                _page->play_sensors[_page->play_sensor_count++] = _cp[_i].object;
            }
        }
    }
    _page->play_plots     = (_what & 4u) != 0u;
    _page->play_var_count = 0u;
    _page->plot_play.time = 0.0;
    for(u32 _k = 0; _k < 26u; _k++) {
        _page->plot_play.held[_k] = RDE_NAN_F64;
    }
    for(u32 _i = 0; _i < _n && _page->play_plots && _page->play_var_count < 4u; _i++) {
        if(!_scope[_i] || fude_zoom_page_play_kind(_page, _i) != 4u) {
            continue;
        }
        f64 _num[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _i, _num, FUDE_ZOOM_SHAPE_NUMBERS);
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_num, _count, _text, sizeof(_text));
        fude_zoom_plot_var _v[8];
        const u32 _got = fude_zoom_plot_vars("graph", _text, _v, 8u);
        for(u32 _k = 0; _k < _got && _page->play_var_count < 4u; _k++) {
            b8 _seen = false;
            for(u32 _j = 0; _j < _page->play_var_count; _j++) {
                _seen = _seen || _page->play_vars[_j].letter == _v[_k].letter;
            }
            if(!_seen) {
                _page->play_vars[_page->play_var_count++] = _v[_k];
            }
        }
    }
    _page->play_on      = _page->play_circuit || _page->play_mech || _page->play_plots;
    _page->play_running = _page->play_on;
    _page->play_time    = 0.0;
    _page->play_sim_span = _page->play_real_span = 0.0;
    _page->play_speed   = 1.0;
    _page->play_slow_told = false;
    _page->play_clock   = rde_engine_get_time_now();
    _page->play_slider  = -1;
    _page->circuit_failed = false;
    return _page->play_on;
}

// What the lasso holds played (it put away: the panel shows instead). False: nothing there plays.
RDE_INTERNAL b8 fude_zoom_page_play_start(fude_zoom_page* _page) {
    fude_zoom_page_play_stop(_page);
    const u32 _n = fude_zoom_scene_object_count(&_page->scene);
    rde_arr_clear(&_page->play_scope);
    u8* _scope = (u8*)rde_arr_add_n(&_page->play_scope, _n);
    rde_memset(_scope, 0, _n);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        if(_o < _n) {
            _scope[_o] = 1u;
        }
    }
    if(!fude_zoom_page_play_build(_page)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_NOTHING));
        return false;
    }
    fude_zoom_select_clear(&_page->selection);
    return true;
}

// Play pressed on a tool (the Circuit's, Motion's): what the lasso holds; nothing held — what plays paused or on.
b8 fude_zoom_page_play_selection_now(fude_zoom_page* _page) {
    if(fude_zoom_select_any(&_page->selection)) {
        return fude_zoom_page_play_start(_page);
    }
    if(_page->play_on) {
        fude_zoom_page_play_pause(_page, _page->play_running);
        return true;
    }
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LASSO));
    return false;
}

// Pauses Play (_pause), or lets it go on, from now.
RDE_INTERNAL void fude_zoom_page_play_pause(fude_zoom_page* _page, b8 _pause) {
    _page->play_running = _page->play_on && !_pause;
    _page->play_clock   = rde_engine_get_time_now();
}

// The lasso's row: Play.
void fude_zoom_page_play_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && _page->open) {
        fude_zoom_page_play_start(_page);
    }
}

// Objects made again for others (a value, a move's wires): played if those were.
void fude_zoom_page_play_remade(fude_zoom_page* _page, const u32* _died, const u32* _born, u32 _n) {
    if(!_page->play_on) {
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        while((u32)rde_arr_length(&_page->play_scope) <= _born[_i]) {
            const u8 _no = 0u;
            rde_arr_add(&_page->play_scope, (any)&_no);
        }
        u8* _scope = (u8*)_page->play_scope.memory;
        if(_died[_i] < (u32)rde_arr_length(&_page->play_scope) && _scope[_died[_i]]) {
            _scope[_born[_i]] = 1u;
        }
    }
}

// Defined below.
RDE_INTERNAL void fude_zoom_page_play_events(fude_zoom_page* _page);
RDE_INTERNAL void fude_zoom_page_play_mech_events(fude_zoom_page* _page);
RDE_INTERNAL void fude_zoom_page_play_pace(fude_zoom_page* _page, f64 _sim, f64 _real);
RDE_INTERNAL const c8* fude_zoom_page_part_name(const fude_zoom_page* _page, u32 _o);

// See page_parts.h. A frame's step: as long as the frame took (a tenth of a second at most) at Play's speed, or a
// Step's 60th of a second; a circuit edited since built again, its state kept. The circuit and the mechanism step
// together when they are coupled, else each on its own; what happened is told after.
void fude_zoom_page_play_step(fude_zoom_page* _page) {
    if(!_page->play_on || !_page->open) {
        return;
    }
    if(_page->play_running || _page->play_step_once) {
        fude_redraw_changed();   // (render on demand: what plays moves every frame)
    }
    const f64 _now = rde_engine_get_time_now();
    const f64 _dt_real = rde_math_min_f64(rde_math_max_f64(_now - _page->play_clock, 0.0), 0.1);
    _page->play_clock = _now;
    fude_zoom_scene* _s = &_page->scene;
    while((u32)rde_arr_length(&_page->play_scope) < fude_zoom_scene_object_count(_s)) {
        const u8 _no = 0u;
        rde_arr_add(&_page->play_scope, (any)&_no);
    }
    if(_page->play_circuit && _s->revision != _page->play_seen) {
        fude_zoom_circuit_build_in(&_page->circuit, _s, (const u8*)_page->play_scope.memory);   // (its state kept)
        _page->play_seen = _s->revision;
        if(_page->play_coupled) {
            fude_zoom_coupling_build(&_page->coupling, &_page->circuit, &_page->mech_run, _s);   // (its parts as they are now)
        }
    }
    // (as fast as asked; paused, a Step's 60th of a second at that speed)
    const b8 _once = !_page->play_running && _page->play_step_once;
    _page->play_step_once = false;
    if(!_page->play_running && !_once) {
        return;
    }
    const f64 _rate = _page->play_rate > 0.0 ? _page->play_rate : 1.0;
    const f64 _dt = _once ? _rate / 60.0 : _dt_real * _rate;
    if(_page->circuit_letting && _page->circuit.time >= _page->circuit_held_at + FUDE_ZOOM_PAGE_PRESS_LEAST) {
        fude_zoom_page_circuit_let_go(_page);   // (a quick tap: down long enough now)
    }
    _page->play_time += _dt;
    const f64 _sim_was = _page->circuit.time;
    b8 _ran = false, _ok = true;
    if(_page->play_coupled) {
        _ok  = fude_zoom_coupling_step(&_page->coupling, &_page->circuit, &_page->mech_run, _dt);
        _ran = true;
    } else if(_page->play_circuit && rde_arr_length(&_page->circuit.parts) > 0u) {
        _ok  = fude_zoom_circuit_run(&_page->circuit, _dt > 0.0 ? _dt : 1.0 / 60.0, _page->play_fine ? FUDE_ZOOM_PAGE_FINE_STEPS : 400u);
        _ran = true;
    }
    if(_ran) {
        // Not solved: said once (where, when it knows), until it is again.
        if(!_ok && !_page->circuit_failed) {
            fude_notice_show(fude_text(_page->circuit.suspected ? FUDE_TEXT_ZOOM_CIRCUIT_FAILED_AT : FUDE_TEXT_ZOOM_CIRCUIT_FAILED));
        }
        _page->circuit_failed = !_ok;
        fude_zoom_page_play_events(_page);
        if(!_once) {
            fude_zoom_page_play_pace(_page, _page->circuit.time - _sim_was, _dt);
        }
    }
    fude_zoom_page_sound_feed(_page);
    if(_page->play_mech && !_page->play_coupled) {
        fude_zoom_mech_world_step(&_page->mech_run, _dt);
    }
    if(_page->play_mech) {
        fude_zoom_page_play_mech_events(_page);
    }
    _page->plot_play.time = _page->play_time;
}

// How much a circuit's event matters to tell: a part burnt 3, a contact's spark 2, anything else (a source past its
// current) 1.
RDE_INTERNAL u32 fude_zoom_page_event_rank(u8 _kind) {
    return _kind == FUDE_ZOOM_CIRCUIT_BURNT ? 3u : (_kind == FUDE_ZOOM_CIRCUIT_ARC ? 2u : 1u);
}

// What happened to the circuit's parts as it stepped, told — one notice: a part burnt before a contact's spark before a
// source past its current, the latest of them. The events are cleared after.
RDE_INTERNAL void fude_zoom_page_play_events(fude_zoom_page* _page) {
    fude_zoom_circuit* _c = &_page->circuit;
    const fude_zoom_circuit_event* _e = (const fude_zoom_circuit_event*)_c->events.memory;
    const u32 _n = (u32)rde_arr_length(&_c->events);
    u32 _tell = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_tell == FUDE_ZOOM_NONE || fude_zoom_page_event_rank(_e[_i].kind) >= fude_zoom_page_event_rank(_e[_tell].kind)) {
            _tell = _i;
        }
    }
    if(_tell != FUDE_ZOOM_NONE && _e[_tell].part < (u32)rde_arr_length(&_c->parts)) {
        static const c8* const _units[FUDE_ZOOM_LIMIT_COUNT] = { "W", "A", "V", "V" };
        const c8* _unit = _units[_e[_tell].limit < FUDE_ZOOM_LIMIT_COUNT ? _e[_tell].limit : 0u];
        c8 _measure[32], _most[32], _say[256];
        fude_zoom_page_si(_e[_tell].measure, _unit, _measure, sizeof(_measure));
        fude_zoom_page_si(_e[_tell].most, _unit, _most, sizeof(_most));
        const u32 _object = ((const fude_zoom_circuit_part*)_c->parts.memory)[_e[_tell].part].object;
        if(_e[_tell].kind == FUDE_ZOOM_CIRCUIT_ARC) {
            fude_zoom_page_si(_e[_tell].measure, "V", _measure, sizeof(_measure));
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PART_ARC, FUDE_TS(fude_zoom_page_part_name(_page, _object)), FUDE_TS(_measure));
        } else {
            FUDE_TEXTF(_say, _e[_tell].kind == FUDE_ZOOM_CIRCUIT_BURNT ? FUDE_TEXT_ZOOM_PART_BURNT : FUDE_TEXT_ZOOM_SOURCE_OVER,
                       FUDE_TS(fude_zoom_page_part_name(_page, _object)), FUDE_TS(_measure), FUDE_TS(_most));
        }
        fude_notice_show(_say);
    }
    rde_arr_clear(&_c->events);
}

// What happened to the mechanism as it ran, told — one notice, the latest (a break before a jam before a misfit): a
// pin, a slide, a spring, a rope, a gear's teeth broken (what pulled it, what it takes); a motor jammed; parts drawn
// not to fit.
RDE_INTERNAL void fude_zoom_page_play_mech_events(fude_zoom_page* _page) {
    fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_mech_event* _e = (const fude_zoom_mech_event*)_w->events.memory;
    const u32 _n = (u32)rde_arr_length(&_w->events);
    u32 _tell = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_tell == FUDE_ZOOM_NONE || _e[_i].kind <= _e[_tell].kind) {
            _tell = _i;
        }
    }
    if(_tell != FUDE_ZOOM_NONE) {
        c8 _say[256];
        if(_e[_tell].kind == FUDE_ZOOM_MECH_BROKE && _e[_tell].joint < (u32)rde_arr_length(&_w->joints)) {
            static const FUDE_TEXT_ _names[5] = { FUDE_TEXT_ZOOM_MECH_PIN, FUDE_TEXT_ZOOM_MECH_SLIDE, FUDE_TEXT_ZOOM_MECH_SPRING, FUDE_TEXT_ZOOM_MECH_ROPE,
                                                  FUDE_TEXT_ZOOM_MECH_TEETH };
            const u8 _kind = ((const fude_zoom_mech_joint*)_w->joints.memory)[_e[_tell].joint].kind;
            const c8* _unit = _kind == FUDE_ZOOM_MECH_JOINT_SPRING ? "m" : "N";   // (a spring's: how far it stretched, mm in home units)
            const f64 _scale = _kind == FUDE_ZOOM_MECH_JOINT_SPRING ? 1e-3 : 1.0;
            c8 _force[32], _most[32];
            fude_zoom_page_si(_e[_tell].force * _scale, _unit, _force, sizeof(_force));
            fude_zoom_page_si(_e[_tell].strength * _scale, _unit, _most, sizeof(_most));
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_MECH_BROKE, FUDE_TS(fude_text(_names[_kind < 5u ? _kind : 0u])), FUDE_TS(_force), FUDE_TS(_most));
        } else {
            rde_snprintf(_say, sizeof(_say), "%s", fude_text(_e[_tell].kind == FUDE_ZOOM_MECH_JAMMED ? FUDE_TEXT_ZOOM_MECH_JAMMED : FUDE_TEXT_ZOOM_MECH_MISFIT));
        }
        fude_notice_show(_say);
    }
    rde_arr_clear(&_w->events);
}

// The circuit's pace: _sim of its seconds in _real of the clock's. Each half second, how many a second it keeps (its
// steps too fine for every one of them to fit in a frame: slow motion) — said once as it falls behind.
RDE_INTERNAL void fude_zoom_page_play_pace(fude_zoom_page* _page, f64 _sim, f64 _real) {
    _page->play_sim_span  += rde_math_max_f64(_sim, 0.0);
    _page->play_real_span += rde_math_max_f64(_real, 0.0);
    if(_page->play_real_span < 0.5) {
        return;
    }
    _page->play_speed = rde_math_min_f64(_page->play_sim_span / _page->play_real_span, 1.0);
    _page->play_sim_span = _page->play_real_span = 0.0;
    if(_page->play_speed < 0.95 && !_page->play_slow_told) {
        c8 _x[16], _say[96];
        rde_snprintf(_x, sizeof(_x), "%.2g", _page->play_speed);
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PLAY_SLOW, FUDE_TS(_x));
        fude_notice_show(_say);
        _page->play_slow_told = true;
    } else if(_page->play_speed >= 0.98) {
        _page->play_slow_told = false;
    }
}

// A slider's range: its variable's, or (one value) round it.
RDE_INTERNAL void fude_zoom_page_play_range(const fude_zoom_plot_var* _v, f64* _lo, f64* _hi) {
    if(_v->range) {
        *_lo = rde_math_min_f64(_v->from, _v->to);
        *_hi = rde_math_max_f64(_v->from, _v->to);
        return;
    }
    const f64 _r = rde_math_abs_f64(_v->from) <= 10.0 ? 10.0 : 2.0 * rde_math_abs_f64(_v->from);
    *_lo = -_r;
    *_hi = _r;
}

// Where the panel and its parts are (screen, centre origin, Y up): at the bottom, in the middle.
void fude_zoom_page_play_lay(const fude_zoom_page* _page, fude_zoom_page_play_layout* _l) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _pad = FUDE_ZOOM_PAGE_PLAY_PAD, _row = FUDE_ZOOM_PAGE_PLAY_ROW, _sr = FUDE_ZOOM_PAGE_PLAY_SLIDER;
    const f64 _w = rde_math_min_f64(FUDE_ZOOM_PAGE_PLAY_W, _half.x * 2.0 - 32.0);
    const u32 _rows = (_page->play_plots ? _page->play_var_count : 0u) + (_page->play_circuit ? _page->play_sensor_count : 0u);
    const f64 _h = _pad * 2.0 + _row + (f64)_rows * _sr;
    const f64 _bottom = -_half.y + 84.0;
    f64 _x0 = -_w * 0.5, _x1 = _w * 0.5;
    fude_zoom_page_clear_of_bar(_page, &_x0, &_x1, _bottom, _bottom + _h, 300.0);   // (wherever the toolbar is)
    _l->panel = (fude_zoom_box){ _x0, _bottom, _x1, _bottom + _h };
    const f64 _top = _l->panel.max_y - _pad;
    // (three buttons with their words, two small ones — its speed, Step —; the time after them)
    const f64 _sw = 50.0, _bw = rde_math_max_f64((_x1 - _x0 - _pad * 2.0 - 90.0 - 2.0 * _sw - 5.0 * 6.0) / 3.0, 40.0);
    f64 _bx = _l->panel.min_x + _pad;
    for(u32 _i = 0; _i < 5u; _i++) {
        const f64 _w = _i < 3u ? _bw : _sw;
        _l->buttons[_i] = (fude_zoom_box){ _bx, _top - _row, _bx + _w, _top };
        _bx += _w + 6.0;
    }
    _l->time = (fude_zoom_box){ _l->panel.max_x - _pad - 90.0, _top - _row, _l->panel.max_x - _pad, _top };
    for(u32 _i = 0; _i < 8u; _i++) {
        const f64 _y1 = _top - _row - (f64)_i * _sr, _y0 = _y1 - _sr;
        _l->names[_i]   = (fude_zoom_box){ _l->panel.min_x + _pad, _y0, _l->panel.min_x + _pad + 110.0, _y1 };
        _l->sliders[_i] = (fude_zoom_box){ _l->panel.min_x + _pad + 120.0, _y0, _l->panel.max_x - _pad - 8.0, _y1 };
    }
}

// How many of the panel's rows are graphs' variables: none unless graphs play.
RDE_INTERNAL u32 fude_zoom_page_play_vars_shown(const fude_zoom_page* _page) {
    return _page->play_plots ? _page->play_var_count : 0u;
}
// The panel's rows: its graphs' variables, then its circuit's sensors.
RDE_INTERNAL u32 fude_zoom_page_play_rows(const fude_zoom_page* _page) {
    return fude_zoom_page_play_vars_shown(_page) + (_page->play_circuit ? _page->play_sensor_count : 0u);
}

// Slider _i set from where the hand is on it.
void fude_zoom_page_play_slide(fude_zoom_page* _page, u32 _i, rde_vec_2F _at) {
    fude_zoom_page_play_layout _l;
    fude_zoom_page_play_lay(_page, &_l);
    const u32 _vars = fude_zoom_page_play_vars_shown(_page);
    if(_i >= _vars) {
        // A sensor's: its light (as its logarithm) or its temperature, from where along its track the hand is.
        const u32 _part = fude_zoom_page_circuit_part(_page, _page->play_sensors[(_i - _vars) % 4u]);
        if(_part != FUDE_ZOOM_NONE) {
            fude_zoom_circuit_part* _q = &((fude_zoom_circuit_part*)_page->circuit.parts.memory)[_part];
            const f64 _t = ((f64)_at.x - _l.sliders[_i].min_x) / rde_math_max_f64(_l.sliders[_i].max_x - _l.sliders[_i].min_x, 1.0);
            _q->state[0] = fude_zoom_circuit_sensor_at(_q->part->model, _t);
        }
        return;
    }
    f64 _lo, _hi;
    fude_zoom_page_play_range(&_page->play_vars[_i], &_lo, &_hi);
    const f64 _t = rde_math_min_f64(rde_math_max_f64(((f64)_at.x - _l.sliders[_i].min_x) / rde_math_max_f64(_l.sliders[_i].max_x - _l.sliders[_i].min_x, 1.0), 0.0), 1.0);
    f64 _v = _lo + (_hi - _lo) * _t;
    const f64 _step = (_hi - _lo) / 200.0;   // (to a two-hundredth of its range)
    if(_step > 0.0) {
        _v = rde_math_round_f64(_v / _step) * _step;
    }
    _page->plot_play.held[(u32)(_page->play_vars[_i].letter - 'a') % 26u] = _v;
}

// See page_parts.h. A hand down on a slider sets it at once; the buttons act when it lifts (fude_zoom_page_play_up),
// by where it went down.
b8 fude_zoom_page_play_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->play_on) {
        return false;
    }
    fude_zoom_page_play_layout _l;
    fude_zoom_page_play_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        return false;   // (the canvas goes on: the panel is no card)
    }
    _page->play_hand   = _hand;
    _page->play_down   = _at;
    _page->play_slider = -1;
    for(u32 _i = 0; _i < fude_zoom_page_play_rows(_page); _i++) {
        if(fude_zoom_page_box_holds(_l.sliders[_i], _at)) {
            _page->play_slider = (i32)_i;
            fude_zoom_page_play_slide(_page, _i, _at);
        }
    }
    return true;
}

// See page_parts.h.
b8 fude_zoom_page_play_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->play_on || _page->play_hand != _hand || _hand == 0u) {
        return false;
    }
    if(_page->play_slider >= 0) {
        fude_zoom_page_play_slide(_page, (u32)_page->play_slider, _at);
    }
    return true;
}

// See page_parts.h. The buttons, by where the hand went down: Pause or Go on, Back to the start (built again, and
// paused or running as it was), Stop, the speed, Step; a variable's name tapped lets its value go.
b8 fude_zoom_page_play_up(fude_zoom_page* _page, u64 _hand) {
    if(!_page->play_on || _page->play_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->play_hand = 0u;
    if(_page->play_slider >= 0) {
        _page->play_slider = -1;
        return true;
    }
    fude_zoom_page_play_layout _l;
    fude_zoom_page_play_lay(_page, &_l);
    const rde_vec_2F _at = _page->play_down;
    if(fude_zoom_page_box_holds(_l.buttons[0], _at)) {
        fude_zoom_page_play_pause(_page, _page->play_running);
    } else if(fude_zoom_page_box_holds(_l.buttons[1], _at)) {
        const b8 _was = _page->play_running;
        fude_zoom_page_play_build(_page);   // (back to the start: built again from what it plays)
        fude_zoom_page_play_pause(_page, !_was);
    } else if(fude_zoom_page_box_holds(_l.buttons[2], _at)) {
        fude_zoom_page_play_stop(_page);
    } else if(fude_zoom_page_box_holds(_l.buttons[3], _at)) {
        // Its speed: 1, ½, ¼, a tenth, round again.
        _page->play_rate = _page->play_rate > 0.75 ? 0.5 : (_page->play_rate > 0.375 ? 0.25 : (_page->play_rate > 0.175 ? 0.1 : 1.0));
        _page->play_sim_span = _page->play_real_span = 0.0;
    } else if(fude_zoom_page_box_holds(_l.buttons[4], _at)) {
        // Step: paused (if it was not), then a 60th of a second on at its speed.
        fude_zoom_page_play_pause(_page, true);
        _page->play_step_once = true;
    } else {
        for(u32 _i = 0; _i < _page->play_var_count; _i++) {
            if(fude_zoom_page_box_holds(_l.names[_i], _at)) {
                _page->plot_play.held[(u32)(_page->play_vars[_i].letter - 'a') % 26u] = RDE_NAN_F64;   // (its name tapped: let go, it moves again)
            }
        }
    }
    if(_page->app != NULL && _page->app->ui != NULL) {
        fude_toolbar_refresh(&_page->app->ui->bar);
    }
    return true;
}

// The panel: its buttons (each its sign and its word), its time, its sliders.
void fude_zoom_page_render_play(fude_zoom_page* _page) {
    if(!_page->play_on || _page->presenting) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_zoom_page_play_layout _l;
    fude_zoom_page_play_lay(_page, &_l);
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                         (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.25f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    const u32 _words[3] = { _page->play_running ? FUDE_TEXT_ZOOM_PLAY_PAUSE : FUDE_TEXT_ZOOM_PLAY_GO, FUDE_TEXT_ZOOM_PLAY_BACK, FUDE_TEXT_ZOOM_PLAY_STOP };
    for(u32 _i = 0; _i < 3u; _i++) {
        const fude_zoom_box _b = _l.buttons[_i];
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) }, 0.45f, 6u,
                                                             _i == 0u ? _t->accent : _t->surface_2, 1.0f, _t->outline, NULL);
        const rde_color _ink = _i == 0u ? fude_kit_text_on(_t->accent) : _t->text;
        // (its word where it fits whole beside its sign — else its sign alone, in its middle: never a word cut)
        const c8* _word = fude_text(_words[_i]);
        const b8 _worded = _page->renderer.font != NULL &&
                           fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _word, 14.0f) <= (f32)(_b.max_x - _b.min_x - 40.0);
        // (its sign: pause's two bars, go on's triangle; back's arrow round; stop's square)
        const f32 _sx = _worded ? (f32)_b.min_x + 20.0f : _c.x, _sy = _c.y, _sz = 7.0f;
        if(_i == 0u && _page->play_running) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _sx - 3.5f, _sy }, (rde_vec_2F){ 4.0f, _sz * 2.0f }, _ink);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _sx + 3.5f, _sy }, (rde_vec_2F){ 4.0f, _sz * 2.0f }, _ink);
        } else if(_i == 0u) {
            const rde_vec_2F _tri[3] = { { _sx - 5.0f, _sy - _sz }, { _sx + 7.0f, _sy }, { _sx - 5.0f, _sy + _sz } };
            rde_rendering_2d_draw_polygon(_tri, 3u, _ink, NULL);
        } else if(_i == 1u) {
            rde_vec_2F _arc[14];
            f32 _rr[14];
            for(u32 _k = 0; _k < 14u; _k++) {
                const f32 _a = 0.6f + 4.6f * (f32)_k / 13.0f;
                _arc[_k] = (rde_vec_2F){ _sx + rde_math_cos_f32(_a) * _sz, _sy + rde_math_sin_f32(_a) * _sz };
                _rr[_k]  = 1.0f;
            }
            rde_rendering_2d_draw_stroke(_arc, _rr, 14u, _ink);
            const rde_vec_2F _head[3] = { { _sx + rde_math_cos_f32(0.6f) * _sz - 3.5f, _sy + rde_math_sin_f32(0.6f) * _sz + 2.5f }, { _sx + rde_math_cos_f32(0.6f) * _sz + 3.5f, _sy + rde_math_sin_f32(0.6f) * _sz + 3.5f },
                                          { _sx + rde_math_cos_f32(0.6f) * _sz + 1.0f, _sy + rde_math_sin_f32(0.6f) * _sz - 3.5f } };
            rde_rendering_2d_draw_polygon(_head, 3u, _ink, NULL);
        } else {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _sx, _sy }, (rde_vec_2F){ _sz * 1.8f, _sz * 1.8f }, _ink);
        }
        if(_worded) {
            fude_zoom_render_text_in(&_page->renderer, _word, (u32)rde_strlen(_word), (fude_zoom_box){ _b.min_x + 34.0, _b.min_y + 4.0, _b.max_x - 6.0, _b.max_y - 4.0 }, 14.0, false, _ink);
        }
    }
    // Its speed (×1, ×½, ×¼, ×0.1: lit when slowed) and Step (a triangle against a bar).
    {
        const fude_zoom_box _b = _l.buttons[3];
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        const b8 _slowed = _page->play_rate < 0.99;
        rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) }, 0.45f, 6u,
                                                             _slowed ? _t->accent : _t->surface_2, 1.0f, _t->outline, NULL);
        const c8* _r = _page->play_rate > 0.75 ? "\xC3\x97" "1" : (_page->play_rate > 0.375 ? "\xC3\x97\xC2\xBD" : (_page->play_rate > 0.175 ? "\xC3\x97\xC2\xBC" : "\xC3\x97" "0.1"));
        fude_zoom_render_text_in(&_page->renderer, _r, (u32)rde_strlen(_r), (fude_zoom_box){ _b.min_x - 10.0, _b.min_y + 4.0, _b.max_x + 10.0, _b.max_y - 4.0 }, 15.0, false,
                                 _slowed ? fude_kit_text_on(_t->accent) : _t->text);
        const fude_zoom_box _s = _l.buttons[4];
        const rde_vec_2F _sc = { (f32)((_s.min_x + _s.max_x) * 0.5), (f32)((_s.min_y + _s.max_y) * 0.5) };
        rde_rendering_2d_draw_rounded_rectangle_with_border(_sc, (rde_vec_2F){ (f32)(_s.max_x - _s.min_x), (f32)(_s.max_y - _s.min_y) }, 0.45f, 6u, _t->surface_2, 1.0f,
                                                             _t->outline, NULL);
        const rde_vec_2F _tri[3] = { { _sc.x - 7.0f, _sc.y - 7.0f }, { _sc.x + 4.0f, _sc.y }, { _sc.x - 7.0f, _sc.y + 7.0f } };
        rde_rendering_2d_draw_polygon(_tri, 3u, _t->text, NULL);
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _sc.x + 7.0f, _sc.y }, (rde_vec_2F){ 3.0f, 14.0f }, _t->text);
    }
    // Its time; in slow motion, how slow (the circuit's seconds a second).
    c8 _time[32];
    const b8 _slow = _page->play_speed < 0.95 && _page->play_circuit;
    if(_slow) {
        rde_snprintf(_time, sizeof(_time), "%.1f s \xC3\x97%.2g", _page->play_time, _page->play_speed);
    } else {
        rde_snprintf(_time, sizeof(_time), "%.1f s", _page->play_time);
    }
    fude_zoom_render_text_in(&_page->renderer, _time, (u32)rde_strlen(_time), _l.time, _slow ? 13.0 : 15.0, false, _slow ? _t->accent : _t->text_soft);
    for(u32 _i = 0; _i < _page->play_var_count && _page->play_plots; _i++) {
        const fude_zoom_plot_var* _v = &_page->play_vars[_i];
        const f64 _now = fude_zoom_plot_var_value(_v, &_page->plot_play);
        const b8 _held = !rde_math_is_nan_f64(_page->plot_play.held[(u32)(_v->letter - 'a') % 26u]);
        c8 _say[40];
        rde_snprintf(_say, sizeof(_say), "%c = %.3g", _v->letter, rde_math_abs_f64(_now) < 1e-12 ? 0.0 : _now);
        fude_zoom_render_text_in(&_page->renderer, _say, (u32)rde_strlen(_say), _l.names[_i], 14.0, false, _held ? _t->accent : _t->text);
        f64 _lo, _hi;
        fude_zoom_page_play_range(_v, &_lo, &_hi);
        const fude_zoom_box _s = _l.sliders[_i];
        const f32 _y = (f32)((_s.min_y + _s.max_y) * 0.5);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_s.min_x, _y }, (rde_vec_2F){ (f32)_s.max_x, _y }, _t->outline, 3.0f);
        const f64 _k = _hi > _lo ? rde_math_min_f64(rde_math_max_f64((_now - _lo) / (_hi - _lo), 0.0), 1.0) : 0.5;
        const f32 _x = (f32)(_s.min_x + (_s.max_x - _s.min_x) * _k);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_s.min_x, _y }, (rde_vec_2F){ _x, _y }, _t->accent, 3.0f);
        rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ _x, _y }, 9.0f, 20u, _t->surface, 2.0f, _t->accent, NULL);
    }
    // The circuit's sensors: each one's light or temperature, its slider.
    const u32 _vars = fude_zoom_page_play_vars_shown(_page);
    for(u32 _k = 0; _page->play_circuit && _k < _page->play_sensor_count; _k++) {
        const u32 _part = fude_zoom_page_circuit_part(_page, _page->play_sensors[_k]);
        if(_part == FUDE_ZOOM_NONE) {
            continue;
        }
        const fude_zoom_circuit_part* _q = &((const fude_zoom_circuit_part*)_page->circuit.parts.memory)[_part];
        c8 _say[40];
        if(fude_zoom_circuit_lit(_q->part->model)) {
            c8 _lux[24];
            rde_snprintf(_lux, sizeof(_lux), "%.0f", _q->state[0]);
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LUX, FUDE_TS(_lux));   // (its light, "lux" in the person's language)
        } else {
            rde_snprintf(_say, sizeof(_say), "%.0f \xC2\xB0" "C", _q->state[0]);
        }
        fude_zoom_render_text_in(&_page->renderer, _say, (u32)rde_strlen(_say), _l.names[_vars + _k], 14.0, false, _t->text);
        const fude_zoom_box _s = _l.sliders[_vars + _k];
        const f32 _y = (f32)((_s.min_y + _s.max_y) * 0.5);
        const f32 _x = (f32)(_s.min_x + (_s.max_x - _s.min_x) * fude_zoom_circuit_sensor_along(_q->part->model, _q->state[0]));
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_s.min_x, _y }, (rde_vec_2F){ (f32)_s.max_x, _y }, _t->outline, 3.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_s.min_x, _y }, (rde_vec_2F){ _x, _y }, _t->accent, 3.0f);
        rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ _x, _y }, 9.0f, 20u, _t->surface, 2.0f, _t->accent, NULL);
    }
}

// How the mechanism's joints stand to their strength, over it (mechrun.h): a joint pulled past six tenths of what it
// takes, a ring warming to red as it nears it; a broken one, a red cross where it was (on what it held); a pin whose
// parts cannot fit, a motor jammed, a pulsing red ring.
RDE_INTERNAL void fude_zoom_page_render_strain(fude_zoom_page* _page, const fude_zoom_mech_world* _w, fude_zoom_sim _hs) {
    const fude_zoom_mech_joint* _j = (const fude_zoom_mech_joint*)_w->joints.memory;
    // The pulse runs on Play's time, so a paused mechanism stands still (a still screen: drawing/base/redraw.h).
    const f32 _pulse = 0.5f + 0.5f * rde_math_sin_f32((f32)_page->play_time * 6.0f);
    const f32 _r = 11.0f;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->joints); _i++) {
        // (where it is now: on its first part as that has moved; a mesh's at its first gear's middle)
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_hs, _j[_i].joint != NULL || _j[_i].kind != FUDE_ZOOM_MECH_JOINT_PIN ?
                                                         fude_zoom_mech_world_point(_w, _j[_i].a, _j[_i].at) : _j[_i].at);
        const rde_vec_2F _c = { (f32)_p.x, (f32)_p.y };
        if(_j[_i].joint == NULL) {
            const rde_color _red = { 210, 40, 30, 235 };
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - _r, _c.y - _r }, (rde_vec_2F){ _c.x + _r, _c.y + _r }, _red, 3.0f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - _r, _c.y + _r }, (rde_vec_2F){ _c.x + _r, _c.y - _r }, _red, 3.0f);
            continue;
        }
        if(_j[_i].apart < 0.0) {
            rde_rendering_2d_draw_circle_with_border(_c, _r * 1.4f, 32u, (rde_color){ 0, 0, 0, 0 }, 2.5f, (rde_color){ 230, 40, 40, (u8)(150.0f + 100.0f * _pulse) }, NULL);
        }
        if(_j[_i].load > 0.6) {
            const f32 _hot = rde_math_min_f32((f32)((_j[_i].load - 0.6) / 0.4), 1.0f);
            for(u32 _ring = 0; _ring < 3u; _ring++) {
                const rde_color _g = { 255, (u8)(170.0f - 130.0f * _hot), 30, (u8)((20.0f + 25.0f * (f32)_ring) * (0.45f + 0.55f * _hot)) };
                rde_rendering_2d_draw_circle(_c, _r * (1.6f - 0.25f * (f32)_ring), 24u, _g, NULL);
            }
        }
    }
    const fude_zoom_mech_motor* _mo = (const fude_zoom_mech_motor*)_w->motors.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->motors); _k++) {
        if(_mo[_k].jammed && _mo[_k].pin < (u32)rde_arr_length(&_w->joints)) {
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_hs, _j[_mo[_k].pin].at);
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, _r * 2.2f, 40u, (rde_color){ 0, 0, 0, 0 }, 3.0f,
                                                     (rde_color){ 230, 40, 40, (u8)(150.0f + 100.0f * _pulse) }, NULL);
        }
    }
}

// A tracer's path's half width (screen points): half the 2.5 its segments were drawn at as lines, so the stroke's ink
// is as wide (its edge now smoothed, as the pen's lines are, and its joins round).
#define FUDE_ZOOM_PAGE_TRAIL_RADIUS 1.25f

// The mechanism's tracers (mechrun.h), over its moving parts: the path each has drawn, in the accent's colour, its pen
// where it is now. Each path is read where the world keeps it (not copied) and drawn as one stroke for each stretch of
// it near the screen (mechrun.h's fude_zoom_mech_trail_runs), into the page's scratch (page.h's mech_trail and the
// others): no allocation, and a call for each stretch rather than one for each of up to FUDE_ZOOM_MECH_TRAIL_MOST
// segments.
void fude_zoom_page_render_trails(fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(!_page->play_on || !_page->play_mech || !fude_zoom_mech_world_on(&_page->mech_run)) {
        return;
    }
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_mech_plan* _plan = &_w->plan;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const fude_zoom_sim _hs = fude_zoom_page_frame_sim(_page, _home);
    const f64 _hk = fude_zoom_sim_scale(_hs);
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_plan->bodies.memory;
    {
        const fude_zoom_mech_tracer* _tr = (const fude_zoom_mech_tracer*)_plan->tracers.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_plan->tracers); _i++) {
            const rde_color _ink = fude_theme_active()->accent;
            u32 _n = 0;
            const fude_zoom_v2* _path = fude_zoom_mech_world_trail(_w, _i, &_n);
            const u32 _runs = fude_zoom_mech_trail_runs(_path, _n, _hs, _half, FUDE_ZOOM_PAGE_TRAIL_RADIUS, &_page->mech_trail, &_page->mech_trail_runs);
            // (its half widths: as many as the most points drawn yet, grown when a path has more, never written again)
            const u32 _had = (u32)rde_arr_length(&_page->mech_trail_radii), _need = (u32)rde_arr_length(&_page->mech_trail);
            if(_had < _need) {
                f32* _more = (f32*)rde_arr_add_n(&_page->mech_trail_radii, _need - _had);
                for(u32 _k = 0; _k < _need - _had; _k++) {
                    _more[_k] = FUDE_ZOOM_PAGE_TRAIL_RADIUS;
                }
            }
            const rde_vec_2F* _q = (const rde_vec_2F*)_page->mech_trail.memory;
            const u32* _run = (const u32*)_page->mech_trail_runs.memory;
            for(u32 _k = 0; _k < _runs; _k++) {
                rde_rendering_2d_draw_stroke(&_q[_run[2u * _k]], (const f32*)_page->mech_trail_radii.memory, _run[2u * _k + 1u], _ink);
            }
            const fude_zoom_v2 _now = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _tr[_i].on, _tr[_i].at));
            const f32 _r = (f32)rde_math_max_f64(rde_math_min_f64(_b[_tr[_i].body].hw, _b[_tr[_i].body].hh) * _hk * 0.85, 4.0);
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_now.x, (f32)_now.y }, _r, 20u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _ink, NULL);
            rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_now.x, (f32)_now.y }, rde_math_max_f32(_r * 0.25f, 2.0f), 12u, _ink, NULL);
        }
    }
    // A part held: a line from where it is held to the hand.
    fude_zoom_v2 _held;
    if(_page->mech_hold == 1u && fude_zoom_mech_world_grabbed(_w, &_held)) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_hs, _held), _c = fude_zoom_sim_apply(_hs, _w->grab_to);
        const rde_color _ink = fude_theme_active()->accent;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _ink, 2.0f);
        rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, 4.0f, 12u, _ink, NULL);
        rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_c.x, (f32)_c.y }, 9.0f, 20u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _ink, NULL);
    }
}

// Returns object _object's value in play_moved, the array made the first time (*_any false: zeroed, a value an
// object, and *_any set); NULL for an object past the scene's.
RDE_INTERNAL f32* fude_zoom_page_moved_at(fude_zoom_page* _page, u32 _object, b8* _any) {
    const u32 _n = fude_zoom_scene_object_count(&_page->scene);
    if(_object >= _n) {
        return NULL;
    }
    if(!*_any) {
        rde_arr_clear(&_page->play_moved);
        rde_memset(rde_arr_add_n(&_page->play_moved, _n), 0, sizeof(f32) * _n);
        *_any = true;
    }
    return &((f32*)_page->play_moved.memory)[_object];
}

// See page_parts.h. What moves in its symbols as it plays (render.h's moved: own units along each one's width): each
// solenoid's plunger as far in as it is (its stroke's share of its half width, back into its coil); each follower as
// far along its guide as it has gone; each worm's threads as far round as it has turned its gear (a lead a tooth of
// it, within a lead). NULL: nothing.
const rde_arr* fude_zoom_page_moving_parts(fude_zoom_page* _page) {
    if(!_page->play_on) {
        return NULL;
    }
    b8 _any = false;
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
    for(u32 _i = 0; _page->play_circuit && _i < (u32)rde_arr_length(&_page->circuit.parts); _i++) {
        if(_p[_i].part->model != FUDE_ZOOM_MODEL_SOLENOID) {
            continue;
        }
        f32* _at = fude_zoom_page_moved_at(_page, _p[_i].object, &_any);
        if(_at != NULL) {
            f64 _sn[3] = { 0.0, 0.0, 0.0 };
            fude_zoom_scene_shape_numbers(&_page->scene, _p[_i].object, _sn, 3u);
            *_at = (f32)(-FUDE_ZOOM_SOLENOID_STROKE * _sn[1] * rde_math_min_f64(rde_math_max_f64(_p[_i].state[2], 0.0), 1.0));
        }
    }
    if(_page->play_mech && fude_zoom_mech_world_on(&_page->mech_run)) {
        const fude_zoom_mech_world* _w = &_page->mech_run;
        const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.bodies); _i++) {
            if(_b[_i].part->kind == FUDE_ZOOM_MECH_FOLLOWER) {
                f32* _at = fude_zoom_page_moved_at(_page, _b[_i].object, &_any);
                if(_at != NULL) {
                    *_at = (f32)(fude_zoom_mech_world_slid(_w, _i) / rde_math_max_f64(_b[_i].scale, 1e-12));
                }
            }
        }
        const fude_zoom_mech_worm* _wm = (const fude_zoom_mech_worm*)_w->plan.worms.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.worms); _i++) {
            f32* _at = fude_zoom_page_moved_at(_page, _b[_wm[_i].body].object, &_any);
            if(_at != NULL) {
                // (a tooth of its gear a turn of the worm, a lead along its threads)
                const f64 _teeth = (f64)_b[_wm[_i].gear].part->teeth;
                const f64 _turns = _wm[_i].side * fude_zoom_mech_world_turned(_w, _wm[_i].gear) * _teeth / (2.0 * RDE_PI);
                *_at = (f32)(FUDE_ZOOM_MECH_WORM_LEAD * (_turns - rde_math_floor_f64(_turns)));
            }
        }
    }
    return _any ? &_page->play_moved : NULL;
}

// A turning part's axle (home units, as drawn): its first hole.
RDE_INTERNAL fude_zoom_v2 fude_zoom_page_axle(const fude_zoom_mech_body* _b) {
    const f64 _x = (f64)_b->part->holes[0][0] * _b->hw, _y = (f64)_b->part->holes[0][1] * _b->hh;
    return (fude_zoom_v2){ _b->at.x + _x * rde_math_cos_f64(_b->angle) - _y * rde_math_sin_f64(_b->angle), _b->at.y + _x * rde_math_sin_f64(_b->angle) + _y * rde_math_cos_f64(_b->angle) };
}

// A belt's (or a chain's) loop round circles _c1 (_r1 round) and _c2 (_r2) (home units): round the first's back, the
// way it turns counter-clockwise; across to the second; round its front — or its back, crossed —; back. Its points into
// _out (home units, closed: the last back to the first). False: no loop (one inside the other, or the two too near
// crossed).
RDE_INTERNAL b8 fude_zoom_page_belt_loop(fude_zoom_v2 _c1, f64 _r1, fude_zoom_v2 _c2, f64 _r2, b8 _crossed, rde_arr* _out) {
    rde_arr_clear(_out);
    const f64 _d = rde_math_hypot_f64(_c2.x - _c1.x, _c2.y - _c1.y), _phi = rde_math_atan2_f64(_c2.y - _c1.y, _c2.x - _c1.x);
    if(!(_d > rde_math_abs_f64(_r1 - _r2)) || (_crossed && !(_d > _r1 + _r2))) {
        return false;
    }
    // (open: tangents at φ ± (π/2 + β) on both; crossed: at φ ± α on the first, opposite on the second)
    f64 _a0, _a1, _b0, _b1;
    if(_crossed) {
        const f64 _al = rde_math_acos_f64((_r1 + _r2) / _d);
        _a0 = _phi + _al; _a1 = _phi - _al + 2.0 * RDE_PI;      // (the first's back, counter-clockwise)
        _b0 = _phi - _al + RDE_PI; _b1 = _phi + _al - RDE_PI;      // (the second's back, clockwise)
    } else {
        const f64 _be = rde_math_asin_f64((_r1 - _r2) / _d);
        _a0 = _phi + 0.5 * RDE_PI + _be; _a1 = _phi + 1.5 * RDE_PI - _be;   // (the first's back)
        _b0 = _phi - 0.5 * RDE_PI - _be; _b1 = _phi + 0.5 * RDE_PI + _be;   // (the second's front)
    }
    for(u32 _k = 0; _k <= 32u; _k++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_k / 32.0;
        const fude_zoom_v2 _q = { _c1.x + rde_math_cos_f64(_a) * _r1, _c1.y + rde_math_sin_f64(_a) * _r1 };
        rde_arr_add(_out, (any)&_q);
    }
    for(u32 _k = 0; _k <= 32u; _k++) {
        const f64 _a = _b0 + (_b1 - _b0) * (f64)_k / 32.0;
        const fude_zoom_v2 _q = { _c2.x + rde_math_cos_f64(_a) * _r2, _c2.y + rde_math_sin_f64(_a) * _r2 };
        rde_arr_add(_out, (any)&_q);
    }
    return true;
}

// The most marks one belt or chain draws: a bound on a frame's drawing (fude_zoom_page_dots_along hands it only those
// on the screen). A belt so thin that more than this crowd the screen leaves the rest out: they
// would be a smear by then.
#define FUDE_ZOOM_PAGE_BELT_MARKS_MOST 8192u

// How a belt's or a chain's marks look: a chain's rollers or a belt's ticks, as wide as the belt says, in its ink.
typedef struct {
    b8        chain;
    f32       wide;
    rde_color ink;
} fude_zoom_page_belt_mark;

// One mark of a belt or a chain at _at on the screen (fude_zoom_page_dots_along's _dot; _user its look).
RDE_INTERNAL void fude_zoom_page_belt_mark_at(any _user, rde_vec_2F _at) {
    const fude_zoom_page_belt_mark* _m = (const fude_zoom_page_belt_mark*)_user;
    if(_m->chain) {
        rde_rendering_2d_draw_circle_with_border(_at, _m->wide * 1.3f, 10u, fude_theme_active()->page, 1.2f, _m->ink, NULL);
    } else {
        rde_rendering_2d_draw_circle(_at, _m->wide * 0.9f, 8u, fude_theme_active()->page, NULL);
    }
}

// The plan's belts and chains as they run: each loop round its wheels, marks along it as far on as its first wheel's
// rim has gone (a chain's: its rollers). Each loop is worked out into the page's scratch (page.h's mech_belt_loop).
RDE_INTERNAL void fude_zoom_page_render_belts(fude_zoom_page* _page, fude_zoom_sim _hs, fude_zoom_v2 _half) {
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    const fude_zoom_mech_belt* _bt = (const fude_zoom_mech_belt*)_w->plan.belts.memory;
    const f64 _hk = fude_zoom_sim_scale(_hs);
    rde_arr* _loop = &_page->mech_belt_loop;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.belts); _i++) {
        if(_b[_bt[_i].body].object >= fude_zoom_scene_object_count(_s)) {
            continue;
        }
        const fude_zoom_v2 _c1 = fude_zoom_mech_world_point(_w, FUDE_ZOOM_NONE, fude_zoom_page_axle(&_b[_bt[_i].a]));
        const fude_zoom_v2 _c2 = fude_zoom_mech_world_point(_w, FUDE_ZOOM_NONE, fude_zoom_page_axle(&_b[_bt[_i].b]));
        if(!fude_zoom_page_belt_loop(_c1, _bt[_i].ra, _c2, _bt[_i].rb, _bt[_i].crossed, _loop)) {
            continue;
        }
        const rde_color _ink = fude_theme_resolve(fude_zoom_scene_object(_s, _b[_bt[_i].body].object)->color);
        const fude_zoom_v2* _q = (const fude_zoom_v2*)_loop->memory;
        const u32 _n = (u32)rde_arr_length(_loop);
        const f32 _wide = (f32)rde_math_max_f64(_b[_bt[_i].body].hh * 0.25 * _hk, 1.5);
        for(u32 _k = 0; _k < _n; _k++) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_hs, _q[_k]), _c = fude_zoom_sim_apply(_hs, _q[(_k + 1u) % _n]);
            if((rde_math_abs_f64(_a.x) > _half.x && rde_math_abs_f64(_c.x) > _half.x) || (rde_math_abs_f64(_a.y) > _half.y && rde_math_abs_f64(_c.y) > _half.y)) {
                continue;
            }
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _ink, _wide);
        }
        // (its marks: a chain's rollers, a belt's ticks, as far along as its first wheel's rim has gone)
        f64 _total = 0.0;
        for(u32 _k = 0; _k < _n; _k++) {
            _total += rde_math_hypot_f64(_q[(_k + 1u) % _n].x - _q[_k].x, _q[(_k + 1u) % _n].y - _q[_k].y);
        }
        const f64 _gap = rde_math_max_f64(_b[_bt[_i].body].hh * (_bt[_i].chain ? 0.6 : 1.6), 1e-6);
        f64 _off = rde_math_fmod_f64(fude_zoom_mech_world_turned(_w, _bt[_i].a) * _bt[_i].ra, _gap);
        _off = _off < 0.0 ? _off + _gap : _off;
        // (only those on the screen, counted, not walked: fude_zoom_page_dots_along)
        const fude_zoom_page_belt_mark _mark = { _bt[_i].chain, _wide, _ink };
        fude_zoom_page_dots_along(_q, _n, true, 0.0, _total, _off, _gap, _hs, _half, FUDE_ZOOM_PAGE_BELT_MARKS_MOST, fude_zoom_page_belt_mark_at, (any)&_mark);
    }
}

// The plan's dampers as they go: a cylinder from one end (as long as drawn), the rod from the other into it to its
// piston.
RDE_INTERNAL void fude_zoom_page_render_dampers(fude_zoom_page* _page, fude_zoom_sim _hs) {
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    const fude_zoom_mech_damper* _dm = (const fude_zoom_mech_damper*)_w->plan.dampers.memory;
    const f64 _hk = fude_zoom_sim_scale(_hs);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.dampers); _i++) {
        if(_b[_dm[_i].body].object >= fude_zoom_scene_object_count(_s)) {
            continue;
        }
        const rde_color _ink = fude_theme_resolve(fude_zoom_scene_object(_s, _b[_dm[_i].body].object)->color);
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _dm[_i].a, _dm[_i].pa));
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _dm[_i].b, _dm[_i].pb));
        const f64 _len = rde_math_hypot_f64(_c.x - _a.x, _c.y - _a.y), _drawn = rde_math_hypot_f64(_dm[_i].pb.x - _dm[_i].pa.x, _dm[_i].pb.y - _dm[_i].pa.y) * _hk;
        if(!(_len > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _u = { (_c.x - _a.x) / _len, (_c.y - _a.y) / _len }, _n = { -_u.y, _u.x };
        const f64 _half_w = _b[_dm[_i].body].hh * 0.7 * _hk;
        // (its cylinder as drawn from its first end — no further than most of the way, squeezed —; its rod as long as
        // drawn from its other end, its piston inside the cylinder)
        const f64 _c0 = rde_math_min_f64(0.083 * _drawn, 0.1 * _len), _c1 = rde_math_min_f64(0.5 * _drawn, 0.75 * _len);
        const f64 _piston = rde_math_min_f64(rde_math_max_f64(_len - 0.583 * _drawn, _c0 + 1.0), _c1 - 1.0);
        const rde_vec_2F _p0 = { (f32)(_a.x + _u.x * _c0), (f32)(_a.y + _u.y * _c0) }, _p1 = { (f32)(_a.x + _u.x * _c1), (f32)(_a.y + _u.y * _c1) };
        const rde_vec_2F _corner[4] = {
            { _p0.x + (f32)(_n.x * _half_w), _p0.y + (f32)(_n.y * _half_w) }, { _p1.x + (f32)(_n.x * _half_w), _p1.y + (f32)(_n.y * _half_w) },
            { _p1.x - (f32)(_n.x * _half_w), _p1.y - (f32)(_n.y * _half_w) }, { _p0.x - (f32)(_n.x * _half_w), _p0.y - (f32)(_n.y * _half_w) } };
        for(u32 _k = 0; _k < 4u; _k++) {
            rde_rendering_2d_draw_line_1(_corner[_k], _corner[(_k + 1u) % 4u], _ink, 1.6f);
        }
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, _p0, _ink, 1.6f);
        const rde_vec_2F _pc = { (f32)(_a.x + _u.x * _piston), (f32)(_a.y + _u.y * _piston) };
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _pc.x + (f32)(_n.x * _half_w * 0.8), _pc.y + (f32)(_n.y * _half_w * 0.8) },
                                     (rde_vec_2F){ _pc.x - (f32)(_n.x * _half_w * 0.8), _pc.y - (f32)(_n.y * _half_w * 0.8) }, _ink, 2.0f);
        rde_rendering_2d_draw_line_1(_pc, (rde_vec_2F){ (f32)_c.x, (f32)_c.y }, _ink, 1.6f);
    }
}

// See page_parts.h. Drawn in this order: how the joints stand to their strength, springs and ropes between their ends
// now (a pulley's rope over it), dampers, then the parts that move where they have moved to (the world's: mechrun.h)
// and pulleys, and belts and chains over their wheels.
void fude_zoom_page_render_mech(fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(!_page->play_on || !_page->play_mech || !fude_zoom_mech_world_on(&_page->mech_run)) {
        return;
    }
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_mech_world* _w = &_page->mech_run;
    const fude_zoom_mech_plan* _plan = &_w->plan;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const fude_zoom_sim _hs = fude_zoom_page_frame_sim(_page, _home);
    const f64 _hk = fude_zoom_sim_scale(_hs);
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_plan->bodies.memory;
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_plan->bodies);
    fude_zoom_page_render_strain(_page, _w, _hs);
    // Springs: zigzags between their ends where what they hold is now.
    const fude_zoom_mech_spring* _sp = (const fude_zoom_mech_spring*)_plan->springs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_plan->springs); _i++) {
        if(fude_zoom_mech_world_broken(_w, FUDE_ZOOM_MECH_JOINT_SPRING, _i)) {
            continue;   // (given: gone)
        }
        // (its own symbol, as the plan keeps it: its colour and its width)
        const u32 _own = _sp[_i].body;
        if(_own >= _nb || _b[_own].object >= fude_zoom_scene_object_count(_s)) {
            continue;
        }
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _sp[_i].a, _sp[_i].pa));
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _sp[_i].b, _sp[_i].pb));
        const f64 _len = rde_math_hypot_f64(_c.x - _a.x, _c.y - _a.y);
        if(!(_len > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _u = { (_c.x - _a.x) / _len, (_c.y - _a.y) / _len }, _n = { -_u.y, _u.x };
        const f64 _amp = _b[_own].hh * _hk * 0.7;
        rde_vec_2F _pts[20];
        f32 _rr[20];
        u32 _m = 0;
        _pts[_m++] = (rde_vec_2F){ (f32)_a.x, (f32)_a.y };
        for(u32 _z = 0; _z < 16u; _z++) {
            const f64 _tt = 0.1 + 0.8 * ((f64)_z + 0.5) / 16.0;
            const f64 _o = (_z % 2u == 0u ? _amp : -_amp);
            _pts[_m++] = (rde_vec_2F){ (f32)(_a.x + _u.x * _len * _tt + _n.x * _o), (f32)(_a.y + _u.y * _len * _tt + _n.y * _o) };
        }
        _pts[_m++] = (rde_vec_2F){ (f32)_c.x, (f32)_c.y };
        for(u32 _z = 0; _z < _m; _z++) {
            _rr[_z] = 1.2f;
        }
        rde_rendering_2d_draw_stroke(_pts, _rr, _m, fude_theme_resolve(fude_zoom_scene_object(_s, _b[_own].object)->color));
    }
    // Ropes: straight when taut, hanging when slack; a pair's over its pulley's groove.
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_plan->ropes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_plan->ropes); _i++) {
        if(_b[_r[_i].body].object >= fude_zoom_scene_object_count(_s) || fude_zoom_mech_world_broken(_w, FUDE_ZOOM_MECH_JOINT_ROPE, _i) ||
           (_r[_i].pair != FUDE_ZOOM_NONE && fude_zoom_mech_world_broken(_w, FUDE_ZOOM_MECH_JOINT_ROPE, _r[_i].pair))) {
            continue;   // (snapped: gone)
        }
        const rde_color _ink = fude_theme_resolve(fude_zoom_scene_object(_s, _b[_r[_i].body].object)->color);
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _r[_i].a, _r[_i].pa));
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_hs, fude_zoom_mech_world_point(_w, _r[_i].b, _r[_i].pb));
        f64 _len = _r[_i].length * _hk;
        if(_r[_i].pair != FUDE_ZOOM_NONE) {
            // (a pair's slack shared: what its other side has not taken)
            const fude_zoom_mech_rope* _q = &_r[_r[_i].pair];
            const fude_zoom_v2 _qa = fude_zoom_mech_world_point(_w, _q->a, _q->pa);
            _len = (_r[_i].length + _q->length) * _hk - rde_math_hypot_f64(_qa.x - _q->pb.x, _qa.y - _q->pb.y) * _hk;
        }
        const f64 _d = rde_math_hypot_f64(_c.x - _a.x, _c.y - _a.y);
        const f64 _sag = _len > _d ? 0.5 * rde_math_sqrt_f64(_len * _len - _d * _d) : 0.0;
        rde_vec_2F _pts[17];
        f32 _rr[17];
        for(u32 _z = 0; _z <= 16u; _z++) {
            const f64 _tt = (f64)_z / 16.0;
            _pts[_z] = (rde_vec_2F){ (f32)(_a.x + (_c.x - _a.x) * _tt), (f32)(_a.y + (_c.y - _a.y) * _tt - _sag * 4.0 * _tt * (1.0 - _tt)) };
            _rr[_z]  = 1.4f;
        }
        rde_rendering_2d_draw_stroke(_pts, _rr, 17u, _ink);
        if(_r[_i].pair != FUDE_ZOOM_NONE && _i < _r[_i].pair && _r[_i].over != FUDE_ZOOM_NONE) {
            // (over the top of the pulley, from one rim to the other)
            const fude_zoom_mech_body* _pl = &_b[_r[_i].over];
            const fude_zoom_v2 _o = _pl->at, _e0 = _r[_i].pb, _e1 = _r[_r[_i].pair].pb;
            const f64 _rad = rde_math_hypot_f64(_e0.x - _o.x, _e0.y - _o.y);
            f64 _a0 = rde_math_atan2_f64(_e0.y - _o.y, _e0.x - _o.x), _a1 = rde_math_atan2_f64(_e1.y - _o.y, _e1.x - _o.x);
            // (the way over its top: through straight up)
            if(_a0 > _a1) { const f64 _tmp = _a0; _a0 = _a1; _a1 = _tmp; }
            const b8 _through_top = _a0 <= RDE_PI * 0.5 && _a1 >= RDE_PI * 0.5;
            if(!_through_top) { _a0 += 2.0 * RDE_PI; const f64 _tmp = _a0; _a0 = _a1; _a1 = _tmp; }
            rde_vec_2F _arc[25];
            f32 _ar[25];
            for(u32 _z = 0; _z <= 24u; _z++) {
                const f64 _aa = _a0 + (_a1 - _a0) * (f64)_z / 24.0;
                _arc[_z] = (rde_vec_2F){ (f32)fude_zoom_sim_apply(_hs, (fude_zoom_v2){ _o.x + rde_math_cos_f64(_aa) * _rad, _o.y + rde_math_sin_f64(_aa) * _rad }).x,
                                         (f32)fude_zoom_sim_apply(_hs, (fude_zoom_v2){ _o.x + rde_math_cos_f64(_aa) * _rad, _o.y + rde_math_sin_f64(_aa) * _rad }).y };
                _ar[_z] = 1.4f;
            }
            rde_rendering_2d_draw_stroke(_arc, _ar, 25u, _ink);
        }
    }
    fude_zoom_page_render_dampers(_page, _hs);
    // The parts that move, and pulleys: the canvas's own drawing of each, moved; belts and chains over their wheels.
    for(u32 _i = 0; _i < _nb; _i++) {
        const u8 _k = _b[_i].part->kind;
        if(_b[_i].object >= fude_zoom_scene_object_count(_s) || !((_pb[_i] != NULL && !_b[_i].fixed) || _k == FUDE_ZOOM_MECH_PULLEY) ||
           _k == FUDE_ZOOM_MECH_PLUNGER || _k == FUDE_ZOOM_MECH_FOLLOWER) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _b[_i].object);
        const fude_zoom_sim _up = fude_zoom_scene_sim(_s, _o->frame, _home);
        const fude_zoom_sim _to = fude_zoom_sim_compose(_hs, fude_zoom_sim_compose(fude_zoom_mech_world_move(_w, _i), _up));
        fude_zoom_render_object(&_page->renderer, _s, _b[_i].object, _to, _half);
    }
    fude_zoom_page_render_belts(_page, _hs, _half);
}

// --- drawings made bodies: Make body on the lasso's row (props.h, bodyform.h) ------------------------------
//
// Whatever closed thing is drawn — a pen loop, a shape, a path, a fill — becomes a body of the mechanism (an open line:
// the ground), keeping its drawing: its properties an attribute on it (props.h). One undo step.

// The drawings the lasso holds that can be bodies (an outline of their own; not a circuit's or a mechanism's part, a
// wire, a sheet): into _out. How many.
u32 fude_zoom_page_body_targets(const fude_zoom_page* _page, u32* _out, u32 _max) {
    const fude_zoom_scene* _s = &_page->scene;
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks) && _n < _max; _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
        b8 _closed;
        if(_ob->kind == FUDE_ZOOM_KIND_SHAPE && (_ob->channels == FUDE_ZOOM_SHAPE_WIRE || _ob->channels == FUDE_ZOOM_SHAPE_SHEET ||
           (_ob->channels == FUDE_ZOOM_SHAPE_SYMBOL && (fude_zoom_part_of(_s, _o) != NULL || fude_zoom_mech_of(_s, _o) != NULL)))) {
            continue;
        }
        if(fude_zoom_scene_object_outline(_s, _o, 32u, &_pts, &_closed)) {
            _out[_n++] = _o;
        }
    }
    rde_arr_free(&_pts);
    return _n;
}

// Every drawing the lasso holds that can be a body (fude_zoom_page_body_targets, given room for all it holds, so none
// is left out however many are lassoed), into _out: an rde_arr of u32 made here on the std allocator, which the
// caller frees. How many.
RDE_INTERNAL u32 fude_zoom_page_body_all(const fude_zoom_page* _page, rde_arr* _out) {
    *_out = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(_out, rde_arr_length(&_page->selection.picks));
    const u32 _n = fude_zoom_page_body_targets(_page, (u32*)_out->memory, (u32)rde_arr_length(_out));
    rde_arr_resize(_out, _n);
    return _n;
}

// Their area together (closed ones; the home frame's square millimetres).
RDE_INTERNAL f64 fude_zoom_page_body_area(const fude_zoom_page* _page, const u32* _targets, u32 _n) {
    const fude_zoom_scene* _s = &_page->scene;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr _home_pts = rde_arr_new(sizeof(fude_sim_v2), rde_memory_allocator_get_default_std());
    f64 _area = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        b8 _closed;
        if(!fude_zoom_scene_object_outline(_s, _targets[_i], 64u, &_pts, &_closed) || !_closed) {
            continue;
        }
        const fude_zoom_sim _up = fude_zoom_scene_sim(_s, fude_zoom_scene_object(_s, _targets[_i])->frame, _home);
        const u32 _m = (u32)rde_arr_length(&_pts);
        rde_arr_resize(&_home_pts, _m);
        fude_sim_v2* _v = (fude_sim_v2*)_home_pts.memory;
        for(u32 _k = 0; _k < _m; _k++) {
            const fude_zoom_v2 _h = fude_zoom_sim_apply(_up, ((const fude_zoom_v2*)_pts.memory)[_k]);
            _v[_k] = (fude_sim_v2){ _h.x, _h.y };
        }
        _area += rde_math_abs_f64(fude_sim_polygon_area(_v, _m));
    }
    rde_arr_free(&_pts);
    rde_arr_free(&_home_pts);
    return _area;
}

// The card's Apply (or Not a body): each drawing's properties made again so (one undo step).
RDE_INTERNAL void fude_zoom_page_body_done(any _self, u32 _material, b8 _fixed, b8 _remove) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s = &_page->scene;
    rde_arr _targets_arr;
    const u32 _n = fude_zoom_page_body_all(_page, &_targets_arr);
    const u32* _targets = (const u32*)_targets_arr.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _died = rde_arr_new(sizeof(u32), _heap), _born = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _old = fude_zoom_props_find(_s, _targets[_i], FUDE_ZOOM_PROPS_BODY);
        if(_old != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _old, false);
            rde_arr_add(&_died, (any)&_old);
        }
        if(!_remove) {
            const fude_zoom_body_props _bp = { _material, _fixed, 0.0, -1.0, -1.0 };
            const u32 _made = fude_zoom_props_add_body(_s, _targets[_i], &_bp);
            rde_arr_add(&_born, (any)&_made);
        }
        const fude_zoom_object* _t = fude_zoom_scene_object(_s, _targets[_i]);
        _box = fude_zoom_box_union(_box, fude_zoom_sim_box(fude_zoom_scene_sim(_s, _t->frame, _s->camera.frame), _t->box));
    }
    if(rde_arr_length(&_died) + rde_arr_length(&_born) > 0u) {
        fude_zoom_history_push(_s, _s->camera.frame, _box, (const u32*)_died.memory, (u32)rde_arr_length(&_died), (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        if(!_remove) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_BODY_MADE));
        }
    }
    rde_arr_free(&_died);
    rde_arr_free(&_born);
    rde_arr_free(&_targets_arr);
}

// The lasso's row: Make body — the card, as the first drawing is now.
void fude_zoom_page_body_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return;
    }
    rde_arr _targets_arr;
    const u32 _n = fude_zoom_page_body_all(_page, &_targets_arr);
    const u32* _targets = (const u32*)_targets_arr.memory;
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_BODY_NONE));
    } else {
        fude_zoom_body_props _bp = { 0u, false, 0.0, -1.0, -1.0 };
        const u32 _old = fude_zoom_props_find(&_page->scene, _targets[0], FUDE_ZOOM_PROPS_BODY);
        const b8 _was = _old != FUDE_ZOOM_NONE && fude_zoom_props_body(&_page->scene, _old, &_bp);
        fude_zoom_body_open(&_page->body_form, _bp.material, _bp.fixed, _was, fude_zoom_page_body_area(_page, _targets, _n), fude_zoom_page_body_done, _page);
    }
    rde_arr_free(&_targets_arr);
}

// --- a part's limits (limits.h): Limits on the lasso's row ---------------------------------------------

// The part lassoed alone, when it has limits (FUDE_ZOOM_NONE: none).
u32 fude_zoom_page_limits_lone(const fude_zoom_page* _page) {
    const u32 _o = fude_zoom_page_lone_symbol(_page);
    return _o != FUDE_ZOOM_NONE && fude_zoom_limits_kinds(fude_zoom_part_of(&_page->scene, _o)) != 0u ? _o : FUDE_ZOOM_NONE;
}

// A part's name as the library says it (its kind's, in the language).
RDE_INTERNAL const c8* fude_zoom_page_part_name(const fude_zoom_page* _page, u32 _o) {
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS) : 0u;
    const u32 _kind = fude_zoom_symbol_kind_of(_n, _c);
    return _kind < FUDE_ZOOM_SYMBOL_KINDS ? fude_text(FUDE_ZOOM_PAGE_SYMBOL_NAMES[_kind]) : "";
}

// A part's typical limits as its text says them (a real part named in it, else its kind's), and which real part those
// are (-1: none).
RDE_INTERNAL fude_zoom_limits fude_zoom_page_limits_typical(const fude_zoom_page* _page, u32 _o, i32* _preset) {
    const fude_zoom_part* _part = fude_zoom_part_of(&_page->scene, _o);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c = fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _c, _text, sizeof(_text));
    f64 _v[2] = { 0.0, 0.0 };
    fude_zoom_circuit_value(_text, 0u, &_v[0]);
    fude_zoom_circuit_value(_text, 1u, &_v[1]);
    i32 _named = fude_zoom_limits_named(_part, _text);
    _named = _named >= 0 ? _named : fude_zoom_limits_named(_part, _part != NULL ? _part->value : NULL);
    u32 _count = 0;
    *_preset = _named >= 0 ? _named : (fude_zoom_limits_presets(_part, &_count) != NULL && _count > 0u ? 0 : -1);
    return fude_zoom_limits_typical(_part, _text, _v);
}

// Apply on the card: its limits kept with it (props.h) — its typical ones, none kept. One undo step.
RDE_INTERNAL void fude_zoom_page_limits_done(any _self, const f64* _most, i32 _preset) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s = &_page->scene;
    const u32 _o = _page->limits_object;
    _page->limits_object = FUDE_ZOOM_NONE;
    if(_o >= fude_zoom_scene_object_count(_s) || !(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    i32 _typical_preset;
    const fude_zoom_limits _typical = fude_zoom_page_limits_typical(_page, _o, &_typical_preset);
    const b8 _own = rde_memcmp(_most, _typical.most, sizeof(_typical.most)) != 0;
    u32 _died[1], _born[1], _nd = 0, _nb = 0;
    const u32 _old = fude_zoom_props_find(_s, _o, FUDE_ZOOM_PROPS_LIMITS);
    if(_old != FUDE_ZOOM_NONE) {
        fude_zoom_scene_set_alive(_s, _old, false);
        _died[_nd++] = _old;
    }
    if(_own) {
        _born[_nb++] = fude_zoom_props_add_limits(_s, _o, _most, _preset);
    }
    if(_nd + _nb > 0u) {
        const fude_zoom_object* _t = fude_zoom_scene_object(_s, _o);
        fude_zoom_history_push(_s, _t->frame, _t->box, _died, _nd, _born, _nb);
    }
}

// The lasso's row: Limits — the card, as the part's are now.
void fude_zoom_page_limits_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _o = _page != NULL && _page->open ? fude_zoom_page_limits_lone(_page) : FUDE_ZOOM_NONE;
    if(_o == FUDE_ZOOM_NONE) {
        return;
    }
    i32 _typical_preset;
    const fude_zoom_limits _typical = fude_zoom_page_limits_typical(_page, _o, &_typical_preset);
    f64 _most[FUDE_ZOOM_LIMIT_COUNT];
    rde_memcpy(_most, _typical.most, sizeof(_most));
    i32 _preset = _typical_preset;
    const u32 _old = fude_zoom_props_find(&_page->scene, _o, FUDE_ZOOM_PROPS_LIMITS);
    if(_old != FUDE_ZOOM_NONE) {
        fude_zoom_props_limits(&_page->scene, _old, _most, &_preset);
    }
    _page->limits_object = _o;
    fude_zoom_limits_open(&_page->limits_form, fude_zoom_part_of(&_page->scene, _o), fude_zoom_page_part_name(_page, _o), _most, _preset,
                          _typical.most, _typical_preset, fude_zoom_page_limits_done, _page);
}

// In Mechanisms (not playing): each body's mark at its drawing's middle — a ⊕ moving, the ground's hatch fixed. A
// body is an attribute, so only the canvas's attributes are looked at (page.h's attributes, gathered once each as
// objects are made), in the order of the objects as before, not every object each frame.
void fude_zoom_page_render_bodies(fude_zoom_page* _page) {
    if(_page->topic != FUDE_ZOOM_TOPIC_MECHANISMS || _page->play_on || _page->presenting) {
        return;
    }
    const fude_zoom_scene* _s = &_page->scene;
    const rde_color _ink = fude_theme_active()->accent;
    fude_zoom_props_gather(_s, &_page->attributes_looked, &_page->attributes);
    const u32* _attributes = (const u32*)_page->attributes.memory;
    for(u32 _j = 0; _j < (u32)rde_arr_length(&_page->attributes); _j++) {
        const u32 _i = _attributes[_j];
        fude_zoom_body_props _bp;
        if(fude_zoom_props_kind(_s, _i) != FUDE_ZOOM_PROPS_BODY || !fude_zoom_props_body(_s, _i, &_bp)) {
            continue;
        }
        const u32 _t = fude_zoom_props_target(_s, _i);
        if(_t == FUDE_ZOOM_NONE) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _t);
        if(fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _o->frame), (fude_zoom_v2){ (_o->box.min_x + _o->box.max_x) * 0.5, (_o->box.min_y + _o->box.max_y) * 0.5 });
        const rde_vec_2F _at = { (f32)_c.x, (f32)_c.y };
        if(_bp.fixed) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _at.x - 9.0f, _at.y }, (rde_vec_2F){ _at.x + 9.0f, _at.y }, _ink, 2.0f);
            for(u32 _k = 0; _k < 4u; _k++) {
                const f32 _x = _at.x - 8.0f + 5.0f * (f32)_k;
                rde_rendering_2d_draw_line_1((rde_vec_2F){ _x, _at.y }, (rde_vec_2F){ _x - 4.0f, _at.y - 5.0f }, _ink, 1.4f);
            }
        } else {
            rde_rendering_2d_draw_circle_with_border(_at, 7.0f, 20u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _ink, NULL);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _at.x - 7.0f, _at.y }, (rde_vec_2F){ _at.x + 7.0f, _at.y }, _ink, 1.4f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _at.x, _at.y - 7.0f }, (rde_vec_2F){ _at.x, _at.y + 7.0f }, _ink, 1.4f);
        }
    }
}
