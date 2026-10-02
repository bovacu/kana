#include "study/screens/exam.h"
#include "study/widgets/header.h"
#include "study/models/review.h"
#include "lang/ja/chart.h"
#include "lang/ja/romaji.h"
#include "drawing/widgets/draw.h"
#include "study/models/examlog.h"
#include "study/models/marks.h"
#include "study/handwriting/match.h"
#include "study/handwriting/score.h"
#include "study/models/select.h"
#include "drawing/base/theme.h"
#include "study/models/vocab.h"
#include "drawing/base/text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See exam.h.
// ===========================================================================

#define FUDE_EXAM_MARGIN      24.0f
#define FUDE_EXAM_CHIP_H      40.0f
#define FUDE_EXAM_CHIP_GAP    8.0f
#define FUDE_EXAM_CELL_MIN    88.0f     // the preview's grid, as Browse's
#define FUDE_EXAM_RESULT_MIN  132.0f    // the results' grid: room to see the writing
#define FUDE_EXAM_SQUARE_MAX  520.0f
#define FUDE_EXAM_TITLE_PX    17.0f     // sizes: a screen's title
#define FUDE_EXAM_CAPTION_PX  10.0f     // a caption
#define FUDE_EXAM_CHIP_PX     13.0f     // a chip's label
#define FUDE_EXAM_MEANING_PX  22.0f     // the prompt's meaning
#define FUDE_EXAM_WORD_PX     19.0f     // the prompt's word
#define FUDE_EXAM_GRADE_WAIT  4.0       // seconds ML Kit may take on one answer before the matcher decides alone
#define FUDE_EXAM_PEN_RADIUS  (FUDE_EXAM_UNITS * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f)

static const u32       FUDE_EXAM_LENGTH_VALUES[FUDE_EXAM_LENGTHS] = { 10u, 20u, 50u, 0u };   // 0: all
static const c8* const FUDE_EXAM_LENGTH_NAMES[FUDE_EXAM_LENGTHS]  = { "10", "20", "50", NULL };   // NULL: All (text.h)
// The sources' names (text.h); the JLPT levels are the same in every language.
static const FUDE_TEXT_ FUDE_EXAM_SOURCE_TEXTS[FUDE_EXAM_SOURCE_COUNT] = {
    FUDE_TEXT_STUDYING, FUDE_TEXT_KNOWN, FUDE_TEXT_COUNT, FUDE_TEXT_COUNT, FUDE_TEXT_COUNT, FUDE_TEXT_COUNT, FUDE_TEXT_COUNT,
    FUDE_TEXT_HIRAGANA, FUDE_TEXT_KATAKANA, FUDE_TEXT_EXAM_SELECTION, FUDE_TEXT_REVIEWS
};
static const c8* const FUDE_EXAM_SOURCE_LEVELS[FUDE_EXAM_SOURCE_COUNT] = { NULL, NULL, "N5", "N4", "N3", "N2", "N1", NULL, NULL, NULL, NULL };

const c8* fude_exam_source_name(FUDE_EXAM_SOURCE_ _source) {
    if(_source >= FUDE_EXAM_SOURCE_COUNT) {
        return "";
    }
    return FUDE_EXAM_SOURCE_LEVELS[_source] != NULL ? FUDE_EXAM_SOURCE_LEVELS[_source] : fude_text(FUDE_EXAM_SOURCE_TEXTS[_source]);
}

RDE_INTERNAL void fude_exam_fresh_ink(fude_ink* _ink) {
    fude_ink_init(_ink);
    _ink->constant_radius = FUDE_EXAM_PEN_RADIUS;
}

void fude_exam_init(fude_exam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    memset(_exam, 0, sizeof(*_exam));
    _exam->db        = _db;
    _exam->catalog   = _catalog;
    _exam->length    = 1u;   // 20
    _exam->grading   = UINT32_MAX;
    _exam->tapped    = -1;
    _exam->selection = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_glyph_init(&_exam->glyph, _db);
    for(u32 _i = 0; _i < FUDE_EXAM_MAX; _i++) {
        fude_exam_fresh_ink(&_exam->items[_i].ink);
    }
}

void fude_exam_destroy(fude_exam* _exam) {
    for(u32 _i = 0; _i < FUDE_EXAM_MAX; _i++) {
        fude_ink_destroy(&_exam->items[_i].ink);
    }
    if(rde_arr_is_inited(&_exam->selection)) {
        rde_arr_free(&_exam->selection);
    }
    fude_glyph_destroy(&_exam->glyph);
    memset(_exam, 0, sizeof(*_exam));
}

// --- what an exam can be of ----------------------------------------------------------

