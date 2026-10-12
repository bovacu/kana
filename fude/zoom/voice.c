// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/voice.h"

#include <math.h>
#include <string.h>

#define FZV_AHEAD   0.3    // seconds kept queued ahead of the speaker
#define FZV_SYNC    2.0    // seconds between a recording's file synced to disk
#define FZV_DEVICE  1.0    // seconds the devices' rings hold

// --- recording -----------------------------------------------------------------------------------

b8 fude_zoom_voice_record(fude_zoom_voice_recorder* _r, const c8* _path, u32 _rate) {
    memset(_r, 0, sizeof(*_r));
    _r->device = rde_audio_recorder_open(_rate, 1u, (u32)((f64)_rate * FZV_DEVICE), NULL);
    if(_r->device == NULL) {
        return false;
    }
    _r->rate = rde_audio_recorder_get_sample_rate(_r->device);
    RDE_TRY({ _r->file = rde_file_open(_path, RDE_FILE_MODE_APPEND_BYTES); });
    if(_r->file == NULL || _r->rate != _rate || rde_audio_recorder_get_channels(_r->device) != 1u || !rde_audio_recorder_start(_r->device)) {
        // (a device that will not give what the file says: none rather than a recording played at the wrong speed)
        if(_r->file != NULL) {
            rde_file_close(_r->file);
        }
        rde_audio_recorder_close(_r->device);
        memset(_r, 0, sizeof(*_r));
        return false;
    }
    _r->buffer_cap = _rate / 4u;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _r->buffer    = (f32*)_heap->malloc(_heap->allocator, sizeof(f32) * _r->buffer_cap);
    _r->synced_at = rde_engine_get_time_now();
    return _r->buffer != NULL;
}

b8 fude_zoom_voice_recording(const fude_zoom_voice_recorder* _r) {
    return _r->device != NULL;
}

RDE_INTERNAL void fzv_write_block(fude_zoom_voice_recorder* _r) {
    u8 _block[FUDE_ZOOM_ADPCM_BLOCK];
    fude_zoom_adpcm_encode(&_r->state, _r->pending, _r->pending_n, _block);
    rde_file_write(_r->file, (const c8*)_block, FUDE_ZOOM_ADPCM_BLOCK);
    _r->pending_n = 0;
}

void fude_zoom_voice_record_pump(fude_zoom_voice_recorder* _r) {
    if(_r->device == NULL) {
        return;
    }
    f64 _sum = 0.0;
    u32 _got_all = 0;
    for(u32 _rounds = 0; _rounds < 16u; _rounds++) {
        const u32 _got = rde_audio_recorder_read(_r->device, _r->buffer, _r->buffer_cap);
        if(_got == 0u) {
            break;
        }
        for(u32 _i = 0; _i < _got; _i++) {
            const f32 _s = _r->buffer[_i];
            _sum += (f64)_s * (f64)_s;
            const f32 _c = _s > 1.0f ? 1.0f : _s < -1.0f ? -1.0f : _s;
            _r->pending[_r->pending_n++] = (i16)lrintf(_c * 32767.0f);
            if(_r->pending_n == FUDE_ZOOM_ADPCM_FRAMES) {
                fzv_write_block(_r);
            }
        }
        _r->frames += _got;
        _got_all   += _got;
    }
    if(_got_all > 0u) {
        _r->level = (f32)fmin(sqrt(_sum / (f64)_got_all) * 4.0, 1.0);
    }
    const f64 _now = rde_engine_get_time_now();
    if(_now - _r->synced_at >= FZV_SYNC) {
        rde_file_sync(_r->file);   // (what is said is on disk within a moment, whatever stops the app)
        _r->synced_at = _now;
    }
}

u32 fude_zoom_voice_record_stop(fude_zoom_voice_recorder* _r) {
    if(_r->device == NULL) {
        return 0u;
    }
    rde_audio_recorder_stop(_r->device);
    fude_zoom_voice_record_pump(_r);   // (what was still in the device's ring)
    // (its last block filled out: a recording is whole blocks, its length theirs)
    if(_r->pending_n > 0u) {
        _r->frames += FUDE_ZOOM_ADPCM_FRAMES - _r->pending_n;
        fzv_write_block(_r);
    }
    rde_file_sync(_r->file);
    rde_file_close(_r->file);
    rde_audio_recorder_close(_r->device);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_r->buffer != NULL) {
        _heap->free(_heap->allocator, _r->buffer);
    }
    const u32 _frames = _r->frames;
    memset(_r, 0, sizeof(*_r));
    return _frames;
}

// --- playing -------------------------------------------------------------------------------------

