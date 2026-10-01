#include "textscan.h"

// ===========================================================================
// See textscan.h. Every platform but iOS (src/textscan_ios.m, in the iOS build
// only): no text in photos yet.
// ===========================================================================

#if !defined(RDE_PLATFORM_IOS)

b8 kana_textscan_available(void) {
    return false;
}

b8 kana_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    return false;
}

KANA_TEXTSCAN_STATE_ kana_textscan_poll(kana_textscan_result* _out) {
    RDE_UNUSED(_out);
    return KANA_TEXTSCAN_IDLE;
}

b8 kana_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    RDE_UNUSED(_rgba);
    RDE_UNUSED(_width);
    RDE_UNUSED(_height);
    RDE_UNUSED(_rotation);
    return false;
}

b8 kana_textscan_poll_frame(kana_textscan_result* _out) {
    RDE_UNUSED(_out);
    return false;
}

#endif
