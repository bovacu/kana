// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/shell.h"

#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/row.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/redraw.h"
#include "drawing/ink/mlkit.h"
#include "zoom/themes.h"
#include "zoom/page.h"
#include "zoom/paper.h"
#include "zoom/product.h"
#include "zoom/look.h"

// ===========================================================================
// See shell.h.
//
// Everything the app owns is a static here, made once in fude_zoom_shell_init and destroyed in fude_zoom_shell_end,
// so there is one app a process. RDE calls the fude_zoom_shell_* functions; each frame's update and render go through
// drawing/app/look.h's timers (fude_look_timed_update, fude_look_timed_render), so that --perf can measure them, and
// the canvas's own looks (zoom/look.h) have their frame inside the update, after the page's.
// ===========================================================================

#define FUDE_ZOOM_SHELL_CONFIG "./assets/config.rdef"   // RDE's config for the app, beside its assets

// A developer's build (debug, RDE_DEBUG) reads launch arguments (drawing/app/look.h, zoom/look.h); a release takes
// none at all (fude_zoom_shell_run). Building with -DFUDE_ALLOW_ARGS keeps them in a release build too, to measure it
// as it really runs.
#if defined(RDE_DEBUG) || defined(FUDE_ALLOW_ARGS)
#define FUDE_ZOOM_SHELL_TAKES_ARGS 1
#else
#define FUDE_ZOOM_SHELL_TAKES_ARGS 0
#endif

// What the app owns, for its whole run: its description, its window and camera, the core's ink, canvas, lasso, notes
// and page input, the UI, the app that ties them together (app.h), the extension it adds, the deep-zoom page, and the
// home screen.
RDE_INTERNAL const fude_zoom_shell_def* shell;   // the app's description (fude_zoom_shell_run)
RDE_INTERNAL rde_window*     window;
RDE_INTERNAL rde_camera      camera;
RDE_INTERNAL fude_ink        ink;
RDE_INTERNAL fude_canvas     canvas;
RDE_INTERNAL fude_lasso      lasso;
RDE_INTERNAL fude_notes      notes;
RDE_INTERNAL fude_page_input page;
RDE_INTERNAL fude_ui         ui;
RDE_INTERNAL fude_app        app;
RDE_INTERNAL fude_extension  extension;         // FUDE_ZOOM_SHELL_EXTENSION, with the app's Tutorial when it has one
RDE_INTERNAL fude_zoom_page  zoom;
RDE_INTERNAL fude_zoom_home  home;              // the home screen: the canvases as cards (notebooks, projects)
RDE_INTERNAL u32             home_cover_seen;   // the cover stamp last seen; a new one reloads that cover
RDE_INTERNAL f64             update_took = 0.0; // the last update's time in seconds, shown with the frame rate

// Draws the notice on top of everything, after the UI (defined below).
RDE_INTERNAL void fude_zoom_shell_render_top(rde_window* _window, f32 _dt);

// --- the home screen and the extension's hooks --------------------------------------------------

// The home screen's New: makes a canvas in the folder it shows and opens it; a notebook of pages, in an app whose New
// makes one (a notes app's: A4).
RDE_INTERNAL void fude_zoom_shell_home_new(fude_app* _app, u32 _folder) {
    c8 _name[FUDE_NOTE_NAME];
    fude_notes_new_name(_app->notes, FUDE_NOTE_CANVAS, _name, sizeof(_name));
    const u32 _id = fude_notes_add(_app->notes, FUDE_NOTE_CANVAS, _folder, _name);
    if(_id == 0u) {
        return;
    }
    if(shell->new_notebook) {
        fude_zoom_page_notebook_next(&zoom, shell->new_pages);
    }
    fude_notes_open(_app->notes, _id);
}

// The welcome's first step: makes the examples and opens the first.
RDE_INTERNAL void fude_zoom_shell_home_first(fude_app* _app) {
    RDE_UNUSED(_app);
    fude_zoom_page_look_examples(&zoom, 0u);
}

