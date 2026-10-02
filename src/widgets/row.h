#ifndef KANA_ROW
#define KANA_ROW

#include "rde.h"

// ===========================================================================
// A row of buttons, each an icon over its label: the bar at the bottom of every
// screen, the menu over the lasso's selection, the page's context menu.
//
// A row is DECLARED as data — its buttons, what each says and does, how it looks
// (kana_row_def) — and each frame its owner says how each button shows now
// (kana_row_face: a count in its label, greyed out, chosen); the row changes
// only what differs. A button's press calls its function with the app and the
// row's owner (a screen: screen.h), then the UI catches up (kana_ui_update).
//
// The declaration knows no widget: a screen declares its rows without RDE's UI.
// The widget (kana_row, below) is built from it on the UI canvas with the kit
// (kit.h), and again when the language changes.
// ===========================================================================

struct kana_app;

#define KANA_ROW_BUTTONS 8u            // a row's buttons, at most
#define KANA_ROW_NONE    UINT32_MAX    // no row (screen.h: row)
#define KANA_ROW_LABEL   48u

typedef enum {
    KANA_ROW_QUIET = 0,   // no background until pressed
    KANA_ROW_PRIMARY,     // the way on: the accent
    KANA_ROW_DANGER,      // destructive: in red
    KANA_ROW_PLAIN        // a toggle that shows off as a plain button (Translate with Google on a photo)
} KANA_ROW_LOOK_;

// What a button does: _self is the row's owner (the screen), _arg the button's own.
typedef void (*kana_row_press)(struct kana_app* _app, void* _self, u32 _arg);

typedef struct {
    u32            text;      // its label (KANA_TEXT_); a counted one ("Practice {0}") shows 0 until its face says
    const c8*      icon;      // icons.h, or a character
    kana_row_press press;
    u32            arg;       // handed to press
    u8             look;      // KANA_ROW_LOOK_
    b8             counted;   // its label ends in a count: room for three digits
    b8           (*present)(void);   // NULL: always there; false: left out of the row (Translate without a translator)
} kana_row_button;

typedef struct {
    const kana_row_button* buttons;
    u32                    count;
} kana_row_def;

#define KANA_ROW_DEF(_buttons) { (_buttons), (u32)(sizeof(_buttons) / sizeof((_buttons)[0])) }

// A button that calls one function of its owner, nothing else:
// KANA_ROW_CALL(kana_exam_row_undo, kana_exam, kana_exam_undo).
#define KANA_ROW_CALL(_name, _type, _fn) \
    RDE_INTERNAL void _name(struct kana_app* _app, void* _self, u32 _arg) { RDE_UNUSED(_app); RDE_UNUSED(_arg); _fn((_type*)_self); }

// How a button shows now. Zeroed, it is as declared.
typedef struct {
    c8        label[KANA_ROW_LABEL];   // "": its own label
    const c8* icon;                    // NULL: its own icon
    b8        disabled;
    b8        selected;                // chosen (a toggle on, the sort shown): the accent's tint, a Fill icon
} kana_row_face;

// --- the widget ------------------------------------------------------------------------

struct kana_row;
typedef struct {
    struct kana_row* row;
    u32              index;
} kana_row_ref;

typedef struct kana_row {
    const kana_row_def* def;
    struct kana_app*    app;
    void*               self;                        // the owner, handed to presses
    void              (*after)(struct kana_app* _app);   // after a press: the UI catches up
    rde_ui_image*       panel;
    rde_ui_button*      buttons[KANA_ROW_BUTTONS];   // NULL: left out
    kana_row_ref        refs[KANA_ROW_BUTTONS];
    kana_row_face       shown[KANA_ROW_BUTTONS];     // the faces as applied
    c8                  base[KANA_ROW_BUTTONS][KANA_ROW_LABEL];   // each label as built
    b8                  open;
    rde_vec_2F          center;                      // UI canvas units
    rde_vec_2F          size;
} kana_row;

// Built under _root from _def (hidden): each button as wide as its label needs.
// The row must not move after this (its buttons point back at it).
void kana_row_create(kana_row* _row, rde_ui_node* _root, const kana_row_def* _def, struct kana_app* _app, void* _self, void (*_after)(struct kana_app*));
// Shown centred at _center (UI units, kept on screen), or hidden. Touches the UI only on a change.
void kana_row_show(kana_row* _row, rde_window* _window, b8 _show, rde_vec_2F _center);
// The usual place: across the bottom, over the safe area.
rde_vec_2F kana_row_bottom(const kana_row* _row, rde_window* _window);
// The buttons as _faces say (count: the row's buttons); only what changed is touched.
void kana_row_apply(kana_row* _row, const kana_row_face* _faces);
// Every button styled again from the theme (its look, and its face over it).
void kana_row_restyle(kana_row* _row);
// Is _ui (UI canvas units) on the row, while it shows?
b8   kana_row_hit(const kana_row* _row, rde_vec_2F _ui);
// The row's height when shown (every row is as tall).
f32  kana_row_height(void);

// A face's label set from a counted text ("Start {0}" with _n).
void kana_row_face_count(kana_row_face* _face, u32 _text, u32 _n);
// Next's face, Finish (its label and icon) on the last one.
void kana_row_face_next(kana_row_face* _face, b8 _last);

#endif
