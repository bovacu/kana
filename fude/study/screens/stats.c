#include "study/widgets/header.h"
#include "study/screens/stats.h"
#include "drawing/base/text.h"
#include "lang/lang.h"
#include "drawing/widgets/draw.h"
#include "study/screens/exam.h"
#include "study/models/examlog.h"
#include "study/models/history.h"
#include "study/models/marks.h"
#include "drawing/base/theme.h"
#include "study/models/vocab.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See stats.h.
// ===========================================================================

// --- the numbers ---------------------------------------------------------------------

// Seconds to add to UTC for local time, now (portable: no tm_gmtoff on Windows).
// --- the Levels card's rows (stats.h): the language's sets, then its levels ---------

RDE_INTERNAL u32 fude_stats_set_count(void) {
    u32 _n = 0;
    for(u32 _g = 0; _g < fude_lang_group_count(); _g++) {
        _n += (fude_lang_group_flags(_g) & FUDE_LANG_GROUP_SET) != 0u ? 1u : 0u;
    }
    return _n;
}

RDE_INTERNAL u32 fude_stats_set_group(u32 _set) {
    for(u32 _g = 0; _g < fude_lang_group_count(); _g++) {
        if((fude_lang_group_flags(_g) & FUDE_LANG_GROUP_SET) != 0u && _set-- == 0u) {
            return _g;
        }
    }
    return FUDE_LANG_NO_GROUP;
}

u32 fude_stats_group_count(void) {
    return fude_stats_set_count() + fude_lang_level_count();
}

// A character's row: its set's (if it is one of the set's core), else its
// level's; -1 when none.
RDE_INTERNAL i32 fude_stats_group_of(const fude_kanji_info* _info) {
    const u8  _group = fude_lang_group(_info->codepoint);
    const u32 _sets  = fude_stats_set_count();
    if(_group != FUDE_LANG_NO_GROUP && (fude_lang_group_flags(_group) & FUDE_LANG_GROUP_SET) != 0u) {
        for(u32 _s = 0; _s < _sets; _s++) {
            if(fude_stats_set_group(_s) == _group) {
                return fude_lang_core(_info->codepoint) ? (i32)_s : -1;
            }
        }
        return -1;
    }
    for(u32 _l = 0; _info->level != 0u && _l < fude_lang_level_count(); _l++) {
        if(fude_lang_level_value(_l) == _info->level) {
            return (i32)(_sets + _l);
        }
    }
    return -1;
}

RDE_INTERNAL i64 fude_stats_local_offset(void) {
    const time_t _now = time(NULL);
    struct tm    _utc = *gmtime(&_now);
    _utc.tm_isdst     = -1;
    return (i64)difftime(_now, mktime(&_utc));
}

RDE_INTERNAL i64 fude_stats_day(u64 _time, i64 _offset) {
    const i64 _t = (i64)_time + _offset;
    return _t >= 0 ? _t / 86400 : (_t - 86399) / 86400;
}

// Monday 0 .. Sunday 6 (day 0, 1970-01-01, was a Thursday).
RDE_INTERNAL u32 fude_stats_weekday(i64 _day) {
    return (u32)(((_day + 3) % 7 + 7) % 7);
}

