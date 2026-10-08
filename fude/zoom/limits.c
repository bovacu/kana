// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/limits.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

// --- real parts ---------------------------------------------------------------------------------------
//
// Each: its name, whether a part's text names it, and its power (W), current (A), voltage (V), reverse or gate voltage
// (V) at most — absolute maximum ratings, as their datasheets give them (the power a small part takes in free air,
// without a heatsink). Each kind's first is the typical one.

#define FZL(_name, _named, _w, _a, _v, _r) { _name, _named, { _w, _a, _v, _r } }
#define FZL_N(_t) (u32)(sizeof(_t) / sizeof(_t[0]))

RDE_INTERNAL const fude_zoom_limits_preset FZL_RESISTOR[] = {
    FZL("0.25 W", false, 0.25, 0, 0, 0), FZL("0.125 W", false, 0.125, 0, 0, 0), FZL("0.5 W", false, 0.5, 0, 0, 0),
    FZL("1 W", false, 1, 0, 0, 0), FZL("2 W", false, 2, 0, 0, 0), FZL("5 W", false, 5, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_POT[] = {
    FZL("0.25 W", false, 0.25, 0, 0, 0), FZL("0.1 W", false, 0.1, 0, 0, 0), FZL("0.5 W", false, 0.5, 0, 0, 0), FZL("2 W", false, 2, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_CAPACITOR[] = {
    FZL("50 V", false, 0, 0, 50, 0), FZL("16 V", false, 0, 0, 16, 0), FZL("25 V", false, 0, 0, 25, 0), FZL("100 V", false, 0, 0, 100, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_ELECTROLYTIC[] = {
    FZL("16 V", false, 0, 0, 16, 1), FZL("6.3 V", false, 0, 0, 6.3, 1), FZL("10 V", false, 0, 0, 10, 1),
    FZL("25 V", false, 0, 0, 25, 1), FZL("35 V", false, 0, 0, 35, 1), FZL("50 V", false, 0, 0, 50, 1),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_INDUCTOR[] = {
    FZL("1 A", false, 0, 1, 0, 0), FZL("0.5 A", false, 0, 0.5, 0, 0), FZL("3 A", false, 0, 3, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_DIODE[] = {
    FZL("1N4148", true, 0.5, 0.3, 0, 100), FZL("1N4007", true, 0, 1, 0, 1000), FZL("1N4001", true, 0, 1, 0, 50),
    FZL("1N5819", true, 0, 1, 0, 40), FZL("1N5408", true, 0, 3, 0, 1000),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_LED[] = {
    FZL("5 mm", false, 0, 0.03, 0, 5), FZL("3 mm", false, 0, 0.03, 0, 5), FZL("SMD 0805", false, 0, 0.025, 0, 5),
    FZL("1 W", false, 0, 0.35, 0, 5), FZL("3 W", false, 0, 0.7, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_ZENER[] = {
    FZL("0.5 W", false, 0.5, 0, 0, 0), FZL("1 W", false, 1, 0, 0, 0), FZL("5 W", false, 5, 0, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_NPN[] = {
    FZL("2N2222", true, 0.5, 0.6, 40, 0), FZL("2N3904", true, 0.625, 0.2, 40, 0), FZL("BC547", true, 0.5, 0.1, 45, 0),
    FZL("BC337", true, 0.625, 0.8, 45, 0), FZL("TIP120", true, 2, 5, 60, 0), FZL("TIP31", true, 2, 3, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_PNP[] = {
    FZL("2N2907", true, 0.4, 0.6, 60, 0), FZL("2N3906", true, 0.625, 0.2, 40, 0), FZL("BC557", true, 0.5, 0.1, 45, 0),
    FZL("BC327", true, 0.625, 0.8, 45, 0), FZL("TIP125", true, 2, 5, 60, 0), FZL("TIP32", true, 2, 3, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_NMOS[] = {
    FZL("IRLZ44N", true, 2, 47, 55, 16), FZL("2N7000", true, 0.4, 0.2, 60, 20), FZL("BSS138", true, 0.36, 0.22, 50, 20),
    FZL("IRF540N", true, 2, 33, 100, 20), FZL("IRF520", true, 2, 9.2, 100, 20),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_PMOS[] = {
    FZL("IRF9540", true, 2, 19, 100, 20), FZL("BS250", true, 0.83, 0.18, 45, 20), FZL("AO3401", true, 1.4, 4, 30, 12),
    FZL("IRF9520", true, 2, 6.8, 100, 20),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_SUPPLY[] = {
    FZL("3 A", false, 0, 3, 0, 0), FZL("1 A", false, 0, 1, 0, 0), FZL("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_BATTERY[] = {
    FZL("PP3 9 V", false, 0, 1, 0, 0), FZL("AA", false, 0, 2, 0, 0), FZL("CR2032", false, 0, 0.02, 0, 0),
    FZL("18650", false, 0, 10, 0, 0), FZL("12 V SLA", false, 0, 50, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_SWITCH[] = {
    FZL("3 A", false, 0, 3, 0, 0), FZL("0.5 A", false, 0, 0.5, 0, 0), FZL("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_BUTTON[] = {
    FZL("6\xC3\x97" "6 mm", false, 0, 0.05, 0, 0), FZL("1 A", false, 0, 1, 0, 0), FZL("3 A", false, 0, 3, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_OPAMP[] = {
    FZL("LM358", true, 0, 0.04, 32, 0), FZL("TL072", true, 0, 0.04, 36, 0), FZL("LM741", true, 0, 0.025, 36, 0),
    FZL("MCP6002", true, 0, 0.02, 6, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_AMMETER[] = {
    FZL("10 A", false, 0, 10, 0, 0), FZL("200 mA", false, 0, 0.2, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_SEVEN_SEG[] = {
    FZL("0.56\"", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_LOGIC[] = {
    FZL("74HC", true, 0, 0.025, 7, 0), FZL("74LS", true, 0, 0.008, 7, 0), FZL("CD40", true, 0, 0.01, 20, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_555[] = {
    FZL("NE555", true, 0.6, 0.2, 16, 0), FZL("TLC555", true, 0, 0.1, 15, 0), FZL("LMC555", true, 0, 0.05, 15, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_REGULATOR[] = {
    FZL("7805", true, 2, 1.5, 35, 0), FZL("AMS1117", true, 1.2, 1, 15, 0), FZL("7812", true, 2, 1.5, 35, 0),
    FZL("LM317", true, 2, 1.5, 40, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_595[] = {
    FZL("74HC595", true, 0, 0.035, 7, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_293[] = {
    FZL("L293D", true, 0, 0.6, 36, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_2003[] = {
    FZL("ULN2003", true, 0, 0.5, 50, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_RELAY[] = {
    FZL("10 A", false, 0, 10, 0, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_PANEL[] = {
    FZL("0.56\"", false, 0, 0.03, 0, 5), FZL("0.36\"", false, 0, 0.025, 0, 5), FZL("1\"", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_MATRIX[] = {
    FZL("3 mm", false, 0, 0.02, 0, 5), FZL("5 mm", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_BAR[] = {
    FZL("10-segment", false, 0, 0.03, 0, 5),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_3914[] = {
    FZL("LM3914", true, 0, 0.03, 25, 0), FZL("LM3915", true, 0, 0.03, 25, 0), FZL("LM3916", true, 0, 0.03, 25, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_LCD[] = {
    FZL("HD44780", true, 0, 0.12, 7, 0),
};
RDE_INTERNAL const fude_zoom_limits_preset FZL_SERVO[] = {
    FZL("SG90", true, 0, 1, 7, 0), FZL("MG90S", true, 0, 1.2, 7, 0), FZL("MG996R", true, 0, 3, 7.5, 0),
};

const fude_zoom_limits_preset* fude_zoom_limits_presets(const fude_zoom_part* _part, u32* _count) {
    const fude_zoom_limits_preset* _t = NULL;
    u32 _n = 0;
#define FZL_IS(_table) do { _t = _table; _n = FZL_N(_table); } while(0)
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:  FZL_IS(FZL_RESISTOR); break;
    case FUDE_ZOOM_MODEL_POT:       FZL_IS(FZL_POT); break;
    case FUDE_ZOOM_MODEL_CAPACITOR:
        if(strcmp(_part->id, "electrolytic") == 0) {
            FZL_IS(FZL_ELECTROLYTIC);
        } else {
            FZL_IS(FZL_CAPACITOR);
        }
        break;
    case FUDE_ZOOM_MODEL_INDUCTOR:  FZL_IS(FZL_INDUCTOR); break;
    case FUDE_ZOOM_MODEL_DIODE:     FZL_IS(FZL_DIODE); break;
    case FUDE_ZOOM_MODEL_LED:       FZL_IS(FZL_LED); break;
    case FUDE_ZOOM_MODEL_ZENER:     FZL_IS(FZL_ZENER); break;
    case FUDE_ZOOM_MODEL_NPN:       FZL_IS(FZL_NPN); break;
    case FUDE_ZOOM_MODEL_PNP:       FZL_IS(FZL_PNP); break;
    case FUDE_ZOOM_MODEL_NMOS:      FZL_IS(FZL_NMOS); break;
    case FUDE_ZOOM_MODEL_PMOS:      FZL_IS(FZL_PMOS); break;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:      FZL_IS(FZL_SUPPLY); break;
    case FUDE_ZOOM_MODEL_BATTERY:   FZL_IS(FZL_BATTERY); break;
    case FUDE_ZOOM_MODEL_SWITCH:    FZL_IS(FZL_SWITCH); break;
    case FUDE_ZOOM_MODEL_BUTTON:    FZL_IS(FZL_BUTTON); break;
    case FUDE_ZOOM_MODEL_OPAMP:     FZL_IS(FZL_OPAMP); break;
    case FUDE_ZOOM_MODEL_AMMETER:   FZL_IS(FZL_AMMETER); break;
    case FUDE_ZOOM_MODEL_SEVEN_SEG: FZL_IS(FZL_SEVEN_SEG); break;
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM:       FZL_IS(FZL_LOGIC); break;
    case FUDE_ZOOM_MODEL_TIMER555:  FZL_IS(FZL_555); break;
    case FUDE_ZOOM_MODEL_REGULATOR: FZL_IS(FZL_REGULATOR); break;
    case FUDE_ZOOM_MODEL_SHIFT595:  FZL_IS(FZL_595); break;
    case FUDE_ZOOM_MODEL_DRIVER293: FZL_IS(FZL_293); break;
    case FUDE_ZOOM_MODEL_ULN2003:   FZL_IS(FZL_2003); break;
    case FUDE_ZOOM_MODEL_RELAY:     FZL_IS(FZL_RELAY); break;
    case FUDE_ZOOM_MODEL_SERVO:     FZL_IS(FZL_SERVO); break;
    case FUDE_ZOOM_MODEL_SEG_PANEL: FZL_IS(FZL_PANEL); break;
    case FUDE_ZOOM_MODEL_LED_MATRIX: FZL_IS(FZL_MATRIX); break;
    case FUDE_ZOOM_MODEL_BAR_GRAPH: FZL_IS(FZL_BAR); break;
    case FUDE_ZOOM_MODEL_LM3914:    FZL_IS(FZL_3914); break;
    case FUDE_ZOOM_MODEL_CHAR_LCD:  FZL_IS(FZL_LCD); break;
    default: break;
    }
#undef FZL_IS
    if(_count != NULL) {
        *_count = _n;
    }
    return _t;
}

// Does _text name _name — a word of it starting so ("1N4007" in "D1 1N4007", "2N2222" in "2N2222A", the series "74HC"
// in "74HC00"; any case)?
RDE_INTERNAL b8 fzl_names(const c8* _text, const c8* _name) {
    const usize _n = strlen(_name);
    for(const c8* _p = _text; _p != NULL && *_p != 0; _p++) {
        if(_p != _text && isalnum((u8)_p[-1])) {
            continue;   // (a word's start only)
        }
        usize _k = 0;
        while(_k < _n && _p[_k] != 0 && toupper((u8)_p[_k]) == toupper((u8)_name[_k])) {
            _k++;
        }
        if(_k == _n) {
            return true;
        }
    }
    return false;
}

i32 fude_zoom_limits_named(const fude_zoom_part* _part, const c8* _text) {
    u32 _n = 0;
    const fude_zoom_limits_preset* _t = fude_zoom_limits_presets(_part, &_n);
    for(u32 _i = 0; _t != NULL && _text != NULL && _i < _n; _i++) {
        if(_t[_i].named && fzl_names(_text, _t[_i].name)) {
            return (i32)_i;
        }
    }
    return -1;
}

fude_zoom_limits fude_zoom_limits_typical(const fude_zoom_part* _part, const c8* _text, const f64* _value) {
    fude_zoom_limits _l;
    memset(&_l, 0, sizeof(_l));
    if(_part == NULL) {
        return _l;
    }
    u32 _n = 0;
    const fude_zoom_limits_preset* _t = fude_zoom_limits_presets(_part, &_n);
    if(_t != NULL && _n > 0u) {
        const i32 _named = fude_zoom_limits_named(_part, _text);
        memcpy(_l.most, _t[_named >= 0 ? (u32)_named : 0u].most, sizeof(_l.most));
        if(_named < 0 && fude_zoom_limits_named(_part, _part->value) >= 0) {
            // (its text names none, but its kind's own text does: the chip it is drawn as)
            memcpy(_l.most, _t[fude_zoom_limits_named(_part, _part->value)].most, sizeof(_l.most));
        }
        return _l;
    }
    // Those whose limits follow from their values: a lamp half again its watts; a motor half again its volts, and half
    // the current it draws held still at them (held still for long, it burns); a buzzer half again its volts.
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
    case FUDE_ZOOM_MODEL_PANEL_METER:
        // An ammeter half again its full scale (its shunt, its fuse); a voltmeter's input 250 V (twice a higher range).
        if(_value != NULL && _value[1] > 0.5) {
            _l.most[FUDE_ZOOM_LIMIT_CURRENT] = 1.5 * _v0;
        } else {
            _l.most[FUDE_ZOOM_LIMIT_VOLTAGE] = fmax(250.0, 2.0 * _v0);
        }
        break;
    default:
        break;
    }
    return _l;
}

#define FZL_BIT_W (1u << FUDE_ZOOM_LIMIT_POWER)
#define FZL_BIT_A (1u << FUDE_ZOOM_LIMIT_CURRENT)
#define FZL_BIT_V (1u << FUDE_ZOOM_LIMIT_VOLTAGE)
#define FZL_BIT_R (1u << FUDE_ZOOM_LIMIT_REVERSE)

u32 fude_zoom_limits_kinds(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:
    case FUDE_ZOOM_MODEL_POT:
    case FUDE_ZOOM_MODEL_ZENER:
    case FUDE_ZOOM_MODEL_LAMP:      return FZL_BIT_W;
    case FUDE_ZOOM_MODEL_CAPACITOR: return strcmp(_part->id, "electrolytic") == 0 ? FZL_BIT_V | FZL_BIT_R : FZL_BIT_V;
    case FUDE_ZOOM_MODEL_INDUCTOR:
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:
    case FUDE_ZOOM_MODEL_BATTERY:
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:
    case FUDE_ZOOM_MODEL_AMMETER:
    case FUDE_ZOOM_MODEL_RELAY:     return FZL_BIT_A;
    case FUDE_ZOOM_MODEL_DIODE:     return FZL_BIT_W | FZL_BIT_A | FZL_BIT_R;
    case FUDE_ZOOM_MODEL_LED:
    case FUDE_ZOOM_MODEL_SEVEN_SEG:
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: return FZL_BIT_A | FZL_BIT_R;
    case FUDE_ZOOM_MODEL_LM3914:
    case FUDE_ZOOM_MODEL_PANEL_METER:
    case FUDE_ZOOM_MODEL_CHAR_LCD:  return FZL_BIT_A | FZL_BIT_V;
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP:
    case FUDE_ZOOM_MODEL_TIMER555:
    case FUDE_ZOOM_MODEL_REGULATOR: return FZL_BIT_W | FZL_BIT_A | FZL_BIT_V;
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS:      return FZL_BIT_W | FZL_BIT_A | FZL_BIT_V | FZL_BIT_R;
    case FUDE_ZOOM_MODEL_MOTOR:
    case FUDE_ZOOM_MODEL_SERVO:
    case FUDE_ZOOM_MODEL_OPAMP:
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM:
    case FUDE_ZOOM_MODEL_SHIFT595:
    case FUDE_ZOOM_MODEL_DRIVER293:
    case FUDE_ZOOM_MODEL_ULN2003:   return FZL_BIT_A | FZL_BIT_V;
    case FUDE_ZOOM_MODEL_BUZZER:    return FZL_BIT_V;
    default:                        return 0u;
    }
}

b8 fude_zoom_limits_gate(const fude_zoom_part* _part) {
    return _part != NULL && (_part->model == FUDE_ZOOM_MODEL_NMOS || _part->model == FUDE_ZOOM_MODEL_PMOS);
}

f64 fude_zoom_limits_tau(const fude_zoom_part* _part) {
    switch(_part != NULL ? _part->model : FUDE_ZOOM_MODEL_NONE) {
    case FUDE_ZOOM_MODEL_RESISTOR:
    case FUDE_ZOOM_MODEL_POT:
    case FUDE_ZOOM_MODEL_INDUCTOR:  return 2.0;
    case FUDE_ZOOM_MODEL_CAPACITOR: return 0.3;
    case FUDE_ZOOM_MODEL_LAMP:      return 0.2;
    case FUDE_ZOOM_MODEL_MOTOR:     return 20.0;
    case FUDE_ZOOM_MODEL_BUZZER:
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:
    case FUDE_ZOOM_MODEL_RELAY:
    case FUDE_ZOOM_MODEL_SERVO:     return 1.0;
    case FUDE_ZOOM_MODEL_AMMETER:
    case FUDE_ZOOM_MODEL_PANEL_METER: return 0.5;
    case FUDE_ZOOM_MODEL_VSOURCE:
    case FUDE_ZOOM_MODEL_ACSOURCE:
    case FUDE_ZOOM_MODEL_RAIL:
    case FUDE_ZOOM_MODEL_BATTERY:   return 0.0;   // (a source: says so, never burns)
    default:                        return 0.05;  // (a semiconductor's junction)
    }
}

// --- measured ---------------------------------------------------------------------------------------

// Is a chip's pin _name its supply's (+), its ground's (−), or another?
RDE_INTERNAL i32 fzl_supply(const c8* _name) {
    if(_name == NULL) {
        return 0;
    }
    if(strncmp(_name, "VCC", 3) == 0 || strcmp(_name, "VDD") == 0 || strcmp(_name, "V+") == 0) {
        return 1;
    }
    if(strcmp(_name, "GND") == 0 || strcmp(_name, "VSS") == 0 || strcmp(_name, "V-") == 0) {
        return -1;
    }
    return 0;
}

void fude_zoom_limits_measure(const fude_zoom_part* _part, const f64* _volts, const f64* _amps, u32 _pins, f64* _out) {
    memset(_out, 0, FUDE_ZOOM_LIMIT_COUNT * sizeof(f64));
    if(_part == NULL || _pins == 0u) {
        return;
    }
    f64 _heat = 0.0;
    for(u32 _k = 0; _k < _pins; _k++) {
        _heat += _volts[_k] * _amps[_k];   // (what goes in at each pin, at its volts: what it turns to heat, all told)
    }
    _out[FUDE_ZOOM_LIMIT_POWER] = fmax(_heat, 0.0);
#define FZL_VOLT(_k) (_k < _pins ? _volts[_k] : 0.0)
#define FZL_AMP(_k) (_k < _pins ? fabs(_amps[_k]) : 0.0)
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_CAPACITOR:
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = fabs(FZL_VOLT(0) - FZL_VOLT(1));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = fmax(FZL_VOLT(1) - FZL_VOLT(0), 0.0);   // (its + the first pin)
        break;
    case FUDE_ZOOM_MODEL_DIODE:
    case FUDE_ZOOM_MODEL_LED:
    case FUDE_ZOOM_MODEL_ZENER:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(0);
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = fabs(FZL_VOLT(0) - FZL_VOLT(1));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = fmax(FZL_VOLT(1) - FZL_VOLT(0), 0.0);   // (its cathode over its anode)
        break;
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(1);   // (B, C, E: its collector's)
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = fabs(FZL_VOLT(1) - FZL_VOLT(2));
        break;
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(1);   // (G, D, S: its drain's)
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = fabs(FZL_VOLT(1) - FZL_VOLT(2));
        _out[FUDE_ZOOM_LIMIT_REVERSE] = fabs(FZL_VOLT(0) - FZL_VOLT(2));   // (its gate's, either way)
        break;
    case FUDE_ZOOM_MODEL_POT:
        for(u32 _k = 0; _k < _pins; _k++) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = fmax(_out[FUDE_ZOOM_LIMIT_CURRENT], FZL_AMP(_k));
        }
        break;
    case FUDE_ZOOM_MODEL_REGULATOR:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(2);   // (IN, GND, OUT: what goes out)
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = FZL_VOLT(0) - FZL_VOLT(1);
        break;
    case FUDE_ZOOM_MODEL_SEVEN_SEG:
        for(u32 _k = 0; _k < 8u && _k < _pins; _k++) {
            _out[FUDE_ZOOM_LIMIT_CURRENT] = fmax(_out[FUDE_ZOOM_LIMIT_CURRENT], FZL_AMP(_k));   // (each segment's)
            _out[FUDE_ZOOM_LIMIT_REVERSE] = fmax(_out[FUDE_ZOOM_LIMIT_REVERSE], FZL_VOLT(8) - FZL_VOLT(_k));
        }
        break;
    case FUDE_ZOOM_MODEL_RELAY:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(3);   // (through its contacts: COM)
        break;
    case FUDE_ZOOM_MODEL_CHAR_LCD:
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(14);                 // (its backlight's: A)
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = FZL_VOLT(1) - FZL_VOLT(0);   // (its supply: VDD over VSS)
        break;
    case FUDE_ZOOM_MODEL_OPAMP:
    case FUDE_ZOOM_MODEL_TIMER555:
    case FUDE_ZOOM_MODEL_SHIFT595:
    case FUDE_ZOOM_MODEL_DRIVER293:
    case FUDE_ZOOM_MODEL_ULN2003:
    case FUDE_ZOOM_MODEL_LM3914:
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_SIM: {
        // A chip: its supply's volts (its highest over its ground), its worst pin's current but the supply's (an output
        // driving too much, an input pulled hard past its supply).
        f64 _ground = 0.0, _supply = -1e300;
        b8 _has_ground = false;
        for(u32 _k = 0; _k < _pins && _part->pins != NULL; _k++) {
            const i32 _s = fzl_supply(_part->pins[_k].name);
            if(_s < 0 && !_has_ground) {
                _ground = _volts[_k];
                _has_ground = true;
            } else if(_s > 0) {
                _supply = fmax(_supply, _volts[_k]);
            }
        }
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = _has_ground && _supply > -1e299 ? fmax(_supply - _ground, 0.0) : 0.0;
        for(u32 _k = 0; _k < _pins; _k++) {
            if(_part->pins == NULL || (fzl_supply(_part->pins[_k].name) == 0 && strcmp(_part->pins[_k].name, "COM") != 0)) {
                _out[FUDE_ZOOM_LIMIT_CURRENT] = fmax(_out[FUDE_ZOOM_LIMIT_CURRENT], FZL_AMP(_k));
            }
        }
        break;
    }
    default:
        // Two pins: through it, across it.
        _out[FUDE_ZOOM_LIMIT_CURRENT] = FZL_AMP(0);
        _out[FUDE_ZOOM_LIMIT_VOLTAGE] = _pins >= 2u ? fabs(FZL_VOLT(0) - FZL_VOLT(1)) : 0.0;
        break;
    }
#undef FZL_VOLT
#undef FZL_AMP
}

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
        *_heat = _s > 1.0 ? *_heat + _dt : 0.0;   // (a source: never burns — its heat how long it has been past its limit)
        return false;
    }
    if(_s > 1.0) {
        *_heat += (_s * _s - 1.0) * _dt / _tau;   // (twice its limit: three times its pace — burnt in a third of _tau)
    } else {
        *_heat -= *_heat * fmin(_dt / (4.0 * _tau), 1.0);   // (cooling, more slowly)
    }
    return *_heat >= 1.0;
}
