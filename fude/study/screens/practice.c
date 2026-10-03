#include "study/screens/practice.h"
#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See practice.h.
// ===========================================================================

#define FUDE_PRACTICE_MARGIN     16.0f
#define FUDE_PRACTICE_GAP        14.0f     // between squares
#define FUDE_PRACTICE_LABEL_H    40.0f     // score and feedback under a square
#define FUDE_PRACTICE_SQUARE_MAX 190.0f
#define FUDE_PRACTICE_DEMO_MAX   190.0f
#define FUDE_PRACTICE_DEMO_REST  1.5       // seconds the finished demo rests before writing again
#define FUDE_PRACTICE_GUIDED_MAX 620.0f    // guided: the one square's size at most
#define FUDE_PRACTICE_GUIDED_TEXT 86.0f    // ...and the room under it for the step (two lines, wrapped) and what the stroke got

// The width of the pen in a square's units: the reference's own stroke width.
#define FUDE_PRACTICE_PEN_RADIUS (FUDE_PRACTICE_UNITS * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f)

RDE_INTERNAL void fude_practice_fresh_ink(fude_ink* _ink) {
    fude_ink_init(_ink);
    _ink->constant_radius = FUDE_PRACTICE_PEN_RADIUS;
}

void fude_practice_init(fude_practice* _practice, const fude_kanji_db* _db) {
    memset(_practice, 0, sizeof(*_practice));
    _practice->db         = _db;
    _practice->squares    = FUDE_PRACTICE_DEFAULT_SQUARES;
    _practice->writing    = -1;
    _practice->strokes_in  = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    _practice->set         = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _practice->set_results = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    fude_glyph_init(&_practice->glyph, _db);
    for(u32 _i = 0; _i < FUDE_PRACTICE_MAX_SQUARES; _i++) {
        fude_practice_fresh_ink(&_practice->inks[_i]);
    }
}

void fude_practice_destroy(fude_practice* _practice) {
    for(u32 _i = 0; _i < FUDE_PRACTICE_MAX_SQUARES; _i++) {
        fude_ink_destroy(&_practice->inks[_i]);
    }
    rde_arr* _arrays[] = { &_practice->strokes_in, &_practice->set, &_practice->set_results };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_glyph_destroy(&_practice->glyph);
    memset(_practice, 0, sizeof(*_practice));
}

// Guided, only the first square is used.
RDE_INTERNAL u32 fude_practice_square_count(const fude_practice* _practice) {
    return _practice->guided ? 1u : _practice->squares;
}

// Every square emptied (and its mark gone); the status stays.
RDE_INTERNAL void fude_practice_clear_squares(fude_practice* _practice) {
    for(u32 _i = 0; _i < FUDE_PRACTICE_MAX_SQUARES; _i++) {
        fude_ink_destroy(&_practice->inks[_i]);
        fude_practice_fresh_ink(&_practice->inks[_i]);
    }
    rde_arr_clear(&_practice->strokes_in);
    memset(_practice->scores, 0, sizeof(_practice->scores));
    _practice->scored      = false;
    _practice->changed     = false;
    _practice->writing     = -1;
    _practice->feedback[0] = 0;
}

void fude_practice_clear(fude_practice* _practice) {
    fude_practice_clear_squares(_practice);
    _practice->status[0] = 0;
    if(_practice->guided) {
        fude_guide_restart(&_practice->guide, rde_engine_get_time_now());   // the step again
    }
}

b8 fude_practice_guiding(const fude_practice* _practice) {
    return _practice->guided && _practice->guide.stage != FUDE_GUIDE_RECALL;
}

void fude_practice_set_guided(fude_practice* _practice, b8 _guided) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }
    _practice->guided = _guided;
    fude_practice_clear(_practice);
    if(_guided) {
        fude_guide_start(&_practice->guide, _practice->db, &_practice->info, rde_engine_get_time_now());
    }
}

void fude_practice_update(fude_practice* _practice) {
    if(_practice->open && _practice->guided && _practice->writing < 0 && fude_guide_update(&_practice->guide, rde_engine_get_time_now())) {
        fude_practice_clear_squares(_practice);   // the next step, on a clean square
    }
}

