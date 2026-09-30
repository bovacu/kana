#include "practice.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See practice.h.
// ===========================================================================

#define KANA_PRACTICE_MARGIN     16.0f
#define KANA_PRACTICE_GAP        14.0f     // between squares
#define KANA_PRACTICE_LABEL_H    40.0f     // score and feedback under a square
#define KANA_PRACTICE_SQUARE_MAX 190.0f
#define KANA_PRACTICE_DEMO_MAX   190.0f
#define KANA_PRACTICE_DEMO_REST  1.5       // seconds the finished demo rests before writing again

// The width of the pen in a square's units: the reference's own stroke width.
#define KANA_PRACTICE_PEN_RADIUS (KANA_PRACTICE_UNITS * KANA_GLYPH_WIDTH / KANA_KANJI_BOX * 0.5f)

RDE_INTERNAL void kana_practice_fresh_ink(kana_ink* _ink) {
    kana_ink_init(_ink);
    _ink->constant_radius = KANA_PRACTICE_PEN_RADIUS;
}

void kana_practice_init(kana_practice* _practice, const kana_kanji_db* _db) {
    memset(_practice, 0, sizeof(*_practice));
    _practice->db         = _db;
    _practice->squares    = KANA_PRACTICE_DEFAULT_SQUARES;
    _practice->writing    = -1;
    _practice->strokes_in  = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _practice->set         = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _practice->set_results = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    kana_glyph_init(&_practice->glyph, _db);
    for(u32 _i = 0; _i < KANA_PRACTICE_MAX_SQUARES; _i++) {
        kana_practice_fresh_ink(&_practice->inks[_i]);
    }
}

void kana_practice_destroy(kana_practice* _practice) {
    for(u32 _i = 0; _i < KANA_PRACTICE_MAX_SQUARES; _i++) {
        kana_ink_destroy(&_practice->inks[_i]);
    }
    rde_arr* _arrays[] = { &_practice->strokes_in, &_practice->set, &_practice->set_results };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    kana_glyph_destroy(&_practice->glyph);
    memset(_practice, 0, sizeof(*_practice));
}

void kana_practice_clear(kana_practice* _practice) {
    for(u32 _i = 0; _i < KANA_PRACTICE_MAX_SQUARES; _i++) {
        kana_ink_destroy(&_practice->inks[_i]);
        kana_practice_fresh_ink(&_practice->inks[_i]);
    }
    rde_arr_clear(&_practice->strokes_in);
    memset(_practice->scores, 0, sizeof(_practice->scores));
    _practice->scored    = false;
    _practice->changed   = false;
    _practice->writing   = -1;
    _practice->status[0] = 0;
}

// Squares cleared, the demo restarted, the history re-read: one character.
RDE_INTERNAL b8 kana_practice_load(kana_practice* _practice, u32 _record) {
    if(_practice->db == NULL || !kana_kanji_at(_practice->db, _record, &_practice->info)) {
        return false;
    }
    _practice->record      = _record;
    _practice->demo_start  = rde_engine_get_time_now();
    _practice->demo_done   = 0.0;
    _practice->has_summary = kana_history_summarize(_practice->info.codepoint, &_practice->summary);
    kana_practice_clear(_practice);
    return true;
}

void kana_practice_open_set(kana_practice* _practice, const u32* _records, u32 _count) {
    if(_count == 0 || _practice->db == NULL) {
        return;
    }
    _count = _count > KANA_PRACTICE_SET_MAX ? KANA_PRACTICE_SET_MAX : _count;

    rde_arr_clear(&_practice->set);
    rde_arr_clear(&_practice->set_results);
    rde_memcpy(rde_arr_add_n(&_practice->set, _count), (any)_records, sizeof(u32) * _count);
    f32* _results = (f32*)rde_arr_add_n(&_practice->set_results, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        _results[_i] = -1.0f;
    }
    _practice->set_position = 0;
    _practice->summary_open = false;

    if(kana_practice_load(_practice, _records[0])) {
        _practice->open = true;
    }
}

void kana_practice_open(kana_practice* _practice, u32 _record) {
    kana_practice_open_set(_practice, &_record, 1);
}

b8 kana_practice_in_set(const kana_practice* _practice) {
    return rde_arr_length(&_practice->set) > 1;
}

b8 kana_practice_at_last(const kana_practice* _practice) {
    return _practice->set_position + 1u >= (u32)rde_arr_length(&_practice->set);
}

