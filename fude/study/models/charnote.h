// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CHARNOTE
#define FUDE_CHARNOTE

#include "rde.h"

// ===========================================================================
// The learner's own note on a character: a story, a trick to remember it, a
// reminder of what it is mixed up with — written in the viewer (its Note), shown
// there under the character's details.
//
// One small file (<save dir>/charnotes.kana), written whole on every change
// (atomically, see kfile.h). Without fude_charnotes_open it lives in memory only
// (the tests).
//
// FILE ('KANA' header, kind 'CNOT'):
//   'CNTS'  u32 count, then per note: u32 the code point, u16 the text's byte
//           length and its UTF-8 bytes.
// ===========================================================================

#define FUDE_CHARNOTE_TEXT 400u   // bytes a note can have (with its NUL)

void      fude_charnotes_open(const c8* _path);
void      fude_charnotes_close(void);

// _codepoint's note ("" when it has none).
const c8* fude_charnote_get(u32 _codepoint);
// Its note set (cut at a whole character to FUDE_CHARNOTE_TEXT); "" or NULL takes
// it away. Saved.
void      fude_charnote_set(u32 _codepoint, const c8* _text);
// How many characters have one.
u32       fude_charnotes_count(void);
// Goes up on every change.
u32       fude_charnotes_revision(void);

#endif
