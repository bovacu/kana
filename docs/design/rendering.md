# The renderer and its caches

The renderer draws a canvas (`scene.h`) on the screen: every frame the camera can see, nested to any depth, exactly,
at 60 frames a second, without giving the GPU a number it cannot hold. It is `fude/zoom/render.c` (the coordinator:
the renderer, its caches, pictures, frames, the backdrop, the draw) and four modules by what they draw:
`render_stroke.c` (strokes and fills), `render_shape.c` (shapes, measures, boards, sheets, guides), `render_text.c`
(formulas, notes, texts) and `render_symbol.c` (symbols and graphs). `render.h` is what the rest of the app sees;
`render_parts.h` is what the modules share.

Most of this note is the caches: what each keeps, by what key, up to what bound, and what makes it stale. They were
reworked on 2026-10-15 (`docs/quality_plan.md` Phase 4, items 4.1 to 4.6, with the numbers); this is the result.

---

## Where it sits, and who owns what

The page (`page.h`) owns one `fude_zoom_renderer`, made by `fude_zoom_render_init` in `fude_zoom_page_init` and freed
by `fude_zoom_render_destroy` in `fude_zoom_page_destroy`. Each frame `page_render.c` sets what the renderer draws with
(the fonts, the units, a board's materials' names, the objects lifted by the lasso, those drawn faint while a
recording plays, graphs playing, a mechanism's moving parts), calls `fude_zoom_render`, and puts the per-draw pointers
back. The lasso draws what it drags with `fude_zoom_render_object` and its glow with `fude_zoom_render_glow`. The
tools ask which frames the last draw went into (`fude_zoom_render_editable`, from the renderer's `visible` list) and
map the pen through them.

The same renderer draws the page's off-screen pictures (a cover, the map, a strip page, a PNG export, a zoom video's
frame, an arrow's preview) with the camera swapped for the draw. They are drawn before the page's own draw each frame
(`fude_zoom_page_render_offscreen`), so `visible` is always the screen's when input comes.

Main thread only, inside the app's 2D drawing block: it draws through the engine and loads textures.

## A draw

`fude_zoom_render(renderer, scene, half, paper)` takes three steps.

**1. The top frame.** Precision comes from frames (`scene_and_index.md`): each frame has its own units, 1024 times
finer than its parent's. A frame is drawn through an f64 transform to the screen; the further up it is, the bigger
that transform's translation in screen points, and the larger its rounding error (|translation| × 2⁻⁵²). The renderer
climbs from the camera's frame at most three levels, while that error stays under `FUDE_ZOOM_RENDER_EXACT_PT` (a
quarter of a point). `infinite_canvas_design.md` §2.3 has the table: three levels up is 0.15 pt for content 10⁴ units
across, four would be 150 pt.

**2. The backdrop.** Above the top frame nothing can be drawn exactly, but zoomed far into a line you are inside its
colour, and the screen must show it. For each frame from the root down to the top frame's parent,
`fude_zoom_render_cover` looks at the camera's point there: the topmost alive stroke, fill or shape covering it whose
draw key (below) is under the child frame's (it is under the drawing), and the topmost whose key is over it (it is
over the drawing). It compares draw keys, the layer's rank and then the order key, because that is how
`fude_zoom_render_frame` sorts what it draws: the backdrop must say what drawing the frame would show, layers and all.
What a hidden layer holds is drawn nowhere, so it covers nothing. The deepest "under" colours the whole screen
first; the first "over" found from the root down colours it last, over everything, as later is on top. The test
reads what the caches already hold, a stroke's decoded points and a shape's kept outline at the detail its size there
asks for, so nothing is decoded or made again while the camera stays inside.

**3. The frames.** The top frame is drawn with the frames beside it in its parent (a cousin's drawing can reach the
screen too), in the parent's order. `fude_zoom_render_frame` draws one frame and goes down into the frames in it:

- it adds the frame to `visible`, with its transform and depth;
- it finds its objects on the screen (the screen and 2 points round it, mapped into the frame's units) with
  `fude_zoom_scene_query`, plus the child frames whose anchor boxes meet that view (FRAME objects are not in the
  index: the frame's `kids` list is walked);
- it pushes them as a run onto the renderer's `items` stack and sorts the run by draw key;
- it draws them in that order, skipping what the lasso lifts (the selection draws it where the drag has it), the
  text being written (it draws itself), and what a hidden layer holds; a child frame is gone into, recursively, when
  its anchor is at least `FUDE_ZOOM_RENDER_MIN_PX` (half a pixel) across;
- it cuts the stack back to where its run began.

```
items:  [ the parent's run, sorted | this frame's run, sorted | a child's run ... ]
                                     ^ _base                    (cut back to _base when done)
```

The stack is read by index each time (`items.memory[_base + i]`), since a frame drawn inside grows it and may move it.
Once it has grown to the deepest draw's size nothing is allocated or freed a draw (4.4).

**The sort key** is `fude_zoom_scene_draw_key`: the object's layer's rank in the top 8 bits, the low 56 bits of its
order key under it. Layers decide what is over what within a frame; within a layer, what was made later is on top.
A child frame sits in its parent's order like a stroke, so "later on top" holds at every depth. A FRAME object's
`layer` field is 0, so a child frame sorts at layer 0's rank, and everything it holds is drawn at that one place
among its parent's things, whatever layers it is on. A frame is drawn whole, in one place of its parent's order, so no
one rank would be right for a frame holding several layers' things, and layer 0's keeps a canvas of one layer as it
always was (`scene.h`, above `fude_zoom_scene_draw_key`). The consequence: something drawn deep on a layer ranked
above layer 0 is drawn under its parent's things on such a layer where they cover it, even those drawn before it.
This is an open decision (below). The order key must stay under 2⁵⁶; keys are spaced 2³² apart (`FUDE_ZOOM_Z_STEP`),
so that is some 16 million objects made on top in one frame.

**Small things.** A stroke, fill or shape under about a third of a pixel is not drawn; a stroke's points closer than
0.6 px to the last one drawn are left out (a marker's, closer than three quarters of its half-width, since a
see-through line that folds over itself shows darker); ink is never thinner than `FUDE_ZOOM_RENDER_MIN_RADIUS`. A
text under 3 px is a faint bar a line, and over 3,000 px nothing. The draw counts strokes, points and frames for the
HUD.

## Far things, and why they are cut on the CPU

Every position is worked out in f64 on the CPU, relative to the camera, and the GPU is given screen-sized f32
numbers (`infinite_canvas_design.md` §2.3). Zoomed deep inside a stroke, its points can be 10¹² points off the screen
and its width more than the screen; f32 cannot place them, and the GPU must never see a number that big. And a stroke
far bigger than the screen, drawn whole, has every round join tessellated, seen or not, which runs the engine's
buffers out (draws dropped: the ink flickers, then goes). So what reaches `FUDE_ZOOM_RENDER_FAR` (10⁵ screen points)
is cut to a guard box on the CPU first, the screen twice its size each way:

| What | When it counts as far | What is done |
|---|---|---|
| a stroke | its half-width, or its translation plus its size, past `FUDE_ZOOM_RENDER_FAR` | each segment's band and each round join made into polygons and clipped to the guard box (`fude_zoom_draw_huge_stroke`) |
| a stroke bigger than the screen | wider than the screen's width and height together | only the runs of it that come near the screen are drawn, each a stroke of its own |
| a fill | a tenth of `FUDE_ZOOM_RENDER_FAR` across | each ring mapped to the screen, clipped, triangulated for this draw |
| a shape or a cut board | its line's half-width or any outline point past `FUDE_ZOOM_RENDER_FAR` | its outline on the screen cut to the guard box: the line through `fude_zoom_draw_huge_stroke`, the fill clipped and triangulated as seen once, not from its kept triangles |
| a picture | half of it a tenth of `FUDE_ZOOM_RENDER_FAR` across | only the part of its pixels on the screen (the screen, half as big again) drawn, as a sub-rectangle |

A far shape is the one case where kept geometry is not used: its kept triangles, moved to the screen, would put
corners far out, which is what this rule exists to prevent (4.3: "a shape far bigger than the screen is still
clipped"). Far things are rare and costly; the far stroke allocates its two arrays for the draw.

## The caches

Three rules shape the renderer's caches since 4.2 to 4.6; where one breaks a rule, its section below says so.

- **Keyed by the whole of what it is made from.** Two things that differ in anything that decides the result never
  share an entry, so nothing is drawn from what came before an edit. Two keys lacked a part before (the notes'
  fonts, draw.c's widths' font size) and could have given a stale answer.
- **Bounded, with the one used longest ago making room, never one used in the current draw.** Forgetting everything
  at once when full (as symbols' geometry and the notes' layouts once did) made a whole screen again each time a new
  thing came. Past a bound, what does not fit is made each time while the rest stay kept.
- **Kept in the thing's own units, drawn through its place.** Moving or turning a thing, or panning, makes nothing
  again; a change of size on the screen does only where the detail it asks for changes (a symbol's detail is rounded
  up to a step of 8 for this).

| Cache | Where | Key | Bound | Made again when |
|---|---|---|---|---|
| decoded strokes and fills | render.c `cache`, `slot_of` | the object's index | 64 MB (`FUDE_ZOOM_RENDER_CACHE`) | let go past the budget, or trimmed |
| kept geometry: symbols, shapes, cut boards | render.c `geos`, `geo_lru` | `fude_zoom_render_geo_key` | 2,048 entries (`FUDE_ZOOM_RENDER_GEOS`), 2²¹ points and corners (`FUDE_ZOOM_RENDER_GEO_POINTS`) | a new payload (an edit), another detail or size |
| notes' layouts, and plain texts' lines | render_text.c `rich`, `rich_lru` | fonts, sizes, width, alignment, then the words | 256 (`FUDE_ZOOM_RENDER_RICH`) and a spare | a word, size, width or font changed; a plain text's lines also when its size or width on the screen changes (a zoom) |
| graphs' lines | render.c `plots`, `plot_lru` | the graph's kind, its text, its half sizes | 24 (`FUDE_ZOOM_RENDER_PLOTS`) and a spare | its text or size changed; every frame while it plays |
| pictures on the GPU | render.c `pictures`, `picture_of` | the payload's handle and serial | 256 MB at 4 bytes a pixel (`FUDE_ZOOM_RENDER_TEXTURES`) | let go past the budget, or trimmed |
| junction dots | page_circuit.c `junctions` | the scene's revision, the hidden layers, the frames drawn and their transforms, the screen's size | one answer | any of those changed |
| text widths and layouts | `drawing/base/draw.c` | the font, its size, the size drawn at, the width, the text | 1,536 widths, 64 layouts, 32 balanced widths, 32 whole sizes | (LRU); a plain text on the canvas keeps its lines in the renderer instead (the row above) |

### Decoded strokes and fills

A stroke's points are kept packed (`codec.h`, about 1.6 bytes a point) and decoded only when drawn:
`fude_zoom_render_decoded` keeps each one's points as f32 offsets from its translation, and its half-widths before
its scale, so neither a move nor a change of scale makes it stale. A fill's triangles (made the first time it is
drawn) and a cut fill's visible edge are kept with it. An object's points never change (an edit makes new objects), so
nothing but trimming makes an entry stale.

It is found directly: `slot_of[object]` is its entry plus one. It is bounded by bytes, 12 a point, 24 a triangle, 12
an edge point, because its entries run from a dot's few bytes to a long stroke's megabytes, and an `rde_lru` has a
fixed count of slots made up front: a count would either reserve room for the most strokes a screen can show or throw
out what still fits. Each entry carries the draw it was last used in; only when keeping a new one would pass the
budget is the cache scanned for the oldest not used in this draw, which is let go, the last entry moved into its
place. A pointer it returns is good until the next decode.

The backdrop's cover test reads the same entries (4.4), so a stroke the camera is inside is decoded once.

### Kept geometry: symbols, shapes and cut boards

One cache for three kinds of geometry (4.3, 4.6), each in its own units with its triangles, so nothing is cut into
curves or triangulated again while it stands still: a symbol's parts at its size and detail (`render_symbol.c`), a
shape's outline at its detail (`fude_zoom_render_shape_geo`), a cut board's rings (`render_shape.c`). Each entry is a
`fude_zoom_geo` in a slot of `geo_lru`, whose key is a `fude_zoom_render_geo_key` compared whole (set to 0 first, so
what a kind leaves out is always alike):

- a symbol's: its kind, its detail (rounded up to a step of 8, what zooming changes little), its half sizes, and its
  custom part's address or 0. The address is a safe name: a made part is kept for as long as the app runs, and a new
  version of a definition is a new part (`logic.h`);
- a shape's: its type, its payload (handle and serial), its detail, and how many of its numbers it is made from (the
  backdrop asks for all of them, the draw for `FUDE_ZOOM_SHAPE_NUMBERS`: two entries);
- a cut board's: its type and its payload.

**Why the payload and its serial.** A shape's numbers are its payload (`scene.h`'s `fude_zoom_blob`). A payload's
bytes never change: an edit makes a new payload. A handle alone is not enough, because a freed payload's slot is given
out again; the serial says which payload a slot holds now. So a key never finds stale geometry, and copies of a shape
(which share its payload) share its geometry.

