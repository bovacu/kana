#include "base/backup.h"
#include "app/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See backup.h.
// ===========================================================================

#define KANA_BACKUP_MAGIC    "KANABKP"   // 8 bytes with its NUL
#define KANA_BACKUP_VERSION  1u
#define KANA_BACKUP_HEAD     (8u + 4u + 4u + 8u + 16u)
#define KANA_BACKUP_PATH_MAX 255u

// FNV-1a, 32 bits, carried from one piece to the next.
RDE_INTERNAL u32 kana_backup_fnv(u32 _hash, const u8* _p, usize _n) {
    for(usize _i = 0; _i < _n; _i++) {
        _hash = (_hash ^ _p[_i]) * 16777619u;
    }
    return _hash;
}

RDE_INTERNAL u32 kana_backup_u32(const u8* _p) {
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

RDE_INTERNAL void kana_backup_put_u32(u8* _p, u32 _v) {
    _p[0] = (u8)_v; _p[1] = (u8)(_v >> 8); _p[2] = (u8)(_v >> 16); _p[3] = (u8)(_v >> 24);
}

// --- the files of a folder ------------------------------------------------------------------

// Paths relative to a folder ('/' between its folders).
RDE_STRUCT {
    c8 (*paths)[RDE_MAX_PATH];
    u32       count;
    u32       capacity;
    const c8* root;
    usize     root_len;
    b8        everything;   // false: only what a backup keeps
    b8        ok;
} kana_backup_list;

RDE_INTERNAL b8 kana_backup_ends_with(const c8* _s, const c8* _end) {
    const usize _n = strlen(_s), _m = strlen(_end);
    return _n >= _m && strcmp(_s + _n - _m, _end) == 0;
}

RDE_INTERNAL b8 kana_backup_starts_with(const c8* _s, const c8* _start) {
    return strncmp(_s, _start, strlen(_start)) == 0;
}

RDE_INTERNAL b8 kana_backup_collect_entry(const c8* _path, b8 _is_dir, any _user_data) {
    kana_backup_list* _list = (kana_backup_list*)_user_data;
    if(_is_dir) {
        return true;
    }
    // Relative to the folder, with '/' only.
    const c8* _rel = kana_backup_starts_with(_path, _list->root) ? _path + _list->root_len : _path;
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
    if(_clean[0] == 0 || kana_backup_starts_with(_clean, KANA_BACKUP_ASIDE "/") || kana_backup_starts_with(_clean, KANA_BACKUP_OUTBOX "/")) {
        return true;
    }
    if(!_list->everything) {
        if(_name[0] == '.' || kana_backup_ends_with(_name, ".bak") || kana_backup_ends_with(_name, ".tmp") || kana_backup_ends_with(_name, ".bad") ||
           strcmp(_name, "perf.txt") == 0 || strcmp(_name, "translate.txt") == 0 || strcmp(_name, "mlkit_samples.txt") == 0) {
            return true;
        }
    }
    if(_list->count == _list->capacity) {
        const u32 _cap  = _list->capacity > 0 ? _list->capacity * 2u : 64u;
        any       _more = realloc(_list->paths, (usize)_cap * RDE_MAX_PATH);
        if(_more == NULL) {
            _list->ok = false;
            return false;
        }
        _list->paths    = (c8(*)[RDE_MAX_PATH])_more;
        _list->capacity = _cap;
    }
    snprintf(_list->paths[_list->count++], RDE_MAX_PATH, "%s", _clean);
    return true;
}

// The files under _dir (none when it is not there). In the save folder
// (_save_folder), before-import/ and outbox/ are not gone into.
RDE_INTERNAL kana_backup_list kana_backup_collect(const c8* _dir, b8 _everything, b8 _save_folder) {
    kana_backup_list _list = { NULL, 0, 0, _dir, strlen(_dir), _everything, true };
    if(rde_file_dir_exists(_dir)) {
        c8* _skip[2] = { (c8*)KANA_BACKUP_ASIDE, (c8*)KANA_BACKUP_OUTBOX };
        rde_file_crawl_dir_recursively(_dir, kana_backup_collect_entry, _save_folder ? _skip : NULL, _save_folder ? 2u : 0u, &_list);
    }
    return _list;
}

RDE_INTERNAL void kana_backup_list_free(kana_backup_list* _list) {
    free(_list->paths);
    _list->paths = NULL;
    _list->count = _list->capacity = 0;
}

// --- export ------------------------------------------------------------------------------

// Writes _n bytes to _f, adding them to the checksum.
RDE_INTERNAL b8 kana_backup_write(FILE* _f, u32* _hash, const any _p, usize _n) {
    *_hash = kana_backup_fnv(*_hash, (const u8*)_p, _n);
    return _n == 0 || fwrite(_p, 1, _n, _f) == _n;
}

RDE_INTERNAL u8* kana_backup_read_file(const c8* _path, usize* _size) {
    *_size = 0;
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return NULL;
    }
    u8*   _data = NULL;
    usize _cap  = 0;
    for(;;) {
        if(*_size == _cap) {
            _cap = _cap > 0 ? _cap * 2u : 64u * 1024u;
            u8* _more = (u8*)realloc(_data, _cap);
            if(_more == NULL) {
                free(_data);
                fclose(_f);
                return NULL;
            }
            _data = _more;
        }
        const usize _got = fread(_data + *_size, 1, _cap - *_size, _f);
        *_size += _got;
        if(_got == 0) {
            break;
        }
    }
    const b8 _bad = ferror(_f) != 0;
    fclose(_f);
    if(_bad) {
        free(_data);
        return NULL;
    }
    return _data != NULL ? _data : (u8*)malloc(1);   // an empty file is a file
}

