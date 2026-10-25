// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/app/study.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/app/side.h"
#include "drawing/app/handwriting.h"
#include "drawing/base/text.h"
#include "study/models/marks.h"
#include "study/models/review.h"
#include "drawing/ink/mlkit.h"
#include "study/services/textscan.h"
#include "study/services/speech.h"
#include "study/app/welcome.h"
#include "drawing/widgets/notice.h"
#include "drawing/base/icons.h"
#include "drawing/doc/doc.h"

// ===========================================================================
// See study.h.
//
// This file holds the study's lifetime, the hooks that hand the page's reading and the word card on to pagetext.c and
// wordcard.c, the toolbar's camera, Settings' Handwriting section and the reading of a document's scanned pages. The
// verbs are in verbs.c, the files in files.c, the launch flags in look.c and the demo in demo.c.
// ===========================================================================

// Returns how many reviews are due today, for the side panel's count. The panel asks every frame, so the count is
// kept and taken again only when what it depends on changes: the marks' revision, the reviews' revision and today's
// day number, each compared as it is (a sum of them could match another day's, and leave the count stale). The count
// is at most FUDE_REVIEW_SESSION, because that is all fude_study_reviews_due finds.
u32 fude_study_reviews_count(fude_app* _app) {
    fude_study* _study  = FUDE_STUDY(_app);
    const u32   _for[3] = { fude_marks_revision(), fude_reviews_revision(), fude_reviews_today() };
    if(!_study->_reviews_known || rde_memcmp(_for, _study->_reviews_for, sizeof(_for)) != 0) {
        u32 _records[FUDE_REVIEW_SESSION];
        _study->_reviews_count = fude_study_reviews_due(_app, _records, FUDE_REVIEW_SESSION);
        rde_memcpy(_study->_reviews_for, _for, sizeof(_for));
        _study->_reviews_known = true;
    }
    return _study->_reviews_count;
}

// --- lifetime ------------------------------------------------------------------------------

// Sets up the page read as text (pagetext.h). The app must be set up, with the study's fields filled in, first.
void fude_study_init(fude_study* _study) {
    fude_pagetext_init(&_study->text, &_study->app);
}

// Lets go of the page read as text.
void fude_study_destroy(fude_study* _study) {
    fude_pagetext_destroy(&_study->text);
}

// Once a frame, before the screens' update. Select mode ends when no screen that ticks (one whose vt has selects) is
// open any more, so the ticks are not left showing over a screen that cannot use them.
void fude_study_update(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    if(_study->selection != NULL && _study->selection->active) {
        b8 _ticking = false;
        for(u32 _s = 0; _s < _app->screen_count; _s++) {
            _ticking = _ticking || (_app->screens[_s].vt != NULL && _app->screens[_s].vt->selects && fude_app_is_open(_app, _s));
        }
        _study->selection->active = _ticking;
    }
    fude_speech_update();   // (Android: the app's audio category back once the voice has stopped)
    if(fude_speech_take_hint()) {
        // Once the basic voice has spoken, a notice says where better voices are found.
        fude_notice_show(fude_text(FUDE_TEXT_SPEECH_BETTER_VOICE));   // where to find better voices
    }
    fude_welcome_update(_study->welcome);   // the voice card, shown once, after the offline card
}

// --- the side panel's button for a missing voice ----------------------------------------

// The side panel's Set up the voice button shows only while the device has no voice for the language taught.
RDE_INTERNAL b8 fude_study_voice_shown(fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_speech_state() == FUDE_SPEECH_MISSING;
}

// The button opens the voice card, which says how to add a voice (welcome.h).
RDE_INTERNAL void fude_study_voice_press(fude_app* _app) {
    if(FUDE_STUDY(_app)->welcome != NULL) {
        fude_welcome_voice(FUDE_STUDY(_app)->welcome);
    }
}

const fude_extension_side_button FUDE_STUDY_VOICE_BUTTON = { FUDE_TEXT_VOICE_BUTTON, FUDE_ICON_SPEAK, fude_study_voice_shown, fude_study_voice_press };

