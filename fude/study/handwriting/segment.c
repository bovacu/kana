// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/handwriting/segment.h"
#include "lang/lang.h"

// ===========================================================================
// See segment.h.
//
// The work of one read (fude_segment_work) holds each stroke prepared, its line, each line's H and where it starts
// across, the pieces, and the room before each stroke. Step 4 is dynamic programming over the pieces: best[p] is the
// cheapest reading of pieces 0 .. p-1, a run never crosses a line, and each run is ranked by the matcher. Reading as
// a text is dynamic programming over (characters, strokes) instead. What the language says (the script around a
// character, the letters at two sizes, the marks that change a letter, the look-alikes across scripts, which
// characters say where a line is) is its own, asked of lang.h (fude_lang_writing): the steps here only apply it, and
// a rule the language does not have is left out.
// ===========================================================================

#define FUDE_SEGMENT_TALL       1.3f     // AUTO: down when the whole is this much taller than wide
#define FUDE_SEGMENT_SPAN       1.4f     // a character spans at most this × H along the line
#define FUDE_SEGMENT_CUT        0.2f     // a cut: the stroke starts past the piece's far end, less this × H
#define FUDE_SEGMENT_MAX_PIECES 8u       // the most pieces in one character
#define FUDE_SEGMENT_MAX_RUN    40u      // the most strokes in one character (the most in the data is about 30)
#ifndef FUDE_SEGMENT_PER_CHAR
#define FUDE_SEGMENT_PER_CHAR   12.0f    // paid per character read, so that the pieces of one character stay together
#endif
// The gaps along the line (segment.h): a character is written tight and there is room between characters, so a run
// joined across a gap pays for it, and a cut where the strokes (nearly) touch pays for that. Both are measured against
// the line's own room between characters, because some people write tight and some fill a grid. The line holds
// about (its length / H) characters, so about that many of its widest gaps are between characters, and the room is
// their median (never under FUDE_SEGMENT_ROOM_MIN × H). One character alone has no room between characters, so there
// is nothing to measure a join against.
#ifndef FUDE_SEGMENT_JOIN
#define FUDE_SEGMENT_JOIN       60.0f    // paid per room of gap past FUDE_SEGMENT_JOIN_FREE
#endif
#ifndef FUDE_SEGMENT_JOIN_FREE
// The gap, in rooms, that a run may join across for free.
#define FUDE_SEGMENT_JOIN_FREE  0.3f
#endif
#ifndef FUDE_SEGMENT_JOIN_CAP
#define FUDE_SEGMENT_JOIN_CAP   45.0f    // a run that has paid this much for its gaps goes no further (for speed)
#endif
#ifndef FUDE_SEGMENT_SPLIT
#define FUDE_SEGMENT_SPLIT      40.0f    // paid per room of gap under FUDE_SEGMENT_SPLIT_FREE
#endif
#ifndef FUDE_SEGMENT_SPLIT_FREE
// A cut across a gap of at least this many rooms is free.
#define FUDE_SEGMENT_SPLIT_FREE 0.5f
#endif
#ifndef FUDE_SEGMENT_ROOM_MIN
// The least room between characters, in H.
#define FUDE_SEGMENT_ROOM_MIN   0.1f
#endif
#define FUDE_SEGMENT_SIZE_SAMPLE 64u      // the strokes whose median size says how big the hand writes (the first ones)
#define FUDE_SEGMENT_GAP_MIN    -0.3f    // a stroke inside the piece so far is as touching as it gets (in H)
#define FUDE_SEGMENT_SIZE       8.0f     // the size term's weight
#define FUDE_SEGMENT_PLACE      8.0f     // the place term's weight (across only)
#ifndef FUDE_SEGMENT_RARE
#define FUDE_SEGMENT_RARE       20.0f    // a kanji in neither the Jouyou nor the Jinmeiyou (丶, 刂: radicals)
#endif
#ifndef FUDE_SEGMENT_FREQ
#define FUDE_SEGMENT_FREQ       3.0f     // × ln(1 + rank / 100): kana are commoner in text than any kanji
#endif
#ifndef FUDE_SEGMENT_RANKED
#define FUDE_SEGMENT_RANKED     16u      // the matcher's results per run, before the size term
#endif
#define FUDE_SEGMENT_CELL_TOP   12.0f    // KanjiVG's box: where a full-height character starts
#define FUDE_SEGMENT_CELL       85.0f    // ...and how tall it is
#define FUDE_SEGMENT_AS_SLACK   3u       // reading as: a run within this many strokes of its character's count
#define FUDE_SEGMENT_TWIN_MARGIN 3.0f    // a look-alike this close (per stroke) may be chosen by its neighbours
#ifndef FUDE_SEGMENT_STROKE_DIFF
#define FUDE_SEGMENT_STROKE_DIFF 10.0f   // per stroke written but not in the character, or the other way round
#endif
#define FUDE_SEGMENT_VOICE_COST 4.0f     // per mark stroke, when the rest is read as the base character (lang.h: voiced)
#define FUDE_SEGMENT_MARK_GAP   0.25f    // a mark (lang.h: mark, such as 、。・) is followed by at least this × H of nothing
#define FUDE_SEGMENT_MARK_COST  20.0f    // added to a mark that is not
// A cost larger than any real one.
#define FUDE_SEGMENT_INF        1e30f

// A stroke as read: resampled, and its extent.
RDE_STRUCT {
    fude_match_stroke raw;          // resampled, Y down, in the drawing's units, not fitted
    rde_vec_2F        min;          // its extent, Y up
    rde_vec_2F        max;
    f32               a0, a1;       // its extent along the line
    f32               c0, c1;       // ...and across it (growing towards the next line)
} fude_segment_stroke;

// The work of one read (see the top).
RDE_STRUCT {
    fude_segment_stroke* strokes;
    u32                  count;
    b8                   vertical;
    u32*                 line_of;    // for each stroke: its line
    u32                  lines;
    f32*                 line_h;     // for each line: H, and where it starts across
    f32*                 line_c0;
    u32*                 pieces;     // the first stroke of each piece; pieces[piece_count] = count
    u32                  piece_count;
    f32*                 gap;        // per stroke: the room before it, in rooms (a line's first: INF)
} fude_segment_work;

// Sets up an empty reading (see the header).
void fude_segment_init(fude_segment* _segment) {
    rde_memset(_segment, 0, sizeof(*_segment));
    _segment->chars = rde_arr_new(sizeof(fude_segment_char), rde_memory_allocator_get_default());
}

// Frees a reading (see the header).
void fude_segment_destroy(fude_segment* _segment) {
    if(rde_arr_is_inited(&_segment->chars)) {
        rde_arr_free(&_segment->chars);
    }
    rde_memset(_segment, 0, sizeof(*_segment));
}

// Frees the work of one read.
RDE_INTERNAL void fude_segment_work_free(fude_segment_work* _work) {
    rde_free(_work->strokes);
    rde_free(_work->line_of);
    rde_free(_work->line_h);
    rde_free(_work->line_c0);
    rde_free(_work->pieces);
    rde_free(_work->gap);
    rde_memset(_work, 0, sizeof(*_work));
}

