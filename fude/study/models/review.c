// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/review.h"
#include "drawing/base/redraw.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"


// ===========================================================================
// See review.h.
//
// The schedules are few (one per character or word being learnt), so they are an array sorted by key and searched by
// bisection, and the whole file is written again after each sitting's answers. Days are day numbers (days since
// 1970-01-01), worked out from the device's local date with Howard Hinnant's civil-date formula.
// ===========================================================================

// The file's format version, the tags of its kind and of its one chunk, and the size in bytes of a schedule record as
// written (a reader accepts longer ones).
#define FUDE_REVIEWS_VERSION     1u
#define FUDE_REVIEWS_KIND        FUDE_TAG('R', 'E', 'V', 'W')
#define FUDE_REVIEWS_CHUNK       FUDE_TAG('R', 'V', 'W', 'S')
#define FUDE_REVIEWS_RECORD_SIZE 20u
#define FUDE_REVIEWS_EASE        250u   // A new schedule's ease, in hundredths (2.5).
// The lowest and the highest ease, in hundredths (1.3 and 3.0).
#define FUDE_REVIEWS_EASE_MIN    130u
#define FUDE_REVIEWS_EASE_MAX    300u
// Which allowance a key counts against: vocab.h's word keys are above every code point.
#define FUDE_REVIEW_KIND(_key)   ((_key) > 0x10FFFFu ? 1u : 0u)   // 0 for a character (a code point), 1 for a word.

// One schedule, as kept in memory and in the file.
typedef struct {
    u32 codepoint;  // The key: a code point, or a word's FUDE_VOCAB_KEY.
    u16 interval;   // The last step, in days.
    u16 ease;       // How easily it comes, in hundredths.
    u32 due;        // The day it is next due, as a day number.
    u16 reps;       // Right answers in a row, given in time.
    u16 lapses;     // Wrong answers, ever.
    u32 first;      // The day of its first review.
} fude_review;

// The schedules, sorted by key. Created on first use (fude_reviews_ensure), so they work in memory without a file, as
// the tests use them.
static rde_arr TYPE(fude_review) fude_reviews_all;
static b8                        fude_reviews_ready = false;
// The file the schedules are saved to, or "" to keep them in memory only.
static c8                        fude_reviews_path[RDE_MAX_PATH];
// Goes up on every change; fude_reviews_revision returns it.
static u32                       fude_reviews_changes = 0;

// Creates the schedules' array the first time it is needed.
RDE_INTERNAL void fude_reviews_ensure(void) {
    if(!fude_reviews_ready) {
        fude_reviews_all   = rde_arr_new(sizeof(fude_review), rde_memory_allocator_get_default_std());
        fude_reviews_ready = true;
    }
}

// Finds the schedule of key _codepoint by bisection. When there is one, sets *_found to true and returns its index;
// otherwise sets *_found to false and returns the index where it would be inserted to keep the array sorted.
RDE_INTERNAL u32 fude_reviews_find(u32 _codepoint, b8* _found) {
    const fude_review* _r  = (const fude_review*)fude_reviews_all.memory;
    u32                _lo = 0;
    u32                _hi = (u32)rde_arr_length(&fude_reviews_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_r[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&fude_reviews_all) && _r[_lo].codepoint == _codepoint;
    return _lo;
}

// Writes the whole file again. Returns false when it could not be written, and true when it was or when the schedules
// are kept in memory only.
RDE_INTERNAL b8 fude_reviews_save(void) {
    if(fude_reviews_path[0] == 0) {
        return true;   // The schedules are kept in memory only.
    }
    const u32  _n = (u32)rde_arr_length(&fude_reviews_all);
    fude_bytes _b = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 32u + _n * FUDE_REVIEWS_RECORD_SIZE + 1u);
    fude_put_header(&_b, FUDE_REVIEWS_VERSION, FUDE_REVIEWS_KIND);
    const u32 _chunk = fude_chunk_begin(&_b, FUDE_REVIEWS_CHUNK);
    fude_put_u32(&_b, _n);
    fude_put_u32(&_b, FUDE_REVIEWS_RECORD_SIZE);
    const fude_review* _r = (const fude_review*)fude_reviews_all.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        fude_put_u32(&_b, _r[_i].codepoint);
        fude_put_u16(&_b, _r[_i].interval);
        fude_put_u16(&_b, _r[_i].ease);
        fude_put_u32(&_b, _r[_i].due);
        fude_put_u16(&_b, _r[_i].reps);
        fude_put_u16(&_b, _r[_i].lapses);
        fude_put_u32(&_b, _r[_i].first);
    }
    fude_chunk_end(&_b, _chunk);
    return fude_bytes_write_and_free(&_b, fude_reviews_path, NULL);
}

