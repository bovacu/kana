// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SCAN
#define FUDE_SCAN

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "study/services/textscan.h"
#include "study/services/translate.h"
#include "study/chars/kanji.h"
#include "study/chars/wordsplit.h"

// ===========================================================================
// Text from a photo: a screen over the page that reads text from a picture and writes it on the page.
//
// Its row: Back, Camera, Photos, Translate with Google, and Hold (while the camera is live) or "Write n". There are
// two ways in:
//
//   CAMERA  the live camera (RDE's): the picture on the screen and every line ML Kit finds boxed over it, read again
//           and again (the newest frame each time the last reading is done). Hold keeps the frame and its lines.
//   PHOTOS  a photo picked from the library with the system's picker, read once.
//
// Either way, the picture is then shown with a box around every line, all of them kept at first; a tap leaves one out
// (or takes it back). Write puts the kept lines on the page, in the order read, one line each, in the characters' own
// strokes (the way Paste text does: fude_study_write_text), where the menu was opened.
//
// The camera's permission is asked for (through RDE) the first time; a refusal says where to allow it. The camera is
// closed as soon as it is not live: on Hold, on Back, or when the app goes to the background. With ML Kit switched
// off in Settings, the screen says so instead.
//
// Translate with Google (where ML Kit's translator is: translate.h): the picture shares the screen with a panel of
// the lines, each with its translation into the reader's language and Google's badge on top. The translations are
// asked for once the picture holds still (a photo, or after Hold), a few lines at a time; the first time, the models
// download, and the screen says so. Lines left out show faded. A drag scrolls the panel when it is long. Each row has
// a speaker that reads its line aloud, where there is a voice. Under each translation are the line's words (from the
// app's dictionary, wordsplit.h), each with its reading and meaning and a bookmark, filled when the word is already
// the learner's; a tap on a word opens the word card (wordcard.h), to save it or see it saved.
//
// Where it sits: a study screen (study/screens), opened from the page's long-press menu (study/widgets/pagetext.c)
// or the toolbar's camera (study/app/study.c). The study shell (study/app/shell.c) owns the one scan, places
// FUDE_SCAN_SCREEN in the app's screen table, and pauses it when the app goes to the background.
// Its main types and their lifecycle: fude_scan, made with fude_scan_init and freed with fude_scan_destroy when the
// app ends. It owns the picture shown (a photo's texture, or the camera's frame) and the camera while it is live,
// and lets them go when it closes.
// Threads: main thread only. The reading (textscan.h) and the translations (translate.h) are asynchronous: they are
// asked for, then polled once a frame. The camera's permission answer comes back on a later frame.
// Files: none of its own; the photo comes from the system's photo library.
// Design: RDE's camera subsystem is switched on only while the camera is live, so the camera is never left running.
// ===========================================================================

typedef enum {
    FUDE_SCAN_EMPTY = 0,      // Nothing yet: the hint.
    FUDE_SCAN_WAITING,        // The photo library is up, or ML Kit is reading the photo.
    FUDE_SCAN_LIVE,           // The camera, read as it goes.
    FUDE_SCAN_RESULT          // A picture and its lines, to keep or leave out.
} FUDE_SCAN_STAGE_;

typedef enum {
    FUDE_SCAN_TRANSLATION_NONE = 0,   // Not asked for yet.
    FUDE_SCAN_TRANSLATION_ASKED,      // Its ticket was given: the answer is on its way.
    FUDE_SCAN_TRANSLATION_DONE        // The translation is in (empty when it could not be had).
} FUDE_SCAN_TRANSLATION_;

// One line found in the picture.
RDE_STRUCT {
    c8                     text[FUDE_TEXTSCAN_TEXT];
    rde_vec_2F             corners[4];     // In the upright picture's pixels, y down.
    u32                    block;
    b8                     kept;           // Written when Write is pressed.
    FUDE_SCAN_TRANSLATION_ translated;
    u32                    ticket;
    c8                     translation[FUDE_TRANSLATE_TEXT];
    f32                    row_top;        // Its row in the panel, in screen y (last frame; a tap there leaves it out).
    f32                    row_bottom;
    // Its words (wordsplit.h), found the first time its row shows.
    b8                     words_found;
    u8                     word_count;
    u32                    words[FUDE_WORDSPLIT_MAX];
} fude_scan_line;

// A word's row in the panel, as laid out last frame: a tap opens the word card on it.
RDE_STRUCT {
    f32 top;
    f32 bottom;
    u32 word;
    u32 line;   // The line it is in (its sentence, for the word card).
} fude_scan_word_hit;
#define FUDE_SCAN_WORD_HITS 64u

