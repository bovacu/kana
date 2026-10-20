// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_RENDER
#define FUDE_ZOOM_RENDER

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/units.h"
#include "zoom/tex.h"

// ===========================================================================
// Render: draws a canvas (scene.h) on the screen.
//
// Where it sits: fude/zoom, over scene.h. The page (page.h) owns the renderer and draws the canvas with it each frame
// (page_render.c); the selection (select.c) draws lifted things and glows through it; the page's tools ask it which
// frames they may edit (fude_zoom_render_editable).
// Its main types and their lifecycle: the page owns one fude_zoom_renderer, made with fude_zoom_render_init and freed
// with fude_zoom_render_destroy. It keeps caches (decoded strokes and their meshes, pictures on the GPU, symbols',
// shapes' and boards' geometry, graphs' lines, texts' layouts) that fude_zoom_render_trim empties.
// Threads: main thread only, inside the app's 2D drawing block (it draws through the engine and loads textures).
// Design (docs/infinite_canvas_design.md §2.3):
//   - It draws from the highest frame above the camera that can still be drawn EXACTLY (where the f64 error of the
//     frame's transform to the screen, |translation| · 2^-52, stays under a quarter of a point: three levels for
//     ordinary content) down through every frame whose anchor box meets the screen, stopping where a frame would be
//     under half a pixel. Each frame's objects come from its index, in their order: later is on top, and a child frame
//     sits in its parent's order like a stroke.
//   - Above that frame is a BACKDROP: the colour of whatever is drawn round the camera's point up there (zoomed far
//     into a line, you are inside its colour). Should something drawn over the frame on the camera's way up there
//     cover the point (after it in the draw order: its layer's rank, then its order key), that colour goes over
//     everything. What a hidden layer holds covers nothing.
//   - All maths is done in f64 on the CPU, relative to the camera, so the GPU only sees screen-sized f32 numbers. A
//     stroke many screens wide (when you are deep inside it) is cut to the screen on the CPU first: its segments and
//     round joins become polygons clipped to a box round the screen, so no vertex is ever far out.
//   - Decoded strokes are kept (only visible ones are decoded) up to a budget in bytes, the least recently drawn let
//     go first; on a phone's low-memory warning they all go (fude_zoom_render_trim).
//   - A stroke's mesh (its triangles, as the engine tessellates it: render_stroke.c) is kept with its decoded points,
//     made in screen units round its translation on the screen: a still or panning view draws it again moved, and
//     only a change of its size or turn on the screen (a zoom) or of a pixel's size makes it again. Its own budget in
//     bytes, the one drawn longest ago let go first, never one drawn in the current draw.
//   - What is costly to make again is kept in its own units and drawn through its place each frame: a fill's, a
//     shape's, a cut board's and a symbol's triangles, a symbol's parts and a shape's outline, a graph's lines, a
//     note's layout (a plain text's lines are kept too, but at its size on the screen, as draw.c wraps them: while the
//     view is still). Each is kept by the whole of what it is made from (a payload's handle and serial, which change
//     whenever its bytes do; a text's words and sizes), so an edit is never drawn from what came before it, and each
//     cache lets go of the one used longest ago, never one used in the current draw: past its bound, what does not
//     fit is made each time, while what does stays kept (rather than all of it being made again).
// ===========================================================================

#define FUDE_ZOOM_RENDER_EXACT_PT   0.25    // the most error (pt) a frame drawn exactly may have
#define FUDE_ZOOM_RENDER_MIN_PX     0.5     // a frame smaller than this on screen is not gone into
#define FUDE_ZOOM_RENDER_EDIT_UP    4096.0  // tools reach a frame above the camera while its unit is at most this (pt)
// The budget for decoded strokes, in bytes.
#define FUDE_ZOOM_RENDER_CACHE      (64u * 1024u * 1024u)
// The budget for strokes' meshes kept (render_stroke.c), in bytes.
#define FUDE_ZOOM_RENDER_MESHES     (64u * 1024u * 1024u)
#define FUDE_ZOOM_RENDER_MIN_RADIUS 0.5f    // ink is never drawn thinner than this (pt), as on the ink page
#define FUDE_ZOOM_RENDER_TEXTURES   (256u * 1024u * 1024u)   // the most bytes of pictures on the GPU (4 a pixel)

// A frame drawn in the last draw, and how it was placed: the page maps the pen into it through to_screen.
typedef struct {
    u32           frame;
    fude_zoom_sim to_screen;
    // The frame's depth (scene.h: fude_zoom_scene_depth).
    u32           depth;
} fude_zoom_visible;

