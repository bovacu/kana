# PDF, and the threads

Two things that meet in one place. A PDF's pages are the user content too slow to draw on the main thread, and the
PDF worker is the most worked-out hand-off between threads in the apps; every other thread keeps the same rules. This
note is in three parts: PDFs coming in (shown under the ink, read, searched), PDFs going out (the writers and the
exports), and every thread the apps run, with the rules a change must keep.

`docs/architecture.md` ("Documents", "Threads", "Caches") is the map; this goes a level deeper. Each module's header
has the details of its own calls.

---

## The parts

Paths are under `fude/`.

| File | Layer | What it is |
|---|---|---|
| `drawing/doc/pdf.h` | doc | the platform's PDF library behind one C interface: open, measure, draw into pixels, read text, search, write |
| `drawing/doc/pdf.c` | doc | Apple's: Core Graphics draws and writes, Core Text sets text, ImageIO reads pictures; stubs elsewhere |
| `drawing/doc/pdf_kit.m` | doc | Apple's text and search: PDFKit, a `PDFDocument` of its own beside Core Graphics' |
| `drawing/doc/pdf_android.c` | doc | Android's: JNI to `FudePdf.java` (PdfRenderer draws, PdfDocument makes a PDF of pictures) and `FudePdfBox.java` (PDFBox: text, search, writing) |
| `drawing/doc/worker.h` | doc | the PDF worker: pieces drawn on a thread, tiles on the GPU |
| `drawing/doc/doc.h` | doc | `fude_doc`: the study apps' document under a canvas's ink (layout, reading scans, search, turns, Share as PDF) |
| `zoom/pdfview.h` | zoom | `fude_zoom_pdfview`: the canvas apps' canvas over a PDF |
| `zoom/pdf.h` | zoom | a PDF 1.4 writer in plain C, for the canvas's exports |
| `zoom/export.h`, `export_formats.c` | zoom | the exports: one walk of the scene, a sink per format |
| `study/models/sheet.h` | study | practice sheets, a PDF written by hand |
| `drawing/app/docbar.c`, `import.c` | app | the document bar (search, go to page, turn, Share as PDF); PDFs, pictures and scans brought in |

Who owns what: the study shell's page (`drawing/app/page.c`, `fude_page_input.doc`) owns one `fude_doc` for the app's
life and updates it each frame. The canvas shell makes the same page (and its `fude_doc`) but never updates it; its
deep-zoom page owns a `fude_zoom_pdfview` (`fude_zoom_page.pdf`), opened when a canvas over a PDF opens
(`page_file.c`) and closed when it closes. Each of the two keeps its own `fude_doc_worker`; the worker module and its
one mutex are shared. In practice an app runs one of them.

---

## PDF in

### The platform layer (`pdf.h`)

A `fude_pdf` is an open document. `fude_pdf_open` reads every page's size there and then, on the main thread
(`FudePdf.pageSizes` opens each page once on Android), so that the thread which draws pages only ever draws. A PDF
that does not open, has no pages, or is locked with a password other than the empty one is refused; a caller then
shows its ink alone and does not try again while the canvas is open (`fude_doc.failed`).

Each platform keeps two documents for one file. Apple's draws with Core Graphics' `CGPDFDocument` and reads text with
a PDFKit `PDFDocument`; Android's draws with `PdfRenderer` and reads text (and writes) with a PDFBox `PDDocument`. The
text side is opened the first time text is asked for, and tried once (`fude_pdf.kit_tried`, `FudePdfBox.Text.tried`).
Why two: neither drawing library gives a page's text (PdfRenderer has none at all, Core Graphics only the page's
operators), and PDFKit and PDFBox give the text and where it is. Apple's drawing stays in plain C (`pdf.c`), its text in
the one Objective-C file (`pdf_kit.m`). On Android a PDF in the app's assets is first copied to the cache
(`FudePdf.fromAssets`), because PdfRenderer needs a file it can seek in. Windows has neither yet: `fude_pdf_available`
is false.

Every position crossing `pdf.h` is in points from the page's top-left, on the page as it is read: turned as the PDF
says, then as the learner turned it (`fude_pdf_set_turn`, quarter turns, kept beside the PDF and never written into
it). PDFKit and PDFBox know only the PDF's own turn, so `pdf.c` and `pdf_android.c` convert both ways
(`fude_pdf_unturn`, `fude_pdf_return`, `fude_pdf_box`). A turn may be set only while nothing of the page is drawn.

### Pieces and tiles (`worker.h`)

