// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/logic.h"
#include "zoom/props.h"
#include "zoom/limits.h"
#include "zoom/display.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FZC_PI 3.14159265358979323846

// --- the catalogue ------------------------------------------------------------------------------------

// How a part is drawn.
typedef enum {
    FZC_RESISTOR = 0, FZC_POT, FZC_CAP, FZC_CAP_POL, FZC_INDUCTOR, FZC_DIODE, FZC_LED, FZC_ZENER,
    FZC_NPN, FZC_PNP, FZC_NMOS, FZC_PMOS, FZC_VSOURCE, FZC_BATTERY, FZC_ACSOURCE, FZC_CLOCK, FZC_ISOURCE,
    FZC_GROUND, FZC_RAIL, FZC_SWITCH, FZC_BUTTON, FZC_LAMP, FZC_MOTOR, FZC_BUZZER, FZC_FUSE, FZC_OPAMP,
    FZC_VOLTMETER, FZC_AMMETER, FZC_SEVEN_SEG, FZC_GATE, FZC_FLIPFLOP, FZC_LOGIC_IN, FZC_LOGIC_OUT,
    FZC_CHIP, FZC_MODULE, FZC_REG3, FZC_BOARD, FZC_BREADBOARD, FZC_BLOCK,
    FZC_SEG_PANEL, FZC_MATRIX, FZC_BAR_GRAPH, FZC_METER, FZC_CHAR_LCD
} FZC_LOOK_;

#define FZP(_u, _v, _name, _side) { (f32)(_u), (f32)(_v), _name, FUDE_ZOOM_PIN_##_side }
// A DIP's pin _k down its left (from its top), or _j up its right (from its bottom), of _n.
#define DL(_n, _k, _name) { -1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) / 2 + 1), _name, FUDE_ZOOM_PIN_LEFT }
#define DR(_n, _j, _name) { 1.0f, -1.0f + 2.0f * (f32)((_j) + 1) / (f32)((_n) / 2 + 1), _name, FUDE_ZOOM_PIN_RIGHT }
// A header's pin _k (from its top) of _n down its left; a board's down its left or right, rows of _n.
#define HL(_n, _k, _name) { -1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) + 1), _name, FUDE_ZOOM_PIN_LEFT }
#define HR(_n, _k, _name) { 1.0f, 1.0f - 2.0f * (f32)((_k) + 1) / (f32)((_n) + 1), _name, FUDE_ZOOM_PIN_RIGHT }

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
RDE_INTERNAL const fude_zoom_pin FZC_METER_P[] = { FZP(-1, 0, "+", LEFT), FZP(1, 0, "-", RIGHT) };

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

#define FZN(_a) _a, (u16)(sizeof(_a) / sizeof(_a[0]))
#define FZ(_id, _model, _look, _gate, _pins, _value, _a, _b) { _id, FUDE_ZOOM_MODEL_##_model, FZC_##_look, _gate, FZN(_pins), _value, _a, _b }
// A definition of the simulation's (logic.h): its pins its ports, made from it when it is first wanted.
#define FZ_SIM(_id, _look, _value) { _id, FUDE_ZOOM_MODEL_SIM, FZC_##_look, 0, NULL, 0, _value, 0, 0 }
// A sized display (display.h): its pins as its text says how big it is, made from it when it is first wanted.
#define FZ_SIZED(_id, _model, _look, _value) { _id, FUDE_ZOOM_MODEL_##_model, FZC_##_look, 0, NULL, 0, _value, 0, 0 }

// (The order is the symbols' ids', not their kinds': each found by its id.)
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
    FZ("LM358",             NONE,      CHIP,      0, FZC_LM358,    "LM358",    0, 0),
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
    { "breadboard", FUDE_ZOOM_MODEL_NONE, FZC_BREADBOARD, 0, NULL, (u16)(FUDE_ZOOM_BREADBOARD_COLS * FUDE_ZOOM_BREADBOARD_ROWS), "", 0, 0 },
};
#define FZC_PART_N ((u32)(sizeof(FZC_PARTS) / sizeof(FZC_PARTS[0])))

const fude_zoom_part* fude_zoom_part_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < FZC_PART_N; _i++) {
        if(strcmp(FZC_PARTS[_i].id, _id) == 0) {
            // (a chip of the simulation's: its pins its definition's ports, round its package)
            const fude_zoom_part* _made = FZC_PARTS[_i].model == FUDE_ZOOM_MODEL_SIM && FZC_PARTS[_i].look == FZC_CHIP ?
                                          fude_zoom_logic_part(&FZC_PARTS[_i], fude_zoom_logic_find(_id), true) : NULL;
            // (a sized display: as its kind comes — "4 digits", "8x8", "16x2")
            return _made != NULL ? _made : fude_zoom_display_part(&FZC_PARTS[_i], FZC_PARTS[_i].value);
        }
    }
    return NULL;
}

const fude_zoom_part* fude_zoom_part_of_kind(u32 _kind) {
    // (each symbol's part found once, its kind its place: symbols are only ever appended)
    static const fude_zoom_part* _cache[1024];
    static u8 _known[1024];
    if(_kind >= 1024u) {
        const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
        return _info != NULL ? fude_zoom_part_find(_info->id) : NULL;
    }
    if(!_known[_kind]) {
        const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
        _cache[_kind] = _info != NULL ? fude_zoom_part_find(_info->id) : NULL;
        _known[_kind] = 1u;
    }
    return _cache[_kind];
}

u32 fude_zoom_part_count(void) {
    return FZC_PART_N;
}

const fude_zoom_part* fude_zoom_part_at(u32 _i) {
    return _i < FZC_PART_N ? &FZC_PARTS[_i] : NULL;
}

f32 fude_zoom_part_inset(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->look : 255u) {
    case FZC_CHIP:      return 0.6f;
    case FZC_MODULE:    return 0.65f;
    case FZC_BOARD:     return 0.8f;
    case FZC_REG3:      return 0.55f;
    case FZC_FLIPFLOP:  return 0.6f;
    case FZC_SEVEN_SEG: return 0.7f;
    case FZC_BAR_GRAPH: return 5.0f / 7.0f;
    case FZC_SEG_PANEL:
    case FZC_MATRIX:
    case FZC_CHAR_LCD:  return 0.9f;   // (as big as each is: fude_zoom_part_insets)
    default:            return 0.0f;
    }
}

void fude_zoom_part_insets(const fude_zoom_part* _part, f32* _u, f32* _v) {
    *_u = fude_zoom_part_inset(_part);
    *_v = 0.8f;
    fude_zoom_display_label_inset(_part, _u, _v);
}

f64 fude_zoom_part_room_h(const fude_zoom_part* _part, f64 _catalogue_h) {
    f64 _w, _h;
    return fude_zoom_display_room(_part, &_w, &_h) ? _h : _catalogue_h;
}

// --- a breadboard ---------------------------------------------------------------------------------

// Its rows down from its top (the rails' rows, a–e, f–j, the rails') at these heights (holes), its columns 1 hole
// apart from 1.5 in; 18.5 holes high (its own v from -1 to 1), 33 wide.
RDE_INTERNAL const f32 FZC_BB_ROW[FUDE_ZOOM_BREADBOARD_ROWS] = { 1.0f, 2.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.5f, 11.5f, 12.5f, 13.5f, 14.5f, 16.5f, 17.5f };
#define FZC_BB_H 18.5f
#define FZC_BB_W 33.0f

u32 fude_zoom_breadboard_strip(u32 _i) {
    const u32 _row = _i / FUDE_ZOOM_BREADBOARD_COLS, _col = _i % FUDE_ZOOM_BREADBOARD_COLS;
    if(_row < 2u || _row >= 12u) {
        return _row;   // (a rail: along)
    }
    return (_row < 7u ? 100u : 200u) + _col;   // (a column's five)
}

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

// --- drawn -------------------------------------------------------------------------------------------

typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    u32      segments;
    u32      first;
} fzc;

RDE_INTERNAL void fzc_pt(fzc* _d, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_d->points, (any)&_p);
}
RDE_INTERNAL void fzc_uv(fzc* _d, f64 _u, f64 _v) {
    fzc_pt(_d, _u * _d->hw, _v * _d->hh);
}
RDE_INTERNAL void fzc_begin(fzc* _d) {
    _d->first = (u32)rde_arr_length(_d->points);
}
RDE_INTERNAL void fzc_end(fzc* _d, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_d->points) - _d->first;
    if(_n >= 2u) {
        const fude_zoom_symbol_part _p = { _d->first, _n, _flags };
        rde_arr_add(_d->parts, (any)&_p);
    } else {
        _d->points->count -= _n;
    }
}
RDE_INTERNAL void fzc_line(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1) {
    fzc_begin(_d);
    fzc_uv(_d, _u0, _v0);
    fzc_uv(_d, _u1, _v1);
    fzc_end(_d, 0u);
}
RDE_INTERNAL void fzc_poly(fzc* _d, const f64* _uv, u32 _n, u8 _flags) {
    fzc_begin(_d);
    for(u32 _i = 0; _i < _n; _i++) {
        fzc_uv(_d, _uv[2u * _i], _uv[2u * _i + 1u]);
    }
    fzc_end(_d, _flags);
}
RDE_INTERNAL void fzc_rect(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _r[8] = { _u0, _v0, _u1, _v0, _u1, _v1, _u0, _v1 };
    fzc_poly(_d, _r, 4u, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// A circle round (u, v) (its centre in the box's proportions), _r its own units round.
RDE_INTERNAL void fzc_circle(fzc* _d, f64 _u, f64 _v, f64 _r, u8 _flags) {
    fzc_begin(_d);
    const u32 _k = _d->segments < 16u ? 16u : _d->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * FZC_PI * (f64)_i / (f64)_k;
        fzc_pt(_d, _u * _d->hw + cos(_a) * _r, _v * _d->hh + sin(_a) * _r);
    }
    fzc_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// An arc's points (own units), the part being made.
RDE_INTERNAL void fzc_arc_pts(fzc* _d, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1) {
    u32 _k = (u32)ceil((f64)(_d->segments < 16u ? 16u : _d->segments) * fabs(_a1 - _a0) / (2.0 * FZC_PI));
    _k = _k < 3u ? 3u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fzc_pt(_d, _cx + cos(_a) * _rx, _cy + sin(_a) * _ry);
    }
}
// A filled arrowhead at (x, y) (own units) pointing along (dx, dy).
RDE_INTERNAL void fzc_head(fzc* _d, f64 _x, f64 _y, f64 _dx, f64 _dy, f64 _len) {
    const f64 _l = hypot(_dx, _dy);
    if(!(_l > 0.0)) {
        return;
    }
    const f64 _ux = _dx / _l, _uy = _dy / _l;
    fzc_begin(_d);
    fzc_pt(_d, _x, _y);
    fzc_pt(_d, _x - _ux * _len - _uy * _len * 0.45, _y - _uy * _len + _ux * _len * 0.45);
    fzc_pt(_d, _x - _ux * _len + _uy * _len * 0.45, _y - _uy * _len - _ux * _len * 0.45);
    fzc_end(_d, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}
// A line with a head at its end (u, v).
RDE_INTERNAL void fzc_arrow(fzc* _d, f64 _u0, f64 _v0, f64 _u1, f64 _v1, f64 _len) {
    fzc_line(_d, _u0, _v0, _u1, _v1);
    fzc_head(_d, _u1 * _d->hw, _v1 * _d->hh, (_u1 - _u0) * _d->hw, (_v1 - _v0) * _d->hh, _len);
}
// A part's pins' leads: from each pin in to (_to_u of the way, a left or right one's; _to_v, a top or bottom one's).
RDE_INTERNAL void fzc_stubs_to(fzc* _d, const fude_zoom_part* _p, f64 _to_u, f64 _to_v) {
    for(u32 _i = 0; _i < _p->pin_count; _i++) {
        const fude_zoom_pin _q = _p->pins[_i];
        if(_q.side == FUDE_ZOOM_PIN_LEFT || _q.side == FUDE_ZOOM_PIN_RIGHT) {
            fzc_line(_d, _q.u, _q.v, _q.u < 0.0f ? -_to_u : _to_u, _q.v);
        } else {
            fzc_line(_d, _q.u, _q.v, _q.u, _q.v > 0.0f ? _to_v : -_to_v);
        }
    }
}
// ...both the same way in.
RDE_INTERNAL void fzc_stubs(fzc* _d, const fude_zoom_part* _p, f64 _to) {
    fzc_stubs_to(_d, _p, _to, _to);
}
// The letters of a few marks, as lines (u, v round (cu, cv), _s high): + − V A M.
RDE_INTERNAL void fzc_mark(fzc* _d, c8 _c, f64 _cu, f64 _cv, f64 _s) {
    const f64 _x = _cu * _d->hw, _y = _cv * _d->hh, _h = _s * 0.5;
    if(_c == '+' || _c == '-') {
        fzc_begin(_d); fzc_pt(_d, _x - _h, _y); fzc_pt(_d, _x + _h, _y); fzc_end(_d, 0u);
        if(_c == '+') {
            fzc_begin(_d); fzc_pt(_d, _x, _y - _h); fzc_pt(_d, _x, _y + _h); fzc_end(_d, 0u);
        }
    } else if(_c == 'V') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y + _h); fzc_pt(_d, _x, _y - _h); fzc_pt(_d, _x + _h * 0.8, _y + _h); fzc_end(_d, 0u);
    } else if(_c == 'A') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y - _h); fzc_pt(_d, _x, _y + _h); fzc_pt(_d, _x + _h * 0.8, _y - _h); fzc_end(_d, 0u);
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.45, _y - _h * 0.15); fzc_pt(_d, _x + _h * 0.45, _y - _h * 0.15); fzc_end(_d, 0u);
    } else if(_c == 'M') {
        fzc_begin(_d); fzc_pt(_d, _x - _h * 0.8, _y - _h); fzc_pt(_d, _x - _h * 0.8, _y + _h); fzc_pt(_d, _x, _y); fzc_pt(_d, _x + _h * 0.8, _y + _h);
        fzc_pt(_d, _x + _h * 0.8, _y - _h); fzc_end(_d, 0u);
    }
}

