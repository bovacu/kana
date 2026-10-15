// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VOICE
#define FUDE_ZOOM_VOICE

#include "rde.h"
#include "zoom/adpcm.h"

// ===========================================================================
// Voice: records a canvas's recordings and plays them back (0.1.93; the files are described in audio.h). Recording
// takes the microphone's samples as they come and adds them to the recording's file as ADPCM blocks. Playing reads a
// recording's blocks as they are wanted, decodes them and pushes them to the speaker, and can pause, say where it is,
// and jump anywhere.
//
// Where it sits: zoom, the canvas apps' layer, over RDE's audio recorder and audio stream. The page owns one
// recorder and one player (page.h) and pumps them once a frame (page_audio.c).
// Its main types and their lifecycle: fude_zoom_voice_recorder and fude_zoom_voice_player are plain structs the caller
// owns. A recorder is begun by fude_zoom_voice_record and let go by fude_zoom_voice_record_stop; a player is opened by
// fude_zoom_voice_open and let go by fude_zoom_voice_close. Both clear the whole struct, so a zeroed struct is idle.
// Threads: call everything on the main thread. The microphone and the speaker run on RDE's own audio threads, which
// fill and drain ring buffers; nothing here runs on them. Pumping once a frame is enough because those rings hold a
// second of sound. Should the main thread be held up for longer while recording, what the microphone's ring could not
// hold is lost: it is written as silence, so the recording keeps its time, and fude_zoom_voice_record_pump says how
// much was lost, for the page to tell the person.
// Files: a recording is appended to its file as it is made and synced to disk every 2 seconds, so what was said is on
// disk within a moment whatever stops the app. Playing only reads.
// Design: see docs/infinite_canvas_design.md §74.
// ===========================================================================

// A recording being made: the microphone, the file, and the samples of the block not written yet.
typedef struct {
    rde_audio_recorder*   device;
    rde_file*             file;
    fude_zoom_adpcm_state state;
    i16                   pending[FUDE_ZOOM_ADPCM_FRAMES];   // The samples of the next block, not written yet.
    u32                   pending_n;
    u32                   frames;     // The samples taken so far, in all.
    u32                   rate;
    f32*                  buffer;     // Room for what is read from the microphone at a time.
    u32                   buffer_cap;
    f32                   level;      // How loud the latest samples were, 0 to 1, for a meter.
    f64                   synced_at;
    u64                   dropped;    // The samples the microphone had lost when last looked (RDE counts them all).
} fude_zoom_voice_recorder;

// Opens the microphone at _rate samples a second, mono, starts it, and records into the file at _path (made, or added
// to). Returns false when it could not be begun, including when the device will not record at exactly _rate in mono:
// no recording is better than one that plays back at the wrong speed.
b8   fude_zoom_voice_record(fude_zoom_voice_recorder* _r, const c8* _path, u32 _rate);
// Writes what the microphone has given since the last call, in whole blocks, and updates the level. Call it once a
// frame while recording. Returns the samples lost since the last call (0 nearly always): the microphone's ring
// overflowed (the main thread was held up past its second) and those samples are gone; they are written as silence
// before what came after them, so every later moment stays where it was said, and a warning is logged.
u32  fude_zoom_voice_record_pump(fude_zoom_voice_recorder* _r);
// Stops recording: writes the last samples (their block filled out, so a recording is always whole blocks), syncs and
// closes the file, and closes the microphone. Returns the recording's length in samples, its filled-out block
// included; 0 when it was not recording.
u32  fude_zoom_voice_record_stop(fude_zoom_voice_recorder* _r);
// Returns whether it is recording.
b8   fude_zoom_voice_recording(const fude_zoom_voice_recorder* _r);

// A recording being played: the speaker's stream, the file, and where it is.
typedef struct {
    rde_audio_stream* stream;     // Open while it plays.
    rde_file*         file;
    u32               rate;
    u32               frames;     // The recording's length in samples.
    u32               next;       // The next sample to push to the speaker.
    // Paused: where it is. Playing: where it began, which is the stream's first sample.
    u32               at;         // A sample number.
    u64               pushed;     // Samples pushed since it began playing.
    b8                playing;
    f32*              buffer;
} fude_zoom_voice_player;

// Opens the recording at _path (_rate samples a second, _frames samples long) ready to play from its start. Its
// length is cut to the whole blocks in its file, so a recording cut short plays to where it got; _frames 0 takes the
// file's whole blocks. Returns false when there is no such file, _rate is 0 or there is no memory for its decoded
// samples; nothing is left open then.
b8   fude_zoom_voice_open(fude_zoom_voice_player* _p, const c8* _path, u32 _rate, u32 _frames);
// Stops playing, closes the file and clears the player.
void fude_zoom_voice_close(fude_zoom_voice_player* _p);
// Starts playing from where it is (from the start again when it is at the end).
void fude_zoom_voice_play(fude_zoom_voice_player* _p);
// Pauses where it is heard to be, dropping what was queued past that.
void fude_zoom_voice_pause(fude_zoom_voice_player* _p);
// Moves to sample _frame, and plays on from there if it was playing.
void fude_zoom_voice_seek(fude_zoom_voice_player* _p, u32 _frame);
// Keeps the speaker's queue 0.3 seconds ahead. At the end of the recording it stops there. Call it once a frame.
void fude_zoom_voice_pump(fude_zoom_voice_player* _p);
// Returns where it is in samples, as heard: what was pushed less what is still queued.
u32  fude_zoom_voice_position(const fude_zoom_voice_player* _p);
// Returns whether a recording is open.
b8   fude_zoom_voice_is_open(const fude_zoom_voice_player* _p);

#endif
