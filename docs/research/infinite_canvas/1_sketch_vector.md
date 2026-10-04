# Sketching and vector art on an infinite, deep-zoom canvas: what the best apps do

Research date: October 2026. Scope: Concepts, Infinite Painter, Procreate, Adobe Fresco and Illustrator for iPad, Affinity Designer, Linearity Curve, Inkscape, Krita, Clip Studio Paint, ToonSquid, Excalidraw, tldraw (perfect-freehand), Apple Notes and Freeform, GoodNotes, Notability, Samsung Notes, Microsoft Whiteboard, Rnote, Xournal++, and the deep-zoom apps ZoomArt, Endless Paper and InfiniPaint.

**Limits on the sources.** My tools could not reach Reddit, and the help pages of Adobe, GoodNotes and Notability blocked direct fetches. For those I relied on search-engine excerpts of the pages and on App Store reviews, Apple Community, Krita Artists, Adobe UserVoice and GitHub issues. Points that are my own engineering opinion, not something an app does, are marked **(our take)**.

---

## 1. Freehand brushes

**Smoothing: the four algorithms everyone uses.**
- **Pulled string / lazy mouse.** The brush tip only moves once a virtual string goes taut, so you can draw slowly and still get sharp corners. Lazy Nezumi Pro uses this model and sets it beside *moving average* (soft corners), *exponential moving average* (very strong smoothing for long curves) and *inertia* (physics, for fast flowing lines) ([Lazy Nezumi](https://lazynezumi.com/smoothing)). Affinity's "Rope" stabilizer is the same idea ([iPad Calligraphy](https://ipadcalligraphy.com/tutorial/affinity-designer-ipad/)).
- **Velocity-adaptive lazy radius.** Infinite Painter's Lazy guide drags the brush tip behind the stylus on a "link", and the link's radius grows with stroke speed ([Creative Bloq](https://www.creativebloq.com/advice/infinite-painter)). This is clever because slow detail work stays responsive.
- **Procreate gives you three separate controls** ([Brush Studio](https://help.procreate.com/procreate/handbook/brushes/brush-studio-settings)):
  - *StreamLine* removes small wobbles and is aimed at inking.
  - *Stabilization* is a speed-dependent moving average.
  - *Motion Filtering* "deletes the extremities of a stroke's wobbles entirely without averaging". It comes with an *Expression* slider that puts some character back in.
- **perfect-freehand, used by tldraw and Excalidraw**, implements `streamline` as an exponential moving average. Each point is lerped toward the input with `t = 0.15 + (1 − streamline)·0.85` ([source](https://github.com/steveruizok/perfect-freehand/blob/main/packages/perfect-freehand/src/constants.ts)).
- **Krita's Stabilizer is the most complete design** ([Krita docs](https://docs.krita.org/en/reference_manual/tools/freehand_brush.html)):
  - sample counts that change with speed;
  - a *Delay* dead-zone so corners stay sharp;
  - *Finish line*, which catches the stroke up to the pen on lift;
  - *Stabilize sensors*, which also smooths pressure and tilt;
  - *Scalable distance*, which makes smoothing independent of zoom. On a zoom canvas this is essential.

**Concepts makes smoothing a property of the stroke, not just of the input.** The 0 / 50 / 100% slider runs from raw input, to polished, to a perfectly straight line between the endpoints. You can change it after drawing by selecting strokes ([Concepts manual](https://concepts.app/en/manual/brushes-and-tools)). Linearity's Pencil also lets you change smoothness after the fact ([Linearity](https://www.linearity.io/academy/curve/ipad/user-guide/vector-editing/drawing-tools/)), and so does Inkscape through its *LPE Simplify* live path effect ([learnwithmn](https://learnwithmn.com/tutorial/pencil-tool-inkscape/)).

**Tapering.**
- Procreate has *Pressure Taper* (artificial taper length plus a "tip" size) and *Touch Taper*, so finger strokes also taper. A *Tip animation* toggle decides whether the taper shows while drawing or only on lift ([Procreate](https://help.procreate.com/procreate/handbook/brushes/brush-studio-settings)).
- perfect-freehand has start and end `taper`, `cap` and easing settings. Its `simulatePressure` derives width from velocity when there is no stylus ([perfect-freehand](https://github.com/steveruizok/perfect-freehand)).
- Infinite Painter uses "stroke profiles": size and flow curves over the beginning, middle and end of a stroke ([docs](https://docs.infinitestudio.art/painter/brushes/settings/stroke/)).
- Inkscape offers caps up to "zero width" ([learnwithmn](https://learnwithmn.com/tutorial/pencil-tool-inkscape/)).

**How vector apps keep strokes editable.**
- Concepts stores a vector centerline plus brush parameters. Pixel nibs and grain are rendered *along* that path, so you can "take the same inked path and remap new brush effects to it" ([Concepts brush designer interview](https://concepts.app/en/stories/digital-brush-design-splash-matthew-baldwin/)). Its Nudge tool pushes a stroke "like a piece of string" ([Nudge](https://concepts.app/en/tutorials/nudge-tool/)).
- Inkscape's PowerPencil stores the width variation as a PowerStroke path effect ([Inkscape 1.0 notes](https://wiki.inkscape.org/wiki/index.php/Release_notes/1.0)).
- tldraw stores the input points (x, y, pressure) delta-encoded in base64. It ends a shape and starts a new one at 600 points (`maxPointsPerShape`) ([tldraw](https://tldraw.dev/sdk-features/draw-shape)).
- Krita is the cautionary tale: its brush tools don't work on vector layers at all. The vector "freehand path" tool has no stabilizer and no pressure ([Virtual Curiosities](https://www.virtualcuriosities.com/articles/1411/overview-of-drawing-tools-in-krita)), and this has been a "wishbug for almost ten years" ([Krita Artists](https://krita-artists.org/t/drawing-freehand-on-vector-layer/1179)).

**Textured brushes in vector apps** work when the texture is a *rendering* of the path (Concepts' pixel nibs), not *geometry*. ToonSquid warns that detailed textures in vector shape brushes create too many control points, and that vector strokes "don't support brush textures with soft edges very well" ([ToonSquid](https://toonsquid.com/handbook/brushes/vector/)).

**Brush size against zoom.**
- In Concepts, tool size is relative to the screen, so zooming in gives you a finer line in canvas terms. A separate *Wire* tool keeps a constant on-screen width at any zoom ([Concepts](https://concepts.app/en/manual/brushes-and-tools)).
- tldraw's "dynamic size" mode scales new strokes inversely with zoom ([tldraw](https://tldraw.dev/sdk-features/draw-shape)).
- **(our take)** This is the core interaction of a ZoomArt-style app. Brush size, smoothing distances and snap thresholds must be defined in *screen points at the moment of drawing* and then stored in world units. A "hairline", non-scaling stroke style is also worth offering for diagrams.

**Complaints.**
- Excalidraw users on iPad say pressure and tilt "change line width unintentionally" and ask for a fixed-width toggle ([#11322](https://github.com/excalidraw/excalidraw/issues/11322)).
- ZoomArt's top complaints: a single brush, weak pressure, no stabilization ([reviews](https://apps.apple.com/us/app/endless-zoom-canvas-zoomart/id6670361104?see-all=reviews&platform=iphone)).
- Endless Paper: "No smoothing options", "brush options are non-existent" ([reviews](https://apps.apple.com/us/app/endless-paper/id1294105620?see-all=reviews&platform=ipad)).
- Deep-zoom apps so far have weak brush engines. That is our opening.

## 2. Bezier and pen tools on touch

- **Inkscape** has the richest set of pen *modes*: Bézier, Spiro (very round), BSpline (easy, evenly smooth curves), straight polyline and paraxial ([manual](https://inkscape-manuals.readthedocs.io/en/latest/pen-tool.html)).
  - The Pencil adds smoothing from 1 to 100 and an experimental *sketch mode* that averages several strokes into one ([pencil manual](https://inkscape-manuals.readthedocs.io/en/latest/pencil-tool.html)), plus Ctrl+L Simplify.
  - Its touch support is poor: under GTK3 on Windows the pen is reported as a mouse, so stray lines and missing gestures are common ([inkscape-devel](https://lists.inkscape.org/hyperkitty/list/inkscape-devel@lists.inkscape.org/thread/SMVKQ6WJL2OKKRCLKBBJ6YKIESKWIE6Q/)).
  - Copy the *modes*, not the UI. BSpline-style "tap to place control points" is the most finger-friendly way to build a curve.
- **Linearity Curve** has the cleanest touch pen ([guide](https://www.linearity.io/academy/curve/ipad/user-guide/vector-editing/drawing-tools/)):
  - tap for a corner, drag for a curve;
  - double-tap the end node, or press "Finish Path", to end;
  - select an open path to continue it from its red end node;
  - "hold a finger on your canvas" to snap to horizontal or vertical.
  - Brush strokes are deselected after drawing so you don't edit them by accident. The finger-held-as-modifier pattern is worth copying.
- **Affinity Designer for iPad** uses an on-screen *Command Controller* in place of Shift, Ctrl, Alt and Cmd. Each button can be inactive, held or locked ([Affinity](https://www.threads.com/@affinity/post/C-CyvS6oQIf)). Users still ask for a true press-and-hold behaviour ([forum](https://forum.affinity.serif.com/index.php?/topic/227307-how-to-touch-and-hold-commandcontrolshiftalt-buttons-rather-than-toggle/)).
  - The Pencil's **Sculpt** mode lets you redraw over part of a path to fix it ([iPad Calligraphy](https://ipadcalligraphy.com/tutorial/affinity-designer-ipad/)). This is the single most valuable editing trick for freehand vector work.
- **Illustrator on iPad** uses a "touch shortcut" ring (the inner ring breaks a handle) and double-tap on an anchor to switch corner and smooth.
  - Its Pencil is widely disliked: 301 votes for desktop-style options. It closes paths into shapes ("I want to draw a line. I want to NOT draw a circle") and has no redraw and no Smooth tool ([UserVoice](https://illustrator.uservoice.com/forums/931888-illustrator-ipad-feature-requests/suggestions/41681191-pencil-tool-options-like-on-desktop-illustrator), [Adobe community](https://community.adobe.com/t5/illustrator-on-the-ipad-discussions/feature-suggestion-for-illustrator-on-the-ipad-smooth-tool/m-p/11736879)). Users say they moved to Affinity because of it.
- **Infinite Painter** lets you build a Bézier path (its *Pen guide*) and then stroke any brush *along* it. Detected shapes include "smooth path" and "polyline" ([features](http://infinitepainter.wikidot.com/features), [shape detection](https://docs.infinitestudio.art/painter/shapes/detection/)).
- **Clip Studio Paint** fixes vector lines after drawing with *Simplify vector line* and *Correct line width* (thicken or narrow a section) ([Clip Studio](https://support.clip-studio.com/en-us/faq/articles/20200031)).
- **Curve fitting.** Linearity's Auto Trace now shows a live preview before you commit ([auto trace](https://www.linearity.io/academy/curve/ipad/user-guide/images/auto-trace/)), and Affinity Studio added vector trace on iPad ([iPad Calligraphy](https://ipadcalligraphy.com/affinity/affinity-studio-vector-trace/)).
  - **(our take, from background knowledge, not fetched)** The standard way to fit freehand strokes is Schneider's least-squares cubic fitting (Graphics Gems, 1990): split at corners, fit, then recursively split segments whose error is too high. Fit with an error tolerance in *screen* pixels at the zoom you drew at.
  - Keep the raw samples so smoothing and fitting can be re-run later, as Concepts and Linearity allow.

## 3. Shapes

**Hold-to-snap is the standard.**
- **Procreate QuickShape** ([handbook](https://help.procreate.com/procreate/handbook/guides/quickshape)):
  - Draw, then keep holding. The stroke snaps to a line, arc, polyline, ellipse, triangle or quadrilateral.
  - *Still holding*, add a second finger to make it perfect: square, circle or equilateral triangle.
  - Drag to scale and rotate. A second finger during the drag gives 15° steps.
  - After release, "Edit Shape" exposes nodes.
  - The delay is adjustable, and Pencil Pro *squeeze* can trigger it instead of the hold ([prefs](https://help.procreate.com/procreate/handbook/actions/actions-preferences)).
- **Concepts** recognises shapes drawn with **one to four strokes**, including arrows. You keep scaling and rotating without lifting, and the activation time is adjustable ([Precision tools](https://concepts.app/en/manual/precision-tools)).
- **Infinite Painter** waits about 1 s and knows eight types, including smooth path, polyline and irregular quadrangle. Closed polygons with more than four corners become ellipses.
  - The shape **stays live until you tap away**, so you can change brush or size and see the result. A *stamp* button drops copies ([docs](https://docs.infinitestudio.art/painter/shapes/detection/)).
- **Others.** Apple Notes snaps a shape when you pause at the end of a stroke ([Apple](https://support.apple.com/en-us/108919)). Microsoft Whiteboard's Ink to Shape includes pentagons, hexagons and parallelograms ([Guiding Tech](https://www.guidingtech.com/top-microsoft-whiteboard-tips-tricks/)). Samsung Notes has "Neat shapes" ([Samsung](https://www.samsung.com/us/support/answer/ANS10003634/)). Xournal++ has a shape-recogniser mode and a ruler mode ([wiki](https://github.com/xournalpp/xournalpp/wiki/User-Manual)).

**Explicit shapes and guides.**
- Concepts *Shape Guides* (line, arc, angle, ellipse, rectangle) are stencils you draw along. **Double-tapping** the guide applies a constraint: half circle, 90°, perfect circle, perfect square ([Precision tools](https://concepts.app/en/manual/precision-tools)).
- Concepts snap options include *Snap to Grid*, *Align to Grid*, *Allow traceback* and *Auto-complete*, which joins a stroke's start and end. It also has perspective and isometric grids.
- Procreate's *Drawing Assist* bends strokes to 2D, isometric, perspective or symmetry guides ([Procreate](https://procreate.com/insight/2018/drawing-guides)).
- Krita's assistants add parallel and infinite rulers, spline, concentric ellipses and fisheye ([Krita](https://docs.krita.org/en/user_manual/painting_with_assistants.html)).
- Rnote's shaper lets you cycle constraints (1:1, 3:2, golden ratio, level, upright) while drawing ([Rnote PR](https://github.com/flxzt/rnote/pull/1829)).
- tldraw: Shift switches to straight segments snapped to 15° within a freehand stroke. Strokes that end near their start close automatically and take the fill style ([tldraw](https://tldraw.dev/sdk-features/draw-shape)).

**Complaints.** Freeform's shape tool "only works on predefined shapes, which can't be adapted" ([Apple Community](https://discussions.apple.com/thread/255211398)). ZoomArt users ask for "straight lines, shapes… color filling" ([reviews](https://apps.apple.com/us/app/endless-zoom-canvas-zoomart/id6670361104?see-all=reviews&platform=iphone)).

**Constraints without a keyboard.** In order of how good they feel: (1) a second finger *during* the gesture (Procreate); (2) double-tap the guide or shape (Concepts); (3) hold a finger anywhere (Linearity); (4) sticky on-screen modifiers (Affinity).

**Filled versus outline.** Make fill a style of any closed shape or closed freehand stroke (tldraw), not a separate tool. Concepts' Fill tool requires smoothing below 100%, which is a confusing coupling to avoid.

## 4. Eraser: whole-stroke versus partial in a vector app

| App | Modes |
|---|---|
| Concepts | *Slice* cuts away what it touches; at width 0 it splits strokes without removing anything. *Hard/Soft Mask* hides without destroying ([manual](https://concepts.app/en/manual/brushes-and-tools)) |
| Fresco | Vector eraser. *Vector Trim* via the Touch Shortcut: swipe once to trim back to intersections, scribble 3× to delete the whole stroke ([Adobe](https://helpx.adobe.com/fresco/using/erasers.html), [community](https://community.adobe.com/t5/fresco-discussions/eraser-in-adobe-fresco/td-p/10680134)) |
| Clip Studio | Touched area / **Up to intersection** / Whole line ([tips](https://tips.clip-studio.com/en-us/articles/7572)) |
| ToonSquid | Trims to the overlap; deletes the whole stroke if there is no overlap ([handbook](https://toonsquid.com/handbook/brushes/vector/)) |
| GoodNotes 6 | Precision / Standard / Stroke, "erase highlighter only", *Auto-deselect* returns to the previous tool on lift ([support](https://support.goodnotes.com/hc/en-us/articles/7353718249231-Erase-content-with-the-Eraser-tool)) |
| Apple Notes / Markup | Pixel and Object eraser; Pencil tilt changes eraser size ([Apple](https://support.apple.com/en-my/guide/ipad/ipad6350b8dc/ipados), [Paperlike](https://paperlike.com/blogs/paperlikers-insights/apple-notes-review)) |
| Samsung Notes | "Erase by line" (default) / "by area" ([Guiding Tech](https://www.guidingtech.com/guide-using-samsung-notes-app/)) |
| Xournal++ | Standard (splits strokes) / Whiteout / Delete stroke ([docs](https://xournalpp.github.io/guide/tools/eraser/)) |
| Rnote | Trash colliding / Split colliding. Split doesn't work on shapes yet ([#1162](https://github.com/flxzt/rnote/issues/1162)) |

**How users feel about it.**
- Whole-stroke-only erasing drives people away. In a FigJam thread one user says "Erasing the whole line is so frustrating and very inefficient, which means I have to look elsewhere" ([Figma forum](https://forum.figma.com/suggest-a-feature-11/why-not-a-figjam-pixel-eraser-like-everyone-else-7319)).
- A naive split that leaves hard, broken stroke ends also gets complaints: users want it to "rub out" like a paint eraser ([frappe/draw #569](https://github.com/frappe/draw/issues/569)).
- Endless Paper's eraser "erases erratically and unevenly" ([reviews](https://apps.apple.com/us/app/endless-paper/id1294105620?see-all=reviews&platform=ipad)).
- Excalidraw's missing support for the stylus eraser button has 58+ upvotes ([#9705](https://github.com/excalidraw/excalidraw/issues/9705)).

**(our take)**
- Ship partial erase (the default for sketching), whole-stroke erase, and Clip Studio and Fresco's *trim to intersection*, which is excellent for line art.
- Partial erase cuts the *centerline* at the eraser disc. The new ends should get the brush's normal round end, not a taper, so the result looks rubbed out, not chopped.
- For textured or soft brushes, a non-destructive mask in the Concepts style looks better.
- Switching: Pencil double-tap or squeeze toggles the eraser, "auto-return to the last tool" is on by default, and a long-press on the eraser icon picks the mode.

## 5. Lasso selection and transforms

- **Concepts sets the bar** ([Selection](https://concepts.app/en/manual/selection)):
  - Selection modes: lasso, item picker (tap single strokes to add or remove them) and *color picker*, which selects strokes by color and vector properties.
  - **Tap and hold anywhere** to select without switching tools; a second finger toggles the mode.
  - Filters for partial selection, locked strokes, and active layer versus all layers.
  - The popup offers Copy, Duplicate, Group, Lock, Delete and Flip. Pinch and twist the selection to scale and rotate it; tap the corner handles for distort, skew and warp.
  - A selection can be recoloured, resized or switched to another brush through the tool wheel.
- **GoodNotes** has "Included in Selection" toggles (handwriting, images, text, stickers), a freehand or rectangle lasso, and actions including recolor, resize and rotate. *Circle-to-Lasso* means circling something with the pen selects it, with no tool switch ([lasso](https://support.goodnotes.com/hc/en-us/articles/7353695644175-Select-Move-and-Edit-Content-With-the-Lasso-Tool), [gesture](https://support.goodnotes.com/hc/en-us/articles/7443556793103-Select-content-with-the-Circle-to-Lasso-pen-gesture)).
- **Constraints.** Affinity uses the Command Controller for straight-line moves and 15° rotations ([Affinity](https://www.threads.com/@affinity/post/C-CyvS6oQIf)). Procreate uses a second finger for 15° steps on QuickShapes.
- **(our take)** Deep zoom raises a problem none of these apps has. A lasso drawn at one zoom level can catch thousands of strokes that are sub-pixel in size because they live deep inside a detail. Add a "visible-size threshold" filter, on by default, that skips objects smaller than about 1 px on screen. Also draw transform handles at a constant screen size.

## 6. Images

- **Concepts.** Import from photos, files or the clipboard. Fade an image with the opacity slider while it is selected, and Lock it so you can trace over it ([basics](https://concepts.app/en/learn-the-basics-of-concepts/), [manual PDF](https://concepts.app/downloads/concepts-manual-4.3.pdf)). Users complain that PDF slides have to be imported "one by one" ([App Store](https://apps.apple.com/us/app/concepts/id560586497)).
- **ZoomArt** 2.0 added image import with duplicate, rotate, flip and lock, but only for raster formats. Users ask for PDF and SVG ([App Store](https://apps.apple.com/us/app/endless-zoom-canvas-zoomart/id6670361104)).
- **Freeform** can crop an image or mask it with any shape, and objects can be locked ([MacMost](https://macmost.com/masking-images-with-shapes-in-keynote-and-freeform.html)). Users complain it has no opacity control for images ([Apple Community](https://discussions.apple.com/thread/255211398)).
- **Floating references.** Procreate's *Reference Companion* is a floating window holding the canvas, an image or the camera, and it doesn't move when you pan or zoom the canvas ([Procreate](https://help.procreate.com/articles/ZWopfZ-reference)). Infinite Painter supports several resizable reference windows ([features](http://infinitepainter.wikidot.com/features)).
- **(our take)** On a deep-zoom canvas a floating reference beats a placed image when you are working deep inside a detail. Placed images need a mipmap or tile pyramid and should become a blurred placeholder below a size threshold. "Trace this image" should run auto-trace with a live preview, as Linearity does.

## 7. Layers, or alternatives

- **Concepts** creates layers *automatically per tool type* (pens share one, pencils share one). It switches to manual layers as soon as you reorder them, and double-tapping a layer enters *Focus mode*. The free tier has 5 layers ([Layers](https://concepts.app/en/manual/layers)).
- **Procreate's** layer count is limited by RAM times canvas size ([Procreate](https://procreate.com/insight/2021/layer-limits)). That is a raster problem a vector app avoids.
- **ZoomArt** uses layer count as a paywall (2–3 free, 30 paid), and users hate it ([reviews](https://apps.apple.com/us/app/endless-zoom-canvas-zoomart/id6670361104?see-all=reviews&platform=iphone)).
- **Freeform** users want folders and a way to organise boards, not layers ([Apple Community](https://discussions.apple.com/thread/255211398)).
- **(our take)** Global layers make little sense across a 10¹²× zoom range. Keep a few global layers (sketch, ink, color, reference) with lock, hide and opacity. Use **groups and frames** as the main organising tool: a frame owns everything drawn inside it at deeper zoom, so it can be moved, hidden or exported as a unit. Never put the layer count behind a paywall.

## 8. Infinite canvas and deep zoom

**Navigation.**
- **Concepts** has an *infinite canvas but not infinite zoom*. It tops out around 1600% "due to technical and usability considerations" ([FAQ](https://tophatch.helpshift.com/hc/en/3-concepts/faq/260-does-concepts-support-infinite-zoom/)). Good touches:
  - zoom snaps at 10, 25, 50 and 100%;
  - **grey edge arrows that point to off-screen strokes**, so a tap takes you back to your work;
  - double-tap the zoom value to recenter ([Infinite Canvas](https://concepts.app/en/manual/infinite-canvas), [lost-drawing FAQ](https://tophatch.helpshift.com/hc/en/3-concepts/faq/133-i-lost-my-drawing-on-the-infinite-canvas-how-do-i-find-it-again/)).
- **Freeform** added *Scenes* in iPadOS 18: saved views, including zoom level, that you jump to with a tap ([9to5Mac](https://9to5mac.com/2024/07/11/ipados-18-adds-one-new-feature-to-freeform-worth-trying-video/)).
- **Endless Paper** and **InfiniPaint** both use bookmarks ([Endless Paper](https://www.endlesspaper.app/), [InfiniPaint](https://infinipaint.com/)). Even with bookmarks, Endless Paper users report: "if you zoom too far out, you lose yourself to the point you can't find it" ([reviews](https://apps.apple.com/us/app/endless-paper/id1294105620?see-all=reviews&platform=ipad)).

**Performance and data loss, the biggest risk.**
- Freeform boards slow down as they fill with Pencil writing, with 2–3 s ink delay, crashes, battery drain, and PDF export that "just hangs" ([thread 1](https://discussions.apple.com/thread/254620888), [thread 2](https://discussions.apple.com/thread/255211398)).
- Excalidraw on iPad: "Significant performance degradation with 1000+ strokes" and freezes on load ([#9705](https://github.com/excalidraw/excalidraw/issues/9705)).
- Concepts: a crash that wipes recent changes ([App Store](https://apps.apple.com/us/app/concepts/id560586497)).
- Endless Paper: a board that "jumped back in time by at least a couple weeks" ([reviews](https://apps.apple.com/us/app/endless-paper/id1294105620?see-all=reviews&platform=ipad)).
- In contrast, Endless Paper advertises its "Fractile" vector engine at 120 fps "with millions of strokes on screen" ([site](https://www.endlesspaper.app/)). Performance is a feature people choose an app for.

**How true infinite zoom is built.** InfiniPaint is open source, and it is the one to study ([repo](https://github.com/ErrorAtLine0/infinipaint)):
- World coordinates are arbitrary-precision fixed-point numbers built on Boost `cpp_int` ([FixedPoint.hpp](https://github.com/ErrorAtLine0/infinipaint/blob/main/include/Helpers/FixedPoint.hpp)).
- Each object carries its own coordinate frame (`pos`, `inverseScale`, `rotation`), and its local geometry stays in plain floats ([CoordSpaceHelper.hpp](https://github.com/ErrorAtLine0/infinipaint/blob/main/src/CoordSpaceHelper.hpp)).
- **(our take)** Copy this pattern: big-number frame per stroke or per frame, float32 geometry inside it. Cull anything smaller than about 0.5 px on screen. Merge or rasterise distant content into cached tiles at each zoom level.

**Export.**
- Concepts exports JPG for free and charges for PNG, PSD, SVG and DXF ([App Store](https://apps.apple.com/us/app/concepts/id560586497)).
- InfiniPaint exports PNG, JPG, WEBP and SVG, plus a "screenshot" tool for any region ([repo](https://github.com/ErrorAtLine0/infinipaint)).
- ZoomArt exports "high-resolution images or animated videos" of the zoom, its signature shareable output ([App Store](https://apps.apple.com/us/app/endless-zoom-canvas-zoomart/id6670361104)).
- Linearity users furiously attacked locked export ("holding your existing work hostage") ([reviews](https://apps.apple.com/au/app/linearity-curve-vector-editor/id1219074514?see-all=reviews&platform=iphone)).

## 9. Undo and redo

- **Gestures.** Procreate and Concepts both use a two-finger tap to undo and a three-finger tap to redo. Procreate also lets you *hold* two or three fingers to step through history quickly. Procreate keeps 250 steps and **loses them when you leave the canvas** ([Procreate gestures](https://help.procreate.com/procreate/handbook/interface-gestures/gestures), [Concepts](https://concepts.app/en/ios/manual/yourworkspace)).
- **Accidental undo.** Two-finger tap can collide with pinch-zoom, and users ask for a way to turn it off ([Procreate Folio](https://folio.procreate.com/discussions/3/6/13132)). On a canvas you navigate all the time, that conflict is constant.
- **Endless Paper** promises unlimited undo with automatic saving, but the iPadOS 26 window controls overlapped its undo and redo buttons ([reviews](https://apps.apple.com/us/app/endless-paper/id1294105620?see-all=reviews&platform=ipad)).
- **(our take)** On an infinite canvas, *undo must move the camera* to show what changed. Otherwise users undo strokes off-screen without noticing. Camera moves must not be undo steps. History should be saved with the document, and a scrub-able history slider is a strong extra.

---

## Recommendations for our app

**Ranked ideas to copy**

1. **Zoom-relative tools.** Brush size, smoothing distance, snap radius and hit-testing are all in screen points at drawing time, then stored in world units (Concepts, Krita "scalable distance", tldraw dynamic size). Add a constant-width "wire" style for diagrams.
2. **Big-number coordinate frames per object, float geometry inside** (InfiniPaint), with sub-pixel culling and tile caches, aiming for Endless Paper-level smoothness with very large stroke counts. Performance is the feature.
3. **Hold-to-snap shapes that stay live while you hold**: second finger for a perfect shape, drag to scale and rotate, second finger for 15° steps (Procreate). Recognise 1–4 stroke shapes (Concepts), keep the shape editable until you tap away, with brush changes allowed (Infinite Painter). Adjustable delay; Pencil Pro squeeze as a trigger.
4. **Editable vector strokes that keep their raw input**, so you can change smoothing, brush, color and width after drawing (Concepts, Linearity). Add Nudge and Affinity-style *Sculpt* (redraw over part of a stroke to fix it).
5. **A stabilizer set**: StreamLine-style exponential moving average by default, plus pulled-string with velocity-adaptive radius (Lazy Nezumi, Infinite Painter) and a "finish line" catch-up on lift (Krita). Separate taper settings for pressure and for finger.
6. **Three eraser modes**: partial (default), whole stroke, and *trim to intersection* (Clip Studio, Fresco), with auto-return to the previous tool and support for Pencil double-tap, squeeze and the eraser button.
7. **Constant edge arrows to off-screen content, plus bookmarks or scenes, plus a "back to my last spot" control** (Concepts, Freeform, Endless Paper). For deep zoom add a breadcrumb of parent frames, because a flat minimap is useless across a 10¹²× range.
8. **Tap-and-hold anywhere to lasso without switching tools** (Concepts); filters by type, color and visible size (GoodNotes, Concepts); recolor, rebrush and duplicate in the selection popup.
9. **Frames and groups as the main way to organise, with only a few light layers.** Automatic per-tool layers (Concepts) are a nice option. Never paywall the layer count.
10. **Freehand-to-curve fitting with a screen-space error tolerance**, and a tap-to-place BSpline-style pen with double-tap to switch node type (Inkscape BSpline, Linearity, Illustrator).
11. **Guides you draw along**: line, ellipse and rectangle stencils that you double-tap to constrain, and perspective or isometric grids with Drawing Assist (Concepts, Procreate, Krita).
12. **Undo that moves the camera to the change**, history saved in the document, and hold-to-scrub (Procreate) without counting navigation as undo steps.
13. **A floating reference window** (Procreate, Infinite Painter), plus placed images with opacity, lock, crop or mask, and auto-trace with live preview (Linearity).
14. **Zoom-video export**, the ZoomArt signature and the most shareable output, along with PNG, SVG and PDF export of any frame or region (InfiniPaint).
15. **Pen-only drawing with a reliable palm and touch split**: when the Pencil is active, fingers only navigate; a fixed-width option for anyone who doesn't want pressure (Excalidraw complaints).

**The five worst pitfalls to avoid**

1. **Losing work.** Crashes that wipe changes (Concepts) and boards that roll back weeks (Endless Paper) destroy trust in an app. Save incrementally and journal every stroke.
2. **Slowing down as the canvas fills** (Freeform's 2–3 s ink lag, Excalidraw freezing beyond 1000 strokes). Design for stroke counts in the millions from day one.
3. **Whole-stroke-only erasing, or splits that look chopped** (FigJam, frappe/draw). Both make sketching miserable.
4. **Thin drawing tools around a great zoom gimmick** (ZoomArt: one brush, no shapes, no stabilizer; Endless Paper: no smoothing). Users notice within minutes.
5. **Hostile monetisation and unintended gestures.** Locked export, paywalled layers, account walls and ads are the top complaints about ZoomArt and Linearity. Accidental two-finger undo while zooming, and palm marks, are the top complaints about the gestures themselves.
