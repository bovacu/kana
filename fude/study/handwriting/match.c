// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/handwriting/match.h"

// ===========================================================================
// See match.h.
//
// The in-order alignment of step 3 is dynamic programming over the two stroke lists (like an edit distance, with
// FUDE_MATCH_GAP for a stroke left unpaired); the order-free pairing pairs the closest strokes greedily, which is
// cheaper than an optimal assignment and close enough for ranking. A ranking first costs every candidate roughly
// (FUDE_MATCH_COARSE points per stroke), keeps the closest, and costs only those exactly. The candidates are found
// through the records sorted by stroke count, made once with the prepared strokes, and a candidate that could not be
// kept is turned away by a floor under its rough cost that needs no square root: reading free writing (segment.h)
// ranks every run of strokes it tries, many for each character.
// ===========================================================================

// The side of the box the strokes are fitted into.
#define FUDE_MATCH_BOX        100.0f
#define FUDE_MATCH_FLATTEN    1.0f      // in KanjiVG units: coarse flattening is fine for matching
// The most points of a stroke used; longer strokes are cut.
#define FUDE_MATCH_MAX_POINTS 1024u
#define FUDE_MATCH_COARSE     4u        // points per stroke in the first, rough pass: 0, 5, 10, 15
// Which of the FUDE_MATCH_POINTS points the rough pass uses: every FUDE_MATCH_COARSE_STEP-th.
#define FUDE_MATCH_COARSE_STEP ((FUDE_MATCH_POINTS - 1) / (FUDE_MATCH_COARSE - 1))
#define FUDE_MATCH_KEEP       64u       // the rough pass keeps at least this many for the exact one
#define FUDE_MATCH_NO_RECORD  0xFFFFFFFFu   // past every record: no record

// Resamples _n points into FUDE_MATCH_POINTS, evenly spaced along their length. A stroke with no length (a dot)
// becomes its first point repeated.
void fude_match_resample_points(const rde_vec_2F* _in, u32 _n, fude_match_stroke* _out) {
    f32 _total = 0.0f;
    for(u32 _i = 1; _i < _n; _i++) {
        const f32 _dx = _in[_i].x - _in[_i - 1].x;
        const f32 _dy = _in[_i].y - _in[_i - 1].y;
        _total += rde_math_sqrt_f32(_dx * _dx + _dy * _dy);
    }

    if(_n < 2 || _total < 1e-6f) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            _out->p[_k] = _n > 0 ? _in[0] : (rde_vec_2F){ 0.0f, 0.0f };
        }
        return;
    }

    const f32 _step   = _total / (f32)(FUDE_MATCH_POINTS - 1);
    u32       _seg    = 1;
    f32       _walked = 0.0f;   // the length up to _in[_seg - 1]
    _out->p[0] = _in[0];

    for(u32 _k = 1; _k < FUDE_MATCH_POINTS - 1; _k++) {
        const f32 _target = _step * (f32)_k;
        for(;;) {
            const f32 _dx  = _in[_seg].x - _in[_seg - 1].x;
            const f32 _dy  = _in[_seg].y - _in[_seg - 1].y;
            const f32 _len = rde_math_sqrt_f32(_dx * _dx + _dy * _dy);
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

// Fits all the strokes, together, into the box: the longer side to FUDE_MATCH_BOX, centred. The proportions are
// kept, so a flat 一 stays flat.
void fude_match_fit(fude_match_stroke* _strokes, u32 _count) {
    rde_vec_2F _min = { 1e30f, 1e30f };
    rde_vec_2F _max = { -1e30f, -1e30f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            const rde_vec_2F _p = _strokes[_s].p[_k];
            _min = (rde_vec_2F){ rde_math_min_f32(_min.x, _p.x), rde_math_min_f32(_min.y, _p.y) };
            _max = (rde_vec_2F){ rde_math_max_f32(_max.x, _p.x), rde_math_max_f32(_max.y, _p.y) };
        }
    }

    const f32        _extent = rde_math_max_f32(_max.x - _min.x, _max.y - _min.y);
    const f32        _scale  = _extent > 1e-4f ? FUDE_MATCH_BOX / _extent : 1.0f;
    const rde_vec_2F _center = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
    for(u32 _s = 0; _s < _count; _s++) {
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            _strokes[_s].p[_k].x = (_strokes[_s].p[_k].x - _center.x) * _scale + FUDE_MATCH_BOX * 0.5f;
            _strokes[_s].p[_k].y = (_strokes[_s].p[_k].y - _center.y) * _scale + FUDE_MATCH_BOX * 0.5f;
        }
    }
}

