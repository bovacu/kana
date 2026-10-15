// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/save.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See save.h.
// ===========================================================================

// --- files ---------------------------------------------------------------------

// The name of the app's folder in a phone's or tablet's own storage: the app's id (info.h), set by fude_app_start;
// "fude" until then. Its room is FUDE_SAVE_FOLDER_MAX (save.h says why that is enough).
RDE_INTERNAL c8 fude_save_folder[FUDE_SAVE_FOLDER_MAX] = "fude";
// The full path of the save folder, ending in '/', found on first use; empty until then, or after the name changes.
RDE_INTERNAL c8 fude_save_dir_found[RDE_MAX_PATH];

// Names the app's folder; a NULL, empty or unchanged name does nothing. A name past FUDE_SAVE_FOLDER_MAX is an app's
// mistake, logged before it is cut.
void fude_save_set_folder(const c8* _name) {
    if(_name != NULL && rde_strlen(_name) >= FUDE_SAVE_FOLDER_MAX) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "save: the app's id \"%s\" is longer than %u bytes and is cut", _name, FUDE_SAVE_FOLDER_MAX - 1u);
    }
    if(_name != NULL && _name[0] != 0 && rde_strcmp(_name, fude_save_folder) != 0) {
        rde_snprintf(fude_save_folder, sizeof(fude_save_folder), "%s", _name);
        fude_save_dir_found[0] = 0;   // The save folder is found again, under the new name.
    }
}

// Returns the save folder, finding and creating it on first use: <the platform's internal storage>/<folder>/ on iOS
// and Android, ./saves/ elsewhere or when the platform gives no path.
const c8* fude_save_dir(void) {
    c8* const _dir = fude_save_dir_found;

    if(_dir[0] != 0) {
        return _dir;
    }

#if defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_ANDROID)
    rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
#if defined(RDE_PLATFORM_IOS)
    const c8* _base = rde_ios_get_internal_storage_data_path(_allocator);
#else
    const c8* _base = rde_android_get_internal_storage_data_path(_allocator);
#endif
    if(_base != NULL) {
        const usize _len = rde_strlen(_base);
        rde_snprintf(_dir, sizeof(fude_save_dir_found), "%s%s%s/", _base, (_len > 0 && _base[_len - 1] == '/') ? "" : "/", fude_save_folder);
        _allocator->free(_allocator->allocator, (any)_base);
    }
#endif

    if(_dir[0] == 0) {
        rde_snprintf(_dir, sizeof(fude_save_dir_found), "./saves/");
    }

    // Create the folder through a file path inside it: rde_file_create_missing_dirs strips the file name and leaves
    // existing folders alone, whereas rde_file_create_dir REPLACES an existing folder.
    c8 _probe[RDE_MAX_PATH];
    rde_snprintf(_probe, sizeof(_probe), "%sprobe", _dir);   // Any file name: only its folders are made.
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "save: could not create the save folder %s", _dir);
    }

    return _dir;
}

// The downloads folder's full path, ending in '/', found on first use; empty until then.
RDE_INTERNAL c8 fude_save_downloads_found[RDE_MAX_PATH];

// Returns the downloads folder (see the header), finding and creating it on first use.
const c8* fude_save_downloads_dir(void) {
    c8* const _dir = fude_save_downloads_found;
    if(_dir[0] != 0) {
        return _dir;
    }
#if defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_ANDROID)
    rde_memory_allocator* _allocator = rde_memory_allocator_get_default();
#if defined(RDE_PLATFORM_IOS)
    const c8* _base = rde_ios_get_cache_data_path(_allocator);
#else
    const c8* _base = rde_android_get_cache_data_path(_allocator);
#endif
    if(_base != NULL) {
        const usize _len = rde_strlen(_base);
        rde_snprintf(_dir, sizeof(fude_save_downloads_found), "%s%sdownloads/", _base, (_len > 0 && _base[_len - 1] == '/') ? "" : "/");
        _allocator->free(_allocator->allocator, (any)_base);
    }
#endif
    if(_dir[0] == 0) {
        rde_snprintf(_dir, sizeof(fude_save_downloads_found), "./downloads/");   // (a computer's: beside ./saves/, not in it)
    }
    c8 _probe[RDE_MAX_PATH];
    rde_snprintf(_probe, sizeof(_probe), "%sprobe", _dir);   // Any file name: only its folders are made.
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "save: could not create the downloads folder %s", _dir);
    }
    return _dir;
}

