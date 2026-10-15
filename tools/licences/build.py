#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# ===========================================================================
# The third-party licences every app must show its users: one document per app,
#
#   apps/<app>/assets/licenses/libraries.txt        Settings › Licences › Libraries
#
# (InfiNote's and InfiniWorkshop's go in their own assets/licenses/, laid over Sketching's when their builds stage
# the assets: tools/build/app.sh.) Each is assembled from
#
#   tools/licences/texts/<library>.txt   each library's licence and copyright lines, copied VERBATIM from its own
#                                        sources (only comment markers around a header licence are taken off; where
#                                        each one comes from is in LIBRARIES below), and
#   APPS below                           which libraries each app gets, on which platform,
#
# plus each app's Android libraries, read from apps/<app>/platform/android/deps.lock.
#
# Run from anywhere:
#   python3 tools/licences/build.py           writes the nine files
#   python3 tools/licences/build.py --check   writes nothing; fails if a file is not what it would write
#
# Run it again whenever what is built into an app changes:
#   - RDE's bundled code (~/RDE/external/, compiled by ~/RDE/rde_build.c): a library added, removed or updated means
#     its text in texts/ and its row in LIBRARIES; a module switched on (ImGui, RDE_IMGUI_MODULE, is off: not listed);
#   - an app's Android libraries (its deps.lock): the Apache-2.0 list is read from it, and a library of a group not in
#     ANDROID_GROUPS stops the build until it is classified;
#   - whisper.cpp's pin (tools/whisper/build.sh) or which apps build it (WHISPER below, as their project files,
#     apps/<app>/<app>.rdeproj, link it); ML Kit's iOS pods (tools/mlkit/setup.py, which calls this at its end);
#   - any text in texts/.
# A text that still says "TODO (owner)" is reported when this runs: its copyright line could not be found in RDE's
# copy of the library, and must be copied from that library's own release (never written from memory).
#
# What each app links, per platform, and how it was established (2026-10-13):
#   every platform  the RDE engine library with everything rde_build.c compiles into it: rde.c, external.c (the
#                   single-header libraries in external.h), SDL3, FreeType (with its own zlib and its libpng
#                   object), HarfBuzz, FriBidi, Box2D/Box3D, meshoptimizer, LuaJIT, etcpak/bc7enc/bcdec, the ASTC
#                   encoder; zlib's own copy on iOS and macOS only (Android uses the system's); ImGui only with -m
#                   imgui (never passed: none of its symbols in any build). Android ships it whole as libSDL3.so,
#                   macOS as libRDE.dylib; iOS links libRDE.a, which pulls in only the objects the app references:
#                   no etcpak, bc7enc, bcdec, ASTC encoder or libpng symbols in the iOS builds (nm), hence their
#                   section below.
#   Android         libc++_shared.so (the NDK's) in every APK; the Java libraries of deps.lock; whisper's libraries in
#                   Sketching's and InfiNote's ([android] libs of their project files; their APKs list libwhisper.so,
#                   libggml*.so).
#   iOS             ML Kit and libmlkit_deps.a (the pods below) in device builds of every app ([profile ios];
#                   -ObjC loads every Objective-C member of it); no whisper on iOS yet.
#   macOS           whisper.cpp linked statically into Sketching and InfiNote (their project files' [macos]); HIDAPI's
#                   macOS backend is compiled into SDL there only.
# ===========================================================================
import os, re, sys, textwrap

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TEXTS = os.path.join(ROOT, "tools", "licences", "texts")

# The apps: display name, whether it builds whisper.cpp (and so downloads the speech model), and a word on what in
# its own data is made from the Unicode Character Database (None: nothing).
APPS = {
    "kana":           ("Kana", False, None),
    "hanzi":          ("Hanzi", False, None),
    "hangul":         ("Hangul", False, None),
    "thai":           ("Thai", False, None),
    "hindi":          ("Hindi", False, "Hindi's letter data is made with it too."),
    "arabic":         ("Arabic", False, "Arabic's letter data (each letter's forms) is made with it too."),
    "sketching":      ("Sketching", True, None),
    "infinote":       ("InfiNote", True, None),
    "infiniworkshop": ("InfiniWorkshop", False, None),
}

