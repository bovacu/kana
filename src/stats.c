#include "stats.h"
#include "chart.h"
#include "draw.h"
#include "exam.h"
#include "examlog.h"
#include "history.h"
#include "marks.h"
#include "theme.h"
#include "userwords.h"

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
RDE_INTERNAL i64 kana_stats_local_offset(void) {
    const time_t _now = time(NULL);
    struct tm    _utc = *gmtime(&_now);
    _utc.tm_isdst     = -1;
    return (i64)difftime(_now, mktime(&_utc));
}

RDE_INTERNAL i64 kana_stats_day(u64 _time, i64 _offset) {
    const i64 _t = (i64)_time + _offset;
    return _t >= 0 ? _t / 86400 : (_t - 86399) / 86400;
}

// Monday 0 .. Sunday 6 (day 0, 1970-01-01, was a Thursday).
RDE_INTERNAL u32 kana_stats_weekday(i64 _day) {
    return (u32)(((_day + 3) % 7 + 7) % 7);
}

RDE_INTERNAL int kana_stats_cmp_i64(const void* _a, const void* _b) {
    const i64 _x = *(const i64*)_a, _y = *(const i64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int kana_stats_cmp_u32(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

typedef struct {
    u32 record;
    f32 value;
} kana_stats_ranked;

RDE_INTERNAL int kana_stats_by_value_up(const void* _a, const void* _b) {
    const f32 _x = ((const kana_stats_ranked*)_a)->value, _y = ((const kana_stats_ranked*)_b)->value;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int kana_stats_by_value_down(const void* _a, const void* _b) {
    return -kana_stats_by_value_up(_a, _b);
}

void kana_stats_compute(kana_stats_data* _d, const kana_kanji_db* _db, const kana_catalog* _catalog) {
    memset(_d, 0, sizeof(*_d));
    _d->average = _d->average_recent = _d->average_before = _d->exam_points = -1.0f;
    for(u32 _w = 0; _w < KANA_STATS_WEEKS; _w++) {
        _d->week_average[_w] = -1.0f;
    }
    const i64 _off   = kana_stats_local_offset();
    _d->today        = kana_stats_day((u64)time(NULL), _off);
    // The calendar: whole weeks, Monday first, the last one holding today.
    const i64 _start = _d->today - (i64)kana_stats_weekday(_d->today) - 7 * (i64)(KANA_STATS_WEEKS - 1u);

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _days      = rde_arr_new(sizeof(i64), _heap);   // every active day, with repeats
    rde_arr _practised = rde_arr_new(sizeof(u32), _heap);   // code points with history
    rde_arr _weakest   = rde_arr_new(sizeof(kana_stats_ranked), _heap);
    rde_arr _improved  = rde_arr_new(sizeof(kana_stats_ranked), _heap);
    f64 _sum = 0.0, _sum_recent = 0.0, _sum_before = 0.0;
    u32 _n_before = 0;
    f64 _week_sum[KANA_STATS_WEEKS] = { 0 };

    // --- practice: every character's history ---
    kana_history_list(&_practised);
    qsort(_practised.memory, rde_arr_length(&_practised), sizeof(u32), kana_stats_cmp_u32);
    kana_history _h;
    kana_history_init(&_h);
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_practised); _c++) {
        const u32 _cp = ((const u32*)_practised.memory)[_c];
        if(!kana_history_load(&_h, _cp) || rde_arr_length(&_h.sessions) == 0) {
            continue;
        }
        _d->characters++;
        const kana_history_session* _sessions = (const kana_history_session*)_h.sessions.memory;
        const kana_history_square*  _squares  = (const kana_history_square*)_h.squares.memory;
        const kana_history_stroke*  _strokes  = (const kana_history_stroke*)_h.strokes.memory;
        const kana_history_point*   _points   = (const kana_history_point*)_h.points.memory;
        const u32                   _ns       = (u32)rde_arr_length(&_h.sessions);
        for(u32 _s = 0; _s < _ns; _s++) {
            const kana_history_session* _session = &_sessions[_s];
            const i64 _day = kana_stats_day(_session->time, _off);
            _d->sessions++;
            rde_arr_add(&_days, &_day);
            if(_d->first_time == 0 || _session->time < _d->first_time) {
                _d->first_time = _session->time;
            }
            const i64 _age = _d->today - _day;
            for(u32 _q = 0; _q < _session->square_count; _q++) {
                const kana_history_square* _sq = &_squares[_session->first_square + _q];
                for(u32 _k = 0; _k < _sq->stroke_count; _k++) {
                    const kana_history_stroke* _st = &_strokes[_sq->first_stroke + _k];
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
                _d->weekdays[kana_stats_weekday(_day)]++;
                if(_day >= _start && _day < _start + (i64)KANA_STATS_DAYS) {
                    const u32 _i = (u32)(_day - _start);
                    _d->day_squares[_i] = (u16)(_d->day_squares[_i] < 65535u ? _d->day_squares[_i] + 1u : 65535u);
                    _week_sum[_i / 7u] += _score;
                    _d->week_squares[_i / 7u]++;
                }
                const b8 _recent = _age >= 0 && _age < KANA_STATS_RECENT;
                if(_recent) {
                    _sum_recent += _score;
                    _d->squares_recent++;
                } else if(_age >= KANA_STATS_RECENT && _age < 2 * KANA_STATS_RECENT) {
                    _sum_before += _score;
                    _n_before++;
                }
                const b8 _mistake[KANA_STATS_MISTAKE_COUNT] = {
                    _sq->score.misplaced > 0, _sq->score.reversed > 0, _sq->score.missing > 0, _sq->score.extra > 0,
                    _sq->score.shape < KANA_STATS_POOR_SHAPE
                };
                for(u32 _m = 0; _m < KANA_STATS_MISTAKE_COUNT; _m++) {
                    _d->mistakes[_m]        += _mistake[_m] ? 1u : 0u;
                    _d->mistakes_recent[_m] += _mistake[_m] && _recent ? 1u : 0u;
                }
            }
        }

        // The character: its latest session, and how far it came from its first.
        u32 _record;
        if(kana_kanji_find_index(_db, _cp, &_record)) {
            const kana_stats_ranked _latest = { _record, _sessions[_ns - 1u].average };
            rde_arr_add(&_weakest, &_latest);
            if(_ns >= 2u && _sessions[_ns - 1u].average > _sessions[0].average) {
                const kana_stats_ranked _gain = { _record, _sessions[_ns - 1u].average - _sessions[0].average };
                rde_arr_add(&_improved, &_gain);
            }
        }
    }
    kana_history_destroy(&_h);

    _d->average        = _d->squares > 0 ? (f32)(_sum / (f64)_d->squares) : -1.0f;
    _d->average_recent = _d->squares_recent > 0 ? (f32)(_sum_recent / (f64)_d->squares_recent) : -1.0f;
    _d->average_before = _n_before > 0 ? (f32)(_sum_before / (f64)_n_before) : -1.0f;
    for(u32 _w = 0; _w < KANA_STATS_WEEKS; _w++) {
        _d->week_average[_w] = _d->week_squares[_w] > 0 ? (f32)(_week_sum[_w] / (f64)_d->week_squares[_w]) : -1.0f;
    }

    // --- exams ---
    const kana_examlog_exam* _exams = kana_examlog_exams();
    const u32                _ne    = kana_examlog_count();
    f64 _points = 0.0;
    for(u32 _e = 0; _e < _ne; _e++) {
        const kana_examlog_exam* _x = &_exams[_e];
        _d->exams++;
        _d->exams_passed += (f32)_x->correct >= KANA_EXAM_PASS * (f32)_x->item_count ? 1u : 0u;
        _d->exam_items   += _x->item_count;
        _d->exam_right   += _x->correct;
        _points          += (f64)_x->score * (f64)_x->item_count;
        const i64 _day = kana_stats_day(_x->time, _off);
        rde_arr_add(&_days, &_day);
        if(_d->first_time == 0 || _x->time < _d->first_time) {
            _d->first_time = _x->time;
        }
        if(_day >= _start && _day < _start + (i64)KANA_STATS_DAYS) {
            const u32 _i = (u32)(_day - _start);
            _d->day_exam_items[_i] = (u16)fminf(65535.0f, (f32)_d->day_exam_items[_i] + (f32)_x->item_count);
        }
    }
    _d->exam_points  = _d->exam_items > 0 ? (f32)(_points / (f64)_d->exam_items) : -1.0f;
    _d->recent_exams = _ne < KANA_STATS_EXAMS ? _ne : KANA_STATS_EXAMS;
    for(u32 _i = 0; _i < _d->recent_exams; _i++) {
        const kana_examlog_exam* _x = &_exams[_ne - _d->recent_exams + _i];
        _d->recent_accuracy[_i] = _x->item_count > 0 ? (f32)_x->correct / (f32)_x->item_count : 0.0f;
        _d->recent_points[_i]   = _x->score;
        _d->recent_source[_i]   = _x->source;
    }

    // --- days: how many, and the streaks ---
    const u32 _nd = (u32)rde_arr_length(&_days);
    i64*      _dv = (i64*)_days.memory;
    qsort(_dv, _nd, sizeof(i64), kana_stats_cmp_i64);
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
    _d->studying = kana_marks_count(KANA_MARK_STUDYING);
    _d->known    = kana_marks_count(KANA_MARK_KNOWN);
    _d->words_added = kana_userwords_total();
    for(u32 _r = 0; _db != NULL && _catalog != NULL && _r < _db->count; _r++) {
        kana_kanji_info _info;
        if(!kana_catalog_passes(_catalog, _r, KANA_FILTER_ALL) || !kana_kanji_at(_db, _r, &_info)) {
            continue;
        }
        i32 _group = -1;
        if(kana_catalog_passes(_catalog, _r, KANA_FILTER_HIRAGANA))      { _group = kana_chart_core_kana(_info.codepoint) ? KANA_STATS_GROUP_HIRAGANA : -1; }
        else if(kana_catalog_passes(_catalog, _r, KANA_FILTER_KATAKANA)) { _group = kana_chart_core_kana(_info.codepoint) ? KANA_STATS_GROUP_KATAKANA : -1; }
        else if(_info.jlpt_n >= 1 && _info.jlpt_n <= 5)                  { _group = KANA_STATS_GROUP_N5 + (5 - (i32)_info.jlpt_n); }
        if(_group < 0) {
            continue;
        }
        kana_stats_coverage* _cov = &_d->coverage[_group];
        _cov->total++;
        _cov->practised += bsearch(&_info.codepoint, _practised.memory, rde_arr_length(&_practised), sizeof(u32), kana_stats_cmp_u32) != NULL ? 1u : 0u;
        const KANA_MARK_ _mark = kana_marks_get(_info.codepoint);
        _cov->studying += _mark == KANA_MARK_STUDYING ? 1u : 0u;
        _cov->known    += _mark == KANA_MARK_KNOWN ? 1u : 0u;
    }

    // --- the weakest, the most improved ---
    qsort(_weakest.memory, rde_arr_length(&_weakest), sizeof(kana_stats_ranked), kana_stats_by_value_up);
    qsort(_improved.memory, rde_arr_length(&_improved), sizeof(kana_stats_ranked), kana_stats_by_value_down);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_weakest) && _i < KANA_STATS_LIST; _i++) {
        _d->weakest[_i]       = ((const kana_stats_ranked*)_weakest.memory)[_i].record;
        _d->weakest_score[_i] = ((const kana_stats_ranked*)_weakest.memory)[_i].value;
        _d->weakest_count++;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_improved) && _i < KANA_STATS_LIST; _i++) {
        _d->improved[_i]       = ((const kana_stats_ranked*)_improved.memory)[_i].record;
        _d->improved_delta[_i] = ((const kana_stats_ranked*)_improved.memory)[_i].value;
        _d->improved_count++;
    }

    rde_arr* _arrays[] = { &_days, &_practised, &_weakest, &_improved };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        rde_arr_free(_arrays[_i]);
    }
}