// Squares cleared, the demo restarted, the history re-read: one character.
RDE_INTERNAL b8 fude_practice_load(fude_practice* _practice, u32 _record) {
    if(_practice->db == NULL || !fude_kanji_at(_practice->db, _record, &_practice->info)) {
        return false;
    }
    _practice->record      = _record;
    _practice->demo_start  = rde_engine_get_time_now();
    _practice->demo_done   = 0.0;
    _practice->has_summary = fude_history_summarize(_practice->info.codepoint, &_practice->summary);
    fude_practice_clear(_practice);
    if(_practice->guided) {
        fude_guide_start(&_practice->guide, _practice->db, &_practice->info, _practice->demo_start);
    }
    return true;
}

void fude_practice_open_set(fude_practice* _practice, const u32* _records, u32 _count) {
    if(_count == 0 || _practice->db == NULL) {
        return;
    }
    _count = _count > FUDE_PRACTICE_SET_MAX ? FUDE_PRACTICE_SET_MAX : _count;

    rde_arr_clear(&_practice->set);
    rde_arr_clear(&_practice->set_results);
    rde_memcpy(rde_arr_add_n(&_practice->set, _count), (any)_records, sizeof(u32) * _count);
    f32* _results = (f32*)rde_arr_add_n(&_practice->set_results, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        _results[_i] = -1.0f;
    }
    _practice->set_position = 0;
    _practice->summary_open = false;

    if(fude_practice_load(_practice, _records[0])) {
        _practice->open = true;
    }
}

void fude_practice_open(fude_practice* _practice, u32 _record) {
    fude_practice_open_set(_practice, &_record, 1);
}

b8 fude_practice_in_set(const fude_practice* _practice) {
    return rde_arr_length(&_practice->set) > 1;
}

b8 fude_practice_at_last(const fude_practice* _practice) {
    return _practice->set_position + 1u >= (u32)rde_arr_length(&_practice->set);
}

void fude_practice_next(fude_practice* _practice) {
    if(!_practice->open || _practice->summary_open) {
        return;
    }
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }

    // Written and not scored: score it, which saves it — moving on loses nothing.
    // (Guided, only a step 3 attempt counts: the others are help.)
    for(u32 _i = 0; _i < fude_practice_square_count(_practice) && _practice->changed && !fude_practice_guiding(_practice); _i++) {
        if(fude_ink_stroke_count(&_practice->inks[_i]) > 0) {
            fude_practice_score(_practice);
            break;
        }
    }

    const u32 _count = (u32)rde_arr_length(&_practice->set);
    while(_practice->set_position + 1u < _count) {
        _practice->set_position++;
        if(fude_practice_load(_practice, ((const u32*)_practice->set.memory)[_practice->set_position])) {
            return;
        }
    }
    _practice->summary_open = true;   // that was the last
}

u32 fude_practice_weak_count(const fude_practice* _practice) {
    const f32* _results = (const f32*)_practice->set_results.memory;
    u32        _n       = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->set_results); _i++) {
        _n += (_results[_i] >= 0.0f && _results[_i] < FUDE_THEME_GRADE_GOOD) ? 1u : 0u;
    }
    return _n;
}

typedef struct {
    u32 record;
    f32 result;
} fude_practice_ranked;

RDE_INTERNAL int fude_practice_by_result(const void* _a, const void* _b) {
    const f32 _x = ((const fude_practice_ranked*)_a)->result;
    const f32 _y = ((const fude_practice_ranked*)_b)->result;
    return _x < _y ? -1 : _x > _y ? 1 : 0;
}

void fude_practice_weakest_again(fude_practice* _practice) {
    // The ones under good, weakest first.
    const u32*           _records = (const u32*)_practice->set.memory;
    const f32*           _results = (const f32*)_practice->set_results.memory;
    fude_practice_ranked _ranked[FUDE_PRACTICE_SET_MAX];
    u32                  _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->set); _i++) {
        if(_results[_i] >= 0.0f && _results[_i] < FUDE_THEME_GRADE_GOOD) {
            _ranked[_n++] = (fude_practice_ranked){ _records[_i], _results[_i] };
        }
    }
    if(_n == 0) {
        return;
    }
    qsort(_ranked, _n, sizeof(_ranked[0]), fude_practice_by_result);

    u32 _weak[FUDE_PRACTICE_SET_MAX];
    for(u32 _i = 0; _i < _n; _i++) {
        _weak[_i] = _ranked[_i].record;
    }
    fude_practice_open_set(_practice, _weak, _n);
}