A **piece** is one job for the worker: a part of one page drawn into pixels, for one of three uses (`FUDE_DOC_PIECE_`):
a page **whole** (mipmapped: it is seen small too), a **sharp** piece (the part of a page on screen, at the screen's
pixels), or a page's picture for the app's **reader** (left as drawn). A whole page is drawn at about its size on the
screen (`fude_doc_whole_size`): `FUDE_DOC_WHOLE_PX` (1100) wide at most, halved while the half is still as wide as the
page on screen, down to `FUDE_DOC_WHOLE_LEAST` (17), so the picture is from once to twice the page's width on screen,
and a document zoomed out over hundreds of pages draws each one small. A page under `FUDE_DOC_SEEN_LEAST` (8) pixels
wide on screen is not drawn whole: it shows as its paper. No texture made here is longer than `FUDE_DOC_SHARP_MAX`
(4096) pixels on either side, a whole page's included (a long receipt is drawn narrower to fit): the least Vulkan
promises, which every Android GPU the apps draw with gives.
A **tile** (`fude_doc_tile`) is a drawn piece on the GPU, with the part of the page it shows and the zoom it was drawn
for. Each page has two: `whole` and `sharp`.

```
 main thread                                      the piece's thread (a detached rde_thread, one per piece)
 fude_doc_worker_start   (only when not busy)
   piece filled: PDF, page, part, size, colours
   worker.job = piece ─────────────────────────► fude_pdf_render into piece->pixels (top row first)
                                                  a page or sharp piece: rows flipped; darkened in a dark theme
                                                  lock: orphaned? done = true; broadcast; unlock
                                                  orphaned: frees the piece, and closes its PDF
 each frame: fude_doc_worker_take
   not done            → NULL: look again next frame (nothing waits)
   stale               → freed here, NULL
   done                → the caller's: fude_doc_tile_take (GPU), or the reader, or dropped (ok false);
                         then fude_doc_piece_free
 fude_doc_worker_colours: the theme changed → stale = (a piece is running)
 fude_doc_worker_forget: a page turned      → stale = (a piece is running)
 fude_doc_worker_wait (a PDF written out): waits on the condition variable until done; still job, taken as ever
 fude_doc_worker_drop (the PDF closing): done → freed, the caller closes the PDF
                                         running → orphaned, close_pdf; the caller forgets the PDF
```

| State | Means | Who frees it |
|---|---|---|
| asked | made and filled by `fude_doc_worker_start`; `job` points to it; `busy` is true | (it is running) |
| drawing | its thread reads the piece and the PDF, writes only `pixels` and `ok` | |
| done | `done` set under the mutex; still `job` until taken, so no second piece starts | |
| taken | handed to the caller by `fude_doc_worker_take`; every field is the main thread's | the caller, `fude_doc_piece_free` |
| stale | the theme changed (`fude_doc_worker_colours`) or a page turned (`fude_doc_worker_forget`) while it drew; it stays `job` until done | `fude_doc_worker_take` throws it away |
| orphaned | its PDF was let go while it drew | its own thread, closing the PDF too |

Why it is built this way:
- **One piece at a time.** `pdf.h` asks that a document be drawn on one thread at a time, on every platform
  (PdfRenderer allows one page open at a time). A caller starts a piece only when `fude_doc_worker_busy` is false.
- **A thread per piece, detached.** Nothing ever has to stop, wake or join a worker: a piece no longer wanted finishes
  on its own. The code says no more than this about the choice; it costs a thread start per piece, which is small
  beside drawing a page.
- **One mutex, guarding two flags.** `done` and `orphaned` are the only fields both threads touch. Everything else in a
  piece belongs to one thread at a time: the main thread until it starts, its thread until `done`, then whoever frees
  it. A condition variable made with the mutex (`fude_doc_worker_done`, never freed) is broadcast each time a piece is
  done, for the one wait there is: a PDF written out draws the PDF's pages on the main thread, so it first waits for
  the piece being drawn (`fude_doc_worker_wait`), at most one piece's drawing, and no new piece starts until it is
  through.
- **Stale and orphaned, never freed under the thread.** A theme change or a page turned cannot drop the running piece
  (its thread is still writing into it), so it is marked and thrown away when taken; this also keeps it `job`, so no
  second piece is started on the same PDF meanwhile (`docs/bug_log.md`: the theme change once orphaned it and started
  another). Closing the PDF is the one case that leaves a piece to its thread, which then closes the PDF itself, so a
  PDF is never closed under a render. `pdf.h` allows that close on the worker for every platform.
