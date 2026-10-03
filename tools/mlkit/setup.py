#!/usr/bin/env python3
# ===========================================================================
# Google ML Kit for the study apps' iOS builds (Kana, Hanzi, Hangul) — without
# CocoaPods: Digital Ink Recognition (handwriting, recognize.h), Text Recognition
# with its Japanese, Chinese and Korean models (text in photos, scan.h: each app
# links and ships only its own language's) and Translation (translate.h).
#
# Downloads the pinned pods (what `pod 'GoogleMLKit/DigitalInkRecognition', '9.0.0'`,
# `pod 'GoogleMLKit/TextRecognitionJapanese', '9.0.0'` and
# `pod 'GoogleMLKit/Translate', '9.0.0'` resolve to), builds
# the source pods they need into one static library the way CocoaPods would (the
# subspecs used, <Pod/Header.h> headers, defines, ARC per pod), and lays
# everything out in third_party/mlkit/ (git-ignored):
#
#   frameworks/  MLKitDigitalInkRecognition, MLKitCommon, MLKitMDD,
#                MLKitTextRecognitionJapanese, MLKitTextRecognitionCommon,
#                MLKitVision, MLImage, MLKitTranslate, MLKitNaturalLanguage
#                (prebuilt, static)
#   lib/         libmlkit_deps.a — GTMSessionFetcher, GoogleDataTransport, nanopb,
#                PromisesObjC, GoogleToolboxForMac, GoogleUtilities, SSZipArchive
#   include/     the frameworks' headers as <MLKit.../X.h>, for the builder's -I
#   bundles/     copied into the .app beside the executable (--ios_bundle=bundles):
#                MLKitDigitalInkRecognition_resource.bundle,
#                MLKitTranslate_resource.bundle (the translation models are NOT in
#                it: ML Kit downloads one per language when first asked), and every
#                SDK's PRIVACY MANIFEST in a bundle of its own, named as CocoaPods
#                names them — static code has no framework to carry one, and
#                Apple's privacy report reads every manifest in the app
#   ocr/<code>/  a language's text model and its manifest (ja, zh, ko), copied in
#                beside the others by the app that reads that language
#                (--ios_bundle=ocr/ja): JapaneseOCRResources.bundle, in the app (no
#                download)
#
# and, into each study app's assets (they ship, and Settings > Licences shows them):
#   apps/<app>/assets/licenses/ml-kit-notices.txt   ML Kit's NOTICES (the software inside it)
#   apps/<app>/assets/licenses/libraries.txt        the source pods' licences
#
# Run from the project root:  python3 tools/mlkit/setup.py
# ===========================================================================
import glob, os, shutil, subprocess, tarfile, urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT  = os.path.join(ROOT, "third_party", "mlkit")
WORK = os.path.join(OUT, "work")
PODS = os.path.join(WORK, "pods")
SDK  = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"]).decode().strip()
MIN  = "15.5"   # ML Kit's own minimum
PINS = [
    ("GoogleMLKit", "https://dl.google.com/dl/cpdc/019adaeea9e4ebc0/GoogleMLKit-9.0.0.tar.gz"),
    ("MLKitDigitalInkRecognition", "https://dl.google.com/dl/cpdc/95473d50497208d4/MLKitDigitalInkRecognition-8.0.0.tar.gz"),
    ("MLKitCommon", "https://dl.google.com/dl/cpdc/00f258dabdb58dfa/MLKitCommon-14.0.0.tar.gz"),
    ("MLKitMDD", "https://dl.google.com/dl/cpdc/b14ff2c7cc91cb0f/MLKitMDD-10.0.0.tar.gz"),
    ("MLKitTextRecognitionJapanese", "https://dl.google.com/dl/cpdc/1855262723e8ed6b/MLKitTextRecognitionJapanese-6.0.0.tar.gz"),
    ("MLKitTextRecognitionChinese", "https://dl.google.com/dl/cpdc/88856ee0a4da8910/MLKitTextRecognitionChinese-6.0.0.tar.gz"),
    ("MLKitTextRecognitionKorean", "https://dl.google.com/dl/cpdc/7bfa31d60eef9311/MLKitTextRecognitionKorean-6.0.0.tar.gz"),
    ("MLKitTextRecognitionCommon", "https://dl.google.com/dl/cpdc/ffd1e8a2dd89e128/MLKitTextRecognitionCommon-6.0.0.tar.gz"),
    ("MLKitVision", "https://dl.google.com/dl/cpdc/4e1652530984149e/MLKitVision-10.0.0.tar.gz"),
    ("MLImage", "https://dl.google.com/dl/cpdc/438c904a2516b489/MLImage-1.0.0-beta8.tar.gz"),
    ("MLKitTranslate", "https://dl.google.com/dl/cpdc/2beeb631ff9efd40/MLKitTranslate-8.0.0.tar.gz"),
    ("MLKitNaturalLanguage", "https://dl.google.com/dl/cpdc/16c4bb76b6337f56/MLKitNaturalLanguage-10.0.0.tar.gz"),
    ("SSZipArchive", "https://github.com/ZipArchive/ZipArchive/archive/refs/tags/2.6.0.tar.gz"),
    ("GTMSessionFetcher", "https://github.com/google/gtm-session-fetcher/archive/refs/tags/v3.5.0.tar.gz"),
    ("GoogleDataTransport", "https://github.com/google/GoogleDataTransport/archive/refs/tags/CocoaPods-10.1.1.tar.gz"),
    ("GoogleToolboxForMac", "https://github.com/google/google-toolbox-for-mac/archive/refs/tags/v4.2.1.tar.gz"),
    ("GoogleUtilities", "https://github.com/google/GoogleUtilities/archive/refs/tags/CocoaPods-8.1.3.tar.gz"),
    ("nanopb", "https://github.com/nanopb/nanopb/archive/refs/tags/0.3.9.10.tar.gz"),
    ("PromisesObjC", "https://github.com/google/promises/archive/refs/tags/2.4.1.tar.gz"),
]