// The page's home button: opens the home screen.
RDE_INTERNAL void fude_zoom_shell_open_home(fude_app* _app) {
    RDE_UNUSED(_app);
    fude_zoom_home_open(&home);
}

// The side panel's Tutorial (an app with a welcome): shows the welcome again.
RDE_INTERNAL void fude_zoom_shell_tutorial(fude_app* _app) {
    RDE_UNUSED(_app);
    fude_zoom_home_show_welcome(&home);
}

// On the first launch, brushes keep their size on the screen at any zoom (the research's first idea), so a stroke
// drawn zoomed in is finer on the canvas.
RDE_INTERNAL void fude_zoom_shell_first_launch(fude_app* _app) {
    _app->ink->brush_scale = FUDE_INK_BRUSH_SCALE_SCREEN;
}

// The canvas's own papers (paper.h), shown after the core's four in the Paper panel.
RDE_INTERNAL const fude_extension_paper FUDE_ZOOM_SHELL_PAPERS[] = {
    { FUDE_PAPER_ENGINEERING, FUDE_TEXT_ZOOM_PAPER_ENGINEERING, FUDE_ICON_PAGE_NUMBER },
    { FUDE_PAPER_ISOMETRIC,   FUDE_TEXT_ZOOM_PAPER_ISOMETRIC,   FUDE_ICON_TRIANGLE },
    { FUDE_PAPER_HEX,         FUDE_TEXT_ZOOM_PAPER_HEX,         FUDE_ICON_GRAPH },
    { FUDE_PAPER_POLAR,       FUDE_TEXT_ZOOM_PAPER_POLAR,       FUDE_ICON_TARGET },
    { FUDE_PAPER_MUSIC,       FUDE_TEXT_ZOOM_PAPER_MUSIC,       FUDE_ICON_SPEAK },
    { FUDE_PAPER_CORNELL,     FUDE_TEXT_ZOOM_PAPER_CORNELL,     FUDE_ICON_COLUMNS },
    { FUDE_PAPER_PLANNER,     FUDE_TEXT_ZOOM_PAPER_PLANNER,     FUDE_ICON_LIST_CHECKS },
};

// A page's layouts (Cornell, To-do) are offered only on a notebook's pages; on the endless canvas they would be just
// ruled lines.
RDE_INTERNAL b8 fude_zoom_shell_paper_offered(const fude_app* _app, u8 _paper) {
    RDE_UNUSED(_app);
    return !fude_zoom_paper_is_page(_paper) || fude_zoom_page_is_notebook(&zoom);
}

// What every canvas app adds to the core (extension.h): the deep-zoom page as its page kind, its tools and menus, its
// Settings sections and UI hooks, and its papers. The app's Tutorial is added at init when it has a welcome.
RDE_INTERNAL const fude_extension FUDE_ZOOM_SHELL_EXTENSION = {
    .page_kind       = &FUDE_ZOOM_PAGE_KIND,
    .tools           = FUDE_ZOOM_TOOLS,
    .tool_count      = FUDE_EXTENSION_TOOLS,   // Topic, then the topic's own tools (page_topics.c's topics).
    .brush_on_screen = false,   // Page, not Screen: a width on the canvas itself, as instruments want.
    .selection_row   = &FUDE_ZOOM_SELECTION_ROW,
    .selection_faces = fude_zoom_page_selection_faces,
    .context_row     = &FUDE_ZOOM_CONTEXT_ROW,
    .context_faces   = fude_zoom_page_context_faces,
    .sections        = FUDE_ZOOM_PAGE_SECTIONS,
    .section_count   = FUDE_ZOOM_PAGE_SECTION_COUNT,
    .settings_gather = fude_zoom_page_settings_gather,
    .settings_apply  = fude_zoom_page_settings_apply,
    .ui_build        = fude_zoom_page_ui_build,
    .ui_forget       = fude_zoom_page_ui_forget,
    .ui_update       = fude_zoom_page_ui_update,
    .ui_restyle      = fude_zoom_page_ui_restyle,
    .ui_hit          = fude_zoom_page_ui_hit,
    .first_launch    = fude_zoom_shell_first_launch,
    .papers          = FUDE_ZOOM_SHELL_PAPERS,
    .paper_count     = (u32)(sizeof(FUDE_ZOOM_SHELL_PAPERS) / sizeof(FUDE_ZOOM_SHELL_PAPERS[0])),
    .paper_offered   = fude_zoom_shell_paper_offered,
};

