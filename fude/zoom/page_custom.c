// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// Page, examples and custom parts: Insert → Examples (examples.h), custom parts made of what is drawn (logic.h),
// and the library's Custom tab with the templates (custom.h).
//
// The file is in three parts, each with a note at its head: the examples, custom parts, then the Custom tab and the
// templates. Making a canvas and drawing on it take two frames, since a new canvas is opened by the session and is
// open only on the next frame: what is to be drawn waits (examples_waiting, template_making) and a _follow function,
// called once a frame, draws it once that canvas is open. My parts are .part files in <save folder>/parts/, each with
// a .piece of its drawing beside it, and templates.txt there says which canvas is whose template.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// Declared ahead: Make part lets go of a part's template (defined with the templates, below).
RDE_INTERNAL void fude_zoom_page_template_drop(fude_zoom_page* _page, u8 _kind, const c8* _key);

// --- examples: Insert → Examples (examples.h) ----------------------------------------------------------------------
//
// Which of them (all, the electronics', the mechanisms', those of both), each made a canvas of its own in a folder of
// examples (numbered from the simplest, in the topic it is for, the view out to all of it), one a frame: a canvas is
// made, it is drawn on once it is open, then the next. At the end, the first opened.

// The examples' names, in examples.h's order (FUDE_ZOOM_EXAMPLE_COUNT of them: checked below).
RDE_INTERNAL const FUDE_TEXT_ FUDE_ZOOM_PAGE_EXAMPLE_NAMES[] = {
    FUDE_TEXT_ZOOM_EX_TORCH, FUDE_TEXT_ZOOM_EX_GATES, FUDE_TEXT_ZOOM_EX_FLASHER, FUDE_TEXT_ZOOM_EX_FULL_ADDER, FUDE_TEXT_ZOOM_EX_ADDER_4,
    FUDE_TEXT_ZOOM_EX_COUNTER, FUDE_TEXT_ZOOM_EX_ADDER_CHIP, FUDE_TEXT_ZOOM_EX_CHASER, FUDE_TEXT_ZOOM_EX_LED_RESISTOR,
    FUDE_TEXT_ZOOM_EX_RESISTOR_WATTS, FUDE_TEXT_ZOOM_EX_SHORT_FUSE, FUDE_TEXT_ZOOM_EX_CAP_POLARITY, FUDE_TEXT_ZOOM_EX_TRANSISTOR_SIZE,
    FUDE_TEXT_ZOOM_EX_RINGING, FUDE_TEXT_ZOOM_EX_DIGIT_COUNTER, FUDE_TEXT_ZOOM_EX_MATRIX_SCAN, FUDE_TEXT_ZOOM_EX_BAR_METER,
    FUDE_TEXT_ZOOM_EX_PANEL_METERS, FUDE_TEXT_ZOOM_EX_LCD_BY_HAND, FUDE_TEXT_ZOOM_EX_SCOPE_RC, FUDE_TEXT_ZOOM_EX_LIGHT_TEMP,
    FUDE_TEXT_ZOOM_EX_DECADE, FUDE_TEXT_ZOOM_EX_FLIPFLOPS, FUDE_TEXT_ZOOM_EX_COMPARATOR, FUDE_TEXT_ZOOM_EX_THYRISTORS,
    FUDE_TEXT_ZOOM_EX_POWER_SUPPLY, FUDE_TEXT_ZOOM_EX_LIGHT_LINK, FUDE_TEXT_ZOOM_EX_SPEAKER_TONE, FUDE_TEXT_ZOOM_EX_TRAFFIC, FUDE_TEXT_ZOOM_EX_PIANO, FUDE_TEXT_ZOOM_EX_CRANK, FUDE_TEXT_ZOOM_EX_GEAR_TRAIN, FUDE_TEXT_ZOOM_EX_PENDULUM, FUDE_TEXT_ZOOM_EX_PULLEYS,
    FUDE_TEXT_ZOOM_EX_PISTON, FUDE_TEXT_ZOOM_EX_RACK, FUDE_TEXT_ZOOM_EX_GEARS_RACK, FUDE_TEXT_ZOOM_EX_BODIES, FUDE_TEXT_ZOOM_EX_GEARBOX,
    FUDE_TEXT_ZOOM_EX_MACHINE, FUDE_TEXT_ZOOM_EX_MATERIALS, FUDE_TEXT_ZOOM_EX_TOO_HEAVY, FUDE_TEXT_ZOOM_EX_MOTOR_TORQUE, FUDE_TEXT_ZOOM_EX_COUPLER_CURVE, FUDE_TEXT_ZOOM_EX_BY_HAND,
    FUDE_TEXT_ZOOM_EX_BELTS, FUDE_TEXT_ZOOM_EX_CAM, FUDE_TEXT_ZOOM_EX_RATCHET, FUDE_TEXT_ZOOM_EX_DAMPER, FUDE_TEXT_ZOOM_EX_WORM, FUDE_TEXT_ZOOM_EX_STRANDBEEST, FUDE_TEXT_ZOOM_EX_ENGINE,
    FUDE_TEXT_ZOOM_EX_MOTOR_GEARS, FUDE_TEXT_ZOOM_EX_DYNAMO, FUDE_TEXT_ZOOM_EX_FORWARD_BACK, FUDE_TEXT_ZOOM_EX_TURN_COUNTER,
    FUDE_TEXT_ZOOM_EX_SHUTTLE, FUDE_TEXT_ZOOM_EX_SERVO_TESTER, FUDE_TEXT_ZOOM_EX_SERVO_ANGLES, FUDE_TEXT_ZOOM_EX_SOLENOID,
    FUDE_TEXT_ZOOM_EX_STEPPER, FUDE_TEXT_ZOOM_EX_SHAFT_SENSORS, FUDE_TEXT_ZOOM_EX_WORKBENCH,
    FUDE_TEXT_ZOOM_EX_EVERYTHING, FUDE_TEXT_ZOOM_EX_SOLENOID_ENGINE,
};
// A compile-time check that every example has a name (the size is -1, which does not compile, when one has not): a
// missing one would be the first string's id, shown as the wrong name.
typedef char fude_zoom_page_example_names_as_examples_h[(sizeof(FUDE_ZOOM_PAGE_EXAMPLE_NAMES) / sizeof(FUDE_ZOOM_PAGE_EXAMPLE_NAMES[0]) == FUDE_ZOOM_EXAMPLE_COUNT) ? 1 : -1];

// The words examples write by what they draw (examples.h's FUDE_ZOOM_EXAMPLE_WORD_, from its first).
RDE_INTERNAL const FUDE_TEXT_ FUDE_ZOOM_PAGE_EXAMPLE_WORDS[] = {
    FUDE_TEXT_ZOOM_EXW_TAP_SHORT, FUDE_TEXT_ZOOM_EXW_NO_FUSE, FUDE_TEXT_ZOOM_EXW_BACKWARDS, FUDE_TEXT_ZOOM_EXW_RESTART,
    FUDE_TEXT_ZOOM_EXW_LCD_STEPS, FUDE_TEXT_ZOOM_EXW_TURN_CRANK, FUDE_TEXT_ZOOM_EXW_DRAG_WEIGHT, FUDE_TEXT_ZOOM_EXW_SLIDERS,
    FUDE_TEXT_ZOOM_EXW_NO_DIODE, FUDE_TEXT_ZOOM_EXW_RATCHET_DRAG, FUDE_TEXT_ZOOM_EXW_FLIPFLOPS, FUDE_TEXT_ZOOM_EXW_COMPARATOR,
    FUDE_TEXT_ZOOM_EXW_THYRISTORS, FUDE_TEXT_ZOOM_EXW_LIGHT, FUDE_TEXT_ZOOM_EXW_SCOPE, FUDE_TEXT_ZOOM_EXW_TRAFFIC, FUDE_TEXT_ZOOM_EXW_PIANO,
    FUDE_TEXT_ZOOM_EXW_STRANDBEEST, FUDE_TEXT_ZOOM_EXW_ENGINE, FUDE_TEXT_ZOOM_EXW_SOLENOID_ENGINE,
};

// See page_parts.h. _example is an example (below FUDE_ZOOM_EXAMPLE_COUNT), a material's name
// (FUDE_ZOOM_EXAMPLE_WORD_MATERIAL on) or one of the words (FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT on): examples.h numbers
// them so.
const c8* fude_zoom_page_example_title(u32 _example) {
    const u32 _words = (u32)(sizeof(FUDE_ZOOM_PAGE_EXAMPLE_WORDS) / sizeof(FUDE_ZOOM_PAGE_EXAMPLE_WORDS[0]));
    if(_example >= FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT) {
        return _example - FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT < _words ? fude_text(FUDE_ZOOM_PAGE_EXAMPLE_WORDS[_example - FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT]) : NULL;
    }
    if(_example >= FUDE_ZOOM_EXAMPLE_WORD_MATERIAL) {
        return fude_zoom_body_material_name(_example - FUDE_ZOOM_EXAMPLE_WORD_MATERIAL);
    }
    return _example < FUDE_ZOOM_EXAMPLE_COUNT ? fude_text(FUDE_ZOOM_PAGE_EXAMPLE_NAMES[_example]) : NULL;
}

