#!/bin/sh
# Kana's tests: each suite built with clang — AddressSanitizer and UBSan on — from
# the app's own sources and the stand-ins in tests/support/, then run against the
# real character data (assets/data/characters.kana).
#
#   tests/run.sh               every suite
#   tests/run.sh vocab exam    just those
#
# RDE is looked for beside the project (../RDE) or where RDE= says. Its header is
# used as it is, with a desktop rde_config.h of the tests' own (the engine's is
# whichever platform it was built for last). Builds and the suites' files go to
# tests/build/ (ignored). macOS or Linux; on Windows, from WSL.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RDE=${RDE:-$ROOT/../RDE}
if [ ! -f "$RDE/engine/include/rde.h" ]; then
    echo "RDE not found at $RDE: set RDE=/path/to/RDE" >&2
    exit 2
fi
OUT=$ROOT/tests/build
mkdir -p "$OUT/rde"
rm -f "$OUT/failed"
cp "$RDE/engine/include/rde.h" "$OUT/rde/"
[ -f "$RDE/engine/include/rde_lua_bindings.h" ] && cp "$RDE/engine/include/rde_lua_bindings.h" "$OUT/rde/"
case $(uname) in Darwin) PLATFORM=RDE_PLATFORM_OSX ;; *) PLATFORM=RDE_PLATFORM_LINUX ;; esac
printf '#ifndef %s\n\t#define %s 1\n#endif\n' $PLATFORM $PLATFORM > "$OUT/rde/rde_config.h"

CFLAGS="-std=c99 -g -O1 -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function -Wno-implicit-function-declaration"
CFLAGS="$CFLAGS -I$OUT/rde -I$RDE/external/include -I$ROOT/src -DKANA_ROOT=\"$ROOT\""
CFLAGS="$CFLAGS -include stdio.h -include stdlib.h -include string.h -include stdarg.h -include sys/stat.h"

