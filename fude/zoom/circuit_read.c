// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, a part's text read: its values ("4.7k", "100 uF", "5V 50Hz"), the words it may hold
// (whole words, in any of the apps' languages: an LED's colour, "on"), the sensors' curves (an LDR's, a thermistor's),
// and each model's numbers and starting state from its text (fzc_read).
//
// The file is in two parts: values and words (fude_zoom_circuit_value, fude_zoom_circuit_word, the LED colours and
// their words in every language); and a part's text read for the build (fzc_read, with fzc_says and fzc_same_name; its
// steps: a component's numbers, a module's, a display's or a sensor's, a fresh part's switches and drives; what a part
// holds beyond its state; fzc_text_of, a symbol's whole text).
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// --- values --------------------------------------------------------------------------------------------

// See the header: 20 kΩ at 10 lux, falling as lux to the power 0.7, held from 100 Ω to 1 MΩ.
f64 fude_zoom_circuit_ldr_ohms(f64 _lux) {
    return rde_math_min_f64(rde_math_max_f64(20000.0 * rde_math_pow_f64(rde_math_max_f64(_lux, 1e-3) / 10.0, -0.7), 100.0), 1e6);
}

// See the header: the B equation, R25 · e^(B (1/T − 1/298.15 K)).
f64 fude_zoom_circuit_ntc_ohms(f64 _r25, f64 _beta, f64 _celsius) {
    const f64 _t = rde_math_max_f64(_celsius + 273.15, 1.0);
    return _r25 * rde_math_exp_f64(_beta * (1.0 / _t - 1.0 / 298.15));
}

// See the header.
b8 fude_zoom_circuit_lit(u8 _model) {
    return _model == FUDE_ZOOM_MODEL_LDR || _model == FUDE_ZOOM_MODEL_PHOTODIODE || _model == FUDE_ZOOM_MODEL_PHOTOTRANSISTOR;
}

// See the header. Light: 1 to 100 000 lux on a log scale; heat: −20 to 120 °C.
f64 fude_zoom_circuit_sensor_along(u8 _model, f64 _value) {
    if(fude_zoom_circuit_lit(_model)) {
        return rde_math_min_f64(rde_math_max_f64(rde_math_log10_f64(rde_math_max_f64(_value, 1.0)) / 5.0, 0.0), 1.0);
    }
    return rde_math_min_f64(rde_math_max_f64((_value + 20.0) / 140.0, 0.0), 1.0);
}

// See the header: fude_zoom_circuit_sensor_along the other way.
f64 fude_zoom_circuit_sensor_at(u8 _model, f64 _along) {
    _along = rde_math_min_f64(rde_math_max_f64(_along, 0.0), 1.0);
    if(fude_zoom_circuit_lit(_model)) {
        return rde_math_pow_f64(10.0, 5.0 * _along);
    }
    return -20.0 + 140.0 * _along;
}

// See the header. Each number in the text counts, whatever is round it ("1N4148" holds 1 and 4148).
b8 fude_zoom_circuit_value(const c8* _text, u32 _index, f64* _out) {
    const c8* _p = _text;
    u32 _seen = 0;
    while(_p != NULL && *_p != 0) {
        if(!((*_p >= '0' && *_p <= '9') || (*_p == '.' && _p[1] >= '0' && _p[1] <= '9'))) {
            _p++;
            continue;
        }
        // A number; then a metric prefix (or one standing for the point: 4k7).
        c8* _end = NULL;
        f64 _v = rde_strtod(_p, &_end);
        if(_end == _p) {
            _p++;
            continue;
        }
        // A minus sign straight before it ("-5V", or the typographic "−5V") makes it negative; a hyphen joining a word
        // or a number to it ("4-20mA", "Q-1") does not.
        const usize _at = (usize)(_p - _text);
        const b8    _hyphen = _at >= 1u && _p[-1] == '-' && (_at == 1u || !((_p[-2] >= '0' && _p[-2] <= '9') || ((_p[-2] | 0x20) >= 'a' && (_p[-2] | 0x20) <= 'z')));
        const b8    _minus  = _at >= 3u && (u8)_p[-3] == 0xE2u && (u8)_p[-2] == 0x88u && (u8)_p[-1] == 0x92u;   // U+2212
        const f64   _sign   = _hyphen || _minus ? -1.0 : 1.0;
        _p = _end;
        while(*_p == ' ') {
            _p++;
        }
        f64 _k = 1.0;
        u32 _skip = 1u;
        switch(*_p) {
        case 'p': _k = 1e-12; break;
        case 'n': _k = 1e-9;  break;
        case 'u': _k = 1e-6;  break;
        case 'm': _k = 1e-3;  break;
        case 'k': case 'K': _k = 1e3; break;
        case 'M': _k = 1e6;   break;
        case 'G': _k = 1e9;   break;
        case 'R': _k = 1.0;   break;   // (4R7)
        default:
            if((u8)_p[0] == 0xC2 && (u8)_p[1] == 0xB5) { _k = 1e-6; _skip = 2u; }   // µ
            else { _skip = 0u; }
            break;
        }
        // The letter after a number is taken as its prefix whatever follows it: "10mA" is 10 milli, and so is "10mm".
        _p += _skip;
        if(_skip > 0u && *_p >= '0' && *_p <= '9') {
            // 4k7: the digits after it its decimals.
            c8* _e2 = NULL;
            const f64 _frac = rde_strtod(_p, &_e2);
            u32 _digits = (u32)(_e2 - _p);
            _v += _frac / rde_math_pow_f64(10.0, (f64)_digits);
            _p = _e2;
        }
        if(_seen == _index) {
            *_out = _sign * _v * _k;
            return true;
        }
        _seen++;
    }
    return false;
}

