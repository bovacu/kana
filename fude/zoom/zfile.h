// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FILE
#define FUDE_ZOOM_FILE

#include "rde.h"
#include "zoom/scene.h"
#include "drawing/base/save.h"

// ===========================================================================
// A canvas's file: Kana's tagged chunk format (kfile.h) — the same header,
// chunks, byte order, and rule that readers skip what they do not know and
// records grow at their end — WRITTEN AS A LOG. Nothing in it is ever
// overwritten: changes are appended, and every so often the whole file is
// written again, compacted.
//
//   header  "KANA" u32 version u32 kind 'ZOOM'
//   chunk*  u32 tag u32 size payload — every payload here ends in a CRC-32 of
//           the rest of it, so a chunk half written by a kill is known as such
//
//   'JRNL'  the scene's journal (scene.h's ops) since the last checkpoint:
//           appended about every second while drawing, then synced to disk
//   'BKTS'  a bucket: u64 frame id, u32 bucket, u32 count, then per object
//           u32 size and its record (scene.h). FUDE_ZOOM_BUCKET objects a
//           bucket, in the order they came; a dead-for-good object stays in
//           its bucket as a record with no points until the file is compacted
//   'FRAM'  every frame's record, parents first: u32 count, per frame u32 size
//           and its record
//   'HIST'  the undo history: u32 action_count, u32 count, per action u32 size
//           and its record — reopening a canvas keeps its undo
//   'INDX'  where everything is: u64 the FRAM's offset, u64 the HIST's, the
//           camera (u64 frame id, f64 x y z), u64 the root's id, u32 frames,
//           per frame u64 id, u32 buckets, u64 each bucket's offset, u32 its size
//   'FOOT'  u64 the INDX's offset, u32 'ZEND' — always the last chunk after a
//           CHECKPOINT (the dirty buckets, FRAM, HIST, INDX, FOOT, then a sync)
//
// OPENING reads the last good FOOT, its INDX, what that points at, then replays
// every good JRNL after it, in order, stopping at the first bad one: a kill
// at any moment loses at most what had not been appended yet. A torn end is
// cut off by compacting at once.
//
// COMPACTING writes a fresh file — only what is alive or undoable, one bucket
// set, FRAM, HIST, INDX, FOOT — to <name>.tmp, syncs it, moves the old one to
// <name>.bak and the new one into place (kfile's rule). It happens when more
// than half the file is replaced copies, and before the file leaves the app.
//
// A file that does not parse at all is kept as <name>.bad, never overwritten
// (Kana's rule), and the .bak is tried.
// ===========================================================================

#define FUDE_ZOOM_FILE_VERSION 1u

// A checkpoint is due once this much journal has been appended since the last.
#define FUDE_ZOOM_FILE_CHECKPOINT_BYTES (256u * 1024u)

typedef struct {
    c8  path[RDE_MAX_PATH];
    u64 size;            // bytes in the file: where the next chunk goes (0: no file yet)
    u64 live;            // bytes of the chunks the last INDX points at
    u64 journal_bytes;   // JRNL bytes appended since the last checkpoint
    u32 appends;         // chunks appended, ever (the HUD's)
    b8  repaired;        // opening found a torn end and compacted it away
} fude_zoom_file;

// Loads the canvas at _path into _s (fresh from fude_zoom_scene_init). On
// MISSING the scene is left as it was (empty) and the first save writes the file.
FUDE_LOAD_ fude_zoom_file_open(fude_zoom_file* _f, const c8* _path, fude_zoom_scene* _s);
// The journal appended (and synced), then a checkpoint if one is due, then a
// compaction if one is due. Nothing to do: true at once. The scene's journal is
// emptied once it is safe on disk.
b8         fude_zoom_file_flush(fude_zoom_file* _f, fude_zoom_scene* _s);
// The dirty buckets, the frames, the history and the index, appended now.
b8         fude_zoom_file_checkpoint(fude_zoom_file* _f, fude_zoom_scene* _s);
// The whole canvas written again, compacted, atomically.
b8         fude_zoom_file_compact(fude_zoom_file* _f, fude_zoom_scene* _s);

u32        fude_zoom_crc32(const u8* _data, u32 _size);

#endif
