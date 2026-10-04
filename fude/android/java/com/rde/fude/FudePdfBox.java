package com.rde.fude;

import android.util.Log;

import com.tom_roush.fontbox.FontBoxFont;
import com.tom_roush.fontbox.ttf.TTFParser;
import com.tom_roush.fontbox.ttf.TrueTypeFont;
import com.tom_roush.pdfbox.android.PDFBoxResourceLoader;
import com.tom_roush.pdfbox.contentstream.operator.Operator;
import com.tom_roush.pdfbox.cos.COSArray;
import com.tom_roush.pdfbox.cos.COSBase;
import com.tom_roush.pdfbox.cos.COSDictionary;
import com.tom_roush.pdfbox.cos.COSInteger;
import com.tom_roush.pdfbox.cos.COSName;
import com.tom_roush.pdfbox.cos.COSStream;
import com.tom_roush.pdfbox.pdfparser.PDFStreamParser;
import com.tom_roush.pdfbox.pdmodel.PDDocument;
import com.tom_roush.pdfbox.pdmodel.PDPage;
import com.tom_roush.pdfbox.pdmodel.PDResources;
import com.tom_roush.pdfbox.pdmodel.common.PDRectangle;
import com.tom_roush.pdfbox.pdmodel.font.CIDFontMapping;
import com.tom_roush.pdfbox.pdmodel.font.FontMapper;
import com.tom_roush.pdfbox.pdmodel.font.FontMappers;
import com.tom_roush.pdfbox.pdmodel.font.FontMapping;
import com.tom_roush.pdfbox.pdmodel.font.PDCIDSystemInfo;
import com.tom_roush.pdfbox.pdmodel.font.PDFont;
import com.tom_roush.pdfbox.pdmodel.font.PDFontDescriptor;
import com.tom_roush.pdfbox.pdmodel.graphics.PDXObject;
import com.tom_roush.pdfbox.pdmodel.graphics.form.PDFormXObject;
import com.tom_roush.pdfbox.pdmodel.graphics.state.PDExtendedGraphicsState;
import com.tom_roush.pdfbox.text.PDFTextStripper;
import com.tom_roush.pdfbox.text.TextPosition;
import com.tom_roush.pdfbox.util.Matrix;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * A PDF's own text, and a PDF written anew (pdf.h), by PDFBox (PdfBox-Android,
 * Apache License 2.0): what PdfRenderer does not do. A document of PDFBox's own
 * for the file FudePdf draws, opened the first time its text is asked for: whether
 * a page has text (a scan's has none), the text in a rectangle of it, a search
 * through it with where each match is; and a new PDF of pages as they are (their
 * content kept as it is, not drawn again) with strokes and unseen text over them.
 *
 * Positions: points from the page's top-left, the page as the PDF reads it (turned
 * as it says); the learner's turns are pdf_android.c's. Every use of PDFBox holds
 * LOCK: the engine's thread asks, and a search goes on in a thread of its own — its
 * matches read without waiting for it (each document's own lock, found).
 */
final class FudePdfBox {
    static final Object LOCK = new Object();
    static int ready;                  // 1: PDFBox readied, -1: not here (0: not tried)
    static TrueTypeFont fallback;      // the font PDFBox measures with where a PDF's own is not in it
    static final int CHARS_KEPT = 400000;   // pages' text kept, a document's, the last asked for: about this many characters

    // Every document's text, by FudePdf's handle; the PDFs being written.
    static final HashMap<Integer, Text> texts = new HashMap<>();
    static final ArrayList<Out>         outs  = new ArrayList<>();

    // The searches' thread.
    static final ExecutorService FINDER = Executors.newSingleThreadExecutor(new ThreadFactory() {
        @Override
        public Thread newThread(Runnable r) {
            Thread t = new Thread(r, "FudePdfFind");
            t.setDaemon(true);
            t.setPriority(Thread.NORM_PRIORITY - 1);
            return t;
        }
    });

    // What PDFTextStripper writes besides what is kept (Reader): nowhere.
    static final Writer NOWHERE = new Writer() {
        @Override public void write(char[] b, int off, int len) {}
        @Override public void flush() {}
        @Override public void close() {}
    };

    /** Is PDFBox here? Readied the first time: its resources (in the app's assets) and the fonts it looks for. */
    static boolean available() {
        synchronized(LOCK) {
            if(ready == 0) {
                try {
                    PDFBoxResourceLoader.init(FudeAndroid.context);
                    FontMappers.set(new Fonts());
                    ready = 1;
                } catch(Throwable t) {
                    Log.e(FudeAndroid.TAG, "no PDFBox: " + t);
                    ready = -1;
                }
            }
            return ready > 0;
        }
    }

    /**
     * The fonts PDFBox finds for a PDF's that are not in it: always the one it
     * carries (Liberation Sans). Its own way looks through every font on the device
     * the first time — seconds, on a phone — and only drawing needs the right one:
     * the text is read from the PDF's own widths and character maps.
     */
    static final class Fonts implements FontMapper {
        static TrueTypeFont font() {
            if(fallback == null) {
                try(InputStream in = PDFBoxResourceLoader.getStream("com/tom_roush/pdfbox/resources/ttf/LiberationSans-Regular.ttf")) {
                    fallback = new TTFParser().parse(in);
                } catch(IOException e) {
                    Log.e(FudeAndroid.TAG, "no fallback font: " + e);
                }
            }
            return fallback;
        }

        @Override
        public FontMapping<TrueTypeFont> getTrueTypeFont(String name, PDFontDescriptor d) {
            return new FontMapping<TrueTypeFont>(font(), true);
        }

        @Override
        public FontMapping<FontBoxFont> getFontBoxFont(String name, PDFontDescriptor d) {
            return new FontMapping<FontBoxFont>(font(), true);
        }

        @Override
        public CIDFontMapping getCIDFont(String name, PDFontDescriptor d, PDCIDSystemInfo info) {
            return new CIDFontMapping(null, font(), true);
        }
    }

    /** A page's quarter turns, from its /Rotate (0, 90, 180, 270). */
    static int quarters(int rotation) {
        return ((rotation % 360) + 360) % 360 / 90;
    }

    // --- a document's text ------------------------------------------------------------------

    /** FudePdf's document _h's text (LOCK held): made the first time, for its file. */
    static Text text(int h) {
        Text t = texts.get(h);
        if(t == null) {
            File file = FudePdf.file(h);
            if(file == null) {
                return null;
            }
            t = new Text(file);
            texts.put(h, t);
        }
        return t;
    }

    /** FudePdf's document _h let go: its text's too (a search going on stops). */
    static void close(int h) {
        Text t;
        synchronized(LOCK) {
            t = texts.remove(h);
        }
        if(t != null) {
            t.close();
        }
    }

    static boolean pageHasText(int h, int page) {
        if(!available()) {
            return false;
        }
        synchronized(LOCK) {
            try {
                Text t = text(h);
                return t != null && t.hasText(page);
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "could not look for page " + page + "'s text: " + e);
                return false;
            }
        }
    }

    static String textIn(int h, int page, float x, float y, float w, float hh) {
        if(!available()) {
            return "";
        }
        synchronized(LOCK) {
            try {
                Text t = text(h);
                return t != null ? t.textIn(page, x, y, w, hh) : "";
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "could not read page " + page + "'s text: " + e);
                return "";
            }
        }
    }

    static void findStart(int h, String query, int max) {
        if(!available()) {
            return;
        }
        Text t;
        synchronized(LOCK) {
            t = text(h);
        }
        if(t != null) {
            t.findStart(query, max);
        }
    }

    static void findStop(int h) {
        Text t;
        synchronized(LOCK) {
            t = texts.get(h);
        }
        if(t != null) {
            t.findStop();
        }
    }

    static float[] findMatches(int h, int known) {
        Text t;
        synchronized(LOCK) {
            t = texts.get(h);
        }
        return t != null ? t.matches(known) : new float[]{ known + 1, 1, 0 };
    }

    /**
     * A page's text as PDFTextStripper reads it out — its order, and the spaces and
     * line breaks it puts in — with each character's box.
     */
    static final class Page {
        final String  text;
        final float[] boxes;    // 4 a character: left, top, right, bottom (NaN: a space or a line break put in)
        final String  folded;   // the text as a search compares it (fold)...
        final int[]   at;       // ...and each of its characters' place in text

        Page(String text, float[] boxes) {
            this.text  = text;
            this.boxes = boxes;
            Folded f    = fold(text);
            this.folded = f.s.toString();
            this.at     = f.at;
        }

        /** The box round characters _from to _to (of text): left, top, right, bottom; null when none has one. */
        float[] box(int from, int to) {
            float l = Float.MAX_VALUE, t = Float.MAX_VALUE, r = -Float.MAX_VALUE, b = -Float.MAX_VALUE;
            boolean any = false;
            for(int i = from; i <= to && i < text.length(); i++) {
                if(Float.isNaN(boxes[4 * i])) {
                    continue;
                }
                any = true;
                l = Math.min(l, boxes[4 * i]);
                t = Math.min(t, boxes[4 * i + 1]);
                r = Math.max(r, boxes[4 * i + 2]);
                b = Math.max(b, boxes[4 * i + 3]);
            }
            return any ? new float[]{ l, t, r, b } : null;
        }
    }

    /** One document's: PDFBox's reading of its file, its pages' text (the last asked for), a search. */
    static final class Text {
        final File file;
        PDDocument doc;
        boolean    tried, closed;
        byte[]     hasText;   // each page's: 0 not asked, 1 it has, 2 none
        final LinkedHashMap<Integer, Page> pages = new LinkedHashMap<>(16, 0.75f, true);   // the last asked for last
        int        kept;      // their characters
        // The search (under found): which one goes on (each start and stop a new
        // one), its matches (page, x, y, w, h each), and a count of their changes for
        // the engine's thread to see.
        final Object found = new Object();
        int     search, count, version;
        float[] matches = new float[0];
        boolean done = true;

        Text(File file) {
            this.file = file;
        }

        /** PDFBox's document (LOCK held), opened the first time; null when it cannot be. */
        PDDocument document() {
            if(!tried && !closed) {
                tried = true;
                try {
                    doc     = PDDocument.load(file);
                    hasText = new byte[doc.getNumberOfPages()];
                } catch(Throwable e) {
                    Log.e(FudeAndroid.TAG, "PDFBox could not open " + file + ": " + e);
                    doc = null;
                }
            }
            return doc;
        }

        void close() {
            synchronized(found) {
                search++;
                done  = true;
                count = 0;
                version++;
            }
            synchronized(LOCK) {
                closed = true;
                pages.clear();
                kept = 0;
                if(doc != null) {
                    try {
                        doc.close();
                    } catch(IOException e) {
                        // gone all the same
                    }
                    doc = null;
                }
            }
        }

        /** Has page _p text of its own (LOCK held)? Asked of every page as a document opens: whether it shows any, not the text itself. */
        boolean hasText(int p) {
            PDDocument d = document();
            if(d == null || p < 0 || p >= hasText.length) {
                return false;
            }
            if(hasText[p] == 0) {
                boolean yes = false;
                try {
                    yes = showsText(d.getPage(p));
                } catch(Exception e) {
                    Log.e(FudeAndroid.TAG, "could not look into page " + p + ": " + e);
                }
                hasText[p] = (byte)(yes ? 1 : 2);
            }
            return hasText[p] == 1;
        }

        /** Page _p's text (LOCK held): read out the first time, kept while among the last asked for. */
        Page page(int p) {
            Page got = pages.get(p);
            if(got != null) {
                return got;
            }
            PDDocument d = document();
            if(d == null || p < 0 || p >= d.getNumberOfPages()) {
                return null;
            }
            try {
                Reader r = new Reader(d.getPage(p));
                r.setStartPage(p + 1);
                r.setEndPage(p + 1);
                r.writeText(d, NOWHERE);
                got = new Page(r.text.toString(), Arrays.copyOf(r.boxes, 4 * r.text.length()));
            } catch(Exception e) {
                Log.e(FudeAndroid.TAG, "could not read page " + p + "'s text: " + e);
                got = new Page("", new float[0]);
            }
            pages.put(p, got);
            kept += got.text.length();
            // The longest unasked let go, past what is kept (a search through a book reads it all).
            for(Iterator<Page> it = pages.values().iterator(); kept > CHARS_KEPT && pages.size() > 1;) {
                kept -= it.next().text.length();
                it.remove();
            }
            return got;
        }

        /**
         * The text in the rectangle at (_x, _y) of _w x _h on page _p (LOCK held): the
         * characters whose middle is in it, in the page's order, its lines as \n.
         */
        String textIn(int p, float x, float y, float w, float h) {
            Page page = page(p);
            if(page == null) {
                return "";
            }
            StringBuilder out = new StringBuilder();
            boolean space = false, line = false;   // put in since the last character kept
            for(int i = 0; i < page.text.length(); i++) {
                char c = page.text.charAt(i);
                float l = page.boxes[4 * i];
                if(Float.isNaN(l)) {
                    line  = line || c == '\n';
                    space = space || c != '\n';
                    continue;
                }
                float cx = (l + page.boxes[4 * i + 2]) * 0.5f, cy = (page.boxes[4 * i + 1] + page.boxes[4 * i + 3]) * 0.5f;
                if(cx < x || cx > x + w || cy < y || cy > y + h) {
                    continue;
                }
                if(out.length() > 0 && (line || space)) {
                    out.append(line ? '\n' : ' ');
                }
                line = space = false;
                out.append(c);
            }
            return trim(out.toString().replace("\r\n", "\n").replace('\r', '\n'));
        }

        void findStart(String query, final int max) {
            final String q = fold(query).s.toString();
            final int which;
            synchronized(found) {
                which = ++search;
                count = 0;
                done  = q.isEmpty();
                version++;
            }
            if(!q.isEmpty()) {
                FINDER.execute(new Runnable() {
                    @Override
                    public void run() {
                        find(which, q, max);
                    }
                });
            }
        }

        void findStop() {
            synchronized(found) {
                search++;
                done  = true;
                count = 0;
                version++;
            }
        }

        boolean current(int which) {
            synchronized(found) {
                return which == search;
            }
        }

        /** Search _which, for _q (folded), on its thread: through every page in order, each page's matches in as it is read. */
        void find(int which, String q, int max) {
            try {
                int pageCount;
                synchronized(LOCK) {
                    PDDocument d = document();
                    pageCount = d != null ? d.getNumberOfPages() : 0;
                }
                for(int p = 0; p < pageCount; p++) {
                    if(!current(which)) {
                        return;
                    }
                    Page page;
                    synchronized(LOCK) {
                        page = closed ? null : page(p);
                    }
                    if(page == null) {
                        continue;
                    }
                    // Each match after the last one (as PDFKit's), as the box round its characters.
                    for(int k = page.folded.indexOf(q); k >= 0; k = page.folded.indexOf(q, k + q.length())) {
                        float[] r = page.box(page.at[k], page.at[k + q.length() - 1]);
                        if(r == null) {
                            continue;
                        }
                        synchronized(found) {
                            if(which != search) {
                                return;
                            }
                            if(matches.length < 5 * (count + 1)) {
                                matches = Arrays.copyOf(matches, Math.max(5 * 64, 2 * matches.length));
                            }
                            matches[5 * count]     = p;
                            matches[5 * count + 1] = r[0];
                            matches[5 * count + 2] = r[1];
                            matches[5 * count + 3] = r[2] - r[0];
                            matches[5 * count + 4] = r[3] - r[1];
                            count++;
                            version++;
                            if(count >= max) {
                                done = true;
                                return;
                            }
                        }
                    }
                }
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "the search stopped: " + e);
            }
            synchronized(found) {
                if(which == search) {
                    done = true;
                    version++;
                }
            }
        }

        /** The matches so far — version, done (1 or 0), count, then page, x, y, w, h each — or null when none changed since _known. */
        float[] matches(int known) {
            synchronized(found) {
                if(version == known) {
                    return null;
                }
                float[] out = new float[3 + 5 * count];
                out[0] = version;
                out[1] = done ? 1 : 0;
                out[2] = count;
                System.arraycopy(matches, 0, out, 3, 5 * count);
                return out;
            }
        }
    }

    /** _s without the spaces (any: U+3000's too) and line breaks round it. */
    static String trim(String s) {
        int a = 0, b = s.length();
        while(a < b && (Character.isWhitespace(s.charAt(a)) || Character.isSpaceChar(s.charAt(a)))) {
            a++;
        }
        while(b > a && (Character.isWhitespace(s.charAt(b - 1)) || Character.isSpaceChar(s.charAt(b - 1)))) {
            b--;
        }
        return s.substring(a, b);
    }

    /** PDFTextStripper reading out one page: what it would write, kept with each character's box. */
    static final class Reader extends PDFTextStripper {
        final StringBuilder text  = new StringBuilder();
        float[]             boxes = new float[4096];
        final int           turn;   // the page's quarter turns (as the PDF says)
        final float         w, h;   // its crop box's size (the characters' places are from its bottom-left)
        final HashMap<PDFont, float[]> heights = new HashMap<>();   // each font's ascent and descent (ems)
        final float[]       box = new float[4];

        Reader(PDPage page) throws IOException {
            PDRectangle crop = page.getCropBox();
            turn = quarters(page.getRotation());
            w    = crop.getWidth();
            h    = crop.getHeight();
        }

        void put(char c, float l, float t, float r, float b) {
            int i = text.length();
            if(4 * i + 4 > boxes.length) {
                boxes = Arrays.copyOf(boxes, boxes.length * 2);
            }
            boxes[4 * i]     = l;
            boxes[4 * i + 1] = t;
            boxes[4 * i + 2] = r;
            boxes[4 * i + 3] = b;
            text.append(c);
        }

        @Override
        protected void writeString(String s, List<TextPosition> glyphs) {
            for(TextPosition g : glyphs) {
                place(g);
                String u = g.getUnicode();
                for(int k = 0; u != null && k < u.length(); k++) {
                    put(u.charAt(k), box[0], box[1], box[2], box[3]);
                }
            }
        }

        @Override
        protected void writeWordSeparator() {
            put(' ', Float.NaN, Float.NaN, Float.NaN, Float.NaN);
        }

        @Override
        protected void writeLineSeparator() {
            put('\n', Float.NaN, Float.NaN, Float.NaN, Float.NaN);
        }

        /** A font's ascent and descent, in ems (a descent down is positive); a usual one's when it says none that makes sense. */
        float[] height(PDFont font) {
            float[] ad = heights.get(font);
            if(ad == null) {
                float ascent = 0.88f, descent = 0.12f;
                PDFontDescriptor fd = font != null ? font.getFontDescriptor() : null;
                if(fd != null) {
                    float a = fd.getAscent() / 1000f, d = -fd.getDescent() / 1000f;
                    ascent  = a > 0.5f && a < 1.5f ? a : ascent;
                    descent = d >= 0f && d < 0.6f ? d : descent;
                }
                ad = new float[]{ ascent, descent };
                heights.put(font, ad);
            }
            return ad;
        }

        /**
         * Glyph _g's box (into box): round its advance and its font's height above
         * and below its baseline (across it, half an em either side, for vertical
         * text), the page as read.
         */
        void place(TextPosition g) {
            Matrix m = g.getTextMatrix();   // text space to the PDF's own, from the crop box's bottom-left
            float a = m.getValue(0, 0), b = m.getValue(0, 1), c = m.getValue(1, 0), d = m.getValue(1, 1);
            float e = m.getValue(2, 0), f = m.getValue(2, 1);
            float ax = g.getEndX() - e, ay = g.getEndY() - f;   // to the next glyph's place
            float ux, uy, lo, hi;
            if(g.getFont() != null && g.getFont().isVertical()) {
                ux = a;
                uy = b;
                lo = -0.5f;
                hi = 0.5f;
            } else {
                // The advance along the baseline only (a rise moves the glyph, not where the next goes).
                float n = a * a + b * b;
                if(n > 1e-12f) {
                    float s = (ax * a + ay * b) / n;
                    ax = s * a;
                    ay = s * b;
                }
                float[] ad = height(g.getFont());
                ux = c;
                uy = d;
                lo = -ad[1];
                hi = ad[0];
            }
            float l = Float.MAX_VALUE, t = Float.MAX_VALUE, r = -Float.MAX_VALUE, bt = -Float.MAX_VALUE;
            for(int k = 0; k < 4; k++) {
                float along = (k & 1) != 0 ? 1f : 0f, up = (k & 2) != 0 ? hi : lo;
                float x = e + along * ax + up * ux, y = f + along * ay + up * uy;
                float px, py;   // the page as read, from its top-left
                switch(turn) {
                    case 1:  px = y;     py = x;     break;
                    case 2:  px = w - x; py = y;     break;
                    case 3:  px = h - y; py = w - x; break;
                    default: px = x;     py = h - y; break;
                }
                l  = Math.min(l, px);
                t  = Math.min(t, py);
                r  = Math.max(r, px);
                bt = Math.max(bt, py);
            }
            box[0] = l;
            box[1] = t;
            box[2] = r;
            box[3] = bt;
        }
    }

    /**
     * Does _page show text (Tj, TJ, ' or ")? Its forms looked into too, a few deep.
     * Not the text itself: asked of every page as a document opens, and a scan's
     * page has none — whether its content shows any is enough, and quick.
     */
    static boolean showsText(PDPage page) throws IOException {
        return (page.hasContents() && showsText(new PDFStreamParser(page))) || formsShowText(page.getResources(), 0);
    }

    static boolean showsText(PDFStreamParser parser) throws IOException {
        for(Object t = parser.parseNextToken(); t != null; t = parser.parseNextToken()) {
            if(t instanceof Operator) {
                String n = ((Operator) t).getName();
                if("Tj".equals(n) || "TJ".equals(n) || "'".equals(n) || "\"".equals(n)) {
                    return true;
                }
            }
        }
        return false;
    }

    static boolean formsShowText(PDResources res, int depth) throws IOException {
        if(res == null || depth > 2) {
            return false;
        }
        for(COSName name : res.getXObjectNames()) {
            if(res.isImageXObject(name)) {
                continue;
            }
            PDXObject x = res.getXObject(name);
            if(x instanceof PDFormXObject) {
                PDFormXObject form = (PDFormXObject) x;
                if(showsText(new PDFStreamParser(form)) || formsShowText(form.getResources(), depth + 1)) {
                    return true;
                }
            }
        }
        return false;
    }

    // --- searching: the text as compared ---------------------------------------------------

    /** Text folded (fold), with each of its characters' place in what was folded. */
    static final class Folded {
        final StringBuilder s  = new StringBuilder();
        int[]               at = new int[64];

        void put(char c, int i) {
            if(s.length() == at.length) {
                at = Arrays.copyOf(at, at.length * 2);
            }
            at[s.length()] = i;
            s.append(c);
        }
    }

    static final HashMap<Integer, String> FOLDED = new HashMap<>();   // characters folded so far (fold: Normalizer is slow)

    /**
     * A character as a search compares it: case, accents and full- or half-width
     * alike, as Apple's (compatibility decomposition, its non-spacing marks out but
     * kana's voicing marks — が is not か there either — composed again, lower case);
     * nothing left of it but spaces: "".
     */
    static String foldOne(int cp) {
        if(cp < 0x80) {
            return String.valueOf((char)(cp >= 'A' && cp <= 'Z' ? cp + 32 : cp));
        }
        synchronized(FOLDED) {
            String got = FOLDED.get(cp);
            if(got != null) {
                return got;
            }
        }
        String d = Normalizer.normalize(new String(Character.toChars(cp)), Normalizer.Form.NFKD);
        StringBuilder b = new StringBuilder(d.length());
        for(int k = 0; k < d.length(); k++) {
            char c = d.charAt(k);
            final boolean mark = Character.getType(c) == Character.NON_SPACING_MARK && c != '\u3099' && c != '\u309A';
            if(!mark && !Character.isWhitespace(c) && !Character.isSpaceChar(c)) {
                b.append(c);
            }
        }
        String got = Normalizer.normalize(b, Normalizer.Form.NFC).toLowerCase(Locale.ROOT);
        synchronized(FOLDED) {
            if(FOLDED.size() < 20000) {
                FOLDED.put(cp, got);
            }
        }
        return got;
    }

    /** Written without spaces between words (Japanese, Chinese): a space there is a line broken, or a gap the stripper took for one. */
    static boolean joins(int cp) {
        if(cp >= 0x3000 && cp <= 0x30FF) {
            return true;   // CJK punctuation, kana
        }
        Character.UnicodeScript s = Character.UnicodeScript.of(cp);
        return s == Character.UnicodeScript.HAN || s == Character.UnicodeScript.HIRAGANA || s == Character.UnicodeScript.KATAKANA;
    }

    /**
     * Text as a search compares it: each character folded (foldOne), any run of
     * spaces and line breaks one space — none at either end, and none between two
     * characters of a writing without spaces (joins).
     */
    static Folded fold(CharSequence text) {
        Folded f = new Folded();
        boolean space = false;
        int     spaceAt = 0, last = 0;
        for(int i = 0; i < text.length();) {
            int cp = Character.codePointAt(text, i);
            int n  = Character.charCount(cp);
            if(Character.isWhitespace(cp) || Character.isSpaceChar(cp)) {
                if(!space) {
                    space   = true;
                    spaceAt = i;
                }
                i += n;
                continue;
            }
            String g = foldOne(cp);
            if(g.length() == 1 && (g.charAt(0) == '\u3099' || g.charAt(0) == '\u309A') && f.s.length() > 0 && !space) {
                // A half-width kana's voicing mark (ﾞ, ﾟ): one with the kana before it, as the full-width one.
                String both = Normalizer.normalize(f.s.charAt(f.s.length() - 1) + g, Normalizer.Form.NFC);
                if(both.length() == 1) {
                    f.s.setCharAt(f.s.length() - 1, both.charAt(0));
                    i += n;
                    continue;
                }
            }
            if(!g.isEmpty()) {
                int first = g.codePointAt(0);
                if(space && f.s.length() > 0 && !(joins(last) && joins(first))) {
                    f.put(' ', spaceAt);
                }
                space = false;
                for(int k = 0; k < g.length(); k++) {
                    f.put(g.charAt(k), i);
                }
                last = g.codePointBefore(g.length());
            }
            i += n;
        }
        return f;
    }

    // --- writing one ------------------------------------------------------------------------

    /**
     * A PDF being written: pages of others' (their content and resources shared —
     * read from them as it is written, so they stay open until then), and what goes
     * over each, in the page as read.
     */
    static final class Out {
        final File       file;
        final PDDocument doc = new PDDocument();
        PDPage           page;        // the page begun
        COSDictionary    own;         // ...its dictionary (a copy of its PDF's)
        PDResources      resources;   // ...its resources (a copy: what is added touches nothing of the PDF's)
        float[]          reading;     // ...the page as read (from its top-left, Y down) to its own space: a b c d e f
        StringBuilder    over;        // ...what goes over it, in the page as read
        HashMap<Integer, COSName> alphas;   // ...its see-through states, by alpha
        COSName          fontName;    // ...the unseen text's font, as its resources name it
        COSDictionary    font;        // that font (one for the whole document)
        COSStream        unicode;     // ...its codes' characters, written at the end
        final HashMap<Integer, Integer> far = new HashMap<>();   // characters past U+FFFF: the code each is written with
        boolean          ok = true;

        Out(File file) {
            this.file = file;
        }
    }

    static Out out(int w) {
        return w >= 0 && w < outs.size() ? outs.get(w) : null;
    }

    static int writeBegin(String path) {
        if(!available() || path.isEmpty()) {
            return -1;
        }
        synchronized(LOCK) {
            Out o = new Out(new File(path));
            for(int i = 0; i < outs.size(); i++) {
                if(outs.get(i) == null) {
                    outs.set(i, o);
                    return i;
                }
            }
            outs.add(o);
            return outs.size() - 1;
        }
    }

    /** The page as read with _q quarter turns (from its top-left, Y down) to the PDF's own space of a page cropped to _crop: a b c d e f. */
    static float[] reading(int q, PDRectangle crop) {
        float x = crop.getLowerLeftX(), y = crop.getLowerLeftY(), w = crop.getWidth(), h = crop.getHeight();
        switch(q & 3) {
            case 1:  return new float[]{ 0, 1, 1, 0, x, y };
            case 2:  return new float[]{ -1, 0, 0, 1, x + w, y };
            case 3:  return new float[]{ 0, -1, -1, 0, x + w, y + h };
            default: return new float[]{ 1, 0, 0, -1, x, y + h };
        }
    }

    /**
     * A page: page _page of FudePdf's document _h, turned _turn quarter turns more
     * than it says. Its dictionary copied (its content and the rest shared with the
     * PDF), turned by its /Rotate; without what points into the PDF it came from
     * (its annotations, beads, structure) — as Apple's, which draws the page alone.
     */
    static void writePage(int w, int h, int page, int turn) {
        synchronized(LOCK) {
            Out o = out(w);
            if(o == null) {
                return;
            }
            try {
                pageEnd(o);
                Text t = text(h);
                PDDocument src = t != null ? t.document() : null;
                if(src == null || page < 0 || page >= src.getNumberOfPages()) {
                    o.ok = false;
                    return;
                }
                PDPage        from = src.getPage(page);
                PDRectangle   crop = from.getCropBox();
                COSDictionary d    = new COSDictionary(from.getCOSObject());
                for(COSName k : new COSName[]{ COSName.PARENT, COSName.ANNOTS, COSName.B, COSName.STRUCT_PARENTS, COSName.AA }) {
                    d.removeItem(k);
                }
                PDPage p = new PDPage(d);
                p.setMediaBox(from.getMediaBox());   // what it had from its parents, its own
                p.setCropBox(crop);
                final int q = (quarters(from.getRotation()) + turn) & 3;
                p.setRotation(90 * q);
                PDResources   res = from.getResources();
                COSDictionary r   = res != null ? new COSDictionary(res.getCOSObject()) : new COSDictionary();
                for(COSName k : new COSName[]{ COSName.EXT_G_STATE, COSName.FONT }) {
                    COSBase sub = r.getDictionaryObject(k);
                    if(sub instanceof COSDictionary) {
                        r.setItem(k, new COSDictionary((COSDictionary) sub));
                    }
                }
                d.setItem(COSName.RESOURCES, r);
                o.doc.addPage(p);
                if(src.getVersion() > o.doc.getVersion()) {
                    o.doc.setVersion(src.getVersion());
                }
                o.page      = p;
                o.own       = d;
                o.resources = new PDResources(r);
                o.reading   = reading(q, crop);
                o.over      = new StringBuilder();
                o.alphas    = new HashMap<>();
                o.fontName  = null;
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "could not copy page " + page + ": " + e);
                o.ok   = false;
                o.page = null;
            }
        }
    }

    /** _v as a PDF number (points: to a thousandth), and a space. Not String.format: a device's language could write it with a comma. */
    static void num(StringBuilder s, float v) {
        long m = Float.isNaN(v) || Float.isInfinite(v) ? 0 : Math.round(v * 1000.0);
        if(m < 0) {
            s.append('-');
            m = -m;
        }
        s.append(m / 1000);
        long frac = m % 1000;
        if(frac != 0) {
            s.append('.');
            if(frac < 100) {
                s.append('0');
            }
            if(frac < 10) {
                s.append('0');
            }
            while(frac % 10 == 0) {
                frac /= 10;
            }
            s.append(frac);
        }
        s.append(' ');
    }

    /** Colour _color's (RGBA, a byte each from the top) red, green and blue, 0 to 1. */
    static void rgb(StringBuilder s, int color) {
        num(s, ((color >>> 24) & 255) / 255f);
        num(s, ((color >>> 16) & 255) / 255f);
        num(s, ((color >>> 8) & 255) / 255f);
    }

    /** The page's see-through state at alpha _a (of 255): added to its resources the first time. */
    static COSName alpha(Out o, int a) {
        COSName n = o.alphas.get(a);
        if(n == null) {
            PDExtendedGraphicsState gs = new PDExtendedGraphicsState();
            gs.setStrokingAlphaConstant(a / 255f);
            gs.setNonStrokingAlphaConstant(a / 255f);
            n = o.resources.add(gs);
            o.alphas.put(a, n);
        }
        return n;
    }

    /**
     * A stroke over the page: through points x, y with half-widths r (_p: x, y, r
     * each), in _color (RGBA). As Apple's (pdf.c): a dot for one point; _even (a
     * marker's: one width, see-through) one path stroked once, never darker where it
     * crosses itself; else a piece at a time, each as wide as its ends, round-capped.
     */
    static void writeStroke(int w, float[] p, int color, boolean even) {
        synchronized(LOCK) {
            Out o = out(w);
            int n = p != null ? p.length / 3 : 0;
            if(o == null || o.page == null || n == 0) {
                return;
            }
            StringBuilder s = o.over;
            int           a = color & 255;
            s.append("q ");
            rgb(s, color);
            s.append("RG ");
            rgb(s, color);
            s.append("rg ");
            if(a < 255) {
                s.append('/').append(alpha(o, a).getName()).append(" gs ");
            }
            s.append("1 J 1 j ");
            if(n == 1) {
                final float x = p[0], y = p[1], r = p[2], k = 0.5523f * r;   // a circle in four curves
                num(s, x + r); num(s, y); s.append("m ");
                num(s, x + r); num(s, y + k); num(s, x + k); num(s, y + r); num(s, x); num(s, y + r); s.append("c ");
                num(s, x - k); num(s, y + r); num(s, x - r); num(s, y + k); num(s, x - r); num(s, y); s.append("c ");
                num(s, x - r); num(s, y - k); num(s, x - k); num(s, y - r); num(s, x); num(s, y - r); s.append("c ");
                num(s, x + k); num(s, y - r); num(s, x + r); num(s, y - k); num(s, x + r); num(s, y); s.append("c f ");
            } else if(even) {
                num(s, 2f * p[2]);
                s.append("w ");
                num(s, p[0]);
                num(s, p[1]);
                s.append("m ");
                for(int i = 1; i < n; i++) {
                    num(s, p[3 * i]);
                    num(s, p[3 * i + 1]);
                    s.append("l ");
                }
                s.append("S ");
            } else {
                for(int i = 0; i + 1 < n; i++) {
                    num(s, p[3 * i + 2] + p[3 * i + 5]);
                    s.append("w ");
                    num(s, p[3 * i]);
                    num(s, p[3 * i + 1]);
                    s.append("m ");
                    num(s, p[3 * i + 3]);
                    num(s, p[3 * i + 4]);
                    s.append("l S ");
                }
            }
            s.append("Q\n");
        }
    }

    /**
     * The code character _cp is written with (Identity-H, two bytes): its own, or
     * for one past U+FFFF one from 0xD800 (a surrogate's: no character's), mapped
     * back in the font's character map. -1: none (a lone surrogate, or too many).
     */
    static int code(Out o, int cp) {
        if(cp < 0xD800 || (cp > 0xDFFF && cp <= 0xFFFF)) {
            return cp;
        }
        if(cp <= 0xFFFF) {
            return -1;
        }
        Integer c = o.far.get(cp);
        if(c == null) {
            if(o.far.size() >= 0x800) {
                return -1;
            }
            c = 0xD800 + o.far.size();
            o.far.put(cp, c);
        }
        return c;
    }

    static void hex4(StringBuilder s, int v) {
        final String digits = "0123456789ABCDEF";
        s.append(digits.charAt((v >> 12) & 15)).append(digits.charAt((v >> 8) & 15)).append(digits.charAt((v >> 4) & 15)).append(digits.charAt(v & 15));
    }

    /**
     * Text put in unseen (render mode 3), filling the box at (_x, _y) of _w x _h: as
     * Apple's, as tall as the box (85%), its baseline a descent above the bottom,
     * stretched across it. Every character as wide as an em, in a font of no glyphs
     * (glyphless): a reader finds and copies the text by the font's character map.
     */
    static void writeHiddenText(int w, String text, float x, float y, float sw, float sh) {
        synchronized(LOCK) {
            Out o = out(w);
            if(o == null || o.page == null || text == null || text.isEmpty() || !(sw > 0f) || !(sh > 0f)) {
                return;
            }
            try {
                StringBuilder hex = new StringBuilder();
                int n = 0;
                for(int i = 0; i < text.length();) {
                    int cp = text.codePointAt(i);
                    i += Character.charCount(cp);
                    int c = code(o, cp);
                    if(c >= 0) {
                        hex4(hex, c);
                        n++;
                    }
                }
                if(n == 0) {
                    return;
                }
                COSName       f    = font(o);
                float         size = sh * 0.85f;
                StringBuilder s    = o.over;
                s.append("q BT 3 Tr /").append(f.getName()).append(' ');
                num(s, size);
                s.append("Tf ");
                num(s, sw / (n * size));
                s.append("0 0 -1 ");   // the page as read has Y down: the glyphs up again
                num(s, x);
                num(s, y + sh - 0.12f * size);
                s.append("Tm <").append(hex).append("> Tj ET Q\n");
            } catch(IOException e) {
                Log.e(FudeAndroid.TAG, "could not put text in: " + e);
            }
        }
    }

    static COSStream stream(PDDocument doc, byte[] bytes) throws IOException {
        COSStream s = doc.getDocument().createCOSStream();
        try(OutputStream out = s.createOutputStream(COSName.FLATE_DECODE)) {
            out.write(bytes);
        }
        return s;
    }

    static final String HIDDEN = "FudeGlyphless";

    /** The unseen text's font in the page's resources: made the first time (a Type 0 font of glyphless's, Identity-H), named in each page's. */
    static COSName font(Out o) throws IOException {
        if(o.font == null) {
            final byte[] ttf  = glyphless();
            COSStream    file = stream(o.doc, ttf);
            file.setInt(COSName.LENGTH1, ttf.length);
            COSDictionary fd = new COSDictionary();
            fd.setItem(COSName.TYPE, COSName.FONT_DESC);
            fd.setName(COSName.FONT_NAME, HIDDEN);
            fd.setInt(COSName.FLAGS, 4);
            COSArray bbox = new COSArray();
            for(int v : new int[]{ 0, -120, 1000, 880 }) {
                bbox.add(COSInteger.get(v));
            }
            fd.setItem(COSName.FONT_BBOX, bbox);
            fd.setInt(COSName.ITALIC_ANGLE, 0);
            fd.setInt(COSName.ASCENT, 880);
            fd.setInt(COSName.DESCENT, -120);
            fd.setInt(COSName.CAP_HEIGHT, 700);
            fd.setInt(COSName.STEM_V, 80);
            fd.setItem(COSName.FONT_FILE2, file);
            COSDictionary info = new COSDictionary();
            info.setString(COSName.REGISTRY, "Adobe");
            info.setString(COSName.ORDERING, "Identity");
            info.setInt(COSName.SUPPLEMENT, 0);
            byte[] gids = new byte[2 * 65536];   // every code to glyph 1
            for(int i = 1; i < gids.length; i += 2) {
                gids[i] = 1;
            }
            COSDictionary cid = new COSDictionary();
            cid.setItem(COSName.TYPE, COSName.FONT);
            cid.setItem(COSName.SUBTYPE, COSName.CID_FONT_TYPE2);
            cid.setName(COSName.BASE_FONT, HIDDEN);
            cid.setItem(COSName.CIDSYSTEMINFO, info);
            cid.setItem(COSName.FONT_DESC, fd);
            cid.setInt(COSName.DW, 1000);
            cid.setItem(COSName.CID_TO_GID_MAP, stream(o.doc, gids));
            COSArray kids = new COSArray();
            kids.add(cid);
            o.unicode = o.doc.getDocument().createCOSStream();
            o.font    = new COSDictionary();
            o.font.setItem(COSName.TYPE, COSName.FONT);
            o.font.setItem(COSName.SUBTYPE, COSName.TYPE0);
            o.font.setName(COSName.BASE_FONT, HIDDEN);
            o.font.setItem(COSName.ENCODING, COSName.IDENTITY_H);
            o.font.setItem(COSName.DESCENDANT_FONTS, kids);
            o.font.setItem(COSName.TO_UNICODE, o.unicode);
        }
        if(o.fontName == null) {
            COSDictionary r     = o.resources.getCOSObject();
            COSBase       sub   = r.getDictionaryObject(COSName.FONT);
            COSDictionary fonts = sub instanceof COSDictionary ? (COSDictionary) sub : new COSDictionary();
            r.setItem(COSName.FONT, fonts);
            COSName name = COSName.getPDFName("FudeHidden");
            for(int k = 1; fonts.containsKey(name); k++) {
                name = COSName.getPDFName("FudeHidden" + k);
            }
            fonts.setItem(name, o.font);
            o.fontName = name;
        }
        return o.fontName;
    }

    /** The font's character map: each code its own character (but the surrogates'), and the codes given to characters past U+FFFF theirs. */
    static void writeUnicode(Out o) throws IOException {
        StringBuilder m = new StringBuilder();
        m.append("/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n");
        m.append("/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n");
        m.append("/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n");
        m.append("1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n");
        ArrayList<String> lines = new ArrayList<>();
        for(int hi = 0; hi < 256; hi++) {
            if(hi >= 0xD8 && hi <= 0xDF) {
                continue;
            }
            StringBuilder l = new StringBuilder("<");
            hex4(l, hi << 8);
            l.append("> <");
            hex4(l, (hi << 8) | 0xFF);
            l.append("> <");
            hex4(l, hi << 8);
            lines.add(l.append(">\n").toString());
        }
        block(m, lines, "bfrange");
        lines.clear();
        for(Map.Entry<Integer, Integer> e : o.far.entrySet()) {
            StringBuilder l = new StringBuilder("<");
            hex4(l, e.getValue());
            l.append("> <");
            for(char c : Character.toChars(e.getKey())) {
                hex4(l, c);
            }
            lines.add(l.append(">\n").toString());
        }
        block(m, lines, "bfchar");
        m.append("endcmap\nCMapName currentdict /CIDInit defineresource pop\nend\nend\n");
        try(OutputStream out = o.unicode.createOutputStream(COSName.FLATE_DECODE)) {
            out.write(m.toString().getBytes(StandardCharsets.US_ASCII));
        }
    }

    // _lines in blocks of at most 100 (as a character map allows).
    static void block(StringBuilder m, List<String> lines, String what) {
        for(int i = 0; i < lines.size(); i += 100) {
            int n = Math.min(100, lines.size() - i);
            m.append(n).append(" begin").append(what).append('\n');
            for(int k = i; k < i + n; k++) {
                m.append(lines.get(k));
            }
            m.append("end").append(what).append('\n');
        }
    }

    /**
     * The page begun ended: what goes over it after its own content — its content
     * wrapped in q/Q, so the PDF's own state is gone, and then turned to the page as
     * read.
     */
    static void pageEnd(Out o) throws IOException {
        if(o.page == null) {
            return;
        }
        try {
            if(o.over.length() > 0) {
                COSArray contents = new COSArray();
                COSBase  was      = o.own.getItem(COSName.CONTENTS);
                COSBase  got      = o.own.getDictionaryObject(COSName.CONTENTS);
                if(got != null) {
                    contents.add(stream(o.doc, "q\n".getBytes(StandardCharsets.US_ASCII)));
                    if(got instanceof COSArray) {
                        COSArray a = (COSArray) got;
                        for(int i = 0; i < a.size(); i++) {
                            contents.add(a.get(i));
                        }
                    } else {
                        contents.add(was);
                    }
                }
                StringBuilder s = new StringBuilder(o.over.length() + 64);
                s.append(got != null ? "Q\nq " : "q ");
                for(float v : o.reading) {
                    num(s, v);
                }
                s.append("cm\n").append(o.over).append("Q\n");
                contents.add(stream(o.doc, s.toString().getBytes(StandardCharsets.US_ASCII)));
                o.own.setItem(COSName.CONTENTS, contents);
            }
        } finally {
            o.page      = null;
            o.own       = null;
            o.resources = null;
            o.over      = null;
            o.alphas    = null;
            o.fontName  = null;
        }
    }

    static void writePageEnd(int w) {
        synchronized(LOCK) {
            Out o = out(w);
            if(o == null) {
                return;
            }
            try {
                pageEnd(o);
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "could not end the page: " + e);
                o.ok = false;
            }
        }
    }

    /** Written (and let go): false when it could not be (nothing is left at its path then). */
    static boolean writeEnd(int w) {
        synchronized(LOCK) {
            Out o = out(w);
            if(o == null) {
                return false;
            }
            outs.set(w, null);
            boolean ok = o.ok;
            try {
                pageEnd(o);
                if(o.unicode != null) {
                    writeUnicode(o);
                }
                ok = ok && o.doc.getNumberOfPages() > 0;
                if(ok) {
                    o.doc.save(o.file);
                }
            } catch(Throwable e) {
                Log.e(FudeAndroid.TAG, "could not write " + o.file + ": " + e);
                ok = false;
            } finally {
                try {
                    o.doc.close();
                } catch(IOException e) {
                    // let go all the same
                }
            }
            if(!ok) {
                o.file.delete();
            }
            return ok;
        }
    }

    // --- the unseen text's font -------------------------------------------------------------

    static byte[] glyphless;

    /**
     * A TrueType font of two glyphs, both empty and an em (1000 units) wide: the
     * unseen text's. Nothing of it is ever drawn, but a reader wants a font it can
     * load — and one of the device's would have to hold every character read.
     */
    static byte[] glyphless() {
        if(glyphless != null) {
            return glyphless;
        }
        ByteBuffer head = ByteBuffer.allocate(54);
        head.putInt(0x00010000).putInt(0x00010000).putInt(0).putInt(0x5F0F3CF5).putShort((short) 0x000B).putShort((short) 1000)
            .putLong(0).putLong(0).putShort((short) 0).putShort((short) -120).putShort((short) 1000).putShort((short) 880)
            .putShort((short) 0).putShort((short) 8).putShort((short) 2).putShort((short) 0).putShort((short) 0);
        ByteBuffer hhea = ByteBuffer.allocate(36);
        hhea.putInt(0x00010000).putShort((short) 880).putShort((short) -120).putShort((short) 0).putShort((short) 1000)
            .putShort((short) 0).putShort((short) 0).putShort((short) 1000).putShort((short) 1).putShort((short) 0).putShort((short) 0)
            .putLong(0).putShort((short) 0).putShort((short) 2);
        ByteBuffer maxp = ByteBuffer.allocate(32);
        maxp.putInt(0x00010000).putShort((short) 2).putShort((short) 0).putShort((short) 0).putShort((short) 0).putShort((short) 0)
            .putShort((short) 2);
        ByteBuffer hmtx = ByteBuffer.allocate(8);
        hmtx.putShort((short) 1000).putShort((short) 0).putShort((short) 1000).putShort((short) 0);
        ByteBuffer loca = ByteBuffer.allocate(6);   // both glyphs empty
        ByteBuffer glyf = ByteBuffer.allocate(4);
        ByteBuffer cmap = ByteBuffer.allocate(36);  // Windows Unicode: nothing but the end
        cmap.putShort((short) 0).putShort((short) 1).putShort((short) 3).putShort((short) 1).putInt(12)
            .putShort((short) 4).putShort((short) 24).putShort((short) 0).putShort((short) 2).putShort((short) 2).putShort((short) 0)
            .putShort((short) 0).putShort((short) 0xFFFF).putShort((short) 0).putShort((short) 0xFFFF).putShort((short) 1).putShort((short) 0);
        ByteBuffer post = ByteBuffer.allocate(32);
        post.putInt(0x00030000).putInt(0).putShort((short) -100).putShort((short) 50);
        final String[]     tags   = { "cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "post" };
        final ByteBuffer[] tables = { cmap, glyf, head, hhea, hmtx, loca, maxp, post };
        int size = 12 + 16 * tables.length;
        for(ByteBuffer t : tables) {
            size += (t.capacity() + 3) & ~3;
        }
        ByteBuffer font = ByteBuffer.allocate(size);
        font.putInt(0x00010000).putShort((short) tables.length).putShort((short) 128).putShort((short) 3).putShort((short) 0);
        int at = 12 + 16 * tables.length, headAt = 0;
        for(int i = 0; i < tables.length; i++) {
            byte[] b = tables[i].array();
            font.put(tags[i].getBytes(StandardCharsets.US_ASCII)).putInt(sum(b)).putInt(at).putInt(b.length);
            if(tables[i] == head) {
                headAt = at;
            }
            at += (b.length + 3) & ~3;
        }
        for(ByteBuffer t : tables) {
            font.put(t.array());
            font.position((font.position() + 3) & ~3);
        }
        font.putInt(headAt + 8, 0xB1B0AFBA - sum(font.array()));
        glyphless = font.array();
        return glyphless;
    }

    // A table's checksum: its 32-bit words (the last padded with zeros) added up.
    static int sum(byte[] b) {
        int s = 0;
        for(int i = 0; i < b.length; i += 4) {
            int v = 0;
            for(int k = 0; k < 4; k++) {
                v = (v << 8) | (i + k < b.length ? b[i + k] & 255 : 0);
            }
            s += v;
        }
        return s;
    }
}