// Steps 1 to 3 (segment.h): the strokes prepared, the direction, the lines and the pieces. Returns false when there
// is nothing to read. Strokes of more than 1024 points are cut.
RDE_INTERNAL b8 fude_segment_prepare(fude_segment_work* _work, const fude_ink* _drawing, FUDE_SEGMENT_DIRECTION_ _direction) {
    static rde_vec_2F _pts[1024];
    rde_memset(_work, 0, sizeof(*_work));
    const u32 _total = fude_ink_alive_strokes(_drawing);
    if(_total == 0) {
        return false;
    }
    _work->strokes = (fude_segment_stroke*)rde_malloc(sizeof(fude_segment_stroke) * _total);
    _work->line_of = (u32*)rde_malloc(sizeof(u32) * _total);
    _work->line_h  = (f32*)rde_malloc(sizeof(f32) * _total);
    _work->line_c0 = (f32*)rde_malloc(sizeof(f32) * _total);
    _work->pieces  = (u32*)rde_malloc(sizeof(u32) * (_total + 1u));
    _work->gap     = (f32*)rde_malloc(sizeof(f32) * _total);

    rde_vec_2F _all_min = { FUDE_SEGMENT_INF, FUDE_SEGMENT_INF };
    rde_vec_2F _all_max = { -FUDE_SEGMENT_INF, -FUDE_SEGMENT_INF };
    for(u32 _i = 0; _i < fude_ink_stroke_count(_drawing) && _work->count < _total; _i++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_drawing, _i);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        fude_segment_stroke*  _s = &_work->strokes[_work->count++];
        const fude_ink_point* _p = fude_ink_stroke_points(_drawing, _stroke);
        const u32             _n = _stroke->point_count < 1024u ? _stroke->point_count : 1024u;
        _s->min = (rde_vec_2F){ FUDE_SEGMENT_INF, FUDE_SEGMENT_INF };
        _s->max = (rde_vec_2F){ -FUDE_SEGMENT_INF, -FUDE_SEGMENT_INF };
        for(u32 _k = 0; _k < _n; _k++) {
            _pts[_k] = (rde_vec_2F){ _p[_k].position.x, -_p[_k].position.y };
            _s->min  = (rde_vec_2F){ rde_math_min_f32(_s->min.x, _p[_k].position.x), rde_math_min_f32(_s->min.y, _p[_k].position.y) };
            _s->max  = (rde_vec_2F){ rde_math_max_f32(_s->max.x, _p[_k].position.x), rde_math_max_f32(_s->max.y, _p[_k].position.y) };
        }
        fude_match_resample_points(_pts, _n, &_s->raw);
        _all_min = (rde_vec_2F){ rde_math_min_f32(_all_min.x, _s->min.x), rde_math_min_f32(_all_min.y, _s->min.y) };
        _all_max = (rde_vec_2F){ rde_math_max_f32(_all_max.x, _s->max.x), rde_math_max_f32(_all_max.y, _s->max.y) };
    }
    if(_work->count == 0) {
        return false;
    }

    // 1. The direction, and each stroke along and across it.
    _work->vertical = _direction == FUDE_SEGMENT_DOWN ||
                      (_direction == FUDE_SEGMENT_AUTO && _all_max.y - _all_min.y > FUDE_SEGMENT_TALL * (_all_max.x - _all_min.x));
    // The first strokes' sizes, for their median below. A sample is all a median needs: 64 strokes are a few lines'
    // worth, and the writing's hand does not change size after them, so a fixed sample keeps it to the stack.
    f32 _sizes[FUDE_SEGMENT_SIZE_SAMPLE];
    u32 _sized = 0;
    for(u32 _i = 0; _i < _work->count; _i++) {
        fude_segment_stroke* _s = &_work->strokes[_i];
        if(_work->vertical) {
            _s->a0 = -_s->max.y; _s->a1 = -_s->min.y;   // along: downwards
            _s->c0 = -_s->max.x; _s->c1 = -_s->min.x;   // across: columns go right to left
        } else {
            _s->a0 = _s->min.x;  _s->a1 = _s->max.x;
            _s->c0 = -_s->max.y; _s->c1 = -_s->min.y;   // across: lines go down
        }
        if(_sized < FUDE_SEGMENT_SIZE_SAMPLE) {
            _sizes[_sized++] = rde_math_max_f32(_s->a1 - _s->a0, _s->c1 - _s->c0);
        }
    }
    // A stroke's typical size (the median of the first FUDE_SEGMENT_SIZE_SAMPLE strokes) is the floor under a line's
    // H, so that a line of flat strokes (一二三) is not read as a sliver.
    for(u32 _i = 1; _i < _sized; _i++) {
        const f32 _v = _sizes[_i];
        u32       _j = _i;
        while(_j > 0 && _sizes[_j - 1] > _v) { _sizes[_j] = _sizes[_j - 1]; _j--; }
        _sizes[_j] = _v;
    }
    const f32 _unit = rde_math_max_f32(_sizes[_sized / 2u], 1e-3f);

    // 2. Lines: a stroke wholly past the line, back towards its start, begins the next. A line is taken as at least two
    // strokes tall for this, so that a flat first stroke (the 一 over 雨) is not a line of its own.
    f32 _A0 = _work->strokes[0].a0, _A1 = _work->strokes[0].a1;
    f32 _C0 = _work->strokes[0].c0, _C1 = _work->strokes[0].c1;
    _work->lines      = 1;
    _work->line_of[0] = 0;
    for(u32 _i = 1; _i < _work->count; _i++) {
        const fude_segment_stroke* _s    = &_work->strokes[_i];
        const f32                  _h    = rde_math_max_f32(_C1 - _C0, 2.0f * _unit);
        const b8                   _past = _s->c0 > _C1 - 0.1f * _h;
        const b8                   _back = _s->a1 < _A1 - _h;
        if(_past && _back) {
            _work->line_h[_work->lines - 1u]  = rde_math_max_f32(_C1 - _C0, 1.2f * _unit);
            _work->line_c0[_work->lines - 1u] = _C0;
            _work->lines++;
            _A0 = _s->a0; _A1 = _s->a1; _C0 = _s->c0; _C1 = _s->c1;
        } else {
            _A0 = rde_math_min_f32(_A0, _s->a0); _A1 = rde_math_max_f32(_A1, _s->a1);
            _C0 = rde_math_min_f32(_C0, _s->c0); _C1 = rde_math_max_f32(_C1, _s->c1);
        }
        _work->line_of[_i] = _work->lines - 1u;
    }
    _work->line_h[_work->lines - 1u]  = rde_math_max_f32(_C1 - _C0, 1.2f * _unit);
    _work->line_c0[_work->lines - 1u] = _C0;

    // 3. Pieces: a stroke clear of the piece so far (in the writing direction) makes a cut.
    f32 _far = 0.0f;
    for(u32 _i = 0; _i < _work->count; _i++) {
        const fude_segment_stroke* _s     = &_work->strokes[_i];
        const b8                   _first = _i == 0 || _work->line_of[_i] != _work->line_of[_i - 1u];
        const f32                  _h     = _work->line_h[_work->line_of[_i]];
        _work->gap[_i] = _first ? FUDE_SEGMENT_INF : rde_math_max_f32((_s->a0 - _far) / _h, FUDE_SEGMENT_GAP_MIN);
        if(_first || _s->a0 >= _far - FUDE_SEGMENT_CUT * _h) {
            _work->pieces[_work->piece_count++] = _i;
            _far = _s->a1;
        } else {
            _far = rde_math_max_f32(_far, _s->a1);
        }
    }
    _work->pieces[_work->piece_count] = _work->count;

    // Each line's room between characters (in H), then every gap in rooms. At most 256 gaps a line are looked at: a
    // line written by hand on a page holds far fewer characters, and the room is a median, which more would not move.
    for(u32 _line = 0; _line < _work->lines; _line++) {
        f32 _gaps[256];
        u32 _n  = 0;
        f32 _a0 = FUDE_SEGMENT_INF, _a1 = -FUDE_SEGMENT_INF;
        for(u32 _i = 0; _i < _work->count; _i++) {
            if(_work->line_of[_i] == _line) {
                _a0 = rde_math_min_f32(_a0, _work->strokes[_i].a0);
                _a1 = rde_math_max_f32(_a1, _work->strokes[_i].a1);
            }
        }
        for(u32 _p = 0; _p < _work->piece_count && _n < 256u; _p++) {
            const u32 _s = _work->pieces[_p];
            if(_work->line_of[_s] == _line && _work->gap[_s] < FUDE_SEGMENT_INF && _work->gap[_s] > 0.0f) {
                _gaps[_n++] = _work->gap[_s];
            }
        }
        for(u32 _i = 1; _i < _n; _i++) {   // widest first
            const f32 _v = _gaps[_i];
            u32       _j = _i;
            while(_j > 0 && _gaps[_j - 1u] < _v) { _gaps[_j] = _gaps[_j - 1u]; _j--; }
            _gaps[_j] = _v;
        }
        const u32 _chars   = (u32)rde_math_floor_f32((_a1 - _a0) / _work->line_h[_line] + 0.5f);
        const u32 _between = _chars > 1u ? (_chars - 1u < _n ? _chars - 1u : _n) : 0u;
        const f32 _room    = _between > 0 ? rde_math_max_f32(_gaps[_between / 2u], FUDE_SEGMENT_ROOM_MIN) : 1e6f;
        for(u32 _i = 0; _i < _work->count; _i++) {
            if(_work->line_of[_i] == _line && _work->gap[_i] < FUDE_SEGMENT_INF) {
                _work->gap[_i] /= _room;
            }
        }
    }
    return true;
}

