// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/vocab.h"
#include "drawing/base/kfile.h"
#include "drawing/base/text.h"
#include "lang/lang.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See vocab.h. A few thousand words at most: one array in the order saved,
// searched in full; the lists, each an array of word ids.
// ===========================================================================

#define FUDE_VOCAB_VERSION    1u
#define FUDE_VOCAB_KIND       FUDE_TAG('U', 'W', 'R', 'D')
#define FUDE_VOCAB_CHUNK      FUDE_TAG('V', 'O', 'C', 'B')
#define FUDE_VOCAB_LIST_CHUNK FUDE_TAG('L', 'I', 'S', 'T')
#define FUDE_VOCAB_SENT_CHUNK FUDE_TAG('S', 'E', 'N', 'T')
#define FUDE_VOCAB_OLD_CHUNK  FUDE_TAG('W', 'R', 'D', 'S')

typedef struct {
    u32               id;
    u64               created;
    c8                name[FUDE_VOCAB_LIST_NAME];
    rde_arr TYPE(u32) words;
} fude_vocab_list;

// A word's sentence (few words have one: kept apart).
typedef struct {
    u32 id;
    c8  japanese[FUDE_VOCAB_SENTENCE];
    c8  translation[FUDE_VOCAB_SENTENCE];
} fude_vocab_sent;

static rde_arr TYPE(fude_vocab_word) fude_vocab_words;
static rde_arr TYPE(fude_vocab_sent) fude_vocab_sents;
static fude_vocab_list               fude_vocab_lists[FUDE_VOCAB_LISTS];
static u32                           fude_vocab_list_n   = 0;
static u32                           fude_vocab_next     = 1;   // the next word id
static u32                           fude_vocab_next_list = 1;
static b8                            fude_vocab_ready    = false;
static c8                            fude_vocab_path[RDE_MAX_PATH];
static u32                           fude_vocab_changes  = 0;

RDE_INTERNAL void fude_vocab_ensure(void) {
    if(!fude_vocab_ready) {
        fude_vocab_words = rde_arr_new(sizeof(fude_vocab_word), rde_memory_allocator_get_default_std());
        fude_vocab_sents = rde_arr_new(sizeof(fude_vocab_sent), rde_memory_allocator_get_default_std());
        fude_vocab_ready = true;
    }
}

RDE_INTERNAL void fude_vocab_clear(void) {
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        rde_arr_free(&fude_vocab_lists[_l].words);
    }
    fude_vocab_list_n    = 0;
    fude_vocab_next      = 1;
    fude_vocab_next_list = 1;
    if(fude_vocab_ready) {
        rde_arr_clear(&fude_vocab_words);
        rde_arr_clear(&fude_vocab_sents);
    }
}

// _s (its first _len bytes) into _out (_size bytes), cut at a whole character.
RDE_INTERNAL void fude_vocab_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;   // not in the middle of a UTF-8 sequence
    }
    if(_n > 0) {
        memcpy(_out, _s, _n);
    }
    _out[_n] = 0;
}

RDE_INTERNAL void fude_vocab_set(c8* _out, usize _size, const c8* _s) {
    fude_vocab_copy(_out, _size, _s != NULL ? _s : "", _s != NULL ? strlen(_s) : 0u);
}

// --- the file ---------------------------------------------------------------------------

RDE_INTERNAL void fude_vocab_put_string(fude_bytes* _b, const c8* _s) {
    const u32 _n = (u32)strlen(_s);
    fude_put_u16(_b, (u16)_n);
    fude_put_data(_b, _s, _n);
}

RDE_INTERNAL b8 fude_vocab_get_string(fude_reader* _r, c8* _out, usize _size) {
    const u32 _n = fude_get_u16(_r);
    if(!_r->ok || _n > _r->size - _r->pos) {
        _r->ok = false;
        return false;
    }
    fude_vocab_copy(_out, _size, (const c8*)&_r->data[_r->pos], _n);
    _r->pos += _n;
    return true;
}