// Returns the mean distance between two strokes' corresponding points (see the header).
f32 fude_match_points_distance(const fude_match_stroke* _a, const fude_match_stroke* _b, b8 _reversed) {
    f32 _sum = 0.0f;
    for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
        const rde_vec_2F _p = _a->p[_k];
        const rde_vec_2F _q = _b->p[_reversed ? FUDE_MATCH_POINTS - 1 - _k : _k];
        _sum += rde_math_sqrt_f32((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
    }
    return _sum / (f32)FUDE_MATCH_POINTS;
}

// Returns the distance between two strokes in both directions in one walk: min(forward, backward +
// FUDE_MATCH_REVERSED).
RDE_INTERNAL f32 fude_match_stroke_distance(const fude_match_stroke* _a, const fude_match_stroke* _b) {
    f32 _fwd = 0.0f;
    f32 _rev = 0.0f;
    for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
        const rde_vec_2F _p = _a->p[_k];
        const rde_vec_2F _q = _b->p[_k];
        const rde_vec_2F _r = _b->p[FUDE_MATCH_POINTS - 1 - _k];
        _fwd += rde_math_sqrt_f32((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
        _rev += rde_math_sqrt_f32((_p.x - _r.x) * (_p.x - _r.x) + (_p.y - _r.y) * (_p.y - _r.y));
    }
    return rde_math_min_f32(_fwd / (f32)FUDE_MATCH_POINTS, _rev / (f32)FUDE_MATCH_POINTS + FUDE_MATCH_REVERSED);
}

// Prepares a drawing's alive strokes (see the header). Y is flipped to KanjiVG's Y down.
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

// Prepares a reference character's strokes (see the header). Only the first _max strokes are prepared.
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

// The reference characters, each prepared (resampled and fitted) the first time it is compared. All of them come to
// about 80,000 strokes, about 10 MB, and take about 80 ms to prepare in a debug build, which every ranking used to
// pay. They are kept for the database they were made from, and fude_match_release lets them go. Their arrays are on
// the std allocator, as everything that grows with the data is (coding_rules.md §3): the engine's pool is fixed.
// With them, the records that can be compared (1 to FUDE_MATCH_MAX_STROKES strokes) sorted by stroke count, so that a
// ranking goes only through the counts it compares, not every record: by_count holds a count's records, in record
// order, from count_first[count] to count_first[count + 1]. Made with the table, for the same database, and let go
// with it.
static struct {
    const u8*          records;    // the database they belong to (its record table)
    u32                count;      // and its record count: a plain number, which with records tells a changed database
    u32*               first;      // for each record: its first stroke in strokes
    u8*                ready;      // for each record: 0 not yet, 1 prepared, 2 none (no strokes, or too many)
    rde_vec_2F*        extent;     // for each record: min, max, in KanjiVG units, Y down, before the fit
    fude_match_stroke* strokes;
    rde_arr TYPE(u32)  by_count;   // the records by stroke count (on the std allocator: it grows with the data)
    // Where each stroke count's records start in by_count: one for each count a record compared can have (0 to
    // FUDE_MATCH_MAX_STROKES, 0 always empty), and one past the last.
    u32                count_first[FUDE_MATCH_MAX_STROKES + 2u];
} fude_match_refs;

// Lets the prepared characters go (see the header).
void fude_match_release(void) {
    if(fude_match_refs.first != NULL) {
        rde_memory_allocator* _std = rde_memory_allocator_get_default_std();
        _std->free(_std->allocator, fude_match_refs.first);
        _std->free(_std->allocator, fude_match_refs.ready);
        _std->free(_std->allocator, fude_match_refs.extent);
        _std->free(_std->allocator, fude_match_refs.strokes);
    }
    if(rde_arr_is_inited(&fude_match_refs.by_count)) {
        rde_arr_free(&fude_match_refs.by_count);
    }
    rde_memset(&fude_match_refs, 0, sizeof(fude_match_refs));
}

// Lays the table out for _db when it was made for another database (or none), dropping everything prepared: each
// record's first stroke, nothing prepared yet, and the records sorted by stroke count (a counting sort, which keeps
// the record order within a count). Every use of the table calls it first.
RDE_INTERNAL void fude_match_layout(const fude_kanji_db* _db) {
    if(fude_match_refs.records == _db->_records && fude_match_refs.count == _db->count) {
        return;
    }
    fude_match_release();
    rde_memory_allocator* _std = rde_memory_allocator_get_default_std();
    u32 _total = 0;
    fude_match_refs.first = (u32*)_std->malloc(_std->allocator, sizeof(u32) * (_db->count + 1u));
    for(u32 _r = 0; _r < _db->count; _r++) {
        fude_kanji_info _in;
        fude_match_refs.first[_r] = _total;
        if(fude_kanji_at(_db, _r, &_in) && _in.strokes <= FUDE_MATCH_MAX_STROKES) {
            _total += _in.strokes;
        }
    }
    fude_match_refs.first[_db->count] = _total;
    fude_match_refs.ready   = (u8*)_std->malloc(_std->allocator, _db->count + 1u);
    fude_match_refs.extent  = (rde_vec_2F*)_std->malloc(_std->allocator, sizeof(rde_vec_2F) * 2u * (_db->count + 1u));
    fude_match_refs.strokes = (fude_match_stroke*)_std->malloc(_std->allocator, sizeof(fude_match_stroke) * (_total + 1u));
    rde_memset(fude_match_refs.ready, 0, _db->count + 1u);
    fude_match_refs.records = _db->_records;
    fude_match_refs.count   = _db->count;

    // A record's strokes are the gap to the next one's first: 0 for one that cannot be read, has none or has too
    // many, which no ranking compares (it went through every record and skipped those).
    const u32* _first = fude_match_refs.first;
    u32*       _start = fude_match_refs.count_first;
    for(u32 _r = 0; _r < _db->count; _r++) {
        const u32 _m = _first[_r + 1u] - _first[_r];
        if(_m > 0u) {
            _start[_m + 1u]++;
        }
    }
    for(u32 _m = 0; _m <= FUDE_MATCH_MAX_STROKES; _m++) {
        _start[_m + 1u] += _start[_m];
    }
    fude_match_refs.by_count = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&fude_match_refs.by_count, _start[FUDE_MATCH_MAX_STROKES + 1u]);
    u32* _sorted = (u32*)fude_match_refs.by_count.memory;
    u32  _at[FUDE_MATCH_MAX_STROKES + 1u];
    rde_memcpy(_at, _start, sizeof(_at));
    for(u32 _r = 0; _r < _db->count; _r++) {
        const u32 _m = _first[_r + 1u] - _first[_r];
        if(_m > 0u) {
            _sorted[_at[_m]++] = _r;
        }
    }
}

// Returns a record's prepared strokes, or NULL when it has none, and its info in _info. The table is laid out again
// (and everything prepared dropped) when the database is another.
RDE_INTERNAL const fude_match_stroke* fude_match_prepared(const fude_kanji_db* _db, u32 _record, fude_kanji_info* _info) {
    fude_match_layout(_db);

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
                    _min = (rde_vec_2F){ rde_math_min_f32(_min.x, _out[_s].p[_k].x), rde_math_min_f32(_min.y, _out[_s].p[_k].y) };
                    _max = (rde_vec_2F){ rde_math_max_f32(_max.x, _out[_s].p[_k].x), rde_math_max_f32(_max.y, _out[_s].p[_k].y) };
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

// Gets a character's extent before the fit (see the header).
b8 fude_match_extent(const fude_kanji_db* _db, u32 _record, rde_vec_2F* _min, rde_vec_2F* _max) {
    fude_kanji_info _info;
    if(_db == NULL || fude_match_prepared(_db, _record, &_info) == NULL) {
        return false;
    }
    *_min = fude_match_refs.extent[_record * 2u];
    *_max = fude_match_refs.extent[_record * 2u + 1u];
    return true;
}

// The cost of the alignment, on a table of stroke distances _d (see match.h, step 3).
RDE_INTERNAL f32 fude_match_align(f32 _d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES], u32 _n, u32 _m) {
    static f32 _dp[FUDE_MATCH_MAX_STROKES + 1][FUDE_MATCH_MAX_STROKES + 1];

    // In order, with gaps for a missing or an extra stroke.
    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * FUDE_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * FUDE_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            const f32 _pair = _dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1];
            const f32 _skip = rde_math_min_f32(_dp[_i - 1][_j], _dp[_i][_j - 1]) + FUDE_MATCH_GAP;
            _dp[_i][_j] = rde_math_min_f32(_pair, _skip);
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);

    if(_n != _m) {
        return _ordered;
    }

    // Order-free: pair the closest strokes greedily.
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

    return rde_math_min_f32(_ordered, _unordered);
}

