// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, the parts: the catalogue (each kind's pins and what it is, FZC_PARTS, found by its
// symbol's id), a breadboard's holes, and the part an object is, with its pins where they are on the canvas.
//
// The file is in three parts, in order: the catalogue (the pin tables, then FZC_PARTS, then a part found, its insets,
// labels and room); a breadboard (its holes worked out from their rows and columns, joined in strips); and pins where
// they are (the part an object is, a custom part's or a sized display's from its text, a pin in its frame's units, the
// parts' grid). FZP, DL, DR, HL, HR, FZ, FZ_SIM, FZ_SIZED and FZN are the catalogue's table macros.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. The catalogue is static; finding a part by its kind fills a static cache. Main thread only.
// ===========================================================================

// --- the catalogue ------------------------------------------------------------------------------------

// A pin at (_u, _v) of its part (from -1 to 1 of its half sizes), named _name, its wire leaving by side _side.
#define FZP(_u, _v, _name, _side) { (f32)(_u), (f32)(_v), _name, FUDE_ZOOM_PIN_##_side }
// A DIP's pin _k down its left (from its top), or _j up its right (from its bottom), of _n.
#define DL(_n, _k, _name) { -1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) / 2 + 1), _name, FUDE_ZOOM_PIN_LEFT }
#define DR(_n, _j, _name) { 1.0f, -1.0f + 2.0f * (f32)((_j) + 1) / (f32)((_n) / 2 + 1), _name, FUDE_ZOOM_PIN_RIGHT }
// A header's pin _k (from its top) of _n down its left; a board's down its left or right, rows of _n.
#define HL(_n, _k, _name) { -1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) + 1), _name, FUDE_ZOOM_PIN_LEFT }
#define HR(_n, _k, _name) { 1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) + 1), _name, FUDE_ZOOM_PIN_RIGHT }

