// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/vocab.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "lang/lang.h"
#include "drawing/base/utf8.h"


// ===========================================================================
// See vocab.h.
//
// There are a few thousand words at most, so they are one array in the order they were saved, and a word is found by
// its id by searching it in full. By its written form and reading (fude_vocab_find, which screens ask for each word
// they show, every frame) it is found through a map of the two (fude_vocab_by_text), kept in step with the array. The
// lists are an array too, in the order they were made, as many as the learner makes, each holding an array of word
// ids. Few words have a sentence, so the sentences are kept apart, in an array of their own keyed by word id. The whole
// file is written again on every change.
// ===========================================================================

// The file's format version, and the tags of its kind and of its chunks (the layout is in vocab.h). 'WRDS' is only
// read, from files made before the vocabulary existed.
#define FUDE_VOCAB_VERSION    1u
#define FUDE_VOCAB_KIND       FUDE_TAG('U', 'W', 'R', 'D')
#define FUDE_VOCAB_CHUNK      FUDE_TAG('V', 'O', 'C', 'B')
#define FUDE_VOCAB_LIST_CHUNK FUDE_TAG('L', 'I', 'S', 'T')
#define FUDE_VOCAB_SENT_CHUNK FUDE_TAG('S', 'E', 'N', 'T')
#define FUDE_VOCAB_OLD_CHUNK  FUDE_TAG('W', 'R', 'D', 'S')

// One of the learner's lists: its id (never 0, never reused), when it was made (Unix seconds), its name, and its
// words' ids in the order they went in.
typedef struct {
    u32               id;
    u64               created;
    c8                name[FUDE_VOCAB_LIST_NAME];
    rde_arr TYPE(u32) words;
} fude_vocab_list;

// A word's sentence: the sentence it was met in and its translation. Few words have one, so they are kept apart from
// the words.
typedef struct {
    u32 id;
    c8  taught[FUDE_VOCAB_SENTENCE];
    c8  translation[FUDE_VOCAB_SENTENCE];
} fude_vocab_sent;

// The words in the order saved, the sentences, and the lists in the order made; the three arrays are created on first
// use (fude_vocab_ensure), so the vocabulary works in memory without a file, as the tests use it.
static rde_arr TYPE(fude_vocab_word) fude_vocab_words;
static rde_arr TYPE(fude_vocab_sent) fude_vocab_sents;
static rde_arr TYPE(fude_vocab_list) fude_vocab_lists;
static u32                           fude_vocab_next     = 1;   // The next word's id.
static u32                           fude_vocab_next_list = 1;
static b8                            fude_vocab_ready    = false;
// The file the vocabulary is saved to, or "" to keep it in memory only.
static c8                            fude_vocab_path[RDE_MAX_PATH];
// Goes up on every change; fude_vocab_revision returns it.
static u32                           fude_vocab_changes  = 0;

// A word's written form and reading as fude_vocab_by_text's key: each zero-filled to its size, so two keys are equal
// only when both texts are, whole (a separator between them could be in a text).
typedef struct {
    c8 written[FUDE_USERWORD_WRITTEN];
    c8 reading[FUDE_USERWORD_READING];
} fude_vocab_key;

// The words by their written form and reading (fude_vocab_key to the word's id), for fude_vocab_find. It is made again
// from the words, in order, when it is next needed after they changed in a way it cannot follow (a word removed or
// edited, all of them read or cleared: fude_vocab_words_changed moves on), and a word added is added to it. Made on the
// std allocator on first use, freed by fude_vocab_close. Where two words have the same texts (only a damaged file
// could hold them), the first in order is the one found, as searching the array in order found it.
static rde_hash_map TYPE(fude_vocab_key, u32) fude_vocab_by_text;
static u32                           fude_vocab_words_changed = 1u;   // Moves on with every change to the words.
static u32                           fude_vocab_by_text_at    = 0u;   // fude_vocab_words_changed when the map was last right.

// fude_vocab_by_text's hash and comparison: the whole key.
RDE_INTERNAL u64 fude_vocab_key_hash(const any _key) {
    return rde_hash_64(_key, sizeof(fude_vocab_key));
}
RDE_INTERNAL i32 fude_vocab_key_compare(const any _a, const any _b) {
    return rde_memcmp(_a, _b, sizeof(fude_vocab_key));
}

