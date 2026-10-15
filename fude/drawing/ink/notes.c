// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/notes.h"
#include "drawing/base/text.h"
#include "drawing/base/kfile.h"
#include "drawing/ink/pagefile.h"


// ===========================================================================
// See notes.h.
//
// The list is one array of fude_note in list order; a lookup is a linear search by id, and every change saves the
// whole index at once. Whoever walks the whole list makes a tree of it first (fude_notes_tree_make), with a map from
// id to index, so the walk stays one pass however many notes there are.
// ===========================================================================

// The index file's kind and its one chunk's tag.
#define FUDE_NOTES_KIND        FUDE_TAG('N', 'O', 'T', 'E')
#define FUDE_NOTES_CHUNK_INDEX FUDE_TAG('N', 'I', 'D', 'X')
#define FUDE_NOTES_RECORD_SIZE 20u    // Bytes: id, parent, kind, expanded, document, 1 reserved, created low and high.

// Returns the notes' folder, <save folder>/notes/, made if missing. It is found again should the save folder change
// (the app's own is set before the saves are read, in fude_app_start).
RDE_INTERNAL const c8* fude_notes_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };
    fude_save_subdir("notes", _dir, sizeof(_dir));
    return _dir;
}

// Writes the index's path, <notes folder>/index.kana, into _out.
RDE_INTERNAL void fude_notes_index_path(c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%sindex.kana", fude_notes_dir());
}

// Writes a canvas's page path, <notes folder>/<id>.kana, into _out.
void fude_notes_canvas_path(u32 _id, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%u.kana", fude_notes_dir(), _id);
}

// Writes a canvas's own document path, <notes folder>/<id>.pdf, into _out.
void fude_notes_document_path(u32 _id, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%u.pdf", fude_notes_dir(), _id);
}

// Makes an empty list on the std allocator; ids start at 1, since 0 means "none".
void fude_notes_init(fude_notes* _notes) {
    rde_memset(_notes, 0, sizeof(*_notes));
    _notes->notes   = rde_arr_new(sizeof(fude_note), rde_memory_allocator_get_default_std());
    _notes->next_id = 1;
}

// Frees the list and zeroes the struct.
void fude_notes_destroy(fude_notes* _notes) {
    if(rde_arr_is_inited(&_notes->notes)) {
        rde_arr_free(&_notes->notes);
    }
    rde_memset(_notes, 0, sizeof(*_notes));
}

// Returns the _i-th note of the list (not checked against the length).
RDE_INTERNAL fude_note* fude_notes_at(const fude_notes* _notes, u32 _i) {
    return &((fude_note*)_notes->notes.memory)[_i];
}

// Returns the first note with _id, or NULL.
const fude_note* fude_notes_find(const fude_notes* _notes, u32 _id) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        if(fude_notes_at(_notes, _i)->id == _id) {
            return fude_notes_at(_notes, _i);
        }
    }
    return NULL;
}

// Writes _in into _out (FUDE_NOTE_NAME bytes) as a name is stored: trimmed of spaces, tabs and line breaks at both
// ends, cut to fit between whole UTF-8 characters, and never empty: an empty name becomes _fallback. _out may be
// _fallback itself, which keeps the old name.
RDE_INTERNAL void fude_notes_clean_name(const c8* _in, c8* _out, const c8* _fallback) {
    while(_in != NULL && (*_in == ' ' || *_in == '\t' || *_in == '\n')) {
        _in++;
    }
    usize _n = _in != NULL ? rde_strlen(_in) : 0;
    while(_n > 0 && (_in[_n - 1] == ' ' || _in[_n - 1] == '\t' || _in[_n - 1] == '\n')) {
        _n--;
    }
    if(_n > FUDE_NOTE_NAME - 1) {
        _n = FUDE_NOTE_NAME - 1;
        while(_n > 0 && ((u8)_in[_n] & 0xC0u) == 0x80u) {   // Never cut inside a character.
            _n--;
        }
    }
    if(_n == 0) {
        rde_snprintf(_out, FUDE_NOTE_NAME, "%s", _fallback);
        return;
    }
    rde_memcpy(_out, _in, _n);
    _out[_n] = 0;
}

