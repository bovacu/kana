// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/practice.h"
#include "lang/lang.h"
#include "drawing/base/clock.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "drawing/base/theme.h"


// ===========================================================================
// See practice.h.
//
// Every square's ink is made once, in fude_practice_init, and emptied again rather than freed. strokes_in records,
// for each stroke in the order written, the square it went in, so Undo can take back the last stroke whichever square
// it is in. In a set, set_results keeps each character's average from this run, for the summary.
// ===========================================================================

// The layout's sizes, in screen points (MARGIN from the screen's sides; the squares and the demo at their largest).
#define FUDE_PRACTICE_MARGIN     16.0f
#define FUDE_PRACTICE_GAP        14.0f     // The space between squares.
#define FUDE_PRACTICE_LABEL_H    40.0f     // The room under a square for its score and feedback.
#define FUDE_PRACTICE_SQUARE_MAX 190.0f
#define FUDE_PRACTICE_DEMO_MAX   190.0f
#define FUDE_PRACTICE_DEMO_REST  1.5       // The seconds the finished demo rests before it writes again.
#define FUDE_PRACTICE_GUIDED_MAX 620.0f    // Guided: the one square's largest size.
#define FUDE_PRACTICE_GUIDED_TEXT 86.0f    // Guided: the room under it for the step (two lines) and the result.

// The width of the pen in a square's units: the reference's own stroke width.
#define FUDE_PRACTICE_PEN_RADIUS (FUDE_PRACTICE_UNITS * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f)

// Makes an empty square ink, drawn at the practice pen's fixed width.
RDE_INTERNAL void fude_practice_fresh_ink(fude_ink* _ink) {
    fude_ink_init(_ink);
    _ink->constant_radius = FUDE_PRACTICE_PEN_RADIUS;
}

// Sets Practice up closed, with FUDE_PRACTICE_DEFAULT_SQUARES squares and an empty ink for each possible square.
void fude_practice_init(fude_practice* _practice, const fude_kanji_db* _db) {
    rde_memset(_practice, 0, sizeof(*_practice));
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

// Frees every square's ink, the arrays and the glyph cache, and zeroes Practice.
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
    rde_memset(_practice, 0, sizeof(*_practice));
}

// Returns how many squares are in use: when guided only the first, else the count shown.
RDE_INTERNAL u32 fude_practice_square_count(const fude_practice* _practice) {
    return _practice->guided ? 1u : _practice->squares;
}

// Empties every square and forgets its score and the guided feedback; the status line stays.
RDE_INTERNAL void fude_practice_clear_squares(fude_practice* _practice) {
    for(u32 _i = 0; _i < FUDE_PRACTICE_MAX_SQUARES; _i++) {
        fude_ink_destroy(&_practice->inks[_i]);
        fude_practice_fresh_ink(&_practice->inks[_i]);
    }
    rde_arr_clear(&_practice->strokes_in);
    rde_memset(_practice->scores, 0, sizeof(_practice->scores));
    _practice->scored      = false;
    _practice->changed     = false;
    _practice->writing     = -1;
    _practice->feedback[0] = 0;
}

// Empties every square and the status line; when guided, the step starts again.
void fude_practice_clear(fude_practice* _practice) {
    fude_practice_clear_squares(_practice);
    _practice->status[0] = 0;
    if(_practice->guided) {
        fude_guide_restart(&_practice->guide, fude_clock_now());   // The same step, from its start.
    }
}

// Returns whether it is guided and on a step that is not scored (any step before writing from memory).
b8 fude_practice_guiding(const fude_practice* _practice) {
    return _practice->guided && _practice->guide.stage != FUDE_GUIDE_RECALL;
}

// Turns guided mode on or off, ending a stroke still being drawn; the squares are cleared, and guided starts at step 1.
void fude_practice_set_guided(fude_practice* _practice, b8 _guided) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }
    _practice->guided = _guided;
    fude_practice_clear(_practice);
    if(_guided) {
        fude_guide_start(&_practice->guide, _practice->db, &_practice->info, fude_clock_now());
    }
}

