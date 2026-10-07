// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/backup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See backup.h.
// ===========================================================================

#define FUDE_BACKUP_MAGIC    "KANABKP"   // 8 bytes with its NUL
#define FUDE_BACKUP_VERSION  1u
#define FUDE_BACKUP_HEAD     (8u + 4u + 4u + 8u + 16u)
#define FUDE_BACKUP_PATH_MAX 255u
#define FUDE_BACKUP_READ     (64u * 1024u)   // a file is read this much at a time
#define FUDE_BACKUP_FILE_MAX 0x7FFFFFFFu     // a file's bytes, at most (an rde_arr's)

// FNV-1a, 32 bits, carried from one piece to the next.
RDE_INTERNAL u32 fude_backup_fnv(u32 _hash, const u8* _p, usize _n) {
    for(usize _i = 0; _i < _n; _i++) {
        _hash = (_hash ^ _p[_i]) * 16777619u;
    }
    return _hash;
}

RDE_INTERNAL u32 fude_backup_u32(const u8* _p) {
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

RDE_INTERNAL void fude_backup_put_u32(u8* _p, u32 _v) {
    _p[0] = (u8)_v; _p[1] = (u8)(_v >> 8); _p[2] = (u8)(_v >> 16); _p[3] = (u8)(_v >> 24);
}

// --- the files of a folder ------------------------------------------------------------------

// Paths relative to a folder ('/' between its folders).
RDE_STRUCT {
    rde_arr TYPE(c8[RDE_MAX_PATH]) paths;
    const c8* root;
    usize     root_len;
    b8        everything;   // false: only what a backup keeps
} fude_backup_list;

RDE_INTERNAL fude_backup_list fude_backup_list_new(const c8* _root, b8 _everything) {
    fude_backup_list _list = { rde_arr_new(RDE_MAX_PATH, rde_memory_allocator_get_default_std()), _root, strlen(_root), _everything };
    return _list;
}

RDE_INTERNAL u32 fude_backup_list_count(const fude_backup_list* _list) {
    return (u32)rde_arr_length(&_list->paths);
}

RDE_INTERNAL const c8* fude_backup_list_at(const fude_backup_list* _list, u32 _i) {
    return (const c8*)_list->paths.memory + (usize)_i * RDE_MAX_PATH;
}

RDE_INTERNAL void fude_backup_list_add(fude_backup_list* _list, const c8* _path) {
    snprintf((c8*)rde_arr_add_n(&_list->paths, 1u), RDE_MAX_PATH, "%s", _path);
}

RDE_INTERNAL b8 fude_backup_ends_with(const c8* _s, const c8* _end) {
    const usize _n = strlen(_s), _m = strlen(_end);
    return _n >= _m && strcmp(_s + _n - _m, _end) == 0;
}

RDE_INTERNAL b8 fude_backup_starts_with(const c8* _s, const c8* _start) {
    return strncmp(_s, _start, strlen(_start)) == 0;
}

RDE_INTERNAL b8 fude_backup_collect_entry(const c8* _path, b8 _is_dir, any _user_data) {
    fude_backup_list* _list = (fude_backup_list*)_user_data;
    if(_is_dir) {
        return true;
    }
    // Relative to the folder, with '/' only.
    const c8* _rel = fude_backup_starts_with(_path, _list->root) ? _path + _list->root_len : _path;
    while(*_rel == '/' || *_rel == '\\') {
        _rel++;
    }
    c8 _clean[RDE_MAX_PATH];
    snprintf(_clean, sizeof(_clean), "%s", _rel);
    for(c8* _c = _clean; *_c != 0; _c++) {
        if(*_c == '\\') {
            *_c = '/';
        }
    }
    const c8* _name = strrchr(_clean, '/') != NULL ? strrchr(_clean, '/') + 1 : _clean;
    if(_clean[0] == 0 || fude_backup_starts_with(_clean, FUDE_BACKUP_ASIDE "/") || fude_backup_starts_with(_clean, FUDE_BACKUP_OUTBOX "/")) {
        return true;
    }
    if(!_list->everything) {
        if(_name[0] == '.' || fude_backup_ends_with(_name, ".bak") || fude_backup_ends_with(_name, ".tmp") || fude_backup_ends_with(_name, ".bad") ||
           strcmp(_name, "perf.txt") == 0 || strcmp(_name, "translate.txt") == 0 || strcmp(_name, "mlkit_samples.txt") == 0) {
            return true;
        }
    }
    fude_backup_list_add(_list, _clean);
    return true;
}

// The files under _dir (none when it is not there). In the save folder
// (_save_folder), before-import/ and outbox/ are not gone into.
RDE_INTERNAL fude_backup_list fude_backup_collect(const c8* _dir, b8 _everything, b8 _save_folder) {
    fude_backup_list _list = fude_backup_list_new(_dir, _everything);
    if(rde_file_dir_exists(_dir)) {
        c8* _skip[2] = { (c8*)FUDE_BACKUP_ASIDE, (c8*)FUDE_BACKUP_OUTBOX };
        rde_file_crawl_dir_recursively(_dir, fude_backup_collect_entry, _save_folder ? _skip : NULL, _save_folder ? 2u : 0u, &_list);
    }
    return _list;
}

RDE_INTERNAL void fude_backup_list_free(fude_backup_list* _list) {
    rde_arr_free(&_list->paths);
}

// --- export ------------------------------------------------------------------------------

// Writes _n bytes to _f, adding them to the checksum.
RDE_INTERNAL b8 fude_backup_write(FILE* _f, u32* _hash, const any _p, usize _n) {
    *_hash = fude_backup_fnv(*_hash, (const u8*)_p, _n);
    return _n == 0 || fwrite(_p, 1, _n, _f) == _n;
}

// _path's bytes into _data (emptied first; an empty file is a file). False: it
// could not be read (or it is past FUDE_BACKUP_FILE_MAX).
RDE_INTERNAL b8 fude_backup_read_file(const c8* _path, rde_arr* _data) {
    rde_arr_clear(_data);
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return false;
    }
    b8 _bad = false;
    for(;;) {
        const usize _had = rde_arr_length(_data);
        if(_had > FUDE_BACKUP_FILE_MAX - FUDE_BACKUP_READ) {
            _bad = true;
            break;
        }
        const usize _got = fread(rde_arr_add_n(_data, FUDE_BACKUP_READ), 1, FUDE_BACKUP_READ, _f);
        rde_arr_resize(_data, _had + _got);
        if(_got == 0) {
            break;
        }
    }
    _bad = _bad || ferror(_f) != 0;
    fclose(_f);
    return !_bad;
}

