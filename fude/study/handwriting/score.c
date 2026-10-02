#include "study/handwriting/score.h"
#include "drawing/base/text.h"
#include "study/handwriting/match.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See score.h.
// ===========================================================================

#define FUDE_SCORE_MAX  FUDE_MATCH_MAX_STROKES
#define FUDE_SCORE_INF  1e30f

// Minimum-cost assignment on an _n x _n matrix (Hungarian algorithm, O(n³)).
// _cost is row-major; _row_of_col[j] gets the row given column j.
RDE_INTERNAL void fude_score_assign(const f32* _cost, u32 _n, u32* _row_of_col) {
    // 1-based arrays, the classic potentials formulation.
    static f32 _u[FUDE_SCORE_MAX + 1], _v[FUDE_SCORE_MAX + 1], _minv[FUDE_SCORE_MAX + 1];
    static u32 _p[FUDE_SCORE_MAX + 1], _way[FUDE_SCORE_MAX + 1];
    static b8  _used[FUDE_SCORE_MAX + 1];

    for(u32 _i = 0; _i <= _n; _i++) {
        _u[_i] = _v[_i] = 0.0f;
        _p[_i] = _way[_i] = 0;
    }

    for(u32 _i = 1; _i <= _n; _i++) {
        _p[0]   = _i;
        u32 _j0 = 0;
        for(u32 _j = 0; _j <= _n; _j++) {
            _minv[_j] = FUDE_SCORE_INF;
            _used[_j] = false;
        }

        do {
            _used[_j0]   = true;
            const u32 _i0 = _p[_j0];
            f32       _delta = FUDE_SCORE_INF;
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
RDE_INTERNAL u32 fude_score_lis(const u32* _seq, u32 _n) {
    u32 _best[FUDE_SCORE_MAX];
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

// The best fit of the drawing onto the reference, given the pairs: per axis, a
// scale and a shift (least squares over every corresponding point), so SHAPE is
// judged without the character's placement, size or a little stretch — a 日 a
// bit wide, a ま whose tail runs on and so stretches the box both are fitted
// by. Kept honest: the axes stay within FUDE_SCORE_FIT_STRETCH of each other
// and the size within FUDE_SCORE_FIT_SIZE of the box fit, so a character can be
// moved and resized, not remade.
typedef struct {
    f32 sx, sy;   // scale
    f32 tx, ty;   // then shift
} fude_score_fit;

RDE_INTERNAL fude_score_fit fude_score_best_fit(const fude_match_stroke* _user, const fude_match_stroke* _ref, const u32* _ref_of_user,
                                                const b8* _backwards, u32 _n) {
    f64 _count = 0.0, _ux = 0.0, _uy = 0.0, _rx = 0.0, _ry = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_ref_of_user[_i] == UINT32_MAX) { continue; }
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            const rde_vec_2F _u = _user[_i].p[_k];
            const rde_vec_2F _r = _ref[_ref_of_user[_i]].p[_backwards[_i] ? FUDE_MATCH_POINTS - 1u - _k : _k];
            _ux += _u.x; _uy += _u.y; _rx += _r.x; _ry += _r.y; _count += 1.0;
        }
    }
    fude_score_fit _fit = { 1.0f, 1.0f, 0.0f, 0.0f };
    if(_count < 2.0) {
        return _fit;
    }
    _ux /= _count; _uy /= _count; _rx /= _count; _ry /= _count;

    f64 _vx = 0.0, _vy = 0.0, _cx = 0.0, _cy = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_ref_of_user[_i] == UINT32_MAX) { continue; }
        for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
            const rde_vec_2F _u = _user[_i].p[_k];
            const rde_vec_2F _r = _ref[_ref_of_user[_i]].p[_backwards[_i] ? FUDE_MATCH_POINTS - 1u - _k : _k];
            _vx += (_u.x - _ux) * (_u.x - _ux); _vy += (_u.y - _uy) * (_u.y - _uy);
            _cx += (_u.x - _ux) * (_r.x - _rx); _cy += (_u.y - _uy) * (_r.y - _ry);
        }
    }

    // One size for both axes first; then each axis may differ from it a little
    // (an axis with almost no extent — 一's height — keeps the shared size).
    const f64 _shared = (_vx + _vy) > 1e-6 ? (_cx + _cy) / (_vx + _vy) : 1.0;
    const f32 _size   = rde_math_clamp_f32((f32)_shared, 1.0f / FUDE_SCORE_FIT_SIZE, FUDE_SCORE_FIT_SIZE);
    const f64 _flat   = 0.05 * (_vx + _vy);
    const f32 _ax     = _vx > _flat ? (f32)(_cx / _vx) : _size;
    const f32 _ay     = _vy > _flat ? (f32)(_cy / _vy) : _size;
    _fit.sx = _size * rde_math_clamp_f32(_ax / _size, 1.0f / FUDE_SCORE_FIT_STRETCH, FUDE_SCORE_FIT_STRETCH);
    _fit.sy = _size * rde_math_clamp_f32(_ay / _size, 1.0f / FUDE_SCORE_FIT_STRETCH, FUDE_SCORE_FIT_STRETCH);
    _fit.tx = (f32)(_rx - (f64)_fit.sx * _ux);
    _fit.ty = (f32)(_ry - (f64)_fit.sy * _uy);
    return _fit;
}