// When guided and the pen is up, lets the guide move on; a finished step goes on to the next on a clean square.
void fude_practice_update(fude_practice* _practice) {
    if(_practice->open && _practice->guided && _practice->writing < 0 && fude_guide_update(&_practice->guide, fude_clock_now())) {
        fude_practice_clear_squares(_practice);   // The next step starts on a clean square.
    }
}

// Loads character _record: the squares cleared, the demo started again, its history's summary read again, and when
// guided, the guide started on it. Returns false when there is no character data or no such record.
RDE_INTERNAL b8 fude_practice_load(fude_practice* _practice, u32 _record) {
    if(_practice->db == NULL || !fude_kanji_at(_practice->db, _record, &_practice->info)) {
        return false;
    }
    _practice->record      = _record;
    _practice->demo_start  = fude_clock_now();
    _practice->demo_done   = 0.0;
    _practice->has_summary = fude_history_summarize(_practice->info.codepoint, &_practice->summary);
    fude_practice_clear(_practice);
    if(_practice->guided) {
        fude_guide_start(&_practice->guide, _practice->db, &_practice->info, _practice->demo_start);
    }
    return true;
}

// Opens on a copy of _records (cut to FUDE_PRACTICE_SET_MAX), none scored yet, on the first. Practice opens only when
// the first character can be loaded.
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

// Opens on one character, as a set of one.
void fude_practice_open(fude_practice* _practice, u32 _record) {
    fude_practice_open_set(_practice, &_record, 1);
}

// Returns whether the set has more than one character.
b8 fude_practice_in_set(const fude_practice* _practice) {
    return rde_arr_length(&_practice->set) > 1;
}

// Returns whether it is on the set's last character.
b8 fude_practice_at_last(const fude_practice* _practice) {
    return _practice->set_position + 1u >= (u32)rde_arr_length(&_practice->set);
}

// Scores what was written and not yet scored (which saves it), then loads the next character of the set that can be
// loaded; after the last, it shows the set's summary.
void fude_practice_next(fude_practice* _practice) {
    if(!_practice->open || _practice->summary_open) {
        return;
    }
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }

    // What was written and not scored is scored now, which saves it, so moving on loses nothing. When guided, only an
    // attempt at step 3 counts, since the other steps are help.
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
    _practice->summary_open = true;   // That was the last character.
}

// Returns how many of the set's characters were scored this run under FUDE_THEME_GRADE_GOOD.
u32 fude_practice_weak_count(const fude_practice* _practice) {
    const f32* _results = (const f32*)_practice->set_results.memory;
    u32        _n       = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->set_results); _i++) {
        _n += (_results[_i] >= 0.0f && _results[_i] < FUDE_THEME_GRADE_GOOD) ? 1u : 0u;
    }
    return _n;
}

// A set's character with its average this run, for sorting the weakest first.
typedef struct {
    u32 record;
    f32 result;
} fude_practice_ranked;

// rde_sort's order for fude_practice_ranked: the lowest average first.
RDE_INTERNAL int fude_practice_by_result(const any _a, const any _b) {
    const f32 _x = ((const fude_practice_ranked*)_a)->result;
    const f32 _y = ((const fude_practice_ranked*)_b)->result;
    return _x < _y ? -1 : _x > _y ? 1 : 0;
}

// Opens a new set of the characters scored under good this run, the weakest first. With none, nothing happens.
void fude_practice_weakest_again(fude_practice* _practice) {
    // The ones under good, the weakest first.
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
    rde_sort(_ranked, _n, sizeof(_ranked[0]), fude_practice_by_result);

    u32 _weak[FUDE_PRACTICE_SET_MAX];
    for(u32 _i = 0; _i < _n; _i++) {
        _weak[_i] = _ranked[_i].record;
    }
    fude_practice_open_set(_practice, _weak, _n);
}

// Closes Practice, ending a stroke still being drawn.
void fude_practice_close(fude_practice* _practice) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }
    _practice->open         = false;
    _practice->summary_open = false;
}