**The spare.** `geos` holds one entry for each slot and one more after them. New geometry is always made into the
spare (`fude_zoom_render_geo_begin`, then `fude_zoom_render_geo_keep`), then moved into a slot: a free one, or that of
the one used longest ago, let go. When that one was used in this draw too (every slot was), or the points budget
cannot be met without letting go of this draw's, the new geometry stays in the spare, is drawn from there, and is made
again next time. So a screen with more than 2,048 kinds of thing keeps 2,048 and makes the rest each draw, instead of
every one pushing out the next. The pointer returned is good until the next geometry is made.

**The points budget** counts each kept entry's points and triangle corners (2²¹ of them, 32 MB of `fude_zoom_v2`).
A shape's or a board's triangles are made the first time a fill asks (`fude_zoom_render_geo_tris`) and counted then;
that may pass the budget for the rest of the draw, and the oldest not used in it go.

The Library's tiles draw their symbols from the same cache (`fude_zoom_render_symbol_geo`): every tile's symbol was
made each frame before (4.6: 11.94 to 0.095 ms a frame).

### Notes' layouts

A text with marks (rich.h: tasks, lists, headings, links) is laid out as a note, in its own units, by
`fude_zoom_render_rich_aligned`. Its key is a `fude_zoom_render_rich_how` (the four fonts' pointers, the size the
fonts are loaded at, the letters' size, the width, the alignment) followed by the words, compared whole. It is the
same pattern as the geometry: 256 slots of `rich_lru` and a spare after them; a new layout goes into a free slot or the
one drawn longest ago, unless that one was drawn in this draw, when it goes into the spare. A plain text (or a sticky
note with no marks) takes a slot too, marked as plain, so the next draw knows without looking at it again. The layout
returned is good until the next one is laid out (4.2: 300 notes over 50 frames, 69.7 to 21.6 ms).

**A plain text's lines** are kept in its slot as well (`fude_zoom_render_plain_lines`: the slot's `plain_spans`, with
the size and width on the screen they were found at). It is wrapped where `fude_draw_text_wrap` would wrap it
(`fude_draw_text_wrap_lines` and `fude_draw_text_wrap_spans`, the same layout and the same calls, so the same pixels),
and laid out again only when that size or width changes: a still view lays nothing out, a zoom lays each text out at
each step, as before. Kept here, it shares the notes' bound of 256 and the spare, so past it the texts one draw uses
stay kept and only the rest are laid out each draw (`test_render_plain_texts`: 300 texts over 50 still draws, 300
laid out the first and 44 each after). A slot made into for another text forgets the lines it held.

