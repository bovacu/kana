// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/exam.h"
#include "study/widgets/header.h"
#include "study/models/review.h"
#include "lang/lang.h"
#include "drawing/base/draw.h"
#include "study/models/examlog.h"
#include "study/models/marks.h"
#include "study/handwriting/match.h"
#include "study/handwriting/score.h"
#include "study/models/select.h"
#include "drawing/base/theme.h"
#include "study/models/vocab.h"
#include "drawing/base/text.h"


// ===========================================================================
// See exam.h.
//
// The items are a fixed array of FUDE_EXAM_MAX, each with its own ink, made once in fude_exam_init and emptied again
// for each exam rather than freed. order lists the included items in the order asked, and the writing, the marking
// and the results all walk it. Marking runs in fude_exam_grade, from fude_exam_update each frame: one answer at a
// time, through ML Kit when it is available, else through the matcher alone. Once every answer is marked,
// fude_exam_keep adds the exam to the log and moves the marks and the reviews.
// ===========================================================================

// The layout's sizes, in screen points (MARGIN from the screen's sides, a chip's height and the gap between chips, and
// the answer square at its largest), then the text sizes, in points.
#define FUDE_EXAM_MARGIN      24.0f
#define FUDE_EXAM_CHIP_H      40.0f
#define FUDE_EXAM_CHIP_GAP    8.0f
#define FUDE_EXAM_CELL_MIN    88.0f     // The preview's smallest cell, as in Browse's grid.
#define FUDE_EXAM_RESULT_MIN  132.0f    // The results' smallest cell, with room to see the writing.
#define FUDE_EXAM_SQUARE_MAX  520.0f
#define FUDE_EXAM_TITLE_PX    17.0f     // A screen's title.
#define FUDE_EXAM_CAPTION_PX  10.0f     // A caption.
#define FUDE_EXAM_CHIP_PX     13.0f     // A chip's label.
#define FUDE_EXAM_MEANING_PX  22.0f     // The prompt's meaning.
#define FUDE_EXAM_WORD_PX     19.0f     // The prompt's word.
#define FUDE_EXAM_GRADE_WAIT  4.0       // The seconds ML Kit may take on one answer before the matcher decides alone.
// The pen's radius, in the answer ink's units: the model's stroke width scaled from KanjiVG's box to the answer square.
#define FUDE_EXAM_PEN_RADIUS  (FUDE_EXAM_UNITS * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f)

// The lengths to choose from and their chips' labels. A length of 0 means all of them, and its label, NULL, shows the
// translated "All" (text.h).
static const u32       FUDE_EXAM_LENGTH_VALUES[FUDE_EXAM_LENGTHS] = { 10u, 20u, 50u, 0u };   // 0 is all.
static const c8* const FUDE_EXAM_LENGTH_NAMES[FUDE_EXAM_LENGTHS]  = { "10", "20", "50", NULL };   // NULL is All.

// --- The sources, numbered from the language's levels and sets (exam.h) ----------------------------------------------

// The source numbers, in exam.h's order: 0 Studying and 1 Known, then the levels, the sets, a selection given and the
// reviews due. How many there are depends on the language.
FUDE_EXAM_SOURCE_ fude_exam_source_level(u32 _level) { return 2u + _level; }
FUDE_EXAM_SOURCE_ fude_exam_source_set(u32 _set)     { return 2u + fude_lang_level_count() + _set; }
FUDE_EXAM_SOURCE_ fude_exam_source_selection(void)  { return 2u + fude_lang_level_count() + fude_catalog_set_count(); }
FUDE_EXAM_SOURCE_ fude_exam_source_review(void)     { return fude_exam_source_selection() + 1u; }
u32               fude_exam_source_count(void)      { return fude_exam_source_review() + 1u; }

// Returns a source's name: Studying, Known, a level's short name (the same in every language of the meanings, such as
// "N5": catalog.h), a set's group name, Selection or Reviews.
const c8* fude_exam_source_name(FUDE_EXAM_SOURCE_ _source) {
    const u32 _level_count = fude_lang_level_count();
    if(_source >= fude_exam_source_count()) {
        return "";
    }
    if(_source == FUDE_EXAM_SOURCE_STUDYING) { return fude_text(FUDE_TEXT_STUDYING); }
    if(_source == FUDE_EXAM_SOURCE_KNOWN)    { return fude_text(FUDE_TEXT_KNOWN); }
    if(_source < 2u + _level_count) {
        return fude_catalog_level_label(_source - 2u);
    }
    if(_source < fude_exam_source_selection()) {
        return fude_text((FUDE_TEXT_)fude_lang_group_name(fude_catalog_set_group(_source - 2u - _level_count)));
    }
    return fude_text(_source == fude_exam_source_selection() ? FUDE_TEXT_EXAM_SELECTION : FUDE_TEXT_REVIEWS);
}

// Makes an empty answer ink, drawn at the exam pen's fixed width.
RDE_INTERNAL void fude_exam_fresh_ink(fude_ink* _ink) {
    fude_ink_init(_ink);
    _ink->constant_radius = FUDE_EXAM_PEN_RADIUS;
}

// Sets the exam up closed, 20 characters long, with an empty ink for every item.
void fude_exam_init(fude_exam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    rde_memset(_exam, 0, sizeof(*_exam));
    _exam->db        = _db;
    _exam->catalog   = _catalog;
    _exam->length    = 1u;   // The second length: 20.
    _exam->grading   = UINT32_MAX;
    _exam->tapped    = -1;
    _exam->selection = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_glyph_init(&_exam->glyph, _db);
    for(u32 _i = 0; _i < FUDE_EXAM_MAX; _i++) {
        fude_exam_fresh_ink(&_exam->items[_i].ink);
    }
}

// Frees every item's ink, the selection and the glyph cache, and zeroes the exam.
void fude_exam_destroy(fude_exam* _exam) {
    for(u32 _i = 0; _i < FUDE_EXAM_MAX; _i++) {
        fude_ink_destroy(&_exam->items[_i].ink);
    }
    if(rde_arr_is_inited(&_exam->selection)) {
        rde_arr_free(&_exam->selection);
    }
    fude_glyph_destroy(&_exam->glyph);
    rde_memset(_exam, 0, sizeof(*_exam));
}

// --- What an exam can be of ------------------------------------------------------------------------------------------

// Returns whether record _record belongs to source _source (not a selection given, nor the reviews: those are lists).
// Only what the catalog lists counts.
RDE_INTERNAL b8 fude_exam_in_source(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source, u32 _record) {
    if(_exam->catalog == NULL || !fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_ALL)) {
        return false;   // Only what the catalog lists (in Kana, the kana and the kanji).
    }
    fude_kanji_info _info;
    const u32       _levels = fude_lang_level_count();
    if(_source == FUDE_EXAM_SOURCE_STUDYING || _source == FUDE_EXAM_SOURCE_KNOWN) {
        return fude_kanji_at(_exam->db, _record, &_info) &&
               fude_marks_get(_info.codepoint) == (_source == FUDE_EXAM_SOURCE_STUDYING ? FUDE_MARK_STUDYING : FUDE_MARK_KNOWN);
    }
    if(_source < 2u + _levels) {
        return fude_catalog_passes(_exam->catalog, _record, fude_filter_level(_source - 2u));
    }
    if(_source < fude_exam_source_selection()) {   // A set: only its core characters.
        return fude_catalog_passes(_exam->catalog, _record, fude_filter_group(fude_catalog_set_group(_source - 2u - _levels))) &&
               fude_kanji_at(_exam->db, _record, &_info) && fude_lang_core(_info.codepoint);
    }
    return false;
}

// Counts source _source's characters by going through every record; a selection or the reviews is the list given.
u32 fude_exam_source_size(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source) {
    if(_source == fude_exam_source_selection() || _source == fude_exam_source_review()) {
        return (u32)rde_arr_length(&_exam->selection);
    }
    u32 _n = 0;
    for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count; _r++) {
        _n += fude_exam_in_source(_exam, _source, _r) ? 1u : 0u;
    }
    return _n;
}

// See the header: the source's size, cut to the length chosen (0 is all) and to FUDE_EXAM_MAX.
u32 fude_exam_planned(const fude_exam* _exam) {
    const u32 _size = fude_exam_source_size(_exam, _exam->source);
    const u32 _want = FUDE_EXAM_LENGTH_VALUES[_exam->length];
    const u32 _n    = _want == 0u || _want > _size ? _size : _want;
    return _n > FUDE_EXAM_MAX ? FUDE_EXAM_MAX : _n;
}