// The cost of one character against another (see match.h, step 3).
RDE_INTERNAL f32 fude_match_cost(const fude_match_stroke* _user, u32 _n, const fude_match_stroke* _ref, u32 _m) {
    static f32 _d[FUDE_MATCH_MAX_STROKES][FUDE_MATCH_MAX_STROKES];
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _j = 0; _j < _m; _j++) {
            _d[_i][_j] = fude_match_stroke_distance(&_user[_i], &_ref[_j]);
        }
    }
    return fude_match_align(_d, _n, _m);
}

// The rough cost: the same, on FUDE_MATCH_COARSE points per stroke, and, for the order-free pairing, each stroke's
// closest (a lower bound on any pairing). It is about a sixth of the work, and close enough to choose which few get
// the exact cost.
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
                _fwd += rde_math_sqrt_f32((_p.x - _q.x) * (_p.x - _q.x) + (_p.y - _q.y) * (_p.y - _q.y));
                _rev += rde_math_sqrt_f32((_p.x - _r.x) * (_p.x - _r.x) + (_p.y - _r.y) * (_p.y - _r.y));
            }
            _d[_i][_j] = rde_math_min_f32(_fwd / (f32)FUDE_MATCH_COARSE, _rev / (f32)FUDE_MATCH_COARSE + FUDE_MATCH_REVERSED);
            _row       = rde_math_min_f32(_row, _d[_i][_j]);
        }
        _closest += _row;
    }

    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * FUDE_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * FUDE_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            _dp[_i][_j] = rde_math_min_f32(_dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1], rde_math_min_f32(_dp[_i - 1][_j], _dp[_i][_j - 1]) + FUDE_MATCH_GAP);
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);
    return _n != _m ? _ordered : rde_math_min_f32(_ordered, _closest / (f32)_n + FUDE_MATCH_UNORDERED);
}