// Reads the reviews file's bytes into the schedules, starting from none; fude_save_load (save.h) calls it, a second
// time for the .bak when the main file fails. A second schedule with the same key is dropped. Returns false when the
// header is not that of a reviews file of this version.
RDE_INTERNAL b8 fude_reviews_read(const u8* _data, u32 _size, any _user) {
    RDE_UNUSED(_user);
    rde_arr_clear(&fude_reviews_all);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_REVIEWS_VERSION, FUDE_REVIEWS_KIND)) {
        return false;
    }
    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_REVIEWS_CHUNK) {
            continue;
        }
        const u32 _count  = fude_get_u32(&_chunk);
        const u32 _record = fude_get_u32(&_chunk);
        if(!_chunk.ok || _record < FUDE_REVIEWS_RECORD_SIZE || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            return false;   // The schedules are damaged: the file is, so its backup is read instead (save.h).
        }
        for(u32 _i = 0; _i < _count; _i++) {
            fude_reader _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            fude_review _v;
            _v.codepoint = fude_get_u32(&_e);
            _v.interval  = fude_get_u16(&_e);
            _v.ease      = fude_get_u16(&_e);
            _v.due       = fude_get_u32(&_e);
            _v.reps      = fude_get_u16(&_e);
            _v.lapses    = fude_get_u16(&_e);
            _v.first     = fude_get_u32(&_e);
            b8 _found;
            const u32 _at = fude_reviews_find(_v.codepoint, &_found);
            if(_e.ok && !_found) {
                rde_arr_insert(&fude_reviews_all, _at, &_v);
            }
        }
    }
    return true;
}

// Replaces the schedules with those saved at _path (none for NULL). See the header.
void fude_reviews_open(const c8* _path) {
    fude_reviews_ensure();
    rde_arr_clear(&fude_reviews_all);
    rde_snprintf(fude_reviews_path, sizeof(fude_reviews_path), "%s", _path != NULL ? _path : "");
    fude_reviews_changes++;
    if(_path == NULL) {
        return;
    }
    fude_save_load(_path, fude_reviews_read, NULL);   // When nothing has been saved yet, there are no schedules.
}

// Frees the schedules and forgets the path, without saving.
void fude_reviews_close(void) {
    if(fude_reviews_ready) {
        rde_arr_free(&fude_reviews_all);
        fude_reviews_ready = false;
    }
    fude_reviews_path[0] = 0;
    fude_reviews_changes++;   // Anything worked out from the schedules is now out of date, as with the other models.
}

// Returns the day number (days since 1970-01-01) of a civil date: year _y, month _m (1..12), day _d (1..31). This is
// Howard Hinnant's days_from_civil, which counts in 400-year eras starting on 1 March, so leap days fall at the end.
RDE_INTERNAL u32 fude_reviews_days(i32 _y, u32 _m, u32 _d) {
    _y -= _m <= 2u ? 1 : 0;
    const i32 _era = (_y >= 0 ? _y : _y - 399) / 400;
    const u32 _yoe = (u32)(_y - _era * 400);
    const u32 _doy = (153u * (_m > 2u ? _m - 3u : _m + 9u) + 2u) / 5u + _d - 1u;
    const u32 _doe = _yoe * 365u + _yoe / 4u - _yoe / 100u + _doy;
    return (u32)(_era * 146097 + (i32)_doe - 719468);
}

