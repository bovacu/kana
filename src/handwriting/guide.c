#include "handwriting/guide.h"
#include "base/text.h"
#include "handwriting/match.h"
#include "widgets/draw.h"
#include "base/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See guide.h. Distances are KanjiVG units (the box is 109), between strokes
// resampled evenly (match.h) — in place, not fitted: the model is there to
// follow, so where the stroke goes counts.
// ===========================================================================

#define KANA_GUIDE_CLOSE_TRACE  10.0f   // a traced stroke this close (mean distance) is right
#define KANA_GUIDE_CLOSE_HINT   13.0f   // ...one written from its start dot alone
#define KANA_GUIDE_OTHER        0.7f    // another stroke this much closer than the expected one: it was that one
#define KANA_GUIDE_BACKWARDS    3.0f    // backwards must be closer by this to say so
#define KANA_GUIDE_SHORT        12.0f   // strokes shorter than this: no arrow, no "backwards" (a dot has no way)
#define KANA_GUIDE_PAUSE        1.2     // seconds a finished step shows its tick
#define KANA_GUIDE_FADE         0.9     // seconds a wrong stroke takes to fade
#define KANA_GUIDE_DEMO_DELAY   0.15    // before a stroke starts writing itself

RDE_INTERNAL rde_color kana_guide_alpha(rde_color _c, f32 _alpha) {
    _c.a = (u8)fmaxf(0.0f, fminf(255.0f, (f32)_c.a * _alpha));
    return _c;
}

// Reference stroke _index: its points (KanjiVG units, Y down) into _points
// (KANA_GLYPH_MAX_POINTS), resampled into _resampled (may be NULL); how many
// points, 0 when there is no such stroke.
RDE_INTERNAL u32 kana_guide_reference(const kana_guide* _guide, u32 _index, kana_kanji_stroke* _stroke, rde_vec_2F* _points, f32* _length,
                                     kana_match_stroke* _resampled) {
    if(_index >= _guide->info.strokes || !kana_kanji_stroke_at(_guide->db, &_guide->info, _index, _stroke)) {
        return 0;
    }
    const u32 _n = kana_glyph_stroke_points(_stroke, _points, _length);
    if(_resampled != NULL && _n > 0) {
        kana_match_resample_points(_points, _n, _resampled);
    }
    return _n;
}

// The point _s along a polyline's length.
RDE_INTERNAL rde_vec_2F kana_guide_along(const rde_vec_2F* _p, u32 _n, f32 _s) {
    for(u32 _i = 1; _i < _n; _i++) {
        const f32 _dx  = _p[_i].x - _p[_i - 1].x;
        const f32 _dy  = _p[_i].y - _p[_i - 1].y;
        const f32 _seg = sqrtf(_dx * _dx + _dy * _dy);
        if(_s <= _seg && _seg > 0.0f) {
            const f32 _t = _s / _seg;
            return (rde_vec_2F){ _p[_i - 1].x + _dx * _t, _p[_i - 1].y + _dy * _t };
        }
        _s -= _seg;
    }
    return _p[_n > 0 ? _n - 1 : 0];
}

void kana_guide_start(kana_guide* _guide, const kana_kanji_db* _db, const kana_kanji_info* _info, f64 _now) {
    memset(_guide, 0, sizeof(*_guide));
    _guide->db   = _db;
    _guide->info = *_info;
    kana_guide_restart(_guide, _now);
}

void kana_guide_restart(kana_guide* _guide, f64 _now) {
    _guide->next           = 0;
    _guide->misses         = 0;
    _guide->shown_at       = _now;
    _guide->stage_done_at  = 0.0;
    _guide->message[0]     = 0;
    _guide->rejected_count = 0;
}

RDE_INTERNAL void kana_guide_next_stage(kana_guide* _guide, f64 _now) {
    _guide->stage = _guide->stage + 1 < KANA_GUIDE_STAGE_COUNT ? (KANA_GUIDE_STAGE_)(_guide->stage + 1) : KANA_GUIDE_RECALL;
    kana_guide_restart(_guide, _now);
}

b8 kana_guide_update(kana_guide* _guide, f64 _now) {
    if(_guide->stage_done_at > 0.0 && _now - _guide->stage_done_at >= KANA_GUIDE_PAUSE) {
        kana_guide_next_stage(_guide, _now);
        return true;
    }
    return false;
}

b8 kana_guide_skip_pause(kana_guide* _guide, f64 _now) {
    if(_guide->stage_done_at > 0.0) {
        kana_guide_next_stage(_guide, _now);
        return true;
    }
    return false;
}