RDE_INTERNAL b8 fude_exam_in_source(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source, u32 _record) {
    if(_exam->catalog == NULL || !fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_ALL)) {
        return false;   // kana and kanji only
    }
    fude_kanji_info _info;
    switch(_source) {
        case FUDE_EXAM_SOURCE_STUDYING:
        case FUDE_EXAM_SOURCE_KNOWN:
            return fude_kanji_at(_exam->db, _record, &_info) &&
                   fude_marks_get(_info.codepoint) == (_source == FUDE_EXAM_SOURCE_STUDYING ? FUDE_MARK_STUDYING : FUDE_MARK_KNOWN);
        case FUDE_EXAM_SOURCE_N5: return fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_N5);
        case FUDE_EXAM_SOURCE_N4: return fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_N4);
        case FUDE_EXAM_SOURCE_N3: return fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_N3);
        case FUDE_EXAM_SOURCE_N2: return fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_N2);
        case FUDE_EXAM_SOURCE_N1: return fude_catalog_passes(_exam->catalog, _record, FUDE_FILTER_N1);
        case FUDE_EXAM_SOURCE_HIRAGANA:
        case FUDE_EXAM_SOURCE_KATAKANA:
            return fude_catalog_passes(_exam->catalog, _record, _source == FUDE_EXAM_SOURCE_HIRAGANA ? FUDE_FILTER_HIRAGANA : FUDE_FILTER_KATAKANA) &&
                   fude_kanji_at(_exam->db, _record, &_info) && fude_romaji_core(_info.codepoint);
        default: return false;
    }
}

u32 fude_exam_source_size(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source) {
    if(_source == FUDE_EXAM_SOURCE_SELECTION || _source == FUDE_EXAM_SOURCE_REVIEW) {
        return (u32)rde_arr_length(&_exam->selection);
    }
    u32 _n = 0;
    for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count; _r++) {
        _n += fude_exam_in_source(_exam, _source, _r) ? 1u : 0u;
    }
    return _n;
}

u32 fude_exam_planned(const fude_exam* _exam) {
    const u32 _size = fude_exam_source_size(_exam, _exam->source);
    const u32 _want = FUDE_EXAM_LENGTH_VALUES[_exam->length];
    const u32 _n    = _want == 0u || _want > _size ? _size : _want;
    return _n > FUDE_EXAM_MAX ? FUDE_EXAM_MAX : _n;
}

RDE_INTERNAL u32 fude_exam_random(fude_exam* _exam) {
    u32 _x = _exam->rng != 0 ? _exam->rng : 0x9E3779B9u;
    _x ^= _x << 13;
    _x ^= _x >> 17;
    _x ^= _x << 5;
    _exam->rng = _x;
    return _x;
}

RDE_INTERNAL void fude_exam_shuffle(fude_exam* _exam, u32* _records, u32 _count) {
    for(u32 _i = _count; _i > 1u; _i--) {
        const u32 _j = fude_exam_random(_exam) % _i;
        const u32 _t = _records[_i - 1u];
        _records[_i - 1u] = _records[_j];
        _records[_j]      = _t;
    }
}

// The items, fresh: nothing answered, every one ticked.
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

void fude_exam_open(fude_exam* _exam) {
    rde_arr_clear(&_exam->selection);
    if(_exam->source == FUDE_EXAM_SOURCE_SELECTION || _exam->source == FUDE_EXAM_SOURCE_REVIEW) {
        _exam->source = FUDE_EXAM_SOURCE_STUDYING;   // those were a list given: gone with it
    }
    _exam->rng   = (u32)time(NULL) | 1u;
    _exam->stage = FUDE_EXAM_SETUP;
    _exam->open  = _exam->db != NULL;
    fude_exam_set_items(_exam, NULL, 0u);
}

void fude_exam_open_with(fude_exam* _exam, const u32* _records, u32 _count) {
    fude_exam_open(_exam);
    if(_count > 0) {
        rde_memcpy(rde_arr_add_n(&_exam->selection, _count), (any)_records, sizeof(u32) * _count);
    }
    _exam->source = FUDE_EXAM_SOURCE_SELECTION;
    _exam->length = FUDE_EXAM_LENGTHS - 1u;   // all of them
    fude_exam_preview(_exam);
}

void fude_exam_open_review(fude_exam* _exam, const u32* _records, u32 _count) {
    fude_exam_open_with(_exam, _records, _count);
    _exam->source = FUDE_EXAM_SOURCE_REVIEW;
    fude_exam_preview(_exam);
    fude_exam_start(_exam);   // no preview: what is due is what is asked
}

b8 fude_exam_open_kept(fude_exam* _exam, u32 _index) {
    if(_exam->db == NULL || _index >= fude_examlog_count()) {
        return false;
    }
    const fude_examlog_exam* _kept  = &fude_examlog_exams()[_index];
    const fude_examlog_item* _items = &fude_examlog_items()[_kept->first_item];

    // Its characters the data still has, as they were asked.
    u32 _records[FUDE_EXAM_MAX];
    u32 _from[FUDE_EXAM_MAX];   // each one's item in the log
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

    // The writing, back from the log into each answer's ink.
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

    _exam->source = _kept->source < FUDE_EXAM_SOURCE_COUNT ? (FUDE_EXAM_SOURCE_)_kept->source : FUDE_EXAM_SOURCE_STUDYING;
    _exam->asked  = _n;
    _exam->saved  = true;   // already kept
    _exam->kept   = _index;
    _exam->stage  = FUDE_EXAM_RESULTS;
    _exam->open   = true;
    return true;
}

