// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LIMITS_H
#define FUDE_ZOOM_LIMITS_H

#include "rde.h"
#include "zoom/circuit.h"

// ===========================================================================
// A CIRCUIT'S PARTS' LIMITS (circuit.h's fude_zoom_limits): what each takes
// at most, as a real part's datasheet says — and real parts to choose from,
// each its own (a 1N4148 or a 1N4007, a 2N2222 or a TIP120, a ¼ W resistor
// or a 2 W one). A part's text naming one of them ("1N4007", "NE555") has
// its limits; else a typical part's of its kind.
//
// Measured each step from its pins' volts and currents (the current INTO
// the part at each pin): the power it turns to heat, the current through
// it (a transistor's collector's, a chip's worst output's), the voltage
// across it (a chip's supply), the voltage the wrong way round. Past a
// limit it heats — the further past, the faster (as the square of how far:
// twice the limit four times the heat), as quickly as its kind does (a
// semiconductor's junction in hundredths of a second, a resistor in
// seconds) — and cools below it more slowly. A source (a battery, a supply)
// only says so: past its current, a short circuit, or near one.
// ===========================================================================

typedef struct {
    const c8* name;    // as the card shows it
    b8        named;   // a real part's number: a part's text naming it ("D1 1N4007") is it
    f64       most[FUDE_ZOOM_LIMIT_COUNT];
} fude_zoom_limits_preset;

// The real parts a part can be (how many into *_count; NULL: none).
const fude_zoom_limits_preset* fude_zoom_limits_presets(const fude_zoom_part* _part, u32* _count);
// The one its text names (its index; -1: none).
i32  fude_zoom_limits_named(const fude_zoom_part* _part, const c8* _text);
// A part's limits as its text says (a real part named in it), else a typical part's of its kind. _value: its values as
// the circuit read them (a lamp's limits follow from its watts, a motor's from its volts).
fude_zoom_limits fude_zoom_limits_typical(const fude_zoom_part* _part, const c8* _text, const f64* _value);
// Which limits a part has (a bit each, FUDE_ZOOM_LIMIT_'s): those its card shows. 0: none (a ground, a logic probe).
u32  fude_zoom_limits_kinds(const fude_zoom_part* _part);
// A part's REVERSE limit is its gate's (a MOSFET's): either way round, not only backwards.
b8   fude_zoom_limits_gate(const fude_zoom_part* _part);
// How quickly a part heats (seconds: twice its limit, it burns in a third of them). 0: it never burns — it only says
// it is past its limit (a source).
f64  fude_zoom_limits_tau(const fude_zoom_part* _part);
// A part's measures (W, A, V, V: FUDE_ZOOM_LIMIT_'s) from its _pins pins' volts and currents into it.
void fude_zoom_limits_measure(const fude_zoom_part* _part, const f64* _volts, const f64* _amps, u32 _pins, f64* _out);
// One step of _dt seconds for a part with limits _limits measured _measure: its stress (the worst measure over its limit;
// 0: none), which limit that is, and its heat (in: as it was; out: now — a source's: the seconds it has been past its
// limit, a charging capacitor's moment not a short). True: it burns now (its heat reached 1).
b8   fude_zoom_limits_step(const fude_zoom_part* _part, const fude_zoom_limits* _limits, const f64* _measure, f64 _dt,
                           f64* _stress, u8* _worst, f64* _heat);

#endif
