# Infinite canvas app: research and recommendations

October 2026. This is research for the planned deep-zoom drawing app, which grows out of apps/draw. The app has three uses:

1. sketching and vector art;
2. diagrams, mostly software;
3. woodworking templates with exact measures.

It also needs sticky notes, images, lasso, layers, fill and handwriting to text, all on one infinite canvas.

This document combines five detailed reports. Each one has the per-app findings, links and sources:

| Report | Covers |
|---|---|
| [1_sketch_vector.md](infinite_canvas/1_sketch_vector.md) | Brushes, bezier pen, shapes, eraser, lasso, images, deep zoom, undo |
| [2_diagrams.md](infinite_canvas/2_diagrams.md) | Connectors, shapes and libraries, layout, sticky notes, import and export, scale |
| [3_precision_woodworking.md](infinite_canvas/3_precision_woodworking.md) | Numeric entry, instruments, snapping, measuring, dimensions, printing, CAM export |
| [4_data_storage.md](infinite_canvas/4_data_storage.md) | Stroke encoding, object model, spatial index, undo, file format, memory budgets |
| [5_layers_fill_handwriting.md](infinite_canvas/5_layers_fill_handwriting.md) | Layers, paint bucket in vector apps, handwriting recognition engines |

Apps studied:
- **Sketch and vector:** Concepts, Procreate, Infinite Painter, Fresco, Illustrator for iPad, Affinity, Linearity Curve, Inkscape, Krita, Clip Studio, ToonSquid, GoodNotes, Notability, Apple Notes and Freeform, Samsung Notes, Rnote, Xournal++.
- **Deep zoom:** ZoomArt, Endless Paper, InfiniPaint.
- **Diagrams:** draw.io, Lucidchart, Visio, Miro, FigJam, Whimsical, Excalidraw, tldraw, OmniGraffle, MyScript, Microsoft Whiteboard, Mermaid, D2.
- **Precision:** SketchUp, Shapr3D, Fusion, Rhino, Morpholio Trace, BlockLayer.

Numbers marked *(sim)* were measured by the research agent on simulated data, not on our strokes. Anything about Apple's iOS 27 `PKStrokeRecognizer` comes from the agent's reading of a WWDC26 session. I haven't verified it, so check it before relying on it.

---

## The short version

**The gap.** No app combines all four of these:
- a zoom that never breaks;
- real drawing tools;
- CAD-grade numbers;
- diagrams that work with a pen.

Today each strength comes with a matching weakness:
- **ZoomArt and Endless Paper** have the zoom but thin tools: one brush, no smoothing, no shapes. Their reviews complain of lag, crashes and lost work.
- **Concepts** has the best pen tools and real precision, but no tiled print and no paint bucket.
- **Diagram tools** (tldraw, Excalidraw, FigJam) are built around hover and modifier keys, which a pen user cannot press.
- **CAD apps** have exact numbers but assume a keyboard and a CAD mindset.
- **On iPad, printing a big template at true size across A4 sheets is close to broken**, and that includes the OS itself.

**What makes ours better.** Seven differentiators, in order of how much they set us apart:

1. **A deep zoom that never breaks.** Nested frames keep every number small at any depth (agreed design, see §3.1). Tools are sized relative to the screen. The zoom video is the shareable output.
2. **Workshop precision with only a pen.**
   - Our own numpad: lengths, angles, fractions and arithmetic.
   - Real instruments, including an **escuadra and cartabón that slide along each other**. No app offers this.
   - **Tiled 1:1 printing that actually works on iPad**, with printer calibration.
3. **Diagrams built for the pen.**
   - Bind a connector by pausing instead of pressing a modifier key.
   - A pen stroke from one shape to another becomes a bound connector.
   - Rough boxes become shapes, and handwriting becomes labels.
4. **Nothing is destructive.**
   - Raw ink is always kept, so you can re-smooth, re-brush, revert converted text, and undo a shape back to the stroke.
   - Undo moves the camera to the change.
5. **Performance is the feature.**
   - Millions of stroke points and 10k+ diagram objects stay smooth.
   - Points cost about 1.5 bytes each on disk.
   - Saves are incremental and journaled, so no work is lost.
6. **Open files.**
   - Clean SVG with mm units, DXF with units set, and PDF.
   - PNG and SVG exports that carry the scene and can be opened again for editing.
   - draw.io and Mermaid import.
7. **Fair monetisation.** Never paywall layers, export, or shape libraries. Locked export, paywalled layers, account walls and ads are the top complaints about ZoomArt, Linearity and Miro. (Pricing is your call. This is only what the reviews say.)

