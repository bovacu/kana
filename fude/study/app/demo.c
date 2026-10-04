#include "study/app/study.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/utf8.h"
#include "study/chars/glyph.h"
#include "study/handwriting/score.h"
#include "study/models/history.h"
#include "study/models/examlog.h"
#include "study/models/marks.h"
#include "study/models/review.h"
#include "study/models/vocab.h"
#include "study/models/charnote.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// The demo (a developer's: --demo[=LANG], study/app/look.c): six weeks of a
// learner's work, for the store's screenshots — written into the study files
// through the study's own models, as the app would have written them.
//
// The learner starts with hiragana, moves on to N5 kanji a week later and to
// katakana in the third week; practises a few characters most days (twelve days
// in a row up to today, a gap before), takes an exam every few days, does the
// reviews that fall due (today's are left for the screenshot: Reviews · n), and
// keeps a vocabulary in four lists and a few notes. Every character practised is
// WRITTEN — its own strokes, off as a hand is off, steadier with every session —
// and scored by the app's scorer, so the scores, the album's pages and the
// statistics are the app's own. The same every run (a fixed seed).
//
// Run it on empty saves: it adds to whatever is there. LANG (en es pt ja fr; none:
// the app's language): the vocabulary's meanings, the lists' names, the notes.
// ===========================================================================

#if defined(RDE_DEBUG)

#define FUDE_DEMO_DAYS      42u     // six weeks, today the last
#define FUDE_DEMO_MAX_CHARS 256u
#define FUDE_DEMO_SQUARES   FUDE_PRACTICE_DEFAULT_SQUARES

RDE_INTERNAL u32 fude_demo_state = 0x9E3779B9u;

RDE_INTERNAL u32 fude_demo_rand(void) {
    u32 _x = fude_demo_state;
    _x ^= _x << 13;
    _x ^= _x >> 17;
    _x ^= _x << 5;
    return fude_demo_state = _x;
}

RDE_INTERNAL f32 fude_demo_unit(void) {
    return (f32)(fude_demo_rand() & 0xFFFFFFu) / 16777216.0f;
}

RDE_INTERNAL f32 fude_demo_range(f32 _a, f32 _b) {
    return _a + (_b - _a) * fude_demo_unit();
}

// --- writing a character as a hand would -----------------------------------------------