// Each kind's pins, in the order its model reads them (the solve and the limits name pins by their index).
RDE_INTERNAL const fude_zoom_pin FZC_TWO[]      = { FZP(-1, 0, "", LEFT), FZP(1, 0, "", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_DIODE_P[]  = { FZP(-1, 0, "A", LEFT), FZP(1, 0, "K", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_POT_P[]    = { FZP(-1, -0.5, "", LEFT), FZP(1, -0.5, "", RIGHT), FZP(0, 1, "", UP) };
RDE_INTERNAL const fude_zoom_pin FZC_BJT_N[]    = { FZP(-1, 0, "B", LEFT), FZP(1.0 / 3.0, 1, "C", UP), FZP(1.0 / 3.0, -1, "E", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_BJT_P[]    = { FZP(-1, 0, "B", LEFT), FZP(1.0 / 3.0, -1, "C", DOWN), FZP(1.0 / 3.0, 1, "E", UP) };
RDE_INTERNAL const fude_zoom_pin FZC_FET_N[]    = { FZP(-1, 0, "G", LEFT), FZP(1.0 / 3.0, 1, "D", UP), FZP(1.0 / 3.0, -1, "S", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_FET_P[]    = { FZP(-1, 0, "G", LEFT), FZP(1.0 / 3.0, -1, "D", DOWN), FZP(1.0 / 3.0, 1, "S", UP) };
RDE_INTERNAL const fude_zoom_pin FZC_SUPPLY[]   = { FZP(0, 1, "+", UP), FZP(0, -1, "-", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_GROUND_P[] = { FZP(0, 1, "", UP) };
RDE_INTERNAL const fude_zoom_pin FZC_RAIL_P[]   = { FZP(0, -1, "", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_BUZZER_P[] = { FZP(-1, -0.5, "", LEFT), FZP(1, -0.5, "", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_OPAMP_P[]  = { FZP(-1, 0.5, "-", LEFT), FZP(-1, -0.5, "+", LEFT), FZP(1, 0, "OUT", RIGHT), FZP(0, 1, "V+", UP), FZP(0, -1, "V-", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_SEG_P[]    = { FZP(-1, 2.0 / 3.0, "a", LEFT), FZP(-1, 1.0 / 3.0, "b", LEFT), FZP(-1, 0, "c", LEFT), FZP(-1, -1.0 / 3.0, "d", LEFT),
                                                    FZP(1, 2.0 / 3.0, "e", RIGHT), FZP(1, 1.0 / 3.0, "f", RIGHT), FZP(1, 0, "g", RIGHT), FZP(1, -1.0 / 3.0, "dp", RIGHT),
                                                    FZP(0, -1, "COM", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_GATE2[]    = { FZP(-1, 2.0 / 3.0, "", LEFT), FZP(-1, -2.0 / 3.0, "", LEFT), FZP(1, 0, "", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_GATE1[]    = { FZP(-1, 0, "", LEFT), FZP(1, 0, "", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_DFF_P[]    = { FZP(-1, 0.5, "D", LEFT), FZP(-1, -0.5, "CLK", LEFT), FZP(1, 0.5, "Q", RIGHT), FZP(1, -0.5, "/Q", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_TFF_P[]    = { FZP(-1, 0.5, "T", LEFT), FZP(-1, -0.5, "CLK", LEFT), FZP(1, 0.5, "Q", RIGHT), FZP(1, -0.5, "/Q", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_OUT1[]     = { FZP(1, 0, "", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_IN1[]      = { FZP(-1, 0, "", LEFT) };
RDE_INTERNAL const fude_zoom_pin FZC_REG_P[]    = { FZP(-1, 0.5, "IN", LEFT), FZP(0, -1, "GND", DOWN), FZP(1, 0.5, "OUT", RIGHT) };

// Chips (DIP: pin 1 at the top left, on down and round, counter-clockwise).
RDE_INTERNAL const fude_zoom_pin FZC_555[] = { DL(8, 0, "GND"), DL(8, 1, "TRIG"), DL(8, 2, "OUT"), DL(8, 3, "RESET"),
                                               DR(8, 0, "CTRL"), DR(8, 1, "THRES"), DR(8, 2, "DISCH"), DR(8, 3, "VCC") };
RDE_INTERNAL const fude_zoom_pin FZC_LM358[] = { DL(8, 0, "OUT1"), DL(8, 1, "IN1-"), DL(8, 2, "IN1+"), DL(8, 3, "GND"),
                                                 DR(8, 0, "IN2+"), DR(8, 1, "IN2-"), DR(8, 2, "OUT2"), DR(8, 3, "VCC") };
RDE_INTERNAL const fude_zoom_pin FZC_328P[] = {
    DL(28, 0, "RESET"), DL(28, 1, "D0"), DL(28, 2, "D1"), DL(28, 3, "D2"), DL(28, 4, "D3"), DL(28, 5, "D4"), DL(28, 6, "VCC"),
    DL(28, 7, "GND"), DL(28, 8, "XTAL1"), DL(28, 9, "XTAL2"), DL(28, 10, "D5"), DL(28, 11, "D6"), DL(28, 12, "D7"), DL(28, 13, "D8"),
    DR(28, 0, "D9"), DR(28, 1, "D10"), DR(28, 2, "D11"), DR(28, 3, "D12"), DR(28, 4, "D13"), DR(28, 5, "AVCC"), DR(28, 6, "AREF"),
    DR(28, 7, "GND"), DR(28, 8, "A0"), DR(28, 9, "A1"), DR(28, 10, "A2"), DR(28, 11, "A3"), DR(28, 12, "A4"), DR(28, 13, "A5") };
RDE_INTERNAL const fude_zoom_pin FZC_595[] = { DL(16, 0, "QB"), DL(16, 1, "QC"), DL(16, 2, "QD"), DL(16, 3, "QE"), DL(16, 4, "QF"), DL(16, 5, "QG"),
                                               DL(16, 6, "QH"), DL(16, 7, "GND"), DR(16, 0, "QH'"), DR(16, 1, "SRCLR"), DR(16, 2, "SRCLK"), DR(16, 3, "RCLK"),
                                               DR(16, 4, "OE"), DR(16, 5, "SER"), DR(16, 6, "QA"), DR(16, 7, "VCC") };
RDE_INTERNAL const fude_zoom_pin FZC_293[] = { DL(16, 0, "EN1,2"), DL(16, 1, "1A"), DL(16, 2, "1Y"), DL(16, 3, "GND"), DL(16, 4, "GND"), DL(16, 5, "2Y"),
                                               DL(16, 6, "2A"), DL(16, 7, "VCC2"), DR(16, 0, "EN3,4"), DR(16, 1, "3A"), DR(16, 2, "3Y"), DR(16, 3, "GND"),
                                               DR(16, 4, "GND"), DR(16, 5, "4Y"), DR(16, 6, "4A"), DR(16, 7, "VCC1") };
RDE_INTERNAL const fude_zoom_pin FZC_2003[] = { DL(16, 0, "IN1"), DL(16, 1, "IN2"), DL(16, 2, "IN3"), DL(16, 3, "IN4"), DL(16, 4, "IN5"), DL(16, 5, "IN6"),
                                                DL(16, 6, "IN7"), DL(16, 7, "GND"), DR(16, 0, "COM"), DR(16, 1, "OUT7"), DR(16, 2, "OUT6"), DR(16, 3, "OUT5"),
                                                DR(16, 4, "OUT4"), DR(16, 5, "OUT3"), DR(16, 6, "OUT2"), DR(16, 7, "OUT1") };
RDE_INTERNAL const fude_zoom_pin FZC_7219[] = {
    DL(24, 0, "DIN"), DL(24, 1, "DIG0"), DL(24, 2, "DIG4"), DL(24, 3, "GND"), DL(24, 4, "DIG6"), DL(24, 5, "DIG2"), DL(24, 6, "DIG3"),
    DL(24, 7, "DIG7"), DL(24, 8, "GND"), DL(24, 9, "DIG5"), DL(24, 10, "DIG1"), DL(24, 11, "LOAD"),
    DR(24, 0, "CLK"), DR(24, 1, "SEG A"), DR(24, 2, "SEG F"), DR(24, 3, "SEG B"), DR(24, 4, "SEG G"), DR(24, 5, "ISET"),
    DR(24, 6, "V+"), DR(24, 7, "SEG C"), DR(24, 8, "SEG E"), DR(24, 9, "SEG DP"), DR(24, 10, "SEG D"), DR(24, 11, "DOUT") };
RDE_INTERNAL const fude_zoom_pin FZC_8574[] = { DL(16, 0, "A0"), DL(16, 1, "A1"), DL(16, 2, "A2"), DL(16, 3, "P0"), DL(16, 4, "P1"), DL(16, 5, "P2"),
                                                DL(16, 6, "P3"), DL(16, 7, "GND"), DR(16, 0, "P4"), DR(16, 1, "P5"), DR(16, 2, "P6"), DR(16, 3, "P7"),
                                                DR(16, 4, "INT"), DR(16, 5, "SCL"), DR(16, 6, "SDA"), DR(16, 7, "VCC") };

// Modules (a breakout's header, down its left).
RDE_INTERNAL const fude_zoom_pin FZC_DS3231[] = { HL(6, 0, "32K"), HL(6, 1, "SQW"), HL(6, 2, "SCL"), HL(6, 3, "SDA"), HL(6, 4, "VCC"), HL(6, 5, "GND") };
RDE_INTERNAL const fude_zoom_pin FZC_MPU[]    = { HL(8, 0, "VCC"), HL(8, 1, "GND"), HL(8, 2, "SCL"), HL(8, 3, "SDA"), HL(8, 4, "XDA"), HL(8, 5, "XCL"),
                                                  HL(8, 6, "AD0"), HL(8, 7, "INT") };
RDE_INTERNAL const fude_zoom_pin FZC_NRF[]    = { HL(8, 0, "GND"), HL(8, 1, "VCC"), HL(8, 2, "CE"), HL(8, 3, "CSN"), HL(8, 4, "SCK"), HL(8, 5, "MOSI"),
                                                  HL(8, 6, "MISO"), HL(8, 7, "IRQ") };
RDE_INTERNAL const fude_zoom_pin FZC_HC05[]   = { HL(6, 0, "STATE"), HL(6, 1, "RXD"), HL(6, 2, "TXD"), HL(6, 3, "GND"), HL(6, 4, "VCC"), HL(6, 5, "EN") };
RDE_INTERNAL const fude_zoom_pin FZC_SR04[]   = { HL(4, 0, "VCC"), HL(4, 1, "TRIG"), HL(4, 2, "ECHO"), HL(4, 3, "GND") };
RDE_INTERNAL const fude_zoom_pin FZC_DHT[]    = { HL(3, 0, "VCC"), HL(3, 1, "DATA"), HL(3, 2, "GND") };
RDE_INTERNAL const fude_zoom_pin FZC_SERVO[]  = { HL(3, 0, "GND"), HL(3, 1, "VCC"), HL(3, 2, "SIG") };
RDE_INTERNAL const fude_zoom_pin FZC_TP4056[] = { HL(6, 0, "IN+"), HL(6, 1, "IN-"), HL(6, 2, "BAT+"), HL(6, 3, "BAT-"), HL(6, 4, "OUT+"), HL(6, 5, "OUT-") };
RDE_INTERNAL const fude_zoom_pin FZC_WS2812[] = { HL(4, 0, "DIN"), HL(4, 1, "VCC"), HL(4, 2, "GND"), HL(4, 3, "DOUT") };
RDE_INTERNAL const fude_zoom_pin FZC_L298N[]  = { HL(13, 0, "IN1"), HL(13, 1, "IN2"), HL(13, 2, "IN3"), HL(13, 3, "IN4"), HL(13, 4, "ENA"), HL(13, 5, "ENB"),
                                                  HL(13, 6, "OUT1"), HL(13, 7, "OUT2"), HL(13, 8, "OUT3"), HL(13, 9, "OUT4"), HL(13, 10, "12V"), HL(13, 11, "GND"),
                                                  HL(13, 12, "5V") };
RDE_INTERNAL const fude_zoom_pin FZC_RELAY[]  = { HL(6, 0, "VCC"), HL(6, 1, "GND"), HL(6, 2, "IN"), HL(6, 3, "COM"), HL(6, 4, "NO"), HL(6, 5, "NC") };
RDE_INTERNAL const fude_zoom_pin FZC_I2C4[]   = { HL(4, 0, "GND"), HL(4, 1, "VCC"), HL(4, 2, "SDA"), HL(4, 3, "SCL") };
// A 10-LED bar graph: its anodes down its left, its cathodes down its right (display.h).
RDE_INTERNAL const fude_zoom_pin FZC_BAR_P[]  = {
    HL(10, 0, "A1"), HL(10, 1, "A2"), HL(10, 2, "A3"), HL(10, 3, "A4"), HL(10, 4, "A5"), HL(10, 5, "A6"), HL(10, 6, "A7"), HL(10, 7, "A8"),
    HL(10, 8, "A9"), HL(10, 9, "A10"), HR(10, 0, "K1"), HR(10, 1, "K2"), HR(10, 2, "K3"), HR(10, 3, "K4"), HR(10, 4, "K5"), HR(10, 5, "K6"),
    HR(10, 6, "K7"), HR(10, 7, "K8"), HR(10, 8, "K9"), HR(10, 9, "K10"),
};
RDE_INTERNAL const fude_zoom_pin FZC_3914[]   = {
    DL(18, 0, "LED1"), DL(18, 1, "V-"), DL(18, 2, "V+"), DL(18, 3, "RLO"), DL(18, 4, "SIG"), DL(18, 5, "RHI"), DL(18, 6, "REFOUT"),
    DL(18, 7, "REFADJ"), DL(18, 8, "MODE"), DR(18, 0, "LED10"), DR(18, 1, "LED9"), DR(18, 2, "LED8"), DR(18, 3, "LED7"), DR(18, 4, "LED6"),
    DR(18, 5, "LED5"), DR(18, 6, "LED4"), DR(18, 7, "LED3"), DR(18, 8, "LED2"),
};
// A panel meter's + and −; a switch's common and its throws (a DPDT's two poles); an oscilloscope's probes and ground.
RDE_INTERNAL const fude_zoom_pin FZC_METER_P[] = { FZP(-1, 0, "+", LEFT), FZP(1, 0, "-", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_SPDT_P[] = { FZP(-1, 0, "COM", LEFT), FZP(1, 2.0 / 3.0, "A", RIGHT), FZP(1, -2.0 / 3.0, "B", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_DPDT_P[] = { FZP(-1, 2.0 / 3.0, "COM1", LEFT), FZP(1, 2.0 / 3.0, "A1", RIGHT), FZP(1, 1.0 / 3.0, "B1", RIGHT),
                                                  FZP(-1, -1.0 / 3.0, "COM2", LEFT), FZP(1, -1.0 / 3.0, "A2", RIGHT), FZP(1, -2.0 / 3.0, "B2", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_SCOPE_P[] = { FZP(-1, 0.5, "CH1", LEFT), FZP(-1, 0, "CH2", LEFT), FZP(-1, -0.5, "GND", LEFT) };
// Worked by, or working, a mechanism: a solenoid's coil (its leads by its left), a stepper's A–D and COM, an encoder's
// A, C, B, a slotted sensor's LED (left) and phototransistor (right).
RDE_INTERNAL const fude_zoom_pin FZC_SOLENOID_P[] = { FZP(-1, 0.5, "", LEFT), FZP(-1, -0.5, "", LEFT) };
RDE_INTERNAL const fude_zoom_pin FZC_STEPPER_P[]  = { HL(5, 0, "A"), HL(5, 1, "B"), HL(5, 2, "C"), HL(5, 3, "D"), HL(5, 4, "COM") };
RDE_INTERNAL const fude_zoom_pin FZC_ENCODER_P[]  = { HL(3, 0, "A"), HL(3, 1, "C"), HL(3, 2, "B") };
// (0.1.75) A JK flip-flop's J, CLK, K and Q, /Q; an SR latch's; a thyristor's (its gate under its cathode's side);
// a transformer's windings; a bridge's AC pins over and under it, + right, − left; a phototransistor's; a speaker's
// leads.
RDE_INTERNAL const fude_zoom_pin FZC_JK_P[]      = { FZP(-1, 0.5, "J", LEFT), FZP(-1, 0, "CLK", LEFT), FZP(-1, -0.5, "K", LEFT), FZP(1, 0.5, "Q", RIGHT),
                                                     FZP(1, -0.5, "/Q", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_SR_P[]      = { FZP(-1, 0.5, "S", LEFT), FZP(-1, -0.5, "R", LEFT), FZP(1, 0.5, "Q", RIGHT), FZP(1, -0.5, "/Q", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_SCR_P[]     = { FZP(-1, 0, "A", LEFT), FZP(1, 0, "K", RIGHT), FZP(1.0 / 3.0, -1, "G", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_TRIAC_P[]   = { FZP(-1, 0, "MT2", LEFT), FZP(1, 0, "MT1", RIGHT), FZP(1.0 / 3.0, -1, "G", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_XFMR_P[]    = { FZP(-1, 0.5, "P1", LEFT), FZP(-1, -0.5, "P2", LEFT), FZP(1, 0.5, "S1", RIGHT), FZP(1, -0.5, "S2", RIGHT) };
RDE_INTERNAL const fude_zoom_pin FZC_BRIDGE_P[]  = { FZP(0, 1, "AC1", UP), FZP(1, 0, "+", RIGHT), FZP(0, -1, "AC2", DOWN), FZP(-1, 0, "-", LEFT) };
RDE_INTERNAL const fude_zoom_pin FZC_PHOTOT_P[]  = { FZP(1.0 / 3.0, 1, "C", UP), FZP(1.0 / 3.0, -1, "E", DOWN) };
RDE_INTERNAL const fude_zoom_pin FZC_SPEAKER_P[] = { FZP(-1, 0.5, "", LEFT), FZP(-1, -0.5, "", LEFT) };
RDE_INTERNAL const fude_zoom_pin FZC_SLOT_P[]     = { FZP(-1, 1.0 / 3.0, "A", LEFT), FZP(-1, -1.0 / 3.0, "K", LEFT), FZP(1, 1.0 / 3.0, "C", RIGHT),
                                                      FZP(1, -1.0 / 3.0, "E", RIGHT) };

// Boards (their headers as on the board, down their left and their right).
RDE_INTERNAL const fude_zoom_pin FZC_UNO[] = {
    HL(16, 0, "IOREF"), HL(16, 1, "RESET"), HL(16, 2, "3V3"), HL(16, 3, "5V"), HL(16, 4, "GND"), HL(16, 5, "GND"), HL(16, 6, "VIN"),
    HL(16, 8, "A0"), HL(16, 9, "A1"), HL(16, 10, "A2"), HL(16, 11, "A3"), HL(16, 12, "A4"), HL(16, 13, "A5"),
    HR(16, 0, "AREF"), HR(16, 1, "GND"), HR(16, 2, "D13"), HR(16, 3, "D12"), HR(16, 4, "D11"), HR(16, 5, "D10"), HR(16, 6, "D9"), HR(16, 7, "D8"),
    HR(16, 8, "D7"), HR(16, 9, "D6"), HR(16, 10, "D5"), HR(16, 11, "D4"), HR(16, 12, "D3"), HR(16, 13, "D2"), HR(16, 14, "D1"), HR(16, 15, "D0") };
RDE_INTERNAL const fude_zoom_pin FZC_NANO[] = {
    HL(15, 0, "D13"), HL(15, 1, "3V3"), HL(15, 2, "AREF"), HL(15, 3, "A0"), HL(15, 4, "A1"), HL(15, 5, "A2"), HL(15, 6, "A3"), HL(15, 7, "A4"),
    HL(15, 8, "A5"), HL(15, 9, "A6"), HL(15, 10, "A7"), HL(15, 11, "5V"), HL(15, 12, "RST"), HL(15, 13, "GND"), HL(15, 14, "VIN"),
    HR(15, 0, "D12"), HR(15, 1, "D11"), HR(15, 2, "D10"), HR(15, 3, "D9"), HR(15, 4, "D8"), HR(15, 5, "D7"), HR(15, 6, "D6"), HR(15, 7, "D5"),
    HR(15, 8, "D4"), HR(15, 9, "D3"), HR(15, 10, "D2"), HR(15, 11, "GND"), HR(15, 12, "RST"), HR(15, 13, "D0"), HR(15, 14, "D1") };
RDE_INTERNAL const fude_zoom_pin FZC_PICO[] = {
    HL(20, 0, "GP0"), HL(20, 1, "GP1"), HL(20, 2, "GND"), HL(20, 3, "GP2"), HL(20, 4, "GP3"), HL(20, 5, "GP4"), HL(20, 6, "GP5"), HL(20, 7, "GND"),
    HL(20, 8, "GP6"), HL(20, 9, "GP7"), HL(20, 10, "GP8"), HL(20, 11, "GP9"), HL(20, 12, "GND"), HL(20, 13, "GP10"), HL(20, 14, "GP11"),
    HL(20, 15, "GP12"), HL(20, 16, "GP13"), HL(20, 17, "GND"), HL(20, 18, "GP14"), HL(20, 19, "GP15"),
    HR(20, 0, "VBUS"), HR(20, 1, "VSYS"), HR(20, 2, "GND"), HR(20, 3, "3V3_EN"), HR(20, 4, "3V3"), HR(20, 5, "ADC_VREF"), HR(20, 6, "GP28"),
    HR(20, 7, "AGND"), HR(20, 8, "GP27"), HR(20, 9, "GP26"), HR(20, 10, "RUN"), HR(20, 11, "GP22"), HR(20, 12, "GND"), HR(20, 13, "GP21"),
    HR(20, 14, "GP20"), HR(20, 15, "GP19"), HR(20, 16, "GP18"), HR(20, 17, "GND"), HR(20, 18, "GP17"), HR(20, 19, "GP16") };
RDE_INTERNAL const fude_zoom_pin FZC_PI40[] = {
    HL(20, 0, "3V3"), HL(20, 1, "GPIO2"), HL(20, 2, "GPIO3"), HL(20, 3, "GPIO4"), HL(20, 4, "GND"), HL(20, 5, "GPIO17"), HL(20, 6, "GPIO27"),
    HL(20, 7, "GPIO22"), HL(20, 8, "3V3"), HL(20, 9, "GPIO10"), HL(20, 10, "GPIO9"), HL(20, 11, "GPIO11"), HL(20, 12, "GND"), HL(20, 13, "ID_SD"),
    HL(20, 14, "GPIO5"), HL(20, 15, "GPIO6"), HL(20, 16, "GPIO13"), HL(20, 17, "GPIO19"), HL(20, 18, "GPIO26"), HL(20, 19, "GND"),
    HR(20, 0, "5V"), HR(20, 1, "5V"), HR(20, 2, "GND"), HR(20, 3, "GPIO14"), HR(20, 4, "GPIO15"), HR(20, 5, "GPIO18"), HR(20, 6, "GND"),
    HR(20, 7, "GPIO23"), HR(20, 8, "GPIO24"), HR(20, 9, "GND"), HR(20, 10, "GPIO25"), HR(20, 11, "GPIO8"), HR(20, 12, "GPIO7"), HR(20, 13, "ID_SC"),
    HR(20, 14, "GND"), HR(20, 15, "GPIO12"), HR(20, 16, "GND"), HR(20, 17, "GPIO16"), HR(20, 18, "GPIO20"), HR(20, 19, "GPIO21") };
RDE_INTERNAL const fude_zoom_pin FZC_ESP32[] = {
    HL(15, 0, "EN"), HL(15, 1, "VP"), HL(15, 2, "VN"), HL(15, 3, "D34"), HL(15, 4, "D35"), HL(15, 5, "D32"), HL(15, 6, "D33"), HL(15, 7, "D25"),
    HL(15, 8, "D26"), HL(15, 9, "D27"), HL(15, 10, "D14"), HL(15, 11, "D12"), HL(15, 12, "D13"), HL(15, 13, "GND"), HL(15, 14, "VIN"),
    HR(15, 0, "D23"), HR(15, 1, "D22"), HR(15, 2, "TX0"), HR(15, 3, "RX0"), HR(15, 4, "D21"), HR(15, 5, "D19"), HR(15, 6, "D18"), HR(15, 7, "D5"),
    HR(15, 8, "TX2"), HR(15, 9, "RX2"), HR(15, 10, "D4"), HR(15, 11, "D2"), HR(15, 12, "D15"), HR(15, 13, "GND"), HR(15, 14, "3V3") };
RDE_INTERNAL const fude_zoom_pin FZC_8266[] = {
    HL(15, 0, "A0"), HL(15, 1, "RSV"), HL(15, 2, "RSV"), HL(15, 3, "SD3"), HL(15, 4, "SD2"), HL(15, 5, "SD1"), HL(15, 6, "CMD"), HL(15, 7, "SD0"),
    HL(15, 8, "CLK"), HL(15, 9, "GND"), HL(15, 10, "3V3"), HL(15, 11, "EN"), HL(15, 12, "RST"), HL(15, 13, "GND"), HL(15, 14, "VIN"),
    HR(15, 0, "D0"), HR(15, 1, "D1"), HR(15, 2, "D2"), HR(15, 3, "D3"), HR(15, 4, "D4"), HR(15, 5, "3V3"), HR(15, 6, "GND"), HR(15, 7, "D5"),
    HR(15, 8, "D6"), HR(15, 9, "D7"), HR(15, 10, "D8"), HR(15, 11, "RX"), HR(15, 12, "TX"), HR(15, 13, "GND"), HR(15, 14, "3V3") };

// A pin table and its length, as fude_zoom_part takes them.
#define FZN(_a) _a, (u16)(sizeof(_a) / sizeof(_a[0]))
// A part: its symbol's id, its model, how it is drawn, its gate (or a flip-flop's kind), its pins, its text as it
// comes, and its model's own two numbers (a regulator's volts and dropout, a board's supply volts).
#define FZ(_id, _model, _look, _gate, _pins, _value, _a, _b) { _id, FUDE_ZOOM_MODEL_##_model, FZC_##_look, _gate, FZN(_pins), _value, _a, _b }
// A definition of the simulation's (logic.h): its pins its ports, made from it when it is first wanted.
#define FZ_SIM(_id, _look, _value) { _id, FUDE_ZOOM_MODEL_SIM, FZC_##_look, 0, NULL, 0, _value, 0, 0 }
// A sized display (display.h): its pins as its text says how big it is, made from it when it is first wanted.
#define FZ_SIZED(_id, _model, _look, _value) { _id, FUDE_ZOOM_MODEL_##_model, FZC_##_look, 0, NULL, 0, _value, 0, 0 }

// The catalogue: every part there is. (The order is the symbols' ids', not their kinds': each found by its id.)
RDE_INTERNAL const fude_zoom_part FZC_PARTS[] = {
    FZ("resistor",          RESISTOR,  RESISTOR,  0, FZC_TWO,      "1k",       0, 0),
    FZ("potentiometer",     POT,       POT,       0, FZC_POT_P,    "10k 50%",  0, 0),
    FZ("capacitor",         CAPACITOR, CAP,       0, FZC_TWO,      "100nF",    0, 0),
    FZ("electrolytic",      CAPACITOR, CAP_POL,   0, FZC_TWO,      "100uF",    0, 0),
    FZ("inductor",          INDUCTOR,  INDUCTOR,  0, FZC_TWO,      "10mH",     0, 0),
    FZ("diode",             DIODE,     DIODE,     0, FZC_DIODE_P,  "1N4148",   0, 0),
    FZ("LED",               LED,       LED,       0, FZC_DIODE_P,  "red",      0, 0),
    FZ("zener",             ZENER,     ZENER,     0, FZC_DIODE_P,  "5.1V",     0, 0),
    FZ("NPN",               NPN,       NPN,       0, FZC_BJT_N,    "2N2222",   0, 0),
    FZ("PNP",               PNP,       PNP,       0, FZC_BJT_P,    "2N2907",   0, 0),
    FZ("N-MOSFET",          NMOS,      NMOS,      0, FZC_FET_N,    "IRLZ44N",  0, 0),
    FZ("P-MOSFET",          PMOS,      PMOS,      0, FZC_FET_P,    "IRF9540",  0, 0),
    FZ("DC source",         VSOURCE,   VSOURCE,   0, FZC_SUPPLY,   "5V",       0, 0),
    FZ("battery",           BATTERY,   BATTERY,   0, FZC_SUPPLY,   "9V",       0, 0),
    FZ("AC source",         ACSOURCE,  ACSOURCE,  0, FZC_SUPPLY,   "5V 50Hz",  0, 0),
    FZ("clock",             CLOCK,     CLOCK,     0, FZC_SUPPLY,   "1Hz",      0, 0),
    FZ("current source",    ISOURCE,   ISOURCE,   0, FZC_SUPPLY,   "10mA",     0, 0),
    FZ("ground",            GROUND,    GROUND,    0, FZC_GROUND_P, "",         0, 0),
    FZ("supply rail",       RAIL,      RAIL,      0, FZC_RAIL_P,   "5V",       0, 0),
    FZ("SPST switch",       SWITCH,    SWITCH,    0, FZC_TWO,      "off",      0, 0),
    FZ("push button",       BUTTON,    BUTTON,    0, FZC_TWO,      "",         0, 0),
    FZ("lamp",              LAMP,      LAMP,      0, FZC_TWO,      "12V 5W",   0, 0),
    FZ("motor",             MOTOR,     MOTOR,     0, FZC_TWO,      "6V",       0, 0),
    FZ("buzzer",            BUZZER,    BUZZER,    0, FZC_BUZZER_P, "5V",       0, 0),
    FZ("fuse",              FUSE,      FUSE,      0, FZC_TWO,      "1A",       0, 0),
    FZ("op-amp",            OPAMP,     OPAMP,     0, FZC_OPAMP_P,  "",         0, 0),
    FZ("voltmeter",         VOLTMETER, VOLTMETER, 0, FZC_TWO,      "",         0, 0),
    FZ("ammeter",           AMMETER,   AMMETER,   0, FZC_TWO,      "",         0, 0),
    FZ("7-segment",         SEVEN_SEG, SEVEN_SEG, 0, FZC_SEG_P,    "",         0, 0),
    FZ("AND gate",          GATE,      GATE, FUDE_ZOOM_GATE_AND,    FZC_GATE2, "", 0, 0),
    FZ("OR gate",           GATE,      GATE, FUDE_ZOOM_GATE_OR,     FZC_GATE2, "", 0, 0),
    FZ("NOT gate",          GATE,      GATE, FUDE_ZOOM_GATE_NOT,    FZC_GATE1, "", 0, 0),
    FZ("NAND gate",         GATE,      GATE, FUDE_ZOOM_GATE_NAND,   FZC_GATE2, "", 0, 0),
    FZ("NOR gate",          GATE,      GATE, FUDE_ZOOM_GATE_NOR,    FZC_GATE2, "", 0, 0),
    FZ("XOR gate",          GATE,      GATE, FUDE_ZOOM_GATE_XOR,    FZC_GATE2, "", 0, 0),
    FZ("XNOR gate",         GATE,      GATE, FUDE_ZOOM_GATE_XNOR,   FZC_GATE2, "", 0, 0),
    FZ("buffer",            GATE,      GATE, FUDE_ZOOM_GATE_BUFFER, FZC_GATE1, "", 0, 0),
    FZ("D flip-flop",       DFF,       FLIPFLOP,  0, FZC_DFF_P,    "",         0, 0),
    FZ("T flip-flop",       TFF,       FLIPFLOP,  1, FZC_TFF_P,    "",         0, 0),
    FZ("logic input",       LOGIC_IN,  LOGIC_IN,  0, FZC_OUT1,     "0",        0, 0),
    FZ("logic probe",       LOGIC_OUT, LOGIC_OUT, 0, FZC_IN1,      "",         0, 0),
    FZ("NE555",             TIMER555,  CHIP,      0, FZC_555,      "NE555",    0, 0),
    FZ("LM358",             OPAMP2,    CHIP,      0, FZC_LM358,    "LM358",    0, 0),
    FZ("ATmega328P",        NONE,      CHIP,      0, FZC_328P,     "ATmega328P", 0, 0),
    FZ("74HC595",           SHIFT595,  CHIP,      0, FZC_595,      "74HC595",  0, 0),
    FZ("L293D",             DRIVER293, CHIP,      0, FZC_293,      "L293D",    0, 0),
    FZ("ULN2003",           ULN2003,   CHIP,      0, FZC_2003,     "ULN2003",  0, 0),
    FZ_SIM("74HC00",        CHIP, "74HC00"),
    FZ_SIM("74HC08",        CHIP, "74HC08"),
    FZ_SIM("74HC32",        CHIP, "74HC32"),
    FZ_SIM("74HC86",        CHIP, "74HC86"),
    FZ_SIM("74HC04",        CHIP, "74HC04"),
    FZ_SIM("74HC02",        CHIP, "74HC02"),
    FZ_SIM("74HC74",        CHIP, "74HC74"),
    FZ_SIM("74HC138",       CHIP, "74HC138"),
    FZ_SIM("74HC157",       CHIP, "74HC157"),
    FZ_SIM("74HC161",       CHIP, "74HC161"),
    FZ_SIM("74HC173",       CHIP, "74HC173"),
    FZ_SIM("74HC245",       CHIP, "74HC245"),
    FZ_SIM("74HC283",       CHIP, "74HC283"),
    FZ_SIM("74HC189",       CHIP, "74HC189"),
    FZ_SIM("custom part",   BLOCK, ""),
    FZ("MAX7219",           NONE,      CHIP,      0, FZC_7219,     "MAX7219",  0, 0),
    FZ("PCF8574",           NONE,      CHIP,      0, FZC_8574,     "PCF8574",  0, 0),
    FZ("7805",              REGULATOR, REG3,      0, FZC_REG_P,    "7805",     5.0f, 2.0f),
    FZ("AMS1117",           REGULATOR, REG3,      0, FZC_REG_P,    "AMS1117-3.3", 3.3f, 1.1f),
    FZ("DS3231",            NONE,      MODULE,    0, FZC_DS3231,   "DS3231 RTC", 0, 0),
    FZ("MPU6050",           NONE,      MODULE,    0, FZC_MPU,      "MPU6050",  0, 0),
    FZ("nRF24L01",          NONE,      MODULE,    0, FZC_NRF,      "nRF24L01", 0, 0),
    FZ("HC-05",             NONE,      MODULE,    0, FZC_HC05,     "HC-05",    0, 0),
    FZ("HC-SR04",           NONE,      MODULE,    0, FZC_SR04,     "HC-SR04",  0, 0),
    FZ("DHT11",             NONE,      MODULE,    0, FZC_DHT,      "DHT11",    0, 0),
    FZ("servo",             SERVO,     MODULE,    0, FZC_SERVO,    "SG90 servo", 0, 0),
    FZ("TP4056",            NONE,      MODULE,    0, FZC_TP4056,   "TP4056",   0, 0),
    FZ("WS2812B",           NONE,      MODULE,    0, FZC_WS2812,   "WS2812B",  0, 0),
    FZ("L298N",             NONE,      MODULE,    0, FZC_L298N,    "L298N",    0, 0),
    FZ("relay module",      RELAY,     MODULE,    0, FZC_RELAY,    "Relay",    0, 0),
    FZ("I2C LCD",           NONE,      MODULE,    0, FZC_I2C4,     "LCD 16x2 I2C", 0, 0),
    FZ("OLED",              NONE,      MODULE,    0, FZC_I2C4,     "OLED SSD1306", 0, 0),
    FZ("Arduino Uno",       BOARD,     BOARD,     0, FZC_UNO,      "Arduino Uno",   5.0f, 0),
    FZ("Arduino Nano",      BOARD,     BOARD,     0, FZC_NANO,     "Arduino Nano",  5.0f, 0),
    FZ("Raspberry Pi Pico", BOARD,     BOARD,     0, FZC_PICO,     "Raspberry Pi Pico", 3.3f, 0),
    FZ("Raspberry Pi GPIO", BOARD,     BOARD,     0, FZC_PI40,     "Raspberry Pi GPIO", 3.3f, 0),
    FZ("ESP32",             BOARD,     BOARD,     0, FZC_ESP32,    "ESP32 DevKit",  3.3f, 0),
    FZ("ESP8266",           BOARD,     BOARD,     0, FZC_8266,     "NodeMCU ESP8266", 3.3f, 0),
    FZ_SIZED("7-segment panel", SEG_PANEL, SEG_PANEL, "4 digits"),
    FZ_SIZED("LED matrix",  LED_MATRIX, MATRIX,  "8x8"),
    FZ("LED bar graph",     BAR_GRAPH, BAR_GRAPH, 0, FZC_BAR_P, "red",   0, 0),
    FZ("LM3914",            LM3914,    CHIP,      0, FZC_3914,  "LM3914", 0, 0),
    FZ_SIM("CD4511",        CHIP, "CD4511"),
    FZ("panel meter",       PANEL_METER, METER,   0, FZC_METER_P, "20V", 0, 0),
    FZ_SIZED("character LCD", CHAR_LCD, CHAR_LCD, "16x2"),
    FZ("oscilloscope",      SCOPE,     SCOPE,     0, FZC_SCOPE_P, "1ms 2V", 0, 0),
    FZ("SPDT switch",       SPDT,      SPDT,      0, FZC_SPDT_P, "A",      0, 0),
    FZ("DPDT switch",       DPDT,      DPDT,      0, FZC_DPDT_P, "A",      0, 0),
    FZ("LDR",               LDR,       LDR,       0, FZC_TWO,    "GL5528", 0, 0),
    FZ("thermistor",        THERMISTOR, NTC,      0, FZC_TWO,    "10k NTC", 0, 0),
    FZ("solenoid",          SOLENOID,  SOLENOID,  0, FZC_SOLENOID_P, "12V",     0, 0),
    FZ("stepper motor",     STEPPER,   STEPPER,   0, FZC_STEPPER_P,  "28BYJ-48", 0, 0),
    FZ("rotary encoder",    ENCODER,   ENCODER,   0, FZC_ENCODER_P,  "20",      0, 0),
    FZ("slotted sensor",    SLOT,      SLOT,      0, FZC_SLOT_P,     "ITR9608", 0, 0),
    FZ("JK flip-flop",      JKFF,      FLIPFLOP,  0, FZC_JK_P,       "",        0, 0),
    FZ("SR latch",          SRLATCH,   FLIPFLOP,  0, FZC_SR_P,       "",        0, 0),
    FZ("LM393",             COMPARATOR, CHIP,     0, FZC_LM358,      "LM393",   0, 0),
    FZ("SCR",               SCR,       SCR,       0, FZC_SCR_P,      "C106",    0, 0),
    FZ("TRIAC",             TRIAC,     TRIAC,     0, FZC_TRIAC_P,    "BT136",   0, 0),
    FZ("transformer",       TRANSFORMER, XFMR,    0, FZC_XFMR_P,     "2:1",     0, 0),
    FZ("bridge rectifier",  BRIDGE,    BRIDGE,    0, FZC_BRIDGE_P,   "DB107",   0, 0),
    FZ("optocoupler",       OPTO,      OPTO,      0, FZC_SLOT_P,     "PC817",   0, 0),
    FZ("photodiode",        PHOTODIODE, PHOTODIODE, 0, FZC_DIODE_P,  "BPW34",   0, 0),
    FZ("phototransistor",   PHOTOTRANSISTOR, PHOTOTRANS, 0, FZC_PHOTOT_P, "TEPT5600", 0, 0),
    FZ("speaker",           SPEAKER,   SPEAKER,   0, FZC_SPEAKER_P,  "8 ohm",   0, 0),
    FZ_SIM("CD4017",        CHIP, "CD4017"),
    { "breadboard", FUDE_ZOOM_MODEL_NONE, FZC_BREADBOARD, 0, NULL, (u16)(FUDE_ZOOM_BREADBOARD_COLS * FUDE_ZOOM_BREADBOARD_ROWS), "", 0, 0 },
};
#define FZC_PART_N ((u32)(sizeof(FZC_PARTS) / sizeof(FZC_PARTS[0])))   // How many parts the catalogue has.

// See the header: a linear search of the catalogue by id. A chip of the simulation's and a sized display are made (and
// kept) the first time they are wanted.
const fude_zoom_part* fude_zoom_part_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < FZC_PART_N; _i++) {
        if(rde_strcmp(FZC_PARTS[_i].id, _id) == 0) {
            // A chip of the simulation's: its pins its definition's ports, round its package.
            const fude_zoom_part* _made = FZC_PARTS[_i].model == FUDE_ZOOM_MODEL_SIM && FZC_PARTS[_i].look == FZC_CHIP ?
                                          fude_zoom_logic_part(&FZC_PARTS[_i], fude_zoom_logic_find(_id), true) : NULL;
            // A sized display: as its kind comes ("4 digits", "8x8", "16x2").
            return _made != NULL ? _made : fude_zoom_display_part(&FZC_PARTS[_i], FZC_PARTS[_i].value);
        }
    }
    return NULL;
}

// See the header: the topics' one kind-to-part map (symbol.h: fude_zoom_symbol_topic_part), which finds each kind's
// part once by its id (fude_zoom_part_find) and keeps it.
const fude_zoom_part* fude_zoom_part_of_kind(u32 _kind) {
    return (const fude_zoom_part*)fude_zoom_symbol_topic_part(_kind, FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT);
}

// See the header.
u32 fude_zoom_part_count(void) {
    return FZC_PART_N;
}

// See the header.
const fude_zoom_part* fude_zoom_part_at(u32 _i) {
    return _i < FZC_PART_N ? &FZC_PARTS[_i] : NULL;
}

// See the header: a value for each look whose body is inset from its pins; 0 for the rest.
f32 fude_zoom_part_inset(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->look : 255u) {
    case FZC_CHIP:      return 0.6f;
    case FZC_MODULE:    return 0.65f;
    case FZC_BOARD:     return 0.8f;
    case FZC_REG3:      return 0.55f;
    case FZC_FLIPFLOP:  return 0.6f;
    case FZC_SEVEN_SEG: return 0.7f;
    case FZC_BAR_GRAPH: return 5.0f / 7.0f;
    case FZC_SCOPE:     return 0.85f;
    case FZC_STEPPER:   return 0.8f;
    case FZC_ENCODER:   return 0.7f;
    case FZC_SLOT:
    case FZC_OPTO:      return 0.8f;
    case FZC_SEG_PANEL:
    case FZC_MATRIX:
    case FZC_CHAR_LCD:  return 0.9f;   // As big as each is: fude_zoom_part_insets.
    default:            return 0.0f;
    }
}

// See the header. Only an oscilloscope has labels: its channels' names over their knobs, and each knob's name under it.
u32 fude_zoom_part_labels(const fude_zoom_part* _part, fude_zoom_part_label* _out, u32 _most) {
    if(_part == NULL || _part->model != FUDE_ZOOM_MODEL_SCOPE) {
        return 0u;
    }
    // An oscilloscope's: its channels over their columns, each knob's name under it.
    static const c8* const _under[FUDE_ZOOM_SCOPE_KNOBS] = { "V/DIV", "V/DIV", "POS", "POS", "TIME/DIV" };
    u32 _n = 0;
    f64 _u, _v, _r;
    for(u32 _ch = 0; _ch < 2u && _n < _most; _ch++) {
        fude_zoom_scope_knob(_ch == 0u ? FUDE_ZOOM_SCOPE_KNOB_VOLTS1 : FUDE_ZOOM_SCOPE_KNOB_VOLTS2, &_u, &_v, &_r);
        _out[_n++] = (fude_zoom_part_label){ (f32)_u, 0.86f, _ch == 0u ? "CH1" : "CH2" };
    }
    for(u32 _k = 0; _k < FUDE_ZOOM_SCOPE_KNOBS && _n < _most; _k++) {
        fude_zoom_scope_knob(_k, &_u, &_v, &_r);
        _out[_n++] = (fude_zoom_part_label){ (f32)_u, (f32)(_v - _r - 0.11), _under[_k] };
    }
    return _n;
}

// See the header: fude_zoom_part_inset across, 0.8 up and down, then a sized display's own (display.h).
void fude_zoom_part_insets(const fude_zoom_part* _part, f32* _u, f32* _v) {
    *_u = fude_zoom_part_inset(_part);
    *_v = 0.8f;
    fude_zoom_display_label_inset(_part, _u, _v);
}

// See the header: a sized display's height as its text makes it, else the catalogue's.
f64 fude_zoom_part_room_h(const fude_zoom_part* _part, f64 _catalogue_h) {
    f64 _w, _h;
    return fude_zoom_display_room(_part, &_w, &_h) ? _h : _catalogue_h;
}

// --- a breadboard ---------------------------------------------------------------------------------

// Its rows down from its top (the rails' rows, a–e, f–j, the rails') at these heights (holes), its columns 1 hole
// apart from 1.5 in; 18.5 holes high (its own v from -1 to 1), 33 wide.
RDE_INTERNAL const f32 FZC_BB_ROW[FUDE_ZOOM_BREADBOARD_ROWS] = { 1.0f, 2.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.5f, 11.5f, 12.5f, 13.5f, 14.5f, 16.5f, 17.5f };

// See the header. Rows 0, 1, 12 and 13 are the rails, each one strip along; the rest are the columns' five holes, a
// strip each, 100 on for the top half (rows 2 to 6) and 200 on for the bottom (rows 7 to 11).
u32 fude_zoom_breadboard_strip(u32 _i) {
    const u32 _row = _i / FUDE_ZOOM_BREADBOARD_COLS, _col = _i % FUDE_ZOOM_BREADBOARD_COLS;
    if(_row < 2u || _row >= 12u) {
        return _row;   // A rail: along.
    }
    return (_row < 7u ? 100u : 200u) + _col;   // A column's five.
}

// See the header. A breadboard's holes are worked out from their row and column, every one facing any way.
b8 fude_zoom_part_pin(const fude_zoom_part* _part, u32 _i, fude_zoom_pin* _out) {
    if(_part == NULL || _i >= _part->pin_count) {
        return false;
    }
    if(_part->look != FZC_BREADBOARD) {
        *_out = _part->pins[_i];
        return true;
    }
    static const c8* const _rails[4] = { "+", "-", "+", "-" };
    static const c8* const _letters[10] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j" };
    const u32 _row = _i / FUDE_ZOOM_BREADBOARD_COLS, _col = _i % FUDE_ZOOM_BREADBOARD_COLS;
    const f32 _x = 1.5f + (f32)_col, _y = FZC_BB_ROW[_row];
    _out->u    = -1.0f + 2.0f * _x / FZC_BB_W;
    _out->v    = 1.0f - 2.0f * _y / FZC_BB_H;
    _out->name = _row < 2u ? _rails[_row] : (_row >= 12u ? _rails[_row - 10u] : _letters[_row - 2u]);
    _out->side = FUDE_ZOOM_PIN_ANY;
    return true;
}

// --- pins where they are ---------------------------------------------------------------------------

// See the header: NULL for an object that is not a living SYMBOL shape with a part's kind.
const fude_zoom_part* fude_zoom_part_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return NULL;
    }
    f64 _n[3];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, 3u);
    // (a kind no u32 holds, a damaged file's, is none)
    const u32 _kind  = _count >= 3u ? fude_zoom_symbol_kind_of(_n, _count) : FUDE_ZOOM_NONE;
    if(_kind == FUDE_ZOOM_NONE) {
        return NULL;
    }
    const fude_zoom_part* _part = fude_zoom_part_of_kind(_kind);
    if(fude_zoom_part_custom(_part) || fude_zoom_display_made(_part)) {
        // A custom part (the definition its text names, its pins round its block), a sized display (as big as its text
        // says).
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fzc_text_of(_s, _object, _text, sizeof(_text));
        return fude_zoom_part_of_text(_kind, _text);
    }
    return _part;
}

// See the header: the catalogue's "custom part" block, or one made from it.
b8 fude_zoom_part_custom(const fude_zoom_part* _part) {
    return _part != NULL && _part->model == FUDE_ZOOM_MODEL_SIM && _part->look == FZC_BLOCK;
}

// See the header: a custom part with pins (one made from a definition), or a sized display made from a text.
b8 fude_zoom_part_made(const fude_zoom_part* _part) {
    return (fude_zoom_part_custom(_part) && _part->pin_count > 0u) || fude_zoom_display_made(_part);
}

// See the header. A custom part whose text names no definition is the catalogue's block itself.
const fude_zoom_part* fude_zoom_part_of_text(u32 _kind, const c8* _text) {
    const fude_zoom_part* _part = fude_zoom_part_of_kind(_kind);
    if(fude_zoom_part_custom(_part)) {
        const fude_zoom_part* _made = fude_zoom_logic_part(_part, fude_zoom_logic_find(_text != NULL ? _text : ""), false);
        return _made != NULL ? _made : _part;
    }
    return fude_zoom_display_part(_part, _text);   // A sized display as big as it says; any other itself.
}

// See the header: as fude_zoom_part_of, from a SYMBOL's numbers rather than its object.
const fude_zoom_part* fude_zoom_part_of_numbers(const f64* _n, u32 _count) {
    // (a kind no u32 holds, a damaged file's, is none)
    const u32 _kind = _count >= 3u ? fude_zoom_symbol_kind_of(_n, _count) : FUDE_ZOOM_NONE;
    if(_kind == FUDE_ZOOM_NONE) {
        return NULL;
    }
    const fude_zoom_part* _part = fude_zoom_part_of_kind(_kind);
    if((fude_zoom_part_custom(_part) || fude_zoom_display_made(_part)) && _count > 4u) {
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
        return fude_zoom_part_of_text(_kind, _text);
    }
    return _part;
}

// See the header: the pin's u, v times the symbol's half sizes, through the object's own place, in its frame's units.
b8 fude_zoom_part_pin_at(const fude_zoom_scene* _s, u32 _object, u32 _pin, fude_zoom_v2* _out) {
    const fude_zoom_part* _p = fude_zoom_part_of(_s, _object);
    fude_zoom_pin _q;
    f64 _n[3];
    if(_p == NULL || !fude_zoom_part_pin(_p, _pin, &_q) || fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) < 3u) {
        return false;
    }
    *_out = fude_zoom_sim_apply(fude_zoom_object_sim(fude_zoom_scene_object(_s, _object)), (fude_zoom_v2){ (f64)_q.u * _n[1], (f64)_q.v * _n[2] });
    return true;
}

// See the header: ten of the catalogue's units at the symbol's size, from its half height against its room's.
f64 fude_zoom_part_unit(const fude_zoom_scene* _s, u32 _object) {
    f64 _n[3];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) < 3u) {
        return 0.0;
    }
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(fude_zoom_symbol_kind_of(_n, 3u));
    const f64 _room = _info != NULL ? fude_zoom_part_room_h(fude_zoom_part_of(_s, _object), (f64)_info->h) : 0.0;
    return _room > 0.0 ? _n[2] * fude_zoom_scene_object(_s, _object)->scale / (_room * 0.5) * 10.0 : 0.0;
}