// A diode's triangle and bar (pointing right, its middle at u 0, ±_h high), solid.
RDE_INTERNAL void fzc_diode_body(fzc* _d, f64 _w, f64 _h) {
    const f64 _t[6] = { -_w, -_h, -_w, _h, _w, 0.0 };
    fzc_poly(_d, _t, 3u, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
    fzc_line(_d, _w, -_h, _w, _h);
    fzc_line(_d, -1.0, 0.0, -_w, 0.0);
    fzc_line(_d, _w, 0.0, 1.0, 0.0);
}

// A logic gate's body (inputs left, output right), its bubble when it inverts.
RDE_INTERNAL void fzc_gate(fzc* _d, u8 _gate) {
    const f64 _hw = _d->hw, _hh = _d->hh;
    const b8 _inv = _gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_NAND || _gate == FUDE_ZOOM_GATE_NOR || _gate == FUDE_ZOOM_GATE_XNOR;
    const f64 _bubble = 0.09 * _hw;
    const f64 _out = 0.62 * _hw;   // where the body ends (its tip)
    fzc_begin(_d);
    if(_gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_BUFFER) {
        fzc_pt(_d, -0.55 * _hw, -0.8 * _hh); fzc_pt(_d, -0.55 * _hw, 0.8 * _hh); fzc_pt(_d, _out - (_inv ? 2.0 * _bubble : 0.0), 0.0);
    } else if(_gate == FUDE_ZOOM_GATE_AND || _gate == FUDE_ZOOM_GATE_NAND) {
        const f64 _x0 = -0.55 * _hw, _r = 0.85 * _hh, _cx = _out - (_inv ? 2.0 * _bubble : 0.0) - _r;
        fzc_pt(_d, _x0, -_r); fzc_pt(_d, _x0, _r);
        fzc_arc_pts(_d, _cx, 0.0, _r, _r, FZC_PI * 0.5, -FZC_PI * 0.5);
    } else {
        // OR, NOR, XOR, XNOR: a curved back, pointed nose.
        const f64 _x0 = -0.55 * _hw, _tip = _out - (_inv ? 2.0 * _bubble : 0.0), _h = 0.85 * _hh;
        const u32 _k = 16u;
        fzc_pt(_d, _x0, -_h);
        for(u32 _i = 0; _i <= _k; _i++) {   // lower curve to the tip
            const f64 _t = (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + (_tip - _x0) * _t, -_h * (1.0 - _t * _t));
        }
        for(u32 _i = 1; _i <= _k; _i++) {   // upper curve back
            const f64 _t = 1.0 - (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + (_tip - _x0) * _t, _h * (1.0 - _t * _t));
        }
        for(u32 _i = 1; _i < _k; _i++) {   // the back, curving in
            const f64 _t = (f64)_i / (f64)_k;
            fzc_pt(_d, _x0 + 0.18 * _hw * sin(_t * FZC_PI), _h - 2.0 * _h * _t);
        }
    }
    fzc_end(_d, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_FILLED);
    if(_gate == FUDE_ZOOM_GATE_XOR || _gate == FUDE_ZOOM_GATE_XNOR) {
        fzc_begin(_d);
        for(u32 _i = 0; _i <= 16u; _i++) {
            const f64 _t = (f64)_i / 16.0;
            fzc_pt(_d, -0.7 * _hw + 0.18 * _hw * sin(_t * FZC_PI), 0.85 * _hh - 1.7 * _hh * _t);
        }
        fzc_end(_d, 0u);
    }
    if(_inv) {
        fzc_circle(_d, (_out - _bubble) / _hw, 0.0, _bubble, 0u);
    }
    // Leads.
    const b8 _one = _gate == FUDE_ZOOM_GATE_NOT || _gate == FUDE_ZOOM_GATE_BUFFER;
    if(_one) {
        fzc_line(_d, -1.0, 0.0, -0.55, 0.0);
    } else {
        const f64 _in = _gate == FUDE_ZOOM_GATE_AND || _gate == FUDE_ZOOM_GATE_NAND ? -0.55 : -0.47;
        fzc_line(_d, -1.0, 2.0 / 3.0, _in, 2.0 / 3.0);
        fzc_line(_d, -1.0, -2.0 / 3.0, _in, -2.0 / 3.0);
    }
    fzc_line(_d, _out / _hw, 0.0, 1.0, 0.0);
}

// A sized display (display.h), laid out in its room (its size's: the catalogue's units) — its leads 10 long; a panel's
// digits, a matrix's dots, an LCD's glass where its characters are.
RDE_INTERNAL void fzc_display(fzc* _d, const fude_zoom_part* _part) {
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED;
    f64 _w = 2.0 * _d->hw, _h = 2.0 * _d->hh;
    fude_zoom_display_room(_part, &_w, &_h);
    const f64 _rw = _w * 0.5, _rh = _h * 0.5;
    const f64 _iu = 1.0 - 10.0 / _rw, _iv = 1.0 - 10.0 / _rh;
    const u32 _cols = fude_zoom_display_cols(_part), _rows = fude_zoom_display_rows(_part);
    fzc_stubs_to(_d, _part, _iu, _iv);
    if(_part->look == FZC_CHAR_LCD) {
        // Its board under its pins; its glass round its characters (18 × 28 each, from 46 under its top).
        fzc_rect(_d, -1.0, -1.0, 1.0, _iv, F);
        const f64 _sx = -9.0 * (f64)_cols, _sy = _rh - 46.0;
        fzc_rect(_d, (_sx - 10.0) / _rw, (_sy + 10.0) / _rh, (10.0 - _sx) / _rw, (_sy - 28.0 * (f64)_rows - 10.0) / _rh, 0u);
        return;
    }
    fzc_rect(_d, -_iu, -_iv, 1.0, 1.0, F);
    if(_part->look == FZC_SEG_PANEL) {
        // Each digit's segments (32 wide, 84 high, 60 apart over its common), its point.
        static const f64 _seg[7][4] = { { -0.5, 1, 0.5, 1 }, { 0.5, 1, 0.5, 0 }, { 0.5, 0, 0.5, -1 }, { -0.5, -1, 0.5, -1 },
                                        { -0.5, 0, -0.5, -1 }, { -0.5, 1, -0.5, 0 }, { -0.5, 0, 0.5, 0 } };
        for(u32 _k = 0; _k < _cols; _k++) {
            const f64 _cx = -_rw + 80.0 + 60.0 * (f64)_k, _cy = 5.0;
            for(u32 _g = 0; _g < 7u; _g++) {
                fzc_line(_d, (_cx + _seg[_g][0] * 32.0) / _rw, (_cy + _seg[_g][1] * 42.0) / _rh, (_cx + _seg[_g][2] * 32.0) / _rw, (_cy + _seg[_g][3] * 42.0) / _rh);
            }
            fzc_circle(_d, (_cx + 24.0) / _rw, (_cy - 42.0) / _rh, 3.0 * _d->hw / _rw, 0u);
        }
        return;
    }
    // A matrix: a ring for each dot (an octagon, 6 round), 20 apart.
    for(u32 _r = 0; _r < _rows; _r++) {
        for(u32 _c = 0; _c < _cols; _c++) {
            const f64 _cx = -_rw + 40.0 + 20.0 * (f64)_c, _cy = _rh - 20.0 - 20.0 * (f64)_r;
            f64 _uv[16];
            for(u32 _k = 0; _k < 8u; _k++) {
                const f64 _a = FZC_PI * 0.25 * (f64)_k + FZC_PI / 8.0;
                _uv[2u * _k]      = (_cx + 6.0 * cos(_a)) / _rw;
                _uv[2u * _k + 1u] = (_cy + 6.0 * sin(_a)) / _rh;
            }
            fzc_poly(_d, _uv, 8u, C);
        }
    }
}

u32 fude_zoom_part_draw(const fude_zoom_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_part == NULL) {
        return 0;
    }
    fzc _d = { _points, _parts, fabs(_hw), fabs(_hh), _segments < 16u ? 16u : (_segments > 128u ? 128u : _segments), 0u };
    const f64 _m = fmin(_d.hw, _d.hh);   // a round part's room
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, C = FUDE_ZOOM_SYMBOL_CLOSED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->look) {
    case FZC_RESISTOR:
        fzc_rect(&_d, -0.6, -0.7, 0.6, 0.7, F);
        fzc_line(&_d, -1.0, 0.0, -0.6, 0.0);
        fzc_line(&_d, 0.6, 0.0, 1.0, 0.0);
        break;
    case FZC_POT:
        fzc_rect(&_d, -0.6, -0.85, 0.6, -0.15, F);
        fzc_line(&_d, -1.0, -0.5, -0.6, -0.5);
        fzc_line(&_d, 0.6, -0.5, 1.0, -0.5);
        fzc_arrow(&_d, 0.0, 1.0, 0.0, -0.1, 0.25 * _d.hh);
        break;
    case FZC_FUSE:
        fzc_rect(&_d, -0.6, -0.6, 0.6, 0.6, F);
        fzc_line(&_d, -1.0, 0.0, 1.0, 0.0);
        break;
    case FZC_CAP:
    case FZC_CAP_POL:
        fzc_line(&_d, -0.15, -0.8, -0.15, 0.8);
        if(_part->look == FZC_CAP) {
            fzc_line(&_d, 0.15, -0.8, 0.15, 0.8);
        } else {
            fzc_begin(&_d);
            fzc_arc_pts(&_d, 0.75 * _d.hw, 0.0, 0.55 * _d.hw, 0.8 * _d.hh, FZC_PI * 0.72, FZC_PI * 1.28);
            fzc_end(&_d, 0u);
            fzc_mark(&_d, '+', -0.45, 0.6, 0.2 * _m);
        }
        fzc_line(&_d, -1.0, 0.0, -0.15, 0.0);
        fzc_line(&_d, _part->look == FZC_CAP ? 0.15 : 0.2, 0.0, 1.0, 0.0);
        break;
    case FZC_INDUCTOR: {
        fzc_begin(&_d);
        fzc_uv(&_d, -1.0, 0.0);
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _c = (-0.6 + 0.15 + 0.3 * (f64)_k) * _d.hw;
            fzc_arc_pts(&_d, _c, 0.0, 0.15 * _d.hw, 0.8 * _d.hh, FZC_PI, 0.0);
        }
        fzc_uv(&_d, 1.0, 0.0);
        fzc_end(&_d, 0u);
        break;
    }
    case FZC_DIODE:
        fzc_diode_body(&_d, 0.3, 0.7);
        break;
    case FZC_ZENER:
        fzc_diode_body(&_d, 0.3, 0.7);
        fzc_line(&_d, 0.3, 0.7, 0.45, 0.9);
        fzc_line(&_d, 0.3, -0.7, 0.15, -0.9);
        break;
    case FZC_LED:
        fzc_diode_body(&_d, 0.25, 0.45);
        fzc_arrow(&_d, 0.0, 0.5, 0.3, 0.95, 0.12 * _d.hw);
        fzc_arrow(&_d, 0.25, 0.4, 0.55, 0.85, 0.12 * _d.hw);
        break;
    case FZC_NPN:
    case FZC_PNP: {
        const f64 _r = 0.8 * _m, _bu = -0.25, _xu = 1.0 / 3.0;
        fzc_circle(&_d, 0.05, 0.0, _r, F);
        fzc_line(&_d, -1.0, 0.0, _bu, 0.0);
        fzc_line(&_d, _bu, -0.5, _bu, 0.5);
        const b8 _p = _part->look == FZC_PNP;
        const f64 _ce = _p ? -1.0 : 1.0;   // (the collector's way: up for NPN, down for PNP)
        fzc_begin(&_d); fzc_uv(&_d, _bu, 0.25 * _ce); fzc_uv(&_d, _xu, 0.65 * _ce); fzc_uv(&_d, _xu, _ce); fzc_end(&_d, 0u);
        fzc_begin(&_d); fzc_uv(&_d, _bu, -0.25 * _ce); fzc_uv(&_d, _xu, -0.65 * _ce); fzc_uv(&_d, _xu, -_ce); fzc_end(&_d, 0u);
        const f64 _ex0 = _bu * _d.hw, _ey0 = -0.25 * _ce * _d.hh, _ex1 = _xu * _d.hw, _ey1 = -0.65 * _ce * _d.hh;
        if(!_p) {
            fzc_head(&_d, _ex0 + (_ex1 - _ex0) * 0.85, _ey0 + (_ey1 - _ey0) * 0.85, _ex1 - _ex0, _ey1 - _ey0, 0.22 * _m);   // (out of the emitter)
        } else {
            fzc_head(&_d, _ex0 + (_ex1 - _ex0) * 0.25, _ey0 + (_ey1 - _ey0) * 0.25, _ex0 - _ex1, _ey0 - _ey1, 0.22 * _m);   // (into the base)
        }
        break;
    }
    case FZC_NMOS:
    case FZC_PMOS: {
        const f64 _r = 0.8 * _m, _xu = 1.0 / 3.0;
        const b8 _p = _part->look == FZC_PMOS;
        const f64 _dv = _p ? -1.0 : 1.0;   // (the drain's way)
        fzc_circle(&_d, 0.05, 0.0, _r, F);
        fzc_line(&_d, -1.0, 0.0, -0.4, 0.0);
        fzc_line(&_d, -0.4, -0.5, -0.4, 0.5);
        fzc_line(&_d, -0.25, 0.3, -0.25, 0.6);
        fzc_line(&_d, -0.25, -0.15, -0.25, 0.15);
        fzc_line(&_d, -0.25, -0.6, -0.25, -0.3);
        fzc_begin(&_d); fzc_uv(&_d, -0.25, 0.45 * _dv); fzc_uv(&_d, _xu, 0.45 * _dv); fzc_uv(&_d, _xu, _dv); fzc_end(&_d, 0u);
        fzc_begin(&_d); fzc_uv(&_d, -0.25, -0.45 * _dv); fzc_uv(&_d, _xu, -0.45 * _dv); fzc_uv(&_d, _xu, -_dv); fzc_end(&_d, 0u);
        fzc_line(&_d, -0.25, 0.0, _xu, 0.0);
        fzc_line(&_d, _xu, 0.0, _xu, -0.45 * _dv);
        if(!_p) {
            fzc_head(&_d, -0.22 * _d.hw, 0.0, -1.0, 0.0, 0.2 * _m);
        } else {
            fzc_head(&_d, 0.2 * _d.hw, 0.0, 1.0, 0.0, 0.2 * _m);
        }
        break;
    }
    case FZC_VSOURCE:
    case FZC_ACSOURCE:
    case FZC_CLOCK:
    case FZC_ISOURCE:
    case FZC_LAMP:
    case FZC_MOTOR:
    case FZC_VOLTMETER:
    case FZC_AMMETER: {
        const b8 _upright = _part->pins[0].side == FUDE_ZOOM_PIN_UP;
        const f64 _r = 0.85 * (_upright ? fmin(_d.hw, _d.hh * 0.6) : fmin(_d.hw * 0.6, _d.hh));
        fzc_circle(&_d, 0.0, 0.0, _r, F);
        if(_upright) {
            fzc_line(&_d, 0.0, 1.0, 0.0, _r / _d.hh);
            fzc_line(&_d, 0.0, -_r / _d.hh, 0.0, -1.0);
        } else {
            fzc_line(&_d, -1.0, 0.0, -_r / _d.hw, 0.0);
            fzc_line(&_d, _r / _d.hw, 0.0, 1.0, 0.0);
        }
        const f64 _ru = _r / _d.hw, _rv = _r / _d.hh;
        if(_part->look == FZC_VSOURCE) {
            fzc_mark(&_d, '+', 0.0, 0.45 * _rv, 0.45 * _r);
            fzc_mark(&_d, '-', 0.0, -0.45 * _rv, 0.45 * _r);
        } else if(_part->look == FZC_ACSOURCE) {
            fzc_begin(&_d);
            for(u32 _i = 0; _i <= 24u; _i++) {
                const f64 _t = (f64)_i / 24.0;
                fzc_pt(&_d, (-0.6 + 1.2 * _t) * _r, 0.35 * _r * sin(_t * 2.0 * FZC_PI));
            }
            fzc_end(&_d, 0u);
        } else if(_part->look == FZC_CLOCK) {
            const f64 _sq[12] = { -0.6 * _ru, -0.3 * _rv, -0.3 * _ru, -0.3 * _rv, -0.3 * _ru, 0.3 * _rv, 0.3 * _ru, 0.3 * _rv, 0.3 * _ru, -0.3 * _rv, 0.6 * _ru, -0.3 * _rv };
            fzc_poly(&_d, _sq, 6u, 0u);
        } else if(_part->look == FZC_ISOURCE) {
            fzc_arrow(&_d, 0.0, -0.5 * _rv, 0.0, 0.6 * _rv, 0.3 * _r);
        } else if(_part->look == FZC_LAMP) {
            const f64 _k = 0.7071 * _r;
            fzc_begin(&_d); fzc_pt(&_d, -_k, -_k); fzc_pt(&_d, _k, _k); fzc_end(&_d, 0u);
            fzc_begin(&_d); fzc_pt(&_d, -_k, _k); fzc_pt(&_d, _k, -_k); fzc_end(&_d, 0u);
        } else {
            fzc_mark(&_d, _part->look == FZC_MOTOR ? 'M' : (_part->look == FZC_VOLTMETER ? 'V' : 'A'), 0.0, 0.0, 0.9 * _r);
        }
        break;
    }
    case FZC_BATTERY:
        fzc_line(&_d, -0.75, 0.35, 0.75, 0.35);
        fzc_line(&_d, -0.35, 0.12, 0.35, 0.12);
        fzc_line(&_d, -0.75, -0.12, 0.75, -0.12);
        fzc_line(&_d, -0.35, -0.35, 0.35, -0.35);
        fzc_line(&_d, 0.0, 1.0, 0.0, 0.35);
        fzc_line(&_d, 0.0, -0.35, 0.0, -1.0);
        fzc_mark(&_d, '+', 0.6, 0.65, 0.25 * _m);
        break;
    case FZC_GROUND:
        fzc_line(&_d, -0.85, 0.2, 0.85, 0.2);
        fzc_line(&_d, 0.0, 1.0, 0.0, 0.2);
        fzc_line(&_d, -0.5, -0.25, 0.5, -0.25);
        fzc_line(&_d, -0.18, -0.7, 0.18, -0.7);
        break;
    case FZC_RAIL: {
        const f64 _a[6] = { -0.7, 0.1, 0.0, 0.9, 0.7, 0.1 };
        fzc_poly(&_d, _a, 3u, 0u);
        fzc_line(&_d, 0.0, -1.0, 0.0, 0.9);
        break;
    }
    case FZC_SWITCH:
        fzc_line(&_d, -0.5, 0.0, 0.45, 0.65);
        fzc_line(&_d, -1.0, 0.0, -0.5, 0.0);
        fzc_line(&_d, 0.5, 0.0, 1.0, 0.0);
        fzc_circle(&_d, -0.5, 0.0, 0.06 * _d.hw, S);
        fzc_circle(&_d, 0.5, 0.0, 0.06 * _d.hw, 0u);
        break;
    case FZC_BUTTON:
        fzc_line(&_d, -0.6, 0.3, 0.6, 0.3);
        fzc_line(&_d, 0.0, 0.3, 0.0, 0.85);
        fzc_line(&_d, -0.25, 0.85, 0.25, 0.85);
        fzc_line(&_d, -1.0, 0.0, -0.5, 0.0);
        fzc_line(&_d, 0.5, 0.0, 1.0, 0.0);
        fzc_circle(&_d, -0.5, 0.0, 0.06 * _d.hw, 0u);
        fzc_circle(&_d, 0.5, 0.0, 0.06 * _d.hw, 0u);
        break;
    case FZC_BUZZER: {
        fzc_begin(&_d);
        fzc_arc_pts(&_d, 0.0, -0.2 * _d.hh, 0.55 * _d.hw, 0.95 * _d.hh, 0.0, FZC_PI);
        fzc_end(&_d, C | F);
        fzc_line(&_d, -1.0, -0.5, -0.3, -0.5);
        fzc_line(&_d, -0.3, -0.5, -0.3, -0.2);
        fzc_line(&_d, 1.0, -0.5, 0.3, -0.5);
        fzc_line(&_d, 0.3, -0.5, 0.3, -0.2);
        break;
    }
    case FZC_OPAMP: {
        const f64 _t[6] = { -0.6, -0.9, -0.6, 0.9, 0.75, 0.0 };
        fzc_poly(&_d, _t, 3u, C | F);
        fzc_line(&_d, -1.0, 0.5, -0.6, 0.5);
        fzc_line(&_d, -1.0, -0.5, -0.6, -0.5);
        fzc_line(&_d, 0.75, 0.0, 1.0, 0.0);
        fzc_line(&_d, 0.0, 1.0, 0.0, 0.9 * 0.75 / 1.35);
        fzc_line(&_d, 0.0, -1.0, 0.0, -0.9 * 0.75 / 1.35);
        fzc_mark(&_d, '-', -0.45, 0.5, 0.15 * _m);
        fzc_mark(&_d, '+', -0.45, -0.5, 0.15 * _m);
        break;
    }
    case FZC_SEVEN_SEG: {
        fzc_rect(&_d, -0.7, -0.75, 0.7, 0.95, F);
        // Its segments (a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle) as lines.
        const f64 _x0 = -0.3, _x1 = 0.3, _y0 = -0.5, _ym = 0.15, _y1 = 0.8;
        fzc_line(&_d, _x0, _y1, _x1, _y1);
        fzc_line(&_d, _x1, _y1, _x1, _ym);
        fzc_line(&_d, _x1, _ym, _x1, _y0);
        fzc_line(&_d, _x0, _y0, _x1, _y0);
        fzc_line(&_d, _x0, _ym, _x0, _y0);
        fzc_line(&_d, _x0, _y1, _x0, _ym);
        fzc_line(&_d, _x0, _ym, _x1, _ym);
        fzc_circle(&_d, 0.45, -0.5, 0.04 * _d.hw, S);
        fzc_stubs(&_d, _part, 0.7);
        fzc_line(&_d, 0.0, -1.0, 0.0, -0.75);
        break;
    }
    case FZC_SEG_PANEL:
    case FZC_MATRIX:
    case FZC_CHAR_LCD:
        fzc_display(&_d, _part);
        break;
    case FZC_BAR_GRAPH:
        // Its body, its ten bars down it, its leads.
        fzc_rect(&_d, -5.0 / 7.0, -1.0, 5.0 / 7.0, 1.0, F);
        for(u32 _k = 0; _k < 10u; _k++) {
            const f64 _v = 1.0 - 2.0 * (f64)(_k + 1u) / 11.0;
            fzc_rect(&_d, -0.28, _v - 0.06, 0.28, _v + 0.06, 0u);
        }
        fzc_stubs(&_d, _part, 5.0 / 7.0);
        break;
    case FZC_METER:
        // Its case, its LCD's window (its digits drawn in it as it reads: display.h), + and − by its leads.
        fzc_rect(&_d, -0.85, -1.0, 0.85, 1.0, F);
        fzc_rect(&_d, -0.78, -0.62, 0.78, 0.62, 0u);
        fzc_stubs(&_d, _part, 0.85);
        fzc_mark(&_d, '+', -0.93, 0.45, 0.22 * _m);
        fzc_mark(&_d, '-', 0.93, 0.45, 0.22 * _m);
        break;
    case FZC_GATE:
        fzc_gate(&_d, _part->gate);
        break;
    case FZC_FLIPFLOP:
        fzc_rect(&_d, -0.6, -0.9, 0.6, 0.9, F);
        fzc_stubs(&_d, _part, 0.6);
        fzc_begin(&_d); fzc_uv(&_d, -0.6, -0.35); fzc_uv(&_d, -0.42, -0.5); fzc_uv(&_d, -0.6, -0.65); fzc_end(&_d, 0u);   // (the clock's notch)
        break;
    case FZC_LOGIC_IN:
        fzc_rect(&_d, -0.8, -0.7, 0.55, 0.7, F);
        fzc_line(&_d, 0.55, 0.0, 1.0, 0.0);
        fzc_rect(&_d, -0.55, -0.3, 0.3, 0.3, 0u);   // (its toggle)
        break;
    case FZC_LOGIC_OUT:
        fzc_circle(&_d, 0.2, 0.0, 0.6 * _m, F);
        fzc_line(&_d, -1.0, 0.0, 0.2 - 0.6 * _m / _d.hw, 0.0);
        break;
    case FZC_CHIP:
        fzc_rect(&_d, -0.6, -1.0, 0.6, 1.0, F);
        fzc_stubs(&_d, _part, 0.6);
        fzc_begin(&_d);   // (its notch, pin 1's end)
        fzc_arc_pts(&_d, 0.0, _d.hh, 0.12 * _d.hw, 0.12 * _d.hw, FZC_PI, 2.0 * FZC_PI);
        fzc_end(&_d, 0u);
        break;
    case FZC_MODULE:
        fzc_rect(&_d, -0.65, -1.0, 1.0, 1.0, F);
        fzc_stubs(&_d, _part, 0.65);
        break;
    case FZC_BLOCK: {
        // (a custom part: a block, its pins' stubs out of it; above and below too where it has pins there)
        b8 _up = false, _down = false;
        for(u32 _k = 0; _k < _part->pin_count; _k++) {
            _up   = _up || _part->pins[_k].side == FUDE_ZOOM_PIN_UP;
            _down = _down || _part->pins[_k].side == FUDE_ZOOM_PIN_DOWN;
        }
        fzc_rect(&_d, -0.75, _down ? -0.75 : -1.0, 0.75, _up ? 0.75 : 1.0, F);
        fzc_stubs(&_d, _part, 0.75);
        break;
    }
    case FZC_REG3:
        fzc_rect(&_d, -0.55, -0.45, 0.55, 1.0, F);
        fzc_stubs(&_d, _part, 0.55);
        fzc_line(&_d, 0.0, -1.0, 0.0, -0.45);
        break;
    case FZC_BOARD: {
            const f64 _r = 0.06 * fmin(_d.hw, _d.hh);
            fzc_begin(&_d);
            fzc_arc_pts(&_d, 0.8 * _d.hw - _r, -_d.hh + _r, _r, _r, -FZC_PI * 0.5, 0.0);
            fzc_arc_pts(&_d, 0.8 * _d.hw - _r, _d.hh - _r, _r, _r, 0.0, FZC_PI * 0.5);
            fzc_arc_pts(&_d, -0.8 * _d.hw + _r, _d.hh - _r, _r, _r, FZC_PI * 0.5, FZC_PI);
            fzc_arc_pts(&_d, -0.8 * _d.hw + _r, -_d.hh + _r, _r, _r, FZC_PI, FZC_PI * 1.5);
            fzc_end(&_d, C | F);
        }
        fzc_stubs(&_d, _part, 0.8);
        break;
    case FZC_BREADBOARD: {
        fzc_rect(&_d, -1.0, -1.0, 1.0, 1.0, F);
        // Its rails' lines (+ and −, along), the gap down its middle, and its holes (each a short dash).
        const f64 _x0 = -1.0 + 2.0 * 1.0 / (f64)FZC_BB_W, _x1 = 1.0 - 2.0 * 1.0 / (f64)FZC_BB_W;
        const f32 _rail_y[4] = { 0.4f, 2.6f, 15.9f, 18.1f };
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _v = 1.0 - 2.0 * (f64)_rail_y[_k] / (f64)FZC_BB_H;
            fzc_line(&_d, _x0, _v, _x1, _v);
        }
        const f64 _gap = 1.0 - 2.0 * 9.25 / (f64)FZC_BB_H;
        fzc_rect(&_d, _x0, _gap - 0.6 / (f64)FZC_BB_H, _x1, _gap + 0.6 / (f64)FZC_BB_H, 0u);
        const f64 _dash = 0.16 * 2.0 / (f64)FZC_BB_W;
        for(u32 _i = 0; _i < (u32)_part->pin_count; _i++) {
            fude_zoom_pin _q;
            fude_zoom_part_pin(_part, _i, &_q);
            fzc_line(&_d, _q.u - _dash, _q.v, _q.u + _dash, _q.v);
        }
        fzc_mark(&_d, '+', -1.0 + 2.0 * 0.6 / (f64)FZC_BB_W, 1.0 - 2.0 * 1.0 / (f64)FZC_BB_H, 0.5 * 2.0 * _d.hh / (f64)FZC_BB_H);
        fzc_mark(&_d, '-', -1.0 + 2.0 * 0.6 / (f64)FZC_BB_W, 1.0 - 2.0 * 2.0 / (f64)FZC_BB_H, 0.5 * 2.0 * _d.hh / (f64)FZC_BB_H);
        break;
    }
    default:
        fzc_rect(&_d, -1.0, -1.0, 1.0, 1.0, F);
        break;
    }
    return (u32)rde_arr_length(_parts);
}

// --- pins where they are ---------------------------------------------------------------------------

RDE_INTERNAL void fzc_text_of(const fude_zoom_scene* _s, u32 _object, c8* _out, usize _size);

const fude_zoom_part* fude_zoom_part_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return NULL;
    }
    f64 _n[3];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) < 3u || !(_n[0] >= 0.0)) {
        return NULL;
    }
    const fude_zoom_part* _part = fude_zoom_part_of_kind((u32)_n[0]);
    if(fude_zoom_part_custom(_part) || fude_zoom_display_made(_part)) {
        // A custom part (the definition its text names, its pins round its block), a sized display (as big as its text
        // says).
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fzc_text_of(_s, _object, _text, sizeof(_text));
        return fude_zoom_part_of_text((u32)_n[0], _text);
    }
    return _part;
}

b8 fude_zoom_part_custom(const fude_zoom_part* _part) {
    return _part != NULL && _part->model == FUDE_ZOOM_MODEL_SIM && _part->look == FZC_BLOCK;
}

b8 fude_zoom_part_made(const fude_zoom_part* _part) {
    return (fude_zoom_part_custom(_part) && _part->pin_count > 0u) || fude_zoom_display_made(_part);
}

const fude_zoom_part* fude_zoom_part_of_text(u32 _kind, const c8* _text) {
    const fude_zoom_part* _part = fude_zoom_part_of_kind(_kind);
    if(fude_zoom_part_custom(_part)) {
        const fude_zoom_part* _made = fude_zoom_logic_part(_part, fude_zoom_logic_find(_text != NULL ? _text : ""), false);
        return _made != NULL ? _made : _part;
    }
    return fude_zoom_display_part(_part, _text);   // (a sized display as big as it says; any other itself)
}

const fude_zoom_part* fude_zoom_part_of_numbers(const f64* _n, u32 _count) {
    if(_count < 3u || !(_n[0] >= 0.0)) {
        return NULL;
    }
    const fude_zoom_part* _part = fude_zoom_part_of_kind((u32)_n[0]);
    if((fude_zoom_part_custom(_part) || fude_zoom_display_made(_part)) && _count > 4u) {
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
        return fude_zoom_part_of_text((u32)_n[0], _text);
    }
    return _part;
}

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

// --- values --------------------------------------------------------------------------------------------

b8 fude_zoom_circuit_value(const c8* _text, u32 _index, f64* _out) {
    const c8* _p = _text;
    u32 _seen = 0;
    while(_p != NULL && *_p != 0) {
        if(!((*_p >= '0' && *_p <= '9') || (*_p == '.' && _p[1] >= '0' && _p[1] <= '9'))) {
            _p++;
            continue;
        }
        // A number; then a metric prefix (or one standing for the point: 4k7).
        c8* _end = NULL;
        f64 _v = strtod(_p, &_end);
        if(_end == _p) {
            _p++;
            continue;
        }
        _p = _end;
        while(*_p == ' ') {
            _p++;
        }
        f64 _k = 1.0;
        u32 _skip = 1u;
        switch(*_p) {
        case 'p': _k = 1e-12; break;
        case 'n': _k = 1e-9;  break;
        case 'u': _k = 1e-6;  break;
        case 'm': _k = 1e-3;  break;
        case 'k': case 'K': _k = 1e3; break;
        case 'M': _k = 1e6;   break;
        case 'G': _k = 1e9;   break;
        case 'R': _k = 1.0;   break;   // (4R7)
        default:
            if((u8)_p[0] == 0xC2 && (u8)_p[1] == 0xB5) { _k = 1e-6; _skip = 2u; }   // µ
            else { _skip = 0u; }
            break;
        }
        // "mm"? A prefix only before a unit's letter, a digit, a space or the end (not "m" of "mA"... which is milli: kept).
        _p += _skip;
        if(_skip > 0u && *_p >= '0' && *_p <= '9') {
            // 4k7: the digits after it its decimals.
            c8* _e2 = NULL;
            const f64 _frac = strtod(_p, &_e2);
            u32 _digits = (u32)(_e2 - _p);
            _v += _frac / pow(10.0, (f64)_digits);
            _p = _e2;
        }
        if(_seen == _index) {
            *_out = _v * _k;
            return true;
        }
        _seen++;
    }
    return false;
}

// --- wires ------------------------------------------------------------------------------------------

u32 fude_zoom_wire_numbers(f64* _n, const fude_zoom_v2* _points, u32 _count, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin) {
    _count = _count < FUDE_ZOOM_WIRE_POINTS ? _count : FUDE_ZOOM_WIRE_POINTS;
    _n[0] = (f64)_count;
    for(u32 _i = 0; _i < _count; _i++) {
        _n[1u + 2u * _i] = _points[_i].x - _points[0].x;
        _n[2u + 2u * _i] = _points[_i].y - _points[0].y;
    }
    u32 _k = 1u + 2u * _count;
    fude_zoom_id_put(&_n[_k], _from);
    _k += 2u;
    _n[_k++] = (f64)_from_pin;
    fude_zoom_id_put(&_n[_k], _to);
    _k += 2u;
    _n[_k++] = (f64)_to_pin;
    return _k;
}

u32 fude_zoom_wire_of(const f64* _n, u32 _count, fude_zoom_v2* _points, fude_zoom_id* _from, i32* _from_pin, fude_zoom_id* _to, i32* _to_pin) {
    if(_count < 1u) {
        return 0;
    }
    const u32 _k = (u32)_n[0];
    if(_k < 2u || _k > FUDE_ZOOM_WIRE_POINTS || _count < 1u + 2u * _k) {
        return 0;
    }
    for(u32 _i = 0; _i < _k && _points != NULL; _i++) {
        _points[_i] = (fude_zoom_v2){ _n[1u + 2u * _i], _n[2u + 2u * _i] };
    }
    const f64* _r = &_n[1u + 2u * _k];
    if(_count >= 7u + 2u * _k) {
        // (each id its two halves)
        if(_from != NULL)     { *_from     = fude_zoom_id_get(&_r[0]); }
        if(_from_pin != NULL) { *_from_pin = (i32)_r[2]; }
        if(_to != NULL)       { *_to       = fude_zoom_id_get(&_r[3]); }
        if(_to_pin != NULL)   { *_to_pin   = (i32)_r[5]; }
        return _k;
    }
    const b8 _refs = _count >= 5u + 2u * _k;   // (an old save's: an id as one number)
    if(_from != NULL)     { *_from     = _refs && _r[0] > 0.0 ? (fude_zoom_id)_r[0] : 0u; }
    if(_from_pin != NULL) { *_from_pin = _refs ? (i32)_r[1] : -1; }
    if(_to != NULL)       { *_to       = _refs && _r[2] > 0.0 ? (fude_zoom_id)_r[2] : 0u; }
    if(_to_pin != NULL)   { *_to_pin   = _refs ? (i32)_r[3] : -1; }
    return _k;
}

u32 fude_zoom_wire_part(const fude_zoom_scene* _s, fude_zoom_id _id, i32 _pin, u32 _frame, fude_zoom_v2 _end) {
    if(_id == 0u || _pin < 0) {
        return FUDE_ZOOM_NONE;
    }
    const u32 _o = fude_zoom_scene_find_object(_s, _id);
    if(_o != FUDE_ZOOM_NONE) {
        return _o;
    }
    // (an old save's id, rounded as a number: of the parts whose ids round so, the one whose pin is at that end)
    const f64 _rounded = (f64)_id;
    u32 _best = FUDE_ZOOM_NONE;
    f64 _least = 1e300;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _i);
        if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_SYMBOL || (f64)_ob->id != _rounded) {
            continue;
        }
        fude_zoom_v2 _p;
        if(fude_zoom_part_of(_s, _i) != NULL && fude_zoom_part_pin_in(_s, _i, (u32)_pin, _frame, &_p, NULL, NULL)) {
            const f64 _d = hypot(_p.x - _end.x, _p.y - _end.y);
            if(_d < _least) {
                _least = _d;
                _best  = _i;
            }
        }
    }
    return _best;
}

// A point a little out of a pin, its side's way.
RDE_INTERNAL fude_zoom_v2 fzc_out(fude_zoom_v2 _p, u8 _side, f64 _by) {
    switch(_side) {
    case FUDE_ZOOM_PIN_LEFT:  return (fude_zoom_v2){ _p.x - _by, _p.y };
    case FUDE_ZOOM_PIN_RIGHT: return (fude_zoom_v2){ _p.x + _by, _p.y };
    case FUDE_ZOOM_PIN_UP:    return (fude_zoom_v2){ _p.x, _p.y + _by };
    case FUDE_ZOOM_PIN_DOWN:  return (fude_zoom_v2){ _p.x, _p.y - _by };
    default:                  return _p;
    }
}