// _info written in a square _units wide (Y up) by a hand of _care (0: a
// beginner's, 1: a steady one): its strokes in order — two swapped now and then,
// one drawn backwards more rarely, while care is low — and off as a hand is off:
// placed, sized and turned a little, each stroke shifted, stretched and wobbling,
// at a hand's pace.
RDE_INTERNAL void fude_demo_write(const fude_kanji_db* _db, const fude_kanji_info* _info, f32 _care, f32 _units, fude_ink* _out) {
    static rde_vec_2F     _pts[FUDE_GLYPH_MAX_POINTS];
    static fude_ink_point _q[FUDE_GLYPH_MAX_POINTS];
    fude_ink_init(_out);
    const f32 _off   = 1.0f - _care;
    const f32 _k     = _units / FUDE_KANJI_BOX * fude_demo_range(0.84f, 0.97f);
    const f32 _cx    = _units * 0.5f + fude_demo_range(-1.0f, 1.0f) * _units * 0.04f * (0.3f + _off);
    const f32 _cy    = _units * 0.5f + fude_demo_range(-1.0f, 1.0f) * _units * 0.04f * (0.3f + _off);
    const f32 _turn  = fude_demo_range(-1.0f, 1.0f) * 0.08f * (0.2f + _off);
    const f32 _cos   = cosf(_turn), _sin = sinf(_turn);
    const f32 _wide  = 1.0f + fude_demo_range(-1.0f, 1.0f) * 0.14f * _off;   // proportions: a little wide, or narrow

    u32 _order[64];
    const u32 _strokes = _info->strokes < 64u ? _info->strokes : 64u;
    for(u32 _s = 0; _s < _strokes; _s++) {
        _order[_s] = _s;
    }
    if(_strokes >= 3u && fude_demo_unit() < 0.6f * _off * _off) {
        const u32 _a = 1u + fude_demo_rand() % (_strokes - 2u);
        const u32 _t = _order[_a];
        _order[_a]     = _order[_a + 1u];
        _order[_a + 1u] = _t;
    }
    const u32 _backwards = _strokes >= 2u && fude_demo_unit() < 0.2f * _off * _off ? fude_demo_rand() % _strokes : UINT32_MAX;

    for(u32 _o = 0; _o < _strokes; _o++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_db, _info, _order[_o], &_stroke)) {
            continue;
        }
        const u32 _n = fude_glyph_stroke_points(&_stroke, _pts, NULL);
        if(_n < 2u) {
            continue;
        }
        const b8  _back    = _order[_o] == _backwards;
        const f32 _dx      = fude_demo_range(-1.0f, 1.0f) * 12.0f * (0.1f + _off * _off);   // KanjiVG units
        const f32 _dy      = fude_demo_range(-1.0f, 1.0f) * 12.0f * (0.1f + _off * _off);
        const f32 _stretch = 1.0f + fude_demo_range(-1.0f, 1.0f) * 0.3f * (0.1f + _off * _off);
        const f32 _amp     = 0.4f + 7.0f * _off * _off;
        const f32 _freq    = fude_demo_range(1.2f, 2.6f);
        const f32 _phase   = fude_demo_range(0.0f, 6.2832f);
        const f32 _seconds = fude_demo_range(0.22f, 0.5f) * (1.0f + 0.7f * _off);
        const rde_vec_2F _from = _pts[_back ? _n - 1u : 0u];
        for(u32 _i = 0; _i < _n; _i++) {
            const rde_vec_2F _p = _pts[_back ? _n - 1u - _i : _i];
            const f32        _t = (f32)_i / (f32)(_n - 1u);
            // Stretched from where the stroke starts, shifted, wobbling across it.
            f32 _x = _from.x + (_p.x - _from.x) * _stretch + _dx + _amp * sinf(6.2832f * _freq * _t + _phase);
            f32 _y = _from.y + (_p.y - _from.y) * _stretch + _dy + _amp * cosf(6.2832f * _freq * _t + _phase * 0.7f);
            // The box's centre at the square's (KanjiVG's Y down → the square's Y up), turned.
            _x  = (_x - FUDE_KANJI_BOX * 0.5f) * _wide;
            _y  = FUDE_KANJI_BOX * 0.5f - _y;
            _q[_i] = (fude_ink_point){
                .position = { _cx + (_x * _cos - _y * _sin) * _k, _cy + (_x * _sin + _y * _cos) * _k },
                .pressure = 0.45f + 0.35f * sinf(3.1416f * _t),
                .radius   = _units * 0.02f,
                .time     = _t * _seconds,
            };
        }
        fude_ink_add_loaded_stroke(_out, _q, _n, FUDE_THEME_INK, true);
    }
}

// --- the learner -----------------------------------------------------------------------------

typedef struct {
    u32 codepoint;
    u32 record;
    u32 introduced;   // the day it was first studied (UINT32_MAX: not yet)
    u32 known;        // the day it was marked Known (UINT32_MAX: not)
    u32 sessions;     // practised so far
    f32 care;         // how steady the hand is with it now (0..1)
} fude_demo_char;

typedef struct {
    fude_app*      app;
    fude_kanji_db* db;
    u64            now;
    u32            today;   // review.h's day number for today
    fude_demo_char chars[FUDE_DEMO_MAX_CHARS];
    u32            count;
} fude_demo;

// The Unix time of a moment of day _day (0 the first, FUDE_DEMO_DAYS - 1 today),
// _hours before now's hour of that day.
RDE_INTERNAL u64 fude_demo_time(const fude_demo* _demo, u32 _day, f32 _hours) {
    return _demo->now - (u64)(FUDE_DEMO_DAYS - 1u - _day) * 86400u - (u64)(_hours * 3600.0f);
}

RDE_INTERNAL void fude_demo_add_chars(fude_demo* _demo, const c8* _utf8) {
    for(const c8* _p = _utf8; *_p != 0 && _demo->count < FUDE_DEMO_MAX_CHARS;) {
        const u32 _cp = fude_utf8_next(&_p);
        u32       _record;
        if(_cp != 0 && fude_kanji_find_index(_demo->db, _cp, &_record)) {
            _demo->chars[_demo->count++] = (fude_demo_char){ _cp, _record, UINT32_MAX, UINT32_MAX, 0u, 0.0f };
        }
    }
}

