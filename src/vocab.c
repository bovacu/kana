#include "vocab.h"
#include "kfile.h"
#include "text.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See vocab.h. A few thousand words at most: one array in the order saved,
// searched in full; the lists, each an array of word ids.
// ===========================================================================

#define KANA_VOCAB_VERSION    1u
#define KANA_VOCAB_KIND       KANA_TAG('U', 'W', 'R', 'D')
#define KANA_VOCAB_CHUNK      KANA_TAG('V', 'O', 'C', 'B')
#define KANA_VOCAB_LIST_CHUNK KANA_TAG('L', 'I', 'S', 'T')
#define KANA_VOCAB_OLD_CHUNK  KANA_TAG('W', 'R', 'D', 'S')

typedef struct {
    u32               id;
    u64               created;
    c8                name[KANA_VOCAB_LIST_NAME];
    rde_arr TYPE(u32) words;
} kana_vocab_list;

static rde_arr TYPE(kana_vocab_word) kana_vocab_words;
static kana_vocab_list               kana_vocab_lists[KANA_VOCAB_LISTS];
static u32                           kana_vocab_list_n   = 0;
static u32                           kana_vocab_next     = 1;   // the next word id
static u32                           kana_vocab_next_list = 1;
static b8                            kana_vocab_ready    = false;
static c8                            kana_vocab_path[RDE_MAX_PATH];
static u32                           kana_vocab_changes  = 0;

RDE_INTERNAL void kana_vocab_ensure(void) {
    if(!kana_vocab_ready) {
        kana_vocab_words = rde_arr_new(sizeof(kana_vocab_word), rde_memory_allocator_get_default_std());
        kana_vocab_ready = true;
    }
}

RDE_INTERNAL void kana_vocab_clear(void) {
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        rde_arr_free(&kana_vocab_lists[_l].words);
    }
    kana_vocab_list_n    = 0;
    kana_vocab_next      = 1;
    kana_vocab_next_list = 1;
    if(kana_vocab_ready) {
        rde_arr_clear(&kana_vocab_words);
    }
}

// _s (its first _len bytes) into _out (_size bytes), cut at a whole character.
RDE_INTERNAL void kana_vocab_copy(c8* _out, usize _size, const c8* _s, usize _len) {
    usize _n = _len < _size - 1u ? _len : _size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;   // not in the middle of a UTF-8 sequence
    }
    if(_n > 0) {
        memcpy(_out, _s, _n);
    }
    _out[_n] = 0;
}

RDE_INTERNAL void kana_vocab_set(c8* _out, usize _size, const c8* _s) {
    kana_vocab_copy(_out, _size, _s != NULL ? _s : "", _s != NULL ? strlen(_s) : 0u);
}

// --- the file ---------------------------------------------------------------------------

RDE_INTERNAL void kana_vocab_put_string(kana_bytes* _b, const c8* _s) {
    const u32 _n = (u32)strlen(_s);
    kana_put_u16(_b, (u16)_n);
    kana_put_data(_b, _s, _n);
}

RDE_INTERNAL b8 kana_vocab_get_string(kana_reader* _r, c8* _out, usize _size) {
    const u32 _n = kana_get_u16(_r);
    if(!_r->ok || _n > _r->size - _r->pos) {
        _r->ok = false;
        return false;
    }
    kana_vocab_copy(_out, _size, (const c8*)&_r->data[_r->pos], _n);
    _r->pos += _n;
    return true;
}

