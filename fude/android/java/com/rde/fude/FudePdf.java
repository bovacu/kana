// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageDecoder;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.pdf.PdfDocument;
import android.graphics.pdf.PdfRenderer;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.ArrayList;

/**
 * PDFs (pdf.h): Android's PdfRenderer draws their pages; PdfDocument makes one of
 * pictures. A PDF in the app's own assets is copied out first (PdfRenderer wants
 * a file it can seek). Opened on the engine's thread, drawn on doc.c's worker —
 * one document drawn at a time, as PdfRenderer asks. Their own text and writing
 * one anew: FudePdfBox's (PDFBox), for the same file; reached from C through here.
 */
public final class FudePdf {
    static final ArrayList<PdfRenderer> open  = new ArrayList<>();
    static final ArrayList<File>        files = new ArrayList<>();   // ...and the file each reads (FudePdfBox's too)

    /** The PDF at _path (a file, or an asset path): a handle (>= 0), or -1. */
    public static int open(byte[] pathBytes) {
        String path = FudeAndroid.text(pathBytes);
        if(path.startsWith("./")) {
            path = path.substring(2);
        }
        try {
            File file = new File(path);
            if(!file.isAbsolute() || !file.exists()) {
                file = fromAssets(path);
            }
            if(file == null) {
                return -1;
            }
            PdfRenderer r = new PdfRenderer(ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY));
            synchronized(open) {
                for(int i = 0; i < open.size(); i++) {
                    if(open.get(i) == null) {
                        open.set(i, r);
                        files.set(i, file);
                        return i;
                    }
                }
                open.add(r);
                files.add(file);
                return open.size() - 1;
            }
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "could not open " + path + ": " + e);
            return -1;
        }
    }

    static File fromAssets(String path) {
        File out = new File(new File(FudeAndroid.context.getCacheDir(), "pdf"), path.replace('/', '_'));
        if(out.exists() && out.length() > 0) {
            return out;
        }
        out.getParentFile().mkdirs();
        try(InputStream in = FudeAndroid.context.getAssets().open(path); OutputStream o = new FileOutputStream(out)) {
            byte[] buf = new byte[1 << 16];
            for(int n; (n = in.read(buf)) > 0;) {
                o.write(buf, 0, n);
            }
            return out;
        } catch(Exception e) {
            out.delete();
            return null;
        }
    }

    static PdfRenderer get(int h) {
        synchronized(open) {
            return h >= 0 && h < open.size() ? open.get(h) : null;
        }
    }

    /** The file document _h reads; null when none is open there. */
    static File file(int h) {
        synchronized(open) {
            return h >= 0 && h < files.size() ? files.get(h) : null;
        }
    }

    public static void close(int h) {
        try {
            FudePdfBox.close(h);   // its text's first: the handle then free for another
        } catch(Throwable e) {
            Log.e(FudeAndroid.TAG, "could not let go of the text: " + e);
        }
        synchronized(open) {
            PdfRenderer r = get(h);
            if(r != null) {
                r.close();
                open.set(h, null);
                files.set(h, null);
            }
        }
    }

    public static int pageCount(int h) {
        PdfRenderer r = get(h);
        return r != null ? r.getPageCount() : 0;
    }

    /** Every page's size in points: w0, h0, w1, h1... */
    public static float[] pageSizes(int h) {
        PdfRenderer r = get(h);
        if(r == null) {
            return new float[0];
        }
        synchronized(r) {
            float[] s = new float[2 * r.getPageCount()];
            for(int i = 0; i < r.getPageCount(); i++) {
                PdfRenderer.Page p = r.openPage(i);
                s[2 * i]     = p.getWidth();
                s[2 * i + 1] = p.getHeight();
                p.close();
            }
            return s;
        }
    }

    /**
     * Part of page _page into _out (_w x _h RGBA pixels, top row first): the
     * rectangle at (_fx, _fy) of _sw x _sh points of the page turned _turn quarter
     * turns clockwise, stretched to fill them, on white.
     */
    public static boolean render(int h, int page, float fx, float fy, float sw, float sh, int w, int hh, int turn, ByteBuffer out) {
        PdfRenderer r = get(h);
        if(r == null || page < 0 || page >= r.getPageCount()) {
            return false;
        }
        synchronized(r) {
            PdfRenderer.Page p = r.openPage(page);
            try {
                final float pw = p.getWidth(), ph = p.getHeight();
                Matrix m = new Matrix();
                switch(turn & 3) {
                    case 1: m.postRotate(90);  m.postTranslate(ph, 0);  break;
                    case 2: m.postRotate(180); m.postTranslate(pw, ph); break;
                    case 3: m.postRotate(270); m.postTranslate(0, pw);  break;
                    default: break;
                }
                m.postTranslate(-fx, -fy);
                m.postScale(w / sw, hh / sh);
                Bitmap b = Bitmap.createBitmap(w, hh, Bitmap.Config.ARGB_8888);
                b.eraseColor(Color.WHITE);
                p.render(b, null, m, PdfRenderer.Page.RENDER_MODE_FOR_DISPLAY);
                out.rewind();
                b.copyPixelsToBuffer(out);
                b.recycle();
                return true;
            } catch(Exception e) {
                Log.e(FudeAndroid.TAG, "could not draw page " + page + ": " + e);
                return false;
            } finally {
                p.close();
            }
        }
    }

    /**
     * A picture (its path) as JPEG bytes — PNG when it has see-through parts —
     * upright, as taken, at most _max pixels on its longer side (Sketching's
     * pictures). Null when it cannot be read.
     */
    public static byte[] pictureJpeg(byte[] pathBytes, int max) {
        try {
            final String path = FudeAndroid.text(pathBytes);
            Bitmap b = FudeText.decode(android.net.Uri.fromFile(new File(path)), max);
            java.io.ByteArrayOutputStream jpeg = new java.io.ByteArrayOutputStream();
            b.compress(b.hasAlpha() ? Bitmap.CompressFormat.PNG : Bitmap.CompressFormat.JPEG, 86, jpeg);
            b.recycle();
            return jpeg.toByteArray();
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "could not read the picture: " + e);
            return null;
        }
    }

    /**
     * Pictures (paths, one per line) as a new PDF at _out: a page each, upright, as
     * big as the picture at 150 dots an inch (at most _max pixels on its longer side).
     */
    public static boolean fromImages(byte[] pathsBytes, byte[] outBytes, int max) {
        PdfDocument doc = new PdfDocument();
        int pages = 0;
        try {
            for(String path : FudeAndroid.text(pathsBytes).split("\n")) {
                if(path.isEmpty()) {
                    continue;
                }
                Bitmap b;
                try {
                    b = FudeText.decode(android.net.Uri.fromFile(new File(path)), max);
                } catch(Exception e) {
                    Log.e(FudeAndroid.TAG, "could not read " + path + ": " + e);
                    continue;
                }
                final int pw = Math.max(1, Math.round(b.getWidth() * 72.0f / 150.0f));
                final int ph = Math.max(1, Math.round(b.getHeight() * 72.0f / 150.0f));
                PdfDocument.Page page = doc.startPage(new PdfDocument.PageInfo.Builder(pw, ph, pages + 1).create());
                Canvas c = page.getCanvas();
                c.drawBitmap(b, null, new Rect(0, 0, pw, ph), new Paint(Paint.FILTER_BITMAP_FLAG));
                doc.finishPage(page);
                b.recycle();
                pages++;
            }
            if(pages == 0) {
                return false;
            }
            try(OutputStream o = new FileOutputStream(FudeAndroid.text(outBytes))) {
                doc.writeTo(o);
            }
            return true;
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "could not write the PDF: " + e);
            return false;
        } finally {
            doc.close();
        }
    }

    // --- a PDF's own text, and writing one anew: FudePdfBox's -------------------------------
    // Positions: points from a page's top-left, the page as the PDF reads it (turned as it
    // says; the learner's turns are pdf_android.c's). Text crosses as UTF-8 bytes.

    /** Can its text be read, and one be written (PDFBox there and readied)? */
    public static boolean textAvailable() {
        return FudePdfBox.available();
    }

    /** Has page _page of document _h text of its own (not a picture of some: a scan's)? */
    public static boolean pageHasText(int h, int page) {
        return FudePdfBox.pageHasText(h, page);
    }

    /** The text in the rectangle at (_x, _y) of _w x _hh on page _page, its lines as \n. */
    public static byte[] textIn(int h, int page, float x, float y, float w, float hh) {
        return FudeAndroid.bytes(FudePdfBox.textIn(h, page, x, y, w, hh));
    }

    /** A search for _query (case, accents and full- or half-width alike), on a thread of its own; at most _max matches. */
    public static void findStart(int h, byte[] query, int max) {
        FudePdfBox.findStart(h, FudeAndroid.text(query), max);
    }

    public static void findStop(int h) {
        FudePdfBox.findStop(h);
    }

    /** The search's matches so far — version, done (1 or 0), count, then page, x, y, w, h each — or null when none changed since version _known. */
    public static float[] findMatches(int h, int known) {
        return FudePdfBox.findMatches(h, known);
    }

    /** A new PDF to write at _path: a handle (>= 0), or -1. */
    public static int writeBegin(byte[] path) {
        return FudePdfBox.writeBegin(FudeAndroid.text(path));
    }

    /** A page: page _page of document _h, turned _turn quarter turns more than the PDF says. */
    public static void writePage(int w, int h, int page, int turn) {
        FudePdfBox.writePage(w, h, page, turn);
    }

    /** A stroke over it: x, y and half-width each point (_points), in _color (RGBA, a byte each from the top); _even: a marker's. */
    public static void writeStroke(int w, float[] points, int color, boolean even) {
        FudePdfBox.writeStroke(w, points, color, even);
    }

    /** Text put in unseen, filling the box at (_x, _y) of _sw x _sh. */
    public static void writeHiddenText(int w, byte[] text, float x, float y, float sw, float sh) {
        FudePdfBox.writeHiddenText(w, FudeAndroid.text(text), x, y, sw, sh);
    }

    public static void writePageEnd(int w) {
        FudePdfBox.writePageEnd(w);
    }

    /** Written, and let go: false when it could not be. */
    public static boolean writeEnd(int w) {
        return FudePdfBox.writeEnd(w);
    }
}