RDE_INTERNAL b8 fude_vocab_save(void) {
    fude_vocab_changes++;
    if(fude_vocab_path[0] == 0) {
        return true;
    }
    const u32  _n = (u32)rde_arr_length(&fude_vocab_words);
    fude_bytes _b = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 64u + _n * 160u);
    fude_put_header(&_b, FUDE_VOCAB_VERSION, FUDE_VOCAB_KIND);
    u32 _chunk = fude_chunk_begin(&_b, FUDE_VOCAB_CHUNK);
    fude_put_u32(&_b, _n);
    fude_put_u32(&_b, fude_vocab_next);
    const fude_vocab_word* _w = (const fude_vocab_word*)fude_vocab_words.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        fude_put_u32(&_b, _w[_i].id);
        fude_put_u32(&_b, _w[_i].kanji);
        fude_put_u32(&_b, (u32)(_w[_i].added & 0xFFFFFFFFu));
        fude_put_u32(&_b, (u32)(_w[_i].added >> 32));
        fude_vocab_put_string(&_b, _w[_i].written);
        fude_vocab_put_string(&_b, _w[_i].reading);
        fude_vocab_put_string(&_b, _w[_i].meaning);
    }
    fude_chunk_end(&_b, _chunk);
    _chunk = fude_chunk_begin(&_b, FUDE_VOCAB_LIST_CHUNK);
    fude_put_u32(&_b, fude_vocab_list_n);
    fude_put_u32(&_b, fude_vocab_next_list);
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        const fude_vocab_list* _list = &fude_vocab_lists[_l];
        fude_put_u32(&_b, _list->id);
        fude_put_u32(&_b, (u32)(_list->created & 0xFFFFFFFFu));
        fude_put_u32(&_b, (u32)(_list->created >> 32));
        fude_vocab_put_string(&_b, _list->name);
        const u32 _m = (u32)rde_arr_length(&_list->words);
        fude_put_u32(&_b, _m);
        for(u32 _k = 0; _k < _m; _k++) {
            fude_put_u32(&_b, ((const u32*)_list->words.memory)[_k]);
        }
    }
    fude_chunk_end(&_b, _chunk);
    const u32 _s = (u32)rde_arr_length(&fude_vocab_sents);
    if(_s > 0) {
        _chunk = fude_chunk_begin(&_b, FUDE_VOCAB_SENT_CHUNK);
        fude_put_u32(&_b, _s);
        for(u32 _i = 0; _i < _s; _i++) {
            const fude_vocab_sent* _sent = &((const fude_vocab_sent*)fude_vocab_sents.memory)[_i];
            fude_put_u32(&_b, _sent->id);
            fude_vocab_put_string(&_b, _sent->japanese);
            fude_vocab_put_string(&_b, _sent->translation);
        }
        fude_chunk_end(&_b, _chunk);
    }
    return fude_bytes_write_and_free(&_b, fude_vocab_path, NULL);
}

RDE_INTERNAL i32 fude_vocab_index(u32 _id) {
    if(!fude_vocab_ready || _id == 0u) {
        return -1;
    }
    const fude_vocab_word* _w = (const fude_vocab_word*)fude_vocab_words.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_vocab_words); _i++) {
        if(_w[_i].id == _id) {
            return (i32)_i;
        }
    }
    return -1;
}

// Word _id's sentence: its index (-1: none).
RDE_INTERNAL i32 fude_vocab_sent_index(u32 _id) {
    if(!fude_vocab_ready || _id == 0u) {
        return -1;
    }
    const fude_vocab_sent* _s = (const fude_vocab_sent*)fude_vocab_sents.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_vocab_sents); _i++) {
        if(_s[_i].id == _id) {
            return (i32)_i;
        }
    }
    return -1;
}

RDE_INTERNAL fude_vocab_list* fude_vocab_list_get(u32 _list) {
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        if(fude_vocab_lists[_l].id == _list) {
            return &fude_vocab_lists[_l];
        }
    }
    return NULL;
}

