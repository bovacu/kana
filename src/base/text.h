#ifndef KANA_TEXT
#define KANA_TEXT

#include "rde.h"

// ===========================================================================
// The UI's words, in the learner's language — through RDE's localization
// (rde_localization_load, one block per language in assets/text/strings.rdel).
//
// Every string has an id (text_ids.h, KANA_TEXT_*). kana_text gives a plain
// one; KANA_TEXTF fills in a template's placeholders ({0}, {0,plural, ...}) —
// RDE renders them, so a translation can put its words in any order. Both are
// cached: the screens ask every frame.
//
// A language is a whole block of the file. Should one lack a string, the
// English one shows instead (English is read first, then the language).
// Without the file (the tests' default) a string is its id.
// ===========================================================================

#define KANA_TEXT_FILE "assets/text/strings.rdel"

typedef enum {
#define KANA_TEXT_ID(_id) KANA_TEXT_##_id,
#include "base/text_ids.h"
#undef KANA_TEXT_ID
    KANA_TEXT_COUNT
} KANA_TEXT_;

// The languages Kana speaks, as the settings keep them (RDE_LANGUAGE_ ordinals:
// RDE's enum is append-only, so a saved one keeps its meaning).
#define KANA_TEXT_LANGUAGES 5u

RDE_STRUCT {
    RDE_LANGUAGE_ language;
    const c8*     name;      // in itself: English, Español...
    const c8*     flag;      // its flag (assets/flags/)
} kana_text_language_info;

extern const kana_text_language_info KANA_TEXT_LANGUAGE_LIST[KANA_TEXT_LANGUAGES];

// Reads _language's strings (English under them). False, and nothing changes,
// when the file or the language cannot be read.
b8            kana_text_set_language(RDE_LANGUAGE_ _language);
RDE_LANGUAGE_ kana_text_language(void);
// The device's language when Kana speaks it; English otherwise.
RDE_LANGUAGE_ kana_text_default_language(void);
// Goes up with every change of language (the UI built with the old one is
// built again).
u32           kana_text_revision(void);

const c8*     kana_text(KANA_TEXT_ _id);

// A template's argument: a whole number or a string (copied as needed).
RDE_STRUCT {
    u8        kind;       // 0: number, 1: string
    i64       number;
    const c8* string;
} kana_text_arg;

#define KANA_TN(_v) ((kana_text_arg){ .kind = 0, .number = (i64)(_v), .string = NULL })
#define KANA_TS(_v) ((kana_text_arg){ .kind = 1, .number = 0, .string = (_v) })

// _id's template with _args into _out (_size bytes).
void kana_text_format(c8* _out, usize _size, KANA_TEXT_ _id, const kana_text_arg* _args, u32 _count);

// The same into a char array: KANA_TEXTF(line, KANA_TEXT_OF_N, KANA_TN(3), KANA_TN(9)).
#define KANA_TEXTF(_out, _id, ...)                                                                 \
    kana_text_format((_out), sizeof(_out), (_id), (const kana_text_arg[]){ __VA_ARGS__ },          \
                     (u32)(sizeof((const kana_text_arg[]){ __VA_ARGS__ }) / sizeof(kana_text_arg)))

// A date as the language writes it (day, month's name, year), from Unix seconds (local time).
void kana_text_date(c8* _out, usize _size, u64 _time);
// The same with its weekday before it and the clock after ("Thu 1 Oct 2026, 14:05").
void kana_text_date_time(c8* _out, usize _size, u64 _time);

#endif