// Returns what a cut before stroke _s costs (nothing at a line's start).
RDE_INTERNAL f32 fude_segment_split_cost(const fude_segment_work* _work, u32 _s) {
    return _work->gap[_s] >= FUDE_SEGMENT_INF ? 0.0f : FUDE_SEGMENT_SPLIT * rde_math_max_f32(0.0f, FUDE_SEGMENT_SPLIT_FREE - _work->gap[_s]);
}

// Returns what joining a run across the gap before stroke _s costs.
RDE_INTERNAL f32 fude_segment_join_cost(const fude_segment_work* _work, u32 _s) {
    return FUDE_SEGMENT_JOIN * rde_math_max_f32(0.0f, _work->gap[_s] - FUDE_SEGMENT_JOIN_FREE);
}

// Finds the extent of strokes _first .. _first + _n - 1: Y up, and along and across the line.
RDE_INTERNAL void fude_segment_extent(const fude_segment_work* _work, u32 _first, u32 _n, rde_vec_2F* _min, rde_vec_2F* _max,
                                      f32* _a0, f32* _a1, f32* _c0, f32* _c1) {
    *_min = (rde_vec_2F){ FUDE_SEGMENT_INF, FUDE_SEGMENT_INF };
    *_max = (rde_vec_2F){ -FUDE_SEGMENT_INF, -FUDE_SEGMENT_INF };
    *_a0  = FUDE_SEGMENT_INF; *_a1 = -FUDE_SEGMENT_INF;
    *_c0  = FUDE_SEGMENT_INF; *_c1 = -FUDE_SEGMENT_INF;
    for(u32 _i = _first; _i < _first + _n; _i++) {
        const fude_segment_stroke* _s = &_work->strokes[_i];
        *_min = (rde_vec_2F){ rde_math_min_f32(_min->x, _s->min.x), rde_math_min_f32(_min->y, _s->min.y) };
        *_max = (rde_vec_2F){ rde_math_max_f32(_max->x, _s->max.x), rde_math_max_f32(_max->y, _s->max.y) };
        *_a0  = rde_math_min_f32(*_a0, _s->a0); *_a1 = rde_math_max_f32(*_a1, _s->a1);
        *_c0  = rde_math_min_f32(*_c0, _s->c0); *_c1 = rde_math_max_f32(*_c1, _s->c1);
    }
}

// The size-and-place term (segment.h) of reading a run as _record, and how often that is written: a character from
// U+3400 up costs more the rarer it is (its newspaper rank), and one outside the Jouyou and Jinmeiyou lists (grade 0:
// a radical like 刂) more again, as does a letter the language says is out of use (lang.h: rare; ヺ, ゐ). It is what
// tells す from 十 when the hand is loose. A script's letters (lang.h: FUDE_LANG_GROUP_SCRIPT; kana, Korean's
// syllables and jamo) and the marks below U+3400 cost nothing: a Hangul syllable has no grade, and paying the rare
// cost for every one of them would weigh the reading of Korean against its own letters.
RDE_INTERNAL f32 fude_segment_place(const fude_kanji_db* _db, const fude_segment_work* _work, u32 _line, u32 _record,
                                    f32 _a0, f32 _a1, f32 _c0, f32 _c1) {
    rde_vec_2F _rmin, _rmax;
    if(!fude_match_extent(_db, _record, &_rmin, &_rmax)) {
        return 0.0f;
    }
    const f32 _h        = _work->line_h[_line];
    const f32 _g_along  = (_a1 - _a0) / _h;
    const f32 _g_across = (_c1 - _c0) / _h;
    const f32 _r_along  = (_work->vertical ? _rmax.y - _rmin.y : _rmax.x - _rmin.x) / FUDE_SEGMENT_CELL;
    const f32 _r_across = (_work->vertical ? _rmax.x - _rmin.x : _rmax.y - _rmin.y) / FUDE_SEGMENT_CELL;
    const f32 _size     = rde_math_abs_f32(rde_math_log_f32((_g_across + 0.1f) / (_r_across + 0.1f))) + 0.5f * rde_math_abs_f32(rde_math_log_f32((_g_along + 0.1f) / (_r_along + 0.1f)));
    f32       _place    = 0.0f;
    if(!_work->vertical) {
        // KanjiVG places a character for writing across: small kana low, and 、。 at the foot.
        const f32 _g_mid = ((_c0 + _c1) * 0.5f - _work->line_c0[_line]) / _h;
        const f32 _r_mid = ((_rmin.y + _rmax.y) * 0.5f - FUDE_SEGMENT_CELL_TOP) / FUDE_SEGMENT_CELL;
        _place = rde_math_abs_f32(_g_mid - _r_mid);
    }
    fude_kanji_info              _info;
    f32                          _often = 0.0f;
    const fude_lang_writing_def* _w     = fude_lang_writing();
    if(fude_kanji_at(_db, _record, &_info) && _info.codepoint >= 0x3400 && !fude_lang_is_letter(_info.codepoint)) {
        const f32 _rank = _info.frequency > 0 ? (f32)_info.frequency : 3000.0f;
        _often = FUDE_SEGMENT_FREQ * rde_math_log_f32(1.0f + _rank / 100.0f) + (_info.grade == 0 ? FUDE_SEGMENT_RARE : 0.0f);
    } else if(_w->rare != NULL && fude_kanji_at(_db, _record, &_info) && _w->rare(_info.codepoint)) {
        _often = FUDE_SEGMENT_RARE;   // A letter out of use (lang.h).
    }
    return FUDE_SEGMENT_SIZE * _size + FUDE_SEGMENT_PLACE * _place + _often;
}

