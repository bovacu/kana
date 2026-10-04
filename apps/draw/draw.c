// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Draw: an endless page to write and draw on, with a pen or a finger — the
// drawing core alone (fude/drawing), the start of the diagram and drawing app.
// The app's shell: what RDE calls (init, the events, each frame's update and
// render, the end), and what it owns.
//
// The page is at the bottom (page.h: writing on it, moving it); no screens stack
// over it yet. The retained UI — the toolbar, the page's menus, the side panel
// and Settings — is one canvas (ui.h). What is saved, and what leaves as a file,
// is the session's (session.h); a developer's launch flags are look.h's.
// ===========================================================================

#include "rde.h"

#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/row.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "version.h"

#define DRAW_CONFIG_PATH "./assets/config.rdef"

// A developer's build (debug, RDE_DEBUG) reads launch arguments (look.h); a
// release takes none at all (main).
#if defined(RDE_DEBUG)
#define DRAW_TAKES_ARGS 1
#else
#define DRAW_TAKES_ARGS 0
#endif

// What Draw is (info.h; its name and version: version.h). Its script font only
// has 日本語, for the language picker (Noto Sans JP, cut down to it).
RDE_INTERNAL const fude_app_info DRAW_INFO = {
    .name = DRAW_NAME, .version = DRAW_VERSION, .store_id = DRAW_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Names.otf", .credits = FUDE_TEXT_COUNT,
    .licences = { { FUDE_TEXT_LICENCE_FONTS, { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } } },
    .licence_count = 1u,
};

RDE_INTERNAL rde_window*     window;
RDE_INTERNAL rde_camera      camera;
RDE_INTERNAL void draw_render_top(rde_window* _window, f32 _dt);   // the notice, after the UI (below)
RDE_INTERNAL fude_ink        ink;
RDE_INTERNAL fude_canvas     canvas;
RDE_INTERNAL fude_lasso      lasso;
RDE_INTERNAL fude_notes      notes;
RDE_INTERNAL fude_page_input page;
RDE_INTERNAL fude_ui         ui;
RDE_INTERNAL fude_app        app;

// --- init -------------------------------------------------------------------------------------

void init_func(i32 _argc, c8** _argv, rde_window* _window) {
    window = _window;
    fude_app_window(_window);    // its units (dp on Android): before anything reads its size
    // The words first: the UI is built in them. The device's language when Draw
    // speaks it; a saved choice replaces it (session.h).
    fude_text_set_language(fude_text_default_language());
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    rde_engine_set_top_overlay_render(draw_render_top);
    fude_look_args(_argc, _argv);

    // The page.
    fude_ink_init(&ink);
    fude_canvas_init(&canvas);
    fude_lasso_init(&lasso);
    fude_notes_init(&notes);

    // The app: the page, the UI over it. No screens, nothing added (extension.h).
    app = (fude_app){
        .info = &DRAW_INFO, .ext = NULL, .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f,
        .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
    };
    fude_app_start(&app);       // its save folder, its window's title: before anything reads a save
#if defined(RDE_PLATFORM_MOBILE)
    app.finger_writes = true;   // a tablet starts with the hand on, until its settings (or a pen) say otherwise
#endif
    fude_page_init(&page, &app);
    fude_ui_init(&ui, &app);
    if(ui.font != NULL) {
        app.font    = ui.font;   // the UI font, in screen units, scaled from the size it was loaded at
        app.font_px = (f32)FUDE_KIT_FONT_SIZE;
    }

    fude_look_start(&app);
    fude_session_load(&app);
    fude_look_loaded(&app);
}

// --- events --------------------------------------------------------------------------------------

void on_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);
    if(_event == NULL) {
        return;
    }
    // Out of sight, or the OS short of memory: the fonts' glyph memory goes back.
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        fude_ui_trim_fonts(&ui);
    }
    // Leaving: save now (the autosave is the guarantee).
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        fude_session_save_on_exit(&app);
    }
    fude_page_event(&page, _event);
}

// --- each frame -------------------------------------------------------------------------------

RDE_INTERNAL void draw_update(f32 _dt) {
    fude_look_frame(&app);
    fude_ui_follow_language(&ui);   // a language chosen in Settings: the UI in it
#if defined(RDE_PLATFORM_IOS)
    // Apple Pencil's double tap: asked for once the app's view is there.
    static b8  _pencil_taps  = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
    }
#endif
    fude_ink_frame_begin(&ink);
    if(!fude_app_update(&app, _dt)) {
        fude_page_update(&page);
    }
    fude_ui_update(&ui);
    fude_session_update(&app);
}

RDE_INTERNAL void draw_render(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    // Every frame: the engine resets the clear colour at the start of each one.
    rde_rendering_clear_background_color(fude_theme_active()->page);
    rde_rendering_2d_begin_drawing(_window, &camera);
    if(!fude_app_render(&app)) {
        fude_page_render(&page, _window);
    }
    fude_app_render_overlays(&app);
    rde_rendering_2d_end_drawing();
}

// After every UI canvas too (the side panel is the engine's UI): the notice, on top of all.
RDE_INTERNAL void draw_render_top(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(app.font == NULL) {
        return;
    }
    rde_rendering_2d_begin_drawing(_window, &camera);
    fude_notice_render(_window, app.font, app.font_px, fude_app_top(&app) == NULL ? 72.0f : 8.0f + fude_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

void on_update(f32 _dt) {
    fude_look_timed_update(draw_update, _dt);
}

void on_render(rde_window* _window, f32 _dt) {
    fude_look_timed_render(&app, draw_render, _window, _dt);
}

void on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

void on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
}

void on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

void end_func(void) {
    fude_session_save_on_exit(&app);
    fude_ui_destroy(&ui);
    fude_lasso_destroy(&lasso);
    fude_notes_destroy(&notes);
    fude_ink_destroy(&ink);
}

int main(i32 _argc, c8* _argv[]) {
#if !DRAW_TAKES_ARGS
    // A release: whatever it is launched with is ignored — by Draw and by RDE
    // (which reads arguments as config overrides). Only the program's name.
    _argc = _argc > 1 ? 1 : _argc;
#endif
    return rde_run(_argc, _argv, DRAW_CONFIG_PATH, init_func, on_event, on_fixed_update, on_update, on_late_update, on_render, on_crash, end_func);
}
