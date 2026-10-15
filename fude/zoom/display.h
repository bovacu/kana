// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_DISPLAY_H
#define FUDE_ZOOM_DISPLAY_H

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/circuit.h"

// ===========================================================================
// Displays: the circuit parts that show something (Electronics; models of circuit.h), and how what they show is
// worked out and drawn.
//
//   - A PANEL of 7-segment digits (1 to 8). Each digit's segments are joined to the other digits' and each digit has
//     a common pin, so it is multiplexed as real panels are: what is lit often enough is seen as steady.
//   - An LED MATRIX of any size up to 32 × 32: a pin for each row, a pin for each column, and an LED at each crossing.
//   - A 10-LED BAR GRAPH, and an LM3914 to drive it: a voltage in, and a bar or a dot that far up.
//   - A PANEL METER: an LCD with a voltmeter's or an ammeter's 3½ digits.
//   - A CHARACTER LCD with an HD44780 controller. Its commands and characters are written on its pins as a
//     microcontroller would write them, or by hand (switches on its data pins, a button on E).
//   - An OSCILLOSCOPE: its screen, its knobs and its sweeps.
//
// The panel, the matrix and the LCD are SIZED: their text says how big ("4 digits"; "8x8", columns × rows; "16x2").
// "CA" (or "anode") makes a panel's commons anodes, and "CC" makes a matrix's rows cathodes. A circuit.h part is made
// for each size and way round once, and kept: its pins (20 points apart, as every part's) and its room.
//
// What the eye sees of an LED (a panel's segment, a matrix's dot, a bar) is its current held for a moment
// (FUDE_ZOOM_DISPLAY_GLOW), so a multiplexed digit lit a quarter of the time looks steady and a quarter as bright.
//
// Where it sits: `fude/zoom`, the canvas layer. circuit.c steps these parts as the circuit plays, page_circuit.c and
// placer.c make and edit them, and fude_zoom_display_render draws them in Play.
// Main types: the sized parts are made here and kept in a list of this module's for as long as the app runs, because
// circuits hold pointers to them; the LCD controller, meter and scope settings are plain values the caller keeps
// (the circuit keeps an LCD's controller in the part's store: circuit_step.c).
// Threads: main thread only. The list of sized parts is shared and not locked, and drawing uses the renderer.
// Design: docs/infinite_canvas_design.md §51 (the displays) and §52, §57 (the oscilloscope and its knobs).
// ===========================================================================

#define FUDE_ZOOM_DISPLAY_DIGITS 8u      // the most digits a panel has
#define FUDE_ZOOM_DISPLAY_SIDE   32u     // the most rows or columns a matrix has
#define FUDE_ZOOM_DISPLAY_GLOW   0.02    // seconds the eye holds a light
#define FUDE_ZOOM_DISPLAY_FULL   0.01    // the current (amperes) through an LED that is seen as fully bright

// --- sized parts ---------------------------------------------------------------------------------

// Returns the part that a display's text makes of its kind's _template, at the size and way round the text says.
// _template may also be a sized part made before, of another size. It returns _template itself when it is not a sized
// display. The part is made once for each size and kept for as long as the app runs.
const fude_zoom_part* fude_zoom_display_part(const fude_zoom_part* _template, const c8* _text);
// Returns whether a part is a sized display's, made by fude_zoom_display_part.
b8   fude_zoom_display_made(const fude_zoom_part* _part);
// Return a sized display's columns and rows: a panel's digits and 1; a matrix's dots; an LCD's characters. 0 when
// the part is not a sized display.
u32  fude_zoom_display_cols(const fude_zoom_part* _part);
u32  fude_zoom_display_rows(const fude_zoom_part* _part);
// Gives a sized display's room as it comes (the catalogue's units, width × height). Returns false when it is not one.
b8   fude_zoom_display_room(const fude_zoom_part* _part, f64* _w, f64* _h);
// Sets where its pins' names go inside it, as fractions of its half sizes: _u for a left or right pin's, _v for a top
// or bottom one's. Leaves them as they were when the part is not a sized display.
void fude_zoom_display_label_inset(const fude_zoom_part* _part, f32* _u, f32* _v);