u32 fude_zoom_wire_route(fude_zoom_v2 _a, u8 _side_a, fude_zoom_v2 _b, u8 _side_b, f64 _stub, fude_zoom_v2* _out) {
    static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
    fude_zoom_v2 _p[6];
    u32 _n = 0;
    _p[_n++] = _a;
    const b8 _sa = _side_a <= FUDE_ZOOM_PIN_DOWN, _sb = _side_b <= FUDE_ZOOM_PIN_DOWN;
    const fude_zoom_v2 _a1 = _sa ? (fude_zoom_v2){ _a.x + _way[_side_a][0] * _stub, _a.y + _way[_side_a][1] * _stub } : _a;
    const fude_zoom_v2 _b1 = _sb ? (fude_zoom_v2){ _b.x + _way[_side_b][0] * _stub, _b.y + _way[_side_b][1] * _stub } : _b;
    if(_sa) {
        _p[_n++] = _a1;
    }
    // Between the stubs' ends: across first when that goes on the way out of _a (or, leaving either way, the longer way).
    const f64 _dx = _b1.x - _a1.x, _dy = _b1.y - _a1.y;
    b8 _across;
    if(_sa) {
        const b8 _horiz = _side_a == FUDE_ZOOM_PIN_LEFT || _side_a == FUDE_ZOOM_PIN_RIGHT;
        const f64 _on = _horiz ? _way[_side_a][0] * _dx : _way[_side_a][1] * _dy;   // (how far on its own way b lies)
        _across = _horiz ? _on > 0.0 : !(_on > 0.0);
    } else if(_sb) {
        const b8 _horiz = _side_b == FUDE_ZOOM_PIN_LEFT || _side_b == FUDE_ZOOM_PIN_RIGHT;
        _across = !_horiz;   // (into b along its own way last)
    } else {
        _across = fabs(_dx) >= fabs(_dy);
    }
    _p[_n++] = _across ? (fude_zoom_v2){ _b1.x, _a1.y } : (fude_zoom_v2){ _a1.x, _b1.y };
    if(_sb) {
        _p[_n++] = _b1;
    }
    _p[_n++] = _b;
    // (points that add nothing: the same as the one before, or on a straight line through)
    const f64 _eps = 1e-9 * fmax(fmax(fabs(_a.x), fabs(_a.y)), fmax(fmax(fabs(_b.x), fabs(_b.y)), 1.0));
    u32 _m = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        if(_m > 0u && fabs(_p[_i].x - _out[_m - 1u].x) <= _eps && fabs(_p[_i].y - _out[_m - 1u].y) <= _eps) {
            continue;
        }
        if(_m >= 2u) {
            const fude_zoom_v2 _u = _out[_m - 2u], _v = _out[_m - 1u];
            const f64 _cross = (_v.x - _u.x) * (_p[_i].y - _v.y) - (_v.y - _u.y) * (_p[_i].x - _v.x);
            const f64 _dot   = (_v.x - _u.x) * (_p[_i].x - _v.x) + (_v.y - _u.y) * (_p[_i].y - _v.y);
            if(fabs(_cross) <= _eps * (fabs(_v.x - _u.x) + fabs(_v.y - _u.y) + fabs(_p[_i].x - _v.x) + fabs(_p[_i].y - _v.y)) && _dot >= 0.0) {
                _out[_m - 1u] = _p[_i];   // (straight on: the middle point goes)
                continue;
            }
        }
        _out[_m++] = _p[_i];
    }
    if(_m < 2u) {
        _out[0] = _a;
        _out[1] = _b;
        _m = 2u;
    }
    return _m;
}

u32 fude_zoom_wire_add(fude_zoom_scene* _s, u32 _frame, const fude_zoom_v2* _points, u32 _count, u32 _from, i32 _from_pin, u32 _to, i32 _to_pin,
                       rde_color _color, f32 _radius) {
    f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
    const fude_zoom_id _fa = _from != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _from)->id : 0u;
    const fude_zoom_id _tb = _to != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _to)->id : 0u;
    const u32 _k = fude_zoom_wire_numbers(_n, _points, _count, _fa, _from != FUDE_ZOOM_NONE ? _from_pin : -1, _tb, _to != FUDE_ZOOM_NONE ? _to_pin : -1);
    return fude_zoom_scene_add_shape(_s, _frame, (fude_zoom_place){ _points[0], 0.0, 1.0 }, FUDE_ZOOM_SHAPE_WIRE, _n, _k, _color, _radius, 0u, 0);
}

u8 fude_zoom_part_side_at(const fude_zoom_scene* _s, u32 _object, u32 _pin) {
    const fude_zoom_part* _p = fude_zoom_part_of(_s, _object);
    fude_zoom_pin _q;
    if(_p == NULL || !fude_zoom_part_pin(_p, _pin, &_q) || _q.side > FUDE_ZOOM_PIN_DOWN) {
        return 255u;
    }
    static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
    const f64 _r = fude_zoom_scene_object(_s, _object)->rotation;
    const f64 _x = _way[_q.side][0] * cos(_r) - _way[_q.side][1] * sin(_r), _y = _way[_q.side][0] * sin(_r) + _way[_q.side][1] * cos(_r);
    return fabs(_x) >= fabs(_y) ? (_x < 0.0 ? FUDE_ZOOM_PIN_LEFT : FUDE_ZOOM_PIN_RIGHT) : (_y > 0.0 ? FUDE_ZOOM_PIN_UP : FUDE_ZOOM_PIN_DOWN);
}

b8 fude_zoom_part_pin_in(const fude_zoom_scene* _s, u32 _object, u32 _pin, u32 _frame, fude_zoom_v2* _at, u8* _side, f64* _stub) {
    fude_zoom_v2 _p;
    if(!fude_zoom_part_pin_at(_s, _object, _pin, &_p)) {
        return false;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    const fude_zoom_sim _to = fude_zoom_scene_sim(_s, _o->frame, _frame);
    *_at = fude_zoom_sim_apply(_to, _p);
    if(_side != NULL) {
        u8 _sd = fude_zoom_part_side_at(_s, _object, _pin);
        const f64 _turn = atan2(_to.b, _to.a);
        if(_sd <= FUDE_ZOOM_PIN_DOWN && fabs(_turn) > 1e-6) {
            // (that frame turned against the part's: the side as it faces there)
            static const f64 _way[4][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
            const f64 _x = _way[_sd][0] * cos(_turn) - _way[_sd][1] * sin(_turn), _y = _way[_sd][0] * sin(_turn) + _way[_sd][1] * cos(_turn);
            _sd = fabs(_x) >= fabs(_y) ? (_x < 0.0 ? FUDE_ZOOM_PIN_LEFT : FUDE_ZOOM_PIN_RIGHT) : (_y > 0.0 ? FUDE_ZOOM_PIN_UP : FUDE_ZOOM_PIN_DOWN);
        }
        *_side = _sd;
    }
    if(_stub != NULL) {
        // (a tenth of a resistor's length: the parts' grid)
        f64 _n3[3];
        const fude_zoom_symbol_info* _info = fude_zoom_scene_shape_numbers(_s, _object, _n3, 3u) >= 3u ? fude_zoom_symbol_info_of((u32)_n3[0]) : NULL;
        const f64 _room = _info != NULL ? fude_zoom_part_room_h(fude_zoom_part_of(_s, _object), (f64)_info->h) : 0.0;
        *_stub = _room > 0.0 ? _n3[2] * _o->scale / (_room * 0.5) * 10.0 * fude_zoom_sim_scale(_to) : 0.0;
    }
    return true;
}

// _wire made again: its ends at their pins (where joined: _a_obj/_b_obj its parts now, by id), routed square; or none
// when nothing moved.
RDE_INTERNAL u32 fzc_wire_again(fude_zoom_scene* _s, u32 _wire, fude_zoom_id _from, i32 _from_pin, fude_zoom_id _to, i32 _to_pin) {
    const fude_zoom_object _o = *fude_zoom_scene_object(_s, _wire);
    f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
    const u32 _count = fude_zoom_scene_shape_numbers(_s, _wire, _n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
    fude_zoom_v2 _p[FUDE_ZOOM_WIRE_POINTS];
    const u32 _k = fude_zoom_wire_of(_n, _count, _p, NULL, NULL, NULL, NULL);
    if(_k < 2u) {
        return FUDE_ZOOM_NONE;
    }
    const fude_zoom_sim _sim = fude_zoom_object_sim(&_o);
    for(u32 _i = 0; _i < _k; _i++) {
        _p[_i] = fude_zoom_sim_apply(_sim, _p[_i]);
    }
    const u32 _a = fude_zoom_wire_part(_s, _from, _from_pin, _o.frame, _p[0]);
    const u32 _b = fude_zoom_wire_part(_s, _to, _to_pin, _o.frame, _p[_k - 1u]);
    fude_zoom_v2 _pa = _p[0], _pb = _p[_k - 1u];
    // (each end at its pin wherever its part is: in the wire's frame, or another, deeper or shallower)
    u8 _sa = 255u, _sb = 255u;
    f64 _stub_a = 0.0, _stub_b = 0.0;
    const b8 _ha = _a != FUDE_ZOOM_NONE && _from_pin >= 0 && fude_zoom_part_pin_in(_s, _a, (u32)_from_pin, _o.frame, &_pa, &_sa, &_stub_a);
    const b8 _hb = _b != FUDE_ZOOM_NONE && _to_pin >= 0 && fude_zoom_part_pin_in(_s, _b, (u32)_to_pin, _o.frame, &_pb, &_sb, &_stub_b);
    fude_zoom_v2 _r[6];
    const f64 _stub = _ha ? _stub_a : _stub_b;
    const u32 _m = fude_zoom_wire_route(_pa, _ha ? _sa : 255u, _pb, _hb ? _sb : 255u, _stub, _r);
    const u16 _layer = _s->layer;
    _s->layer = _o.layer;
    const u32 _made = fude_zoom_wire_add(_s, _o.frame, _r, _m, _ha ? _a : FUDE_ZOOM_NONE, _from_pin, _hb ? _b : FUDE_ZOOM_NONE, _to_pin, _o.color, _o.radius);
    _s->layer = _layer;
    return _made;
}

u32 fude_zoom_wire_follow(fude_zoom_scene* _s, const u32* _moved, u32 _count, rde_arr* _died, rde_arr* _born) {
    u32 _made = 0;
    const u32 _all = fude_zoom_scene_object_count(_s);
    for(u32 _i = 0; _i < _all; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_WIRE) {
            continue;
        }
        f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
        const u32 _c = fude_zoom_scene_shape_numbers(_s, _i, _n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
        fude_zoom_id _from, _to;
        i32 _fp, _tp;
        fude_zoom_v2 _ends[FUDE_ZOOM_WIRE_POINTS];
        const u32 _k = fude_zoom_wire_of(_n, _c, _ends, &_from, &_fp, &_to, &_tp);
        if(_k < 2u) {
            continue;
        }
        // (the parts its ends keep: by their ids — an old save's, by their pins where its ends are)
        const fude_zoom_sim _ws = fude_zoom_object_sim(_o);
        const u32 _pa = fude_zoom_wire_part(_s, _from, _fp, _o->frame, fude_zoom_sim_apply(_ws, _ends[0]));
        const u32 _pb = fude_zoom_wire_part(_s, _to, _tp, _o->frame, fude_zoom_sim_apply(_ws, _ends[_k - 1u]));
        b8 _from_moved = false, _to_moved = false, _self = false;
        for(u32 _m = 0; _m < _count; _m++) {
            _from_moved = _from_moved || (_pa != FUDE_ZOOM_NONE && _moved[_m] == _pa);
            _to_moved   = _to_moved || (_pb != FUDE_ZOOM_NONE && _moved[_m] == _pb);
            _self       = _self || _moved[_m] == _i;
        }
        // (moved itself: whole with its parts — or, joined to one left where it was, made again to it)
        const b8 _whole = _self && (_from == 0u || _from_moved) && (_to == 0u || _to_moved);
        if(_whole || (!_self && !_from_moved && !_to_moved)) {
            continue;
        }
        const u32 _w = fzc_wire_again(_s, _i, _from, _fp, _to, _tp);
        if(_w != FUDE_ZOOM_NONE) {
            fude_zoom_scene_set_alive(_s, _i, false);
            rde_arr_add(_died, (any)&_i);
            rde_arr_add(_born, (any)&_w);
            _made++;
        }
    }
    return _made;
}

// Does home point _p lie along wire _w (its own frame's polyline, to the home frame) — within its width?
RDE_INTERNAL b8 fzc_end_on_wire(const fude_zoom_scene* _s, u32 _w, u32 _home, fude_zoom_v2 _p) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _w);
    f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
    const u32 _c = fude_zoom_scene_shape_numbers(_s, _w, _n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
    fude_zoom_v2 _q[FUDE_ZOOM_WIRE_POINTS];
    const u32 _k = fude_zoom_wire_of(_n, _c, _q, NULL, NULL, NULL, NULL);
    const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
    const f64 _tol = fmax((f64)_o->radius * _o->scale * fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * 1.5, 1e-9);
    for(u32 _i = 0; _i + 1u < _k; _i++) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_to, _q[_i]), _b = fude_zoom_sim_apply(_to, _q[_i + 1u]);
        const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
        const f64 _ll = _d.x * _d.x + _d.y * _d.y;
        const f64 _u = _ll > 0.0 ? fmin(fmax(((_p.x - _a.x) * _d.x + (_p.y - _a.y) * _d.y) / _ll, 0.0), 1.0) : 0.0;
        if(hypot(_a.x + _d.x * _u - _p.x, _a.y + _d.y * _u - _p.y) <= _tol) {
            return true;
        }
    }
    return false;
}

u32 fude_zoom_wire_orphans(fude_zoom_scene* _s, rde_arr* _died) {
    const u32 _no = fude_zoom_scene_object_count(_s);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_arr _dead_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_dead_arr, _no);
    u8* _dead = (u8*)_dead_arr.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_died); _i++) {
        const u32 _o = ((const u32*)_died->memory)[_i];
        if(_o < _no) {
            _dead[_o] = 1u;
        }
    }
    u32 _made = 0;
    for(b8 _again = true; _again;) {   // (round after round: a wire gone takes those on it with it)
        _again = false;
        for(u32 _w = 0; _w < _no; _w++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _w);
            if(_dead[_w] || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_WIRE) {
                continue;
            }
            f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
            const u32 _c = fude_zoom_scene_shape_numbers(_s, _w, _n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
            fude_zoom_v2 _q[FUDE_ZOOM_WIRE_POINTS];
            fude_zoom_id _ids[2];
            i32 _pins[2];
            const u32 _k = fude_zoom_wire_of(_n, _c, _q, &_ids[0], &_pins[0], &_ids[1], &_pins[1]);
            if(_k < 2u) {
                continue;
            }
            const fude_zoom_sim _to = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
            b8 _orphan = false;
            for(u32 _e = 0; _e < 2u && !_orphan; _e++) {
                if(_ids[_e] != 0u && _pins[_e] >= 0) {
                    // (at a part's pin: that part gone)
                    const u32 _part = fude_zoom_scene_find_object(_s, _ids[_e]);
                    _orphan = _part != FUDE_ZOOM_NONE && _part < _no && _dead[_part];
                    continue;
                }
                // (on a wire: that wire gone, and nothing else alive under the end)
                const fude_zoom_v2 _p = fude_zoom_sim_apply(_to, _q[_e == 0u ? 0u : _k - 1u]);
                b8 _on_dead = false, _on_live = false;
                for(u32 _v = 0; _v < _no && !_on_live; _v++) {
                    const fude_zoom_object* _vo = fude_zoom_scene_object(_s, _v);
                    if(_v == _w || _vo->kind != FUDE_ZOOM_KIND_SHAPE || _vo->channels != FUDE_ZOOM_SHAPE_WIRE) {
                        continue;
                    }
                    const b8 _gone = _dead[_v] != 0u;
                    if(!_gone && !(_vo->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                        continue;
                    }
                    if(fzc_end_on_wire(_s, _v, _home, _p)) {
                        _on_dead = _on_dead || _gone;
                        _on_live = _on_live || !_gone;
                    }
                }
                _orphan = _on_dead && !_on_live;
            }
            if(_orphan) {
                fude_zoom_scene_set_alive(_s, _w, false);
                _dead[_w] = 1u;
                rde_arr_add(_died, (any)&_w);
                _made++;
                _again = true;
            }
        }
    }
    rde_arr_free(&_dead_arr);
    return _made;
}

u32 fude_zoom_wire_remap(fude_zoom_scene* _s, u32 _wire, const fude_zoom_id* _old, const fude_zoom_id* _new, u32 _n) {
    f64 _num[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
    const u32 _c = fude_zoom_scene_shape_numbers(_s, _wire, _num, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
    fude_zoom_id _from, _to;
    i32 _fp, _tp;
    if(fude_zoom_wire_of(_num, _c, NULL, &_from, &_fp, &_to, &_tp) < 2u) {
        return FUDE_ZOOM_NONE;
    }
    b8 _any = false;
    for(u32 _i = 0; _i < _n; _i++) {
        // (an old save's id, rounded as a number, not found as it is: the one it rounds from)
        const b8 _fa = _from != 0u && (_from == _old[_i] || (fude_zoom_scene_find_object(_s, _from) == FUDE_ZOOM_NONE && (f64)_from == (f64)_old[_i]));
        const b8 _tb = _to != 0u && (_to == _old[_i] || (fude_zoom_scene_find_object(_s, _to) == FUDE_ZOOM_NONE && (f64)_to == (f64)_old[_i]));
        if(_fa) { _from = _new[_i]; _any = true; }
        if(_tb) { _to = _new[_i];   _any = true; }
    }
    return _any ? fzc_wire_again(_s, _wire, _from, _fp, _to, _tp) : FUDE_ZOOM_NONE;
}

// --- the circuit, from the canvas ---------------------------------------------------------------------

#define FZC_GMIN      1e-12
#define FZC_VT        0.025852
#define FZC_GROUND    0xFFFFu     // (a "pin" that is ground itself: node 0)
#define FZC_LOGIC_V   5.0         // a standalone gate's, flip-flop's, logic input's high
#define FZC_LOGIC_R   25.0        // ...and its output's resistance
#define FZC_PIN_MAX   64u

typedef struct {
    fude_zoom_v2 at;
    f64          unit;
    u32          part;    // its part (in the circuit's), or FUDE_ZOOM_NONE: a wire's end
    u32          pin;     // ...its pin; a wire's: which end (0, 1)
    u32          wire;
} fzc_term;

RDE_INTERNAL u32 fzc_root(u32* _p, u32 _x) {
    while(_p[_x] != _x) {
        _p[_x] = _p[_p[_x]];
        _x = _p[_x];
    }
    return _x;
}

RDE_INTERNAL void fzc_join(u32* _p, u32 _a, u32 _b) {
    _a = fzc_root(_p, _a);
    _b = fzc_root(_p, _b);
    if(_a < _b) {
        _p[_b] = _a;
    } else if(_b < _a) {
        _p[_a] = _b;
    }
}

RDE_INTERNAL b8 fzc_same_name(const c8* _a, const c8* _b);

// Its blocks' systems let go.
RDE_INTERNAL void fzc_systems_free(fude_zoom_circuit* _c) {
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    for(u32 _b = 0; _b < (u32)rde_arr_length(&_c->block_system); _b++) {
        fude_sim_sparse_free(&_sys[_b]);
    }
    rde_arr_clear(&_c->block_system);
}

// Its unknowns in blocks (fude_zoom_circuit's): the nodes each part's pins are on joined (ground not: what is joined only
// through it is apart), each block numbered as its first node comes; each node's slot its block's first and its place
// in it; each block's matrix after the one before's.
RDE_INTERNAL void fzc_blocks(fude_zoom_circuit* _c) {
    const u32 _n = _c->nodes > 0u ? _c->nodes : 1u;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _up_arr = rde_arr_new(sizeof(u32), _heap), _id_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_up_arr, _n);
    rde_arr_resize(&_id_arr, _n);
    u32* _up = (u32*)_up_arr.memory;
    u32* _id = (u32*)_id_arr.memory;
    for(u32 _k = 0; _k < _n; _k++) {
        _up[_k] = _k;
        _id[_k] = FUDE_ZOOM_NONE;
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        u32 _first = FUDE_ZOOM_NONE;
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < FZC_PIN_MAX; _k++) {
            const u32 _node = _p[_i].node[_k];
            if(_node == FUDE_ZOOM_NONE || _node == 0u || _node >= _n) {
                continue;
            }
            if(_first == FUDE_ZOOM_NONE) {
                _first = _node;
            } else {
                fzc_join(_up, _first, _node);
            }
        }
    }
    rde_arr_resize(&_c->node_block, _n);
    rde_arr_resize(&_c->node_slot, _n);
    rde_arr_clear(&_c->block_size);
    u32* _block = (u32*)_c->node_block.memory;
    u32* _slot  = (u32*)_c->node_slot.memory;
    _block[0] = _slot[0] = FUDE_ZOOM_NONE;   // (ground: no unknown)
    u32 _blocks = 0;
    for(u32 _k = 1; _k < _n; _k++) {
        const u32 _r = fzc_root(_up, _k);
        if(_id[_r] == FUDE_ZOOM_NONE) {
            _id[_r] = _blocks++;
            const u32 _zero = 0u;
            rde_arr_add(&_c->block_size, (any)&_zero);
        }
        _block[_k] = _id[_r];
        _slot[_k]  = ((u32*)_c->block_size.memory)[_id[_r]]++;   // (its place in its block, for now)
    }
    _c->blocks = _blocks;
    rde_arr_resize(&_c->block_first, _blocks);
    rde_arr_resize(&_c->block_done, _blocks);
    const u32* _size = (const u32*)_c->block_size.memory;
    u32* _first = (u32*)_c->block_first.memory;
    u32 _slots = 0;
    for(u32 _b = 0; _b < _blocks; _b++) {
        _first[_b] = _slots;
        _slots    += _size[_b];
    }
    // Each block's system, shaped before it is first solved (fzc_shape).
    fzc_systems_free(_c);
    rde_arr_resize(&_c->block_system, _blocks);
    for(u32 _b = 0; _b < _blocks; _b++) {
        fude_sim_sparse_init(&((fude_sim_sparse*)_c->block_system.memory)[_b]);
    }
    rde_arr_clear(&_c->joins);
    _c->reshape = true;
    for(u32 _k = 1; _k < _n; _k++) {
        _slot[_k] += _first[_block[_k]];
    }
    fude_zoom_circuit_part* _q = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    for(u32 _i = 0; _i < _np; _i++) {
        _q[_i].block = FUDE_ZOOM_NONE;
        for(u32 _k = 0; _k < _q[_i].part->pin_count && _k < FZC_PIN_MAX && _q[_i].block == FUDE_ZOOM_NONE; _k++) {
            const u32 _node = _q[_i].node[_k];
            _q[_i].block = _node != FUDE_ZOOM_NONE && _node != 0u && _node < _n ? _block[_node] : FUDE_ZOOM_NONE;
        }
    }
    // A block ground reaches nowhere (a battery and what it lights, on a canvas whose ground is elsewhere) has its level
    // from nothing — a system all but singular. Its reference is held at 0 V, as a canvas without ground holds its first
    // source's minus: a board's GND, else a source's minus, else its first node. (Nothing flows that way: nothing else of
    // it reaches ground.)
    rde_arr_resize(&_c->block_reference, _blocks);
    u32* _ref = (u32*)_c->block_reference.memory;
    rde_arr TYPE(u8) _grounded_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_grounded_arr, _blocks);
    u8* _grounded = (u8*)_grounded_arr.memory;
    for(u32 _i = 0; _i < _np; _i++) {
        for(u32 _k = 0; _k < _q[_i].part->pin_count && _k < FZC_PIN_MAX && _q[_i].block != FUDE_ZOOM_NONE; _k++) {
            _grounded[_q[_i].block] = _grounded[_q[_i].block] || _q[_i].node[_k] == 0u;
        }
    }
    for(u32 _b = 0; _b < _blocks; _b++) {
        _ref[_b] = FUDE_ZOOM_NONE;
    }
    for(u32 _pass = 0; _pass < 3u; _pass++) {
        const u32 _count = _pass < 2u ? _np : _n;
        for(u32 _i = 0; _i < _count; _i++) {
            u32 _node = FUDE_ZOOM_NONE;
            if(_pass == 2u) {
                _node = _i;   // (its first node)
            } else if(_pass == 0u && _q[_i].part->model == FUDE_ZOOM_MODEL_BOARD) {
                for(u32 _k = 0; _k < _q[_i].part->pin_count && _k < FZC_PIN_MAX && _node == FUDE_ZOOM_NONE; _k++) {
                    _node = fzc_same_name(_q[_i].part->pins[_k].name, "GND") ? _q[_i].node[_k] : FUDE_ZOOM_NONE;
                }
            } else if(_pass == 1u) {
                const u8 _m = _q[_i].part->model;
                const b8 _source = _m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_ACSOURCE ||
                                   _m == FUDE_ZOOM_MODEL_CLOCK || _m == FUDE_ZOOM_MODEL_ISOURCE;
                _node = _source ? _q[_i].node[1] : FUDE_ZOOM_NONE;   // (its minus)
            }
            if(_node != FUDE_ZOOM_NONE && _node != 0u && _node < _n && !_grounded[_block[_node]] && _ref[_block[_node]] == FUDE_ZOOM_NONE) {
                _ref[_block[_node]] = _node;
            }
        }
    }
    rde_arr_free(&_grounded_arr);
    rde_arr_free(&_up_arr);
    rde_arr_free(&_id_arr);
}