### Graphs' lines

A graph (plot.h) made from its text, an equation's grid or a curve followed finely, is not quick to make, so its lines
are kept while it stands still: `fude_zoom_render_plot_made`, keyed by its kind, a 0x1F byte, its text and its half
sizes' bytes, in 24 slots of `plot_lru` and a spare after them, the same pattern as the geometry and the notes. New
lines go into a free slot or the one drawn longest ago, never one drawn in this draw: when every slot was, they are
made into the spare and drawn from there. So past 24 graphs on the screen the 24 kept stay kept and only the rest are
made each draw, instead of every one pushing out the next (`test_render_kept_graphs`: 40 graphs over 30 draws make 40
the first and 16 each after). While a graph plays (its time, its sliders), its lines are made each frame into arrays
of their own.

### Pictures

A picture's texture is made from its payload's bytes the first time it is drawn (`fude_zoom_render_picture`) and kept
in `pictures`, found through `picture_of`, a hash map from the payload's handle and serial (the handle in the high 32
bits) to the picture's place, kept up to date as pictures move. Copies of a picture share one texture. One that could
not be read is kept as NULL and not tried again. Past `FUDE_ZOOM_RENDER_TEXTURES` (counted at 4 bytes a pixel), the
least recently drawn go first, found by a scan of their draw stamps when a new one would pass the budget, never one
drawn in this draw. Bounded by bytes for the decoded strokes' reason: a picture is anything from an icon to 4,096
pixels a side.