// Makes _key from a written form and a reading (NULL is ""). Returns false when either is too long to be a saved
// word's (the words keep them cut to their sizes), so no word has them.
RDE_INTERNAL b8 fude_vocab_key_make(fude_vocab_key* _key, const c8* _written, const c8* _reading) {
    const c8*   _rd = _reading != NULL ? _reading : "";
    const usize _wn = rde_strlen(_written), _rn = rde_strlen(_rd);
    if(_wn >= sizeof(_key->written) || _rn >= sizeof(_key->reading)) {
        return false;
    }
    rde_memset(_key, 0, sizeof(*_key));
    rde_memcpy(_key->written, _written, _wn);
    rde_memcpy(_key->reading, _rd, _rn);
    return true;
}

// The words changed in a way the map cannot follow: it is made again when next needed.
RDE_INTERNAL void fude_vocab_words_touched(void) {
    fude_vocab_words_changed++;
}

// Word _w was just added at the end of the words: added to the map too when the map was right, so it stays right
// (rde_hash_map_add keeps an earlier word with the same texts); else the map is made again when next needed.
RDE_INTERNAL void fude_vocab_words_added(const fude_vocab_word* _w) {
    const b8 _right = fude_vocab_by_text.memory != NULL && fude_vocab_by_text_at == fude_vocab_words_changed;
    fude_vocab_words_changed++;
    fude_vocab_key _key;
    if(_right && fude_vocab_key_make(&_key, _w->written, _w->reading)) {
        rde_hash_map_add(&fude_vocab_by_text, (const any)&_key, (const any)&_w->id);
        fude_vocab_by_text_at = fude_vocab_words_changed;
    }
}

// Creates the words', the sentences' and the lists' arrays the first time they are needed.
RDE_INTERNAL void fude_vocab_ensure(void) {
    if(!fude_vocab_ready) {
        fude_vocab_words = rde_arr_new(sizeof(fude_vocab_word), rde_memory_allocator_get_default_std());
        fude_vocab_sents = rde_arr_new(sizeof(fude_vocab_sent), rde_memory_allocator_get_default_std());
        fude_vocab_lists = rde_arr_new(sizeof(fude_vocab_list), rde_memory_allocator_get_default_std());
        fude_vocab_ready = true;
    }
}

// Returns the _index-th list in the order made; _index must be under fude_vocab_list_count(). The pointer is valid
// until a list is next added or removed.
RDE_INTERNAL fude_vocab_list* fude_vocab_list_slot(u32 _index) {
    return &((fude_vocab_list*)fude_vocab_lists.memory)[_index];
}

// Empties the vocabulary: frees every list's words, empties the words, the sentences and the lists, and starts the ids
// at 1 again.
RDE_INTERNAL void fude_vocab_clear(void) {
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        rde_arr_free(&fude_vocab_list_slot(_l)->words);
    }
    fude_vocab_next      = 1;
    fude_vocab_next_list = 1;
    fude_vocab_words_touched();
    if(fude_vocab_ready) {
        rde_arr_clear(&fude_vocab_words);
        rde_arr_clear(&fude_vocab_sents);
        rde_arr_clear(&fude_vocab_lists);
    }
}

// --- the file ---------------------------------------------------------------------------

// Writes a string as the file keeps it: a u16 length in bytes, then its bytes, without the NUL.
RDE_INTERNAL void fude_vocab_put_string(fude_bytes* _b, const c8* _s) {
    const u32 _n = (u32)rde_strlen(_s);
    fude_put_u16(_b, (u16)_n);
    fude_put_data(_b, _s, _n);
}

// Reads a string written by fude_vocab_put_string into _out (_size bytes), cut at a whole character when it does not
// fit. Returns false, and turns the reader's ok false, when the string runs past the end.
RDE_INTERNAL b8 fude_vocab_get_string(fude_reader* _r, c8* _out, usize _size) {
    const u32 _n = fude_get_u16(_r);
    if(!_r->ok || _n > _r->size - _r->pos) {
        _r->ok = false;
        return false;
    }
    fude_utf8_copy(_out, _size, (const c8*)&_r->data[_r->pos], _n);
    _r->pos += _n;
    return true;
}

