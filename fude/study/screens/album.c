// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/album.h"
#include "drawing/base/redraw.h"
#include "study/widgets/header.h"
#include "drawing/base/text.h"
#include "drawing/base/draw.h"
#include "study/screens/exam.h"
#include "drawing/base/theme.h"


// ===========================================================================
// See album.h.
//
// Each view is laid out while it is drawn: the render functions place everything in content space (y down from the
// top of the scrolled content, turned into screen y by FUDE_ALBUM_SY), record each thing that can be tapped in hits,
// and set content_height for the scroller. A tap is then matched against the last frame's hits. The overview and the
// exams view share one scroller; a character's page has its own, so going back to the overview finds it where it was.
//
// A replay needs no state of its own beyond what is being replayed and when it started: each frame draws the attempt
// as far as the time since replay_start has got.
// ===========================================================================

// The layout's sizes, in screen points.
#define FUDE_ALBUM_MARGIN      16.0f     // The space from the screen's sides (inside its safe area) and under the end.
#define FUDE_ALBUM_CELL_MIN    96.0f     // The overview's smallest cell: the grid has as many columns as fit at this.
#define FUDE_ALBUM_TITLE_H     72.0f     // The overview's and the exams view's title: "Album" and the counts.
#define FUDE_ALBUM_GLYPH       128.0f    // The character at the top of its page.
#define FUDE_ALBUM_HEAD_H      168.0f    // The page's head: the character, its summary and its trend line.
#define FUDE_ALBUM_TREND_W     420.0f    // The trend line's widest.
#define FUDE_ALBUM_TREND_H     60.0f     // The trend line's height.
#define FUDE_ALBUM_THUMB       104.0f    // An attempt's square, at its largest.
#define FUDE_ALBUM_THUMB_GAP   10.0f     // The space between two attempts.
#define FUDE_ALBUM_SESSION_H   36.0f     // A session's (or an exam answer's) line with its date.
#define FUDE_ALBUM_LABEL_H     24.0f     // The score under an attempt.
#define FUDE_ALBUM_DETAIL_H    30.0f     // The line that describes the attempt being replayed.
#define FUDE_ALBUM_SESSION_GAP 18.0f     // The space after a session.
#define FUDE_ALBUM_STROKE_GAP  0.25      // The pause between strokes in a replay, in seconds.
#define FUDE_ALBUM_EXAM_PAD    14.0f     // The space inside an exam's card, in the exams view.
#define FUDE_ALBUM_EXAM_GLYPH  26.0f     // The size of each character asked, on an exam's card.
#define FUDE_ALBUM_EXAM_STEP   34.0f     // The room each character asked takes on the card, across and down.
#define FUDE_ALBUM_EXAM_GAP    12.0f     // The space between two exams' cards.

