// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/wordexam.h"
#include "drawing/base/redraw.h"
#include "study/screens/exam.h"   // FUDE_EXAM_CANDIDATES: a box is marked the way an exam's answer is.
#include "lang/lang.h"
#include "study/widgets/header.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "study/handwriting/match.h"
#include "study/models/review.h"
#include "study/handwriting/score.h"
#include "study/services/speech.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"
#include "drawing/base/theme.h"


// ===========================================================================
// See wordexam.h.
//
// The screen is drawn by hand like the other exams, and a box's answer is read the way exams read a character
// (match.h, recognize.h, score.h). The items are a fixed array of FUDE_WORDEXAM_MAX, each with an ink per box, made
// once in fude_wordexam_init and emptied again for each exam. Before a word is asked, its written form is split into
// boxes (fude_wordexam_boxes): a letter with the signs written on it is one box, and a letter takes the form it has
// there (lang.h: fude_lang_form). The setup counts the words that can be asked with the same split, so the two never
// disagree. The answers are read one box at a time, from fude_wordexam_update.
// ===========================================================================

// The layout's sizes, in screen points (MARGIN from the screen's sides; a chip; a box at its largest; a results row).
#define FUDE_WORDEXAM_MARGIN     24.0f
#define FUDE_WORDEXAM_CHIP_H     40.0f
#define FUDE_WORDEXAM_CHIP_GAP   8.0f
#define FUDE_WORDEXAM_CHIP_PX    13.0f
#define FUDE_WORDEXAM_BOX_MAX    220.0f
#define FUDE_WORDEXAM_ROW        84.0f
#define FUDE_WORDEXAM_WAIT       4.0     // The seconds ML Kit may take on a box before the matcher decides alone.
// The pen's radius, in a box ink's units: the model's stroke width scaled from KanjiVG's box to the box's side.
#define FUDE_WORDEXAM_PEN_RADIUS (FUDE_WORDEXAM_UNITS * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f)

// The lengths to choose from; 0 means all of them.
#define FUDE_WORDEXAM_LENGTHS 4u
static const u32 FUDE_WORDEXAM_LENGTH_VALUES[FUDE_WORDEXAM_LENGTHS] = { 10u, 20u, 50u, 0u };   // 0 is all.

// Makes an empty box ink, drawn at the exam pen's fixed width.
RDE_INTERNAL void fude_wordexam_fresh_ink(fude_ink* _ink) {
    fude_ink_init(_ink);
    _ink->constant_radius = FUDE_WORDEXAM_PEN_RADIUS;
}

// Sets the word exam up closed, 20 words long, with an empty ink for every box of every item.
void fude_wordexam_init(fude_wordexam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    rde_memset(_exam, 0, sizeof(*_exam));
    _exam->db           = _db;
    _exam->catalog      = _catalog;
    _exam->length       = 1u;   // The second length: 20.
    _exam->grading_item = UINT32_MAX;
    _exam->tapped       = -1;
    _exam->words        = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _exam->strokes      = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_glyph_init(&_exam->glyph, _db);
    for(u32 _i = 0; _i < FUDE_WORDEXAM_MAX; _i++) {
        for(u32 _b = 0; _b < FUDE_WORDEXAM_CHARS; _b++) {
            fude_wordexam_fresh_ink(&_exam->items[_i].ink[_b]);
        }
    }
}

// Frees every box's ink, the words, the strokes' boxes and the glyph cache, and zeroes the exam.
void fude_wordexam_destroy(fude_wordexam* _exam) {
    for(u32 _i = 0; _i < FUDE_WORDEXAM_MAX; _i++) {
        for(u32 _b = 0; _b < FUDE_WORDEXAM_CHARS; _b++) {
            fude_ink_destroy(&_exam->items[_i].ink[_b]);
        }
    }
    if(rde_arr_is_inited(&_exam->words)) {
        rde_arr_free(&_exam->words);
    }
    if(rde_arr_is_inited(&_exam->strokes)) {
        rde_arr_free(&_exam->strokes);
    }
    fude_glyph_destroy(&_exam->glyph);
    rde_memset(_exam, 0, sizeof(*_exam));
}

// --- What is asked ---------------------------------------------------------------------------------------------------

// Returns whether _cp is written in a box: a CJK ideograph, or a character of one of the language's groups (lang.h: a
// kana, a hangul syllable, a Thai or Devanagari letter; what recognition reads). The rest (a Latin letter, a digit,
// punctuation, Japanese's ー and 々) is given.
RDE_INTERNAL b8 fude_wordexam_written(u32 _cp) {
    return fude_utf8_is_han(_cp) || fude_lang_group(_cp) != FUDE_LANG_NO_GROUP;
}

// Splits word _id into the boxes it is asked in, at most FUDE_WORDEXAM_CHARS: a letter with the signs written on it is
// one box, and a letter takes the form it has there (lang.h: fude_lang_form; Arabic's joined forms, and ل and ا as one,
// ﻻ). When _it is not NULL, each box's text, letter and record go into it. Returns how many boxes, or 0 when the word
// cannot be asked: it is gone, needs more boxes than there are, has a box whose text is too long, or has nothing to
// write. Asking a word (fude_wordexam_item_set) and counting the words that can be (fude_wordexam_can_ask) both split
// with it, so they agree.
RDE_INTERNAL u32 fude_wordexam_boxes(const fude_wordexam* _exam, u32 _id, fude_wordexam_item* _it) {
    const fude_vocab_word* _w = fude_vocab_get(_id);
    if(_w == NULL || _exam->db == NULL) {
        return 0u;
    }
    // Its letters, each with the signs written on it (Thai's and Hindi's vowel signs and tone marks, Arabic's
    // harakat), since a box holds a syllable. One letter more than the boxes is allowed, as two may join into one box.
    u32       _cps[FUDE_WORDEXAM_CHARS + 1u];
    const c8* _from[FUDE_WORDEXAM_CHARS + 1u];
    const c8* _to[FUDE_WORDEXAM_CHARS + 1u];
    u32       _letters = 0;
    for(const c8* _p = _w->written; *_p != 0;) {
        const c8* _was = _p;
        const u32 _cp  = fude_utf8_next(&_p);
        if(_cp == 0u) {
            break;
        }
        if(_letters > 0u && fude_lang_combining(_cp)) {
            _to[_letters - 1u] = _p;
            continue;
        }
        if(_letters == FUDE_WORDEXAM_CHARS + 1u) {
            return 0u;   // Too long to ask.
        }
        _cps[_letters]  = _cp;
        _from[_letters] = _was;
        _to[_letters]   = _p;
        _letters++;
    }
    // Each box: the letter in the form it is written there (lang.h; Arabic's joined forms, and ل and ا as one, ﻻ),
    // and its text, which is its letters and signs as typed.
    u32 _n      = 0;
    b8  _writes = false;
    for(u32 _l = 0; _l < _letters; _l++) {
        if(_n == FUDE_WORDEXAM_CHARS) {
            return 0u;   // Too long to ask.
        }
        b8        _with_after = false;
        const u32 _cp         = fude_lang_form(_l > 0u ? _cps[_l - 1u] : 0u, _cps[_l], _l + 1u < _letters ? _cps[_l + 1u] : 0u, &_with_after);
        const c8* _end        = _with_after ? _to[_l + 1u] : _to[_l];
        const usize _len      = (usize)(_end - _from[_l]);
        if(_len >= sizeof(_it->written[0])) {   // (sizeof reads no item, so _it may be NULL here.)
            return 0u;
        }
        u32       _found;
        const u32 _record = fude_wordexam_written(_cp) && fude_kanji_find_index(_exam->db, _cp, &_found) ? _found : UINT32_MAX;
        _writes           = _writes || _record != UINT32_MAX;
        if(_it != NULL) {
            rde_memcpy(_it->written[_n], _from[_l], _len);
            _it->written[_n][_len] = 0;
            _it->chars[_n]         = _cp;
            _it->records[_n]       = _record;
        }
        _n++;
        _l += _with_after ? 1u : 0u;
    }
    return _writes ? _n : 0u;
}