// --- init -------------------------------------------------------------------------------------

// Makes the home screen, the app's only screen, with the app's own words, New and welcome, and puts it in the app's
// table of screens.
RDE_INTERNAL void fude_zoom_shell_place_home(void) {
    fude_zoom_home_init(&home, &app);
    home.title    = shell->home_title;
    home.new_text = shell->home_new;
    if(shell->welcome != NULL) {
        home.welcome_title = shell->welcome->title;
        for(u32 _k = 0; _k < FUDE_ZOOM_HOME_LINES; _k++) {
            home.welcome_lines[_k] = shell->welcome->lines[_k];
        }
        home.welcome_first = shell->welcome->first;
        home.on_first      = fude_zoom_shell_home_first;
    }
    home.on_new  = fude_zoom_shell_home_new;
    zoom.on_home = fude_zoom_shell_open_home;
    app.screens[0]   = (fude_screen_slot){ &FUDE_ZOOM_HOME_SCREEN, &home };
    app.screen_count = 1u;
}

// RDE's start: sets up the core's parts, the app, the deep-zoom page and the home screen, then loads the session. The
// order matters: each step needs the ones before it.
RDE_INTERNAL void fude_zoom_shell_init(i32 _argc, c8** _argv, rde_window* _window) {
    window = _window;
    fude_redraw_start();         // Render on demand (redraw.h): the launch flags below may draw every frame instead.
    fude_app_window(_window);    // Its units (dp on Android), set before anything reads its size.
    fude_text_drop_taught_language();   // A drawing app teaches no language, so it has no fourth one (Japanese).
    fude_text_set_language(fude_text_default_language());
    // What the app offers (docs/product_split.md), set before the page is made.
    fude_zoom_product_set(shell->product);
    if(fude_zoom_product_get()->workshop) {
        fude_zoom_themes_use();  // The workshop's themes instead of the language apps' (Paper and Night are kept).
    }
    camera = rde_camera_create(_window, RDE_CAMERA_TYPE_ORTHOGRAPHIC);
    rde_engine_set_top_overlay_render(fude_zoom_shell_render_top);
    fude_look_args(_argc, _argv);

    fude_ink_init(&ink);
    ink.brush_scale = FUDE_INK_BRUSH_SCALE_SCREEN;   // Until the settings say otherwise.
    fude_canvas_init(&canvas);
    fude_lasso_init(&lasso);
    fude_notes_init(&notes);

    extension          = FUDE_ZOOM_SHELL_EXTENSION;
    extension.tutorial = shell->welcome != NULL ? fude_zoom_shell_tutorial : NULL;
    app = (fude_app){
        .info = shell->info, .ext = &extension, .window = _window, .font = rde_font_get_default_missing(), .font_px = 14.0f,
        .ink = &ink, .canvas = &canvas, .lasso = &lasso, .notes = &notes, .page = &page,
    };
    fude_app_start(&app);       // Its save folder and its window's title, set before anything reads a save.
#if defined(RDE_PLATFORM_MOBILE)
    app.finger_writes = true;   // A tablet starts with the finger drawing, until its settings (or a pen) say otherwise.
#endif
    fude_page_init(&page, &app);
    fude_zoom_page_init(&zoom, &app);
    fude_zoom_shell_place_home();
    fude_ui_init(&ui, &app);
    if(ui.font != NULL) {
        app.font    = ui.font;
        app.font_px = (f32)FUDE_KIT_FONT_SIZE;
    }

    fude_look_start(&app);
    fude_session_load(&app);    // This also opens the canvas on the deep-zoom page (through the page kind).
    fude_look_loaded(&app);
    // The handwriting model is the language the app is shown in, set before anything prepares it (Settings' Download,
    // To text, Find). It is not fetched here: a canvas app downloads it the first time handwriting is read.
    fude_mlkit_set_model(fude_zoom_ink_model());
    // An app that opens on its home screen does so with the last canvas under it, and shows its welcome over it once.
    // Launched with flags (a developer's look), it opens on the canvas.
    if(shell->opens_home && _argc <= 1) {
        fude_zoom_home_open(&home);
        if(!(app.cards_read & FUDE_ZOOM_HOME_WELCOMED)) {
            fude_zoom_home_show_welcome(&home);
        }
    }
    fude_zoom_look_start(&app, &zoom, &home, _argc, _argv);
}