// Sets the album up empty, on the overview sorted weakest first; it reads the history when it is opened.
void fude_album_init(fude_album* _album, const fude_kanji_db* _db) {
    rde_memset(_album, 0, sizeof(*_album));
    fude_kit_scroller_init(&_album->scroller);
    fude_kit_scroller_init(&_album->page_scroller);
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

// Frees the arrays that were made (none are when init was never called), the page's history and the glyph cache, and
// zeroes the album.
void fude_album_destroy(fude_album* _album) {
    rde_arr* _arrays[] = { &_album->entries, &_album->hits, &_album->_points, &_album->page_exams, &_album->page_exam_strokes, &_album->page_exam_points };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    fude_history_destroy(&_album->history);
    fude_glyph_destroy(&_album->glyph);
    rde_memset(_album, 0, sizeof(*_album));
}

// --- The overview's list ---------------------------------------------------------------------------------------------

// Orders two entries by code point, lowest first: the last tie-break, so every sort comes out the same each time.
RDE_INTERNAL i32 fude_album_by_codepoint(const fude_album_entry* _a, const fude_album_entry* _b) {
    return _a->codepoint < _b->codepoint ? -1 : _a->codepoint > _b->codepoint ? 1 : 0;
}

// Orders two entries by their latest session, most recent first, then by code point.
RDE_INTERNAL i32 fude_album_by_recent(const fude_album_entry* _a, const fude_album_entry* _b) {
    if(_a->summary.last_time != _b->summary.last_time) {
        return _a->summary.last_time > _b->summary.last_time ? -1 : 1;
    }
    return fude_album_by_codepoint(_a, _b);
}

// rde_sort's comparison for FUDE_ALBUM_SORT_WEAKEST: the lowest last average first, then the most recent.
RDE_INTERNAL int fude_album_compare_weakest(const any _a, const any _b) {
    const fude_album_entry* _x = (const fude_album_entry*)_a;
    const fude_album_entry* _y = (const fude_album_entry*)_b;
    if(_x->summary.last < _y->summary.last) { return -1; }
    if(_x->summary.last > _y->summary.last) { return  1; }
    return fude_album_by_recent(_x, _y);
}

// rde_sort's comparison for FUDE_ALBUM_SORT_RECENT: the most recently practised first.
RDE_INTERNAL int fude_album_compare_recent(const any _a, const any _b) {
    return fude_album_by_recent((const fude_album_entry*)_a, (const fude_album_entry*)_b);
}

// rde_sort's comparison for FUDE_ALBUM_SORT_MOST: the most sessions first, then the most recent.
RDE_INTERNAL int fude_album_compare_most(const any _a, const any _b) {
    const fude_album_entry* _x = (const fude_album_entry*)_a;
    const fude_album_entry* _y = (const fude_album_entry*)_b;
    if(_x->summary.sessions != _y->summary.sessions) {
        return _x->summary.sessions > _y->summary.sessions ? -1 : 1;
    }
    return fude_album_by_recent(_x, _y);
}

// Sorts the overview's entries in the album's chosen order. The table is indexed by FUDE_ALBUM_SORT_.
RDE_INTERNAL void fude_album_sort(fude_album* _album) {
    int (*_compare[FUDE_ALBUM_SORT_COUNT])(const any, const any) = { fude_album_compare_weakest, fude_album_compare_recent, fude_album_compare_most };
    if(rde_arr_length(&_album->entries) > 1) {
        rde_sort(_album->entries.memory, rde_arr_length(&_album->entries), sizeof(fude_album_entry), _compare[_album->sort]);
    }
}

// Reads the overview again: every character that has a practice history and that the character data knows, each
// with its summary, sorted. It notes the history's revision, so fude_album_update knows when to read it again.
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
        rde_memset(&_entry, 0, sizeof(_entry));
        _entry.codepoint = ((const u32*)_codepoints.memory)[_i];
        if(fude_kanji_find_index(_album->db, _entry.codepoint, &_entry.record) && fude_history_summarize(_entry.codepoint, &_entry.summary)) {
            rde_arr_add(&_album->entries, &_entry);
            _album->sessions += _entry.summary.sessions;
        }
    }

    rde_arr_free(&_codepoints);
    fude_album_sort(_album);
}

// Gives the weakest characters' records, at most _max of them, and returns how many.
u32 fude_album_weakest(const fude_album* _album, u32* _out, u32 _max) {
    // It sorts a copy in weakest order, so the order the overview shows is left alone.
    const u32 _count = (u32)rde_arr_length(&_album->entries);
    if(_count == 0 || _max == 0) {
        return 0;
    }
    fude_album_entry* _sorted = (fude_album_entry*)rde_malloc(sizeof(fude_album_entry) * _count);
    rde_memcpy(_sorted, _album->entries.memory, sizeof(fude_album_entry) * _count);
    rde_sort(_sorted, _count, sizeof(fude_album_entry), fude_album_compare_weakest);

    const u32 _n = _count < _max ? _count : _max;
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = _sorted[_i].record;
    }
    rde_free(_sorted);
    return _n;
}

// --- Opening and closing ---------------------------------------------------------------------------------------------

// Opens the album scrolled to the top, off any page, and reads the history again. The view (the characters or the
// exams) stays the one last shown.
void fude_album_open(fude_album* _album) {
    _album->open            = true;
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    _album->tapped_exam     = -1;
    rde_ui_scroller_stop(&_album->scroller);
    _album->scroller.offset.y = 0.0f;
    fude_album_load(_album);
}

// Closes the album, and any page and replay with it.
void fude_album_close(fude_album* _album) {
    _album->open            = false;
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    rde_ui_scroller_stop(&_album->scroller);
    rde_ui_scroller_stop(&_album->page_scroller);
}

// Sorts the overview in order _sort, shows it (leaving the exams view), and scrolls to the top.
void fude_album_set_sort(fude_album* _album, FUDE_ALBUM_SORT_ _sort) {
    _album->sort = _sort;
    _album->view = FUDE_ALBUM_VIEW_CHARACTERS;
    fude_album_sort(_album);
    rde_ui_scroller_stop(&_album->scroller);
    _album->scroller.offset.y = 0.0f;
}

