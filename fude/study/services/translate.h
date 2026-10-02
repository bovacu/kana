#ifndef FUDE_TRANSLATE
#define FUDE_TRANSLATE

#include "rde.h"

// ===========================================================================
// Translation — the platform's side: Japanese text into the reader's language,
// by Google ML Kit's on-device translator. Its models are not in the app: ML
// Kit downloads Japanese's and the target's (about 30 MB each) the first time
// they are asked for; from then on it works offline, and the text never leaves
// the device. scan.h shows what it translates (Text from a photo).
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
// never called. Asynchronous: ask, then poll once a frame.
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

// Can this platform translate at all (ML Kit's translator is in it)?
b8                    fude_translate_available(void);
// The language translations go into: the app's ("en", "es", "pt", "fr") —
// English when the app is in Japanese.
const c8*             fude_translate_target(void);
// Japanese into _target: the state of its models.
FUDE_TRANSLATE_STATE_ fude_translate_state(const c8* _target);
// Gets Japanese into _target ready: downloads what is missing (asynchronous;
// fude_translate_state follows it).
void                  fude_translate_prepare(const c8* _target);
// Starts translating _text (Japanese, UTF-8) into _target: a ticket for its
// answer, 0 when it cannot now (not READY).
u32                   fude_translate_text(const c8* _text, const c8* _target);
// True once an answer is in — one per call: its ticket, and the translation
// into _out (UTF-8), empty when it failed.
b8                    fude_translate_poll(u32* _ticket, c8* _out, usize _size);

// Debug builds only (a release ignores it): pretend, on any platform — READY
// at once, every text "translated" to a stand-in. For the desktop's looks.
void                  fude_translate_demo(b8 _on);

// The platform's side, for translate.c (which adds the Settings switch and the
// demo around it): src/translate_ios.m on iOS, translate.c's own "not here"
// elsewhere. The same meanings as the calls above.
b8                    fude_translate_platform_available(void);
FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _target);
void                  fude_translate_platform_prepare(const c8* _target);
u32                   fude_translate_platform_text(const c8* _text, const c8* _target);
b8                    fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size);

#endif
