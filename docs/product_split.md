# Two apps from Sketching

Borja, 2026-10-10: Sketching is big and complete, but it covers so much that it is hard to sell — no one sees
themselves in it at a glance. Split it in two: a **note-taking app** ("Notability, but much better") and a
**hobby-projects app** (circuits, mechanisms, woodworking, floor plans), and later perhaps a **maths app**. This is the
plan: who each app is for, what goes in each, how the code divides, what Notes still lacks and how to build it, and an
order of work. The apps are **InfiNote** (Notes below) and **InfiniWorkshop** (Workshop below), as Borja named them on
2026-10-11 (§9); **Maths** is still a working name.

---

## 1. In short

- **Split: yes.** One engine (`fude/zoom`, `fude/sim`, `fude/drawing`), two apps that are each obvious in a sentence:
  - Notes — *write and draw on pages, and zoom into any of them without end; real maths; your PDFs.*
  - Workshop — *a project notebook whose drawings work: circuits that run, mechanisms that move, plans at true size.*
- **Workshop first.** It is nearly all built (0.1.46–0.1.78), and nothing in the stores is like it. That makes it the
  quickest way to test the store pipeline, pricing and the engine on strangers' devices. Notes is the bigger market, but
  it is crowded. To win there it needs features it does not have yet: pages, typeset maths and synced audio (§4).
- **Same file format in both.** A canvas made in one opens in the other. What one app does not know is drawn and left
  alone.
- **The code splits in three steps** (§3):
  1. profiles, which are cheap and safe (started: §8);
  2. untangling `page.c` (21,600 lines) into domain files, so each app links only what it uses;
  3. two shells, `apps/notes` and `apps/workshop`.

---

## 2. Who each app is for

| | Notes | Workshop | Maths (later) |
|---|---|---|---|
| Who | students, professionals, people keeping a diary or sketchbook | makers, electronics and woodworking hobbyists, DIY home projects, physics/engineering students | students and teachers of maths and science |
| Against | GoodNotes, Notability, Samsung Notes, Noteshelf, Nebo, OneNote | EveryCircuit and iCircuit (circuits only, on tablets), Falstad and Tinkercad (web), Fritzing (desktop), Linkage (desktop), floor-plan apps | Apple's Math Notes (iPad only, free), Desmos, GeoGebra, Photomath, Mathpix |
| What they lack that we have | infinite zoom; pages *and* an endless canvas in one notebook; precision instruments | a notebook around it all; circuits **and** mechanisms working on each other; true-size plans; woodworking | an Android equivalent of Math Notes; one place for notes, graphs and maths |
| What we still lack | paged notebooks, rich text written in place, LaTeX, handwritten maths, audio synced to ink, a home screen of notebooks | store polish, a projects home screen, its own name and icon | everything maths beyond `calc.c`/`plot.c` |

Points of reference:

- **Notability:** its best-known feature is time-synced audio. You tap a handwritten word and it plays from the moment
  it was written. GoodNotes records audio but does not sync it to ink.
- **GoodNotes:** freemium, with a one-time purchase. Notability needs a subscription for its full feature set.
- **EveryCircuit:** $14.99 one-time, with free circuits limited to 5 parts.
- **Apple's Math Notes** (iPadOS 18 and 26): solves handwritten equations as you write "=", draws interactive graphs
  (3D since iPadOS 26), and lets you scrub numbers. There is nothing like it on Android tablets.

---

## 3. What goes where

A **topic** (`FUDE_ZOOM_TOPICS[]`, page.c 5693) is already a per-canvas profile. It sets which tools, Insert entries,
instruments and exports the bar shows. The split is mostly a choice of topics per app, plus filtering within the topics
both apps keep.

| Topic | Notes | Workshop | Note |
|---|---|---|---|
| GENERAL | ✓ as "Notes" | ✓ as "Sketch" | Each app shows only its own Insert, instrument and export entries |
| PDF | ✓ | ✓ | Datasheets, plans and manuals in Workshop |
| MATHS | ✓ | — | Its graphs could join Workshop later for physics |
| DIAGRAMS | ✓ | — | Flowcharts, UML, Mermaid, Kanban: notes and study |
| TECHNICAL, WOOD | — | ✓ | |
| ELECTRONICS, MECHANISMS | — | ✓ | |
| FLOORPLAN, WIRING | — | ✓ | |
| SEWING | — | ✓ | A craft, so it goes with the projects |