// Shows view _view, scrolled to the top; asking for the view already shown changes nothing.
void fude_album_set_view(fude_album* _album, FUDE_ALBUM_VIEW_ _view) {
    if(_album->view == _view) {
        return;
    }
    _album->view        = _view;
    _album->tapped_exam = -1;
    rde_ui_scroller_stop(&_album->scroller);
    _album->scroller.offset.y = 0.0f;
}

// Hands out the exam tapped once: tapped_exam goes back to -1, which means none is waiting.
b8 fude_album_take_exam(fude_album* _album, u32* _exam) {
    if(_album->tapped_exam < 0) {
        return false;
    }
    *_exam              = (u32)_album->tapped_exam;
    _album->tapped_exam = -1;
    return true;
}

// Reads every exam answer to the page's character, with its writing, from the exam log; it notes the log's revision,
// so fude_album_update knows when to read them again.
RDE_INTERNAL void fude_album_load_page_exams(fude_album* _album) {
    rde_arr_clear(&_album->page_exams);
    rde_arr_clear(&_album->page_exam_strokes);
    rde_arr_clear(&_album->page_exam_points);
    _album->exams_revision = fude_examlog_revision();
    fude_examlog_read_writing(UINT32_MAX, _album->page_codepoint, &_album->page_exams, &_album->page_exam_strokes, &_album->page_exam_points);
}

// Opens _codepoint's page at its top, reading its history and its exam answers. Its record stays 0 when the character
// data lacks it.
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
    rde_ui_scroller_stop(&_album->page_scroller);
    _album->page_scroller.offset.y = 0.0f;
}

// Goes back from a page to the view under it, ending any replay.
void fude_album_close_page(fude_album* _album) {
    _album->page_open       = false;
    _album->selected        = -1;
    _album->selected_answer = -1;
    rde_ui_scroller_stop(&_album->page_scroller);
}

// --- Input -----------------------------------------------------------------------------------------------------------

// Returns the scroller of what is shown: the page's while a page is open, else the one the two views share.
RDE_INTERNAL rde_ui_scroller* fude_album_scroller(fude_album* _album) {
    return _album->page_open ? &_album->page_scroller : &_album->scroller;
}

// Starts a drag of what is shown; whether it is a tap is known only when the pointer is lifted.
void fude_album_pointer_down(fude_album* _album, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_down(fude_album_scroller(_album), _screen, _time);
}

// Drags what is shown.
void fude_album_pointer_moved(fude_album* _album, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_moved(fude_album_scroller(_album), _screen, _time);
}

// Ends the drag. A tap inside the view acts on what the last frame laid out under it: on a page it replays an
// attempt, in the exams view it keeps the exam for fude_album_take_exam, and on the overview it opens a page.
void fude_album_pointer_up(fude_album* _album, f64 _time) {
    rde_ui_scroller* _scroller = fude_album_scroller(_album);
    rde_ui_scroller_pointer_up(_scroller, _time);

    rde_vec_2F _at;
    if(!rde_ui_scroller_take_tap(_scroller, &_at) || _at.y > _album->view_top || _at.y < _album->view_bottom) {
        return;
    }

    const f32             _content_y = _album->view_top - _at.y + _scroller->offset.y;
    const fude_album_hit* _hits      = (const fude_album_hit*)_album->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_album->hits); _i++) {
        const fude_album_hit* _h = &_hits[_i];
        if(_at.x < _h->x || _at.x >= _h->x + _h->w || _content_y < _h->y || _content_y >= _h->y + _h->h) {
            continue;
        }

        if(_album->page_open) {
            // An attempt was tapped: replay it, from the start again if it was already being replayed.
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

// Reads the files again when they changed, then scrolls what is shown.
void fude_album_update(fude_album* _album, f32 _dt) {
    if(!_album->open) {
        return;
    }

    // A session was saved (by Practice, opened from a page), so everything is read again: the overview, and the page's
    // history, whose replay no longer points at the same square.
    if(!_album->loaded || _album->loaded_revision != fude_history_revision()) {
        fude_album_load(_album);
        if(_album->page_open) {
            fude_history_load(&_album->history, _album->page_codepoint);
            _album->selected        = -1;
            _album->selected_answer = -1;
        }
    }
    // An exam was taken (such as a kept exam's Retry wrong), so the page's exam answers are read again too.
    if(_album->page_open && _album->exams_revision != fude_examlog_revision()) {
        fude_album_load_page_exams(_album);
        _album->selected        = -1;
        _album->selected_answer = -1;
    }

    fude_kit_scroller_update(fude_album_scroller(_album), _dt, _album->content_height, _album->view_top - _album->view_bottom);
}

// --- Drawing helpers -------------------------------------------------------------------------------------------------

// Writes time _time (Unix seconds) into _out as a local date, with the weekday and the clock time when _with_clock.
RDE_INTERNAL void fude_album_date(u64 _time, b8 _with_clock, c8* _out, usize _size) {
    if(_with_clock) {
        fude_text_date_time(_out, _size, _time);
    } else {
        fude_text_date(_out, _size, _time);
    }
}

// Draws one attempt as it was written: strokes _first to _first + _count - 1 of _strokes, whose points are in _points,
// in the square whose top-left is _tl and which is _size wide. With _elapsed below 0 it is drawn whole. Otherwise it is
// drawn as far as a replay has got after _elapsed seconds: each stroke at the speed it was written, with a pause of
// FUDE_ALBUM_STROKE_GAP between strokes, and the pen's tip drawn where the stroke being written has got to.
RDE_INTERNAL void fude_album_draw_attempt(fude_album* _album, const fude_history_stroke* _strokes, const fude_history_point* _points, u32 _first, u32 _count,
                                          rde_vec_2F _tl, f32 _size, f64 _elapsed) {
    const fude_theme* _theme = fude_theme_active();

    // The same width as the practice pen: the model's own stroke width, scaled to the square.
    const f32        _radius = rde_math_max_f32(0.8f, _size * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f);
    const rde_vec_2F _bl     = { _tl.x, _tl.y - _size };
    // A point's x and y are fractions of the square's side times 65535, with Y up (history.h).
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
                fude_redraw_changed();   // (render on demand: a replay plays with the clock, every frame until done)
                return;   // The replay has not reached this stroke yet.
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
            fude_redraw_changed();   // (render on demand: a replay plays with the clock, every frame until done)
            return;
        }
        _clock -= (f64)_duration + FUDE_ALBUM_STROKE_GAP;
    }
}

