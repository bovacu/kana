// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TEXT
#define FUDE_TEXT

#include "rde.h"

// ===========================================================================
// Text: the UI's words in the learner's language, through RDE's localization (rde_localization_load).
//
// Where it sits: drawing/base. Every screen asks it for its strings; the settings choose the language, and a study
// app's lang layer names the language it teaches.
// Its main types: every string has an id (FUDE_TEXT_*, from the app's generated text_ids.h). A template's arguments
// are fude_text_arg values made with FUDE_TN and FUDE_TS.
// Threads: main thread only.
// Files: reads assets/text/strings.rdel, which holds one block per language (the format is RDE's localization).
// Design: fude_text gives a plain string; FUDE_TEXTF fills in a template's placeholders ({0}, {0,plural, ...}). RDE
// renders the templates, so a translation can put its words in any order. Both are cached, because the screens ask
// for the same strings every frame: every plain string is rendered once per language, and the last 256 templates
// filled in are kept. Every number in a UI string comes out in the language's own digits (its @digits).
// A language is a whole block of the file. Should one lack a string, the English one shows instead (English is read
// first, then the language). Without the file (the tests' default) a string is its id's name.
// ===========================================================================

#define FUDE_TEXT_FILE "assets/text/strings.rdel"

// The string ids, one per string.
typedef enum {
#define FUDE_TEXT_ID(_id) FUDE_TEXT_##_id,
#include "text_ids.h"   // the app's own, found because its folder is on the include path
#undef FUDE_TEXT_ID
    FUDE_TEXT_COUNT
} FUDE_TEXT_;

// The languages the UI is offered in, as the settings keep them (RDE_LANGUAGE_ values: RDE's enum only ever grows at
// the end, so a saved one keeps its meaning). The fourth is the language the app teaches, because a learner may use
// the app in it as practice: Japanese unless the app says otherwise (fude_text_set_taught_language).
#define FUDE_TEXT_LANGUAGES 5u
#define FUDE_TEXT_TAUGHT    3u   // the taught language's place in the list

// One language of the list.
RDE_STRUCT {
    RDE_LANGUAGE_ language;
    const c8*     name;      // its name in itself: English, Español...
    const c8*     flag;      // the image of its flag (assets/flags/)
} fude_text_language_info;

// The list: English, Spanish, Brazilian Portuguese, the taught language, French.
extern fude_text_language_info FUDE_TEXT_LANGUAGE_LIST[FUDE_TEXT_LANGUAGES];

// Makes _language the fourth language, the one the app teaches (a study app's lang layer gives it: Hanzi's Chinese,
// Hangul's Korean), with its name in itself and its flag; both are kept, not copied. Call at start, before the first
// fude_text_set_language. It is taken only when the strings file has that language; until then the fourth stays
// Japanese.
void          fude_text_set_taught_language(RDE_LANGUAGE_ _language, const c8* _name, const c8* _flag);
// For an app that teaches no language (Sketching): the fourth language is not offered at all. It is not in Settings,
// not taken from the device, and a saved choice of it is let go. Call at start, before the first
// fude_text_set_language.
void          fude_text_drop_taught_language(void);
// fude_text_language_shown returns whether the list's _index-th language is offered; fude_text_language_count
// returns how many are.
b8            fude_text_language_shown(u32 _index);
u32           fude_text_language_count(void);
// Returns whether _language is one the app offers (a saved choice may name one it no longer offers).
b8            fude_text_language_offered(RDE_LANGUAGE_ _language);

// Loads _language's strings, with English's under them. Returns false, and nothing changes, when the language is
// unknown or the strings file cannot be read. A language the file lacks falls back to English (logged), which still
// returns true. fude_text_language returns the language loaded (RDE_LANGUAGE_NONE before the first load).
b8            fude_text_set_language(RDE_LANGUAGE_ _language);
RDE_LANGUAGE_ fude_text_language(void);
// Returns the device's language when the app offers it, English otherwise.
RDE_LANGUAGE_ fude_text_default_language(void);
// Returns a number that goes up with every change of language, so UI built in the old language is built again.
u32           fude_text_revision(void);

// Returns _id's string in the current language. It stays valid until the next fude_text_set_language. An id out of
// range gives "".
const c8*     fude_text(FUDE_TEXT_ _id);

// A template's argument: a whole number or a string (the string is copied when needed, so it only has to live
// through the call). Made with FUDE_TN or FUDE_TS.
RDE_STRUCT {
    u8        kind;       // 0 for a number, 1 for a string
    i64       number;
    const c8* string;
} fude_text_arg;

// FUDE_TN makes a number argument, FUDE_TS a string argument.
#define FUDE_TN(_v) ((fude_text_arg){ .kind = 0, .number = (i64)(_v), .string = NULL })
#define FUDE_TS(_v) ((fude_text_arg){ .kind = 1, .number = 0, .string = (_v) })

// Fills _id's template with _args (_count of them) into _out (_size bytes, NUL-terminated, cut to fit). A template
// the language lacks is filled in from English's, by English's plural rule.
void fude_text_format(c8* _out, usize _size, FUDE_TEXT_ _id, const fude_text_arg* _args, u32 _count);

// The same into a char array, whose size is taken with sizeof:
// FUDE_TEXTF(line, FUDE_TEXT_OF_N, FUDE_TN(3), FUDE_TN(9)).
#define FUDE_TEXTF(_out, _id, ...)                                                                 \
    fude_text_format((_out), sizeof(_out), (_id), (const fude_text_arg[]){ __VA_ARGS__ },          \
                     (u32)(sizeof((const fude_text_arg[]){ __VA_ARGS__ }) / sizeof(fude_text_arg)))

// Writes a date as the language writes it (the day, the month's name, the year), from Unix seconds, in local time.
void fude_text_date(c8* _out, usize _size, u64 _time);
// The same with the weekday before it and the time of day after it ("Thu 1 Oct 2026, 14:05").
void fude_text_date_time(c8* _out, usize _size, u64 _time);

#endif
