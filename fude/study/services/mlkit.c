// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/mlkit.h"

// ===========================================================================
// See mlkit.h. The switch, everywhere; and every platform but iOS and Android:
// no ML Kit (mlkit_ios.m and mlkit_android.c, in those builds only).
// ===========================================================================

static b8 fude_mlkit_on = true;

void fude_mlkit_set_enabled(b8 _enabled) {
    fude_mlkit_on = _enabled;
}

b8 fude_mlkit_enabled(void) {
    return fude_mlkit_on;
}

#if (!defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)) && !defined(RDE_PLATFORM_ANDROID)   // ML Kit: a device's (iOS, Android: *_android.c), not the Simulator's

FUDE_MLKIT_ fude_mlkit_state(void) {
    return FUDE_MLKIT_UNAVAILABLE;
}

void fude_mlkit_prepare(void) {
}

b8 fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context) {
    RDE_UNUSED(_ink);
    RDE_UNUSED(_pre_context);
    return false;
}

b8 fude_mlkit_poll(c8* _out, usize _size, f64* _ms) {
    RDE_UNUSED(_out);
    RDE_UNUSED(_size);
    RDE_UNUSED(_ms);
    return false;
}

#endif
