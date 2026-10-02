#include "handwriting/segment.h"

#include <math.h>
#include <string.h>

// ===========================================================================
// See segment.h.
// ===========================================================================

#define KANA_SEGMENT_TALL       1.3f     // AUTO: down when the whole is this much taller than wide
#define KANA_SEGMENT_SPAN       1.4f     // a character: at most this × H along the line
#define KANA_SEGMENT_CUT        0.2f     // a cut: the stroke starts past the piece's far end, less this × H
#define KANA_SEGMENT_MAX_PIECES 8u       // pieces in one character
#define KANA_SEGMENT_MAX_RUN    40u      // strokes in one character (the most in the data is ~30)
#ifndef KANA_SEGMENT_PER_CHAR
#define KANA_SEGMENT_PER_CHAR   12.0f    // paid per character read: pieces of one character stay together
#endif
// The gaps along the line (segment.h): a character is written tight and there
// is room between characters, so a run joined across a gap pays for it, and a
// cut where the strokes (nearly) touch pays for that. Measured against the
// line's own room between characters — some write tight, some fill a grid: the
// line holds about (its length / H) characters, so about that many of its
// widest gaps are between characters, and the room is their median (never
// under KANA_SEGMENT_ROOM_MIN × H). One character alone has no room between
// characters: nothing to measure a join against.
#ifndef KANA_SEGMENT_JOIN
#define KANA_SEGMENT_JOIN       60.0f    // per room of gap past KANA_SEGMENT_JOIN_FREE
#endif
#ifndef KANA_SEGMENT_JOIN_FREE
#define KANA_SEGMENT_JOIN_FREE  0.3f
#endif
#ifndef KANA_SEGMENT_JOIN_CAP
#define KANA_SEGMENT_JOIN_CAP   45.0f    // a run that has paid this much for its gaps goes no further (speed)
#endif
#ifndef KANA_SEGMENT_SPLIT
#define KANA_SEGMENT_SPLIT      40.0f    // per room of gap under KANA_SEGMENT_SPLIT_FREE
#endif
#ifndef KANA_SEGMENT_SPLIT_FREE
#define KANA_SEGMENT_SPLIT_FREE 0.5f
#endif
#ifndef KANA_SEGMENT_ROOM_MIN
#define KANA_SEGMENT_ROOM_MIN   0.1f
#endif
#define KANA_SEGMENT_GAP_MIN    -0.3f    // a stroke inside the piece so far: as touching as it gets (in H)
#define KANA_SEGMENT_SIZE       8.0f     // the size term's weight
#define KANA_SEGMENT_PLACE      8.0f     // the place term's weight (across only)
#ifndef KANA_SEGMENT_RARE
#define KANA_SEGMENT_RARE       20.0f    // a kanji in neither the Jouyou nor the Jinmeiyou (丶, 刂: radicals)
#endif
#ifndef KANA_SEGMENT_FREQ
#define KANA_SEGMENT_FREQ       3.0f     // × ln(1 + rank / 100): kana are commoner in text than any kanji
#endif
#ifndef KANA_SEGMENT_RANKED
#define KANA_SEGMENT_RANKED     16u      // matcher results per run, before the size term
#endif
#define KANA_SEGMENT_CELL_TOP   12.0f    // KanjiVG's box: where a full-height character starts
#define KANA_SEGMENT_CELL       85.0f    //                and how tall it is
#define KANA_SEGMENT_AS_SLACK   3u       // reading as: a run within this many strokes of its character's
#define KANA_SEGMENT_TWIN_MARGIN 3.0f    // a look-alike this close (per stroke) may be chosen by its neighbours
#ifndef KANA_SEGMENT_SWITCH_KANA
#define KANA_SEGMENT_SWITCH_KANA 12.0f   // hiragana next to katakana (a word is in one script)
#endif
#ifndef KANA_SEGMENT_SWITCH_KANJI
#define KANA_SEGMENT_SWITCH_KANJI 6.0f   // katakana next to kanji (hiragana next to kanji is free: okurigana, particles)
#endif
#ifndef KANA_SEGMENT_STROKE_DIFF
#define KANA_SEGMENT_STROKE_DIFF 10.0f   // per stroke written but not in the character, or the other way
#endif
#define KANA_SEGMENT_VOICE_SIZE 0.22f    // a dakuten tick or handakuten ring: at most this × H
#define KANA_SEGMENT_VOICE_COST 4.0f     // per mark stroke, when the rest is read as the base character
#define KANA_SEGMENT_MARK_GAP   0.25f    // 、。・ are followed by at least this × H of nothing
#define KANA_SEGMENT_MARK_COST  20.0f    // added to a mark that is not
#define KANA_SEGMENT_INF        1e30f

// Look-alikes across scripts, katakana first: the same shape, so which one is
// meant is told by the characters around it.
static const u32 KANA_SEGMENT_TWINS[][2] = {
    { 0x30D8, 0x3078 },   // ヘ へ
    { 0x30D9, 0x3079 },   // ベ べ
    { 0x30DA, 0x307A },   // ペ ぺ
    { 0x30FC, 0x4E00 },   // ー 一
    { 0x30AB, 0x529B },   // カ 力
    { 0x30A8, 0x5DE5 },   // エ 工
    { 0x30ED, 0x53E3 },   // ロ 口
    { 0x30CB, 0x4E8C },   // ニ 二
    { 0x30CF, 0x516B },   // ハ 八
    { 0x30BF, 0x5915 },   // タ 夕
    { 0x30C8, 0x535C },   // ト 卜
    { 0x30EA, 0x308A },   // リ り
    { 0x30CA, 0x5341 },   // ナ 十   (alike rather than the same)
    { 0x30C1, 0x5343 },   // チ 千
    { 0x30AA, 0x624D },   // オ 才
    { 0x30D2, 0x5315 },   // ヒ 匕
    { 0x30E0, 0x53B6 },   // ム 厶
    { 0x30CC, 0x53C8 },   // ヌ 又
    { 0x30CE, 0x4E3F },   // ノ 丿
    { 0x30EB, 0x513F },   // ル 儿
    { 0x30BB, 0x305B },   // セ せ
    { 0x30E2, 0x3082 },   // モ も
    { 0x30E4, 0x3084 },   // ヤ や
    { 0x30AD, 0x304D },   // キ き
};

RDE_STRUCT {
    kana_match_stroke raw;          // resampled, Y down, the drawing's units — not fitted
    rde_vec_2F        min;          // extent, Y up
    rde_vec_2F        max;
    f32               a0, a1;       // extent along the line
    f32               c0, c1;       // and across it (growing towards the next line)
} kana_segment_stroke;

RDE_STRUCT {
    kana_segment_stroke* strokes;
    u32                  count;
    b8                   vertical;
    u32*                 line_of;    // per stroke
    u32                  lines;
    f32*                 line_h;     // per line: H, and where it starts across
    f32*                 line_c0;
    u32*                 pieces;     // the first stroke of each piece; pieces[piece_count] = count
    u32                  piece_count;
    f32*                 gap;        // per stroke: the room before it along the line, in rooms (a line's first: a lot)
} kana_segment_work;

void kana_segment_init(kana_segment* _segment) {
    memset(_segment, 0, sizeof(*_segment));
    _segment->chars = rde_arr_new(sizeof(kana_segment_char), rde_memory_allocator_get_default());
}

void kana_segment_destroy(kana_segment* _segment) {
    if(rde_arr_is_inited(&_segment->chars)) {
        rde_arr_free(&_segment->chars);
    }
    memset(_segment, 0, sizeof(*_segment));
}