void kana_practice_next(kana_practice* _practice) {
    if(!_practice->open || _practice->summary_open) {
        return;
    }
    if(_practice->writing >= 0) {
        kana_practice_pen_up(_practice);
    }

    // Written and not scored: score it, which saves it — moving on loses nothing.
    for(u32 _i = 0; _i < _practice->squares && _practice->changed; _i++) {
        if(kana_ink_stroke_count(&_practice->inks[_i]) > 0) {
            kana_practice_score(_practice);
            break;
        }
    }

    const u32 _count = (u32)rde_arr_length(&_practice->set);
    while(_practice->set_position + 1u < _count) {
        _practice->set_position++;
        if(kana_practice_load(_practice, ((const u32*)_practice->set.memory)[_practice->set_position])) {
            return;
        }
    }
    _practice->summary_open = true;   // that was the last
}

u32 kana_practice_weak_count(const kana_practice* _practice) {
    const f32* _results = (const f32*)_practice->set_results.memory;
    u32        _n       = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->set_results); _i++) {
        _n += (_results[_i] >= 0.0f && _results[_i] < KANA_THEME_GRADE_GOOD) ? 1u : 0u;
    }
    return _n;
}

typedef struct {
    u32 record;
    f32 result;
} kana_practice_ranked;

RDE_INTERNAL int kana_practice_by_result(const void* _a, const void* _b) {
    const f32 _x = ((const kana_practice_ranked*)_a)->result;
    const f32 _y = ((const kana_practice_ranked*)_b)->result;
    return _x < _y ? -1 : _x > _y ? 1 : 0;
}

void kana_practice_weakest_again(kana_practice* _practice) {
    // The ones under good, weakest first.
    const u32*           _records = (const u32*)_practice->set.memory;
    const f32*           _results = (const f32*)_practice->set_results.memory;
    kana_practice_ranked _ranked[KANA_PRACTICE_SET_MAX];
    u32                  _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->set); _i++) {
        if(_results[_i] >= 0.0f && _results[_i] < KANA_THEME_GRADE_GOOD) {
            _ranked[_n++] = (kana_practice_ranked){ _records[_i], _results[_i] };
        }
    }
    if(_n == 0) {
        return;
    }
    qsort(_ranked, _n, sizeof(_ranked[0]), kana_practice_by_result);

    u32 _weak[KANA_PRACTICE_SET_MAX];
    for(u32 _i = 0; _i < _n; _i++) {
        _weak[_i] = _ranked[_i].record;
    }
    kana_practice_open_set(_practice, _weak, _n);
}

void kana_practice_close(kana_practice* _practice) {
    if(_practice->writing >= 0) {
        kana_practice_pen_up(_practice);
    }
    _practice->open         = false;
    _practice->summary_open = false;
}

void kana_practice_set_squares(kana_practice* _practice, u32 _count) {
    _count = _count < 1u ? 1u : (_count > KANA_PRACTICE_MAX_SQUARES ? KANA_PRACTICE_MAX_SQUARES : _count);

    // Squares that go away take their drawings with them.
    for(u32 _i = _count; _i < _practice->squares; _i++) {
        kana_ink_destroy(&_practice->inks[_i]);
        kana_practice_fresh_ink(&_practice->inks[_i]);
    }
    // ...and their strokes out of the undo order.
    u8* _order = (u8*)_practice->strokes_in.memory;
    u32 _kept  = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->strokes_in); _i++) {
        if(_order[_i] < _count) {
            _order[_kept++] = _order[_i];
        }
    }
    _practice->strokes_in.count = _kept;   // rde_arr has no truncate: rde_arr_clear's operation, to a length

    _practice->squares = _count;
}

void kana_practice_undo(kana_practice* _practice) {
    const u32 _n = (u32)rde_arr_length(&_practice->strokes_in);
    if(_n == 0 || _practice->writing >= 0) {
        return;
    }

    const u8 _square = ((const u8*)_practice->strokes_in.memory)[_n - 1u];
    _practice->strokes_in.count = _n - 1u;
    kana_ink_undo(&_practice->inks[_square]);
    _practice->scores[_square].empty = true;   // its mark no longer matches its drawing
    _practice->changed = true;
}