// Appends a note with the next id, expanded, at the top level, named _name (cleaned as every name is), and returns
// it; the index is not saved. fude_notes_add and fude_notes_rebuild both add through it.
RDE_INTERNAL fude_note* fude_notes_append(fude_notes* _notes, FUDE_NOTE_ _kind, const c8* _name) {
    fude_note _n;
    rde_memset(&_n, 0, sizeof(_n));
    _n.id       = _notes->next_id++;
    _n.kind     = (u8)_kind;
    _n.expanded = true;
    _n.created  = (u64)rde_date_now_unix_seconds();
    fude_notes_clean_name(_name, _n.name, fude_text(_kind == FUDE_NOTE_FOLDER ? FUDE_TEXT_FOLDER : FUDE_TEXT_CANVAS));
    rde_arr_add(&_notes->notes, &_n);
    return fude_notes_at(_notes, (u32)rde_arr_length(&_notes->notes) - 1u);
}

// --- the index file -------------------------------------------------------------------

// Writes the whole index (see notes.h for the layout) atomically. Returns false when it could not be written.
b8 fude_notes_save(fude_notes* _notes) {
    c8 _path[RDE_MAX_PATH];
    fude_notes_index_path(_path, sizeof(_path));

    const u32  _count = (u32)rde_arr_length(&_notes->notes);
    fude_bytes _b     = fude_bytes_new(64u + _count * (FUDE_NOTES_RECORD_SIZE + 1u + FUDE_NOTE_NAME));
    fude_put_header(&_b, FUDE_NOTES_VERSION, FUDE_NOTES_KIND);

    const u32 _chunk = fude_chunk_begin(&_b, FUDE_NOTES_CHUNK_INDEX);
    fude_put_u32(&_b, _notes->next_id);
    fude_put_u32(&_b, _notes->open);
    fude_put_u32(&_b, _count);
    fude_put_u32(&_b, FUDE_NOTES_RECORD_SIZE);
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_note* _n = fude_notes_at(_notes, _i);
        fude_put_u32(&_b, _n->id);
        fude_put_u32(&_b, _n->parent);
        fude_put_u8(&_b, _n->kind);
        fude_put_u8(&_b, _n->expanded ? 1u : 0u);
        fude_put_u8(&_b, _n->kind == FUDE_NOTE_CANVAS ? _n->document : 0u);
        fude_put_u8(&_b, 0u);
        fude_put_u64(&_b, _n->created);
        const u32 _len = (u32)rde_strlen(_n->name);
        fude_put_u8(&_b, (u8)_len);
        fude_put_data(&_b, (const u8*)_n->name, _len);
    }
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// fude_save_load's reader for the index: rebuilds the list from nothing. Records longer than this build's are skipped
// past their known fields; notes with id 0 or an unknown kind are dropped. Returns false, so the file is set aside,
// when it has no readable list.
RDE_INTERNAL b8 fude_notes_read(const u8* _data, u32 _size, any _user) {
    fude_notes* _notes = (fude_notes*)_user;
    rde_arr_clear(&_notes->notes);
    _notes->next_id = 1;
    _notes->open    = 0;
    fude_reader _r = fude_reader_make(_data, _size);
    if(!fude_read_header(&_r, FUDE_NOTES_VERSION, FUDE_NOTES_KIND)) {
        return false;
    }

    b8          _found = false;
    u32         _tag;
    fude_reader _c;
    while(fude_next_chunk(&_r, &_tag, &_c)) {
        if(_tag != FUDE_NOTES_CHUNK_INDEX) {
            continue;
        }
        const u32 _next  = fude_get_u32(&_c);
        const u32 _open  = fude_get_u32(&_c);
        const u32 _count = fude_get_u32(&_c);
        const u32 _rec   = fude_get_u32(&_c);
        // There is no limit on how many notes there are, but each takes its record and its name's length byte at
        // least, so a count the chunk cannot hold is damage.
        if(!_c.ok || _rec < FUDE_NOTES_RECORD_SIZE || (u64)_count * ((u64)_rec + 1u) > (u64)(_c.size - _c.pos)) {
            break;
        }

        rde_arr_clear(&_notes->notes);
        for(u32 _i = 0; _i < _count && _c.ok; _i++) {
            const u32 _start = _c.pos;
            fude_note _n;
            rde_memset(&_n, 0, sizeof(_n));
            _n.id       = fude_get_u32(&_c);
            _n.parent   = fude_get_u32(&_c);
            _n.kind     = fude_get_u8(&_c);
            _n.expanded = fude_get_u8(&_c) != 0;
            _n.document = fude_get_u8(&_c);   // 0 in a file from before documents.
            fude_get_u8(&_c);
            _n.created = fude_get_u64(&_c);
            if(!fude_reader_has(&_c, _start + _rec - _c.pos)) {
                break;
            }
            _c.pos = _start + _rec;   // Skip the rest of a newer build's longer record.

            const u32 _len = fude_get_u8(&_c);
            if(!fude_reader_has(&_c, _len)) {
                break;
            }
            c8 _raw[256];
            rde_memcpy(_raw, &_c.data[_c.pos], _len);
            _raw[_len] = 0;
            _c.pos += _len;
            fude_notes_clean_name(_raw, _n.name, fude_text(_n.kind == FUDE_NOTE_FOLDER ? FUDE_TEXT_FOLDER : FUDE_TEXT_CANVAS));

            if(_n.id != 0 && _n.kind <= FUDE_NOTE_CANVAS) {
                rde_arr_add(&_notes->notes, &_n);
            }
        }
        _notes->next_id = _next;
        _notes->open    = _open;
        _found          = true;
    }
    return _found;
}

