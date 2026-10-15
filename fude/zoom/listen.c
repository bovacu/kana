// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/listen.h"
#include "drawing/base/kfile.h"
#include "drawing/base/save.h"

// ===========================================================================
// See listen.h.
//
// The model's path and its check come first, in every build. Then, with FUDE_WHISPER, the reading thread's state,
// whisper's callbacks, the thread itself and the public functions; without it, the same functions reading nothing.
// The file's own names start with fzls_ (plot.c's are fzl_, logic.c's fzlg_).
// ===========================================================================

// See the header.
void fude_listen_model_path(c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s%s", fude_save_downloads_dir(), FUDE_LISTEN_MODEL_FILE);
}

// See the header. Asked on the main thread only, so a model left where earlier builds kept it (<save>models/, which
// every backup took in) is moved to the downloads folder before the reading thread can look for it, rather than
// downloaded again.
b8 fude_listen_model_ready(void) {
    c8 _path[RDE_MAX_PATH], _old[RDE_MAX_PATH];
    fude_listen_model_path(_path, sizeof(_path));
    rde_snprintf(_old, sizeof(_old), "%smodels/%s", fude_save_dir(), FUDE_LISTEN_MODEL_FILE);
    if(!rde_file_exists(_path) && fude_file_size(_old) == FUDE_LISTEN_MODEL_BYTES) {
        rde_file_move(_old, _path);
    }
    return fude_file_size(_path) == FUDE_LISTEN_MODEL_BYTES;
}

#if defined(FUDE_WHISPER)

#include "whisper.h"
#include "ggml-backend.h"

// The thread's: what it reads, and what it read. The lock and the wake-up are made the first time the module is asked
// anything (fzls_sync), on the main thread, before the reading thread exists. fzls_lock guards the piece's three flags
// (below), fzls_stop, fzls_failed, fzls_found, and the guesses of the encoding's time and share (fzls_encode_s,
// fzls_share), which the thread sets once a piece is read and fude_listen_progress reads. What the piece is (its
// samples, tag, offset, language, prompt, threads) is written by fude_listen_submit only while no piece is waiting,
// being read or done, so the thread reads it without the lock. fzls_wake wakes the thread for a piece or to stop.
// fzls_ctx is the model, loaded by the thread with its first piece and kept until fude_listen_shutdown.
static rde_thread*          fzls_thread = NULL;
static rde_mutex            fzls_lock   = NULL;
static rde_cond             fzls_wake   = NULL;
static b8                   fzls_started = false, fzls_stop = false;
static struct whisper_context* fzls_ctx = NULL;
// The piece: waiting (fzls_job), being read (fzls_running), done (fzls_done).
static b8   fzls_job = false, fzls_running = false, fzls_done = false;
// How the piece went (FUDE_LISTEN_); its tag, where it starts in its recording (ms), and the threads to read it with.
static u8   fzls_failed = FUDE_LISTEN_OK;
static u32  fzls_tag = 0, fzls_offset_ms = 0, fzls_threads = 2;
static rde_arr TYPE(f32) fzls_pcm;   // the piece's samples (its room kept from one piece to the next)
// How many of fzls_pcm's samples are the piece's; the language asked for ("" to let whisper find it), the words said
// before the piece, and the language whisper found.
static u32  fzls_frames = 0;
static c8   fzls_language[8], fzls_prompt[512], fzls_found[8];
// How far a piece is, as it is read. Whisper reads 30 s at a time (a piece is one window, or a second for what the
// first left): it encodes the window — the same time each window, timed here and guessed from the last — then writes
// its words token by token, the phrases' time tokens among them saying how far into the window the writing is.
// Shown: the encoding's share of the time (measured piece by piece) as it goes, then the writing's, by its times.
static volatile u32 fzls_piece_no  = 0;      // (each piece picked up: the progress shown starts again)
static volatile f32 fzls_piece_s   = 0.0f;   // its length, in seconds
static volatile f32 fzls_window    = 0.0f;   // where the window being read starts (of the piece)
static volatile f64 fzls_encode_at = 0.0;    // the first window's encoding began (0: not yet)
static volatile f64 fzls_write_at  = 0.0;    // its writing began (0: not yet)
static volatile f32 fzls_written   = 0.0f;   // how far the writing is (of the piece)
static f32          fzls_encode_s  = 4.0f;   // what encoding a window takes (a first guess till measured; under the lock)
static f32          fzls_share     = 0.4f;   // of a piece's time, the encoding's (likewise)
// The piece's phrases, their times into the recording: written by the thread, then taken by fude_listen_take once
// fzls_done is set.
static fude_zoom_transcript fzls_result;