// Makes word _id the item _it: its boxes (fude_wordexam_boxes), each one's text and letter and record, and empty inks.
// Returns false when it cannot be asked: the word is gone, too long, or has nothing to write.
RDE_INTERNAL b8 fude_wordexam_item_set(fude_wordexam* _exam, fude_wordexam_item* _it, u32 _id) {
    const u32 _n = fude_wordexam_boxes(_exam, _id, _it);
    if(_n == 0u) {
        return false;
    }
    _it->word     = _id;
    _it->count    = _n;
    _it->answered = _it->graded = _it->correct = false;
    _it->points   = 0.0f;
    for(u32 _b = 0; _b < FUDE_WORDEXAM_CHARS; _b++) {
        fude_ink_destroy(&_it->ink[_b]);
        fude_wordexam_fresh_ink(&_it->ink[_b]);
        _it->box_graded[_b] = _it->box_right[_b] = false;
        _it->box_points[_b] = 0.0f;
    }
    return true;
}

// Makes the items from _ids, in order, keeping those that can be asked, at most _max (and FUDE_WORDEXAM_MAX), and
// starts from the first word with nothing being read.
RDE_INTERNAL void fude_wordexam_set_items(fude_wordexam* _exam, const u32* _ids, u32 _count, u32 _max) {
    fude_recognize_forget(&_exam->recognition);
    _exam->grading_item = UINT32_MAX;
    _exam->count        = 0;
    for(u32 _i = 0; _i < _count && _exam->count < _max && _exam->count < FUDE_WORDEXAM_MAX; _i++) {
        if(fude_wordexam_item_set(_exam, &_exam->items[_exam->count], _ids[_i])) {
            _exam->count++;
        }
    }
    _exam->current      = 0;
    rde_arr_clear(&_exam->strokes);
    _exam->pen          = false;
    _exam->kept         = false;
    _exam->spoken       = false;
}

// Returns whether word _id can be asked: it is there, not too long, and has something to write. It splits the word
// into boxes as asking it does (fude_wordexam_boxes), so a word counted here is one that can be asked: a sign goes in
// the box before it, and two letters written as one (Arabic's lām-alif, ﻻ) take one box.
RDE_INTERNAL b8 fude_wordexam_can_ask(const fude_wordexam* _exam, u32 _id) {
    return fude_wordexam_boxes(_exam, _id, NULL) > 0u;
}

// Returns how many of the exam's words can be asked.
RDE_INTERNAL u32 fude_wordexam_askable(fude_wordexam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_exam->words); _i++) {
        _n += fude_wordexam_can_ask(_exam, ((const u32*)_exam->words.memory)[_i]) ? 1u : 0u;
    }
    return _n;
}

// Opens the setup over a copy of _ids, named _title. Without a voice, the words are asked by meaning. The shuffle is
// seeded from the clock.
void fude_wordexam_open(fude_wordexam* _exam, const u32* _ids, u32 _count, const c8* _title) {
    rde_arr_clear(&_exam->words);
    if(_count > 0) {
        rde_memcpy(rde_arr_add_n(&_exam->words, _count), _ids, sizeof(u32) * _count);
    }
    rde_snprintf(_exam->title, sizeof(_exam->title), "%s", _title != NULL ? _title : "");
    _exam->open   = _exam->db != NULL;
    _exam->stage  = FUDE_WORDEXAM_SETUP;
    _exam->review = false;
    _exam->rng    = rde_random_new_from_entropy();
    _exam->tapped = -1;
    _exam->count  = 0;
    if(!fude_speech_available()) {
        _exam->by = FUDE_WORDEXAM_BY_MEANING;
    }
    fude_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

// Opens on the reviews due, by meaning and reading, straight to writing them in their order (when any can be asked).
void fude_wordexam_open_review(fude_wordexam* _exam, const u32* _ids, u32 _count) {
    fude_wordexam_open(_exam, _ids, _count, fude_text(FUDE_TEXT_REVIEWS));
    _exam->review = true;
    _exam->by     = FUDE_WORDEXAM_BY_MEANING;
    fude_wordexam_set_items(_exam, _ids, _count, FUDE_WORDEXAM_MAX);   // What is due, in its order.
    if(_exam->count > 0) {
        _exam->stage = FUDE_WORDEXAM_WRITING;
    }
}

// See the header: the words that can be asked, cut to the length chosen (0 is all) and to FUDE_WORDEXAM_MAX.
u32 fude_wordexam_planned(const fude_wordexam* _exam) {
    const u32 _size = fude_wordexam_askable((fude_wordexam*)_exam);
    const u32 _want = FUDE_WORDEXAM_LENGTH_VALUES[_exam->length];
    const u32 _n    = _want == 0u || _want > _size ? _size : _want;
    return _n > FUDE_WORDEXAM_MAX ? FUDE_WORDEXAM_MAX : _n;
}

// Starts writing: a shuffled copy of the words, as many as planned that can be asked. With none, it stays on the
// setup.
void fude_wordexam_start(fude_wordexam* _exam) {
    const u32 _n   = (u32)rde_arr_length(&_exam->words);
    u32*      _ids = (u32*)rde_malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
    if(_n > 0) {
        rde_memcpy(_ids, _exam->words.memory, sizeof(u32) * _n);
    }
    rde_random_shuffle(&_exam->rng, _ids, _n, sizeof(u32));
    fude_wordexam_set_items(_exam, _ids, _n, fude_wordexam_planned(_exam));
    rde_free(_ids);
    if(_exam->count > 0) {
        _exam->stage = FUDE_WORDEXAM_WRITING;
    }
}

// --- Writing ---------------------------------------------------------------------------------------------------------

// Returns the word being written, or NULL when the exam is not at the writing stage.
RDE_INTERNAL fude_wordexam_item* fude_wordexam_current(fude_wordexam* _exam) {
    return _exam->stage == FUDE_WORDEXAM_WRITING && _exam->current < _exam->count ? &_exam->items[_exam->current] : NULL;
}

// Takes back the word's last stroke from the box it went in, unless the pen is down.
void fude_wordexam_undo(fude_wordexam* _exam) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    const u32           _n  = (u32)rde_arr_length(&_exam->strokes);
    if(_it != NULL && !_exam->pen && _n > 0) {
        fude_ink_undo(&_it->ink[((const u32*)_exam->strokes.memory)[_n - 1u]]);
        rde_arr_resize(&_exam->strokes, _n - 1u);
    }
}