RDE_INTERNAL int fude_stats_cmp_i64(const void* _a, const void* _b) {
    const i64 _x = *(const i64*)_a, _y = *(const i64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_stats_cmp_u32(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

typedef struct {
    u32 record;
    f32 value;
} fude_stats_ranked;

RDE_INTERNAL int fude_stats_by_value_up(const void* _a, const void* _b) {
    const f32 _x = ((const fude_stats_ranked*)_a)->value, _y = ((const fude_stats_ranked*)_b)->value;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_stats_by_value_down(const void* _a, const void* _b) {
    return -fude_stats_by_value_up(_a, _b);
}

void fude_stats_compute(fude_stats_data* _d, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    memset(_d, 0, sizeof(*_d));
    _d->average = _d->average_recent = _d->average_before = _d->exam_points = -1.0f;
    for(u32 _w = 0; _w < FUDE_STATS_WEEKS; _w++) {
        _d->week_average[_w] = -1.0f;
    }
    const i64 _off   = fude_stats_local_offset();
    _d->today        = fude_stats_day((u64)time(NULL), _off);
    // The calendar: whole weeks, Monday first, the last one holding today.
    const i64 _start = _d->today - (i64)fude_stats_weekday(_d->today) - 7 * (i64)(FUDE_STATS_WEEKS - 1u);

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _days      = rde_arr_new(sizeof(i64), _heap);   // every active day, with repeats
    rde_arr _practised = rde_arr_new(sizeof(u32), _heap);   // code points with history
    rde_arr _weakest   = rde_arr_new(sizeof(fude_stats_ranked), _heap);
    rde_arr _improved  = rde_arr_new(sizeof(fude_stats_ranked), _heap);
    f64 _sum = 0.0, _sum_recent = 0.0, _sum_before = 0.0;
    u32 _n_before = 0;
    f64 _week_sum[FUDE_STATS_WEEKS] = { 0 };

    // --- practice: every character's history ---
    fude_history_list(&_practised);
    qsort(_practised.memory, rde_arr_length(&_practised), sizeof(u32), fude_stats_cmp_u32);
    fude_history _h;
    fude_history_init(&_h);
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_practised); _c++) {
        const u32 _cp = ((const u32*)_practised.memory)[_c];
        if(!fude_history_load(&_h, _cp) || rde_arr_length(&_h.sessions) == 0) {
            continue;
        }
        _d->characters++;
        const fude_history_session* _sessions = (const fude_history_session*)_h.sessions.memory;
        const fude_history_square*  _squares  = (const fude_history_square*)_h.squares.memory;
        const fude_history_stroke*  _strokes  = (const fude_history_stroke*)_h.strokes.memory;
        const fude_history_point*   _points   = (const fude_history_point*)_h.points.memory;
        const u32                   _ns       = (u32)rde_arr_length(&_h.sessions);
        for(u32 _s = 0; _s < _ns; _s++) {
            const fude_history_session* _session = &_sessions[_s];
            const i64 _day = fude_stats_day(_session->time, _off);
            _d->sessions++;
            rde_arr_add(&_days, &_day);
            if(_d->first_time == 0 || _session->time < _d->first_time) {
                _d->first_time = _session->time;
            }
            const i64 _age = _d->today - _day;
            for(u32 _q = 0; _q < _session->square_count; _q++) {
                const fude_history_square* _sq = &_squares[_session->first_square + _q];
                for(u32 _k = 0; _k < _sq->stroke_count; _k++) {
                    const fude_history_stroke* _st = &_strokes[_sq->first_stroke + _k];
                    if(_st->point_count > 0) {
                        _d->writing_seconds += _points[_st->first_point + _st->point_count - 1u].time;
                    }
                }
                if(_sq->score.empty) {
                    continue;
                }
                const f32 _score = _sq->score.score;
                _d->squares++;
                _sum += _score;
                const i64 _hour = (((i64)_session->time + _off) % 86400 + 86400) % 86400 / 3600;
                _d->hours[_hour]++;
                _d->weekdays[fude_stats_weekday(_day)]++;
                if(_day >= _start && _day < _start + (i64)FUDE_STATS_DAYS) {
                    const u32 _i = (u32)(_day - _start);
                    _d->day_squares[_i] = (u16)(_d->day_squares[_i] < 65535u ? _d->day_squares[_i] + 1u : 65535u);
                    _week_sum[_i / 7u] += _score;
                    _d->week_squares[_i / 7u]++;
                }
                const b8 _recent = _age >= 0 && _age < FUDE_STATS_RECENT;
                if(_recent) {
                    _sum_recent += _score;
                    _d->squares_recent++;
                } else if(_age >= FUDE_STATS_RECENT && _age < 2 * FUDE_STATS_RECENT) {
                    _sum_before += _score;
                    _n_before++;
                }
                const b8 _mistake[FUDE_STATS_MISTAKE_COUNT] = {
                    _sq->score.misplaced > 0, _sq->score.reversed > 0, _sq->score.missing > 0, _sq->score.extra > 0,
                    _sq->score.shape < FUDE_STATS_POOR_SHAPE
                };
                for(u32 _m = 0; _m < FUDE_STATS_MISTAKE_COUNT; _m++) {
                    _d->mistakes[_m]        += _mistake[_m] ? 1u : 0u;
                    _d->mistakes_recent[_m] += _mistake[_m] && _recent ? 1u : 0u;
                }
            }
        }

        // The character: its latest session, and how far it came from its first.
        u32 _record;
        if(fude_kanji_find_index(_db, _cp, &_record)) {
            const fude_stats_ranked _latest = { _record, _sessions[_ns - 1u].average };
            rde_arr_add(&_weakest, &_latest);
            if(_ns >= 2u && _sessions[_ns - 1u].average > _sessions[0].average) {
                const fude_stats_ranked _gain = { _record, _sessions[_ns - 1u].average - _sessions[0].average };
                rde_arr_add(&_improved, &_gain);
            }
        }
    }
    fude_history_destroy(&_h);

    _d->average        = _d->squares > 0 ? (f32)(_sum / (f64)_d->squares) : -1.0f;
    _d->average_recent = _d->squares_recent > 0 ? (f32)(_sum_recent / (f64)_d->squares_recent) : -1.0f;
    _d->average_before = _n_before > 0 ? (f32)(_sum_before / (f64)_n_before) : -1.0f;
    for(u32 _w = 0; _w < FUDE_STATS_WEEKS; _w++) {
        _d->week_average[_w] = _d->week_squares[_w] > 0 ? (f32)(_week_sum[_w] / (f64)_d->week_squares[_w]) : -1.0f;
    }

    // --- exams ---
    const fude_examlog_exam* _exams = fude_examlog_exams();
    const u32                _ne    = fude_examlog_count();
    f64 _points = 0.0;
    for(u32 _e = 0; _e < _ne; _e++) {
        const fude_examlog_exam* _x = &_exams[_e];
        _d->exams++;
        _d->exams_passed += (f32)_x->correct >= FUDE_EXAM_PASS * (f32)_x->item_count ? 1u : 0u;
        _d->exam_items   += _x->item_count;
        _d->exam_right   += _x->correct;
        _points          += (f64)_x->score * (f64)_x->item_count;
        const i64 _day = fude_stats_day(_x->time, _off);
        rde_arr_add(&_days, &_day);
        if(_d->first_time == 0 || _x->time < _d->first_time) {
            _d->first_time = _x->time;
        }
        if(_day >= _start && _day < _start + (i64)FUDE_STATS_DAYS) {
            const u32 _i = (u32)(_day - _start);
            _d->day_exam_items[_i] = (u16)fminf(65535.0f, (f32)_d->day_exam_items[_i] + (f32)_x->item_count);
        }
    }
    _d->exam_points  = _d->exam_items > 0 ? (f32)(_points / (f64)_d->exam_items) : -1.0f;
    _d->recent_exams = _ne < FUDE_STATS_EXAMS ? _ne : FUDE_STATS_EXAMS;
    for(u32 _i = 0; _i < _d->recent_exams; _i++) {
        const fude_examlog_exam* _x = &_exams[_ne - _d->recent_exams + _i];
        _d->recent_accuracy[_i] = _x->item_count > 0 ? (f32)_x->correct / (f32)_x->item_count : 0.0f;
        _d->recent_points[_i]   = _x->score;
        _d->recent_source[_i]   = _x->source;
    }

    // --- days: how many, and the streaks ---
    const u32 _nd = (u32)rde_arr_length(&_days);
    i64*      _dv = (i64*)_days.memory;
    qsort(_dv, _nd, sizeof(i64), fude_stats_cmp_i64);
    u32 _run = 0;
    for(u32 _i = 0; _i < _nd; _i++) {
        if(_i > 0 && _dv[_i] == _dv[_i - 1u]) {
            continue;   // the same day again
        }
        _run = _i > 0 && _dv[_i] == _dv[_i - 1u] + 1 ? _run + 1u : 1u;
        _d->days_active++;
        _d->streak_best = _run > _d->streak_best ? _run : _d->streak_best;
        if(_dv[_i] == _d->today || _dv[_i] == _d->today - 1) {
            _d->streak = _run;   // the run that reaches today (or yesterday: today may still come)
        }
    }

    // --- marks and coverage ---
    _d->studying = fude_marks_count(FUDE_MARK_STUDYING);
    _d->known    = fude_marks_count(FUDE_MARK_KNOWN);

    // Marks over time: the changes replayed, oldest first, the counts taken as
    // each week ends (and FUDE_STATS_RECENT days ago).
    {
        const fude_marks_change* _changes = fude_marks_history();
        const u32                _nc      = fude_marks_history_count();
        const u64                _recent  = (u64)time(NULL) - (u64)FUDE_STATS_RECENT * 86400u;
        rde_arr _state = rde_arr_new(sizeof(fude_marks_change), _heap);   // each character's mark so far, by code point
        u32     _count[FUDE_MARK_COUNT] = { 0 };
        u32     _week = 0;
        b8      _before_taken = false;
        _d->mark_changes = _nc;
        for(u32 _i = 0; _i <= _nc; _i++) {
            // The weeks (and the recent mark) that end before this change.
            const i64 _day = _i < _nc ? fude_stats_day(_changes[_i].time, _off) : INT64_MAX;
            while(_week < FUDE_STATS_WEEKS && _day >= _start + 7 * (i64)(_week + 1u)) {
                _d->week_known[_week]    = _count[FUDE_MARK_KNOWN];
                _d->week_studying[_week] = _count[FUDE_MARK_STUDYING];
                _week++;
            }
            if(!_before_taken && (_i == _nc || _changes[_i].time >= _recent)) {
                _d->known_before = _count[FUDE_MARK_KNOWN];
                _before_taken    = true;
            }
            if(_i == _nc) {
                break;
            }
            // This change: its character's mark before it, then after.
            const fude_marks_change* _c  = &_changes[_i];
            fude_marks_change*       _s  = (fude_marks_change*)_state.memory;
            u32                      _lo = 0, _hi = (u32)rde_arr_length(&_state);
            while(_lo < _hi) {
                const u32 _mid = (_lo + _hi) / 2u;
                if(_s[_mid].codepoint < _c->codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
            }
            if(_lo == (u32)rde_arr_length(&_state) || _s[_lo].codepoint != _c->codepoint) {
                const fude_marks_change _none = { .codepoint = _c->codepoint, .mark = FUDE_MARK_NONE, .time = 0 };
                rde_arr_insert(&_state, _lo, &_none);
                _s = (fude_marks_change*)_state.memory;
            }
            if(_s[_lo].mark < FUDE_MARK_COUNT && _s[_lo].mark != FUDE_MARK_NONE) { _count[_s[_lo].mark]--; }
            _s[_lo].mark = _c->mark;
            if(_c->mark < FUDE_MARK_COUNT && _c->mark != FUDE_MARK_NONE)         { _count[_c->mark]++; }
        }
        rde_arr_free(&_state);
    }
    _d->words_added = fude_vocab_count();
    for(u32 _r = 0; _db != NULL && _catalog != NULL && _r < _db->count; _r++) {
        fude_kanji_info _info;
        if(!fude_catalog_passes(_catalog, _r, FUDE_FILTER_ALL) || !fude_kanji_at(_db, _r, &_info)) {
            continue;
        }
        const i32 _group = fude_stats_group_of(&_info);
        if(_group < 0) {
            continue;
        }
        fude_stats_coverage* _cov = &_d->coverage[_group];
        _cov->total++;
        _cov->practised += bsearch(&_info.codepoint, _practised.memory, rde_arr_length(&_practised), sizeof(u32), fude_stats_cmp_u32) != NULL ? 1u : 0u;
        const FUDE_MARK_ _mark = fude_marks_get(_info.codepoint);
        _cov->studying += _mark == FUDE_MARK_STUDYING ? 1u : 0u;
        _cov->known    += _mark == FUDE_MARK_KNOWN ? 1u : 0u;
    }

    // --- the weakest, the most improved ---
    qsort(_weakest.memory, rde_arr_length(&_weakest), sizeof(fude_stats_ranked), fude_stats_by_value_up);
    qsort(_improved.memory, rde_arr_length(&_improved), sizeof(fude_stats_ranked), fude_stats_by_value_down);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_weakest) && _i < FUDE_STATS_LIST; _i++) {
        _d->weakest[_i]       = ((const fude_stats_ranked*)_weakest.memory)[_i].record;
        _d->weakest_score[_i] = ((const fude_stats_ranked*)_weakest.memory)[_i].value;
        _d->weakest_count++;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_improved) && _i < FUDE_STATS_LIST; _i++) {
        _d->improved[_i]       = ((const fude_stats_ranked*)_improved.memory)[_i].record;
        _d->improved_delta[_i] = ((const fude_stats_ranked*)_improved.memory)[_i].value;
        _d->improved_count++;
    }

    rde_arr* _arrays[] = { &_days, &_practised, &_weakest, &_improved };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        rde_arr_free(_arrays[_i]);
    }
}