// See the header: whisper is in this build.
b8 fude_listen_built(void) {
    return true;
}

// Makes the lock, the wake-up and the samples' array, once, on the main thread (every public function is called from
// it), before the reading thread can use them.
RDE_INTERNAL void fzls_sync(void) {
    if(fzls_lock == NULL) {
        fzls_lock = rde_mutex_create();
        fzls_wake = rde_cond_create();
        fzls_pcm  = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    }
}

// whisper's and ggml's log: only errors are passed on, as warnings; the rest of its chatter is left out.
RDE_INTERNAL void fzls_log(enum ggml_log_level _level, const char* _text, any _user) {
    RDE_UNUSED(_user);
    if(_level >= GGML_LOG_LEVEL_ERROR) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "listen: %s", _text);   // (only what went wrong: its chatter left out)
    }
}

// whisper's progress callback, as a window is about to be read: where it starts, from whisper's progress in hundredths
// of the piece.
RDE_INTERNAL void fzls_progress_cb(struct whisper_context* _ctx, struct whisper_state* _state, int _progress, any _user) {
    RDE_UNUSED(_ctx); RDE_UNUSED(_state); RDE_UNUSED(_user);
    fzls_window = (f32)_progress / 100.0f;
}

// whisper's encoder callback, as a window's encoding begins: the first one's time is kept, to time the encoding.
// Returns true, so the reading goes on.
RDE_INTERNAL bool fzls_encode_cb(struct whisper_context* _ctx, struct whisper_state* _state, any _user) {
    RDE_UNUSED(_ctx); RDE_UNUSED(_state); RDE_UNUSED(_user);
    if(fzls_encode_at <= 0.0) {
        fzls_encode_at = rde_engine_get_time_now();
    }
    return true;
}

// whisper's logits filter, called before each token is written, used here only to follow the writing: the last time
// token among those written so far says how far into the window they are (each a fiftieth of a second from its
// start), which moves fzls_written on, never back. The first call times the writing's start. The logits are left as
// they are.
RDE_INTERNAL void fzls_token_cb(struct whisper_context* _ctx, struct whisper_state* _state, const whisper_token_data* _tokens, int _n, float* _logits, any _user) {
    RDE_UNUSED(_state); RDE_UNUSED(_logits); RDE_UNUSED(_user);
    if(fzls_write_at <= 0.0) {
        fzls_write_at = rde_engine_get_time_now();
    }
    const whisper_token _beg = whisper_token_beg(_ctx);
    for(int _i = _n - 1; _i >= 0 && fzls_piece_s > 0.0f; _i--) {
        if(_tokens[_i].id >= _beg) {
            const f32 _at = fzls_window + (f32)(_tokens[_i].id - _beg) * 0.02f / fzls_piece_s;
            fzls_written = _at > fzls_written ? (_at < 1.0f ? _at : 1.0f) : fzls_written;
            break;
        }
    }
}

