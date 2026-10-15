// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/examlog.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"

// ===========================================================================
// See examlog.h.
//
// In memory, the exams are one array and all their items another, each exam pointing at its run of items. Each exam
// also remembers which of the file's exam chunks it came from (counting damaged chunks too), so that
// fude_examlog_read_writing can find its drawings in the file without keeping them in memory. Adding an exam reads the
// whole file, appends one chunk and writes it all back atomically; the old bytes are copied unchanged, so an exam
// written by a newer version (with longer item records) survives.
// ===========================================================================

// The file's format version, the tags of its kind and of its exam chunk (both 'EXAM'), and the size in bytes of one
// item record as written. A reader accepts longer records and skips their extra bytes.
#define FUDE_EXAMLOG_VERSION     1u
#define FUDE_EXAMLOG_KIND        FUDE_TAG('E', 'X', 'A', 'M')
#define FUDE_EXAMLOG_CHUNK       FUDE_TAG('E', 'X', 'A', 'M')
#define FUDE_EXAMLOG_ITEM_SIZE   24u

// Every exam, oldest first, and every exam's items in one array. Created on first use (fude_examlog_ensure), so the
// log works in memory without a file, as the tests use it.
static rde_arr TYPE(fude_examlog_exam) fude_examlog_all;
static rde_arr TYPE(fude_examlog_item) fude_examlog_all_items;
static b8                              fude_examlog_ready = false;
// The file the exams are appended to, or "" to keep them in memory only.
static c8                              fude_examlog_path[RDE_MAX_PATH];
// Goes up with every change; fude_examlog_revision returns it.
static u32                             fude_examlog_changes = 0;
static u32                             fude_examlog_chunks  = 0;   // The file's exam chunks, damaged ones too.

// Creates the two arrays the first time they are needed.
RDE_INTERNAL void fude_examlog_ensure(void) {
    if(!fude_examlog_ready) {
        fude_examlog_all       = rde_arr_new(sizeof(fude_examlog_exam), rde_memory_allocator_get_default_std());
        fude_examlog_all_items = rde_arr_new(sizeof(fude_examlog_item), rde_memory_allocator_get_default_std());
        fude_examlog_ready     = true;
    }
}

// Adds one exam's results to memory: its items are appended to the items array, and its summary (how many were
// correct, the average points) is worked out from them. _chunk is the exam's chunk ordinal in the file, or UINT32_MAX
// when it is not in the file yet.
RDE_INTERNAL void fude_examlog_remember(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, u32 _chunk) {
    fude_examlog_exam _exam = { .time = _time, .source = _source, .first_item = (u32)rde_arr_length(&fude_examlog_all_items), .item_count = _count,
                                .chunk = _chunk };
    f32 _sum = 0.0f;
    for(u32 _i = 0; _i < _count; _i++) {
        rde_arr_add(&fude_examlog_all_items, &_items[_i]);
        _exam.correct += _items[_i].correct ? 1u : 0u;
        _sum          += _items[_i].score;
    }
    _exam.score = _count > 0 ? _sum / (f32)_count : 0.0f;
    rde_arr_add(&fude_examlog_all, &_exam);
}

// Reads the exam log file's bytes into memory, starting from no exams; fude_save_load (save.h) calls it, a second time
// for the .bak when the main file fails. Only the results are read, not the drawings. A damaged exam chunk is skipped
// but still counted, so the chunk ordinals stay those of the file. Returns false when the header is not that of an
// exam log of this version.
RDE_INTERNAL b8 fude_examlog_read(const u8* _data, u32 _size, any _user) {
    RDE_UNUSED(_user);
    rde_arr_clear(&fude_examlog_all);
    rde_arr_clear(&fude_examlog_all_items);
    fude_examlog_chunks = 0;
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
        return false;
    }
    // One exam's items, at most 256 as fude_examlog_add keeps; static so the 5 KB are not on the stack.
    static fude_examlog_item _items[256];
    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_EXAMLOG_CHUNK) {
            continue;
        }
        const u32 _ordinal = fude_examlog_chunks++;
        const u64 _time   = fude_get_u64(&_chunk);
        const u8  _source = fude_get_u8(&_chunk);
        fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk);
        const u32 _count  = fude_get_u32(&_chunk);
        const u32 _record = fude_get_u32(&_chunk);
        // A record must hold at least the 20 bytes read below; longer ones (a newer version) are skipped past.
        if(!_chunk.ok || _record < 20u || _count > 256u || (u64)_count * _record > (u64)(_chunk.size - _chunk.pos)) {
            continue;   // This exam is damaged; skip it, and the other exams still count.
        }
        for(u32 _i = 0; _i < _count; _i++) {
            fude_reader _e = fude_reader_make(&_chunk.data[_chunk.pos + _i * _record], _record);
            _items[_i].codepoint = fude_get_u32(&_e);
            _items[_i].correct   = fude_get_u8(&_e) != 0;
            fude_get_u8(&_e); fude_get_u8(&_e); fude_get_u8(&_e);
            _items[_i].score     = fude_get_f32(&_e);
            _items[_i].quality   = fude_get_f32(&_e);
            _items[_i].read_as   = fude_get_u32(&_e);
        }
        fude_examlog_remember(_time, _source, _items, _count, _ordinal);
    }
    return true;
}