// --- the screen ------------------------------------------------------------------------

#define KANA_STATS_MARGIN    16.0f
#define KANA_STATS_GAP       14.0f     // between cards
#define KANA_STATS_PAD       16.0f     // inside a card
#define KANA_STATS_TITLE     40.0f     // a card's title line
#define KANA_STATS_TWO_COLS  880.0f    // this wide and more: two columns of cards
#define KANA_STATS_TILE_W    180.0f    // a number and its label: "characters practised" fits
#define KANA_STATS_TILE_H    74.0f
#define KANA_STATS_ROW       34.0f     // a bar's row
#define KANA_STATS_GLYPH     64.0f     // a character in the weakest / most improved

typedef enum {
    KANA_STATS_CARD_OVERVIEW = 0,
    KANA_STATS_CARD_ACTIVITY,
    KANA_STATS_CARD_SCORES,
    KANA_STATS_CARD_EXAMS,
    KANA_STATS_CARD_COVERAGE,
    KANA_STATS_CARD_MISTAKES,
    KANA_STATS_CARD_CHARACTERS,
    KANA_STATS_CARD_WHEN,
    KANA_STATS_CARD_COUNT
} KANA_STATS_CARD_;

static const c8* const KANA_STATS_CARD_TITLES[KANA_STATS_CARD_COUNT] = {
    "Overview", "Activity", "Practice scores", "Exams", "Levels", "Mistakes", "Characters", "When you practise"
};
static const c8* const KANA_STATS_GROUP_NAMES[KANA_STATS_GROUP_COUNT]     = { "Hiragana", "Katakana", "N5", "N4", "N3", "N2", "N1" };
static const c8* const KANA_STATS_MISTAKE_NAMES[KANA_STATS_MISTAKE_COUNT] = { "Stroke order", "Direction", "Missing strokes", "Extra strokes", "Shape" };

