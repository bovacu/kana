#ifndef FUDE_ALBUM
#define FUDE_ALBUM

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/history.h"
#include "study/models/examlog.h"

// ===========================================================================
// The album: progress, from the practice history (history.h).
//
// The OVERVIEW is every character practised, as a grid: its last session's
// average, coloured by grade, and how many sessions — sorted weakest first, most
// recent first, or most practised first. Or, the EXAMS view, every exam taken
// (examlog.h), newest first: what it was of, when, how it went, and each
// character asked, green when right; a tap opens its results again (exam.h).
//
// A tap on a character opens its PAGE: the character, a summary and a trend line
// of every session's average, then the sessions and its exam answers, newest
// first — the date, the average (or right or wrong), and every attempt as it was
// drawn, over the model, faint. A tap on an attempt replays it stroke by stroke,
// as it was written (an exam's at a steady pace: its timing is not kept), and
// says what its score said.
//
// Both re-read the history whenever a session was saved (Practice, opened from a
// page, sits on top of it), and the exams whenever one was taken, so coming back
// shows the new one.
// ===========================================================================

typedef enum {
    FUDE_ALBUM_VIEW_CHARACTERS = 0,
    FUDE_ALBUM_VIEW_EXAMS
} FUDE_ALBUM_VIEW_;

typedef enum {
    FUDE_ALBUM_SORT_WEAKEST = 0,   // lowest last average first
    FUDE_ALBUM_SORT_RECENT,        // most recently practised first
    FUDE_ALBUM_SORT_MOST,          // most sessions first
    FUDE_ALBUM_SORT_COUNT
} FUDE_ALBUM_SORT_;

RDE_STRUCT {
    u32                  codepoint;
    u32                  record;     // in the character data
    fude_history_summary summary;
} fude_album_entry;

// Something tappable, as laid out for the last frame (content space: y down
// from the top of the scrolled content).
RDE_STRUCT {
    f32 x, y, w, h;
    u32 index;       // overview: an entry, or an exam (examlog.h); page: a square of the history, or FUDE_ALBUM_EXAM | an exam answer
} fude_album_hit;

#define FUDE_ALBUM_EXAM 0x80000000u

RDE_STRUCT {
    const fude_kanji_db*            db;
    fude_glyph                      glyph;
    b8                              open;
    FUDE_ALBUM_VIEW_                view;
    FUDE_ALBUM_SORT_                sort;
    i32                             tapped_exam;    // the exams view: an exam tapped, to open (-1: none)

    // The overview.
    rde_arr TYPE(fude_album_entry)  entries;
    u32                             sessions;       // over all entries
    u32                             loaded_revision;
    b8                              loaded;
    fude_scroller                   scroller;

    // A character's page (page_open).
    b8                              page_open;
    u32                             page_codepoint;
    u32                             page_record;
    fude_history                    history;
    fude_scroller                   page_scroller;
    i32                             selected;       // the square being replayed (history.squares), or -1
    i32                             selected_answer;// ...or the exam answer (page_exams), or -1
    f64                             replay_start;
    // ...its exam answers, oldest first, and their writing.
    rde_arr TYPE(fude_examlog_writing) page_exams;
    rde_arr TYPE(fude_history_stroke)  page_exam_strokes;
    rde_arr TYPE(fude_history_point)   page_exam_points;
    u32                             exams_revision;

    // Layout of the last frame, for taps.
    rde_arr TYPE(fude_album_hit)    hits;
    f32                             view_top;
    f32                             view_bottom;
    f32                             content_height;

    // Scratch for drawing strokes.
    rde_arr TYPE(rde_vec_2F)        _points;
} fude_album;

void fude_album_init(fude_album* _album, const fude_kanji_db* _db);
void fude_album_destroy(fude_album* _album);

// Opens on the overview (re-reading the history).
void fude_album_open(fude_album* _album);
void fude_album_close(fude_album* _album);
// Sorting shows the characters; the exams view, the exams.
void fude_album_set_sort(fude_album* _album, FUDE_ALBUM_SORT_ _sort);
void fude_album_set_view(fude_album* _album, FUDE_ALBUM_VIEW_ _view);
// An exam tapped in the exams view: its index (examlog.h), once per tap.
b8   fude_album_take_exam(fude_album* _album, u32* _exam);

// The weakest characters (lowest last average first), as records: up to _max
// into _out; how many.
u32  fude_album_weakest(const fude_album* _album, u32* _out, u32 _max);

// A character's page; back to the overview.
void fude_album_open_page(fude_album* _album, u32 _codepoint);
void fude_album_close_page(fude_album* _album);

// One pointer (pen, finger or mouse), screen space: drags scroll, taps open a
// character or an exam (overview) or replay an attempt (page).
void fude_album_pointer_down(fude_album* _album, rde_vec_2F _screen, f64 _time);
void fude_album_pointer_moved(fude_album* _album, rde_vec_2F _screen, f64 _time);
void fude_album_pointer_up(fude_album* _album, f64 _time);

// Once a frame while it is the top screen: scrolling, and a re-read if a session
// was saved since.
void fude_album_update(fude_album* _album, f32 _dt);

// Draws the album between _top and _bottom (screen y). Inside a 2D drawing block.
void fude_album_render(fude_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_ALBUM_SCREEN;

#endif
