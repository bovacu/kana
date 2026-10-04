# Layers, Fill, and Handwriting-to-Text on an Infinite Vector Canvas

Research date: October 2026. Scope: three features for a tablet-first (Pencil + touch, also desktop) infinite, deep-zoom vector canvas written in C, used for sketching, software diagrams and woodworking templates.

**Top-line findings**

- **Layers**: in a vector app a layer costs almost nothing. Procreate's layer cap comes from raster memory, and we do not have that problem. The whiteboard tools held out against layers for years (Excalidraw, tldraw, FigJam), but users kept asking for them, mostly to **show and hide sets of content** (presentation steps, annotations, construction stages). Miro finally shipped layers in Oct 2024. Make layers document-global, keep them out of sight until someone needs them, and use them for visibility and lock, tracing underlays and export.
- **Fill**: Concepts has **no** paint bucket. It only has a "Filled Stroke" lasso-fill. Linearity Curve and Xournal++ don't have one either. The working approaches are raster-flood-then-trace (Inkscape, Blender Grease Pencil, probably Fresco), boolean region-building (Affinity Vector Flood Fill), and a live planar map (Illustrator Live Paint). Users complain most about **leaks through gaps, results that depend on zoom, jaggy or overly dense nodes, hairline gaps between the fill and the line, and fills that don't update**.
- **Handwriting**: MyScript (Nebo, now "MyScript Notes") is still the quality benchmark. GoodNotes moved to its own on-device model. ML Kit is good, but it **assumes one line per request**, returns **text and score only** (no mapping back to strokes), and runs **only on Android and iOS**. Line and word segmentation, diagram awareness and placing the text are our job. iOS/macOS 27 adds a free on-device `PKStrokeRecognizer` that works with custom canvases.

---

## 1. Layers

### How the reference apps do it

