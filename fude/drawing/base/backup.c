// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/backup.h"
#include "drawing/base/kfile.h"
#include "drawing/base/crash.h"

// ===========================================================================
// See backup.h.
//
// Export lists the save folder's files (fude_backup_list, paths relative to the folder) and writes them one after the
// other, adding each piece written to a running checksum. Inspect and restore share one walk over a backup's bytes
// (fude_backup_walk), which checks the checksum and the layout first; restore gives it a callback that writes each
// file.
// ===========================================================================

// The file's first 8 bytes, counting the NUL. It is the format's name, not an app's: every backup ever written starts
// with it, whichever app made it, so it never changes.
#define FUDE_BACKUP_MAGIC    "KANABKP"
// The format version written in the header; a backup with any other version is refused.
#define FUDE_BACKUP_VERSION  1u
// The header's size in bytes: the magic (8), the version (4), the file count (4), the time (8), the app's version (16).
#define FUDE_BACKUP_HEAD     (8u + 4u + 4u + 8u + 16u)
// The longest path a backup holds, in bytes. A longer path fails an export, and makes a backup read as damaged. The
// paths are the save folder's own names (notes/<id>.<kind>, practice/<code point>.kana...), a few dozen bytes, so this
// is a check on a damaged or crafted file rather than a limit a user's data can reach.
#define FUDE_BACKUP_PATH_MAX 255u
#define FUDE_BACKUP_FILE_MAX 0x7FFFFFFFu     // The largest file a backup holds, in bytes: what an rde_arr can hold.

// Adds _n bytes to a 32-bit FNV-1a hash and returns it, so the checksum can be carried from one piece to the next.
// It is the backup format's checksum, and every backup ever written uses it, so it stays FNV-1a rather than RDE's hash:
// an old backup must still pass the check.
RDE_INTERNAL u32 fude_backup_fnv(u32 _hash, const u8* _p, usize _n) {
    for(usize _i = 0; _i < _n; _i++) {
        _hash = (_hash ^ _p[_i]) * 16777619u;
    }
    return _hash;
}

// Reads a little-endian u32 from _p.
RDE_INTERNAL u32 fude_backup_u32(const u8* _p) {
    return (u32)_p[0] | ((u32)_p[1] << 8) | ((u32)_p[2] << 16) | ((u32)_p[3] << 24);
}

// Writes _v at _p as a little-endian u32.
RDE_INTERNAL void fude_backup_put_u32(u8* _p, u32 _v) {
    _p[0] = (u8)_v; _p[1] = (u8)(_v >> 8); _p[2] = (u8)(_v >> 16); _p[3] = (u8)(_v >> 24);
}

// --- the files of a folder ------------------------------------------------------------------

// A list of the files under a folder, as paths relative to it with '/' between folders. Each path takes a fixed
// RDE_MAX_PATH slot in the array; the list is on the std allocator, because a save folder can hold many files.
RDE_STRUCT {
    rde_arr TYPE(c8[RDE_MAX_PATH]) paths;
    // The folder the paths are relative to, ending in '/', and its length in bytes.
    const c8* root;
    usize     root_len;
    b8        everything;   // When false, only the files a backup keeps are listed (see fude_backup_collect_entry).
    b8        save_folder;  // The root is the save folder: its own before-import/ and outbox/ are left out.
} fude_backup_list;

// Makes an empty list of the files under _root; _everything says whether the files a backup leaves out are listed too.
// The list keeps _root's pointer, so _root must outlive it. Free it with fude_backup_list_free.
RDE_INTERNAL fude_backup_list fude_backup_list_new(const c8* _root, b8 _everything) {
    fude_backup_list _list = { rde_arr_new(RDE_MAX_PATH, rde_memory_allocator_get_default_std()), _root, rde_strlen(_root), _everything, false };
    return _list;
}

// Returns how many paths the list holds.
RDE_INTERNAL u32 fude_backup_list_count(const fude_backup_list* _list) {
    return (u32)rde_arr_length(&_list->paths);
}

