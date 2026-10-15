// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/app/study.h"
#include "study/services/speech.h"
#include "drawing/widgets/notice.h"
#include "lang/lang.h"
#include "drawing/app/look.h"
#include "drawing/app/ui.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "study/models/vocab.h"
#include "study/models/charnote.h"
#include "study/handwriting/recognize.h"
#include "drawing/ink/mlkit.h"
#include "study/services/translate.h"
#include "study/services/textscan.h"
#include "study/widgets/wordcard.h"
#include "study/chars/wordsplit.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// See study.h. This file holds the study's launch flags, a developer's (look.h has the core's, and how they are
// read). They open a screen in a given state, do something at a given frame of a shot's sequence, or probe two of the
// device's own services. TEXT, JA and the like are in the language taught (Japanese in Kana).
//
//   --browse --browse-sort=N --chart --library --notice   the lists, the language's chart (lang.h), the Library, and
//                                                          the longest notice at the bottom
//   --viewer=HEX [--note=TEXT] --practice=HEX --guided=HEX --sheet=FILE   (HEX is a character's code point)
//   --album --album-exams --album-page=HEX --kept-exam=N --stats [--scroll=PX]   (--scroll also scrolls Settings)
//   --exam --exam-start --vocab[=N] --vocab-search=WORDS --vocab-sample --word-exam[=STAGE] --word-card=TEXT
//   [--word-sentence=JA|TRANSLATION]   the word card's word as met in that sentence (JA: the sentence in the
//                                      language taught)
//   --word-translate=MEANING   the word card, with MEANING translated into the language taught (Translate with
//                              Google, made up)
//   --paste-text=TEXT [--save-selection] [--translate-selection]
//   --scan-demo=PNG [--scan-demo-turn=DEG] [--scan-demo-top-first] [--scan-demo-translate] --scan-live
//   --read-demo   a document's scanned pages read with made-up lines
//   --mlkit-samples=0-27,28-58  --translate-probe=xx
//   --demo[=LANG]   six weeks of a learner's work, for the store's screenshots (demo.c)
//   --translator=TEXT[|TEXT...]   the translator into the language taught, over the Vocabulary, asked each TEXT
//                                 (the translations are made up)
//   --translate-demo   Translate with Google pretends, and opens nothing (its buttons show, as on a device)
//   --welcome[=PAGE] --offline-note   at frame 10: the welcome (on that page), and why the app works offline
//   --voice-card[=android] [--voice-missing]   at frame 12: the voice card (with Android's steps); --voice-missing
//                                              makes a computer's voice count as missing
//   --demo also closes the welcome, because the demo's saves are a learner's, not a first launch.
// ===========================================================================

// The most ranges --mlkit-samples takes.
#define FUDE_STUDY_LOOK_SAMPLES 32

// What the flags asked for, read once by fude_study_look_args. The arguments are kept to be read again by
// fude_study_look_start, so they must stay valid for the whole run. -1 means a number flag was not given.
RDE_INTERNAL struct {
    i32       argc;
    c8**      argv;
    b8        stats, vocab_sample, save_sel, translate_sel, scan_live, scan_rows, scan_translate;
    // --voice-card[=android]: the voice card at frame 12, with Android's steps when =android is given.
    b8        voice_card;   // the voice card at frame 12
    b8        voice_android;
    i32       vocab, word_exam;
    i64       kept_exam;
    f32       scroll, scan_turn;
    const c8* note;
    const c8* paste_text;
    const c8* word_card;
    const c8* vocab_search;   // --vocab-search=WORDS: what to type in the Vocabulary's field
    const c8* word_sentence;
    const c8* word_translate;
    const c8* scan_demo;
    const c8* sheet;
    const c8* translate_probe;
    const c8* demo;         // --demo[=LANG] ("" for the app's language)
    const c8* translator;   // --translator=TEXT[|TEXT...]
    i32       welcome;      // --welcome[=PAGE]: the welcome at frame 10, on that page (1 is the first; -1: not asked)
    b8        offline;      // --offline-note: why the app works offline (the must-read card) at frame 10
    // --mlkit-samples: ranges of the page's strokes, each read by ML Kit in turn. sample_next is the range being read,
    // and sample_asked says it has been handed to ML Kit.
    u32       sample_first[FUDE_STUDY_LOOK_SAMPLES];
    u32       sample_last[FUDE_STUDY_LOOK_SAMPLES];
    u32       sample_count, sample_next;
    b8        sample_asked;
} fude_study_look = { .vocab = -1, .word_exam = -1, .kept_exam = -1, .welcome = -1 };

