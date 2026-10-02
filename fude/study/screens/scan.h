#ifndef FUDE_SCAN
#define FUDE_SCAN

#include "rde.h"
#include "drawing/widgets/scroll.h"
#include "study/services/textscan.h"
#include "study/services/translate.h"
#include "study/chars/kanji.h"
#include "lang/ja/wordsplit.h"

// ===========================================================================
// Text from a photo: a screen over the page, opened from its long-press menu.
//
// Its row: Back, Camera, Photos, and Hold (while the camera is live) or Write
// n. Two ways in:
//
//   CAMERA  the live camera (RDE's), the picture on the screen and every line
//           ML Kit finds boxed over it, read again and again — the newest frame
//           each time the last read is done. Hold keeps the frame and its lines.
//   PHOTOS  a photo picked from the library (the system's picker), read once.
//
// Either way, then: the picture with a box around every line — all kept at
// first; a tap leaves one out (or takes it back). Write puts the kept lines on
// the page, in the order read, one line each, in the characters' own strokes
// (Paste text's way: fude_study_write_text), where the menu was opened.
//
// The camera's permission is asked (RDE) the first time; a refusal says where
// to allow it. It is closed as soon as it is not live: Hold, Back, the app
// going to the background. With ML Kit switched off in Settings, the screen
// says so instead.
//
// Translate with Google (where ML Kit's translator is: translate.h): the
// picture shares the screen with a panel of the lines, each with its
// translation into the reader's language and Google's badge on top. Asked for
// once the picture holds still (a photo, or Hold), a few lines at a time; the
// first time, the models download (the screen says so). Left-out lines show
// faded. A drag scrolls the panel when it is long. Under each translation, the
// line's words (Kana's dictionary, wordsplit.h): each with its reading and
// meaning, + to add it to the learner's words (✓ once theirs; again to take it
// back).
// ===========================================================================

typedef enum {
    FUDE_SCAN_EMPTY = 0,      // nothing yet: the hint
    FUDE_SCAN_WAITING,        // the library is up, or ML Kit is reading the photo
    FUDE_SCAN_LIVE,           // the camera, read as it goes
    FUDE_SCAN_RESULT          // a picture and its lines, to keep or leave out
} FUDE_SCAN_STAGE_;

typedef enum {
    FUDE_SCAN_TRANSLATION_NONE = 0,   // not asked for yet
    FUDE_SCAN_TRANSLATION_ASKED,      // ticket: its answer is on its way
    FUDE_SCAN_TRANSLATION_DONE        // translation: in (empty: it could not be)
} FUDE_SCAN_TRANSLATION_;

RDE_STRUCT {
    c8                     text[FUDE_TEXTSCAN_TEXT];
    rde_vec_2F             corners[4];     // the upright picture's pixels, y down
    u32                    block;
    b8                     kept;           // written when Write is pressed
    FUDE_SCAN_TRANSLATION_ translated;
    u32                    ticket;
    c8                     translation[FUDE_TRANSLATE_TEXT];
    f32                    row_top;        // its row in the panel, screen y (last frame; a tap there leaves it out)
    f32                    row_bottom;
    // Its words (wordsplit.h), found the first time its row shows.
    b8                     words_found;
    u8                     word_count;
    u32                    words[FUDE_WORDSPLIT_MAX];
} fude_scan_line;

// A word's row in the panel, last frame: a tap adds it to (or takes it from)
// the learner's words.
RDE_STRUCT {
    f32 top;
    f32 bottom;
    u32 word;
    u32 line;   // the line it is in (its sentence, for the word card)
} fude_scan_word_hit;
#define FUDE_SCAN_WORD_HITS 64u

