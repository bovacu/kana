// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See text.h. RDE renders (rde_localization_render); Kana keeps what it
// rendered: every plain string once per language, and the last templates
// asked for — the screens ask for the same ones every frame.
// ===========================================================================

static const c8* const FUDE_TEXT_NAMES[FUDE_TEXT_COUNT] = {
#define FUDE_TEXT_ID(_id) #_id,
#include "text_ids.h"
#undef FUDE_TEXT_ID
};

fude_text_language_info FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_LANGUAGES] = {
    { RDE_LANGUAGE_EN_US, "English",            "assets/flags/en.png" },
    { RDE_LANGUAGE_ES_ES, "Espa\xC3\xB1ol",       "assets/flags/es.png" },
    { RDE_LANGUAGE_PT_BR, "Portugu\xC3\xAAs (Brasil)", "assets/flags/br.png" },
    { RDE_LANGUAGE_JA_JP, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "assets/flags/jp.png" },   // 日本語
    { RDE_LANGUAGE_FR_FR, "Fran\xC3\xA7" "ais",   "assets/flags/fr.png" },
};

#define FUDE_TEXT_RENDERS  256u   // templates kept, rendered
#define FUDE_TEXT_KEY      160u   // a template's arguments, as kept to be compared

typedef struct {
    u32        hash;
    FUDE_TEXT_ id;
    u32        key_len;
    c8         key[FUDE_TEXT_KEY];
    c8*        text;
} fude_text_render_slot;

RDE_INTERNAL c8*                   fude_text_plain[FUDE_TEXT_COUNT];     // the language's (or English's) plain strings
RDE_INTERNAL c8*                   fude_text_english[FUDE_TEXT_COUNT];   // English's, as written: a template's stand-in
RDE_INTERNAL b8                    fude_text_missing[FUDE_TEXT_COUNT];   // the language lacks it: English stands in
RDE_INTERNAL fude_text_render_slot fude_text_renders[FUDE_TEXT_RENDERS];
RDE_INTERNAL RDE_LANGUAGE_         fude_text_active  = RDE_LANGUAGE_NONE;
RDE_INTERNAL u32                   fude_text_changes = 0;

RDE_INTERNAL c8* fude_text_dup(const c8* _s) {
    const usize _n = strlen(_s);
    c8*         _d = (c8*)malloc(_n + 1u);
    memcpy(_d, _s, _n + 1u);
    return _d;
}

// _id rendered by RDE with _args, in the language loaded now; a copy of our own.
RDE_INTERNAL c8* fude_text_render(FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count) {
    rde_localization_binding _binding = rde_localization_binding_new(FUDE_TEXT_NAMES[_id]);
    for(u32 _i = 0; _i < _count; _i++) {
        if(_args[_i].kind == 0) {
            rde_localization_binding_add_i64(&_binding, _args[_i].number);
        } else {
            rde_localization_binding_add_str(&_binding, _args[_i].string != NULL ? _args[_i].string : "");
        }
    }
    const c8* _rendered = rde_localization_render(&_binding);
    // In the language's digits (its @digits: Arabic's ٠١٢٣٤٥٦٧٨٩), every number
    // of a UI string — RDE leaves a bare {0} as it is.
    const usize _n    = strlen(_rendered != NULL ? _rendered : "");
    c8*         _copy = (c8*)malloc(_n * 4u + 1u);
    memcpy(_copy, _rendered != NULL ? _rendered : "", _n + 1u);
    const usize _len  = rde_localization_localize_digits(_copy, _n * 4u + 1u);
    _copy             = (c8*)realloc(_copy, _len + 1u);
    rde_localization_free_translation(_rendered);
    rde_localization_binding_free(&_binding);
    return _copy;
}

