#ifndef KANA_CHECK
#define KANA_CHECK

#include "rde.h"
#include "kanji.h"
#include "catalog.h"
#include "glyph.h"
#include "ink.h"
#include "match.h"
#include "score.h"
#include "segment.h"
#include "mlkit.h"

// ===========================================================================
// Check: free writing checked — design §4, without a recogniser model.
//
// The lasso's selection on a canvas is READ (segment.h): split into the
// characters it was written as, each ranked against every character by the
// draw-to-search matcher (match.h) — what it looks like. Each character's best
// match is chosen and the writing scored against it (score.h) — order,
// direction, shape, the same feedback line as Practice; a tap on another
// candidate scores it against that one instead ("I meant this").
//
// "I meant" for the whole selection: a text typed in (romaji becomes kana,
// UPPER CASE katakana; or Japanese from the keyboard) reads the selection AS
// that text — the strokes split to fit it — and scores each character against
// what was meant.
//
// A screen of its own, over the page. One character: the writing in a square —
// the chosen character faint behind it — the candidates beside it, the score
// and its line below. More: the whole writing first, a box around each
// character read, and the reading under it, each character coloured by its
// score; a tap on either looks closer at that one, in the square below. Its row
// (toolbar): Back, Stroke order, Practice — the reading as a set.
//
// Reading takes a moment (tens of milliseconds a character in a debug build),
// so it happens the frame after the screen first shows ("Reading…").
//
// Where Google ML Kit is (iOS, mlkit.h) and nothing is typed, it reads first —
// a model trained on many writers — and the selection is split and scored AS
// its answer; Kana's own reading is what is left when it is not there.
// ===========================================================================

#define KANA_CHECK_CANDIDATES 8
#define KANA_CHECK_MEANT      128      // characters of a typed text, at most

RDE_STRUCT {
    u32               first;           // its strokes in the check's drawing: first .. first + count - 1
    u32               count;
    rde_vec_2F        min;             // their extent, canvas units
    rde_vec_2F        max;
    kana_match_result candidates[KANA_CHECK_CANDIDATES];
    u32               candidate_count;
    i32               chosen;          // into candidates, or -1
    kana_score        score;           // against the chosen one
} kana_check_char;

// Where a character is on screen, for taps (Kana screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u32        index;
} kana_check_hit;

RDE_STRUCT {
    const kana_kanji_db* db;
    const kana_catalog*  catalog;
    kana_glyph           glyph;
    b8                   open;

    kana_ink             drawing;       // the selected strokes, copied, in writing order
    rde_vec_2F           bounds_min;    // their extent, canvas units
    rde_vec_2F           bounds_max;

    kana_segment                  segment;
    rde_arr TYPE(kana_check_char) chars;       // the reading, in order
    u32                           selected;    // the character looked at closely
    b8                            pending;     // read on the next update (once the screen has shown)
    b8                            _drawn;

    // What was meant, typed: its characters as records. None: read freely.
    u32                  meant[KANA_CHECK_MEANT];
    u32                  meant_count;
    b8                   meant_failed;  // the selection could not be read as it (too few strokes)

    // ML Kit (mlkit.h), where there is one: with nothing typed, it is asked what
    // the selection says, and the selection is read as its answer.
    c8                   mlkit_text[256];   // its answer, "" for none
    b8                   by_mlkit;          // the reading is ML Kit's
    b8                   _asked_mlkit;

    // Layout of the last frame, for taps (screen space).
    rde_vec_2F           cell_tl[KANA_CHECK_CANDIDATES];
    f32                  cell_size;
    rde_arr TYPE(kana_check_hit) hits;
    rde_vec_2F           press;
    b8                   pressing;
} kana_check;

void kana_check_init(kana_check* _check, const kana_kanji_db* _db, const kana_catalog* _catalog);
void kana_check_destroy(kana_check* _check);

// Opens on strokes _ids of _ink (the lasso's selection, any order). False, and
// not opened, when there is nothing to check. The reading follows on a later
// kana_check_update.
b8   kana_check_open(kana_check* _check, const kana_ink* _ink, const u32* _ids, u32 _count);
void kana_check_close(kana_check* _check);
// Once a frame while open: reads the selection when it is due.
void kana_check_update(kana_check* _check);

// Reads the selection as _text (UTF-8: Japanese, or romaji — lower case to
// hiragana, UPPER to katakana); an empty one (or one with no characters) reads
// it freely again. Also on a later update.
void kana_check_read_as(kana_check* _check, const c8* _text);

// How many characters were read (0 while reading).
u32  kana_check_count(const kana_check* _check);
// Looks closer at character _index.
void kana_check_select(kana_check* _check, u32 _index);
// Scores the selected character against its candidate _index.
void kana_check_choose(kana_check* _check, u32 _index);
// The selected character's chosen record; false when there is none.
b8   kana_check_chosen_record(const kana_check* _check, u32* _record);
// Every character's chosen record, in order (_unique: each once): how many.
u32  kana_check_records(const kana_check* _check, u32* _out, u32 _max, b8 _unique);

// One pointer, screen space: a tap on a candidate chooses it; on a character
// (in the writing or the reading), looks closer at it.
void kana_check_pointer_down(kana_check* _check, rde_vec_2F _screen);
void kana_check_pointer_up(kana_check* _check, rde_vec_2F _screen);

// Draws the screen between _top and _bottom (screen y). Inside a 2D drawing block.
void kana_check_render(kana_check* _check, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