RDE_STRUCT {
    b8                            open;
    rde_window*                   window;
    rde_vec_2F                    canvas_at;     // where on the page the lines are written
    FUDE_SCAN_STAGE_              stage;
    u32                           message;       // a FUDE_TEXT_ shown under the title (0: the stage's own)

    // What is shown: a photo (a texture of its own, upright) or the camera's
    // frame (turned by its rotation). picture_w/_h: the UPRIGHT size, the space
    // the lines are in.
    rde_texture*                  photo;
    rde_memory_texture*           frame;
    const rde_texture*            shown;
    f32                           shown_rotation;   // degrees clockwise to stand what is shown upright
    b8                            shown_top_first;  // its rows top first (a camera frame; RDE's textures go bottom first)
    u32                           picture_w;
    u32                           picture_h;
    rde_arr TYPE(fude_scan_line)  lines;

    // The live camera.
    rde_device_camera*            camera;
    rde_memory_texture*           probe;            // takes the first frame, which tells the size
    b8                            camera_granted;   // the permission came: the camera opens on the next update
    // Counted while live, for measuring (--perf): frames shown, frames read and
    // the time the reads took.
    u32                           frames_shown;
    u32                           frames_read;
    f64                           read_seconds;
    f64                           _read_started;    // 0: no read under way

    b8                            write;            // Write was pressed: fude_scan_take_text
    // Translate with Google: on (the panel shows), into which language, how
    // many lines are on their way, and whether this turn's download was asked.
    b8                            translate;
    c8                            translate_to[8];
    u32                           translate_asked;
    b8                            translate_prepared;
    f32                           panel_content;    // the panel's rows, tall (last frame), and its view
    f32                           panel_view;
    f32                           panel_left;       // where its rows are on screen (last frame)
    f32                           panel_right;
    f32                           panel_rows_top;
    f32                           panel_rows_bottom;
    const fude_kanji_db*          db;               // for the lines' words (NULL: none shown)
    fude_scan_word_hit            word_hits[FUDE_SCAN_WORD_HITS];
    u32                           word_hit_count;
    FUDE_SCAN_STAGE_              _before;          // the stage to go back to if the pick is cancelled

    // Layout of the last frame: where the picture is on screen.
    rde_vec_2F                    picture_tl;
    f32                           picture_scale;
    fude_scroller                 taps;             // taps told from drags (scroll.h)
} fude_scan;

void fude_scan_init(fude_scan* _scan);
void fude_scan_destroy(fude_scan* _scan);

// Opens on the hint; what it writes goes to _canvas_at (page units).
void fude_scan_open(fude_scan* _scan, rde_window* _window, rde_vec_2F _canvas_at);
void fude_scan_close(fude_scan* _scan);
// The app goes to the background: the camera stops (the frame shown is kept).
void fude_scan_pause(fude_scan* _scan);

// The live camera (asking for it first, when it has not been); the library.
void fude_scan_camera(fude_scan* _scan);
void fude_scan_photos(fude_scan* _scan);
// Live: keeps the frame and its lines, and stops the camera.
void fude_scan_hold(fude_scan* _scan);

// Translate with Google, on or off (the panel); whether it is on.
void fude_scan_translate(fude_scan* _scan);
b8   fude_scan_translating(const fude_scan* _scan);

// How many lines are kept; Write pressed.
u32  fude_scan_kept(const fude_scan* _scan);
void fude_scan_write(fude_scan* _scan);
// Once Write was pressed: the kept lines, one a line, into _out — true once.
b8   fude_scan_take_text(fude_scan* _scan, c8* _out, usize _size);

// The pointer, screen space: a tap on a line leaves it out or takes it back.
void fude_scan_pointer_down(fude_scan* _scan, rde_vec_2F _screen, f64 _time);
void fude_scan_pointer_moved(fude_scan* _scan, rde_vec_2F _screen, f64 _time);
void fude_scan_pointer_up(fude_scan* _scan, f64 _time);

// Once a frame while open: the camera's frames, the photo once read.
void fude_scan_update(fude_scan* _scan, f32 _dt);
// A photo's result as if it had come from the platform (the desktop's look flags).
void fude_scan_show(fude_scan* _scan, const fude_textscan_result* _result);
void fude_scan_render(fude_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_SCAN_SCREEN;

#endif
