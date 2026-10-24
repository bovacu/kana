// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ALBUM
#define FUDE_ALBUM

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/kit.h"
#include "study/models/history.h"
#include "study/models/examlog.h"

// ===========================================================================
// Album: the learner's progress, from the practice history (history.h) and the exam log (examlog.h).
//
// The OVERVIEW is every character practised, as a grid: each shows its last session's average, coloured by grade,
// and how many sessions it has. It is sorted weakest first, most recent first, or most practised first. The EXAMS
// view instead lists every exam taken (examlog.h), newest first: what it was of, when, how it went, and each
// character asked, green when it was right. A tap on an exam opens its results again (exam.h).
//
// A tap on a character opens its PAGE: the character, a summary and a trend line of every session's average, then
// its sessions and its exam answers, newest first. Each shows the date, the average (or right or wrong), and every
// attempt as it was drawn, over the model, faint. A tap on an attempt replays it stroke by stroke as it was written
// (an exam answer at a steady pace, since its timing is not kept) and says what its score said.
//
// Where it sits: a study screen (study/screens), opened from the side panel. The study shell (study/app/shell.c)
// owns the one album and places FUDE_ALBUM_SCREEN in the app's screen table.
// Its main types and their lifecycle: fude_album, made by the shell with fude_album_init over the character data and
// freed with fude_album_destroy when the app ends. fude_album_open and fude_album_close show and hide it.
// Threads: main thread only; it runs in the app's frame (update, render, pointer).
// Files: it reads the practice history files (history.h) and the exam log's writing (examlog.h); it writes nothing.
// Design: both views read the files again whenever a session has been saved (Practice, opened from a page, sits on
// top of it) or an exam taken, found by the models' revision numbers, so coming back shows the new one.
// ===========================================================================

typedef enum {
    FUDE_ALBUM_VIEW_CHARACTERS = 0,
    FUDE_ALBUM_VIEW_EXAMS
} FUDE_ALBUM_VIEW_;

typedef enum {
    FUDE_ALBUM_SORT_WEAKEST = 0,   // The lowest last average first.
    FUDE_ALBUM_SORT_RECENT,        // The most recently practised first.
    FUDE_ALBUM_SORT_MOST,          // The most sessions first.
    FUDE_ALBUM_SORT_COUNT
} FUDE_ALBUM_SORT_;

// A character on the overview.
RDE_STRUCT {
    u32                  codepoint;
    u32                  record;     // Its record in the character data.
    fude_history_summary summary;
} fude_album_entry;

// Something that can be tapped, as laid out for the last frame, in content space (y down from the top of the scrolled
// content). What index means depends on what is shown: on the overview it is an entry, in the exams view an exam
// (examlog.h), and on a character's page a square of its history, or FUDE_ALBUM_EXAM | an exam answer.
RDE_STRUCT {
    f32 x, y, w, h;
    u32 index;       // What was laid out here (see above).
} fude_album_hit;

// Set in a hit's index on a character's page when it is an exam answer (into page_exams), not a square of the history.
#define FUDE_ALBUM_EXAM 0x80000000u

RDE_STRUCT {
    const fude_kanji_db*            db;
    fude_glyph                      glyph;
    b8                              open;
    FUDE_ALBUM_VIEW_                view;
    FUDE_ALBUM_SORT_                sort;
    i32                             tapped_exam;    // In the exams view, an exam tapped, to open (-1: none).

    // The overview.
    rde_arr TYPE(fude_album_entry)  entries;
    u32                             sessions;       // Sessions over all entries.
    u32                             loaded_revision;
    b8                              loaded;
    rde_ui_scroller                 scroller;

    // A character's page (while page_open).
    b8                              page_open;
    u32                             page_codepoint;
    u32                             page_record;
    fude_history                    history;
    rde_ui_scroller                 page_scroller;
    i32                             selected;       // The square being replayed (in history.squares), or -1.
    i32                             selected_answer;// Or the exam answer being replayed (in page_exams), or -1.
    f64                             replay_start;
    // The page's exam answers, oldest first, and their writing (examlog.h: fude_examlog_read_writing).
    rde_arr TYPE(fude_examlog_writing) page_exams;
    rde_arr TYPE(fude_history_stroke)  page_exam_strokes;
    rde_arr TYPE(fude_history_point)   page_exam_points;
    u32                             exams_revision;

    // The layout of the last frame, for taps.
    rde_arr TYPE(fude_album_hit)    hits;
    f32                             view_top;
    f32                             view_bottom;
    f32                             content_height;

    // Scratch space for drawing strokes.
    rde_arr TYPE(rde_vec_2F)        _points;
} fude_album;

// Makes the album over the character data (_db may be NULL: the album then stays empty); destroy frees it.
void fude_album_init(fude_album* _album, const fude_kanji_db* _db);
void fude_album_destroy(fude_album* _album);

// Opens the album off any page, scrolled to the top, on the view last shown (the characters or the exams), reading the
// history again; closes it.
void fude_album_open(fude_album* _album);
void fude_album_close(fude_album* _album);
// fude_album_set_sort sorts the overview's characters, and shows them if the exams view was showing;
// fude_album_set_view shows the characters or the exams.
void fude_album_set_sort(fude_album* _album, FUDE_ALBUM_SORT_ _sort);
void fude_album_set_view(fude_album* _album, FUDE_ALBUM_VIEW_ _view);
// Whether an exam was tapped in the exams view; if so its index in the exam log (examlog.h) goes into *_exam. Each
// tap is handed out once.
b8   fude_album_take_exam(fude_album* _album, u32* _exam);

// The weakest characters (the lowest last average first), whatever order the overview shows: their records, at most
// _max into _out. Returns how many.
u32  fude_album_weakest(const fude_album* _album, u32* _out, u32 _max);

// Opens a character's page, reading its history and its exam answers; and goes back to the overview.
void fude_album_open_page(fude_album* _album, u32 _codepoint);
void fude_album_close_page(fude_album* _album);

// One pointer (pen, finger or mouse), in screen space: a drag scrolls, and a tap opens a character or an exam (the
// overview, the exams view) or replays an attempt (a page).
void fude_album_pointer_down(fude_album* _album, rde_vec_2F _screen, f64 _time);
void fude_album_pointer_moved(fude_album* _album, rde_vec_2F _screen, f64 _time);
void fude_album_pointer_up(fude_album* _album, f64 _time);

// Once a frame while it is the top screen: scrolls, and reads the history again when a session was saved since, and
// the page's exam answers when an exam was taken.
void fude_album_update(fude_album* _album, f32 _dt);

// Draws the album between _top and _bottom (screen y), clipped to that band. Call it inside a 2D drawing block.
void fude_album_render(fude_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The album as a screen (screen.h). Its rows: on the overview the three sorts, Exams, "Practice n" (its weakest
// characters, up to ten, as a practice set) and Close; on a character's page Back and Practice. Escape goes from a
// page back to the overview, else closes it. A kept exam tapped opens its results over the album.
extern const struct fude_screen FUDE_ALBUM_SCREEN;

#endif