// The examples' folders' names (examples.h's FUDE_ZOOM_EXAMPLE_SET_), and their groups'.
RDE_INTERNAL const FUDE_TEXT_ FUDE_ZOOM_PAGE_EXAMPLE_SETS[FUDE_ZOOM_EXAMPLE_SETS] = {
    FUDE_TEXT_ZOOM_EXS_FIRST, FUDE_TEXT_ZOOM_EXS_LOGIC, FUDE_TEXT_ZOOM_EXS_DISPLAYS, FUDE_TEXT_ZOOM_EXS_SENSING, FUDE_TEXT_ZOOM_EXS_POWER,
    FUDE_TEXT_ZOOM_EXS_WRONG, FUDE_TEXT_ZOOM_EXS_BIG, FUDE_TEXT_ZOOM_EXS_LINKAGES, FUDE_TEXT_ZOOM_EXS_GEARS, FUDE_TEXT_ZOOM_EXS_MOTION,
    FUDE_TEXT_ZOOM_EXS_WRONG, FUDE_TEXT_ZOOM_EXS_BIG, FUDE_TEXT_ZOOM_EXS_MOTORS, FUDE_TEXT_ZOOM_EXS_ACTUATORS, FUDE_TEXT_ZOOM_EXS_MACHINE_SENSORS,
    FUDE_TEXT_ZOOM_EXS_BIG,
};
// Returns an examples' group's folder's name: Electronics, Mechanisms, or the both's.
RDE_INTERNAL const c8* fude_zoom_page_example_group_name(u8 _group) {
    return fude_text(_group == FUDE_ZOOM_EXAMPLES_ELECTRONICS ? FUDE_TEXT_ZOOM_TOPIC_ELECTRONICS :
                     (_group == FUDE_ZOOM_EXAMPLES_MECHANISMS ? FUDE_TEXT_ZOOM_TOPIC_MECHANISMS : FUDE_TEXT_ZOOM_EXS_BOTH));
}

// The folder _name in folder _parent (0: the top level): its id — made there when it is not and _make; else 0.
RDE_INTERNAL u32 fude_zoom_page_folder_in(fude_zoom_page* _page, u32 _parent, const c8* _name, b8 _make) {
    fude_notes* _notes = _page->app->notes;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_notes->notes.memory)[_i];
        if(_n->kind == FUDE_NOTE_FOLDER && _n->parent == _parent && rde_strcmp(_n->name, _name) == 0) {
            return _n->id;
        }
    }
    return _make ? fude_notes_add(_notes, FUDE_NOTE_FOLDER, _parent, _name) : 0u;
}

// Example _e's folder: its set's in its group's, in the examples' (made when not there and _make; else 0 when not).
RDE_INTERNAL u32 fude_zoom_page_example_folder(fude_zoom_page* _page, u32 _e, b8 _make) {
    const u8 _set = fude_zoom_example_set(_e);
    const u32 _group = _page->examples_folder != 0u ? fude_zoom_page_folder_in(_page, _page->examples_folder, fude_zoom_page_example_group_name(fude_zoom_example_set_group(_set)), _make) : 0u;
    return _group != 0u ? fude_zoom_page_folder_in(_page, _group, fude_text(FUDE_ZOOM_PAGE_EXAMPLE_SETS[_set]), _make) : 0u;
}

// Example _e's place in its folder (its set's examples in the list's order: 1, 2, ...).
RDE_INTERNAL u32 fude_zoom_page_example_place(u32 _e) {
    u32 _place = 1;
    for(u32 _k = 0; _k < _e; _k++) {
        _place += fude_zoom_example_set(_k) == fude_zoom_example_set(_e) ? 1u : 0u;
    }
    return _place;
}

// Example _e's canvas anywhere in the folder of examples (its folder's, or where an older version put it — all in the
// folder itself): one named for it ("3. Pulleys": its place, which may have changed since — the number not
// counted), its id; 0: none.
RDE_INTERNAL u32 fude_zoom_page_example_canvas(fude_zoom_page* _page, u32 _e) {
    const fude_notes* _notes = _page->app->notes;
    const c8* _title = fude_zoom_page_example_title(_e);
    for(u32 _i = 0; _title != NULL && _page->examples_folder != 0u && _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_notes->notes.memory)[_i];
        if(_n->kind != FUDE_NOTE_CANVAS || !fude_notes_is_within(_notes, _n->parent, _page->examples_folder)) {
            continue;
        }
        const c8* _t = _n->name;
        while(*_t >= '0' && *_t <= '9') {
            _t++;
        }
        _t += _t != _n->name && _t[0] == '.' && _t[1] == ' ' ? 2 : 0;
        if(rde_strcmp(_t, _title) == 0) {
            return _n->id;
        }
    }
    return 0u;
}

// Each example's canvas in its folder (examples.h's sets, in their groups'), named by its place there, in that order
// (after anything else there); the groups' folders and theirs in order too: the folders as the list is, those made
// before kept as they were (what was done on them too) — those an older version left in the examples' folder itself
// moved into theirs.
RDE_INTERNAL void fude_zoom_page_examples_order(fude_zoom_page* _page) {
    fude_notes* _notes = _page->app->notes;
    for(u32 _e = 0; _e < FUDE_ZOOM_EXAMPLE_COUNT; _e++) {
        const u32 _id = fude_zoom_page_example_canvas(_page, _e);
        if(_id == 0u) {
            continue;
        }
        c8 _name[FUDE_NOTE_NAME];
        rde_snprintf(_name, sizeof(_name), "%u. %s", fude_zoom_page_example_place(_e), fude_zoom_page_example_title(_e));
        const fude_note* _n = fude_notes_find(_notes, _id);
        if(_n != NULL && rde_strcmp(_n->name, _name) != 0) {
            fude_notes_rename(_notes, _id, _name);
        }
        fude_notes_move(_notes, _id, fude_zoom_page_example_folder(_page, _e, true), 0u);
    }
    for(u8 _g = FUDE_ZOOM_EXAMPLES_ELECTRONICS; _g <= FUDE_ZOOM_EXAMPLES_BOTH; _g++) {
        const u32 _gf = fude_zoom_page_folder_in(_page, _page->examples_folder, fude_zoom_page_example_group_name(_g), false);
        if(_gf == 0u) {
            continue;
        }
        fude_notes_move(_notes, _gf, _page->examples_folder, 0u);
        for(u8 _set = 0; _set < FUDE_ZOOM_EXAMPLE_SETS; _set++) {
            const u32 _sf = fude_zoom_example_set_group(_set) == _g ? fude_zoom_page_folder_in(_page, _gf, fude_text(FUDE_ZOOM_PAGE_EXAMPLE_SETS[_set]), false) : 0u;
            if(_sf != 0u) {
                fude_notes_move(_notes, _sf, _gf, 0u);
            }
        }
    }
}

// The next one's canvas made (opened next frame); none left: the folder put in order, the first one made opened (none
// made: the first one asked for), how many said.
RDE_INTERNAL void fude_zoom_page_examples_next(fude_zoom_page* _page) {
    fude_app* _app = _page->app;
    if(rde_arr_length(&_page->examples_todo) == 0u) {
        _page->examples_waiting = 0u;
        fude_zoom_page_examples_order(_page);
        if(_page->examples_first != 0u) {
            fude_notes_open(_app->notes, _page->examples_first);
        }
        const fude_note* _folder = fude_notes_find(_app->notes, _page->examples_folder);
        c8 _say[300];
        if(_page->examples_made > 0u) {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_EXAMPLES_MADE, FUDE_TN(_page->examples_made), FUDE_TS(_folder != NULL ? _folder->name : ""));
        } else {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_EXAMPLES_THERE, FUDE_TS(_folder != NULL ? _folder->name : ""));
        }
        fude_notice_show(_say);
        return;
    }
    const u32 _e = ((const u32*)_page->examples_todo.memory)[0];
    c8 _name[FUDE_NOTE_NAME];
    rde_snprintf(_name, sizeof(_name), "%u. %s", fude_zoom_page_example_place(_e), fude_zoom_page_example_title(_e));
    const u32 _id = fude_documents_new_canvas(_app, 0u, NULL, _name, fude_zoom_page_example_folder(_page, _e, true));
    if(_id == 0u) {
        rde_arr_clear(&_page->examples_todo);
        _page->examples_waiting = 0u;
        rde_log_level(RDE_LOG_LEVEL_ERROR, "examples: the canvas for \"%s\" could not be made", _name);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_CANVAS_NOT_MADE));
        return;
    }
    _page->examples_waiting = _id;
    if(_page->examples_first == 0u) {
        _page->examples_first = _id;
    }
}

// Returns the frame things put in the middle of the view go in (the drawing frame there, for a box 4 points round the
// camera's point).
RDE_INTERNAL u32 fude_zoom_page_middle_frame(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    const f64 _near = 4.0 / _s->camera.z;
    return fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _s->camera.at.x - _near, _s->camera.at.y - _near, _s->camera.at.x + _near, _s->camera.at.y + _near });
}

