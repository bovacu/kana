#!/bin/zsh
# Builds every app, so a change that breaks one is seen at once. Each build is tools/build/app.sh's, from the app's
# project file (apps/<app>/<app>.rdeproj).
#
#   zsh tools/check/build_all.sh              # every app for the Mac (debug): about a minute an app
#   zsh tools/check/build_all.sh --android    # and the nine released apps' release APKs
#   zsh tools/check/build_all.sh --sim        # and the nine for the iOS Simulator (both may be given)
#
# Prints one line per build (ok, or FAILED with the first errors) and exits 1 if any failed. Logs:
# build/check/<mac|android|sim>-<app>.log. There is no engine step: a build first rebuilds the engine it links when that
# is missing or older than the engine's sources, for its own platform, so the first app of a platform pays for it.
K=$(cd "$(dirname "$0")/../.." && pwd)
for option in "$@"; do
    case $option in
        --android|--sim) ;;
        *) echo "unknown option $option (--android, --sim)" >&2; exit 1 ;;
    esac
done
APPS=(kana hanzi hangul thai hindi arabic sketching infinote infiniworkshop)
LOGS=$K/build/check
mkdir -p $LOGS
failed=()

build() {   # $1 label, $2... the command
    local label=$1; shift
    if "$@" > $LOGS/$label.log 2>&1 && grep -q "Build finished successfully" $LOGS/$label.log; then
        printf '%-28s ok\n' $label
    else
        printf '%-28s FAILED (build/check/%s.log)\n' $label $label
        grep -m5 -E " error:|Undefined|ld: |ERROR|problem" $LOGS/$label.log | sed 's/^/    /'
        failed+=($label)
    fi
}

# Draw too, on the Mac: it is not released, but it shows the drawing core still builds on its own.
for app in draw $APPS; do
    build mac-$app zsh $K/tools/build/app.sh $app
done

for option in "$@"; do
    case $option in
        --android) for app in $APPS; do build android-$app zsh $K/tools/build/app.sh $app android; done ;;
        --sim)     for app in $APPS; do build sim-$app zsh $K/tools/build/app.sh $app sim; done ;;
    esac
done

if (( ${#failed} )); then
    echo "failed: $failed"
    exit 1
fi
echo "every build passed"