RDE_INTERNAL b8 fude_backup_is_canvas(const c8* _rel) {
    return fude_backup_starts_with(_rel, "notes/") && fude_backup_ends_with(_rel, ".kana") && strcmp(_rel, "notes/index.kana") != 0;
}

b8 fude_backup_export(const c8* _dir, const c8* _out, const c8* _version, fude_backup_info* _info) {
    fude_backup_list _list = fude_backup_collect(_dir, false, true);
    FILE*            _f    = fopen(_out, "wb");
    if(_f == NULL) {
        fude_backup_list_free(&_list);
        return false;
    }
    fude_backup_info _made;
    memset(&_made, 0, sizeof(_made));
    _made.files   = fude_backup_list_count(&_list);
    _made.created = (u64)time(NULL);
    snprintf(_made.version, sizeof(_made.version), "%s", _version != NULL ? _version : "");

    u32 _hash = 2166136261u;
    u8  _head[FUDE_BACKUP_HEAD];
    memset(_head, 0, sizeof(_head));
    memcpy(_head, FUDE_BACKUP_MAGIC, 8);
    fude_backup_put_u32(_head + 8, FUDE_BACKUP_VERSION);
    fude_backup_put_u32(_head + 12, _made.files);
    fude_backup_put_u32(_head + 16, (u32)(_made.created & 0xFFFFFFFFu));
    fude_backup_put_u32(_head + 20, (u32)(_made.created >> 32));
    memcpy(_head + 24, _made.version, strlen(_made.version) < 16 ? strlen(_made.version) : 15);
    b8 _ok = fude_backup_write(_f, &_hash, _head, sizeof(_head));

    rde_arr TYPE(u8) _data = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // each file's bytes in turn
    for(u32 _i = 0; _ok && _i < _made.files; _i++) {
        const c8*   _rel = fude_backup_list_at(&_list, _i);
        const usize _len = strlen(_rel);
        c8          _full[RDE_MAX_PATH];
        snprintf(_full, sizeof(_full), "%s%s", _dir, _rel);
        if(_len > FUDE_BACKUP_PATH_MAX || !fude_backup_read_file(_full, &_data)) {
            _ok = false;
            break;
        }
        const usize _size = rde_arr_length(&_data);
        u8 _n16[2] = { (u8)_len, (u8)(_len >> 8) };
        u8 _n32[4];
        fude_backup_put_u32(_n32, (u32)_size);
        _ok = fude_backup_write(_f, &_hash, _n16, 2) && fude_backup_write(_f, &_hash, _rel, _len) &&
              fude_backup_write(_f, &_hash, _n32, 4) && fude_backup_write(_f, &_hash, _data.memory, _size);
        _made.bytes    += _size;
        _made.canvases += fude_backup_is_canvas(_rel) ? 1u : 0u;
    }
    if(_ok) {
        u8 _sum[4];
        fude_backup_put_u32(_sum, _hash);
        _ok = fwrite(_sum, 1, 4, _f) == 4;
    }
    _ok = fclose(_f) == 0 && _ok;
    rde_arr_free(&_data);
    fude_backup_list_free(&_list);
    if(!_ok) {
        remove(_out);
        return false;
    }
    if(_info != NULL) {
        *_info = _made;
    }
    return true;
}