// Shows _count squares, kept between 1 and FUDE_PRACTICE_MAX_SQUARES.
void fude_practice_set_squares(fude_practice* _practice, u32 _count) {
    _count = _count < 1u ? 1u : (_count > FUDE_PRACTICE_MAX_SQUARES ? FUDE_PRACTICE_MAX_SQUARES : _count);

    // Squares that go away take their drawings and their scores with them, so a square that comes back is empty (and
    // shows no old score).
    for(u32 _i = _count; _i < _practice->squares; _i++) {
        fude_ink_destroy(&_practice->inks[_i]);
        fude_practice_fresh_ink(&_practice->inks[_i]);
        rde_memset(&_practice->scores[_i], 0, sizeof(_practice->scores[_i]));
        _practice->scores[_i].empty = true;
    }
    // Their strokes also leave the undo order.
    u8* _order = (u8*)_practice->strokes_in.memory;
    u32 _kept  = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_practice->strokes_in); _i++) {
        if(_order[_i] < _count) {
            _order[_kept++] = _order[_i];
        }
    }
    rde_arr_resize(&_practice->strokes_in, _kept);

    _practice->squares = _count;
}

// Takes back the last stroke, from whichever square it went in, unless the pen is down. That square's score no longer
// shows, and the guide is told when guided.
void fude_practice_undo(fude_practice* _practice) {
    const u32 _n = (u32)rde_arr_length(&_practice->strokes_in);
    if(_n == 0 || _practice->writing >= 0) {
        return;
    }

    const u8 _square = ((const u8*)_practice->strokes_in.memory)[_n - 1u];
    rde_arr_resize(&_practice->strokes_in, _n - 1u);
    fude_ink_undo(&_practice->inks[_square]);
    _practice->scores[_square].empty = true;   // Its score no longer matches its drawing.
    _practice->changed = true;
    if(_practice->guided) {
        fude_guide_took_back(&_practice->guide, fude_clock_now());
    }
}

// Scores every square in use, records the average for the set's summary, and saves the session to the history when
// something was written since the last save. The status line says the average and whether it was saved.
void fude_practice_score(fude_practice* _practice) {
    if(_practice->writing >= 0) {
        fude_practice_pen_up(_practice);
    }

    if(fude_practice_guiding(_practice)) {
        return;   // Steps 1 and 2 are help, and are not scored.
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
        rde_snprintf(_practice->status, sizeof(_practice->status), "%s", fude_text(FUDE_TEXT_PRACTICE_NOTHING));
        return;
    }

    const f32 _average = _sum / (f32)_scored;
    if(_practice->set_position < (u32)rde_arr_length(&_practice->set_results)) {
        ((f32*)_practice->set_results.memory)[_practice->set_position] = _average;   // For the set's summary.
    }
    if(!_practice->changed) {
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_ALREADY_SAVED, FUDE_TN(rde_math_round_to_i64_f32(_average)));
        return;
    }

    const fude_ink* _drawings[FUDE_PRACTICE_MAX_SQUARES];
    for(u32 _i = 0; _i < _squares; _i++) {
        _drawings[_i] = &_practice->inks[_i];
    }
    const b8 _saved = fude_history_save(_practice->info.codepoint, (u64)rde_date_now_unix_seconds(), _squares, _practice->scores,
                                        _drawings, FUDE_PRACTICE_UNITS);
    if(_saved) {
        _practice->changed     = false;
        _practice->has_summary = fude_history_summarize(_practice->info.codepoint, &_practice->summary);
    }
    if(_practice->guided) {
        rde_snprintf(_practice->feedback, sizeof(_practice->feedback), "%s", _practice->scores[0].feedback);
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_FROM_MEMORY, FUDE_TN(rde_math_round_to_i64_f32(_average)), FUDE_TS(fude_text(_saved ? FUDE_TEXT_SAVED : FUDE_TEXT_NOT_SAVED)));
    } else {
        c8 _squares_n[512];
        FUDE_TEXTF(_squares_n, FUDE_TEXT_SQUARES_N, FUDE_TN(_scored));
        FUDE_TEXTF(_practice->status, FUDE_TEXT_PRACTICE_AVERAGE, FUDE_TN(rde_math_round_to_i64_f32(_average)), FUDE_TS(_squares_n),
                   FUDE_TS(fude_text(_saved ? FUDE_TEXT_SAVED : FUDE_TEXT_NOT_SAVED)));
    }
}

