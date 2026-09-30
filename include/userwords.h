#ifndef KANA_USERWORDS
#define KANA_USERWORDS

#include "rde.h"
#include "kanji.h"

// ===========================================================================
// The learner's own words for a kanji: added in the viewer (Add) — picked from
// the character's longer list of JMdict words (kanji.h 'WORD'), or typed in —
// and shown with its examples, first. Exams prompt with them first too.
//
// Kept as their text (not as word numbers), so a re-bake of the character data
// cannot lose or change them. One small file (<save dir>/words.kana), written
// whole on every change (atomically, see kfile.h). Without kana_userwords_open
// they live in memory only (the tests).
//
// FILE ('KANA' header, kind 'UWRD'):
//   'WRDS'  u32 count, then per word: u32 the kanji's code point, u32 time low,
//           u32 time high (when added, Unix seconds), then its written form,
//           reading and meaning, each as u16 byte length and the UTF-8 bytes.
// ===========================================================================

#define KANA_USERWORD_WRITTEN 64u     // bytes a written form can have (with its NUL)
#define KANA_USERWORD_READING 96u
#define KANA_USERWORD_MEANING 160u

void kana_userwords_open(const c8* _path);
void kana_userwords_close(void);

// The words added to _kanji (a code point), oldest first.
u32  kana_userwords_count(u32 _kanji);
b8   kana_userwords_at(u32 _kanji, u32 _index, kana_kanji_word* _out);
// Is _written (read _reading) one of _kanji's?
b8   kana_userwords_has(u32 _kanji, const c8* _written, const c8* _reading);
// Adds it (cut to the sizes above; nothing when it is there already) and saves.
// False when it could not be saved.
b8   kana_userwords_add(u32 _kanji, const c8* _written, const c8* _reading, const c8* _meaning);
// Takes it off _kanji's words and saves.
b8   kana_userwords_remove(u32 _kanji, const c8* _written, const c8* _reading);

// Every word added, for any kanji.
u32  kana_userwords_total(void);
// Goes up on every change.
u32  kana_userwords_revision(void);

#endif
