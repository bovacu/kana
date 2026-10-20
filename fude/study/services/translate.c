// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/translate.h"
#include "drawing/base/redraw.h"
#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/ink/mlkit.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See translate.h. What every platform shares: the language translations go into, the Settings switch, the demo,
// the badge, and the box where answers wait for their askers. The platform files do the rest, through the
// fude_translate_platform_ calls; the stand-ins for platforms with no translator are at the end of this file.
// ===========================================================================

#if defined(RDE_DEBUG)
// The demo (debug builds only): the texts asked for wait in an rde_ring_buffer of FUDE_TRANSLATE_DEMO_QUEUE (made on
// first use, on the std allocator, kept for the run; a text asked for while it is full gets no ticket), and one is
// answered each poll. Tickets count up from 1, since 0 means no ticket.
#define FUDE_TRANSLATE_DEMO_QUEUE 256u
typedef struct { u32 ticket; b8 into_studied; c8 text[256]; } fude_translate_demo_asked;
RDE_INTERNAL b8  fude_translate_demo_on = false;
RDE_INTERNAL u32 fude_translate_demo_next = 1u;
RDE_INTERNAL rde_ring_buffer fude_translate_demo_queue;   // fude_translate_demo_asked, oldest first
#endif

// See the header. Does nothing in a release build.
void fude_translate_demo(b8 _on) {
#if defined(RDE_DEBUG)
    fude_translate_demo_on = _on;
#else
    RDE_UNUSED(_on);
#endif
}

// Whether the demo is on; never in a release build.
RDE_INTERNAL b8 fude_translate_demoing(void) {
#if defined(RDE_DEBUG)
    return fude_translate_demo_on;
#else
    return false;
#endif
}

// The two badges (translate.h), the colour one first and the white one for a dark theme, each loaded the first time
// it is needed and kept for the run.
RDE_INTERNAL rde_texture* fude_translate_badges[2];

// See the header.
rde_texture* fude_translate_badge(b8 _dark) {
    const u32 _b = _dark ? 1u : 0u;
    if(fude_translate_badges[_b] == NULL) {
        fude_translate_badges[_b] = rde_texture_load(_dark ? FUDE_TRANSLATE_BADGE_WHITE : FUDE_TRANSLATE_BADGE_COLOR, NULL);
    }
    return fude_translate_badges[_b];
}

// See the header. Draws nothing when the badge's PNG could not be loaded.
void fude_translate_draw_badge(f32 _left, f32 _y, b8 _dark) {
    const u32 _b = _dark ? 1u : 0u;
    if(fude_translate_badge(_dark) == NULL) {
        return;
    }
    const rde_vec_2UI _px = rde_texture_get_size(fude_translate_badges[_b]);
    rde_rendering_2d_draw_texture_2(fude_translate_badges[_b], (rde_vec_3F){ fude_draw_x(_left + FUDE_TRANSLATE_BADGE_W * 0.5f), _y, 0.0f },
                                    (rde_vec_2F){ FUDE_TRANSLATE_BADGE_W / (f32)_px.x, FUDE_TRANSLATE_BADGE_H / (f32)_px.y }, 0.0f,
                                    (rde_color){ 255, 255, 255, 255 });
}

// See the header. The demo counts as a translator.
b8 fude_translate_available(void) {
    return fude_translate_demoing() || fude_translate_platform_available();
}

// See the header. The app's own language when ML Kit knows it, else English.
const c8* fude_translate_target(void) {
    const c8* _ui;
    switch(fude_text_language()) {
        case RDE_LANGUAGE_ES_ES: _ui = "es"; break;
        case RDE_LANGUAGE_PT_BR: _ui = "pt"; break;
        case RDE_LANGUAGE_FR_FR: _ui = "fr"; break;
        case RDE_LANGUAGE_JA_JP: _ui = "ja"; break;
        case RDE_LANGUAGE_ZH_CN: _ui = "zh"; break;
        case RDE_LANGUAGE_KO_KR: _ui = "ko"; break;
        default:                 _ui = "en"; break;
    }
    return rde_strcmp(_ui, fude_lang_code()) == 0 ? "en" : _ui;   // App shown in the language it teaches: English.
}