void kana_stats_init(kana_stats* _stats, const kana_kanji_db* _db, const kana_catalog* _catalog) {
    memset(_stats, 0, sizeof(*_stats));
    _stats->db          = _db;
    _stats->catalog     = _catalog;
    _stats->tapped_list = -1;
    _stats->hits        = rde_arr_new(sizeof(kana_stats_hit), rde_memory_allocator_get_default_std());
    kana_glyph_init(&_stats->glyph, _db);
}

void kana_stats_destroy(kana_stats* _stats) {
    if(rde_arr_is_inited(&_stats->hits)) {
        rde_arr_free(&_stats->hits);
    }
    kana_glyph_destroy(&_stats->glyph);
    memset(_stats, 0, sizeof(*_stats));
}

void kana_stats_open(kana_stats* _stats) {
    kana_stats_compute(&_stats->data, _stats->db, _stats->catalog);
    kana_scroller_stop(&_stats->scroller);
    _stats->scroller.offset = 0.0f;
    _stats->tapped_list     = -1;
    _stats->open            = true;
}

void kana_stats_close(kana_stats* _stats) {
    _stats->open = false;
}

void kana_stats_pointer_down(kana_stats* _stats, rde_vec_2F _screen, f64 _time)  { kana_scroller_down(&_stats->scroller, _screen, _time); }
void kana_stats_pointer_moved(kana_stats* _stats, rde_vec_2F _screen, f64 _time) { kana_scroller_moved(&_stats->scroller, _screen, _time); }
void kana_stats_pointer_up(kana_stats* _stats, f64 _time)                         { kana_scroller_up(&_stats->scroller, _time); }

b8 kana_stats_take_tap(kana_stats* _stats, const u32** _records, u32* _count, u32* _position) {
    if(_stats->tapped_list < 0) {
        return false;
    }
    *_records  = _stats->tapped_list == 0 ? _stats->data.weakest : _stats->data.improved;
    *_count    = _stats->tapped_list == 0 ? _stats->data.weakest_count : _stats->data.improved_count;
    *_position = _stats->tapped_index;
    _stats->tapped_list = -1;
    return true;
}