#if defined(FUDE_TESTS) || defined(RDE_DEBUG)
u32 fude_reviews_fake_today = 0;   // The day it is for the tests and the demo (review.h); 0 means the real day.
#endif

// Returns today's day number in the device's local time, or the fake day when the tests or the demo set one.
u32 fude_reviews_today(void) {
#if defined(FUDE_TESTS) || defined(RDE_DEBUG)
    if(fude_reviews_fake_today != 0) {
        return fude_reviews_fake_today;
    }
#endif
    const rde_date_components _today = rde_date_local_components(rde_date_now_unix_seconds());
    // Render on demand: what shows the day (the reviews due, "in 2 days") changes at midnight with no event, so the
    // frame just after it is a timer (asked each time the day is read for the screen).
    const u32 _into = (u32)_today.hour * 3600u + (u32)_today.minute * 60u + (u32)_today.second;
    fude_redraw_after((f64)(_into < 86400u ? 86400u - _into : 1u) + 0.5);
    return fude_reviews_days((i32)_today.year, (u32)_today.month, (u32)_today.day);
}

// Returns the day the key _codepoint is next due, or 0 when it has never been reviewed.
u32 fude_reviews_due_day(u32 _codepoint) {
    fude_reviews_ensure();
    b8        _found;
    const u32 _at = fude_reviews_find(_codepoint, &_found);
    return _found ? ((const fude_review*)fude_reviews_all.memory)[_at].due : 0u;
}

// A candidate that is due, as fude_reviews_due sorts them: its key, and the day it was due.
typedef struct { u32 codepoint; u32 due; } fude_review_due;

// The order for rde_sort of fude_reviews_due's list (fude_review_due): the earliest due day first, which is the
// longest waiting, and within the same day by key.
RDE_INTERNAL int fude_reviews_by_due(const any _a, const any _b) {
    const fude_review_due* _x = (const fude_review_due*)_a;
    const fude_review_due* _y = (const fude_review_due*)_b;
    if(_x->due != _y->due) {
        return _x->due < _y->due ? -1 : 1;
    }
    return _x->codepoint < _y->codepoint ? -1 : (_x->codepoint > _y->codepoint ? 1 : 0);
}

// Picks today's reviews from _candidates (see the header): first those due, the longest waiting first, then new ones
// while today's allowance lasts. Characters and words have separate allowances, each FUDE_REVIEW_NEW_PER_DAY less the
// ones of its kind first reviewed today.
u32 fude_reviews_due(const u32* _candidates, u32 _count, u32* _out, u32 _max) {
    fude_reviews_ensure();
    const u32 _today = fude_reviews_today();
    // The new ones today still has room for: the allowance less those first reviewed today, counted separately for
    // the characters and for the words.
    u32 _new_today[2] = { 0u, 0u };
    const fude_review* _all = (const fude_review*)fude_reviews_all.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_reviews_all); _i++) {
        _new_today[FUDE_REVIEW_KIND(_all[_i].codepoint)] += _all[_i].first == _today ? 1u : 0u;
    }
    u32 _room[2];
    for(u32 _k = 0; _k < 2u; _k++) {
        _room[_k] = _new_today[_k] < FUDE_REVIEW_NEW_PER_DAY ? FUDE_REVIEW_NEW_PER_DAY - _new_today[_k] : 0u;
    }

    // The candidates that are due, the longest waiting first.
    rde_arr TYPE(fude_review_due) _due_list = rde_arr_new_with_capacity(sizeof(fude_review_due), _count > 0u ? _count : 1u, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_due_list, _count);
    fude_review_due* _due = (fude_review_due*)_due_list.memory;
    u32              _nd  = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        b8        _found;
        const u32 _at = fude_reviews_find(_candidates[_i], &_found);
        if(_found && _all[_at].due <= _today) {
            _due[_nd++] = (fude_review_due){ _candidates[_i], _all[_at].due };
        }
    }
    if(_nd > 1u) {
        rde_sort(_due, _nd, sizeof(fude_review_due), fude_reviews_by_due);
    }
    u32 _n = 0;
    for(u32 _i = 0; _i < _nd && _n < _max && _n < FUDE_REVIEW_SESSION; _i++) {
        _out[_n++] = _due[_i].codepoint;
    }
    rde_arr_free(&_due_list);
    // Then the new ones (those with no schedule yet), in the order given.
    for(u32 _i = 0; _i < _count && _n < _max && _n < FUDE_REVIEW_SESSION; _i++) {
        b8        _found;
        const u32 _kind = FUDE_REVIEW_KIND(_candidates[_i]);
        fude_reviews_find(_candidates[_i], &_found);
        if(!_found && _room[_kind] > 0u) {
            _out[_n++] = _candidates[_i];
            _room[_kind]--;
        }
    }
    return _n;
}