def fetch():
    os.makedirs(os.path.join(WORK, "dl"), exist_ok=True)
    for name, url in PINS:
        dest = os.path.join(PODS, name)
        if os.path.isdir(dest):
            continue
        tgz = os.path.join(WORK, "dl", name + ".tar.gz")
        if not os.path.isfile(tgz):
            print("fetching", name)
            req = urllib.request.Request(url, headers={"User-Agent": "CocoaPods/1.15.2 cocoapods-downloader/2.1"})
            open(tgz, "wb").write(urllib.request.urlopen(req).read())
        os.makedirs(dest)
        tarfile.open(tgz).extractall(dest)

fetch()
root = { n: os.path.join(PODS, n, os.listdir(os.path.join(PODS, n))[0]) for n in
         ["GTMSessionFetcher", "GoogleDataTransport", "nanopb", "PromisesObjC", "GoogleToolboxForMac", "GoogleUtilities", "SSZipArchive"] }
plan = {
  "GTMSessionFetcher":  (["Sources/Core/**/*.h", "Sources/Core/**/*.m"], True, [], ["GTMSessionFetcher"]),
  "GoogleDataTransport":(["GoogleDataTransport/GDTCORLibrary/**/*", "GoogleDataTransport/GDTCCTLibrary/**/*"], True,
                         ["PB_FIELD_32BIT=1", "PB_NO_PACKED_STRUCTS=1", "PB_ENABLE_MALLOC=1", "GDTCOR_VERSION=10.1.1"], ["GoogleDataTransport"]),
  "nanopb":             (["*.h", "*.c"], False, ["PB_FIELD_32BIT=1", "PB_NO_PACKED_STRUCTS=1", "PB_ENABLE_MALLOC=1"], ["nanopb"]),
  "PromisesObjC":       (["Sources/FBLPromises/**/*.h", "Sources/FBLPromises/**/*.m"], True, [], ["FBLPromises", "PromisesObjC"]),
  "GoogleToolboxForMac":(["GTMDefines.h", "Foundation/GTMLogger.h", "Foundation/GTMLogger.m", "Foundation/GTMNSData+zlib.h", "Foundation/GTMNSData+zlib.m",
                          "Foundation/GTMStringEncoding.h", "Foundation/GTMStringEncoding.m"],
                         ["Foundation/GTMNSData+zlib.m"], [], ["GoogleToolboxForMac"]),
  "GoogleUtilities":    (["GoogleUtilities/Environment/**/*.m", "GoogleUtilities/Environment/**/*.h", "third_party/IsAppEncrypted/**/*.m", "third_party/IsAppEncrypted/**/*.h",
                          "GoogleUtilities/Logger/**/*.m", "GoogleUtilities/Logger/**/*.h", "GoogleUtilities/UserDefaults/**/*.m", "GoogleUtilities/UserDefaults/**/*.h"], True, [], ["GoogleUtilities"]),
  "SSZipArchive":       (["SSZipArchive/*.m", "SSZipArchive/*.h", "SSZipArchive/include/*.m", "SSZipArchive/include/*.h", "SSZipArchive/minizip/*.c", "SSZipArchive/minizip/*.h"], True,
                         ["HAVE_ARC4RANDOM_BUF", "HAVE_INTTYPES_H", "HAVE_PKCRYPT", "HAVE_STDINT_H", "HAVE_WZAES", "HAVE_ZLIB", "ZLIB_COMPAT"], ["SSZipArchive"]),
}

