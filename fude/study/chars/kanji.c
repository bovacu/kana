#include "study/chars/kanji.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See kanji.h.
// ===========================================================================

#define FUDE_KANJI_STROKE_HEADER 8u     // 4 bytes of counts/types + start x, y
#define FUDE_KANJI_SEGMENT_SIZE  12u    // 6 x i16
#define FUDE_KANJI_MAX_DEPTH     10u    // Bézier subdivision limit

void fude_kanji_unload(fude_kanji_db* _db) {
    rde_free(_db->_word_text);
    rde_free((any)_db->_sentences);
    fude_file_free(_db->_file);
    fude_file_free(_db->_strokes_file);
    memset(_db, 0, sizeof(*_db));
}

// The WORD chunk (_chunk just past its count, which matched the records): the
// lists in place, and where each word starts, found once — the strings are
// checked to be all there, so reading them later needs no checks.
RDE_INTERNAL void fude_kanji_load_words(fude_kanji_db* _db, fude_reader* _chunk) {
    const u32 _count = _db->count;
    if((u64)_count * 4u + 8u > (u64)(_chunk->size - _chunk->pos)) {
        return;
    }
    const u8* _index = &_chunk->data[_chunk->pos];
    _chunk->pos     += _count * 4u;
    const u32 _words = fude_get_u32(_chunk);
    const u32 _lists = fude_get_u32(_chunk);
    if(!_chunk->ok || _lists > _chunk->size - _chunk->pos || _words == 0) {
        return;
    }
    const u8* _list_data = &_chunk->data[_chunk->pos];
    const c8* _text      = (const c8*)&_chunk->data[_chunk->pos + _lists];
    const u32 _text_size = _chunk->size - _chunk->pos - _lists;

    const c8** _starts = (const c8**)rde_malloc(sizeof(const c8*) * _words);
    u32        _at     = 0;
    for(u32 _w = 0; _w < _words; _w++) {
        _starts[_w] = &_text[_at];
        for(u32 _s = 0; _s < 3u; _s++) {   // written, reading, meaning
            const c8* _nul = _at < _text_size ? memchr(&_text[_at], 0, _text_size - _at) : NULL;
            if(_nul == NULL) {
                rde_free(_starts);
                return;   // cut short: no words rather than broken ones
            }
            _at = (u32)(_nul - _text) + 1u;
        }
    }

    _db->_words_index     = _index;
    _db->_word_lists      = _list_data;
    _db->_word_lists_size = _lists;
    _db->word_count       = _words;
    _db->_word_text       = _starts;
}

// An 'LNxx' chunk: its pairs and text in place, when all there — the text
// ends with a NUL, so every string in it does.
RDE_INTERNAL void fude_kanji_load_language(fude_kanji_db* _db, u32 _tag, fude_reader* _chunk) {
    if(_db->_language_count >= FUDE_KANJI_LANGUAGES) {
        return;
    }
    fude_kanji_language _l = { .code = { (c8)((_tag >> 16) & 0xFFu), (c8)((_tag >> 24) & 0xFFu), 0 } };
    _l.kanji_count = fude_get_u32(_chunk);
    if(!_chunk->ok || (u64)_l.kanji_count * 8u > (u64)(_chunk->size - _chunk->pos)) {
        return;
    }
    _l.kanji       = &_chunk->data[_chunk->pos];
    _chunk->pos   += _l.kanji_count * 8u;
    _l.word_count  = fude_get_u32(_chunk);
    if(!_chunk->ok || (u64)_l.word_count * 8u > (u64)(_chunk->size - _chunk->pos)) {
        return;
    }
    _l.words       = &_chunk->data[_chunk->pos];
    _chunk->pos   += _l.word_count * 8u;
    _l.text_size   = fude_get_u32(_chunk);
    if(!_chunk->ok || _l.text_size == 0 || _l.text_size > _chunk->size - _chunk->pos || _chunk->data[_chunk->pos + _l.text_size - 1u] != 0) {
        return;
    }
    _l.text = (const c8*)&_chunk->data[_chunk->pos];
    _db->_languages[_db->_language_count++] = _l;
}