// A floor under the distance from _p to _q as fude_match_rough_cost works it out, with no square root: 0.99 of the
// octagon inside the circle, the largest of |dx|, |dy| and (|dx| + |dy|) × 0.7071 (under 1/√2). Rounding moves both
// by under a millionth, far inside the 1%. Under 1e-18 it is 0, since there the squares could round to nothing.
RDE_INTERNAL f32 fude_match_floor_term(rde_vec_2F _p, rde_vec_2F _q) {
    const f32 _dx = _p.x - _q.x, _dy = _p.y - _q.y;
    const f32 _ax = _dx < 0.0f ? -_dx : _dx, _ay = _dy < 0.0f ? -_dy : _dy;
    const f32 _ab = (_ax + _ay) * 0.7071f;
    const f32 _o  = _ax > _ay ? (_ax > _ab ? _ax : _ab) : (_ay > _ab ? _ay : _ab);
    return _o >= 1e-18f ? _o * 0.99f : 0.0f;
}

// A floor under fude_match_rough_cost, with no square root, for turning a candidate away before its cost is worked
// out. It is the rough cost's own working (the same table, alignment and closest sum, in the same order) on each point
// distance's floor (fude_match_floor_term). Each operation in it (adding, dividing by a count, the lesser of two)
// never falls as an operand grows, in floating point too (rounding keeps order), so from floors no more than the
// distances it never comes out over the rough cost: when it reaches the cost to beat, the rough cost would too, and the
// candidate is turned away as it would have been. It rules out about four in five, at about a third of the work of the
// cost in an optimised build: reading a sentence 1.2 to 2.1 times as fast. (Built without optimisation, its own
// arithmetic costs more than the square roots it saves, which are the engine's: reading is then two to three times as
// slow.) The lesser of two is written out, not the engine's call: it runs for every candidate, and on numbers that are
// never NaN (points in the box) it is the same lesser.
RDE_INTERNAL f32 fude_match_floor(const rde_vec_2F _user[][FUDE_MATCH_COARSE], u32 _n, const fude_match_stroke* _ref, u32 _m) {
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
                _fwd += fude_match_floor_term(_p, _ref[_j].p[_k * FUDE_MATCH_COARSE_STEP]);
                _rev += fude_match_floor_term(_p, _ref[_j].p[(FUDE_MATCH_COARSE - 1u - _k) * FUDE_MATCH_COARSE_STEP]);
            }
            const f32 _f = _fwd / (f32)FUDE_MATCH_COARSE, _r = _rev / (f32)FUDE_MATCH_COARSE + FUDE_MATCH_REVERSED;
            _d[_i][_j] = _f < _r ? _f : _r;
            _row       = _d[_i][_j] < _row ? _d[_i][_j] : _row;
        }
        _closest += _row;
    }

    _dp[0][0] = 0.0f;
    for(u32 _i = 1; _i <= _n; _i++) { _dp[_i][0] = (f32)_i * FUDE_MATCH_GAP; }
    for(u32 _j = 1; _j <= _m; _j++) { _dp[0][_j] = (f32)_j * FUDE_MATCH_GAP; }
    for(u32 _i = 1; _i <= _n; _i++) {
        for(u32 _j = 1; _j <= _m; _j++) {
            const f32 _pair = _dp[_i - 1][_j - 1] + _d[_i - 1][_j - 1];
            const f32 _skip = (_dp[_i - 1][_j] < _dp[_i][_j - 1] ? _dp[_i - 1][_j] : _dp[_i][_j - 1]) + FUDE_MATCH_GAP;
            _dp[_i][_j] = _pair < _skip ? _pair : _skip;
        }
    }
    const f32 _ordered = _dp[_n][_m] / (f32)(_n > _m ? _n : _m);
    const f32 _free    = _closest / (f32)_n + FUDE_MATCH_UNORDERED;
    return _n != _m || _ordered < _free ? _ordered : _free;
}

