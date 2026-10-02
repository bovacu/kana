#include "screens/wordexam.h"
#include "widgets/header.h"
#include "widgets/draw.h"
#include "widgets/icons.h"
#include "handwriting/match.h"
#include "study/review.h"
#include "handwriting/score.h"
#include "services/speech.h"
#include "base/text.h"
#include "base/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See wordexam.h. Drawn by hand like the other exams; a box's answer is read as
// exams read a character (match.h, recognize.h, score.h).
// ===========================================================================

#define KANA_WORDEXAM_MARGIN     24.0f
#define KANA_WORDEXAM_CHIP_H     40.0f
#define KANA_WORDEXAM_CHIP_GAP   8.0f
#define KANA_WORDEXAM_CHIP_PX    13.0f
#define KANA_WORDEXAM_BOX_MAX    220.0f
#define KANA_WORDEXAM_ROW        84.0f
#define KANA_WORDEXAM_CANDIDATES 3u      // right when among this many first candidates (as exams)
#define KANA_WORDEXAM_PASS       0.8f
#define KANA_WORDEXAM_WAIT       4.0     // seconds ML Kit may take on a box before the matcher decides alone
#define KANA_WORDEXAM_PEN_RADIUS (KANA_WORDEXAM_UNITS * KANA_GLYPH_WIDTH / KANA_KANJI_BOX * 0.5f)

// The font drawn with this frame (render's): the boxes' given characters.
static rde_font* kana_wordexam_font    = NULL;
static f32       kana_wordexam_font_px = 1.0f;

#define KANA_WORDEXAM_LENGTHS 4u
static const u32 KANA_WORDEXAM_LENGTH_VALUES[KANA_WORDEXAM_LENGTHS] = { 10u, 20u, 50u, 0u };   // 0: all

RDE_INTERNAL void kana_wordexam_fresh_ink(kana_ink* _ink) {
    kana_ink_init(_ink);
    _ink->constant_radius = KANA_WORDEXAM_PEN_RADIUS;
}

void kana_wordexam_init(kana_wordexam* _exam, const kana_kanji_db* _db, const kana_catalog* _catalog) {
    memset(_exam, 0, sizeof(*_exam));
    _exam->db           = _db;
    _exam->catalog      = _catalog;
    _exam->length       = 1u;   // 20
    _exam->grading_item = UINT32_MAX;
    _exam->tapped       = -1;
    _exam->words        = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_glyph_init(&_exam->glyph, _db);
    for(u32 _i = 0; _i < KANA_WORDEXAM_MAX; _i++) {
        for(u32 _b = 0; _b < KANA_WORDEXAM_CHARS; _b++) {
            kana_wordexam_fresh_ink(&_exam->items[_i].ink[_b]);
        }
    }
}

void kana_wordexam_destroy(kana_wordexam* _exam) {
    for(u32 _i = 0; _i < KANA_WORDEXAM_MAX; _i++) {
        for(u32 _b = 0; _b < KANA_WORDEXAM_CHARS; _b++) {
            kana_ink_destroy(&_exam->items[_i].ink[_b]);
        }
    }
    if(rde_arr_is_inited(&_exam->words)) {
        rde_arr_free(&_exam->words);
    }
    kana_glyph_destroy(&_exam->glyph);
    memset(_exam, 0, sizeof(*_exam));
}

// --- what is asked ---------------------------------------------------------------------