// N5 kanji, most frequent first.
RDE_INTERNAL void fude_demo_add_n5(fude_demo* _demo, u32 _max) {
    u32 _cps[512];
    u32 _freq[512];
    u32 _n = 0;
    for(u32 _r = 0; _r < _demo->db->count && _n < 512u; _r++) {
        fude_kanji_info _info;
        if(fude_kanji_at(_demo->db, _r, &_info) && _info.level == 5u && _info.codepoint >= 0x4E00u) {
            _cps[_n]  = _info.codepoint;
            _freq[_n] = _info.frequency != 0 ? _info.frequency : 9999u;
            _n++;
        }
    }
    for(u32 _i = 1; _i < _n; _i++) {   // insertion sort: a few dozen
        for(u32 _j = _i; _j > 0 && _freq[_j] < _freq[_j - 1u]; _j--) {
            const u32 _f = _freq[_j]; _freq[_j] = _freq[_j - 1u]; _freq[_j - 1u] = _f;
            const u32 _c = _cps[_j];  _cps[_j]  = _cps[_j - 1u];  _cps[_j - 1u]  = _c;
        }
    }
    for(u32 _i = 0; _i < _n && _i < _max && _demo->count < FUDE_DEMO_MAX_CHARS; _i++) {
        u32 _record;
        if(fude_kanji_find_index(_demo->db, _cps[_i], &_record)) {
            _demo->chars[_demo->count++] = (fude_demo_char){ _cps[_i], _record, UINT32_MAX, UINT32_MAX, 0u, 0.0f };
        }
    }
}

// Those of [_first, _first + _count) to introduce on _day: _per a day from _start.
RDE_INTERNAL void fude_demo_introduce(fude_demo* _demo, u32 _first, u32 _count, u32 _start, f32 _per, u32 _day) {
    if(_day < _start) {
        return;
    }
    const u32 _upto = (u32)fminf((f32)_count, (f32)(_day - _start + 1u) * _per);
    u32       _new[64];
    u32       _n = 0;
    for(u32 _i = 0; _i < _upto && _n < 64u; _i++) {
        fude_demo_char* _c = &_demo->chars[_first + _i];
        if(_c->introduced == UINT32_MAX) {
            _c->introduced = _day;
            _c->care       = fude_demo_range(0.12f, 0.25f) + 0.35f * (f32)_day / (f32)FUDE_DEMO_DAYS;   // the hand better every week
            _new[_n++]     = _c->codepoint;
        }
    }
    fude_marks_set_many_at(_new, _n, FUDE_MARK_STUDYING, fude_demo_time(_demo, _day, 3.0f));
}

// The day's end: what has been studied long enough, practised enough and written
// steadily enough is Known (kana sooner than kanji).
RDE_INTERNAL void fude_demo_settle(fude_demo* _demo, u32 _day) {
    u32 _known[FUDE_DEMO_MAX_CHARS];
    u32 _n = 0;
    for(u32 _i = 0; _i < _demo->count; _i++) {
        fude_demo_char* _c    = &_demo->chars[_i];
        const b8        _kana = _c->codepoint < 0x3100u;
        if(_c->introduced != UINT32_MAX && _c->known == UINT32_MAX && _day >= _c->introduced + (_kana ? 3u : 6u) &&
           _c->sessions >= (_kana ? 1u : 2u) && _c->care >= (_kana ? 0.4f : 0.5f)) {
            _c->known  = _day;
            _known[_n++] = _c->codepoint;
        }
    }
    fude_marks_set_many_at(_known, _n, FUDE_MARK_KNOWN, fude_demo_time(_demo, _day, 0.5f));
}