// --perf's summary line gets the camera's frames shown and ML Kit's reads of them, while Text from a photo is open.
RDE_INTERNAL void fude_study_look_perf_note(fude_app* _app, c8* _out, usize _size) {
    const fude_scan* _scan = FUDE_STUDY(_app)->scan;
    if(_scan->open) {
        rde_snprintf(_out, _size, " | camera %u frames shown, %u read, %.0f ms a read", _scan->frames_shown, _scan->frames_read,
                 _scan->frames_read > 0 ? 1000.0 * _scan->read_seconds / (f64)_scan->frames_read : 0.0);
    }
}

// Reads the study's flags before anything is built, and keeps the arguments for fude_study_look_start.
void fude_study_look_args(i32 _argc, c8** _argv) {
    fude_study_look.argc = _argc;
    fude_study_look.argv = _argv;
    fude_look_perf_note(fude_study_look_perf_note);
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _a = _argv[_i];
        const c8* _v;
        // Translate with Google pretends (debug builds): this must be known before the UI is built.
        if(fude_look_is(_a, "--scan-demo-translate")) { fude_translate_demo(true); fude_study_look.scan_translate = true; }
        if(fude_look_is(_a, "--translate-selection")) { fude_translate_demo(true); fude_study_look.translate_sel = true; }
        if(fude_look_is(_a, "--save-selection"))      { fude_study_look.save_sel = true; }
        if(fude_look_is(_a, "--translate-demo"))      { fude_translate_demo(true); }
        if(fude_look_is(_a, "--read-demo"))           { fude_study_picture_demo(true); }   // made-up lines
        if(fude_look_is(_a, "--scan-demo-top-first")) { fude_study_look.scan_rows = true; }
        if(fude_look_is(_a, "--scan-live"))           { fude_study_look.scan_live = true; }
        if(fude_look_is(_a, "--stats"))               { fude_study_look.stats = true; }
        if(fude_look_is(_a, "--vocab-sample"))        { fude_study_look.vocab_sample = true; }
        if(fude_look_is(_a, "--vocab"))               { fude_study_look.vocab = 0; }
        if(fude_look_is(_a, "--word-exam"))           { fude_study_look.word_exam = 0; }
        if(fude_look_is(_a, "--demo"))                { fude_study_look.demo = ""; }
        if((_v = fude_look_value(_a, "--demo")) != NULL)           { fude_study_look.demo = _v; }
        if((_v = fude_look_value(_a, "--translator")) != NULL)     { fude_study_look.translator = _v; fude_translate_demo(true); }
        if((_v = fude_look_value(_a, "--vocab")) != NULL)          { fude_study_look.vocab = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--vocab-search")) != NULL)   { fude_study_look.vocab_search = _v; }
        if((_v = fude_look_value(_a, "--word-exam")) != NULL)      { fude_study_look.word_exam = (i32)rde_strtol(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--kept-exam")) != NULL)      { fude_study_look.kept_exam = (i64)rde_strtoul(_v, NULL, 10); }
        if((_v = fude_look_value(_a, "--scroll")) != NULL)         { fude_study_look.scroll = rde_strtof(_v, NULL); }
        if(fude_look_is(_a, "--voice-card"))                       { fude_study_look.voice_card = true; }
        if((_v = fude_look_value(_a, "--voice-card")) != NULL)     { fude_study_look.voice_card = true; fude_study_look.voice_android = rde_strcmp(_v, "android") == 0; }
        if(fude_look_is(_a, "--voice-missing"))                    { fude_speech_look_missing(); }
        if(fude_look_is(_a, "--welcome"))                          { fude_study_look.welcome = 0; }
        if((_v = fude_look_value(_a, "--welcome")) != NULL)        { fude_study_look.welcome = (i32)rde_strtol(_v, NULL, 10) - 1; }
        if(fude_look_is(_a, "--offline-note"))                     { fude_study_look.offline = true; }
        if((_v = fude_look_value(_a, "--scan-demo-turn")) != NULL) { fude_study_look.scan_turn = rde_strtof(_v, NULL); }
        if((_v = fude_look_value(_a, "--note")) != NULL)           { fude_study_look.note = _v; }
        if((_v = fude_look_value(_a, "--word-card")) != NULL)      { fude_study_look.word_card = _v; }
        if((_v = fude_look_value(_a, "--word-sentence")) != NULL)  { fude_study_look.word_sentence = _v; }
        if((_v = fude_look_value(_a, "--word-translate")) != NULL) { fude_study_look.word_translate = _v; fude_translate_demo(true); }
        if((_v = fude_look_value(_a, "--scan-demo")) != NULL)      { fude_study_look.scan_demo = _v; }
        if((_v = fude_look_value(_a, "--sheet")) != NULL)          { fude_study_look.sheet = _v; }
        if((_v = fude_look_value(_a, "--translate-probe")) != NULL) { fude_study_look.translate_probe = _v; }
        if((_v = fude_look_value(_a, "--paste-text")) != NULL) {
            // "\\n" in the argument stands for a line break, because RDE reads its arguments as config lines. The text
            // is cut at 511 bytes.
            static c8 _pasted[512];
            usize     _n = 0;
            for(const c8* _p = _v; *_p != 0 && _n + 1u < sizeof(_pasted); _p++) {
                if(_p[0] == '\\' && _p[1] == 'n') { _pasted[_n++] = '\n'; _p++; }
                else                               { _pasted[_n++] = *_p; }
            }
            _pasted[_n]                = 0;
            fude_study_look.paste_text = _pasted;
        }
        if((_v = fude_look_value(_a, "--mlkit-samples")) != NULL) {
            const c8* _p = _v;
            while(*_p != 0 && fude_study_look.sample_count < FUDE_STUDY_LOOK_SAMPLES) {
                c8* _end = NULL;
                fude_study_look.sample_first[fude_study_look.sample_count] = (u32)rde_strtoul(_p, &_end, 10);
                fude_study_look.sample_last[fude_study_look.sample_count]  = *_end == '-' ? (u32)rde_strtoul(_end + 1, &_end, 10) : fude_study_look.sample_first[fude_study_look.sample_count];
                fude_study_look.sample_count++;
                _p = *_end == ',' ? _end + 1 : _end;
                if(*_end == 0) { break; }
            }
        }
    }
}

// Finds the record of the character whose code point is _hex (hexadecimal), when the data has it.
RDE_INTERNAL b8 fude_study_look_record(const fude_app* _app, const c8* _hex, u32* _record) {
    const fude_study* _study = FUDE_STUDY(_app);
    return _study->db != NULL && fude_kanji_find_index(_study->db, (u32)rde_strtoul(_hex, NULL, 16), _record);
}

// Opens the screens the flags ask for that do not need the saves, before they load.
void fude_study_look_start(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    for(i32 _i = 1; _i < fude_study_look.argc; _i++) {
        const c8* _a = fude_study_look.argv[_i];
        const c8* _v;
        u32       _record = 0;
        if(fude_look_is(_a, "--album"))      { fude_album_open(_study->album); }
        if(fude_look_is(_a, "--chart"))      { fude_chart_open(_study->chart); }
        if(fude_look_is(_a, "--library"))    { fude_library_open(_study->library); }
        if(fude_look_is(_a, "--browse"))     { fude_browse_open(_study->browse); }
        // --notice shows the longest notice, at the bottom.
        if(fude_look_is(_a, "--notice"))     { fude_notice_show(fude_text(FUDE_TEXT_REVIEWS_NONE)); }   // the longest
        if((_v = fude_look_value(_a, "--browse-sort")) != NULL) { fude_browse_open(_study->browse); fude_browse_set_sort(_study->browse, (FUDE_SORT_)rde_strtoul(_v, NULL, 10)); }
        if(fude_look_is(_a, "--album-exams")) { fude_album_open(_study->album); fude_album_set_view(_study->album, FUDE_ALBUM_VIEW_EXAMS); }
        if((_v = fude_look_value(_a, "--album-page")) != NULL) { fude_album_open(_study->album); fude_album_open_page(_study->album, (u32)rde_strtoul(_v, NULL, 16)); }
        if(fude_look_value(_a, "--kept-exam") != NULL)         { fude_album_open(_study->album); fude_album_set_view(_study->album, FUDE_ALBUM_VIEW_EXAMS); }
        if((_v = fude_look_value(_a, "--viewer")) != NULL)     { fude_viewer_show_codepoint(_study->viewer, (u32)rde_strtoul(_v, NULL, 16)); }
        if((_v = fude_look_value(_a, "--practice")) != NULL && fude_study_look_record(_app, _v, &_record)) { fude_practice_open(_study->practice, _record); }
        if((_v = fude_look_value(_a, "--guided")) != NULL && fude_study_look_record(_app, _v, &_record)) {
            fude_practice_open(_study->practice, _record);
            fude_practice_set_guided(_study->practice, true);
        }
        if(fude_look_is(_a, "--exam") || fude_look_is(_a, "--exam-start")) {
            fude_exam_open(_study->exam);
            if(fude_look_is(_a, "--exam-start")) {
                _study->exam->source = fude_exam_source_level(0u);
                fude_exam_preview(_study->exam);
                fude_exam_start(_study->exam);
            }
        }
    }
}

// --scan-demo=PNG: Text from a photo on that image, as a photo read would show it: four lines across it, the second
// one left out. The picture's size is read from the PNG's header (its IHDR's width and height, bytes 16 to 23).
RDE_INTERNAL void fude_study_look_scan_demo(fude_app* _app) {
    fude_scan* _scan = FUDE_STUDY(_app)->scan;
    // The screen opens first, so that it stays empty when there is no such file.
    fude_scan_open(_scan, _app->window, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));   // empty without the file
    // The file is kept for the whole run, as the static buffer it replaced was, because the screen shows the picture
    // from these bytes.
    static rde_arr _file;
    if(!rde_arr_is_inited(&_file)) {
        _file = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    }
    if(!fude_file_read_into(fude_study_look.scan_demo, &_file) || rde_arr_length(&_file) < 24u) {
        return;
    }
    const u8*   _png = (const u8*)_file.memory;
    const usize _n   = rde_arr_length(&_file);
    static fude_textscan_line _demo[4];
    const c8* const _texts[4] = { "今日は、日本語を", "勉強します。", "Hello 漢字", "とかな" };
    for(u32 _l = 0; _l < 4u; _l++) {
        rde_snprintf(_demo[_l].text, sizeof(_demo[_l].text), "%s", _texts[_l]);
        const f32 _y0 = 288.0f + 90.0f * (f32)_l, _y1 = _y0 + 64.0f;
        _demo[_l].corners[0] = (rde_vec_2F){ 86.0f, _y0 };
        _demo[_l].corners[1] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y0 };
        _demo[_l].corners[2] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y1 };
        _demo[_l].corners[3] = (rde_vec_2F){ 86.0f, _y1 };
        _demo[_l].block      = _l < 2u ? 0u : 1u;
    }
    const fude_textscan_result _r = { _png, _n, ((u32)_png[16] << 24) | ((u32)_png[17] << 16) | ((u32)_png[18] << 8) | _png[19],
                                      ((u32)_png[20] << 24) | ((u32)_png[21] << 16) | ((u32)_png[22] << 8) | _png[23], _demo, 4u };
    fude_scan_show(_scan, &_r);
    if(rde_arr_length(&_scan->lines) > 1u) {   // (none when the file's picture could not be shown: the screen says so)
        ((fude_scan_line*)_scan->lines.memory)[1].kept = false;
    }
    if(fude_study_look.scan_translate) {
        fude_scan_translate(_scan);
    }
    _scan->shown_top_first = fude_study_look.scan_rows;
    if(fude_study_look.scan_turn != 0.0f) {
        // The file holds the picture turned the other way; it is shown as a camera frame is. A turn of about 90 degrees
        // swaps the picture's width and height.
        _scan->shown_rotation = fude_study_look.scan_turn;
        if(rde_math_fmod_f32(rde_math_abs_f32(fude_study_look.scan_turn), 180.0f) > 45.0f) {
            const u32 _w = _scan->picture_w;
            _scan->picture_w = _scan->picture_h;
            _scan->picture_h = _w;
        }
    }
}