// Empties every box of the word, unless the pen is down.
void fude_wordexam_clear(fude_wordexam* _exam) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        for(u32 _b = 0; _b < _it->count; _b++) {
            fude_ink_destroy(&_it->ink[_b]);
            fude_wordexam_fresh_ink(&_it->ink[_b]);
        }
        rde_arr_clear(&_exam->strokes);
    }
}

// Returns whether the word being written is the last one.
b8 fude_wordexam_at_last(const fude_wordexam* _exam) {
    return _exam->current + 1u >= _exam->count;
}

// Ends a stroke still being drawn, marks the word answered (to be read), and moves on: to the next word, or after the
// last to the results.
void fude_wordexam_next(fude_wordexam* _exam) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_it == NULL) {
        return;
    }
    if(_exam->pen) {
        fude_ink_end(&_it->ink[_exam->pen_box]);
        _exam->pen = false;
    }
    _it->answered       = true;
    rde_arr_clear(&_exam->strokes);
    _exam->spoken       = false;
    if(_exam->current + 1u < _exam->count) {
        _exam->current++;
    } else {
        _exam->stage = FUDE_WORDEXAM_RESULTS;
        fude_scroller_stop(&_exam->scroller);
        _exam->scroller.offset = 0.0f;
    }
}

// --- Marking ---------------------------------------------------------------------------------------------------------

// Marks box _b of _it. It is right when its character is among the first FUDE_EXAM_CANDIDATES candidates (_r is ML
// Kit's answer, or NULL for the matcher alone), and its points are how well it was written. An empty box is wrong.
RDE_INTERNAL void fude_wordexam_mark_box(fude_wordexam* _exam, fude_wordexam_item* _it, u32 _b, const fude_recognition* _r) {
    _it->box_graded[_b] = true;
    _it->box_right[_b]  = false;
    _it->box_points[_b] = 0.0f;
    if(fude_ink_alive_strokes(&_it->ink[_b]) == 0) {
        return;   // Nothing written is wrong.
    }
    // A syllable (a letter with signs on it) is right when ML Kit read it whole, and then it gets 100 points. Without
    // ML Kit, the matcher knows letters only, so only its letter is checked.
    c8 _letter[8];
    fude_utf8_put(fude_lang_letter(_it->chars[_b]), _letter);
    // Arabic's lām-alif ligatures (U+FEF5 to U+FEFC) take one letter's box but two letters' text; they are not
    // syllables.
    const b8 _ligature = _it->chars[_b] >= 0xFEF5u && _it->chars[_b] <= 0xFEFCu;   // Lām-alif.
    const b8 _syllable = rde_strcmp(_it->written[_b], _letter) != 0 && !_ligature;
    if(_syllable && _r != NULL) {
        for(u32 _l = 0; _l < _r->line_count && _l < FUDE_EXAM_CANDIDATES; _l++) {
            _it->box_right[_b] = _it->box_right[_b] || rde_strcmp(_r->lines[_l], _it->written[_b]) == 0;
        }
        _it->box_points[_b] = _it->box_right[_b] ? 100.0f : 0.0f;
        return;
    }
    fude_match_result _matched[16];
    fude_match_result _candidates[8];
    const u32 _m = fude_match_rank(_exam->db, _exam->catalog, FUDE_FILTER_ALL, &_it->ink[_b], _matched, 16u);
    const u32 _n = fude_recognize_candidates(_exam->db, _r, 0u, 1u, _exam->catalog, FUDE_FILTER_ALL, _matched, _m, _candidates, 8u);
    for(u32 _i = 0; _i < _n && _i < FUDE_EXAM_CANDIDATES; _i++) {
        _it->box_right[_b] = _it->box_right[_b] || fude_recognize_same(_exam->db, _candidates[_i].record, _it->records[_b]);   // Forms count as letters.
    }
    fude_kanji_info _info;
    if(_syllable) {
        _it->box_points[_b] = _it->box_right[_b] ? 70.0f : 0.0f;   // Only its letter was checked, not the signs.
    } else if(fude_kanji_at(_exam->db, _it->records[_b], &_info)) {
        const fude_score _s = fude_score_drawing(_exam->db, &_info, &_it->ink[_b]);
        _it->box_points[_b] = _s.empty ? 0.0f : _s.score;
    }
}

// Once every box is read, marks the word: right when every box is, and its points the mean of its written boxes'
// (given boxes do not count). Until then it does nothing.
RDE_INTERNAL void fude_wordexam_finish_item(fude_wordexam_item* _it) {
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

// Returns whether every word asked has been read.
b8 fude_wordexam_graded(const fude_wordexam* _exam) {
    for(u32 _i = 0; _i < _exam->count; _i++) {
        if(!_exam->items[_i].graded) {
            return false;
        }
    }
    return true;
}

// Returns how many words read are wrong.
u32 fude_wordexam_wrong(const fude_wordexam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _n += _exam->items[_i].graded && !_exam->items[_i].correct ? 1u : 0u;
    }
    return _n;
}

