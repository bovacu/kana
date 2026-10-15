// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_NOTES
#define FUDE_NOTES

#include "rde.h"

// ===========================================================================
// Notes: the canvases (infinite pages for notes, text samples, essays) and the folders they are kept in. The side
// panel lists them, and one canvas is open on screen at a time.
//
// Where it sits: drawing/ink. The session (app/session.c) loads the list at start and opens the chosen canvas; the
// side panel adds, renames, moves and removes notes; documents (doc.h) set what is under a canvas's ink.
// Its main types and their lifecycle: fude_notes is the caller's struct; fude_notes_init gives it its list (on the
// standard heap) and fude_notes_destroy frees it. Each fude_note lives in that list.
// Threads: main thread only.
// Files, under <save folder>/notes/:
//   index.kana   the list (the KANA format, kfile.h; kind 'NOTE'), in one chunk:
//     'NIDX'  u32 next id, u32 open canvas, u32 count, u32 record size, then per note: u32 id, u32 parent, u8 kind,
//             u8 expanded, u8 document (what is under a canvas's ink: see fude_note), 1 reserved byte, u32 created
//             low half, u32 created high half (Unix seconds), then u8 name length and the name (UTF-8). The name is
//             outside the fixed record, so a newer build's longer record is skipped whole.
//   <id>.kana    each canvas's page (pagefile.h's DOCUMENT kind).
//   <id>.pdf     a canvas's own document (FUDE_NOTE_DOCUMENT_OWN): the PDF under its ink, imported or made from
//                pictures (doc.h).
// Removing a canvas deletes its page (with its .bak and .tmp copies), its own document and that document's read text
// (<id>.lines), and a deep-zoom canvas's file (<id>.zoom and its copies, zoom/zfile.h).
// Design: a canvas is found by its id, so a name can be anything, and renaming touches no file. Folders nest: a
// canvas or a folder is at the top level or in a folder. The ORDER is the list's order among the notes with the same
// parent, folders and canvases mixed, as they were put. The first time, the page saved before notes existed
// (page.kana) becomes the first canvas, "Page". There is always a canvas open: removing the last one makes a new,
// empty one. Every change saves the index. What a damaged index gets wrong is repaired as it loads: a note whose
// folder is gone goes to the top level, a folder inside itself is taken out, and the open note is always a canvas.
// ===========================================================================

#define FUDE_NOTES_VERSION 1u
#define FUDE_NOTE_NAME     64      // a name's room in bytes, the NUL included

// What a note is.
typedef enum {
    FUDE_NOTE_FOLDER = 0,
    FUDE_NOTE_CANVAS
} FUDE_NOTE_;

#define FUDE_NOTE_DOCUMENT_OWN 1u   // fude_note.document for the canvas's own PDF; 2 and up are the library's books

// One note: a folder or a canvas.
RDE_STRUCT {
    u32 id;                    // never 0
    u32 parent;                // its folder, or 0 for the top level
    u8  kind;                  // FUDE_NOTE_
    b8  expanded;              // a folder's contents are shown in the list
    u8  document;              // a canvas's: what is under its ink. 0 is nothing, FUDE_NOTE_DOCUMENT_OWN its own
                               // <id>.pdf, anything else a book of the app's library, by its id (doc.h: fude_doc_book)
    u64 created;               // when it was made, in Unix seconds
    c8  name[FUDE_NOTE_NAME];
} fude_note;

// The list of notes, and which canvas is open.
RDE_STRUCT {
    rde_arr TYPE(fude_note) notes;   // in list order: among the notes with the same parent, the order they show in
    u32                     next_id;
    u32                     open;       // the id of the canvas on screen
    u32                     revision;   // goes up on every change, so whoever lists the notes builds the list again
} fude_notes;

// fude_notes_init sets up an empty list; fude_notes_destroy frees it.
void fude_notes_init(fude_notes* _notes);
void fude_notes_destroy(fude_notes* _notes);

// fude_notes_load reads the index as every save is read (save.h: from its .bak when the index is missing or damaged,
// a damaged one set aside as .bad), repairs it, and saves it. The first time, it makes the index and the first
// canvas. When neither file can be read, the list is made again from the canvas files in the folder, each at the
// top under a new name, so no canvas is lost and no id is used twice. fude_notes_save writes the index, returning
// false when it could not be written.
void fude_notes_load(fude_notes* _notes);
b8   fude_notes_save(fude_notes* _notes);