// Replaces the exams in memory with those saved at _path (none for NULL). See the header.
void fude_examlog_open(const c8* _path) {
    fude_examlog_ensure();
    rde_arr_clear(&fude_examlog_all);
    rde_arr_clear(&fude_examlog_all_items);
    rde_snprintf(fude_examlog_path, sizeof(fude_examlog_path), "%s", _path != NULL ? _path : "");
    fude_examlog_changes++;
    fude_examlog_chunks = 0;
    if(_path == NULL) {
        return;   // Keep the exams in memory only, as the other models allow.
    }
    fude_save_load(_path, fude_examlog_read, NULL);   // When nothing has been saved yet, there are no exams.
}

// Frees the exams and forgets the path.
void fude_examlog_close(void) {
    if(fude_examlog_ready) {
        rde_arr_free(&fude_examlog_all);
        rde_arr_free(&fude_examlog_all_items);
        fude_examlog_ready = false;
    }
    fude_examlog_path[0] = 0;
    fude_examlog_changes++;
    fude_examlog_chunks = 0;
}

// Converts a coordinate in the drawing's units into the file's u16: a fraction of the answer square's side _units,
// times 65535. A point outside the square is clamped to its edge, and a square of no size gives 0.
RDE_INTERNAL u16 fude_examlog_fraction(f32 _v, f32 _units) {
    const f32 _f = _units > 0.0f ? rde_math_clamp_f32(_v / _units, 0.0f, 1.0f) : 0.0f;
    return (u16)(_f * 65535.0f + 0.5f);
}

// fude_save_load's reader for an add: the file's bytes, kept whole when its header is an exam log's.
RDE_INTERNAL b8 fude_examlog_read_whole(const u8* _data, u32 _size, any _user) {
    fude_bytes* _b = (fude_bytes*)_user;
    rde_arr_clear(_b);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
        return false;
    }
    fude_put_data(_b, _data, _size);
    return true;
}