// Is _cp written in a box: a kana or a kanji (what recognition reads)? The rest —
// a letter, a digit, ー, 々 — is given.
RDE_INTERNAL b8 kana_wordexam_written(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu) || (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

// Word _id as an item: its characters (at most KANA_WORDEXAM_CHARS) and their
// records. False when it cannot be asked (gone, too long, nothing to write).
RDE_INTERNAL b8 kana_wordexam_item_set(kana_wordexam* _exam, kana_wordexam_item* _it, u32 _id) {
    const kana_vocab_word* _w = kana_vocab_get(_id);
    if(_w == NULL || _exam->db == NULL) {
        return false;
    }
    u32 _n       = 0;
    b8  _writes  = false;
    for(const c8* _p = _w->written; *_p != 0;) {
        const u32 _cp = kana_kanji_utf8_next(&_p);
        if(_cp == 0u) {
            break;
        }
        if(_n == KANA_WORDEXAM_CHARS) {
            return false;   // too long to ask
        }
        u32 _record;
        _it->chars[_n]   = _cp;
        _it->records[_n] = kana_wordexam_written(_cp) && kana_kanji_find_index(_exam->db, _cp, &_record) ? _record : UINT32_MAX;
        _writes          = _writes || _it->records[_n] != UINT32_MAX;
        _n++;
    }
    if(_n == 0u || !_writes) {
        return false;
    }
    _it->word     = _id;
    _it->count    = _n;
    _it->answered = _it->graded = _it->correct = false;
    _it->points   = 0.0f;
    for(u32 _b = 0; _b < KANA_WORDEXAM_CHARS; _b++) {
        kana_ink_destroy(&_it->ink[_b]);
        kana_wordexam_fresh_ink(&_it->ink[_b]);
        _it->box_graded[_b] = _it->box_right[_b] = false;
        _it->box_points[_b] = 0.0f;
    }
    return true;
}

// The items: _ids in order, those that can be asked, at most _max.
RDE_INTERNAL void kana_wordexam_set_items(kana_wordexam* _exam, const u32* _ids, u32 _count, u32 _max) {
    kana_recognize_forget(&_exam->recognition);
    _exam->grading_item = UINT32_MAX;
    _exam->count        = 0;
    for(u32 _i = 0; _i < _count && _exam->count < _max && _exam->count < KANA_WORDEXAM_MAX; _i++) {
        if(kana_wordexam_item_set(_exam, &_exam->items[_exam->count], _ids[_i])) {
            _exam->count++;
        }
    }
    _exam->current      = 0;
    _exam->stroke_count = 0;
    _exam->pen          = false;
    _exam->kept         = false;
    _exam->spoken       = false;
}

RDE_INTERNAL u32 kana_wordexam_random(kana_wordexam* _exam) {
    u32 _x = _exam->rng != 0 ? _exam->rng : 0x9E3779B9u;
    _x ^= _x << 13;
    _x ^= _x >> 17;
    _x ^= _x << 5;
    _exam->rng = _x;
    return _x;
}

RDE_INTERNAL void kana_wordexam_shuffle(kana_wordexam* _exam, u32* _ids, u32 _n) {
    for(u32 _i = _n; _i > 1u; _i--) {
        const u32 _j = kana_wordexam_random(_exam) % _i;
        const u32 _t = _ids[_i - 1u];
        _ids[_i - 1u] = _ids[_j];
        _ids[_j]      = _t;
    }
}

// Can word _id be asked: there, not too long, something to write?
RDE_INTERNAL b8 kana_wordexam_can_ask(const kana_wordexam* _exam, u32 _id) {
    const kana_vocab_word* _w = kana_vocab_get(_id);
    if(_w == NULL || _exam->db == NULL) {
        return false;
    }
    u32 _n      = 0;
    b8  _writes = false;
    for(const c8* _p = _w->written; *_p != 0 && _n <= KANA_WORDEXAM_CHARS;) {
        const u32 _cp = kana_kanji_utf8_next(&_p);
        u32       _record;
        _writes = _writes || (_cp != 0u && kana_wordexam_written(_cp) && kana_kanji_find_index(_exam->db, _cp, &_record));
        _n++;
    }
    return _n > 0u && _n <= KANA_WORDEXAM_CHARS && _writes;
}

// Of the words it can ask, how many can be.
RDE_INTERNAL u32 kana_wordexam_askable(kana_wordexam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_exam->words); _i++) {
        _n += kana_wordexam_can_ask(_exam, ((const u32*)_exam->words.memory)[_i]) ? 1u : 0u;
    }
    return _n;
}

void kana_wordexam_open(kana_wordexam* _exam, const u32* _ids, u32 _count, const c8* _title) {
    rde_arr_clear(&_exam->words);
    if(_count > 0) {
        memcpy(rde_arr_add_n(&_exam->words, _count), _ids, sizeof(u32) * _count);
    }
    snprintf(_exam->title, sizeof(_exam->title), "%s", _title != NULL ? _title : "");
    _exam->open   = _exam->db != NULL;
    _exam->stage  = KANA_WORDEXAM_SETUP;
    _exam->review = false;
    _exam->rng    = (u32)time(NULL) | 1u;
    _exam->tapped = -1;
    _exam->count  = 0;
    if(!kana_speech_available()) {
        _exam->by = KANA_WORDEXAM_BY_MEANING;
    }
    kana_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

void kana_wordexam_open_review(kana_wordexam* _exam, const u32* _ids, u32 _count) {
    kana_wordexam_open(_exam, _ids, _count, kana_text(KANA_TEXT_REVIEWS));
    _exam->review = true;
    _exam->by     = KANA_WORDEXAM_BY_MEANING;
    kana_wordexam_set_items(_exam, _ids, _count, KANA_WORDEXAM_MAX);   // what is due, in its order
    if(_exam->count > 0) {
        _exam->stage = KANA_WORDEXAM_WRITING;
    }
}

void kana_wordexam_close(kana_wordexam* _exam) {
    kana_recognize_forget(&_exam->recognition);
    _exam->grading_item = UINT32_MAX;
    _exam->open         = false;
    _exam->pen          = false;
}

u32 kana_wordexam_planned(const kana_wordexam* _exam) {
    const u32 _size = kana_wordexam_askable((kana_wordexam*)_exam);
    const u32 _want = KANA_WORDEXAM_LENGTH_VALUES[_exam->length];
    const u32 _n    = _want == 0u || _want > _size ? _size : _want;
    return _n > KANA_WORDEXAM_MAX ? KANA_WORDEXAM_MAX : _n;
}

void kana_wordexam_start(kana_wordexam* _exam) {
    const u32 _n   = (u32)rde_arr_length(&_exam->words);
    u32*      _ids = (u32*)rde_malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
    if(_n > 0) {
        memcpy(_ids, _exam->words.memory, sizeof(u32) * _n);
    }
    kana_wordexam_shuffle(_exam, _ids, _n);
    kana_wordexam_set_items(_exam, _ids, _n, kana_wordexam_planned(_exam));
    rde_free(_ids);
    if(_exam->count > 0) {
        _exam->stage = KANA_WORDEXAM_WRITING;
    }
}

// --- writing ---------------------------------------------------------------------------

RDE_INTERNAL kana_wordexam_item* kana_wordexam_current(kana_wordexam* _exam) {
    return _exam->stage == KANA_WORDEXAM_WRITING && _exam->current < _exam->count ? &_exam->items[_exam->current] : NULL;
}

void kana_wordexam_undo(kana_wordexam* _exam) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_it != NULL && !_exam->pen && _exam->stroke_count > 0) {
        kana_ink_undo(&_it->ink[_exam->strokes[--_exam->stroke_count]]);
    }
}

