// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

package com.rde.fude;

import android.media.Image;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.util.Log;

import java.io.File;
import java.nio.ByteBuffer;

/**
 * A video made frame by frame (Sketching's zoom video, video.h): each frame handed
 * in as YUV 4:2:0 (I420: Y, U, V planes packed, from C), put into the encoder's own
 * input image as its planes lie, encoded as H.264 by MediaCodec and written as an
 * MP4 by MediaMuxer. One at a time, on the engine's thread.
 */
public final class FudeVideo {
    static MediaCodec            codec;
    static MediaMuxer            muxer;
    static MediaCodec.BufferInfo info;
    static String                path;
    static int                   track = -1, width, height, fps;
    static long                  frames;
    static boolean               muxing, ended;

    /** A new video at pathBytes (UTF-8), w × h (even), rate frames a second: begun? */
    public static boolean open(byte[] pathBytes, int w, int h, int rate) {
        drop();
        try {
            path   = FudeAndroid.text(pathBytes);
            width  = w;
            height = h;
            fps    = Math.max(1, rate);
            new File(path).delete();
            MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, w, h);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            format.setInteger(MediaFormat.KEY_BIT_RATE, Math.max(2_000_000, w * h * fps / 5));   // (sharp lines: generous)
            format.setInteger(MediaFormat.KEY_FRAME_RATE, fps);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            codec.start();
            muxer  = new MediaMuxer(path, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            info   = new MediaCodec.BufferInfo();
            track  = -1;
            frames = 0;
            muxing = false;
            ended  = false;
            return true;
        } catch(Exception e) {
            Log.e("fude", "FudeVideo.open", e);
            drop();
            return false;
        }
    }

    /** The next frame (I420, width × height × 3 / 2 bytes): taken? */
    public static boolean add(byte[] yuv) {
        if(codec == null) {
            return false;
        }
        try {
            int index = -1;
            for(int tries = 0; tries < 300 && index < 0; tries++) {   // (the encoder busy: what it made taken out, then again)
                index = codec.dequeueInputBuffer(10_000);
                if(index < 0) {
                    drain(false);
                }
            }
            if(index < 0) {
                return false;
            }
            Image image = codec.getInputImage(index);
            if(image == null) {
                return false;
            }
            Image.Plane[] planes = image.getPlanes();
            put(planes[0], yuv, 0, width, height);
            put(planes[1], yuv, width * height, width / 2, height / 2);
            put(planes[2], yuv, width * height + (width / 2) * (height / 2), width / 2, height / 2);
            codec.queueInputBuffer(index, 0, width * height * 3 / 2, frames * 1_000_000L / fps, 0);
            frames++;
            drain(false);
            return true;
        } catch(Exception e) {
            Log.e("fude", "FudeVideo.add", e);
            return false;
        }
    }

    /** Finished: the file written (keep), or thrown away. A good file? */
    public static boolean close(boolean keep) {
        if(codec == null) {
            return false;
        }
        boolean ok = false;
        try {
            if(keep) {
                int index = codec.dequeueInputBuffer(1_000_000);
                if(index >= 0) {
                    codec.queueInputBuffer(index, 0, 0, frames * 1_000_000L / fps, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    drain(true);
                }
                ok = muxing && frames > 0 && ended;
            }
        } catch(Exception e) {
            Log.e("fude", "FudeVideo.close", e);
            ok = false;
        }
        String was = path;
        drop();
        if(!ok && was != null) {
            new File(was).delete();
        }
        return ok;
    }

    // A plane of rows w long, h of them, from yuv at from — as the image's plane lies (its rows' and its pixels' strides).
    static void put(Image.Plane plane, byte[] yuv, int from, int w, int h) {
        ByteBuffer b     = plane.getBuffer();
        int        row   = plane.getRowStride();
        int        pixel = plane.getPixelStride();
        if(pixel == 1) {
            for(int y = 0; y < h; y++) {
                b.position(y * row);
                b.put(yuv, from + y * w, w);
            }
            return;
        }
        for(int y = 0; y < h; y++) {
            int at = y * row, src = from + y * w;
            for(int x = 0; x < w; x++) {
                b.put(at + x * pixel, yuv[src + x]);
            }
        }
    }

    // What the encoder has made, into the file (waiting for its end when the stream has been ended).
    static void drain(boolean toEnd) {
        int idle = 0;
        while(true) {
            int out = codec.dequeueOutputBuffer(info, toEnd ? 10_000 : 0);
            if(out == MediaCodec.INFO_TRY_AGAIN_LATER) {
                if(!toEnd || ++idle > 500) {
                    return;
                }
                continue;
            }
            if(out == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                if(!muxing) {
                    track  = muxer.addTrack(codec.getOutputFormat());
                    muxer.start();
                    muxing = true;
                }
                continue;
            }
            if(out < 0) {
                continue;
            }
            ByteBuffer data = codec.getOutputBuffer(out);
            if((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                info.size = 0;   // (in the track's format already)
            }
            if(info.size > 0 && muxing && data != null) {
                data.position(info.offset);
                data.limit(info.offset + info.size);
                muxer.writeSampleData(track, data, info);
            }
            codec.releaseOutputBuffer(out, false);
            if((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                ended = true;
                return;
            }
        }
    }

    // Everything let go.
    static void drop() {
        if(codec != null) {
            try { codec.stop(); } catch(Exception e) { }
            try { codec.release(); } catch(Exception e) { }
        }
        if(muxer != null) {
            if(muxing) {
                try { muxer.stop(); } catch(Exception e) { }
            }
            try { muxer.release(); } catch(Exception e) { }
        }
        codec  = null;
        muxer  = null;
        muxing = false;
        track  = -1;
        path   = null;
    }
}