- **Flip and darken on the thread.** A memory texture's first row is its bottom, and a dark theme turns white paper into
  its page colour and black print into its text colour (`fude_doc_worker_darken`), so the main thread only copies to
  the GPU. The reader's picture is left top row first, in its own colours.
- **Allocators a thread may free into.** The piece is `rde_calloc`'s (the engine's pool, made thread-safe by RDE), its
  pixels the std allocator's. A tile's memory texture, and the copy of the pixels it keeps, are on the std allocator
  too (`fude_doc_tile_take`), as anything as big as a page is: the engine's pool is a fixed block (the app's
  `config.rdef`), which a few pages and a sharp piece (up to 64 MB) would fill.

### The two callers

| | `doc.c` (study apps) | `pdfview.c` (canvas apps) |
|---|---|---|
| Layout | pages down from canvas y 0, `FUDE_DOC_PAGE_W` (1000) units wide, 48 apart | pages at their true size, a unit a millimetre, 8 mm apart |
| View | the reader's frame (`canvas.h`): the pages' width fits the screen at 100%, the least zoom | kept over the pages (`fude_zoom_page_keep_view_over`), out until all fit, in as deep as any canvas |
| Order of pieces | the same (`fude_doc_next`, `fude_zoom_pdfview_next`, from what `fude_doc_wholes` chose): pages on screen whole, nearest the middle first; once the view has rested `FUDE_DOC_REST` (0.15 s) and the screen gives a page over 15% more pixels than its whole picture, the sharp pieces; then whole pages within a screen round, while they fit the budget (below) | |
| A sharp piece kept while | its zoom is within 2% and it covers what is on screen (to half a point) | the same, within 1% of what is on screen |
| Turning pages | yes (`fude_doc_turn_page`) | no |
| Reading scans | yes (below) | no: Kana's reading of pictures was not brought over (`docs/infinite_canvas_design.md` §19) |
| Writing out | `fude_doc_export` (Share as PDF) | `fude_zoom_pdfview_export` (Export → Document over a PDF) |

`fude_doc_update`, once a frame, in this order: drop a reading no longer wanted (`reader_drain`); open the document the
open canvas names, closing the one before; ask four more pages whether they have text (`fude_doc_probe`, only once
`fude_pdf_text_prepare` says the PDF's text is ready) and take the reader's answer; gather a search's matches; take
the canvas's page turns (a turn waiting marks the piece being drawn stale, `fude_doc_worker_forget`; the turns are
taken only while the worker is not busy, and no piece starts while a turn waits, so one piece after another cannot put
it off); keep the view on the pages; take the theme's colours (dropping every tile when they change); note whether the
view moved; take the finished piece; choose the whole pages to keep and the next to draw (`fude_doc_wholes_plan`,
through `fude_doc_wholes`: those no longer kept are let go there and then); start the next piece, else the next page
to read; let go of the sharp pieces of the pages off screen (`fude_doc_let_go`, which frees nothing else).
`fude_zoom_pdfview_update` does the same for what it has (no turns, no reading): the theme's colours, whether the view
moved, the finished piece, its plan (`fude_zoom_pdfview_wholes`), the next piece, the sharp pieces off screen let go
(`fude_zoom_pdfview_let_go`), then the search's matches. It begins the PDF's text as the PDF opens
(`fude_pdf_text_prepare`), so on Android PDFBox is opening it before Find or a lasso asks.

### Text in a PDF

A page's own text: `fude_pdf_page_has_text` and `fude_pdf_text_in` (the lasso's box, a line between pages), on the main
thread. Android keeps each page's "has text" answer and the text of the pages asked for last, about 400,000 characters
(`FudePdfBox.CHARS_KEPT`), so a search through a book does not hold the whole book. PDFBox takes seconds to open a big
PDF on a phone, so `fude_pdf_text_prepare` begins the opening on FudePdfBox's own thread ("FudePdfFind":
`Text.open`) and says when it is done; until then a question about a page's text would wait for the rest of the
opening, so `doc.c` asks its pages only once it says so. Letting the text go (`FudePdfBox.close`) never waits either:
the search stops at its next page and the document is closed on that same thread (`Text.close`). Apple's PDFKit opens
its document at once, so there `fude_pdf_text_prepare` is always true.

