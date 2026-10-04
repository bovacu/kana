// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/handwriting/match.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See match.h.
// ===========================================================================

#define FUDE_MATCH_BOX        100.0f
#define FUDE_MATCH_FLATTEN    1.0f      // KanjiVG units: coarse is fine for matching
#define FUDE_MATCH_MAX_POINTS 1024u
#define FUDE_MATCH_COARSE     4u        // points per stroke in the first, rough pass: 0, 5, 10, 15
#define FUDE_MATCH_COARSE_STEP ((FUDE_MATCH_POINTS - 1) / (FUDE_MATCH_COARSE - 1))
#define FUDE_MATCH_KEEP       64u       // the rough pass keeps this many (at least) for the exact one

// _n points → FUDE_MATCH_POINTS, evenly spaced along the length.
void fude_match_resample_points(const rde_vec_2F* _in, u32 _n, fude_match_stroke* _out) {
    f32 _total = 0.0f;
    for(u32 _i = 1; _i < _n; _i++) {
        const f32 _dx = _in[_i].x - _in[_i - 1].x;
        const f32 _dy = _in[_i].y - _in[_i - 1].y;
        _total += sqrtf(_dx * _dx + _dy * _dy);
    }

    if(_n < 2 || _total < 1e-6f) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            _out->p[_k] = _n > 0 ? _in[0] : (rde_vec_2F){ 0.0f, 0.0f };
        }
        return;
    }

    const f32 _step   = _total / (f32)(FUDE_MATCH_POINTS - 1);
    u32       _seg    = 1;
    f32       _walked = 0.0f;   // length up to _in[_seg - 1]
    _out->p[0] = _in[0];

    for(u32 _k = 1; _k < FUDE_MATCH_POINTS - 1; _k++) {
        const f32 _target = _step * (f32)_k;
        for(;;) {
            const f32 _dx  = _in[_seg].x - _in[_seg - 1].x;
            const f32 _dy  = _in[_seg].y - _in[_seg - 1].y;
            const f32 _len = sqrtf(_dx * _dx + _dy * _dy);
            if(_walked + _len >= _target || _seg + 1 >= _n) {
                const f32 _t = _len > 1e-6f ? rde_math_clamp_f32((_target - _walked) / _len, 0.0f, 1.0f) : 0.0f;
                _out->p[_k] = (rde_vec_2F){ _in[_seg - 1].x + _dx * _t, _in[_seg - 1].y + _dy * _t };
                break;
            }
            _walked += _len;
            _seg++;
        }
    }

    _out->p[FUDE_MATCH_POINTS - 1] = _in[_n - 1];
}

// Fits all strokes, together, into the box: the longer side to FUDE_MATCH_BOX,
// centred. Proportions are kept — a flat 一 stays flat.
void fude_match_fit(fude_match_stroke* _strokes, u32 _count) {
    rde_vec_2F _min = { 1e30f, 1e30f };
    rde_vec_2F _max = { -1e30f, -1e30f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            const rde_vec_2F _p = _strokes[_s].p[_k];
            _min = (rde_vec_2F){ fminf(_min.x, _p.x), fminf(_min.y, _p.y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _p.x), fmaxf(_max.y, _p.y) };
        }
    }

    const f32        _extent = fmaxf(_max.x - _min.x, _max.y - _min.y);
    const f32        _scale  = _extent > 1e-4f ? FUDE_MATCH_BOX / _extent : 1.0f;
    const rde_vec_2F _center = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            _strokes[_s].p[_k].x = (_strokes[_s].p[_k].x - _center.x) * _scale + FUDE_MATCH_BOX * 0.5f;
            _strokes[_s].p[_k].y = (_strokes[_s].p[_k].y - _center.y) * _scale + FUDE_MATCH_BOX * 0.5f;
        }
    }
}