void fude_exam_close(fude_exam* _exam) {
    if(_exam->pen) {
        fude_ink_end(&_exam->items[_exam->order[_exam->current]].ink);
        _exam->pen = false;
    }
    _exam->open = false;
}

void fude_exam_preview(fude_exam* _exam) {
    static u32 _records[16384];
    u32        _n = 0;
    if(_exam->source == FUDE_EXAM_SOURCE_SELECTION || _exam->source == FUDE_EXAM_SOURCE_REVIEW) {
        const u32* _sel = (const u32*)_exam->selection.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_exam->selection) && _n < 16384u; _i++) {
            _records[_n++] = _sel[_i];
        }
    } else {
        for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count && _n < 16384u; _r++) {
            if(fude_exam_in_source(_exam, _exam->source, _r)) {
                _records[_n++] = _r;
            }
        }
    }
    fude_exam_shuffle(_exam, _records, _n);
    fude_exam_set_items(_exam, _records, fude_exam_planned(_exam) < _n ? fude_exam_planned(_exam) : _n);
    _exam->stage = FUDE_EXAM_PREVIEW;
}

void fude_exam_back(fude_exam* _exam) {
    _exam->stage = FUDE_EXAM_SETUP;
    fude_exam_set_items(_exam, NULL, 0u);
}

void fude_exam_tick_all(fude_exam* _exam, b8 _on) {
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _exam->items[_i].included = _on;
    }
}

u32 fude_exam_included(const fude_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _n += _exam->items[_i].included ? 1u : 0u;
    }
    return _n;
}

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

// --- writing ---------------------------------------------------------------------------

RDE_INTERNAL fude_exam_item* fude_exam_current(fude_exam* _exam) {
    return _exam->stage == FUDE_EXAM_WRITING && _exam->current < _exam->asked ? &_exam->items[_exam->order[_exam->current]] : NULL;
}

void fude_exam_undo(fude_exam* _exam) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        fude_ink_undo(&_it->ink);
    }
}

void fude_exam_clear(fude_exam* _exam) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        fude_ink_destroy(&_it->ink);
        fude_exam_fresh_ink(&_it->ink);
    }
}

b8 fude_exam_at_last(const fude_exam* _exam) {
    return _exam->current + 1u >= _exam->asked;
}

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
        _exam->stage = FUDE_EXAM_RESULTS;   // the answers still being read show as such
        fude_scroller_stop(&_exam->scroller);
        _exam->scroller.offset = 0.0f;
    }
}

// --- marking ---------------------------------------------------------------------------

// An answer read: right when among the first candidates of recognition (_r's
// readings of one character, then the matcher's ranking); its points, how well
// it was written.
RDE_INTERNAL void fude_exam_mark(fude_exam* _exam, fude_exam_item* _it, const fude_recognition* _r) {
    _it->graded  = true;
    _it->correct = false;
    _it->score   = _it->quality = 0.0f;
    _it->read_as = UINT32_MAX;
    if(fude_ink_alive_strokes(&_it->ink) == 0) {
        return;   // nothing written: wrong
    }

    fude_match_result _matched[16];
    fude_match_result _candidates[8];
    const u32 _m = fude_match_rank(_exam->db, _exam->catalog, FUDE_FILTER_ALL, &_it->ink, _matched, 16u);
    const u32 _n = fude_recognize_candidates(_exam->db, _r, 0u, 1u, _exam->catalog, FUDE_FILTER_ALL, _matched, _m, _candidates, 8u);
    for(u32 _i = 0; _i < _n && _i < FUDE_EXAM_CANDIDATES; _i++) {
        _it->correct = _it->correct || _candidates[_i].record == _it->record;
    }
    _it->read_as = _n > 0 ? _candidates[0].record : UINT32_MAX;

    fude_kanji_info _info;
    if(fude_kanji_at(_exam->db, _it->record, &_info)) {
        const fude_score _s = fude_score_drawing(_exam->db, &_info, &_it->ink);
        _it->quality = _s.empty ? 0.0f : _s.score;
    }
    _it->score = _it->correct ? _it->quality : 0.0f;
}

b8 fude_exam_graded(const fude_exam* _exam) {
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        if(!_exam->items[_exam->order[_i]].graded) {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL u32 fude_exam_graded_count(const fude_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        _n += _exam->items[_exam->order[_i]].graded ? 1u : 0u;
    }
    return _n;
}

// The answers, read one at a time: ML Kit while it is on and has its model (it
// answers a frame or more later), else the matcher alone.
RDE_INTERNAL void fude_exam_grade(fude_exam* _exam) {
    static f64 _asked_at = 0.0;
    const f64  _now      = rde_engine_get_time_now();
    if(_exam->grading != UINT32_MAX) {
        fude_exam_item* _it = &_exam->items[_exam->grading];
        if(fude_recognize_poll(&_exam->recognition)) {
            fude_exam_mark(_exam, _it, &_exam->recognition);
            _exam->grading = UINT32_MAX;
        } else if(_now - _asked_at > FUDE_EXAM_GRADE_WAIT) {
            fude_recognize_forget(&_exam->recognition);   // no answer: the matcher decides
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
                _exam->grading = _exam->order[_i];
                _asked_at      = _now;
            }
            return;   // started, or ML Kit busy elsewhere: a later frame
        }
        fude_exam_mark(_exam, _it, NULL);
        return;   // one a frame: the matcher takes a moment
    }
}

