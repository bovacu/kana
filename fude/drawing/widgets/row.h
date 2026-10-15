// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ROW
#define FUDE_ROW

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Row: a row of buttons, each an icon over its label. It is the bar at the bottom of every screen, the menu over the
// lasso's selection and the page's context menu. Where the buttons do not all fit (on a phone), the row keeps what
// does, always including its primary button and its Back or Close, and a More button opens a card of the others
// above it.
//
// Where it sits: drawing/widgets, built with the kit (kit.h). The screens (screen.h), the page's menus (pagemenu.h)
// and the study apps' screens declare rows and build them.
//
// Its main types and their lifecycle: a row is DECLARED as data (fude_row_def: its buttons, what each says and does,
// how it looks), usually a static constant of its owner. The widget (fude_row) is held by its owner, built from the
// declaration on the UI canvas by fude_row_create, and built again when the language changes. Its nodes belong to the
// UI tree, so it has no destroy function. Each frame the owner says how each button shows now (fude_row_face: a
// count in its label, greyed out, chosen), and the row changes only what differs. A press calls the button's
// function with the app and the row's owner (a screen: screen.h), and then the UI catches up (fude_ui_update).
//
// Threads: main thread only, like all of RDE's UI.
//
// Design: the declaration knows no widget, so a screen can declare its rows without RDE's UI.
// ===========================================================================

struct fude_app;

#define FUDE_ROW_BUTTONS 24u           // The most buttons in a row (a FUDE_ROW_DEF with more does not compile).
#define FUDE_ROW_NONE    UINT32_MAX    // No row, or no button (screen.h: row; fude_row_def_find).
#define FUDE_ROW_LABEL   128u          // The longest label, in bytes.

// How a button looks.
typedef enum {
    FUDE_ROW_QUIET = 0,   // No background until pressed.
    FUDE_ROW_PRIMARY,     // The way forward, in the accent.
    FUDE_ROW_DANGER,      // Destructive, in red.
    FUDE_ROW_PLAIN        // A toggle that shows "off" as a plain button (Translate with Google on a photo).
} FUDE_ROW_LOOK_;

// What a button does when pressed: _self is the row's owner (the screen), and _arg is the button's own value.
typedef void (*fude_row_press)(struct fude_app* _app, any _self, u32 _arg);

// One button of a row, as declared.
typedef struct {
    // Its label (a FUDE_TEXT_ id). A counted one ("Practice {0}") shows 0 until its face says otherwise.
    u32            text;
    const c8*      icon;      // From icons.h, or a character.
    fude_row_press press;
    u32            arg;       // Handed to press.
    u8             look;      // FUDE_ROW_LOOK_
    b8             counted;   // Its label ends in a count, so it is made wide enough for three digits.
    // NULL means it is always there; false leaves it out of the row (Translate without a translator).
    b8           (*present)(void);
} fude_row_button;

// A row as declared: its buttons, in order.
typedef struct {
    const fude_row_button* buttons;
    u32                    count;
} fude_row_def;

// Declares a row from a static array of buttons. A row of more than FUDE_ROW_BUTTONS does not compile, because its last
// buttons would otherwise be left out unseen.
#define FUDE_ROW_DEF(_buttons) { (_buttons), (u32)(sizeof(_buttons) / sizeof((_buttons)[0]) + \
                                 0u * sizeof(char[sizeof(_buttons) / sizeof((_buttons)[0]) <= FUDE_ROW_BUTTONS ? 1 : -1])) }

// Returns which of _def's buttons calls _press, or FUDE_ROW_NONE when none does. A row put together from other rows'
// buttons (the page's menus) uses it to find theirs.
u32 fude_row_def_find(const fude_row_def* _def, fude_row_press _press);

// Defines a press function _name that calls one function of its owner and does nothing else:
// FUDE_ROW_CALL(fude_exam_row_undo, fude_exam, fude_exam_undo).
#define FUDE_ROW_CALL(_name, _type, _fn) \
    RDE_INTERNAL void _name(struct fude_app* _app, any _self, u32 _arg) { RDE_UNUSED(_app); RDE_UNUSED(_arg); _fn((_type*)_self); }

