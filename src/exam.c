#include "exam.h"
#include "review.h"
#include "chart.h"
#include "draw.h"
#include "examlog.h"
#include "marks.h"
#include "match.h"
#include "score.h"
#include "select.h"
#include "theme.h"
#include "userwords.h"
#include "text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See exam.h.
// ===========================================================================

#define KANA_EXAM_MARGIN      24.0f
#define KANA_EXAM_CHIP_H      40.0f
#define KANA_EXAM_CHIP_GAP    8.0f
#define KANA_EXAM_CELL_MIN    88.0f     // the preview's grid, as Browse's
#define KANA_EXAM_RESULT_MIN  132.0f    // the results' grid: room to see the writing
#define KANA_EXAM_SQUARE_MAX  520.0f
#define KANA_EXAM_TITLE_PX    17.0f     // sizes: a screen's title
#define KANA_EXAM_CAPTION_PX  10.0f     // a caption
#define KANA_EXAM_CHIP_PX     13.0f     // a chip's label
#define KANA_EXAM_MEANING_PX  22.0f     // the prompt's meaning
#define KANA_EXAM_WORD_PX     19.0f     // the prompt's word
#define KANA_EXAM_GRADE_WAIT  4.0       // seconds ML Kit may take on one answer before the matcher decides alone
#define KANA_EXAM_PEN_RADIUS  (KANA_EXAM_UNITS * KANA_GLYPH_WIDTH / KANA_KANJI_BOX * 0.5f)

static const u32       KANA_EXAM_LENGTH_VALUES[KANA_EXAM_LENGTHS] = { 10u, 20u, 50u, 0u };   // 0: all
static const c8* const KANA_EXAM_LENGTH_NAMES[KANA_EXAM_LENGTHS]  = { "10", "20", "50", NULL };   // NULL: All (text.h)
// The sources' names (text.h); the JLPT levels are the same in every language.
static const KANA_TEXT_ KANA_EXAM_SOURCE_TEXTS[KANA_EXAM_SOURCE_COUNT] = {
    KANA_TEXT_STUDYING, KANA_TEXT_KNOWN, KANA_TEXT_COUNT, KANA_TEXT_COUNT, KANA_TEXT_COUNT, KANA_TEXT_COUNT, KANA_TEXT_COUNT,
    KANA_TEXT_HIRAGANA, KANA_TEXT_KATAKANA, KANA_TEXT_EXAM_SELECTION, KANA_TEXT_REVIEWS
};
static const c8* const KANA_EXAM_SOURCE_LEVELS[KANA_EXAM_SOURCE_COUNT] = { NULL, NULL, "N5", "N4", "N3", "N2", "N1", NULL, NULL, NULL, NULL };

const c8* kana_exam_source_name(KANA_EXAM_SOURCE_ _source) {
    if(_source >= KANA_EXAM_SOURCE_COUNT) {
        return "";
    }
    return KANA_EXAM_SOURCE_LEVELS[_source] != NULL ? KANA_EXAM_SOURCE_LEVELS[_source] : kana_text(KANA_EXAM_SOURCE_TEXTS[_source]);
}

RDE_INTERNAL void kana_exam_fresh_ink(kana_ink* _ink) {
    kana_ink_init(_ink);
    _ink->constant_radius = KANA_EXAM_PEN_RADIUS;
}

void kana_exam_init(kana_exam* _exam, const kana_kanji_db* _db, const kana_catalog* _catalog) {
    memset(_exam, 0, sizeof(*_exam));
    _exam->db        = _db;
    _exam->catalog   = _catalog;
    _exam->length    = 1u;   // 20
    _exam->grading   = UINT32_MAX;
    _exam->tapped    = -1;
    _exam->selection = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_glyph_init(&_exam->glyph, _db);
    for(u32 _i = 0; _i < KANA_EXAM_MAX; _i++) {
        kana_exam_fresh_ink(&_exam->items[_i].ink);
    }
}

void kana_exam_destroy(kana_exam* _exam) {
    for(u32 _i = 0; _i < KANA_EXAM_MAX; _i++) {
        kana_ink_destroy(&_exam->items[_i].ink);
    }
    if(rde_arr_is_inited(&_exam->selection)) {
        rde_arr_free(&_exam->selection);
    }
    kana_glyph_destroy(&_exam->glyph);
    memset(_exam, 0, sizeof(*_exam));
}

// --- what an exam can be of ----------------------------------------------------------

RDE_INTERNAL b8 kana_exam_in_source(const kana_exam* _exam, KANA_EXAM_SOURCE_ _source, u32 _record) {
    if(_exam->catalog == NULL || !kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_ALL)) {
        return false;   // kana and kanji only
    }
    kana_kanji_info _info;
    switch(_source) {
        case KANA_EXAM_SOURCE_STUDYING:
        case KANA_EXAM_SOURCE_KNOWN:
            return kana_kanji_at(_exam->db, _record, &_info) &&
                   kana_marks_get(_info.codepoint) == (_source == KANA_EXAM_SOURCE_STUDYING ? KANA_MARK_STUDYING : KANA_MARK_KNOWN);
        case KANA_EXAM_SOURCE_N5: return kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_N5);
        case KANA_EXAM_SOURCE_N4: return kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_N4);
        case KANA_EXAM_SOURCE_N3: return kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_N3);
        case KANA_EXAM_SOURCE_N2: return kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_N2);
        case KANA_EXAM_SOURCE_N1: return kana_catalog_passes(_exam->catalog, _record, KANA_FILTER_N1);
        case KANA_EXAM_SOURCE_HIRAGANA:
        case KANA_EXAM_SOURCE_KATAKANA:
            return kana_catalog_passes(_exam->catalog, _record, _source == KANA_EXAM_SOURCE_HIRAGANA ? KANA_FILTER_HIRAGANA : KANA_FILTER_KATAKANA) &&
                   kana_kanji_at(_exam->db, _record, &_info) && kana_chart_core_kana(_info.codepoint);
        default: return false;
    }
}

u32 kana_exam_source_size(const kana_exam* _exam, KANA_EXAM_SOURCE_ _source) {
    if(_source == KANA_EXAM_SOURCE_SELECTION || _source == KANA_EXAM_SOURCE_REVIEW) {
        return (u32)rde_arr_length(&_exam->selection);
    }
    u32 _n = 0;
    for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count; _r++) {
        _n += kana_exam_in_source(_exam, _source, _r) ? 1u : 0u;
    }
    return _n;
}