// fude_notes_rebuild's crawl callback: adds to the u32 array in _user the id of each canvas file in the folder
// (<id>.kana, a deep-zoom canvas's <id>.zoom, or either's .bak, which is all a kill in the middle of a save may leave).
// Ids have at most 9 digits. Always returns true, so the crawl goes on.
RDE_INTERNAL b8 fude_notes_found_file(const c8* _path, b8 _is_dir, any _user) {
    if(_is_dir) {
        return true;
    }
    const c8* _name = _path;
    for(const c8* _c = _path; *_c != 0; _c++) {
        if(*_c == '/' || *_c == '\\') {
            _name = _c + 1;
        }
    }
    u32   _id     = 0;
    usize _digits = 0;
    for(; _name[_digits] >= '0' && _name[_digits] <= '9' && _digits < 9u; _digits++) {
        _id = _id * 10u + (u32)(_name[_digits] - '0');
    }
    const c8* _rest = &_name[_digits];
    if(_digits > 0 && _id != 0 && (rde_strcmp(_rest, ".kana") == 0 || rde_strcmp(_rest, ".kana.bak") == 0 ||
                                   rde_strcmp(_rest, ".zoom") == 0 || rde_strcmp(_rest, ".zoom.bak") == 0)) {
        rde_arr_add((rde_arr*)_user, &_id);
    }
    return true;
}

// Called when neither the index nor its backup could be read: makes the list again from the canvases' own files.
// Without this, a damaged index would hide every canvas, and new ones would take the ids of canvases whose files are
// still there (and open those old pages). The names, the folders and the library books went with the index, so each
// canvas comes back at the top under a new name ("Canvas 1", "Canvas 2"... in the order of their ids: the list starts
// empty, so those are the names fude_notes_new_name would give), over its own document when it had one. The index is
// saved once, by fude_notes_load after the repair, not once a canvas.
RDE_INTERNAL void fude_notes_rebuild(fude_notes* _notes) {
    rde_arr TYPE(u32) _ids = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_file_crawl_dir_recursively(fude_notes_dir(), fude_notes_found_file, NULL, 0, &_ids);
    rde_sort(_ids.memory, rde_arr_length(&_ids), sizeof(u32), rde_sort_compare_u32);
    rde_arr_clear(&_notes->notes);
    _notes->next_id = 1;
    _notes->open    = 0;
    const c8*  _word = fude_text(FUDE_TEXT_CANVAS);
    const u32* _id   = (const u32*)_ids.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_ids); _i++) {
        if(_id[_i] < _notes->next_id) {
            continue;   // Another file of the same canvas.
        }
        c8 _name[FUDE_NOTE_NAME];
        rde_snprintf(_name, sizeof(_name), "%s %u", _word, (u32)rde_arr_length(&_notes->notes) + 1u);
        _notes->next_id = _id[_i];
        fude_note* _n   = fude_notes_append(_notes, FUDE_NOTE_CANVAS, _name);
        c8 _pdf[RDE_MAX_PATH];
        fude_notes_document_path(_id[_i], _pdf, sizeof(_pdf));
        if(rde_file_exists(_pdf)) {
            _n->document = FUDE_NOTE_DOCUMENT_OWN;
        }
    }
    rde_log_level(RDE_LOG_LEVEL_WARNING, "notes: the index could not be read; %u canvases found again from their files",
                  (u32)rde_arr_length(&_notes->notes));
    rde_arr_free(&_ids);
}

