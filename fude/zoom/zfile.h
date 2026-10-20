// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FILE
#define FUDE_ZOOM_FILE

#include "rde.h"
#include "zoom/scene.h"
#include "drawing/base/save.h"

// ===========================================================================
// Canvas file: how a canvas is kept on disk. It uses the KANA tagged chunk format (kfile.h): the same header, chunks
// and byte order, and the same rules that readers skip what they do not know and records only grow at their end. But
// it is WRITTEN AS A LOG: nothing in it is ever overwritten. Changes are appended, and every so often the whole file
// is written again, compacted.
//
// Where it sits: fude/zoom, over scene.h. The page opens the canvas with it, flushes it about every second while
// drawing and checkpoints it when the canvas is left (page_file.c), and peeks into other canvases for Find
// (page_find.c).
// Its main types and their lifecycle: fude_zoom_file is a plain value the page keeps beside its scene; it holds the
// path and where the file stands, and has nothing to free.
// Threads: main thread only. fude_zoom_crc32 builds its table on first use without a lock.
// Files: <name> (the canvas: page_file.c names it <id>.zoom), with <name>.tmp, <name>.bak and <name>.bad as below:
//
//   header  "KANA" u32 version u32 kind 'ZOOM'
//   chunk*  u32 tag u32 size payload. Every payload here ends in a CRC-32 of the rest of it, so a chunk half written
//           when the app was killed is known as such.
//
//   'JRNL'  the scene's journal (scene.h's ops) since the last checkpoint: appended about every second while drawing,
//           then synced to disk
//   'BKTS'  a bucket: u64 frame id, u32 bucket, u32 count, then for each object u32 size and its record (scene.h).
//           A bucket holds FUDE_ZOOM_BUCKET objects, in the order they came. An object dead for good stays in its
//           bucket as a record with no points until the file is compacted.
//   'FRAM'  every frame's record, parents first: u32 count, then for each frame u32 size and its record
//   'HIST'  the undo history: u32 action_count, u32 count, then for each action u32 size and its record, so
//           reopening a canvas keeps its undo
//   'INDX'  where everything is: u64 the FRAM's offset, u64 the HIST's, the camera (u64 frame id, f64 x y z), u64
//           the root's id, u32 frames, then for each frame u64 id, u32 buckets, and for each bucket u64 its offset
//           and u32 its size
//   'FOOT'  u64 the INDX's offset, u32 'ZEND'. It is always the last chunk of a CHECKPOINT, which writes the dirty
//           buckets, FRAM, HIST, INDX and FOOT, then syncs.
//
// Design:
//   - OPENING reads the last good FOOT, its INDX and what that points at, then replays every good JRNL after it, in
//     order, stopping at the first bad one. So a kill at any moment loses at most what had not been appended yet. A
//     torn end is cut off by compacting at once.
//   - COMPACTING writes a fresh file (only what is alive or can be undone: one set of buckets, FRAM, HIST, INDX,
//     FOOT) to <name>.tmp, syncs it, moves the old file to <name>.bak and the new one into place (kfile.h's rule). It
//     happens when the bytes no index points at (old copies, old journals) are more than the live ones and more than
//     64 KB (zfile.c's FUDE_ZOOM_FILE_COMPACT_MIN), the first time a canvas is written, after a failed write, and when
//     opening found a torn end or read the backup. Nothing compacts a file before it leaves the app: Your data
//     (backup.h) copies it as it is, which opens as a compacted one does.
//   - A file that does not parse at all is kept as <name>.bad and never overwritten (kfile.h's rule), and the .bak is
//     tried instead.
// ===========================================================================

// The version written in the file's header.
#define FUDE_ZOOM_FILE_VERSION 1u

// A checkpoint is due once this much journal has been appended since the last.
#define FUDE_ZOOM_FILE_CHECKPOINT_BYTES (256u * 1024u)

// An open canvas file: its path and where it stands.
typedef struct {
    c8  path[RDE_MAX_PATH];
    u64 size;            // the bytes in the file, which is where the next chunk goes (0: no file yet, or one a failed
                         // write may have left a fragment in: the next save writes the whole file again)
    u64 live;            // the bytes of the chunks the last INDX points at
    u64 journal_bytes;   // the JRNL bytes appended since the last checkpoint
    u32 appends;         // how many times anything was appended or the file compacted (shown on the HUD)
    b8  repaired;        // opening found a torn end and compacted it away
} fude_zoom_file;

// Loads the canvas at _path into _s, which must be fresh from fude_zoom_scene_init. Returns OK (a torn end is
// compacted away at once, and repaired says so), RECOVERED (the file did not parse: it was set aside as .bad and the
// .bak was loaded and written as the new file), MISSING (no file: the scene is left empty, and the first save writes
// the file) or CORRUPT (neither the file nor its .bak parsed: the scene is left empty).
FUDE_LOAD_ fude_zoom_file_open(fude_zoom_file* _f, const c8* _path, fude_zoom_scene* _s);
// Like fude_zoom_file_open, but only reads, never writes: for a canvas looked through without being opened (Find's
// words, words.h). A torn end is left as it is, and a damaged file is not set aside. Returns OK, or RECOVERED when the
// end was torn and what was whole was read. Any other result (MISSING, CORRUPT) means nothing usable was read, and
// the scene may be half filled: the caller throws it away.
FUDE_LOAD_ fude_zoom_file_peek(const c8* _path, fude_zoom_scene* _s);
// Appends the scene's journal to the file (and syncs it), then makes a checkpoint if one is due, then compacts the
// file if that is due. With no file yet, the whole file is written once there is something in it. The scene's
// journal is emptied once it is safe on disk. Returns false when a write failed; with nothing to do, true at once.
// After a failed append the next flush writes the whole file again (fude_zoom_file_compact), so a fragment the failed
// write may have left is never built on.
b8         fude_zoom_file_flush(fude_zoom_file* _f, fude_zoom_scene* _s);
// Appends a checkpoint now: the dirty buckets, the frames, the history, the index and the footer. With no file yet,
// writes the whole file instead. Returns false when the write failed; the next save then writes the whole file.
b8         fude_zoom_file_checkpoint(fude_zoom_file* _f, fude_zoom_scene* _s);
// Writes the whole canvas again, compacted, atomically (through <name>.tmp and <name>.bak). Returns false when it
// could not; the next save then writes everything again.
b8         fude_zoom_file_compact(fude_zoom_file* _f, fude_zoom_scene* _s);

// Returns the CRC-32 of _size bytes at _data (the zip one: polynomial 0xEDB88320).
u32        fude_zoom_crc32(const u8* _data, u32 _size);

#endif