# name | its own flags | what it is built from (paths from the project root)
SUITES='
kanji|-include base/save.h|tests/kanji/test.c tests/support/engine.c tests/support/arr.c src/chars/kanji.c src/base/kfile.c
catalog|-include base/save.h|tests/catalog/test.c tests/support/engine.c tests/support/arr.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/study/marks.c src/lang/ja/romaji.c
score|-include base/save.h|tests/score/test.c tests/support/engine.c tests/support/arr.c src/handwriting/score.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/study/marks.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c
match|-include base/save.h|tests/match/test.c tests/support/engine.c tests/support/arr.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/widgets/draw.c src/study/marks.c tests/support/rich.c src/lang/ja/romaji.c
history|-include unistd.h -include base/save.h|tests/history/test.c tests/support/engine.c tests/support/arr.c src/study/history.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/handwriting/score.c src/handwriting/match.c src/chars/kanji.c src/chars/catalog.c src/study/marks.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c
album|-include unistd.h -include base/save.h|tests/album/test.c tests/support/engine.c tests/support/arr.c src/screens/album.c src/screens/exam.c src/study/review.c src/study/examlog.c src/study/vocab.c src/study/select.c src/screens/chart.c src/handwriting/recognize.c src/services/mlkit.c src/study/history.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/handwriting/score.c src/handwriting/match.c src/chars/kanji.c src/chars/catalog.c src/widgets/glyph.c src/widgets/scroll.c src/widgets/draw.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c src/widgets/header.c
parts|-include unistd.h -include base/save.h|tests/parts/test.c tests/support/engine.c tests/support/arr.c src/screens/browse.c src/chars/catalog.c src/handwriting/match.c src/widgets/scroll.c src/screens/chart.c src/ink/ink.c src/widgets/glyph.c src/chars/kanji.c src/base/kfile.c src/base/theme.c src/widgets/draw.c src/handwriting/recognize.c src/services/mlkit.c src/study/select.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c
set|-include unistd.h -include base/save.h|tests/set/test.c tests/support/engine.c tests/support/arr.c src/screens/practice.c src/handwriting/guide.c src/study/history.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/handwriting/score.c src/handwriting/match.c src/chars/kanji.c src/chars/catalog.c src/widgets/glyph.c src/widgets/draw.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c tests/support/app.c src/lang/ja/romaji.c
check|-include unistd.h -include base/save.h|tests/check/test.c tests/support/engine.c tests/support/arr.c src/screens/check.c src/handwriting/segment.c src/handwriting/score.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/widgets/glyph.c src/base/theme.c src/widgets/scroll.c src/widgets/draw.c src/handwriting/recognize.c src/services/mlkit.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c tests/support/app.c src/lang/ja/romaji.c src/widgets/header.c
notes|-include unistd.h -include base/save.h|tests/notes/test.c tests/support/engine.c tests/support/arr.c src/ink/notes.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/base/text.c tests/support/text.c
save||tests/save/test.c tests/support/arr.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c
lasso|-include base/save.h|tests/lasso/test.c tests/support/engine.c tests/support/arr.c src/ink/ink.c src/ink/lasso.c src/ink/canvas.c src/base/theme.c src/widgets/draw.c src/chars/kanji.c src/base/kfile.c tests/support/rich.c
guide|-include unistd.h -include base/save.h|tests/guide/test.c tests/support/engine.c tests/support/arr.c src/screens/practice.c src/handwriting/guide.c src/study/history.c src/base/save.c src/base/kfile.c src/ink/ink.c src/base/theme.c src/handwriting/score.c src/handwriting/match.c src/chars/kanji.c src/chars/catalog.c src/widgets/glyph.c src/widgets/draw.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c tests/support/app.c src/lang/ja/romaji.c
exam|-include unistd.h -include base/save.h|tests/exam/test.c tests/support/engine.c tests/support/arr.c src/study/vocab.c src/screens/exam.c src/study/review.c src/study/examlog.c src/study/marks.c src/study/select.c src/screens/chart.c src/widgets/scroll.c src/handwriting/recognize.c src/services/mlkit.c src/handwriting/score.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/widgets/glyph.c src/base/theme.c src/widgets/draw.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c src/widgets/header.c
stats|-include unistd.h -include base/save.h|tests/stats/test.c tests/support/engine.c tests/support/arr.c src/study/vocab.c src/screens/stats.c src/study/history.c src/base/save.c src/study/examlog.c src/study/marks.c src/screens/chart.c src/widgets/scroll.c src/study/select.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/widgets/glyph.c src/base/theme.c src/widgets/draw.c src/screens/exam.c src/study/review.c src/handwriting/recognize.c src/services/mlkit.c src/handwriting/score.c src/handwriting/match.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c src/widgets/header.c
text||tests/text/test.c src/base/text.c tests/support/text.c
textink|-include unistd.h -include base/save.h|tests/textink/test.c tests/support/engine.c tests/support/arr.c src/handwriting/textink.c src/ink/lasso.c src/handwriting/segment.c src/handwriting/score.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/widgets/glyph.c src/base/theme.c src/widgets/draw.c src/handwriting/recognize.c src/services/mlkit.c src/study/marks.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c
scan|-DRDE_DEBUG -include unistd.h -include base/save.h|tests/scan/test.c tests/scan/stubs.c tests/support/engine.c tests/support/arr.c src/screens/scan.c src/study/vocab.c src/lang/ja/wordsplit.c src/services/textscan.c src/services/translate.c src/services/speech.c src/services/mlkit.c src/widgets/scroll.c src/widgets/draw.c src/base/theme.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c src/widgets/header.c src/widgets/glyph.c
backup||tests/backup/test.c src/base/backup.c
review|-DKANA_TESTS -include base/save.h|tests/review/test.c tests/support/engine.c tests/support/arr.c src/study/review.c src/base/kfile.c
wordsplit|-include base/save.h|tests/wordsplit/test.c tests/support/engine.c tests/support/arr.c src/lang/ja/wordsplit.c src/chars/kanji.c src/base/kfile.c
sheet|-include base/save.h|tests/sheet/test.c tests/support/engine.c tests/support/arr.c src/study/sheet.c src/chars/kanji.c src/base/kfile.c src/base/text.c tests/support/text.c
vocab|-include base/save.h|tests/vocab/test.c tests/support/engine.c tests/support/arr.c src/study/vocab.c src/chars/kanji.c src/base/kfile.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c
wordexam|-include unistd.h -include base/save.h|tests/wordexam/test.c tests/support/engine.c tests/support/arr.c src/screens/wordexam.c src/study/vocab.c src/study/review.c src/study/charnote.c src/services/speech.c src/study/marks.c src/widgets/scroll.c src/handwriting/recognize.c src/services/mlkit.c src/handwriting/score.c src/handwriting/match.c src/chars/catalog.c src/chars/kanji.c src/base/kfile.c src/ink/ink.c src/widgets/glyph.c src/base/theme.c src/widgets/draw.c tests/support/rich.c src/base/text.c tests/support/text.c src/lang/ja/romaji.c tests/support/app.c src/widgets/header.c
'

echo "$SUITES" | while IFS='|' read -r name flags sources; do
    [ -z "$name" ] && continue
    if [ $# -gt 0 ]; then
        case " $* " in *" $name "*) ;; *) continue ;; esac
    fi
    dir=$OUT/$name
    mkdir -p "$dir"
    files=""
    for f in $sources; do files="$files $ROOT/$f"; done
    # $CFLAGS, $flags and $files split into words on purpose.
    if ! clang $CFLAGS $flags $files -o "$dir/$name" -lm > "$dir/build.log" 2>&1; then
        printf '%-10s BUILD FAILED (tests/build/%s/build.log)\n' "$name" "$name"
        grep -m5 "error" "$dir/build.log"
        echo "$name" >> "$OUT/failed"
        continue
    fi
    # Each suite in its own folder: they write their saves where they run.
    rm -rf "$dir/saves" "$dir/sheets"
    result=$(cd "$dir" && "./$name" "$ROOT/assets/data/characters.kana" 2>&1 | tail -1)
    printf '%-10s %s\n' "$name" "$result"
    case "$result" in *"ALL PASSED"*) ;; *) echo "$name" >> "$OUT/failed" ;; esac
done
if [ -f "$OUT/failed" ]; then
    echo "failed: $(tr '\n' ' ' < "$OUT/failed")"
    rm -f "$OUT/failed"
    exit 1
fi
echo "every suite passed"
