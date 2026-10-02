#ifndef FUDE_TRANSLATE
#define FUDE_TRANSLATE

#include "rde.h"

// ===========================================================================
// Translation — the platform's side: Japanese text into the reader's language
// (Text from a photo, the page's Translate), and the reader's language into
// Japanese (the Vocabulary's Into Japanese: translator.h), by Google ML Kit's
// on-device translator. Its models are not in the app: ML Kit downloads each
// language's (about 30 MB) the first time a pair needs it; from then on it works
// offline, and the text never leaves the device. A pair is two language codes:
// "ja" and the reader's ("en" "es" "pt" "fr"), either way round.
//
// Google's terms for showing its translations (ML Kit's translation terms point
// to Cloud Translation's attribution rules): the button that asks for one reads
// "Translate with Google"; the "powered by Google Translate" badge sits by the
// results (FUDE_TRANSLATE_BADGE_*); and Google's disclaimer is in the app
// (Settings › Licences › ML Kit, written by tools/mlkit/setup.py). ML Kit's own
// word for its quality: "for casual and simple translations" — and between two
// languages that are not English it goes through English.
//
// iOS: src/translate_ios.m. Android: to come (ML Kit's translator is the same
// there). Elsewhere: not available — but a debug build can pretend
// (fude_translate_demo), to look at the screens on the desktop.
//
// Like everything ML Kit, it follows the Settings switch (mlkit.h): off, it is
// never called. Asynchronous: ask, then take its answer once a frame.
// ===========================================================================

#define FUDE_TRANSLATE_TEXT 768u   // bytes of a translation, at most

typedef enum {
    FUDE_TRANSLATE_UNAVAILABLE = 0,   // not on this platform, or ML Kit switched off
    FUDE_TRANSLATE_MISSING,           // the models are not on the device yet
    FUDE_TRANSLATE_DOWNLOADING,
    FUDE_TRANSLATE_READY,
    FUDE_TRANSLATE_FAILED             // the download failed (fude_translate_prepare tries again)
} FUDE_TRANSLATE_STATE_;

// Google's badge, for by the translations: PNGs at 3x, drawn
// FUDE_TRANSLATE_BADGE_W x _H. The colour one on light backgrounds, the white
// one on dark ones.
#define FUDE_TRANSLATE_BADGE_COLOR "assets/translate/powered-by-google.png"
#define FUDE_TRANSLATE_BADGE_WHITE "assets/translate/powered-by-google-white.png"
#define FUDE_TRANSLATE_BADGE_W     176.0f
#define FUDE_TRANSLATE_BADGE_H     16.0f

// Google's badge drawn: its left edge at _left, centred on _y (Kana's screen
// space), the white one on a _dark background. Loaded the first time.
void                  fude_translate_draw_badge(f32 _left, f32 _y, b8 _dark);
// The badge itself (for a kit image), loaded the first time; NULL when missing.
rde_texture*          fude_translate_badge(b8 _dark);

// Can this platform translate at all (ML Kit's translator is in it)?
b8                    fude_translate_available(void);
// The reader's language: the app's ("en", "es", "pt", "fr") — English when the
// app is in Japanese.
const c8*             fude_translate_target(void);
// _from into _to: the state of its models.
FUDE_TRANSLATE_STATE_ fude_translate_state(const c8* _from, const c8* _to);
// Gets _from into _to ready: downloads what is missing (asynchronous;
// fude_translate_state follows it).
void                  fude_translate_prepare(const c8* _from, const c8* _to);
// Starts translating _text (UTF-8) from _from into _to: a ticket for its
// answer, 0 when it cannot now (not READY).
u32                   fude_translate_text(const c8* _text, const c8* _from, const c8* _to);
// True once _ticket's answer is in (once): the translation into _out (UTF-8),
// empty when it failed. Answers to other tickets wait for their askers.
b8                    fude_translate_take(u32 _ticket, c8* _out, usize _size);

// Debug builds only (a release ignores it): pretend, on any platform — READY
// at once, every text "translated" to a stand-in. For the desktop's looks.
void                  fude_translate_demo(b8 _on);

// The platform's side, for translate.c (which adds the Settings switch and the
// demo around it): src/translate_ios.m on iOS, translate.c's own "not here"
// elsewhere. The same meanings as the calls above.
b8                    fude_translate_platform_available(void);
FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to);
void                  fude_translate_platform_prepare(const c8* _from, const c8* _to);
u32                   fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to);
b8                    fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size);

#endif