// A stroke or fill decoded for drawing, kept in the renderer's cache.
typedef struct {
    u32               object;
    u32               count;
    rde_arr TYPE(f32) xy;         // the points as x, y pairs, in frame units from the object's translation
    rde_arr TYPE(f32) radius;
    u32               used;       // the draw it was last used in
    // A fill's inside as triangles (fill.h): x, y for each corner, in the same units. Not initialised until made.
    rde_arr TYPE(f32) tris;       // x, y for each corner (see above)
    u32               tri_count;
    rde_arr TYPE(u32) rings;      // a fill's points' rings (fill.h; when not initialised, all are the outline's)
    // A cut fill's edge as it shows (fill.h's edges_rings): x, y for each point, in the same units. Not initialised
    // until made.
    rde_arr TYPE(f32) edges;      // x, y for each point (see above)
    rde_arr TYPE(u32) edge_lines; // the line each of those points belongs to
    u32               edge_count;
    // A stroke's mesh as last drawn whole (render_stroke.c's fude_zoom_render_stroke_mesh): tessellated in screen
    // units round its translation on the screen, so a pan draws it again as it is. What it was made from is kept with
    // it: the turn and scale of its way to the screen (mesh_a, mesh_b), its half-widths' scale (mesh_wscale), a
    // pixel's size (mesh_px); and how many points it was made from (mesh_points), the draw it was last drawn in
    // (mesh_used) and what it holds against the renderer's mesh budget (mesh_bytes). Not initialised until made.
    rde_stroke_mesh   mesh;
    f64               mesh_a, mesh_b, mesh_wscale;
    f32               mesh_px;
    u32               mesh_points;
    u32               mesh_used;
    u64               mesh_bytes;
    // The mesh's copy on the GPU (rde_rendering_2d_keep_stroke_mesh), made the first time the kept mesh is drawn again
    // unchanged (a pan): drawn from there, nothing of it uploaded per frame. A zeroed handle: none. Let go with the
    // mesh (render_stroke.c's fude_zoom_render_mesh_let_go, render.c's fude_zoom_render_let_go), and when the mesh is
    // made again (render_stroke.c's fude_zoom_render_stroke_mesh).
    rde_kept_stroke_mesh mesh_gpu;
} fude_zoom_decoded;

// Geometry the renderer keeps, in the thing's own units (before its place), made once and shared by everything it is
// the same for (render.c's kept geometry, one cache for all three): a symbol's parts as its kind makes them at its size
// and detail (render_symbol.c), a shape's outline at its detail, or a cut board's rings (render_shape.c); and the
// triangles of what is filled, so nothing is triangulated again while it stands still.
typedef struct {
    rde_arr TYPE(fude_zoom_v2)          points;      // a symbol's parts' points, a shape's outline, a board's rings' points
    rde_arr TYPE(fude_zoom_symbol_part) parts;       // a symbol's parts (not made for the others)
    rde_arr TYPE(u32)                   rings;       // a board's: the ring each point is on (not made for the others)
    rde_arr TYPE(fude_zoom_v2)          tris;        // its inside as triangles, three corners each (a symbol's: its
                                                     // filled parts', one part after another)
    rde_arr TYPE(u32)                   tri_range;   // a symbol's: for each part, its first triangle and how many (0: not
                                                     // filled; not made for the others)
    u32 parts_n;        // how many parts the symbol's maker said it draws
    b8  closed;         // a shape's outline closes
    b8  triangulated;   // tris are made: a symbol's with it, a shape's or a board's the first time a fill needs them
    u32 used;           // the draw it was last used in (never let go during that draw)
    u64 cost;           // its points and triangle corners (against render_parts.h's FUDE_ZOOM_RENDER_GEO_POINTS)
} fude_zoom_geo;

// A graph's lines as plot.h makes them from its text at its size. render.c keeps them while the graph stands still
// rather than making them again each frame, because an equation's grid or a curve followed finely is not quick. Which
// graph an entry holds is the renderer's plot_lru's key for its slot (its kind, text and half sizes, compared whole).
typedef struct {
    rde_arr points;   // fude_zoom_v2 (the arrays are made the first time the entry is made into)
    rde_arr parts;    // fude_zoom_symbol_part
    rde_arr labels;   // fude_zoom_plot_label
    u32     used;     // the draw it was last used in (never let go during that draw)
} fude_zoom_plot_made;

