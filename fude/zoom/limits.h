// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LIMITS_H
#define FUDE_ZOOM_LIMITS_H

#include "rde.h"
#include "zoom/circuit.h"

// ===========================================================================
// Limits: what each part of a circuit takes at most (circuit.h's fude_zoom_limits), as a real part's datasheet says,
// and the real parts to choose from, each with its own (a 1N4148 or a 1N4007, a 2N2222 or a TIP120, a ¼ W resistor
// or a 2 W one). A part whose text names one of them ("1N4007", "NE555") has that part's limits; otherwise it has a
// typical part's of its kind.
//
// Where it sits: zoom, the canvas apps' layer, over circuit.h. The circuit's step (circuit_step.c) measures and heats
// its parts with it at every step, and the limits card (limitsform.h) shows and edits them.
// Its main types and their lifecycle: fude_zoom_limits_preset is a real part's entry in constant tables compiled into
// limits.c; nothing is created or freed.
// Threads: called on the main thread, from the circuit's step. It keeps no state between calls.
// Design:
//   Each part is measured at every step from its pins' volts and currents (the current INTO the part at each pin): the
//   power it turns to heat, the current through it (a transistor's collector's, a chip's worst output's), the voltage
//   across it (a chip's supply), and the voltage the wrong way round.
//   Past a limit it heats, and the further past, the faster: its heat grows by (stress² − 1) × dt / tau, so at twice
//   its limit it burns in a third of its tau. Tau is as quick as its kind is (a semiconductor's junction in hundredths
//   of a second, a resistor in seconds). Below its limits it cools, four times more slowly.
//   A source (a battery, a supply) never burns: it only says that it is past its current, as with a short circuit or
//   something near one.
//   See docs/infinite_canvas_design.md §47.
// ===========================================================================

// A real part a circuit part can be, with its absolute maximum ratings.
typedef struct {
    const c8* name;    // as the card shows it
    b8        named;   // it is a real part number, so a part whose text names it ("D1 1N4007") is that part
    // Its limits by FUDE_ZOOM_LIMIT_: power (W), current (A), voltage (V), reverse or gate voltage (V); 0 for none.
    f64       most[FUDE_ZOOM_LIMIT_COUNT];
} fude_zoom_limits_preset;

// Returns the real parts _part can be, the typical one first, and how many into *_count (which may be NULL). Returns
// NULL, with a count of 0, when its kind has none. The table is constant and valid for the program's life.
const fude_zoom_limits_preset* fude_zoom_limits_presets(const fude_zoom_part* _part, u32* _count);
// Returns the index of the real part that _text names (a word of it starting with the part's number, in any case), or
// -1 when it names none.
i32  fude_zoom_limits_named(const fude_zoom_part* _part, const c8* _text);
// Returns _part's limits as its text says (a real part named in it), or else a typical part's of its kind. _value
// holds its values as the circuit read them (may be NULL), because some limits follow from them: a lamp's from its
// watts, a motor's from its volts. A part with no limits gets all zeros.
fude_zoom_limits fude_zoom_limits_typical(const fude_zoom_part* _part, const c8* _text, const f64* _value);
// Returns which limits _part has, one bit for each FUDE_ZOOM_LIMIT_ (1 << the limit): those its card shows. 0 means
// none (a ground, a logic probe).
u32  fude_zoom_limits_kinds(const fude_zoom_part* _part);
// Returns whether _part's REVERSE limit is its gate's (a MOSFET's), which holds either way round, not only backwards.
b8   fude_zoom_limits_gate(const fude_zoom_part* _part);
// Returns how quickly _part heats, as tau in seconds: at twice its limit it burns in a third of them. 0 means it never
// burns and only says it is past its limit (a source).
f64  fude_zoom_limits_tau(const fude_zoom_part* _part);
// Writes _part's measures into _out (FUDE_ZOOM_LIMIT_COUNT of them: W, A, V, V) from its _pins pins' volts and the
// currents into it at each pin. A pin joined to nothing has NaN volts: nothing flows through it, so every voltage
// across it measures 0 (an LED with its cathode on 9 V and its anode on nothing is not reversed). All are 0 for a NULL
// part or no pins.
void fude_zoom_limits_measure(const fude_zoom_part* _part, const f64* _volts, const f64* _amps, u32 _pins, f64* _out);
// Advances a part with limits _limits and measures _measure by _dt seconds. *_stress gets its worst measure divided by
// its limit (0 when it has no limits) and *_worst which limit that is. *_heat is read and updated: for a source it
// counts the seconds it has been past its limit, so a moment's surge (a capacitor charging) is not taken for a short.
// Returns true when the part burns now (its heat has reached 1); a source never does.
b8   fude_zoom_limits_step(const fude_zoom_part* _part, const fude_zoom_limits* _limits, const f64* _measure, f64 _dt,
                           f64* _stress, u8* _worst, f64* _heat);

#endif