// Draws the page's trend line: every session's average, oldest on the left, in the box _w wide and _h tall whose
// top-left is _tl, from 0 at the bottom to 100 at the top. Each session is a dot in its grade's colour.
RDE_INTERNAL void fude_album_trend(fude_album* _album, rde_vec_2F _tl, f32 _w, f32 _h) {
    const fude_history_session* _sessions = (const fude_history_session*)_album->history.sessions.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    const fude_theme*           _theme    = fude_theme_active();
    if(_count == 0 || _w <= 0.0f) {
        return;
    }

    // Two faint lines across, where the grades "fair" and "good" start.
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
    // The dots get smaller when there are more than 40 sessions, so they do not run into each other.
    for(u32 _i = 0; _i < _count; _i++) {
        const rde_vec_2F _at = ((const rde_vec_2F*)_album->_points.memory)[_i];
        rde_rendering_2d_draw_circle(fude_draw_at(_at), _count > 40 ? 2.0f : 3.5f, 12u, fude_theme_grade(_sessions[_i].average), NULL);
    }
}

// --- The overview ----------------------------------------------------------------------------------------------------

// Draws the overview between _left and _right: the title and the counts, then the grid of characters, each with its
// last average (in its grade's colour) and its number of sessions. Only the cells in view are drawn and made tappable.
RDE_INTERNAL void fude_album_render_overview(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme  = fude_theme_active();
    const f32         _scroll = _album->scroller.offset.y;
    const u32         _count  = (u32)rde_arr_length(&_album->entries);
    // Turns a content y (down from the top of the content) into a screen y.
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_ALBUM), _left, FUDE_ALBUM_SY(0.0f) - 2.0f);
    c8 _line[512];
    if(_count == 0) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_ALBUM_EMPTY));
    } else {
        c8 _chars[512], _sessions_n[48];
        FUDE_TEXTF(_chars, FUDE_TEXT_CHARACTERS_N, FUDE_TN(_count));
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_album->sessions));
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_COUNTS, FUDE_TS(_chars), FUDE_TS(_sessions_n));
    }
    fude_draw_text_wrap(_font, _font_px, _line, _left, FUDE_ALBUM_SY(0.0f) - 60.0f, 17.0f, _right - _left, 23.0f, _theme->text_soft);   // Two lines when empty, on a narrow screen.

    const f32 _width   = _right - _left;
    const u32 _columns = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32(_width / FUDE_ALBUM_CELL_MIN));
    const f32 _cell    = _width / (f32)_columns;
    const f32 _glyph   = _cell * 0.55f;
    const u32 _rows    = (_count + _columns - 1u) / _columns;
    _album->content_height = FUDE_ALBUM_TITLE_H + (f32)_rows * _cell + FUDE_ALBUM_MARGIN;

    const fude_album_entry* _entries = (const fude_album_entry*)_album->entries.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const f32 _x  = _left + (f32)(_i % _columns) * _cell;
        const f32 _cy = FUDE_ALBUM_TITLE_H + (f32)(_i / _columns) * _cell;   // The cell's top, in content space.
        const f32 _y  = FUDE_ALBUM_SY(_cy);
        if(_y - _cell > _top || _y < _bottom) {
            continue;
        }

        const fude_album_hit _hit = { _x, _cy, _cell, _cell, _i };
        rde_arr_add(&_album->hits, &_hit);

        const fude_album_entry* _e = &_entries[_i];
        fude_draw_line((rde_vec_2F){ _x + 4.0f, _y - _cell }, (rde_vec_2F){ _x + _cell - 4.0f, _y - _cell }, 0.5f, _theme->line);
        fude_glyph_character(&_album->glyph, _e->codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - _cell * 0.06f }, _glyph, _theme->ink);

        rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_e->summary.last);
        fude_draw_text(_font, _font_px, _line, _x + 8.0f, _y - _cell + 10.0f, 17.0f, fude_theme_grade(_e->summary.last));
        rde_snprintf(_line, sizeof(_line), "\xC3\x97%u", _e->summary.sessions);   // The sessions, as "×n" (UTF-8 ×).
        fude_draw_text(_font, _font_px, _line, _x + _cell * 0.58f, _y - _cell + 10.0f, 15.0f, _theme->text_soft);
    }

    #undef FUDE_ALBUM_SY
}

