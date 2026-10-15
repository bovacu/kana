// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TRANSLATE
#define FUDE_TRANSLATE

#include "rde.h"

// ===========================================================================
// Translate: the platform's side of translation, by Google ML Kit's on-device translator. It translates the
// language taught into the reader's language (Text from a photo, the page's Translate, the word card), and the
// reader's language into the language taught (the Vocabulary's Into Japanese: translator.h).
//
// Where it sits: the study layer's services (study/services), used by the scan, translator, pagetext and wordcard
// screens and widgets.
// Its main types and their lifecycle: none to create or free. translate.c holds what every platform shares (the
// reader's language, the Settings switch, the demo, the box of answers waiting for their askers, the badge's
// textures, loaded once and kept for the run). The platform files do the rest: translate_ios.m on an iOS device,
// translate_android.c on Android (Java's com.rde.fude.FudeTranslate), and translate.c's own "not here" stand-ins
// elsewhere (the desktop, the iOS Simulator).
// Threads: call everything from the app's main thread. It is asynchronous: ask, then take the answer once a frame.
// On iOS, ML Kit answers on the main queue, where the app's frame runs, so its answers simply wait in a queue there.
// On Android, FudeTranslate gets ML Kit's answers on Android's own UI thread and keeps them behind a lock, from
// where the polls take them through JNI.
// Files: it reads the badge's PNGs (FUDE_TRANSLATE_BADGE_COLOR, _WHITE). ML Kit keeps the language models where it
// likes.
// Design: the models are not in the app. ML Kit downloads each language's (about 30 MB) the first time a pair needs
// it; from then on it works offline, and the text never leaves the device. A pair is two language codes, the
// language taught (lang.h: fude_lang_code, "ja") and the reader's (fude_translate_target: "en", "es", "pt", "fr"),
// either way round. ML Kit describes its own quality as "for casual and simple translations", and between two
// languages that are not English it goes through English. Google's terms for showing its translations (ML Kit's
// translation terms point to Cloud Translation's attribution rules) are met three ways: the button that asks for one
// reads "Translate with Google"; the "powered by Google Translate" badge sits by the results
// (FUDE_TRANSLATE_BADGE_*); and Google's disclaimer is in the app (Settings › Licences › ML Kit, written by
// tools/mlkit/setup.py). Like everything ML Kit, it follows the Settings switch (mlkit.h): when it is off, it is
// never called. A debug build can pretend to translate (fude_translate_demo), to look at the screens on the desktop.
// ===========================================================================

#define FUDE_TRANSLATE_TEXT 768u   // bytes of a translation, at most

typedef enum {
    FUDE_TRANSLATE_UNAVAILABLE = 0,   // not on this platform, not a pair it knows, or ML Kit switched off
    FUDE_TRANSLATE_MISSING,           // the models are not on the device yet
    FUDE_TRANSLATE_DOWNLOADING,
    FUDE_TRANSLATE_READY,
    FUDE_TRANSLATE_FAILED             // the download failed (fude_translate_prepare tries again)
} FUDE_TRANSLATE_STATE_;

// Google's badge, to go by the translations: PNGs at 3x, drawn FUDE_TRANSLATE_BADGE_W x _H. The colour one goes on
// light backgrounds, the white one on dark ones.
#define FUDE_TRANSLATE_BADGE_COLOR "assets/translate/powered-by-google.png"
#define FUDE_TRANSLATE_BADGE_WHITE "assets/translate/powered-by-google-white.png"
#define FUDE_TRANSLATE_BADGE_W     176.0f
#define FUDE_TRANSLATE_BADGE_H     16.0f

// Draws Google's badge with its left edge at _left, centred on _y (the app's screen space), the white one when
// _dark says the background is dark. The badge is loaded the first time; nothing is drawn when its file is missing.
void                  fude_translate_draw_badge(f32 _left, f32 _y, b8 _dark);
// Returns the badge itself (for a kit image), loaded the first time, or NULL when its file is missing. The texture
// is this module's and stays valid for the whole run.
rde_texture*          fude_translate_badge(b8 _dark);

// Returns true when this platform can translate at all (ML Kit's translator is in the build), or a debug build is
// pretending (fude_translate_demo).
b8                    fude_translate_available(void);
// Returns the reader's language: the code of the app's own language ("en", "es", "pt", "fr", "ja", "zh", "ko"), or
// English when the app is in the language it teaches. The string is a constant.
const c8*             fude_translate_target(void);
// Returns the state of the models for translating _from into _to. UNAVAILABLE when either code is NULL or ML Kit is
// switched off; always READY while a debug build pretends.
FUDE_TRANSLATE_STATE_ fude_translate_state(const c8* _from, const c8* _to);
// Gets _from into _to ready, downloading whichever models are missing in the background; fude_translate_state
// follows the download. Does nothing when the switch is off.
void                  fude_translate_prepare(const c8* _from, const c8* _to);
// Starts translating _text (UTF-8) from _from into _to. Returns a ticket for its answer, or 0 when it cannot now
// (the pair is not READY, or an argument is NULL).
u32                   fude_translate_text(const c8* _text, const c8* _from, const c8* _to);
// Returns true, once, when _ticket's answer is in: the translation goes into _out (UTF-8), empty when it failed.
// Every answer that has come in is kept for its own asker, so several askers can wait at once (a word card over
// Into Japanese); when 32 are waiting, the oldest is dropped. A _ticket of 0 never matches.
b8                    fude_translate_take(u32 _ticket, c8* _out, usize _size);

// Makes it pretend, on any platform: READY at once, and every text "translated" into a stand-in (and now and then
// into nothing, as a line that could not be translated). For the desktop's looks; a release build ignores it.
void                  fude_translate_demo(b8 _on);

// The platform's side, for translate.c, which adds the Settings switch and the demo around it: translate_ios.m on
// iOS, translate_android.c on Android, and translate.c's own "not here" stand-ins elsewhere. They mean the same as
// the calls above. fude_translate_platform_poll hands over the next answer in, whoever asked for it: its ticket
// into *_ticket, its text into _out; false when none is waiting.
b8                    fude_translate_platform_available(void);
FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to);
void                  fude_translate_platform_prepare(const c8* _from, const c8* _to);
u32                   fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to);
b8                    fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size);

#endif
