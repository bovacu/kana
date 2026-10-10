// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_DISPLAY_H
#define FUDE_ZOOM_DISPLAY_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/circuit.h"

// ===========================================================================
// DISPLAYS (Electronics; circuit.h's models): a PANEL of 7-segment digits
// (1 to 8, each digit's segments joined to the others' — a common pin a
// digit: multiplexed as real ones are, what is lit often enough seen as
// steady), an LED MATRIX of any size up to 32 × 32 (a pin a row, a pin a
// column, an LED at each crossing), a 10-LED BAR GRAPH and an LM3914 to drive
// it (a voltage in: a bar or a dot that far up), a PANEL METER (an LCD: a
// voltmeter's or an ammeter's 3½ digits), and a CHARACTER LCD — an HD44780's,
// its commands and characters written on its pins as a microcontroller would
// write them, or by hand (switches on its data pins, a button on E).
//
// The panel, the matrix and the LCD are SIZED: their text says how big
// ("4 digits", "8x8": columns × rows, "16x2"; "CA" a panel's commons anodes,
// "CC" a matrix's rows cathodes) — a part of circuit.h made for each size,
// once, kept: its pins (20 points apart, as every part's), its room.
//
// What the eye sees of an LED (a panel's segment, a matrix's dot, a bar):
// its current, held a moment (FUDE_ZOOM_DISPLAY_GLOW) — a multiplexed digit
// lit a quarter of the time a quarter as bright, and steady.
// ===========================================================================

#define FUDE_ZOOM_DISPLAY_DIGITS 8u      // a panel's digits at most
#define FUDE_ZOOM_DISPLAY_SIDE   32u     // a matrix's rows or columns at most
#define FUDE_ZOOM_DISPLAY_GLOW   0.02    // seconds the eye holds a light
#define FUDE_ZOOM_DISPLAY_FULL   0.01    // an LED's current seen as fully bright (A)

// --- sized parts ---------------------------------------------------------------------------------

// The part a display's text makes of its kind's _template (its size, its way round); _template itself when it is not a
// sized display's.
const fude_zoom_part* fude_zoom_display_part(const fude_zoom_part* _template, const c8* _text);
// Whether a part is a sized display's, made so.
b8   fude_zoom_display_made(const fude_zoom_part* _part);
// A sized display's columns and rows (a panel's: its digits, 1; a matrix's: its dots; an LCD's: its characters). 0: not one.
u32  fude_zoom_display_cols(const fude_zoom_part* _part);
u32  fude_zoom_display_rows(const fude_zoom_part* _part);
// Its room as it comes (the catalogue's units, w × h). False: not a sized display.
b8   fude_zoom_display_room(const fude_zoom_part* _part, f64* _w, f64* _h);
// Where its pins' names go in it, of its half sizes (u: a left or right pin's, v: a top or bottom one's); as they were
// when it is not a sized display.
void fude_zoom_display_label_inset(const fude_zoom_part* _part, f32* _u, f32* _v);

// --- its LEDs ------------------------------------------------------------------------------------

// How many LEDs a display has (a panel's 8 a digit, a matrix's a dot each, a bar graph's 10); 0: none.
u32  fude_zoom_display_lights(const fude_zoom_part* _part);
// LED _k's anode and cathode pins (a panel's digit _k / 8, segment _k % 8 — a b c d e f g dp —; a matrix's row
// _k / its columns, column _k % them). False: none such.
b8   fude_zoom_display_led(const fude_zoom_part* _part, u32 _k, u32* _anode, u32* _cathode);
// A 7-segment digit's segments for a character ('0'–'9', '-', ' ': bit 0 a … bit 6 g).
u8   fude_zoom_display_segments(c8 _c);

// --- a character LCD (an HD44780) ----------------------------------------------------------------

#define FUDE_ZOOM_LCD_DL 0x10u   // its function's: 8 data bits (else 4)
#define FUDE_ZOOM_LCD_N  0x08u   // ...two lines (else one)
#define FUDE_ZOOM_LCD_D  0x04u   // its display control's: on
#define FUDE_ZOOM_LCD_C  0x02u   // ...its cursor shown
#define FUDE_ZOOM_LCD_B  0x01u   // ...its cursor blinking
#define FUDE_ZOOM_LCD_ID 0x02u   // its entry mode's: the address counts up (else down)
#define FUDE_ZOOM_LCD_S  0x01u   // ...the display moves as it is written

