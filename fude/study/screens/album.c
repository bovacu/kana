#include "study/screens/album.h"
#include "study/widgets/header.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "study/screens/exam.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See album.h.
// ===========================================================================

#define FUDE_ALBUM_MARGIN      16.0f
#define FUDE_ALBUM_CELL_MIN    96.0f     // overview: the grid fits as many columns as this allows
#define FUDE_ALBUM_TITLE_H     72.0f     // overview: "Album" and the counts
#define FUDE_ALBUM_GLYPH       128.0f    // page: the character
#define FUDE_ALBUM_HEAD_H      168.0f    // page: the character, its summary and trend
#define FUDE_ALBUM_TREND_W     420.0f
#define FUDE_ALBUM_TREND_H     60.0f
#define FUDE_ALBUM_THUMB       104.0f    // an attempt
#define FUDE_ALBUM_THUMB_GAP   10.0f
#define FUDE_ALBUM_SESSION_H   36.0f     // a session's date line
#define FUDE_ALBUM_LABEL_H     24.0f     // the score under an attempt
#define FUDE_ALBUM_DETAIL_H    30.0f     // the replayed attempt's line
#define FUDE_ALBUM_SESSION_GAP 18.0f
#define FUDE_ALBUM_STROKE_GAP  0.25      // replay: seconds between strokes
#define FUDE_ALBUM_EXAM_PAD    14.0f     // exams view: inside an exam's card
#define FUDE_ALBUM_EXAM_GLYPH  26.0f     // ...a character asked
#define FUDE_ALBUM_EXAM_STEP   34.0f     // ...and the room it takes
#define FUDE_ALBUM_EXAM_GAP    12.0f     // ...between cards

void fude_album_init(fude_album* _album, const fude_kanji_db* _db) {
    memset(_album, 0, sizeof(*_album));
    _album->db              = _db;
    _album->selected        = -1;
    _album->selected_answer = -1;
    _album->tapped_exam     = -1;
    _album->sort            = FUDE_ALBUM_SORT_WEAKEST;
    _album->view            = FUDE_ALBUM_VIEW_CHARACTERS;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _album->entries           = rde_arr_new(sizeof(fude_album_entry), _heap);
    _album->hits              = rde_arr_new(sizeof(fude_album_hit), _heap);
    _album->_points           = rde_arr_new(sizeof(rde_vec_2F), _heap);
    _album->page_exams        = rde_arr_new(sizeof(fude_examlog_writing), _heap);
    _album->page_exam_strokes = rde_arr_new(sizeof(fude_history_stroke), _heap);
    _album->page_exam_points  = rde_arr_new(sizeof(fude_history_point), _heap);
    fude_history_init(&_album->history);
    fude_glyph_init(&_album->glyph, _db);
}

void fude_album_destroy(fude_album* _album) {
    rde_arr* _arrays[] = { &_album->entries, &_album->hits, &_album->_points, &_album->page_exams, &_album->page_exam_strokes, &_album->page_exam_points };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_history_destroy(&_album->history);
    fude_glyph_destroy(&_album->glyph);
    memset(_album, 0, sizeof(*_album));
}

// --- the overview's list --------------------------------------------------------------

RDE_INTERNAL i32 fude_album_by_codepoint(const fude_album_entry* _a, const fude_album_entry* _b) {
    return _a->codepoint < _b->codepoint ? -1 : _a->codepoint > _b->codepoint ? 1 : 0;
}

RDE_INTERNAL i32 fude_album_by_recent(const fude_album_entry* _a, const fude_album_entry* _b) {
    if(_a->summary.last_time != _b->summary.last_time) {
        return _a->summary.last_time > _b->summary.last_time ? -1 : 1;
    }
    return fude_album_by_codepoint(_a, _b);
}

RDE_INTERNAL int fude_album_compare_weakest(const void* _a, const void* _b) {
    const fude_album_entry* _x = (const fude_album_entry*)_a;
    const fude_album_entry* _y = (const fude_album_entry*)_b;
    if(_x->summary.last < _y->summary.last) { return -1; }
    if(_x->summary.last > _y->summary.last) { return  1; }
    return fude_album_by_recent(_x, _y);
}

RDE_INTERNAL int fude_album_compare_recent(const void* _a, const void* _b) {
    return fude_album_by_recent((const fude_album_entry*)_a, (const fude_album_entry*)_b);
}

RDE_INTERNAL int fude_album_compare_most(const void* _a, const void* _b) {
    const fude_album_entry* _x = (const fude_album_entry*)_a;
    const fude_album_entry* _y = (const fude_album_entry*)_b;
    if(_x->summary.sessions != _y->summary.sessions) {
        return _x->summary.sessions > _y->summary.sessions ? -1 : 1;
    }
    return fude_album_by_recent(_x, _y);
}

RDE_INTERNAL void fude_album_sort(fude_album* _album) {
    int (*_compare[FUDE_ALBUM_SORT_COUNT])(const void*, const void*) = { fude_album_compare_weakest, fude_album_compare_recent, fude_album_compare_most };
    if(rde_arr_length(&_album->entries) > 1) {
        qsort(_album->entries.memory, rde_arr_length(&_album->entries), sizeof(fude_album_entry), _compare[_album->sort]);
    }
}

