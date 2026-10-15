// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, Find: Find in a canvas and through every canvas, and handwriting read as text (drawing/ink/mlkit.h,
// handfind.h) for Find and for To text.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
//
// Its sections: Find in the open canvas; Find through every canvas, a few canvases a frame; handwriting read as text
// by ML Kit, for To text and for Find. The reader takes one piece of writing at a time: a line for Find is not handed
// to it while To text reads, and To text asked for while a line is being read waits for that answer (read_wanted).
// ===========================================================================

// --- Find --------------------------------------------------------------------------------------

// Returns whether _hay's _len bytes hold _needle, the letters A to Z in either case alike; false for an empty _needle.
RDE_INTERNAL b8 fude_zoom_find_in(const c8* _hay, u32 _len, const c8* _needle) {
    const usize _n = rde_strlen(_needle);
    if(_n == 0 || _n > _len) {
        return false;
    }
    for(u32 _i = 0; _i + _n <= _len; _i++) {
        usize _k = 0;
        while(_k < _n) {
            c8 _a = _hay[_i + _k], _b = _needle[_k];
            _a = (_a >= 'A' && _a <= 'Z') ? (c8)(_a + 32) : _a;
            _b = (_b >= 'A' && _b <= 'Z') ? (c8)(_b + 32) : _b;
            if(_a != _b) {
                break;
            }
            _k++;
        }
        if(_k == _n) {
            return true;
        }
    }
    return false;
}

// The handwriting for Find, defined below.
RDE_INTERNAL void fude_zoom_page_hand_begin(fude_zoom_page* _page);
RDE_INTERNAL const c8* fude_zoom_page_hand_text(const fude_zoom_page* _page, u64 _key);
RDE_INTERNAL u32 fude_zoom_page_hand_unread(const fude_zoom_page* _page);

// The words of object _i Find looks through (a text's, a diagram symbol's, a board's name and what it is made of: "oak"
// finds the oak ones): into *_words, *_len (_name: room for those made up). False: none (not alive, not such a thing).
RDE_INTERNAL b8 fude_zoom_page_object_words(const fude_zoom_page* _page, const fude_zoom_scene* _s, u32 _i, c8* _name, usize _size, const c8** _words, u32* _len) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
    *_words = NULL;
    *_len   = 0u;
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return false;
    }
    if(_o->kind == FUDE_ZOOM_KIND_TEXT) {
        return fude_zoom_scene_text(_s, _i, NULL, NULL, NULL, NULL, _words, _len);
    }
    if(_o->kind != FUDE_ZOOM_KIND_SHAPE || (_o->channels != FUDE_ZOOM_SHAPE_SYMBOL && _o->channels != FUDE_ZOOM_SHAPE_BOARD)) {
        return false;
    }
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _nc = fude_zoom_scene_shape_numbers(_s, _i, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
        fude_zoom_symbol_text(_n, _nc, _name, _size);
    } else {
        fude_zoom_board_name(_n, _nc, _name, _size < FUDE_ZOOM_BOARD_NAME ? _size : FUDE_ZOOM_BOARD_NAME);
        const u8 _material = fude_zoom_board_material(_n, _nc);
        if(_material != FUDE_ZOOM_MATERIAL_NONE) {
            const usize _at = rde_strlen(_name);
            rde_snprintf(_name + _at, _size - _at, "\n%s", _page->material_words[_material]);
        }
    }
    *_words = _name;
    *_len   = (u32)rde_strlen(_name);
    return true;
}

// Find through every canvas, defined below.
RDE_INTERNAL void fude_zoom_page_everywhere_start(fude_zoom_page* _page, const c8* _query);