// Counts a change and, when the vocabulary has a file, writes the whole file again: the words, the lists, then the
// sentences (that chunk only when there are any). Returns false when the file could not be written, and true when it
// was or when the vocabulary is kept in memory only.
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
        fude_put_u64(&_b, _w[_i].added);
        fude_vocab_put_string(&_b, _w[_i].written);
        fude_vocab_put_string(&_b, _w[_i].reading);
        fude_vocab_put_string(&_b, _w[_i].meaning);
    }
    fude_chunk_end(&_b, _chunk);
    _chunk = fude_chunk_begin(&_b, FUDE_VOCAB_LIST_CHUNK);
    fude_put_u32(&_b, fude_vocab_list_count());
    fude_put_u32(&_b, fude_vocab_next_list);
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        const fude_vocab_list* _list = fude_vocab_list_slot(_l);
        fude_put_u32(&_b, _list->id);
        fude_put_u64(&_b, _list->created);
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
            fude_vocab_put_string(&_b, _sent->taught);
            fude_vocab_put_string(&_b, _sent->translation);
        }
        fude_chunk_end(&_b, _chunk);
    }
    return fude_bytes_write_and_free(&_b, fude_vocab_path, NULL);
}

// Returns the index of the word with id _id, or -1 when there is none (or _id is 0).
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

// Returns the index of word _id's sentence, or -1 when it has none.
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

// Returns the list with id _list, or NULL when there is none. The pointer is valid until a list is next added or
// removed.
RDE_INTERNAL fude_vocab_list* fude_vocab_list_get(u32 _list) {
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        if(fude_vocab_list_slot(_l)->id == _list) {
            return fude_vocab_list_slot(_l);
        }
    }
    return NULL;
}