void fude_practice_close(fude_practice* _practice) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }
    _practice->open         = false;
    _practice->summary_open = false;
}

void fude_practice_set_squares(fude_practice* _practice, u32 _count) {
    _count = _count < 1u ? 1u : (_count > FUDE_PRACTICE_MAX_SQUARES ? FUDE_PRACTICE_MAX_SQUARES : _count);

    // Squares that go away take their drawings with them.
    for(u32 _i = _count; _i < _practice->squares; _i++) {
        fude_ink_destroy(&_practice->inks[_i]);
        fude_practice_fresh_ink(&_practice->inks[_i]);
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

void fude_practice_undo(fude_practice* _practice) {
    const u32 _n = (u32)rde_arr_length(&_practice->strokes_in);
    if(_n == 0 || _practice->writing >= 0) {
        return;
    }

    const u8 _square = ((const u8*)_practice->strokes_in.memory)[_n - 1u];
    _practice->strokes_in.count = _n - 1u;
    fude_ink_undo(&_practice->inks[_square]);
    _practice->scores[_square].empty = true;   // its mark no longer matches its drawing
    _practice->changed = true;
    if(_practice->guided) {
        fude_guide_took_back(&_practice->guide, rde_engine_get_time_now());
    }
}

void fude_practice_score(fude_practice* _practice) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }

    if(fude_practice_guiding(_practice)) {
        return;   // steps 1 and 2 are help, not scored
    }

    const u32 _squares = fude_practice_square_count(_practice);
    f32       _sum     = 0.0f;
    u32       _scored  = 0;
    for(u32 _i = 0; _i < _squares; _i++) {
        _practice->scores[_i] = fude_score_drawing(_practice->db, &_practice->info, &_practice->inks[_i]);
        if(!_practice->scores[_i].empty) {
            _sum += _practice->scores[_i].score;
            _scored++;
        }
    }
    _practice->scored = true;

    if(_scored == 0) {
        snprintf(_practice->status, sizeof(_practice->status), "%s", fude_text(FUDE_TEXT_PRACTICE_NOTHING));
        return;
    }

    const f32 _average = _sum / (f32)_scored;
    if(_practice->set_position < (u32)rde_arr_length(&_practice->set_results)) {
        ((f32*)_practice->set_results.memory)[_practice->set_position] = _average;   // this run's, for the set's summary
    }
    if(!_practice->changed) {
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_ALREADY_SAVED, FUDE_TN(lroundf(_average)));
        return;
    }

    const fude_ink* _drawings[FUDE_PRACTICE_MAX_SQUARES];
    for(u32 _i = 0; _i < _squares; _i++) {
        _drawings[_i] = &_practice->inks[_i];
    }
    const b8 _saved = fude_history_save(_practice->info.codepoint, (u64)time(NULL), _squares, _practice->scores,
                                        _drawings, FUDE_PRACTICE_UNITS);
    if(_saved) {
        _practice->changed     = false;
        _practice->has_summary = fude_history_summarize(_practice->info.codepoint, &_practice->summary);
    }
    if(_practice->guided) {
        snprintf(_practice->feedback, sizeof(_practice->feedback), "%s", _practice->scores[0].feedback);
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_FROM_MEMORY, FUDE_TN(lroundf(_average)), FUDE_TS(fude_text(_saved ? FUDE_TEXT_SAVED : FUDE_TEXT_NOT_SAVED)));
    } else {
        c8 _squares_n[48];
        FUDE_TEXTF(_squares_n, FUDE_TEXT_SQUARES_N, FUDE_TN(_scored));
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_AVERAGE, FUDE_TN(lroundf(_average)), FUDE_TS(_squares_n),
                   FUDE_TS(fude_text(_saved ? FUDE_TEXT_SAVED : FUDE_TEXT_NOT_SAVED)));
    }
}

// --- the pen -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F fude_practice_local(const fude_practice* _practice, u32 _square, rde_vec_2F _screen) {
    // From the square's bottom-left corner, in its own units, kept inside it.
    const f32 _k = FUDE_PRACTICE_UNITS / _practice->square_size;
    const rde_vec_2F _bl = { _practice->square_tl[_square].x, _practice->square_tl[_square].y - _practice->square_size };
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _bl.x) * _k, 0.0f, FUDE_PRACTICE_UNITS),
        rde_math_clamp_f32((_screen.y - _bl.y) * _k, 0.0f, FUDE_PRACTICE_UNITS)
    };
}