// --- inspect --------------------------------------------------------------------------------

// A path a backup may write: relative, inside the save folder, plainly spelt.
RDE_INTERNAL b8 fude_backup_safe_path(const c8* _p, usize _n) {
    if(_n == 0 || _n > FUDE_BACKUP_PATH_MAX || _p[0] == '/' || _p[_n - 1] == '/') {
        return false;
    }
    usize _seg = 0;   // where the current folder or name began
    for(usize _i = 0; _i <= _n; _i++) {
        const c8 _c = _i < _n ? _p[_i] : '/';
        if(_i < _n && (_c == 0 || _c == '\\' || _c == ':')) {
            return false;
        }
        if(_c == '/') {
            const usize _len = _i - _seg;
            if(_len == 0 || (_len == 1 && _p[_seg] == '.') || (_len == 2 && _p[_seg] == '.' && _p[_seg + 1] == '.')) {
                return false;
            }
            _seg = _i + 1;
        }
    }
    const usize _aside = strlen(FUDE_BACKUP_ASIDE), _outbox = strlen(FUDE_BACKUP_OUTBOX);
    return !(_n > _aside && strncmp(_p, FUDE_BACKUP_ASIDE "/", _aside + 1) == 0) && !(_n > _outbox && strncmp(_p, FUDE_BACKUP_OUTBOX "/", _outbox + 1) == 0);
}

// Walks a backup's files: _each (may be NULL) gets each one; false when the
// layout does not hold together.
typedef b8 (*fude_backup_each_fn)(const c8* _path, const u8* _data, u32 _size, any _user_data);

RDE_INTERNAL b8 fude_backup_walk(const u8* _data, usize _size, fude_backup_info* _info, fude_backup_each_fn _each, any _user_data) {
    if(_data == NULL || _size < FUDE_BACKUP_HEAD + 4u || memcmp(_data, FUDE_BACKUP_MAGIC, 8) != 0 || fude_backup_u32(_data + 8) != FUDE_BACKUP_VERSION) {
        return false;
    }
    if(fude_backup_fnv(2166136261u, _data, _size - 4u) != fude_backup_u32(_data + _size - 4u)) {
        return false;   // damaged, or cut short
    }
    fude_backup_info _found;
    memset(&_found, 0, sizeof(_found));
    _found.files   = fude_backup_u32(_data + 12);
    _found.created = (u64)fude_backup_u32(_data + 16) | ((u64)fude_backup_u32(_data + 20) << 32);
    memcpy(_found.version, _data + 24, 15);

    const usize _end = _size - 4u;
    usize       _at  = FUDE_BACKUP_HEAD;
    for(u32 _i = 0; _i < _found.files; _i++) {
        if(_end - _at < 2u) {
            return false;
        }
        const usize _len = (usize)_data[_at] | ((usize)_data[_at + 1] << 8);
        _at += 2u;
        if(_end - _at < _len + 4u || !fude_backup_safe_path((const c8*)_data + _at, _len)) {
            return false;
        }
        c8 _path[FUDE_BACKUP_PATH_MAX + 1u];
        memcpy(_path, _data + _at, _len);
        _path[_len] = 0;
        _at += _len;
        const u32 _bytes = fude_backup_u32(_data + _at);
        _at += 4u;
        if(_end - _at < _bytes) {
            return false;
        }
        if(_each != NULL && !_each(_path, _data + _at, _bytes, _user_data)) {
            return false;
        }
        _at             += _bytes;
        _found.bytes    += _bytes;
        _found.canvases += fude_backup_is_canvas(_path) ? 1u : 0u;
    }
    if(_at != _end) {
        return false;
    }
    if(_info != NULL) {
        *_info = _found;
    }
    return true;
}