// Every character with a history the data knows, summarized.
RDE_INTERNAL void fude_album_load(fude_album* _album) {
    rde_arr_clear(&_album->entries);
    _album->sessions        = 0;
    _album->loaded          = true;
    _album->loaded_revision = fude_history_revision();
    if(_album->db == NULL) {
        return;
    }

    rde_arr _codepoints = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_history_list(&_codepoints);

    for(u32 _i = 0; _i < (u32)rde_arr_length(&_codepoints); _i++) {
        fude_album_entry _entry;
        memset(&_entry, 0, sizeof(_entry));
        _entry.codepoint = ((const u32*)_codepoints.memory)[_i];
        if(fude_kanji_find_index(_album->db, _entry.codepoint, &_entry.record) && fude_history_summarize(_entry.codepoint, &_entry.summary)) {
            rde_arr_add(&_album->entries, &_entry);
            _album->sessions += _entry.summary.sessions;
        }
    }

    rde_arr_free(&_codepoints);
    fude_album_sort(_album);
}

u32 fude_album_weakest(const fude_album* _album, u32* _out, u32 _max) {
    // A copy in weakest order, whatever order the overview shows.
    const u32 _count = (u32)rde_arr_length(&_album->entries);
    if(_count == 0 || _max == 0) {
        return 0;
    }
    fude_album_entry* _sorted = (fude_album_entry*)rde_malloc(sizeof(fude_album_entry) * _count);
    memcpy(_sorted, _album->entries.memory, sizeof(fude_album_entry) * _count);
    qsort(_sorted, _count, sizeof(fude_album_entry), fude_album_compare_weakest);

    const u32 _n = _count < _max ? _count : _max;
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = _sorted[_i].record;
    }
    rde_free(_sorted);
    return _n;
}

// --- opening and closing ------------------------------------------------------------

void fude_album_open(fude_album* _album) {
    _album->open            = true;
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    _album->tapped_exam     = -1;
    fude_scroller_stop(&_album->scroller);
    _album->scroller.offset = 0.0f;
    fude_album_load(_album);
}

void fude_album_close(fude_album* _album) {
    _album->open            = false;
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    fude_scroller_stop(&_album->scroller);
    fude_scroller_stop(&_album->page_scroller);
}

void fude_album_set_sort(fude_album* _album, FUDE_ALBUM_SORT_ _sort) {
    _album->sort = _sort;
    _album->view = FUDE_ALBUM_VIEW_CHARACTERS;
    fude_album_sort(_album);
    fude_scroller_stop(&_album->scroller);
    _album->scroller.offset = 0.0f;
}

void fude_album_set_view(fude_album* _album, FUDE_ALBUM_VIEW_ _view) {
    if(_album->view == _view) {
        return;
    }
    _album->view        = _view;
    _album->tapped_exam = -1;
    fude_scroller_stop(&_album->scroller);
    _album->scroller.offset = 0.0f;
}

b8 fude_album_take_exam(fude_album* _album, u32* _exam) {
    if(_album->tapped_exam < 0) {
        return false;
    }
    *_exam              = (u32)_album->tapped_exam;
    _album->tapped_exam = -1;
    return true;
}

// The page's character in the exams: every answer to it, with its writing.
RDE_INTERNAL void fude_album_load_page_exams(fude_album* _album) {
    rde_arr_clear(&_album->page_exams);
    rde_arr_clear(&_album->page_exam_strokes);
    rde_arr_clear(&_album->page_exam_points);
    _album->exams_revision = fude_examlog_revision();
    fude_examlog_read_writing(UINT32_MAX, _album->page_codepoint, &_album->page_exams, &_album->page_exam_strokes, &_album->page_exam_points);
}

void fude_album_open_page(fude_album* _album, u32 _codepoint) {
    _album->page_codepoint = _codepoint;
    _album->page_record    = 0;
    if(_album->db != NULL) {
        fude_kanji_find_index(_album->db, _codepoint, &_album->page_record);
    }
    fude_history_load(&_album->history, _codepoint);
    fude_album_load_page_exams(_album);
    _album->page_open       = true;
    _album->selected        = -1;
    _album->selected_answer = -1;
    fude_scroller_stop(&_album->page_scroller);
    _album->page_scroller.offset = 0.0f;
}

void fude_album_close_page(fude_album* _album) {
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    fude_scroller_stop(&_album->page_scroller);
}

// --- input ---------------------------------------------------------------------------

RDE_INTERNAL fude_scroller* fude_album_scroller(fude_album* _album) {
    return _album->page_open ? &_album->page_scroller : &_album->scroller;
}

void fude_album_pointer_down(fude_album* _album, rde_vec_2F _screen, f64 _time) {
    fude_scroller_down(fude_album_scroller(_album), _screen, _time);
}

void fude_album_pointer_moved(fude_album* _album, rde_vec_2F _screen, f64 _time) {
    fude_scroller_moved(fude_album_scroller(_album), _screen, _time);
}

