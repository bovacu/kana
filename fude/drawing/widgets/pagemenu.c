// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/pagemenu.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See pagemenu.h.
// ===========================================================================

#define FUDE_PAGEMENU_GAP          6.0f    // between the selection's box and its menu
#define FUDE_PAGEMENU_CONTEXT_LIFT 56.0f   // the context menu sits this far above the finger
#define FUDE_PAGEMENU_COPIED_TIME  1.2     // seconds Copy reads "Copied"

// --- the selection's menu ----------------------------------------------------------

// The app's own page (extension.h: page_kind) takes the menus' commands itself.
RDE_INTERNAL b8 fude_pagemenu_kind_command(fude_app* _app, FUDE_PAGE_CMD_ _cmd, rde_vec_2F _at) {
    const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
    if(_kind == NULL || _kind->command == NULL) {
        return false;
    }
    _kind->command(_app, (u32)_cmd, _at);
    return true;
}

void fude_pagemenu_on_cut(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_CUT, (rde_vec_2F){ 0.0f, 0.0f })) {
        fude_lasso_cut(_app->lasso, _app->ink);   // copy, then one undoable delete
    }
}

void fude_pagemenu_copied(fude_pagemenu* _menu, fude_row_press _press) {
    _menu->copied       = fude_row_def_find(_menu->selection.def, _press);
    _menu->copied_until = rde_engine_get_time_now() + FUDE_PAGEMENU_COPIED_TIME;
}

void fude_pagemenu_on_copy(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_COPY, (rde_vec_2F){ 0.0f, 0.0f })) {
        fude_lasso_copy(_app->lasso, _app->ink);
    }
    fude_pagemenu_copied((fude_pagemenu*)_self, fude_pagemenu_on_copy);   // nothing on the page changes, so say it worked
}

void fude_pagemenu_on_duplicate(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_DUPLICATE, (rde_vec_2F){ 0.0f, 0.0f })) {
        fude_lasso_duplicate(_app->lasso, _app->ink, _app->canvas->view.zoom);
    }
}

void fude_pagemenu_on_delete(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_DELETE, (rde_vec_2F){ 0.0f, 0.0f })) {
        fude_lasso_delete(_app->lasso, _app->ink);   // one undoable edit
    }
}

static const fude_row_button FUDE_PAGEMENU_SELECTION[] = {
    FUDE_PAGEMENU_BUTTON_CUT, FUDE_PAGEMENU_BUTTON_COPY, FUDE_PAGEMENU_BUTTON_DUPLICATE, FUDE_PAGEMENU_BUTTON_DELETE,
};
static const fude_row_def FUDE_PAGEMENU_SELECTION_ROW = FUDE_ROW_DEF(FUDE_PAGEMENU_SELECTION);

// "Copied" a moment on the button that copied; then the app's own faces (Kana:
// "Reading…" while the selection is read for a button).
RDE_INTERNAL void fude_pagemenu_selection_faces(fude_pagemenu* _menu, fude_row_face* _faces) {
    memset(_faces, 0, sizeof(fude_row_face) * FUDE_ROW_BUTTONS);
    if(_menu->copied != FUDE_ROW_NONE && rde_engine_get_time_now() < _menu->copied_until) {
        snprintf(_faces[_menu->copied].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_SEL_COPIED));
    }
    const fude_extension* _ext = fude_app_ext(_menu->app);
    if(_ext->selection_faces != NULL) {
        _ext->selection_faces(_menu->app, _menu->selection.def, _faces);
    }
}