// See page_parts.h. The things shown that hold _query, at any depth (texts, diagram symbols, boards), then the lines of
// handwriting read that say it: the next of them flown to, and asked again with the same words, the one after, round
// to the first past the last. Over a PDF, its matches come after those: searched anew for new words, its first flown
// to as it comes when nothing else holds them. New words also start Find through every canvas. Each answer is said in
// a notice ("3 of 7", nothing found, or the handwriting still being read).
void fude_zoom_page_find(fude_zoom_page* _page, const c8* _query) {
    fude_zoom_scene* _s = &_page->scene;
    const b8 _new = rde_strcmp(_query, _page->find_query) != 0 || (fude_zoom_pdfview_is_open(&_page->pdf) && fude_pdf_text_available() && !_page->pdf.searching);
    if(_new) {
        rde_snprintf(_page->find_query, sizeof(_page->find_query), "%s", _query);
        _page->find_next = 0;
    }
    if(rde_strcmp(_query, _page->everywhere_query) != 0) {
        fude_zoom_page_everywhere_start(_page, _query);   // (the other canvases looked through too, a few a frame)
    }
    u32 _count = 0, _hit = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || (_o->kind != FUDE_ZOOM_KIND_TEXT && _o->kind != FUDE_ZOOM_KIND_SHAPE) || fude_zoom_scene_hides(_s, _o) ||
           !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const c8* _words = NULL;
        u32 _len = 0;
        c8 _name[FUDE_ZOOM_SYMBOL_TEXT];
        if(!fude_zoom_page_object_words(_page, _s, _i, _name, sizeof(_name), &_words, &_len)) {
            continue;
        }
        if(!fude_zoom_find_in(_words, _len, _query)) {
            continue;
        }
        if(_count == _page->find_next) {
            _hit = _i;
        }
        _count++;
    }
    // Handwriting read (handfind.h): its lines that say it, after the texts. Asked for the first time: read from now
    // on.
    fude_zoom_page_hand_begin(_page);
    u32 _hand_hit = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _page->hand_on && _i < (u32)rde_arr_length(&_page->hand_lines); _i++) {
        const fude_zoom_hand_line* _line = &((const fude_zoom_hand_line*)_page->hand_lines.memory)[_i];
        const c8* _said = fude_zoom_page_hand_text(_page, _line->key);
        if(_said == NULL || !fude_zoom_scene_frame_shown(_s, _line->frame) || !fude_zoom_find_in(_said, (u32)rde_strlen(_said), _query)) {
            continue;
        }
        if(_count == _page->find_next) {
            _hand_hit = _i;
        }
        _count++;
    }
    const u32 _unread = _page->hand_on ? fude_zoom_page_hand_unread(_page) : 0u;
    // The PDF's: searched anew for new words (its first gone to as it comes, when no text holds them).
    fude_zoom_pdfview* _pdf = &_page->pdf;
    if(fude_zoom_pdfview_is_open(_pdf)) {
        if(_new) {
            fude_zoom_pdfview_search(_pdf, _query);
            _pdf->search_go       = _count == 0u;
            _page->find_pdf_base  = _count;
        }
        const u32 _all = _count + _pdf->match_count;
        if(_all == 0u) {
            fude_notice_show(fude_text(_pdf->searching && !_pdf->search_done ? FUDE_TEXT_DOC_SEARCHING : FUDE_TEXT_DOC_NO_MATCHES));
            return;
        }
        if(_page->find_next >= _all) {
            _page->find_next = 0;   // past the last: round to the first
        }
        if(_page->find_next >= _count) {
            fude_zoom_box _box;
            if(fude_zoom_pdfview_match_box(_pdf, _page->find_next - _count, &_box)) {
                _pdf->search_go = false;
                fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _s->home, _box, 0.15), true);
            }
            c8 _say[96];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_FIND_OF, FUDE_TN(_page->find_next + 1u), FUDE_TN(_all));
            fude_notice_show(_say);
            _page->find_next++;
            return;
        }
        if(_hit == FUDE_ZOOM_NONE && _hand_hit == FUDE_ZOOM_NONE) {
            _page->find_next = 0;
            fude_zoom_page_find(_page, _query);
            return;
        }
    }
    if(_count == 0) {
        if(_unread > 0u) {
            c8 _say[200];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_FIND_READING, FUDE_TN(_unread));   // (the handwriting still being read)
            fude_notice_show(_say);
        } else {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_FIND_NONE));
        }
        return;
    }
    if(_hit == FUDE_ZOOM_NONE && _hand_hit == FUDE_ZOOM_NONE) {   // past the last: round to the first
        _page->find_next = 0;
        fude_zoom_page_find(_page, _query);
        return;
    }
    if(_hit != FUDE_ZOOM_NONE) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _hit);
        fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _o->frame, _o->box, 0.35), true);
    } else {
        const fude_zoom_hand_line* _line = &((const fude_zoom_hand_line*)_page->hand_lines.memory)[_hand_hit];
        fude_zoom_page_fly(_page, fude_zoom_nav_framing(fude_zoom_page_half(_page), _line->frame, _line->box, 0.35), true);
    }
    c8 _say[96];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_FIND_OF, FUDE_TN(_page->find_next + 1u), FUDE_TN(_count));
    fude_notice_show(_say);
    _page->find_next++;
}