void kana_stats_update(kana_stats* _stats, f32 _dt) {
    if(!_stats->open) {
        return;
    }
    kana_scroller_update(&_stats->scroller, _dt, _stats->content_height, _stats->view_top - _stats->view_bottom);
    rde_vec_2F _at;
    if(kana_scroller_take_tap(&_stats->scroller, &_at)) {
        const f32 _y = _stats->view_top - _at.y + _stats->scroller.offset;   // content space
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_stats->hits); _i++) {
            const kana_stats_hit* _hit = &((const kana_stats_hit*)_stats->hits.memory)[_i];
            if(_at.x >= _hit->x && _at.x <= _hit->x + _hit->size && _y >= _hit->y && _y <= _hit->y + _hit->size) {
                _stats->tapped_list  = _hit->list;
                _stats->tapped_index = _hit->index;
            }
        }
    }
}

// How tall a card is at _width.
RDE_INTERNAL f32 kana_stats_card_height(const kana_stats* _stats, KANA_STATS_CARD_ _card, f32 _width) {
    const f32 _inner = _width - 2.0f * KANA_STATS_PAD;
    const f32 _frame = KANA_STATS_TITLE + 2.0f * KANA_STATS_PAD;
    switch(_card) {
        case KANA_STATS_CARD_OVERVIEW: {
            const u32 _cols = (u32)fmaxf(1.0f, floorf(_inner / KANA_STATS_TILE_W));
            return _frame + (f32)((13u + _cols - 1u) / _cols) * KANA_STATS_TILE_H;
        }
        case KANA_STATS_CARD_ACTIVITY: {
            const f32 _cell = fminf(20.0f, (_inner - 34.0f) / (f32)KANA_STATS_WEEKS);
            return _frame + 7.0f * _cell + 34.0f;
        }
        case KANA_STATS_CARD_SCORES:     return _frame + 220.0f;
        case KANA_STATS_CARD_EXAMS:      return _frame + (_stats->data.exams > 0 ? 222.0f : 30.0f);
        case KANA_STATS_CARD_COVERAGE:   return _frame + (f32)KANA_STATS_GROUP_COUNT * KANA_STATS_ROW + 28.0f;
        case KANA_STATS_CARD_MISTAKES:   return _frame + (_stats->data.squares > 0 ? (f32)KANA_STATS_MISTAKE_COUNT * KANA_STATS_ROW + 28.0f : 30.0f);
        case KANA_STATS_CARD_CHARACTERS: return _frame + (_stats->data.weakest_count > 0 ? 2.0f * (26.0f + KANA_STATS_GLYPH + 26.0f) : 30.0f);
        case KANA_STATS_CARD_WHEN:       return _frame + (_stats->data.squares > 0 ? 2.0f * 130.0f : 30.0f);
        default:                         return _frame;
    }
}

// Drawing context for a card: its inner box on screen.
typedef struct {
    kana_stats* stats;
    rde_font*   font;
    f32         font_px;
    f32         left;      // inner left, screen
    f32         width;     // inner width
    f32         top;       // inner top (below the title), screen
    f32         content_y; // the inner top in content space (for taps)
} kana_stats_box;

RDE_INTERNAL rde_color kana_stats_alpha(rde_color _c, f32 _a) {
    _c.a = (u8)fmaxf(0.0f, fminf(255.0f, (f32)_c.a * _a));
    return _c;
}

// A horizontal bar: _value of _max, from _x, _w long at most.
RDE_INTERNAL void kana_stats_bar(f32 _x, f32 _y, f32 _w, f32 _h, f32 _fraction, rde_color _color) {
    const f32 _len = _w * fmaxf(0.0f, fminf(1.0f, _fraction));
    if(_len > 0.5f) {
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _len * 0.5f, _y - _h * 0.5f }, (rde_vec_2F){ _len, _h }, _color);
    }
}

RDE_INTERNAL void kana_stats_text_right(const kana_stats_box* _b, const c8* _text, f32 _right, f32 _y, f32 _px, rde_color _color) {
    kana_draw_text(_b->font, _b->font_px, _text, _right - kana_draw_text_width(_b->font, _b->font_px, _text, _px), _y, _px, _color);
}

RDE_INTERNAL void kana_stats_duration(f32 _seconds, c8* _out, usize _size) {
    const u32 _minutes = (u32)(_seconds / 60.0f + 0.5f);
    if(_minutes >= 60u) { snprintf(_out, _size, "%u h %02u min", _minutes / 60u, _minutes % 60u); }
    else if(_minutes > 0u) { snprintf(_out, _size, "%u min", _minutes); }
    else { snprintf(_out, _size, "%.0f s", (f64)_seconds); }
}

RDE_INTERNAL void kana_stats_draw_overview(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    c8 _values[13][32];
    const c8* _labels[13] = { "practice sessions", "characters practised", "squares written", "writing time", "days active", "day streak",
                              "best streak", "exams taken", "exams passed", "exam accuracy", "studying", "known", "words added" };
    snprintf(_values[0], 32, "%u", _d->sessions);
    snprintf(_values[1], 32, "%u", _d->characters);
    snprintf(_values[2], 32, "%u", _d->squares);
    kana_stats_duration(_d->writing_seconds, _values[3], 32);
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
    const u32 _cols = (u32)fmaxf(1.0f, floorf(_b->width / KANA_STATS_TILE_W));
    const f32 _tw   = _b->width / (f32)_cols;
    for(u32 _i = 0; _i < 13u; _i++) {
        const f32 _x = _b->left + (f32)(_i % _cols) * _tw;
        const f32 _y = _b->top - (f32)(_i / _cols) * KANA_STATS_TILE_H;
        const rde_color _accent = _i == 5u && _d->streak > 0 ? _theme->score_fair : _theme->text;
        c8 _fit[48];
        kana_draw_text_fit(_b->font, _b->font_px, _values[_i], 30.0f, _tw - 10.0f, _fit, sizeof(_fit));
        kana_draw_text(_b->font, _b->font_px, _fit, _x, _y - 32.0f, 30.0f, _accent);
        kana_draw_text_fit(_b->font, _b->font_px, _labels[_i], 14.0f, _tw - 10.0f, _fit, sizeof(_fit));
        kana_draw_text(_b->font, _b->font_px, _fit, _x, _y - 56.0f, 14.0f, _theme->text_soft);
    }
}

