#include "examlog.h"
#include "kfile.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See examlog.h.
// ===========================================================================

#define KANA_EXAMLOG_VERSION     1u
#define KANA_EXAMLOG_KIND        KANA_TAG('E', 'X', 'A', 'M')
#define KANA_EXAMLOG_CHUNK       KANA_TAG('E', 'X', 'A', 'M')
#define KANA_EXAMLOG_ITEM_SIZE   24u

static rde_arr TYPE(kana_examlog_exam) kana_examlog_all;
static rde_arr TYPE(kana_examlog_item) kana_examlog_all_items;
static b8                              kana_examlog_ready = false;
static c8                              kana_examlog_path[RDE_MAX_PATH];
static u32                             kana_examlog_changes = 0;

RDE_INTERNAL void kana_examlog_ensure(void) {
    if(!kana_examlog_ready) {
        kana_examlog_all       = rde_arr_new(sizeof(kana_examlog_exam), rde_memory_allocator_get_default_std());
        kana_examlog_all_items = rde_arr_new(sizeof(kana_examlog_item), rde_memory_allocator_get_default_std());
        kana_examlog_ready     = true;
    }
}

// One exam's results into memory (the summary worked out from its items).
RDE_INTERNAL void kana_examlog_remember(u64 _time, u8 _source, const kana_examlog_item* _items, u32 _count) {
    kana_examlog_exam _exam = { .time = _time, .source = _source, .first_item = (u32)rde_arr_length(&kana_examlog_all_items), .item_count = _count };
    f32 _sum = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        rde_arr_add(&kana_examlog_all_items, &_items[_i]);
        _exam.correct += _items[_i].correct ? 1u : 0u;
        _sum          += _items[_i].score;
    }
    _exam.score = _count > 0 ? _sum / (f32)_count : 0.0f;
    rde_arr_add(&kana_examlog_all, &_exam);
}

void kana_examlog_open(const c8* _path) {
    kana_examlog_ensure();
    rde_arr_clear(&kana_examlog_all);
    rde_arr_clear(&kana_examlog_all_items);
    snprintf(kana_examlog_path, sizeof(kana_examlog_path), "%s", _path);
    kana_examlog_changes++;

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // no exams yet
        }
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_from, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_EXAMLOG_VERSION, KANA_EXAMLOG_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }

    static kana_examlog_item _items[256];
    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_EXAMLOG_CHUNK) {
            continue;
        }
        const u32 _lo     = kana_get_u32(&_chunk);
        const u32 _hi     = kana_get_u32(&_chunk);
        const u8  _source = kana_get_u8(&_chunk);
        kana_get_u8(&_chunk); kana_get_u8(&_chunk); kana_get_u8(&_chunk);
        const u32 _count  = kana_get_u32(&_chunk);
        const u32 _record = kana_get_u32(&_chunk);
        if(!_chunk.ok || _record < 20u || _count > 256u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            continue;   // a damaged exam: the others still count
        }
        for(u32 _i = 0; _i < _count; _i++) {
            kana_reader _e = kana_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            _items[_i].codepoint = kana_get_u32(&_e);
            _items[_i].correct   = kana_get_u8(&_e) != 0;
            kana_get_u8(&_e); kana_get_u8(&_e); kana_get_u8(&_e);
            _items[_i].score     = kana_get_f32(&_e);
            _items[_i].quality   = kana_get_f32(&_e);
            _items[_i].read_as   = kana_get_u32(&_e);
        }
        kana_examlog_remember((u64)_lo | ((u64)_hi << 32), _source, _items, _count);
    }
    kana_file_free(_data);
}

void kana_examlog_close(void) {
    if(kana_examlog_ready) {
        rde_arr_free(&kana_examlog_all);
        rde_arr_free(&kana_examlog_all_items);
        kana_examlog_ready = false;
    }
    kana_examlog_path[0] = 0;
    kana_examlog_changes++;
}