// --- Find through every canvas (0.1.92) ---------------------------------------------------------------
//
// Each canvas's words kept beside it (words.h: <id>.words), written as it is left; a canvas never left since, or
// whose file grew after (the app stopped while it was open), looked through without being opened (zfile.h' peek)
// and its words kept then. New words to find: the others looked through a few a frame, those holding them listed in
// a panel where the strip goes, most matches first; a tap opens one and finds them in it.

#define FUDE_ZOOM_PAGE_EVERYWHERE_BUDGET 0.006   // seconds a frame spent looking through other canvases

// See page_parts.h: the canvas's own path (notes.h) with its extension changed.
void fude_zoom_page_side_path(u32 _canvas, const c8* _ext, c8* _out, usize _size) {
    c8 _kana[RDE_MAX_PATH];
    fude_notes_canvas_path(_canvas, _kana, sizeof(_kana));
    c8* _dot = rde_strrchr(_kana, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.%s", _kana, _ext);
}

// Every word Find would look through in _s, a line each, into _out (c8; cleared first) — the handwriting read on the
// open canvas too (_hand).
RDE_INTERNAL void fude_zoom_page_words_gather(const fude_zoom_page* _page, const fude_zoom_scene* _s, b8 _hand, rde_arr* _out) {
    rde_arr_clear(_out);
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const c8* _words = NULL;
        u32 _len = 0;
        c8 _name[FUDE_ZOOM_SYMBOL_TEXT];
        if(!fude_zoom_page_object_words(_page, _s, _i, _name, sizeof(_name), &_words, &_len) || _len == 0u) {
            continue;
        }
        rde_memcpy(rde_arr_add_n(_out, _len), _words, _len);
        *(c8*)rde_arr_add_n(_out, 1u) = '\n';
    }
    for(u32 _i = 0; _hand && _page->hand_on && _i < (u32)rde_arr_length(&_page->hand_lines); _i++) {
        const c8* _said = fude_zoom_page_hand_text(_page, ((const fude_zoom_hand_line*)_page->hand_lines.memory)[_i].key);
        const u32 _len  = _said != NULL ? (u32)rde_strlen(_said) : 0u;
        if(_len > 0u) {
            rde_memcpy(rde_arr_add_n(_out, _len), _said, _len);
            *(c8*)rde_arr_add_n(_out, 1u) = '\n';
        }
    }
}

// See page_parts.h. The handwriting read is kept too; the file's size is what fude_zoom_page_words_of compares later.
void fude_zoom_page_words_keep(fude_zoom_page* _page) {
    if(_page->canvas == 0u) {
        return;
    }
    rde_arr _words = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    fude_zoom_page_words_gather(_page, &_page->scene, true, &_words);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_side_path(_page->canvas, "words", _path, sizeof(_path));
    fude_zoom_words_save(_path, _page->file.size, (u64)rde_date_now_unix_seconds(), (const c8*)_words.memory, (u32)rde_arr_length(&_words));
    rde_arr_free(&_words);
}

// Canvas _canvas's words into _out (c8, NUL after): those kept, or — none kept, or its file grown since — read from its
// file (never written to) and kept now.
RDE_INTERNAL void fude_zoom_page_words_of(fude_zoom_page* _page, u32 _canvas, rde_arr* _out) {
    c8 _zoom[RDE_MAX_PATH], _kept[RDE_MAX_PATH];
    fude_zoom_page_path(_canvas, _zoom, sizeof(_zoom));
    fude_zoom_page_side_path(_canvas, "words", _kept, sizeof(_kept));
    const u64 _size = fude_file_size(_zoom);
    u64 _was = 0u, _left = 0u;
    if(fude_zoom_words_load(_kept, &_was, &_left, _out) && _was == _size) {
        return;
    }
    rde_arr_clear(_out);
    if(_size > 0u) {
        fude_zoom_scene _s;
        fude_zoom_scene_init(&_s, _page->device);
        const FUDE_LOAD_ _r = fude_zoom_file_peek(_zoom, &_s);
        if(_r == FUDE_LOAD_OK || _r == FUDE_LOAD_RECOVERED) {
            fude_zoom_page_words_gather(_page, &_s, false, _out);
        }
        fude_zoom_scene_destroy(&_s);
    }
    fude_zoom_words_save(_kept, _size, _left, (const c8*)_out->memory, (u32)rde_arr_length(_out));
    *(c8*)rde_arr_add_n(_out, 1u) = 0;
}