RDE_INTERNAL void kana_segment_work_free(kana_segment_work* _work) {
    rde_free(_work->strokes);
    rde_free(_work->line_of);
    rde_free(_work->line_h);
    rde_free(_work->line_c0);
    rde_free(_work->pieces);
    rde_free(_work->gap);
    memset(_work, 0, sizeof(*_work));
}

// Steps 1-3 (segment.h): the strokes prepared, the direction, the lines, the pieces.
RDE_INTERNAL b8 kana_segment_prepare(kana_segment_work* _work, const kana_ink* _drawing, KANA_SEGMENT_DIRECTION_ _direction) {
    static rde_vec_2F _pts[1024];
    memset(_work, 0, sizeof(*_work));
    const u32 _total = kana_ink_alive_strokes(_drawing);
    if(_total == 0) {
        return false;
    }
    _work->strokes = (kana_segment_stroke*)rde_malloc(sizeof(kana_segment_stroke) * _total);
    _work->line_of = (u32*)rde_malloc(sizeof(u32) * _total);
    _work->line_h  = (f32*)rde_malloc(sizeof(f32) * _total);
    _work->line_c0 = (f32*)rde_malloc(sizeof(f32) * _total);
    _work->pieces  = (u32*)rde_malloc(sizeof(u32) * (_total + 1u));
    _work->gap     = (f32*)rde_malloc(sizeof(f32) * _total);

    rde_vec_2F _all_min = { KANA_SEGMENT_INF, KANA_SEGMENT_INF };
    rde_vec_2F _all_max = { -KANA_SEGMENT_INF, -KANA_SEGMENT_INF };
    for(u32 _i = 0; _i < kana_ink_stroke_count(_drawing) && _work->count < _total; _i++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_drawing, _i);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        kana_segment_stroke*  _s = &_work->strokes[_work->count++];
        const kana_ink_point* _p = kana_ink_stroke_points(_drawing, _stroke);
        const u32             _n = _stroke->point_count < 1024u ? _stroke->point_count : 1024u;
        _s->min = (rde_vec_2F){ KANA_SEGMENT_INF, KANA_SEGMENT_INF };
        _s->max = (rde_vec_2F){ -KANA_SEGMENT_INF, -KANA_SEGMENT_INF };
        for(u32 _k = 0; _k < _n; _k++) {
            _pts[_k] = (rde_vec_2F){ _p[_k].position.x, -_p[_k].position.y };
            _s->min  = (rde_vec_2F){ fminf(_s->min.x, _p[_k].position.x), fminf(_s->min.y, _p[_k].position.y) };
            _s->max  = (rde_vec_2F){ fmaxf(_s->max.x, _p[_k].position.x), fmaxf(_s->max.y, _p[_k].position.y) };
        }
        kana_match_resample_points(_pts, _n, &_s->raw);
        _all_min = (rde_vec_2F){ fminf(_all_min.x, _s->min.x), fminf(_all_min.y, _s->min.y) };
        _all_max = (rde_vec_2F){ fmaxf(_all_max.x, _s->max.x), fmaxf(_all_max.y, _s->max.y) };
    }
    if(_work->count == 0) {
        return false;
    }

    // 1. The direction, and each stroke along and across it.
    _work->vertical = _direction == KANA_SEGMENT_DOWN ||
                      (_direction == KANA_SEGMENT_AUTO && _all_max.y - _all_min.y > KANA_SEGMENT_TALL * (_all_max.x - _all_min.x));
    f32 _sizes[64];
    u32 _sized = 0;
    for(u32 _i = 0; _i < _work->count; _i++) {
        kana_segment_stroke* _s = &_work->strokes[_i];
        if(_work->vertical) {
            _s->a0 = -_s->max.y; _s->a1 = -_s->min.y;   // downwards
            _s->c0 = -_s->max.x; _s->c1 = -_s->min.x;   // columns go right to left
        } else {
            _s->a0 = _s->min.x;  _s->a1 = _s->max.x;
            _s->c0 = -_s->max.y; _s->c1 = -_s->min.y;   // lines go down
        }
        if(_sized < 64u) {
            _sizes[_sized++] = fmaxf(_s->a1 - _s->a0, _s->c1 - _s->c0);
        }
    }
    // A stroke's typical size: the floor under a line's H, so a line of flat
    // strokes (一二三) is not read as a sliver.
    for(u32 _i = 1; _i < _sized; _i++) {
        const f32 _v = _sizes[_i];
        u32       _j = _i;
        while(_j > 0 && _sizes[_j - 1] > _v) { _sizes[_j] = _sizes[_j - 1]; _j--; }
        _sizes[_j] = _v;
    }
    const f32 _unit = fmaxf(_sizes[_sized / 2u], 1e-3f);

    // 2. Lines: a stroke wholly past the line, back towards its start, begins the
    // next. A line is at least two strokes tall for this: a flat first stroke
    // (the 一 over 雨) is not a line of its own.
    f32 _A0 = _work->strokes[0].a0, _A1 = _work->strokes[0].a1;
    f32 _C0 = _work->strokes[0].c0, _C1 = _work->strokes[0].c1;
    _work->lines      = 1;
    _work->line_of[0] = 0;
    for(u32 _i = 1; _i < _work->count; _i++) {
        const kana_segment_stroke* _s    = &_work->strokes[_i];
        const f32                  _h    = fmaxf(_C1 - _C0, 2.0f * _unit);
        const b8                   _past = _s->c0 > _C1 - 0.1f * _h;
        const b8                   _back = _s->a1 < _A1 - _h;
        if(_past && _back) {
            _work->line_h[_work->lines - 1u]  = fmaxf(_C1 - _C0, 1.2f * _unit);
            _work->line_c0[_work->lines - 1u] = _C0;
            _work->lines++;
            _A0 = _s->a0; _A1 = _s->a1; _C0 = _s->c0; _C1 = _s->c1;
        } else {
            _A0 = fminf(_A0, _s->a0); _A1 = fmaxf(_A1, _s->a1);
            _C0 = fminf(_C0, _s->c0); _C1 = fmaxf(_C1, _s->c1);
        }
        _work->line_of[_i] = _work->lines - 1u;
    }
    _work->line_h[_work->lines - 1u]  = fmaxf(_C1 - _C0, 1.2f * _unit);
    _work->line_c0[_work->lines - 1u] = _C0;

    // 3. Pieces: a stroke clear of the piece so far (in the writing direction) cuts.
    f32 _far = 0.0f;
    for(u32 _i = 0; _i < _work->count; _i++) {
        const kana_segment_stroke* _s     = &_work->strokes[_i];
        const b8                   _first = _i == 0 || _work->line_of[_i] != _work->line_of[_i - 1u];
        const f32                  _h     = _work->line_h[_work->line_of[_i]];
        _work->gap[_i] = _first ? KANA_SEGMENT_INF : fmaxf((_s->a0 - _far) / _h, KANA_SEGMENT_GAP_MIN);
        if(_first || _s->a0 >= _far - KANA_SEGMENT_CUT * _h) {
            _work->pieces[_work->piece_count++] = _i;
            _far = _s->a1;
        } else {
            _far = fmaxf(_far, _s->a1);
        }
    }
    _work->pieces[_work->piece_count] = _work->count;

    // Each line's room between characters (in H), then every gap in rooms.
    for(u32 _line = 0; _line < _work->lines; _line++) {
        f32 _gaps[256];
        u32 _n  = 0;
        f32 _a0 = KANA_SEGMENT_INF, _a1 = -KANA_SEGMENT_INF;
        for(u32 _i = 0; _i < _work->count; _i++) {
            if(_work->line_of[_i] == _line) {
                _a0 = fminf(_a0, _work->strokes[_i].a0);
                _a1 = fmaxf(_a1, _work->strokes[_i].a1);
            }
        }
        for(u32 _p = 0; _p < _work->piece_count && _n < 256u; _p++) {
            const u32 _s = _work->pieces[_p];
            if(_work->line_of[_s] == _line && _work->gap[_s] < KANA_SEGMENT_INF && _work->gap[_s] > 0.0f) {
                _gaps[_n++] = _work->gap[_s];
            }
        }
        for(u32 _i = 1; _i < _n; _i++) {   // widest first
            const f32 _v = _gaps[_i];
            u32       _j = _i;
            while(_j > 0 && _gaps[_j - 1u] < _v) { _gaps[_j] = _gaps[_j - 1u]; _j--; }
            _gaps[_j] = _v;
        }
        const u32 _chars   = (u32)floorf((_a1 - _a0) / _work->line_h[_line] + 0.5f);
        const u32 _between = _chars > 1u ? (_chars - 1u < _n ? _chars - 1u : _n) : 0u;
        const f32 _room    = _between > 0 ? fmaxf(_gaps[_between / 2u], KANA_SEGMENT_ROOM_MIN) : 1e6f;
        for(u32 _i = 0; _i < _work->count; _i++) {
            if(_work->line_of[_i] == _line && _work->gap[_i] < KANA_SEGMENT_INF) {
                _work->gap[_i] /= _room;
            }
        }
    }
    return true;
}