// A picture on the GPU, made from its payload's bytes (copies of the picture share it).
typedef struct {
    u32          blob;
    u32          serial;   // the payload's serial (scene.h: fude_zoom_blob), since the scene gives a freed slot out again
    // The texture, or NULL when the bytes could not be read as a picture.
    rde_texture* texture;
    // What it costs on the GPU, in bytes.
    u64          bytes;
    // The draw it was last used in.
    u32          used;
} fude_zoom_picture;

// The renderer: what the last draw went into, its caches, and what the page sets for drawing.
typedef struct {
    // The frames the last draw went into, and the pictures on the GPU with what they cost in all; picture_of finds a
    // picture's place in pictures by its payload (its blob handle in the high 32 bits, its serial in the low 32).
    rde_arr TYPE(fude_zoom_visible) visible;
    rde_arr TYPE(fude_zoom_picture) pictures;
    u64                             picture_bytes;
    rde_hash_map TYPE(u64, u32)     picture_of;
    // The decoded strokes and fills, and what they cost against their budget (FUDE_ZOOM_RENDER_CACHE).
    rde_arr TYPE(fude_zoom_decoded) cache;
    rde_arr TYPE(u32)               slot_of;    // for each object, its cache entry + 1 (0: none)
    u64                             cache_bytes;
    u64                             cache_budget;
    // The strokes' meshes kept in their decoded entries: what they hold against their budget (FUDE_ZOOM_RENDER_MESHES),
    // and the draw in which one found no room (no room is looked for again until the next draw). mesh_scratch is a
    // mesh drawn and not kept: a glow, a run of a stroke far bigger than the screen, one past the budget.
    u64                             mesh_bytes;
    u64                             mesh_budget;
    u32                             mesh_full;
    rde_stroke_mesh                 mesh_scratch;
    // Geometry kept (fude_zoom_geo): geos holds one for each slot of geo_lru, which knows whose each is (its key:
    // render_parts.h's fude_zoom_render_geo_key) and which was used longest ago, and one more after them, the spare,
    // which new geometry is made into (kept by moving it into a slot, or drawn from there when every slot was used in
    // this draw). geo_points is what the kept ones hold, in points and triangle corners.
    rde_arr TYPE(fude_zoom_geo)        geos;
    rde_lru                            geo_lru;
    u64                                geo_points;
    // Graphs' lines, kept (fude_zoom_plot_made): plots holds one for each slot of plot_lru, which knows which graph
    // each is (its key: the graph's kind, a 0x1F byte, its text, then its half sizes' bytes) and which was drawn
    // longest ago (the one made again into, past render.c's FUDE_ZOOM_RENDER_PLOTS), and one more after them, the
    // spare, made into when every slot was drawn in this draw; plot_key is where a key is built.
    rde_arr TYPE(fude_zoom_plot_made)  plots;
    rde_lru                            plot_lru;
    rde_arr TYPE(u8)                   plot_key;
    // How many draws so far (what the decoded strokes', pictures', geometry's and layouts' used fields count in).
    u32                             draws;
    // Scratch arrays, kept so they are not allocated again each time.
    rde_arr TYPE(u32)        found;
    rde_arr TYPE(rde_vec_2F) screen;
    rde_arr TYPE(f32)        radii;
    rde_arr TYPE(fude_zoom_qpoint) q;
    rde_arr TYPE(fude_zoom_v2)     shape;
    rde_arr TYPE(fude_zoom_v2)     fill;      // triangles, or a polygon cut to the screen
    rde_arr TYPE(fude_zoom_v2)     cut;
    rde_arr TYPE(fude_zoom_v2)     shape_screen;   // a shape's outline on the screen (any number of points)
    rde_arr TYPE(f64)              shape_radii;    // its line's radius at each (for a line far bigger than the screen)
    rde_arr TYPE(f64)              numbers;        // a shape's numbers, all of them (a cut board's run past the few)
    rde_arr TYPE(u32)              ring_ids;       // each point's ring, for what is triangulated in rings (fill.h)
    // Each frame's objects in their draw order as the draw goes down through the frames: a stack, each frame's run
    // above its parent's while it is drawn, cut back when it is done (render.c's fude_zoom_render_item).
    rde_arr                        items;
    // One u8 per object: when set, the canvas does not draw it, because the selection draws it, lifted while it is
    // dragged (select.h). NULL draws everything.
    const rde_arr* lifted;
    // One u8 per object: when set, it is drawn faint (while a recording plays, what was written after the point it has
    // reached: audio.h). NULL draws nothing faint.
    const rde_arr* faint;
    u32            skip;   // an object not drawn (FUDE_ZOOM_NONE: none): a text being written on the page draws itself
    b8             no_guides;   // guides are not drawn (when presenting, or making a picture of the page)
    // Graphs playing (plot.h): one u8 per object; when set, it is drawn as plot_play says (its time, its sliders).
    // NULL: none playing.
    const rde_arr*                    plot_mask;
    const struct fude_zoom_plot_play* plot_play;
    // What moves in a symbol as it plays (its FUDE_ZOOM_SYMBOL_MOVES parts, such as a solenoid's plunger): one f32 per
    // object, how far along its own width those parts are drawn from where they are (in its own units). NULL draws
    // them where they are.
    const rde_arr*                    moved;
    u32                               drawing;   // the object being drawn (render_symbol's)
    // The rulers stuck to the screen's edges at the last draw (fude_zoom_render_sheets_stuck): each band as drawn, its
    // sheet, and which it is (1 for the top one, along the sheet's X; 0 for the left one, up its Y).
    fude_zoom_box  stuck[4];
    u32            stuck_sheet[4];
    u8             stuck_top[4];
    u32            stuck_count;
    // The font texts and dimensions' numbers (shape.h) are written in (NULL: they are not written), at font_px.
    // mm_per_unit and units below say how many millimetres a unit of the canvas's home frame is, and how lengths are
    // written (units.h).
    rde_font*             font;
    f32                   font_px;
    // For notes (rich.h): the bold, italic and bold italic fonts (NULL uses the plain font), the links' colour, and the
    // texts' layouts kept: rich holds one for each slot of rich_lru, which knows whose each is (its key: the fonts and
    // sizes it is measured with, its width and alignment, then its words: render_text.c's
    // fude_zoom_render_rich_aligned) and which was drawn longest ago, and one more after them, the spare
    // (FUDE_ZOOM_RENDER_RICH of them in all, and the spare); rich_key is where a key is built.
    rde_font*             font_bold;
    rde_font*             font_italic;
    rde_font*             font_bold_italic;
    rde_color             link_color;
    rde_arr               rich;        // render_parts.h's fude_zoom_render_rich_kept, one per slot and the spare
    rde_lru               rich_lru;
    rde_arr TYPE(u8)      rich_key;
    struct fude_zoom_formulas* formulas;   // formulas' font and kept settings (formula.h; NULL: formulas not drawn)
    f64                   mm_per_unit;
    fude_zoom_units_style units;
    // Boards' materials' names, in shape.h's FUDE_ZOOM_MATERIAL_ order (NULL: they are not written).
    const c8* const*      material_words;
    // What the last draw cost (shown on the HUD); meshes_made: the strokes it tessellated (the rest of those it drew
    // came from their kept meshes).
    u32 strokes_drawn;
    u32 points_drawn;
    u32 frames_drawn;
    u32 meshes_made;
} fude_zoom_renderer;