// A pair list's text for _key (pairs sorted by key), or NULL.
RDE_INTERNAL const c8* fude_kanji_lookup(const fude_kanji_language* _l, const u8* _pairs, u32 _count, u32 _key) {
    u32 _lo = 0, _hi = _count;
    while(_lo < _hi) {
        const u32   _mid = (_lo + _hi) / 2u;
        fude_reader _r   = fude_reader_make(&_pairs[(usize)_mid * 8u], 8u);
        const u32   _k   = fude_get_u32(&_r);
        const u32   _at  = fude_get_u32(&_r);
        if(_k == _key) {
            return _at < _l->text_size ? &_l->text[_at] : NULL;
        }
        if(_k < _key) { _lo = _mid + 1u; }
        else          { _hi = _mid; }
    }
    return NULL;
}

void fude_kanji_set_language(fude_kanji_db* _db, const c8* _code) {
    _db->_language = -1;
    for(u32 _i = 0; _code != NULL && _i < _db->_language_count; _i++) {
        if(strcmp(_db->_languages[_i].code, _code) == 0) {
            _db->_language = (i32)_i;
        }
    }
}

// The 'SENT' chunk: an index to each sentence (five strings each, all checked
// there), and the words' table when it is for these words.
RDE_INTERNAL void fude_kanji_load_sentences(fude_kanji_db* _db, fude_reader* _r) {
    const u32 _n    = fude_get_u32(_r);
    const u32 _size = fude_get_u32(_r);
    if(!_r->ok || _n == 0 || (u64)_size > (u64)(_r->size - _r->pos)) {
        return;
    }
    const c8* _text  = (const c8*)&_r->data[_r->pos];
    const c8** _starts = (const c8**)rde_malloc(sizeof(c8*) * _n);
    u32       _at    = 0;
    for(u32 _s = 0; _s < _n; _s++) {
        _starts[_s] = &_text[_at];
        for(u32 _k = 0; _k < 5u; _k++) {
            const c8* _nul = _at < _size ? memchr(&_text[_at], 0, _size - _at) : NULL;
            if(_nul == NULL) {
                rde_free(_starts);
                return;   // cut short: no sentences rather than broken ones
            }
            _at = (u32)(_nul - _text) + 1u;
        }
    }
    _r->pos += _size;
    const u32 _words = fude_get_u32(_r);
    if(!_r->ok || _words != _db->word_count || (u64)_words * 4u > (u64)(_r->size - _r->pos)) {
        rde_free(_starts);
        return;   // for other words: none rather than wrong ones
    }
    _db->_sentences     = _starts;
    _db->sentence_count = _n;
    _db->_sentence_of   = &_r->data[_r->pos];
}

