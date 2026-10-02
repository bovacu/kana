#include "review.h"
#include "kfile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See review.h. Few records (the characters being learnt): an array sorted by
// code point, searched by bisection, saved whole after each sitting's answers.
// ===========================================================================

#define KANA_REVIEWS_VERSION     1u
#define KANA_REVIEWS_KIND        KANA_TAG('R', 'E', 'V', 'W')
#define KANA_REVIEWS_CHUNK       KANA_TAG('R', 'V', 'W', 'S')
#define KANA_REVIEWS_RECORD_SIZE 20u
#define KANA_REVIEWS_EASE        250u   // a new character's ease, in hundredths (2.5)
#define KANA_REVIEWS_EASE_MIN    130u
#define KANA_REVIEWS_EASE_MAX    300u
#define KANA_REVIEW_KIND(_key)   ((_key) > 0x10FFFFu ? 1u : 0u)   // 0 a character (a code point), 1 a word (vocab.h's keys)

typedef struct {
    u32 codepoint;
    u16 interval;   // days, the last step
    u16 ease;       // hundredths
    u32 due;        // day number
    u16 reps;       // right in a row, in time
    u16 lapses;     // wrong, ever
    u32 first;      // the day of its first review
} kana_review;

static rde_arr TYPE(kana_review) kana_reviews_all;
static b8                        kana_reviews_ready = false;
static c8                        kana_reviews_path[RDE_MAX_PATH];
static u32                       kana_reviews_changes = 0;

RDE_INTERNAL void kana_reviews_ensure(void) {
    if(!kana_reviews_ready) {
        kana_reviews_all   = rde_arr_new(sizeof(kana_review), rde_memory_allocator_get_default_std());
        kana_reviews_ready = true;
    }
}

RDE_INTERNAL u32 kana_reviews_find(u32 _codepoint, b8* _found) {
    const kana_review* _r  = (const kana_review*)kana_reviews_all.memory;
    u32                _lo = 0;
    u32                _hi = (u32)rde_arr_length(&kana_reviews_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_r[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&kana_reviews_all) && _r[_lo].codepoint == _codepoint;
    return _lo;
}

RDE_INTERNAL b8 kana_reviews_save(void) {
    if(kana_reviews_path[0] == 0) {
        return true;   // in memory only
    }
    const u32  _n = (u32)rde_arr_length(&kana_reviews_all);
    kana_bytes _b = kana_bytes_new(KANA_FILE_HEADER_SIZE + 32u + _n * KANA_REVIEWS_RECORD_SIZE + 1u);
    kana_put_header(&_b, KANA_REVIEWS_VERSION, KANA_REVIEWS_KIND);
    const u32 _chunk = kana_chunk_begin(&_b, KANA_REVIEWS_CHUNK);
    kana_put_u32(&_b, _n);
    kana_put_u32(&_b, KANA_REVIEWS_RECORD_SIZE);
    const kana_review* _r = (const kana_review*)kana_reviews_all.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_put_u32(&_b, _r[_i].codepoint);
        kana_put_u16(&_b, _r[_i].interval);
        kana_put_u16(&_b, _r[_i].ease);
        kana_put_u32(&_b, _r[_i].due);
        kana_put_u16(&_b, _r[_i].reps);
        kana_put_u16(&_b, _r[_i].lapses);
        kana_put_u32(&_b, _r[_i].first);
    }
    kana_chunk_end(&_b, _chunk);
    return kana_bytes_write_and_free(&_b, kana_reviews_path, NULL);
}