// Reads the answers given, one box at a time. ML Kit reads it while it is on and has its model: it answers a frame or
// more later and is given FUDE_WORDEXAM_WAIT seconds. Otherwise the matcher reads it alone, one box a frame. A given
// box is right without reading.
RDE_INTERNAL void fude_wordexam_grade(fude_wordexam* _exam) {
    const f64 _now = rde_engine_get_time_now();
    if(_exam->grading_item != UINT32_MAX) {
        fude_redraw_changed();   // (render on demand: grading goes on frame by frame, ML Kit polled, until all are marked)
        fude_wordexam_item* _it = &_exam->items[_exam->grading_item];
        if(fude_recognize_poll(&_exam->recognition)) {
            fude_wordexam_mark_box(_exam, _it, _exam->grading_box, &_exam->recognition);
            _exam->grading_item = UINT32_MAX;
        } else if(_now - _exam->grading_since > FUDE_WORDEXAM_WAIT) {
            fude_recognize_forget(&_exam->recognition);   // No answer in time: the matcher decides alone.
            fude_wordexam_mark_box(_exam, _it, _exam->grading_box, NULL);
            _exam->grading_item = UINT32_MAX;
        }
        fude_wordexam_finish_item(_it);
        return;
    }
    for(u32 _i = 0; _i < _exam->count; _i++) {
        fude_wordexam_item* _it = &_exam->items[_i];
        if(!_it->answered || _it->graded) {
            continue;
        }
        fude_redraw_changed();   // (render on demand: one to grade, this frame or the next)
        for(u32 _b = 0; _b < _it->count; _b++) {
            if(_it->box_graded[_b]) {
                continue;
            }
            if(_it->records[_b] == UINT32_MAX) {
                _it->box_graded[_b] = _it->box_right[_b] = true;   // Given.
                continue;
            }
            if(fude_ink_alive_strokes(&_it->ink[_b]) > 0 && fude_recognize_available()) {
                if(fude_recognize_start(&_exam->recognition, &_it->ink[_b])) {
                    _exam->grading_item  = _i;
                    _exam->grading_box   = _b;
                    _exam->grading_since = _now;
                }
                return;   // Asked, or ML Kit is busy elsewhere and it is tried on a later frame.
            }
            fude_wordexam_mark_box(_exam, _it, _b, NULL);
            fude_wordexam_finish_item(_it);
            return;   // One a frame, since the matcher takes a moment.
        }
        fude_wordexam_finish_item(_it);
    }
}

// Once every word is read: moves each word's review (review.h, under its FUDE_VOCAB_KEY), with its points as the
// quality, from 0 to 1.
RDE_INTERNAL void fude_wordexam_keep(fude_wordexam* _exam) {
    fude_review_answer _answers[FUDE_WORDEXAM_MAX];
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _answers[_i] = (fude_review_answer){ FUDE_VOCAB_KEY(_exam->items[_i].word), _exam->items[_i].correct, _exam->items[_i].points / 100.0f };
    }
    fude_reviews_answer(_answers, _exam->count);
    _exam->kept = true;
}

// An exam closed on its results before every box was read (Done, or another screen opened from the side panel) is
// still kept: the boxes left are read by the matcher alone, all at once. Reading goes on only while the screen is open
// (update), and ML Kit can take seconds a box, so without this the exam's reviews would be lost. Quit while writing is
// different: the learner gave the exam up, and nothing is kept.
RDE_INTERNAL void fude_wordexam_keep_unread(fude_wordexam* _exam) {
    if(_exam->stage != FUDE_WORDEXAM_RESULTS || _exam->kept || _exam->count == 0) {
        return;
    }
    fude_recognize_forget(&_exam->recognition);
    _exam->grading_item = UINT32_MAX;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        fude_wordexam_item* _it = &_exam->items[_i];
        for(u32 _b = 0; _b < _it->count && !_it->graded; _b++) {
            if(_it->box_graded[_b]) {
                continue;
            }
            if(_it->records[_b] == UINT32_MAX) {
                _it->box_graded[_b] = _it->box_right[_b] = true;   // Given.
            } else {
                fude_wordexam_mark_box(_exam, _it, _b, NULL);
            }
        }
        fude_wordexam_finish_item(_it);
    }
    fude_wordexam_keep(_exam);
}

// Closes the exam. One on its results is kept even when not every box has been read (fude_wordexam_keep_unread); one
// quit while writing is not. A stroke still being written is ended first, as Next ends one, so its ink is left whole.
void fude_wordexam_close(fude_wordexam* _exam) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_exam->pen && _it != NULL) {
        fude_ink_end(&_it->ink[_exam->pen_box]);
    }
    fude_wordexam_keep_unread(_exam);
    fude_recognize_forget(&_exam->recognition);
    _exam->grading_item = UINT32_MAX;
    _exam->open         = false;
    _exam->pen          = false;
}

// Makes the wrong words, shuffled, an exam of their own and starts writing it, once every answer has been read. The
// exam just taken was kept already.
void fude_wordexam_retry_wrong(fude_wordexam* _exam) {
    if(!fude_wordexam_graded(_exam)) {
        return;
    }
    u32 _ids[FUDE_WORDEXAM_MAX];
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        if(!_exam->items[_i].correct) {
            _ids[_n++] = _exam->items[_i].word;
        }
    }
    if(_n == 0) {
        return;
    }
    rde_random_shuffle(&_exam->rng, _ids, _n, sizeof(u32));
    fude_wordexam_set_items(_exam, _ids, _n, FUDE_WORDEXAM_MAX);
    _exam->stage = FUDE_WORDEXAM_WRITING;
}

// Hands out the result tapped once, as its word's id: tapped goes back to -1, which means none is waiting.
b8 fude_wordexam_take_tap(fude_wordexam* _exam, u32* _word) {
    if(_exam->tapped < 0) {
        return false;
    }
    *_word        = _exam->items[_exam->tapped].word;
    _exam->tapped = -1;
    return true;
}

// --- The pointer -----------------------------------------------------------------------------------------------------

// Returns whether point _p is inside the box from _min to _max; a box with no width (none) has nothing in it.
RDE_INTERNAL b8 fude_wordexam_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

// Returns box _b's place in the row, 0 being the leftmost as laid out. A right-to-left language puts its first letter
// at the right, so its boxes go from the right; but a right-to-left screen's mirror (draw.h) already does that, so on
// such a screen the boxes go in their own order.
RDE_INTERNAL f32 fude_wordexam_slot(const fude_wordexam_item* _it, u32 _b) {
    return (f32)(fude_lang_rtl() != fude_draw_is_rtl() ? _it->count - 1u - _b : _b);
}

// Returns the box of the word being written under _screen, or UINT32_MAX.
RDE_INTERNAL u32 fude_wordexam_box_at(const fude_wordexam* _exam, const fude_wordexam_item* _it, rde_vec_2F _screen) {
    for(u32 _b = 0; _b < _it->count && _exam->box > 0.0f; _b++) {
        const f32 _x = _exam->boxes_tl.x + fude_wordexam_slot(_it, _b) * (_exam->box + _exam->box_gap);
        if(_screen.x >= _x && _screen.x <= _x + _exam->box && _screen.y <= _exam->boxes_tl.y && _screen.y >= _exam->boxes_tl.y - _exam->box) {
            return _b;
        }
    }
    return UINT32_MAX;
}