// Starts Find through every canvas for _query: the list emptied, the other canvases counted, and the panel shown in
// the strip's place while there are any to look through (nothing for empty words).
RDE_INTERNAL void fude_zoom_page_everywhere_start(fude_zoom_page* _page, const c8* _query) {
    rde_snprintf(_page->everywhere_query, sizeof(_page->everywhere_query), "%s", _query);
    rde_arr_clear(&_page->everywhere_found);
    _page->everywhere_next   = 0u;
    _page->everywhere_done   = 0u;
    _page->everywhere_scroll = 0.0;
    _page->everywhere_total  = 0u;
    const fude_notes* _notes = _page->app->notes;
    for(u32 _i = 0; _notes != NULL && _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_notes->notes.memory)[_i];
        _page->everywhere_total += _n->kind == FUDE_NOTE_CANVAS && _n->id != _page->canvas ? 1u : 0u;
    }
    _page->everywhere_on    = _query[0] != 0 && _page->everywhere_total > 0u;
    _page->everywhere_shown = _page->everywhere_on;
    if(_page->everywhere_shown) {
        _page->strip_open = false;
    }
}

// See page_parts.h. Canvases are looked through for FUDE_ZOOM_PAGE_EVERYWHERE_BUDGET of the frame, in the notes'
// order; each one found goes into the list in its place, after those with as many matches. When the last is done the
// search stops, and the panel goes if nothing was found.
void fude_zoom_page_everywhere_step(fude_zoom_page* _page) {
    if(!_page->everywhere_on) {
        return;
    }
    const fude_notes* _notes = _page->app->notes;
    const f64 _from = rde_engine_get_time_now();
    rde_arr _words = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    while(_notes != NULL && _page->everywhere_next < (u32)rde_arr_length(&_notes->notes) &&
          rde_engine_get_time_now() - _from < FUDE_ZOOM_PAGE_EVERYWHERE_BUDGET) {
        const fude_note _n = ((const fude_note*)_notes->notes.memory)[_page->everywhere_next++];
        if(_n.kind != FUDE_NOTE_CANVAS || _n.id == _page->canvas) {
            continue;
        }
        _page->everywhere_done++;
        fude_zoom_page_words_of(_page, _n.id, &_words);
        const u32 _len   = rde_arr_length(&_words) > 0u ? (u32)rde_arr_length(&_words) - 1u : 0u;
        const u32 _count = fude_zoom_words_count((const c8*)_words.memory, _len, _page->everywhere_query);
        if(_count == 0u) {
            continue;
        }
        fude_zoom_page_found _f;
        rde_memset(&_f, 0, sizeof(_f));
        _f.note  = _n.id;
        _f.count = _count;
        rde_snprintf(_f.name, sizeof(_f.name), "%s", _n.name);
        fude_zoom_words_excerpt((const c8*)_words.memory, _len, _page->everywhere_query, 60u, _f.excerpt, sizeof(_f.excerpt));
        // (in its place: most matches first)
        u32 _at = (u32)rde_arr_length(&_page->everywhere_found);
        const fude_zoom_page_found* _all = (const fude_zoom_page_found*)_page->everywhere_found.memory;
        while(_at > 0u && _all[_at - 1u].count < _count) {
            _at--;
        }
        rde_arr_add(&_page->everywhere_found, (any)&_f);
        fude_zoom_page_found* _list = (fude_zoom_page_found*)_page->everywhere_found.memory;
        rde_memmove(&_list[_at + 1u], &_list[_at], (rde_arr_length(&_page->everywhere_found) - 1u - _at) * sizeof(fude_zoom_page_found));
        _list[_at] = _f;
    }
    rde_arr_free(&_words);
    if(_notes == NULL || _page->everywhere_next >= (u32)rde_arr_length(&_notes->notes)) {
        _page->everywhere_on = false;
        if(rde_arr_length(&_page->everywhere_found) == 0u) {
            _page->everywhere_shown = false;   // (in no other: nothing to show)
        }
    }
}

// Defined below (declared at the top of the file as well).
RDE_INTERNAL void fude_zoom_page_hand_begin(fude_zoom_page* _page);

