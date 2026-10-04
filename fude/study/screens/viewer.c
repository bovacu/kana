#include "study/screens/viewer.h"
#include "lang/lang.h"
#include "study/widgets/header.h"
#include "study/services/speech.h"
#include "study/widgets/wordcard.h"
#include "lang/wordsplit.h"
#include "study/models/examlog.h"
#include "study/models/charnote.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/kit.h"
#include "study/models/select.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See viewer.h.
// ===========================================================================

#define FUDE_VIEWER_MARGIN     28.0f     // screen units around everything
#define FUDE_VIEWER_SWIPE      56.0f     // a swipe this far across turns the page...
#define FUDE_VIEWER_FLICK      28.0f     // ...or this far, quickly
#define FUDE_VIEWER_FLICK_TIME 0.25      // (seconds)
#define FUDE_VIEWER_LINE       44.0f     // text line height
#define FUDE_VIEWER_KANA_SIZE  30.0f     // a stroke-drawn kana in a reading
#define FUDE_VIEWER_GUTTER     40.0f     // wide: between the character and its details
#define FUDE_VIEWER_MIN_CHAR   300.0f    // stacked: words go (down to 3; 2 for a sentence) before the character gets smaller than this
#define FUDE_VIEWER_MIN_CHAR_SENT 260.0f // ...or than this, to keep the example sentence
#define FUDE_VIEWER_TITLE      48.0f     // the "Words" line: its title and Add
#define FUDE_VIEWER_ROW        44.0f     // an example word's row (a finger's height)
#define FUDE_VIEWER_WORD_PX    19.0f     // its written form
#define FUDE_VIEWER_SMALL_PX   13.0f     // its reading and meaning
#define FUDE_VIEWER_CHIP_PX    11.0f     // the header's chips
#define FUDE_VIEWER_MEANING_PX 22.0f     // the meaning, the details' title
#define FUDE_VIEWER_CARD_PAD   12.0f     // the words' card, inside
#define FUDE_VIEWER_SENT_PX    18.0f     // the example sentence's Japanese
#define FUDE_VIEWER_SENT_LINE  27.0f     // ...a line of it
#define FUDE_VIEWER_TRANS_PX   14.0f     // its translation
#define FUDE_VIEWER_TRANS_LINE 20.0f     // ...a line of it
#define FUDE_VIEWER_SENT_TITLE 36.0f     // the sentence card's title line
#define FUDE_VIEWER_SENT_GAP   10.0f     // between the words' card and the sentence's
#define FUDE_VIEWER_SPEAKER_W  28.0f     // the speaker before the sentence
#define FUDE_VIEWER_CHIP_H     28.0f     // a sentence's word
#define FUDE_VIEWER_CHIP_GAP   5.0f

void fude_viewer_init(fude_viewer* _viewer, const fude_kanji_db* _db) {
    memset(_viewer, 0, sizeof(*_viewer));
    _viewer->db   = _db;
    _viewer->list     = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _viewer->pressed  = -1;
    _viewer->rows_for = UINT32_MAX;
    _viewer->sentences_for = UINT32_MAX;
    _viewer->similar_for   = UINT32_MAX;
    _viewer->similar_pressed = -1;
    fude_glyph_init(&_viewer->glyph, _db);
}

void fude_viewer_destroy(fude_viewer* _viewer) {
    if(rde_arr_is_inited(&_viewer->list)) {
        rde_arr_free(&_viewer->list);
    }
    fude_glyph_destroy(&_viewer->glyph);
    memset(_viewer, 0, sizeof(*_viewer));
}

b8 fude_viewer_available(const fude_viewer* _viewer) {
    return _viewer->db != NULL && _viewer->db->count > 0;
}

void fude_viewer_replay(fude_viewer* _viewer) {
    _viewer->started = rde_engine_get_time_now();
}

// --- the words: listed, scrolled, tapped ------------------------------------------------

RDE_INTERNAL b8 fude_viewer_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

// A character with words: in the data (a kanji's, a Korean syllable's), or the learner's.
RDE_INTERNAL b8 fude_viewer_has_words(const fude_viewer* _viewer, u32 _record, u32 _codepoint) {
    u32 _one[1];
    return fude_kanji_words(_viewer->db, _record, _one, 1u, NULL) > 0u || fude_vocab_kanji_count(_codepoint) > 0u;
}

u32 fude_viewer_codepoint(const fude_viewer* _viewer) {
    fude_kanji_info _info;
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0 ||
       !fude_kanji_at(_viewer->db, ((const u32*)_viewer->list.memory)[_viewer->position], &_info)) {
        return 0;
    }
    return _info.codepoint;
}

RDE_INTERNAL void fude_viewer_row_set(fude_viewer_row* _row, u8 _kind, b8 _ticked, const fude_kanji_word* _w) {
    _row->kind   = _kind;
    _row->ticked = _ticked;
    snprintf(_row->written, sizeof(_row->written), "%s", _w->written);
    snprintf(_row->reading, sizeof(_row->reading), "%s", _w->reading);
    snprintf(_row->meaning, sizeof(_row->meaning), "%s", _w->meaning);
}

// The words for the character on screen: listed, the learner's first then the
// examples; in Add, the ones typed in, then the character's further words.
RDE_INTERNAL void fude_viewer_list_rows(fude_viewer* _viewer, u32 _record, u32 _codepoint) {
    const b8 _moved = _viewer->rows_for != _record || _viewer->rows_adding != _viewer->adding;
    if(!_moved && _viewer->rows_words == fude_vocab_revision()) {
        return;
    }
    _viewer->rows_for    = _record;
    _viewer->rows_adding = _viewer->adding;
    _viewer->rows_words  = fude_vocab_revision();
    _viewer->row_count   = 0;
    if(_moved) {
        fude_scroller_stop(&_viewer->scroll);
        _viewer->scroll.offset = 0.0f;
    }
    if(!fude_viewer_is_kanji(_codepoint) && !fude_viewer_has_words(_viewer, _record, _codepoint)) {
        return;
    }

    u32       _words[FUDE_KANJI_MAX_WORDS];
    u32       _examples = 0;
    const u32 _n        = fude_kanji_words(_viewer->db, _record, _words, FUDE_KANJI_MAX_WORDS, &_examples);
    const u32 _yours    = fude_vocab_kanji_count(_codepoint);
    fude_kanji_word _w;
    for(u32 _i = 0; _i < _yours && _viewer->row_count < FUDE_VIEWER_ROWS; _i++) {
        if(!fude_vocab_kanji_at(_codepoint, _i, &_w)) {
            continue;
        }
        if(_viewer->adding) {
            // In Add, only the ones typed in (the others show where they are, ticked).
            b8 _listed = false;
            for(u32 _k = 0; _k < _n && !_listed; _k++) {
                fude_kanji_word _d;
                _listed = fude_kanji_word_at(_viewer->db, _words[_k], &_d) && strcmp(_d.written, _w.written) == 0 && strcmp(_d.reading, _w.reading) == 0;
            }
            if(_listed) {
                continue;
            }
        }
        fude_viewer_row_set(&_viewer->rows[_viewer->row_count++], _viewer->adding ? FUDE_VIEWER_ROW_TYPED : FUDE_VIEWER_ROW_YOURS, true, &_w);
    }
    const u32 _from = _viewer->adding ? _examples : 0u;
    const u32 _to   = _viewer->adding ? _n : _examples;
    for(u32 _k = _from; _k < _to && _viewer->row_count < FUDE_VIEWER_ROWS; _k++) {
        if(fude_kanji_word_at(_viewer->db, _words[_k], &_w)) {
            fude_viewer_row_set(&_viewer->rows[_viewer->row_count++], _viewer->adding ? FUDE_VIEWER_ROW_TO_ADD : FUDE_VIEWER_ROW_EXAMPLE,
                                _viewer->adding && (fude_vocab_find(_w.written, _w.reading) != 0u), &_w);
        }
    }
}

// The character's words with an example sentence: the learner's first, then its
// examples, then the rest; each sentence once.
RDE_INTERNAL void fude_viewer_list_sentences(fude_viewer* _viewer, u32 _record, u32 _codepoint) {
    const b8 _moved = _viewer->sentences_for != _record;
    if(!_moved && _viewer->sentences_words == fude_vocab_revision()) {
        return;
    }
    const u32 _shown = _viewer->sentence_at < _viewer->sentence_count ? _viewer->sentences[_viewer->sentence_at] : UINT32_MAX;
    _viewer->sentences_for   = _record;
    _viewer->sentences_words = fude_vocab_revision();
    if(_moved) {
        _viewer->sentence_room = 0.0f;
    }
    _viewer->sentence_count  = 0;
    _viewer->sentence_at     = 0;
    if(!fude_viewer_is_kanji(_codepoint)) {
        return;
    }
    u32       _words[FUDE_KANJI_MAX_WORDS];
    const u32 _n = fude_kanji_words(_viewer->db, _record, _words, FUDE_KANJI_MAX_WORDS, NULL);
    const c8* _seen[FUDE_VIEWER_SENTENCES];
    for(u32 _pass = 0; _pass < 2u; _pass++) {   // the learner's, then the others
        for(u32 _k = 0; _k < _n && _viewer->sentence_count < FUDE_VIEWER_SENTENCES; _k++) {
            fude_kanji_word     _w;
            fude_kanji_sentence _s;
            if(!fude_kanji_word_at(_viewer->db, _words[_k], &_w) || ((fude_vocab_find(_w.written, _w.reading) != 0u) != (_pass == 0u)) ||
               !fude_kanji_word_sentence(_viewer->db, _words[_k], &_s)) {
                continue;
            }
            b8 _again = false;
            for(u32 _i = 0; _i < _viewer->sentence_count && !_again; _i++) {
                _again = _seen[_i] == _s.japanese;
            }
            if(!_again) {
                const u32 _at = _viewer->sentence_count++;
                _seen[_at]               = _s.japanese;
                _viewer->sentences[_at]  = _words[_k];
                _viewer->sentence_word_n[_at] = (u8)fude_wordsplit(_viewer->db, _s.japanese, _viewer->sentence_words[_at], FUDE_VIEWER_SENTENCE_WORDS);
            }
        }
    }
    for(u32 _i = 0; !_moved && _i < _viewer->sentence_count; _i++) {
        if(_viewer->sentences[_i] == _shown) {
            _viewer->sentence_at = _i;   // the learner's words changed: still that one
        }
    }
}