// How a button shows now. A zeroed face shows the button as declared.
typedef struct {
    c8        label[FUDE_ROW_LABEL];   // "" shows its own label.
    const c8* icon;                    // NULL shows its own icon.
    b8        disabled;
    b8        selected;                // Chosen (a toggle on, the sort shown): the accent's tint and a Fill icon.
    // Left out of the row for now, because nothing it does applies; the other buttons close up.
    b8        hidden;
} fude_row_face;

// --- The widget ------------------------------------------------------------------------

struct fude_row;
// What a button's press callback gets: its row and its index in it.
typedef struct {
    struct fude_row* row;
    u32              index;
} fude_row_ref;

// A row built on the UI canvas. The fields are the row's own; its owner only reads them.
typedef struct fude_row {
    const fude_row_def* def;
    struct fude_app*    app;
    any                 self;                        // The owner, handed to presses.
    void              (*after)(struct fude_app* _app);   // Called after a press, so the UI catches up.
    rde_ui_image*       panel;
    rde_ui_button*      buttons[FUDE_ROW_BUTTONS];   // NULL for a button left out.
    fude_row_ref        refs[FUDE_ROW_BUTTONS];
    fude_row_face       shown[FUDE_ROW_BUTTONS];     // The faces as last applied.
    c8                  base[FUDE_ROW_BUTTONS][FUDE_ROW_LABEL];   // Each label as built.
    b8                  open;
    rde_vec_2F          center;                      // In UI canvas units.
    rde_vec_2F          size;
    // For when the buttons do not all fit: More, and the card of the buttons it holds.
    f32                 widths[FUDE_ROW_BUTTONS];    // Each button's width as its label wants it (0 when left out).
    rde_ui_button*      more;
    fude_kit_modal      menu;
    rde_ui_button*      items[FUDE_ROW_BUTTONS];     // The buttons' items in the card (NULL for one left out).
    b8                  in_menu[FUDE_ROW_BUTTONS];   // Whether each button is under More now.
    b8                  menu_open;
    f32                 _laid_for;                   // The width it was laid out in (0 when not laid out yet).
    rde_window*         _window;                     // The window it was last shown in: where the card opens.
} fude_row;

// Builds the row under _root from _def, hidden, each button as wide as its label needs. _self and _after are handed
// to presses as described above. _row must not move after this, because its buttons point back at it.
void fude_row_create(fude_row* _row, rde_ui_node* _root, const fude_row_def* _def, struct fude_app* _app, any _self, void (*_after)(struct fude_app*));
// Shows the row centred at _center (UI units, kept on screen), or hides it. The UI is touched only on a change.
void fude_row_show(fude_row* _row, rde_window* _window, b8 _show, rde_vec_2F _center);
// The usual centre for a row: across the bottom, just above the safe area.
rde_vec_2F fude_row_bottom(const fude_row* _row, rde_window* _window);
// Shows the buttons as _faces say; _faces holds one face for each of the row's buttons. Only what changed is touched.
void fude_row_apply(fude_row* _row, const fude_row_face* _faces);
// Styles every button again from the theme: its look, with its face over it.
void fude_row_restyle(fude_row* _row);
// Whether the point _ui (UI canvas units) is on the row, or on its More card, while it shows. While the card is open
// every point counts, because its backdrop covers the screen.
b8   fude_row_hit(const fude_row* _row, rde_vec_2F _ui);
// Closes the row's More card, for Back (Android's, or Escape). Returns false when it was not open.
b8   fude_row_close_menu(fude_row* _row);
// The height of a shown row; every row is the same height.
f32  fude_row_height(void);

// Sets a face's label from a counted text ("Start {0}" with _n).
void fude_row_face_count(fude_row_face* _face, u32 _text, u32 _n);
// Sets a Next button's face: on the last step it becomes Finish (its label and icon); otherwise it is left as it is.
void fude_row_face_next(fude_row_face* _face, b8 _last);

#endif