// Returns a record's code point, or 0 when there is no such record.
RDE_INTERNAL u32 fude_segment_codepoint(const fude_kanji_db* _db, u32 _record) {
    fude_kanji_info _info;
    return fude_kanji_at(_db, _record, &_info) ? _info.codepoint : 0u;
}

// Gathers strokes _first .. _first + _n - 1 into _out, fitted together as one character.
RDE_INTERNAL void fude_segment_gather(const fude_segment_work* _work, u32 _first, u32 _n, fude_match_stroke* _out) {
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = _work->strokes[_first + _i].raw;
    }
    fude_match_fit(_out, _n);
}

// Returns the room after strokes _first .. _first + _n - 1 along the line, before the next few (3) strokes in it;
// a lot when they are the line's last.
RDE_INTERNAL f32 fude_segment_room_after(const fude_segment_work* _work, u32 _first, u32 _n, f32 _a1) {
    const u32 _line = _work->line_of[_first];
    f32       _next = FUDE_SEGMENT_INF;
    for(u32 _s = _first + _n; _s < _work->count && _s < _first + _n + 3u && _work->line_of[_s] == _line; _s++) {
        _next = rde_math_min_f32(_next, _work->strokes[_s].a0);
    }
    return _next - _a1;
}

// Returns whether the run ends with marks that change a letter (lang.h: voiced), written last, at most _voice_size × H
// each, at the upper right of the rest: 2 for two ticks (Japanese's dakuten), 1 for a small ring (its handakuten), 0
// for none.
RDE_INTERNAL u32 fude_segment_marks(const fude_segment_work* _work, f32 _voice_size, u32 _first, u32 _n) {
    const f32 _h = _work->line_h[_work->line_of[_first]];
    for(u32 _k = 2; _k >= 1; _k--) {
        if(_n < _k + 1u) {
            continue;
        }
        rde_vec_2F _min = { FUDE_SEGMENT_INF, FUDE_SEGMENT_INF }, _max = { -FUDE_SEGMENT_INF, -FUDE_SEGMENT_INF };
        for(u32 _s = _first; _s < _first + _n - _k; _s++) {
            _min = (rde_vec_2F){ rde_math_min_f32(_min.x, _work->strokes[_s].min.x), rde_math_min_f32(_min.y, _work->strokes[_s].min.y) };
            _max = (rde_vec_2F){ rde_math_max_f32(_max.x, _work->strokes[_s].max.x), rde_math_max_f32(_max.y, _work->strokes[_s].max.y) };
        }
        const rde_vec_2F _mid = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
        b8 _all = true;
        for(u32 _s = _first + _n - _k; _s < _first + _n && _all; _s++) {
            const fude_segment_stroke* _m    = &_work->strokes[_s];
            const f32                  _size = rde_math_max_f32(_m->max.x - _m->min.x, _m->max.y - _m->min.y);
            const rde_vec_2F           _at   = { (_m->min.x + _m->max.x) * 0.5f, (_m->min.y + _m->max.y) * 0.5f };
            const rde_vec_2F           _p0   = _m->raw.p[0];
            const rde_vec_2F           _p1   = _m->raw.p[FUDE_MATCH_POINTS - 1];
            const b8                   _ring = rde_math_sqrt_f32((_p1.x - _p0.x) * (_p1.x - _p0.x) + (_p1.y - _p0.y) * (_p1.y - _p0.y)) < 0.4f * _size;
            _all = _size < _voice_size * _h && _at.x > _mid.x && _at.y > _mid.y && (_k == 1u ? _ring : !_ring);
        }
        if(_all) {
            return _k;
        }
    }
    return 0;
}

// A run that cannot be read (more strokes than the matcher takes) as a character with no candidates: its strokes,
// line and extent only, as the rest of the reading expects of a character nothing was found for.
RDE_INTERNAL void fude_segment_unread(const fude_segment_work* _work, u32 _first, u32 _n, fude_segment_char* _out) {
    f32 _a0, _a1, _c0, _c1;
    rde_memset(_out, 0, sizeof(*_out));
    fude_segment_extent(_work, _first, _n, &_out->min, &_out->max, &_a0, &_a1, &_c0, &_c1);
    _out->first = _first;
    _out->count = _n;
    _out->line  = _work->line_of[_first];
}