### Junction dots

Where a wire's loose end lies along another wire, a dot is drawn (`fude_zoom_page_render_circuit_junctions`,
page_circuit.c). Finding them takes every loose end against every wire segment on the screen, so the answer is kept
in screen points with what it was found from: the scene's revision, its hidden layers, the frames drawn and their
transforms (the editable list), and the screen's size (`fude_zoom_page_junctions_kept`). Any change finds them again
(a pan or a zoom too, since the dots are in screen points); a still view does no work. A canvas that has never had a
wire never looks: its objects are looked through once each,
as they are made. The scratch arrays are the page's, so a look allocates nothing (4.1). As the canvas closes, the
answer and the look are reset: its revisions start again.

### draw.c's text caches, as the canvas uses them

The renderer measures and wraps text with the core's `drawing/base/draw.c`, whose caches are the app's, kept for its
life:

- **Widths** (`fude_draw_text_width`): 1,536 in an `rde_lru`, per unit of size, by the font's pointer, the size it was
  loaded at and the text's bytes. The size drawn at is not in the key, since a width scales with it. A text whose key
  would pass 1,024 bytes is measured each time, and one with the engine's markup (a `[`) is estimated from average
  advances instead. Notes, labels, dimensions, pins' names and tags all go through it.
- **Wrapped layouts** (64), **balanced widths** (32) and **whole sizes** (32), each by the font, its loaded size, the
  size drawn at, the width and the text. A plain text on the canvas is wrapped by draw.c at its size and width on the
  screen, but its lines are kept by the renderer, in its slot of the notes' layouts (above), so draw.c's 64 layouts
  do not bound how many plain texts a still screen keeps: the notes' 256 and the spare do.

Not the renderer's: the formulas' set boxes (512) and glyphs' meshes (6,000) are `formula.h`'s, owned by the page,
and are dropped all at once when full. The strip's page pictures, the arrows' previews, the map and the cover are the
page's (`page_driver.md`).

## Trim and destroy

`fude_zoom_render_trim` empties every cache: the pictures off the GPU and out of their map, the decoded strokes and
`slot_of`, the kept geometry and its spare, the graphs' lines and the notes' layouts. All of them come back as they are
drawn. It is called:

- **when a canvas closes** (`fude_zoom_page_close`). This one is a must, not a saving: the decoded strokes are found
  by object index and the pictures and geometry by payload handle and serial, which are a scene's own. The next canvas
  gives the same numbers to other things (its serials start again from 1);
- **when memory runs short**, or the app goes to the background (`fude_zoom_page_trim`, from the shell's
  `RDE_EVENT_TYPE_MOBILE_LOW_MEMORY` and `RDE_EVENT_TYPE_MOBILE_DID_ENTER_BACKGROUND`), with the page's own caches and
  the fonts' glyphs.