// A template filled in by Kana: English's, standing in for a string the
// language lacks — {n} and {n,plural, one{..} other{..}} (English's rule).
RDE_INTERNAL void fude_text_fill(c8* _out, usize _size, const c8* _template, const fude_text_arg* _args, u32 _count) {
    usize _n = 0;
    #define FUDE_TEXT_PUT(_c) do { if(_n + 1u < _size) { _out[_n++] = (_c); } } while(0)
    for(const c8* _p = _template; *_p != 0; _p++) {
        if(*_p != '{' || _p[1] < '0' || _p[1] > '9') {
            FUDE_TEXT_PUT(*_p);
            continue;
        }
        const u32 _k = (u32)(_p[1] - '0');
        c8 _value[64] = "";
        if(_k < _count) {
            if(_args[_k].kind == 0) { snprintf(_value, sizeof(_value), "%lld", (long long)_args[_k].number); }
            else                    { snprintf(_value, sizeof(_value), "%s", _args[_k].string != NULL ? _args[_k].string : ""); }
        }
        const c8* _close = _p + 2;
        if(*_close == '}') {
            for(const c8* _v = _value; *_v != 0; _v++) { FUDE_TEXT_PUT(*_v); }
            _p = _close;
            continue;
        }
        // A plural: the branch for the count, '#' the count.
        const b8  _one    = _k < _count && _args[_k].kind == 0 && _args[_k].number == 1;
        const c8* _branch = strstr(_close, _one ? "one{" : "other{");
        i32       _depth  = 1;
        const c8* _end    = _close;
        for(; *_end != 0 && _depth > 0; _end++) {
            if(*_end == '{') { _depth++; }
            if(*_end == '}') { _depth--; }
        }
        if(_branch != NULL && _branch < _end) {
            for(const c8* _b = _branch + (_one ? 4 : 6); *_b != 0 && *_b != '}'; _b++) {
                if(*_b == '#') { for(const c8* _v = _value; *_v != 0; _v++) { FUDE_TEXT_PUT(*_v); } }
                else           { FUDE_TEXT_PUT(*_b); }
            }
        }
        _p = _end - 1;
    }
    _out[_n] = 0;
    #undef FUDE_TEXT_PUT
}

RDE_INTERNAL void fude_text_forget_renders(void) {
    for(u32 _i = 0; _i < FUDE_TEXT_RENDERS; _i++) {
        free(fude_text_renders[_i].text);
    }
    memset(fude_text_renders, 0, sizeof(fude_text_renders));
}

b8 fude_text_set_language(RDE_LANGUAGE_ _language) {
    const c8* _code = rde_localization_language_code(_language);
    if(_code[0] == 0) {
        return false;
    }
    // English first: what stands in for anything the language lacks.
    if(!rde_localization_load(FUDE_TEXT_FILE, "EN-US", NULL)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "fude: no strings (%s)", FUDE_TEXT_FILE);
        return false;
    }
    for(u32 _i = 0; _i < FUDE_TEXT_COUNT; _i++) {
        free(fude_text_english[_i]);
        fude_text_english[_i] = fude_text_render((FUDE_TEXT_)_i, NULL, 0);
    }
    if(_language != RDE_LANGUAGE_EN_US && !rde_localization_load(FUDE_TEXT_FILE, _code, NULL)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "fude: no %s strings, English instead", _code);
        _language = RDE_LANGUAGE_EN_US;
        rde_localization_load(FUDE_TEXT_FILE, "EN-US", NULL);
    }
    for(u32 _i = 0; _i < FUDE_TEXT_COUNT; _i++) {
        free(fude_text_plain[_i]);
        fude_text_missing[_i] = !rde_localization_string_id_exists(FUDE_TEXT_NAMES[_i]);
        fude_text_plain[_i]   = fude_text_missing[_i] ? fude_text_dup(fude_text_english[_i]) : fude_text_render((FUDE_TEXT_)_i, NULL, 0);
    }
    fude_text_forget_renders();
    fude_text_active = _language;
    fude_text_changes++;
    return true;
}

RDE_LANGUAGE_ fude_text_language(void) {
    return fude_text_active;
}

void fude_text_set_taught_language(RDE_LANGUAGE_ _language, const c8* _name, const c8* _flag) {
    const c8* _code = rde_localization_language_code(_language);
    if(_code[0] == 0 || _name == NULL || _flag == NULL) {
        return;
    }
    // Its strings, there? (Loaded to know — a file without the language loads, with
    // nothing in it, so a string is looked for too. fude_text_set_language loads
    // the one chosen next.)
    if(_language != FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT].language &&
       (!rde_localization_load(FUDE_TEXT_FILE, _code, NULL) || !rde_localization_string_id_exists(FUDE_TEXT_NAMES[0]))) {
        rde_log_level(RDE_LOG_LEVEL_INFO, "fude: no %s strings yet: the fourth language stays %s", _code, FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT].name);
        return;
    }
    FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT] = (fude_text_language_info){ _language, _name, _flag };
}

RDE_INTERNAL b8 fude_text_no_taught = false;

void fude_text_drop_taught_language(void) {
    fude_text_no_taught = true;
}

b8 fude_text_language_shown(u32 _index) {
    return _index < FUDE_TEXT_LANGUAGES && !(fude_text_no_taught && _index == FUDE_TEXT_TAUGHT);
}

u32 fude_text_language_count(void) {
    return fude_text_no_taught ? FUDE_TEXT_LANGUAGES - 1u : FUDE_TEXT_LANGUAGES;
}