// Returns the _i-th path of the list, relative to its folder.
RDE_INTERNAL const c8* fude_backup_list_at(const fude_backup_list* _list, u32 _i) {
    return (const c8*)_list->paths.memory + (usize)_i * RDE_MAX_PATH;
}

// Adds a copy of _path to the list; a path longer than RDE_MAX_PATH - 1 bytes is cut short.
RDE_INTERNAL void fude_backup_list_add(fude_backup_list* _list, const c8* _path) {
    rde_snprintf((c8*)rde_arr_add_n(&_list->paths, 1u), RDE_MAX_PATH, "%s", _path);
}

// Returns true when _s ends with _end.
RDE_INTERNAL b8 fude_backup_ends_with(const c8* _s, const c8* _end) {
    const usize _n = rde_strlen(_s), _m = rde_strlen(_end);
    return _n >= _m && rde_strcmp(_s + _n - _m, _end) == 0;
}

// Returns true when _s starts with _start.
RDE_INTERNAL b8 fude_backup_starts_with(const c8* _s, const c8* _start) {
    return rde_strncmp(_s, _start, rde_strlen(_start)) == 0;
}

// The crawl's callback: adds one file to the list in _user_data (a fude_backup_list), as a path relative to the list's
// folder with '\\' turned into '/'. Folders are not listed, nor, in the save folder, anything in its before-import/ or
// outbox/. Unless the list
// wants everything, it also leaves out what a backup does not keep: hidden files, the .bak, .tmp and .bad copies, and
// the diagnostics (perf.txt, translate.txt, mlkit_samples.txt). Always returns true, so the crawl goes on.
RDE_INTERNAL b8 fude_backup_collect_entry(const c8* _path, b8 _is_dir, any _user_data) {
    fude_backup_list* _list = (fude_backup_list*)_user_data;
    if(_is_dir) {
        return true;
    }
    // Make the path relative to the folder, with '/' as the only separator, whatever the platform gave.
    const c8* _rel = fude_backup_starts_with(_path, _list->root) ? _path + _list->root_len : _path;
    while(*_rel == '/' || *_rel == '\\') {
        _rel++;
    }
    c8 _clean[RDE_MAX_PATH];
    rde_snprintf(_clean, sizeof(_clean), "%s", _rel);
    for(c8* _c = _clean; *_c != 0; _c++) {
        if(*_c == '\\') {
            *_c = '/';
        }
    }
    const c8* _name = rde_strrchr(_clean, '/') != NULL ? rde_strrchr(_clean, '/') + 1 : _clean;
    if(_clean[0] == 0) {
        return true;
    }
    // The save folder's own (at its top, never a folder or file of the same name deeper down): before-import/ and
    // outbox/ (an import's and an export's own), and the crash report's files (this device's: crash.h).
    if(_list->save_folder && (fude_backup_starts_with(_clean, FUDE_BACKUP_ASIDE "/") || fude_backup_starts_with(_clean, FUDE_BACKUP_OUTBOX "/") ||
                              rde_strcmp(_clean, FUDE_CRASH_REPORT_FILE) == 0 || rde_strcmp(_clean, FUDE_CRASH_RUNNING_FILE) == 0)) {
        return true;
    }
    if(!_list->everything) {
        if(_name[0] == '.' || fude_backup_ends_with(_name, ".bak") || fude_backup_ends_with(_name, ".tmp") || fude_backup_ends_with(_name, ".bad") ||
           rde_strcmp(_name, "perf.txt") == 0 || rde_strcmp(_name, "translate.txt") == 0 || rde_strcmp(_name, "mlkit_samples.txt") == 0) {
            return true;
        }
    }
    fude_backup_list_add(_list, _clean);
    return true;
}