# Where a library is built in, as the document's sections, in order.
SECTIONS = [
    ("all",     "IN EVERY VERSION OF {APP}",
                None),
    ("android+mac", "IN {APP} FOR ANDROID AND FOR MAC",
                None),
    ("android", "IN {APP} FOR ANDROID",
                None),
    ("mac",     "IN {APP} FOR MAC",
                None),
    ("ios",     "IN {APP} FOR IPHONE AND IPAD",
                "Open-source libraries that Google ML Kit uses, built into {APP} for iPhone and iPad:"),
]

# Texts printed once per document in full; later uses point back to the first.
SHARED = {"apache-2.0.txt": "Apache License 2.0", "mit.txt": "MIT License"}

# Every library: (where, title, [parts], who gets it). A part is a text file, or ("lead-in sentence", file); a text
# of its own may be None for a lead-in alone. "who": None for every app, else a predicate on the app's id.
# The comment above each row says where its text was copied from.
WHISPER = lambda app: APPS[app][1]
LIBRARIES = [
    # ~/RDE/external/include/SDL3/SDL_copying.h; the other copyright lines: every SDL file rde_build.c compiles for
    # Android, iOS or macOS whose header names someone besides Sam Lantinga (under SDL's own licence).
    ("all", "SDL 3 (Simple DirectMedia Layer) - zlib License",
     ["sdl3.txt", ("SDL's files contributed under the same licence also carry these copyright lines:",
                   "sdl3-contributors.txt")], None),
    # SDL3/src/libm/*.c (FreeBSD's msun, from Sun's fdlibm).
    ("all", "Math functions (in SDL, from fdlibm) - Sun Microsystems notice", ["sdl3-fdlibm.txt"], None),
    # SDL3/src/stdlib/SDL_qsort.c.
    ("all", "qsort (in SDL), by Gareth McCaughan - zlib-style licence", ["sdl3-qsort.txt"], None),
    # SDL3/src/video/SDL_rotate.c.
    ("all", "Surface rotation (in SDL, from SDL_gfx) - zlib License", ["sdl3-gfx-rotate.txt"], None),
    # SDL3/src/video/yuv2rgb/LICENSE.
    ("all", "yuv2rgb (in SDL) - BSD 3-Clause License", ["sdl3-yuv2rgb.txt"], None),
    # ~/RDE/external/include/freetype: src/base/ftinit.c's copyright, LICENSE.TXT; FTL.TXT is not in RDE's copy and
    # was taken from FreeType 2.13.2's, as Android Studio ships it (Contents/license/libandroid_runtime.dylib.txt);
    # FTL.TXT is dated 2006-Jan-27 and the same in every release since.
    ("all", "FreeType 2.13.3 - The FreeType License",
     [("This software is based in part on the work of the FreeType Team.", "freetype.txt"), "freetype-ftl.txt"],
     None),
    # freetype/src/bdf/README, src/pcf/README (both drivers are compiled: include/freetype/config/ftmodule.h).
    ("all", "BDF font driver (in FreeType) - MIT-style licence", ["freetype-bdf.txt"], None),
    ("all", "PCF font driver (in FreeType) - MIT-style licence", ["freetype-pcf.txt"], None),
    # ~/RDE/external/include/zlib/LICENSE (FreeType's own copy, src/gzip, is the same zlib under the same licence).
    ("all", "zlib (in RDE, and in FreeType) - zlib License", ["zlib.txt"], None),
    # ~/RDE/external/include/harfbuzz/src: every "Copyright ©" line of the files harfbuzz.cc builds and their headers
    # (the backends that are not compiled left out), and hb.h's licence ("Old MIT").
    ("all", "HarfBuzz 11.1.0 - Old MIT License",
     [("HarfBuzz's files name these copyright holders:", "harfbuzz.txt")], None),
    # harfbuzz/src/hb-ucd.cc, hb-algs.hh, ms-use/COPYING.
    ("all", "UCDN (in HarfBuzz) - ISC-style licence", ["harfbuzz-ucdn.txt"], None),
    ("all", "fasthash (in HarfBuzz) - MIT License", ["harfbuzz-fasthash.txt"], None),
    ("all", "Universal Shaping Engine data (in HarfBuzz) - MIT License", ["harfbuzz-ms-use.txt"], None),
    # ~/RDE/external/include/fribidi: lib/fribidi.c (its version string's copyright lines and its header),
    # lib/fribidi-config.h (the version), COPYING.
    ("all", "GNU FriBidi 1.0.16 - GNU Lesser General Public License 2.1",
     [("{APP} uses GNU FriBidi. FriBidi and its use are covered by the GNU Lesser General Public License, "
       "version 2.1, printed below.", "fribidi.txt"), "lgpl-2.1.txt"], None),
    # The Unicode License V3 as apps/hanzi/assets/data/LICENSE-data.txt quotes it. HarfBuzz's tables are generated
    # from the UCD (hb-ucd-table.hh, hb-ot-shaper-use-table.hh); FriBidi's .tab.i files too (its AUTHORS says so).
    ("all", "Unicode Character Database - Unicode License V3",
     [("Tables made from the Unicode Character Database are built into HarfBuzz and FriBidi.{UNICODE_DATA}",
       "unicode-v3.txt")], None),
    # ~/RDE/external/include/Box/Box2D, Box3D: their files' SPDX lines (they have no LICENSE file in RDE's copy);
    # the MIT License's words from whisper.cpp's LICENSE.
    ("all", "Box2D - MIT License", ["box2d.txt", "mit.txt"], None),
    ("all", "Box3D - MIT License", ["box3d.txt", "mit.txt"], None),
    # Box/Box3D/src/qsort.h, verstable.h.
    ("all", "qsort.h (in Box3D), by Alexey Tourbin - MIT License", ["box3d-qsort.txt"], None),
    ("all", "Verstable (in Box3D) - MIT License", ["box3d-verstable.txt"], None),
    # ~/RDE/external/include/meshoptimizer/meshoptimizer.h (its end).
    ("all", "meshoptimizer 1.1 - MIT License", ["meshoptimizer.txt"], None),
    # ~/RDE/external/include/luajit/COPYRIGHT.
    ("all", "LuaJIT (with code from Lua 5.1/5.2 and dlmalloc) - MIT License", ["luajit.txt"], None),
    # ~/RDE/engine/src/rde_plugin.c embeds it (its SPDX lines).
    ("all", "debugger.lua (in RDE) - MIT License", ["debugger-lua.txt", "mit.txt"], None),
    # ~/RDE/external/include/external.h: each library's section.
    ("all", "fast_obj 1.3 - MIT License", ["fast-obj.txt"], None),
    ("all", "cgltf 1.15 - MIT License", ["cgltf.txt"], None),
    ("all", "jsmn (in cgltf) - MIT License", ["jsmn.txt"], None),
    ("all", "ufbx 0.21.3", ["ufbx.txt"], None),
    ("all", "minicoro 0.1.3 - Public Domain (Unlicense) or MIT No Attribution", ["minicoro.txt"], None),
    ("all", "LuaCoco assembly (in minicoro), by Mike Pall - MIT License", ["luacoco.txt"], None),
    ("all", "stb_image, stb_image_write, stb_vorbis, stb_dxt - MIT License or Public Domain", ["stb.txt"], None),
    ("all", "miniaudio 0.11.18 - Public Domain (Unlicense) or MIT No Attribution", ["miniaudio.txt"], None),
    # ~/RDE/external/include/nanosvg/LICENSE.txt.
    ("all", "NanoSVG - zlib License", ["nanosvg.txt"], None),

    # ~/RDE/external/include/freetype/libpng/LICENSE.
    ("android+mac", "libpng (in RDE) - PNG Reference Library License", ["libpng.txt"], None),
    # ~/RDE/external/include/etcpak: no licence in RDE's copy (texts/etcpak.txt says what to do).
    ("android+mac", "etcpak", ["etcpak.txt"], None),
    # etcpak/bc7enc.cpp (its end), etcpak/bcdec.h (its end).
    ("android+mac", "bc7enc - MIT License or Public Domain", ["bc7enc.txt"], None),
    ("android+mac", "bcdec - MIT License or The Unlicense", ["bcdec.txt"], None),
    # ~/RDE/external/include/astc: every copyright line of its sources (the RISC-V header left out) and astcenc.h's
    # notice; the licence's text from GTMSessionFetcher's LICENSE (the Apache License 2.0, unchanged).
    ("android+mac", "Arm ASTC Encoder - Apache License 2.0", ["astc-encoder.txt", "apache-2.0.txt"], None),
    # astc/ThirdParty/wuffs-v0.3.c, tinyexr.h (which carries miniz 1.15, public domain).
    ("android+mac", "Wuffs (in the ASTC Encoder) - Apache License 2.0", ["wuffs.txt", "apache-2.0.txt"], None),
    ("android+mac", "TinyEXR (in the ASTC Encoder, with miniz, public domain) - BSD 3-Clause License",
     ["tinyexr.txt"], None),
    # build/whisper/src (tools/whisper/build.sh's pinned clone, v1.9.5): LICENSE; ggml/src/ggml-cpu/ops.cpp.
    ("android+mac", "whisper.cpp and ggml - MIT License", ["whisper-cpp.txt"], WHISPER),
    ("android+mac", "YaRN rotary embedding (in ggml) - MIT License", ["ggml-yarn.txt", "mit.txt"], WHISPER),
    # OpenAI's licence for Whisper, as whisper.cpp quotes it (tests/librispeech/normalizers/LICENSE). The model:
    # fude/zoom/listen.h, FUDE_LISTEN_MODEL_URL.
    ("android+mac", "Whisper speech model (OpenAI) - MIT License",
     [("{APP} downloads a speech model the first time it reads a recording: ggml-base-q5_1.bin, OpenAI's Whisper "
       "base model converted for whisper.cpp (https://huggingface.co/ggerganov/whisper.cpp). OpenAI's licence for "
       "Whisper:", "whisper-model.txt")], WHISPER),

    # SDL3/src/hidapi/LICENSE-bsd.txt (HIDAPI offers three licences, LICENSE.txt: the BSD-style one is used).
    ("mac", "HIDAPI (in SDL) - BSD-style licence", ["hidapi-bsd.txt"], None),

    # The NDK's NOTICE.toolchain (NDK 29.0.14206865, the newest installed: the one the builder picks): libc++'s
    # LICENSE.TXT.
    ("android", "LLVM libc++ (libc++_shared.so) - Apache License 2.0 with LLVM Exceptions", ["libcxx.txt"], None),
    ("android", "ANDROID_APACHE", [], None),      # written from deps.lock (android_libraries below)
    # PdfBox-Android's own notices (every app's Android build has it: FudePdfBox.java).
    ("android", "PdfBox-Android (Apache PDFBox for Android) - Apache License 2.0", ["pdfbox-android.txt"], None),
    ("android", "Liberation Sans (in PdfBox-Android) - SIL Open Font License 1.1", ["liberation-sans.txt"], None),
    ("android", "Unicode Character Database files (in PdfBox-Android: Scripts.txt,\nBidiMirroring.txt) - "
                "Unicode License", ["unicode-pdfbox.txt"], None),

    # The pods tools/mlkit/setup.py fetches (third_party/mlkit/work/pods/<pod>/<release>/), their licence files.
    ("ios", "GTMSessionFetcher (Google) - Apache License 2.0", ["gtm-session-fetcher.txt"], None),
    ("ios", "GoogleDataTransport (Google) - Apache License 2.0", ["google-data-transport.txt"], None),
    ("ios", "GoogleUtilities (Google) - Apache License 2.0", ["google-utilities.txt"], None),
    ("ios", "GoogleToolboxForMac (Google) - Apache License 2.0", ["google-toolbox-for-mac.txt"], None),
    ("ios", "Promises (Google) - Apache License 2.0", ["promises.txt"], None),
    ("ios", "nanopb - zlib License", ["nanopb.txt"], None),
    ("ios", "SSZipArchive - MIT License", ["ssziparchive.txt"], None),
    ("ios", "minizip-ng (in SSZipArchive) - zlib License", ["minizip-ng.txt"], None),
]