void kana_practice_score(kana_practice* _practice) {
    if(_practice->writing >= 0) {
        kana_practice_pen_up(_practice);
    }

    f32 _sum    = 0.0f;
    u32 _scored = 0;
    for(u32 _i = 0; _i < _practice->squares; _i++) {
        _practice->scores[_i] = kana_score_drawing(_practice->db, &_practice->info, &_practice->inks[_i]);
        if(!_practice->scores[_i].empty) {
            _sum += _practice->scores[_i].score;
            _scored++;
        }
    }
    _practice->scored = true;

    if(_scored == 0) {
        snprintf(_practice->status, sizeof(_practice->status), "Nothing written yet: write in the squares with the pen");
        return;
    }

    const f32 _average = _sum / (f32)_scored;
    if(_practice->set_position < (u32)rde_arr_length(&_practice->set_results)) {
        ((f32*)_practice->set_results.memory)[_practice->set_position] = _average;   // this run's, for the set's summary
    }
    if(!_practice->changed) {
        snprintf(_practice->status, sizeof(_practice->status), "Average %.0f  (already saved)", (f64)_average);
        return;
    }

    const kana_ink* _drawings[KANA_PRACTICE_MAX_SQUARES];
    for(u32 _i = 0; _i < _practice->squares; _i++) {
        _drawings[_i] = &_practice->inks[_i];
    }
    const b8 _saved = kana_history_save(_practice->info.codepoint, (u64)time(NULL), _practice->squares, _practice->scores,
                                        _drawings, KANA_PRACTICE_UNITS);
    if(_saved) {
        _practice->changed     = false;
        _practice->has_summary = kana_history_summarize(_practice->info.codepoint, &_practice->summary);
    }
    snprintf(_practice->status, sizeof(_practice->status), "Average %.0f over %u square%s  -  %s",
             (f64)_average, _scored, _scored == 1 ? "" : "s", _saved ? "saved" : "COULD NOT SAVE");
}

// --- the pen -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F kana_practice_local(const kana_practice* _practice, u32 _square, rde_vec_2F _screen) {
    // From the square's bottom-left corner, in its own units, kept inside it.
    const f32 _k = KANA_PRACTICE_UNITS / _practice->square_size;
    const rde_vec_2F _bl = { _practice->square_tl[_square].x, _practice->square_tl[_square].y - _practice->square_size };
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _bl.x) * _k, 0.0f, KANA_PRACTICE_UNITS),
        rde_math_clamp_f32((_screen.y - _bl.y) * _k, 0.0f, KANA_PRACTICE_UNITS)
    };
}

void kana_practice_pen_down(kana_practice* _practice, rde_vec_2F _screen) {
    _practice->writing = -1;
    if(_practice->square_size <= 0.0f) {
        return;
    }

    for(u32 _i = 0; _i < _practice->squares; _i++) {
        const rde_vec_2F _tl = _practice->square_tl[_i];
        if(_screen.x >= _tl.x && _screen.x <= _tl.x + _practice->square_size &&
           _screen.y <= _tl.y && _screen.y >= _tl.y - _practice->square_size) {
            _practice->writing = (i32)_i;
            break;
        }
    }
    if(_practice->writing < 0) {
        return;
    }

    kana_ink* _ink = &_practice->inks[_practice->writing];
    _ink->zoom = _practice->square_size / KANA_PRACTICE_UNITS;   // the min step stays a screen unit
    kana_ink_begin(_ink, kana_practice_local(_practice, (u32)_practice->writing, _screen), true, false);

    u8 _square = (u8)_practice->writing;
    rde_arr_add(&_practice->strokes_in, &_square);
    _practice->scores[_practice->writing].empty = true;   // written in again: its old mark is gone
    _practice->changed = true;
}

void kana_practice_pen_moved(kana_practice* _practice, rde_vec_2F _screen) {
    if(_practice->writing >= 0) {
        kana_ink_extend(&_practice->inks[_practice->writing], kana_practice_local(_practice, (u32)_practice->writing, _screen));
    }
}

void kana_practice_pen_up(kana_practice* _practice) {
    if(_practice->writing >= 0) {
        kana_ink_end(&_practice->inks[_practice->writing]);
        _practice->writing = -1;
    }
}

// --- drawing -----------------------------------------------------------------------