// Returns whether the character at _p (UTF-8) belongs to a word of Latin letters: an ASCII letter or digit, or a
// letter of Latin-1 or Latin Extended (U+00C0 to U+024F, lead bytes C3 to C9: "á", "ç", "é"), but not × or ÷.
RDE_INTERNAL b8 fzc_latin_at(const c8* _p) {
    const u8 _c = (u8)_p[0];
    if(_c < 0x80u) {
        return (_c >= '0' && _c <= '9') || ((_c | 0x20u) >= 'a' && (_c | 0x20u) <= 'z');
    }
    return _c >= 0xC3u && _c <= 0xC9u && !(_c == 0xC3u && ((u8)_p[1] == 0x97u || (u8)_p[1] == 0xB7u));
}

// Returns whether the character before _p in _text belongs to a word of Latin letters (fzc_latin_at): back over its
// UTF-8 continuation bytes to its first.
RDE_INTERNAL b8 fzc_latin_before(const c8* _text, const c8* _p) {
    if(_p <= _text) {
        return false;
    }
    const c8* _q = _p - 1;
    while(_q > _text && ((u8)*_q & 0xC0u) == 0x80u) {
        _q--;
    }
    return fzc_latin_at(_q);
}

// Returns a byte with an ASCII capital made small; any other byte as it is.
RDE_INTERNAL u8 fzc_fold(c8 _c) {
    const u8 _u = (u8)_c;
    return _u >= 'A' && _u <= 'Z' ? (u8)(_u | 0x20u) : _u;
}

// Returns where in _text the first of _words (_count of them) begins, as fude_zoom_circuit_word finds it, and sets
// *_means to what it means and *_length to its bytes; NULL when _text (which may be NULL) holds none. At each character
// where a Latin word may begin (none of its letters before it), every word is tried and the longest there wins; a Latin
// word must also end where its word does. Without one, the text is read on from the next character (a Latin word's own
// letters are never a start, so nothing is found inside it).
RDE_INTERNAL const c8* fzc_word_at(const c8* _text, const fude_zoom_word* _words, u32 _count, u8* _means, usize* _length) {
    for(const c8* _p = _text; _p != NULL && *_p != 0;) {
        const b8 _start = !fzc_latin_before(_text, _p);
        usize _best = 0;
        for(u32 _i = 0; _i < _count; _i++) {
            const c8* _w = _words[_i].word;
            const b8 _latin = fzc_latin_at(_w);
            if(_latin && !_start) {
                continue;
            }
            usize _n = 0;
            while(_w[_n] != 0 && _p[_n] != 0 && fzc_fold(_p[_n]) == fzc_fold(_w[_n])) {
                _n++;
            }
            if(_w[_n] != 0 || _n <= _best || (_latin && fzc_latin_at(_p + _n))) {
                continue;   // Not there, no longer than one found, or a longer word's beginning.
            }
            _best   = _n;
            *_means = _words[_i].means;
        }
        if(_best > 0u) {
            *_length = _best;
            return _p;
        }
        _p++;
        while(*_p != 0 && ((u8)*_p & 0xC0u) == 0x80u) {
            _p++;   // (to the next character's first byte)
        }
    }
    return NULL;
}

// See the header: the first word fzc_word_at finds.
u8 fude_zoom_circuit_word(const c8* _text, const fude_zoom_word* _words, u32 _count, u8 _none) {
    u8 _means = _none;
    usize _length = 0;
    return fzc_word_at(_text, _words, _count, &_means, &_length) != NULL ? _means : _none;
}

// Returns whether byte _c may be part of a number as a part's text writes it: a digit, its point, a metric prefix
// (fude_zoom_circuit_value's: p n u m k K M G, R for the point) or one of the two bytes of µ.
RDE_INTERNAL b8 fzc_number_byte(c8 _c) {
    return (_c >= '0' && _c <= '9') || _c == '.' || _c == 'p' || _c == 'n' || _c == 'u' || _c == 'm' || _c == 'k' || _c == 'K' || _c == 'M' ||
           _c == 'G' || _c == 'R' || (u8)_c == 0xC2u || (u8)_c == 0xB5u;
}