**Search** (`fude_pdf_find_start`, `_matches` once a frame, `_stop`). Case, accents and full- or half-width forms match
alike. It keeps at most `FUDE_PDF_MATCHES` (512): the matches are copied out and marked every frame, and the bound keeps
that frame's work small however common the word (`pdf.h` says why).
- **Apple:** the search goes a page at a time, each page's text (PDFKit's `string`) searched from the last match on,
  for 6 ms each time the matches are asked for, on the main thread (`pdf_kit.m`: `-[FudePdfText work]`), because
  PDFKit's own background search answers through a run loop the app's frame does not turn. The time is looked at after
  every page and every match, so no call runs on: PDFKit's `findString` went on through the whole document until it
  found one, which held a frame for as long as a rare or missing word took in a long book. Each page's text is searched
  alone, so a match that runs across two pages is not found. It is done at the document's end, or at 512 matches.
- **Android:** a single-thread executor of `FudePdfBox`'s ("FudePdfFind") reads page after page. Each search has a
  number (`Text.search`); start and stop move it on, and the thread checks it between pages and before keeping a match,
  so a stopped search's late matches are dropped. Its matches change `Text.version`; `findMatches` hands them over only
  when the version moved, and reads them under the `found` lock alone, never PDFBox's `LOCK`: the documents' map has a
  lock of its own, so asking never waits for a page being read (it once froze the app for half a second at a time).
  Starting and stopping a search take no `LOCK` either: a search's number and matches are under `found`, and the
  search itself runs on "FudePdfFind".
- `doc.c` adds the matches in pages read from pictures after the PDF's own (`fude_doc_search_lines`, compared by
  `fude_utf8_find`), and a search is done only when no page is left to read. `pdfview.c` flies to the first match as it
  comes (`fude_zoom_pdfview_search_wanted`).

### Scans

There is no scan module under `drawing/doc`. A scan is a PDF of pictures: the document camera, Photos and pictures
from Files come in through `import.c`, which makes them into one PDF a page each (`fude_pdf_from_images`: upright, 150
dots an inch, at most `FUDE_PDF_IMAGE_PX` (2400) pixels on the longer side, kept as JPEG), on the main thread. Such a
page has no text of its own, so `doc.c` reads it:

1. `fude_doc_probe` asks four pages a frame whether they have text; one without is `FUDE_DOC_TEXT_TO_READ`.
2. When the worker is idle and no reading is out, the page nearest the view (or the one a lasso is waiting on,
   `wanted`) is drawn as a reader's piece, `FUDE_DOC_READ_PX` (1600) wide, at most `FUDE_DOC_READ_MAX_H` (2400) tall:
   a receipt is squeezed rather than handed over as a picture the reader takes too long on.
3. Taken, it goes to the extension's `picture_read` (the study's `fude_study_picture_read`, which hands it to
   `textscan.h`'s page channel: ML Kit text recognition). The pixels are copied before `picture_read` returns, because
   the piece is freed right after. A reader that refuses turns reading off for that document (`reader_off`).
4. `picture_lines` is polled each frame; the lines are turned from the picture's pixels into page points, kept, and the
   whole `<id>.lines` file (a kfile, kind 'LINS', a 'PAGE' chunk a page, format at the top of `doc.c`) is written at
   once, so a page is never read twice. A page whose picture could not be drawn is `FAILED`: not saved, tried again at
   the next opening. Lines of the app's own books are never kept (they have text of their own).
5. A reading no longer wanted (its document closed, its page turned) is drained: `reader_drain` takes the answer when it
   comes and drops it.