// A drawn stroke's distance to its reference stroke once fitted.
RDE_INTERNAL f32 fude_score_fitted_distance(const fude_match_stroke* _user, const fude_match_stroke* _ref, b8 _backwards, fude_score_fit _fit) {
    f32 _sum = 0.0f;
    for(u32 _k = 0; _k < FUDE_MATCH_POINTS; _k++) {
        const rde_vec_2F _u  = _user->p[_k];
        const rde_vec_2F _r  = _ref->p[_backwards ? FUDE_MATCH_POINTS - 1u - _k : _k];
        const f32        _dx = _u.x * _fit.sx + _fit.tx - _r.x;
        const f32        _dy = _u.y * _fit.sy + _fit.ty - _r.y;
        _sum += sqrtf(_dx * _dx + _dy * _dy);
    }
    return _sum / (f32)FUDE_MATCH_POINTS;
}

void fude_score_describe(fude_score* _s) {
    // One line: the most important thing first.
    // In the learner's language (text.h), as it was when scored: history keeps the words.
    c8 _strokes[48];
    if(_s->empty) {
        snprintf(_s->feedback, sizeof(_s->feedback), "%s", fude_text(FUDE_TEXT_SCORE_EMPTY));
    } else if(_s->expected == 0) {
        snprintf(_s->feedback, sizeof(_s->feedback), "%s", fude_text(FUDE_TEXT_SCORE_NO_REFERENCE));
    } else if(_s->drawn != _s->expected) {
        FUDE_TEXTF(_strokes, FUDE_TEXT_STROKES_N, FUDE_TN(_s->drawn));
        FUDE_TEXTF(_s->feedback, FUDE_TEXT_SCORE_COUNT, FUDE_TS(_strokes), FUDE_TN(_s->expected));
    } else if(_s->misplaced > 0 && _s->swap_a != 0) {
        FUDE_TEXTF(_s->feedback, FUDE_TEXT_SCORE_ORDER_SWAP, FUDE_TN(_s->swap_a), FUDE_TN(_s->swap_b));
    } else if(_s->misplaced > 0) {
        FUDE_TEXTF(_strokes, FUDE_TEXT_STROKES_N, FUDE_TN(_s->misplaced));
        FUDE_TEXTF(_s->feedback, FUDE_TEXT_SCORE_ORDER_MISPLACED, FUDE_TS(_strokes));
    } else if(_s->reversed > 0) {
        FUDE_TEXTF(_s->feedback, _s->reversed > 1 ? FUDE_TEXT_SCORE_BACKWARDS_MORE : FUDE_TEXT_SCORE_BACKWARDS, FUDE_TN(_s->first_reversed));
    } else if(_s->shape < 70.0f && _s->worst != 0) {
        FUDE_TEXTF(_s->feedback, FUDE_TEXT_SCORE_SHAPE, FUDE_TN(_s->worst));
    } else {
        snprintf(_s->feedback, sizeof(_s->feedback), "%s", fude_text(_s->score >= 90.0f ? FUDE_TEXT_SCORE_EXCELLENT : _s->score >= 75.0f ? FUDE_TEXT_SCORE_GOOD : FUDE_TEXT_SCORE_OK));
    }
}