// The view moved to the middle of _box (frame _f's units) and out, until all of it is on the screen with room round it
// (clear of the toolbar's side).
RDE_INTERNAL void fude_zoom_page_view_out_to(fude_zoom_page* _page, u32 _f, fude_zoom_box _box) {
    if(fude_zoom_box_is_empty(_box)) {
        return;
    }
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    fude_zoom_box _on = fude_zoom_sim_box(fude_zoom_page_frame_sim(_page, _f), _box);
    // Its middle a little over the screen's (60 points at most): room under it for Play's panel.
    const f64 _lift = rde_math_min_f64(60.0, _half.y * 0.1);
    fude_zoom_camera_pan(&_page->scene, (fude_zoom_v2){ -(_on.min_x + _on.max_x) * 0.5, _lift - (_on.min_y + _on.max_y) * 0.5 });
    fude_zoom_camera_settle(&_page->scene, _half);
    _on = fude_zoom_sim_box(fude_zoom_page_frame_sim(_page, _f), _box);
    const f64 _reach = rde_math_max_f64((_on.max_x - _on.min_x) * 0.5 / rde_math_max_f64(_half.x - 120.0, _half.x * 0.6),
                            (_on.max_y - _on.min_y) * 0.5 / rde_math_max_f64(_half.y - _lift - 150.0, _half.y * 0.5));
    if(_reach > 1.0) {
        fude_zoom_camera_zoom_at(&_page->scene, (fude_zoom_v2){ 0.0, _lift }, 1.0 / _reach);
        fude_zoom_camera_settle(&_page->scene, _half);
    }
}

// Makes what was just made (_born, in frame _f) one undo step. Returns what it covers in _f (only what is in _f
// counts).
RDE_INTERNAL fude_zoom_box fude_zoom_page_made_step(fude_zoom_page* _page, u32 _f, const rde_arr* _born) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_box _box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < (u32)rde_arr_length(_born); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_born->memory)[_i]);
        if(_o->frame == _f && !fude_zoom_box_is_empty(_o->box)) {
            _box = fude_zoom_box_union(_box, _o->box);
        }
    }
    if(rde_arr_length(_born) > 0u) {
        fude_zoom_history_push(_s, _f, _box, NULL, 0, (const u32*)_born->memory, (u32)rde_arr_length(_born));
    }
    return _box;
}

// See page_parts.h. Example _e drawn on the canvas open (its new one): one undo step, the canvas in its topic
// (Mechanisms for the mechanisms', else Electronics), the view out to all of it.
void fude_zoom_page_examples_draw(fude_zoom_page* _page, u32 _e) {
    const u32 _f = fude_zoom_page_middle_frame(_page);
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _f);
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_example_build(&_page->scene, _f, fude_zoom_sim_inverse(_fs), FUDE_THEME_INK, _e, fude_zoom_page_example_title, &_born);
    const fude_zoom_box _box = fude_zoom_page_made_step(_page, _f, &_born);
    rde_arr_free(&_born);
    const u8 _group = fude_zoom_example_group(_e);
    _page->topic = _group == FUDE_ZOOM_EXAMPLES_MECHANISMS ? FUDE_ZOOM_TOPIC_MECHANISMS : FUDE_ZOOM_TOPIC_ELECTRONICS;
    fude_zoom_page_tools_save(_page);
    fude_zoom_page_view_out_to(_page, _f, _box);
}

// The folder _name at the top of the canvases (made, if not there; 0: not made).
RDE_INTERNAL u32 fude_zoom_page_top_folder(fude_zoom_page* _page, const c8* _name) {
    return fude_zoom_page_folder_in(_page, 0u, _name, true);
}

// See page_parts.h. Which of them chosen (_which: 0 all, 1 electronics', 2 mechanisms', 3 those of both): those, in
// order, each a canvas, in the folder of examples (made at the top, if not there). Nothing while some are being made.
void fude_zoom_page_examples_chosen(any _self, u32 _which) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(_page->examples_waiting != 0u) {
        return;   // Some being made already.
    }
    rde_arr_clear(&_page->examples_todo);
    for(u32 _e = 0; _e < FUDE_ZOOM_EXAMPLE_COUNT; _e++) {
        const u8 _g = fude_zoom_example_group(_e);
        if(_which == 0u || (_which == 1u && _g == FUDE_ZOOM_EXAMPLES_ELECTRONICS) || (_which == 2u && _g == FUDE_ZOOM_EXAMPLES_MECHANISMS) ||
           (_which == 3u && _g == FUDE_ZOOM_EXAMPLES_BOTH)) {
            rde_arr_add(&_page->examples_todo, (any)&_e);
        }
    }
    _page->examples_folder = fude_zoom_page_top_folder(_page, fude_text(FUDE_TEXT_ZOOM_EXAMPLES));
    _page->examples_first = 0u;
    _page->examples_made  = 0u;
    // Those there already kept as they are: only the others made; none to make, the first of them opened.
    u32* _todo = (u32*)_page->examples_todo.memory;
    u32 _left = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->examples_todo); _i++) {
        const u32 _there = fude_zoom_page_example_canvas(_page, _todo[_i]);
        if(_there == 0u) {
            _todo[_left++] = _todo[_i];
        } else if(_page->examples_first == 0u && _left == 0u) {
            _page->examples_first = _there;
        }
    }
    const b8 _some = _left > 0u;
    rde_arr_resize(&_page->examples_todo, _left);
    if(_some) {
        _page->examples_first = 0u;   // The first one made is opened at the end.
    }
    if(rde_arr_length(&_page->examples_todo) > 0u) {
        c8 _say[200];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_EXAMPLES_MAKING, FUDE_TN((u32)rde_arr_length(&_page->examples_todo)));
        fude_notice_show(_say);
    }
    fude_zoom_page_examples_next(_page);
}

// See page_parts.h: the choice card, its four choices in fude_zoom_page_examples_chosen's order.
void fude_zoom_page_examples_card(fude_zoom_page* _page) {
    const c8* _labels[4] = { fude_text(FUDE_TEXT_ZOOM_EXAMPLES_ALL), fude_text(FUDE_TEXT_ZOOM_TOPIC_ELECTRONICS), fude_text(FUDE_TEXT_ZOOM_TOPIC_MECHANISMS),
                             fude_text(FUDE_TEXT_ZOOM_EXAMPLES_BOTH) };
    const c8* _icons[4] = { FUDE_ICON_STACK, FUDE_ICON_SIGNAL_HIGH, FUDE_ICON_ROTATE, FUDE_ICON_SHAPES };
    fude_zoom_choice_open(&_page->choice_form, fude_text(FUDE_TEXT_ZOOM_EXAMPLES_TITLE), _labels, _icons, 4u, fude_zoom_page_examples_chosen, _page);
}

// See page_parts.h. Once a frame: the one waiting drawn once its canvas is open, then the next made.
void fude_zoom_page_examples_follow(fude_zoom_page* _page) {
    if(_page->examples_waiting != 0u) {
        fude_redraw_changed();   // (render on demand: its canvas opens a frame or two on)
    }
    if(_page->examples_waiting == 0u || !_page->open || _page->canvas != _page->examples_waiting || rde_arr_length(&_page->examples_todo) == 0u) {
        return;
    }
    const u32 _e = ((const u32*)_page->examples_todo.memory)[0];
    rde_arr_remove(&_page->examples_todo, 0u);
    fude_zoom_page_examples_draw(_page, _e);
    _page->examples_made++;
    _page->examples_waiting = 0u;
    fude_zoom_page_examples_next(_page);
}

// --- custom parts made of what is drawn: Make part on the lasso's row (logic.h) ------------------------------------
//
// What the lasso holds of a circuit (gates, flip-flops, chips, custom parts, wired) made one part: its logic inputs
// its inputs, its probes its outputs (their texts their names), named by a typed text in the lasso (else "Part N"),
// kept in My parts (a .part file each, beside the canvases), and one of it put beside what it was made of. Made again
// with the same name: a newer version, every one of it on the canvas following.

// Returns the folder My parts are kept in, <save folder>/parts/, created if missing; found again should the save
// folder change.
RDE_INTERNAL const c8* fude_zoom_page_parts_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };   // The folder, as last found.
    static c8 _in[RDE_MAX_PATH]  = { 0 };   // The save folder it was found in.
    if(_dir[0] == 0 || rde_strcmp(_in, fude_save_dir()) != 0) {
        rde_snprintf(_in, sizeof(_in), "%s", fude_save_dir());
        rde_snprintf(_dir, sizeof(_dir), "%sparts/", _in);
        c8 _probe[RDE_MAX_PATH];
        rde_snprintf(_probe, sizeof(_probe), "%sx.part", _dir);
        rde_file_create_missing_dirs(_probe);
    }
    return _dir;
}

// fude_zoom_page_parts_load's crawler: a .part file read and learnt into the logic library. One that cannot be read, or
// that the library cannot learn, is logged (with what was wrong with it) and skipped; the others are still learnt.
// Returns true, to go on.
RDE_INTERNAL b8 fude_zoom_page_parts_learn_one(const c8* _path, b8 _is_dir, any _user) {
    RDE_UNUSED(_user);
    const usize _n = rde_strlen(_path);
    if(_is_dir || _n < 5u || rde_strcmp(_path + _n - 5u, ".part") != 0) {
        return true;
    }
    u32 _size = 0;
    u8* _text = fude_file_read(_path, &_size);
    if(_text == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "my parts: %s could not be read; skipped", _path);
        return true;
    }
    c8 _error[160];
    _error[0] = 0;
    if(fude_zoom_logic_learn((const c8*)_text, _size, _error, sizeof(_error)) == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "my parts: %s could not be learnt (%s); skipped", _path, _error);
    }
    fude_file_free(_text);
    return true;
}

