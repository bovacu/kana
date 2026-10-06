// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_DXF
#define FUDE_ZOOM_DXF

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// Templates out to a CNC router or a laser (research §2.10, item 8): DXF in
// millimetres, a layer for each operation (cut, score, engrave, drill), and a
// pre-flight check of what was added, before anything is cut.
//
// The caller adds geometry already in millimetres, Y up (as DXF has it), each
// piece on a layer; finishing writes the whole file. The format is AutoCAD R12
// ASCII DXF (AC1009), the one every CAM program reads (LightBurn, VCarve,
// Fusion, LibreCAD, Inkscape). R12 has no units of its own, so the header also
// carries $INSUNITS 4 (millimetres) and $MEASUREMENT 1 (metric): variables of
// later versions, but readers that know them honour them in an R12 file and
// the rest skip them, so nobody has to guess between inches and millimetres.
//
//   HEADER    $ACADVER, $DWGCODEPAGE, $INSBASE, $EXTMIN and $EXTMAX (from the
//             geometry; a text by its insertion point only, as its width
//             depends on the reader's font), $INSUNITS, $MEASUREMENT
//   TABLES    LTYPE (CONTINUOUS), LAYER ("0" and the caller's, each with its
//             colour), STYLE (STANDARD, which texts use)
//   BLOCKS    empty
//   ENTITIES  LINE, POLYLINE (its VERTEXes and SEQEND; flag 70 is 1 when
//             closed), CIRCLE, ARC, TEXT, in the order added; colour and line
//             type by layer
//
// Each group code is on its own line, right-aligned in three ("  0", " 10"),
// its value on the next. Lines end in "\r\n", as AutoCAD writes them: every
// reader has to take that, and a few older Windows ones take nothing else.
// Numbers have up to six decimals, trailing zeros dropped, never an exponent,
// written by hand so that a locale's decimal comma cannot get in. Anything not
// finite is stored as 0 when added, so the file, its extents and the checks
// agree.
//
// Layers: a name keeps letters, digits, '_' and '-' (any other character
// becomes one '_'), at most 31 of them, and is compared without case, as
// AutoCAD does: the first spelling and colour are kept. No name, or "0", is
// layer "0" (index 0, colour 7). Colours are AutoCAD's index, 1 to 255; 0,
// which a layer cannot have, becomes 7. An index fude_zoom_dxf_layer did not
// give puts the geometry on layer "0".
//
// Texts: ASCII as it is, anything else as DXF's \U+XXXX (past U+FFFF, a UTF-16
// pair of them). A '%' before another '%' is written %%% so it stays a percent
// sign rather than a control code, a backslash that would start an escape is
// \U+005C, and control characters become spaces. At most 255 bytes (R12's
// limit), cut at a whole character.
//
// Left out, as there is nothing to cut: a polyline of fewer than two points,
// an empty text, an arc of no sweep. An arc of a whole turn or more is written
// as a CIRCLE; a closed polyline whose last point repeats its first loses the
// repeat (the flag closes it). Text heights not above zero are 1 mm.
//
// Pre-flight, two things meeting when they are within _tiny_mm:
//   open_paths     open contours. Lines, open polylines and arcs are pieces;
//                  pieces on one layer whose ends meet join into one contour
//                  (a score line does not close a cut), and a contour is open
//                  when any end in it meets no other end. A piece's own two
//                  ends meeting close it only if it goes further than
//                  _tiny_mm from its start somewhere (a vertex, an arc's
//                  middle), so a speck, or a line, never closes on itself.
//                  Four lines round a square are closed; a lone line, or
//                  three in a row, is one open path. Closed polylines and
//                  circles are closed; an end on the middle of something else
//                  is still loose.
//   duplicates     entities the same as an earlier one on the same layer (the
//                  same path on two layers is two operations): lines and
//                  polylines by their points in either direction (closed ones
//                  from any start; a line is an open polyline of two points),
//                  circles by centre and radius, arcs by centre, radius, ends
//                  and middle, texts by place, height, angle and string. They
//                  stay out of open_paths: a line drawn back over itself is
//                  not a loop.
//   tiny_segments  segments shorter than _tiny_mm: a line, each side of a
//                  polyline (a closed one's closing side too), an arc or a
//                  circle by its length.
// ===========================================================================

typedef struct fude_zoom_dxf fude_zoom_dxf;

typedef struct {
    u32 open_paths;
    u32 duplicates;
    u32 tiny_segments;
    u32 entities;        // everything in ENTITIES (a polyline is one)
} fude_zoom_dxf_check;

// An empty drawing with layer "0". NULL when there is no memory.
fude_zoom_dxf*      fude_zoom_dxf_new(void);
void                fude_zoom_dxf_free(fude_zoom_dxf* _d);

// The layer of that name (see above), made with colour _aci if it is new. Its index.
u32                 fude_zoom_dxf_layer(fude_zoom_dxf* _d, const c8* _name, u8 _aci);

// Geometry in millimetres, Y up.
void                fude_zoom_dxf_line(fude_zoom_dxf* _d, u32 _layer, f64 _x1, f64 _y1, f64 _x2, f64 _y2);
// _count points, x and y in turn; _closed joins the last back to the first.
void                fude_zoom_dxf_polyline(fude_zoom_dxf* _d, u32 _layer, const f64* _xy, u32 _count, b8 _closed);
void                fude_zoom_dxf_circle(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r);
// Counter-clockwise from _start_deg to _end_deg (DXF's way; 350 to 10 is 20 degrees).
void                fude_zoom_dxf_arc(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r, f64 _start_deg, f64 _end_deg);
// _height in millimetres, its baseline starting at (_x, _y), turned _rotation_deg anticlockwise.
void                fude_zoom_dxf_text(fude_zoom_dxf* _d, u32 _layer, f64 _x, f64 _y, f64 _height, f64 _rotation_deg, const c8* _utf8);

// The whole file appended to _out. False, and nothing written, without a drawing or an _out.
b8                  fude_zoom_dxf_finish(const fude_zoom_dxf* _d, fude_bytes* _out);

// What a CNC operator would want to know before cutting (see above).
fude_zoom_dxf_check fude_zoom_dxf_preflight(const fude_zoom_dxf* _d, f64 _tiny_mm);

#endif
