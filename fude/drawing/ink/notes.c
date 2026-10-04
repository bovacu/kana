// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/notes.h"
#include "drawing/base/text.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See notes.h.
// ===========================================================================

#define FUDE_NOTES_KIND        FUDE_TAG('N', 'O', 'T', 'E')
#define FUDE_NOTES_CHUNK_INDEX FUDE_TAG('N', 'I', 'D', 'X')
#define FUDE_NOTES_RECORD_SIZE 20u    // id, parent, kind, expanded, document, 1 reserved, created lo/hi
#define FUDE_NOTES_MAX         4096u  // more is a damaged file, not a notebook

// Under the save folder — found again should that change (the app's own is set
// before the saves are read: fude_app_start).
RDE_INTERNAL const c8* fude_notes_dir(void) {
    static c8 _dir[RDE_MAX_PATH] = { 0 };
    static c8 _in[RDE_MAX_PATH]  = { 0 };
    if(_dir[0] == 0 || strcmp(_in, fude_save_dir()) != 0) {
        snprintf(_in, sizeof(_in), "%s", fude_save_dir());
        snprintf(_dir, sizeof(_dir), "%snotes/", _in);
        c8 _probe[RDE_MAX_PATH];
        snprintf(_probe, sizeof(_probe), "%sx.kana", _dir);
        rde_file_create_missing_dirs(_probe);
    }
    return _dir;
}

RDE_INTERNAL void fude_notes_index_path(c8* _out, usize _size) {
    snprintf(_out, _size, "%sindex.kana", fude_notes_dir());
}

void fude_notes_canvas_path(u32 _id, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%u.kana", fude_notes_dir(), _id);
}

void fude_notes_document_path(u32 _id, c8* _out, usize _size) {
    snprintf(_out, _size, "%s%u.pdf", fude_notes_dir(), _id);
}

void fude_notes_init(fude_notes* _notes) {
    memset(_notes, 0, sizeof(*_notes));
    _notes->notes   = rde_arr_new(sizeof(fude_note), rde_memory_allocator_get_default_std());
    _notes->next_id = 1;
}

void fude_notes_destroy(fude_notes* _notes) {
    if(rde_arr_is_inited(&_notes->notes)) {
        rde_arr_free(&_notes->notes);
    }
    memset(_notes, 0, sizeof(*_notes));
}

RDE_INTERNAL fude_note* fude_notes_at(const fude_notes* _notes, u32 _i) {
    return &((fude_note*)_notes->notes.memory)[_i];
}

const fude_note* fude_notes_find(const fude_notes* _notes, u32 _id) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        if(fude_notes_at(_notes, _i)->id == _id) {
            return fude_notes_at(_notes, _i);
        }
    }
    return NULL;
}

// A name as stored: trimmed of spaces at both ends, cut to fit (on a UTF-8
// boundary), never empty.
RDE_INTERNAL void fude_notes_clean_name(const c8* _in, c8* _out, const c8* _fallback) {
    while(_in != NULL && (*_in == ' ' || *_in == '\t' || *_in == '\n')) {
        _in++;
    }
    usize _n = _in != NULL ? strlen(_in) : 0;
    while(_n > 0 && (_in[_n - 1] == ' ' || _in[_n - 1] == '\t' || _in[_n - 1] == '\n')) {
        _n--;
    }
    if(_n > FUDE_NOTE_NAME - 1) {
        _n = FUDE_NOTE_NAME - 1;
        while(_n > 0 && ((u8)_in[_n] & 0xC0u) == 0x80u) {   // not inside a character
            _n--;
        }
    }
    if(_n == 0) {
        snprintf(_out, FUDE_NOTE_NAME, "%s", _fallback);
        return;
    }
    memcpy(_out, _in, _n);
    _out[_n] = 0;
}

// --- the index file -------------------------------------------------------------------

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
        fude_put_u32(&_b, (u32)(_n->created & 0xFFFFFFFFu));
        fude_put_u32(&_b, (u32)(_n->created >> 32));
        const u32 _len = (u32)strlen(_n->name);
        fude_put_u8(&_b, (u8)_len);
        fude_put_data(&_b, (const u8*)_n->name, _len);
    }
    fude_chunk_end(&_b, _chunk);

    return fude_bytes_write_and_free(&_b, _path, NULL);
}