// Draws a dimension on the screen (origin at the centre, Y up): it measures from _a to _b, with its line _offset points
// to their left, its arrows, and its number _label (NULL for none). Used for the canvas's dimensions and for the
// measure being made (page_render.c).
void fude_zoom_render_dimension(const fude_zoom_renderer* _r, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, f32 _width, rde_color _color, const c8* _label);

// Draws text in a box on the screen (origin at the centre, Y up): its lines are broken to fit the box's width and each
// is centred, the whole in the box's middle (or from its top, with _top). It is never cut, and may run past the box.
// Nothing is drawn without a font, or when _px (the letters' size) is under 3 points.
void fude_zoom_render_text_in(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, rde_color _color);
// The same, with each line from the box's left when _left is set.
void fude_zoom_render_text_at(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, b8 _left, rde_color _color);
// How fude_zoom_render_text_aligned places each line.
typedef enum { FUDE_ZOOM_ALIGN_CENTRE = 0, FUDE_ZOOM_ALIGN_LEFT, FUDE_ZOOM_ALIGN_RIGHT } FUDE_ZOOM_ALIGN_;
// The same, with each line placed as _align (FUDE_ZOOM_ALIGN_) says.
void fude_zoom_render_text_aligned(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, u8 _align, rde_color _color);

// Returns a guide's colour (shape.h's GUIDE): a cyan that shows on the current theme's page colour.
rde_color fude_zoom_guide_color(void);

