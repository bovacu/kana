#ifndef FUDE_BACKUP
#define FUDE_BACKUP

#include "rde.h"

// ===========================================================================
// Your data: everything Kana keeps — the canvases, marks, exams, words,
// practice histories, settings: every file in the save folder — in ONE file, to
// keep a copy or to carry to another device, and back again. Kana is offline:
// this file is the only way its data leaves the device, and only when the
// learner exports it (Settings › Your data).
//
// The file (Kana's: .kanabackup, the app's info.h):
//   "KANABKP\0", u32 version (1), u32 file count, u64 when it was made (Unix
//   seconds), c8[16] the app's version — then per file: u16 path length, the path
//   (relative to the save folder, '/' between folders, no NUL), u32 size, its
//   bytes — and last a u32 FNV-1a checksum of everything before it.
//
// Left out: the .bak / .tmp / .bad copies (a backup's files are the real ones),
// before-import/ and outbox/ (below), and diagnostics (perf.txt, translate.txt,
// mlkit_samples.txt).
//
// An import replaces everything: what was in the save folder is moved to
// before-import/ first (the last import's is let go), then the backup's files
// are written. When anything fails, what was there is put back: an import
// either happens whole or not at all.
// ===========================================================================

#define FUDE_BACKUP_ASIDE     "before-import"   // in the save folder: what an import replaced
#define FUDE_BACKUP_OUTBOX    "outbox"          // in the save folder: an export waiting to be shared

RDE_STRUCT {
    u32 files;
    u64 bytes;      // of the files, together
    u64 created;    // Unix seconds
    u32 canvases;   // the pages (notes/<n>.kana)
    c8  version[16];
} fude_backup_info;

// Everything in _dir (the save folder, ending in '/') into one file at _out,
// made by the app at _version: true when written whole. _info (may be NULL)
// gets what went in.
b8 fude_backup_export(const c8* _dir, const c8* _out, const c8* _version, fude_backup_info* _info);

// Is _data a backup, whole (its checksum)? _info (may be NULL) gets what is in it.
b8 fude_backup_inspect(const u8* _data, usize _size, fude_backup_info* _info);

// _dir's files replaced by the backup's (see above). False when _data is not a
// whole backup, or a file could not be written — then _dir is as it was.
b8 fude_backup_restore(const u8* _data, usize _size, const c8* _dir);

#endif