// --- its LEDs ------------------------------------------------------------------------------------

// Returns how many LEDs a display has (a panel's 8 for each digit, a matrix's one for each dot, a bar graph's 10);
// 0 for none.
u32  fude_zoom_display_lights(const fude_zoom_part* _part);
// Gives LED _k's anode and cathode pins. On a panel, LED _k is digit _k / 8's segment _k % 8 (a b c d e f g dp); on a
// matrix, it is row _k / columns, column _k % columns. Returns false when there is no such LED.
b8   fude_zoom_display_led(const fude_zoom_part* _part, u32 _k, u32* _anode, u32* _cathode);
// Returns the segments that show a character on a 7-segment digit ('0'–'9', '-', and ' '; anything else shows
// nothing): bit 0 is segment a, up to bit 6, segment g.
u8   fude_zoom_display_segments(c8 _c);

// --- a character LCD (an HD44780) ----------------------------------------------------------------

// The controller's mode bits. Function set: DL and N. Display control: D, C and B. Entry mode: ID and S.
#define FUDE_ZOOM_LCD_DL 0x10u   // function set: 8 data bits (else 4)
#define FUDE_ZOOM_LCD_N  0x08u   // function set: two lines (else one)
#define FUDE_ZOOM_LCD_D  0x04u   // display control: the display is on
#define FUDE_ZOOM_LCD_C  0x02u   // display control: the cursor is shown
#define FUDE_ZOOM_LCD_B  0x01u   // display control: the cursor blinks
#define FUDE_ZOOM_LCD_ID 0x02u   // entry mode: the address counts up (else down)
#define FUDE_ZOOM_LCD_S  0x01u   // entry mode: the display moves as characters are written

// What the controller holds: its memory of 80 characters (two lines of 40, or one of 80); 8 characters of its own
// (CGRAM, 5 × 8 dots each); its address counter and which memory it points into; its modes; the display's shift;
// half a byte (in 4-bit mode, the high half waiting for its low half); E's level at the last step; whether it has
// power; and whether it has been told anything since it came on. Until it has, its first row shows dark blocks, as a
// real one does when it is powered but not set up.
typedef struct {
    u8 ddram[80];
    u8 cgram[64];
    u8 ac;          // its address counter
    u8 cg;          // which memory the counter points into: CGRAM (1) or the characters (0)
    u8 entry;       // FUDE_ZOOM_LCD_ID | _S
    u8 control;     // FUDE_ZOOM_LCD_D | _C | _B
    u8 function;    // FUDE_ZOOM_LCD_DL | _N (and the 5 × 10 dot font bit, which is not drawn)
    u8 half;        // in 4-bit mode: 1 while a high half waits for its low half
    u8 nibble;      // that high half
    u8 e;           // whether E was high at the last step, to find where it falls
    u8 powered;
    u8 fresh;       // nothing has been written since it came on
    u8 spare;
    i8 shift;       // the display's window along its lines: how many characters it has moved left
    u8 spare2[4];
} fude_zoom_lcd;