// What the controller holds: its 80 characters' memory (two lines of 40, or one of 80), its 8 characters of its own
// (CGRAM: 5 × 8 each), its address counter and what it is in, its modes, the display's shift, half a byte (4 bits:
// its high half waiting for its low one), E as it was, whether it has power, and whether it has been told anything
// since it came on (not yet: its first row a row of dark blocks, as a real one's is, powered and not set up).
typedef struct {
    u8 ddram[80];
    u8 cgram[64];
    u8 ac;          // its address counter
    u8 cg;          // written to: CGRAM (1), the characters (0)
    u8 entry;       // FUDE_ZOOM_LCD_ID | _S
    u8 control;     // FUDE_ZOOM_LCD_D | _C | _B
    u8 function;    // FUDE_ZOOM_LCD_DL | _N (| 5 × 10 dots: not drawn)
    u8 half;        // 4 bits: a high half waiting
    u8 nibble;      // ...it
    u8 e;           // E high as last stepped
    u8 powered;
    u8 fresh;       // nothing written since it came on
    u8 spare;
    i8 shift;       // the display's window along its lines (characters it moved left)
    u8 spare2[4];
} fude_zoom_lcd;

// As it comes on: its characters spaces, 8 bits, one line, the display off, counting up.
void fude_zoom_lcd_power_on(fude_zoom_lcd* _l);
// A byte as the controller takes it: a command (_rs 0) or a character (1).
void fude_zoom_lcd_write(fude_zoom_lcd* _l, b8 _rs, u8 _byte);
// E fallen, its data pins read (bit _i: D_i high): a byte (8 bits) or half of one (4 bits: D7–D4, the high half first).
void fude_zoom_lcd_strobe(fude_zoom_lcd* _l, b8 _rs, u8 _pins);
// What a display _cols × _rows shows at column _col of row _row: its character's code (a space: nothing), and whether
// its cursor is there (_cursor; NULL: not asked). Rows a line does not drive (one line on four rows: the second and
// fourth) show spaces.
u8   fude_zoom_lcd_shown(const fude_zoom_lcd* _l, u32 _cols, u32 _rows, u32 _col, u32 _row, b8* _cursor);
// A character's 5 × 8 dots (its ROM's — the A00's: ASCII, ¥ for \, → ←, ° —, or its own 0–7: CGRAM's), a row each
// from the top (bit 4 its left column).
void fude_zoom_lcd_glyph(const fude_zoom_lcd* _l, u8 _code, u8* _rows);

// --- a panel meter -------------------------------------------------------------------------------

// What a panel meter reads, from its text ("20V", "200mV", "2A", "20mA"): up to what (V or A), in amperes or volts, its
// digits in its range's own unit (V or mV: times what), how many of them after its point (its 3½ digits: up to 1999).
typedef struct {
    f64 full;
    b8  amps;
    f64 times;
    u32 decimals;
} fude_zoom_meter;

typedef struct {
    b8 minus;
    c8 digit[4];   // '0'–'9' or ' ' (the first its half digit: '1' or blank)
    b8 point[4];   // a decimal point after it
} fude_zoom_meter_shown;

fude_zoom_meter fude_zoom_display_meter(const c8* _text);
// What it shows reading _value (V or A): its digits; past its range, a 1 alone.
void fude_zoom_display_meter_show(const fude_zoom_meter* _m, f64 _value, fude_zoom_meter_shown* _out);

// --- an oscilloscope -----------------------------------------------------------------------------

#define FUDE_ZOOM_SCOPE_SAMPLES 400u   // a sweep's (10 divisions across)
#define FUDE_ZOOM_SCOPE_DIVS_X  10.0
#define FUDE_ZOOM_SCOPE_DIVS_Y  8.0