// --- The exams -------------------------------------------------------------------------------------------------------

// Draws the exams view between _left and _right: the title and how many exams were taken and passed, then every exam
// in the log, newest first, a card each. A card shows what the exam was of and when, whether it was passed, how many
// were right and the points, and each character asked, green when it was right and red when not. A tapped card opens
// the exam's results (exam.h).
RDE_INTERNAL void fude_album_render_exams(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*        _theme  = fude_theme_active();
    const f32                _scroll = _album->scroller.offset.y;
    const u32                _count  = fude_examlog_count();
    const fude_examlog_exam* _exams  = fude_examlog_exams();
    const fude_examlog_item* _items  = fude_examlog_items();
    // Turns a content y (down from the top of the content) into a screen y.
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_ALBUM), _left, FUDE_ALBUM_SY(0.0f) - 2.0f);
    c8  _line[512];
    u32 _passed = 0;
    for(u32 _e = 0; _e < _count; _e++) {
        _passed += (f32)_exams[_e].correct >= FUDE_EXAM_PASS * (f32)_exams[_e].item_count ? 1u : 0u;
    }
    if(_count == 0) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_STATS_NO_EXAMS));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_EXAMS_COUNTS, FUDE_TN(_count), FUDE_TN(_passed));
    }
    fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(0.0f) - 60.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 17.0f, _right - _left),
                   _theme->text_soft);

    const f32 _width    = _right - _left;
    const u32 _per_row  = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32((_width - 2.0f * FUDE_ALBUM_EXAM_PAD) / FUDE_ALBUM_EXAM_STEP));
    f32       _y        = FUDE_ALBUM_TITLE_H;
    for(u32 _n = 0; _n < _count; _n++) {
        const u32                _e    = _count - 1u - _n;
        const fude_examlog_exam* _exam = &_exams[_e];
        const u32                _rows = (_exam->item_count + _per_row - 1u) / _per_row;
        // The card's height: its padding, 52 points for its two lines of text, and its lines of characters.
        const f32                _h    = 2.0f * FUDE_ALBUM_EXAM_PAD + 52.0f + (f32)_rows * FUDE_ALBUM_EXAM_STEP;
        const f32                _sy   = FUDE_ALBUM_SY(_y);
        if(!(_sy - _h > _top || _sy < _bottom)) {
            const fude_album_hit _hit = { _left, _y, _width, _h, _e };
            rde_arr_add(&_album->hits, &_hit);
            fude_draw_card((rde_vec_2F){ _left, _sy - _h }, (rde_vec_2F){ _right, _sy }, 14.0f, _theme->surface, _theme->outline);

            // What the exam was of and when, with whether it was passed on a chip at the right.
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
                           fude_draw_text_px_to_fit(_font, _font_px, _line, 18.0f, _width - 2.0f * FUDE_ALBUM_EXAM_PAD - _chip_w - 12.0f), _theme->text);
            FUDE_TEXTF(_line, FUDE_TEXT_EXAM_RESULT, FUDE_TN(_exam->correct), FUDE_TN(_exam->item_count), FUDE_TN(rde_math_round_to_i64_f32(_exam->score)));
            fude_draw_text(_font, _font_px, _line, _x, _base - 24.0f, 15.0f, _theme->text_soft);

            // Each character asked, in the colour of its answer.
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

// --- A character's page ----------------------------------------------------------------------------------------------

// Draws exam answer _index (into page_exams) on the page, from content y _y: right or wrong, which exam and when, then
// the writing over the model (faint), with its points under it when it was right, or what it was read as when it was
// not. _thumb is the square's size and _now the time, for a replay. Returns the content y where the next thing goes.
RDE_INTERNAL f32 fude_album_render_answer(fude_album* _album, rde_font* _font, f32 _font_px, u32 _index, f32 _left, f32 _top, f32 _bottom, f32 _y, f32 _thumb,
                                          f64 _now) {
    const fude_theme*           _theme  = fude_theme_active();
    const f32                   _scroll = _album->page_scroller.offset.y;
    const fude_examlog_writing* _answer = &((const fude_examlog_writing*)_album->page_exams.memory)[_index];
    const fude_examlog_exam*    _exam   = &fude_examlog_exams()[_answer->exam];
    const fude_examlog_item*    _item   = &fude_examlog_items()[_exam->first_item + _answer->item];
    // Turns a content y (down from the top of the content) into a screen y.
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    c8        _title[512], _date[96], _line[32];
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
        fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _thumb }, (rde_vec_2F){ _tl.x + _thumb, _tl.y }, false);   // Handwriting is never mirrored (draw.h).
        fude_glyph_character(&_album->glyph, _album->page_codepoint, _tl, _thumb, _theme->reference);
        fude_album_draw_attempt(_album, (const fude_history_stroke*)_album->page_exam_strokes.memory, (const fude_history_point*)_album->page_exam_points.memory,
                                _answer->first_stroke, _answer->stroke_count, _tl, _thumb, _is_selected ? _now - _album->replay_start : -1.0);
        fude_draw_keep_end();
        if(_is_selected) {
            fude_draw_outline((rde_vec_2F){ _tl.x - 3.0f, _tl.y - _thumb - 3.0f }, (rde_vec_2F){ _tl.x + _thumb + 3.0f, _tl.y + 3.0f }, 1.5f, _theme->select);
        }
        if(_item->correct) {
            rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_item->score);
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