// --- the screen ------------------------------------------------------------------------

#define FUDE_STATS_MARGIN    24.0f
#define FUDE_STATS_GAP       14.0f     // between cards
#define FUDE_STATS_PAD       16.0f     // inside a card
#define FUDE_STATS_TITLE     40.0f     // a card's title line
#define FUDE_STATS_TWO_COLS  880.0f    // this wide and more: two columns of cards
#define FUDE_STATS_TILE_W    180.0f    // a number and its label: "characters practised" fits
#define FUDE_STATS_TILE_H    74.0f
#define FUDE_STATS_ROW       34.0f     // a bar's row
#define FUDE_STATS_GLYPH     64.0f     // a character in the weakest / most improved

typedef enum {
    FUDE_STATS_CARD_OVERVIEW = 0,
    FUDE_STATS_CARD_ACTIVITY,
    FUDE_STATS_CARD_SCORES,
    FUDE_STATS_CARD_EXAMS,
    FUDE_STATS_CARD_MARKS,
    FUDE_STATS_CARD_COVERAGE,
    FUDE_STATS_CARD_MISTAKES,
    FUDE_STATS_CARD_CHARACTERS,
    FUDE_STATS_CARD_WHEN,
    FUDE_STATS_CARD_COUNT
} FUDE_STATS_CARD_;

static const FUDE_TEXT_ FUDE_STATS_CARD_TITLES[FUDE_STATS_CARD_COUNT] = {
    FUDE_TEXT_STATS_OVERVIEW, FUDE_TEXT_STATS_ACTIVITY, FUDE_TEXT_STATS_SCORES, FUDE_TEXT_EXAMS, FUDE_TEXT_STATS_MARKS, FUDE_TEXT_STATS_LEVELS, FUDE_TEXT_STATS_MISTAKES,
    FUDE_TEXT_STATS_CHARACTERS, FUDE_TEXT_STATS_WHEN
};
static const FUDE_TEXT_ FUDE_STATS_MISTAKE_NAMES[FUDE_STATS_MISTAKE_COUNT] = { FUDE_TEXT_MISTAKE_ORDER, FUDE_TEXT_MISTAKE_DIRECTION, FUDE_TEXT_MISTAKE_MISSING,
                                                                               FUDE_TEXT_MISTAKE_EXTRA, FUDE_TEXT_MISTAKE_SHAPE };

// A row's name: its set's group name, or its level's short name ("N5").
RDE_INTERNAL const c8* fude_stats_group_name(u32 _row) {
    static c8 _levels[FUDE_LANG_LEVELS][16];
    const u32 _sets = fude_stats_set_count();
    if(_row < _sets) {
        return fude_text((FUDE_TEXT_)fude_lang_group_name(fude_stats_set_group(_row)));
    }
    const u32 _level = _row - _sets;
    fude_lang_level_name(fude_lang_level_value(_level), false, _levels[_level], sizeof(_levels[_level]));
    return _levels[_level];
}

void fude_stats_init(fude_stats* _stats, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    memset(_stats, 0, sizeof(*_stats));
    _stats->db          = _db;
    _stats->catalog     = _catalog;
    _stats->tapped_list = -1;
    _stats->hits        = rde_arr_new(sizeof(fude_stats_hit), rde_memory_allocator_get_default_std());
    fude_glyph_init(&_stats->glyph, _db);
}

void fude_stats_destroy(fude_stats* _stats) {
    if(rde_arr_is_inited(&_stats->hits)) {
        rde_arr_free(&_stats->hits);
    }
    fude_glyph_destroy(&_stats->glyph);
    memset(_stats, 0, sizeof(*_stats));
}

void fude_stats_open(fude_stats* _stats) {
    fude_stats_compute(&_stats->data, _stats->db, _stats->catalog);
    fude_scroller_stop(&_stats->scroller);
    _stats->scroller.offset = 0.0f;
    _stats->tapped_list     = -1;
    _stats->open            = true;
}

void fude_stats_close(fude_stats* _stats) {
    _stats->open = false;
}

void fude_stats_pointer_down(fude_stats* _stats, rde_vec_2F _screen, f64 _time)  { fude_scroller_down(&_stats->scroller, _screen, _time); }
void fude_stats_pointer_moved(fude_stats* _stats, rde_vec_2F _screen, f64 _time) { fude_scroller_moved(&_stats->scroller, _screen, _time); }
void fude_stats_pointer_up(fude_stats* _stats, f64 _time)                         { fude_scroller_up(&_stats->scroller, _time); }

b8 fude_stats_take_tap(fude_stats* _stats, const u32** _records, u32* _count, u32* _position) {
    if(_stats->tapped_list < 0) {
        return false;
    }
    *_records  = _stats->tapped_list == 0 ? _stats->data.weakest : _stats->data.improved;
    *_count    = _stats->tapped_list == 0 ? _stats->data.weakest_count : _stats->data.improved_count;
    *_position = _stats->tapped_index;
    _stats->tapped_list = -1;
    return true;
}

void fude_stats_update(fude_stats* _stats, f32 _dt) {
    if(!_stats->open) {
        return;
    }
    fude_scroller_update(&_stats->scroller, _dt, _stats->content_height, _stats->view_top - _stats->view_bottom);
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_stats->scroller, &_at)) {
        const f32 _y = _stats->view_top - _at.y + _stats->scroller.offset;   // content space
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_stats->hits); _i++) {
            const fude_stats_hit* _hit = &((const fude_stats_hit*)_stats->hits.memory)[_i];
            if(_at.x >= _hit->x && _at.x <= _hit->x + _hit->size && _y >= _hit->y && _y <= _hit->y + _hit->size) {
                _stats->tapped_list  = _hit->list;
                _stats->tapped_index = _hit->index;
            }
        }
    }
}

// A card's message when it has nothing to show, wrapped to its width: the font
// it was last drawn in (the card's height counts its lines before the first
// frame draws it: one).
#define FUDE_STATS_EMPTY_PX   16.0f
#define FUDE_STATS_EMPTY_LINE 21.0f
RDE_INTERNAL rde_font* fude_stats_font    = NULL;
RDE_INTERNAL f32       fude_stats_font_px = 14.0f;