u32 kana_exam_planned(const kana_exam* _exam) {
    const u32 _size = kana_exam_source_size(_exam, _exam->source);
    const u32 _want = KANA_EXAM_LENGTH_VALUES[_exam->length];
    const u32 _n    = _want == 0u || _want > _size ? _size : _want;
    return _n > KANA_EXAM_MAX ? KANA_EXAM_MAX : _n;
}

RDE_INTERNAL u32 kana_exam_random(kana_exam* _exam) {
    u32 _x = _exam->rng != 0 ? _exam->rng : 0x9E3779B9u;
    _x ^= _x << 13;
    _x ^= _x >> 17;
    _x ^= _x << 5;
    _exam->rng = _x;
    return _x;
}

RDE_INTERNAL void kana_exam_shuffle(kana_exam* _exam, u32* _records, u32 _count) {
    for(u32 _i = _count; _i > 1u; _i--) {
        const u32 _j = kana_exam_random(_exam) % _i;
        const u32 _t = _records[_i - 1u];
        _records[_i - 1u] = _records[_j];
        _records[_j]      = _t;
    }
}

// The items, fresh: nothing answered, every one ticked.
RDE_INTERNAL void kana_exam_set_items(kana_exam* _exam, const u32* _records, u32 _count) {
    _count = _count > KANA_EXAM_MAX ? KANA_EXAM_MAX : _count;
    for(u32 _i = 0; _i < KANA_EXAM_MAX; _i++) {
        kana_exam_item* _it = &_exam->items[_i];
        kana_ink_destroy(&_it->ink);
        kana_exam_fresh_ink(&_it->ink);
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
    kana_recognize_forget(&_exam->recognition);
    kana_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

void kana_exam_open(kana_exam* _exam) {
    rde_arr_clear(&_exam->selection);
    if(_exam->source == KANA_EXAM_SOURCE_SELECTION || _exam->source == KANA_EXAM_SOURCE_REVIEW) {
        _exam->source = KANA_EXAM_SOURCE_STUDYING;   // those were a list given: gone with it
    }
    _exam->rng   = (u32)time(NULL) | 1u;
    _exam->stage = KANA_EXAM_SETUP;
    _exam->open  = _exam->db != NULL;
    kana_exam_set_items(_exam, NULL, 0u);
}

void kana_exam_open_with(kana_exam* _exam, const u32* _records, u32 _count) {
    kana_exam_open(_exam);
    if(_count > 0) {
        rde_memcpy(rde_arr_add_n(&_exam->selection, _count), (any)_records, sizeof(u32) * _count);
    }
    _exam->source = KANA_EXAM_SOURCE_SELECTION;
    _exam->length = KANA_EXAM_LENGTHS - 1u;   // all of them
    kana_exam_preview(_exam);
}

void kana_exam_open_review(kana_exam* _exam, const u32* _records, u32 _count) {
    kana_exam_open_with(_exam, _records, _count);
    _exam->source = KANA_EXAM_SOURCE_REVIEW;
    kana_exam_preview(_exam);
    kana_exam_start(_exam);   // no preview: what is due is what is asked
}

b8 kana_exam_open_kept(kana_exam* _exam, u32 _index) {
    if(_exam->db == NULL || _index >= kana_examlog_count()) {
        return false;
    }
    const kana_examlog_exam* _kept  = &kana_examlog_exams()[_index];
    const kana_examlog_item* _items = &kana_examlog_items()[_kept->first_item];

    // Its characters the data still has, as they were asked.
    u32 _records[KANA_EXAM_MAX];
    u32 _from[KANA_EXAM_MAX];   // each one's item in the log
    u32 _n = 0;
    for(u32 _i = 0; _i < _kept->item_count && _n < KANA_EXAM_MAX; _i++) {
        if(kana_kanji_find_index(_exam->db, _items[_i].codepoint, &_records[_n])) {
            _from[_n++] = _i;
        }
    }
    rde_arr_clear(&_exam->selection);
    kana_exam_set_items(_exam, _records, _n);
    for(u32 _k = 0; _k < _n; _k++) {
        kana_exam_item*          _it  = &_exam->items[_k];
        const kana_examlog_item* _log = &_items[_from[_k]];
        u32                      _read;
        _it->answered = _it->graded = true;
        _it->correct  = _log->correct;
        _it->score    = _log->score;
        _it->quality  = _log->quality;
        _it->read_as  = _log->read_as != 0 && kana_kanji_find_index(_exam->db, _log->read_as, &_read) ? _read : UINT32_MAX;
        _exam->order[_k] = _k;
    }

    // The writing, back from the log into each answer's ink.
    rde_memory_allocator* _heap     = rde_memory_allocator_get_default_std();
    rde_arr               _writings = rde_arr_new(sizeof(kana_examlog_writing), _heap);
    rde_arr               _strokes  = rde_arr_new(sizeof(kana_history_stroke), _heap);
    rde_arr               _points   = rde_arr_new(sizeof(kana_history_point), _heap);
    rde_arr               _ink      = rde_arr_new(sizeof(kana_ink_point), _heap);
    kana_examlog_read_writing(_index, 0u, &_writings, &_strokes, &_points);
    const kana_examlog_writing* _w = (const kana_examlog_writing*)_writings.memory;
    const kana_history_stroke*  _s = (const kana_history_stroke*)_strokes.memory;
    const kana_history_point*   _p = (const kana_history_point*)_points.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_writings); _i++) {
        for(u32 _k = 0; _k < _n; _k++) {
            if(_from[_k] != _w[_i].item) {
                continue;
            }
            for(u32 _t = 0; _t < _w[_i].stroke_count; _t++) {
                const kana_history_stroke* _stroke = &_s[_w[_i].first_stroke + _t];
                rde_arr_clear(&_ink);
                kana_ink_point* _q = (kana_ink_point*)rde_arr_add_n(&_ink, _stroke->point_count);
                for(u32 _j = 0; _j < _stroke->point_count; _j++) {
                    const kana_history_point* _hp = &_p[_stroke->first_point + _j];
                    _q[_j] = (kana_ink_point){
                        .position = { (f32)_hp->x / 65535.0f * KANA_EXAM_UNITS, (f32)_hp->y / 65535.0f * KANA_EXAM_UNITS },
                        .pressure = 1.0f, .radius = KANA_EXAM_PEN_RADIUS, .time = _hp->time
                    };
                }
                kana_ink_add_loaded_stroke(&_exam->items[_k].ink, _q, _stroke->point_count, KANA_THEME_INK, true);
            }
            break;
        }
    }
    rde_arr_free(&_writings);
    rde_arr_free(&_strokes);
    rde_arr_free(&_points);
    rde_arr_free(&_ink);

    _exam->source = _kept->source < KANA_EXAM_SOURCE_COUNT ? (KANA_EXAM_SOURCE_)_kept->source : KANA_EXAM_SOURCE_STUDYING;
    _exam->asked  = _n;
    _exam->saved  = true;   // already kept
    _exam->kept   = _index;
    _exam->stage  = KANA_EXAM_RESULTS;
    _exam->open   = true;
    return true;
}