# The Android libraries (deps.lock's --android_dep lines), by group: open source under the Apache License 2.0 (as each
# one's Maven POM names it, checked in build/android_deps), Google's own (ML Kit and Play services: Google's terms,
# their notices in ML Kit's), or one with an entry of its own above.
ANDROID_GROUPS = [
    ("androidx.", "apache"), ("org.jetbrains.kotlin:", "apache"), ("org.jetbrains.kotlinx:", "apache"),
    ("org.jetbrains:annotations", "apache"), ("com.squareup.okhttp3:", "apache"), ("com.squareup.okio:", "apache"),
    ("javax.inject:", "apache"), ("com.google.guava:", "apache"), ("com.google.firebase:", "apache"),
    ("com.google.android.datatransport:", "apache"),
    ("com.google.mlkit:", "google"), ("com.google.android.gms:", "google"), ("com.google.android.odml:", "google"),
    ("com.tom-roush:pdfbox-android:", "own"),
]

BAR = "=" * 60
SECTION_BAR = "#" * 60


def text(name):
    path = os.path.join(TEXTS, name)
    with open(path, encoding="utf-8") as f:
        return f.read().rstrip("\n")


def android_libraries(app):
    # The app's Android libraries under the Apache License 2.0, and Google's own, from its deps.lock.
    lock = os.path.join(ROOT, "apps", app, "platform", "android", "deps.lock")
    apache, google = [], []
    with open(lock, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"\s*--android_dep=(\S+)", line)
            if not m:
                continue
            dep = m.group(1)
            kind = next((k for prefix, k in ANDROID_GROUPS if dep.startswith(prefix)), None)
            if kind is None:
                sys.exit("%s: %s is in no group of ANDROID_GROUPS: find its licence (its POM) and add it" % (lock, dep))
            if kind == "apache":
                apache.append(dep)
            elif kind == "google":
                google.append(dep)
    return sorted(apache), sorted(google)