On the Mac, where ML Kit is not, `--read-demo` (study's look) reads every page with made-up lines.

### Memory, and what bounds it

| What | Bound | Where it lives |
|---|---|---|
| Whole pages | by bytes: those on screen always (each at most about twice its width on screen, so together a few screens' worth); the others nearest the view first while all kept fit `FUDE_DOC_KEEP_BYTES` (48 MB, at 4 bytes a pixel): about seven A4 pages at 1100 × 1556, or many more seen small (`fude_doc_wholes`) | each byte twice: on the GPU (a third more for the mipmaps) and in the memory texture's copy, which is on the std allocator (`fude_doc_tile_take`). An A4 page at full width is 6.8 MB |
| Sharp pieces | only pages on screen; freed as a page leaves the screen; at most `FUDE_DOC_SHARP_MAX` (4096) pixels on the longer side | as above, no mipmaps |
| The piece being drawn | one per worker | its pixels on the std allocator, freed when taken |
| A reader's picture | one at a time, 1600 × 2400 at most (15 MB) | copied by the reader, the piece freed |
| Matches | 512 (`FUDE_PDF_MATCHES`) | one array per document, made on the first search |
| PDFBox's text | about 400,000 characters, the pages asked for longest ago go first | Java's heap |
| Read lines | not bounded: they are the scans' text, the user's | the document's `lines` |

**Why by bytes, and drawn at their size.** Whole pages were once kept by count, the six nearest the view, each drawn
1100 pixels wide. That left two gaps, both closed now. A page much taller than it is wide (a long receipt) made one
texture as tall as its shape asked; now `fude_doc_whole_size` shrinks such a page both ways to fit
`FUDE_DOC_SHARP_MAX`. And the band the next piece is chosen from (the view and a screen round) could hold more than six
pages (short, wide pages on a tall screen; a PDF canvas zoomed out until all its pages fit): the farthest was let go,
the next piece drew it again, and the worker never rested, so a document's scans were never read. Now a page is drawn
only when `fude_doc_wholes` keeps it, so nothing drawn is let go at once, and once every kept page is drawn the worker
rests and the reader has its turn. Drawn at about its size on screen, a page seen small costs little, so a document
zoomed out over hundreds of pages shows every one (`tests/docwork`: `test_wholes_small_pages`,
`test_wholes_zoomed_out`, `test_wholes_budget`).

**A tile's copy cannot be dropped.** RDE's `rde_memory_texture_gpu_upload` keeps the memory texture's pixels after the
upload and does not look at the parameters' `free_data`, so each tile's pixels are held twice, on the CPU and the GPU.
Dropping the CPU copy is a change in RDE, the owner's.

---

## PDF out

### Three writers, and why three

| Writer | Used by | Pages | Text | Pictures |
|---|---|---|---|---|
| `drawing/doc/pdf.h`'s `fude_pdf_write_*` | Share as PDF (`fude_doc_export`); Export → Document over a PDF (`fude_zoom_pdfview_export`) | copied from an existing PDF as they are, so their text stays text; strokes, fills, pictures and text drawn over | Roboto in four faces from the assets (Apple: `CGFont`, Core Text finding other fonts for what it lacks; Android: PDFBox, the face embedded whole, a missing letter left out). Read lines go in unseen: Hiragino Sans W3 on Apple's (named, so every device writes the same), a glyphless font of its own on Android's | ImageIO / a PDFBox image, JPEG or PNG bytes on three corners |
| `zoom/pdf.h` (`fude_zoom_pdf_*`) | every canvas export of the view, its areas, a notebook's pages, sheets to scale, 1:1 tiles, the printer's check | made new, in memory, PDF 1.4 | the fonts the screen uses, each embedded as a Type 0 font (TrueType cut down to the glyphs used, CFF whole) with widths and a ToUnicode map; Helvetica when none are set | a JPEG passed through as DCTDecode; RGBA as an RGB image with its alpha as a soft mask |
| `study/models/sheet.c` | practice sheets | made new, in memory, A4 | Helvetica only; characters drawn from their strokes as curves, so no font goes in | none |

Why three. Copying a page of an existing PDF needs a PDF reader, which only the platform libraries give (Core Graphics
draws a page into a PDF context as vectors; PDFBox imports its content). A canvas export has no PDF under it, so it
needs no reader: `zoom/pdf.h` writes it in plain C with no library, the same bytes on every platform, tested on the
Mac. Practice sheets are in the study layer, which may not include `zoom` (the layer table), and need no font: a
character is its strokes.

### Share as PDF: a document with its ink (`DOC_EXPORT`)

`fude_doc_export` writes each page: copied from the PDF as the learner turned it (`fude_pdf_write_page`), then the ink
whose bounds touch it in two passes, the marker's strokes first (under the rest, as on the canvas, thinned the way the
renderer thins them, `FUDE_INK_MARKER_STEP`), then the page's read lines as unseen text. The pages are white paper, so
ink in the theme's colour is written in the light theme's ink. Core Graphics reports no error on closing, so
`fude_pdf_write_end` checks the file begins with `%PDF-`. On a device the docbar writes it to the outbox and hands it to
the share sheet (`fude_session_outbox_path`, `rde_mobile_share_file`); on a computer a save dialog asks where first.
`fude_zoom_pdfview_export` is the canvas apps' twin: the scene's objects at any depth, in drawing order, each kind
written its own way, a text's lines broken as the canvas breaks them.

### The canvas exports

Every export walks the scene once as the renderer would (`fude_zoom_export_walk`), handing what to draw to a sink of
its format, so all of them show what the screen shows. `page_export.c: fude_zoom_page_export_write` picks the format
(numbered as `page.h`'s `fude_zoom_page_look_export` lists them): PNG (0), SVG (1), PDF (2), DXF (3), 1:1 tiles on A4
and Letter (4, 7), the printer's check (5), the cut list (6), the zoom video (8), areas (9), sheets to scale (10), STL
(11), the parts list (12). PDF is the view, or a notebook's pages at their true size, or, over a PDF, the PDF itself
with its ink. Everything but PNG and the video is written at once into a `fude_bytes`, then to the file. A PNG is drawn
into a render texture in one frame's render and read back the frame after (`export_stage` 1 then 2,
`fude_zoom_page_render_offscreen`, `fude_zoom_page_export_finish`), because the GPU has not drawn it until then. The
video goes the same way, a frame at a time (stages 10 and 11).

Before each export the PDF writer is given the screen's fonts (`fude_zoom_page_pdf_fonts`: each face's Roboto, the
app's script and Latin fonts, Phosphor's icons behind the regular face). The font files are read the first time a
character is looked for and let go when no document made with them is left (`fude_zoom_pdf_free`), so fonts take memory
only while a PDF is being made. In the PDF sink a picture's bytes are a JPEG when they start `FF D8` (passed through);
anything else is decoded (`rde_image_decode`) and added as RGBA. The same picture drawn twice is put in once (the first
64 pictures met are remembered).