// Reads a 'VOCB' chunk's words. A word that is empty, has id 0 or repeats an id is dropped. _ids holds the ids read
// so far, so that a repeated one is found at once and reading a file stays linear in its words.
RDE_INTERNAL void fude_vocab_read_words(fude_reader* _chunk, rde_hash_set* _ids) {
    const u32 _count = fude_get_u32(_chunk);
    fude_vocab_next  = fude_get_u32(_chunk);
    fude_vocab_next  = fude_vocab_next > 0u ? fude_vocab_next : 1u;   // 0 is never an id (a damaged file)
    for(u32 _i = 0; _i < _count && _chunk->ok; _i++) {
        fude_vocab_word _w = { 0 };
        _w.id              = fude_get_u32(_chunk);
        _w.kanji           = fude_get_u32(_chunk);
        _w.added           = fude_get_u64(_chunk);
        if(fude_vocab_get_string(_chunk, _w.written, sizeof(_w.written)) && fude_vocab_get_string(_chunk, _w.reading, sizeof(_w.reading)) &&
           fude_vocab_get_string(_chunk, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && _w.id != 0u && !rde_hash_set_contains(_ids, (any)&_w.id)) {
            rde_hash_set_add(_ids, (any)&_w.id);
            rde_arr_add(&fude_vocab_words, &_w);
            fude_vocab_words_added(&_w);
            fude_vocab_next = _w.id >= fude_vocab_next ? _w.id + 1u : fude_vocab_next;
        }
    }
}

// Reads a 'LIST' chunk's lists, however many there are. A list that has id 0 or repeats an id is dropped.
RDE_INTERNAL void fude_vocab_read_lists(fude_reader* _chunk) {
    const u32 _count     = fude_get_u32(_chunk);
    fude_vocab_next_list = fude_get_u32(_chunk);
    fude_vocab_next_list = fude_vocab_next_list > 0u ? fude_vocab_next_list : 1u;   // 0 means All, never a list
    for(u32 _i = 0; _i < _count && _chunk->ok; _i++) {
        fude_vocab_list _l = { 0 };
        _l.id              = fude_get_u32(_chunk);
        _l.created         = fude_get_u64(_chunk);
        if(!fude_vocab_get_string(_chunk, _l.name, sizeof(_l.name))) {
            break;
        }
        const u32 _m = fude_get_u32(_chunk);
        if(!_chunk->ok || (u64)_m * 4u > (u64)(_chunk->size - _chunk->pos)) {
            break;
        }
        _l.words = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        for(u32 _k = 0; _k < _m; _k++) {
            const u32 _id = fude_get_u32(_chunk);
            rde_arr_add(&_l.words, (any)&_id);
        }
        if(_l.id == 0u || fude_vocab_list_get(_l.id) != NULL) {
            rde_arr_free(&_l.words);
            continue;
        }
        rde_arr_add(&fude_vocab_lists, &_l);
        fude_vocab_next_list = _l.id >= fude_vocab_next_list ? _l.id + 1u : fude_vocab_next_list;
    }
}

// Reads a 'SENT' chunk's sentences. One with word id 0, with no text, or for a word that has one already is dropped;
// _seen holds the word ids that have one so far.
RDE_INTERNAL void fude_vocab_read_sentences(fude_reader* _chunk, rde_hash_set* _seen) {
    const u32 _count = fude_get_u32(_chunk);
    for(u32 _i = 0; _i < _count && _chunk->ok; _i++) {
        fude_vocab_sent _sent = { 0 };
        _sent.id              = fude_get_u32(_chunk);
        if(fude_vocab_get_string(_chunk, _sent.taught, sizeof(_sent.taught)) && fude_vocab_get_string(_chunk, _sent.translation, sizeof(_sent.translation)) &&
           _sent.id != 0u && (_sent.taught[0] != 0 || _sent.translation[0] != 0) && !rde_hash_set_contains(_seen, (any)&_sent.id)) {
            rde_hash_set_add(_seen, (any)&_sent.id);
            rde_arr_add(&fude_vocab_sents, &_sent);
        }
    }
}

// Reads the old 'WRDS' chunk of a file from before the vocabulary existed: its characters' words become the
// vocabulary, each word once, with new ids (added to _ids).
RDE_INTERNAL void fude_vocab_read_old(fude_reader* _old, rde_hash_set* _ids) {
    const u32 _count = fude_get_u32(_old);
    for(u32 _i = 0; _i < _count && _old->ok; _i++) {
        fude_vocab_word _w = { 0 };
        _w.kanji           = fude_get_u32(_old);
        _w.added           = fude_get_u64(_old);
        if(fude_vocab_get_string(_old, _w.written, sizeof(_w.written)) && fude_vocab_get_string(_old, _w.reading, sizeof(_w.reading)) &&
           fude_vocab_get_string(_old, _w.meaning, sizeof(_w.meaning)) && _w.written[0] != 0 && fude_vocab_find(_w.written, _w.reading) == 0u) {
            _w.id = fude_vocab_next++;
            rde_hash_set_add(_ids, (any)&_w.id);
            rde_arr_add(&fude_vocab_words, &_w);
            fude_vocab_words_added(&_w);
        }
    }
}

// Drops the sentences and the lists' entries whose word is not in the vocabulary (_ids: the words' ids).
RDE_INTERNAL void fude_vocab_read_prune(const rde_hash_set* _ids) {
    for(u32 _k = (u32)rde_arr_length(&fude_vocab_sents); _k-- > 0;) {
        if(!rde_hash_set_contains(_ids, (any)&((const fude_vocab_sent*)fude_vocab_sents.memory)[_k].id)) {
            rde_arr_remove(&fude_vocab_sents, _k);
        }
    }
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        rde_arr* _list = &fude_vocab_list_slot(_l)->words;
        for(u32 _k = (u32)rde_arr_length(_list); _k-- > 0;) {
            if(!rde_hash_set_contains(_ids, (any)&((const u32*)_list->memory)[_k])) {
                rde_arr_remove(_list, _k);
            }
        }
    }
}

// Reads the vocabulary file's bytes into memory, starting from an empty vocabulary; fude_save_load (save.h) calls it, a
// second time for the .bak when the main file fails. The chunks are read by the steps above. The old 'WRDS' chunk is
// read only when there is no 'VOCB' chunk, which is known only once every chunk has been seen, so it is kept aside
// until then. Last, the sentences and the lists' entries whose word is not in the vocabulary are dropped. The ids
// read are kept in a set while reading, so a file is read in time linear in its size. Returns false when the header
// is not that of a vocabulary file of this version.
RDE_INTERNAL b8 fude_vocab_read(const u8* _data, u32 _size, any _user) {
    RDE_UNUSED(_user);
    fude_vocab_clear();
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_VOCAB_VERSION, FUDE_VOCAB_KIND)) {
        return false;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_hash_set TYPE(u32) _ids = rde_hash_set_new(sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL, _heap);    // the words'
    rde_hash_set TYPE(u32) _sent = rde_hash_set_new(sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL, _heap);   // with a sentence
    u32         _tag;
    fude_reader _chunk;
    fude_reader _old     = { 0 };
    b8          _has_old = false;
    b8          _has_new = false;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag == FUDE_VOCAB_CHUNK) {
            _has_new = true;
            fude_vocab_read_words(&_chunk, &_ids);
        } else if(_tag == FUDE_VOCAB_LIST_CHUNK) {
            fude_vocab_read_lists(&_chunk);
        } else if(_tag == FUDE_VOCAB_SENT_CHUNK) {
            fude_vocab_read_sentences(&_chunk, &_sent);
        } else if(_tag == FUDE_VOCAB_OLD_CHUNK) {
            _old     = _chunk;
            _has_old = true;
        }
    }
    if(_has_old && !_has_new) {
        fude_vocab_read_old(&_old, &_ids);
    }
    fude_vocab_read_prune(&_ids);
    rde_hash_set_free(&_ids);
    rde_hash_set_free(&_sent);
    return true;
}