// See page_parts.h. My parts learnt (once a folder: a person's other saves, theirs).
void fude_zoom_page_parts_load(void) {
    static c8 _from[RDE_MAX_PATH] = { 0 };   // The folder My parts were last learnt from.
    const c8* _dir = fude_zoom_page_parts_dir();
    if(rde_strcmp(_from, _dir) == 0) {
        return;
    }
    rde_snprintf(_from, sizeof(_from), "%s", _dir);
    if(rde_file_dir_exists(_dir)) {
        rde_file_crawl_dir_recursively(_dir, fude_zoom_page_parts_learn_one, NULL, 0u, NULL);
    }
}

// See page_parts.h. Returns whether the lasso holds a logic part other than an input or a probe (what a part is made
// of).
b8 fude_zoom_page_lasso_logic(const fude_zoom_page* _page) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        const fude_zoom_part* _p = fude_zoom_part_of(&_page->scene, _o);
        if(_p != NULL && fude_zoom_logic_is(_p) && _p->model != FUDE_ZOOM_MODEL_LOGIC_IN && _p->model != FUDE_ZOOM_MODEL_LOGIC_OUT) {
            return true;
        }
    }
    return false;
}

#define FUDE_ZOOM_PAGE_PART_W 120.0   // a custom part's block put down (screen points; its pins a row of 24 apart)

// Writes how many inputs and outputs custom part _def has (every port not an input counts as an output).
RDE_INTERNAL void fude_zoom_page_part_ports(const fude_sim_def* _def, u32* _ins, u32* _outs) {
    *_ins = *_outs = 0u;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->ports); _i++) {
        if(((const fude_sim_port*)_def->ports.memory)[_i].dir == FUDE_SIM_IN) { (*_ins)++; } else { (*_outs)++; }
    }
}

// Writes where custom part _id's file of extension _ext is: its slug (the id without "user/"), in My parts.
RDE_INTERNAL void fude_zoom_page_part_path(const c8* _id, const c8* _ext, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%s%s", fude_zoom_page_parts_dir(), rde_strncmp(_id, "user/", 5u) == 0 ? _id + 5 : _id, _ext);
}

// Says that custom part _name could not be saved in My parts (its file not written: logged where it was written).
RDE_INTERNAL void fude_zoom_page_part_not_saved(const c8* _name) {
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PART_NOT_SAVED, FUDE_TS(_name));
    fude_notice_show(_say);
}

// Custom part _def kept: learnt (in place of an older one with its id: every one of it on every canvas follows) and
// written to My parts. Returns the definition learnt (NULL: not read back). It is learnt from its own text, as a
// .part file would be read. *_saved gets whether its file was written: when not (fude_bytes_write_and_free logs it),
// it is known only until the app closes, and the caller says so (fude_zoom_page_part_not_saved).
RDE_INTERNAL const fude_sim_def* fude_zoom_page_part_keep(const fude_sim_def* _def, b8* _saved) {
    fude_bytes _text = fude_bytes_new(1024u);
    fude_sim_def_write(_def, &_text);
    c8 _error[160];
    const fude_sim_def* _learnt = fude_zoom_logic_learn((const c8*)_text.memory, fude_bytes_size(&_text), _error, sizeof(_error));
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_part_path(_def->id, ".part", _path, sizeof(_path));
    *_saved = fude_bytes_write_and_free(&_text, _path, NULL);
    return _learnt;
}

// Keeps custom part _id's drawing beside it as a .piece (what _sel holds): its template is made of it the first time
// it opens. Returns false when its file could not be written (fude_zoom_piece_save logs it); true when it was, or there
// was nothing to keep.
RDE_INTERNAL b8 fude_zoom_page_part_drawing(fude_zoom_page* _page, const fude_zoom_selection* _sel, const c8* _id, const c8* _name) {
    fude_zoom_piece _drawn;
    rde_memset(&_drawn, 0, sizeof(_drawn));
    _drawn.items = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
    rde_snprintf(_drawn.name, sizeof(_drawn.name), "%s", _name);
    b8 _saved = true;
    if(fude_zoom_page_held_clips(_page, _sel, &_drawn.items, &_drawn.box)) {
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_part_path(_id, ".piece", _path, sizeof(_path));
        _saved = fude_zoom_piece_save(&_drawn, _path);
    }
    fude_zoom_piece_clear(&_drawn);
    return _saved;
}

// Puts one of custom part _def at _x, _y (screen points from the middle), lassoed alone: one undo step. Its block is
// FUDE_ZOOM_PAGE_PART_W wide, and tall enough for its pins 24 points apart (60 at least). Returns its object
// (FUDE_ZOOM_NONE: none).
RDE_INTERNAL u32 fude_zoom_page_part_put_at(fude_zoom_page* _page, const fude_sim_def* _def, f64 _x, f64 _y) {
    fude_zoom_scene* _s = &_page->scene;
    u32 _ins, _outs;
    fude_zoom_page_part_ports(_def, &_ins, &_outs);
    const f64 _rows = (f64)(_ins > _outs ? _ins : _outs);
    const u32 _f = fude_zoom_page_middle_frame(_page);
    rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    const u32 _made = fude_zoom_page_look_sized(_page, _f, "custom part", _x, _y, 0.0, FUDE_ZOOM_PAGE_PART_W, rde_math_max_f64(60.0, (_rows + 1.0) * 24.0), _def->name, &_born);
    if(_made != FUDE_ZOOM_NONE) {
        fude_zoom_page_made_step(_page, _f, &_born);
        fude_zoom_select_clear(&_page->selection);
        rde_arr_add(&_page->selection.picks, &(fude_zoom_pick){ _made });
        fude_zoom_select_update(&_page->selection, _s);
    }
    rde_arr_free(&_born);
    return _made;
}

// See page.h. Its name is the lasso's first typed text's first line (up to FUDE_SIM_NAME bytes, its quotes made
// apostrophes), else "Part N" (N one more than the person's parts). It is said when nothing could be made of it, or it
// has no inputs or outputs, and when it was made but could not be saved in My parts.
void fude_zoom_page_make_part(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    const u32 _no = fude_zoom_scene_object_count(_s);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _scope_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_scope_arr, _no);
    u8* _scope = (u8*)_scope_arr.memory;
    // Its name: the lasso's first typed text's first line. What the lasso holds is the circuit's scope.
    c8 _name[FUDE_SIM_NAME] = { 0 };
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->selection.picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        if(_o >= _no) {
            continue;
        }
        _scope[_o] = 1u;
        f64 _size, _w, _h;
        u8 _style;
        const c8* _text;
        u32 _len;
        if(_name[0] == 0 && fude_zoom_scene_object(_s, _o)->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_text(_s, _o, &_size, &_w, &_h, &_style, &_text, &_len)) {
            // (its first line, cut before a whole character when it is too long; a " made ' as a name cannot hold one)
            u32 _line = 0;
            while(_line < _len && _text[_line] != '\n') {
                _line++;
            }
            fude_utf8_copy(_name, sizeof(_name), _text, _line);
            for(c8* _q = _name; *_q != 0; _q++) {
                *_q = *_q == '"' ? '\'' : *_q;
            }
        }
    }
    if(_name[0] == 0) {
        u32 _mine = 0;
        const fude_sim_lib* _lib = fude_zoom_logic_library();
        const fude_sim_def* const* _d = (const fude_sim_def* const*)_lib->defs.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_lib->defs); _i++) {
            _mine += rde_strncmp(_d[_i]->id, "user/", 5u) == 0 ? 1u : 0u;
        }
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_PART_DEFAULT, FUDE_TN(_mine + 1u));
    }
    c8 _slug[40], _id[FUDE_SIM_ID];
    fude_zoom_logic_slug(_name, _slug, sizeof(_slug));
    rde_snprintf(_id, sizeof(_id), "user/%s", _slug);
    fude_zoom_circuit _c;
    fude_zoom_circuit_init(&_c);
    fude_zoom_circuit_build_in(&_c, _s, _scope);
    rde_arr_free(&_scope_arr);
    u32 _problem = FUDE_ZOOM_MAKE_OK;
    fude_sim_def* _def = fude_zoom_logic_make(&_c, _s, _id, _name, &_problem);
    fude_zoom_circuit_destroy(&_c);
    if(_def == NULL) {
        fude_notice_show(fude_text(_problem == FUDE_ZOOM_MAKE_NOTHING ? FUDE_TEXT_ZOOM_PART_NOTHING : FUDE_TEXT_ZOOM_PART_NO_PORTS));
        return;
    }
    // Made again: its pins where they were, so what is wired to them on canvases is still on them.
    fude_zoom_logic_keep_order(_def, fude_sim_lib_find(fude_zoom_logic_library(), _id));
    u32 _ins = 0, _outs = 0;
    fude_zoom_page_part_ports(_def, &_ins, &_outs);
    // Learnt (in place of an older one of its name: every one of it follows), kept in My parts, its drawing beside it.
    b8 _saved = false;
    const fude_sim_def* _learnt = fude_zoom_page_part_keep(_def, &_saved);
    fude_sim_def_free(_def);
    _saved = fude_zoom_page_part_drawing(_page, &_page->selection, _id, _name) && _saved;
    fude_zoom_page_template_drop(_page, FUDE_ZOOM_CUSTOM_PART, _id);   // Its template made afresh from this drawing.
    // One of it beside what it was made of, lassoed.
    const fude_zoom_box _b = _page->selection.box;
    if(_learnt != NULL) {
        fude_zoom_page_part_put_at(_page, _learnt, fude_zoom_box_is_empty(_b) ? 0.0 : _b.max_x + 40.0 + FUDE_ZOOM_PAGE_PART_W * 0.5,
                                   fude_zoom_box_is_empty(_b) ? 0.0 : (_b.min_y + _b.max_y) * 0.5);
    }
    if(!_saved) {
        fude_zoom_page_part_not_saved(_name);   // (made, but not kept for the next time)
        return;
    }
    c8 _say[160];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PART_MADE, FUDE_TS(_name), FUDE_TN(_ins), FUDE_TN(_outs));
    fude_notice_show(_say);
}

