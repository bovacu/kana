// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef HINDI_APP_H
#define HINDI_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "lang/hi/chart.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Hindi's own part of the app: what it is (HINDI_INFO), and what it adds to
// the drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Alphabet — few enough letters for one table — Album,
// Reviews, Vocabulary, Lectures, Exams, Statistics), the welcome as the
// tutorial and on the very first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    HINDI_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    HINDI_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    HINDI_SCREEN_PRACTICE,      // over whatever opened it
    HINDI_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    HINDI_SCREEN_SCAN,
    HINDI_SCREEN_WORDEXAM,      // over the Vocabulary
    HINDI_SCREEN_EXAM,          // over the album (a kept exam)
    HINDI_SCREEN_STATS,
    HINDI_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Japanese)
    HINDI_SCREEN_VOCAB,
    HINDI_SCREEN_CHECK,
    HINDI_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    HINDI_SCREEN_ALBUM,
    HINDI_SCREEN_CHART,
    HINDI_SCREEN_BROWSE,
    HINDI_SCREEN_COUNT
} HINDI_SCREEN_;

// Hindi: the study app (study.h: first, so a fude_app* is a hindi_app*), and
// its own screens: Hindi's chart (the Devanagari alphabet) and the welcome.
typedef struct hindi_app {
    fude_study    study;
    fude_chart*   chart;
    fude_welcome* welcome;
    fude_library* library;
} hindi_app;

#define HINDI_APP(_app) ((hindi_app*)(_app))

extern const fude_app_info  HINDI_INFO;
extern const fude_extension HINDI_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --chart (the
// chart), --welcome[=PAGE]. Called where the core's are (look.h).
void hindi_look_args(i32 _argc, c8** _argv);
void hindi_look_start(fude_app* _app);
void hindi_look_loaded(fude_app* _app);
void hindi_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let
// go first; Browse and the chart are one or the other).
void hindi_app_open(fude_app* _app, HINDI_SCREEN_ _screen);

#endif