// Returns the list of the files under _dir (empty when the folder does not exist); _everything is as in
// fude_backup_list_new. When _dir is the save folder (_save_folder), the crawl does not go into before-import/ and
// outbox/. The caller frees the list with fude_backup_list_free.
RDE_INTERNAL fude_backup_list fude_backup_collect(const c8* _dir, b8 _everything, b8 _save_folder) {
    fude_backup_list _list = fude_backup_list_new(_dir, _everything);
    _list.save_folder      = _save_folder;
    if(rde_file_dir_exists(_dir)) {
        // No folder is excluded by the crawl itself: it matches a name at any depth. The callback leaves out the save
        // folder's own before-import/ and outbox/ by their place.
        rde_file_crawl_dir_recursively(_dir, fude_backup_collect_entry, NULL, 0u, &_list);
    }
    return _list;
}

// Frees the list's paths.
RDE_INTERNAL void fude_backup_list_free(fude_backup_list* _list) {
    rde_arr_free(&_list->paths);
}

// --- export ------------------------------------------------------------------------------

// Writes _n bytes to _f and adds them to the running checksum *_hash. Returns false when the file could not take them.
RDE_INTERNAL b8 fude_backup_write(rde_file* _f, u32* _hash, const any _p, usize _n) {
    *_hash = fude_backup_fnv(*_hash, (const u8*)_p, _n);
    if(_n == 0) {
        return true;
    }
    b8 _ok = false;
    RDE_TRY({
        rde_file_write_bytes(_f, (const u8*)_p, _n);
        _ok = !rde_failed();
    });
    return _ok;
}

// Reads the whole file at _path into _data, which is emptied first (an empty file is still a file and reads as true).
// Returns false when it could not be read, or when it is larger than FUDE_BACKUP_FILE_MAX.
RDE_INTERNAL b8 fude_backup_read_file(const c8* _path, rde_arr* _data) {
    rde_arr_clear(_data);
    if(fude_file_size(_path) > FUDE_BACKUP_FILE_MAX) {
        return false;   // Too big to be one of the app's files, so it is not read at all.
    }
    return fude_file_read_into(_path, _data);
}

// Returns true when _rel (relative to the save folder) is a canvas page: notes/<n>.kana, but not notes/index.kana.
RDE_INTERNAL b8 fude_backup_is_canvas(const c8* _rel) {
    return fude_backup_starts_with(_rel, "notes/") && fude_backup_ends_with(_rel, ".kana") && rde_strcmp(_rel, "notes/index.kana") != 0;
}

// Lists the save folder, then writes the header, each file (path length, path, size, bytes) and the checksum, as
// backup.h describes. Any failure stops the export and deletes the partial file. See the header.
b8 fude_backup_export(const c8* _dir, const c8* _out, const c8* _version, fude_backup_info* _info) {
    fude_backup_list _list = fude_backup_collect(_dir, false, true);
    rde_file*        _f    = NULL;
    RDE_TRY({
        _f = rde_file_open(_out, RDE_FILE_MODE_WRITE_BYTES);
        if(rde_failed()) {
            _f = NULL;
        }
    });
    if(_f == NULL) {
        fude_backup_list_free(&_list);
        return false;
    }
    fude_backup_info _made;
    rde_memset(&_made, 0, sizeof(_made));
    _made.files   = fude_backup_list_count(&_list);
    _made.created = (u64)rde_date_now_unix_seconds();
    rde_snprintf(_made.version, sizeof(_made.version), "%s", _version != NULL ? _version : "");

    // FNV-1a's starting value (its offset basis).
    u32 _hash = 2166136261u;
    u8  _head[FUDE_BACKUP_HEAD];
    rde_memset(_head, 0, sizeof(_head));
    rde_memcpy(_head, FUDE_BACKUP_MAGIC, 8);
    fude_backup_put_u32(_head + 8, FUDE_BACKUP_VERSION);
    fude_backup_put_u32(_head + 12, _made.files);
    fude_backup_put_u32(_head + 16, (u32)(_made.created & 0xFFFFFFFFu));
    fude_backup_put_u32(_head + 20, (u32)(_made.created >> 32));
    rde_memcpy(_head + 24, _made.version, rde_strlen(_made.version) < 16 ? rde_strlen(_made.version) : 15);
    b8 _ok = fude_backup_write(_f, &_hash, _head, sizeof(_head));

    rde_arr TYPE(u8) _data = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());   // Each file's bytes.
    for(u32 _i = 0; _ok && _i < _made.files; _i++) {
        const c8*   _rel = fude_backup_list_at(&_list, _i);
        const usize _len = rde_strlen(_rel);
        c8          _full[RDE_MAX_PATH];
        rde_snprintf(_full, sizeof(_full), "%s%s", _dir, _rel);
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
        u8  _sum[4];
        u32 _unused = 0;
        fude_backup_put_u32(_sum, _hash);
        _ok = fude_backup_write(_f, &_unused, _sum, 4);   // The checksum is not added to itself.
    }
    RDE_TRY({
        rde_file_close(_f);
        _ok = _ok && !rde_failed();
    });
    rde_arr_free(&_data);
    fude_backup_list_free(&_list);
    if(!_ok) {
        rde_file_delete(_out);
        return false;
    }
    if(_info != NULL) {
        *_info = _made;
    }
    return true;
}