// What an oscilloscope's text says ("1ms 2V", "1ms 2V 0.5V", "trig 1.5V"): its time a division (seconds), CH1's volts a
// division, the level CH1 rises through to start a sweep (NAN: halfway between what the last sweep saw at its least and
// most), CH2's volts a division (a second volts: its own; else CH1's), and where each channel's 0 V is (divisions up from
// the screen's middle: its POSITION knob's, 0 as it comes).
typedef struct {
    f64 time_div, volt_div, level, volt_div2, pos1, pos2;
} fude_zoom_scope;

fude_zoom_scope fude_zoom_display_scope(const c8* _text);

// Its face: its screen on its left (a 10 × 8 graticule), its knobs on its right — as a bench oscilloscope's, each
// channel's VOLTS/DIV (how big its wave is: the fewer volts a division, the bigger) over its POSITION (where its 0 V is),
// CH1's column and CH2's, and TIME/DIV under them (how much of the wave a sweep shows). Each knob turns in steps: tapped on
// its right half, a step clockwise (bigger: fewer volts or less time a division — the wave grown, stretched —, or up);
// on its left, back. Volts and time go 1, 2, 5, 10... (1 mV to 50 V; 10 µs to 5 s), position half a division at a time
// (8 either way).
enum {
    FUDE_ZOOM_SCOPE_KNOB_VOLTS1, FUDE_ZOOM_SCOPE_KNOB_VOLTS2, FUDE_ZOOM_SCOPE_KNOB_POS1, FUDE_ZOOM_SCOPE_KNOB_POS2, FUDE_ZOOM_SCOPE_KNOB_TIME,
    FUDE_ZOOM_SCOPE_KNOBS
};
#define FUDE_ZOOM_SCOPE_POS_MOST 8.0   // divisions a channel's 0 V goes up or down at most

// A circuit's oscilloscope's settings as they are now (its text's, its knobs turned since: circuit.c's values 0–5).
fude_zoom_scope fude_zoom_display_scope_of(const fude_zoom_circuit_part* _q);
// Its screen on its face (of its half sizes: u across, v up).
void fude_zoom_scope_screen(f64* _u0, f64* _v0, f64* _u1, f64* _v1);
// Knob _k's middle on its face (of its half sizes) and its radius (of its half height).
void fude_zoom_scope_knob(u32 _k, f64* _u, f64* _v, f64* _r);
// The knob a point on its face is on — anywhere on its panel: each knob's cell its name and the room round it (of its half
// sizes) —, and which way a tap there turns it (+1: right of its middle, -1: left). FUDE_ZOOM_NONE: off its panel.
u32  fude_zoom_scope_knob_at(f64 _u, f64 _v, i32* _way);
// Knob _k turned a step _way (+1, -1) on _s. False: at its end already.
b8   fude_zoom_scope_turn(fude_zoom_scope* _s, u32 _k, i32 _way);
// How far round knob _k is on _s: 0 at its stop counter-clockwise, 1 at its clockwise one.
f64  fude_zoom_scope_knob_round(const fude_zoom_scope* _s, u32 _k);
// Its name ("CH1 VOLTS/DIV") and what it is set to ("0.5 V", "+1.5", "200 µs").
const c8* fude_zoom_scope_knob_name(u32 _k);
void fude_zoom_scope_knob_say(const fude_zoom_scope* _s, u32 _k, c8* _out, usize _size);

// One step of the circuit's for an oscilloscope (state: 8 numbers, a part's; samples: CH1's, then CH2's, a sweep each):
// the voltages it has now (_v1, _v2) at time _t. Sampled evenly along each sweep (what lies between the steps from the
// voltages either side), a sweep started when CH1 rises through the level — or, waiting twice a sweep for it, at once
// (auto). Its first call (_first): where it starts from.
void fude_zoom_scope_step(const fude_zoom_scope* _s, f64* _state, f32* _samples, f64 _t, f64 _v1, f64 _v2, b8 _first);
// How far the sweep has got (samples taken; FUDE_ZOOM_SCOPE_SAMPLES: waiting for its trigger).
u32  fude_zoom_scope_at(const f64* _state);

// --- drawn in Play -------------------------------------------------------------------------------

// What display part _q shows, over its symbol: _all its own units to the screen, _hw × _hh its half sizes there, _led its
// LEDs' colour, _time the circuit's (a cursor's blink).
void fude_zoom_display_render(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh, rde_color _led,
                              f64 _time);

#endif
