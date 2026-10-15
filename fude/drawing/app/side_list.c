// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/side_parts.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/ink/notes.h"
#include "drawing/base/icons.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See side.h and side_parts.h. The side panel's list of canvases and folders: its rows, built again from the notes'
// tree whenever the notes change or another canvas opens (side.c's update asks), a tap on a row and on its "…", and a
// row dragged by its handle to another place, with the line or the box that shows where it would land.
// ===========================================================================

// The list's layout, in UI units (side_parts.h has the sizes side.c shares).
#define FUDE_SIDE_DOTS_W    44.0f     // A row's "…".
#define FUDE_SIDE_INDENT    18.0f     // A level deeper in a folder.
#define FUDE_SIDE_INDENT_MAX 8u       // Levels past this are listed at this indent, so a name never leaves the panel.
#define FUDE_SIDE_HANDLE_W  28.0f     // A row's drag handle.

// Typed views of the panel's arrays (side.h): the notes list's rows...
RDE_INTERNAL fude_side_row*      fude_side_rows(const fude_side* _side)      { return (fude_side_row*)_side->_rows.memory; }
// ...and their refs.
RDE_INTERNAL fude_side_note_ref* fude_side_note_refs(const fude_side* _side) { return (fude_side_note_ref*)_side->_note_refs.memory; }

// --- a row's taps -----------------------------------------------------------------------

// A row of the notes list was tapped: a folder opens or closes; a canvas opens on the page and the panel closes.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_note(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_note_ref* _ref     = (const fude_side_note_ref*)_user_data;
    fude_ui*             _ui = _ref->ui;
    const fude_note*          _note    = fude_notes_find(_ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_note->kind == FUDE_NOTE_FOLDER) {
        fude_notes_set_expanded(_ui->app->notes, _note->id, !_note->expanded);
    } else {
        fude_notes_open(_ui->app->notes, _note->id);   // The session sees it and switches the page.
        _ui->side.open = false;
        fude_ui_update(_ui);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A row's "…": opens the note card with its actions.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_note_dots(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_side_note_ref* _ref = (const fude_side_note_ref*)_user_data;
    fude_side_card(_ref->ui, FUDE_SIDE_CARD_ACTIONS, _ref->id);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- dragging a row ------------------------------------------------------------------

// Hides the drop line and the drop box.
void fude_side_hide_drop(fude_side* _side) {
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), false);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), false);
}

// How far in a row at folder depth _depth is drawn: a level's indent each, up to FUDE_SIDE_INDENT_MAX levels.
RDE_INTERNAL f32 fude_side_indent(u32 _depth) {
    return (f32)(_depth < FUDE_SIDE_INDENT_MAX ? _depth : FUDE_SIDE_INDENT_MAX) * FUDE_SIDE_INDENT;
}

