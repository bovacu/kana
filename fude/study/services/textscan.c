// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/textscan.h"

// ===========================================================================
// See textscan.h. The stand-ins for every platform but an iOS device and Android (textscan_ios.m and
// textscan_android.c, built only there), where there is no reading of text in pictures yet. Every call refuses.
// ===========================================================================

// ML Kit runs on a device only (iOS, and Android in textscan_android.c), not in the iOS Simulator.
#if (!defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)) && !defined(RDE_PLATFORM_ANDROID)   // Stand-ins

// Nothing can be read here.
b8 fude_textscan_available(void) {
    return false;
}

// No photo library to open.
b8 fude_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    return false;
}

// Nothing is ever picked, so it stays idle.
FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return FUDE_TEXTSCAN_IDLE;
}

// A frame cannot be read here.
b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    RDE_UNUSED(_rgba);
    RDE_UNUSED(_width);
    RDE_UNUSED(_height);
    RDE_UNUSED(_rotation);
    return false;
}

// No frame's reading ever comes in.
b8 fude_textscan_poll_frame(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return false;
}

// A page cannot be read here.
b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    RDE_UNUSED(_rgba); RDE_UNUSED(_width); RDE_UNUSED(_height);
    return false;
}

// No page's reading ever comes in.
b8 fude_textscan_poll_page(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return false;
}

#endif