// Step 4 for one run: fills _out with its candidates, place included, best first. Returns the run's whole cost
// (the cost per stroke × strokes), or FUDE_SEGMENT_INF when it has none.
RDE_INTERNAL f32 fude_segment_rank_run(const fude_kanji_db* _db, const fude_catalog* _catalog, const fude_segment_work* _work,
                                       u32 _first, u32 _n, fude_segment_char* _out) {
    static fude_match_stroke     _strokes[FUDE_MATCH_MAX_STROKES];
    fude_match_result            _ranked[FUDE_SEGMENT_RANKED];
    fude_match_result            _pool[FUDE_SEGMENT_RANKED * 2u];   // the cost is the whole run's, over all its strokes
    u32                          _cps[FUDE_SEGMENT_RANKED * 2u];
    u32                          _found = 0;
    const fude_lang_writing_def* _w     = fude_lang_writing();

    // As matched. A character with more strokes than were written pays for the ones missing (the matcher spreads them
    // over its own count), and reading is stricter than looking up: every stroke too many or too few costs more.
    fude_segment_gather(_work, _first, _n, _strokes);
    const u32 _ranked_n = fude_match_rank_strokes(_db, _catalog, FUDE_MATCH_TEXT, _strokes, _n, _ranked, FUDE_SEGMENT_RANKED);
    for(u32 _i = 0; _i < _ranked_n; _i++) {
        fude_kanji_info _info;
        if(fude_kanji_at(_db, _ranked[_i].record, &_info)) {
            const u32 _diff = _info.strokes > _n ? _info.strokes - _n : _n - _info.strokes;
            _cps[_found]    = _info.codepoint;
            _pool[_found++] = (fude_match_result){ .record = _ranked[_i].record,
                                                   .cost = _ranked[_i].cost * (f32)(_info.strokes > _n ? _info.strokes : _n) + FUDE_SEGMENT_STROKE_DIFF * (f32)_diff };
        }
    }

    // When the run ends in marks that change a letter (the language's: Japanese's voicing marks), the rest is read as
    // the base, and its changed forms are added at its cost.
    const u32 _marks = _w->voiced != NULL ? fude_segment_marks(_work, _w->voice_size, _first, _n) : 0u;
    if(_marks > 0) {
        const u32 _base = _n - _marks;
        fude_segment_gather(_work, _first, _base, _strokes);
        const u32 _based = fude_match_rank_strokes(_db, _catalog, FUDE_MATCH_TEXT, _strokes, _base, _ranked, FUDE_SEGMENT_RANKED);
        for(u32 _i = 0; _i < _based; _i++) {
            fude_kanji_info _info;
            u32             _record;
            const u32       _voiced = fude_kanji_at(_db, _ranked[_i].record, &_info) ? _w->voiced(_info.codepoint, _marks) : 0u;
            if(_voiced == 0 || !fude_kanji_find_index(_db, _voiced, &_record)) {
                continue;
            }
            const u32 _diff = _info.strokes > _base ? _info.strokes - _base : _base - _info.strokes;
            const f32 _cost = _ranked[_i].cost * (f32)(_info.strokes > _base ? _info.strokes : _base) + FUDE_SEGMENT_STROKE_DIFF * (f32)_diff +
                              FUDE_SEGMENT_VOICE_COST * (f32)_marks;
            u32       _k    = 0;
            while(_k < _found && _pool[_k].record != _record) {
                _k++;
            }
            if(_k < _found) {
                _pool[_k].cost = rde_math_min_f32(_pool[_k].cost, _cost);
            } else if(_found < FUDE_SEGMENT_RANKED * 2u) {
                _cps[_found]    = _voiced;
                _pool[_found++] = (fude_match_result){ .record = _record, .cost = _cost };
            }
        }
    }

    f32 _a0, _a1, _c0, _c1;
    fude_segment_extent(_work, _first, _n, &_out->min, &_out->max, &_a0, &_a1, &_c0, &_c1);
    _out->first           = _first;
    _out->count           = _n;
    _out->line            = _work->line_of[_first];
    _out->candidate_count = 0;
    // A mark (the language's: 、。・) has its cell to itself: one with the next character close after it is a stroke of
    // that character (語's first dot, written down).
    const b8 _crowded = fude_segment_room_after(_work, _first, _n, _a1) < FUDE_SEGMENT_MARK_GAP * _work->line_h[_out->line];
    for(u32 _i = 0; _i < _found; _i++) {
        // A letter at its two sizes (the language's: a small kana and its full-size one) is one shape written at two
        // sizes: the matcher's difference between them is noise, so both get the better cost and size and place alone
        // choose.
        f32       _shape = _pool[_i].cost / (f32)_n + (_crowded && _w->mark != NULL && _w->mark(_cps[_i]) ? FUDE_SEGMENT_MARK_COST : 0.0f);
        const u32 _twin  = _w->size_twin != NULL ? _w->size_twin(_cps[_i]) : 0u;
        for(u32 _j = 0; _j < _found && _twin != 0; _j++) {
            if(_cps[_j] == _twin) {
                _shape = rde_math_min_f32(_shape, _pool[_j].cost / (f32)_n);
            }
        }
        const f32 _cost = _shape + fude_segment_place(_db, _work, _out->line, _pool[_i].record, _a0, _a1, _c0, _c1) / (f32)_n;
        u32       _at   = _out->candidate_count < FUDE_SEGMENT_CANDIDATES ? _out->candidate_count : FUDE_SEGMENT_CANDIDATES;
        if(_at == FUDE_SEGMENT_CANDIDATES && _cost >= _out->candidates[_at - 1u].cost) {
            continue;
        }
        while(_at > 0 && _out->candidates[_at - 1u].cost > _cost) {
            if(_at < FUDE_SEGMENT_CANDIDATES) {
                _out->candidates[_at] = _out->candidates[_at - 1u];
                _out->shape[_at]      = _out->shape[_at - 1u];
            }
            _at--;
        }
        _out->candidates[_at] = (fude_match_result){ .record = _pool[_i].record, .cost = _cost };
        _out->shape[_at]      = _shape;
        if(_out->candidate_count < FUDE_SEGMENT_CANDIDATES) {
            _out->candidate_count++;
        }
    }
    return _out->candidate_count > 0 ? _out->candidates[0].cost * (f32)_n : FUDE_SEGMENT_INF;
}

// Whether a character's place says where its line is (lang.h: placed), by default every character from U+3040 on.
RDE_INTERNAL b8 fude_segment_placed(const fude_lang_writing_def* _w, u32 _cp) {
    return _w->placed != NULL ? _w->placed(_cp) : _cp >= 0x3040u;
}

