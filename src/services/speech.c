#include "services/speech.h"

// ===========================================================================
// See speech.h. Every platform but iOS (src/speech_ios.m): no voice yet.
// ===========================================================================

#if !defined(RDE_PLATFORM_IOS)

b8 kana_speech_available(void) {
    return false;
}

void kana_speak(const c8* _text) {
    RDE_UNUSED(_text);
}

void kana_speech_stop(void) {
}

b8 kana_speech_take_hint(void) {
    return false;
}

#endif
