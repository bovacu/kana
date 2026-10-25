// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/text.h"

// ===========================================================================
// See text.h.
//
// RDE renders the strings (rde_localization_render), and this file keeps what it rendered, because the screens ask for
// the same strings every frame. Every plain string is rendered once per language (fude_text_plain). Filled-in templates
// are kept, FUDE_TEXT_RENDERS of them, in an rde_lru by the whole of what they are made from (the id and every
// argument: fude_text_key), the one used longest ago making room (fude_text_renders). English stands in for what a
// language lacks as RDE's fallback language (rde_localization_set_fallback_language).
// ===========================================================================

// Each string id's name, as text_ids.h lists them: the key RDE looks strings up by, and what a string shows when there
// is no strings file.
static const c8* const FUDE_TEXT_NAMES[FUDE_TEXT_COUNT] = {
#define FUDE_TEXT_ID(_id) #_id,
#include "text_ids.h"
#undef FUDE_TEXT_ID
};

// The languages offered, each with its name written in itself and its flag; the fourth (the taught language) may be
// replaced by fude_text_set_taught_language.
fude_text_language_info FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_LANGUAGES] = {
    { RDE_LANGUAGE_EN_US, "English",            "assets/flags/en.png" },
    { RDE_LANGUAGE_ES_ES, "Espa\xC3\xB1ol",       "assets/flags/es.png" },
    { RDE_LANGUAGE_PT_BR, "Portugu\xC3\xAAs (Brasil)", "assets/flags/br.png" },
    { RDE_LANGUAGE_JA_JP, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "assets/flags/jp.png" },   // 日本語
    { RDE_LANGUAGE_FR_FR, "Fran\xC3\xA7" "ais",   "assets/flags/fr.png" },
};

#define FUDE_TEXT_RENDERS  256u   // How many filled-in templates are kept (the one used longest ago makes room).

RDE_INTERNAL rde_str               fude_text_plain[FUDE_TEXT_COUNT];     // The plain strings, English's if missing.
// The filled-in templates kept: the text RDE rendered for each slot of fude_text_renders_lru (made on first use, on
// the std allocator, kept for the run), which knows what each was rendered from (fude_text_key) and which was used
// longest ago. A text is not inited while its slot is empty. fude_text_key_bytes is where a key is built.
RDE_INTERNAL rde_str               fude_text_renders[FUDE_TEXT_RENDERS];
RDE_INTERNAL rde_lru               fude_text_renders_lru;
RDE_INTERNAL rde_arr TYPE(u8)      fude_text_key_bytes;
// The language loaded, and how many times the language has changed (fude_text_revision).
RDE_INTERNAL RDE_LANGUAGE_         fude_text_active  = RDE_LANGUAGE_NONE;
RDE_INTERNAL u32                   fude_text_changes = 0;

// Frees a kept string; one that was never made is left alone.
RDE_INTERNAL void fude_text_drop(rde_str* _s) {
    if(rde_str_is_inited(_s)) {
        rde_str_free(_s);
    }
}

