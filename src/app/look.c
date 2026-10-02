#include "app/look.h"
#include "app/app.h"
#include "app/ui.h"
#include "app/session.h"
#include "base/save.h"
#include "base/theme.h"
#include "base/text.h"
#include "study/vocab.h"
#include "study/charnote.h"
#include "handwriting/recognize.h"
#include "services/mlkit.h"
#include "services/translate.h"
#include "services/textscan.h"
#include "widgets/wordcard.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See look.h.
// ===========================================================================

#define KANA_LOOK_SAMPLES 32

RDE_INTERNAL struct {
    i32       argc;
    c8**      argv;
    // Wanted once the saves are loaded, or some frames in (a shot's sequence).
    const c8* shot;
    u32       frames;
    b8        paper, stats, vocab_sample, deselect, trim_fonts, data, data_replace, save_sel, translate_sel, scan_live, scan_rows, scan_translate;
    i32       vocab, word_exam, welcome, theme;
    i64       kept_exam;
    f32       scroll, scan_turn;
    const c8* note;
    const c8* paste_text;
    const c8* word_card;
    const c8* scan_demo;
    const c8* data_export;
    const c8* data_import;
    const c8* sheet;
    const c8* translate_probe;
    const c8* press;      // --press=I,J,...: the top screen's row's buttons pressed in turn, from frame 30
    i32       language;   // --language=N: Kana's Nth language (text.h), chosen at frame 12
    // --mlkit-samples: ranges of the page's strokes, each read by ML Kit in turn.
    u32       sample_first[KANA_LOOK_SAMPLES];
    u32       sample_last[KANA_LOOK_SAMPLES];
    u32       sample_count, sample_next;
    b8        sample_asked;
    // --perf: frame times over this many seconds (0: off).
    f64       perf_seconds, perf_start;
    u32       perf_frames, perf_settle, perf_slow;
    f64       perf_dt_sum, perf_dt_max, perf_update_sum, perf_update_max, perf_render_sum, perf_render_max;
    c8        perf_label[160];
} kana_look = { .vocab = -1, .word_exam = -1, .welcome = -1, .theme = -1, .kept_exam = -1, .language = -1 };

// The flag's value when _arg is _flag=value; NULL otherwise.
RDE_INTERNAL const c8* kana_look_value(const c8* _arg, const c8* _flag) {
    const usize _n = strlen(_flag);
    return _arg != NULL && strncmp(_arg, _flag, _n) == 0 && _arg[_n] == '=' ? _arg + _n + 1 : NULL;
}

RDE_INTERNAL b8 kana_look_is(const c8* _arg, const c8* _flag) {
    return _arg != NULL && strcmp(_arg, _flag) == 0;
}