// Returns the note with _id through _index, a map from each id to the index of the first note with it (as
// fude_notes_find finds it), or NULL when there is none.
RDE_INTERNAL fude_note* fude_notes_by_index(const fude_notes* _notes, const rde_hash_map* _index, u32 _id) {
    const u32* _at = (const u32*)rde_hash_map_find(_index, (any)&_id);
    return _at != NULL ? fude_notes_at(_notes, *_at) : NULL;
}

// Makes the list consistent, whatever the file said: every id below next_id, every parent a folder that exists (else
// the note goes to the top), no folder inside itself, and the open note a canvas (the first canvas, or a new one).
// A map from id to index stands in for fude_notes_find, so a load is not a search of the list per note and per step
// up its folders: one pass, and one walk up each folder's parents.
RDE_INTERNAL void fude_notes_repair(fude_notes* _notes) {
    const u32    _count = (u32)rde_arr_length(&_notes->notes);
    rde_hash_map TYPE(u32, u32) _index = rde_hash_map_new(sizeof(u32), sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL,
                                                         rde_memory_allocator_get_default_std());
    rde_hash_map_reserve(&_index, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        rde_hash_map_add(&_index, (any)&fude_notes_at(_notes, _i)->id, &_i);   // The first note with an id keeps it.
    }
    for(u32 _i = 0; _i < _count; _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->id >= _notes->next_id) {
            _notes->next_id = _n->id + 1u;
        }
        const fude_note* _parent = _n->parent != 0 ? fude_notes_by_index(_notes, &_index, _n->parent) : NULL;
        if(_n->parent != 0 && (_parent == NULL || _parent->kind != FUDE_NOTE_FOLDER)) {
            _n->parent = 0;   // An orphan goes to the top level.
        }
    }
    // A folder inside itself (from a damaged file) goes to the top level, which breaks the loop. It is found as
    // fude_notes_is_within finds it: up its parents from its folder, a step a note at most.
    for(u32 _i = 0; _i < _count; _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->kind != FUDE_NOTE_FOLDER || _n->parent == 0) {
            continue;
        }
        u32 _at = _n->parent;
        for(u32 _steps = 0; _at != 0 && _at != _n->id && _steps <= _count; _steps++) {
            const fude_note* _up = fude_notes_by_index(_notes, &_index, _at);
            _at = _up != NULL ? _up->parent : 0u;
        }
        if(_at != 0 && _at == _n->id) {
            _n->parent = 0;
        }
    }
    rde_hash_map_free(&_index);

    const fude_note* _open = fude_notes_find(_notes, _notes->open);
    if(_open == NULL || _open->kind != FUDE_NOTE_CANVAS) {
        _notes->open = 0;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes) && _notes->open == 0; _i++) {
            if(fude_notes_at(_notes, _i)->kind == FUDE_NOTE_CANVAS) {
                _notes->open = fude_notes_at(_notes, _i)->id;
            }
        }
    }
    if(_notes->open == 0) {
        c8 _name[FUDE_NOTE_NAME];
        fude_notes_new_name(_notes, FUDE_NOTE_CANVAS, _name, sizeof(_name));
        _notes->open = fude_notes_add(_notes, FUDE_NOTE_CANVAS, 0, _name);
    }
}