// What the character is mixed up with: the learner's exams first (read as
// another, or another read as it — the latest first), then its look-alikes.
RDE_INTERNAL void fude_viewer_list_similar(fude_viewer* _viewer, u32 _record, u32 _codepoint) {
    if(_viewer->similar_for == _record && _viewer->similar_log == fude_examlog_revision()) {
        return;
    }
    _viewer->similar_for   = _record;
    _viewer->similar_log   = fude_examlog_revision();
    _viewer->similar_count = 0;
    _viewer->similar_mine  = 0;
    #define FUDE_VIEWER_ADD_SIMILAR(_cp) do { \
        b8 _seen = (_cp) == 0u || (_cp) == _codepoint; \
        for(u32 _k = 0; _k < _viewer->similar_count && !_seen; _k++) { _seen = _viewer->similar[_k] == (_cp); } \
        u32 _index; \
        if(!_seen && _viewer->similar_count < FUDE_VIEWER_SIMILAR && fude_kanji_find_index(_viewer->db, (_cp), &_index)) { \
            _viewer->similar[_viewer->similar_count++] = (_cp); \
        } \
    } while(0)
    const fude_examlog_exam* _exams = fude_examlog_exams();
    const fude_examlog_item* _items = fude_examlog_items();
    for(u32 _e = fude_examlog_count(); _e-- > 0 && _viewer->similar_count < 3u;) {
        for(u32 _i = _exams[_e].item_count; _i-- > 0 && _viewer->similar_count < 3u;) {
            const fude_examlog_item* _it = &_items[_exams[_e].first_item + _i];
            if(_it->correct || _it->read_as == 0u) {
                continue;
            }
            if(_it->codepoint == _codepoint) {
                FUDE_VIEWER_ADD_SIMILAR(_it->read_as);
            } else if(_it->read_as == _codepoint) {
                FUDE_VIEWER_ADD_SIMILAR(_it->codepoint);
            }
        }
    }
    _viewer->similar_mine = _viewer->similar_count;
    u32       _look[FUDE_KANJI_LOOKALIKES];
    const u32 _n = fude_kanji_lookalikes(_viewer->db, _record, _look, FUDE_KANJI_LOOKALIKES);
    for(u32 _i = 0; _i < _n; _i++) {
        FUDE_VIEWER_ADD_SIMILAR(_look[_i]);
    }
    #undef FUDE_VIEWER_ADD_SIMILAR
}

RDE_INTERNAL b8 fude_viewer_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

RDE_INTERNAL i32 fude_viewer_row_at(const fude_viewer* _viewer, rde_vec_2F _p) {
    if(!fude_viewer_inside(_p, _viewer->list_min, _viewer->list_max) || _viewer->row_h <= 0.0f) {
        return -1;
    }
    const i32 _i = (i32)floorf((_viewer->list_max.y - _p.y + _viewer->scroll.offset) / _viewer->row_h);
    return _i >= 0 && _i < (i32)_viewer->row_count ? _i : -1;
}

// The character's readings aloud: its on and kun (without the - and . that mark
// where okurigana go), or a kana itself.
RDE_INTERNAL void fude_viewer_speak_readings(fude_viewer* _viewer) {
    fude_kanji_info _info;
    if(_viewer->db == NULL || !fude_kanji_find(_viewer->db, fude_viewer_codepoint(_viewer), &_info)) {
        return;
    }
    c8        _say[512];
    usize     _n     = 0;
    const c8* _parts[2] = { fude_kanji_reading(_viewer->db, &_info, 0u), fude_kanji_reading(_viewer->db, &_info, 1u) };
    for(u32 _k = 0; _k < 2u; _k++) {
        if(_parts[_k][0] == 0) {
            continue;
        }
        if(_n > 0 && _n + 3u < sizeof(_say)) {
            memcpy(&_say[_n], "\xE3\x80\x81", 3);   // 、
            _n += 3u;
        }
        for(const c8* _c = _parts[_k]; *_c != 0 && _n + 1u < sizeof(_say); _c++) {
            if(*_c != '-' && *_c != '.') {
                _say[_n++] = *_c;
            }
        }
    }
    _say[_n] = 0;
    if(_n == 0) {
        fude_utf8_put(_info.codepoint, _say);   // a letter (a Korean syllable): itself
    }
    if(_n == 0) {
        fude_utf8_put(fude_viewer_codepoint(_viewer), _say);   // a kana: itself
    }
    fude_speak(_say);
}

void fude_viewer_pointer_down(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    _viewer->swipe      = FUDE_VIEWER_SWIPE_UNKNOWN;
    _viewer->swipe_from = _viewer->swipe_at = _screen;
    _viewer->swipe_time = _time;
    _viewer->note_pressed = fude_viewer_inside(_screen, _viewer->note_min, _viewer->note_max) ||
                            fude_viewer_inside(_screen, _viewer->note_line_min, _viewer->note_line_max);
    _viewer->similar_pressed = -1;
    for(u32 _k = 0; _k < _viewer->similar_shown; _k++) {
        if(fude_viewer_inside(_screen, _viewer->similar_min[_k], _viewer->similar_max[_k])) {
            _viewer->similar_pressed = (i32)_k;
        }
    }
    _viewer->readings_pressed = fude_speech_available() && fude_viewer_inside(_screen, _viewer->readings_min, _viewer->readings_max);
    _viewer->sentence_pressed = fude_viewer_inside(_screen, _viewer->next_min, _viewer->next_max) ? 2u
                              : fude_speech_available() && fude_viewer_inside(_screen, _viewer->sentence_min, _viewer->sentence_max) ? 1u : 0u;
    for(u32 _k = 0; _k < _viewer->word_shown; _k++) {
        if(fude_viewer_inside(_screen, _viewer->word_min[_k], _viewer->word_max[_k])) {
            _viewer->sentence_pressed = (u8)(3u + _k);
        }
    }
    _viewer->add_pressed = fude_viewer_inside(_screen, _viewer->add_min, _viewer->add_max);
    _viewer->in_list     = !_viewer->add_pressed && fude_viewer_inside(_screen, _viewer->list_min, _viewer->list_max);
    if(_viewer->in_list) {
        fude_scroller_down(&_viewer->scroll, _screen, _time);
        _viewer->pressed = fude_viewer_row_at(_viewer, _screen);
    }
}

void fude_viewer_pointer_moved(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    // Moved as far as a drag: mostly sideways, a swipe — nothing under it is
    // pressed any more, and the words do not scroll.
    _viewer->swipe_at = _screen;
    const f32 _dx = _screen.x - _viewer->swipe_from.x;
    const f32 _dy = _screen.y - _viewer->swipe_from.y;
    if(_viewer->swipe == FUDE_VIEWER_SWIPE_UNKNOWN && _dx * _dx + _dy * _dy > FUDE_SCROLL_TAP_SLOP * FUDE_SCROLL_TAP_SLOP) {
        _viewer->swipe = fabsf(_dx) > 1.2f * fabsf(_dy) ? FUDE_VIEWER_SWIPE_YES : FUDE_VIEWER_SWIPE_NO;
        if(_viewer->swipe == FUDE_VIEWER_SWIPE_YES) {
            if(_viewer->in_list) {
                fude_scroller_stop(&_viewer->scroll);
            }
            _viewer->in_list          = false;
            _viewer->pressed          = -1;
            _viewer->add_pressed      = false;
            _viewer->readings_pressed = false;
            _viewer->note_pressed     = false;
            _viewer->similar_pressed  = -1;
            _viewer->sentence_pressed = 0u;
        }
    }
    if(_viewer->swipe == FUDE_VIEWER_SWIPE_YES) {
        return;
    }
    if(_viewer->in_list) {
        fude_scroller_moved(&_viewer->scroll, _screen, _time);
        if(_viewer->scroll.dragging) {
            _viewer->pressed = -1;
        }
    } else if(_viewer->add_pressed && !fude_viewer_inside(_screen, _viewer->add_min, _viewer->add_max)) {
        _viewer->add_pressed = false;   // slid off it
    }
    if(_viewer->readings_pressed && !fude_viewer_inside(_screen, _viewer->readings_min, _viewer->readings_max)) {
        _viewer->readings_pressed = false;
    }
    if(_viewer->note_pressed && !fude_viewer_inside(_screen, _viewer->note_min, _viewer->note_max) &&
       !fude_viewer_inside(_screen, _viewer->note_line_min, _viewer->note_line_max)) {
        _viewer->note_pressed = false;
    }
    if(_viewer->similar_pressed >= 0 &&
       !fude_viewer_inside(_screen, _viewer->similar_min[_viewer->similar_pressed], _viewer->similar_max[_viewer->similar_pressed])) {
        _viewer->similar_pressed = -1;   // slid off it
    }
    const u32 _word = _viewer->sentence_pressed >= 3u ? _viewer->sentence_pressed - 3u : UINT32_MAX;
    if((_viewer->sentence_pressed == 1u && !fude_viewer_inside(_screen, _viewer->sentence_min, _viewer->sentence_max)) ||
       (_viewer->sentence_pressed == 2u && !fude_viewer_inside(_screen, _viewer->next_min, _viewer->next_max)) ||
       (_word < _viewer->word_shown && !fude_viewer_inside(_screen, _viewer->word_min[_word], _viewer->word_max[_word]))) {
        _viewer->sentence_pressed = 0u;   // slid off it
    }
}

