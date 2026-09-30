#include "album.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See album.h.
// ===========================================================================

#define KANA_ALBUM_MARGIN      16.0f
#define KANA_ALBUM_CELL_MIN    96.0f     // overview: the grid fits as many columns as this allows
#define KANA_ALBUM_TITLE_H     72.0f     // overview: "Album" and the counts
#define KANA_ALBUM_GLYPH       128.0f    // page: the character
#define KANA_ALBUM_HEAD_H      168.0f    // page: the character, its summary and trend
#define KANA_ALBUM_TREND_W     420.0f
#define KANA_ALBUM_TREND_H     60.0f
#define KANA_ALBUM_THUMB       104.0f    // an attempt
#define KANA_ALBUM_THUMB_GAP   10.0f
#define KANA_ALBUM_SESSION_H   36.0f     // a session's date line
#define KANA_ALBUM_LABEL_H     24.0f     // the score under an attempt
#define KANA_ALBUM_DETAIL_H    30.0f     // the replayed attempt's line
#define KANA_ALBUM_SESSION_GAP 18.0f
#define KANA_ALBUM_STROKE_GAP  0.25      // replay: seconds between strokes

void kana_album_init(kana_album* _album, const kana_kanji_db* _db) {
    memset(_album, 0, sizeof(*_album));
    _album->db       = _db;
    _album->selected = -1;
    _album->sort     = KANA_ALBUM_SORT_WEAKEST;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _album->entries = rde_arr_new(sizeof(kana_album_entry), _heap);
    _album->hits    = rde_arr_new(sizeof(kana_album_hit), _heap);
    _album->_points = rde_arr_new(sizeof(rde_vec_2F), _heap);
    kana_history_init(&_album->history);
    kana_glyph_init(&_album->glyph, _db);
}

void kana_album_destroy(kana_album* _album) {
    rde_arr* _arrays[] = { &_album->entries, &_album->hits, &_album->_points };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    kana_history_destroy(&_album->history);
    kana_glyph_destroy(&_album->glyph);
    memset(_album, 0, sizeof(*_album));
}

// --- the overview's list --------------------------------------------------------------

RDE_INTERNAL i32 kana_album_by_codepoint(const kana_album_entry* _a, const kana_album_entry* _b) {
    return _a->codepoint < _b->codepoint ? -1 : _a->codepoint > _b->codepoint ? 1 : 0;
}

RDE_INTERNAL i32 kana_album_by_recent(const kana_album_entry* _a, const kana_album_entry* _b) {
    if(_a->summary.last_time != _b->summary.last_time) {
        return _a->summary.last_time > _b->summary.last_time ? -1 : 1;
    }
    return kana_album_by_codepoint(_a, _b);
}

RDE_INTERNAL int kana_album_compare_weakest(const void* _a, const void* _b) {
    const kana_album_entry* _x = (const kana_album_entry*)_a;
    const kana_album_entry* _y = (const kana_album_entry*)_b;
    if(_x->summary.last < _y->summary.last) { return -1; }
    if(_x->summary.last > _y->summary.last) { return  1; }
    return kana_album_by_recent(_x, _y);
}

RDE_INTERNAL int kana_album_compare_recent(const void* _a, const void* _b) {
    return kana_album_by_recent((const kana_album_entry*)_a, (const kana_album_entry*)_b);
}

RDE_INTERNAL int kana_album_compare_most(const void* _a, const void* _b) {
    const kana_album_entry* _x = (const kana_album_entry*)_a;
    const kana_album_entry* _y = (const kana_album_entry*)_b;
    if(_x->summary.sessions != _y->summary.sessions) {
        return _x->summary.sessions > _y->summary.sessions ? -1 : 1;
    }
    return kana_album_by_recent(_x, _y);
}

