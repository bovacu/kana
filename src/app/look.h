#ifndef KANA_LOOK
#define KANA_LOOK

#include "rde.h"

// ===========================================================================
// A developer's looks and measures: launch flags that open a screen in a given
// state, take a screenshot and quit (to check a layout without a device), and
// measure (--perf, --mlkit-samples, --translate-probe). Only a developer's build
// reads its arguments (kana.c: main); a release is launched with none.
//
//   --size=744x1133    the window (an iPad mini's points)   --theme=N   a theme, shown not saved
//   --shot=FILE        a screenshot once the screen has settled, then quit
//   --side --settings --licences=N --data --welcome[=PAGE] --paper
//   --browse --kana --viewer=HEX [--note=TEXT] --practice=HEX --guided=HEX --sheet=FILE
//   --album --album-exams --album-page=HEX --kept-exam=N --stats [--scroll=PX]
//   --exam --exam-start --vocab[=N] --vocab-sample --word-exam[=STAGE] --word-card=TEXT
//   --paste-text=TEXT [--deselect] [--save-selection] [--translate-selection]
//   --scan-demo=PNG [--scan-demo-turn=DEG] [--scan-demo-top-first] [--scan-demo-translate] --scan-live
//   --data-export=FILE --data-import=FILE [--data-replace] --trim-fonts
//   --press=I,J,...  the screen on top's row's buttons, pressed in turn   --language=N
//   --perf=SECONDS  --mlkit-samples=0-27,28-58  --translate-probe=xx
// (COMMANDS.txt has what each is for.)
// ===========================================================================

struct kana_app;

// The flags read (before anything is built: some change what is).
void kana_look_args(i32 _argc, c8** _argv);
// The screens asked for, opened — before the saves load, and after (those that need them).
void kana_look_start(struct kana_app* _app);
void kana_look_loaded(struct kana_app* _app);
// Once a frame: a probe's next step, a shot's sequence (a frame count in).
void kana_look_frame(struct kana_app* _app);
// The engine's update and render, timed when --perf asks (frame times over its
// seconds, appended to <save dir>/perf.txt).
void kana_look_timed_update(void (*_update)(f32), f32 _dt);
void kana_look_timed_render(struct kana_app* _app, void (*_render)(rde_window*, f32), rde_window* _window, f32 _dt);

#endif
