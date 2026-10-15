// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/codec.h"

// ===========================================================================
// See codec.h.
//
// Three layers, from the bottom: a bit writer and reader (each byte filled from its low bit), the Rice code of a
// channel's values in blocks, and the stroke, whose channels are turned into differences and written one after the
// other. Encoding and decoding walk the channels in the same order: x, y, then pressure and time when the stroke has
// them.
// ===========================================================================

#define FUDE_ZOOM_CODEC_BLOCK  32u   // values that share one k (the header's blocks of 32)
#define FUDE_ZOOM_CODEC_UNARY  15u   // ones before the escape to raw bits
#define FUDE_ZOOM_CODEC_K_BITS 5u    // bits that store a block's k (0 to 31; the encoder tries 0 to 30)

// --- bits --------------------------------------------------------------------------------

// Writes bits into a fude_bytes a byte at a time, each byte filled from its low bit.
typedef struct {
    fude_bytes* out;    // where the bytes are appended
    u64         acc;    // bits not yet written
    u32         bits;   // in acc, from its low end
} fude_zoom_bit_writer;

// Writes the low _n bits of _value, and every byte that fills.
RDE_INTERNAL void fude_zoom_bits_put(fude_zoom_bit_writer* _w, u32 _value, u32 _n) {
    // _n ≤ 32: acc never holds more than 39 bits before it is drained below 8.
    _w->acc  |= (u64)_value << _w->bits;
    _w->bits += _n;
    while(_w->bits >= 8u) {
        fude_put_u8(_w->out, (u8)(_w->acc & 0xFFu));
        _w->acc  >>= 8;
        _w->bits -= 8u;
    }
}

// Writes the bits left, padded with zeros to a whole byte, and empties the writer.
RDE_INTERNAL void fude_zoom_bits_flush(fude_zoom_bit_writer* _w) {
    if(_w->bits > 0) {
        fude_put_u8(_w->out, (u8)(_w->acc & 0xFFu));
    }
    _w->acc  = 0;
    _w->bits = 0;
}

// Reads back what a fude_zoom_bit_writer wrote, from data's size bytes. ok turns false, for good, when a read runs
// past the end.
typedef struct {
    const u8* data;
    u32       size;
    u32       pos;     // next byte
    u64       acc;     // bits read but not yet used, from its low end
    u32       bits;    // how many
    b8        ok;      // false once a read ran past the end
} fude_zoom_bit_reader;

// Reads _n bits (at most 32), the first written in the low end. Past the end of the data returns 0 and turns ok
// false.
RDE_INTERNAL u32 fude_zoom_bits_get(fude_zoom_bit_reader* _r, u32 _n) {
    while(_r->bits < _n) {
        if(_r->pos >= _r->size) {
            _r->ok = false;
            return 0;
        }
        _r->acc  |= (u64)_r->data[_r->pos++] << _r->bits;
        _r->bits += 8u;
    }
    const u32 _v = (u32)(_r->acc & (_n == 32u ? 0xFFFFFFFFull : ((1ull << _n) - 1ull)));
    _r->acc  >>= _n;
    _r->bits -= _n;
    return _v;
}

// --- values ---------------------------------------------------------------------------------

// Moves the sign into the low bit: 0, -1, 1, -2… become 0, 1, 2, 3…, so a value near 0 of either sign is a small
// number.
RDE_INTERNAL u32 fude_zoom_zigzag(i32 _v) {
    return ((u32)_v << 1) ^ (u32)(_v >> 31);
}

// Undoes fude_zoom_zigzag.
RDE_INTERNAL i32 fude_zoom_unzigzag(u32 _v) {
    return (i32)(_v >> 1) ^ -(i32)(_v & 1u);
}

// Returns how many bits fude_zoom_rice_put writes for _v with parameter _k.
RDE_INTERNAL u32 fude_zoom_rice_cost(u32 _v, u32 _k) {
    const u32 _q = _v >> _k;
    return _q < FUDE_ZOOM_CODEC_UNARY ? _q + 1u + _k : FUDE_ZOOM_CODEC_UNARY + 32u;
}