### Practice sheets

`fude_sheet_write` builds the whole PDF in memory and writes it in one go: a character's meaning and readings, its
stroke order, then rows of boxes (whole and numbered, faded to trace, empty with the dashed cross). At most
`FUDE_SHEET_MAX` (200) characters, six a page: a sheet is printed, and 200 already make 34 pages. Main thread only
(`study/app/files.c: fude_study_sheet_write`, from the viewer, Select mode and the Vocabulary screen).

---

## Threads

### The main thread, on each platform

Everything runs on the thread that runs `rde_run`'s callbacks: the app, the UI, the session and every file. On the Mac
and iOS it is the process's main thread, and on iOS a block a library puts on the main queue (ML Kit's answers, the
text scanner's) runs between two frames on that same thread, so the state it sets needs no lock. On Android it is SDL's
thread, **not** Android's UI thread: anything that answers on Android's UI thread (ML Kit, text-to-speech, translation)
answers on another thread, and its Java side keeps the answer behind a lock or in a volatile field.

### Every thread

| Thread | Started by | What runs there, and what it touches | How the result comes back | A late answer is dropped by |
|---|---|---|---|---|
| A PDF piece | `fude_doc_worker_start`: a detached `rde_thread` a piece | `fude_pdf_render` on the piece's PDF; the piece's pixels and `ok` only | `done` under the shared mutex, taken once a frame (a PDF written out waits for it: `fude_doc_worker_wait`) | `stale` (theme, a page turned), `orphaned` (PDF closed) |
| PDFBox's search (Android) | `FudePdfBox.FINDER`, "FudePdfFind" | opens and closes PDFBox's document (`Text.open`, `Text.close`) and reads pages' text, under `LOCK` there; keeps matches under `found` | matches and `version` under `found`, polled each frame; the opening's end as `Text.opened` (volatile), read by `fude_pdf_text_prepare` | the search's number |
| Speech to text | the first `fude_listen_submit`: one `rde_thread`, kept until `fude_listen_shutdown` | loads whisper's model, reads one piece (its own copy of the samples), writes its own transcript; whisper reads with 2 or 3 CPU threads (`n_threads`), the others its own | `fzls_job`, `_running`, `_done` under `fzls_lock`, a condition variable to wake it; progress in volatile numbers read without the lock, each written whole by one thread; the guesses they are weighed by (`fzls_encode_s`, `fzls_share`) set as a piece ends and read, both under the lock | the page drains a piece whose canvas has gone (`heard_reading` 0) |
| A download | `fude_fetch_start`: NSURLSession's delegate queue (Apple), the Java thread "FudeFetch" (Android) | writes `<path>.part`, then moves it into place | a volatile state and byte counts, polled each frame | one at a time; the file appears only whole |
| Audio | RDE's (SDL's) audio devices | fill the microphone's ring, drain the speaker's (a second of sound each) | the page pumps them once a frame (`voice.c`) | |
| ML Kit handwriting | ML Kit | iOS: answers on the main queue; Android: on the UI thread into `FudeInk`, under its `LOCK` | `fude_mlkit_poll` once a frame | `fude_mlkit_generation`, `FudeInk.generation`; given up after 10 s |
| Text in photos and scans | `textscan` | iOS: pictures made upright on the serial queues "fude.textscan.frames" and ".pages", ML Kit answering on the main queue; Android: the UI thread, and a Java thread reading a picked photo | polled each frame; Android's behind `FudeText`'s lock | `reader_drain` (a page's reading) |
| Translation | `translate` | iOS: the main queue; Android: the UI thread into `FudeTranslate` | polled each frame; Android's behind a lock | |
| Speech (Android) | `FudeSpeech` | TextToSpeech, made and answering on the UI thread | a volatile state | |
| The pickers (Android) | `FudeImport` | a Java thread copies what was picked | `paths`, then `kind`, both volatile, polled by `fude_import_update` | |
| A crash | the signal, on the thread that crashed | RDE's handler: open, write, close, nothing else | the report, read at the next start | |

Not threads, but worth knowing. A computer's file dialog answers on the main thread on the Mac (SDL's sheet or modal
panel); SDL calls back on a thread of its own on Windows and Linux, which a port must bring back to the main thread.
The zoom video waits for the system's encoder on the main thread, up to 2 s a frame and 20 s to finish (`video.h`).