void fude_viewer_pointer_up(fude_viewer* _viewer, f64 _time) {
    // A swipe far enough (or quick enough): the next character to the left, the one before to the right.
    if(_viewer->swipe == FUDE_VIEWER_SWIPE_YES) {
        _viewer->swipe = FUDE_VIEWER_SWIPE_UNKNOWN;
        const f32 _dx = _viewer->swipe_at.x - _viewer->swipe_from.x;
        if(fabsf(_dx) >= FUDE_VIEWER_SWIPE || (fabsf(_dx) >= FUDE_VIEWER_FLICK && _time - _viewer->swipe_time <= FUDE_VIEWER_FLICK_TIME)) {
            if(_dx < 0.0f) { fude_viewer_next(_viewer); } else { fude_viewer_prev(_viewer); }
        }
        return;
    }
    _viewer->swipe = FUDE_VIEWER_SWIPE_UNKNOWN;
    if(_viewer->in_list) {
        fude_scroller_up(&_viewer->scroll, _time);
    }
    if(_viewer->add_pressed) {
        fude_viewer_set_adding(_viewer, true);
    }
    if(_viewer->readings_pressed) {
        fude_viewer_speak_readings(_viewer);
    }
    if(_viewer->sentence_pressed == 2u && _viewer->sentence_count > 0u) {
        _viewer->sentence_at = (_viewer->sentence_at + 1u) % _viewer->sentence_count;
    } else if(_viewer->sentence_pressed == 1u && _viewer->sentence_at < _viewer->sentence_count) {
        fude_kanji_sentence _s;
        if(fude_kanji_word_sentence(_viewer->db, _viewer->sentences[_viewer->sentence_at], &_s)) {
            fude_speak(_s.japanese);
        }
    } else if(_viewer->sentence_pressed >= 3u && _viewer->sentence_at < _viewer->sentence_count) {
        const u32           _k = _viewer->sentence_pressed - 3u < _viewer->word_shown ? _viewer->word_of[_viewer->sentence_pressed - 3u] : UINT32_MAX;
        fude_kanji_word     _w;
        fude_kanji_sentence _s = { "", "" };
        if(_k < _viewer->sentence_word_n[_viewer->sentence_at] && fude_kanji_word_at(_viewer->db, _viewer->sentence_words[_viewer->sentence_at][_k], &_w)) {
            fude_kanji_word_sentence(_viewer->db, _viewer->sentences[_viewer->sentence_at], &_s);
            fude_wordcard_ask_in(_w.written, _w.reading, _w.meaning, _s.japanese, _s.translation);   // into the vocabulary (with its sentence), or the saved one
        }
    }
    _viewer->sentence_pressed = 0u;
    if(_viewer->similar_pressed >= 0 && (u32)_viewer->similar_pressed < _viewer->similar_count) {
        u32 _record;
        if(fude_kanji_find_index(_viewer->db, _viewer->similar[_viewer->similar_pressed], &_record)) {
            fude_viewer_visit(_viewer, _record);   // a look-alike: there, and Prev back
        }
        _viewer->readings_pressed = false;
    }
    if(_viewer->note_pressed) {
        fude_wordcard_ask_note(fude_viewer_codepoint(_viewer));   // its note, to write
        _viewer->readings_pressed = false;
    }
    _viewer->note_pressed     = false;
    _viewer->similar_pressed  = -1;
    _viewer->readings_pressed = false;
    _viewer->in_list     = false;
    _viewer->add_pressed = false;
    _viewer->pressed     = -1;
}

void fude_viewer_set_adding(fude_viewer* _viewer, b8 _adding) {
    _viewer->adding      = _adding;
    _viewer->_tapped[0]  = 0;
}

void fude_viewer_update(fude_viewer* _viewer, f32 _dt) {
    if(!_viewer->open) {
        return;
    }
    fude_scroller_update(&_viewer->scroll, _dt, (f32)_viewer->row_count * _viewer->row_h, _viewer->list_max.y - _viewer->list_min.y);
    rde_vec_2F _at;
    if(!fude_scroller_take_tap(&_viewer->scroll, &_at)) {
        return;
    }
    const i32 _row = fude_viewer_row_at(_viewer, _at);
    if(_row < 0) {
        return;
    }
    const fude_viewer_row* _r = &_viewer->rows[_row];
    if(!_viewer->adding && _at.x >= _viewer->save_x0 && _at.x < _viewer->save_x0 + 40.0f) {
        fude_wordcard_ask(_r->written, _r->reading, _r->meaning, fude_viewer_codepoint(_viewer));   // into the vocabulary, or the saved one
        return;
    }
    if(!_viewer->adding && fude_speech_available() && _at.x >= _viewer->reading_x0 && _at.x < _viewer->reading_x1) {
        fude_speak(_r->reading);   // its reading: aloud
        return;
    }
    if(!_viewer->adding) {
        snprintf(_viewer->_tapped, sizeof(_viewer->_tapped), "%s", _r->written);   // to practise
        return;
    }
    // In Add: on or off the learner's words.
    const u32 _cp = fude_viewer_codepoint(_viewer);
    if(_r->ticked) {
        fude_vocab_remove(fude_vocab_find(_r->written, _r->reading));
    } else {
        fude_vocab_add(_r->written, _r->reading, _r->meaning, _cp);
    }
}

u32 fude_viewer_take_word(fude_viewer* _viewer, u32* _out, u32 _max) {
    if(_viewer->_tapped[0] == 0) {
        return 0;
    }
    u32       _n = 0;
    const c8* _p = _viewer->_tapped;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0 && _n < _max; _cp = fude_utf8_next(&_p)) {
        u32 _record;
        if(!fude_viewer_is_kanji(_cp) || !fude_kanji_find_index(_viewer->db, _cp, &_record)) {
            continue;
        }
        b8 _seen = false;
        for(u32 _i = 0; _i < _n && !_seen; _i++) {
            _seen = _out[_i] == _record;
        }
        if(!_seen) {
            _out[_n++] = _record;
        }
    }
    _viewer->_tapped[0] = 0;
    return _n;
}

void fude_viewer_show(fude_viewer* _viewer, const u32* _records, u32 _count, u32 _position) {
    if(!fude_viewer_available(_viewer) || _count == 0) {
        return;
    }

    rde_arr_clear(&_viewer->list);
    memcpy(rde_arr_add_n(&_viewer->list, _count), _records, (usize)_count * sizeof(u32));
    _viewer->position = _position < _count ? _position : 0u;
    _viewer->open     = true;
    _viewer->pressed  = -1;
    _viewer->adding   = false;
    _viewer->rows_for = UINT32_MAX;   // listed again
    fude_viewer_replay(_viewer);
}

void fude_viewer_visit(fude_viewer* _viewer, u32 _record) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(!_viewer->open || _n == 0u) {
        return;
    }
    u32* _list = (u32*)_viewer->list.memory;
    if(_viewer->position + 1u >= _n || _list[_viewer->position + 1u] != _record) {
        rde_arr_insert(&_viewer->list, _viewer->position + 1u, (any)&_record);
    }
    _viewer->position++;
    _viewer->adding = false;
    fude_viewer_replay(_viewer);
}

b8 fude_viewer_show_codepoint(fude_viewer* _viewer, u32 _codepoint) {
    for(u32 _i = 0; fude_viewer_available(_viewer) && _i < _viewer->db->count; _i++) {
        fude_kanji_info _info;
        if(fude_kanji_at(_viewer->db, _i, &_info) && _info.codepoint == _codepoint) {
            fude_viewer_show(_viewer, &_i, 1u, 0u);
            return true;
        }
    }
    return false;
}

void fude_viewer_close(fude_viewer* _viewer) {
    _viewer->open   = false;
    _viewer->adding = false;
}

void fude_viewer_next(fude_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + 1u) % _n;
        fude_viewer_replay(_viewer);
    }
}

void fude_viewer_prev(fude_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + _n - 1u) % _n;
        fude_viewer_replay(_viewer);
    }
}

// --- the page ----------------------------------------------------------------------