RDE_INTERNAL f32 fude_stats_empty_height(FUDE_TEXT_ _text, f32 _inner) {
    const u32 _lines = fude_stats_font != NULL ? fude_draw_text_wrap_lines(fude_stats_font, fude_stats_font_px, fude_text(_text), FUDE_STATS_EMPTY_PX, _inner) : 1u;
    return 30.0f + (f32)(_lines > 1u ? _lines - 1u : 0u) * FUDE_STATS_EMPTY_LINE;
}

// How tall a card is at _width.
RDE_INTERNAL f32 fude_stats_card_height(const fude_stats* _stats, FUDE_STATS_CARD_ _card, f32 _width) {
    const f32 _inner = _width - 2.0f * FUDE_STATS_PAD;
    const f32 _frame = FUDE_STATS_TITLE + 2.0f * FUDE_STATS_PAD;
    switch(_card) {
        case FUDE_STATS_CARD_OVERVIEW: {
            const u32 _cols = (u32)fmaxf(1.0f, floorf(_inner / FUDE_STATS_TILE_W));
            return _frame + (f32)((13u + _cols - 1u) / _cols) * FUDE_STATS_TILE_H;
        }
        case FUDE_STATS_CARD_ACTIVITY: {
            const f32 _cell = fminf(20.0f, (_inner - 34.0f) / (f32)FUDE_STATS_WEEKS);
            return _frame + 7.0f * _cell + 34.0f;
        }
        case FUDE_STATS_CARD_SCORES:     return _frame + (_stats->data.squares > 0 ? 220.0f : fmaxf(220.0f, fude_stats_empty_height(FUDE_TEXT_STATS_NO_SCORES, _inner)));
        case FUDE_STATS_CARD_EXAMS:      return _frame + (_stats->data.exams > 0 ? 222.0f : fude_stats_empty_height(FUDE_TEXT_STATS_NO_EXAMS, _inner));
        case FUDE_STATS_CARD_MARKS:      return _frame + (_stats->data.mark_changes > 0 ? 242.0f : 52.0f);
        case FUDE_STATS_CARD_COVERAGE:   return _frame + (f32)fude_stats_group_count() * FUDE_STATS_ROW + 28.0f;
        case FUDE_STATS_CARD_MISTAKES:   return _frame + (_stats->data.squares > 0 ? (f32)FUDE_STATS_MISTAKE_COUNT * FUDE_STATS_ROW + 28.0f : fude_stats_empty_height(FUDE_TEXT_STATS_NO_PRACTICE, _inner));
        case FUDE_STATS_CARD_CHARACTERS: return _frame + (_stats->data.weakest_count > 0 ? 2.0f * (26.0f + FUDE_STATS_GLYPH + 26.0f) : fude_stats_empty_height(FUDE_TEXT_STATS_NO_PRACTICE, _inner));
        case FUDE_STATS_CARD_WHEN:       return _frame + (_stats->data.squares > 0 ? 2.0f * 130.0f : fude_stats_empty_height(FUDE_TEXT_STATS_NO_PRACTICE, _inner));
        default:                         return _frame;
    }
}

// Drawing context for a card: its inner box on screen.
typedef struct {
    fude_stats* stats;
    rde_font*   font;
    f32         font_px;
    f32         left;      // inner left, screen
    f32         width;     // inner width
    f32         top;       // inner top (below the title), screen
    f32         content_y; // the inner top in content space (for taps)
} fude_stats_box;

// A card's message when it has nothing to show (fude_stats_empty_height).
RDE_INTERNAL void fude_stats_draw_empty(const fude_stats_box* _b, FUDE_TEXT_ _text) {
    fude_draw_text_wrap(_b->font, _b->font_px, fude_text(_text), _b->left, _b->top - 20.0f, FUDE_STATS_EMPTY_PX, _b->width, FUDE_STATS_EMPTY_LINE, fude_theme_active()->text_soft);
}

RDE_INTERNAL rde_color fude_stats_alpha(rde_color _c, f32 _a) {
    _c.a = (u8)fmaxf(0.0f, fminf(255.0f, (f32)_c.a * _a));
    return _c;
}

// A horizontal bar: _value of _max, from _x, _w long at most.
RDE_INTERNAL void fude_stats_bar(f32 _x, f32 _y, f32 _w, f32 _h, f32 _fraction, rde_color _color) {
    const f32 _len = _w * fmaxf(0.0f, fminf(1.0f, _fraction));
    if(_len > 0.5f) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _len * 0.5f, _y - _h * 0.5f }, (rde_vec_2F){ _len, _h }, _color);
    }
}

RDE_INTERNAL void fude_stats_text_right(const fude_stats_box* _b, const c8* _text, f32 _right, f32 _y, f32 _px, rde_color _color) {
    fude_draw_text(_b->font, _b->font_px, _text, _right - fude_draw_text_width(_b->font, _b->font_px, _text, _px), _y, _px, _color);
}

RDE_INTERNAL void fude_stats_duration(f32 _seconds, c8* _out, usize _size) {
    const u32 _minutes = (u32)(_seconds / 60.0f + 0.5f);
    if(_minutes >= 60u) {
        c8 _mm[8];
        snprintf(_mm, sizeof(_mm), "%02u", _minutes % 60u);
        fude_text_format(_out, _size, FUDE_TEXT_DURATION_HM, (const fude_text_arg[]){ FUDE_TN(_minutes / 60u), FUDE_TS(_mm) }, 2u);
    } else if(_minutes > 0u) {
        fude_text_format(_out, _size, FUDE_TEXT_DURATION_M, (const fude_text_arg[]){ FUDE_TN(_minutes) }, 1u);
    } else {
        fude_text_format(_out, _size, FUDE_TEXT_DURATION_S, (const fude_text_arg[]){ FUDE_TN(lroundf(_seconds)) }, 1u);
    }
}

RDE_INTERNAL void fude_stats_draw_overview(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    c8 _values[13][32];
    const c8* _labels[13] = { fude_text(FUDE_TEXT_TILE_SESSIONS), fude_text(FUDE_TEXT_TILE_CHARACTERS), fude_text(FUDE_TEXT_TILE_SQUARES),
                              fude_text(FUDE_TEXT_TILE_TIME), fude_text(FUDE_TEXT_TILE_DAYS), fude_text(FUDE_TEXT_TILE_STREAK),
                              fude_text(FUDE_TEXT_TILE_BEST_STREAK), fude_text(FUDE_TEXT_TILE_EXAMS), fude_text(FUDE_TEXT_TILE_PASSED),
                              fude_text(FUDE_TEXT_TILE_ACCURACY), fude_text(FUDE_TEXT_TILE_STUDYING), fude_text(FUDE_TEXT_TILE_KNOWN),
                              fude_text(FUDE_TEXT_TILE_WORDS) };
    snprintf(_values[0], 32, "%u", _d->sessions);
    snprintf(_values[1], 32, "%u", _d->characters);
    snprintf(_values[2], 32, "%u", _d->squares);
    fude_stats_duration(_d->writing_seconds, _values[3], 32);
    snprintf(_values[4], 32, "%u", _d->days_active);
    snprintf(_values[5], 32, "%u", _d->streak);
    snprintf(_values[6], 32, "%u", _d->streak_best);
    snprintf(_values[7], 32, "%u", _d->exams);
    snprintf(_values[8], 32, "%u", _d->exams_passed);
    if(_d->exam_items > 0) { snprintf(_values[9], 32, "%.0f%%", (f64)(100.0f * (f32)_d->exam_right / (f32)_d->exam_items)); }
    else                   { snprintf(_values[9], 32, "-"); }
    snprintf(_values[10], 32, "%u", _d->studying);
    snprintf(_values[11], 32, "%u", _d->known);
    snprintf(_values[12], 32, "%u", _d->words_added);
    const u32 _cols = (u32)fmaxf(1.0f, floorf(_b->width / FUDE_STATS_TILE_W));
    const f32 _tw   = _b->width / (f32)_cols;
    for(u32 _i = 0; _i < 13u; _i++) {
        const f32 _x = _b->left + (f32)(_i % _cols) * _tw;
        const f32 _y = _b->top - (f32)(_i / _cols) * FUDE_STATS_TILE_H;
        const rde_color _accent = _i == 5u && _d->streak > 0 ? _theme->score_fair : _theme->text;
        c8 _fit[48];
        fude_draw_text_fit(_b->font, _b->font_px, _values[_i], 30.0f, _tw - 10.0f, _fit, sizeof(_fit));
        fude_draw_text(_b->font, _b->font_px, _fit, _x, _y - 32.0f, 30.0f, _accent);
        fude_draw_text_fit(_b->font, _b->font_px, _labels[_i], 14.0f, _tw - 10.0f, _fit, sizeof(_fit));
        fude_draw_text(_b->font, _b->font_px, _fit, _x, _y - 56.0f, 14.0f, _theme->text_soft);
    }
}