// --- inspect --------------------------------------------------------------------------------

// Returns true when the path _p (_n bytes, not NUL-terminated) is one a backup may write: relative, at most
// FUDE_BACKUP_PATH_MAX bytes, with no empty, "." or ".." part, no NUL, '\\' or ':', and not inside before-import/ or
// outbox/. That keeps a crafted backup from writing outside the save folder.
RDE_INTERNAL b8 fude_backup_safe_path(const c8* _p, usize _n) {
    if(_n == 0 || _n > FUDE_BACKUP_PATH_MAX || _p[0] == '/' || _p[_n - 1] == '/') {
        return false;
    }
    usize _seg = 0;   // Where the current folder or file name began.
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
    const usize _aside = rde_strlen(FUDE_BACKUP_ASIDE), _outbox = rde_strlen(FUDE_BACKUP_OUTBOX);
    return !(_n > _aside && rde_strncmp(_p, FUDE_BACKUP_ASIDE "/", _aside + 1) == 0) && !(_n > _outbox && rde_strncmp(_p, FUDE_BACKUP_OUTBOX "/", _outbox + 1) == 0);
}

// Called by fude_backup_walk for each file in a backup: its path (NUL-terminated), its bytes and their count. Returns
// false to stop the walk, which then fails.
typedef b8 (*fude_backup_each_fn)(const c8* _path, const u8* _data, u32 _size, any _user_data);

