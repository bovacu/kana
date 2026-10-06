// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_RENDER
#define FUDE_ZOOM_RENDER

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/units.h"

// ===========================================================================
// Drawing a canvas (scene.h) on the screen.
//
// From the highest frame above the camera that can still be drawn EXACTLY —
// the f64 error of its frame → screen transform, |translation| · 2^-52, under a
// quarter of a point, which is three levels for ordinary content — down through
// every frame whose anchor meets the screen, stopping where a frame would be
// under half a pixel. Each frame's objects come from its index, in their order
// (later on top, a child frame in its parent's order like a stroke).
//
// Above that frame: a BACKDROP — the colour of whatever is drawn round the
// camera's point up there (zoomed far into a line, you are inside its colour)
// — and, should something up there drawn LATER cover the point, that colour
// over everything.
//
// All maths in f64 on the CPU, relative to the camera; the GPU sees screen-
// sized f32. A stroke many screens wide (deep inside it) is cut to the screen
// on the CPU first: its segments and round joins as polygons clipped to a box
// round the screen, so no vertex is ever far out.
//
// Decoded strokes are kept (only the visible ones decode), up to a budget in
// bytes, the least recently drawn let go first; on a phone's low-memory warning,
// all of them (fude_zoom_render_trim).
// ===========================================================================

#define FUDE_ZOOM_RENDER_EXACT_PT   0.25    // the transform's error drawn exactly, at most
#define FUDE_ZOOM_RENDER_MIN_PX     0.5     // a frame smaller than this on screen is not gone into
#define FUDE_ZOOM_RENDER_EDIT_UP    4096.0  // a frame above the camera's: tools reach it while its unit is no more on screen
#define FUDE_ZOOM_RENDER_CACHE      (64u * 1024u * 1024u)
#define FUDE_ZOOM_RENDER_MIN_RADIUS 0.5f    // ink is never drawn thinner than this (pt), as on the ink page
#define FUDE_ZOOM_RENDER_TEXTURES   (256u * 1024u * 1024u)   // pictures on the GPU, at most (bytes: 4 a pixel)

// A frame drawn this time, and how: what the page maps the pen through.
typedef struct {
    u32           frame;
    fude_zoom_sim to_screen;
    u32           depth;
} fude_zoom_visible;

typedef struct {
    u32  object;
    u32  count;
    f32* xy;         // the points as x, y pairs, frame units from the translation
    f32* radius;
    u32  used;       // the draw it was last used in
    f32* tris;       // a fill's inside as triangles (fill.h): x, y each corner, the same units (NULL: not yet)
    u32  tri_count;
    u32* rings;      // a fill's points' rings (fill.h; NULL: all the outline's)
    f32* edges;      // a cut fill's edge as it shows (fill.h's edges_rings): x, y each point, the same units (NULL: not yet)
    u32* edge_lines; // each of those points' line
    u32  edge_count;
} fude_zoom_decoded;

// A picture on the GPU, made from its payload's bytes (copies share it).
typedef struct {
    u32          blob;
    rde_texture* texture;
    u64          bytes;
    u32          used;
} fude_zoom_picture;

typedef struct {
    rde_arr TYPE(fude_zoom_visible) visible;
    rde_arr TYPE(fude_zoom_picture) pictures;
    u64                             picture_bytes;
    rde_arr TYPE(fude_zoom_decoded) cache;
    rde_arr TYPE(u32)               slot_of;    // object → its cache entry + 1 (0: none)
    u64                             cache_bytes;
    u64                             cache_budget;
    u32                             draws;
    // Scratch.
    rde_arr TYPE(u32)        found;
    rde_arr TYPE(rde_vec_2F) screen;
    rde_arr TYPE(f32)        radii;
    rde_arr TYPE(fude_zoom_qpoint) q;
    rde_arr TYPE(fude_zoom_v2)     shape;
    rde_arr TYPE(fude_zoom_v2)     fill;      // triangles, or a polygon cut to the screen
    rde_arr TYPE(fude_zoom_v2)     cut;
    // Per object, set: not drawn by the canvas (the selection draws it, lifted
    // while dragged: select.h). NULL: everything drawn.
    const rde_arr* lifted;
    // Dimensions' numbers (shape.h): the font they are written in (NULL: not
    // written), how many millimetres a unit of the canvas's home frame is, and
    // how lengths are written (units.h).
    rde_font*             font;
    f32                   font_px;
    f64                   mm_per_unit;
    fude_zoom_units_style units;
    // Boards' materials' names (shape.h' FUDE_ZOOM_MATERIAL_ order; NULL: not said).
    const c8* const*      material_words;
    // What the last draw cost (the HUD's).
    u32 strokes_drawn;
    u32 points_drawn;
    u32 frames_drawn;
} fude_zoom_renderer;

// A dimension on the screen (centre origin, Y up): what it measures from _a
// to _b, its line _offset points to their left, its arrows and its number
// _label (NULL: none) — the canvas's, and the measure being made (page.c).
void fude_zoom_render_dimension(const fude_zoom_renderer* _r, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _offset, f32 _width, rde_color _color, const c8* _label);

// Text in a box on the screen (centre origin, Y up), its lines broken to fit its width and each centred, the
// whole in the box's middle (_top: from its top) — never cut. Nothing without a font, or under 3 points.
void fude_zoom_render_text_in(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, rde_color _color);
// ...each line from the box's left (_left) instead.
void fude_zoom_render_text_at(const fude_zoom_renderer* _r, const c8* _text, u32 _len, fude_zoom_box _box, f64 _px, b8 _top, b8 _left, rde_color _color);

void fude_zoom_render_init(fude_zoom_renderer* _r);
void fude_zoom_render_destroy(fude_zoom_renderer* _r);
// The decoded strokes let go (the OS short of memory, a canvas closed).
void fude_zoom_render_trim(fude_zoom_renderer* _r);

// The scene from its camera, on a screen _half wide each way (app units).
// _paper: the backdrop's colour when nothing is up there. Inside a 2D drawing block.
void fude_zoom_render(fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper);

// A picture's texture (made the first time), and its size in pixels. NULL: it
// could not be made (not a JPEG or PNG the engine reads).
rde_texture* fude_zoom_render_picture(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, rde_vec_2UI* _size);

// One object through _to_screen (its frame's units → the screen): a stroke, or
// a frame with everything in it — the selection's lifted things.
void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half);
// A stroke again, _extra pt wider each side, all in _color (a selection's glow).
void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color);

// The frames the last draw went into that tools may change — at the camera's
// depth and below, and above it while a unit there is at most
// FUDE_ZOOM_RENDER_EDIT_UP points on screen — into _out (fude_zoom_visible). Their count.
u32  fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out);

#endif