// A practice session of _c on _day: FUDE_DEMO_SQUARES squares written, scored,
// kept; the hand steadier for the next.
RDE_INTERNAL void fude_demo_practise(fude_demo* _demo, fude_demo_char* _c, u32 _day, f32 _hours) {
    fude_kanji_info _info;
    if(!fude_kanji_at(_demo->db, _c->record, &_info)) {
        return;
    }
    static fude_ink   _inks[FUDE_DEMO_SQUARES];
    fude_score        _scores[FUDE_DEMO_SQUARES];
    const fude_ink*   _drawings[FUDE_DEMO_SQUARES];
    for(u32 _s = 0; _s < FUDE_DEMO_SQUARES; _s++) {
        const f32 _care = fminf(0.97f, _c->care + 0.03f * (f32)_s + fude_demo_range(-0.08f, 0.08f));   // steadier square by square
        fude_demo_write(_demo->db, &_info, _care, FUDE_PRACTICE_UNITS, &_inks[_s]);
        _scores[_s]   = fude_score_drawing(_demo->db, &_info, &_inks[_s]);
        _drawings[_s] = &_inks[_s];
    }
    fude_history_save(_c->codepoint, fude_demo_time(_demo, _day, _hours), FUDE_DEMO_SQUARES, _scores, _drawings, FUDE_PRACTICE_UNITS);
    for(u32 _s = 0; _s < FUDE_DEMO_SQUARES; _s++) {
        fude_ink_destroy(&_inks[_s]);
    }
    _c->sessions++;
    _c->care = fminf(0.92f, _c->care + fude_demo_range(0.08f, 0.16f));
}

// An exam on _day of up to _max of the characters introduced (from _first, _count
// of them), written from memory, scored and marked; Known after a good run.
RDE_INTERNAL void fude_demo_exam(fude_demo* _demo, u32 _first, u32 _count, u8 _source, u32 _max, u32 _day) {
    static fude_ink          _inks[16];
    static fude_examlog_item _items[16];
    const fude_ink*          _drawings[16];
    u32                      _picked[16];
    u32                      _n = 0;
    if(_count == 0) {
        return;   // a group the app's data has none of (the demo's are Japanese's)
    }
    for(u32 _tries = 0; _tries < 200u && _n < _max && _n < 16u; _tries++) {
        const u32 _i = _first + fude_demo_rand() % _count;
        if(_demo->chars[_i].introduced == UINT32_MAX || _demo->chars[_i].introduced >= _day) {
            continue;
        }
        b8 _again = false;
        for(u32 _j = 0; _j < _n; _j++) {
            _again = _again || _picked[_j] == _i;
        }
        if(!_again) {
            _picked[_n++] = _i;
        }
    }
    if(_n < 4u) {
        return;
    }
    u32 _known[16];
    u32 _nk = 0;
    for(u32 _j = 0; _j < _n; _j++) {
        fude_demo_char* _c = &_demo->chars[_picked[_j]];
        fude_kanji_info _info, _wrote;
        fude_kanji_at(_demo->db, _c->record, &_info);
        // From memory: now and then it is not there, and a look-alike is written instead.
        u32       _alike[4];
        const u32 _alikes = fude_kanji_lookalikes(_demo->db, _c->record, _alike, 4u);
        const b8  _forgot = _alikes > 0u && fude_demo_unit() < 0.06f + 0.3f * (1.0f - _c->care) && fude_kanji_find(_demo->db, _alike[0], &_wrote);   // code points
        fude_demo_write(_demo->db, _forgot ? &_wrote : &_info, fminf(0.95f, _c->care + 0.05f), FUDE_EXAM_UNITS, &_inks[_j]);
        const fude_score _s       = fude_score_drawing(_demo->db, &_info, &_inks[_j]);
        const f32        _quality = _s.empty ? 0.0f : _s.score;
        const b8         _right   = !_forgot && _quality >= 50.0f;
        _items[_j] = (fude_examlog_item){
            .codepoint = _c->codepoint, .correct = _right, .score = _right ? _quality : 0.0f, .quality = _quality,
            .read_as   = _forgot ? _wrote.codepoint : _right ? _c->codepoint : 0u,
        };
        _drawings[_j] = &_inks[_j];
        if(_right && _c->known == UINT32_MAX && _c->sessions >= 2u && _day >= _c->introduced + 5u) {
            _c->known    = _day;
            _known[_nk++] = _c->codepoint;
        }
    }
    const u64 _at = fude_demo_time(_demo, _day, 1.5f);
    fude_examlog_add(_at, _source, _items, _n, _drawings, FUDE_EXAM_UNITS);
    fude_marks_set_many_at(_known, _nk, FUDE_MARK_KNOWN, _at + 60u);
    for(u32 _j = 0; _j < _n; _j++) {
        fude_reviews_answer(_items[_j].codepoint, _items[_j].correct, _items[_j].quality / 100.0f);
        fude_ink_destroy(&_inks[_j]);
    }
}