void kana_exam_close(kana_exam* _exam) {
    if(_exam->pen) {
        kana_ink_end(&_exam->items[_exam->order[_exam->current]].ink);
        _exam->pen = false;
    }
    _exam->open = false;
}

void kana_exam_preview(kana_exam* _exam) {
    static u32 _records[16384];
    u32        _n = 0;
    if(_exam->source == KANA_EXAM_SOURCE_SELECTION || _exam->source == KANA_EXAM_SOURCE_REVIEW) {
        const u32* _sel = (const u32*)_exam->selection.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_exam->selection) && _n < 16384u; _i++) {
            _records[_n++] = _sel[_i];
        }
    } else {
        for(u32 _r = 0; _exam->db != NULL && _r < _exam->db->count && _n < 16384u; _r++) {
            if(kana_exam_in_source(_exam, _exam->source, _r)) {
                _records[_n++] = _r;
            }
        }
    }
    kana_exam_shuffle(_exam, _records, _n);
    kana_exam_set_items(_exam, _records, kana_exam_planned(_exam) < _n ? kana_exam_planned(_exam) : _n);
    _exam->stage = KANA_EXAM_PREVIEW;
}

void kana_exam_back(kana_exam* _exam) {
    _exam->stage = KANA_EXAM_SETUP;
    kana_exam_set_items(_exam, NULL, 0u);
}

void kana_exam_tick_all(kana_exam* _exam, b8 _on) {
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _exam->items[_i].included = _on;
    }
}

u32 kana_exam_included(const kana_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->count; _i++) {
        _n += _exam->items[_i].included ? 1u : 0u;
    }
    return _n;
}

void kana_exam_start(kana_exam* _exam) {
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
    _exam->stage   = KANA_EXAM_WRITING;
    kana_scroller_stop(&_exam->scroller);
    _exam->scroller.offset = 0.0f;
}

// --- writing ---------------------------------------------------------------------------

RDE_INTERNAL kana_exam_item* kana_exam_current(kana_exam* _exam) {
    return _exam->stage == KANA_EXAM_WRITING && _exam->current < _exam->asked ? &_exam->items[_exam->order[_exam->current]] : NULL;
}

void kana_exam_undo(kana_exam* _exam) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        kana_ink_undo(&_it->ink);
    }
}

void kana_exam_clear(kana_exam* _exam) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_it != NULL && !_exam->pen) {
        kana_ink_destroy(&_it->ink);
        kana_exam_fresh_ink(&_it->ink);
    }
}

b8 kana_exam_at_last(const kana_exam* _exam) {
    return _exam->current + 1u >= _exam->asked;
}

void kana_exam_next(kana_exam* _exam) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_it == NULL) {
        return;
    }
    if(_exam->pen) {
        kana_ink_end(&_it->ink);
        _exam->pen = false;
    }
    _it->answered = true;
    if(_exam->current + 1u < _exam->asked) {
        _exam->current++;
    } else {
        _exam->stage = KANA_EXAM_RESULTS;   // the answers still being read show as such
        kana_scroller_stop(&_exam->scroller);
        _exam->scroller.offset = 0.0f;
    }
}

// --- marking ---------------------------------------------------------------------------

// An answer read: right when among the first candidates of recognition (_r's
// readings of one character, then the matcher's ranking); its points, how well
// it was written.
RDE_INTERNAL void kana_exam_mark(kana_exam* _exam, kana_exam_item* _it, const kana_recognition* _r) {
    _it->graded  = true;
    _it->correct = false;
    _it->score   = _it->quality = 0.0f;
    _it->read_as = UINT32_MAX;
    if(kana_ink_alive_strokes(&_it->ink) == 0) {
        return;   // nothing written: wrong
    }

    kana_match_result _matched[16];
    kana_match_result _candidates[8];
    const u32 _m = kana_match_rank(_exam->db, _exam->catalog, KANA_FILTER_ALL, &_it->ink, _matched, 16u);
    const u32 _n = kana_recognize_candidates(_exam->db, _r, 0u, 1u, _exam->catalog, KANA_FILTER_ALL, _matched, _m, _candidates, 8u);
    for(u32 _i = 0; _i < _n && _i < KANA_EXAM_CANDIDATES; _i++) {
        _it->correct = _it->correct || _candidates[_i].record == _it->record;
    }
    _it->read_as = _n > 0 ? _candidates[0].record : UINT32_MAX;

    kana_kanji_info _info;
    if(kana_kanji_at(_exam->db, _it->record, &_info)) {
        const kana_score _s = kana_score_drawing(_exam->db, &_info, &_it->ink);
        _it->quality = _s.empty ? 0.0f : _s.score;
    }
    _it->score = _it->correct ? _it->quality : 0.0f;
}

b8 kana_exam_graded(const kana_exam* _exam) {
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        if(!_exam->items[_exam->order[_i]].graded) {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL u32 kana_exam_graded_count(const kana_exam* _exam) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        _n += _exam->items[_exam->order[_i]].graded ? 1u : 0u;
    }
    return _n;
}

