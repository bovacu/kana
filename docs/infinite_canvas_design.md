# Infinite canvas: Phase 0 design (the core)

Proposal, 2026-10-04, for review before any code. It builds what [research/infinite_canvas_research.md](research/infinite_canvas_research.md) puts first: the frame system, the object model, the stroke codec, undo, and the file. Every tool after this hangs off them.

Decisions already taken (research §6):
- a new app;
- Kana's chunk format extended, not SQLite;
- Clipper2, from Phase 2;
- all three erasers, with partial as the default;
- phases in the agreed order.

## What Phase 0 delivers

- **The canvas:**
  - depth frames, the camera that moves between them, and drawing across levels;
  - zoom in and out without limit, both ways.
- **The model:**
  - the object table, the stroke codec, the R-tree per frame;
  - memory budgets in bytes.
- **Editing:**
  - the pen with zoom-relative width;
  - the three erasers;
  - undo and redo, saved with the document.
- **The file:**
  - Kana's chunk format made log-structured;
  - a journal every second, crash recovery, lazy loading, compaction.
- **The app's shell:** on fude/drawing, so it has the side panel, the canvases and folders, Settings, FAQ & Contact and About from day one.
- **A gate** that must pass before Phase 1 starts (§10).

Not in Phase 0: shapes, lasso, images, text, layers UI, sections (see the terms below). The model leaves room for all of them.

**Terms used below:**
- **frame**: a depth frame, a precision mechanism the user never sees.
- **section**: a frame the user does see (a diagram container, a slide). Sections are a Phase 3 object and are unrelated to depth frames.
- **unit**: a frame's local unit.
- **pt**: a screen point.
- **z**: zoom, in pt per unit.

---

## 1. Where the code goes

| Where | What |
|---|---|
| `fude/zoom/` | A new layer over `fude/drawing`, the way `fude/study` is. It holds the deep-zoom canvas: `frame` (frames and their transforms), `camera`, `object` (the table), `codec` (stroke channels), `index` (R-tree), `stroke` (capture into records), `erase`, `history` (undo), `zfile` (the file), `render`, `page` (pen, fingers and tools on this canvas). Models know nothing of the screen and are tested alone, as in the other layers. |
| `apps/<name>/` | The new app: its shell (like `draw.c`), `src/` (its info, version, text_ids.h), `assets/`, `platform/`, `tools/strings.py`. |
| `third_party/clipper2/` | From Phase 2 (offset/kerf) and Phase 4 (fill). Phase 0 doesn't need it. RDE's builder already compiles `.cpp` files with clang++ on every platform, as it does for meshoptimizer and imgui. |

**Changes to `fude/drawing`.** Every existing app must stay identical: same behavior, byte-identical LTR shots.

1. **The page seam.** Today the shell calls the ink page directly in session, page, toolbar, page menu and UI. Add a **page kind** to the extension: a table of functions the app supplies, with NULL meaning today's ink page:
   - event, update, render, let go;
   - undo and redo with their step counts (the toolbar's buttons);
   - clear;
   - a canvas's new, load and save, its file name, and a revision for autosave.

   Session, toolbar, page menu and UI go through it.
2. **The toolbar's tools.** Today the toolbar knows `FUDE_TOOL_` (draw, erase, lasso, mark). The page kind gives it a table of tools instead: icon, label, and a mode menu on long-press (the eraser's three modes). The new app keeps the toolbar's look, grip, docking and RTL for free. The ink page's table is today's four tools.
3. **The pen sampler.** Move the live pen state out of `ink.c` into `drawing/ink/pen.h`:
   - pressure smoothing, and simulated pressure for pens without it;
   - tilt, event time, and the Hz and latency measurement.

   `fude_ink` and `fude/zoom` both use it. Kana behaves exactly as before.

**Change to RDE:** `rde_file_sync(rde_file*)`. It uses `fsync`, with `F_FULLFSYNC` on Apple and `FlushFileBuffers` on Windows. The engine has no way to force data to disk today, and the journal needs one. Nothing else changes: `rde_file_read_chunk`, append mode and `rde_file_get_size` already cover lazy reads and appends.

---

## 2. Depth frames

### 2.1 What a frame is

The agreed design was nested frames, so no stored number is ever huge or tiny. Phase 0 settles the details.

- **A frame** has an id, a parent, and a **transform into its parent**:
  - an origin (f64, in parent units);
  - a scale (f64, nominally 1/1024, so one unit is 1/1024 of a parent unit);
  - a rotation (f32, 0 unless the user rotated content that contains the frame, §2.6).
- **Positions inside a frame are f64 where it matters.** Each object's translation is f64, and its geometry relative to that translation is small (quantized integers, §4).
  - f64 keeps 1/16-pt precision out to about 10¹² units. Panning sideways is therefore unlimited for any real use, with **no lateral tiling**. Frames only exist for depth.
  - This refines the research doc's f32 translation. It costs 8 more bytes per object, which is nothing next to the points.
