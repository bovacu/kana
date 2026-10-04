#include "study/handwriting/guide.h"
#include "drawing/base/text.h"
#include "study/handwriting/match.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See guide.h. Distances are KanjiVG units (the box is 109), between strokes
// resampled evenly (match.h) — in place, not fitted: the model is there to
// follow, so where the stroke goes counts.
// ===========================================================================

#define FUDE_GUIDE_CLOSE_TRACE  10.0f   // a traced stroke this close (mean distance) is right
#define FUDE_GUIDE_CLOSE_HINT   13.0f   // ...one written from its start dot alone
#define FUDE_GUIDE_OTHER        0.7f    // another stroke this much closer than the expected one: it was that one
#define FUDE_GUIDE_BACKWARDS    3.0f    // backwards must be closer by this to say so
#define FUDE_GUIDE_SHORT        12.0f   // strokes shorter than this: no arrow, no "backwards" (a dot has no way)
#define FUDE_GUIDE_PAUSE        1.2     // seconds a finished step shows its tick
#define FUDE_GUIDE_FADE         0.9     // seconds a wrong stroke takes to fade
#define FUDE_GUIDE_DEMO_DELAY   0.15    // before a stroke starts writing itself

RDE_INTERNAL rde_color fude_guide_alpha(rde_color _c, f32 _alpha) {
    _c.a = (u8)fmaxf(0.0f, fminf(255.0f, (f32)_c.a * _alpha));
    return _c;
}

// Reference stroke _index: its points (KanjiVG units, Y down) into _points
// (FUDE_GLYPH_MAX_POINTS), resampled into _resampled (may be NULL); how many
// points, 0 when there is no such stroke.
RDE_INTERNAL u32 fude_guide_reference(const fude_guide* _guide, u32 _index, fude_kanji_stroke* _stroke, rde_vec_2F* _points, f32* _length,
                                     fude_match_stroke* _resampled) {
    if(_index >= _guide->info.strokes || !fude_kanji_stroke_at(_guide->db, &_guide->info, _index, _stroke)) {
        return 0;
    }
    const u32 _n = fude_glyph_stroke_points(_stroke, _points, _length);
    if(_resampled != NULL && _n > 0) {
        fude_match_resample_points(_points, _n, _resampled);
    }
    return _n;
}