// Fits the line frames to what was read, and weighs the candidates again (segment.h). Only across, because KanjiVG
// places characters for writing across.
RDE_INTERNAL void fude_segment_refit(const fude_kanji_db* _db, fude_segment_work* _work, fude_segment_char* _chars, u32 _count) {
    if(_work->vertical) {
        return;
    }
    const fude_lang_writing_def* _w = fude_lang_writing();
    for(u32 _line = 0; _line < _work->lines; _line++) {
        // Across = alpha + beta × KanjiVG y, by least squares over the tops and bottoms of the characters read. The
        // characters whose place says nothing of the line (the language's: small kana and marks) are left out, because
        // their place is what is being judged.
        f64 _sx = 0.0, _sy = 0.0, _sxx = 0.0, _sxy = 0.0;
        u32 _points = 0;
        for(u32 _i = 0; _i < _count; _i++) {
            const fude_segment_char* _c = &_chars[_i];
            const u32                _cp = fude_segment_codepoint(_db, _c->candidates[0].record);
            rde_vec_2F               _rmin, _rmax;
            if(_c->line != _line || _c->candidate_count == 0 || !fude_segment_placed(_w, _cp) ||
               !fude_match_extent(_db, _c->candidates[0].record, &_rmin, &_rmax) || _rmax.y - _rmin.y < 10.0f) {
                continue;
            }
            rde_vec_2F _min, _max;
            f32        _a0, _a1, _c0, _c1;
            fude_segment_extent(_work, _c->first, _c->count, &_min, &_max, &_a0, &_a1, &_c0, &_c1);
            const f64 _xs[2] = { _rmin.y, _rmax.y };
            const f64 _ys[2] = { _c0, _c1 };
            for(u32 _k = 0; _k < 2; _k++) {
                _sx += _xs[_k]; _sy += _ys[_k]; _sxx += _xs[_k] * _xs[_k]; _sxy += _xs[_k] * _ys[_k];
                _points++;
            }
        }
        const f64 _den = (f64)_points * _sxx - _sx * _sx;
        if(_points < 4 || rde_math_abs_f64(_den) < 1e-6) {
            continue;   // one character, or none to go by: the extent stands
        }
        const f64 _beta  = ((f64)_points * _sxy - _sx * _sy) / _den;
        const f64 _alpha = (_sy - _beta * _sx) / (f64)_points;
        if(_beta <= 0.0) {
            continue;
        }
        _work->line_h[_line]  = (f32)(_beta * FUDE_SEGMENT_CELL);
        _work->line_c0[_line] = (f32)(_alpha + _beta * FUDE_SEGMENT_CELL_TOP);
    }

    for(u32 _i = 0; _i < _count; _i++) {
        fude_segment_char* _c = &_chars[_i];
        rde_vec_2F         _min, _max;
        f32                _a0, _a1, _c0, _c1;
        fude_segment_extent(_work, _c->first, _c->count, &_min, &_max, &_a0, &_a1, &_c0, &_c1);
        for(u32 _k = 0; _k < _c->candidate_count; _k++) {
            _c->candidates[_k].cost = _c->shape[_k] + fude_segment_place(_db, _work, _c->line, _c->candidates[_k].record, _a0, _a1, _c0, _c1) / (f32)_c->count;
        }
        for(u32 _k = 1; _k < _c->candidate_count; _k++) {
            const fude_match_result _r = _c->candidates[_k];
            const f32               _s = _c->shape[_k];
            u32                     _j = _k;
            while(_j > 0 && _c->candidates[_j - 1u].cost > _r.cost) {
                _c->candidates[_j] = _c->candidates[_j - 1u];
                _c->shape[_j]      = _c->shape[_j - 1u];
                _j--;
            }
            _c->candidates[_j] = _r;
            _c->shape[_j]      = _s;
        }
    }
}

// Returns the script the character read at _i says the characters around it are in (lang.h: script), or
// FUDE_LANG_SCRIPT_NONE when it says nothing: a mark, or a look-alike whose twin is as likely (the one being chosen).
// A look-alike that is clearly itself counts.
RDE_INTERNAL u32 fude_segment_script(const fude_lang_writing_def* _w, const fude_kanji_db* _db, const fude_segment_char* _chars, u32 _i) {
    const fude_segment_char* _c    = &_chars[_i];
    const u32                _cp   = fude_segment_codepoint(_db, _c->candidates[0].record);
    const u32                _twin = _w->twin(_cp);
    for(u32 _k = 1; _k < _c->candidate_count && _twin != 0u; _k++) {
        if(fude_segment_codepoint(_db, _c->candidates[_k].record) == _twin &&
           _c->candidates[_k].cost <= _c->candidates[0].cost + FUDE_SEGMENT_TWIN_MARGIN) {
            return FUDE_LANG_SCRIPT_NONE;
        }
    }
    return _w->script(_cp);
}

// Brings the candidate _want of _c to the front, when its cost is within FUDE_SEGMENT_TWIN_MARGIN of the first's.
RDE_INTERNAL void fude_segment_prefer(const fude_kanji_db* _db, fude_segment_char* _c, u32 _want) {
    for(u32 _k = 1; _k < _c->candidate_count; _k++) {
        if(fude_segment_codepoint(_db, _c->candidates[_k].record) == _want &&
           _c->candidates[_k].cost <= _c->candidates[0].cost + FUDE_SEGMENT_TWIN_MARGIN) {
            const fude_match_result _swap  = _c->candidates[0];
            const f32               _shape = _c->shape[0];
            _c->candidates[0]  = _c->candidates[_k];
            _c->candidates[_k] = _swap;
            _c->shape[0]       = _c->shape[_k];
            _c->shape[_k]      = _shape;
            return;
        }
    }
}

// A word is written in one script, so each character's candidate is chosen for the whole line of them, and put
// first: its cost (× strokes) plus what switching script from its neighbour costs (the language's switch_cost, lang.h;
// dynamic programming over the candidates). With no cost for switching, each character keeps its best.
RDE_INTERNAL void fude_segment_scripts(const fude_kanji_db* _db, fude_segment_char* _chars, u32 _count) {
    const fude_lang_writing_def* _w = fude_lang_writing();
    if(_count < 2 || _w->switch_cost == NULL) {
        return;
    }
    f32* _best = (f32*)rde_malloc(sizeof(f32) * _count * FUDE_SEGMENT_CANDIDATES);
    u8*  _from = (u8*)rde_malloc(_count * FUDE_SEGMENT_CANDIDATES);
    u32* _cps  = (u32*)rde_malloc(sizeof(u32) * _count * FUDE_SEGMENT_CANDIDATES);   // Each candidate's code point.
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_segment_char* _c = &_chars[_i];
        for(u32 _k = 0; _k < _c->candidate_count; _k++) {
            const u32 _at   = _i * FUDE_SEGMENT_CANDIDATES + _k;
            const f32 _cost = _c->candidates[_k].cost * (f32)_c->count;
            _cps[_at]  = fude_segment_codepoint(_db, _c->candidates[_k].record);
            _best[_at] = FUDE_SEGMENT_INF;
            _from[_at] = 0;
            if(_i == 0 || _chars[_i - 1u].candidate_count == 0) {
                _best[_at] = _cost;
                continue;
            }
            for(u32 _q = 0; _q < _chars[_i - 1u].candidate_count; _q++) {
                const u32 _prev = (_i - 1u) * FUDE_SEGMENT_CANDIDATES + _q;
                const f32 _sum  = _best[_prev] + _cost + _w->switch_cost(_cps[_prev], _cps[_at]);
                if(_sum < _best[_at]) {
                    _best[_at] = _sum;
                    _from[_at] = (u8)_q;
                }
            }
        }
    }
    // Back from the cheapest last choice; a character with no candidates starts afresh.
    u32 _pick = UINT32_MAX;
    for(u32 _i = _count; _i > 0; _i--) {
        fude_segment_char* _c = &_chars[_i - 1u];
        if(_c->candidate_count == 0) {
            _pick = UINT32_MAX;
            continue;
        }
        if(_pick == UINT32_MAX) {
            _pick = 0;
            for(u32 _k = 1; _k < _c->candidate_count; _k++) {
                if(_best[(_i - 1u) * FUDE_SEGMENT_CANDIDATES + _k] < _best[(_i - 1u) * FUDE_SEGMENT_CANDIDATES + _pick]) {
                    _pick = _k;
                }
            }
        }
        const u32 _prev = _from[(_i - 1u) * FUDE_SEGMENT_CANDIDATES + _pick];
        if(_pick != 0) {
            const fude_match_result _swap  = _c->candidates[0];
            const f32               _shape = _c->shape[0];
            _c->candidates[0]     = _c->candidates[_pick];
            _c->candidates[_pick] = _swap;
            _c->shape[0]          = _c->shape[_pick];
            _c->shape[_pick]      = _shape;
        }
        _pick = _prev;
    }
    rde_free(_best);
    rde_free(_from);
    rde_free(_cps);
}