void fude_album_pointer_up(fude_album* _album, f64 _time) {
    fude_scroller* _scroller = fude_album_scroller(_album);
    fude_scroller_up(_scroller, _time);

    rde_vec_2F _at;
    if(!fude_scroller_take_tap(_scroller, &_at) || _at.y > _album->view_top || _at.y < _album->view_bottom) {
        return;
    }

    const f32             _content_y = _album->view_top - _at.y + _scroller->offset;
    const fude_album_hit* _hits      = (const fude_album_hit*)_album->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_album->hits); _i++) {
        const fude_album_hit* _h = &_hits[_i];
        if(_at.x < _h->x || _at.x >= _h->x + _h->w || _content_y < _h->y || _content_y >= _h->y + _h->h) {
            continue;
        }

        if(_album->page_open) {
            // An attempt: replay it (again, if it already was).
            const b8 _answer        = (_h->index & FUDE_ALBUM_EXAM) != 0u;
            _album->selected        = _answer ? -1 : (i32)_h->index;
            _album->selected_answer = _answer ? (i32)(_h->index & ~FUDE_ALBUM_EXAM) : -1;
            _album->replay_start    = rde_engine_get_time_now();
        } else if(_album->view == FUDE_ALBUM_VIEW_EXAMS) {
            _album->tapped_exam = (i32)_h->index;
        } else if(_h->index < (u32)rde_arr_length(&_album->entries)) {
            fude_album_open_page(_album, ((const fude_album_entry*)_album->entries.memory)[_h->index].codepoint);
        }
        return;
    }
}

void fude_album_update(fude_album* _album, f32 _dt) {
    if(!_album->open) {
        return;
    }

    // A session was saved (Practice, opened from a page): read it all again.
    if(!_album->loaded || _album->loaded_revision != fude_history_revision()) {
        fude_album_load(_album);
        if(_album->page_open) {
            fude_history_load(&_album->history, _album->page_codepoint);
            _album->selected        = -1;
            _album->selected_answer = -1;
        }
    }
    // An exam was taken (a kept one's Retry wrong): its answers too.
    if(_album->page_open && _album->exams_revision != fude_examlog_revision()) {
        fude_album_load_page_exams(_album);
        _album->selected        = -1;
        _album->selected_answer = -1;
    }

    fude_scroller_update(fude_album_scroller(_album), _dt, _album->content_height, _album->view_top - _album->view_bottom);
}

// --- drawing helpers --------------------------------------------------------------------

// A time as local date (and weekday and clock time).
RDE_INTERNAL void fude_album_date(u64 _time, b8 _with_clock, c8* _out, usize _size) {
    if(_with_clock) {
        fude_text_date_time(_out, _size, _time);
    } else {
        fude_text_date(_out, _size, _time);
    }
}

// One attempt as it was drawn — strokes [_first, +_count) of _strokes — in the
// square at _tl (_size wide). _elapsed < 0 draws it whole; otherwise as far as a
// replay has got after _elapsed seconds — each stroke at the speed it was
// written, a pause between strokes — with the pen's tip where it is.
RDE_INTERNAL void fude_album_draw_attempt(fude_album* _album, const fude_history_stroke* _strokes, const fude_history_point* _points, u32 _first, u32 _count,
                                          rde_vec_2F _tl, f32 _size, f64 _elapsed) {
    const fude_theme* _theme = fude_theme_active();

    // The practice pen's width: the model's own stroke width, as a part of the square.
    const f32        _radius = fmaxf(0.8f, _size * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f);
    const rde_vec_2F _bl     = { _tl.x, _tl.y - _size };
    const f32        _k      = _size / 65535.0f;
    f64              _clock  = _elapsed;

    for(u32 _s = 0; _s < _count; _s++) {
        const fude_history_stroke* _stroke = &_strokes[_first + _s];
        if(_stroke->point_count == 0) {
            continue;
        }
        const fude_history_point* _p        = &_points[_stroke->first_point];
        const f32                 _duration = _p[_stroke->point_count - 1].time;

        u32 _shown   = _stroke->point_count;
        b8  _partial = false;
        if(_elapsed >= 0.0) {
            if(_clock < 0.0) {
                return;   // not reached yet
            }
            if(_clock < (f64)_duration) {
                _shown = 0;
                while(_shown < _stroke->point_count && (f64)_p[_shown].time <= _clock) {
                    _shown++;
                }
                _partial = true;
            }
        }
        if(_shown == 0) {
            return;
        }

        rde_arr_clear(&_album->_points);
        rde_vec_2F* _pos = (rde_vec_2F*)rde_arr_add_n(&_album->_points, _shown);
        for(u32 _q = 0; _q < _shown; _q++) {
            _pos[_q] = (rde_vec_2F){ _bl.x + (f32)_p[_q].x * _k, _bl.y + (f32)_p[_q].y * _k };
        }
        fude_draw_stroke_even(_pos, _shown, _radius, _theme->ink);

        if(_partial) {
            rde_rendering_2d_draw_circle(fude_draw_at(_pos[_shown - 1]), _radius * 1.35f, 16u, _theme->pen_tip, NULL);
            return;
        }
        _clock -= (f64)_duration + FUDE_ALBUM_STROKE_GAP;
    }
}

