#include "score.h"
#include "match.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See score.h.
// ===========================================================================

#define KANA_SCORE_MAX  KANA_MATCH_MAX_STROKES
#define KANA_SCORE_INF  1e30f

// Minimum-cost assignment on an _n x _n matrix (Hungarian algorithm, O(n³)).
// _cost is row-major; _row_of_col[j] gets the row given column j.
RDE_INTERNAL void kana_score_assign(const f32* _cost, u32 _n, u32* _row_of_col) {
    // 1-based arrays, the classic potentials formulation.
    static f32 _u[KANA_SCORE_MAX + 1], _v[KANA_SCORE_MAX + 1], _minv[KANA_SCORE_MAX + 1];
    static u32 _p[KANA_SCORE_MAX + 1], _way[KANA_SCORE_MAX + 1];
    static b8  _used[KANA_SCORE_MAX + 1];

    for(u32 _i = 0; _i <= _n; _i++) {
        _u[_i] = _v[_i] = 0.0f;
        _p[_i] = _way[_i] = 0;
    }

    for(u32 _i = 1; _i <= _n; _i++) {
        _p[0]   = _i;
        u32 _j0 = 0;
        for(u32 _j = 0; _j <= _n; _j++) {
            _minv[_j] = KANA_SCORE_INF;
            _used[_j] = false;
        }

        do {
            _used[_j0]   = true;
            const u32 _i0 = _p[_j0];
            f32       _delta = KANA_SCORE_INF;
            u32       _j1    = 0;
            for(u32 _j = 1; _j <= _n; _j++) {
                if(_used[_j]) {
                    continue;
                }
                const f32 _cur = _cost[(_i0 - 1u) * _n + (_j - 1u)] - _u[_i0] - _v[_j];
                if(_cur < _minv[_j]) {
                    _minv[_j] = _cur;
                    _way[_j]  = _j0;
                }
                if(_minv[_j] < _delta) {
                    _delta = _minv[_j];
                    _j1    = _j;
                }
            }
            for(u32 _j = 0; _j <= _n; _j++) {
                if(_used[_j]) {
                    _u[_p[_j]] += _delta;
                    _v[_j]     -= _delta;
                } else {
                    _minv[_j] -= _delta;
                }
            }
            _j0 = _j1;
        } while(_p[_j0] != 0);

        do {
            const u32 _j1 = _way[_j0];
            _p[_j0] = _p[_j1];
            _j0     = _j1;
        } while(_j0 != 0);
    }

    for(u32 _j = 1; _j <= _n; _j++) {
        _row_of_col[_j - 1] = _p[_j] - 1u;
    }
}

// The length of the longest strictly increasing subsequence of _seq.
RDE_INTERNAL u32 kana_score_lis(const u32* _seq, u32 _n) {
    u32 _best[KANA_SCORE_MAX];
    u32 _len = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        u32 _lo = 0, _hi = _len;
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u;
            if(_best[_mid] < _seq[_i]) { _lo = _mid + 1u; } else { _hi = _mid; }
        }
        _best[_lo] = _seq[_i];
        if(_lo == _len) {
            _len++;
        }
    }
    return _len;
}