---

## 1. What others do well and badly

| Area | Best at it | Copy | Avoid |
|---|---|---|---|
| Deep zoom | InfiniPaint (per-object big-number frames), Endless Paper (smoothness) | Coordinates local to a frame, sub-pixel culling, edge arrows to off-screen content, bookmarks | ZoomArt's lag, "white screen" and crashes; Endless Paper boards rolling back weeks |
| Brushes | Concepts, Procreate, Krita, Lazy Nezumi | Zoom-relative size; smoothing that stays editable after drawing; a set of stabilizers | One brush and no smoothing (ZoomArt, Endless Paper); Freeform's 2–3 s ink lag |
| Bezier pen | Inkscape BSpline, Linearity, Illustrator | Fit curves to freehand with a screen-space tolerance; tap-to-place pen; double-tap to switch node type | Desktop-only node editing that needs modifier keys |
| Shapes | Procreate QuickShape, Concepts | Hold to snap; stays live while held; 1–4 stroke recognition; editable until you tap away | Recognition that fires during normal drawing |
| Eraser | Clip Studio, Fresco, GoodNotes | Partial, whole stroke and trim to intersection; auto-return to the previous tool | Whole-stroke only (FigJam); chopped ends (frappe/draw); erratic erasing (Endless Paper) |
| Lasso | Concepts, GoodNotes | Tap-and-hold to lasso from any tool; filter by type, color and visible size | Lasso picking up hidden or locked content |
| Diagram connectors | draw.io, tldraw, Visio | Floating vs pinned binding; routing that avoids obstacles; line jumps; 3 labels per connector | "Smart" lines that rewrite what the user shaped (Miro, Lucid) |
| Diagram creation | draw.io (blue arrows), Whimsical, MyScript | Quick-add handles; ink → shapes and connectors; Mermaid → native shapes | Snapping and binding hung on Ctrl, Cmd or Alt |
| Diagram scale | — | GPU renderer, culling, re-route only the edges that changed, 10k+ objects | Miro and draw.io slow at 1–5k objects; Excalidraw freezing past ~1000 strokes |
| Sticky notes | FigJam, Miro | Fixed font size that grows the note; Tab adds the next note; ink belongs to the note | Text that shrinks until it can't be read |
| Precision entry | SketchUp, Shapr3D, Fusion | Type a value at any time; tap the live label to edit it; relative coordinates | System keyboard, no fraction keys, required hardware keyboard |
| Instruments | Apple and GoodNotes rulers, Concepts guides | Pen-to-edge snapping, soft detents, tappable angle readout | Rulers that jump (Fresco) or snap back to 0° (GoodNotes) |
| Printing | Sewing-pattern PDFs, Rhino (X/Y scale) | Tiles, overlap, registration marks, test square, per-printer calibration | iOS scale-to-fit, no tiling (Illustrator iPad, Inkscape since 2004) |
| CAM export | — | mm SVG, DXF with INSUNITS, layers by operation, a pre-flight check | 72 vs 96 dpi SVG, unitless DXF, open contours |
| Layers | Procreate (gestures), Affinity (layer states) | Global layers hidden until needed; focus mode; reference layer | Layer caps and paywalls; per-object alpha posing as layer opacity |
| Fill | Affinity Vector Flood Fill, Blender Grease Pencil | Gap closing with a preview; fill under the line; refuse unclosed regions | Leaks, results that depend on zoom, node soup, 1-px seams (Fresco) |
| Handwriting | MyScript (quality), GoodNotes, Windows InkAnalyzer | Lasso → convert, keep the ink, alternatives per line, search without converting | Destroying the ink, lost line breaks, auto-convert while drawing, cloud paywalls |
| Storage and undo | ISF (Windows ink), tldraw, Figma | Quantized delta-coded points, stable ids, fractional z-keys, delta undo | JSON points, snapshot undo, unbounded raster caches, non-atomic saves |

---

## 2. Recommendations by feature

### 2.1 The canvas and navigation

- **Tools are sized relative to the screen.** Brush size, smoothing distance, snap radius and hit tolerance are all screen points at drawing time, and are stored in the frame's local units. Zooming in gives a finer line, which is the ZoomArt experience.
  - Add a **wire** style that keeps a constant on-screen width at any zoom (Concepts), for diagram lines and construction lines.
- **Getting around.**
  - Arrows at the screen edge point to content off screen.
  - Bookmarks are named camera positions.
  - A "back to where I was" button.
  - A **breadcrumb of parent frames**, because a flat minimap is useless across a 10¹²× zoom range.
  - A search hit (text or recognized ink) flies the camera through the nested frames.
