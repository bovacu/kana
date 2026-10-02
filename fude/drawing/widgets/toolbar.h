#ifndef FUDE_TOOLBAR
#define FUDE_TOOLBAR

#include "rde.h"
#include "drawing/ink/canvas.h"
#include "drawing/app/extension.h"

// ===========================================================================
// The floating toolbar: a movable bar of the page's tools that can sit anywhere
// on screen, vertical or horizontal — Undo, Redo, the hand, Draw, Mark, Erase, Lasso,
// Clear, the brush's size, Color, Page/Screen, Paper, the app's own tools
// (extension.h; Kana's camera), Rotate, Reset —
// with its two panels beside it: the colour palette and the paper. Shown over
// the page only (a screen on top hides it).
//
// Children are placed by hand (fude_toolbar_layout) rather than by an hbox/vbox:
// rotating the bar is then just laying the same children out along the other
// axis, and none of the container sizing rules come into it. The grip's end
// drags it; a double tap there folds it down to the grip.
//
// INPUT: the RDE UI only ever sees MOUSE events — finger taps and pen touches
// reach it as SDL's synthetic mouse, AFTER the app has already had the pen/touch
// event. So the page asks fude_ui_hit before starting ink, an erase or a pan,
// or writing on a button would also draw under it.
// ===========================================================================

struct fude_app;

typedef enum {
    FUDE_TOOL_DRAW = 0,
    FUDE_TOOL_ERASE,
    FUDE_TOOL_LASSO,
    FUDE_TOOL_MARK     // the marker: see-through, thick, over what it marks (ink.h); its own colours and width
} FUDE_TOOL_;

#define FUDE_TOOLBAR_PALETTE_COUNT 8
#define FUDE_TOOLBAR_SEPARATORS    5

// Slider range for the brush half-width (see fude_ink.constant_radius).
#define FUDE_TOOLBAR_SIZE_MIN  0.5f
#define FUDE_TOOLBAR_SIZE_MAX 12.0f

struct fude_toolbar;
// One swatch's (or paper choice's) callback context: which bar, which one.
typedef struct {
    struct fude_toolbar* toolbar;
    u32                  index;
} fude_toolbar_ref;

typedef struct fude_toolbar {
    struct fude_app* app;

    FUDE_TOOL_     tool;
    FUDE_TOOL_     tool_before;     // the one before it (a pen's double tap goes back to it)
    b8             vertical;
    b8             minimized;       // folded down to the grip
    rde_vec_2F     center;          // panel centre, UI canvas units (bottom-left origin, Y up)
    rde_vec_2F     panel_size;
    b8             hidden;          // a screen is over the page (fude_toolbar_update)

    rde_ui_image*  panel;
    // The grip's end of the bar: what drags it, and a double tap there folds the
    // bar down to just this and opens it again. The handle drawn in it.
    rde_ui_image*  grip_area;
    rde_ui_label*  grip;            // six dots (icons.h), across the bar
    // The tools, in a strip that scrolls along the bar when they do not all fit.
    rde_ui_scroll_area* strip;
    rde_ui_button* undo;
    rde_ui_button* redo;
    rde_ui_button* finger;          // the hand: one finger writes, or (off) only the pen — a tablet's
    rde_ui_button* draw;
    rde_ui_button* mark;            // the marker
    rde_ui_button* erase;
    rde_ui_button* lasso_tool;
    rde_ui_button* clear;
    rde_ui_slider* size;
    rde_ui_button* color;
    rde_ui_image*  color_dot;       // in Color: the colour writing now
    rde_ui_button* brush_scale;
    rde_ui_button* paper;           // opens the paper panel: the page's dots, lines, squares or nothing (canvas.h)
    rde_ui_button* rotate;
    rde_ui_button* reset_view;
    rde_ui_button* tools[FUDE_EXTENSION_TOOLS];   // the app's own (NULL: none), where they are available
    fude_toolbar_ref tool_refs[FUDE_EXTENSION_TOOLS];
    rde_ui_image*  separators[FUDE_TOOLBAR_SEPARATORS];   // between the groups of tools

    rde_ui_image*     palette;
    rde_ui_button*    swatches[FUDE_TOOLBAR_PALETTE_COUNT];
    fude_toolbar_ref  swatch_refs[FUDE_TOOLBAR_PALETTE_COUNT];
    b8                palette_open;
    rde_vec_2F        palette_center;   // UI canvas units, set whenever it is placed
    rde_vec_2F        palette_size;

    // The page's paper, a panel beside the bar like the palette (one of the two
    // open at a time). In FUDE_PAPER_ order.
    rde_ui_image*     paper_panel;
    rde_ui_button*    paper_choices[FUDE_PAPER_COUNT];
    fude_toolbar_ref  paper_refs[FUDE_PAPER_COUNT];
    b8                paper_open;
    rde_vec_2F        paper_center;
    rde_vec_2F        paper_size;

    // Grip drag, and the grip's last tap (for a double tap).
    rde_vec_2F     drag_start_center;
    rde_vec_2F     drag_press;
    f64            grip_tapped;        // engine clock (0: no tap waiting for its second)
    rde_vec_2F     grip_tapped_at;

    // What the bar shows, so fude_toolbar_update only touches it on a change.
    b8             _history_shown;
    b8             _can_undo_shown;
    b8             _can_redo_shown;
    FUDE_PAPER_    _paper_shown;
} fude_toolbar;

// Built under _root (its panels hidden). The tool, the placement and the hand
// are the caller's to keep across a rebuild (another language).
void fude_toolbar_create(fude_toolbar* _toolbar, rde_ui_node* _root, struct fude_app* _app);
// Once a frame: hidden under a screen (_hidden), Undo/Redo greyed out when there
// is nothing to undo/redo, Paper showing the page's paper.
void fude_toolbar_update(fude_toolbar* _toolbar, b8 _hidden);
// Every widget's colours from the theme, and what the tools show.
void fude_toolbar_restyle(fude_toolbar* _toolbar);
// The tools as the state says: the tool, the colour, the hand, the brush's width.
void fude_toolbar_refresh(fude_toolbar* _toolbar);
// Is _ui (UI canvas units) on the bar or an open panel?
b8   fude_toolbar_hit(const fude_toolbar* _toolbar, rde_vec_2F _ui);
// Laid out again along its axis, clamped on screen (the safe area changed, the screen turned).
void fude_toolbar_layout(fude_toolbar* _toolbar);

void fude_toolbar_set_tool(fude_toolbar* _toolbar, FUDE_TOOL_ _tool);
// Puts the bar back where a save left it (orientation, centre in UI canvas units,
// folded to its grip or not). Clamped on screen, so a centre from a bigger or
// rotated screen is fine.
void fude_toolbar_set_placement(fude_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized);
// The colour palette, or the paper panel, beside the bar: open or closed (one at a time).
void fude_toolbar_set_palette_open(fude_toolbar* _toolbar, b8 _open);
void fude_toolbar_set_paper_open(fude_toolbar* _toolbar, b8 _open);
// Apple Pencil's double tap (RDE_EVENT_TYPE_PEN_DOUBLE_TAP), as the learner set
// it in the system's settings (_action, RDE_PEN_TAP_ACTION_): the eraser and back,
// the tool before, or the colour palette; nothing when they turned it off.
void fude_toolbar_pen_double_tap(fude_toolbar* _toolbar, u8 _action);

#endif