f32 fude_match_points_distance(const fude_match_stroke* _a, const fude_match_stroke* _b, b8 _reversed) {
    f32 _sum = 0.0f;
    for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
        const rde_vec_2F _p = _a->p[_k];
        const rde_vec_2F _q = _b->p[_reversed ? FUDE_MATCH_POINTS - 1 - _k : _k];
        _sum += sqrtf((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
    }
    return _sum / (f32)FUDE_MATCH_POINTS;
}

// Both directions in one walk: min(forward, backward + FUDE_MATCH_REVERSED).
RDE_INTERNAL f32 fude_match_stroke_distance(const fude_match_stroke* _a, const fude_match_stroke* _b) {
    f32 _fwd = 0.0f;
    f32 _rev = 0.0f;
    for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
        const rde_vec_2F _p = _a->p[_k];
        const rde_vec_2F _q = _b->p[_k];
        const rde_vec_2F _r = _b->p[FUDE_MATCH_POINTS - 1 - _k];
        _fwd += sqrtf((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
        _rev += sqrtf((_p.x - _r.x) * (_p.x - _r.x) + (_p.y - _r.y) * (_p.y - _r.y));
    }
    return fminf(_fwd / (f32)FUDE_MATCH_POINTS, _rev / (f32)FUDE_MATCH_POINTS + FUDE_MATCH_REVERSED);
}

u32 fude_match_drawing(const fude_ink* _drawing, fude_match_stroke* _out, u32 _max) {
    static rde_vec_2F _pts[FUDE_MATCH_MAX_POINTS];
    u32 _n = 0;

    for(u32 _s = 0; _s < fude_ink_stroke_count(_drawing) && _n < _max; _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_drawing, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const fude_ink_point* _p     = fude_ink_stroke_points(_drawing, _stroke);
        const u32             _count = _stroke->point_count < FUDE_MATCH_MAX_POINTS ? _stroke->point_count : FUDE_MATCH_MAX_POINTS;
        for(u32 _i = 0; _i < _count; _i++) {
            _pts[_i] = (rde_vec_2F){ _p[_i].position.x, -_p[_i].position.y };
        }
        fude_match_resample_points(_pts, _count, &_out[_n++]);
    }

    if(_n > 0) {
        fude_match_fit(_out, _n);
    }
    return _n;
}

u32 fude_match_reference(const fude_kanji_db* _db, const fude_kanji_info* _info, fude_match_stroke* _out, u32 _max) {
    static rde_vec_2F _pts[FUDE_MATCH_MAX_POINTS];
    const u32 _m = _info->strokes < _max ? _info->strokes : _max;

    for(u32 _s = 0; _s < _m; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_db, _info, _s, &_stroke)) {
            return 0;
        }
        const u32 _count = fude_kanji_stroke_points(&_stroke, FUDE_MATCH_FLATTEN, _pts, FUDE_MATCH_MAX_POINTS);
        fude_match_resample_points(_pts, _count, &_out[_s]);
    }

    if(_m > 0) {
        fude_match_fit(_out, _m);
    }
    return _m;
}

// The references, each prepared once (resampled, fitted) the first time it is
// compared: all of them are ~80,000 strokes, ~10 MB, ~80 ms to prepare in a
// debug build — which every ranking paid before. Kept for the database they
// were made from; fude_match_release lets them go.
static struct {
    const u8*          records;    // the database they belong to (its record table)
    u32                count;
    u32*               first;      // per record: its first stroke in strokes
    u8*                ready;      // per record: 0 not yet, 1 prepared, 2 none (no strokes, too many)
    rde_vec_2F*        extent;     // per record: min, max — KanjiVG units, Y down
    fude_match_stroke* strokes;
} fude_match_refs;

void fude_match_release(void) {
    if(fude_match_refs.first != NULL) {
        rde_free(fude_match_refs.first);
        rde_free(fude_match_refs.ready);
        rde_free(fude_match_refs.extent);
        rde_free(fude_match_refs.strokes);
    }
    memset(&fude_match_refs, 0, sizeof(fude_match_refs));
}

