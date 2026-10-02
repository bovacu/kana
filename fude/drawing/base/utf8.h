#ifndef FUDE_UTF8
#define FUDE_UTF8

#include "rde.h"

// ===========================================================================
// UTF-8, a code point at a time: what text in any script is read and written as.
// ===========================================================================

// The next code point of a UTF-8 string, advancing *_s past it. 0 at the end; a
// malformed byte is skipped (U+FFFD).
u32  fude_utf8_next(const c8** _s);
// A code point's UTF-8 into _out (at least 5 bytes), NUL-terminated.
void fude_utf8_put(u32 _codepoint, c8* _out);

#endif