void fude_vocab_open(const c8* _path) {
    fude_vocab_ensure();
    fude_vocab_clear();
    snprintf(fude_vocab_path, sizeof(fude_vocab_path), "%s", _path != NULL ? _path : "");
    fude_vocab_changes++;
    if(_path == NULL) {
        return;
    }

    c8 _from[RDE_MAX_PATH];
    snprintf(_from, sizeof(_from), "%s", _path);
    if(!rde_file_exists(_from)) {
        snprintf(_from, sizeof(_from), "%s.bak", _path);
        if(!rde_file_exists(_from)) {
            return;   // none saved yet
        }
    }
    u32         _size = 0;
    u8*         _data = fude_file_read(_from, &_size);
    fude_reader _r    = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_VOCAB_VERSION, FUDE_VOCAB_KIND)) {
        fude_file_free(_data);
        fude_file_set_aside(_from);
        return;
    }
    u32         _tag;
    fude_reader _chunk;
    fude_reader _old     = { 0 };
    b8          _has_old = false;
    b8          _has_new = false;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == FUDE_VOCAB_CHUNK) {
            _has_new          = true;
            const u32 _count  = fude_get_u32(&_chunk);
            fude_vocab_next   = fude_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
                fude_vocab_word _w = { 0 };
                _w.id              = fude_get_u32(&_chunk);
                _w.kanji           = fude_get_u32(&_chunk);
                const u32 _lo      = fude_get_u32(&_chunk);
                const u32 _hi      = fude_get_u32(&_chunk);
                _w.added           = (u64)_lo | ((u64)_hi << 32);
                if(fude_vocab_get_string(&_chunk, _w.written, sizeof(_w.written)) && fude_vocab_get_string(&_chunk, _w.reading, sizeof(_w.reading)) &&
                   fude_vocab_get_string(&_chunk, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && _w.id != 0u && fude_vocab_index(_w.id) < 0) {
                    rde_arr_add(&fude_vocab_words, &_w);
                    fude_vocab_next = _w.id >= fude_vocab_next ? _w.id + 1u : fude_vocab_next;
                }
            }
        } else if(_tag == FUDE_VOCAB_LIST_CHUNK) {
            const u32 _count     = fude_get_u32(&_chunk);
            fude_vocab_next_list = fude_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _chunk.ok && fude_vocab_list_n < FUDE_VOCAB_LISTS; _i++) {
                fude_vocab_list _l = { 0 };
                _l.id              = fude_get_u32(&_chunk);
                const u32 _lo      = fude_get_u32(&_chunk);
                const u32 _hi      = fude_get_u32(&_chunk);
                _l.created         = (u64)_lo | ((u64)_hi << 32);
                if(!fude_vocab_get_string(&_chunk, _l.name, sizeof(_l.name))) {
                    break;
                }
                const u32 _m = fude_get_u32(&_chunk);
                if(!_chunk.ok || (u64)_m * 4u > (u64)(_chunk.size - _chunk.pos)) {
                    break;
                }
                _l.words = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
                for(u32 _k = 0; _k < _m; _k++) {
                    const u32 _id = fude_get_u32(&_chunk);
                    rde_arr_add(&_l.words, (any)&_id);
                }
                if(_l.id == 0u || fude_vocab_list_get(_l.id) != NULL) {
                    rde_arr_free(&_l.words);
                    continue;
                }
                fude_vocab_lists[fude_vocab_list_n++] = _l;
                fude_vocab_next_list = _l.id >= fude_vocab_next_list ? _l.id + 1u : fude_vocab_next_list;
            }
        } else if(_tag == FUDE_VOCAB_SENT_CHUNK) {
            const u32 _count = fude_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
                fude_vocab_sent _sent = { 0 };
                _sent.id              = fude_get_u32(&_chunk);
                if(fude_vocab_get_string(&_chunk, _sent.japanese, sizeof(_sent.japanese)) && fude_vocab_get_string(&_chunk, _sent.translation, sizeof(_sent.translation)) &&
                   _sent.id != 0u && (_sent.japanese[0] != 0 || _sent.translation[0] != 0) && fude_vocab_sent_index(_sent.id) < 0) {
                    rde_arr_add(&fude_vocab_sents, &_sent);
                }
            }
        } else if(_tag == FUDE_VOCAB_OLD_CHUNK) {
            _old     = _chunk;
            _has_old = true;
        }
    }
    // A file from before the vocabulary: the kanji's words, each once.
    if(_has_old && !_has_new) {
        const u32 _count = fude_get_u32(&_old);
        for(u32 _i = 0; _i < _count && _old.ok; _i++) {
            fude_vocab_word _w = { 0 };
            _w.kanji           = fude_get_u32(&_old);
            const u32 _lo      = fude_get_u32(&_old);
            const u32 _hi      = fude_get_u32(&_old);
            _w.added           = (u64)_lo | ((u64)_hi << 32);
            if(fude_vocab_get_string(&_old, _w.written, sizeof(_w.written)) && fude_vocab_get_string(&_old, _w.reading, sizeof(_w.reading)) &&
               fude_vocab_get_string(&_old, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && fude_vocab_find(_w.written, _w.reading) == 0u) {
                _w.id = fude_vocab_next++;
                rde_arr_add(&fude_vocab_words, &_w);
            }
        }
    }
    // Sentences of words still here only.
    for(u32 _k = (u32)rde_arr_length(&fude_vocab_sents); _k-- > 0;) {
        if(fude_vocab_index(((const fude_vocab_sent*)fude_vocab_sents.memory)[_k].id) < 0) {
            rde_arr_remove(&fude_vocab_sents, _k);
        }
    }
    // Lists of words still here only.
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        rde_arr* _ids = &fude_vocab_lists[_l].words;
        for(u32 _k = (u32)rde_arr_length(_ids); _k-- > 0;) {
            if(fude_vocab_index(((const u32*)_ids->memory)[_k]) < 0) {
                rde_arr_remove(_ids, _k);
            }
        }
    }
    fude_file_free(_data);
}