// Floating just above the selection's box — below it when there is no room
// above — and hidden while the selection is dragged.
RDE_INTERNAL void fude_pagemenu_place_selection(fude_pagemenu* _menu, b8 _hidden) {
    fude_app*  _app = _menu->app;
    rde_vec_2F _min, _max;
    // The app's own page: its selection's box is on the screen already.
    const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
    if(_kind != NULL && _kind->selection != NULL) {
        b8 _busy = false;
        const b8 _has = _kind->selection(_app, &_min, &_max, &_busy);
        const b8 _on  = !_hidden && _has && !_busy;
        rde_vec_2F _c = _menu->selection.center;
        if(_on) {
            const rde_vec_2F _screen = fude_kit_screen_size(_app->window);
            const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);
            const rde_vec_2F _size   = _menu->selection.size;
            const f32        _top    = _max.y + _screen.y * 0.5f + FUDE_LASSO_BOX_PAD;
            const f32        _bottom = _min.y + _screen.y * 0.5f - FUDE_LASSO_BOX_PAD;
            _c = (rde_vec_2F){ (_min.x + _max.x) * 0.5f + _screen.x * 0.5f, _top + FUDE_PAGEMENU_GAP + _size.y * 0.5f };
            if(_c.y + _size.y * 0.5f > _screen.y - (f32)_insets.y - FUDE_KIT_SCREEN_EDGE) {
                _c.y = _bottom - FUDE_PAGEMENU_GAP - _size.y * 0.5f;
            }
        }
        fude_row_show(&_menu->selection, _app->window, _on, _c);
        return;
    }
    fude_lasso_sync(_app->lasso, _app->ink);
    // Ink's row over ink; the text row over an area of a PDF's text.
    const b8   _text   = _menu->text.panel != NULL && fude_lasso_area(_app->lasso, &_min, &_max);
    fude_row*  _row    = _text ? &_menu->text : &_menu->selection;
    const b8   _show   = !_hidden && !fude_lasso_busy(_app->lasso) && fude_lasso_box(_app->lasso, _app->ink, &_min, &_max);
    if(_menu->text.panel != NULL) {
        fude_row_show(_text ? &_menu->selection : &_menu->text, _app->window, false, _menu->selection.center);
    }
    rde_vec_2F _center = _row->center;
    if(_show) {
        // Page → the app's screen (centre origin) → UI canvas (bottom-left origin).
        const fude_view* _view   = &_app->canvas->view;
        const rde_vec_2F _screen = fude_kit_screen_size(_app->window);
        const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);   // left, top, right, bottom
        const rde_vec_2F _size   = _row->size;
        const f32        _x      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x + _screen.x * 0.5f;
        const f32        _top    = _max.y * _view->zoom + _view->offset.y + _screen.y * 0.5f + FUDE_LASSO_BOX_PAD;
        const f32        _bottom = _min.y * _view->zoom + _view->offset.y + _screen.y * 0.5f - FUDE_LASSO_BOX_PAD;
        _center = (rde_vec_2F){ _x, _top + FUDE_PAGEMENU_GAP + _size.y * 0.5f };
        if(_center.y + _size.y * 0.5f > _screen.y - (f32)_insets.y - FUDE_KIT_SCREEN_EDGE) {
            _center.y = _bottom - FUDE_PAGEMENU_GAP - _size.y * 0.5f;
        }
    }
    fude_row_show(_row, _app->window, _show, _center);
}

// --- the context menu ----------------------------------------------------------------

void fude_pagemenu_on_paste(fude_app* _app, void* _self, u32 _arg) {
    fude_pagemenu* _menu = (fude_pagemenu*)_self;
    RDE_UNUSED(_arg);
    fude_pagemenu_close_context(_menu);
    // What was pasted comes in selected, ready to drag: that is the Lasso's job.
    fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
    // (The app's own page was handed the screen point where the menu opened.)
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_PASTE, _menu->context_canvas)) {
        fude_lasso_paste(_app->lasso, _app->ink, _menu->context_canvas);
    }
}

void fude_pagemenu_on_select_all(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    fude_pagemenu_close_context((fude_pagemenu*)_self);
    fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
    if(!fude_pagemenu_kind_command(_app, FUDE_PAGE_CMD_SELECT_ALL, (rde_vec_2F){ 0.0f, 0.0f })) {
        fude_lasso_select_all(_app->lasso, _app->ink);
    }
}

static const fude_row_button FUDE_PAGEMENU_CONTEXT[] = {
    FUDE_PAGEMENU_BUTTON_PASTE, FUDE_PAGEMENU_BUTTON_SELECT_ALL,
};
static const fude_row_def FUDE_PAGEMENU_CONTEXT_ROW = FUDE_ROW_DEF(FUDE_PAGEMENU_CONTEXT);