// --- the page read as text ------------------------------------------------------------------

// The page's menus: the page read as text keeps its state up to date (pagetext.h).
void fude_study_menu_update(fude_app* _app) {
    fude_pagetext_update(&FUDE_STUDY(_app)->text);
}

// Sets the faces of the selection's menu (pagetext.h).
void fude_study_selection_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    fude_pagetext_selection_faces(&FUDE_STUDY(_app)->text, _row, _faces);
}

// Sets the faces of the page's context menu (pagetext.h).
void fude_study_context_faces(fude_app* _app, const fude_row_def* _row, fude_row_face* _faces) {
    fude_pagetext_context_faces(&FUDE_STUDY(_app)->text, _row, _faces);
}

// Draws what the page's reading leaves over the page.
void fude_study_page_render(fude_app* _app, rde_window* _window) {
    fude_pagetext_render(&FUDE_STUDY(_app)->text, _window);   // Translate with Google's card, beside the selection
}

// A press over the page, given first to the translation card.
void fude_study_page_press(fude_app* _app, rde_vec_2F _screen) {
    fude_pagetext_press(&FUDE_STUDY(_app)->text, _screen);   // the card's speaker, or a word on it
}

// --- its widgets ------------------------------------------------------------------------------

// Builds the word card on the UI's canvas.
void fude_study_ui_build(fude_ui* _ui, rde_ui_node* _root) {
    fude_wordcard_create(_ui, _root);   // over everything, because it opens from any screen
}

// Lets go of the word card's own memory (its lists' chips) before the canvas, which frees its widgets, goes.
void fude_study_ui_forget(fude_ui* _ui) {
    fude_wordcard_forget(_ui);
}

// Once a frame: opens the word card when a word was asked for (wordcard.h), and lays it out again when the screen
// turns.
void fude_study_ui_update(fude_ui* _ui, b8 _full) {
    RDE_UNUSED(_full);
    fude_wordcard_update(_ui);   // opens a word asked for, and lays it out again when the screen turns
}

// Gives the word card the active theme's colours.
void fude_study_ui_restyle(fude_ui* _ui) {
    fude_wordcard_apply_theme(_ui);
}

// Returns true when the point is on the translation card beside the selection, or anywhere while the word card is
// open, because the word card takes the whole screen.
b8 fude_study_ui_hit(const fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas) {
    RDE_UNUSED(_canvas);
    const fude_study* _study = FUDE_STUDY(_ui->app);
    return _study->word.open || fude_pagetext_hit(&_study->text, _screen);
}

// Writes _text on the page at _canvas in the characters' own strokes (pagetext.h).
void fude_study_write_text(fude_app* _app, const c8* _text, rde_vec_2F _canvas) {
    fude_pagetext_write(&FUDE_STUDY(_app)->text, _text, _canvas);
}

// --- the toolbar's camera -------------------------------------------------------------------