// Sets the controller as it comes on: every character a space, 8-bit mode, one line, the display off, counting up.
void fude_zoom_lcd_power_on(fude_zoom_lcd* _l);
// Gives the controller a byte: a command when _rs is 0, a character (or a CGRAM row) when it is 1.
void fude_zoom_lcd_write(fude_zoom_lcd* _l, b8 _rs, u8 _byte);
// Called when E falls, with its data pins as read (bit _i set when D_i is high). In 8-bit mode it takes the whole
// byte; in 4-bit mode it takes half of one from D7–D4, the high half first.
void fude_zoom_lcd_strobe(fude_zoom_lcd* _l, b8 _rs, u8 _pins);
// Returns the character code a display of _cols × _rows shows at column _col of row _row (a space shows nothing), and
// sets *_cursor to whether the cursor is there (_cursor may be NULL). Rows that no line drives show spaces: with one
// line on four rows, the second and the fourth.
u8   fude_zoom_lcd_shown(const fude_zoom_lcd* _l, u32 _cols, u32 _rows, u32 _col, u32 _row, b8* _cursor);
// Writes a character's 5 × 8 dots into _rows (8 bytes), one row each from the top, bit 4 being its left column. Codes
// 0–7 (and 8–15 again) are its own characters from CGRAM; the rest come from its ROM, the A00's: ASCII but ¥ for the
// backslash, → and ←, and ° (codes it does not draw, such as katakana, stay blank).
void fude_zoom_lcd_glyph(const fude_zoom_lcd* _l, u8 _code, u8* _rows);

// --- a panel meter -------------------------------------------------------------------------------

// What a panel meter reads, from its text ("20V", "200mV", "2A", "20mA"): `full` is its range's top, in volts or
// amperes (`amps`); `times` turns that into its range's own unit (1, or 1000 for mV and mA); `decimals` is how many of
// its digits come after the point, so that its 3½ digits show at most 1999.
typedef struct {
    f64 full;
    b8  amps;
    f64 times;
    u32 decimals;
} fude_zoom_meter;

// What a panel meter shows: a minus sign, four digits and the decimal points between them.
typedef struct {
    b8 minus;
    c8 digit[4];   // '0'–'9' or ' ' (the first is its half digit: '1' or blank)
    b8 point[4];   // a decimal point after this digit
} fude_zoom_meter_shown;

// Reads a panel meter's text (a range of 20 when it gives none). Its unit is a whole token, a number's ("20V", "2 A"):
// "AC 20V" reads volts, an A in a word being no unit; volts when it gives none.
fude_zoom_meter fude_zoom_display_meter(const c8* _text);
// Works out what it shows when it reads _value (volts or amperes): its digits, or a 1 alone when the value is past its
// range, as a real one shows.
void fude_zoom_display_meter_show(const fude_zoom_meter* _m, f64 _value, fude_zoom_meter_shown* _out);

// --- an oscilloscope -----------------------------------------------------------------------------

#define FUDE_ZOOM_SCOPE_SAMPLES 400u   // samples in a sweep, across its 10 divisions
// Its screen's divisions, across and up.
#define FUDE_ZOOM_SCOPE_DIVS_X  10.0
#define FUDE_ZOOM_SCOPE_DIVS_Y  8.0

// An oscilloscope's settings, as its text says them ("1ms 2V", "1ms 2V 0.5V", "trig 1.5V"): the time a division
// (seconds; 1 ms when not given); CH1's volts a division (2 V when not given); the level CH1 rises through to start a
// sweep (NaN: halfway between the least and most the last sweep saw); CH2's volts a division (a second volts value
// gives it, else it is CH1's); and where each channel's 0 V is, in divisions up from the screen's middle (its
// POSITION knob's setting, 0 as it comes).
typedef struct {
    f64 time_div, volt_div, level, volt_div2, pos1, pos2;
} fude_zoom_scope;

// Reads an oscilloscope's text into its settings.
fude_zoom_scope fude_zoom_display_scope(const c8* _text);