// --- The pen ---------------------------------------------------------------------------------------------------------

// Turns a screen point into square _square's own units.
RDE_INTERNAL rde_vec_2F fude_practice_local(const fude_practice* _practice, u32 _square, rde_vec_2F _screen) {
    // From the square's bottom-left corner, FUDE_PRACTICE_UNITS across, clamped to the square.
    const f32 _k = FUDE_PRACTICE_UNITS / _practice->square_size;
    const rde_vec_2F _bl = { _practice->square_tl[_square].x, _practice->square_tl[_square].y - _practice->square_size };
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _bl.x) * _k, 0.0f, FUDE_PRACTICE_UNITS),
        rde_math_clamp_f32((_screen.y - _bl.y) * _k, 0.0f, FUDE_PRACTICE_UNITS)
    };
}

// Starts a stroke in the square the pen comes down in; outside every square nothing happens. When guided, the pen also
// skips a pause between steps, or starts a new attempt from memory after one was scored.
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
        const f64 _now = fude_clock_now();
        if(fude_guide_skip_pause(&_practice->guide, _now)) {
            fude_practice_clear_squares(_practice);   // A step was done, and the pen starts the next one.
            _practice->writing = 0;
        } else if(_practice->guide.stage == FUDE_GUIDE_RECALL && _practice->scored) {
            fude_practice_clear_squares(_practice);   // A new attempt from memory.
            _practice->status[0] = 0;
            _practice->writing   = 0;
        }
    }

    fude_ink* _ink = &_practice->inks[_practice->writing];
    _ink->zoom = _practice->square_size / FUDE_PRACTICE_UNITS;   // The ink's smallest step stays one screen unit.
    fude_ink_begin(_ink, fude_practice_local(_practice, (u32)_practice->writing, _screen), true);

    u8 _square = (u8)_practice->writing;
    rde_arr_add(&_practice->strokes_in, &_square);
    _practice->scores[_practice->writing].empty = true;   // Written in again, so its old score goes.
    _practice->changed = true;
}

// Extends the stroke being written.
void fude_practice_pen_moved(fude_practice* _practice, rde_vec_2F _screen) {
    if(_practice->writing >= 0) {
        fude_ink_extend(&_practice->inks[_practice->writing], fude_practice_local(_practice, (u32)_practice->writing, _screen));
    }
}

// Ends the stroke being written.
void fude_practice_pen_up(fude_practice* _practice) {
    if(_practice->writing < 0) {
        return;
    }
    fude_ink* _ink = &_practice->inks[_practice->writing];
    fude_ink_end(_ink);
    _practice->writing = -1;

    // When guided, the stroke is checked as the pen lifts: a wrong stroke is taken back (so it also leaves the undo
    // order), and the last stroke written from memory scores the attempt.
    if(_practice->guided) {
        const FUDE_GUIDE_RESULT_ _result = fude_guide_stroke(&_practice->guide, _ink, FUDE_PRACTICE_UNITS, fude_clock_now());
        if(_result == FUDE_GUIDE_TAKEN_BACK && rde_arr_length(&_practice->strokes_in) > 0) {
            rde_arr_remove(&_practice->strokes_in, rde_arr_length(&_practice->strokes_in) - 1u);
        } else if(_result == FUDE_GUIDE_COMPLETE) {
            fude_practice_score(_practice);
        }
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Draws the set's summary: how many were scored and their mean, how many are under good, and every character of the
// set in a grid whose cells shrink (from 96 down to 48 points) until it fits, each with its average this run.
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

    c8 _line[512];
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_SET_DONE), _left, _top - 32.0f, 24.0f, _theme->text);
    if(_done > 0) {
        FUDE_TEXTF(_line, FUDE_TEXT_SET_SUMMARY, FUDE_TN(_done), FUDE_TN(_count), FUDE_TN(rde_math_round_to_i64_f32(_sum / (f32)_done)));
    } else {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SET_NOTHING));
    }
    fude_draw_text(_font, _font_px, _line, _left, _top - 64.0f, 19.0f, _theme->text);
    if(_weak > 0) {
        FUDE_TEXTF(_line, FUDE_TEXT_SET_WEAK, FUDE_TN(_weak), FUDE_TN(rde_math_round_to_i64_f32(FUDE_THEME_GRADE_GOOD)));
    } else if(_done > 0) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SET_ALL_GOOD));
    } else {
        _line[0] = 0;
    }
    fude_draw_text(_font, _font_px, _line, _left, _top - 92.0f, 17.0f, _theme->text_soft);

    // The set, in order, as big as fits in the room left.
    const f32 _grid_top = _top - 112.0f;
    const f32 _width    = _right - _left;
    const f32 _height   = _grid_top - _bottom;
    f32       _cell     = 96.0f;
    u32       _cols     = 1;
    while(_cell > 48.0f) {
        _cols = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32(_width / _cell));
        if((f32)((_count + _cols - 1u) / _cols) * _cell <= _height) {
            break;
        }
        _cell -= 4.0f;
    }
    _cols = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32(_width / _cell));

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
            rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_results[_i]);
            fude_draw_text(_font, _font_px, _line, _x + 6.0f, _y - _cell + 9.0f, 16.0f, fude_theme_grade(_results[_i]));
        }
    }
}