RDE_INTERNAL void kana_stats_draw_activity(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    const f32 _cell = fminf(20.0f, (_b->width - 34.0f) / (f32)KANA_STATS_WEEKS);
    const f32 _dot  = _cell - 3.0f;
    const c8* _days[7] = { "M", "", "W", "", "F", "", "S" };
    for(u32 _r = 0; _r < 7u; _r++) {
        kana_draw_text(_b->font, _b->font_px, _days[_r], _b->left, _b->top - (f32)_r * _cell - _cell * 0.75f, 12.0f, _theme->text_soft);
    }
    const i64 _start = _d->today - (i64)kana_stats_weekday(_d->today) - 7 * (i64)(KANA_STATS_WEEKS - 1u);
    for(u32 _i = 0; _i < KANA_STATS_DAYS; _i++) {
        if(_start + (i64)_i > _d->today) {
            break;   // the rest of this week is still to come
        }
        const u32 _n = (u32)_d->day_squares[_i] + (u32)_d->day_exam_items[_i];
        rde_color _c = _theme->line;
        if(_n > 0) {
            _c = kana_stats_alpha(_theme->score_good, _n >= 30u ? 1.0f : _n >= 12u ? 0.75f : _n >= 4u ? 0.5f : 0.3f);
        }
        const f32 _x = _b->left + 22.0f + (f32)(_i / 7u) * _cell;
        const f32 _y = _b->top - (f32)(_i % 7u) * _cell;
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _dot * 0.5f, _y - _dot * 0.5f }, (rde_vec_2F){ _dot, _dot }, _c);
    }
    c8 _line[96];
    snprintf(_line, sizeof(_line), "The last %u weeks: squares practised and exam answers, a day each", KANA_STATS_WEEKS);
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 7.0f * _cell - 22.0f, 14.0f, _theme->text_soft);
}

// A chart area with lines at 0, 50 (fair) and 80 (good), 0..100.
RDE_INTERNAL void kana_stats_chart_frame(const kana_stats_box* _b, f32 _x, f32 _y, f32 _w, f32 _h, f32 _good) {
    const kana_theme* _theme = kana_theme_active();
    const f32 _lines[3] = { 0.0f, 50.0f, _good };
    for(u32 _i = 0; _i < 3u; _i++) {
        const f32 _ly = _y - _h + _h * _lines[_i] / 100.0f;
        kana_draw_line((rde_vec_2F){ _x, _ly }, (rde_vec_2F){ _x + _w, _ly }, 0.5f, _i == 2u ? kana_stats_alpha(_theme->score_good, 0.6f) : _theme->line);
        c8 _label[8];
        snprintf(_label, sizeof(_label), "%.0f", (f64)_lines[_i]);
        kana_stats_text_right(_b, _label, _x - 6.0f, _ly - 5.0f, 12.0f, _theme->text_soft);
    }
}

