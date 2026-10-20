// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SPEECH
#define FUDE_SPEECH

#include "rde.h"

// ===========================================================================
// Speech: the language taught, read aloud by the platform's own voice, on the device and with no network.
//
// Where it sits: the study layer's services (study/services). The screens and widgets that read things aloud use it
// (the viewer, the Vocabulary, its word exam, Into Japanese, Text from a photo, the page's Translate card), and the
// welcome's voice card (welcome.h) and the side panel's Set up the voice button (study.h) follow its state. What is
// read aloud: a word's reading (its kana: the pronunciation, with nothing left for the voice to guess), a
// character's readings, and the lines of Text from a photo and of Translate with Google (as written: the voice
// reads the kanji as it would).
// Its main types and their lifecycle: none to create or free. The platform file holds the state: speech_ios.m on
// iOS (AVSpeechSynthesizer), speech_android.c on Android (the device's TextToSpeech, through Java's
// com.rde.fude.FudeSpeech, started the first time a voice is asked about), and stand-ins in speech.c elsewhere,
// where there is no voice.
// Threads: call everything from the app's main thread. On Android, the TextToSpeech engine starts and answers on
// Android's own thread and keeps its state in a volatile field, which fude_speech_state reads through JNI.
// Files: none.
// Design: the voice is the language's (lang.h: fude_lang_voice, "ja-JP"). On iOS each reading picks the best voice
// present, so one downloaded in Settings while the app is open counts at once; it reads a little slower than the
// default, because a learner listens for each sound.
// ===========================================================================

// Returns true when the device has a voice for the language taught.
b8   fude_speech_available(void);

// Where the voice is. CHECKING: Android's engine answers a moment after it is first asked. MISSING: the device
// could have a voice but has none installed (the voice card in welcome.h says how to add one). NONE: there is no
// voice on this platform (a computer).
typedef enum { FUDE_SPEECH_CHECKING = 0, FUDE_SPEECH_READY, FUDE_SPEECH_MISSING, FUDE_SPEECH_NONE } FUDE_SPEECH_;
// Returns where the voice is now. Ask it once a frame: on Android, coming back to the app after a while (from
// Settings, where a voice may just have been installed) makes it ask the engine again.
FUDE_SPEECH_ fude_speech_state(void);
// On Android, opens the device's text-to-speech settings, where a voice is installed. Returns false elsewhere, or
// when the device has no such screen. Back from them, the voice is asked about again, so one installed there shows
// by itself.
b8   fude_speech_open_settings(void);
// For a developer's look on a computer, which has no voice: makes the voice show as missing, so that its card and
// its button can be seen. It has no effect on iOS or Android.
void fude_speech_look_missing(void);
// The app is back on screen (the shell calls it on the foreground event): on Android, the next fude_speech_state
// asks the engine again when there was no voice. fude_speech_take_came_back returns whether it was called since it
// last returned true (the platform's fude_speech_state takes it). Main thread.
void fude_speech_came_back(void);
b8   fude_speech_take_came_back(void);
// Reads _text (UTF-8) aloud, first stopping whatever was being read. Does nothing for NULL, or where there is no
// voice.
void fude_speak(const c8* _text);
// Stops reading aloud at once.
void fude_speech_stop(void);
// Returns true once a run, after the device's basic voice has spoken because no better one was downloaded: the
// time to tell the learner where the better ones are. Only iOS has better voices to offer; elsewhere it is false.
b8   fude_speech_take_hint(void);

#endif
