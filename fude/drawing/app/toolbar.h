// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TOOLBAR
#define FUDE_TOOLBAR

#include "rde.h"
#include "drawing/ink/canvas.h"
#include "drawing/app/extension.h"

// ===========================================================================
// Toolbar: the floating toolbar, a movable bar of the page's tools that can sit anywhere on screen, vertical or
// horizontal: Undo, Redo, the hand, Draw, Mark, Erase, Lasso, Clear, the brush's size, Color, Page/Screen, Paper,
// the app's own tools (extension.h; a camera, say), Rotate and Reset. Beside it open its panels: the colour palette,
// the paper, and an app tool's choices. It shows over the page only; a screen on top hides it.
//
// Where it sits: drawing/app. The UI (ui.h) holds one fude_toolbar and drives it; the page, the session (which saves
// its placement) and the app's extension read and set its tool.
//
// Its main types and their lifecycle: fude_toolbar_create builds the widgets under the UI's root; they belong to the
// UI tree and are built again on a rebuild (another language), when the UI keeps the tool, the placement and the hand.
//
// Threads: main thread only, like all of RDE's UI.
//
// Design: the children are placed by hand (fude_toolbar_layout) rather than by an hbox or vbox, so rotating the bar
// is just laying the same children out along the other axis, and none of the container sizing rules come into it.
// The grip's end drags the bar, and a double tap there folds it down to the grip.
//
// INPUT: the RDE UI only ever sees MOUSE events. Finger taps and pen touches reach it as SDL's synthetic mouse, AFTER
// the app has already had the pen or touch event. So the page asks fude_ui_hit before starting ink, an erase or a pan;
// otherwise writing on a button would also draw under it.
// ===========================================================================

struct fude_app;

// The tool in the pen's hand.
typedef enum {
    FUDE_TOOL_DRAW = 0,
    FUDE_TOOL_ERASE,
    FUDE_TOOL_LASSO,
    // The marker: see-through and thick, drawn over what it marks (ink.h), with its own colours and width.
    FUDE_TOOL_MARK
} FUDE_TOOL_;

#define FUDE_TOOLBAR_PALETTE_COUNT 8   // The colours in the palette.
#define FUDE_TOOLBAR_SEPARATORS    5   // The lines between groups of tools.

// The slider's range for the brush's half-width (see fude_ink.constant_radius).
#define FUDE_TOOLBAR_SIZE_MIN  0.5f
#define FUDE_TOOLBAR_SIZE_MAX 12.0f

struct fude_toolbar;
// What a swatch's (or a paper choice's, or an app tool's) callback gets: its bar, and which one it is.
typedef struct {
    struct fude_toolbar* toolbar;
    u32                  index;
} fude_toolbar_ref;

// The toolbar. The UI and the session read and set tool, vertical, minimized and center; the rest is toolbar.c's.
typedef struct fude_toolbar {
    struct fude_app* app;

    FUDE_TOOL_     tool;
    FUDE_TOOL_     tool_before;     // The tool before it (a pen's double tap goes back to it).
    // How many times Erase was pressed while already chosen (a canvas app moves to the next eraser mode on each).
    u32            erase_taps;
    // How many times one of the bar's tools was chosen; an app's own tool lets go when it changes (extension.h).
    u32            tool_taps;
    b8             vertical;
    b8             minimized;       // Folded down to the grip.
    rde_vec_2F     center;          // The panel's centre, in UI canvas units (bottom-left origin, Y up).
    rde_vec_2F     panel_size;
    b8             hidden;          // A screen is over the page (fude_toolbar_update).

    rde_ui_image*  panel;
    // The grip's end of the bar: dragging it moves the bar, and a double tap there folds the bar down to just this and
    // opens it again. The grip is the handle drawn in it: six dots (icons.h), across the bar.
    rde_ui_image*  grip_area;
    rde_ui_label*  grip;
    // The tools, in a strip that scrolls along the bar when they do not all fit.
    rde_ui_scroll_area* strip;
    rde_ui_button* undo;
    rde_ui_button* redo;
    rde_ui_button* finger;          // The hand, on a tablet: on, one finger writes; off, only the pen does.
    rde_ui_button* draw;
    rde_ui_button* mark;            // The marker.
    rde_ui_button* erase;
    rde_ui_button* lasso_tool;
    rde_ui_button* clear;
    rde_ui_slider* size;
    rde_ui_button* color;
    rde_ui_image*  color_dot;       // Inside Color: the colour that writes now.
    rde_ui_button* brush_scale;
    // Opens the paper panel: the page's dots, lines, squares or nothing (canvas.h).
    rde_ui_button* paper;
    rde_ui_button* rotate;
    rde_ui_button* reset_view;
    rde_ui_button* tools[FUDE_EXTENSION_TOOLS];   // The app's own tools, where available (NULL: none).
    fude_toolbar_ref tool_refs[FUDE_EXTENSION_TOOLS];
    // An app tool's choices (extension.h), in a panel beside the bar like Paper's.
    rde_ui_image*     tool_panels[FUDE_EXTENSION_TOOLS];                           // NULL when that tool has none.
    rde_ui_button*    tool_choices[FUDE_EXTENSION_TOOLS][FUDE_EXTENSION_CHOICES];
    // Each choice's ref; its index is tool * FUDE_EXTENSION_CHOICES + choice.
    fude_toolbar_ref  choice_refs[FUDE_EXTENSION_TOOLS][FUDE_EXTENSION_CHOICES];
    i32               tool_open;          // The tool whose panel is open (-1 when none is).
    rde_vec_2F        tool_panel_center;
    rde_vec_2F        tool_panel_size;
    rde_ui_image*  separators[FUDE_TOOLBAR_SEPARATORS];   // Between the groups of tools.

    rde_ui_image*     palette;
    rde_ui_button*    swatches[FUDE_TOOLBAR_PALETTE_COUNT];
    fude_toolbar_ref  swatch_refs[FUDE_TOOLBAR_PALETTE_COUNT];
    b8                palette_open;
    rde_vec_2F        palette_center;   // In UI canvas units, set whenever it is placed.
    rde_vec_2F        palette_size;

    // The page's paper, in a panel beside the bar like the palette (only one panel is open at a time). The choices are
    // in FUDE_PAPER_ order.
    rde_ui_image*     paper_panel;
    rde_ui_button*    paper_choices[FUDE_PAPER_COUNT];
    fude_toolbar_ref  paper_refs[FUDE_PAPER_COUNT];
    b8                paper_open;
    rde_vec_2F        paper_center;
    rde_vec_2F        paper_size;

    // A drag of the grip, and the grip's last tap (to tell a double tap).
    rde_vec_2F     drag_start_center;
    rde_vec_2F     drag_press;
    f64            grip_tapped;        // When, on the engine clock (0 when no tap is waiting for its second).
    rde_vec_2F     grip_tapped_at;

    // What the bar shows now, so fude_toolbar_update only touches it on a change.
    b8             _history_shown;
    b8             _can_undo_shown;
    b8             _can_redo_shown;
    FUDE_PAPER_    _paper_shown;
} fude_toolbar;