// The point _s along a polyline's length.
RDE_INTERNAL rde_vec_2F fude_guide_along(const rde_vec_2F* _p, u32 _n, f32 _s) {
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

void fude_guide_start(fude_guide* _guide, const fude_kanji_db* _db, const fude_kanji_info* _info, f64 _now) {
    memset(_guide, 0, sizeof(*_guide));
    _guide->db   = _db;
    _guide->info = *_info;
    fude_guide_restart(_guide, _now);
}

void fude_guide_restart(fude_guide* _guide, f64 _now) {
    _guide->next           = 0;
    _guide->misses         = 0;
    _guide->shown_at       = _now;
    _guide->stage_done_at  = 0.0;
    _guide->message[0]     = 0;
    _guide->rejected_count = 0;
}

RDE_INTERNAL void fude_guide_next_stage(fude_guide* _guide, f64 _now) {
    _guide->stage = _guide->stage + 1 < FUDE_GUIDE_STAGE_COUNT ? (FUDE_GUIDE_STAGE_)(_guide->stage + 1) : FUDE_GUIDE_RECALL;
    fude_guide_restart(_guide, _now);
}

b8 fude_guide_update(fude_guide* _guide, f64 _now) {
    if(_guide->stage_done_at > 0.0 && _now - _guide->stage_done_at >= FUDE_GUIDE_PAUSE) {
        fude_guide_next_stage(_guide, _now);
        return true;
    }
    return false;
}

b8 fude_guide_skip_pause(fude_guide* _guide, f64 _now) {
    if(_guide->stage_done_at > 0.0) {
        fude_guide_next_stage(_guide, _now);
        return true;
    }
    return false;
}

void fude_guide_took_back(fude_guide* _guide, f64 _now) {
    if(_guide->stage != FUDE_GUIDE_RECALL && _guide->next > 0) {
        _guide->next--;
        _guide->misses        = 0;
        _guide->shown_at      = _now;
        _guide->stage_done_at = 0.0;
        _guide->message[0]    = 0;
    }
}

FUDE_GUIDE_RESULT_ fude_guide_stroke(fude_guide* _guide, fude_ink* _ink, f32 _units, f64 _now) {
    const u32 _count = fude_ink_stroke_count(_ink);
    if(_count == 0) {
        return FUDE_GUIDE_KEPT;
    }

    // Recalling: nothing to check until it is all there.
    if(_guide->stage == FUDE_GUIDE_RECALL) {
        return fude_ink_alive_strokes(_ink) >= _guide->info.strokes ? FUDE_GUIDE_COMPLETE : FUDE_GUIDE_KEPT;
    }
    if(_guide->next >= _guide->info.strokes || _guide->stage_done_at > 0.0) {
        return FUDE_GUIDE_KEPT;   // (the step is done: the caller moved on before writing)
    }

    // The stroke, in KanjiVG's units (Y down), resampled.
    const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _count - 1u);
    const fude_ink_point*  _ip     = fude_ink_stroke_points(_ink, _stroke);
    static rde_vec_2F      _drawn_points[FUDE_GUIDE_MAX_POINTS];
    const u32              _n      = _stroke->point_count < FUDE_GUIDE_MAX_POINTS ? _stroke->point_count : FUDE_GUIDE_MAX_POINTS;
    const f32              _k      = FUDE_KANJI_BOX / _units;
    for(u32 _i = 0; _i < _n; _i++) {
        _drawn_points[_i] = (rde_vec_2F){ _ip[_i].position.x * _k, FUDE_KANJI_BOX - _ip[_i].position.y * _k };
    }
    fude_match_stroke _drawn;
    fude_match_resample_points(_drawn_points, _n > 0 ? _n : 1u, &_drawn);

    // Against the one expected, and the ones still to come.
    static rde_vec_2F  _ref_points[FUDE_GLYPH_MAX_POINTS];
    fude_kanji_stroke  _ref_stroke;
    fude_match_stroke  _ref;
    f32                _length = 0.0f;
    const f32          _close  = _guide->stage == FUDE_GUIDE_TRACE ? FUDE_GUIDE_CLOSE_TRACE : FUDE_GUIDE_CLOSE_HINT;
    if(fude_guide_reference(_guide, _guide->next, &_ref_stroke, _ref_points, &_length, &_ref) == 0) {
        return FUDE_GUIDE_KEPT;
    }
    const f32 _forward  = fude_match_points_distance(&_drawn, &_ref, false);
    const f32 _backward = fude_match_points_distance(&_drawn, &_ref, true);

    u32 _other      = UINT32_MAX;
    f32 _other_dist = _close;
    for(u32 _j = _guide->next + 1u; _j < _guide->info.strokes; _j++) {
        fude_kanji_stroke _s;
        fude_match_stroke _r;
        if(fude_guide_reference(_guide, _j, &_s, _ref_points, NULL, &_r) == 0) {
            continue;
        }
        const f32 _d = fminf(fude_match_points_distance(&_drawn, &_r, false), fude_match_points_distance(&_drawn, &_r, true));
        if(_d <= _other_dist) {
            _other      = _j;
            _other_dist = _d;
        }
    }

    const u32 _number = _guide->next + 1u;
    if(_other != UINT32_MAX && _other_dist < FUDE_GUIDE_OTHER * fminf(_forward, _close)) {
        FUDE_TEXTF(_guide->message, FUDE_TEXT_GUIDE_WRONG_STROKE, FUDE_TN(_other + 1u), FUDE_TN(_number));
    } else if(_length >= FUDE_GUIDE_SHORT && _backward + FUDE_GUIDE_BACKWARDS < _forward && _backward <= _close) {
        FUDE_TEXTF(_guide->message, FUDE_TEXT_GUIDE_BACKWARDS, FUDE_TN(_number));
    } else if(_forward <= _close) {
        // Right: it stays.
        _guide->next++;
        _guide->misses     = 0;
        _guide->shown_at   = _now;
        _guide->message[0] = 0;
        if(_guide->next >= _guide->info.strokes) {
            _guide->stage_done_at = _now;
        }
        return FUDE_GUIDE_KEPT;
    } else if(_guide->stage == FUDE_GUIDE_TRACE) {
        FUDE_TEXTF(_guide->message, FUDE_TEXT_GUIDE_FOLLOW, FUDE_TN(_number));
    } else {
        FUDE_TEXTF(_guide->message, FUDE_TEXT_GUIDE_WATCH, FUDE_TN(_number));
    }

    // Wrong: it flashes red and goes, and the stroke to write shows itself again.
    const f32 _back = _units / FUDE_KANJI_BOX;
    _guide->rejected_count = _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _guide->rejected[_i] = (rde_vec_2F){ _drawn_points[_i].x * _back, (FUDE_KANJI_BOX - _drawn_points[_i].y) * _back };
    }
    _guide->rejected_at = _now;
    _guide->misses++;
    _guide->shown_at = _now;
    fude_ink_undo(_ink);
    return FUDE_GUIDE_TAKEN_BACK;
}

// --- drawing -----------------------------------------------------------------------

