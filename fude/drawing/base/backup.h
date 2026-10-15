// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_BACKUP
#define FUDE_BACKUP

#include "rde.h"

// ===========================================================================
// Backup ("Your data"): everything the app keeps, in ONE file, to keep a copy or to carry to another device, and to
// read back. That is every file in the save folder: the canvases, marks, exams, words, practice histories and settings.
//
// Where it sits: drawing/base. The session (app/session.c) exports, inspects and restores backups for Settings ›
// Your data. The apps work offline, so this file is the only way their data leaves the device, and only when the
// learner exports it.
// Its main types and their lifecycle: fude_backup_info is a plain summary the caller owns. A backup being read is the
// caller's bytes, which these functions only read.
// Threads: main thread only.
// Files: the backup file (its extension is the app's, from its info.h: Kana's is .kanabackup). Every number is
// little-endian:
//   "KANABKP\0", u32 version (1), u32 file count, u64 when it was made (Unix seconds), c8[16] the app's version;
//   then per file: u16 path length, the path (relative to the save folder, '/' between folders, no NUL), u32 size,
//   its bytes;
//   and last a u32 FNV-1a checksum of everything before it.
// Left out of a backup: the .bak, .tmp and .bad copies (a backup holds only the real files), hidden files (a name
// starting with '.'), the before-import/ and outbox/ folders (below), the crash report's two files (this device's:
// crash.h), and diagnostics (perf.txt, translate.txt, mlkit_samples.txt). An import leaves the crash files where they
// are, too. What was downloaded is not in the save folder at all (save.h: fude_save_downloads_dir).
// Design: an import replaces everything. What was in the save folder is moved to before-import/ first (the previous
// import's leftovers there are deleted), then the backup's files are written. When anything fails, what was written
// is deleted and what was moved comes back, so an import either happens whole or not at all. A path in a backup is
// written only when it stays inside the save folder (no "..", no '\\' or ':'), so a crafted file cannot write
// elsewhere. The checksum stays FNV-1a, not RDE's hash, because every backup ever written uses it.
// ===========================================================================

#define FUDE_BACKUP_ASIDE     "before-import"   // a folder in the save folder: what the last import replaced
#define FUDE_BACKUP_OUTBOX    "outbox"          // a folder in the save folder: an export waiting to be shared

// What a backup holds, as export and inspect report it.
RDE_STRUCT {
    u32 files;
    u64 bytes;      // the files' sizes added together
    u64 created;    // when the backup was made, in Unix seconds
    u32 canvases;   // how many canvas pages it holds (notes/<n>.kana)
    // The version of the app that made it, NUL-terminated.
    c8  version[16];
} fude_backup_info;

// Writes every file under _dir (the save folder, ending in '/') into one backup file at _out, recording _version as
// the app version that made it. Returns true when the file was written whole; on failure the partial file is deleted
// (a file over 2 GiB, or a path over 255 bytes, also fails it). _info (may be NULL) gets what went in.
b8 fude_backup_export(const c8* _dir, const c8* _out, const c8* _version, fude_backup_info* _info);

// Returns true when _data (_size bytes) is a whole backup: its header, its layout and its checksum hold, and every
// path in it is safe to write. _info (may be NULL) gets what is in it.
b8 fude_backup_inspect(const u8* _data, usize _size, fude_backup_info* _info);

// Replaces _dir's files with the backup's, as described above. Returns false when _data is not a whole backup, or a
// file could not be moved aside or written; _dir is then as it was.
b8 fude_backup_restore(const u8* _data, usize _size, const c8* _dir);

#endif
