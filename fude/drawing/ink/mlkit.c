// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/mlkit.h"

// ===========================================================================
// See mlkit.h.
//
// This file holds the switch and the model's tag on every platform. On every platform other than an iOS device and
// Android it also holds the stand-ins, which report ML Kit as unavailable; mlkit_ios.m and mlkit_android.c hold the
// real calls, and are built only on those platforms.
// ===========================================================================

// The Settings switch: whether ML Kit may be used at all. On by default.
static b8 fude_mlkit_on = true;

// Turns the switch on or off.
void fude_mlkit_set_enabled(b8 _enabled) {
    fude_mlkit_on = _enabled;
}

// Returns whether the switch is on.
b8 fude_mlkit_enabled(void) {
    return fude_mlkit_on;
}

static c8 fude_mlkit_tag[32] = "en-US";   // Until the app sets one; ML Kit's tags are short ("ja", "pt-BR").

// Sets the model's language tag, cut to 31 bytes; NULL or "" leave it as it is.
void fude_mlkit_set_model(const c8* _tag) {
    if(_tag != NULL && _tag[0] != 0) {
        rde_snprintf(fude_mlkit_tag, sizeof(fude_mlkit_tag), "%s", _tag);
    }
}

// Returns the model's language tag.
const c8* fude_mlkit_model(void) {
    return fude_mlkit_tag;
}

#if (!defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)) && !defined(RDE_PLATFORM_ANDROID)   // No ML Kit

// The stand-ins, for where ML Kit has no build (the desktop and the iOS Simulator): it is always unavailable.

// Returns FUDE_MLKIT_UNAVAILABLE.
FUDE_MLKIT_ fude_mlkit_state(void) {
    return FUDE_MLKIT_UNAVAILABLE;
}

// Does nothing.
void fude_mlkit_prepare(void) {
}

// Never starts a reading: returns false.
b8 fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context) {
    RDE_UNUSED(_ink);
    RDE_UNUSED(_pre_context);
    return false;
}

// Never has an answer: returns false.
b8 fude_mlkit_poll(c8* _out, usize _size, f64* _ms) {
    RDE_UNUSED(_out);
    RDE_UNUSED(_size);
    RDE_UNUSED(_ms);
    return false;
}

#endif