kana_score kana_score_drawing(const kana_kanji_db* _db, const kana_kanji_info* _info, const kana_ink* _drawing) {
    static kana_match_stroke _user[KANA_SCORE_MAX];
    static kana_match_stroke _ref[KANA_SCORE_MAX];
    static f32               _cost[KANA_SCORE_MAX * KANA_SCORE_MAX];
    static f32               _fwd[KANA_SCORE_MAX][KANA_SCORE_MAX];
    static f32               _rev[KANA_SCORE_MAX][KANA_SCORE_MAX];

    kana_score _s;
    memset(&_s, 0, sizeof(_s));

    const u32 _n = kana_match_drawing(_drawing, _user, KANA_SCORE_MAX);
    const u32 _m = kana_match_reference(_db, _info, _ref, KANA_SCORE_MAX);
    _s.drawn     = (u8)_n;
    _s.expected  = (u8)_m;

    if(_n == 0) {
        _s.empty = true;
        snprintf(_s.feedback, sizeof(_s.feedback), "Empty");
        return _s;
    }
    if(_m == 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "No reference for this character");
        return _s;
    }

    // Pairing costs, padded square: a real pair costs its distance (backwards a
    // little more, so a forward reading wins a tie); "no partner" costs the gap.
    const u32 _size = _n > _m ? _n : _m;
    for(u32 _i = 0; _i < _size; _i++) {
        for(u32 _j = 0; _j < _size; _j++) {
            f32 _c = KANA_SCORE_GAP;
            if(_i < _n && _j < _m) {
                _fwd[_i][_j] = kana_match_points_distance(&_user[_i], &_ref[_j], false);
                _rev[_i][_j] = kana_match_points_distance(&_user[_i], &_ref[_j], true);
                _c = fminf(_fwd[_i][_j], _rev[_i][_j] + 2.0f);
            }
            _cost[_i * _size + _j] = _c;
        }
    }

    u32 _row_of_col[KANA_SCORE_MAX];
    kana_score_assign(_cost, _size, _row_of_col);

    // Read the pairing back as "which reference stroke did drawn stroke i become".
    u32 _ref_of_user[KANA_SCORE_MAX];
    for(u32 _i = 0; _i < _size; _i++) {
        _ref_of_user[_i] = UINT32_MAX;
    }
    for(u32 _j = 0; _j < _size; _j++) {
        const u32 _i = _row_of_col[_j];
        if(_i < _n && _j < _m) {
            _ref_of_user[_i] = _j;
        } else if(_i >= _n && _j < _m) {
            _s.missing++;
        } else if(_i < _n && _j >= _m) {
            _s.extra++;
        }
    }

    // Shape and direction over the pairs, in writing order; the reference order
    // they came in, for the order check.
    u32 _sequence[KANA_SCORE_MAX];
    u32 _pairs     = 0;
    f32 _total     = 0.0f;
    f32 _worst_d   = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _j = _ref_of_user[_i];
        if(_j == UINT32_MAX) {
            continue;
        }

        const b8  _backwards = _rev[_i][_j] + KANA_SCORE_REVERSE_MARGIN < _fwd[_i][_j];
        const f32 _d         = _backwards ? _rev[_i][_j] : _fwd[_i][_j];
        if(_backwards) {
            _s.reversed++;
            if(_s.first_reversed == 0) {
                _s.first_reversed = (u8)(_j + 1u);
            }
        }
        if(_d > _worst_d) {
            _worst_d = _d;
            _s.worst = (u8)(_j + 1u);
        }

        _total += _d;
        _sequence[_pairs++] = _j;
    }

    // Order: whatever is not on the longest in-order run was written out of place.
    _s.misplaced = (u8)(_pairs - kana_score_lis(_sequence, _pairs));
    for(u32 _k = 0; _k + 1u < _pairs && _s.misplaced > 0; _k++) {
        if(_sequence[_k] > _sequence[_k + 1u]) {
            _s.swap_a = (u8)(_sequence[_k + 1u] + 1u);
            _s.swap_b = (u8)(_sequence[_k] + 1u);
            break;
        }
    }

    const f32 _mean = _pairs > 0 ? _total / (f32)_pairs : KANA_SCORE_SHAPE_ZERO;
    _s.shape = 100.0f * rde_math_clamp_f32(1.0f - (_mean - KANA_SCORE_SHAPE_PERFECT) / (KANA_SCORE_SHAPE_ZERO - KANA_SCORE_SHAPE_PERFECT), 0.0f, 1.0f);
    _s.score = rde_math_clamp_f32(_s.shape
                                  - KANA_SCORE_MISSING   * (f32)_s.missing
                                  - KANA_SCORE_EXTRA     * (f32)_s.extra
                                  - KANA_SCORE_MISPLACED * (f32)_s.misplaced
                                  - KANA_SCORE_REVERSED  * (f32)_s.reversed, 0.0f, 100.0f);

    // One line: the most important thing first.
    if(_n != _m) {
        snprintf(_s.feedback, sizeof(_s.feedback), "%u stroke%s, should be %u", _n, _n == 1 ? "" : "s", _m);
    } else if(_s.misplaced > 0 && _s.swap_a != 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "Order: stroke %u before %u", _s.swap_a, _s.swap_b);
    } else if(_s.misplaced > 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "Order: %u stroke%s out of place", _s.misplaced, _s.misplaced == 1 ? "" : "s");
    } else if(_s.reversed > 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "Stroke %u drawn backwards%s", _s.first_reversed, _s.reversed > 1 ? " (and more)" : "");
    } else if(_s.shape < 70.0f && _s.worst != 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "Stroke %u's shape is off", _s.worst);
    } else {
        snprintf(_s.feedback, sizeof(_s.feedback), "%s", _s.score >= 90.0f ? "Excellent" : _s.score >= 75.0f ? "Good" : "OK, keep practising");
    }

    return _s;
}