// All marked: kept in the log, and the marks follow.
RDE_INTERNAL void fude_exam_keep(fude_exam* _exam) {
    static fude_examlog_item _log[FUDE_EXAM_MAX];
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
    fude_examlog_add((u64)time(NULL), (u8)_exam->source, _log, _exam->asked, _drawings, FUDE_EXAM_UNITS);

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
    // Every answer written from memory is a review (review.h): the schedules move.
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        fude_reviews_answer(_log[_i].codepoint, _log[_i].correct, _log[_i].quality / 100.0f);   // quality is points (0..100); reviews take 0..1
    }
    _exam->saved = true;
}

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

void fude_exam_retry_wrong(fude_exam* _exam) {
    u32       _wrong[FUDE_EXAM_MAX];
    const u32 _n = fude_exam_wrong(_exam, _wrong, FUDE_EXAM_MAX);
    if(_n == 0 || !fude_exam_graded(_exam)) {
        return;
    }
    fude_exam_shuffle(_exam, _wrong, _n);
    fude_exam_set_items(_exam, _wrong, _n);
    fude_exam_start(_exam);
}

u32 fude_exam_asked(const fude_exam* _exam, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked && _n < _max; _i++) {
        _out[_n++] = _exam->items[_exam->order[_i]].record;
    }
    return _n;
}

b8 fude_exam_take_tap(fude_exam* _exam, u32* _record) {
    if(_exam->tapped < 0) {
        return false;
    }
    *_record       = _exam->items[_exam->tapped].record;
    _exam->tapped  = -1;
    return true;
}

// --- the pointer -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F fude_exam_local(const fude_exam* _exam, rde_vec_2F _screen) {
    const f32 _k = FUDE_EXAM_UNITS / _exam->square_size;
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _exam->square_tl.x) * _k, 0.0f, FUDE_EXAM_UNITS),
        rde_math_clamp_f32((_screen.y - (_exam->square_tl.y - _exam->square_size)) * _k, 0.0f, FUDE_EXAM_UNITS)
    };
}

void fude_exam_pointer_down(fude_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_it != NULL) {
        // Writing: only the pen, only in the square (a resting hand does nothing).
        const b8 _in = _screen.x >= _exam->square_tl.x && _screen.x <= _exam->square_tl.x + _exam->square_size &&
                       _screen.y <= _exam->square_tl.y && _screen.y >= _exam->square_tl.y - _exam->square_size;
        if(_pen && _in && _exam->square_size > 0.0f) {
            _it->ink.zoom = _exam->square_size / FUDE_EXAM_UNITS;   // the min step stays a screen unit
            fude_ink_begin(&_it->ink, fude_exam_local(_exam, _screen), true, false);
            _exam->pen = true;
        }
        return;
    }
    fude_scroller_down(&_exam->scroller, _screen, _time);
}

void fude_exam_pointer_moved(fude_exam* _exam, rde_vec_2F _screen, f64 _time) {
    fude_exam_item* _it = fude_exam_current(_exam);
    if(_exam->pen && _it != NULL) {
        fude_ink_extend(&_it->ink, fude_exam_local(_exam, _screen));
        return;
    }
    fude_scroller_moved(&_exam->scroller, _screen, _time);
}

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

// The grid cell under _p (the preview's items, the results' asked ones), or -1.
RDE_INTERNAL i32 fude_exam_cell_at(const fude_exam* _exam, rde_vec_2F _p, u32 _cells) {
    if(_exam->cell <= 0.0f || _p.x < _exam->grid_min.x || _p.x > _exam->grid_max.x || _p.y < _exam->grid_min.y || _p.y > _exam->grid_max.y) {
        return -1;
    }
    const u32 _col = (u32)((_p.x - _exam->grid_min.x) / _exam->cell);
    const u32 _row = (u32)((_exam->grid_max.y - _p.y + _exam->scroller.offset) / _exam->cell);
    const u32 _i   = _row * _exam->columns + _col;
    return _col < _exam->columns && _i < _cells ? (i32)_i : -1;
}

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

// --- drawing ---------------------------------------------------------------------------

