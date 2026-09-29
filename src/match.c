#include "match.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See match.h.
// ===========================================================================

#define KANA_MATCH_BOX        100.0f
#define KANA_MATCH_FLATTEN    1.0f      // KanjiVG units: coarse is fine for matching
#define KANA_MATCH_MAX_POINTS 1024u

// _n points → KANA_MATCH_POINTS, evenly spaced along the length.
RDE_INTERNAL void kana_match_resample(const rde_vec_2F* _in, u32 _n, kana_match_stroke* _out) {
    f32 _total = 0.0f;
    for(u32 _i = 1; _i < _n; _i++) {
        const f32 _dx = _in[_i].x - _in[_i - 1].x;
        const f32 _dy = _in[_i].y - _in[_i - 1].y;
        _total += sqrtf(_dx * _dx + _dy * _dy);
    }

    if(_n < 2 || _total < 1e-6f) {
        for(u32 _k = 0; _k < KANA_MATCH_POINTS; _k++) {
            _out->p[_k] = _n > 0 ? _in[0] : (rde_vec_2F){ 0.0f, 0.0f };
        }
        return;
    }

    const f32 _step   = _total / (f32)(KANA_MATCH_POINTS - 1);
    u32       _seg    = 1;
    f32       _walked = 0.0f;   // length up to _in[_seg - 1]
    _out->p[0] = _in[0];

    for(u32 _k = 1; _k < KANA_MATCH_POINTS - 1; _k++) {
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

    _out->p[KANA_MATCH_POINTS - 1] = _in[_n - 1];
}

// Fits all strokes, together, into the box: the longer side to KANA_MATCH_BOX,
// centred. Proportions are kept — a flat 一 stays flat.
RDE_INTERNAL void kana_match_normalize(kana_match_stroke* _strokes, u32 _count) {
    rde_vec_2F _min = { 1e30f, 1e30f };
    rde_vec_2F _max = { -1e30f, -1e30f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < KANA_MATCH_POINTS; _k++) {
            const rde_vec_2F _p = _strokes[_s].p[_k];
            _min = (rde_vec_2F){ fminf(_min.x, _p.x), fminf(_min.y, _p.y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _p.x), fmaxf(_max.y, _p.y) };
        }
    }

    const f32        _extent = fmaxf(_max.x - _min.x, _max.y - _min.y);
    const f32        _scale  = _extent > 1e-4f ? KANA_MATCH_BOX / _extent : 1.0f;
    const rde_vec_2F _center = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < KANA_MATCH_POINTS; _k++) {
            _strokes[_s].p[_k].x = (_strokes[_s].p[_k].x - _center.x) * _scale + KANA_MATCH_BOX * 0.5f;
            _strokes[_s].p[_k].y = (_strokes[_s].p[_k].y - _center.y) * _scale + KANA_MATCH_BOX * 0.5f;
        }
    }
}