// Draws a formula laid out by formula.h with _r's formulas, in _ink (nothing when they are not set, or _box is NULL).
// _em maps its ems (Y up, its baseline's left end at 0) to the screen. Each glyph is drawn as triangles at the detail
// its size asks for, each rule as two triangles; what is too small to see is left out. Used for formula text objects
// and the Formula card's preview.
void fude_zoom_render_formula(fude_zoom_renderer* _r, const fude_zoom_tex_box* _box, fude_zoom_sim _em, rde_color _ink);

// Returns a text's layout as a note (rich.h): its words _text (_len bytes), its letters _size, _width wide (0 leaves
// its lines as long as they are), in its own units (Y down from its words' top left). Returns NULL when the text has
// no marks (it is plain lines) or there is no font. The renderer keeps the layout; the pointer stays valid until the
// next call that lays out a text, or fude_zoom_render_trim.
const struct fude_zoom_rich* fude_zoom_render_rich(fude_zoom_renderer* _r, const c8* _text, u32 _len, f64 _size, f64 _width);
// The same, with its plain lines set to one side of its width (rich.h's fude_zoom_rich_lay_aligned: a diagram shape's
// label).
const struct fude_zoom_rich* fude_zoom_render_rich_aligned(fude_zoom_renderer* _r, const c8* _text, u32 _len, f64 _size, f64 _width, u8 _align);
// Returns how wide _len bytes of _text are in a note's style _style, its letters _px. It is rich.h's measure callback:
// _self is the renderer.
f32 fude_zoom_render_measure(any _self, const c8* _text, u32 _len, f32 _px, u8 _style);
// Returns a text object's layout (NULL when it is plain, or not a plain or sticky text), and writes its words' top left
// in its own units into _origin (x right, y up; a sticky note's padding in). The pointer stays valid as long as
// fude_zoom_render_rich's.
const struct fude_zoom_rich* fude_zoom_render_rich_of(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_v2* _origin);

// fude_zoom_render_init makes a renderer with empty caches; fude_zoom_render_destroy frees it and all it holds.
void fude_zoom_render_init(fude_zoom_renderer* _r);
void fude_zoom_render_destroy(fude_zoom_renderer* _r);
// Empties the caches (when the system is short of memory, or a canvas closes): the decoded strokes and their meshes,
// the pictures on the GPU, symbols' geometry, graphs' lines and texts' layouts. They are made again as they are drawn.
void fude_zoom_render_trim(fude_zoom_renderer* _r);

// Draws the scene from its camera on a screen _half wide each way (app units), and remembers which frames it drew.
// _paper is the backdrop's colour when nothing is drawn above. Call it inside a 2D drawing block.
void fude_zoom_render(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper);
// Called after fude_zoom_render, over everything: draws the rulers of each measured sheet the last draw went into,
// stuck to the screen's edges where the sheet's own are off the screen (sheet.h), so zoomed in on a sheet you still
// see where on it you are.
void fude_zoom_render_sheets_stuck(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half);

// Returns a picture object's texture, made the first time it is asked for, and writes its size in pixels into _size
// (which may be NULL). Returns NULL when it could not be made (not a JPEG or PNG the engine reads). The renderer owns
// the texture: it stays valid until fude_zoom_render_trim, or until it is let go to make room for another picture
// (never one used in the current draw).
rde_texture* fude_zoom_render_picture(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, rde_vec_2UI* _size);

// Returns symbol _kind's geometry at half sizes _hw × _hh and _segments (rounded up to a step of 8, what zooming
// changes little): its parts' points in its own units, its parts, and each filled part's triangles (fude_zoom_geo).
// Kept by the renderer, by its kind, sizes and detail, and shared with the canvas's symbols; the pointer stays valid
// until the next call that makes geometry, or fude_zoom_render_trim. The Library's tiles draw their symbols with it.
const fude_zoom_geo* fude_zoom_render_symbol_geo(fude_zoom_renderer* _r, u32 _kind, f64 _hw, f64 _hh, u32 _segments);

// Draws one object through _to_screen (from its frame's units to the screen): a stroke, a shape, a picture, a fill, a
// text, or a frame with everything in it. The selection draws its lifted things with it.
void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half);
// Draws an object again, _extra points wider on each side, all in _color: the selection's glow.
void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color);

// Appends to _out (an rde_arr of fude_zoom_visible) the frames the last draw went into that tools may change: those at
// the camera's depth and below, and those above it while one of their units is at most FUDE_ZOOM_RENDER_EDIT_UP
// points on screen (further up, a stroke's width runs to thousands of points). Returns how many it appended.
u32  fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out);

#endif