// Adds one exam to memory and appends it to the file, with its drawings (only the strokes that are alive, at most
// 65535 strokes and 65535 points a stroke). The file as it is now is kept byte for byte, because the log is only ever
// added to: it is read as every save is (save.h), so a damaged file is set aside and its backup used instead, rather
// than a new file with this exam alone taking the place of every exam before it. See the header for the parameters and
// the return value.
b8 fude_examlog_add(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, const fude_ink* const* _drawings, f32 _units) {
    fude_examlog_ensure();
    _count = _count > 256u ? 256u : _count;
    if(fude_examlog_path[0] == 0) {
        fude_examlog_remember(_time, _source, _items, _count, UINT32_MAX);
        fude_examlog_changes++;
        return true;   // The exams are kept in memory only.
    }

    fude_bytes       _b   = fude_bytes_new(1024u + _count * 64u);
    const FUDE_LOAD_ _was = fude_save_load(fude_examlog_path, fude_examlog_read_whole, &_b);
    if(_was == FUDE_LOAD_RECOVERED) {
        // The file had to come from its backup, so what memory holds may not match it: it is read again from it.
        fude_examlog_read((const u8*)_b.memory, fude_bytes_size(&_b), NULL);
    } else if(_was != FUDE_LOAD_OK) {
        // No file yet (or none that reads): a new one, so the exams in memory have no chunk in it.
        rde_arr_clear(&_b);
        fude_put_header(&_b, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND);
        fude_examlog_exam* _exams = (fude_examlog_exam*)fude_examlog_all.memory;
        for(u32 _e = 0; _e < (u32)rde_arr_length(&fude_examlog_all); _e++) {
            _exams[_e].chunk = UINT32_MAX;
        }
        fude_examlog_chunks = 0;
    }
    fude_examlog_remember(_time, _source, _items, _count, UINT32_MAX);
    fude_examlog_changes++;
    // The new exam, the last one in memory, gets the chunk about to be appended.
    ((fude_examlog_exam*)fude_examlog_all.memory)[rde_arr_length(&fude_examlog_all) - 1u].chunk = fude_examlog_chunks++;

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_EXAMLOG_CHUNK);
    fude_put_u64(&_b, _time);
    fude_put_u8(&_b, _source);
    fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u);
    fude_put_u32(&_b, _count);
    fude_put_u32(&_b, FUDE_EXAMLOG_ITEM_SIZE);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_put_u32(&_b, _items[_i].codepoint);
        fude_put_u8(&_b, _items[_i].correct ? 1u : 0u);
        fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u); fude_put_u8(&_b, 0u);
        fude_put_f32(&_b, _items[_i].score);
        fude_put_f32(&_b, _items[_i].quality);
        fude_put_u32(&_b, _items[_i].read_as);
        fude_put_u32(&_b, 0u);
    }
    // Then each item's drawing: its live strokes first counted, then written point by point.
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_ink* _ink = _drawings != NULL ? _drawings[_i] : NULL;
        u32 _strokes = 0;
        for(u32 _s = 0; _ink != NULL && _s < fude_ink_stroke_count(_ink); _s++) {
            _strokes += fude_ink_stroke_at(_ink, _s)->alive ? 1u : 0u;
        }
        fude_put_u16(&_b, (u16)(_strokes < 65535u ? _strokes : 65535u));
        for(u32 _s = 0; _ink != NULL && _s < fude_ink_stroke_count(_ink); _s++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
            if(!_stroke->alive) {
                continue;
            }
            const u32 _n = _stroke->point_count < 65535u ? _stroke->point_count : 65535u;
            const fude_ink_point* _p = fude_ink_stroke_points(_ink, _stroke);
            fude_put_u16(&_b, (u16)_n);
            for(u32 _k = 0; _k < _n; _k++) {
                fude_put_u16(&_b, fude_examlog_fraction(_p[_k].position.x, _units));
                fude_put_u16(&_b, fude_examlog_fraction(_p[_k].position.y, _units));
            }
        }
    }
    fude_chunk_end(&_b, _chunk);
    return fude_bytes_write_and_free(&_b, fude_examlog_path, NULL);
}

// Returns how many exams are kept.
u32 fude_examlog_count(void) {
    return fude_examlog_ready ? (u32)rde_arr_length(&fude_examlog_all) : 0u;
}

// Returns the exams, oldest first, or NULL before the log is used. See the header for how long it stays valid.
const fude_examlog_exam* fude_examlog_exams(void) {
    return fude_examlog_ready ? (const fude_examlog_exam*)fude_examlog_all.memory : NULL;
}

// Returns every exam's items in one array, or NULL before the log is used.
const fude_examlog_item* fude_examlog_items(void) {
    return fude_examlog_ready ? (const fude_examlog_item*)fude_examlog_all_items.memory : NULL;
}

// Counts, from the latest exam back, the exams that had _codepoint right, stopping at the first that had it wrong.
// Exams without it are passed over; an exam that asked it twice counts by its first answer.
u32 fude_examlog_streak(u32 _codepoint) {
    const fude_examlog_exam* _exams = fude_examlog_exams();
    const fude_examlog_item* _items = fude_examlog_items();
    u32 _streak = 0;
    for(u32 _e = fude_examlog_count(); _e-- > 0;) {
        for(u32 _i = 0; _i < _exams[_e].item_count; _i++) {
            const fude_examlog_item* _it = &_items[_exams[_e].first_item + _i];
            if(_it->codepoint != _codepoint) {
                continue;
            }
            if(!_it->correct) {
                return _streak;
            }
            _streak++;
            break;   // Each exam counts once.
        }
    }
    return _streak;
}

// Returns the change counter, which goes up with every exam added so a view knows when to look again.
u32 fude_examlog_revision(void) {
    return fude_examlog_changes;
}