// What a cut before stroke _s costs (nothing at a line's start), and what
// joining across it does.
RDE_INTERNAL f32 kana_segment_split_cost(const kana_segment_work* _work, u32 _s) {
    return _work->gap[_s] >= KANA_SEGMENT_INF ? 0.0f : KANA_SEGMENT_SPLIT * fmaxf(0.0f, KANA_SEGMENT_SPLIT_FREE - _work->gap[_s]);
}

RDE_INTERNAL f32 kana_segment_join_cost(const kana_segment_work* _work, u32 _s) {
    return KANA_SEGMENT_JOIN * fmaxf(0.0f, _work->gap[_s] - KANA_SEGMENT_JOIN_FREE);
}

// Strokes _first .. _first + _n - 1: their extent (Y up, and along / across).
RDE_INTERNAL void kana_segment_extent(const kana_segment_work* _work, u32 _first, u32 _n, rde_vec_2F* _min, rde_vec_2F* _max,
                                      f32* _a0, f32* _a1, f32* _c0, f32* _c1) {
    *_min = (rde_vec_2F){ KANA_SEGMENT_INF, KANA_SEGMENT_INF };
    *_max = (rde_vec_2F){ -KANA_SEGMENT_INF, -KANA_SEGMENT_INF };
    *_a0  = KANA_SEGMENT_INF; *_a1 = -KANA_SEGMENT_INF;
    *_c0  = KANA_SEGMENT_INF; *_c1 = -KANA_SEGMENT_INF;
    for(u32 _i = _first; _i < _first + _n; _i++) {
        const kana_segment_stroke* _s = &_work->strokes[_i];
        *_min = (rde_vec_2F){ fminf(_min->x, _s->min.x), fminf(_min->y, _s->min.y) };
        *_max = (rde_vec_2F){ fmaxf(_max->x, _s->max.x), fmaxf(_max->y, _s->max.y) };
        *_a0  = fminf(*_a0, _s->a0); *_a1 = fmaxf(*_a1, _s->a1);
        *_c0  = fminf(*_c0, _s->c0); *_c1 = fmaxf(*_c1, _s->c1);
    }
}

// The size-and-place term (segment.h) of reading a run as _record — and how
// often it is written: a kanji costs more the rarer it is (its newspaper rank;
// kana and marks nothing), and one outside the Jouyou and Jinmeiyou lists (a
// radical like 刂) more again, as does a kana out of use (ヺ, ゐ). It is what tells す from 十 when the hand is loose.
RDE_INTERNAL f32 kana_segment_place(const kana_kanji_db* _db, const kana_segment_work* _work, u32 _line, u32 _record,
                                    f32 _a0, f32 _a1, f32 _c0, f32 _c1) {
    rde_vec_2F _rmin, _rmax;
    if(!kana_match_extent(_db, _record, &_rmin, &_rmax)) {
        return 0.0f;
    }
    const f32 _h        = _work->line_h[_line];
    const f32 _g_along  = (_a1 - _a0) / _h;
    const f32 _g_across = (_c1 - _c0) / _h;
    const f32 _r_along  = (_work->vertical ? _rmax.y - _rmin.y : _rmax.x - _rmin.x) / KANA_SEGMENT_CELL;
    const f32 _r_across = (_work->vertical ? _rmax.x - _rmin.x : _rmax.y - _rmin.y) / KANA_SEGMENT_CELL;
    const f32 _size     = fabsf(logf((_g_across + 0.1f) / (_r_across + 0.1f))) + 0.5f * fabsf(logf((_g_along + 0.1f) / (_r_along + 0.1f)));
    f32       _place    = 0.0f;
    if(!_work->vertical) {
        // KanjiVG places a character for writing across: small kana low, 、。 at the foot.
        const f32 _g_mid = ((_c0 + _c1) * 0.5f - _work->line_c0[_line]) / _h;
        const f32 _r_mid = ((_rmin.y + _rmax.y) * 0.5f - KANA_SEGMENT_CELL_TOP) / KANA_SEGMENT_CELL;
        _place = fabsf(_g_mid - _r_mid);
    }
    kana_kanji_info _info;
    f32             _often = 0.0f;
    if(kana_kanji_at(_db, _record, &_info) && _info.codepoint >= 0x3400) {
        const f32 _rank = _info.frequency > 0 ? (f32)_info.frequency : 3000.0f;
        _often = KANA_SEGMENT_FREQ * logf(1.0f + _rank / 100.0f) + (_info.grade == 0 ? KANA_SEGMENT_RARE : 0.0f);
    } else if(kana_kanji_at(_db, _record, &_info)) {
        switch(_info.codepoint) {   // kana out of use: ゐ ゑ ヰ ヱ ヷ ヸ ヹ ヺ ゔ ゎ ヮ ゕ ゖ ヵ ゟ ヿ
            case 0x3090: case 0x3091: case 0x30F0: case 0x30F1: case 0x30F7: case 0x30F8: case 0x30F9: case 0x30FA:
            case 0x3094: case 0x308E: case 0x30EE: case 0x3095: case 0x3096: case 0x30F5: case 0x309F: case 0x30FF:
                _often = KANA_SEGMENT_RARE;
                break;
            default:
                break;
        }
    }
    return KANA_SEGMENT_SIZE * _size + KANA_SEGMENT_PLACE * _place + _often;
}

// A small kana's full-size one and the other way round (ゃ や, ッ ツ); 0 for anything else.
RDE_INTERNAL u32 kana_segment_size_twin(u32 _cp) {
    static const u32 _small[] = { 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308E,
                                  0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE };
    for(u32 _i = 0; _i < sizeof(_small) / sizeof(_small[0]); _i++) {
        if(_cp == _small[_i])      { return _cp + 1u; }
        if(_cp == _small[_i] + 1u) { return _cp - 1u; }
    }
    switch(_cp) {
        case 0x3095: return 0x304B;   // ゕ か
        case 0x304B: return 0x3095;
        case 0x3096: return 0x3051;   // ゖ け
        case 0x3051: return 0x3096;
        case 0x30F5: return 0x30AB;   // ヵ カ
        case 0x30AB: return 0x30F5;
        case 0x30F6: return 0x30B1;   // ヶ ケ
        case 0x30B1: return 0x30F6;
        default:     return 0;
    }
}