void fude_zoom_circuit_init(fude_zoom_circuit* _c) {
    memset(_c, 0, sizeof(*_c));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _c->parts      = rde_arr_new(sizeof(fude_zoom_circuit_part), _heap);
    _c->wires      = rde_arr_new(sizeof(fude_zoom_circuit_wire), _heap);
    _c->cut_vertex = rde_arr_new(sizeof(u32), _heap);
    _c->pin_vertex = rde_arr_new(sizeof(u32), _heap);
    _c->a          = rde_arr_new(sizeof(f64), _heap);
    _c->rhs        = rde_arr_new(sizeof(f64), _heap);
    _c->x          = rde_arr_new(sizeof(f64), _heap);
    _c->v          = rde_arr_new(sizeof(f64), _heap);
    _c->v_keep     = rde_arr_new(sizeof(f64), _heap);
    _c->limits_of  = rde_arr_new(sizeof(u32), _heap);
    _c->events     = rde_arr_new(sizeof(fude_zoom_circuit_event), _heap);
    _c->node_block  = rde_arr_new(sizeof(u32), _heap);
    _c->node_slot   = rde_arr_new(sizeof(u32), _heap);
    _c->block_first = rde_arr_new(sizeof(u32), _heap);
    _c->block_size  = rde_arr_new(sizeof(u32), _heap);
    _c->block_system = rde_arr_new(sizeof(fude_sim_sparse), _heap);
    _c->block_reference = rde_arr_new(sizeof(u32), _heap);
    _c->joins       = rde_arr_new(sizeof(u64), _heap);
    _c->block_done  = rde_arr_new(sizeof(u8), _heap);
    _c->block_most  = rde_arr_new(sizeof(f64), _heap);
    _c->cols        = rde_arr_new(sizeof(u32), _heap);
    _c->store       = rde_arr_new(sizeof(u8), _heap);
    _c->step       = 1e-3;
    _c->logic      = (fude_zoom_logic*)_heap->calloc(_heap->allocator, 1, sizeof(fude_zoom_logic));
    fude_zoom_logic_init(_c->logic);
}

void fude_zoom_circuit_destroy(fude_zoom_circuit* _c) {
    rde_arr_free(&_c->parts);
    rde_arr_free(&_c->wires);
    rde_arr_free(&_c->cut_vertex);
    rde_arr_free(&_c->pin_vertex);
    rde_arr_free(&_c->a);
    rde_arr_free(&_c->rhs);
    rde_arr_free(&_c->x);
    rde_arr_free(&_c->v);
    rde_arr_free(&_c->v_keep);
    rde_arr_free(&_c->limits_of);
    rde_arr_free(&_c->events);
    rde_arr_free(&_c->node_block);
    rde_arr_free(&_c->node_slot);
    rde_arr_free(&_c->block_first);
    rde_arr_free(&_c->block_size);
    fzc_systems_free(_c);
    rde_arr_free(&_c->block_system);
    rde_arr_free(&_c->block_reference);
    rde_arr_free(&_c->joins);
    rde_arr_free(&_c->block_done);
    rde_arr_free(&_c->block_most);
    rde_arr_free(&_c->cols);
    rde_arr_free(&_c->store);
    if(_c->logic != NULL) {
        fude_zoom_logic_destroy(_c->logic);
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _heap->free(_heap->allocator, _c->logic);
    }
    memset(_c, 0, sizeof(*_c));
}

// Case-insensitive: does _text hold _word?
RDE_INTERNAL b8 fzc_says(const c8* _text, const c8* _word) {
    const usize _n = strlen(_word);
    for(const c8* _p = _text; *_p != 0; _p++) {
        usize _k = 0;
        while(_k < _n && _p[_k] != 0 && (_p[_k] | 0x20) == (_word[_k] | 0x20)) {
            _k++;
        }
        if(_k == _n) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL b8 fzc_same_name(const c8* _a, const c8* _b) {
    while(*_a != 0 && *_b != 0) {
        if((*_a | 0x20) != (*_b | 0x20)) {
            return false;
        }
        _a++;
        _b++;
    }
    return *_a == 0 && *_b == 0;
}

// An LED's forward voltage at 10 mA, by its colour.
RDE_INTERNAL f64 fzc_led_volts(const c8* _text, c8* _color, usize _size) {
    static const struct { const c8* name; f64 v; } _c[] = {
        { "red", 1.8 }, { "orange", 2.0 }, { "yellow", 2.1 }, { "green", 2.2 }, { "blue", 3.0 }, { "white", 3.0 }, { "pink", 3.0 },
        { "purple", 3.1 }, { "IR", 1.2 }, { "rojo", 1.8 }, { "verde", 2.2 }, { "azul", 3.0 }, { "blanco", 3.0 }, { "amarillo", 2.1 }, { "naranja", 2.0 },
    };
    for(u32 _i = 0; _i < sizeof(_c) / sizeof(_c[0]); _i++) {
        if(fzc_says(_text, _c[_i].name)) {
            snprintf(_color, _size, "%s", _c[_i].name);
            return _c[_i].v;
        }
    }
    snprintf(_color, _size, "red");
    return 1.8;
}

// What a part holds beyond its state (bytes, eight at a time): a display's lights as seen (an f32 each), an LCD's
// controller (display.h).
RDE_INTERNAL u32 fzc_store_size(const fude_zoom_part* _part) {
    if(_part->model == FUDE_ZOOM_MODEL_CHAR_LCD) {
        return (u32)((sizeof(fude_zoom_lcd) + 7u) & ~(usize)7u);
    }
    return (fude_zoom_display_lights(_part) * (u32)sizeof(f32) + 7u) & ~7u;
}

// A part's numbers from its text; _fresh: its switches and drives too (it is new to the circuit, or reset).
RDE_INTERNAL void fzc_read(fude_zoom_circuit_part* _p, const c8* _text, b8 _fresh) {
    f64 _v = 0.0, _w = 0.0;
    const b8 _has = fude_zoom_circuit_value(_text, 0u, &_v), _has2 = fude_zoom_circuit_value(_text, 1u, &_w);
    memset(_p->value, 0, sizeof(_p->value));
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_RESISTOR:  _p->value[0] = _has && _v > 0.0 ? fmax(_v, 1e-3) : 1000.0; break;
    case FUDE_ZOOM_MODEL_POT:
        _p->value[0] = _has && _v > 0.0 ? fmax(_v, 1.0) : 10000.0;
        _p->value[1] = _has2 ? (_w > 1.0 ? _w / 100.0 : _w) : 0.5;
        if(_fresh) {
            _p->state[0] = fmin(fmax(_p->value[1], 0.0), 1.0);
        }
        break;
    case FUDE_ZOOM_MODEL_CAPACITOR: _p->value[0] = _has && _v > 0.0 ? _v : 100e-9; break;
    case FUDE_ZOOM_MODEL_INDUCTOR:  _p->value[0] = _has && _v > 0.0 ? _v : 10e-3; break;
    case FUDE_ZOOM_MODEL_LED:       _p->value[0] = fzc_led_volts(_text, _p->color, sizeof(_p->color)); break;
    case FUDE_ZOOM_MODEL_ZENER:     _p->value[0] = _has && _v > 0.0 ? _v : 5.1; break;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:      _p->value[0] = _has ? _v : 5.0; break;
    case FUDE_ZOOM_MODEL_BATTERY:   _p->value[0] = _has ? _v : 9.0; break;
    case FUDE_ZOOM_MODEL_ACSOURCE:  _p->value[0] = _has ? _v : 5.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 50.0; break;
    case FUDE_ZOOM_MODEL_CLOCK: {
        // Its frequency, its volts, and how long each pulse is high (none: half its period) — "50Hz 5V 1.5ms".
        f64 _pulse = 0.0;
        _p->value[0] = _has && _v > 0.0 ? _v : 1.0;
        _p->value[1] = _has2 ? _w : 5.0;
        _p->value[2] = fude_zoom_circuit_value(_text, 2u, &_pulse) && _pulse > 0.0 ? _pulse : 0.0;
        break;
    }
    case FUDE_ZOOM_MODEL_ISOURCE:   _p->value[0] = _has ? _v : 10e-3; break;
    case FUDE_ZOOM_MODEL_SERVO:
        // Which it is (its text naming it; else an SG90): its stall torque (the mechanisms' torque units), its speed at
        // 4.8 V (radians a second: 60° in 0.1 s), the current it draws held still.
        if(strstr(_text, "MG996") != NULL) {
            _p->value[0] = 500.0; _p->value[1] = FZC_PI / 3.0 / 0.17; _p->value[2] = 2.5;
        } else if(strstr(_text, "MG90") != NULL) {
            _p->value[0] = 120.0; _p->value[1] = FZC_PI / 3.0 / 0.1; _p->value[2] = 0.7;
        } else {
            _p->value[0] = 100.0; _p->value[1] = FZC_PI / 3.0 / 0.1; _p->value[2] = 0.65;
        }
        if(_fresh) {
            _p->state[0] = 90.0;   // (its horn: at the middle of its travel)
            _p->state[1] = -1.0;   // (no pulse yet: nowhere to go)
            _p->state[4] = -1.0;
            _p->state[7] = -1.0;   // (on no mechanism's shaft)
        }
        break;
    case FUDE_ZOOM_MODEL_LAMP:      _p->value[0] = _has && _v > 0.0 ? _v : 12.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 5.0; break;
    case FUDE_ZOOM_MODEL_MOTOR:     _p->value[0] = _has && _v > 0.0 ? _v : 6.0; _p->value[1] = _has2 && _w > 0.0 ? _w : 60.0; break;
    case FUDE_ZOOM_MODEL_FUSE:      _p->value[0] = _has && _v > 0.0 ? _v : 1.0; break;
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: _p->value[0] = fzc_led_volts(_text, _p->color, sizeof(_p->color)); break;
    case FUDE_ZOOM_MODEL_CHAR_LCD:
        // Its backlight: white on blue, or a yellow-green one's (its LEDs' forward voltage; its own 100 Ω behind them).
        snprintf(_p->color, sizeof(_p->color), "%s", fzc_says(_text, "blue") ? "blue" : "green");
        _p->value[0] = fzc_says(_text, "blue") ? 3.0 : 2.1;
        break;
    case FUDE_ZOOM_MODEL_PANEL_METER: {
        // Up to what it reads, in amperes or volts, its range's unit, its decimals (display.h).
        const fude_zoom_meter _m = fude_zoom_display_meter(_text);
        _p->value[0] = _m.full;
        _p->value[1] = _m.amps ? 1.0 : 0.0;
        _p->value[2] = _m.times;
        _p->value[3] = (f64)_m.decimals;
        break;
    }
    case FUDE_ZOOM_MODEL_LM3914:
        if(_fresh) {
            _p->state[1] = 0.01;   // (its outputs' current until its reference's is known)
        }
        break;
    default: break;
    }
    if(!_fresh) {
        return;
    }
    if(_p->part->model == FUDE_ZOOM_MODEL_SWITCH) {
        _p->switch_on = fzc_says(_text, "on") || fzc_says(_text, "closed") || fzc_says(_text, "1") ? 1u : 0u;
    } else if(_p->part->model == FUDE_ZOOM_MODEL_LOGIC_IN) {
        _p->switch_on = fzc_says(_text, "1") || fzc_says(_text, "high") || fzc_says(_text, "on") ? 1u : 0u;
    } else if(_p->part->model == FUDE_ZOOM_MODEL_BOARD) {
        // Its lines after its name: "D13 blink", "D9 high", "GP15 low".
        _p->drive = _p->drive_high = _p->drive_blink = 0u;
        const c8* _line = strchr(_text, '\n');
        while(_line != NULL) {
            _line++;
            c8 _name[24] = { 0 }, _mode[24] = { 0 };
            if(sscanf(_line, "%23s %23s", _name, _mode) == 2) {
                for(u32 _k = 0; _k < _p->part->pin_count && _k < FZC_PIN_MAX; _k++) {
                    if(fzc_same_name(_p->part->pins[_k].name, _name)) {
                        const u64 _bit = 1ull << _k;
                        if(fzc_says(_mode, "high") || fzc_says(_mode, "on")) { _p->drive |= _bit; _p->drive_high |= _bit; }
                        else if(fzc_says(_mode, "low") || fzc_says(_mode, "off")) { _p->drive |= _bit; }
                        else if(fzc_says(_mode, "blink")) { _p->drive |= _bit; _p->drive_blink |= _bit; }
                        break;
                    }
                }
            }
            _line = strchr(_line, '\n');
        }
    }
}

// A symbol's text (its numbers', all of them).
RDE_INTERNAL void fzc_text_of(const fude_zoom_scene* _s, u32 _object, c8* _out, usize _size) {
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    const u32 _k = fude_zoom_scene_shape_numbers_all(_s, _object, &_n);
    fude_zoom_symbol_text((const f64*)_n.memory, _k, _out, _size);
    rde_arr_free(&_n);
}

// An id kept in the build's refs (its bits as they are).
RDE_INTERNAL fude_zoom_id fzc_ref_id(const f64* _r) {
    fude_zoom_id _id;
    memcpy(&_id, _r, sizeof(_id));
    return _id;
}

// Is _p on wire _w's way (within _tol)?
RDE_INTERNAL b8 fzc_on_wire(const fude_zoom_circuit_wire* _w, fude_zoom_v2 _p, f64 _tol) {
    for(u32 _k = 0; _k + 1u < _w->count; _k++) {
        const fude_zoom_v2 _a = _w->points[_k], _b = _w->points[_k + 1u];
        const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _ll = _dx * _dx + _dy * _dy;
        const f64 _t = _ll > 0.0 ? fmin(fmax(((_p.x - _a.x) * _dx + (_p.y - _a.y) * _dy) / _ll, 0.0), 1.0) : 0.0;
        if(hypot(_a.x + _dx * _t - _p.x, _a.y + _dy * _t - _p.y) <= _tol) {
            return true;
        }
    }
    return false;
}

u32 fude_zoom_circuit_build(fude_zoom_circuit* _c, const fude_zoom_scene* _s) {
    return fude_zoom_circuit_build_in(_c, _s, NULL);
}

// A pin's lead's inner end (its part's own u, v): 45% of the way in from it toward its part's middle, along its side's
// way — where a gate's, a chip's, a block's drawn lead meets its body. False: none (a breadboard's hole).
RDE_INTERNAL b8 fzc_lead(const fude_zoom_part* _part, u32 _k, fude_zoom_pin* _out) {
    if(!fude_zoom_part_pin(_part, _k, _out) || _out->side == FUDE_ZOOM_PIN_ANY) {
        return false;
    }
    if(_out->side == FUDE_ZOOM_PIN_LEFT || _out->side == FUDE_ZOOM_PIN_RIGHT) {
        _out->u *= 0.55f;
    } else {
        _out->v *= 0.55f;
    }
    return true;
}

u32 fude_zoom_circuit_build_in(fude_zoom_circuit* _c, const fude_zoom_scene* _s, const u8* _scope) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // What it was (each part's state kept where it is still there).
    const u32 _was_n = (u32)rde_arr_length(&_c->parts);
    rde_arr TYPE(fude_zoom_circuit_part) _was_arr = rde_arr_new(sizeof(fude_zoom_circuit_part), _heap);
    rde_arr_add_arr(&_was_arr, &_c->parts);
    const fude_zoom_circuit_part* _was = (const fude_zoom_circuit_part*)_was_arr.memory;
    const u32 _was_w = (u32)rde_arr_length(&_c->wires);
    rde_arr TYPE(fude_zoom_circuit_wire) _was_wires_arr = rde_arr_new(sizeof(fude_zoom_circuit_wire), _heap);
    rde_arr_add_arr(&_was_wires_arr, &_c->wires);
    const fude_zoom_circuit_wire* _was_wires = (const fude_zoom_circuit_wire*)_was_wires_arr.memory;
    rde_arr TYPE(u8) _was_store = rde_arr_new(sizeof(u8), _heap);
    rde_arr_add_arr(&_was_store, &_c->store);
    rde_arr_clear(&_c->parts);
    rde_arr_clear(&_c->wires);
    rde_arr_clear(&_c->store);
    // Each part's limits attribute, found once (props.h: each names the object it is on).
    rde_arr_resize(&_c->limits_of, 0u);
    rde_arr_resize(&_c->limits_of, fude_zoom_scene_object_count(_s));
    u32* _limits_of = (u32*)_c->limits_of.memory;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        _limits_of[_i] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        if(fude_zoom_props_kind(_s, _i) == FUDE_ZOOM_PROPS_LIMITS) {
            const u32 _t = fude_zoom_props_target(_s, _i);
            if(_t != FUDE_ZOOM_NONE) {
                _limits_of[_t] = _i;
            }
        }
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_arr _refs = rde_arr_new(sizeof(f64), _heap);   // each wire's ends' (id, pin, id, pin)
    rde_arr _units = rde_arr_new(sizeof(f64), _heap);  // each part's grid
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    _c->unit = 0.0;
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        if(_o->channels == FUDE_ZOOM_SHAPE_SYMBOL) {
            const fude_zoom_part* _part = fude_zoom_part_of(_s, _i);
            if(_part == NULL || (_scope != NULL && !_scope[_i])) {
                continue;
            }
            fude_zoom_circuit_part _p;
            memset(&_p, 0, sizeof(_p));
            _p.object   = _i;
            _p.part     = _part;
            _p.branch   = FUDE_ZOOM_NONE;
            _p.internal = FUDE_ZOOM_NONE;
            b8 _fresh = true;
            const fude_zoom_circuit_part* _kept = NULL;
            for(u32 _k = 0; _k < _was_n; _k++) {
                if(_was[_k].object == _i && _was[_k].part == _part) {
                    _kept = &_was[_k];
                    memcpy(_p.state, _was[_k].state, sizeof(_p.state));
                    _p.switch_on   = _was[_k].switch_on;
                    _p.drive       = _was[_k].drive;
                    _p.drive_high  = _was[_k].drive_high;
                    _p.drive_blink = _was[_k].drive_blink;
                    _p.shown       = _was[_k].shown;
                    _p.heat        = _was[_k].heat;
                    _p.burnt       = _was[_k].burnt;
                    _p.worst       = _was[_k].worst;
                    _p.worst_seen  = _was[_k].worst_seen;
                    _p.told        = _was[_k].told;
                    _fresh = false;
                    break;
                }
            }
            fzc_text_of(_s, _i, _text, sizeof(_text));
            fzc_read(&_p, _text, _fresh);
            // What it holds beyond its state (a display's lights as seen, an LCD's controller): as it was, kept; else new.
            _p.store_size = fzc_store_size(_part);
            _p.store_at   = (u32)rde_arr_length(&_c->store);
            if(_p.store_size > 0u) {
                u8* _mine = (u8*)rde_arr_add_n(&_c->store, _p.store_size);
                if(_kept != NULL && _kept->store_size == _p.store_size && (usize)_kept->store_at + _p.store_size <= rde_arr_length(&_was_store)) {
                    memcpy(_mine, (const u8*)_was_store.memory + _kept->store_at, _p.store_size);
                } else {
                    memset(_mine, 0, _p.store_size);
                }
            }
            // Its limits: its own (its card's), else those of the real part its text names, else a typical one's.
            _p.limits = fude_zoom_limits_typical(_part, _text, _p.value);
            const u32 _lim = _i < (u32)rde_arr_length(&_c->limits_of) ? ((const u32*)_c->limits_of.memory)[_i] : FUDE_ZOOM_NONE;
            i32 _preset = -1;
            if(_lim != FUDE_ZOOM_NONE) {
                fude_zoom_props_limits(_s, _lim, _p.limits.most, &_preset);
            }
            _p.lit = FUDE_ZOOM_PROBE_LIT;
            if(_part->model == FUDE_ZOOM_MODEL_LOGIC_OUT) {
                const u32 _light = fude_zoom_props_find(_s, _i, FUDE_ZOOM_PROPS_LIGHT);
                if(_light != FUDE_ZOOM_NONE) {
                    fude_zoom_props_light(_s, _light, &_p.lit);
                }
            }
            rde_arr_add(&_c->parts, (any)&_p);
            // Its grid: a tenth of its pins' room as it came, in the home frame's units.
            f64 _n[3];
            fude_zoom_scene_shape_numbers(_s, _i, _n, 3u);
            const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of((u32)_n[0]);
            const f64 _k = fude_zoom_sim_scale(fude_zoom_scene_sim(_s, _o->frame, _home)) * _o->scale;
            const f64 _room = _info != NULL ? fude_zoom_part_room_h(_part, (f64)_info->h) : 0.0;
            f64 _unit = _room > 0.0 ? _n[2] * _k / (_room * 0.5) * 10.0 : _n[2] * _k * 0.5;
            if(_part->look == FZC_BREADBOARD) {
                _unit = 2.0 * _n[2] * _k / (f64)FZC_BB_H;   // (a hole apart)
            }
            rde_arr_add(&_units, (any)&_unit);
            _c->unit = _c->unit > 0.0 ? fmin(_c->unit, _unit) : _unit;
        } else if(_o->channels == FUDE_ZOOM_SHAPE_WIRE) {
            f64 _n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
            const u32 _cnt = fude_zoom_scene_shape_numbers(_s, _i, _n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
            fude_zoom_circuit_wire _w;
            memset(&_w, 0, sizeof(_w));
            fude_zoom_id _from, _to;
            i32 _fp, _tp;
            _w.count = fude_zoom_wire_of(_n, _cnt, _w.points, &_from, &_fp, &_to, &_tp);
            if(_w.count < 2u) {
                continue;
            }
            const fude_zoom_sim _to_home = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
            for(u32 _k = 0; _k < _w.count; _k++) {
                _w.points[_k] = fude_zoom_sim_apply(_to_home, _w.points[_k]);
                if(_k > 0u) {
                    _w.length += hypot(_w.points[_k].x - _w.points[_k - 1u].x, _w.points[_k].y - _w.points[_k - 1u].y);
                }
            }
            _w.object = _i;
            _w.node   = FUDE_ZOOM_NONE;
            for(u32 _k = 0; _k < _was_w; _k++) {
                if(_was_wires[_k].object == _i) {
                    memcpy(_w.phase, _was_wires[_k].phase, sizeof(_w.phase));
                }
            }
            rde_arr_add(&_c->wires, (any)&_w);
            f64 _r[4] = { 0.0, (f64)_fp, 0.0, (f64)_tp };   // (each id's bits as they are: fzc_ref_id)
            memcpy(&_r[0], &_from, sizeof(f64));
            memcpy(&_r[2], &_to, sizeof(f64));
            for(u32 _k = 0; _k < 4u; _k++) {
                rde_arr_add(&_refs, (any)&_r[_k]);
            }
        }
    }
    rde_arr_free(&_was_arr);
    rde_arr_free(&_was_wires_arr);
    rde_arr_free(&_was_store);
    if(_scope != NULL && rde_arr_length(&_c->wires) > 0u) {
        // Only what is played: its own wires, those joined to its parts, and those joined to such wires (an end on one,
        // or one's end on it).
        fude_zoom_circuit_wire* _ws = (fude_zoom_circuit_wire*)_c->wires.memory;
        f64* _rf = (f64*)_refs.memory;
        const u32 _nw0 = (u32)rde_arr_length(&_c->wires);
        const fude_zoom_circuit_part* _ps = (const fude_zoom_circuit_part*)_c->parts.memory;
        rde_arr TYPE(u8) _in_arr = rde_arr_new(sizeof(u8), _heap);
        rde_arr_resize(&_in_arr, _nw0);
        u8* _in = (u8*)_in_arr.memory;   // (sized once: it stays put)
        for(u32 _w = 0; _w < _nw0; _w++) {
            _in[_w] = _scope[_ws[_w].object] ? 1u : 0u;
            for(u32 _e = 0; _e < 2u && !_in[_w]; _e++) {
                const fude_zoom_id _id = fzc_ref_id(&_rf[4u * _w + 2u * _e]);
                const u32 _obj = fude_zoom_wire_part(_s, _id, (i32)_rf[4u * _w + 2u * _e + 1u], _home, _ws[_w].points[_e == 0u ? 0u : _ws[_w].count - 1u]);
                for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts) && _obj != FUDE_ZOOM_NONE; _i++) {
                    _in[_w] = _in[_w] || _ps[_i].object == _obj;
                }
            }
        }
        const f64 _tol = (_c->unit > 0.0 ? _c->unit : 1.0) * 0.5;
        for(b8 _more = true; _more;) {
            _more = false;
            for(u32 _w = 0; _w < _nw0; _w++) {
                for(u32 _v = 0; _v < _nw0 && !_in[_w]; _v++) {
                    if(!_in[_v]) {
                        continue;
                    }
                    const fude_zoom_v2 _we[2] = { _ws[_w].points[0], _ws[_w].points[_ws[_w].count - 1u] };
                    const fude_zoom_v2 _ve[2] = { _ws[_v].points[0], _ws[_v].points[_ws[_v].count - 1u] };
                    if(fzc_on_wire(&_ws[_v], _we[0], _tol) || fzc_on_wire(&_ws[_v], _we[1], _tol) || fzc_on_wire(&_ws[_w], _ve[0], _tol) ||
                       fzc_on_wire(&_ws[_w], _ve[1], _tol)) {
                        _in[_w] = 1u;
                        _more = true;
                    }
                }
            }
        }
        u32 _kept = 0;
        for(u32 _w = 0; _w < _nw0; _w++) {
            if(_in[_w]) {
                _ws[_kept] = _ws[_w];
                memmove(&_rf[4u * _kept], &_rf[4u * _w], 4u * sizeof(f64));
                _kept++;
            }
        }
        _c->wires.count = _kept;
        _refs.count     = 4u * _kept;
        rde_arr_free(&_in_arr);
    }
    fude_zoom_circuit_part* _parts = (fude_zoom_circuit_part*)_c->parts.memory;
    fude_zoom_circuit_wire* _wires = (fude_zoom_circuit_wire*)_c->wires.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts), _nw = (u32)rde_arr_length(&_c->wires);
    if(!(_c->unit > 0.0)) {
        _c->unit = 1.0;
    }
    // Terminals: each part's pins, each wire's two ends. A pin's lead too: from it in toward its part's middle (as its
    // drawing's lead goes: a gate's, a chip's stub) — a wire's end dropped on it is on the pin.
    rde_arr _terms = rde_arr_new(sizeof(fzc_term), _heap);
    rde_arr _first = rde_arr_new(sizeof(u32), _heap);   // each part's first terminal
    rde_arr TYPE(fude_zoom_v2) _leads = rde_arr_new(sizeof(fude_zoom_v2), _heap);   // each pin's lead's inner end
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _at = (u32)rde_arr_length(&_terms);
        rde_arr_add(&_first, (any)&_at);
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _parts[_i].object);
        const fude_zoom_sim _up = fude_zoom_scene_sim(_s, _o->frame, _home);
        f64 _hn[3] = { 0.0, 0.0, 0.0 };
        fude_zoom_scene_shape_numbers(_s, _parts[_i].object, _hn, 3u);
        const fude_zoom_sim _whole = fude_zoom_sim_compose(_up, fude_zoom_object_sim(_o));
        for(u32 _k = 0; _k < _parts[_i].part->pin_count; _k++) {
            fzc_term _t = { { 0.0, 0.0 }, ((const f64*)_units.memory)[_i], _i, _k, FUDE_ZOOM_NONE };
            fude_zoom_v2 _q, _in;
            fude_zoom_pin _pin;
            if(fude_zoom_part_pin_at(_s, _parts[_i].object, _k, &_q)) {
                _t.at = fude_zoom_sim_apply(_up, _q);
            }
            _in = _t.at;
            if(fzc_lead(_parts[_i].part, _k, &_pin)) {
                _in = fude_zoom_sim_apply(_whole, (fude_zoom_v2){ (f64)_pin.u * _hn[1], (f64)_pin.v * _hn[2] });
            }
            rde_arr_add(&_terms, (any)&_t);
            rde_arr_add(&_leads, (any)&_in);
        }
    }
    const u32 _wire_first = (u32)rde_arr_length(&_terms);
    for(u32 _w = 0; _w < _nw; _w++) {
        for(u32 _e = 0; _e < 2u; _e++) {
            const fzc_term _t = { _wires[_w].points[_e == 0u ? 0u : _wires[_w].count - 1u], _c->unit, FUDE_ZOOM_NONE, _e, _w };
            rde_arr_add(&_terms, (any)&_t);
        }
    }
    const u32 _nt = (u32)rde_arr_length(&_terms);
    fzc_term* _tm = (fzc_term*)_terms.memory;
    rde_arr TYPE(u32) _net_arr   = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u32) _touch_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_net_arr, _nt);
    rde_arr_resize(&_touch_arr, _nt);
    u32* _net   = (u32*)_net_arr.memory;     // joined: one node (sized once: they stay put)
    u32* _touch = (u32*)_touch_arr.memory;   // ...touching: one place (a vertex)
    for(u32 _i = 0; _i < _nt; _i++) {
        _net[_i] = _touch[_i] = _i;
    }
    rde_arr TYPE(u8) _held_arr = rde_arr_new(sizeof(u8), _heap);   // each wire end: something holds it (a pin, a wire)
    rde_arr_resize(&_held_arr, _nt);
    u8* _held = (u8*)_held_arr.memory;
    // A wire's ends to the pins they keep, and to each other.
    for(u32 _w = 0; _w < _nw; _w++) {
        const f64* _r = &((const f64*)_refs.memory)[4u * _w];
        fzc_join(_net, _wire_first + 2u * _w, _wire_first + 2u * _w + 1u);
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_id _id = fzc_ref_id(&_r[2u * _e]);
            const i32 _pin = (i32)_r[2u * _e + 1u];
            if(_id == 0u || _pin < 0) {
                continue;
            }
            const u32 _obj = fude_zoom_wire_part(_s, _id, _pin, _home, _wires[_w].points[_e == 0u ? 0u : _wires[_w].count - 1u]);
            for(u32 _i = 0; _i < _np && _obj != FUDE_ZOOM_NONE; _i++) {
                if(_parts[_i].object == _obj && (u32)_pin < _parts[_i].part->pin_count) {
                    const u32 _pt = ((const u32*)_first.memory)[_i] + (u32)_pin;
                    fzc_join(_net, _wire_first + 2u * _w + _e, _pt);
                    fzc_join(_touch, _wire_first + 2u * _w + _e, _pt);
                    _held[_wire_first + 2u * _w + _e] = 1u;
                }
            }
        }
    }
    // Terminals where each other is (sorted along x, each looking on only as far as it could touch).
    rde_arr TYPE(u32) _order_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_order_arr, _nt);
    u32* _order = (u32*)_order_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _order[_i] = _i;
    }
    for(u32 _i = 1; _i < _nt; _i++) {   // (insertion sort: a few hundred at most, mostly in order)
        const u32 _v = _order[_i];
        u32 _j = _i;
        while(_j > 0u && _tm[_order[_j - 1u]].at.x > _tm[_v].at.x) {
            _order[_j] = _order[_j - 1u];
            _j--;
        }
        _order[_j] = _v;
    }
    f64 _reach = 0.0;
    for(u32 _i = 0; _i < _nt; _i++) {
        _reach = fmax(_reach, 0.3 * _tm[_i].unit);
    }
    for(u32 _i = 0; _i < _nt; _i++) {
        const fzc_term* _a = &_tm[_order[_i]];
        for(u32 _j = _i + 1u; _j < _nt && _tm[_order[_j]].at.x - _a->at.x <= _reach; _j++) {
            const fzc_term* _b = &_tm[_order[_j]];
            if(_a->part != FUDE_ZOOM_NONE && _a->part == _b->part) {
                continue;   // (a part's own pins: never by being near)
            }
            if(hypot(_b->at.x - _a->at.x, _b->at.y - _a->at.y) <= 0.3 * fmin(_a->unit, _b->unit)) {
                fzc_join(_net, _order[_i], _order[_j]);
                fzc_join(_touch, _order[_i], _order[_j]);
                _held[_order[_i]] = _held[_order[_j]] = 1u;
            }
        }
    }
    rde_arr_free(&_order_arr);
    // A wire's end on another wire, along it: joined there (a junction: that wire's pieces meet at it).
    rde_arr_clear(&_c->cut_vertex);
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].pieces = 1u;
        _wires[_w].cut[0] = 0.0;
        _wires[_w].cut[1] = _wires[_w].length;
    }
    rde_arr _junction = rde_arr_new(sizeof(u32), _heap);   // (host wire, terminal) pairs, with the length along it
    rde_arr _along    = rde_arr_new(sizeof(f64), _heap);
    for(u32 _t = _wire_first; _t < _nt; _t++) {
        const fude_zoom_v2 _p = _tm[_t].at;
        const f64* _r = &((const f64*)_refs.memory)[4u * _tm[_t].wire];
        if(_r[2u * _tm[_t].pin] != 0.0 && _r[2u * _tm[_t].pin + 1u] >= 0.0) {
            continue;   // (an end at a pin: a wire passing over the pin is not joined to it)
        }
        for(u32 _w = 0; _w < _nw; _w++) {
            if(_w == _tm[_t].wire) {
                continue;
            }
            f64 _len = 0.0;
            for(u32 _k = 0; _k + 1u < _wires[_w].count; _k++) {
                const fude_zoom_v2 _a = _wires[_w].points[_k], _b = _wires[_w].points[_k + 1u];
                const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
                const f64 _ll = _d.x * _d.x + _d.y * _d.y, _l = sqrt(_ll);
                const f64 _u = _ll > 0.0 ? fmin(fmax(((_p.x - _a.x) * _d.x + (_p.y - _a.y) * _d.y) / _ll, 0.0), 1.0) : 0.0;
                const f64 _off = hypot(_a.x + _d.x * _u - _p.x, _a.y + _d.y * _u - _p.y);
                const f64 _at = _len + _u * _l;
                if(_off <= 0.3 * _c->unit && _at > 0.3 * _c->unit && _at < _wires[_w].length - 0.3 * _c->unit) {
                    fzc_join(_net, _t, _wire_first + 2u * _w);
                    _held[_t] = 1u;
                    const u32 _pair[2] = { _w, _t };
                    rde_arr_add(&_junction, (any)&_pair[0]);
                    rde_arr_add(&_junction, (any)&_pair[1]);
                    rde_arr_add(&_along, (any)&_at);
                    break;
                }
                _len += _l;
            }
        }
    }
    // A wire's end held by nothing yet, on a pin's lead: on the pin (it looks joined there: it is).
    const fude_zoom_v2* _lead = (const fude_zoom_v2*)_leads.memory;
    for(u32 _t = _wire_first; _t < _nt; _t++) {
        if(_held[_t]) {
            continue;
        }
        const fude_zoom_v2 _p = _tm[_t].at;
        for(u32 _q = 0; _q < _wire_first; _q++) {
            const fude_zoom_v2 _a = _tm[_q].at, _b = _lead[_q];
            const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
            const f64 _ll = _d.x * _d.x + _d.y * _d.y;
            if(!(_ll > 0.0)) {
                continue;
            }
            const f64 _u = fmin(fmax(((_p.x - _a.x) * _d.x + (_p.y - _a.y) * _d.y) / _ll, 0.0), 1.0);
            if(hypot(_a.x + _d.x * _u - _p.x, _a.y + _d.y * _u - _p.y) <= 0.3 * fmin(_tm[_q].unit, _tm[_t].unit)) {
                fzc_join(_net, _t, _q);
                fzc_join(_touch, _t, _q);
                _held[_t] = 1u;
                break;
            }
        }
    }
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].loose[0] = !_held[_wire_first + 2u * _w];
        _wires[_w].loose[1] = !_held[_wire_first + 2u * _w + 1u];
    }
    // Each wire's pieces: cut where other wires' ends meet it.
    const u32 _nj = (u32)rde_arr_length(&_along);
    for(u32 _j = 0; _j < _nj; _j++) {
        fude_zoom_circuit_wire* _w = &_wires[((const u32*)_junction.memory)[2u * _j]];
        const f64 _at = ((const f64*)_along.memory)[_j];
        if(_w->pieces >= FUDE_ZOOM_WIRE_PIECES) {
            continue;
        }
        u32 _k = _w->pieces;   // (sorted: in after the cuts below it)
        while(_k > 0u && _w->cut[_k - 1u] > _at) {
            _k--;
        }
        if(_k == 0u || fabs(_w->cut[_k - 1u] - _at) < 1e-9 * fmax(_w->length, 1.0)) {
            continue;
        }
        memmove(&_w->cut[_k + 1u], &_w->cut[_k], (usize)(_w->pieces + 1u - _k) * sizeof(f64));
        _w->cut[_k] = _at;
        _w->pieces++;
    }
    // Same-named pins inside a part (a board's GNDs, its 5Vs) are one; a breadboard's strips each one.
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        const u32 _f = ((const u32*)_first.memory)[_i];
        if(_part->look == FZC_BREADBOARD) {
            u32 _strip_at[256];
            for(u32 _k = 0; _k < 256u; _k++) {
                _strip_at[_k] = FUDE_ZOOM_NONE;
            }
            for(u32 _k = 0; _k < _part->pin_count; _k++) {
                const u32 _st = fude_zoom_breadboard_strip(_k) % 256u;
                if(_strip_at[_st] == FUDE_ZOOM_NONE) {
                    _strip_at[_st] = _f + _k;
                } else {
                    fzc_join(_net, _strip_at[_st], _f + _k);
                }
            }
            continue;
        }
        for(u32 _a = 0; _a < _part->pin_count; _a++) {
            if(_part->pins[_a].name[0] == 0) {
                continue;
            }
            for(u32 _b = _a + 1u; _b < _part->pin_count; _b++) {
                // (exactly the same name: a board's GNDs are one; a CD4511's a and A are not)
                if(strcmp(_part->pins[_a].name, _part->pins[_b].name) == 0) {
                    fzc_join(_net, _f + _a, _f + _b);
                }
            }
        }
    }
    // Ground: every ground part's, else a board's GND, else the first source's minus.
    u32 _ground = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _np; _i++) {
        if(_parts[_i].part->model == FUDE_ZOOM_MODEL_GROUND) {
            const u32 _t = ((const u32*)_first.memory)[_i];
            if(_ground == FUDE_ZOOM_NONE) {
                _ground = _t;
            } else {
                fzc_join(_net, _ground, _t);
            }
        }
    }
    _c->grounded = _ground != FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _np && _ground == FUDE_ZOOM_NONE; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        if(_part->model == FUDE_ZOOM_MODEL_BOARD) {
            for(u32 _k = 0; _k < _part->pin_count; _k++) {
                if(fzc_same_name(_part->pins[_k].name, "GND")) {
                    _ground = ((const u32*)_first.memory)[_i] + _k;
                    break;
                }
            }
        }
    }
    for(u32 _i = 0; _i < _np && _ground == FUDE_ZOOM_NONE; _i++) {
        const u8 _m = _parts[_i].part->model;
        if(_m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_ACSOURCE || _m == FUDE_ZOOM_MODEL_CLOCK || _m == FUDE_ZOOM_MODEL_ISOURCE) {
            _ground = ((const u32*)_first.memory)[_i] + 1u;
        }
    }
    // Nodes: what something works on (a part's pin with a model), ground 0.
    rde_arr TYPE(u32) _node_of_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_node_of_arr, _nt);
    u32* _node_of = (u32*)_node_of_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _node_of[_i] = FUDE_ZOOM_NONE;
    }
    const u32 _ground_root = _ground != FUDE_ZOOM_NONE ? fzc_root(_net, _ground) : FUDE_ZOOM_NONE;
    if(_ground_root != FUDE_ZOOM_NONE) {
        _node_of[_ground_root] = 0u;
    }
    // (a pin joined to nothing: no node — nothing flows through it, an op-amp's supply left open is its own)
    rde_arr TYPE(u32) _size_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_size_arr, _nt);
    u32* _size = (u32*)_size_arr.memory;
    for(u32 _i = 0; _i < _nt; _i++) {
        _size[fzc_root(_net, _i)]++;
    }
    u32 _nodes = 1u;
    for(u32 _i = 0; _i < _np; _i++) {
        const fude_zoom_part* _part = _parts[_i].part;
        const u32 _f = ((const u32*)_first.memory)[_i];
        for(u32 _k = 0; _k < FZC_PIN_MAX; _k++) {
            _parts[_i].node[_k] = FUDE_ZOOM_NONE;
        }
        if(_part->model == FUDE_ZOOM_MODEL_NONE) {
            continue;
        }
        for(u32 _k = 0; _k < _part->pin_count && _k < FZC_PIN_MAX; _k++) {
            const u32 _r = fzc_root(_net, _f + _k);
            if(_size[_r] < 2u && _r != _ground_root && _part->model != FUDE_ZOOM_MODEL_TIMER555) {
                continue;   // (a 555's control pin is its own divider's, left open)
            }
            if(_node_of[_r] == FUDE_ZOOM_NONE) {
                _node_of[_r] = _nodes++;
            }
            _parts[_i].node[_k] = _node_of[_r];
        }
    }
    _c->nodes = _nodes;
    rde_arr_resize(&_c->v, _nodes);   // (those still there as they were: where Newton starts)
    fzc_blocks(_c);
    for(u32 _w = 0; _w < _nw; _w++) {
        _wires[_w].node = _node_of[fzc_root(_net, _wire_first + 2u * _w)];
    }
    // Vertices (for the wires' currents): touching terminals one; a junction's cut is the touching end's.
    rde_arr TYPE(u32) _vertex_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_vertex_arr, _nt);
    u32* _vertex = (u32*)_vertex_arr.memory;
    u32 _nv = 0;
    for(u32 _i = 0; _i < _nt; _i++) {
        _vertex[_i] = FUDE_ZOOM_NONE;
    }
    for(u32 _i = 0; _i < _nt; _i++) {
        const u32 _r = fzc_root(_touch, _i);
        if(_vertex[_r] == FUDE_ZOOM_NONE) {
            _vertex[_r] = _nv++;
        }
        _vertex[_i] = _vertex[_r];
    }
    _c->vertices = _nv;
    rde_arr_clear(&_c->cut_vertex);
    u32* _cv = _nw > 0u ? (u32*)rde_arr_add_n(&_c->cut_vertex, _nw * (FUDE_ZOOM_WIRE_PIECES + 1u)) : NULL;   // (parts alone, no wire: none)
    for(u32 _w = 0; _w < _nw; _w++) {
        for(u32 _k = 0; _k <= FUDE_ZOOM_WIRE_PIECES; _k++) {
            _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k] = FUDE_ZOOM_NONE;
        }
        _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u)]                     = _vertex[_wire_first + 2u * _w];
        _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _wires[_w].pieces] = _vertex[_wire_first + 2u * _w + 1u];
    }
    for(u32 _j = 0; _j < _nj; _j++) {
        const u32 _w = ((const u32*)_junction.memory)[2u * _j], _t = ((const u32*)_junction.memory)[2u * _j + 1u];
        const f64 _at = ((const f64*)_along.memory)[_j];
        for(u32 _k = 1; _k < _wires[_w].pieces; _k++) {
            if(fabs(_wires[_w].cut[_k] - _at) < 1e-9 * fmax(_wires[_w].length, 1.0)) {
                _cv[_w * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k] = _vertex[_t];
            }
        }
    }
    rde_arr_clear(&_c->pin_vertex);
    u32* _pv = _np > 0u ? (u32*)rde_arr_add_n(&_c->pin_vertex, _np * FZC_PIN_MAX) : NULL;
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _f = ((const u32*)_first.memory)[_i];
        for(u32 _k = 0; _k < FZC_PIN_MAX; _k++) {
            _pv[_i * FZC_PIN_MAX + _k] = _k < _parts[_i].part->pin_count ? _vertex[_f + _k] : FUDE_ZOOM_NONE;
        }
    }
    // Its logic, made from what the parts are now (logic.h).
    fude_zoom_logic_build(_c->logic, _c);
    // Its step of time: a millisecond, finer for what changes faster (a 40th of a source's period); what it is now a jump.
    fude_zoom_circuit_jump(_c);
    _c->step = 1e-3;
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _parts[_i].part->model;
        if((_m == FUDE_ZOOM_MODEL_ACSOURCE && _parts[_i].value[1] > 0.0) || (_m == FUDE_ZOOM_MODEL_CLOCK && _parts[_i].value[0] > 0.0)) {
            const f64 _f = _m == FUDE_ZOOM_MODEL_ACSOURCE ? _parts[_i].value[1] : _parts[_i].value[0];
            _c->step = fmin(_c->step, fmax(1.0 / (40.0 * _f), 1e-6));
        }
    }
    rde_arr_free(&_net_arr);
    rde_arr_free(&_touch_arr);
    rde_arr_free(&_size_arr);
    rde_arr_free(&_node_of_arr);
    rde_arr_free(&_vertex_arr);
    rde_arr_free(&_terms);
    rde_arr_free(&_first);
    rde_arr_free(&_refs);
    rde_arr_free(&_units);
    rde_arr_free(&_junction);
    rde_arr_free(&_along);
    rde_arr_free(&_leads);
    rde_arr_free(&_held_arr);
    return _np;
}