RDE_INTERNAL void fude_stats_draw_activity(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    const f32 _cell = fminf(20.0f, (_b->width - 34.0f) / (f32)FUDE_STATS_WEEKS);
    const f32 _dot  = _cell - 3.0f;
    const c8* _days[7] = { fude_text(FUDE_TEXT_DAY_INITIAL_MON), "", fude_text(FUDE_TEXT_DAY_INITIAL_WED), "", fude_text(FUDE_TEXT_DAY_INITIAL_FRI), "",
                           fude_text(FUDE_TEXT_DAY_INITIAL_SUN) };
    for(u32 _r = 0; _r < 7u; _r++) {
        fude_draw_text(_b->font, _b->font_px, _days[_r], _b->left, _b->top - (f32)_r * _cell - _cell * 0.75f, 12.0f, _theme->text_soft);
    }
    const i64 _start = _d->today - (i64)fude_stats_weekday(_d->today) - 7 * (i64)(FUDE_STATS_WEEKS - 1u);
    for(u32 _i = 0; _i < FUDE_STATS_DAYS; _i++) {
        if(_start + (i64)_i > _d->today) {
            break;   // the rest of this week is still to come
        }
        const u32 _n = (u32)_d->day_squares[_i] + (u32)_d->day_exam_items[_i];
        rde_color _c = _theme->surface_2;
        if(_n > 0) {
            _c = fude_stats_alpha(_theme->score_good, _n >= 30u ? 1.0f : _n >= 12u ? 0.75f : _n >= 4u ? 0.5f : 0.3f);
        }
        const f32 _x = _b->left + 22.0f + (f32)(_i / 7u) * _cell;
        const f32 _y = _b->top - (f32)(_i % 7u) * _cell;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _dot * 0.5f, _y - _dot * 0.5f }, (rde_vec_2F){ _dot, _dot }, 0.4f, 3, _c, NULL);
    }
    c8 _line[96];
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_ACTIVITY_CAPTION, FUDE_TN(FUDE_STATS_WEEKS));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 7.0f * _cell - 22.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 14.0f, _b->width, 0.6f),
                   _theme->text_soft);
}

// A chart area with lines at 0, 50 (fair) and 80 (good), 0..100.
RDE_INTERNAL void fude_stats_chart_frame(const fude_stats_box* _b, f32 _x, f32 _y, f32 _w, f32 _h, f32 _good) {
    const fude_theme* _theme = fude_theme_active();
    const f32 _lines[3] = { 0.0f, 50.0f, _good };
    for(u32 _i = 0; _i < 3u; _i++) {
        const f32 _ly = _y - _h + _h * _lines[_i] / 100.0f;
        fude_draw_line((rde_vec_2F){ _x, _ly }, (rde_vec_2F){ _x + _w, _ly }, 0.5f, _i == 2u ? fude_stats_alpha(_theme->score_good, 0.6f) : _theme->line);
        c8 _label[8];
        snprintf(_label, sizeof(_label), "%.0f", (f64)_lines[_i]);
        fude_stats_text_right(_b, _label, _x - 6.0f, _ly - 5.0f, 12.0f, _theme->text_soft);
    }
}

RDE_INTERNAL void fude_stats_draw_scores(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    c8 _line[160];
    if(_d->squares == 0) {
        fude_stats_draw_empty(_b, FUDE_TEXT_STATS_NO_SCORES);
        return;
    }
    // The last 30 days against the 30 before, and all of it.
    if(_d->average_recent >= 0.0f && _d->average_before >= 0.0f) {
        const f32 _delta = _d->average_recent - _d->average_before;
        c8 _signed[16];
        snprintf(_signed, sizeof(_signed), "%s%ld", _delta >= 0.0f ? "+" : "", lroundf(_delta));
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_SCORES_TREND, FUDE_TN(FUDE_STATS_RECENT), FUDE_TN(lroundf(_d->average_recent)), FUDE_TS(_signed),
                   FUDE_TN(FUDE_STATS_RECENT), FUDE_TN(lroundf(_d->average)));
    } else if(_d->average_recent >= 0.0f) {
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_SCORES_RECENT, FUDE_TN(FUDE_STATS_RECENT), FUDE_TN(lroundf(_d->average_recent)), FUDE_TN(lroundf(_d->average)));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_SCORES_ALL, FUDE_TN(lroundf(_d->average)));
    }
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 18.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 16.0f, _b->width, 0.6f), _theme->text);

    // Weekly averages: a line through the weeks that had practice.
    const f32 _x = _b->left + 28.0f, _y = _b->top - 36.0f, _w = _b->width - 28.0f, _h = 140.0f;
    fude_stats_chart_frame(_b, _x, _y, _w, _h, FUDE_THEME_GRADE_GOOD);
    rde_vec_2F _prev = { 0.0f, 0.0f };
    b8         _have = false;
    for(u32 _i = 0; _i < FUDE_STATS_WEEKS; _i++) {
        if(_d->week_average[_i] < 0.0f) {
            continue;
        }
        const rde_vec_2F _p = { _x + _w * ((f32)_i + 0.5f) / (f32)FUDE_STATS_WEEKS, _y - _h + _h * _d->week_average[_i] / 100.0f };
        if(_have) {
            fude_draw_line(_prev, _p, 1.4f, _theme->button_selected);
        }
        rde_rendering_2d_draw_circle(_p, 3.5f, 16, fude_theme_grade(_d->week_average[_i]), NULL);
        _prev = _p;
        _have = true;
    }
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_SCORES_CAPTION, FUDE_TN(FUDE_STATS_WEEKS));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _y - _h - 36.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 13.0f, _b->width, 0.6f), _theme->text_soft);
}

