#include "drawing/ink/notes.h"
#include "drawing/base/save.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static b8 exists(const char* p) { struct stat st; return stat(p, &st) == 0; }
static void touch(const char* p) { FILE* f = fopen(p, "wb"); fputs("x", f); fclose(f); }
static const fude_note* by_name(const fude_notes* n, const char* name) { const fude_note* a = n->notes.memory; for(u32 i = 0; i < n->notes.count; i++) if(!strcmp(a[i].name, name)) return &a[i]; return NULL; }

int main(void) {
    // A page from before notes, with its backup.
    mkdir("saves", 0755); touch("saves/page.kana"); touch("saves/page.kana.bak");

    fude_notes n; fude_notes_init(&n); fude_notes_load(&n);
    CHECK(n.notes.count == 1 && n.open != 0);
    const fude_note* page = fude_notes_find(&n, n.open);
    char path[512]; fude_notes_canvas_path(n.open, path, sizeof path);
    printf("migrated: '%s' -> %s\n", page ? page->name : "?", path);
    CHECK(page && !strcmp(page->name, "Page") && page->kind == FUDE_NOTE_CANVAS);
    CHECK(exists(path) && !exists("saves/page.kana") && !exists("saves/page.kana.bak"));
    char bak[520]; snprintf(bak, sizeof bak, "%s.bak", path); CHECK(exists(bak));

    // Folders and canvases.
    char name[FUDE_NOTE_NAME];
    fude_notes_new_name(&n, FUDE_NOTE_CANVAS, name, sizeof name); CHECK(!strcmp(name, "Canvas 1"));
    const u32 f1 = fude_notes_add(&n, FUDE_NOTE_FOLDER, 0, "  Japanese  ");
    const u32 c1 = fude_notes_add(&n, FUDE_NOTE_CANVAS, f1, "Essay 1");
    const u32 c2 = fude_notes_add(&n, FUDE_NOTE_CANVAS, f1, "");                  // empty: a fallback name
    const u32 c3 = fude_notes_add(&n, FUDE_NOTE_CANVAS, 0, "Scratch");
    const u32 bad = fude_notes_add(&n, FUDE_NOTE_FOLDER, f1, "Nested");            // folders nest now
    CHECK(f1 && c1 && c2 && c3 && bad);
    CHECK(!strcmp(fude_notes_find(&n, f1)->name, "Japanese") && !strcmp(fude_notes_find(&n, c2)->name, "Canvas"));
    CHECK(fude_notes_find(&n, bad)->parent == f1 && fude_notes_count_in(&n, f1) == 2);
    fude_notes_new_name(&n, FUDE_NOTE_CANVAS, name, sizeof name); printf("next canvas name: %s\n", name);

    // A long name is cut on a character boundary; UTF-8 kept whole.
    char longname[200]; strcpy(longname, "日本語"); while(strlen(longname) < 150) strcat(longname, "語");
    CHECK(fude_notes_rename(&n, c1, longname));
    const char* nm = fude_notes_find(&n, c1)->name; const size_t L = strlen(nm);
    printf("long name kept %zu bytes\n", L);
    CHECK(L < FUDE_NOTE_NAME && L % 3 == 0 && !strncmp(nm, "日本語", 9));

    fude_notes_open(&n, c2); fude_notes_set_expanded(&n, f1, false);
    for(u32 id = c1; id <= c3; id++) { fude_notes_canvas_path(id, path, sizeof path); touch(path); }

    // Round trip.
    fude_notes m; fude_notes_init(&m); fude_notes_load(&m);
    CHECK(m.notes.count == n.notes.count && m.open == c2 && m.next_id == n.next_id);
    CHECK(!fude_notes_find(&m, f1)->expanded && !strcmp(fude_notes_find(&m, c1)->name, nm) && fude_notes_find(&m, c1)->parent == f1);

    // Removing the folder takes its canvases (and their files); the open one (c2) goes: another opens.
    fude_notes_remove(&m, f1);
    CHECK(!fude_notes_find(&m, f1) && !fude_notes_find(&m, c1) && !fude_notes_find(&m, c2));
    fude_notes_canvas_path(c1, path, sizeof path); CHECK(!exists(path));
    fude_notes_canvas_path(c3, path, sizeof path); CHECK(exists(path));
    CHECK(m.open != c2 && fude_notes_find(&m, m.open) && fude_notes_find(&m, m.open)->kind == FUDE_NOTE_CANVAS);
    printf("after removing the folder, open: '%s'\n", fude_notes_find(&m, m.open)->name);

    // --- nesting and moving (on m, which has: Page, Scratch, and the folder Nested... gone with f1)
    const u32 A  = fude_notes_add(&m, FUDE_NOTE_FOLDER, 0, "A");
    const u32 B  = fude_notes_add(&m, FUDE_NOTE_FOLDER, A, "B");
    const u32 x1 = fude_notes_add(&m, FUDE_NOTE_CANVAS, B, "x1");
    const u32 x2 = fude_notes_add(&m, FUDE_NOTE_CANVAS, A, "x2");
    const u32 x3 = fude_notes_add(&m, FUDE_NOTE_CANVAS, 0, "x3");
    CHECK(fude_notes_count_in(&m, A) == 2 && fude_notes_count_in(&m, B) == 1);
    CHECK(fude_notes_is_within(&m, x1, A) && fude_notes_is_within(&m, B, A) && !fude_notes_is_within(&m, A, B));
    // A folder into itself or into something inside it: refused.
    CHECK(!fude_notes_move(&m, A, B, 0) && !fude_notes_move(&m, A, A, 0) && fude_notes_find(&m, A)->parent == 0);
    // x3 into B, before x1: B's order is x3, x1.
    CHECK(fude_notes_move(&m, x3, B, x1));
    { const fude_note* all = m.notes.memory; u32 order[8], k = 0; for(u32 i = 0; i < m.notes.count; i++) if(all[i].parent == B) order[k++] = all[i].id;
      CHECK(k == 2 && order[0] == x3 && order[1] == x1); }
    CHECK(fude_notes_count_in(&m, A) == 3);
    // B out to the top level, at the end; then x2 out, before A.
    CHECK(fude_notes_move(&m, B, 0, 0) && fude_notes_find(&m, B)->parent == 0 && fude_notes_count_in(&m, A) == 1);
    CHECK(fude_notes_move(&m, x2, 0, A));
    { const fude_note* all = m.notes.memory; u32 ix2 = 99, iA = 99; for(u32 i = 0; i < m.notes.count; i++) { if(all[i].id == x2) ix2 = i; if(all[i].id == A) iA = i; }
      CHECK(ix2 + 1 == iA && fude_notes_find(&m, x2)->parent == 0); }
    // Moves survive a reload.
    { fude_notes r; fude_notes_init(&r); fude_notes_load(&r);
      CHECK(fude_notes_find(&r, x3)->parent == B && fude_notes_find(&r, B)->parent == 0 && fude_notes_find(&r, x2)->parent == 0);
      fude_notes_destroy(&r); }
    // B back into A; removing A takes B, x3 and x1 with their files.
    CHECK(fude_notes_move(&m, B, A, 0));
    for(u32 id = x1; id <= x3; id++) { fude_notes_canvas_path(id, path, sizeof path); touch(path); }
    fude_notes_remove(&m, A);
    CHECK(!fude_notes_find(&m, B) && !fude_notes_find(&m, x1) && !fude_notes_find(&m, x3) && fude_notes_find(&m, x2));
    fude_notes_canvas_path(x1, path, sizeof path); CHECK(!exists(path));
    fude_notes_canvas_path(x3, path, sizeof path); CHECK(!exists(path));
    fude_notes_canvas_path(x2, path, sizeof path); CHECK(exists(path));

    // A loop in a damaged index (P in Q, Q in P) is broken on load.
    { const u32 P = fude_notes_add(&m, FUDE_NOTE_FOLDER, 0, "P"); const u32 Q = fude_notes_add(&m, FUDE_NOTE_FOLDER, P, "Q");
      fude_note* all = m.notes.memory; for(u32 i = 0; i < m.notes.count; i++) if(all[i].id == P) all[i].parent = Q;
      fude_notes_save(&m);
      fude_notes r; fude_notes_init(&r); fude_notes_load(&r);
      CHECK(!fude_notes_is_within(&r, P, Q) || !fude_notes_is_within(&r, Q, P));
      printf("loop broken: P parent %u, Q parent %u\n", fude_notes_find(&r, P)->parent, fude_notes_find(&r, Q)->parent);
      fude_notes_destroy(&r); fude_notes_remove(&m, P); fude_notes_remove(&m, Q); }

    // Removing every canvas leaves a new one open.
    const fude_note* a = m.notes.memory;
    while(1) { u32 victim = 0; for(u32 i = 0; i < m.notes.count; i++) if(a[i].kind == FUDE_NOTE_CANVAS) { victim = a[i].id; break; } if(!victim) break;
               const u32 before = m.notes.count; fude_notes_remove(&m, victim); a = m.notes.memory; if(m.notes.count >= before) break; if(m.notes.count == 1 && fude_notes_find(&m, m.open) && fude_notes_find(&m, m.open)->id != victim && by_name(&m, "Nested") == NULL) break; if(m.notes.count <= 2) break; }
    CHECK(fude_notes_find(&m, m.open) && fude_notes_find(&m, m.open)->kind == FUDE_NOTE_CANVAS);

    // A damaged index: set aside, and a fresh start.
    FILE* f = fopen("saves/notes/index.kana", "wb"); fputs("garbage", f); fclose(f);
    remove("saves/notes/index.kana.bak");
    fude_notes k; fude_notes_init(&k); fude_notes_load(&k);
    CHECK(k.notes.count == 1 && fude_notes_find(&k, k.open) && exists("saves/notes/index.kana.bad"));

    fude_notes_destroy(&n); fude_notes_destroy(&m); fude_notes_destroy(&k);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