// Every session's average, oldest to newest, in the box under _tl.
RDE_INTERNAL void fude_album_trend(fude_album* _album, rde_vec_2F _tl, f32 _w, f32 _h) {
    const fude_history_session* _sessions = (const fude_history_session*)_album->history.sessions.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    const fude_theme*           _theme    = fude_theme_active();
    if(_count == 0 || _w <= 0.0f) {
        return;
    }

    // The grade lines, faint: what "fair" and "good" start at.
    const f32 _levels[2] = { FUDE_THEME_GRADE_FAIR, FUDE_THEME_GRADE_GOOD };
    for(u32 _i = 0; _i < 2; _i++) {
        const f32 _y = _tl.y - _h + _h * _levels[_i] / 100.0f;
        fude_draw_line((rde_vec_2F){ _tl.x, _y }, (rde_vec_2F){ _tl.x + _w, _y }, 0.5f, _theme->line);
    }

    rde_arr_clear(&_album->_points);
    rde_vec_2F* _pos = (rde_vec_2F*)rde_arr_add_n(&_album->_points, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x = _count == 1 ? _tl.x + _w * 0.5f : _tl.x + _w * (f32)_i / (f32)(_count - 1u);
        _pos[_i] = (rde_vec_2F){ _x, _tl.y - _h + _h * rde_math_clamp_f32(_sessions[_i].average, 0.0f, 100.0f) / 100.0f };
    }
    if(_count > 1) {
        fude_draw_stroke_even(_pos, _count, 1.2f, _theme->text_soft);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const rde_vec_2F _at = ((const rde_vec_2F*)_album->_points.memory)[_i];
        rde_rendering_2d_draw_circle(fude_draw_at(_at), _count > 40 ? 2.0f : 3.5f, 12u, fude_theme_grade(_sessions[_i].average), NULL);
    }
}

// --- the overview ----------------------------------------------------------------------

RDE_INTERNAL void fude_album_render_overview(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme  = fude_theme_active();
    const f32         _scroll = _album->scroller.offset;
    const u32         _count  = (u32)rde_arr_length(&_album->entries);
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_ALBUM), _left, FUDE_ALBUM_SY(0.0f) - 2.0f);
    c8 _line[128];
    if(_count == 0) {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_ALBUM_EMPTY));
    } else {
        c8 _chars[48], _sessions_n[48];
        FUDE_TEXTF(_chars, FUDE_TEXT_CHARACTERS_N, FUDE_TN(_count));
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_album->sessions));
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_COUNTS, FUDE_TS(_chars), FUDE_TS(_sessions_n));
    }
    fude_draw_text_wrap(_font, _font_px, _line, _left, FUDE_ALBUM_SY(0.0f) - 60.0f, 17.0f, _right - _left, 23.0f, _theme->text_soft);   // the empty one's two lines on a narrow screen

    const f32 _width   = _right - _left;
    const u32 _columns = (u32)fmaxf(1.0f, floorf(_width / FUDE_ALBUM_CELL_MIN));
    const f32 _cell    = _width / (f32)_columns;
    const f32 _glyph   = _cell * 0.55f;
    const u32 _rows    = (_count + _columns - 1u) / _columns;
    _album->content_height = FUDE_ALBUM_TITLE_H + (f32)_rows * _cell + FUDE_ALBUM_MARGIN;

    const fude_album_entry* _entries = (const fude_album_entry*)_album->entries.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x  = _left + (f32)(_i % _columns) * _cell;
        const f32 _cy = FUDE_ALBUM_TITLE_H + (f32)(_i / _columns) * _cell;   // the cell's top, content space
        const f32 _y  = FUDE_ALBUM_SY(_cy);
        if(_y - _cell > _top || _y < _bottom) {
            continue;
        }

        const fude_album_hit _hit = { _x, _cy, _cell, _cell, _i };
        rde_arr_add(&_album->hits, &_hit);

        const fude_album_entry* _e = &_entries[_i];
        fude_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, _theme->line);
        fude_glyph_character(&_album->glyph, _e->codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.06f }, _glyph, _theme->ink);

        snprintf(_line, sizeof(_line), "%.0f", (f64)_e->summary.last);
        fude_draw_text(_font, _font_px, _line, _x + 8.0f, _y - _cell + 10.0f, 17.0f, fude_theme_grade(_e->summary.last));
        snprintf(_line, sizeof(_line), "\xC3\x97%u", _e->summary.sessions);   // ×n
        fude_draw_text(_font, _font_px, _line, _x + _cell * 0.58f, _y - _cell + 10.0f, 15.0f, _theme->text_soft);
    }

    #undef FUDE_ALBUM_SY
}

// --- the exams ------------------------------------------------------------------------------