f32 kana_match_points_distance(const kana_match_stroke* _a, const kana_match_stroke* _b, b8 _reversed) {
    f32 _sum = 0.0f;
    for(u32 _k = 0; _k < KANA_MATCH_POINTS; _k++) {
        const rde_vec_2F _p = _a->p[_k];
        const rde_vec_2F _q = _b->p[_reversed ? KANA_MATCH_POINTS - 1 - _k : _k];
        _sum += sqrtf((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
    }
    return _sum / (f32)KANA_MATCH_POINTS;
}

RDE_INTERNAL f32 kana_match_stroke_distance(const kana_match_stroke* _a, const kana_match_stroke* _b) {
    return fminf(kana_match_points_distance(_a, _b, false), kana_match_points_distance(_a, _b, true) + KANA_MATCH_REVERSED);
}

u32 kana_match_drawing(const kana_ink* _drawing, kana_match_stroke* _out, u32 _max) {
    static rde_vec_2F _pts[KANA_MATCH_MAX_POINTS];
    u32 _n = 0;

    for(u32 _s = 0; _s < kana_ink_stroke_count(_drawing) && _n < _max; _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_drawing, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const kana_ink_point* _p     = kana_ink_stroke_points(_drawing, _stroke);
        const u32             _count = _stroke->point_count < KANA_MATCH_MAX_POINTS ? _stroke->point_count : KANA_MATCH_MAX_POINTS;
        for(u32 _i = 0; _i < _count; _i++) {
            _pts[_i] = (rde_vec_2F){ _p[_i].position.x, -_p[_i].position.y };
        }
        kana_match_resample(_pts, _count, &_out[_n++]);
    }

    if(_n > 0) {
        kana_match_normalize(_out, _n);
    }
    return _n;
}

u32 kana_match_reference(const kana_kanji_db* _db, const kana_kanji_info* _info, kana_match_stroke* _out, u32 _max) {
    static rde_vec_2F _pts[KANA_MATCH_MAX_POINTS];
    const u32 _m = _info->strokes < _max ? _info->strokes : _max;

    for(u32 _s = 0; _s < _m; _s++) {
        kana_kanji_stroke _stroke;
        if(!kana_kanji_stroke_at(_db, _info, _s, &_stroke)) {
            return 0;
        }
        const u32 _count = kana_kanji_stroke_points(&_stroke, KANA_MATCH_FLATTEN, _pts, KANA_MATCH_MAX_POINTS);
        kana_match_resample(_pts, _count, &_out[_s]);
    }

    if(_m > 0) {
        kana_match_normalize(_out, _m);
    }
    return _m;
}

// The character-to-character cost (see match.h, step 3).
RDE_INTERNAL f32 kana_match_cost(const kana_match_stroke* _user, u32 _n, const kana_match_stroke* _ref, u32 _m) {
    static f32 _d[KANA_MATCH_MAX_STROKES][KANA_MATCH_MAX_STROKES];
    static f32 _dp[KANA_MATCH_MAX_STROKES + 1][KANA_MATCH_MAX_STROKES + 1];

    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _j = 0; _j < _m; _j++) {
            _d[_i][_j] = kana_match_stroke_distance(&_user[_i], &_ref[_j]);
        }
    }

    // In order, with gaps for a missing or extra stroke.
    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * KANA_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * KANA_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            const f32 _pair = _dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1];
            const f32 _skip = fminf(_dp[_i - 1][_j], _dp[_i][_j - 1]) + KANA_MATCH_GAP;
            _dp[_i][_j] = fminf(_pair, _skip);
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);

    if(_n != _m) {
        return _ordered;
    }

    // Order-free: greedily pair the closest strokes.
    b8  _row_used[KANA_MATCH_MAX_STROKES] = { 0 };
    b8  _col_used[KANA_MATCH_MAX_STROKES] = { 0 };
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
    const f32 _unordered = _sum / (f32)_n + KANA_MATCH_UNORDERED;

    return fminf(_ordered, _unordered);
}

u32 kana_match_rank(const kana_kanji_db* _db, const kana_catalog* _catalog, KANA_FILTER_ _filter,
                    const kana_ink* _drawing, kana_match_result* _out, u32 _max) {
    static kana_match_stroke _user[KANA_MATCH_MAX_STROKES];
    static kana_match_stroke _ref[KANA_MATCH_MAX_STROKES];

    if(_db == NULL || _max == 0) {
        return 0;
    }

    const u32 _n = kana_match_drawing(_drawing, _user, KANA_MATCH_MAX_STROKES);
    if(_n == 0) {
        return 0;
    }

    u32 _found = 0;
    for(u32 _r = 0; _r < _db->count; _r++) {
        kana_kanji_info _info;
        if(!kana_kanji_at(_db, _r, &_info) || _info.strokes == 0 || _info.strokes > KANA_MATCH_MAX_STROKES) {
            continue;
        }
        const u32 _m = _info.strokes;
        if(_m + KANA_MATCH_STROKE_SLACK < _n || _m > _n + KANA_MATCH_STROKE_SLACK) {
            continue;
        }
        if(!kana_catalog_passes(_catalog, _r, _filter)) {
            continue;
        }

        if(kana_match_reference(_db, &_info, _ref, KANA_MATCH_MAX_STROKES) != _m) {
            continue;
        }

        const f32 _cost = kana_match_cost(_user, _n, _ref, _m);

        // Keep the best _max, sorted: insert, dropping the worst.
        if(_found < _max || _cost < _out[_found - 1].cost) {
            u32 _at = _found < _max ? _found : _max - 1u;
            while(_at > 0 && _out[_at - 1].cost > _cost) {
                if(_at < _max) {
                    _out[_at] = _out[_at - 1];
                }
                _at--;
            }
            _out[_at] = (kana_match_result){ .record = _r, .cost = _cost };
            if(_found < _max) {
                _found++;
            }
        }
    }

    return _found;
}
