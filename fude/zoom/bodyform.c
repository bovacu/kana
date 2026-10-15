// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/bodyform.h"
#include "sim/body.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"

// ===========================================================================
// See bodyform.h.
//
// The card is the kit's form (kit.h: its title, Cancel, a tap off it, laying it out again). Its widgets are made once
// and shown again for each drawing: the chips show what is chosen, and the label under them its mass, both written
// again by fude_zoom_bodyf_show whenever a choice changes. Its own functions and constants are prefixed
// fude_zoom_bodyf_ and FUDE_ZOOM_BODYF_. Sizes are UI points.
// ===========================================================================

#define FUDE_ZOOM_BODYF_CHIP 44.0f   // A chip's height.

// The materials' names, in sim/body.h's order.
RDE_INTERNAL const FUDE_TEXT_ FUDE_ZOOM_BODYF_NAMES[FUDE_ZOOM_BODY_MATERIALS] = {
    FUDE_TEXT_ZOOM_MAT_WOOD, FUDE_TEXT_ZOOM_MAT_STEEL, FUDE_TEXT_ZOOM_MAT_ALUMINIUM, FUDE_TEXT_ZOOM_MAT_RUBBER,
    FUDE_TEXT_ZOOM_MAT_PLASTIC, FUDE_TEXT_ZOOM_MAT_GLASS, FUDE_TEXT_ZOOM_MAT_ICE, FUDE_TEXT_ZOOM_MAT_FOAM,
};

// See the header.
const c8* fude_zoom_body_material_name(u32 _material) {
    return _material < FUDE_ZOOM_BODY_MATERIALS ? fude_text(FUDE_ZOOM_BODYF_NAMES[_material]) : NULL;
}

// Shows what is chosen: the material's chip and moving or fixed lit, and the mass line under them. The mass is in kg
// from 1 kg up, in g below, to 3 significant figures; it is left empty when the area is not known.
RDE_INTERNAL void fude_zoom_bodyf_show(fude_zoom_body_form* _form) {
    for(u32 _i = 0; _i < FUDE_ZOOM_BODY_MATERIALS; _i++) {
        fude_kit_button_chip(_form->materials[_i], _i == _form->material);
    }
    fude_kit_button_chip(_form->kinds[0], !_form->fixed);
    fude_kit_button_chip(_form->kinds[1], _form->fixed);
    // Its mass as its material makes it (density in kg/m3, the area taken to square metres, times the plate's
    // thickness: sim/body.h), or that it is the ground.
    c8 _say[96];
    const fude_sim_material* _m = fude_sim_material_at(_form->material);
    if(_form->fixed) {
        rde_snprintf(_say, sizeof(_say), "%s", fude_text(FUDE_TEXT_ZOOM_BODY_GROUND));
    } else if(_m != NULL && _form->area_mm2 > 0.0) {
        const f64 _kg = _m->density * _form->area_mm2 * 1e-6 * FUDE_SIM_BODY_THICKNESS;
        c8 _mass[32];
        if(_kg >= 1.0) {
            rde_snprintf(_mass, sizeof(_mass), "%.3g kg", _kg);
        } else {
            rde_snprintf(_mass, sizeof(_mass), "%.3g g", _kg * 1000.0);
        }
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_BODY_MASS, FUDE_TS(_mass));
    } else {
        _say[0] = 0;
    }
    rde_ui_label_set_text(_form->mass, _say);
}