// Makes the items _records (at most FUDE_EXAM_MAX of them), fresh: nothing answered, every one ticked, every ink
// empty. It also forgets any answer being read, and starts the grid from its top.
RDE_INTERNAL void fude_exam_set_items(fude_exam* _exam, const u32* _records, u32 _count) {
    _count = _count > FUDE_EXAM_MAX ? FUDE_EXAM_MAX : _count;
    for(u32 _i = 0; _i < FUDE_EXAM_MAX; _i++) {
        fude_exam_item* _it = &_exam->items[_i];
        fude_ink_destroy(&_it->ink);
        fude_exam_fresh_ink(&_it->ink);
        _it->record   = _i < _count ? _records[_i] : 0u;
        _it->included = _i < _count;
        _it->answered = _it->graded = _it->correct = false;
        _it->score    = _it->quality = 0.0f;
        _it->read_as  = UINT32_MAX;
    }
    _exam->count   = _count;
    _exam->asked   = 0;
    _exam->current = 0;
    _exam->saved   = false;
    _exam->kept    = UINT32_MAX;
    _exam->grading = UINT32_MAX;
    _exam->tapped  = -1;
    _exam->pen     = false;
    fude_recognize_forget(&_exam->recognition);
    fude_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

// Opens on the setup, keeping the source and length chosen last time. The shuffle is seeded from the clock.
void fude_exam_open(fude_exam* _exam) {
    rde_arr_clear(&_exam->selection);
    if(_exam->source == fude_exam_source_selection() || _exam->source == fude_exam_source_review()) {
        _exam->source = FUDE_EXAM_SOURCE_STUDYING;   // Those were lists given, which are gone now.
    }
    _exam->rng   = rde_random_new_from_entropy();
    _exam->stage = FUDE_EXAM_SETUP;
    _exam->open  = _exam->db != NULL;
    fude_exam_set_items(_exam, NULL, 0u);
}

// Opens on a copy of _records, as a selection, all of them, straight to the preview.
void fude_exam_open_with(fude_exam* _exam, const u32* _records, u32 _count) {
    fude_exam_open(_exam);
    if(_count > 0) {
        rde_memcpy(rde_arr_add_n(&_exam->selection, _count), (any)_records, sizeof(u32) * _count);
    }
    _exam->source = fude_exam_source_selection();
    _exam->length = FUDE_EXAM_LENGTHS - 1u;   // All of them.
    fude_exam_preview(_exam);
}

// Opens on a copy of the reviews due, as the source Reviews, and starts writing at once.
void fude_exam_open_review(fude_exam* _exam, const u32* _records, u32 _count) {
    fude_exam_open_with(_exam, _records, _count);
    _exam->source = fude_exam_source_review();
    fude_exam_preview(_exam);
    fude_exam_start(_exam);   // No preview: everything due is asked.
}

// Opens kept exam _index on its results, as they were: its items marked as the log says, and their writing read back
// from the log. Characters the data no longer has are left out.
b8 fude_exam_open_kept(fude_exam* _exam, u32 _index) {
    if(_exam->db == NULL || _index >= fude_examlog_count()) {
        return false;
    }
    const fude_examlog_exam* _kept  = &fude_examlog_exams()[_index];
    const fude_examlog_item* _items = &fude_examlog_items()[_kept->first_item];

    // The kept exam's characters that the data still has, in the order they were asked.
    u32 _records[FUDE_EXAM_MAX];
    u32 _from[FUDE_EXAM_MAX];   // Each one's item in the log.
    u32 _n = 0;
    for(u32 _i = 0; _i < _kept->item_count && _n < FUDE_EXAM_MAX; _i++) {
        if(fude_kanji_find_index(_exam->db, _items[_i].codepoint, &_records[_n])) {
            _from[_n++] = _i;
        }
    }
    rde_arr_clear(&_exam->selection);
    fude_exam_set_items(_exam, _records, _n);
    for(u32 _k = 0; _k < _n; _k++) {
        fude_exam_item*          _it  = &_exam->items[_k];
        const fude_examlog_item* _log = &_items[_from[_k]];
        u32                      _read;
        _it->answered = _it->graded = true;
        _it->correct  = _log->correct;
        _it->score    = _log->score;
        _it->quality  = _log->quality;
        _it->read_as  = _log->read_as != 0 && fude_kanji_find_index(_exam->db, _log->read_as, &_read) ? _read : UINT32_MAX;
        _exam->order[_k] = _k;
    }

    // The writing is read back from the log into each answer's ink: a point's x and y go from a fraction of the square
    // times 65535 to the ink's FUDE_EXAM_UNITS.
    rde_memory_allocator* _heap     = rde_memory_allocator_get_default_std();
    rde_arr               _writings = rde_arr_new(sizeof(fude_examlog_writing), _heap);
    rde_arr               _strokes  = rde_arr_new(sizeof(fude_history_stroke), _heap);
    rde_arr               _points   = rde_arr_new(sizeof(fude_history_point), _heap);
    rde_arr               _ink      = rde_arr_new(sizeof(fude_ink_point), _heap);
    fude_examlog_read_writing(_index, 0u, &_writings, &_strokes, &_points);
    const fude_examlog_writing* _w = (const fude_examlog_writing*)_writings.memory;
    const fude_history_stroke*  _s = (const fude_history_stroke*)_strokes.memory;
    const fude_history_point*   _p = (const fude_history_point*)_points.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_writings); _i++) {
        for(u32 _k = 0; _k < _n; _k++) {
            if(_from[_k] != _w[_i].item) {
                continue;
            }
            for(u32 _t = 0; _t < _w[_i].stroke_count; _t++) {
                const fude_history_stroke* _stroke = &_s[_w[_i].first_stroke + _t];
                rde_arr_clear(&_ink);
                fude_ink_point* _q = (fude_ink_point*)rde_arr_add_n(&_ink, _stroke->point_count);
                for(u32 _j = 0; _j < _stroke->point_count; _j++) {
                    const fude_history_point* _hp = &_p[_stroke->first_point + _j];
                    _q[_j] = (fude_ink_point){
                        .position = { (f32)_hp->x / 65535.0f * FUDE_EXAM_UNITS, (f32)_hp->y / 65535.0f * FUDE_EXAM_UNITS },
                        .pressure = 1.0f, .radius = FUDE_EXAM_PEN_RADIUS, .time = _hp->time
                    };
                }
                fude_ink_add_loaded_stroke(&_exam->items[_k].ink, _q, _stroke->point_count, FUDE_THEME_INK, true);
            }
            break;
        }
    }
    rde_arr_free(&_writings);
    rde_arr_free(&_strokes);
    rde_arr_free(&_points);
    rde_arr_free(&_ink);

    _exam->source = _kept->source < fude_exam_source_count() ? (FUDE_EXAM_SOURCE_)_kept->source : FUDE_EXAM_SOURCE_STUDYING;
    _exam->asked  = _n;
    _exam->saved  = true;   // It is already in the log.
    _exam->kept   = _index;
    _exam->stage  = FUDE_EXAM_RESULTS;
    _exam->open   = true;
    return true;
}

// Goes on to the preview: the source's characters, shuffled, cut to the length planned. They are gathered whole, in an
// array as long as the source is (on the std allocator, freed before it returns), so any of them may be drawn.
void fude_exam_preview(fude_exam* _exam) {
    rde_arr TYPE(u32) _records = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    if(_exam->source == fude_exam_source_selection() || _exam->source == fude_exam_source_review()) {
        rde_arr_add_arr(&_records, &_exam->selection);
    } else {
        for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count; _r++) {
            if(fude_exam_in_source(_exam, _exam->source, _r)) {
                rde_arr_add(&_records, (any)&_r);
            }
        }
    }
    const u32 _n = (u32)rde_arr_length(&_records);
    rde_random_shuffle(&_exam->rng, _records.memory, _n, sizeof(u32));
    fude_exam_set_items(_exam, (const u32*)_records.memory, fude_exam_planned(_exam) < _n ? fude_exam_planned(_exam) : _n);
    rde_arr_free(&_records);
    _exam->stage = FUDE_EXAM_PREVIEW;
}

// Goes back to the setup, dropping the items.
void fude_exam_back(fude_exam* _exam) {
    _exam->stage = FUDE_EXAM_SETUP;
    fude_exam_set_items(_exam, NULL, 0u);
}

