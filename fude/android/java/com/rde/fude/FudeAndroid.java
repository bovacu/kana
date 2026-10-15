// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.graphics.Bitmap;
import android.graphics.ImageDecoder;
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

    /**
     * Crash reports (fude/drawing/base/crash.h): a Java exception nothing catches written to the report (the C side's
     * header first, then the exception and its stack), then handed on as before (the system ends the app).
     */
    public static void crashInstall(byte[] path, byte[] header) {
        final String where = text(path), head = text(header);
        final Thread.UncaughtExceptionHandler before = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override public void uncaughtException(Thread t, Throwable e) {
                try (FileOutputStream out = new FileOutputStream(where)) {
                    StringBuilder b = new StringBuilder(head);
                    b.append("What: a Java exception on the thread ").append(t.getName()).append(": ").append(e).append("\nWhere:\n");
                    for(Throwable c = e; c != null; c = c.getCause()) {
                        if(c != e) {
                            b.append("Caused by: ").append(c).append('\n');
                        }
                        StackTraceElement[] s = c.getStackTrace();
                        for(int i = 0; i < s.length && i < 40; i++) {
                            b.append("  at ").append(s[i]).append('\n');
                        }
                    }
                    out.write(bytes(b.toString()));
                } catch(Throwable ignored) {
                    // (no report: the system's own handling still goes on)
                }
                if(before != null) {
                    before.uncaughtException(t, e);
                }
            }
        });
    }

    /**
     * A file downloaded on a thread of its own (fude/zoom/fetch.h): into path + ".part" as it comes, renamed when
     * whole. One at a time; its state polled (fetchState: { state, bytes got, bytes in all }).
     */
    static volatile int  fetchStatus = 0;   // 0 idle, 1 running, 2 done, 3 failed
    static volatile long fetchGot = 0, fetchTotal = 0;

    public static boolean fetchStart(byte[] url, byte[] path) {
        if(fetchStatus == 1) {
            return false;
        }
        final String from = text(url), to = text(path);
        fetchStatus = 1;
        fetchGot    = 0;
        fetchTotal  = 0;
        new Thread(new Runnable() {
            @Override public void run() {
                File part = new File(to + ".part");
                try {
                    java.net.URL u = new java.net.URL(from);
                    java.net.HttpURLConnection c = null;
                    // (the redirects followed by hand: a CDN's may change host)
                    for(int hops = 0; hops < 8; hops++) {
                        c = (java.net.HttpURLConnection)u.openConnection();
                        c.setInstanceFollowRedirects(false);
                        c.setConnectTimeout(20000);
                        c.setReadTimeout(30000);
                        int code = c.getResponseCode();
                        if(code >= 300 && code < 400 && c.getHeaderField("Location") != null) {
                            u = new java.net.URL(u, c.getHeaderField("Location"));
                            c.disconnect();
                            continue;
                        }
                        if(code < 200 || code >= 300) {
                            throw new java.io.IOException("HTTP " + code);
                        }
                        break;
                    }
                    fetchTotal = c.getContentLengthLong();
                    File dir = part.getParentFile();
                    if(dir != null) {
                        dir.mkdirs();
                    }
                    try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(part)) {
                        byte[] buf = new byte[1 << 16];
                        int n;
                        while((n = in.read(buf)) > 0) {
                            out.write(buf, 0, n);
                            fetchGot += n;
                        }
                    }
                    c.disconnect();
                    File dest = new File(to);
                    dest.delete();
                    fetchStatus = part.renameTo(dest) ? 2 : 3;
                } catch(Throwable e) {
                    Log.w(TAG, "fetch: " + e);
                    part.delete();
                    fetchStatus = 3;
                }
            }
        }, "FudeFetch").start();
        return true;
    }

    public static long[] fetchState() {
        return new long[] { fetchStatus, fetchGot, fetchTotal };
    }

    static String text(byte[] utf8) {
        return utf8 != null ? new String(utf8, StandardCharsets.UTF_8) : "";
    }

    static byte[] bytes(String s) {
        return s != null ? s.getBytes(StandardCharsets.UTF_8) : new byte[0];
    }

    /**
     * A picture's pixels, upright (as taken), at most max pixels on its longer side, in memory the CPU can read (a
     * software bitmap). Shared by the documents made from pictures (FudePdf) and the study apps' photos (FudeText).
     */
    static Bitmap decodePicture(Uri uri, final int max) throws Exception {
        ImageDecoder.Source src = ImageDecoder.createSource(context.getContentResolver(), uri);
        return ImageDecoder.decodeBitmap(src, new ImageDecoder.OnHeaderDecodedListener() {
            @Override
            public void onHeaderDecoded(ImageDecoder d, ImageDecoder.ImageInfo info, ImageDecoder.Source s) {
                d.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                final int w = info.getSize().getWidth(), h = info.getSize().getHeight();
                final int longer = Math.max(w, h);
                if(longer > max) {
                    d.setTargetSize(Math.max(1, w * max / longer), Math.max(1, h * max / longer));
                }
            }
        });
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
