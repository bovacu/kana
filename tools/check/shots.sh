#!/bin/zsh
# Screenshots of every app's screens, compared before and after a change: the check that a cleanup changed nothing
# a user can see.
#
#   zsh tools/check/shots.sh --baseline [app...]   # capture into build/shots/baseline/ (before the change)
#   zsh tools/check/shots.sh [app...]              # capture into build/shots/current/ and compare with the baseline
#
# The apps must be built for the Mac first (zsh tools/build/app.sh <app>, or tools/check/build_all.sh). The screens
# are listed in tools/check/shots.txt. A capture runs the app's debug build with the look harness's flags and --shot;
# each starts from the same saves, so the same code draws the same pixels. Prints one line per picture that is
# DIFFERENT, NEW or MISSING, and exits 1 if any is DIFFERENT or MISSING. Pictures are compared byte for byte (the PNG
# encoder is deterministic): open both to see what changed.
K=$(cd "$(dirname "$0")/../.." && pwd)
RUN=current
if [ "$1" = "--baseline" ]; then RUN=baseline; shift; fi
APPS=($@)
if (( ${#APPS} == 0 )); then APPS=(kana hanzi hangul thai hindi arabic sketching infinote infiniworkshop); fi

# Each study app's sample character (hex): the viewer's and practice's. The apps without one are the canvas apps.
typeset -A CP=(kana 6728 hanzi 6C49 hangul AC00 thai 0E01 hindi 0915 arabic 0628)
app_kind() {
    if [[ -n ${CP[$1]} ]]; then echo study; else echo canvas; fi
}

capture() {   # $1 app, $2 name, $3... flags
    local app=$1 name=$2; shift 2
    local size=744x1133
    [ "$(app_kind $app)" = "canvas" ] && size=1024x768
    rm -rf saves && cp -R seed saves
    perl -e 'alarm 120; exec @ARGV' ./runnable --size=$size "$@" --shot=$K/build/shots/$RUN/$app/$name.png > $K/build/shots/$RUN/$app/$name.log 2>&1
}

for app in $APPS; do
    EXE=$K/build/$app/osx-silicon/debug/project/exe
    if [ ! -x $EXE/runnable ]; then echo "$app: not built (zsh tools/build/app.sh $app)"; continue; fi
    DIR=$K/build/shots/run/$app
    mkdir -p $DIR $K/build/shots/$RUN/$app
    cd $DIR
    ln -sfn $EXE/runnable runnable; ln -sfn $EXE/libRDE.dylib libRDE.dylib; ln -sfn $EXE/assets assets
    # The first run, from nothing: the welcome, and the saves every other capture starts from.
    rm -rf saves seed
    size=744x1133; [ "$(app_kind $app)" = "canvas" ] && size=1024x768
    welcome=(); [ "$(app_kind $app)" = "canvas" ] && welcome=(--welcome=5 --shot-at=30)
    perl -e 'alarm 120; exec @ARGV' ./runnable --size=$size $welcome --shot=$K/build/shots/$RUN/$app/welcome.png > $K/build/shots/$RUN/$app/welcome.log 2>&1
    cp -R saves seed
    kind=$(app_kind $app)
    while IFS='|' read -r who name flags; do
        who=${who// /}; name=${name// /}
        [[ -z $who || $who == \#* ]] && continue
        [[ $who != $kind && $who != $app ]] && continue
        flags=${flags//\{CP\}/${CP[$app]}}
        capture $app $name ${=flags}
    done < $K/tools/check/shots.txt
    cd $K
done

[ $RUN = baseline ] && { echo "baseline captured: build/shots/baseline/"; exit 0; }

bad=0
for app in $APPS; do
    for f in $K/build/shots/baseline/$app/*.png(N); do
        n=${f:t}
        if [ ! -f $K/build/shots/current/$app/$n ]; then echo "MISSING   $app/$n"; bad=1
        elif ! cmp -s $f $K/build/shots/current/$app/$n; then echo "DIFFERENT $app/$n"; bad=1; fi
    done
    for f in $K/build/shots/current/$app/*.png(N); do
        [ -f $K/build/shots/baseline/$app/${f:t} ] || echo "NEW       $app/${f:t}"
    done
done
(( bad )) && exit 1
echo "every screen is as it was"