// Its face: the screen on its left (a 10 × 8 graticule), the knobs on its right, as on a bench oscilloscope. Each
// channel's VOLTS/DIV (how big its wave is: the fewer volts a division, the bigger) is over its POSITION (where its
// 0 V is), CH1's column then CH2's, and TIME/DIV is under them (how much of the wave a sweep shows). Each knob turns in
// steps. Tapped on its right half it turns a step clockwise: bigger (fewer volts or less time a division, so the wave
// grows or stretches), or up. Tapped on its left half it turns back. Volts and time go 1, 2, 5, 10... (1 mV to 50 V;
// 10 µs to 5 s), and a position moves half a division at a time (at most 8 either way).
enum {
    FUDE_ZOOM_SCOPE_KNOB_VOLTS1, FUDE_ZOOM_SCOPE_KNOB_VOLTS2, FUDE_ZOOM_SCOPE_KNOB_POS1, FUDE_ZOOM_SCOPE_KNOB_POS2, FUDE_ZOOM_SCOPE_KNOB_TIME,
    FUDE_ZOOM_SCOPE_KNOBS
};
#define FUDE_ZOOM_SCOPE_POS_MOST 8.0   // the most divisions a channel's 0 V moves up or down

// Returns a circuit oscilloscope's settings as they are now: its text's, with its knobs' turns since then (the circuit
// keeps them in the part's values 0–5: circuit_read.c reads them from its text, circuit_play.c turns them).
fude_zoom_scope fude_zoom_display_scope_of(const fude_zoom_circuit_part* _q);
// Gives where the screen is on its face, as fractions of its half sizes (u across, v up).
void fude_zoom_scope_screen(f64* _u0, f64* _v0, f64* _u1, f64* _v1);
// Gives knob _k's middle on its face (fractions of its half sizes) and its radius (a fraction of its half height).
void fude_zoom_scope_knob(u32 _k, f64* _u, f64* _v, f64* _r);
// Returns the knob that a point on its face belongs to, and sets *_way to the way a tap there turns it (+1 right of
// the knob's middle, -1 left of it). Every point on the knobs' panel belongs to some knob: each knob's cell holds its
// name and the room round it. Returns FUDE_ZOOM_NONE off the panel.
u32  fude_zoom_scope_knob_at(f64 _u, f64 _v, i32* _way);
// Turns knob _k on _s a step _way (+1 clockwise, -1 back). Returns false when it is at its end already.
b8   fude_zoom_scope_turn(fude_zoom_scope* _s, u32 _k, i32 _way);
// Returns how far round knob _k is on _s: 0 at its counter-clockwise stop, 1 at its clockwise one.
f64  fude_zoom_scope_knob_round(const fude_zoom_scope* _s, u32 _k);
// fude_zoom_scope_knob_name returns a knob's name ("CH1 VOLTS/DIV"; a static string, "" for no such knob);
// fude_zoom_scope_knob_say writes what it is set to ("0.5 V", "+1.5", "200 µs") into _out.
const c8* fude_zoom_scope_knob_name(u32 _k);
void fude_zoom_scope_knob_say(const fude_zoom_scope* _s, u32 _k, c8* _out, usize _size);

// Takes one circuit step for an oscilloscope: its voltages now (_v1, _v2) at time _t. _state is the part's 8 numbers
// (the sweep's start, the next sample, the last step's voltages and time, since when it has waited, and the least and
// most CH1 was in the last sweep); _samples holds CH1's sweep, then CH2's. Samples are taken evenly along each sweep,
// those between two steps from the voltages on either side. A sweep starts when CH1 rises through the level, or at
// once (auto) after waiting two sweeps' time for it. On its first call (_first) it only notes where it starts from.
void fude_zoom_scope_step(const fude_zoom_scope* _s, f64* _state, f32* _samples, f64 _t, f64 _v1, f64 _v2, b8 _first);
// Returns how far the sweep has got: the samples taken so far, or FUDE_ZOOM_SCOPE_SAMPLES while it waits for its
// trigger.
u32  fude_zoom_scope_at(const f64* _state);

// --- drawn in Play -------------------------------------------------------------------------------

// Draws what display part _q shows, over its symbol. _all takes its own units to the screen, _hw × _hh are its half
// sizes there, _led is its LEDs' colour, and _time is the circuit's time (for the cursor's blink). Called while the
// canvas is drawn.
void fude_zoom_display_render(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh, rde_color _led,
                              f64 _time);

#endif