// Replaces the vocabulary with the one saved at _path (empty for NULL). See the header.
void fude_vocab_open(const c8* _path) {
    fude_vocab_ensure();
    fude_vocab_clear();
    rde_snprintf(fude_vocab_path, sizeof(fude_vocab_path), "%s", _path != NULL ? _path : "");
    fude_vocab_changes++;
    if(_path == NULL) {
        return;
    }

    fude_save_load(_path, fude_vocab_read, NULL);   // When nothing has been saved yet, the vocabulary is empty.
}

// Frees the words, lists and sentences and forgets the path, without saving.
void fude_vocab_close(void) {
    fude_vocab_clear();
    if(fude_vocab_ready) {
        rde_arr_free(&fude_vocab_words);
        rde_arr_free(&fude_vocab_sents);
        rde_arr_free(&fude_vocab_lists);
        fude_vocab_ready = false;
    }
    if(fude_vocab_by_text.memory != NULL) {
        rde_hash_map_free(&fude_vocab_by_text);
        rde_memset(&fude_vocab_by_text, 0, sizeof(fude_vocab_by_text));   // (made again on first use)
    }
    fude_vocab_path[0] = 0;
    fude_vocab_changes++;
}

// --- words ------------------------------------------------------------------------------

// Returns how many words are saved.
u32 fude_vocab_count(void) {
    return fude_vocab_ready ? (u32)rde_arr_length(&fude_vocab_words) : 0u;
}

// Returns the _index-th word in the order saved, or NULL past the end.
const fude_vocab_word* fude_vocab_at(u32 _index) {
    return _index < fude_vocab_count() ? &((const fude_vocab_word*)fude_vocab_words.memory)[_index] : NULL;
}

// Returns the word with id _id, or NULL when there is none.
const fude_vocab_word* fude_vocab_get(u32 _id) {
    const i32 _at = fude_vocab_index(_id);
    return _at >= 0 ? fude_vocab_at((u32)_at) : NULL;
}

// Returns the id of the word with exactly this written form and reading (NULL reading is ""), or 0 when none: looked
// up in fude_vocab_by_text, made again first when the words changed since it was right.
u32 fude_vocab_find(const c8* _written, const c8* _reading) {
    fude_vocab_key _key;
    if(_written == NULL || fude_vocab_count() == 0u || !fude_vocab_key_make(&_key, _written, _reading)) {
        return 0u;
    }
    if(fude_vocab_by_text.memory == NULL) {
        fude_vocab_by_text = rde_hash_map_new(sizeof(fude_vocab_key), sizeof(u32), fude_vocab_key_hash, fude_vocab_key_compare, NULL,
                                              rde_memory_allocator_get_default_std());
        fude_vocab_by_text_at = fude_vocab_words_changed - 1u;   // (not right yet)
    }
    if(fude_vocab_by_text_at != fude_vocab_words_changed) {
        rde_hash_map_clear(&fude_vocab_by_text);
        for(u32 _i = 0; _i < fude_vocab_count(); _i++) {   // (in order: the first of two alike is kept)
            const fude_vocab_word* _w = fude_vocab_at(_i);
            fude_vocab_key _k;
            if(fude_vocab_key_make(&_k, _w->written, _w->reading)) {
                rde_hash_map_add(&fude_vocab_by_text, (const any)&_k, (const any)&_w->id);
            }
        }
        fude_vocab_by_text_at = fude_vocab_words_changed;
    }
    // Copied out, not read through a u32*: a map keeps each entry packed (state, key, value), and this key's size (its
    // two texts, each a multiple of four bytes and one more) leaves the value where no u32 may be read from.
    const any _found = rde_hash_map_find(&fude_vocab_by_text, (const any)&_key);
    u32 _id = 0u;
    if(_found != NULL) {
        rde_memcpy(&_id, _found, sizeof(_id));
    }
    return _id;
}