// Ticks every item (_on) or unticks them all.
void fude_exam_tick_all(fude_exam* _exam, b8 _on) {
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _exam->items[_i].included = _on;
    }
}

// Returns how many items are ticked.
u32 fude_exam_included(const fude_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _n += _exam->items[_i].included ? 1u : 0u;
    }
    return _n;
}

// Starts writing the ticked items, in their order; with none ticked it stays where it is.
void fude_exam_start(fude_exam* _exam) {
    _exam->asked = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        if(_exam->items[_i].included) {
            _exam->order[_exam->asked++] = _i;
        }
    }
    if(_exam->asked == 0) {
        return;
    }
    _exam->current = 0;
    _exam->saved   = false;
    _exam->stage   = FUDE_EXAM_WRITING;
    fude_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

// --- Writing ---------------------------------------------------------------------------------------------------------

// Returns the item being written, or NULL when the exam is not at the writing stage.
RDE_INTERNAL fude_exam_item* fude_exam_current(fude_exam* _exam) {
    return _exam->stage == FUDE_EXAM_WRITING && _exam->current < _exam->asked ? &_exam->items[_exam->order[_exam->current]] : NULL;
}

// Takes back the answer's last stroke, unless the pen is down.
void fude_exam_undo(fude_exam* _exam) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        fude_ink_undo(&_it->ink);
    }
}

// Empties the answer, unless the pen is down.
void fude_exam_clear(fude_exam* _exam) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        fude_ink_destroy(&_it->ink);
        fude_exam_fresh_ink(&_it->ink);
    }
}

// Returns whether the character being written is the last one asked.
b8 fude_exam_at_last(const fude_exam* _exam) {
    return _exam->current + 1u >= _exam->asked;
}

// Ends a stroke still being drawn, marks the answer answered (to be read), and moves on: to the next character, or
// after the last to the results.
void fude_exam_next(fude_exam* _exam) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it == NULL) {
        return;
    }
    if(_exam->pen) {
        fude_ink_end(&_it->ink);
        _exam->pen = false;
    }
    _it->answered = true;
    if(_exam->current + 1u < _exam->asked) {
        _exam->current++;
    } else {
        _exam->stage = FUDE_EXAM_RESULTS;   // Answers still being read show as such.
        fude_scroller_stop(&_exam->scroller);
        _exam->scroller.offset = 0.0f;
    }
}

// --- Marking ---------------------------------------------------------------------------------------------------------

// Marks one answer. It is right when the character asked is among the first FUDE_EXAM_CANDIDATES candidates of
// recognition: ML Kit's readings of one character in _r (NULL when there are none), then the matcher's ranking. Its
// quality is how well it was written against the character asked, and its points are that quality when it is right,
// 0 when not.
RDE_INTERNAL void fude_exam_mark(fude_exam* _exam, fude_exam_item* _it, const fude_recognition* _r) {
    _it->graded  = true;
    _it->correct = false;
    _it->score   = _it->quality = 0.0f;
    _it->read_as = UINT32_MAX;
    if(fude_ink_alive_strokes(&_it->ink) == 0) {
        return;   // Nothing written is wrong.
    }

    fude_match_result _matched[16];
    fude_match_result _candidates[8];
    const u32 _m = fude_match_rank(_exam->db, _exam->catalog, FUDE_FILTER_ALL, &_it->ink, _matched, 16u);
    const u32 _n = fude_recognize_candidates(_exam->db, _r, 0u, 1u, _exam->catalog, FUDE_FILTER_ALL, _matched, _m, _candidates, 8u);
    // A joined form counts as its letter (recognize.h: fude_recognize_same).
    for(u32 _i = 0; _i < _n && _i < FUDE_EXAM_CANDIDATES; _i++) {
        _it->correct = _it->correct || fude_recognize_same(_exam->db, _candidates[_i].record, _it->record);   // Forms count as letters.
    }
    _it->read_as = _n > 0 ? _candidates[0].record : UINT32_MAX;

    fude_kanji_info _info;
    if(fude_kanji_at(_exam->db, _it->record, &_info)) {
        const fude_score _s = fude_score_drawing(_exam->db, &_info, &_it->ink);
        _it->quality = _s.empty ? 0.0f : _s.score;
    }
    _it->score = _it->correct ? _it->quality : 0.0f;
}

// Returns whether every answer asked has been read.
b8 fude_exam_graded(const fude_exam* _exam) {
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        if(!_exam->items[_exam->order[_i]].graded) {
            return false;
        }
    }
    return true;
}

// Returns how many answers asked have been read.
RDE_INTERNAL u32 fude_exam_graded_count(const fude_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        _n += _exam->items[_exam->order[_i]].graded ? 1u : 0u;
    }
    return _n;
}

// Reads the answers given, one at a time. ML Kit reads it while it is on and has its model: it answers a frame or more
// later and is given FUDE_EXAM_GRADE_WAIT seconds from when it was asked (grading_since). Otherwise the matcher reads
// it alone, one answer a frame.
RDE_INTERNAL void fude_exam_grade(fude_exam* _exam) {
    const f64 _now = rde_engine_get_time_now();
    if(_exam->grading != UINT32_MAX) {
        fude_exam_item* _it = &_exam->items[_exam->grading];
        if(fude_recognize_poll(&_exam->recognition)) {
            fude_exam_mark(_exam, _it, &_exam->recognition);
            _exam->grading = UINT32_MAX;
        } else if(_now - _exam->grading_since > FUDE_EXAM_GRADE_WAIT) {
            fude_recognize_forget(&_exam->recognition);   // No answer in time: the matcher decides alone.
            fude_exam_mark(_exam, _it, NULL);
            _exam->grading = UINT32_MAX;
        }
        return;
    }
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        fude_exam_item* _it = &_exam->items[_exam->order[_i]];
        if(!_it->answered || _it->graded) {
            continue;
        }
        if(fude_ink_alive_strokes(&_it->ink) > 0 && fude_recognize_available()) {
            if(fude_recognize_start(&_exam->recognition, &_it->ink)) {
                _exam->grading       = _exam->order[_i];
                _exam->grading_since = _now;
            }
            return;   // Asked, or ML Kit is busy elsewhere and it is tried on a later frame.
        }
        fude_exam_mark(_exam, _it, NULL);
        return;   // One a frame, since the matcher takes a moment.
    }
}

// Keeps the exam once every answer is marked: it is added to the log with each answer's writing, and the marks follow.
// A character right in FUDE_EXAM_KNOWN_STREAK exams in a row becomes Known, and a Known one that is wrong is Studying
// again. Every answer also moves its character's review schedule.
RDE_INTERNAL void fude_exam_keep(fude_exam* _exam) {
    // Scratch for this one exam, each as long as an exam can be (FUDE_EXAM_MAX, its most characters), on the stack:
    // the log, the marks and the reviews copy what they keep.
    fude_examlog_item        _log[FUDE_EXAM_MAX];
    const fude_ink*          _drawings[FUDE_EXAM_MAX];
    u32                      _known[FUDE_EXAM_MAX];
    u32                      _studying[FUDE_EXAM_MAX];
    u32                      _nk = 0;
    u32                      _ns = 0;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        const fude_exam_item* _it = &_exam->items[_exam->order[_i]];
        fude_kanji_info _info, _read;
        fude_kanji_at(_exam->db, _it->record, &_info);
        _log[_i] = (fude_examlog_item){
            .codepoint = _info.codepoint, .correct = _it->correct, .score = _it->score, .quality = _it->quality,
            .read_as   = _it->read_as != UINT32_MAX && fude_kanji_at(_exam->db, _it->read_as, &_read) ? _read.codepoint : 0u
        };
        _drawings[_i] = &_it->ink;
    }
    fude_examlog_add((u64)rde_date_now_unix_seconds(), (u8)_exam->source, _log, _exam->asked, _drawings, FUDE_EXAM_UNITS);

    for(u32 _i = 0; _i < _exam->asked; _i++) {
        const u32 _cp = _log[_i].codepoint;
        if(_log[_i].correct && fude_examlog_streak(_cp) >= FUDE_EXAM_KNOWN_STREAK && fude_marks_get(_cp) != FUDE_MARK_KNOWN) {
            _known[_nk++] = _cp;
        } else if(!_log[_i].correct && fude_marks_get(_cp) == FUDE_MARK_KNOWN) {
            _studying[_ns++] = _cp;
        }
    }
    fude_marks_set_many(_known, _nk, FUDE_MARK_KNOWN);
    fude_marks_set_many(_studying, _ns, FUDE_MARK_STUDYING);
    // Every answer written from memory counts as a review (review.h), so the schedules move.
    fude_review_answer _answers[FUDE_EXAM_MAX];
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        _answers[_i] = (fude_review_answer){ _log[_i].codepoint, _log[_i].correct, _log[_i].quality / 100.0f };   // Quality is in points (0 to 100); reviews take 0 to 1.
    }
    fude_reviews_answer(_answers, _exam->asked);
    _exam->saved = true;
}