// Reads the index (else its backup, else rebuilds it from the canvas files), or, the first time, makes it from the
// page saved before notes existed; then repairs and saves it. See the header.
void fude_notes_load(fude_notes* _notes) {
    c8 _index[RDE_MAX_PATH];
    fude_notes_index_path(_index, sizeof(_index));
    const FUDE_LOAD_ _was = fude_save_load(_index, fude_notes_read, _notes);
    if(_was == FUDE_LOAD_CORRUPT) {
        fude_notes_rebuild(_notes);
    } else if(_was == FUDE_LOAD_MISSING) {
        // The first time: the page saved before notes existed (page.kana) becomes the first canvas, moved with its
        // backup.
        rde_arr_clear(&_notes->notes);
        _notes->next_id = 1;
        _notes->open    = 0;

        c8 _old[RDE_MAX_PATH];
        rde_snprintf(_old, sizeof(_old), "%s%s", fude_save_dir(), FUDE_SAVE_DOCUMENT_FILE);
        c8 _old_bak[RDE_MAX_PATH];
        rde_snprintf(_old_bak, sizeof(_old_bak), "%s.bak", _old);
        if(rde_file_exists(_old) || rde_file_exists(_old_bak)) {
            const u32 _id = fude_notes_add(_notes, FUDE_NOTE_CANVAS, 0, fude_text(FUDE_TEXT_NOTE_PAGE));
            c8 _new[RDE_MAX_PATH];
            fude_notes_canvas_path(_id, _new, sizeof(_new));
            if(rde_file_exists(_old)) {
                rde_file_move(_old, _new);
            }
            if(rde_file_exists(_old_bak)) {
                c8 _new_bak[RDE_MAX_PATH];
                rde_snprintf(_new_bak, sizeof(_new_bak), "%s.bak", _new);
                rde_file_move(_old_bak, _new_bak);
            }
            _notes->open = _id;
        }
    }

    fude_notes_repair(_notes);
    _notes->revision++;
    fude_notes_save(_notes);
}

// --- changes ------------------------------------------------------------------------------

// Writes "<kind's word> <k>" for the first k, from 1, that no note's name uses. One pass over the names marks the
// numbers they use, written as this writes them (no leading zero, nothing after): the notes can use at most as many
// numbers as there are notes, so the first count + 1 are the only ones that can be the answer, and the rest are not
// kept.
void fude_notes_new_name(const fude_notes* _notes, FUDE_NOTE_ _kind, c8* _out, usize _size) {
    const c8*   _word  = fude_text(_kind == FUDE_NOTE_FOLDER ? FUDE_TEXT_FOLDER : FUDE_TEXT_CANVAS);
    const usize _wlen  = rde_strlen(_word);
    const u32   _count = (u32)rde_arr_length(&_notes->notes);
    rde_arr TYPE(b8) _used = rde_arr_new(sizeof(b8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_used, (usize)_count + 2u);   // Whether k is used, for k from 1 to count + 1; all false.
    b8* _taken = (b8*)_used.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const c8* _name = fude_notes_at(_notes, _i)->name;
        if(rde_strncmp(_name, _word, _wlen) != 0 || _name[_wlen] != ' ' || _name[_wlen + 1u] < '1' || _name[_wlen + 1u] > '9') {
            continue;
        }
        u64       _k = 0;
        const c8* _d = _name + _wlen + 1u;
        for(; *_d >= '0' && *_d <= '9' && _k <= (u64)_count + 1u; _d++) {
            _k = _k * 10u + (u64)(*_d - '0');
        }
        if(*_d == 0 && _k <= (u64)_count + 1u) {
            _taken[_k] = true;
        }
    }
    u32 _k = 1;
    while(_taken[_k]) {
        _k++;   // One of 1 .. count + 1 is free, since count names use count numbers at most.
    }
    rde_arr_free(&_used);
    rde_snprintf(_out, _size, "%s %u", _word, _k);
}

// Adds a note at the end of the list with the next id, expanded, in _parent when that is a folder (else at the top),
// opens that folder, and saves the index. Returns the id. There is no limit on how many notes there are: the list is
// an rde_arr on the std allocator.
u32 fude_notes_add(fude_notes* _notes, FUDE_NOTE_ _kind, u32 _parent, const c8* _name) {
    const fude_note* _folder = _parent != 0 ? fude_notes_find(_notes, _parent) : NULL;
    const u32        _into   = (_folder != NULL && _folder->kind == FUDE_NOTE_FOLDER) ? _parent : 0u;
    fude_note*       _n      = fude_notes_append(_notes, _kind, _name);
    _n->parent               = _into;
    const u32        _id     = _n->id;

    // A note added to a folder must show, so the folder opens.
    if(_into != 0) {
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
            if(fude_notes_at(_notes, _i)->id == _into) {
                fude_notes_at(_notes, _i)->expanded = true;
            }
        }
    }

    _notes->revision++;
    fude_notes_save(_notes);
    return _id;
}