RDE_INTERNAL void kana_album_sort(kana_album* _album) {
    int (*_compare[KANA_ALBUM_SORT_COUNT])(const void*, const void*) = { kana_album_compare_weakest, kana_album_compare_recent, kana_album_compare_most };
    if(rde_arr_length(&_album->entries) > 1) {
        qsort(_album->entries.memory, rde_arr_length(&_album->entries), sizeof(kana_album_entry), _compare[_album->sort]);
    }
}

// Every character with a history the data knows, summarized.
RDE_INTERNAL void kana_album_load(kana_album* _album) {
    rde_arr_clear(&_album->entries);
    _album->sessions        = 0;
    _album->loaded          = true;
    _album->loaded_revision = kana_history_revision();
    if(_album->db == NULL) {
        return;
    }

    rde_arr _codepoints = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_history_list(&_codepoints);

    for(u32 _i = 0; _i < (u32)rde_arr_length(&_codepoints); _i++) {
        kana_album_entry _entry;
        memset(&_entry, 0, sizeof(_entry));
        _entry.codepoint = ((const u32*)_codepoints.memory)[_i];
        if(kana_kanji_find_index(_album->db, _entry.codepoint, &_entry.record) && kana_history_summarize(_entry.codepoint, &_entry.summary)) {
            rde_arr_add(&_album->entries, &_entry);
            _album->sessions += _entry.summary.sessions;
        }
    }

    rde_arr_free(&_codepoints);
    kana_album_sort(_album);
}

u32 kana_album_weakest(const kana_album* _album, u32* _out, u32 _max) {
    // A copy in weakest order, whatever order the overview shows.
    const u32 _count = (u32)rde_arr_length(&_album->entries);
    if(_count == 0 || _max == 0) {
        return 0;
    }
    kana_album_entry* _sorted = (kana_album_entry*)rde_malloc(sizeof(kana_album_entry) * _count);
    memcpy(_sorted, _album->entries.memory, sizeof(kana_album_entry) * _count);
    qsort(_sorted, _count, sizeof(kana_album_entry), kana_album_compare_weakest);

    const u32 _n = _count < _max ? _count : _max;
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = _sorted[_i].record;
    }
    rde_free(_sorted);
    return _n;
}

// --- opening and closing ------------------------------------------------------------

void kana_album_open(kana_album* _album) {
    _album->open      = true;
    _album->page_open = false;
    _album->selected  = -1;
    kana_scroller_stop(&_album->scroller);
    _album->scroller.offset = 0.0f;
    kana_album_load(_album);
}

void kana_album_close(kana_album* _album) {
    _album->open      = false;
    _album->page_open = false;
    _album->selected  = -1;
    kana_scroller_stop(&_album->scroller);
    kana_scroller_stop(&_album->page_scroller);
}

void kana_album_set_sort(kana_album* _album, KANA_ALBUM_SORT_ _sort) {
    _album->sort = _sort;
    kana_album_sort(_album);
    kana_scroller_stop(&_album->scroller);
    _album->scroller.offset = 0.0f;
}

void kana_album_open_page(kana_album* _album, u32 _codepoint) {
    _album->page_codepoint = _codepoint;
    _album->page_record    = 0;
    if(_album->db != NULL) {
        kana_kanji_find_index(_album->db, _codepoint, &_album->page_record);
    }
    kana_history_load(&_album->history, _codepoint);
    _album->page_open = true;
    _album->selected  = -1;
    kana_scroller_stop(&_album->page_scroller);
    _album->page_scroller.offset = 0.0f;
}

void kana_album_close_page(kana_album* _album) {
    _album->page_open = false;
    _album->selected  = -1;
    kana_scroller_stop(&_album->page_scroller);
}

// --- input ---------------------------------------------------------------------------

RDE_INTERNAL kana_scroller* kana_album_scroller(kana_album* _album) {
    return _album->page_open ? &_album->page_scroller : &_album->scroller;
}

void kana_album_pointer_down(kana_album* _album, rde_vec_2F _screen, f64 _time) {
    kana_scroller_down(kana_album_scroller(_album), _screen, _time);
}