**Procreate (raster).** Layer count is capped by canvas size × device RAM. Procreate itself says "maximum canvas size and layer limits are dependent on the iPad model" ([Procreate on X](https://x.com/Procreate/status/1637679823429545985)). One third-party measurement found about 204 layers on a 3000×3000 canvas with 16 GB of RAM ([Art Side of Life](https://artsideoflife.com/procreate-layer-limit/)). The panel UX is the best on tablets and worth copying:

- swipe right to multi-select
- pinch to merge
- two-finger tap, then drag on the canvas, for opacity
- two-finger swipe for alpha lock ([Procreate gestures](https://help.procreate.com/procreate/handbook/5.0/interface-gestures/gestures))

A **Reference** layer lets ColorDrop fill on any layer "as though [fills] are flowing into the linework on the Reference Layer" ([Procreate layer options](https://help.procreate.com/procreate/handbook/layers/layers-options)).

**Concepts (vector, infinite canvas).** This is our closest analogue. Layers are global to the drawing ([Concepts manual](https://concepts.app/en/manual/layers)):

- 5 free, unlimited with Pro
- visibility, lock, opacity slider, drag to reorder, Merge Down
- move content by dragging a selection onto a layer
- **Focus Mode**: double-tap a layer and everything else is subdued

The default **Automatic** mode creates and switches layers by tool ("will add a new layer when it recognizes a new tool being used"). Changing order or the active layer silently flips you to Manual. Complaints ([Parka Blogs review](https://www.parkablogs.com/picture/concepts-app-review-sketching-vector-and-infinite-canvas)):

- no blend modes
- layer opacity is applied **per object**, so overlapping strokes on a 50% layer double-darken
- there is no z-order within a layer, so people use layers as a z-order workaround

**Affinity Designer.** Every object is a layer, so the panel becomes an object tree and there is no merge ([Logos By Nick](https://logosbynick.com/affinity-designer-layers/)). The standout idea is **Layer States** (v2.4): captured visibility snapshots, plus *queries* by tag color, type, name or lock status ([Affinity help](https://affinity.help/publisher2/en-US.lproj/pages/ObjectControl/layerStates.html)). Version 2.4 also added DWG/DXF support ([AlternativeTo](https://alternativeto.net/news/2024/2/affinity-2-4-adds-layer-states-dwg-dxf-support-and-more-to-designer-publisher-and-photo)), which matters for CAD and woodworking export.

**Illustrator / Inkscape.** A layer is just a top-level group. In Inkscape the only difference is `inkscape:groupmode="layer"` on an SVG `<g>` ([Inkscape forum](https://alpha.inkscape.org/vectors/www.inkscapeforum.com/viewtopic468c.html?t=10604)). Illustrator iPad has long-standing bugs where layers can't be selected or moved ([UserVoice](https://illustrator.uservoice.com/forums/931885-illustrator-ipad-bugs/suggestions/42416677-can-t-select-certain-layers)).

**Adobe Fresco.** Separate pixel and vector layers, no limit. Merging even two *empty* vector layers produces a pixel layer, a recurring complaint ([Adobe community](https://community.adobe.com/t5/fresco-discussions/merging-two-vector-layers-turns-them-into-a-pixel-layer-even-if-both-layers-are-empty/td-p/15410087)).

**Linearity Curve.** Layers sit under artboards. You reorder by drag and drop, drag selections between layers, groups and artboards, and on iPad multi-select with a two-finger drag ([Curve docs](https://www.linearity.io/academy/curve/ipad/user-guide/workspace/organize-layers/)).

**Infinite Painter / Krita.** Infinite Painter's layers are limited by memory ([docs](https://docs.infinitestudio.art/painter/layers/)). Krita's vector layers are general-purpose SVG layers, "not made for line art" ([Virtual Curiosities](https://www.virtualcuriosities.com/articles/1464/how-vector-layers-in-krita-are-different-from-clip-studio-paint)).

**Whiteboards (why they avoided layers).**

- **Excalidraw** maintainers: layers "doesn't fit into the UI simplicity principle" ([#2170](https://github.com/excalidraw/excalidraw/issues/2170)). Excalidraw shipped **frames** instead in June 2023, for organizing content, constraining exports and building presentations ([Excalidraw on X](https://x.com/excalidraw/status/1669023307738357770)). Users keep reopening the request, almost always for **step-by-step reveal, annotation overlays, and construction stages or materials** ([#6266](https://github.com/excalidraw/excalidraw/issues/6266), [#7725](https://github.com/excalidraw/excalidraw/issues/7725)). One user notes that faking layers with lock still leaves the elements selectable. A community PR adds create, rename, reorder, eye toggle and "Move to Layer".
- **tldraw** closed its layers request as not planned ([#1739](https://github.com/tldraw/tldraw/issues/1739)). It ships a *sample* panel that shows the shape, group and frame tree with visibility stored in shape metadata ([tldraw example](https://tldraw.dev/examples/layer-panel)).
- **FigJam** has no layers panel at all, only object lock and section lock ("Lock background only") ([Figma help](https://help.figma.com/hc/en-us/articles/4939765379351-Organize-your-FigJam-board-with-sections)).
- **Miro** gave in. The idea, open since March 2020 with 814 upvotes, was delivered in Oct 2024, pitched as "multi-dimensional views" of one board ([Miro community](https://community.miro.com/ideas/layers-93)). Users immediately asked for per-layer access rights ([thread](https://community.miro.com/ask-the-community-45/layers-17212)).

**Note apps.** GoodNotes 6 added Arrange (send to back or front). Layers are experimental, **Pro only, up to 5 layers** ([GoodNotes feedback](https://feedback.goodnotes.com/forums/191274-customer-suggestions-for-goodnotes-apple/suggestions/43325880-give-writing-a-layer-so-that-we-can-move-it-to-any), per GoodNotes' FAQ). Notability has no real layers.

### What this tells us

1. **There are two different jobs.** (a) z-order and organization: groups, arrange, frames. (b) **Visibility and lock sets**: views, overlays, underlays. Diagram users want (b). Artists want (b) plus tracing underlays. Woodworkers want (b) plus **export separation**: cut, engrave, dimensions, reference.
2. **Vector layers are cheap**, so there is no reason for a cap. The real cost is **true group opacity and blend modes**, which need offscreen compositing. Concepts probably avoided that, and users noticed.
3. **Infinite canvas and deep zoom.** Every infinite-canvas product with layers (Concepts, Miro) makes them **global to the document**, not tied to a region.

### Recommendations for our app

- **Data model.** `Layer { id, name, color_tag, visible, locked, opacity, export_flag, kind: normal|reference }` lives at the document level. Every object, including frames, stickies, connectors and images, carries a `layer_id`. Groups and frames are a separate, orthogonal hierarchy.
- **Deep zoom rule.** Layers are global tags that apply inside every nested frame. Within any one frame, draw order is `(layer order, z-index)`, and a nested frame draws in its parent's slot. "Hide Dimensions" then hides dimensions at every zoom depth. Hidden layers are also removed from hit-testing, snapping, lasso, fill boundaries, handwriting indexing and export.
- **Hide the feature until needed.** Start with one implicit layer and no visible panel (the whiteboard lesson). Do **not** copy Concepts' Automatic-by-tool mode, because it silently changes where ink goes.
- **Tablet panel.** A slide-over list. Each row has a color tag, name, object count (more useful than thumbnails on an infinite canvas), eye, lock and a drag handle. Tap a row to make it active. Copy Procreate's gestures for multi-select, merge and opacity. To move content, **drag a selection onto a layer row**, or use a "Move to layer ▸" item in the selection menu. Double-tap a row for **Focus Mode** (dim everything else).
- **Views / layer states.** Named visibility snapshots, like Affinity's, cover the presentation use case that drove Excalidraw and Miro demand. For woodworking, a view can be "cut sheet" versus "assembly".
- **Tracing.** A `reference` layer kind for imported photos and scans:
  - locked and unselectable by default
  - its own opacity
  - excluded from export
  - usable as a boundary for fill (see §2)
  - pairs with a "calibrate scale" step for woodworking templates
- **Opacity.** Implement it as true group compositing: an offscreen buffer per visible tile per layer, only when opacity < 1. Never use per-object alpha. **Blend modes: skip in v1.** If artists ask, add Multiply only, since shading is the case people actually cite.
- **Export.** Map layers to SVG `<g inkscape:groupmode="layer">` and to DXF layers.
- **Pitfalls.**
  - Drawing onto a hidden or locked active layer: refuse and flash the layer row.
  - Lasso grabbing hidden content.
  - Merge operations that change object types.
  - Per-object opacity pretending to be layer opacity.
  - Layer state that is ambiguous between per-user and per-document if you ever add collaboration (Miro users asked for permissions straight away).

---

## 2. Fill / paint bucket in a vector app

### How the reference apps do it

**Procreate ColorDrop (raster).** A flood fill whose threshold you set by dragging. Help docs admit "any gaps may result in the ColorDrop fill spilling through" ([Procreate help](https://help.procreate.com/articles/dxxave-color-flood-using-colordrop)). The Reference layer separates line art from color.

**Concepts.** Correction to the brief: Concepts has **no bucket fill**. "While Concepts does not have an automatic 'paint bucket' fill…" ([Concepts help](https://tophatch.helpshift.com/hc/en/3-concepts/faq/171-how-do-you-fill-a-region-with-a-color/)). What it has:

- a **Filled Stroke** tool that fills the "positive space" your pen motion encloses ([tutorial](https://concepts.app/en/tutorials/drawing-shapes-instead-lines-8-exercises-filled-stroke/))
- converting a *single closed stroke* into a fill

Reviewers flag this as a productivity hit ([Parka Blogs](https://www.parkablogs.com/picture/concepts-app-review-sketching-vector-and-infinite-canvas)).

**Linearity Curve** has no bucket; the forum request is "not on the roadmap" ([Linearity forum](https://forum.linearity.io/t/fill-in-bucket-tool/3265)). **Xournal++** maintainers say it "would be very complicated to implement" ([#3489](https://github.com/xournalpp/xournalpp/issues/3489)).

**Illustrator Live Paint** is the gold standard and the most complex. It is built on a **planar map**: paths stay untouched, and on every edit heuristics re-match regions and edges and carry fills over ([Asente, Schuster, Pettit, SIGGRAPH 2007](https://www.lri.fr/~mbl/FundHCI/papers/Asente-SIGGRAPH07.pdf)). Gap detection offers Small, Medium, Large or Custom, treats a sub-threshold gap "as if the end points were connected by a straight line", and can "Close Gaps with Paths" ([Illustrator How](https://illustratorhow.com/live-paint-bucket/)). Users have asked Adobe to bring it to Illustrator iPad ([UserVoice](https://illustrator.uservoice.com/forums/931888-illustrator-ipad-feature-requests/suggestions/41680873-live-paint-bucket-please)).

**Affinity Designer Vector Flood Fill (2.1).**

- You select curves first, then tap a region and get a **new shape**. In effect it is a macro for boolean divide ([Affinity forum](https://forum.affinity.serif.com/index.php?/topic/180854-vector-flood-fill/)).
- Serif on gaps: "A gap tolerance is on our wish list… unlikely to be available in its first release."
- It is not live. Users contrast it with Adobe's live update ([VectorStyler forum](https://vectorstyler.com/forum/topic/2935/vector-flood-fill-tool-in-affinity-designer-beta)).

**Adobe Fresco.** Fill on a vector layer produces vector shapes. The gap-closing "color margin" is **disabled for vector brushes**, and adjacent vector fills show a 1-px seam that Adobe calls an antialiasing limitation and only offers workarounds for ([Adobe community](https://community.adobe.com/questions-646/fill-with-vector-308062), [bug thread](https://community.adobe.com/questions-646/vector-layers-fill-bugs-305670)).

**Inkscape Paint Bucket.** It rasterizes the visible area **at the current zoom**, flood-fills, and traces the result to a new path. Options are Threshold, Grow/Shrink and Close gaps (None, or Small/Medium/Large = 2/4/6 **screen** pixels). The documented workflow is a confession: zoom in when the fill is too rough, zoom out when it leaks, because "different zoom levels will give different results" ([Tavmjong Bah's manual](http://tavmjong.free.fr/INKSCAPE/MANUAL/html/Bucket-Gap.html), [wiki tutorial](https://wiki.inkscape.org/wiki/Scratchpad_paintbuckettutorial)). Inkscape's own wiki lists the problems ([A better Bucket Fill](https://wiki.inkscape.org/wiki/A_better_Bucket_Fill_tool_fill)):

- nodes are "too dense and/or are not corners when needed"
- it "always leaves small pixel areas unfilled"
- high thresholds freeze it

**Blender Grease Pencil** is the closest analogue: vector strokes, a fill tool that renders to a bitmap, floods, and vectorizes the result into a new filled stroke. Its options are a good checklist for us ([Blender manual](https://docs.blender.org/manual/en/4.2/grease_pencil/modes/draw/tools/fill.html)):

- **Precision** multiplier and **Dilate/Contract**
- **Simplify**
- **Layers** used as boundary: visible, active, above or below
- **Gap closure**, either *Radius* (connect nearby open endpoints) or *Extend* (extend stroke ends), with on-screen visual aids, collision checks, and a length you adjust before committing
- invisible **boundary strokes** you draw by hand

Dave Pagurek added endpoint-to-endpoint connections and extensions at sharp curvature, and argues that gap closing in **vector space** beats bitmap tricks ([blog](https://www.davepagurek.com/blog/blender-flood-fill/)).

**Krita 5.3** added **Close Gap** in pixels, alongside Spread and an adaptive Grow ([Krita docs](https://docs.krita.org/en/reference_manual/tools/fill.html)). Academic work estimates gap sizes automatically, without a threshold, by clustering dangling endpoints with MSTs ([Visual Computer 2021](https://dl.acm.org/doi/10.1007/s00371-021-02235-x)).

### Technical approaches compared

| Approach | Pros | Cons on an infinite / deep-zoom canvas |
|---|---|---|
| Raster flood → trace (Inkscape, Blender) | Robust to messy, overlapping pressure strokes; simple | Accuracy depends on resolution; Inkscape ties it to screen zoom; jaggies and node soup; needs curve fitting; gaps measured in pixels |
| Polygon booleans (Affinity-like) | Exact and resolution-independent; handles holes and islands | Freehand outlines are dense; needs robust integer clipping; needs explicit gap segments |
| Planar map, live (Live Paint) | Fill follows edits | Much more engineering: arrangement maintenance plus re-matching heuristics |

### Recommendations for our app

- **Algorithm: polygon booleans in the local frame, not raster.** We already compute stroke outline polygons for rendering.
  1. Query a spatial index for boundary objects around the seed, growing the search box step by step.
  2. **Union** their outlines plus thin "gap-closer" segments.
  3. The fill region is the **hole that contains the seed**. Islands inside it (an "O", a hole in a jig) become holes in the fill.
  4. Simplify (RDP) and fit Béziers.

  Clipper2 has a C ABI (`clipper.export.h`, int64 coordinates) ([docs](http://www.angusj.com/clipper2/Docs/Units/Clipper.Export/_Body.htm)). Working in int64 local-frame coordinates avoids both Inkscape's zoom dependence and float trouble at deep zoom. That matters for woodworking templates.
- **Refuse unbounded fills.** If the seed is not inside any hole, or the search box exceeds a budget, flash "region not closed" and highlight the nearest open endpoints. Never flood to infinity.
- **Gap closing, Blender style, in world units.** Connect endpoint↔endpoint and endpoint↔stroke within a tolerance, with optional tangent extension. The tolerance defaults to about 8 *screen* px at tap time, converted to local units and **stored**, so a recompute is deterministic. Show the closer lines as a preview while the pen is held, and let a vertical drag adjust the tolerance (Procreate's threshold-drag idiom). Lift to commit.
- **No seams.** Place the fill **under** the boundary strokes and extend it to each stroke's centerline (or grow by half the width). This is the fix for Fresco's 1-px seam.
- **Boundary set.** Default to visible, unlocked layers. Add an option for "Reference layer only" (Procreate) or "active layer" (Blender), so line art on one layer drives color on another. Ignore images and text as boundaries by default.
- **Semi-live fills.** Save the fill as an ordinary closed path, plus provenance: `{seed in local coords, boundary object ids, tolerance}`. When a boundary object changes, recompute in the background. If the new region leaks or changes area beyond a set fraction, keep the old shape and badge it with "Refill?". That gets most of Live Paint's benefit without a planar map.
- **Also ship the cheap modes.**
  - tap a closed shape or closed stroke to set its fill attribute (no new geometry)
  - a Concepts-style lasso-fill brush for loose coloring
- **Pitfalls.**
  - pixel-based tolerances that behave differently at different zooms
  - node soup from tracing
  - self-intersecting strokes; normalize them with a union first
  - very large neighborhoods; cap the work and run it async with a spinner
  - fills that hide the lines because they were inserted on top
  - forgetting to drop hidden layers from the boundary set

---

## 3. Handwriting to text

### How the reference apps do it

**MyScript Notes (formerly Nebo, renamed Sept 2025).** Widely ranked best for accuracy.

- Live grey preview of recognized words above the ink; tap a word for **alternatives**.
- **Double-tap** to convert.
- Gestures: scratch-out to delete, vertical stroke to split or join ([MacStories](https://www.macstories.net/reviews/nebos-handwriting-recognition-elevates-your-notes/)).
- Diagram blocks turn into clean shapes, lines and text; text can be grouped with a figure. Math converts and copies as **LaTeX** ([Paperlike](https://paperlike.com/blogs/paperlikers-insights/myscript-notes-app-review)).
- 66 languages ([MyScript](https://help.myscript.com/notes/overview/available-languages/)).
- Backlash when conversion moved behind a subscription ([App Store reviews](https://apps.apple.com/us/app/myscript-notes-ai-handwriting/id1119601770?see-all=reviews&platform=iphone)).

**GoodNotes.** Lasso → Convert → Text **replaces the ink with a text box**, or use Copy Text ([GoodNotes support](https://support.goodnotes.com/hc/en-us/articles/7353695997199-Convert-handwriting-to-text-in-Goodnotes)). It built its own **fully on-device** model with a ">95% accurate" target, plus automatic language detection ([GoodNotes blog](https://www.goodnotes.com/blog/machine-learning-engineer-handwriting-recognition)). Handwriting search works without converting. Complaints mention weak OCR on some scripts and missing Greek letters ([G2](https://www.g2.com/products/goodnotes/reviews?page=4)).

**Notability.** Lasso → Convert. Handwriting search and recognition in 23 languages ([Notability on X](https://x.com/NotabilityApp/status/1274053483168071682)). Math is converted to images.

**Apple Notes.**

- Scribble writes into text fields.
- **Copy as Text**, and handwriting search.
- **Smart Script** refines and straightens your own handwriting and can paste typed text *in your handwriting* ([AppleInsider](https://appleinsider.com/articles/24/06/12/smart-script-impressively-forges-handwriting-in-ipados-18-notes-app)).
- **Math Notes** solves an expression when you write "=" ([Apple Newsroom](https://www.apple.com/newsroom/2024/06/ipados-18-introduces-powerful-intelligence-features-and-apps-for-apple-pencil/)).

**Samsung Notes.** Convert to text, AI straightening, and "clean up" with automatic formatting ([Samsung](https://www.samsung.com/us/support/answer/ANS10003634/)).

**Microsoft.** OneNote/Word's **Ink to Text Pen** is a *separate tool* that converts after each word as you write, with pen gestures for split, join and new line ([Microsoft support](https://support.microsoft.com/en-us/onenote/onenote-help-and-learning/explore-the-ink-to-text-pen)). Whiteboard offers ink-to-shape for squares, rectangles, triangles, circles, hexagons, pentagons and parallelograms, plus ink beautification ([Guiding Tech](https://www.guidingtech.com/top-microsoft-whiteboard-tips-tricks/)). Its ink-to-table was later removed ([MS Q&A](https://learn.microsoft.com/en-us/answers/questions/5166918/ink-to-table-feature-removed-from-microsoft-whiteb?forum=msoffice-all)).

**reMarkable.** MyScript-powered conversion that needs Wi-Fi and a cloud account ([reMarkable support](https://support.remarkable.com/s/article/Convert-handwritten-notes-into-text)).

**Concepts.** No conversion, but Pro "Deep Note Search" searches handwriting ([Concepts Pro](https://concepts.app/en/getting-started-with-concepts-pro-features/)).

### Engines

**Google ML Kit Digital Ink** (what we already have) ([overview](https://developers.google.com/ml-kit/vision/digital-ink-recognition), [Android guide](https://developers.google.com/ml-kit/vision/digital-ink-recognition/android)):

- 300+ languages and 25+ scripts; about **20 MB per language**, downloaded on demand; fully on-device; **Android and iOS only**.
- Input is strokes with points and **timestamps**.
- The size and length of a request are not documented. What is documented: "the recognizer assumes that the writing area only contains a single line of text". For multi-line areas, pass a `WritingArea` whose height is one line.
- **Pre-context** of up to 20 characters fixes word breaks and ambiguity.
- Accuracy depends on **stroke order**. For an inserted correction, Google recommends "sending the newly written word separately… and merging".
- A candidate is just `text` plus an optional `score` ([iOS ref](https://developers.google.com/ml-kit/reference/ios/mlkitdigitalinkrecognition/api/reference/Classes/MLKDigitalInkRecognitionCandidate)). There is **no stroke-to-word mapping**.
- Extra models: gestures, emoji, autodraw, and **basic shapes** (rectangle, triangle, arrow, ellipse). Shape classifiers treat the whole Ink as **one** shape ([models](https://developers.google.com/ml-kit/vision/digital-ink-recognition/base-models)).

**Apple PencilKit `PKStrokeRecognizer`** (iOS, iPadOS, macOS, visionOS 27) ([WWDC26 "Read between the strokes"](https://developer.apple.com/videos/play/wwdc2026/203/)):

- On-device, with the model bundled in the OS; 29 languages.
- `recognizedText()` for a whole drawing or a subset of strokeIDs.
- `indexableContent` returns all candidates concatenated, for search.
- `search("word")` returns bounds you can highlight.
- It works with **custom canvases**: convert Béziers to `PKStrokePath` and supply size, opacity and force per point.
- No math or shapes.

**Windows Ink** ([Microsoft Learn](https://learn.microsoft.com/en-us/windows/uwp/ui-input/convert-ink-to-text)):

- `InkAnalyzer` classifies writing versus drawing and returns a tree (writing region → paragraph → line → word, plus drawings) with shape kinds and per-node stroke IDs.
- `InkRecognizerContainer` handles single-line recognition with `GetTextCandidates()` alternates.
- Language packs come from Windows Settings.
- Microsoft's own sample places text at the ink bounding box with `FontSize = boundingRect.Height`.

**MyScript iink SDK** is the best quality option:

- The **RawContent** recognizer auto-classifies text, shapes, math and decorations on a free-form canvas ([iink 3.1](https://www.myscript.com/blog/iink-sdk-3-1/)); multilingual text arrived in 4.3.
- Native licensing is per device, by quote.
- Cloud costs $10 per 1,000 requests after 2,000 free per month ([MyScript support](https://developer-support.myscript.com/support/discussions/topics/16000030978)).

### Common complaints

- Accuracy on cursive, non-Latin scripts and symbols.
- **Line breaks and layout lost** on conversion.
- Converted text that is mis-sized or mis-placed.
- **Destructive conversion**: the ink is gone.
- Paywalls and cloud dependence (Nebo, reMarkable) and platform gaps (GoodNotes conversion isn't on Windows).
- Live conversion firing while you are drawing. Microsoft avoids this by making it an explicit pen.

### Recommendations for our app

- **Primary UX is selection-based:** lasso → "Convert to text".
  - Place an editable text box at the ink's bounding box, with font size set from the measured line height (≈ x-height × ~2, or the Windows `bbox.height` heuristic per line).
  - **Keep the ink** attached to the text object as `source_ink` with "Revert to ink". Don't delete it.
  - Show a per-line alternatives sheet built from ML Kit candidates, with a tap to swap.
  - Optional later: a live "text pen" as a *separate tool*, OneNote style. Never auto-convert the normal pen.
- **We own segmentation**, because ML Kit gives no stroke mapping and assumes one line:
  1. Rotate the strokes to the dominant baseline.
  2. Cluster them into lines by vertical overlap, with spatial and time gaps.
  3. Send each line with `WritingArea` = line height and pre-context = the last 20 characters of the previous line.
  4. Sort strokes in a line by timestamp.
  5. Rejoin lines with newlines. That keeps the line breaks users complain about losing.

  Synthesize monotonic timestamps for imported or duplicated strokes. Normalize coordinates to the local frame at a sane scale, so deep zoom doesn't send absurd magnitudes.
- **Diagram awareness happens before recognition:**
  - Strokes fully inside a shape or sticky become that container's **bound label**, using Excalidraw's `containerId` pattern ([schema](https://plus.excalidraw.com/docs/api/scene-content-schema)).
  - Strokes near a connector's midpoint become the connector's label.
  - Long, closed or low-density strokes are excluded as drawing.
  - For "ink to shape", call the ML Kit shapes model on **one candidate shape at a time** and accept only when the top score clearly beats the second (Google's own advice).
- **Search without converting.** Run a background idle-time indexer per line cluster. Store the top-N candidates (like `indexableContent`), the bounding box in local coordinates, and the **frame path**, so a search hit can fly the camera through nested frames. Re-index lines when they are edited.
- **Engine abstraction.** `recognize(line_ink, lang, precontext) → candidates[]`, with these backends:
  - ML Kit on iOS and Android, which we already have.
  - `PKStrokeRecognizer` on macOS (and as an option on iPadOS 27). It is free, on-device, and covers the desktop Mac.
  - `InkAnalyzer` on Windows.
  - Nothing on Linux: hide the feature there.

  Buy MyScript only if math or diagram parsing becomes core, and budget for quote-based per-device fees.
- **Woodworking extra.** Post-process recognized lines for dimensions such as `450 mm`, `3/4"` or `1' 6"` and offer "make this a dimension". That is a small parser on top of the text, and it plays to Math Notes' appeal.
- **Pitfalls.**
  - sending a whole page as one Ink to ML Kit
  - recognizing hidden-layer or non-text strokes
  - no UI for downloading the ~20 MB language models or for working offline
  - assuming ML Kit runs on the desktop (it doesn't)
  - destroying ink on convert
  - auto-conversion that misfires mid-drawing