// The bar's Find: opens the Find card with the last words, selected, so typing replaces them; the handwriting starts
// being read meanwhile.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_find(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->text_field == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    fude_zoom_page_hand_begin(_page);   // (the handwriting read while the words are typed)
    const usize _bytes = rde_ui_text_editor_get_byte_count(_page->text_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_page->text_field, 0, _bytes);
    }
    rde_ui_text_editor_insert_at(_page->text_field, 0, _page->find_query, rde_strlen(_page->find_query));
    rde_ui_text_editor_select_all(_page->text_field);
    rde_ui_text_editor_set_placeholder(_page->text_field, fude_text(FUDE_TEXT_ZOOM_FIND_HINT));
    rde_ui_button_set_text(_page->text_save, fude_text(FUDE_TEXT_ZOOM_FIND));
    _page->text_object = FUDE_ZOOM_NONE;
    _page->text_style  = FUDE_ZOOM_PAGE_TEXT_FIND;
    _page->places_open = false;
    _page->layers_open = false;
    fude_zoom_page_text_card(_page, true);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- handwriting read as text (drawing/ink/mlkit.h) ------------------------------------------

// See page.h: the language the app is shown in, as ML Kit's tag.
const c8* fude_zoom_ink_model(void) {
    switch(fude_text_language()) {
        case RDE_LANGUAGE_ES_ES: return "es-ES";
        case RDE_LANGUAGE_PT_BR: return "pt-BR";
        case RDE_LANGUAGE_FR_FR: return "fr-FR";
        case RDE_LANGUAGE_JA_JP: return "ja";
        case RDE_LANGUAGE_IT_IT: return "it-IT";
        case RDE_LANGUAGE_DE_DE: return "de-DE";
        default:                 return "en-US";
    }
}

// See page_parts.h. To text shows while ML Kit is in the build and switched on (Settings' Handwriting): off, it is
// never called.
b8 fude_zoom_read_present(void) {
    return fude_mlkit_state() != FUDE_MLKIT_UNAVAILABLE && fude_mlkit_enabled();
}

// A stroke for To text: its z (the order it was drawn in) and its object.
typedef struct { u64 z; u32 object; } fude_zoom_read_item;

