#include "translate.h"
#include "text.h"
#include "mlkit.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See translate.h. What every platform shares: the language translations go
// into, the Settings switch, and the demo; the platform does the rest.
// ===========================================================================

#if defined(RDE_DEBUG)
// The demo: the texts asked for, answered one a poll.
#define KANA_TRANSLATE_DEMO_QUEUE 256u
RDE_INTERNAL b8  kana_translate_demo_on = false;
RDE_INTERNAL u32 kana_translate_demo_next = 1u;
RDE_INTERNAL u32 kana_translate_demo_head = 0u;
RDE_INTERNAL u32 kana_translate_demo_count = 0u;
RDE_INTERNAL struct { u32 ticket; c8 text[256]; } kana_translate_demo_queue[KANA_TRANSLATE_DEMO_QUEUE];
#endif

void kana_translate_demo(b8 _on) {
#if defined(RDE_DEBUG)
    kana_translate_demo_on = _on;
#else
    RDE_UNUSED(_on);
#endif
}

RDE_INTERNAL b8 kana_translate_demoing(void) {
#if defined(RDE_DEBUG)
    return kana_translate_demo_on;
#else
    return false;
#endif
}

// The two badges (translate.h), loaded once.
RDE_INTERNAL rde_texture* kana_translate_badges[2];

void kana_translate_draw_badge(f32 _left, f32 _y, b8 _dark) {
    const u32 _b = _dark ? 1u : 0u;
    if(kana_translate_badges[_b] == NULL) {
        kana_translate_badges[_b] = rde_texture_load(_dark ? KANA_TRANSLATE_BADGE_WHITE : KANA_TRANSLATE_BADGE_COLOR, NULL);
    }
    if(kana_translate_badges[_b] == NULL) {
        return;
    }
    const rde_vec_2UI _px = rde_texture_get_size(kana_translate_badges[_b]);
    rde_rendering_2d_draw_texture_2(kana_translate_badges[_b], (rde_vec_3F){ _left + KANA_TRANSLATE_BADGE_W * 0.5f, _y, 0.0f },
                                    (rde_vec_2F){ KANA_TRANSLATE_BADGE_W / (f32)_px.x, KANA_TRANSLATE_BADGE_H / (f32)_px.y }, 0.0f,
                                    (rde_color){ 255, 255, 255, 255 });
}

b8 kana_translate_available(void) {
    return kana_translate_demoing() || kana_translate_platform_available();
}

const c8* kana_translate_target(void) {
    switch(kana_text_language()) {
        case RDE_LANGUAGE_ES_ES: return "es";
        case RDE_LANGUAGE_PT_BR: return "pt";
        case RDE_LANGUAGE_FR_FR: return "fr";
        default:                 return "en";   // English, and Japanese's readers
    }
}

KANA_TRANSLATE_STATE_ kana_translate_state(const c8* _target) {
    if(kana_translate_demoing()) {
        return KANA_TRANSLATE_READY;
    }
    if(!kana_mlkit_enabled() || _target == NULL) {
        return KANA_TRANSLATE_UNAVAILABLE;
    }
    return kana_translate_platform_state(_target);
}

void kana_translate_prepare(const c8* _target) {
    if(!kana_translate_demoing() && kana_mlkit_enabled() && _target != NULL) {
        kana_translate_platform_prepare(_target);
    }
}

u32 kana_translate_text(const c8* _text, const c8* _target) {
    if(_text == NULL || _target == NULL) {
        return 0u;
    }
#if defined(RDE_DEBUG)
    if(kana_translate_demo_on) {
        if(kana_translate_demo_count == KANA_TRANSLATE_DEMO_QUEUE) {
            return 0u;
        }
        const u32 _at = (kana_translate_demo_head + kana_translate_demo_count++) % KANA_TRANSLATE_DEMO_QUEUE;
        kana_translate_demo_queue[_at].ticket = kana_translate_demo_next++;
        snprintf(kana_translate_demo_queue[_at].text, sizeof(kana_translate_demo_queue[_at].text), "%s", _text);
        return kana_translate_demo_queue[_at].ticket;
    }
#endif
    if(kana_translate_state(_target) != KANA_TRANSLATE_READY) {
        return 0u;
    }
    return kana_translate_platform_text(_text, _target);
}

b8 kana_translate_poll(u32* _ticket, c8* _out, usize _size) {
#if defined(RDE_DEBUG)
    if(kana_translate_demo_on) {
        if(kana_translate_demo_count == 0u) {
            return false;
        }
        const u32 _at = kana_translate_demo_head;
        kana_translate_demo_head = (kana_translate_demo_head + 1u) % KANA_TRANSLATE_DEMO_QUEUE;
        kana_translate_demo_count--;
        *_ticket = kana_translate_demo_queue[_at].ticket;
        // A stand-in long enough to wrap, and an empty answer now and then (a
        // line that could not be translated).
        if(*_ticket % 5u == 0u) {
            _out[0] = 0;
        } else {
            snprintf(_out, _size, "A stand-in translation of %s (%zu bytes of Japanese), long enough to need a second line.",
                     kana_translate_demo_queue[_at].text, strlen(kana_translate_demo_queue[_at].text));
        }
        return true;
    }
#endif
    return kana_translate_platform_poll(_ticket, _out, _size);
}

// --- not here ---------------------------------------------------------------------------
// Every platform but iOS (src/translate_ios.m): no translator yet.

#if !defined(RDE_PLATFORM_IOS)

b8 kana_translate_platform_available(void) {
    return false;
}

KANA_TRANSLATE_STATE_ kana_translate_platform_state(const c8* _target) {
    RDE_UNUSED(_target);
    return KANA_TRANSLATE_UNAVAILABLE;
}

void kana_translate_platform_prepare(const c8* _target) {
    RDE_UNUSED(_target);
}

u32 kana_translate_platform_text(const c8* _text, const c8* _target) {
    RDE_UNUSED(_text);
    RDE_UNUSED(_target);
    return 0u;
}

b8 kana_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    RDE_UNUSED(_ticket);
    RDE_UNUSED(_out);
    RDE_UNUSED(_size);
    return false;
}

#endif