void fude_vocab_close(void) {
    fude_vocab_clear();
    if(fude_vocab_ready) {
        rde_arr_free(&fude_vocab_words);
        rde_arr_free(&fude_vocab_sents);
        fude_vocab_ready = false;
    }
    fude_vocab_path[0] = 0;
    fude_vocab_changes++;
}

// --- words ------------------------------------------------------------------------------

u32 fude_vocab_count(void) {
    return fude_vocab_ready ? (u32)rde_arr_length(&fude_vocab_words) : 0u;
}

const fude_vocab_word* fude_vocab_at(u32 _index) {
    return _index < fude_vocab_count() ? &((const fude_vocab_word*)fude_vocab_words.memory)[_index] : NULL;
}

const fude_vocab_word* fude_vocab_get(u32 _id) {
    const i32 _at = fude_vocab_index(_id);
    return _at >= 0 ? fude_vocab_at((u32)_at) : NULL;
}

u32 fude_vocab_find(const c8* _written, const c8* _reading) {
    if(_written == NULL) {
        return 0u;
    }
    for(u32 _i = 0; _i < fude_vocab_count(); _i++) {
        const fude_vocab_word* _w = fude_vocab_at(_i);
        if(strcmp(_w->written, _written) == 0 && strcmp(_w->reading, _reading != NULL ? _reading : "") == 0) {
            return _w->id;
        }
    }
    return 0u;
}