// rde_sort's order for To text's strokes: as they were drawn.
RDE_INTERNAL int fude_zoom_read_by_z(const any _a, const any _b) {
    const u64 _x = ((const fude_zoom_read_item*)_a)->z, _y = ((const fude_zoom_read_item*)_b)->z;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// --- handwriting for Find (handfind.h) ----------------------------------------------------------

#define FUDE_ZOOM_PAGE_HAND_SAID 200u   // what a line says, bytes kept at most

// What a line of handwriting says, once read: the value the page's hand_said map keeps under its line's key
// (handfind.h: a hash of its strokes' ids, so a line whose strokes have not changed keeps its reading). A line read
// again keeps its newest reading.
typedef struct {
    c8 text[FUDE_ZOOM_PAGE_HAND_SAID];
} fude_zoom_page_hand_said;

// What the line keyed _key says, once read (NULL: not yet). The map is made with the lines (hand_begin).
RDE_INTERNAL const c8* fude_zoom_page_hand_text(const fude_zoom_page* _page, u64 _key) {
    if(!rde_arr_is_inited(&_page->hand_lines)) {
        return NULL;
    }
    const fude_zoom_page_hand_said* _s = (const fude_zoom_page_hand_said*)rde_hash_map_find(&_page->hand_said, (const any)&_key);
    return _s != NULL ? _s->text : NULL;
}

// Returns how many of the canvas's lines of writing are still to read.
RDE_INTERNAL u32 fude_zoom_page_hand_unread(const fude_zoom_page* _page) {
    u32 _n = 0;
    for(u32 _i = 0; rde_arr_is_inited(&_page->hand_lines) && _i < (u32)rde_arr_length(&_page->hand_lines); _i++) {
        _n += fude_zoom_page_hand_text(_page, ((const fude_zoom_hand_line*)_page->hand_lines.memory)[_i].key) == NULL ? 1u : 0u;
    }
    return _n;
}

// Find asked for: the canvas's handwriting read from now on, its lines found at once and the model got ready the
// first time. Does nothing while it is on already, or when ML Kit is not in the build or is switched off.
RDE_INTERNAL void fude_zoom_page_hand_begin(fude_zoom_page* _page) {
    if(_page->hand_on || fude_mlkit_state() == FUDE_MLKIT_UNAVAILABLE || !fude_mlkit_enabled()) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(!rde_arr_is_inited(&_page->hand_lines)) {
        _page->hand_lines   = rde_arr_new(sizeof(fude_zoom_hand_line), _heap);
        _page->hand_strokes = rde_arr_new(sizeof(u32), _heap);
        _page->hand_said    = rde_hash_map_new(sizeof(u64), sizeof(fude_zoom_page_hand_said), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, _heap);
    }
    _page->hand_on       = true;
    _page->hand_found_at = 0.0;
    fude_zoom_hand_lines(&_page->scene, &_page->hand_lines, &_page->hand_strokes);   // (its lines found now)
    _page->hand_revision = _page->scene.revision;
    if(fude_mlkit_state() == FUDE_MLKIT_IDLE || fude_mlkit_state() == FUDE_MLKIT_FAILED) {
        fude_mlkit_set_model(fude_zoom_ink_model());
        fude_mlkit_prepare();   // (a download the first time)
    }
}

// See page_parts.h. A line being read, and a To text under way, are marked stale (their strokes forgotten: they were
// the canvas left's), so each answer is dropped as it comes rather than kept or written in the canvas open now: ids
// are a canvas's own, so another canvas's line could have the same key. To text asked for after a line is not done
// either.
void fude_zoom_page_hand_forget(fude_zoom_page* _page) {
    if(_page->reading) {
        _page->read_stale = true;
        rde_arr_clear(&_page->read_strokes);
    }
    _page->hand_asking_stale = _page->hand_asking != 0u;
    _page->read_wanted = false;
    _page->hand_on = false;
    if(rde_arr_is_inited(&_page->hand_lines)) {
        rde_arr_clear(&_page->hand_lines);
        rde_arr_clear(&_page->hand_strokes);
        rde_hash_map_clear(&_page->hand_said);
    }
}

// Once a frame: an answer taken in; the lines found again when the canvas has changed (a second
// apart at most); the next line not read yet handed to the reader — never while To text reads,
// nor while drawing.
void fude_zoom_page_hand_update(fude_zoom_page* _page) {
    if(_page->hand_asking != 0u) {
        c8  _out[1024];
        f64 _ms = 0.0;
        if(!fude_mlkit_poll(_out, sizeof(_out), &_ms)) {
            return;
        }
        c8* _end = rde_strchr(_out, '\n');
        if(_end != NULL) {
            *_end = 0;   // the best candidate
        }
        fude_zoom_page_hand_said _said;
        rde_memset(&_said, 0, sizeof(_said));
        const u64 _key = _page->hand_asking;
        usize _len = rde_strlen(_out);
        if(_len >= sizeof(_said.text)) {
            _len = sizeof(_said.text) - 1u;
            while(_len > 0u && ((u8)_out[_len] & 0xC0u) == 0x80u) {
                _len--;   // (cut at a whole character)
            }
        }
        rde_memcpy(_said.text, _out, _len);
        // (kept under its key while the lines are; a line read again keeps its newest reading; one read on a canvas
        // left since, let go)
        if(!_page->hand_asking_stale && rde_arr_is_inited(&_page->hand_lines) && !rde_hash_map_update_entry(&_page->hand_said, (const any)&_key, (const any)&_said)) {
            rde_hash_map_add(&_page->hand_said, (const any)&_key, (const any)&_said);
        }
        _page->hand_asking       = 0u;
        _page->hand_asking_stale = false;
        if(_page->read_wanted) {
            _page->read_wanted = false;
            fude_zoom_page_read_selection(_page->app, NULL, 0);   // (To text, asked for meanwhile)
            return;
        }
    }
    if(!_page->hand_on || !_page->open || _page->reading || _page->drawing || fude_mlkit_state() != FUDE_MLKIT_READY) {
        return;
    }
    const f64 _now = rde_engine_get_time_now();
    if(_page->hand_revision != _page->scene.revision && _now - _page->hand_found_at >= 1.0) {
        fude_zoom_hand_lines(&_page->scene, &_page->hand_lines, &_page->hand_strokes);
        _page->hand_revision = _page->scene.revision;
        _page->hand_found_at = _now;
    }
    const fude_zoom_hand_line* _lines = (const fude_zoom_hand_line*)_page->hand_lines.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->hand_lines); _i++) {
        if(fude_zoom_page_hand_text(_page, _lines[_i].key) != NULL) {
            continue;
        }
        // Its strokes as ink, in the order drawn: its own units, a usual stroke about a hundred tall.
        const fude_zoom_scene* _s = &_page->scene;
        const f64 _k = 100.0 / rde_math_max_f64(_lines[_i].height, 1e-300);
        fude_ink _ink;
        fude_ink_init(&_ink);
        rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
        f64 _t = 0.0;
        for(u32 _j = 0; _j < _lines[_i].count; _j++) {
            const u32               _object = ((const u32*)_page->hand_strokes.memory)[_lines[_i].first + _j];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            rde_arr_clear(&_q);
            rde_arr_add_n(&_q, _o->count);
            if(_o->count == 0 || !fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                continue;
            }
            for(u32 _p = 0; _p < _o->count; _p++) {
                const fude_zoom_v2 _at = fude_zoom_scene_point_at(_o, &((const fude_zoom_qpoint*)_q.memory)[_p]);
                const rde_vec_2F   _v  = { (f32)((_at.x - _lines[_i].box.min_x) * _k), (f32)((_at.y - _lines[_i].box.min_y) * _k) };
                _ink.sample_time = _t;
                _t += 0.008;
                if(_p == 0) {
                    fude_ink_begin(&_ink, _v, true);
                } else {
                    fude_ink_extend(&_ink, _v);
                }
            }
            fude_ink_end(&_ink);
            _t += 0.25;
        }
        rde_arr_free(&_q);
        if(fude_mlkit_recognize(&_ink, NULL)) {
            _page->hand_asking = _lines[_i].key;
        }
        fude_ink_destroy(&_ink);
        return;   // (one at a time)
    }
}