// Reads the number _text gives with the first of _words it holds (whole, as fude_zoom_circuit_word finds them): the
// one written straight before it ("500 lux", "1k lux", "200 steps": only spaces between), else the one straight after
// it ("steps: 200"). A number elsewhere in the text is not its: "28BYJ-48 stepper" gives no steps, "flux 5" no lux.
// Returns false when it gives none.
RDE_INTERNAL b8 fzc_number_by(const c8* _text, const fude_zoom_word* _words, u32 _count, f64* _out) {
    u8 _means = 0u;
    usize _length = 0;
    const c8* _w = fzc_word_at(_text, _words, _count, &_means, &_length);
    if(_w == NULL) {
        return false;
    }
    // Before it: back over the spaces, then over what a number is written with, to its first digit.
    const c8* _end = _w;
    while(_end > _text && _end[-1] == ' ') {
        _end--;
    }
    const c8* _n = _end;
    while(_n > _text && fzc_number_byte(_n[-1])) {
        _n--;
    }
    while(_n < _end && !((*_n >= '0' && *_n <= '9') || (*_n == '.' && _n[1] >= '0' && _n[1] <= '9'))) {
        _n++;
    }
    if(_n < _end) {
        // Only the number's own bytes are read: from the whole text, the value reader would take the word's first
        // letter as a prefix past the spaces ("48 pasos" as 48 pico, the p of pasos).
        c8 _number[32];
        const usize _bytes = (usize)(_end - _n);
        if(_bytes >= sizeof(_number)) {
            return false;   // (no number is written that long)
        }
        rde_memcpy(_number, _n, _bytes);
        _number[_bytes] = 0;
        return fude_zoom_circuit_value(_number, 0u, _out);
    }
    // After it: over spaces, a colon or an equals sign, to a number straight there.
    const c8* _a = _w + _length;
    while(*_a == ' ' || *_a == ':' || *_a == '=') {
        _a++;
    }
    return ((*_a >= '0' && *_a <= '9') || (*_a == '.' && _a[1] >= '0' && _a[1] <= '9')) && fude_zoom_circuit_value(_a, 0u, _out);
}

// The words a stepper's steps a turn and a lit sensor's light are written with, in the apps' languages.
RDE_INTERNAL const fude_zoom_word FZC_STEP_WORDS[] = {
    { "step", 1u }, { "steps", 1u }, { "pasos", 1u }, { "passos", 1u }, { "pas", 1u },
    { "\xE3\x82\xB9\xE3\x83\x86\xE3\x83\x83\xE3\x83\x97", 1u },   // ステップ
};
RDE_INTERNAL const fude_zoom_word FZC_LUX_WORDS[] = {
    { "lux", 1u }, { "lx", 1u },
    { "\xE3\x83\xAB\xE3\x82\xAF\xE3\x82\xB9", 1u },               // ルクス
};

// The LED colours (circuit.h's FUDE_ZOOM_LED_ order): each one's English name, forward voltage and light.
RDE_INTERNAL const fude_zoom_led_color FZC_LED_COLORS[FUDE_ZOOM_LED_COLORS] = {
    { "red",      1.8, { 255,  40,  40, 255 } },
    { "orange",   2.0, { 255, 140,  20, 255 } },
    { "yellow",   2.1, { 255, 220,  30, 255 } },
    { "green",    2.2, {  40, 230,  70, 255 } },
    { "blue",     3.0, {  60, 130, 255, 255 } },
    { "white",    3.0, { 255, 255, 250, 255 } },
    { "pink",     3.0, { 255, 110, 200, 255 } },
    { "purple",   3.1, { 170,  80, 255, 255 } },
    { "infrared", 1.2, { 140,  20,  20, 255 } },
};