void kana_wordexam_clear(kana_wordexam* _exam) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        for(u32 _b = 0; _b < _it->count; _b++) {
            kana_ink_destroy(&_it->ink[_b]);
            kana_wordexam_fresh_ink(&_it->ink[_b]);
        }
        _exam->stroke_count = 0;
    }
}

b8 kana_wordexam_at_last(const kana_wordexam* _exam) {
    return _exam->current + 1u >= _exam->count;
}

void kana_wordexam_next(kana_wordexam* _exam) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_it == NULL) {
        return;
    }
    if(_exam->pen) {
        kana_ink_end(&_it->ink[_exam->pen_box]);
        _exam->pen = false;
    }
    _it->answered       = true;
    _exam->stroke_count = 0;
    _exam->spoken       = false;
    if(_exam->current + 1u < _exam->count) {
        _exam->current++;
    } else {
        _exam->stage = KANA_WORDEXAM_RESULTS;
        kana_scroller_stop(&_exam->scroller);
        _exam->scroller.offset = 0.0f;
    }
}

// --- marking ---------------------------------------------------------------------------

// Box _b of _it read: right when its character is among the first candidates
// (_r: ML Kit's answer, or NULL: the matcher alone); its points as written.
RDE_INTERNAL void kana_wordexam_mark_box(kana_wordexam* _exam, kana_wordexam_item* _it, u32 _b, const kana_recognition* _r) {
    _it->box_graded[_b] = true;
    _it->box_right[_b]  = false;
    _it->box_points[_b] = 0.0f;
    if(kana_ink_alive_strokes(&_it->ink[_b]) == 0) {
        return;   // nothing written: wrong
    }
    kana_match_result _matched[16];
    kana_match_result _candidates[8];
    const u32 _m = kana_match_rank(_exam->db, _exam->catalog, KANA_FILTER_ALL, &_it->ink[_b], _matched, 16u);
    const u32 _n = kana_recognize_candidates(_exam->db, _r, 0u, 1u, _exam->catalog, KANA_FILTER_ALL, _matched, _m, _candidates, 8u);
    for(u32 _i = 0; _i < _n && _i < KANA_WORDEXAM_CANDIDATES; _i++) {
        _it->box_right[_b] = _it->box_right[_b] || _candidates[_i].record == _it->records[_b];
    }
    kana_kanji_info _info;
    if(kana_kanji_at(_exam->db, _it->records[_b], &_info)) {
        const kana_score _s = kana_score_drawing(_exam->db, &_info, &_it->ink[_b]);
        _it->box_points[_b] = _s.empty ? 0.0f : _s.score;
    }
}

// Every box read: the word right when each is; its points the written ones' mean.
RDE_INTERNAL void kana_wordexam_finish_item(kana_wordexam_item* _it) {
    b8  _right   = true;
    f32 _points  = 0.0f;
    u32 _written = 0;
    for(u32 _b = 0; _b < _it->count; _b++) {
        if(!_it->box_graded[_b]) {
            return;
        }
        _right = _right && _it->box_right[_b];
        if(_it->records[_b] != UINT32_MAX) {
            _points += _it->box_points[_b];
            _written++;
        }
    }
    _it->graded  = true;
    _it->correct = _right;
    _it->points  = _written > 0 ? _points / (f32)_written : 0.0f;
}

b8 kana_wordexam_graded(const kana_wordexam* _exam) {
    for(u32 _i = 0; _i < _exam->count; _i++) {
        if(!_exam->items[_i].graded) {
            return false;
        }
    }
    return true;
}

u32 kana_wordexam_wrong(const kana_wordexam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _n += _exam->items[_i].graded && !_exam->items[_i].correct ? 1u : 0u;
    }
    return _n;
}

// The answers read a box at a time: ML Kit while it is on and has its model (a
// frame or more later), else the matcher alone.
RDE_INTERNAL void kana_wordexam_grade(kana_wordexam* _exam) {
    const f64 _now = rde_engine_get_time_now();
    if(_exam->grading_item != UINT32_MAX) {
        kana_wordexam_item* _it = &_exam->items[_exam->grading_item];
        if(kana_recognize_poll(&_exam->recognition)) {
            kana_wordexam_mark_box(_exam, _it, _exam->grading_box, &_exam->recognition);
            _exam->grading_item = UINT32_MAX;
        } else if(_now - _exam->grading_since > KANA_WORDEXAM_WAIT) {
            kana_recognize_forget(&_exam->recognition);   // no answer: the matcher decides
            kana_wordexam_mark_box(_exam, _it, _exam->grading_box, NULL);
            _exam->grading_item = UINT32_MAX;
        }
        kana_wordexam_finish_item(_it);
        return;
    }
    for(u32 _i = 0; _i < _exam->count; _i++) {
        kana_wordexam_item* _it = &_exam->items[_i];
        if(!_it->answered || _it->graded) {
            continue;
        }
        for(u32 _b = 0; _b < _it->count; _b++) {
            if(_it->box_graded[_b]) {
                continue;
            }
            if(_it->records[_b] == UINT32_MAX) {
                _it->box_graded[_b] = _it->box_right[_b] = true;   // given
                continue;
            }
            if(kana_ink_alive_strokes(&_it->ink[_b]) > 0 && kana_recognize_available()) {
                if(kana_recognize_start(&_exam->recognition, &_it->ink[_b])) {
                    _exam->grading_item  = _i;
                    _exam->grading_box   = _b;
                    _exam->grading_since = _now;
                }
                return;   // started, or ML Kit busy elsewhere: a later frame
            }
            kana_wordexam_mark_box(_exam, _it, _b, NULL);
            kana_wordexam_finish_item(_it);
            return;   // one a frame: the matcher takes a moment
        }
        kana_wordexam_finish_item(_it);
    }
}