// The answers, read one at a time: ML Kit while it is on and has its model (it
// answers a frame or more later), else the matcher alone.
RDE_INTERNAL void kana_exam_grade(kana_exam* _exam) {
    static f64 _asked_at = 0.0;
    const f64  _now      = rde_engine_get_time_now();
    if(_exam->grading != UINT32_MAX) {
        kana_exam_item* _it = &_exam->items[_exam->grading];
        if(kana_recognize_poll(&_exam->recognition)) {
            kana_exam_mark(_exam, _it, &_exam->recognition);
            _exam->grading = UINT32_MAX;
        } else if(_now - _asked_at > KANA_EXAM_GRADE_WAIT) {
            kana_recognize_forget(&_exam->recognition);   // no answer: the matcher decides
            kana_exam_mark(_exam, _it, NULL);
            _exam->grading = UINT32_MAX;
        }
        return;
    }
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        kana_exam_item* _it = &_exam->items[_exam->order[_i]];
        if(!_it->answered || _it->graded) {
            continue;
        }
        if(kana_ink_alive_strokes(&_it->ink) > 0 && kana_recognize_available()) {
            if(kana_recognize_start(&_exam->recognition, &_it->ink)) {
                _exam->grading = _exam->order[_i];
                _asked_at      = _now;
            }
            return;   // started, or ML Kit busy elsewhere: a later frame
        }
        kana_exam_mark(_exam, _it, NULL);
        return;   // one a frame: the matcher takes a moment
    }
}

// All marked: kept in the log, and the marks follow.
RDE_INTERNAL void kana_exam_keep(kana_exam* _exam) {
    static kana_examlog_item _log[KANA_EXAM_MAX];
    const kana_ink*          _drawings[KANA_EXAM_MAX];
    u32                      _known[KANA_EXAM_MAX];
    u32                      _studying[KANA_EXAM_MAX];
    u32                      _nk = 0;
    u32                      _ns = 0;
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        const kana_exam_item* _it = &_exam->items[_exam->order[_i]];
        kana_kanji_info _info, _read;
        kana_kanji_at(_exam->db, _it->record, &_info);
        _log[_i] = (kana_examlog_item){
            .codepoint = _info.codepoint, .correct = _it->correct, .score = _it->score, .quality = _it->quality,
            .read_as   = _it->read_as != UINT32_MAX && kana_kanji_at(_exam->db, _it->read_as, &_read) ? _read.codepoint : 0u
        };
        _drawings[_i] = &_it->ink;
    }
    kana_examlog_add((u64)time(NULL), (u8)_exam->source, _log, _exam->asked, _drawings, KANA_EXAM_UNITS);

    for(u32 _i = 0; _i < _exam->asked; _i++) {
        const u32 _cp = _log[_i].codepoint;
        if(_log[_i].correct && kana_examlog_streak(_cp) >= KANA_EXAM_KNOWN_STREAK && kana_marks_get(_cp) != KANA_MARK_KNOWN) {
            _known[_nk++] = _cp;
        } else if(!_log[_i].correct && kana_marks_get(_cp) == KANA_MARK_KNOWN) {
            _studying[_ns++] = _cp;
        }
    }
    kana_marks_set_many(_known, _nk, KANA_MARK_KNOWN);
    kana_marks_set_many(_studying, _ns, KANA_MARK_STUDYING);
    // Every answer written from memory is a review (review.h): the schedules move.
    for(u32 _i = 0; _i < _exam->asked; _i++) {
        kana_reviews_answer(_log[_i].codepoint, _log[_i].correct, _log[_i].quality);
    }
    _exam->saved = true;
}

u32 kana_exam_wrong(const kana_exam* _exam, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked && _n < _max; _i++) {
        const kana_exam_item* _it = &_exam->items[_exam->order[_i]];
        if(_it->graded && !_it->correct) {
            _out[_n++] = _it->record;
        }
    }
    return _n;
}

void kana_exam_retry_wrong(kana_exam* _exam) {
    u32       _wrong[KANA_EXAM_MAX];
    const u32 _n = kana_exam_wrong(_exam, _wrong, KANA_EXAM_MAX);
    if(_n == 0 || !kana_exam_graded(_exam)) {
        return;
    }
    kana_exam_shuffle(_exam, _wrong, _n);
    kana_exam_set_items(_exam, _wrong, _n);
    kana_exam_start(_exam);
}

u32 kana_exam_asked(const kana_exam* _exam, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _exam->asked && _n < _max; _i++) {
        _out[_n++] = _exam->items[_exam->order[_i]].record;
    }
    return _n;
}

b8 kana_exam_take_tap(kana_exam* _exam, u32* _record) {
    if(_exam->tapped < 0) {
        return false;
    }
    *_record       = _exam->items[_exam->tapped].record;
    _exam->tapped  = -1;
    return true;
}

// --- the pointer -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F kana_exam_local(const kana_exam* _exam, rde_vec_2F _screen) {
    const f32 _k = KANA_EXAM_UNITS / _exam->square_size;
    return (rde_vec_2F){
        rde_math_clamp_f32((_screen.x - _exam->square_tl.x) * _k, 0.0f, KANA_EXAM_UNITS),
        rde_math_clamp_f32((_screen.y - (_exam->square_tl.y - _exam->square_size)) * _k, 0.0f, KANA_EXAM_UNITS)
    };
}

void kana_exam_pointer_down(kana_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_it != NULL) {
        // Writing: only the pen, only in the square (a resting hand does nothing).
        const b8 _in = _screen.x >= _exam->square_tl.x && _screen.x <= _exam->square_tl.x + _exam->square_size &&
                       _screen.y <= _exam->square_tl.y && _screen.y >= _exam->square_tl.y - _exam->square_size;
        if(_pen && _in && _exam->square_size > 0.0f) {
            _it->ink.zoom = _exam->square_size / KANA_EXAM_UNITS;   // the min step stays a screen unit
            kana_ink_begin(&_it->ink, kana_exam_local(_exam, _screen), true, false);
            _exam->pen = true;
        }
        return;
    }
    kana_scroller_down(&_exam->scroller, _screen, _time);
}

void kana_exam_pointer_moved(kana_exam* _exam, rde_vec_2F _screen, f64 _time) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_exam->pen && _it != NULL) {
        kana_ink_extend(&_it->ink, kana_exam_local(_exam, _screen));
        return;
    }
    kana_scroller_moved(&_exam->scroller, _screen, _time);
}

void kana_exam_pointer_up(kana_exam* _exam, f64 _time) {
    kana_exam_item* _it = kana_exam_current(_exam);
    if(_exam->pen) {
        if(_it != NULL) {
            kana_ink_end(&_it->ink);
        }
        _exam->pen = false;
        return;
    }
    kana_scroller_up(&_exam->scroller, _time);
}