void fude_zoom_circuit_reset(fude_zoom_circuit* _c) {
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        memset(_p[_i].state, 0, sizeof(_p[_i].state));
        _p[_i].shown = 0.0;
        _p[_i].switch_on = 0u;
        _p[_i].drive = _p[_i].drive_high = _p[_i].drive_blink = 0u;
    }
    rde_arr_clear(&_c->parts);   // (its states read from the parts' texts again at the next build)
    fude_zoom_logic_forget(_c->logic);   // (nor what its logic held carried over)
    memset(_c->v.memory, 0, rde_arr_length(&_c->v) * sizeof(f64));
    _c->time     = 0.0;
    _c->step_now = 0.0;
    _c->be_next  = true;
    _c->ok   = false;
}

// --- solved -----------------------------------------------------------------------------------------

typedef struct {
    fude_zoom_circuit* c;
    u32                n;      // unknowns (nodes less ground)
    b8                 tran;   // stepping in time (capacitors and inductors by their companions)
    f64                dt;
    f64                t;
    b8                 be;     // this step by backward Euler (after a jump), not the trapezoidal rule
} fzc_ctx;

RDE_INTERNAL u32 fzc_node(const fude_zoom_circuit_part* _p, u32 _pin) {
    return _pin == FZC_GROUND ? 0u : (_pin < FZC_PIN_MAX ? _p->node[_pin] : FUDE_ZOOM_NONE);
}

RDE_INTERNAL f64 fzc_volt(const fzc_ctx* _x, u32 _node) {
    return _node == FUDE_ZOOM_NONE || _node == 0u ? 0.0 : ((const f64*)_x->c->v.memory)[_node];
}

RDE_INTERNAL f64 fzc_pv(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _pin) {
    return fzc_volt(_x, fzc_node(_p, _pin));
}

RDE_INTERNAL void fzc_add_pin_i(fude_zoom_circuit_part* _p, u32 _pin, f64 _i) {
    if(_pin < FZC_PIN_MAX) {
        _p->pin_i[_pin] += _i;
    }
}

// The system's entry at node _row's row, node _col's column (both in one block: a part's pins always are; were they
// not, a place no one reads). While the systems are shaped (fzc_shape): each place a stamp writes noted, nothing
// written. After: a place its block's shape has not (a stamp that writes somewhere new — a chip's output turned on)
// noted too, and the system shaped again before it is solved.
RDE_INTERNAL f64* fzc_entry(fude_zoom_circuit* _c, u32 _row, u32 _col) {
    static f64 _nowhere;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32  _b     = _block[_row];
    if(_b == FUDE_ZOOM_NONE || _block[_col] != _b) {
        return &_nowhere;
    }
    fude_sim_sparse* _sys   = &((fude_sim_sparse*)_c->block_system.memory)[_b];
    const u32*       _slot  = (const u32*)_c->node_slot.memory;
    const u32        _first = ((const u32*)_c->block_first.memory)[_b];
    const u32 _e = _c->recording ? FUDE_SIM_SPARSE_NONE : fude_sim_sparse_find(_sys, _slot[_row] - _first, _slot[_col] - _first);
    if(_e == FUDE_SIM_SPARSE_NONE) {
        const u64 _join = ((u64)_row << 32) | _col;
        rde_arr_add(&_c->joins, (any)&_join);
        _c->reshape = _c->reshape || !_c->recording;
        return &_nowhere;
    }
    return &((f64*)_sys->val.memory)[_e];
}

// A conductance _g between pins _a and _b.
RDE_INTERNAL void fzc_g(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _g) {
    const u32 _a = fzc_node(_p, _pa), _b = fzc_node(_p, _pb);
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE) {
        return;
    }
    const f64 _i = _g * (fzc_volt(_x, _a) - fzc_volt(_x, _b));
    fzc_add_pin_i(_p, _pa, _i);
    fzc_add_pin_i(_p, _pb, -_i);
    fude_zoom_circuit* _c = _x->c;
    if(_a > 0u) { *fzc_entry(_c, _a, _a) += _g; }
    if(_b > 0u) { *fzc_entry(_c, _b, _b) += _g; }
    if(_a > 0u && _b > 0u) {
        *fzc_entry(_c, _a, _b) -= _g;
        *fzc_entry(_c, _b, _a) -= _g;
    }
}

// A current _i from pin _a through the part to pin _b.
RDE_INTERNAL void fzc_i(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _i) {
    const u32 _a = fzc_node(_p, _pa), _b = fzc_node(_p, _pb);
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE) {
        return;
    }
    fzc_add_pin_i(_p, _pa, _i);
    fzc_add_pin_i(_p, _pb, -_i);
    f64* _rhs = (f64*)_x->c->rhs.memory;
    const u32* _slot = (const u32*)_x->c->node_slot.memory;
    if(_a > 0u) { _rhs[_slot[_a]] -= _i; }
    if(_b > 0u) { _rhs[_slot[_b]] += _i; }
}

// A voltage source _v (pin _a over pin _b) behind _r.
RDE_INTERNAL void fzc_vsrc(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _v, f64 _r) {
    fzc_g(_x, _p, _pa, _pb, 1.0 / _r);
    fzc_i(_x, _p, _pb, _pa, _v / _r);
}

// What is not linear: the currents into the part at pins _pins (_k of them) as _fn says from their voltages, and
// how they change (worked out a little either way), made linear where they are now.
typedef void (*fzc_fn)(const f64* _v, f64* _i, const void* _user);

RDE_INTERNAL void fzc_nonlinear(fzc_ctx* _x, fude_zoom_circuit_part* _p, const u32* _pins, u32 _k, fzc_fn _fn, const void* _user) {
    f64 _v[8], _i0[8], _ih[8], _j[8][8];
    u32 _node[8];
    for(u32 _t = 0; _t < _k; _t++) {
        _node[_t] = fzc_node(_p, _pins[_t]);
        if(_node[_t] == FUDE_ZOOM_NONE) {
            return;
        }
        _v[_t] = fzc_volt(_x, _node[_t]);
    }
    _fn(_v, _i0, _user);
    for(u32 _u = 0; _u < _k; _u++) {
        const f64 _h = 1e-7 * fmax(1.0, fabs(_v[_u]));
        f64 _vh[8];
        memcpy(_vh, _v, sizeof(f64) * _k);
        _vh[_u] += _h;
        _fn(_vh, _ih, _user);
        for(u32 _t = 0; _t < _k; _t++) {
            _j[_t][_u] = (_ih[_t] - _i0[_t]) / _h;
        }
    }
    f64* _rhs = (f64*)_x->c->rhs.memory;
    const u32* _slot = (const u32*)_x->c->node_slot.memory;
    for(u32 _t = 0; _t < _k; _t++) {
        fzc_add_pin_i(_p, _pins[_t], _i0[_t]);
        if(_node[_t] == 0u) {
            continue;
        }
        f64 _lin = _i0[_t];
        for(u32 _u = 0; _u < _k; _u++) {
            _lin -= _j[_t][_u] * _v[_u];
            if(_node[_u] != 0u) {
                *fzc_entry(_x->c, _node[_t], _node[_u]) += _j[_t][_u];
            }
        }
        _rhs[_slot[_node[_t]]] -= _lin;
    }
}

// e^x, carried on straight past e^40 (Newton's steps stay finite).
RDE_INTERNAL f64 fzc_exp(f64 _x) {
    return _x > 40.0 ? exp(40.0) * (1.0 + _x - 40.0) : exp(_x);
}