// A chip: a pill drawn here, not UI (the setup's choices are many and change
// with what there is to choose from). The accent when chosen.
RDE_INTERNAL void fude_exam_chip_draw(rde_font* _font, f32 _font_px, rde_vec_2F _min, rde_vec_2F _max, const c8* _label, b8 _chosen, b8 _usable) {
    const fude_theme* _theme = fude_theme_active();
    const rde_vec_2F  _size  = { _max.x - _min.x, _max.y - _min.y };
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f }, _size, 1.0f, 10,
                                            _chosen ? _theme->accent : _theme->surface_2, NULL);
    const f32 _px = FUDE_EXAM_CHIP_PX;
    const f32 _w  = fude_draw_text_width(_font, _font_px, _label, _px);
    fude_draw_text(_font, _font_px, _label, (_min.x + _max.x - _w) * 0.5f, (_min.y + _max.y) * 0.5f - _px * 0.42f, _px,
                   !_usable ? _theme->button_text_disabled : _chosen ? _theme->on_accent : _theme->text);
}

// A label that is a Japanese character (試, 音, 訓), written from its strokes in
// the accent on its tint, _box square, its top-left at _tl.
RDE_INTERNAL void fude_exam_render_setup(fude_exam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top) {
    const fude_theme* _theme = fude_theme_active();
    fude_header_draw(&_exam->glyph, 0x8A66u, _font, _font_px, _left, _right, _top, fude_text(FUDE_TEXT_EXAM), fude_text(FUDE_TEXT_EXAM_INTRO), NULL);
    f32 _y = _top - 56.0f;

    // What it is of, and how many.
    _exam->chip_count = 0;
    c8 _label[48];
    for(u32 _group = 0; _group < 2u; _group++) {
        _y -= 30.0f;
        fude_draw_text(_font, _font_px, fude_text(_group == 0 ? FUDE_TEXT_EXAM_WHAT : FUDE_TEXT_EXAM_HOW_MANY), _left, _y, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
        _y -= 12.0f;
        f32       _x   = _left;
        const u32 _n   = _group == 0 ? (u32)FUDE_EXAM_SOURCE_COUNT : FUDE_EXAM_LENGTHS;
        for(u32 _i = 0; _i < _n; _i++) {
            b8 _usable = true;
            b8 _chosen;
            if(_group == 0) {
                if((_i == FUDE_EXAM_SOURCE_SELECTION && rde_arr_length(&_exam->selection) == 0) || _i == FUDE_EXAM_SOURCE_REVIEW) {
                    continue;   // only when opened from Select mode; Reviews has its own way in
                }
                const u32 _size = fude_exam_source_size(_exam, (FUDE_EXAM_SOURCE_)_i);
                snprintf(_label, sizeof(_label), "%s  %u", fude_exam_source_name((FUDE_EXAM_SOURCE_)_i), _size);
                _usable = _size > 0;
                _chosen = _exam->source == (FUDE_EXAM_SOURCE_)_i;
            } else {
                snprintf(_label, sizeof(_label), "%s", FUDE_EXAM_LENGTH_NAMES[_i] != NULL ? FUDE_EXAM_LENGTH_NAMES[_i] : fude_text(FUDE_TEXT_ALL));
                _chosen = _exam->length == _i;
            }
            const f32 _w = fmaxf(64.0f, fude_draw_text_width(_font, _font_px, _label, FUDE_EXAM_CHIP_PX) + 36.0f);
            if(_x + _w > _right && _x > _left) {   // on to another row
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

    // What that makes, in a card.
    _y -= 24.0f;
    const u32 _size    = fude_exam_source_size(_exam, _exam->source);
    const u32 _planned = fude_exam_planned(_exam);
    fude_draw_card((rde_vec_2F){ _left, _y - 76.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
    if(_size == 0) {
        snprintf(_label, sizeof(_label), "%s", fude_text(_exam->source == FUDE_EXAM_SOURCE_STUDYING ? FUDE_TEXT_EXAM_NOTHING_STUDYING : FUDE_TEXT_EXAM_NOTHING));
        fude_draw_text(_font, _font_px, _label, _left + 18.0f, _y - 32.0f, 15.0f, _theme->text);
        fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_EXAM_MARK_HINT), _left + 18.0f, _y - 56.0f, 12.0f, _theme->text_soft);
    } else {
        c8 _line[160];
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_PLAN, FUDE_TN(_planned), FUDE_TN(_size), FUDE_TS(fude_exam_source_name(_exam->source)));
        fude_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 32.0f, 15.0f, _theme->text);
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_EXAM_RULE));
        if(_size > FUDE_EXAM_MAX && FUDE_EXAM_LENGTH_VALUES[_exam->length] == 0u) {
            FUDE_TEXTF(_line, FUDE_TEXT_EXAM_MAX, FUDE_TN(FUDE_EXAM_MAX));
        }
        fude_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 56.0f, 12.0f, _theme->text_soft);
    }
}

// A grid below _top: cells for _count things, scrolled; the layout kept for taps.
RDE_INTERNAL void fude_exam_grid(fude_exam* _exam, f32 _left, f32 _right, f32 _top, f32 _bottom, f32 _cell_min, u32 _count) {
    const f32 _width = _right - _left;
    _exam->columns   = (u32)fmaxf(1.0f, floorf(_width / _cell_min));
    _exam->cell      = _width / (f32)_exam->columns;
    _exam->grid_min  = (rde_vec_2F){ _left, _bottom };
    _exam->grid_max  = (rde_vec_2F){ _right, _top };
    _exam->content_h = (f32)((_count + _exam->columns - 1u) / _exam->columns) * _exam->cell;
}