// The words' rows, _visible of them from _top down between _x0 and _right,
// scrolled; each written, reading and meaning — then › (tap: practise) or, in
// Add, a tick. The area is kept for the pointer.
RDE_INTERNAL void fude_viewer_draw_rows(fude_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _x0, f32 _right, f32 _top, u32 _visible,
                                        const c8* _empty) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _h     = FUDE_VIEWER_ROW;
    _viewer->row_h    = _h;
    _viewer->list_min = (rde_vec_2F){ _x0, _top - (f32)_visible * _h };
    _viewer->list_max = (rde_vec_2F){ _right, _top };
    if(_viewer->row_count == 0) {
        fude_draw_text(_font, _font_px, _empty, _x0 + 8.0f, _top - _h * 0.5f - 6.0f, 16.0f, _theme->text_soft);
        return;
    }

    // The columns: the widest written form and reading.
    c8        _line[256];
    const f32 _em        = fude_draw_text_width(_font, _font_px, "\xE3\x81\x82", 1.0f);   // a Japanese character (あ), per unit of size
    f32       _written_w = 2.0f * _em * FUDE_VIEWER_WORD_PX;
    f32       _reading_w = 3.0f * _em * FUDE_VIEWER_SMALL_PX;
    for(u32 _i = 0; _i < _viewer->row_count; _i++) {
        _written_w = fmaxf(_written_w, fude_draw_text_width(_font, _font_px, _viewer->rows[_i].written, FUDE_VIEWER_WORD_PX));
        _reading_w = fmaxf(_reading_w, fude_draw_text_width(_font, _font_px, _viewer->rows[_i].reading, FUDE_VIEWER_SMALL_PX));
    }
    _written_w = fminf(_written_w, 5.0f * _em * FUDE_VIEWER_WORD_PX);
    _reading_w = fminf(_reading_w, 8.0f * _em * FUDE_VIEWER_SMALL_PX);
    // With a voice, a speaker before each reading: a tap there says it.
    const b8  _speak     = fude_speech_available() && !_viewer->adding;
    const f32 _reading_x = _x0 + 12.0f + _written_w + (_speak ? 34.0f : 16.0f);
    const f32 _meaning_x = _reading_x + _reading_w + 16.0f;
    _viewer->reading_x0 = _speak ? _reading_x - 26.0f : 0.0f;
    _viewer->reading_x1 = _speak ? _meaning_x - 8.0f : 0.0f;
    // Out of Add, a bookmark before the › (filled: in the vocabulary): a tap there
    // opens the word card.
    const b8  _save      = !_viewer->adding;
    const f32 _end       = _save ? _right - 68.0f : _right - 40.0f;   // the meaning's end
    _viewer->save_x0     = _save ? _right - 64.0f : _right + 1.0f;

    const f32 _bottom = _top - (f32)_visible * _h;
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_x0 + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _x0), (u32)(_top - _bottom) });
    for(u32 _i = 0; _i < _viewer->row_count; _i++) {
        const fude_viewer_row* _r        = &_viewer->rows[_i];
        const f32              _row_top  = _top - (f32)_i * _h + _viewer->scroll.offset;
        if(_row_top - _h > _top || _row_top < _bottom) {
            continue;
        }
        const f32 _mid      = _row_top - _h * 0.5f;
        const f32 _baseline = _mid - FUDE_VIEWER_SMALL_PX * 0.35f;
        if(_viewer->pressed == (i32)_i) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_x0 + _right) * 0.5f, _mid }, (rde_vec_2F){ _right - _x0, _h }, _theme->select_fill);
        }
        if(_r->kind == FUDE_VIEWER_ROW_YOURS || _r->kind == FUDE_VIEWER_ROW_TYPED) {
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x0 + 2.0f, _mid }, (rde_vec_2F){ 4.0f, _h - 14.0f }, 1.0f, 4, _theme->accent, NULL);   // the learner's
        }
        fude_draw_text_whole(_font, _font_px, _r->written, _x0 + 12.0f, _mid, FUDE_VIEWER_WORD_PX, _written_w, 1u, _theme->ink);
        if(_speak) {
            fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _reading_x - 14.0f, _mid }, 14.0f, _theme->accent);
        }
        fude_draw_text_whole(_font, _font_px, _r->reading, _reading_x, _mid, FUDE_VIEWER_SMALL_PX, _reading_w, 1u, _theme->text_soft);
        // A long meaning: smaller, then on up to three lines — whole, never cut.
        fude_draw_text_whole(_font, _font_px, _r->meaning, _meaning_x, _mid, FUDE_VIEWER_SMALL_PX, _end - _meaning_x, 3u, _theme->text);
        if(_r->kind == FUDE_VIEWER_ROW_TO_ADD || _r->kind == FUDE_VIEWER_ROW_TYPED) {
            // A tick: on, the learner's; off, one to add.
            const rde_vec_2F _c = { _right - 20.0f, _mid };
            if(_r->ticked) {
                rde_rendering_2d_draw_circle(_c, 11.0f, 24, _theme->accent, NULL);
                fude_draw_line((rde_vec_2F){ _c.x - 5.0f, _c.y }, (rde_vec_2F){ _c.x - 1.5f, _c.y - 4.0f }, 1.4f, _theme->on_accent);
                fude_draw_line((rde_vec_2F){ _c.x - 1.5f, _c.y - 4.0f }, (rde_vec_2F){ _c.x + 5.5f, _c.y + 4.5f }, 1.4f, _theme->on_accent);
            } else {
                rde_rendering_2d_draw_circle_border(_c, 11.0f, 1.5f, 24, _theme->text_soft, NULL);
            }
        } else {
            fude_draw_icon(_font, _font_px, FUDE_ICON_NEXT, (rde_vec_2F){ _right - 12.0f, _mid }, 16.0f, _theme->text_soft);   // it opens
            if(fude_vocab_find(_r->written, _r->reading) != 0u) {
                fude_draw_icon_fill(FUDE_ICON_BOOKMARK, (rde_vec_2F){ _right - 44.0f, _mid }, 17.0f, _theme->accent);
            } else {
                fude_draw_icon(_font, _font_px, FUDE_ICON_BOOKMARK, (rde_vec_2F){ _right - 44.0f, _mid }, 17.0f, _theme->text_soft);
            }
        }
        if(_i + 1u < _viewer->row_count) {
            fude_draw_line((rde_vec_2F){ _x0, _row_top - _h }, (rde_vec_2F){ _right, _row_top - _h }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}

// The sentence card's text: the Japanese's width, and how many lines each takes
// (_tr_lines may be NULL).
RDE_INTERNAL f32 fude_viewer_sentence_text_w(f32 _width) {
    return _width - 2.0f * FUDE_VIEWER_CARD_PAD - (fude_speech_available() ? FUDE_VIEWER_SPEAKER_W : 0.0f);
}

// Sentence _i's words as chips — written, then the reading — in rows from _top
// between _x0 and _x1; drawn (and kept for the pointer) when _draw. How tall.
RDE_INTERNAL f32 fude_viewer_word_chips(fude_viewer* _viewer, rde_font* _font, f32 _font_px, u32 _i, f32 _x0, f32 _x1, f32 _top, b8 _draw) {
    const fude_theme* _theme = fude_theme_active();
    f32               _x     = _x0;
    u32               _rows  = 0;
    if(_draw) {
        _viewer->word_shown = 0;
    }
    for(u32 _k = 0; _k < _viewer->sentence_word_n[_i];) {
        fude_kanji_word _w;
        if(!fude_kanji_word_at(_viewer->db, _viewer->sentence_words[_i][_k], &_w)) {
            _k++;
            continue;
        }
        // The same word read more ways, one after another (wordsplit.h: 人 じん, にん,
        // ひと — the data cannot tell which): one chip, its readings joined. A tap
        // takes the one saved, else the commonest (the first).
        c8  _readings[96];
        snprintf(_readings, sizeof(_readings), "%s", _w.reading);
        b8  _saved = fude_vocab_find(_w.written, _w.reading) != 0u;
        u32 _pick  = _k;
        u32 _next  = _k + 1u;
        for(; _next < _viewer->sentence_word_n[_i]; _next++) {
            fude_kanji_word _o;
            if(!fude_kanji_word_at(_viewer->db, _viewer->sentence_words[_i][_next], &_o) || strcmp(_o.written, _w.written) != 0) {
                break;
            }
            const usize _len = strlen(_readings);
            snprintf(_readings + _len, sizeof(_readings) - _len, "・%s", _o.reading);
            if(!_saved && fude_vocab_find(_o.written, _o.reading) != 0u) {
                _saved = true;
                _pick  = _next;
            }
        }
        const f32 _ww = fude_draw_text_width(_font, _font_px, _w.written, 14.0f);
        const f32 _rw = fude_draw_text_width(_font, _font_px, _readings, 11.0f);
        const f32 _cw = fminf(_x1 - _x0, _ww + _rw + 34.0f);
        if(_rows == 0u || (_x > _x0 && _x + _cw > _x1)) {
            _x = _x0;
            _rows++;
        }
        if(_draw) {
            const f32  _cy    = _top - (f32)(_rows - 1u) * (FUDE_VIEWER_CHIP_H + FUDE_VIEWER_CHIP_GAP) - FUDE_VIEWER_CHIP_H * 0.5f;
            const b8   _down  = _viewer->sentence_pressed == 3u + _viewer->word_shown;
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _cw * 0.5f, _cy }, (rde_vec_2F){ _cw, FUDE_VIEWER_CHIP_H }, 1.0f, 8,
                                                    _down ? _theme->select_fill : _saved ? _theme->tint : _theme->surface_2, NULL);
            if(_saved) {
                fude_draw_icon_fill(FUDE_ICON_BOOKMARK, (rde_vec_2F){ _x + 11.0f, _cy }, 11.0f, _theme->accent);
            }
            fude_draw_text(_font, _font_px, _w.written, _x + 18.0f, _cy - 14.0f * 0.38f, 14.0f, _saved ? _theme->accent : _theme->ink);
            fude_draw_text(_font, _font_px, _readings, _x + 22.0f + _ww, _cy - 11.0f * 0.38f, 11.0f, _theme->text_soft);
            _viewer->word_of[_viewer->word_shown]  = (u8)_pick;
            _viewer->word_min[_viewer->word_shown] = (rde_vec_2F){ _x, _cy - FUDE_VIEWER_CHIP_H * 0.5f };
            _viewer->word_max[_viewer->word_shown] = (rde_vec_2F){ _x + _cw, _cy + FUDE_VIEWER_CHIP_H * 0.5f };
            _viewer->word_shown++;
        }
        _x += _cw + FUDE_VIEWER_CHIP_GAP;
        _k  = _next;
    }
    return _rows > 0u ? 6.0f + (f32)_rows * FUDE_VIEWER_CHIP_H + (f32)(_rows - 1u) * FUDE_VIEWER_CHIP_GAP : 0.0f;
}