// The exams view: every exam, newest first, a card each — what it was of, when,
// passed or not, how many right and the points, and each character asked (green
// when right, red when not). A tap: the exam's results (exam.h).
RDE_INTERNAL void fude_album_render_exams(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*        _theme  = fude_theme_active();
    const f32                _scroll = _album->scroller.offset;
    const u32                _count  = fude_examlog_count();
    const fude_examlog_exam* _exams  = fude_examlog_exams();
    const fude_examlog_item* _items  = fude_examlog_items();
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_ALBUM), _left, FUDE_ALBUM_SY(0.0f) - 2.0f);
    c8  _line[160];
    u32 _passed = 0;
    for(u32 _e = 0; _e < _count; _e++) {
        _passed += (f32)_exams[_e].correct >= FUDE_EXAM_PASS * (f32)_exams[_e].item_count ? 1u : 0u;
    }
    if(_count == 0) {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_STATS_NO_EXAMS));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_EXAMS_COUNTS, FUDE_TN(_count), FUDE_TN(_passed));
    }
    fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(0.0f) - 60.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 17.0f, _right - _left, 0.6f),
                   _theme->text_soft);

    const f32 _width    = _right - _left;
    const u32 _per_row  = (u32)fmaxf(1.0f, floorf((_width - 2.0f * FUDE_ALBUM_EXAM_PAD) / FUDE_ALBUM_EXAM_STEP));
    f32       _y        = FUDE_ALBUM_TITLE_H;
    for(u32 _n = 0; _n < _count; _n++) {
        const u32                _e    = _count - 1u - _n;
        const fude_examlog_exam* _exam = &_exams[_e];
        const u32                _rows = (_exam->item_count + _per_row - 1u) / _per_row;
        const f32                _h    = 2.0f * FUDE_ALBUM_EXAM_PAD + 52.0f + (f32)_rows * FUDE_ALBUM_EXAM_STEP;
        const f32                _sy   = FUDE_ALBUM_SY(_y);
        if(!(_sy - _h > _top || _sy < _bottom)) {
            const fude_album_hit _hit = { _left, _y, _width, _h, _e };
            rde_arr_add(&_album->hits, &_hit);
            fude_draw_card((rde_vec_2F){ _left, _sy - _h }, (rde_vec_2F){ _right, _sy }, 14.0f, _theme->surface, _theme->outline);

            // What it was of and when; passed or not, at the right.
            const f32 _x       = _left + FUDE_ALBUM_EXAM_PAD;
            const f32 _base    = _sy - FUDE_ALBUM_EXAM_PAD - 18.0f;
            const b8  _ok      = (f32)_exam->correct >= FUDE_EXAM_PASS * (f32)_exam->item_count;
            const c8* _verdict = fude_text(_ok ? FUDE_TEXT_EXAM_PASSED : FUDE_TEXT_EXAM_NOT_PASSED);
            const f32 _chip_w  = fude_draw_text_width(_font, _font_px, _verdict, 12.0f) + 22.0f;
            fude_draw_chip(_font, _font_px, _verdict, _right - FUDE_ALBUM_EXAM_PAD - _chip_w, _base + 6.0f, 12.0f, _ok ? _theme->score_good : _theme->score_poor,
                           _theme->on_accent);
            c8 _date[96];
            fude_text_date_time(_date, sizeof(_date), _exam->time);
            FUDE_TEXTF(_line, FUDE_TEXT_EXAM_KEPT, FUDE_TS(fude_exam_source_name((FUDE_EXAM_SOURCE_)_exam->source)), FUDE_TS(_date));
            fude_draw_text(_font, _font_px, _line, _x, _base,
                           fude_draw_text_px_to_fit(_font, _font_px, _line, 18.0f, _width - 2.0f * FUDE_ALBUM_EXAM_PAD - _chip_w - 12.0f, 0.6f), _theme->text);
            FUDE_TEXTF(_line, FUDE_TEXT_EXAM_RESULT, FUDE_TN(_exam->correct), FUDE_TN(_exam->item_count), FUDE_TN(lroundf(_exam->score)));
            fude_draw_text(_font, _font_px, _line, _x, _base - 24.0f, 15.0f, _theme->text_soft);

            // Each character asked.
            for(u32 _i = 0; _i < _exam->item_count; _i++) {
                const fude_examlog_item* _it = &_items[_exam->first_item + _i];
                const rde_vec_2F         _tl = { _x + (f32)(_i % _per_row) * FUDE_ALBUM_EXAM_STEP,
                                                 _base - 40.0f - (f32)(_i / _per_row) * FUDE_ALBUM_EXAM_STEP };
                if(_tl.y - FUDE_ALBUM_EXAM_GLYPH > _top || _tl.y < _bottom) {
                    continue;
                }
                fude_glyph_character(&_album->glyph, _it->codepoint, _tl, FUDE_ALBUM_EXAM_GLYPH, _it->correct ? _theme->score_good : _theme->score_poor);
            }
        }
        _y += _h + FUDE_ALBUM_EXAM_GAP;
    }
    _album->content_height = _y + FUDE_ALBUM_MARGIN;
    #undef FUDE_ALBUM_SY
}

// --- a character's page ------------------------------------------------------------------

