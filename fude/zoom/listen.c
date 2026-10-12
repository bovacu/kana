// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/listen.h"
#include "drawing/base/save.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See listen.h.
// ===========================================================================

void fude_listen_model_path(c8* _out, usize _size) {
    snprintf(_out, _size, "%smodels/%s", fude_save_dir(), FUDE_LISTEN_MODEL_FILE);
}

b8 fude_listen_model_ready(void) {
    c8 _path[RDE_MAX_PATH];
    fude_listen_model_path(_path, sizeof(_path));
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return false;
    }
    fseek(_f, 0, SEEK_END);
    const long _n = ftell(_f);
    fclose(_f);
    return (u64)_n == FUDE_LISTEN_MODEL_BYTES;
}

#if defined(FUDE_WHISPER)

#include "whisper.h"
#include "ggml-backend.h"
#include <pthread.h>
#include <stdlib.h>
#if defined(RDE_PLATFORM_ANDROID)
#include <dlfcn.h>
#endif

// The thread's: what it reads, and what it read (under fzl_lock).
static pthread_t            fzl_thread;
static pthread_mutex_t      fzl_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t       fzl_wake = PTHREAD_COND_INITIALIZER;
static b8                   fzl_started = false, fzl_stop = false;
static struct whisper_context* fzl_ctx = NULL;
// The piece: waiting (fzl_job), being read (fzl_running), done (fzl_done).
static b8   fzl_job = false, fzl_running = false, fzl_done = false;
static u8   fzl_failed = FUDE_LISTEN_OK;
static u32  fzl_tag = 0, fzl_offset_ms = 0, fzl_threads = 2;
static f32* fzl_pcm = NULL;
static u32  fzl_frames = 0;
static c8   fzl_language[8], fzl_prompt[512], fzl_found[8];
// How far a piece is, as it is read. Whisper reads 30 s at a time (a piece is one window, or a second for what the
// first left): it encodes the window — the same time each window, timed here and guessed from the last — then writes
// its words token by token, the phrases' time tokens among them saying how far into the window the writing is.
// Shown: the encoding's share of the time (measured piece by piece) as it goes, then the writing's, by its times.
static volatile u32 fzl_piece_no  = 0;      // (each piece picked up: the progress shown starts again)
static volatile f32 fzl_piece_s   = 0.0f;   // its length, in seconds
static volatile f32 fzl_window    = 0.0f;   // where the window being read starts (of the piece)
static volatile f64 fzl_encode_at = 0.0;    // the first window's encoding began (0: not yet)
static volatile f64 fzl_write_at  = 0.0;    // its writing began (0: not yet)
static volatile f32 fzl_written   = 0.0f;   // how far the writing is (of the piece)
static f32          fzl_encode_s  = 4.0f;   // what encoding a window takes (a first guess till measured)
static f32          fzl_share     = 0.4f;   // of a piece's time, the encoding's (likewise)
static fude_zoom_transcript fzl_result;

b8 fude_listen_built(void) {
    return true;
}

RDE_INTERNAL void fzl_log(enum ggml_log_level _level, const char* _text, void* _user) {
    RDE_UNUSED(_user);
    if(_level >= GGML_LOG_LEVEL_ERROR) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "listen: %s", _text);   // (only what went wrong: its chatter left out)
    }
}

// A window about to be read: where it starts (whisper's progress, in hundredths of the piece).
RDE_INTERNAL void fzl_progress_cb(struct whisper_context* _ctx, struct whisper_state* _state, int _progress, void* _user) {
    RDE_UNUSED(_ctx); RDE_UNUSED(_state); RDE_UNUSED(_user);
    fzl_window = (f32)_progress / 100.0f;
}

// Its encoding begins (the first window's timed).
RDE_INTERNAL bool fzl_encode_cb(struct whisper_context* _ctx, struct whisper_state* _state, void* _user) {
    RDE_UNUSED(_ctx); RDE_UNUSED(_state); RDE_UNUSED(_user);
    if(fzl_encode_at <= 0.0) {
        fzl_encode_at = rde_engine_get_time_now();
    }
    return true;
}

