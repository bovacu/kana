// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.util.Log;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.mlkit.common.model.DownloadConditions;
import com.google.mlkit.common.model.RemoteModelManager;
import com.google.mlkit.nl.translate.TranslateRemoteModel;
import com.google.mlkit.nl.translate.Translation;
import com.google.mlkit.nl.translate.Translator;
import com.google.mlkit.nl.translate.TranslatorOptions;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/** Translation (translate.h): ML Kit's on-device translator, each language's model downloaded once. */
public final class FudeTranslate {
    // translate.h's FUDE_TRANSLATE_STATE_: 1 missing, 2 downloading, 3 ready, 4 failed.
    static final HashMap<String, Translator> translators = new HashMap<>();
    static final HashMap<String, Integer>    pairs       = new HashMap<>();   // a pair's state while downloading or failed
    static volatile Set<String>              downloaded  = new HashSet<>();   // the languages on the device ("en" ...)
    static volatile boolean                  asked;
    static final ArrayDeque<Object[]>        answers     = new ArrayDeque<>(); // { ticket, text }

    static String key(String from, String to) {
        return from + ">" + to;
    }

    /** Which languages' models are on the device: asked once, then kept up to date. */
    static void refresh() {
        asked = true;
        RemoteModelManager.getInstance().getDownloadedModels(TranslateRemoteModel.class).addOnSuccessListener(new OnSuccessListener<Set<TranslateRemoteModel>>() {
            @Override
            public void onSuccess(Set<TranslateRemoteModel> models) {
                HashSet<String> have = new HashSet<>();
                for(TranslateRemoteModel m : models) {
                    have.add(m.getLanguage());
                }
                downloaded = have;
            }
        });
    }

    public static int state(byte[] fromBytes, byte[] toBytes) {
        if(!asked) {
            refresh();
        }
        final String from = FudeAndroid.text(fromBytes), to = FudeAndroid.text(toBytes);
        synchronized(pairs) {
            Integer s = pairs.get(key(from, to));
            if(s != null) {
                return s;
            }
        }
        final Set<String> have = downloaded;
        return have.contains(from) && have.contains(to) ? 3 : 1;
    }

    static Translator translator(String from, String to) {
        synchronized(translators) {
            Translator t = translators.get(key(from, to));
            if(t == null) {
                t = Translation.getClient(new TranslatorOptions.Builder().setSourceLanguage(from).setTargetLanguage(to).build());
                translators.put(key(from, to), t);
            }
            return t;
        }
    }

    public static void prepare(byte[] fromBytes, byte[] toBytes) {
        final String from = FudeAndroid.text(fromBytes), to = FudeAndroid.text(toBytes);
        synchronized(pairs) {
            Integer s = pairs.get(key(from, to));
            if(s != null && s == 2) {
                return;
            }
            pairs.put(key(from, to), 2);
        }
        translator(from, to).downloadModelIfNeeded(new DownloadConditions.Builder().build()).addOnSuccessListener(new OnSuccessListener<Void>() {
            @Override
            public void onSuccess(Void v) {
                synchronized(pairs) {
                    pairs.remove(key(from, to));
                }
                HashSet<String> have = new HashSet<>(downloaded);
                have.add(from);
                have.add(to);
                downloaded = have;
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(FudeAndroid.TAG, "translation models could not be downloaded: " + e);
                synchronized(pairs) {
                    pairs.put(key(from, to), 4);
                }
            }
        });
    }

    /** Starts translating; its answer comes to take() with _ticket. */
    public static void translate(final int ticket, byte[] text, byte[] fromBytes, byte[] toBytes) {
        final String from = FudeAndroid.text(fromBytes), to = FudeAndroid.text(toBytes);
        translator(from, to).translate(FudeAndroid.text(text)).addOnSuccessListener(new OnSuccessListener<String>() {
            @Override
            public void onSuccess(String s) {
                synchronized(answers) {
                    answers.add(new Object[]{ ticket, s });
                }
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(FudeAndroid.TAG, "translation failed: " + e);
                synchronized(answers) {
                    answers.add(new Object[]{ ticket, "" });
                }
            }
        });
    }

    /** The next answer's ticket (0: none in); then takeText() its text. */
    public static int takeTicket() {
        synchronized(answers) {
            Object[] a = answers.peek();
            return a != null ? (Integer)a[0] : 0;
        }
    }

    public static byte[] takeText() {
        synchronized(answers) {
            Object[] a = answers.poll();
            return a != null ? FudeAndroid.bytes((String)a[1]) : new byte[0];
        }
    }
}