b8 fude_zoom_voice_open(fude_zoom_voice_player* _p, const c8* _path, u32 _rate, u32 _frames) {
    memset(_p, 0, sizeof(*_p));
    if(!rde_file_exists(_path) || _rate == 0u) {
        return false;
    }
    RDE_TRY({ _p->file = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES); });
    if(_p->file == NULL) {
        return false;
    }
    // (as long as its whole blocks: one cut short by the app stopping plays to where it got)
    const u64 _bytes = rde_file_get_size(_p->file);
    const u32 _whole = (u32)((_bytes / FUDE_ZOOM_ADPCM_BLOCK) * FUDE_ZOOM_ADPCM_FRAMES);
    _p->rate   = _rate;
    _p->frames = _frames > 0u && _frames < _whole ? _frames : _whole;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _p->buffer = (f32*)_heap->malloc(_heap->allocator, sizeof(f32) * FUDE_ZOOM_ADPCM_FRAMES);
    return _p->buffer != NULL;
}

b8 fude_zoom_voice_is_open(const fude_zoom_voice_player* _p) {
    return _p->file != NULL;
}

void fude_zoom_voice_close(fude_zoom_voice_player* _p) {
    if(_p->stream != NULL) {
        rde_audio_stream_close(_p->stream);
    }
    if(_p->file != NULL) {
        rde_file_close(_p->file);
    }
    if(_p->buffer != NULL) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _heap->free(_heap->allocator, _p->buffer);
    }
    memset(_p, 0, sizeof(*_p));
}

u32 fude_zoom_voice_position(const fude_zoom_voice_player* _p) {
    if(!_p->playing || _p->stream == NULL) {
        return _p->at;
    }
    const u64 _queued = rde_audio_stream_queued_frames(_p->stream);
    const u64 _heard  = _p->pushed > _queued ? _p->pushed - _queued : 0u;
    const u64 _at     = (u64)_p->at + _heard;
    return _at < _p->frames ? (u32)_at : _p->frames;
}

void fude_zoom_voice_pause(fude_zoom_voice_player* _p) {
    if(!_p->playing) {
        return;
    }
    _p->at      = fude_zoom_voice_position(_p);
    _p->playing = false;
    if(_p->stream != NULL) {
        rde_audio_stream_close(_p->stream);   // (what was queued past where it is: let go)
        _p->stream = NULL;
    }
}

void fude_zoom_voice_play(fude_zoom_voice_player* _p) {
    if(_p->playing || _p->file == NULL) {
        return;
    }
    if(_p->at >= _p->frames) {
        _p->at = 0u;   // (at its end: from its start again)
    }
    _p->stream = rde_audio_stream_open(_p->rate, 1u, (u32)((f64)_p->rate * FZV_DEVICE), NULL);
    if(_p->stream == NULL) {
        return;
    }
    _p->next    = _p->at;
    _p->pushed  = 0u;
    _p->playing = true;
    fude_zoom_voice_pump(_p);   // (some queued before it starts: no silence first)
    rde_audio_stream_start(_p->stream);
}

void fude_zoom_voice_seek(fude_zoom_voice_player* _p, u32 _frame) {
    const b8 _was = _p->playing;
    fude_zoom_voice_pause(_p);
    _p->at = _frame < _p->frames ? _frame : _p->frames;
    if(_was) {
        fude_zoom_voice_play(_p);
    }
}

void fude_zoom_voice_pump(fude_zoom_voice_player* _p) {
    if(!_p->playing || _p->stream == NULL) {
        return;
    }
    const u32 _ahead = (u32)((f64)_p->rate * FZV_AHEAD);
    i16 _pcm[FUDE_ZOOM_ADPCM_FRAMES];
    u8  _block[FUDE_ZOOM_ADPCM_BLOCK];
    while(_p->next < _p->frames && rde_audio_stream_queued_frames(_p->stream) < _ahead) {
        const u32 _k = _p->next / FUDE_ZOOM_ADPCM_FRAMES, _from = _p->next % FUDE_ZOOM_ADPCM_FRAMES;
        if(rde_file_read_at(_p->file, (u64)_k * FUDE_ZOOM_ADPCM_BLOCK, _block, FUDE_ZOOM_ADPCM_BLOCK) != FUDE_ZOOM_ADPCM_BLOCK) {
            _p->frames = _p->next;   // (the file shorter than it said: it ends here)
            break;
        }
        fude_zoom_adpcm_decode(_block, _pcm);
        u32 _n = FUDE_ZOOM_ADPCM_FRAMES - _from;
        _n = _p->next + _n > _p->frames ? _p->frames - _p->next : _n;
        for(u32 _i = 0; _i < _n; _i++) {
            _p->buffer[_i] = (f32)_pcm[_from + _i] / 32768.0f;
        }
        const u32 _took = rde_audio_stream_push(_p->stream, _p->buffer, _n);
        _p->next   += _took;
        _p->pushed += _took;
        if(_took < _n) {
            break;   // (its ring full: the rest next frame)
        }
    }
    if(_p->next >= _p->frames && fude_zoom_voice_position(_p) >= _p->frames) {
        _p->at      = _p->frames;   // (played to its end: stopped there)
        _p->playing = false;
        rde_audio_stream_close(_p->stream);
        _p->stream = NULL;
    }
}
