// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CODEC
#define FUDE_ZOOM_CODEC

#include "rde.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// A stroke's points as bytes: about 1.5 bytes a point, exactly.
//
// A stroke keeps its points as INTEGERS — multiples of its quantum (2^q frame
// units, about 1/16 pt at the zoom it was drawn at), relative to its first point,
// which is its translation (scene.h). Integers mean the bytes decode to exactly
// what was encoded, forever: no drift, no matter how often a file is saved.
//
// Each channel is stored apart, one after the other, bit-packed:
//   x, y      delta of delta (a pen moves smoothly: the second difference is small)
//   pressure  delta, 0..1023: the share of the stroke's half-width the pen gave
//             each point (the capture's own width curve, kept exactly)
//   time      ms since the first point, delta of delta (samples come evenly)
// then each value zigzagged (sign into the low bit) and Rice-coded in blocks of
// 32: a block's best k in 5 bits, each value as its high part in unary (up to
// 15 ones, then an escape to 32 raw bits — one wild value costs 47 bits, not a
// run of thousands) and its k low bits.
//
// The channels a stroke has are its record's (FUDE_ZOOM_CHANNEL_): only what it
// has is stored, and only the count and those flags are needed to decode.
// ===========================================================================

#define FUDE_ZOOM_CHANNEL_PRESSURE 0x01u
#define FUDE_ZOOM_CHANNEL_TIME     0x02u

// A point's coordinates fit in ±2^28 quanta (a stroke 16 million quanta long is
// a million points across at 1/16 pt: never). The capture cuts strokes long
// before (FUDE_ZOOM_STROKE_MAX).
#define FUDE_ZOOM_CODEC_MAX_COORD (1 << 28)

typedef struct {
    i32 x, y;          // quanta from the stroke's first point (the first is 0, 0)
    u16 pressure;      // 0..1023
    u32 time;          // ms since the first point
} fude_zoom_qpoint;

// Appends _count points' channels to _out. The first point must be (0, 0).
void fude_zoom_codec_encode(fude_bytes* _out, const fude_zoom_qpoint* _points, u32 _count, u8 _channels);
// Decodes _count points from _data (_size bytes) into _points. Channels the
// stroke does not have read as 0. False when the bytes run out or are not a
// stroke's: then _points holds nothing usable.
b8   fude_zoom_codec_decode(const u8* _data, u32 _size, fude_zoom_qpoint* _points, u32 _count, u8 _channels);

#endif