// The words for them in the apps' languages (English, Spanish, Portuguese, French and Japanese; an adjective's other
// gender too), the English name of each among them.
RDE_INTERNAL const fude_zoom_word FZC_LED_WORDS[] = {
    { "red", FUDE_ZOOM_LED_RED }, { "rojo", FUDE_ZOOM_LED_RED }, { "roja", FUDE_ZOOM_LED_RED }, { "vermelho", FUDE_ZOOM_LED_RED },
    { "vermelha", FUDE_ZOOM_LED_RED }, { "rouge", FUDE_ZOOM_LED_RED },
    { "\xE8\xB5\xA4", FUDE_ZOOM_LED_RED },                                       // 赤
    { "\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x89", FUDE_ZOOM_LED_RED },               // レッド
    { "orange", FUDE_ZOOM_LED_ORANGE }, { "naranja", FUDE_ZOOM_LED_ORANGE }, { "laranja", FUDE_ZOOM_LED_ORANGE },
    { "\xE6\xA9\x99", FUDE_ZOOM_LED_ORANGE },                                    // 橙
    { "\xE3\x82\xAA\xE3\x83\xAC\xE3\x83\xB3\xE3\x82\xB8", FUDE_ZOOM_LED_ORANGE }, // オレンジ
    { "yellow", FUDE_ZOOM_LED_YELLOW }, { "amarillo", FUDE_ZOOM_LED_YELLOW }, { "amarilla", FUDE_ZOOM_LED_YELLOW },
    { "amarelo", FUDE_ZOOM_LED_YELLOW }, { "amarela", FUDE_ZOOM_LED_YELLOW }, { "jaune", FUDE_ZOOM_LED_YELLOW },
    { "\xE9\xBB\x84", FUDE_ZOOM_LED_YELLOW },                                    // 黄
    { "\xE3\x82\xA4\xE3\x82\xA8\xE3\x83\xAD\xE3\x83\xBC", FUDE_ZOOM_LED_YELLOW }, // イエロー
    { "green", FUDE_ZOOM_LED_GREEN }, { "verde", FUDE_ZOOM_LED_GREEN }, { "vert", FUDE_ZOOM_LED_GREEN }, { "verte", FUDE_ZOOM_LED_GREEN },
    { "\xE7\xB7\x91", FUDE_ZOOM_LED_GREEN },                                     // 緑
    { "\xE3\x82\xB0\xE3\x83\xAA\xE3\x83\xBC\xE3\x83\xB3", FUDE_ZOOM_LED_GREEN },  // グリーン
    { "blue", FUDE_ZOOM_LED_BLUE }, { "azul", FUDE_ZOOM_LED_BLUE }, { "bleu", FUDE_ZOOM_LED_BLUE }, { "bleue", FUDE_ZOOM_LED_BLUE },
    { "\xE9\x9D\x92", FUDE_ZOOM_LED_BLUE },                                      // 青
    { "\xE3\x83\x96\xE3\x83\xAB\xE3\x83\xBC", FUDE_ZOOM_LED_BLUE },              // ブルー
    { "white", FUDE_ZOOM_LED_WHITE }, { "blanco", FUDE_ZOOM_LED_WHITE }, { "blanca", FUDE_ZOOM_LED_WHITE }, { "branco", FUDE_ZOOM_LED_WHITE },
    { "branca", FUDE_ZOOM_LED_WHITE }, { "blanc", FUDE_ZOOM_LED_WHITE }, { "blanche", FUDE_ZOOM_LED_WHITE },
    { "\xE7\x99\xBD", FUDE_ZOOM_LED_WHITE },                                     // 白
    { "\xE3\x83\x9B\xE3\x83\xAF\xE3\x82\xA4\xE3\x83\x88", FUDE_ZOOM_LED_WHITE },  // ホワイト
    { "pink", FUDE_ZOOM_LED_PINK }, { "rosa", FUDE_ZOOM_LED_PINK }, { "rose", FUDE_ZOOM_LED_PINK },
    { "\xE6\xA1\x83", FUDE_ZOOM_LED_PINK },                                      // 桃
    { "\xE3\x83\x94\xE3\x83\xB3\xE3\x82\xAF", FUDE_ZOOM_LED_PINK },              // ピンク
    { "purple", FUDE_ZOOM_LED_PURPLE }, { "violet", FUDE_ZOOM_LED_PURPLE }, { "morado", FUDE_ZOOM_LED_PURPLE },
    { "morada", FUDE_ZOOM_LED_PURPLE }, { "violeta", FUDE_ZOOM_LED_PURPLE }, { "roxo", FUDE_ZOOM_LED_PURPLE }, { "roxa", FUDE_ZOOM_LED_PURPLE },
    { "violette", FUDE_ZOOM_LED_PURPLE },
    { "\xE7\xB4\xAB", FUDE_ZOOM_LED_PURPLE },                                    // 紫
    { "\xE3\x83\x91\xE3\x83\xBC\xE3\x83\x97\xE3\x83\xAB", FUDE_ZOOM_LED_PURPLE }, // パープル
    { "infrared", FUDE_ZOOM_LED_INFRARED }, { "infra-red", FUDE_ZOOM_LED_INFRARED }, { "IR", FUDE_ZOOM_LED_INFRARED },
    { "infrarrojo", FUDE_ZOOM_LED_INFRARED }, { "infravermelho", FUDE_ZOOM_LED_INFRARED }, { "infrarouge", FUDE_ZOOM_LED_INFRARED },
    { "\xE8\xB5\xA4\xE5\xA4\x96", FUDE_ZOOM_LED_INFRARED },                      // 赤外 (longer than 赤: it wins there)
    { "\xE8\xB5\xA4\xE5\xA4\x96\xE7\xB7\x9A", FUDE_ZOOM_LED_INFRARED },          // 赤外線
};

// See the header: the first colour word (fude_zoom_circuit_word), red without one.
u8 fude_zoom_led_color_of(const c8* _text) {
    return fude_zoom_circuit_word(_text, FZC_LED_WORDS, (u32)(sizeof(FZC_LED_WORDS) / sizeof(FZC_LED_WORDS[0])), FUDE_ZOOM_LED_RED);
}

// See the header.
const fude_zoom_led_color* fude_zoom_led_color_get(u8 _color) {
    return &FZC_LED_COLORS[_color < FUDE_ZOOM_LED_COLORS ? _color : FUDE_ZOOM_LED_RED];
}