void kana_wordexam_retry_wrong(kana_wordexam* _exam) {
    if(!kana_wordexam_graded(_exam)) {
        return;
    }
    u32 _ids[KANA_WORDEXAM_MAX];
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        if(!_exam->items[_i].correct) {
            _ids[_n++] = _exam->items[_i].word;
        }
    }
    if(_n == 0) {
        return;
    }
    kana_wordexam_shuffle(_exam, _ids, _n);
    kana_wordexam_set_items(_exam, _ids, _n, KANA_WORDEXAM_MAX);
    _exam->stage = KANA_WORDEXAM_WRITING;
}

b8 kana_wordexam_take_tap(kana_wordexam* _exam, u32* _word) {
    if(_exam->tapped < 0) {
        return false;
    }
    *_word        = _exam->items[_exam->tapped].word;
    _exam->tapped = -1;
    return true;
}

// --- the pointer -----------------------------------------------------------------------

RDE_INTERNAL b8 kana_wordexam_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

// The box under _screen (writing), or UINT32_MAX.
RDE_INTERNAL u32 kana_wordexam_box_at(const kana_wordexam* _exam, const kana_wordexam_item* _it, rde_vec_2F _screen) {
    for(u32 _b = 0; _b < _it->count && _exam->box > 0.0f; _b++) {
        const f32 _x = _exam->boxes_tl.x + (f32)_b * (_exam->box + _exam->box_gap);
        if(_screen.x >= _x && _screen.x <= _x + _exam->box && _screen.y <= _exam->boxes_tl.y && _screen.y >= _exam->boxes_tl.y - _exam->box) {
            return _b;
        }
    }
    return UINT32_MAX;
}

RDE_INTERNAL rde_vec_2F kana_wordexam_local(const kana_wordexam* _exam, u32 _b, rde_vec_2F _screen) {
    const f32 _k = KANA_WORDEXAM_UNITS / _exam->box;
    const f32 _x = _exam->boxes_tl.x + (f32)_b * (_exam->box + _exam->box_gap);
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _x) * _k, 0.0f, KANA_WORDEXAM_UNITS),
        rde_math_clamp_f32((_screen.y - (_exam->boxes_tl.y - _exam->box)) * _k, 0.0f, KANA_WORDEXAM_UNITS)
    };
}

void kana_wordexam_pointer_down(kana_wordexam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_it != NULL) {
        if(kana_wordexam_inside(_screen, _exam->speaker_min, _exam->speaker_max)) {
            const kana_vocab_word* _w = kana_vocab_get(_it->word);
            if(_w != NULL) {
                kana_speak(_w->reading[0] != 0 ? _w->reading : _w->written);   // again
            }
            return;
        }
        // Writing: only the pen, only in a box (a resting hand does nothing).
        const u32 _b = kana_wordexam_box_at(_exam, _it, _screen);
        if(_pen && _b != UINT32_MAX && _it->records[_b] != UINT32_MAX) {
            _it->ink[_b].zoom = _exam->box / KANA_WORDEXAM_UNITS;
            kana_ink_begin(&_it->ink[_b], kana_wordexam_local(_exam, _b, _screen), true, false);
            _exam->pen     = true;
            _exam->pen_box = _b;
        }
        return;
    }
    kana_scroller_down(&_exam->scroller, _screen, _time);
}

void kana_wordexam_pointer_moved(kana_wordexam* _exam, rde_vec_2F _screen, f64 _time) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_exam->pen && _it != NULL) {
        kana_ink_extend(&_it->ink[_exam->pen_box], kana_wordexam_local(_exam, _exam->pen_box, _screen));
        return;
    }
    kana_scroller_moved(&_exam->scroller, _screen, _time);
}

void kana_wordexam_pointer_up(kana_wordexam* _exam, f64 _time) {
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_exam->pen) {
        if(_it != NULL) {
            kana_ink_end(&_it->ink[_exam->pen_box]);
            if(_exam->stroke_count < sizeof(_exam->strokes) / sizeof(_exam->strokes[0])) {
                _exam->strokes[_exam->stroke_count++] = _exam->pen_box;
            }
        }
        _exam->pen = false;
        return;
    }
    kana_scroller_up(&_exam->scroller, _time);
}

RDE_INTERNAL void kana_wordexam_tap(kana_wordexam* _exam, rde_vec_2F _at) {
    if(_exam->stage == KANA_WORDEXAM_SETUP) {
        for(u32 _i = 0; _i < _exam->chip_count; _i++) {
            const kana_wordexam_chip* _c = &_exam->chips[_i];
            if(kana_wordexam_inside(_at, _c->min, _c->max)) {
                if(_c->kind == 0u) { _exam->length = _c->value; }
                else               { _exam->by = (KANA_WORDEXAM_BY_)_c->value; }
            }
        }
    } else if(_exam->stage == KANA_WORDEXAM_RESULTS && kana_wordexam_inside(_at, _exam->rows_min, _exam->rows_max)) {
        const i32 _i = (i32)floorf((_exam->rows_max.y - _at.y + _exam->scroller.offset) / KANA_WORDEXAM_ROW);
        if(_i >= 0 && _i < (i32)_exam->count) {
            _exam->tapped = _i;
        }
    }
}

