// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.ImageDecoder;
import android.graphics.Point;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Log;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.mlkit.vision.common.InputImage;
import com.google.mlkit.vision.text.Text;
import com.google.mlkit.vision.text.TextRecognition;
import com.google.mlkit.vision.text.TextRecognizer;
import com.google.mlkit.vision.text.TextRecognizerOptionsInterface;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;

/**
 * Text in pictures (textscan.h): ML Kit Text Recognition with the language's own
 * model, bundled — read in a photo picked from the library, a camera frame, a
 * document's page, each a channel of its own (0, 1, 2). A result crosses as text:
 * a line each, "block \t text \t x0 \t y0 ... \t x3 \t y3", its corners clockwise
 * from the top-left as the text runs, in the picture's (upright) pixels.
 */
public final class FudeText {
    static TextRecognizer recognizer;

    // The photo's state (textscan.h's FUDE_TEXTSCAN_STATE_): 0 idle, 1 picking,
    // 2 reading, 3 done, 4 cancelled, 5 failed.
    static volatile int    photoState;
    static volatile byte[] photoJpeg;
    static volatile int    photoW, photoH;
    static final String[]  lines = new String[3];      // per channel: the last result (null: none in)
    static final int[]     width = new int[3], height = new int[3];
    static final boolean[] busy  = new boolean[3];

    /** The recognizer for the language taught ("ja", "zh", "ko", "hi"), by name: each app has only its own (Thai has none: lang.h). */
    public static boolean start(byte[] code) {
        if(recognizer != null) {
            return true;
        }
        final String c = FudeAndroid.text(code);
        final String name = c.equals("zh") ? "com.google.mlkit.vision.text.chinese.ChineseTextRecognizerOptions"
                          : c.equals("ko") ? "com.google.mlkit.vision.text.korean.KoreanTextRecognizerOptions"
                          : c.equals("hi") ? "com.google.mlkit.vision.text.devanagari.DevanagariTextRecognizerOptions"
                          : "com.google.mlkit.vision.text.japanese.JapaneseTextRecognizerOptions";
        try {
            Object builder = Class.forName(name + "$Builder").getConstructor().newInstance();
            Object options = builder.getClass().getMethod("build").invoke(builder);
            recognizer = TextRecognition.getClient((TextRecognizerOptionsInterface)options);
            return true;
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "no text recognizer for " + c + ": " + e);
            return false;
        }
    }

    static void read(final int channel, Bitmap bitmap, int rotation, final int w, final int h) {
        synchronized(busy) {
            busy[channel] = true;
        }
        recognizer.process(InputImage.fromBitmap(bitmap, rotation)).addOnSuccessListener(new OnSuccessListener<Text>() {
            @Override
            public void onSuccess(Text text) {
                StringBuilder s = new StringBuilder();
                int block = 0;
                for(Text.TextBlock b : text.getTextBlocks()) {
                    for(Text.Line l : b.getLines()) {
                        Point[] c = l.getCornerPoints();
                        if(c == null || c.length < 4) {
                            continue;
                        }
                        s.append(block).append('\t').append(l.getText().replace('\t', ' ').replace('\n', ' '));
                        for(int k = 0; k < 4; k++) {
                            s.append('\t').append(c[k].x).append('\t').append(c[k].y);
                        }
                        s.append('\n');
                    }
                    block++;
                }
                finish(channel, s.toString(), w, h);
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(FudeAndroid.TAG, "text recognition failed: " + e);
                finish(channel, null, w, h);
            }
        });
    }

    static void finish(int channel, String s, int w, int h) {
        synchronized(busy) {
            lines[channel]  = s != null ? s : "";
            width[channel]  = w;
            height[channel] = h;
            busy[channel]   = false;
        }
        if(channel == 0) {
            photoState = s != null ? 3 : 5;
        }
    }

    // --- a photo from the library ------------------------------------------------------

    public static boolean pick() {
        if(recognizer == null || photoState == 1 || photoState == 2) {
            return false;
        }
        photoState = 1;
        Intent pick = Build.VERSION.SDK_INT >= 33 ? new Intent(MediaStore.ACTION_PICK_IMAGES) : new Intent(Intent.ACTION_GET_CONTENT).setType("image/*");
        FudeAndroid.ask(pick, null, new FudeAndroid.Answer() {
            @Override
            public void done(int result, Intent data) {
                if(result != Activity.RESULT_OK || data == null || data.getData() == null) {
                    photoState = 4;
                    return;
                }
                final Uri uri = data.getData();
                photoState = 2;
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        readPhoto(uri);
                    }
                }).start();
            }
        });
        return true;
    }

    /** A picture's pixels, upright (as taken), at most _max on its longer side. */
    static Bitmap decode(Uri uri, final int max) throws Exception {
        ImageDecoder.Source src = ImageDecoder.createSource(FudeAndroid.context.getContentResolver(), uri);
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

    static void readPhoto(Uri uri) {
        try {
            Bitmap b = decode(uri, 3000);
            ByteArrayOutputStream jpeg = new ByteArrayOutputStream();
            b.compress(Bitmap.CompressFormat.JPEG, 88, jpeg);
            photoJpeg = jpeg.toByteArray();
            photoW    = b.getWidth();
            photoH    = b.getHeight();
            read(0, b, 0, photoW, photoH);
        } catch(Exception e) {
            Log.e(FudeAndroid.TAG, "the photo could not be read: " + e);
            photoState = 5;
        }
    }

    /** The photo's state; DONE, CANCELLED and FAILED once, then idle. */
    public static int photoState() {
        final int s = photoState;
        if(s >= 3) {
            photoState = 0;
        }
        return s;
    }

    public static byte[] photoJpeg() {
        return photoJpeg;
    }

    // --- a camera frame (1) or a page (2): RGBA pixels ----------------------------------

    public static boolean readPixels(int channel, byte[] rgba, int w, int h, int rotation) {
        synchronized(busy) {
            if(recognizer == null || busy[channel]) {
                return false;
            }
            busy[channel]  = true;
            lines[channel] = null;
        }
        Bitmap b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        b.copyPixelsFromBuffer(ByteBuffer.wrap(rgba));
        final boolean sideways = rotation == 90 || rotation == 270;
        read(channel, b, rotation, sideways ? h : w, sideways ? w : h);
        return true;
    }

    /** A channel's lines, once (null while none in); width()/height() the picture's then. */
    public static byte[] takeLines(int channel) {
        synchronized(busy) {
            final String s = lines[channel];
            if(s == null || busy[channel]) {
                return null;
            }
            lines[channel] = null;
            return FudeAndroid.bytes(s);
        }
    }

    public static int width(int channel) {
        return channel == 0 ? photoW : width[channel];
    }

    public static int height(int channel) {
        return channel == 0 ? photoH : height[channel];
    }
}
