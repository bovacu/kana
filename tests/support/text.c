// A stand-in for RDE's localization, for the tests: reads the real
// assets/text/strings.rdel (one language block), and renders {n} and
// {n,plural, one{..} other{..}} with the block's @plural rule (n != 1, n > 1, 1).
// It renders as RDE's renderer does (rde_localization.c): braces nest, a plural's
// branch is a message rendered again (so a {1} inside it is filled in, and # is the
// count), and an apostrophe escapes only { } # and itself.
#include "rde.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_MAX 1024
static char* ts_ids[TS_MAX];
static char* ts_values[TS_MAX];
static int   ts_count = 0;
static char  ts_rule[32] = "n != 1";
static int   ts_loaded = 0;

static void ts_clear(void) {
    for(int i = 0; i < ts_count; i++) { free(ts_ids[i]); free(ts_values[i]); }
    ts_count = 0; ts_loaded = 0; strcpy(ts_rule, "n != 1");
}

b8 rde_localization_load(const c8* _file, const c8* _language, rde_memory_allocator* _a) {
    (void)_a;
    ts_clear();
    FILE* f = fopen(_file, "rb");
    if(f == NULL) {
        char alt[1024];
        snprintf(alt, sizeof alt, "%s/%s", FUDE_ROOT, _file);   // run.sh: the repository
        f = fopen(alt, "rb");
    }
    if(f == NULL) return false;
    char line[4096];
    int in = 0;
    while(fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while(n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if(n == 0 || (line[0] == '/' && line[1] == '/')) continue;
        char* eq = strchr(line, '=');
        if(eq == NULL) {
            char* colon = strchr(line, ':');
            if(colon != NULL) {
                *colon = 0;
                if(in) break;
                in = strcmp(line, _language) == 0;
            }
            continue;
        }
        if(!in) continue;
        *eq = 0;
        char* key = line; char* val = eq + 1;
        if(key[0] == '@') {
            if(strncmp(key, "@plural", 7) == 0) { while(*val == ' ') val++; snprintf(ts_rule, sizeof ts_rule, "%s", val); }
            continue;
        }
        if(ts_count < TS_MAX) { ts_ids[ts_count] = strdup(key); ts_values[ts_count] = strdup(val); ts_count++; }
    }
    fclose(f);
    ts_loaded = in;
    return in ? true : false;
}

static const char* ts_find(const char* id) {
    for(int i = 0; i < ts_count; i++) if(strcmp(ts_ids[i], id) == 0) return ts_values[i];
    return NULL;
}

b8 rde_localization_string_id_exists(const c8* _id) { return ts_find(_id) != NULL; }

const c8* rde_localization_language_code(RDE_LANGUAGE_ _l) {
    switch(_l) {
        case RDE_LANGUAGE_EN_US: return "EN-US"; case RDE_LANGUAGE_ES_ES: return "ES-ES"; case RDE_LANGUAGE_FR_FR: return "FR-FR";
        case RDE_LANGUAGE_PT_BR: return "PT-BR"; case RDE_LANGUAGE_JA_JP: return "JA-JP"; default: return "";
    }
}
RDE_LANGUAGE_ rde_localization_get_system_language(void) { return RDE_LANGUAGE_EN_US; }

// Bindings: the id and the arguments, kept in the struct's own fields.
static rde_str ts_str(const char* s) { rde_str r; memset(&r, 0, sizeof r); r.str = strdup(s ? s : ""); r.length = (u32)strlen(r.str); r.capacity = r.length + 1; return r; }
rde_localization_binding rde_localization_binding_new(const c8* _id) { rde_localization_binding b; memset(&b, 0, sizeof b); b.string_id = ts_str(_id); return b; }
void rde_localization_binding_free(rde_localization_binding* _b) {
    free(_b->string_id.str);
    for(u32 i = 0; i < _b->args_count; i++) if(_b->args_data[i].type == RDE_LOCALIZATION_BINDING_DATA_TYPE_STR) free(_b->args_data[i].data.t_str.str);
    memset(_b, 0, sizeof *_b);
}
b8 rde_localization_binding_add_i64(rde_localization_binding* _b, i64 _v) {
    if(_b->args_count >= RDE_LOCALIZATION_MAX_ARGS) return false;
    _b->args_data[_b->args_count].type = RDE_LOCALIZATION_BINDING_DATA_TYPE_I64; _b->args_data[_b->args_count].data.t_i64 = _v; _b->args_count++; return true;
}
b8 rde_localization_binding_add_str(rde_localization_binding* _b, const c8* _v) {
    if(_b->args_count >= RDE_LOCALIZATION_MAX_ARGS) return false;
    _b->args_data[_b->args_count].type = RDE_LOCALIZATION_BINDING_DATA_TYPE_STR; _b->args_data[_b->args_count].data.t_str = ts_str(_v); _b->args_count++; return true;
}

static int ts_one(long long n) {
    if(strstr(ts_rule, "n > 1")) return !(n > 1);
    if(strcmp(ts_rule, "1") == 0) return 0;
    return n == 1;
}

// What is rendered goes into a buffer of cap bytes at *o, always leaving room for the NUL.
static void ts_put(char* out, size_t* o, size_t cap, char c) { if(*o + 1 < cap) out[(*o)++] = c; }

// Whether an apostrophe before c escapes it: only before the format's own { } # and the apostrophe (RDE's rule).
static int ts_escapable(char c) { return c == '{' || c == '}' || c == '#' || c == '\''; }

// Returns the '}' closing the '{' just before p, counting the braces nested in it and skipping escaped ones; NULL
// when it is never closed (RDE's _rde_localization_find_close).
static const char* ts_close(const char* p) {
    int depth = 1;
    for(; *p; p++) {
        if(*p == '\'' && ts_escapable(p[1])) { p++; continue; }
        if(*p == '{') depth++;
        else if(*p == '}' && --depth == 0) return p;
    }
    return NULL;
}

// Appends argument k as plain text: a string as it is, a number in digits.
static void ts_plain(const rde_localization_binding* b, unsigned k, char* out, size_t* o, size_t cap) {
    char val[256];
    if(b->args_data[k].type == RDE_LOCALIZATION_BINDING_DATA_TYPE_STR) snprintf(val, sizeof val, "%s", b->args_data[k].data.t_str.str);
    else snprintf(val, sizeof val, "%lld", (long long)b->args_data[k].data.t_i64);
    for(char* v = val; *v; v++) ts_put(out, o, cap, *v);
}

static void ts_render(const rde_localization_binding* b, const char* msg, char* out, size_t* o, size_t cap);

// Renders a plural whose branches are body .. end ("one{..} other{..}"), for argument k: the branch the rule picks,
// else "other", else the count as plain text. The branch is a message of its own: # becomes the count (an escaped
// '# a #), any other escape is kept whole for the render after, and it is rendered again, so the placeholders
// inside it are filled in.
static void ts_plural(const rde_localization_binding* b, unsigned k, const char* body, const char* end, char* out, size_t* o, size_t cap) {
    const long long n    = b->args_data[k].type == RDE_LOCALIZATION_BINDING_DATA_TYPE_STR ? 0 : (long long)b->args_data[k].data.t_i64;
    const char*     want = ts_one(n) ? "one" : "other";
    const char *pick = NULL, *pick_end = NULL, *other = NULL, *other_end = NULL;
    for(const char* p = body; p < end;) {
        while(p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        const char* name = p;
        while(p < end && *p != '{') p++;
        if(p >= end) break;
        const size_t len    = (size_t)(p - name);
        const char*  branch = p + 1;
        const char*  close  = ts_close(branch);
        if(close == NULL || close > end) break;
        if(pick == NULL && len == strlen(want) && strncmp(name, want, len) == 0) { pick = branch; pick_end = close; }
        if(len == 5 && strncmp(name, "other", 5) == 0) { other = branch; other_end = close; }
        p = close + 1;
    }
    if(pick == NULL) { pick = other; pick_end = other_end; }
    if(pick == NULL) { ts_plain(b, k, out, o, cap); return; }
    char branch[4096]; size_t bn = 0;
    for(const char* q = pick; q < pick_end; q++) {
        if(*q == '\'' && q + 1 < pick_end && ts_escapable(q[1])) {
            if(q[1] != '#') ts_put(branch, &bn, sizeof branch, '\'');
            ts_put(branch, &bn, sizeof branch, q[1]);
            q++;
            continue;
        }
        if(*q == '#') {
            char num[32]; snprintf(num, sizeof num, "%lld", n);
            for(char* v = num; *v; v++) ts_put(branch, &bn, sizeof branch, *v);
            continue;
        }
        ts_put(branch, &bn, sizeof branch, *q);
    }
    branch[bn] = 0;
    ts_render(b, branch, out, o, cap);
}

// Renders a placeholder whose inside is spec .. end ("1", "0,plural, one{..} other{..}"): with no argument of its
// index, verbatim; with no type, the argument as plain text; a plural by its branch; any other type as plain text.
static void ts_placeholder(const rde_localization_binding* b, const char* spec, const char* end, char* out, size_t* o, size_t cap) {
    const char* p = spec;
    unsigned    k = 0;
    int         digits = 0;
    while(p < end && (*p == ' ' || *p == '\t')) p++;
    for(; p < end && *p >= '0' && *p <= '9'; p++, digits++) k = k * 10u + (unsigned)(*p - '0');
    if(digits == 0 || k >= b->args_count) {
        ts_put(out, o, cap, '{');
        for(const char* q = spec; q < end; q++) ts_put(out, o, cap, *q);
        ts_put(out, o, cap, '}');
        return;
    }
    while(p < end && (*p == ' ' || *p == '\t')) p++;
    if(p >= end || *p != ',') { ts_plain(b, k, out, o, cap); return; }
    for(p++; p < end && (*p == ' ' || *p == '\t'); p++) {}
    const char* type = p;
    while(p < end && *p != ',' && *p != ' ' && *p != '\t') p++;
    if((size_t)(p - type) != 6 || strncmp(type, "plural", 6) != 0 || p >= end || *p != ',') { ts_plain(b, k, out, o, cap); return; }
    ts_plural(b, k, p + 1, end, out, o, cap);
}

// Renders msg into out: escapes resolved, each placeholder filled in, an unclosed '{' and what follows it verbatim.
static void ts_render(const rde_localization_binding* b, const char* msg, char* out, size_t* o, size_t cap) {
    for(const char* p = msg; *p;) {
        if(*p == '\'' && ts_escapable(p[1])) { ts_put(out, o, cap, p[1]); p += 2; continue; }
        if(*p != '{') { ts_put(out, o, cap, *p); p++; continue; }
        const char* close = ts_close(p + 1);
        if(close == NULL) { for(; *p; p++) ts_put(out, o, cap, *p); return; }
        ts_placeholder(b, p + 1, close, out, o, cap);
        p = close + 1;
    }
}

const c8* rde_localization_render(const rde_localization_binding* _b) {
    const char* msg = ts_find(_b->string_id.str);
    if(msg == NULL) return strdup(_b->string_id.str);
    char out[4096]; size_t o = 0;
    ts_render(_b, msg, out, &o, sizeof out);
    out[o] = 0;
    return strdup(out);
}
void rde_localization_free_translation(const c8* _t) { free((any)_t); }
// With nothing to fill in, as RDE's: the value with its escapes resolved and every placeholder as written.
const c8* rde_localization_get_translation(const c8* _id) {
    rde_localization_binding b = rde_localization_binding_new(_id);
    const c8* t = rde_localization_render(&b);
    rde_localization_binding_free(&b);
    return t;
}

// The tests speak English: loaded before main.
#include "drawing/base/text.h"
__attribute__((weak, visibility("hidden"))) void rde_log_level(RDE_LOG_LEVEL_ _level, const c8* _fmt, ...) { (void)_level; (void)_fmt; }
__attribute__((constructor)) static void ts_english(void) { fude_text_set_language(RDE_LANGUAGE_EN_US); }

// The locale's digits (Arabic's own): the tests' languages write 0-9, so nothing changes.
__attribute__((weak, visibility("hidden"))) usize rde_localization_localize_digits(c8* _io, usize _capacity) { (void)_capacity; return strlen(_io); }
