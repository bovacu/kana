#!/bin/zsh
# Every compiler warning in the apps' code. A build shows the warnings of the files it compiles, and an incremental
# build compiles few; this checks them all. Each source file of every app is checked once (a fude file with the first
# app that builds it), for the Mac, with the very command the app's debug build compiles it with, and nothing is
# written: `builder compdb` puts those commands in apps/<app>/compile_commands.json (which editors read too), and each
# runs here with -fsyntax-only instead of its outputs. Whisper's headers come in with -isystem (the project files say
# so), so their own warnings are not counted.
#
#   zsh tools/check/warnings.sh          # the warnings, one per line (file:line: text), and how many
#
# Exits 1 if there is any. The goal is none.
K=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$K/build/check/warnings.txt
mkdir -p $K/build/check
: > $OUT
typeset -A seen

# A compile_commands.json as lines of folder, file and command (tab-separated), each command without -c, -o and the
# dependency file, and with -fsyntax-only.
COMMANDS='
import json, os, shlex, sys
for entry in json.load(open(sys.argv[1])):
    words, kept, skip = shlex.split(entry["command"]), [], False
    for word in words:
        if skip:
            skip = False
        elif word in ("-o", "-MF"):
            skip = True
        elif word not in ("-c", "-MMD"):
            kept.append(word)
    print(entry["directory"], os.path.normpath(entry["file"]), shlex.join(kept + ["-fsyntax-only"]), sep="\t")
'

for app in draw kana hanzi hangul thai hindi arabic sketching infinote infiniworkshop; do   # (Draw too: its shell is its own)
    if ! zsh $K/tools/build/app.sh $app compdb > $K/build/check/compdb-$app.log 2>&1; then
        echo "$app: builder compdb failed (build/check/compdb-$app.log)" | tee -a $OUT
        continue
    fi
    python3 -c $COMMANDS $K/apps/$app/compile_commands.json > $K/build/check/compdb-$app.tsv
    while IFS=$'\t' read -r dir file command; do
        [[ -n ${seen[$file]} ]] && continue
        seen[$file]=1
        # Each diagnostic's file is as clang found it, from the app's folder: made relative to the repository.
        for line in ${(f)"$(cd $dir && eval $command 2>&1)"}; do
            [[ $line == *(warning|error):* ]] || continue
            if [[ $line =~ '^([^:]+):[0-9]+:' ]]; then
                p=$match[1]
                [[ $p == /* ]] || p=$dir/$p
                line="${${p:A}#$K/}:${line#*:}"
            fi
            echo $line
        done >> $OUT
    done < $K/build/check/compdb-$app.tsv
done
sort -u -o $OUT $OUT
cat $OUT
n=$(wc -l < $OUT | tr -d ' ')
echo "$n warnings (build/check/warnings.txt)"
(( n == 0 ))
