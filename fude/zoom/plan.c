// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/plan.h"
#include "zoom/symbol.h"
#include <math.h>
#include <string.h>

#define FZP_PI 3.14159265358979323846

typedef enum {
    FZP_DOOR = 0, FZP_DOUBLE_DOOR, FZP_SLIDING, FZP_WINDOW, FZP_OPENING,
    FZP_BED1, FZP_BED2, FZP_SOFA, FZP_ARMCHAIR, FZP_TABLE, FZP_ROUND_TABLE, FZP_CHAIR, FZP_DESK, FZP_WARDROBE,
    FZP_COUNTER, FZP_SINK, FZP_STOVE, FZP_FRIDGE, FZP_WASHER, FZP_TOILET, FZP_BATHTUB, FZP_SHOWER, FZP_BASIN, FZP_STAIRS,
    FZP_SOCKET, FZP_SOCKET2, FZP_SWITCH, FZP_SWITCH2, FZP_DIMMER, FZP_BELL, FZP_LIGHT, FZP_WALL_LIGHT, FZP_TUBE,
    FZP_BOARD, FZP_JUNCTION, FZP_TV, FZP_DATA, FZP_SMOKE, FZP_THERMOSTAT, FZP_FAN, FZP_METER
} FZP_LOOK_;

RDE_INTERNAL const fude_zoom_plan_part FZP_PARTS[] = {
    { "door 70",         FZP_DOOR,        70, 80, true, "" },
    { "door 80",         FZP_DOOR,        80, 90, true, "" },
    { "door 90",         FZP_DOOR,        90, 100, true, "" },
    { "double door",     FZP_DOUBLE_DOOR, 140, 80, true, "" },
    { "sliding door",    FZP_SLIDING,     160, 10, true, "" },
    { "window 60",       FZP_WINDOW,      60, 10, true, "" },
    { "window 100",      FZP_WINDOW,      100, 10, true, "" },
    { "window 120",      FZP_WINDOW,      120, 10, true, "" },
    { "window 150",      FZP_WINDOW,      150, 10, true, "" },
    { "window 200",      FZP_WINDOW,      200, 10, true, "" },
    { "opening 90",      FZP_OPENING,     90, 10, true, "" },
    { "single bed",      FZP_BED1,        90, 190, true, "" },
    { "double bed",      FZP_BED2,        140, 190, true, "" },
    { "queen bed",       FZP_BED2,        160, 200, true, "" },
    { "sofa",            FZP_SOFA,        200, 90, true, "" },
    { "armchair",        FZP_ARMCHAIR,    80, 80, true, "" },
    { "dining table",    FZP_TABLE,       160, 90, true, "" },
    { "round table",     FZP_ROUND_TABLE, 100, 100, true, "" },
    { "chair",           FZP_CHAIR,       45, 50, true, "" },
    { "desk",            FZP_DESK,        140, 70, true, "" },
    { "wardrobe",        FZP_WARDROBE,    120, 60, true, "" },
    { "kitchen counter", FZP_COUNTER,     240, 60, true, "" },
    { "kitchen sink",    FZP_SINK,        80, 50, true, "" },
    { "hob",             FZP_STOVE,       60, 60, true, "" },
    { "fridge",          FZP_FRIDGE,      60, 65, true, "" },
    { "washing machine", FZP_WASHER,      60, 60, true, "" },
    { "toilet",          FZP_TOILET,      40, 65, true, "" },
    { "bathtub",         FZP_BATHTUB,     170, 75, true, "" },
    { "shower",          FZP_SHOWER,      90, 90, true, "" },
    { "washbasin",       FZP_BASIN,       60, 45, true, "" },
    { "stairs",          FZP_STAIRS,      100, 280, true, "" },
    { "socket",          FZP_SOCKET,      40, 30, false, "" },
    { "double socket",   FZP_SOCKET2,     50, 30, false, "" },
    { "light switch",    FZP_SWITCH,      36, 36, false, "" },
    { "two-way switch",  FZP_SWITCH2,     36, 36, false, "" },
    { "dimmer",          FZP_DIMMER,      36, 36, false, "" },
    { "bell push",       FZP_BELL,        30, 30, false, "" },
    { "ceiling light",   FZP_LIGHT,       40, 40, false, "" },
    { "wall light",      FZP_WALL_LIGHT,  40, 40, false, "" },
    { "tube light",      FZP_TUBE,        80, 20, false, "" },
    { "consumer unit",   FZP_BOARD,       60, 30, false, "" },
    { "junction box",    FZP_JUNCTION,    24, 24, false, "" },
    { "TV point",        FZP_TV,          36, 30, false, "TV" },
    { "data point",      FZP_DATA,        36, 30, false, "" },
    { "smoke detector",  FZP_SMOKE,       36, 36, false, "" },
    { "thermostat",      FZP_THERMOSTAT,  36, 36, false, "" },
    { "ceiling fan",     FZP_FAN,         50, 50, false, "" },
    { "meter",           FZP_METER,       44, 30, false, "kWh" },
};
#define FZP_N ((u32)(sizeof(FZP_PARTS) / sizeof(FZP_PARTS[0])))