// A material's chip: chooses it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_bodyf_on_material(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_body_ref* _ref = (const fude_zoom_body_ref*)_user_data;
    _ref->form->material = _ref->index;
    fude_zoom_bodyf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Moving (index 0) or fixed (index 1): chooses it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_bodyf_on_kind(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_zoom_body_ref* _ref = (const fude_zoom_body_ref*)_user_data;
    _ref->form->fixed = _ref->index == 1u;
    fude_zoom_bodyf_show(_ref->form);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Apply or Not a body (_remove): puts the card away, then hands the choice to the opener's done. The card goes first,
// so a callback that opens it again leaves it up.
RDE_INTERNAL void fude_zoom_bodyf_finish(fude_zoom_body_form* _form, b8 _remove) {
    const fude_zoom_body_done _done = _form->done;
    any const _self = _form->self;
    const u32 _material = _form->material;
    const b8 _fixed = _form->fixed;
    fude_zoom_body_close(_form);
    if(_done != NULL) {
        _done(_self, _material, _fixed, _remove);
    }
}

// Apply: makes it a body as chosen.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_bodyf_on_apply(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_bodyf_finish((fude_zoom_body_form*)_user_data, false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Not a body: hands back that it is to be a body no more.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_bodyf_on_remove(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_bodyf_finish((fude_zoom_body_form*)_user_data, true);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header. Each chip is handed its own ref (the form and its index) as its user data. Apply's words are the
// size card's.
void fude_zoom_body_build(fude_zoom_body_form* _form, rde_ui_node* _root, rde_window* _window, rde_font* _font) {
    rde_memset(_form, 0, sizeof(*_form));
    rde_ui_node* _card = fude_kit_form_build(&_form->card, _root, _window, _font, fude_text(FUDE_TEXT_ZOOM_BODY));
    _form->mass = fude_kit_form_label(&_form->card, _card, NULL, 14.0f);
    for(u32 _i = 0; _i < FUDE_ZOOM_BODY_MATERIALS; _i++) {
        _form->material_refs[_i] = (fude_zoom_body_ref){ _form, _i };
        _form->materials[_i] = fude_kit_button(_card, fude_text(FUDE_ZOOM_BODYF_NAMES[_i]), fude_zoom_bodyf_on_material, &_form->material_refs[_i]);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _form->kind_refs[_i] = (fude_zoom_body_ref){ _form, _i };
        _form->kinds[_i] = fude_kit_button(_card, fude_text(_i == 0u ? FUDE_TEXT_ZOOM_BODY_MOVING : FUDE_TEXT_ZOOM_BODY_FIXED), fude_zoom_bodyf_on_kind, &_form->kind_refs[_i]);
    }
    _form->remove = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_BODY_REMOVE), fude_zoom_bodyf_on_remove, _form);
    _form->cancel = fude_kit_button(_card, fude_text(FUDE_TEXT_CANCEL), fude_kit_form_on_cancel, &_form->card);
    _form->apply  = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_SIZE_APPLY), fude_zoom_bodyf_on_apply, _form);
    fude_zoom_body_restyle(_form);
    fude_kit_modal_show(&_form->card.modal, false);
}

// See the header: with the card NULL, every other call does nothing.
void fude_zoom_body_forget(fude_zoom_body_form* _form) {
    rde_memset(_form, 0, sizeof(*_form));
}

// See the header: false before the card is built and after it is forgotten.
b8 fude_zoom_body_shown(const fude_zoom_body_form* _form) {
    return fude_kit_form_shown(&_form->card);
}

// See the header. The chips take their colours from what is chosen, so they are styled by fude_zoom_bodyf_show.
void fude_zoom_body_restyle(fude_zoom_body_form* _form) {
    if(!fude_kit_form_restyle(&_form->card)) {
        return;
    }
    rde_ui_label_set_color(_form->mass, fude_theme_active()->text_soft);
    fude_kit_button_quiet(_form->remove);
    fude_kit_button_quiet(_form->cancel);
    fude_kit_button_primary(_form->apply);
    fude_zoom_bodyf_show(_form);
}