void kana_wordexam_update(kana_wordexam* _exam, f32 _dt) {
    if(!_exam->open) {
        return;
    }
    kana_scroller_update(&_exam->scroller, _dt, _exam->content_h, _exam->rows_max.y - _exam->rows_min.y);
    rde_vec_2F _at;
    if(kana_scroller_take_tap(&_exam->scroller, &_at)) {
        kana_wordexam_tap(_exam, _at);
    }
    // By ear: each word said as it comes up.
    kana_wordexam_item* _it = kana_wordexam_current(_exam);
    if(_it != NULL && _exam->by == KANA_WORDEXAM_BY_EAR && !_exam->spoken) {
        const kana_vocab_word* _w = kana_vocab_get(_it->word);
        if(_w != NULL) {
            kana_speak(_w->reading[0] != 0 ? _w->reading : _w->written);
        }
        _exam->spoken = true;
    }
    kana_wordexam_grade(_exam);
    // All read: every word's review moves (review.h).
    if(_exam->stage == KANA_WORDEXAM_RESULTS && !_exam->kept && _exam->count > 0 && kana_wordexam_graded(_exam)) {
        for(u32 _i = 0; _i < _exam->count; _i++) {
            kana_reviews_answer(KANA_VOCAB_KEY(_exam->items[_i].word), _exam->items[_i].correct, _exam->items[_i].points / 100.0f);
        }
        _exam->kept = true;
    }
}

// --- drawing ---------------------------------------------------------------------------

RDE_INTERNAL void kana_wordexam_chip_draw(rde_font* _font, f32 _font_px, rde_vec_2F _min, rde_vec_2F _max, const c8* _label, b8 _chosen) {
    const kana_theme* _theme = kana_theme_active();
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f }, (rde_vec_2F){ _max.x - _min.x, _max.y - _min.y }, 1.0f, 10,
                                            _chosen ? _theme->accent : _theme->surface_2, NULL);
    const f32 _w = kana_draw_text_width(_font, _font_px, _label, KANA_WORDEXAM_CHIP_PX);
    kana_draw_text(_font, _font_px, _label, (_min.x + _max.x - _w) * 0.5f, (_min.y + _max.y) * 0.5f - KANA_WORDEXAM_CHIP_PX * 0.42f, KANA_WORDEXAM_CHIP_PX,
                   _chosen ? _theme->on_accent : _theme->text);
}

RDE_INTERNAL void kana_wordexam_render_setup(kana_wordexam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top) {
    const kana_theme* _theme = kana_theme_active();
    c8 _title[128];
    KANA_TEXTF(_title, KANA_TEXT_WORDEXAM_TITLE_OF, KANA_TS(_exam->title));
    kana_header_draw(&_exam->glyph, 0x8A9Eu, _font, _font_px, _left, _right, _top, _title, kana_text(KANA_TEXT_WORDEXAM_INTRO), NULL);
    f32 _y = _top - 56.0f;
    _exam->chip_count = 0;
    const u32 _size   = kana_wordexam_askable(_exam);
    c8        _label[64];
    for(u32 _group = 0; _group < 2u; _group++) {
        if(_group == 1u && !kana_speech_available()) {
            break;   // no voice: by meaning only
        }
        _y -= 30.0f;
        kana_draw_text(_font, _font_px, kana_text(_group == 0u ? KANA_TEXT_EXAM_HOW_MANY : KANA_TEXT_WORDEXAM_BY), _left, _y, KANA_HEADER_CAPTION_PX, _theme->text_soft);
        _y -= 12.0f;
        f32       _x = _left;
        const u32 _n = _group == 0u ? KANA_WORDEXAM_LENGTHS : 2u;
        for(u32 _i = 0; _i < _n; _i++) {
            b8 _chosen;
            if(_group == 0u) {
                if(KANA_WORDEXAM_LENGTH_VALUES[_i] == 0u) {
                    snprintf(_label, sizeof(_label), "%s  %u", kana_text(KANA_TEXT_ALL), _size);
                } else {
                    snprintf(_label, sizeof(_label), "%u", KANA_WORDEXAM_LENGTH_VALUES[_i]);
                }
                _chosen = _exam->length == _i;
            } else {
                snprintf(_label, sizeof(_label), "%s %s", _i == 0u ? KANA_ICON_TEXT_COPY : KANA_ICON_LISTEN,
                         kana_text(_i == 0u ? KANA_TEXT_WORDEXAM_BY_MEANING : KANA_TEXT_WORDEXAM_BY_EAR));
                _chosen = (u32)_exam->by == _i;
            }
            const f32 _w = fmaxf(64.0f, kana_draw_text_width(_font, _font_px, _label, KANA_WORDEXAM_CHIP_PX) + 36.0f);
            if(_x + _w > _right && _x > _left) {
                _x  = _left;
                _y -= KANA_WORDEXAM_CHIP_H + KANA_WORDEXAM_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - KANA_WORDEXAM_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            kana_wordexam_chip_draw(_font, _font_px, _min, _max, _label, _chosen);
            if(_exam->chip_count < 8u) {
                _exam->chips[_exam->chip_count++] = (kana_wordexam_chip){ _min, _max, (u8)_group, (u8)_i };
            }
            _x += _w + KANA_WORDEXAM_CHIP_GAP;
        }
        _y -= KANA_WORDEXAM_CHIP_H;
    }
    // What that makes.
    _y -= 24.0f;
    kana_draw_card((rde_vec_2F){ _left, _y - 76.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
    c8 _line[192];
    if(_size == 0u) {
        snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_WORDEXAM_NONE));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_WORDEXAM_PLAN, KANA_TN(kana_wordexam_planned(_exam)), KANA_TN(_size));
    }
    kana_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 32.0f, kana_draw_text_px_to_fit(_font, _font_px, _line, 15.0f, _right - _left - 36.0f, 0.6f), _theme->text);
    snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_WORDEXAM_RULE));
    kana_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 56.0f, kana_draw_text_px_to_fit(_font, _font_px, _line, 12.0f, _right - _left - 36.0f, 0.6f), _theme->text_soft);
}