RDE_INTERNAL void fude_stats_draw_exams(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    c8 _line[160];
    if(_d->exams == 0) {
        fude_stats_draw_empty(_b, FUDE_TEXT_STATS_NO_EXAMS);
        return;
    }
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_EXAMS_LINE, FUDE_TN(_d->exams), FUDE_TN(_d->exams_passed), FUDE_TN(_d->exam_right), FUDE_TN(_d->exam_items),
               FUDE_TN(lroundf(_d->exam_points)));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 18.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 16.0f, _b->width, 0.6f), _theme->text);

    // The last exams: how many right (the bar), how many points (the dot).
    const f32 _x = _b->left + 28.0f, _y = _b->top - 36.0f, _w = _b->width - 28.0f, _h = 130.0f;
    fude_stats_chart_frame(_b, _x, _y, _w, _h, 100.0f * FUDE_EXAM_PASS);
    const f32 _slot = _w / (f32)FUDE_STATS_EXAMS;
    for(u32 _i = 0; _i < _d->recent_exams; _i++) {
        const f32 _cx  = _x + _slot * ((f32)_i + 0.5f);
        const f32 _acc = _d->recent_accuracy[_i];
        const f32 _bh  = _h * _acc;
        const rde_color _c = _acc >= FUDE_EXAM_PASS ? _theme->score_good : _acc >= 0.5f ? _theme->score_fair : _theme->score_poor;
        if(_bh > 0.5f) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _y - _h + _bh * 0.5f }, (rde_vec_2F){ _slot * 0.55f, _bh }, fude_stats_alpha(_c, 0.8f));
        }
        rde_rendering_2d_draw_circle((rde_vec_2F){ _cx, _y - _h + _h * _d->recent_points[_i] / 100.0f }, 3.5f, 16, _theme->text, NULL);
        const c8* _name = fude_exam_source_name((FUDE_EXAM_SOURCE_)_d->recent_source[_i]);
        // Its first two characters (whole ones: a name may be Japanese).
        c8        _short[16] = "";
        const c8* _p         = _name;
        for(u32 _k = 0; _k < 2u; _k++) {
            const c8* _from = _p;
            if(fude_utf8_next(&_p) == 0) { break; }
            strncat(_short, _from, (usize)(_p - _from));
        }
        fude_draw_text(_b->font, _b->font_px, _short, _cx - fude_draw_text_width(_b->font, _b->font_px, _short, 11.0f) * 0.5f, _y - _h - 20.0f, 11.0f,
                       _theme->text_soft);
    }
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_EXAMS_CAPTION, FUDE_TN(FUDE_STATS_EXAMS), FUDE_TN(lroundf(100.0f * FUDE_EXAM_PASS)));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _y - _h - 46.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 13.0f, _b->width, 0.6f), _theme->text_soft);
}

// A chart's top value for _max: a round number at or above it (10 at least).
RDE_INTERNAL u32 fude_stats_round_up(u32 _max) {
    const u32 _steps[3] = { 1u, 2u, 5u };
    for(u32 _scale = 10u;; _scale *= 10u) {
        for(u32 _k = 0; _k < 3u; _k++) {
            if(_steps[_k] * _scale >= _max) {
                return _steps[_k] * _scale;
            }
        }
        if(_scale > 100000000u) {
            return _max;
        }
    }
}

// Known and Studying at the end of each week: stacked bars, Known below.
RDE_INTERNAL void fude_stats_draw_marks(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    c8 _line[160];
    if(_d->mark_changes == 0) {
        const c8* _hint = fude_text(FUDE_TEXT_EXAM_MARK_HINT);
        fude_draw_text(_b->font, _b->font_px, fude_text(FUDE_TEXT_STATS_NO_MARKS), _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
        fude_draw_text(_b->font, _b->font_px, _hint, _b->left, _b->top - 42.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _hint, 13.0f, _b->width, 0.6f),
                       _theme->text_soft);
        return;
    }
    const rde_color _known_c    = _theme->score_good;
    const rde_color _studying_c = fude_stats_alpha(_theme->button_selected, 0.8f);

    // Now, and how Known moved recently.
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_MARKS_NOW, FUDE_TN(_d->known), FUDE_TN(_d->studying));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 18.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 16.0f, _b->width, 0.6f), _theme->text);
    const i64 _delta = (i64)_d->known - (i64)_d->known_before;
    c8 _signed[24];
    snprintf(_signed, sizeof(_signed), "%s%lld", _delta > 0 ? "+" : "", (long long)_delta);
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_MARKS_RECENT, FUDE_TS(_signed), FUDE_TN(FUDE_STATS_RECENT));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 40.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 13.0f, _b->width, 0.6f),
                   _theme->text_soft);

    // The weeks: lines at 0, half and the top, labelled; a bar each.
    u32 _max = 0;
    for(u32 _w = 0; _w < FUDE_STATS_WEEKS; _w++) {
        const u32 _total = _d->week_known[_w] + _d->week_studying[_w];
        _max = _total > _max ? _total : _max;
    }
    const u32 _top_value = fude_stats_round_up(_max);
    const f32 _x = _b->left + 34.0f, _y = _b->top - 60.0f, _w = _b->width - 34.0f, _h = 120.0f;
    for(u32 _i = 0; _i < 3u; _i++) {
        const f32 _ly = _y - _h + _h * (f32)_i * 0.5f;
        fude_draw_line((rde_vec_2F){ _x, _ly }, (rde_vec_2F){ _x + _w, _ly }, 0.5f, _theme->line);
        snprintf(_line, sizeof(_line), "%u", _top_value * _i / 2u);
        fude_stats_text_right(_b, _line, _x - 6.0f, _ly - 5.0f, 12.0f, _theme->text_soft);
    }
    const f32 _slot = _w / (f32)FUDE_STATS_WEEKS;
    const f32 _bw   = fmaxf(2.0f, _slot * 0.62f);
    for(u32 _i = 0; _i < FUDE_STATS_WEEKS; _i++) {
        const f32 _cx = _x + _slot * ((f32)_i + 0.5f);
        const f32 _kh = _h * (f32)_d->week_known[_i] / (f32)_top_value;
        const f32 _sh = _h * (f32)_d->week_studying[_i] / (f32)_top_value;
        if(_kh > 0.25f) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _y - _h + _kh * 0.5f }, (rde_vec_2F){ _bw, _kh }, _known_c);
        }
        if(_sh > 0.25f) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _y - _h + _kh + _sh * 0.5f }, (rde_vec_2F){ _bw, _sh }, _studying_c);
        }
    }

    // The legend, then what it shows.
    const f32 _ly = _y - _h - 32.0f;
    const struct { FUDE_TEXT_ text; rde_color color; } _legend[2] = {
        { FUDE_TEXT_STATS_LEGEND_KNOWN, _known_c },
        { FUDE_TEXT_STATS_LEGEND_STUDYING, _studying_c },
    };
    f32 _lx = _b->left;
    for(u32 _k = 0; _k < 2u; _k++) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _lx + 6.0f, _ly + 4.0f }, (rde_vec_2F){ 12.0f, 12.0f }, _legend[_k].color);
        fude_draw_text(_b->font, _b->font_px, fude_text(_legend[_k].text), _lx + 16.0f, _ly, 13.0f, _theme->text_soft);
        _lx += 16.0f + fude_draw_text_width(_b->font, _b->font_px, fude_text(_legend[_k].text), 13.0f) + 18.0f;
    }
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_MARKS_CAPTION, FUDE_TN(FUDE_STATS_WEEKS));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _ly - 24.0f, fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 13.0f, _b->width, 0.6f),
                   _theme->text_soft);
}

