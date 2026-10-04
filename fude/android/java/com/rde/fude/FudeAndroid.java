// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.database.Cursor;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

/**
 * What the other Fude* classes share: the app's context, its main thread, and a
 * way to start system UI (a picker, the document scanner) and have its answer
 * back — a headless fragment on the activity, as the engine's activity hands
 * results to no one. Everything here is called from C (fude/drawing/base/
 * android.c) on the engine's thread; answers are left in fields that C polls,
 * never called back. Text crosses as UTF-8 bytes, not jstrings (JNI's modified
 * UTF-8 has no 4-byte characters: CJK's extensions).
 */
public final class FudeAndroid {
    static final String TAG = "Fude";
    static volatile Activity activity;
    static volatile Context  context;
    static final Handler main = new Handler(Looper.getMainLooper());

    /** Once, at start. */
    public static void init(Activity a) {
        activity = a;
        context  = a.getApplicationContext();
    }

    static String text(byte[] utf8) {
        return utf8 != null ? new String(utf8, StandardCharsets.UTF_8) : "";
    }

    static byte[] bytes(String s) {
        return s != null ? s.getBytes(StandardCharsets.UTF_8) : new byte[0];
    }

    /** What a request's answer is handed to, on the main thread. */
    interface Answer {
        void done(int resultCode, Intent data);
    }

    /** One request: started when attached, its result passed on, then gone. */
    public static final class Asker extends Fragment {
        Answer       answer;
        Intent       intent;
        IntentSender sender;

        @Override
        public void onCreate(Bundle state) {
            super.onCreate(state);
            try {
                if(sender != null) {
                    startIntentSenderForResult(sender, 4711, null, 0, 0, 0, null);
                } else if(intent != null) {
                    startActivityForResult(intent, 4711);
                }
            } catch(Exception e) {
                Log.e(TAG, "could not start " + e);
                finish(Activity.RESULT_CANCELED, null);
            }
        }

        @Override
        public void onActivityResult(int request, int result, Intent data) {
            finish(result, data);
        }

        void finish(int result, Intent data) {
            final Answer a = answer;
            answer = null;
            if(a != null) {
                a.done(result, data);
            }
            try {
                FragmentManager fm = getFragmentManager();
                if(fm != null) {
                    fm.beginTransaction().remove(this).commitAllowingStateLoss();
                }
            } catch(Exception e) {
                // already gone
            }
        }
    }

    /** Starts _intent (or _sender) over the app; _answer gets its result. */
    static void ask(final Intent intent, final IntentSender sender, final Answer answer) {
        main.post(new Runnable() {
            @Override
            public void run() {
                Activity a = activity;
                if(a == null) {
                    answer.done(Activity.RESULT_CANCELED, null);
                    return;
                }
                Asker f   = new Asker();
                f.answer  = answer;
                f.intent  = intent;
                f.sender  = sender;
                a.getFragmentManager().beginTransaction().add(f, "fude_asker").commitAllowingStateLoss();
            }
        });
    }

    /** A file's own name, from its provider (null when it says none). */
    static String displayName(Uri uri) {
        try(Cursor c = context.getContentResolver().query(uri, new String[]{ OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if(c != null && c.moveToFirst()) {
                return c.getString(0);
            }
        } catch(Exception e) {
            // none
        }
        return null;
    }

    /** _uri's bytes into a new file in the cache, named _name: its path, or null. */
    static String copyToCache(Uri uri, String folder, String name) {
        try {
            File dir = new File(context.getCacheDir(), folder);
            dir.mkdirs();
            File out = new File(dir, name);
            ContentResolver r = context.getContentResolver();
            try(InputStream in = r.openInputStream(uri); OutputStream o = new FileOutputStream(out)) {
                if(in == null) {
                    return null;
                }
                byte[] buf = new byte[1 << 16];
                for(int n; (n = in.read(buf)) > 0;) {
                    o.write(buf, 0, n);
                }
            }
            return out.getAbsolutePath();
        } catch(Exception e) {
            Log.e(TAG, "could not copy " + uri + ": " + e);
            return null;
        }
    }

    /** A cache folder emptied (the last picks'). */
    static void clearCache(String folder) {
        File dir = new File(context.getCacheDir(), folder);
        File[] files = dir.listFiles();
        if(files != null) {
            for(File f : files) {
                f.delete();
            }
        }
    }
}