// A token about to be written, after those so far: the last time token among them, how far into the window they are
// (each a fiftieth of a second from its start).
RDE_INTERNAL void fzl_token_cb(struct whisper_context* _ctx, struct whisper_state* _state, const whisper_token_data* _tokens, int _n, float* _logits, void* _user) {
    RDE_UNUSED(_state); RDE_UNUSED(_logits); RDE_UNUSED(_user);
    if(fzl_write_at <= 0.0) {
        fzl_write_at = rde_engine_get_time_now();
    }
    const whisper_token _beg = whisper_token_beg(_ctx);
    for(int _i = _n - 1; _i >= 0 && fzl_piece_s > 0.0f; _i--) {
        if(_tokens[_i].id >= _beg) {
            const f32 _at = fzl_window + (f32)(_tokens[_i].id - _beg) * 0.02f / fzl_piece_s;
            fzl_written = _at > fzl_written ? (_at < 1.0f ? _at : 1.0f) : fzl_written;
            break;
        }
    }
}

// The CPU's code, where it is a library of its own (Android: tools/whisper/build.sh): the best of them this CPU can
// run — the dot product, half floats, int8 matrices, as far as it has them — loaded from where the app's libraries
// are (libwhisper.so's folder). Elsewhere it is linked in. False: none could be.
RDE_INTERNAL b8 fzl_backends(void) {
#if defined(RDE_PLATFORM_ANDROID)
    static b8 _tried = false, _ok = false;
    if(_tried) {
        return _ok;
    }
    _tried = true;
    Dl_info _dl;
    if(dladdr((void*)&whisper_init_from_file_with_params, &_dl) != 0 && _dl.dli_fname != NULL) {
        c8 _dir[1024];
        snprintf(_dir, sizeof(_dir), "%s", _dl.dli_fname);
        c8* _slash = strrchr(_dir, '/');
        if(_slash != NULL) {
            *_slash = 0;
        }
        ggml_backend_load_all_from_path(_dir);
    }
    _ok = ggml_backend_dev_count() > 0u;
    for(size_t _i = 0; _i < ggml_backend_dev_count(); _i++) {
        rde_log_level(RDE_LOG_LEVEL_INFO, "listen: %s", ggml_backend_dev_description(ggml_backend_dev_get(_i)));
    }
    return _ok;
#else
    return true;
#endif
}