// The grid cell under _p (the preview's items, the results' asked ones), or -1.
RDE_INTERNAL i32 kana_exam_cell_at(const kana_exam* _exam, rde_vec_2F _p, u32 _cells) {
    if(_exam->cell <= 0.0f || _p.x < _exam->grid_min.x || _p.x > _exam->grid_max.x || _p.y < _exam->grid_min.y || _p.y > _exam->grid_max.y) {
        return -1;
    }
    const u32 _col = (u32)((_p.x - _exam->grid_min.x) / _exam->cell);
    const u32 _row = (u32)((_exam->grid_max.y - _p.y + _exam->scroller.offset) / _exam->cell);
    const u32 _i   = _row * _exam->columns + _col;
    return _col < _exam->columns && _i < _cells ? (i32)_i : -1;
}

RDE_INTERNAL void kana_exam_tap(kana_exam* _exam, rde_vec_2F _at) {
    switch(_exam->stage) {
        case KANA_EXAM_SETUP:
            for(u32 _i = 0; _i < _exam->chip_count; _i++) {
                const kana_exam_chip* _c = &_exam->chips[_i];
                if(_at.x >= _c->min.x && _at.x <= _c->max.x && _at.y >= _c->min.y && _at.y <= _c->max.y) {
                    if(_c->kind == 0u) { _exam->source = (KANA_EXAM_SOURCE_)_c->value; }
                    else               { _exam->length = _c->value; }
                }
            }
            break;
        case KANA_EXAM_PREVIEW: {
            const i32 _i = kana_exam_cell_at(_exam, _at, _exam->count);
            if(_i >= 0) {
                _exam->items[_i].included = !_exam->items[_i].included;
            }
        } break;
        case KANA_EXAM_RESULTS: {
            const i32 _i = kana_exam_cell_at(_exam, _at, _exam->asked);
            if(_i >= 0) {
                _exam->tapped = (i32)_exam->order[_i];
            }
        } break;
        default: break;
    }
}

void kana_exam_update(kana_exam* _exam, f32 _dt) {
    if(!_exam->open) {
        return;
    }
    kana_scroller_update(&_exam->scroller, _dt, _exam->content_h, _exam->grid_max.y - _exam->grid_min.y);
    rde_vec_2F _at;
    if(kana_scroller_take_tap(&_exam->scroller, &_at)) {
        kana_exam_tap(_exam, _at);
    }
    kana_exam_grade(_exam);
    if(_exam->stage == KANA_EXAM_RESULTS && !_exam->saved && _exam->asked > 0 && kana_exam_graded(_exam)) {
        kana_exam_keep(_exam);
    }
}

// --- drawing ---------------------------------------------------------------------------

// A chip: a pill drawn here, not UI (the setup's choices are many and change
// with what there is to choose from). The accent when chosen.
RDE_INTERNAL void kana_exam_chip_draw(rde_font* _font, f32 _font_px, rde_vec_2F _min, rde_vec_2F _max, const c8* _label, b8 _chosen, b8 _usable) {
    const kana_theme* _theme = kana_theme_active();
    const rde_vec_2F  _size  = { _max.x - _min.x, _max.y - _min.y };
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f }, _size, 1.0f, 10,
                                            _chosen ? _theme->accent : _theme->surface_2, NULL);
    const f32 _px = KANA_EXAM_CHIP_PX;
    const f32 _w  = kana_draw_text_width(_font, _font_px, _label, _px);
    kana_draw_text(_font, _font_px, _label, (_min.x + _max.x - _w) * 0.5f, (_min.y + _max.y) * 0.5f - _px * 0.42f, _px,
                   !_usable ? _theme->button_text_disabled : _chosen ? _theme->on_accent : _theme->text);
}

// A label that is a Japanese character (試, 音, 訓), written from its strokes in
// the accent on its tint, _box square, its top-left at _tl.
RDE_INTERNAL void kana_exam_glyph_label(kana_exam* _exam, u32 _codepoint, rde_vec_2F _tl, f32 _box) {
    const kana_theme* _theme = kana_theme_active();
    kana_draw_card((rde_vec_2F){ _tl.x, _tl.y - _box }, (rde_vec_2F){ _tl.x + _box, _tl.y }, _box * 0.26f, _theme->tint, _theme->tint);
    const f32 _in = _box * 0.14f;
    kana_glyph_character(&_exam->glyph, _codepoint, (rde_vec_2F){ _tl.x + _in, _tl.y - _in }, _box - 2.0f * _in, _theme->accent);
}

// The screen's title: 試 and _title, a caption under it; _right_text at the right.
RDE_INTERNAL void kana_exam_header(kana_exam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top,
                                   const c8* _title, const c8* _caption, const c8* _right_text) {
    const kana_theme* _theme = kana_theme_active();
    kana_exam_glyph_label(_exam, 0x8A66u, (rde_vec_2F){ _left, _top }, 40.0f);   // 試
    kana_draw_text(_font, _font_px, _title, _left + 54.0f, _top - 19.0f, KANA_EXAM_TITLE_PX, _theme->text);
    if(_caption != NULL) {
        kana_draw_text(_font, _font_px, _caption, _left + 54.0f, _top - 37.0f, KANA_EXAM_CAPTION_PX, _theme->text_soft);
    }
    if(_right_text != NULL) {
        kana_draw_text(_font, _font_px, _right_text, _right - kana_draw_text_width(_font, _font_px, _right_text, 14.0f), _top - 25.0f, 14.0f, _theme->text);
    }
}