void kana_look_args(i32 _argc, c8** _argv) {
    kana_look.argc = _argc;
    kana_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _a = _argv[_i];
        const c8* _v;
        // Translate with Google pretends (debug builds): known before the UI is built.
        if(kana_look_is(_a, "--scan-demo-translate")) { kana_translate_demo(true); kana_look.scan_translate = true; }
        if(kana_look_is(_a, "--translate-selection")) { kana_translate_demo(true); kana_look.translate_sel = true; }
        if(kana_look_is(_a, "--save-selection"))      { kana_look.save_sel = true; }
        if(kana_look_is(_a, "--paper"))               { kana_look.paper = true; }
        if(kana_look_is(_a, "--scan-demo-top-first")) { kana_look.scan_rows = true; }
        if(kana_look_is(_a, "--scan-live"))           { kana_look.scan_live = true; }
        if(kana_look_is(_a, "--deselect"))            { kana_look.deselect = true; }
        if(kana_look_is(_a, "--trim-fonts"))          { kana_look.trim_fonts = true; }
        if(kana_look_is(_a, "--data"))                { kana_look.data = true; }
        if(kana_look_is(_a, "--data-replace"))        { kana_look.data_replace = true; }
        if(kana_look_is(_a, "--stats"))               { kana_look.stats = true; }
        if(kana_look_is(_a, "--vocab-sample"))        { kana_look.vocab_sample = true; }
        if(kana_look_is(_a, "--vocab"))               { kana_look.vocab = 0; }
        if(kana_look_is(_a, "--word-exam"))           { kana_look.word_exam = 0; }
        if(kana_look_is(_a, "--welcome"))             { kana_look.welcome = 0; }
        if((_v = kana_look_value(_a, "--vocab")) != NULL)          { kana_look.vocab = (i32)strtol(_v, NULL, 10); }
        if((_v = kana_look_value(_a, "--word-exam")) != NULL)      { kana_look.word_exam = (i32)strtol(_v, NULL, 10); }
        if((_v = kana_look_value(_a, "--welcome")) != NULL)        { kana_look.welcome = (i32)strtol(_v, NULL, 10) - 1; }
        if((_v = kana_look_value(_a, "--theme")) != NULL)          { kana_look.theme = (i32)strtol(_v, NULL, 10); }
        if((_v = kana_look_value(_a, "--kept-exam")) != NULL)      { kana_look.kept_exam = (i64)strtoul(_v, NULL, 10); }
        if((_v = kana_look_value(_a, "--scroll")) != NULL)         { kana_look.scroll = strtof(_v, NULL); }
        if((_v = kana_look_value(_a, "--scan-demo-turn")) != NULL) { kana_look.scan_turn = strtof(_v, NULL); }
        if((_v = kana_look_value(_a, "--shot")) != NULL)           { kana_look.shot = _v; }
        if((_v = kana_look_value(_a, "--note")) != NULL)           { kana_look.note = _v; }
        if((_v = kana_look_value(_a, "--word-card")) != NULL)      { kana_look.word_card = _v; }
        if((_v = kana_look_value(_a, "--scan-demo")) != NULL)      { kana_look.scan_demo = _v; }
        if((_v = kana_look_value(_a, "--data-export")) != NULL)    { kana_look.data_export = _v; }
        if((_v = kana_look_value(_a, "--data-import")) != NULL)    { kana_look.data_import = _v; }
        if((_v = kana_look_value(_a, "--sheet")) != NULL)          { kana_look.sheet = _v; }
        if((_v = kana_look_value(_a, "--translate-probe")) != NULL) { kana_look.translate_probe = _v; }
        if((_v = kana_look_value(_a, "--press")) != NULL)          { kana_look.press = _v; }
        if((_v = kana_look_value(_a, "--language")) != NULL)       { kana_look.language = (i32)strtol(_v, NULL, 10); }
        if((_v = kana_look_value(_a, "--paste-text")) != NULL) {
            // "\\n" for a line break: RDE reads its arguments as config lines.
            static c8 _pasted[512];
            usize     _n = 0;
            for(const c8* _p = _v; *_p != 0 && _n + 1u < sizeof(_pasted); _p++) {
                if(_p[0] == '\\' && _p[1] == 'n') { _pasted[_n++] = '\n'; _p++; }
                else                               { _pasted[_n++] = *_p; }
            }
            _pasted[_n]          = 0;
            kana_look.paste_text = _pasted;
        }
        if((_v = kana_look_value(_a, "--mlkit-samples")) != NULL) {
            const c8* _p = _v;
            while(*_p != 0 && kana_look.sample_count < KANA_LOOK_SAMPLES) {
                c8* _end = NULL;
                kana_look.sample_first[kana_look.sample_count] = (u32)strtoul(_p, &_end, 10);
                kana_look.sample_last[kana_look.sample_count]  = *_end == '-' ? (u32)strtoul(_end + 1, &_end, 10) : kana_look.sample_first[kana_look.sample_count];
                kana_look.sample_count++;
                _p = *_end == ',' ? _end + 1 : _end;
                if(*_end == 0) { break; }
            }
        }
        if((_v = kana_look_value(_a, "--perf")) != NULL) {
            kana_look.perf_seconds = strtod(_v, NULL);
            for(i32 _k = 1; _k < _argc; _k++) {
                if(_argv[_k] != NULL && strncmp(_argv[_k], "--perf=", 7) != 0 && strlen(kana_look.perf_label) + strlen(_argv[_k]) + 2u < sizeof(kana_look.perf_label)) {
                    strcat(kana_look.perf_label, _argv[_k]);
                    strcat(kana_look.perf_label, " ");
                }
            }
        }
    }
}

// The record of the character at _hex (a code point), when the data has it.
RDE_INTERNAL b8 kana_look_record(const kana_app* _app, const c8* _hex, u32* _record) {
    return _app->db != NULL && kana_kanji_find_index(_app->db, (u32)strtoul(_hex, NULL, 16), _record);
}

