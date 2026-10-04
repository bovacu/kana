// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef THAI_APP_H
#define THAI_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "lang/th/chart.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Thai's own part of the app: what it is (THAI_INFO), and what it adds to
// the drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Alphabet — few enough letters for one table — Album,
// Reviews, Vocabulary, Lectures, Exams, Statistics), the welcome as the
// tutorial and on the very first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    THAI_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    THAI_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    THAI_SCREEN_PRACTICE,      // over whatever opened it
    THAI_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    THAI_SCREEN_SCAN,
    THAI_SCREEN_WORDEXAM,      // over the Vocabulary
    THAI_SCREEN_EXAM,          // over the album (a kept exam)
    THAI_SCREEN_STATS,
    THAI_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Japanese)
    THAI_SCREEN_VOCAB,
    THAI_SCREEN_CHECK,
    THAI_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    THAI_SCREEN_ALBUM,
    THAI_SCREEN_CHART,
    THAI_SCREEN_BROWSE,
    THAI_SCREEN_COUNT
} THAI_SCREEN_;

// Thai: the study app (study.h: first, so a fude_app* is a thai_app*), and
// its own screens: Thai's chart (the Thai alphabet) and the welcome.
typedef struct thai_app {
    fude_study    study;
    fude_chart*   chart;
    fude_welcome* welcome;
    fude_library* library;
} thai_app;

#define THAI_APP(_app) ((thai_app*)(_app))

extern const fude_app_info  THAI_INFO;
extern const fude_extension THAI_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --chart (the
// chart), --welcome[=PAGE]. Called where the core's are (look.h).
void thai_look_args(i32 _argc, c8** _argv);
void thai_look_start(fude_app* _app);
void thai_look_loaded(fude_app* _app);
void thai_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let
// go first; Browse and the chart are one or the other).
void thai_app_open(fude_app* _app, THAI_SCREEN_ _screen);

#endif