// An exam closed on its results before every answer was read (Done, or another screen opened from the side panel) is
// still kept: the answers left are read by the matcher alone, all at once. Reading goes on only while the screen is
// open (update), and ML Kit can take seconds an answer, so without this the whole exam (its log, the marks and the
// reviews) would be lost. Quit while writing is different: the learner gave the exam up, and nothing is kept.
RDE_INTERNAL void fude_exam_keep_unread(fude_exam* _exam) {
    if(_exam->stage != FUDE_EXAM_RESULTS || _exam->saved || _exam->asked == 0) {
        return;
    }
    if(_exam->grading != UINT32_MAX) {
        fude_recognize_forget(&_exam->recognition);
        _exam->grading = UINT32_MAX;
    }
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        fude_exam_item* _it = &_exam->items[_exam->order[_i]];
        if(!_it->graded) {
            fude_exam_mark(_exam, _it, NULL);
        }
    }
    fude_exam_keep(_exam);
}

// Closes the exam, ending a stroke still being drawn. An exam on its results is kept even when not every answer has
// been read (fude_exam_keep_unread); one quit while writing is not kept.
void fude_exam_close(fude_exam* _exam) {
    if(_exam->pen) {
        fude_ink_end(&_exam->items[_exam->order[_exam->current]].ink);
        _exam->pen = false;
    }
    fude_exam_keep_unread(_exam);
    _exam->open = false;
}

// See the header: the records of the answers read and wrong, in the order asked.
u32 fude_exam_wrong(const fude_exam* _exam, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked && _n < _max; _i++) {
        const fude_exam_item* _it = &_exam->items[_exam->order[_i]];
        if(_it->graded && !_it->correct) {
            _out[_n++] = _it->record;
        }
    }
    return _n;
}

// Makes the wrong ones, shuffled, an exam of their own and starts writing it, once every answer has been read. The
// exam just taken was kept already, so nothing is lost.
void fude_exam_retry_wrong(fude_exam* _exam) {
    u32       _wrong[FUDE_EXAM_MAX];
    const u32 _n = fude_exam_wrong(_exam, _wrong, FUDE_EXAM_MAX);
    if(_n == 0 || !fude_exam_graded(_exam)) {
        return;
    }
    rde_random_shuffle(&_exam->rng, _wrong, _n, sizeof(u32));
    fude_exam_set_items(_exam, _wrong, _n);
    fude_exam_start(_exam);
}

// See the header: the records asked, in order.
u32 fude_exam_asked(const fude_exam* _exam, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked && _n < _max; _i++) {
        _out[_n++] = _exam->items[_exam->order[_i]].record;
    }
    return _n;
}

// Hands out the result tapped once, as its record: tapped goes back to -1, which means none is waiting.
b8 fude_exam_take_tap(fude_exam* _exam, u32* _record) {
    if(_exam->tapped < 0) {
        return false;
    }
    *_record       = _exam->items[_exam->tapped].record;
    _exam->tapped  = -1;
    return true;
}

// --- The pointer -----------------------------------------------------------------------------------------------------

// Turns a screen point into the answer ink's units: from the square's bottom-left corner, FUDE_EXAM_UNITS across, Y
// up, clamped to the square.
RDE_INTERNAL rde_vec_2F fude_exam_local(const fude_exam* _exam, rde_vec_2F _screen) {
    const f32 _k = FUDE_EXAM_UNITS / _exam->square_size;
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _exam->square_tl.x) * _k, 0.0f, FUDE_EXAM_UNITS),
        rde_math_clamp_f32((_screen.y - (_exam->square_tl.y - _exam->square_size)) * _k, 0.0f, FUDE_EXAM_UNITS)
    };
}

// While writing, a pen in the square starts a stroke; at the other stages any pointer starts a drag or a tap.
void fude_exam_pointer_down(fude_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL) {
        // While writing, only the pen writes, and only inside the square, so a resting hand does nothing.
        const b8 _in = _screen.x >= _exam->square_tl.x && _screen.x <= _exam->square_tl.x + _exam->square_size &&
                       _screen.y <= _exam->square_tl.y && _screen.y >= _exam->square_tl.y - _exam->square_size;
        if(_pen && _in && _exam->square_size > 0.0f) {
            _it->ink.zoom = _exam->square_size / FUDE_EXAM_UNITS;   // The ink's smallest step stays one screen unit.
            fude_ink_begin(&_it->ink, fude_exam_local(_exam, _screen), true);
            _exam->pen = true;
        }
        return;
    }
    fude_scroller_down(&_exam->scroller, _screen, _time);
}

// Extends the stroke, or drags.
void fude_exam_pointer_moved(fude_exam* _exam, rde_vec_2F _screen, f64 _time) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_exam->pen && _it != NULL) {
        fude_ink_extend(&_it->ink, fude_exam_local(_exam, _screen));
        return;
    }
    fude_scroller_moved(&_exam->scroller, _screen, _time);
}

// Ends the stroke, or the drag; a tap is taken by fude_exam_update.
void fude_exam_pointer_up(fude_exam* _exam, f64 _time) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_exam->pen) {
        if(_it != NULL) {
            fude_ink_end(&_it->ink);
        }
        _exam->pen = false;
        return;
    }
    fude_scroller_up(&_exam->scroller, _time);
}

// Returns the index of the grid cell under _p among _cells (the preview's items, or the results' asked ones), or -1
// when there is none.
RDE_INTERNAL i32 fude_exam_cell_at(const fude_exam* _exam, rde_vec_2F _p, u32 _cells) {
    if(_exam->cell <= 0.0f || _p.x < _exam->grid_min.x || _p.x > _exam->grid_max.x || _p.y < _exam->grid_min.y || _p.y > _exam->grid_max.y) {
        return -1;
    }
    const u32 _col = (u32)((_p.x - _exam->grid_min.x) / _exam->cell);
    const u32 _row = (u32)((_exam->grid_max.y - _p.y + _exam->scroller.offset) / _exam->cell);
    const u32 _i   = _row * _exam->columns + _col;
    return _col < _exam->columns && _i < _cells ? (i32)_i : -1;
}

// Acts on a tap at _at, as the stage takes it: a setup chip chooses its source or length, a preview cell ticks or
// unticks its item, and a results cell is kept (as the item's index) for fude_exam_take_tap.
RDE_INTERNAL void fude_exam_tap(fude_exam* _exam, rde_vec_2F _at) {
    switch(_exam->stage) {
        case FUDE_EXAM_SETUP:
            for(u32 _i = 0; _i < _exam->chip_count; _i++) {
                const fude_exam_chip* _c = &_exam->chips[_i];
                if(_at.x >= _c->min.x && _at.x <= _c->max.x && _at.y >= _c->min.y && _at.y <= _c->max.y) {
                    if(_c->kind == 0u) { _exam->source = (FUDE_EXAM_SOURCE_)_c->value; }
                    else               { _exam->length = _c->value; }
                }
            }
            break;
        case FUDE_EXAM_PREVIEW: {
            const i32 _i = fude_exam_cell_at(_exam, _at, _exam->count);
            if(_i >= 0) {
                _exam->items[_i].included = !_exam->items[_i].included;
            }
        } break;
        case FUDE_EXAM_RESULTS: {
            const i32 _i = fude_exam_cell_at(_exam, _at, _exam->asked);
            if(_i >= 0) {
                _exam->tapped = (i32)_exam->order[_i];
            }
        } break;
        default: break;
    }
}

