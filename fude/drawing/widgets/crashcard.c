// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/crashcard.h"
#include "drawing/app/ui.h"
#include "drawing/app/info.h"
#include "drawing/app/look.h"
#include "drawing/base/crash.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/icons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FUDE_CRASHCARD_W 520.0f
#define FUDE_CRASHCARD_H 260.0f
#define FUDE_CRASHCARD_PAD 24.0f

RDE_INTERNAL rde_ui_label* fude_crashcard_label(fude_ui* _ui, rde_ui_node* _parent, const c8* _text, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _ui->font);
    rde_ui_label_set_text(_label, _text);
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_TOP);
    rde_ui_label_set_wrap(_label, true);
    rde_ui_label_set_auto_fit(_label, true);   // (a longer language smaller, never cut)
    rde_ui_label_set_auto_fit_min_scale(_label, 0.6f);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

RDE_INTERNAL void fude_crashcard_close(fude_ui* _ui) {
    fude_crash_forget();
    _ui->crash.open = false;
    fude_kit_modal_show(&_ui->crash.modal, false);
}

// Send report: an email to write to, the report in it (cut to what a mailto: link carries), then gone.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_crashcard_on_send(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui* _ui = (fude_ui*)_user_data;
    static c8 _report[8192], _url[6000];
    if(fude_crash_pending(_report, sizeof(_report))) {
        c8 _subject[160];
        snprintf(_subject, sizeof(_subject), "%s %s: a crash report", _ui->app->info->name, _ui->app->info->version);
        fude_crash_mailto(_report, FUDE_APP_CONTACT, _subject, _url, sizeof(_url));
        rde_engine_open_url(_url);
    }
    fude_crashcard_close(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_crashcard_on_later(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_crashcard_close((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

void fude_crashcard_create(fude_ui* _ui, rde_ui_node* _root) {
    fude_crashcard* _c = &_ui->crash;
    const b8 _asked = _c->asked, _open = _c->open;
    memset(_c, 0, sizeof(*_c));
    _c->asked = _asked;   // (built again for another language: as it was)
    _c->open  = _open;
    fude_kit_modal_create(&_c->modal, _root, fude_crashcard_on_later, _ui);
    rde_ui_node* _card = rde_ui_image_as_node(_c->modal.card);
    c8 _title[256];
    FUDE_TEXTF(_title, FUDE_TEXT_CRASH_TITLE, FUDE_TS(_ui->app->info->name));
    _c->title = fude_crashcard_label(_ui, _card, _title, 22.0f);
    _c->text  = fude_crashcard_label(_ui, _card, fude_text(FUDE_TEXT_CRASH_TEXT), 16.0f);
    _c->later = fude_kit_button(_card, fude_text(FUDE_TEXT_CRASH_LATER), fude_crashcard_on_later, _ui);
    _c->send  = fude_kit_button(_card, fude_text(FUDE_TEXT_CRASH_SEND), fude_crashcard_on_send, _ui);
    fude_kit_icon(_c->send, FUDE_ICON_SEND, FUDE_KIT_ICON_LEFT, 15.0f);
    fude_crashcard_restyle(_ui);
    fude_kit_modal_show(&_c->modal, false);
    if(_c->open) {
        fude_kit_modal_show(&_c->modal, true);
    }
}

void fude_crashcard_forget(fude_ui* _ui) {
    const b8 _asked = _ui->crash.asked, _open = _ui->crash.open;
    memset(&_ui->crash, 0, sizeof(_ui->crash));
    _ui->crash.asked = _asked;
    _ui->crash.open  = _open;
}

void fude_crashcard_restyle(fude_ui* _ui) {
    fude_crashcard* _c = &_ui->crash;
    if(_c->send == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    fude_kit_modal_restyle(&_c->modal, 20.0f);
    rde_ui_label_set_color(_c->title, _t->text);
    rde_ui_label_set_color(_c->text, _t->text_soft);
    fude_kit_button_primary(_c->send);
    fude_kit_restyle_quiet(_c->later);
}

b8 fude_crashcard_shown(const fude_ui* _ui) {
    return _ui->crash.open;
}

void fude_crashcard_update(fude_ui* _ui) {
    fude_crashcard* _c = &_ui->crash;
    if(_c->send == NULL) {
        return;
    }
    if(!_c->asked) {
        // (once a launch: the last session's report, unless a look is running — its shots are what it shows)
        _c->asked = true;
        c8 _probe[8];
        _c->open = (!fude_look_given() || fude_look_has("--crash-card")) && fude_crash_pending(_probe, sizeof(_probe));
        fude_kit_modal_show(&_c->modal, _c->open);
    }
    if(!_c->open) {
        return;
    }
    const rde_vec_2F _screen = fude_kit_screen_size(_ui->window);
    if(_screen.x == _c->_laid_out.x && _screen.y == _c->_laid_out.y) {
        return;
    }
    _c->_laid_out = _screen;
    const f32 _w = fminf(FUDE_CRASHCARD_W, _screen.x - 32.0f), _h = FUDE_CRASHCARD_H;
    fude_kit_modal_place(&_c->modal, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, (rde_vec_2F){ _w, _h });
    const f32 _p = FUDE_CRASHCARD_PAD;
    fude_kit_place(rde_ui_label_as_node(_c->title), (rde_vec_2F){ _w * 0.5f, _h - _p - 18.0f }, (rde_vec_2F){ _w - 2.0f * _p, 40.0f });
    fude_kit_place(rde_ui_label_as_node(_c->text), (rde_vec_2F){ _w * 0.5f, _h * 0.5f + 4.0f }, (rde_vec_2F){ _w - 2.0f * _p, _h - 2.0f * _p - 40.0f - 56.0f });
    const f32 _bw = fminf(200.0f, (_w - 3.0f * _p) * 0.5f);
    fude_kit_place(rde_ui_button_as_node(_c->send), (rde_vec_2F){ _w - _p - _bw * 0.5f, _p + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
    fude_kit_place(rde_ui_button_as_node(_c->later), (rde_vec_2F){ _w - 2.0f * _p - _bw * 1.5f, _p + 22.0f }, (rde_vec_2F){ _bw, 44.0f });
}