RDE_INTERNAL void fude_exam_render_preview(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    c8 _line[160];
    FUDE_TEXTF(_line, FUDE_TEXT_OF_N, FUDE_TN(fude_exam_included(_exam)), FUDE_TN(_exam->count));
    c8 _title[96];
    FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
    fude_header_draw(&_exam->glyph, 0x8A66u, _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_EXAM_TAP_TO_LEAVE), _line);
    RDE_UNUSED(_theme);

    const f32 _grid_top = _top - 60.0f;
    fude_exam_grid(_exam, _left, _right, _grid_top, _bottom, FUDE_EXAM_CELL_MIN, _exam->count);
    if(_grid_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_grid_top + _bottom) * 0.5f) },
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

// A word with the kanji in it, the kanji blanked (□): the learner's own first
// (vocab.h), then the character's — of two characters or more (the kanji
// alone leaves nothing to show). False when there is none.
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
            if(_cp == _info->codepoint) { snprintf(_one, sizeof(_one), "\xE2\x96\xA1"); }   // □
            else                        { fude_utf8_put(_cp, _one); }
            _len += (usize)snprintf(_blanked + _len, _size - _len, "%s", _one);
            _chars++;
        }
        if(_chars >= 2u) {
            return true;
        }
    }
    return false;
}

// The prompt, in a card from _y down: for a kanji its meaning, its readings (音,
// 訓) and a word with it blanked out; for a kana its romaji. Returns the y below it.
RDE_INTERNAL f32 fude_exam_prompt(fude_exam* _exam, const fude_kanji_info* _info, u32 _record, b8 _kana, rde_font* _font, f32 _font_px,
                                  f32 _left, f32 _right, f32 _y) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _pad   = 18.0f;
    const f32         _x     = _left + _pad;
    c8                _line[256];

    // How tall it is: the caption and the big line, then a line a reading, and the word.
    const c8* _on  = _kana ? "" : fude_kanji_on(_exam->db, _info);
    const c8* _kun = _kana ? "" : fude_kanji_kun(_exam->db, _info);
    c8              _blanked[96];
    fude_kanji_word _word;
    const b8        _has_word = !_kana && fude_exam_blanked_word(_exam, _info, _record, _blanked, sizeof(_blanked), &_word);
    const u32       _readings = (_on[0] != 0 ? 1u : 0u) + (_kun[0] != 0 ? 1u : 0u);
    const f32       _h = _pad + 18.0f + (_kana ? 64.0f : 40.0f) + (_readings > 0 ? 44.0f : 0.0f) + (_has_word ? 52.0f : 0.0f) + _pad - 6.0f;
    fude_draw_card((rde_vec_2F){ _left, _y - _h }, (rde_vec_2F){ _right, _y }, 16.0f, _theme->surface, _theme->outline);

    f32 _cy = _y - _pad - 10.0f;
    if(_kana) {
        const b8 _katakana = _info->codepoint >= 0x30A0u && _info->codepoint <= 0x30FFu;
        fude_draw_text(_font, _font_px, fude_text(_katakana ? FUDE_TEXT_EXAM_WRITE_KATAKANA : FUDE_TEXT_EXAM_WRITE_HIRAGANA), _x, _cy, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
        fude_draw_text(_font, _font_px, fude_romaji(_info->codepoint), _x, _cy - 54.0f, 40.0f, _theme->text);
        return _y - _h;
    }
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_EXAM_WRITE_KANJI), _x, _cy, FUDE_EXAM_CAPTION_PX, _theme->text_soft);
    const c8* _meaning = fude_kanji_meanings(_exam->db, _info);
    fude_draw_text_fit(_font, _font_px, _meaning[0] != 0 ? _meaning : fude_text(FUDE_TEXT_NO_MEANING), FUDE_EXAM_MEANING_PX, _right - _pad - _x, _line, sizeof(_line));
    fude_draw_text(_font, _font_px, _line, _x, _cy - 34.0f, FUDE_EXAM_MEANING_PX, _theme->text);
    _cy -= 40.0f;

    // The readings side by side, each behind its label.
    if(_readings > 0) {
        const f32 _mid = _cy - 24.0f;
        f32       _rx  = _x;
        for(u32 _k = 0; _k < 2u; _k++) {
            const c8* _text = _k == 0u ? _on : _kun;
            if(_text[0] == 0 || _rx > _right - 80.0f) {
                continue;
            }
            fude_header_badge(&_exam->glyph, _k == 0u ? 0x97F3u : 0x8A13u, (rde_vec_2F){ _rx, _mid + 15.0f }, 30.0f);   // 音, 訓
            const f32 _end = fude_glyph_reading(&_exam->glyph, _text, (rde_vec_2F){ _rx + 42.0f, _mid + 12.0f }, 24.0f,
                                                _k == 0u && _kun[0] != 0 ? (_left + _right) * 0.5f + 40.0f : _right - _pad, _theme->ink, _theme->text_soft);
            _rx = fmaxf(_end + 28.0f, _k == 0u ? _x : _rx);
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
        fude_draw_text_fit(_font, _font_px, _word.meaning, 13.0f, _right - _pad - _mx, _line, sizeof(_line));
        fude_draw_text(_font, _font_px, _line, _mx, _mid - 13.0f * 0.42f, 13.0f, _theme->text);
    }
    return _y - _h;
}

