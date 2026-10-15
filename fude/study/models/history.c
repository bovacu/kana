// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/history.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"

// ===========================================================================
// See history.h.
//
// Nothing is held between calls: every function reads or writes the character's file then and there. Saving reads the
// whole file through fude_save_load, appends one session chunk and writes it all back atomically. Reading is done by
// three readers handed to fude_save_load: one keeps the bytes whole (for a save), one fills a fude_history (the
// album), and one reads only each session's time and average (the summary). A session that does not read is dropped
// whole, so the rest of the history still shows.
// ===========================================================================

// The tags of the file's kind and of its session chunk.
#define FUDE_HISTORY_KIND          FUDE_TAG('P', 'R', 'A', 'C')
#define FUDE_HISTORY_CHUNK_SESSION FUDE_TAG('S', 'E', 'S', 'S')
#define FUDE_HISTORY_SQUARE_SIZE   20u     // A square record's size in bytes as written; a newer one may be longer.

// Sanity limits for reading: a count past them means the file is damaged, not that the drawing was that big.
#define FUDE_HISTORY_MAX_SQUARES   64u
#define FUDE_HISTORY_MAX_STROKES   256u
#define FUDE_HISTORY_MAX_POINTS    100000u

RDE_INTERNAL u32 fude_history_saves = 0;   // Counts the saves; fude_history_revision returns it.

// Returns the practice folder under the save folder. fude_save_subdir finds it again only when the save folder has
// changed, so this is cheap to call every time.
const c8* fude_history_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };
    fude_save_subdir("practice", _dir, sizeof(_dir));
    return _dir;
}

// Writes into _out the path of _codepoint's history file: its code point in upper-case hex, at least four digits
// (6728.kana).
RDE_INTERNAL void fude_history_path(u32 _codepoint, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%04X.kana", fude_history_dir(), _codepoint);
}

