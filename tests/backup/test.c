// backup.h: export, inspect, restore — with real files in ./saves/.
#include "base/backup.h"
#include "app/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// RDE's file calls, as the engine does them on POSIX (rde_file.c).
b8 rde_file_dir_exists(const c8* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static b8 ends_with(const c8* s, const c8* e) { size_t n = strlen(s), m = strlen(e); return n >= m && strcmp(s + n - m, e) == 0; }
void rde_file_crawl_dir_recursively(const c8* _path, b8 (_cb)(const c8*, b8, any), c8* _ex[RDE_MAX_PATH], u64 _n, any _ud) {
    DIR* d = opendir(_path); if(!d) return;
    struct dirent* e; c8 full[RDE_MAX_PATH];
    while((e = readdir(d)) != NULL) {
        if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(full, sizeof full, "%s/%s", _path, e->d_name);
        struct stat st; if(stat(full, &st) == -1) continue;
        if(S_ISDIR(st.st_mode)) {
            b8 skip = false; for(u64 i = 0; i < _n; i++) if(ends_with(full, _ex[i])) skip = true;
            if(!skip) { if(!_cb(full, true, _ud)) break; rde_file_crawl_dir_recursively(full, _cb, _ex, _n, _ud); }
        } else if(!_cb(full, false, _ud)) break;
    }
    closedir(d);
}
b8 rde_file_create_missing_dirs(const c8* p) {
    c8 b[RDE_MAX_PATH]; snprintf(b, sizeof b, "%s", p);
    c8* slash = strrchr(b, '/'); if(!slash) return true; *slash = 0;
    for(c8* c = b + 1; *c; c++) if(*c == '/') { *c = 0; mkdir(b, 0755); *c = '/'; }
    mkdir(b, 0755); return rde_file_dir_exists(b);
}
b8 rde_file_move(const c8* a, const c8* b) { return rename(a, b) == 0; }
b8 rde_file_delete(const c8* p) { return remove(p) == 0; }

static void put(const c8* rel, const c8* text) {
    c8 p[512]; snprintf(p, sizeof p, "saves/%s", rel); rde_file_create_missing_dirs(p);
    FILE* f = fopen(p, "wb"); fputs(text, f); fclose(f);
}
static b8 has(const c8* rel, const c8* text) {
    c8 p[512], buf[256] = ""; snprintf(p, sizeof p, "saves/%s", rel);
    FILE* f = fopen(p, "rb"); if(!f) return false; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
    return text == NULL || strcmp(buf, text) == 0;
}
static u8* slurp(const c8* p, usize* n) {
    FILE* f = fopen(p, "rb"); if(!f) return NULL; fseek(f, 0, SEEK_END); *n = (usize)ftell(f); fseek(f, 0, SEEK_SET);
    u8* d = malloc(*n); fread(d, 1, *n, f); fclose(f); return d;
}

int main(void) {
    system("rm -rf saves out.kanabackup");
    // A save folder as Kana leaves it.
    put("settings.kana", "settings v1");
    put("marks.kana", "marks v1");
    put("marks.kana.bak", "marks OLD");
    put("notes/index.kana", "index v1");
    put("notes/1.kana", "page one");
    put("notes/2.kana", "page two");
    put("notes/2.kana.bak", "page two OLD");
    put("practice/3042.kana", "あ history");
    put("perf.txt", "diagnostics");
    put(".DS_Store", "finder");
    put("outbox/old.kanabackup", "an earlier export");

    // Export: the real files only.
    kana_backup_info info;
    CHECK(kana_backup_export("saves/", "out.kanabackup", &info));
    CHECK(info.files == 6 && info.canvases == 2 && strcmp(info.version, KANA_VERSION) == 0 && info.created > 1700000000u);
    usize n = 0; u8* data = slurp("out.kanabackup", &n);
    CHECK(data != NULL && n > 40);
    kana_backup_info seen;
    CHECK(kana_backup_inspect(data, n, &seen) && seen.files == 6 && seen.canvases == 2 && seen.bytes == info.bytes && seen.created == info.created);
    printf("exported %u files, %llu bytes, %u canvases, into %zu bytes\n", info.files, (unsigned long long)info.bytes, info.canvases, n);

    // Damaged or cut short: not a backup, and nothing changes.
    data[n / 2] ^= 0x40; CHECK(!kana_backup_inspect(data, n, NULL)); data[n / 2] ^= 0x40;
    CHECK(!kana_backup_inspect(data, n - 1, NULL));
    CHECK(!kana_backup_inspect((const u8*)"KANABKP", 8, NULL));
    put("notes/1.kana", "page one EDITED");
    CHECK(!kana_backup_restore(data, n - 3, "saves/") && has("notes/1.kana", "page one EDITED"));

    // Things change after the export: a page edited, one added, marks lost.
    put("notes/3.kana", "page three");
    remove("saves/marks.kana");
    put("words.kana", "words");

    // Import: exactly the backup's files, what was there aside.
    CHECK(kana_backup_restore(data, n, "saves/"));
    CHECK(has("notes/1.kana", "page one") && has("notes/2.kana", "page two") && has("marks.kana", "marks v1") && has("practice/3042.kana", "あ history"));
    CHECK(!has("notes/3.kana", NULL) && !has("words.kana", NULL));               // not in the backup: gone
    CHECK(!has("marks.kana.bak", NULL) && !has("notes/2.kana.bak", NULL));       // no stale copies to fall back on
    CHECK(has("before-import/notes/3.kana", "page three") && has("before-import/notes/1.kana", "page one EDITED") &&
          has("before-import/marks.kana.bak", "marks OLD") && has("before-import/words.kana", "words"));
    CHECK(has("outbox/old.kanabackup", NULL));                                    // the outbox is left alone

    // A second import: the first one's "before" is let go, this one's kept.
    put("notes/1.kana", "page one AGAIN");
    CHECK(kana_backup_restore(data, n, "saves/"));
    CHECK(has("before-import/notes/1.kana", "page one AGAIN") && !has("before-import/notes/3.kana", NULL));

    // A backup that would write outside the save folder is not one.
    static const c8* const bad[] = { "../escape.kana", "/abs.kana", "notes/../../x", "a\\b", "before-import/x", "notes//x", "./x" };
    for(u32 i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        // Hand-made: header, one file, the checksum.
        u8 buf[256]; usize len = strlen(bad[i]), at = 0; u32 h = 2166136261u;
        memcpy(buf, "KANABKP", 8); at = 8;
        u32 v[2] = { 1, 1 }; memcpy(buf + at, v, 8); at += 8; memset(buf + at, 0, 24); at += 24;
        buf[at++] = (u8)len; buf[at++] = 0; memcpy(buf + at, bad[i], len); at += len;
        u32 four = 4; memcpy(buf + at, &four, 4); at += 4; memcpy(buf + at, "evil", 4); at += 4;
        for(usize k = 0; k < at; k++) h = (h ^ buf[k]) * 16777619u;
        memcpy(buf + at, &h, 4); at += 4;
        CHECK(!kana_backup_inspect(buf, at, NULL));
        CHECK(!kana_backup_restore(buf, at, "saves/"));
    }
    CHECK(has("notes/1.kana", "page one AGAIN") == false && has("notes/1.kana", "page one"));   // restored copy still there
    free(data);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