void kana_guide_took_back(kana_guide* _guide, f64 _now) {
    if(_guide->stage != KANA_GUIDE_RECALL && _guide->next > 0) {
        _guide->next--;
        _guide->misses        = 0;
        _guide->shown_at      = _now;
        _guide->stage_done_at = 0.0;
        _guide->message[0]    = 0;
    }
}

KANA_GUIDE_RESULT_ kana_guide_stroke(kana_guide* _guide, kana_ink* _ink, f32 _units, f64 _now) {
    const u32 _count = kana_ink_stroke_count(_ink);
    if(_count == 0) {
        return KANA_GUIDE_KEPT;
    }

    // Recalling: nothing to check until it is all there.
    if(_guide->stage == KANA_GUIDE_RECALL) {
        return kana_ink_alive_strokes(_ink) >= _guide->info.strokes ? KANA_GUIDE_COMPLETE : KANA_GUIDE_KEPT;
    }
    if(_guide->next >= _guide->info.strokes || _guide->stage_done_at > 0.0) {
        return KANA_GUIDE_KEPT;   // (the step is done: the caller moved on before writing)
    }

    // The stroke, in KanjiVG's units (Y down), resampled.
    const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _count - 1u);
    const kana_ink_point*  _ip     = kana_ink_stroke_points(_ink, _stroke);
    static rde_vec_2F      _drawn_points[KANA_GUIDE_MAX_POINTS];
    const u32              _n      = _stroke->point_count < KANA_GUIDE_MAX_POINTS ? _stroke->point_count : KANA_GUIDE_MAX_POINTS;
    const f32              _k      = KANA_KANJI_BOX / _units;
    for(u32 _i = 0; _i < _n; _i++) {
        _drawn_points[_i] = (rde_vec_2F){ _ip[_i].position.x * _k, KANA_KANJI_BOX - _ip[_i].position.y * _k };
    }
    kana_match_stroke _drawn;
    kana_match_resample_points(_drawn_points, _n > 0 ? _n : 1u, &_drawn);

    // Against the one expected, and the ones still to come.
    static rde_vec_2F  _ref_points[KANA_GLYPH_MAX_POINTS];
    kana_kanji_stroke  _ref_stroke;
    kana_match_stroke  _ref;
    f32                _length = 0.0f;
    const f32          _close  = _guide->stage == KANA_GUIDE_TRACE ? KANA_GUIDE_CLOSE_TRACE : KANA_GUIDE_CLOSE_HINT;
    if(kana_guide_reference(_guide, _guide->next, &_ref_stroke, _ref_points, &_length, &_ref) == 0) {
        return KANA_GUIDE_KEPT;
    }
    const f32 _forward  = kana_match_points_distance(&_drawn, &_ref, false);
    const f32 _backward = kana_match_points_distance(&_drawn, &_ref, true);

    u32 _other      = UINT32_MAX;
    f32 _other_dist = _close;
    for(u32 _j = _guide->next + 1u; _j < _guide->info.strokes; _j++) {
        kana_kanji_stroke _s;
        kana_match_stroke _r;
        if(kana_guide_reference(_guide, _j, &_s, _ref_points, NULL, &_r) == 0) {
            continue;
        }
        const f32 _d = fminf(kana_match_points_distance(&_drawn, &_r, false), kana_match_points_distance(&_drawn, &_r, true));
        if(_d <= _other_dist) {
            _other      = _j;
            _other_dist = _d;
        }
    }

    const u32 _number = _guide->next + 1u;
    if(_other != UINT32_MAX && _other_dist < KANA_GUIDE_OTHER * fminf(_forward, _close)) {
        KANA_TEXTF(_guide->message, KANA_TEXT_GUIDE_WRONG_STROKE, KANA_TN(_other + 1u), KANA_TN(_number));
    } else if(_length >= KANA_GUIDE_SHORT && _backward + KANA_GUIDE_BACKWARDS < _forward && _backward <= _close) {
        KANA_TEXTF(_guide->message, KANA_TEXT_GUIDE_BACKWARDS, KANA_TN(_number));
    } else if(_forward <= _close) {
        // Right: it stays.
        _guide->next++;
        _guide->misses     = 0;
        _guide->shown_at   = _now;
        _guide->message[0] = 0;
        if(_guide->next >= _guide->info.strokes) {
            _guide->stage_done_at = _now;
        }
        return KANA_GUIDE_KEPT;
    } else if(_guide->stage == KANA_GUIDE_TRACE) {
        KANA_TEXTF(_guide->message, KANA_TEXT_GUIDE_FOLLOW, KANA_TN(_number));
    } else {
        KANA_TEXTF(_guide->message, KANA_TEXT_GUIDE_WATCH, KANA_TN(_number));
    }

    // Wrong: it flashes red and goes, and the stroke to write shows itself again.
    const f32 _back = _units / KANA_KANJI_BOX;
    _guide->rejected_count = _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _guide->rejected[_i] = (rde_vec_2F){ _drawn_points[_i].x * _back, (KANA_KANJI_BOX - _drawn_points[_i].y) * _back };
    }
    _guide->rejected_at = _now;
    _guide->misses++;
    _guide->shown_at = _now;
    kana_ink_undo(_ink);
    return KANA_GUIDE_TAKEN_BACK;
}

