#include "study/services/speech.h"

// ===========================================================================
// See speech.h. Every platform but iOS (speech_ios.m) and Android (speech_android.c): no voice yet.
// ===========================================================================

#if !defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_ANDROID)   // Android: speech_android.c

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
