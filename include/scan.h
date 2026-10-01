#ifndef KANA_SCAN
#define KANA_SCAN

#include "rde.h"
#include "scroll.h"
#include "textscan.h"
#include "translate.h"
#include "kanji.h"
#include "wordsplit.h"

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
// (Paste text's way: kana_toolbar_paste_text), where the menu was opened.
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
    KANA_SCAN_EMPTY = 0,      // nothing yet: the hint
    KANA_SCAN_WAITING,        // the library is up, or ML Kit is reading the photo
    KANA_SCAN_LIVE,           // the camera, read as it goes
    KANA_SCAN_RESULT          // a picture and its lines, to keep or leave out
} KANA_SCAN_STAGE_;

typedef enum {
    KANA_SCAN_TRANSLATION_NONE = 0,   // not asked for yet
    KANA_SCAN_TRANSLATION_ASKED,      // ticket: its answer is on its way
    KANA_SCAN_TRANSLATION_DONE        // translation: in (empty: it could not be)
} KANA_SCAN_TRANSLATION_;

RDE_STRUCT {
    c8                     text[KANA_TEXTSCAN_TEXT];
    rde_vec_2F             corners[4];     // the upright picture's pixels, y down
    u32                    block;
    b8                     kept;           // written when Write is pressed
    KANA_SCAN_TRANSLATION_ translated;
    u32                    ticket;
    c8                     translation[KANA_TRANSLATE_TEXT];
    f32                    row_top;        // its row in the panel, screen y (last frame; a tap there leaves it out)
    f32                    row_bottom;
    // Its words (wordsplit.h), found the first time its row shows.
    b8                     words_found;
    u8                     word_count;
    u32                    words[KANA_WORDSPLIT_MAX];
} kana_scan_line;

// A word's row in the panel, last frame: a tap adds it to (or takes it from)
// the learner's words.
RDE_STRUCT {
    f32 top;
    f32 bottom;
    u32 word;
} kana_scan_word_hit;
#define KANA_SCAN_WORD_HITS 64u

RDE_STRUCT {
    b8                            open;
    rde_window*                   window;
    rde_vec_2F                    canvas_at;     // where on the page the lines are written
    KANA_SCAN_STAGE_              stage;
    u32                           message;       // a KANA_TEXT_ shown under the title (0: the stage's own)

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
    rde_arr TYPE(kana_scan_line)  lines;

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

    b8                            write;            // Write was pressed: kana_scan_take_text
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
    const kana_kanji_db*          db;               // for the lines' words (NULL: none shown)
    kana_scan_word_hit            word_hits[KANA_SCAN_WORD_HITS];
    u32                           word_hit_count;
    KANA_SCAN_STAGE_              _before;          // the stage to go back to if the pick is cancelled

    // Layout of the last frame: where the picture is on screen.
    rde_vec_2F                    picture_tl;
    f32                           picture_scale;
    kana_scroller                 taps;             // taps told from drags (scroll.h)
} kana_scan;

void kana_scan_init(kana_scan* _scan);
void kana_scan_destroy(kana_scan* _scan);

// Opens on the hint; what it writes goes to _canvas_at (page units).
void kana_scan_open(kana_scan* _scan, rde_window* _window, rde_vec_2F _canvas_at);
void kana_scan_close(kana_scan* _scan);
// The app goes to the background: the camera stops (the frame shown is kept).
void kana_scan_pause(kana_scan* _scan);

// The live camera (asking for it first, when it has not been); the library.
void kana_scan_camera(kana_scan* _scan);
void kana_scan_photos(kana_scan* _scan);
// Live: keeps the frame and its lines, and stops the camera.
void kana_scan_hold(kana_scan* _scan);

// Translate with Google, on or off (the panel); whether it is on.
void kana_scan_translate(kana_scan* _scan);
b8   kana_scan_translating(const kana_scan* _scan);

// How many lines are kept; Write pressed.
u32  kana_scan_kept(const kana_scan* _scan);
void kana_scan_write(kana_scan* _scan);
// Once Write was pressed: the kept lines, one a line, into _out — true once.
b8   kana_scan_take_text(kana_scan* _scan, c8* _out, usize _size);

// The pointer, screen space: a tap on a line leaves it out or takes it back.
void kana_scan_pointer_down(kana_scan* _scan, rde_vec_2F _screen, f64 _time);
void kana_scan_pointer_moved(kana_scan* _scan, rde_vec_2F _screen, f64 _time);
void kana_scan_pointer_up(kana_scan* _scan, f64 _time);

// Once a frame while open: the camera's frames, the photo once read.
void kana_scan_update(kana_scan* _scan, f32 _dt);
// A photo's result as if it had come from the platform (the desktop's look flags).
void kana_scan_show(kana_scan* _scan, const kana_textscan_result* _result);
void kana_scan_render(kana_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
