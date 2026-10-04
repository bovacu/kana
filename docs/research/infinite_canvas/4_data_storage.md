# How drawing apps store and manage data: lessons for a nested-frame infinite canvas

Scope: how the data is stored on disk, how it is held in RAM, how it is indexed, how undo works, and how files are saved. Each claim cites docs or source code. Numbers marked **(sim)** come from my own synthetic test, not from a cited source. It used 2,000 handwriting-like strokes, 430k points, sampled at 240 Hz with 52 pt/cm speeds. Treat them as order-of-magnitude figures.

---

## 1. Stroke representation and compression

### What existing systems do

| System | Point model | Storage | Size per point |
|---|---|---|---|
| **Apple PencilKit** | `PKStrokePoint`: location, timeOffset, size, opacity, force, azimuth, altitude. A path is a **uniform cubic B-spline whose points are control points**, not samples. `interpolatedPoints(strideBy:)` samples it by distance, time or parameter. A stroke = ink + path + **transform** + optional **mask** ([WWDC20 10148](https://developer.apple.com/videos/play/wwdc2020/10148/)) | `PKDrawing.dataRepresentation()`, proprietary. Apple says points "are stored in a lossily compressed format" (same talk) | undocumented |
| **Microsoft ISF** | integer X/Y plus optional pressure and other channels | Binary. Tags are 1-byte indexes into a GUID table. Numbers are varints, and signed values keep the sign in the LSB, so \|v\|<64 fits in 1 byte. Packet data = first value, first delta, then **second differences**, compressed with a tuned Huffman table ("second derivative values will generally be zero or one"). An alternative codec bit-packs each stroke with a 5-bit width header. **Transforms go in a transform table instead of being applied to the points**, which preserves precision and compression ([ISF spec](https://www.loc.gov/preservation/digital/formats/digformatspecs/InkSerializedFormat(ISF)Specification.pdf), pp. 10–25) | a few bits per coordinate (spec gives no figure) |
| **W3C InkML** | channels declared in `<traceFormat>` (X Y F T…) | Text. Prefix `!` = explicit value, `'` = first difference, `"` = second difference ([InkML](https://www.w3.org/TR/InkML/)) | text |
| **Wacom UIM** | spline X/Y/Z plus per-point properties | Protobuf inside RIFF. `int = float × 10^decimalPrecision`, then delta-coded ([UIM encoding](https://developer-docs.wacom.com/docs/sdk-for-ink/uim/encoding/)) | varint |
| **Xournal++ (.xopp)** | `Point{double x,y,z}`, where z is pressure and -1 means none ([Point.h](https://raw.githubusercontent.com/xournalpp/xournalpp/master/src/core/model/Point.h)) | gzipped XML. Coordinates are text, and the `width` attribute holds one width per point. Typical strokes have ~100 points and pages hold 100–1,000 strokes ([discussion #4862](https://github.com/xournalpp/xournalpp/discussions/4862)) | 24 B/pt in RAM |
| **Rnote** (Rust) | `BrushStroke{path: PenPath, style, hitboxes}`. A PenPath is a start element plus segments, and each element carries a position and a pressure ([brushstroke.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/strokes/brushstroke.rs)) | gzip(level 5) of serde JSON ([rnoteformat/mod.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/fileformats/rnoteformat/mod.rs)). In an experiment on one file, **JSON was 46 MB raw / 5.5 MB compressed; bitcode was 17.4 MB / 4.9 MB with zstd and deserialized 40× faster (0.020 s vs 0.822 s)** ([PR #1177](https://github.com/flxzt/rnote/pull/1177)) | f64 in RAM |
| **Excalidraw** | `freedraw` element: `points` relative to the element's x/y, plus `pressures[]`, `simulatePressure`, `strokeOptions` ([types.ts](https://raw.githubusercontent.com/excalidraw/excalidraw/master/packages/element/src/types.ts)) | JSON. Outlines are generated at render time by [perfect-freehand](https://github.com/steveruizok/perfect-freehand), which turns `[x,y,pressure]` into an outline polygon | ~29 B/pt JSON **(sim)** |
| **tldraw ≥4.3** | draw shape `segments[{type:'free'\|'straight', path, dim}]` | `path` is base64. The **first point is 3×Float32 (12 B); every later point is a Float16 delta (6 B, or 4 B when there is no pressure, `dim:2`)**. The encoded form is also used at runtime ([draw-shape docs](https://tldraw.dev/sdk-features/draw-shape), [TLDrawShape.ts](https://raw.githubusercontent.com/tldraw/tldraw/main/packages/tlschema/src/shapes/TLDrawShape.ts), [4.3 blog](https://tldraw.dev/blog/tldraw-sdk-4.3)). The release notes report it as ~80% smaller ([releases](https://github.com/tldraw/tldraw/releases)). **Long strokes are cut into new shapes after `maxPointsPerShape` = 600** | 6 B (8 chars in base64) |
| **Notability** | `curvespoints` = packed f32 x,y pairs, plus `curvesnumpoints` and per-curve `curveswidth`, inside a binary plist in a zip ([jvns](https://jvns.ca/blog/2018/03/31/reverse-engineering-notability-format/)) | zip | 8 B/pt |
| **GoodNotes** | A community converter describes zip + protobuf, Bézier chains and framed LZ4 in 32 KiB blocks ([gnnote PR](https://github.com/jakubfabrici/notability-goodnotes/pull/1)). Low confidence | | |
| **Concepts** | no public documentation found | | |

### Encodings measured on the same data (sim)

| Encoding | B/pt (x,y,pressure) | after deflate |
|---|---|---|
| JSON, full float repr | 62.0 | 28.2 |
| JSON, 4 decimal places (Excalidraw-like) | 29.2 | 11.0 |
| raw f64 (Xournal++ / Rnote RAM) | 24.0 | – |
| raw f32 | 12.0 | 11.2 |
| tldraw Float16 deltas | 6.0 (base64 8.0) | 5.7 |
| int quantized to 1/16 pt, delta-of-delta, zigzag varint | 3.0 (floor: 1 byte per value) | 1.3 |
| same, **adaptive Rice code per 32-value block** | **1.36** | – |
| same, per-16-value bit-packing (ISF-style) | 1.51 | – |
| 1/32 pt quantum + Rice | 1.58 | – |
| adding tilt (2×8 bit) and time (ms), varint | 6.0 | 1.8–2.0 |

Findings:

- **Quantize to integers, then take delta-of-delta, then entropy-code.** This is ISF's 2007 recipe and still beats everything above. Byte-aligned varints stop at about 1 B per channel per point. To get under 2 B/pt you need Rice or Huffman coding, bit-packing, or a general compressor.
- **Simplification barely shrinks files once the data is entropy-coded.** RDP at 0.1 pt kept 56% of points but only went from 1.36 to 1.18 B per original point **(sim)**. Dense, smooth samples already have near-zero second differences. RDP and Schneider Bézier fitting ([FitCurves.c](https://github.com/erich666/GraphicsGems/blob/master/gems/FitCurves.c): tangents, chord-length parameters, least squares, Newton–Raphson, split at the worst point) are worth doing for **RAM, GPU vertex count and hit-testing**, or for a "smooth/vectorize" command. They are not a file-size tool. RDP is O(n log n) expected and O(n²) worst case ([Wikipedia](https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm)).
- **Float deltas drift.** tldraw-style Float16 deltas build up error along a stroke unless the encoder takes each delta from the *reconstructed* previous point. Integer deltas are exact.
- Pressure needs 8–10 bits. Tilt and time hardly change between samples (240 Hz is near constant), so their second differences are mostly 0 and cost almost nothing after entropy coding.
- Google's polyline format is the same idea in text: ×1e5, delta, zigzag, 5-bit chunks ([polyline](https://developers.google.com/maps/documentation/utilities/polylinealgorithm)). Protobuf's varint and zigzag are the standard primitives ([encoding](https://protobuf.dev/programming-guides/encoding/)).

## 2. Object and scene model for mixed content

**tldraw**: a flat reactive store of JSON records `{id, typeName}` with branded ids such as `shape:`, `page:`, `binding:`. Records have a scope: *document* (persisted and synced), *session* (local: camera, current page) or *presence* (synced, never saved) ([store](https://tldraw.dev/sdk-features/store)). Shapes carry `parentId` (a page, group or frame), `index` (a fractional index key), and x/y/rotation in their parent's space. **Bindings are their own records**: `fromId` (the arrow), `toId` (the target), and props `terminal`, `normalizedAnchor`, `isPrecise`, `isExact`. `BindingUtil` callbacks such as `onAfterChangeToShape` move the arrow when the target moves ([bindings](https://tldraw.dev/sdk-features/bindings)). Images are **asset records** that shapes reference through `assetId`. "Multiple shapes can reference the same asset." The asset resolver receives `screenScale`, `steppedScreenScale` (powers of 2) and `dpr`, so it can serve a resolution that suits the zoom ([assets](https://tldraw.dev/sdk-features/assets)).

**Excalidraw**: a flat element array. Every element has `version` (incremented on each change), `versionNonce` (random), `updated`, an `isDeleted` tombstone, an `index` (fractional), `groupIds` (deepest to shallowest, so groups are not objects), `frameId`, and `boundElements` back-references. Arrows hold `startBinding`/`endBinding` (`elementId` + `fixedPoint`) ([types.ts](https://raw.githubusercontent.com/excalidraw/excalidraw/master/packages/element/src/types.ts)). Because a binding is recorded on both ends, the history code needs "binding repair" ([PR #7348](https://github.com/excalidraw/excalidraw/pull/7348)). Image `fileId` is the **SHA-1 of the file content**, so identical images are stored once in `files: Record<FileId, {dataURL,…}>` ([JSON schema](https://docs.excalidraw.com/docs/codebase/json-schema), [PR #4011](https://github.com/excalidraw/excalidraw/pull/4011/files)).

**Figma**: "Every Figma document is a tree of objects", conceptually `Map<ObjectID, Map<Property, Value>>`. The parent link and the fractional position are **one property, so they update atomically**. IDs are client ID + counter ([multiplayer](https://www.figma.com/blog/how-figmas-multiplayer-technology-works/)). Fractional indexes are strings in base 95 with "0." dropped ([ordered sequences](https://www.figma.com/blog/realtime-editing-of-ordered-sequences/)). On the server, swapping the per-node `BTreeMap<u16, ptr>` (~60 keys per node, <200 fields in total) for a **sorted `Vec<(u16, ptr)>` cut memory by 20–25%** ([Rust memory](https://www.figma.com/blog/supporting-faster-file-load-times-with-memory-optimizations-in-rust/)). The client stores "compact 32-bit floats or even bytes" ([Wallace](https://www.figma.com/blog/building-a-professional-design-tool-on-the-web/)). `.fig` files are Kiwi binary with an embedded schema and a zstd data chunk, roughly 2.3 MB compressed to 29 MB raw ([OpenFig research](https://github.com/OpenFig-org/openfig-core/blob/main/docs/research.md)).

**draw.io / mxGraph**: `mxCell` has `id`, `parent`, `vertex`/`edge`, `source`/`target` terminals, an `mxGeometry`, and a style *string* ([manual](https://jgraph.github.io/mxgraph/docs/manual.html)). Files are XML, by default raw-deflated and base64-encoded per diagram ([j2r2b](https://j2r2b.github.io/2019/08/01/drawio-decompressed-xml.html)).

**Takeaways:** use stable ids that never get reused; keep z-order as an order key, separate from the parent link; store relations (bindings, asset refs) as first-class records instead of copying geometry; deduplicate blobs by content hash; keep styles in a shared table (ISF's drawing-attributes table does this).

## 3. Spatial indexing, LOD and deep zoom

- **tldraw 4.4** moved hit-testing and culling from an O(n) scan to an **RBush R-tree**. Culled shapes get `display:none` ([v4.4](https://tldraw.dev/releases/v4.4.0), [culling](https://tldraw.dev/sdk-features/culling)). At low zoom, draw shapes render solid ([draw-shape](https://tldraw.dev/sdk-features/draw-shape)).
- **Rnote** keeps an `rstar` R-tree keyed by slotmap keys and `bulk_load`s it on rebuild ([keytree.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/store/keytree.rs)). Each stroke caches images for the *viewport plus a margin* at the current scale. These are regenerated on rayon threads when the scale changes by more than 1% (`RENDER_IMAGE_SCALE_TOLERANCE = 0.01`) ([render_comp.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/store/render_comp.rs)).
- **Excalidraw** has no index. It caches each element as an offscreen canvas in a `WeakMap` keyed by the immutable element and regenerates it on zoom change. The cache is capped at **16,777,216 px of area ("~safari mobile canvas area limit")** ([renderElement.ts](https://raw.githubusercontent.com/excalidraw/excalidraw/master/packages/element/src/renderElement.ts)). Zoom is clamped to 0.1–30 ([constants.ts](https://raw.githubusercontent.com/excalidraw/excalidraw/master/packages/common/src/constants.ts)). Most apps sidestep deep zoom by clamping it.
- **InfiniPaint** (C++/Skia, open source, unlimited zoom) is the closest relative of our design. World coordinates are `FixedPoint::Number<boost::multiprecision::cpp_int, 32>` (an arbitrary-precision integer with 32 fraction bits). **Each component has its own `CoordSpaceHelper{pos, inverseScale, rotation}` and stores its content as local `Vector2f`** ([SharedTypes.hpp](https://raw.githubusercontent.com/ErrorAtLine0/infinipaint/HEAD/src/SharedTypes.hpp), [FixedPoint.hpp](https://raw.githubusercontent.com/ErrorAtLine0/infinipaint/HEAD/include/Helpers/FixedPoint.hpp), [CoordSpaceHelper.hpp](https://raw.githubusercontent.com/ErrorAtLine0/infinipaint/HEAD/src/CoordSpaceHelper.hpp)). Rendering uses a BVH (≤50 components per node, rebuilt past 1,000) that caches nodes as 2048-px surfaces, at most **96 surfaces (40 on web)** ([DrawingProgramCache.cpp](https://raw.githubusercontent.com/ErrorAtLine0/infinipaint/HEAD/src/DrawingProgram/DrawingProgramCache.cpp)). By my arithmetic, 96 × 2048² × 4 B is about 1.6 GB, which would be fatal on a tablet. Nested frames get the same "small local floats" property without bignum math.
- **Static vs dynamic index:** Flatbush, a packed Hilbert R-tree in a single buffer, indexes 1M rectangles in 109 ms ([flatbush](https://github.com/mourner/flatbush)). RBush picking over 20k shapes took 0.088 ms ([infinitecanvas.cc L8](https://infinitecanvas.cc/guide/lesson-008)). A uniform grid struggles with the huge range of object sizes, and a quadtree hits the same problem at its edges. R-trees cope with mixed sizes best. With nested frames, **the frame tree already acts as a coarse multi-scale index**, so each frame only needs a small flat R-tree.
- **Raster tiling:** Krita uses 64×64 tiles, LZF-compressed for both saving and swap ([tile format](https://community.kde.org/Krita/Tile_Data_Format)). Procreate stores `{col}~{row}.chunk` LZO/LZ4 tiles per layer in a zip ([silicate](https://github.com/Avarel/silicate)). Figma's renderer is "a highly-optimized tile-based engine" ([Wallace](https://www.figma.com/blog/building-a-professional-design-tool-on-the-web/)).
- **Deep zoom precedents:** Deep Zoom pyramids halve resolution at each level with 256×256 tiles. Its **"sparse images" put a pyramid inside a pyramid**, which is nested frames for raster data ([Deep Zoom](https://learn.microsoft.com/en-us/previous-versions/windows/silverlight/dotnet-windows-silverlight/cc645077(v=vs.95))). In map tiles, pixel = world × 2^zoom ([Google Maps](https://developers.google.com/maps/documentation/javascript/coordinates)). On float precision: f32 gives ~7 digits, and 1 cm steps last only up to 131,071 m. The fixes are **relative-to-center** (CPU subtracts in double, GPU gets small floats), which is what frame re-anchoring does, and relative-to-eye high/low float pairs ([AGI "Precisions, Precisions"](https://help.agi.com/AGIComponents/html/BlogPrecisionsPrecisions.htm)). For camera animation across many zoom levels, use log-space van Wijk–Nuij interpolation ([d3 interpolateZoom](https://d3js.org/d3-interpolate/zoom)).

## 4. Undo and redo models

| Model | Example | Memory | Notes |
|---|---|---|---|
| **Command with atomic changes** | mxGraph: an `mxUndoableEdit` collects `mxGeometryChange`, `mxTerminalChange`, `mxChildChange`… A `beginUpdate`/`endUpdate` counter groups them into one undo step ([mxGraphModel](https://jgraph.github.io/mxgraph/docs/js-api/files/model/mxGraphModel-js.html)) | proportional to the change | each change swaps old and new values |
| **Record diffs + marks** | tldraw: changes build up in a *pending diff* (squashed) until `markHistoryStoppingPoint()`, then become one entry. Stacks hold `diff` or `stop` entries. `bailToMark`, `squashToMark`, `history:'ignore'`. Only `source:'user'` changes are recorded ([history](https://tldraw.dev/sdk-features/history), [HistoryManager.ts](https://raw.githubusercontent.com/tldraw/tldraw/main/packages/editor/src/lib/editor/managers/HistoryManager/HistoryManager.ts)) | whole before/after records for each touched record | squashing makes a 600-point stroke one entry |
| **Property-level deltas** | Excalidraw ≥0.18: `Delta{deleted, inserted}`, inverted by swapping the two. Per-user stacks. Entries with no visible effect are skipped ([PR #7348](https://github.com/excalidraw/excalidraw/pull/7348)) | smallest | replaced the old model (next row) |
| **Scene snapshots with dedup** | Excalidraw ≤0.17: every entry stored **(id, versionNonce) for *every* element**, plus a cache of element versions ([v0.17 history.ts](https://raw.githubusercontent.com/excalidraw/excalidraw/v0.17.0/src/history.ts)) | O(N) per entry | was abandoned |
| **Persistent snapshots (structural sharing)** | Rnote: an entry clones `Arc`-wrapped slotmaps, and unchanged strokes are shared pointers. `HISTORY_MAX_LEN = 100` ([store/mod.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/store/mod.rs)) | O(map size) of pointers per entry, plus changed strokes | simple and robust |
| **Raster transactions** | Krita: a central per-layer store of old tile versions (the earlier memento design cost "two hash tables at least 4KiB each" per transaction). Undo tiles are swapped to disk first ([transactions](https://community.kde.org/Krita/Transactions_Design), [tile engine](https://dimula73.blogspot.com/2009/08/gsoc-krita-tile-engine-wrap-up.html)) | 16 KB per touched 64×64 RGBA tile | |

Other points:

- **Multiplayer rule from Figma:** undoing a lot, copying, then redoing must leave the document unchanged. "An undo operation modifies redo history at the time of the undo" ([Figma](https://www.figma.com/blog/how-figmas-multiplayer-technology-works/)).
- **Transforms:** if transforms live on the object (as in ISF and PencilKit), undoing a move or rotation of 10k points costs one matrix.
- **Connectors:** tldraw's binding callbacks run inside the same transaction, so derived arrow updates land in the same undo entry. The cheaper option is to keep routes out of history and recompute them.
- **Eraser:** Rnote records history only when the eraser lifts ([eraser.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/pens/eraser.rs)).

## 5. File format and saving

- **Container choices.** gzip+JSON: Rnote, Xournal++, Excalidraw, `.tldr`. Zip packages: Procreate, Notability, Krita `.kra`, `.fig`. Custom binary: ISF, Kiwi. JSON is easy to version but costs 10–60 B/pt and loads slowly (Rnote: 0.82 s JSON vs 0.02 s bitcode). Base64 adds 33% to embedded images (Xournal++ stats).
- **SQLite as the document.** Commits are atomic and crash-safe. "Only those parts of the file that actually change are written". The app can "load as much material as is needed to draw the next screen". Adding tables or columns keeps old files compatible. Size is within ±1% of zip ([appfileformat](https://www.sqlite.org/appfileformat.html)).
- **Atomic replace.** Rnote: "write to temporary file, check temporary file, replace save file" ([PR #1177](https://github.com/flxzt/rnote/pull/1177)). Caveat: "rename isn't atomic on crash" without `fsync` of both the file and its directory ([Dan Luu](https://danluu.com/file-consistency/)). Platform helpers exist: `Data.write(options:.atomic)` on Apple and [`AtomicFile`](https://developer.android.com/reference/android/util/AtomicFile) on Android.
- **Versioning.** Rnote reads `version` first and upgrades through a chain of `TryFrom` converters (0.5→0.6→0.9→0.13→0.15) ([mod.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/fileformats/rnoteformat/mod.rs)). It plans to move the version **outside the compressed data** ([issue #1173](https://github.com/flxzt/rnote/issues/1173)). tldraw migrates each record type ([store](https://tldraw.dev/sdk-features/store)). Kiwi embeds its schema. ISF's warning: a reader cannot decode a codec added later without a version bump, so put a codec ID byte on every blob.
- **Lazy loading.** Figma's dynamic page loading fetches the first page plus its dependencies (tracked by QueryGraph): **70% fewer nodes in memory, 33% fewer out-of-memory crashes, slowest loads 33% faster** ([Figma](https://www.figma.com/blog/speeding-up-file-load-times-one-page-at-a-time/)). Rnote wants partial loading using stored R-tree bounds ([#1173](https://github.com/flxzt/rnote/issues/1173)).

## 6. Eraser in a vector app

- **Whole-object erasers:** tldraw, Excalidraw, Xournal++ "Delete stroke", Rnote `TrashCollidingStrokes`, PencilKit `.vector`. Undo is trivial: restore the tombstoned objects.
- **Splitting erasers:** Xournal++ "Standard" ([guide](https://xournalpp.github.io/guide/tools/eraser/)) calls `intersectWithPaddedBox`, which returns a **sorted, even-length list of `PathParameter{segment index, t}`** marking where the stroke enters and leaves the eraser box padded by the stroke width. `cloneSection(lo, hi)` then builds the surviving pieces ([Stroke.h](https://raw.githubusercontent.com/xournalpp/xournalpp/master/src/core/model/Stroke.h)). Rnote has `SplitCollidingStrokes` ([eraser.rs](https://raw.githubusercontent.com/flxzt/rnote/main/crates/rnote-engine/src/pens/eraser.rs)).
- **Masks instead of splitting:** PencilKit's pixel eraser leaves the points alone and adds a **mask** that clips rendering and hit-testing ([WWDC20](https://developer.apple.com/videos/play/wwdc2020/10148/)).
- **Geometry for variable-width strokes.** The eraser path is a capsule chain of radius r. A centerline point c(t) is erased when dist(c(t), eraser) < r + w(t)/2. Solve this per segment for entry and exit values of t, merge the intervals, and interpolate position, pressure and tilt at each cut. Give cut ends flat caps, or clip the stroke's round caps against the eraser; otherwise the caps bulge back into the erased gap.
- **Data and undo.** Record one entry per gesture: the removed stroke IDs plus the new piece records. A more compact option stores the original stroke ID and the list of erased intervals, and regenerates the pieces deterministically. Pieces get new IDs. Don't let erasing create slivers: drop pieces shorter than about 1 px at draw scale.

## 7. RAM and GPU budgets on tablets

- **iPad.** Jetsam kills an app that goes over its limit. In 2021 that limit was ~5 GB on both 8 GB and 16 GB M1 iPad Pros, and ~3 GB (about half of RAM) on 6 GB models ([9to5Mac](https://9to5mac.com/2021/05/28/ipad-pro-ram-limits/)). The `increased-memory-limit` entitlement (iOS 15+) raises it ([Apple](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.kernel.increased-memory-limit)), and `os_proc_available_memory()` reports what's left. Metal 2D textures max out at 16,384 px ([feature tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)).
- **Procreate's limits** are set by RAM. Canvases go up to 16,384 px on the longest edge and 33–134 MP depending on model ([help](https://help.procreate.com/articles/dabqrn-maximum-canvas-size)). At 8192², one layer is 256 MB, and an M1 with 16 GB allowed 24 layers ([layer limits](https://procreate.com/insight/2021/layer-limits)).
- **Android** caps the Java heap (`getMemoryClass()`), lets the low-memory killer reclaim processes, and sends `onTrimMemory` warnings ([memory overview](https://developer.android.com/topic/performance/memory-overview)). Browser tabs get about 2 GB (Figma).
- **Rule of thumb:** vector data is tiny. One million points at 12 B is 12 MB decoded, or ~1.5 MB encoded. **Raster caches dominate.** One 2048² RGBA8 tile is 16.8 MB, and one full-screen 13" iPad buffer (2752×2064) is 22.7 MB.

---

## Recommendations for our app

**1. Stroke encoding: about 1.5 B/pt for x, y and pressure; about 2–2.5 B/pt with tilt and time.**
- Header per stroke: origin as f32 in frame-local coordinates; `q_exp` (int8) so that quantum = 2^q_exp local units, chosen near **1/16 screen pt at the drawing zoom**; a style-table index; point count; and a codec byte.
- Channels, stored separately: x, y, pressure (8 or 10 bit), optional azimuth and altitude (8 bit each) and time (ms). Each is integer delta-of-delta, zigzag, adaptive Rice coding per 32-value block. Measured 1.36 B/pt for x,y,p **(sim)**, against 12 B for raw f32 and 6 B for tldraw.
- Store raw input unsimplified, as a centerline plus samples. Fit curves only when rendering.
- Cut strokes at about 512 points, as tldraw does at 600. This keeps bounding boxes tight and saves incremental.
- In RAM: off-screen strokes stay encoded. Visible strokes decode into SoA f32 x/y plus u16 pressure (~10 B/pt) inside an LRU budget. Tessellate only the LOD you need.
- *Why:* this is ISF's proven recipe. Exact integers mean no drift, and a per-stroke quantum stays correct at any frame depth.

**2. Object model.**
- A frame tree. Each frame holds a flat table of objects: a stable 64-bit ID (device ID + counter), a type, a **transform kept apart from geometry** (translation f32, rotation, uniform scale), a parent frame or group, a **fractional z-key**, a style ID, and a payload blob reference.
- Types: stroke, shape (parametric: rect, ellipse, line), path (cubic Béziers), text, sticky, image (holds a SHA-256 asset ID; the asset table stores the bytes once, with mip thumbnails), group, and connector.
- **Bindings as separate records** `{connector, end, target id, normalized anchor, precise}`. Routes are derived data, recomputed and never persisted.
- *Why:* tldraw and Figma both show that stable IDs, order keys and relation records hold up. Separate transforms make moves cheap and keep quantized data intact.

**3. Spatial index.**
- The frame tree is the coarse multi-scale index. Inside each frame, keep an R-tree over local f32 AABBs: bulk-load it (STR or Hilbert) when the frame loads, insert dynamically on edits, rebuild lazily.
- Far-away frames draw from cached **impostor textures** (also saved in the file as thumbnails).
- Cap the cache by bytes (for example 256–512 MB on iPad, less on Android), evict LRU, and purge on memory warnings.
- *Why:* RBush, rstar and Flatbush all show an R-tree handles mixed object sizes, and nesting keeps every tree small.

**4. Undo model.**
- Transactions of property-level deltas `{frame, object, field, before, after}`, grouped by marks the way tldraw does it.
- Large payloads (point blobs, images) are immutable and reference-counted, so an entry holds a handle, not a copy. A whole eraser gesture is one entry.
- Derived data is recomputed, not recorded. Bound history by bytes (e.g. 32–64 MB), not entry count.
- *Why:* both Excalidraw and tldraw converged on diffs. Snapshots cost O(N) per entry.

**5. File format.**
- **One SQLite file.** Tables: `meta` (format version, readable before anything else), `frames` (id, parent, transform, bounds, thumbnail), `chunks` (frame id, sequence, codec byte, zstd blob of object records), `assets` (hash, MIME type, bytes, mips), `styles`.
- Autosave writes only the dirty chunks in one WAL transaction. Opening a file loads only frames near the camera's frame. Migrations are a chain of per-version functions.
- Export or share a compacted copy (`VACUUM INTO`) using temp-file-then-rename with fsync.
- *Why:* atomic, incremental and lazy-loading come for free from a C library, at about zip-level size.

**The 5 worst pitfalls to avoid**
1. **Baking transforms into points**, or re-quantizing on every move. Precision drifts, compression suffers and undo entries grow (ISF's founding lesson).
2. **Storing absolute floats or JSON text for points** (12–62 B/pt), or float deltas without closed-loop quantization, which drift.
3. **Snapshot-style undo, or putting derived data into history or the file.** That covers whole-scene snapshots, connector routes and tessellations; old Excalidraw's O(N)-per-entry history is the cautionary example.
4. **Unbounded raster or texture caches.** They, not the vectors, get apps killed by jetsam (InfiniPaint's worst case is ~1.6 GB, and Procreate's whole layer system is RAM-gated). Budget in bytes and degrade gracefully.
5. **Full rewrites or non-atomic autosaves of large documents.** Also avoid hiding the version inside compressed data, and avoid live SQLite or WAL files in a cloud-synced folder: edit a local copy and publish atomically.