const fude_zoom_plan_part* fude_zoom_plan_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < FZP_N; _i++) {
        if(strcmp(FZP_PARTS[_i].id, _id) == 0) {
            return &FZP_PARTS[_i];
        }
    }
    return NULL;
}

const fude_zoom_plan_part* fude_zoom_plan_of_kind(u32 _kind) {
    static const fude_zoom_plan_part* _cache[1024];
    static u8 _known[1024];
    const fude_zoom_symbol_info* _info;
    if(_kind >= 1024u) {
        _info = fude_zoom_symbol_info_of(_kind);
        return _info != NULL ? fude_zoom_plan_find(_info->id) : NULL;
    }
    if(!_known[_kind]) {
        _info = fude_zoom_symbol_info_of(_kind);
        _cache[_kind] = _info != NULL ? fude_zoom_plan_find(_info->id) : NULL;
        _known[_kind] = 1u;
    }
    return _cache[_kind];
}

// --- drawn -------------------------------------------------------------------------------------------

typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    f64      k;          // own units a centimetre (or a point)
    u32      segments;
    u32      first;
} fzp;

RDE_INTERNAL void fzp_pt(fzp* _d, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_d->points, (any)&_p);
}
RDE_INTERNAL void fzp_begin(fzp* _d) {
    _d->first = (u32)rde_arr_length(_d->points);
}
RDE_INTERNAL void fzp_end(fzp* _d, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_d->points) - _d->first;
    if(_n >= 2u) {
        const fude_zoom_symbol_part _p = { _d->first, _n, _flags };
        rde_arr_add(_d->parts, (any)&_p);
    } else {
        _d->points->count -= _n;
    }
}
// In centimetres (or points) from the middle.
RDE_INTERNAL void fzp_line(fzp* _d, f64 _x0, f64 _y0, f64 _x1, f64 _y1) {
    fzp_begin(_d);
    fzp_pt(_d, _x0 * _d->k, _y0 * _d->k);
    fzp_pt(_d, _x1 * _d->k, _y1 * _d->k);
    fzp_end(_d, 0u);
}
RDE_INTERNAL void fzp_rect(fzp* _d, f64 _x0, f64 _y0, f64 _x1, f64 _y1, u8 _flags) {
    fzp_begin(_d);
    fzp_pt(_d, _x0 * _d->k, _y0 * _d->k); fzp_pt(_d, _x1 * _d->k, _y0 * _d->k);
    fzp_pt(_d, _x1 * _d->k, _y1 * _d->k); fzp_pt(_d, _x0 * _d->k, _y1 * _d->k);
    fzp_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
RDE_INTERNAL void fzp_arc_pts(fzp* _d, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1) {
    u32 _n = (u32)ceil((f64)(_d->segments < 16u ? 16u : _d->segments) * fabs(_a1 - _a0) / (2.0 * FZP_PI));
    _n = _n < 3u ? 3u : _n;
    for(u32 _i = 0; _i <= _n; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_n;
        fzp_pt(_d, (_cx + cos(_a) * _rx) * _d->k, (_cy + sin(_a) * _ry) * _d->k);
    }
}
RDE_INTERNAL void fzp_ellipse(fzp* _d, f64 _cx, f64 _cy, f64 _rx, f64 _ry, u8 _flags) {
    fzp_begin(_d);
    fzp_arc_pts(_d, _cx, _cy, _rx, _ry, 0.0, 2.0 * FZP_PI * (1.0 - 1.0 / 64.0));
    fzp_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
RDE_INTERNAL void fzp_rounded(fzp* _d, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _r, u8 _flags) {
    _r = fmin(_r, fmin(fabs(_x1 - _x0), fabs(_y1 - _y0)) * 0.5);
    fzp_begin(_d);
    fzp_arc_pts(_d, _x1 - _r, _y0 + _r, _r, _r, -FZP_PI * 0.5, 0.0);
    fzp_arc_pts(_d, _x1 - _r, _y1 - _r, _r, _r, 0.0, FZP_PI * 0.5);
    fzp_arc_pts(_d, _x0 + _r, _y1 - _r, _r, _r, FZP_PI * 0.5, FZP_PI);
    fzp_arc_pts(_d, _x0 + _r, _y0 + _r, _r, _r, FZP_PI, FZP_PI * 1.5);
    fzp_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

u32 fude_zoom_plan_draw(const fude_zoom_plan_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_part == NULL) {
        return 0;
    }
    fzp _d = { _points, _parts, fabs(_hw), fabs(_hh), 1.0, _segments < 16u ? 16u : (_segments > 96u ? 96u : _segments), 0u };
    // Drawn in its own size's units (centimetres, points) from its middle, stretched to its box.
    const f64 _w = (f64)_part->w * 0.5, _h = (f64)_part->h * 0.5;
    _d.k = fmin(_d.hw / _w, _d.hh / _h);
    const f64 _ky = _d.hh / _h;   // (stretched one way: what is drawn by its box's corners follows it)
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID, D = FUDE_ZOOM_SYMBOL_DASHED;
    #define BOX(_flags) do { fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh); fzp_pt(&_d, _d.hw, -_d.hh); fzp_pt(&_d, _d.hw, _d.hh); fzp_pt(&_d, -_d.hw, _d.hh); fzp_end(&_d, (u8)((_flags) | FUDE_ZOOM_SYMBOL_CLOSED)); } while(0)
    switch(_part->look) {
    case FZP_DOOR: {
        // Its band (the wall's opening) at its foot, its leaf up from its left, its swing round to its right.
        const f64 _band = 10.0 * _ky, _y0 = -_d.hh, _y1 = -_d.hh + _band, _r = 2.0 * _d.hw;
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _y0); fzp_pt(&_d, _d.hw, _y0); fzp_pt(&_d, _d.hw, _y1); fzp_pt(&_d, -_d.hw, _y1); fzp_end(&_d, F | FUDE_ZOOM_SYMBOL_CLOSED);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _y1); fzp_pt(&_d, -_d.hw, fmin(_y1 + _r, _d.hh)); fzp_end(&_d, 0u);
        fzp_begin(&_d);
        for(u32 _i = 0; _i <= 24u; _i++) {
            const f64 _a = FZP_PI * 0.5 * (1.0 - (f64)_i / 24.0);
            fzp_pt(&_d, -_d.hw + cos(_a) * _r, _y1 + sin(_a) * fmin(_r, _d.hh - _y1));
        }
        fzp_end(&_d, 0u);
        break;
    }
    case FZP_DOUBLE_DOOR: {
        const f64 _band = 10.0 * _ky, _y0 = -_d.hh, _y1 = -_d.hh + _band, _r = _d.hw;
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _y0); fzp_pt(&_d, _d.hw, _y0); fzp_pt(&_d, _d.hw, _y1); fzp_pt(&_d, -_d.hw, _y1); fzp_end(&_d, F | FUDE_ZOOM_SYMBOL_CLOSED);
        for(u32 _s = 0; _s < 2u; _s++) {
            const f64 _x = _s == 0u ? -_d.hw : _d.hw, _dir = _s == 0u ? 1.0 : -1.0;
            fzp_begin(&_d); fzp_pt(&_d, _x, _y1); fzp_pt(&_d, _x, fmin(_y1 + _r, _d.hh)); fzp_end(&_d, 0u);
            fzp_begin(&_d);
            for(u32 _i = 0; _i <= 16u; _i++) {
                const f64 _a = FZP_PI * 0.5 * (1.0 - (f64)_i / 16.0);
                fzp_pt(&_d, _x + _dir * cos(_a) * _r, _y1 + sin(_a) * fmin(_r, _d.hh - _y1));
            }
            fzp_end(&_d, 0u);
        }
        break;
    }
    case FZP_SLIDING:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh * 0.3); fzp_pt(&_d, _d.hw * 0.1, -_d.hh * 0.3); fzp_end(&_d, 0u);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw * 0.1, _d.hh * 0.3); fzp_pt(&_d, _d.hw, _d.hh * 0.3); fzp_end(&_d, 0u);
        break;
    case FZP_WINDOW:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh * 0.25); fzp_pt(&_d, _d.hw, -_d.hh * 0.25); fzp_end(&_d, 0u);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh * 0.25); fzp_pt(&_d, _d.hw, _d.hh * 0.25); fzp_end(&_d, 0u);
        break;
    case FZP_OPENING:
        BOX(F | D);
        break;
    case FZP_BED1:
    case FZP_BED2: {
        BOX(F);
        const u32 _pillows = _part->look == FZP_BED2 ? 2u : 1u;
        const f64 _pw = (2.0 * _d.hw - 12.0 * _d.k * (f64)(_pillows + 1u)) / (f64)_pillows;
        for(u32 _i = 0; _i < _pillows; _i++) {
            const f64 _x0 = -_d.hw + 12.0 * _d.k + (f64)_i * (_pw + 12.0 * _d.k);
            fzp_begin(&_d);
            fzp_pt(&_d, _x0, _d.hh - 12.0 * _d.k); fzp_pt(&_d, _x0 + _pw, _d.hh - 12.0 * _d.k);
            fzp_pt(&_d, _x0 + _pw, _d.hh - 45.0 * _d.k); fzp_pt(&_d, _x0, _d.hh - 45.0 * _d.k);
            fzp_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED);
        }
        // (its cover folded back)
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh - 60.0 * _d.k); fzp_pt(&_d, _d.hw, _d.hh - 60.0 * _d.k); fzp_end(&_d, 0u);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh - 60.0 * _d.k); fzp_pt(&_d, -_d.hw + 25.0 * _d.k, _d.hh - 85.0 * _d.k); fzp_end(&_d, 0u);
        break;
    }
    case FZP_SOFA:
    case FZP_ARMCHAIR: {
        BOX(F);
        const f64 _arm = 18.0 * _d.k, _back = 22.0 * _d.k;
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw + _arm, -_d.hh); fzp_pt(&_d, -_d.hw + _arm, _d.hh - _back); fzp_pt(&_d, _d.hw - _arm, _d.hh - _back);
        fzp_pt(&_d, _d.hw - _arm, -_d.hh); fzp_end(&_d, 0u);
        if(_part->look == FZP_SOFA) {
            for(u32 _i = 1; _i < 3u; _i++) {
                const f64 _x = -_d.hw + _arm + (2.0 * _d.hw - 2.0 * _arm) * (f64)_i / 3.0;
                fzp_begin(&_d); fzp_pt(&_d, _x, -_d.hh); fzp_pt(&_d, _x, _d.hh - _back); fzp_end(&_d, 0u);
            }
        }
        break;
    }
    case FZP_TABLE:
        BOX(F);
        break;
    case FZP_ROUND_TABLE:
        fzp_begin(&_d);
        for(u32 _i = 0; _i < 48u; _i++) {
            const f64 _a = 2.0 * FZP_PI * (f64)_i / 48.0;
            fzp_pt(&_d, cos(_a) * _d.hw, sin(_a) * _d.hh);
        }
        fzp_end(&_d, F | FUDE_ZOOM_SYMBOL_CLOSED);
        break;
    case FZP_CHAIR:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh - 8.0 * _d.k); fzp_pt(&_d, _d.hw, _d.hh - 8.0 * _d.k); fzp_end(&_d, 0u);
        break;
    case FZP_DESK:
        BOX(F);
        fzp_rect(&_d, (_d.hw / _d.k) - 45.0, -(_d.hh / _d.k) + 3.0, (_d.hw / _d.k) - 3.0, (_d.hh / _d.k) - 3.0, 0u);   // (its drawers)
        break;
    case FZP_WARDROBE:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh); fzp_pt(&_d, _d.hw, _d.hh); fzp_end(&_d, 0u);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh); fzp_pt(&_d, _d.hw, -_d.hh); fzp_end(&_d, 0u);
        break;
    case FZP_COUNTER: {
        BOX(F);
        for(f64 _x = -_d.hw + 60.0 * _d.k; _x < _d.hw - 1e-9; _x += 60.0 * _d.k) {   // (a unit each 60 cm)
            fzp_begin(&_d); fzp_pt(&_d, _x, -_d.hh); fzp_pt(&_d, _x, _d.hh); fzp_end(&_d, 0u);
        }
        break;
    }
    case FZP_SINK:
        BOX(F);
        fzp_rounded(&_d, -32.0, -18.0, 20.0, 18.0, 6.0, 0u);
        fzp_ellipse(&_d, 30.0, 12.0, 3.0, 3.0, 0u);
        break;
    case FZP_STOVE:
        BOX(F);
        fzp_ellipse(&_d, -14.0, 14.0, 9.0, 9.0, 0u);
        fzp_ellipse(&_d, 14.0, 14.0, 7.0, 7.0, 0u);
        fzp_ellipse(&_d, -14.0, -14.0, 7.0, 7.0, 0u);
        fzp_ellipse(&_d, 14.0, -14.0, 9.0, 9.0, 0u);
        break;
    case FZP_FRIDGE:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh + 6.0 * _d.k); fzp_pt(&_d, _d.hw, -_d.hh + 6.0 * _d.k); fzp_end(&_d, 0u);
        break;
    case FZP_WASHER:
        BOX(F);
        fzp_ellipse(&_d, 0.0, -3.0, 20.0, 20.0, 0u);
        break;
    case FZP_TOILET:
        fzp_rect(&_d, -20.0, 32.5 - 18.0, 20.0, 32.5, F);
        fzp_ellipse(&_d, 0.0, -6.0, 17.0, 26.0, 0u);
        break;
    case FZP_BATHTUB:
        BOX(F);
        fzp_rounded(&_d, -_d.hw / _d.k + 6.0, -_d.hh / _d.k + 6.0, _d.hw / _d.k - 6.0, _d.hh / _d.k - 6.0, 25.0, 0u);
        fzp_ellipse(&_d, _d.hw / _d.k - 18.0, 0.0, 3.0, 3.0, 0u);
        break;
    case FZP_SHOWER:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh); fzp_pt(&_d, _d.hw, _d.hh); fzp_end(&_d, 0u);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _d.hh); fzp_pt(&_d, _d.hw, -_d.hh); fzp_end(&_d, 0u);
        fzp_ellipse(&_d, 0.0, 0.0, 4.0, 4.0, 0u);
        break;
    case FZP_BASIN:
        fzp_rounded(&_d, -_d.hw / _d.k, -_d.hh / _d.k, _d.hw / _d.k, _d.hh / _d.k, 8.0, F);
        fzp_ellipse(&_d, 0.0, -3.0, 22.0, 14.0, 0u);
        break;
    case FZP_STAIRS: {
        BOX(F);
        const f64 _step = 28.0 * _d.k;
        for(f64 _y = -_d.hh + _step; _y < _d.hh - 1e-9; _y += _step) {
            fzp_begin(&_d); fzp_pt(&_d, -_d.hw, _y); fzp_pt(&_d, _d.hw, _y); fzp_end(&_d, 0u);
        }
        fzp_begin(&_d); fzp_pt(&_d, 0.0, -_d.hh + _step * 0.5); fzp_pt(&_d, 0.0, _d.hh - _step * 0.5); fzp_end(&_d, 0u);   // (its way up)
        fzp_begin(&_d); fzp_pt(&_d, -0.3 * _d.hw, _d.hh - _step * 1.2); fzp_pt(&_d, 0.0, _d.hh - _step * 0.5); fzp_pt(&_d, 0.3 * _d.hw, _d.hh - _step * 1.2); fzp_end(&_d, 0u);
        break;
    }
    // House wiring (points).
    case FZP_SOCKET:
    case FZP_SOCKET2: {
        const u32 _n = _part->look == FZP_SOCKET2 ? 2u : 1u;
        const f64 _r = 12.0;
        for(u32 _i = 0; _i < _n; _i++) {
            const f64 _cx = _n == 1u ? 0.0 : (_i == 0u ? -10.0 : 10.0);
            fzp_begin(&_d);
            fzp_arc_pts(&_d, _cx, -6.0, _r, _r, 0.0, FZP_PI);
            fzp_end(&_d, _i == 0u ? (u8)(F | FUDE_ZOOM_SYMBOL_CLOSED) : 0u);
        }
        fzp_line(&_d, _n == 1u ? -_r : -22.0, -6.0, _n == 1u ? _r : 22.0, -6.0);
        fzp_line(&_d, 0.0, -6.0, 0.0, -15.0);
        break;
    }
    case FZP_SWITCH:
    case FZP_SWITCH2:
    case FZP_DIMMER:
        fzp_ellipse(&_d, 0.0, 0.0, 5.0, 5.0, F);
        fzp_line(&_d, 3.5, 3.5, 13.0, 13.0);
        fzp_line(&_d, 13.0, 13.0, 17.0, 9.0);
        if(_part->look == FZP_SWITCH2) {
            fzp_line(&_d, -3.5, -3.5, -13.0, -13.0);
            fzp_line(&_d, -13.0, -13.0, -17.0, -9.0);
        } else if(_part->look == FZP_DIMMER) {
            fzp_line(&_d, -12.0, -8.0, 4.0, -8.0);
            fzp_begin(&_d); fzp_pt(&_d, 4.0 * _d.k, -5.0 * _d.k); fzp_pt(&_d, 8.0 * _d.k, -8.0 * _d.k); fzp_pt(&_d, 4.0 * _d.k, -11.0 * _d.k); fzp_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | S);
        }
        break;
    case FZP_BELL:
        fzp_ellipse(&_d, 0.0, 0.0, 11.0, 11.0, F);
        fzp_ellipse(&_d, 0.0, 0.0, 4.0, 4.0, S);
        break;
    case FZP_LIGHT:
    case FZP_WALL_LIGHT:
        fzp_ellipse(&_d, 0.0, 0.0, 13.0, 13.0, F);
        fzp_line(&_d, -9.2, -9.2, 9.2, 9.2);
        fzp_line(&_d, -9.2, 9.2, 9.2, -9.2);
        if(_part->look == FZP_WALL_LIGHT) {
            fzp_line(&_d, 0.0, -13.0, 0.0, -20.0);
            fzp_line(&_d, -8.0, -20.0, 8.0, -20.0);
        }
        break;
    case FZP_TUBE:
        fzp_rect(&_d, -34.0, -4.0, 34.0, 4.0, F);
        fzp_line(&_d, -40.0, -8.0, -40.0, 8.0);
        fzp_line(&_d, 40.0, -8.0, 40.0, 8.0);
        fzp_line(&_d, -40.0, 0.0, -34.0, 0.0);
        fzp_line(&_d, 34.0, 0.0, 40.0, 0.0);
        break;
    case FZP_BOARD:
        BOX(F);
        fzp_begin(&_d); fzp_pt(&_d, -_d.hw, -_d.hh); fzp_pt(&_d, _d.hw, -_d.hh); fzp_pt(&_d, _d.hw, _d.hh); fzp_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | S);
        break;
    case FZP_JUNCTION:
        fzp_ellipse(&_d, 0.0, 0.0, 10.0, 10.0, F);
        fzp_ellipse(&_d, 0.0, 0.0, 3.0, 3.0, S);
        break;
    case FZP_TV:
    case FZP_METER:
        BOX(F);
        break;
    case FZP_DATA: {
        const f64 _t[6] = { -15.0, -12.0, 15.0, -12.0, 0.0, 13.0 };
        fzp_begin(&_d);
        for(u32 _i = 0; _i < 3u; _i++) {
            fzp_pt(&_d, _t[2u * _i] * _d.k, _t[2u * _i + 1u] * _d.k);
        }
        fzp_end(&_d, F | FUDE_ZOOM_SYMBOL_CLOSED);
        fzp_line(&_d, 0.0, -12.0, 0.0, -17.0);
        break;
    }
    case FZP_SMOKE:
        fzp_rect(&_d, -16.0, -16.0, 16.0, 16.0, F);
        fzp_ellipse(&_d, 0.0, 0.0, 9.0, 9.0, 0u);
        fzp_ellipse(&_d, 0.0, 0.0, 2.5, 2.5, S);
        break;
    case FZP_THERMOSTAT:
        fzp_ellipse(&_d, 0.0, 0.0, 14.0, 14.0, F);
        fzp_line(&_d, 0.0, -8.0, 0.0, 8.0);
        fzp_ellipse(&_d, 0.0, -9.0, 3.0, 3.0, S);
        break;
    case FZP_FAN:
        fzp_ellipse(&_d, 0.0, 0.0, 4.0, 4.0, F);
        for(u32 _b = 0; _b < 3u; _b++) {
            const f64 _a = 2.0 * FZP_PI * (f64)_b / 3.0;
            fzp_begin(&_d);
            fzp_pt(&_d, cos(_a) * 4.0 * _d.k, sin(_a) * 4.0 * _d.k);
            fzp_pt(&_d, (cos(_a + 0.25) * 22.0) * _d.k, (sin(_a + 0.25) * 22.0) * _d.k);
            fzp_pt(&_d, (cos(_a - 0.15) * 23.0) * _d.k, (sin(_a - 0.15) * 23.0) * _d.k);
            fzp_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED);
        }
        break;
    default:
        BOX(F);
        break;
    }
    #undef BOX
    return (u32)rde_arr_length(_parts);
}