// How tall the sentence card is, _width wide, with the gap above it: the one
// shown, or (_tallest) the tallest of the character's — the room kept, so ›
// does not move the page. 0: none.
RDE_INTERNAL f32 fude_viewer_sentence_height(fude_viewer* _viewer, rde_font* _font, f32 _font_px, f32 _width, b8 _tallest) {
    const f32 _text_w = fude_viewer_sentence_text_w(_width);
    f32       _body   = 0.0f;
    for(u32 _i = 0; _i < _viewer->sentence_count && _text_w > 40.0f; _i++) {
        fude_kanji_sentence _s;
        if((_tallest || _i == _viewer->sentence_at) && fude_kanji_word_sentence(_viewer->db, _viewer->sentences[_i], &_s)) {
            _body = fmaxf(_body, (f32)fude_draw_text_wrap_lines(_font, _font_px, _s.japanese, FUDE_VIEWER_SENT_PX, _text_w) * FUDE_VIEWER_SENT_LINE +
                                 (f32)fude_draw_text_wrap_lines(_font, _font_px, _s.translation, FUDE_VIEWER_TRANS_PX, _text_w) * FUDE_VIEWER_TRANS_LINE +
                                 fude_viewer_word_chips(_viewer, _font, _font_px, _i, 0.0f, _width - 2.0f * FUDE_VIEWER_CARD_PAD, 0.0f, false));
        }
    }
    return _body > 0.0f ? FUDE_VIEWER_SENT_GAP + FUDE_VIEWER_SENT_TITLE + 4.0f + _body + FUDE_VIEWER_CARD_PAD : 0.0f;
}

// The sentence card, its top at _top (under the gap): "Example", the word, ›
// when there are more; the Japanese behind a speaker, its translation under it.
RDE_INTERNAL void fude_viewer_draw_sentence(fude_viewer* _viewer, rde_font* _font, f32 _font_px, f32 _x0, f32 _right, f32 _top) {
    const f32 _height = fude_viewer_sentence_height(_viewer, _font, _font_px, _right - _x0, false) - FUDE_VIEWER_SENT_GAP;
    fude_kanji_word     _w;
    fude_kanji_sentence _s;
    if(_viewer->sentence_at >= _viewer->sentence_count || !fude_kanji_word_at(_viewer->db, _viewer->sentences[_viewer->sentence_at], &_w) ||
       !fude_kanji_word_sentence(_viewer->db, _viewer->sentences[_viewer->sentence_at], &_s)) {
        return;
    }
    const fude_theme* _theme  = fude_theme_active();
    const f32         _inner  = FUDE_VIEWER_CARD_PAD;
    const f32         _bottom = _top - _height;
    fude_draw_card((rde_vec_2F){ _x0, _bottom }, (rde_vec_2F){ _right, _top }, 14.0f, _theme->surface, _theme->outline);

    // The title: what it is and whose; › (with where it is) when there are more.
    const f32 _tmid  = _top - FUDE_VIEWER_SENT_TITLE * 0.5f;
    f32       _end   = _right - _inner;
    c8        _line[128];
    if(_viewer->sentence_count > 1u) {
        snprintf(_line, sizeof(_line), "%u / %u  " FUDE_ICON_NEXT, _viewer->sentence_at + 1u, _viewer->sentence_count);
        const f32 _w2    = fude_draw_text_width(_font, _font_px, _line, 13.0f) + 34.0f;
        const f32 _h     = 28.0f;
        _viewer->next_min = (rde_vec_2F){ _end - _w2, _tmid - _h * 0.5f };
        _viewer->next_max = (rde_vec_2F){ _end, _tmid + _h * 0.5f };
        const b8 _down = _viewer->sentence_pressed == 2u;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _end - _w2 * 0.5f, _tmid }, (rde_vec_2F){ _w2, _h }, 1.0f, 8, _down ? _theme->accent : _theme->tint, NULL);
        fude_draw_text(_font, _font_px, _line, _viewer->next_min.x + 14.0f, _tmid - 13.0f * 0.42f, 13.0f, _down ? _theme->on_accent : _theme->accent);
        _end = _viewer->next_min.x - 10.0f;
    }
    const c8* _title = fude_text(FUDE_TEXT_EXAMPLE);
    fude_draw_text(_font, _font_px, _title, _x0 + _inner, _tmid - 16.0f * 0.42f, 16.0f, _theme->text);
    const f32 _wx = _x0 + _inner + fude_draw_text_width(_font, _font_px, _title, 16.0f) + 12.0f;
    snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _w.written, _w.reading);   // ·
    const f32 _wpx = fude_draw_text_px_to_fit(_font, _font_px, _line, 13.0f, _end - _wx, 0.7f);
    fude_draw_text(_font, _font_px, _line, _wx, _tmid - _wpx * 0.42f, _wpx, _theme->text_soft);
    fude_draw_line((rde_vec_2F){ _x0, _top - FUDE_VIEWER_SENT_TITLE }, (rde_vec_2F){ _right, _top - FUDE_VIEWER_SENT_TITLE }, 0.5f, _theme->outline);

    // The sentence, then its translation; a tap on them reads the sentence aloud.
    const b8  _speak  = fude_speech_available();
    const f32 _tx     = _x0 + _inner + (_speak ? FUDE_VIEWER_SPEAKER_W : 0.0f);
    const f32 _text_w = fude_viewer_sentence_text_w(_right - _x0);
    const f32 _body   = _top - FUDE_VIEWER_SENT_TITLE - 4.0f;   // the first line's top
    const f32 _lines_h = (f32)fude_draw_text_wrap_lines(_font, _font_px, _s.japanese, FUDE_VIEWER_SENT_PX, _text_w) * FUDE_VIEWER_SENT_LINE +
                         (f32)fude_draw_text_wrap_lines(_font, _font_px, _s.translation, FUDE_VIEWER_TRANS_PX, _text_w) * FUDE_VIEWER_TRANS_LINE;
    if(_speak) {
        _viewer->sentence_min = (rde_vec_2F){ _x0, _body - _lines_h - 2.0f };
        _viewer->sentence_max = (rde_vec_2F){ _right, _top - FUDE_VIEWER_SENT_TITLE };
        if(_viewer->sentence_pressed == 1u) {
            fude_draw_card((rde_vec_2F){ _x0 + 4.0f, _viewer->sentence_min.y }, (rde_vec_2F){ _right - 4.0f, _viewer->sentence_max.y - 2.0f }, 10.0f, _theme->select_fill,
                           _theme->select_fill);
        }
        fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _x0 + _inner + 10.0f, _body - FUDE_VIEWER_SENT_LINE * 0.5f }, 16.0f, _theme->accent);
    }
    const u32 _lines = fude_draw_text_wrap(_font, _font_px, _s.japanese, _tx, _body - FUDE_VIEWER_SENT_LINE * 0.5f - FUDE_VIEWER_SENT_PX * 0.38f, FUDE_VIEWER_SENT_PX,
                                           _text_w, FUDE_VIEWER_SENT_LINE, _theme->ink);
    const f32 _ty    = _body - (f32)_lines * FUDE_VIEWER_SENT_LINE;
    fude_draw_text_wrap(_font, _font_px, _s.translation, _tx, _ty - FUDE_VIEWER_TRANS_LINE * 0.5f - FUDE_VIEWER_TRANS_PX * 0.38f, FUDE_VIEWER_TRANS_PX, _text_w,
                        FUDE_VIEWER_TRANS_LINE, _theme->text_soft);
    // Its words, each with its reading: a tap, the word card.
    fude_viewer_word_chips(_viewer, _font, _font_px, _viewer->sentence_at, _x0 + _inner, _right - _inner, _body - _lines_h - 6.0f, true);
}

// Add: the character small, what to do, and every word to pick from.
RDE_INTERNAL void fude_viewer_render_adding(fude_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, const fude_kanji_info* _info,
                                            f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _size  = fminf(170.0f, (_right - _left) * 0.3f);
    const rde_vec_2F  _tl    = { _left, _top - 48.0f };
    fude_glyph_box(_tl, _size);
    fude_glyph_writing(&_viewer->glyph, _info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 13.0f);

    const f32 _tx = _left + _size + 24.0f;
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_VIEWER_ADD_WORDS), _tx, _tl.y - 30.0f, 20.0f, _theme->text);
    c8 _line[160];
    FUDE_TEXTF(_line, FUDE_TEXT_VIEWER_YOURS_N, FUDE_TN(fude_vocab_kanji_count(_info->codepoint)));
    fude_draw_text(_font, _font_px, _line, _tx, _tl.y - 62.0f, 17.0f, _theme->text_soft);
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_VIEWER_ADD_HELP), _tx, _tl.y - 96.0f, 13.0f, _theme->text_soft);
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_VIEWER_ADD_OTHER), _tx, _tl.y - 122.0f, 13.0f, _theme->text_soft);

    const f32 _list_top = _tl.y - _size - 20.0f;
    fude_draw_line((rde_vec_2F){ _left, _list_top }, (rde_vec_2F){ _right, _list_top }, 0.5f, _theme->line);
    const u32 _visible = (u32)fmaxf(1.0f, floorf((_list_top - _bottom) / FUDE_VIEWER_ROW));
    fude_viewer_draw_rows(_viewer, _window, _font, _font_px, _left, _right, _list_top, _visible, fude_text(FUDE_TEXT_VIEWER_NO_MORE_WORDS));
}