// Converts a coordinate in the drawing's units into the file's u16: a fraction of the square's side _size, times
// 65535. A point outside the square is clamped to its edge, and a square of no size gives 0.
RDE_INTERNAL u16 fude_history_fraction(f32 _v, f32 _size) {
    const f32 _f = _size > 0.0f ? rde_math_clamp_f32(_v / _size, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

// The reader fude_history_save hands to fude_save_load: copies the file's bytes whole into the fude_bytes that _user
// points to, when the header is that of a history file. Returns false, with nothing copied, when it is not.
RDE_INTERNAL b8 fude_history_read_whole(const u8* _data, u32 _size, any _user) {
    fude_bytes* _b = (fude_bytes*)_user;
    rde_arr_clear(_b);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
        return false;
    }
    fude_put_data(_b, _data, _size);
    return true;
}

// Appends one session to the character's file (see the header). Only the strokes that are alive are written. The
// revision counter goes up whether or not the file could be written.
b8 fude_history_save(u32 _codepoint, u64 _time, u32 _squares, const fude_score* _scores,
                     const fude_ink* const* _drawings, f32 _square_size) {
    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));

    // The file as it is now is kept byte for byte, because the history is only ever added to. It is read the way
    // every saved file is read (save.h): a damaged file is set aside and its backup used, so today's session is
    // neither lost nor written over it, and a file missing after the app was killed mid-save is found in its backup.
    fude_bytes _b = fude_bytes_new(4096u);
    const FUDE_LOAD_ _was = fude_save_load(_path, fude_history_read_whole, &_b);
    if(_was != FUDE_LOAD_OK && _was != FUDE_LOAD_RECOVERED) {
        rde_arr_clear(&_b);
        fude_put_header(&_b, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND);
    }

    // The session's average is over the squares that were scored; the empty ones do not count.
    f32 _sum    = 0.0f;
    u32 _scored = 0;
    for(u32 _i = 0; _i < _squares; _i++) {
        if(!_scores[_i].empty) {
            _sum += _scores[_i].score;
            _scored++;
        }
    }

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_HISTORY_CHUNK_SESSION);
    fude_put_u64(&_b, _time);
    fude_put_u32(&_b, _codepoint);
    fude_put_f32(&_b, _scored > 0 ? _sum / (f32)_scored : 0.0f);
    fude_put_u32(&_b, _squares);
    fude_put_u32(&_b, FUDE_HISTORY_SQUARE_SIZE);

    for(u32 _i = 0; _i < _squares; _i++) {
        const fude_score* _s = &_scores[_i];
        fude_put_u8(&_b, _s->empty ? 0u : 1u);
        fude_put_f32(&_b, _s->score);
        fude_put_f32(&_b, _s->shape);
        fude_put_u8(&_b, _s->drawn);
        fude_put_u8(&_b, _s->expected);
        fude_put_u8(&_b, _s->missing);
        fude_put_u8(&_b, _s->extra);
        fude_put_u8(&_b, _s->misplaced);
        fude_put_u8(&_b, _s->reversed);
        fude_put_u8(&_b, _s->first_reversed);
        fude_put_u8(&_b, _s->swap_a);
        fude_put_u8(&_b, _s->swap_b);
        fude_put_u8(&_b, _s->worst);
        fude_put_u8(&_b, 0u);   // A reserved byte: 1 + 4 + 4 + 10 + 1 bytes make the 20-byte record.

        // The drawing, stored relative to its square: the live strokes are counted first, then written.
        // At most what reading takes (FUDE_HISTORY_MAX_STROKES strokes, FUDE_HISTORY_MAX_POINTS points a stroke): a
        // square scribbled past them keeps its first strokes, rather than being saved whole and then dropped, with
        // its session, every time the file is read.
        const fude_ink* _ink   = _drawings[_i];
        u32             _count = 0;
        for(u32 _k = 0; _k < fude_ink_stroke_count(_ink); _k++) {
            _count += fude_ink_stroke_at(_ink, _k)->alive ? 1u : 0u;
        }
        _count = _count < FUDE_HISTORY_MAX_STROKES ? _count : FUDE_HISTORY_MAX_STROKES;
        fude_put_u32(&_b, _count);
        u32 _written = 0;
        for(u32 _k = 0; _k < fude_ink_stroke_count(_ink) && _written < _count; _k++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _k);
            if(!_stroke->alive) {
                continue;
            }
            _written++;
            const fude_ink_point* _p      = fude_ink_stroke_points(_ink, _stroke);
            const u32             _points = _stroke->point_count < FUDE_HISTORY_MAX_POINTS ? _stroke->point_count : FUDE_HISTORY_MAX_POINTS;
            fude_put_u32(&_b, _points);
            for(u32 _q = 0; _q < _points; _q++) {
                fude_put_u16(&_b, fude_history_fraction(_p[_q].position.x, _square_size));
                fude_put_u16(&_b, fude_history_fraction(_p[_q].position.y, _square_size));
                fude_put_f32(&_b, _p[_q].time);
            }
        }
    }
    fude_chunk_end(&_b, _chunk);

    const b8 _ok = fude_bytes_write_and_free(&_b, _path, NULL);
    fude_history_saves++;
    return _ok;
}

// Returns the save counter, which goes up on every save so a view knows when to read the history again.
u32 fude_history_revision(void) {
    return fude_history_saves;
}

// The callback fude_history_list hands to the folder crawl: adds the code point of a file named exactly
// "<hex code point>.kana", with four to six hex digits of either case, to the rde_arr that _user_data points to. The
// .bak, .tmp and .bad files beside it do not match. Always returns true, to go on crawling.
RDE_INTERNAL b8 fude_history_list_entry(const c8* _path, b8 _is_dir, any _user_data) {
    if(_is_dir) {
        return true;
    }

    // The file's name: what follows the last slash.
    const c8* _name = _path;
    for(const c8* _c = _path; *_c != 0; _c++) {
        if(*_c == '/' || *_c == '\\') {
            _name = _c + 1;
        }
    }

    // Read the hex digits at the start of the name.
    u32   _cp     = 0;
    usize _digits = 0;
    for(; _name[_digits] != 0; _digits++) {
        const c8 _h = _name[_digits];
        const i32 _v = (_h >= '0' && _h <= '9') ? _h - '0' : (_h >= 'A' && _h <= 'F') ? _h - 'A' + 10 : (_h >= 'a' && _h <= 'f') ? _h - 'a' + 10 : -1;
        if(_v < 0) {
            break;
        }
        _cp = _cp * 16u + (u32)_v;
    }

    if(_digits >= 4 && _digits <= 6 && rde_strcmp(&_name[_digits], ".kana") == 0) {
        rde_arr_add((rde_arr*)_user_data, &_cp);
    }
    return true;
}