// True when record _r passes _filter. FUDE_MATCH_TEXT also lets through the marks written in text that the catalog
// leaves out (、。々・ー！？, ASCII's ! and ?, and the digits).
RDE_INTERNAL b8 fude_match_passes(const fude_kanji_db* _db, const fude_catalog* _catalog, u32 _r, FUDE_FILTER_ _filter, const fude_kanji_info* _info) {
    if(_filter != FUDE_MATCH_TEXT) {
        return fude_catalog_passes(_catalog, _r, _filter);
    }
    if(fude_catalog_passes(_catalog, _r, FUDE_FILTER_ALL)) {
        return true;
    }
    RDE_UNUSED(_db);
    switch(_info->codepoint) {
        case 0x3001: case 0x3002: case 0x3005: case 0x30FB: case 0x30FC: case 0xFF01: case 0xFF1F: case 0x0021: case 0x003F:
            return true;
        default:
            return _info->codepoint >= '0' && _info->codepoint <= '9';
    }
}

// Keeps the best _max results given so far in _list, sorted by cost (*_found of them).
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

// Returns what prepared strokes cost against one character (see the header).
f32 fude_match_cost_record(const fude_kanji_db* _db, u32 _record, const fude_match_stroke* _strokes, u32 _count) {
    fude_kanji_info          _info;
    const fude_match_stroke* _ref = _db != NULL ? fude_match_prepared(_db, _record, &_info) : NULL;
    if(_ref == NULL || _count == 0 || _count > FUDE_MATCH_MAX_STROKES) {
        return 1e30f;
    }
    return fude_match_cost(_strokes, _count, _ref, _info.strokes);
}

