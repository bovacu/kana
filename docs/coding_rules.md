# Coding rules

The rules every line of this repository follows. Nothing ships until the code is certified against them. They apply to
new code and to every line that is touched. `tools/check/rules.py` checks the mechanical ones (§9).

---

## 1. Layers

The code is in four levels, and each uses only the levels below it:

```
apps/<app>          an app: what it is (name, languages, data, store ids) and its shell — data, not logic
fude/<layer>        the shared code, in layers (below)
RDE (~/RDE)         the engine: platform, rendering, UI, containers, files, threads, maths, strings
platform            iOS / Android / macOS APIs, reached only through RDE or through fude's *_ios.m / *_android.c
```

Inside `fude/`, from the bottom up:

| Layer | May include | Is |
|---|---|---|
| `drawing/base` | RDE | files, saves, UTF-8, text, theme, crash notes, and drawing with them (`draw.h`, `icons.h`): no UI |
| `drawing/ink` | base | strokes, the lasso, the canvas pages |
| `drawing/doc` | base, ink | documents under the ink (PDF) |
| `drawing/widgets` | base, ink, doc | pieces of UI that know no app (kit, rows, the filter bar, notices, scrolling) |
| `drawing/app` | all of `drawing` | the app shell and its own UI (toolbar, side panel, menus, cards, the Library, import): screens, page, session, settings, the look harness |
| `sim` | RDE | pure simulation (circuits, mechanisms): no UI, no fude |
| `study` | `drawing`, `lang/lang.h` | what a study app teaches and its screens |
| `lang/<code>` | `drawing`, `study` | one language: what `lang.h` asks |
| `zoom` | `drawing`, `sim` | the infinite canvas and its tools |

Rules:
- **Nothing includes upwards or sideways.** `zoom` never includes `study`; `drawing` never includes `zoom`.
- **No app identity inside `fude`.** No app names in code or logs, and no `if this is InfiNote`. An app describes what it
  has (capabilities, data, strings), and `fude` acts on that description.
- **Nothing app-specific goes into RDE.** A thing goes into RDE only when any app or game could use it: a scalar
  maths function, a condition variable, a crash handler. A stroke codec does not.
- **Anything two apps both do lives in `fude`**, in the lowest layer that can hold it.

## 2. RDE only, never the C library

App and `fude` code calls RDE, never the C standard library or POSIX directly:

| Instead of | Use |
|---|---|
| `memcpy memset memmove memcmp` | `rde_memcpy rde_memset rde_memmove rde_memcmp` |
| `strlen strcmp strncmp strstr strchr strrchr …` | `rde_strlen rde_strcmp …` |
| `snprintf vsnprintf` | `rde_snprintf rde_vsprintf` |
| `malloc calloc realloc free` | `rde_malloc …`, or better a container (§3) |
| `fopen fread fwrite fclose remove rename` | `rde_file_*` |
| `printf fprintf` | `rde_log_level` |
| `qsort bsearch` | `rde_arr_sort`, `rde_sort`, `rde_bsearch` |
| `atoi strtol strtod sscanf` | `rde_str_to_*`, `rde_strtol`, `rde_strtod`, `rde_sscanf` |
| `sqrt sin cos fabs fmin fmax floor …` | `rde_math_sqrt_f64`, `rde_math_sin_f64`… (f32 and f64 versions) |
| `isdigit tolower …` | `rde_char_is_digit`, `rde_char_to_lower`… |
| `time localtime strftime` | `rde_date_*` |
| `pthread_*` | `rde_thread_*`, `rde_mutex_*`, `rde_cond_*` |
| `void*`, `const void*` | `any`, `const any` (RDE's name for it: `#define any void*`, so `const any` points to const data) |

If RDE lacks something, it is added to RDE, as a generic function with a unit test in `~/RDE/tests/unit_tests`, and
then used. The only C headers app code may include are the type headers RDE itself relies on (`stdint.h`, `stdbool.h`,
`stdarg.h`, through `rde.h`).

**The exception is the platform layer.** Objective-C (`*_ios.m`, `*_apple.m`) and JNI (`*_android.c`) files call their
platform's APIs, because that is their job. They still use RDE for everything that is not the platform's.

## 3. Containers

- Growable arrays are `rde_arr`. Maps are `rde_hash_map`, sets `rde_hash_set`; also `rde_stack`, `rde_ring_buffer`,
  `rde_sparse_set`, `rde_str`. No hand-written `{ptr, count, capacity}` structs, hash tables or linked lists.
- In hot loops, add many elements at once with `rde_arr_add_n` and write them in place.
- A fixed-size array is allowed only when its bound is part of the design, and a comment says why the bound exists.
  For example, the crash handler's note ring cannot allocate inside a signal handler, and a file format's header has
  a fixed size. User data (documents, pages, files) never has a fixed cap.
- **Which allocator.** `rde_malloc`, `rde_calloc` and `rde_free` take from the engine's pool, a fixed-size block
  set in the app's `config.rdef`. That suits small objects: one struct, a job, a short string. Anything that grows
  with the user's content goes on the std allocator (`rde_memory_allocator_get_default_std()`), as an `rde_arr`:
  pages, strokes, documents, a file read whole, audio, a table per character. Memory is always freed through the
  same allocator that gave it. A function that hands memory out says which, and gives a matching free when it is
  not obvious (`fude_android_take_alloc` / `fude_android_free`).
- Every allocation has one documented owner, and the comment on the function that returns it says who frees it.

## 4. No duplication

- One behaviour, one implementation. Before writing a helper (UTF-8, hashing, geometry, file paths, UI helpers),
  search for an existing one. If two copies are found, merge them into the lowest layer both can see.
- Apps are data. An app's own `.c` defines what the app is and hands it to the shared shell. It holds no logic that
  another app also needs.
- Generated files (`text_ids.h`, `strings.rdel`) are generated from one source and never edited by hand.

## 5. Documentation

Every module documents itself, so the code can be understood, extended and fixed without help.

**Each header starts with an overview:**
```c
// ===========================================================================
// <Module name>: what it is for, in one or two plain sentences.
//
// Where it sits: its layer, and who uses it.
// Its main types and their lifecycle: who creates them, who frees them.
// Threads: which thread calls it, and what is safe from another thread.
// Files: what it reads and writes, and their format, or where the format is described.
// Design: the decisions a reader would question, each with WHY it was done this way.
// ===========================================================================
```

**Each public function has a comment directly above it** saying:
- what it does;
- what each parameter means, when that is not obvious;
- what it returns, and what a NULL, 0 or false return means;
- who owns returned memory, and how long a returned pointer stays valid;
- whether it may be called from another thread.

**Comments are plain English.** Full sentences that explain WHY, not WHAT. No private shorthand: write
`// Older files have no frame rate; use the default.`, not `// ...the frame rate never shown`. Name magic numbers as
constants, and name the encoding of packed bit fields.

**Big decisions get a design note.** Algorithms, precision schemes, caches, file formats and thread models are
described in `docs/architecture.md` or a document it links to. The code refers to that note.

## 6. Readable code

- **Each app's flow can be followed from its entry point:** start, then each frame (input, update, render), then save
  and stop. `docs/architecture.md` describes that flow for the study apps and for the canvas apps.
- **Size limits:** a source file stays under about 2,000 lines, and a function under about 150. A longer one is split
  by responsibility, or a comment at its top says why it is not.
- **Names say what things are.** Public names are `fude_<layer>_<module>_<what>`, parameters are `_name`, and file-local
  functions are `RDE_INTERNAL` (static).
- **One way of doing each thing.** Errors go through `rde_failed()`, logs through `rde_log_level`, and UI text through
  text ids.

## 7. Correctness and safety

- **Every change comes with tests**: aggressive, including random and fuzz cases where input varies. Suites live in
  `tests/`, run with `bash tests/run.sh`, and must all pass.
- **Visible changes are checked** against before and after screenshots from the look harness.
- **Failure is never silent.** A file that cannot be read or written is reported, logged, or both.
- **Threads:** shared state is guarded by a mutex or handed over through a documented hand-off. Each module's header
  says which threads touch it.
- **No user-visible text in code.** It comes from the strings files, in every language the app ships, and is never
  cut with an ellipsis.

## 8. Performance

- **Nothing is allocated, sorted, shaped or re-triangulated every frame** when it could be cached. A cache documents
  its key, its limit, and when it is invalidated.
- **No linear search where a map belongs**, and no O(n²) over user content.
- **Optimise only what is measured.** Use `--perf` and `--gate` (`COMMANDS.txt`), and note the numbers before and
  after in the change's design note.

## 9. Checked mechanically

`zsh tools/check/warnings.sh` lists every compiler warning in the apps' code, which the RDE builder does not show.
The goal is none.

`python3 tools/check/rules.py` counts what this file forbids, in `fude/` and `apps/`:
- C library and POSIX headers and calls (`malloc` and `qsort` among them), in the platform files too (their
  frameworks' headers are allowed);
- `void*`, where RDE writes `any`;
- app names in `fude`;
- includes against the layer table;
- files and functions over the size limits;
- public functions with no comment.

It does not find fixed-cap user arrays (§3); a review does.

Its counts only ever go down, and at release they are all zero, or each remaining one is listed with a reason.