// Works out where the dragged row would land with the pointer at _side->drag_at (drop_parent, drop_before and
// drop_valid), and shows it: by the line between rows, indented to the level it lands at, or by the box round a folder
// when the pointer is over the middle of the folder's row (into it). A folder never lands inside itself. A pointer
// well to the side of the list drops nowhere.
void fude_side_drop_target(fude_ui* _ui) {
    fude_side*       _side  = &_ui->side;
    const fude_note* _moving = fude_notes_find(_ui->app->notes, _side->drag_id);
    const f32        _pitch = FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP;
    const f32        _top   = _side->_list_bl.y + _side->_list_size.y;
    const f32        _scroll = rde_ui_scroll_area_get_scroll(_side->notes_list).y;

    _side->drop_valid = false;
    fude_side_hide_drop(_side);
    if(_moving == NULL || _side->_row_count == 0 ||
       _side->drag_at.x < _side->_list_bl.x - 20.0f || _side->drag_at.x > _side->_list_bl.x + _side->_list_size.x + 60.0f) {
        return;
    }

    const f32 _cy = rde_math_clamp_f32(_top - _side->drag_at.y, 0.0f, _side->_list_size.y) + _scroll;   // From its top.
    i32       _r  = (i32)rde_math_floor_f32(_cy / _pitch);
    f32       _t  = (_cy - (f32)_r * _pitch) / FUDE_SIDE_NOTE_H;   // Down the row: 0 at its top, 1 at its bottom.
    b8        _after_all = false;
    if(_r >= (i32)_side->_row_count) {
        _r         = (i32)_side->_row_count - 1;
        _after_all = true;
    }
    const fude_side_row* _rows = fude_side_rows(_side);
    const fude_side_row* _row  = &_rows[_r];

    b8  _into  = false;
    u32 _depth = 0;
    u32 _line  = (u32)_r;   // The line sits at the top of this row (or past the last).
    // The middle of a folder's row (from 28% to 72% down it) drops into the folder; the upper half of a row drops
    // before it; the lower half, after it.
    if(!_after_all && _row->kind == FUDE_NOTE_FOLDER && _t > 0.28f && _t < 0.72f) {
        _into               = true;
        _side->drop_parent  = _row->id;
        _side->drop_before  = 0;
    } else if(!_after_all && _t <= 0.5f) {
        _side->drop_parent  = _row->parent;
        _side->drop_before  = _row->id;
        _depth              = _row->depth;
    } else {
        _line = (u32)_r + 1u;
        if(_line < _side->_row_count && _rows[_line].depth > _row->depth) {
            // After an open folder, it goes first inside it.
            _side->drop_parent = _row->id;
            _side->drop_before = _rows[_line].id;
            _depth             = _rows[_line].depth;
        } else {
            // After the row, in the same folder: before its next sibling, or at the end.
            _side->drop_parent = _row->parent;
            _side->drop_before = 0;
            for(u32 _k = _line; _k < _side->_row_count; _k++) {
                if(_rows[_k].depth < _row->depth) {
                    break;
                }
                if(_rows[_k].parent == _row->parent) {
                    _side->drop_before = _rows[_k].id;
                    break;
                }
            }
            _depth = _row->depth;
        }
    }

    // A folder cannot go into itself or anything inside it, and a note dropped on its own place goes nowhere.
    if(_moving->kind == FUDE_NOTE_FOLDER && _side->drop_parent != 0 && fude_notes_is_within(_ui->app->notes, _side->drop_parent, _moving->id)) {
        return;
    }
    if(_side->drop_before == _moving->id) {
        return;
    }
    _side->drop_valid = true;

    if(_into) {
        const f32 _indent = fude_side_indent(_row->depth);
        const f32 _y      = _top - ((f32)_r * _pitch - _scroll) - FUDE_SIDE_NOTE_H * 0.5f;
        fude_kit_place(rde_ui_image_as_node(_side->drop_box), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent + 4.0f, FUDE_SIDE_NOTE_H + 4.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_box), true);
    } else {
        const f32 _indent = fude_side_indent(_depth);
        const f32 _y      = _top - ((f32)_line * _pitch - _scroll) + FUDE_SIDE_GAP * 0.5f;
        if(_y > _top + FUDE_SIDE_GAP || _y < _side->_list_bl.y - FUDE_SIDE_GAP) {
            return;   // Valid, but scrolled out of sight.
        }
        fude_kit_place(rde_ui_image_as_node(_side->drop_line), (rde_vec_2F){ _side->_list_bl.x + (_indent + _side->_list_size.x) * 0.5f, _y },
                           (rde_vec_2F){ _side->_list_size.x - _indent, 3.0f });
        rde_ui_node_set_active(rde_ui_image_as_node(_side->drop_line), true);
    }
}

// A row's handle starts a drag: the note's name follows the pointer, and where it would land is shown.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_begin(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref  = (const fude_side_note_ref*)_user_data;
    fude_side*                _side = &_ref->ui->side;
    const fude_note*          _note = fude_notes_find(_ref->ui->app->notes, _ref->id);
    if(_note == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_id = _ref->id;
    _side->drag_at = _info->position;
    rde_ui_label_set_text(_side->drag_ghost_label, _note->name);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), true);
    fude_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    fude_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;   // The handle takes it, so the list does not scroll with it.
}