- **Undo moves the camera to the change.** Camera moves are never undo steps. The history is saved in the document.
- **Zoom-video export.** The camera runs at constant speed on a log-zoom path, rendered offscreen and encoded by the platform encoder. Whether RDE can encode video is still unchecked.
- **Pen and finger split.**
  - While the Pencil is active, a finger only navigates. This is reliable palm rejection.
  - Accidental two-finger undo while zooming is a top complaint, so the undo gesture must not fire during a pinch.

### 2.2 Brushes and strokes

- **Strokes stay editable.** Store the raw samples. Smoothing, brush, color and width are stroke properties that can be changed after drawing (Concepts, Linearity).
  - **Nudge** pushes a stroke like string.
  - **Sculpt** redraws over part of a stroke to fix it (Affinity).
- **Stabilizers.**
  - An exponential moving average, StreamLine style, is the default.
  - A pulled-string mode whose radius adapts to speed (Lazy Nezumi, Infinite Painter).
  - A "finish line" catch-up when the pen lifts (Krita).
  - Separate taper settings for pressure and for finger.
  - A fixed-width option for people who don't want pressure.
- **Textured brushes** are a rendering of the centerline, never geometry (Concepts' pixel nibs). Real painting (wet blending, smudge) would need raster layers, so it stays out of scope.

### 2.3 Bezier pen (Inkscape-style)

- **A tap-to-place pen in the BSpline style:**
  - tap places a node, drag pulls handles, and double-tap a node to switch between smooth and corner;
  - close the path by tapping its first node.
- **Freehand-to-curve fitting** with a screen-space error tolerance. The raw ink stays underneath, so the fit can be redone.
- **Node editing works by touch:**
  - large handles;
  - tap a segment to add a node, long-press a node to delete it;
  - no modifier keys needed. Modifiers are optional shortcuts on desktop.

### 2.4 Shapes

- **A shape palette:**
  - line, rectangle, circle, ellipse, polygon, arc and arrow;
  - each with **outline-only and filled variants**, chosen by a style toggle rather than separate tools;
  - shapes are stored as parameters (a rect is x, y, w, h, corner radius), not as points.
- **Hold to snap:**
  - draw roughly and hold: it becomes a clean shape;
  - keep dragging to scale and rotate, and a second finger adds 15° steps (Procreate);
  - recognize shapes of 1–4 strokes (Concepts);
  - the shape stays editable until you tap away, and the brush can change during that time (Infinite Painter);
  - the hold delay is adjustable, and Pencil Pro squeeze also triggers it;
  - undo goes back to the raw stroke.
- **Guides you draw along:**
  - line, ellipse and rectangle stencils; double-tap a guide to constrain to a half circle, 90°, a perfect circle or a square;
  - perspective and isometric grids.

### 2.5 Eraser: the decision

**Ship all three modes, with partial as the default.** All three exist in the best apps, and the reviews clearly punish having only one:

1. **Partial (default).**
   - It cuts the centerline where the eraser disc touches. A cut end gets the brush's normal round end, so it looks rubbed out, not chopped.
   - Pieces shorter than about 1 px at draw scale are dropped, so no slivers are left.
2. **Whole stroke (Kana's eraser).** Touching a stroke removes all of it. Best for notes and quick fixes.
3. **Trim to intersection (Clip Studio, Fresco).** It removes the touched part back to the nearest crossing strokes. Excellent for line art and for cleaning construction lines in woodworking drawings.

Behavior:
- Long-press the eraser icon to pick the mode.
- Pencil double-tap, squeeze and the eraser end of a stylus all toggle the eraser.
- "Return to the last tool when lifted" is on by default (GoodNotes).
- Lines and parametric shapes:
  - Partial or trim erasing on a line leaves line pieces.
  - On a rect or ellipse it turns the cut shape into a path.
  - Text, images and stickies are always erased whole.
- One eraser gesture is one undo step. Storage is covered in §3.5.

### 2.6 Lasso, selection and transforms

- **Lasso without changing tool:**
  - tap-and-hold with the pen starts a lasso from any tool (Concepts);
  - a lasso tool and a rectangle select also exist.
- **Filters:**
  - by type: strokes, shapes, text, images, connectors;
  - by color, layer and **visible size**. The visible-size filter matters a lot with deep zoom: it stops a lasso from picking up a million sub-pixel strokes in child frames.
- **The selection:**
  - **move, scale and rotate** handles, with snapped rotation steps;
  - a popup with recolor, rebrush, duplicate, move to layer, group, convert to text and convert to shapes.
- **Transforms are stored apart from geometry**, so moving 10,000 strokes changes 10,000 small transforms and never touches the point data.

### 2.7 Images

- **Import:** photos, files and paste. Each image gets move, scale and rotate, plus opacity, lock, crop and mask.
- **Stored by content hash:**
  - stored once even when used many times;
  - mip thumbnails saved, so a far zoom never decodes the full image.
- **Tracing:** an image can sit on a **reference layer** (§2.11) and be **calibrated from a known length**. For woodworking, that means photographing a part next to a ruler and tracing it at true size.
- **A floating reference window** shows an image that isn't on the canvas (Procreate, Infinite Painter).
- **Later:** auto-trace to vector with a live preview (Linearity).

### 2.8 Sticky notes and text

- **Sticky notes:**
  - typed with the keyboard;
  - **the font size stays fixed and the note grows vertically**, so text never shrinks to fit;
  - Tab, Cmd+Enter or a clone handle adds the next note next to it;
  - pen ink written on a note belongs to the note and moves with it;
  - lasso → convert turns that ink into the note's text.
- **Text boxes:** the same engine as stickies without the note background. They use the RDE text engine, so bidi works as in the language apps.

### 2.9 Diagrams

1. **Connectors bind by pausing, not with modifier keys.**
   - By default, an endpoint attaches to the whole shape: it aims at the center and ends on the edge.
   - **Pausing** on a port or an edge point pins it there. tldraw uses 600 ms and 320 ms.
   - The endpoint shows its state (a dot for pinned, a ring for floating).
   - A loose line never attaches by itself, and there is always a visible unbind.
2. **Ink becomes a diagram.**
   - A pen stroke from shape A to shape B becomes a bound connector.
   - Rough boxes, diamonds and circles become shapes.
   - Handwritten labels become text (§2.13).
   - Bindings survive the conversion, and undo always restores the raw ink.
3. **Quick-add handles.**
   - Tap a handle on a selected shape to clone it and connect it.
   - Drag a handle to place the new shape yourself, then pick its type.
   - If a shape already sits in that direction, the handle connects to it instead.
   - On desktop: Alt+Arrow adds, and Tab cycles the shape type.
4. **Routing.**
   - Straight, curved and orthogonal connectors. Orthogonal routes avoid obstacles.
   - **Once the user moves a segment, that segment is pinned**; double-tap releases it.
   - Line jumps at crossings; edges that share a side fan out.
   - Re-route only the connectors a change touches.
5. **Labels and heads.**
   - Three labels per connector (source, middle, target) that move with the line.
   - Full software arrowheads: UML triangle and diamonds, crow's-foot zero, one and many.
6. **Layout aids.**
   - Snapping is on by default: edges, centers, equal gaps.
   - **Tidy up** for a messy selection (FigJam).
   - Align and distribute.
   - Auto-layout of a selection (layered, tree, orthogonal). It is animated and **stable**: adding a node never reshuffles everything, and pinned nodes stay put.
7. **Containers.**
   - Frames clip their contents and carry them along.
   - Swimlanes and groups fit their contents, and resizing a lane pushes its contents.
   - Dropping a shape onto a connector splits it.
8. **Software shapes are built in and free.**
   - UML classes with compartments, ER tables, sequence diagrams, C4, flowchart, BPMN basics.
   - User libraries.
9. **Mermaid import.** Pasting Mermaid creates native shapes inside a container that keeps the source, so the diagram can be regenerated without losing style overrides. D2 and PlantUML can follow.
10. **One diagram, two looks.** A document-level sketch/clean render style. The sketch randomness is stable per element, so the drawing doesn't jitter.

### 2.10 Precision and woodworking

1. **Our own numpad, and type at any time.**
   - While drawing a line, typing (or tapping the live length label) sets the length; Tab moves to the angle.
   - Keys for fractions (`1-3/8"`), units (mm, cm, m, in, ft), arithmetic (`450-2*18`), and **named variables** (`stock`, `kerf`, `bit`) in every numeric field.
   - **Never the system keyboard**, which covers the drawing and has no fraction keys.
2. **Real instruments:**
   - **ruler**, **45° set square (escuadra)**, **30/60° set square (cartabón)**, protractor and compass;
   - all scale-aware, with soft detents that only catch slow movement;
   - a tappable angle readout;
   - the pen snaps to the edge, and the stroke stays on the edge.
3. **Instruments that slide along each other.** Dock the cartabón against the escuadra and slide it, exactly as on a drawing board, to draw parallels and perpendiculars. **No app offers this.** It is our signature woodworking tool.
4. **A measuring tape.**
   - Point to point, path length, the angle between two lines, radius.
   - **Measure → pin → dimension**: one tap turns a measurement into a permanent dimension.
5. **Driving dimensions.** Edit a dimension's number and the geometry follows. This is constraint-lite, with no Fusion-style blue and black states and no solver.
6. **Snapping you can predict:**
   - a visible target before committing, using Pencil hover where available;
   - a radius in screen space and a strict priority order (endpoint > intersection > midpoint > edge > grid);
   - only nearby or recent candidates;
   - holding a finger down bypasses snapping, with a Pencil Pro haptic tick on snap;
   - **no length snapping**, because silent rounding breaks trust.
7. **Honest units.**
   - Display precision is set per document.
   - **A rounded value shows `≈`.**
   - Typed values are stored exactly (§3.1).
8. **Export for workshop**, a sheet with three modes:
   - **Tiled print:**
     - A4 or Letter pages, 10–15 mm overlap, registration crosses;
     - page labels ("B3") and a mini-map on every page;
     - a 100 mm test square on every page;
     - **PDF pages exactly the paper size**, so iOS has nothing to scale to fit;
     - a one-time "print, measure, enter" wizard that saves an **X/Y calibration per printer**.
   - **Full-size PDF** for a plotter or print shop.
   - **CNC or laser:** SVG in mm and DXF with INSUNITS set, layers by operation (cut, score, engrave, drill), and a pre-flight check for open paths, duplicates and tiny segments.
9. **Woodworking helpers:**
   - an **offset/kerf tool** with presets (kerf shading on the waste side, bushing offset, inside/outside);
   - **board objects** with real size, grain arrow and label, which lead to a cut list later;
   - **joint stamps** with parameters: finger and box joints with kerf compensation, dovetails by ratio, 32 mm hole rows;
   - a **true-size screen view**, so 100 mm on screen is 100 mm and you can hold a part against the iPad.

### 2.11 Layers

- **Layers belong to the document, not to a region**, and apply inside every nested frame. "Hide dimensions" hides dimensions at every depth.
  - Within a frame, draw order is (layer order, z).
  - A child frame draws in its parent's slot.
- **Hidden until needed.** A document starts with one implicit layer and no panel showing (the whiteboard lesson).
  - Don't copy Concepts' automatic layer-per-tool mode, because it silently moves where the ink goes.
- **The panel:**
  - each row shows a color tag, name, object count (more useful than thumbnails on an infinite canvas), eye, lock and a drag handle;
  - drag a selection onto a row to move it there;
  - double-tap a row for **focus mode**, which dims everything else.
- **Named layer views** (Affinity's layer states): "cut sheet" vs "assembly", or presentation steps.
- **Reference layer:** locked and unselectable by default, with its own opacity, left out of export, and usable as the fill boundary.
- **Opacity is true group opacity**, composited offscreen only when it is below 1. **No blend modes in v1.**
- A hidden layer is out of hit-testing, snapping, lasso, fill boundaries, the handwriting index and export.
- **Export:** layers become SVG layers (Inkscape groups) and DXF layers. This is what lets woodworkers separate cut, engrave and dimensions.
- **Vector layers cost almost nothing**, so there is never a cap.

### 2.12 Fill (toolbar bucket)

Concepts, Linearity and Xournal++ have no paint bucket at all. The users who have one complain about leaks through gaps, results that depend on zoom, jagged node soup, and hairline seams.

- **Cheap modes first:**
  - tapping a closed shape or closed stroke sets its fill color, with no new geometry;
  - a lasso-fill brush for loose coloring (Concepts).
- **Real bucket fill on freehand line art:**
  1. Find the boundary objects around the tap.
  2. Union their outlines plus short gap-closer segments.
  3. The fill is the hole that contains the tap. Islands inside it become holes.
  4. Simplify, then fit curves.
  - The work is done in the frame's local units, never in screen pixels, so the result doesn't depend on zoom.
  - **Gap closing:** the tolerance defaults to about 8 screen px at tap time. It is converted to local units and **stored**, so a recompute gives the same result. While the pen is held, the closing lines show as a preview, and a vertical drag adjusts the tolerance. Lift to commit.
  - **A region that isn't closed is refused**: the app flashes "region not closed" and highlights the open endpoints. It never floods to infinity.
  - **The fill goes under the strokes** and reaches each stroke's centerline, so there is no seam.
  - **Semi-live:** the fill keeps its tap point, boundary ids and tolerance. If a boundary stroke is edited, it recomputes in the background. If the result would leak, it keeps the old shape and shows "Refill?".
  - **Boundary set:** visible, unlocked layers by default, or "reference layer only", so line art on one layer drives color on another.
- **This needs robust polygon booleans**, which is a dependency decision (§3.7).

### 2.13 Handwriting to text

- **Lasso → "Convert to text":**
  - an editable text box at the ink's position, with its size taken from the measured line height;
  - **the ink is kept** inside the text object, with "Revert to ink";
  - a sheet of alternatives for each line;
  - never auto-convert the normal pen. A separate "text pen" can come later (OneNote style).
- **We do the line segmentation.** ML Kit assumes one line per request and gives no mapping from strokes to words, so:
  - rotate the strokes to the baseline;
  - cluster them into lines;
  - send each line with a one-line WritingArea and the previous line's last 20 characters as pre-context;
  - rejoin the lines with newlines, which keeps the line breaks users complain about losing.
- **Diagram-aware:**
  - ink inside a shape or sticky becomes that container's label;
  - ink near a connector's middle becomes the connector's label;
  - long or closed strokes are treated as drawing and skipped.
- **Search without converting.** A background index per ink line stores the top candidates, the bounds and the frame path, so a hit flies the camera there.
- **Dimensions from handwriting.** Recognized text like `450 mm`, `3/4"` or `1' 6"` offers "make this a dimension".
- **Engines, behind one `recognize(line, language, context)` call:**
  - **iOS and Android:** ML Kit Digital Ink. Kana already has it (fude/study/services/mlkit.h). Models are about 20 MB per language and download on demand, so there needs to be a download UI.
  - **macOS (and optionally iPadOS):** Apple's `PKStrokeRecognizer`, reported for iOS/macOS 27. It is free, runs on device and works with custom canvases, but **it is unverified**.
  - **Windows:** InkAnalyzer.
  - **Linux:** no engine, so the feature is hidden.
  - MyScript has the best quality, but its pricing is per device and by quote. Only worth it if math or full diagram parsing becomes core.

---

## 3. Architecture: data, storage and undo

Everything here builds on the agreed nested-frame design and reuses Kana's pieces:
- the stroke model;
- `rde_rendering_2d_draw_stroke` with screen-space points computed on the CPU;
- the undo action log;
- the tagged chunk file format;
- `rde_arr` on the std allocator.

As the plan says: **build the frame system first**, before any tool.

### 3.1 Frames and coordinates

- **Frames.** Each frame has its own local coordinate system, about 1000× smaller than its parent's, and an offset inside its parent. Objects live in the frame that was current when they were made, in local f32, so every stored number stays small at any depth.
  - The camera is (frame, local offset, local zoom). It re-anchors to a child or the parent when it crosses a zoom threshold.
  - Only frames within a few levels of the camera are drawn.
- **Physical scale (new, for woodworking).** A frame can declare a real unit: "1 local unit = 1 mm". Rulers, the measuring tape, dimensions, true-size view and exports then work in real units at any zoom.
  - A template is a frame with a scale.
  - A zoomed-in detail of a joint is a child frame with the same physical scale and more precision.
- **Exact values.** The numbers a user types (lengths, angles, dimensions, variables) are stored as **f64 parameters** on the object. The f32 geometry is derived from them for display. Exports compute from the f64 values, so a typed `9.525 mm` (3/8") comes out exactly.
  - f32 alone would be fine for display: a 1 m board has a step of about 0.00006 mm. f32 is only wrong for the "typed it, must get it back" promise.

### 3.2 Object model

- **Each frame holds a flat table of objects.** Every object has:
  - a **stable 64-bit id** (device id + counter);
  - a type, a **transform kept apart from its geometry** (f32 translation, rotation, uniform scale), a parent (frame or group), a **fractional z-key**, a layer id, a style id, and a reference to its payload.
- **Types:**
  - stroke, shape (parametric), path (cubic Béziers), text, sticky, image (content hash), group, connector, dimension, fill;
  - instrument positions are view state, not document objects.
- **Bindings are separate records**: {connector, end, target id, normalized anchor, pinned}. Routes, tessellations and fills are **derived**. They are recomputed and never saved, apart from the fill's closed path, which is saved together with its provenance (§2.12).
- **A fractional z-key** lets "bring to front" or an insert between two objects change one record, not renumber the frame.

### 3.3 Stroke encoding: about 1.5 bytes per point

- **A header per stroke:**
  - origin (f32, local);
  - `q_exp` (int8): quantum = 2^q_exp local units, chosen near **1/16 screen pt at the drawing zoom**;
  - style index, point count and a codec byte.
- **Channels stored separately:** x, y, pressure (8–10 bit), and optionally tilt (8+8 bit) and time (ms). Each channel is integer delta-of-delta, zigzag, adaptive Rice coding in blocks of 32.
- **Size:**
  - about **1.36 B/pt for x, y and pressure** *(sim)*, and 2–2.5 B/pt with tilt and time;
  - for comparison, raw f32 costs 12 B/pt and tldraw about 6.
  - Exact integers mean no drift, and the per-stroke quantum stays right at any frame depth.
- **Raw input is stored as is.** Smoothing and curve fitting happen at render time, which is what lets strokes stay editable.
- **Long strokes are cut at about 512 points** into linked pieces. This keeps bounding boxes tight and saves small.
- **In RAM:**
  - strokes off screen stay encoded;
  - visible strokes decode into SoA f32 x/y plus u16 pressure (about 10 B/pt), inside an LRU budget.
  - A million points take about 1.5 MB encoded and about 10 MB decoded.

### 3.4 Spatial index and memory

- **The frame tree is the coarse index across scales.** Inside each frame:
  - an **R-tree** over local AABBs;
  - bulk-loaded when the frame loads, updated on edits, rebuilt lazily;
  - each tree stays small because the frames are nested.
- **Far frames draw from cached impostor textures.** These are saved in the file as thumbnails, so opening a document is instant.
- **Raster caches are what get apps killed, not vectors.**
  - Budget all caches in **bytes**: roughly 256–512 MB on iPad, less on Android.
  - Evict the least recently used, and purge on memory warnings (`os_proc_available_memory`, `onTrimMemory`).
  - InfiniPaint's worst case is about 1.6 GB, and a single full-screen 13" iPad buffer is about 23 MB.

### 3.5 Undo

- **Transactions of property-level deltas** {frame, object, field, before, after}, grouped by marks (tldraw). This extends Kana's action log (dead flags + tail truncation), which already works for strokes.
- **Big payloads are never copied.** Point blobs and images are immutable and reference-counted, so an undo entry holds a handle.
- **One gesture is one step.** An eraser pass, a lasso move or a fill each make one entry.
  - A partial erase records the original stroke id plus the erased intervals.
  - The surviving pieces are regenerated from those, so the entry is a few bytes, not a copy of the pieces.
- **Derived data is never recorded.** Routes, tessellations and caches are recomputed on undo.
- **Bounded by bytes**, about 32–64 MB, not by step count. **History is saved in the document.**
- **The undo log doubles as the autosave journal** (§3.6), so every stroke is on disk within about a second.

### 3.6 File format: the decision

The storage report suggests **SQLite**. It gives atomic, incremental, lazy-loaded saves for free, at about zip size. It is plain C, but it is a new dependency, and your rule is C plus RDE unless the effort would be huge.

**My recommendation: extend Kana's tagged chunk format into a log-structured file, not SQLite.** It is a few hundred lines on top of code we already have, and it gives the same three properties:

- **Layout:**
  - a header with magic and format version, readable before anything else;
  - then chunks: FRAME, OBJECTS (a frame's objects in buckets of about 512), ASSET, STYLE, THUMB, JOURNAL;
  - then an INDEX chunk and a fixed footer {magic, index offset, checksum}.
- **Saving appends, never rewrites:**
  - **Every second or so**, append a JOURNAL chunk with the new undo transactions. That is cheap and makes every stroke durable.
  - **Every so often** (dirty bytes or time), append the dirty OBJECTS buckets as a checkpoint, then a new INDEX and footer, then fsync.
  - The old chunks become garbage.
- **Crash safety:** opening reads the last valid footer. If the tail is torn, it scans back to the previous good one and replays any JOURNAL chunks after it.
- **Lazy loading:** the INDEX maps frame id → chunk offsets, bounds and thumbnail. Opening loads only the frames near the camera.
- **Compaction:** when garbage passes about 50%, or on export or share, write a compacted copy to a temp file, fsync it, then rename it over the original.
- **Cloud folders:** never edit a live file in a synced folder. Edit a local copy and publish atomically.
- **No general compressor:** points are already about 1.5 B, other records are small, and images arrive compressed. That avoids a zstd dependency.

When to switch to SQLite instead: if we add multi-device sync or collaboration, or if documents grow so large that random in-place updates matter. Neither is planned.

### 3.7 Dependencies to decide

| Need | Recommendation | Why |
|---|---|---|
| File format | Our own (§3.6) | Small effort, reuses Kana's chunk code |
| Polygon booleans and offsets (fill, kerf offset, shape union and subtract for templates) | **Clipper2** (Boost licence, used through its official C interface) | Robust polygon clipping and offsetting in C would be the "huge effort" case. One library serves three features. It is C++ inside, so the build must compile its few .cpp files on every platform. |
| Handwriting | Platform engines (§2.13) | Already have ML Kit; the others are OS APIs |
| Auto-layout | Our own simple layered and tree layout first | ELK is Java and the alternatives are big. Our needs are small: stable layered, tree, tidy-up |
| Mermaid import | Our own parser for flowchart, class, sequence and ER | The subset is small; the official renderer is JavaScript |
| Video export | Platform encoders (AVFoundation, MediaCodec) | Check first whether RDE has anything |

If you'd rather not take Clipper2:
- Fill can be done in pure C by flooding a raster in the frame's local units (not screen pixels), then tracing it. That is moderate work, not exact, and fine for coloring.
- The kerf/offset tool and shape booleans would wait.

---

## 4. Suggested phases

Each phase ends with something usable and a release build.

**Phase 0: the core, before any tool.**
- Frames, re-anchoring camera, R-tree per frame.
- Object table, stroke codec, delta undo, log-structured file with journal.
- Zoom-relative pen, the eraser modes, layer id on every object (no panel).
- **Benchmark gate:** 1 million points and 10k objects stay smooth on the Galaxy Tab and the iPad, and a forced crash loses at most a second of work.

**Phase 1: sketching (use 1).**
- Brushes and stabilizers, editable strokes.
- Shapes with hold-to-snap, plus the shape palette (outline and filled).
- Lasso with filters, transforms, images, guides.
- Navigation (edge arrows, bookmarks, breadcrumb), undo that moves the camera.
- Tap-to-fill closed shapes.
- PNG, SVG and PDF export of a frame.

**Phase 2: precision and woodworking (use 3).** This phase is the differentiator. It also builds the snapping and numeric entry that diagrams reuse.
- Numpad entry, units, variables.
- Frames with a physical scale.
- Instruments, including the sliding escuadra and cartabón; measuring tape; dimensions; driving dimensions.
- Bezier pen and node editing.
- Tiled print with calibration; SVG and DXF for CNC; true-size view.

**Phase 3: diagrams (use 2).**
- Connectors with pause-binding, routing, labels and heads.
- Sticky notes and text boxes; quick-add handles; containers.
- Software shape libraries; snapping, Tidy up, align and distribute.
- The layers panel and layer views.

**Phase 4: intelligence and polish.**
- Bucket fill with gap closing.
- Handwriting to text and the search index; ink → shapes and connectors.
- Mermaid import, auto-layout, sketch/clean render style.
- Offset/kerf, board objects and joint stamps.
- Zoom-video export, Sculpt and Nudge.

---

## 5. The pitfalls that matter most

1. **Losing work.** Concepts crashes that wipe changes, and Endless Paper boards that roll back weeks, destroy trust. → Journal every stroke, save atomically, never rewrite the whole file.
2. **Slowing down as the canvas fills.** Freeform has 2–3 s of ink lag, and Excalidraw freezes past about 1000 strokes. → Design for millions of points from day one, with the Phase 0 benchmark gate.
3. **Silent inexactness.** A template 3% small is worse than none. → `≈` on rounded values, no length snapping, exact typed values, calibrated printing, unit-correct exports.
4. **Tools that override the user.** Smart connectors re-routing shaped lines, snaps you can't escape, auto-convert firing mid-drawing. → Every automatic action is visible, can be overridden, and undoes to the raw input.
5. **Designing for mouse and keyboard.** Hover, Ctrl, Cmd and Alt don't exist on a pen. → Use pause, handles, on-screen toggles and our own numpad. A keyboard is only an accelerator.
6. **Thin tools around a great zoom.** Users notice within minutes. → Don't ship the zoom without the Phase 1 tools.
7. **Unbounded caches.** Raster caches, not vectors, get apps killed. → Budget in bytes and degrade gracefully.
8. **Lock-in and hostile monetisation.** → Open exports that round-trip; never paywall layers, export or libraries.

---

## 6. Decisions (Borja, 2026-10-04)

1. **Clipper2: yes.** It is used for fill, kerf/offset and shape booleans, through its C interface.
2. **File format: extend Kana's tagged chunk format** into the log-structured file in §3.6, because it is easier to understand, maintain and extend than adding SQLite.
3. **Phase order as in §4:** core → sketching → precision and woodworking → diagrams → intelligence and polish.
4. **Eraser: all three modes, partial by default.**
5. **A new app.** apps/draw stays the simple endless page.
6. **Handwriting:** hidden on Linux; the Mac engine is unverified until we test it.