// --- events --------------------------------------------------------------------------------------

// RDE's events. In the background or short of memory, the fonts' glyphs and the page's caches are given back; when
// the app leaves, a recording is paused and the session saved. The rest goes to the page, or to a screen on top.
RDE_INTERNAL void fude_zoom_shell_event(rde_window* _window, rde_event* _event) {
    RDE_UNUSED(_window);
    if(_event == NULL) {
        return;
    }
    fude_redraw_changed();   // Render on demand: whatever an event does is drawn (redraw.h).
    if(_event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_LOW_MEMORY) {
        fude_ui_trim_fonts(&ui);
        fude_zoom_page_trim(&zoom);
    }
    if(_event->type == RDE_EVENT_TYPE_MOBILE_WILL_ENTER_BACKGROUND || _event->type == RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND ||
       _event->type == RDE_EVENT_TYPE_MOBILE_TERMINATING) {
        fude_zoom_page_audio_pause(&zoom);   // A recording is kept as it is, since the app may not come back.
        fude_session_save_on_exit(&app);
    }
    if(fude_app_top(&app) == NULL) {
        fude_zoom_page_event(&zoom, _event);
    } else {
        fude_app_screen_event(&app, _event);   // A screen is on top, so the home screen has the pointer.
    }
}

// --- each frame -------------------------------------------------------------------------------

// One frame's update: the page (unless a screen above it takes the frame), the developer's looks due this frame, the
// UI, then the session's autosave.
RDE_INTERNAL void fude_zoom_shell_update(f32 _dt) {
    fude_look_frame(&app);
    fude_ui_follow_language(&ui);
    fude_zoom_page_topic_follow(&zoom, &ui);   // A topic was chosen: the bar shows its tools.
#if defined(RDE_PLATFORM_IOS)
    // Apple Pencil's double tap is listened for once the app's view is there; it is tried for up to 120 frames.
    static b8  _pencil_taps  = false;
    static u32 _pencil_tries = 0;
    if(!_pencil_taps && _pencil_tries < 120u) {
        _pencil_tries++;
        _pencil_taps = rde_pen_listen_double_tap(window);
        fude_redraw_after(0.0);   // (render on demand: tried again on the next frame, event or not)
    }
#endif
    if(zoom.cover_stamp != home_cover_seen) {
        home_cover_seen = zoom.cover_stamp;
        fude_zoom_home_forget_cover(&home, zoom.cover_note);   // A canvas's cover was written again: show it afresh.
    }
    fude_ink_frame_begin(&ink);
    fude_ink_frame_begin(&zoom.capture);
    if(!fude_app_update(&app, _dt)) {
        fude_zoom_page_update(&zoom);
    }
    fude_zoom_look_frame();
    fude_ui_update(&ui);
    fude_session_update(&app);
}

// One frame's drawing: an export's off-screen view first, then the page (unless a screen above it draws instead), then
// the app's overlays.
RDE_INTERNAL void fude_zoom_shell_render_frame(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    fude_zoom_page_render_offscreen(&zoom, _window, &camera);   // A PNG export's view, when one is under way.
    rde_rendering_clear_background_color(fude_theme_active()->page);
    rde_rendering_2d_begin_drawing(_window, &camera);
    if(!fude_app_render(&app)) {
        fude_zoom_page_render(&zoom, _window);
    }
    fude_app_render_overlays(&app);
    rde_rendering_2d_end_drawing();
}