// Does fude_vocab_add's work in memory only, so a caller adding many words can save once. Sets *_id to the word's id:
// the new one, the saved word's when that written form and reading are saved already, or 0 for an empty written form.
// Returns true only when a word was added. The texts are compared after being cut to the word's sizes.
RDE_INTERNAL b8 fude_vocab_add_in_memory(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji, u32* _id) {
    fude_vocab_ensure();
    *_id = 0u;
    if(_written == NULL || _written[0] == 0) {
        return false;
    }
    fude_vocab_word _w = { .kanji = _kanji, .added = (u64)rde_date_now_unix_seconds() };
    fude_utf8_set(_w.written, sizeof(_w.written), _written);
    fude_utf8_set(_w.reading, sizeof(_w.reading), _reading);
    fude_utf8_set(_w.meaning, sizeof(_w.meaning), _meaning);
    *_id = fude_vocab_find(_w.written, _w.reading);
    if(*_id != 0u) {
        return false;
    }
    _w.id = fude_vocab_next++;
    rde_arr_add(&fude_vocab_words, &_w);
    fude_vocab_words_added(&_w);
    *_id = _w.id;
    return true;
}

// Saves a word and returns its id, saving the file only when the word is new. See the header.
u32 fude_vocab_add(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    u32 _id;
    if(fude_vocab_add_in_memory(_written, _reading, _meaning, _kanji, &_id) && !fude_vocab_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "vocab: could not save the vocabulary (%s)", fude_vocab_path);
    }
    return _id;
}

// Changes word _id's texts and saves; its id, lists, sentence and reviews stay. See the header for when it refuses.
// A failed save is not reported here.
b8 fude_vocab_update(u32 _id, const c8* _written, const c8* _reading, const c8* _meaning) {
    const i32 _at = fude_vocab_index(_id);
    if(_at < 0 || _written == NULL || _written[0] == 0) {
        return false;
    }
    c8 _wr[FUDE_USERWORD_WRITTEN];
    c8 _rd[FUDE_USERWORD_READING];
    fude_utf8_set(_wr, sizeof(_wr), _written);
    fude_utf8_set(_rd, sizeof(_rd), _reading);
    const u32 _other = fude_vocab_find(_wr, _rd);
    if(_other != 0u && _other != _id) {
        return false;   // Another saved word already has this written form and reading.
    }
    fude_vocab_word* _w = &((fude_vocab_word*)fude_vocab_words.memory)[_at];
    rde_memcpy(_w->written, _wr, sizeof(_wr));
    rde_memcpy(_w->reading, _rd, sizeof(_rd));
    fude_utf8_set(_w->meaning, sizeof(_w->meaning), _meaning);
    fude_vocab_words_touched();   // (its texts changed: the map is made again when next needed)
    fude_vocab_save();
    return true;
}

// Removes word _id, its sentence and its place in every list, and saves. Nothing happens when there is no such word.
void fude_vocab_remove(u32 _id) {
    const i32 _at = fude_vocab_index(_id);
    if(_at < 0) {
        return;
    }
    rde_arr_remove(&fude_vocab_words, (usize)_at);
    fude_vocab_words_touched();
    const i32 _sent = fude_vocab_sent_index(_id);
    if(_sent >= 0) {
        rde_arr_remove(&fude_vocab_sents, (usize)_sent);
    }
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        rde_arr* _ids = &fude_vocab_list_slot(_l)->words;
        for(u32 _k = (u32)rde_arr_length(_ids); _k-- > 0;) {
            if(((const u32*)_ids->memory)[_k] == _id) {
                rde_arr_remove(_ids, _k);
            }
        }
    }
    fude_vocab_save();
}

// --- a word's sentence ------------------------------------------------------------------