// Turns a screen point into box _b's ink units: from its bottom-left corner, FUDE_WORDEXAM_UNITS across, Y up,
// clamped to the box.
RDE_INTERNAL rde_vec_2F fude_wordexam_local(const fude_wordexam* _exam, const fude_wordexam_item* _it, u32 _b, rde_vec_2F _screen) {
    const f32 _k = FUDE_WORDEXAM_UNITS / _exam->box;
    const f32 _x = _exam->boxes_tl.x + fude_wordexam_slot(_it, _b) * (_exam->box + _exam->box_gap);
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _x) * _k, 0.0f, FUDE_WORDEXAM_UNITS),
        rde_math_clamp_f32((_screen.y - (_exam->boxes_tl.y - _exam->box)) * _k, 0.0f, FUDE_WORDEXAM_UNITS)
    };
}

// While writing, a tap on the speaker says the word again, and a pen in a box that is not given starts a stroke. At
// the other stages any pointer starts a drag or a tap.
void fude_wordexam_pointer_down(fude_wordexam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_it != NULL) {
        if(fude_wordexam_inside(_screen, _exam->speaker_min, _exam->speaker_max)) {
            const fude_vocab_word* _w = fude_vocab_get(_it->word);
            if(_w != NULL) {
                fude_speak(_w->reading[0] != 0 ? _w->reading : _w->written);   // Said again.
            }
            return;
        }
        // While writing, only the pen writes, and only inside a box, so a resting hand does nothing.
        const u32 _b = fude_wordexam_box_at(_exam, _it, _screen);
        if(_pen && _b != UINT32_MAX && _it->records[_b] != UINT32_MAX) {
            _it->ink[_b].zoom = _exam->box / FUDE_WORDEXAM_UNITS;
            fude_ink_begin(&_it->ink[_b], fude_wordexam_local(_exam, _it, _b, _screen), true);
            _exam->pen     = true;
            _exam->pen_box = _b;
        }
        return;
    }
    fude_scroller_down(&_exam->scroller, _screen, _time);
}

// Extends the stroke, or drags.
void fude_wordexam_pointer_moved(fude_wordexam* _exam, rde_vec_2F _screen, f64 _time) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_exam->pen && _it != NULL) {
        fude_ink_extend(&_it->ink[_exam->pen_box], fude_wordexam_local(_exam, _it, _exam->pen_box, _screen));
        return;
    }
    fude_scroller_moved(&_exam->scroller, _screen, _time);
}

// Ends the stroke, noting its box for Undo (every stroke of the word, however many), or ends the drag.
void fude_wordexam_pointer_up(fude_wordexam* _exam, f64 _time) {
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_exam->pen) {
        if(_it != NULL) {
            fude_ink_end(&_it->ink[_exam->pen_box]);
            rde_arr_add(&_exam->strokes, (any)&_exam->pen_box);
        }
        _exam->pen = false;
        return;
    }
    fude_scroller_up(&_exam->scroller, _time);
}

// Acts on a tap: a setup chip chooses its length or how words are asked, and a results row is kept (as its item) for
// fude_wordexam_take_tap.
RDE_INTERNAL void fude_wordexam_tap(fude_wordexam* _exam, rde_vec_2F _at) {
    if(_exam->stage == FUDE_WORDEXAM_SETUP) {
        for(u32 _i = 0; _i < _exam->chip_count; _i++) {
            const fude_wordexam_chip* _c = &_exam->chips[_i];
            if(fude_wordexam_inside(_at, _c->min, _c->max)) {
                if(_c->kind == 0u) { _exam->length = _c->value; }
                else               { _exam->by = (FUDE_WORDEXAM_BY_)_c->value; }
            }
        }
    } else if(_exam->stage == FUDE_WORDEXAM_RESULTS && fude_wordexam_inside(_at, _exam->rows_min, _exam->rows_max)) {
        const i32 _i = (i32)rde_math_floor_f32((_exam->rows_max.y - _at.y + _exam->scroller.offset) / FUDE_WORDEXAM_ROW);
        if(_i >= 0 && _i < (i32)_exam->count) {
            _exam->tapped = _i;
        }
    }
}

// Scrolls and takes a tap, says a word asked by ear as it comes up, reads an answer, and moves the reviews once every
// answer is read.
void fude_wordexam_update(fude_wordexam* _exam, f32 _dt) {
    if(!_exam->open) {
        return;
    }
    fude_scroller_update(&_exam->scroller, _dt, _exam->content_h, _exam->rows_max.y - _exam->rows_min.y);
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_exam->scroller, &_at)) {
        fude_wordexam_tap(_exam, _at);
    }
    // By ear, each word is said once as it comes up.
    fude_wordexam_item* _it = fude_wordexam_current(_exam);
    if(_it != NULL && _exam->by == FUDE_WORDEXAM_BY_EAR && !_exam->spoken) {
        const fude_vocab_word* _w = fude_vocab_get(_it->word);
        if(_w != NULL) {
            fude_speak(_w->reading[0] != 0 ? _w->reading : _w->written);
        }
        _exam->spoken = true;
    }
    fude_wordexam_grade(_exam);
    if(_exam->stage == FUDE_WORDEXAM_RESULTS && !_exam->kept && _exam->count > 0 && fude_wordexam_graded(_exam)) {
        fude_wordexam_keep(_exam);
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Draws a setup chip from _min to _max, filled with the accent when chosen.
RDE_INTERNAL void fude_wordexam_chip_draw(rde_font* _font, f32 _font_px, rde_vec_2F _min, rde_vec_2F _max, const c8* _label, b8 _chosen) {
    const fude_theme* _theme = fude_theme_active();
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x((_min.x + _max.x) * 0.5f), (_min.y + _max.y) * 0.5f }, (rde_vec_2F){ _max.x - _min.x, _max.y - _min.y }, 1.0f, 10,
                                            _chosen ? _theme->accent : _theme->surface_2, NULL);
    const f32 _w = fude_draw_text_width(_font, _font_px, _label, FUDE_WORDEXAM_CHIP_PX);
    fude_draw_text(_font, _font_px, _label, (_min.x + _max.x - _w) * 0.5f, (_min.y + _max.y) * 0.5f - FUDE_WORDEXAM_CHIP_PX * 0.42f, FUDE_WORDEXAM_CHIP_PX,
                   _chosen ? _theme->on_accent : _theme->text);
}