u32 fude_vocab_add(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    fude_vocab_ensure();
    if(_written == NULL || _written[0] == 0) {
        return 0u;
    }
    fude_vocab_word _w = { .kanji = _kanji, .added = (u64)time(NULL) };
    fude_vocab_set(_w.written, sizeof(_w.written), _written);
    fude_vocab_set(_w.reading, sizeof(_w.reading), _reading);
    fude_vocab_set(_w.meaning, sizeof(_w.meaning), _meaning);
    const u32 _there = fude_vocab_find(_w.written, _w.reading);
    if(_there != 0u) {
        return _there;
    }
    _w.id = fude_vocab_next++;
    rde_arr_add(&fude_vocab_words, &_w);
    if(!fude_vocab_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not save the vocabulary (%s)", fude_vocab_path);
    }
    return _w.id;
}

b8 fude_vocab_update(u32 _id, const c8* _written, const c8* _reading, const c8* _meaning) {
    const i32 _at = fude_vocab_index(_id);
    if(_at < 0 || _written == NULL || _written[0] == 0) {
        return false;
    }
    c8 _wr[FUDE_USERWORD_WRITTEN];
    c8 _rd[FUDE_USERWORD_READING];
    fude_vocab_set(_wr, sizeof(_wr), _written);
    fude_vocab_set(_rd, sizeof(_rd), _reading);
    const u32 _other = fude_vocab_find(_wr, _rd);
    if(_other != 0u && _other != _id) {
        return false;   // that word is saved already
    }
    fude_vocab_word* _w = &((fude_vocab_word*)fude_vocab_words.memory)[_at];
    memcpy(_w->written, _wr, sizeof(_wr));
    memcpy(_w->reading, _rd, sizeof(_rd));
    fude_vocab_set(_w->meaning, sizeof(_w->meaning), _meaning);
    fude_vocab_save();
    return true;
}

void fude_vocab_remove(u32 _id) {
    const i32 _at = fude_vocab_index(_id);
    if(_at < 0) {
        return;
    }
    rde_arr_remove(&fude_vocab_words, (usize)_at);
    const i32 _sent = fude_vocab_sent_index(_id);
    if(_sent >= 0) {
        rde_arr_remove(&fude_vocab_sents, (usize)_sent);
    }
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        rde_arr* _ids = &fude_vocab_lists[_l].words;
        for(u32 _k = (u32)rde_arr_length(_ids); _k-- > 0;) {
            if(((const u32*)_ids->memory)[_k] == _id) {
                rde_arr_remove(_ids, _k);
            }
        }
    }
    fude_vocab_save();
}

// --- a word's sentence ------------------------------------------------------------------

void fude_vocab_set_sentence(u32 _id, const c8* _japanese, const c8* _translation) {
    if(fude_vocab_index(_id) < 0) {
        return;
    }
    const b8  _none = (_japanese == NULL || _japanese[0] == 0) && (_translation == NULL || _translation[0] == 0);
    const i32 _at   = fude_vocab_sent_index(_id);
    if(_none) {
        if(_at >= 0) {
            rde_arr_remove(&fude_vocab_sents, (usize)_at);
            fude_vocab_save();
        }
        return;
    }
    fude_vocab_sent _sent = { .id = _id };
    fude_vocab_set(_sent.japanese, sizeof(_sent.japanese), _japanese);
    fude_vocab_set(_sent.translation, sizeof(_sent.translation), _translation);
    if(_at >= 0) {
        fude_vocab_sent* _there = &((fude_vocab_sent*)fude_vocab_sents.memory)[_at];
        if(strcmp(_there->japanese, _sent.japanese) == 0 && strcmp(_there->translation, _sent.translation) == 0) {
            return;   // as it was
        }
        *_there = _sent;
    } else {
        rde_arr_add(&fude_vocab_sents, &_sent);
    }
    fude_vocab_save();
}

const c8* fude_vocab_sentence(u32 _id, const c8** _translation) {
    const i32              _at   = fude_vocab_sent_index(_id);
    const fude_vocab_sent* _sent = _at >= 0 ? &((const fude_vocab_sent*)fude_vocab_sents.memory)[_at] : NULL;
    if(_translation != NULL) {
        *_translation = _sent != NULL ? _sent->translation : "";
    }
    return _sent != NULL ? _sent->japanese : "";
}

