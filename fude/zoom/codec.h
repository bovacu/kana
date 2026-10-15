// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CODEC
#define FUDE_ZOOM_CODEC

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// Stroke codec: a stroke's points packed into bytes, losslessly, at about 1.6 bytes a point (1.64 measured for x, y and
// pressure; 1.98 with time: docs/infinite_canvas_design.md §13).
//
// Where it sits: zoom, the canvas apps' layer, under the scene. scene.c packs every stroke and fill it is given and
// unpacks them to draw and edit them; piece.c and pdfview.c unpack the points they are handed.
// Threads: none of its own, and no shared state: it may be called on any thread with the caller's own buffers. In
// practice everything calls it on the main thread.
// Files: these bytes are a stroke's payload inside the canvas's file (zfile.h) and a piece's (piece.h).
//
// A stroke keeps its points as INTEGERS: multiples of its quantum (2^q frame units, about 1/16 of a point at the zoom
// it was drawn at), relative to its first point, which is its translation (scene.h). Because they are integers, the
// bytes decode to exactly what was encoded, however often a file is saved: nothing drifts.
//
// Each channel is stored after the one before, bit-packed:
//   x, y      the delta of the delta (a pen moves smoothly, so the second difference is small)
//   pressure  the delta, of values 0..1023: the share of the stroke's half-width the pen gave each point (the
//             capture's own width curve, kept exactly)
//   time      milliseconds since the first point, delta of delta (samples come at an even pace)
// Then each value is zigzagged (its sign moved into the low bit) and Rice-coded in blocks of 32: each block's best k
// in 5 bits, then each value's high part in unary and its k low bits. The unary part stops after 15 ones and escapes
// to 32 raw bits, so one wild value costs 47 bits instead of a run of thousands.
//
// A stroke stores only the channels its record says it has (FUDE_ZOOM_CHANNEL_), and only the point count and those
// flags are needed to decode it.
// Design: see docs/infinite_canvas_design.md §4 (strokes) and §7 (the file).
// ===========================================================================

// The optional channels a stroke may have (bits of its record's channels). x and y are always there.
#define FUDE_ZOOM_CHANNEL_PRESSURE 0x01u
#define FUDE_ZOOM_CHANNEL_TIME     0x02u

// A point's coordinates fit in ±2^28 quanta: at 1/16 of a point each, about 16 million points either way. Even a
// stroke 16 million quanta long would be a million points across, which never happens: the capture cuts strokes long
// before that (FUDE_ZOOM_STROKE_MAX).
#define FUDE_ZOOM_CODEC_MAX_COORD (1 << 28)

// A point as a stroke keeps it: integers (quanta) and its pressure and time.
typedef struct {
    i32 x, y;          // Quanta from the stroke's first point (the first point is 0, 0).
    u16 pressure;      // 0..1023: the share of the stroke's half-width at this point.
    u32 time;          // Milliseconds since the first point.
} fude_zoom_qpoint;

// Appends the encoded channels of _count points to _out. The first point must be (0, 0). _channels: the
// FUDE_ZOOM_CHANNEL_ bits to store besides x and y.
void fude_zoom_codec_encode(fude_bytes* _out, const fude_zoom_qpoint* _points, u32 _count, u8 _channels);
// Decodes _count points from _data (_size bytes) into _points, which must hold _count. Channels the stroke does not
// have (by _channels) read as 0. Returns false when the bytes run out or are not a stroke's; _points then holds
// nothing usable.
b8   fude_zoom_codec_decode(const u8* _data, u32 _size, fude_zoom_qpoint* _points, u32 _count, u8 _channels);

#endif