// See the header. Unavailable while ML Kit is switched off; always ready in the demo.
FUDE_TRANSLATE_STATE_ fude_translate_state(const c8* _from, const c8* _to) {
    if(fude_translate_demoing()) {
        return FUDE_TRANSLATE_READY;
    }
    if(!fude_mlkit_enabled() || _from == NULL || _to == NULL) {
        return FUDE_TRANSLATE_UNAVAILABLE;
    }
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_platform_state(_from, _to);
    if(_state == FUDE_TRANSLATE_DOWNLOADING) {
        fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: the model's arrival is polled, with no event)
    }
    return _state;
}

// See the header. Nothing happens in the demo or while ML Kit is switched off.
void fude_translate_prepare(const c8* _from, const c8* _to) {
    if(!fude_translate_demoing() && fude_mlkit_enabled() && _from != NULL && _to != NULL) {
        fude_translate_platform_prepare(_from, _to);
    }
}

// See the header. In the demo the text is queued instead (0 when the queue is full).
u32 fude_translate_text(const c8* _text, const c8* _from, const c8* _to) {
    if(_text == NULL || _from == NULL || _to == NULL) {
        return 0u;
    }
#if defined(RDE_DEBUG)
    if(fude_translate_demo_on) {
        if(!rde_ring_buffer_is_inited(&fude_translate_demo_queue)) {
            fude_translate_demo_queue = rde_ring_buffer_new(sizeof(fude_translate_demo_asked), FUDE_TRANSLATE_DEMO_QUEUE, rde_memory_allocator_get_default_std());
        }
        if(rde_ring_buffer_is_full(&fude_translate_demo_queue)) {
            return 0u;
        }
        fude_translate_demo_asked _asked;
        rde_memset(&_asked, 0, sizeof(_asked));
        _asked.ticket       = fude_translate_demo_next++;
        _asked.into_studied = rde_strcmp(_to, fude_lang_code()) == 0;
        rde_snprintf(_asked.text, sizeof(_asked.text), "%s", _text);
        rde_ring_buffer_try_push(&fude_translate_demo_queue, (const any)&_asked);
        return _asked.ticket;
    }
#endif
    if(fude_translate_state(_from, _to) != FUDE_TRANSLATE_READY) {
        return 0u;
    }
    return fude_translate_platform_text(_text, _from, _to);
}

// Takes the next answer that has come in, from the demo or the platform: its ticket into *_ticket and its text into
// _out. Returns false when none has come.
RDE_INTERNAL b8 fude_translate_poll(u32* _ticket, c8* _out, usize _size) {
#if defined(RDE_DEBUG)
    if(fude_translate_demo_on) {
        fude_translate_demo_asked _asked;
        if(!rde_ring_buffer_is_inited(&fude_translate_demo_queue) || !rde_ring_buffer_pop_into(&fude_translate_demo_queue, (any)&_asked)) {
            return false;
        }
        *_ticket = _asked.ticket;
        // A stand-in long enough to wrap onto a second line, and an empty answer every fifth ticket, as for a line
        // that could not be translated. It names no language of its own, so it serves every app: into the language
        // taught, the text itself with that language's code after it ("本 → ja").
        if(*_ticket % 5u == 0u) {
            _out[0] = 0;
        } else if(_asked.into_studied) {
            rde_snprintf(_out, _size, "%s \xE2\x86\x92 %s", _asked.text, fude_lang_code());   // →
        } else {
            rde_snprintf(_out, _size, "A stand-in translation of %s (%zu bytes of %s), long enough to need a second line.",
                         _asked.text, rde_strlen(_asked.text), fude_lang_code());
        }
        return true;
    }
#endif
    return fude_translate_platform_poll(_ticket, _out, _size);
}