// The reviews due on _day (the fake day set), answered — mostly right, by how
// steady the hand is with each — but the last _leave.
RDE_INTERNAL void fude_demo_reviews(fude_demo* _demo, u32 _leave) {
    u32 _candidates[FUDE_DEMO_MAX_CHARS];
    u32 _n = 0;
    for(u32 _i = 0; _i < _demo->count; _i++) {
        if(_demo->chars[_i].introduced != UINT32_MAX) {
            _candidates[_n++] = _demo->chars[_i].codepoint;
        }
    }
    u32       _due[FUDE_REVIEW_SESSION];
    const u32 _d = fude_reviews_due(_candidates, _n, _due, FUDE_REVIEW_SESSION);
    for(u32 _k = 0; _k + _leave < _d; _k++) {
        f32 _care = 0.7f;
        for(u32 _i = 0; _i < _demo->count; _i++) {
            _care = _demo->chars[_i].codepoint == _due[_k] ? _demo->chars[_i].care : _care;
        }
        fude_reviews_answer(_due[_k], fude_demo_unit() < 0.55f + 0.4f * _care, fminf(1.0f, _care + fude_demo_range(-0.1f, 0.1f)));
    }
}

// --- the vocabulary and the notes ---------------------------------------------------------

// The word _written, as the dictionary has it among one of its characters'
// words: its reading and meaning (in the meanings' language now). False: not there.
RDE_INTERNAL b8 fude_demo_word(const fude_kanji_db* _db, const c8* _written, fude_kanji_word* _out) {
    const c8* _p = _written;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0; _cp = fude_utf8_next(&_p)) {
        u32 _record;
        if(!fude_kanji_find_index(_db, _cp, &_record)) {
            continue;
        }
        u32       _words[64];
        u32       _examples;
        const u32 _n = fude_kanji_words(_db, _record, _words, 64u, &_examples);
        for(u32 _i = 0; _i < _n; _i++) {
            if(fude_kanji_word_at(_db, _words[_i], _out) && strcmp(_out->written, _written) == 0) {
                return true;
            }
        }
    }
    return false;
}

RDE_INTERNAL u32 fude_demo_lang(const c8* _lang) {
    static const c8* const _codes[] = { "en", "es", "pt", "ja", "fr" };
    for(u32 _i = 0; _i < 5u; _i++) {
        if(strcmp(_lang, _codes[_i]) == 0) {
            return _i;
        }
    }
    return 0u;
}

RDE_INTERNAL void fude_demo_vocabulary(fude_demo* _demo, u32 _lang) {
    static const c8* const _names[4][5] = {
        { "Lesson 1", "Lección 1", "Lição 1", "第1課", "Leçon 1" },
        { "Food", "Comida", "Comida", "食べ物", "Nourriture" },
        { "Travel", "Viajes", "Viagem", "旅行", "Voyage" },
        { "Verbs", "Verbos", "Verbos", "動詞", "Verbes" },
    };
    static const c8* const _words[4][10] = {
        { "日本語", "先生", "学生", "友達", "名前", "学校", "毎日", "今日", "明日", "時間" },
        { "食べ物", "飲む", "水", "魚", "肉", "野菜", "料理", "果物", "牛乳", NULL },
        { "電車", "駅", "旅行", "東京", "空港", "地図", "切符", "道", "天気", NULL },
        { "来る", "見る", "書く", "読む", "話す", "聞く", "買う", "休む", NULL, NULL },
    };
    for(u32 _l = 0; _l < 4u; _l++) {
        const u32 _list = fude_vocab_list_add(_names[_l][_lang]);
        for(u32 _w = 0; _w < 10u && _words[_l][_w] != NULL; _w++) {
            fude_kanji_word _word;
            if(!fude_demo_word(_demo->db, _words[_l][_w], &_word)) {
                rde_log_level(RDE_LOG_LEVEL_WARNING, "demo: %s is not in the dictionary", _words[_l][_w]);
                continue;
            }
            // A meaning short enough for a row: up to its second "; ".
            c8          _meaning[FUDE_USERWORD_MEANING];
            snprintf(_meaning, sizeof(_meaning), "%s", _word.meaning);
            const c8*   _cut = strstr(_meaning, "; ");
            _cut = _cut != NULL ? strstr(_cut + 2, "; ") : NULL;
            if(_cut != NULL) {
                _meaning[_cut - _meaning] = 0;
            }
            const u32 _id = fude_vocab_add(_word.written, _word.reading, _meaning, 0u);
            if(_id != 0u && _list != 0u) {
                fude_vocab_set_in_list(_list, _id, true);
            }
        }
    }
}