void fude_practice_pen_down(fude_practice* _practice, rde_vec_2F _screen) {
    _practice->writing = -1;
    if(_practice->square_size <= 0.0f) {
        return;
    }

    for(u32 _i = 0; _i < fude_practice_square_count(_practice); _i++) {
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

    if(_practice->guided) {
        const f64 _now = rde_engine_get_time_now();
        if(fude_guide_skip_pause(&_practice->guide, _now)) {
            fude_practice_clear_squares(_practice);   // a step was done: the pen starts the next
            _practice->writing = 0;
        } else if(_practice->guide.stage == FUDE_GUIDE_RECALL && _practice->scored) {
            fude_practice_clear_squares(_practice);   // a new attempt from memory
            _practice->status[0] = 0;
            _practice->writing   = 0;
        }
    }

    fude_ink* _ink = &_practice->inks[_practice->writing];
    _ink->zoom = _practice->square_size / FUDE_PRACTICE_UNITS;   // the min step stays a screen unit
    fude_ink_begin(_ink, fude_practice_local(_practice, (u32)_practice->writing, _screen), true, false);

    u8 _square = (u8)_practice->writing;
    rde_arr_add(&_practice->strokes_in, &_square);
    _practice->scores[_practice->writing].empty = true;   // written in again: its old mark is gone
    _practice->changed = true;
}

void fude_practice_pen_moved(fude_practice* _practice, rde_vec_2F _screen) {
    if(_practice->writing >= 0) {
        fude_ink_extend(&_practice->inks[_practice->writing], fude_practice_local(_practice, (u32)_practice->writing, _screen));
    }
}

void fude_practice_pen_up(fude_practice* _practice) {
    if(_practice->writing < 0) {
        return;
    }
    fude_ink* _ink = &_practice->inks[_practice->writing];
    fude_ink_end(_ink);
    _practice->writing = -1;

    // Guided: checked as the pen lifts — a wrong stroke goes; the last stroke
    // from memory is scored.
    if(_practice->guided) {
        const FUDE_GUIDE_RESULT_ _result = fude_guide_stroke(&_practice->guide, _ink, FUDE_PRACTICE_UNITS, rde_engine_get_time_now());
        if(_result == FUDE_GUIDE_TAKEN_BACK && rde_arr_length(&_practice->strokes_in) > 0) {
            _practice->strokes_in.count--;
        } else if(_result == FUDE_GUIDE_COMPLETE) {
            fude_practice_score(_practice);
        }
    }
}

// --- drawing -----------------------------------------------------------------------

// The end of a set: every character in it and how it went this run.
RDE_INTERNAL void fude_practice_render_summary(fude_practice* _practice, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme   = fude_theme_active();
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
    const u32 _weak = fude_practice_weak_count(_practice);

    c8 _line[160];
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_SET_DONE), _left, _top - 32.0f, 24.0f, _theme->text);
    if(_done > 0) {
        FUDE_TEXTF(_line, FUDE_TEXT_SET_SUMMARY, FUDE_TN(_done), FUDE_TN(_count), FUDE_TN(lroundf(_sum / (f32)_done)));
    } else {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SET_NOTHING));
    }
    fude_draw_text(_font, _font_px, _line, _left, _top - 64.0f, 19.0f, _theme->text);
    if(_weak > 0) {
        FUDE_TEXTF(_line, FUDE_TEXT_SET_WEAK, FUDE_TN(_weak), FUDE_TN(lroundf(FUDE_THEME_GRADE_GOOD)));
    } else if(_done > 0) {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SET_ALL_GOOD));
    } else {
        _line[0] = 0;
    }
    fude_draw_text(_font, _font_px, _line, _left, _top - 92.0f, 17.0f, _theme->text_soft);

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
        fude_kanji_info _info;
        if(!fude_kanji_at(_practice->db, _records[_i], &_info)) {
            continue;
        }
        fude_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, _theme->line);
        fude_glyph_character(&_practice->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.06f }, _glyph,
                             _results[_i] >= 0.0f ? _theme->ink : _theme->ghost);
        if(_results[_i] >= 0.0f) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_results[_i]);
            fude_draw_text(_font, _font_px, _line, _x + 6.0f, _y - _cell + 9.0f, 16.0f, fude_theme_grade(_results[_i]));
        }
    }
}