// What the language says (segment.h; lang.h's rules): a letter at its small size where none can stand is read full
// size (Japanese: a small kana), and a look-alike across scripts is chosen by its neighbours, the nearest before and
// after whose script says something (Japanese: the katakana one next to katakana, ロボット, and the other otherwise, 口).
RDE_INTERNAL void fude_segment_context(const fude_kanji_db* _db, fude_segment_char* _chars, u32 _count) {
    const fude_lang_writing_def* _w = fude_lang_writing();
    for(u32 _i = 0; _i < _count; _i++) {
        fude_segment_char* _c = &_chars[_i];
        if(_c->candidate_count < 2) {
            continue;
        }
        const u32 _cp = fude_segment_codepoint(_db, _c->candidates[0].record);
        if(_w->small != NULL && _w->small(_cp)) {
            const u32 _before = _i > 0 ? fude_segment_codepoint(_db, _chars[_i - 1u].candidates[0].record) : 0u;
            const u32 _after  = _i + 1u < _count && _chars[_i + 1u].candidate_count > 0 ? fude_segment_codepoint(_db, _chars[_i + 1u].candidates[0].record) : 0u;
            if(!_w->small_fits(_cp, _before, _after)) {
                fude_segment_prefer(_db, _c, _w->size_twin(_cp));
            }
            continue;
        }
        if(_w->twin == NULL || _w->twin(_cp) == 0u) {
            continue;
        }
        // The nearest neighbours whose script says something.
        u32 _before = FUDE_LANG_SCRIPT_NONE;
        u32 _after  = FUDE_LANG_SCRIPT_NONE;
        for(u32 _j = _i; _j > 0 && _before == FUDE_LANG_SCRIPT_NONE; _j--) {
            _before = fude_segment_script(_w, _db, _chars, _j - 1u);
        }
        for(u32 _j = _i + 1u; _j < _count && _after == FUDE_LANG_SCRIPT_NONE; _j++) {
            _after = fude_segment_script(_w, _db, _chars, _j);
        }
        if(_before == FUDE_LANG_SCRIPT_NONE && _after == FUDE_LANG_SCRIPT_NONE) {
            continue;
        }
        const u32 _want = _w->choose(_cp, _before, _after);
        if(_want != _cp) {
            fude_segment_prefer(_db, _c, _want);
        }
    }
}

// Reads free writing (see the header): steps 1 to 4, then the line frames refitted, the scripts and the language's
// rules.
u32 fude_segment_read(fude_segment* _segment, const fude_kanji_db* _db, const fude_catalog* _catalog,
                      const fude_ink* _drawing, FUDE_SEGMENT_DIRECTION_ _direction) {
    rde_arr_clear(&_segment->chars);
    _segment->lines = 0;
    fude_segment_work _work;
    rde_memset(&_work, 0, sizeof(_work));
    if(_db == NULL || !fude_segment_prepare(&_work, _drawing, _direction)) {
        fude_segment_work_free(&_work);
        return 0;
    }
    _segment->vertical = _work.vertical;
    _segment->lines    = _work.lines;

    // Over the pieces: best[p] reads pieces 0 .. p-1, and a run never crosses a line.
    const u32          _pieces = _work.piece_count;
    f32*               _best   = (f32*)rde_malloc(sizeof(f32) * (_pieces + 1u));
    u32*               _from   = (u32*)rde_malloc(sizeof(u32) * (_pieces + 1u));
    fude_segment_char* _chosen = (fude_segment_char*)rde_malloc(sizeof(fude_segment_char) * (_pieces + 1u));
    _best[0] = 0.0f;
    for(u32 _p = 1; _p <= _pieces; _p++) {
        _best[_p] = FUDE_SEGMENT_INF;
        _from[_p] = _p - 1u;
    }
    for(u32 _i = 0; _i < _pieces; _i++) {
        if(_best[_i] >= FUDE_SEGMENT_INF) {
            continue;
        }
        const u32 _line  = _work.line_of[_work.pieces[_i]];
        const f32 _h     = _work.line_h[_line];
        const f32 _split = fude_segment_split_cost(&_work, _work.pieces[_i]);
        f32       _join  = 0.0f;
        f32       _a0    = FUDE_SEGMENT_INF, _a1 = -FUDE_SEGMENT_INF;
        for(u32 _j = _i; _j < _pieces && _j < _i + FUDE_SEGMENT_MAX_PIECES; _j++) {
            if(_work.line_of[_work.pieces[_j]] != _line) {
                break;
            }
            if(_j > _i) {
                _join += fude_segment_join_cost(&_work, _work.pieces[_j]);
                if(_join > FUDE_SEGMENT_JOIN_CAP) {
                    break;   // across that much room, it is not one character
                }
            }
            const u32 _first = _work.pieces[_i];
            const u32 _n     = _work.pieces[_j + 1u] - _first;
            for(u32 _s = _work.pieces[_j]; _s < _work.pieces[_j + 1u]; _s++) {
                _a0 = rde_math_min_f32(_a0, _work.strokes[_s].a0);
                _a1 = rde_math_max_f32(_a1, _work.strokes[_s].a1);
            }
            if(_j > _i && (_a1 - _a0 > FUDE_SEGMENT_SPAN * _h || _n > FUDE_SEGMENT_MAX_RUN)) {
                break;
            }
            if(_n > FUDE_MATCH_MAX_STROKES) {
                // One piece with more strokes than the matcher takes is still stepped over, as a character that could
                // not be read, at an unread run's cost. Without that step nothing after it could be reached, and the
                // walk back below would copy characters that were never written.
                if(_j == _i) {
                    fude_segment_char _unread;
                    fude_segment_unread(&_work, _first, _n, &_unread);
                    const f32 _sum = _best[_i] + (f32)_n * FUDE_MATCH_GAP * 2.0f + FUDE_SEGMENT_PER_CHAR + _split;
                    if(_sum < _best[_j + 1u]) {
                        _best[_j + 1u]   = _sum;
                        _from[_j + 1u]   = _i;
                        _chosen[_j + 1u] = _unread;
                    }
                }
                break;
            }
            fude_segment_char _run;
            const f32         _cost = fude_segment_rank_run(_db, _catalog, &_work, _first, _n, &_run);
            const f32         _sum  = _best[_i] + (_cost < FUDE_SEGMENT_INF ? _cost : (f32)_n * FUDE_MATCH_GAP * 2.0f) + FUDE_SEGMENT_PER_CHAR + _split + _join;
            if(_sum < _best[_j + 1u]) {
                _best[_j + 1u]   = _sum;
                _from[_j + 1u]   = _i;
                _chosen[_j + 1u] = _run;
            }
        }
    }

    // Walk back from the end, then put the characters in writing order.
    u32 _count = 0;
    for(u32 _p = _pieces; _p > 0; _p = _from[_p]) {
        _count++;
    }
    fude_segment_char* _out = (fude_segment_char*)rde_arr_add_n(&_segment->chars, _count);
    u32 _k = _count;
    for(u32 _p = _pieces; _p > 0; _p = _from[_p]) {
        _out[--_k] = _chosen[_p];
    }

    fude_segment_refit(_db, &_work, _out, _count);
    fude_segment_scripts(_db, _out, _count);
    fude_segment_context(_db, _out, _count);

    rde_free(_best);
    rde_free(_from);
    rde_free(_chosen);
    fude_segment_work_free(&_work);
    return _count;
}