// Walks a backup's _size bytes: checks the magic, the version and the checksum, then reads each file's entry, handing
// it to _each (may be NULL). Returns false when the backup is damaged, its layout does not hold together, a path is
// not safe, or _each returned false. _info (may be NULL) gets the summary only when it returns true.
RDE_INTERNAL b8 fude_backup_walk(const u8* _data, usize _size, fude_backup_info* _info, fude_backup_each_fn _each, any _user_data) {
    if(_data == NULL || _size < FUDE_BACKUP_HEAD + 4u || rde_memcmp(_data, FUDE_BACKUP_MAGIC, 8) != 0 || fude_backup_u32(_data + 8) != FUDE_BACKUP_VERSION) {
        return false;
    }
    if(fude_backup_fnv(2166136261u, _data, _size - 4u) != fude_backup_u32(_data + _size - 4u)) {
        return false;   // The backup is damaged or was cut short.
    }
    fude_backup_info _found;
    rde_memset(&_found, 0, sizeof(_found));
    _found.files   = fude_backup_u32(_data + 12);
    _found.created = (u64)fude_backup_u32(_data + 16) | ((u64)fude_backup_u32(_data + 20) << 32);
    rde_memcpy(_found.version, _data + 24, 15);

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
        rde_memcpy(_path, _data + _at, _len);
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

// Checks a backup by walking it without writing anything. See the header.
b8 fude_backup_inspect(const u8* _data, usize _size, fude_backup_info* _info) {
    return fude_backup_walk(_data, _size, _info, NULL, NULL);
}

// --- restore --------------------------------------------------------------------------------

// What fude_backup_restore_file needs while a backup is restored.
RDE_STRUCT {
    // The save folder, ending in '/'.
    const c8*         dir;
    fude_backup_list* written;   // The files written so far, deleted again if the restore fails.
} fude_backup_restoring;

// The walk's callback while restoring: writes one file of the backup under the save folder, making its folders first,
// and records it in the list of files written. Returns false when it could not be written.
RDE_INTERNAL b8 fude_backup_restore_file(const c8* _path, const u8* _data, u32 _size, any _user_data) {
    fude_backup_restoring* _r = (fude_backup_restoring*)_user_data;
    c8 _full[RDE_MAX_PATH];
    rde_snprintf(_full, sizeof(_full), "%s%s", _r->dir, _path);
    if(!rde_file_create_missing_dirs(_full)) {
        return false;
    }
    const b8 _ok = fude_file_write(_full, _data, _size);
    // Recorded even when the write failed, so that whatever part of it is there is deleted too.
    fude_backup_list_add(_r->written, _path);
    return _ok;
}

// Moves the first _count files of _list from the folder _from to the folder _to (both ending in '/'), making folders
// as needed. Stops at the first file that cannot be moved, and returns how many were moved.
RDE_INTERNAL u32 fude_backup_move_all(const fude_backup_list* _list, u32 _count, const c8* _from, const c8* _to) {
    for(u32 _i = 0; _i < _count; _i++) {
        c8 _src[RDE_MAX_PATH], _dst[RDE_MAX_PATH];
        rde_snprintf(_src, sizeof(_src), "%s%s", _from, fude_backup_list_at(_list, _i));
        rde_snprintf(_dst, sizeof(_dst), "%s%s", _to, fude_backup_list_at(_list, _i));
        if(!rde_file_create_missing_dirs(_dst) || !rde_file_move(_src, _dst)) {
            return _i;
        }
    }
    return _count;
}

// Replaces the save folder's files with the backup's: checks the backup, clears before-import/, moves every file there,
// then writes the backup's files. On any failure it deletes what it wrote and moves the old files back. See the header.
b8 fude_backup_restore(const u8* _data, usize _size, const c8* _dir) {
    if(!fude_backup_inspect(_data, _size, NULL)) {
        return false;
    }
    c8 _aside[RDE_MAX_PATH];
    rde_snprintf(_aside, sizeof(_aside), "%s%s/", _dir, FUDE_BACKUP_ASIDE);

    // Delete what the previous import moved aside, then move aside what is in the save folder now.
    fude_backup_list _old = fude_backup_collect(_aside, true, false);
    for(u32 _i = 0; _i < fude_backup_list_count(&_old); _i++) {
        c8 _p[RDE_MAX_PATH];
        rde_snprintf(_p, sizeof(_p), "%s%s", _aside, fude_backup_list_at(&_old, _i));
        rde_file_delete(_p);
    }
    fude_backup_list_free(&_old);

    fude_backup_list _here  = fude_backup_collect(_dir, true, true);
    const u32        _moved = fude_backup_move_all(&_here, fude_backup_list_count(&_here), _dir, _aside);
    b8               _ok    = _moved == fude_backup_list_count(&_here);

    // Write the backup's files.
    fude_backup_list      _written = fude_backup_list_new(_dir, true);
    fude_backup_restoring _r       = { _dir, &_written };
    if(_ok) {
        _ok = fude_backup_walk(_data, _size, NULL, fude_backup_restore_file, &_r);
    }
    if(!_ok) {
        // Put the folder back as it was: delete what was written and move back what was moved aside.
        for(u32 _i = 0; _i < fude_backup_list_count(&_written); _i++) {
            c8 _p[RDE_MAX_PATH];
            rde_snprintf(_p, sizeof(_p), "%s%s", _dir, fude_backup_list_at(&_written, _i));
            rde_file_delete(_p);
        }
        fude_backup_move_all(&_here, _moved, _aside, _dir);
    }
    fude_backup_list_free(&_written);
    fude_backup_list_free(&_here);
    return _ok;
}
