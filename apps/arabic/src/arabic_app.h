// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef ARABIC_APP_H
#define ARABIC_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "lang/ar/chart.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Arabic's own part of the app: what it is (ARABIC_INFO), and what it adds to
// the drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Alphabet — few enough letters for one table — Album,
// Reviews, Vocabulary, Lectures, Exams, Statistics), the welcome as the
// tutorial and on the very first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    ARABIC_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    ARABIC_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    ARABIC_SCREEN_PRACTICE,      // over whatever opened it
    ARABIC_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    ARABIC_SCREEN_SCAN,
    ARABIC_SCREEN_WORDEXAM,      // over the Vocabulary
    ARABIC_SCREEN_EXAM,          // over the album (a kept exam)
    ARABIC_SCREEN_STATS,
    ARABIC_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Japanese)
    ARABIC_SCREEN_VOCAB,
    ARABIC_SCREEN_CHECK,
    ARABIC_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    ARABIC_SCREEN_ALBUM,
    ARABIC_SCREEN_CHART,
    ARABIC_SCREEN_BROWSE,
    ARABIC_SCREEN_COUNT
} ARABIC_SCREEN_;

// Arabic: the study app (study.h: first, so a fude_app* is an arabic_app*), and
// its own screens: Arabic's chart (the letters and their joined forms) and the welcome.
typedef struct arabic_app {
    fude_study    study;
    fude_chart*   chart;
    fude_welcome* welcome;
    fude_library* library;
} arabic_app;

#define ARABIC_APP(_app) ((arabic_app*)(_app))

extern const fude_app_info  ARABIC_INFO;
extern const fude_extension ARABIC_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --chart (the
// chart), --welcome[=PAGE]. Called where the core's are (look.h).
void arabic_look_args(i32 _argc, c8** _argv);
void arabic_look_start(fude_app* _app);
void arabic_look_loaded(fude_app* _app);
void arabic_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let
// go first; Browse and the chart are one or the other).
void arabic_app_open(fude_app* _app, ARABIC_SCREEN_ _screen);

#endif