void fude_viewer_render(fude_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _bottom_bar) {
    _viewer->list_min = _viewer->list_max = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->add_min  = _viewer->add_max  = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->sentence_min = _viewer->sentence_max = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->word_shown   = 0;
    _viewer->note_min      = _viewer->note_max      = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->note_line_min = _viewer->note_line_max = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->next_min     = _viewer->next_max     = (rde_vec_2F){ 0.0f, 0.0f };
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0) {
        return;
    }

    fude_kanji_info _info;
    const u32 _record = ((const u32*)_viewer->list.memory)[_viewer->position];
    if(!fude_kanji_at(_viewer->db, _record, &_info)) {
        return;
    }
    fude_viewer_list_rows(_viewer, _record, _info.codepoint);
    fude_viewer_list_sentences(_viewer, _record, _info.codepoint);
    fude_viewer_list_similar(_viewer, _record, _info.codepoint);
    _viewer->similar_shown = 0;

    const rde_vec_2I _size_px = rde_window_get_size(_window);
    const rde_vec_4I _insets  = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32 _hw     = (f32)_size_px.x * 0.5f;
    const f32 _hh     = (f32)_size_px.y * 0.5f;
    const f32 _top    = _hh - (f32)_insets.y - FUDE_VIEWER_MARGIN;
    const f32 _bottom = -_hh + (f32)_insets.w + _bottom_bar + FUDE_VIEWER_MARGIN;
    const f32 _left   = -_hw + (f32)_insets.x + FUDE_VIEWER_MARGIN;
    const f32 _right  = _hw - (f32)_insets.z - FUDE_VIEWER_MARGIN;

    // --- header: what it is, as chips; where it is in the list at the right ---------
    const b8          _kana  = !fude_viewer_is_kanji(_info.codepoint);
    // A letter with words of its own (Korean's syllables: 한 → 한국, 한글) shows as a
    // character does — its Latin name for a meaning, no readings; the others
    // (kana, jamo) alone.
    const b8          _letter = _kana && fude_viewer_has_words(_viewer, _record, _info.codepoint);
    const b8          _plain  = _kana && !_letter;
    const u32         _kinds  = _letter ? 0u : fude_lang_reading_kinds();
    const fude_theme* _theme = fude_theme_active();
    c8 _line[256];
    {
        const u8  _group   = fude_lang_group(_info.codepoint);
        const f32 _mid     = _top - 14.0f;
        // At the right: where it is in the list, and Note before it.
        c8 _pos[32];
        snprintf(_pos, sizeof(_pos), "%u / %u", _viewer->position + 1u, (u32)rde_arr_length(&_viewer->list));
        const f32 _pos_w = fude_draw_text_width(_font, _font_px, _pos, 14.0f);
        fude_draw_text(_font, _font_px, _pos, _right - _pos_w, _mid - 14.0f * 0.42f, 14.0f, _theme->text_soft);
        f32 _chips_end = _right - _pos_w - 16.0f;   // the chips stop short of them
        // Note: the learner's own, written in the word card.
        if(!_viewer->adding) {
            c8 _note[64];
            snprintf(_note, sizeof(_note), FUDE_ICON_NOTE_EDIT " %s", fude_text(FUDE_TEXT_NOTE));
            const f32 _nw = fude_draw_text_width(_font, _font_px, _note, 12.0f) + 24.0f;
            const f32 _nx = _right - _pos_w - 16.0f - _nw;
            const b8  _has = fude_charnote_get(_info.codepoint)[0] != 0;
            _viewer->note_min = (rde_vec_2F){ _nx, _mid - 14.0f };
            _viewer->note_max = (rde_vec_2F){ _nx + _nw, _mid + 14.0f };
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _nx + _nw * 0.5f, _mid }, (rde_vec_2F){ _nw, 28.0f }, 1.0f, 8,
                                                    _viewer->note_pressed ? _theme->accent : _has ? _theme->tint : _theme->surface_2, NULL);
            fude_draw_text(_font, _font_px, _note, _nx + 12.0f, _mid - 12.0f * 0.42f, 12.0f, _viewer->note_pressed ? _theme->on_accent : _theme->accent);
            _chips_end = _nx - 8.0f;
        }
        // The chips, as many as fit before them (a phone: the grade, then the
        // strokes, are left out). Its group (one outside them all, as the
        // language names it: 々 a kanji), its level, its strokes, its grade.
        c8         _chip[4][96];
        rde_color  _fill[4], _ink[4];
        u32        _n = 0;
        snprintf(_chip[_n], sizeof(_chip[_n]), "%s", fude_text((FUDE_TEXT_)fude_lang_group_name(_group)));
        _fill[_n] = _theme->surface_2; _ink[_n] = _theme->text; _n++;
        if(_info.level != 0) {
            fude_lang_level_name(_info.level, false, _chip[_n], sizeof(_chip[_n]));
            _fill[_n] = _theme->accent; _ink[_n] = _theme->on_accent; _n++;
        }
        FUDE_TEXTF(_chip[_n], FUDE_TEXT_STROKES_N, FUDE_TN(_info.strokes));
        _fill[_n] = _theme->surface_2; _ink[_n] = _theme->text; _n++;
        const c8* _grade = NULL;
        if(_info.grade >= 1 && _info.grade <= 6)       { FUDE_TEXTF(_line, FUDE_TEXT_GRADE_N, FUDE_TN(_info.grade)); _grade = _line; }
        else if(_info.grade == 8)                      { _grade = fude_text(FUDE_TEXT_GRADE_SECONDARY); }
        else if(_info.grade == 9 || _info.grade == 10) { _grade = fude_text(FUDE_TEXT_GRADE_NAMES); }
        if(_grade != NULL) {
            snprintf(_chip[_n], sizeof(_chip[_n]), "%s", _grade);
            _fill[_n] = _theme->surface_2; _ink[_n] = _theme->text; _n++;
        }
        f32 _x = _left;
        for(u32 _i = 0; _i < _n; _i++) {
            const f32 _w = fude_draw_text_width(_font, _font_px, _chip[_i], FUDE_VIEWER_CHIP_PX) + 22.0f;   // fude_draw_chip's
            if(_i > 0u && _x + _w > _chips_end) {
                break;
            }
            _x += fude_draw_chip(_font, _font_px, _chip[_i], _x, _mid, FUDE_VIEWER_CHIP_PX, _fill[_i], _ink[_i]) + 6.0f;
        }
    }

    if(_viewer->adding && !_plain) {
        fude_viewer_render_adding(_viewer, _window, _font, _font_px, &_info, _left, _right, _top, _bottom);
        return;
    }

    // The parts it is built from, those that can be drawn.
    u32 _parts[FUDE_KANJI_MAX_PARTS];
    u32 _part_count = 0;
    if(!_plain) {
        u32 _all[FUDE_KANJI_MAX_PARTS];
        const u32 _n = fude_kanji_parts(_viewer->db, _record, _all, FUDE_KANJI_MAX_PARTS);
        for(u32 _i = 0; _i < _n; _i++) {
            u32 _index;
            if(fude_kanji_find_index(_viewer->db, _all[_i], &_index)) {
                _parts[_part_count++] = _all[_i];
            }
        }
    }

    // --- the character, and where its details go --------------------------------------
    // The words show as many rows as fit (they scroll); at least one, for "none yet".
    const u32 _rows    = _viewer->row_count > 0u ? _viewer->row_count : 1u;
    u32       _visible = _plain ? 0u : (_rows < 6u ? _rows : 6u);
    const b8  _similar = _viewer->similar_count > 0u;
    const c8* _note    = fude_charnote_get(_info.codepoint);
    const b8  _noted   = !_plain && _note[0] != 0;
    const b8  _wide    = !_plain && (_right - _left) > (_top - _bottom) * 1.25f;
    // 部 and 似 share a line when the column (beside the character: under 55%
    // of the width) has room for both.
    const f32 _parts_w = _part_count > 0 ? 46.0f + (f32)_part_count * FUDE_VIEWER_KANA_SIZE * 1.3f : 0.0f;
    const f32 _sim_w   = 46.0f + (f32)_viewer->similar_count * FUDE_VIEWER_KANA_SIZE * 1.6f;
    const f32 _column  = _wide ? (_right - _left) * 0.55f - FUDE_VIEWER_GUTTER : _right - _left;
    const b8  _shared  = !_plain && _similar && _part_count > 0 && _parts_w + 24.0f + _sim_w <= _column;
    const u32 _lines   = 1u + _kinds + (_part_count > 0 ? 1u : 0u);   // meaning, each reading's kind (On, Kun), parts
    const f32 _details = (_plain ? 0.0f : (f32)_lines * FUDE_VIEWER_LINE) + (_similar && !_shared ? FUDE_VIEWER_LINE : 0.0f) +
                         (_noted ? FUDE_VIEWER_LINE : 0.0f);   // ...; 似 (beside the parts, or its own); 記
    f32        _size;
    rde_vec_2F _tl;
    f32        _x0;   // the details' left
    f32        _y0;   // and top
    f32        _sent_h = 0.0f;   // the sentence card, under the words (0: none)
    if(_wide) {
        // Beside it: the character as tall as the screen allows, up to under half
        // its width; the words as many rows as the column holds.
        _size = fmaxf(120.0f, fminf(_top - 48.0f - _bottom, (_right - _left) * 0.45f));
        _tl   = (rde_vec_2F){ _left, _top - 48.0f };
        _x0   = _left + _size + FUDE_VIEWER_GUTTER;
        _y0   = _tl.y - 8.0f;
        // The sentence shown's height (beside the character, › changes only how
        // many words show); the words down to two rows for it.
        _sent_h = _plain ? 0.0f : fude_viewer_sentence_height(_viewer, _font, _font_px, _right - _x0, false);
        f32 _fit = floorf((_y0 - _details - 8.0f - FUDE_VIEWER_TITLE - FUDE_VIEWER_CARD_PAD - _sent_h - _bottom) / FUDE_VIEWER_ROW);
        if(_sent_h > 0.0f && _fit < fminf(2.0f, (f32)_rows)) {
            _sent_h = 0.0f;   // a short column: the words before the sentence
            _fit    = floorf((_y0 - _details - 8.0f - FUDE_VIEWER_TITLE - FUDE_VIEWER_CARD_PAD - _bottom) / FUDE_VIEWER_ROW);
        }
        _visible = _plain || _fit < 1.0f ? 0u : (u32)fminf((f32)_rows, _fit);
    } else {
        // Under it: rows go, down to three (two, to make room for a sentence),
        // before the character gets small; then the sentence goes. The room kept
        // for the sentence is the tallest shown for this character so far: ›
        // moves the character once at most, never back and forth.
        const f32 _room = (_top - 48.0f) - (_bottom + _details);
        // A phone has less room for all of it: the character smaller before the
        // words go under two rows or the example sentence goes.
        const b8  _phone         = fude_kit_compact(_window);
        const f32 _min_char      = _phone ? 180.0f : FUDE_VIEWER_MIN_CHAR;
        const f32 _min_char_sent = _phone ? 115.0f : FUDE_VIEWER_MIN_CHAR_SENT;
        #define FUDE_VIEWER_WORDS_H(n) ((n) > 0u ? 8.0f + FUDE_VIEWER_TITLE + (f32)(n) * FUDE_VIEWER_ROW + FUDE_VIEWER_CARD_PAD : 0.0f)
        _viewer->sentence_room = _plain ? 0.0f : fmaxf(_viewer->sentence_room, fude_viewer_sentence_height(_viewer, _font, _font_px, _right - _left, false));
        _sent_h = _viewer->sentence_room;
        u32 _fits = _visible;
        while(_fits > 2u && _room - _sent_h - FUDE_VIEWER_WORDS_H(_fits) < _min_char) {
            _fits--;
        }
        if(_sent_h > 0.0f && _room - _sent_h - FUDE_VIEWER_WORDS_H(_fits) < _min_char_sent) {
            _sent_h = 0.0f;
            _fits   = _visible;
            while(_fits > 3u && _room - FUDE_VIEWER_WORDS_H(_fits) < _min_char) {
                _fits--;
            }
        }
        _visible = _fits;
        if(_room - _sent_h - FUDE_VIEWER_WORDS_H(_visible) < (_phone ? 110.0f : 160.0f)) {
            _visible = 0;   // a small screen: the character first
            _sent_h  = 0.0f;
        }
        _size = fmaxf(120.0f, fminf(_right - _left, _room - _sent_h - FUDE_VIEWER_WORDS_H(_visible)));
        #undef FUDE_VIEWER_WORDS_H
        _tl   = (rde_vec_2F){ -_size * 0.5f, _top - 48.0f };
        _x0   = _left;
        _y0   = _tl.y - _size - 20.0f;
    }

    fude_glyph_box(_tl, _size);
    fude_glyph_writing(&_viewer->glyph, &_info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 18.0f);
    fude_selection_draw_mark(fude_marks_get(_info.codepoint), _tl, _size, false);
    // 似: the look-alikes (a kana's under it alone; a kanji's after its parts, below).
    const u32 _similar_line = _plain ? 0u : _shared ? 1u + _kinds : 1u + _kinds + (_part_count > 0 ? 1u : 0u);
    if(_similar) {
        const f32 _ly  = _y0 - (f32)_similar_line * FUDE_VIEWER_LINE;   // the line's top
        const f32 _mid = _ly - FUDE_VIEWER_LINE * 0.5f;
        const f32 _box = FUDE_HEADER_LABEL;
        const f32 _lx  = _shared ? _x0 + _parts_w + 24.0f : _x0;   // beside the parts, or at the start of its own line
        fude_header_label(&_viewer->glyph, fude_lang_badge(FUDE_LANG_BADGE_LOOKALIKES), _lx, _mid);   // 似
        f32 _x = _lx + _box + 14.0f;
        for(u32 _i = 0; _i < _viewer->similar_count && _x + FUDE_VIEWER_KANA_SIZE <= _right; _i++) {
            const f32 _s = FUDE_VIEWER_KANA_SIZE;
            if(_viewer->similar_pressed == (i32)_i) {
                rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _s * 0.5f, _mid }, (rde_vec_2F){ _s + 12.0f, _s + 10.0f }, 1.0f, 8, _theme->select_fill, NULL);
            }
            fude_glyph_character(&_viewer->glyph, _viewer->similar[_i], (rde_vec_2F){ _x, _mid + _s * 0.5f }, _s, _i < _viewer->similar_mine ? _theme->score_poor : _theme->ink);
            _viewer->similar_min[_viewer->similar_shown] = (rde_vec_2F){ _x - 8.0f, _mid - _s * 0.5f - 6.0f };
            _viewer->similar_max[_viewer->similar_shown] = (rde_vec_2F){ _x + _s + 8.0f, _mid + _s * 0.5f + 6.0f };
            _viewer->similar_shown++;
            _x += _s * 1.6f;
        }
    }
    // 記: the learner's note (a kanji's), under 似.
    if(_noted) {
        const f32 _ly  = _y0 - (f32)(1u + _kinds + (_part_count > 0 ? 1u : 0u) + (_similar && !_shared ? 1u : 0u)) * FUDE_VIEWER_LINE;
        const f32 _mid = _ly - FUDE_VIEWER_LINE * 0.5f;
        const f32 _box = FUDE_HEADER_LABEL;
        fude_header_label(&_viewer->glyph, fude_lang_badge(FUDE_LANG_BADGE_NOTE), _x0, _mid);   // 記
        // Two lines at most: smaller when it needs more.
        const f32 _tx = _x0 + _box + 14.0f;
        const f32 _tw = _right - _tx;
        f32       _px = 14.0f;
        while(_px > 10.0f && fude_draw_text_wrap_lines(_font, _font_px, _note, _px, _tw) > 2u) {
            _px -= 1.0f;
        }
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _note, _px, _tw);
        const f32 _lh    = _px * 1.3f;
        if(_viewer->note_pressed) {
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_tx + _right) * 0.5f, _mid }, (rde_vec_2F){ _tw + 8.0f, FUDE_VIEWER_LINE - 4.0f }, 1.0f, 8, _theme->select_fill, NULL);
        }
        rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_tx + _right) * 0.5f), (i32)_mid }, (rde_vec_2UI){ (u32)_tw + 4u, (u32)FUDE_VIEWER_LINE });
        fude_draw_text_wrap(_font, _font_px, _note, _tx, _mid + (f32)((_lines < 2u ? _lines : 2u) - 1u) * _lh * 0.5f - _px * 0.38f, _px, _tw, _lh, _theme->text);
        rde_rendering_end_clipping_rect();
        _viewer->note_line_min = (rde_vec_2F){ _x0, _ly - FUDE_VIEWER_LINE };
        _viewer->note_line_max = (rde_vec_2F){ _right, _ly };
    }
    if(_plain) {
        return;
    }

    // --- meaning, readings, parts -------------------------------------------------------
    // The meaning first, as the title; then each reading and the parts behind a
    // label that is one of the language's characters (lang.h's badges: 音, 訓, 部), written from its strokes.
    // A letter's is its Latin name, and what it stands for when its data says
    // (Thai's ก: ko kai · chicken).
    const c8* _meanings = fude_kanji_meanings(_viewer->db, &_info);
    c8        _named[192];
    if(_letter && _meanings[0] != 0) {
        snprintf(_named, sizeof(_named), "%s \xC2\xB7 %s", fude_lang_latin(_info.codepoint), _meanings);
        _meanings = _named;
    } else if(_letter) {
        _meanings = fude_lang_latin(_info.codepoint);
    }
    fude_draw_text_whole(_font, _font_px, _meanings[0] != 0 ? _meanings : fude_text(FUDE_TEXT_NO_MEANING), _x0, _y0 - FUDE_VIEWER_LINE * 0.5f - FUDE_VIEWER_MEANING_PX * 0.04f,
                         FUDE_VIEWER_MEANING_PX, _right - _x0, 1u, _theme->text);

    // With a voice: the reading lines say themselves when tapped (a speaker at their
    // end); a letter's line, the letter.
    if(fude_speech_available()) {
        const u32 _from = _letter ? 0u : 1u;
        _viewer->readings_min = (rde_vec_2F){ _x0, _y0 - (f32)(_from + (_letter ? 1u : _kinds)) * FUDE_VIEWER_LINE };
        _viewer->readings_max = (rde_vec_2F){ _right, _y0 - (f32)_from * FUDE_VIEWER_LINE };
        fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _right - 12.0f, _y0 - ((f32)_from + 0.5f) * FUDE_VIEWER_LINE }, 18.0f, _theme->accent);
    } else {
        _viewer->readings_min = _viewer->readings_max = (rde_vec_2F){ 0.0f, 0.0f };
    }
    for(u32 _k = 0; _k < _kinds + (_part_count > 0 ? 1u : 0u); _k++) {
        const f32 _ly = _y0 - (f32)(_k + 1u) * FUDE_VIEWER_LINE;   // the line's top
        const f32 _mid = _ly - FUDE_VIEWER_LINE * 0.5f;
        const f32 _box = FUDE_HEADER_LABEL;
        fude_header_label(&_viewer->glyph, fude_lang_badge(_k < _kinds ? (FUDE_LANG_BADGE_)(FUDE_LANG_BADGE_READING_0 + _k) : FUDE_LANG_BADGE_PARTS), _x0, _mid);   // 音 訓 部
        const f32 _tx = _x0 + _box + 14.0f;
        if(_k < _kinds) {
            fude_glyph_reading(&_viewer->glyph, fude_kanji_reading(_viewer->db, &_info, _k),
                               (rde_vec_2F){ _tx, _mid + FUDE_VIEWER_KANA_SIZE * 0.5f }, FUDE_VIEWER_KANA_SIZE,
                               fude_speech_available() ? _right - 32.0f : _right, _theme->ink, _theme->text_soft);   // clear of the speaker
        } else {
            f32 _x = _tx;
            for(u32 _i = 0; _i < _part_count && _x + FUDE_VIEWER_KANA_SIZE <= _right; _i++) {
                fude_glyph_character(&_viewer->glyph, _parts[_i], (rde_vec_2F){ _x, _mid + FUDE_VIEWER_KANA_SIZE * 0.5f }, FUDE_VIEWER_KANA_SIZE, _theme->ink);
                _x += FUDE_VIEWER_KANA_SIZE * 1.3f;
            }
        }
    }

    // --- words: the learner's, then the examples, in a card; Add by the title ----------
    if(_visible == 0) {
        return;
    }
    const f32 _wy    = _y0 - _details - 8.0f;                              // the card's top
    const f32 _inner = FUDE_VIEWER_CARD_PAD;
    const f32 _cb    = _wy - FUDE_VIEWER_TITLE - (f32)_visible * FUDE_VIEWER_ROW - _inner;   // its bottom
    fude_draw_card((rde_vec_2F){ _x0, _cb }, (rde_vec_2F){ _right, _wy }, 14.0f, _theme->surface, _theme->outline);
    const f32 _tmid = _wy - FUDE_VIEWER_TITLE * 0.5f;
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_WORDS), _x0 + _inner, _tmid - 16.0f * 0.42f, 16.0f, _theme->text);
    {
        const u32 _yours = fude_vocab_kanji_count(_info.codepoint);
        const u32 _all   = _viewer->row_count >= _yours ? _viewer->row_count - _yours : 0u;
        FUDE_TEXTF(_line, FUDE_TEXT_WORDS_EXAMPLES, FUDE_TN(_all));
        if(_yours > 0) {
            c8 _examples[96];
            snprintf(_examples, sizeof(_examples), "%s", _line);
            FUDE_TEXTF(_line, FUDE_TEXT_WORDS_YOURS, FUDE_TN(_yours));
            snprintf(_line + strlen(_line), sizeof(_line) - strlen(_line), " \xC2\xB7 %s", _examples);   // ·
        }
        fude_draw_text(_font, _font_px, _line, _x0 + _inner + fude_draw_text_width(_font, _font_px, fude_text(FUDE_TEXT_WORDS), 16.0f) + 12.0f, _tmid - 12.0f * 0.42f, 12.0f,
                       _theme->text_soft);
    }
    {
        c8 _label[64];
        snprintf(_label, sizeof(_label), FUDE_ICON_PLUS " %s", fude_text(FUDE_TEXT_ADD));
        const f32 _w     = fude_draw_text_width(_font, _font_px, _label, 13.0f) + 28.0f;
        const f32 _h     = 28.0f;
        _viewer->add_min = (rde_vec_2F){ _right - _inner - _w, _tmid - _h * 0.5f };
        _viewer->add_max = (rde_vec_2F){ _right - _inner, _tmid + _h * 0.5f };
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_viewer->add_min.x + _viewer->add_max.x) * 0.5f, _tmid }, (rde_vec_2F){ _w, _h }, 1.0f, 8,
                                                _viewer->add_pressed ? _theme->accent : _theme->tint, NULL);
        fude_draw_text(_font, _font_px, _label, _viewer->add_min.x + 14.0f, _tmid - 13.0f * 0.42f, 13.0f,
                       _viewer->add_pressed ? _theme->on_accent : _theme->accent);
    }
    fude_draw_line((rde_vec_2F){ _x0, _wy - FUDE_VIEWER_TITLE }, (rde_vec_2F){ _right, _wy - FUDE_VIEWER_TITLE }, 0.5f, _theme->outline);
    fude_viewer_draw_rows(_viewer, _window, _font, _font_px, _x0 + _inner, _right - _inner, _wy - FUDE_VIEWER_TITLE, _visible, fude_text(FUDE_TEXT_NO_WORDS));
    if(_sent_h > 0.0f) {
        fude_viewer_draw_sentence(_viewer, _font, _font_px, _x0, _right, _cb - FUDE_VIEWER_SENT_GAP);
    }
}