// Reads the strokes as the characters _records (see the header).
b8 fude_segment_read_as(fude_segment* _segment, const fude_kanji_db* _db, const fude_ink* _drawing,
                        FUDE_SEGMENT_DIRECTION_ _direction, const u32* _records, u32 _count) {
    static fude_match_stroke _strokes[FUDE_MATCH_MAX_STROKES];
    rde_arr_clear(&_segment->chars);
    _segment->lines = 0;
    fude_segment_work _work;
    rde_memset(&_work, 0, sizeof(_work));
    if(_db == NULL || _count == 0 || !fude_segment_prepare(&_work, _drawing, _direction) || _work.count < _count) {
        fude_segment_work_free(&_work);
        return false;
    }

    // best[t * (n + 1) + s] is the cheapest reading of strokes 0 .. s-1 as characters 0 .. t-1.
    const u32 _n     = _work.count;
    const u32 _cells = (_count + 1u) * (_n + 1u);
    f32*      _best  = (f32*)rde_malloc(sizeof(f32) * _cells);
    u32*      _from  = (u32*)rde_malloc(sizeof(u32) * _cells);
    f32*      _cost  = (f32*)rde_malloc(sizeof(f32) * _cells);

    // First with each run near its character's stroke count and no longer than a character; if that finds no reading,
    // with any run.
    for(u32 _pass = 0; _pass < 2; _pass++) {
        const b8 _strict = _pass == 0;
        for(u32 _c = 0; _c < _cells; _c++) {
            _best[_c] = FUDE_SEGMENT_INF;
        }
        _best[0] = 0.0f;
        for(u32 _t = 0; _t < _count; _t++) {
            fude_kanji_info _info;
            const u32       _m    = fude_kanji_at(_db, _records[_t], &_info) ? _info.strokes : 0u;
            const u32       _low  = _strict && _m > FUDE_SEGMENT_AS_SLACK ? _m - FUDE_SEGMENT_AS_SLACK : 1u;
            const u32       _high = _strict ? _m + FUDE_SEGMENT_AS_SLACK : FUDE_SEGMENT_MAX_RUN;
            for(u32 _s = 0; _s < _n; _s++) {
                const f32 _here = _best[_t * (_n + 1u) + _s];
                if(_here >= FUDE_SEGMENT_INF) {
                    continue;
                }
                const u32 _line  = _work.line_of[_s];
                const f32 _split = fude_segment_split_cost(&_work, _s);
                f32       _join  = 0.0f;
                f32       _a0    = FUDE_SEGMENT_INF, _a1 = -FUDE_SEGMENT_INF, _c0 = FUDE_SEGMENT_INF, _c1 = -FUDE_SEGMENT_INF;
                for(u32 _e = _s + 1u; _e <= _n && _e - _s <= _high && _e - _s <= FUDE_MATCH_MAX_STROKES; _e++) {
                    const fude_segment_stroke* _last = &_work.strokes[_e - 1u];
                    if(_work.line_of[_e - 1u] != _line) {
                        break;
                    }
                    if(_e - 1u > _s) {
                        _join += fude_segment_join_cost(&_work, _e - 1u);
                    }
                    _a0 = rde_math_min_f32(_a0, _last->a0); _a1 = rde_math_max_f32(_a1, _last->a1);
                    _c0 = rde_math_min_f32(_c0, _last->c0); _c1 = rde_math_max_f32(_c1, _last->c1);
                    if(_strict && _e - _s > 1u && _a1 - _a0 > FUDE_SEGMENT_SPAN * _work.line_h[_line]) {
                        break;
                    }
                    if(_e - _s < _low) {
                        continue;
                    }
                    fude_segment_gather(&_work, _s, _e - _s, _strokes);
                    const f32 _per = fude_match_cost_record(_db, _records[_t], _strokes, _e - _s);
                    if(_per >= FUDE_SEGMENT_INF) {
                        continue;
                    }
                    const f32 _run = _per * (f32)(_m > _e - _s ? _m : _e - _s) + fude_segment_place(_db, &_work, _line, _records[_t], _a0, _a1, _c0, _c1);
                    const u32 _to  = (_t + 1u) * (_n + 1u) + _e;
                    if(_here + _run + _split + _join < _best[_to]) {
                        _best[_to] = _here + _run + _split + _join;
                        _from[_to] = _s;
                        _cost[_to] = _run / (f32)(_e - _s);
                    }
                }
            }
        }
        if(_best[_count * (_n + 1u) + _n] < FUDE_SEGMENT_INF) {
            break;
        }
    }

    const b8 _read = _best[_count * (_n + 1u) + _n] < FUDE_SEGMENT_INF;
    if(_read) {
        _segment->vertical = _work.vertical;
        _segment->lines    = _work.lines;
        fude_segment_char* _out = (fude_segment_char*)rde_arr_add_n(&_segment->chars, _count);
        u32 _e = _n;
        for(u32 _t = _count; _t > 0; _t--) {
            const u32 _at = _t * (_n + 1u) + _e;
            const u32 _s  = _from[_at];
            f32 _a0, _a1, _c0, _c1;
            rde_memset(&_out[_t - 1u], 0, sizeof(fude_segment_char));
            fude_segment_extent(&_work, _s, _e - _s, &_out[_t - 1u].min, &_out[_t - 1u].max, &_a0, &_a1, &_c0, &_c1);
            _out[_t - 1u].first           = _s;
            _out[_t - 1u].count           = _e - _s;
            _out[_t - 1u].line            = _work.line_of[_s];
            _out[_t - 1u].candidates[0]   = (fude_match_result){ .record = _records[_t - 1u], .cost = _cost[_at] };
            _out[_t - 1u].shape[0]        = _cost[_at];
            _out[_t - 1u].candidate_count = 1;
            _e = _s;
        }
    }

    rde_free(_best);
    rde_free(_from);
    rde_free(_cost);
    fude_segment_work_free(&_work);
    return _read;
}
