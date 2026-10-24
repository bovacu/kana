# The scene, the index and the file

The scene is a canvas in memory: its frames, the objects in them, the camera, the undo history and the journal of
changes (`fude/zoom/scene.c`, `scene_history.c`, `scene_records.c`; `scene.h` is what the rest sees, `scene_parts.h`
what its modules share). Each frame has a spatial index, an R-tree over its objects' boxes (`index.c`). The canvas's
file (`zfile.c`) keeps the scene on disk as a log. Almost every zoom module works on a scene: the page owns the open
canvas's (`page_driver.md`), the renderer draws it (`rendering.md`), the tools change it.

The design was made in `infinite_canvas_design.md` §2 to §8 (2026-10-04); this note is the design as the code has it
now, with where it went another way.

---

## The main types

| Type | Is | Lives in |
|---|---|---|
| `fude_zoom_scene` | the canvas: the tables below, the camera, the history, the journal, the layers' state | the page (`fude_zoom_page.scene`); Find makes short-lived ones to read other canvases |
| `fude_zoom_frame` | a frame: its place in its parent, its objects in three orders, its index, its buckets' places in the file | `scene.frames`, by slot |
| `fude_zoom_object` | one thing: a stroke, a frame's entry in its parent, a shape, a picture, a fill, a bookmark, a text, a layer's record | `scene.objects`, by index |
| `fude_zoom_blob` | an object's payload: its bytes, shared by copies | `scene.blobs`, by handle (slot + 1) |
| `fude_zoom_action` | one undo step | `scene.actions`, naming objects through `scene.targets` and `scene.places` |
| `fude_zoom_camera` | a frame, a point in it, a zoom | `scene.camera` |
| `fude_zoom_index` | a frame's R-tree | `fude_zoom_frame.index` |
| `fude_zoom_file` | where the canvas's file stands: its path, its size, its live bytes | the page (`fude_zoom_page.file`) |

`fude_zoom_scene_init` makes an empty canvas (one root frame, also home, the camera on it at zoom 1);
`fude_zoom_scene_destroy` frees it all. **Nothing is ever taken out of the tables while a scene lives**: an object's
index and a frame's slot never change, so the rest of the app holds them as plain numbers. A pointer from
`fude_zoom_scene_object` or `fude_zoom_scene_frame` is good only until the next object or frame is added (the tables
may move). Everything is on the std allocator: a canvas has no natural size limit, and the engine's pool keeps what it
grows by until the app ends. Main thread only; nothing here locks.

## Frames

Frames are the precision mechanism, and the user never sees them. Each frame has its own units, and a transform into
its parent (`fude_zoom_xform`: an origin, a scale, a rotation). A child's unit is 1/1024 of its parent's
(`FUDE_ZOOM_CHILD_SCALE`). Inside a frame an object's translation is an f64 and its points are small integers from it
(quanta of 2^q units, q chosen at the zoom it was drawn at, about 1/16 of a point: `fude_zoom_quantum_for`). So no
stored number is ever huge or tiny, at any depth, and f64 keeps sideways panning unlimited for any real use: frames
exist only for depth, never for tiling.

**Frames go both ways.** In a frame the camera's zoom (points per unit) stays between `FUDE_ZOOM_Z_MIN` (1/64) and
`FUDE_ZOOM_Z_MAX` (64). Past 64 it goes into a child, where its zoom is 1/16; under 1/64 it goes up to the parent,
where it is 16. The gap is the hysteresis: zooming back and forth at a threshold does not flip frames. Zooming out past
the root makes a new root above it (`fude_zoom_scene_new_root`), the old root becoming its child at its origin, so
neither way has an end. The canvas's first root stays its **home** (`scene.home`): its zoom 1 is ×1, and Reset view
goes there.