// Writes _v Rice-coded with parameter _k: its high part (_v >> _k) in unary, then its _k low bits. A high part of
// FUDE_ZOOM_CODEC_UNARY or more is written as that many ones and then _v in 32 raw bits.
RDE_INTERNAL void fude_zoom_rice_put(fude_zoom_bit_writer* _w, u32 _v, u32 _k) {
    const u32 _q = _v >> _k;
    if(_q < FUDE_ZOOM_CODEC_UNARY) {
        fude_zoom_bits_put(_w, (1u << _q) - 1u, _q + 1u);   // _q ones, then the zero that ends them
        if(_k > 0) {
            fude_zoom_bits_put(_w, _v & ((1u << _k) - 1u), _k);
        }
    } else {
        fude_zoom_bits_put(_w, (1u << FUDE_ZOOM_CODEC_UNARY) - 1u, FUDE_ZOOM_CODEC_UNARY);
        fude_zoom_bits_put(_w, _v, 32u);
    }
}

// Reads a value written by fude_zoom_rice_put.
RDE_INTERNAL u32 fude_zoom_rice_get(fude_zoom_bit_reader* _r, u32 _k) {
    u32 _q = 0;
    while(_q < FUDE_ZOOM_CODEC_UNARY && fude_zoom_bits_get(_r, 1u) == 1u) {
        _q++;
    }
    if(_q == FUDE_ZOOM_CODEC_UNARY) {
        return fude_zoom_bits_get(_r, 32u);
    }
    return (_q << _k) | (_k > 0 ? fude_zoom_bits_get(_r, _k) : 0u);
}

// Writes a channel's values, already zigzagged, in blocks of FUDE_ZOOM_CODEC_BLOCK: each block's k first, the one
// that makes it shortest, then its values.
RDE_INTERNAL void fude_zoom_channel_put(fude_zoom_bit_writer* _w, const u32* _v, u32 _count) {
    for(u32 _at = 0; _at < _count; _at += FUDE_ZOOM_CODEC_BLOCK) {
        const u32 _n     = _count - _at < FUDE_ZOOM_CODEC_BLOCK ? _count - _at : FUDE_ZOOM_CODEC_BLOCK;
        u32       _best  = 0;
        u32       _cost  = UINT32_MAX;
        for(u32 _k = 0; _k < 31u; _k++) {
            u32 _c = 0;
            for(u32 _i = 0; _i < _n; _i++) {
                _c += fude_zoom_rice_cost(_v[_at + _i], _k);
            }
            if(_c < _cost) {
                _cost = _c;
                _best = _k;
            }
        }
        fude_zoom_bits_put(_w, _best, FUDE_ZOOM_CODEC_K_BITS);
        for(u32 _i = 0; _i < _n; _i++) {
            fude_zoom_rice_put(_w, _v[_at + _i], _best);
        }
    }
}

// Reads _count values written by fude_zoom_channel_put, stopping at a block once ok is false.
RDE_INTERNAL void fude_zoom_channel_get(fude_zoom_bit_reader* _r, u32* _v, u32 _count) {
    for(u32 _at = 0; _at < _count && _r->ok; _at += FUDE_ZOOM_CODEC_BLOCK) {
        const u32 _n = _count - _at < FUDE_ZOOM_CODEC_BLOCK ? _count - _at : FUDE_ZOOM_CODEC_BLOCK;
        const u32 _k = fude_zoom_bits_get(_r, FUDE_ZOOM_CODEC_K_BITS);
        for(u32 _i = 0; _i < _n; _i++) {
            _v[_at + _i] = fude_zoom_rice_get(_r, _k);
        }
    }
}

// Writes the first (_order 1) or second (_order 2) differences of _in into _out, zigzagged. The first value is kept
// as it is, and the second as a first difference. Nothing overflows: coordinates are within ±2^28, so a second
// difference is within ±2^30.
RDE_INTERNAL void fude_zoom_differences(const i32* _in, u32* _out, u32 _count, u32 _order) {
    for(u32 _i = 0; _i < _count; _i++) {
        i32 _d = _in[_i];
        if(_i >= 1) {
            _d -= _in[_i - 1];
        }
        if(_order == 2u && _i >= 2) {
            _d -= _in[_i - 1] - _in[_i - 2];
        }
        _out[_i] = fude_zoom_zigzag(_d);
    }
}