// Draws, beside the demo, what the character is (Practice or Guided, its place in the set, its strokes and level), its
// readings, its meanings, its history's summary and the status line. _left and _right are the page's edges, _top its
// top, and _demo the demo's size.
RDE_INTERNAL void fude_practice_render_about(fude_practice* _practice, const fude_kanji_info* _info, rde_font* _font, f32 _font_px, f32 _left, f32 _right,
                                             f32 _top, f32 _demo) {
    const f32 _tx = _left + _demo + 20.0f;
    f32       _ty = _top - 30.0f;
    c8        _line[512];
    c8 _where[512];
    rde_snprintf(_where, sizeof(_where), "%s", fude_text(_practice->guided ? FUDE_TEXT_GUIDED : FUDE_TEXT_PRACTICE));
    if(fude_practice_in_set(_practice)) {
        FUDE_TEXTF(_where, FUDE_TEXT_OF_N, FUDE_TN(_practice->set_position + 1u), FUDE_TN(rde_arr_length(&_practice->set)));
    }
    c8 _strokes[512];
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
    const c8* _first  = fude_kanji_reading(_practice->db, _info, 0u);   // The language's two kinds of reading (lang.h).
    const c8* _second = fude_kanji_reading(_practice->db, _info, 1u);
    if(_first[0] != 0 || _second[0] != 0) {
        const f32 _x = fude_glyph_reading(&_practice->glyph, _first, (rde_vec_2F){ _tx, _ty + 22.0f }, 24.0f, _right, fude_theme_active()->ink, fude_theme_active()->text_soft);
        fude_glyph_reading(&_practice->glyph, _second, (rde_vec_2F){ _x + 18.0f, _ty + 22.0f }, 24.0f, _right, fude_theme_active()->ink, fude_theme_active()->text_soft);
        _ty -= 34.0f;
    }
    const c8* _meanings = fude_kanji_meanings(_practice->db, _info);
    if(_meanings[0] != 0) {
        fude_draw_text_whole(_font, _font_px, _meanings, _tx, _ty + 17.0f * 0.38f, 17.0f, _right - _tx, 1u, fude_theme_active()->text_soft);
        _ty -= 30.0f;
    }

    if(_practice->has_summary) {
        c8 _sessions_n[512];
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_practice->summary.sessions));
        FUDE_TEXTF(_line, FUDE_TEXT_PRACTICE_HISTORY, FUDE_TS(_sessions_n), FUDE_TN(rde_math_round_to_i64_f32(_practice->summary.best)), FUDE_TN(rde_math_round_to_i64_f32(_practice->summary.last)),
                   FUDE_TN(rde_math_round_to_i64_f32(_practice->summary.first)));
    } else {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_NOT_PRACTISED));
    }
    fude_draw_text(_font, _font_px, _line, _tx, _ty, 17.0f, fude_theme_active()->text);
    _ty -= 30.0f;

    if(_practice->status[0] != 0) {
        fude_draw_text(_font, _font_px, _practice->status, _tx, _ty, 19.0f, fude_theme_active()->text);
    }
}

