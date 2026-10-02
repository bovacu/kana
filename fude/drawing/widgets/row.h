#ifndef FUDE_ROW
#define FUDE_ROW

#include "rde.h"

// ===========================================================================
// A row of buttons, each an icon over its label: the bar at the bottom of every
// screen, the menu over the lasso's selection, the page's context menu.
//
// A row is DECLARED as data — its buttons, what each says and does, how it looks
// (fude_row_def) — and each frame its owner says how each button shows now
// (fude_row_face: a count in its label, greyed out, chosen); the row changes
// only what differs. A button's press calls its function with the app and the
// row's owner (a screen: screen.h), then the UI catches up (fude_ui_update).
//
// The declaration knows no widget: a screen declares its rows without RDE's UI.
// The widget (fude_row, below) is built from it on the UI canvas with the kit
// (kit.h), and again when the language changes.
// ===========================================================================

struct fude_app;

#define FUDE_ROW_BUTTONS 8u            // a row's buttons, at most
#define FUDE_ROW_NONE    UINT32_MAX    // no row (screen.h: row)
#define FUDE_ROW_LABEL   48u

typedef enum {
    FUDE_ROW_QUIET = 0,   // no background until pressed
    FUDE_ROW_PRIMARY,     // the way on: the accent
    FUDE_ROW_DANGER,      // destructive: in red
    FUDE_ROW_PLAIN        // a toggle that shows off as a plain button (Translate with Google on a photo)
} FUDE_ROW_LOOK_;

// What a button does: _self is the row's owner (the screen), _arg the button's own.
typedef void (*fude_row_press)(struct fude_app* _app, void* _self, u32 _arg);

typedef struct {
    u32            text;      // its label (FUDE_TEXT_); a counted one ("Practice {0}") shows 0 until its face says
    const c8*      icon;      // icons.h, or a character
    fude_row_press press;
    u32            arg;       // handed to press
    u8             look;      // FUDE_ROW_LOOK_
    b8             counted;   // its label ends in a count: room for three digits
    b8           (*present)(void);   // NULL: always there; false: left out of the row (Translate without a translator)
} fude_row_button;

typedef struct {
    const fude_row_button* buttons;
    u32                    count;
} fude_row_def;

#define FUDE_ROW_DEF(_buttons) { (_buttons), (u32)(sizeof(_buttons) / sizeof((_buttons)[0])) }

// Which of _def's buttons does _press (FUDE_ROW_NONE: none): a row put together
// from others' buttons (the page's menus) finds theirs.
u32 fude_row_def_find(const fude_row_def* _def, fude_row_press _press);

// A button that calls one function of its owner, nothing else:
// FUDE_ROW_CALL(fude_exam_row_undo, fude_exam, fude_exam_undo).
#define FUDE_ROW_CALL(_name, _type, _fn) \
    RDE_INTERNAL void _name(struct fude_app* _app, void* _self, u32 _arg) { RDE_UNUSED(_app); RDE_UNUSED(_arg); _fn((_type*)_self); }

// How a button shows now. Zeroed, it is as declared.
typedef struct {
    c8        label[FUDE_ROW_LABEL];   // "": its own label
    const c8* icon;                    // NULL: its own icon
    b8        disabled;
    b8        selected;                // chosen (a toggle on, the sort shown): the accent's tint, a Fill icon
} fude_row_face;

// --- the widget ------------------------------------------------------------------------

struct fude_row;
typedef struct {
    struct fude_row* row;
    u32              index;
} fude_row_ref;

typedef struct fude_row {
    const fude_row_def* def;
    struct fude_app*    app;
    void*               self;                        // the owner, handed to presses
    void              (*after)(struct fude_app* _app);   // after a press: the UI catches up
    rde_ui_image*       panel;
    rde_ui_button*      buttons[FUDE_ROW_BUTTONS];   // NULL: left out
    fude_row_ref        refs[FUDE_ROW_BUTTONS];
    fude_row_face       shown[FUDE_ROW_BUTTONS];     // the faces as applied
    c8                  base[FUDE_ROW_BUTTONS][FUDE_ROW_LABEL];   // each label as built
    b8                  open;
    rde_vec_2F          center;                      // UI canvas units
    rde_vec_2F          size;
} fude_row;

// Built under _root from _def (hidden): each button as wide as its label needs.
// The row must not move after this (its buttons point back at it).
void fude_row_create(fude_row* _row, rde_ui_node* _root, const fude_row_def* _def, struct fude_app* _app, void* _self, void (*_after)(struct fude_app*));
// Shown centred at _center (UI units, kept on screen), or hidden. Touches the UI only on a change.
void fude_row_show(fude_row* _row, rde_window* _window, b8 _show, rde_vec_2F _center);
// The usual place: across the bottom, over the safe area.
rde_vec_2F fude_row_bottom(const fude_row* _row, rde_window* _window);
// The buttons as _faces say (count: the row's buttons); only what changed is touched.
void fude_row_apply(fude_row* _row, const fude_row_face* _faces);
// Every button styled again from the theme (its look, and its face over it).
void fude_row_restyle(fude_row* _row);
// Is _ui (UI canvas units) on the row, while it shows?
b8   fude_row_hit(const fude_row* _row, rde_vec_2F _ui);
// The row's height when shown (every row is as tall).
f32  fude_row_height(void);

// A face's label set from a counted text ("Start {0}" with _n).
void fude_row_face_count(fude_row_face* _face, u32 _text, u32 _n);
// Next's face, Finish (its label and icon) on the last one.
void fude_row_face_next(fude_row_face* _face, b8 _last);

#endif