// --- the screen (screen.h): its rows, and what they do ------------------------------------

#include "study/app/study.h"
#include "study/models/marks.h"

FUDE_SCREEN_ADAPTERS(fude_viewer, fude_viewer)

RDE_INTERNAL void fude_viewer_screen_resume(void* _self) {
    fude_viewer_replay((fude_viewer*)_self);   // back from Practice: it writes itself again
}

RDE_INTERNAL void fude_viewer_screen_render(void* _self, const fude_screen_frame* _f) {
    fude_viewer_render((fude_viewer*)_self, _f->window, _f->font, _f->font_px, _f->row_h + 16.0f);
}

// A tapped example word practises its kanji; the arrows and Space, on a keyboard.
RDE_INTERNAL void fude_viewer_screen_update(fude_app* _app, void* _self, f32 _dt) {
    fude_viewer* _viewer = (fude_viewer*)_self;
    fude_viewer_update(_viewer, _dt);
    u32       _kanji[FUDE_VIEWER_WORD_KANJI];
    const u32 _n = fude_viewer_take_word(_viewer, _kanji, FUDE_VIEWER_WORD_KANJI);
    fude_study_practice_set(_app, _kanji, _n);
    if(rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_RIGHT)) { fude_viewer_next(_viewer); }
    if(rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_LEFT))  { fude_viewer_prev(_viewer); }
    if(rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_SPACE)) { fude_viewer_replay(_viewer); }
}