// Draws on top of everything, after every UI canvas: the page's top layer, then the notice, kept clear of what the
// page shows at the top (fude_zoom_page_notice_above), or at another height while a screen is up.
RDE_INTERNAL void fude_zoom_shell_render_top(rde_window* _window, f32 _dt) {
    RDE_UNUSED(_dt);
    if(app.font == NULL) {
        return;
    }
    rde_rendering_2d_begin_drawing(_window, &camera);
    fude_zoom_page_render_top(&zoom);   // The Formula card's preview.
    fude_notice_render(_window, app.font, app.font_px, fude_app_top(&app) == NULL ? rde_math_max_f32(72.0f, fude_zoom_page_notice_above(&zoom)) : 8.0f + fude_row_height() + 16.0f);
    rde_rendering_2d_end_drawing();
}

// RDE's update, timed by drawing/app/look.h for a developer's measurements, and by itself for the frame rate's
// breakdown.
RDE_INTERNAL void fude_zoom_shell_on_update(f32 _dt) {
    const f64 _t0 = rde_engine_get_time_now();
    fude_look_timed_update(fude_zoom_shell_update, _dt);
    update_took = rde_engine_get_time_now() - _t0;
}

// RDE's render, timed the same way; the page's performance counter gets both times.
RDE_INTERNAL void fude_zoom_shell_on_render(rde_window* _window, f32 _dt) {
    const f64 _t0 = rde_engine_get_time_now();
    fude_look_timed_render(&app, fude_zoom_shell_render_frame, _window, _dt);
    fude_zoom_page_perf_frame(&zoom, update_took, rde_engine_get_time_now() - _t0);
}

// RDE's fixed update: the app has nothing to do in it.
RDE_INTERNAL void fude_zoom_shell_on_fixed_update(f32 _fixed_dt) {
    RDE_UNUSED(_fixed_dt);
}

// RDE's late update, after RDE's UI has moved: the frame is drawn, or left as it is (render on demand: redraw.h).
RDE_INTERNAL void fude_zoom_shell_on_late_update(f32 _dt) {
    RDE_UNUSED(_dt);
    fude_redraw_frame_end(window, fude_app_shown(&app));
}

// RDE's crash handler: logs the error and its call stack.
RDE_INTERNAL void fude_zoom_shell_on_crash(const c8* _error, const c8* _callstack) {
    rde_log_level(RDE_LOG_LEVEL_ERROR, "%s", _error);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "	%s", _callstack);
}

// --- the end ----------------------------------------------------------------------------------

// RDE's end: saves, then lets go of everything the app owns.
RDE_INTERNAL void fude_zoom_shell_end(void) {
    fude_session_save_on_exit(&app);
    fude_zoom_home_destroy(&home);
    fude_zoom_page_destroy(&zoom);
    fude_ui_destroy(&ui);
    fude_lasso_destroy(&lasso);
    fude_page_destroy(&page);
    fude_notes_destroy(&notes);
    fude_ink_destroy(&ink);
}

// Runs the app through RDE (see the header). A release passes on only the program's name, so neither the app nor RDE
// (which reads arguments as config overrides) sees what it was launched with.
i32 fude_zoom_shell_run(const fude_zoom_shell_def* _def, i32 _argc, c8** _argv) {
    shell = _def;
#if !FUDE_ZOOM_SHELL_TAKES_ARGS
    _argc = _argc > 1 ? 1 : _argc;
#endif
    return rde_run(_argc, _argv, FUDE_ZOOM_SHELL_CONFIG, fude_zoom_shell_init, fude_zoom_shell_event, fude_zoom_shell_on_fixed_update,
                   fude_zoom_shell_on_update, fude_zoom_shell_on_late_update, fude_zoom_shell_on_render, fude_zoom_shell_on_crash,
                   fude_zoom_shell_end);
}