- **Frames go both ways.** Zooming in past a frame's range enters a child, made if none fits. Zooming out past the root makes a **new root** above it, and the old root becomes its child at origin 0 with scale 1/1024. Zoom-out is as unlimited as zoom-in.
- **A frame is kept only if something is drawn in it.** An empty frame is dropped when the camera leaves it.

### 2.2 The camera

- **The camera is (frame, position, z):** the frame, a position (f64, local) and a zoom.
- **Zoom range:** z stays within [1/64, 64] in its frame.
- **Zooming in:** at z > 64, the camera enters a child, so its z becomes 64/1024 = 1/16.
  - Which child: the one whose **anchor box** contains the view's centre. The anchor box is the child's content bounds plus the view it was made for, in parent units. The most recently visited child wins.
  - No such child: a new one, with its origin at the view centre.
- **Zooming out:** at z < 1/64, the camera goes to the parent, so its z becomes 1024/64 = 16.
  - The gap between 16 and 64 is the hysteresis: zooming back and forth at a threshold doesn't flip frames.
- **Panning hands the camera over.** When the view centre leaves the camera frame's anchor box and lies in a sibling's or cousin's at the same depth, the camera moves to that frame. New strokes then go where their neighbours are, which keeps files and selections local.
- **Bookmarks and "back to where I was" (Phase 1)** store a full camera: frame id, position and z.

All numbers (1024, 64, 1/64) get tuned in the prototype. The rules stay the same.

### 2.3 Drawing across levels

Walk the frame tree from the **highest ancestor that can be drawn exactly** down through every frame whose bounds meet the view:
- Draw each frame's objects through its R-tree.
- Stop descending where a frame's content would be under ½ px on screen.

The camera frame is only the anchor for precision. Siblings and cousins at the same depth draw like anything else.

- **All maths on the CPU in f64, relative to the camera.** The results go to `rde_rendering_2d_draw_stroke` as screen-space f32, as in Kana, so the GPU never sees a big number.
- **How far up exact drawing reaches.** The error of drawing frame A, n levels up, is about |camera position in A| × 2⁻⁵² × z_A, where z_A = z × 1024ⁿ:

  | Levels up | z_A at most | Error with content 10⁴ units across |
  |---|---|---|
  | 1 | 2¹⁶ | 0.0000001 pt |
  | 2 | 2²⁶ | 0.0001 pt |
  | 3 | 2³⁶ | 0.15 pt |
  | 4 | 2⁴⁶ | 150 pt |

  The renderer computes that bound for each frame and draws exactly while it stays under ¼ pt, which is three levels for normal content. **Beyond that, a backdrop:** the colour of whatever ancestor content covers the camera's point, else the paper. That is what zooming into a line looks like anyway: you end up inside its colour.
- **Going down:** children are drawn while visible. At most zooms they are 1/16 to 1/1024 of their natural size, so dense ones draw from a cached **impostor** texture (§8). Grandchildren are under 1/16384 size and never drawn until the camera gets closer.

### 2.4 Later is on top

A child frame is an entry in its parent's draw order, with a z-key like any object. The problem: if the user zooms out, paints a big stroke over a detail's area, then zooms back into that detail and draws, the new strokes would land **under** the paint, because their frame's slot is below it.

The rule: **a frame takes new content only while nothing later in its parent covers the area.**
- When a stroke starts, check the camera frame's slot, and its ancestors' slots up the exactly drawn levels, against the later objects over the view.
- If something later covers it, open a new sibling frame on top and draw there.

Frames are cheap, so "drawn later is on top" holds everywhere, as users expect from ZoomArt.

### 2.5 Editing across levels

- **Tools act at the camera's depth and below.** That covers the camera frame, its siblings and cousins, and their descendants.
- **Shallower frames are a read-only backdrop.** You are inside their strokes, and cutting a parent stroke from deep inside would cut it more finely than it was stored. Zoom out to edit it.
- Erasing, picking and (in Phase 1) lasso test descendant objects through the f64 transforms.

### 2.6 Moving and scaling carry the detail inside

Example: draw a tree, zoom two levels into a leaf, draw a ladybug, zoom out, and move the tree. The ladybug must come along.

So a selection includes descendants:
- A child frame whose anchor box lies **wholly inside** the selection moves, scales or rotates **as one**. Its transform changes in a single undo record, and everything below it comes along for free. This is why frames carry a scale and a rotation, not only an origin.
- A child frame that **straddles** the selection's edge is taken object by object, and its own children by the same rule, recursively.

The lasso is Phase 1, but this rule shapes the frame record now.

---

## 3. Objects

- **A frame holds:**
  - an object table (`rde_arr` of fixed records) and an R-tree over it;
  - its children, as entries in the same table (kind FRAME), so they have a z-key like everything else;
  - a cached impostor.