// Lists the characters that have a history file. See the header.
void fude_history_list(rde_arr* _out) {
    rde_file_crawl_dir_recursively(fude_history_dir(), fude_history_list_entry, NULL, 0, _out);
}

// Zeroes _history and creates its four arrays on the standard heap.
void fude_history_init(fude_history* _history) {
    rde_memset(_history, 0, sizeof(*_history));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _history->sessions = rde_arr_new(sizeof(fude_history_session), _heap);
    _history->squares  = rde_arr_new(sizeof(fude_history_square),  _heap);
    _history->strokes  = rde_arr_new(sizeof(fude_history_stroke),  _heap);
    _history->points   = rde_arr_new(sizeof(fude_history_point),   _heap);
}

// Frees the arrays that were created and zeroes _history, so destroying a zeroed history, or one twice, is safe.
void fude_history_destroy(fude_history* _history) {
    rde_arr* _arrays[] = { &_history->sessions, &_history->squares, &_history->strokes, &_history->points };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_memset(_history, 0, sizeof(*_history));
}

// Reads one session's _squares squares (records of _record_size bytes, each followed by its drawing) from _c into
// _history's squares, strokes and points. Returns false when the chunk is damaged or a count is past the sanity
// limits; the caller then drops whatever this session added.
RDE_INTERNAL b8 fude_history_read_squares(fude_history* _history, fude_reader* _c, u32 _squares, u32 _record_size) {
    for(u32 _i = 0; _i < _squares; _i++) {
        const u32           _start = _c->pos;
        fude_history_square _sq;
        rde_memset(&_sq, 0, sizeof(_sq));

        _sq.score.empty          = fude_get_u8(_c) == 0;
        _sq.score.score          = fude_get_f32(_c);
        _sq.score.shape          = fude_get_f32(_c);
        _sq.score.drawn          = fude_get_u8(_c);
        _sq.score.expected       = fude_get_u8(_c);
        _sq.score.missing        = fude_get_u8(_c);
        _sq.score.extra          = fude_get_u8(_c);
        _sq.score.misplaced      = fude_get_u8(_c);
        _sq.score.reversed       = fude_get_u8(_c);
        _sq.score.first_reversed = fude_get_u8(_c);
        _sq.score.swap_a         = fude_get_u8(_c);
        _sq.score.swap_b         = fude_get_u8(_c);
        _sq.score.worst          = fude_get_u8(_c);
        fude_score_describe(&_sq.score);

        // A record written by a newer build may be longer: skip the bytes this build does not know.
        if(!fude_reader_has(_c, _start + _record_size - _c->pos)) {
            return false;
        }
        _c->pos = _start + _record_size;

        const u32 _strokes = fude_get_u32(_c);
        if(!_c->ok || _strokes > FUDE_HISTORY_MAX_STROKES) {
            return false;
        }
        _sq.first_stroke = (u32)rde_arr_length(&_history->strokes);
        _sq.stroke_count = _strokes;

        for(u32 _k = 0; _k < _strokes; _k++) {
            const u32 _n = fude_get_u32(_c);
            if(!_c->ok || _n > FUDE_HISTORY_MAX_POINTS || !fude_reader_has(_c, _n * 8u)) {
                return false;
            }
            const fude_history_stroke _stroke = { (u32)rde_arr_length(&_history->points), _n };
            rde_arr_add(&_history->strokes, (any)&_stroke);
            if(_n == 0) {
                continue;
            }

            fude_history_point* _p = (fude_history_point*)rde_arr_add_n(&_history->points, _n);
            for(u32 _q = 0; _q < _n; _q++) {
                _p[_q].x    = fude_get_u16(_c);
                _p[_q].y    = fude_get_u16(_c);
                _p[_q].time = fude_get_f32(_c);
            }
        }

        rde_arr_add(&_history->squares, &_sq);
    }
    return _c->ok;
}