// To text: the selected strokes, in the order they were drawn, handed to the
// reader (on the screen's coordinates); the answer comes in a later frame.
void fude_zoom_page_read_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->reading) {
        return;
    }
    if(_page->hand_asking != 0u) {
        _page->read_wanted = true;   // (a line of writing being read for Find: this after it)
        return;
    }
    const FUDE_MLKIT_ _state = fude_mlkit_state();
    if(_state == FUDE_MLKIT_IDLE || _state == FUDE_MLKIT_FAILED || _state == FUDE_MLKIT_DOWNLOADING) {
        fude_mlkit_set_model(fude_zoom_ink_model());
        fude_mlkit_prepare();   // the model, once (a download the first time)
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_READ_GETTING));
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_selection* _sel = &_page->selection;
    const u32 _n = (u32)rde_arr_length(&_sel->picks);
    rde_arr _items_arr = rde_arr_new(sizeof(fude_zoom_read_item), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = ((const fude_zoom_pick*)_sel->picks.memory)[_i].object;
        if(fude_zoom_scene_object(_s, _o)->kind == FUDE_ZOOM_KIND_STROKE) {
            const fude_zoom_read_item _item = { fude_zoom_scene_object(_s, _o)->z, _o };
            rde_arr_add(&_items_arr, (any)&_item);
        }
    }
    const u32 _k = (u32)rde_arr_length(&_items_arr);
    fude_zoom_read_item* _items = (fude_zoom_read_item*)_items_arr.memory;
    if(_k == 0) {
        rde_arr_free(&_items_arr);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_READ_NOTHING));
        return;
    }
    rde_sort(_items, _k, sizeof(_items[0]), fude_zoom_read_by_z);
    fude_ink _ink;
    fude_ink_init(&_ink);
    rde_arr _q = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    rde_arr_clear(&_page->read_strokes);
    // Their box kept on the canvas (the camera's frame's units, its zoom with it), not on the screen: the view may move
    // before the answer comes, and the text goes where the handwriting is.
    _page->read_box   = fude_zoom_box_empty();
    _page->read_frame = _s->camera.frame;
    _page->read_z     = _s->camera.z;
    const fude_zoom_sim _to_frame = fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera));
    f64 _t = 0.0;
    for(u32 _i = 0; _i < _k; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _items[_i].object);
        rde_arr_resize(&_q, _o->count);
        if(_o->count == 0 || !fude_zoom_scene_points(_s, _items[_i].object, (fude_zoom_qpoint*)_q.memory)) {
            continue;
        }
        if(_i == 0) {
            _page->read_color = _o->color;
        }
        rde_arr_add(&_page->read_strokes, (any)&_o->id);
        const fude_zoom_sim     _to = fude_zoom_page_frame_sim(_page, _o->frame);
        const fude_zoom_qpoint* _p  = (const fude_zoom_qpoint*)_q.memory;
        for(u32 _j = 0; _j < _o->count; _j++) {
            const fude_zoom_v2 _at = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_p[_j]));
            const fude_zoom_v2 _on = fude_zoom_sim_apply(_to_frame, _at);
            _page->read_box = fude_zoom_box_union(_page->read_box, (fude_zoom_box){ _on.x, _on.y, _on.x, _on.y });
            _ink.sample_time = _t;
            _t += 0.008;
            if(_j == 0) {
                fude_ink_begin(&_ink, (rde_vec_2F){ (f32)_at.x, (f32)_at.y }, true);
            } else {
                fude_ink_extend(&_ink, (rde_vec_2F){ (f32)_at.x, (f32)_at.y });
            }
        }
        fude_ink_end(&_ink);
        _t += 0.25;   // a pen lifted between strokes
    }
    rde_arr_free(&_q);
    rde_arr_free(&_items_arr);
    _page->reading    = fude_mlkit_recognize(&_ink, NULL);
    _page->read_stale = false;
    fude_ink_destroy(&_ink);
    if(!_page->reading) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_READ_GETTING));
    }
}