// The end of a set: every character in it and how it went this run.
RDE_INTERNAL void kana_practice_render_summary(kana_practice* _practice, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme   = kana_theme_active();
    const u32*        _records = (const u32*)_practice->set.memory;
    const f32*        _results = (const f32*)_practice->set_results.memory;
    const u32         _count   = (u32)rde_arr_length(&_practice->set);

    f32 _sum  = 0.0f;
    u32 _done = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_results[_i] >= 0.0f) {
            _sum += _results[_i];
            _done++;
        }
    }
    const u32 _weak = kana_practice_weak_count(_practice);

    c8 _line[160];
    kana_draw_text(_font, _font_px, "Set done", _left, _top - 32.0f, 26.0f, _theme->text);
    if(_done > 0) {
        snprintf(_line, sizeof(_line), "%u of %u practised, average %.0f", _done, _count, (f64)(_sum / (f32)_done));
    } else {
        snprintf(_line, sizeof(_line), "Nothing was practised in this set");
    }
    kana_draw_text(_font, _font_px, _line, _left, _top - 64.0f, 19.0f, _theme->text);
    if(_weak > 0) {
        snprintf(_line, sizeof(_line), "%u under %.0f: \"Weakest again\" practises them, weakest first", _weak, (f64)KANA_THEME_GRADE_GOOD);
    } else if(_done > 0) {
        snprintf(_line, sizeof(_line), "All good this run");
    } else {
        _line[0] = 0;
    }
    kana_draw_text(_font, _font_px, _line, _left, _top - 92.0f, 17.0f, _theme->text_soft);

    // The set, in order, as big as fits the room left.
    const f32 _grid_top = _top - 112.0f;
    const f32 _width    = _right - _left;
    const f32 _height   = _grid_top - _bottom;
    f32       _cell     = 96.0f;
    u32       _cols     = 1;
    while(_cell > 48.0f) {
        _cols = (u32)fmaxf(1.0f, floorf(_width / _cell));
        if((f32)((_count + _cols - 1u) / _cols) * _cell <= _height) {
            break;
        }
        _cell -= 4.0f;
    }
    _cols = (u32)fmaxf(1.0f, floorf(_width / _cell));

    const f32 _glyph = _cell * 0.55f;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x = _left + (f32)(_i % _cols) * _cell;
        const f32 _y = _grid_top - (f32)(_i / _cols) * _cell;
        if(_y - _cell < _bottom - 1.0f) {
            break;
        }
        kana_kanji_info _info;
        if(!kana_kanji_at(_practice->db, _records[_i], &_info)) {
            continue;
        }
        kana_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, _theme->line);
        kana_glyph_character(&_practice->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.06f }, _glyph,
                             _results[_i] >= 0.0f ? _theme->ink : _theme->ghost);
        if(_results[_i] >= 0.0f) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_results[_i]);
            kana_draw_text(_font, _font_px, _line, _x + 6.0f, _y - _cell + 9.0f, 16.0f, kana_theme_grade(_results[_i]));
        }
    }
}

