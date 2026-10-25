// The page's touch input (fude/drawing/app/page.c: fude_page_event), on the canvas the study apps and the drawing app
// write on (drawing/ink/canvas.c). A finger the system takes away (RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED: Android cancels
// a resting hand's touch as the pen comes down, and every finger of an edge gesture) never comes up. The page followed
// it for good: the next finger down made a pair with it and pinched the page against where it had been, and a finger
// writing (the toolbar's hand) kept its state. It is let go now, as when a pen comes down. The rest of the app (its UI,
// toolbar, documents, import) is not under test: stand-ins below.
#include "drawing/app/page.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/documents.h"
#include "drawing/app/import.h"
#include "drawing/app/pagemenu.h"
#include "drawing/ink/canvas.h"
#include "drawing/ink/ink.h"
#include "drawing/ink/lasso.h"
#include "drawing/ink/notes.h"
#include "drawing/doc/doc.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- what page.c calls besides the touches: nothing to do, nothing hit, nothing open ------------------------------
static const fude_extension NO_EXTENSION = { 0 };
const fude_extension* fude_app_ext(const fude_app* _app) { (void)_app; return &NO_EXTENSION; }
void       fude_app_set_finger_writes(fude_app* _app, b8 _on) { _app->finger_writes = _on; }
void       fude_doc_destroy(fude_doc* _doc) { (void)_doc; }
void       fude_doc_init(fude_doc* _doc) { (void)_doc; }
void       fude_doc_render(fude_doc* _doc, const fude_canvas* _canvas, rde_window* _window) { (void)_doc; (void)_canvas; (void)_window; }
void       fude_doc_render_pill(fude_doc* _doc, fude_view _view, rde_font* _font, f32 _font_px, rde_window* _window) { (void)_doc; (void)_view; (void)_font; (void)_font_px; (void)_window; }
usize      fude_doc_text_in(fude_doc* _doc, rde_vec_2F _min, rde_vec_2F _max, c8* _out, usize _size, b8* _pending) { (void)_doc; (void)_min; (void)_max; (void)_out; (void)_size; (void)_pending; return 0; }
void       fude_doc_update(fude_doc* _doc, const fude_doc_place* _at) { (void)_doc; (void)_at; }
b8         fude_documents_path(const fude_app* _app, u32 _canvas, u8 _document, c8* _out, usize _size) { (void)_app; (void)_canvas; (void)_document; (void)_out; (void)_size; return false; }
b8         fude_import_update(struct fude_app* _app) { (void)_app; return false; }
const fude_note* fude_notes_find(const fude_notes* _notes, u32 _id) { (void)_notes; (void)_id; return NULL; }
void       fude_pagemenu_close_context(fude_pagemenu* _menu) { (void)_menu; }
void       fude_pagemenu_open_context(fude_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas) { (void)_menu; (void)_screen; (void)_canvas; }
b8         fude_toolbar_close_panels(fude_toolbar* _toolbar) { (void)_toolbar; return false; }
void       fude_toolbar_pen_double_tap(fude_toolbar* _toolbar, u8 _action) { (void)_toolbar; (void)_action; }
void       fude_toolbar_refresh(fude_toolbar* _toolbar) { (void)_toolbar; }
b8         fude_ui_hit(const fude_ui* _ui, rde_vec_2F _screen) { (void)_ui; (void)_screen; return false; }   // (no UI over the page)

// --- the app around the page: its canvas, ink and lasso real, its UI blank --------------------------------------
static fude_canvas     canvas;
static fude_ink        ink;
static fude_lasso      lasso;
static fude_ui         ui;
static fude_app        app;
static fude_page_input page;

static void fresh(b8 _finger_writes) {
    rde_memset(&canvas, 0, sizeof(canvas));
    rde_memset(&page, 0, sizeof(page));
    canvas.view.zoom  = 1.0f;
    app.canvas        = &canvas;
    app.ink           = &ink;
    app.lasso         = &lasso;
    app.ui            = &ui;
    app.finger_writes = _finger_writes;
    page.app          = &app;
}

// Finger _finger's event of _type at (_x, _y), screen space.
static void finger(RDE_EVENT_TYPE_ _type, u64 _finger, i32 _x, i32 _y) {
    rde_event _e;
    rde_memset(&_e, 0, sizeof(_e));
    _e.type                                        = _type;
    _e.data.mobile_event_data.finger_id            = _finger;
    _e.data.mobile_event_data.init_touch_position  = (rde_vec_2I){ _x, _y };
    _e.data.mobile_event_data.moved_touch_position = (rde_vec_2I){ _x, _y };
    _e.data.mobile_event_data.end_touch_position   = (rde_vec_2I){ _x, _y };
    fude_page_event(&page, &_e);
}

int main(int _argc, char** _argv) {
    (void)_argc; (void)_argv;

    // A finger down on the page, taken away by the system; then another finger down 100 to its right, moved 100 more
    // to the right. Alone on the glass, it pans the page 100 across (before: it pinched against the first, where it
    // had been, and the page zoomed to twice its size).
    fresh(false);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 1u, 0, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 1u, 0, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 2u, 100, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED, 2u, 200, 0);
    CHECK(canvas.view.zoom > 0.999f && canvas.view.zoom < 1.001f);
    CHECK(canvas.view.offset.x > 99.0f && canvas.view.offset.x < 101.0f);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_UP, 2u, 200, 0);

    // Two fingers pinching, both taken away: the next finger on its own pans too (Android cancels every finger of a
    // gesture, one event each).
    fresh(false);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 1u, -50, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 2u, 50, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED, 2u, 60, 0);
    const f32 _pinched = canvas.view.zoom;
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 1u, -50, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 2u, 60, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 3u, 0, 0);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED, 3u, 0, 40);
    CHECK(_pinched > 1.0f);
    CHECK(canvas.view.zoom > _pinched - 0.001f && canvas.view.zoom < _pinched + 0.001f);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_UP, 3u, 0, 40);

    // With the toolbar's hand on, a finger pending to write, taken away: the page has no finger any more (before: it
    // stayed pending for good, and a finger after it was taken as a second one, a gesture).
    fresh(true);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 1u, 0, 0);
    CHECK(page.finger_state != 0u);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED, 1u, 0, 0);
    CHECK(page.finger_state == 0u);
    CHECK(page.finger_count == 0u);

    // A pen's echo of its own touch cancelled (from_pen) is not a finger: nothing is let go.
    fresh(false);
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN, 1u, 0, 0);
    {
        rde_event _e;
        rde_memset(&_e, 0, sizeof(_e));
        _e.type                              = RDE_EVENT_TYPE_MOBILE_TOUCH_CANCELED;
        _e.data.mobile_event_data.finger_id  = 7u;
        _e.data.mobile_event_data.from_pen   = true;
        fude_page_event(&page, &_e);
    }
    finger(RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED, 1u, 30, 0);
    CHECK(canvas.view.offset.x > 29.0f && canvas.view.offset.x < 31.0f);

    if(fails == 0) {
        printf("ALL PASSED\n");
        return 0;
    }
    printf("%d FAILED\n", fails);
    return 1;
}
