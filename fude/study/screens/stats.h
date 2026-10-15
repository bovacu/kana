// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STATS
#define FUDE_STATS

#include "rde.h"
#include "lang/lang.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"

// ===========================================================================
// Statistics: how the learning is going, from everything the app keeps: every practice session (history.h: its
// squares, scores, mistakes and the writing's timing), every exam (examlog.h), the study marks (marks.h) and the
// vocabulary (vocab.h).
//
// The numbers (fude_stats_data) are worked out when the screen opens, reading every history file once. The screen
// shows them as cards: the overview, a calendar of activity, how practice scores move week by week, the last exams,
// the marks week by week, how much of each level is known, studied and practised, the kinds of mistakes, the weakest
// and the most improved characters (a tap opens the viewer), and when practice happens. Days are local days.
//
// Where it sits: a study screen (study/screens), opened from the side panel. The study shell (study/app/shell.c)
// owns the one statistics screen and places FUDE_STATS_SCREEN in the app's screen table.
// Its main types and their lifecycle: fude_stats, made with fude_stats_init over the character data and Browse's
// catalog (borrowed, not owned) and freed with fude_stats_destroy when the app ends. fude_stats_data is plain data
// that fude_stats_compute fills, with nothing to free.
// Threads: main thread only.
// Files: it reads every practice history file (history.h) when it opens; it writes nothing.
// ===========================================================================

#define FUDE_STATS_WEEKS    26u                        // The weeks the calendar and the score trend cover.
#define FUDE_STATS_DAYS     (FUDE_STATS_WEEKS * 7u)
#define FUDE_STATS_EXAMS    12u                        // How many of the last exams are shown.
#define FUDE_STATS_LIST     10u                        // How many weakest and most improved characters are shown.
#define FUDE_STATS_RECENT   30                         // How many days count as "recent".
#define FUDE_STATS_POOR_SHAPE 60.0f                    // A square's shape under this counts as a shape mistake.

// The Levels card's rows: each of the language's sets (lang.h: the groups learnt whole, by their core characters;
// for Japanese, hiragana and katakana), then each of its levels (N5..N1). FUDE_STATS_GROUP_MAX is the most rows any
// language can have.
#define FUDE_STATS_GROUP_MAX (FUDE_LANG_GROUPS + FUDE_LANG_LEVELS)
// How many rows the language has: its sets, then its levels.
u32 fude_stats_group_count(void);

typedef enum {
    FUDE_STATS_MISTAKE_ORDER = 0,   // Strokes out of order.
    FUDE_STATS_MISTAKE_DIRECTION,   // Strokes written backwards.
    FUDE_STATS_MISTAKE_MISSING,     // Strokes left out.
    FUDE_STATS_MISTAKE_EXTRA,       // Strokes too many.
    FUDE_STATS_MISTAKE_SHAPE,       // The shape under FUDE_STATS_POOR_SHAPE.
    FUDE_STATS_MISTAKE_COUNT
} FUDE_STATS_MISTAKE_;

// One row of the Levels card.
RDE_STRUCT {
    u32 total;       // The characters in the group.
    u32 practised;   // Those with at least one practice session.
    u32 studying;    // Those marked Studying.
    u32 known;
} fude_stats_coverage;

