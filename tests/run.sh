#!/bin/sh
# Kana's tests: each suite built with clang — AddressSanitizer and UBSan on — from
# the app's own sources and the stand-ins in tests/support/, then run against the
# real character data (apps/kana/assets/data/characters.kana).
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
CFLAGS="$CFLAGS -I$OUT/rde -I$RDE/external/include -I$ROOT/fude -I$ROOT/apps/kana/src -DFUDE_ROOT=\"$ROOT/apps/kana\""
CFLAGS="$CFLAGS -include stdio.h -include stdlib.h -include string.h -include stdarg.h -include sys/stat.h"

# name | its own flags | what it is built from (paths from the project root)
SUITES='
kanji|-include drawing/base/save.h|tests/kanji/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/chars/kanji.c fude/drawing/base/kfile.c
catalog|-include drawing/base/save.h|tests/catalog/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/study/models/marks.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
score|-include drawing/base/save.h|tests/score/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/models/marks.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
match|-include drawing/base/save.h|tests/match/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c fude/study/models/marks.c tests/support/rich.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
history|-include unistd.h -include drawing/base/save.h|tests/history/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/models/marks.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
album|-include unistd.h -include drawing/base/save.h|tests/album/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/album.c fude/study/screens/exam.c fude/study/models/review.c fude/study/models/examlog.c fude/study/models/vocab.c fude/study/models/select.c fude/lang/ja/chart.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/widgets/scroll.c fude/drawing/widgets/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c
parts|-include unistd.h -include drawing/base/save.h|tests/parts/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/browse.c fude/study/chars/catalog.c fude/study/handwriting/match.c fude/drawing/widgets/scroll.c fude/lang/ja/chart.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/models/select.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c
set|-include unistd.h -include drawing/base/save.h|tests/set/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/practice.c fude/study/handwriting/guide.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/widgets/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
check|-include unistd.h -include drawing/base/save.h|tests/check/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/check.c fude/study/handwriting/segment.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/scroll.c fude/drawing/widgets/draw.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/study/widgets/header.c
notes|-include unistd.h -include drawing/base/save.h|tests/notes/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/drawing/ink/notes.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/base/text.c tests/support/text.c
save||tests/save/test.c fude/drawing/base/utf8.c tests/support/arr.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/ink/canvas.c fude/drawing/widgets/draw.c tests/support/engine.c tests/support/rich.c
lasso|-include drawing/base/save.h|tests/lasso/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/drawing/ink/ink.c fude/drawing/ink/lasso.c fude/drawing/ink/canvas.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c fude/study/chars/kanji.c fude/drawing/base/kfile.c tests/support/rich.c
guide|-include unistd.h -include drawing/base/save.h|tests/guide/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/practice.c fude/study/handwriting/guide.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/widgets/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
exam|-include unistd.h -include drawing/base/save.h|tests/exam/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/vocab.c fude/study/screens/exam.c fude/study/models/review.c fude/study/models/examlog.c fude/study/models/marks.c fude/study/models/select.c fude/lang/ja/chart.c fude/drawing/widgets/scroll.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c
stats|-include unistd.h -include drawing/base/save.h|tests/stats/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/vocab.c fude/study/screens/stats.c fude/study/models/history.c fude/drawing/base/save.c fude/study/models/examlog.c fude/study/models/marks.c fude/lang/ja/chart.c fude/drawing/widgets/scroll.c fude/study/models/select.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c fude/study/screens/exam.c fude/study/models/review.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c
text||tests/text/test.c fude/drawing/base/utf8.c fude/drawing/base/text.c tests/support/text.c
textink|-include unistd.h -include drawing/base/save.h|tests/textink/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/handwriting/textink.c fude/drawing/ink/lasso.c fude/study/handwriting/segment.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
scan|-DRDE_DEBUG -include unistd.h -include drawing/base/save.h|tests/scan/test.c fude/drawing/base/utf8.c tests/scan/stubs.c tests/support/engine.c tests/support/arr.c fude/study/screens/scan.c fude/study/models/vocab.c fude/lang/ja/wordsplit.c fude/study/services/textscan.c fude/study/services/translate.c fude/study/services/speech.c fude/study/services/mlkit.c fude/drawing/widgets/scroll.c fude/drawing/widgets/draw.c fude/drawing/base/theme.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/study/chars/glyph.c
backup||tests/backup/test.c fude/drawing/base/utf8.c fude/drawing/base/backup.c
review|-DFUDE_TESTS -include drawing/base/save.h|tests/review/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/review.c fude/drawing/base/kfile.c
wordsplit|-include drawing/base/save.h|tests/wordsplit/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/lang/ja/wordsplit.c fude/study/chars/kanji.c fude/drawing/base/kfile.c
sheet|-include drawing/base/save.h|tests/sheet/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/sheet.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
vocab|-include drawing/base/save.h|tests/vocab/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/models/vocab.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
zoom||tests/zoom/test.c fude/zoom/nest.c fude/zoom/map.c fude/zoom/piece.c fude/zoom/sheet.c fude/zoom/stl.c fude/zoom/snap.c fude/zoom/cut.c fude/zoom/select.c fude/zoom/bucket.c fude/zoom/connect.c fude/zoom/instrument.c fude/zoom/units.c fude/zoom/pdf.c fude/zoom/dxf.c fude/zoom/zoom.c fude/zoom/shape.c fude/zoom/smooth.c fude/zoom/nav.c fude/zoom/fill.c fude/zoom/export.c fude/drawing/base/theme.c fude/zoom/codec.c fude/zoom/index.c fude/zoom/scene.c fude/zoom/erase.c fude/zoom/zfile.c fude/zoom/video.c fude/zoom/handfind.c fude/zoom/symbol.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c
units||tests/units/test.c fude/zoom/units.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c
pdf||tests/pdf/test.c fude/zoom/pdf.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c
dxf||tests/dxf/test.c fude/zoom/dxf.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c
graph||tests/graph/test.c fude/zoom/graph.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c
wordexam|-include unistd.h -include drawing/base/save.h|tests/wordexam/test.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/arr.c fude/study/screens/wordexam.c fude/study/models/vocab.c fude/study/models/review.c fude/study/models/charnote.c fude/study/services/speech.c fude/study/models/marks.c fude/drawing/widgets/scroll.c fude/study/handwriting/recognize.c fude/study/services/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/draw.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c
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
    result=$(cd "$dir" && "./$name" "$ROOT/apps/kana/assets/data/characters.kana" 2>&1 | tail -1)
    printf '%-10s %s\n' "$name" "$result"
    case "$result" in *"ALL PASSED"*) ;; *) echo "$name" >> "$OUT/failed" ;; esac
done
if [ -f "$OUT/failed" ]; then
    echo "failed: $(tr '\n' ' ' < "$OUT/failed")"
    rm -f "$OUT/failed"
    exit 1
fi
echo "every suite passed"