// Undoes fude_zoom_differences: the values rebuilt from their zigzagged differences. The sums are unsigned on the way,
// so bytes that are not a stroke's (a damaged file) wrap instead of overflowing; a real stroke's never leave ±2^28.
RDE_INTERNAL void fude_zoom_sums(const u32* _in, i32* _out, u32 _count, u32 _order) {
    for(u32 _i = 0; _i < _count; _i++) {
        u32 _v = (u32)fude_zoom_unzigzag(_in[_i]);
        if(_i >= 1) {
            _v += (u32)_out[_i - 1];
        }
        if(_order == 2u && _i >= 2) {
            _v += (u32)_out[_i - 1] - (u32)_out[_i - 2];
        }
        _out[_i] = (i32)_v;
    }
}

// --- the stroke ----------------------------------------------------------------------------

// See the header. x, y and time are written as second differences, pressure as first.
void fude_zoom_codec_encode(fude_bytes* _out, const fude_zoom_qpoint* _points, u32 _count, u8 _channels) {
    if(_count == 0) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _in_arr  = rde_arr_new(sizeof(i32), _heap);
    rde_arr _cod_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_in_arr, _count);
    rde_arr_resize(&_cod_arr, _count);
    i32* _in  = (i32*)_in_arr.memory;   // (sized once: they stay put)
    u32* _cod = (u32*)_cod_arr.memory;

    fude_zoom_bit_writer _w = { .out = _out };
    for(u32 _ch = 0; _ch < 4u; _ch++) {
        u32 _order = 2u;
        if(_ch == 2u) {
            if(!(_channels & FUDE_ZOOM_CHANNEL_PRESSURE)) {
                continue;
            }
            _order = 1u;
        }
        if(_ch == 3u && !(_channels & FUDE_ZOOM_CHANNEL_TIME)) {
            continue;
        }
        for(u32 _i = 0; _i < _count; _i++) {
            const fude_zoom_qpoint* _p = &_points[_i];
            _in[_i] = _ch == 0u ? _p->x : _ch == 1u ? _p->y : _ch == 2u ? (i32)_p->pressure : (i32)_p->time;
        }
        fude_zoom_differences(_in, _cod, _count, _order);
        fude_zoom_channel_put(&_w, _cod, _count);
    }
    fude_zoom_bits_flush(&_w);

    rde_arr_free(&_in_arr);
    rde_arr_free(&_cod_arr);
}

// See the header. _points is zeroed first, so a channel the stroke does not have reads as 0.
b8 fude_zoom_codec_decode(const u8* _data, u32 _size, fude_zoom_qpoint* _points, u32 _count, u8 _channels) {
    if(_count == 0) {
        return true;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _val_arr = rde_arr_new(sizeof(i32), _heap);
    rde_arr _cod_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_val_arr, _count);
    rde_arr_resize(&_cod_arr, _count);
    i32* _val = (i32*)_val_arr.memory;   // (sized once: they stay put)
    u32* _cod = (u32*)_cod_arr.memory;
    rde_memset(_points, 0, (usize)_count * sizeof(fude_zoom_qpoint));

    fude_zoom_bit_reader _r = { .data = _data, .size = _size, .ok = true };
    for(u32 _ch = 0; _ch < 4u && _r.ok; _ch++) {
        u32 _order = 2u;
        if(_ch == 2u) {
            if(!(_channels & FUDE_ZOOM_CHANNEL_PRESSURE)) {
                continue;
            }
            _order = 1u;
        }
        if(_ch == 3u && !(_channels & FUDE_ZOOM_CHANNEL_TIME)) {
            continue;
        }
        fude_zoom_channel_get(&_r, _cod, _count);
        fude_zoom_sums(_cod, _val, _count, _order);
        for(u32 _i = 0; _i < _count; _i++) {
            fude_zoom_qpoint* _p = &_points[_i];
            if(_ch == 0u) {
                _p->x = _val[_i];
            } else if(_ch == 1u) {
                _p->y = _val[_i];
            } else if(_ch == 2u) {
                _p->pressure = (u16)_val[_i];
            } else {
                _p->time = (u32)_val[_i];
            }
        }
    }

    rde_arr_free(&_val_arr);
    rde_arr_free(&_cod_arr);
    return _r.ok;
}