void kana_practice_render(kana_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_practice->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_PRACTICE_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_PRACTICE_MARGIN;
    const f32        _width  = _right - _left;
    const kana_kanji_info* _info = &_practice->info;

    if(_practice->summary_open) {
        _practice->square_size = 0.0f;   // no squares to write in
        kana_practice_render_summary(_practice, _font, _font_px, _left, _right, _top, _bottom);
        return;
    }

    // --- the demo: the character writing itself, over and over --------------------
    const f32        _demo    = fminf(KANA_PRACTICE_DEMO_MAX, _width * 0.3f);
    const rde_vec_2F _demo_tl = { _left, _top - 8.0f };
    kana_glyph_box(_demo_tl, _demo);
    const f64 _now = rde_engine_get_time_now();
    if(kana_glyph_writing(&_practice->glyph, _info, _demo_tl, _demo, _now - _practice->demo_start, _font, _font_px, 13.0f)) {
        // Finished: rest a moment on the whole character, then write it again.
        if(_practice->demo_done <= 0.0) {
            _practice->demo_done = _now;
        } else if(_now - _practice->demo_done > KANA_PRACTICE_DEMO_REST) {
            _practice->demo_start = _now;
            _practice->demo_done  = 0.0;
        }
    }

    // --- beside it: what it is, how it has gone before, how this one went ---------------
    const f32 _tx = _left + _demo + 20.0f;
    f32       _ty = _top - 30.0f;
    c8        _line[160];
    c8 _where[32] = "Practice";
    if(kana_practice_in_set(_practice)) {
        snprintf(_where, sizeof(_where), "%u of %u", _practice->set_position + 1u, (u32)rde_arr_length(&_practice->set));
    }
    if(_info->jlpt_n != 0) {
        snprintf(_line, sizeof(_line), "%s   %u stroke%s   JLPT N%u", _where, _info->strokes, _info->strokes == 1 ? "" : "s", _info->jlpt_n);
    } else {
        snprintf(_line, sizeof(_line), "%s   %u stroke%s", _where, _info->strokes, _info->strokes == 1 ? "" : "s");
    }
    kana_draw_text(_font, _font_px, _line, _tx, _ty, 22.0f, kana_theme_active()->text);

    _ty -= 38.0f;
    const c8* _on  = kana_kanji_on(_practice->db, _info);
    const c8* _kun = kana_kanji_kun(_practice->db, _info);
    if(_on[0] != 0 || _kun[0] != 0) {
        const f32 _x = kana_glyph_reading(&_practice->glyph, _on, (rde_vec_2F){ _tx, _ty + 22.0f }, 24.0f, _right, kana_theme_active()->ink, kana_theme_active()->text_soft);
        kana_glyph_reading(&_practice->glyph, _kun, (rde_vec_2F){ _x + 18.0f, _ty + 22.0f }, 24.0f, _right, kana_theme_active()->ink, kana_theme_active()->text_soft);
        _ty -= 34.0f;
    }
    const c8* _meanings = kana_kanji_meanings(_practice->db, _info);
    if(_meanings[0] != 0) {
        snprintf(_line, sizeof(_line), "%.60s", _meanings);
        kana_draw_text(_font, _font_px, _line, _tx, _ty, 17.0f, kana_theme_active()->text_soft);
        _ty -= 30.0f;
    }

    if(_practice->has_summary) {
        snprintf(_line, sizeof(_line), "%u session%s   best %.0f   last %.0f   first %.0f",
                 _practice->summary.sessions, _practice->summary.sessions == 1 ? "" : "s",
                 (f64)_practice->summary.best, (f64)_practice->summary.last, (f64)_practice->summary.first);
    } else {
        snprintf(_line, sizeof(_line), "Not practised yet");
    }
    kana_draw_text(_font, _font_px, _line, _tx, _ty, 17.0f, kana_theme_active()->text);
    _ty -= 30.0f;

    if(_practice->status[0] != 0) {
        kana_draw_text(_font, _font_px, _practice->status, _tx, _ty, 19.0f, kana_theme_active()->text);
    }

    // --- the squares ------------------------------------------------------------------
    const f32 _grid_top = _demo_tl.y - _demo - 24.0f;
    const f32 _room_h   = _grid_top - _bottom;
    u32       _cols     = (u32)fmaxf(1.0f, floorf((_width + KANA_PRACTICE_GAP) / (KANA_PRACTICE_SQUARE_MAX * 0.8f + KANA_PRACTICE_GAP)));
    _cols               = _cols > _practice->squares ? _practice->squares : _cols;
    const u32 _rows     = (_practice->squares + _cols - 1u) / _cols;
    f32       _square   = fminf(KANA_PRACTICE_SQUARE_MAX, (_width - KANA_PRACTICE_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    _square             = fminf(_square, (_room_h - (f32)_rows * (KANA_PRACTICE_LABEL_H + KANA_PRACTICE_GAP)) / (f32)_rows);
    _square             = fmaxf(_square, 60.0f);
    _practice->square_size = _square;

    const rde_vec_2F _half = { (f32)_size.x * 0.5f, (f32)_size.y * 0.5f };
    for(u32 _i = 0; _i < _practice->squares; _i++) {
        const u32        _col = _i % _cols;
        const u32        _row = _i / _cols;
        const rde_vec_2F _tl  = { _left + (f32)_col * (_square + KANA_PRACTICE_GAP),
                                  _grid_top - (f32)_row * (_square + KANA_PRACTICE_LABEL_H + KANA_PRACTICE_GAP) };
        _practice->square_tl[_i] = _tl;

        kana_glyph_box(_tl, _square);

        const kana_score* _s = &_practice->scores[_i];
        const b8 _show = _practice->scored && !_s->empty;
        if(_show) {
            // The model, faint, behind what was written.
            kana_glyph_character(&_practice->glyph, _info->codepoint, _tl, _square, kana_theme_active()->reference);
        }

        kana_ink_render(&_practice->inks[_i], (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / KANA_PRACTICE_UNITS,
                        _half, rde_engine_get_time_now(), false);

        if(_show) {
            c8 _mark[8];
            snprintf(_mark, sizeof(_mark), "%.0f", (f64)_s->score);
            kana_draw_text(_font, _font_px, _mark, _tl.x + _square - 34.0f, _tl.y - 24.0f, 20.0f, kana_theme_grade(_s->score));
            kana_draw_text(_font, _font_px, _s->feedback, _tl.x + 2.0f, _tl.y - _square - 20.0f, 13.0f, kana_theme_active()->text);
        }
    }
}