// Lays the card out in the middle of the screen, at most 460 points wide (16 less than the screen on each side): the
// title, the materials in two rows of four, moving and fixed, the mass, and at the bottom Not a body (only for a body
// already), Cancel and Apply.
RDE_INTERNAL void fude_zoom_bodyf_layout(fude_zoom_body_form* _form) {
    const rde_vec_2F _screen = fude_kit_screen_size(_form->card.window);
    const f32 _pad = FUDE_KIT_FORM_PAD, _gap = FUDE_KIT_FORM_GAP, _ch = FUDE_ZOOM_BODYF_CHIP;
    const f32 _w  = rde_math_min_f32(460.0f, _screen.x - 32.0f);
    const f32 _iw = _w - 2.0f * _pad;
    const f32 _h  = _pad + 28.0f + _gap + 2.0f * (_ch + _gap) + _gap + (_ch + _gap) + 24.0f + _gap + 48.0f + _pad;
    f32 _y = fude_kit_form_place(&_form->card, _screen, FUDE_KIT_FORM_MIDDLE, _w, _h);
    // Its materials, four a row; then moving or fixed; then its mass.
    const f32 _cw = (_iw - 3.0f * _gap) / 4.0f;
    for(u32 _i = 0; _i < FUDE_ZOOM_BODY_MATERIALS; _i++) {
        const f32 _x = _pad + (f32)(_i % 4u) * (_cw + _gap) + _cw * 0.5f;
        const f32 _cy = _y - (f32)(_i / 4u) * (_ch + _gap) - _ch * 0.5f;
        fude_kit_place(rde_ui_button_as_node(_form->materials[_i]), (rde_vec_2F){ _x, _cy }, (rde_vec_2F){ _cw, _ch });
    }
    _y -= 2.0f * (_ch + _gap) + _gap;
    const f32 _kw = (_iw - _gap) / 2.0f;
    for(u32 _i = 0; _i < 2u; _i++) {
        fude_kit_place(rde_ui_button_as_node(_form->kinds[_i]), (rde_vec_2F){ _pad + (f32)_i * (_kw + _gap) + _kw * 0.5f, _y - _ch * 0.5f }, (rde_vec_2F){ _kw, _ch });
    }
    _y -= _ch + _gap;
    fude_kit_place(rde_ui_label_as_node(_form->mass), (rde_vec_2F){ _pad + _iw * 0.5f, _y - 12.0f }, (rde_vec_2F){ _iw, 24.0f });
    const f32 _bw = (_iw - 2.0f * _gap) / 3.0f;
    rde_ui_node_set_active(rde_ui_button_as_node(_form->remove), _form->was_body);
    fude_kit_place(rde_ui_button_as_node(_form->remove), (rde_vec_2F){ _pad + _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->cancel), (rde_vec_2F){ _pad + _bw + _gap + _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_form->apply), (rde_vec_2F){ _w - _pad - _bw * 0.5f, _pad + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
}

// See the header. The card's opening time is kept for the tap off it.
void fude_zoom_body_open(fude_zoom_body_form* _form, u32 _material, b8 _fixed, b8 _was_body, f64 _area_mm2, fude_zoom_body_done _done, any _self) {
    if(_form->card.modal.card == NULL) {
        return;
    }
    _form->material = _material < FUDE_ZOOM_BODY_MATERIALS ? _material : 0u;
    _form->fixed    = _fixed;
    _form->was_body = _was_body;
    _form->area_mm2 = _area_mm2;
    _form->done     = _done;
    _form->self     = _self;
    fude_kit_form_show(&_form->card);
    fude_zoom_bodyf_layout(_form);
    fude_zoom_body_restyle(_form);
}

// See the header.
void fude_zoom_body_close(fude_zoom_body_form* _form) {
    fude_kit_form_close(&_form->card);
}

// See the header: lays the card out again only while it is up and the screen's size has changed.
void fude_zoom_body_update(fude_zoom_body_form* _form) {
    if(fude_kit_form_resized(&_form->card)) {
        fude_zoom_bodyf_layout(_form);
    }
}

// See the header. A material out of range is taken as the first, as fude_zoom_body_open takes it. Not a body is
// pressed even when the card does not show it.
void fude_zoom_body_look(fude_zoom_body_form* _form, u32 _material, b8 _fixed, u32 _finish) {
    if(!fude_zoom_body_shown(_form)) {
        return;
    }
    _form->material = _material < FUDE_ZOOM_BODY_MATERIALS ? _material : 0u;
    _form->fixed    = _fixed;
    fude_zoom_bodyf_show(_form);
    if(_finish != 0u) {
        fude_zoom_bodyf_finish(_form, _finish == 2u);
    }
}
