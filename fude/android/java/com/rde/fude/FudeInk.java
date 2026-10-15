// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.util.Log;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.mlkit.common.model.DownloadConditions;
import com.google.mlkit.common.model.RemoteModelManager;
import com.google.mlkit.vision.digitalink.common.RecognitionCandidate;
import com.google.mlkit.vision.digitalink.common.RecognitionResult;
import com.google.mlkit.vision.digitalink.recognition.DigitalInkRecognition;
import com.google.mlkit.vision.digitalink.recognition.DigitalInkRecognitionModel;
import com.google.mlkit.vision.digitalink.recognition.DigitalInkRecognitionModelIdentifier;
import com.google.mlkit.vision.digitalink.recognition.DigitalInkRecognizer;
import com.google.mlkit.vision.digitalink.recognition.DigitalInkRecognizerOptions;
import com.google.mlkit.vision.digitalink.recognition.Ink;
import com.google.mlkit.vision.digitalink.recognition.RecognitionContext;
import com.google.mlkit.vision.digitalink.recognition.WritingArea;

/** Handwriting (mlkit.h): ML Kit Digital Ink Recognition, its model downloaded once. */
public final class FudeInk {
    // mlkit.h's FUDE_MLKIT_: 1 idle, 2 downloading, 3 ready, 4 failed.
    static volatile int                        state = 1;
    static DigitalInkRecognitionModel          model;
    static volatile DigitalInkRecognizer       recognizer;
    static volatile boolean                    busy;
    static volatile String                     answer;   // null: none in
    static volatile double                     took;
    static volatile long                       started;
    // Each reading's number: a reading given up on (take, after GIVE_UP_NS) has its late answer dropped, so it is
    // never taken for the next reading's. ML Kit always calls back, but should it ever not, handwriting would
    // otherwise stay unreadable until the app restarts.
    static volatile int                        generation;
    static final long                          GIVE_UP_NS = 10_000_000_000L;
    // Guards a reading's start, its answer (the generation checked and finish together) and the give-up in take: the
    // answer comes on Android's UI thread and take runs on the app's, so without it an answer could pass the check
    // just before take gives up and then land as the next reading's.
    static final Object                        LOCK = new Object();

    public static int state() {
        return state;
    }

    /** The model for _tag ("ja", "zh-Hani-CN", "ko"): downloaded when missing. */
    public static void prepare(byte[] tag) {
        if(state == 2 || state == 3) {
            return;
        }
        try {
            if(model == null) {
                model = DigitalInkRecognitionModel.builder(DigitalInkRecognitionModelIdentifier.fromLanguageTag(FudeAndroid.text(tag))).build();
            }
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "no handwriting model for " + FudeAndroid.text(tag) + ": " + e);
            state = 4;
            return;
        }
        state = 2;
        final RemoteModelManager manager = RemoteModelManager.getInstance();
        manager.isModelDownloaded(model).addOnSuccessListener(new OnSuccessListener<Boolean>() {
            @Override
            public void onSuccess(Boolean have) {
                if(have) {
                    ready();
                    return;
                }
                manager.download(model, new DownloadConditions.Builder().build()).addOnSuccessListener(new OnSuccessListener<Void>() {
                    @Override
                    public void onSuccess(Void v) {
                        Log.i(FudeAndroid.TAG, "the handwriting model is downloaded");
                        ready();
                    }
                }).addOnFailureListener(new OnFailureListener() {
                    @Override
                    public void onFailure(Exception e) {
                        Log.e(FudeAndroid.TAG, "the handwriting model could not be downloaded: " + e);
                        state = 4;
                    }
                });
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                state = 4;
            }
        });
    }

    static void ready() {
        recognizer = DigitalInkRecognition.getClient(DigitalInkRecognizerOptions.builder(model).build());
        state = 3;
    }

    /**
     * Reads strokes: _xy their points (x, y pairs, y DOWN), _t each point's time
     * (ms), _ends where each stroke ends (in points); _width x _height the area
     * written in. False when not ready or still reading the last.
     */
    public static boolean recognize(float[] xy, long[] t, int[] ends, byte[] preContext, float width, float height) {
        if(state != 3 || busy || recognizer == null) {
            return false;
        }
        Ink.Builder ink = Ink.builder();
        int p = 0;
        for(int end : ends) {
            Ink.Stroke.Builder stroke = Ink.Stroke.builder();
            for(; p < end; p++) {
                stroke.addPoint(Ink.Point.create(xy[2 * p], xy[2 * p + 1], t[p]));
            }
            ink.addStroke(stroke.build());
        }
        RecognitionContext context = RecognitionContext.builder()
            .setPreContext(FudeAndroid.text(preContext))
            .setWritingArea(new WritingArea(Math.max(width, 1.0f), Math.max(height, 1.0f)))
            .build();
        final int gen;
        synchronized(LOCK) {
            busy    = true;
            answer  = null;
            started = System.nanoTime();
            gen     = ++generation;
        }
        recognizer.recognize(ink.build(), context).addOnSuccessListener(new OnSuccessListener<RecognitionResult>() {
            @Override
            public void onSuccess(RecognitionResult result) {
                StringBuilder s = new StringBuilder();
                for(RecognitionCandidate c : result.getCandidates()) {
                    s.append(c.getText()).append('\n');
                }
                synchronized(LOCK) {
                    if(gen == generation) {   // (else given up on: not the current reading's answer)
                        finish(s.toString());
                    }
                }
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                synchronized(LOCK) {
                    if(gen != generation) {
                        return;
                    }
                    Log.e(FudeAndroid.TAG, "recognition failed: " + e);
                    finish("");
                }
            }
        });
        return true;
    }

    /** The reading's answer is in (called under LOCK). */
    static void finish(String s) {
        took   = (System.nanoTime() - started) / 1.0e6;
        answer = s;
        busy   = false;
    }

    /** The answer, once (candidates best first, one per line, UTF-8); null while none. */
    public static byte[] take() {
        final String a;
        synchronized(LOCK) {
            if(answer == null && busy && System.nanoTime() - started > GIVE_UP_NS) {
                Log.w(FudeAndroid.TAG, "no answer after 10 s; given up");
                generation++;
                finish("");   // an empty answer: the reader falls back to its own, and the next reading can start
            }
            a = answer;
            answer = null;
        }
        return a != null ? FudeAndroid.bytes(a) : null;
    }

    public static double took() {
        return took;
    }
}