void fude_practice_render(fude_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_practice->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_PRACTICE_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_PRACTICE_MARGIN;
    const f32        _width  = _right - _left;
    const fude_kanji_info* _info = &_practice->info;

    if(_practice->summary_open) {
        _practice->square_size = 0.0f;   // no squares to write in
        fude_practice_render_summary(_practice, _font, _font_px, _left, _right, _top, _bottom);
        return;
    }

    // --- the demo: the character writing itself, over and over --------------------
    const f32        _demo    = fminf(FUDE_PRACTICE_DEMO_MAX, _width * 0.3f);
    const rde_vec_2F _demo_tl = { _left, _top - 8.0f };
    fude_glyph_box(_demo_tl, _demo);
    const f64 _now = rde_engine_get_time_now();
    if(fude_glyph_writing(&_practice->glyph, _info, _demo_tl, _demo, _now - _practice->demo_start, _font, _font_px, 13.0f)) {
        // Finished: rest a moment on the whole character, then write it again.
        if(_practice->demo_done <= 0.0) {
            _practice->demo_done = _now;
        } else if(_now - _practice->demo_done > FUDE_PRACTICE_DEMO_REST) {
            _practice->demo_start = _now;
            _practice->demo_done  = 0.0;
        }
    }

    // --- beside it: what it is, how it has gone before, how this one went ---------------
    const f32 _tx = _left + _demo + 20.0f;
    f32       _ty = _top - 30.0f;
    c8        _line[160];
    c8 _where[64];
    snprintf(_where, sizeof(_where), "%s", fude_text(_practice->guided ? FUDE_TEXT_GUIDED : FUDE_TEXT_PRACTICE));
    if(fude_practice_in_set(_practice)) {
        FUDE_TEXTF(_where, FUDE_TEXT_OF_N, FUDE_TN(_practice->set_position + 1u), FUDE_TN(rde_arr_length(&_practice->set)));
    }
    c8 _strokes[48];
    FUDE_TEXTF(_strokes, FUDE_TEXT_STROKES_N, FUDE_TN(_info->strokes));
    if(_info->level != 0) {
        c8 _level[32];
        fude_lang_level_name(_info->level, true, _level, sizeof(_level));
        FUDE_TEXTF(_line, FUDE_TEXT_PRACTICE_HEADER_LEVEL, FUDE_TS(_where), FUDE_TS(_strokes), FUDE_TS(_level));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_PRACTICE_HEADER, FUDE_TS(_where), FUDE_TS(_strokes));
    }
    fude_draw_text(_font, _font_px, _line, _tx, _ty, 22.0f, fude_theme_active()->text);

    _ty -= 38.0f;
    const c8* _on  = fude_kanji_reading(_practice->db, _info, 0u);
    const c8* _kun = fude_kanji_reading(_practice->db, _info, 1u);
    if(_on[0] != 0 || _kun[0] != 0) {
        const f32 _x = fude_glyph_reading(&_practice->glyph, _on, (rde_vec_2F){ _tx, _ty + 22.0f }, 24.0f, _right, fude_theme_active()->ink, fude_theme_active()->text_soft);
        fude_glyph_reading(&_practice->glyph, _kun, (rde_vec_2F){ _x + 18.0f, _ty + 22.0f }, 24.0f, _right, fude_theme_active()->ink, fude_theme_active()->text_soft);
        _ty -= 34.0f;
    }
    const c8* _meanings = fude_kanji_meanings(_practice->db, _info);
    if(_meanings[0] != 0) {
        fude_draw_text_whole(_font, _font_px, _meanings, _tx, _ty + 17.0f * 0.38f, 17.0f, _right - _tx, 1u, fude_theme_active()->text_soft);
        _ty -= 30.0f;
    }

    if(_practice->has_summary) {
        c8 _sessions_n[48];
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_practice->summary.sessions));
        FUDE_TEXTF(_line, FUDE_TEXT_PRACTICE_HISTORY, FUDE_TS(_sessions_n), FUDE_TN(lroundf(_practice->summary.best)), FUDE_TN(lroundf(_practice->summary.last)),
                   FUDE_TN(lroundf(_practice->summary.first)));
    } else {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_NOT_PRACTISED));
    }
    fude_draw_text(_font, _font_px, _line, _tx, _ty, 17.0f, fude_theme_active()->text);
    _ty -= 30.0f;

    if(_practice->status[0] != 0) {
        fude_draw_text(_font, _font_px, _practice->status, _tx, _ty, 19.0f, fude_theme_active()->text);
    }

    // --- guided: one big square, the help in it, the step under it ---------------------
    if(_practice->guided) {
        const f32 _grid_top = _demo_tl.y - _demo - 24.0f;
        f32       _square   = fminf(_width, _grid_top - _bottom - FUDE_PRACTICE_GUIDED_TEXT);
        _square             = fmaxf(120.0f, fminf(_square, FUDE_PRACTICE_GUIDED_MAX));
        const rde_vec_2F _tl = { (_left + _right) * 0.5f - _square * 0.5f, _grid_top };
        _practice->square_tl[0] = _tl;
        _practice->square_size  = _square;

        const f64         _now   = rde_engine_get_time_now();
        const fude_theme* _theme = fude_theme_active();
        const b8          _shown = _practice->scored && !_practice->scores[0].empty;   // a step 3 attempt, scored
        fude_glyph_box(_tl, _square);
        fude_guide_render(&_practice->guide, &_practice->glyph, _tl, _square, _now);
        if(_shown) {
            fude_glyph_character(&_practice->glyph, _info->codepoint, _tl, _square, _theme->reference);   // the model, behind
        }
        fude_ink_render(&_practice->inks[0], (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_PRACTICE_UNITS,
                        (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, _now, false);
        fude_guide_render_over(&_practice->guide, _tl, _square, FUDE_PRACTICE_UNITS, _now);
        if(_shown) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_practice->scores[0].score);
            fude_draw_text(_font, _font_px, _line, _tl.x + _square - 52.0f, _tl.y - 40.0f, 32.0f, fude_theme_grade(_practice->scores[0].score));
        }

        // The step, then what went wrong (or what to do again), wrapped from the
        // square's left to the screen's right margin.
        const f32 _tw = _right - _tl.x;
        fude_guide_prompt(&_practice->guide, _line, sizeof(_line));
        const u32 _step = fude_draw_text_wrap(_font, _font_px, _line, _tl.x, _tl.y - _square - 26.0f, 16.0f,
                                              fude_draw_text_balanced_width(_font, _font_px, _line, 16.0f, _tw), 21.0f, _theme->text);
        const f32 _next = _tl.y - _square - 52.0f - (f32)(_step > 1u ? _step - 1u : 0u) * 21.0f;
        if(_practice->guide.message[0] != 0) {
            fude_draw_text_wrap(_font, _font_px, _practice->guide.message, _tl.x, _next, 16.0f, _tw, 21.0f, _theme->score_poor);
        } else if(_shown) {
            FUDE_TEXTF(_line, FUDE_TEXT_WRITE_AGAIN, FUDE_TS(_practice->feedback));
            fude_draw_text_wrap(_font, _font_px, _line, _tl.x, _next, 16.0f, _tw, 21.0f, _theme->text_soft);
        }
        return;
    }

    // --- the squares ------------------------------------------------------------------
    const f32 _grid_top = _demo_tl.y - _demo - 24.0f;
    const f32 _room_h   = _grid_top - _bottom;
    u32       _cols     = (u32)fmaxf(1.0f, floorf((_width + FUDE_PRACTICE_GAP) / (FUDE_PRACTICE_SQUARE_MAX * 0.8f + FUDE_PRACTICE_GAP)));
    _cols               = _cols > _practice->squares ? _practice->squares : _cols;
    const u32 _rows     = (_practice->squares + _cols - 1u) / _cols;
    f32       _square   = fminf(FUDE_PRACTICE_SQUARE_MAX, (_width - FUDE_PRACTICE_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    _square             = fminf(_square, (_room_h - (f32)_rows * (FUDE_PRACTICE_LABEL_H + FUDE_PRACTICE_GAP)) / (f32)_rows);
    _square             = fmaxf(_square, 60.0f);
    _practice->square_size = _square;

    const rde_vec_2F _half = { (f32)_size.x * 0.5f, (f32)_size.y * 0.5f };
    for(u32 _i = 0; _i < _practice->squares; _i++) {
        const u32        _col = _i % _cols;
        const u32        _row = _i / _cols;
        const rde_vec_2F _tl  = { _left + (f32)_col * (_square + FUDE_PRACTICE_GAP),
                                  _grid_top - (f32)_row * (_square + FUDE_PRACTICE_LABEL_H + FUDE_PRACTICE_GAP) };
        _practice->square_tl[_i] = _tl;

        fude_glyph_box(_tl, _square);

        const fude_score* _s = &_practice->scores[_i];
        const b8 _show = _practice->scored && !_s->empty;
        if(_show) {
            // The model, faint, behind what was written.
            fude_glyph_character(&_practice->glyph, _info->codepoint, _tl, _square, fude_theme_active()->reference);
        }

        fude_ink_render(&_practice->inks[_i], (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_PRACTICE_UNITS,
                        _half, rde_engine_get_time_now(), false);

        if(_show) {
            c8 _mark[8];
            snprintf(_mark, sizeof(_mark), "%.0f", (f64)_s->score);
            fude_draw_text(_font, _font_px, _mark, _tl.x + _square - 34.0f, _tl.y - 24.0f, 20.0f, fude_theme_grade(_s->score));
            fude_draw_text(_font, _font_px, _s->feedback, _tl.x + 2.0f, _tl.y - _square - 20.0f, 13.0f, fude_theme_active()->text);
        }
    }
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"

FUDE_SCREEN_RENDER(fude_practice, fude_practice)

RDE_INTERNAL b8   fude_practice_screen_is_open(const void* _self) { return ((const fude_practice*)_self)->open; }
RDE_INTERNAL void fude_practice_screen_close(void* _self) { fude_practice_close((fude_practice*)_self); }
RDE_INTERNAL void fude_practice_screen_update(fude_app* _app, void* _self, f32 _dt) { RDE_UNUSED(_app); RDE_UNUSED(_dt); fude_practice_update((fude_practice*)_self); }   // guided: a finished step moves on
// The pen writes (and the mouse; a finger with the hand on).
RDE_INTERNAL void fude_practice_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); RDE_UNUSED(_now); fude_practice_pen_down((fude_practice*)_self, _at); }
RDE_INTERNAL void fude_practice_screen_moved(void* _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_now); fude_practice_pen_moved((fude_practice*)_self, _at); }
RDE_INTERNAL void fude_practice_screen_up(void* _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_at); RDE_UNUSED(_now); fude_practice_pen_up((fude_practice*)_self); }

