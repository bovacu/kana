// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CHARNOTE
#define FUDE_CHARNOTE

#include "rde.h"
#include "study/models/vocab.h"   // FUDE_USERWORD_CHAR_BYTES

// ===========================================================================
// Character notes: the learner's own note on a character, such as a story, a trick to remember it, or a reminder of
// what it gets mixed up with. The learner writes it from the viewer's Note button and it is shown in the viewer, under
// the character's details.
//
// Where it sits: a study model (study/models). The word card's note form (wordcard.h) writes the notes, the viewer
// (viewer.h) reads them, and the study's session opens the file (study/app/files.c).
// Its main types and their lifecycle: there is no object; the notes are one set held by this module.
// fude_charnotes_open loads them when the session opens and fude_charnotes_close frees them when the app ends.
// Without fude_charnotes_open the notes live in memory only, which is what the tests use.
// Threads: main thread only.
// Files: one small file, <save dir>/charnotes.kana, in the KANA format (kfile.h), written whole and atomically on
// every change and read back with fude_save_load (save.h: the main file, else its .bak). Its layout:
//   header kind 'CNOT', then one chunk:
//   'CNTS'  u32 count, then per note: u32 the code point, u16 the text's length in bytes, and its UTF-8 bytes.
// Design: a few hundred notes at most, so they are kept as an array sorted by code point and the whole file is
// rewritten on each change; that is simpler than appending and costs nothing at this size.
// ===========================================================================

// The most characters a note can have, as the word card lets one be typed (wordcard.c), and its bytes, its NUL
// included, at FUDE_USERWORD_CHAR_BYTES a character (vocab.h says why), so that what was typed is kept whole.
#define FUDE_CHARNOTE_CHARS 180u
#define FUDE_CHARNOTE_TEXT  (FUDE_USERWORD_CHAR_BYTES * FUDE_CHARNOTE_CHARS + 1u)

// Loads the notes saved at _path, and saves there from now on. When nothing has been saved yet there are no notes.
// A NULL _path keeps the notes in memory only.
void      fude_charnotes_open(const c8* _path);
// Forgets every note and the path, and frees their memory. Nothing is saved.
void      fude_charnotes_close(void);

// The note on _codepoint, or "" when it has none. The pointer is the module's and stays valid until the notes next
// change (fude_charnote_set, open or close).
const c8* fude_charnote_get(u32 _codepoint);
// Sets the note on _codepoint and saves the file. The spaces and line breaks around the text are dropped, and a long
// text is cut at a whole character to fit FUDE_CHARNOTE_TEXT. An empty text (or NULL) takes the note away. A file
// that cannot be written is logged; the note is kept in memory all the same.
void      fude_charnote_set(u32 _codepoint, const c8* _text);
// How many characters have a note.
u32       fude_charnotes_count(void);
// A number that goes up on every change, so what shows the notes knows when to look again.
u32       fude_charnotes_revision(void);

#endif