// Draws the setup: the header, the chips of the lengths and (with a voice) of how words are asked, and a card saying
// what that choice makes. It records the chips for taps (at most 8).
RDE_INTERNAL void fude_wordexam_render_setup(fude_wordexam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top) {
    const fude_theme* _theme = fude_theme_active();
    c8 _title[512];
    FUDE_TEXTF(_title, FUDE_TEXT_WORDEXAM_TITLE_OF, FUDE_TS(_exam->title));
    fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_WORDEXAM_INTRO), NULL);
    f32 _y = _top - 56.0f;
    _exam->chip_count = 0;
    const u32 _size   = fude_wordexam_askable(_exam);
    c8        _label[512];
    for(u32 _group = 0; _group < 2u; _group++) {
        if(_group == 1u && !fude_speech_available()) {
            break;   // Without a voice, words are asked by meaning only.
        }
        _y -= 30.0f;
        fude_draw_text(_font, _font_px, fude_text(_group == 0u ? FUDE_TEXT_EXAM_HOW_MANY : FUDE_TEXT_WORDEXAM_BY), _left, _y, FUDE_HEADER_CAPTION_PX, _theme->text_soft);
        _y -= 12.0f;
        f32       _x = _left;
        const u32 _n = _group == 0u ? FUDE_WORDEXAM_LENGTHS : 2u;
        for(u32 _i = 0; _i < _n; _i++) {
            b8 _chosen;
            if(_group == 0u) {
                if(FUDE_WORDEXAM_LENGTH_VALUES[_i] == 0u) {
                    rde_snprintf(_label, sizeof(_label), "%s  %u", fude_text(FUDE_TEXT_ALL), _size);
                } else {
                    rde_snprintf(_label, sizeof(_label), "%u", FUDE_WORDEXAM_LENGTH_VALUES[_i]);
                }
                _chosen = _exam->length == _i;
            } else {
                rde_snprintf(_label, sizeof(_label), "%s %s", _i == 0u ? FUDE_ICON_TEXT_COPY : FUDE_ICON_LISTEN,
                         fude_text(_i == 0u ? FUDE_TEXT_WORDEXAM_BY_MEANING : FUDE_TEXT_WORDEXAM_BY_EAR));
                _chosen = (u32)_exam->by == _i;
            }
            const f32 _w = rde_math_max_f32(64.0f, fude_draw_text_width(_font, _font_px, _label, FUDE_WORDEXAM_CHIP_PX) + 36.0f);
            if(_x + _w > _right && _x > _left) {
                _x  = _left;
                _y -= FUDE_WORDEXAM_CHIP_H + FUDE_WORDEXAM_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - FUDE_WORDEXAM_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            fude_wordexam_chip_draw(_font, _font_px, _min, _max, _label, _chosen);
            if(_exam->chip_count < 8u) {
                _exam->chips[_exam->chip_count++] = (fude_wordexam_chip){ _min, _max, (u8)_group, (u8)_i };
            }
            _x += _w + FUDE_WORDEXAM_CHIP_GAP;
        }
        _y -= FUDE_WORDEXAM_CHIP_H;
    }
    // What that choice makes.
    _y -= 24.0f;
    fude_draw_card((rde_vec_2F){ _left, _y - 76.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
    c8 _line[512];
    if(_size == 0u) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_WORDEXAM_NONE));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_WORDEXAM_PLAN, FUDE_TN(fude_wordexam_planned(_exam)), FUDE_TN(_size));
    }
    fude_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 32.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 15.0f, _right - _left - 36.0f), _theme->text);
    rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_WORDEXAM_RULE));
    fude_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 56.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 12.0f, _right - _left - 36.0f), _theme->text_soft);
}

// Draws a word's boxes from _tl, each _box wide with _gap between them: the writing, or for a given box its character
// in grey, in _font (_font_px its size as loaded), and when _verdicts, each written box's verdict in its corner.
RDE_INTERNAL void fude_wordexam_draw_boxes(rde_window* _window, rde_font* _font, f32 _font_px, fude_wordexam_item* _it, rde_vec_2F _tl, f32 _box, f32 _gap, b8 _verdicts) {
    const fude_theme* _theme = fude_theme_active();
    const rde_vec_2I  _size  = rde_window_get_size(_window);
    for(u32 _b = 0; _b < _it->count; _b++) {
        const rde_vec_2F _btl = { _tl.x + fude_wordexam_slot(_it, _b) * (_box + _gap), _tl.y };
        // Handwriting is never mirrored, nor is a pen's place in it (draw.h).
        fude_draw_keep_begin((rde_vec_2F){ _btl.x, _btl.y - _box }, (rde_vec_2F){ _btl.x + _box, _btl.y }, true);
        fude_glyph_box(_btl, _box);
        if(_it->records[_b] == UINT32_MAX) {
            // A given box: the character as text, in grey.
            const c8* _ch = _it->written[_b];
            const f32 _px = _box * 0.5f;
            fude_draw_text(_font, _font_px, _ch, _btl.x + (_box - fude_draw_text_width(_font, _font_px, _ch, _px)) * 0.5f,
                           _btl.y - _box * 0.5f - _px * 0.38f, _px, _theme->text_soft);
        } else {
            fude_ink_render(&_it->ink[_b], (rde_vec_2F){ _btl.x, _btl.y - _box }, _box / FUDE_WORDEXAM_UNITS, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f },
                            rde_engine_get_time_now(), false);
            if(_verdicts && _it->box_graded[_b]) {
                const f32 _r = rde_math_max_f32(6.0f, _box * 0.1f);
                fude_draw_verdict((rde_vec_2F){ _btl.x + _box - _r - 3.0f, _btl.y - _r - 3.0f }, _r, _it->box_right[_b]);
            }
        }
        fude_draw_keep_end();
    }
}