// A word's boxes from _tl, each _box with _gap between: the writing (or, given,
// the character), and — when _verdicts — each box's verdict in its corner.
RDE_INTERNAL void kana_wordexam_draw_boxes(kana_wordexam* _exam, rde_window* _window, kana_wordexam_item* _it, rde_vec_2F _tl, f32 _box, f32 _gap, b8 _verdicts) {
    const kana_theme* _theme = kana_theme_active();
    const rde_vec_2I  _size  = rde_window_get_size(_window);
    for(u32 _b = 0; _b < _it->count; _b++) {
        const rde_vec_2F _btl = { _tl.x + (f32)_b * (_box + _gap), _tl.y };
        kana_glyph_box(_btl, _box);
        if(_it->records[_b] == UINT32_MAX) {
            // Given: the character as text, grey.
            c8 _ch[8];
            kana_kanji_utf8(_it->chars[_b], _ch);
            const f32 _px = _box * 0.5f;
            kana_draw_text(kana_wordexam_font, kana_wordexam_font_px, _ch, _btl.x + (_box - kana_draw_text_width(kana_wordexam_font, kana_wordexam_font_px, _ch, _px)) * 0.5f,
                           _btl.y - _box * 0.5f - _px * 0.38f, _px, _theme->text_soft);
            continue;
        }
        kana_ink_render(&_it->ink[_b], (rde_vec_2F){ _btl.x, _btl.y - _box }, _box / KANA_WORDEXAM_UNITS, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f },
                        rde_engine_get_time_now(), false);
        if(_verdicts && _it->box_graded[_b]) {
            const f32 _r = fmaxf(6.0f, _box * 0.1f);
            kana_draw_verdict((rde_vec_2F){ _btl.x + _box - _r - 3.0f, _btl.y - _r - 3.0f }, _r, _it->box_right[_b]);
        }
    }
}