RDE_INTERNAL void* fzl_main(void* _arg) {
    RDE_UNUSED(_arg);
    for(;;) {
        pthread_mutex_lock(&fzl_lock);
        while(!fzl_job && !fzl_stop) {
            pthread_cond_wait(&fzl_wake, &fzl_lock);
        }
        if(fzl_stop) {
            pthread_mutex_unlock(&fzl_lock);
            break;
        }
        fzl_job        = false;
        fzl_running    = true;
        fzl_piece_no  += 1u;     // (this piece's, once it begins: the last one's not taken for it)
        fzl_piece_s    = 0.0f;
        fzl_window     = 0.0f;
        fzl_encode_at  = 0.0;
        fzl_write_at   = 0.0;
        fzl_written    = 0.0f;
        pthread_mutex_unlock(&fzl_lock);

        // (the model loaded the first time: a second or two — the CPU's code first)
        u8 _why = FUDE_LISTEN_OK;
        if(fzl_ctx == NULL) {
            if(!fzl_backends()) {
                _why = FUDE_LISTEN_CANNOT;
            } else {
                c8 _path[RDE_MAX_PATH];
                fude_listen_model_path(_path, sizeof(_path));
                struct whisper_context_params _cp = whisper_context_default_params();
                _cp.use_gpu = false;
                fzl_ctx = whisper_init_from_file_with_params(_path, _cp);
                _why = fzl_ctx != NULL ? FUDE_LISTEN_OK : FUDE_LISTEN_DAMAGED;
            }
        }
        fude_zoom_transcript_clear(&fzl_result);
        b8 _ok = fzl_ctx != NULL;
        c8 _found[8] = { 0 };
        if(_ok) {
            struct whisper_full_params _p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            _p.n_threads        = (int)fzl_threads;
            _p.print_progress   = false;
            _p.print_realtime   = false;
            _p.print_special    = false;
            _p.print_timestamps = false;
            _p.translate        = false;
            _p.no_context       = true;
            _p.single_segment   = false;
            _p.suppress_blank   = true;
            _p.suppress_nst     = true;
            _p.language         = fzl_language[0] != 0 ? fzl_language : "auto";
            _p.detect_language  = false;
            _p.initial_prompt   = fzl_prompt[0] != 0 ? fzl_prompt : NULL;
            _p.progress_callback      = fzl_progress_cb;
            _p.encoder_begin_callback = fzl_encode_cb;
            _p.logits_filter_callback = fzl_token_cb;
            fzl_piece_s = (f32)fzl_frames / (f32)FUDE_LISTEN_RATE;
            _ok  = whisper_full(fzl_ctx, _p, fzl_pcm, (int)fzl_frames) == 0;
            // (what the encoding took, and its share of the piece's time: the next piece's guesses)
            const f64 _end = rde_engine_get_time_now();
            if(_ok && fzl_piece_s > 3.0f && fzl_encode_at > 0.0 && fzl_write_at > fzl_encode_at && _end > fzl_write_at) {
                const f32 _encode = (f32)(fzl_write_at - fzl_encode_at), _share = _encode / (f32)(_end - fzl_encode_at);
                fzl_encode_s = 0.5f * fzl_encode_s + 0.5f * _encode;
                fzl_share    = 0.5f * fzl_share + 0.5f * (_share < 0.1f ? 0.1f : (_share > 0.9f ? 0.9f : _share));
            }
            _why = _ok ? FUDE_LISTEN_OK : FUDE_LISTEN_CANNOT;
            if(_ok) {
                const int _n = whisper_full_n_segments(fzl_ctx);
                for(int _i = 0; _i < _n; _i++) {
                    // (its times in hundredths of a second, from the piece's start)
                    const u32 _t0 = fzl_offset_ms + (u32)(whisper_full_get_segment_t0(fzl_ctx, _i) * 10);
                    const u32 _t1 = fzl_offset_ms + (u32)(whisper_full_get_segment_t1(fzl_ctx, _i) * 10);
                    fude_zoom_transcript_add(&fzl_result, _t0, _t1, whisper_full_get_segment_text(fzl_ctx, _i));
                }
                const c8* _lang = whisper_lang_str(whisper_full_lang_id(fzl_ctx));
                snprintf(_found, sizeof(_found), "%s", _lang != NULL ? _lang : "");
            }
        }
        pthread_mutex_lock(&fzl_lock);
        snprintf(fzl_found, sizeof(fzl_found), "%s", _found);
        fzl_failed  = _ok ? FUDE_LISTEN_OK : (_why != FUDE_LISTEN_OK ? _why : FUDE_LISTEN_CANNOT);
        fzl_running = false;
        fzl_done    = true;
        free(fzl_pcm);
        fzl_pcm = NULL;
        pthread_mutex_unlock(&fzl_lock);
    }
    return NULL;
}

b8 fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads) {
    if(_frames == 0u || !fude_listen_model_ready()) {
        return false;
    }
    pthread_mutex_lock(&fzl_lock);
    if(fzl_job || fzl_running || fzl_done) {
        pthread_mutex_unlock(&fzl_lock);
        return false;   // (one at a time: the last one taken first)
    }
    if(!fzl_started) {
        whisper_log_set(fzl_log, NULL);
        fude_zoom_transcript_init(&fzl_result);
        fzl_started = pthread_create(&fzl_thread, NULL, fzl_main, NULL) == 0;
        if(!fzl_started) {
            pthread_mutex_unlock(&fzl_lock);
            return false;
        }
    }
    fzl_pcm = (f32*)malloc(sizeof(f32) * _frames);
    if(fzl_pcm == NULL) {
        pthread_mutex_unlock(&fzl_lock);
        return false;
    }
    memcpy(fzl_pcm, _pcm, sizeof(f32) * _frames);
    fzl_frames    = _frames;
    fzl_tag       = _tag;
    fzl_offset_ms = _offset_ms;
    fzl_threads   = _threads > 0u ? _threads : 2u;
    snprintf(fzl_language, sizeof(fzl_language), "%s", _language != NULL ? _language : "");
    snprintf(fzl_prompt, sizeof(fzl_prompt), "%s", _prompt != NULL ? _prompt : "");
    fzl_job      = true;
    pthread_cond_signal(&fzl_wake);
    pthread_mutex_unlock(&fzl_lock);
    return true;
}

