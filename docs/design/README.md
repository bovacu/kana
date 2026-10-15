# Design notes

How the deep-zoom canvas and its simulations are built now, and why: one note for each big part, read top to bottom
once. Each says what the part is for, who owns what, the life of its main objects, its threads, its caches, the
invariants a change must keep, how to change common things, and where its tests are. The map of the whole code is
`docs/architecture.md`, and each module's own header has its details.

| Note | Is |
|---|---|
| [page_driver.md](page_driver.md) | the deep-zoom page (`fude/zoom/page.c` and its `page_*.c` modules): a frame's input, update and drawing, tools and topics, a canvas opened and closed, undo, saving, and the shell and the apps above it |
| [rendering.md](rendering.md) | the renderer (`render.c` and its modules): a draw through frames inside frames, far things cut on the CPU, and every cache with its key, bound and invalidation |
| [scene_and_index.md](scene_and_index.md) | the canvas in memory (`scene.c`), its frames, objects, payloads, ids and orders, the R-tree's life, the camera's frames, the history, loading, and the file written as a log |
| [circuits.md](circuits.md) | the circuit simulator: the analog solver (`circuit.c` and its modules), logic on `fude/sim`'s digital engine, parts' ratings, displays, and Play's loop |
| [mechanisms.md](mechanisms.md) | mechanisms: the parts and their plan (`mech.c`), the world run on RDE's 2D physics (`mechrun.c`), drawings made into bodies, and a mechanism played with a circuit |
| [pdf_and_threads.md](pdf_and_threads.md) | PDFs coming in (shown under the ink, read, searched) and going out (the writers, the exports), and every thread the apps run, with its rules |

`docs/infinite_canvas_design.md` is the history: dated sections, each decision as it was made, the numbers measured
then, and what was planned and built differently. Read it to learn why something was chosen, and what was tried
before. Its sections are not revised when the code changes. These notes are the design as it is now, checked against
the code; where the history and a note disagree, the note (and the code) is right, and a note says where the plan went
another way.
