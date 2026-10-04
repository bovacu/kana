// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/speech.h"

// ===========================================================================
// See speech.h. Every platform but iOS (speech_ios.m) and Android (speech_android.c): no voice yet.
// ===========================================================================

RDE_INTERNAL b8 fude_speech_missing_look = false;

void fude_speech_look_missing(void) {
    fude_speech_missing_look = true;
}

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

FUDE_SPEECH_ fude_speech_state(void) {
    return fude_speech_missing_look ? FUDE_SPEECH_MISSING : FUDE_SPEECH_NONE;
}

b8 fude_speech_open_settings(void) {
    return false;
}

#endif
