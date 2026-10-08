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

// --- drawn in Play -------------------------------------------------------------------------------

// What display part _q shows, over its symbol: _all its own units to the screen, _hw × _hh its half sizes there, _led its
// LEDs' colour, _time the circuit's (a cursor's blink).
void fude_zoom_display_render(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh, rde_color _led,
                              f64 _time);

#endif