// Draws guided mode's one big square, as big as the room under the demo allows: the guide's help in it, the model
// behind a scored attempt, what was written and its score; and under it the step, then what went wrong or what to do
// again. It records the square's place for the pen. _size is the window's size; _left, _right and _width are the
// page's edges and the width between them, and _bottom its bottom; _demo_tl and _demo are the demo's corner and size.
RDE_INTERNAL void fude_practice_render_guided(fude_practice* _practice, const fude_kanji_info* _info, rde_font* _font, f32 _font_px, rde_vec_2I _size,
                                              f32 _left, f32 _right, f32 _width, f32 _bottom, rde_vec_2F _demo_tl, f32 _demo) {
    c8 _line[512];   // Each text drawn here is written into it first.
    const f32 _grid_top = _demo_tl.y - _demo - 24.0f;
    f32       _square   = rde_math_min_f32(_width, _grid_top - _bottom - FUDE_PRACTICE_GUIDED_TEXT);
    _square             = rde_math_max_f32(120.0f, rde_math_min_f32(_square, FUDE_PRACTICE_GUIDED_MAX));
    const rde_vec_2F _tl = { (_left + _right) * 0.5f - _square * 0.5f, _grid_top };
    _practice->square_tl[0] = _tl;
    _practice->square_size  = _square;

    const f64         _now   = fude_clock_now();
    const fude_theme* _theme = fude_theme_active();
    const b8          _shown = _practice->scored && !_practice->scores[0].empty;   // A scored attempt at step 3.
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _square }, (rde_vec_2F){ _tl.x + _square, _tl.y }, true);   // Handwriting is never mirrored (draw.h).
    fude_glyph_box(_tl, _square);
    fude_guide_render(&_practice->guide, &_practice->glyph, _tl, _square, _now);
    if(_shown) {
        fude_glyph_character(&_practice->glyph, _info->codepoint, _tl, _square, _theme->reference);   // The model, behind.
    }
    fude_ink_render(&_practice->inks[0], (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_PRACTICE_UNITS,
                    (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, _now, false);
    fude_guide_render_over(&_practice->guide, _tl, _square, FUDE_PRACTICE_UNITS, _now);
    if(_shown) {
        rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_practice->scores[0].score);
        fude_draw_text(_font, _font_px, _line, _tl.x + _square - 52.0f, _tl.y - 40.0f, 32.0f, fude_theme_grade(_practice->scores[0].score));
    }
    fude_draw_keep_end();

    // The step, then what went wrong (or what to do again), wrapped from the square's left edge to the screen's
    // right margin.
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
}