// Opens the screens the flags ask for that need the saves, once they are loaded, and fills the saves first for the
// demo and --vocab-sample.
void fude_study_look_loaded(fude_app* _app) {
    if(fude_study_look.demo != NULL) {
        fude_study_demo(_app, fude_study_look.demo);   // first, so that what the flags below open shows it
    }
    fude_study* _study = FUDE_STUDY(_app);
    if(fude_study_look.stats) {
        fude_stats_open(_study->stats);
    }
    if(fude_study_look.note != NULL && _study->viewer->open) {
        fude_charnote_set(fude_viewer_codepoint(_study->viewer), fude_study_look.note);
    }
    // --vocab-sample: a few words and two lists, when the vocabulary is empty. Every other word goes in the first list.
    if(fude_study_look.vocab_sample && fude_vocab_count() == 0u) {
        static const c8* const _sample[][3] = {
            { "日本語", "にほんご", "Japanese (language)" }, { "勉強", "べんきょう", "study" }, { "食べる", "たべる", "to eat" }, { "東京", "とうきょう", "Tokyo" },
            { "本", "ほん", "book; volume; script" }, { "先生", "せんせい", "teacher; instructor; master" }, { "学生", "がくせい", "student" }, { "読む", "よむ", "to read" },
        };
        const u32 _list = fude_vocab_list_add("Lesson 1");
        for(u32 _w = 0; _w < sizeof(_sample) / sizeof(_sample[0]); _w++) {
            const u32 _id = fude_vocab_add(_sample[_w][0], _sample[_w][1], _sample[_w][2], 0u);
            if(_w % 2u == 0u) {
                fude_vocab_set_in_list(_list, _id, true);
            }
        }
        fude_vocab_list_add("Food");
    }
    // --word-exam[=STAGE]: a word exam of every word: 0 shows its setup, 1 its writing, 2 its writing by ear, and 3 its
    // results.
    if(fude_study_look.word_exam >= 0) {
        const u32 _n   = fude_vocab_count();
        u32*      _ids = (u32*)rde_malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
        for(u32 _i = 0; _i < _n; _i++) {
            _ids[_i] = fude_vocab_at(_i)->id;
        }
        fude_wordexam_open(_study->wordexam, _ids, _n, fude_text(FUDE_TEXT_VOCAB));
        rde_free(_ids);
        if(fude_study_look.word_exam >= 1) {
            _study->wordexam->by = fude_study_look.word_exam == 2 ? FUDE_WORDEXAM_BY_EAR : FUDE_WORDEXAM_BY_MEANING;
            fude_wordexam_start(_study->wordexam);
        }
        for(u32 _i = 0; fude_study_look.word_exam == 3 && _i < FUDE_WORDEXAM_MAX && _study->wordexam->stage == FUDE_WORDEXAM_WRITING; _i++) {
            fude_wordexam_next(_study->wordexam);   // nothing is written, so the results show every word wrong
        }
    }
    if(fude_study_look.translator != NULL) {
        fude_vocabview_open(FUDE_STUDY(_app)->vocab);
        fude_translator_open(FUDE_STUDY(_app)->translator);
        c8 _texts[512];
        rde_snprintf(_texts, sizeof(_texts), "%s", fude_study_look.translator);
        c8* _t_rest = NULL;   // where rde_strtok carries on from
        for(c8* _t = rde_strtok(_texts, "|", &_t_rest); _t != NULL; _t = rde_strtok(NULL, "|", &_t_rest)) {
            fude_translator_ask(FUDE_STUDY(_app)->translator, _t);
        }
    }
    if(fude_study_look.vocab >= 0) {
        fude_vocabview_open(_study->vocab);
        fude_vocabview_show_list(_study->vocab, fude_study_look.vocab > 0 ? fude_vocab_list_at((u32)fude_study_look.vocab - 1u) : 0u);
        if(fude_study_look.vocab_search != NULL) {
            fude_vocabview_search(_study->vocab, fude_study_look.vocab_search);
        }
    }
    if(fude_study_look.kept_exam >= 0) {
        fude_exam_open_kept(_study->exam, (u32)fude_study_look.kept_exam);
    }
    if(fude_study_look.scan_live) {
        fude_scan_open(_study->scan, _app->window, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
        fude_scan_camera(_study->scan);
    }
    if(fude_study_look.scan_demo != NULL) {
        fude_study_look_scan_demo(_app);
    }
    if(fude_study_look.demo != NULL) {
        _study->welcome->open = false;   // the demo's saves are a learner's, not a first launch
    }
}

// --mlkit-samples: hands the next range of strokes to ML Kit (recognize.h), and once it answers, writes the answer to
// <save dir>/mlkit_samples.txt (to be copied off the device and compared). One range at a time, across frames; the
// file is started afresh by the first range and added to by the others.
RDE_INTERNAL void fude_study_look_samples(fude_app* _app) {
    static fude_recognition _reading;
    static f64              _asked_at;
    if(fude_study_look.sample_next >= fude_study_look.sample_count) {
        return;
    }
    if(fude_mlkit_state() == FUDE_MLKIT_FAILED) {
        fude_mlkit_prepare();
    }
    const u32 _first = fude_study_look.sample_first[fude_study_look.sample_next];
    const u32 _last  = fude_study_look.sample_last[fude_study_look.sample_next];
    if(!fude_study_look.sample_asked) {
        fude_ink _one;
        fude_ink_init(&_one);
        for(u32 _s = _first; _s <= _last && _s < fude_ink_stroke_count(_app->ink); _s++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_app->ink, _s);
            fude_ink_add_loaded_stroke(&_one, fude_ink_stroke_points(_app->ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        }
        fude_study_look.sample_asked = fude_recognize_start(&_reading, &_one);
        _asked_at              = rde_engine_get_time_now();
        fude_ink_destroy(&_one);
        return;
    }
    if(fude_recognize_poll(&_reading)) {
        c8 _path[512];
        rde_snprintf(_path, sizeof(_path), "%smlkit_samples.txt", fude_save_dir());
        const f64 _ms   = (rde_engine_get_time_now() - _asked_at) * 1000.0;
        RDE_TRY({
            rde_file* _file = rde_file_open(_path, fude_study_look.sample_next == 0 ? RDE_FILE_MODE_WRITE : RDE_FILE_MODE_APPEND);
            if(_file != NULL && !rde_failed()) {
                rde_file_write_fmt(_file, "strokes %u-%u (%.0f ms):\n", _first, _last, _ms);
                for(u32 _l = 0; _l < _reading.line_count; _l++) {
                    rde_file_write_fmt(_file, "%s\n", _reading.lines[_l]);
                }
                rde_file_write_fmt(_file, "\n");
                rde_file_close(_file);
            }
        });
        rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit, strokes %u-%u (%.0f ms): %s", _first, _last, _ms, _reading.line_count > 0 ? _reading.lines[0] : "");
        fude_study_look.sample_next++;
        fude_study_look.sample_asked = false;
    }
}

// --translate-probe=xx: translates a few sentences into xx with ML Kit's translator on the device itself, downloading
// its models when they are missing, and writes the answers with their times to <save dir>/translate.txt. It gives up
// after three minutes at most. The app then carries on as usual, because an iOS app cannot quit itself. The sentences
// are Japanese, so it runs only in an app that teaches Japanese; in any other, the flag is logged and dropped.
RDE_INTERNAL void fude_study_look_translate_probe(void) {
    static const c8* const _sentences[4] = { "今日は日本語を勉強します。", "駅はどこですか？", "この本はとても面白かったです。", "憂鬱" };
    static f64 _start, _ready_at, _sent_at;
    static u32 _tickets[4], _got;
    static b8  _sent;
    static c8  _answers[4][FUDE_TRANSLATE_TEXT];
    static f64 _took[4];
    if(rde_strcmp(fude_lang_code(), "ja") != 0) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "translate probe: its sentences are Japanese, and this app teaches %s: not run", fude_lang_code());
        fude_study_look.translate_probe = NULL;
        return;
    }
    const c8*  _to  = fude_study_look.translate_probe;
    const f64  _now = rde_engine_get_time_now();
    if(_start == 0.0) {
        _start = _now;
        fude_translate_prepare(fude_lang_code(), _to);
    }
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_state(fude_lang_code(), _to);
    if(_state == FUDE_TRANSLATE_READY && !_sent) {
        _sent     = true;
        _ready_at = _sent_at = _now;
        for(u32 _i = 0; _i < 4u; _i++) {
            _tickets[_i] = fude_translate_text(_sentences[_i], fude_lang_code(), _to);
        }
    }
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_tickets[_i] != 0u && fude_translate_take(_tickets[_i], _answers[_i], sizeof(_answers[_i]))) {
            _took[_i] = _now - _sent_at;
            _got++;
        }
    }
    if((_sent && _got == 4u) || _state == FUDE_TRANSLATE_FAILED || _state == FUDE_TRANSLATE_UNAVAILABLE || _now - _start > 180.0) {
        c8 _path[RDE_MAX_PATH];
        rde_snprintf(_path, sizeof(_path), "%stranslate.txt", fude_save_dir());
        RDE_TRY({
            rde_file* _f = rde_file_open(_path, RDE_FILE_MODE_APPEND);
            if(_f != NULL && !rde_failed()) {
                rde_file_write_fmt(_f, "ja -> %s: state %d, ready after %.1f s, %u of 4 answered\n", _to, (i32)_state, _ready_at > 0.0 ? _ready_at - _start : -1.0, _got);
                for(u32 _i = 0; _i < 4u; _i++) {
                    rde_file_write_fmt(_f, "  %s -> %s (%.0f ms)\n", _sentences[_i], _answers[_i], 1000.0 * _took[_i]);
                }
                rde_file_close(_f);
            }
        });
        rde_log_level(RDE_LOG_LEVEL_INFO, "translate probe: written to %s", _path);
        fude_study_look.translate_probe = NULL;
    }
}