// A record's prepared strokes, NULL when it has none; its info in _info.
RDE_INTERNAL const fude_match_stroke* fude_match_prepared(const fude_kanji_db* _db, u32 _record, fude_kanji_info* _info) {
    if(fude_match_refs.records != _db->_records || fude_match_refs.count != _db->count) {
        fude_match_release();
        u32 _total = 0;
        fude_match_refs.first = (u32*)rde_malloc(sizeof(u32) * (_db->count + 1u));
        for(u32 _r = 0; _r < _db->count; _r++) {
            fude_kanji_info _in;
            fude_match_refs.first[_r] = _total;
            if(fude_kanji_at(_db, _r, &_in) && _in.strokes <= FUDE_MATCH_MAX_STROKES) {
                _total += _in.strokes;
            }
        }
        fude_match_refs.first[_db->count] = _total;
        fude_match_refs.ready   = (u8*)rde_malloc(_db->count + 1u);
        fude_match_refs.extent  = (rde_vec_2F*)rde_malloc(sizeof(rde_vec_2F) * 2u * (_db->count + 1u));
        fude_match_refs.strokes = (fude_match_stroke*)rde_malloc(sizeof(fude_match_stroke) * (_total + 1u));
        memset(fude_match_refs.ready, 0, _db->count + 1u);
        fude_match_refs.records = _db->_records;
        fude_match_refs.count   = _db->count;
    }

    if(_record >= _db->count || !fude_kanji_at(_db, _record, _info)) {
        return NULL;
    }
    if(fude_match_refs.ready[_record] == 0) {
        static rde_vec_2F _pts[FUDE_MATCH_MAX_POINTS];
        fude_match_stroke* _out = &fude_match_refs.strokes[fude_match_refs.first[_record]];
        const u32          _m   = fude_match_refs.first[_record + 1u] - fude_match_refs.first[_record];
        rde_vec_2F         _min = { 1e30f, 1e30f };
        rde_vec_2F         _max = { -1e30f, -1e30f };
        b8                 _ok  = _m > 0 && _m == _info->strokes;
        for(u32 _s = 0; _s < _m && _ok; _s++) {
            fude_kanji_stroke _stroke;
            _ok = fude_kanji_stroke_at(_db, _info, _s, &_stroke);
            if(_ok) {
                const u32 _count = fude_kanji_stroke_points(&_stroke, FUDE_MATCH_FLATTEN, _pts, FUDE_MATCH_MAX_POINTS);
                fude_match_resample_points(_pts, _count, &_out[_s]);
                for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
                    _min = (rde_vec_2F){ fminf(_min.x, _out[_s].p[_k].x), fminf(_min.y, _out[_s].p[_k].y) };
                    _max = (rde_vec_2F){ fmaxf(_max.x, _out[_s].p[_k].x), fmaxf(_max.y, _out[_s].p[_k].y) };
                }
            }
        }
        if(_ok) {
            fude_match_fit(_out, _m);
            fude_match_refs.extent[_record * 2u]      = _min;
            fude_match_refs.extent[_record * 2u + 1u] = _max;
        }
        fude_match_refs.ready[_record] = _ok ? 1u : 2u;
    }
    return fude_match_refs.ready[_record] == 1u ? &fude_match_refs.strokes[fude_match_refs.first[_record]] : NULL;
}

b8 fude_match_extent(const fude_kanji_db* _db, u32 _record, rde_vec_2F* _min, rde_vec_2F* _max) {
    fude_kanji_info _info;
    if(_db == NULL || fude_match_prepared(_db, _record, &_info) == NULL) {
        return false;
    }
    *_min = fude_match_refs.extent[_record * 2u];
    *_max = fude_match_refs.extent[_record * 2u + 1u];
    return true;
}

