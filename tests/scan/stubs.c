#include "rde.h"

// The word card (wordcard.h): a word tapped only asks for it; the test reads what.
#include "study/widgets/wordcard.h"
const c8* scantest_card_asked = NULL;
void fude_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) { (void)_reading; (void)_meaning; (void)_kanji; scantest_card_asked = _written; }
void fude_wordcard_ask_in(const c8* _written, const c8* _reading, const c8* _meaning, const c8* _sentence, const c8* _translation) { (void)_sentence; (void)_translation; fude_wordcard_ask(_written, _reading, _meaning, 0u); }