// Scrolls and takes a tap, reads an answer, and keeps the exam once every answer is read.
void fude_exam_update(fude_exam* _exam, f32 _dt) {
    if(!_exam->open) {
        return;
    }
    fude_scroller_update(&_exam->scroller, _dt, _exam->content_h, _exam->grid_max.y - _exam->grid_min.y);
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_exam->scroller, &_at)) {
        fude_exam_tap(_exam, _at);
    }
    fude_exam_grade(_exam);
    if(_exam->stage == FUDE_EXAM_RESULTS && !_exam->saved && _exam->asked > 0 && fude_exam_graded(_exam)) {
        fude_exam_keep(_exam);
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Draws a chip from _min to _max: a pill drawn here rather than a UI widget, because the setup's choices are many and
// change with what there is to choose from. It is filled with the accent when chosen, and its label is greyed out when
// it is not _usable.
RDE_INTERNAL void fude_exam_chip_draw(rde_font* _font, f32 _font_px, rde_vec_2F _min, rde_vec_2F _max, const c8* _label, b8 _chosen, b8 _usable) {
    const fude_theme* _theme = fude_theme_active();
    const rde_vec_2F  _size  = { _max.x - _min.x, _max.y - _min.y };
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x((_min.x + _max.x) * 0.5f), (_min.y + _max.y) * 0.5f }, _size, 1.0f, 10,
                                            _chosen ? _theme->accent : _theme->surface_2, NULL);
    const f32 _px = FUDE_EXAM_CHIP_PX;
    const f32 _w  = fude_draw_text_width(_font, _font_px, _label, _px);
    fude_draw_text(_font, _font_px, _label, (_min.x + _max.x - _w) * 0.5f, (_min.y + _max.y) * 0.5f - _px * 0.42f, _px,
                   !_usable ? _theme->button_text_disabled : _chosen ? _theme->on_accent : _theme->text);
}

// Draws the setup from _top: the header, the chips of the sources (each with how many characters it has) and of the
// lengths, wrapping onto more rows, and a card saying what that choice makes. It records the chips for taps.
RDE_INTERNAL void fude_exam_render_setup(fude_exam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top) {
    const fude_theme* _theme = fude_theme_active();
    fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_EXAM), _font, _font_px, _left, _right, _top, fude_text(FUDE_TEXT_EXAM), fude_text(FUDE_TEXT_EXAM_INTRO), NULL);
    f32 _y = _top - 56.0f;

    // The chips: what the exam is of, then how many.
    _exam->chip_count = 0;
    c8 _label[512];
    for(u32 _group = 0; _group < 2u; _group++) {
        _y -= 30.0f;
        fude_draw_text(_font, _font_px, fude_text(_group == 0 ? FUDE_TEXT_EXAM_WHAT : FUDE_TEXT_EXAM_HOW_MANY), _left, _y, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
        _y -= 12.0f;
        f32       _x   = _left;
        const u32 _n   = _group == 0 ? fude_exam_source_count() : FUDE_EXAM_LENGTHS;
        for(u32 _i = 0; _i < _n; _i++) {
            b8 _usable = true;
            b8 _chosen;
            if(_group == 0) {
                if((_i == fude_exam_source_selection() && rde_arr_length(&_exam->selection) == 0) || _i == fude_exam_source_review()) {
                    continue;   // Selection only when opened with one; Reviews has its own way in.
                }
                const u32 _size = fude_exam_source_size(_exam, (FUDE_EXAM_SOURCE_)_i);
                rde_snprintf(_label, sizeof(_label), "%s  %u", fude_exam_source_name((FUDE_EXAM_SOURCE_)_i), _size);
                _usable = _size > 0;
                _chosen = _exam->source == (FUDE_EXAM_SOURCE_)_i;
            } else {
                rde_snprintf(_label, sizeof(_label), "%s", FUDE_EXAM_LENGTH_NAMES[_i] != NULL ? FUDE_EXAM_LENGTH_NAMES[_i] : fude_text(FUDE_TEXT_ALL));
                _chosen = _exam->length == _i;
            }
            const f32 _w = rde_math_max_f32(64.0f, fude_draw_text_width(_font, _font_px, _label, FUDE_EXAM_CHIP_PX) + 36.0f);
            if(_x + _w > _right && _x > _left) {   // The chip goes on to the next row.
                _x  = _left;
                _y -= FUDE_EXAM_CHIP_H + FUDE_EXAM_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - FUDE_EXAM_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            fude_exam_chip_draw(_font, _font_px, _min, _max, _label, _chosen, _usable);
            _exam->chips[_exam->chip_count++] = (fude_exam_chip){ _min, _max, (u8)_group, (u8)_i };
            _x += _w + FUDE_EXAM_CHIP_GAP;
        }
        _y -= FUDE_EXAM_CHIP_H;
    }

    // What that choice makes, in a card.
    _y -= 24.0f;
    const u32 _size    = fude_exam_source_size(_exam, _exam->source);
    const u32 _planned = fude_exam_planned(_exam);
    // Its two texts wrap to the card's width; it is as tall as they are.
    c8 _first[512], _second[256];
    if(_size == 0) {
        rde_snprintf(_first, sizeof(_first), "%s", fude_text(_exam->source == FUDE_EXAM_SOURCE_STUDYING ? FUDE_TEXT_EXAM_NOTHING_STUDYING : FUDE_TEXT_EXAM_NOTHING));
        rde_snprintf(_second, sizeof(_second), "%s", fude_text(FUDE_TEXT_EXAM_MARK_HINT));
    } else {
        FUDE_TEXTF(_first, FUDE_TEXT_EXAM_PLAN, FUDE_TN(_planned), FUDE_TN(_size), FUDE_TS(fude_exam_source_name(_exam->source)));
        rde_snprintf(_second, sizeof(_second), "%s", fude_text(FUDE_TEXT_EXAM_RULE));
        if(_size > FUDE_EXAM_MAX && FUDE_EXAM_LENGTH_VALUES[_exam->length] == 0u) {
            FUDE_TEXTF(_second, FUDE_TEXT_EXAM_MAX, FUDE_TN(FUDE_EXAM_MAX));
        }
    }
    const f32 _tw  = _right - _left - 36.0f;
    const f32 _tw1 = fude_draw_text_balanced_width(_font, _font_px, _first, 15.0f, _tw);
    const f32 _tw2 = fude_draw_text_balanced_width(_font, _font_px, _second, 12.0f, _tw);
    const u32 _l1  = fude_draw_text_wrap_lines(_font, _font_px, _first, 15.0f, _tw1);
    const u32 _l2  = fude_draw_text_wrap_lines(_font, _font_px, _second, 12.0f, _tw2);
    const f32 _more1 = (f32)(_l1 > 1u ? _l1 - 1u : 0u) * 19.0f;
    const f32 _more2 = (f32)(_l2 > 1u ? _l2 - 1u : 0u) * 16.0f;
    fude_draw_card((rde_vec_2F){ _left, _y - 76.0f - _more1 - _more2 }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
    fude_draw_text_wrap(_font, _font_px, _first, _left + 18.0f, _y - 32.0f, 15.0f, _tw1, 19.0f, _theme->text);
    fude_draw_text_wrap(_font, _font_px, _second, _left + 18.0f, _y - 56.0f - _more1, 12.0f, _tw2, 16.0f, _theme->text_soft);
}

// Lays out a grid between _top and _bottom for _count cells of at least _cell_min, and keeps the layout for taps and
// scrolling. It draws nothing.
RDE_INTERNAL void fude_exam_grid(fude_exam* _exam, f32 _left, f32 _right, f32 _top, f32 _bottom, f32 _cell_min, u32 _count) {
    const f32 _width = _right - _left;
    _exam->columns   = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32(_width / _cell_min));
    _exam->cell      = _width / (f32)_exam->columns;
    _exam->grid_min  = (rde_vec_2F){ _left, _bottom };
    _exam->grid_max  = (rde_vec_2F){ _right, _top };
    _exam->content_h = (f32)((_count + _exam->columns - 1u) / _exam->columns) * _exam->cell;
}

// Draws the preview: the header with how many are ticked, then the grid of the characters chosen, each ticked or not,
// the unticked ones faint.
RDE_INTERNAL void fude_exam_render_preview(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    c8 _line[512];
    FUDE_TEXTF(_line, FUDE_TEXT_OF_N, FUDE_TN(fude_exam_included(_exam)), FUDE_TN(_exam->count));
    c8 _title[512];
    FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
    fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_EXAM), _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_EXAM_TAP_TO_LEAVE), _line);
    RDE_UNUSED(_theme);

    const f32 _grid_top = _top - 60.0f;
    fude_exam_grid(_exam, _left, _right, _grid_top, _bottom, FUDE_EXAM_CELL_MIN, _exam->count);
    if(_grid_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_grid_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_grid_top - _bottom) });
    const f32 _cell  = _exam->cell;
    const f32 _glyph = _cell * 0.62f;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        const f32 _x = _left + (f32)(_i % _exam->columns) * _cell;
        const f32 _y = _grid_top - (f32)(_i / _exam->columns) * _cell + _exam->scroller.offset;
        if(_y - _cell > _grid_top || _y < _bottom) {
            continue;
        }
        fude_kanji_info _info;
        if(!fude_kanji_at(_exam->db, _exam->items[_i].record, &_info)) {
            continue;
        }
        const b8 _on = _exam->items[_i].included;
        fude_selection_draw_behind(_on, (rde_vec_2F){ _x, _y }, _cell);
        fude_glyph_character(&_exam->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - (_cell - _glyph) * 0.5f }, _glyph,
                             _on ? _theme->ink : _theme->ghost);
        fude_selection_draw_tick(_on, (rde_vec_2F){ _x, _y }, _cell);
    }
    rde_rendering_end_clipping_rect();
}

