#!/bin/zsh
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.
#
# whisper.cpp (MIT, github.com/ggml-org/whisper.cpp): speech to text for the recordings (fude/zoom/listen.h), built
# as static libraries for the platforms the apps ship on, from a pinned release cloned into build/ (never in the
# repository). Run once, and again after changing the pin:
#
#   zsh tools/whisper/build.sh            # Mac (arm64) and Android (arm64-v8a)
#
# Into build/whisper/<platform>/lib and build/whisper/include. The apps that transcribe (InfiNote, Sketching:
# tools/android/build.sh, COMMANDS.txt) link them and are built with -DFUDE_WHISPER.
#
#   mac            static libraries, for Apple silicon (every one has the dot product and half floats)
#   android-arm64  shared ones (libwhisper.so, libggml.so, libggml-base.so) and the CPU's code four times over, each a
#                  library of its own (libggml-cpu-android_armv8.0_1.so: any ARMv8; _armv8.2_1: the dot product;
#                  _armv8.2_2: and half floats; _armv8.6_1: and int8 matrices) — the app loads the best its CPU can run
#                  (listen.c), so a Galaxy Tab S6 Lite's Exynos 9611 (none of them) gets the first, a newer one the
#                  fastest it has. Packed into the APK beside libmain.so (the builder's --android_lib).
#
# The model is not here: the app downloads it the first time it is wanted (listen.h).
set -e
TAG=v1.9.5
K=$(cd "$(dirname "$0")/../.." && pwd)
W=$K/build/whisper
SRC=$W/src
SDK=${ANDROID_SDK:-$HOME/Library/Android/sdk}
NDK=${ANDROID_NDK:-$(ls -d $SDK/ndk/*/ | sort -V | tail -1)}
mkdir -p $W
if [ ! -f $SRC/include/whisper.h ] || [ "$(cat $W/tag 2>/dev/null)" != "$TAG" ]; then
    rm -rf $SRC
    git clone --depth 1 --branch $TAG https://github.com/ggml-org/whisper.cpp $SRC
    echo $TAG > $W/tag
fi
COMMON=(-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF
        -DWHISPER_BUILD_SERVER=OFF -DWHISPER_SDL2=OFF -DWHISPER_CURL=OFF -DGGML_OPENMP=OFF -DGGML_METAL=OFF -DGGML_BLAS=OFF
        -DGGML_ACCELERATE=OFF -DGGML_LLAMAFILE=OFF)

build() {   # build <platform> <cmake args...>
    local P=$1; shift
    cmake -S $SRC -B $W/$P-build "${COMMON[@]}" "$@" > $W/$P.log 2>&1
    cmake --build $W/$P-build --config Release -j 8 >> $W/$P.log 2>&1
    rm -rf $W/$P && mkdir -p $W/$P/lib
    find $W/$P-build \( -name "*.a" -o -name "*.so" \) -exec cp {} $W/$P/lib/ \;
    echo "$P: $(ls $W/$P/lib | tr '\n' ' ')"
}

build mac -DCMAKE_OSX_ARCHITECTURES=arm64 -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod
build android-arm64 -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_shared -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON \
      -DGGML_NATIVE=OFF
# (the shared ones stripped: their symbols are what makes them big)
STRIP=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip
for f in $W/android-arm64/lib/*.so; do $STRIP --strip-unneeded $f; done
find $W/android-arm64/lib -name "*.a" -delete
rm -rf $W/include && mkdir -p $W/include
cp $SRC/include/whisper.h $SRC/ggml/include/*.h $W/include/
echo "headers: $(ls $W/include | wc -l | tr -d ' ')"