// The alignment, on a table of stroke distances _d (see match.h, step 3).
RDE_INTERNAL f32 fude_match_align(f32 _d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES], u32 _n, u32 _m) {
    static f32 _dp[FUDE_MATCH_MAX_STROKES + 1][FUDE_MATCH_MAX_STROKES + 1];

    // In order, with gaps for a missing or extra stroke.
    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * FUDE_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * FUDE_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            const f32 _pair = _dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1];
            const f32 _skip = fminf(_dp[_i - 1][_j], _dp[_i][_j - 1]) + FUDE_MATCH_GAP;
            _dp[_i][_j] = fminf(_pair, _skip);
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);

    if(_n != _m) {
        return _ordered;
    }

    // Order-free: greedily pair the closest strokes.
    b8  _row_used[FUDE_MATCH_MAX_STROKES] = { 0 };
    b8  _col_used[FUDE_MATCH_MAX_STROKES] = { 0 };
    f32 _sum = 0.0f;
    for(u32 _pairs = 0; _pairs < _n; _pairs++) {
        f32 _best = 1e30f;
        u32 _bi = 0, _bj = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            if(_row_used[_i]) { continue; }
            for(u32 _j = 0; _j < _m; _j++) {
                if(!_col_used[_j] && _d[_i][_j] < _best) {
                    _best = _d[_i][_j];
                    _bi   = _i;
                    _bj   = _j;
                }
            }
        }
        _row_used[_bi] = true;
        _col_used[_bj] = true;
        _sum += _best;
    }
    const f32 _unordered = _sum / (f32)_n + FUDE_MATCH_UNORDERED;

    return fminf(_ordered, _unordered);
}

// The character-to-character cost (see match.h, step 3).
RDE_INTERNAL f32 fude_match_cost(const fude_match_stroke* _user, u32 _n, const fude_match_stroke* _ref, u32 _m) {
    static f32 _d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES];
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _j = 0; _j < _m; _j++) {
            _d[_i][_j] = fude_match_stroke_distance(&_user[_i], &_ref[_j]);
        }
    }
    return fude_match_align(_d, _n, _m);
}

// The rough cost: the same, on FUDE_MATCH_COARSE points per stroke, and for the
// order-free pairing each stroke's closest (a bound under any pairing) — about a
// sixth of the work, and close enough to choose which few get the exact one.
RDE_INTERNAL f32 fude_match_rough_cost(const rde_vec_2F _user[][FUDE_MATCH_COARSE], u32 _n, const fude_match_stroke* _ref, u32 _m) {
    static f32 _d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES];
    static f32 _dp[FUDE_MATCH_MAX_STROKES + 1][FUDE_MATCH_MAX_STROKES + 1];
    f32 _closest = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        f32 _row = 1e30f;
        for(u32 _j = 0; _j < _m; _j++) {
            f32 _fwd = 0.0f;
            f32 _rev = 0.0f;
            for(u32 _k = 0; _k < FUDE_MATCH_COARSE; _k++) {
                const rde_vec_2F _p = _user[_i][_k];
                const rde_vec_2F _q = _ref[_j].p[_k * FUDE_MATCH_COARSE_STEP];
                const rde_vec_2F _r = _ref[_j].p[(FUDE_MATCH_COARSE - 1u - _k) * FUDE_MATCH_COARSE_STEP];
                _fwd += sqrtf((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
                _rev += sqrtf((_p.x - _r.x) * (_p.x - _r.x) + (_p.y - _r.y) * (_p.y - _r.y));
            }
            _d[_i][_j] = fminf(_fwd / (f32)FUDE_MATCH_COARSE, _rev / (f32)FUDE_MATCH_COARSE + FUDE_MATCH_REVERSED);
            _row       = fminf(_row, _d[_i][_j]);
        }
        _closest += _row;
    }

    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * FUDE_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * FUDE_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            _dp[_i][_j] = fminf(_dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1], fminf(_dp[_i - 1][_j], _dp[_i][_j - 1]) + FUDE_MATCH_GAP);
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);
    return _n != _m ? _ordered : fminf(_ordered, _closest / (f32)_n + FUDE_MATCH_UNORDERED);
}

// Does record _r pass _filter (FUDE_MATCH_TEXT included)?
RDE_INTERNAL b8 fude_match_passes(const fude_kanji_db* _db, const fude_catalog* _catalog, u32 _r, FUDE_FILTER_ _filter, const fude_kanji_info* _info) {
    if(_filter != FUDE_MATCH_TEXT) {
        return fude_catalog_passes(_catalog, _r, _filter);
    }
    if(fude_catalog_passes(_catalog, _r, FUDE_FILTER_ALL)) {
        return true;
    }
    RDE_UNUSED(_db);
    switch(_info->codepoint) {
        case 0x3001: case 0x3002: case 0x3005: case 0x30FB: case 0x30FC: case 0xFF01: case 0x0021: case 0x003F:
            return true;
        default:
            return _info->codepoint >= '0' && _info->codepoint <= '9';
    }
}