**Speech to text** (`listen.c`) is the model for a long-lived worker. The page hands in one piece (at most
`FUDE_LISTEN_PIECE_MS`, 28 s of 16 kHz samples, copied) only when no piece is waiting, being read or done. The thread
waits on the condition variable for `fzls_job` or `fzls_stop`, marks the piece running under the lock, reads it outside
the lock (the main thread writes the piece only while all three flags are clear), then marks it done. The main thread
takes it with `fude_listen_take`, which frees the module for the next. `fude_listen_shutdown` waits on the main thread
for a piece being read to finish: as the app goes, and before a damaged model is downloaded again.

**The crash handler's stacks.** A stack overflow leaves no stack to run a handler on, so RDE's handler runs on a stack
of its own (`SA_ONSTACK`): the installing thread's is made by `rde_crash_install`, and every `rde_thread` gets 64 KiB
as it starts (`_rde_crash_thread_begin` in RDE's thread wrappers). A thread RDE did not start (GCD's queues, Java's,
SDL's audio, whisper's) has none: a crash there is still reported, but an overflow there is not. On Android a Java
exception nothing catches is written into the same report by `FudeAndroid.crashInstall`'s handler, on the thread that
threw. The report's header is made at start (`fude_crash_install`), because the handler can only write.

### The rules a change must keep

1. **Nothing the main thread owns is touched from another thread**: the scene, the ink, the notes, the UI, the theme,
   the strings, the session's files, textures and anything else on the GPU, `rde_crash_note`. A thread is handed a job
   of its own, filled before it starts, and reports back through one flag under a mutex (or the main queue, or a Java
   field behind a lock or volatile), which the main thread looks at once a frame.
2. **A PDF is drawn by one thread at a time.** Start a piece only when the worker is not busy, and only through
   `fude_doc_worker_start`. What the main thread needs of a PDF is read as it opens. `fude_pdf_render` is the one
   `pdf.h` call a worker may make, and `fude_pdf_close` the one more, for an orphaned piece's PDF.
3. **A PDF is closed only through `fude_doc_worker_drop`.** When it returns true the PDF is the worker's now: forget it,
   never close it. Stop its search first (`fude_pdf_find_stop`).
4. **A result no longer wanted is dropped by a flag or a number** (stale, orphaned, a generation, a search's number,
   `reader_drain`), never freed under the thread still using it.
5. **Turn a page only while the worker is not busy**, and start no piece while a turn waits.
6. **Hand a buffer to another thread or library by copy**, or make its owner wait until it is done with it: a piece is
   freed the moment `picture_read` returns, and `textscan` copies it first.
7. **Memory a thread may free comes from an allocator safe on any thread**: `rde_calloc` (the engine's pool, made
   thread-safe) or the std allocator. Anything as big as a page (a piece's pixels, a tile's memory texture and its
   copy) is on the std allocator, never the engine's fixed pool, which a few pages would fill.
