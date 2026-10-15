// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LOOK_H
#define FUDE_ZOOM_LOOK_H

#include "rde.h"
#include "zoom/page.h"
#include "zoom/home.h"

// ===========================================================================
// Canvas looks: the canvas apps' own launch flags, a developer's (drawing/app/look.h has the core's, and how they are
// read). They draw demos, put the page into a state as a hand would and press its buttons at given frames, so that a
// screenshot or a measurement shows one state of the app. look.c lists every flag, with what it does; COMMANDS.txt
// says what each is for.
//
// Where it sits: zoom, beside page_look.c (the page's own harness: the fude_zoom_page_look_* functions most flags
// call). The canvas shell (shell.h) calls it once at launch and once a frame. As the study's looks (study/app/look.c),
// it reads only the arguments the shell is given, and a release is given none (unless it is built with
// FUDE_ALLOW_ARGS), so only a developer's build does anything here.
// Its state: one look per process, kept in look.c's statics. The app, the page and the home screen it acts on are the
// shell's, and must outlive it (they live for the whole run).
// Threads: main thread only.
// Files: reads what its flags name (--demo-picture, --pdf, --mermaid, --audio-import, a --text's file:PATH), and
// writes what --export asks for.
// ===========================================================================

struct fude_app;

// Reads the canvas's flags from the launch arguments, and does at once what they ask at launch (--demo-deep,
// --demo-erase, --gate, --demo-picture, --paper-is, --pan, --zoom); the rest are kept for fude_zoom_look_frame. Called
// once, at the end of the shell's init, after the saves are loaded and the app has opened on its first screen. _app,
// _page and _home are the shell's, and the arguments must stay valid for the whole run. With no arguments past the
// program's name it does nothing, and so does every frame after.
void fude_zoom_look_start(struct fude_app* _app, fude_zoom_page* _page, fude_zoom_home* _home, i32 _argc, c8** _argv);
// Counts the frame and does what the flags ask at this frame, in a fixed order. Called once a frame by the shell's
// update, after the page's (or the screen's) update and before the UI's.
void fude_zoom_look_frame(void);

#endif