// Draws the writing stage: the header and the progress, the prompt, and the word's boxes, as big as fit. It records
// the boxes and the speaker for the pointer.
RDE_INTERNAL void fude_wordexam_render_writing(fude_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*   _theme = fude_theme_active();
    fude_wordexam_item* _it    = fude_wordexam_current(_exam);
    _exam->speaker_min = _exam->speaker_max = (rde_vec_2F){ 0.0f, 0.0f };
    if(_it == NULL) {
        return;
    }
    const fude_vocab_word* _w = fude_vocab_get(_it->word);
    c8 _title[512], _line[64];
    FUDE_TEXTF(_title, FUDE_TEXT_WORDEXAM_TITLE_OF, FUDE_TS(_exam->title));
    FUDE_TEXTF(_line, FUDE_TEXT_OF_N, FUDE_TN(_exam->current + 1u), FUDE_TN(_exam->count));
    fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_WORDEXAM_INTRO), _line);
    // The progress: a segment per word.
    const f32 _py  = _top - 54.0f;
    const f32 _gap = 3.0f;
    const f32 _sw  = (_right - _left - _gap * (f32)(_exam->count - 1u)) / (f32)_exam->count;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        const rde_color _c = _i < _exam->current ? _theme->accent : _i == _exam->current ? _theme->tint : _theme->surface_2;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_left + (f32)_i * (_sw + _gap) + _sw * 0.5f), _py }, (rde_vec_2F){ _sw, 6.0f }, 1.0f, 4, _c, NULL);
    }

    // What to write: its meaning and reading, or by ear a speaker and its meaning.
    f32 _y = _py - 24.0f;
    if(_w != NULL) {
        if(_exam->by == FUDE_WORDEXAM_BY_EAR) {
            const f32 _r = 34.0f;
            const rde_vec_2F _c = { (_left + _right) * 0.5f, _y - _r };
            rde_rendering_2d_draw_circle(fude_draw_at(_c), _r, 40, _theme->accent, NULL);
            fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, _c, 30.0f, _theme->on_accent);
            _exam->speaker_min = (rde_vec_2F){ _c.x - _r, _c.y - _r };
            _exam->speaker_max = (rde_vec_2F){ _c.x + _r, _c.y + _r };
            _y -= 2.0f * _r + 12.0f;
            const c8* _again = fude_text(FUDE_TEXT_WORDEXAM_HEAR_AGAIN);
            fude_draw_text(_font, _font_px, _again, (_left + _right - fude_draw_text_width(_font, _font_px, _again, 11.0f)) * 0.5f, _y - 8.0f, 11.0f, _theme->text_soft);
            _y -= 24.0f;
        }
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _w->meaning, 22.0f, _right - _left);
        fude_draw_text_wrap(_font, _font_px, _w->meaning[0] != 0 ? _w->meaning : "", _left, _y - 22.0f, 22.0f, _right - _left, 30.0f, _theme->text);
        _y -= (f32)(_lines > 0 ? _lines : 1u) * 30.0f + 4.0f;
        if(_exam->by == FUDE_WORDEXAM_BY_MEANING) {
            fude_draw_text(_font, _font_px, _w->reading, _left, _y - 20.0f, 20.0f, _theme->accent);
            _y -= 34.0f;
        }
    }

    // The boxes, a character each, centred, as big as fit (between 48 and FUDE_WORDEXAM_BOX_MAX).
    const f32 _room = _y - 16.0f - (_bottom + 30.0f);
    const f32 _gap2 = 10.0f;
    const f32 _box  = rde_math_max_f32(48.0f, rde_math_min_f32(rde_math_min_f32(FUDE_WORDEXAM_BOX_MAX, _room), (_right - _left - _gap2 * (f32)(_it->count - 1u)) / (f32)_it->count));
    const f32 _all  = _box * (f32)_it->count + _gap2 * (f32)(_it->count - 1u);
    _exam->boxes_tl = (rde_vec_2F){ (_left + _right - _all) * 0.5f, _y - 16.0f };
    _exam->box      = _box;
    _exam->box_gap  = _gap2;
    fude_wordexam_draw_boxes(_window, _font, _font_px, _it, _exam->boxes_tl, _box, _gap2, false);
    fude_draw_text(_font, _font_px, fude_text(fude_wordexam_at_last(_exam) ? FUDE_TEXT_WORDEXAM_THEN_FINISH : FUDE_TEXT_WORDEXAM_THEN_NEXT),
                   _exam->boxes_tl.x, _exam->boxes_tl.y - _box - 20.0f, 12.0f, _theme->text_soft);
}