void kana_album_pointer_moved(kana_album* _album, rde_vec_2F _screen, f64 _time) {
    kana_scroller_moved(kana_album_scroller(_album), _screen, _time);
}

void kana_album_pointer_up(kana_album* _album, f64 _time) {
    kana_scroller* _scroller = kana_album_scroller(_album);
    kana_scroller_up(_scroller, _time);

    rde_vec_2F _at;
    if(!kana_scroller_take_tap(_scroller, &_at) || _at.y > _album->view_top || _at.y < _album->view_bottom) {
        return;
    }

    const f32             _content_y = _album->view_top - _at.y + _scroller->offset;
    const kana_album_hit* _hits      = (const kana_album_hit*)_album->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_album->hits); _i++) {
        const kana_album_hit* _h = &_hits[_i];
        if(_at.x < _h->x || _at.x >= _h->x + _h->w || _content_y < _h->y || _content_y >= _h->y + _h->h) {
            continue;
        }

        if(_album->page_open) {
            // An attempt: replay it (again, if it already was).
            _album->selected     = (i32)_h->index;
            _album->replay_start = rde_engine_get_time_now();
        } else if(_h->index < (u32)rde_arr_length(&_album->entries)) {
            kana_album_open_page(_album, ((const kana_album_entry*)_album->entries.memory)[_h->index].codepoint);
        }
        return;
    }
}

void kana_album_update(kana_album* _album, f32 _dt) {
    if(!_album->open) {
        return;
    }

    // A session was saved (Practice, opened from a page): read it all again.
    if(!_album->loaded || _album->loaded_revision != kana_history_revision()) {
        kana_album_load(_album);
        if(_album->page_open) {
            kana_history_load(&_album->history, _album->page_codepoint);
            _album->selected = -1;
        }
    }

    kana_scroller_update(kana_album_scroller(_album), _dt, _album->content_height, _album->view_top - _album->view_bottom);
}

// --- drawing helpers --------------------------------------------------------------------

// A time as local date (and clock time).
RDE_INTERNAL void kana_album_date(u64 _time, b8 _with_clock, c8* _out, usize _size) {
    const time_t _t  = (time_t)_time;
    struct tm*   _tm = localtime(&_t);
    if(_tm == NULL || strftime(_out, _size, _with_clock ? "%a %d %b %Y, %H:%M" : "%d %b %Y", _tm) == 0) {
        snprintf(_out, _size, "?");
    }
}

// One attempt as it was drawn, in the square at _tl (_size wide). _elapsed < 0
// draws it whole; otherwise as far as a replay has got after _elapsed seconds
// — each stroke at the speed it was written, a pause between strokes — with the
// pen's tip where it is.
RDE_INTERNAL void kana_album_draw_attempt(kana_album* _album, const kana_history_square* _square, rde_vec_2F _tl, f32 _size, f64 _elapsed) {
    const kana_history_stroke* _strokes = (const kana_history_stroke*)_album->history.strokes.memory;
    const kana_history_point*  _points  = (const kana_history_point*)_album->history.points.memory;
    const kana_theme*          _theme   = kana_theme_active();

    // The practice pen's width: the model's own stroke width, as a part of the square.
    const f32        _radius = fmaxf(0.8f, _size * KANA_GLYPH_WIDTH / KANA_KANJI_BOX * 0.5f);
    const rde_vec_2F _bl     = { _tl.x, _tl.y - _size };
    const f32        _k      = _size / 65535.0f;
    f64              _clock  = _elapsed;

    for(u32 _s = 0; _s < _square->stroke_count; _s++) {
        const kana_history_stroke* _stroke = &_strokes[_square->first_stroke + _s];
        if(_stroke->point_count == 0) {
            continue;
        }
        const kana_history_point* _p        = &_points[_stroke->first_point];
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
        kana_draw_stroke_even(_pos, _shown, _radius, _theme->ink);

        if(_partial) {
            rde_rendering_2d_draw_circle(_pos[_shown - 1], _radius * 1.35f, 16u, _theme->pen_tip, NULL);
            return;
        }
        _clock -= (f64)_duration + KANA_ALBUM_STROKE_GAP;
    }
}

