// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MAP_H
#define FUDE_ZOOM_MAP_H

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// The canvas from above (Sketching's Map, and Present): what is drawn, seen
// from the top of the canvas — its root frame, where whatever is deeper is a
// speck at its place — the view on it, the places marked, and the AREAS.
//
// An AREA is a region of the canvas with a name: a symbol of the diagram
// library's kind "area" (symbol.h: its outline, its name in its corner), made
// by Insert's Named area round what the lasso holds or the view. The map
// names them, a tap there frames one, and Present goes through them in the
// order they were made (or, with none, through the places by number).
//
// Here only what is where, in the root's units; the page draws the map and
// takes its taps (page.c, "the map").
// ===========================================================================

#define FUDE_ZOOM_MAP_AREAS 64u    // areas named on the map (and presented) at most
#define FUDE_ZOOM_MAP_NAME  96u    // an area's name's bytes kept, its NUL included
#define FUDE_ZOOM_MAP_PLACES 32u   // places shown at most

typedef struct {
    u32           object;                    // the area's symbol
    fude_zoom_box box;                       // root units
    c8            name[FUDE_ZOOM_MAP_NAME];
} fude_zoom_map_area;

typedef struct {
    u32          object;   // the place's MARK
    u32          number;   // "Place 3"
    fude_zoom_v2 at;       // root units
} fude_zoom_map_place;

// Is _object an area (alive, a symbol of kind "area")?
b8 fude_zoom_map_is_area(const fude_zoom_scene* _s, u32 _object);

// What is drawn, root units: the root's own things and its frames as far as their content reaches (not
// the places, nor the layers' own records, nor what is on a hidden layer, nor frames nothing was drawn
// in). Empty: nothing drawn.
fude_zoom_box fude_zoom_map_contents(const fude_zoom_scene* _s);

// The camera's view (the screen's half-size _half, points), root units.
fude_zoom_box fude_zoom_map_view(const fude_zoom_scene* _s, fude_zoom_v2 _half);

// The areas in the order they were made (on layers shown), their boxes in root units: how many.
u32 fude_zoom_map_areas(const fude_zoom_scene* _s, fude_zoom_map_area* _out, u32 _max);

// The areas in the order they are presented (and exported): those whose names begin with a number ("1 Intro",
// "2. The kitchen") first, by that number; then the others in the order they were made. Reordered in place.
void fude_zoom_map_areas_order(fude_zoom_map_area* _areas, u32 _n);

// The places, by number, their middles in root units: how many.
u32 fude_zoom_map_places(const fude_zoom_scene* _s, fude_zoom_map_place* _out, u32 _max);

// What a map of shape _aspect (its height over its width) shows: _contents and _view together, a margin
// round them, grown about their middle to that shape. With nothing drawn, the view's surroundings.
fude_zoom_box fude_zoom_map_world(fude_zoom_box _contents, fude_zoom_box _view, f64 _aspect);

// The view a tap at _p on the map (root units) flies to: there, as far in as the view is now — unless that
// would show less than a ninth of the map's width (_world) or more than two thirds of it: then the nearer of
// those (deep in, a tap elsewhere lands where something can be seen round it). _half: the screen's half-size.
fude_zoom_camera fude_zoom_map_tap_view(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_box _world, fude_zoom_v2 _p);

#endif