RDE_INTERNAL b8 kana_vocab_save(void) {
    kana_vocab_changes++;
    if(kana_vocab_path[0] == 0) {
        return true;
    }
    const u32  _n = (u32)rde_arr_length(&kana_vocab_words);
    kana_bytes _b = kana_bytes_new(KANA_FILE_HEADER_SIZE + 64u + _n * 160u);
    kana_put_header(&_b, KANA_VOCAB_VERSION, KANA_VOCAB_KIND);
    u32 _chunk = kana_chunk_begin(&_b, KANA_VOCAB_CHUNK);
    kana_put_u32(&_b, _n);
    kana_put_u32(&_b, kana_vocab_next);
    const kana_vocab_word* _w = (const kana_vocab_word*)kana_vocab_words.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_put_u32(&_b, _w[_i].id);
        kana_put_u32(&_b, _w[_i].kanji);
        kana_put_u32(&_b, (u32)(_w[_i].added & 0xFFFFFFFFu));
        kana_put_u32(&_b, (u32)(_w[_i].added >> 32));
        kana_vocab_put_string(&_b, _w[_i].written);
        kana_vocab_put_string(&_b, _w[_i].reading);
        kana_vocab_put_string(&_b, _w[_i].meaning);
    }
    kana_chunk_end(&_b, _chunk);
    _chunk = kana_chunk_begin(&_b, KANA_VOCAB_LIST_CHUNK);
    kana_put_u32(&_b, kana_vocab_list_n);
    kana_put_u32(&_b, kana_vocab_next_list);
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        const kana_vocab_list* _list = &kana_vocab_lists[_l];
        kana_put_u32(&_b, _list->id);
        kana_put_u32(&_b, (u32)(_list->created & 0xFFFFFFFFu));
        kana_put_u32(&_b, (u32)(_list->created >> 32));
        kana_vocab_put_string(&_b, _list->name);
        const u32 _m = (u32)rde_arr_length(&_list->words);
        kana_put_u32(&_b, _m);
        for(u32 _k = 0; _k < _m; _k++) {
            kana_put_u32(&_b, ((const u32*)_list->words.memory)[_k]);
        }
    }
    kana_chunk_end(&_b, _chunk);
    return kana_bytes_write_and_free(&_b, kana_vocab_path, NULL);
}

RDE_INTERNAL i32 kana_vocab_index(u32 _id) {
    if(!kana_vocab_ready || _id == 0u) {
        return -1;
    }
    const kana_vocab_word* _w = (const kana_vocab_word*)kana_vocab_words.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&kana_vocab_words); _i++) {
        if(_w[_i].id == _id) {
            return (i32)_i;
        }
    }
    return -1;
}

RDE_INTERNAL kana_vocab_list* kana_vocab_list_get(u32 _list) {
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        if(kana_vocab_lists[_l].id == _list) {
            return &kana_vocab_lists[_l];
        }
    }
    return NULL;
}

