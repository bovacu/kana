#include "drawing/base/utf8.h"

// ===========================================================================
// See utf8.h.
// ===========================================================================

u32 fude_utf8_next(const c8** _s) {
    const u8* _u = (const u8*)*_s;

    if(_u[0] == 0) {
        return 0;
    }
    if(_u[0] < 0x80u) {
        *_s += 1;
        return _u[0];
    }
    if((_u[0] & 0xE0u) == 0xC0u && _u[1] != 0) {
        *_s += 2;
        return ((u32)(_u[0] & 0x1Fu) << 6) | (u32)(_u[1] & 0x3Fu);
    }
    if((_u[0] & 0xF0u) == 0xE0u && _u[1] != 0 && _u[2] != 0) {
        *_s += 3;
        return ((u32)(_u[0] & 0x0Fu) << 12) | ((u32)(_u[1] & 0x3Fu) << 6) | (u32)(_u[2] & 0x3Fu);
    }
    if((_u[0] & 0xF8u) == 0xF0u && _u[1] != 0 && _u[2] != 0 && _u[3] != 0) {
        *_s += 4;
        return ((u32)(_u[0] & 0x07u) << 18) | ((u32)(_u[1] & 0x3Fu) << 12) | ((u32)(_u[2] & 0x3Fu) << 6) | (u32)(_u[3] & 0x3Fu);
    }

    *_s += 1;          // malformed: skip the byte
    return 0xFFFDu;
}

void fude_utf8_put(u32 _cp, c8* _out) {
    if(_cp < 0x80u) {
        _out[0] = (c8)_cp;
        _out[1] = 0;
    } else if(_cp < 0x800u) {
        _out[0] = (c8)(0xC0u | (_cp >> 6));
        _out[1] = (c8)(0x80u | (_cp & 0x3Fu));
        _out[2] = 0;
    } else if(_cp < 0x10000u) {
        _out[0] = (c8)(0xE0u | (_cp >> 12));
        _out[1] = (c8)(0x80u | ((_cp >> 6) & 0x3Fu));
        _out[2] = (c8)(0x80u | (_cp & 0x3Fu));
        _out[3] = 0;
    } else {
        _out[0] = (c8)(0xF0u | (_cp >> 18));
        _out[1] = (c8)(0x80u | ((_cp >> 12) & 0x3Fu));
        _out[2] = (c8)(0x80u | ((_cp >> 6) & 0x3Fu));
        _out[3] = (c8)(0x80u | (_cp & 0x3Fu));
        _out[4] = 0;
    }
}