// Is _cp one of _text's characters?
RDE_INTERNAL b8 fude_vocab_has_char(const c8* _text, u32 _cp) {
    const c8* _p = _text;
    for(u32 _c = fude_utf8_next(&_p); _c != 0u; _c = fude_utf8_next(&_p)) {
        if(_c == _cp) {
            return true;
        }
    }
    return false;
}

u32 fude_vocab_of_kanji(u32 _kanji, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_vocab_count() && _kanji != 0u; _i++) {
        const fude_vocab_word* _w = fude_vocab_at(_i);
        if(_w->kanji == _kanji || fude_vocab_has_char(_w->written, _kanji)) {
            if(_out != NULL && _n < _max) {
                _out[_n] = _w->id;
            }
            _n++;
        }
    }
    return _out != NULL && _n > _max ? _max : _n;
}

u32 fude_vocab_kanji_count(u32 _kanji) {
    return fude_vocab_of_kanji(_kanji, NULL, 0u);
}

b8 fude_vocab_kanji_at(u32 _kanji, u32 _index, fude_kanji_word* _out) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_vocab_count() && _kanji != 0u; _i++) {
        const fude_vocab_word* _w = fude_vocab_at(_i);
        if((_w->kanji == _kanji || fude_vocab_has_char(_w->written, _kanji)) && _n++ == _index) {
            _out->written = _w->written;
            _out->reading = _w->reading;
            _out->meaning = _w->meaning;
            return true;
        }
    }
    return false;
}

// --- lists ------------------------------------------------------------------------------

u32 fude_vocab_list_count(void) {
    return fude_vocab_list_n;
}

u32 fude_vocab_list_at(u32 _index) {
    return _index < fude_vocab_list_n ? fude_vocab_lists[_index].id : 0u;
}

const c8* fude_vocab_list_name(u32 _list) {
    const fude_vocab_list* _l = fude_vocab_list_get(_list);
    return _l != NULL ? _l->name : "";
}

u32 fude_vocab_list_add(const c8* _name) {
    if(fude_vocab_list_n >= FUDE_VOCAB_LISTS || _name == NULL || _name[0] == 0) {
        return 0u;
    }
    fude_vocab_list* _l = &fude_vocab_lists[fude_vocab_list_n++];
    memset(_l, 0, sizeof(*_l));
    _l->id      = fude_vocab_next_list++;
    _l->created = (u64)time(NULL);
    _l->words   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_vocab_set(_l->name, sizeof(_l->name), _name);
    fude_vocab_save();
    return _l->id;
}

b8 fude_vocab_list_rename(u32 _list, const c8* _name) {
    fude_vocab_list* _l = fude_vocab_list_get(_list);
    if(_l == NULL || _name == NULL || _name[0] == 0) {
        return false;
    }
    fude_vocab_set(_l->name, sizeof(_l->name), _name);
    fude_vocab_save();
    return true;
}

void fude_vocab_list_remove(u32 _list) {
    for(u32 _l = 0; _l < fude_vocab_list_n; _l++) {
        if(fude_vocab_lists[_l].id == _list) {
            rde_arr_free(&fude_vocab_lists[_l].words);
            memmove(&fude_vocab_lists[_l], &fude_vocab_lists[_l + 1u], sizeof(fude_vocab_list) * (fude_vocab_list_n - _l - 1u));
            fude_vocab_list_n--;
            fude_vocab_save();
            return;
        }
    }
}

u32 fude_vocab_list_words(u32 _list, u32* _out, u32 _max) {
    const fude_vocab_list* _l = fude_vocab_list_get(_list);
    if(_l == NULL) {
        return 0u;
    }
    const u32 _n = (u32)rde_arr_length(&_l->words);
    if(_out == NULL) {
        return _n;
    }
    const u32 _k = _n < _max ? _n : _max;
    if(_k > 0) {
        memcpy(_out, _l->words.memory, sizeof(u32) * _k);
    }
    return _k;
}