// Renames a note (an empty name keeps the old one) and saves the index. False when there is no such note or the save
// failed.
b8 fude_notes_rename(fude_notes* _notes, u32 _id, const c8* _name) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->id == _id) {
            fude_notes_clean_name(_name, _n->name, _n->name);
            _notes->revision++;
            return fude_notes_save(_notes);
        }
    }
    return false;
}

// Shows or hides a folder's contents, saving the index when it changed.
void fude_notes_set_expanded(fude_notes* _notes, u32 _id, b8 _expanded) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->id == _id && _n->expanded != _expanded) {
            _n->expanded = _expanded;
            _notes->revision++;
            fude_notes_save(_notes);
        }
    }
}

// Makes canvas _id the open one, saving the index when it changed; anything else is ignored.
void fude_notes_open(fude_notes* _notes, u32 _id) {
    const fude_note* _n = fude_notes_find(_notes, _id);
    if(_n != NULL && _n->kind == FUDE_NOTE_CANVAS && _notes->open != _id) {
        _notes->open = _id;
        _notes->revision++;
        fude_notes_save(_notes);
    }
}

// Returns whether _id is _folder or inside it, at any depth.
b8 fude_notes_is_within(const fude_notes* _notes, u32 _id, u32 _folder) {
    // Walk up the parents from _id. A loop from a damaged file ends at the step limit, one step per note.
    u32 _at = _id;
    for(u32 _steps = 0; _at != 0 && _steps <= (u32)rde_arr_length(&_notes->notes); _steps++) {
        if(_at == _folder) {
            return true;
        }
        const fude_note* _n = fude_notes_find(_notes, _at);
        _at = _n != NULL ? _n->parent : 0u;
    }
    return false;
}

// Returns how many canvases are inside _folder, at any depth.
u32 fude_notes_count_in(const fude_notes* _notes, u32 _folder) {
    u32 _count = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        const fude_note* _n = fude_notes_at(_notes, _i);
        _count += _n->kind == FUDE_NOTE_CANVAS && _n->id != _folder && _n->parent != 0 && fude_notes_is_within(_notes, _n->parent, _folder) ? 1u : 0u;
    }
    return _count;
}

// Takes a note out of the list and puts it back in _parent, just before _before or at the end; opens the folder and
// saves the index. See the header.
b8 fude_notes_move(fude_notes* _notes, u32 _id, u32 _parent, u32 _before) {
    const fude_note* _target = fude_notes_find(_notes, _id);
    const fude_note* _folder = _parent != 0 ? fude_notes_find(_notes, _parent) : NULL;
    if(_target == NULL || _before == _id || (_parent != 0 && (_folder == NULL || _folder->kind != FUDE_NOTE_FOLDER))) {
        return false;
    }
    if(_target->kind == FUDE_NOTE_FOLDER && _parent != 0 && fude_notes_is_within(_notes, _parent, _id)) {
        return false;   // A folder into itself, or into something inside it.
    }

    // Take it out of the list...
    fude_note _moving = *_target;
    fude_note* _all   = (fude_note*)_notes->notes.memory;
    u32        _count = (u32)rde_arr_length(&_notes->notes);
    u32        _from  = 0;
    while(_all[_from].id != _id) {
        _from++;
    }
    rde_memmove(&_all[_from], &_all[_from + 1u], sizeof(fude_note) * (_count - _from - 1u));
    _count--;

    // ...and put it back: just before _before when that is in _parent, else at the end. The list is read per parent,
    // in order, so the end of the list is the end of the folder.
    u32              _at   = _count;
    const fude_note* _next = _before != 0 ? fude_notes_find(_notes, _before) : NULL;   // The stale copy is past _count.
    if(_next != NULL && _next->parent == _parent && (u32)(_next - _all) < _count) {
        _at = (u32)(_next - _all);
    }
    rde_memmove(&_all[_at + 1u], &_all[_at], sizeof(fude_note) * (_count - _at));
    _moving.parent = _parent;
    _all[_at]      = _moving;

    // Open the folder it landed in, so it can be seen.
    for(u32 _i = 0; _i <= _count; _i++) {
        if(_parent != 0 && _all[_i].id == _parent) {
            _all[_i].expanded = true;
        }
    }

    _notes->revision++;
    fude_notes_save(_notes);
    return true;
}