b8 fude_text_language_offered(RDE_LANGUAGE_ _language) {
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        if(fude_text_language_shown(_i) && FUDE_TEXT_LANGUAGE_LIST[_i].language == _language) {
            return true;
        }
    }
    return false;
}

RDE_LANGUAGE_ fude_text_default_language(void) {
    const RDE_LANGUAGE_ _system = rde_localization_get_system_language();
    return fude_text_language_offered(_system) ? _system : RDE_LANGUAGE_EN_US;
}

u32 fude_text_revision(void) {
    return fude_text_changes;
}

const c8* fude_text(FUDE_TEXT_ _id) {
    if((u32)_id >= FUDE_TEXT_COUNT) {
        return "";
    }
    return fude_text_plain[_id] != NULL ? fude_text_plain[_id] : FUDE_TEXT_NAMES[_id];
}

void fude_text_format(c8* _out, usize _size, FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count) {
    if(_size == 0) {
        return;
    }
    if((u32)_id >= FUDE_TEXT_COUNT || fude_text_plain[_id] == NULL) {
        snprintf(_out, _size, "%s", (u32)_id < FUDE_TEXT_COUNT ? FUDE_TEXT_NAMES[_id] : "");
        return;
    }
    if(fude_text_missing[_id]) {
        fude_text_fill(_out, _size, fude_text_english[_id], _args, _count);
        return;
    }

    // The arguments as a key: kept renders are found by it.
    c8  _key[FUDE_TEXT_KEY];
    u32 _len = 0;
    b8  _keep = true;
    for(u32 _i = 0; _i < _count && _keep; _i++) {
        c8        _one[72];
        const i32 _w = _args[_i].kind == 0 ? snprintf(_one, sizeof(_one), "#%lld\x1F", (long long)_args[_i].number)
                                           : snprintf(_one, sizeof(_one), "$%s\x1F", _args[_i].string != NULL ? _args[_i].string : "");
        if(_w < 0 || (usize)_w >= sizeof(_one) || _len + (u32)_w > FUDE_TEXT_KEY) {
            _keep = false;
            break;
        }
        memcpy(_key + _len, _one, (usize)_w);
        _len += (u32)_w;
    }
    u32 _hash = 2166136261u ^ (u32)_id;
    for(u32 _i = 0; _i < _len; _i++) {
        _hash = (_hash ^ (u8)_key[_i]) * 16777619u;
    }
    fude_text_render_slot* _slot = &fude_text_renders[_hash % FUDE_TEXT_RENDERS];
    if(_keep && _slot->text != NULL && _slot->hash == _hash && _slot->id == _id && _slot->key_len == _len && memcmp(_slot->key, _key, _len) == 0) {
        snprintf(_out, _size, "%s", _slot->text);
        return;
    }

    c8* _text = fude_text_render(_id, _args, _count);
    snprintf(_out, _size, "%s", _text);
    if(_keep) {
        free(_slot->text);
        _slot->hash    = _hash;
        _slot->id      = _id;
        _slot->key_len = _len;
        memcpy(_slot->key, _key, _len);
        _slot->text    = _text;
    } else {
        free(_text);
    }
}

void fude_text_date(c8* _out, usize _size, u64 _time) {
    const time_t _t  = (time_t)_time;
    struct tm*   _tm = localtime(&_t);
    if(_tm == NULL) {
        snprintf(_out, _size, "?");
        return;
    }
    fude_text_format(_out, _size, FUDE_TEXT_DATE, (const fude_text_arg[]){ FUDE_TN(_tm->tm_mday), FUDE_TS(fude_text((FUDE_TEXT_)(FUDE_TEXT_MONTH_1 + _tm->tm_mon))),
                                                                           FUDE_TN(_tm->tm_year + 1900) }, 3u);
}

void fude_text_date_time(c8* _out, usize _size, u64 _time) {
    c8 _date[64];
    fude_text_date(_date, sizeof(_date), _time);
    const time_t _t  = (time_t)_time;
    struct tm*   _tm = localtime(&_t);
    c8 _clock[16] = "";
    c8 _day[512];
    if(_tm != NULL) {
        snprintf(_clock, sizeof(_clock), "%02d:%02d", _tm->tm_hour, _tm->tm_min);
        snprintf(_day, sizeof(_day), "%s %s", fude_text((FUDE_TEXT_)(FUDE_TEXT_DAY_MON + (_tm->tm_wday + 6) % 7)), _date);   // Monday first
    } else {
        snprintf(_day, sizeof(_day), "%s", _date);
    }
    fude_text_format(_out, _size, FUDE_TEXT_DATE_TIME, (const fude_text_arg[]){ FUDE_TS(_day), FUDE_TS(_clock) }, 2u);
}