// Opens Text from a photo with the camera live straight away. What is written from it goes to the middle of the page
// as it is on screen. The tool shows only where text can be read in pictures (fude_textscan_available).
RDE_INTERNAL void fude_study_camera(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    fude_scan_open(_study->scan, _app->window, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
    fude_scan_camera(_study->scan);
}

const fude_extension_tool FUDE_STUDY_CAMERA = { .text = FUDE_TEXT_SCAN_CAMERA, .icon = FUDE_ICON_CAMERA, .press = fude_study_camera,
                                                 .available = fude_textscan_available };

// --- Settings' sections ------------------------------------------------------------------

// Handwriting (drawing/app/handwriting.h): Google ML Kit on or off, and its model.
const fude_extension_section FUDE_STUDY_SECTIONS[FUDE_STUDY_SECTION_COUNT] = { FUDE_HANDWRITING_SECTION };

// --- pictures read: a document's scanned pages ---------------------------------------------
//
// A debug build can make the lines up instead of reading them (--read-demo), to look at a document's scanned pages
// without a device. The demo keeps the size of the picture asked for, and is due once for each picture.

RDE_INTERNAL b8  fude_study_picture_demo_on = false;
RDE_INTERNAL b8  fude_study_picture_demo_due = false;
RDE_INTERNAL u32 fude_study_picture_demo_w, fude_study_picture_demo_h;

// Turns made-up lines on or off. Only a debug build does anything.
void fude_study_picture_demo(b8 _on) {
#if defined(RDE_DEBUG)
    fude_study_picture_demo_on = _on;
#else
    RDE_UNUSED(_on);
#endif
}

// Starts reading a page's picture with ML Kit (textscan.h), or remembers it for the made-up lines.
b8 fude_study_picture_read(const u8* _rgba, u32 _w, u32 _h) {
    if(fude_study_picture_demo_on) {
        fude_study_picture_demo_due = true;
        fude_study_picture_demo_w   = _w;
        fude_study_picture_demo_h   = _h;
        return true;
    }
    return fude_textscan_read_page(_rgba, _w, _h);
}

// Returns true once the lines of the picture asked for are in, and adds them to _out (see the header).
b8 fude_study_picture_lines(rde_arr* _out) {
    if(fude_study_picture_demo_on) {
        // The made-up lines: four down the top half of the page, the same on every page.
        if(!fude_study_picture_demo_due) {
            return false;
        }
        fude_study_picture_demo_due = false;
        static const c8* const _says[4] = { "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x82\x92\xE5\x8B\x89\xE5\xBC\xB7\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99",   // 日本語を勉強します
                                            "Scan line two", "\xE3\x81\x82\xE3\x82\x8A\xE3\x81\x8C\xE3\x81\xA8\xE3\x81\x86",   // ありがとう
                                            "The last line read" };
        const f32 _w = (f32)fude_study_picture_demo_w, _h = (f32)fude_study_picture_demo_h;
        for(u32 _i = 0; _i < 4u; _i++) {
            fude_doc_line* _l = (fude_doc_line*)rde_arr_add_n(_out, 1u);
            rde_memset(_l, 0, sizeof(*_l));
            _l->from = (rde_vec_2F){ _w * 0.12f, _h * (0.18f + 0.08f * (f32)_i) };
            _l->size = (rde_vec_2F){ _w * 0.6f, _h * 0.035f };
            rde_snprintf(_l->text, sizeof(_l->text), "%s", _says[_i]);
        }
        return true;
    }
    fude_textscan_result _r;
    if(!fude_textscan_poll_page(&_r)) {
        return false;
    }
    // Each line's box is the extent of its four corners, in the picture's pixels.
    for(u32 _i = 0; _i < _r.line_count; _i++) {
        const fude_textscan_line* _line = &_r.lines[_i];
        rde_vec_2F _lo = _line->corners[0], _hi = _line->corners[0];
        for(u32 _k = 1; _k < 4u; _k++) {
            _lo = (rde_vec_2F){ rde_math_min_f32(_lo.x, _line->corners[_k].x), rde_math_min_f32(_lo.y, _line->corners[_k].y) };
            _hi = (rde_vec_2F){ rde_math_max_f32(_hi.x, _line->corners[_k].x), rde_math_max_f32(_hi.y, _line->corners[_k].y) };
        }
        fude_doc_line* _l = (fude_doc_line*)rde_arr_add_n(_out, 1u);
        rde_memset(_l, 0, sizeof(*_l));
        _l->from = _lo;
        _l->size = (rde_vec_2F){ _hi.x - _lo.x, _hi.y - _lo.y };
        rde_snprintf(_l->text, sizeof(_l->text), "%s", _line->text);
    }
    return true;
}

// Opens the welcome at its first page (the side panel's Tutorial, and the very first launch).
void fude_study_tutorial(fude_app* _app) {
    fude_welcome_open(FUDE_STUDY(_app)->welcome);
}