`fude_zoom_render_destroy` trims, then frees the LRUs, the arrays and the pictures' map.

## Invariants

- The renderer draws one scene at a time; another scene (another canvas) only after a trim.
- A payload's bytes never change. Whatever edits a thing makes a new payload; an in-place edit would leave every kept
  geometry and texture of it stale.
- Nothing used in the current draw is let go; a returned pointer is used before the next call that makes the same
  kind of thing.
- No vertex far off the screen reaches the GPU: anything at `FUDE_ZOOM_RENDER_FAR` is cut on the CPU.
- A draw allocates nothing in the steady state (scratch arrays are the renderer's); the exceptions are far strokes and
  playing graphs.
- Off-screen draws come before the page's own, so `visible` is the screen's.

## How to change common things

**Keep geometry for a new kind of thing.** Add a value to `FUDE_ZOOM_RENDER_GEO_` and fill a
`fude_zoom_render_geo_key` (zeroed first) with everything that decides the geometry, a payload's handle and serial
when it is made from one. `fude_zoom_render_geo_find`; if nothing, `fude_zoom_render_geo_begin`, make the arrays you
fill with `fude_zoom_render_fresh`, then `fude_zoom_render_geo_keep`; draw from the pointer it returns through the
thing's place, before making other geometry.

**Draw a new kind of object.** A case in `fude_zoom_render_frame`, `fude_zoom_render_object` and
`fude_zoom_render_glow`, in the module of its kind; honour `_as` and `_extra` (a glow or a faint thing); skip it under
a third of a pixel; cut it on the CPU when it is far.

**A new cache.** Key it by the whole of what it is made from, bound it, never let go of what this draw used, and empty
it in `fude_zoom_render_trim`; then say so in this note and in the module's header.

## Open points

- **A child frame sorts at layer 0's rank: an open decision for the owner.** A FRAME object's `layer` field is 0, so
  everything a frame holds is drawn at layer 0's place among its parent's things, whatever layers it is on (`scene.h`
  documents it above `fude_zoom_scene_draw_key`). So something drawn deep on a layer ranked above layer 0 is drawn
  under its parent's things on such a layer where they cover it, even those drawn before it. The two ways out both
  change more than the renderer: draw each layer through every depth (the renderer's draw, `export.c`'s walk and
  `pdfview.c`'s export, which sort by the same key, would each go down once a layer), or give each frame the layer it
  was first drawn on. The backdrop (`fude_zoom_render_cover`) and the frame a new stroke goes in
  (`scene.c`'s `fude_zoom_scene_covered`) compare draw keys, so they follow whatever the key says.

## Where the tests are

`tests/zoom` (`bash tests/run.sh zoom`): `test_render_kept_shapes` (shapes' and cut boards' triangles kept, covering
what those made each frame cover; a reused payload slot drawn as the new shape; a far one still cut; 3,000 past the
bound), `test_render_tiles` (the Library's tiles from kept symbols), `test_render_kept_notes` (300 notes over 50 draws,
256 kept), `test_render_pictures` (found by payload, shared by copies, let go past the budget),
`test_render_frames` (the items stack, frames' order, the backdrop eight levels into a stroke),
`test_render_plain_texts` (plain texts' lines kept: a still view lays nothing out, and draws what laying out each draw
drew; past the bound, what one draw uses is kept), `test_render_kept_graphs` (graphs' lines kept, 24 and the spare,
timed against making all of them each draw), `test_cover_layers` (the backdrop with layers: a hidden layer's stroke
covers nothing, under and over by draw key), `test_far_views` and `test_far_lasso` (views far from 0). `tests/draw`
checks draw.c's layouts and balanced widths. Each change in Phase 4 was timed in its test against the old code kept
there (`docs/quality_plan.md` Phase 4 has the numbers); whole-app
frame times come from `--perf` and `--gate` (`COMMANDS.txt`). A stroke-heavy canvas spends about 55% of its render in
the engine tessellating strokes; keeping meshes is 4.9, not yet built.