// See page_parts.h: fude_zoom_page_make_part, with a canvas open.
void fude_zoom_page_part_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL && _page->open) {
        fude_zoom_page_make_part(_page);
    }
}

// --- Custom: the library's tab of what is kept, and the templates (custom.h) ---------------------------------------
//
// Every library panel's last tab: My pieces of its area (those with its symbols in them; a drawing of none, every
// panel's) and, the circuits', My parts. A tap puts one down; its ⋯, or a hold on it, its card: Edit (a piece) or
// Inside (a part), Rename, Delete. Each has a canvas of its own in the folder Custom, its template, made the first
// time it is opened (the piece put on it, the part's drawing, or its inside drawn from what it is): what it holds
// when it is left is kept as the piece, or made the part again, every one of it on every canvas following. A custom
// part lassoed alone has Inside on the lasso's row; Back on a template goes back where it was opened from.
//
// A tile's item is a u32: a piece's index in My pieces, or a part's index in the logic library with
// FUDE_ZOOM_PAGE_CUSTOM_PART set. A kept item is named by its kind and a key: a piece's id as text, a part's id.

#define FUDE_ZOOM_PAGE_CUSTOM_PART 0x80000000u   // a Custom tile's item: a custom part's place in the logic library
#define FUDE_ZOOM_PAGE_CUSTOM_HOLD 0.55          // seconds a hand held on a tile still: its card

// Returns templates.txt's path in My parts: which canvas is whose template.
RDE_INTERNAL const c8* fude_zoom_page_templates_path(void) {
    static c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%stemplates.txt", fude_zoom_page_parts_dir());
    return _path;
}

// Returns which canvas is whose (read the first time wanted, and again for another person's saves), and kept.
RDE_INTERNAL fude_zoom_templates* fude_zoom_page_templates(fude_zoom_page* _page) {
    static c8 _from[RDE_MAX_PATH] = { 0 };   // The file the list was last read from.
    if(!_page->templates_ready) {
        fude_zoom_templates_init(&_page->templates);
        _page->templates_ready = true;
        _from[0] = 0;
    }
    const c8* _path = fude_zoom_page_templates_path();
    if(rde_strcmp(_from, _path) != 0) {
        rde_snprintf(_from, sizeof(_from), "%s", _path);
        fude_zoom_templates_read(&_page->templates, _path);
    }
    return &_page->templates;
}

// Writes which canvas is whose to templates.txt. A failed write is logged: the map in memory still holds it, and the
// next change writes it whole again.
RDE_INTERNAL void fude_zoom_page_templates_save(fude_zoom_page* _page) {
    if(!fude_zoom_templates_write(fude_zoom_page_templates(_page), fude_zoom_page_templates_path())) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "custom: %s could not be written", fude_zoom_page_templates_path());
    }
}

// Returns a custom part of a person's own by its id (NULL: none, forgotten since). My parts are learnt first.
RDE_INTERNAL const fude_sim_def* fude_zoom_page_custom_def(const c8* _id) {
    fude_zoom_page_parts_load();
    const fude_sim_def* _d = fude_sim_lib_find(fude_zoom_logic_library(), _id);
    return _d != NULL && rde_strncmp(_d->id, "user/", 5u) == 0 ? _d : NULL;
}

// Returns a kept piece's place in My pieces by its key, its id as text (FUDE_ZOOM_NONE: none).
RDE_INTERNAL u32 fude_zoom_page_custom_piece(fude_zoom_page* _page, const c8* _key) {
    const fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    c8* _end = NULL;
    const unsigned long _id = rde_strtoul(_key, &_end, 10);
    for(u32 _i = 0; _end != _key && _i < _lib->count; _i++) {
        if(_lib->list[_i].id == (u32)_id) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// Returns what _kind's _key is called (NULL: none such).
RDE_INTERNAL const c8* fude_zoom_page_custom_name(fude_zoom_page* _page, u8 _kind, const c8* _key) {
    if(_kind == FUDE_ZOOM_CUSTOM_PART) {
        const fude_sim_def* _d = fude_zoom_page_custom_def(_key);
        return _d != NULL ? _d->name : NULL;
    }
    const u32 _i = fude_zoom_page_custom_piece(_page, _key);
    return _i != FUDE_ZOOM_NONE ? fude_zoom_page_pieces(_page)->list[_i].name : NULL;
}

// See page_parts.h. A Custom tile's item as a kind and a key (FUDE_ZOOM_CUSTOM_KEY bytes). False: none such.
b8 fude_zoom_page_custom_of(fude_zoom_page* _page, u32 _item, u8* _kind, c8* _key) {
    if(_item == FUDE_ZOOM_NONE) {
        return false;
    }
    if(_item & FUDE_ZOOM_PAGE_CUSTOM_PART) {
        const fude_sim_lib* _lib = fude_zoom_logic_library();
        const u32 _at = _item & ~FUDE_ZOOM_PAGE_CUSTOM_PART;
        if(_at >= (u32)rde_arr_length(&_lib->defs)) {
            return false;
        }
        *_kind = FUDE_ZOOM_CUSTOM_PART;
        rde_snprintf(_key, FUDE_ZOOM_CUSTOM_KEY, "%s", ((const fude_sim_def* const*)_lib->defs.memory)[_at]->id);
        return true;
    }
    const fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
    if(_item >= _lib->count) {
        return false;
    }
    *_kind = FUDE_ZOOM_CUSTOM_PIECE;
    rde_snprintf(_key, FUDE_ZOOM_CUSTOM_KEY, "%u", _lib->list[_item].id);
    return true;
}

// See page_parts.h. The panel's Custom tiles, listed again into custom_items: My parts (the circuits' panel's), then
// My pieces of the panel's area (those with its symbols in them, and drawings with none: every panel's). Returns how
// many.
u32 fude_zoom_page_custom_items(fude_zoom_page* _page) {
    rde_arr_clear(&_page->custom_items);
    const u32 _tabs = _page->library_tabs != 0u ? _page->library_tabs : FUDE_ZOOM_PAGE_LIBRARY_DIAGRAMS;
    if(_tabs & (1u << FUDE_ZOOM_SYMBOL_FAMILY_LOGIC)) {
        fude_zoom_page_parts_load();
        const fude_sim_lib* _lib = fude_zoom_logic_library();
        const fude_sim_def* const* _d = (const fude_sim_def* const*)_lib->defs.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_lib->defs); _i++) {
            if(rde_strncmp(_d[_i]->id, "user/", 5u) == 0) {
                const u32 _item = FUDE_ZOOM_PAGE_CUSTOM_PART | _i;
                rde_arr_add(&_page->custom_items, (any)&_item);
            }
        }
    }
    const fude_zoom_pieces* _p = fude_zoom_page_pieces(_page);
    const u32 _families = _tabs & ((1u << FUDE_ZOOM_SYMBOL_FAMILIES) - 1u);
    for(u32 _i = 0; _i < _p->count; _i++) {
        if(_p->list[_i].families == 0u || (_p->list[_i].families & _families) != 0u) {
            rde_arr_add(&_page->custom_items, (any)&_i);
        }
    }
    return (u32)rde_arr_length(&_page->custom_items);
}

// Draws a custom part's picture in a box: its block (0.56 of the box wide), its inputs' stubs on its left, its
// outputs' on its right, at most 10 apart.
RDE_INTERNAL void fude_zoom_page_draw_block_in(const fude_sim_def* _def, fude_zoom_box _box, rde_color _ink, rde_color _paper) {
    u32 _ins, _outs;
    fude_zoom_page_part_ports(_def, &_ins, &_outs);
    const u32 _rows  = _ins > _outs ? (_ins > 0u ? _ins : 1u) : (_outs > 0u ? _outs : 1u);
    const f64 _pitch = rde_math_min_f64(10.0, (_box.max_y - _box.min_y) / (f64)(_rows + 1u));
    const f64 _w = (_box.max_x - _box.min_x) * 0.56, _h = _pitch * (f64)(_rows + 1u);
    const f64 _cx = (_box.min_x + _box.max_x) * 0.5, _cy = (_box.min_y + _box.max_y) * 0.5;
    const f64 _stub = (_box.max_x - _box.min_x - _w) * 0.5;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)_cx, (f32)_cy }, (rde_vec_2F){ (f32)_w, (f32)_h }, 0.08f, 6u, _paper, 1.2f, _ink, NULL);
    for(u32 _side = 0; _side < 2u; _side++) {
        const u32 _n = _side == 0u ? _ins : _outs;
        const f64 _x = _side == 0u ? _cx - _w * 0.5 : _cx + _w * 0.5;
        const f64 _top = _cy + _pitch * (f64)(_n - (_n > 0u ? 1u : 0u)) * 0.5;
        for(u32 _k = 0; _k < _n; _k++) {
            const f32 _y = (f32)(_top - _pitch * (f64)_k);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x, _y }, (rde_vec_2F){ (f32)(_side == 0u ? _x - _stub : _x + _stub), _y }, _ink, 1.2f);
        }
    }
}