// The level words (circuit.h's LEVELS) in the apps' languages: English, Spanish, Portuguese (where it differs), French
// and Japanese; an adjective's other gender too. Japanese has only オン, オフ and 点滅: a Japanese word is matched
// wherever it is (fude_zoom_circuit_word), and the shorter ハイ and ロー are inside too many other words (ハイウェイ,
// フロー) to be read as levels.
RDE_INTERNAL const fude_zoom_word FZC_LEVEL_WORDS[] = {
    { "on", FUDE_ZOOM_LEVEL_HIGH }, { "closed", FUDE_ZOOM_LEVEL_HIGH }, { "high", FUDE_ZOOM_LEVEL_HIGH }, { "1", FUDE_ZOOM_LEVEL_HIGH },
    { "off", FUDE_ZOOM_LEVEL_LOW }, { "open", FUDE_ZOOM_LEVEL_LOW }, { "low", FUDE_ZOOM_LEVEL_LOW }, { "0", FUDE_ZOOM_LEVEL_LOW },
    { "blink", FUDE_ZOOM_LEVEL_BLINK },
    { "encendido", FUDE_ZOOM_LEVEL_HIGH }, { "encendida", FUDE_ZOOM_LEVEL_HIGH }, { "cerrado", FUDE_ZOOM_LEVEL_HIGH },
    { "cerrada", FUDE_ZOOM_LEVEL_HIGH }, { "alto", FUDE_ZOOM_LEVEL_HIGH }, { "alta", FUDE_ZOOM_LEVEL_HIGH },
    { "apagado", FUDE_ZOOM_LEVEL_LOW }, { "apagada", FUDE_ZOOM_LEVEL_LOW }, { "abierto", FUDE_ZOOM_LEVEL_LOW }, { "abierta", FUDE_ZOOM_LEVEL_LOW },
    { "bajo", FUDE_ZOOM_LEVEL_LOW }, { "baja", FUDE_ZOOM_LEVEL_LOW },
    { "parpadeo", FUDE_ZOOM_LEVEL_BLINK }, { "parpadea", FUDE_ZOOM_LEVEL_BLINK }, { "parpadear", FUDE_ZOOM_LEVEL_BLINK },
    { "intermitente", FUDE_ZOOM_LEVEL_BLINK },
    { "ligado", FUDE_ZOOM_LEVEL_HIGH }, { "ligada", FUDE_ZOOM_LEVEL_HIGH }, { "fechado", FUDE_ZOOM_LEVEL_HIGH }, { "fechada", FUDE_ZOOM_LEVEL_HIGH },
    { "desligado", FUDE_ZOOM_LEVEL_LOW }, { "desligada", FUDE_ZOOM_LEVEL_LOW }, { "aberto", FUDE_ZOOM_LEVEL_LOW }, { "aberta", FUDE_ZOOM_LEVEL_LOW },
    { "baixo", FUDE_ZOOM_LEVEL_LOW }, { "baixa", FUDE_ZOOM_LEVEL_LOW },
    { "pisca", FUDE_ZOOM_LEVEL_BLINK }, { "piscar", FUDE_ZOOM_LEVEL_BLINK }, { "piscando", FUDE_ZOOM_LEVEL_BLINK },
    { "allum\xC3\xA9", FUDE_ZOOM_LEVEL_HIGH }, { "allum\xC3\xA9" "e", FUDE_ZOOM_LEVEL_HIGH }, { "ferm\xC3\xA9", FUDE_ZOOM_LEVEL_HIGH },
    { "ferm\xC3\xA9" "e", FUDE_ZOOM_LEVEL_HIGH }, { "haut", FUDE_ZOOM_LEVEL_HIGH }, { "haute", FUDE_ZOOM_LEVEL_HIGH }, { "marche", FUDE_ZOOM_LEVEL_HIGH },
    { "\xC3\xA9teint", FUDE_ZOOM_LEVEL_LOW }, { "\xC3\xA9teinte", FUDE_ZOOM_LEVEL_LOW }, { "ouvert", FUDE_ZOOM_LEVEL_LOW },
    { "ouverte", FUDE_ZOOM_LEVEL_LOW }, { "bas", FUDE_ZOOM_LEVEL_LOW }, { "basse", FUDE_ZOOM_LEVEL_LOW }, { "arr\xC3\xAAt", FUDE_ZOOM_LEVEL_LOW },
    { "clignote", FUDE_ZOOM_LEVEL_BLINK }, { "clignotant", FUDE_ZOOM_LEVEL_BLINK }, { "clignoter", FUDE_ZOOM_LEVEL_BLINK },
    { "\xE3\x82\xAA\xE3\x83\xB3", FUDE_ZOOM_LEVEL_HIGH },   // オン
    { "\xE3\x82\xAA\xE3\x83\x95", FUDE_ZOOM_LEVEL_LOW },    // オフ
    { "\xE7\x82\xB9\xE6\xBB\x85", FUDE_ZOOM_LEVEL_BLINK },  // 点滅
};

// See the header: the first level word (fude_zoom_circuit_word).
u8 fude_zoom_circuit_level(const c8* _text, u8 _none) {
    return fude_zoom_circuit_word(_text, FZC_LEVEL_WORDS, (u32)(sizeof(FZC_LEVEL_WORDS) / sizeof(FZC_LEVEL_WORDS[0])), _none);
}

// See the header. Compared byte by byte, an ASCII capital as its small letter (fzc_fold), as fude_zoom_circuit_word
// compares; a word that says BLINK is not a level.
b8 fude_zoom_circuit_level_word(const c8* _word) {
    for(u32 _i = 0; _word != NULL && _i < (u32)(sizeof(FZC_LEVEL_WORDS) / sizeof(FZC_LEVEL_WORDS[0])); _i++) {
        const c8* _w = FZC_LEVEL_WORDS[_i].word;
        usize _n = 0;
        while(_w[_n] != 0 && _word[_n] != 0 && fzc_fold(_word[_n]) == fzc_fold(_w[_n])) {
            _n++;
        }
        if(_w[_n] == 0 && _word[_n] == 0 && FZC_LEVEL_WORDS[_i].means != FUDE_ZOOM_LEVEL_BLINK) {
            return true;
        }
    }
    return false;
}

// Returns whether _text holds _word as a whole word, in any case (fude_zoom_circuit_word): "on" is in "on" and in
// "S1 on", but not in "contact"; "1" is in "1" but not in "SW1" or "A1".
RDE_INTERNAL b8 fzc_says(const c8* _text, const c8* _word) {
    const fude_zoom_word _w = { _word, 1u };
    return fude_zoom_circuit_word(_text, &_w, 1u, 0u) == 1u;
}

// Returns whether two names are the same in any case (ASCII).
b8 fzc_same_name(const c8* _a, const c8* _b) {
    while(*_a != 0 && *_b != 0) {
        if((*_a | 0x20) != (*_b | 0x20)) {
            return false;
        }
        _a++;
        _b++;
    }
    return *_a == 0 && *_b == 0;
}

