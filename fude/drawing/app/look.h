#ifndef FUDE_LOOK
#define FUDE_LOOK

#include "rde.h"

// ===========================================================================
// A developer's looks and measures: launch flags that open a screen in a given
// state, take a screenshot and quit (to check a layout without a device), and
// measure (--perf). Only a developer's build reads its arguments (the app's
// main); a release is launched with none. These are the core's:
//
//   --size=744x1133    the window (an iPad mini's points)   --theme=N   a theme, shown not saved
//   --shot=FILE        a screenshot once the screen has settled, then quit
//   --stay             the same sequence (presses, pastes...) without the shot, and
//                      the app carries on: for a screenshot taken from outside (the Simulator's)
//   --side --settings --licences=N --data --paper
//   --data-export=FILE --data-import=FILE [--data-replace] --deselect --trim-fonts
//   --press=I,J,...  the screen on top's row's buttons, pressed in turn   --language=N
//   --rtl            the UI right to left, whatever the language
//   --swipe=DX       a finger dragged DX across the screen on top
//   --perf=SECONDS
//
// An app reads its own the same way (study/app/look.c, Kana's kana_app.c), with
// fude_look_value and fude_look_is over the same arguments, and times what it
// does in a shot's sequence by fude_look_shot_frame. (COMMANDS.txt has what each
// flag is for.)
// ===========================================================================

struct fude_app;

// The flags read (before anything is built: some change what is).
void fude_look_args(i32 _argc, c8** _argv);
// The screens asked for, opened — before the saves load, and after (those that need them).
void fude_look_start(struct fude_app* _app);
void fude_look_loaded(struct fude_app* _app);
// Once a frame: a shot's sequence (a frame count in).
void fude_look_frame(struct fude_app* _app);
// The engine's update and render, timed when --perf asks (frame times over its
// seconds, appended to <save dir>/perf.txt).
void fude_look_timed_update(void (*_update)(f32), f32 _dt);
void fude_look_timed_render(struct fude_app* _app, void (*_render)(rde_window*, f32), rde_window* _window, f32 _dt);

// For an app's own flags: the flag's value when _arg is _flag=value (NULL
// otherwise); whether _arg is _flag.
const c8* fude_look_value(const c8* _arg, const c8* _flag);
b8        fude_look_is(const c8* _arg, const c8* _flag);
// This frame of a shot's sequence (fude_look_frame counts them; 0: no --shot):
// an app does what its flags ask at the frames they ask.
u32       fude_look_shot_frame(void);
// --rtl: the UI laid out right to left whatever the language.
b8        fude_look_rtl(void);
// --perf's summary: what the app adds to its line (Kana: the camera's frames).
void      fude_look_perf_note(void (*_note)(struct fude_app* _app, c8* _out, usize _size));

#endif
