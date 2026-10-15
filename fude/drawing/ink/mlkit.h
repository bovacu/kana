// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_MLKIT
#define FUDE_MLKIT

#include "rde.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// ML Kit handwriting: Google ML Kit Digital Ink Recognition, which reads handwriting with a model trained on many
// writers, and the Settings switch that every ML Kit service in the app obeys.
//
// Where it sits: drawing/ink, since what it reads is ink (ink.h), and every app reads handwriting with it. The study
// apps' handwriting reader (study/handwriting/recognize.h) asks it what a stroke sequence says, and their text and
// translation services (textscan.h, translate.h) and Settings' Handwriting section (study.h) read its switch. The
// canvas apps read the lasso's To text and Find's handwriting with it (zoom/page_find.c). It answers with text (its
// candidates, best first), not with which strokes make which character: the study's segment.h does that.
// Its main types and their lifecycle: none to create or free. The state lives in the platform file: mlkit_ios.m on
// an iOS device, mlkit_android.c on Android (Java's com.rde.fude.FudeInk), and stand-ins in mlkit.c everywhere else,
// including the iOS Simulator, for which ML Kit has no build. The switch itself lives in mlkit.c on every platform.
// Threads: call everything from the app's main thread. Everything is asynchronous: ask, then poll once a frame. On
// iOS, ML Kit announces downloads and delivers answers on the main queue, so the state only changes there. On
// Android, FudeInk gets ML Kit's answers on Android's own UI thread and keeps them in volatile fields, which these
// calls read through JNI.
// Files: none of its own. ML Kit keeps the downloaded model where it likes. The switch is the core's setting,
// saved in the settings file with the others (drawing/app/session.c: fude_settings.mlkit).
// Design: the model is the app's to choose (fude_mlkit_set_model): a study app's is the language it teaches
// (lang.h: fude_lang_ink_model; Japanese's is about 20 MB), a canvas app's the language it is shown in. It is told
// rather than asked so that this layer knows no language and no app. It is downloaded the first time, which needs
// the network once; after that it works offline. It was first tried on the ml-kit
// branch; tools/mlkit/setup.py shows how it is built into the app.
// ===========================================================================

typedef enum {
    FUDE_MLKIT_UNAVAILABLE = 0,     // not in this build (the desktop, the iOS Simulator), or no Java side (Android)
    FUDE_MLKIT_IDLE,                // not prepared yet
    FUDE_MLKIT_DOWNLOADING,         // the model is on its way
    FUDE_MLKIT_READY,
    FUDE_MLKIT_FAILED               // the model could not be had (fude_mlkit_prepare tries again)
} FUDE_MLKIT_;

// Where the handwriting model is now.
FUDE_MLKIT_ fude_mlkit_state(void);

// The writer's choice, from Settings and saved with them; on by default. When it is off, ML Kit is never called:
// nothing is downloaded, nothing reaches Google, and the app reads handwriting on its own.
void fude_mlkit_set_enabled(b8 _enabled);
b8   fude_mlkit_enabled(void);

// fude_mlkit_set_model sets which handwriting model to use, by ML Kit's language tag ("ja", "pt-BR"); "en-US" until
// it is set, and NULL or "" leave it as it is. The app sets it before the first fude_mlkit_prepare: the model is
// chosen then and kept until the app ends. fude_mlkit_model returns the tag (this module's, always valid).
void      fude_mlkit_set_model(const c8* _tag);
const c8* fude_mlkit_model(void);
// Gets the model ready, downloading it in the background when it is missing. Does nothing while the switch is off,
// while a download is under way, or once the model is ready; fude_mlkit_state follows its progress.
void fude_mlkit_prepare(void);

// Starts reading the alive strokes of _ink (any units, Y up). _pre_context is the text written just before them, which
// helps the model guess, and may be NULL. Returns false when it cannot start: the switch is off, the model is not
// READY, an earlier reading is still under way, or there is no stroke to read.
b8   fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context);

// Returns true once the answer is in, and then ML Kit is idle again. The answer goes into _out (UTF-8, cut to _size):
// the candidates, best first, one per line, each ending in a line break. *_ms gets how long the reading took in
// milliseconds. Either may be NULL. An empty answer is an answer too: it means nothing could be read.
b8   fude_mlkit_poll(c8* _out, usize _size, f64* _ms);

#endif