// Draws the results: how many are being read, or once all are, how many were right, the mean points and whether it
// passed; then a row per word, clipped and scrolled. It records the rows' area for taps.
RDE_INTERNAL void fude_wordexam_render_results(fude_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    c8 _line[512], _title[512];
    FUDE_TEXTF(_title, FUDE_TEXT_WORDEXAM_TITLE_OF, FUDE_TS(_exam->title));
    if(!fude_wordexam_graded(_exam)) {
        u32 _graded = 0;
        for(u32 _i = 0; _i < _exam->count; _i++) {
            _graded += _exam->items[_i].graded ? 1u : 0u;
        }
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_READING_ANSWERS, FUDE_TN(_graded), FUDE_TN(_exam->count));
        fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, _title, _line, NULL);
    } else {
        u32 _right_n = 0;
        f32 _points  = 0.0f;
        for(u32 _i = 0; _i < _exam->count; _i++) {
            _right_n += _exam->items[_i].correct ? 1u : 0u;
            _points  += _exam->items[_i].correct ? _exam->items[_i].points : 0.0f;
        }
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_RESULT, FUDE_TN(_right_n), FUDE_TN(_exam->count), FUDE_TN(rde_math_round_to_i64_f32(_points / (f32)_exam->count)));
        fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, _title, _line, NULL);
        const b8 _passed = (f32)_right_n >= FUDE_EXAM_PASS * (f32)_exam->count;   // the exams' pass mark (exam.h)
        const c8* _verdict = fude_text(_passed ? FUDE_TEXT_EXAM_PASSED : FUDE_TEXT_EXAM_NOT_PASSED);
        const f32 _vw = fude_draw_text_width(_font, _font_px, _verdict, 12.0f) + 22.0f;
        fude_draw_chip(_font, _font_px, _verdict, _right - _vw, _top - 20.0f, 12.0f, _passed ? _theme->score_good : _theme->score_poor, _theme->on_accent);
    }

    // A row for each word: what was written, then the word, and how it went.
    const f32 _rows_top = _top - 60.0f;
    _exam->rows_min  = (rde_vec_2F){ _left, _bottom };
    _exam->rows_max  = (rde_vec_2F){ _right, _rows_top };
    _exam->content_h = (f32)_exam->count * FUDE_WORDEXAM_ROW;
    if(_rows_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_rows_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_rows_top - _bottom) });
    for(u32 _i = 0; _i < _exam->count; _i++) {
        const f32 _row_top = _rows_top - (f32)_i * FUDE_WORDEXAM_ROW + _exam->scroller.offset;
        if(_row_top - FUDE_WORDEXAM_ROW > _rows_top || _row_top < _bottom) {
            continue;
        }
        fude_wordexam_item*    _it  = &_exam->items[_i];
        const fude_vocab_word* _w   = fude_vocab_get(_it->word);
        const f32              _box = 64.0f;
        const f32              _mid = _row_top - FUDE_WORDEXAM_ROW * 0.5f;
        fude_wordexam_draw_boxes(_window, _font, _font_px, _it, (rde_vec_2F){ _left, _mid + _box * 0.5f }, _box, 4.0f, true);
        const f32 _tx = _left + (f32)_it->count * (_box + 4.0f) + 16.0f;
        if(_w != NULL) {
            const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _w->written, 20.0f, _right - 60.0f - _tx);
            fude_draw_text(_font, _font_px, _w->written, _tx, _mid + 4.0f, _px, _theme->ink);
            rde_snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _w->reading, _w->meaning);   // A middle dot, ·.
            fude_draw_text(_font, _font_px, _line, _tx, _mid - 18.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 12.0f, _right - 60.0f - _tx), _theme->text_soft);
        }
        if(_it->graded) {
            fude_draw_verdict((rde_vec_2F){ _right - 22.0f, _mid }, 14.0f, _it->correct);
        } else {
            rde_rendering_2d_draw_circle_border((rde_vec_2F){ fude_draw_x(_right - 22.0f), _mid }, 14.0f, 1.5f, 24, _theme->text_soft, NULL);
        }
        if(_i + 1u < _exam->count) {
            fude_draw_line((rde_vec_2F){ _left, _row_top - FUDE_WORDEXAM_ROW }, (rde_vec_2F){ _right, _row_top - FUDE_WORDEXAM_ROW }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}

// Draws the stage. The setup has no rows, so their area and height are zeroed.
void fude_wordexam_render(fude_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_exam->open) {
        return;
    }
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_WORDEXAM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_WORDEXAM_MARGIN;
    switch(_exam->stage) {
        case FUDE_WORDEXAM_SETUP:
            fude_wordexam_render_setup(_exam, _font, _font_px, _left, _right, _top);
            _exam->rows_min = _exam->rows_max = (rde_vec_2F){ 0.0f, 0.0f };
            _exam->content_h = 0.0f;
            break;
        case FUDE_WORDEXAM_WRITING: fude_wordexam_render_writing(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case FUDE_WORDEXAM_RESULTS: fude_wordexam_render_results(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
    }
}

// --- The screen (screen.h): its rows of buttons, and what they do ----------------------------------------------------

#include "study/app/study.h"
#include "study/widgets/wordcard.h"

// The screen's is_open, close, pointer and render functions, which call the word exam's own (screen.h); the pen writes.
FUDE_SCREEN_ADAPTERS_PEN(fude_wordexam, fude_wordexam)
FUDE_SCREEN_RENDER(fude_wordexam, fude_wordexam)

// Updates the exam, which reads its answers as it goes, then hands on a word tapped in the results: the word card
// opens on it.
RDE_INTERNAL void fude_wordexam_screen_update(fude_app* _app, any _self, f32 _dt) {
    fude_wordexam* _exam = (fude_wordexam*)_self;
    RDE_UNUSED(_app);
    fude_wordexam_update(_exam, _dt);
    u32 _word;
    if(fude_wordexam_take_tap(_exam, &_word)) {
        fude_wordcard_ask_saved(_word);
    }
}

// The buttons that call one of the exam's functions: Close, Quit and Done close it; Start n starts; Undo, Clear and
// Next act on the word; Retry wrong retries.
FUDE_ROW_CALL(fude_wordexam_row_close, fude_wordexam, fude_wordexam_close)
FUDE_ROW_CALL(fude_wordexam_row_start, fude_wordexam, fude_wordexam_start)
FUDE_ROW_CALL(fude_wordexam_row_undo,  fude_wordexam, fude_wordexam_undo)
FUDE_ROW_CALL(fude_wordexam_row_clear, fude_wordexam, fude_wordexam_clear)
FUDE_ROW_CALL(fude_wordexam_row_next,  fude_wordexam, fude_wordexam_next)
FUDE_ROW_CALL(fude_wordexam_row_retry, fude_wordexam, fude_wordexam_retry_wrong)

// One row per stage, in FUDE_WORDEXAM_STAGE_ order: the setup's, the writing's and the results'. The enums are the
// indices of the buttons whose faces change.
enum { FUDE_WORDEXAM_SETUP_START = 1 };
enum { FUDE_WORDEXAM_WRITE_NEXT = 3 };
enum { FUDE_WORDEXAM_RESULTS_RETRY = 1 };
static const fude_row_button FUDE_WORDEXAM_SETUP_ROW[] = {
    { FUDE_TEXT_CLOSE,   FUDE_ICON_CLOSE,       fude_wordexam_row_close, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_START_N, FUDE_ICON_ARROW_RIGHT, fude_wordexam_row_start, 0, FUDE_ROW_PRIMARY, true,  NULL },
};
static const fude_row_button FUDE_WORDEXAM_WRITE_ROW[] = {
    { FUDE_TEXT_QUIT,  FUDE_ICON_CLOSE,       fude_wordexam_row_close, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_UNDO,  FUDE_ICON_UNDO,        fude_wordexam_row_undo,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_CLEAR, FUDE_ICON_TRASH,       fude_wordexam_row_clear, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NEXT,  FUDE_ICON_ARROW_RIGHT, fude_wordexam_row_next,  0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_button FUDE_WORDEXAM_RESULTS_ROW[] = {
    { FUDE_TEXT_DONE,        FUDE_ICON_CHECK, fude_wordexam_row_close, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_RETRY_WRONG, FUDE_ICON_RETRY, fude_wordexam_row_retry, 0, FUDE_ROW_QUIET, false, NULL },
};
static const fude_row_def FUDE_WORDEXAM_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_WORDEXAM_SETUP_ROW), FUDE_ROW_DEF(FUDE_WORDEXAM_WRITE_ROW), FUDE_ROW_DEF(FUDE_WORDEXAM_RESULTS_ROW) };

// Which row is up: the stage's.
RDE_INTERNAL u32 fude_wordexam_screen_row(const any _self) {
    return (u32)((const fude_wordexam*)_self)->stage;
}

// How the buttons show: Start n says how many words are planned (greyed out at 0); Next is Finish on the last word;
// Retry wrong is greyed out until every answer is read and while none is wrong.
RDE_INTERNAL void fude_wordexam_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    const fude_wordexam* _exam = (const fude_wordexam*)_self;
    if(_row == FUDE_WORDEXAM_SETUP) {
        const u32 _planned = fude_wordexam_planned(_exam);
        fude_row_face_count(&_faces[FUDE_WORDEXAM_SETUP_START], FUDE_TEXT_START_N, _planned);
        _faces[FUDE_WORDEXAM_SETUP_START].disabled = _planned == 0u;
    } else if(_row == FUDE_WORDEXAM_WRITING) {
        fude_row_face_next(&_faces[FUDE_WORDEXAM_WRITE_NEXT], fude_wordexam_at_last(_exam));
    } else {
        _faces[FUDE_WORDEXAM_RESULTS_RETRY].disabled = !fude_wordexam_graded(_exam) || fude_wordexam_wrong(_exam) == 0u;
    }
}

const fude_screen FUDE_WORDEXAM_SCREEN = {
    .name = "wordexam", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_wordexam_screen_is_open, .close = fude_wordexam_screen_close,
    .update = fude_wordexam_screen_update, .render = fude_wordexam_screen_render,
    .pointer_down = fude_wordexam_screen_down, .pointer_moved = fude_wordexam_screen_moved, .pointer_up = fude_wordexam_screen_up,
    .rows = FUDE_WORDEXAM_BUTTON_ROWS, .row_count = 3u, .row = fude_wordexam_screen_row, .faces = fude_wordexam_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