// Draws a character's page between _left and _right: the head (the character, a summary and the trend line), then
// its sessions and exam answers merged newest first. A session shows its average and date, then each attempt in rows,
// with its score; the attempt being replayed is outlined and described under its rows.
RDE_INTERNAL void fude_album_render_page(fude_album* _album, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*           _theme    = fude_theme_active();
    const f32                   _scroll   = _album->page_scroller.offset.y;
    const fude_history_session* _sessions = (const fude_history_session*)_album->history.sessions.memory;
    const fude_history_square*  _squares  = (const fude_history_square*)_album->history.squares.memory;
    const u32                   _count    = (u32)rde_arr_length(&_album->history.sessions);
    // Turns a content y (down from the top of the content) into a screen y.
    #define FUDE_ALBUM_SY(_content_y) (_top - ((_content_y) - _scroll))

    c8 _line[512];
    c8 _date[64];

    // --- The head: the character, its summary and its trend line ---
    const f32 _head = FUDE_ALBUM_SY(8.0f);
    fude_glyph_character(&_album->glyph, _album->page_codepoint, (rde_vec_2F){ _left, _head }, FUDE_ALBUM_GLYPH, _theme->ink);

    const f32 _tx = _left + FUDE_ALBUM_GLYPH + 28.0f;
    if(_count > 0) {
        f32 _best = 0.0f;
        for(u32 _i = 0; _i < _count; _i++) {
            _best = rde_math_max_f32(_best, _sessions[_i].average);
        }
        fude_album_date(_sessions[0].time, false, _date, sizeof(_date));
        c8 _sessions_n[512];
        FUDE_TEXTF(_sessions_n, FUDE_TEXT_SESSIONS_N, FUDE_TN(_count));
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_SINCE, FUDE_TS(_sessions_n), FUDE_TS(_date));
        fude_draw_text(_font, _font_px, _line, _tx, _head - 24.0f, 22.0f, _theme->text);
        FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_FIRST_BEST_LAST, FUDE_TN(rde_math_round_to_i64_f32(_sessions[0].average)), FUDE_TN(rde_math_round_to_i64_f32(_best)), FUDE_TN(rde_math_round_to_i64_f32(_sessions[_count - 1].average)));
        fude_draw_text(_font, _font_px, _line, _tx, _head - 52.0f, 17.0f, _theme->text_soft);
        fude_album_trend(_album, (rde_vec_2F){ _tx, _head - 70.0f }, rde_math_min_f32(FUDE_ALBUM_TREND_W, _right - _tx), FUDE_ALBUM_TREND_H);
    } else {
        fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_ALBUM_NO_SESSIONS), _tx, _head - 24.0f, 22.0f, _theme->text);
    }

    // --- The sessions, newest first ---
    // As many attempts to a row as fit at FUDE_ALBUM_THUMB; on a narrow screen the squares shrink to fill the width.
    const f32 _width = _right - _left;
    const u32 _cols  = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32((_width + FUDE_ALBUM_THUMB_GAP) / (FUDE_ALBUM_THUMB + FUDE_ALBUM_THUMB_GAP)));
    const f32 _thumb = rde_math_min_f32(FUDE_ALBUM_THUMB, (_width - FUDE_ALBUM_THUMB_GAP * (f32)(_cols - 1u)) / (f32)_cols);
    const f64 _now   = rde_engine_get_time_now();
    f32       _y     = FUDE_ALBUM_HEAD_H;

    // The sessions and the exam answers are both kept oldest first; each step draws the newer of the two still to
    // show, so they come out merged, newest first.
    const fude_examlog_writing* _answers  = (const fude_examlog_writing*)_album->page_exams.memory;
    const fude_examlog_exam*    _exams    = fude_examlog_exams();
    u32                         _ns       = _count;                                   // Sessions to show, newest last.
    u32                         _na       = (u32)rde_arr_length(&_album->page_exams);
    while(_ns > 0 || _na > 0) {
        if(_na > 0 && (_ns == 0 || _exams[_answers[_na - 1u].exam].time >= _sessions[_ns - 1u].time)) {
            _na--;
            _y = fude_album_render_answer(_album, _font, _font_px, _na, _left, _top, _bottom, _y, _thumb, _now);
            continue;
        }
        const fude_history_session* _session = &_sessions[--_ns];

        // The session's average, then its date and time.
        rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_session->average);
        fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(_y) - 24.0f, 20.0f, fude_theme_grade(_session->average));
        fude_album_date(_session->time, true, _date, sizeof(_date));
        fude_draw_text(_font, _font_px, _date, _left + 44.0f, FUDE_ALBUM_SY(_y) - 24.0f, 17.0f, _theme->text_soft);
        _y += FUDE_ALBUM_SESSION_H;

        // Its attempts, which are the squares written in (empty squares are skipped), in rows.
        u32 _shown    = 0;
        i32 _selected = -1;   // The attempt being replayed in this session, counted among those shown (-1: none).
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
            fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _thumb }, (rde_vec_2F){ _tl.x + _thumb, _tl.y }, false);   // Handwriting is never mirrored (draw.h).
            fude_glyph_character(&_album->glyph, _album->page_codepoint, _tl, _thumb, _theme->reference);
            fude_album_draw_attempt(_album, (const fude_history_stroke*)_album->history.strokes.memory, (const fude_history_point*)_album->history.points.memory,
                                    _square->first_stroke, _square->stroke_count, _tl, _thumb, _is_selected ? _now - _album->replay_start : -1.0);
            fude_draw_keep_end();
            if(_is_selected) {
                fude_draw_outline((rde_vec_2F){ _tl.x - 3.0f, _tl.y - _thumb - 3.0f }, (rde_vec_2F){ _tl.x + _thumb + 3.0f, _tl.y + 3.0f }, 1.5f, _theme->select);
            }

            rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_square->score.score);
            fude_draw_text(_font, _font_px, _line, _tl.x + 2.0f, _tl.y - _thumb - 19.0f, 15.0f, fude_theme_grade(_square->score.score));
        }
        const u32 _rows = (_shown + _cols - 1u) / _cols;
        _y += (f32)_rows * (_thumb + FUDE_ALBUM_LABEL_H + FUDE_ALBUM_THUMB_GAP);

        // The attempt being replayed, in words: its number, its score and what its score said.
        if(_selected >= 0) {
            const fude_history_square* _square = &_squares[_album->selected];
            FUDE_TEXTF(_line, FUDE_TEXT_ALBUM_ATTEMPT, FUDE_TN(_selected + 1), FUDE_TN(rde_math_round_to_i64_f32(_square->score.score)), FUDE_TS(_square->score.feedback));
            fude_draw_text(_font, _font_px, _line, _left, FUDE_ALBUM_SY(_y) - 18.0f, 16.0f, _theme->text);
            _y += FUDE_ALBUM_DETAIL_H;
        }
        _y += FUDE_ALBUM_SESSION_GAP;
    }

    _album->content_height = _y;
    #undef FUDE_ALBUM_SY
}

