#include "viewer.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See viewer.h.
// ===========================================================================

#define KANA_VIEWER_MARGIN     28.0f     // screen units around everything
#define KANA_VIEWER_LINE       44.0f     // text line height
#define KANA_VIEWER_KANA_SIZE  30.0f     // a stroke-drawn kana in a reading

void kana_viewer_init(kana_viewer* _viewer, const kana_kanji_db* _db) {
    memset(_viewer, 0, sizeof(*_viewer));
    _viewer->db   = _db;
    _viewer->list = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_glyph_init(&_viewer->glyph, _db);
}

void kana_viewer_destroy(kana_viewer* _viewer) {
    if(rde_arr_is_inited(&_viewer->list)) {
        rde_arr_free(&_viewer->list);
    }
    kana_glyph_destroy(&_viewer->glyph);
    memset(_viewer, 0, sizeof(*_viewer));
}

b8 kana_viewer_available(const kana_viewer* _viewer) {
    return _viewer->db != NULL && _viewer->db->count > 0;
}

void kana_viewer_replay(kana_viewer* _viewer) {
    _viewer->started = rde_engine_get_time_now();
}

void kana_viewer_show(kana_viewer* _viewer, const u32* _records, u32 _count, u32 _position) {
    if(!kana_viewer_available(_viewer) || _count == 0) {
        return;
    }

    rde_arr_clear(&_viewer->list);
    memcpy(rde_arr_add_n(&_viewer->list, _count), _records, (usize)_count * sizeof(u32));
    _viewer->position = _position < _count ? _position : 0u;
    _viewer->open     = true;
    kana_viewer_replay(_viewer);
}

b8 kana_viewer_show_codepoint(kana_viewer* _viewer, u32 _codepoint) {
    for(u32 _i = 0; kana_viewer_available(_viewer) && _i < _viewer->db->count; _i++) {
        kana_kanji_info _info;
        if(kana_kanji_at(_viewer->db, _i, &_info) && _info.codepoint == _codepoint) {
            kana_viewer_show(_viewer, &_i, 1u, 0u);
            return true;
        }
    }
    return false;
}

void kana_viewer_close(kana_viewer* _viewer) {
    _viewer->open = false;
}

void kana_viewer_next(kana_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + 1u) % _n;
        kana_viewer_replay(_viewer);
    }
}

void kana_viewer_prev(kana_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + _n - 1u) % _n;
        kana_viewer_replay(_viewer);
    }
}

// --- the page ----------------------------------------------------------------------

// Text _px screen units tall (em), baseline at _y.
RDE_INTERNAL void kana_viewer_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

RDE_INTERNAL b8 kana_viewer_is_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

void kana_viewer_render(kana_viewer* _viewer, rde_font* _font, f32 _font_px, rde_vec_2I _window, rde_vec_4I _insets, f32 _bottom_bar) {
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0) {
        return;
    }

    kana_kanji_info _info;
    const u32 _record = ((const u32*)_viewer->list.memory)[_viewer->position];
    if(!kana_kanji_at(_viewer->db, _record, &_info)) {
        return;
    }

    const f32 _hw     = (f32)_window.x * 0.5f;
    const f32 _hh     = (f32)_window.y * 0.5f;
    const f32 _top    = _hh - (f32)_insets.y - KANA_VIEWER_MARGIN;
    const f32 _bottom = -_hh + (f32)_insets.w + _bottom_bar + KANA_VIEWER_MARGIN;
    const f32 _left   = -_hw + (f32)_insets.x + KANA_VIEWER_MARGIN;
    const f32 _right  = _hw - (f32)_insets.z - KANA_VIEWER_MARGIN;

    // --- header: what it is -------------------------------------------------------
    const b8 _kana = kana_viewer_is_kana(_info.codepoint);
    c8 _line[256];
    c8 _extra[96] = "";
    if(_info.jlpt_n != 0) {
        snprintf(_extra, sizeof(_extra), "   JLPT N%u", _info.jlpt_n);
    }
    {
        const usize _len = strlen(_extra);
        if(_info.grade >= 1 && _info.grade <= 6)       { snprintf(_extra + _len, sizeof(_extra) - _len, "   grade %u", _info.grade); }
        else if(_info.grade == 8)                      { snprintf(_extra + _len, sizeof(_extra) - _len, "   secondary school"); }
        else if(_info.grade == 9 || _info.grade == 10) { snprintf(_extra + _len, sizeof(_extra) - _len, "   jinmeiyou (names)"); }
    }
    snprintf(_line, sizeof(_line), "%s   %u stroke%s%s      %u / %u",
             _kana ? (_info.codepoint < 0x30A0u ? "Hiragana" : "Katakana") : "Kanji",
             _info.strokes, _info.strokes == 1 ? "" : "s", _extra,
             _viewer->position + 1u, (u32)rde_arr_length(&_viewer->list));
    kana_viewer_text(_font, _font_px, _line, _left, _top - 16.0f, 22.0f, kana_theme_active()->text);

    // --- the character ---------------------------------------------------------------
    const f32        _text_lines = _kana ? 0.0f : 3.0f;
    const f32        _room_h     = (_top - 48.0f) - (_bottom + _text_lines * KANA_VIEWER_LINE);
    const f32        _size       = fmaxf(120.0f, fminf(_right - _left, _room_h));
    const rde_vec_2F _tl         = { -_size * 0.5f, _top - 48.0f };

    kana_glyph_box(_tl, _size);
    kana_glyph_writing(&_viewer->glyph, &_info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 18.0f);

    // --- readings and meaning (kanji) ------------------------------------------------
    if(!_kana) {
        const f32 _y0 = _tl.y - _size - 20.0f;
        kana_viewer_text(_font, _font_px, "On", _left, _y0 - KANA_VIEWER_KANA_SIZE * 0.7f, 20.0f, kana_theme_active()->text_soft);
        kana_glyph_reading(&_viewer->glyph, kana_kanji_on(_viewer->db, &_info), (rde_vec_2F){ _left + 60.0f, _y0 },
                           KANA_VIEWER_KANA_SIZE, _right, kana_theme_active()->ink, kana_theme_active()->text_soft);
        kana_viewer_text(_font, _font_px, "Kun", _left, _y0 - KANA_VIEWER_LINE - KANA_VIEWER_KANA_SIZE * 0.7f, 20.0f, kana_theme_active()->text_soft);
        kana_glyph_reading(&_viewer->glyph, kana_kanji_kun(_viewer->db, &_info), (rde_vec_2F){ _left + 60.0f, _y0 - KANA_VIEWER_LINE },
                           KANA_VIEWER_KANA_SIZE, _right, kana_theme_active()->ink, kana_theme_active()->text_soft);

        const c8* _meanings = kana_kanji_meanings(_viewer->db, &_info);
        snprintf(_line, sizeof(_line), "%s", _meanings[0] != 0 ? _meanings : "(no meaning listed)");
        kana_viewer_text(_font, _font_px, _line, _left, _y0 - 2.0f * KANA_VIEWER_LINE - KANA_VIEWER_KANA_SIZE * 0.7f, 22.0f, kana_theme_active()->text);
    }
}