void kana_vocab_open(const c8* _path) {
    kana_vocab_ensure();
    kana_vocab_clear();
    snprintf(kana_vocab_path, sizeof(kana_vocab_path), "%s", _path != NULL ? _path : "");
    kana_vocab_changes++;
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
    u8*         _data = kana_file_read(_from, &_size);
    kana_reader _r    = kana_reader_make(_data, _size);
    if(_data == NULL || !kana_read_header(&_r, KANA_VOCAB_VERSION, KANA_VOCAB_KIND)) {
        kana_file_free(_data);
        kana_file_set_aside(_from);
        return;
    }
    u32         _tag;
    kana_reader _chunk;
    kana_reader _old     = { 0 };
    b8          _has_old = false;
    b8          _has_new = false;
    while(kana_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == KANA_VOCAB_CHUNK) {
            _has_new          = true;
            const u32 _count  = kana_get_u32(&_chunk);
            kana_vocab_next   = kana_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
                kana_vocab_word _w = { 0 };
                _w.id              = kana_get_u32(&_chunk);
                _w.kanji           = kana_get_u32(&_chunk);
                const u32 _lo      = kana_get_u32(&_chunk);
                const u32 _hi      = kana_get_u32(&_chunk);
                _w.added           = (u64)_lo | ((u64)_hi << 32);
                if(kana_vocab_get_string(&_chunk, _w.written, sizeof(_w.written)) && kana_vocab_get_string(&_chunk, _w.reading, sizeof(_w.reading)) &&
                   kana_vocab_get_string(&_chunk, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && _w.id != 0u && kana_vocab_index(_w.id) < 0) {
                    rde_arr_add(&kana_vocab_words, &_w);
                    kana_vocab_next = _w.id >= kana_vocab_next ? _w.id + 1u : kana_vocab_next;
                }
            }
        } else if(_tag == KANA_VOCAB_LIST_CHUNK) {
            const u32 _count     = kana_get_u32(&_chunk);
            kana_vocab_next_list = kana_get_u32(&_chunk);
            for(u32 _i = 0; _i < _count && _chunk.ok && kana_vocab_list_n < KANA_VOCAB_LISTS; _i++) {
                kana_vocab_list _l = { 0 };
                _l.id              = kana_get_u32(&_chunk);
                const u32 _lo      = kana_get_u32(&_chunk);
                const u32 _hi      = kana_get_u32(&_chunk);
                _l.created         = (u64)_lo | ((u64)_hi << 32);
                if(!kana_vocab_get_string(&_chunk, _l.name, sizeof(_l.name))) {
                    break;
                }
                const u32 _m = kana_get_u32(&_chunk);
                if(!_chunk.ok || (u64)_m * 4u > (u64)(_chunk.size - _chunk.pos)) {
                    break;
                }
                _l.words = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
                for(u32 _k = 0; _k < _m; _k++) {
                    const u32 _id = kana_get_u32(&_chunk);
                    rde_arr_add(&_l.words, (any)&_id);
                }
                if(_l.id == 0u || kana_vocab_list_get(_l.id) != NULL) {
                    rde_arr_free(&_l.words);
                    continue;
                }
                kana_vocab_lists[kana_vocab_list_n++] = _l;
                kana_vocab_next_list = _l.id >= kana_vocab_next_list ? _l.id + 1u : kana_vocab_next_list;
            }
        } else if(_tag == KANA_VOCAB_OLD_CHUNK) {
            _old     = _chunk;
            _has_old = true;
        }
    }
    // A file from before the vocabulary: the kanji's words, each once.
    if(_has_old && !_has_new) {
        const u32 _count = kana_get_u32(&_old);
        for(u32 _i = 0; _i < _count && _old.ok; _i++) {
            kana_vocab_word _w = { 0 };
            _w.kanji           = kana_get_u32(&_old);
            const u32 _lo      = kana_get_u32(&_old);
            const u32 _hi      = kana_get_u32(&_old);
            _w.added           = (u64)_lo | ((u64)_hi << 32);
            if(kana_vocab_get_string(&_old, _w.written, sizeof(_w.written)) && kana_vocab_get_string(&_old, _w.reading, sizeof(_w.reading)) &&
               kana_vocab_get_string(&_old, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && kana_vocab_find(_w.written, _w.reading) == 0u) {
                _w.id = kana_vocab_next++;
                rde_arr_add(&kana_vocab_words, &_w);
            }
        }
    }
    // Lists of words still here only.
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        rde_arr* _ids = &kana_vocab_lists[_l].words;
        for(u32 _k = (u32)rde_arr_length(_ids); _k-- > 0;) {
            if(kana_vocab_index(((const u32*)_ids->memory)[_k]) < 0) {
                rde_arr_remove(_ids, _k);
            }
        }
    }
    kana_file_free(_data);
}

void kana_vocab_close(void) {
    kana_vocab_clear();
    if(kana_vocab_ready) {
        rde_arr_free(&kana_vocab_words);
        kana_vocab_ready = false;
    }
    kana_vocab_path[0] = 0;
    kana_vocab_changes++;
}

// --- words ------------------------------------------------------------------------------

u32 kana_vocab_count(void) {
    return kana_vocab_ready ? (u32)rde_arr_length(&kana_vocab_words) : 0u;
}

const kana_vocab_word* kana_vocab_at(u32 _index) {
    return _index < kana_vocab_count() ? &((const kana_vocab_word*)kana_vocab_words.memory)[_index] : NULL;
}

const kana_vocab_word* kana_vocab_get(u32 _id) {
    const i32 _at = kana_vocab_index(_id);
    return _at >= 0 ? kana_vocab_at((u32)_at) : NULL;
}