b8 fude_listen_busy(void) {
    pthread_mutex_lock(&fzl_lock);
    const b8 _busy = fzl_job || fzl_running || fzl_done;
    pthread_mutex_unlock(&fzl_lock);
    return _busy;
}

f32 fude_listen_progress(void) {
    pthread_mutex_lock(&fzl_lock);
    const b8 _running = fzl_running, _done = fzl_done;
    pthread_mutex_unlock(&fzl_lock);
    if(_done) {
        return 1.0f;
    }
    // (never back, within a piece: the encoding's guess passed, a window read again)
    static u32 _of = 0;
    static f32 _shown = 0.0f;
    if(_of != fzl_piece_no) {
        _of = fzl_piece_no;
        _shown = 0.0f;
    }
    if(!_running) {
        _shown = 0.0f;   // (between pieces: the next not begun)
        return 0.0f;
    }
    if(fzl_piece_s <= 0.0f || fzl_encode_at <= 0.0) {
        return _shown;
    }
    f32 _p;
    if(fzl_write_at <= 0.0) {
        const f32 _part = (f32)(rde_engine_get_time_now() - fzl_encode_at) / fzl_encode_s;
        _p = fzl_share * (_part < 0.95f ? _part : 0.95f);
    } else {
        _p = fzl_share + (1.0f - fzl_share) * fzl_written;
    }
    _shown = _p > _shown ? (_p < 0.99f ? _p : 0.99f) : _shown;   // (never done till it is)
    return _shown;
}

b8 fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed) {
    pthread_mutex_lock(&fzl_lock);
    if(!fzl_done) {
        pthread_mutex_unlock(&fzl_lock);
        return false;
    }
    *_tag    = fzl_tag;
    *_failed = fzl_failed;
    if(fzl_failed == FUDE_LISTEN_OK) {
        fude_zoom_transcript_append(_into, &fzl_result);
    }
    if(_language != NULL && _size > 0u) {
        snprintf(_language, _size, "%s", fzl_found);
    }
    fzl_done = false;
    pthread_mutex_unlock(&fzl_lock);
    return true;
}

void fude_listen_shutdown(void) {
    if(!fzl_started) {
        return;
    }
    pthread_mutex_lock(&fzl_lock);
    fzl_stop = true;
    pthread_cond_signal(&fzl_wake);
    pthread_mutex_unlock(&fzl_lock);
    pthread_join(fzl_thread, NULL);   // (a piece being read finishes first)
    fzl_started = false;
    fzl_stop    = false;
    if(fzl_ctx != NULL) {
        whisper_free(fzl_ctx);
        fzl_ctx = NULL;
    }
    fude_zoom_transcript_free(&fzl_result);
}

#else

// (whisper not in this build: nothing read)
b8 fude_listen_built(void) {
    return false;
}

b8 fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads) {
    RDE_UNUSED(_tag); RDE_UNUSED(_pcm); RDE_UNUSED(_frames); RDE_UNUSED(_offset_ms); RDE_UNUSED(_language); RDE_UNUSED(_prompt); RDE_UNUSED(_threads);
    return false;
}

b8 fude_listen_busy(void) {
    return false;
}

f32 fude_listen_progress(void) {
    return 0.0f;
}

b8 fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed) {
    RDE_UNUSED(_tag); RDE_UNUSED(_into); RDE_UNUSED(_language); RDE_UNUSED(_size); RDE_UNUSED(_failed);
    return false;
}

void fude_listen_shutdown(void) {
}

#endif
