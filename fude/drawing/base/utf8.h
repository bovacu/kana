// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_UTF8
#define FUDE_UTF8

#include "rde.h"

// ===========================================================================
// UTF-8: reading and writing text one code point at a time, which is how text in any script is handled here.
//
// Where it sits: drawing/base. Used wherever text is walked or cut: drawing and wrapping text, names, searches, the
// study's characters.
// Threads: any thread. The functions keep no state.
// ===========================================================================

// fude_utf8_next returns the next code point of a UTF-8 string and moves *_s past it. Returns 0 at the end of the
// string. A malformed byte is skipped on its own and returned as U+FFFD: a byte out of place, or a sequence that is
// not well formed (an overlong form, a surrogate, past U+10FFFF), whose lead byte alone is taken. fude_utf8_decode
// reads the same code point at _s without moving, its byte count into *_len (0 at the end, 1 for a malformed byte).
u32  fude_utf8_next(const c8** _s);
u32  fude_utf8_decode(const c8* _s, u32* _len);
// Writes a code point's UTF-8 into _out (room for at least 5 bytes), NUL-terminated. Returns how many bytes it took,
// 1 to 4, the NUL not counted.
u32  fude_utf8_put(u32 _codepoint, c8* _out);
// fude_utf8_copy copies _s's first _len bytes into _out (_size bytes, NUL-terminated). When they do not fit, the text
// is cut before a whole character, never in the middle of one. fude_utf8_set copies the whole of _s the same way
// (NULL copies as ""). A _size of 0 writes nothing.
void fude_utf8_copy(c8* _out, usize _size, const c8* _s, usize _len);
void fude_utf8_set(c8* _out, usize _size, const c8* _s);
// Returns how many characters (code points) a UTF-8 string has, by counting the bytes that start one.
u32  fude_utf8_count(const c8* _s);
// Returns whether a code point is a Han character (a kanji, a hanzi, a hanja): a CJK ideograph of the main block, of
// its extensions (A, and B onwards in the supplementary planes), or of the compatibility forms.
b8   fude_utf8_is_han(u32 _codepoint);
// Returns a Latin letter the way a romanization is searched: lower case, without its accent (ǎ ā ṭ → a a t;
// ɛ ɔ ʉ ə → e o u e). Returns 0 for a combining accent or a modifier letter such as ʿ ʾ, which a search leaves out.
// Anything else comes back as it is.
u32  fude_utf8_latin_base(u32 _codepoint);
// Searching text the way the PDF searches compare it (PDFKit's, and FudePdfBox's fold on Android), so a document's
// read lines are found as its own text is. fude_utf8_search_fold returns a character as a search compares it: a
// full-width form (Ａ ！ ０) as its ASCII, the ideographic space as a space, then a Latin letter without its case or
// accent (fude_utf8_latin_base), and 0 for a combining accent or a modifier letter, which a search leaves out. Other
// scripts' capitals and half-width kana are compared as they are. fude_utf8_find looks for _needle in _hay from byte
// _from on, comparing folded characters; it returns the byte where the match starts and puts the byte just past it
// (the accents after its last letter included) into *_end. It returns -1 when there is no match, or _needle has
// nothing to compare.
u32  fude_utf8_search_fold(u32 _codepoint);
i32  fude_utf8_find(const c8* _hay, const c8* _needle, usize _from, usize* _end);

#endif
