// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_LOOK
#define FUDE_LOOK

#include "rde.h"

// ===========================================================================
// Look harness: a developer's looks and measures. Launch flags open a screen in a given state, take a screenshot and
// quit (to check a layout without a device), or measure (--perf). Only a developer's build reads its arguments (the
// app's main); a release is launched with none. These flags are the core's:
//
//   --size=744x1133    the window (an iPad mini's points)   --theme=N   a theme, shown but not saved
//   --shot=FILE        a screenshot once the screen has settled, then quit
//   --shot-at=N        ...at frame N instead (for an app's own sequence that takes longer)
//   --stay             the same sequence (presses, pastes...) without the shot, and the app carries on: for a
//                      screenshot taken from outside (the Simulator's)
//   --side --faq --settings --licences=N --data --paper
//   --settings-end     Settings scrolled to its end, where the app's own sections are
//   --data-export=FILE --data-import=FILE [--data-replace] --deselect --trim-fonts
//   --press=I,J,...  presses the top screen's row's buttons in turn   --language=N
//   --rtl            the UI right to left, whatever the language
//   --swipe=DX       a finger dragged DX across the screen on top
//   --perf=SECONDS
// look.c also reads --ui-size=N, --note-card=... and the document flags (--doc, --book, --doc-view, --mark,
// --lasso-area, --doc-search, --doc-page, --doc-export, --doc-turn), each described at its field there; the crash
// card reads --crash-card.
//
// An app reads its own flags the same way (the study apps' in study/app/look.c, the canvas apps' in
// zoom/look.c), with fude_look_value and fude_look_is over the same arguments, and times what it does
// in a shot's sequence by fude_look_shot_frame. (COMMANDS.txt says what each flag is for.)
//
// Where it sits: drawing/app. The shells call it at launch and each frame; the crash card and the UI ask it whether a
// look is running.
//
// Its state: one look per process, kept in look.c's statics.
//
// Threads: main thread only.
//
// Files: writes the screenshot (--shot), the files of --data-export, and <save dir>/perf.txt for --perf.
// ===========================================================================

struct fude_app;

// Reads the flags, before anything is built, since some change what is built. The arguments must stay valid for the
// app's life: they are read again later.
void fude_look_args(i32 _argc, c8** _argv);
// Opens the screens the flags ask for: fude_look_start before the saves load, fude_look_loaded after (for those that
// need them).
void fude_look_start(struct fude_app* _app);
void fude_look_loaded(struct fude_app* _app);
// Called once a frame: moves a shot's sequence on (it counts the frames).
void fude_look_frame(struct fude_app* _app);
// Runs the engine's update and render, timing them when --perf asks (frame times over its seconds, appended to
// <save dir>/perf.txt).
void fude_look_timed_update(void (*_update)(f32), f32 _dt);
void fude_look_timed_render(struct fude_app* _app, void (*_render)(rde_window*, f32), rde_window* _window, f32 _dt);

// For an app's own flags: fude_look_value returns the value when _arg is "_flag=value" (a pointer into _arg), and
// NULL otherwise; fude_look_is says whether _arg is exactly _flag.
const c8* fude_look_value(const c8* _arg, const c8* _flag);
b8        fude_look_is(const c8* _arg, const c8* _flag);
// The current frame of a shot's sequence, as fude_look_frame counts them (0 when there is no shot running), so an app
// does what its flags ask at the frames they ask.
u32       fude_look_shot_frame(void);
// Whether the look harness owns the pointer: while a screenshot is taken (--shot) the real mouse is ignored, since
// where it happens to sit over the window would change the picture (the canvas draws its brush under it).
b8        fude_look_owns_pointer(void);
// Whether --rtl was given: the UI is laid out right to left whatever the language.
b8        fude_look_rtl(void);
// Whether a look is running (arguments were given), so that what would cover its shots (the crash card) is left out.
b8        fude_look_given(void);
// Whether _flag was given (the crash card's --crash-card shows the card in a look all the same).
b8        fude_look_has(const c8* _flag);
// Sets what the app adds to --perf's summary line (Kana: the camera's frames).
void      fude_look_perf_note(void (*_note)(struct fude_app* _app, c8* _out, usize _size));

#endif
