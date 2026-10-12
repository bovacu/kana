// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/adpcm.h"

#include <string.h>

static const i16 FZA_STEPS[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552,
    1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};
static const i8 FZA_MOVES[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

RDE_INTERNAL i32 fza_clamp(i32 _v, i32 _lo, i32 _hi) {
    return _v < _lo ? _lo : _v > _hi ? _hi : _v;
}

// A nibble's step from the predictor (the decoder's sum, exactly as it does it).
RDE_INTERNAL i32 fza_apply(i32 _predictor, i32 _index, u8 _nibble) {
    const i32 _step = FZA_STEPS[_index];
    i32 _diff = _step >> 3;
    if(_nibble & 4u) { _diff += _step; }
    if(_nibble & 2u) { _diff += _step >> 1; }
    if(_nibble & 1u) { _diff += _step >> 2; }
    return fza_clamp((_nibble & 8u) ? _predictor - _diff : _predictor + _diff, -32768, 32767);
}

void fude_zoom_adpcm_encode(fude_zoom_adpcm_state* _state, const i16* _in, u32 _frames, u8* _out) {
    memset(_out, 0, FUDE_ZOOM_ADPCM_BLOCK);
    if(_frames == 0u) {
        return;
    }
    _frames = _frames > FUDE_ZOOM_ADPCM_FRAMES ? FUDE_ZOOM_ADPCM_FRAMES : _frames;
    i32 _predictor = _in[0];
    i32 _index     = fza_clamp(_state->index, 0, 88);
    _out[0] = (u8)(_predictor & 0xFF);
    _out[1] = (u8)((_predictor >> 8) & 0xFF);
    _out[2] = (u8)_index;
    _out[3] = 0u;
    for(u32 _k = 1; _k < FUDE_ZOOM_ADPCM_FRAMES; _k++) {
        // (past the samples given: the last one held, so the block's tail is quiet)
        const i32 _sample = (i32)_in[_k < _frames ? _k : _frames - 1u];
        const i32 _step   = FZA_STEPS[_index];
        i32 _diff = _sample - _predictor;
        u8  _nib  = 0u;
        if(_diff < 0) {
            _nib  = 8u;
            _diff = -_diff;
        }
        if(_diff >= _step) { _nib |= 4u; _diff -= _step; }
        if(_diff >= _step >> 1) { _nib |= 2u; _diff -= _step >> 1; }
        if(_diff >= _step >> 2) { _nib |= 1u; }
        _predictor = fza_apply(_predictor, _index, _nib);
        _index     = fza_clamp(_index + FZA_MOVES[_nib], 0, 88);
        const u32 _at = 4u + (_k - 1u) / 2u;
        _out[_at] |= (u8)(((_k - 1u) & 1u) ? (_nib << 4) : _nib);
    }
    _state->index = _index;
}

void fude_zoom_adpcm_decode(const u8* _in, i16* _out) {
    i32 _predictor = (i16)((u16)_in[0] | ((u16)_in[1] << 8));
    i32 _index     = fza_clamp((i32)_in[2], 0, 88);
    _out[0] = (i16)_predictor;
    for(u32 _k = 1; _k < FUDE_ZOOM_ADPCM_FRAMES; _k++) {
        const u8 _byte = _in[4u + (_k - 1u) / 2u];
        const u8 _nib  = ((_k - 1u) & 1u) ? (u8)(_byte >> 4) : (u8)(_byte & 0x0Fu);
        _predictor = fza_apply(_predictor, _index, _nib);
        _index     = fza_clamp(_index + FZA_MOVES[_nib], 0, 88);
        _out[_k]   = (i16)_predictor;
    }
}
