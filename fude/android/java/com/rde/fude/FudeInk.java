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
    static long                                started;

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
        busy    = true;
        answer  = null;
        started = System.nanoTime();
        recognizer.recognize(ink.build(), context).addOnSuccessListener(new OnSuccessListener<RecognitionResult>() {
            @Override
            public void onSuccess(RecognitionResult result) {
                StringBuilder s = new StringBuilder();
                for(RecognitionCandidate c : result.getCandidates()) {
                    s.append(c.getText()).append('\n');
                }
                finish(s.toString());
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(FudeAndroid.TAG, "recognition failed: " + e);
                finish("");
            }
        });
        return true;
    }

    static void finish(String s) {
        took   = (System.nanoTime() - started) / 1.0e6;
        answer = s;
        busy   = false;
    }

    /** The answer, once (candidates best first, one per line, UTF-8); null while none. */
    public static byte[] take() {
        final String a = answer;
        if(a == null) {
            return null;
        }
        answer = null;
        return FudeAndroid.bytes(a);
    }

    public static double took() {
        return took;
    }
}