RDE_INTERNAL rde_vec_2F fude_guide_screen(rde_vec_2F _tl, f32 _scale, rde_vec_2F _p) {
    return (rde_vec_2F){ _tl.x + _p.x * _scale, _tl.y - _p.y * _scale };
}

// An arrow beside the start of a stroke, the way it goes: along its first part,
// set off to one side so it does not cover where the pen goes.
RDE_INTERNAL void fude_guide_arrow(const rde_vec_2F* _p, u32 _n, f32 _length, rde_vec_2F _tl, f32 _scale, rde_color _color) {
    const f32  _s0     = fminf(_length * 0.12f, 6.0f);
    const f32  _s1     = fminf(_length * 0.65f, _s0 + 24.0f);
    const f32  _offset = 7.0f;
    const u32  _steps  = 10;
    rde_vec_2F _line[11];
    rde_vec_2F _tip_dir = { 1.0f, 0.0f };
    for(u32 _i = 0; _i <= _steps; _i++) {
        const f32  _s = _s0 + (_s1 - _s0) * (f32)_i / (f32)_steps;
        const rde_vec_2F _a = fude_guide_along(_p, _n, fmaxf(0.0f, _s - 1.5f));
        const rde_vec_2F _b = fude_guide_along(_p, _n, _s + 1.5f);
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
        const rde_vec_2F _at = fude_guide_along(_p, _n, _s);
        _line[_i] = fude_guide_screen(_tl, _scale, (rde_vec_2F){ _at.x - _dy * _offset, _at.y + _dx * _offset });
    }
    const f32 _width = fmaxf(1.2f, _scale * 0.7f);
    fude_draw_stroke_even(_line, _steps + 1u, _width, _color);

    // The head: two short lines back from the tip.
    const f32  _head = 4.5f * _scale;
    const rde_vec_2F _tip = _line[_steps];
    const rde_vec_2F _dir = { _tip_dir.x, -_tip_dir.y };   // on screen, Y up
    const f32 _c = cosf(2.6f), _s = sinf(2.6f);
    const rde_vec_2F _l = { _dir.x * _c - _dir.y * _s, _dir.x * _s + _dir.y * _c };
    const rde_vec_2F _r = { _dir.x * _c + _dir.y * _s, -_dir.x * _s + _dir.y * _c };
    fude_draw_line(_tip, (rde_vec_2F){ _tip.x + _l.x * _head, _tip.y + _l.y * _head }, _width, _color);
    fude_draw_line(_tip, (rde_vec_2F){ _tip.x + _r.x * _head, _tip.y + _r.y * _head }, _width, _color);
}

RDE_INTERNAL void fude_guide_render_steps(fude_guide* _guide, fude_glyph* _glyph, rde_vec_2F _tl, f32 _scale, f32 _pen, f64 _now);

void fude_guide_render(fude_guide* _guide, fude_glyph* _glyph, rde_vec_2F _tl, f32 _size, f64 _now) {
    const f32         _scale = _size / FUDE_KANJI_BOX;
    const f32         _pen   = FUDE_GLYPH_WIDTH * _scale * 0.5f;

    if(_guide->stage == FUDE_GUIDE_RECALL) {
        return;   // a blank square
    }
    // Never mirrored, as a character is (glyph.h).
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, false);
    fude_guide_render_steps(_guide, _glyph, _tl, _scale, _pen, _now);
    fude_draw_keep_end();
}

// fude_guide_render's character and the stroke to write, inside its kept box.
RDE_INTERNAL void fude_guide_render_steps(fude_guide* _guide, fude_glyph* _glyph, rde_vec_2F _tl, f32 _scale, f32 _pen, f64 _now) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _size  = _scale * FUDE_KANJI_BOX;

    // The character, faint (fainter at step 2).
    fude_glyph_character(_glyph, _guide->info.codepoint, _tl, _size, fude_guide_alpha(_theme->ghost, _guide->stage == FUDE_GUIDE_TRACE ? 1.0f : 0.5f));
    if(_guide->next >= _guide->info.strokes) {
        return;
    }

    static rde_vec_2F _points[FUDE_GLYPH_MAX_POINTS];
    fude_kanji_stroke _stroke;
    f32               _length = 0.0f;
    const u32         _n      = fude_guide_reference(_guide, _guide->next, &_stroke, _points, &_length, NULL);
    if(_n == 0) {
        return;
    }

    // Tracing, the stroke to write stands out from the rest.
    if(_guide->stage == FUDE_GUIDE_TRACE) {
        fude_glyph_stroke(_glyph, &_stroke, _tl, _scale, _pen, 1.0f, fude_guide_alpha(_theme->ink, 0.3f));
    }

    // It writes itself: tracing, whenever it becomes the one to write; with only
    // the dot, after a miss.
    if(_guide->stage == FUDE_GUIDE_TRACE || _guide->misses > 0) {
        const f64 _duration = FUDE_GLYPH_STROKE_BASE + FUDE_GLYPH_STROKE_PER * (f64)_length;
        const f64 _t        = (_now - _guide->shown_at - FUDE_GUIDE_DEMO_DELAY) / _duration;
        if(_t > 0.0 && _t < 1.0) {
            const rde_vec_2F _end = fude_glyph_stroke(_glyph, &_stroke, _tl, _scale, _pen, (f32)_t, fude_guide_alpha(_theme->pen_tip, 0.75f));
            rde_rendering_2d_draw_circle(fude_draw_at(_end), _pen * 1.4f, 20, _theme->pen_tip, NULL);
        }
    }

    // Where it starts, and (tracing) which way it goes.
    if(_guide->stage == FUDE_GUIDE_TRACE && _length >= FUDE_GUIDE_SHORT) {
        fude_guide_arrow(_points, _n, _length, _tl, _scale, fude_guide_alpha(_theme->pen_tip, 0.8f));
    }
    rde_rendering_2d_draw_circle(fude_draw_at(fude_guide_screen(_tl, _scale, _points[0])), fmaxf(4.0f, _pen * 1.6f), 24, _theme->pen_tip, NULL);
}