RDE_INTERNAL void fude_stats_draw_coverage(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    // The columns as wide as their widest text: the names, the counts.
    c8  _line[96];
    f32 _label = 0.0f;
    f32 _count = 0.0f;
    for(u32 _g = 0; _g < fude_stats_group_count(); _g++) {
        const fude_stats_coverage* _c = &_d->coverage[_g];
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_LEVEL_LINE, FUDE_TN(_c->known), FUDE_TN(_c->practised), FUDE_TN(_c->total));
        _label = fmaxf(_label, fude_draw_text_width(_b->font, _b->font_px, fude_stats_group_name(_g), 16.0f));
        _count = fmaxf(_count, fude_draw_text_width(_b->font, _b->font_px, _line, 13.0f));
    }
    _label += 14.0f;
    _count += 14.0f;
    const f32 _bar = fmaxf(40.0f, _b->width - _label - _count);
    for(u32 _g = 0; _g < fude_stats_group_count(); _g++) {
        const fude_stats_coverage* _c = &_d->coverage[_g];
        const f32 _y = _b->top - (f32)_g * FUDE_STATS_ROW;
        fude_draw_text(_b->font, _b->font_px, fude_stats_group_name(_g), _b->left, _y - 22.0f, 16.0f, _theme->text);
        const f32 _x = _b->left + _label;
        // The whole level, then practised, studying and known over it.
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _bar * 0.5f, _y - 16.0f }, (rde_vec_2F){ _bar, 14.0f }, _theme->line);
        if(_c->total > 0) {
            const f32 _t = (f32)_c->total;
            fude_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)_c->practised / _t, fude_stats_alpha(_theme->button_selected, 0.35f));
            fude_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)(_c->known + _c->studying) / _t, fude_stats_alpha(_theme->button_selected, 0.8f));
            fude_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)_c->known / _t, _theme->score_good);
        }
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_LEVEL_LINE, FUDE_TN(_c->known), FUDE_TN(_c->practised), FUDE_TN(_c->total));
        fude_stats_text_right(_b, _line, _b->left + _b->width, _y - 22.0f, 13.0f, _theme->text_soft);
    }
    const f32 _ly = _b->top - (f32)fude_stats_group_count() * FUDE_STATS_ROW - 14.0f;
    // The legend: a swatch and its word, each after the last.
    const struct { FUDE_TEXT_ text; rde_color color; } _legend[3] = {
        { FUDE_TEXT_STATS_LEGEND_KNOWN, _theme->score_good },
        { FUDE_TEXT_STATS_LEGEND_STUDYING, fude_stats_alpha(_theme->button_selected, 0.8f) },
        { FUDE_TEXT_STATS_LEGEND_PRACTISED, fude_stats_alpha(_theme->button_selected, 0.35f) },
    };
    f32 _lx = _b->left;
    for(u32 _k = 0; _k < 3u; _k++) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _lx + 6.0f, _ly + 4.0f }, (rde_vec_2F){ 12.0f, 12.0f }, _legend[_k].color);
        fude_draw_text(_b->font, _b->font_px, fude_text(_legend[_k].text), _lx + 16.0f, _ly, 13.0f, _theme->text_soft);
        _lx += 16.0f + fude_draw_text_width(_b->font, _b->font_px, fude_text(_legend[_k].text), 13.0f) + 18.0f;
    }
}

RDE_INTERNAL void fude_stats_draw_mistakes(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    if(_d->squares == 0) {
        fude_stats_draw_empty(_b, FUDE_TEXT_STATS_NO_PRACTICE);
        return;
    }
    c8  _line[80];
    f32 _label = 0.0f;
    for(u32 _m = 0; _m < FUDE_STATS_MISTAKE_COUNT; _m++) {
        _label = fmaxf(_label, fude_draw_text_width(_b->font, _b->font_px, fude_text(FUDE_STATS_MISTAKE_NAMES[_m]), 15.0f));
    }
    _label += 14.0f;
    const f32 _count = fude_draw_text_width(_b->font, _b->font_px, "100%", 14.0f) + 14.0f;
    const f32 _bar   = fmaxf(40.0f, _b->width - _label - _count);
    for(u32 _m = 0; _m < FUDE_STATS_MISTAKE_COUNT; _m++) {
        const f32 _y   = _b->top - (f32)_m * FUDE_STATS_ROW;
        const f32 _all = (f32)_d->mistakes[_m] / (f32)_d->squares;
        fude_draw_text(_b->font, _b->font_px, fude_text(FUDE_STATS_MISTAKE_NAMES[_m]), _b->left, _y - 22.0f, 15.0f, _theme->text);
        const f32 _x = _b->left + _label;
        fude_stats_bar(_x, _y - 6.0f, _bar, 9.0f, _all, fude_stats_alpha(_theme->score_poor, 0.45f));
        if(_d->squares_recent > 0) {
            fude_stats_bar(_x, _y - 17.0f, _bar, 9.0f, (f32)_d->mistakes_recent[_m] / (f32)_d->squares_recent, _theme->score_poor);
        }
        snprintf(_line, sizeof(_line), "%.0f%%", (f64)(100.0f * _all));
        fude_stats_text_right(_b, _line, _b->left + _b->width, _y - 22.0f, 14.0f, _theme->text_soft);
    }
    FUDE_TEXTF(_line, FUDE_TEXT_STATS_MISTAKES_CAPTION, FUDE_TN(FUDE_STATS_RECENT));
    fude_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - (f32)FUDE_STATS_MISTAKE_COUNT * FUDE_STATS_ROW - 14.0f,
                   fude_draw_text_px_to_fit(_b->font, _b->font_px, _line, 13.0f, _b->width, 0.6f), _theme->text_soft);
}

RDE_INTERNAL void fude_stats_draw_characters(const fude_stats_box* _b) {
    fude_stats*            _stats = _b->stats;
    const fude_stats_data* _d     = &_stats->data;
    const fude_theme*      _theme = fude_theme_active();
    if(_d->weakest_count == 0) {
        fude_stats_draw_empty(_b, FUDE_TEXT_STATS_NO_PRACTICE);
        return;
    }
    c8 _line[16];
    for(u32 _list = 0; _list < 2u; _list++) {
        const f32  _y      = _b->top - (f32)_list * (26.0f + FUDE_STATS_GLYPH + 26.0f);
        const u32  _count  = _list == 0 ? _d->weakest_count : _d->improved_count;
        const u32* _recs   = _list == 0 ? _d->weakest : _d->improved;
        fude_draw_text(_b->font, _b->font_px, fude_text(_list == 0 ? FUDE_TEXT_STATS_WEAKEST : FUDE_TEXT_STATS_IMPROVED),
                       _b->left, _y - 16.0f, 15.0f, _theme->text_soft);
        if(_count == 0) {
            fude_draw_text(_b->font, _b->font_px, fude_text(FUDE_TEXT_STATS_TWICE), _b->left, _y - 44.0f, 14.0f, _theme->text_soft);
            continue;
        }
        const f32 _step = FUDE_STATS_GLYPH + 12.0f;
        for(u32 _i = 0; _i < _count && (f32)(_i + 1u) * _step <= _b->width + 12.0f; _i++) {
            fude_kanji_info _info;
            if(!fude_kanji_at(_stats->db, _recs[_i], &_info)) {
                continue;
            }
            const f32 _x  = _b->left + (f32)_i * _step;
            const f32 _gy = _y - 26.0f;
            fude_glyph_character(&_stats->glyph, _info.codepoint, (rde_vec_2F){ _x, _gy }, FUDE_STATS_GLYPH, _theme->ink);
            if(_list == 0) { snprintf(_line, sizeof(_line), "%.0f", (f64)_d->weakest_score[_i]); }
            else           { snprintf(_line, sizeof(_line), "+%.0f", (f64)_d->improved_delta[_i]); }
            fude_draw_text(_b->font, _b->font_px, _line, _x + FUDE_STATS_GLYPH * 0.5f - fude_draw_text_width(_b->font, _b->font_px, _line, 14.0f) * 0.5f,
                           _gy - FUDE_STATS_GLYPH - 14.0f, 14.0f, _list == 0 ? fude_theme_grade(_d->weakest_score[_i]) : _theme->score_good);
            const fude_stats_hit _hit = { _x, _b->content_y + (_b->top - _gy), FUDE_STATS_GLYPH, (u8)_list, _i };
            rde_arr_add(&_stats->hits, &_hit);
        }
    }
}