// Remembers the view's band for tapping and scrolling, clears last frame's hits, and draws what is shown (a page, the
// exams or the overview), clipped to the band.
void fude_album_render(fude_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_album->open) {
        return;
    }

    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // The order is left, top, right, bottom.
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_ALBUM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_ALBUM_MARGIN;

    _album->view_top    = _top;
    _album->view_bottom = _bottom;
    rde_arr_clear(&_album->hits);
    if(_top <= _bottom) {
        return;
    }

    // The clip is 8 points wider than the content, so outlines drawn just outside a square at the edge still show.
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

// --- The screen (screen.h): its rows of buttons, and what they do ----------------------------------------------------

#include "study/app/study.h"
#include "drawing/base/icons.h"

// The screen's is_open, close, pointer and render functions, which call the album's own (screen.h).
FUDE_SCREEN_ADAPTERS(fude_album, fude_album)
FUDE_SCREEN_RENDER(fude_album, fude_album)

#define FUDE_ALBUM_PRACTICE_MAX 10u   // How many of the weakest characters "Practice n" takes, at most.

// Escape: from a character's page back to the overview, else closes the album (the close FUDE_SCREEN_ADAPTERS made).
RDE_INTERNAL void fude_album_screen_back(any _self) {
    fude_album* _album = (fude_album*)_self;
    if(_album->page_open) { fude_album_close_page(_album); } else { fude_album_screen_close(_self); }
}