RDE_INTERNAL b8 kana_backup_is_canvas(const c8* _rel) {
    return kana_backup_starts_with(_rel, "notes/") && kana_backup_ends_with(_rel, ".kana") && strcmp(_rel, "notes/index.kana") != 0;
}

b8 kana_backup_export(const c8* _dir, const c8* _out, kana_backup_info* _info) {
    kana_backup_list _list = kana_backup_collect(_dir, false, true);
    FILE*            _f    = _list.ok ? fopen(_out, "wb") : NULL;
    if(_f == NULL) {
        kana_backup_list_free(&_list);
        return false;
    }
    kana_backup_info _made;
    memset(&_made, 0, sizeof(_made));
    _made.files   = _list.count;
    _made.created = (u64)time(NULL);
    snprintf(_made.version, sizeof(_made.version), "%s", KANA_VERSION);

    u32 _hash = 2166136261u;
    u8  _head[KANA_BACKUP_HEAD];
    memset(_head, 0, sizeof(_head));
    memcpy(_head, KANA_BACKUP_MAGIC, 8);
    kana_backup_put_u32(_head + 8, KANA_BACKUP_VERSION);
    kana_backup_put_u32(_head + 12, _list.count);
    kana_backup_put_u32(_head + 16, (u32)(_made.created & 0xFFFFFFFFu));
    kana_backup_put_u32(_head + 20, (u32)(_made.created >> 32));
    memcpy(_head + 24, _made.version, strlen(_made.version) < 16 ? strlen(_made.version) : 15);
    b8 _ok = kana_backup_write(_f, &_hash, _head, sizeof(_head));

    for(u32 _i = 0; _ok && _i < _list.count; _i++) {
        const c8*   _rel = _list.paths[_i];
        const usize _len = strlen(_rel);
        c8          _full[RDE_MAX_PATH];
        snprintf(_full, sizeof(_full), "%s%s", _dir, _rel);
        usize _size = 0;
        u8*   _data = _len <= KANA_BACKUP_PATH_MAX ? kana_backup_read_file(_full, &_size) : NULL;
        if(_data == NULL || _size > 0xFFFFFFFFu) {
            free(_data);
            _ok = false;
            break;
        }
        u8 _n16[2] = { (u8)_len, (u8)(_len >> 8) };
        u8 _n32[4];
        kana_backup_put_u32(_n32, (u32)_size);
        _ok = kana_backup_write(_f, &_hash, _n16, 2) && kana_backup_write(_f, &_hash, _rel, _len) &&
              kana_backup_write(_f, &_hash, _n32, 4) && kana_backup_write(_f, &_hash, _data, _size);
        free(_data);
        _made.bytes    += _size;
        _made.canvases += kana_backup_is_canvas(_rel) ? 1u : 0u;
    }
    if(_ok) {
        u8 _sum[4];
        kana_backup_put_u32(_sum, _hash);
        _ok = fwrite(_sum, 1, 4, _f) == 4;
    }
    _ok = fclose(_f) == 0 && _ok;
    kana_backup_list_free(&_list);
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
RDE_INTERNAL b8 kana_backup_safe_path(const c8* _p, usize _n) {
    if(_n == 0 || _n > KANA_BACKUP_PATH_MAX || _p[0] == '/' || _p[_n - 1] == '/') {
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
    const usize _aside = strlen(KANA_BACKUP_ASIDE), _outbox = strlen(KANA_BACKUP_OUTBOX);
    return !(_n > _aside && strncmp(_p, KANA_BACKUP_ASIDE "/", _aside + 1) == 0) && !(_n > _outbox && strncmp(_p, KANA_BACKUP_OUTBOX "/", _outbox + 1) == 0);
}

// Walks a backup's files: _each (may be NULL) gets each one; false when the
// layout does not hold together.
typedef b8 (*kana_backup_each_fn)(const c8* _path, const u8* _data, u32 _size, any _user_data);

RDE_INTERNAL b8 kana_backup_walk(const u8* _data, usize _size, kana_backup_info* _info, kana_backup_each_fn _each, any _user_data) {
    if(_data == NULL || _size < KANA_BACKUP_HEAD + 4u || memcmp(_data, KANA_BACKUP_MAGIC, 8) != 0 || kana_backup_u32(_data + 8) != KANA_BACKUP_VERSION) {
        return false;
    }
    if(kana_backup_fnv(2166136261u, _data, _size - 4u) != kana_backup_u32(_data + _size - 4u)) {
        return false;   // damaged, or cut short
    }
    kana_backup_info _found;
    memset(&_found, 0, sizeof(_found));
    _found.files   = kana_backup_u32(_data + 12);
    _found.created = (u64)kana_backup_u32(_data + 16) | ((u64)kana_backup_u32(_data + 20) << 32);
    memcpy(_found.version, _data + 24, 15);

    const usize _end = _size - 4u;
    usize       _at  = KANA_BACKUP_HEAD;
    for(u32 _i = 0; _i < _found.files; _i++) {
        if(_end - _at < 2u) {
            return false;
        }
        const usize _len = (usize)_data[_at] | ((usize)_data[_at + 1] << 8);
        _at += 2u;
        if(_end - _at < _len + 4u || !kana_backup_safe_path((const c8*)_data + _at, _len)) {
            return false;
        }
        c8 _path[KANA_BACKUP_PATH_MAX + 1u];
        memcpy(_path, _data + _at, _len);
        _path[_len] = 0;
        _at += _len;
        const u32 _bytes = kana_backup_u32(_data + _at);
        _at += 4u;
        if(_end - _at < _bytes) {
            return false;
        }
        if(_each != NULL && !_each(_path, _data + _at, _bytes, _user_data)) {
            return false;
        }
        _at             += _bytes;
        _found.bytes    += _bytes;
        _found.canvases += kana_backup_is_canvas(_path) ? 1u : 0u;
    }
    if(_at != _end) {
        return false;
    }
    if(_info != NULL) {
        *_info = _found;
    }
    return true;
}

b8 kana_backup_inspect(const u8* _data, usize _size, kana_backup_info* _info) {
    return kana_backup_walk(_data, _size, _info, NULL, NULL);
}

// --- restore --------------------------------------------------------------------------------

RDE_STRUCT {
    const c8*         dir;
    kana_backup_list* written;   // what was written, to take back on a failure
} kana_backup_restoring;

RDE_INTERNAL b8 kana_backup_restore_file(const c8* _path, const u8* _data, u32 _size, any _user_data) {
    kana_backup_restoring* _r = (kana_backup_restoring*)_user_data;
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
    kana_backup_list* _w = _r->written;
    if(_w->count == _w->capacity) {
        const u32 _cap  = _w->capacity > 0 ? _w->capacity * 2u : 64u;
        any       _more = realloc(_w->paths, (usize)_cap * RDE_MAX_PATH);
        if(_more == NULL) {
            remove(_full);
            return false;
        }
        _w->paths    = (c8(*)[RDE_MAX_PATH])_more;
        _w->capacity = _cap;
    }
    snprintf(_w->paths[_w->count++], RDE_MAX_PATH, "%s", _path);
    return _ok && _closed;
}

// Moves _list's files from _from to _to (both folders, ending in '/'): how many moved.
RDE_INTERNAL u32 kana_backup_move_all(const kana_backup_list* _list, const c8* _from, const c8* _to) {
    for(u32 _i = 0; _i < _list->count; _i++) {
        c8 _src[RDE_MAX_PATH], _dst[RDE_MAX_PATH];
        snprintf(_src, sizeof(_src), "%s%s", _from, _list->paths[_i]);
        snprintf(_dst, sizeof(_dst), "%s%s", _to, _list->paths[_i]);
        if(!rde_file_create_missing_dirs(_dst) || !rde_file_move(_src, _dst)) {
            return _i;
        }
    }
    return _list->count;
}

b8 kana_backup_restore(const u8* _data, usize _size, const c8* _dir) {
    if(!kana_backup_inspect(_data, _size, NULL)) {
        return false;
    }
    c8 _aside[RDE_MAX_PATH];
    snprintf(_aside, sizeof(_aside), "%s%s/", _dir, KANA_BACKUP_ASIDE);

    // The last import's leftovers go; what is here now goes aside.
    kana_backup_list _old = kana_backup_collect(_aside, true, false);
    for(u32 _i = 0; _i < _old.count; _i++) {
        c8 _p[RDE_MAX_PATH];
        snprintf(_p, sizeof(_p), "%s%s", _aside, _old.paths[_i]);
        rde_file_delete(_p);
    }
    kana_backup_list_free(&_old);

    kana_backup_list _here = kana_backup_collect(_dir, true, true);
    if(!_here.ok) {
        kana_backup_list_free(&_here);
        return false;
    }
    const u32 _moved = kana_backup_move_all(&_here, _dir, _aside);
    b8        _ok    = _moved == _here.count;

    // The backup's files.
    kana_backup_list      _written = { NULL, 0, 0, _dir, strlen(_dir), true, true };
    kana_backup_restoring _r       = { _dir, &_written };
    if(_ok) {
        _ok = kana_backup_walk(_data, _size, NULL, kana_backup_restore_file, &_r);
    }
    if(!_ok) {
        // Back as it was: what was written goes, what was moved comes back.
        for(u32 _i = 0; _i < _written.count; _i++) {
            c8 _p[RDE_MAX_PATH];
            snprintf(_p, sizeof(_p), "%s%s", _dir, _written.paths[_i]);
            rde_file_delete(_p);
        }
        kana_backup_list _back = _here;
        _back.count = _moved;
        kana_backup_move_all(&_back, _aside, _dir);
    }
    kana_backup_list_free(&_written);
    kana_backup_list_free(&_here);
    return _ok;
}
