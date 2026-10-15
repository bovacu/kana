#include "drawing/ink/notes.h"
#include "drawing/base/save.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static b8 exists(const char* p) { struct stat st; return stat(p, &st) == 0; }
static void touch(const char* p) { FILE* f = fopen(p, "wb"); fputs("x", f); fclose(f); }
static const fude_note* by_name(const fude_notes* n, const char* name) { const fude_note* a = (const fude_note*)n->notes.memory; for(u32 i = 0; i < n->notes.count; i++) if(!strcmp(a[i].name, name)) return &a[i]; return NULL; }

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
    // What is under a canvas's ink: its own PDF (c1), a book of the app's (c3); a folder has none.
    fude_notes_set_document(&n, c1, FUDE_NOTE_DOCUMENT_OWN); fude_notes_set_document(&n, c3, 7u); fude_notes_set_document(&n, f1, 3u);
    fude_notes_document_path(c1, path, sizeof path); touch(path);
    CHECK(fude_notes_find(&n, c1)->document == FUDE_NOTE_DOCUMENT_OWN && fude_notes_find(&n, f1)->document == 0u && fude_notes_find(&n, c2)->document == 0u);

    // Round trip.
    fude_notes m; fude_notes_init(&m); fude_notes_load(&m);
    CHECK(m.notes.count == n.notes.count && m.open == c2 && m.next_id == n.next_id);
    CHECK(!fude_notes_find(&m, f1)->expanded && !strcmp(fude_notes_find(&m, c1)->name, nm) && fude_notes_find(&m, c1)->parent == f1);
    CHECK(fude_notes_find(&m, c1)->document == FUDE_NOTE_DOCUMENT_OWN && fude_notes_find(&m, c3)->document == 7u && fude_notes_find(&m, c2)->document == 0u);

    // Every file a canvas owns is <id>.<something> in the notes folder: a damaged copy, a backup of its read text, a
    // canvas app's recordings and their text are found by that name. Another canvas whose id starts with the same
    // digits ("<c1>7.kana"), and a file of the same name in a folder below, are not its own.
    char dir[512]; fude_notes_canvas_path(c1, dir, sizeof dir); *strrchr(dir, '/') = 0;
    const char* owned[] = { "kana.bad", "kana.2.bad", "lines.bak", "rec1", "rec1.text", "tools", "pages", "words" };
    char side[600];
    for(u32 i = 0; i < sizeof owned / sizeof owned[0]; i++) { snprintf(side, sizeof side, "%s/%u.%s", dir, c1, owned[i]); touch(side); }
    char other[600]; snprintf(other, sizeof other, "%s/%u7.kana", dir, c1); touch(other);
    char below[600]; snprintf(below, sizeof below, "%s/sub", dir); mkdir(below, 0755);
    snprintf(below, sizeof below, "%s/sub/%u.rec1", dir, c1); touch(below);

    // Removing the folder takes its canvases (and their files); the open one (c2) goes: another opens.
    fude_notes_remove(&m, f1);
    for(u32 i = 0; i < sizeof owned / sizeof owned[0]; i++) { snprintf(side, sizeof side, "%s/%u.%s", dir, c1, owned[i]); CHECK(!exists(side)); }
    CHECK(exists(other) && exists(below));
    remove(other); remove(below); snprintf(below, sizeof below, "%s/sub", dir); rmdir(below);
    CHECK(!fude_notes_find(&m, f1) && !fude_notes_find(&m, c1) && !fude_notes_find(&m, c2));
    fude_notes_canvas_path(c1, path, sizeof path); CHECK(!exists(path));
    fude_notes_document_path(c1, path, sizeof path); CHECK(!exists(path));   // its own PDF went with it
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
    { const fude_note* all = (const fude_note*)m.notes.memory; u32 order[8], k = 0; for(u32 i = 0; i < m.notes.count; i++) if(all[i].parent == B) order[k++] = all[i].id;
      CHECK(k == 2 && order[0] == x3 && order[1] == x1); }
    CHECK(fude_notes_count_in(&m, A) == 3);
    // B out to the top level, at the end; then x2 out, before A.
    CHECK(fude_notes_move(&m, B, 0, 0) && fude_notes_find(&m, B)->parent == 0 && fude_notes_count_in(&m, A) == 1);
    CHECK(fude_notes_move(&m, x2, 0, A));
    { const fude_note* all = (const fude_note*)m.notes.memory; u32 ix2 = 99, iA = 99; for(u32 i = 0; i < m.notes.count; i++) { if(all[i].id == x2) ix2 = i; if(all[i].id == A) iA = i; }
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
      fude_note* all = (fude_note*)m.notes.memory; for(u32 i = 0; i < m.notes.count; i++) if(all[i].id == P) all[i].parent = Q;
      fude_notes_save(&m);
      fude_notes r; fude_notes_init(&r); fude_notes_load(&r);
      CHECK(!fude_notes_is_within(&r, P, Q) || !fude_notes_is_within(&r, Q, P));
      printf("loop broken: P parent %u, Q parent %u\n", fude_notes_find(&r, P)->parent, fude_notes_find(&r, Q)->parent);
      fude_notes_destroy(&r); fude_notes_remove(&m, P); fude_notes_remove(&m, Q); }

    // Removing every canvas leaves a new one open.
    const fude_note* a = (const fude_note*)m.notes.memory;
    while(1) { u32 victim = 0; for(u32 i = 0; i < m.notes.count; i++) if(a[i].kind == FUDE_NOTE_CANVAS) { victim = a[i].id; break; } if(!victim) break;
               const u32 before = m.notes.count; fude_notes_remove(&m, victim); a = (const fude_note*)m.notes.memory; if(m.notes.count >= before) break; if(m.notes.count == 1 && fude_notes_find(&m, m.open) && fude_notes_find(&m, m.open)->id != victim && by_name(&m, "Nested") == NULL) break; if(m.notes.count <= 2) break; }
    CHECK(fude_notes_find(&m, m.open) && fude_notes_find(&m, m.open)->kind == FUDE_NOTE_CANVAS);

    // A damaged index: set aside, and its backup read (save.h), the list as it was before the last change.
    fude_notes_save(&m);
    FILE* f = fopen("saves/notes/index.kana", "wb"); fputs("garbage", f); fclose(f);
    fude_notes j; fude_notes_init(&j); fude_notes_load(&j);
    CHECK(j.notes.count == m.notes.count && j.open == m.open && exists("saves/notes/index.kana.bad"));
    fude_notes_destroy(&j);

    // Both damaged: the list is made again from the canvases' files, so none is lost and no id is used twice.
    fude_notes_canvas_path(77, path, sizeof path); f = fopen(path, "wb"); fputs("a page", f); fclose(f);
    fude_notes_document_path(77, path, sizeof path); f = fopen(path, "wb"); fputs("%PDF", f); fclose(f);
    f = fopen("saves/notes/index.kana", "wb"); fputs("garbage", f); fclose(f);
    f = fopen("saves/notes/index.kana.bak", "wb"); fputs("garbage", f); fclose(f);
    fude_notes k; fude_notes_init(&k); fude_notes_load(&k);
    CHECK(fude_notes_find(&k, 77) && fude_notes_find(&k, 77)->kind == FUDE_NOTE_CANVAS && fude_notes_find(&k, 77)->document == FUDE_NOTE_DOCUMENT_OWN);
    // ...each at the top, named "Canvas 1", "Canvas 2"... in the order of their ids, and the index written once for
    // them all (once a canvas, the second write would have left the first as index.kana.bak).
    { const fude_note* all = (const fude_note*)k.notes.memory; u32 misnamed = 0, before = 0;
      for(u32 i = 0; i < k.notes.count; i++) {
          char want[64]; snprintf(want, sizeof want, "Canvas %u", i + 1u);
          misnamed += strcmp(all[i].name, want) != 0 || all[i].parent != 0u || all[i].kind != FUDE_NOTE_CANVAS || all[i].id <= before;
          before = all[i].id;
      }
      CHECK(k.notes.count > 1u && misnamed == 0u && !exists("saves/notes/index.kana.bak")); }
    CHECK(fude_notes_find(&k, k.open) && k.next_id > 77u && fude_notes_add(&k, FUDE_NOTE_CANVAS, 0, "New") > 77u);

    // The tree the side panel lists from: each folder's notes in list order, and its canvases at any depth, the same as
    // fude_notes_count_in finds them one folder at a time. On a random nest of folders and canvases, moved about so the
    // list's order is not the order they were added in.
    { fude_notes t; fude_notes_init(&t);
      u32 folders[64], nf = 0, seed = 12345u;
      for(u32 i = 0; i < 300; i++) {
          seed = seed * 1103515245u + 12345u;
          const u32 parent = nf > 0 && (seed >> 16) % 4u != 0 ? folders[(seed >> 8) % nf] : 0u;
          const b8  folder = nf < 64 && (seed >> 20) % 3u == 0;
          const u32 id     = fude_notes_add(&t, folder ? FUDE_NOTE_FOLDER : FUDE_NOTE_CANVAS, parent, folder ? "F" : "C");
          CHECK(id != 0);
          if(folder) folders[nf++] = id;
      }
      CHECK(nf > 0);
      for(u32 i = 0; i < 60 && nf > 0; i++) {
          seed = seed * 1103515245u + 12345u;
          const fude_note* all = (const fude_note*)t.notes.memory;
          fude_notes_move(&t, all[(seed >> 8) % t.notes.count].id, (seed >> 16) % 3u == 0 ? 0u : folders[(seed >> 4) % nf], 0);   // a folder into itself is refused
      }
      fude_notes_tree tree; fude_notes_tree_make(&t, &tree);
      const fude_note* all = (const fude_note*)t.notes.memory; const u32 count = t.notes.count;
      const u32* first = (const u32*)tree.first.memory; const u32* end = (const u32*)tree.end.memory;
      const u32* child = (const u32*)tree.child.memory; const u32* canvases = (const u32*)tree.canvases.memory;
      CHECK(tree.child.count == count && first[0] == 0 && end[count] == count);
      u32 seen = 0, total = 0;
      for(u32 p = 0; p <= count; p++) {
          const u32 pid = p < count ? all[p].id : 0u;   // the last group is the top level's
          for(u32 c = first[p]; c < end[p]; c++) { CHECK(all[child[c]].parent == pid); CHECK(c == first[p] || child[c - 1] < child[c]); seen++; }
          if(p < count && all[p].kind == FUDE_NOTE_FOLDER) CHECK(canvases[p] == fude_notes_count_in(&t, all[p].id));
          if(p < count && all[p].kind == FUDE_NOTE_CANVAS) CHECK(first[p] == end[p] && canvases[p] == 0);
      }
      for(u32 i = 0; i < count; i++) total += all[i].kind == FUDE_NOTE_CANVAS ? 1u : 0u;
      CHECK(seen == count && canvases[count] == total);
      fude_notes_tree_free(&tree); CHECK(tree.child.memory == NULL);
      // An empty list: only the top level, with nothing in it.
      fude_notes e; fude_notes_init(&e); fude_notes_tree_make(&e, &tree);
      CHECK(tree.first.count == 1 && ((u32*)tree.first.memory)[0] == 0 && ((u32*)tree.end.memory)[0] == 0 && ((u32*)tree.canvases.memory)[0] == 0);
      fude_notes_tree_free(&tree); fude_notes_destroy(&e);
      fude_notes_destroy(&t); }

    // A new name: the first number no name uses, as new_name writes numbers ("Canvas 02", "Canvas 2x" and "Canvas" use
    // none), found in one pass however many notes there are: 3000 named "Canvas 1" to "Canvas 3000", out of order.
    { fude_notes b; fude_notes_init(&b);
      const char* names[] = { "Canvas 1", "Canvas 3", "Canvas 02", "Canvas 2x", "Canvas", "Folder 2", "Canvas 99999999999999999999" };
      for(u32 i = 0; i < sizeof names / sizeof names[0]; i++) {
          fude_note x; memset(&x, 0, sizeof x); x.id = i + 1u; x.kind = FUDE_NOTE_CANVAS; snprintf(x.name, sizeof x.name, "%s", names[i]);
          rde_arr_add(&b.notes, &x);
      }
      fude_notes_new_name(&b, FUDE_NOTE_CANVAS, name, sizeof name); CHECK(!strcmp(name, "Canvas 2"));
      fude_notes_new_name(&b, FUDE_NOTE_FOLDER, name, sizeof name); CHECK(!strcmp(name, "Folder 1"));
      rde_arr_clear(&b.notes);
      for(u32 i = 0; i < 3000u; i++) {
          fude_note x; memset(&x, 0, sizeof x); x.id = i + 1u; x.kind = FUDE_NOTE_CANVAS; snprintf(x.name, sizeof x.name, "Canvas %u", 3000u - i);
          rde_arr_add(&b.notes, &x);
      }
      fude_notes_new_name(&b, FUDE_NOTE_CANVAS, name, sizeof name); CHECK(!strcmp(name, "Canvas 3001"));
      snprintf(((fude_note*)b.notes.memory)[1234].name, FUDE_NOTE_NAME, "Mine");   // "Canvas 1766" is free again
      fude_notes_new_name(&b, FUDE_NOTE_CANVAS, name, sizeof name); CHECK(!strcmp(name, "Canvas 1766"));
      fude_notes_destroy(&b); }

    // A damaged index whose 2000 folders hang one from the next, and the last from the first, a loop through them all:
    // the load breaks it at the first folder in the list (as one folder found inside itself at a time would), and the
    // rest stay where they were. Each note is found by a map, not a search of the list per step up the folders.
    { fude_notes d; fude_notes_init(&d);
      for(u32 i = 0; i < 2000u; i++) {
          fude_note x; memset(&x, 0, sizeof x); x.id = i + 1u; x.kind = FUDE_NOTE_FOLDER; x.parent = i + 2u > 2000u ? 1u : i + 2u;
          snprintf(x.name, sizeof x.name, "F%u", i); rde_arr_add(&d.notes, &x);
      }
      d.next_id = 2001u; fude_notes_save(&d);
      fude_notes r; fude_notes_init(&r); fude_notes_load(&r);
      CHECK(fude_notes_find(&r, 1u)->parent == 0u && fude_notes_find(&r, 2u)->parent == 3u && fude_notes_find(&r, 2000u)->parent == 1u);
      CHECK(fude_notes_is_within(&r, 2u, 1u) && !fude_notes_is_within(&r, 1u, 2u));
      CHECK(fude_notes_find(&r, r.open) && fude_notes_find(&r, r.open)->kind == FUDE_NOTE_CANVAS);   // a canvas made to open
      fude_notes_destroy(&r); fude_notes_destroy(&d); }

    fude_notes_destroy(&n); fude_notes_destroy(&m); fude_notes_destroy(&k);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
