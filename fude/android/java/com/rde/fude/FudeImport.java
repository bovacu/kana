package com.rde.fude;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.IntentSender;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.mlkit.vision.documentscanner.GmsDocumentScanner;
import com.google.mlkit.vision.documentscanner.GmsDocumentScannerOptions;
import com.google.mlkit.vision.documentscanner.GmsDocumentScanning;
import com.google.mlkit.vision.documentscanner.GmsDocumentScanningResult;

import java.util.ArrayList;
import java.util.List;

/**
 * Documents brought in (import.h): pictures from the photo library, files (a PDF
 * or pictures) from the system's picker, pages scanned with Google's document
 * scanner (it finds the page's edges and straightens it). Each comes back as
 * copies in the app's cache — a PDF named .pdf, pictures as they are — for
 * import.c to make a canvas of. Kind as import.h's FUDE_IMPORT_: 0 files, 1
 * photos, 2 camera.
 */
public final class FudeImport {
    static volatile int    kind = -1;   // -1: nothing in
    static volatile String paths;       // one per line; "" when cancelled

    static void arrive(int k, List<Uri> uris, String folder) {
        FudeAndroid.clearCache(folder);
        StringBuilder s = new StringBuilder();
        int n = 0;
        for(Uri u : uris) {
            String name = FudeAndroid.displayName(u);
            String type = FudeAndroid.context.getContentResolver().getType(u);
            if(name == null || name.indexOf('.') < 0) {
                name = "page" + n + ("application/pdf".equals(type) ? ".pdf" : ".jpg");
            }
            String p = FudeAndroid.copyToCache(u, folder, n + "_" + name);
            if(p != null) {
                s.append(p).append('\n');
                n++;
            }
        }
        paths = s.toString();
        kind  = k;
    }

    static List<Uri> uris(Intent data) {
        ArrayList<Uri> out = new ArrayList<>();
        if(data == null) {
            return out;
        }
        ClipData clip = data.getClipData();
        if(clip != null) {
            for(int i = 0; i < clip.getItemCount(); i++) {
                out.add(clip.getItemAt(i).getUri());
            }
        } else if(data.getData() != null) {
            out.add(data.getData());
        }
        return out;
    }

    static void pick(final int k, Intent intent, final String folder) {
        FudeAndroid.ask(intent, null, new FudeAndroid.Answer() {
            @Override
            public void done(int result, Intent data) {
                if(result != Activity.RESULT_OK) {
                    paths = "";
                    kind  = k;
                    return;
                }
                final List<Uri> u = uris(data);
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        arrive(k, u, folder);
                    }
                }).start();
            }
        });
    }

    public static void pickPhotos() {
        Intent i;
        if(Build.VERSION.SDK_INT >= 33) {
            i = new Intent(MediaStore.ACTION_PICK_IMAGES).putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, 50);
        } else {
            i = new Intent(Intent.ACTION_GET_CONTENT).setType("image/*").putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }
        pick(1, i, "import_photos");
    }

    public static void pickFiles() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*")
            .putExtra(Intent.EXTRA_MIME_TYPES, new String[]{ "application/pdf", "image/*" })
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        pick(0, i, "import_files");
    }

    /** Google's document scanner (Play services): there? */
    public static boolean scanAvailable() {
        try {
            return GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(FudeAndroid.context) == ConnectionResult.SUCCESS;
        } catch(Exception e) {
            return false;
        }
    }

    public static void scan() {
        final Activity a = FudeAndroid.activity;
        if(a == null) {
            return;
        }
        GmsDocumentScannerOptions options = new GmsDocumentScannerOptions.Builder()
            .setGalleryImportAllowed(true)
            .setPageLimit(50)
            .setResultFormats(GmsDocumentScannerOptions.RESULT_FORMAT_JPEG)
            .setScannerMode(GmsDocumentScannerOptions.SCANNER_MODE_FULL)
            .build();
        GmsDocumentScanner scanner = GmsDocumentScanning.getClient(options);
        scanner.getStartScanIntent(a).addOnSuccessListener(new OnSuccessListener<IntentSender>() {
            @Override
            public void onSuccess(IntentSender sender) {
                FudeAndroid.ask(null, sender, new FudeAndroid.Answer() {
                    @Override
                    public void done(int result, Intent data) {
                        GmsDocumentScanningResult r = result == Activity.RESULT_OK ? GmsDocumentScanningResult.fromActivityResultIntent(data) : null;
                        final ArrayList<Uri> pages = new ArrayList<>();
                        if(r != null && r.getPages() != null) {
                            for(GmsDocumentScanningResult.Page p : r.getPages()) {
                                pages.add(p.getImageUri());
                            }
                        }
                        new Thread(new Runnable() {
                            @Override
                            public void run() {
                                arrive(2, pages, "import_scan");
                            }
                        }).start();
                    }
                });
            }
        }).addOnFailureListener(new OnFailureListener() {
            @Override
            public void onFailure(Exception e) {
                Log.e(FudeAndroid.TAG, "the document scanner could not start: " + e);
                paths = "";
                kind  = 2;
            }
        });
    }

    /** What came in: its kind (-1: nothing), then takePaths() the files, one per line. */
    public static int takeKind() {
        return kind;
    }

    public static byte[] takePaths() {
        final String p = paths;
        kind  = -1;
        paths = null;
        return FudeAndroid.bytes(p);
    }
}