// One exam answer on the page, from content y _y: right or wrong, which exam and
// when, then the writing (over the model, faint) and its points or what it read
// as. Where the next thing goes.
RDE_INTERNAL f32 fude_album_render_answer(fude_album* _album, rde_font* _font, f32 _font_px, u32 _index, f32 _left, f32 _top, f32 _bottom, f32 _y, f32 _thumb,
                                          f64 _now) {
    const fude_theme*           _theme  = fude_theme_active();
    const f32                   _scroll = _album->page_scroller.offset;
    const fude_examlog_writing* _answer = &((const fude_examlog_writing*)_album->page_exams.memory)[_index];
    const fude_examlog_exam*    _exam   = &fude_examlog_exams()[_answer->exam];
    const fude_examlog_item*    _item   = &fude_examlog_items()[_exam->first_item + _answer->item];
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    c8        _title[64], _date[96], _line[32];
    const f32 _base = FUDE_ALBUM_SY(_y) - 24.0f;
    fude_draw_verdict((rde_vec_2F){ _left + 11.0f, _base + 6.0f }, 11.0f, _item->correct);
    FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name((FUDE_EXAM_SOURCE_)_exam->source)));
    fude_draw_text(_font, _font_px, _title, _left + 30.0f, _base, 17.0f, _theme->text);
    fude_text_date_time(_date, sizeof(_date), _exam->time);
    fude_draw_text(_font, _font_px, _date, _left + 30.0f + fude_draw_text_width(_font, _font_px, _title, 17.0f) + 12.0f, _base, 17.0f, _theme->text_soft);
    _y += FUDE_ALBUM_SESSION_H;

    const rde_vec_2F _tl          = { _left, FUDE_ALBUM_SY(_y) };
    const b8         _is_selected = _album->selected_answer == (i32)_index;
    if(!(_tl.y - _thumb - FUDE_ALBUM_LABEL_H > _top || _tl.y < _bottom)) {
        const fude_album_hit _hit = { _tl.x, _y, _thumb, _thumb + FUDE_ALBUM_LABEL_H, FUDE_ALBUM_EXAM | _index };
        rde_arr_add(&_album->hits, &_hit);
        fude_glyph_box(_tl, _thumb);
        fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _thumb }, (rde_vec_2F){ _tl.x + _thumb, _tl.y }, false);   // the writing as written (draw.h)
        fude_glyph_character(&_album->glyph, _album->page_codepoint, _tl, _thumb, _theme->reference);
        fude_album_draw_attempt(_album, (const fude_history_stroke*)_album->page_exam_strokes.memory, (const fude_history_point*)_album->page_exam_points.memory,
                                _answer->first_stroke, _answer->stroke_count, _tl, _thumb, _is_selected ? _now - _album->replay_start : -1.0);
        fude_draw_keep_end();
        if(_is_selected) {
            fude_draw_outline((rde_vec_2F){ _tl.x - 3.0f, _tl.y - _thumb - 3.0f }, (rde_vec_2F){ _tl.x + _thumb + 3.0f, _tl.y + 3.0f }, 1.5f, _theme->select);
        }
        if(_item->correct) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_item->score);
            fude_draw_text(_font, _font_px, _line, _tl.x + 2.0f, _tl.y - _thumb - 19.0f, 15.0f, fude_theme_grade(_item->score));
        } else if(_item->read_as != 0 && _item->read_as != _album->page_codepoint) {
            const c8* _read = fude_text(FUDE_TEXT_EXAM_READ_AS);
            fude_draw_text(_font, _font_px, _read, _tl.x + 2.0f, _tl.y - _thumb - 18.0f, 13.0f, _theme->text_soft);
            fude_glyph_character(&_album->glyph, _item->read_as, (rde_vec_2F){ _tl.x + 8.0f + fude_draw_text_width(_font, _font_px, _read, 13.0f), _tl.y - _thumb - 3.0f },
                                 18.0f, _theme->score_poor);
        }
    }
    #undef FUDE_ALBUM_SY
    return _y + _thumb + FUDE_ALBUM_LABEL_H + FUDE_ALBUM_THUMB_GAP + FUDE_ALBUM_SESSION_GAP;
}