// Updates the album, then hands on a kept exam tapped in the exams view: its results open over the album.
RDE_INTERNAL void fude_album_screen_update(fude_app* _app, any _self, f32 _dt) {
    fude_album* _album = (fude_album*)_self;
    fude_album_update(_album, _dt);
    u32 _kept;
    if(fude_album_take_exam(_album, &_kept)) {
        fude_exam_open_kept(FUDE_STUDY(_app)->exam, _kept);
    }
}

// Close closes the album; a page's Back goes back to the overview.
FUDE_ROW_CALL(fude_album_row_close, fude_album, fude_album_close)
FUDE_ROW_CALL(fude_album_row_back,  fude_album, fude_album_close_page)

// A sort's button: sorts the overview in order _arg (a FUDE_ALBUM_SORT_) and shows it.
RDE_INTERNAL void fude_album_row_sort(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_album_set_sort((fude_album*)_self, (FUDE_ALBUM_SORT_)_arg);
}

// Exams: shows the exams view.
RDE_INTERNAL void fude_album_row_exams(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_album_set_view((fude_album*)_self, FUDE_ALBUM_VIEW_EXAMS);
}

// "Practice n": opens Practice on the weakest characters, up to FUDE_ALBUM_PRACTICE_MAX, as a set.
RDE_INTERNAL void fude_album_row_weakest(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_ALBUM_PRACTICE_MAX];
    const u32 _n = fude_album_weakest((const fude_album*)_self, _records, FUDE_ALBUM_PRACTICE_MAX);
    fude_study_practice_set(_app, _records, _n);
}

// A page's Practice: opens Practice on the page's character. Practice's Back comes back to the page, which then shows
// the new session.
RDE_INTERNAL void fude_album_row_practice(fude_app* _app, any _self, u32 _arg) {
    const fude_album* _album = (const fude_album*)_self;
    RDE_UNUSED(_arg);
    if(_album->page_open) {
        fude_study_practice(_app, &_album->page_record, 1u);
    }
}

// The album's two rows: the overview's (the sorts first, in FUDE_ALBUM_SORT_ order, so a sort's index is its button's,
// then Exams, Practice and Close) and a character's page's.
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

// Which row is up: the page's while a page is open, else the overview's (also in the exams view).
RDE_INTERNAL u32 fude_album_screen_row(const any _self) {
    return ((const fude_album*)_self)->page_open ? FUDE_ALBUM_ROW_PAGE : FUDE_ALBUM_ROW_OVERVIEW;
}

// How the overview's buttons show: the sort shown (or Exams, in the exams view) is selected, and Practice says how many
// of the weakest it takes, up to ten, and is greyed out when there are none.
RDE_INTERNAL void fude_album_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
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