RDE_INTERNAL void kana_exam_render_setup(kana_exam* _exam, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top) {
    const kana_theme* _theme = kana_theme_active();
    kana_exam_header(_exam, _font, _font_px, _left, _right, _top, kana_text(KANA_TEXT_EXAM), kana_text(KANA_TEXT_EXAM_INTRO), NULL);
    f32 _y = _top - 56.0f;

    // What it is of, and how many.
    _exam->chip_count = 0;
    c8 _label[48];
    for(u32 _group = 0; _group < 2u; _group++) {
        _y -= 30.0f;
        kana_draw_text(_font, _font_px, kana_text(_group == 0 ? KANA_TEXT_EXAM_WHAT : KANA_TEXT_EXAM_HOW_MANY), _left, _y, KANA_EXAM_CAPTION_PX, _theme->text_soft);
        _y -= 12.0f;
        f32       _x   = _left;
        const u32 _n   = _group == 0 ? (u32)KANA_EXAM_SOURCE_COUNT : KANA_EXAM_LENGTHS;
        for(u32 _i = 0; _i < _n; _i++) {
            b8 _usable = true;
            b8 _chosen;
            if(_group == 0) {
                if((_i == KANA_EXAM_SOURCE_SELECTION && rde_arr_length(&_exam->selection) == 0) || _i == KANA_EXAM_SOURCE_REVIEW) {
                    continue;   // only when opened from Select mode; Reviews has its own way in
                }
                const u32 _size = kana_exam_source_size(_exam, (KANA_EXAM_SOURCE_)_i);
                snprintf(_label, sizeof(_label), "%s  %u", kana_exam_source_name((KANA_EXAM_SOURCE_)_i), _size);
                _usable = _size > 0;
                _chosen = _exam->source == (KANA_EXAM_SOURCE_)_i;
            } else {
                snprintf(_label, sizeof(_label), "%s", KANA_EXAM_LENGTH_NAMES[_i] != NULL ? KANA_EXAM_LENGTH_NAMES[_i] : kana_text(KANA_TEXT_ALL));
                _chosen = _exam->length == _i;
            }
            const f32 _w = fmaxf(64.0f, kana_draw_text_width(_font, _font_px, _label, KANA_EXAM_CHIP_PX) + 36.0f);
            if(_x + _w > _right && _x > _left) {   // on to another row
                _x  = _left;
                _y -= KANA_EXAM_CHIP_H + KANA_EXAM_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - KANA_EXAM_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            kana_exam_chip_draw(_font, _font_px, _min, _max, _label, _chosen, _usable);
            _exam->chips[_exam->chip_count++] = (kana_exam_chip){ _min, _max, (u8)_group, (u8)_i };
            _x += _w + KANA_EXAM_CHIP_GAP;
        }
        _y -= KANA_EXAM_CHIP_H;
    }

    // What that makes, in a card.
    _y -= 24.0f;
    const u32 _size    = kana_exam_source_size(_exam, _exam->source);
    const u32 _planned = kana_exam_planned(_exam);
    kana_draw_card((rde_vec_2F){ _left, _y - 76.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
    if(_size == 0) {
        snprintf(_label, sizeof(_label), "%s", kana_text(_exam->source == KANA_EXAM_SOURCE_STUDYING ? KANA_TEXT_EXAM_NOTHING_STUDYING : KANA_TEXT_EXAM_NOTHING));
        kana_draw_text(_font, _font_px, _label, _left + 18.0f, _y - 32.0f, 15.0f, _theme->text);
        kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_EXAM_MARK_HINT), _left + 18.0f, _y - 56.0f, 12.0f, _theme->text_soft);
    } else {
        c8 _line[160];
        KANA_TEXTF(_line, KANA_TEXT_EXAM_PLAN, KANA_TN(_planned), KANA_TN(_size), KANA_TS(kana_exam_source_name(_exam->source)));
        kana_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 32.0f, 15.0f, _theme->text);
        snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_EXAM_RULE));
        if(_size > KANA_EXAM_MAX && KANA_EXAM_LENGTH_VALUES[_exam->length] == 0u) {
            KANA_TEXTF(_line, KANA_TEXT_EXAM_MAX, KANA_TN(KANA_EXAM_MAX));
        }
        kana_draw_text(_font, _font_px, _line, _left + 18.0f, _y - 56.0f, 12.0f, _theme->text_soft);
    }
}

// A grid below _top: cells for _count things, scrolled; the layout kept for taps.
RDE_INTERNAL void kana_exam_grid(kana_exam* _exam, f32 _left, f32 _right, f32 _top, f32 _bottom, f32 _cell_min, u32 _count) {
    const f32 _width = _right - _left;
    _exam->columns   = (u32)fmaxf(1.0f, floorf(_width / _cell_min));
    _exam->cell      = _width / (f32)_exam->columns;
    _exam->grid_min  = (rde_vec_2F){ _left, _bottom };
    _exam->grid_max  = (rde_vec_2F){ _right, _top };
    _exam->content_h = (f32)((_count + _exam->columns - 1u) / _exam->columns) * _exam->cell;
}

RDE_INTERNAL void kana_exam_render_preview(kana_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    c8 _line[160];
    KANA_TEXTF(_line, KANA_TEXT_OF_N, KANA_TN(kana_exam_included(_exam)), KANA_TN(_exam->count));
    c8 _title[96];
    KANA_TEXTF(_title, KANA_TEXT_EXAM_TITLE_SOURCE, KANA_TS(kana_exam_source_name(_exam->source)));
    kana_exam_header(_exam, _font, _font_px, _left, _right, _top, _title, kana_text(KANA_TEXT_EXAM_TAP_TO_LEAVE), _line);
    RDE_UNUSED(_theme);

    const f32 _grid_top = _top - 60.0f;
    kana_exam_grid(_exam, _left, _right, _grid_top, _bottom, KANA_EXAM_CELL_MIN, _exam->count);
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
        kana_kanji_info _info;
        if(!kana_kanji_at(_exam->db, _exam->items[_i].record, &_info)) {
            continue;
        }
        const b8 _on = _exam->items[_i].included;
        kana_selection_draw_behind(_on, (rde_vec_2F){ _x, _y }, _cell);
        kana_glyph_character(&_exam->glyph, _info.codepoint, (rde_vec_2F){ _x + (_cell - _glyph) * 0.5f, _y - (_cell - _glyph) * 0.5f }, _glyph,
                             _on ? _theme->ink : _theme->ghost);
        kana_selection_draw_tick(_on, (rde_vec_2F){ _x, _y }, _cell);
    }
    rde_rendering_end_clipping_rect();
}