// Draws the squares in a grid under the demo, each with the model behind a scored square, what was written, its score
// and its feedback, and records their places for the pen. _size is the window's size; _left and _width are the page's
// left edge and width, and _bottom its bottom; _demo_tl and _demo are the demo's corner and size.
RDE_INTERNAL void fude_practice_render_squares(fude_practice* _practice, const fude_kanji_info* _info, rde_font* _font, f32 _font_px, rde_vec_2I _size,
                                               f32 _left, f32 _width, f32 _bottom, rde_vec_2F _demo_tl, f32 _demo) {
    const f32 _grid_top = _demo_tl.y - _demo - 24.0f;
    const f32 _room_h   = _grid_top - _bottom;
    u32       _cols     = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32((_width + FUDE_PRACTICE_GAP) / (FUDE_PRACTICE_SQUARE_MAX * 0.8f + FUDE_PRACTICE_GAP)));
    _cols               = _cols > _practice->squares ? _practice->squares : _cols;
    const u32 _rows     = (_practice->squares + _cols - 1u) / _cols;
    f32       _square   = rde_math_min_f32(FUDE_PRACTICE_SQUARE_MAX, (_width - FUDE_PRACTICE_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    _square             = rde_math_min_f32(_square, (_room_h - (f32)_rows * (FUDE_PRACTICE_LABEL_H + FUDE_PRACTICE_GAP)) / (f32)_rows);
    _square             = rde_math_max_f32(_square, 60.0f);
    _practice->square_size = _square;

    const rde_vec_2F _half = { (f32)_size.x * 0.5f, (f32)_size.y * 0.5f };
    for(u32 _i = 0; _i < _practice->squares; _i++) {
        const u32        _col = _i % _cols;
        const u32        _row = _i / _cols;
        const rde_vec_2F _tl  = { _left + (f32)_col * (_square + FUDE_PRACTICE_GAP),
                                  _grid_top - (f32)_row * (_square + FUDE_PRACTICE_LABEL_H + FUDE_PRACTICE_GAP) };
        _practice->square_tl[_i] = _tl;

        fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _square }, (rde_vec_2F){ _tl.x + _square, _tl.y }, true);   // Handwriting is never mirrored (draw.h).
        fude_glyph_box(_tl, _square);

        const fude_score* _s = &_practice->scores[_i];
        const b8 _show = _practice->scored && !_s->empty;
        if(_show) {
            // The model, faint, behind what was written.
            fude_glyph_character(&_practice->glyph, _info->codepoint, _tl, _square, fude_theme_active()->reference);
        }

        fude_ink_render(&_practice->inks[_i], (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_PRACTICE_UNITS,
                        _half, fude_clock_now(), false);

        if(_show) {
            c8 _mark[8];
            rde_snprintf(_mark, sizeof(_mark), "%.0f", (f64)_s->score);
            fude_draw_text(_font, _font_px, _mark, _tl.x + _square - 34.0f, _tl.y - 24.0f, 20.0f, fude_theme_grade(_s->score));
        }
        fude_draw_keep_end();
        if(_show) {
            fude_draw_text(_font, _font_px, _s->feedback, _tl.x + 2.0f, _tl.y - _square - 20.0f, 13.0f, fude_theme_active()->text);
        }
    }
}

// Draws the page: the demo and beside it the header, the readings, the meanings, the history's summary and the status
// line; under them the squares, or when guided the one big square with the guide's help and its step. In the summary
// it draws that instead. It records the squares' places for the pen.
void fude_practice_render(fude_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_practice->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // The order is left, top, right, bottom.
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_PRACTICE_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_PRACTICE_MARGIN;
    const f32        _width  = _right - _left;
    const fude_kanji_info* _info = &_practice->info;

    if(_practice->summary_open) {
        _practice->square_size = 0.0f;   // There are no squares to write in.
        fude_practice_render_summary(_practice, _font, _font_px, _left, _right, _top, _bottom);
        return;
    }

    // --- The demo: the character writing itself, over and over ---
    const f32        _demo    = rde_math_min_f32(FUDE_PRACTICE_DEMO_MAX, _width * 0.3f);
    const rde_vec_2F _demo_tl = { _left, _top - 8.0f };
    fude_glyph_box(_demo_tl, _demo);
    const f64 _now = fude_clock_now();
    if(fude_glyph_writing(&_practice->glyph, _info, _demo_tl, _demo, _now - _practice->demo_start, _font, _font_px, 13.0f)) {
        // Once finished, it rests on the whole character for FUDE_PRACTICE_DEMO_REST, then writes it again.
        if(_practice->demo_done <= 0.0) {
            _practice->demo_done = _now;
        } else if(_now - _practice->demo_done > FUDE_PRACTICE_DEMO_REST) {
            _practice->demo_start = _now;
            _practice->demo_done  = 0.0;
        }
    }

    // --- Beside it: what it is, how it went before, and how this session went ---
    fude_practice_render_about(_practice, _info, _font, _font_px, _left, _right, _top, _demo);

    // --- Guided: one big square with the help in it, and the step under it ---
    if(_practice->guided) {
        fude_practice_render_guided(_practice, _info, _font, _font_px, _size, _left, _right, _width, _bottom, _demo_tl, _demo);
        return;
    }

    // --- The squares: as many columns as fit at 80% of their largest size, and as big as the room allows (at least
    // 60 points) ---
    fude_practice_render_squares(_practice, _info, _font, _font_px, _size, _left, _width, _bottom, _demo_tl, _demo);
}