RDE_INTERNAL void fude_exam_render_writing(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    fude_exam_item*   _it    = fude_exam_current(_exam);
    fude_kanji_info   _info;
    if(_it == NULL || !fude_kanji_at(_exam->db, _it->record, &_info)) {
        return;
    }

    // The title, where it is, and the progress: a segment per character (a bar
    // when there are many).
    c8 _title[96];
    c8 _line[64];
    FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
    FUDE_TEXTF(_line, FUDE_TEXT_OF_N, FUDE_TN(_exam->current + 1u), FUDE_TN(_exam->asked));
    fude_header_draw(&_exam->glyph, 0x8A66u, _font, _font_px, _left, _right, _top, _title, fude_text(FUDE_TEXT_EXAM_ONCE_EACH), _line);
    const f32 _py = _top - 54.0f;
    if(_exam->asked > 0 && _exam->asked <= 30u) {
        const f32 _gap = 3.0f;
        const f32 _w   = (_right - _left - _gap * (f32)(_exam->asked - 1u)) / (f32)_exam->asked;
        for(u32 _i = 0; _i < _exam->asked; _i++) {
            const rde_color _c = _i < _exam->current ? _theme->accent : _i == _exam->current ? _theme->tint : _theme->surface_2;
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _left + (f32)_i * (_w + _gap) + _w * 0.5f, _py }, (rde_vec_2F){ _w, 6.0f }, 1.0f, 4, _c, NULL);
        }
    } else if(_exam->asked > 0) {
        const f32 _done = (_right - _left) * (f32)_exam->current / (f32)_exam->asked;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_left + _right) * 0.5f, _py }, (rde_vec_2F){ _right - _left, 6.0f }, 1.0f, 4, _theme->surface_2, NULL);
        if(_done > 6.0f) {
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _left + _done * 0.5f, _py }, (rde_vec_2F){ _done, 6.0f }, 1.0f, 4, _theme->accent, NULL);
        }
    }

    // What to write.
    const b8 _kana = fude_romaji(_info.codepoint) != NULL && _info.text == UINT32_MAX;
    const f32 _y   = fude_exam_prompt(_exam, &_info, _it->record, _kana, _font, _font_px, _left, _right, _py - 16.0f);

    // The square.
    const f32 _room   = _y - 18.0f - (_bottom + 26.0f);
    const f32 _square = fmaxf(120.0f, fminf(fminf(_right - _left, _room), FUDE_EXAM_SQUARE_MAX));
    const rde_vec_2F _tl = { (_left + _right) * 0.5f - _square * 0.5f, _y - 18.0f };
    _exam->square_tl   = _tl;
    _exam->square_size = _square;
    fude_glyph_box(_tl, _square);
    const rde_vec_2I _size = rde_window_get_size(_window);
    fude_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / FUDE_EXAM_UNITS,
                    (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
    fude_draw_text(_font, _font_px, fude_text(fude_exam_at_last(_exam) ? FUDE_TEXT_EXAM_THEN_FINISH : FUDE_TEXT_EXAM_THEN_NEXT),
                   _tl.x, _tl.y - _square - 20.0f, 12.0f, _theme->text_soft);
}

