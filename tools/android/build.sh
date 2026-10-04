#!/bin/zsh
# The study apps for Android, built from a Mac: an APK each, for a tablet.
#
#   zsh tools/android/build.sh engine [--release]           # RDE for Android: FIRST, after any engine change,
#                                                           # and after an engine build for another platform
#   zsh tools/android/build.sh kana|hanzi|hangul|thai|hindi|arabic [--release] [builder flags...]
#   zsh tools/android/build.sh sketching [--release] [builder flags...]   # the deep-zoom drawing app (no study layer)
#
# The APK: build/<app>-android/<App>.apk, <App>-release.apk with --release (signed with the builder's keystore,
# build/<app>-android/com.rde.<app>.keystore — KEEP A COPY of it outside build/:
# an APK signed with another key cannot update an installed one, nor Play's).
# With the key kept elsewhere, pass --android_sign_path=<keystore>. Install with
#   adb install -r build/kana-android/Kana.apk
# COMMANDS.txt, ANDROID (from the Mac), has the rest (the emulator, the libraries).
#
# Google's ML Kit (handwriting, translation, text in photos, the document scanner)
# comes from Maven, as apps/<app>/platform/android/deps.lock pins it: downloaded
# once into build/android_deps. To change a version, resolve the lock again:
#   ~/RDE/builder --android_dep_cache=build/android_deps \
#     --android_dep_resolve=com.google.mlkit:digital-ink-recognition:19.0.0 \
#     --android_dep_resolve=com.google.mlkit:translate:17.0.3 \
#     --android_dep_resolve=com.google.mlkit:text-recognition-japanese:16.0.1 \
#     --android_dep_resolve=com.google.android.gms:play-services-mlkit-document-scanner:16.0.0 \
#     --android_deps_out=apps/kana/platform/android/deps.lock
# (every root in one command: chinese for hanzi, korean for hangul, devanagari for hindi;
# thai and arabic: text-recognition, the Latin one, only for FudeText.java to build — no
# Thai or Arabic model.)
# --android_deps_out writes the lock anew: add back by hand the PDFBox lines at its
# end (com.tom-roush:pdfbox-android, without the BouncyCastle it would resolve to).
#
# Needs Android Studio (its JDK and the SDK at ~/Library/Android/sdk: build-tools
# and platform 37, an NDK). The NDK is the newest installed unless ANDROID_NDK
# says; the SDK is ANDROID_SDK or ~/Library/Android/sdk. On Windows: COMMANDS.txt,
# ANDROID — the real test.
set -e
K=$(cd "$(dirname "$0")/../.." && pwd)
SDK=${ANDROID_SDK:-$HOME/Library/Android/sdk}
NDK=${ANDROID_NDK:-$(ls -d $SDK/ndk/*/ | sort -V | tail -1)}
API=37
# The JDK: Android Studio's, through a path with no space in it (the builder runs
# keytool through the shell, unquoted).
mkdir -p $K/build
ln -sfn "/Applications/Android Studio.app/Contents/jbr/Contents/Home" $K/build/jdk
export JAVA_HOME=$K/build/jdk
export PATH="$JAVA_HOME/bin:$PATH"

WHAT=$1; shift || true
MODE=--debug
if [ "$1" = "--release" ]; then MODE=--release; shift; fi
ANDROID=(--android --abi_armv8a --android_ndk=$NDK --android_sdk=$SDK/ --android_api_level=$API)

if [ "$WHAT" = "engine" ]; then
    cd ~/RDE && ./builder --engine $MODE $ANDROID
    exit $?
fi

# Sketching: the drawing core and the deep-zoom canvas (fude/zoom), no study layer.
# Its Java is the core's (fude/android/java), whose ML Kit classes build against
# the same pinned libraries as Kana's (apps/sketching/platform/android/deps.lock).
if [ "$WHAT" = "sketching" ]; then
    OUT=$K/build/sketching-android
    cd $K
    ~/RDE/builder --project $MODE $ANDROID \
        --android_app_name=Sketching --android_package_name=com.rde.sketching \
        --android_icon=$K/apps/sketching/platform/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png \
        --assets_path=$K/apps/sketching/assets/ \
        --android_java=$K/fude/android/java \
        --android_deps_file=$K/apps/sketching/platform/android/deps.lock --android_dep_cache=$K/build/android_deps \
        --android_internet --android_network_state --android_wake_lock --android_camera \
        "$@" \
        apps/sketching/sketching.c $(ls fude/drawing/*/*.c | grep -v '_android.c$' | grep -v '/android.c$') $(ls fude/zoom/*.c) \
        fude/drawing/base/android.c fude/drawing/doc/pdf_android.c fude/drawing/doc/import_android.c \
        -I$K/fude -I$K/apps/sketching/src -Wall -Wextra \
        --output_path=$OUT/
    APK=$OUT/Sketching.apk
    if [ "$MODE" = "--release" ]; then APK=$OUT/Sketching-release.apk; fi
    cp $OUT/game.apk $APK
    echo "$APK"
    exit 0
fi

# The core's and the study layer's sources (every study app's).
COMMON=(
    fude/drawing/app/app.c fude/drawing/app/look.c fude/drawing/app/page.c fude/drawing/app/session.c \
    fude/drawing/app/ui.c fude/drawing/base/backup.c fude/drawing/base/kfile.c fude/drawing/base/save.c \
    fude/drawing/base/text.c fude/drawing/base/theme.c fude/drawing/base/utf8.c fude/drawing/ink/canvas.c \
    fude/drawing/ink/ink.c fude/drawing/ink/lasso.c fude/drawing/ink/notes.c fude/drawing/doc/doc.c \
    fude/drawing/doc/pdf.c fude/drawing/doc/import.c fude/drawing/doc/library.c fude/drawing/widgets/draw.c \
    fude/drawing/widgets/docbar.c fude/drawing/widgets/filterbar.c fude/drawing/widgets/kit.c \
    fude/drawing/widgets/notice.c fude/drawing/widgets/pagemenu.c fude/drawing/widgets/row.c \
    fude/drawing/widgets/scroll.c fude/drawing/widgets/side.c fude/drawing/widgets/toolbar.c \
    fude/study/app/study.c fude/study/app/files.c fude/study/app/look.c fude/study/app/demo.c \
    fude/study/app/verbs.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/study/chars/kanji.c \
    fude/study/chars/bake.c fude/study/handwriting/guide.c fude/study/handwriting/match.c \
    fude/study/handwriting/recognize.c fude/study/handwriting/score.c fude/study/handwriting/segment.c \
    fude/study/handwriting/textink.c fude/study/models/charnote.c fude/study/models/examlog.c \
    fude/study/models/history.c fude/study/models/marks.c fude/study/models/review.c \
    fude/study/models/select.c fude/study/models/sheet.c fude/study/models/vocab.c fude/study/screens/album.c \
    fude/study/screens/browse.c fude/study/screens/check.c fude/study/screens/exam.c \
    fude/study/screens/practice.c fude/study/screens/scan.c fude/study/screens/stats.c \
    fude/study/screens/viewer.c fude/study/screens/vocabview.c fude/study/screens/translator.c \
    fude/study/screens/wordexam.c fude/study/widgets/header.c fude/study/widgets/pagetext.c \
    fude/study/widgets/wordcard.c fude/study/services/mlkit.c fude/study/services/speech.c \
    fude/study/services/textscan.c fude/study/services/translate.c fude/study/app/welcome.c fude/drawing/widgets/readcard.c
)
# Android's sides of the services (JNI into fude/android/java).
ANDROID_SRC=(
    fude/drawing/base/android.c fude/drawing/doc/pdf_android.c fude/drawing/doc/import_android.c \
    fude/study/services/speech_android.c fude/study/services/mlkit_android.c \
    fude/study/services/translate_android.c fude/study/services/textscan_android.c
)
case $WHAT in
    kana)   NAME="Kana";   LANG_SRC=(fude/lang/ja/bake.c fude/lang/ja/chart.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/lang/ja/wordsplit.c) ;;
    hanzi)  NAME="Hanzi";  LANG_SRC=(fude/lang/zh/bake.c fude/lang/zh/lang.c fude/lang/zh/wordsplit.c) ;;
    hangul) NAME="Hangul"; LANG_SRC=(fude/lang/ko/bake.c fude/lang/ko/chart.c fude/lang/ko/lang.c fude/lang/ko/wordsplit.c) ;;
    thai)   NAME="Thai";   LANG_SRC=(fude/lang/th/bake.c fude/lang/th/chart.c fude/lang/th/lang.c fude/lang/th/wordsplit.c) ;;
    hindi)  NAME="Hindi";  LANG_SRC=(fude/lang/hi/bake.c fude/lang/hi/chart.c fude/lang/hi/lang.c fude/lang/hi/wordsplit.c) ;;
    arabic) NAME="Arabic"; LANG_SRC=(fude/lang/ar/bake.c fude/lang/ar/chart.c fude/lang/ar/lang.c fude/lang/ar/wordsplit.c) ;;
    *) echo "usage: zsh tools/android/build.sh engine|kana|hanzi|hangul|thai|hindi|arabic [--release] [builder flags...]" >&2; exit 1 ;;
esac
OUT=$K/build/$WHAT-android
cd $K
~/RDE/builder --project $MODE $ANDROID \
    "--android_app_name=$NAME Learn!" --android_package_name=com.rde.$WHAT \
    --android_icon=$K/apps/$WHAT/platform/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png \
    --assets_path=$K/apps/$WHAT/assets/ \
    --android_java=$K/fude/android/java \
    --android_deps_file=$K/apps/$WHAT/platform/android/deps.lock --android_dep_cache=$K/build/android_deps \
    --android_internet --android_network_state --android_wake_lock --android_camera \
    --android_query_intent=android.intent.action.TTS_SERVICE \
    "$@" \
    apps/$WHAT/$WHAT.c apps/$WHAT/src/${WHAT}_app.c $COMMON $LANG_SRC $ANDROID_SRC \
    -I$K/fude -I$K/apps/$WHAT/src -Wall -Wextra \
    --output_path=$OUT/
APK=$OUT/$NAME.apk
if [ "$MODE" = "--release" ]; then APK=$OUT/$NAME-release.apk; fi   # beside the debug one
cp $OUT/game.apk $APK
echo "$APK"