void kana_look_start(kana_app* _app) {
    for(i32 _i = 1; _i < kana_look.argc; _i++) {
        const c8* _a = kana_look.argv[_i];
        const c8* _v;
        u32       _record = 0;
        if(kana_look_is(_a, "--settings"))   { kana_side_open_settings(_app->ui, -1); }
        if(kana_look_is(_a, "--side"))       { _app->ui->side.open = true; }
        if(kana_look_is(_a, "--album"))      { kana_album_open(_app->album); }
        if(kana_look_is(_a, "--browse"))     { kana_browse_open(_app->browse); }
        if(kana_look_is(_a, "--kana"))       { kana_chart_open(_app->chart); }
        if(kana_look_is(_a, "--album-exams")) { kana_album_open(_app->album); kana_album_set_view(_app->album, KANA_ALBUM_VIEW_EXAMS); }
        if((_v = kana_look_value(_a, "--licences")) != NULL)   { kana_side_open_settings(_app->ui, (i32)strtol(_v, NULL, 10)); }
        if((_v = kana_look_value(_a, "--album-page")) != NULL) { kana_album_open(_app->album); kana_album_open_page(_app->album, (u32)strtoul(_v, NULL, 16)); }
        if(kana_look_value(_a, "--kept-exam") != NULL)         { kana_album_open(_app->album); kana_album_set_view(_app->album, KANA_ALBUM_VIEW_EXAMS); }
        if((_v = kana_look_value(_a, "--viewer")) != NULL)     { kana_viewer_show_codepoint(_app->viewer, (u32)strtoul(_v, NULL, 16)); }
        if((_v = kana_look_value(_a, "--practice")) != NULL && kana_look_record(_app, _v, &_record)) { kana_practice_open(_app->practice, _record); }
        if((_v = kana_look_value(_a, "--guided")) != NULL && kana_look_record(_app, _v, &_record)) {
            kana_practice_open(_app->practice, _record);
            kana_practice_set_guided(_app->practice, true);
        }
        if((_v = kana_look_value(_a, "--size")) != NULL) {
            c8*       _end = NULL;
            const i32 _w   = (i32)strtol(_v, &_end, 10);
            const i32 _h   = _end != NULL && *_end == 'x' ? (i32)strtol(_end + 1, NULL, 10) : 0;
            if(_w > 0 && _h > 0) {
                rde_window_set_size(_app->window, (rde_vec_2I){ _w, _h });
            }
        }
        if(kana_look_is(_a, "--exam") || kana_look_is(_a, "--exam-start")) {
            kana_exam_open(_app->exam);
            if(kana_look_is(_a, "--exam-start")) {
                _app->exam->source = KANA_EXAM_SOURCE_N5;
                kana_exam_preview(_app->exam);
                kana_exam_start(_app->exam);
            }
        }
    }
}

// --scan-demo=PNG: Text from a photo on that image (its size read from its
// header), four lines across it, the second left out: the screen as a photo
// read would show it.
RDE_INTERNAL void kana_look_scan_demo(kana_app* _app) {
    kana_scan* _scan = _app->scan;
    kana_scan_open(_scan, _app->window, kana_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));   // no such file: the screen empty
    FILE* _f = fopen(kana_look.scan_demo, "rb");
    if(_f == NULL) {
        return;
    }
    static u8   _png[8u << 20];
    const usize _n = fread(_png, 1, sizeof(_png), _f);
    fclose(_f);
    static kana_textscan_line _demo[4];
    const c8* const _texts[4] = { "今日は、日本語を", "勉強します。", "Hello 漢字", "とかな" };
    for(u32 _l = 0; _l < 4u; _l++) {
        snprintf(_demo[_l].text, sizeof(_demo[_l].text), "%s", _texts[_l]);
        const f32 _y0 = 288.0f + 90.0f * (f32)_l, _y1 = _y0 + 64.0f;
        _demo[_l].corners[0] = (rde_vec_2F){ 86.0f, _y0 };
        _demo[_l].corners[1] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y0 };
        _demo[_l].corners[2] = (rde_vec_2F){ _l == 3u ? 300.0f : 660.0f, _y1 };
        _demo[_l].corners[3] = (rde_vec_2F){ 86.0f, _y1 };
        _demo[_l].block      = _l < 2u ? 0u : 1u;
    }
    const kana_textscan_result _r = { _png, _n, ((u32)_png[16] << 24) | ((u32)_png[17] << 16) | ((u32)_png[18] << 8) | _png[19],
                                      ((u32)_png[20] << 24) | ((u32)_png[21] << 16) | ((u32)_png[22] << 8) | _png[23], _demo, 4u };
    kana_scan_show(_scan, &_r);
    ((kana_scan_line*)_scan->lines.memory)[1].kept = false;
    if(kana_look.scan_translate) {
        kana_scan_translate(_scan);
    }
    _scan->shown_top_first = kana_look.scan_rows;
    if(kana_look.scan_turn != 0.0f) {
        // The file holds the picture turned the other way: shown as a frame is.
        _scan->shown_rotation = kana_look.scan_turn;
        if(fmodf(fabsf(kana_look.scan_turn), 180.0f) > 45.0f) {
            const u32 _w = _scan->picture_w;
            _scan->picture_w = _scan->picture_h;
            _scan->picture_h = _w;
        }
    }
}