// What fude_notes_delete_files looks for in the notes folder: the folder (without its last separator), the canvas's
// prefix ("<id>."), and the paths found (each RDE_MAX_PATH bytes).
typedef struct {
    c8      dir[RDE_MAX_PATH];
    c8      prefix[16];
    rde_arr paths;
} fude_notes_owned;

// fude_notes_delete_files' crawl callback: keeps each file directly in the notes folder whose name starts with the
// canvas's prefix (a folder's files deeper down are not the canvas's). Always returns true, so the crawl goes on.
RDE_INTERNAL b8 fude_notes_found_owned(const c8* _path, b8 _is_dir, any _user) {
    fude_notes_owned* _owned = (fude_notes_owned*)_user;
    const c8*         _name  = _path;
    for(const c8* _c = _path; *_c != 0; _c++) {
        if(*_c == '/' || *_c == '\\') {
            _name = _c + 1;
        }
    }
    if(_is_dir || rde_strncmp(_name, _owned->prefix, rde_strlen(_owned->prefix)) != 0) {
        return true;
    }
    usize _dir_len = (usize)(_name - _path);
    while(_dir_len > 0 && (_path[_dir_len - 1u] == '/' || _path[_dir_len - 1u] == '\\')) {
        _dir_len--;
    }
    if(_dir_len == rde_strlen(_owned->dir) && rde_strncmp(_path, _owned->dir, _dir_len) == 0) {
        c8 _keep[RDE_MAX_PATH];
        rde_snprintf(_keep, sizeof(_keep), "%s", _path);
        rde_arr_add(&_owned->paths, _keep);
    }
    return true;
}

// Deletes every file of a canvas: each is <id>.<something> in the notes folder (its page and the page's copies, a
// deep-zoom canvas's .zoom file, its own document and the text read from it, and whatever a page kind keeps beside
// them: a canvas app's recordings, transcripts, pages and instruments). They are found by that name rather than
// listed here, so a file a page kind adds is never left behind, and this layer names none of them. The .bad copies
// go too: the canvas they were kept for is gone.
RDE_INTERNAL void fude_notes_delete_files(u32 _id) {
    fude_notes_owned _owned;
    rde_snprintf(_owned.dir, sizeof(_owned.dir), "%s", fude_notes_dir());
    usize _n = rde_strlen(_owned.dir);
    while(_n > 0 && (_owned.dir[_n - 1u] == '/' || _owned.dir[_n - 1u] == '\\')) {
        _owned.dir[--_n] = 0;
    }
    rde_snprintf(_owned.prefix, sizeof(_owned.prefix), "%u.", _id);
    _owned.paths = rde_arr_new(RDE_MAX_PATH, rde_memory_allocator_get_default_std());
    // (the paths gathered first, then deleted: the crawl is not asked to walk a folder that changes under it)
    rde_file_crawl_dir_recursively(fude_notes_dir(), fude_notes_found_owned, NULL, 0, &_owned);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_owned.paths); _i++) {
        rde_file_delete((const c8*)rde_arr_at(&_owned.paths, _i));
    }
    rde_arr_free(&_owned.paths);
}

// Sets what is under a canvas's ink, saving the index when it changed; folders are ignored.
void fude_notes_set_document(fude_notes* _notes, u32 _id, u8 _document) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->id == _id && _n->kind == FUDE_NOTE_CANVAS && _n->document != _document) {
            _n->document = _document;
            _notes->revision++;
            fude_notes_save(_notes);
        }
    }
}

// Removes a note and everything inside it, deleting each removed canvas's files, then repairs (which opens another
// canvas, or makes one, when the open one went) and saves the index.
void fude_notes_remove(fude_notes* _notes, u32 _id) {
    const fude_note* _target = fude_notes_find(_notes, _id);
    if(_target == NULL) {
        return;
    }
    // Mark the note and everything inside it, at any depth. This is decided before anything is removed, because
    // removing breaks the parent chains the check walks.
    const u32 _count = (u32)rde_arr_length(&_notes->notes);
    rde_arr   _gone  = rde_arr_new(sizeof(b8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_gone, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        ((b8*)_gone.memory)[_i] = fude_notes_is_within(_notes, fude_notes_at(_notes, _i)->id, _id);
    }
    u32 _kept = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_note _n    = *fude_notes_at(_notes, _i);
        const b8        _goes = ((const b8*)_gone.memory)[_i];
        if(_goes) {
            if(_n.kind == FUDE_NOTE_CANVAS) {
                fude_notes_delete_files(_n.id);
            }
            continue;
        }
        *fude_notes_at(_notes, _kept++) = _n;
    }
    rde_arr_resize(&_notes->notes, _kept);
    rde_arr_free(&_gone);

    fude_notes_repair(_notes);    // The open canvas may have gone: open another, or a new one.
    _notes->revision++;
    fude_notes_save(_notes);
}

