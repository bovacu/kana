#!/bin/zsh
# Builds an app with RDE's builder, from its project file: apps/<app>/<app>.rdeproj says everything about the build
# (sources, flags, assets, frameworks, packaging) but the four things this script does first.
#
#   zsh tools/build/app.sh <app> [command] [profile] [builder options] [-- the app's arguments]
#
#   zsh tools/build/app.sh kana                   the Mac's debug build: build/kana/osx-silicon/debug/project/exe/runnable
#   zsh tools/build/app.sh kana release           the Mac's release (it takes no launch arguments)
#   zsh tools/build/app.sh kana measure           a release that takes them, to measure it: build/kana-measure/
#   zsh tools/build/app.sh kana install android-measure   the same for the tablet (arguments through adb: COMMANDS.txt)
#   zsh tools/build/app.sh kana android           the release APK: build/kana-android/game.apk
#   zsh tools/build/app.sh kana install android   ...installed on the tablet with adb, and started (--device=SERIAL)
#   zsh tools/build/app.sh kana bundle            Google Play's App Bundle: build/kana-android/game.aab
#   zsh tools/build/app.sh kana ios               the iPad's, signed: build/kana-ios/Kana.ipa (install ios: onto it)
#   zsh tools/build/app.sh kana run sim           the Simulator's, installed on the booted Simulator and started
#   zsh tools/build/app.sh kana show android      every setting of a profile, where it came from, and its command
#
# The commands and the profiles are the builder's (~/RDE/docs/docs/project_files.md); every app has these profiles
# (Draw: the computer's three). What this script does, because a project file cannot say it:
#   - The version. It is written once, in apps/<app>/src/version.h (<APP>_VERSION: the app shows it), and given to the
#     builder as --version: Android's versionName and versionCode, iOS's two bundle versions, config.rdef's version.
#     The builder does not hand a project file's version to the C code, so there it would be written twice.
#   - The staged assets. A project file whose assets are build/<app>-stage/assets/ (InfiNote's, InfiniWorkshop's) gets
#     Sketching's there with the app's own over them: the builder takes one folder.
#   - whisper.cpp. A project file that links it (speech to text) needs its libraries, which tools/whisper/build.sh
#     builds: run here when they are missing.
#   - iOS signing, which is the developer's and not the app's, from the environment: FUDE_IOS_PROFILE, the
#     .mobileprovision made in Xcode for com.rde.<app> (a free team's lasts 7 days), and FUDE_APPLE_TEAM
#     ("you@example.com (TEAMID)"), needed only when the keychain holds more than one Apple Development identity.
# And Android's JDK is Android Studio's, as the APKs have always been built with.
set -e
K=$(cd "$(dirname "$0")/../.." && pwd)
R=${RDE:-$K/../RDE}
APP=$1
PROJECT=$K/apps/$APP/$APP.rdeproj
if [[ -z $APP || ! -f $PROJECT ]]; then
    echo "usage: zsh tools/build/app.sh <app> [command] [profile] [builder options]   (apps/<app>/<app>.rdeproj)" >&2
    exit 1
fi
shift

VERSION=$(sed -n 's/^#define [A-Z_]*_VERSION "\([0-9.]*\)".*/\1/p' $K/apps/$APP/src/version.h | head -1)
if [[ -z $VERSION ]]; then
    echo "apps/$APP/src/version.h has no #define <APP>_VERSION \"X.Y.Z\"" >&2
    exit 1
fi

# Put together in a scratch folder, then copied over the staged one by rsync, which rewrites only the files that
# changed (each by a rename): a build of the same app that is running meanwhile never sees the folder half made.
if grep -q "^assets *= *../../build/$APP-stage/assets" $PROJECT; then
    mkdir -p $K/build/$APP-stage/assets
    T=$(mktemp -d $K/build/$APP-stage.XXXXXX)
    cp -pR $K/apps/sketching/assets/. $T/
    cp -pR $K/apps/$APP/assets/. $T/
    rsync -a --delete $T/ $K/build/$APP-stage/assets/
    rm -rf $T
fi

if grep -q 'build/whisper/' $PROJECT; then
    if [[ ! -f $K/build/whisper/mac/lib/libwhisper.a || ! -f $K/build/whisper/android-arm64/lib/libwhisper.so ]]; then
        zsh $K/tools/whisper/build.sh
    fi
fi

SIGN=()
if [[ -n $FUDE_IOS_PROFILE ]]; then SIGN+=("--ios_provisioning_profile=$FUDE_IOS_PROFILE"); fi
if [[ -n $FUDE_APPLE_TEAM ]]; then SIGN+=("--apple_development_name=$FUDE_APPLE_TEAM"); fi

# Android Studio's JDK, by its own path: the builder quotes every argument, so the space in it is fine.
JBR="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
if [[ -d $JBR ]]; then
    export JAVA_HOME=$JBR
    export PATH="$JAVA_HOME/bin:$PATH"
fi

cd $K
exec $R/builder --file=$PROJECT --version=$VERSION "${SIGN[@]}" "$@"