// Returns _id rendered by RDE with _args in the language loaded now, with its digits in the language's own, as a copy
// on the std allocator that the caller frees.
RDE_INTERNAL rde_str fude_text_render(FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count) {
    // A string argument's digits are its own (a canvas called "Notes 2", a file's name), not a number the template
    // shows: they are hidden from the digit conversion below as private-use characters (U+E000 to U+E009, three bytes
    // each), then put back. Every argument has a slot in one buffer, NUL after each (a number's is empty), and they are
    // bound once the buffer is whole, so its memory no longer moves.
    rde_arr TYPE(c8) _hidden = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _count; _i++) {
        for(const c8* _p = _args[_i].kind == 0 || _args[_i].string == NULL ? "" : _args[_i].string; *_p != 0; _p++) {
            if(*_p >= '0' && *_p <= '9') {
                c8* _h = (c8*)rde_arr_add_n(&_hidden, 3u);
                _h[0]  = (c8)0xEE;
                _h[1]  = (c8)0x80;
                _h[2]  = (c8)(0x80 + (*_p - '0'));
            } else {
                *(c8*)rde_arr_add_n(&_hidden, 1u) = *_p;
            }
        }
        *(c8*)rde_arr_add_n(&_hidden, 1u) = 0;
    }
    rde_localization_binding _binding = rde_localization_binding_new(FUDE_TEXT_NAMES[_id]);
    const c8*                _next    = (const c8*)_hidden.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_args[_i].kind == 0) {
            rde_localization_binding_add_i64(&_binding, _args[_i].number);
        } else {
            rde_localization_binding_add_str(&_binding, _next);
        }
        _next += rde_strlen(_next) + 1u;   // every argument has its slot in the buffer, a number's empty
    }
    // With no arguments, the string as written, a template's placeholders kept: RDE reads it so through
    // rde_localization_get_translation, without the warning per placeholder that a binding short of one gets.
    const c8* _rendered = _count == 0 ? rde_localization_get_translation(FUDE_TEXT_NAMES[_id]) : rde_localization_render(&_binding);
    // Every number in a UI string is turned into the language's digits (its @digits, such as Arabic's ٠١٢٣٤٥٦٧٨٩),
    // because RDE leaves a number filled into a bare {0} as it is.
    const usize _n      = rde_strlen(_rendered != NULL ? _rendered : "");
    rde_arr     _digits = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());   // 4 bytes a digit.
    rde_arr_resize(&_digits, _n * 4u + 1u);
    rde_memcpy(_digits.memory, _rendered != NULL ? _rendered : "", _n + 1u);
    rde_localization_localize_digits((c8*)_digits.memory, _n * 4u + 1u);
    // The string arguments' digits back as they were.
    c8* _w = (c8*)_digits.memory;
    for(const c8* _r = _w; *_r != 0;) {
        if((u8)_r[0] == 0xEEu && (u8)_r[1] == 0x80u && (u8)_r[2] >= 0x80u && (u8)_r[2] <= 0x89u) {
            *_w++ = (c8)('0' + ((u8)_r[2] - 0x80u));
            _r   += 3;
        } else {
            *_w++ = *_r++;
        }
    }
    *_w = 0;
    rde_str _copy = rde_str_new((const c8*)_digits.memory, rde_memory_allocator_get_default_std());
    rde_arr_free(&_digits);
    rde_arr_free(&_hidden);
    rde_localization_free_translation(_rendered);
    rde_localization_binding_free(&_binding);
    return _copy;
}

// Frees and empties every kept template, as when the language changes.
RDE_INTERNAL void fude_text_forget_renders(void) {
    for(u32 _i = 0; _i < FUDE_TEXT_RENDERS; _i++) {
        fude_text_drop(&fude_text_renders[_i]);
    }
    rde_memset(fude_text_renders, 0, sizeof(fude_text_renders));
    rde_lru_clear(&fude_text_renders_lru);
}

// Appends _n bytes at _bytes to the key being built.
RDE_INTERNAL void fude_text_key_put(const any _bytes, usize _n) {
    if(_n > 0) {
        rde_memcpy(rde_arr_add_n(&fude_text_key_bytes, _n), _bytes, _n);
    }
}