RDE_INTERNAL u16 kana_examlog_fraction(f32 _v, f32 _units) {
    const f32 _f = _units > 0.0f ? rde_math_clamp_f32(_v / _units, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

b8 kana_examlog_add(u64 _time, u8 _source, const kana_examlog_item* _items, u32 _count, const kana_ink* const* _drawings, f32 _units) {
    kana_examlog_ensure();
    _count = _count > 256u ? 256u : _count;
    kana_examlog_remember(_time, _source, _items, _count);
    kana_examlog_changes++;
    if(kana_examlog_path[0] == 0) {
        return true;   // in memory only
    }

    // What is there already, kept byte for byte: the log is only ever added to.
    u32 _old_size = 0;
    u8* _old      = rde_file_exists(kana_examlog_path) ? kana_file_read(kana_examlog_path, &_old_size) : NULL;
    if(_old != NULL) {
        kana_reader _r = kana_reader_make(_old, _old_size);
        if(!kana_read_header(&_r, KANA_EXAMLOG_VERSION, KANA_EXAMLOG_KIND)) {
            kana_file_free(_old);   // damaged: set aside, not lost nor overwritten
            _old = NULL;
            kana_file_set_aside(kana_examlog_path);
        }
    }

    kana_bytes _b = kana_bytes_new((_old != NULL ? _old_size : 0u) + 1024u + _count * 64u);
    if(_old != NULL) {
        kana_put_data(&_b, _old, _old_size);
        kana_file_free(_old);
    } else {
        kana_put_header(&_b, KANA_EXAMLOG_VERSION, KANA_EXAMLOG_KIND);
    }

    const u32 _chunk = kana_chunk_begin(&_b, KANA_EXAMLOG_CHUNK);
    kana_put_u32(&_b, (u32)(_time & 0xFFFFFFFFu));
    kana_put_u32(&_b, (u32)(_time >> 32));
    kana_put_u8(&_b, _source);
    kana_put_u8(&_b, 0u); kana_put_u8(&_b, 0u); kana_put_u8(&_b, 0u);
    kana_put_u32(&_b, _count);
    kana_put_u32(&_b, KANA_EXAMLOG_ITEM_SIZE);
    for(u32 _i = 0; _i < _count; _i++) {
        kana_put_u32(&_b, _items[_i].codepoint);
        kana_put_u8(&_b, _items[_i].correct ? 1u : 0u);
        kana_put_u8(&_b, 0u); kana_put_u8(&_b, 0u); kana_put_u8(&_b, 0u);
        kana_put_f32(&_b, _items[_i].score);
        kana_put_f32(&_b, _items[_i].quality);
        kana_put_u32(&_b, _items[_i].read_as);
        kana_put_u32(&_b, 0u);
    }
    for(u32 _i = 0; _i < _count; _i++) {
        const kana_ink* _ink = _drawings != NULL ? _drawings[_i] : NULL;
        u32 _strokes = 0;
        for(u32 _s = 0; _ink != NULL && _s < kana_ink_stroke_count(_ink); _s++) {
            _strokes += kana_ink_stroke_at(_ink, _s)->alive ? 1u : 0u;
        }
        kana_put_u16(&_b, (u16)(_strokes < 65535u ? _strokes : 65535u));
        for(u32 _s = 0; _ink != NULL && _s < kana_ink_stroke_count(_ink); _s++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
            if(!_stroke->alive) {
                continue;
            }
            const u32 _n = _stroke->point_count < 65535u ? _stroke->point_count : 65535u;
            const kana_ink_point* _p = kana_ink_stroke_points(_ink, _stroke);
            kana_put_u16(&_b, (u16)_n);
            for(u32 _k = 0; _k < _n; _k++) {
                kana_put_u16(&_b, kana_examlog_fraction(_p[_k].position.x, _units));
                kana_put_u16(&_b, kana_examlog_fraction(_p[_k].position.y, _units));
            }
        }
    }
    kana_chunk_end(&_b, _chunk);
    return kana_bytes_write_and_free(&_b, kana_examlog_path, NULL);
}

u32 kana_examlog_count(void) {
    return kana_examlog_ready ? (u32)rde_arr_length(&kana_examlog_all) : 0u;
}

const kana_examlog_exam* kana_examlog_exams(void) {
    return kana_examlog_ready ? (const kana_examlog_exam*)kana_examlog_all.memory : NULL;
}

const kana_examlog_item* kana_examlog_items(void) {
    return kana_examlog_ready ? (const kana_examlog_item*)kana_examlog_all_items.memory : NULL;
}

u32 kana_examlog_streak(u32 _codepoint) {
    const kana_examlog_exam* _exams = kana_examlog_exams();
    const kana_examlog_item* _items = kana_examlog_items();
    u32 _streak = 0;
    for(u32 _e = kana_examlog_count(); _e-- > 0;) {
        for(u32 _i = 0; _i < _exams[_e].item_count; _i++) {
            const kana_examlog_item* _it = &_items[_exams[_e].first_item + _i];
            if(_it->codepoint != _codepoint) {
                continue;
            }
            if(!_it->correct) {
                return _streak;
            }
            _streak++;
            break;   // once per exam
        }
    }
    return _streak;
}

u32 kana_examlog_revision(void) {
    return kana_examlog_changes;
}