b8 fude_backup_inspect(const u8* _data, usize _size, fude_backup_info* _info) {
    return fude_backup_walk(_data, _size, _info, NULL, NULL);
}

// --- restore --------------------------------------------------------------------------------

RDE_STRUCT {
    const c8*         dir;
    fude_backup_list* written;   // what was written, to take back on a failure
} fude_backup_restoring;

RDE_INTERNAL b8 fude_backup_restore_file(const c8* _path, const u8* _data, u32 _size, any _user_data) {
    fude_backup_restoring* _r = (fude_backup_restoring*)_user_data;
    c8 _full[RDE_MAX_PATH];
    snprintf(_full, sizeof(_full), "%s%s", _r->dir, _path);
    if(!rde_file_create_missing_dirs(_full)) {
        return false;
    }
    FILE* _f = fopen(_full, "wb");
    if(_f == NULL) {
        return false;
    }
    const b8 _ok = (_size == 0 || fwrite(_data, 1, _size, _f) == _size);
    const b8 _closed = fclose(_f) == 0;
    // Counted even when it failed: a part-written file is taken back too.
    fude_backup_list_add(_r->written, _path);
    return _ok && _closed;
}

// Moves _list's first _count files from _from to _to (both folders, ending in '/'): how many moved.
RDE_INTERNAL u32 fude_backup_move_all(const fude_backup_list* _list, u32 _count, const c8* _from, const c8* _to) {
    for(u32 _i = 0; _i < _count; _i++) {
        c8 _src[RDE_MAX_PATH], _dst[RDE_MAX_PATH];
        snprintf(_src, sizeof(_src), "%s%s", _from, fude_backup_list_at(_list, _i));
        snprintf(_dst, sizeof(_dst), "%s%s", _to, fude_backup_list_at(_list, _i));
        if(!rde_file_create_missing_dirs(_dst) || !rde_file_move(_src, _dst)) {
            return _i;
        }
    }
    return _count;
}

b8 fude_backup_restore(const u8* _data, usize _size, const c8* _dir) {
    if(!fude_backup_inspect(_data, _size, NULL)) {
        return false;
    }
    c8 _aside[RDE_MAX_PATH];
    snprintf(_aside, sizeof(_aside), "%s%s/", _dir, FUDE_BACKUP_ASIDE);

    // The last import's leftovers go; what is here now goes aside.
    fude_backup_list _old = fude_backup_collect(_aside, true, false);
    for(u32 _i = 0; _i < fude_backup_list_count(&_old); _i++) {
        c8 _p[RDE_MAX_PATH];
        snprintf(_p, sizeof(_p), "%s%s", _aside, fude_backup_list_at(&_old, _i));
        rde_file_delete(_p);
    }
    fude_backup_list_free(&_old);

    fude_backup_list _here  = fude_backup_collect(_dir, true, true);
    const u32        _moved = fude_backup_move_all(&_here, fude_backup_list_count(&_here), _dir, _aside);
    b8               _ok    = _moved == fude_backup_list_count(&_here);

    // The backup's files.
    fude_backup_list      _written = fude_backup_list_new(_dir, true);
    fude_backup_restoring _r       = { _dir, &_written };
    if(_ok) {
        _ok = fude_backup_walk(_data, _size, NULL, fude_backup_restore_file, &_r);
    }
    if(!_ok) {
        // Back as it was: what was written goes, what was moved comes back.
        for(u32 _i = 0; _i < fude_backup_list_count(&_written); _i++) {
            c8 _p[RDE_MAX_PATH];
            snprintf(_p, sizeof(_p), "%s%s", _dir, fude_backup_list_at(&_written, _i));
            rde_file_delete(_p);
        }
        fude_backup_move_all(&_here, _moved, _aside, _dir);
    }
    fude_backup_list_free(&_written);
    fude_backup_list_free(&_here);
    return _ok;
}