// A diode: is·(e^(v/nvt) − 1) — its exponent counted from _shift volts (an LED's: its forward voltage, its is 10 mA
// there), so that it is as steep as it is at what it conducts at however high that is (fzc_exp straightens past e^40:
// from 0 V, that is 2 V for an LED's nvt — a blue one's 3 V would barely conduct).
typedef struct { f64 is, nvt, zener, shift; } fzc_diode_k;

RDE_INTERNAL void fzc_diode_fn(const f64* _v, f64* _i, const void* _user) {
    const fzc_diode_k* _k = (const fzc_diode_k*)_user;
    const f64 _vd = _v[0] - _v[1];
    // (straightened past a kiloampere, not only past e^40: an LED's exponent counted from its forward voltage would
    // otherwise reach 10^16 A a volt past it — conductances no solve keeps its digits through)
    const f64 _x = (_vd - _k->shift) / _k->nvt, _top = fmin(40.0, log(1e3 / _k->is));
    const f64 _e = _x > _top ? exp(_top) * (1.0 + _x - _top) : exp(_x);
    f64 _id = _k->is * (_e - exp(-_k->shift / _k->nvt)) + 1e-12 * _vd;
    if(_k->zener > 0.0) {
        _id -= 1e-14 * (fzc_exp((-_vd - _k->zener) / FZC_VT) - 1.0);   // (its breakdown, the other way)
    }
    _i[0] = _id;
    _i[1] = -_id;
}

// An LED behind a resistor of its own (an LCD's backlight): the current through both, the LED's drop as it is at it
// (halved till it is: what goes through is never more than all the voltage over the resistor).
typedef struct { f64 vf, r; } fzc_led_r_k;

RDE_INTERNAL void fzc_led_r_fn(const f64* _v, f64* _i, const void* _user) {
    const fzc_led_r_k* _k = (const fzc_led_r_k*)_user;
    const fzc_diode_k _d = { 0.01, 2.0 * FZC_VT, 0.0, _k->vf };
    const f64 _vd = _v[0] - _v[1];
    f64 _lo = 0.0, _hi = fmax(_vd / _k->r, 0.0), _at[2] = { 0.0, 0.0 }, _led[2];
    for(u32 _n = 0; _n < 60u && _hi > 0.0; _n++) {
        const f64 _mid = 0.5 * (_lo + _hi);
        _at[0] = _vd - _mid * _k->r;
        fzc_diode_fn(_at, _led, &_d);
        if(_led[0] > _mid) {
            _lo = _mid;
        } else {
            _hi = _mid;
        }
    }
    const f64 _id = 0.5 * (_lo + _hi) + 1e-12 * _vd;
    _i[0] = _id;
    _i[1] = -_id;
}

// An output that sinks a current (an LM3914's: *_user amperes), less as its pin nears what it sinks into.
RDE_INTERNAL void fzc_sink_fn(const f64* _v, f64* _i, const void* _user) {
    const f64 _id = *(const f64*)_user * tanh(fmax(_v[0] - _v[1], 0.0) / 0.15) + 1e-9 * (_v[0] - _v[1]);
    _i[0] = _id;
    _i[1] = -_id;
}

// An LM3914's LED _k's pin (LED1 its pin 1; LED2–LED10 its pins 18 down to 10).
RDE_INTERNAL u32 fzc_3914_led(u32 _k) {
    return _k == 0u ? 0u : 18u - _k;
}

RDE_INTERNAL void fzc_bjt_fn(const f64* _v, f64* _i, const void* _user) {
    // Pins B, C, E; an NPN's (a PNP's: everything the other way, its user says -1).
    const f64 _s = *(const f64*)_user;
    const f64 _is = 1e-14, _bf = 100.0, _br = 1.0;
    const f64 _vbe = _s * (_v[0] - _v[2]), _vbc = _s * (_v[0] - _v[1]);
    const f64 _if = _is * (fzc_exp(_vbe / FZC_VT) - 1.0), _ir = _is * (fzc_exp(_vbc / FZC_VT) - 1.0);
    const f64 _ic = _if - _ir * (1.0 + _br) / _br, _ib = _if / _bf + _ir / _br;
    _i[0] = _s * _ib + 1e-12 * (_v[0] - _v[2]);
    _i[1] = _s * _ic;
    _i[2] = -_s * (_ib + _ic) - 1e-12 * (_v[0] - _v[2]);
}

RDE_INTERNAL void fzc_mos_fn(const f64* _v, f64* _i, const void* _user) {
    // Pins G, D, S; an N-channel's (a P's: the other way).
    const f64 _s = *(const f64*)_user;
    const f64 _vth = 2.0, _k = 1.0, _lambda = 0.01;
    f64 _vgs = _s * (_v[0] - _v[2]), _vds = _s * (_v[1] - _v[2]);
    f64 _sign = 1.0;
    if(_vds < 0.0) {   // (the other way through: its drain and source as each other)
        _vgs = _s * (_v[0] - _v[1]);
        _vds = -_vds;
        _sign = -1.0;
    }
    const f64 _ov = _vgs - _vth;
    f64 _id = 0.0;
    if(_ov > 0.0) {
        _id = _vds < _ov ? _k * (_ov * _vds - 0.5 * _vds * _vds) * (1.0 + _lambda * _vds) : 0.5 * _k * _ov * _ov * (1.0 + _lambda * _vds);
    }
    _id = _s * _sign * _id + 1e-9 * (_v[1] - _v[2]);
    _i[0] = 0.0;
    _i[1] = _id;
    _i[2] = -_id;
}

typedef struct { f64 vp, vn; } fzc_opamp_k;

RDE_INTERNAL void fzc_opamp_fn(const f64* _v, f64* _i, const void* _user) {
    // Pins IN−, IN+, OUT: its output a source behind 1 Ω, as high as the difference × 10⁵, between its rails.
    const fzc_opamp_k* _k = (const fzc_opamp_k*)_user;
    const f64 _mid = 0.5 * (_k->vp + _k->vn), _half = fmax(0.5 * (_k->vp - _k->vn) - 0.05, 0.01);
    const f64 _want = _mid + _half * tanh(1e5 * (_v[1] - _v[0]) / _half);
    const f64 _io = (_v[2] - _want) / 1.0;
    _i[0] = 1e-12 * _v[0];
    _i[1] = 1e-12 * _v[1];
    _i[2] = _io;
}

// A pin's voltage over another's.
RDE_INTERNAL f64 fzc_over(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _a, u32 _b) {
    return fzc_pv(_x, _p, _a) - fzc_pv(_x, _p, _b);
}

// A pin's node joined to nothing else (its supply left open: the op-amp's own rails then).
RDE_INTERNAL b8 fzc_open(const fude_zoom_circuit_part* _p, u32 _pin) {
    return fzc_node(_p, _pin) == FUDE_ZOOM_NONE;
}

// A board's supply pins: its volts for a pin's name (0: not one).
RDE_INTERNAL f64 fzc_supply(const c8* _name, f64 _logic) {
    RDE_UNUSED(_logic);
    if(fzc_same_name(_name, "5V") || fzc_same_name(_name, "VBUS") || fzc_same_name(_name, "VSYS") || fzc_same_name(_name, "IOREF")) {
        return 5.0;
    }
    if(fzc_same_name(_name, "3V3") || fzc_same_name(_name, "3.3V")) {
        return 3.3;
    }
    return 0.0;
}

RDE_INTERNAL u32 fzc_pin_named(const fude_zoom_part* _part, const c8* _name) {
    for(u32 _k = 0; _k < _part->pin_count; _k++) {
        if(fzc_same_name(_part->pins[_k].name, _name)) {
            return _k;
        }
    }
    return FUDE_ZOOM_NONE;
}

// A logic part (logic.h): where its pins meet the analog circuit, its bridges — an output a source of its level
// (its supply's high over its ground, through its output's resistance; none while it is released, unknown, or
// unpowered), an input a little leak to its ground; its supply's own draw.
RDE_INTERNAL void fzc_logic_stamp(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_logic* _l = _x->c->logic;
    const u32 _index = (u32)(_p - (fude_zoom_circuit_part*)_x->c->parts.memory);
    u32 _first;
    const u32 _n = fude_zoom_logic_bridges_of(_l, _index, &_first);
    const fude_zoom_bridge* _b = &((const fude_zoom_bridge*)_l->bridges.memory)[_first];
    u32 _supply = FUDE_ZOOM_NONE, _ground = FUDE_ZOOM_NONE;
    for(u32 _k = 0; _k < _n; _k++) {
        const u32 _gp = _b[_k].ground != FUDE_ZOOM_NONE ? _b[_k].ground : FZC_GROUND;
        const f64 _high = _b[_k].supply != FUDE_ZOOM_NONE ? fzc_over(_x, _p, _b[_k].supply, _gp) : FZC_LOGIC_V;
        if(_b[_k].way != FUDE_ZOOM_BRIDGE_IN && (_b[_k].drive == FUDE_SIM_0 || _b[_k].drive == FUDE_SIM_1) && _high > 1.0) {
            fzc_vsrc(_x, _p, _b[_k].pin, _gp, _b[_k].drive == FUDE_SIM_1 ? _high : 0.0, FZC_LOGIC_R);
        }
        if(_b[_k].way != FUDE_ZOOM_BRIDGE_OUT) {
            fzc_g(_x, _p, _b[_k].pin, _gp, _b[_k].supply != FUDE_ZOOM_NONE ? 1e-9 : 1e-6);
        }
        _supply = _b[_k].supply;
        _ground = _b[_k].ground;
    }
    if(_p->part->model == FUDE_ZOOM_MODEL_SIM && _n == 0u) {
        // (no bridge to say them: its own)
        for(u32 _k = 0; _k < _p->part->pin_count && _k < FZC_PIN_MAX; _k++) {
            const c8* _name = _p->part->pins[_k].name;
            if(_name != NULL && strcmp(_name, "VCC") == 0) { _supply = _k; }
            if(_name != NULL && strcmp(_name, "GND") == 0) { _ground = _k; }
        }
    }
    if(_supply != FUDE_ZOOM_NONE && _ground != FUDE_ZOOM_NONE) {
        fzc_g(_x, _p, _supply, _ground, 1e-5);   // (what a chip draws)
    }
}

// A clock's part of each period high (its pulse over its period; none given: half).
RDE_INTERNAL f64 fzc_duty(const fude_zoom_circuit_part* _p) {
    return _p->value[2] > 0.0 && _p->value[0] > 0.0 ? fmin(fmax(_p->value[2] * _p->value[0], 0.0), 1.0) : 0.5;
}

// Where in its period a clock is at _t (0 ..1): a whisker more, so that a step landing on an edge (its time a sum of
// steps, a rounding short of it) is past it — as the clock's level and its next edge both take it.
RDE_INTERNAL f64 fzc_clock_phase(const fude_zoom_circuit_part* _p, f64 _t) {
    return fmod(_t * _p->value[0] + 1e-9, 1.0);
}