RDE_INTERNAL u32 kana_segment_codepoint(const kana_kanji_db* _db, u32 _record) {
    kana_kanji_info _info;
    return kana_kanji_at(_db, _record, &_info) ? _info.codepoint : 0u;
}

// Strokes _first .. _first + _n - 1, fitted together as one character.
RDE_INTERNAL void kana_segment_gather(const kana_segment_work* _work, u32 _first, u32 _n, kana_match_stroke* _out) {
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = _work->strokes[_first + _i].raw;
    }
    kana_match_fit(_out, _n);
}

// Step 4 for one run: its candidates, place included, best first. The run's
// whole cost (per-stroke cost × strokes), KANA_SEGMENT_INF with none.
RDE_INTERNAL b8 kana_segment_is_mark(u32 _cp) {
    return _cp == 0x3001 || _cp == 0x3002 || _cp == 0x30FB;
}

// The room after strokes _first .. _first + _n - 1 along the line, before the
// next few strokes in it; a lot when they are the line's last.
RDE_INTERNAL f32 kana_segment_room_after(const kana_segment_work* _work, u32 _first, u32 _n, f32 _a1) {
    const u32 _line = _work->line_of[_first];
    f32       _next = KANA_SEGMENT_INF;
    for(u32 _s = _first + _n; _s < _work->count && _s < _first + _n + 3u && _work->line_of[_s] == _line; _s++) {
        _next = fminf(_next, _work->strokes[_s].a0);
    }
    return _next - _a1;
}

// A base kana's voiced form: _marks 2 its dakuten one (か が, ウ ヴ), 1 its
// handakuten one (は ぱ); 0 when it has none.
RDE_INTERNAL u32 kana_segment_voiced(u32 _base, u32 _marks) {
    const b8  _kata = _base >= 0x30A1 && _base <= 0x30F6;
    const u32 _h    = _kata ? _base - 0x60u : _base;
    u32       _v    = 0;
    if(_marks == 2u) {
        if(_h >= 0x304B && _h <= 0x3061 && ((_h - 0x304B) % 2u) == 0u) { _v = _h + 1u; }   // か..ち
        else if(_h == 0x3064 || _h == 0x3066 || _h == 0x3068)           { _v = _h + 1u; }   // つ て と
        else if(_h >= 0x306F && _h <= 0x307B && ((_h - 0x306F) % 3u) == 0u) { _v = _h + 1u; } // は..ほ
        else if(_h == 0x3046)                                             { return _kata ? 0x30F4 : 0x3094; }   // う
    } else if(_marks == 1u) {
        if(_h >= 0x306F && _h <= 0x307B && ((_h - 0x306F) % 3u) == 0u) { _v = _h + 2u; }   // は..ほ
    }
    return _v != 0 && _kata ? _v + 0x60u : _v;
}

// Does the run end with voicing marks — written last, small, at the upper right
// of the rest? 2: a dakuten (two ticks), 1: a handakuten (a small ring), 0: no.
RDE_INTERNAL u32 kana_segment_marks(const kana_segment_work* _work, u32 _first, u32 _n) {
    const f32 _h = _work->line_h[_work->line_of[_first]];
    for(u32 _k = 2; _k >= 1; _k--) {
        if(_n < _k + 1u) {
            continue;
        }
        rde_vec_2F _min = { KANA_SEGMENT_INF, KANA_SEGMENT_INF }, _max = { -KANA_SEGMENT_INF, -KANA_SEGMENT_INF };
        for(u32 _s = _first; _s < _first + _n - _k; _s++) {
            _min = (rde_vec_2F){ fminf(_min.x, _work->strokes[_s].min.x), fminf(_min.y, _work->strokes[_s].min.y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _work->strokes[_s].max.x), fmaxf(_max.y, _work->strokes[_s].max.y) };
        }
        const rde_vec_2F _mid = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
        b8 _all = true;
        for(u32 _s = _first + _n - _k; _s < _first + _n && _all; _s++) {
            const kana_segment_stroke* _m    = &_work->strokes[_s];
            const f32                  _size = fmaxf(_m->max.x - _m->min.x, _m->max.y - _m->min.y);
            const rde_vec_2F           _at   = { (_m->min.x + _m->max.x) * 0.5f, (_m->min.y + _m->max.y) * 0.5f };
            const rde_vec_2F           _p0   = _m->raw.p[0];
            const rde_vec_2F           _p1   = _m->raw.p[KANA_MATCH_POINTS - 1];
            const b8                   _ring = sqrtf((_p1.x - _p0.x) * (_p1.x - _p0.x) + (_p1.y - _p0.y) * (_p1.y - _p0.y)) < 0.4f * _size;
            _all = _size < KANA_SEGMENT_VOICE_SIZE * _h && _at.x > _mid.x && _at.y > _mid.y && (_k == 1u ? _ring : !_ring);
        }
        if(_all) {
            return _k;
        }
    }
    return 0;
}