// --- the answers, each to its asker ---------------------------------------------------
// Answers are taken from the platform as they come and kept in a box until their askers take them. Each asker takes
// only its own, so several can wait at once (a word card over the Translator, say). When the box is full, the oldest
// answer is dropped, since nobody took it (its asker has gone).
//
// Why a fixed box is safe: the bound is what throws away the answers nobody will take (an asker that went before its
// answer came: Text from a photo closed while translating), which would otherwise pile up for the whole run. The
// askers keep few tickets on their way: Text from a photo FUDE_SCAN_TRANSLATE_AT_ONCE (4), the translator, the page's
// card and the word card one each, the developer's look harness 4: 11 at most. Each asker takes its answer the frame
// after it comes, while it is up, so an answer that is waited for is always among the newest few, and the oldest one
// dropped from a box of 32 is one that nobody is waiting for.

#define FUDE_TRANSLATE_BOX 32u   // Answers kept at most, waiting for their askers (see above).
RDE_INTERNAL struct { u32 ticket; c8 text[FUDE_TRANSLATE_TEXT]; } fude_translate_box[FUDE_TRANSLATE_BOX];
RDE_INTERNAL u32 fude_translate_box_count = 0u;

// See the header. First moves every answer that has come into the box, then looks there for _ticket's.
b8 fude_translate_take(u32 _ticket, c8* _out, usize _size) {
    u32 _in;
    c8  _answer[FUDE_TRANSLATE_TEXT];
    while(fude_translate_poll(&_in, _answer, sizeof(_answer))) {
        if(fude_translate_box_count == FUDE_TRANSLATE_BOX) {
            rde_memmove(&fude_translate_box[0], &fude_translate_box[1], sizeof(fude_translate_box[0]) * (FUDE_TRANSLATE_BOX - 1u));
            fude_translate_box_count--;
        }
        fude_translate_box[fude_translate_box_count].ticket = _in;
        rde_memcpy(fude_translate_box[fude_translate_box_count].text, _answer, sizeof(_answer));
        fude_translate_box_count++;
    }
    for(u32 _i = 0; _i < fude_translate_box_count && _ticket != 0u; _i++) {
        if(fude_translate_box[_i].ticket == _ticket) {
            rde_snprintf(_out, _size, "%s", fude_translate_box[_i].text);
            rde_memmove(&fude_translate_box[_i], &fude_translate_box[_i + 1u], sizeof(fude_translate_box[0]) * (fude_translate_box_count - _i - 1u));
            fude_translate_box_count--;
            fude_redraw_changed();   // (render on demand: its asker shows it)
            return true;
        }
    }
    if(_ticket != 0u) {
        fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: an answer waited for is polled, with no event)
    }
    return false;
}

// --- not here ---------------------------------------------------------------------------
// The stand-ins for every platform but an iOS device (translate_ios.m) and Android (translate_android.c), where there
// is no translator yet. ML Kit runs on a device only, not in the iOS Simulator.

#if (!defined(RDE_PLATFORM_IOS) || defined(RDE_PLATFORM_IOS_SIMULATOR)) && !defined(RDE_PLATFORM_ANDROID)   // Stand-ins

// No translator here.
b8 fude_translate_platform_available(void) {
    return false;
}

// Every pair is unavailable here.
FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to) {
    RDE_UNUSED(_from);
    RDE_UNUSED(_to);
    return FUDE_TRANSLATE_UNAVAILABLE;
}

// Nothing to download here.
void fude_translate_platform_prepare(const c8* _from, const c8* _to) {
    RDE_UNUSED(_from);
    RDE_UNUSED(_to);
}

// Nothing can be translated here: no ticket.
u32 fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to) {
    RDE_UNUSED(_text);
    RDE_UNUSED(_from);
    RDE_UNUSED(_to);
    return 0u;
}

// No answer ever comes here.
b8 fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    RDE_UNUSED(_ticket);
    RDE_UNUSED(_out);
    RDE_UNUSED(_size);
    return false;
}

#endif
