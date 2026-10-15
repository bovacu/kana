// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/chars/wordsplit.h"

#include "drawing/base/utf8.h"

// ===========================================================================
// See wordsplit.h.
//
// The index keys each written form by its bytes, compared whole: two words are "spelt alike" only when their forms
// are equal, never because their hashes are. The forms are kept, folded, in one buffer of the index's own, so that a
// key never points into the database. The map gives a form's GROUP, the words spelt that way: they lie together in
// one array (alike), the commonest of them first by rank, so the first is the word a form is found as, and the
// order is the one fude_wordsplit_offer offers them in.
// ===========================================================================

// The most words of a character looked at for their place among its examples.
#define FUDE_WORDSPLIT_EXAMPLES 1024u   // per character

// A written form: `bytes` bytes at text. It is not NUL-terminated, because it is a stretch of a line or a form in the
// index's buffer.
typedef struct {
    const c8* text;
    usize     bytes;
} fude_wordsplit_key;

// The index, of one database at a time (see the header).
static struct {
    const fude_kanji_db* db;      // what the index is of; NULL when none is built
    rde_arr              forms;   // c8: every word's written form, folded, one after another (the keys point into it)
    rde_hash_map         groups;  // fude_wordsplit_key to u32: the group of the words written so
    // For each word, how common it is: the data's 'WFRQ' (lower is commoner) in the high 16 bits, and how soon it is
    // one of a character's examples (its place in the lists, best first; 0xFFFF for never) in the low 16. Lower wins.
    rde_arr              rank;    // u32 per word
    // Every word that has a form, by group: group g is alike[group_at[g] .. group_at[g + 1]), the commonest first
    // (the lower rank, then the lower word number, so that the order is the same every time).
    rde_arr              alike;      // u32: word numbers
    rde_arr              group_at;   // u32 per group, then one past the last word
    rde_arr              group_of;   // u32 per word: its group, or UINT32_MAX when it has no form
} fude_wordsplit_index;

// Hashes a key. The map hands keys over unaligned (rde.h, rde_hash_map_hash_fn), so they are copied out to be read.
RDE_INTERNAL u64 fude_wordsplit_key_hash(const any _key) {
    fude_wordsplit_key _k;
    rde_memcpy(&_k, _key, sizeof(_k));
    return rde_hash_64((any)_k.text, _k.bytes);
}

// Compares two keys, by length first, then by bytes. They are copied out to be read, as in the hash.
RDE_INTERNAL i32 fude_wordsplit_key_cmp(const any _a, const any _b) {
    fude_wordsplit_key _ka, _kb;
    rde_memcpy(&_ka, _a, sizeof(_ka));
    rde_memcpy(&_kb, _b, sizeof(_kb));
    if(_ka.bytes != _kb.bytes) {
        return _ka.bytes < _kb.bytes ? -1 : 1;
    }
    return rde_memcmp(_ka.text, _kb.text, _ka.bytes);
}

// Returns _c as the language compares it, or 0 when the language leaves it out (lang.h: fude_lang_words_def.fold).
RDE_INTERNAL u32 fude_wordsplit_fold(const fude_lang_words_def* _def, u32 _c) {
    return _def->fold != NULL ? _def->fold(_c) : _c;
}

// Adds _written, folded, onto the end of the index's forms.
RDE_INTERNAL void fude_wordsplit_add_form(const fude_lang_words_def* _def, const c8* _written) {
    if(_def->fold == NULL) {
        const usize _bytes = rde_strlen(_written);   // without a fold, the form is its bytes as they are
        if(_bytes > 0) {
            rde_memcpy(rde_arr_add_n(&fude_wordsplit_index.forms, _bytes), _written, _bytes);
        }
        return;
    }
    const c8* _p = _written;
    for(u32 _read = fude_utf8_next(&_p); _read != 0u; _read = fude_utf8_next(&_p)) {
        const u32 _c = fude_wordsplit_fold(_def, _read);
        if(_c == 0u) {
            continue;
        }
        c8        _one[5];
        const u32 _len = fude_utf8_put(_c, _one);
        rde_memcpy(rde_arr_add_n(&fude_wordsplit_index.forms, _len), _one, _len);
    }
}