// The CPU's code, where it is a library of its own (Android: tools/whisper/build.sh): the best of them this CPU can
// run — the dot product, half floats, int8 matrices, as far as it has them — loaded from where the app's libraries
// are (libwhisper.so's folder). Elsewhere it is linked in. False: none could be.
RDE_INTERNAL b8 fzls_backends(void) {
#if defined(RDE_PLATFORM_ANDROID)
    static b8 _tried = false, _ok = false;
    if(_tried) {
        return _ok;
    }
    _tried = true;
    c8 _dir[1024];
    if(rde_file_get_module_path((const any)&whisper_init_from_file_with_params, _dir, sizeof(_dir))) {
        c8* _slash = rde_strrchr(_dir, '/');
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

// The reading thread. It waits for a piece (or to stop), marks it running, loads the CPU's code and the model the
// first time, reads the piece with whisper (greedy, no context carried over from the last piece, the words before it
// as the prompt), updates the guesses of the encoding's time and share for the next piece's progress, and marks the
// piece done with its phrases in fzls_result and how it went in fzls_failed.
RDE_INTERNAL any fzls_main(rde_thread* _thread, any _arg) {
    RDE_UNUSED(_thread); RDE_UNUSED(_arg);
    for(;;) {
        rde_mutex_lock(fzls_lock);
        while(!fzls_job && !fzls_stop) {
            rde_cond_wait(fzls_wake, fzls_lock);
        }
        if(fzls_stop) {
            rde_mutex_unlock(fzls_lock);
            break;
        }
        fzls_job        = false;
        fzls_running    = true;
        fzls_piece_no  += 1u;     // (this piece's, once it begins: the last one's not taken for it)
        fzls_piece_s    = 0.0f;
        fzls_window     = 0.0f;
        fzls_encode_at  = 0.0;
        fzls_write_at   = 0.0;
        fzls_written    = 0.0f;
        rde_mutex_unlock(fzls_lock);

        // (the model loaded the first time: a second or two — the CPU's code first)
        u8 _why = FUDE_LISTEN_OK;
        if(fzls_ctx == NULL) {
            if(!fzls_backends()) {
                _why = FUDE_LISTEN_CANNOT;
            } else {
                c8 _path[RDE_MAX_PATH];
                fude_listen_model_path(_path, sizeof(_path));
                struct whisper_context_params _cp = whisper_context_default_params();
                _cp.use_gpu = false;
                fzls_ctx = whisper_init_from_file_with_params(_path, _cp);
                _why = fzls_ctx != NULL ? FUDE_LISTEN_OK : FUDE_LISTEN_DAMAGED;
            }
        }
        fude_zoom_transcript_clear(&fzls_result);
        b8  _ok = fzls_ctx != NULL;
        c8  _found[8] = { 0 };
        f32 _encode = 0.0f, _share = 0.0f;   // (measured: what the encoding took and its share; 0: not measured)
        if(_ok) {
            struct whisper_full_params _p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            _p.n_threads        = (int)fzls_threads;
            _p.print_progress   = false;
            _p.print_realtime   = false;
            _p.print_special    = false;
            _p.print_timestamps = false;
            _p.translate        = false;
            _p.no_context       = true;
            _p.single_segment   = false;
            _p.suppress_blank   = true;
            _p.suppress_nst     = true;
            _p.language         = fzls_language[0] != 0 ? fzls_language : "auto";
            _p.detect_language  = false;
            _p.initial_prompt   = fzls_prompt[0] != 0 ? fzls_prompt : NULL;
            _p.progress_callback      = fzls_progress_cb;
            _p.encoder_begin_callback = fzls_encode_cb;
            _p.logits_filter_callback = fzls_token_cb;
            fzls_piece_s = (f32)fzls_frames / (f32)FUDE_LISTEN_RATE;
            _ok  = whisper_full(fzls_ctx, _p, (const f32*)fzls_pcm.memory, (int)fzls_frames) == 0;
            // (what the encoding took, and its share of the piece's time: the next piece's guesses, set under the lock
            // with the piece's end, below, since fude_listen_progress reads them)
            const f64 _end = rde_engine_get_time_now();
            if(_ok && fzls_piece_s > 3.0f && fzls_encode_at > 0.0 && fzls_write_at > fzls_encode_at && _end > fzls_write_at) {
                _encode = (f32)(fzls_write_at - fzls_encode_at);
                _share  = _encode / (f32)(_end - fzls_encode_at);
            }
            _why = _ok ? FUDE_LISTEN_OK : FUDE_LISTEN_CANNOT;
            if(_ok) {
                const int _n = whisper_full_n_segments(fzls_ctx);
                for(int _i = 0; _i < _n; _i++) {
                    // (its times in hundredths of a second, from the piece's start)
                    const u32 _t0 = fzls_offset_ms + (u32)(whisper_full_get_segment_t0(fzls_ctx, _i) * 10);
                    const u32 _t1 = fzls_offset_ms + (u32)(whisper_full_get_segment_t1(fzls_ctx, _i) * 10);
                    fude_zoom_transcript_add(&fzls_result, _t0, _t1, whisper_full_get_segment_text(fzls_ctx, _i));
                }
                const c8* _lang = whisper_lang_str(whisper_full_lang_id(fzls_ctx));
                rde_snprintf(_found, sizeof(_found), "%s", _lang != NULL ? _lang : "");
            }
        }
        rde_mutex_lock(fzls_lock);
        if(_encode > 0.0f) {
            fzls_encode_s = 0.5f * fzls_encode_s + 0.5f * _encode;
            fzls_share    = 0.5f * fzls_share + 0.5f * (_share < 0.1f ? 0.1f : (_share > 0.9f ? 0.9f : _share));
        }
        rde_snprintf(fzls_found, sizeof(fzls_found), "%s", _found);
        fzls_failed  = _ok ? FUDE_LISTEN_OK : (_why != FUDE_LISTEN_OK ? _why : FUDE_LISTEN_CANNOT);
        fzls_running = false;
        fzls_done    = true;
        rde_mutex_unlock(fzls_lock);
    }
    return NULL;
}

// See the header. The thread is started, and whisper's log set, by the first piece handed in; the samples are copied
// into fzls_pcm, whose room is kept from one piece to the next.
b8 fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads) {
    if(_frames == 0u || !fude_listen_model_ready()) {
        return false;
    }
    fzls_sync();
    rde_mutex_lock(fzls_lock);
    if(fzls_job || fzls_running || fzls_done) {
        rde_mutex_unlock(fzls_lock);
        return false;   // (one at a time: the last one taken first)
    }
    if(!fzls_started) {
        whisper_log_set(fzls_log, NULL);
        fude_zoom_transcript_init(&fzls_result);
        fzls_thread  = rde_thread_run(fzls_main, NULL);
        fzls_started = true;
    }
    rde_arr_resize(&fzls_pcm, _frames);
    rde_memcpy(fzls_pcm.memory, _pcm, sizeof(f32) * _frames);
    fzls_frames    = _frames;
    fzls_tag       = _tag;
    fzls_offset_ms = _offset_ms;
    fzls_threads   = _threads > 0u ? _threads : 2u;
    rde_snprintf(fzls_language, sizeof(fzls_language), "%s", _language != NULL ? _language : "");
    rde_snprintf(fzls_prompt, sizeof(fzls_prompt), "%s", _prompt != NULL ? _prompt : "");
    fzls_job      = true;
    rde_cond_signal(fzls_wake);
    rde_mutex_unlock(fzls_lock);
    return true;
}

// See the header.
b8 fude_listen_busy(void) {
    fzls_sync();
    rde_mutex_lock(fzls_lock);
    const b8 _busy = fzls_job || fzls_running || fzls_done;
    rde_mutex_unlock(fzls_lock);
    return _busy;
}

// See the header. While the window is encoded, the share of the piece's time the encoding takes is filled as the time
// since it began over its guessed length (held under 95% of it); then the rest is filled by how far the writing is.
// What is shown never goes back within a piece and stays under 99% until the piece is done.
f32 fude_listen_progress(void) {
    fzls_sync();
    rde_mutex_lock(fzls_lock);
    const b8  _running = fzls_running, _done = fzls_done;
    const f32 _encode_s = fzls_encode_s, _share = fzls_share;   // (the thread sets them as a piece ends)
    rde_mutex_unlock(fzls_lock);
    if(_done) {
        return 1.0f;
    }
    // (never back, within a piece: the encoding's guess passed, a window read again)
    static u32 _of = 0;
    static f32 _shown = 0.0f;
    if(_of != fzls_piece_no) {
        _of = fzls_piece_no;
        _shown = 0.0f;
    }
    if(!_running) {
        _shown = 0.0f;   // (between pieces: the next not begun)
        return 0.0f;
    }
    if(fzls_piece_s <= 0.0f || fzls_encode_at <= 0.0) {
        return _shown;
    }
    f32 _p;
    if(fzls_write_at <= 0.0) {
        const f32 _part = (f32)(rde_engine_get_time_now() - fzls_encode_at) / _encode_s;
        _p = _share * (_part < 0.95f ? _part : 0.95f);
    } else {
        _p = _share + (1.0f - _share) * fzls_written;
    }
    _shown = _p > _shown ? (_p < 0.99f ? _p : 0.99f) : _shown;   // (never done till it is)
    return _shown;
}

// See the header.
b8 fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed) {
    fzls_sync();
    rde_mutex_lock(fzls_lock);
    if(!fzls_done) {
        rde_mutex_unlock(fzls_lock);
        return false;
    }
    *_tag    = fzls_tag;
    *_failed = fzls_failed;
    if(fzls_failed == FUDE_LISTEN_OK) {
        fude_zoom_transcript_append(_into, &fzls_result);
    }
    if(_language != NULL && _size > 0u) {
        rde_snprintf(_language, _size, "%s", fzls_found);
    }
    fzls_done = false;
    rde_mutex_unlock(fzls_lock);
    return true;
}