// --- drawing -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F kana_guide_screen(rde_vec_2F _tl, f32 _scale, rde_vec_2F _p) {
    return (rde_vec_2F){ _tl.x + _p.x * _scale, _tl.y - _p.y * _scale };
}

// An arrow beside the start of a stroke, the way it goes: along its first part,
// set off to one side so it does not cover where the pen goes.
RDE_INTERNAL void kana_guide_arrow(const rde_vec_2F* _p, u32 _n, f32 _length, rde_vec_2F _tl, f32 _scale, rde_color _color) {
    const f32  _s0     = fminf(_length * 0.12f, 6.0f);
    const f32  _s1     = fminf(_length * 0.65f, _s0 + 24.0f);
    const f32  _offset = 7.0f;
    const u32  _steps  = 10;
    rde_vec_2F _line[11];
    rde_vec_2F _tip_dir = { 1.0f, 0.0f };
    for(u32 _i = 0; _i <= _steps; _i++) {
        const f32  _s = _s0 + (_s1 - _s0) * (f32)_i / (f32)_steps;
        const rde_vec_2F _a = kana_guide_along(_p, _n, fmaxf(0.0f, _s - 1.5f));
        const rde_vec_2F _b = kana_guide_along(_p, _n, _s + 1.5f);
        f32 _dx = _b.x - _a.x;
        f32 _dy = _b.y - _a.y;
        const f32 _d = sqrtf(_dx * _dx + _dy * _dy);
        if(_d > 0.0f) {
            _dx /= _d;
            _dy /= _d;
            _tip_dir = (rde_vec_2F){ _dx, _dy };
        } else {
            _dx = _tip_dir.x;
            _dy = _tip_dir.y;
        }
        const rde_vec_2F _at = kana_guide_along(_p, _n, _s);
        _line[_i] = kana_guide_screen(_tl, _scale, (rde_vec_2F){ _at.x - _dy * _offset, _at.y + _dx * _offset });
    }
    const f32 _width = fmaxf(1.2f, _scale * 0.7f);
    kana_draw_stroke_even(_line, _steps + 1u, _width, _color);

    // The head: two short lines back from the tip.
    const f32  _head = 4.5f * _scale;
    const rde_vec_2F _tip = _line[_steps];
    const rde_vec_2F _dir = { _tip_dir.x, -_tip_dir.y };   // on screen, Y up
    const f32 _c = cosf(2.6f), _s = sinf(2.6f);
    const rde_vec_2F _l = { _dir.x * _c - _dir.y * _s, _dir.x * _s + _dir.y * _c };
    const rde_vec_2F _r = { _dir.x * _c + _dir.y * _s, -_dir.x * _s + _dir.y * _c };
    kana_draw_line(_tip, (rde_vec_2F){ _tip.x + _l.x * _head, _tip.y + _l.y * _head }, _width, _color);
    kana_draw_line(_tip, (rde_vec_2F){ _tip.x + _r.x * _head, _tip.y + _r.y * _head }, _width, _color);
}