// Returns an LED's forward voltage at 10 mA by the first colour its text names (fude_zoom_led_color_of: whole words,
// in any of the apps' languages), and writes that colour's English name into _color; red when it names none.
RDE_INTERNAL f64 fzc_led_volts(const c8* _text, c8* _color, usize _size) {
    const fude_zoom_led_color* _c = fude_zoom_led_color_get(fude_zoom_led_color_of(_text));
    rde_snprintf(_color, _size, "%s", _c->name);
    return _c->volts;
}

// What a part holds beyond its state (bytes, eight at a time): a display's lights as seen (an f32 each), an LCD's
// controller (display.h).
u32 fzc_store_size(const fude_zoom_part* _part) {
    if(_part->model == FUDE_ZOOM_MODEL_CHAR_LCD) {
        return (u32)((sizeof(fude_zoom_lcd) + 7u) & ~(usize)7u);
    }
    if(_part->model == FUDE_ZOOM_MODEL_SCOPE) {
        return 2u * FUDE_ZOOM_SCOPE_SAMPLES * (u32)sizeof(f32);   // (a sweep of CH1's, of CH2's)
    }
    return (fude_zoom_display_lights(_part) * (u32)sizeof(f32) + 7u) & ~7u;
}

// Reads a component's numbers from its text (_v and _w its first two numbers, _has and _has2 whether it has them),
// each with its default: a resistance, a pot's and its wiper, a capacitance, an inductance, an LED's or a zener's
// volts, a source's, a clock's, a lamp's, a motor's, a fuse's, a thyristor's currents, a transformer's ratio, an
// optocoupler's CTR, a speaker's ohms, a thermistor's. False: its model is none of them.
RDE_INTERNAL b8 fzc_read_component(fude_zoom_circuit_part* _p, const c8* _text, b8 _fresh, f64 _v, f64 _w, b8 _has, b8 _has2) {
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_RESISTOR:  _p->value[0] = _has && _v > 0.0 ? rde_math_max_f64(_v, 1e-3) : 1000.0; break;
    case FUDE_ZOOM_MODEL_POT:
        _p->value[0] = _has && _v > 0.0 ? rde_math_max_f64(_v, 1.0) : 10000.0;
        _p->value[1] = _has2 ? (_w > 1.0 ? _w / 100.0 : _w) : 0.5;
        if(_fresh) {
            _p->state[0] = rde_math_min_f64(rde_math_max_f64(_p->value[1], 0.0), 1.0);
        }
        break;
    case FUDE_ZOOM_MODEL_CAPACITOR: _p->value[0] = _has && _v > 0.0 ? _v : 100e-9; break;
    case FUDE_ZOOM_MODEL_INDUCTOR:  _p->value[0] = _has && _v > 0.0 ? _v : 10e-3; break;
    case FUDE_ZOOM_MODEL_LED:       _p->value[0] = fzc_led_volts(_text, _p->color, sizeof(_p->color)); break;
    case FUDE_ZOOM_MODEL_ZENER:     _p->value[0] = _has && _v > 0.0 ? _v : 5.1; break;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:      _p->value[0] = _has ? _v : 5.0; break;
    case FUDE_ZOOM_MODEL_BATTERY:   _p->value[0] = _has ? _v : 9.0; break;
    case FUDE_ZOOM_MODEL_ACSOURCE:  _p->value[0] = _has ? _v : 5.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 50.0; break;
    case FUDE_ZOOM_MODEL_CLOCK: {
        // Its frequency, its volts, and how long each pulse is high (none: half its period) — "50Hz 5V 1.5ms".
        f64 _pulse = 0.0;
        _p->value[0] = _has && _v > 0.0 ? _v : 1.0;
        _p->value[1] = _has2 ? _w : 5.0;
        _p->value[2] = fude_zoom_circuit_value(_text, 2u, &_pulse) && _pulse > 0.0 ? _pulse : 0.0;
        break;
    }
    case FUDE_ZOOM_MODEL_ISOURCE:   _p->value[0] = _has ? _v : 10e-3; break;
    case FUDE_ZOOM_MODEL_LAMP:      _p->value[0] = _has && _v > 0.0 ? _v : 12.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 5.0; break;
    case FUDE_ZOOM_MODEL_MOTOR:     _p->value[0] = _has && _v > 0.0 ? _v : 6.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 60.0; break;
    case FUDE_ZOOM_MODEL_FUSE:      _p->value[0] = _has && _v > 0.0 ? _v : 1.0; break;
    case FUDE_ZOOM_MODEL_SCR:
    case FUDE_ZOOM_MODEL_TRIAC:
        // Its gate's trigger current, its holding current: a C106's, a BT136's.
        _p->value[0] = _p->part->model == FUDE_ZOOM_MODEL_SCR ? 0.2e-3 : 5e-3;
        _p->value[1] = _p->part->model == FUDE_ZOOM_MODEL_SCR ? 3e-3 : 10e-3;
        break;
    case FUDE_ZOOM_MODEL_TRANSFORMER: {
        // Its turns' ratio ("2:1", or two numbers: "24V 12V"), its primary's inductance and resistance.
        f64 _n = 2.0;
        const c8* _colon = rde_strchr(_text, ':');
        f64 _a = 0.0, _b2 = 0.0;
        if(_colon != NULL && fude_zoom_circuit_value(_text, 0u, &_a) && fude_zoom_circuit_value(_colon + 1, 0u, &_b2) && _a > 0.0 && _b2 > 0.0) {
            _n = _a / _b2;
        } else if(_has && _has2 && _v > 0.0 && _w > 0.0) {
            _n = _v / _w;
        }
        _p->value[0] = _n;
        _p->value[1] = 10.0;   // H: a mains transformer's magnetising current a few % of what it carries.
        _p->value[2] = 1.0;
        break;
    }
    case FUDE_ZOOM_MODEL_OPTO:      _p->value[0] = 1.0; break;   // Its CTR: a PC817's, 100%.
    case FUDE_ZOOM_MODEL_SPEAKER:   _p->value[0] = _has && _v > 0.0 ? _v : 8.0; break;
    case FUDE_ZOOM_MODEL_THERMISTOR:
        // Its resistance at 25 °C (its text's first number: "10k"), its B (a number of a thousand or more after it,
        // else 3950).
        _p->value[0] = _has && _v > 0.0 ? _v : 10000.0;
        _p->value[1] = _has2 && _w >= 1000.0 ? _w : 3950.0;
        if(_fresh) {
            _p->state[0] = 25.0;
        }
        break;
    default:
        return false;
    }
    return true;
}

