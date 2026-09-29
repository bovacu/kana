#include "history.h"
#include "kfile.h"
#include "save.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See history.h.
// ===========================================================================

#define KANA_HISTORY_KIND          KANA_TAG('P', 'R', 'A', 'C')
#define KANA_HISTORY_CHUNK_SESSION KANA_TAG('S', 'E', 'S', 'S')
#define KANA_HISTORY_SQUARE_SIZE   20u     // the fixed part of a square record

const c8* kana_history_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };

    if(_dir[0] == 0) {
        snprintf(_dir, sizeof(_dir), "%spractice/", kana_save_dir());
        c8 _probe[RDE_MAX_PATH];
        snprintf(_probe, sizeof(_probe), "%sx.kana", _dir);
        rde_file_create_missing_dirs(_probe);
    }

    return _dir;
}

RDE_INTERNAL void kana_history_path(u32 _codepoint, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%04X.kana", kana_history_dir(), _codepoint);
}

RDE_INTERNAL u16 kana_history_fraction(f32 _v, f32 _size) {
    const f32 _f = _size > 0.0f ? rde_math_clamp_f32(_v / _size, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

RDE_INTERNAL void kana_history_put_u64(kana_bytes* _b, u64 _v) {
    kana_put_u32(_b, (u32)(_v & 0xFFFFFFFFu));
    kana_put_u32(_b, (u32)(_v >> 32));
}

RDE_INTERNAL u64 kana_history_get_u64(kana_reader* _r) {
    const u64 _lo = kana_get_u32(_r);
    const u64 _hi = kana_get_u32(_r);
    return _lo | (_hi << 32);
}

b8 kana_history_save(u32 _codepoint, u64 _time, u32 _squares, const kana_score* _scores,
                     const kana_ink* const* _drawings, f32 _square_size) {
    c8 _path[RDE_MAX_PATH];
    kana_history_path(_codepoint, _path, sizeof(_path));

    // What is there already, kept byte for byte: history is only ever added to.
    u32 _old_size = 0;
    u8* _old      = rde_file_exists(_path) ? kana_file_read(_path, &_old_size) : NULL;
    if(_old != NULL) {
        kana_reader _r = kana_reader_make(_old, _old_size);
        if(!kana_read_header(&_r, KANA_HISTORY_VERSION, KANA_HISTORY_KIND)) {
            // Damaged: set it aside rather than lose today's session or overwrite it.
            kana_file_free(_old);
            _old = NULL;
            kana_file_set_aside(_path);
        }
    }

    kana_bytes _b = kana_bytes_new(_old != NULL ? _old_size + 4096u : 4096u);
    if(_old != NULL) {
        kana_put_data(&_b, _old, _old_size);
        kana_file_free(_old);
    } else {
        kana_put_header(&_b, KANA_HISTORY_VERSION, KANA_HISTORY_KIND);
    }

    // The average over the squares that were scored.
    f32 _sum    = 0.0f;
    u32 _scored = 0;
    for(u32 _i = 0; _i < _squares; _i++) {
        if(!_scores[_i].empty) {
            _sum += _scores[_i].score;
            _scored++;
        }
    }

    const u32 _chunk = kana_chunk_begin(&_b, KANA_HISTORY_CHUNK_SESSION);
    kana_history_put_u64(&_b, _time);
    kana_put_u32(&_b, _codepoint);
    kana_put_f32(&_b, _scored > 0 ? _sum / (f32)_scored : 0.0f);
    kana_put_u32(&_b, _squares);
    kana_put_u32(&_b, KANA_HISTORY_SQUARE_SIZE);

    for(u32 _i = 0; _i < _squares; _i++) {
        const kana_score* _s = &_scores[_i];
        kana_put_u8(&_b, _s->empty ? 0u : 1u);
        kana_put_f32(&_b, _s->score);
        kana_put_f32(&_b, _s->shape);
        kana_put_u8(&_b, _s->drawn);
        kana_put_u8(&_b, _s->expected);
        kana_put_u8(&_b, _s->missing);
        kana_put_u8(&_b, _s->extra);
        kana_put_u8(&_b, _s->misplaced);
        kana_put_u8(&_b, _s->reversed);
        kana_put_u8(&_b, _s->first_reversed);
        kana_put_u8(&_b, _s->swap_a);
        kana_put_u8(&_b, _s->swap_b);
        kana_put_u8(&_b, _s->worst);
        kana_put_u8(&_b, 0u);   // reserved: 1 + 4 + 4 + 10 + 1 = the 20-byte record

        // The drawing, relative to its square.
        const kana_ink* _ink   = _drawings[_i];
        u32             _count = 0;
        for(u32 _k = 0; _k < kana_ink_stroke_count(_ink); _k++) {
            _count += kana_ink_stroke_at(_ink, _k)->alive ? 1u : 0u;
        }
        kana_put_u32(&_b, _count);
        for(u32 _k = 0; _k < kana_ink_stroke_count(_ink); _k++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _k);
            if(!_stroke->alive) {
                continue;
            }
            const kana_ink_point* _p = kana_ink_stroke_points(_ink, _stroke);
            kana_put_u32(&_b, _stroke->point_count);
            for(u32 _q = 0; _q < _stroke->point_count; _q++) {
                kana_put_u16(&_b, kana_history_fraction(_p[_q].position.x, _square_size));
                kana_put_u16(&_b, kana_history_fraction(_p[_q].position.y, _square_size));
                kana_put_f32(&_b, _p[_q].time);
            }
        }
    }
    kana_chunk_end(&_b, _chunk);

    return kana_bytes_write_and_free(&_b, _path, NULL);
}

b8 kana_history_summarize(u32 _codepoint, kana_history_summary* _out) {
    memset(_out, 0, sizeof(*_out));

    c8 _path[RDE_MAX_PATH];
    kana_history_path(_codepoint, _path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        return false;
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_path, &_size);
    kana_reader _r = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_HISTORY_VERSION, KANA_HISTORY_KIND)) {
        kana_file_free(_data);
        return false;
    }

    u32         _tag;
    kana_reader _chunk;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != KANA_HISTORY_CHUNK_SESSION) {
            continue;
        }
        const u64 _time    = kana_history_get_u64(&_chunk);
        const u32 _cp      = kana_get_u32(&_chunk);
        const f32 _average = kana_get_f32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint) {
            continue;
        }

        if(_out->sessions == 0) {
            _out->first = _average;
        }
        _out->sessions++;
        _out->last      = _average;
        _out->last_time = _time;
        _out->best      = _average > _out->best ? _average : _out->best;
    }

    kana_file_free(_data);
    return _out->sessions > 0;
}