// A word with the kanji in it, the kanji blanked (□): the learner's own first
// (userwords.h), then the character's — of two characters or more (the kanji
// alone leaves nothing to show). False when there is none.
RDE_INTERNAL b8 kana_exam_blanked_word(kana_exam* _exam, const kana_kanji_info* _info, u32 _record, c8* _blanked, usize _size, kana_kanji_word* _out) {
    u32       _words[KANA_KANJI_MAX_WORDS];
    const u32 _yours = kana_userwords_count(_info->codepoint);
    const u32 _nw    = kana_kanji_words(_exam->db, _record, _words, KANA_KANJI_MAX_WORDS, NULL);
    for(u32 _w = 0; _w < _yours + _nw; _w++) {
        if(_w < _yours ? !kana_userwords_at(_info->codepoint, _w, _out) : !kana_kanji_word_at(_exam->db, _words[_w - _yours], _out)) {
            continue;
        }
        usize     _len   = 0;
        u32       _chars = 0;
        const c8* _p     = _out->written;
        _blanked[0] = 0;
        for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0; _cp = kana_kanji_utf8_next(&_p)) {
            c8 _one[8];
            if(_cp == _info->codepoint) { snprintf(_one, sizeof(_one), "\xE2\x96\xA1"); }   // □
            else                        { kana_kanji_utf8(_cp, _one); }
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
RDE_INTERNAL f32 kana_exam_prompt(kana_exam* _exam, const kana_kanji_info* _info, u32 _record, b8 _kana, rde_font* _font, f32 _font_px,
                                  f32 _left, f32 _right, f32 _y) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _pad   = 18.0f;
    const f32         _x     = _left + _pad;
    c8                _line[256];

    // How tall it is: the caption and the big line, then a line a reading, and the word.
    const c8* _on  = _kana ? "" : kana_kanji_on(_exam->db, _info);
    const c8* _kun = _kana ? "" : kana_kanji_kun(_exam->db, _info);
    c8              _blanked[96];
    kana_kanji_word _word;
    const b8        _has_word = !_kana && kana_exam_blanked_word(_exam, _info, _record, _blanked, sizeof(_blanked), &_word);
    const u32       _readings = (_on[0] != 0 ? 1u : 0u) + (_kun[0] != 0 ? 1u : 0u);
    const f32       _h = _pad + 18.0f + (_kana ? 64.0f : 40.0f) + (_readings > 0 ? 44.0f : 0.0f) + (_has_word ? 52.0f : 0.0f) + _pad - 6.0f;
    kana_draw_card((rde_vec_2F){ _left, _y - _h }, (rde_vec_2F){ _right, _y }, 16.0f, _theme->surface, _theme->outline);

    f32 _cy = _y - _pad - 10.0f;
    if(_kana) {
        const b8 _katakana = _info->codepoint >= 0x30A0u && _info->codepoint <= 0x30FFu;
        kana_draw_text(_font, _font_px, kana_text(_katakana ? KANA_TEXT_EXAM_WRITE_KATAKANA : KANA_TEXT_EXAM_WRITE_HIRAGANA), _x, _cy, KANA_EXAM_CAPTION_PX, _theme->text_soft);
        kana_draw_text(_font, _font_px, kana_chart_romaji(_info->codepoint), _x, _cy - 54.0f, 40.0f, _theme->text);
        return _y - _h;
    }
    kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_EXAM_WRITE_KANJI), _x, _cy, KANA_EXAM_CAPTION_PX, _theme->text_soft);
    const c8* _meaning = kana_kanji_meanings(_exam->db, _info);
    kana_draw_text_fit(_font, _font_px, _meaning[0] != 0 ? _meaning : kana_text(KANA_TEXT_NO_MEANING), KANA_EXAM_MEANING_PX, _right - _pad - _x, _line, sizeof(_line));
    kana_draw_text(_font, _font_px, _line, _x, _cy - 34.0f, KANA_EXAM_MEANING_PX, _theme->text);
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
            kana_exam_glyph_label(_exam, _k == 0u ? 0x97F3u : 0x8A13u, (rde_vec_2F){ _rx, _mid + 15.0f }, 30.0f);   // 音, 訓
            const f32 _end = kana_glyph_reading(&_exam->glyph, _text, (rde_vec_2F){ _rx + 42.0f, _mid + 12.0f }, 24.0f,
                                                _k == 0u && _kun[0] != 0 ? (_left + _right) * 0.5f + 40.0f : _right - _pad, _theme->ink, _theme->text_soft);
            _rx = fmaxf(_end + 28.0f, _k == 0u ? _x : _rx);
        }
        _cy -= 44.0f;
    }

    // The word, on the page's colour inside the card.
    if(_has_word) {
        const f32 _mid = _cy - 26.0f;
        kana_draw_card((rde_vec_2F){ _x - 6.0f, _mid - 20.0f }, (rde_vec_2F){ _right - _pad + 6.0f, _mid + 20.0f }, 10.0f, _theme->page, _theme->page);
        kana_draw_text(_font, _font_px, _blanked, _x + 6.0f, _mid - KANA_EXAM_WORD_PX * 0.38f, KANA_EXAM_WORD_PX, _theme->ink);
        const f32 _rx = _x + 6.0f + kana_draw_text_width(_font, _font_px, _blanked, KANA_EXAM_WORD_PX) + 14.0f;
        kana_draw_text(_font, _font_px, _word.reading, _rx, _mid - 13.0f * 0.42f, 13.0f, _theme->text_soft);
        const f32 _mx = _rx + kana_draw_text_width(_font, _font_px, _word.reading, 13.0f) + 14.0f;
        kana_draw_text_fit(_font, _font_px, _word.meaning, 13.0f, _right - _pad - _mx, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _mx, _mid - 13.0f * 0.42f, 13.0f, _theme->text);
    }
    return _y - _h;
}

RDE_INTERNAL void kana_exam_render_writing(kana_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    kana_exam_item*   _it    = kana_exam_current(_exam);
    kana_kanji_info   _info;
    if(_it == NULL || !kana_kanji_at(_exam->db, _it->record, &_info)) {
        return;
    }

    // The title, where it is, and the progress: a segment per character (a bar
    // when there are many).
    c8 _title[96];
    c8 _line[64];
    KANA_TEXTF(_title, KANA_TEXT_EXAM_TITLE_SOURCE, KANA_TS(kana_exam_source_name(_exam->source)));
    KANA_TEXTF(_line, KANA_TEXT_OF_N, KANA_TN(_exam->current + 1u), KANA_TN(_exam->asked));
    kana_exam_header(_exam, _font, _font_px, _left, _right, _top, _title, kana_text(KANA_TEXT_EXAM_ONCE_EACH), _line);
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
    const b8 _kana = kana_chart_romaji(_info.codepoint) != NULL && _info.text == UINT32_MAX;
    const f32 _y   = kana_exam_prompt(_exam, &_info, _it->record, _kana, _font, _font_px, _left, _right, _py - 16.0f);

    // The square.
    const f32 _room   = _y - 18.0f - (_bottom + 26.0f);
    const f32 _square = fmaxf(120.0f, fminf(fminf(_right - _left, _room), KANA_EXAM_SQUARE_MAX));
    const rde_vec_2F _tl = { (_left + _right) * 0.5f - _square * 0.5f, _y - 18.0f };
    _exam->square_tl   = _tl;
    _exam->square_size = _square;
    kana_glyph_box(_tl, _square);
    const rde_vec_2I _size = rde_window_get_size(_window);
    kana_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _square }, _square / KANA_EXAM_UNITS,
                    (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
    kana_draw_text(_font, _font_px, kana_text(kana_exam_at_last(_exam) ? KANA_TEXT_EXAM_THEN_FINISH : KANA_TEXT_EXAM_THEN_NEXT),
                   _tl.x, _tl.y - _square - 20.0f, 12.0f, _theme->text_soft);
}