fude_score fude_score_drawing(const fude_kanji_db* _db, const fude_kanji_info* _info, const fude_ink* _drawing) {
    static fude_match_stroke _user[FUDE_SCORE_MAX];
    static fude_match_stroke _ref[FUDE_SCORE_MAX];
    static f32               _cost[FUDE_SCORE_MAX * FUDE_SCORE_MAX];
    static f32               _fwd[FUDE_SCORE_MAX][FUDE_SCORE_MAX];
    static f32               _rev[FUDE_SCORE_MAX][FUDE_SCORE_MAX];

    fude_score _s;
    memset(&_s, 0, sizeof(_s));

    const u32 _n = fude_match_drawing(_drawing, _user, FUDE_SCORE_MAX);
    const u32 _m = fude_match_reference(_db, _info, _ref, FUDE_SCORE_MAX);
    _s.drawn     = (u8)_n;
    _s.expected  = (u8)_m;

    if(_n == 0) {
        _s.empty = true;
        snprintf(_s.feedback, sizeof(_s.feedback), "%s", fude_text(FUDE_TEXT_SCORE_EMPTY));
        return _s;
    }
    if(_m == 0) {
        snprintf(_s.feedback, sizeof(_s.feedback), "%s", fude_text(FUDE_TEXT_SCORE_NO_REFERENCE));
        return _s;
    }

    // Pairing costs, padded square: a real pair costs its distance (backwards a
    // little more, so a forward reading wins a tie); "no partner" costs the gap.
    const u32 _size = _n > _m ? _n : _m;
    for(u32 _i = 0; _i < _size; _i++) {
        for(u32 _j = 0; _j < _size; _j++) {
            f32 _c = FUDE_SCORE_GAP;
            if(_i < _n && _j < _m) {
                _fwd[_i][_j] = fude_match_points_distance(&_user[_i], &_ref[_j], false);
                _rev[_i][_j] = fude_match_points_distance(&_user[_i], &_ref[_j], true);
                _c = fminf(_fwd[_i][_j], _rev[_i][_j] + 2.0f);
            }
            _cost[_i * _size + _j] = _c;
        }
    }

    u32 _row_of_col[FUDE_SCORE_MAX];
    fude_score_assign(_cost, _size, _row_of_col);

    // Read the pairing back as "which reference stroke did drawn stroke i become".
    u32 _ref_of_user[FUDE_SCORE_MAX];
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

    // Direction over the pairs, in writing order; the reference order they came
    // in, for the order check.
    u32 _sequence[FUDE_SCORE_MAX];
    b8  _backwards[FUDE_SCORE_MAX];
    u32 _pairs = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _j = _ref_of_user[_i];
        _backwards[_i] = false;
        if(_j == UINT32_MAX) {
            continue;
        }

        _backwards[_i] = _rev[_i][_j] + FUDE_SCORE_REVERSE_MARGIN < _fwd[_i][_j];
        if(_backwards[_i]) {
            _s.reversed++;
            if(_s.first_reversed == 0) {
                _s.first_reversed = (u8)(_j + 1u);
            }
        }
        _sequence[_pairs++] = _j;
    }

    // Shape: the pairs' distance once the drawing is fitted onto the reference.
    const fude_score_fit _fit     = fude_score_best_fit(_user, _ref, _ref_of_user, _backwards, _n);
    f32                  _total   = 0.0f;
    f32                  _worst_d = 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _j = _ref_of_user[_i];
        if(_j == UINT32_MAX) {
            continue;
        }
        const f32 _d = fude_score_fitted_distance(&_user[_i], &_ref[_j], _backwards[_i], _fit);
        if(_d > _worst_d) {
            _worst_d = _d;
            _s.worst = (u8)(_j + 1u);
        }
        _total += _d;
    }

    // Order: whatever is not on the longest in-order run was written out of place.
    _s.misplaced = (u8)(_pairs - fude_score_lis(_sequence, _pairs));
    for(u32 _k = 0; _k + 1u < _pairs && _s.misplaced > 0; _k++) {
        if(_sequence[_k] > _sequence[_k + 1u]) {
            _s.swap_a = (u8)(_sequence[_k + 1u] + 1u);
            _s.swap_b = (u8)(_sequence[_k] + 1u);
            break;
        }
    }

    const f32 _mean = _pairs > 0 ? _total / (f32)_pairs : FUDE_SCORE_SHAPE_ZERO;
    _s.shape = 100.0f * rde_math_clamp_f32(1.0f - (_mean - FUDE_SCORE_SHAPE_PERFECT) / (FUDE_SCORE_SHAPE_ZERO - FUDE_SCORE_SHAPE_PERFECT), 0.0f, 1.0f);
    _s.score = rde_math_clamp_f32(_s.shape
                                  - FUDE_SCORE_MISSING   * (f32)_s.missing
                                  - FUDE_SCORE_EXTRA     * (f32)_s.extra
                                  - FUDE_SCORE_MISPLACED * (f32)_s.misplaced
                                  - FUDE_SCORE_REVERSED  * (f32)_s.reversed, 0.0f, 100.0f);

    fude_score_describe(&_s);
    return _s;
}