// Builds in fude_text_key_bytes the whole of what a filled-in template is made from: the id, how many arguments, then
// each argument's kind and value (a number's 8 bytes; a string's length in bytes, then its bytes; NULL is ""), so no
// two calls that could render differently share a key. Returns its length in bytes.
RDE_INTERNAL usize fude_text_key(FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count) {
    if(!rde_arr_is_inited(&fude_text_key_bytes)) {
        fude_text_key_bytes = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(&fude_text_key_bytes);
    const u32 _head[2] = { (u32)_id, _count };
    fude_text_key_put((const any)_head, sizeof(_head));
    for(u32 _i = 0; _i < _count; _i++) {
        const u8 _kind = _args[_i].kind == 0 ? 0u : 1u;
        fude_text_key_put((const any)&_kind, sizeof(_kind));
        if(_kind == 0u) {
            const i64 _number = _args[_i].number;
            fude_text_key_put((const any)&_number, sizeof(_number));
        } else {
            const c8* _s = _args[_i].string != NULL ? _args[_i].string : "";
            const u32 _n = (u32)rde_strlen(_s);
            fude_text_key_put((const any)&_n, sizeof(_n));
            fude_text_key_put((const any)_s, _n);
        }
    }
    return rde_arr_length(&fude_text_key_bytes);
}

// Loads _language (English if the file lacks it), English's strings standing in for any it lacks (RDE's fallback
// language), and renders each of its plain strings. See the header.
b8 fude_text_set_language(RDE_LANGUAGE_ _language) {
    const c8* _code = rde_localization_language_code(_language);
    if(_code[0] == 0) {
        return false;
    }
    rde_localization_set_fallback_language("EN-US");
    if(!rde_localization_load(FUDE_TEXT_FILE, _code, NULL)) {
        if(_language == RDE_LANGUAGE_EN_US || !rde_localization_load(FUDE_TEXT_FILE, "EN-US", NULL)) {
            rde_log_level(RDE_LOG_LEVEL_ERROR, "text: no strings (%s)", FUDE_TEXT_FILE);
            return false;
        }
        rde_log_level(RDE_LOG_LEVEL_WARNING, "text: no %s strings, English instead", _code);
        _language = RDE_LANGUAGE_EN_US;
    }
    for(u32 _i = 0; _i < FUDE_TEXT_COUNT; _i++) {
        fude_text_drop(&fude_text_plain[_i]);
        fude_text_plain[_i] = fude_text_render((FUDE_TEXT_)_i, NULL, 0);
    }
    fude_text_forget_renders();
    fude_text_active = _language;
    fude_text_changes++;
    return true;
}

// Returns the language loaded, or RDE_LANGUAGE_NONE before the first load.
RDE_LANGUAGE_ fude_text_language(void) {
    return fude_text_active;
}

// Puts _language in the list's fourth place when the strings file has it. See the header.
void fude_text_set_taught_language(RDE_LANGUAGE_ _language, const c8* _name, const c8* _flag) {
    const c8* _code = rde_localization_language_code(_language);
    if(_code[0] == 0 || _name == NULL || _flag == NULL) {
        return;
    }
    // Check that the file has the language's strings by loading it. A file without the language still loads, empty,
    // so the first string is looked for too. fude_text_set_language later loads the language chosen.
    if(_language != FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT].language &&
       (!rde_localization_load(FUDE_TEXT_FILE, _code, NULL) || !rde_localization_string_id_exists(FUDE_TEXT_NAMES[0]))) {
        rde_log_level(RDE_LOG_LEVEL_INFO, "text: no %s strings yet: the fourth language stays %s", _code, FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT].name);
        return;
    }
    FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_TAUGHT] = (fude_text_language_info){ _language, _name, _flag };
}

// True when the app teaches no language, so the fourth language is not offered.
RDE_INTERNAL b8 fude_text_no_taught = false;

// Stops offering the fourth language. See the header.
void fude_text_drop_taught_language(void) {
    fude_text_no_taught = true;
}

// Returns whether the list's _index-th language is offered.
b8 fude_text_language_shown(u32 _index) {
    return _index < FUDE_TEXT_LANGUAGES && !(fude_text_no_taught && _index == FUDE_TEXT_TAUGHT);
}

// Returns how many languages are offered.
u32 fude_text_language_count(void) {
    return fude_text_no_taught ? FUDE_TEXT_LANGUAGES - 1u : FUDE_TEXT_LANGUAGES;
}

// Returns whether _language is one of the languages offered.
b8 fude_text_language_offered(RDE_LANGUAGE_ _language) {
    for(u32 _i = 0; _i < FUDE_TEXT_LANGUAGES; _i++) {
        if(fude_text_language_shown(_i) && FUDE_TEXT_LANGUAGE_LIST[_i].language == _language) {
            return true;
        }
    }
    return false;
}

