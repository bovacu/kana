// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SCREEN_IFACE
#define FUDE_SCREEN_IFACE

#include "rde.h"
#include "drawing/widgets/row.h"

// ===========================================================================
// Screen: the interface of what fills the screen over the page (Browse, the viewer, an exam, Practice...). Each module
// that is a screen fills a fude_screen with its own functions (FUDE_<NAME>_SCREEN, at the end of its .c), and the app
// keeps the screens in order (app.h): the first open one is on top, and only it gets the pointer, the frame, Escape
// and its row of buttons.
//
// Where it sits: drawing/app. The study apps' screens (fude/study/screens, the welcome) and the canvas apps implement
// it; app.c and ui.c call it.
//
// Its main types and their lifecycle: a fude_screen is a constant table of functions. Its state (`self`) belongs to
// the module that is the screen, which the app holds in a fude_screen_slot for the app's life.
//
// Threads: main thread only.
//
// Design: a screen does not open another one itself, so the ways between screens stay in one place. What it hands on
// (a character tapped, a word to practise) it either keeps for its update to pass to a verb (study.h:
// fude_study_view, fude_study_practice...), or passes through one from a button of its row.
// ===========================================================================

struct fude_app;

// How the pointer reaches a screen (fude_app_screen_event).
typedef enum {
    // Lists and cards: the pen, a finger or the mouse points (a finger writes on a pad only with the hand on).
    FUDE_SCREEN_INPUT_POINT = 0,
    // A writing surface (Practice): the pen writes, a finger only with the hand on, and a pen takes over from a finger.
    FUDE_SCREEN_INPUT_WRITE,
} FUDE_SCREEN_INPUT_;

// Where a screen draws, in the app's screen space (centre origin, Y up): from top down to bottom, which is clear of
// its row of buttons. The UI fills it in (ui.h: fude_ui_frame).
typedef struct {
    rde_window* window;
    rde_font*   font;       // The UI font, at font_px.
    f32         font_px;
    f32         top;        // Below the safe area, and below its own bar if it has one.
    f32         bottom;     // Above its row.
    f32         row_h;      // Its row's height (0 when it has no row).
} fude_screen_frame;

// A screen's functions and how it behaves. _self is always the screen's own state (its fude_screen_slot.self).
typedef struct fude_screen {
    const c8* name;
    u8        input;     // FUDE_SCREEN_INPUT_
    b8        overlay;   // It is drawn over what is under it (the welcome), not instead of it.
    // Its taps can tick items in a study app's Select mode (Browse, the chart); the mode ends when no such screen is
    // open.
    b8        selects;

    b8   (*is_open)(const any _self);
    // Escape (and Back): closes it, or takes a step back inside it (the album's page). NULL: Escape does nothing.
    void (*close)(any _self);
    // Called when it is on top again because what was over it closed (NULL when there is nothing to do). The viewer
    // replays.
    void (*resume)(any _self);
    // Its frame: what moves by itself, keys, and what it hands on (the verbs: study.h).
    void (*update)(struct fude_app* _app, any _self, f32 _dt);
    void (*render)(any _self, const fude_screen_frame* _frame);

    // One pointer at a time: the pen, a finger or the mouse, at _at in screen space. _pen says it writes where a pen
    // would (a pen, the mouse, or a finger with the hand on). NULL pointer_down: the screen takes no pointer. A screen
    // that takes it gives all three, since a press is always followed by its moves and its release, or its cancel
    // (the adapters below define the three together).
    void (*pointer_down)(any _self, rde_vec_2F _at, b8 _pen, f64 _now);
    void (*pointer_moved)(any _self, rde_vec_2F _at, f64 _now);
    void (*pointer_up)(any _self, rde_vec_2F _at, f64 _now);
    // The finger was taken away instead of lifted: the system cancelled the touch (a hand resting on the glass as a
    // pen comes down, an edge gesture, a window over the app). What it was doing ends, a stroke as if lifted, and
    // nothing it pressed acts, since the user chose nothing: a cancelled touch is no tap. NULL: the screen is told
    // nothing, and its next pointer_down starts afresh (a list's press or drag just begins again).
    void (*pointer_cancel)(any _self, f64 _now);

    // Its rows of buttons (row.h): the ones it has; which is up now (FUDE_ROW_NONE: none); and how each button of that
    // row shows, written into a zeroed array of FUDE_ROW_BUTTONS faces (NULL: as declared).
    const fude_row_def* rows;
    u32                 row_count;
    u32  (*row)(const any _self);
    void (*faces)(const any _self, u32 _row, fude_row_face* _faces);

    // A bar across its top (filterbar.h: chips to filter and sort, a search field, toggles); NULL when it has none.
    const struct fude_filterbar_def* bar;

    // A field of its own at the top right (Check's "I meant…"): its hint (FUDE_TEXT_COUNT: no field), its most
    // characters, what Return does with its text, and what every keystroke does (a search's; NULL: nothing until
    // Return). The field is emptied each time the screen opens.
    u32  field_hint;
    u32  field_max;
    void (*field_submit)(any _self, const c8* _text);
    void (*field_change)(any _self, const c8* _text);

    // The characters in view, in order, as records into the character data: what its Practice and Select's All take.
    // It returns how many and points *_records at the screen's own list, valid until the screen changes it. NULL: it
    // has none.
    u32  (*in_view)(any _self, const u32** _records);
} fude_screen;

// A screen and its state, as the app keeps it (vt NULL for an unused slot).
typedef struct {
    const fude_screen* vt;
    any                self;
} fude_screen_slot;

// The adapters most screens need, for a module whose functions have the usual shape: fude_X_close(X*),
// fude_X_pointer_down(X*, at, [pen,] now), fude_X_pointer_moved(X*, at, now), fude_X_pointer_up(X*, now), and an `open`
// flag. FUDE_SCREEN_ADAPTERS defines _X_screen_is_open, _close, _down, _moved and _up for its fude_screen;
// FUDE_SCREEN_ADAPTERS_PEN does the same for a pointer_down that takes whether it writes. FUDE_SCREEN_RENDER defines
// _X_screen_render, for fude_X_render(X*, window, font, px, top, bottom).
#define FUDE_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                        \
    RDE_INTERNAL b8   _x##_screen_is_open(const any _self) { return ((const _type*)_self)->open; }                                   \
    RDE_INTERNAL void _x##_screen_close(any _self) { _x##_close((_type*)_self); }                                                    \
    RDE_INTERNAL void _x##_screen_moved(any _self, rde_vec_2F _at, f64 _now) { _x##_pointer_moved((_type*)_self, _at, _now); }      \
    RDE_INTERNAL void _x##_screen_up(any _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_at); _x##_pointer_up((_type*)_self, _now); }
#define FUDE_SCREEN_ADAPTERS(_type, _x)                                                                                               \
    FUDE_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                            \
    RDE_INTERNAL void _x##_screen_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); _x##_pointer_down((_type*)_self, _at, _now); }
#define FUDE_SCREEN_ADAPTERS_PEN(_type, _x)                                                                                           \
    FUDE_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                            \
    RDE_INTERNAL void _x##_screen_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) { _x##_pointer_down((_type*)_self, _at, _pen, _now); }
#define FUDE_SCREEN_RENDER(_type, _x)                                                                                                 \
    RDE_INTERNAL void _x##_screen_render(any _self, const fude_screen_frame* _f) {                                                   \
        _x##_render((_type*)_self, _f->window, _f->font, _f->font_px, _f->top, _f->bottom);                                            \
    }

#endif
