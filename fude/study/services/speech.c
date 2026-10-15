// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/speech.h"

// ===========================================================================
// See speech.h. The stand-ins for every platform but iOS (speech_ios.m) and Android (speech_android.c), where there
// is no voice yet. Only fude_speech_look_missing is built everywhere; it has an effect only here.
// ===========================================================================

// Set by fude_speech_look_missing: the voice shows as missing instead of as none, for a developer's look.
RDE_INTERNAL b8 fude_speech_missing_look = false;

// See the header.
void fude_speech_look_missing(void) {
    fude_speech_missing_look = true;
}

#if !defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_ANDROID)   // iOS: speech_ios.m; Android: speech_android.c.

// No voice here.
b8 fude_speech_available(void) {
    return false;
}

// Nothing to read with.
void fude_speak(const c8* _text) {
    RDE_UNUSED(_text);
}

// Nothing is ever being read.
void fude_speech_stop(void) {
}

// There are no better voices to point to here.
b8 fude_speech_take_hint(void) {
    return false;
}

// None, unless a developer asked to see the voice as missing.
FUDE_SPEECH_ fude_speech_state(void) {
    return fude_speech_missing_look ? FUDE_SPEECH_MISSING : FUDE_SPEECH_NONE;
}

// No settings screen to open here.
b8 fude_speech_open_settings(void) {
    return false;
}

#endif