RDE_INTERNAL void fzc_stamp(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    const f64 _t = _x->t;
    if(_p->burnt) {
        fzc_g(_x, _p, 0, 1, 1e-9);   // (burnt: open, all but a leak)
        return;
    }
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_RESISTOR:
        fzc_g(_x, _p, 0, 1, 1.0 / _p->value[0]);
        break;
    case FUDE_ZOOM_MODEL_POT: {
        const f64 _f = fmin(fmax(_p->state[0], 0.0), 1.0);
        fzc_g(_x, _p, 0, 2, 1.0 / fmax(_p->value[0] * _f, 1.0));
        fzc_g(_x, _p, 2, 1, 1.0 / fmax(_p->value[0] * (1.0 - _f), 1.0));
        break;
    }
    case FUDE_ZOOM_MODEL_CAPACITOR:
        // Its companion for the step (state[0] its voltage, state[1] its current, as last stepped): the trapezoidal
        // rule's — 2C/h, and the last current carried — or backward Euler's (C/h).
        if(_x->tran) {
            const f64 _g = (_x->be ? 1.0 : 2.0) * _p->value[0] / _x->dt;
            fzc_g(_x, _p, 0, 1, _g);
            fzc_i(_x, _p, 0, 1, -_g * _p->state[0] - (_x->be ? 0.0 : _p->state[1]));
        }
        break;
    case FUDE_ZOOM_MODEL_INDUCTOR:
        // (state[0] its current, state[1] its voltage: the trapezoidal rule's h/2L and the last voltage carried, or h/L)
        if(_x->tran) {
            const f64 _g = _x->dt / ((_x->be ? 1.0 : 2.0) * _p->value[0]);
            fzc_g(_x, _p, 0, 1, _g);
            fzc_i(_x, _p, 0, 1, _p->state[0] + (_x->be ? 0.0 : _g * _p->state[1]));
        } else {
            fzc_g(_x, _p, 0, 1, 1e3);
        }
        break;
    case FUDE_ZOOM_MODEL_DIODE:
    case FUDE_ZOOM_MODEL_ZENER:
    case FUDE_ZOOM_MODEL_LED: {
        fzc_diode_k _k = { 1e-14, FZC_VT, 0.0, 0.0 };
        if(_part->model == FUDE_ZOOM_MODEL_LED) {
            _k.nvt   = 2.0 * FZC_VT;
            _k.is    = 0.01;   // (10 mA at its forward voltage)
            _k.shift = _p->value[0];
        } else if(_part->model == FUDE_ZOOM_MODEL_ZENER) {
            _k.zener = _p->value[0];
        }
        const u32 _pins[2] = { 0u, 1u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP: {
        const f64 _s = _part->model == FUDE_ZOOM_MODEL_NPN ? 1.0 : -1.0;
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_bjt_fn, &_s);
        break;
    }
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS: {
        const f64 _s = _part->model == FUDE_ZOOM_MODEL_NMOS ? 1.0 : -1.0;
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_mos_fn, &_s);
        break;
    }
    case FUDE_ZOOM_MODEL_VSOURCE:  fzc_vsrc(_x, _p, 0, 1, _p->value[0], 1e-3); break;
    case FUDE_ZOOM_MODEL_BATTERY:  fzc_vsrc(_x, _p, 0, 1, _p->value[0], 0.5); break;
    case FUDE_ZOOM_MODEL_ACSOURCE: fzc_vsrc(_x, _p, 0, 1, _p->value[0] * sin(2.0 * FZC_PI * _p->value[1] * _t), 1e-3); break;
    case FUDE_ZOOM_MODEL_CLOCK:    fzc_vsrc(_x, _p, 0, 1, fzc_clock_phase(_p, _t) < fzc_duty(_p) ? _p->value[1] : 0.0, 1e-3); break;
    case FUDE_ZOOM_MODEL_ISOURCE:  fzc_i(_x, _p, 1, 0, _p->value[0]); break;
    case FUDE_ZOOM_MODEL_RAIL:     fzc_vsrc(_x, _p, 0, FZC_GROUND, _p->value[0], 1e-3); break;
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:   fzc_g(_x, _p, 0, 1, (_p->switch_on & 1u) || _p->pushed ? 100.0 : 1e-9); break;
    case FUDE_ZOOM_MODEL_LAMP:     fzc_g(_x, _p, 0, 1, _p->value[1] / (_p->value[0] * _p->value[0])); break;
    case FUDE_ZOOM_MODEL_MOTOR:
        // (its shaft turning something of a mechanism: its winding behind its back-EMF; else a load, as ever)
        if(_p->shafted) {
            fzc_vsrc(_x, _p, 0, 1, fude_zoom_motor_k(_p) * _p->spin, FUDE_ZOOM_MOTOR_R);
        } else {
            fzc_g(_x, _p, 0, 1, 1.0 / FUDE_ZOOM_MOTOR_R);
        }
        break;
    case FUDE_ZOOM_MODEL_BUZZER:   fzc_g(_x, _p, 0, 1, 1.0 / 100.0); break;
    case FUDE_ZOOM_MODEL_SERVO:
        // GND, VCC, SIG: its signal an input (1 MΩ); what it draws from its supply as it last stepped (state[6]).
        fzc_g(_x, _p, 2, 0, 1e-6);
        fzc_g(_x, _p, 1, 0, 1e-6);
        fzc_i(_x, _p, 1, 0, _p->state[6]);
        break;
    case FUDE_ZOOM_MODEL_FUSE:     fzc_g(_x, _p, 0, 1, _p->state[0] > 0.5 ? 1e-9 : 20.0); break;
    case FUDE_ZOOM_MODEL_VOLTMETER: fzc_g(_x, _p, 0, 1, 1e-7); break;
    case FUDE_ZOOM_MODEL_AMMETER:  fzc_g(_x, _p, 0, 1, 1e3); break;
    case FUDE_ZOOM_MODEL_OPAMP: {
        fzc_opamp_k _k = { fzc_open(_p, 3) ? 12.0 : fzc_pv(_x, _p, 3), fzc_open(_p, 4) ? -12.0 : fzc_pv(_x, _p, 4) };
        if(_k.vp <= _k.vn) {
            _k.vp = _k.vn + 0.1;
        }
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_opamp_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: {
        // An LED from each anode it has to its cathode (display.h), as bright a colour's forward voltage as its text says.
        const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
        const u32 _n = fude_zoom_display_lights(_part);
        for(u32 _l = 0; _l < _n; _l++) {
            u32 _pins[2];
            if(fude_zoom_display_led(_part, _l, &_pins[0], &_pins[1])) {
                fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_LM3914: {
        // LED1 0, V− 1, V+ 2, RLO 3, SIG 4, RHI 5, REF OUT 6, REF ADJ 7, MODE 8, LED10 9 … LED2 17: what it draws, its
        // reference (1.25 V, REF OUT over REF ADJ), its divider (ten 1 kΩ from RLO to RHI), its inputs; each output its
        // comparator lit (last step: state[0]'s bits) sinking ten times what REF OUT gives (state[1]).
        fzc_g(_x, _p, 2, 1, 1.0 / 2400.0);
        fzc_vsrc(_x, _p, 6, 7, 1.25, 0.2);
        fzc_g(_x, _p, 3, 5, 1.0 / 10000.0);
        fzc_g(_x, _p, 4, 1, 1e-9);
        fzc_g(_x, _p, 8, 1, 1e-9);
        const u32 _lit = (u32)_p->state[0];
        for(u32 _k = 0; _k < 10u; _k++) {
            const u32 _pins[2] = { fzc_3914_led(_k), 1u };
            if(_lit & (1u << _k)) {
                fzc_nonlinear(_x, _p, _pins, 2u, fzc_sink_fn, &_p->state[1]);
            } else {
                fzc_g(_x, _p, _pins[0], 1, 1e-9);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_PANEL_METER:
        // An ammeter's shunt (200 mV across it at its full scale), or a voltmeter's 10 MΩ.
        fzc_g(_x, _p, 0, 1, _p->value[1] > 0.5 ? _p->value[0] / 0.2 : 1e-7);
        break;
    case FUDE_ZOOM_MODEL_CHAR_LCD: {
        // VSS 0, VDD 1, V0 2, RS 3, RW 4, E 5, D0–D7 6–13, A 14, K 15: what it draws (1.5 mA), its inputs (a little each),
        // its backlight (its LEDs behind its own 100 Ω).
        fzc_g(_x, _p, 1, 0, 1.0 / 3300.0);
        for(u32 _k = 2; _k < 14u; _k++) {
            fzc_g(_x, _p, _k, 0, 1e-9);
        }
        const fzc_led_r_k _k = { _p->value[0], 100.0 };
        const u32 _pins[2] = { 14u, 15u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_led_r_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SEVEN_SEG: {
        fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, 1.8 };
        for(u32 _s = 0; _s < 8u; _s++) {
            const u32 _pins[2] = { _s, 8u };
            fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_LOGIC_IN:
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
    case FUDE_ZOOM_MODEL_SIM:
        fzc_logic_stamp(_x, _p);
        break;
    case FUDE_ZOOM_MODEL_TIMER555: {
        // GND 0, TRIG 1, OUT 2, RESET 3, CTRL 4, THRES 5, DISCH 6, VCC 7.
        fzc_g(_x, _p, 7, 4, 1.0 / 5000.0);
        fzc_g(_x, _p, 4, 0, 1.0 / 10000.0);
        const f64 _vcc = fmax(fzc_over(_x, _p, 7, 0), 0.0);
        fzc_vsrc(_x, _p, 2, 0, _p->state[0] > 0.5 ? fmax(_vcc - 1.5, 0.0) : 0.0, 10.0);
        fzc_g(_x, _p, 6, 0, _p->state[0] > 0.5 ? 1e-9 : 0.1);
        fzc_g(_x, _p, 7, 0, 1.0 / 1e5);   // (what it draws)
        for(u32 _k = 1; _k < 6u; _k += 2u) {
            fzc_g(_x, _p, _k, 0, 1e-9);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_REGULATOR: {
        // IN 0, GND 1, OUT 2: as high as it is made for, while its input is over that by its dropout.
        const f64 _want = fmax(fmin((f64)_part->a, fzc_over(_x, _p, 0, 1) - (f64)_part->b), 0.0);
        fzc_vsrc(_x, _p, 2, 1, _want, 0.05);
        fzc_i(_x, _p, 0, 1, fmax(_p->state[1], 0.0));   // (what goes out comes in)
        fzc_g(_x, _p, 0, 1, 1e-4);
        break;
    }
    case FUDE_ZOOM_MODEL_SHIFT595: {
        // QA 14, QB–QH 0–6, GND 7, QH' 8, SRCLR 9, SRCLK 10, RCLK 11, OE 12, SER 13, VCC 15.
        const f64 _pow = fzc_over(_x, _p, 15, 7);
        fzc_g(_x, _p, 15, 7, 1e-5);
        const u32 _latch = (u32)_p->state[1], _reg = (u32)_p->state[0];
        const b8 _on = _pow > 1.0 && fzc_over(_x, _p, 12, 7) < _pow * 0.5;
        static const u32 _q[8] = { 14, 0, 1, 2, 3, 4, 5, 6 };
        for(u32 _k = 0; _k < 8u; _k++) {
            if(_on) {
                fzc_vsrc(_x, _p, _q[_k], 7, (_latch >> _k) & 1u ? _pow : 0.0, FZC_LOGIC_R);
            }
        }
        if(_pow > 1.0) {
            fzc_vsrc(_x, _p, 8, 7, (_reg >> 7) & 1u ? _pow : 0.0, FZC_LOGIC_R);
        }
        for(u32 _k = 9; _k <= 13u; _k++) {
            fzc_g(_x, _p, _k, 7, 1e-9);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_DRIVER293: {
        // EN1,2 0, 1A 1, 1Y 2, GND 3, 2Y 5, 2A 6, VCC2 7, EN3,4 8, 3A 9, 3Y 10, 4Y 13, 4A 14, VCC1 15.
        const f64 _vm = fzc_over(_x, _p, 7, 3);
        static const u32 _ch[4][3] = { { 1, 2, 0 }, { 6, 5, 0 }, { 9, 10, 8 }, { 14, 13, 8 } };
        const u32 _bits = (u32)_p->state[0];
        for(u32 _k = 0; _k < 4u; _k++) {
            if((_bits >> (4u + _k)) & 1u) {   // (enabled)
                fzc_vsrc(_x, _p, _ch[_k][1], 3, (_bits >> _k) & 1u ? fmax(_vm - 1.4, 0.0) : 0.0, 1.0);
            }
            fzc_g(_x, _p, _ch[_k][0], 3, 1e-9);
        }
        fzc_g(_x, _p, 15, 3, 1e-4);
        break;
    }
    case FUDE_ZOOM_MODEL_ULN2003: {
        const u32 _bits = (u32)_p->state[0];
        for(u32 _k = 0; _k < 7u; _k++) {
            fzc_g(_x, _p, 15u - _k, 7, (_bits >> _k) & 1u ? 1.0 / 2.0 : 1e-9);
            fzc_g(_x, _p, _k, 7, 1.0 / 2700.0);   // (its input's resistor)
        }
        break;
    }
    case FUDE_ZOOM_MODEL_RELAY: {
        // VCC 0, GND 1, IN 2, COM 3, NO 4, NC 5.
        const b8 _on = _p->state[0] > 0.5;
        fzc_g(_x, _p, 0, 1, _on ? 1.0 / 70.0 : 1e-6);
        fzc_g(_x, _p, 2, 1, 1e-5);
        fzc_g(_x, _p, 3, 4, _on ? 20.0 : 1e-9);
        fzc_g(_x, _p, 3, 5, _on ? 1e-9 : 20.0);
        break;
    }
    case FUDE_ZOOM_MODEL_BOARD: {
        const u32 _gnd = fzc_pin_named(_part, "GND");
        if(_gnd == FUDE_ZOOM_NONE) {
            break;
        }
        for(u32 _k = 0; _k < _part->pin_count && _k < FZC_PIN_MAX; _k++) {
            const f64 _sv = fzc_supply(_part->pins[_k].name, (f64)_part->a);
            if(_sv > 0.0) {
                fzc_vsrc(_x, _p, _k, _gnd, _sv, 0.02);
                continue;
            }
            const u64 _bit = 1ull << _k;
            if(_p->drive & _bit) {
                const b8 _high = (_p->drive_blink & _bit) ? fmod(_t, 1.0) < 0.5 : (_p->drive_high & _bit) != 0u;
                fzc_vsrc(_x, _p, _k, _gnd, _high ? (f64)_part->a : 0.0, FZC_LOGIC_R);
            }
        }
        break;
    }
    default:
        break;
    }
}

// What is stepped (logic, a 555, a fuse) worked out from the voltages now. True: something changed (solved again).
RDE_INTERNAL b8 fzc_digital(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    const f64 _was = _p->state[0], _was1 = _p->state[1];
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_TIMER555: {
        const f64 _ctrl = fzc_over(_x, _p, 4, 0);
        if(fzc_over(_x, _p, 3, 0) < 0.7) {
            _p->state[0] = 0.0;
        } else if(fzc_over(_x, _p, 1, 0) < _ctrl * 0.5) {
            _p->state[0] = 1.0;
        } else if(fzc_over(_x, _p, 5, 0) > _ctrl) {
            _p->state[0] = 0.0;
        }
        break;
    }
    case FUDE_ZOOM_MODEL_SHIFT595: {
        const f64 _half = 0.5 * fzc_over(_x, _p, 15, 7);
        const b8 _srclk = fzc_over(_x, _p, 10, 7) > _half, _rclk = fzc_over(_x, _p, 11, 7) > _half;
        u32 _reg = (u32)_p->state[0];
        if(fzc_over(_x, _p, 9, 7) < _half) {
            _reg = 0u;   // (cleared)
        } else if(_srclk && _p->state[2] < 0.5) {
            _reg = ((_reg << 1) | (fzc_over(_x, _p, 13, 7) > _half ? 1u : 0u)) & 0xFFu;
        }
        if(_rclk && _p->state[3] < 0.5) {
            _p->state[1] = (f64)_reg;
        }
        _p->state[0] = (f64)_reg;
        _p->state[2] = _srclk ? 1.0 : 0.0;
        _p->state[3] = _rclk ? 1.0 : 0.0;
        return _p->state[0] != _was || _p->state[1] != _was1;
    }
    case FUDE_ZOOM_MODEL_DRIVER293: {
        const f64 _half = 0.5 * fmax(fzc_over(_x, _p, 15, 3), 3.0);
        static const u32 _ch[4][3] = { { 1, 2, 0 }, { 6, 5, 0 }, { 9, 10, 8 }, { 14, 13, 8 } };
        u32 _bits = 0;
        for(u32 _k = 0; _k < 4u; _k++) {
            _bits |= fzc_over(_x, _p, _ch[_k][0], 3) > _half ? 1u << _k : 0u;
            _bits |= fzc_over(_x, _p, _ch[_k][2], 3) > _half ? 1u << (4u + _k) : 0u;
        }
        _p->state[0] = (f64)_bits;
        break;
    }
    case FUDE_ZOOM_MODEL_ULN2003: {
        u32 _bits = 0;
        for(u32 _k = 0; _k < 7u; _k++) {
            _bits |= fzc_over(_x, _p, _k, 7) > 1.5 ? 1u << _k : 0u;
        }
        _p->state[0] = (f64)_bits;
        break;
    }
    case FUDE_ZOOM_MODEL_RELAY:
        _p->state[0] = fzc_over(_x, _p, 2, 1) > 2.0 && fzc_over(_x, _p, 0, 1) > 3.0 ? 1.0 : 0.0;
        break;
    case FUDE_ZOOM_MODEL_FUSE:
        if(fabs(_p->pin_i[0]) > _p->value[0] * 1.5) {
            _p->state[0] = 1.0;   // (blown, until the circuit starts again)
        }
        break;
    default:
        return false;
    }
    return _p->state[0] != _was;
}

// The system solved (Gaussian elimination, the largest pivot each column). False: singular.
RDE_INTERNAL b8 fzc_solve(f64* _a, f64* _b, f64* _x, u32 _n, u32* _cols) {
    for(u32 _k = 0; _k < _n; _k++) {
        u32 _best = _k;
        f64 _big = fabs(_a[_k * _n + _k]);
        for(u32 _i = _k + 1u; _i < _n; _i++) {
            if(fabs(_a[_i * _n + _k]) > _big) {
                _big  = fabs(_a[_i * _n + _k]);
                _best = _i;
            }
        }
        if(!(_big > 1e-30)) {
            return false;
        }
        if(_best != _k) {
            for(u32 _j = _k; _j < _n; _j++) {   // (left of _k: nothing read again)
                const f64 _t = _a[_k * _n + _j];
                _a[_k * _n + _j] = _a[_best * _n + _j];
                _a[_best * _n + _j] = _t;
            }
            const f64 _t = _b[_k];
            _b[_k] = _b[_best];
            _b[_best] = _t;
        }
        // (a circuit's rows are mostly zeros: only the pivot row's others taken from the rows under it)
        const f64* _row = &_a[_k * _n];
        u32 _m = 0;
        for(u32 _j = _k + 1u; _j < _n; _j++) {
            if(_row[_j] != 0.0) {
                _cols[_m++] = _j;
            }
        }
        const f64 _piv = _row[_k];
        for(u32 _i = _k + 1u; _i < _n; _i++) {
            f64* _under = &_a[_i * _n];
            const f64 _f = _under[_k] / _piv;
            if(_f == 0.0) {
                continue;
            }
            for(u32 _j = 0; _j < _m; _j++) {
                _under[_cols[_j]] -= _f * _row[_cols[_j]];
            }
            _b[_i] -= _f * _b[_k];
        }
    }
    for(u32 _k = _n; _k-- > 0u;) {
        f64 _s = _b[_k];
        for(u32 _j = _k + 1u; _j < _n; _j++) {
            _s -= _a[_k * _n + _j] * _x[_j];
        }
        _x[_k] = _s / _a[_k * _n + _k];
    }
    return true;
}

#define FZC_REFERENCE_G 1.0    // a block ground reaches nowhere: its reference's conductance to ground (siemens)
#define FZC_DENSE_MOST 400u   // a block solved densely when its sparse factoring cannot: so many unknowns at most
#define FZC_RESHAPES   8u     // a Newton solve's systems shaped again (a stamp writing somewhere new): so often at most

RDE_INTERNAL int fzc_u64_cmp(const void* _a, const void* _b) {
    const u64 _x = *(const u64*)_a, _y = *(const u64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Its blocks' systems shaped (sim/sparse.h): where each part's stamps write — as they would now, and wherever they
// wrote before (the joins kept) —, each block's in its own numbering (its slots from its first).
RDE_INTERNAL void fzc_shape(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    _c->recording = true;
    for(u32 _i = 0; _i < _np; _i++) {
        fzc_stamp(_x, &_p[_i]);   // (its currents written too: put right when it is stamped for real)
    }
    _c->recording = false;
    _c->reshape   = false;
    // The joins, each once; then dealt to their blocks.
    u64* _j = (u64*)_c->joins.memory;
    u32 _nj = (u32)rde_arr_length(&_c->joins);
    qsort(_j, _nj, sizeof(u64), fzc_u64_cmp);
    u32 _kept = 0;
    for(u32 _k = 0; _k < _nj; _k++) {
        if(_kept == 0u || _j[_k] != _j[_kept - 1u]) {
            _j[_kept++] = _j[_k];
        }
    }
    rde_arr_resize(&_c->joins, _kept);
    _nj = _kept;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    const u32* _first = (const u32*)_c->block_first.memory;
    const u32* _size  = (const u32*)_c->block_size.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(u32) _at_arr    = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u64) _local_arr = rde_arr_new(sizeof(u64), _heap);
    rde_arr_resize(&_at_arr, _c->blocks + 1u);
    rde_arr_resize(&_local_arr, _nj);
    u32* _at    = (u32*)_at_arr.memory;   // (each block's first in _local: counted, then added up)
    u64* _local = (u64*)_local_arr.memory;
    for(u32 _k = 0; _k < _nj; _k++) {
        _at[_block[(u32)(_j[_k] >> 32)] + 1u]++;
    }
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _at[_b + 1u] += _at[_b];
    }
    for(u32 _k = 0; _k < _nj; _k++) {
        const u32 _r = (u32)(_j[_k] >> 32), _q = (u32)(_j[_k] & 0xFFFFFFFFull), _b = _block[_r];
        _local[_at[_b]++] = ((u64)(_slot[_r] - _first[_b]) << 32) | (_slot[_q] - _first[_b]);
    }
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    u32 _from = 0;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        fude_sim_sparse_shape(&_sys[_b], _size[_b], &_local[_from], _at[_b] - _from);
        _from = _at[_b];
    }
    rde_arr_free(&_at_arr);
    rde_arr_free(&_local_arr);
}

// Block _b's system solved for _rhs into _sol (its slots): factored as it is shaped; when a pivot is too small for
// that (a gain ahead of what it drives, a source straight across another), densely with rows swapped — not too big.
RDE_INTERNAL b8 fzc_block_solve(fude_zoom_circuit* _c, u32 _b, f64* _rhs, f64* _sol) {
    fude_sim_sparse* _s = &((fude_sim_sparse*)_c->block_system.memory)[_b];
    if(fude_sim_sparse_factor(_s)) {
        fude_sim_sparse_solve(_s, _rhs, _sol);
        return true;
    }
    if(_s->n > FZC_DENSE_MOST) {
        return false;
    }
    rde_arr_resize(&_c->a, (usize)_s->n * _s->n);
    fude_sim_sparse_dense(_s, (f64*)_c->a.memory);
    return fzc_solve((f64*)_c->a.memory, _rhs, _sol, _s->n, (u32*)_c->cols.memory);
}

// Not solved: the parts where it would not settle marked (suspect) — in block _b, each on a node that moved at least
// half as much as the most in it did last time round (FUDE_ZOOM_NONE: every block not settled); _all: every part of it
// (it could not be solved at all: a source across another, a loop of them).
RDE_INTERNAL void fzc_suspect(fude_zoom_circuit* _c, u32 _b, b8 _all) {
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    const u8*  _done  = (const u8*)_c->block_done.memory;
    const f64* _sol   = (const f64*)_c->x.memory;
    const f64* _v     = (const f64*)_c->v.memory;
    f64 _most = 0.0;
    for(u32 _k = 1; _k < _c->nodes; _k++) {
        const u32 _kb = _block[_k];
        if(_kb != FUDE_ZOOM_NONE && (_kb == _b || (_b == FUDE_ZOOM_NONE && _done[_kb] != 1u))) {
            _most = fmax(_most, fabs(_sol[_slot[_k]] - _v[_k]));
        }
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const u32 _pb = _p[_i].block;
        if(_pb == FUDE_ZOOM_NONE || !(_pb == _b || (_b == FUDE_ZOOM_NONE && _done[_pb] != 1u))) {
            continue;
        }
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < FZC_PIN_MAX && !_p[_i].suspect; _k++) {
            const u32 _node = _p[_i].node[_k];
            _p[_i].suspect = _all || (_node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes &&
                                      fabs(_sol[_slot[_node]] - _v[_node]) >= 0.5 * _most && _most > 0.0);
        }
        _c->suspected = _c->suspected || _p[_i].suspect;
    }
}

// Each part's stamps, the system solved, again until it settles (Newton) — block by block, each settled left as it is
// (_only: that block alone; FUDE_ZOOM_NONE: all). False: it did not (all of them: where, marked).
RDE_INTERNAL b8 fzc_newton(fzc_ctx* _x, u32 _only) {
    fude_zoom_circuit* _c = _x->c;
    const u32 _n = _x->n;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    if(_n == 0u || _c->blocks == 0u) {
        for(u32 _i = 0; _i < _np; _i++) {
            memset(_p[_i].pin_i, 0, sizeof(_p[_i].pin_i));
        }
        return true;
    }
    if(_c->reshape) {
        fzc_shape(_x);
    }
    f64* _rhs = (f64*)_c->rhs.memory;   // (sized by fzc_room)
    f64* _sol = (f64*)_c->x.memory;
    const u32* _first = (const u32*)_c->block_first.memory;
    const u32* _size  = (const u32*)_c->block_size.memory;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    f64* _v    = (f64*)_c->v.memory;
    u8*  _done = (u8*)_c->block_done.memory;    // 1: settled; 0, 2: being solved (2: nothing in it moved yet this time)
    f64* _most = (f64*)_c->block_most.memory;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _done[_b] = _only != FUDE_ZOOM_NONE && _b != _only ? 1u : 0u;
    }
    u32 _reshapes = 0;
    for(u32 _it = 0; _it < 150u;) {
        // (a settled block's system, and its parts' currents, as they were)
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            if(_done[_b] != 1u) {
                fude_sim_sparse_clear(&_sys[_b]);
                memset(&_rhs[_first[_b]], 0, (usize)_size[_b] * sizeof(f64));
            }
        }
        for(u32 _i = 0; _i < _np; _i++) {
            if(_p[_i].block != FUDE_ZOOM_NONE && _done[_p[_i].block] == 1u) {
                continue;
            }
            const u32 _pins = _p[_i].part->pin_count < FZC_PIN_MAX ? _p[_i].part->pin_count : FZC_PIN_MAX;
            memset(_p[_i].pin_i, 0, (usize)_pins * sizeof(f64));
            fzc_stamp(_x, &_p[_i]);
        }
        if(_c->reshape) {
            // A stamp wrote where its block's shape had no place: shaped again with it, and solved from the start (every
            // part's currents written again).
            if(++_reshapes > FZC_RESHAPES) {
                return false;
            }
            fzc_shape(_x);
            for(u32 _b = 0; _b < _c->blocks; _b++) {
                _done[_b] = _only != FUDE_ZOOM_NONE && _b != _only ? 1u : 0u;
            }
            continue;
        }
        const u32* _ref = (const u32*)_c->block_reference.memory;
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            if(_done[_b] == 1u) {
                continue;
            }
            fude_sim_sparse_add_diagonal(&_sys[_b], FZC_GMIN);
            if(_ref[_b] != FUDE_ZOOM_NONE) {
                *fzc_entry(_c, _ref[_b], _ref[_b]) += FZC_REFERENCE_G;   // (its diagonal: always a place)
            }
            if(!fzc_block_solve(_c, _b, &_rhs[_first[_b]], &_sol[_first[_b]])) {
                if(_only == FUDE_ZOOM_NONE) {
                    fzc_suspect(_c, _b, true);
                }
                return false;
            }
            _most[_b] = 0.0;
            _done[_b] = 2u;
        }
        // (each block's step: at most 2 V at its most — exponentials stay near —, settled when nothing in it moves)
        for(u32 _k = 1; _k < _c->nodes; _k++) {
            const u32 _b = _block[_k];
            if(_b != FUDE_ZOOM_NONE && _done[_b] != 1u) {
                _most[_b] = fmax(_most[_b], fabs(_sol[_slot[_k]] - _v[_k]));
            }
        }
        for(u32 _k = 1; _k < _c->nodes; _k++) {
            const u32 _b = _block[_k];
            if(_b == FUDE_ZOOM_NONE || _done[_b] == 1u) {
                continue;
            }
            const f64 _damp = _most[_b] > 2.0 ? 2.0 / _most[_b] : 1.0;
            const f64 _d    = (_sol[_slot[_k]] - _v[_k]) * _damp;
            if(!(fabs(_d) <= 1e-7 + 1e-6 * fabs(_sol[_slot[_k]]))) {
                _done[_b] = 0u;
            }
            _v[_k] += _d;
        }
        b8 _all = true;
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            _done[_b] = _done[_b] == 2u ? 1u : _done[_b];
            _all = _all && _done[_b] == 1u;
        }
        _it++;
        _c->solves = _it;
        if(_all) {
            return true;
        }
    }
    if(_only == FUDE_ZOOM_NONE) {
        fzc_suspect(_c, FUDE_ZOOM_NONE, false);
    }
    return false;
}

// The system's scratch sized for _n unknowns and its blocks (the dense matrix only when one is solved so: as it is).
RDE_INTERNAL void fzc_room(fude_zoom_circuit* _c, u32 _n) {
    rde_arr_resize(&_c->rhs, _n > 0u ? _n : 1u);
    rde_arr_resize(&_c->x, _n > 0u ? _n : 1u);
    rde_arr_resize(&_c->block_done, _c->blocks);
    rde_arr_resize(&_c->block_most, _c->blocks);
    rde_arr_resize(&_c->cols, _n > 0u ? _n : 1u);
}

// The wires' currents: each piece's, as the parts' pins push current into the places they touch, along a tree of the
// wires (a loop of wires: one of its pieces carries none).
RDE_INTERNAL void fzc_wire_currents(fude_zoom_circuit* _c) {
    const u32 _nv = _c->vertices, _nw = (u32)rde_arr_length(&_c->wires), _np = (u32)rde_arr_length(&_c->parts);
    fude_zoom_circuit_wire* _w = (fude_zoom_circuit_wire*)_c->wires.memory;
    if(_nv == 0u || _nw == 0u) {
        return;
    }
    const u32* _cv = (const u32*)_c->cut_vertex.memory;
    const u32* _pv = (const u32*)_c->pin_vertex.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(f64) _inject_arr      = rde_arr_new(sizeof(f64), _heap);
    rde_arr TYPE(u32) _parent_edge_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u32) _order_arr       = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u8)  _seen_arr        = rde_arr_new(sizeof(u8), _heap);
    rde_arr TYPE(u32) _count_arr       = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_inject_arr, _nv);
    rde_arr_resize(&_parent_edge_arr, _nv);
    rde_arr_resize(&_order_arr, _nv);
    rde_arr_resize(&_seen_arr, _nv);
    rde_arr_resize(&_count_arr, _nv + 1u);
    f64* _inject = (f64*)_inject_arr.memory;   // (sized once: they stay put)
    u32* _parent_edge = (u32*)_parent_edge_arr.memory;   // (wire × PIECES + piece; NONE: a root)
    u32* _order = (u32*)_order_arr.memory;
    u8*  _seen = (u8*)_seen_arr.memory;
    // (each vertex's pieces: a flat list)
    u32* _count = (u32*)_count_arr.memory;
    for(u32 _i = 0; _i < _nw; _i++) {
        for(u32 _k = 0; _k < _w[_i].pieces; _k++) {
            const u32 _a = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
            if(_a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
                _count[_a + 1u]++;
                _count[_b + 1u]++;
            }
            _w[_i].current[_k] = 0.0;
        }
    }
    for(u32 _v = 0; _v < _nv; _v++) {
        _count[_v + 1u] += _count[_v];
    }
    rde_arr TYPE(u32) _adj_arr  = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u32) _fill_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_adj_arr, _count[_nv]);
    rde_arr_resize(&_fill_arr, _nv);
    u32* _adj = (u32*)_adj_arr.memory;
    u32* _fill = (u32*)_fill_arr.memory;
    memcpy(_fill, _count, (usize)_nv * sizeof(u32));
    for(u32 _i = 0; _i < _nw; _i++) {
        for(u32 _k = 0; _k < _w[_i].pieces; _k++) {
            const u32 _a = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
            if(_a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
                _adj[_fill[_a]++] = _i * FUDE_ZOOM_WIRE_PIECES + _k;
                _adj[_fill[_b]++] = _i * FUDE_ZOOM_WIRE_PIECES + _k;
            }
        }
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < _np; _i++) {
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < FZC_PIN_MAX; _k++) {
            const u32 _v = _pv[_i * FZC_PIN_MAX + _k];
            if(_v != FUDE_ZOOM_NONE) {
                _inject[_v] -= _p[_i].pin_i[_k];   // (out of the part, into where it touches)
            }
        }
    }
    // Breadth first from each untouched vertex; then from the leaves in, each piece carrying what is beyond it.
    u32 _n = 0;
    for(u32 _s = 0; _s < _nv; _s++) {
        if(_seen[_s]) {
            continue;
        }
        _seen[_s] = 1u;
        _parent_edge[_s] = FUDE_ZOOM_NONE;
        u32 _head = _n;
        _order[_n++] = _s;
        while(_head < _n) {
            const u32 _v = _order[_head++];
            for(u32 _e = _count[_v]; _e < _count[_v + 1u]; _e++) {
                const u32 _edge = _adj[_e], _wi = _edge / FUDE_ZOOM_WIRE_PIECES, _k = _edge % FUDE_ZOOM_WIRE_PIECES;
                const u32 _a = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
                const u32 _o = _a == _v ? _b : _a;
                if(!_seen[_o]) {
                    _seen[_o] = 1u;
                    _parent_edge[_o] = _edge;
                    _order[_n++] = _o;
                }
            }
        }
    }
    for(u32 _i = _n; _i-- > 0u;) {
        const u32 _v = _order[_i], _edge = _parent_edge[_v];
        if(_edge == FUDE_ZOOM_NONE) {
            continue;
        }
        const u32 _wi = _edge / FUDE_ZOOM_WIRE_PIECES, _k = _edge % FUDE_ZOOM_WIRE_PIECES;
        const u32 _a = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
        const u32 _up = _a == _v ? _b : _a;
        // What comes into _v flows on to its parent along the piece: along it when _v is its start.
        _w[_wi].current[_k] = _a == _v ? _inject[_v] : -_inject[_v];
        _inject[_up] += _inject[_v];
    }
    rde_arr_free(&_inject_arr);
    rde_arr_free(&_parent_edge_arr);
    rde_arr_free(&_order_arr);
    rde_arr_free(&_seen_arr);
    rde_arr_free(&_count_arr);
    rde_arr_free(&_adj_arr);
    rde_arr_free(&_fill_arr);
}

// After a step: what the parts show (an LED's brightness, a meter's reading) and remember (a capacitor's voltage).
#define FZC_STEP_DV    0.05    // a step's change at most: a capacitor's volts (and 2% of them)…
#define FZC_STEP_DI    1e-3    // …an inductor's amperes (and 2% of them)
#define FZC_STEP_LEAST 1e-7    // a step's least (seconds)
#define FZC_STEP_FIRST 1e-6    // the first step (and the next after a jump, at least): growing twice at a time

// A 555 as its capacitor heads for a threshold (charging, its output high: THRES for CTRL; discharging: TRIG for half
// of it): when it will reach it, on as it goes now — kept (state[7]: the time; 0, none) for the next step to land just
// past it, its output's edge where it is.
RDE_INTERNAL void fzc_555_ahead(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const f64 _ctrl = fzc_over(_x, _p, 4, 0);
    const f64 _thr  = fzc_over(_x, _p, 5, 0) - _ctrl, _trg = fzc_over(_x, _p, 1, 0) - 0.5 * _ctrl;
    const f64 _dt   = _x->t - _p->state[6];
    _p->state[7] = 0.0;
    if(_p->state[6] > 0.0 && _dt > 0.0) {
        const f64 _ds = (_thr - _p->state[4]) / _dt, _dg = (_trg - _p->state[5]) / _dt;
        if(_p->state[0] > 0.5 && _thr < 0.0 && _ds > 0.0) {
            _p->state[7] = _x->t + -_thr / _ds;
        } else if(_p->state[0] <= 0.5 && _trg > 0.0 && _dg < 0.0) {
            _p->state[7] = _x->t + _trg / -_dg;
        }
    }
    _p->state[4] = _thr;
    _p->state[5] = _trg;
    _p->state[6] = _x->t;
}

#define FZC_SERVO_HIGH   1.5    // a servo's signal high from (volts over its ground)
#define FZC_SERVO_LEAST  3.5    // its supply at least for it to work (volts)

// A servo a step on (GND, VCC, SIG; state: 0 its horn's angle (degrees), 1 where its pulses say to go (-1: none), 2 its
// signal high, 3 when its pulse began, 4 when its last one ended, 5 its signal's volts, 6 what it draws, 7 on a
// mechanism's shaft — how hard it strains (0..1) — or not, -1). Its pulses timed where its signal crosses 1.5 V — at the
// end of the step it crossed in: the steps land on a source's edges and a 555's thresholds, where a pulse jumps (a line
// across the step would put it early by most of the step before): 0.5 to 2.5 ms its angle, 0 to 180°; none for 60 ms,
// it lets go.
// Its horn turned toward it as fast as its supply lets it (its speed at 4.8 V) — on a shaft, coupling turns it. What it
// draws: 10 mA at rest, up to a third of its stall's turning, its stall's held back.
RDE_INTERNAL void fzc_servo(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const f64 _vcc = fzc_over(_x, _p, 1, 0), _sig = fzc_over(_x, _p, 2, 0), _was = _p->state[5];
    if(_was < FZC_SERVO_HIGH && _sig >= FZC_SERVO_HIGH) {
        _p->state[3] = _x->t;
        _p->state[2] = 1.0;
    } else if(_was >= FZC_SERVO_HIGH && _sig < FZC_SERVO_HIGH && _p->state[2] > 0.5) {
        const f64 _end = _x->t;
        const f64 _width = _end - _p->state[3];
        if(_width >= 0.4e-3 && _width <= 2.7e-3) {
            _p->state[1] = fmin(fmax((_width - 0.5e-3) / 2.0e-3 * 180.0, 0.0), 180.0);
            _p->state[4] = _end;
        }
        _p->state[2] = 0.0;
    }
    _p->state[5] = _sig;
    if(_p->state[4] >= 0.0 && _x->t - _p->state[4] > 0.06) {
        _p->state[1] = -1.0;
    }
    const b8  _on  = _vcc >= FZC_SERVO_LEAST && _p->state[1] >= 0.0;
    const f64 _top = _p->value[1] * 180.0 / FZC_PI * fmin(_vcc / 4.8, 1.3);   // (degrees a second)
    f64 _pace = 0.0, _strain = _p->state[7] >= 0.0 ? _p->state[7] : 0.0;
    if(_on && _p->state[7] < 0.0) {
        const f64 _d = fmin(fmax(_p->state[1] - _p->state[0], -_top * _x->dt), _top * _x->dt);
        _p->state[0] += _d;
        _pace = _top > 0.0 ? fabs(_d) / (_top * _x->dt) : 0.0;
    } else if(!_on) {
        _strain = 0.0;
    }
    _p->state[6] = _vcc >= FZC_SERVO_LEAST ? 0.01 + _p->value[2] * fmax(_pace / 3.0, _strain) : 0.0;
    _p->shown = _p->state[0];
}

// How long until something is about to happen (seconds; a longer step would miss its moment): a clock's next edge, a
// 555's threshold (just past it), an AC source's fortieth of a period.
RDE_INTERNAL f64 fzc_breakpoint(const fude_zoom_circuit* _c) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const f64 _t = _c->time;
    f64 _h = 1e300;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const fude_zoom_circuit_part* _q = &_p[_i];
        switch(_q->part->model) {
        case FUDE_ZOOM_MODEL_CLOCK:
            if(_q->value[0] > 0.0) {
                const f64 _f = _q->value[0], _d = fzc_duty(_q);
                const f64 _ph = fzc_clock_phase(_q, _t);
                _h = fmin(_h, ((_ph < _d ? _d : 1.0) - _ph + 1e-9) / _f);   // (landing just past it)
            }
            break;
        case FUDE_ZOOM_MODEL_ACSOURCE:
            if(_q->value[1] > 0.0) {
                _h = fmin(_h, 1.0 / (40.0 * _q->value[1]));
            }
            break;
        case FUDE_ZOOM_MODEL_TIMER555:
            if(_q->state[7] > _t) {
                const f64 _to = _q->state[7] - _t;
                _h = fmin(_h, _to + fmax(1e-3 * _to, 2.0 * FZC_STEP_LEAST));
            }
            break;
        default:
            break;
        }
    }
    return _h;
}

