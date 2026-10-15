// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/limits.h"
#include "zoom/display.h"

// ===========================================================================
// See limits.h.
//
// The file is in two parts: the real parts (a constant table for each kind, and what follows from them: which a text
// names, a part's typical limits, which limits it has, how quickly it heats), then the measuring and heating at each
// step. Each kind's pins are taken in the order the circuit's catalogue (circuit_catalogue.c) defines them, so the
// measures name pins by their index.
// The file's own prefix is fzlm_ (FZLM_).
// ===========================================================================

// --- real parts ---------------------------------------------------------------------------------------
//
// Each: its name, whether a part's text names it, and its power (W), current (A), voltage (V), reverse or gate voltage
// (V) at most: absolute maximum ratings, as their datasheets give them (the power a small part takes in free air,
// without a heatsink). Each kind's first is the typical one.

// One table entry: its name, whether it is a part number, and its four limits (0 for none).
#define FZLM(_name, _named, _w, _a, _v, _r) { _name, _named, { _w, _a, _v, _r } }
// How many entries table _t has.
#define FZLM_N(_t) (u32)(sizeof(_t) / sizeof(_t[0]))

RDE_INTERNAL const fude_zoom_limits_preset FZLM_RESISTOR[] = {
    FZLM("0.25 W", false, 0.25, 0, 0, 0), FZLM("0.125 W", false, 0.125, 0, 0, 0), FZLM("0.5 W", false, 0.5, 0, 0, 0),
    FZLM("1 W", false, 1, 0, 0, 0), FZLM("2 W", false, 2, 0, 0, 0), FZLM("5 W", false, 5, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_POT[] = {
    FZLM("0.25 W", false, 0.25, 0, 0, 0), FZLM("0.1 W", false, 0.1, 0, 0, 0), FZLM("0.5 W", false, 0.5, 0, 0, 0), FZLM("2 W", false, 2, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_CAPACITOR[] = {
    FZLM("50 V", false, 0, 0, 50, 0), FZLM("16 V", false, 0, 0, 16, 0), FZLM("25 V", false, 0, 0, 25, 0), FZLM("100 V", false, 0, 0, 100, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_ELECTROLYTIC[] = {
    FZLM("16 V", false, 0, 0, 16, 1), FZLM("6.3 V", false, 0, 0, 6.3, 1), FZLM("10 V", false, 0, 0, 10, 1),
    FZLM("25 V", false, 0, 0, 25, 1), FZLM("35 V", false, 0, 0, 35, 1), FZLM("50 V", false, 0, 0, 50, 1),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_INDUCTOR[] = {
    FZLM("1 A", false, 0, 1, 0, 0), FZLM("0.5 A", false, 0, 0.5, 0, 0), FZLM("3 A", false, 0, 3, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_DIODE[] = {
    FZLM("1N4148", true, 0.5, 0.3, 0, 100), FZLM("1N4007", true, 0, 1, 0, 1000), FZLM("1N4001", true, 0, 1, 0, 50),
    FZLM("1N5819", true, 0, 1, 0, 40), FZLM("1N5408", true, 0, 3, 0, 1000),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_LED[] = {
    FZLM("5 mm", false, 0, 0.03, 0, 5), FZLM("3 mm", false, 0, 0.03, 0, 5), FZLM("SMD 0805", false, 0, 0.025, 0, 5),
    FZLM("1 W", false, 0, 0.35, 0, 5), FZLM("3 W", false, 0, 0.7, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_ZENER[] = {
    FZLM("0.5 W", false, 0.5, 0, 0, 0), FZLM("1 W", false, 1, 0, 0, 0), FZLM("5 W", false, 5, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_NPN[] = {
    FZLM("2N2222", true, 0.5, 0.6, 40, 0), FZLM("2N3904", true, 0.625, 0.2, 40, 0), FZLM("BC547", true, 0.5, 0.1, 45, 0),
    FZLM("BC337", true, 0.625, 0.8, 45, 0), FZLM("TIP120", true, 2, 5, 60, 0), FZLM("TIP31", true, 2, 3, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_PNP[] = {
    FZLM("2N2907", true, 0.4, 0.6, 60, 0), FZLM("2N3906", true, 0.625, 0.2, 40, 0), FZLM("BC557", true, 0.5, 0.1, 45, 0),
    FZLM("BC327", true, 0.625, 0.8, 45, 0), FZLM("TIP125", true, 2, 5, 60, 0), FZLM("TIP32", true, 2, 3, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_NMOS[] = {
    FZLM("IRLZ44N", true, 2, 47, 55, 16), FZLM("2N7000", true, 0.4, 0.2, 60, 20), FZLM("BSS138", true, 0.36, 0.22, 50, 20),
    FZLM("IRF540N", true, 2, 33, 100, 20), FZLM("IRF520", true, 2, 9.2, 100, 20),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_PMOS[] = {
    FZLM("IRF9540", true, 2, 19, 100, 20), FZLM("BS250", true, 0.83, 0.18, 45, 20), FZLM("AO3401", true, 1.4, 4, 30, 12),
    FZLM("IRF9520", true, 2, 6.8, 100, 20),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SUPPLY[] = {
    FZLM("3 A", false, 0, 3, 0, 0), FZLM("1 A", false, 0, 1, 0, 0), FZLM("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_BATTERY[] = {
    FZLM("PP3 9 V", false, 0, 1, 0, 0), FZLM("AA", false, 0, 2, 0, 0), FZLM("CR2032", false, 0, 0.02, 0, 0),
    FZLM("18650", false, 0, 10, 0, 0), FZLM("12 V SLA", false, 0, 50, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SWITCH[] = {
    FZLM("3 A", false, 0, 3, 0, 0), FZLM("0.5 A", false, 0, 0.5, 0, 0), FZLM("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_BUTTON[] = {
    FZLM("6\xC3\x97" "6 mm", false, 0, 0.05, 0, 0), FZLM("1 A", false, 0, 1, 0, 0), FZLM("3 A", false, 0, 3, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_OPAMP[] = {
    FZLM("LM358", true, 0, 0.04, 32, 0), FZLM("TL072", true, 0, 0.04, 36, 0), FZLM("LM741", true, 0, 0.025, 36, 0),
    FZLM("MCP6002", true, 0, 0.02, 6, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_AMMETER[] = {
    FZLM("10 A", false, 0, 10, 0, 0), FZLM("200 mA", false, 0, 0.2, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SEVEN_SEG[] = {
    FZLM("0.56\"", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_LOGIC[] = {
    FZLM("74HC", true, 0, 0.025, 7, 0), FZLM("74LS", true, 0, 0.008, 7, 0), FZLM("CD40", true, 0, 0.01, 20, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_555[] = {
    FZLM("NE555", true, 0.6, 0.2, 16, 0), FZLM("TLC555", true, 0, 0.1, 15, 0), FZLM("LMC555", true, 0, 0.05, 15, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_REGULATOR[] = {
    FZLM("7805", true, 2, 1.5, 35, 0), FZLM("AMS1117", true, 1.2, 1, 15, 0), FZLM("7812", true, 2, 1.5, 35, 0),
    FZLM("LM317", true, 2, 1.5, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_595[] = {
    FZLM("74HC595", true, 0, 0.035, 7, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_293[] = {
    FZLM("L293D", true, 0, 0.6, 36, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_2003[] = {
    FZLM("ULN2003", true, 0, 0.5, 50, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_RELAY[] = {
    FZLM("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_PANEL[] = {
    FZLM("0.56\"", false, 0, 0.03, 0, 5), FZLM("0.36\"", false, 0, 0.025, 0, 5), FZLM("1\"", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_MATRIX[] = {
    FZLM("3 mm", false, 0, 0.02, 0, 5), FZLM("5 mm", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_BAR[] = {
    FZLM("10-segment", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_3914[] = {
    FZLM("LM3914", true, 0, 0.03, 25, 0), FZLM("LM3915", true, 0, 0.03, 25, 0), FZLM("LM3916", true, 0, 0.03, 25, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_LCD[] = {
    FZLM("HD44780", true, 0, 0.12, 7, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_LDR[] = {
    FZLM("GL5528", true, 0.1, 0, 150, 0), FZLM("GL5537", true, 0.1, 0, 150, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_NTC[] = {
    FZLM("MF52", true, 0.05, 0, 0, 0), FZLM("NTC 0.5 W", false, 0.5, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SERVO[] = {
    FZLM("SG90", true, 0, 1, 7, 0), FZLM("MG90S", true, 0, 1.2, 7, 0), FZLM("MG996R", true, 0, 3, 7.5, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_COMPARATOR[] = {
    FZLM("LM393", true, 0, 0.02, 36, 0), FZLM("LM339", true, 0, 0.02, 36, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SCR[] = {
    FZLM("C106", true, 0, 4, 400, 0), FZLM("2N5064", true, 0, 0.8, 200, 0), FZLM("BT151", true, 0, 12, 500, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_TRIAC[] = {
    FZLM("BT136", true, 0, 4, 600, 0), FZLM("BTA16", true, 0, 16, 600, 0), FZLM("Z0607", true, 0, 0.8, 600, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_XFMR[] = {
    FZLM("1 A", false, 0, 1, 0, 0), FZLM("3 A", false, 0, 3, 0, 0), FZLM("0.3 A", false, 0, 0.3, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_BRIDGE[] = {
    FZLM("DB107", true, 0, 1, 0, 1000), FZLM("KBP206", true, 0, 2, 0, 600), FZLM("GBU806", true, 0, 8, 0, 600),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_OPTO[] = {
    FZLM("PC817", true, 0, 0.05, 0, 6), FZLM("4N35", true, 0, 0.06, 0, 6),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_PHOTODIODE[] = {
    FZLM("BPW34", true, 0, 0, 0, 32),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_PHOTOT[] = {
    FZLM("TEPT5600", true, 0.1, 0.02, 6, 0), FZLM("BPW85", true, 0.1, 0.05, 70, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SPEAKER[] = {
    FZLM("0.5 W", false, 0.5, 0, 0, 0), FZLM("2 W", false, 2, 0, 0, 0), FZLM("10 W", false, 10, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_STEPPER[] = {
    FZLM("28BYJ-48", true, 0, 0.16, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_ENCODER[] = {
    FZLM("EC11", true, 0, 0.01, 0, 0), FZLM("KY-040", true, 0, 0.01, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZLM_SLOT[] = {
    FZLM("ITR9608", true, 0, 0.05, 0, 5), FZLM("H21A1", true, 0, 0.06, 0, 6), FZLM("TCST2103", true, 0, 0.06, 0, 6),
};

// See the header: a switch from the part's model to its table. A capacitor's table is the electrolytic's when its id
// is "electrolytic"; several kinds share one (the supplies, the switches, the logic).
const fude_zoom_limits_preset* fude_zoom_limits_presets(const fude_zoom_part* _part, u32* _count) {
    const fude_zoom_limits_preset* _t = NULL;
    u32 _n = 0;
#define FZLM_IS(_table) do { _t = _table; _n = FZLM_N(_table); } while(0)   // The kind's table is _table.
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:  FZLM_IS(FZLM_RESISTOR); break;
    case FUDE_ZOOM_MODEL_POT:       FZLM_IS(FZLM_POT); break;
    case FUDE_ZOOM_MODEL_CAPACITOR:
        if(rde_strcmp(_part->id, "electrolytic") == 0) {
            FZLM_IS(FZLM_ELECTROLYTIC);
        } else {
            FZLM_IS(FZLM_CAPACITOR);
        }
        break;
    case FUDE_ZOOM_MODEL_INDUCTOR:  FZLM_IS(FZLM_INDUCTOR); break;
    case FUDE_ZOOM_MODEL_DIODE:     FZLM_IS(FZLM_DIODE); break;
    case FUDE_ZOOM_MODEL_LED:       FZLM_IS(FZLM_LED); break;
    case FUDE_ZOOM_MODEL_ZENER:     FZLM_IS(FZLM_ZENER); break;
    case FUDE_ZOOM_MODEL_NPN:       FZLM_IS(FZLM_NPN); break;
    case FUDE_ZOOM_MODEL_PNP:       FZLM_IS(FZLM_PNP); break;
    case FUDE_ZOOM_MODEL_NMOS:      FZLM_IS(FZLM_NMOS); break;
    case FUDE_ZOOM_MODEL_PMOS:      FZLM_IS(FZLM_PMOS); break;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:      FZLM_IS(FZLM_SUPPLY); break;
    case FUDE_ZOOM_MODEL_BATTERY:   FZLM_IS(FZLM_BATTERY); break;
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:      FZLM_IS(FZLM_SWITCH); break;
    case FUDE_ZOOM_MODEL_LDR:       FZLM_IS(FZLM_LDR); break;
    case FUDE_ZOOM_MODEL_THERMISTOR: FZLM_IS(FZLM_NTC); break;
    case FUDE_ZOOM_MODEL_BUTTON:    FZLM_IS(FZLM_BUTTON); break;
    case FUDE_ZOOM_MODEL_OPAMP:     FZLM_IS(FZLM_OPAMP); break;
    case FUDE_ZOOM_MODEL_AMMETER:   FZLM_IS(FZLM_AMMETER); break;
    case FUDE_ZOOM_MODEL_SEVEN_SEG: FZLM_IS(FZLM_SEVEN_SEG); break;
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM:       FZLM_IS(FZLM_LOGIC); break;
    case FUDE_ZOOM_MODEL_TIMER555:  FZLM_IS(FZLM_555); break;
    case FUDE_ZOOM_MODEL_REGULATOR: FZLM_IS(FZLM_REGULATOR); break;
    case FUDE_ZOOM_MODEL_SHIFT595:  FZLM_IS(FZLM_595); break;
    case FUDE_ZOOM_MODEL_DRIVER293: FZLM_IS(FZLM_293); break;
    case FUDE_ZOOM_MODEL_ULN2003:   FZLM_IS(FZLM_2003); break;
    case FUDE_ZOOM_MODEL_RELAY:     FZLM_IS(FZLM_RELAY); break;
    case FUDE_ZOOM_MODEL_SERVO:     FZLM_IS(FZLM_SERVO); break;
    case FUDE_ZOOM_MODEL_STEPPER:   FZLM_IS(FZLM_STEPPER); break;
    case FUDE_ZOOM_MODEL_COMPARATOR: FZLM_IS(FZLM_COMPARATOR); break;
    case FUDE_ZOOM_MODEL_OPAMP2:    FZLM_IS(FZLM_OPAMP); break;
    case FUDE_ZOOM_MODEL_JKFF:
    case FUDE_ZOOM_MODEL_SRLATCH:   FZLM_IS(FZLM_LOGIC); break;
    case FUDE_ZOOM_MODEL_SCR:       FZLM_IS(FZLM_SCR); break;
    case FUDE_ZOOM_MODEL_TRIAC:     FZLM_IS(FZLM_TRIAC); break;
    case FUDE_ZOOM_MODEL_TRANSFORMER: FZLM_IS(FZLM_XFMR); break;
    case FUDE_ZOOM_MODEL_BRIDGE:    FZLM_IS(FZLM_BRIDGE); break;
    case FUDE_ZOOM_MODEL_OPTO:      FZLM_IS(FZLM_OPTO); break;
    case FUDE_ZOOM_MODEL_PHOTODIODE: FZLM_IS(FZLM_PHOTODIODE); break;
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR: FZLM_IS(FZLM_PHOTOT); break;
    case FUDE_ZOOM_MODEL_SPEAKER:   FZLM_IS(FZLM_SPEAKER); break;
    case FUDE_ZOOM_MODEL_ENCODER:   FZLM_IS(FZLM_ENCODER); break;
    case FUDE_ZOOM_MODEL_SLOT:      FZLM_IS(FZLM_SLOT); break;
    case FUDE_ZOOM_MODEL_SEG_PANEL: FZLM_IS(FZLM_PANEL); break;
    case FUDE_ZOOM_MODEL_LED_MATRIX: FZLM_IS(FZLM_MATRIX); break;
    case FUDE_ZOOM_MODEL_BAR_GRAPH: FZLM_IS(FZLM_BAR); break;
    case FUDE_ZOOM_MODEL_LM3914:    FZLM_IS(FZLM_3914); break;
    case FUDE_ZOOM_MODEL_CHAR_LCD:  FZLM_IS(FZLM_LCD); break;
    default: break;
    }
#undef FZLM_IS
    if(_count != NULL) {
        *_count = _n;
    }
    return _t;
}

// Returns whether _text names _name: a word of it starting so ("1N4007" in "D1 1N4007", "2N2222" in "2N2222A", the
// series "74HC" in "74HC00"; any case). A word starts after anything but a letter or a digit.
RDE_INTERNAL b8 fzlm_names(const c8* _text, const c8* _name) {
    const usize _n = rde_strlen(_name);
    for(const c8* _p = _text; _p != NULL && *_p != 0; _p++) {
        if(_p != _text && rde_char_is_alnum((u8)_p[-1])) {
            continue;   // A word's start only.
        }
        usize _k = 0;
        while(_k < _n && _p[_k] != 0 && rde_char_to_upper((u8)_p[_k]) == rde_char_to_upper((u8)_name[_k])) {
            _k++;
        }
        if(_k == _n) {
            return true;
        }
    }
    return false;
}

// See the header: the first of the kind's real part numbers the text names, in table order.
i32 fude_zoom_limits_named(const fude_zoom_part* _part, const c8* _text) {
    u32 _n = 0;
    const fude_zoom_limits_preset* _t = fude_zoom_limits_presets(_part, &_n);
    for(u32 _i = 0; _t != NULL && _text != NULL && _i < _n; _i++) {
        if(_t[_i].named && fzlm_names(_text, _t[_i].name)) {
            return (i32)_i;
        }
    }
    return -1;
}

// See the header. A kind with a table takes the real part its text names, else the one its kind's own text names (the
// chip it is drawn as), else its typical one. A kind with none may have limits that follow from its values; any other
// has none.
fude_zoom_limits fude_zoom_limits_typical(const fude_zoom_part* _part, const c8* _text, const f64* _value) {
    fude_zoom_limits _l;
    rde_memset(&_l, 0, sizeof(_l));
    if(_part == NULL) {
        return _l;
    }
    u32 _n = 0;
    const fude_zoom_limits_preset* _t = fude_zoom_limits_presets(_part, &_n);
    if(_t != NULL && _n > 0u) {
        const i32 _named = fude_zoom_limits_named(_part, _text);
        rde_memcpy(_l.most, _t[_named >= 0 ? (u32)_named : 0u].most, sizeof(_l.most));
        if(_named < 0 && fude_zoom_limits_named(_part, _part->value) >= 0) {
            // Its text names none, but its kind's own text does: the chip it is drawn as.
            rde_memcpy(_l.most, _t[fude_zoom_limits_named(_part, _part->value)].most, sizeof(_l.most));
        }
        return _l;
    }
    // Those whose limits follow from their values: a lamp half again its watts; a motor half again its volts, and half
    // the current it draws held still at them (held still for long, it burns); a buzzer half again its volts; a
    // solenoid's coil half again its watts at its rated volts.
    const f64 _v0 = _value != NULL ? _value[0] : 0.0, _v1 = _value != NULL ? _value[1] : 0.0;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_LAMP:
        _l.most[FUDE_ZOOM_LIMIT_POWER] = _v1 > 0.0 ? 1.5 * _v1 : 0.0;
        break;
    case FUDE_ZOOM_MODEL_MOTOR:
        _l.most[FUDE_ZOOM_LIMIT_VOLTAGE] = _v0 > 0.0 ? 1.5 * _v0 : 0.0;
        _l.most[FUDE_ZOOM_LIMIT_CURRENT] = _v0 > 0.0 ? 0.5 * _v0 / FUDE_ZOOM_MOTOR_R : 0.0;
        break;
    case FUDE_ZOOM_MODEL_BUZZER:
        _l.most[FUDE_ZOOM_LIMIT_VOLTAGE] = _v0 > 0.0 ? 1.5 * _v0 : 0.0;
        break;
    case FUDE_ZOOM_MODEL_SOLENOID:
        _l.most[FUDE_ZOOM_LIMIT_POWER] = _v0 > 0.0 && _v1 > 0.0 ? 1.5 * _v0 * _v0 / _v1 : 0.0;
        break;
    case FUDE_ZOOM_MODEL_PANEL_METER:
        // An ammeter half again its full scale (its shunt, its fuse); a voltmeter's input 250 V (twice a higher range).
        if(_value != NULL && _value[1] > 0.5) {
            _l.most[FUDE_ZOOM_LIMIT_CURRENT] = 1.5 * _v0;
        } else {
            _l.most[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_max_f64(250.0, 2.0 * _v0);
        }
        break;
    default:
        break;
    }
    return _l;
}

// Each limit's bit in fude_zoom_limits_kinds' answer: power, current, voltage, reverse (or gate) voltage.
#define FZLM_BIT_W (1u << FUDE_ZOOM_LIMIT_POWER)
#define FZLM_BIT_A (1u << FUDE_ZOOM_LIMIT_CURRENT)
#define FZLM_BIT_V (1u << FUDE_ZOOM_LIMIT_VOLTAGE)
#define FZLM_BIT_R (1u << FUDE_ZOOM_LIMIT_REVERSE)

// See the header: a switch from the part's model to the limits its card shows.
u32 fude_zoom_limits_kinds(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:
    case FUDE_ZOOM_MODEL_POT:
    case FUDE_ZOOM_MODEL_ZENER:
    case FUDE_ZOOM_MODEL_THERMISTOR:
    case FUDE_ZOOM_MODEL_SOLENOID:
    case FUDE_ZOOM_MODEL_LAMP:      return FZLM_BIT_W;
    case FUDE_ZOOM_MODEL_STEPPER:
    case FUDE_ZOOM_MODEL_ENCODER:
    case FUDE_ZOOM_MODEL_TRANSFORMER: return FZLM_BIT_A;
    case FUDE_ZOOM_MODEL_SLOT:
    case FUDE_ZOOM_MODEL_OPTO:
    case FUDE_ZOOM_MODEL_BRIDGE:    return FZLM_BIT_A | FZLM_BIT_R;
    case FUDE_ZOOM_MODEL_PHOTODIODE: return FZLM_BIT_R;
    case FUDE_ZOOM_MODEL_SPEAKER:   return FZLM_BIT_W;
    case FUDE_ZOOM_MODEL_SCR:
    case FUDE_ZOOM_MODEL_TRIAC:
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR:
    case FUDE_ZOOM_MODEL_COMPARATOR:
    case FUDE_ZOOM_MODEL_OPAMP2:
    case FUDE_ZOOM_MODEL_JKFF:
    case FUDE_ZOOM_MODEL_SRLATCH:   return FZLM_BIT_A | FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_LDR:       return FZLM_BIT_W | FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:      return FZLM_BIT_A;
    case FUDE_ZOOM_MODEL_CAPACITOR: return rde_strcmp(_part->id, "electrolytic") == 0 ? FZLM_BIT_V | FZLM_BIT_R : FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_INDUCTOR:
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:
    case FUDE_ZOOM_MODEL_BATTERY:
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:
    case FUDE_ZOOM_MODEL_AMMETER:
    case FUDE_ZOOM_MODEL_RELAY:     return FZLM_BIT_A;
    case FUDE_ZOOM_MODEL_DIODE:     return FZLM_BIT_W | FZLM_BIT_A | FZLM_BIT_R;
    case FUDE_ZOOM_MODEL_LED:
    case FUDE_ZOOM_MODEL_SEVEN_SEG:
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: return FZLM_BIT_A | FZLM_BIT_R;
    case FUDE_ZOOM_MODEL_LM3914:
    case FUDE_ZOOM_MODEL_PANEL_METER:
    case FUDE_ZOOM_MODEL_CHAR_LCD:  return FZLM_BIT_A | FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP:
    case FUDE_ZOOM_MODEL_TIMER555:
    case FUDE_ZOOM_MODEL_REGULATOR: return FZLM_BIT_W | FZLM_BIT_A | FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS:      return FZLM_BIT_W | FZLM_BIT_A | FZLM_BIT_V | FZLM_BIT_R;
    case FUDE_ZOOM_MODEL_MOTOR:
    case FUDE_ZOOM_MODEL_SERVO:
    case FUDE_ZOOM_MODEL_OPAMP:
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM:
    case FUDE_ZOOM_MODEL_SHIFT595:
    case FUDE_ZOOM_MODEL_DRIVER293:
    case FUDE_ZOOM_MODEL_ULN2003:   return FZLM_BIT_A | FZLM_BIT_V;
    case FUDE_ZOOM_MODEL_BUZZER:    return FZLM_BIT_V;
    default:                        return 0u;
    }
}

// See the header: a MOSFET's, N or P.
b8 fude_zoom_limits_gate(const fude_zoom_part* _part) {
    return _part != NULL && (_part->model == FUDE_ZOOM_MODEL_NMOS || _part->model == FUDE_ZOOM_MODEL_PMOS);
}

// See the header: 2 seconds for a resistor and its like, 20 for a motor or a winding, 0.05 for a semiconductor's
// junction (every kind not listed), and between for the rest.
f64 fude_zoom_limits_tau(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:
    case FUDE_ZOOM_MODEL_POT:
    case FUDE_ZOOM_MODEL_LDR:
    case FUDE_ZOOM_MODEL_THERMISTOR:
    case FUDE_ZOOM_MODEL_INDUCTOR:  return 2.0;
    case FUDE_ZOOM_MODEL_CAPACITOR: return 0.3;
    case FUDE_ZOOM_MODEL_LAMP:      return 0.2;
    case FUDE_ZOOM_MODEL_MOTOR:
    case FUDE_ZOOM_MODEL_SOLENOID:
    case FUDE_ZOOM_MODEL_STEPPER:
    case FUDE_ZOOM_MODEL_TRANSFORMER: return 20.0;
    case FUDE_ZOOM_MODEL_SPEAKER:   return 2.0;
    case FUDE_ZOOM_MODEL_BUZZER:
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:
    case FUDE_ZOOM_MODEL_BUTTON:
    case FUDE_ZOOM_MODEL_RELAY:
    case FUDE_ZOOM_MODEL_ENCODER:
    case FUDE_ZOOM_MODEL_SERVO:     return 1.0;
    case FUDE_ZOOM_MODEL_AMMETER:
    case FUDE_ZOOM_MODEL_PANEL_METER: return 0.5;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:
    case FUDE_ZOOM_MODEL_BATTERY:   return 0.0;   // A source: it says so, and never burns.
    default:                        return 0.05;  // A semiconductor's junction.
    }
}

// --- measured ---------------------------------------------------------------------------------------

// Returns 1 when a chip's pin _name is its supply's (a name starting "VCC", or "VDD" or "V+"), -1 when it is its
// ground's ("GND", "VSS" or "V-"), and 0 for any other or NULL.
RDE_INTERNAL i32 fzlm_supply(const c8* _name) {
    if(_name == NULL) {
        return 0;
    }
    if(rde_strncmp(_name, "VCC", 3) == 0 || rde_strcmp(_name, "VDD") == 0 || rde_strcmp(_name, "V+") == 0) {
        return 1;
    }
    if(rde_strcmp(_name, "GND") == 0 || rde_strcmp(_name, "VSS") == 0 || rde_strcmp(_name, "V-") == 0) {
        return -1;
    }
    return 0;
}

// Returns pin _a's volts over pin _b's, of the _pins in _volts; 0 when either is past them or is joined to nothing
// (its volts NaN: fude_zoom_limits_measure). Nothing flows through a pin joined to nothing, so nothing across it
// stresses the part: an LED whose cathode is on 9 V and whose anode is on nothing is not 9 V the wrong way round.
RDE_INTERNAL f64 fzlm_over(const f64* _volts, u32 _pins, u32 _a, u32 _b) {
    if(_a >= _pins || _b >= _pins || rde_math_is_nan_f64(_volts[_a]) || rde_math_is_nan_f64(_volts[_b])) {
        return 0.0;
    }
    return _volts[_a] - _volts[_b];
}

// A display's measures, LED by LED (display.h's two pins for each: a 7-segment panel's, an LED matrix's, a bar
// graph's): the worst LED's current, as much as both its pins carry at most (a bar graph's LED has its two pins to
// itself, so that is its own; a panel's or a matrix's pin carries a digit's or a row's LEDs, so it is at most that),
// and the most an LED's cathode is over its anode. The circuit measures each LED's own current again from its model,
// over its joined pins (circuit_step.c's fzc_display_measure). An LED with a pin past _pins is left out.
RDE_INTERNAL void fzlm_measure_leds(const fude_zoom_part* _part, const f64* _volts, const f64* _amps, u32 _pins, f64* _out) {
    const u32 _n = fude_zoom_display_lights(_part);
    for(u32 _l = 0; _l < _n; _l++) {
        u32 _a, _k;
        if(fude_zoom_display_led(_part, _l, &_a, &_k) && _a < _pins && _k < _pins) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_CURRENT], rde_math_min_f64(rde_math_abs_f64(_amps[_a]), rde_math_abs_f64(_amps[_k])));
            _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_REVERSE], fzlm_over(_volts, _pins, _k, _a));
        }
    }
}

// See the header. The power is what goes in at all its pins, at their volts (never below 0); the rest is each kind's,
// from the pins in the catalogue's order for it (circuit_catalogue.c). A pin past _pins reads 0, and a voltage across a
// pin joined to nothing (NaN) is 0 (fzlm_over).
void fude_zoom_limits_measure(const fude_zoom_part* _part, const f64* _volts, const f64* _amps, u32 _pins, f64* _out) {
    rde_memset(_out, 0, FUDE_ZOOM_LIMIT_COUNT * sizeof(f64));
    if(_part == NULL || _pins == 0u) {
        return;
    }
    f64 _heat = 0.0;
    for(u32 _k = 0; _k < _pins; _k++) {
        if(!rde_math_is_nan_f64(_volts[_k])) {
            _heat += _volts[_k] * _amps[_k];   // What goes in at each pin, at its volts: what it turns to heat, all told.
        }
    }
    _out[FUDE_ZOOM_LIMIT_POWER] = rde_math_max_f64(_heat, 0.0);
// Pin _a's volts over pin _b's (fzlm_over: 0 past the part's pins or across one joined to nothing), and the size of pin
// _k's current (0 past the part's pins).
#define FZLM_OVER(_a, _b) fzlm_over(_volts, _pins, (_a), (_b))
#define FZLM_AMP(_k) (_k < _pins ? rde_math_abs_f64(_amps[_k]) : 0.0)
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_CAPACITOR:
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_abs_f64(FZLM_OVER(0, 1));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(FZLM_OVER(1, 0), 0.0);   // Its + is the first pin.
        break;
    case FUDE_ZOOM_MODEL_DIODE:
    case FUDE_ZOOM_MODEL_LED:
    case FUDE_ZOOM_MODEL_ZENER:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(0);
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_abs_f64(FZLM_OVER(0, 1));
        // Its cathode over its anode.
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(FZLM_OVER(1, 0), 0.0);
        break;
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(1);   // B, C, E: its collector's.
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_abs_f64(FZLM_OVER(1, 2));
        break;
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(1);   // G, D, S: its drain's.
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_abs_f64(FZLM_OVER(1, 2));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_abs_f64(FZLM_OVER(0, 2));   // Its gate's, either way.
        break;
    case FUDE_ZOOM_MODEL_POT:
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:
        // Its worst pin's current.
        for(u32 _k = 0; _k < _pins; _k++) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_CURRENT], FZLM_AMP(_k));
        }
        break;
    case FUDE_ZOOM_MODEL_REGULATOR:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(2);   // IN, GND, OUT: what goes out.
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = FZLM_OVER(0, 1);
        break;
    case FUDE_ZOOM_MODEL_SEVEN_SEG:
        // Its eight segments (pins 0 to 7): the worst one's current, and the most its common pin (8) is over one.
        for(u32 _k = 0; _k < 8u && _k < _pins; _k++) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_CURRENT], FZLM_AMP(_k));
            _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_REVERSE], FZLM_OVER(8, _k));
        }
        break;
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH:
        fzlm_measure_leds(_part, _volts, _amps, _pins, _out);   // LED by LED
        break;
    case FUDE_ZOOM_MODEL_RELAY:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(3);   // Through its contacts: COM.
        break;
    case FUDE_ZOOM_MODEL_STEPPER:
        // Its worst coil's current (pins 0 to 3).
        for(u32 _k = 0; _k < 4u && _k < _pins; _k++) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_CURRENT], FZLM_AMP(_k));
        }
        break;
    case FUDE_ZOOM_MODEL_ENCODER:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(FZLM_AMP(0), FZLM_AMP(2));   // A's contact, B's.
        break;
    case FUDE_ZOOM_MODEL_SLOT:
    case FUDE_ZOOM_MODEL_OPTO:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(0);                       // Its LED's: A, K, C, E.
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(FZLM_OVER(1, 0), 0.0);
        break;
    case FUDE_ZOOM_MODEL_PHOTODIODE:
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(FZLM_OVER(1, 0), 0.0);
        break;
    case FUDE_ZOOM_MODEL_SCR:
    case FUDE_ZOOM_MODEL_TRIAC:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(0);                       // Through it: A or MT2.
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = rde_math_abs_f64(FZLM_OVER(0, 1));
        break;
    case FUDE_ZOOM_MODEL_TRANSFORMER:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(2);                       // Its secondary's.
        break;
    case FUDE_ZOOM_MODEL_BRIDGE: {
        // What it gives, out of +; each diode's backwards voltage: + over an AC pin, an AC pin over −.
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(1);
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(rde_math_max_f64(FZLM_OVER(1, 0), FZLM_OVER(1, 2)), rde_math_max_f64(FZLM_OVER(0, 3), FZLM_OVER(2, 3)));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_REVERSE], 0.0);
        break;
    }
    case FUDE_ZOOM_MODEL_CHAR_LCD:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(14);                 // Its backlight's: A.
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = FZLM_OVER(1, 0);   // Its supply: VDD over VSS.
        break;
    case FUDE_ZOOM_MODEL_OPAMP:
    case FUDE_ZOOM_MODEL_COMPARATOR:
    case FUDE_ZOOM_MODEL_OPAMP2:
    case FUDE_ZOOM_MODEL_JKFF:
    case FUDE_ZOOM_MODEL_SRLATCH:
    case FUDE_ZOOM_MODEL_TIMER555:
    case FUDE_ZOOM_MODEL_SHIFT595:
    case FUDE_ZOOM_MODEL_DRIVER293:
    case FUDE_ZOOM_MODEL_ULN2003:
    case FUDE_ZOOM_MODEL_LM3914:
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM: {
        // A chip: its supply's volts (its highest supply pin over its first ground pin; 0 without both), its worst
        // pin's current but the supply's and COM's (an output driving too much, an input pulled hard past its supply).
        f64 _ground = 0.0, _supply = -1e300;
        b8 _has_ground = false;
        for(u32 _k = 0; _k < _pins && _part->pins != NULL; _k++) {
            const i32 _s = fzlm_supply(_part->pins[_k].name);
            if(rde_math_is_nan_f64(_volts[_k])) {
                continue;   // (a supply or ground pin joined to nothing gives nothing)
            }
            if(_s < 0 && !_has_ground) {
                _ground = _volts[_k];
                _has_ground = true;
            } else if(_s > 0) {
                _supply = rde_math_max_f64(_supply, _volts[_k]);
            }
        }
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = _has_ground && _supply > -1e299 ? rde_math_max_f64(_supply - _ground, 0.0) : 0.0;
        for(u32 _k = 0; _k < _pins; _k++) {
            if(_part->pins == NULL || (fzlm_supply(_part->pins[_k].name) == 0 && rde_strcmp(_part->pins[_k].name, "COM") != 0)) {
                _out[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_max_f64(_out[FUDE_ZOOM_LIMIT_CURRENT], FZLM_AMP(_k));
            }
        }
        break;
    }
    default:
        // Two pins: through it, across it.
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZLM_AMP(0);
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = _pins >= 2u ? rde_math_abs_f64(FZLM_OVER(0, 1)) : 0.0;
        break;
    }
#undef FZLM_OVER
#undef FZLM_AMP
}

// See the header. The stress is the worst of its measures over their limits (a limit of 0 is none); on a tie the first
// limit in FUDE_ZOOM_LIMIT_ order is the worst. Below its limits its heat falls by dt / (4 tau) of itself each step.
b8 fude_zoom_limits_step(const fude_zoom_part* _part, const fude_zoom_limits* _limits, const f64* _measure, f64 _dt,
                         f64* _stress, u8* _worst, f64* _heat) {
    f64 _s = 0.0;
    u8 _w = 0;
    for(u32 _k = 0; _k < FUDE_ZOOM_LIMIT_COUNT; _k++) {
        if(_limits->most[_k] > 0.0 && _measure[_k] / _limits->most[_k] > _s) {
            _s = _measure[_k] / _limits->most[_k];
            _w = (u8)_k;
        }
    }
    *_stress = _s;
    *_worst  = _w;
    const f64 _tau = fude_zoom_limits_tau(_part);
    if(!(_tau > 0.0)) {
        // A source never burns: its heat is how long it has been past its limit.
        *_heat = _s > 1.0 ? *_heat + _dt : 0.0;
        return false;
    }
    if(_s > 1.0) {
        // At twice its limit it heats at three times its pace: burnt in a third of _tau.
        *_heat += (_s * _s - 1.0) * _dt / _tau;
    } else {
        *_heat -= *_heat * rde_math_min_f64(_dt / (4.0 * _tau), 1.0);   // Cooling, more slowly.
    }
    return *_heat >= 1.0;
}
