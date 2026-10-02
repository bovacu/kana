#include "study/services/translate.h"
#include "drawing/base/text.h"
#include "study/services/mlkit.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See translate.h. What every platform shares: the language translations go
// into, the Settings switch, and the demo; the platform does the rest.
// ===========================================================================

#if defined(RDE_DEBUG)
// The demo: the texts asked for, answered one a poll.
#define FUDE_TRANSLATE_DEMO_QUEUE 256u
RDE_INTERNAL b8  fude_translate_demo_on = false;
RDE_INTERNAL u32 fude_translate_demo_next = 1u;
RDE_INTERNAL u32 fude_translate_demo_head = 0u;
RDE_INTERNAL u32 fude_translate_demo_count = 0u;
RDE_INTERNAL struct { u32 ticket; c8 text[256]; } fude_translate_demo_queue[FUDE_TRANSLATE_DEMO_QUEUE];
#endif

void fude_translate_demo(b8 _on) {
#if defined(RDE_DEBUG)
    fude_translate_demo_on = _on;
#else
    RDE_UNUSED(_on);
#endif
}

RDE_INTERNAL b8 fude_translate_demoing(void) {
#if defined(RDE_DEBUG)
    return fude_translate_demo_on;
#else
    return false;
#endif
}

// The two badges (translate.h), loaded once.
RDE_INTERNAL rde_texture* fude_translate_badges[2];

void fude_translate_draw_badge(f32 _left, f32 _y, b8 _dark) {
    const u32 _b = _dark ? 1u : 0u;
    if(fude_translate_badges[_b] == NULL) {
        fude_translate_badges[_b] = rde_texture_load(_dark ? FUDE_TRANSLATE_BADGE_WHITE : FUDE_TRANSLATE_BADGE_COLOR, NULL);
    }
    if(fude_translate_badges[_b] == NULL) {
        return;
    }
    const rde_vec_2UI _px = rde_texture_get_size(fude_translate_badges[_b]);
    rde_rendering_2d_draw_texture_2(fude_translate_badges[_b], (rde_vec_3F){ _left + FUDE_TRANSLATE_BADGE_W * 0.5f, _y, 0.0f },
                                    (rde_vec_2F){ FUDE_TRANSLATE_BADGE_W / (f32)_px.x, FUDE_TRANSLATE_BADGE_H / (f32)_px.y }, 0.0f,
                                    (rde_color){ 255, 255, 255, 255 });
}

b8 fude_translate_available(void) {
    return fude_translate_demoing() || fude_translate_platform_available();
}

const c8* fude_translate_target(void) {
    switch(fude_text_language()) {
        case RDE_LANGUAGE_ES_ES: return "es";
        case RDE_LANGUAGE_PT_BR: return "pt";
        case RDE_LANGUAGE_FR_FR: return "fr";
        default:                 return "en";   // English, and Japanese's readers
    }
}

FUDE_TRANSLATE_STATE_ fude_translate_state(const c8* _target) {
    if(fude_translate_demoing()) {
        return FUDE_TRANSLATE_READY;
    }
    if(!fude_mlkit_enabled() || _target == NULL) {
        return FUDE_TRANSLATE_UNAVAILABLE;
    }
    return fude_translate_platform_state(_target);
}

void fude_translate_prepare(const c8* _target) {
    if(!fude_translate_demoing() && fude_mlkit_enabled() && _target != NULL) {
        fude_translate_platform_prepare(_target);
    }
}

u32 fude_translate_text(const c8* _text, const c8* _target) {
    if(_text == NULL || _target == NULL) {
        return 0u;
    }
#if defined(RDE_DEBUG)
    if(fude_translate_demo_on) {
        if(fude_translate_demo_count == FUDE_TRANSLATE_DEMO_QUEUE) {
            return 0u;
        }
        const u32 _at = (fude_translate_demo_head + fude_translate_demo_count++) % FUDE_TRANSLATE_DEMO_QUEUE;
        fude_translate_demo_queue[_at].ticket = fude_translate_demo_next++;
        snprintf(fude_translate_demo_queue[_at].text, sizeof(fude_translate_demo_queue[_at].text), "%s", _text);
        return fude_translate_demo_queue[_at].ticket;
    }
#endif
    if(fude_translate_state(_target) != FUDE_TRANSLATE_READY) {
        return 0u;
    }
    return fude_translate_platform_text(_text, _target);
}

b8 fude_translate_poll(u32* _ticket, c8* _out, usize _size) {
#if defined(RDE_DEBUG)
    if(fude_translate_demo_on) {
        if(fude_translate_demo_count == 0u) {
            return false;
        }
        const u32 _at = fude_translate_demo_head;
        fude_translate_demo_head = (fude_translate_demo_head + 1u) % FUDE_TRANSLATE_DEMO_QUEUE;
        fude_translate_demo_count--;
        *_ticket = fude_translate_demo_queue[_at].ticket;
        // A stand-in long enough to wrap, and an empty answer now and then (a
        // line that could not be translated).
        if(*_ticket % 5u == 0u) {
            _out[0] = 0;
        } else {
            snprintf(_out, _size, "A stand-in translation of %s (%zu bytes of Japanese), long enough to need a second line.",
                     fude_translate_demo_queue[_at].text, strlen(fude_translate_demo_queue[_at].text));
        }
        return true;
    }
#endif
    return fude_translate_platform_poll(_ticket, _out, _size);
}

// --- not here ---------------------------------------------------------------------------
// Every platform but iOS (src/translate_ios.m): no translator yet.

#if !defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)   // ML Kit: a device's, not the Simulator's

b8 fude_translate_platform_available(void) {
    return false;
}

FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _target) {
    RDE_UNUSED(_target);
    return FUDE_TRANSLATE_UNAVAILABLE;
}

void fude_translate_platform_prepare(const c8* _target) {
    RDE_UNUSED(_target);
}

u32 fude_translate_platform_text(const c8* _text, const c8* _target) {
    RDE_UNUSED(_text);
    RDE_UNUSED(_target);
    return 0u;
}

b8 fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    RDE_UNUSED(_ticket);
    RDE_UNUSED(_out);
    RDE_UNUSED(_size);
    return false;
}

#endif