// Sets, replaces or (both texts empty) removes word _id's sentence, and saves when it changed. See the header.
void fude_vocab_set_sentence(u32 _id, const c8* _taught, const c8* _translation) {
    if(fude_vocab_index(_id) < 0) {
        return;
    }
    const b8  _none = (_taught == NULL || _taught[0] == 0) && (_translation == NULL || _translation[0] == 0);
    const i32 _at   = fude_vocab_sent_index(_id);
    if(_none) {
        if(_at >= 0) {
            rde_arr_remove(&fude_vocab_sents, (usize)_at);
            fude_vocab_save();
        }
        return;
    }
    fude_vocab_sent _sent = { .id = _id };
    fude_utf8_set(_sent.taught, sizeof(_sent.taught), _taught);
    fude_utf8_set(_sent.translation, sizeof(_sent.translation), _translation);
    if(_at >= 0) {
        fude_vocab_sent* _there = &((fude_vocab_sent*)fude_vocab_sents.memory)[_at];
        if(rde_strcmp(_there->taught, _sent.taught) == 0 && rde_strcmp(_there->translation, _sent.translation) == 0) {
            return;   // The sentence is as it was, so there is nothing to save.
        }
        *_there = _sent;
    } else {
        rde_arr_add(&fude_vocab_sents, &_sent);
    }
    fude_vocab_save();
}

// Returns word _id's sentence and sets *_translation (when not NULL) to its translation; "" for each when it has none.
const c8* fude_vocab_sentence(u32 _id, const c8** _translation) {
    const i32              _at   = fude_vocab_sent_index(_id);
    const fude_vocab_sent* _sent = _at >= 0 ? &((const fude_vocab_sent*)fude_vocab_sents.memory)[_at] : NULL;
    if(_translation != NULL) {
        *_translation = _sent != NULL ? _sent->translation : "";
    }
    return _sent != NULL ? _sent->taught : "";
}

// Returns whether code point _cp is one of the UTF-8 _text's characters.
RDE_INTERNAL b8 fude_vocab_has_char(const c8* _text, u32 _cp) {
    const c8* _p = _text;
    for(u32 _c = fude_utf8_next(&_p); _c != 0u; _c = fude_utf8_next(&_p)) {
        if(_c == _cp) {
            return true;
        }
    }
    return false;
}

// Lists a character's own words: those saved from its page, and those written with it. See the header for _out and
// the return value. A _kanji of 0 has no words.
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

// Returns how many words character _kanji has, as fude_vocab_of_kanji counts them.
u32 fude_vocab_kanji_count(u32 _kanji) {
    return fude_vocab_of_kanji(_kanji, NULL, 0u);
}

// Fills _out with the _index-th of character _kanji's words, pointing into the saved word. Returns false past the last.
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

// Returns how many lists there are (none before the vocabulary's arrays are created).
u32 fude_vocab_list_count(void) {
    return fude_vocab_ready ? (u32)rde_arr_length(&fude_vocab_lists) : 0u;
}

// Returns the id of the _index-th list in the order made, or 0 past the end.
u32 fude_vocab_list_at(u32 _index) {
    return _index < fude_vocab_list_count() ? fude_vocab_list_slot(_index)->id : 0u;
}

// Returns list _list's name, or "" when there is no such list.
const c8* fude_vocab_list_name(u32 _list) {
    const fude_vocab_list* _l = fude_vocab_list_get(_list);
    return _l != NULL ? _l->name : "";
}

// Makes a new, empty list named _name (cut to FUDE_VOCAB_LIST_NAME), saves, and returns its id. Returns 0 when the
// name is empty.
u32 fude_vocab_list_add(const c8* _name) {
    if(_name == NULL || _name[0] == 0) {
        return 0u;
    }
    fude_vocab_ensure();
    fude_vocab_list _l = { .id = fude_vocab_next_list++, .created = (u64)rde_date_now_unix_seconds() };
    _l.words           = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_utf8_set(_l.name, sizeof(_l.name), _name);
    rde_arr_add(&fude_vocab_lists, &_l);
    fude_vocab_save();
    return _l.id;
}

// Renames list _list and saves. Returns false when there is no such list or the name is empty.
b8 fude_vocab_list_rename(u32 _list, const c8* _name) {
    fude_vocab_list* _l = fude_vocab_list_get(_list);
    if(_l == NULL || _name == NULL || _name[0] == 0) {
        return false;
    }
    fude_utf8_set(_l->name, sizeof(_l->name), _name);
    fude_vocab_save();
    return true;
}

// Removes list _list, keeping the others in the order made, and saves. Its words stay saved.
void fude_vocab_list_remove(u32 _list) {
    for(u32 _l = 0; _l < fude_vocab_list_count(); _l++) {
        if(fude_vocab_list_slot(_l)->id == _list) {
            rde_arr_free(&fude_vocab_list_slot(_l)->words);
            rde_arr_remove(&fude_vocab_lists, _l);   // The later lists move down one place.
            fude_vocab_save();
            return;
        }
    }
}

