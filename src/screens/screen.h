#ifndef KANA_SCREEN_IFACE
#define KANA_SCREEN_IFACE

#include "rde.h"
#include "widgets/row.h"

// ===========================================================================
// A screen: what fills the screen over the page — Browse, the viewer, an exam,
// Practice... Each module that is one fills a kana_screen with its own
// functions (KANA_<NAME>_SCREEN, at the end of its .c) and the app keeps them
// in order (app.h): the first open one is on top, and only it gets the
// pointer, the frame, Escape and its row of buttons.
//
// A screen does not open another one itself: what it hands on (a character
// tapped, a word to practise) it either keeps for its update to pass to a verb
// of the app's (app.h: kana_app_view, kana_app_practice...), or does through
// one from a button of its row.
// ===========================================================================

struct kana_app;

// How the pointer reaches a screen (kana_app_screen_event).
typedef enum {
    KANA_SCREEN_INPUT_POINT = 0,   // lists and cards: the pen, a finger or the mouse points (a finger writes on a pad only with the hand on)
    KANA_SCREEN_INPUT_WRITE,       // a writing surface (Practice): the pen writes, a finger only with the hand on, and a pen takes over from it
} KANA_SCREEN_INPUT_;

// Where it draws, Kana's screen space (centre origin, Y up): from _top down to
// _bottom, which is clear of its row of buttons.
typedef struct {
    rde_window* window;
    rde_font*   font;       // the UI font, at font_px
    f32         font_px;
    f32         top;        // under the safe area (and its own bar, if it has one)
    f32         bottom;     // over its row
    f32         row_h;      // its row's height (0: none shown)
} kana_screen_frame;

typedef struct kana_screen {
    const c8* name;
    u8        input;     // KANA_SCREEN_INPUT_
    b8        overlay;   // drawn over what is under it (the welcome), not instead of it

    b8   (*is_open)(const void* _self);
    // Escape (and Back): closed — or a step back inside it (the album's page). NULL: Escape does nothing.
    void (*close)(void* _self);
    // On top again, what was over it gone (NULL: nothing to do). The viewer replays.
    void (*resume)(void* _self);
    // Its frame: what moves by itself, keys, and what it hands on (app.h's verbs).
    void (*update)(struct kana_app* _app, void* _self, f32 _dt);
    void (*render)(void* _self, const kana_screen_frame* _frame);

    // One pointer at a time: the pen, a finger or the mouse. _pen: it writes where
    // a pen would (a pen, the mouse, or a finger with the hand on).
    void (*pointer_down)(void* _self, rde_vec_2F _at, b8 _pen, f64 _now);
    void (*pointer_moved)(void* _self, rde_vec_2F _at, f64 _now);
    void (*pointer_up)(void* _self, rde_vec_2F _at, f64 _now);

    // Its rows of buttons (row.h): the ones it has, which is up now
    // (KANA_ROW_NONE: none), and how each button of that one shows (NULL: as declared).
    const kana_row_def* rows;
    u32                 row_count;
    u32  (*row)(const void* _self);
    void (*faces)(const void* _self, u32 _row, kana_row_face* _faces);

    // A bar across its top (filterbar.h: chips to filter and sort, a search
    // field, toggles): NULL, none.
    const struct kana_filterbar_def* bar;

    // A field of its own at the top right (Check's "I meant…"): its hint
    // (KANA_TEXT_COUNT: none), its length, and what Return does with it.
    u32  field_hint;
    u32  field_max;
    void (*field_submit)(void* _self, const c8* _text);

    // The characters in view, in order (records into the character data): what
    // its Practice and Select's All take. NULL: it has none.
    u32  (*in_view)(void* _self, const u32** _records);
} kana_screen;

// A screen and its state, as the app keeps it.
typedef struct {
    const kana_screen* vt;
    void*              self;
} kana_screen_slot;

// The adapters most screens need, for functions in the usual shape —
// kana_X_close(X*), kana_X_pointer_down(X*, at, [pen,] now), kana_X_pointer_moved(X*,
// at, now), kana_X_pointer_up(X*, now) and an `open` flag: _X_screen_is_open,
// _close, _down, _moved and _up, for its kana_screen (_PEN: its pointer_down
// takes whether it writes). KANA_SCREEN_RENDER: _X_screen_render, for
// kana_X_render(X*, window, font, px, top, bottom).
#define KANA_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                        \
    RDE_INTERNAL b8   _x##_screen_is_open(const void* _self) { return ((const _type*)_self)->open; }                                   \
    RDE_INTERNAL void _x##_screen_close(void* _self) { _x##_close((_type*)_self); }                                                    \
    RDE_INTERNAL void _x##_screen_moved(void* _self, rde_vec_2F _at, f64 _now) { _x##_pointer_moved((_type*)_self, _at, _now); }      \
    RDE_INTERNAL void _x##_screen_up(void* _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_at); _x##_pointer_up((_type*)_self, _now); }
#define KANA_SCREEN_ADAPTERS(_type, _x)                                                                                               \
    KANA_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                            \
    RDE_INTERNAL void _x##_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); _x##_pointer_down((_type*)_self, _at, _now); }
#define KANA_SCREEN_ADAPTERS_PEN(_type, _x)                                                                                           \
    KANA_SCREEN_ADAPTERS_COMMON(_type, _x)                                                                                            \
    RDE_INTERNAL void _x##_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) { _x##_pointer_down((_type*)_self, _at, _pen, _now); }
#define KANA_SCREEN_RENDER(_type, _x)                                                                                                 \
    RDE_INTERNAL void _x##_screen_render(void* _self, const kana_screen_frame* _f) {                                                   \
        _x##_render((_type*)_self, _f->window, _f->font, _f->font_px, _f->top, _f->bottom);                                            \
    }

#endif