// The reader fude_history_load hands to fude_save_load: reads every session of the character into the fude_history
// that _user points to, starting from an empty one. A session of another code point, or past the sanity limits, is
// skipped. Returns false when the header is not that of a history file.
RDE_INTERNAL b8 fude_history_read(const u8* _data, u32 _size, any _user) {
    fude_history* _history  = (fude_history*)_user;
    const u32     _codepoint = _history->codepoint;
    rde_arr_clear(&_history->sessions);
    rde_arr_clear(&_history->squares);
    rde_arr_clear(&_history->strokes);
    rde_arr_clear(&_history->points);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
        return false;
    }

    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_HISTORY_CHUNK_SESSION) {
            continue;
        }

        fude_history_session _session;
        _session.time    = fude_get_u64(&_chunk);
        const u32 _cp    = fude_get_u32(&_chunk);
        _session.average = fude_get_f32(&_chunk);
        const u32 _count = fude_get_u32(&_chunk);
        const u32 _rec   = fude_get_u32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint || _count > FUDE_HISTORY_MAX_SQUARES || _rec < FUDE_HISTORY_SQUARE_SIZE) {
            continue;
        }

        // A damaged session is dropped whole, so remember where the arrays ended to roll back what it added.
        const u32 _squares0 = (u32)rde_arr_length(&_history->squares);
        const u32 _strokes0 = (u32)rde_arr_length(&_history->strokes);
        const u32 _points0  = (u32)rde_arr_length(&_history->points);
        _session.first_square = _squares0;
        _session.square_count = _count;

        if(fude_history_read_squares(_history, &_chunk, _count, _rec)) {
            rde_arr_add(&_history->sessions, &_session);
        } else {
            rde_arr_resize(&_history->squares, _squares0);
            rde_arr_resize(&_history->strokes, _strokes0);
            rde_arr_resize(&_history->points,  _points0);
        }
    }

    return true;
}

// Fills _history with _codepoint's sessions. Returns false, with _history empty, when there are none.
b8 fude_history_load(fude_history* _history, u32 _codepoint) {
    rde_arr_clear(&_history->sessions);
    rde_arr_clear(&_history->squares);
    rde_arr_clear(&_history->strokes);
    rde_arr_clear(&_history->points);
    _history->codepoint = _codepoint;

    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));
    fude_save_load(_path, fude_history_read, _history);   // Nothing practised yet means no sessions.
    return rde_arr_length(&_history->sessions) > 0;
}

// What fude_history_read_summary is handed: the character, and the summary it fills in.
typedef struct {
    u32                   codepoint;
    fude_history_summary* out;
} fude_history_summary_read;

// The reader fude_history_summarize hands to fude_save_load: sums up every session of the character from its time and
// average alone, starting from a zeroed summary. The squares are not read. Returns false when the header is not that
// of a history file.
RDE_INTERNAL b8 fude_history_read_summary(const u8* _data, u32 _size, any _user) {
    const fude_history_summary_read* _read      = (const fude_history_summary_read*)_user;
    const u32                        _codepoint = _read->codepoint;
    fude_history_summary*            _out       = _read->out;
    rde_memset(_out, 0, sizeof(*_out));
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_HISTORY_VERSION, FUDE_HISTORY_KIND)) {
        return false;
    }

    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_HISTORY_CHUNK_SESSION) {
            continue;
        }
        const u64 _time    = fude_get_u64(&_chunk);
        const u32 _cp      = fude_get_u32(&_chunk);
        const f32 _average = fude_get_f32(&_chunk);
        if(!_chunk.ok || _cp != _codepoint) {
            continue;
        }

        if(_out->sessions == 0) {
            _out->first = _average;
        }
        _out->sessions++;
        _out->last      = _average;
        _out->last_time = _time;
        _out->best      = _average > _out->best ? _average : _out->best;
    }

    return true;
}

// Sums up _codepoint's sessions into _out. Returns false, with _out zeroed, when there are none.
b8 fude_history_summarize(u32 _codepoint, fude_history_summary* _out) {
    rde_memset(_out, 0, sizeof(*_out));
    c8 _path[RDE_MAX_PATH];
    fude_history_path(_codepoint, _path, sizeof(_path));
    fude_history_summary_read _read = { .codepoint = _codepoint, .out = _out };
    fude_save_load(_path, fude_history_read_summary, &_read);   // Nothing practised yet means no sessions.
    return _out->sessions > 0;
}
