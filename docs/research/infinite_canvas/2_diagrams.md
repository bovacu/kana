# Diagramming on an infinite canvas: what the best tools do

Scope: draw.io/diagrams.net, Lucidchart, Miro, FigJam, Whimsical, Excalidraw, tldraw, Apple Freeform, OmniGraffle, Visio, yEd, Microsoft Whiteboard, MyScript Notes (Nebo), Mermaid/PlantUML/D2/Graphviz, Eraser, Napkin. Research date: Oct 2026.

**Short version.** Connectors decide whether people like a diagram tool. The tools people praise (draw.io, Whimsical, Visio) give you connectors that are predictable and that you can override. The tools people complain about (Miro, Lucidchart's "Smart Lines", early Excalidraw binding) have connectors that are "smart" and keep rewriting what the user drew. No major tool is pen-first for diagrams. Most rely on hover and modifier keys, and a tablet has neither. That gap is our opening.

---

## 1. Connectors and arrows

**Binding model: every serious tool ends up with the same two attachment types.**
- *Floating / dynamic* (attached to the shape as a whole). The endpoint slides around the perimeter and takes the shortest route as shapes move. *Fixed / point* (attached to one spot). The endpoint stays on a chosen port. draw.io marks the difference on the selected endpoint: a "." means floating and an "X" means fixed. While you drag, the target shows a blue outline for floating and a green highlight for fixed. Holding Alt lets you attach to any point on the shape, not only the preset ports ([draw.io connectors](https://www.drawio.com/docs/manual/connectors/), [floating vs fixed](https://drawio-app.com/blog/floating-and-fixed-connections-in-draw-io/)).
- Visio calls these "dynamic glue" and "point glue". It shows a filled dot when the endpoint wanders around the shape and an open dot when it is pinned ([Visio glue](http://guides.wmlcloud.com/office/microsoft-visio-2010---connecting-shapes---understanding-visio-connectors-(part-2)---connecting-to-shapes-versus-points-on-shapes.aspx)). OmniGraffle does the same thing: drop on a shape's center and the line snaps to the nearest "magnet", drop on a magnet and the line remembers it ([OmniGraffle manual](https://support.omnigroup.com/documentation/omnigraffle/mac/7.19/en/diagramming-basics/)).
- **tldraw's version is the cleverest, and it works with a pen.** Drag fast and the arrow is "imprecise": it aims at the shape's center and stops at the edge. Pause over the target (600 ms while hovering, 320 ms while dragging) and it becomes "precise", pinned to a normalized anchor. Hold Alt and it becomes "exact", meaning the arrow goes inside the shape to that point ([tldraw bindings](https://tldraw.dev/examples/arrow-binding-options), [default shapes](https://tldraw.dev/sdk-features/default-shapes)). Pausing is a modifier that needs no keyboard.

**Routing styles.** draw.io has the widest set: straight, orthogonal, simple, isometric, curved, entity-relation (leaves and enters on the same side with two bends) and auto-route around shapes. It also has rounded corners and line jumps where lines cross ([connector styles](https://www.drawio.com/docs/manual/styles/connector-styles/)). tldraw has arcs controlled by a bend handle, plus elbow arrows since v3.13 ([elbow PR](https://github.com/tldraw/tldraw/pull/5572)). FigJam has bent, curved and straight ([FigJam connectors](https://help.figma.com/hc/en-us/articles/1500004414542-Create-diagrams-and-flows-with-connectors-in-FigJam)).
- **Excalidraw's elbow arrows are the best editing model we found.** The arrow routes itself automatically. If you drag a segment, that segment stays where you put it (only its direction is preserved). Double-clicking the hollow knob in the middle of a segment hands it back to auto-routing ([PR #8952](https://github.com/excalidraw/excalidraw/pull/8952), [PR #8615](https://github.com/excalidraw/excalidraw/pull/8615)).
- Visio lets each connector choose how much automation it accepts: *Reroute Freely / As Needed / On Crossover / Never* ([PacketLife](https://packetlife.net/blog/2012/jan/23/visio-connector-tips/)). That is what users actually want.

**Obstacle avoidance: the biggest source of complaints.**
- draw.io's orthogonal routing is essentially a fixed 2–3 segment Z-shape. The middle segment sits at the exact halfway point, so it falls off the grid ([issue #4826](https://github.com/jgraph/drawio/issues/4826)). Ports placed inside a shape's outline give "garbage behavior" ([discussion #5504](https://github.com/jgraph/drawio/discussions/5504)).
- Lucidchart has no setting that stops lines crossing through shapes. A user in July 2025: "I have hundreds lines to move" ([Lucid idea](https://community.lucid.co/ideas/automatically-prevent-lines-from-overlapping-crossing-through-an-existing-shape-in-lucidchart-2948)). Its "Smart Lines" move endpoints to other sides of a shape, and the overlaps "get worse as you add more" ([Lucid idea](https://community.lucid.co/ideas/improve-connector-lines-in-lucidchart-10258)).
- Miro: "I spend probably a quarter of my time in a given diagram trying to coax the connectors". Users also report auto-routed arrows that "jump erratically" and the lack of line jumps ([Miro idea](https://community.miro.com/ideas/autorouting-of-connectors-9758)). Snapping is "too eager", and loose lines connect themselves to nearby objects ([Miro idea](https://community.miro.com/ideas/better-lines-connectors-in-miro-8328)).
- Excalidraw: "Arrows lock to *everything*… seconds saved and hours lost" ([#4797](https://github.com/excalidraw/excalidraw/issues/4797)). Excalidraw responded by binding only when you deliberately drag an endpoint onto a shape, and in 2026 it added a preference to turn binding off, with Cmd/Ctrl as a temporary override ([X post](https://x.com/excalidraw/status/2029240702262006191)).
- Whimsical is the counterexample. Reviewers praise it because "connectors auto-route around nodes… almost no manual cleanup" ([review](https://www.produkthub.dk/tools/whimsical-review)). It gets there by being opinionated and limiting styles.

**Labels.** draw.io allows three labels per connector: middle, source end and target end. The end labels are exactly what UML multiplicities and ER cardinalities need, and you drag them by diamond handles ([connectors](https://www.drawio.com/docs/manual/connectors/)). tldraw labels sit at a 0–1 position along the arrow and wrap to the arrow's geometry ([2025 update](https://tldraw.dev/blog/whats-new-2025)). FigJam only made connector labels movable in Feb 2025 ([Figma](https://x.com/figma/status/1887959697497444589)).

**Arrowheads.** tldraw has 9 types (arrow, triangle, square, dot, pipe, diamond, inverted, bar, none). FigJam has 5. Excalidraw added crow's-foot heads (one, many, one-or-many) in 2025 but hides them behind a "more" toggle ([PR #8942](https://github.com/excalidraw/excalidraw/pull/8942)). Software diagrams need, at minimum, the hollow triangle, the open and filled diamonds, the open arrow, and the full crow's-foot set (zero/one/many combinations).

**Drawing connectors by pen or finger.** Touch has no hover. draw.io on touch: tap to select, then drag from a direction arrow ([touch guide](https://www.drawio.com/docs/tutorials/touch-screen-diagrams/)). Freeform: select an item, drag one of its edge arrows, and a shape picker opens where you let go ([Apple](https://support.apple.com/guide/freeform/add-a-diagram-frfm1e6c3d3e/mac)). On iPad, Lucid has users "accidentally moving the page when they wanted to drag the corner of a box" ([HN](https://news.ycombinator.com/item?id=21514557)). Freeform users cannot connect several objects to one line ([Apple Community](https://discussions.apple.com/thread/256055815)). Microsoft Whiteboard only got anchored connectors in Aug 2023 ([handsontek](https://m365admin.handsontek.net/whiteboard-connectors-line-anchoring/)).

## 2. Shapes and libraries

- **draw.io has the deepest free catalogue**: UML, ER tables, BPMN, C4, AWS/Azure/GCP, Cisco, Kubernetes, 50+ icon sets. That is why "for formal diagrams draw.io wins decisively" ([comparison](https://instapods.com/apps/excalidraw/vs/drawio/)). Miro Diagrams claims 3,000+ shapes and layers ([Miro June 2025](https://miro.com/blog/what-we-launched-june-2025/)), but BPMN, AWS and similar packs are locked to Business plans and above ([Miro](https://help.miro.com/hc/en-us/articles/4403634496402-Miro-for-mapping-diagramming)). tldraw ships only ~20 geometric shapes. Excalidraw depends on community libraries.
- **Structured objects beat drawn ones.** Lucidchart imports SQL into ready-made table shapes and draws the relationships when you drop related tables. It also builds sequence diagrams from markup ([Lucid ERD](https://help.lucid.co/hc/en-us/articles/16471565238292-Create-an-Entity-Relationship-Diagram-in-Lucidchart)). Sequence diagrams in particular are painful to draw by hand, so every serious tool offers a generator.
- **Model vs picture.** Developers' main complaint about architecture diagrams is *drift*: "devs need system design tools, not diagramming tools" ([HN](https://news.ycombinator.com/item?id=40977308)). Structurizr and IcePanel derive several C4 views from one model ([IcePanel vs Structurizr](https://icepanel.io/blog/2025-11-14-icepanel-vs-structurizr)). For us this is a later "linked element" feature, not a v1 item.
- **Containers.**
  - tldraw frames clip their children and move them along ([default shapes](https://tldraw.dev/sdk-features/default-shapes)).
  - Lucid swimlanes are "magnetized", so their contents move with them. Its "assisted layout → Fit" grows and shrinks a container to fit what is inside. However, resizing a lane does *not* move the shapes in it, and users dislike that ([Lucid](https://community.lucid.co/ideas/automatically-resize-frames-and-containers-based-on-the-shapes-inside-them-8382)).
  - FigJam's sections organize the board but do not lay anything out.
- **Text in shapes.** Excalidraw re-wraps bound text and grows the container to fit ([deepwiki](https://deepwiki.com/excalidraw/excalidraw/3.8-bound-text-and-container-system)). draw.io has an explicit Autosize command (Ctrl+Shift+Y) ([shortcuts](https://www.drawio.com/docs/reference/shortcuts/)). tldraw text is auto-width by default or fixed-width with wrapping, and supports rich text including in shape labels.
- **Changing a shape's type.** Lucid can swap a shape for another type and keep its text ([Lucid](https://lucid.co/blog/features-for-quicker-diagramming)). Excalidraw lets you press Tab while flowcharting to cycle rectangle, diamond and ellipse.
- **Style memory.** FigJam remembers the last connector style you used. Whimsical's quick-add keeps your previous styling. Both remove a lot of reformatting.

## 3. Layout aids

- **Snapping.** Excalidraw snaps to edges, centers and *equal gaps* while moving, resizing and *inserting*. Alt+S toggles snapping and Cmd/Ctrl inverts it temporarily ([LinkedIn](https://www.linkedin.com/posts/excalidraw_snapping-to-object-activity-7113431894989361152-M95j)). tldraw adds "gap center" snapping, which centers an object in a gap, but by default you have to hold Ctrl to snap at all ([tldraw snapping](https://tldraw.dev/sdk-features/snapping)). Pen users have no Ctrl key, so snapping must be on by default and toggled from the UI.
- **Tidy up (FigJam).** Select 3+ objects and they snap into an evenly spaced grid. Pink spacing lines inside the selection can then be dragged to change the gaps ([FigJam](https://help.figma.com/hc/en-us/articles/1500004292221-Select-move-and-order-objects-in-FigJam)). This works very well with touch.
- **tldraw's align, distribute and stack commands take arrow-connected shapes into account** ([2025](https://tldraw.dev/blog/whats-new-2025)).
- **Auto-layout.**
  - yEd is the reference: hierarchical layout that minimizes crossings, orthogonal, organic ([yEd](https://yed.yworks.com/support/manual/layout/layout_hierarchic_incremental.html)).
  - draw.io uses ELK for vertical and horizontal flow, tree, radial tree and organic layouts ([draw.io layouts](https://www.drawio.com/docs/manual/layouts/)).
  - Whimsical: select connected shapes and choose vertical or horizontal, and all connectors re-route ([Whimsical](https://whimsical.com/learn/get-started/flowcharts)).
  - OmniGraffle animates its auto-layout so you can see where things went, and can lay out only connected objects, leaving headers and logos alone ([release notes](https://www.omnigroup.com/index.php/releasenotes/omnigraffle-mac/P65)).
  - D2's TALA produces orthogonal, whiteboard-like architecture layouts and can lock some nodes in place. The cost is that it is non-deterministic: adding one node can rearrange the whole diagram, while dagre and ELK only nudge things ([TALA](https://d2lang.com/blog/tala-is-open-source/), [D2 layouts](https://d2lang.com/tour/layouts/)).

## 4. Fast creation

- **draw.io has the most complete set**:
  - Hover a shape and four blue arrows appear. Click one to clone the shape and connect it. Hover the arrow to choose a different shape. Ctrl-drag the arrow to place the clone yourself.
  - Drop a library shape onto an arrow or onto a connector's end to connect it.
  - Alt+Shift+Arrow clones and connects in that direction. If a shape is already there, it just connects to it.
  - Double-clicking empty canvas opens a shape picker right there.
  - Sources: [connect shapes](https://drawio.com/blog/connect-shapes), [clone-connect](https://www.drawio.com/blog/shortcut-clone-connect), [double-click](https://www.drawio.com/blog/double-click-shortcut).
- **Lucid**:
  - "Ghost shapes" preview where the next node could go, each with its own hotkey.
  - Drop a shape onto an existing line and it inserts itself in the middle.
  - Automatic branching spaces new lines evenly ([Lucid](https://lucid.co/blog/features-for-quicker-diagramming)).
  - Visio has had AutoConnect blue triangles with a Quick Shapes mini-toolbar since 2007 ([Visio](https://support.microsoft.com/en-us/visio/add-connectors-between-visio-shapes)).
- **Keyboard flowcharting.**
  - Whimsical: Opt/Alt+Arrow, and Q toggles the quick-add buttons ([Whimsical](https://whimsical.com/learn/get-started/flowcharts)).
  - Excalidraw: Cmd/Ctrl+Arrow creates a node. Press the arrow repeatedly to fan out several nodes, and Tab changes the shape ([X](https://x.com/excalidraw/status/1823079626156961937)).
  - tldraw sticky notes: Tab goes right, Shift+Tab left, Cmd+Enter below. Each creates a note or selects the one already there ([tldraw notes](https://tldraw.dev/sdk-features/note-shape)).
- **Turning sketches into clean diagrams.**
  - MyScript Notes (Nebo) is the gold standard. Draw shapes, arrows and handwritten labels, double-tap, and you get clean shapes and text. The connectors stay attached afterwards, and the result exports to SVG and GraphML ([MacStories](https://www.macstories.net/reviews/nebos-handwriting-recognition-elevates-your-notes/), [MyScript SDK](https://www.myscript.com/sdk/)).
  - Microsoft Whiteboard straightens a shape when you hold the pen still at the end of the stroke, turns drawn grids into editable tables, and undo brings back the raw ink ([Whiteboard](https://support.microsoft.com/en-us/whiteboard/draw-and-ink-in-whiteboard)).
  - OmniGraffle iOS adds arrowheads when you draw a chevron at the end of a line ([Omni](https://support.omnigroup.com/omnigraffle-ios-draw-with-shape-recognition/)).
  - Miro "smart drawing" turns single strokes into shapes, stickies and lines ([Miro](https://help.miro.com/hc/en-us/articles/360017572014-Smart-drawing)).
  - Excalidraw only got an "Autoshape" (Shift+X) in July 2026. Its tablet meta-issue had listed shape recognition as missing ([changelog](https://plus.excalidraw.com/changelog), [#9705](https://github.com/excalidraw/excalidraw/issues/9705)).
- **Text-to-diagram.**
  - draw.io inserts Mermaid as *native* shapes inside a container that keeps the source. You can re-edit the code, and your style overrides survive regeneration ([draw.io Mermaid](https://www.drawio.com/blog/mermaid-updates/)).
  - Miro renders pasted Mermaid as editable shapes with **bidirectional** sync ([Miro Mermaid](https://miro.com/mermaid-diagram/)).
  - tldraw parses Mermaid, renders it offscreen and reads node positions from Mermaid's own SVG, so it gets layout for free. It supports flowchart, sequence, state and mindmap ([tldraw blog](https://tldraw.dev/blog/turning-mermaid-code-into-shapes)). Users already want the result to stay editable as a diagram after conversion ([#10801](https://github.com/tldraw/tldraw/issues/10801)).
  - Pure text tools hit a ceiling. Past 10–15 nodes Mermaid gives "crossing lines, overlapping labels… no manual override" ([zandrey](https://www.zandrey.com/blog/why-mermaid-sucks)). Eraser pairs diagram-as-code with a canvas ([Eraser](https://docs.eraser.io/diagram-as-code)). Napkin-style AI produces a decent first pass, then "customization hits a ceiling quickly" ([review](https://www.therundown.ai/tools/napkin-ai)).

## 5. Sticky notes and text

- **Notes should grow, not shrink.** FigJam stickies grow taller with their content and snap to double width when dragged sideways. Cmd+Enter creates the next sticky ([FigJam](https://help.figma.com/hc/en-us/articles/1500004414322-Sticky-notes-in-FigJam)). tldraw notes keep a fixed width, grow vertically, and have **clone handles** (+ on each edge) ([tldraw](https://tldraw.dev/sdk-features/note-shape)). Miro's auto font size shrinks text to fit, which produces long-running "lock text size" requests ([Miro idea](https://community.miro.com/ideas/lock-text-size-on-sticky-note-2463)).
- **Tablet input.** Miro converts handwriting to text through Apple Scribble ([Miro tablet](https://help.miro.com/hc/en-us/articles/360017731633-Tablet-app)). Scribble did not work in FigJam on iPad ([Figma forum](https://forum.figma.com/ask-the-community-7/apple-pencil-scribble-feature-doesn-t-work-in-figjam-in-ipad-app-28524)). Miro users ask how to make Pencil writing *stick to* a sticky note ([Miro](https://community.miro.com/ask-the-community-45/can-i-adhere-text-and-apple-pencil-writing-to-a-sticky-or-shape-2793)). Ink written on a note should belong to that note and move with it.
- **Rich text.** tldraw added bold, italics, lists, links and code to every text tool and label in 2025. Excalidraw still has no WYSIWYG rich text: Markdown in pasted Mermaid becomes plain text ([#8430](https://github.com/excalidraw/excalidraw/issues/8430)).

## 6. Hand-drawn vs clean

- Excalidraw uses rough.js with three sloppiness levels: Architect (clean), Artist, Cartoonist. draw.io added rough.js as a per-shape or whole-diagram "Sketch" style, with jiggle and hachure controls and a sketch editor theme ([draw.io rough](https://www.drawio.com/docs/manual/styles/rough-style/)). D2 has a hand-drawn render mode ([D2](https://d2lang.com/blog/hand-drawn-diagrams/)). Lucid users keep asking for one ([Lucid idea](https://community.lucid.co/ideas/add-hand-sketch-look-8611)).
- Why it matters: a sketchy look "suggests it's a draft" and invites feedback ([HN](https://news.ycombinator.com/item?id=22102834)). The same diagram should be able to switch between sketch and clean as a **render style**, not as different geometry, and the sketch should not re-jitter every time a shape moves.
- **Mixing ink with diagrams.** Freehand ink costs a lot of performance. Miro names iPad ink, with its many points, as a cause of lag ([Miro](https://help.miro.com/hc/en-us/articles/360013588560-Board-performance-and-loading-issues)). tldraw switches strokes to simpler solid paths when zoomed out ([tldraw perf](https://tldraw.dev/sdk-features/performance)). Excalidraw captures one pen sample per frame and drops points when frames stutter ([PR](https://github.com/zsviczian/excalidraw/pull/440)).

## 7. Navigation and organization

- **Miro**: frames double as slides in presentation mode. There is a minimap (M), a grid of frames to jump between, and copy-link-to-object ([Miro frames](https://help.miro.com/hc/en-us/articles/360018261813-Frames)). Complaint: links to objects inside frames break, and the screen goes black, during presentation ([Miro idea](https://community.miro.com/ideas/in-presentation-mode-links-to-internal-objects-within-frames-should-work-4559)).
- **FigJam**: users want a table of contents built from section names ([forum](https://forum.figma.com/suggest-a-feature-11/implement-table-of-contents-navigation-for-sections-in-figjam-28871)).
- **Excalidraw** added element links and text search across the canvas in 2024, and presenter view and notes in 2026 ([2024](https://plus.excalidraw.com/blog/excalidraw-in-2024)).
- **draw.io** relies on pages and an Outline view, and recommends splitting big diagrams across pages for speed ([draw.io perf](https://groups.google.com/g/drawio/c/RKNBCRn6Yu0)).
- **tldraw** has deep links to shapes or positions and animates the camera between areas (`zoomToBounds`) ([camera](https://tldraw.dev/sdk-features/camera)).

## 8. Import and export

- **draw.io's best idea is editable images.** PNG, SVG and PDF exports can carry the full diagram XML inside the file (in PNG it goes in the zTxt chunk). Drag the image back onto the canvas and it opens as an editable diagram ([xml-in-png](https://www.drawio.com/docs/manual/export/xml-in-png/)). It also exports VSDX, HTML, a URL and JSON ([formats](https://www.drawio.com/docs/manual/export/export-diagram/)). One downside: draw.io XML is "walls of coordinates in pull requests", while Excalidraw's JSON diffs readably ([comparison](https://instapods.com/apps/excalidraw/vs/drawio/)).
- **Lock-in complaints.** FigJam cannot export SVG; the workaround is pasting into a Figma Design file ([Figma](https://help.figma.com/hc/en-us/articles/4407699832855-Export-your-FigJam-board)). Whimsical reviewers miss "solid SVG export". Miro's vector export of large boards drops images and produces illegible text ([Miro](https://community.miro.com/ask-the-community-45/exporting-content-via-vector-3767)).
- **Visio interop** is table stakes in companies. Even Lucid struggles to load very large Visio drawings ([Lucid](https://community.lucid.co/product-questions-3/slowness-when-loading-very-large-drawings-from-visio-106)).

## 9. Performance and scale

| Tool | Where users hit the wall |
|---|---|
| draw.io | Lag above ~1,000 objects or ~20 images. Rendering is SVG/DOM, and the official advice is to split into pages ([Google Group](https://groups.google.com/g/drawio/c/RKNBCRn6Yu0)) |
| Miro | 2,000+ elements slow; above 5,000, responsiveness is "extremely poor". Reports of 15–30 s lags ([community](https://community.miro.com/ask-the-community-45/boards-are-very-laggy-and-slow-on-desktop-app-and-web-app-12191)) |
| Lucidchart | Documents with many tabs and heavy content take minutes ([community](https://community.lucid.co/product-questions-3/lucidchart-is-slow-and-lagging-1037)) |
| Excalidraw | 5,000+ elements unresponsive; 1,000+ strokes lag on tablets ([#8136](https://github.com/excalidraw/excalidraw/issues/8136), [#9705](https://github.com/excalidraw/excalidraw/issues/9705)) |
| tldraw | Best web approach: an R-tree spatial index, off-screen shapes not rendered, level of detail (LOD), batched updates ([tldraw perf](https://tldraw.dev/sdk-features/performance)) |
| TALA / auto-layout | Runtime grows nonlinearly with diagram size ([TALA](https://d2lang.com/blog/tala-is-open-source/)) |

---

## Recommendations for our app

### Top ideas, ranked

1. **Two binding modes, chosen by pausing instead of modifier keys.** By default an endpoint attaches to the whole shape (floating, aimed at the center, ending on the edge). Pausing on a port or edge point pins it (tldraw's 600 ms / 320 ms model). Show the state on the endpoint (draw.io's dot vs X). Hold to override: no hover or keyboard needed.
2. **A pen stroke from shape A to shape B becomes a bound connector.** Rough boxes, diamonds and circles become shapes. Handwritten labels become text, either on a double-tap convert (MyScript) or by holding at the end of the stroke (Whiteboard). Bindings persist after conversion, and undo always restores the raw ink.
3. **Quick-add handles on the selection (touch version of draw.io's blue arrows).** Tap = clone and connect. Drag = place it yourself, then a shape picker. If a shape already exists in that direction, connect to it instead. Desktop mirror: Opt/Alt+Arrow, with Tab to cycle the shape type (Whimsical/Excalidraw).
4. **Orthogonal routing that avoids obstacles and respects edits.**
   - Re-route automatically only until the user moves a segment. Then pin that segment (Excalidraw), with a double-tap to release it.
   - A per-connector reroute policy (Visio).
   - Line jumps, a grid-aligned middle segment, and fanning out several edges that share a side.
5. **Three labels per connector (middle, source, target)** that move with the line and wrap. Also a full software arrowhead set: UML triangle and diamonds, crow's-foot zero/one/many.
6. **Snapping on by default, toggled from the UI.**
   - Edges, centers, equal gaps and gap-centering, applied on insert as well as on move.
   - FigJam-style **Tidy up**, with spacing handles you drag.
   - Align and distribute commands that account for connectors.
7. **Auto-layout on a selection.**
   - ELK hierarchical layout (top-to-bottom and left-to-right), tree, orthogonal, organic.
   - Animated (OmniGraffle). Only connected objects move, and pinned nodes stay put.
   - Stable rather than "optimal": adding a node should not reshuffle everything.
8. **Pasting Mermaid (later D2/PlantUML) creates native shapes inside a container that keeps the source** (draw.io). Use the generator's own layout (tldraw). Aim for two-way sync (Miro). Style overrides survive regeneration.
9. **Editable exports.** PNG, SVG and PDF that carry the scene data (draw.io), clean SVG always, draw.io XML import and export, and VSDX import. Plus a text-diffable native format.
10. **Containers that behave.** Frames clip their contents and move them along. Swimlanes and containers auto-fit their contents (Lucid "Fit"), and resizing a lane pushes its contents. Dropping a shape onto a connector splits it (Lucid/Visio).
11. **Sketch and clean as a document-level render style** with stable per-element randomness (Excalidraw/draw.io/D2). One diagram, two looks.
12. **Sticky notes that keep a fixed font size and grow vertically.** Tab / Cmd+Enter and clone handles create the next note. Pencil ink written on a note belongs to that note, and handwriting can convert to text.
13. **Structured software objects with free libraries.** UML class boxes with compartments, ER tables (with SQL import), a generated sequence diagram object, C4, BPMN, cloud icons, and user libraries. Never paywalled (Miro's mistake).
14. **Navigation**: frames as slides, an outline / table-of-contents panel, a minimap, search over all text including ink-recognized text, and deep links to objects that also work while presenting.
15. **Performance budget**:
    - A GPU renderer, spatial-index culling and LOD for ink.
    - Keep every pen sample: use coalesced touches instead of one sample per frame.
    - Re-route only the edges a change touches.
    - Target 10k+ objects smoothly. Miro and draw.io struggle at 1–5k.

### Five pitfalls to avoid

1. **"Smart" connectors that rewrite user intent.** Miro's eager snapping, Lucid's Smart Lines, and early Excalidraw's bind-to-everything were each reported as costing users hours. Never re-route a segment the user shaped, never auto-attach a loose line, and always offer a visible unbind.
2. **Designing for hover and modifier keys.** In tldraw, Excalidraw and FigJam, snapping and binding hang off Ctrl, Cmd or Alt, which a pen user cannot press. In iPad browsers, finger events stop entirely while the Pencil touches the screen ([tldraw](https://tldraw.dev/blog/a-touchy-subject)). Use pause, on-screen toggles and handles instead. Have a deliberate pen mode for palm rejection: pen draws, a finger pans.
3. **Lock-in.** No SVG export (FigJam), weak SVG export (Whimsical), lossy vector export (Miro), one-way Mermaid conversion (tldraw #10801). Make every export round-trippable.
4. **A generic canvas without diagram semantics.** About 20 shapes (tldraw), shape packs gated by plan (Miro), and connectors limited to one line per object (Freeform) push serious users back to draw.io. Diagram-type knowledge (ports, cardinality, compartments) must be built in.
5. **Falling over at scale.** That covers DOM/SVG rendering, ink with many points, dropped pen samples, and auto-layout that is nonlinear or non-deterministic. Users experience all of these as "the app got slow" and "my layout jumped".
