// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef HANZI_APP_H
#define HANZI_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Hanzi's own part of the app: what it is (HANZI_INFO), and what it adds to the
// drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Hanzi, Album, Reviews, Vocabulary, Lectures, Exams,
// Statistics), the welcome as the tutorial and on the very first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    HANZI_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    HANZI_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    HANZI_SCREEN_PRACTICE,      // over whatever opened it
    HANZI_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    HANZI_SCREEN_SCAN,
    HANZI_SCREEN_WORDEXAM,      // over the Vocabulary
    HANZI_SCREEN_EXAM,          // over the album (a kept exam)
    HANZI_SCREEN_STATS,
    HANZI_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Chinese)
    HANZI_SCREEN_VOCAB,
    HANZI_SCREEN_CHECK,
    HANZI_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    HANZI_SCREEN_ALBUM,
    HANZI_SCREEN_BROWSE,
    HANZI_SCREEN_COUNT
} HANZI_SCREEN_;

// Hanzi: the study app (study.h: first, so a fude_app* is a hanzi_app*), and its
// own screen: the welcome.
typedef struct hanzi_app {
    fude_study    study;
    fude_welcome* welcome;
    fude_library* library;
} hanzi_app;

#define HANZI_APP(_app) ((hanzi_app*)(_app))

extern const fude_app_info  HANZI_INFO;
extern const fude_extension HANZI_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --welcome[=PAGE],
// --library. Called where the core's are (look.h).
void hanzi_look_args(i32 _argc, c8** _argv);
void hanzi_look_start(fude_app* _app);
void hanzi_look_loaded(fude_app* _app);
void hanzi_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let go first).
void hanzi_app_open(fude_app* _app, HANZI_SCREEN_ _screen);

#endif