// --- The screen (screen.h): its rows of buttons, and what they do ----------------------------------------------------

#include "study/app/study.h"
#include "drawing/base/icons.h"

// The screen's render function, which calls fude_practice_render (screen.h).
FUDE_SCREEN_RENDER(fude_practice, fude_practice)

// The screen's is_open, close and update functions. Practice's pen functions have their own shape, so these are
// written out rather than made by FUDE_SCREEN_ADAPTERS.
RDE_INTERNAL b8   fude_practice_screen_is_open(const any _self) { return ((const fude_practice*)_self)->open; }
RDE_INTERNAL void fude_practice_screen_close(any _self) { fude_practice_close((fude_practice*)_self); }
RDE_INTERNAL void fude_practice_screen_update(fude_app* _app, any _self, f32 _dt) { RDE_UNUSED(_app); RDE_UNUSED(_dt); fude_practice_update((fude_practice*)_self); }   // Guided: a finished step moves on.
// The pointer: it is a writing screen, so this is the pen (or the mouse, or a finger with the hand on).
RDE_INTERNAL void fude_practice_screen_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); RDE_UNUSED(_now); fude_practice_pen_down((fude_practice*)_self, _at); }
RDE_INTERNAL void fude_practice_screen_moved(any _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_now); fude_practice_pen_moved((fude_practice*)_self, _at); }
RDE_INTERNAL void fude_practice_screen_up(any _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_at); RDE_UNUSED(_now); fude_practice_pen_up((fude_practice*)_self); }

// The buttons that call one of Practice's functions: Back and Done close it; Undo, Clear, Score, Next and Weakest
// again do what they say.
FUDE_ROW_CALL(fude_practice_row_close, fude_practice, fude_practice_close)
FUDE_ROW_CALL(fude_practice_row_undo,  fude_practice, fude_practice_undo)
FUDE_ROW_CALL(fude_practice_row_clear, fude_practice, fude_practice_clear)
FUDE_ROW_CALL(fude_practice_row_score, fude_practice, fude_practice_score)
FUDE_ROW_CALL(fude_practice_row_next,  fude_practice, fude_practice_next)
FUDE_ROW_CALL(fude_practice_row_again, fude_practice, fude_practice_weakest_again)

// Fewer: one square less, never under one.
RDE_INTERNAL void fude_practice_row_fewer(fude_app* _app, any _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_squares(_practice, _practice->squares > 1u ? _practice->squares - 1u : 1u);
}

// More: one square more, up to FUDE_PRACTICE_MAX_SQUARES.
RDE_INTERNAL void fude_practice_row_more(fude_app* _app, any _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_squares(_practice, _practice->squares + 1u);
}

// Guided: turns guided mode on or off.
RDE_INTERNAL void fude_practice_row_guided(fude_app* _app, any _self, u32 _arg) {
    fude_practice* _practice = (fude_practice*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_practice_set_guided(_practice, !_practice->guided);
}

// The rows: one character's; a set's (with Next, which is Finish on the last); and the set's summary's. The enums are
// each row's button indices.
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

// Which row is up: the summary's while it shows, else a set's or one character's.
RDE_INTERNAL u32 fude_practice_screen_row(const any _self) {
    const fude_practice* _practice = (const fude_practice*)_self;
    return _practice->summary_open ? FUDE_PRACTICE_ROW_SUMMARY : fude_practice_in_set(_practice) ? FUDE_PRACTICE_ROW_SET : FUDE_PRACTICE_ROW_ONE;
}

// How the buttons show: Guided is selected when on, and Fewer and More are greyed out then, since the squares' count
// means nothing while guided. Score is greyed out on guided steps 1 and 2, which are help and not scored. Next is
// Finish on a set's last character, and Weakest again is greyed out when nothing was weak.
RDE_INTERNAL void fude_practice_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
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
