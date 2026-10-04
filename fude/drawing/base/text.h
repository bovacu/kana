// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TEXT
#define FUDE_TEXT

#include "rde.h"

// ===========================================================================
// The UI's words, in the learner's language — through RDE's localization
// (rde_localization_load, one block per language in assets/text/strings.rdel).
//
// Every string has an id (text_ids.h, FUDE_TEXT_*). fude_text gives a plain
// one; FUDE_TEXTF fills in a template's placeholders ({0}, {0,plural, ...}) —
// RDE renders them, so a translation can put its words in any order. Both are
// cached: the screens ask every frame.
//
// A language is a whole block of the file. Should one lack a string, the
// English one shows instead (English is read first, then the language).
// Without the file (the tests' default) a string is its id.
// ===========================================================================

#define FUDE_TEXT_FILE "assets/text/strings.rdel"

typedef enum {
#define FUDE_TEXT_ID(_id) FUDE_TEXT_##_id,
#include "text_ids.h"   // the app's (its own folder is on the include path)
#undef FUDE_TEXT_ID
    FUDE_TEXT_COUNT
} FUDE_TEXT_;

// The languages Kana speaks, as the settings keep them (RDE_LANGUAGE_ ordinals:
// RDE's enum is append-only, so a saved one keeps its meaning). The fourth is the
// one the app teaches — a learner may use the app in it as practice: Japanese
// unless the app says otherwise (fude_text_set_taught_language).
#define FUDE_TEXT_LANGUAGES 5u
#define FUDE_TEXT_TAUGHT    3u   // the taught language's place in the list

RDE_STRUCT {
    RDE_LANGUAGE_ language;
    const c8*     name;      // in itself: English, Español...
    const c8*     flag;      // its flag (assets/flags/)
} fude_text_language_info;

extern fude_text_language_info FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_LANGUAGES];

// The fourth language: the one the app teaches (a study app's lang layer gives
// it: Hanzi's Chinese, Hangul's Korean), at start, before the first
// fude_text_set_language. Taken only when the strings file has that language;
// until then the fourth stays Japanese.
void          fude_text_set_taught_language(RDE_LANGUAGE_ _language, const c8* _name, const c8* _flag);
// Is _language one of the list's (a saved choice may name one the app no longer offers)?
b8            fude_text_language_offered(RDE_LANGUAGE_ _language);

// Reads _language's strings (English under them). False, and nothing changes,
// when the file or the language cannot be read.
b8            fude_text_set_language(RDE_LANGUAGE_ _language);
RDE_LANGUAGE_ fude_text_language(void);
// The device's language when Kana speaks it; English otherwise.
RDE_LANGUAGE_ fude_text_default_language(void);
// Goes up with every change of language (the UI built with the old one is
// built again).
u32           fude_text_revision(void);

const c8*     fude_text(FUDE_TEXT_ _id);

// A template's argument: a whole number or a string (copied as needed).
RDE_STRUCT {
    u8        kind;       // 0: number, 1: string
    i64       number;
    const c8* string;
} fude_text_arg;

#define FUDE_TN(_v) ((fude_text_arg){ .kind = 0, .number = (i64)(_v), .string = NULL })
#define FUDE_TS(_v) ((fude_text_arg){ .kind = 1, .number = 0, .string = (_v) })

// _id's template with _args into _out (_size bytes).
void fude_text_format(c8* _out, usize _size, FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count);

// The same into a char array: FUDE_TEXTF(line, FUDE_TEXT_OF_N, FUDE_TN(3), FUDE_TN(9)).
#define FUDE_TEXTF(_out, _id, ...)                                                                 \
    fude_text_format((_out), sizeof(_out), (_id), (const fude_text_arg[]){ __VA_ARGS__ },          \
                     (u32)(sizeof((const fude_text_arg[]){ __VA_ARGS__ }) / sizeof(fude_text_arg)))

// A date as the language writes it (day, month's name, year), from Unix seconds (local time).
void fude_text_date(c8* _out, usize _size, u64 _time);
// The same with its weekday before it and the clock after ("Thu 1 Oct 2026, 14:05").
void fude_text_date_time(c8* _out, usize _size, u64 _time);

#endif