// Writes into _out the path of a canvas's own document (<id>.pdf, FUDE_NOTE_DOCUMENT_OWN).
void fude_notes_document_path(u32 _id, c8* _out, usize _size);
// Sets what is under a canvas's ink (fude_note.document), saving the index when it changed. Folders are ignored.
void fude_notes_set_document(fude_notes* _notes, u32 _id, u8 _document);
// Writes into _out the path a canvas's page is saved at (<id>.kana).
void fude_notes_canvas_path(u32 _id, c8* _out, usize _size);

// Returns the note with _id, or NULL when there is none. The pointer stays valid until a note is added, moved or
// removed.
const fude_note* fude_notes_find(const fude_notes* _notes, u32 _id);
// Adds a new folder or canvas at the end of _parent (0 for the top level, otherwise a folder, which opens to show
// it). _name is trimmed and cut to fit; an empty one becomes the kind's word. Returns the new note's id, never 0: there
// is no limit on how many notes there are. The index is saved.
u32  fude_notes_add(fude_notes* _notes, FUDE_NOTE_ _kind, u32 _parent, const c8* _name);
// Renames a note (trimmed and cut to fit; an empty name keeps the old one) and saves the index. Returns false when
// there is no such note, or the index could not be saved.
b8   fude_notes_rename(fude_notes* _notes, u32 _id, const c8* _name);
// Shows or hides a folder's contents in the list, saving the index when it changed.
void fude_notes_set_expanded(fude_notes* _notes, u32 _id, b8 _expanded);
// Makes canvas _id the one on screen, saving the index when it changed. Anything but a canvas is ignored.
void fude_notes_open(fude_notes* _notes, u32 _id);
// Removes a canvas (its files too) or a folder with everything in it, at any depth. If the open canvas goes, another
// opens, or a new one is made. The index is saved.
void fude_notes_remove(fude_notes* _notes, u32 _id);
// Moves a note into _parent (0 for the top level), just before _before (a note in _parent; 0 for its end). Returns
// false, and nothing moves, when that would put a folder inside itself, _parent is not a folder, or there is no such
// note. The folder it lands in opens, and the index is saved.
b8   fude_notes_move(fude_notes* _notes, u32 _id, u32 _parent, u32 _before);
// Returns whether _id is _folder itself, or anywhere inside it.
b8   fude_notes_is_within(const fude_notes* _notes, u32 _id, u32 _folder);
// Returns how many canvases are inside a folder, at any depth. It walks the whole list, so a caller asking it of every
// folder uses a tree instead (fude_notes_tree).
u32  fude_notes_count_in(const fude_notes* _notes, u32 _folder);
// Writes a name like "Canvas 3" into _out: the kind's word, in the UI's language, and the first number not in use.
void fude_notes_new_name(const fude_notes* _notes, FUDE_NOTE_ _kind, c8* _out, usize _size);

// The list as a tree, for whoever walks it whole (the side panel's list): each folder's notes in list order, and how
// many canvases each folder holds at any depth, made in one pass instead of searching the list once per folder. Indices
// are into fude_notes.notes, and the top level is the index one past the last note (the list's length). It is a copy
// of the list as it was, so it is made again after the list changes (its revision). A note whose folder is not in the
// list is at the top level, where the load's repair puts it.
RDE_STRUCT {
    rde_arr TYPE(u32) first;      // per note, then the top level: where the notes in it start in child...
    rde_arr TYPE(u32) end;        // ...and where they end
    rde_arr TYPE(u32) child;      // every note's index, grouped by its folder, in list order within each group
    rde_arr TYPE(u32) canvases;   // per note, then the top level: how many canvases are inside it, at any depth
} fude_notes_tree;

// fude_notes_tree_make fills _tree from the list, on the std allocator; the caller frees it with fude_notes_tree_free.
// Main thread, as the list is.
void fude_notes_tree_make(const fude_notes* _notes, fude_notes_tree* _tree);
void fude_notes_tree_free(fude_notes_tree* _tree);

#endif