// Finds a word with the character in it and writes it into _blanked (_size bytes) with the character replaced by □:
// the learner's own words first (vocab.h), then the character data's. Only a word of two characters or more is taken,
// since the character alone would leave nothing to show. The word goes into *_out. Returns false when there is none.
RDE_INTERNAL b8 fude_exam_blanked_word(fude_exam* _exam, const fude_kanji_info* _info, u32 _record, c8* _blanked, usize _size, fude_kanji_word* _out) {
    u32       _words[FUDE_KANJI_MAX_WORDS];
    const u32 _yours = fude_vocab_kanji_count(_info->codepoint);
    const u32 _nw    = fude_kanji_words(_exam->db, _record, _words, FUDE_KANJI_MAX_WORDS, NULL);
    for(u32 _w = 0; _w < _yours + _nw; _w++) {
        if(_w < _yours ? !fude_vocab_kanji_at(_info->codepoint, _w, _out) : !fude_kanji_word_at(_exam->db, _words[_w - _yours], _out)) {
            continue;
        }
        usize     _len   = 0;
        u32       _chars = 0;
        const c8* _p     = _out->written;
        _blanked[0] = 0;
        for(u32 _cp = fude_utf8_next(&_p); _cp != 0; _cp = fude_utf8_next(&_p)) {
            c8 _one[8];
            if(_cp == _info->codepoint) { rde_snprintf(_one, sizeof(_one), "\xE2\x96\xA1"); }   // U+25A1, the blank: □
            else                        { fude_utf8_put(_cp, _one); }
            // Only whole characters that fit: a long word (the learner's own can be any length) is cut at the last
            // one that does, rather than written past _blanked.
            const usize _one_len = rde_strlen(_one);
            if(_len + _one_len + 1u > _size) {
                break;
            }
            rde_memcpy(_blanked + _len, _one, _one_len + 1u);
            _len += _one_len;
            _chars++;
        }
        if(_chars >= 2u) {
            return true;
        }
    }
    return false;
}

// Draws the prompt in a card from _y down. For a character with readings (a kanji) it is the meaning, the readings
// (each after the language's badge, such as 音 and 訓) and a word with the character blanked out; for a script's letter
// (_kana: a kana) it is its Latin name. Returns the y under the card.
RDE_INTERNAL f32 fude_exam_prompt(fude_exam* _exam, const fude_kanji_info* _info, u32 _record, b8 _kana, rde_font* _font, f32 _font_px,
                                  f32 _left, f32 _right, f32 _y) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _pad   = 18.0f;
    const f32         _x     = _left + _pad;

    // The card's height: the caption and the big line, then a line for the readings, and one for the word.
    const c8* _first  = _kana ? "" : fude_kanji_reading(_exam->db, _info, 0u);   // The language's first kind of reading...
    const c8* _second = _kana ? "" : fude_kanji_reading(_exam->db, _info, 1u);   // ...and its second kind.
    c8              _blanked[96];
    fude_kanji_word _word;
    const b8        _has_word = !_kana && fude_exam_blanked_word(_exam, _info, _record, _blanked, sizeof(_blanked), &_word);
    const u32       _readings = (_first[0] != 0 ? 1u : 0u) + (_second[0] != 0 ? 1u : 0u);
    const f32       _h = _pad + 18.0f + (_kana ? 64.0f : 40.0f) + (_readings > 0 ? 44.0f : 0.0f) + (_has_word ? 52.0f : 0.0f) + _pad - 6.0f;
    fude_draw_card((rde_vec_2F){ _left, _y - _h }, (rde_vec_2F){ _right, _y }, 16.0f, _theme->surface, _theme->outline);

    f32 _cy = _y - _pad - 10.0f;
    if(_kana) {
        fude_draw_text(_font, _font_px, fude_text((FUDE_TEXT_)fude_lang_group_prompt(fude_lang_group(_info->codepoint))), _x, _cy, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
        fude_draw_text(_font, _font_px, fude_lang_latin(_info->codepoint), _x, _cy - 54.0f, 40.0f, _theme->text);
        return _y - _h;
    }
    fude_draw_text(_font, _font_px, fude_text((FUDE_TEXT_)fude_lang_group_prompt(fude_lang_group(_info->codepoint))), _x, _cy, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
    const c8* _meaning = fude_kanji_meanings(_exam->db, _info);
    fude_draw_text_whole(_font, _font_px, _meaning[0] != 0 ? _meaning : fude_text(FUDE_TEXT_NO_MEANING), _x, _cy - 34.0f + FUDE_EXAM_MEANING_PX * 0.38f,
                         FUDE_EXAM_MEANING_PX, _right - _pad - _x, 1u, _theme->text);
    _cy -= 40.0f;

    // The readings side by side, each after its badge.
    if(_readings > 0) {
        const f32 _mid = _cy - 24.0f;
        f32       _rx  = _x;
        for(u32 _k = 0; _k < fude_lang_reading_kinds() && _k < 2u; _k++) {
            const c8* _text = _k == 0u ? _first : _second;
            if(_text[0] == 0 || _rx > _right - 80.0f) {
                continue;
            }
            fude_header_badge(&_exam->glyph, fude_lang_badge(_k == 0u ? FUDE_LANG_BADGE_READING_0 : FUDE_LANG_BADGE_READING_1), (rde_vec_2F){ _rx, _mid + 15.0f }, 30.0f);   // Such as 音 and 訓.
            const f32 _end = fude_glyph_reading(&_exam->glyph, _text, (rde_vec_2F){ _rx + 42.0f, _mid + 12.0f }, 24.0f,
                                                _k == 0u && _second[0] != 0 ? (_left + _right) * 0.5f + 40.0f : _right - _pad, _theme->ink, _theme->text_soft);
            _rx = rde_math_max_f32(_end + 28.0f, _k == 0u ? _x : _rx);
        }
        _cy -= 44.0f;
    }

    // The word, on the page's colour inside the card.
    if(_has_word) {
        const f32 _mid = _cy - 26.0f;
        fude_draw_card((rde_vec_2F){ _x - 6.0f, _mid - 20.0f }, (rde_vec_2F){ _right - _pad + 6.0f, _mid + 20.0f }, 10.0f, _theme->page, _theme->page);
        fude_draw_text(_font, _font_px, _blanked, _x + 6.0f, _mid - FUDE_EXAM_WORD_PX * 0.38f, FUDE_EXAM_WORD_PX, _theme->ink);
        const f32 _rx = _x + 6.0f + fude_draw_text_width(_font, _font_px, _blanked, FUDE_EXAM_WORD_PX) + 14.0f;
        fude_draw_text(_font, _font_px, _word.reading, _rx, _mid - 13.0f * 0.42f, 13.0f, _theme->text_soft);
        const f32 _mx = _rx + fude_draw_text_width(_font, _font_px, _word.reading, 13.0f) + 14.0f;
        fude_draw_text_whole(_font, _font_px, _word.meaning, _mx, _mid - 13.0f * 0.04f, 13.0f, _right - _pad - _mx, 2u, _theme->text);
    }
    return _y - _h;
}

// Draws the writing stage: the header and the progress, the prompt, and the answer square with the answer's ink. It
// records the square for the pointer.
RDE_INTERNAL void fude_exam_render_writing(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    fude_exam_item*   _it    = fude_exam_current(_exam);
    fude_kanji_info   _info;
    if(_it == NULL || !fude_kanji_at(_exam->db, _it->record, &_info)) {
        return;
    }

    // The title, how far through it is, and the progress: a segment per character, or a single bar when there are more
    // than 30.
    c8 _title[512];
    c8 _line[512];
    FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
    FUDE_TEXTF(_line, FUDE_TEXT_OF_N, FUDE_TN(_exam->current + 1u), FUDE_TN(_exam->asked));
    fude_header_draw(&_exam->glyph, fude_lang_badge(FUDE_LANG_BADGE_EXAM), _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_EXAM_ONCE_EACH), _line);
    const f32 _py = _top - 54.0f;
    if(_exam->asked > 0 && _exam->asked <= 30u) {
        const f32 _gap = 3.0f;
        const f32 _w   = (_right - _left - _gap * (f32)(_exam->asked - 1u)) / (f32)_exam->asked;
        for(u32 _i = 0; _i < _exam->asked; _i++) {
            const rde_color _c = _i < _exam->current ? _theme->accent : _i == _exam->current ? _theme->tint : _theme->surface_2;
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_left + (f32)_i * (_w + _gap) + _w * 0.5f), _py }, (rde_vec_2F){ _w, 6.0f }, 1.0f, 4, _c, NULL);
        }
    } else if(_exam->asked > 0) {
        const f32 _done = (_right - _left) * (f32)_exam->current / (f32)_exam->asked;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x((_left + _right) * 0.5f), _py }, (rde_vec_2F){ _right - _left, 6.0f }, 1.0f, 4, _theme->surface_2, NULL);
        if(_done > 6.0f) {
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_left + _done * 0.5f), _py }, (rde_vec_2F){ _done, 6.0f }, 1.0f, 4, _theme->accent, NULL);
        }
    }

    // What to write. A script's letter (a kana) is one with a Latin name and no text of its own.
    const b8 _kana = fude_lang_latin(_info.codepoint) != NULL && _info.text == UINT32_MAX;
    const f32 _y   = fude_exam_prompt(_exam, &_info, _it->record, _kana, _font, _font_px, _left, _right, _py - 16.0f);

    // The answer square: as big as fits under the prompt, never smaller than 120 nor larger than FUDE_EXAM_SQUARE_MAX.
    const f32 _room   = _y - 18.0f - (_bottom + 26.0f);
    const f32 _square = rde_math_max_f32(120.0f, rde_math_min_f32(rde_math_min_f32(_right - _left, _room), FUDE_EXAM_SQUARE_MAX));
    const rde_vec_2F _tl = { (_left + _right) * 0.5f - _square * 0.5f, _y - 18.0f };
    _exam->square_tl   = _tl;
    _exam->square_size = _square;
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _square }, (rde_vec_2F){ _tl.x + _square, _tl.y }, true);   // Handwriting is never mirrored (draw.h).
    fude_glyph_box(_tl, _square);
    const rde_vec_2I _size = rde_window_get_size(_window);
    fude_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_EXAM_UNITS,
                    (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
    fude_draw_keep_end();
    fude_draw_text(_font, _font_px, fude_text(fude_exam_at_last(_exam) ? FUDE_TEXT_EXAM_THEN_FINISH : FUDE_TEXT_EXAM_THEN_NEXT),
                   _tl.x, _tl.y - _square - 20.0f, 12.0f, _theme->text_soft);
}