- **A record, about 64 B:**
  - id (u64) and kind (u8);
  - flags: alive, locked, hidden, and "continues" for pieces of one gesture;
  - layer (u16) and style (u32);
  - z-key (u64);
  - translation (2 × f64), rotation (f32), scale (f32);
  - bounds (f32, rounded outward, for the R-tree);
  - payload handle and size.
- **Ids** are (device id << 32) | counter. The device id is random, made at install. Ids stay unique if files ever merge or sync.
- **The z-key** is a u64, spaced 2³² apart as objects are added. Inserting between two objects takes the midpoint, and in the rare case of no room, the frame's keys are renumbered in one undo record. No string keys are needed.
- **Nothing is deleted by an edit (Kana's rule).**
  - Erasing marks objects dead.
  - A payload is freed only when no history entry can bring it back (§6).
  - Payloads are immutable and shared by handle.
- **Kinds:**
  - in Phase 0: STROKE and FRAME;
  - reserved for later: SHAPE, PATH, TEXT, STICKY, IMAGE, GROUP, CONNECTOR, DIMENSION, FILL;
  - plus binding records (connector ends) in a table of their own, from Phase 3.
- **Layers:** every record carries its layer from day one (layer 0, implicit). The panel comes in Phase 3.

---

## 4. Strokes

- **Capture.**
  - The pen sampler (shared with Kana) gives positions in the camera frame's units (f64).
  - The first point becomes the stroke's translation.
  - Every point is stored relative to it as an integer multiple of a quantum 2^q, where q = floor(log₂(1/(16·z))): about 1/16 pt at the zoom it was drawn at. In the camera's z range, q runs from −10 to +2, so it fits an i8.
- **Width is zoom-relative.** The brush size is set in screen points and turned into units at draw time; this is Kana's SCREEN brush scale, now the default. It is stored as the stroke's base half-width in units, with pressure shaping it per point. Kana's minimum on-screen width stays, for display only.
- **The encoding.** The record holds the translation, q, point count, style, base half-width, and flags (from pen, has tilt, has time). The payload holds separate channels:

  | Channel | Encoding |
  |---|---|
  | x, y | delta of delta, zigzag, adaptive Rice code in blocks of 32 |
  | pressure | 8 bit, or 10 when the pen has real pressure |
  | time | ms deltas, the same coding |
  | tilt | 8 + 8 bit, when present |

  This is about 1.4 B per point for x, y and pressure (the research agent's simulated figure, to be measured on our ink in the gate). The integers are exact, so there is no drift ever.
- **Long strokes are cut at 512 points** into pieces flagged "continues". One gesture is still one undo step, and the whole-stroke eraser takes all its pieces.
- **Raw input is stored as is.** Smoothing and stabilizers (Phase 1) are a stroke property applied when drawing, so they can be changed afterwards. Phase 0 draws the raw points, as Kana does. *Changed in Phase 1 (§14): the smoothed line is what is kept.*
- **Drawing.** Visible strokes decode into a cache: x and y as f32 relative to the translation, plus the half-width. The cache is LRU and budgeted in bytes. Each stroke goes through the f64 camera transform, then `rde_rendering_2d_draw_stroke`.

---

## 5. The erasers

All three modes live behind one eraser button:
- long-press the button to pick the mode;
- **partial is the default**;
- the eraser returns to the previous tool when it lifts (setting, on by default);
- the pen's eraser end and a double tap switch to it.

It acts at the camera's depth and below (§2.5).

1. **Partial.**
   - The eraser's path is a chain of capsules of radius r, given in pt and turned into units.
   - For each stroke it touches: find the stretches of its centerline within r + the stroke's half-width, and merge them.
   - The parts in between survive as new strokes. Each cut point is interpolated (position, pressure, time), and the cut end gets the brush's normal round end, so it looks rubbed out, not chopped.
   - Pieces shorter than 1 px at the eraser's zoom are dropped.
   - The original is marked dead.
2. **Whole stroke (Kana's).** Every stroke the path touches is marked dead, with all its pieces.
3. **Trim to intersection.**
   - For the touched stroke, find where its centerline crosses other live strokes' centerlines (candidates come from the R-tree).
   - Remove the stretch between the two crossings nearest the touch.
   - With no crossing, the whole stroke goes (ToonSquid's rule).

One sweep of the eraser is one undo step: what died, and what was born from it.

---

## 6. Undo

Kana's log, generalized. It keeps the applied actions plus the undone ones after them, and a new edit drops the undone tail.

- **Actions:**
  - BORN (ids): a stroke, a paste;
  - DIED (ids): whole-stroke erase, delete;
  - REPLACED (dead ids + born ids): the partial and trim erasers;
  - MOVED (ids or frames, with the transform before and after);
  - SET (object, field, before, after): restyling, from Phase 1;
  - FRAME_NEW.

  Every action names its frame.
- **One gesture, one action:** a stroke, an eraser sweep, a move.
- **Undo goes to the change.** If an undone change is off screen, the camera flies to it, keeping its zoom where it can. Moving the camera is never an action.
- **Bounded by bytes** (32–64 MB), not by step count. The oldest actions drop off, and so do payloads only they kept alive.
- **Saved with the document.** The journal (§7) is the action log, so reopening a canvas keeps its undo history, with the same byte bound.

---

## 7. The file

One file per canvas, beside today's in `notes/`: `<id>.<ext>`, with the extension chosen with the app's name. The canvases-and-folders index (`notes.h`) is reused as it is.

**The same chunk layout as every Kana file** (`kfile.h`):
- the header "KANA", version, and kind 'ZOOM';
- chunks of u32 tag, u32 size, payload;
- little-endian, written byte by byte;
- unknown tags skipped, and records grow at their end.

Two things are new:
- the file is written as a log;
- a footer lets a reader jump to the index.

**Chunks:**

| Tag | Holds |
|---|---|
| `JRNL` | The actions since the last checkpoint, in order, ending in a checksum |
| `BKTS` | A bucket of up to ~512 of one frame's records, with their payloads |
| `FRAM` | The frame records: transform, anchor box, impostor reference |
| `THMB` | A frame's impostor, as PNG |
| `INDX` | Frame id → its bucket offsets, bounds and thumbnail; the camera; the journal position it covers |
| `FOOT` | Fixed size: magic, the `INDX` offset (u64), checksum. Always the file's last chunk after a checkpoint |

**Writing:**
- **The journal, about every second while editing:** append a `JRNL` with the new actions, then `rde_file_sync`. This is small and cheap, and it means every stroke is on disk within a second.
- **A checkpoint, every few hundred KB of journal, when leaving the canvas, or when going to the background:**
  - append the dirty buckets, frames and thumbnails;
  - then a new `INDX` and `FOOT`;
  - then sync.

  Earlier copies of those chunks become garbage. Nothing is ever overwritten in place.
- **Compaction, when garbage passes ~50%, and before export, share or backup ("Your data"):** write a fresh file with only live chunks to `.tmp`, sync it, and rename it into place. This uses kfile's atomic rename and its `.bak` rule.

**Opening:**
1. Read the header, then the `FOOT` at the end.
2. If the footer is torn (the app was killed during a checkpoint), scan back to the previous valid one.
3. Read the `INDX`, and load only the frames near the saved camera. The rest load as the camera comes near them.
4. Replay the `JRNL` chunks after that index, in order, stopping at the first bad checksum.

The worst case after a kill is losing the last second.

**Kana's safety rules stay.** A file that doesn't parse is set aside as `.bad` and never overwritten, so a reader bug cannot wipe a canvas by autosaving an empty one over it.

**Size target:** a million points should take about 1.5–2.5 MB.

---

## 8. Memory

- **Budgets in bytes:**

  | Cache | iPad | Android |
  |---|---|---|
  | Decoded strokes | 64 MB | 32 MB |
  | Impostor textures | 128 MB | 64 MB |
  | Undo | 32–64 MB | 32–64 MB |

  Tuned in the gate.
- **LRU eviction.** Everything is purged on `RDE_EVENT_TYPE_MOBILE_LOW_MEMORY`; the shells already handle that event for fonts.
- **Frames far from the camera unload** their buckets and load them again from the file when the camera returns. Opening a huge canvas costs only what is near.
- **Impostors** are rendered offscreen for frames that are dense or small on screen. They are saved as `THMB`, so the first frame after opening a file is already drawn.

---

## 9. Tests

New suites in `tests/`, run by `tests/run.sh` with ASan and UBSan like the others:
- **Codec:** encode and decode round-trips exactly, across random strokes, every q, and the edge sizes (1 point, 512, 513).
- **Frames:** transforms both ways; entering and leaving at the thresholds (hysteresis); a new root; handover when panning; the "later on top" rule.
- **R-tree:** query results against brute force on random boxes.
- **Erasers:** the pieces against a reference implementation; no slivers; trim with and without crossings.
- **Undo:** random sequences of edits, undo and redo, against a simple reference model.
- **File:** truncate a written file at **every byte offset**, then open it. It must open at the last good checkpoint plus every whole journal chunk after it. Flipped bytes are caught by the checksums. Compaction keeps exactly the live content.

---

## 10. The gate before Phase 1

On the Galaxy Tab and the iPad, with release builds:
1. **Speed.** A synthetic canvas of **1 million points and 10k objects** spread over 4 depth levels pans and zooms at 60 fps. Pen latency (`stale_ms`) is no worse than Kana's.
2. **Deep zoom.** Zoom in through 20 levels (10⁶⁰) and back out. Strokes drawn at every level stay exactly where they were, with no wobble; this is checked with screenshot comparisons at each level.
3. **Crash.** On the desktop, kill the app at random moments during writing and saving, 1000 times. The file always opens, and at most the last second is lost.
4. **Size.** A million points saved take at most 2.5 MB. The real bytes per point on our ink are measured and written into this doc.

---

## 11. Order of work

1. RDE: `rde_file_sync`.
2. `fude/drawing`: the pen sampler, the page seam, the toolbar's tool table. Check that every existing app's shots are byte-identical.
3. `fude/zoom` models with their tests: codec → frames and camera → objects and R-tree → undo → file.
4. `fude/zoom` page and drawing: the pen, the erasers, rendering across levels, impostors.
5. `apps/<name>`: the shell, its strings (in the five languages, overnight per the usual rule), the icon later.
6. The gate. Then release builds.

## 12. Open for you

1. **The app's name.** It sets `apps/<name>/`, the store name, the file extension and the bundle id.
2. **The toolbar.** I propose extending the shared toolbar with a tool table (§1.2), so the app keeps the toolbar's look, docking and RTL. The alternative is a toolbar of its own, since later phases add many tools (shapes, instruments). My recommendation is to share it and add a "more tools" panel when it fills up.

---

## 13. Status (2026-10-04): Phase 0 built

**Built.** `fude/zoom/` and `apps/sketching/`:

- **The canvas:**
  - depth frames, the camera with its thresholds and hysteresis, a new root when zooming out past the top;
  - handover between frames at the same depth, and the "later on top" frames.
- **Drawing:**
  - exact while the error bound holds, with a backdrop above that;
  - strokes far bigger than the screen cut to it on the CPU;
  - a decoded-stroke cache with a byte budget.
- **Editing:**
  - the pen (zoom-relative width by default);
  - the three erasers;
  - undo and redo that fly to the change and are saved with the file.
- **The file:** the log-structured format of §7.
- **Engine:** RDE gained `rde_file_sync` and `rde_file_read_at`.
- **Shell:** the core gained the page kind (§1.1).

**Measured:**

| What | Result |
|---|---|
| Codec | 1.64 bytes a point for x, y and pressure; 1.98 with time |
| A million points on disk, everything included | 2.04 MB (target: at most 2.5) |
| A million points over four depths, Mac debug build | 94 fps, 10.4 ms to draw |
| Test suite `zoom` (ASan + UBSan) | passes, see the list below |

The `zoom` suite covers:
- 20 levels (10⁶⁰) in and back out, with every stroke exactly where it was drawn;
- the erasers;
- 300 random edits undone and redone;
- the file reopened point for point, and cut at every byte offset, always opening to the last state that was on disk.

Screenshots from inside a stroke's edge at 10³, 10⁶ and 10⁹ put the edge where the maths says. At 10¹⁵ the screen is the stroke's colour.

**Where the build differs from the plan, and why:**

- **The pen sampler (§1.3) was not moved out of `ink.c`.** The deep-zoom page captures each stroke with an ink of its own (Kana's pen exactly: pressure, pens without pressure, sampling), then converts it when the pen lifts. Kana is untouched. Move it out if the capture ever has to differ.
- **The toolbar's tool table (§1.2) waits for Phase 1**, when shapes and instruments need it.
  - For now the toolbar counts presses of Erase while the eraser is already chosen (`erase_taps`), and the page turns each into the next eraser mode, announced in a notice.
  - E does the same on a computer.
- **The pressure channel holds each point's share of the stroke's half-width** (0–1023), as the capture's width curve gave it, so widths come back exactly. Raw pen pressure for re-brushing later (Phase 1) will need a channel of its own.
- **Lazy loading:** the INDX allows loading per frame, but every frame loads when a canvas opens. Frames never unload. The cost is small: records plus encoded points, about 1.6 bytes a point.
- **No impostor textures yet.** Child frames are drawn directly, skipped under half a pixel, and points closer than 0.6 px are left out. This was fast enough at a million points on the Mac. Revisit after the device gate.
- **Paper (done since):** dots, ruled lines and plain squares, all on one lattice of 50·2ⁿ frame units.
  - The n is the one that keeps the spacing comfortable on screen.
  - The next finer level fades in as you zoom, so it never jumps.
  - New frames' origins snap to the lattice, so it runs unbroken through a change of frame.
  - Settings' Lines & squares size sets how dense lines and squares are.
- **Lasso and the page's long-press menu** are not on this page yet (Phase 1).

**Found on the Galaxy Tab: an engine bug, now fixed.** RDE's `rde_file_write_bytes` reopened an append handle as "wb", which truncated the file. Every journal append therefore left only itself, and the canvas reopened as damaged.
- The fix: writes keep append and read-write handles as they are.
- The unit tests had missed it because they use stdio stand-ins for the engine's files.
- Checked since with the real engine on the Mac: quit and reopen, and `kill -9` after drawing then reopen. Both bring everything back.

**The kill test (§10.3), 200 rounds on the Mac.** Each round: `--stress` draws without pause, a `kill -9` lands at a random moment between 0.8 and 6 s, then the canvas reopens.

| Rounds | Edits | Result |
|---|---|---|
| 100 | Mixed: strokes, sweeps, undo, zoom | Never damaged, never recovered from the backup |
| 100 | Strokes and zooms only (`--stress-add`), so the count can only grow | Never fewer strokes than the last time it opened: nothing on disk was ever lost |

The script is in the session's scratchpad (`kill_test.sh`). The full 1000 rounds can run overnight.

**The gate (§10) still open: device numbers.**
- **Galaxy Tab:** the Android release runs and survives a force-stop. Its frame times with `--gate` still need a build that takes arguments.
- **iPad:** needs a `com.rde.sketching` profile from Xcode.


---

## 14. Phase 1, as it goes (2026-10-04)

**Built:**

- **Paper:** dots, ruled lines and plain squares on one lattice that runs on unbroken at any zoom (§13).
- **Lasso** (`zoom/select.h`):
  - **Taking things:** a loop takes a stroke or shape when most of it is inside. It takes a frame whole, with everything drawn deeper inside it, when its anchor is all inside. A frame only partly inside is gone into.
  - **Handles:** drag inside the box to move; a corner scales round the opposite one; the knob turns round the middle. While dragged, the selection is lifted and drawn on top. One undo step, a MOVED action.
  - **Menu:** the core's Cut, Copy, Duplicate, Delete, Paste and Select all, through the page kind.
  - **Clipboard:** copies, not references, so it survives a change of canvas. Paste keeps the size on screen.
- **Places:** objects carry a rotation and a scale apart from their points (f64), so moving never touches the points. Erasing a turned or scaled stroke cuts it in its own coordinates. Frames can be deleted and undeleted, and that survives a reopen.
- **Shapes** (`zoom/shape.h`): line, rectangle, ellipse and polygon, kept as numbers, outline or filled.
  - **Hold to snap:** pen held still for half a second at a stroke's end. Line, circle or ellipse, rectangle, triangle or polygon (whichever fits closer). The shape then follows the pen, bigger, smaller or turned, until it lifts. The first undo brings the stroke back.
  - **The Shapes tool:** an app tool with a choices panel (a new core toolbar feature, extension.h: line, rectangle, ellipse, triangle, Filled), dragged out.
  - **Everywhere else:** erased whole, lassoed, copied, saved.
- **Pictures** (an IMAGE object: its half-size, then the file's own JPEG or PNG bytes):
  - **Bringing one in:** the Picture tool (Photos or Files), a share into the app, or a drop. The core hands arrivals to the page kind's new `imported` hook. Every picture is first read upright, as it was taken, cut down to 4096 px on the long side, and kept as JPEG, or as PNG when it has transparent parts (`fude_picture_bytes`: ImageIO on a Mac or an iPad, which also reads HEIC; ImageDecoder on Android). A picture lands in the middle at about 60% of the screen, selected, as one undo step.
  - **Drawing it:** decoded once into a texture cache (256 MB, least recently drawn dropped first). Deep in, only the part on screen is drawn, so a picture stays right at any zoom.
  - **Moving it:** lassoed when most of its corners and its middle are inside, then moved, scaled, turned, copied and deleted like anything else.
  - **Erasing:** the eraser never takes a picture. What is written over a photo rubs out and the photo stays, as in GoodNotes and Notability.
  - **Tests:** a round trip through the file, compaction and undo (`tests/zoom`). Checked by eye at 1×, 12×, 100× and 3000×, and lassoed, moved and turned.
- **Smoothing** (`zoom/smooth.h`, the toolbar's Smoothing: Off, Low, Medium, High, Rope; remembered in the settings, Low by default):
  - **Low, Medium, High:** a Gaussian along the stroke, 2, 5 or 10 screen points wide. Each point is put where a weighted line through its neighbours puts it (local linear regression), so it never lags behind the pen as StreamLine's average does. Near an end, the window widens on the side that has points, so an end is about as steady as the middle and gets no hook. Measured on 150 shaky strokes, the ends sit about half as far off as with a window that narrows to the last point, and strokes do not get shorter.
  - **Rope:** the pulled string (Lazy Nezumi, Krita). A tip 22 points behind the pen moves only when the string is taut, and catches up when the pen lifts (Krita's finish line). Low's smoothing goes over it.
  - **Drawing it:** the page keeps the pen's points and smooths only the end that has not settled yet: a point is final once the pen is past its window. What is drawn while the pen moves is exactly what is saved (a test checks the live result against smoothing the whole stroke at once).
  - **A change from §4:** the smoothed line is what is kept, not the raw points. Smoothing at draw time would kink at the 512-point piece joins and at erase cuts, and the eraser and lasso would act on a line other than the one on screen. Changing smoothing after drawing can come back as an undoable command on selected strokes.
  - **The widths are first guesses**, to be tuned on the tablet and the iPad.
  - **The toolbar:** app tools went from 2 to 4 (`FUDE_EXTENSION_TOOLS`). Four Phosphor icons were added (wave, three signal levels), the fonts cut again in all 8 apps, the bearings measured again.
- **Getting around** (`zoom/nav.h`):
  - **Flying:** the camera is carried between two views however many levels apart. It goes out until both ends are on screen, across, then in; the zoom eases on a log scale and the pan happens mostly while far out. Each step is worked out from one end's own coordinates: the coarser end while the camera is further out than it, the deeper end otherwise. The camera then goes into the ancestor where that zoom belongs. This keeps every step exact at any depth and creates no frames on the way. Tests fly from the top down 8 levels, back up, and across to another branch: every step's zoom stays in range, the frame count never changes, and the camera lands exactly. A flight takes 0.45 s for a short hop and up to 2.5 s across a thousand binary levels; any touch stops it where it is.
  - **What flies:** undo and redo (to the change, when it is off screen), Reset view, Back, the marks and the depth's levels.
  - **Marks:** when nothing is drawn on screen, arrows at the edge point to the nearest content in each eighth of a turn, and rings mark content that is on screen but under 2 points. They search the camera's frame and the two above it, best first through a new R-tree nearest search (`fude_zoom_index_nearest`). An arrow under the toolbar slides inward until clear. A tap flies to the thing and its leaf's neighbours, filling 60% of the screen.
  - **Back:** a button beside the menu while there is somewhere to go back to (16 views, by frame id; a frame dropped since is seen from its parent). B on a keyboard.
  - **Depth:** a chip at the bottom left once the zoom is not ×1: ×12, ×0.004, ×10⁶, ×3·10³. Tapping it lists the levels above (×1, ×10³, ×10⁶…, the top last); a level flies there.
  - **Home:** the canvas's own frame, its first root. A new root above it (zooming out past the top) leaves it home, and frame records now end with a byte saying which frame it is (older files: the root). The depth is measured from home and Reset view goes there; before, both followed the current root, which moved 1024× with every new root.
  - **Places** (bookmarks): a button at the bottom left (the depth chip beside it) opens a list: Mark this view (Unmark this place when the view is at one), then the places, the newest first, each "Place N · ×depth" with a × beside it to remove it; a place flies there. Each is a MARK object (a new kind, never drawn: the view at its translation, its zoom and number in its payload), so making one and letting one go are undo steps like any other (undo flies to the view it kept), and it is kept as any object is; Clear leaves them. (First they were kept apart from the history, like the camera; Borja expected undo to reach them.) Tests: undone and redone, through the journal, a checkpoint, a reopen and a compaction.
  - **Not yet:** search hits flying through frames (Phase 4).
- **Tap to fill** (`zoom/fill.h`, the toolbar's Fill):
  - **A closed shape** takes the brush's colour as its own fill (a new flag, FILL_OWN, and a fill colour that object records now end with), drawn under its line so the line stays whole. Tapped again in the same colour, it lets the fill go.
  - **A closed line** gets a FILL object: a new kind whose points are a loop of the gesture's centreline (all its pieces), drawn just under the line in order, so no seam shows between them. A line counts as closed when its ends are within a fifth of its size (or 24 points on screen), or when its end runs over its beginning. The line is then split at its crossings into simple loops (walking it, each loop it closes is taken out), and the smallest loop round the tap is filled: a figure 8's one lobe, a hand's loop without the bit that ran past its start (asked for on the tablet: at first both lobes filled).
  - **A fill** tapped is recoloured, or removed when it already has the brush's colour.
  - **Drawing it:** RDE fills only convex polygons (a fan), so fills are triangulated here: a slab decomposition cut at every corner's height and every crossing's, even-odd. It is exact for any shape and any crossings (tests: a square, a star, a figure 8, a box cut from a huge square). The triangles are kept with the decoded points; a fill far bigger than the screen is cut to it first. Concave polygon shapes now fill correctly too.
  - **Erasing:** the partial eraser rubs a fill out where it passes. Each sweep's reach (everywhere within the eraser's radius of its path) becomes one cut ring of the fill: the distance to the path sampled on a grid a third of the radius fine, its edge traced by marching squares, the outer loop kept and simplified. It is traced again as the path grows, and the points carry their ring in the time channel. The triangulation takes the union of the cuts out of the outline in the same slab sweep (with an active edge list), so it stays exact and needs no polygon library. At first each eraser step added its own capsule ring; hundreds of overlapping rings a sweep brought the tablet to a near stop (reported). Now a 240-step sweep over a 600-corner fill costs about 0.5 ms a step on the Mac, cut and triangulated, and a test keeps it under 16 ms. A fill rubbed out all over in a sweep is removed; undo brings it back whole. A shape the partial eraser reaches becomes its line (a closed stroke) and its fill (a FILL under it), which are cut as any line and fill — its numbers are let go only then. The other eraser modes still take shapes and fills whole. (At first a fill went whole as soon as the eraser touched it, even only its line: reported from the tablet.)
  - **Elsewhere:** the lasso picks a fill like a stroke; copy and paste keep fills, cuts and fill colours; the deep-zoom backdrop shows a shape's own fill.
  - **Not yet:** bucket fill of regions bounded by several strokes, with gap closing (Phase 4).
- **Export** (`zoom/export.h`, the toolbar's Export: Image or Drawing (SVG); shared on a tablet, saved where chosen on a computer). Both are of the view as the camera has it:
  - **PNG:** the view drawn again off screen at up to 3 pixels a point (4096 on the longer side), through the same renderer into a render texture, then read back in the next frame's update (the GPU has to have finished it) and encoded. RDE gained `rde_image_encode_png` for that (its vendored stb_image_write, now public).
  - **SVG:** written on the CPU from the same frames the renderer draws (three levels up, down to half a point, child frames in their places). Strokes are paths, their width followed in runs where the pressure moves it (round ends meet at the joins). Shapes are their outlines with fill and line in one element. Fills are even-odd paths with the eraser's cuts as a mask. Pictures are embedded as their own JPEG or PNG, placed by a matrix.
  - Found on the way: child frames are not in a frame's index (they are in its list of kids), so the empty screen's arrows had missed what is drawn deeper. They look there too now.
  - **Not yet:** PDF, a chosen region or a whole frame rather than the view, and SVGs that open again for editing (research §6).
- **The pen's tip under the pen** (reported from the tablet: the drawn tip trailed it). While the pen is down, the smoothed line's last two widths are eased onto the raw pen point, so the tip is always under the pen; when it lifts, the smoothed end takes its place. (A prediction that drew the live stroke 24 ms ahead of the pen was tried and taken out: plain extrapolation overshoots at every curve and stop, and on the tablet writing felt less steady than in Kana, which draws the pen's own points.)
- **The brush's circle:** a soft disc of the pen's, the marker's or the eraser's size, where it would draw — while the pen hovers (the S Pen; an Apple Pencil on iPads that hover), under the mouse on a computer, and under the eraser as it erases. With the width following the pressure: the full width, and a dot for the lightest. Asked for on the tablet. The hover point is eased (a pen leaving the glass reads unsteadily), hidden for a moment after the pen lifts, and a finger's touch drops it (a finger does not hover).
- **The brush always on the screen:** Sketching's pen is the same size on screen at any zoom, so the toolbar's Page/Screen is left out (a new extension flag, `brush_on_screen`, which also keeps a saved Page setting from coming back). Asked for on the tablet.
- **Tools reach a little above the camera** (reported: at ×180 the eraser and Fill did nothing to the writing on screen). Phase 0 let tools act only at the camera's depth and below, but past ×64 the camera is in a frame of its own, so writing drawn further out was seen and read-only. Now a frame above it is editable while its unit is at most 256 points on screen (`FUDE_ZOOM_RENDER_EDIT_UP`); further up, a stroke's width runs to thousands of points and the eraser would take huge cuts. Pieces the eraser cuts there get finer quanta (a power of two finer, never past 32-bit numbers), so their ends fall where the eraser was, not on the stroke's coarse grid.
- **Strokes far bigger than the screen are drawn in the runs near it** (reported: zooming into writing, the last bit flickered, then went). Every round join of the whole stroke was tessellated, far out of sight, and ran the engine's 2D buffers out, so the draws were dropped. Now a stroke over twice the screen is drawn only in the runs that come near it, each starting and ending further than its width away, so the caps are not seen.
- **Fill and the pen:** Fill (or a shape) chosen with the eraser or the lasso in hand puts the pen in hand under it (reported: the tap erased instead). Not a tap of the bar's own, which would put Fill down again.
- **Filling an "a"** (reported): a line closed only by crossing itself fills only the loops it draws (its bowl). The end is joined back to the start only when the two are near each other.
- **The selection box deep in:** each side is clipped to the screen before its dashes are drawn. At 3000× a side was 1.4 million points long and filled the engine's line buffer.

**Core changes:**
- **Page kind:** gained `selection`, `command`, `can_paste` and `imported`.
- **Toolbar:** app tools can have choices panels; the toolbar counts `tool_taps` and `erase_taps`.
- **Looks:** gained `--shot-at=N`.
- **Icons:** six new ones (fonts cut again, bearings added, existing ones unchanged).

**Next in Phase 1:** guides; PDF export.