// Reads the wanted drawings back from the file (see the header). It maps each of the file's exam chunks to the exam
// in memory that came from it, then walks the chunks, skipping the item records, and copies the wanted items' strokes
// and points. A chunk whose item count is not that of the exam remembered is left alone. A drawing that runs past its
// chunk's end (a damaged file) is not kept: the strokes and points it had added are taken back off the caller's
// arrays, as history.c does, so none is left without its writing. The points get a steady timing,
// FUDE_EXAMLOG_POINT_SECONDS apart, because the file keeps none.
u32 fude_examlog_read_writing(u32 _exam, u32 _codepoint, rde_arr* _writings, rde_arr* _strokes, rde_arr* _points) {
    const u32 _count = fude_examlog_count();
    if(_count == 0 || fude_examlog_path[0] == 0 || !rde_file_exists(fude_examlog_path)) {
        return 0;
    }
    // Which exam each chunk of the file belongs to; UINT32_MAX for a chunk that is not wanted.
    const fude_examlog_exam* _exams    = fude_examlog_exams();
    const fude_examlog_item* _items    = fude_examlog_items();
    u32*                     _by_chunk = (u32*)rde_malloc(sizeof(u32) * (fude_examlog_chunks + 1u));
    for(u32 _c = 0; _c <= fude_examlog_chunks; _c++) {
        _by_chunk[_c] = UINT32_MAX;
    }
    for(u32 _e = 0; _e < _count; _e++) {
        if(_exams[_e].chunk < fude_examlog_chunks && (_exam == UINT32_MAX || _e == _exam)) {
            _by_chunk[_exams[_e].chunk] = _e;
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(fude_examlog_path, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    u32 _found = 0;
    if(_data != NULL && fude_read_header(&_r, FUDE_EXAMLOG_VERSION, FUDE_EXAMLOG_KIND)) {
        u32         _tag;
        fude_reader _chunk;
        u32         _ordinal = 0;
        while(fude_next_chunk(&_r, &_tag, &_chunk)) {
            if(_tag != FUDE_EXAMLOG_CHUNK) {
                continue;
            }
            const u32 _e = _ordinal < fude_examlog_chunks ? _by_chunk[_ordinal] : UINT32_MAX;
            _ordinal++;
            if(_e == UINT32_MAX) {
                continue;
            }
            fude_get_u32(&_chunk); fude_get_u32(&_chunk);                       // The time, as two halves.
            fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk); fude_get_u8(&_chunk);
            const u32 _n      = fude_get_u32(&_chunk);
            const u32 _record = fude_get_u32(&_chunk);
            if(!_chunk.ok || _n != _exams[_e].item_count || (u64)_n * _record > (u64)(_chunk.size - _chunk.pos)) {
                continue;   // This is not the exam remembered from this chunk, so leave it alone.
            }
            _chunk.pos += _n * _record;
            for(u32 _i = 0; _i < _n && _chunk.ok; _i++) {
                const b8  _wanted  = _codepoint == 0 || _items[_exams[_e].first_item + _i].codepoint == _codepoint;
                const u32 _nstroke = fude_get_u16(&_chunk);
                const u32 _points0 = (u32)rde_arr_length(_points);   // The lengths before this drawing, to roll back to.
                fude_examlog_writing _w = { .exam = _e, .item = _i, .first_stroke = (u32)rde_arr_length(_strokes), .stroke_count = 0 };
                for(u32 _s = 0; _s < _nstroke && _chunk.ok; _s++) {
                    const u32 _np = fude_get_u16(&_chunk);
                    if(!fude_reader_has(&_chunk, _np * 4u)) {
                        _chunk.ok = false;
                        break;
                    }
                    if(!_wanted || _np == 0) {
                        _chunk.pos += _np * 4u;
                        continue;
                    }
                    const fude_history_stroke _stroke = { .first_point = (u32)rde_arr_length(_points), .point_count = _np };
                    fude_history_point*       _p      = (fude_history_point*)rde_arr_add_n(_points, _np);
                    for(u32 _k = 0; _k < _np; _k++) {
                        _p[_k].x    = fude_get_u16(&_chunk);
                        _p[_k].y    = fude_get_u16(&_chunk);
                        _p[_k].time = (f32)_k * FUDE_EXAMLOG_POINT_SECONDS;
                    }
                    rde_arr_add(_strokes, &_stroke);
                    _w.stroke_count++;
                }
                if(!_chunk.ok) {
                    rde_arr_resize(_strokes, _w.first_stroke);   // Damaged: what it added goes.
                    rde_arr_resize(_points, _points0);
                } else if(_wanted) {
                    rde_arr_add(_writings, &_w);
                    _found++;
                }
            }
        }
    }
    fude_file_free(_data);
    rde_free(_by_chunk);
    return _found;
}