// Once a frame, after fude_look_frame: runs the probes, and does what the flags ask for at the frames of a shot's
// sequence they ask for (10, 12, 20, 28 and 30).
void fude_study_look_frame(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    if(fude_study_look.translate_probe != NULL) {
        fude_study_look_translate_probe();
    }
    fude_study_look_samples(_app);
    const u32 _frame = fude_look_shot_frame();
    if(_frame == 12u && fude_study_look.voice_card && _study->welcome != NULL) {
        fude_welcome_voice_as(_study->welcome, fude_study_look.voice_android);
    }
    if(_frame == 20u && fude_study_look.paste_text != NULL) {
        fude_study_write_text(_app, fude_study_look.paste_text, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
    }
    if(_frame == 20u && fude_study_look.sheet != NULL && _study->viewer->open && rde_arr_length(&_study->viewer->list) > 0) {
        fude_study_sheet_write(_app, &((const u32*)_study->viewer->list.memory)[_study->viewer->position], 1u, false, fude_study_look.sheet, false);
    }
    if(_frame == 28u && fude_study_look.translate_sel) {
        fude_pagetext_translate(&_study->text);
    }
    if(_frame == 28u && fude_study_look.save_sel) {
        fude_pagetext_save_word(&_study->text);
    }
    if(_frame == 28u && fude_study_look.word_card != NULL) {
        fude_wordcard_ask_read(_study->db, fude_study_look.word_card);
        u32             _w;
        fude_kanji_word _kw;
        if(fude_study_look.word_sentence != NULL && fude_wordsplit(_study->db, fude_study_look.word_card, &_w, 1u) == 1u && fude_kanji_word_at(_study->db, _w, &_kw)) {
            c8          _ja[FUDE_VOCAB_SENTENCE];
            const c8*   _bar = rde_strchr(fude_study_look.word_sentence, '|');
            const usize _n   = _bar != NULL ? (usize)(_bar - fude_study_look.word_sentence) : rde_strlen(fude_study_look.word_sentence);
            rde_snprintf(_ja, sizeof(_ja), "%.*s", (i32)_n, fude_study_look.word_sentence);
            fude_wordcard_ask_in(_kw.written, _kw.reading, _kw.meaning, _ja, _bar != NULL ? _bar + 1 : "");
        }
    }
    if(_frame == 28u && fude_study_look.word_translate != NULL) {
        fude_wordcard_ask("", "", fude_study_look.word_translate, 0u);
    }
    if(_frame == 30u && fude_study_look.word_translate != NULL) {
        fude_wordcard_translate(_app->ui);
    }
    if(_frame == 30u && fude_study_look.scroll > 0.0f) {
        _study->stats->scroller.offset      = fude_study_look.scroll;
        _study->album->scroller.offset      = fude_study_look.scroll;
        _study->album->page_scroller.offset = fude_study_look.scroll;
        _study->scan->taps.offset           = fude_study_look.scroll;   // the panel of translations
        rde_ui_scroll_area_set_scroll(_app->ui->side.settings_body, (rde_vec_2F){ 0.0f, fude_study_look.scroll });   // Settings, on a phone
    }
    if(_frame == 10u && fude_study_look.welcome >= 0) {
        fude_welcome_open(_study->welcome);
        _study->welcome->page = (u32)fude_study_look.welcome < FUDE_WELCOME_PAGES ? (u32)fude_study_look.welcome : 0u;
    }
    if(_frame == 10u && fude_study_look.offline) {
        fude_welcome_offline(_study->welcome);
    }
}