// Reads the index into _notes; false when there is none (or it is unreadable).
RDE_INTERNAL b8 fude_notes_read(fude_notes* _notes) {
    c8 _path[RDE_MAX_PATH];
    fude_notes_index_path(_path, sizeof(_path));
    if(!rde_file_exists(_path)) {
        snprintf(_path + strlen(_path), sizeof(_path) - strlen(_path), ".bak");
        if(!rde_file_exists(_path)) {
            return false;
        }
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    fude_reader _r = fude_reader_make(_data, _size);
    if(_data == NULL || !fude_read_header(&_r, FUDE_NOTES_VERSION, FUDE_NOTES_KIND)) {
        fude_file_free(_data);
        fude_file_set_aside(_path);
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
        if(!_c.ok || _count > FUDE_NOTES_MAX || _rec < FUDE_NOTES_RECORD_SIZE) {
            break;
        }

        rde_arr_clear(&_notes->notes);
        for(u32 _i = 0; _i < _count && _c.ok; _i++) {
            const u32 _start = _c.pos;
            fude_note _n;
            memset(&_n, 0, sizeof(_n));
            _n.id       = fude_get_u32(&_c);
            _n.parent   = fude_get_u32(&_c);
            _n.kind     = fude_get_u8(&_c);
            _n.expanded = fude_get_u8(&_c) != 0;
            _n.document = fude_get_u8(&_c);   // 0 in a file from before documents
            fude_get_u8(&_c);
            const u64 _lo = fude_get_u32(&_c);
            const u64 _hi = fude_get_u32(&_c);
            _n.created = _lo | (_hi << 32);
            if(!fude_reader_has(&_c, _start + _rec - _c.pos)) {
                break;
            }
            _c.pos = _start + _rec;   // a newer build's longer record

            const u32 _len = fude_get_u8(&_c);
            if(!fude_reader_has(&_c, _len)) {
                break;
            }
            c8 _raw[256];
            memcpy(_raw, &_c.data[_c.pos], _len);
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

    fude_file_free(_data);
    return _found;
}

// Every id below next_id, every parent a folder that exists, the open one a
// canvas — whatever the file said.
RDE_INTERNAL void fude_notes_repair(fude_notes* _notes) {
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->id >= _notes->next_id) {
            _notes->next_id = _n->id + 1u;
        }
        const fude_note* _parent = _n->parent != 0 ? fude_notes_find(_notes, _n->parent) : NULL;
        if(_n->parent != 0 && (_parent == NULL || _parent->kind != FUDE_NOTE_FOLDER)) {
            _n->parent = 0;   // an orphan goes to the top
        }
    }
    // A folder inside itself (a damaged file): it goes to the top, which breaks the loop.
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        fude_note* _n = fude_notes_at(_notes, _i);
        if(_n->kind == FUDE_NOTE_FOLDER && _n->parent != 0 && fude_notes_is_within(_notes, _n->parent, _n->id)) {
            _n->parent = 0;
        }
    }

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

void fude_notes_load(fude_notes* _notes) {
    if(!fude_notes_read(_notes)) {
        // The first time: the page from before notes (page.kana) becomes the first
        // canvas, moved with its backup.
        rde_arr_clear(&_notes->notes);
        _notes->next_id = 1;
        _notes->open    = 0;

        c8 _old[RDE_MAX_PATH];
        snprintf(_old, sizeof(_old), "%s%s", fude_save_dir(), FUDE_SAVE_DOCUMENT_FILE);
        c8 _old_bak[RDE_MAX_PATH];
        snprintf(_old_bak, sizeof(_old_bak), "%s.bak", _old);
        if(rde_file_exists(_old) || rde_file_exists(_old_bak)) {
            const u32 _id = fude_notes_add(_notes, FUDE_NOTE_CANVAS, 0, fude_text(FUDE_TEXT_NOTE_PAGE));
            c8 _new[RDE_MAX_PATH];
            fude_notes_canvas_path(_id, _new, sizeof(_new));
            if(rde_file_exists(_old)) {
                rde_file_move(_old, _new);
            }
            if(rde_file_exists(_old_bak)) {
                c8 _new_bak[RDE_MAX_PATH];
                snprintf(_new_bak, sizeof(_new_bak), "%s.bak", _new);
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

void fude_notes_new_name(const fude_notes* _notes, FUDE_NOTE_ _kind, c8* _out, usize _size) {
    const c8* _word = fude_text(_kind == FUDE_NOTE_FOLDER ? FUDE_TEXT_FOLDER : FUDE_TEXT_CANVAS);
    for(u32 _k = 1; _k < 100000u; _k++) {
        snprintf(_out, _size, "%s %u", _word, _k);
        b8 _taken = false;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes) && !_taken; _i++) {
            _taken = strcmp(fude_notes_at(_notes, _i)->name, _out) == 0;
        }
        if(!_taken) {
            return;
        }
    }
}

u32 fude_notes_add(fude_notes* _notes, FUDE_NOTE_ _kind, u32 _parent, const c8* _name) {
    if(rde_arr_length(&_notes->notes) >= FUDE_NOTES_MAX) {
        return 0;
    }
    const fude_note* _folder = _parent != 0 ? fude_notes_find(_notes, _parent) : NULL;

    fude_note _n;
    memset(&_n, 0, sizeof(_n));
    _n.id       = _notes->next_id++;
    _n.parent   = (_folder != NULL && _folder->kind == FUDE_NOTE_FOLDER) ? _parent : 0u;
    _n.kind     = (u8)_kind;
    _n.expanded = true;
    _n.created  = (u64)time(NULL);
    fude_notes_clean_name(_name, _n.name, fude_text(_kind == FUDE_NOTE_FOLDER ? FUDE_TEXT_FOLDER : FUDE_TEXT_CANVAS));
    rde_arr_add(&_notes->notes, &_n);

    // A canvas added to a folder shows: the folder opens.
    if(_n.parent != 0) {
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
            if(fude_notes_at(_notes, _i)->id == _n.parent) {
                fude_notes_at(_notes, _i)->expanded = true;
            }
        }
    }

    _notes->revision++;
    fude_notes_save(_notes);
    return _n.id;
}

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

void fude_notes_open(fude_notes* _notes, u32 _id) {
    const fude_note* _n = fude_notes_find(_notes, _id);
    if(_n != NULL && _n->kind == FUDE_NOTE_CANVAS && _notes->open != _id) {
        _notes->open = _id;
        _notes->revision++;
        fude_notes_save(_notes);
    }
}

b8 fude_notes_is_within(const fude_notes* _notes, u32 _id, u32 _folder) {
    // Up the parents from _id; a damaged file's loop ends at the step limit.
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

u32 fude_notes_count_in(const fude_notes* _notes, u32 _folder) {
    u32 _count = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_notes->notes); _i++) {
        const fude_note* _n = fude_notes_at(_notes, _i);
        _count += _n->kind == FUDE_NOTE_CANVAS && _n->id != _folder && _n->parent != 0 && fude_notes_is_within(_notes, _n->parent, _folder) ? 1u : 0u;
    }
    return _count;
}

b8 fude_notes_move(fude_notes* _notes, u32 _id, u32 _parent, u32 _before) {
    const fude_note* _target = fude_notes_find(_notes, _id);
    const fude_note* _folder = _parent != 0 ? fude_notes_find(_notes, _parent) : NULL;
    if(_target == NULL || _before == _id || (_parent != 0 && (_folder == NULL || _folder->kind != FUDE_NOTE_FOLDER))) {
        return false;
    }
    if(_target->kind == FUDE_NOTE_FOLDER && _parent != 0 && fude_notes_is_within(_notes, _parent, _id)) {
        return false;   // a folder into itself, or into something inside it
    }

    // Out of the list...
    fude_note _moving = *_target;
    fude_note* _all   = (fude_note*)_notes->notes.memory;
    u32        _count = (u32)rde_arr_length(&_notes->notes);
    u32        _from  = 0;
    while(_all[_from].id != _id) {
        _from++;
    }
    memmove(&_all[_from], &_all[_from + 1u], sizeof(fude_note) * (_count - _from - 1u));
    _count--;

    // ...and back in: just before _before if it is in _parent, else at the end —
    // the list is read per parent, in order, so the end of the list is the end
    // of the folder.
    u32              _at   = _count;
    const fude_note* _next = _before != 0 ? fude_notes_find(_notes, _before) : NULL;   // (the stale last slot is past _count)
    if(_next != NULL && _next->parent == _parent && (u32)(_next - _all) < _count) {
        _at = (u32)(_next - _all);
    }
    memmove(&_all[_at + 1u], &_all[_at], sizeof(fude_note) * (_count - _at));
    _moving.parent = _parent;
    _all[_at]      = _moving;

    // Somewhere to see it land: its folder opens.
    for(u32 _i = 0; _i <= _count; _i++) {
        if(_parent != 0 && _all[_i].id == _parent) {
            _all[_i].expanded = true;
        }
    }

    _notes->revision++;
    fude_notes_save(_notes);
    return true;
}

RDE_INTERNAL void fude_notes_delete_files(u32 _id) {
    c8 _path[RDE_MAX_PATH];
    fude_notes_canvas_path(_id, _path, sizeof(_path));
    const c8* const _suffixes[] = { "", ".bak", ".tmp" };
    for(u32 _i = 0; _i < sizeof(_suffixes) / sizeof(_suffixes[0]); _i++) {
        c8 _file[RDE_MAX_PATH];
        snprintf(_file, sizeof(_file), "%s%s", _path, _suffixes[_i]);
        if(rde_file_exists(_file)) {
            rde_file_delete(_file);
        }
    }
    // A deep-zoom canvas's file (Sketching's, zoom/zfile.h): <id>.zoom beside the page's.
    fude_notes_canvas_path(_id, _path, sizeof(_path));
    c8* _dot = strrchr(_path, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    const c8* const _zoom[] = { ".zoom", ".zoom.bak", ".zoom.tmp", ".zoom.bad" };
    for(u32 _i = 0; _i < sizeof(_zoom) / sizeof(_zoom[0]); _i++) {
        c8 _file[RDE_MAX_PATH];
        snprintf(_file, sizeof(_file), "%s%s", _path, _zoom[_i]);
        if(rde_file_exists(_file)) {
            rde_file_delete(_file);
        }
    }
    fude_notes_document_path(_id, _path, sizeof(_path));   // its own document, if it had one
    if(rde_file_exists(_path)) {
        rde_file_delete(_path);
    }
    const usize _n = strlen(_path);
    if(_n > 4u) {
        snprintf(&_path[_n - 4u], sizeof(_path) - (_n - 4u), ".lines");   // its pictures' text, read (doc.h)
        if(rde_file_exists(_path)) {
            rde_file_delete(_path);
        }
    }
}

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

void fude_notes_remove(fude_notes* _notes, u32 _id) {
    const fude_note* _target = fude_notes_find(_notes, _id);
    if(_target == NULL) {
        return;
    }
    // The note, and everything inside a folder, at any depth. (Decided before
    // anything is removed: removing breaks the parent chains it walks.)
    const u32 _count = (u32)rde_arr_length(&_notes->notes);
    b8*       _gone  = (b8*)calloc(_count > 0 ? _count : 1u, sizeof(b8));
    for(u32 _i = 0; _i < _count; _i++) {
        _gone[_i] = fude_notes_is_within(_notes, fude_notes_at(_notes, _i)->id, _id);
    }
    u32 _kept = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_note _n    = *fude_notes_at(_notes, _i);
        const b8        _goes = _gone[_i];
        if(_goes) {
            if(_n.kind == FUDE_NOTE_CANVAS) {
                fude_notes_delete_files(_n.id);
            }
            continue;
        }
        *fude_notes_at(_notes, _kept++) = _n;
    }
    _notes->notes.count = _kept;   // rde_arr has no truncate: rde_arr_clear's operation, to a length
    free(_gone);

    fude_notes_repair(_notes);    // the open canvas may have gone: another, or a new one
    _notes->revision++;
    fude_notes_save(_notes);
}