b8 fude_kanji_load(fude_kanji_db* _db, const c8* _path) {
    memset(_db, 0, sizeof(*_db));
    _db->_language = -1;

    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: no character data at %s (run the desktop build with --bake)", _path);
        return false;
    }

    _db->_file = fude_file_read(_path, &_db->_file_size);
    fude_reader _r = fude_reader_make(_db->_file, _db->_file_size);

    if(!fude_read_header(&_r, FUDE_KANJI_VERSION, FUDE_KANJI_KIND)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: %s is not a character data file this build reads", _path);
        fude_kanji_unload(_db);
        return false;
    }

    u32         _tag;
    fude_reader _chunk;
    u32         _parts_count = 0;
    u32         _words_count = 0;
    fude_reader _words       = { 0 };
    const u8*   _freq        = NULL;
    u32         _freq_count  = 0;
    fude_reader _sent        = { 0 };
    b8          _has_sent    = false;
    const u8*   _look        = NULL;
    u32         _look_count  = 0;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == FUDE_KANJI_CHUNK_CHARS) {
            const u32 _count  = fude_get_u32(&_chunk);
            const u32 _record = fude_get_u32(&_chunk);
            if(_chunk.ok && _record == FUDE_KANJI_RECORD_SIZE && (u64)_count * _record <= (u64)(_chunk.size - _chunk.pos)) {
                _db->count    = _count;
                _db->_records = &_chunk.data[_chunk.pos];
            }
        } else if(_tag == FUDE_KANJI_CHUNK_GEOM) {
            _db->_geometry      = _chunk.data;
            _db->_geometry_size = _chunk.size;
        } else if(_tag == FUDE_KANJI_CHUNK_TEXT) {
            _db->_text      = (const c8*)_chunk.data;
            _db->_text_size = _chunk.size;
        } else if(_tag == FUDE_KANJI_CHUNK_PARTS) {
            const u32 _count = fude_get_u32(&_chunk);
            if(_chunk.ok && (u64)_count * 4u <= (u64)(_chunk.size - _chunk.pos)) {
                _db->_parts_index = &_chunk.data[_chunk.pos];
                _db->_parts       = &_chunk.data[_chunk.pos + _count * 4u];
                _db->_parts_size  = _chunk.size - _chunk.pos - _count * 4u;
                _parts_count      = _count;
            }
        } else if(_tag == FUDE_KANJI_CHUNK_WORDS) {
            _words_count = fude_get_u32(&_chunk);
            _words       = _chunk;
        } else if(_tag == FUDE_KANJI_CHUNK_LOOK) {
            const u32 _count = fude_get_u32(&_chunk);
            if(_chunk.ok && (u64)_count * FUDE_KANJI_LOOKALIKES * 4u <= (u64)(_chunk.size - _chunk.pos)) {
                _look       = &_chunk.data[_chunk.pos];
                _look_count = _count;
            }
        } else if(_tag == FUDE_KANJI_CHUNK_SENTENCES) {
            _sent     = _chunk;
            _has_sent = true;
        } else if(_tag == FUDE_KANJI_CHUNK_WORD_FREQ) {
            const u32 _count = fude_get_u32(&_chunk);
            if(_chunk.ok && (u64)_count * 2u <= (u64)(_chunk.size - _chunk.pos)) {
                _freq       = &_chunk.data[_chunk.pos];
                _freq_count = _count;
            }
        } else if((_tag & 0xFFFFu) == (FUDE_TAG('L', 'N', 0, 0) & 0xFFFFu)) {
            fude_kanji_load_language(_db, _tag, &_chunk);
        }
    }

    // No strokes in it: they are in their own file beside it (kanji.h).
    if(_r.ok && _db->_geometry == NULL) {
        c8 _strokes[RDE_MAX_PATH];
        const c8* _slash = strrchr(_path, '/');
        snprintf(_strokes, sizeof(_strokes), "%.*s%s", _slash != NULL ? (int)(_slash - _path + 1) : 0, _path, FUDE_KANJI_STROKES_FILE);
        u32 _size = 0;
        _db->_strokes_file = rde_file_exists(_strokes) ? fude_file_read(_strokes, &_size) : NULL;
        fude_reader _s = fude_reader_make(_db->_strokes_file, _size);
        if(_db->_strokes_file != NULL && fude_read_header(&_s, FUDE_KANJI_VERSION, FUDE_KANJI_STROKES_KIND)) {
            while(fude_next_chunk(&_s, &_tag, &_chunk)) {
                if(_tag == FUDE_KANJI_CHUNK_GEOM) {
                    _db->_geometry      = _chunk.data;
                    _db->_geometry_size = _chunk.size;
                }
            }
        }
    }

    if(!_r.ok || _db->_records == NULL || _db->_geometry == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "kana: %s is damaged", _path);
        fude_kanji_unload(_db);
        return false;
    }
    if(_parts_count != _db->count) {
        _db->_parts_index = NULL;   // parts for another set of records: none rather than wrong ones
        _db->_parts       = NULL;
        _db->_parts_size  = 0;
    }
    if(_words_count == _db->count) {
        fude_kanji_load_words(_db, &_words);
    }
    _db->_word_freq = _freq != NULL && _freq_count == _db->word_count ? _freq : NULL;   // for these words, or none
    if(_has_sent) {
        fude_kanji_load_sentences(_db, &_sent);
    }
    _db->_look = _look != NULL && _look_count == _db->count ? _look : NULL;   // for these characters, or none

    return true;
}