// Moves one answer's schedule in memory, by the rules in review.h; fude_reviews_answer saves them all afterwards. A
// key with no schedule gets a new one, due today, first reviewed today, before the answer is applied. _quality is
// 0..1: 0.8 or more makes it easier, under 0.5 makes it harder.
RDE_INTERNAL void fude_reviews_step(u32 _codepoint, b8 _right, f32 _quality) {
    const u32 _today = fude_reviews_today();
    b8        _found;
    const u32 _at = fude_reviews_find(_codepoint, &_found);
    if(!_found) {
        const fude_review _fresh = { _codepoint, 0u, FUDE_REVIEWS_EASE, _today, 0u, 0u, _today };
        rde_arr_insert(&fude_reviews_all, _at, &_fresh);
    }
    fude_review* _v = &((fude_review*)fude_reviews_all.memory)[_at];
    if(_right) {
        if(_v->due > _today) {
            return;   // Answered early: the step was not waited for, so nothing changes.
        }
        // The step: 1 day, then 3, then the last step times the ease; always at least a day longer than the last,
        // and at most 3650 days (ten years).
        _v->reps++;
        u32 _step = _v->reps == 1u ? 1u : _v->reps == 2u ? 3u : (u32)rde_math_round_to_i64_f32((f32)_v->interval * (f32)_v->ease / 100.0f);
        _step = _step > (u32)_v->interval ? _step : (u32)_v->interval + 1u;
        _v->interval = (u16)(_step < 3650u ? _step : 3650u);
        if(_quality >= 0.8f && _v->ease + 10u <= FUDE_REVIEWS_EASE_MAX) {
            _v->ease += 10u;
        } else if(_quality < 0.5f && _v->ease >= FUDE_REVIEWS_EASE_MIN + 10u) {
            _v->ease -= 10u;
        }
        _v->due = _today + _v->interval;
    } else {
        _v->lapses++;
        _v->reps     = 0u;
        _v->interval = 0u;   // The steps start over, so the next right answer is a step of one day again.
        _v->ease     = _v->ease >= FUDE_REVIEWS_EASE_MIN + 20u ? (u16)(_v->ease - 20u) : (u16)FUDE_REVIEWS_EASE_MIN;
        _v->due      = _today + 1u;
    }
}

// Applies a sitting's answers and saves once; a failed save is logged. See the header.
void fude_reviews_answer(const fude_review_answer* _answers, u32 _count) {
    if(_count == 0) {
        return;
    }
    fude_reviews_ensure();
    for(u32 _i = 0; _i < _count; _i++) {
        fude_reviews_step(_answers[_i].key, _answers[_i].right, _answers[_i].quality);
    }
    fude_reviews_changes++;
    if(!fude_reviews_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "reviews: could not save the reviews (%s)", fude_reviews_path);
    }
}

// Returns the change counter, which goes up with every sitting's answers so a view knows when to look again.
u32 fude_reviews_revision(void) {
    return fude_reviews_changes;
}