void fude_guide_render_over(fude_guide* _guide, rde_vec_2F _tl, f32 _size, f32 _units, f64 _now) {
    const fude_theme* _theme = fude_theme_active();
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, false);   // as fude_guide_render

    // The wrong stroke, red, fading out.
    const f64 _since = _now - _guide->rejected_at;
    if(_guide->rejected_count > 0 && _since < FUDE_GUIDE_FADE) {
        static rde_vec_2F _screen[FUDE_GUIDE_MAX_POINTS];
        const f32         _k = _size / _units;
        for(u32 _i = 0; _i < _guide->rejected_count; _i++) {
            _screen[_i] = (rde_vec_2F){ _tl.x + _guide->rejected[_i].x * _k, _tl.y - _size + _guide->rejected[_i].y * _k };
        }
        fude_draw_stroke_even(_screen, _guide->rejected_count, FUDE_GLYPH_WIDTH * _size / FUDE_KANJI_BOX * 0.5f,
                              fude_guide_alpha(_theme->score_poor, (f32)(1.0 - _since / FUDE_GUIDE_FADE)));
    }

    // A step done: a tick over the square.
    if(_guide->stage_done_at > 0.0) {
        const f32        _in = (f32)fmin(1.0, (_now - _guide->stage_done_at) / 0.25);
        const rde_vec_2F _c  = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
        const f32        _u  = _size * 0.22f;
        const rde_color  _g  = fude_guide_alpha(_theme->score_good, _in);
        const rde_vec_2F _a  = { _c.x - _u * 0.8f, _c.y + _u * 0.05f };
        const rde_vec_2F _b  = { _c.x - _u * 0.2f, _c.y - _u * 0.55f };
        const rde_vec_2F _e  = { _c.x + _u * 0.9f, _c.y + _u * 0.7f };
        fude_draw_line(_a, _b, _size * 0.022f, _g);
        fude_draw_line(_b, _e, _size * 0.022f, _g);
    }
    fude_draw_keep_end();
}

void fude_guide_prompt(const fude_guide* _guide, c8* _out, usize _size) {
    const u32 _total  = _guide->info.strokes;
    const u32 _number = _guide->next + 1u < _total ? _guide->next + 1u : _total;
    switch(_guide->stage) {
        case FUDE_GUIDE_TRACE:
            if(_guide->stage_done_at > 0.0) { snprintf(_out, _size, "%s", fude_text(FUDE_TEXT_GUIDE_STEP1_DONE)); }
            else                           { fude_text_format(_out, _size, FUDE_TEXT_GUIDE_STEP1, (const fude_text_arg[]){ FUDE_TN(_number), FUDE_TN(_total) }, 2u); }
            break;
        case FUDE_GUIDE_HINT:
            if(_guide->stage_done_at > 0.0) { snprintf(_out, _size, "%s", fude_text(FUDE_TEXT_GUIDE_STEP2_DONE)); }
            else                           { fude_text_format(_out, _size, FUDE_TEXT_GUIDE_STEP2, (const fude_text_arg[]){ FUDE_TN(_number), FUDE_TN(_total) }, 2u); }
            break;
        default:
            {
                c8 _strokes[48];
                FUDE_TEXTF(_strokes, FUDE_TEXT_STROKES_N, FUDE_TN(_total));
                fude_text_format(_out, _size, FUDE_TEXT_GUIDE_STEP3, (const fude_text_arg[]){ FUDE_TS(_strokes) }, 1u);
            }
            break;
    }
}