// Keeps the best _max of what it is given, sorted, in _list (*_found of them).
RDE_INTERNAL void fude_match_keep(fude_match_result* _list, u32* _found, u32 _max, u32 _record, f32 _cost) {
    if(*_found < _max || _cost < _list[*_found - 1u].cost) {
        u32 _at = *_found < _max ? *_found : _max - 1u;
        while(_at > 0 && _list[_at - 1u].cost > _cost) {
            if(_at < _max) {
                _list[_at] = _list[_at - 1u];
            }
            _at--;
        }
        _list[_at] = (fude_match_result){ .record = _record, .cost = _cost };
        if(*_found < _max) {
            (*_found)++;
        }
    }
}

f32 fude_match_cost_record(const fude_kanji_db* _db, u32 _record, const fude_match_stroke* _strokes, u32 _count) {
    fude_kanji_info          _info;
    const fude_match_stroke* _ref = _db != NULL ? fude_match_prepared(_db, _record, &_info) : NULL;
    if(_ref == NULL || _count == 0 || _count > FUDE_MATCH_MAX_STROKES) {
        return 1e30f;
    }
    return fude_match_cost(_strokes, _count, _ref, _info.strokes);
}

u32 fude_match_rank_strokes(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                            const fude_match_stroke* _user, u32 _n, fude_match_result* _out, u32 _max) {
    static rde_vec_2F        _rough_user[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_COARSE];
    static fude_match_result _kept[FUDE_MATCH_KEEP * 4u];

    if(_db == NULL || _max == 0 || _n == 0 || _n > FUDE_MATCH_MAX_STROKES) {
        return 0;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _k = 0; _k < FUDE_MATCH_COARSE; _k++) {
            _rough_user[_i][_k] = _user[_i].p[_k * FUDE_MATCH_COARSE_STEP];
        }
    }

    // The rough pass over every character near the stroke count...
    const u32 _keep  = _max * 3u > FUDE_MATCH_KEEP ? (_max * 3u < FUDE_MATCH_KEEP * 4u ? _max * 3u : FUDE_MATCH_KEEP * 4u) : FUDE_MATCH_KEEP;
    u32       _kept_n = 0;
    for(u32 _r = 0; _r < _db->count; _r++) {
        fude_kanji_info _info;
        if(!fude_kanji_at(_db, _r, &_info) || _info.strokes == 0 || _info.strokes > FUDE_MATCH_MAX_STROKES) {
            continue;
        }
        const u32 _m = _info.strokes;
        if(_m + FUDE_MATCH_STROKE_SLACK < _n || _m > _n + FUDE_MATCH_STROKE_SLACK) {
            continue;
        }
        if(!fude_match_passes(_db, _catalog, _r, _filter, &_info)) {
            continue;
        }
        const fude_match_stroke* _ref = fude_match_prepared(_db, _r, &_info);
        if(_ref == NULL) {
            continue;
        }
        fude_match_keep(_kept, &_kept_n, _keep, _r, fude_match_rough_cost(_rough_user, _n, _ref, _m));
    }

    // ...and the exact one over the closest of those.
    u32 _found = 0;
    for(u32 _i = 0; _i < _kept_n; _i++) {
        fude_kanji_info          _info;
        const fude_match_stroke* _ref = fude_match_prepared(_db, _kept[_i].record, &_info);
        fude_match_keep(_out, &_found, _max, _kept[_i].record, fude_match_cost(_user, _n, _ref, _info.strokes));
    }
    return _found;
}

u32 fude_match_rank(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                    const fude_ink* _drawing, fude_match_result* _out, u32 _max) {
    static fude_match_stroke _user[FUDE_MATCH_MAX_STROKES];

    if(_db == NULL || _max == 0) {
        return 0;
    }
    const u32 _n = fude_match_drawing(_drawing, _user, FUDE_MATCH_MAX_STROKES);
    return fude_match_rank_strokes(_db, _catalog, _filter, _user, _n, _out, _max);
}