// The character shown (false: none).
RDE_INTERNAL b8 fude_viewer_record(const fude_viewer* _viewer, u32* _record) {
    if(rde_arr_length(&_viewer->list) == 0) {
        return false;
    }
    *_record = ((const u32*)_viewer->list.memory)[_viewer->position];
    return true;
}

FUDE_ROW_CALL(fude_viewer_row_back,   fude_viewer, fude_viewer_close)
FUDE_ROW_CALL(fude_viewer_row_prev,   fude_viewer, fude_viewer_prev)
FUDE_ROW_CALL(fude_viewer_row_replay, fude_viewer, fude_viewer_replay)
FUDE_ROW_CALL(fude_viewer_row_next,   fude_viewer, fude_viewer_next)

// Study: none → Studying → Known → none, for the character shown.
RDE_INTERNAL void fude_viewer_row_study(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    const u32 _cp = fude_viewer_codepoint((const fude_viewer*)_self);
    if(_cp != 0u) {
        const FUDE_MARK_ _mark = fude_marks_get(_cp);
        fude_marks_set(_cp, _mark == FUDE_MARK_NONE ? FUDE_MARK_STUDYING : _mark == FUDE_MARK_STUDYING ? FUDE_MARK_KNOWN : FUDE_MARK_NONE);
    }
}

// A practice sheet of the character on screen (sheet.h).
RDE_INTERNAL void fude_viewer_row_sheet(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32 _record;
    if(fude_viewer_record((const fude_viewer*)_self, &_record)) {
        fude_study_sheet(_app, &_record, 1u);
    }
}

RDE_INTERNAL void fude_viewer_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32 _record;
    if(fude_viewer_record((const fude_viewer*)_self, &_record)) {
        fude_study_practice(_app, &_record, 1u);
    }
}

// Add (viewer.h): Done, or a word of the learner's own.
RDE_INTERNAL void fude_viewer_row_add_done(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_viewer_set_adding((fude_viewer*)_self, false);
}

RDE_INTERNAL void fude_viewer_row_add_type(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_wordcard_ask_typed(fude_viewer_codepoint((const fude_viewer*)_self));
}

enum { FUDE_VIEWER_ROW_MAIN = 0, FUDE_VIEWER_ROW_ADD };
enum { FUDE_VIEWER_BACK = 0, FUDE_VIEWER_PREV, FUDE_VIEWER_REPLAY, FUDE_VIEWER_NEXT, FUDE_VIEWER_STUDY, FUDE_VIEWER_SHEET, FUDE_VIEWER_PRACTICE };
static const fude_row_button FUDE_VIEWER_MAIN[] = {
    { FUDE_TEXT_BACK,     FUDE_ICON_BACK,   fude_viewer_row_back,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PREV,     FUDE_ICON_PREV,   fude_viewer_row_prev,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_REPLAY,   FUDE_ICON_REPLAY, fude_viewer_row_replay,   0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NEXT,     FUDE_ICON_NEXT,   fude_viewer_row_next,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_STUDY,    FUDE_ICON_STAR,   fude_viewer_row_study,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SHEET,    FUDE_ICON_PRINT,  fude_viewer_row_sheet,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE, FUDE_ICON_PEN,    fude_viewer_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_button FUDE_VIEWER_ADD[] = {
    { FUDE_TEXT_DONE,          FUDE_ICON_CHECK,    fude_viewer_row_add_done, 0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_TYPE_YOUR_OWN, FUDE_ICON_KEYBOARD, fude_viewer_row_add_type, 0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_VIEWER_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_VIEWER_MAIN), FUDE_ROW_DEF(FUDE_VIEWER_ADD) };

RDE_INTERNAL u32 fude_viewer_screen_row(const void* _self) {
    return ((const fude_viewer*)_self)->adding ? FUDE_VIEWER_ROW_ADD : FUDE_VIEWER_ROW_MAIN;
}

// Study shows the character's mark: Study, Studying (chosen), Known (chosen, its own icon).
RDE_INTERNAL void fude_viewer_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    const u32 _cp = fude_viewer_codepoint((const fude_viewer*)_self);
    if(_row != FUDE_VIEWER_ROW_MAIN || _cp == 0u) {
        return;
    }
    const FUDE_MARK_ _mark = fude_marks_get(_cp);
    if(_mark != FUDE_MARK_NONE) {
        snprintf(_faces[FUDE_VIEWER_STUDY].label, FUDE_ROW_LABEL, "%s", fude_text(_mark == FUDE_MARK_KNOWN ? FUDE_TEXT_KNOWN : FUDE_TEXT_STUDYING));
        _faces[FUDE_VIEWER_STUDY].icon     = _mark == FUDE_MARK_KNOWN ? FUDE_ICON_KNOWN : NULL;
        _faces[FUDE_VIEWER_STUDY].selected = true;
    }
}

const fude_screen FUDE_VIEWER_SCREEN = {
    .name = "viewer", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_viewer_screen_is_open, .close = fude_viewer_screen_close, .resume = fude_viewer_screen_resume,
    .update = fude_viewer_screen_update, .render = fude_viewer_screen_render,
    .pointer_down = fude_viewer_screen_down, .pointer_moved = fude_viewer_screen_moved, .pointer_up = fude_viewer_screen_up,
    .rows = FUDE_VIEWER_BUTTON_ROWS, .row_count = 2u, .row = fude_viewer_screen_row, .faces = fude_viewer_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
