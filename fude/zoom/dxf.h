// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_DXF
#define FUDE_ZOOM_DXF

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// DXF writer: templates out to a CNC router or a laser (research §2.10, item 8). It writes DXF in millimetres, with a
// layer for each operation (cut, score, engrave, drill), and runs a pre-flight check of what was added before
// anything is cut.
//
// Where it sits: zoom, the canvas apps' layer. export.c (fude_zoom_export_dxf) is its user: it adds the view's
// geometry, a layer for each colour drawn with, and shows the pre-flight's findings after the export.
// Its main types and their lifecycle: fude_zoom_dxf_new makes a fude_zoom_dxf (opaque) and fude_zoom_dxf_free frees
// it. The caller adds geometry already in millimetres with Y up (as DXF has it), each piece on a layer, then
// fude_zoom_dxf_finish writes the whole file.
// Threads: none of its own and no shared state; one drawing must be used on one thread at a time. Today it is only
// used on the main thread.
// Files: it writes no file itself: fude_zoom_dxf_finish appends the DXF's bytes to a fude_bytes. The format is AutoCAD
// R12 ASCII DXF (AC1009), which every CAM program reads (LightBurn, VCarve, Fusion, LibreCAD, Inkscape). R12 has no
// units of its own, so the header also carries $INSUNITS 4 (millimetres) and $MEASUREMENT 1 (metric): these are
// variables of later versions, but readers that know them honour them in an R12 file and the others skip them, so
// nobody has to guess between inches and millimetres.
//
//   HEADER    $ACADVER, $DWGCODEPAGE, $INSBASE, $EXTMIN and $EXTMAX (from the geometry; a text counts by its insertion
//             point only, as its width depends on the reader's font), $INSUNITS, $MEASUREMENT
//   TABLES    LTYPE (CONTINUOUS), LAYER ("0" and the caller's, each with its colour), STYLE (STANDARD, which texts
//             use)
//   BLOCKS    empty
//   ENTITIES  LINE, POLYLINE (its VERTEXes and SEQEND; flag 70 is 1 when closed), CIRCLE, ARC, TEXT (centred or set
//             right: group 72 and the same point again as 11/21, its alignment point), in the order added; colour and
//             line type by layer
//
// Each group code is on its own line, right-aligned in three characters ("  0", " 10"), its value on the next line.
// Lines end in "\r\n", as AutoCAD writes them: every reader has to accept that, and a few older Windows ones accept
// nothing else. Numbers have up to six decimals, trailing zeros dropped, never an exponent, and are written by hand
// so that a locale's decimal comma cannot get in. Anything not finite is stored as 0 when it is added, so the file,
// its extents and the checks agree.
//
// Layers: a name keeps letters, digits, '_' and '-' (any other character becomes one '_'), at most 31 of them, and is
// compared without case, as AutoCAD does: the first spelling and colour are kept. No name, or "0", is layer "0"
// (index 0, colour 7). Colours are AutoCAD's colour index, 1 to 255; 0, which a layer cannot have, becomes 7.
// Geometry on a layer index fude_zoom_dxf_layer did not give goes on layer "0".
//
// Texts: ASCII is written as it is, anything else as DXF's \U+XXXX (past U+FFFF, a UTF-16 pair of them). A '%'
// before another '%' is written %%% so it stays a percent sign rather than starting a control code, a backslash that
// would start an escape is written \U+005C, and control characters become spaces. At most 255 bytes (R12's limit),
// cut at a whole character.
//
// Left out, as there is nothing to cut: a polyline of fewer than two points, an empty text, an arc of no sweep. An arc
// of a whole turn or more is written as a CIRCLE; a closed polyline whose last point repeats its first loses the
// repeat (the closed flag closes it). Text heights not above zero become 1 mm.
//
// Pre-flight, where two things meet when they are within _tiny_mm:
//   open_paths     open contours. Lines, open polylines and arcs are pieces; pieces on one layer whose ends meet join
//                  into one contour (a score line does not close a cut), and a contour is open when any end in it
//                  meets no other end. A piece's own two ends meeting close it only if it goes further than _tiny_mm
//                  from its start somewhere (a vertex, an arc's middle), so a speck, or a line, never closes on
//                  itself. Four lines round a square are closed; a lone line, or three in a row, is one open path.
//                  Closed polylines and circles are closed; an end on the middle of something else is still loose.
//   duplicates     entities the same as an earlier one on the same layer (the same path on two layers is two
//                  operations): lines and polylines by their points in either direction (closed ones from any
//                  start; a line counts as an open polyline of two points), circles by centre and radius, arcs by
//                  centre, radius, ends and middle, texts by place, height, angle, alignment and string. They are
//                  left out of open_paths: a line drawn back over itself is not a loop.
//   tiny_segments  segments shorter than _tiny_mm: a line, each side of a polyline (a closed one's closing side too),
//                  an arc or a circle by its length.
// Design: see docs/infinite_canvas_design.md §15.
// ===========================================================================