// Step 4 for one run: its candidates, place included, best first. The run's
// whole cost (per-stroke cost × strokes), KANA_SEGMENT_INF with none.
RDE_INTERNAL f32 kana_segment_rank_run(const kana_kanji_db* _db, const kana_catalog* _catalog, const kana_segment_work* _work,
                                       u32 _first, u32 _n, kana_segment_char* _out) {
    static kana_match_stroke _strokes[KANA_MATCH_MAX_STROKES];
    kana_match_result        _ranked[KANA_SEGMENT_RANKED];
    kana_match_result        _pool[KANA_SEGMENT_RANKED * 2u];   // cost: the whole run's, over all its strokes
    u32                      _cps[KANA_SEGMENT_RANKED * 2u];
    u32                      _found = 0;

    // As matched. A character with more strokes than were written pays for the
    // ones missing (the matcher spreads them over its own count), and reading
    // is stricter than looking up: every stroke too many or too few costs more.
    kana_segment_gather(_work, _first, _n, _strokes);
    const u32 _ranked_n = kana_match_rank_strokes(_db, _catalog, KANA_MATCH_TEXT, _strokes, _n, _ranked, KANA_SEGMENT_RANKED);
    for(u32 _i = 0; _i < _ranked_n; _i++) {
        kana_kanji_info _info;
        if(kana_kanji_at(_db, _ranked[_i].record, &_info)) {
            const u32 _diff = _info.strokes > _n ? _info.strokes - _n : _n - _info.strokes;
            _cps[_found]    = _info.codepoint;
            _pool[_found++] = (kana_match_result){ .record = _ranked[_i].record,
                                                   .cost = _ranked[_i].cost * (f32)(_info.strokes > _n ? _info.strokes : _n) + KANA_SEGMENT_STROKE_DIFF * (f32)_diff };
        }
    }

    // Ending in voicing marks: the rest as the base, its voiced forms at its cost.
    const u32 _marks = kana_segment_marks(_work, _first, _n);
    if(_marks > 0) {
        const u32 _base = _n - _marks;
        kana_segment_gather(_work, _first, _base, _strokes);
        const u32 _based = kana_match_rank_strokes(_db, _catalog, KANA_MATCH_TEXT, _strokes, _base, _ranked, KANA_SEGMENT_RANKED);
        for(u32 _i = 0; _i < _based; _i++) {
            kana_kanji_info _info;
            u32             _record;
            const u32       _voiced = kana_kanji_at(_db, _ranked[_i].record, &_info) ? kana_segment_voiced(_info.codepoint, _marks) : 0u;
            if(_voiced == 0 || !kana_kanji_find_index(_db, _voiced, &_record)) {
                continue;
            }
            const u32 _diff = _info.strokes > _base ? _info.strokes - _base : _base - _info.strokes;
            const f32 _cost = _ranked[_i].cost * (f32)(_info.strokes > _base ? _info.strokes : _base) + KANA_SEGMENT_STROKE_DIFF * (f32)_diff +
                              KANA_SEGMENT_VOICE_COST * (f32)_marks;
            u32       _k    = 0;
            while(_k < _found && _pool[_k].record != _record) {
                _k++;
            }
            if(_k < _found) {
                _pool[_k].cost = fminf(_pool[_k].cost, _cost);
            } else if(_found < KANA_SEGMENT_RANKED * 2u) {
                _cps[_found]    = _voiced;
                _pool[_found++] = (kana_match_result){ .record = _record, .cost = _cost };
            }
        }
    }

    f32 _a0, _a1, _c0, _c1;
    kana_segment_extent(_work, _first, _n, &_out->min, &_out->max, &_a0, &_a1, &_c0, &_c1);
    _out->first           = _first;
    _out->count           = _n;
    _out->line            = _work->line_of[_first];
    _out->candidate_count = 0;
    // A mark has its cell to itself: one with the next character close after it
    // is a stroke of that character (語's first dot, written down).
    const b8 _crowded = kana_segment_room_after(_work, _first, _n, _a1) < KANA_SEGMENT_MARK_GAP * _work->line_h[_out->line];
    for(u32 _i = 0; _i < _found; _i++) {
        // A small kana and its full-size one are one shape written at two sizes:
        // the matcher's difference between them is noise, so both get the better
        // cost and size-and-place alone chooses.
        f32       _shape = _pool[_i].cost / (f32)_n + (_crowded && kana_segment_is_mark(_cps[_i]) ? KANA_SEGMENT_MARK_COST : 0.0f);
        const u32 _twin  = kana_segment_size_twin(_cps[_i]);
        for(u32 _j = 0; _j < _found && _twin != 0; _j++) {
            if(_cps[_j] == _twin) {
                _shape = fminf(_shape, _pool[_j].cost / (f32)_n);
            }
        }
        const f32 _cost = _shape + kana_segment_place(_db, _work, _out->line, _pool[_i].record, _a0, _a1, _c0, _c1) / (f32)_n;
        u32       _at   = _out->candidate_count < KANA_SEGMENT_CANDIDATES ? _out->candidate_count : KANA_SEGMENT_CANDIDATES;
        if(_at == KANA_SEGMENT_CANDIDATES && _cost >= _out->candidates[_at - 1u].cost) {
            continue;
        }
        while(_at > 0 && _out->candidates[_at - 1u].cost > _cost) {
            if(_at < KANA_SEGMENT_CANDIDATES) {
                _out->candidates[_at] = _out->candidates[_at - 1u];
                _out->shape[_at]      = _out->shape[_at - 1u];
            }
            _at--;
        }
        _out->candidates[_at] = (kana_match_result){ .record = _pool[_i].record, .cost = _cost };
        _out->shape[_at]      = _shape;
        if(_out->candidate_count < KANA_SEGMENT_CANDIDATES) {
            _out->candidate_count++;
        }
    }
    return _out->candidate_count > 0 ? _out->candidates[0].cost * (f32)_n : KANA_SEGMENT_INF;
}

RDE_INTERNAL b8 kana_segment_is_small(u32 _cp);

