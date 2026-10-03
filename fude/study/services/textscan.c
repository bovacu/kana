#include "study/services/textscan.h"

// ===========================================================================
// See textscan.h. Every platform but iOS and Android (textscan_ios.m and
// textscan_android.c, in those builds only): no text in photos yet.
// ===========================================================================

#if (!defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)) && !defined(RDE_PLATFORM_ANDROID)   // ML Kit: a device's (iOS, Android: *_android.c), not the Simulator's

b8 fude_textscan_available(void) {
    return false;
}

b8 fude_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    return false;
}

FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return FUDE_TEXTSCAN_IDLE;
}

b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    RDE_UNUSED(_rgba);
    RDE_UNUSED(_width);
    RDE_UNUSED(_height);
    RDE_UNUSED(_rotation);
    return false;
}

b8 fude_textscan_poll_frame(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return false;
}

b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    RDE_UNUSED(_rgba); RDE_UNUSED(_width); RDE_UNUSED(_height);
    return false;
}

b8 fude_textscan_poll_page(fude_textscan_result* _out) {
    RDE_UNUSED(_out);
    return false;
}

#endif