8. **Nothing on the main thread waits for a thread**, but for `fude_listen_shutdown`, the video encoder, and a PDF
   written out (`fude_doc_export`, `fude_zoom_pdfview_export`), which waits for the one piece being drawn
   (`fude_doc_worker_wait`) because it draws the PDF's pages on the main thread. Android's PDFBox calls that take
   `LOCK` on the main thread (`pageHasText`, `textIn`, and the writer's) can wait for the one page a search is reading;
   keep new ones off that lock, as `findStart`, `findStop`, `findMatches` and `close` are. PDFBox's document is opened
   and closed on "FudePdfFind" (`Text.open`, `Text.close`), and readying PDFBox takes a lock of its own (`READYING`),
   so neither the main thread nor a PDF worker closing a PDF waits on `LOCK` for them; ask a page's text only once
   `fude_pdf_text_prepare` says the opening is done, or the question waits for it.
9. **The main thread must not stall for a second while recording**: the microphone's ring holds one second, and what
   overflows it is lost. What is lost is written as silence (`voice.c`), so the recording keeps its time, and the
   page tells the person (`fude_zoom_voice_record_pump` returns how much; `FUDE_TEXT_ZOOM_AUDIO_LOST`).
10. **A native thread of the app's is an `rde_thread`** (`rde_thread_run`, `rde_thread_run_detached`), so a stack
    overflow on it is reported. **JNI on a native thread** deletes each local reference at once, because a native
    thread's are never freed on their own; `jmethodID`s may be kept.
11. **The header says it.** A module that starts or is called from a thread says which, and what it shares, in its
    overview's Threads line, and `docs/architecture.md`'s Threads table has a row for it.

---

## How to change common things

**A new kind of piece** (a thumbnail, say). Add it to `FUDE_DOC_PIECE_`; decide in `fude_doc_worker_work` whether it is
flipped and darkened; decide in `fude_doc_tile_take` whether it is mipmapped; handle it in each caller's take
(`fude_doc_take`, `fude_zoom_pdfview_take`) and give it its turn in their `_next`. It still waits its turn: one piece at
a time. Add a case to `tests/docwork`.

**A new drawing call for the platform writer** (`fude_pdf_write_*`). Positions are page points from the top-left, on
the page as read. Write it three times: `pdf.c` (Core Graphics, Y flipped by the page's height), `pdf_android.c` with
`FudePdf.java` and `FudePdfBox.java` (under `LOCK`), and the stubs at the bottom of `pdf.c`. No suite builds these:
check it with `--doc-export` on the Mac and by sharing on a tablet.

**A new background job.** Copy `listen.c`'s shape: the job's state in the module, a mutex and a condition variable made
on the main thread before the thread starts, three flags (waiting, running, done) under the mutex, the job's inputs
written only while all three are clear, a take that clears done. Start it with `rde_thread_run`. Give a reading that can
be overtaken a number, and drop answers whose number is old. Write the Threads line and the table's row (rule 11).

**A new export format.** A sink of `fude_zoom_export_walk` in `export_formats.c` (callbacks left NULL for what it does
not want), its number and MIME type in `fude_zoom_page_export_write`, its choice in `FUDE_ZOOM_EXPORT_CHOICES` and
`fude_zoom_export_choose`'s `_formats`, and a test in the zoom suite.

---

## Tests

| Suite | What it checks |
|---|---|
| `tests/docwork` | the worker alone, with a stand-in PDF whose render waits on a condition variable until the test lets it go on, so every state is seen: a piece drawn and taken, flipped and darkened (a reader's picture left as drawn), stale after a colour change or a page turned (forgotten), waited for (as a PDF written out waits), orphaned (its thread frees it and closes the PDF), failed, and the tiles on the std allocator (a GPU stand-in); then the whole pages' plan (`fude_doc_wholes`): a page's size on screen, many small pages drawn once with the worker resting, hundreds zoomed out each drawn small, the budget kept |
| `tests/pdf` | `zoom/pdf.c`: every file read back as a reader would (the xref's offsets, each stream's `/Length`), alpha states, JPEG passed through, RGBA with its soft mask, text in Helvetica and in embedded fonts (glyph ids, widths, ToUnicode); it leaves `out.pdf` beside the suite to open by hand |
| `tests/sheet` | practice sheets for 1, 3, 7 and over 200 characters, in Japanese and Spanish, each checked as a PDF |
| `tests/crash` | a report found and forgotten, the session's marker, a real crash in a child process writing its report and still dying, a death with no report, Android's reasons, the email |

No suite builds `doc.c`, `pdfview.c`, the platform PDF code (`pdf.c`, `pdf_kit.m`, `pdf_android.c`, the Java),
`listen.c`'s thread, `fetch` or ML Kit, and `tests/crash` does not crash on a worker thread. Check those with the look
harness on the Mac: a study app's `--doc=PDF`, `--book=N`, `--doc-view`, `--doc-search`, `--doc-page`, `--doc-turn`,
`--doc-export`, `--theme`, `--read-demo`; a canvas app's `--pdf=PATH@N`, `--loop-at=X1,Y1,X2,Y2@N` and
`--export=pdf:PATH`; then on a tablet.