// Returns the device's language when it is offered, English otherwise.
RDE_LANGUAGE_ fude_text_default_language(void) {
    const RDE_LANGUAGE_ _system = rde_localization_get_system_language();
    return fude_text_language_offered(_system) ? _system : RDE_LANGUAGE_EN_US;
}

// Returns how many times the language has changed.
u32 fude_text_revision(void) {
    return fude_text_changes;
}

// Returns _id's plain string, or its id's name before any language is loaded; "" for an id out of range.
const c8* fude_text(FUDE_TEXT_ _id) {
    if((u32)_id >= FUDE_TEXT_COUNT) {
        return "";
    }
    return rde_str_is_inited(&fude_text_plain[_id]) ? rde_str_to_char_ptr(&fude_text_plain[_id]) : FUDE_TEXT_NAMES[_id];
}

// Fills _id's template with the arguments into _out: from the kept render for the same id and arguments, else rendered
// by RDE and kept. Before any language is loaded, it writes the id's name. See the header.
void fude_text_format(c8* _out, usize _size, FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count) {
    if(_size == 0) {
        return;
    }
    if((u32)_id >= FUDE_TEXT_COUNT || !rde_str_is_inited(&fude_text_plain[_id])) {
        rde_snprintf(_out, _size, "%s", (u32)_id < FUDE_TEXT_COUNT ? FUDE_TEXT_NAMES[_id] : "");
        return;
    }
    // The id and the arguments, whole, are the key a kept render is found by.
    if(!rde_lru_is_inited(&fude_text_renders_lru)) {
        fude_text_renders_lru = rde_lru_new(FUDE_TEXT_RENDERS, rde_memory_allocator_get_default_std());
    }
    const usize _key_len = fude_text_key(_id, _args, _count);
    const u32   _kept    = rde_lru_find(&fude_text_renders_lru, (const any)fude_text_key_bytes.memory, _key_len);
    if(_kept != RDE_LRU_NONE && rde_str_is_inited(&fude_text_renders[_kept])) {
        rde_snprintf(_out, _size, "%s", rde_str_to_char_ptr(&fude_text_renders[_kept]));
        return;
    }

    rde_str _text = fude_text_render(_id, _args, _count);
    rde_snprintf(_out, _size, "%s", rde_str_to_char_ptr(&_text));
    const usize _again = fude_text_key(_id, _args, _count);   // (built again: rendering may have used the scratch)
    const u32   _slot  = rde_lru_add(&fude_text_renders_lru, (const any)fude_text_key_bytes.memory, _again, NULL);
    fude_text_drop(&fude_text_renders[_slot]);   // (the render used longest ago, making room)
    fude_text_renders[_slot] = _text;
}

// Writes a date through the DATE template: the day, the month's name and the year, in local time.
void fude_text_date(c8* _out, usize _size, u64 _time) {
    const rde_date_components _c = rde_date_local_components((i64)_time);
    fude_text_format(_out, _size, FUDE_TEXT_DATE, (const fude_text_arg[]){ FUDE_TN((i32)_c.day), FUDE_TS(fude_text((FUDE_TEXT_)(FUDE_TEXT_MONTH_1 + (_c.month - 1)))),
                                                                           FUDE_TN((i32)_c.year) }, 3u);
}

// Writes the weekday's short name and the date, then the time as HH:MM, through the DATE_TIME template.
void fude_text_date_time(c8* _out, usize _size, u64 _time) {
    c8 _date[64];
    fude_text_date(_date, sizeof(_date), _time);
    const rde_date_components _c = rde_date_local_components((i64)_time);
    c8 _clock[16] = "";
    c8 _day[512];
    rde_snprintf(_clock, sizeof(_clock), "%02d:%02d", (i32)_c.hour, (i32)_c.minute);
    rde_snprintf(_day, sizeof(_day), "%s %s", fude_text((FUDE_TEXT_)(FUDE_TEXT_DAY_MON + (_c.day_of_week + 6) % 7)), _date);   // The DAY_ ids start on Monday.
    fude_text_format(_out, _size, FUDE_TEXT_DATE_TIME, (const fude_text_arg[]){ FUDE_TS(_day), FUDE_TS(_clock) }, 2u);
}