RDE_INTERNAL void kana_wordexam_render_writing(kana_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme*   _theme = kana_theme_active();
    kana_wordexam_item* _it    = kana_wordexam_current(_exam);
    _exam->speaker_min = _exam->speaker_max = (rde_vec_2F){ 0.0f, 0.0f };
    if(_it == NULL) {
        return;
    }
    const kana_vocab_word* _w = kana_vocab_get(_it->word);
    c8 _title[128], _line[64];
    KANA_TEXTF(_title, KANA_TEXT_WORDEXAM_TITLE_OF, KANA_TS(_exam->title));
    KANA_TEXTF(_line, KANA_TEXT_OF_N, KANA_TN(_exam->current + 1u), KANA_TN(_exam->count));
    kana_header_draw(&_exam->glyph, 0x8A9Eu, _font, _font_px, _left, _right, _top, _title, kana_text(KANA_TEXT_WORDEXAM_INTRO), _line);
    // Progress: a segment per word.
    const f32 _py  = _top - 54.0f;
    const f32 _gap = 3.0f;
    const f32 _sw  = (_right - _left - _gap * (f32)(_exam->count - 1u)) / (f32)_exam->count;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        const rde_color _c = _i < _exam->current ? _theme->accent : _i == _exam->current ? _theme->tint : _theme->surface_2;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _left + (f32)_i * (_sw + _gap) + _sw * 0.5f, _py }, (rde_vec_2F){ _sw, 6.0f }, 1.0f, 4, _c, NULL);
    }

    // What to write: its meaning and reading — or, by ear, a speaker and its meaning.
    f32 _y = _py - 24.0f;
    if(_w != NULL) {
        if(_exam->by == KANA_WORDEXAM_BY_EAR) {
            const f32 _r = 34.0f;
            const rde_vec_2F _c = { (_left + _right) * 0.5f, _y - _r };
            rde_rendering_2d_draw_circle(_c, _r, 40, _theme->accent, NULL);
            kana_draw_icon(_font, _font_px, KANA_ICON_SPEAK, _c, 30.0f, _theme->on_accent);
            _exam->speaker_min = (rde_vec_2F){ _c.x - _r, _c.y - _r };
            _exam->speaker_max = (rde_vec_2F){ _c.x + _r, _c.y + _r };
            _y -= 2.0f * _r + 12.0f;
            const c8* _again = kana_text(KANA_TEXT_WORDEXAM_HEAR_AGAIN);
            kana_draw_text(_font, _font_px, _again, (_left + _right - kana_draw_text_width(_font, _font_px, _again, 11.0f)) * 0.5f, _y - 8.0f, 11.0f, _theme->text_soft);
            _y -= 24.0f;
        }
        const u32 _lines = kana_draw_text_wrap_lines(_font, _font_px, _w->meaning, 22.0f, _right - _left);
        kana_draw_text_wrap(_font, _font_px, _w->meaning[0] != 0 ? _w->meaning : "", _left, _y - 22.0f, 22.0f, _right - _left, 30.0f, _theme->text);
        _y -= (f32)(_lines > 0 ? _lines : 1u) * 30.0f + 4.0f;
        if(_exam->by == KANA_WORDEXAM_BY_MEANING) {
            kana_draw_text(_font, _font_px, _w->reading, _left, _y - 20.0f, 20.0f, _theme->accent);
            _y -= 34.0f;
        }
    }

    // The boxes, a character each, centred; as big as fits.
    const f32 _room = _y - 16.0f - (_bottom + 30.0f);
    const f32 _gap2 = 10.0f;
    const f32 _box  = fmaxf(48.0f, fminf(fminf(KANA_WORDEXAM_BOX_MAX, _room), (_right - _left - _gap2 * (f32)(_it->count - 1u)) / (f32)_it->count));
    const f32 _all  = _box * (f32)_it->count + _gap2 * (f32)(_it->count - 1u);
    _exam->boxes_tl = (rde_vec_2F){ (_left + _right - _all) * 0.5f, _y - 16.0f };
    _exam->box      = _box;
    _exam->box_gap  = _gap2;
    kana_wordexam_draw_boxes(_exam, _window, _it, _exam->boxes_tl, _box, _gap2, false);
    kana_draw_text(_font, _font_px, kana_text(kana_wordexam_at_last(_exam) ? KANA_TEXT_WORDEXAM_THEN_FINISH : KANA_TEXT_WORDEXAM_THEN_NEXT),
                   _exam->boxes_tl.x, _exam->boxes_tl.y - _box - 20.0f, 12.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_wordexam_render_results(kana_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    c8 _line[192], _title[128];
    KANA_TEXTF(_title, KANA_TEXT_WORDEXAM_TITLE_OF, KANA_TS(_exam->title));
    if(!kana_wordexam_graded(_exam)) {
        u32 _graded = 0;
        for(u32 _i = 0; _i < _exam->count; _i++) {
            _graded += _exam->items[_i].graded ? 1u : 0u;
        }
        KANA_TEXTF(_line, KANA_TEXT_EXAM_READING_ANSWERS, KANA_TN(_graded), KANA_TN(_exam->count));
        kana_header_draw(&_exam->glyph, 0x8A9Eu, _font, _font_px, _left, _right, _top, _title, _line, NULL);
    } else {
        u32 _right_n = 0;
        f32 _points  = 0.0f;
        for(u32 _i = 0; _i < _exam->count; _i++) {
            _right_n += _exam->items[_i].correct ? 1u : 0u;
            _points  += _exam->items[_i].correct ? _exam->items[_i].points : 0.0f;
        }
        KANA_TEXTF(_line, KANA_TEXT_EXAM_RESULT, KANA_TN(_right_n), KANA_TN(_exam->count), KANA_TN(lroundf(_points / (f32)_exam->count)));
        kana_header_draw(&_exam->glyph, 0x8A9Eu, _font, _font_px, _left, _right, _top, _title, _line, NULL);
        const b8 _passed = (f32)_right_n >= KANA_WORDEXAM_PASS * (f32)_exam->count;
        const c8* _verdict = kana_text(_passed ? KANA_TEXT_EXAM_PASSED : KANA_TEXT_EXAM_NOT_PASSED);
        const f32 _vw = kana_draw_text_width(_font, _font_px, _verdict, 12.0f) + 22.0f;
        kana_draw_chip(_font, _font_px, _verdict, _right - _vw, _top - 20.0f, 12.0f, _passed ? _theme->score_good : _theme->score_poor, _theme->on_accent);
    }

    // A row a word: what was written, then the word and how it went.
    const f32 _rows_top = _top - 60.0f;
    _exam->rows_min  = (rde_vec_2F){ _left, _bottom };
    _exam->rows_max  = (rde_vec_2F){ _right, _rows_top };
    _exam->content_h = (f32)_exam->count * KANA_WORDEXAM_ROW;
    if(_rows_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_rows_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_rows_top - _bottom) });
    for(u32 _i = 0; _i < _exam->count; _i++) {
        const f32 _row_top = _rows_top - (f32)_i * KANA_WORDEXAM_ROW + _exam->scroller.offset;
        if(_row_top - KANA_WORDEXAM_ROW > _rows_top || _row_top < _bottom) {
            continue;
        }
        kana_wordexam_item*    _it  = &_exam->items[_i];
        const kana_vocab_word* _w   = kana_vocab_get(_it->word);
        const f32              _box = 64.0f;
        const f32              _mid = _row_top - KANA_WORDEXAM_ROW * 0.5f;
        kana_wordexam_draw_boxes(_exam, _window, _it, (rde_vec_2F){ _left, _mid + _box * 0.5f }, _box, 4.0f, true);
        const f32 _tx = _left + (f32)_it->count * (_box + 4.0f) + 16.0f;
        if(_w != NULL) {
            const f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _w->written, 20.0f, _right - 60.0f - _tx, 0.5f);
            kana_draw_text(_font, _font_px, _w->written, _tx, _mid + 4.0f, _px, _theme->ink);
            snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _w->reading, _w->meaning);   // ·
            kana_draw_text(_font, _font_px, _line, _tx, _mid - 18.0f, kana_draw_text_px_to_fit(_font, _font_px, _line, 12.0f, _right - 60.0f - _tx, 0.6f), _theme->text_soft);
        }
        if(_it->graded) {
            kana_draw_verdict((rde_vec_2F){ _right - 22.0f, _mid }, 14.0f, _it->correct);
        } else {
            rde_rendering_2d_draw_circle_border((rde_vec_2F){ _right - 22.0f, _mid }, 14.0f, 1.5f, 24, _theme->text_soft, NULL);
        }
        if(_i + 1u < _exam->count) {
            kana_draw_line((rde_vec_2F){ _left, _row_top - KANA_WORDEXAM_ROW }, (rde_vec_2F){ _right, _row_top - KANA_WORDEXAM_ROW }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}

void kana_wordexam_render(kana_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_exam->open) {
        return;
    }
    kana_wordexam_font    = _font;
    kana_wordexam_font_px = _font_px;
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_WORDEXAM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_WORDEXAM_MARGIN;
    switch(_exam->stage) {
        case KANA_WORDEXAM_SETUP:
            kana_wordexam_render_setup(_exam, _font, _font_px, _left, _right, _top);
            _exam->rows_min = _exam->rows_max = (rde_vec_2F){ 0.0f, 0.0f };
            _exam->content_h = 0.0f;
            break;
        case KANA_WORDEXAM_WRITING: kana_wordexam_render_writing(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case KANA_WORDEXAM_RESULTS: kana_wordexam_render_results(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
    }
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "app/app.h"
#include "widgets/wordcard.h"

KANA_SCREEN_ADAPTERS_PEN(kana_wordexam, kana_wordexam)
KANA_SCREEN_RENDER(kana_wordexam, kana_wordexam)

// Its answers read as it goes; a word tapped in the results opens the word card.
RDE_INTERNAL void kana_wordexam_screen_update(kana_app* _app, void* _self, f32 _dt) {
    kana_wordexam* _exam = (kana_wordexam*)_self;
    RDE_UNUSED(_app);
    kana_wordexam_update(_exam, _dt);
    u32 _word;
    if(kana_wordexam_take_tap(_exam, &_word)) {
        kana_wordcard_ask_saved(_word);
    }
}

KANA_ROW_CALL(kana_wordexam_row_close, kana_wordexam, kana_wordexam_close)
KANA_ROW_CALL(kana_wordexam_row_start, kana_wordexam, kana_wordexam_start)
KANA_ROW_CALL(kana_wordexam_row_undo,  kana_wordexam, kana_wordexam_undo)
KANA_ROW_CALL(kana_wordexam_row_clear, kana_wordexam, kana_wordexam_clear)
KANA_ROW_CALL(kana_wordexam_row_next,  kana_wordexam, kana_wordexam_next)
KANA_ROW_CALL(kana_wordexam_row_retry, kana_wordexam, kana_wordexam_retry_wrong)

// One row per stage (KANA_WORDEXAM_ order): its setup's, writing's, results'.
enum { KANA_WORDEXAM_SETUP_START = 1 };
enum { KANA_WORDEXAM_WRITE_NEXT = 3 };
enum { KANA_WORDEXAM_RESULTS_RETRY = 1 };
static const kana_row_button KANA_WORDEXAM_SETUP_ROW[] = {
    { KANA_TEXT_CLOSE,   KANA_ICON_CLOSE,       kana_wordexam_row_close, 0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_START_N, KANA_ICON_ARROW_RIGHT, kana_wordexam_row_start, 0, KANA_ROW_PRIMARY, true,  NULL },
};
static const kana_row_button KANA_WORDEXAM_WRITE_ROW[] = {
    { KANA_TEXT_QUIT,  KANA_ICON_CLOSE,       kana_wordexam_row_close, 0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_UNDO,  KANA_ICON_UNDO,        kana_wordexam_row_undo,  0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_CLEAR, KANA_ICON_TRASH,       kana_wordexam_row_clear, 0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_NEXT,  KANA_ICON_ARROW_RIGHT, kana_wordexam_row_next,  0, KANA_ROW_PRIMARY, false, NULL },
};
static const kana_row_button KANA_WORDEXAM_RESULTS_ROW[] = {
    { KANA_TEXT_DONE,        KANA_ICON_CHECK, kana_wordexam_row_close, 0, KANA_ROW_QUIET, false, NULL },
    { KANA_TEXT_RETRY_WRONG, KANA_ICON_RETRY, kana_wordexam_row_retry, 0, KANA_ROW_QUIET, false, NULL },
};
static const kana_row_def KANA_WORDEXAM_BUTTON_ROWS[] = { KANA_ROW_DEF(KANA_WORDEXAM_SETUP_ROW), KANA_ROW_DEF(KANA_WORDEXAM_WRITE_ROW), KANA_ROW_DEF(KANA_WORDEXAM_RESULTS_ROW) };

RDE_INTERNAL u32 kana_wordexam_screen_row(const void* _self) {
    return (u32)((const kana_wordexam*)_self)->stage;
}

// Start n (the words planned), Next or Finish, Retry the wrong ones when there are.
RDE_INTERNAL void kana_wordexam_screen_faces(const void* _self, u32 _row, kana_row_face* _faces) {
    const kana_wordexam* _exam = (const kana_wordexam*)_self;
    if(_row == KANA_WORDEXAM_SETUP) {
        const u32 _planned = kana_wordexam_planned(_exam);
        kana_row_face_count(&_faces[KANA_WORDEXAM_SETUP_START], KANA_TEXT_START_N, _planned);
        _faces[KANA_WORDEXAM_SETUP_START].disabled = _planned == 0u;
    } else if(_row == KANA_WORDEXAM_WRITING) {
        kana_row_face_next(&_faces[KANA_WORDEXAM_WRITE_NEXT], kana_wordexam_at_last(_exam));
    } else {
        _faces[KANA_WORDEXAM_RESULTS_RETRY].disabled = !kana_wordexam_graded(_exam) || kana_wordexam_wrong(_exam) == 0u;
    }
}

const kana_screen KANA_WORDEXAM_SCREEN = {
    .name = "wordexam", .input = KANA_SCREEN_INPUT_POINT,
    .is_open = kana_wordexam_screen_is_open, .close = kana_wordexam_screen_close,
    .update = kana_wordexam_screen_update, .render = kana_wordexam_screen_render,
    .pointer_down = kana_wordexam_screen_down, .pointer_moved = kana_wordexam_screen_moved, .pointer_up = kana_wordexam_screen_up,
    .rows = KANA_WORDEXAM_BUTTON_ROWS, .row_count = 3u, .row = kana_wordexam_screen_row, .faces = kana_wordexam_screen_faces,
    .field_hint = KANA_TEXT_COUNT,
};