RDE_INTERNAL void kana_stats_draw_scores(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    c8 _line[160];
    if(_d->squares == 0) {
        kana_draw_text(_b->font, _b->font_px, "No practice yet: scores show here week by week.", _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
        return;
    }
    // The last 30 days against the 30 before, and all of it.
    if(_d->average_recent >= 0.0f && _d->average_before >= 0.0f) {
        const f32 _delta = _d->average_recent - _d->average_before;
        snprintf(_line, sizeof(_line), "Last %d days %.0f (%s%.0f on the %d before)  -  all time %.0f", KANA_STATS_RECENT, (f64)_d->average_recent,
                 _delta >= 0.0f ? "+" : "", (f64)_delta, KANA_STATS_RECENT, (f64)_d->average);
    } else if(_d->average_recent >= 0.0f) {
        snprintf(_line, sizeof(_line), "Last %d days %.0f  -  all time %.0f", KANA_STATS_RECENT, (f64)_d->average_recent, (f64)_d->average);
    } else {
        snprintf(_line, sizeof(_line), "All time %.0f", (f64)_d->average);
    }
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 18.0f, 16.0f, _theme->text);

    // Weekly averages: a line through the weeks that had practice.
    const f32 _x = _b->left + 28.0f, _y = _b->top - 36.0f, _w = _b->width - 28.0f, _h = 140.0f;
    kana_stats_chart_frame(_b, _x, _y, _w, _h, KANA_THEME_GRADE_GOOD);
    rde_vec_2F _prev = { 0.0f, 0.0f };
    b8         _have = false;
    for(u32 _i = 0; _i < KANA_STATS_WEEKS; _i++) {
        if(_d->week_average[_i] < 0.0f) {
            continue;
        }
        const rde_vec_2F _p = { _x + _w * ((f32)_i + 0.5f) / (f32)KANA_STATS_WEEKS, _y - _h + _h * _d->week_average[_i] / 100.0f };
        if(_have) {
            kana_draw_line(_prev, _p, 1.4f, _theme->button_selected);
        }
        rde_rendering_2d_draw_circle(_p, 3.5f, 16, kana_theme_grade(_d->week_average[_i]), NULL);
        _prev = _p;
        _have = true;
    }
    snprintf(_line, sizeof(_line), "Average score each week, the last %u weeks", KANA_STATS_WEEKS);
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _y - _h - 36.0f, 13.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_stats_draw_exams(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    c8 _line[160];
    if(_d->exams == 0) {
        kana_draw_text(_b->font, _b->font_px, "No exams yet: Exams, in the side panel.", _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
        return;
    }
    snprintf(_line, sizeof(_line), "%u taken, %u passed  -  %u of %u answers right  -  %.0f points", _d->exams, _d->exams_passed, _d->exam_right,
             _d->exam_items, (f64)_d->exam_points);
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - 18.0f, 16.0f, _theme->text);

    // The last exams: how many right (the bar), how many points (the dot).
    const f32 _x = _b->left + 28.0f, _y = _b->top - 36.0f, _w = _b->width - 28.0f, _h = 130.0f;
    kana_stats_chart_frame(_b, _x, _y, _w, _h, 100.0f * KANA_EXAM_PASS);
    const f32 _slot = _w / (f32)KANA_STATS_EXAMS;
    for(u32 _i = 0; _i < _d->recent_exams; _i++) {
        const f32 _cx  = _x + _slot * ((f32)_i + 0.5f);
        const f32 _acc = _d->recent_accuracy[_i];
        const f32 _bh  = _h * _acc;
        const rde_color _c = _acc >= KANA_EXAM_PASS ? _theme->score_good : _acc >= 0.5f ? _theme->score_fair : _theme->score_poor;
        if(_bh > 0.5f) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _y - _h + _bh * 0.5f }, (rde_vec_2F){ _slot * 0.55f, _bh }, kana_stats_alpha(_c, 0.8f));
        }
        rde_rendering_2d_draw_circle((rde_vec_2F){ _cx, _y - _h + _h * _d->recent_points[_i] / 100.0f }, 3.5f, 16, _theme->text, NULL);
        const c8* _name = kana_exam_source_name((KANA_EXAM_SOURCE_)_d->recent_source[_i]);
        c8 _short[4] = { _name[0], _name[1], 0, 0 };
        kana_draw_text(_b->font, _b->font_px, _short, _cx - kana_draw_text_width(_b->font, _b->font_px, _short, 11.0f) * 0.5f, _y - _h - 20.0f, 11.0f,
                       _theme->text_soft);
    }
    snprintf(_line, sizeof(_line), "The last %u: bars, the share right (passed at %.0f%%); dots, the points", KANA_STATS_EXAMS, (f64)(100.0f * KANA_EXAM_PASS));
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _y - _h - 46.0f, 13.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_stats_draw_coverage(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    // The columns as wide as their widest text: the names, the counts.
    c8  _line[96];
    f32 _label = 0.0f;
    f32 _count = 0.0f;
    for(u32 _g = 0; _g < KANA_STATS_GROUP_COUNT; _g++) {
        const kana_stats_coverage* _c = &_d->coverage[_g];
        snprintf(_line, sizeof(_line), "%u known, %u practised / %u", _c->known, _c->practised, _c->total);
        _label = fmaxf(_label, kana_draw_text_width(_b->font, _b->font_px, KANA_STATS_GROUP_NAMES[_g], 16.0f));
        _count = fmaxf(_count, kana_draw_text_width(_b->font, _b->font_px, _line, 13.0f));
    }
    _label += 14.0f;
    _count += 14.0f;
    const f32 _bar = fmaxf(40.0f, _b->width - _label - _count);
    for(u32 _g = 0; _g < KANA_STATS_GROUP_COUNT; _g++) {
        const kana_stats_coverage* _c = &_d->coverage[_g];
        const f32 _y = _b->top - (f32)_g * KANA_STATS_ROW;
        kana_draw_text(_b->font, _b->font_px, KANA_STATS_GROUP_NAMES[_g], _b->left, _y - 22.0f, 16.0f, _theme->text);
        const f32 _x = _b->left + _label;
        // The whole level, then practised, studying and known over it.
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + _bar * 0.5f, _y - 16.0f }, (rde_vec_2F){ _bar, 14.0f }, _theme->line);
        if(_c->total > 0) {
            const f32 _t = (f32)_c->total;
            kana_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)_c->practised / _t, kana_stats_alpha(_theme->button_selected, 0.35f));
            kana_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)(_c->known + _c->studying) / _t, kana_stats_alpha(_theme->button_selected, 0.8f));
            kana_stats_bar(_x, _y - 9.0f, _bar, 14.0f, (f32)_c->known / _t, _theme->score_good);
        }
        snprintf(_line, sizeof(_line), "%u known, %u practised / %u", _c->known, _c->practised, _c->total);
        kana_stats_text_right(_b, _line, _b->left + _b->width, _y - 22.0f, 13.0f, _theme->text_soft);
    }
    const f32 _ly = _b->top - (f32)KANA_STATS_GROUP_COUNT * KANA_STATS_ROW - 14.0f;
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ _b->left + 6.0f, _ly + 4.0f }, (rde_vec_2F){ 12.0f, 12.0f }, _theme->score_good);
    kana_draw_text(_b->font, _b->font_px, "known", _b->left + 16.0f, _ly, 13.0f, _theme->text_soft);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ _b->left + 76.0f, _ly + 4.0f }, (rde_vec_2F){ 12.0f, 12.0f }, kana_stats_alpha(_theme->button_selected, 0.8f));
    kana_draw_text(_b->font, _b->font_px, "studying", _b->left + 86.0f, _ly, 13.0f, _theme->text_soft);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ _b->left + 162.0f, _ly + 4.0f }, (rde_vec_2F){ 12.0f, 12.0f }, kana_stats_alpha(_theme->button_selected, 0.35f));
    kana_draw_text(_b->font, _b->font_px, "practised", _b->left + 172.0f, _ly, 13.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_stats_draw_mistakes(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    if(_d->squares == 0) {
        kana_draw_text(_b->font, _b->font_px, "No practice yet.", _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
        return;
    }
    c8  _line[80];
    f32 _label = 0.0f;
    for(u32 _m = 0; _m < KANA_STATS_MISTAKE_COUNT; _m++) {
        _label = fmaxf(_label, kana_draw_text_width(_b->font, _b->font_px, KANA_STATS_MISTAKE_NAMES[_m], 15.0f));
    }
    _label += 14.0f;
    const f32 _count = kana_draw_text_width(_b->font, _b->font_px, "100%", 14.0f) + 14.0f;
    const f32 _bar   = fmaxf(40.0f, _b->width - _label - _count);
    for(u32 _m = 0; _m < KANA_STATS_MISTAKE_COUNT; _m++) {
        const f32 _y   = _b->top - (f32)_m * KANA_STATS_ROW;
        const f32 _all = (f32)_d->mistakes[_m] / (f32)_d->squares;
        kana_draw_text(_b->font, _b->font_px, KANA_STATS_MISTAKE_NAMES[_m], _b->left, _y - 22.0f, 15.0f, _theme->text);
        const f32 _x = _b->left + _label;
        kana_stats_bar(_x, _y - 6.0f, _bar, 9.0f, _all, kana_stats_alpha(_theme->score_poor, 0.45f));
        if(_d->squares_recent > 0) {
            kana_stats_bar(_x, _y - 17.0f, _bar, 9.0f, (f32)_d->mistakes_recent[_m] / (f32)_d->squares_recent, _theme->score_poor);
        }
        snprintf(_line, sizeof(_line), "%.0f%%", (f64)(100.0f * _all));
        kana_stats_text_right(_b, _line, _b->left + _b->width, _y - 22.0f, 14.0f, _theme->text_soft);
    }
    snprintf(_line, sizeof(_line), "Squares with each: all time (light), the last %d days (strong)", KANA_STATS_RECENT);
    kana_draw_text(_b->font, _b->font_px, _line, _b->left, _b->top - (f32)KANA_STATS_MISTAKE_COUNT * KANA_STATS_ROW - 14.0f, 13.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_stats_draw_characters(const kana_stats_box* _b) {
    kana_stats*            _stats = _b->stats;
    const kana_stats_data* _d     = &_stats->data;
    const kana_theme*      _theme = kana_theme_active();
    if(_d->weakest_count == 0) {
        kana_draw_text(_b->font, _b->font_px, "No practice yet.", _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
        return;
    }
    c8 _line[16];
    for(u32 _list = 0; _list < 2u; _list++) {
        const f32  _y      = _b->top - (f32)_list * (26.0f + KANA_STATS_GLYPH + 26.0f);
        const u32  _count  = _list == 0 ? _d->weakest_count : _d->improved_count;
        const u32* _recs   = _list == 0 ? _d->weakest : _d->improved;
        kana_draw_text(_b->font, _b->font_px, _list == 0 ? "Weakest (their latest session)" : "Most improved (first session to latest)",
                       _b->left, _y - 16.0f, 15.0f, _theme->text_soft);
        if(_count == 0) {
            kana_draw_text(_b->font, _b->font_px, "Practise a character twice to see it here.", _b->left, _y - 44.0f, 14.0f, _theme->text_soft);
            continue;
        }
        const f32 _step = KANA_STATS_GLYPH + 12.0f;
        for(u32 _i = 0; _i < _count && (f32)(_i + 1u) * _step <= _b->width + 12.0f; _i++) {
            kana_kanji_info _info;
            if(!kana_kanji_at(_stats->db, _recs[_i], &_info)) {
                continue;
            }
            const f32 _x  = _b->left + (f32)_i * _step;
            const f32 _gy = _y - 26.0f;
            kana_glyph_character(&_stats->glyph, _info.codepoint, (rde_vec_2F){ _x, _gy }, KANA_STATS_GLYPH, _theme->ink);
            if(_list == 0) { snprintf(_line, sizeof(_line), "%.0f", (f64)_d->weakest_score[_i]); }
            else           { snprintf(_line, sizeof(_line), "+%.0f", (f64)_d->improved_delta[_i]); }
            kana_draw_text(_b->font, _b->font_px, _line, _x + KANA_STATS_GLYPH * 0.5f - kana_draw_text_width(_b->font, _b->font_px, _line, 14.0f) * 0.5f,
                           _gy - KANA_STATS_GLYPH - 14.0f, 14.0f, _list == 0 ? kana_theme_grade(_d->weakest_score[_i]) : _theme->score_good);
            const kana_stats_hit _hit = { _x, _b->content_y + (_b->top - _gy), KANA_STATS_GLYPH, (u8)_list, _i };
            rde_arr_add(&_stats->hits, &_hit);
        }
    }
}

// Squares written by the hour and by the weekday.
RDE_INTERNAL void kana_stats_draw_when(const kana_stats_box* _b) {
    const kana_stats_data* _d     = &_b->stats->data;
    const kana_theme*      _theme = kana_theme_active();
    if(_d->squares == 0) {
        kana_draw_text(_b->font, _b->font_px, "No practice yet.", _b->left, _b->top - 20.0f, 16.0f, _theme->text_soft);
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
        kana_draw_text(_b->font, _b->font_px, _part == 0 ? "By hour of the day" : "By day of the week", _b->left, _y - 14.0f, 14.0f, _theme->text_soft);
        const f32 _base = _y - 24.0f - _h;
        for(u32 _i = 0; _i < _n; _i++) {
            const f32 _bh = _h * (f32)_values[_i] / (f32)_max;
            const f32 _cx = _b->left + _slot * ((f32)_i + 0.5f);
            if(_bh > 0.5f) {
                rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cx, _base + _bh * 0.5f }, (rde_vec_2F){ _slot * 0.6f, _bh }, kana_stats_alpha(_theme->button_selected, 0.75f));
            }
            c8 _label[4];
            if(_part == 0) {
                if(_i % 6u != 0u) { continue; }
                snprintf(_label, sizeof(_label), "%u", _i);
            } else {
                const c8* _days[7] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
                snprintf(_label, sizeof(_label), "%s", _days[_i]);
            }
            kana_draw_text(_b->font, _b->font_px, _label, _cx - 7.0f, _base - 16.0f, 12.0f, _theme->text_soft);
        }
    }
}

void kana_stats_render(kana_stats* _stats, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_stats->open) {
        return;
    }
    const kana_theme*      _theme  = kana_theme_active();
    const kana_stats_data* _d      = &_stats->data;
    const rde_vec_2I       _size   = rde_window_get_size(_window);
    const rde_vec_4I       _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32              _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_STATS_MARGIN;
    const f32              _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_STATS_MARGIN;

    // The header: since when.
    c8 _line[160];
    kana_draw_text(_font, _font_px, "Statistics", _left, _top - 30.0f, 28.0f, _theme->text);
    if(_d->first_time != 0) {
        const time_t _first = (time_t)_d->first_time;
        c8 _date[40];
        strftime(_date, sizeof(_date), "%d %b %Y", localtime(&_first));
        snprintf(_line, sizeof(_line), "since %s", _date);
        kana_draw_text(_font, _font_px, _line, _left + kana_draw_text_width(_font, _font_px, "Statistics", 28.0f) + 16.0f, _top - 30.0f, 17.0f, _theme->text_soft);
    }
    const f32 _view_top = _top - 50.0f;
    _stats->view_top    = _view_top;
    _stats->view_bottom = _bottom;
    if(_view_top <= _bottom) {
        return;
    }

    // The cards, in columns: each to the shortest one.
    const u32 _cols   = (_right - _left) >= KANA_STATS_TWO_COLS ? 2u : 1u;
    const f32 _cw     = ((_right - _left) - KANA_STATS_GAP * (f32)(_cols - 1u)) / (f32)_cols;
    f32       _col_y[2] = { 0.0f, 0.0f };
    rde_arr_clear(&_stats->hits);
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_view_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_view_top - _bottom) });
    for(u32 _c = 0; _c < KANA_STATS_CARD_COUNT; _c++) {
        const u32 _col = _cols == 2u && _col_y[1] < _col_y[0] ? 1u : 0u;
        const f32 _h   = kana_stats_card_height(_stats, (KANA_STATS_CARD_)_c, _cw);
        const f32 _x   = _left + (f32)_col * (_cw + KANA_STATS_GAP);
        const f32 _cy  = _col_y[_col];   // content y of its top
        _col_y[_col]  += _h + KANA_STATS_GAP;
        const f32 _y   = _view_top - (_cy - _stats->scroller.offset);   // its top on screen
        if(_y - _h > _view_top || _y < _bottom) {
            continue;
        }
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _cw * 0.5f, _y - _h * 0.5f }, (rde_vec_2F){ _cw, _h }, 24.0f / fminf(_cw, _h), 8,
                                                _theme->sheet, NULL);
        kana_draw_text(_font, _font_px, KANA_STATS_CARD_TITLES[_c], _x + KANA_STATS_PAD, _y - KANA_STATS_PAD - 20.0f, 20.0f, _theme->text);
        const kana_stats_box _box = {
            .stats = _stats, .font = _font, .font_px = _font_px, .left = _x + KANA_STATS_PAD, .width = _cw - 2.0f * KANA_STATS_PAD,
            .top = _y - KANA_STATS_PAD - KANA_STATS_TITLE, .content_y = _cy + KANA_STATS_PAD + KANA_STATS_TITLE
        };
        switch((KANA_STATS_CARD_)_c) {
            case KANA_STATS_CARD_OVERVIEW:   kana_stats_draw_overview(&_box);   break;
            case KANA_STATS_CARD_ACTIVITY:   kana_stats_draw_activity(&_box);   break;
            case KANA_STATS_CARD_SCORES:     kana_stats_draw_scores(&_box);     break;
            case KANA_STATS_CARD_EXAMS:      kana_stats_draw_exams(&_box);      break;
            case KANA_STATS_CARD_COVERAGE:   kana_stats_draw_coverage(&_box);   break;
            case KANA_STATS_CARD_MISTAKES:   kana_stats_draw_mistakes(&_box);   break;
            case KANA_STATS_CARD_CHARACTERS: kana_stats_draw_characters(&_box); break;
            case KANA_STATS_CARD_WHEN:       kana_stats_draw_when(&_box);       break;
            default: break;
        }
    }
    rde_rendering_end_clipping_rect();
    _stats->content_height = fmaxf(_col_y[0], _col_y[1]);
}
