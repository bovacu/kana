#ifndef KANA_NOTES
#define KANA_NOTES

#include "rde.h"

// ===========================================================================
// Notes: the canvases — infinite pages for notes, text samples, essays — and
// the folders they are kept in. The side panel lists them; one is open on
// screen at a time.
//
// FILES, under <save dir>/notes/:
//   index.kana   the list (the KANA format, kfile.h; kind 'NOTE'):
//     'NIDX'  u32 next id, u32 open canvas, u32 count, u32 record size, then per
//             note: u32 id, u32 parent, u8 kind, u8 expanded, 2 reserved,
//             u32 created lo, hi (Unix seconds), then u8 name length and the
//             name (UTF-8) — the name outside the fixed record, so a newer
//             build's longer record is skipped whole.
//   <id>.kana    each canvas's page (save.h's DOCUMENT format).
// A canvas is found by its id, so a name is anything and renaming touches no
// file.
//
// Folders nest: a canvas or a folder is at the top level or in a folder. The
// ORDER is the list's order among the notes with the same parent — folders and
// canvases mixed, as they were put. The first time, the page saved before notes
// existed (page.kana) becomes the first canvas, "Page". There is always a canvas
// open: removing the last one makes a new, empty one.
// ===========================================================================

#define KANA_NOTES_VERSION 1u
#define KANA_NOTE_NAME     64      // bytes, NUL included

typedef enum {
    KANA_NOTE_FOLDER = 0,
    KANA_NOTE_CANVAS
} KANA_NOTE_;

RDE_STRUCT {
    u32 id;                    // > 0
    u32 parent;                // its folder, 0: the top level
    u8  kind;                  // KANA_NOTE_
    b8  expanded;              // a folder shows its canvases
    u64 created;               // Unix seconds
    c8  name[KANA_NOTE_NAME];
} kana_note;

RDE_STRUCT {
    rde_arr TYPE(kana_note) notes;   // in the order they were made
    u32                     next_id;
    u32                     open;       // the canvas on screen
    u32                     revision;   // goes up on every change: whoever lists them rebuilds
} kana_notes;

void kana_notes_init(kana_notes* _notes);
void kana_notes_destroy(kana_notes* _notes);

// Reads the index (making it, and the first canvas, the first time) and saves it.
void kana_notes_load(kana_notes* _notes);
b8   kana_notes_save(kana_notes* _notes);

// Where a canvas's page is saved.
void kana_notes_canvas_path(u32 _id, c8* _out, usize _size);

const kana_note* kana_notes_find(const kana_notes* _notes, u32 _id);
// A new folder or canvas at the end of _parent (0: the top level; a folder
// otherwise): its id, 0 when it could not be made. The index is saved.
u32  kana_notes_add(kana_notes* _notes, KANA_NOTE_ _kind, u32 _parent, const c8* _name);
b8   kana_notes_rename(kana_notes* _notes, u32 _id, const c8* _name);
void kana_notes_set_expanded(kana_notes* _notes, u32 _id, b8 _expanded);
void kana_notes_open(kana_notes* _notes, u32 _id);
// Removes a canvas (its page file too) or a folder with everything in it, at any
// depth. If the open canvas goes, another opens — or a new one is made.
void kana_notes_remove(kana_notes* _notes, u32 _id);
// Moves a note into _parent (0: the top level), just before _before (a note in
// _parent; 0: at its end). False — and nothing moves — when that would put a
// folder inside itself, or _parent is not a folder. The index is saved.
b8   kana_notes_move(kana_notes* _notes, u32 _id, u32 _parent, u32 _before);
// Is _id _folder itself, or anywhere inside it?
b8   kana_notes_is_within(const kana_notes* _notes, u32 _id, u32 _folder);
// Canvases in a folder, at any depth.
u32  kana_notes_count_in(const kana_notes* _notes, u32 _folder);
// A name like "Canvas 3": the kind's word and the first number not in use.
void kana_notes_new_name(const kana_notes* _notes, KANA_NOTE_ _kind, c8* _out, usize _size);

#endif