def document(app):
    name, _, unicode_data = APPS[app]
    out = [
        "OPEN-SOURCE SOFTWARE IN " + name.upper(),
        "",
        name + " is built with the open-source software below, each listed with its",
        "copyright notices and its licence. Most of it is in every version of " + name + ";",
        "the sections after the first list what only its Android, Mac or iPhone and",
        "iPad versions have.",
        "",
    ]
    printed = {}

    def entry(title, parts):
        out.extend(["", BAR, title, BAR, ""])
        for part in parts:
            lead, f = part if isinstance(part, tuple) else (None, part)
            if lead:
                lead = lead.replace("{APP}", name)
                lead = lead.replace("{UNICODE_DATA}", (" " + unicode_data) if unicode_data else "")
                for para in lead.split("\n\n"):   # wrapped as the texts are, but for lists (indented lines)
                    out.append(para if para.startswith(" ") else
                               textwrap.fill(para, 80, break_on_hyphens=False, break_long_words=False))
                    out.append("")
            if f is None:
                continue
            if f in SHARED and f in printed:
                out.append("[The %s's text is printed above, under %s.]" % (SHARED[f], printed[f]))
            else:
                out.append(text(f))
                if f in SHARED:
                    printed[f] = title.split(" - ")[0].replace("\n", " ")
            out.append("")

    for where, heading, lead in SECTIONS:
        rows = [r for r in LIBRARIES if r[0] == where and (r[3] is None or r[3](app))]
        if not rows:
            continue
        out.extend(["", SECTION_BAR, heading.replace("{APP}", name.upper()), SECTION_BAR, ""])
        if lead:
            out.append(textwrap.fill(lead.replace("{APP}", name), 80, break_on_hyphens=False))
        for _, title, parts, _ in rows:
            if title == "ANDROID_APACHE":
                apache, google = android_libraries(app)
                listing = "\n".join("  " + d for d in apache)
                entry("AndroidX, Kotlin and other Android libraries - Apache License 2.0",
                      [("These libraries are built into " + name + " for Android, each under the Apache "
                        "License 2.0:\n\n" + listing, None), "apache-2.0.txt"])
                if google:
                    entry("Google ML Kit and Google Play services (Android)",
                          [("Google's own libraries, used under Google's terms (ML Kit Terms of Service, "
                            "https://developers.google.com/ml-kit/terms, and the Android Software Development Kit "
                            "License):\n\n" + "\n".join("  " + d for d in google), None)])
                continue
            entry(title, parts)
    return "\n".join(out).rstrip("\n") + "\n"


def main():
    check = "--check" in sys.argv[1:]
    stale, todo = [], set()
    for app in APPS:
        doc = document(app)
        for f in os.listdir(TEXTS):
            if "TODO (owner)" in text(f) and text(f) in doc:
                todo.add(f)
        path = os.path.join(ROOT, "apps", app, "assets", "licenses", "libraries.txt")
        old = open(path, encoding="utf-8").read() if os.path.isfile(path) else None
        if old == doc:
            continue
        if check:
            stale.append(os.path.relpath(path, ROOT))
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(doc)
        print("wrote", os.path.relpath(path, ROOT), "(%d lines)" % doc.count("\n"))
    for f in sorted(todo):
        print("TODO: texts/%s still asks the owner for its licence (see the file)" % f)
    if stale:
        print("out of date (run python3 tools/licences/build.py):\n  " + "\n  ".join(stale))
        sys.exit(1)


if __name__ == "__main__":
    main()