RDE_INTERNAL void fude_album_render_page(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*           _theme    = fude_theme_active();
    const f32                   _scroll   = _album->page_scroller.offset;
    const fude_history_session* _sessions = (const fude_history_session*)_album->history.sessions.memory;
    const fude_history_square*  _squares  = (const fude_history_square*)_album->history.squares.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    c8 _line[160];
    c8 _date[64];

    // --- the character, its summary, its trend ---------------------------------------
    const f32 _head = FUDE_ALBUM_SY(8.0f);
    fude_glyph_character(&_album->glyph, _album->page_codepoint, (rde_vec_2F){ _left, _head }, FUDE_ALBUM_GLYPH, _theme->ink);

    const f32 _tx = _left + FUDE_ALBUM_GLYPH + 28.0f;
    if(_count > 0) {
        f32 _best = 0.0f;
        for(u32 _i = 0; _i < _count; _i++) {
            _best = fmaxf(_best, _sessions[_i].average);
        }
        fude_album_date(_sessions[0].time, false, _date, sizeof(_date));
        c8 _sessions_n[48];
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_count));
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_SINCE, FUDE_TS(_sessions_n), FUDE_TS(_date));
        fude_draw_text(_font, _font_px, _line, _tx, _head - 24.0f, 22.0f, _theme->text);
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_FIRST_BEST_LAST, FUDE_TN(lroundf(_sessions[0].average)), FUDE_TN(lroundf(_best)), FUDE_TN(lroundf(_sessions[_count - 1].average)));
        fude_draw_text(_font, _font_px, _line, _tx, _head - 52.0f, 17.0f, _theme->text_soft);
        fude_album_trend(_album, (rde_vec_2F){ _tx, _head - 70.0f }, fminf(FUDE_ALBUM_TREND_W, _right - _tx), FUDE_ALBUM_TREND_H);
    } else {
        fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_ALBUM_NO_SESSIONS), _tx, _head - 24.0f, 22.0f, _theme->text);
    }

    // --- the sessions, newest first ----------------------------------------------------
    const f32 _width = _right - _left;
    const u32 _cols  = (u32)fmaxf(1.0f, floorf((_width + FUDE_ALBUM_THUMB_GAP) / (FUDE_ALBUM_THUMB + FUDE_ALBUM_THUMB_GAP)));
    const f32 _thumb = fminf(FUDE_ALBUM_THUMB, (_width - FUDE_ALBUM_THUMB_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    const f64 _now   = rde_engine_get_time_now();
    f32       _y     = FUDE_ALBUM_HEAD_H;

    // Sessions and exam answers, the newer of the next two each time.
    const fude_examlog_writing* _answers  = (const fude_examlog_writing*)_album->page_exams.memory;
    const fude_examlog_exam*    _exams    = fude_examlog_exams();
    u32                         _ns       = _count;                                   // still to show, the newest last
    u32                         _na       = (u32)rde_arr_length(&_album->page_exams);
    while(_ns > 0 || _na > 0) {
        if(_na > 0 && (_ns == 0 || _exams[_answers[_na - 1u].exam].time >= _sessions[_ns - 1u].time)) {
            _na--;
            _y = fude_album_render_answer(_album, _font, _font_px, _na, _left, _top, _bottom, _y, _thumb, _now);
            continue;
        }
        const fude_history_session* _session = &_sessions[--_ns];

        // Its average, then when.
        snprintf(_line, sizeof(_line), "%.0f", (f64)_session->average);
        fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(_y) - 24.0f, 20.0f, fude_theme_grade(_session->average));
        fude_album_date(_session->time, true, _date, sizeof(_date));
        fude_draw_text(_font, _font_px, _date, _left + 44.0f, FUDE_ALBUM_SY(_y) - 24.0f, 17.0f, _theme->text_soft);
        _y += FUDE_ALBUM_SESSION_H;

        // Its attempts — the squares written in — in rows.
        u32 _shown    = 0;
        i32 _selected = -1;   // this session's replayed attempt, as its number among the shown
        for(u32 _q = 0; _q < _session->square_count; _q++) {
            const u32                  _index  = _session->first_square + _q;
            const fude_history_square* _square = &_squares[_index];
            if(_square->score.empty) {
                continue;
            }

            const f32        _cy = _y + (f32)(_shown / _cols) * (_thumb + FUDE_ALBUM_LABEL_H + FUDE_ALBUM_THUMB_GAP);
            const rde_vec_2F _tl = { _left + (f32)(_shown % _cols) * (_thumb + FUDE_ALBUM_THUMB_GAP), FUDE_ALBUM_SY(_cy) };
            const b8         _is_selected = (i32)_index == _album->selected;
            if(_is_selected) {
                _selected = (i32)_shown;
            }
            _shown++;

            if(_tl.y - _thumb - FUDE_ALBUM_LABEL_H > _top || _tl.y < _bottom) {
                continue;
            }
            const fude_album_hit _hit = { _tl.x, _cy, _thumb, _thumb + FUDE_ALBUM_LABEL_H, _index };
            rde_arr_add(&_album->hits, &_hit);

            fude_glyph_box(_tl, _thumb);
            fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _thumb }, (rde_vec_2F){ _tl.x + _thumb, _tl.y }, false);   // the writing as written (draw.h)
            fude_glyph_character(&_album->glyph, _album->page_codepoint, _tl, _thumb, _theme->reference);
            fude_album_draw_attempt(_album, (const fude_history_stroke*)_album->history.strokes.memory, (const fude_history_point*)_album->history.points.memory,
                                    _square->first_stroke, _square->stroke_count, _tl, _thumb, _is_selected ? _now - _album->replay_start : -1.0);
            fude_draw_keep_end();
            if(_is_selected) {
                fude_draw_outline((rde_vec_2F){ _tl.x - 3.0f, _tl.y - _thumb - 3.0f }, (rde_vec_2F){ _tl.x + _thumb + 3.0f, _tl.y + 3.0f }, 1.5f, _theme->select);
            }

            snprintf(_line, sizeof(_line), "%.0f", (f64)_square->score.score);
            fude_draw_text(_font, _font_px, _line, _tl.x + 2.0f, _tl.y - _thumb - 19.0f, 15.0f, fude_theme_grade(_square->score.score));
        }
        const u32 _rows = (_shown + _cols - 1u) / _cols;
        _y += (f32)_rows * (_thumb + FUDE_ALBUM_LABEL_H + FUDE_ALBUM_THUMB_GAP);

        // The replayed attempt, in words.
        if(_selected >= 0) {
            const fude_history_square* _square = &_squares[_album->selected];
            FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_ATTEMPT, FUDE_TN(_selected + 1), FUDE_TN(lroundf(_square->score.score)), FUDE_TS(_square->score.feedback));
            fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(_y) - 18.0f, 16.0f, _theme->text);
            _y += FUDE_ALBUM_DETAIL_H;
        }
        _y += FUDE_ALBUM_SESSION_GAP;
    }

    _album->content_height = _y;
    #undef FUDE_ALBUM_SY
}

void fude_album_render(fude_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_album->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_ALBUM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_ALBUM_MARGIN;

    _album->view_top    = _top;
    _album->view_bottom = _bottom;
    rde_arr_clear(&_album->hits);
    if(_top <= _bottom) {
        return;
    }

    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left + 8.0f), (u32)(_top - _bottom) });
    if(_album->page_open) {
        fude_album_render_page(_album, _font, _font_px, _left, _right, _top, _bottom);
    } else if(_album->view == FUDE_ALBUM_VIEW_EXAMS) {
        fude_album_render_exams(_album, _font, _font_px, _left, _right, _top, _bottom);
    } else {
        fude_album_render_overview(_album, _font, _font_px, _left, _right, _top, _bottom);
    }
    rde_rendering_end_clipping_rect();
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"

FUDE_SCREEN_ADAPTERS(fude_album, fude_album)
FUDE_SCREEN_RENDER(fude_album, fude_album)

#define FUDE_ALBUM_PRACTICE_MAX 10u   // "Practice n": its weakest this many

