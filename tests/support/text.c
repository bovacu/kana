// A stand-in for RDE's localization, for the tests: reads the real
// assets/text/strings.rdel (one language block), and renders {n} and
// {n,plural, one{..} other{..}} with the block's @plural rule (n != 1, n > 1, 1).
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

const c8* rde_localization_render(const rde_localization_binding* _b) {
    const char* msg = ts_find(_b->string_id.str);
    if(msg == NULL) return strdup(_b->string_id.str);
    char out[4096]; size_t o = 0;
    #define PUT(c) do { if(o + 1 < sizeof out) out[o++] = (c); } while(0)
    for(const char* p = msg; *p; p++) {
        if(*p == '\'' && p[1]) { PUT(p[1]); p++; continue; }
        if(*p != '{') { PUT(*p); continue; }
        unsigned k = (unsigned)(p[1] - '0');
        char val[256] = "";
        long long num = 0;
        if(k < _b->args_count) {
            if(_b->args_data[k].type == RDE_LOCALIZATION_BINDING_DATA_TYPE_STR) snprintf(val, sizeof val, "%s", _b->args_data[k].data.t_str.str);
            else { num = _b->args_data[k].data.t_i64; snprintf(val, sizeof val, "%lld", num); }
        } else {
            // out of range: verbatim
            const char* e = p; int d = 0; do { if(*e == '{') d++; if(*e == '}') d--; PUT(*e); e++; } while(*e && d > 0);
            p = e - 1; continue;
        }
        if(p[2] == '}') { for(char* v = val; *v; v++) PUT(*v); p += 2; continue; }
        // plural
        const char* end = p + 1; int d = 1;
        for(; *end && d > 0; end++) { if(*end == '{') d++; if(*end == '}') d--; }
        const char* br = strstr(p, ts_one(num) ? "one{" : "other{");
        if(br && br < end) {
            for(const char* q = br + (ts_one(num) ? 4 : 6); *q && *q != '}'; q++) { if(*q == '#') { for(char* v = val; *v; v++) PUT(*v); } else PUT(*q); }
        }
        p = end - 1;
    }
    out[o] = 0;
    return strdup(out);
}
void rde_localization_free_translation(const c8* _t) { free((void*)_t); }

// The tests speak English: loaded before main.
#include "drawing/base/text.h"
__attribute__((weak)) void rde_log_level(RDE_LOG_LEVEL_ _level, const c8* _fmt, ...) { (void)_level; (void)_fmt; }
__attribute__((constructor)) static void ts_english(void) { fude_text_set_language(RDE_LANGUAGE_EN_US); }

// The locale's digits (Arabic's own): the tests' languages write 0-9, so nothing changes.
__attribute__((weak)) usize rde_localization_localize_digits(c8* _io, usize _capacity) { (void)_capacity; return strlen(_io); }