BUILD = os.path.join(WORK, "build")
if os.path.isdir(BUILD): shutil.rmtree(BUILD)
hdr = os.path.join(BUILD, "headers"); objdir = os.path.join(BUILD, "obj"); os.makedirs(hdr); os.makedirs(objdir)
files = {}
for pod, (globs, arc, defs, mods) in plan.items():
    fs = []
    for g in globs:
        fs += [f for f in glob.glob(os.path.join(root[pod], g), recursive=True) if os.path.isfile(f)]
    files[pod] = sorted(set(fs))
    for m in mods:
        d = os.path.join(hdr, m); os.makedirs(d, exist_ok=True)
        for f in files[pod]:
            if f.endswith(".h"): shutil.copy(f, os.path.join(d, os.path.basename(f)))
incs = ["-I" + hdr] + ["-I" + os.path.join(hdr, m) for p in plan for m in plan[p][3]] + ["-I" + r for r in root.values()]
objs = []
for pod, (globs, arc, defs, mods) in plan.items():
    for f in files[pod]:
        if not f.endswith((".m", ".c")): continue
        use_arc = arc if isinstance(arc, bool) else any(f.endswith(a) for a in arc)
        o = os.path.join(objdir, pod + "_" + os.path.relpath(f, root[pod]).replace("/", "_") + ".o")
        cmd = ["xcrun", "clang", "-arch", "arm64", "-isysroot", SDK, "-miphoneos-version-min=" + MIN, "-O2", "-w", "-fmodules", "-c", f, "-o", o] + \
              (["-fobjc-arc"] if use_arc and f.endswith(".m") else []) + ["-D" + d for d in defs] + incs + ["-I" + os.path.dirname(f)]
        subprocess.check_call(cmd)
        objs.append(o)

# The layout the build uses.
for d in ("frameworks", "lib", "include"):
    shutil.rmtree(os.path.join(OUT, d), ignore_errors=True); os.makedirs(os.path.join(OUT, d))
subprocess.check_call(["xcrun", "libtool", "-static", "-o", os.path.join(OUT, "lib", "libmlkit_deps.a")] + objs)
FRAMEWORKS = ("MLKitDigitalInkRecognition", "MLKitCommon", "MLKitMDD", "MLKitTextRecognitionCommon", "MLKitVision", "MLImage",
              "MLKitTranslate", "MLKitNaturalLanguage")
# Each language's text model, and the app that reads it.
OCR = {"ja": ("MLKitTextRecognitionJapanese", "JapaneseOCRResources", "kana"),
       "zh": ("MLKitTextRecognitionChinese",  "ChineseOCRResources",  "hanzi"),
       "ko": ("MLKitTextRecognitionKorean",   "KoreanOCRResources",   "hangul")}