// A drawing being built (opaque).
typedef struct fude_zoom_dxf fude_zoom_dxf;

// What the pre-flight found (fude_zoom_dxf_preflight; see above).
typedef struct {
    u32 open_paths;
    u32 duplicates;
    u32 tiny_segments;
    u32 entities;        // Everything in ENTITIES (a polyline counts as one).
} fude_zoom_dxf_check;

// fude_zoom_dxf_new makes an empty drawing with layer "0"; NULL when there is no memory. fude_zoom_dxf_free frees it
// and accepts NULL.
fude_zoom_dxf*      fude_zoom_dxf_new(void);
void                fude_zoom_dxf_free(fude_zoom_dxf* _d);

// Returns the index of the layer named _name (cleaned and compared as described above), making it with colour _aci
// if it is new. An empty name, or a NULL drawing, gives layer 0.
u32                 fude_zoom_dxf_layer(fude_zoom_dxf* _d, const c8* _name, u8 _aci);

// Add geometry on layer _layer, in millimetres with Y up: a line from (_x1, _y1) to (_x2, _y2).
void                fude_zoom_dxf_line(fude_zoom_dxf* _d, u32 _layer, f64 _x1, f64 _y1, f64 _x2, f64 _y2);
// _count points, x and y in turn at _xy; _closed joins the last back to the first.
void                fude_zoom_dxf_polyline(fude_zoom_dxf* _d, u32 _layer, const f64* _xy, u32 _count, b8 _closed);
// A circle round (_cx, _cy) of radius _r.
void                fude_zoom_dxf_circle(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r);
// An arc counter-clockwise from _start_deg to _end_deg, as DXF has it (350 to 10 is 20 degrees).
void                fude_zoom_dxf_arc(fude_zoom_dxf* _d, u32 _layer, f64 _cx, f64 _cy, f64 _r, f64 _start_deg, f64 _end_deg);
// How a text's line sits on its point: R12's horizontal justification (TEXT's group 72), so a reader centres or sets
// it right by its own font's widths, which the writer cannot know.
enum {
    FUDE_ZOOM_DXF_ALIGN_LEFT   = 0,   // the baseline starts at the point
    FUDE_ZOOM_DXF_ALIGN_CENTRE = 1,   // the baseline's middle is at the point
    FUDE_ZOOM_DXF_ALIGN_RIGHT  = 2,   // the baseline ends at the point
};

// A text _height millimetres high on its baseline's point (_x, _y), as _align (FUDE_ZOOM_DXF_ALIGN_; anything else is
// left) says, turned _rotation_deg anticlockwise.
void                fude_zoom_dxf_text(fude_zoom_dxf* _d, u32 _layer, f64 _x, f64 _y, f64 _height, f64 _rotation_deg, u8 _align, const c8* _utf8);

// Appends the whole file to _out. Returns false, writing nothing, when there is no drawing or no _out.
b8                  fude_zoom_dxf_finish(const fude_zoom_dxf* _d, fude_bytes* _out);

// Returns what a CNC operator would want to know before cutting (see above). All zeros for a NULL drawing; a
// negative or NaN _tiny_mm counts as 0.
fude_zoom_dxf_check fude_zoom_dxf_preflight(const fude_zoom_dxf* _d, f64 _tiny_mm);

#endif