// Every session's average, oldest to newest, in the box under _tl.
RDE_INTERNAL void kana_album_trend(kana_album* _album, rde_vec_2F _tl, f32 _w, f32 _h) {
    const kana_history_session* _sessions = (const kana_history_session*)_album->history.sessions.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    const kana_theme*           _theme    = kana_theme_active();
    if(_count == 0 || _w <= 0.0f) {
        return;
    }

    // The grade lines, faint: what "fair" and "good" start at.
    const f32 _levels[2] = { KANA_THEME_GRADE_FAIR, KANA_THEME_GRADE_GOOD };
    for(u32 _i = 0; _i < 2; _i++) {
        const f32 _y = _tl.y - _h + _h * _levels[_i] / 100.0f;
        kana_draw_line((rde_vec_2F){ _tl.x, _y }, (rde_vec_2F){ _tl.x + _w, _y }, 0.5f, _theme->line);
    }

    rde_arr_clear(&_album->_points);
    rde_vec_2F* _pos = (rde_vec_2F*)rde_arr_add_n(&_album->_points, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x = _count == 1 ? _tl.x + _w * 0.5f : _tl.x + _w * (f32)_i / (f32)(_count - 1u);
        _pos[_i] = (rde_vec_2F){ _x, _tl.y - _h + _h * rde_math_clamp_f32(_sessions[_i].average, 0.0f, 100.0f) / 100.0f };
    }
    if(_count > 1) {
        kana_draw_stroke_even(_pos, _count, 1.2f, _theme->text_soft);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const rde_vec_2F _at = ((const rde_vec_2F*)_album->_points.memory)[_i];
        rde_rendering_2d_draw_circle(_at, _count > 40 ? 2.0f : 3.5f, 12u, kana_theme_grade(_sessions[_i].average), NULL);
    }
}

// --- the overview ----------------------------------------------------------------------

RDE_INTERNAL void kana_album_render_overview(kana_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme  = kana_theme_active();
    const f32         _scroll = _album->scroller.offset;
    const u32         _count  = (u32)rde_arr_length(&_album->entries);
    #define KANA_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    kana_draw_text(_font, _font_px, "Album", _left, KANA_ALBUM_SY(0.0f) - 32.0f, 24.0f, _theme->text);
    c8 _line[128];
    if(_count == 0) {
        snprintf(_line, sizeof(_line), "Nothing practised yet: open a character (Kanji or Kana), then Practice, then Score.");
    } else {
        snprintf(_line, sizeof(_line), "%u character%s practised, %u session%s", _count, _count == 1 ? "" : "s", _album->sessions, _album->sessions == 1 ? "" : "s");
    }
    kana_draw_text(_font, _font_px, _line, _left, KANA_ALBUM_SY(0.0f) - 60.0f, 17.0f, _theme->text_soft);

    const f32 _width   = _right - _left;
    const u32 _columns = (u32)fmaxf(1.0f, floorf(_width / KANA_ALBUM_CELL_MIN));
    const f32 _cell    = _width / (f32)_columns;
    const f32 _glyph   = _cell * 0.55f;
    const u32 _rows    = (_count + _columns - 1u) / _columns;
    _album->content_height = KANA_ALBUM_TITLE_H + (f32)_rows * _cell + KANA_ALBUM_MARGIN;

    const kana_album_entry* _entries = (const kana_album_entry*)_album->entries.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x  = _left + (f32)(_i % _columns) * _cell;
        const f32 _cy = KANA_ALBUM_TITLE_H + (f32)(_i / _columns) * _cell;   // the cell's top, content space
        const f32 _y  = KANA_ALBUM_SY(_cy);
        if(_y - _cell > _top || _y < _bottom) {
            continue;
        }

        const kana_album_hit _hit = { _x, _cy, _cell, _cell, _i };
        rde_arr_add(&_album->hits, &_hit);

        const kana_album_entry* _e = &_entries[_i];
        kana_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, _theme->line);
        kana_glyph_character(&_album->glyph, _e->codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.06f }, _glyph, _theme->ink);

        snprintf(_line, sizeof(_line), "%.0f", (f64)_e->summary.last);
        kana_draw_text(_font, _font_px, _line, _x + 8.0f, _y - _cell + 10.0f, 17.0f, kana_theme_grade(_e->summary.last));
        snprintf(_line, sizeof(_line), "\xC3\x97%u", _e->summary.sessions);   // ×n
        kana_draw_text(_font, _font_px, _line, _x + _cell * 0.58f, _y - _cell + 10.0f, 15.0f, _theme->text_soft);
    }

    #undef KANA_ALBUM_SY
}

