#ifndef KANA_CHARNOTE
#define KANA_CHARNOTE

#include "rde.h"

// ===========================================================================
// The learner's own note on a character: a story, a trick to remember it, a
// reminder of what it is mixed up with — written in the viewer (its Note), shown
// there under the character's details.
//
// One small file (<save dir>/charnotes.kana), written whole on every change
// (atomically, see kfile.h). Without kana_charnotes_open it lives in memory only
// (the tests).
//
// FILE ('KANA' header, kind 'CNOT'):
//   'CNTS'  u32 count, then per note: u32 the code point, u16 the text's byte
//           length and its UTF-8 bytes.
// ===========================================================================

#define KANA_CHARNOTE_TEXT 400u   // bytes a note can have (with its NUL)

void      kana_charnotes_open(const c8* _path);
void      kana_charnotes_close(void);

// _codepoint's note ("" when it has none).
const c8* kana_charnote_get(u32 _codepoint);
// Its note set (cut at a whole character to KANA_CHARNOTE_TEXT); "" or NULL takes
// it away. Saved.
void      kana_charnote_set(u32 _codepoint, const c8* _text);
// How many characters have one.
u32       kana_charnotes_count(void);
// Goes up on every change.
u32       kana_charnotes_revision(void);

#endif