void kana_look_loaded(kana_app* _app) {
    if(kana_look.theme >= 0) {
        kana_theme_set((KANA_THEME_)kana_look.theme);
        kana_ui_apply_theme(_app->ui);
        kana_session_settings_seen(_app);   // a look, not a change to save
    }
    if(kana_look.stats) {
        kana_stats_open(_app->stats);
    }
    if(kana_look.note != NULL && _app->viewer->open) {
        kana_charnote_set(kana_viewer_codepoint(_app->viewer), kana_look.note);
    }
    // --vocab-sample: a few words and a list, when the vocabulary is empty.
    if(kana_look.vocab_sample && kana_vocab_count() == 0u) {
        static const c8* const _sample[][3] = {
            { "日本語", "にほんご", "Japanese (language)" }, { "勉強", "べんきょう", "study" }, { "食べる", "たべる", "to eat" }, { "東京", "とうきょう", "Tokyo" },
            { "本", "ほん", "book; volume; script" }, { "先生", "せんせい", "teacher; instructor; master" }, { "学生", "がくせい", "student" }, { "読む", "よむ", "to read" },
        };
        const u32 _list = kana_vocab_list_add("Lesson 1");
        for(u32 _w = 0; _w < sizeof(_sample) / sizeof(_sample[0]); _w++) {
            const u32 _id = kana_vocab_add(_sample[_w][0], _sample[_w][1], _sample[_w][2], 0u);
            if(_w % 2u == 0u) {
                kana_vocab_set_in_list(_list, _id, true);
            }
        }
        kana_vocab_list_add("Food");
    }
    // --word-exam[=STAGE]: a word exam of every word (0 its setup, 1 writing, 2 by ear, 3 its results).
    if(kana_look.word_exam >= 0) {
        const u32 _n   = kana_vocab_count();
        u32*      _ids = (u32*)rde_malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
        for(u32 _i = 0; _i < _n; _i++) {
            _ids[_i] = kana_vocab_at(_i)->id;
        }
        kana_wordexam_open(_app->wordexam, _ids, _n, kana_text(KANA_TEXT_VOCAB));
        rde_free(_ids);
        if(kana_look.word_exam >= 1) {
            _app->wordexam->by = kana_look.word_exam == 2 ? KANA_WORDEXAM_BY_EAR : KANA_WORDEXAM_BY_MEANING;
            kana_wordexam_start(_app->wordexam);
        }
        for(u32 _i = 0; kana_look.word_exam == 3 && _i < KANA_WORDEXAM_MAX && _app->wordexam->stage == KANA_WORDEXAM_WRITING; _i++) {
            kana_wordexam_next(_app->wordexam);   // nothing written: the results, every word wrong
        }
    }
    if(kana_look.vocab >= 0) {
        kana_vocabview_open(_app->vocab);
        kana_vocabview_show_list(_app->vocab, kana_look.vocab > 0 ? kana_vocab_list_at((u32)kana_look.vocab - 1u) : 0u);
    }
    if(kana_look.kept_exam >= 0) {
        kana_exam_open_kept(_app->exam, (u32)kana_look.kept_exam);
    }
    if(kana_look.scan_live) {
        kana_scan_open(_app->scan, _app->window, kana_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
        kana_scan_camera(_app->scan);
    }
    if(kana_look.scan_demo != NULL) {
        kana_look_scan_demo(_app);
    }
}

// --mlkit-samples: the next range to ML Kit (recognize.h), and its answer
// written to <save dir>/mlkit_samples.txt (copied off the device to compare).
RDE_INTERNAL void kana_look_samples(kana_app* _app) {
    static kana_recognition _reading;
    static f64              _asked_at;
    if(kana_look.sample_next >= kana_look.sample_count) {
        return;
    }
    if(kana_mlkit_state() == KANA_MLKIT_FAILED) {
        kana_mlkit_prepare();
    }
    const u32 _first = kana_look.sample_first[kana_look.sample_next];
    const u32 _last  = kana_look.sample_last[kana_look.sample_next];
    if(!kana_look.sample_asked) {
        kana_ink _one;
        kana_ink_init(&_one);
        for(u32 _s = _first; _s <= _last && _s < kana_ink_stroke_count(_app->ink); _s++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(_app->ink, _s);
            kana_ink_add_loaded_stroke(&_one, kana_ink_stroke_points(_app->ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        }
        kana_look.sample_asked = kana_recognize_start(&_reading, &_one);
        _asked_at              = rde_engine_get_time_now();
        kana_ink_destroy(&_one);
        return;
    }
    if(kana_recognize_poll(&_reading)) {
        c8 _path[512];
        snprintf(_path, sizeof(_path), "%smlkit_samples.txt", kana_save_dir());
        const f64 _ms   = (rde_engine_get_time_now() - _asked_at) * 1000.0;
        FILE*     _file = fopen(_path, kana_look.sample_next == 0 ? "wb" : "ab");
        if(_file != NULL) {
            fprintf(_file, "strokes %u-%u (%.0f ms):\n", _first, _last, _ms);
            for(u32 _l = 0; _l < _reading.line_count; _l++) {
                fprintf(_file, "%s\n", _reading.lines[_l]);
            }
            fprintf(_file, "\n");
            fclose(_file);
        }
        rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit, strokes %u-%u (%.0f ms): %s", _first, _last, _ms, _reading.line_count > 0 ? _reading.lines[0] : "");
        kana_look.sample_next++;
        kana_look.sample_asked = false;
    }
}

// --translate-probe=xx: Japanese into xx with ML Kit's translator, on the device
// itself — its models downloaded when missing, then a few sentences — written to
// <save dir>/translate.txt with the times. Three minutes at most. The app then
// carries on as usual: an iOS app cannot quit itself.
RDE_INTERNAL void kana_look_translate_probe(void) {
    static const c8* const _sentences[4] = { "今日は日本語を勉強します。", "駅はどこですか？", "この本はとても面白かったです。", "憂鬱" };
    static f64 _start, _ready_at, _sent_at;
    static u32 _tickets[4], _got;
    static b8  _sent;
    static c8  _answers[4][KANA_TRANSLATE_TEXT];
    static f64 _took[4];
    const c8*  _to  = kana_look.translate_probe;
    const f64  _now = rde_engine_get_time_now();
    if(_start == 0.0) {
        _start = _now;
        kana_translate_prepare(_to);
    }
    const KANA_TRANSLATE_STATE_ _state = kana_translate_state(_to);
    if(_state == KANA_TRANSLATE_READY && !_sent) {
        _sent     = true;
        _ready_at = _sent_at = _now;
        for(u32 _i = 0; _i < 4u; _i++) {
            _tickets[_i] = kana_translate_text(_sentences[_i], _to);
        }
    }
    u32 _ticket;
    c8  _out[KANA_TRANSLATE_TEXT];
    while(kana_translate_poll(&_ticket, _out, sizeof(_out))) {
        for(u32 _i = 0; _i < 4u; _i++) {
            if(_tickets[_i] != 0u && _tickets[_i] == _ticket) {
                snprintf(_answers[_i], sizeof(_answers[_i]), "%s", _out);
                _took[_i] = _now - _sent_at;
                _got++;
            }
        }
    }
    if((_sent && _got == 4u) || _state == KANA_TRANSLATE_FAILED || _state == KANA_TRANSLATE_UNAVAILABLE || _now - _start > 180.0) {
        c8 _path[RDE_MAX_PATH];
        snprintf(_path, sizeof(_path), "%stranslate.txt", kana_save_dir());
        FILE* _f = fopen(_path, "a");
        if(_f != NULL) {
            fprintf(_f, "ja -> %s: state %d, ready after %.1f s, %u of 4 answered\n", _to, (i32)_state, _ready_at > 0.0 ? _ready_at - _start : -1.0, _got);
            for(u32 _i = 0; _i < 4u; _i++) {
                fprintf(_f, "  %s -> %s (%.0f ms)\n", _sentences[_i], _answers[_i], 1000.0 * _took[_i]);
            }
            fclose(_f);
        }
        rde_log_level(RDE_LOG_LEVEL_INFO, "kana translate probe: written to %s", _path);
        kana_look.translate_probe = NULL;
    }
}

// The frame the shot is taken at: 45, or after the last press has settled.
RDE_INTERNAL u32 kana_look_shot_frame(void) {
    u32 _presses = 0;
    for(const c8* _p = kana_look.press; _p != NULL && *_p != 0; _p = strchr(_p, ',') != NULL ? strchr(_p, ',') + 1 : NULL) {
        _presses++;
    }
    const u32 _last = 30u + 4u * _presses + 10u;
    return _presses > 0 && _last > 45u ? _last : 45u;
}

void kana_look_frame(kana_app* _app) {
    if(kana_look.translate_probe != NULL) {
        kana_look_translate_probe();
    }
    kana_look_samples(_app);
    // --paper: the paper panel, open once the bar has been laid out.
    if(kana_look.paper && kana_look.frames == 20u) {
        kana_toolbar_set_paper_open(&_app->ui->bar, true);
        kana_look.paper = false;
    }
    if(kana_look.shot == NULL) {
        kana_look.frames += kana_look.paper ? 1u : 0u;
        return;
    }
    const u32 _frame = ++kana_look.frames;
    kana_pagemenu* _page = &_app->ui->page;
    if(_frame == 10u && kana_look.welcome >= 0) {
        kana_welcome_open(_app->welcome);
        _app->welcome->page = (u32)kana_look.welcome < KANA_WELCOME_PAGES ? (u32)kana_look.welcome : 0u;
    }
    if(_frame == 12u && kana_look.language >= 0 && (u32)kana_look.language < KANA_TEXT_LANGUAGES) {
        kana_text_set_language(KANA_TEXT_LANGUAGE_LIST[kana_look.language].language);
    }
    // --press: one button every four frames from frame 30 (the UI catching up between).
    if(kana_look.press != NULL && _frame >= 30u && (_frame - 30u) % 4u == 0u) {
        const c8* _p = kana_look.press;
        for(u32 _k = 0; _k < (_frame - 30u) / 4u && _p != NULL; _k++) {
            _p = strchr(_p, ',');
            _p = _p != NULL ? _p + 1 : NULL;
        }
        if(_p != NULL && *_p != 0 && !kana_ui_press(_app->ui, (u32)strtoul(_p, NULL, 10))) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "kana look: --press %s: no such button on the screen on top", _p);
        }
    }
    if(_frame == 15u && kana_look.data) {
        kana_side_open_settings(_app->ui, -1);
        _app->ui->side.data_open = true;
    }
    if(_frame == 20u && kana_look.paste_text != NULL) {
        kana_app_write_text(_app, kana_look.paste_text, kana_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
    }
    if(_frame == 20u && kana_look.data_export != NULL) {
        kana_session_export_to(_app, kana_look.data_export, false);
    }
    if(_frame == 20u && kana_look.sheet != NULL && _app->viewer->open && rde_arr_length(&_app->viewer->list) > 0) {
        kana_session_sheet(_app, &((const u32*)_app->viewer->list.memory)[_app->viewer->position], 1u, false, kana_look.sheet, false);
    }
    if(_frame == 22u && kana_look.data_import != NULL) {
        kana_session_import_pick(_app, kana_look.data_import);
    }
    if(_frame == 25u && kana_look.deselect) {
        kana_lasso_clear(_app->lasso, _app->ink);
    }
    if(_frame == 26u && kana_look.data_replace) {
        _app->ui->side.data_request = KANA_SIDE_DATA_REPLACE;
    }
    if(_frame == 28u && kana_look.translate_sel) {
        kana_pagemenu_translate(_page);
    }
    if(_frame == 28u && kana_look.save_sel) {
        kana_pagemenu_save_word(_page);
    }
    if(_frame == 28u && kana_look.word_card != NULL) {
        kana_wordcard_ask_read(_app->db, kana_look.word_card);
    }
    if(_frame == 30u && kana_look.scroll > 0.0f) {
        _app->stats->scroller.offset      = kana_look.scroll;
        _app->album->scroller.offset      = kana_look.scroll;
        _app->album->page_scroller.offset = kana_look.scroll;
        _app->scan->taps.offset           = kana_look.scroll;   // the panel of translations
    }
    if(_frame == 40u && kana_look.trim_fonts) {
        kana_ui_trim_fonts(_app->ui);   // the shot then shows every glyph uploaded again
    }
    if(_frame == kana_look_shot_frame()) {
        rde_window_take_screenshot(_app->window, (rde_vec_2I){ 0, 0 }, (rde_vec_2I){ 0, 0 }, kana_look.shot, NULL);   // the next frame, whole
    } else if(_frame == kana_look_shot_frame() + 5u) {
        rde_engine_set_running(false);
    }
}

// --- --perf ---------------------------------------------------------------------------------

// The time is up: the summary, once.
RDE_INTERNAL void kana_look_perf_write(kana_app* _app, f64 _now) {
    const f64 _n    = kana_look.perf_frames > 0 ? (f64)kana_look.perf_frames : 1.0;
    const f64 _span = _now - kana_look.perf_start;
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%sperf.txt", kana_save_dir());
    FILE* _f = fopen(_path, "a");
    if(_f != NULL) {
        fprintf(_f, "%s| %u frames in %.1f s: %.1f fps, frame %.2f ms avg, %.2f ms worst, %u over 20 ms | update %.2f ms avg, %.2f worst | render %.2f ms avg, %.2f worst",
                kana_look.perf_label, kana_look.perf_frames, _span, _n / _span, 1000.0 * kana_look.perf_dt_sum / _n, 1000.0 * kana_look.perf_dt_max, kana_look.perf_slow,
                1000.0 * kana_look.perf_update_sum / _n, 1000.0 * kana_look.perf_update_max, 1000.0 * kana_look.perf_render_sum / _n, 1000.0 * kana_look.perf_render_max);
        const kana_scan* _scan = _app->scan;
        if(_scan->open) {   // the camera: frames shown, and ML Kit's reads of them
            fprintf(_f, " | camera %u frames shown, %u read, %.0f ms a read", _scan->frames_shown, _scan->frames_read,
                    _scan->frames_read > 0 ? 1000.0 * _scan->read_seconds / (f64)_scan->frames_read : 0.0);
        }
        fprintf(_f, "\n");
        fclose(_f);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana perf: %u frames, %.2f ms avg, %.2f ms worst (written to %s)", kana_look.perf_frames,
                  1000.0 * kana_look.perf_dt_sum / _n, 1000.0 * kana_look.perf_dt_max, _path);
    kana_look.perf_seconds = 0.0;
}

void kana_look_timed_update(void (*_update)(f32), f32 _dt) {
    if(kana_look.perf_seconds <= 0.0) {
        _update(_dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    if(kana_look.perf_start <= 0.0) {
        kana_look.perf_start = kana_look.perf_settle++ >= 60u ? _t0 : 0.0;   // a second to settle first
    } else {
        kana_look.perf_frames++;
        kana_look.perf_dt_sum += (f64)_dt;
        kana_look.perf_dt_max  = (f64)_dt > kana_look.perf_dt_max ? (f64)_dt : kana_look.perf_dt_max;
        kana_look.perf_slow   += _dt > 0.020f ? 1u : 0u;
    }
    _update(_dt);
    if(kana_look.perf_start > 0.0 && kana_look.perf_frames > 0) {
        const f64 _t = rde_engine_get_time_now() - _t0;
        kana_look.perf_update_sum += _t;
        kana_look.perf_update_max  = _t > kana_look.perf_update_max ? _t : kana_look.perf_update_max;
    }
}

void kana_look_timed_render(kana_app* _app, void (*_render)(rde_window*, f32), rde_window* _window, f32 _dt) {
    if(kana_look.perf_seconds <= 0.0) {
        _render(_window, _dt);
        return;
    }
    const f64 _t0 = rde_engine_get_time_now();
    _render(_window, _dt);
    if(kana_look.perf_start > 0.0 && kana_look.perf_frames > 0) {
        const f64 _now = rde_engine_get_time_now();
        kana_look.perf_render_sum += _now - _t0;
        kana_look.perf_render_max  = _now - _t0 > kana_look.perf_render_max ? _now - _t0 : kana_look.perf_render_max;
        if(_now - kana_look.perf_start >= kana_look.perf_seconds) {
            kana_look_perf_write(_app, _now);
        }
    }
}
