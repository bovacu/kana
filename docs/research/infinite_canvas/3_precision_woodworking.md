# Precision drawing for woodworking templates: what to copy, what to avoid

Research for a tablet-first (Pencil + touch, also desktop) infinite-canvas app, covering the woodworking use case: accurate templates and plans that get printed at 1:1 or sent to a CNC or laser.

**Short version.** No app combines good pen feel, CAD-grade numbers and a reliable workshop printout:
- Note apps have pleasant rulers but nothing to measure with.
- CAD apps have exact numbers but assume a keyboard or a CAD mindset.
- Concepts and Morpholio Trace come closest on iPad.

Even the best of them are weak at the last step: printing a big template across A4 sheets at true scale. On iPad that step is close to broken at the OS level, and that gap is our clearest opportunity.

---

## 1. Entering exact values

**SketchUp's Measurements box (VCB) is the benchmark.** Start a line, move roughly in the right direction, then just type `600` or `3' 6 3/8"` and press Enter. You never click into a field; clicking into the box is the classic beginner mistake ([MasterSketchUp](https://mastersketchup.com/drawing-accurately-sketchup/)). Details worth copying:
- A unit can be typed on any value ([units](https://help.sketchup.com/en/managing-units-measurement)).
- Relative `<x,y,z>` coordinates ([drawing basics](https://help.sketchup.com/en/sketchup/introducing-drawing-basics-and-concepts)).
- Angles as slopes such as `6:12` ([protractor explainer](https://justnecessary.substack.com/p/the-sketchup-protractor-tool-decoded)).
- Precision down to 1/64".
- Best of all, a **`~` tilde on any displayed value that is rounded** ([forum](https://forums.sketchup.com/t/what-does-the-tilde-mean-in-measurements/24425)). For woodworkers, an honest "not exact" marker beats extra decimals.

**On iPad, SketchUp shows the box only when a number is expected** ([AEC Magazine](https://aecmag.com/concept-design/sketchup-for-ipad/)). It had to fix Pencil taps leaking to the canvas during entry, and it *removed* Apple Pencil Scribble from the box because handwriting broke value editing ([release notes](https://apps.apple.com/us/app/sketchup-3d-modeling/id796352563)). Lesson: don't rely on handwriting recognition for numbers; give the user a real numpad.

**CAD command lines** use `x,y`, `@dx,dy` and `@dist<angle` ([QCAD](https://www.qcad.org/doc/qcad/latest/reference/en/scripts/Widgets/CommandLine/doc/CommandLine_en.html)). Users begged for relative entry without the `@` to be the default ("nobody drafting… uses abs coordinates"), and QCAD added it ([forum](https://forum.qcad.org/viewtopic.php?t=8793)).

**Fusion 360** shows on-canvas length and angle boxes while you draw. Tab locks one so the mouse can't change it ([Product Design Online](https://productdesignonline.com/tips-and-tricks/how-to-use-the-line-command-in-fusion-360-secrets-revealed/)). This is the best pattern for "length *and* angle".

**Shapr3D on iPad is a cautionary tale.** You tap a parameter label to get a numpad or calculator, with expressions and named variables ([variables](https://support.shapr3d.com/hc/en-us/articles/18320182069916-Variables-and-expressions)). Two updates then caused revolts:
- **Fraction keypad removed.** Users were pushed onto the system keyboard, where iPadOS auto-replaced `1/8` with `⅛` and the app rejected it ([forum](https://discourse.shapr3d.com/t/inputting-fractions/36574)).
- **Pencil-only exact lengths broken.** After one update, exact lengths needed Tab on a hardware keyboard: "the Apple Pencil should be able to do everything" ([forum](https://discourse.shapr3d.com/t/cannot-set-exact-length/30241)).

**Concepts** sets exact values by tap-and-holding a measurement label, plus a "Measurement Popup" ([manual](https://concepts.app/en/manual/precision-tools)). It works but is hard to discover.

**Fractions matter commercially.** Woodworking calculators sell on fraction handling alone: One16 has a "denominator keyboard" and rounds to 1/16, 1/32 or 1/64 ([One16](https://codecarton.com/one16)). Fraction display is a recurring CutList Optimizer complaint ([reviews](https://mwm.ai/apps/cutlist-optimizer/6465744401)).

**Scale.** Concepts, Trace and Affinity let you calibrate from one known length: measure a segment on an imported PDF and say "this is 2.4 m" ([Concepts](https://concepts.app/en/tutorials/scale-and-measurement-concepts/), [Trace](https://morpholioapps.com/trace/), [Affinity](https://affinity.fandom.com/wiki/Measure_Tool)). Illustrator's dimension scale applies only to *new* dimensions and can't be changed later ([CreativePro](https://creativepro.com/using-the-dimension-tool-in-illustrator/)). Don't do that.

**Opinion.** Templates are 1:1: store mm as doubles and treat scale purely as a view/print setting. Build our own numpad:
- fraction keys `½ ¼ ⅛ 1/16 1/32`
- unit keys `mm cm in ″ ′`
- arithmetic
- recent values

Accept input such as `1' 3-3/8"`, `35cm` and `600/3`.

## 2. Virtual drawing instruments

What exists today:
- **Apple Notes:** one finger moves the ruler, two fingers rotate it ([Apple](https://support.apple.com/guide/ipad/write-and-draw-in-documents-ipad6350b8dc/ipados)). No measuring, no exact angle.
- **GoodNotes:** two-finger rotate; a double tap gives *Set Angle*, *Set Position*, *Hide Digits* and in/cm; ink snaps to the edge ([help](https://support.goodnotes.com/hc/en-us/articles/8254946748687-Draw-straight-lines-with-the-Ruler-tool)).
  - The "ruler and compass" request reached **7,423 votes** and was closed with only a ruler. Users still want a compass and set squares ([feedback](https://feedback.goodnotes.com/forums/191274-customer-suggestions-for-goodnotes/suggestions/39573643-ruler-and-compass)).
  - The ruler snaps back to 0° when you try to set a small angle to match a skewed scan ([feedback](https://feedback.goodnotes.com/forums/191274-customer-suggestions-for-goodnotes-apple/suggestions/48660938-turn-off-ruler-snap-to-zero)).
- **OneNote (Win10):** one finger moves, two rotate, **three fingers rotate in 5° steps**, plus a degrees field ([Microsoft](https://support.microsoft.com/en-us/office/draw-straight-lines-or-measure-with-the-ruler-in-onenote-for-windows-10-a50537fe-c097-4786-be47-a501c33b4bbb)).
- **Windows Ink:** the mouse wheel rotates the ruler and resizes the protractor ([InformIT](https://www.informit.com/articles/article.aspx?p=2873374&seqNum=6)). The Snipping Tool protractor measures an angle as you trace its rim, but **can't be resized once placed** ([LadEdu](https://ladedu.com/how-to-use-the-snip-and-sketch-protractor-to-measure-angles/)).
- **Whiteboard kits (Iolaos/Speechi)** are closest to a real *escuadra y cartabón* ([Speechi](https://speechi.com/teaching-geometry-iolaos-tools/)):
  - a stretchable ruler (7–30 cm) that **shows the drawn length live**
  - a resizable set square that switches to the 45° isosceles version with one button
  - a protractor with arms
  - a compass that shows its opening
- **Morpholio Trace:**
  - Ruler, angle-adjustable **Triangle**, and a Protractor that stretches into an ellipse template ([Trace](https://morpholioapps.com/trace/)). A double tap rotates an instrument 90° ([gestures](https://morpholioapps.com/userguide/trace/?advanced_hand_gestures)).
  - Its "Super Ruler" needs no instrument: strokes straighten with 15/30/45/90/∞ angle snap, and **tapping a finger mid-stroke toggles the snap** ([Morpholio](https://morpholio.medium.com/3-new-trace-features-you-need-to-know-da6fe47bea18)).
- **Concepts:** deliberately no ruler ([FAQ](https://tophatch.helpshift.com/hc/en/3-concepts/faq/113-where-is-the-ruler-in-concepts-how-do-i-measure-things/)). Instead there are traceable shape guides (line, arc, angle, ellipse, rectangle). A double tap "contains" the stroke so it can't overshoot the guide's handles ([manual](https://concepts.app/en/manual/precision-tools)). This is excellent for exact lengths.
- **Procreate QuickShape:** hold to straighten, add a second finger for 15° steps ([Procreate](https://help.procreate.com/procreate/handbook/guides/quickshape)).
- **Adobe Fresco** shows the failure mode: rulers "snap randomly to the place it wants", and users have wanted an off switch for years ([Adobe](https://community.adobe.com/questions-646/ruler-shapes-snapping-is-frustrating-307855)).

**Opinion.** Build real, scale-aware instruments:
- a ruler with ticks in document units
- 45° and 30/60° set squares
- a protractor
- a compass

Use the note-app gestures, with *soft* detents (0/15/30/45/90°) that engage only on slow rotation. Keep an always-visible angle readout that opens the numpad when tapped. The pen snaps to an edge only if the stroke *starts* near it, and a Concepts-style "contain" stops the stroke at the typed length.

The killer feature, which I found in no app: **instruments that slide along each other**. On paper, parallels and perpendiculars are drawn by sliding the cartabón along a fixed escuadra.

## 3. Snapping and inference

**SketchUp's inference engine** snaps to endpoints, midpoints, points on an edge and intersections, and to parallel, perpendicular and axis directions. You "encourage" an inference by pausing over a point, and lock it with Shift or the arrow keys ([SketchUp](https://help.sketchup.com/en/sketchup/introducing-drawing-basics-and-concepts)). Complaints:
- Users must "zoom in and out and 'feel' around a line until I touch the midpoint", and want AutoCAD-style one-shot snap overrides ([request](https://forums.sketchup.com/t/force-inference/241705)).
- Length snapping with coarse precision makes lines jump to the wrong point ([forum](https://forums.sketchup.com/t/disable-this-annoying-new-point-snapping-feature/7982)).

**Touch has no cursor.** On iPad, SketchUp makes you lock parallel or perpendicular *before* drawing ([AEC](https://aecmag.com/concept-design/sketchup-for-ipad/)). With **Apple Pencil hover** it unlocks "full inferencing" ([SketchUp iPad](https://help.sketchup.com/en/sketchup-ipad/getting-started)). Hover is the tablet's cursor; use it.

Other good ideas:
- **Affinity "snapping candidates":** only recently created or hovered objects are snap targets, which kills far-away noise ([docs](https://s3-eu-west-1.amazonaws.com/affinity-docs/help/photo/English.lproj/pages/DesignAids/snapping.html)).
- **Inkscape 1.2:** a *simple* three-option snap popover plus an advanced mode; alignment snapping labels distances ([release notes](https://wiki.inkscape.org/wiki/index.php/Release_notes/1.2)).
- **LibreCAD:** an exclusive one-snap mode and "snap distance" (a point N units from an endpoint) ([docs](https://docs.librecad.org/en/latest/ref/snaps.html)).
- **Concepts:** separates *snap to grid* from *align to grid* ([manual](https://concepts.app/en/manual/precision-tools)).
- **Affinity on iPad:** a movable on-screen **Command Controller** for modifier keys (Shift = 15° steps) ([Affinity](https://www.threads.com/@affinity/post/C-CyvS6oQIf)).

UI clutter causes snapping failures too. Shapr3D's constraint glyphs once sat on top of the nodes users needed to hit, making the iPad app "almost unusable" ([forum](https://discourse.shapr3d.com/t/shapr3d-ipad-unusable-because-constraints-tags/31758), [forum](https://discourse.shapr3d.com/t/constraint-buttons-preventing-sketching-from-nodes-with-apple-pencil/31552)).

**Opinion.**
1. Always *show* the snap target ("Midpoint", "⊥", "45°") before commit: via hover when available, otherwise after pen-down with adjust-then-lift.
2. Use a screen-space radius so snapping behaves the same at any zoom.
3. Use a strict priority: typed value > endpoint/intersection > midpoint > on-edge > ⊥/∥ > angle step > grid.
4. Only nearby or recent geometry is a candidate.
5. A resting finger bypasses snapping.
6. Give a haptic tick on Pencil Pro ([Apple Pencil](https://www.apple.com/apple-pencil/), [UICanvasFeedbackGenerator](https://developer.apple.com/documentation/uikit/uicanvasfeedbackgenerator)).
7. Length snapping is off by default.

## 4. Measuring

- **SketchUp tape measure** measures and drops guide lines. Type a new value afterwards and it offers to **resize the whole model** ([SketchUp](https://help.sketchup.com/en/sketchup/measuring-angles-and-distances-model-precisely)), which is perfect for calibrating a photo of a chair back. Its protractor goes vertex, then baseline, then a typed angle or slope.
- **Inkscape's measure tool** is the richest ([guide](https://note.com/inkscape_memo/n/nc383939324bd?hl=en)):
  - every segment length where the ruler crosses geometry, plus the angle
  - an optional "phantom" of the previous measurement
  - one-click conversion to guides, permanent items or a dimension line
- **Affinity's measure tool** handles distance, path length and area, but results are **not saved and not printable** ([wiki](https://affinity.fandom.com/wiki/Measure_Tool)). An architect complains the measurement "disappears when you move to another tool" ([forum](https://forum.affinity.serif.com/index.php?%2Ftopic%2F178620-dimension-lines-traits-de-cote%2F=)).
- **Concepts** attaches live labels to strokes (including area), and a tap makes them stick ([tutorial](https://concepts.app/en/tutorials/scale-and-measurement-concepts/)).
- **AR apps** (Apple Measure): "measurements are approximate", with a best range of 0.5–3 m ([Apple](https://support.apple.com/en-asia/guide/ipad/measure-iphd8ac2cfea/13.0/ipados)). Fine for rough room or stock notes, useless for templates.

**Opinion.** One Measure tool that does it all:
- point-to-point, with the same snapping as drawing, showing length, ΔX/ΔY and angle
- tap a path to get its length (for laminations and edge banding)
- tap two lines to get the angle between them
- temporary by default, with **one tap to pin it as a dimension**
- a "make this X" action that resizes the selection or calibrates the image

## 5. Dimensions and annotations

**Illustrator's Dimension tool** (2024) does linear, angular and radial dimensions. They are *associative*, following resizes, rotations and moves ([CreativePro](https://creativepro.com/using-the-dimension-tool-in-illustrator/), [UserVoice](https://illustrator.uservoice.com/forums/333657-illustrator-desktop-feature-requests/suggestions/47555246-the-dimension-tool-needs-to-have-live-updates)). The complaints:
- some users immediately wanted live updating switchable *off*
- dimensions don't copy with duplicated objects
- "expanding" a dimension silently detaches it
- the scale ratio is fixed after creation

**Morpholio Trace's "tap, tap, pull"** dimensions, dimension strings and angle notes are the right gesture weight for a pen ([Trace](https://morpholioapps.com/trace/)). **SketchUp** shows fractional inches, but woodworkers still ask for the `1-1/2"` hyphen style ([forum](https://forums.sketchup.com/t/dimensions-display-inches-only-no-feet-with-fractional-accuracy-1-2-1-4-etc/125062)).

**Opinion.** Associative by default, with an explicit "detach". Dimensions duplicate with their geometry. Each document has a dimension style: unit, precision (0.5 mm or 1/16"), fraction format, `≈` marker, arrow or tick ends.

Most important: **the dimension's number drives the geometry**. Tap "412", type "400", and the edge changes. That covers most of what woodworkers want from "parametric". Leader notes ("glue face", "6 mm roundover") are anchored text that follows the object.

## 6. Constraints: worth it?

**Fusion's model is heavy.** Lines are blue when under-constrained, black when fully constrained and red when over-constrained, and "how do I make blue lines black?" is a perennial beginner question ([Autodesk forum](https://forums.autodesk.com/t5/fusion-design-validate-document/how-to-make-blue-lines-black/td-p/7761791), [Varsity Tutors](https://www.varsitytutors.com/practice/subjects/autodesk-fusion-360/lessons/diagnosing-sketch-constraints)).

**Shapr3D is lighter.** It uses the Siemens D-Cubed solver and auto-applies horizontal/vertical, perpendicular, tangent and coincident constraints while you sketch; auto-constraining can be switched off ([Shapr3D](https://www.shapr3d.com/product/cad-constraints)). Dimensions are added by selecting geometry. Reviewers still note a learning curve ([AEC](https://aecmag.com/cad/review-shapr3d/)).

**Verdict for a 2D template tool:** no user-managed constraint system. Do ship "constraint-lite":
1. **Driving dimensions** (section 5).
2. **Implicit relations from snapping.** Joined endpoints stay joined, and lines drawn ⊥ stay ⊥. These are hidden by default and shown in a "relations" overlay on demand.
3. **Named variables** (`stock = 18`, `kerf = 3.2`) usable in any field, as in [Shapr3D](https://support.shapr3d.com/hc/en-us/articles/18320182069916-Variables-and-expressions). Change `stock` to the real 18.5 mm plywood and every slot updates.
4. **No colour-coded "under-constrained" state.** On a solver conflict, keep the last valid geometry and highlight the offending dimension.

## 7. Output for the workshop

**True-scale tiled printing is broken almost everywhere:**
- Inkscape's tiled-print request dates from **2004** and was never built in ([Launchpad](https://bugs.launchpad.net/inkscape/+bug/170274)). The workaround is laying out pages by hand ([Kevin Cox](https://kevincox.ca/2023/06/25/inkscape-poster-printing/)).
- Illustrator on iPad has **no tiling**; Adobe says to use a desktop ([Adobe](https://community.adobe.com/questions-15/printing-a-large-illustrator-design-to-scale-over-tiled-pages-on-an-ipad-7388)).
- Since iOS 17, AirPrint defaults to "Scale to Fit" with no visible way out ([Apple Community](https://discussions.apple.com/thread/255452275)). Pattern sellers say mobile devices can't reliably print at actual size ([Sussex Seamstress](https://www.sussexseamstress.com/sewing-tips-blog/printing-pdf-sewing-patterns-with-ios-android)).
- Woodworkers resort to SketchUp hacks with hand-made registration marks ([Fine Woodworking](https://www.finewoodworking.com/2010/03/30/printing-templates-two-approaches)).

**Copy the sewing-pattern conventions** ([Style Arc](https://www.stylearc.com/magazine/sewing-tutorials/how-to-assemble-style-arcs-print-at-home-tiled-pdf-patterns/)):
- a 10 cm / 1" test square, printed first
- "Actual size" printing
- a tile map
- lettered triangles on joining edges
- "trim the same two edges on every tile"

**Printers drift.** Consumer printers are accurate along the print-head (X) direction but slip along the paper feed (Y), which is why Rhino has separate X/Y scale factors ([McNeel](https://discourse.mcneel.com/t/cannot-print-1-1-scale-of-model/80321)). Calibrate by printing a known length and applying *expected / measured* ([Clint Brown](https://clintbrown.co.uk/2021/04/08/fusion-360-calibrating-11-prints/)). BlockLayer adds on-screen calibration: scale the diagram until it matches a ruler held to the screen ([BlockLayer](https://www.blocklayer.com/woodjoints/dovetaileng)).

**CNC and laser traps:**
- **SVG:** Inkscape assumes 96 px/in and Illustrator 72, so a 10 mm square arrives as 7.5 mm ([CCHS wiki](https://github.com/CCHS-Melbourne/Laser-Cutters/wiki/Knowledge-Base)).
- **DXF:** coordinates are unitless and `$INSUNITS` is often missing; a 25.4× error is the tell ([TechDraw](https://techdrawai.com/blog/dxf-wrong-size-fix)).
- **Geometry:** CAM wants closed polylines, no duplicates, and splines converted to arcs or polylines ([BOCO](https://bococustom.com/blogs/news/precision-fabrication-essential-steps-for-preparing-dxf-files-for-cnc-cutting)).

**Opinion.** Build an "Export for workshop" sheet with three modes:
1. **Tiled print:**
   - A4 or Letter
   - 10–15 mm overlap with crosshair registration marks
   - "B3" labels and a mini-map on every page
   - a test square and a 100 mm ruler on every page
   - PDF pages exactly the paper size, with content inside safe margins so iOS has nothing to scale
   - saved per-printer X/Y calibration from a one-time "print, measure, enter" wizard
2. **Full-size PDF** for a plotter or print shop.
3. **CNC/laser:**
   - SVG with mm `width`/`height` and a mm viewBox
   - DXF with INSUNITS set
   - layers by operation (cut, score, engrave, drill)
   - a pre-flight for open paths, duplicates and tiny segments

Also add a **True-size view**: device PPI is known, so 100 mm on screen can be 100 mm, letting the user hold a part against the iPad.

## 8. Woodworking-specific helpers

- **Cut lists:**
  - OpenCutList (free, for SketchUp) generates parts lists, cutting diagrams and labels with grain, edge banding and SVG/DXF export ([OpenCutList](https://extensions.sketchup.com/extension/00f0bf69-7a42-4295-9e1c-226080814e3e/open-cut-list)).
  - CutList Optimizer handles kerf, grain and banding. A Fine Woodworking author wanted it to **minimise table-saw fence changes** ([FWW](https://www.finewoodworking.com/2021/07/20/my-experience-with-cutlistoptimizer-com)).
  - SketchList 3D's "board-based" parts know their grain and thickness, and the cut list updates live ([SketchList](https://sketchlist.com/woodworking/)).
  - Takeaway: if shapes know they're boards, a cut list comes almost free.
- **Stock with real sizes:** a "2×4" is 1½ × 3½" ([Wikipedia](https://en.wikipedia.org/wiki/Lumber)). Keep nominal names *and* editable actual dimensions.
- **Joints:**
  - BlockLayer generates dovetail templates from board size, ratio (1:4–1:8) and tail count/spacing, and prints them at 100% ([BlockLayer](https://www.blocklayer.com/woodjoints/dovetaileng)).
  - boxes.py and MakerCase generate finger joints with **kerf compensation** and export SVG/DXF ([UMD wiki](https://sandbox.umd.edu/wiki/index.php/Laser_Box_Generator_Programs), [MakerCase](https://en.makercase.com/)).
  - Offer these as parametric "stamps" applied to an edge.
- **Grain direction:** an arrow attribute per part, printed on the template and used by the cut list.
- **Holes:** a hole-pattern tool with presets such as the **32 mm system with 37 mm setback** ([ToolsToday](https://toolstoday.com/learn/make-shelf-pin-holes)). Output centre-punch crosses on print and drill ops in exports.
- **Kerf and offsets:**
  - Woodworkers cut on the marked *waste side* ([Handyman's Daughter](https://www.thehandymansdaughter.com/saw-kerf/)), so cut lines should optionally shade a kerf-width band on the waste side.
  - Router templates need a **bushing offset** of (bushing Ø − bit Ø)/2: undersized for outside cuts, oversized for inside cuts ([Lee Valley](https://www.leevalley.com/en-us/discover/woodworking/2020/august/router-template-guides)).
  - An offset tool with kerf, bushing and bearing presets is cheap and very valuable.

---

## Recommendations for our app

### Ranked: the 15 most valuable ideas

1. **Type-anytime entry plus our own numpad.** Typing or tapping the live label sets length, and Tab moves to angle. Keys for fractions, units, arithmetic and variables. Relative coordinates by default. Never the system keyboard.
2. **A tiled 1:1 print pipeline that works on iPad.** Overlap, registration crosses, labels, a mini-map, a test square, paper-exact pages, and per-printer X/Y calibration. This is our clearest differentiator.
3. **Real instruments.** Ruler, 45° and 30/60° set squares, protractor and compass, all scale-aware. Slow-only soft detents, a tappable angle readout, pen-to-edge snapping, and stroke containment.
4. **Instruments that dock and slide along each other**: the escuadra-and-cartabón move, which no app offers.
5. **Predictable snapping.** Visible target before commit (Pencil hover), screen-space radius, strict priority, only nearby or recent candidates, finger-hold bypass, Pencil Pro haptics, length snapping off.
6. **Measure → pin → dimension.** Path length, the angle between two lines, radius.
7. **Driving dimensions** as constraint-lite: edit the number and the geometry follows, with no blue/black states.
8. **Honest units.** mm as doubles, per-document display precision, `1-3/8"` fraction style, `≈` on rounded values.
9. **Calibrate from a known length** for photos and PDFs (SketchUp's tape-measure resize).
10. **Named variables** (`stock`, `kerf`, `bit`) in every numeric field.
11. **An offset tool with woodworking presets**: kerf shading on the waste side, bushing offset, inside/outside.
12. **Clean CAM export.** mm SVG, DXF with INSUNITS, layers by operation, pre-flight checks.
13. **Board objects** with actual size, grain arrow and label, leading to a cut list later.
14. **Parametric stamps**: finger/box joints with kerf compensation, dovetails by ratio, 32 mm hole rows.
15. **A true-size screen view** for checking parts against the display.

### The 5 worst pitfalls to avoid

1. **Silent inexactness.** Rounded values without a marker, length snapping, or a scale frozen into dimensions (SketchUp, Illustrator). Woodworkers check prints with calipers.
2. **Snapping you can't escape.** Rulers that jump (Fresco) or snap back to 0° (GoodNotes), "feeling around" for inferences (SketchUp), glyphs covering snap points (Shapr3D). Every snap must be visible and instantly overridable.
3. **Breaking Pencil-only precision.** Relying on the system keyboard, dropping fraction keys, requiring a hardware keyboard (Shapr3D backlash), or using handwriting recognition for numbers (removed by SketchUp).
4. **Trusting the OS and the printer.** iPad scale-to-fit, no tiling, no test square, no calibration. A template 3% small is worse than none.
5. **Exporting ambiguous or dirty files.** 72 vs 96 dpi SVG, unitless DXF, open contours, duplicate lines. One wrongly sized laser cut destroys trust. A close sixth: forcing a Fusion-style constraint model on people who just want a jig template.