#define FZC_OVER_TELL 0.02   // a source past its current so long (seconds) before it is said (not a capacitor's charging)

// Each part against its limits (limits.h), a step of time on: measured, heated past them — burnt, or a source past its
// current, said (an event).
RDE_INTERNAL void fzc_display_measure(fzc_ctx* _x, fude_zoom_circuit_part* _p);

RDE_INTERNAL void fzc_limits(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    f64 _volts[FZC_PIN_MAX], _amps[FZC_PIN_MAX];
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _q = &_p[_i];
        if(fude_zoom_limits_kinds(_q->part) == 0u) {
            continue;
        }
        if(_q->burnt) {
            memset(_q->measure, 0, sizeof(_q->measure));   // (open: nothing in it now)
            _q->stress = 0.0;
            continue;
        }
        const u32 _pins = _q->part->pin_count < FZC_PIN_MAX ? _q->part->pin_count : FZC_PIN_MAX;
        for(u32 _k = 0; _k < _pins; _k++) {
            _volts[_k] = fzc_pv(_x, _q, _k);
            _amps[_k]  = _q->node[_k] != FUDE_ZOOM_NONE ? _q->pin_i[_k] : 0.0;
        }
        fude_zoom_limits_measure(_q->part, _volts, _amps, _pins, _q->measure);
        if(fude_zoom_display_lights(_q->part) > 0u) {
            fzc_display_measure(_x, _q);
        }
        const u8 _m = _q->part->model;
        if((_m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_ACSOURCE) && _q->node[0] != FUDE_ZOOM_NONE &&
           _q->node[0] == _q->node[1]) {
            // A wire straight across it (its two pins one node): a dead short — as much as its own resistance lets go.
            _q->measure[FUDE_ZOOM_LIMIT_CURRENT] = fabs(_q->value[0]) / (_m == FUDE_ZOOM_MODEL_BATTERY ? 0.5 : 1e-3);
            _q->heat = FZC_OVER_TELL;
        }
        const b8 _burns = fude_zoom_limits_step(_q->part, &_q->limits, _q->measure, _x->dt, &_q->stress, &_q->worst, &_q->heat);
        if(_q->stress > 1.0) {
            _q->worst_seen = fmax(_q->worst_seen, _q->measure[_q->worst]);
        }
        const fude_zoom_circuit_event _e = { _i, 0u, _q->worst, _q->measure[_q->worst], _q->limits.most[_q->worst] };
        if(_burns) {
            _q->burnt = true;
            fude_zoom_circuit_event _b = _e;
            _b.kind    = FUDE_ZOOM_CIRCUIT_BURNT;
            _b.measure = fmax(_q->worst_seen, _e.measure);
            rde_arr_add(&_c->events, (any)&_b);
        } else if(_q->stress > 1.0 && !(fude_zoom_limits_tau(_q->part) > 0.0) && _q->heat >= FZC_OVER_TELL && !_q->told) {
            _q->told = true;
            fude_zoom_circuit_event _o = _e;
            _o.kind = FUDE_ZOOM_CIRCUIT_OVER;
            rde_arr_add(&_c->events, (any)&_o);
        } else if(_q->stress <= 1.0) {
            _q->told = false;
        }
    }
}

// A display's LEDs as the eye sees them (display.h): each one's current against a fully bright one's, held a moment —
// stepping, eased toward it over FUDE_ZOOM_DISPLAY_GLOW; solved for a moment, as it is.
RDE_INTERNAL void fzc_glow(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    f32* _glow = (f32*)fude_zoom_circuit_store(_x->c, _p);
    if(_glow == NULL) {
        return;
    }
    const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
    const f64 _ease = _x->tran ? 1.0 - exp(-_x->dt / FUDE_ZOOM_DISPLAY_GLOW) : 1.0;
    const u32 _n = fude_zoom_display_lights(_p->part);
    for(u32 _l = 0; _l < _n; _l++) {
        u32 _a, _b;
        f64 _see = 0.0;
        if(!_p->burnt && fude_zoom_display_led(_p->part, _l, &_a, &_b) && _p->node[_a] != FUDE_ZOOM_NONE && _p->node[_b] != FUDE_ZOOM_NONE) {
            const f64 _v[2] = { fzc_pv(_x, _p, _a), fzc_pv(_x, _p, _b) };
            f64 _i[2];
            fzc_diode_fn(_v, _i, &_k);
            _see = fmin(fmax(_i[0] / FUDE_ZOOM_DISPLAY_FULL, 0.0), 1.5);
        }
        _glow[_l] += (f32)((_see - (f64)_glow[_l]) * _ease);
    }
}

// A display's limits measured LED by LED (a panel's segment pin carries a digit's LED each: what each takes is its own
// current, and how far it is the wrong way round).
RDE_INTERNAL void fzc_display_measure(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
    f64 _most = 0.0, _back = 0.0;
    const u32 _n = fude_zoom_display_lights(_p->part);
    for(u32 _l = 0; _l < _n; _l++) {
        u32 _a, _b;
        if(fude_zoom_display_led(_p->part, _l, &_a, &_b) && _p->node[_a] != FUDE_ZOOM_NONE && _p->node[_b] != FUDE_ZOOM_NONE) {
            const f64 _v[2] = { fzc_pv(_x, _p, _a), fzc_pv(_x, _p, _b) };
            f64 _i[2];
            fzc_diode_fn(_v, _i, &_k);
            _most = fmax(_most, _i[0]);
            _back = fmax(_back, _v[1] - _v[0]);
        }
    }
    _p->measure[FUDE_ZOOM_LIMIT_CURRENT] = _most;
    _p->measure[FUDE_ZOOM_LIMIT_REVERSE] = _back;
}

// An LM3914 after a step: its outputs' current (ten times what REF OUT gives: 1 to 30 mA); which its comparators light
// — each of its divider's ten steps SIG is over, from RLO up to RHI (a bar: all of them, MODE on V+; a dot: the highest
// alone) — its supply 3 V at least. What changes jumps.
RDE_INTERNAL void fzc_3914(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    _p->state[1] = fmin(fmax(-10.0 * _p->pin_i[6], 1e-3), 0.03);
    const f64 _supply = fzc_over(_x, _p, 2, 1);
    u32 _lit = 0u;
    if(_supply >= 3.0 && !_p->burnt) {
        const f64 _lo = fzc_over(_x, _p, 3, 1), _hi = fzc_over(_x, _p, 5, 1), _sig = fzc_over(_x, _p, 4, 1);
        for(u32 _k = 0; _k < 10u; _k++) {
            if(_sig > _lo + (_hi - _lo) * (f64)(_k + 1u) / 10.0) {
                _lit |= 1u << _k;
            }
        }
        const b8 _bar = _p->node[8] != FUDE_ZOOM_NONE && fzc_over(_x, _p, 8, 1) > _supply - 0.6;
        if(!_bar && _lit != 0u) {
            u32 _top = 0;
            while((_lit >> (_top + 1u)) != 0u) {
                _top++;
            }
            _lit = 1u << _top;
        }
    }
    if((f64)_lit != _p->state[0]) {
        _p->state[0] = (f64)_lit;
        fude_zoom_circuit_jump(_x->c);
    }
}

// A character LCD after a step (display.h): its backlight as bright as its current; with power (2.7 V at least), its
// controller on (as it comes, when it was off), its contrast (VDD over V0: from 3 V, fully at 4.2 V); E fallen while RW
// is low, its data pins read — a byte, or half of one — and RS: a command or a character. Its levels half its supply.
RDE_INTERNAL void fzc_lcd(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    fude_zoom_lcd* _l = (fude_zoom_lcd*)fude_zoom_circuit_store(_x->c, _p);
    if(_l == NULL) {
        return;
    }
    _p->shown = fmin(fmax(_p->pin_i[14] / 0.015, 0.0), 1.5);
    const f64 _vdd = fzc_over(_x, _p, 1, 0);
    if(_p->burnt || _vdd < 2.7 || _p->node[0] == FUDE_ZOOM_NONE || _p->node[1] == FUDE_ZOOM_NONE) {
        _l->powered  = 0u;
        _p->state[0] = 0.0;
        return;
    }
    if(!_l->powered) {
        fude_zoom_lcd_power_on(_l);
    }
    _p->state[0] = fmin(fmax((_vdd - fzc_over(_x, _p, 2, 0) - 3.0) / 1.2, 0.0), 1.0);
    const f64 _half = 0.5 * _vdd;
    const b8 _e = fzc_over(_x, _p, 5, 0) > _half;
    if(_l->e && !_e && fzc_over(_x, _p, 4, 0) < _half) {
        u8 _pins = 0u;
        for(u32 _b = 0; _b < 8u; _b++) {
            if(fzc_over(_x, _p, 6u + _b, 0) > _half) {
                _pins |= (u8)(1u << _b);
            }
        }
        fude_zoom_lcd_strobe(_l, fzc_over(_x, _p, 3, 0) > _half, _pins);
    }
    _l->e = _e ? 1u : 0u;
}

RDE_INTERNAL void fzc_after(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    if(_x->tran) {
        fzc_limits(_x);
        _c->step_ratio = 0.0;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _q = &_p[_i];
        switch(_q->part->model) {
        case FUDE_ZOOM_MODEL_CAPACITOR: {
            // (how much it changed this step against what a step should change it by: the next step's size)
            const f64 _now = fzc_over(_x, _q, 0, 1);
            if(_x->tran) {
                _c->step_ratio = fmax(_c->step_ratio, fabs(_now - _q->state[0]) / (FZC_STEP_DV + 0.02 * fabs(_now)));
            }
            _q->state[0] = _now;
            _q->state[1] = _x->tran ? _q->pin_i[0] : 0.0;   // (its current: none, solved for a moment)
            break;
        }
        case FUDE_ZOOM_MODEL_INDUCTOR:
            if(_x->tran) {
                _c->step_ratio = fmax(_c->step_ratio, fabs(_q->pin_i[0] - _q->state[0]) / (FZC_STEP_DI + 0.02 * fabs(_q->pin_i[0])));
                _q->state[0] = _q->pin_i[0];
                _q->state[1] = fzc_over(_x, _q, 0, 1);
            }
            break;
        case FUDE_ZOOM_MODEL_TIMER555:
            if(_x->tran) {
                fzc_555_ahead(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_SERVO:
            if(_x->tran) {
                fzc_servo(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_LED:       _q->shown = fmin(fmax(_q->pin_i[0] / 0.01, 0.0), 1.5); break;
        case FUDE_ZOOM_MODEL_SEG_PANEL:
        case FUDE_ZOOM_MODEL_LED_MATRIX:
        case FUDE_ZOOM_MODEL_BAR_GRAPH:   fzc_glow(_x, _q); break;
        case FUDE_ZOOM_MODEL_LM3914:      fzc_3914(_x, _q); break;
        case FUDE_ZOOM_MODEL_PANEL_METER: _q->shown = _q->value[1] > 0.5 ? _q->pin_i[0] : fzc_over(_x, _q, 0, 1); break;
        case FUDE_ZOOM_MODEL_CHAR_LCD:    fzc_lcd(_x, _q); break;
        case FUDE_ZOOM_MODEL_SEVEN_SEG:
            for(u32 _s = 0; _s < 8u; _s++) {
                _q->state[_s] = fmin(fmax(fzc_over(_x, _q, _s, 8) - 1.5, 0.0) / 0.3, 1.0);
            }
            break;
        case FUDE_ZOOM_MODEL_LAMP:      _q->shown = fabs(fzc_over(_x, _q, 0, 1) * _q->pin_i[0]) / fmax(_q->value[1], 1e-9); break;
        case FUDE_ZOOM_MODEL_MOTOR:     _q->shown += _q->shafted ? _q->spin * _x->dt : _q->pin_i[0] * _x->dt * 60.0; break;
        case FUDE_ZOOM_MODEL_BUZZER:    _q->shown = fabs(_q->pin_i[0]) > 2e-3 ? 1.0 : 0.0; break;
        case FUDE_ZOOM_MODEL_VOLTMETER: _q->shown = fzc_over(_x, _q, 0, 1); break;
        case FUDE_ZOOM_MODEL_AMMETER:   _q->shown = _q->pin_i[0]; break;
        case FUDE_ZOOM_MODEL_LOGIC_OUT: _q->shown = fude_zoom_logic_probe(_c->logic, _i) == FUDE_SIM_1 ? 1.0 : 0.0; break;
        case FUDE_ZOOM_MODEL_REGULATOR: _q->state[1] = -_q->pin_i[2]; break;
        case FUDE_ZOOM_MODEL_RELAY:     _q->shown = _q->state[0]; break;
        default: break;
        }
    }
}

RDE_INTERNAL b8 fzc_at(fude_zoom_circuit* _c, b8 _tran, f64 _dt) {
    const u32 _n = _c->nodes > 0u ? _c->nodes - 1u : 0u;
    fzc_room(_c, _n > 0u ? _n : 1u);
    fzc_ctx _x = { _c, _n, _tran, _dt, _c->time + (_tran ? _dt : 0.0), _c->be_next };
    b8 _jumped = false;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    b8 _ok = fzc_newton(&_x, FUDE_ZOOM_NONE);
    // Logic settles within the step: solved again while it changes (16 times at most) — what is stepped worked out
    // from the voltages, the logic's bridges read and what they drive again.
    for(u32 _settle = 0; _ok && _settle < 16u; _settle++) {
        b8 _changed = fude_zoom_logic_step(_c->logic, _c, _x.t);
        for(u32 _i = 0; _i < _np; _i++) {
            _changed = fzc_digital(&_x, &_p[_i]) || _changed;
        }
        if(!_changed) {
            break;
        }
        _jumped = true;
        _ok = fzc_newton(&_x, FUDE_ZOOM_NONE);
    }
    if(_ok && _c->suspected) {
        for(u32 _i = 0; _i < _np; _i++) {
            _p[_i].suspect = false;   // (solved again: nothing to look at)
        }
        _c->suspected = false;
    }
    if(_ok) {
        // (a node only logic is on: its level's volts, as the analog circuit does not know it)
        f64* _v = (f64*)_c->v.memory;
        for(u32 _node = 1; _node < _c->nodes; _node++) {
            const u8 _level = fude_zoom_logic_node(_c->logic, _node);
            if(_level == FUDE_SIM_0 || _level == FUDE_SIM_1) {
                _v[_node] = _level == FUDE_SIM_1 ? FZC_LOGIC_V : 0.0;
            }
        }
        fzc_after(&_x);
        if(_tran) {
            _c->time     += _dt;
            _c->step_last = _dt;
            _c->be_last   = _x.be;
            _c->be_next   = _jumped;   // (a jump: the next step backward Euler, and short)
            if(_jumped) {
                _c->step_now = fmax(_dt / 8.0, FZC_STEP_FIRST);
            }
        }
    }
    _c->ok = _ok;
    return _ok;
}

b8 fude_zoom_circuit_dc(fude_zoom_circuit* _c) {
    const b8 _ok = fzc_at(_c, false, _c->step);
    if(_ok) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

b8 fude_zoom_circuit_run(fude_zoom_circuit* _c, f64 _dt, u32 _max_steps) {
    return fude_zoom_circuit_advance(_c, _dt, _max_steps, true);
}

b8 fude_zoom_circuit_advance(fude_zoom_circuit* _c, f64 _span, u32 _max_steps, b8 _wires) {
    const f64 _most = _c->step > 0.0 ? _c->step : 1e-3;
    const f64 _end  = _c->time + fmax(_span, 0.0);
    b8 _ok = true;
    for(u32 _k = 0; _ok && _k < _max_steps && _c->time < _end - 0.5 * FZC_STEP_LEAST; _k++) {
        const f64 _planned = fmax(fmin(fmin(_c->step_now > 0.0 ? _c->step_now : FZC_STEP_FIRST, _most), fzc_breakpoint(_c)), FZC_STEP_LEAST);
        // (the last one landing on the end: never a sliver left over)
        const f64 _h = _c->time + _planned > _end - FZC_STEP_LEAST ? _end - _c->time : _planned;
        _ok = fzc_at(_c, true, _h);
        // The next: as this one's changes say — its worst change at most what a step should make (grown at most twice,
        // cut at most to a quarter); as it was when this one was cut short to land somewhere, unless it changed too much.
        const f64 _r     = _c->step_ratio;
        const f64 _scale = _r > 1e-12 ? fmin(fmax(0.9 / sqrt(_r), 0.25), 2.0) : 2.0;
        const f64 _base  = _h < _planned && _scale >= 1.0 ? fmax(_planned, _c->step_now) : _h;
        _c->step_now = fmin(fmax(_base * (_h < _planned && _scale >= 1.0 ? 1.0 : _scale), FZC_STEP_LEAST), _most);
    }
    if(_ok && _wires) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

b8 fude_zoom_circuit_steps(fude_zoom_circuit* _c, u32 _steps, b8 _wires) {
    const f64 _step = _c->step > 0.0 ? _c->step : 1e-3;
    b8 _ok = true;
    for(u32 _k = 0; _k < _steps && _ok; _k++) {
        _ok = fzc_at(_c, true, _step);
    }
    if(_ok && _wires) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

void fude_zoom_circuit_jump(fude_zoom_circuit* _c) {
    _c->be_next  = true;
    _c->step_now = _c->step_now > 0.0 ? fmax(_c->step_now / 8.0, FZC_STEP_FIRST) : 0.0;
}

u8* fude_zoom_circuit_store(fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p) {
    return _p != NULL && _p->store_size > 0u && (usize)_p->store_at + _p->store_size <= rde_arr_length(&_c->store) ? (u8*)_c->store.memory + _p->store_at : NULL;
}

const u8* fude_zoom_circuit_store_of(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p) {
    return _p != NULL && _p->store_size > 0u && (usize)_p->store_at + _p->store_size <= rde_arr_length(&_c->store) ? (const u8*)_c->store.memory + _p->store_at : NULL;
}

f64 fude_zoom_circuit_volts(const fude_zoom_circuit* _c, u32 _node) {
    return _node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes && _node < (u32)rde_arr_length(&_c->v) ? ((const f64*)_c->v.memory)[_node] : 0.0;
}

f64 fude_zoom_motor_k(const fude_zoom_circuit_part* _motor) {
    // (its rated volts at its rated speed: rpm in radians a second)
    const f64 _volts = _motor->value[0] > 0.0 ? _motor->value[0] : 6.0, _rpm = _motor->value[1] > 0.0 ? _motor->value[1] : 60.0;
    return _volts / (_rpm * 2.0 * 3.141592653589793 / 60.0);
}

b8 fude_zoom_circuit_motor_load(fude_zoom_circuit* _c, u32 _part, f64* _conductance, f64* _still) {
    *_conductance = 0.0;
    *_still = 0.0;
    if(_part >= (u32)rde_arr_length(&_c->parts) || !_c->ok) {
        return false;
    }
    fude_zoom_circuit_part* _p = &((fude_zoom_circuit_part*)_c->parts.memory)[_part];
    if(_p->part->model != FUDE_ZOOM_MODEL_MOTOR || !_p->shafted) {
        return false;
    }
    const f64 _k = fude_zoom_motor_k(_p), _e = _k * _p->spin, _i0 = _p->pin_i[0];
    *_still = _e;
    const u32 _n = _c->nodes > 0u ? _c->nodes - 1u : 0u;
    if(_n == 0u || fzc_node(_p, 0) == FUDE_ZOOM_NONE || fzc_node(_p, 1) == FUDE_ZOOM_NONE) {
        return true;   // (joined to nothing: nothing through it whatever it turns)
    }
    // Solved again with its back-EMF a little more (as the last step was: its state as it is), what it changes through
    // it; then all as it was.
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    fude_zoom_circuit_part* _all = (fude_zoom_circuit_part*)_c->parts.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _keep_arr = rde_arr_new(sizeof(f64), _heap);
    rde_arr_resize(&_keep_arr, (usize)_np * 64u);
    f64* _keep = (f64*)_keep_arr.memory;
    rde_arr_resize(&_c->v_keep, _c->nodes);
    memcpy(_c->v_keep.memory, _c->v.memory, (usize)_c->nodes * sizeof(f64));
    for(u32 _i = 0; _i < _np; _i++) {
        memcpy(&_keep[(usize)_i * 64u], _all[_i].pin_i, sizeof(_all[_i].pin_i));
    }
    const u32 _solves = _c->solves;
    const f64 _de = fmax(0.02 * fabs(_e), 0.05);
    const f64 _spin = _p->spin;
    _p->spin = _spin + _de / _k;
    fzc_room(_c, _n);
    fzc_ctx _x = { _c, _n, true, _c->step_last > 0.0 ? _c->step_last : _c->step, _c->time, _c->be_last };
    // (only its own block: the others are as they were)
    const u32 _own = fzc_node(_p, 0) != 0u ? fzc_node(_p, 0) : fzc_node(_p, 1);
    const u32 _blk = _own != 0u && _own < _c->nodes ? ((const u32*)_c->node_block.memory)[_own] : FUDE_ZOOM_NONE;
    const b8 _ok = _blk != FUDE_ZOOM_NONE && fzc_newton(&_x, _blk);
    const f64 _i1 = _p->pin_i[0];
    _p->spin = _spin;
    memcpy(_c->v.memory, _c->v_keep.memory, (usize)_c->nodes * sizeof(f64));
    for(u32 _i = 0; _i < _np; _i++) {
        memcpy(_all[_i].pin_i, &_keep[(usize)_i * 64u], sizeof(_all[_i].pin_i));
    }
    _c->solves = _solves;
    rde_arr_free(&_keep_arr);
    if(!_ok) {
        return false;
    }
    // (more back-EMF, less through it: a conductance; never more than its winding's own)
    const f64 _g = fmin(fmax((_i0 - _i1) / _de, 0.0), 1.0 / FUDE_ZOOM_MOTOR_R);
    *_conductance = _g;
    *_still = _g > 1e-12 ? _e + _i0 / _g : _e;
    return true;
}

RDE_INTERNAL int fzc_f64_order(const void* _a, const void* _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

f64 fude_zoom_circuit_tag_px(f64* _sizes, u32 _count, f64 _most, f64 _least) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_sizes[_i] > 0.0 && isfinite(_sizes[_i])) {
            _sizes[_n++] = _sizes[_i];
        }
    }
    if(_n == 0u) {
        return 0.0;
    }
    qsort(_sizes, _n, sizeof(f64), fzc_f64_order);
    const f64 _px = fmin(0.5 * _sizes[_n / 2u], _most);
    return _px >= _least ? _px : 0.0;
}

f64 fude_zoom_part_unit(const fude_zoom_scene* _s, u32 _object) {
    f64 _n[3];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, 3u) < 3u) {
        return 0.0;
    }
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of((u32)_n[0]);
    const f64 _room = _info != NULL ? fude_zoom_part_room_h(fude_zoom_part_of(_s, _object), (f64)_info->h) : 0.0;
    return _room > 0.0 ? _n[2] * fude_zoom_scene_object(_s, _object)->scale / (_room * 0.5) * 10.0 : 0.0;
}

b8 fude_zoom_circuit_tap(fude_zoom_circuit* _c, u32 _part, i32 _pin, c8* _say, usize _size) {
    if(_part >= (u32)rde_arr_length(&_c->parts)) {
        return false;
    }
    fude_zoom_circuit_part* _p = &((fude_zoom_circuit_part*)_c->parts.memory)[_part];
    _say[0] = 0;
    fude_zoom_circuit_jump(_c);   // (what it does now jumps: the steps after it short, the first backward Euler)
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_LOGIC_IN:
        _p->switch_on ^= 1u;
        snprintf(_say, _size, "%s", _p->part->model == FUDE_ZOOM_MODEL_SWITCH ? ((_p->switch_on & 1u) ? "on" : "off") : ((_p->switch_on & 1u) ? "1" : "0"));
        return true;
    case FUDE_ZOOM_MODEL_BUTTON:
        _p->switch_on ^= 1u;
        return true;
    case FUDE_ZOOM_MODEL_POT: {
        const f64 _f = _p->state[0] + 0.25;
        _p->state[0] = _f > 1.0001 ? 0.0 : _f;
        snprintf(_say, _size, "%.0f%%", _p->state[0] * 100.0);
        return true;
    }
    case FUDE_ZOOM_MODEL_BOARD: {
        if(_pin < 0 || (u32)_pin >= _p->part->pin_count || _pin >= (i32)FZC_PIN_MAX || fzc_supply(_p->part->pins[_pin].name, 0.0) > 0.0 ||
           fzc_same_name(_p->part->pins[_pin].name, "GND")) {
            return false;
        }
        // Its pin through input, high, low, blinking.
        const u64 _bit = 1ull << (u32)_pin;
        const c8* _mode;
        if(!(_p->drive & _bit)) {
            _p->drive |= _bit; _p->drive_high |= _bit; _p->drive_blink &= ~_bit; _mode = "HIGH";
        } else if((_p->drive_high & _bit) && !(_p->drive_blink & _bit)) {
            _p->drive_high &= ~_bit; _mode = "LOW";
        } else if(!(_p->drive_blink & _bit)) {
            _p->drive_blink |= _bit; _mode = "blink";
        } else {
            _p->drive &= ~_bit; _p->drive_high &= ~_bit; _p->drive_blink &= ~_bit; _mode = "input";
        }
        snprintf(_say, _size, "%s: %s", _p->part->pins[_pin].name, _mode);
        return true;
    }
    default:
        return false;
    }
}