void kana_reviews_open(const c8* _path) {
    kana_reviews_ensure();
    rde_arr_clear(&kana_reviews_all);
    snprintf(kana_reviews_path, sizeof(kana_reviews_path), "%s", _path != NULL ? _path : "");
    kana_reviews_changes++;
    if(_path == NULL) {
        return;
    }
    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // nothing reviewed yet
        }
    }
    u32         _size = 0;
    u8*         _data = kana_file_read(_from, &_size);
    kana_reader _r    = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_REVIEWS_VERSION, KANA_REVIEWS_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }
    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_REVIEWS_CHUNK) {
            continue;
        }
        const u32 _count  = kana_get_u32(&_chunk);
        const u32 _record = kana_get_u32(&_chunk);
        if(!_chunk.ok || _record < KANA_REVIEWS_RECORD_SIZE || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            break;
        }
        for(u32 _i = 0; _i < _count; _i++) {
            kana_reader _e = kana_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            kana_review _v;
            _v.codepoint = kana_get_u32(&_e);
            _v.interval  = kana_get_u16(&_e);
            _v.ease      = kana_get_u16(&_e);
            _v.due       = kana_get_u32(&_e);
            _v.reps      = kana_get_u16(&_e);
            _v.lapses    = kana_get_u16(&_e);
            _v.first     = kana_get_u32(&_e);
            b8 _found;
            const u32 _at = kana_reviews_find(_v.codepoint, &_found);
            if(_e.ok && !_found) {
                rde_arr_insert(&kana_reviews_all, _at, &_v);
            }
        }
    }
    kana_file_free(_data);
}

void kana_reviews_close(void) {
    if(kana_reviews_ready) {
        rde_arr_free(&kana_reviews_all);
        kana_reviews_ready = false;
    }
    kana_reviews_path[0] = 0;
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's).
RDE_INTERNAL u32 kana_reviews_days(i32 _y, u32 _m, u32 _d) {
    _y -= _m <= 2u ? 1 : 0;
    const i32 _era = (_y >= 0 ? _y : _y - 399) / 400;
    const u32 _yoe = (u32)(_y - _era * 400);
    const u32 _doy = (153u * (_m > 2u ? _m - 3u : _m + 9u) + 2u) / 5u + _d - 1u;
    const u32 _doe = _yoe * 365u + _yoe / 4u - _yoe / 100u + _doy;
    return (u32)(_era * 146097 + (i32)_doe - 719468);
}

#if defined(KANA_TESTS)
u32 kana_reviews_fake_today = 0;   // tests only: the day it is (0: the real one)
#endif

u32 kana_reviews_today(void) {
#if defined(KANA_TESTS)
    if(kana_reviews_fake_today != 0) {
        return kana_reviews_fake_today;
    }
#endif
    const time_t     _now = time(NULL);
    const struct tm* _tm  = localtime(&_now);
    if(_tm == NULL) {
        return (u32)(_now / 86400);
    }
    return kana_reviews_days(_tm->tm_year + 1900, (u32)_tm->tm_mon + 1u, (u32)_tm->tm_mday);
}

u32 kana_reviews_due_day(u32 _codepoint) {
    kana_reviews_ensure();
    b8        _found;
    const u32 _at = kana_reviews_find(_codepoint, &_found);
    return _found ? ((const kana_review*)kana_reviews_all.memory)[_at].due : 0u;
}

// Longest waiting first; at the same day, by code point.
RDE_INTERNAL int kana_reviews_by_due(const void* _a, const void* _b) {
    const u32* _x = (const u32*)_a;
    const u32* _y = (const u32*)_b;
    if(_x[1] != _y[1]) {
        return _x[1] < _y[1] ? -1 : 1;
    }
    return _x[0] < _y[0] ? -1 : (_x[0] > _y[0] ? 1 : 0);
}

u32 kana_reviews_due(const u32* _candidates, u32 _count, u32* _out, u32 _max) {
    kana_reviews_ensure();
    const u32 _today = kana_reviews_today();
    // New ones today has room for: the allowance less those first reviewed today
    // — the characters' and the words' each their own.
    u32 _new_today[2] = { 0u, 0u };
    const kana_review* _all = (const kana_review*)kana_reviews_all.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&kana_reviews_all); _i++) {
        _new_today[KANA_REVIEW_KIND(_all[_i].codepoint)] += _all[_i].first == _today ? 1u : 0u;
    }
    u32 _room[2];
    for(u32 _k = 0; _k < 2u; _k++) {
        _room[_k] = _new_today[_k] < KANA_REVIEW_NEW_PER_DAY ? KANA_REVIEW_NEW_PER_DAY - _new_today[_k] : 0u;
    }

    // The due, the longest waiting first.
    typedef struct { u32 codepoint; u32 due; } kana_review_due;
    kana_review_due* _due = _count > 0 ? (kana_review_due*)malloc(sizeof(kana_review_due) * _count) : NULL;
    u32              _nd  = 0;
    for(u32 _i = 0; _due != NULL && _i < _count; _i++) {
        b8        _found;
        const u32 _at = kana_reviews_find(_candidates[_i], &_found);
        if(_found && _all[_at].due <= _today) {
            _due[_nd++] = (kana_review_due){ _candidates[_i], _all[_at].due };
        }
    }
    if(_nd > 1u) {
        qsort(_due, _nd, sizeof(kana_review_due), kana_reviews_by_due);
    }
    u32 _n = 0;
    for(u32 _i = 0; _i < _nd && _n < _max && _n < KANA_REVIEW_SESSION; _i++) {
        _out[_n++] = _due[_i].codepoint;
    }
    free(_due);
    // Then the new ones, in the order given.
    for(u32 _i = 0; _i < _count && _n < _max && _n < KANA_REVIEW_SESSION; _i++) {
        b8        _found;
        const u32 _kind = KANA_REVIEW_KIND(_candidates[_i]);
        kana_reviews_find(_candidates[_i], &_found);
        if(!_found && _room[_kind] > 0u) {
            _out[_n++] = _candidates[_i];
            _room[_kind]--;
        }
    }
    return _n;
}