u32 kana_vocab_find(const c8* _written, const c8* _reading) {
    if(_written == NULL) {
        return 0u;
    }
    for(u32 _i = 0; _i < kana_vocab_count(); _i++) {
        const kana_vocab_word* _w = kana_vocab_at(_i);
        if(strcmp(_w->written, _written) == 0 && strcmp(_w->reading, _reading != NULL ? _reading : "") == 0) {
            return _w->id;
        }
    }
    return 0u;
}

u32 kana_vocab_add(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    kana_vocab_ensure();
    if(_written == NULL || _written[0] == 0) {
        return 0u;
    }
    kana_vocab_word _w = { .kanji = _kanji, .added = (u64)time(NULL) };
    kana_vocab_set(_w.written, sizeof(_w.written), _written);
    kana_vocab_set(_w.reading, sizeof(_w.reading), _reading);
    kana_vocab_set(_w.meaning, sizeof(_w.meaning), _meaning);
    const u32 _there = kana_vocab_find(_w.written, _w.reading);
    if(_there != 0u) {
        return _there;
    }
    _w.id = kana_vocab_next++;
    rde_arr_add(&kana_vocab_words, &_w);
    if(!kana_vocab_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "kana: could not save the vocabulary (%s)", kana_vocab_path);
    }
    return _w.id;
}

b8 kana_vocab_update(u32 _id, const c8* _written, const c8* _reading, const c8* _meaning) {
    const i32 _at = kana_vocab_index(_id);
    if(_at < 0 || _written == NULL || _written[0] == 0) {
        return false;
    }
    c8 _wr[KANA_USERWORD_WRITTEN];
    c8 _rd[KANA_USERWORD_READING];
    kana_vocab_set(_wr, sizeof(_wr), _written);
    kana_vocab_set(_rd, sizeof(_rd), _reading);
    const u32 _other = kana_vocab_find(_wr, _rd);
    if(_other != 0u && _other != _id) {
        return false;   // that word is saved already
    }
    kana_vocab_word* _w = &((kana_vocab_word*)kana_vocab_words.memory)[_at];
    memcpy(_w->written, _wr, sizeof(_wr));
    memcpy(_w->reading, _rd, sizeof(_rd));
    kana_vocab_set(_w->meaning, sizeof(_w->meaning), _meaning);
    kana_vocab_save();
    return true;
}

void kana_vocab_remove(u32 _id) {
    const i32 _at = kana_vocab_index(_id);
    if(_at < 0) {
        return;
    }
    rde_arr_remove(&kana_vocab_words, (usize)_at);
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        rde_arr* _ids = &kana_vocab_lists[_l].words;
        for(u32 _k = (u32)rde_arr_length(_ids); _k-- > 0;) {
            if(((const u32*)_ids->memory)[_k] == _id) {
                rde_arr_remove(_ids, _k);
            }
        }
    }
    kana_vocab_save();
}

// Is _cp one of _text's characters?
RDE_INTERNAL b8 kana_vocab_has_char(const c8* _text, u32 _cp) {
    const c8* _p = _text;
    for(u32 _c = kana_kanji_utf8_next(&_p); _c != 0u; _c = kana_kanji_utf8_next(&_p)) {
        if(_c == _cp) {
            return true;
        }
    }
    return false;
}

u32 kana_vocab_of_kanji(u32 _kanji, u32* _out, u32 _max) {
    u32 _n = 0;
    for(u32 _i = 0; _i < kana_vocab_count() && _kanji != 0u; _i++) {
        const kana_vocab_word* _w = kana_vocab_at(_i);
        if(_w->kanji == _kanji || kana_vocab_has_char(_w->written, _kanji)) {
            if(_out != NULL && _n < _max) {
                _out[_n] = _w->id;
            }
            _n++;
        }
    }
    return _out != NULL && _n > _max ? _max : _n;
}

u32 kana_vocab_kanji_count(u32 _kanji) {
    return kana_vocab_of_kanji(_kanji, NULL, 0u);
}