// Words reviewed over the weeks (word exams answered), on the fake day set.
RDE_INTERNAL void fude_demo_word_reviews(u32 _day_index, u32 _days) {
    const u32 _n = fude_vocab_count();
    for(u32 _i = 0; _i < _n; _i++) {
        // Each word's own first day: some never reviewed (New), some recently.
        if((_i * 7u) % _days == _day_index % _days || (_i % 3u == 0u && _day_index % 5u == _i % 5u)) {
            fude_reviews_answer(FUDE_VOCAB_KEY(fude_vocab_at(_i)->id), fude_demo_unit() < 0.8f, fude_demo_range(0.6f, 0.95f));
        }
    }
}

RDE_INTERNAL void fude_demo_notes(u32 _lang) {
    static const struct { u32 codepoint; const c8* text[5]; } _notes[] = {
        { 0x65E5u, { "One bar inside: the sun. Two bars: 目, an eye.", "Una barra dentro: el sol. Dos barras: 目, un ojo.",
                     "Uma barra dentro: o sol. Duas barras: 目, um olho.", "中の横棒が1本なら日、2本なら目。",
                     "Une barre à l’intérieur : le soleil. Deux barres : 目, un œil." } },
        { 0x4F11u, { "A person (亻) resting against a tree (木).", "Una persona (亻) descansando junto a un árbol (木).",
                     "Uma pessoa (亻) descansando junto a uma árvore (木).", "人（亻）が木のそばで休む。",
                     "Une personne (亻) qui se repose contre un arbre (木)." } },
        { 0x53F3u, { "右 has 口 (the hand you eat with); 左 has 工.", "右 lleva 口 (la mano con la que se come); 左 lleva 工.",
                     "右 tem 口 (a mão com que se come); 左 tem 工.", "右は口、左は工。",
                     "右 a 口 (la main qui mange) ; 左 a 工." } },
    };
    for(u32 _i = 0; _i < sizeof(_notes) / sizeof(_notes[0]); _i++) {
        fude_charnote_set(_notes[_i].codepoint, _notes[_i].text[_lang]);
    }
}

// --- the six weeks -----------------------------------------------------------------------