// Escape: a character's page back to the overview, or the album closed.
RDE_INTERNAL void fude_album_screen_back(void* _self) {
    fude_album* _album = (fude_album*)_self;
    if(_album->page_open) { fude_album_close_page(_album); } else { fude_album_close(_album); }
}

// A kept exam tapped: its results, over the album.
RDE_INTERNAL void fude_album_screen_update(fude_app* _app, void* _self, f32 _dt) {
    fude_album* _album = (fude_album*)_self;
    fude_album_update(_album, _dt);
    u32 _kept;
    if(fude_album_take_exam(_album, &_kept)) {
        fude_exam_open_kept(FUDE_STUDY(_app)->exam, _kept);
    }
}

FUDE_ROW_CALL(fude_album_row_close, fude_album, fude_album_close)
FUDE_ROW_CALL(fude_album_row_back,  fude_album, fude_album_close_page)

// A sort (_arg, FUDE_ALBUM_SORT_), or the exams.
RDE_INTERNAL void fude_album_row_sort(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_album_set_sort((fude_album*)_self, (FUDE_ALBUM_SORT_)_arg);
}

RDE_INTERNAL void fude_album_row_exams(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_album_set_view((fude_album*)_self, FUDE_ALBUM_VIEW_EXAMS);
}

// The weakest, as a set.
RDE_INTERNAL void fude_album_row_weakest(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_ALBUM_PRACTICE_MAX];
    const u32 _n = fude_album_weakest((const fude_album*)_self, _records, FUDE_ALBUM_PRACTICE_MAX);
    fude_study_practice_set(_app, _records, _n);
}

// A character's page: Practice it (its Back comes back here, to the new session).
RDE_INTERNAL void fude_album_row_practice(fude_app* _app, void* _self, u32 _arg) {
    const fude_album* _album = (const fude_album*)_self;
    RDE_UNUSED(_arg);
    if(_album->page_open) {
        fude_study_practice(_app, &_album->page_record, 1u);
    }
}

// The overview (the sorts first, in FUDE_ALBUM_SORT_ order) and a character's page.
enum { FUDE_ALBUM_ROW_OVERVIEW = 0, FUDE_ALBUM_ROW_PAGE };
enum { FUDE_ALBUM_MENU_EXAMS = FUDE_ALBUM_SORT_COUNT, FUDE_ALBUM_MENU_PRACTICE, FUDE_ALBUM_MENU_CLOSE };
static const fude_row_button FUDE_ALBUM_OVERVIEW[] = {
    { FUDE_TEXT_ALBUM_WEAKEST, FUDE_ICON_WEAKEST, fude_album_row_sort,    FUDE_ALBUM_SORT_WEAKEST, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_ALBUM_RECENT,  FUDE_ICON_CLOCK,   fude_album_row_sort,    FUDE_ALBUM_SORT_RECENT,  FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_ALBUM_MOST,    FUDE_ICON_STACK,   fude_album_row_sort,    FUDE_ALBUM_SORT_MOST,    FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_EXAMS,         FUDE_ICON_EXAM,    fude_album_row_exams,   0,                       FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE,      FUDE_ICON_PEN,     fude_album_row_weakest, 0,                       FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_CLOSE,         FUDE_ICON_CLOSE,   fude_album_row_close,   0,                       FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_button FUDE_ALBUM_PAGE[] = {
    { FUDE_TEXT_BACK,     FUDE_ICON_BACK, fude_album_row_back,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE, FUDE_ICON_PEN,  fude_album_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_def FUDE_ALBUM_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_ALBUM_OVERVIEW), FUDE_ROW_DEF(FUDE_ALBUM_PAGE) };

RDE_INTERNAL u32 fude_album_screen_row(const void* _self) {
    return ((const fude_album*)_self)->page_open ? FUDE_ALBUM_ROW_PAGE : FUDE_ALBUM_ROW_OVERVIEW;
}

// The order shown (or the exams) chosen; "Practice n", its weakest up to ten.
RDE_INTERNAL void fude_album_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    const fude_album* _album = (const fude_album*)_self;
    if(_row != FUDE_ALBUM_ROW_OVERVIEW) {
        return;
    }
    _faces[_album->view == FUDE_ALBUM_VIEW_EXAMS ? (u32)FUDE_ALBUM_MENU_EXAMS : (u32)_album->sort].selected = true;
    const u32 _entries = (u32)rde_arr_length(&_album->entries);
    const u32 _n       = _entries < FUDE_ALBUM_PRACTICE_MAX ? _entries : FUDE_ALBUM_PRACTICE_MAX;
    if(_n > 0u) {
        fude_row_face_count(&_faces[FUDE_ALBUM_MENU_PRACTICE], FUDE_TEXT_PRACTICE_N, _n);
    }
    _faces[FUDE_ALBUM_MENU_PRACTICE].disabled = _n == 0u;
}

const fude_screen FUDE_ALBUM_SCREEN = {
    .name = "album", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_album_screen_is_open, .close = fude_album_screen_back,
    .update = fude_album_screen_update, .render = fude_album_screen_render,
    .pointer_down = fude_album_screen_down, .pointer_moved = fude_album_screen_moved, .pointer_up = fude_album_screen_up,
    .rows = FUDE_ALBUM_BUTTON_ROWS, .row_count = 2u, .row = fude_album_screen_row, .faces = fude_album_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