// Everything the screen shows.
RDE_STRUCT {
    // Practice.
    u32 sessions;
    u32 squares;             // Squares written and scored.
    u32 characters;          // Characters practised at least once.
    f32 writing_seconds;     // The pen's time on the page, over all strokes.
    f32 average;             // Over every square's score (-1: none).
    f32 average_recent;      // Over the last FUDE_STATS_RECENT days (-1: none).
    f32 average_before;      // Over the FUDE_STATS_RECENT days before those (-1: none).
    u64 first_time;          // The first practice or exam, in Unix seconds (0: none).

    // Days (local): today, the active ones, the streaks; the calendar, oldest first, its last day being today; and the
    // weekly score averages, oldest first (-1 for a week with none).
    i64 today;
    u32 days_active;
    u32 streak;              // Days in a row up to today, or up to yesterday, since today may still come.
    u32 streak_best;
    u16 day_squares[FUDE_STATS_DAYS];
    u16 day_exam_items[FUDE_STATS_DAYS];
    f32 week_average[FUDE_STATS_WEEKS];
    u32 week_squares[FUDE_STATS_WEEKS];
    u32 hours[24];           // Squares written, by local hour.
    u32 weekdays[7];         // The same by weekday, Monday first.

    // Mistakes: how many squares had each kind, of all time and recently.
    u32 mistakes[FUDE_STATS_MISTAKE_COUNT];
    u32 mistakes_recent[FUDE_STATS_MISTAKE_COUNT];
    u32 squares_recent;

    // Exams.
    u32 exams;
    u32 exams_passed;
    u32 exam_items;
    u32 exam_right;
    f32 exam_points;         // The average points (-1: none).
    u32 recent_exams;        // How many of the arrays below are filled (the last exams, oldest first).
    f32 recent_accuracy[FUDE_STATS_EXAMS];   // 0..1.
    f32 recent_points[FUDE_STATS_EXAMS];
    u8  recent_source[FUDE_STATS_EXAMS];

    // Marks and coverage.
    u32                 studying;
    u32                 known;
    // The marks over time (marks.h keeps every change): how many were Known and Studying at the end of each calendar
    // week, oldest first (the last week being now), and how many were Known FUDE_STATS_RECENT days ago.
    u32                 week_known[FUDE_STATS_WEEKS];
    u32                 week_studying[FUDE_STATS_WEEKS];
    u32                 known_before;
    u32                 mark_changes;    // How many changes are kept (0: nothing to show).
    u32                 words_added;     // The words in the learner's vocabulary (vocab.h).
    fude_stats_coverage coverage[FUDE_STATS_GROUP_MAX];

    // Characters (as records): the weakest by their latest session, and the most improved from their first session
    // to their latest.
    u32 weakest[FUDE_STATS_LIST];
    f32 weakest_score[FUDE_STATS_LIST];
    u32 weakest_count;
    u32 improved[FUDE_STATS_LIST];
    f32 improved_delta[FUDE_STATS_LIST];
    u32 improved_count;
} fude_stats_data;

// Works everything out into _data, now, reading every history file. _catalog is for the levels (Browse's).
void fude_stats_compute(fude_stats_data* _data, const fude_kanji_db* _db, const fude_catalog* _catalog);

// A character that can be tapped on the screen (content space: y down from the top).
RDE_STRUCT {
    f32 x, y, size;
    u8  list;        // 0 for the weakest, 1 for the most improved.
    u32 index;
} fude_stats_hit;

RDE_STRUCT {
    const fude_kanji_db*          db;
    const fude_catalog*           catalog;   // Borrowed (Browse's).
    fude_glyph                    glyph;
    b8                            open;
    fude_stats_data               data;
    fude_scroller                 scroller;
    rde_arr TYPE(fude_stats_hit)  hits;
    f32                           content_height;
    f32                           view_top;
    f32                           view_bottom;
    i32                           tapped_list;    // A tapped character: its list (-1: none) and its position in it.
    u32                           tapped_index;
} fude_stats;

// Makes the screen over the character data and Browse's catalog (both borrowed); destroy frees it.
void fude_stats_init(fude_stats* _stats, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_stats_destroy(fude_stats* _stats);
// Opens it, working the numbers out afresh; closes it.
void fude_stats_open(fude_stats* _stats);
void fude_stats_close(fude_stats* _stats);

// One pointer, in screen space: a drag scrolls, and a tap is taken in fude_stats_update.
void fude_stats_pointer_down(fude_stats* _stats, rde_vec_2F _screen, f64 _time);
void fude_stats_pointer_moved(fude_stats* _stats, rde_vec_2F _screen, f64 _time);
void fude_stats_pointer_up(fude_stats* _stats, f64 _time);
// Whether a character was tapped; if so, its list's records (the weakest or the most improved), how many, and where
// it is in them, for the viewer to walk. The records belong to the screen and stay valid until it next opens. Each
// tap is handed out once.
b8   fude_stats_take_tap(fude_stats* _stats, const u32** _records, u32* _count, u32* _position);

// Once a frame while open: scrolls, and finds which character a tap was on. fude_stats_render draws the cards
// between _top and _bottom (screen y), inside a 2D drawing block.
void fude_stats_update(fude_stats* _stats, f32 _dt);
void fude_stats_render(fude_stats* _stats, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The statistics as a screen (screen.h). Its row is Close alone; a character tapped opens the viewer, walking its
// list.
extern const struct fude_screen FUDE_STATS_SCREEN;

#endif