void fude_study_demo(fude_app* _app, const c8* _lang) {
    fude_study* _study = FUDE_STUDY(_app);
    if(_study->db == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "demo: no character data");
        return;
    }
    static fude_demo _demo;
    memset(&_demo, 0, sizeof(_demo));
    fude_demo_state = 0x9E3779B9u;
    _demo.app   = _app;
    _demo.db    = _study->db;
    _demo.now   = (u64)time(NULL);
    _demo.today = fude_reviews_today();

    const RDE_LANGUAGE_ _ui   = fude_text_language();
    const c8*           _code = _lang != NULL && _lang[0] != 0 ? _lang
                              : _ui == RDE_LANGUAGE_ES_ES ? "es" : _ui == RDE_LANGUAGE_PT_BR ? "pt" : _ui == RDE_LANGUAGE_JA_JP ? "ja" : _ui == RDE_LANGUAGE_FR_FR ? "fr" : "en";
    const u32           _lang_i = fude_demo_lang(_code);

    // The characters, in the order they are taken up.
    const u32 _hira = _demo.count;
    fude_demo_add_chars(&_demo, "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん");
    const u32 _hira_n = _demo.count - _hira;
    const u32 _kanji = _demo.count;
    fude_demo_add_n5(&_demo, 80u);
    const u32 _kanji_n = _demo.count - _kanji;
    const u32 _kata = _demo.count;
    fude_demo_add_chars(&_demo, "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン");
    const u32 _kata_n = _demo.count - _kata;

    for(u32 _day = 0; _day < FUDE_DEMO_DAYS; _day++) {
        // Twelve days in a row up to today; most days before, a gap just before them.
        const b8 _active = _day >= FUDE_DEMO_DAYS - 12u || (_day >= 3u && _day <= 18u) ||
                           (_day != FUDE_DEMO_DAYS - 13u && fude_demo_unit() < 0.55f);
        fude_reviews_fake_today = _demo.today - (FUDE_DEMO_DAYS - 1u - _day);
        if(!_active) {
            continue;
        }
        fude_demo_introduce(&_demo, _hira, _hira_n, 0u, 7.0f, _day);
        fude_demo_introduce(&_demo, _kanji, _kanji_n, 7u, 1.6f, _day);
        fude_demo_introduce(&_demo, _kata, _kata_n, 18u, 2.2f, _day);

        // Practice: five to eight characters, the newest and the least steady first.
        const u32 _sessions = 5u + fude_demo_rand() % 4u;
        for(u32 _s = 0; _s < _sessions; _s++) {
            fude_demo_char* _pick = NULL;
            f32             _best = -1.0f;
            for(u32 _i = 0; _i < _demo.count; _i++) {
                fude_demo_char* _c = &_demo.chars[_i];
                if(_c->introduced == UINT32_MAX || (_c->sessions > 0u && _c->introduced == _day && fude_demo_unit() < 0.5f)) {
                    continue;
                }
                const f32 _want = (1.0f - _c->care) + (_c->introduced + 6u >= _day ? 0.4f : 0.0f) + fude_demo_range(0.0f, 0.5f);
                if(_want > _best && _c->sessions < 6u) {
                    _best = _want;
                    _pick = _c;
                }
            }
            if(_pick != NULL) {
                fude_demo_practise(&_demo, _pick, _day, 4.0f - (f32)_s * 0.3f);
            }
        }

        // An exam every few days, of what is there by then; the reviews due, but today's.
        if(_day % 3u == 1u) {
            if(_day < 9u)        { fude_demo_exam(&_demo, _hira, _hira_n, fude_exam_source_set(0u), 10u, _day); }
            else if(_day % 2u)   { fude_demo_exam(&_demo, _kanji, _kanji_n, fude_exam_source_level(0u), 10u, _day); }
            else if(_day >= 20u) { fude_demo_exam(&_demo, _kata, _kata_n, fude_exam_source_set(1u), 10u, _day); }
            else                 { fude_demo_exam(&_demo, _kanji, _kanji_n, FUDE_EXAM_SOURCE_STUDYING, 10u, _day); }
        }
        fude_demo_reviews(&_demo, _day == FUDE_DEMO_DAYS - 1u ? 12u : 0u);   // today's: a dozen left (Reviews · 12)
        fude_demo_settle(&_demo, _day);
        if(_day == 20u) {
            // The meanings in the language asked for (the app's own set back after).
            fude_kanji_set_language(_demo.db, _lang_i == 1u ? "es" : _lang_i == 2u ? "pt" : _lang_i == 4u ? "fr" : NULL);
            fude_demo_vocabulary(&_demo, _lang_i);
            fude_kanji_set_language(_demo.db, _ui == RDE_LANGUAGE_ES_ES ? "es" : _ui == RDE_LANGUAGE_PT_BR ? "pt" : _ui == RDE_LANGUAGE_FR_FR ? "fr" : NULL);
        }
        if(_day > 20u && _day < FUDE_DEMO_DAYS - 1u) {
            fude_demo_word_reviews(_day, 9u);
        }
    }
    fude_reviews_fake_today = 0u;
    fude_demo_notes(_lang_i);

    u32 _practised = 0, _known = 0;
    for(u32 _i = 0; _i < _demo.count; _i++) {
        _practised += _demo.chars[_i].sessions > 0u ? 1u : 0u;
        _known     += _demo.chars[_i].known != UINT32_MAX ? 1u : 0u;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "demo (%s): %u characters practised, %u known, %u exams, %u words", _code, _practised, _known,
                  fude_examlog_count(), fude_vocab_count());
}

#else

void fude_study_demo(fude_app* _app, const c8* _lang) {
    RDE_UNUSED(_app);
    RDE_UNUSED(_lang);
}

#endif