// Draws the results: how many are being read, or once all are, how many were right, the mean points and whether it
// passed; then each answer's writing in a grid, with what was asked, right or wrong, and its points or what it was read
// as.
RDE_INTERNAL void fude_exam_render_results(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    c8 _line[512];
    if(!fude_exam_graded(_exam)) {
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_READING_ANSWERS, FUDE_TN(fude_exam_graded_count(_exam)), FUDE_TN(_exam->asked));
        fude_draw_text(_font, _font_px, _line, _left, _top - 30.0f, FUDE_EXAM_TITLE_PX, _theme->text);
    } else {
        u32 _right_n = 0;
        f32 _points  = 0.0f;
        for(u32 _i = 0; _i < _exam->asked; _i++) {
            _right_n += _exam->items[_exam->order[_i]].correct ? 1u : 0u;
            _points  += _exam->items[_exam->order[_i]].score;
        }
        const b8 _passed = (f32)_right_n >= FUDE_EXAM_PASS * (f32)_exam->asked;
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_RESULT, FUDE_TN(_right_n), FUDE_TN(_exam->asked), FUDE_TN(rde_math_round_to_i64_f32(_points / (f32)_exam->asked)));
        fude_draw_text(_font, _font_px, _line, _left, _top - 30.0f, FUDE_EXAM_TITLE_PX, _theme->text);
        fude_draw_chip(_font, _font_px, fude_text(_passed ? FUDE_TEXT_EXAM_PASSED : FUDE_TEXT_EXAM_NOT_PASSED), _left + fude_draw_text_width(_font, _font_px, _line, FUDE_EXAM_TITLE_PX) + 16.0f,
                       _top - 30.0f + FUDE_EXAM_TITLE_PX * 0.42f, 12.0f, _passed ? _theme->score_good : _theme->score_poor, _theme->on_accent);
    }

    // For a kept exam, a line saying which exam it was and when.
    f32 _grid_top = _top - 56.0f;
    if(_exam->kept < fude_examlog_count()) {
        c8 _title[512], _when[96];
        FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
        fude_text_date_time(_when, sizeof(_when), fude_examlog_exams()[_exam->kept].time);
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_KEPT, FUDE_TS(_title), FUDE_TS(_when));
        fude_draw_text(_font, _font_px, _line, _left, _top - 54.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, FUDE_EXAM_CAPTION_PX, _right - _left),
                       _theme->text_soft);
        _grid_top = _top - 72.0f;
    }
    fude_exam_grid(_exam, _left, _right, _grid_top, _bottom, FUDE_EXAM_RESULT_MIN, _exam->asked);
    if(_grid_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_grid_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_grid_top - _bottom) });
    const rde_vec_2I _size = rde_window_get_size(_window);
    const f32        _cell = _exam->cell;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        const f32 _x = _left + (f32)(_i % _exam->columns) * _cell;
        const f32 _y = _grid_top - (f32)(_i / _exam->columns) * _cell + _exam->scroller.offset;
        if(_y - _cell > _grid_top || _y < _bottom) {
            continue;
        }
        fude_exam_item* _it = &_exam->items[_exam->order[_i]];
        fude_kanji_info _info;
        if(!fude_kanji_at(_exam->db, _it->record, &_info)) {
            continue;
        }
        // The writing in its square, with what was asked small in its corner.
        const f32 _sq = _cell - 10.0f;
        const rde_vec_2F _tl = { _x + 5.0f, _y - 5.0f };
        fude_glyph_box(_tl, _sq);
        fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _sq }, (rde_vec_2F){ _tl.x + _sq, _tl.y }, false);   // Handwriting is never mirrored (draw.h).
        fude_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _sq }, _sq / FUDE_EXAM_UNITS,
                        (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
        fude_draw_keep_end();
        fude_glyph_character(&_exam->glyph, _info.codepoint, (rde_vec_2F){ _tl.x + 4.0f, _tl.y - 4.0f }, _sq * 0.26f, _theme->text_soft);

        // Right or wrong, and the points. An answer still being read shows an empty circle.
        const f32        _r = rde_math_max_f32(10.0f, _sq * 0.1f);
        const rde_vec_2F _c = { _tl.x + _sq - _r - 5.0f, _tl.y - _r - 5.0f };
        if(!_it->graded) {
            rde_rendering_2d_draw_circle_border(fude_draw_at(_c), _r, 1.5f, 24, _theme->text_soft, NULL);
            continue;
        }
        fude_draw_verdict(_c, _r, _it->correct);
        if(_it->correct) {
            rde_snprintf(_line, sizeof(_line), "%.0f", (f64)_it->score);
            fude_draw_text(_font, _font_px, _line, _tl.x + _sq - fude_draw_text_width(_font, _font_px, _line, 18.0f) - 6.0f, _tl.y - _sq + 8.0f, 18.0f,
                           fude_theme_grade(_it->score));
        } else if(_it->read_as != UINT32_MAX && _it->read_as != _it->record) {
            // What it was read as instead.
            fude_kanji_info _read;
            if(fude_kanji_at(_exam->db, _it->read_as, &_read)) {
                fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_EXAM_READ_AS), _tl.x + 6.0f, _tl.y - _sq + 8.0f, 11.0f, _theme->text_soft);
                const f32 _after = _tl.x + 12.0f + fude_draw_text_width(_font, _font_px, fude_text(FUDE_TEXT_EXAM_READ_AS), 11.0f);
                fude_glyph_character(&_exam->glyph, _read.codepoint, (rde_vec_2F){ _after, _tl.y - _sq + 26.0f }, 20.0f, _theme->score_poor);
            }
        }
    }
    rde_rendering_end_clipping_rect();
}

