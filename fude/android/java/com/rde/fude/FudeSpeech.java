package com.rde.fude;

import android.speech.tts.TextToSpeech;
import android.util.Log;

import java.util.Locale;

/** Reading aloud (speech.h): the device's TextToSpeech, in the language taught. */
public final class FudeSpeech {
    static volatile TextToSpeech tts;
    static volatile int          state;   // 0 not started, 1 starting, 2 ready, 3 none

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

    public static void speak(byte[] text) {
        if(state == 2) {
            tts.speak(FudeAndroid.text(text), TextToSpeech.QUEUE_FLUSH, null, "fude");
        }
    }

    public static void stop() {
        if(state == 2) {
            tts.stop();
        }
    }
}
