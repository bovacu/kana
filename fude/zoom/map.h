// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MAP_H
#define FUDE_ZOOM_MAP_H

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Map: the canvas seen from above (Map, and Present). It works out what is drawn, seen from the top of
// the canvas (its root frame, where whatever is deeper is a speck at its place), the view on it, the places marked,
// and the AREAS.
//
// An AREA is a region of the canvas with a name: a symbol of the diagram library's kind "area" (symbol.h: its outline,
// its name in its corner), made by Insert's Named area round what the lasso holds, or round the view. The map shows
// their names, a tap there frames one, and Present goes through them in the order they were made (or, when there are
// none, through the places by number).
//
// Where it sits: zoom, the canvas apps' layer. Here is only what is where, in the root frame's units; the page
// (page_present.c) draws the map, takes its taps, presents, and exports the areas.
// Threads: main thread only (it reads the scene). It keeps no state.
// Design: see docs/infinite_canvas_design.md §22.
// ===========================================================================

#define FUDE_ZOOM_MAP_AREAS 64u    // The most areas named on the map (and presented).
#define FUDE_ZOOM_MAP_NAME  96u    // The most bytes of an area's name kept, its NUL included.
#define FUDE_ZOOM_MAP_PLACES 32u   // The most places shown.

// A named area on the map.
typedef struct {
    u32           object;                    // The area's symbol.
    fude_zoom_box box;                       // In root units.
    c8            name[FUDE_ZOOM_MAP_NAME];
} fude_zoom_map_area;

// A place marked on the canvas.
typedef struct {
    u32          object;   // The place's MARK.
    u32          number;   // Its number, as in "Place 3".
    fude_zoom_v2 at;       // In root units.
} fude_zoom_map_place;

// Returns whether _object is an area: alive, and a symbol of kind "area".
b8 fude_zoom_map_is_area(const fude_zoom_scene* _s, u32 _object);

// Returns what is drawn, in root units: the root's own things and its frames as far as their contents reach. The
// places, the layers' own records, what is on a hidden layer, and frames nothing was drawn in are left out. Empty
// when nothing is drawn.
fude_zoom_box fude_zoom_map_contents(const fude_zoom_scene* _s);

// Returns the camera's view in root units, for a screen _half each way (points).
fude_zoom_box fude_zoom_map_view(const fude_zoom_scene* _s, fude_zoom_v2 _half);

// Fills _out with the areas on shown layers, at most _max, in the order they were made, their boxes in root units and
// their names their text's first line. Returns how many.
u32 fude_zoom_map_areas(const fude_zoom_scene* _s, fude_zoom_map_area* _out, u32 _max);

// Puts the areas in the order they are presented (and exported), in place: those whose names begin with a number
// ("1 Intro", "2. The kitchen") first, by that number; then the others in the order they were made.
void fude_zoom_map_areas_order(fude_zoom_map_area* _areas, u32 _n);

// Fills _out with the places, by number, their marks' places in root units. Past _max, the highest numbers are left
// out. Returns how many.
u32 fude_zoom_map_places(const fude_zoom_scene* _s, fude_zoom_map_place* _out, u32 _max);

// Returns what a map of shape _aspect (its height over its width) shows: _contents and _view together, with a margin
// round them, grown about their middle to that shape. With nothing drawn, it shows the view and some way round it.
fude_zoom_box fude_zoom_map_world(fude_zoom_box _contents, fude_zoom_box _view, f64 _aspect);

// Returns the view a tap at _p on the map (root units) flies to: centred there, as far in as the view is now, unless
// that would show less than a ninth of the map's width (_world) or more than two thirds of it; then the nearer of those
// (so deep in, a tap elsewhere lands where something can be seen round it). _half: the screen's half-size, in points.
fude_zoom_camera fude_zoom_map_tap_view(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_box _world, fude_zoom_v2 _p);

#endif