void kana_guide_render(kana_guide* _guide, kana_glyph* _glyph, rde_vec_2F _tl, f32 _size, f64 _now) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _scale = _size / KANA_KANJI_BOX;
    const f32         _pen   = KANA_GLYPH_WIDTH * _scale * 0.5f;

    if(_guide->stage == KANA_GUIDE_RECALL) {
        return;   // a blank square
    }

    // The character, faint (fainter at step 2).
    kana_glyph_character(_glyph, _guide->info.codepoint, _tl, _size, kana_guide_alpha(_theme->ghost, _guide->stage == KANA_GUIDE_TRACE ? 1.0f : 0.5f));
    if(_guide->next >= _guide->info.strokes) {
        return;
    }

    static rde_vec_2F _points[KANA_GLYPH_MAX_POINTS];
    kana_kanji_stroke _stroke;
    f32               _length = 0.0f;
    const u32         _n      = kana_guide_reference(_guide, _guide->next, &_stroke, _points, &_length, NULL);
    if(_n == 0) {
        return;
    }

    // Tracing, the stroke to write stands out from the rest.
    if(_guide->stage == KANA_GUIDE_TRACE) {
        kana_glyph_stroke(_glyph, &_stroke, _tl, _scale, _pen, 1.0f, kana_guide_alpha(_theme->ink, 0.3f));
    }

    // It writes itself: tracing, whenever it becomes the one to write; with only
    // the dot, after a miss.
    if(_guide->stage == KANA_GUIDE_TRACE || _guide->misses > 0) {
        const f64 _duration = KANA_GLYPH_STROKE_BASE + KANA_GLYPH_STROKE_PER * (f64)_length;
        const f64 _t        = (_now - _guide->shown_at - KANA_GUIDE_DEMO_DELAY) / _duration;
        if(_t > 0.0 && _t < 1.0) {
            const rde_vec_2F _end = kana_glyph_stroke(_glyph, &_stroke, _tl, _scale, _pen, (f32)_t, kana_guide_alpha(_theme->pen_tip, 0.75f));
            rde_rendering_2d_draw_circle(_end, _pen * 1.4f, 20, _theme->pen_tip, NULL);
        }
    }

    // Where it starts, and (tracing) which way it goes.
    if(_guide->stage == KANA_GUIDE_TRACE && _length >= KANA_GUIDE_SHORT) {
        kana_guide_arrow(_points, _n, _length, _tl, _scale, kana_guide_alpha(_theme->pen_tip, 0.8f));
    }
    rde_rendering_2d_draw_circle(kana_guide_screen(_tl, _scale, _points[0]), fmaxf(4.0f, _pen * 1.6f), 24, _theme->pen_tip, NULL);
}

void kana_guide_render_over(kana_guide* _guide, rde_vec_2F _tl, f32 _size, f32 _units, f64 _now) {
    const kana_theme* _theme = kana_theme_active();

    // The wrong stroke, red, fading out.
    const f64 _since = _now - _guide->rejected_at;
    if(_guide->rejected_count > 0 && _since < KANA_GUIDE_FADE) {
        static rde_vec_2F _screen[KANA_GUIDE_MAX_POINTS];
        const f32         _k = _size / _units;
        for(u32 _i = 0; _i < _guide->rejected_count; _i++) {
            _screen[_i] = (rde_vec_2F){ _tl.x + _guide->rejected[_i].x * _k, _tl.y - _size + _guide->rejected[_i].y * _k };
        }
        kana_draw_stroke_even(_screen, _guide->rejected_count, KANA_GLYPH_WIDTH * _size / KANA_KANJI_BOX * 0.5f,
                              kana_guide_alpha(_theme->score_poor, (f32)(1.0 - _since / KANA_GUIDE_FADE)));
    }

    // A step done: a tick over the square.
    if(_guide->stage_done_at > 0.0) {
        const f32        _in = (f32)fmin(1.0, (_now - _guide->stage_done_at) / 0.25);
        const rde_vec_2F _c  = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
        const f32        _u  = _size * 0.22f;
        const rde_color  _g  = kana_guide_alpha(_theme->score_good, _in);
        const rde_vec_2F _a  = { _c.x - _u * 0.8f, _c.y + _u * 0.05f };
        const rde_vec_2F _b  = { _c.x - _u * 0.2f, _c.y - _u * 0.55f };
        const rde_vec_2F _e  = { _c.x + _u * 0.9f, _c.y + _u * 0.7f };
        kana_draw_line(_a, _b, _size * 0.022f, _g);
        kana_draw_line(_b, _e, _size * 0.022f, _g);
    }
}

void kana_guide_prompt(const kana_guide* _guide, c8* _out, usize _size) {
    const u32 _total  = _guide->info.strokes;
    const u32 _number = _guide->next + 1u < _total ? _guide->next + 1u : _total;
    switch(_guide->stage) {
        case KANA_GUIDE_TRACE:
            if(_guide->stage_done_at > 0.0) { snprintf(_out, _size, "%s", kana_text(KANA_TEXT_GUIDE_STEP1_DONE)); }
            else                           { kana_text_format(_out, _size, KANA_TEXT_GUIDE_STEP1, (const kana_text_arg[]){ KANA_TN(_number), KANA_TN(_total) }, 2u); }
            break;
        case KANA_GUIDE_HINT:
            if(_guide->stage_done_at > 0.0) { snprintf(_out, _size, "%s", kana_text(KANA_TEXT_GUIDE_STEP2_DONE)); }
            else                           { kana_text_format(_out, _size, KANA_TEXT_GUIDE_STEP2, (const kana_text_arg[]){ KANA_TN(_number), KANA_TN(_total) }, 2u); }
            break;
        default:
            {
                c8 _strokes[48];
                KANA_TEXTF(_strokes, KANA_TEXT_STROKES_N, KANA_TN(_total));
                kana_text_format(_out, _size, KANA_TEXT_GUIDE_STEP3, (const kana_text_arg[]){ KANA_TS(_strokes) }, 1u);
            }
            break;
    }
}
