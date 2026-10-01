#ifndef KANA_SPEECH
#define KANA_SPEECH

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
b8   kana_speech_available(void);
// Reads _text (UTF-8) aloud, after stopping whatever was being read.
void kana_speak(const c8* _text);
void kana_speech_stop(void);
// True once a run, after the device's basic voice spoke (no better Japanese one
// downloaded): time to say where the better ones are.
b8   kana_speech_take_hint(void);

#endif