// Writes <save folder>/<_name>/ into _dir, creating the folder only when the path differs from what _dir held.
void fude_save_subdir(const c8* _name, c8* _dir, usize _size) {
    c8 _now[RDE_MAX_PATH];
    rde_snprintf(_now, sizeof(_now), "%s%s/", fude_save_dir(), _name);
    if(rde_strcmp(_now, _dir) == 0) {
        return;   // Unchanged, so the folder was made already.
    }
    rde_snprintf(_dir, _size, "%s", _now);
    c8 _probe[RDE_MAX_PATH];
    rde_snprintf(_probe, sizeof(_probe), "%sprobe", _dir);   // Any file name: only its folders are made.
    if(!rde_file_create_missing_dirs(_probe)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "save: could not create the folder %s", _dir);
    }
}

// --- reading back -------------------------------------------------------------------

// Reads one file through _read. Returns OK, MISSING when the file is not there, or CORRUPT when it could not be read
// or parsed, after setting it aside as <_file>.bad.
RDE_INTERNAL FUDE_LOAD_ fude_save_load_file(const c8* _file, fude_save_reader_fn _read, any _user) {
    if(!rde_file_exists(_file)) {
        return FUDE_LOAD_MISSING;
    }
    u32      _size = 0;
    u8*      _data = fude_file_read(_file, &_size);
    const b8 _ok   = _data != NULL && _read(_data, _size, _user);
    fude_file_free(_data);
    if(!_ok) {
        fude_file_set_aside(_file);
        return FUDE_LOAD_CORRUPT;
    }
    return FUDE_LOAD_OK;
}

// Reads the main file, else its .bak, and puts a .bak that was read back in place as the main file. See the header.
FUDE_LOAD_ fude_save_load(const c8* _path, fude_save_reader_fn _read, any _user) {
    const FUDE_LOAD_ _main = fude_save_load_file(_path, _read, _user);
    if(_main == FUDE_LOAD_OK) {
        return FUDE_LOAD_OK;
    }
    // The main file is missing (the app was killed between a save's two renames) or damaged, so read the backup,
    // which is the previous save.
    c8 _bak[RDE_MAX_PATH];
    rde_snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    const FUDE_LOAD_ _backup = fude_save_load_file(_bak, _read, _user);
    if(_backup == FUDE_LOAD_OK) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "save: %s %s, its backup read", _path, _main == FUDE_LOAD_MISSING ? "missing" : "damaged");
        // The backup is put back as the main file, so that what is saved next builds on what was recovered. A file
        // that is only ever added to (the exam log) would otherwise start anew without the recovered entries, and the
        // save after that would turn the new file into the .bak, so the recovered one would be gone for good. The
        // copy goes to a .tmp and is renamed into place, as every save is (kfile.h), so a kill never leaves half a
        // file; should the copy fail, the .bak is still there for the next launch.
        c8 _tmp[RDE_MAX_PATH];
        rde_snprintf(_tmp, sizeof(_tmp), "%s.tmp", _path);
        if(!rde_file_copy(_bak, _tmp) || !rde_file_move(_tmp, _path)) {
            rde_log_level(RDE_LOG_LEVEL_ERROR, "save: could not put %s back as %s", _bak, _path);
        }
        return FUDE_LOAD_RECOVERED;
    }
    return (_main == FUDE_LOAD_CORRUPT || _backup == FUDE_LOAD_CORRUPT) ? FUDE_LOAD_CORRUPT : FUDE_LOAD_MISSING;
}
