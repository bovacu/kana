// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/models/charnote.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See charnote.h.
//
// The notes are one array sorted by code point, so a note is found by binary search and a new one is inserted in its
// place. There are a few hundred notes at most, so moving the rest of the array on an insert costs nothing. Each note
// keeps its text in a fixed buffer of FUDE_CHARNOTE_TEXT bytes, which keeps the array's items all the same size.
// ===========================================================================

// The file's format version, and the four-character tags of its kind and of its one chunk (the layout is in
// charnote.h).
#define FUDE_CHARNOTES_VERSION 1u
#define FUDE_CHARNOTES_KIND    FUDE_TAG('C', 'N', 'O', 'T')
#define FUDE_CHARNOTES_CHUNK   FUDE_TAG('C', 'N', 'T', 'S')

// One note: the character's code point and the note's text, in UTF-8 and NUL-terminated.
typedef struct {
    u32 codepoint;
    c8  text[FUDE_CHARNOTE_TEXT];
} fude_charnote;

// The notes, sorted by code point. The array is created on first use (fude_charnotes_ensure), so the notes work in
// memory without a file, as the tests use them.
static rde_arr TYPE(fude_charnote) fude_charnotes_all;
static b8                          fude_charnotes_ready   = false;
// The file the notes are saved to, or "" to keep them in memory only.
static c8                          fude_charnotes_path[RDE_MAX_PATH];
// Goes up on every change; fude_charnotes_revision returns it.
static u32                         fude_charnotes_changes = 0;

// Creates the notes' array the first time it is needed.
RDE_INTERNAL void fude_charnotes_ensure(void) {
    if(!fude_charnotes_ready) {
        fude_charnotes_all   = rde_arr_new(sizeof(fude_charnote), rde_memory_allocator_get_default_std());
        fude_charnotes_ready = true;
    }
}

// Finds _codepoint's note by binary search. When it has one, sets *_found to true and returns its index; otherwise
// sets *_found to false and returns the index where its note would be inserted to keep the array sorted.
RDE_INTERNAL u32 fude_charnotes_find(u32 _codepoint, b8* _found) {
    const fude_charnote* _n  = (const fude_charnote*)fude_charnotes_all.memory;
    u32                  _lo = 0;
    u32                  _hi = (u32)rde_arr_length(&fude_charnotes_all);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_n[_mid].codepoint < _codepoint) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    *_found = _lo < (u32)rde_arr_length(&fude_charnotes_all) && _n[_lo].codepoint == _codepoint;
    return _lo;
}

// Counts a change and, when the notes have a file, writes the whole file again. The buffer starts with room for about
// 64 bytes a note and grows when the notes are longer. A file that cannot be written is logged; the notes stay in
// memory all the same.
RDE_INTERNAL void fude_charnotes_save(void) {
    fude_charnotes_changes++;
    if(fude_charnotes_path[0] == 0) {
        return;
    }
    const u32  _count = (u32)rde_arr_length(&fude_charnotes_all);
    fude_bytes _b     = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 16u + _count * 64u);
    fude_put_header(&_b, FUDE_CHARNOTES_VERSION, FUDE_CHARNOTES_KIND);
    const u32 _chunk = fude_chunk_begin(&_b, FUDE_CHARNOTES_CHUNK);
    fude_put_u32(&_b, _count);
    const fude_charnote* _n = (const fude_charnote*)fude_charnotes_all.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _len = (u32)rde_strlen(_n[_i].text);
        fude_put_u32(&_b, _n[_i].codepoint);
        fude_put_u16(&_b, (u16)_len);
        fude_put_data(&_b, _n[_i].text, _len);
    }
    fude_chunk_end(&_b, _chunk);
    if(!fude_bytes_write_and_free(&_b, fude_charnotes_path, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "character notes: could not save the character notes (%s)", fude_charnotes_path);
    }
}

