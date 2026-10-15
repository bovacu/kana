// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SAVE
#define FUDE_SAVE

#include "rde.h"

// ===========================================================================
// Save: where the app's files are saved, and how a saved file is read back.
//
// Where it sits: drawing/base. Each kind of file belongs to its owner (a page's to ink/pagefile.h, the settings to
// app/settings.h, the study's marks, words and exams to the study), and all of them are in the KANA format (kfile.h).
// fude_app_start names the app's folder, before anything is saved or read.
// Threads: main thread. fude_save_dir is also read on zoom/listen.c's worker thread, which is safe because the folder
// is found on the main thread at start and only changes if fude_save_set_folder is called again.
// Files: every file is written atomically (kfile.h) and read back from the main file, else from its .bak (the
// previous save) when the main one is missing or damaged. A file that fails to parse is renamed to <name>.bad (the
// same file damaged again: <name>.2.bad, kfile.h) and never overwritten.
// Design: a damaged file is set aside rather than replaced, so a bug in a reader cannot destroy a page by autosaving
// an empty one over it.
// ===========================================================================

// How a load went.
typedef enum {
    FUDE_LOAD_OK = 0,
    FUDE_LOAD_RECOVERED,  // the main file was missing or damaged, so the .bak was loaded instead
    FUDE_LOAD_MISSING,    // nothing has been saved yet
    FUDE_LOAD_CORRUPT     // there were files but none parsed; each was set aside as .bad
} FUDE_LOAD_;

// fude_save_dir returns the folder the app saves into, ending in '/'. The folder is created if it does not exist
// yet. On iOS it is in the app's Application Support (private to the app, and included in device backups), on
// Android in its internal storage, in a folder of its own that fude_save_set_folder names (the app's id, from
// info.h; fude_app_start sets it, before anything is saved or read). On desktop it is ./saves/ under the
// working directory. The returned string is this module's and stays valid; it changes only when
// fude_save_set_folder names another folder (a NULL or empty name is ignored; one too long for
// FUDE_SAVE_FOLDER_MAX is logged and cut).
const c8*  fude_save_dir(void);
void       fude_save_set_folder(const c8* _name);
// The room for the folder's name, the app's id, in bytes with its NUL. A fixed size is safe: an id is a short word the
// app's description gives (info.h), never anything the person typed, and it names a folder and a backup's file
// extension, so 63 bytes is far more than any app needs and well within a file name's 255.
#define FUDE_SAVE_FOLDER_MAX 64u

// Returns the folder for what was downloaded and can be downloaded again (InfiNote's speech model, tens of MB), ending
// in '/', and creates it on first use: downloads/ in the platform's cache folder for the app (Android's cache, iOS's
// Library/Caches; ./downloads/ on a computer). No backup takes it in, neither the device's (iCloud's, or
// Google's, which keeps at most 25 MB of an app) nor Your data's, since it is nobody's work; and the system may empty
// it when space runs short, so whatever is kept there is fetched again when it is missing. The string is this module's.
const c8*  fude_save_downloads_dir(void);

// Writes into _dir the path of a kind's own folder under the save folder (_name, such as "notes"), ending in '/', and
// creates the folder if missing. _dir is the caller's to keep (usually a static): the folder is made only when the
// path there differs from the current one, so calling this every time is cheap, and it follows the save folder
// should that change (the app's own is set before anything is saved or read, in fude_app_start).
void       fude_save_subdir(const c8* _name, c8* _dir, usize _size);

// Reads one file's bytes, its header included, into what _user says. It must start from nothing each time, because
// it is called again for the backup when the main file fails. Returns true when the file was read; false means the
// file is damaged and gets set aside.
typedef b8 (*fude_save_reader_fn)(const u8* _data, u32 _size, any _user);
// Reads a saved file back by the rule above: the main file at _path, else <_path>.bak when the main one is missing or
// does not read (a damaged one is set aside as <name>.bad first). A backup that was read is copied back as the main
// file, so the next save adds to what was recovered. Returns OK, RECOVERED (the backup was read), MISSING (neither
// file is there) or CORRUPT.
FUDE_LOAD_ fude_save_load(const c8* _path, fude_save_reader_fn _read, any _user);

#endif