// See page_parts.h. A tile's ⋯ (its card): its top right corner, 26 points square.
fude_zoom_box fude_zoom_page_custom_more(fude_zoom_box _tile) {
    return (fude_zoom_box){ _tile.max_x - 30.0, _tile.max_y - 30.0, _tile.max_x - 4.0, _tile.max_y - 4.0 };
}

// See page_parts.h. A Custom tile: its picture (a piece's lines, a part's block), its name under it, its ⋯.
void fude_zoom_page_custom_tile(fude_zoom_page* _page, u32 _item, fude_zoom_box _tile) {
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _picture = { _tile.min_x + 12.0, _tile.max_y - 64.0, _tile.max_x - 28.0, _tile.max_y - 12.0 };
    const c8* _name = "";
    if(_item & FUDE_ZOOM_PAGE_CUSTOM_PART) {
        const fude_sim_lib* _lib = fude_zoom_logic_library();
        const u32 _at = _item & ~FUDE_ZOOM_PAGE_CUSTOM_PART;
        if(_at >= (u32)rde_arr_length(&_lib->defs)) {
            return;
        }
        const fude_sim_def* _d = ((const fude_sim_def* const*)_lib->defs.memory)[_at];
        fude_zoom_page_draw_block_in(_d, _picture, _t->text, _t->page);
        _name = _d->name;
    } else {
        const fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
        if(_item >= _lib->count) {
            return;
        }
        fude_zoom_page_piece_picture(_page, _item, _picture, _t->text);
        _name = _lib->list[_item].name;
    }
    fude_zoom_render_text_in(&_page->renderer, _name, (u32)rde_strlen(_name),
                             (fude_zoom_box){ _tile.min_x + 3.0, _tile.min_y + 2.0, _tile.max_x - 3.0, _tile.max_y - 66.0 }, 11.0, true, _t->text);
    const fude_zoom_box _m = fude_zoom_page_custom_more(_tile);
    const rde_vec_2F _mc = { (f32)((_m.min_x + _m.max_x) * 0.5), (f32)((_m.min_y + _m.max_y) * 0.5) };
    rde_rendering_2d_draw_circle_with_border(_mc, 11.0f, 20u, _t->surface_2, 1.0f, _t->outline, NULL);
    fude_zoom_render_text_in(&_page->renderer, FUDE_ICON_MORE, (u32)rde_strlen(FUDE_ICON_MORE), _m, 13.0, false, _t->text);
}

// See page_parts.h. A Custom tile tapped: put down in the middle of the view (a piece as it was kept, a part's
// block), lassoed. Nothing is added while playing.
void fude_zoom_page_custom_tap(fude_zoom_page* _page, u32 _item) {
    if(_page->play_on) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PLAY_LOCKED));   // Playing: nothing added.
        return;
    }
    u8 _kind;
    c8 _key[FUDE_ZOOM_CUSTOM_KEY];
    if(!fude_zoom_page_custom_of(_page, _item, &_kind, _key)) {
        return;
    }
    if(_kind == FUDE_ZOOM_CUSTOM_PART) {
        const fude_sim_def* _d = fude_zoom_page_custom_def(_key);
        if(_d != NULL) {
            fude_zoom_page_part_put_at(_page, _d, 0.0, 0.0);
        }
    } else {
        fude_zoom_page_piece_put(_page, _item);
    }
}

// Declared ahead: a tile's card opens its template (defined with the templates, below).
RDE_INTERNAL void fude_zoom_page_template_open(fude_zoom_page* _page, u8 _kind, const c8* _key);

// Deleting it, sure: gone (Delete pressed on its card).
RDE_INTERNAL void fude_zoom_page_custom_delete_chosen(any _self, u32 _which) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(_which == 0u) {
        fude_zoom_page_custom_delete(_page, _page->custom_kind, _page->custom_key);
    }
}

// See page_parts.h. Its card's choice: 0 Edit or Inside (its template), 1 Rename (its name asked for on the text
// card), 2 Delete (asked again, on another choice card).
void fude_zoom_page_custom_chosen(any _self, u32 _which) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    const c8* _name = fude_zoom_page_custom_name(_page, _page->custom_kind, _page->custom_key);
    if(_name == NULL) {
        return;
    }
    if(_which == 0u) {
        _page->library_open = false;
        fude_zoom_page_template_open(_page, _page->custom_kind, _page->custom_key);
    } else if(_which == 1u && _page->text_field != NULL) {
        const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
        if(_bytes > 0) {
            rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
        }
        rde_ui_text_editor_insert_at(_page->text_field, 0, _name, rde_strlen(_name));
        rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_CUSTOM_NAME));
        _page->text_object = FUDE_ZOOM_NONE;
        _page->text_style  = FUDE_ZOOM_PAGE_TEXT_CUSTOM;
        fude_zoom_page_text_card(_page, true);
        rde_ui_text_editor_select_all(_page->text_field);
    } else if(_which == 2u) {
        c8 _title[200];
        FUDE_TEXTF(_title, FUDE_TEXT_ZOOM_CUSTOM_DELETE_SURE, FUDE_TS(_name));
        const c8* _labels[1] = { fude_text(FUDE_TEXT_DELETE) };
        const c8* _icons[1]  = { FUDE_ICON_TRASH };
        fude_zoom_choice_open(&_page->choice_form, _title, _labels, _icons, 1u, fude_zoom_page_custom_delete_chosen, _page);
    }
}

// See page_parts.h. The item is kept in custom_kind and custom_key for the choice.
void fude_zoom_page_custom_card(fude_zoom_page* _page, u32 _item) {
    if(!fude_zoom_page_custom_of(_page, _item, &_page->custom_kind, _page->custom_key)) {
        return;
    }
    const c8* _name = fude_zoom_page_custom_name(_page, _page->custom_kind, _page->custom_key);
    const b8  _part = _page->custom_kind == FUDE_ZOOM_CUSTOM_PART;
    const c8* _labels[3] = { fude_text(_part ? FUDE_TEXT_ZOOM_CUSTOM_INSIDE : FUDE_TEXT_ZOOM_CUSTOM_EDIT), fude_text(FUDE_TEXT_ZOOM_CUSTOM_RENAME), fude_text(FUDE_TEXT_DELETE) };
    const c8* _icons[3]  = { _part ? FUDE_ICON_FOLDER_OPEN : FUDE_ICON_NOTE_EDIT, FUDE_ICON_TEXT, FUDE_ICON_TRASH };
    fude_zoom_choice_open(&_page->choice_form, _name != NULL ? _name : "", _labels, _icons, 3u, fude_zoom_page_custom_chosen, _page);
}

// See page_parts.h. Once a frame: a hand held still on a Custom tile for FUDE_ZOOM_PAGE_CUSTOM_HOLD seconds, its card;
// what that hand does next is the panel's no more.
void fude_zoom_page_library_hold(fude_zoom_page* _page) {
    if(!_page->library_open || _page->library_hand == 0u || _page->library_dragged || _page->library_family != FUDE_ZOOM_PAGE_LIBRARY_CUSTOM) {
        return;
    }
    if(rde_engine_get_time_now() - _page->library_down_at < FUDE_ZOOM_PAGE_CUSTOM_HOLD) {
        fude_redraw_after(FUDE_ZOOM_PAGE_CUSTOM_HOLD - (rde_engine_get_time_now() - _page->library_down_at));   // (render on demand: a timer)
        return;
    }
    fude_zoom_page_library_layout _l;
    fude_zoom_page_library_lay(_page, &_l);
    const u32 _item = fude_zoom_page_library_tile(_page, &_l, _page->library_down, NULL);
    _page->library_gone = _page->library_hand;
    _page->library_hand = 0u;
    if(_item != FUDE_ZOOM_NONE) {
        fude_zoom_page_custom_card(_page, _item);
    }
}