// Copies list _list's word ids into _out, or only counts them when _out is NULL. See the header.
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
        rde_memcpy(_out, _l->words.memory, sizeof(u32) * _k);
    }
    return _k;
}

// Returns whether word _word is in list _list; false when there is no such list.
b8 fude_vocab_in_list(u32 _list, u32 _word) {
    const fude_vocab_list* _l = fude_vocab_list_get(_list);
    for(u32 _k = 0; _l != NULL && _k < (u32)rde_arr_length(&_l->words); _k++) {
        if(((const u32*)_l->words.memory)[_k] == _word) {
            return true;
        }
    }
    return false;
}

// Does fude_vocab_set_in_list's work in memory only, so a caller changing many can save once. Returns whether the
// list changed: false when there is no such list, the word is already in (or out), or the word to put in is not saved.
RDE_INTERNAL b8 fude_vocab_set_in_list_in_memory(u32 _list, u32 _word, b8 _in) {
    fude_vocab_list* _l = fude_vocab_list_get(_list);
    if(_l == NULL || fude_vocab_in_list(_list, _word) == _in || (_in && fude_vocab_index(_word) < 0)) {
        return false;
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
    return true;
}

// Puts word _word in list _list or takes it out, and saves when the list changed.
void fude_vocab_set_in_list(u32 _list, u32 _word, b8 _in) {
    if(fude_vocab_set_in_list_in_memory(_list, _word, _in)) {
        fude_vocab_save();
    }
}

// Writes into _out the app's "List N" name, in its language, for the first N from the number of lists plus one that
// no list is named yet.
void fude_vocab_list_new_name(c8* _out, usize _size) {
    for(u32 _n = fude_vocab_list_count() + 1u;; _n++) {
        const fude_text_arg _arg = FUDE_TN(_n);
        fude_text_format(_out, _size, FUDE_TEXT_VOCAB_LIST_N, &_arg, 1u);
        b8 _taken = false;
        for(u32 _l = 0; _l < fude_vocab_list_count() && !_taken; _l++) {
            _taken = rde_strcmp(fude_vocab_list_slot(_l)->name, _out) == 0;
        }
        if(!_taken) {
            return;
        }
    }
}

// Returns the change counter, which goes up on every change so a view knows when to look again.
u32 fude_vocab_revision(void) {
    return fude_vocab_changes;
}

// Saves characters as a new list of one-character words (see the header for how each is read). A character already
// saved as a word with the same reading is not added again but goes in the list all the same. The words are added in
// memory and the file saved once at the end.
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
            // A letter of a script (Japanese: a kana) has no text of its own, so its meaning is its Latin name.
            rde_snprintf(_reading, sizeof(_reading), "%s", _written);   // A script's letter is read as itself.
            _meaning = _romaji;
        } else {
            // The first reading of the kind a one-character word is read by (lang.h; Japanese: kun, else on), up to
            // its stem mark '.', without the '-' marks.
            const c8* _try  = fude_kanji_reading(_db, &_ch, fude_lang_word_reading_kind(0u));
            const c8* _from = _try[0] != 0 ? _try : fude_kanji_reading(_db, &_ch, fude_lang_word_reading_kind(1u));
            // Whole characters at a time, so a reading too long for the field ends before a character, never in one.
            usize     _k    = 0;
            u32       _len  = 0;
            for(const c8* _c = _from; *_c != 0; _c += _len) {
                const u32 _cp = fude_utf8_decode(_c, &_len);
                if(_cp == '.' || _cp == 0x3001u || _k + _len >= sizeof(_reading)) {
                    break;   // the stem mark, an ideographic comma (、: the next reading), or no room for the character
                }
                if(_cp != '-') {
                    rde_memcpy(&_reading[_k], _c, _len);
                    _k += _len;
                }
            }
            _reading[_k] = 0;
        }
        // Counted when it goes in: a character asked for twice is in the list once, and counted once.
        u32 _id;
        fude_vocab_add_in_memory(_written, _reading, _meaning, _ch.codepoint, &_id);
        if(_id != 0u && fude_vocab_set_in_list_in_memory(_list, _id, true)) {
            (*_saved)++;
        }
    }
    // The file is saved once for all the characters, not twice for each (once added, once put in the list).
    if(!fude_vocab_save()) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "vocab: could not save the vocabulary (%s)", fude_vocab_path);
    }
    return _list;
}