u32 fude_kanji_lookalikes(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max) {
    if(_db->_look == NULL || _index >= _db->count) {
        return 0u;
    }
    u32 _n = 0;
    for(u32 _k = 0; _k < FUDE_KANJI_LOOKALIKES && _n < _max; _k++) {
        const u8* _p  = &_db->_look[((usize)_index * FUDE_KANJI_LOOKALIKES + _k) * 4u];
        const u32 _cp = (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
        if(_cp != 0u) {
            _out[_n++] = _cp;
        }
    }
    return _n;
}

u32 fude_kanji_words(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max, u32* _examples) {
    if(_examples != NULL) {
        *_examples = 0;
    }
    if(_db->_words_index == NULL || _index >= _db->count) {
        return 0;
    }

    fude_reader _ix = fude_reader_make(&_db->_words_index[(usize)_index * 4u], 4u);
    const u32 _at   = fude_get_u32(&_ix);
    if(_at == UINT32_MAX || _at >= _db->_word_lists_size) {
        return 0;
    }

    fude_reader _r    = fude_reader_make(&_db->_word_lists[_at], _db->_word_lists_size - _at);
    const u32   _n    = fude_get_u8(&_r);
    const u32   _show = fude_get_u8(&_r);
    u32         _k    = 0;
    if(_examples != NULL && _r.ok) {
        *_examples = _show < _max ? _show : _max;
    }
    for(u32 _i = 0; _i < _n && _k < _max; _i++) {
        const u32 _word = fude_get_u32(&_r);
        if(!_r.ok) {
            break;
        }
        if(_word < _db->word_count) {
            _out[_k++] = _word;
        }
    }
    return _k;
}

b8 fude_kanji_word_at(const fude_kanji_db* _db, u32 _word, fude_kanji_word* _out) {
    if(_db->_word_text == NULL || _word >= _db->word_count) {
        return false;
    }
    _out->written = _db->_word_text[_word];
    _out->reading = _out->written + strlen(_out->written) + 1u;
    _out->meaning = _out->reading + strlen(_out->reading) + 1u;
    if(_db->_language >= 0) {
        const fude_kanji_language* _l = &_db->_languages[_db->_language];
        const c8*                  _m = fude_kanji_lookup(_l, _l->words, _l->word_count, _word);
        if(_m != NULL && _m[0] != 0) {
            _out->meaning = _m;
        }
    }
    return true;
}

b8 fude_kanji_word_sentence(const fude_kanji_db* _db, u32 _word, fude_kanji_sentence* _out) {
    if(_db->_sentences == NULL || _db->_sentence_of == NULL || _word >= _db->word_count) {
        return false;
    }
    const u8* _p = &_db->_sentence_of[(usize)_word * 4u];
    const u32 _s = (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
    if(_s >= _db->sentence_count) {
        return false;
    }
    // Its texts: the Japanese, then en, es, fr, pt.
    const c8* _texts[5];
    _texts[0] = _db->_sentences[_s];
    for(u32 _k = 1; _k < 5u; _k++) {
        _texts[_k] = _texts[_k - 1u] + strlen(_texts[_k - 1u]) + 1u;
    }
    u32 _slot = 1u;   // English
    if(_db->_language >= 0) {
        const c8* _code = _db->_languages[_db->_language].code;
        _slot = strcmp(_code, "es") == 0 ? 2u : strcmp(_code, "fr") == 0 ? 3u : strcmp(_code, "pt") == 0 ? 4u : 1u;
    }
    _out->japanese    = _texts[0];
    _out->translation = _texts[_slot][0] != 0 ? _texts[_slot] : _texts[1];
    return true;
}

u16 fude_kanji_word_freq(const fude_kanji_db* _db, u32 _word) {
    if(_db->_word_freq == NULL || _word >= _db->word_count) {
        return 0xFFFFu;
    }
    return (u16)(_db->_word_freq[_word * 2u] | (_db->_word_freq[_word * 2u + 1u] << 8));
}

const c8* fude_kanji_word_meaning_english(const fude_kanji_db* _db, u32 _word) {
    if(_db->_word_text == NULL || _word >= _db->word_count) {
        return "";
    }
    const c8* _reading = _db->_word_text[_word] + strlen(_db->_word_text[_word]) + 1u;
    return _reading + strlen(_reading) + 1u;
}

b8 fude_kanji_has_parts(const fude_kanji_db* _db) {
    return _db->_parts_index != NULL;
}

u32 fude_kanji_parts(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max) {
    if(_db->_parts_index == NULL || _index >= _db->count) {
        return 0;
    }

    fude_reader _ix = fude_reader_make(&_db->_parts_index[(usize)_index * 4u], 4u);
    const u32 _at   = fude_get_u32(&_ix);
    if(_at == UINT32_MAX || _at >= _db->_parts_size) {
        return 0;
    }

    fude_reader _r = fude_reader_make(&_db->_parts[_at], _db->_parts_size - _at);
    const u32   _n = fude_get_u8(&_r);
    u32         _k = 0;
    for(u32 _i = 0; _i < _n && _k < _max; _i++) {
        const u32 _cp = fude_get_u32(&_r);
        if(!_r.ok) {
            break;
        }
        _out[_k++] = _cp;
    }
    return _k;
}

// --- records ---------------------------------------------------------------------

RDE_INTERNAL void fude_kanji_decode(const u8* _record, fude_kanji_info* _out) {
    fude_reader _r = fude_reader_make(_record, FUDE_KANJI_RECORD_SIZE);
    _out->codepoint = fude_get_u32(&_r);
    _out->geometry  = fude_get_u32(&_r);
    _out->text      = fude_get_u32(&_r);
    _out->frequency = fude_get_u16(&_r);
    _out->strokes   = fude_get_u8(&_r);
    _out->grade     = fude_get_u8(&_r);
    _out->jlpt      = fude_get_u8(&_r);
    _out->radical   = fude_get_u8(&_r);
    _out->level    = fude_get_u8(&_r);
}

b8 fude_kanji_at(const fude_kanji_db* _db, u32 _index, fude_kanji_info* _out) {
    if(_index >= _db->count) {
        return false;
    }

    fude_kanji_decode(&_db->_records[(usize)_index * FUDE_KANJI_RECORD_SIZE], _out);
    return true;
}

b8 fude_kanji_find(const fude_kanji_db* _db, u32 _codepoint, fude_kanji_info* _out) {
    u32 _index = 0;
    return fude_kanji_find_index(_db, _codepoint, &_index) && fude_kanji_at(_db, _index, _out);
}

b8 fude_kanji_find_index(const fude_kanji_db* _db, u32 _codepoint, u32* _index) {
    u32 _lo = 0;
    u32 _hi = _db->count;

    // The records are sorted by code point.
    while(_lo < _hi) {
        const u32 _mid = _lo + (_hi - _lo) / 2u;
        fude_kanji_info _info;
        fude_kanji_decode(&_db->_records[(usize)_mid * FUDE_KANJI_RECORD_SIZE], &_info);

        if(_info.codepoint == _codepoint) {
            *_index = _mid;
            return true;
        }

        if(_info.codepoint < _codepoint) {
            _lo = _mid + 1u;
        } else {
            _hi = _mid;
        }
    }

    return false;
}

// --- strokes ---------------------------------------------------------------------

RDE_INTERNAL u32 fude_kanji_type(u8 _stored) {
    return _stored == 0 ? 0u : FUDE_KANJI_STROKE_BASE + (u32)_stored - 1u;
}

RDE_INTERNAL f32 fude_kanji_coord(fude_reader* _r) {
    return (f32)fude_get_i16(_r) / FUDE_KANJI_FIXED;
}

b8 fude_kanji_stroke_at(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _index, fude_kanji_stroke* _out) {
    if(_index >= _info->strokes || _info->geometry >= _db->_geometry_size) {
        return false;
    }

    fude_reader _r = fude_reader_make(_db->_geometry, _db->_geometry_size);
    _r.pos = _info->geometry;

    // Strokes are variable length: walk past the ones before.
    for(u32 _s = 0; _s < _index; _s++) {
        const u32 _segments = fude_get_u8(&_r);
        const u32 _skip     = (FUDE_KANJI_STROKE_HEADER - 1u) + _segments * FUDE_KANJI_SEGMENT_SIZE;
        if(!fude_reader_has(&_r, _skip)) {
            return false;
        }
        _r.pos += _skip;
    }

    _out->segments    = fude_get_u8(&_r);
    _out->type        = fude_kanji_type(fude_get_u8(&_r));
    _out->variant     = (c8)fude_get_u8(&_r);
    _out->alternative = fude_kanji_type(fude_get_u8(&_r));
    _out->start.x     = fude_kanji_coord(&_r);
    _out->start.y     = fude_kanji_coord(&_r);
    _out->_data       = &_r.data[_r.pos];

    return _r.ok && fude_reader_has(&_r, _out->segments * FUDE_KANJI_SEGMENT_SIZE);
}

// Distance from _p to the line through _a and _b (to _a when they coincide).
RDE_INTERNAL f32 fude_kanji_line_distance(rde_vec_2F _p, rde_vec_2F _a, rde_vec_2F _b) {
    const f32 _dx  = _b.x - _a.x;
    const f32 _dy  = _b.y - _a.y;
    const f32 _len = sqrtf(_dx * _dx + _dy * _dy);

    if(_len < 1e-6f) {
        return sqrtf((_p.x - _a.x) * (_p.x - _a.x) + (_p.y - _a.y) * (_p.y - _a.y));
    }

    return fabsf(_dx * (_p.y - _a.y) - _dy * (_p.x - _a.x)) / _len;
}

// Emits the curve's points after p0: split in half until the control points lie
// within _tolerance of the chord, which is then close enough to the curve.
RDE_INTERNAL void fude_kanji_flatten(rde_vec_2F _p0, rde_vec_2F _p1, rde_vec_2F _p2, rde_vec_2F _p3,
                                     f32 _tolerance, u32 _depth, rde_vec_2F* _out, u32 _max, u32* _count) {
    const f32 _d1 = fude_kanji_line_distance(_p1, _p0, _p3);
    const f32 _d2 = fude_kanji_line_distance(_p2, _p0, _p3);

    if(_depth >= FUDE_KANJI_MAX_DEPTH || (_d1 <= _tolerance && _d2 <= _tolerance)) {
        if(*_count < _max) {
            _out[(*_count)++] = _p3;
        }
        return;
    }

    // de Casteljau at t = 0.5.
    const rde_vec_2F _a   = { (_p0.x + _p1.x) * 0.5f, (_p0.y + _p1.y) * 0.5f };
    const rde_vec_2F _b   = { (_p1.x + _p2.x) * 0.5f, (_p1.y + _p2.y) * 0.5f };
    const rde_vec_2F _c   = { (_p2.x + _p3.x) * 0.5f, (_p2.y + _p3.y) * 0.5f };
    const rde_vec_2F _ab  = { (_a.x + _b.x) * 0.5f, (_a.y + _b.y) * 0.5f };
    const rde_vec_2F _bc  = { (_b.x + _c.x) * 0.5f, (_b.y + _c.y) * 0.5f };
    const rde_vec_2F _mid = { (_ab.x + _bc.x) * 0.5f, (_ab.y + _bc.y) * 0.5f };

    fude_kanji_flatten(_p0, _a, _ab, _mid, _tolerance, _depth + 1u, _out, _max, _count);
    fude_kanji_flatten(_mid, _bc, _c, _p3, _tolerance, _depth + 1u, _out, _max, _count);
}

void fude_kanji_stroke_segment(const fude_kanji_stroke* _stroke, u32 _segment, rde_vec_2F _out[3]) {
    fude_reader _r = fude_reader_make(_stroke->_data, _stroke->segments * FUDE_KANJI_SEGMENT_SIZE);
    _r.pos         = _segment < _stroke->segments ? _segment * FUDE_KANJI_SEGMENT_SIZE : _r.size;
    for(u32 _i = 0; _i < 3u; _i++) {
        const f32 _x = fude_kanji_coord(&_r);   // one read per statement (see below)
        const f32 _y = fude_kanji_coord(&_r);
        _out[_i]     = _r.ok ? (rde_vec_2F){ _x, _y } : _stroke->start;
    }
}

u32 fude_kanji_stroke_points(const fude_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, u32 _max) {
    if(_max == 0) {
        return 0;
    }

    u32        _count = 0;
    rde_vec_2F _p0    = _stroke->start;
    _out[_count++]    = _p0;

    fude_reader _r = fude_reader_make(_stroke->_data, _stroke->segments * FUDE_KANJI_SEGMENT_SIZE);
    for(u32 _s = 0; _s < _stroke->segments; _s++) {
        // One read per statement: the order of side effects inside an initializer
        // list is unspecified in C, and x and y could swap.
        f32 _v[6];
        for(u32 _i = 0; _i < 6u; _i++) {
            _v[_i] = fude_kanji_coord(&_r);
        }
        const rde_vec_2F _p1 = { _v[0], _v[1] };
        const rde_vec_2F _p2 = { _v[2], _v[3] };
        const rde_vec_2F _p3 = { _v[4], _v[5] };
        fude_kanji_flatten(_p0, _p1, _p2, _p3, _tolerance > 1e-4f ? _tolerance : 1e-4f, 0u, _out, _max, &_count);
        _p0 = _p3;
    }

    return _count;
}

// --- text ------------------------------------------------------------------------

// The _nth NUL-terminated string at the character's text offset.
RDE_INTERNAL const c8* fude_kanji_text(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _nth) {
    if(_db->_text == NULL || _info->text >= _db->_text_size) {
        return "";
    }

    u32 _at = _info->text;
    for(u32 _i = 0; _i < _nth; _i++) {
        const c8* _nul = memchr(&_db->_text[_at], 0, _db->_text_size - _at);
        if(_nul == NULL) {
            return "";
        }
        _at = (u32)(_nul - _db->_text) + 1u;
        if(_at >= _db->_text_size) {
            return "";
        }
    }

    // Only a string that ends inside the chunk.
    return memchr(&_db->_text[_at], 0, _db->_text_size - _at) != NULL ? &_db->_text[_at] : "";
}

const c8* fude_kanji_reading(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _kind) { return _kind < 2u ? fude_kanji_text(_db, _info, _kind) : ""; }
const c8* fude_kanji_meanings_english(const fude_kanji_db* _db, const fude_kanji_info* _info) { return fude_kanji_text(_db, _info, 2); }

const c8* fude_kanji_meanings(const fude_kanji_db* _db, const fude_kanji_info* _info) {
    if(_db->_language >= 0) {
        const fude_kanji_language* _l = &_db->_languages[_db->_language];
        const c8*                  _m = fude_kanji_lookup(_l, _l->kanji, _l->kanji_count, _info->codepoint);
        if(_m != NULL && _m[0] != 0) {
            return _m;
        }
    }
    return fude_kanji_text(_db, _info, 2);
}