// Reads the character notes file's bytes into the notes, starting from no notes; fude_save_load (save.h) calls it, a
// second time for the .bak when the main file fails. Unknown chunks are skipped, a note that runs past the chunk's end
// stops the reading, and an empty note or a second note on the same character is dropped. Returns false when the
// header is not that of a character notes file of this version.
RDE_INTERNAL b8 fude_charnotes_read(const u8* _data, u32 _size, any _user) {
    RDE_UNUSED(_user);
    rde_arr_clear(&fude_charnotes_all);
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_CHARNOTES_VERSION, FUDE_CHARNOTES_KIND)) {
        return false;
    }
    u32         _tag;
    fude_reader _chunk;
    while(fude_next_chunk(&_r, &_tag, &_chunk)) {
        if(_tag != FUDE_CHARNOTES_CHUNK) {
            continue;
        }
        const u32 _count = fude_get_u32(&_chunk);
        for(u32 _i = 0; _i < _count && _chunk.ok; _i++) {
            fude_charnote _n = { 0 };
            _n.codepoint     = fude_get_u32(&_chunk);
            const u32 _len   = fude_get_u16(&_chunk);
            if(!_chunk.ok || _len > _chunk.size - _chunk.pos) {
                break;
            }
            fude_utf8_copy(_n.text, sizeof(_n.text), (const c8*)&_chunk.data[_chunk.pos], _len);
            _chunk.pos += _len;
            b8        _found;
            const u32 _at = fude_charnotes_find(_n.codepoint, &_found);
            if(!_found && _n.text[0] != 0) {
                rde_arr_insert(&fude_charnotes_all, _at, &_n);
            }
        }
    }
    return true;
}

// Replaces the notes with those saved at _path (none for NULL). See the header.
void fude_charnotes_open(const c8* _path) {
    fude_charnotes_ensure();
    rde_arr_clear(&fude_charnotes_all);
    rde_snprintf(fude_charnotes_path, sizeof(fude_charnotes_path), "%s", _path != NULL ? _path : "");
    fude_charnotes_changes++;
    if(_path == NULL) {
        return;
    }
    fude_save_load(_path, fude_charnotes_read, NULL);   // When nothing has been saved yet, there are no notes.
}

// Frees the notes and forgets the path, without saving.
void fude_charnotes_close(void) {
    if(fude_charnotes_ready) {
        rde_arr_free(&fude_charnotes_all);
        fude_charnotes_ready = false;
    }
    fude_charnotes_path[0] = 0;
    fude_charnotes_changes++;
}

// Returns _codepoint's note, or "" when it has none. See the header for how long the pointer stays valid.
const c8* fude_charnote_get(u32 _codepoint) {
    if(!fude_charnotes_ready) {
        return "";
    }
    b8        _found;
    const u32 _at = fude_charnotes_find(_codepoint, &_found);
    return _found ? ((const fude_charnote*)fude_charnotes_all.memory)[_at].text : "";
}

// Sets, replaces or (with an empty text) removes _codepoint's note, and saves the file when something changed. A text
// equal to the current note changes nothing and is not saved again.
void fude_charnote_set(u32 _codepoint, const c8* _text) {
    fude_charnotes_ensure();
    // Trim the spaces and line breaks around the text.
    const c8* _s = _text != NULL ? _text : "";
    while(*_s == ' ' || *_s == '\n') {
        _s++;
    }
    usize _len = rde_strlen(_s);
    while(_len > 0 && (_s[_len - 1u] == ' ' || _s[_len - 1u] == '\n')) {
        _len--;
    }
    b8        _found;
    const u32 _at = fude_charnotes_find(_codepoint, &_found);
    if(_len == 0) {
        if(_found) {
            rde_arr_remove(&fude_charnotes_all, _at);
            fude_charnotes_save();
        }
        return;
    }
    fude_charnote _n = { .codepoint = _codepoint };
    fude_utf8_copy(_n.text, sizeof(_n.text), _s, _len);
    if(_found) {
        fude_charnote* _old = &((fude_charnote*)fude_charnotes_all.memory)[_at];
        if(rde_strcmp(_old->text, _n.text) == 0) {
            return;
        }
        *_old = _n;
    } else {
        rde_arr_insert(&fude_charnotes_all, _at, &_n);
    }
    fude_charnotes_save();
}

// Returns how many characters have a note.
u32 fude_charnotes_count(void) {
    return fude_charnotes_ready ? (u32)rde_arr_length(&fude_charnotes_all) : 0u;
}

// Returns the change counter, which goes up on every change so a view knows when to look at the notes again.
u32 fude_charnotes_revision(void) {
    return fude_charnotes_changes;
}