**A frame is an object of its parent.** Each frame but a root has a FRAME object in its parent, so it has a place in
its parent's order like a stroke: what was drawn later is on top, at any depth. Its box is the frame's **anchor**: in
parent units, what it holds and the view it was made for. `fude_zoom_scene_frame_reach` grows the anchor (and its
parents') as content is drawn past it; anchors only grow.

**A frame is kept only if something is in it.** A frame made for the camera is journaled only once something is
drawn in it (`saved`), so zooming about leaves no trace in the file; one left empty is dropped as the camera leaves
(`removed`). `fude_zoom_scene_keep_frame` makes the file keep one that something names though nothing is drawn in it:
the camera's, as the canvas is left, and those the instruments lie in.

## Objects

An object is one record, `fude_zoom_object`: its id, its gesture's first id, its frame, its kind, flags, layer, order
key, translation, rotation and scale, its box in frame units, colours, base half-width, quantum, and its payload's
handle. What each kind's payload holds is in `scene.h`'s `FUDE_ZOOM_KIND_`. A stroke longer than
`FUDE_ZOOM_STROKE_MAX` (512) points is cut into pieces of one gesture (`FUDE_ZOOM_FLAG_CONTINUES`), which keeps their
boxes tight and their saves small.

**Nothing is deleted by an edit** (Kana's rule). Erasing marks an object dead; undo marks it alive again. An object's
life:

```
alive ──erase──▶ dead ──(no undo step names it any more)──▶ garbage
  ▲               │                                          its payload let go, its record kept with no points
  └────undo───────┘                                          until the file is compacted; never alive again
```

`history` counts the undo steps that name an object. `fude_zoom_scene_collect` makes a dead object garbage once that
count is 0: when the steps naming it are dropped (a new edit drops what was undone; the budget drops the oldest), and
for everything at load. Frames are never collected. `fude_zoom_scene_discard` is for a piece made and unmade inside
one gesture, which no step will ever name.

**Payloads and their serials.** A payload (`fude_zoom_blob`) is the bytes of an object's points, numbers, picture or
words, with a count of the objects sharing it: a copy shares its original's (`fude_zoom_scene_copy_stroke`). A
payload's bytes never change after it is made; an edit makes a new object with a new payload. Freed slots are given
out again (`free_blobs`, the last freed first), so each payload also has a **serial**, `++scene.blob_serials`, and
what is kept for a payload elsewhere (the renderer's pictures and geometry) is matched by handle and serial together,
never by handle alone.

**Ids.** An id is `(device << 32) | counter` (`fude_zoom_scene_new_id`), never 0. The device number is made new at each
launch (`fude_zoom_page_init`: the time, the page's address and a constant), and a file's ids move this device's
counter past them as they are read (`fude_zoom_scene_saw_id`), so ids from other devices and earlier launches never
repeat. (The history log planned one made at install; a number per launch needs nothing kept.) Ids are unique within
a canvas only: each scene's counter starts again, so two canvases made in one launch give out the same ids. The file
and the journal name objects and frames by id; `object_ids` and `frame_ids` map them to slots. Where an id is kept in
a shape's numbers (a wire's ends, a constraint's lines, a dimension's), it is kept **in two halves**
(`fude_zoom_id_put`, `fude_zoom_id_get`), each a whole number an f64 holds exactly: one f64 would lose the id's last
bits once the device half is large. A half a u32 cannot hold (negative, too big, not a number: a damaged file's) reads
as no id, since casting it would be undefined.

## Orders: z and arrival

A frame lists its objects three ways:

- **`order`**, by order key (`z`): what is drawn over what. Keys are spaced `FUDE_ZOOM_Z_STEP` (2³²) apart as objects
  come on top (`z_next`), so what is made from an object can go right after it: the pieces an eraser cuts from a
  stroke take its key plus 1, 2, ..., keeping its place in the order; a copy moved to another layer takes the
  original's key, the original let go in the same step. A new object goes in by a binary search
  (`fude_zoom_order_insert`). The draw key adds the layer's rank above the key (`fude_zoom_scene_draw_key`); a
  FRAME object takes layer 0's rank (Open points, below).
- **`arrival`**, as they came: an object's `file_pos` is its place here, and `file_pos / FUDE_ZOOM_BUCKET` (512) is
  its bucket in the file. A change to an object marks its bucket dirty (`fude_zoom_scene_touch`), and the next
  checkpoint writes the dirty buckets alone. Buckets by arrival keep an old bucket unchanged while new things are drawn.
- **`kids`**: its FRAME objects, as they came. The camera walks these, not the order (below).

## The index: an R-tree for each frame

Drawing, erasing and picking look only at what is near, through the frame's `fude_zoom_index`, an R-tree over its
objects' boxes in the frame's own units. Its nodes are one `rde_arr`, node 0 the root, each holding up to
`FUDE_ZOOM_INDEX_FAN` (16) entries. An insert goes down the path whose box grows least; a full node splits along its
longer side at the middle of its entries. `fude_zoom_index_build` packs a whole tree at once (Sort-Tile-Recursive),
which is also the fastest tree to query.

**Its life** (the rules since 4.4):

- **Each object goes in once, with its box.** An object whose box comes from its payload (a shape's outline, a
  picture's or a text's corners) is placed first, boxed (`fude_zoom_scene_rebox`), then put in
  (`fude_zoom_scene_put_in` with `_boxed` false, then the index add). They were once put in at {0,0,0,0} and again,
  which left an entry behind for every one.
- **Nothing is taken out.** Objects are never removed (undo needs them), so neither are their entries. A dead object's
  entry stays, and queries skip it. A moved object goes in again at its new place, and its old entry is **stale**
  (`frame.stale`). A garbage object's entry is **dead** (`frame.dead`).
- **Built again when the waste piles up.** Each time an object moves or becomes garbage, `fude_zoom_scene_reindex`
  checks its frame: once dead and stale entries are more than 64 and more than half the frame's arrival order, the
  index is built again, packed, from the arrival order without garbage. A rebuild costs the whole frame, so (the
  evident reason) it waits until the waste is that large a share, and small frames are left alone.
- **Queries.** `fude_zoom_scene_query` keeps what is alive and whose own box (not the entry's) meets the box asked
  for, each once. Only a move leaves an object in the index twice, so the results are sorted and deduplicated only
  while the frame has stale entries; queries were once sorted after every erase.
- **FRAME objects are in no index.** They are few and their anchors grow; whoever needs them walks `kids`.

**Nearest first.** `fude_zoom_index_nearest` visits values nearest a point first: a heap of nodes and entries by
squared distance, each node opened counted against a budget. The empty screen's marks use it (`nav.c`: an arrow to
the nearest thing off the screen in each eighth of a turn, a ring for what is too small to see), with a budget of
4,096 nodes; the visitor is handed each value's entry box and its leaf's box (what a tap on the arrow shows: it and
its neighbours). It reads the tree as it is, stale entries and all, so the visitor (`nav.c`'s `fude_zoom_nav_visit`)
skips what is not alive, passes over an entry whose box is not the object's own (a stale one, where a moved object
was), and visits each object once: one moved away and back has two entries with its own box, so while the frame has
stale entries the search keeps a set of the objects visited (on the std allocator, freed with the search). Otherwise
an arrow could lead to where something used to be, or count one thing twice.

## The camera and its frames

`fude_zoom_camera_settle` keeps the camera in range after every pan and zoom (the page calls it each frame when no
gesture holds the camera), at most 256 frames at a time:

- **Into a child** past the top of the range: `fude_zoom_camera_child_at` takes the child whose anchor holds the
  camera's point, the most recently visited first (`visited`, from `scene.clock`), and of those visited alike (every
  one, in a canvas just opened) the lowest key, then the first listed, which is what the order gives. It walks the
  frame's `kids`, not all its objects (4.4), because the camera settles every frame and looks here each time it is
  panned outside its own frame's anchor. None holds it: a new frame is made there, its origin on the paper's lattice
  (`fude_zoom_paper_snap`, so the paper runs on unbroken), its anchor the view.
- **Up to the parent** past the bottom: an empty canvas stops at its root (there is nothing out there to see);
  otherwise a new root is made when needed. The frame left is dropped when it was made for the camera and nothing is
  in it.
- **Across**: panned out of its frame's anchor onto a sibling's, the camera is handed to the sibling, so new strokes
  go where their neighbours are, which keeps files and selections local.

**Later is on top.** `fude_zoom_camera_drawing_frame`, as a stroke begins, looks three levels up (the levels the
renderer draws exactly) for anything alive drawn later than the camera's frame over where the stroke starts
(`fude_zoom_scene_covered`). If something covers it, a new frame is made on top at that level, with one inside it for
each level below, down to the camera's depth, and the camera moves into it: otherwise the new stroke would go under
the later paint. "Drawn later" is by draw key, as the renderer sorts. What a hidden layer holds is drawn nowhere, so it
does not count. Of the rest, only what is on a layer of the frame's rank (layer 0's: below) and later in the order
counts, since a new frame on top would get that rank and the next order key: what a layer ranked above holds is drawn
over any frame there, a new one too, and what one ranked below holds is drawn under this one.

The camera is not journaled. Only a checkpoint keeps it (the file's index), which is why the page checkpoints when a
canvas is left after only looking round.

## The history

Kana's undo log, generalised (`scene_history.c`). `actions[0 .. action_count)` are done; the ones after were undone
and can be redone until a new step drops them. One gesture is one step: a stroke and all its pieces, an eraser
sweep, a move. A step names what it made (`BORN`), erased (`DIED`), both (`REPLACED`) or moved (`MOVED`, with each
object's place before and after), through `targets` and `places`, which only grow at the end. The caller changes the
scene first, then pushes the step (`fude_zoom_history_push` and its kin); undo makes what the step made dead, what it
erased alive, and puts what it moved back, and redo does the opposite. Neither deletes anything.

- **Bounded by bytes**, not steps: each step counts the payloads and records it keeps alive, and past
  `FUDE_ZOOM_HISTORY_BUDGET` (48 MB) the oldest steps go, never the one just done. What only they named becomes
  garbage.
- **Kept in the file**: pushes, undos and redos (as the new `action_count`) and drops are journal ops, and the history
  is a chunk of every checkpoint, so a canvas reopens with its undo.
- **A step knows its frame and box**, so the page can fly to it, and can carry the page's own `tag`.
- `history_pushes` counts the steps pushed since the scene was made (those read from the file too), and
  `history_dropped` the steps dropped from the oldest end: a step's place among all steps ever is `history_dropped`
  plus its index (the instruments' undo lines up with it).

**Clear** (`fude_zoom_scene_clear`: the bar's bin with nothing lassoed) is an edit like any other: it makes dead
everything alive, at every depth, that a tool may touch, and lists their indices, in index order, for the caller's one
step. It leaves the frames, the bookmarks and the layers' records (a canvas's places and its layers' names, eyes,
locks and order are not what is drawn), and whatever a hidden or locked layer holds (`fude_zoom_scene_touchable`: a
locked layer's things are not to be changed, and a hidden one's would go unseen).

## The journal and the records

Every change that does not come from the file is written, as it happens, into `scene.journal`: `u8` op, `u32` size,
body (`FUDE_ZOOM_OP_`: a frame's record, an object's, alive or dead, an action pushed, the action count, steps dropped,
a new root, a place). It is appended to the file about every second. Changes replayed from the file are not journaled
(`replaying`). Every change worth saving also moves `scene.revision`, which the session watches.

The records are written by `fude_zoom_put_frame`, `_put_object` and `_put_action` and read by their `fude_zoom_get_*`
(`scene_records.c`, whose comment has their layout). New fields go at the end of a record, and readers take a record
by its size, so an older build reads a newer file and the reverse. An op of a kind a build does not know is skipped
whole. An object read whose id is there already is skipped (the first stands); one read with no points is garbage.

## Loading

`fude_zoom_file_open` reads into a fresh scene. While it reads, the scene's `loading` is set: objects are appended to
their frames' orders as they come, unsorted, and put into no index; the journal's ops find objects and frames by id,
so nothing looks them up on the way. Then `fude_zoom_scene_loaded`:

1. collects everything no undo step can bring back;
2. settles each frame's order (`fude_zoom_order_settle`): most come in their keys' order already (objects arrive as
   they were made) and are left as they are; the rest (pieces an eraser cut, copies on another layer, FRAME objects,
   which are read before the objects round them) are sorted by key, then by index, which is the order putting each in
   one at a time gives, since their indices are the order they came in;
3. builds each frame's index once, packed;
4. clears `loading`, empties the journal, and makes the root home for a file from before frames said which is home.

A read that fails clears `loading` too, so a scene kept by mistake would not go on skipping its index. Chunks are
found by a binary search on their offsets (`fude_zoom_file_find`): the index names one for every bucket of every
frame, among thousands, and the chunks were found walking the file forward, so they are in offset order. The CRC is
worked eight bytes a step. Together (4.8): a canvas of 48,000 objects in 3,001 frames, 134 to 68 ms; the gate's canvas
opens in 3.3 to 3.5 ms, from 9.2. A canvas re-saved is byte for byte what it was.

`fude_zoom_file_peek` reads another canvas without ever writing (Find, through `<id>.words`, when its words are not
kept or the file grew since).

## The file, written as a log

`<id>.zoom` uses the KANA chunk format (`kfile.h`): the header ("KANA", version 1, kind 'ZOOM'), then chunks of `u32`
tag, `u32` size, payload, little-endian, unknown tags skipped. Two things are its own: every payload ends in a CRC-32
of the rest of it, so a chunk half written when the app was killed is known as such; and **nothing in it is ever
overwritten**. Changes are appended, and now and then the whole file is written again, compacted. `zfile.h` has every
chunk's layout.

| Chunk | Holds |
|---|---|
| `JRNL` | the journal's ops since the last checkpoint |
| `BKTS` | one bucket of a frame: up to 512 object records, in arrival order |
| `FRAM` | every kept frame's record, parents first |
| `HIST` | the action count and every action |
| `INDX` | where the FRAM and HIST are, the camera, the root, and each frame's buckets' places and sizes |
| `FOOT` | the INDX's place and 'ZEND': always the last chunk of a checkpoint |

**Writing** (the page decides when: `page_driver.md`):

- **A flush** (`fude_zoom_file_flush`) appends the journal as one JRNL and syncs it, about every second while there is
  anything new. Then a checkpoint if 256 KB of journal have been appended since the last
  (`FUDE_ZOOM_FILE_CHECKPOINT_BYTES`), then a compaction if due. With no file yet, the whole file is written the first
  time there is anything in it.
- **A checkpoint** (`fude_zoom_file_checkpoint`) appends, in one write, the dirty buckets (and new ones), FRAM, HIST,
  INDX and FOOT, then syncs. Only once that write succeeded do the frames learn where their buckets went and the
  journal empty. The page makes one as a canvas is left.
- **A compaction** (`fude_zoom_file_compact`) writes everything alive or undoable, garbage left out of the buckets, to
  `<id>.zoom.tmp`, syncs it, moves the old file to `.bak` and the new one into place (kfile.h's rule; the `.bak` put
  back if the move fails). It is due when the bytes no index points at (old copies, old journals) are more than the
  live ones and more than 64 KB. It is also how a torn end is cut off, and how a file is rewritten after a failure.
- **A failed write** (a full disk) may have left some bytes in the file that cannot be taken off again. The file's
  size is then set to 0, so the next save writes the whole file afresh rather than appending behind a fragment that
  opening could not read past; what failed is still in memory (the journal kept, the buckets still dirty).

**Opening** (`fude_zoom_file_open`): the header; then every whole chunk found by its header alone, noting a torn end;
the last good FOOT whose INDX is good; the frames, the buckets and the history it points at; the camera (the root at
zoom 1 if its frame is gone); then every good JRNL after that footer, replayed in order. A chunk after the footer that
is not a JRNL (a checkpoint a kill cut short) is passed over; a bad JRNL ends the replay. An action over something the
file does not have ends the history there, and the canvas still opens. A torn or passed-over end is compacted away at
once (`repaired`).

**Recovery.** A file that does not parse at all is set aside as `.bad` (`fude_file_set_aside`) and never overwritten,
so a reader's bug can never wipe a canvas by saving an empty one over it; the `.bak` is read instead and written as the
new file (`FUDE_LOAD_RECOVERED`). The `.bak` is also read when the main file is missing: a kill between a compaction's
two moves leaves only the `.bak`, and without reading it the canvas would open empty and the next compaction would
write over the backup. Neither parsing: the canvas opens empty (`FUDE_LOAD_CORRUPT`), the damaged main file kept
aside.

**What `kill -9` can lose, and what it cannot.**

- It cannot lose anything in a JRNL or checkpoint that was synced before the kill: every write is synced before it
  counts, and opening reads everything whole up to the first damaged chunk.
- It loses what had not been appended yet: the finished gestures of the last second or so, and the one in progress.
- It loses where the camera was since the last checkpoint (the camera is not journaled), and the instruments' file if
  they moved in the last second.
- A kill during a compaction loses nothing that was on disk: before the moves, the old file is whole; between them,
  the `.bak` is read.
- The kill test (`--stress`, `COMMANDS.txt`) found, over 200 rounds (`infinite_canvas_design.md` §13), the canvas
  never damaged, and, drawing strokes only, never with fewer strokes than when it was last opened.

**Where the plan went another way.** Every frame is read as a canvas opens, and none is ever unloaded: the INDX would
allow loading frame by frame, but records and packed points are small (about 1.6 bytes a point) and opening is a few
milliseconds. There are no impostor textures (`THMB`): frames are drawn directly, skipped under half a pixel. The
camera is kept by checkpoints, not by the journal. And the plan's compaction before a file leaves the app is not done:
Your data (`backup.h`) copies the file as it is, log and all, which opens as a compacted one does (`zfile.h` says so).

## Invariants

- Object indices and frame slots never change while a scene lives; nothing is removed from the tables.
- Nothing is deleted by an edit. A gesture changes the scene, then pushes one step; the step names everything it
  made, erased or moved.
- A payload's bytes never change after it is made.
- Every change not replayed from the file is journaled and moves `revision`.
- Each object is put into its frame's index exactly once, with its box, and again at each move (its old entry then
  stale). Whoever puts one in with `_boxed` false puts it into the index once its box is known.
- While `loading`, no index is touched and orders are appended; `fude_zoom_scene_loaded` (or a failed read) ends it.
- Frames are journaled parents first, and a frame only once something is in it or something names it.
- Records only grow at their end; new op kinds go at the end of `FUDE_ZOOM_OP_`; old files must still read.

## How to change common things

**A new kind of object.** A value at the end of `FUDE_ZOOM_KIND_`, its payload's layout in `scene.h`, an adder that
places it, boxes it, puts it in once and journals it (as `fude_zoom_scene_add_payload` does), `fude_zoom_scene_rebox`
for its box, a case in `fude_zoom_get_object`'s list of kinds this build knows, and its drawing (`rendering.md`).

**A new field in a record.** At the end, written by `fude_zoom_put_*`, read in `fude_zoom_get_*` only where the reader
has the bytes (`fude_reader_has`), with the value an older file means when it does not.

**A new journal op.** A value at the end of `FUDE_ZOOM_OP_`, written where the change happens (not while
`replaying`), applied in `fude_zoom_apply_ops`. An older build skips it, so it must not be something an older build
needs to open the canvas right.

## Open points

- **A child frame sorts at layer 0's rank: an open decision for the owner.** The draw key puts the layer's rank above
  the order key, but a FRAME object's `layer` field is 0, so a frame and all it holds are drawn at layer 0's place
  among its parent's things, whatever layers they are on (`scene.h` documents it above `fude_zoom_scene_draw_key`).
  The cost: something drawn deep on a layer ranked above layer 0 is drawn under its parent's things on such a layer
  where they cover it, even those drawn before it. The two ways out: draw each layer through every depth (the
  renderer, `export.c`'s walk and `pdfview.c`'s export each going down once a layer), or give each frame the layer it
  was first drawn on. `fude_zoom_scene_covered` and the renderer's backdrop compare draw keys, so they follow whichever
  is chosen (`rendering.md` has the drawing side).

## Where the tests are

`tests/zoom` (`bash tests/run.sh zoom`): `test_codec`, `test_sim_and_index` (the index against brute force,
incremental and packed), `test_frames` (20 levels in and back out, every stroke where it was drawn),
`test_history`, `test_clear` (what Clear takes and leaves, as one step; a second takes nothing; undone, all back),
`test_cover_layers` (later on top and the backdrop with layers: a hidden layer's things count for neither, and a
layer ranked above a frame's makes no new frame), `test_moves`, `test_index_clean` (adds, copies, erases and moves at
random: each object in once, waste never past the rebuild's point, every query as brute force's), `test_child_at`,
`test_navigation` (nearest first against brute force, each object visited once), `test_file` (reopened point for
point; cut at every byte, it opens to the last state the cut keeps; a flipped byte; not a canvas; only the backup
there; compaction), `test_file_failed_write`, `test_crc`,
`test_load_whole` (a loaded canvas's orders, index and bytes the same as the saved one's), `test_load_big` (timed),
`test_history_tags`, `test_dim_ids` and `test_radial_ids` (ids in halves).