// Squares written by the hour and by the weekday.
RDE_INTERNAL void fude_stats_draw_when(const fude_stats_box* _b) {
    const fude_stats_data* _d     = &_b->stats->data;
    const fude_theme*      _theme = fude_theme_active();
    if(_d->squares == 0) {
        fude_stats_draw_empty(_b, FUDE_TEXT_STATS_NO_PRACTICE);
        return;
    }
    for(u32 _part = 0; _part < 2u; _part++) {
        const u32  _n      = _part == 0 ? 24u : 7u;
        const u32* _values = _part == 0 ? _d->hours : _d->weekdays;
        u32 _max = 1;
        for(u32 _i = 0; _i < _n; _i++) {
            _max = _values[_i] > _max ? _values[_i] : _max;
        }
        const f32 _y    = _b->top - (f32)_part * 130.0f;
        const f32 _h    = 80.0f;
        const f32 _slot = _b->width / (f32)_n;
        fude_draw_text(_b->font, _b->font_px, fude_text(_part == 0 ? FUDE_TEXT_STATS_BY_HOUR : FUDE_TEXT_STATS_BY_DAY), _b->left, _y - 14.0f, 14.0f, _theme->text_soft);
        const f32 _base = _y - 24.0f - _h;
        for(u32 _i = 0; _i < _n; _i++) {
            const f32 _bh = _h * (f32)_values[_i] / (f32)_max;
            const f32 _cx = _b->left + _slot * ((f32)_i + 0.5f);
            if(_bh > 0.5f) {
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _base + _bh * 0.5f }, (rde_vec_2F){ _slot * 0.6f, _bh }, fude_stats_alpha(_theme->button_selected, 0.75f));
            }
            c8 _label[16];
            if(_part == 0) {
                if(_i % 6u != 0u) { continue; }
                snprintf(_label, sizeof(_label), "%u", _i);
            } else {
                snprintf(_label, sizeof(_label), "%s", fude_text((FUDE_TEXT_)(FUDE_TEXT_DAY_MON + _i)));   // Monday first
            }
            fude_draw_text(_b->font, _b->font_px, _label, _cx - fude_draw_text_width(_b->font, _b->font_px, _label, 12.0f) * 0.5f, _base - 16.0f, 12.0f,
                           _theme->text_soft);
        }
    }
}

void fude_stats_render(fude_stats* _stats, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_stats->open) {
        return;
    }
    fude_stats_font    = _font;
    fude_stats_font_px = _font_px;
    const fude_theme*      _theme  = fude_theme_active();
    const fude_stats_data* _d      = &_stats->data;
    const rde_vec_2I       _size   = rde_window_get_size(_window);
    const rde_vec_4I       _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32              _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_STATS_MARGIN;
    const f32              _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_STATS_MARGIN;

    // The header: since when.
    c8 _line[160];
    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_STATISTICS), _left, _top);
    if(_d->first_time != 0) {
        const time_t _first = (time_t)_d->first_time;
        c8 _date[40];
        fude_text_date(_date, sizeof(_date), (u64)_first);
        FUDE_TEXTF(_line, FUDE_TEXT_STATS_SINCE, FUDE_TS(_date));
        fude_draw_text(_font, _font_px, _line, _left + fude_draw_text_width(_font, _font_px, fude_text(FUDE_TEXT_STATISTICS), 24.0f) + 16.0f, _top - 30.0f, 14.0f,
                       _theme->text_soft);
    }
    const f32 _view_top = _top - 50.0f;
    _stats->view_top    = _view_top;
    _stats->view_bottom = _bottom;
    if(_view_top <= _bottom) {
        return;
    }

    // The cards, in columns: each to the shortest one.
    const u32 _cols   = (_right - _left) >= FUDE_STATS_TWO_COLS ? 2u : 1u;
    const f32 _cw     = ((_right - _left) - FUDE_STATS_GAP * (f32)(_cols - 1u)) / (f32)_cols;
    f32       _col_y[2] = { 0.0f, 0.0f };
    rde_arr_clear(&_stats->hits);
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_view_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_view_top - _bottom) });
    for(u32 _c = 0; _c < FUDE_STATS_CARD_COUNT; _c++) {
        const u32 _col = _cols == 2u && _col_y[1] < _col_y[0] ? 1u : 0u;
        const f32 _h   = fude_stats_card_height(_stats, (FUDE_STATS_CARD_)_c, _cw);
        const f32 _x   = _left + (f32)_col * (_cw + FUDE_STATS_GAP);
        const f32 _cy  = _col_y[_col];   // content y of its top
        _col_y[_col]  += _h + FUDE_STATS_GAP;
        const f32 _y   = _view_top - (_cy - _stats->scroller.offset);   // its top on screen
        if(_y - _h > _view_top || _y < _bottom) {
            continue;
        }
        fude_draw_card((rde_vec_2F){ _x, _y - _h }, (rde_vec_2F){ _x + _cw, _y }, 16.0f, _theme->surface, _theme->outline);
        fude_draw_text(_font, _font_px, fude_text(FUDE_STATS_CARD_TITLES[_c]), _x + FUDE_STATS_PAD, _y - FUDE_STATS_PAD - 18.0f, 16.0f, _theme->text);
        const fude_stats_box _box = {
            .stats = _stats, .font = _font, .font_px = _font_px, .left = _x + FUDE_STATS_PAD, .width = _cw - 2.0f * FUDE_STATS_PAD,
            .top = _y - FUDE_STATS_PAD - FUDE_STATS_TITLE, .content_y = _cy + FUDE_STATS_PAD + FUDE_STATS_TITLE
        };
        switch((FUDE_STATS_CARD_)_c) {
            case FUDE_STATS_CARD_OVERVIEW:   fude_stats_draw_overview(&_box);   break;
            case FUDE_STATS_CARD_ACTIVITY:   fude_stats_draw_activity(&_box);   break;
            case FUDE_STATS_CARD_SCORES:     fude_stats_draw_scores(&_box);     break;
            case FUDE_STATS_CARD_EXAMS:      fude_stats_draw_exams(&_box);      break;
            case FUDE_STATS_CARD_MARKS:      fude_stats_draw_marks(&_box);      break;
            case FUDE_STATS_CARD_COVERAGE:   fude_stats_draw_coverage(&_box);   break;
            case FUDE_STATS_CARD_MISTAKES:   fude_stats_draw_mistakes(&_box);   break;
            case FUDE_STATS_CARD_CHARACTERS: fude_stats_draw_characters(&_box); break;
            case FUDE_STATS_CARD_WHEN:       fude_stats_draw_when(&_box);       break;
            default: break;
        }
    }
    rde_rendering_end_clipping_rect();
    _stats->content_height = fmaxf(_col_y[0], _col_y[1]);
}

// --- the screen (screen.h): its row ---------------------------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"

FUDE_SCREEN_ADAPTERS(fude_stats, fude_stats)
FUDE_SCREEN_RENDER(fude_stats, fude_stats)

// A character tapped opens the viewer, walking its list.
RDE_INTERNAL void fude_stats_screen_update(fude_app* _app, void* _self, f32 _dt) {
    fude_stats* _stats = (fude_stats*)_self;
    fude_stats_update(_stats, _dt);
    const u32* _records;
    u32        _count, _position;
    if(fude_stats_take_tap(_stats, &_records, &_count, &_position)) {
        fude_study_view(_app, _records, _count, _position);
    }
}

FUDE_ROW_CALL(fude_stats_row_close, fude_stats, fude_stats_close)

static const fude_row_button FUDE_STATS_BUTTONS[] = {
    { FUDE_TEXT_CLOSE, FUDE_ICON_CLOSE, fude_stats_row_close, 0, FUDE_ROW_QUIET, false, NULL },
};
static const fude_row_def FUDE_STATS_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_STATS_BUTTONS) };

RDE_INTERNAL u32 fude_stats_screen_row(const void* _self) {
    RDE_UNUSED(_self);
    return 0u;
}

const fude_screen FUDE_STATS_SCREEN = {
    .name = "stats", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_stats_screen_is_open, .close = fude_stats_screen_close,
    .update = fude_stats_screen_update, .render = fude_stats_screen_render,
    .pointer_down = fude_stats_screen_down, .pointer_moved = fude_stats_screen_moved, .pointer_up = fude_stats_screen_up,
    .rows = FUDE_STATS_BUTTON_ROWS, .row_count = 1u, .row = fude_stats_screen_row,
    .field_hint = FUDE_TEXT_COUNT,
};