// The drag moves: the name follows, and where it would land is shown again.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_move(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref  = (const fude_side_note_ref*)_user_data;
    fude_side*                _side = &_ref->ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    fude_kit_place(rde_ui_image_as_node(_side->drag_ghost), (rde_vec_2F){ _info->position.x + 90.0f, _info->position.y }, (rde_vec_2F){ 170.0f, 36.0f });
    fude_side_drop_target(_ref->ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The drag ends: the note moves where it was shown to land, if that is allowed, and the line, box and name go.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_side_on_drag_end(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node);
    const fude_side_note_ref* _ref     = (const fude_side_note_ref*)_user_data;
    fude_ui*             _ui = _ref->ui;
    fude_side*                _side    = &_ui->side;
    if(_side->drag_id == 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _side->drag_at = _info->position;
    fude_side_drop_target(_ui);
    if(_side->drop_valid) {
        fude_notes_move(_ui->app->notes, _side->drag_id, _side->drop_parent, _side->drop_before);   // Rebuilt later.
    }
    _side->drag_id = 0;
    fude_side_hide_drop(_side);
    rde_ui_node_set_active(rde_ui_image_as_node(_side->drag_ghost), false);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the list ---------------------------------------------------------------------------

// Appends to _out the rows in note _folder (an index into the notes; the top level is their count) in list order, depth
// first: a folder, then (when it is open) what is in it. _count is how many there are so far. It goes through the
// tree's groups (fude_notes_tree), so each note is looked at once, however many folders there are.
RDE_INTERNAL void fude_side_collect_rows(const fude_notes* _notes, const fude_notes_tree* _tree, u32 _folder, u8 _depth, fude_side_row* _out, u32* _count) {
    const fude_note* _all   = (const fude_note*)_notes->notes.memory;
    const u32*       _child = (const u32*)_tree->child.memory;
    for(u32 _c = ((const u32*)_tree->first.memory)[_folder]; _c < ((const u32*)_tree->end.memory)[_folder]; _c++) {
        const u32 _i = _child[_c];
        _out[(*_count)++] = (fude_side_row){ _all[_i].id, _all[_i].parent, _depth, _all[_i].kind, _all[_i].expanded, _i };
        // Every level is listed, however deep (a tree has no loops, so this ends); the indent stops growing at the
        // deepest u8 can say.
        if(_all[_i].kind == FUDE_NOTE_FOLDER && _all[_i].expanded) {
            fude_side_collect_rows(_notes, _tree, _i, (u8)(_depth < 255u ? _depth + 1u : 255u), _out, _count);
        }
    }
}

// Builds the notes list again: each row has a drag handle, the name (a folder's with +/– and how many canvases are in
// it) and "…". The open canvas is marked.
void fude_side_build_notes(fude_ui* _ui) {
    fude_side*        _side  = &_ui->side;
    const fude_notes* _notes = _ui->app->notes;
    const u32         _count = (u32)rde_arr_length(&_notes->notes);

    rde_ui_scroll_area_clear_contents(_side->notes_list);
    if(!rde_arr_is_inited(&_side->_rows)) {   // Both are made together, the first time.
        _side->_rows      = rde_arr_new(sizeof(fude_side_row), rde_memory_allocator_get_default_std());
        _side->_note_refs = rde_arr_new(sizeof(fude_side_note_ref), rde_memory_allocator_get_default_std());
    }
    // Room for a row for every note, all zero. The arrays are not grown again until the list is built again, because
    // the refs are its nodes' user data and must not move.
    rde_arr_clear(&_side->_rows);
    rde_arr_clear(&_side->_note_refs);
    rde_arr_resize(&_side->_rows, _count);
    rde_arr_resize(&_side->_note_refs, _count);
    _side->_row_count      = 0;
    _side->_notes_revision = _notes->revision;
    _side->_notes_open     = _notes->open;
    _side->_notes_built    = true;
    // The notes as a tree, for the rows and each folder's count: made once here, not searched for every row.
    fude_notes_tree _tree;
    fude_notes_tree_make(_notes, &_tree);
    fude_side_collect_rows(_notes, &_tree, _count, 0, fude_side_rows(_side), &_side->_row_count);
    const fude_note* _all      = (const fude_note*)_notes->notes.memory;
    const u32*       _canvases = (const u32*)_tree.canvases.memory;

    const f32 _width   = _side->_list_width;
    const f32 _content = rde_math_max_f32(1.0f, (f32)_side->_row_count * (FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP));
    rde_ui_scroll_area_set_content_size(_side->notes_list, (rde_vec_2F){ _width, _content });
    rde_ui_node* _list = rde_ui_scroll_area_as_node(_side->notes_list);
    const fude_theme* _t = fude_theme_active();

    for(u32 _r = 0; _r < _side->_row_count; _r++) {
        const fude_side_row* _row = &fude_side_rows(_side)[_r];
        const fude_note*     _n   = &_all[_row->index];
        const b8             _folder = _row->kind == FUDE_NOTE_FOLDER;
        const f32            _indent = fude_side_indent(_row->depth);
        const f32            _y      = _content - (f32)_r * (FUDE_SIDE_NOTE_H + FUDE_SIDE_GAP) - FUDE_SIDE_NOTE_H * 0.5f;

        fude_side_note_ref* _ref = &fude_side_note_refs(_side)[_r];
        *_ref                    = (fude_side_note_ref){ _ui, _row->id };

        // The handle: a grip of dots; a drag from it moves the row.
        rde_ui_image* _handle = rde_ui_image_create(NULL);
        rde_ui_node*  _h      = rde_ui_image_as_node(_handle);
        rde_ui_image_set_style(_handle, RDE_UI_STATE_NORMAL, fude_kit_style((rde_color){ 0, 0, 0, 0 }, 8.0f));
        rde_ui_node_set_blocks_input(_h, true);
        rde_ui_node_set_user_data(_h, _ref);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_BEGIN, fude_side_on_drag_begin);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_MOVE,  fude_side_on_drag_move);
        rde_ui_node_set_callback(_h, RDE_UI_EVENT_MOUSE_DRAG_END,   fude_side_on_drag_end);
        rde_ui_node_add_child(_list, _h);
        fude_kit_place(_h, (rde_vec_2F){ _indent + FUDE_SIDE_HANDLE_W * 0.5f, _y }, (rde_vec_2F){ FUDE_SIDE_HANDLE_W, FUDE_SIDE_NOTE_H });
        rde_ui_label* _dots6 = fude_side_label(_ui, _h, FUDE_ICON_GRIP_V, 14.0f);
        rde_ui_label_set_font(_dots6, _ui->font_icons != NULL ? _ui->font_icons : _ui->font);
        rde_ui_label_set_alignment(_dots6, RDE_UI_LABEL_H_ALIGN_CENTER, RDE_UI_LABEL_V_ALIGN_MIDDLE);
        rde_ui_label_set_color(_dots6, _t->grip);
        const f32 _back = fude_kit_icon_back(FUDE_ICON_GRIP_V, 14.0f);   // Centred.
        fude_kit_place(rde_ui_label_as_node(_dots6), (rde_vec_2F){ FUDE_SIDE_HANDLE_W * 0.5f - _back, FUDE_SIDE_NOTE_H * 0.5f }, (rde_vec_2F){ FUDE_SIDE_HANDLE_W, FUDE_SIDE_NOTE_H });

        c8 _label[FUDE_NOTE_NAME + 32];
        if(_folder) {
            c8 _count[32];
            rde_snprintf(_count, sizeof(_count), "%u", _canvases[_row->index]);
            rde_localization_localize_digits(_count, sizeof(_count));   // The count only; the name stays as typed.
            rde_snprintf(_label, sizeof(_label), "%s  (%s)", _n->name, _count);
        } else {
            rde_snprintf(_label, sizeof(_label), "%s", _n->name);
        }

        const f32        _x0       = _indent + FUDE_SIDE_HANDLE_W + FUDE_SIDE_GAP;
        const rde_vec_2F _row_size = { _width - _x0 - FUDE_SIDE_DOTS_W - FUDE_SIDE_GAP, FUDE_SIDE_NOTE_H };
        rde_ui_button*   _button   = fude_kit_button(_list, _label, fude_side_on_note, _ui);
        rde_ui_button_set_on_click(_button, fude_side_on_note, _ref);
        fude_kit_place(rde_ui_button_as_node(_button), (rde_vec_2F){ _x0 + _row_size.x * 0.5f, _y }, _row_size);
        fude_kit_icon(_button, _folder ? (_row->expanded ? FUDE_ICON_FOLDER_OPEN : FUDE_ICON_FOLDER) : _n->document != 0u ? FUDE_ICON_BOOK : FUDE_ICON_NOTE, FUDE_KIT_ICON_LEFT, 15.0f);   // A canvas over a document: the book.
        if(!_folder && _row->id == _notes->open) {
            fude_kit_button_selected(_button);   // The canvas on the page.
        } else {
            fude_kit_button_quiet(_button);
            fude_kit_icon_color(_button, _t->text_soft);
        }

        rde_ui_button* _dots = fude_kit_button(_list, "\xE2\x80\xA6", fude_side_on_note_dots, _ui);   // "…" in UTF-8.
        rde_ui_button_set_on_click(_dots, fude_side_on_note_dots, _ref);
        fude_kit_icon(_dots, FUDE_ICON_MORE, FUDE_KIT_ICON_ONLY, 17.0f);
        fude_kit_button_quiet(_dots);
        fude_kit_place(rde_ui_button_as_node(_dots), (rde_vec_2F){ _width - FUDE_SIDE_DOTS_W * 0.5f, _y }, (rde_vec_2F){ FUDE_SIDE_DOTS_W, FUDE_SIDE_NOTE_H });
    }
    fude_notes_tree_free(&_tree);
}