b8 fude_vocab_in_list(u32 _list, u32 _word) {
    const fude_vocab_list* _l = fude_vocab_list_get(_list);
    for(u32 _k = 0; _l != NULL && _k < (u32)rde_arr_length(&_l->words); _k++) {
        if(((const u32*)_l->words.memory)[_k] == _word) {
            return true;
        }
    }
    return false;
}

void fude_vocab_set_in_list(u32 _list, u32 _word, b8 _in) {
    fude_vocab_list* _l = fude_vocab_list_get(_list);
    if(_l == NULL || fude_vocab_in_list(_list, _word) == _in || (_in && fude_vocab_index(_word) < 0)) {
        return;
    }
    if(_in) {
        rde_arr_add(&_l->words, (any)&_word);
    } else {
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_l->words); _k++) {
            if(((const u32*)_l->words.memory)[_k] == _word) {
                rde_arr_remove(&_l->words, _k);
                break;
            }
        }
    }
    fude_vocab_save();
}

void fude_vocab_list_new_name(c8* _out, usize _size) {
    for(u32 _n = fude_vocab_list_n + 1u;; _n++) {
        const fude_text_arg _arg = FUDE_TN(_n);
        fude_text_format(_out, _size, FUDE_TEXT_VOCAB_LIST_N, &_arg, 1u);
        b8 _taken = false;
        for(u32 _l = 0; _l < fude_vocab_list_n && !_taken; _l++) {
            _taken = strcmp(fude_vocab_lists[_l].name, _out) == 0;
        }
        if(!_taken) {
            return;
        }
    }
}

u32 fude_vocab_revision(void) {
    return fude_vocab_changes;
}

u32 fude_vocab_add_characters(const fude_kanji_db* _db, const u32* _records, u32 _count, c8* _name, usize _name_size, u32* _saved) {
    *_saved = 0;
    fude_vocab_list_new_name(_name, _name_size);
    const u32 _list = fude_vocab_list_add(_name);
    if(_list == 0u) {
        return 0u;
    }
    for(u32 _i = 0; _i < _count; _i++) {
        fude_kanji_info _ch;
        if(!fude_kanji_at(_db, _records[_i], &_ch)) {
            continue;
        }
        c8 _written[8];
        fude_utf8_put(_ch.codepoint, _written);
        c8        _reading[FUDE_USERWORD_READING] = "";
        const c8* _meaning = fude_kanji_meanings(_db, &_ch);
        const c8* _romaji  = fude_lang_latin(_ch.codepoint);
        if(_romaji != NULL && _ch.text == UINT32_MAX) {
            snprintf(_reading, sizeof(_reading), "%s", _written);   // a script's letter (a kana): itself
            _meaning = _romaji;
        } else {
            // The first reading of the kind a one-character word is read by (lang.h;
            // Japanese: kun, else on), up to its stem mark, without - marks.
            const c8* _try  = fude_kanji_reading(_db, &_ch, fude_lang_word_reading_kind(0u));
            const c8* _from = _try[0] != 0 ? _try : fude_kanji_reading(_db, &_ch, fude_lang_word_reading_kind(1u));
            usize     _k    = 0;
            for(const c8* _c = _from; *_c != 0 && *_c != '.' && _k + 1u < sizeof(_reading);) {
                if(_c[0] == '\xE3' && _c[1] == '\x80' && _c[2] == '\x81') {
                    break;   // 、 the next reading
                }
                if(*_c != '-') {
                    _reading[_k++] = *_c;
                }
                _c++;
            }
            _reading[_k] = 0;
        }
        const u32 _id = fude_vocab_add(_written, _reading, _meaning, _ch.codepoint);
        if(_id != 0u) {
            fude_vocab_set_in_list(_list, _id, true);
            (*_saved)++;
        }
    }
    return _list;
}