// The line frames fitted to what was read, and the candidates weighed again
// (segment.h). Across only: KanjiVG places characters for writing across.
RDE_INTERNAL void kana_segment_refit(const kana_kanji_db* _db, kana_segment_work* _work, kana_segment_char* _chars, u32 _count) {
    if(_work->vertical) {
        return;
    }
    for(u32 _line = 0; _line < _work->lines; _line++) {
        // Across = alpha + beta * KanjiVG y, by least squares over the tops and
        // bottoms of the characters read — not small kana or marks, whose place
        // is what is being judged.
        f64 _sx = 0.0, _sy = 0.0, _sxx = 0.0, _sxy = 0.0;
        u32 _points = 0;
        for(u32 _i = 0; _i < _count; _i++) {
            const kana_segment_char* _c = &_chars[_i];
            const u32                _cp = kana_segment_codepoint(_db, _c->candidates[0].record);
            rde_vec_2F               _rmin, _rmax;
            if(_c->line != _line || _c->candidate_count == 0 || kana_segment_is_small(_cp) || _cp < 0x3040 || _cp == 0x30FC ||
               !kana_match_extent(_db, _c->candidates[0].record, &_rmin, &_rmax) || _rmax.y - _rmin.y < 10.0f) {
                continue;
            }
            rde_vec_2F _min, _max;
            f32        _a0, _a1, _c0, _c1;
            kana_segment_extent(_work, _c->first, _c->count, &_min, &_max, &_a0, &_a1, &_c0, &_c1);
            const f64 _xs[2] = { _rmin.y, _rmax.y };
            const f64 _ys[2] = { _c0, _c1 };
            for(u32 _k = 0; _k < 2; _k++) {
                _sx += _xs[_k]; _sy += _ys[_k]; _sxx += _xs[_k] * _xs[_k]; _sxy += _xs[_k] * _ys[_k];
                _points++;
            }
        }
        const f64 _den = (f64)_points * _sxx - _sx * _sx;
        if(_points < 4 || fabs(_den) < 1e-6) {
            continue;   // one character, or none to go by: the extent stands
        }
        const f64 _beta  = ((f64)_points * _sxy - _sx * _sy) / _den;
        const f64 _alpha = (_sy - _beta * _sx) / (f64)_points;
        if(_beta <= 0.0) {
            continue;
        }
        _work->line_h[_line]  = (f32)(_beta * KANA_SEGMENT_CELL);
        _work->line_c0[_line] = (f32)(_alpha + _beta * KANA_SEGMENT_CELL_TOP);
    }

    for(u32 _i = 0; _i < _count; _i++) {
        kana_segment_char* _c = &_chars[_i];
        rde_vec_2F         _min, _max;
        f32                _a0, _a1, _c0, _c1;
        kana_segment_extent(_work, _c->first, _c->count, &_min, &_max, &_a0, &_a1, &_c0, &_c1);
        for(u32 _k = 0; _k < _c->candidate_count; _k++) {
            _c->candidates[_k].cost = _c->shape[_k] + kana_segment_place(_db, _work, _c->line, _c->candidates[_k].record, _a0, _a1, _c0, _c1) / (f32)_c->count;
        }
        for(u32 _k = 1; _k < _c->candidate_count; _k++) {
            const kana_match_result _r = _c->candidates[_k];
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

// 0 not kana, 1 hiragana, 2 katakana — of the character read at _i; UINT32_MAX
// when it says nothing of the script around it: a mark, or a look-alike whose
// twin is as likely (the one being chosen). A look-alike clearly itself counts.
RDE_INTERNAL u32 kana_segment_script(const kana_kanji_db* _db, const kana_segment_char* _chars, u32 _i) {
    const kana_segment_char* _c  = &_chars[_i];
    const u32                _cp = kana_segment_codepoint(_db, _c->candidates[0].record);
    for(u32 _t = 0; _t < sizeof(KANA_SEGMENT_TWINS) / sizeof(KANA_SEGMENT_TWINS[0]); _t++) {
        if(_cp != KANA_SEGMENT_TWINS[_t][0] && _cp != KANA_SEGMENT_TWINS[_t][1]) {
            continue;
        }
        const u32 _twin = KANA_SEGMENT_TWINS[_t][_cp == KANA_SEGMENT_TWINS[_t][0] ? 1 : 0];
        for(u32 _k = 1; _k < _c->candidate_count; _k++) {
            if(kana_segment_codepoint(_db, _c->candidates[_k].record) == _twin &&
               _c->candidates[_k].cost <= _c->candidates[0].cost + KANA_SEGMENT_TWIN_MARGIN) {
                return UINT32_MAX;
            }
        }
        break;
    }
    if(_cp >= 0x3041 && _cp <= 0x3096) { return 1u; }
    if(_cp >= 0x30A1 && _cp <= 0x30FA) { return 2u; }
    if(_cp >= 0x4E00 && _cp <= 0x9FFF) { return 0u; }
    return UINT32_MAX;
}

RDE_INTERNAL b8 kana_segment_among(u32 _cp, const u32* _list, u32 _count) {
    for(u32 _k = 0; _k < _count; _k++) {
        if(_cp == _list[_k]) {
            return true;
        }
    }
    return false;
}

// Can small kana _small stand between _before and _after (0: nothing)?
// ゃゅょ follow an i-row kana (きゃ, しょ; テュ, フュ in katakana). っ follows
// something and comes before a consonant — not a vowel, ん, a small kana or ー
// (あっい is あつい). ァィゥェォ follow the katakana they sound with (ティ, ファ,
// ウォ; not ドィ), and in hiragana hardly at all. ゎ, ゕ, ゖ are all but unused.
RDE_INTERNAL b8 kana_segment_small_fits(u32 _small, u32 _before, u32 _after) {
    const b8  _kata   = _small >= 0x30A1;
    const u32 _hira   = _kata ? _small - 0x60u : _small;
    const u32 _b      = _before >= 0x30A1 && _before <= 0x30F6 ? _before - 0x60u : _before;   // before, as hiragana
    const u32 _a      = _after >= 0x30A1 && _after <= 0x30F6 ? _after - 0x60u : _after;
    static const u32 _i_row[] = { 0x304D, 0x304E, 0x3057, 0x3058, 0x3061, 0x3062, 0x306B, 0x3072, 0x3073, 0x3074, 0x307F, 0x308A };
    static const u32 _vowels[] = { 0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x3093, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049,
                                   0x3063, 0x3083, 0x3085, 0x3087, 0x30FC - 0x60u };
    switch(_hira) {
        case 0x3063:                               // っ
            return _before != 0 && !kana_segment_among(_a, _vowels, sizeof(_vowels) / sizeof(_vowels[0])) && _after != 0x30FC;
        case 0x3083: case 0x3085: case 0x3087:     // ゃ ゅ ょ
            return kana_segment_among(_b, _i_row, sizeof(_i_row) / sizeof(_i_row[0])) ||
                   (_kata && (_b == 0x3066 || _b == 0x3067 || _b == 0x3075 || _b == 0x3094));   // テュ デュ フュ ヴュ
        case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: {   // ァ ィ ゥ ェ ォ
            if(!_kata || !(_before >= 0x30A1 && _before <= 0x30FC)) {
                return false;
            }
            static const u32 _for_a[] = { 0x3075, 0x3094, 0x3064, 0x304F, 0x3050 };                                           // フ ヴ ツ ク グ
            static const u32 _for_i[] = { 0x3066, 0x3067, 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050, 0x3059, 0x305A };   // テ デ ウ ヴ フ ツ ク グ ス ズ
            static const u32 _for_u[] = { 0x3068, 0x3069, 0x3075 };                                                          // ト ド フ
            static const u32 _for_e[] = { 0x3057, 0x3058, 0x3061, 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050, 0x3044 };   // シ ジ チ ウ ヴ フ ツ ク グ イ
            static const u32 _for_o[] = { 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050 };                                   // ウ ヴ フ ツ ク グ
            switch(_hira) {
                case 0x3041: return kana_segment_among(_b, _for_a, sizeof(_for_a) / sizeof(_for_a[0]));
                case 0x3043: return kana_segment_among(_b, _for_i, sizeof(_for_i) / sizeof(_for_i[0]));
                case 0x3045: return kana_segment_among(_b, _for_u, sizeof(_for_u) / sizeof(_for_u[0]));
                case 0x3047: return kana_segment_among(_b, _for_e, sizeof(_for_e) / sizeof(_for_e[0]));
                default:     return kana_segment_among(_b, _for_o, sizeof(_for_o) / sizeof(_for_o[0]));
            }
        }
        default:
            return false;
    }
}

RDE_INTERNAL b8 kana_segment_is_small(u32 _cp) {
    const u32 _twin = kana_segment_size_twin(_cp);
    return _twin != 0 && (_twin == _cp + 1u || _cp == 0x3095 || _cp == 0x3096 || _cp == 0x30F5 || _cp == 0x30F6);
}

// Brings candidate _want of _c to the front when it is within the margin.
RDE_INTERNAL void kana_segment_prefer(const kana_kanji_db* _db, kana_segment_char* _c, u32 _want) {
    for(u32 _k = 1; _k < _c->candidate_count; _k++) {
        if(kana_segment_codepoint(_db, _c->candidates[_k].record) == _want &&
           _c->candidates[_k].cost <= _c->candidates[0].cost + KANA_SEGMENT_TWIN_MARGIN) {
            const kana_match_result _swap  = _c->candidates[0];
            const f32               _shape = _c->shape[0];
            _c->candidates[0]  = _c->candidates[_k];
            _c->candidates[_k] = _swap;
            _c->shape[0]       = _c->shape[_k];
            _c->shape[_k]      = _shape;
            return;
        }
    }
}

// 0 hiragana, 1 katakana (ー too), 2 kanji, 3 anything else, 4 a hiragana
// particle (の と を は が に で へ も や か ね よ: between katakana words all the time).
RDE_INTERNAL u32 kana_segment_script_of(u32 _cp) {
    switch(_cp) {
        case 0x306E: case 0x3068: case 0x3092: case 0x306F: case 0x304C: case 0x306B: case 0x3067:
        case 0x3078: case 0x3082: case 0x3084: case 0x304B: case 0x306D: case 0x3088:
            return 4u;
        default:
            break;
    }
    if(_cp >= 0x3041 && _cp <= 0x3096) { return 0u; }
    if(_cp >= 0x30A1 && _cp <= 0x30FC) { return 1u; }
    if((_cp >= 0x4E00 && _cp <= 0x9FFF) || (_cp >= 0x3400 && _cp <= 0x4DBF)) { return 2u; }
    return 3u;
}

RDE_INTERNAL f32 kana_segment_switch(u32 _a, u32 _b) {
    if(_a == 4u || _b == 4u) {
        // A particle: half the switch — it stands between katakana words (ツナのサンド)
        // but is also a kana inside hiragana ones (ありが).
        return (_a == 1u || _b == 1u) ? KANA_SEGMENT_SWITCH_KANA * 0.5f : 0.0f;
    }
    if(_a == 3u || _b == 3u || _a == _b) {
        return 0.0f;
    }
    if(_a + _b == 1u) {                  // hiragana, katakana
        return KANA_SEGMENT_SWITCH_KANA;
    }
    return (_a == 1u || _b == 1u) ? KANA_SEGMENT_SWITCH_KANJI : 0.0f;
}

// A word is written in one script: each character's candidate is chosen for
// the whole line of them — its cost (× strokes) plus what switching script from
// its neighbour costs (dynamic programming over the candidates) — and put first.
RDE_INTERNAL void kana_segment_scripts(const kana_kanji_db* _db, kana_segment_char* _chars, u32 _count) {
    if(_count < 2) {
        return;
    }
    f32* _best = (f32*)rde_malloc(sizeof(f32) * _count * KANA_SEGMENT_CANDIDATES);
    u8*  _from = (u8*)rde_malloc(_count * KANA_SEGMENT_CANDIDATES);
    u8*  _kind = (u8*)rde_malloc(_count * KANA_SEGMENT_CANDIDATES);
    for(u32 _i = 0; _i < _count; _i++) {
        const kana_segment_char* _c = &_chars[_i];
        for(u32 _k = 0; _k < _c->candidate_count; _k++) {
            const u32 _at   = _i * KANA_SEGMENT_CANDIDATES + _k;
            const f32 _cost = _c->candidates[_k].cost * (f32)_c->count;
            _kind[_at] = (u8)kana_segment_script_of(kana_segment_codepoint(_db, _c->candidates[_k].record));
            _best[_at] = KANA_SEGMENT_INF;
            _from[_at] = 0;
            if(_i == 0 || _chars[_i - 1u].candidate_count == 0) {
                _best[_at] = _cost;
                continue;
            }
            for(u32 _q = 0; _q < _chars[_i - 1u].candidate_count; _q++) {
                const u32 _prev = (_i - 1u) * KANA_SEGMENT_CANDIDATES + _q;
                const f32 _sum  = _best[_prev] + _cost + kana_segment_switch(_kind[_prev], _kind[_at]);
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
        kana_segment_char* _c = &_chars[_i - 1u];
        if(_c->candidate_count == 0) {
            _pick = UINT32_MAX;
            continue;
        }
        if(_pick == UINT32_MAX) {
            _pick = 0;
            for(u32 _k = 1; _k < _c->candidate_count; _k++) {
                if(_best[(_i - 1u) * KANA_SEGMENT_CANDIDATES + _k] < _best[(_i - 1u) * KANA_SEGMENT_CANDIDATES + _pick]) {
                    _pick = _k;
                }
            }
        }
        const u32 _prev = _from[(_i - 1u) * KANA_SEGMENT_CANDIDATES + _pick];
        if(_pick != 0) {
            const kana_match_result _swap  = _c->candidates[0];
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
    rde_free(_kind);
}

// What the language says (segment.h): look-alikes across scripts
// (KANA_SEGMENT_TWINS) are chosen by their neighbours — the katakana one next to
// katakana (ロボット), the other otherwise (口); へ, a particle too, by what
// follows it (ヘン, but ロボットへ行く); ー only after katakana (コーヒー, but
// 一杯) — and a small kana where one cannot be is read full size.
RDE_INTERNAL void kana_segment_context(const kana_kanji_db* _db, kana_segment_char* _chars, u32 _count) {
    for(u32 _i = 0; _i < _count; _i++) {
        kana_segment_char* _c = &_chars[_i];
        if(_c->candidate_count < 2) {
            continue;
        }
        const u32 _cp = kana_segment_codepoint(_db, _c->candidates[0].record);
        if(kana_segment_is_small(_cp)) {
            const u32 _before = _i > 0 ? kana_segment_codepoint(_db, _chars[_i - 1u].candidates[0].record) : 0u;
            const u32 _after  = _i + 1u < _count && _chars[_i + 1u].candidate_count > 0 ? kana_segment_codepoint(_db, _chars[_i + 1u].candidates[0].record) : 0u;
            if(!kana_segment_small_fits(_cp, _before, _after)) {
                kana_segment_prefer(_db, _c, kana_segment_size_twin(_cp));
            }
            continue;
        }
        for(u32 _t = 0; _t < sizeof(KANA_SEGMENT_TWINS) / sizeof(KANA_SEGMENT_TWINS[0]); _t++) {
            const b8 _is_kata  = _cp == KANA_SEGMENT_TWINS[_t][0];
            const b8 _is_other = _cp == KANA_SEGMENT_TWINS[_t][1];
            if(!_is_kata && !_is_other) {
                continue;
            }
            // The nearest neighbours whose script says something.
            u32 _before = UINT32_MAX;
            u32 _after  = UINT32_MAX;
            for(u32 _j = _i; _j > 0 && _before == UINT32_MAX; _j--) {
                _before = kana_segment_script(_db, _chars, _j - 1u);
            }
            for(u32 _j = _i + 1u; _j < _count && _after == UINT32_MAX; _j++) {
                _after = kana_segment_script(_db, _chars, _j);
            }
            if(_before == UINT32_MAX && _after == UINT32_MAX) {
                break;
            }
            const b8 _particle = KANA_SEGMENT_TWINS[_t][1] == 0x3078;   // へ, "to"
            b8       _want_kata;
            if(KANA_SEGMENT_TWINS[_t][0] == 0x30FC) {
                _want_kata = _before == 2u;
            } else if(_particle && _after != UINT32_MAX) {
                _want_kata = _after == 2u;
            } else {
                _want_kata = _before == 2u || _after == 2u;
            }
            const u32 _want = KANA_SEGMENT_TWINS[_t][_want_kata ? 0 : 1];
            if(_want == _cp) {
                break;
            }
            kana_segment_prefer(_db, _c, _want);
            break;
        }
    }
}

u32 kana_segment_read(kana_segment* _segment, const kana_kanji_db* _db, const kana_catalog* _catalog,
                      const kana_ink* _drawing, KANA_SEGMENT_DIRECTION_ _direction) {
    rde_arr_clear(&_segment->chars);
    _segment->lines = 0;
    kana_segment_work _work;
    memset(&_work, 0, sizeof(_work));
    if(_db == NULL || !kana_segment_prepare(&_work, _drawing, _direction)) {
        kana_segment_work_free(&_work);
        return 0;
    }
    _segment->vertical = _work.vertical;
    _segment->lines    = _work.lines;

    // Over the pieces: best[p] reads pieces 0 .. p-1; a run never crosses a line.
    const u32          _pieces = _work.piece_count;
    f32*               _best   = (f32*)rde_malloc(sizeof(f32) * (_pieces + 1u));
    u32*               _from   = (u32*)rde_malloc(sizeof(u32) * (_pieces + 1u));
    kana_segment_char* _chosen = (kana_segment_char*)rde_malloc(sizeof(kana_segment_char) * (_pieces + 1u));
    _best[0] = 0.0f;
    for(u32 _p = 1; _p <= _pieces; _p++) {
        _best[_p] = KANA_SEGMENT_INF;
        _from[_p] = _p - 1u;
    }
    for(u32 _i = 0; _i < _pieces; _i++) {
        if(_best[_i] >= KANA_SEGMENT_INF) {
            continue;
        }
        const u32 _line  = _work.line_of[_work.pieces[_i]];
        const f32 _h     = _work.line_h[_line];
        const f32 _split = kana_segment_split_cost(&_work, _work.pieces[_i]);
        f32       _join  = 0.0f;
        f32       _a0    = KANA_SEGMENT_INF, _a1 = -KANA_SEGMENT_INF;
        for(u32 _j = _i; _j < _pieces && _j < _i + KANA_SEGMENT_MAX_PIECES; _j++) {
            if(_work.line_of[_work.pieces[_j]] != _line) {
                break;
            }
            if(_j > _i) {
                _join += kana_segment_join_cost(&_work, _work.pieces[_j]);
                if(_join > KANA_SEGMENT_JOIN_CAP) {
                    break;   // across that much room it is not one character
                }
            }
            const u32 _first = _work.pieces[_i];
            const u32 _n     = _work.pieces[_j + 1u] - _first;
            for(u32 _s = _work.pieces[_j]; _s < _work.pieces[_j + 1u]; _s++) {
                _a0 = fminf(_a0, _work.strokes[_s].a0);
                _a1 = fmaxf(_a1, _work.strokes[_s].a1);
            }
            if(_j > _i && (_a1 - _a0 > KANA_SEGMENT_SPAN * _h || _n > KANA_SEGMENT_MAX_RUN)) {
                break;
            }
            if(_n > KANA_MATCH_MAX_STROKES) {
                break;
            }
            kana_segment_char _run;
            const f32         _cost = kana_segment_rank_run(_db, _catalog, &_work, _first, _n, &_run);
            const f32         _sum  = _best[_i] + (_cost < KANA_SEGMENT_INF ? _cost : (f32)_n * KANA_MATCH_GAP * 2.0f) + KANA_SEGMENT_PER_CHAR + _split + _join;
            if(_sum < _best[_j + 1u]) {
                _best[_j + 1u]   = _sum;
                _from[_j + 1u]   = _i;
                _chosen[_j + 1u] = _run;
            }
        }
    }

    // Walk back from the end, then put them in writing order.
    u32 _count = 0;
    for(u32 _p = _pieces; _p > 0; _p = _from[_p]) {
        _count++;
    }
    kana_segment_char* _out = (kana_segment_char*)rde_arr_add_n(&_segment->chars, _count);
    u32 _k = _count;
    for(u32 _p = _pieces; _p > 0; _p = _from[_p]) {
        _out[--_k] = _chosen[_p];
    }

    kana_segment_refit(_db, &_work, _out, _count);
    kana_segment_scripts(_db, _out, _count);
    kana_segment_context(_db, _out, _count);

    rde_free(_best);
    rde_free(_from);
    rde_free(_chosen);
    kana_segment_work_free(&_work);
    return _count;
}

b8 kana_segment_read_as(kana_segment* _segment, const kana_kanji_db* _db, const kana_ink* _drawing,
                        KANA_SEGMENT_DIRECTION_ _direction, const u32* _records, u32 _count) {
    static kana_match_stroke _strokes[KANA_MATCH_MAX_STROKES];
    rde_arr_clear(&_segment->chars);
    _segment->lines = 0;
    kana_segment_work _work;
    memset(&_work, 0, sizeof(_work));
    if(_db == NULL || _count == 0 || !kana_segment_prepare(&_work, _drawing, _direction) || _work.count < _count) {
        kana_segment_work_free(&_work);
        return false;
    }

    // best[t * (n + 1) + s]: strokes 0 .. s-1 read as characters 0 .. t-1.
    const u32 _n     = _work.count;
    const u32 _cells = (_count + 1u) * (_n + 1u);
    f32*      _best  = (f32*)rde_malloc(sizeof(f32) * _cells);
    u32*      _from  = (u32*)rde_malloc(sizeof(u32) * _cells);
    f32*      _cost  = (f32*)rde_malloc(sizeof(f32) * _cells);

    // First with each run near its character's stroke count and no longer than
    // a character; if that finds no reading, with any run.
    for(u32 _pass = 0; _pass < 2; _pass++) {
        const b8 _strict = _pass == 0;
        for(u32 _c = 0; _c < _cells; _c++) {
            _best[_c] = KANA_SEGMENT_INF;
        }
        _best[0] = 0.0f;
        for(u32 _t = 0; _t < _count; _t++) {
            kana_kanji_info _info;
            const u32       _m    = kana_kanji_at(_db, _records[_t], &_info) ? _info.strokes : 0u;
            const u32       _low  = _strict && _m > KANA_SEGMENT_AS_SLACK ? _m - KANA_SEGMENT_AS_SLACK : 1u;
            const u32       _high = _strict ? _m + KANA_SEGMENT_AS_SLACK : KANA_SEGMENT_MAX_RUN;
            for(u32 _s = 0; _s < _n; _s++) {
                const f32 _here = _best[_t * (_n + 1u) + _s];
                if(_here >= KANA_SEGMENT_INF) {
                    continue;
                }
                const u32 _line  = _work.line_of[_s];
                const f32 _split = kana_segment_split_cost(&_work, _s);
                f32       _join  = 0.0f;
                f32       _a0    = KANA_SEGMENT_INF, _a1 = -KANA_SEGMENT_INF, _c0 = KANA_SEGMENT_INF, _c1 = -KANA_SEGMENT_INF;
                for(u32 _e = _s + 1u; _e <= _n && _e - _s <= _high && _e - _s <= KANA_MATCH_MAX_STROKES; _e++) {
                    const kana_segment_stroke* _last = &_work.strokes[_e - 1u];
                    if(_work.line_of[_e - 1u] != _line) {
                        break;
                    }
                    if(_e - 1u > _s) {
                        _join += kana_segment_join_cost(&_work, _e - 1u);
                    }
                    _a0 = fminf(_a0, _last->a0); _a1 = fmaxf(_a1, _last->a1);
                    _c0 = fminf(_c0, _last->c0); _c1 = fmaxf(_c1, _last->c1);
                    if(_strict && _e - _s > 1u && _a1 - _a0 > KANA_SEGMENT_SPAN * _work.line_h[_line]) {
                        break;
                    }
                    if(_e - _s < _low) {
                        continue;
                    }
                    kana_segment_gather(&_work, _s, _e - _s, _strokes);
                    const f32 _per = kana_match_cost_record(_db, _records[_t], _strokes, _e - _s);
                    if(_per >= KANA_SEGMENT_INF) {
                        continue;
                    }
                    const f32 _run = _per * (f32)(_m > _e - _s ? _m : _e - _s) + kana_segment_place(_db, &_work, _line, _records[_t], _a0, _a1, _c0, _c1);
                    const u32 _to  = (_t + 1u) * (_n + 1u) + _e;
                    if(_here + _run + _split + _join < _best[_to]) {
                        _best[_to] = _here + _run + _split + _join;
                        _from[_to] = _s;
                        _cost[_to] = _run / (f32)(_e - _s);
                    }
                }
            }
        }
        if(_best[_count * (_n + 1u) + _n] < KANA_SEGMENT_INF) {
            break;
        }
    }

    const b8 _read = _best[_count * (_n + 1u) + _n] < KANA_SEGMENT_INF;
    if(_read) {
        _segment->vertical = _work.vertical;
        _segment->lines    = _work.lines;
        kana_segment_char* _out = (kana_segment_char*)rde_arr_add_n(&_segment->chars, _count);
        u32 _e = _n;
        for(u32 _t = _count; _t > 0; _t--) {
            const u32 _at = _t * (_n + 1u) + _e;
            const u32 _s  = _from[_at];
            f32 _a0, _a1, _c0, _c1;
            memset(&_out[_t - 1u], 0, sizeof(kana_segment_char));
            kana_segment_extent(&_work, _s, _e - _s, &_out[_t - 1u].min, &_out[_t - 1u].max, &_a0, &_a1, &_c0, &_c1);
            _out[_t - 1u].first           = _s;
            _out[_t - 1u].count           = _e - _s;
            _out[_t - 1u].line            = _work.line_of[_s];
            _out[_t - 1u].candidates[0]   = (kana_match_result){ .record = _records[_t - 1u], .cost = _cost[_at] };
            _out[_t - 1u].shape[0]        = _cost[_at];
            _out[_t - 1u].candidate_count = 1;
            _e = _s;
        }
    }

    rde_free(_best);
    rde_free(_from);
    rde_free(_cost);
    kana_segment_work_free(&_work);
    return _read;
}
