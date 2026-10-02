#include "rde.h"

// The word card (wordcard.h): a word tapped only asks for it; the test reads what.
#include "widgets/wordcard.h"
const c8* scantest_card_asked = NULL;
void kana_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) { (void)_reading; (void)_meaning; (void)_kanji; scantest_card_asked = _written; }