// Ranks the characters against prepared strokes (see the header): a rough pass over every character near the
// stroke count, then the exact cost of the closest (three times _max, from 64 to 256 of them).
//
// The rough pass goes through the records of each stroke count from _n - FUDE_MATCH_STROKE_SLACK to _n +
// FUDE_MATCH_STROKE_SLACK (fude_match_refs.by_count), not every record, taking them together in record order: each
// count's are in record order, so the next is the least of each count's next. That is the order going through every
// record took them in, so among equal costs the same ones are kept, in the same order (fude_match_keep keeps the first
// of equals). Once _keep are kept, a candidate whose floor (fude_match_floor) is no less than the worst kept is turned
// away without its cost: fude_match_keep would have turned it away (it keeps only a cost under the worst), so what is
// kept, and in what order, is as before.
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
    fude_match_layout(_db);
    const u32  _keep   = _max * 3u > FUDE_MATCH_KEEP ? (_max * 3u < FUDE_MATCH_KEEP * 4u ? _max * 3u : FUDE_MATCH_KEEP * 4u) : FUDE_MATCH_KEEP;
    const u32  _lo     = _n > FUDE_MATCH_STROKE_SLACK ? _n - FUDE_MATCH_STROKE_SLACK : 1u;
    const u32  _hi     = _n + FUDE_MATCH_STROKE_SLACK < FUDE_MATCH_MAX_STROKES ? _n + FUDE_MATCH_STROKE_SLACK : FUDE_MATCH_MAX_STROKES;
    const u32* _sorted = (const u32*)fude_match_refs.by_count.memory;
    const u32* _start  = fude_match_refs.count_first;
    u32        _next[FUDE_MATCH_STROKE_SLACK * 2u + 1u];   // each stroke count's next record, in _sorted
    u32        _kept_n = 0;
    for(u32 _m = _lo; _m <= _hi; _m++) {
        _next[_m - _lo] = _start[_m];
    }
    for(;;) {
        u32 _m = 0, _r = FUDE_MATCH_NO_RECORD;
        for(u32 _c = _lo; _c <= _hi; _c++) {
            if(_next[_c - _lo] < _start[_c + 1u] && _sorted[_next[_c - _lo]] < _r) {
                _m = _c;
                _r = _sorted[_next[_c - _lo]];
            }
        }
        if(_r == FUDE_MATCH_NO_RECORD) {
            break;
        }
        _next[_m - _lo]++;
        fude_kanji_info _info;
        if(!fude_kanji_at(_db, _r, &_info) || !fude_match_passes(_db, _catalog, _r, _filter, &_info)) {
            continue;
        }
        const fude_match_stroke* _ref = fude_match_prepared(_db, _r, &_info);
        if(_ref == NULL) {
            continue;
        }
        // (the kept full, one whose floor is no less than the worst kept would be turned away: fude_match_floor)
        if(_kept_n == _keep && fude_match_floor(_rough_user, _n, _ref, _m) >= _kept[_kept_n - 1u].cost) {
            continue;
        }
        fude_match_keep(_kept, &_kept_n, _keep, _r, fude_match_rough_cost(_rough_user, _n, _ref, _m));
    }

    // ...and the exact cost over the closest of those.
    u32 _found = 0;
    for(u32 _i = 0; _i < _kept_n; _i++) {
        fude_kanji_info          _info;
        const fude_match_stroke* _ref = fude_match_prepared(_db, _kept[_i].record, &_info);
        fude_match_keep(_out, &_found, _max, _kept[_i].record, fude_match_cost(_user, _n, _ref, _info.strokes));
    }
    return _found;
}

// Ranks the characters against a drawing (see the header).
u32 fude_match_rank(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                    const fude_ink* _drawing, fude_match_result* _out, u32 _max) {
    static fude_match_stroke _user[FUDE_MATCH_MAX_STROKES];

    if(_db == NULL || _max == 0) {
        return 0;
    }
    const u32 _n = fude_match_drawing(_drawing, _user, FUDE_MATCH_MAX_STROKES);
    return fude_match_rank_strokes(_db, _catalog, _filter, _user, _n, _out, _max);
}
