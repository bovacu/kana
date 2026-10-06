// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/video.h"

// ===========================================================================
// See video.h. Here: the colour conversion every encoder side may use, and
// (neither Apple's nor Android's) no video at all.
// ===========================================================================

void fude_zoom_video_i420(const u8* _rgba, u32 _width, u32 _height, u32 _stride, u8* _out) {
    u8* _y = _out;
    u8* _u = _out + (usize)_width * _height;
    u8* _v = _u + ((usize)_width / 2u) * (_height / 2u);
    for(u32 _row = 0; _row < _height; _row++) {
        const u8* _p = _rgba + (usize)_row * _stride;
        u8*       _o = _y + (usize)_row * _width;
        for(u32 _x = 0; _x < _width; _x++) {
            const i32 _r = _p[4u * _x], _g = _p[4u * _x + 1u], _b = _p[4u * _x + 2u];
            _o[_x] = (u8)(((66 * _r + 129 * _g + 25 * _b + 128) >> 8) + 16);
        }
    }
    // Each 2 × 2 block's colour, its four pixels' average.
    for(u32 _row = 0; _row + 1u < _height; _row += 2u) {
        const u8* _p0 = _rgba + (usize)_row * _stride;
        const u8* _p1 = _p0 + _stride;
        for(u32 _x = 0; _x + 1u < _width; _x += 2u) {
            const i32 _r = (_p0[4u * _x] + _p0[4u * _x + 4u] + _p1[4u * _x] + _p1[4u * _x + 4u] + 2) >> 2;
            const i32 _g = (_p0[4u * _x + 1u] + _p0[4u * _x + 5u] + _p1[4u * _x + 1u] + _p1[4u * _x + 5u] + 2) >> 2;
            const i32 _b = (_p0[4u * _x + 2u] + _p0[4u * _x + 6u] + _p1[4u * _x + 2u] + _p1[4u * _x + 6u] + 2) >> 2;
            const usize _at = (usize)(_row / 2u) * (_width / 2u) + _x / 2u;
            _u[_at] = (u8)(((-38 * _r - 74 * _g + 112 * _b + 128) >> 8) + 128);
            _v[_at] = (u8)(((112 * _r - 94 * _g - 18 * _b + 128) >> 8) + 128);
        }
    }
}

#if !defined(__APPLE__) && !defined(RDE_PLATFORM_ANDROID)

b8 fude_zoom_video_available(void) {
    return false;
}

fude_zoom_video* fude_zoom_video_open(const c8* _path, u32 _width, u32 _height, u32 _fps) {
    RDE_UNUSED(_path); RDE_UNUSED(_width); RDE_UNUSED(_height); RDE_UNUSED(_fps);
    return NULL;
}

b8 fude_zoom_video_add(fude_zoom_video* _v, const u8* _rgba, u32 _stride) {
    RDE_UNUSED(_v); RDE_UNUSED(_rgba); RDE_UNUSED(_stride);
    return false;
}

b8 fude_zoom_video_close(fude_zoom_video* _v, b8 _keep) {
    RDE_UNUSED(_v); RDE_UNUSED(_keep);
    return false;
}

#endif