// The answer in: its best reading written where the handwriting was, a text
// of about its height, in its colour; the strokes let go (one undo step: the
// first undo brings the handwriting back). An answer for a canvas left since
// (read_stale), or for strokes not all there any more, is let go unused.
void fude_zoom_page_read_update(fude_zoom_page* _page) {
    if(!_page->reading) {
        return;
    }
    c8  _out[2048];
    f64 _ms = 0.0;
    if(!fude_mlkit_poll(_out, sizeof(_out), &_ms)) {
        return;
    }
    _page->reading = false;
    if(_page->read_stale) {
        _page->read_stale = false;   // (read on a canvas left since: let go)
        rde_arr_clear(&_page->read_strokes);
        return;
    }
    c8* _end = rde_strchr(_out, '\n');
    if(_end != NULL) {
        *_end = 0;   // the best candidate: the first line
    }
    if(_out[0] == 0 || rde_arr_length(&_page->read_strokes) == 0) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_READ_NONE));
        return;
    }
    fude_zoom_scene* _s = &_page->scene;
    // The strokes it read, found again by their ids. One of them gone meanwhile (rubbed out, undone) and the answer no
    // longer says what is there: it is let go, and nothing is written or taken.
    const u32 _n = (u32)rde_arr_length(&_page->read_strokes);
    rde_arr _read = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _o = fude_zoom_scene_find_object(_s, ((const fude_zoom_id*)_page->read_strokes.memory)[_i]);
        if(_o == FUDE_ZOOM_NONE || !(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_object(_s, _o)->kind != FUDE_ZOOM_KIND_STROKE) {
            break;
        }
        rde_arr_add(&_read, (any)&_o);
    }
    rde_arr_clear(&_page->read_strokes);
    if((u32)rde_arr_length(&_read) != _n) {
        rde_arr_free(&_read);
        return;
    }
    // Written where the handwriting is, wherever the view has gone since: in the frame the reading began in, at the top
    // left of the handwriting's box there, as tall as it was on the screen then (read_z): 0.8 of its height, 10 to 240
    // points.
    const u32           _frame = _page->read_frame;
    const f64           _k     = _page->read_z;
    if(_frame >= fude_zoom_scene_frame_count(_s) || !(_k > 0.0)) {
        rde_arr_free(&_read);   // (no such frame any more: nothing written or taken)
        return;
    }
    const fude_zoom_box _b  = _page->read_box;
    const f32 _px = (f32)rde_math_min_f64(rde_math_max_f64((_b.max_y - _b.min_y) * _k * 0.8, 10.0), 240.0);
    f32 _w, _h;
    fude_zoom_page_measure_words(_page, _out, _px, &_w, &_h);
    const fude_zoom_v2  _tl = { _b.min_x, _b.max_y };
    const u32 _text = fude_zoom_scene_add_text(_s, _frame, (fude_zoom_place){ _tl, 0.0, 1.0 }, (f64)_px / _k, (f64)_w / _k, (f64)_h / _k, FUDE_ZOOM_TEXT_PLAIN,
                                               _page->read_color, (rde_color){ 0, 0, 0, 0 }, _out, (u32)rde_strlen(_out), 0);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_scene_set_alive(_s, ((const u32*)_read.memory)[_i], false);
    }
    fude_zoom_select_clear(&_page->selection);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _text)->box, (const u32*)_read.memory, _n, &_text, 1u);
    rde_arr_free(&_read);
}
