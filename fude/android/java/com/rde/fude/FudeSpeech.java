// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.content.Intent;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;

/** Reading aloud (speech.h): the device's TextToSpeech, in the language taught. */
public final class FudeSpeech {
    static volatile TextToSpeech tts;
    static volatile int          state;   // 0 not started, 1 starting, 2 ready, 3 none
    // The utterances handed to the engine and not ended yet: each ends once, done, stopped (flushed by the next, or by
    // stop) or failed, on the engine's own thread. speaking() reads it from the app's.
    static final AtomicInteger   talking = new AtomicInteger();

    /** Starts the engine for _voice ("ja-JP"); ready a moment later. */
    public static void start(byte[] voice) {
        if(state != 0) {
            return;
        }
        state = 1;
        final Locale locale = Locale.forLanguageTag(FudeAndroid.text(voice));
        FudeAndroid.main.post(new Runnable() {
            @Override
            public void run() {
                tts = new TextToSpeech(FudeAndroid.context, new TextToSpeech.OnInitListener() {
                    @Override
                    public void onInit(int status) {
                        if(status != TextToSpeech.SUCCESS) {
                            state = 3;
                            return;
                        }
                        tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                            @Override public void onStart(String id) { }
                            @Override public void onDone(String id) { talking.decrementAndGet(); }
                            @Override public void onStop(String id, boolean interrupted) { talking.decrementAndGet(); }
                            @Override public void onError(String id) { talking.decrementAndGet(); }
                        });
                        final int r = tts.setLanguage(locale);
                        state = (r == TextToSpeech.LANG_MISSING_DATA || r == TextToSpeech.LANG_NOT_SUPPORTED) ? 3 : 2;
                        if(state == 3) {
                            Log.w(FudeAndroid.TAG, "no voice for " + locale);
                        }
                    }
                });
            }
        });
    }

    public static boolean available() {
        return state == 2;
    }

    /** 0 not started, 1 starting, 2 ready, 3 no voice for the language (speech.h's state). */
    public static int state() {
        return state;
    }

    /** No voice: the engine let go, to be started again (back from the settings, a voice may be there now). */
    public static void recheck() {
        if(state != 3) {
            return;
        }
        final TextToSpeech old = tts;
        tts   = null;
        state = 0;
        if(old != null) {
            FudeAndroid.main.post(new Runnable() {
                @Override
                public void run() {
                    old.shutdown();
                }
            });
        }
    }

    /** The device's text-to-speech settings (where a voice is installed); false when there is no such screen. */
    public static boolean openSettings() {
        final Intent[] tries = { new Intent("com.android.settings.TTS_SETTINGS"), new Intent(android.provider.Settings.ACTION_SETTINGS) };
        for(Intent intent : tries) {
            try {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                FudeAndroid.context.startActivity(intent);
                return true;
            } catch(Exception e) {
                Log.w(FudeAndroid.TAG, "no " + intent.getAction());
            }
        }
        return false;
    }

    /** Whether an utterance is still being read, or waits to be (speech_android.c: the spoken category held till not). */
    public static boolean speaking() {
        return talking.get() > 0;
    }

    public static void speak(byte[] text) {
        if(state == 2) {
            talking.incrementAndGet();
            if(tts.speak(FudeAndroid.text(text), TextToSpeech.QUEUE_FLUSH, null, "fude") != TextToSpeech.SUCCESS) {
                talking.decrementAndGet();   // (not queued: it will never end)
            }
        }
    }

    public static void stop() {
        if(state == 2) {
            tts.stop();
        }
    }
}