// Draws the stage. The setup has no grid, so its grid and content height are zeroed (for the scroller).
void fude_exam_render(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_exam->open) {
        return;
    }
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // The order is left, top, right, bottom.
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_EXAM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_EXAM_MARGIN;
    switch(_exam->stage) {
        case FUDE_EXAM_SETUP:   fude_exam_render_setup(_exam, _font, _font_px, _left, _right, _top); _exam->grid_min = _exam->grid_max = (rde_vec_2F){ 0 }; _exam->content_h = 0.0f; break;
        case FUDE_EXAM_PREVIEW: fude_exam_render_preview(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case FUDE_EXAM_WRITING: fude_exam_render_writing(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case FUDE_EXAM_RESULTS: fude_exam_render_results(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
    }
}

// --- The screen (screen.h): its rows of buttons, and what they do ----------------------------------------------------

#include "study/app/study.h"
#include "drawing/base/icons.h"

// The screen's is_open, close, pointer and render functions, which call the exam's own (screen.h); the pen writes.
FUDE_SCREEN_ADAPTERS_PEN(fude_exam, fude_exam)
FUDE_SCREEN_RENDER(fude_exam, fude_exam)

// Updates the exam, which reads its answers as it goes, then hands on a character tapped in the results: the viewer
// opens on it, able to walk the exam's characters.
RDE_INTERNAL void fude_exam_screen_update(fude_app* _app, any _self, f32 _dt) {
    fude_exam* _exam = (fude_exam*)_self;
    fude_exam_update(_exam, _dt);
    u32 _tapped;
    if(fude_exam_take_tap(_exam, &_tapped)) {
        u32       _asked[FUDE_EXAM_MAX];
        const u32 _n = fude_exam_asked(_exam, _asked, FUDE_EXAM_MAX);
        for(u32 _i = 0; _i < _n; _i++) {
            if(_asked[_i] == _tapped) {
                fude_study_view(_app, _asked, _n, _i);
                break;
            }
        }
    }
}

// The buttons that call one of the exam's functions: Close, Quit and Done close it; Next n goes on to the preview,
// Back back to the setup, Start n starts writing; Undo, Clear and Next act on the answer; Retry wrong retries.
FUDE_ROW_CALL(fude_exam_row_close,   fude_exam, fude_exam_close)
FUDE_ROW_CALL(fude_exam_row_preview, fude_exam, fude_exam_preview)
FUDE_ROW_CALL(fude_exam_row_back,    fude_exam, fude_exam_back)
FUDE_ROW_CALL(fude_exam_row_start,   fude_exam, fude_exam_start)
FUDE_ROW_CALL(fude_exam_row_undo,    fude_exam, fude_exam_undo)
FUDE_ROW_CALL(fude_exam_row_clear,   fude_exam, fude_exam_clear)
FUDE_ROW_CALL(fude_exam_row_next,    fude_exam, fude_exam_next)
FUDE_ROW_CALL(fude_exam_row_retry,   fude_exam, fude_exam_retry_wrong)

// All and None: tick every character in the preview (_arg 1) or none (_arg 0).
RDE_INTERNAL void fude_exam_row_tick(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_exam_tick_all((fude_exam*)_self, _arg != 0u);
}

// Practice wrong: opens Practice on the wrong ones, as a set, over the results.
RDE_INTERNAL void fude_exam_row_practice(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _wrong[FUDE_EXAM_MAX];
    const u32 _n = fude_exam_wrong((const fude_exam*)_self, _wrong, FUDE_EXAM_MAX);
    fude_study_practice_set(_app, _wrong, _n);
}

// One row per stage, in FUDE_EXAM_STAGE_ order: the setup's, the preview's, the writing's and the results'. The enums
// are the indices of the buttons whose faces change.
enum { FUDE_EXAM_SETUP_NEXT = 1 };
enum { FUDE_EXAM_PREVIEW_START = 3 };
enum { FUDE_EXAM_WRITE_NEXT = 3 };
enum { FUDE_EXAM_RESULTS_RETRY = 1, FUDE_EXAM_RESULTS_PRACTICE };
static const fude_row_button FUDE_EXAM_SETUP_ROW[] = {
    { FUDE_TEXT_CLOSE,  FUDE_ICON_CLOSE,       fude_exam_row_close,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NEXT_N, FUDE_ICON_ARROW_RIGHT, fude_exam_row_preview, 0, FUDE_ROW_PRIMARY, true,  NULL },
};
static const fude_row_button FUDE_EXAM_PREVIEW_ROW[] = {
    { FUDE_TEXT_BACK,    FUDE_ICON_BACK,        fude_exam_row_back,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_ALL,     FUDE_ICON_SELECT_ALL,  fude_exam_row_tick,  1, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NONE,    FUDE_ICON_SELECT_NONE, fude_exam_row_tick,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_START_N, FUDE_ICON_PLAY,        fude_exam_row_start, 0, FUDE_ROW_PRIMARY, true,  NULL },
};
static const fude_row_button FUDE_EXAM_WRITE_ROW[] = {
    { FUDE_TEXT_QUIT,  FUDE_ICON_CLOSE,       fude_exam_row_close, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_UNDO,  FUDE_ICON_UNDO,        fude_exam_row_undo,  0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_CLEAR, FUDE_ICON_TRASH,       fude_exam_row_clear, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NEXT,  FUDE_ICON_ARROW_RIGHT, fude_exam_row_next,  0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_button FUDE_EXAM_RESULTS_ROW[] = {
    { FUDE_TEXT_DONE,           FUDE_ICON_CHECK, fude_exam_row_close,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_RETRY_WRONG,    FUDE_ICON_RETRY, fude_exam_row_retry,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE_WRONG, FUDE_ICON_PEN,   fude_exam_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_def FUDE_EXAM_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_EXAM_SETUP_ROW), FUDE_ROW_DEF(FUDE_EXAM_PREVIEW_ROW), FUDE_ROW_DEF(FUDE_EXAM_WRITE_ROW),
                                               FUDE_ROW_DEF(FUDE_EXAM_RESULTS_ROW) };

// Which row is up: the stage's.
RDE_INTERNAL u32 fude_exam_screen_row(const any _self) {
    return (u32)((const fude_exam*)_self)->stage;
}

// How the buttons show: Next n says how many are planned and Start n how many are ticked (each greyed out at 0); the
// writing's Next becomes Finish on the last; Retry and Practice wrong are greyed out until every answer is read and
// while none is wrong.
RDE_INTERNAL void fude_exam_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    const fude_exam* _exam = (const fude_exam*)_self;
    if(_row == FUDE_EXAM_SETUP) {
        const u32 _planned = fude_exam_planned(_exam);
        fude_row_face_count(&_faces[FUDE_EXAM_SETUP_NEXT], FUDE_TEXT_NEXT_N, _planned);
        _faces[FUDE_EXAM_SETUP_NEXT].disabled = _planned == 0u;
    } else if(_row == FUDE_EXAM_PREVIEW) {
        const u32 _included = fude_exam_included(_exam);
        fude_row_face_count(&_faces[FUDE_EXAM_PREVIEW_START], FUDE_TEXT_START_N, _included);
        _faces[FUDE_EXAM_PREVIEW_START].disabled = _included == 0u;
    } else if(_row == FUDE_EXAM_WRITING) {
        fude_row_face_next(&_faces[FUDE_EXAM_WRITE_NEXT], fude_exam_at_last(_exam));
    } else {
        u32      _wrong[FUDE_EXAM_MAX];
        const b8 _none = !fude_exam_graded(_exam) || fude_exam_wrong(_exam, _wrong, FUDE_EXAM_MAX) == 0u;
        _faces[FUDE_EXAM_RESULTS_RETRY].disabled    = _none;
        _faces[FUDE_EXAM_RESULTS_PRACTICE].disabled = _none;
    }
}

const fude_screen FUDE_EXAM_SCREEN = {
    .name = "exam", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_exam_screen_is_open, .close = fude_exam_screen_close,
    .update = fude_exam_screen_update, .render = fude_exam_screen_render,
    .pointer_down = fude_exam_screen_down, .pointer_moved = fude_exam_screen_moved, .pointer_up = fude_exam_screen_up,
    .rows = FUDE_EXAM_BUTTON_ROWS, .row_count = 4u, .row = fude_exam_screen_row, .faces = fude_exam_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