// Reads a module's, a display's or a sensor's numbers from its text (as fzc_read_component), and on _fresh its
// state's starting values: a servo, a solenoid, a stepper, an encoder, a display's LEDs, a character LCD's
// backlight, a panel meter's range, an LM3914, a lit sensor's light, an oscilloscope's knobs. False: its model is
// none of them.
RDE_INTERNAL b8 fzc_read_module(fude_zoom_circuit_part* _p, const c8* _text, b8 _fresh, f64 _v, b8 _has) {
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_SERVO:
        // Which it is (its text naming it; else an SG90): its stall torque (the mechanisms' torque units), its speed at
        // 4.8 V (radians a second: 60° in 0.1 s), the current it draws held still.
        if(rde_strstr(_text, "MG996") != NULL) {
            _p->value[0] = 500.0; _p->value[1] = RDE_PI / 3.0 / 0.17; _p->value[2] = 2.5;
        } else if(rde_strstr(_text, "MG90") != NULL) {
            _p->value[0] = 120.0; _p->value[1] = RDE_PI / 3.0 / 0.1; _p->value[2] = 0.7;
        } else {
            _p->value[0] = 100.0; _p->value[1] = RDE_PI / 3.0 / 0.1; _p->value[2] = 0.65;
        }
        if(_fresh) {
            _p->state[0] = 90.0;   // Its horn: at the middle of its travel.
            _p->state[1] = -1.0;   // No pulse yet: nowhere to go.
            _p->state[4] = -1.0;
            _p->state[7] = -1.0;   // On no mechanism's shaft.
        }
        break;
    case FUDE_ZOOM_MODEL_SOLENOID:
        // Its rated volts; its coil: their half an ampere's resistance, 2 ms of it its inductance.
        _p->value[0] = _has && _v > 0.0 ? _v : 12.0;
        _p->value[1] = _p->value[0] / 0.5;
        _p->value[2] = _p->value[1] * 0.002;
        break;
    case FUDE_ZOOM_MODEL_STEPPER: {
        // Its steps a turn of its shaft: a 28BYJ-48's 2048 (32 of its rotor's, geared 64 to 1), or as many as it says
        // ("200 steps": the number by the word, not its name's "28" or "17").
        f64 _steps = 0.0;
        _p->value[0] = fzc_number_by(_text, FZC_STEP_WORDS, (u32)(sizeof(FZC_STEP_WORDS) / sizeof(FZC_STEP_WORDS[0])), &_steps) && _steps >= 4.0 ? _steps : 2048.0;
        if(_fresh) {
            rde_memset(_p->state, 0, sizeof(_p->state));
            _p->state[7] = -1.0;   // On no mechanism's shaft.
        }
        break;
    }
    case FUDE_ZOOM_MODEL_ENCODER:
        _p->value[0] = _has && _v >= 1.0 ? _v : 20.0;   // Its detents a turn.
        if(_fresh) {
            rde_memset(_p->state, 0, sizeof(_p->state));
            _p->state[7] = -1.0;
        }
        break;
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: _p->value[0] = fzc_led_volts(_text, _p->color, sizeof(_p->color)); break;
    case FUDE_ZOOM_MODEL_CHAR_LCD: {
        // Its backlight: white on blue (its text naming blue, in any of the apps' languages), or a yellow-green one's
        // (its LEDs' forward voltage; its own 100 Ω behind them).
        const b8 _blue = fude_zoom_led_color_of(_text) == FUDE_ZOOM_LED_BLUE;
        rde_snprintf(_p->color, sizeof(_p->color), "%s", _blue ? "blue" : "green");
        _p->value[0] = _blue ? 3.0 : 2.1;
        break;
    }
    case FUDE_ZOOM_MODEL_PANEL_METER: {
        // Up to what it reads, in amperes or volts, its range's unit, its decimals (display.h).
        const fude_zoom_meter _m = fude_zoom_display_meter(_text);
        _p->value[0] = _m.full;
        _p->value[1] = _m.amps ? 1.0 : 0.0;
        _p->value[2] = _m.times;
        _p->value[3] = (f64)_m.decimals;
        break;
    }
    case FUDE_ZOOM_MODEL_LM3914:
        if(_fresh) {
            _p->state[1] = 0.01;   // Its outputs' current until its reference's is known.
        }
        break;
    case FUDE_ZOOM_MODEL_LDR:
    case FUDE_ZOOM_MODEL_PHOTODIODE:
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR:
        if(_fresh) {
            // As much light as its text says ("500 lux": the number by the word lux, whole), else a room's.
            f64 _lux = 0.0;
            _p->state[0] = fzc_number_by(_text, FZC_LUX_WORDS, (u32)(sizeof(FZC_LUX_WORDS) / sizeof(FZC_LUX_WORDS[0])), &_lux) && _lux > 0.0 ? _lux : 100.0;
        }
        break;
    case FUDE_ZOOM_MODEL_SCOPE: {
        // Its time a division, its volts a division, its trigger's level (NAN: auto): display.h.
        const fude_zoom_scope _sc = fude_zoom_display_scope(_text);
        _p->value[0] = _sc.time_div;
        _p->value[1] = _sc.volt_div;
        _p->value[2] = _sc.level;
        _p->value[3] = _sc.volt_div2;
        _p->value[4] = _sc.pos1;   // Its knobs as it comes: each channel's 0 V across its screen's middle.
        _p->value[5] = _sc.pos2;
        if(_fresh) {
            _p->state[1] = -2.0;   // Not started: its first step where it starts from.
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

// Reads what a part new to the circuit (or reset) starts as, from its text: a switch, a logic input or a changeover
// switch on or thrown (its first level word HIGH: "on", "1", "encendido"; a changeover's "B"), a board's pins driven
// (its lines after its name: "D13 blink", "D9 high").
RDE_INTERNAL void fzc_read_switches(fude_zoom_circuit_part* _p, const c8* _text) {
    const b8 _high = fude_zoom_circuit_level(_text, FUDE_ZOOM_LEVEL_LOW) == FUDE_ZOOM_LEVEL_HIGH;
    if(_p->part->model == FUDE_ZOOM_MODEL_SPDT || _p->part->model == FUDE_ZOOM_MODEL_DPDT) {
        // Its text "B" (a word of its own, and no "A"): thrown.
        _p->switch_on = _high || (fzc_says(_text, "B") && !fzc_says(_text, "A")) ? 1u : 0u;
    } else if(_p->part->model == FUDE_ZOOM_MODEL_SWITCH || _p->part->model == FUDE_ZOOM_MODEL_LOGIC_IN) {
        _p->switch_on = _high ? 1u : 0u;
    } else if(_p->part->model == FUDE_ZOOM_MODEL_BOARD) {
        // Its lines after its name: "D13 blink", "D9 high", "GP15 low".
        _p->drive = _p->drive_high = _p->drive_blink = 0u;
        const c8* _line = rde_strchr(_text, '\n');
        while(_line != NULL) {
            _line++;
            c8 _name[24] = { 0 }, _mode[24] = { 0 };
            if(rde_sscanf(_line, "%23s %23s", _name, _mode) == 2) {
                for(u32 _k = 0; _k < _p->part->pin_count && _k < FZC_PIN_BITS; _k++) {   // (a board's: fewer than 64)
                    if(fzc_same_name(_p->part->pins[_k].name, _name)) {
                        const u64 _bit = 1ull << _k;
                        const u8  _level = fude_zoom_circuit_level(_mode, 0xFFu);
                        if(_level == FUDE_ZOOM_LEVEL_HIGH) { _p->drive |= _bit; _p->drive_high |= _bit; }
                        else if(_level == FUDE_ZOOM_LEVEL_LOW) { _p->drive |= _bit; }
                        else if(_level == FUDE_ZOOM_LEVEL_BLINK) { _p->drive |= _bit; _p->drive_blink |= _bit; }
                        break;
                    }
                }
            }
            _line = rde_strchr(_line, '\n');
        }
    }
}

// Reads a part's numbers (value[]) from its text, with each kind's defaults; _fresh: its state's starting values, its
// switches and drives too (it is new to the circuit, or reset). A switch, a logic input or an SPDT starts on when the
// first level word its text holds says HIGH ("on", "closed", "high", "1", in any of the apps' languages: circuit.h's
// LEVELS; an SPDT "B" without "A" too), each a whole word in any case: a switch named "SW1" or "contact" starts open,
// a logic input named "A1" low.
void fzc_read(fude_zoom_circuit_part* _p, const c8* _text, b8 _fresh) {
    f64 _v = 0.0, _w = 0.0;
    const b8 _has = fude_zoom_circuit_value(_text, 0u, &_v), _has2 = fude_zoom_circuit_value(_text, 1u, &_w);
    rde_memset(_p->value, 0, sizeof(_p->value));
    // Its numbers: a component's, or a module's, a display's or a sensor's (any other model has none).
    if(!fzc_read_component(_p, _text, _fresh, _v, _w, _has, _has2)) {
        fzc_read_module(_p, _text, _fresh, _v, _has);
    }
    if(!_fresh) {
        return;
    }
    fzc_read_switches(_p, _text);
}

// Writes a symbol's text (read from all its numbers, not only the first FUDE_ZOOM_SHAPE_NUMBERS) into _out.
void fzc_text_of(const fude_zoom_scene* _s, u32 _object, c8* _out, usize _size) {
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    const u32 _k = fude_zoom_scene_shape_numbers_all(_s, _object, &_n);
    fude_zoom_symbol_text((const f64*)_n.memory, _k, _out, _size);
    rde_arr_free(&_n);
}