RDE_INTERNAL void kana_exam_render_results(kana_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    c8 _line[160];
    if(!kana_exam_graded(_exam)) {
        KANA_TEXTF(_line, KANA_TEXT_EXAM_READING_ANSWERS, KANA_TN(kana_exam_graded_count(_exam)), KANA_TN(_exam->asked));
        kana_draw_text(_font, _font_px, _line, _left, _top - 30.0f, KANA_EXAM_TITLE_PX, _theme->text);
    } else {
        u32 _right_n = 0;
        f32 _points  = 0.0f;
        for(u32 _i = 0; _i < _exam->asked; _i++) {
            _right_n += _exam->items[_exam->order[_i]].correct ? 1u : 0u;
            _points  += _exam->items[_exam->order[_i]].score;
        }
        const b8 _passed = (f32)_right_n >= KANA_EXAM_PASS * (f32)_exam->asked;
        KANA_TEXTF(_line, KANA_TEXT_EXAM_RESULT, KANA_TN(_right_n), KANA_TN(_exam->asked), KANA_TN(lroundf(_points / (f32)_exam->asked)));
        kana_draw_text(_font, _font_px, _line, _left, _top - 30.0f, KANA_EXAM_TITLE_PX, _theme->text);
        kana_draw_chip(_font, _font_px, kana_text(_passed ? KANA_TEXT_EXAM_PASSED : KANA_TEXT_EXAM_NOT_PASSED), _left + kana_draw_text_width(_font, _font_px, _line, KANA_EXAM_TITLE_PX) + 16.0f,
                       _top - 30.0f + KANA_EXAM_TITLE_PX * 0.42f, 12.0f, _passed ? _theme->score_good : _theme->score_poor, _theme->on_accent);
    }

    // A kept exam: which, and when.
    f32 _grid_top = _top - 56.0f;
    if(_exam->kept < kana_examlog_count()) {
        c8 _title[64], _when[96];
        KANA_TEXTF(_title, KANA_TEXT_EXAM_TITLE_SOURCE, KANA_TS(kana_exam_source_name(_exam->source)));
        kana_text_date_time(_when, sizeof(_when), kana_examlog_exams()[_exam->kept].time);
        KANA_TEXTF(_line, KANA_TEXT_EXAM_KEPT, KANA_TS(_title), KANA_TS(_when));
        kana_draw_text(_font, _font_px, _line, _left, _top - 54.0f, kana_draw_text_px_to_fit(_font, _font_px, _line, KANA_EXAM_CAPTION_PX, _right - _left, 0.6f),
                       _theme->text_soft);
        _grid_top = _top - 72.0f;
    }
    kana_exam_grid(_exam, _left, _right, _grid_top, _bottom, KANA_EXAM_RESULT_MIN, _exam->asked);
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
        kana_exam_item* _it = &_exam->items[_exam->order[_i]];
        kana_kanji_info _info;
        if(!kana_kanji_at(_exam->db, _it->record, &_info)) {
            continue;
        }
        // The writing, in its square; what was asked, small, in the corner.
        const f32 _sq = _cell - 10.0f;
        const rde_vec_2F _tl = { _x + 5.0f, _y - 5.0f };
        kana_glyph_box(_tl, _sq);
        kana_ink_render(&_it->ink, (rde_vec_2F){ _tl.x, _tl.y - _sq }, _sq / KANA_EXAM_UNITS,
                        (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), false);
        kana_glyph_character(&_exam->glyph, _info.codepoint, (rde_vec_2F){ _tl.x + 4.0f, _tl.y - 4.0f }, _sq * 0.26f, _theme->text_soft);

        // Right or wrong, and the points.
        const f32        _r = fmaxf(10.0f, _sq * 0.1f);
        const rde_vec_2F _c = { _tl.x + _sq - _r - 5.0f, _tl.y - _r - 5.0f };
        if(!_it->graded) {
            rde_rendering_2d_draw_circle_border(_c, _r, 1.5f, 24, _theme->text_soft, NULL);
            continue;
        }
        kana_draw_verdict(_c, _r, _it->correct);
        if(_it->correct) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_it->score);
            kana_draw_text(_font, _font_px, _line, _tl.x + _sq - kana_draw_text_width(_font, _font_px, _line, 18.0f) - 6.0f, _tl.y - _sq + 8.0f, 18.0f,
                           kana_theme_grade(_it->score));
        } else if(_it->read_as != UINT32_MAX && _it->read_as != _it->record) {
            // What it read as instead.
            kana_kanji_info _read;
            if(kana_kanji_at(_exam->db, _it->read_as, &_read)) {
                kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_EXAM_READ_AS), _tl.x + 6.0f, _tl.y - _sq + 8.0f, 11.0f, _theme->text_soft);
                const f32 _after = _tl.x + 12.0f + kana_draw_text_width(_font, _font_px, kana_text(KANA_TEXT_EXAM_READ_AS), 11.0f);
                kana_glyph_character(&_exam->glyph, _read.codepoint, (rde_vec_2F){ _after, _tl.y - _sq + 26.0f }, 20.0f, _theme->score_poor);
            }
        }
    }
    rde_rendering_end_clipping_rect();
}

void kana_exam_render(kana_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_exam->open) {
        return;
    }
    const rde_vec_2I _size   = rde_window_get_size(_window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32        _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_EXAM_MARGIN;
    const f32        _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_EXAM_MARGIN;
    switch(_exam->stage) {
        case KANA_EXAM_SETUP:   kana_exam_render_setup(_exam, _font, _font_px, _left, _right, _top); _exam->grid_min = _exam->grid_max = (rde_vec_2F){ 0 }; _exam->content_h = 0.0f; break;
        case KANA_EXAM_PREVIEW: kana_exam_render_preview(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case KANA_EXAM_WRITING: kana_exam_render_writing(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
        case KANA_EXAM_RESULTS: kana_exam_render_results(_exam, _window, _font, _font_px, _left, _right, _top, _bottom); break;
    }
}