b8 kana_vocab_kanji_at(u32 _kanji, u32 _index, kana_kanji_word* _out) {
    u32 _n = 0;
    for(u32 _i = 0; _i < kana_vocab_count() && _kanji != 0u; _i++) {
        const kana_vocab_word* _w = kana_vocab_at(_i);
        if((_w->kanji == _kanji || kana_vocab_has_char(_w->written, _kanji)) && _n++ == _index) {
            _out->written = _w->written;
            _out->reading = _w->reading;
            _out->meaning = _w->meaning;
            return true;
        }
    }
    return false;
}

// --- lists ------------------------------------------------------------------------------

u32 kana_vocab_list_count(void) {
    return kana_vocab_list_n;
}

u32 kana_vocab_list_at(u32 _index) {
    return _index < kana_vocab_list_n ? kana_vocab_lists[_index].id : 0u;
}

const c8* kana_vocab_list_name(u32 _list) {
    const kana_vocab_list* _l = kana_vocab_list_get(_list);
    return _l != NULL ? _l->name : "";
}

u32 kana_vocab_list_add(const c8* _name) {
    if(kana_vocab_list_n >= KANA_VOCAB_LISTS || _name == NULL || _name[0] == 0) {
        return 0u;
    }
    kana_vocab_list* _l = &kana_vocab_lists[kana_vocab_list_n++];
    memset(_l, 0, sizeof(*_l));
    _l->id      = kana_vocab_next_list++;
    _l->created = (u64)time(NULL);
    _l->words   = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    kana_vocab_set(_l->name, sizeof(_l->name), _name);
    kana_vocab_save();
    return _l->id;
}

b8 kana_vocab_list_rename(u32 _list, const c8* _name) {
    kana_vocab_list* _l = kana_vocab_list_get(_list);
    if(_l == NULL || _name == NULL || _name[0] == 0) {
        return false;
    }
    kana_vocab_set(_l->name, sizeof(_l->name), _name);
    kana_vocab_save();
    return true;
}

void kana_vocab_list_remove(u32 _list) {
    for(u32 _l = 0; _l < kana_vocab_list_n; _l++) {
        if(kana_vocab_lists[_l].id == _list) {
            rde_arr_free(&kana_vocab_lists[_l].words);
            memmove(&kana_vocab_lists[_l], &kana_vocab_lists[_l + 1u], sizeof(kana_vocab_list) * (kana_vocab_list_n - _l - 1u));
            kana_vocab_list_n--;
            kana_vocab_save();
            return;
        }
    }
}

u32 kana_vocab_list_words(u32 _list, u32* _out, u32 _max) {
    const kana_vocab_list* _l = kana_vocab_list_get(_list);
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

b8 kana_vocab_in_list(u32 _list, u32 _word) {
    const kana_vocab_list* _l = kana_vocab_list_get(_list);
    for(u32 _k = 0; _l != NULL && _k < (u32)rde_arr_length(&_l->words); _k++) {
        if(((const u32*)_l->words.memory)[_k] == _word) {
            return true;
        }
    }
    return false;
}

void kana_vocab_set_in_list(u32 _list, u32 _word, b8 _in) {
    kana_vocab_list* _l = kana_vocab_list_get(_list);
    if(_l == NULL || kana_vocab_in_list(_list, _word) == _in || (_in && kana_vocab_index(_word) < 0)) {
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
    kana_vocab_save();
}

void kana_vocab_list_new_name(c8* _out, usize _size) {
    for(u32 _n = kana_vocab_list_n + 1u;; _n++) {
        const kana_text_arg _arg = KANA_TN(_n);
        kana_text_format(_out, _size, KANA_TEXT_VOCAB_LIST_N, &_arg, 1u);
        b8 _taken = false;
        for(u32 _l = 0; _l < kana_vocab_list_n && !_taken; _l++) {
            _taken = strcmp(kana_vocab_lists[_l].name, _out) == 0;
        }
        if(!_taken) {
            return;
        }
    }
}

u32 kana_vocab_revision(void) {
    return kana_vocab_changes;
}
