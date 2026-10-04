// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SPEECH
#define FUDE_SPEECH

#include "rde.h"

// ===========================================================================
// Japanese read aloud, by the platform's own voice, on the device (no network):
// iOS's Japanese voice through AVSpeechSynthesizer (src/speech_ios.m).
// Android: to come (its TextToSpeech has Japanese too). Elsewhere: no voice.
//
// What Kana reads aloud: a word's reading (its kana — the pronunciation, with
// nothing for the voice to guess), a character's readings, and the lines of
// Text from a photo and of Translate with Google (as written: the voice reads
// the kanji as it would).
// ===========================================================================

// Is there a Japanese voice here?
b8   fude_speech_available(void);

// Where the voice is. CHECKING: Android's engine answers a moment after it is
// first asked; MISSING: the device could have one but has not (the voice card
// says how: welcome.h); NONE: not on this platform (a computer).
typedef enum { FUDE_SPEECH_CHECKING = 0, FUDE_SPEECH_READY, FUDE_SPEECH_MISSING, FUDE_SPEECH_NONE } FUDE_SPEECH_;
FUDE_SPEECH_ fude_speech_state(void);
// Android: the device's text-to-speech settings, opened (where a voice is
// installed). False elsewhere, or when there is no such screen. Back from them,
// the voice is asked about again: one installed there shows by itself.
b8   fude_speech_open_settings(void);
// A look's (a computer, which has no voice): the voice as missing, to see its card and button.
void fude_speech_look_missing(void);
// Reads _text (UTF-8) aloud, after stopping whatever was being read.
void fude_speak(const c8* _text);
void fude_speech_stop(void);
// True once a run, after the device's basic voice spoke (no better Japanese one
// downloaded): time to say where the better ones are.
b8   fude_speech_take_hint(void);

#endif
