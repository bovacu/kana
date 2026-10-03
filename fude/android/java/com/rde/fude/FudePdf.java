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
 * one document drawn at a time, as PdfRenderer asks.
 */
public final class FudePdf {
    static final ArrayList<PdfRenderer> open = new ArrayList<>();

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
                        return i;
                    }
                }
                open.add(r);
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

    public static void close(int h) {
        synchronized(open) {
            PdfRenderer r = get(h);
            if(r != null) {
                r.close();
                open.set(h, null);
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
}