**Shared core (both apps):**
- ink, brushes and smoothing; shapes; fill; lasso; layers;
- places, the map, areas and Present with the laser;
- text and sticky notes;
- handwriting to text (ML Kit), handwriting search, text to handwriting (Hershey);
- instruments (ruler, set squares, protractor, compass, templates, French curve, stencils as "custom rulers");
- export to PNG, SVG, PDF and video; My pieces; the canvas list and folders; Settings.

**Notes only:** maths (`calc.c`, `plot.c`), diagrams (`graph.c` is Mermaid's layout, not maths), Kanban, and everything
in §4.

**Workshop only:**
- circuits: `circuit.c`, `display.c`, `logic.c`, `limits.c`, `fude/sim`;
- mechanisms: `mech.c`, `mechrun.c`, `coupling.c`;
- examples: `examples.c`, `placer.c`;
- plans: `plan.c`;
- wood: `cut.c`, `nest.c`, `trim.c`;
- sheets to scale: `sheet.c`;
- DXF, STL, the cut list and parts lists;
- the workshop themes (Kraft, Blueprint, Cutting mat).

### How the code divides

1. **Profiles** (small, safe, started in §8). A product descriptor sets:
   - which topics the Topic menu offers;
   - which Insert, instrument, export and app-tool entries each kept topic may show (product ∩ topic);
   - the hobby-only lasso actions: Limits, Make part, Make body, To curve/Fillet/Fit parts, Chamfer.

   One build per app, chosen by a compile-time define. Everything is still linked; the user only sees their app.
2. **Untangling** (the big one, done as features are touched). `page.c` holds every domain's code and `page.h` every
   domain's state. Core modules call hobby code directly:
   - `render.c` → sheet, plot and parts;
   - `erase.c` and `select.c` → wires and boards;
   - `export.c` → parts, cut and DXF;
   - `shape.c` → boards;
   - `symbol.c` → circuit, plan and mech drawing.

   The plan:
   - Move each domain into its own page file (`page_circuit.c`, `page_mech.c`, `page_plan.c`, `page_wood.c`,
     `page_pdf.c`, `page_maths.c`, `page_diagrams.c`), each registering hooks in a table:
     - its tools and Insert entries;
     - its lasso actions;
     - its Play;
     - its drawing and export hooks;
     - its save fields.
   - The core calls through that table, so an app links only the domains it lists. That gives smaller apps, faster
     starts, and Notes free of circuit code.
   - Two smaller fixes on the way:
     - `fude_lang_ink_model` moves out of `page.c` (it would clash with `fude/lang/*/lang.c`);
     - Sketching's settings move out of the core `fude_settings`.
3. **Two shells.** `apps/notes` and `apps/workshop`, each with:
   - `<app>.c` and `src/<app>_app.c` (its `fude_app_info` and extension, as Kana's `kana_app.c`);
   - its own `version.h`, strings (`{APP}`), icons, Android package and iOS bundle id;
   - a branch in `tools/android/build.sh` (today Sketching has a hard-coded one at lines 64–84).

   The canvases already on Borja's tablet live under the save id "sketching". Workshop could keep that id so they carry
   over, or both apps could offer "Open a Sketching canvas".

---

## 4. Notes: what is new, and how

### 4.1 Paged notebooks (the flagship)

A notebook of pages, as many as you like, each of a set size, and each one an infinite-zoom canvas inside its edges.

**Model**
- A notebook is a canvas whose top frame lays out **pages**: bounded regions at true size, 1 unit = 1 mm (as PDF
  canvases are, `pdfview.c`), stacked down (or across) with a gap.
- Each page has:
  - a size (A4, A5, Letter, or custom), from `sheet.c`'s list;
  - an orientation;
  - a background: paper (lines, dots, squares, Cornell, blank) or a PDF page.

**Writing**
- Ink starts on the page under the pen and is clipped to it.
- Zooming in goes deeper inside that page (the frames already make this exact), and the view is held to the pages as a
  PDF canvas's is.
- Writing near the end of the last page adds the next page ("infinite pages").

**Pages**
- A strip of thumbnails to go to, insert, duplicate, move and delete pages.
- Export: one PDF page per page at true size (`pdf.c` writes PDF; export already does sheets this way).
- A PDF imported into Notes becomes a notebook whose pages are the PDF's. Blank pages can go between them.

**What it reuses:** `pdfview.c` (pages stacked, the view clamped, tiles), `sheet.c` (sizes), the paper lattice
(`render_paper`), and the export to sheets.

### 4.2 Text written in place, markdown, rich text

- **In place:** text is edited on the canvas, with the caret where you tap, not in a modal card (the card stays for
  prompts).
- **Rich text:** a text object becomes runs (bold, italic, a size, a colour) plus blocks (headings, bulleted and
  numbered lists, checkboxes, code, quotes).
  - Markdown is how they are typed (`# `, `- `, `**…**`, `` `…` ``) and how they are exported.
  - `$…$` and `$$…$$` are maths, set by §4.3.
- **From handwriting:** text converted from handwriting (ML Kit, already there) lands as such a text object, editable
  like any other.

### 4.3 Maths: LaTeX set properly, handwritten maths, and back

**Can we render LaTeX? Yes, with our own maths typesetter.**

- **The approach:** TeX's own rules for laying out maths (*The TeXbook*, Appendix G) with an OpenType **MATH** font. That
  covers:
  - fractions, sub- and superscripts, roots;
  - big operators with limits;
  - `\left…\right`;
  - matrices, cases and aligned equations;
  - accents, Greek letters and symbols, `\text`, `\mathbb` and the other alphabets.
- **Fonts:**
  - STIX Two Math, under the SIL Open Font License (free to ship in a paid app);
  - or Latin Modern Math, under the GUST Font License.
- **Drawing:** glyphs drawn as **vector outlines**, so a formula is as sharp at ×10⁶ as at ×1. Text today goes through
  a font atlas and stops at 3000 px.
- **Size:** about 3–5k lines of C, in the style of the rest.
- **The ready-made alternative:** MicroTeX (MIT, C++). It would be the codebase's first C++, and its drawing would need
  bending to infinite zoom.
- **Recommendation:** our own. The same module then serves the Maths app.

**Handwriting → LaTeX: possible, but it is the hard part.**

Google's ML Kit (our handwriting engine) has no maths model. The options:

| Option | Quality | Cost | Note |
|---|---|---|---|
| **MyScript iink** (Nebo's engine) | the best: about 250 maths symbols, on device, exports LaTeX and MathML | commercial licence, price on request | free to develop with |
| **Mathpix** | very good | paid per request | cloud only: notes leave the device |
| **Our own recogniser** | good for a core set if done well | our time | details below |

What building our own would involve:
- Pieces: a symbol classifier, the strokes grouped into symbols, and a 2D structure parser (baselines, scripts,
  fraction bars, roots, limits).
- Training data is the catch:
  - Google's MathWriting (630k samples) is CC BY-NC-SA, so it **cannot** train a model we sell;
  - CROHME is for research too.
- We would make our own data: expressions set by §4.3's typesetter and drawn by a handwriting synthesiser, plus what
  users correct.

Recommendation: prototype our own for the common core, and decide on MyScript only if it falls short.

**LaTeX → handwriting:** easy once the typesetter exists. Lay out with it, then draw each glyph in handwritten strokes,
as `hand.c` does for text (Hershey), extended to maths symbols.

**Later, as Math Notes does:**
- write "=" and get the answer (`calc.c` evaluates already);
- graphs from a handwritten function (`plot.c` draws already);
- numbers you scrub.

### 4.4 Audio synced to ink

Notability's signature, and the one feature people switch for.

- Record while writing. Every stroke already keeps its points' times (the codec's TIME channel).
- Tap any stroke to play from when it was written. A timeline replays the page as it was written.
- It needs microphone capture in RDE: an engine change on Android and iOS, beside 0.1.75's `rde_audio_stream`.

### 4.5 What a notes app is expected to have

- A home screen of notebooks with covers and thumbnails (the side panel's list is not enough), and templates.
- Search across notebooks: handwriting, text and PDFs (Find does one canvas today).
- Sharing, and backup to iCloud or Drive.
- Pen presets (fountain, brush, pencil), palm rejection checked on many tablets, and quick colours.

---

## 5. Workshop: what it needs before a store

- **A home screen of projects.** The examples are already in folders (0.1.77).
- **First-run guidance:**
  - "Insert → Examples";
  - a part from the library, wired, and played;
  - a mechanism.
- **A name and an icon,** and GENERAL renamed "Sketch".
- **Reliability on big canvases:**
  - 0.1.78 made big circuits fast;
  - "Everything at once" runs at 60 fps (0.1.60).
- **Pricing.** The usual pattern is a free tier with limits and a one-time unlock (EveryCircuit's). A subscription is
  hard to sell to hobbyists.

---

## 6. The Maths app (later)

It is built from what Notes needs anyway, plus a few new pieces:

| Piece | Status |
|---|---|
| The typesetter (§4.3) | shared with Notes |
| Handwritten maths (§4.3) | shared with Notes |
| `calc.c` and `plot.c` | exist |
| Implicit and parametric plots | new |
| 3D graphs | new |
| Sliders on any number | new |
| A computer-algebra core (simplify, solve, differentiate, integrate) | the big new part; it could start from an existing C library if one with a usable licence fits |

The opening: Apple's Math Notes is iPad-only, and Android tablets have nothing like it.

---

## 7. Order of work (proposed)

1. **Profiles and two preview builds** (started, §8).
2. **Workshop to the store:** its shell, name, icon, home screen and first-run guidance. Untangle `page.c` as each part
   is touched.
3. **Notes, in this order:**
   1. paged notebooks;
   2. text written in place, with markdown;
   3. the LaTeX typesetter;
   4. handwriting → LaTeX (our prototype first);
   5. audio synced to ink;
   6. the home screen.

   Its shell comes when the paged notebooks work.
4. **Maths** on Notes' maths pieces.

---

## 8. Done so far (2026-10-10)

- This plan, and the research behind it (§2, §4.3; sources below).
- **Profiles** (`fude/zoom/product.{h,c}`, infinite_canvas_design.md §60):
  - Sketching, Notes and Workshop come from one build define;
  - the Topic menu and each topic's tools are cut to the app's;
  - Notes and Workshop previews were built for the Mac (not installed anywhere).
- **Notebooks v1** (`fude/zoom/pages.{h,c}`, §60), in Sketching now as the Pages tool:
  - A4, A5, Letter or square pages;
  - always one empty page after the last written;
  - paper at its true spacing, finer as you zoom in;
  - PDF export at true size.
- **The two apps** (2026-10-11): `apps/infinote` and `apps/infiniworkshop`.
  - Each has its own `src/version.h` (name, version, product, and the app whose backups it opens), its own strings
    (`tools/strings.py`, `{APP}` its name), `config.rdef`, Android deps, iOS files and its icon.
  - Both build Sketching's shell (`apps/sketching/sketching.c`) with their own `src/`. Their assets are Sketching's
    shared ones with their own over them, put together at build time (`build/<app>-stage/assets/`).
  - `zsh tools/android/build.sh infinote|infiniworkshop --release` builds them.
  - Sketching stays the developer's build, with everything in it.
- **The LaTeX typesetter, and formulas on the canvas** (0.1.81, infinite_canvas_design.md §63). §4.3's plan is built,
  with one change: the font is **Noto Sans Math** (OFL) in place of STIX Two Math, because it is already shipped and its
  MATH table is complete.
  - `fude/zoom/mathfont.{h,c}` reads the font, `fude/zoom/tex.{h,c}` sets the maths by Appendix G, and
    `fude/zoom/formula.{h,c}` draws glyphs as cached triangles at nine levels of detail.
  - In InfiNote (and Sketching), Insert → Formula. The card previews the formula as it is typed.
  - SVG and PDF exports write the formula's outlines.
  - About 2,600 lines in all.
- **Graphs** (0.1.81, §64): equations and inequalities of x and y (shaded regions), curves of t, polar curves, curves
  followed finely and broken at asymptotes, roots, turning points, crossings and areas marked, and a view fitted
  round, one unit as long across as up.
- **Math Notes' first two** (0.1.81, §63): a sum ending in `=` is answered in place, and Graph on the lasso's row
  puts the held formulas on a graph beside them.
- **Both ways by hand** (0.1.82, §65):
  - LaTeX → handwriting: Handwriting on the lasso's row for formulas (`handmath.{h,c}`, with Hershey's maths and Greek
    sets);
  - handwriting → LaTeX: our own recogniser, Formula on the row for ink (`mathread.{h,c}`). It reads 93 of 96
    synthetic handwritten formulas; it needs real handwriting to tune with.
  - Graph legends are now set as maths. InfiniWorkshop has graphs (its Graphs topic; Borja, 2026-10-11).
- **Next for maths:**
  - tune the recogniser on real handwriting (Borja's strokes; a "correct it" loop that keeps what users fix as
    training data, §4.3);
  - more symbols in the reader (∏, ∂ limits, matrices) and handwritten maths read as it is written;
  - numbers you scrub.
- **Next for notebooks:**
  - a page number on screen, and a strip of thumbnails to go to, insert, move and delete pages;
  - the paper in the PDF export;
  - per-page templates;
  - a PDF's pages and blank ones in one notebook.

## 9. Decided (Borja, 2026-10-11)

1. **The names:**
   - **InfiNote**, the notes app: package `com.rde.infinote`, save id `infinote`;
   - **InfiniWorkshop**, the projects app: `com.rde.infiniworkshop`, `infiniworkshop`.
2. **Price:** paid once, a one-time purchase (no subscription).
3. **The canvases on the tablet go to InfiNote.** Android keeps each app's files to itself, so they move over by Your
   data:
   - export in Sketching;
   - import in InfiNote, which opens Sketching's backup files (`fude_app_info.imports`).

   InfiniWorkshop can open them too.
4. **Diagrams and Kanban are in both apps.**
5. **Handwritten maths: our own recogniser.** A paid SDK such as MyScript would make the app too expensive to sell.
   Training data cannot come from MathWriting (non-commercial). We make our own (§4.3): expressions set by our
   typesetter and drawn by a handwriting synthesiser, plus what users correct.

Still open:
- whether InfiniWorkshop ships first (the recommendation, §1);
- the platforms after Android (iPad);
- (The icons are done: `tools/icons/infini_icons.py` draws them. InfiNote's is a notebook page with an infinity written
  across it in a fountain pen's line and a small one inside its loop; InfiniWorkshop's is a blueprint with an amber
  infinity whose right loop is a gear and whose left loop runs circuit traces.)

## 10. Backlog: handwritten maths, on hold (Borja, 2026-10-11)

Borja stopped this until there is real handwriting to train on. Until then, **formulas are hidden from InfiNote and
InfiniWorkshop**. Notes' product no longer has `FUDE_ZOOM_INSERT_FORMULA`, and the lasso's Formula, Handwriting and
Graph-from-formula check `fude_zoom_formula_present()`. **Sketching keeps them all** (the developer build). Formulas
already on a canvas still draw and can be edited by tapping them with the text tool. Graph legends set as maths stay.

**What is built** (infinite_canvas_design.md §63–§65, all tested):
- the typesetter (`tex`, `mathfont`), formulas on the canvas (`formula`), sums answered and Graph from formulas
  (`texcalc`);
- LaTeX → handwriting (`handmath`, Hershey's maths and Greek sets in `tools/hershey/`);
- the reader, handwriting → LaTeX (`mathread`: $P symbols, segmentation by dynamic programming, layout).

It reads 93 of 96 formulas the app writes by hand itself, but **failed on Borja's first real test**. His `√(b²)` came
out as `π(k·)`, for three reasons:
1. a `√` drawn in one stroke with a long bar over everything: its templates had short bars;
2. the small `²` taken for a dot: the dot rule went by size only, and a dot is also a short stroke;
3. `b` merged with a `²` that touches it into `k`: touching was penalised as hard as crossing, and should be much
   weaker.

These strokes, traced from his screenshot, are a ready test (scratch `realtest.c`; to go into `test_mathread`). They
currently read as `\in ^{2})`.

**Collecting real handwriting (where it stands).** Borja writes samples in InfiNote on ruled paper, one row per item,
many copies in varied hands. When a row is skipped it is left empty, so the order alone gives the labels.
- Done on the tablet (2026-10-11, not exported yet): group 1, digits 0–9; group 2, lowercase a–z with ñ (Spanish
  order; check that w is there).
- Still to write:
  3. operators: `+ − × ÷ · ± = ≠ < > ≤ ≥ ≈ → ⇒ ∈ ∞`;
  4. brackets, half short and half tall in each row: `( ) [ ] { } |`;
  5. big symbols:
     - `√` with a short bar;
     - `√` with a long bar (just the sign);
     - `∑ ∏ ∫ ∂ ∇`;
  6. Greek: `α β γ δ ε θ λ μ π ρ σ τ φ ω`;
  7. capitals A–Z, then `Γ Δ Θ Λ Σ Φ Ψ Ω` (about 6 each);
  8. 42 formulas, each written twice on its row:
     1. x² + y² = r²
     2. a² − b² = (a − b)(a + b)
     3. 1 over 2
     4. a + b over c
     5. x over y + 1 over x
     6. 1 over 1 + x²
     7. (1 over 2) over 3
     8. √2
     9. √(x + 1)
     10. √(a² + b²)
     11. ∛8
     12. x₁ + x₂
     13. xᵢ²
     14. e^(x+1)
     15. 2ⁿ + 1
     16. aₙ = aₙ₋₁ + d
     17. ∑ i=1..n of i
     18. ∑ k=0..∞ of 1 over k! = e
     19. ∫ 0..1 of x² dx
     20. ∫ a..b of f(x) dx
     21. ∏ i=1..n of i = n!
     22. lim x→0 of (sin x over x) = 1
     23. f′(x) = 2x
     24. dy over dx = 3x²
     25. ∂f over ∂x
     26. y = mx + b
     27. y = sin x + cos x
     28. log₂ 8 = 3
     29. ln e = 1
     30. x = −b ± √(b² − 4ac), all over 2a
     31. |x − 3| < 5
     32. 3.14 × 2 = 6.28
     33. α + β = γ
     34. θ ≈ 0.5
     35. x ≤ 2
     36. x ≠ 0
     37. f(x) = (x + 1)²
     38. x ∈ [0, 1]
     39. {1, 2, 3}
     40. E = mc²
     41. F = ma
     42. n! = n(n − 1)!
- How the data comes over: ☰ → Settings → Your data → **Export**, saved to Downloads, then `adb pull` from
  `/sdcard/Download`. The backup keeps every stroke's points and their order, which a screenshot loses.

**Next, when Borja is back:**
1. A tool that reads the backup and turns it into labelled samples. It cuts each canvas into rows (the paper's lines)
   and each row into copies by gaps, then gives each copy its label from the row's place in the list.
2. Templates built from his samples (several per symbol) in place of, or beside, Hershey's.
3. Tune the segmentation (touching, dots, long root bars) and the layout on his formulas. Measure on a held-out half.
4. A "correct it" loop: when a reading is fixed in the formula card, keep the strokes and the fix as more samples, on
   the device.
5. Then show formulas in InfiNote again.

---

Sources (searched 2026-10-10):
- ML Kit digital ink: [Google Developers Blog](https://developers.googleblog.com/digital-ink-recognition-in-ml-kit/)
- MyScript: [SDK](https://www.myscript.com/sdk/), [pricing note](https://medium.com/@myscriptdeveloper/new-pricing-on-myscript-developer-79c22f44e17a)
- Mathpix: [Digital Ink](https://mathpix.com/digital-ink)
- MathWriting: [the paper](https://arxiv.org/html/2404.10690v1), [its licence on Hugging Face](https://huggingface.co/datasets/deepcopy/MathWriting-human)
- MicroTeX: [GitHub](https://github.com/NanoMichael/MicroTeX)
- Notability and GoodNotes: [affine.pro comparison](https://affine.pro/vs/goodnotes-vs-notability), [Rambox](https://rambox.app/blog/goodnotes-vs-notability/)
- EveryCircuit: [everycircuit.com](https://everycircuit.com/), [circuitsim.com](https://circuitsim.com/p/everycircuit-alternative)
- Apple Math Notes: [MacRumors, iPadOS 26](https://www.macrumors.com/2025/06/12/ipados-26-math-notes-3d-graphing/), [Apple newsroom, iPadOS 18](https://www.apple.com/newsroom/2024/06/ipados-18-introduces-powerful-intelligence-features-and-apps-for-apple-pencil/)