void fude_pagemenu_open_context(fude_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas) {
    fude_app* _app = _menu->app;
    if(_menu->context.panel == NULL) {
        return;
    }
    _menu->context_canvas = _canvas;
    // What can be pasted now; then the app's own buttons (Kana: whether there is
    // text to paste — iOS says only that — and whether a photo can be read).
    memset(_menu->context_faces, 0, sizeof(_menu->context_faces));
    const u32 _paste = fude_row_def_find(_menu->context.def, fude_pagemenu_on_paste);
    if(_paste != FUDE_ROW_NONE) {
        const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
        _menu->context_faces[_paste].disabled = _kind != NULL && _kind->can_paste != NULL ? !_kind->can_paste(_app) : !fude_lasso_can_paste(_app->lasso);
    }
    const fude_extension* _ext = fude_app_ext(_app);
    if(_ext->context_faces != NULL) {
        _ext->context_faces(_app, _menu->context.def, _menu->context_faces);
    }
    fude_row_apply(&_menu->context, _menu->context_faces);

    // The app's screen (centre origin) → UI canvas (bottom-left origin); above the
    // finger, or below it with no room above.
    const rde_vec_2F _ui     = fude_kit_screen_size(_app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);   // left, top, right, bottom
    const rde_vec_2F _size   = _menu->context.size;
    const rde_vec_2F _p      = { _screen.x + _ui.x * 0.5f, _screen.y + _ui.y * 0.5f };
    rde_vec_2F       _center = { _p.x, _p.y + FUDE_PAGEMENU_CONTEXT_LIFT + _size.y * 0.5f };
    if(_center.y + _size.y * 0.5f > _ui.y - (f32)_insets.y - FUDE_KIT_SCREEN_EDGE) {
        _center.y = _p.y - FUDE_PAGEMENU_CONTEXT_LIFT - _size.y * 0.5f;
    }
    fude_row_show(&_menu->context, _app->window, true, _center);
}

void fude_pagemenu_close_context(fude_pagemenu* _menu) {
    fude_row_show(&_menu->context, _menu->app->window, false, _menu->context.center);
}

// --- lifetime ------------------------------------------------------------------------------

void fude_pagemenu_init(fude_pagemenu* _menu, fude_app* _app) {
    memset(_menu, 0, sizeof(*_menu));
    _menu->app    = _app;
    _menu->copied = FUDE_ROW_NONE;
}

void fude_pagemenu_build(fude_pagemenu* _menu, rde_ui_node* _root) {
    const fude_extension* _ext = fude_app_ext(_menu->app);
    fude_row_create(&_menu->selection, _root, _ext->selection_row != NULL ? _ext->selection_row : &FUDE_PAGEMENU_SELECTION_ROW, _menu->app, _menu, fude_ui_after_press);
    fude_row_create(&_menu->context, _root, _ext->context_row != NULL ? _ext->context_row : &FUDE_PAGEMENU_CONTEXT_ROW, _menu->app, _menu, fude_ui_after_press);
    if(_ext->text_row != NULL) {
        fude_row_create(&_menu->text, _root, _ext->text_row, _menu->app, _menu, fude_ui_after_press);
    }
}

void fude_pagemenu_restyle(fude_pagemenu* _menu) {
    fude_row_restyle(&_menu->selection);
    if(_menu->text.panel != NULL) {
        fude_row_restyle(&_menu->text);
    }
    fude_row_restyle(&_menu->context);
}

void fude_pagemenu_update(fude_pagemenu* _menu, b8 _hidden) {
    const fude_extension* _ext = fude_app_ext(_menu->app);
    if(_ext->menu_update != NULL) {
        _ext->menu_update(_menu->app);
    }
    if(_hidden) {
        fude_pagemenu_close_context(_menu);
    }
    fude_pagemenu_place_selection(_menu, _hidden);
    fude_row_face _faces[FUDE_ROW_BUTTONS];
    fude_pagemenu_selection_faces(_menu, _faces);
    fude_row_apply(&_menu->selection, _faces);
    if(_menu->text.panel != NULL) {
        memset(_faces, 0, sizeof(_faces));
        const fude_extension* _ext = fude_app_ext(_menu->app);
        if(_ext->selection_faces != NULL) {
            _ext->selection_faces(_menu->app, _menu->text.def, _faces);   // the app's ("Reading…")
        }
        fude_row_apply(&_menu->text, _faces);
    }
}

b8 fude_pagemenu_hit(const fude_pagemenu* _menu, rde_vec_2F _ui) {
    return fude_row_hit(&_menu->selection, _ui) || fude_row_hit(&_menu->context, _ui) || (_menu->text.panel != NULL && fude_row_hit(&_menu->text, _ui));
}