RDE_STRUCT {
    b8                            open;
    rde_window*                   window;
    rde_vec_2F                    canvas_at;     // Where on the page the lines are written.
    FUDE_SCAN_STAGE_              stage;
    u32                           message;       // A FUDE_TEXT_ shown under the title (0: the stage's own).

    // What is shown: a photo (a texture of its own, upright) or the camera's frame (turned by its rotation).
    // picture_w and picture_h are the UPRIGHT size, the space the lines are in.
    rde_texture*                  photo;
    rde_memory_texture*           frame;
    const rde_texture*            shown;
    f32                           shown_rotation;   // Degrees clockwise that stand what is shown upright.
    b8                            shown_top_first;  // Rows top first (a camera frame); RDE's textures go bottom first.
    u32                           picture_w;
    u32                           picture_h;
    rde_arr TYPE(fude_scan_line)  lines;

    // The live camera.
    rde_device_camera*            camera;
    rde_memory_texture*           probe;            // Takes the first frame, which tells the size.
    b8                            camera_granted;   // The permission came: the camera opens on the next update.
    // Counted while live, for measuring (--perf): the frames shown, the frames read, and the time the readings took.
    u32                           frames_shown;
    u32                           frames_read;
    f64                           read_seconds;
    f64                           _read_started;    // 0 when no reading is under way.

    b8                            write;            // Write was pressed: see fude_scan_take_text.
    // Translate with Google: whether it is on (the panel shows), into which language, how many lines are on their
    // way, and whether this turn's download was asked for.
    b8                            translate;
    c8                            translate_to[8];
    u32                           translate_asked;
    b8                            translate_prepared;
    f32                           panel_content;    // The height of the panel's rows (last frame), and of its view.
    f32                           panel_view;
    f32                           panel_left;       // Where its rows are on screen (last frame).
    f32                           panel_right;
    f32                           panel_rows_top;
    f32                           panel_rows_bottom;
    const fude_kanji_db*          db;               // For the lines' words (NULL: no words are shown).
    fude_scan_word_hit            word_hits[FUDE_SCAN_WORD_HITS];
    u32                           word_hit_count;
    FUDE_SCAN_STAGE_              _before;          // The stage to go back to if the pick is cancelled.

    // The layout of the last frame: where the picture is on screen.
    rde_vec_2F                    picture_tl;
    f32                           picture_scale;
    rde_ui_scroller               taps;             // Tells taps from drags (rde_ui_scroller).
} fude_scan;

// Makes the screen, empty; destroy frees it and whatever it shows.
void fude_scan_init(fude_scan* _scan);
void fude_scan_destroy(fude_scan* _scan);

// Opens on the hint; what it writes will go to _canvas_at, in page units. Closing lets the picture and the camera go.
void fude_scan_open(fude_scan* _scan, rde_window* _window, rde_vec_2F _canvas_at);
void fude_scan_close(fude_scan* _scan);
// The app is going to the background: the camera stops, and the frame shown is kept (as Hold does).
void fude_scan_pause(fude_scan* _scan);

// Starts the live camera, asking for its permission first when it has not been given; and opens the photo library.
// Neither does anything while waiting for a photo (nor the camera while it is live); with ML Kit switched off, the
// screen says so instead.
void fude_scan_camera(fude_scan* _scan);
void fude_scan_photos(fude_scan* _scan);
// While live: keeps the frame and its lines, and stops the camera.
void fude_scan_hold(fude_scan* _scan);

// Turns Translate with Google (the panel) on or off; and whether it is on.
void fude_scan_translate(fude_scan* _scan);
b8   fude_scan_translating(const fude_scan* _scan);

// How many lines are kept (0 unless a picture with lines is shown); and Write pressed, which does something only when
// some are kept.
u32  fude_scan_kept(const fude_scan* _scan);
void fude_scan_write(fude_scan* _scan);
// Once Write was pressed: the kept lines into _out, separated by line breaks. Returns true once per press, and false
// when nothing was written; lines that do not fit in _size are left out, and none are with fude_scan_text_size bytes.
b8    fude_scan_take_text(fude_scan* _scan, c8* _out, usize _size);
// The bytes fude_scan_take_text needs to write every kept line: their text, a line break or the final 0 each, and one
// to spare. A reading keeps every line it has, so this has no bound of its own.
usize fude_scan_text_size(const fude_scan* _scan);

// The pointer, in screen space: a tap on a line (its box, or its row in the panel) leaves it out or takes it back; a
// tap on a row's speaker says the line, and one on a word opens the word card.
void fude_scan_pointer_down(fude_scan* _scan, rde_vec_2F _screen, f64 _time);
void fude_scan_pointer_moved(fude_scan* _scan, rde_vec_2F _screen, f64 _time);
void fude_scan_pointer_up(fude_scan* _scan, f64 _time);

// Once a frame while open: the camera's frames and their readings, the photo once read, and the translations.
void fude_scan_update(fude_scan* _scan, f32 _dt);
// Shows a photo's result as if it had come from the platform (for the desktop's look flags). The result's image is
// copied into a texture of the screen's own.
void fude_scan_show(fude_scan* _scan, const fude_textscan_result* _result);
// Draws the screen between _top and _bottom (screen y). Call it inside a 2D drawing block.
void fude_scan_render(fude_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// Text from a photo as a screen (screen.h). Its row: Back, Camera, Photos, Translate with Google (only where the
// translator is) and Hold or "Write n". Write closes the screen and writes the kept lines on the page.
extern const struct fude_screen FUDE_SCAN_SCREEN;

#endif