// Frees the index, when one is built.
RDE_INTERNAL void fude_wordsplit_free(void) {
    if(fude_wordsplit_index.db == NULL) {
        return;
    }
    rde_arr_free(&fude_wordsplit_index.forms);
    rde_hash_map_free(&fude_wordsplit_index.groups);
    rde_arr_free(&fude_wordsplit_index.rank);
    rde_arr_free(&fude_wordsplit_index.alike);
    rde_arr_free(&fude_wordsplit_index.group_at);
    rde_arr_free(&fude_wordsplit_index.group_of);
    fude_wordsplit_index.db = NULL;
}

// Orders two word numbers the commonest first (see fude_wordsplit_index.alike): by rank, then by number.
RDE_INTERNAL i32 fude_wordsplit_by_rank(const any _a, const any _b) {
    const u32* _rank = (const u32*)fude_wordsplit_index.rank.memory;
    const u32  _x    = *(const u32*)_a;
    const u32  _y    = *(const u32*)_b;
    if(_rank[_x] != _rank[_y]) {
        return _rank[_x] < _rank[_y] ? -1 : 1;
    }
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Fills each word's rank (see fude_wordsplit_index.rank).
RDE_INTERNAL void fude_wordsplit_rank(const fude_kanji_db* _db, u32* _rank) {
    for(u32 _w = 0; _w < _db->word_count; _w++) {
        _rank[_w] = ((u32)fude_kanji_word_freq(_db, _w) << 16) | 0xFFFFu;
    }
    static u32 _list[FUDE_WORDSPLIT_EXAMPLES];
    for(u32 _r = 0; _r < _db->count; _r++) {
        const u32 _n = fude_kanji_words(_db, _r, _list, FUDE_WORDSPLIT_EXAMPLES, NULL);
        for(u32 _p = 0; _p < _n; _p++) {
            const u32 _w = _list[_p];
            if(_w < _db->word_count && _p < (_rank[_w] & 0xFFFFu)) {
                _rank[_w] = (_rank[_w] & 0xFFFF0000u) | _p;
            }
        }
    }
}

// Builds the index of _db's words, after freeing any other.
RDE_INTERNAL void fude_wordsplit_build(const fude_kanji_db* _db) {
    fude_wordsplit_free();
    const fude_lang_words_def* _def   = fude_lang_words();
    const u32                  _count = _db->word_count;
    rde_memory_allocator*      _heap  = rde_memory_allocator_get_default_std();
    fude_wordsplit_index.db    = _db;
    // A word's written form takes about 8 bytes.
    fude_wordsplit_index.forms = rde_arr_new_with_capacity(sizeof(c8), 8u * (usize)_count, _heap);   // the forms
    fude_wordsplit_index.groups = rde_hash_map_new(sizeof(fude_wordsplit_key), sizeof(u32), fude_wordsplit_key_hash, fude_wordsplit_key_cmp, NULL, _heap);
    rde_hash_map_reserve(&fude_wordsplit_index.groups, _count);
    fude_wordsplit_index.rank     = rde_arr_new_with_capacity(sizeof(u32), _count, _heap);
    fude_wordsplit_index.alike    = rde_arr_new_with_capacity(sizeof(u32), _count, _heap);
    fude_wordsplit_index.group_at = rde_arr_new(sizeof(u32), _heap);
    fude_wordsplit_index.group_of = rde_arr_new_with_capacity(sizeof(u32), _count, _heap);
    rde_arr_resize(&fude_wordsplit_index.rank, _count);
    rde_arr_resize(&fude_wordsplit_index.group_of, _count);
    u32* _group_of = (u32*)fude_wordsplit_index.group_of.memory;
    fude_wordsplit_rank(_db, (u32*)fude_wordsplit_index.rank.memory);

    // The forms go in first, all of them, so that the buffer no longer moves once the keys point into it.
    rde_arr _starts = rde_arr_new_with_capacity(sizeof(usize), _count + 1u, _heap);   // a usize per word, then the end
    for(u32 _w = 0; _w <= _count; _w++) {
        const usize _start = rde_arr_length(&fude_wordsplit_index.forms);
        rde_arr_add(&_starts, (any)&_start);
        fude_kanji_word _word;
        if(_w < _count && fude_kanji_word_at(_db, _w, &_word)) {
            fude_wordsplit_add_form(_def, _word.written);
        }
    }
    // Each word's group: the first word of a form makes it, and the others written so join it.
    rde_arr TYPE(u32) _sizes = rde_arr_new(sizeof(u32), _heap);   // words per group
    const usize*      _start = (const usize*)_starts.memory;
    for(u32 _w = 0; _w < _count; _w++) {
        _group_of[_w] = UINT32_MAX;
        const fude_wordsplit_key _key = { (const c8*)fude_wordsplit_index.forms.memory + _start[_w], _start[_w + 1u] - _start[_w] };
        if(_key.bytes == 0) {
            continue;
        }
        u32 _g = (u32)rde_arr_length(&_sizes);
        if(rde_hash_map_add(&fude_wordsplit_index.groups, (any)&_key, &_g)) {
            const u32 _none = 0u;
            rde_arr_add(&_sizes, (any)&_none);   // the first word written so: a new group
        } else {
            _g = *(const u32*)rde_hash_map_find(&fude_wordsplit_index.groups, (any)&_key);
        }
        ((u32*)_sizes.memory)[_g]++;
        _group_of[_w] = _g;
    }
    rde_arr_free(&_starts);

    // Where each group starts, then its words put there in word order, then each group the commonest first.
    const u32 _groups = (u32)rde_arr_length(&_sizes);
    rde_arr_resize(&fude_wordsplit_index.group_at, _groups + 1u);
    u32* _at   = (u32*)fude_wordsplit_index.group_at.memory;
    u32* _next = (u32*)_sizes.memory;   // from here on, each group's next free place in alike
    u32  _sum  = 0;
    for(u32 _g = 0; _g < _groups; _g++) {
        _at[_g]   = _sum;
        _sum     += _next[_g];
        _next[_g] = _at[_g];
    }
    _at[_groups] = _sum;
    rde_arr_resize(&fude_wordsplit_index.alike, _sum);
    u32* _alike = (u32*)fude_wordsplit_index.alike.memory;
    for(u32 _w = 0; _w < _count; _w++) {
        if(_group_of[_w] != UINT32_MAX) {
            _alike[_next[_group_of[_w]]++] = _w;
        }
    }
    for(u32 _g = 0; _g < _groups; _g++) {
        if(_at[_g + 1u] - _at[_g] > 1u) {
            rde_sort(&_alike[_at[_g]], _at[_g + 1u] - _at[_g], sizeof(u32), fude_wordsplit_by_rank);
        }
    }
    rde_arr_free(&_sizes);
}

// Makes sure the index is of _db, building it when it is of another. Returns false when _db has no words. The index
// knows its database by its address alone, which is enough because an app loads its one database at start and keeps
// it to the end: a database unloaded and another loaded at the same address would be split with the old index.
// Keeping a revision in the database would be the way, should an app ever load another.
RDE_INTERNAL b8 fude_wordsplit_ready(const fude_kanji_db* _db) {
    if(_db == NULL || _db->word_count == 0) {
        return false;
    }
    if(_db != fude_wordsplit_index.db) {
        fude_wordsplit_build(_db);
    }
    return true;
}

// Makes _text ready to split (lang.h: fude_lang_line): its lines are joined, each character is folded, and it is cut
// at FUDE_LANG_LINE characters. letter_at[k] is the first letter at or after character k.
RDE_INTERNAL void fude_wordsplit_line(const fude_lang_words_def* _def, const c8* _text, fude_lang_line* _line) {
    u32 _bytes = 0;
    _line->n   = 0;
    for(const c8* _p = _text; _line->n < FUDE_LANG_LINE;) {
        const u32 _read = fude_utf8_next(&_p);
        if(_read == 0u) {
            break;
        }
        const u32 _c = _read == '\n' || _read == '\r' ? 0u : fude_wordsplit_fold(_def, _read);
        if(_c == 0u) {
            continue;
        }
        _line->at[_line->n]    = _bytes;
        _line->cps[_line->n++] = _c;
        _bytes += fude_utf8_put(_c, _line->text + _bytes);   // 4 bytes a character at most, so it fits
    }
    _line->at[_line->n] = _bytes;
    _line->text[_bytes] = 0;
    u32 _next = _line->n;
    _line->letter_at[_line->n] = _next;
    for(u32 _k = _line->n; _k-- > 0u;) {
        if(_def->letter(_line->cps[_k])) {
            _next = _k;
        }
        _line->letter_at[_k] = _next;
    }
}

// Finds the longest stretch at _at, up to look characters, that is a word as written: it must hold a letter and end
// where a word can (lang.h). It is what a language without a matcher of its own uses. *_take gets how many
// characters it covers (1 when none is a word). Returns the word, or UINT32_MAX.
RDE_INTERNAL u32 fude_wordsplit_longest(const fude_lang_words_def* _def, const fude_lang_line* _line, u32 _at, u32* _take) {
    *_take = 1u;
    for(u32 _len = RDE_MIN(_def->look, _line->n - _at); _len >= 1u && _line->letter_at[_at] < _at + _len; _len--) {
        const u32 _end = _at + _len;
        if(_def->ends != NULL && !_def->ends(_line->cps[_end - 1u], _end < _line->n ? _line->cps[_end] : 0u)) {
            continue;
        }
        const u32 _word = fude_wordsplit_find(_line->text + _line->at[_at], _line->at[_end] - _line->at[_at]);
        if(_word != UINT32_MAX) {
            *_take = _len;
            return _word;
        }
    }
    return UINT32_MAX;
}

// Adds _w at the end of the _found words of _out, unless _out has it already. Returns how many _out then has.
RDE_INTERNAL u32 fude_wordsplit_add_once(u32 _w, u32* _out, u32 _found) {
    for(u32 _k = 0; _k < _found; _k++) {
        if(_out[_k] == _w) {
            return _found;
        }
    }
    _out[_found] = _w;
    return _found + 1u;
}

// Adds _word after the _found words of _out, with the words written the same that are as common: of those, the first
// _alike_max (lang.h: fude_lang_words_def.alike), _word first and the rest the commonest first (fude_wordsplit_index.
// alike), each one unless _out has it already. Returns how many _out then has.
RDE_INTERNAL u32 fude_wordsplit_offer(u32 _word, u32 _alike_max, u32* _out, u32 _found, u32 _max) {
    if(_found >= _max || _alike_max == 0u) {
        return _found;
    }
    const u32* _rank  = (const u32*)fude_wordsplit_index.rank.memory;
    const u32* _alike = (const u32*)fude_wordsplit_index.alike.memory;
    const u32* _at    = (const u32*)fude_wordsplit_index.group_at.memory;
    const u32  _g     = ((const u32*)fude_wordsplit_index.group_of.memory)[_word];
    _found            = fude_wordsplit_add_once(_word, _out, _found);
    u32 _taken        = 1u;
    for(u32 _k = _g != UINT32_MAX ? _at[_g] : 0u; _g != UINT32_MAX && _k < _at[_g + 1u] && _found < _max && _taken < _alike_max; _k++) {
        const u32 _w = _alike[_k];
        if(_w == _word || (_rank[_w] >> 16) != (_rank[_word] >> 16)) {
            continue;
        }
        _taken++;
        _found = fude_wordsplit_add_once(_w, _out, _found);
    }
    return _found;
}

// Splits _text into words (see the header): from the left, the language's matcher, or the longest word, at each
// place, then on past what it took.
u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max) {
    if(_text == NULL || _max == 0 || !fude_wordsplit_ready(_db)) {
        return 0;
    }
    const fude_lang_words_def* _def = fude_lang_words();
    fude_lang_line             _line;
    fude_wordsplit_line(_def, _text, &_line);
    u32 _found = 0;
    for(u32 _at = 0; _at < _line.n && _found < _max;) {
        u32       _take = 1u;
        const u32 _word = _def->match != NULL ? _def->match(&_line, _at, &_take) : fude_wordsplit_longest(_def, &_line, _at, &_take);
        if(_word != UINT32_MAX) {
            _found = fude_wordsplit_offer(_word, _def->alike, _out, _found, _max);
        }
        _at += RDE_MAX(_take, 1u);
    }
    return _found;
}

// Finds the word written exactly as _written (see the header).
u32 fude_wordsplit_word(const fude_kanji_db* _db, const c8* _written) {
    if(_written == NULL || !fude_wordsplit_ready(_db)) {
        return UINT32_MAX;
    }
    fude_lang_line _line;
    fude_wordsplit_line(fude_lang_words(), _written, &_line);
    return fude_wordsplit_find(_line.text, _line.at[_line.n]);
}

// Finds a word by its folded form (see the header).
u32 fude_wordsplit_find(const c8* _key, usize _bytes) {
    if(fude_wordsplit_index.db == NULL || _bytes == 0) {
        return UINT32_MAX;
    }
    // The group's first word is its commonest.
    const fude_wordsplit_key _k = { _key, _bytes };
    const u32*               _g = (const u32*)rde_hash_map_find(&fude_wordsplit_index.groups, (any)&_k);
    return _g != NULL ? ((const u32*)fude_wordsplit_index.alike.memory)[((const u32*)fude_wordsplit_index.group_at.memory)[*_g]] : UINT32_MAX;
}
