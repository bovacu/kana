// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VOICE
#define FUDE_ZOOM_VOICE

#include "rde.h"
#include "zoom/adpcm.h"

// ===========================================================================
// A recording made and played (0.1.93, audio.h's files): the microphone's
// samples as they come, ADPCM blocks added to the recording's file; and a
// recording's blocks read as they are wanted, decoded, pushed to the speaker —
// where it is, a pause, a jump anywhere. Pumped once a frame: they keep up on
// their own (the devices buffer on their threads).
// ===========================================================================

typedef struct {
    rde_audio_recorder*   device;
    rde_file*             file;
    fude_zoom_adpcm_state state;
    i16                   pending[FUDE_ZOOM_ADPCM_FRAMES];   // a block's samples not yet written
    u32                   pending_n;
    u32                   frames;     // samples taken, all told
    u32                   rate;
    f32*                  buffer;     // (what is read at a time)
    u32                   buffer_cap;
    f32                   level;      // the loudness of what came last (0 .. 1: a meter)
    f64                   synced_at;
} fude_zoom_voice_recorder;

// Recording into _path (made, or added to) at _rate, mono: the microphone opened and started. False: it could not be.
b8   fude_zoom_voice_record(fude_zoom_voice_recorder* _r, const c8* _path, u32 _rate);
// What the microphone has given since, written (whole blocks).
void fude_zoom_voice_record_pump(fude_zoom_voice_recorder* _r);
// Done: the last samples written (their block filled out), the microphone closed. Its samples.
u32  fude_zoom_voice_record_stop(fude_zoom_voice_recorder* _r);
b8   fude_zoom_voice_recording(const fude_zoom_voice_recorder* _r);

typedef struct {
    rde_audio_stream* stream;     // open while it plays
    rde_file*         file;
    u32               rate;
    u32               frames;     // the recording's
    u32               next;       // the next sample to push
    u32               at;         // where it is, paused (or where it began, playing: its stream's first)
    u64               pushed;     // pushed since it began playing
    b8                playing;
    f32*              buffer;
} fude_zoom_voice_player;

// Recording _path (its _rate, its _frames samples) ready to play, from its start. False: no such file.
b8   fude_zoom_voice_open(fude_zoom_voice_player* _p, const c8* _path, u32 _rate, u32 _frames);
void fude_zoom_voice_close(fude_zoom_voice_player* _p);
void fude_zoom_voice_play(fude_zoom_voice_player* _p);
void fude_zoom_voice_pause(fude_zoom_voice_player* _p);
// From sample _frame (playing on, if it was).
void fude_zoom_voice_seek(fude_zoom_voice_player* _p, u32 _frame);
// Kept a little ahead of the speaker; at the end, stopped there.
void fude_zoom_voice_pump(fude_zoom_voice_player* _p);
// Where it is (samples), as heard.
u32  fude_zoom_voice_position(const fude_zoom_voice_player* _p);
b8   fude_zoom_voice_is_open(const fude_zoom_voice_player* _p);

#endif