RDE_INTERNAL void fude_exam_render_results(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    c8 _line[160];
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
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_RESULT, FUDE_TN(_right_n), FUDE_TN(_exam->asked), FUDE_TN(lroundf(_points / (f32)_exam->asked)));
        fude_draw_text(_font, _font_px, _line, _left, _top - 30.0f, FUDE_EXAM_TITLE_PX, _theme->text);
        fude_draw_chip(_font, _font_px, fude_text(_passed ? FUDE_TEXT_EXAM_PASSED : FUDE_TEXT_EXAM_NOT_PASSED), _left + fude_draw_text_width(_font, _font_px, _line, FUDE_EXAM_TITLE_PX) + 16.0f,
                       _top - 30.0f + FUDE_EXAM_TITLE_PX * 0.42f, 12.0f, _passed ? _theme->score_good : _theme->score_poor, _theme->on_accent);
    }

    // A kept exam: which, and when.
    f32 _grid_top = _top - 56.0f;
    if(_exam->kept < fude_examlog_count()) {
        c8 _title[64], _when[96];
        FUDE_TEXTF(_title, FUDE_TEXT_EXAM_TITLE_SOURCE, FUDE_TS(fude_exam_source_name(_exam->source)));
        fude_text_date_time(_when, sizeof(_when), fude_examlog_exams()[_exam->kept].time);
        FUDE_TEXTF(_line, FUDE_TEXT_EXAM_KEPT, FUDE_TS(_title), FUDE_TS(_when));
        fude_draw_text(_font, _font_px, _line, _left, _top - 54.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, FUDE_EXAM_CAPTION_PX, _right - _left, 0.6f),
                       _theme->text_soft);
        _grid_top = _top - 72.0f;
    }
    fude_exam_grid(_exam, _left, _right, _grid_top, _bottom, FUDE_EXAM_RESULT_MIN, _exam->asked);
    if(_grid_top <= _bottom) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_grid_top + _bottom) * 0.5f) },
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
        // The writing, in its square; what was asked, small, in the corner.
        const f32 _sq = _cell - 10.0f;
        const rde_vec_2F _tl = { _x + 5.0f, _y - 5.0f };
        fude_glyph_box(_tl, _sq);
        fude_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _sq }, _sq / FUDE_EXAM_UNITS,
                        (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
        fude_glyph_character(&_exam->glyph, _info.codepoint, (rde_vec_2F){ _tl.x + 4.0f, _tl.y - 4.0f }, _sq * 0.26f, _theme->text_soft);

        // Right or wrong, and the points.
        const f32        _r = fmaxf(10.0f, _sq * 0.1f);
        const rde_vec_2F _c = { _tl.x + _sq - _r - 5.0f, _tl.y - _r - 5.0f };
        if(!_it->graded) {
            rde_rendering_2d_draw_circle_border(_c, _r, 1.5f, 24, _theme->text_soft, NULL);
            continue;
        }
        fude_draw_verdict(_c, _r, _it->correct);
        if(_it->correct) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_it->score);
            fude_draw_text(_font, _font_px, _line, _tl.x + _sq - fude_draw_text_width(_font, _font_px, _line, 18.0f) - 6.0f, _tl.y - _sq + 8.0f, 18.0f,
                           fude_theme_grade(_it->score));
        } else if(_it->read_as != UINT32_MAX && _it->read_as != _it->record) {
            // What it read as instead.
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

void fude_exam_render(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_exam->open) {
        return;
    }
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_EXAM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_EXAM_MARGIN;
    switch(_exam->stage) {
        case FUDE_EXAM_SETUP:   fude_exam_render_setup(_exam, _font, _font_px, _left, _right, _top); _exam->grid_min = _exam->grid_max = (rde_vec_2F){ 0 }; _exam->content_h = 0.0f; break;
        case FUDE_EXAM_PREVIEW: fude_exam_render_preview(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case FUDE_EXAM_WRITING: fude_exam_render_writing(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case FUDE_EXAM_RESULTS: fude_exam_render_results(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
    }
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"

FUDE_SCREEN_ADAPTERS_PEN(fude_exam, fude_exam)
FUDE_SCREEN_RENDER(fude_exam, fude_exam)

// Its answers read as it goes; a character tapped in the results opens the
// viewer on it, walking the exam's characters.
RDE_INTERNAL void fude_exam_screen_update(fude_app* _app, void* _self, f32 _dt) {
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

FUDE_ROW_CALL(fude_exam_row_close,   fude_exam, fude_exam_close)
FUDE_ROW_CALL(fude_exam_row_preview, fude_exam, fude_exam_preview)
FUDE_ROW_CALL(fude_exam_row_back,    fude_exam, fude_exam_back)
FUDE_ROW_CALL(fude_exam_row_start,   fude_exam, fude_exam_start)
FUDE_ROW_CALL(fude_exam_row_undo,    fude_exam, fude_exam_undo)
FUDE_ROW_CALL(fude_exam_row_clear,   fude_exam, fude_exam_clear)
FUDE_ROW_CALL(fude_exam_row_next,    fude_exam, fude_exam_next)
FUDE_ROW_CALL(fude_exam_row_retry,   fude_exam, fude_exam_retry_wrong)

// All, None: every character in the preview ticked, or none (_arg).
RDE_INTERNAL void fude_exam_row_tick(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app);
    fude_exam_tick_all((fude_exam*)_self, _arg != 0u);
}

// The wrong ones as a practice set, over the results.
RDE_INTERNAL void fude_exam_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _wrong[FUDE_EXAM_MAX];
    const u32 _n = fude_exam_wrong((const fude_exam*)_self, _wrong, FUDE_EXAM_MAX);
    fude_study_practice_set(_app, _wrong, _n);
}

// One row per stage (FUDE_EXAM_ order): its setup's, preview's, writing's, results'.
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

RDE_INTERNAL u32 fude_exam_screen_row(const void* _self) {
    return (u32)((const fude_exam*)_self)->stage;
}

// Next n (the characters planned), Start n (those ticked), Next or Finish; Retry
// and Practice the wrong ones when there are.
RDE_INTERNAL void fude_exam_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
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