// --- a character's page ------------------------------------------------------------------

RDE_INTERNAL void kana_album_render_page(kana_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme*           _theme    = kana_theme_active();
    const f32                   _scroll   = _album->page_scroller.offset;
    const kana_history_session* _sessions = (const kana_history_session*)_album->history.sessions.memory;
    const kana_history_square*  _squares  = (const kana_history_square*)_album->history.squares.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    #define KANA_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    c8 _line[160];
    c8 _date[64];

    // --- the character, its summary, its trend ---------------------------------------
    const f32 _head = KANA_ALBUM_SY(8.0f);
    kana_glyph_character(&_album->glyph, _album->page_codepoint, (rde_vec_2F){ _left, _head }, KANA_ALBUM_GLYPH, _theme->ink);

    const f32 _tx = _left + KANA_ALBUM_GLYPH + 28.0f;
    if(_count > 0) {
        f32 _best = 0.0f;
        for(u32 _i = 0; _i < _count; _i++) {
            _best = fmaxf(_best, _sessions[_i].average);
        }
        kana_album_date(_sessions[0].time, false, _date, sizeof(_date));
        snprintf(_line, sizeof(_line), "%u session%s since %s", _count, _count == 1 ? "" : "s", _date);
        kana_draw_text(_font, _font_px, _line, _tx, _head - 24.0f, 22.0f, _theme->text);
        snprintf(_line, sizeof(_line), "First %.0f, best %.0f, last %.0f", (f64)_sessions[0].average, (f64)_best, (f64)_sessions[_count - 1].average);
        kana_draw_text(_font, _font_px, _line, _tx, _head - 52.0f, 17.0f, _theme->text_soft);
        kana_album_trend(_album, (rde_vec_2F){ _tx, _head - 70.0f }, fminf(KANA_ALBUM_TREND_W, _right - _tx), KANA_ALBUM_TREND_H);
    } else {
        kana_draw_text(_font, _font_px, "No sessions", _tx, _head - 24.0f, 22.0f, _theme->text);
    }

    // --- the sessions, newest first ----------------------------------------------------
    const f32 _width = _right - _left;
    const u32 _cols  = (u32)fmaxf(1.0f, floorf((_width + KANA_ALBUM_THUMB_GAP) / (KANA_ALBUM_THUMB + KANA_ALBUM_THUMB_GAP)));
    const f32 _thumb = fminf(KANA_ALBUM_THUMB, (_width - KANA_ALBUM_THUMB_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    const f64 _now   = rde_engine_get_time_now();
    f32       _y     = KANA_ALBUM_HEAD_H;

    for(u32 _n = 0; _n < _count; _n++) {
        const kana_history_session* _session = &_sessions[_count - 1u - _n];

        // Its average, then when.
        snprintf(_line, sizeof(_line), "%.0f", (f64)_session->average);
        kana_draw_text(_font, _font_px, _line, _left, KANA_ALBUM_SY(_y) - 24.0f, 20.0f, kana_theme_grade(_session->average));
        kana_album_date(_session->time, true, _date, sizeof(_date));
        kana_draw_text(_font, _font_px, _date, _left + 44.0f, KANA_ALBUM_SY(_y) - 24.0f, 17.0f, _theme->text_soft);
        _y += KANA_ALBUM_SESSION_H;

        // Its attempts — the squares written in — in rows.
        u32 _shown    = 0;
        i32 _selected = -1;   // this session's replayed attempt, as its number among the shown
        for(u32 _q = 0; _q < _session->square_count; _q++) {
            const u32                  _index  = _session->first_square + _q;
            const kana_history_square* _square = &_squares[_index];
            if(_square->score.empty) {
                continue;
            }

            const f32        _cy = _y + (f32)(_shown / _cols) * (_thumb + KANA_ALBUM_LABEL_H + KANA_ALBUM_THUMB_GAP);
            const rde_vec_2F _tl = { _left + (f32)(_shown % _cols) * (_thumb + KANA_ALBUM_THUMB_GAP), KANA_ALBUM_SY(_cy) };
            const b8         _is_selected = (i32)_index == _album->selected;
            if(_is_selected) {
                _selected = (i32)_shown;
            }
            _shown++;

            if(_tl.y - _thumb - KANA_ALBUM_LABEL_H > _top || _tl.y < _bottom) {
                continue;
            }
            const kana_album_hit _hit = { _tl.x, _cy, _thumb, _thumb + KANA_ALBUM_LABEL_H, _index };
            rde_arr_add(&_album->hits, &_hit);

            kana_glyph_box(_tl, _thumb);
            kana_glyph_character(&_album->glyph, _album->page_codepoint, _tl, _thumb, _theme->reference);
            kana_album_draw_attempt(_album, _square, _tl, _thumb, _is_selected ? _now - _album->replay_start : -1.0);
            if(_is_selected) {
                kana_draw_outline((rde_vec_2F){ _tl.x - 3.0f, _tl.y - _thumb - 3.0f }, (rde_vec_2F){ _tl.x + _thumb + 3.0f, _tl.y + 3.0f }, 1.5f, _theme->select);
            }

            snprintf(_line, sizeof(_line), "%.0f", (f64)_square->score.score);
            kana_draw_text(_font, _font_px, _line, _tl.x + 2.0f, _tl.y - _thumb - 19.0f, 15.0f, kana_theme_grade(_square->score.score));
        }
        const u32 _rows = (_shown + _cols - 1u) / _cols;
        _y += (f32)_rows * (_thumb + KANA_ALBUM_LABEL_H + KANA_ALBUM_THUMB_GAP);

        // The replayed attempt, in words.
        if(_selected >= 0) {
            const kana_history_square* _square = &_squares[_album->selected];
            snprintf(_line, sizeof(_line), "Attempt %d: %.0f. %s", _selected + 1, (f64)_square->score.score, _square->score.feedback);
            kana_draw_text(_font, _font_px, _line, _left, KANA_ALBUM_SY(_y) - 18.0f, 16.0f, _theme->text);
            _y += KANA_ALBUM_DETAIL_H;
        }
        _y += KANA_ALBUM_SESSION_GAP;
    }

    _album->content_height = _y;
    #undef KANA_ALBUM_SY
}

void kana_album_render(kana_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_album->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_ALBUM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_ALBUM_MARGIN;

    _album->view_top    = _top;
    _album->view_bottom = _bottom;
    rde_arr_clear(&_album->hits);
    if(_top <= _bottom) {
        return;
    }

    rde_rendering_begin_clipping_rect(_window,
                                      (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left + 8.0f), (u32)(_top - _bottom) });
    if(_album->page_open) {
        kana_album_render_page(_album, _font, _font_px, _left, _right, _top, _bottom);
    } else {
        kana_album_render_overview(_album, _font, _font_px, _left, _right, _top, _bottom);
    }
    rde_rendering_end_clipping_rect();
}