for fw in FRAMEWORKS + tuple(o[0] for o in OCR.values()):
    src = os.path.join(PODS, fw, "Frameworks", fw + ".framework")
    shutil.copytree(src, os.path.join(OUT, "frameworks", fw + ".framework"), symlinks=True)
    if os.path.isdir(os.path.join(src, "Headers")):
        shutil.copytree(os.path.join(src, "Headers"), os.path.join(OUT, "include", fw))
BUNDLES = os.path.join(OUT, "bundles")
shutil.rmtree(BUNDLES, ignore_errors=True); os.makedirs(BUNDLES)
shutil.rmtree(os.path.join(OUT, "MLKitDigitalInkRecognition_resource.bundle"), ignore_errors=True)   # where it was before
INFO = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key><string>org.cocoapods.%s</string>
    <key>CFBundleName</key><string>%s</string>
    <key>CFBundlePackageType</key><string>BNDL</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundleVersion</key><string>1</string>
</dict>
</plist>
"""
def bundle(name, files, into=BUNDLES):
    d = os.path.join(into, name + ".bundle"); os.makedirs(d)
    open(os.path.join(d, "Info.plist"), "w").write(INFO % (name.replace("_", "-"), name))
    for src, dst in files:
        if os.path.isdir(src): shutil.copytree(src, os.path.join(d, dst), dirs_exist_ok=True)
        else: shutil.copy(src, os.path.join(d, dst))
res = os.path.join(PODS, "MLKitDigitalInkRecognition", "Resources", "MLKitDigitalInkRecognition_resource")
bundle("MLKitDigitalInkRecognition_resource", [(os.path.join(res, f), f) for f in os.listdir(res)])
# Each language's text model, and its manifest, apart (ocr/<code>/).
shutil.rmtree(os.path.join(OUT, "ocr"), ignore_errors=True)
for code, (fw, resources, _) in OCR.items():
    into = os.path.join(OUT, "ocr", code); os.makedirs(into)
    res = os.path.join(PODS, fw, "Resources", resources)
    bundle(resources, [(os.path.join(res, f), f) for f in os.listdir(res)], into)
    manifest = os.path.join(PODS, fw, "Frameworks", fw + ".framework", "PrivacyInfo.xcprivacy")
    if os.path.isfile(manifest):
        bundle(fw + "_Privacy", [(manifest, "PrivacyInfo.xcprivacy")], into)
res = os.path.join(PODS, "MLKitTranslate", "Resources", "MLKitTranslate_resource")
bundle("MLKitTranslate_resource", [(os.path.join(res, f), f) for f in os.listdir(res)])
PRIVACY = [   # bundle name, pod, its manifest
    ("GTMSessionFetcher_Core_Privacy",     "GTMSessionFetcher",   "Sources/Core/Resources/PrivacyInfo.xcprivacy"),
    ("GoogleDataTransport_Privacy",        "GoogleDataTransport", "GoogleDataTransport/Resources/PrivacyInfo.xcprivacy"),
    ("nanopb_Privacy",                     "nanopb",              "spm_resources/PrivacyInfo.xcprivacy"),
    ("FBLPromises_Privacy",                "PromisesObjC",        "Sources/FBLPromises/Resources/PrivacyInfo.xcprivacy"),
    ("GoogleToolboxForMac_Privacy",        "GoogleToolboxForMac", "Resources/Base/PrivacyInfo.xcprivacy"),
    ("GoogleToolboxForMac_Logger_Privacy", "GoogleToolboxForMac", "Resources/Logger/PrivacyInfo.xcprivacy"),
    ("GoogleUtilities_Privacy",            "GoogleUtilities",     "GoogleUtilities/Privacy/Resources/PrivacyInfo.xcprivacy"),
    ("SSZipArchive",                       "SSZipArchive",        "SSZipArchive/Supporting Files/PrivacyInfo.xcprivacy"),
]
for name, pod, path in PRIVACY:
    bundle(name, [(os.path.join(root[pod], path), "PrivacyInfo.xcprivacy")])
for fw in FRAMEWORKS:
    manifest = os.path.join(PODS, fw, "Frameworks", fw + ".framework", "PrivacyInfo.xcprivacy")
    if os.path.isfile(manifest):
        bundle(fw + "_Privacy", [(manifest, "PrivacyInfo.xcprivacy")])

# The licences, into each study app's assets (an app not made yet is passed over).
for code, (ocr_fw, _, app) in OCR.items():
    app_dir = os.path.join(ROOT, "apps", app)
    if not os.path.isdir(app_dir):
        continue
    used = FRAMEWORKS[:3] + (ocr_fw,) + FRAMEWORKS[3:]   # the text model where Kana always listed it
    LIC = os.path.join(app_dir, "assets", "licenses"); os.makedirs(LIC, exist_ok=True)
    with open(os.path.join(LIC, "ml-kit-notices.txt"), "w") as out:
        out.write("GOOGLE ML KIT (" + ", ".join(used) + ")\n"
                  "Used under Google's terms: developers.google.com/ml-kit/terms\n\n"
                  # Google's disclaimer for its translations (Translate with Google),
                  # as its attribution requirements give it for apps:
                  # docs.cloud.google.com/translate/attribution
                  "TRANSLATIONS: THIS SERVICE MAY CONTAIN TRANSLATIONS POWERED BY GOOGLE. GOOGLE "
                  "DISCLAIMS ALL WARRANTIES RELATED TO THE TRANSLATIONS, EXPRESS OR IMPLIED, INCLUDING "
                  "ANY WARRANTIES OF ACCURACY, RELIABILITY, AND ANY IMPLIED WARRANTIES OF "
                  "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n\n"
                  "Notices for the software it contains, as Google ships them:\n\n")
        written = set()
        for fw in ("MLKitDigitalInkRecognition", ocr_fw, "MLKitTextRecognitionCommon", "MLKitVision", "MLImage",
                   "MLKitTranslate", "MLKitNaturalLanguage"):
            notices = os.path.join(PODS, fw, "NOTICES")
            if not os.path.isfile(notices):
                continue
            text = open(notices, encoding="utf-8", errors="replace").read()
            if text in written:
                continue   # the same notices again
            written.add(text)
            out.write("-" * 60 + "\n" + fw + "\n" + "-" * 60 + "\n\n" + text + "\n")
    LICENCES = [   # what, pod, its licence files
        ("GTMSessionFetcher (Google) - Apache License 2.0",   "GTMSessionFetcher",   ["LICENSE"]),
        ("GoogleDataTransport (Google) - Apache License 2.0", "GoogleDataTransport", ["LICENSE"]),
        ("GoogleUtilities (Google) - Apache License 2.0",     "GoogleUtilities",     ["LICENSE"]),
        ("GoogleToolboxForMac (Google) - Apache License 2.0", "GoogleToolboxForMac", ["LICENSE"]),
        ("Promises (Google) - Apache License 2.0",            "PromisesObjC",        ["LICENSE"]),
        ("nanopb - zlib License",                             "nanopb",              ["LICENSE.txt"]),
        ("SSZipArchive - MIT License",                        "SSZipArchive",        ["LICENSE.txt"]),
        ("minizip-ng (in SSZipArchive) - zlib License",       "SSZipArchive",        ["SSZipArchive/minizip/LICENSE"]),
    ]
    with open(os.path.join(LIC, "libraries.txt"), "w") as out:
        out.write("OPEN-SOURCE LIBRARIES that Google ML Kit uses, built into " + app.capitalize() + ":\n\n")
        for what, pod, files in LICENCES:
            out.write("=" * 60 + "\n" + what + "\n" + "=" * 60 + "\n\n")
            for f in files:
                out.write(open(os.path.join(root[pod], f), encoding="utf-8", errors="replace").read().strip() + "\n\n")
print("ML Kit ready in", OUT, "-", len(objs), "objects in libmlkit_deps.a,", len(os.listdir(BUNDLES)), "bundles, text models", ", ".join(OCR))