// See page_parts.h. Its new name (the card's first line, trimmed; none: as it was): the piece's or the part's (its file
// written again), its template canvas's. When its file could not be written (logged where it was), its old name is put
// back, in memory as on disk, so it is not one name now and another the next time; that is said.
void fude_zoom_page_custom_rename(fude_zoom_page* _page, const c8* _text) {
    c8 _name[FUDE_ZOOM_PIECE_NAME];
    const c8* _a = _text;
    while(*_a == ' ' || *_a == '\n') {
        _a++;
    }
    usize _len = rde_strcspn(_a, "\n");
    while(_len > 0 && _a[_len - 1u] == ' ') {
        _len--;
    }
    // A part's name is shorter; either is cut at a whole character.
    const usize _most = (_page->custom_kind == FUDE_ZOOM_CUSTOM_PART ? FUDE_SIM_NAME : sizeof(_name)) - 1u;
    if(_len > _most) {
        _len = _most;
        while(_len > 0 && ((u8)_a[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    if(_len == 0) {
        return;
    }
    rde_memcpy(_name, _a, _len);
    _name[_len] = 0;
    const c8* _key = _page->custom_key;
    const c8* _was = fude_zoom_page_custom_name(_page, _page->custom_kind, _key);
    if(_was == NULL) {
        return;
    }
    c8 _old[FUDE_ZOOM_PIECE_NAME];
    rde_snprintf(_old, sizeof(_old), "%s", _was);
    b8 _saved = true;   // (a part's or a piece's file written again: when not, its old name back, and said)
    if(_page->custom_kind == FUDE_ZOOM_CUSTOM_PART) {
        for(c8* _c = _name; *_c != 0; _c++) {
            *_c = *_c == '"' ? '\'' : *_c;   // A part's text keeps its name between quotes.
        }
        const fude_sim_def* _d = fude_zoom_page_custom_def(_key);
        if(_d == NULL || !fude_zoom_logic_rename(_key, _name)) {
            return;
        }
        fude_bytes _out = fude_bytes_new(1024u);
        fude_sim_def_write(_d, &_out);
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_part_path(_key, ".part", _path, sizeof(_path));
        _saved = fude_bytes_write_and_free(&_out, _path, NULL);   // (logged when not)
        if(!_saved) {
            fude_zoom_logic_rename(_key, _old);   // (its file still has the old name)
        }
    } else {
        const u32 _i = fude_zoom_page_custom_piece(_page, _key);
        if(_i == FUDE_ZOOM_NONE) {
            return;
        }
        fude_zoom_pieces* _pieces = fude_zoom_page_pieces(_page);
        _saved = fude_zoom_pieces_rename(_pieces, fude_zoom_page_pieces_dir(), _i, _name);   // (logged when not)
        if(!_saved) {
            // Its file still has the old name: the list's put back without writing again (that write failed).
            rde_snprintf(_pieces->list[_i].name, sizeof(_pieces->list[_i].name), "%s", _old);
        }
        fude_zoom_page_pieces_stale(_page);
    }
    if(!_saved) {
        c8 _say[200];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_CUSTOM_NOT_RENAMED, FUDE_TS(_old));
        fude_notice_show(_say);
        return;
    }
    const u32 _canvas = fude_zoom_templates_canvas(fude_zoom_page_templates(_page), _page->custom_kind, _key);
    if(_canvas != 0u && fude_notes_find(_page->app->notes, _canvas) != NULL) {
        fude_notes_rename(_page->app->notes, _canvas, _name);
    }
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_CUSTOM_RENAMED, FUDE_TS(_name));
    fude_notice_show(_say);
}

// _kind's _key's template let go: no one's, its canvas deleted (unless it is the one open: then a canvas as any
// other).
RDE_INTERNAL void fude_zoom_page_template_drop(fude_zoom_page* _page, u8 _kind, const c8* _key) {
    const u32 _canvas = fude_zoom_templates_drop(fude_zoom_page_templates(_page), _kind, _key);
    if(_canvas == 0u) {
        return;
    }
    fude_zoom_page_templates_save(_page);
    if(_canvas == _page->canvas) {
        _page->template_on = false;
    } else if(fude_notes_find(_page->app->notes, _canvas) != NULL) {
        fude_notes_remove(_page->app->notes, _canvas);
    }
}

// See page_parts.h. _kind's _key deleted: the piece (its file), the part (forgotten: what is drawn of it holds it no
// more; its files), its template canvas (unless it is the one open: then a canvas as any other).
void fude_zoom_page_custom_delete(fude_zoom_page* _page, u8 _kind, const c8* _key) {
    const c8* _was = fude_zoom_page_custom_name(_page, _kind, _key);
    if(_was == NULL) {
        return;
    }
    c8 _name[FUDE_ZOOM_PIECE_NAME], _k[FUDE_ZOOM_CUSTOM_KEY];
    rde_snprintf(_name, sizeof(_name), "%s", _was);
    rde_snprintf(_k, sizeof(_k), "%s", _key);   // _key may be the page's own, written over below.
    if(_kind == FUDE_ZOOM_CUSTOM_PART) {
        fude_zoom_logic_forget_part(_k);
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_part_path(_k, ".part", _path, sizeof(_path));
        rde_file_delete(_path);
        fude_zoom_page_part_path(_k, ".piece", _path, sizeof(_path));
        if(rde_file_exists(_path)) {
            rde_file_delete(_path);
        }
    } else {
        fude_zoom_pieces_remove(fude_zoom_page_pieces(_page), fude_zoom_page_pieces_dir(), fude_zoom_page_custom_piece(_page, _k));
        fude_zoom_page_pieces_stale(_page);
    }
    fude_zoom_page_template_drop(_page, _kind, _k);
    c8 _say[200];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_CUSTOM_DELETED, FUDE_TS(_name));
    fude_notice_show(_say);
}

// See page_parts.h. The canvas open now: a template (whose, from the templates' list), or not; then Back is the app's
// again. Its revision as of now (left changed: what it holds kept). _say: what it is, said.
void fude_zoom_page_template_opened(fude_zoom_page* _page, b8 _say) {
    const fude_zoom_template* _e = fude_zoom_templates_of(fude_zoom_page_templates(_page), _page->canvas);
    const c8* _name = _e != NULL ? fude_zoom_page_custom_name(_page, _e->kind, _e->key) : NULL;
    _page->template_on = _name != NULL;
    if(_page->template_return_for != _page->canvas) {
        _page->template_return = 0u;   // Not opened from a canvas: Back is the app's.
    }
    if(!_page->template_on) {
        return;
    }
    _page->template_kind = _e->kind;
    rde_snprintf(_page->template_key, sizeof(_page->template_key), "%s", _e->key);
    _page->template_seen = _page->scene.revision;
    if(_say) {
        c8 _line[300];
        FUDE_TEXTF(_line, _e->kind == FUDE_ZOOM_CUSTOM_PART ? FUDE_TEXT_ZOOM_TEMPLATE_EDITING_PART : FUDE_TEXT_ZOOM_TEMPLATE_EDITING_PIECE, FUDE_TS(_name));
        fude_notice_show(_line);
    }
}

// _kind's _key's template opened: its canvas — made the first time, in the folder Custom (filled once it is open).
RDE_INTERNAL void fude_zoom_page_template_open(fude_zoom_page* _page, u8 _kind, const c8* _key) {
    fude_app*   _app  = _page->app;
    const c8*   _name = fude_zoom_page_custom_name(_page, _kind, _key);
    if(_name == NULL || _page->template_making != 0u) {
        return;
    }
    fude_zoom_templates* _t = fude_zoom_page_templates(_page);
    const u32 _canvas = fude_zoom_templates_canvas(_t, _kind, _key);
    const fude_note* _note = _canvas != 0u ? fude_notes_find(_app->notes, _canvas) : NULL;
    const u32 _from = _page->canvas;
    if(_note != NULL && _note->kind == FUDE_NOTE_CANVAS) {
        if(_canvas != _from) {
            _page->template_return     = _from;
            _page->template_return_for = _canvas;
            fude_notes_open(_app->notes, _canvas);
        }
        return;
    }
    c8 _title[FUDE_NOTE_NAME];
    rde_snprintf(_title, sizeof(_title), "%s", _name);
    c8 _k[FUDE_ZOOM_CUSTOM_KEY];
    rde_snprintf(_k, sizeof(_k), "%s", _key);   // _key may be the page's own.
    _page->template_making_kind   = _kind;
    rde_snprintf(_page->template_making_key, sizeof(_page->template_making_key), "%s", _k);
    _page->template_making_return = _from;
    // A canvas as any other (its paper the app's), in the folder Custom.
    const u32 _id = fude_notes_add(_app->notes, FUDE_NOTE_CANVAS, fude_zoom_page_top_folder(_page, fude_text(FUDE_TEXT_ZOOM_LIB_CUSTOM)), _title);
    if(_id == 0u) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "custom: the template's canvas for \"%s\" could not be made", _title);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_CANVAS_NOT_MADE));
        return;
    }
    fude_zoom_templates_set(_t, _kind, _k, _id);
    fude_zoom_page_templates_save(_page);
    _page->template_making     = _id;
    _page->template_return     = _from;
    _page->template_return_for = _id;
    fude_notes_open(_app->notes, _id);
}