// See the header. Nothing happens when the thread was never started.
void fude_listen_shutdown(void) {
    if(!fzls_started) {
        return;
    }
    fzls_sync();
    rde_mutex_lock(fzls_lock);
    fzls_stop = true;
    rde_cond_signal(fzls_wake);
    rde_mutex_unlock(fzls_lock);
    rde_thread_wait(fzls_thread);   // (a piece being read finishes first)
    fzls_thread  = NULL;
    fzls_started = false;
    fzls_stop    = false;
    // A piece still waiting when the thread stopped is dropped with it: left marked, it would keep every later submit
    // refused and busy true for good, once the thread started again.
    fzls_job     = false;
    fzls_running = false;
    fzls_done    = false;
    if(fzls_ctx != NULL) {
        whisper_free(fzls_ctx);
        fzls_ctx = NULL;
    }
    fude_zoom_transcript_free(&fzls_result);
}

#else

// Without whisper in this build, nothing is read: each function below says so (false, or 0).
// See the header: whisper is not in this build.
b8 fude_listen_built(void) {
    return false;
}

// Takes nothing: there is no whisper to read with.
b8 fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads) {
    RDE_UNUSED(_tag); RDE_UNUSED(_pcm); RDE_UNUSED(_frames); RDE_UNUSED(_offset_ms); RDE_UNUSED(_language); RDE_UNUSED(_prompt); RDE_UNUSED(_threads);
    return false;
}

// Never busy.
b8 fude_listen_busy(void) {
    return false;
}

// Nothing is read, so nothing is under way.
f32 fude_listen_progress(void) {
    return 0.0f;
}

// No piece is ever done.
b8 fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed) {
    RDE_UNUSED(_tag); RDE_UNUSED(_into); RDE_UNUSED(_language); RDE_UNUSED(_size); RDE_UNUSED(_failed);
    return false;
}

// Nothing to stop.
void fude_listen_shutdown(void) {
}

#endif