FUDE_ROW_CALL(fude_practice_row_close, fude_practice, fude_practice_close)
FUDE_ROW_CALL(fude_practice_row_undo,  fude_practice, fude_practice_undo)
FUDE_ROW_CALL(fude_practice_row_clear, fude_practice, fude_practice_clear)
FUDE_ROW_CALL(fude_practice_row_score, fude_practice, fude_practice_score)
FUDE_ROW_CALL(fude_practice_row_next,  fude_practice, fude_practice_next)
FUDE_ROW_CALL(fude_practice_row_again, fude_practice, fude_practice_weakest_again)

RDE_INTERNAL void fude_practice_row_fewer(fude_app* _app, void* _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_squares(_practice, _practice->squares > 1u ? _practice->squares - 1u : 1u);
}

RDE_INTERNAL void fude_practice_row_more(fude_app* _app, void* _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_squares(_practice, _practice->squares + 1u);
}

RDE_INTERNAL void fude_practice_row_guided(fude_app* _app, void* _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_guided(_practice, !_practice->guided);
}

// One character; a set (Next, Finish on the last); the set's summary.
enum { FUDE_PRACTICE_ROW_ONE = 0, FUDE_PRACTICE_ROW_SET, FUDE_PRACTICE_ROW_SUMMARY };
enum { FUDE_PRACTICE_ONE_BACK = 0, FUDE_PRACTICE_ONE_UNDO, FUDE_PRACTICE_ONE_CLEAR, FUDE_PRACTICE_ONE_SCORE, FUDE_PRACTICE_ONE_FEWER, FUDE_PRACTICE_ONE_MORE, FUDE_PRACTICE_ONE_GUIDED };
enum { FUDE_PRACTICE_SET_BACK = 0, FUDE_PRACTICE_SET_UNDO, FUDE_PRACTICE_SET_CLEAR, FUDE_PRACTICE_SET_SCORE, FUDE_PRACTICE_SET_GUIDED, FUDE_PRACTICE_SET_NEXT };
enum { FUDE_PRACTICE_SUMMARY_AGAIN = 0, FUDE_PRACTICE_SUMMARY_DONE };
static const fude_row_button FUDE_PRACTICE_ONE[] = {
    { FUDE_TEXT_BACK,   FUDE_ICON_BACK,   fude_practice_row_close,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_UNDO,   FUDE_ICON_UNDO,   fude_practice_row_undo,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_CLEAR,  FUDE_ICON_TRASH,  fude_practice_row_clear,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCORE,  FUDE_ICON_TARGET, fude_practice_row_score,  0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_FEWER,  FUDE_ICON_MINUS,  fude_practice_row_fewer,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_MORE,   FUDE_ICON_PLUS,   fude_practice_row_more,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_GUIDED, FUDE_ICON_GUIDED, fude_practice_row_guided, 0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_button FUDE_PRACTICE_SET[] = {
    { FUDE_TEXT_BACK,   FUDE_ICON_BACK,        fude_practice_row_close,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_UNDO,   FUDE_ICON_UNDO,        fude_practice_row_undo,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_CLEAR,  FUDE_ICON_TRASH,       fude_practice_row_clear,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCORE,  FUDE_ICON_TARGET,      fude_practice_row_score,  0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_GUIDED, FUDE_ICON_GUIDED,      fude_practice_row_guided, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NEXT,   FUDE_ICON_ARROW_RIGHT, fude_practice_row_next,   0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_button FUDE_PRACTICE_SUMMARY[] = {
    { FUDE_TEXT_WEAKEST_AGAIN, FUDE_ICON_RETRY, fude_practice_row_again, 0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_DONE,          FUDE_ICON_CHECK, fude_practice_row_close, 0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_PRACTICE_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_PRACTICE_ONE), FUDE_ROW_DEF(FUDE_PRACTICE_SET), FUDE_ROW_DEF(FUDE_PRACTICE_SUMMARY) };

RDE_INTERNAL u32 fude_practice_screen_row(const void* _self) {
    const fude_practice* _practice = (const fude_practice*)_self;
    return _practice->summary_open ? FUDE_PRACTICE_ROW_SUMMARY : fude_practice_in_set(_practice) ? FUDE_PRACTICE_ROW_SET : FUDE_PRACTICE_ROW_ONE;
}

// Guided shows it on (the squares' count means nothing then); Score only where
// there is something to score (not guided steps 1 and 2, which are help); Next
// is Finish on a set's last character; Weakest again needs something weak.
RDE_INTERNAL void fude_practice_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    const fude_practice* _practice = (const fude_practice*)_self;
    const b8             _guiding  = fude_practice_guiding(_practice);
    if(_row == FUDE_PRACTICE_ROW_ONE) {
        _faces[FUDE_PRACTICE_ONE_GUIDED].selected = _practice->guided;
        _faces[FUDE_PRACTICE_ONE_FEWER].disabled  = _practice->guided;
        _faces[FUDE_PRACTICE_ONE_MORE].disabled   = _practice->guided;
        _faces[FUDE_PRACTICE_ONE_SCORE].disabled  = _guiding;
    } else if(_row == FUDE_PRACTICE_ROW_SET) {
        _faces[FUDE_PRACTICE_SET_GUIDED].selected = _practice->guided;
        _faces[FUDE_PRACTICE_SET_SCORE].disabled  = _guiding;
        fude_row_face_next(&_faces[FUDE_PRACTICE_SET_NEXT], fude_practice_at_last(_practice));
    } else {
        _faces[FUDE_PRACTICE_SUMMARY_AGAIN].disabled = fude_practice_weak_count(_practice) == 0u;
    }
}

const fude_screen FUDE_PRACTICE_SCREEN = {
    .name = "practice", .input = FUDE_SCREEN_INPUT_WRITE,
    .is_open = fude_practice_screen_is_open, .close = fude_practice_screen_close,
    .update = fude_practice_screen_update, .render = fude_practice_screen_render,
    .pointer_down = fude_practice_screen_down, .pointer_moved = fude_practice_screen_moved, .pointer_up = fude_practice_screen_up,
    .rows = FUDE_PRACTICE_BUTTON_ROWS, .row_count = 3u, .row = fude_practice_screen_row, .faces = fude_practice_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