// See page_parts.h. Once a frame: the template being made, filled once its canvas is open: the piece put on it, or the
// part's drawing (as it was made) or its inside drawn from what it is; then the view out to all of it.
void fude_zoom_page_template_follow(fude_zoom_page* _page) {
    if(_page->template_making != 0u) {
        fude_redraw_changed();   // (render on demand: its canvas opens a frame or two on)
    }
    if(_page->template_making == 0u || !_page->open || _page->canvas != _page->template_making) {
        return;
    }
    _page->template_making = 0u;
    const c8* _key = _page->template_making_key;
    const u32 _f   = fude_zoom_page_middle_frame(_page);
    if(_page->template_making_kind == FUDE_ZOOM_CUSTOM_PART) {
        const fude_sim_def* _d = fude_zoom_page_custom_def(_key);
        fude_zoom_piece _drawn;
        rde_memset(&_drawn, 0, sizeof(_drawn));
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_part_path(_key, ".piece", _path, sizeof(_path));
        if(_d == NULL) {
            // Gone since: nothing to draw.
        } else if(rde_file_exists(_path) && fude_zoom_piece_load(&_drawn, _path)) {
            fude_zoom_page_put_piece(_page, &_drawn);
        } else {
            rde_arr _born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
            fude_zoom_placer _p = fude_zoom_placer_make(&_page->scene, _f, fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _f)), FUDE_THEME_INK, &_born);
            fude_zoom_logic_draw(_d, &_p);
            fude_zoom_page_made_step(_page, _f, &_born);
            rde_arr_free(&_born);
        }
        fude_zoom_piece_clear(&_drawn);
        _page->topic = FUDE_ZOOM_TOPIC_ELECTRONICS;
    } else {
        const u32 _i = fude_zoom_page_custom_piece(_page, _key);
        if(_i != FUDE_ZOOM_NONE) {
            const u32 _families = fude_zoom_page_pieces(_page)->list[_i].families;
            fude_zoom_page_piece_put(_page, _i);
            if(_families & (1u << FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS)) {
                _page->topic = FUDE_ZOOM_TOPIC_MECHANISMS;
            } else if(_families & FUDE_ZOOM_PAGE_LIBRARY_CIRCUITS & ~FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT) {
                _page->topic = FUDE_ZOOM_TOPIC_ELECTRONICS;
            }
        }
    }
    fude_zoom_page_tools_save(_page);
    fude_zoom_box _sb;
    if(fude_zoom_select_box(&_page->selection, &_sb)) {
        // What was put down, lassoed: its box on the screen, into the frame's units.
        fude_zoom_page_view_out_to(_page, _f, fude_zoom_sim_box(fude_zoom_sim_inverse(fude_zoom_page_frame_sim(_page, _f)), _sb));
    } else {
        fude_zoom_page_view_out_to(_page, _f, fude_zoom_map_contents(&_page->scene));
    }
    fude_zoom_select_clear(&_page->selection);
    _page->template_return     = _page->template_making_return;
    _page->template_return_for = _page->canvas;
    _page->map_stale = true;
    fude_zoom_page_template_opened(_page, true);
}

// See page_parts.h. A template kept (its canvas closing, the app put away): what it holds now, if it changed since,
// kept as its piece, or made its part again (its pins where they were: what is wired to them on canvases still on
// them), its drawing beside it. What the lasso holds is left as it was.
void fude_zoom_page_template_keep(fude_zoom_page* _page) {
    if(!_page->template_on || !_page->open || _page->scene.revision == _page->template_seen) {
        return;   // As it was.
    }
    _page->template_seen = _page->scene.revision;
    fude_zoom_scene* _s = &_page->scene;
    const c8* _key = _page->template_key;
    c8 _say[300];
    // All of it, held by a selection of its own.
    fude_zoom_selection _all;
    fude_zoom_select_init(&_all);
    fude_zoom_select_all(&_all, _s, &_page->renderer);
    if(_page->template_kind == FUDE_ZOOM_CUSTOM_PART) {
        const fude_sim_def* _old = fude_zoom_page_custom_def(_key);
        if(_old == NULL) {
            fude_zoom_select_destroy(&_all);
            return;
        }
        c8 _name[FUDE_SIM_NAME];
        rde_snprintf(_name, sizeof(_name), "%s", _old->name);
        fude_zoom_circuit _c;
        fude_zoom_circuit_init(&_c);
        fude_zoom_circuit_build(&_c, _s);
        u32 _problem = FUDE_ZOOM_MAKE_OK;
        fude_sim_def* _def = fude_zoom_logic_make(&_c, _s, _key, _name, &_problem);
        fude_zoom_circuit_destroy(&_c);
        if(_def == NULL) {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_TEMPLATE_NOT_UPDATED, FUDE_TS(_name));
            fude_notice_show(_say);
            fude_zoom_select_destroy(&_all);
            return;
        }
        fude_zoom_logic_keep_order(_def, _old);
        u32 _ins, _outs;
        fude_zoom_page_part_ports(_def, &_ins, &_outs);
        // Learnt only if what it is changed (its text compared, at the old version): an edit undone, a part moved,
        // leaves every one of it as it was.
        _def->version = _old->version;
        fude_bytes _a = fude_bytes_new(1024u), _b = fude_bytes_new(1024u);
        fude_sim_def_write(_old, &_a);
        fude_sim_def_write(_def, &_b);
        const b8 _same = fude_bytes_size(&_a) == fude_bytes_size(&_b) && rde_memcmp(_a.memory, _b.memory, fude_bytes_size(&_a)) == 0;
        rde_arr_free(&_a);
        rde_arr_free(&_b);
        b8 _saved = true;
        if(!_same) {
            fude_zoom_page_part_keep(_def, &_saved);
        }
        fude_sim_def_free(_def);
        _saved = fude_zoom_page_part_drawing(_page, &_all, _key, _name) && _saved;
        if(_saved) {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_TEMPLATE_PART_UPDATED, FUDE_TS(_name), FUDE_TN(_ins), FUDE_TN(_outs));
            fude_notice_show(_say);
        } else {
            fude_zoom_page_part_not_saved(_name);
        }
    } else if(fude_zoom_page_custom_piece(_page, _key) != FUDE_ZOOM_NONE) {
        const u32 _i = fude_zoom_page_custom_piece(_page, _key);
        fude_zoom_pieces* _lib = fude_zoom_page_pieces(_page);
        rde_arr _clips = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
        fude_zoom_box _box;
        // Nothing left on it: the piece as it was (Delete on its card lets it go). Its file not written (logged by
        // fude_bytes_write_and_free): the piece is as it is now until the app closes, and as it was after; said so.
        if(fude_zoom_page_held_clips(_page, &_all, &_clips, &_box)) {
            const b8 _kept = fude_zoom_pieces_replace(_lib, fude_zoom_page_pieces_dir(), _i, (const fude_zoom_clip*)_clips.memory, (u32)rde_arr_length(&_clips), _box);
            FUDE_TEXTF(_say, _kept ? FUDE_TEXT_ZOOM_TEMPLATE_PIECE_UPDATED : FUDE_TEXT_ZOOM_TEMPLATE_PIECE_NOT_SAVED, FUDE_TS(_lib->list[_i].name));
            fude_notice_show(_say);
        }
        fude_zoom_select_clips_free(&_clips);
        rde_arr_free(&_clips);
        fude_zoom_page_pieces_stale(_page);
    }
    fude_zoom_select_destroy(&_all);
}

// See page_parts.h. A template left (its canvas closing): kept, a canvas no more.
void fude_zoom_page_template_leave(fude_zoom_page* _page) {
    fude_zoom_page_template_keep(_page);
    _page->template_on = false;
}

// Returns the definition of the custom part of a person's own that object _o is (NULL: not one), found by its text.
RDE_INTERNAL const fude_sim_def* fude_zoom_page_custom_part_of(const fude_zoom_page* _page, u32 _o) {
    if(_o >= fude_zoom_scene_object_count(&_page->scene) || !fude_zoom_part_custom(fude_zoom_part_of(&_page->scene, _o))) {
        return NULL;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    c8  _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS), _text, sizeof(_text));
    const fude_sim_def* _d = fude_zoom_logic_find(_text);
    return _d != NULL && rde_strncmp(_d->id, "user/", 5u) == 0 ? _d : NULL;
}

// See page_parts.h. The custom part the lasso holds alone: its definition (NULL: none, or it holds more).
const fude_sim_def* fude_zoom_page_lone_custom(const fude_zoom_page* _page) {
    return rde_arr_length(&_page->selection.picks) == 1u ? fude_zoom_page_custom_part_of(_page, ((const fude_zoom_pick*)_page->selection.picks.memory)[0].object) : NULL;
}

// _def's template opened (its id copied first: the library may change under it).
RDE_INTERNAL void fude_zoom_page_inside(fude_zoom_page* _page, const fude_sim_def* _def) {
    c8 _id[FUDE_ZOOM_CUSTOM_KEY];
    rde_snprintf(_id, sizeof(_id), "%s", _def->id);
    fude_zoom_select_clear(&_page->selection);
    fude_zoom_page_template_open(_page, FUDE_ZOOM_CUSTOM_PART, _id);
}

// See page_parts.h. The lasso's row: Inside, a custom part's template (what it is made of: to see, to change).
void fude_zoom_page_inside_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return;
    }
    const fude_sim_def* _d = fude_zoom_page_lone_custom(_page);
    if(_d != NULL) {
        fude_zoom_page_inside(_page, _d);
    }
}

// See page_parts.h. The page's menu: Inside, on a custom part (the one the lasso holds alone when it opened on the
// selection, else the one it opened on).
const fude_sim_def* fude_zoom_page_context_custom(const fude_zoom_page* _page) {
    return _page->context_held ? fude_zoom_page_lone_custom(_page) : fude_zoom_page_custom_part_of(_page, _page->context_target);
}

// See page_parts.h. _self is the page's menu, closed first.
void fude_zoom_page_context_inside(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_pagemenu_close_context((fude_pagemenu*)_self);
    fude_zoom_page* _page = fude_zoom_page_the;
    const fude_sim_def* _d = _page != NULL && _page->open ? fude_zoom_page_context_custom(_page) : NULL;
    if(_d != NULL) {
        fude_zoom_page_inside(_page, _d);
    }
}
