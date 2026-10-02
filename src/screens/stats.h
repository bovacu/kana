#ifndef KANA_STATS
#define KANA_STATS

#include "rde.h"
#include "chars/kanji.h"
#include "chars/catalog.h"
#include "widgets/glyph.h"
#include "widgets/scroll.h"

// ===========================================================================
// Statistics: how the learning goes, from everything the app keeps — every
// practice session (history.h: its squares, scores, mistakes and the writing's
// timing), every exam (examlog.h) and the study marks (marks.h).
//
// The numbers (kana_stats_data) are worked out when the screen opens — every
// history file read once — and the screen shows them as cards: the overview,
// a calendar of activity, how practice scores move week by week, the last
// exams, the marks week by week, how much of each level is known, studied and
// practised, the kinds of
// mistakes, the weakest and the most improved characters (a tap: the viewer),
// and when practice happens. Days are local days.
// ===========================================================================

#define KANA_STATS_WEEKS    26u                        // the calendar and the score trend
#define KANA_STATS_DAYS     (KANA_STATS_WEEKS * 7u)
#define KANA_STATS_EXAMS    12u                        // the last exams shown
#define KANA_STATS_LIST     10u                        // weakest / most improved
#define KANA_STATS_RECENT   30                         // days that are "recent"
#define KANA_STATS_POOR_SHAPE 60.0f                    // a square's shape under this counts as a shape mistake

typedef enum {
    KANA_STATS_GROUP_HIRAGANA = 0,
    KANA_STATS_GROUP_KATAKANA,
    KANA_STATS_GROUP_N5,
    KANA_STATS_GROUP_N4,
    KANA_STATS_GROUP_N3,
    KANA_STATS_GROUP_N2,
    KANA_STATS_GROUP_N1,
    KANA_STATS_GROUP_COUNT
} KANA_STATS_GROUP_;

typedef enum {
    KANA_STATS_MISTAKE_ORDER = 0,   // strokes out of order
    KANA_STATS_MISTAKE_DIRECTION,   // strokes backwards
    KANA_STATS_MISTAKE_MISSING,     // strokes left out
    KANA_STATS_MISTAKE_EXTRA,       // strokes too many
    KANA_STATS_MISTAKE_SHAPE,       // the shape under KANA_STATS_POOR_SHAPE
    KANA_STATS_MISTAKE_COUNT
} KANA_STATS_MISTAKE_;

RDE_STRUCT {
    u32 total;       // characters in the group
    u32 practised;   // with at least one practice session
    u32 studying;    // marked
    u32 known;
} kana_stats_coverage;

RDE_STRUCT {
    // Practice.
    u32 sessions;
    u32 squares;             // written and scored
    u32 characters;          // practised at least once
    f32 writing_seconds;     // the pen's time on the page, all strokes
    f32 average;             // every square's score (-1: none)
    f32 average_recent;      // the last KANA_STATS_RECENT days (-1: none)
    f32 average_before;      // the KANA_STATS_RECENT days before those (-1: none)
    u64 first_time;          // the first practice or exam (0: none)

    // Days (local): today, active ones, streaks; the calendar, oldest first, the
    // last being today; weekly score averages, oldest first (-1: none that week).
    i64 today;
    u32 days_active;
    u32 streak;              // days in a row up to today (or up to yesterday: today may still come)
    u32 streak_best;
    u16 day_squares[KANA_STATS_DAYS];
    u16 day_exam_items[KANA_STATS_DAYS];
    f32 week_average[KANA_STATS_WEEKS];
    u32 week_squares[KANA_STATS_WEEKS];
    u32 hours[24];           // squares written, by local hour
    u32 weekdays[7];         // ...by weekday, Monday first

    // Mistakes: squares with each, all time and recently.
    u32 mistakes[KANA_STATS_MISTAKE_COUNT];
    u32 mistakes_recent[KANA_STATS_MISTAKE_COUNT];
    u32 squares_recent;

    // Exams.
    u32 exams;
    u32 exams_passed;
    u32 exam_items;
    u32 exam_right;
    f32 exam_points;         // the average points (-1: none)
    u32 recent_exams;        // how many of the below there are (the last ones, oldest first)
    f32 recent_accuracy[KANA_STATS_EXAMS];   // 0..1
    f32 recent_points[KANA_STATS_EXAMS];
    u8  recent_source[KANA_STATS_EXAMS];

    // Marks and coverage.
    u32                 studying;
    u32                 known;
    // Marks over time (marks.h keeps every change): how many were Known and
    // Studying at the end of each calendar week, oldest first (the last: now),
    // and how many were Known KANA_STATS_RECENT days ago.
    u32                 week_known[KANA_STATS_WEEKS];
    u32                 week_studying[KANA_STATS_WEEKS];
    u32                 known_before;
    u32                 mark_changes;    // changes kept (0: nothing to show)
    u32                 words_added;     // the learner's vocabulary (vocab.h)
    kana_stats_coverage coverage[KANA_STATS_GROUP_COUNT];

    // Characters (records): the weakest by their latest session, and the most
    // improved from their first session to their latest.
    u32 weakest[KANA_STATS_LIST];
    f32 weakest_score[KANA_STATS_LIST];
    u32 weakest_count;
    u32 improved[KANA_STATS_LIST];
    f32 improved_delta[KANA_STATS_LIST];
    u32 improved_count;
} kana_stats_data;

// Works everything out, now (every history file read). _catalog for the levels.
void kana_stats_compute(kana_stats_data* _data, const kana_kanji_db* _db, const kana_catalog* _catalog);

// A tappable character on the screen (content space: y down from the top).
RDE_STRUCT {
    f32 x, y, size;
    u8  list;        // 0: weakest, 1: most improved
    u32 index;
} kana_stats_hit;

RDE_STRUCT {
    const kana_kanji_db*          db;
    const kana_catalog*           catalog;   // borrowed (Browse's)
    kana_glyph                    glyph;
    b8                            open;
    kana_stats_data               data;
    kana_scroller                 scroller;
    rde_arr TYPE(kana_stats_hit)  hits;
    f32                           content_height;
    f32                           view_top;
    f32                           view_bottom;
    i32                           tapped_list;    // a tapped character: its list (-1: none) and position
    u32                           tapped_index;
} kana_stats;

void kana_stats_init(kana_stats* _stats, const kana_kanji_db* _db, const kana_catalog* _catalog);
void kana_stats_destroy(kana_stats* _stats);
// Opens, working the numbers out afresh.
void kana_stats_open(kana_stats* _stats);
void kana_stats_close(kana_stats* _stats);

void kana_stats_pointer_down(kana_stats* _stats, rde_vec_2F _screen, f64 _time);
void kana_stats_pointer_moved(kana_stats* _stats, rde_vec_2F _screen, f64 _time);
void kana_stats_pointer_up(kana_stats* _stats, f64 _time);
// A character tapped: its list's records (weakest or most improved) and where it
// is in them — for the viewer. Once per tap.
b8   kana_stats_take_tap(kana_stats* _stats, const u32** _records, u32* _count, u32* _position);

void kana_stats_update(kana_stats* _stats, f32 _dt);
void kana_stats_render(kana_stats* _stats, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct kana_screen KANA_STATS_SCREEN;

#endif
