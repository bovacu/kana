// The engine's files, log, clock and drawing, as the tests need them: files
// through stdio (as engine/src/rde_file.c does on POSIX), a log on stdout, a
// clock that stands still, drawing that does nothing. Every one is weak: a suite
// that needs its own (a clock it moves, a file that fails) defines it in its test.c.
#include "rde.h"
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define KANA_STUB __attribute__((weak))

struct rde_file { FILE* f; };
static b8 g_failed = false;
KANA_STUB b8   rde_failed(void) { return g_failed; }
KANA_STUB void rde_error_policy_push(RDE_ON_FAIL_ p) { (void)p; }
KANA_STUB void rde_error_policy_pop(void) {}
KANA_STUB void rde_log_level(RDE_LOG_LEVEL_ l, const c8* fmt, ...) { (void)l; va_list a; va_start(a, fmt); printf("  [log] "); vprintf(fmt, a); printf("\n"); va_end(a); }
KANA_STUB void rde_log_color(RDE_LOG_COLOR_ c, const c8* fmt, ...) { (void)c; (void)fmt; }
KANA_STUB rde_file* rde_file_open(const c8* p, RDE_FILE_MODE_ m) {
    FILE* f = fopen(p, m == RDE_FILE_MODE_WRITE_BYTES ? "wb" : "rb");
    g_failed = f == NULL; if(!f) return NULL;
    rde_file* r = malloc(sizeof(*r)); r->f = f; return r;
}
KANA_STUB void rde_file_write_bytes(rde_file* f, const u8* d, usize n) { g_failed = fwrite(d, 1, n, f->f) != n; }
KANA_STUB void rde_file_close(rde_file* f) { fclose(f->f); free(f); }
KANA_STUB u8* rde_file_read_full_file_bytes(rde_file* f, usize* n, rde_memory_allocator* a) {
    fseek(f->f, 0, SEEK_END); long sz = ftell(f->f); rewind(f->f);
    u8* d = a->calloc(a->allocator, (usize)sz + 1, 1); *n = fread(d, 1, (usize)sz, f->f); g_failed = false; return d;
}
KANA_STUB b8 rde_file_exists(const c8* p) { struct stat st; return stat(p, &st) == 0; }
KANA_STUB b8 rde_file_move(const c8* a, const c8* b) { return rename(a, b) == 0; }
KANA_STUB b8 rde_file_create_missing_dirs(const c8* p) {
    char buf[1024]; snprintf(buf, sizeof buf, "%s", p);
    char* slash = strrchr(buf, '/'); if(!slash) return true; *slash = 0;   // parent folders of a file path
    for(char* q = buf + 1; *q; q++) if(*q == '/') { *q = 0; mkdir(buf, 0755); *q = '/'; }
    mkdir(buf, 0755); return true;
}
static f64 fake_now = 1.0;   // the clock stands still
KANA_STUB f64  rde_engine_get_time_now(void) { return fake_now; }
KANA_STUB void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
KANA_STUB void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
KANA_STUB void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
KANA_STUB b8   rde_file_delete(const c8* p) { return remove(p) == 0; }


// POSIX crawl, as engine/src/rde_file.c does it (no recursion needed here).
KANA_STUB void rde_file_crawl_dir_recursively(const c8* _path, b8 (_callback)(const c8* _p, b8 _is_dir, any _user_data), c8* _exclude_dirs[RDE_MAX_PATH], u64 _exclude_dirs_size, any _user_data) {
    (void)_exclude_dirs; (void)_exclude_dirs_size;
    DIR* d = opendir(_path); if(!d) return;
    struct dirent* e; char full[1024];
    while((e = readdir(d)) != NULL) {
        if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(full, sizeof full, "%s/%s", _path, e->d_name);
        struct stat st; if(stat(full, &st) == -1) continue;
        if(!_callback(full, S_ISDIR(st.st_mode), _user_data)) break;
    }
    closedir(d);
}