// --- the tree -----------------------------------------------------------------------------

// Returns how many canvases are inside note _i (the top level: the list's length), at any depth, and keeps the count of
// every note under it in _tree->canvases. Each note has one folder, so a loop from a damaged list hangs from nothing
// and is never reached from the top: the walk always ends.
RDE_INTERNAL u32 fude_notes_tree_count(fude_notes_tree* _tree, const fude_note* _all, u32 _i) {
    const u32* _first  = (const u32*)_tree->first.memory;
    const u32* _end    = (const u32*)_tree->end.memory;
    const u32* _child  = (const u32*)_tree->child.memory;
    u32        _inside = 0;
    for(u32 _c = _first[_i]; _c < _end[_i]; _c++) {
        const u32 _k = _child[_c];
        _inside += (_all[_k].kind == FUDE_NOTE_CANVAS ? 1u : 0u) + fude_notes_tree_count(_tree, _all, _k);
    }
    ((u32*)_tree->canvases.memory)[_i] = _inside;
    return _inside;
}

// Fills _tree from the list in one pass (see the header): a map from id to index gives each note's folder, a counting
// sort groups the notes by folder in list order, and one walk down from the top counts each folder's canvases.
void fude_notes_tree_make(const fude_notes* _notes, fude_notes_tree* _tree) {
    rde_memory_allocator* _heap  = rde_memory_allocator_get_default_std();
    const fude_note*      _all   = (const fude_note*)_notes->notes.memory;
    const u32             _count = (u32)rde_arr_length(&_notes->notes);   // Also the top level's index.

    // Each note's index by its id: the first note with that id, as fude_notes_find finds it.
    rde_hash_map TYPE(u32, u32) _index = rde_hash_map_new(sizeof(u32), sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL, _heap);
    rde_hash_map_reserve(&_index, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        rde_hash_map_add(&_index, (any)&_all[_i].id, &_i);
    }

    // Each note's folder as an index: the top level for a note at the top, or in a folder that is not in the list.
    rde_arr TYPE(u32) _parents = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_parents, _count);
    u32* _parent = (u32*)_parents.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32* _at = _all[_i].parent != 0u ? (const u32*)rde_hash_map_find(&_index, (any)&_all[_i].parent) : NULL;
        _parent[_i] = _at != NULL ? *_at : _count;
    }
    rde_hash_map_free(&_index);

    // Group the notes by folder, in list order: count each folder's notes, set where each group starts, then put each
    // note at its group's end.
    _tree->first    = rde_arr_new(sizeof(u32), _heap);
    _tree->end      = rde_arr_new(sizeof(u32), _heap);
    _tree->child    = rde_arr_new(sizeof(u32), _heap);
    _tree->canvases = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_tree->first, _count + 1u);
    rde_arr_resize(&_tree->end, _count + 1u);
    rde_arr_resize(&_tree->child, _count);
    rde_arr_resize(&_tree->canvases, _count + 1u);
    u32* _first = (u32*)_tree->first.memory;
    u32* _end   = (u32*)_tree->end.memory;
    u32* _child = (u32*)_tree->child.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        _end[_parent[_i]]++;   // For now, how many notes each folder has.
    }
    for(u32 _p = 0, _at = 0; _p <= _count; _p++) {
        _first[_p] = _at;
        _at       += _end[_p];
        _end[_p]   = _first[_p];
    }
    for(u32 _i = 0; _i < _count; _i++) {
        _child[_end[_parent[_i]]++] = _i;
    }
    rde_arr_free(&_parents);

    fude_notes_tree_count(_tree, _all, _count);
}

// Frees the tree's arrays and zeroes it. A tree never made (zeroed) is left as it is.
void fude_notes_tree_free(fude_notes_tree* _tree) {
    rde_arr* _arrays[] = { &_tree->first, &_tree->end, &_tree->child, &_tree->canvases };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_memset(_tree, 0, sizeof(*_tree));
}