// Builds the bar under _root, its panels hidden. The tool, the placement and the hand are the caller's to keep across
// a rebuild (another language).
void fude_toolbar_create(fude_toolbar* _toolbar, rde_ui_node* _root, struct fude_app* _app);
// Called once a frame: hides the bar (and closes its panels) under a screen when _hidden, greys out Undo and Redo when
// there is nothing to undo or redo, and makes Paper show the page's paper.
void fude_toolbar_update(fude_toolbar* _toolbar, b8 _hidden);
// Takes every widget's colours from the theme, and shows the tools' state.
void fude_toolbar_restyle(fude_toolbar* _toolbar);
// Shows the tools as the state says: the tool, the colour, the hand, the paper, the brush's width.
void fude_toolbar_refresh(fude_toolbar* _toolbar);
// Whether the point _ui (UI canvas units) is on the bar while it shows, or on an open panel.
b8   fude_toolbar_hit(const fude_toolbar* _toolbar, rde_vec_2F _ui);
// For a press on the page: closes the bar's open panels (the palette, the paper, a tool's choices); the press still
// does its own job. Returns true when one was open.
b8   fude_toolbar_close_panels(fude_toolbar* _toolbar);
// Lays the bar out again along its axis, clamped on screen (when the safe area changed or the screen turned).
void fude_toolbar_layout(fude_toolbar* _toolbar);
// Moves the bar when the screen went from size _from to _to (UI units): it keeps its distance to an edge it was near,
// or else its place in proportion. The caller lays it out afterwards.
void fude_toolbar_follow_screen(fude_toolbar* _toolbar, rde_vec_2F _from, rde_vec_2F _to);

// Puts _tool in the pen's hand. Leaving the Lasso drops its selection, which would otherwise sit there with no way to
// act on it.
void fude_toolbar_set_tool(fude_toolbar* _toolbar, FUDE_TOOL_ _tool);
// Puts the bar back where a save left it (its orientation, its centre in UI canvas units, folded to its grip or not).
// It is clamped on screen, so a centre from a bigger or rotated screen is fine.
void fude_toolbar_set_placement(fude_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized);
// Opens or closes the colour palette, or the paper panel, beside the bar. Opening one closes the other panels.
void fude_toolbar_set_palette_open(fude_toolbar* _toolbar, b8 _open);
void fude_toolbar_set_paper_open(fude_toolbar* _toolbar, b8 _open);
// Opens an app tool's choices (_tool: its place in extension.h's list), or with -1 closes whichever is open.
void fude_toolbar_set_tool_open(fude_toolbar* _toolbar, i32 _tool);
// Handles Apple Pencil's double tap (RDE_EVENT_TYPE_PEN_DOUBLE_TAP) as the learner set it in the system's settings
// (_action, an RDE_PEN_TAP_ACTION_): to the eraser and back, to the tool before, or the colour palette; nothing when
// they turned it off.
void fude_toolbar_pen_double_tap(fude_toolbar* _toolbar, u8 _action);

#endif
