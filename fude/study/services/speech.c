#include "study/services/speech.h"

// ===========================================================================
// See speech.h. Every platform but iOS (src/speech_ios.m): no voice yet.
// ===========================================================================

#if !defined(RDE_PLATFORM_IOS)

b8 fude_speech_available(void) {
    return false;
}

void fude_speak(const c8* _text) {
    RDE_UNUSED(_text);
}

void fude_speech_stop(void) {
}

b8 fude_speech_take_hint(void) {
    return false;
}

#endif