void kana_reviews_answer(u32 _codepoint, b8 _right, f32 _quality) {
    kana_reviews_ensure();
    const u32 _today = kana_reviews_today();
    b8        _found;
    const u32 _at = kana_reviews_find(_codepoint, &_found);
    if(!_found) {
        const kana_review _fresh = { _codepoint, 0u, KANA_REVIEWS_EASE, _today, 0u, 0u, _today };
        rde_arr_insert(&kana_reviews_all, _at, &_fresh);
    }
    kana_review* _v = &((kana_review*)kana_reviews_all.memory)[_at];
    if(_right) {
        if(_v->due > _today) {
            return;   // early: the step was not waited for — nothing changes
        }
        _v->reps++;
        u32 _step = _v->reps == 1u ? 1u : _v->reps == 2u ? 3u : (u32)lroundf((f32)_v->interval * (f32)_v->ease / 100.0f);
        _step = _step > (u32)_v->interval ? _step : (u32)_v->interval + 1u;
        _v->interval = (u16)(_step < 3650u ? _step : 3650u);
        if(_quality >= 0.8f && _v->ease + 10u <= KANA_REVIEWS_EASE_MAX) {
            _v->ease += 10u;
        } else if(_quality < 0.5f && _v->ease >= KANA_REVIEWS_EASE_MIN + 10u) {
            _v->ease -= 10u;
        }
        _v->due = _today + _v->interval;
    } else {
        _v->lapses++;
        _v->reps     = 0u;
        _v->interval = 0u;   // the steps start over (the next right one is a day again)
        _v->ease     = _v->ease >= KANA_REVIEWS_EASE_MIN + 20u ? (u16)(_v->ease - 20u) : (u16)KANA_REVIEWS_EASE_MIN;
        _v->due      = _today + 1u;
    }
    kana_reviews_changes++;
    if(!kana_reviews_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not save the reviews (%s)", kana_reviews_path);
    }
}

u32 kana_reviews_revision(void) {
    return kana_reviews_changes;
}
