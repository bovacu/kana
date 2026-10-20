#!/bin/sh
# Kana's tests: each suite built with clang — AddressSanitizer and UBSan on — from
# the app's own sources and the stand-ins in tests/support/, then run against the
# real character data (apps/kana/assets/data/characters.kana).
#
#   tests/run.sh               every suite
#   tests/run.sh vocab exam    just those
#
# RDE is looked for beside the project (../RDE) or where RDE= says. Its header is
# used as it is: it tells the platform from the compiler's own defines. Builds and the suites' files go to
# tests/build/ (ignored), or to the folder TESTS_OUT= names, so two runs at once each build in their own.
# macOS or Linux; on Windows, from WSL.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RDE=${RDE:-$ROOT/../RDE}
if [ ! -f "$RDE/engine/include/rde.h" ]; then
    echo "RDE not found at $RDE: set RDE=/path/to/RDE" >&2
    exit 2
fi
OUT=${TESTS_OUT:-$ROOT/tests/build}
mkdir -p "$OUT"
rm -f "$OUT/failed"

# The engine's real library (its Mac or Linux debug build), linked into every suite: sets up
# its allocator. Built here if it is missing.
case $(uname) in Darwin) RDE_LIB=${RDE_LIB:-$RDE/build/osx-silicon/debug/engine/lib} ;; *) RDE_LIB=${RDE_LIB:-$RDE/build/linux/debug/engine/lib} ;; esac
if [ ! -f "$RDE_LIB/libRDE.dylib" ] && [ ! -f "$RDE_LIB/libRDE.so" ]; then
    echo "building the engine's library ($RDE_LIB)…"
    (cd "$RDE" && ./builder --engine --debug) > "$OUT/engine.log" 2>&1 || { echo "the engine did not build: $OUT/engine.log" >&2; exit 2; }
fi
LINK="-L$RDE_LIB -lRDE -Wl,-rpath,$RDE_LIB"
# A sanitizer's report ends the suite with exit 1, not abort(): an abort goes to macOS's crash reporter, which can hold
# the process (and the run) for many minutes. Undefined behaviour stops the suite at once, with its stack.
export ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=0}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}

# (no -Wno-implicit-function-declaration: a suite that calls what it does not include fails to build, as the apps do)
CFLAGS="-std=c99 -g -O1 -fsanitize=address,undefined -Wall -Wextra -Wno-unused-function"
CFLAGS="$CFLAGS -I$RDE/engine/include -I$RDE/external/include -I$ROOT/fude -I$ROOT/apps/kana/src"
CFLAGS="$CFLAGS -include stdio.h -include stdlib.h -include string.h -include stdarg.h -include sys/stat.h"

# name | its own flags | what it is built from (paths from the project root). In the flags, @RDE is RDE's folder,
# @CANVAS builds the suite with a canvas app's strings, and @APP=<app> builds it as that study app (its strings, its
# character data: a suite of another language's fude/lang/<code>); in the files, @BOX2D is Box2D's sources.
SUITES='
kanji|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/kanji/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/chars/kanji.c fude/drawing/base/kfile.c
catalog|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/catalog/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/study/models/marks.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/save.c
score|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/score/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/models/marks.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/save.c
match|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/match/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/models/marks.c tests/support/rich.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/save.c
history|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/history/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/models/marks.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
album|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/album/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/album.c fude/study/screens/exam.c fude/study/models/review.c fude/study/models/examlog.c fude/study/models/vocab.c fude/study/models/select.c fude/lang/ja/chart.c fude/study/screens/chart.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/widgets/scroll.c fude/drawing/base/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/drawing/base/redraw.c
parts|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/parts/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/browse.c fude/study/chars/catalog.c fude/study/handwriting/match.c fude/drawing/widgets/scroll.c fude/lang/ja/chart.c fude/study/screens/chart.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/models/select.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/drawing/base/save.c fude/drawing/base/redraw.c
set|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/set/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/practice.c fude/study/handwriting/guide.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/base/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/clock.c fude/drawing/base/redraw.c
glyph|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/glyph/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/chars/glyph.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/theme.c fude/drawing/base/draw.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/redraw.c fude/drawing/base/save.c
check|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/check/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/check.c fude/study/handwriting/segment.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/widgets/scroll.c fude/drawing/base/draw.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/study/widgets/header.c fude/drawing/base/save.c fude/drawing/base/redraw.c
notes|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/notes/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/drawing/ink/notes.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/base/text.c tests/support/text.c
save||tests/save/test.c fude/drawing/base/utf8.c fude/drawing/base/save.c fude/drawing/ink/pagefile.c fude/drawing/app/settings.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/ink/canvas.c fude/drawing/base/draw.c tests/support/engine.c tests/support/rich.c fude/drawing/base/redraw.c
lasso|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/lasso/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/drawing/ink/ink.c fude/drawing/ink/lasso.c fude/drawing/ink/canvas.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/chars/kanji.c fude/drawing/base/kfile.c tests/support/rich.c fude/drawing/base/redraw.c
guide|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/guide/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/practice.c fude/study/handwriting/guide.c fude/study/models/history.c fude/drawing/base/save.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/kanji.c fude/study/chars/catalog.c fude/study/chars/glyph.c fude/drawing/base/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c tests/support/app.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/clock.c fude/drawing/base/redraw.c
exam|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/exam/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/vocab.c fude/study/screens/exam.c fude/study/models/review.c fude/study/models/examlog.c fude/study/models/marks.c fude/study/models/select.c fude/lang/ja/chart.c fude/study/screens/chart.c fude/drawing/widgets/scroll.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/base/draw.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/drawing/base/save.c fude/drawing/base/redraw.c
stats|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/stats/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/vocab.c fude/study/screens/stats.c fude/study/models/history.c fude/drawing/base/save.c fude/study/models/examlog.c fude/study/models/marks.c fude/lang/ja/chart.c fude/study/screens/chart.c fude/drawing/widgets/scroll.c fude/study/models/select.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/screens/exam.c fude/study/models/review.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/drawing/base/redraw.c
text||tests/text/test.c fude/drawing/base/utf8.c fude/drawing/base/text.c tests/support/text.c
draw||tests/draw/test.c fude/drawing/base/utf8.c fude/drawing/base/theme.c fude/drawing/base/draw.c tests/support/engine.c tests/support/rich.c
textink|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/textink/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/handwriting/textink.c fude/drawing/ink/lasso.c fude/study/handwriting/segment.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/save.c fude/drawing/base/redraw.c
scan|-DRDE_DEBUG -include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/scan/test.c fude/drawing/base/utf8.c tests/scan/stubs.c tests/support/engine.c fude/study/screens/scan.c fude/study/models/vocab.c fude/lang/ja/wordsplit.c fude/study/chars/wordsplit.c fude/study/services/textscan.c fude/study/services/translate.c fude/study/services/speech.c fude/drawing/ink/mlkit.c fude/drawing/widgets/scroll.c fude/drawing/base/draw.c fude/drawing/base/theme.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/study/chars/glyph.c fude/drawing/base/save.c fude/drawing/base/redraw.c
backup||tests/backup/test.c fude/drawing/base/utf8.c fude/drawing/base/backup.c fude/drawing/base/kfile.c
review|-DFUDE_TESTS -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/review/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/review.c fude/drawing/base/kfile.c fude/drawing/base/save.c fude/drawing/base/redraw.c
wordsplit|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/wordsplit/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/lang/ja/wordsplit.c fude/study/chars/wordsplit.c fude/study/chars/kanji.c fude/drawing/base/kfile.c
sheet|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/sheet/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/sheet.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c
vocab|-include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/vocab/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/models/vocab.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c fude/drawing/base/save.c
crash|-include unistd.h|tests/crash/test.c tests/support/engine.c fude/drawing/base/crash.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c
zoom|@CANVAS|tests/zoom/test.c tests/support/zoomdraw.c fude/zoom/product.c fude/zoom/pages.c fude/zoom/words.c fude/zoom/homelist.c fude/zoom/inktest.c fude/zoom/adpcm.c fude/zoom/audio.c fude/zoom/voice.c fude/zoom/transcript.c fude/drawing/ink/notes.c fude/drawing/base/save.c fude/drawing/ink/ink.c fude/drawing/base/text.c tests/support/text.c fude/zoom/mathfont.c fude/zoom/tex.c fude/zoom/handmath.c fude/zoom/mathread.c fude/zoom/props.c fude/zoom/logic.c fude/sim/sparse.c fude/sim/body.c fude/sim/def.c fude/sim/elab.c fude/sim/digital.c fude/sim/logic.c fude/sim/chips.c fude/zoom/calc.c fude/zoom/plot.c fude/zoom/chart.c fude/zoom/plan.c fude/zoom/mech.c fude/zoom/examples.c fude/zoom/examples_electronics.c fude/zoom/examples_mechanisms.c fude/zoom/examples_both.c fude/zoom/placer.c fude/zoom/custom.c fude/zoom/circuit.c fude/zoom/circuit_catalogue.c fude/zoom/circuit_draw.c fude/zoom/circuit_read.c fude/zoom/circuit_wires.c fude/zoom/circuit_build.c fude/zoom/circuit_blocks.c fude/zoom/circuit_stamp.c fude/zoom/circuit_solve.c fude/zoom/circuit_step.c fude/zoom/circuit_play.c fude/zoom/limits.c fude/zoom/display.c fude/zoom/trim.c fude/zoom/nest.c fude/zoom/map.c fude/zoom/piece.c fude/zoom/sheet.c fude/zoom/stl.c fude/zoom/snap.c fude/zoom/cut.c fude/zoom/select.c fude/zoom/render.c fude/zoom/render_stroke.c fude/zoom/render_shape.c fude/zoom/render_text.c fude/zoom/render_symbol.c fude/zoom/bucket.c fude/zoom/connect.c fude/zoom/instrument.c fude/zoom/instrument_hands.c fude/zoom/instrument_pen.c fude/zoom/instrument_draw.c fude/zoom/units.c fude/zoom/pdf.c fude/zoom/dxf.c fude/zoom/zoom.c fude/zoom/shape.c fude/zoom/smooth.c fude/zoom/nav.c fude/zoom/fill.c fude/zoom/export.c fude/zoom/export_formats.c fude/zoom/formula.c fude/zoom/texcalc.c fude/drawing/base/theme.c fude/zoom/codec.c fude/zoom/index.c fude/zoom/scene.c fude/zoom/scene_history.c fude/zoom/scene_records.c fude/zoom/erase.c fude/zoom/zfile.c fude/zoom/video.c fude/zoom/handfind.c fude/zoom/symbol.c fude/zoom/symbol_diagrams.c fude/zoom/symbol_shapes.c fude/zoom/rich.c fude/zoom/paper.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c fude/drawing/base/redraw.c
spice||tests/spice/test.c tests/support/zoomdraw.c fude/zoom/props.c fude/zoom/logic.c fude/sim/sparse.c fude/sim/body.c fude/sim/def.c fude/sim/elab.c fude/sim/digital.c fude/sim/logic.c fude/sim/chips.c fude/zoom/calc.c fude/zoom/plot.c fude/zoom/chart.c fude/zoom/plan.c fude/zoom/mech.c fude/zoom/examples.c fude/zoom/examples_electronics.c fude/zoom/examples_mechanisms.c fude/zoom/examples_both.c fude/zoom/placer.c fude/zoom/custom.c fude/zoom/circuit.c fude/zoom/circuit_catalogue.c fude/zoom/circuit_draw.c fude/zoom/circuit_read.c fude/zoom/circuit_wires.c fude/zoom/circuit_build.c fude/zoom/circuit_blocks.c fude/zoom/circuit_stamp.c fude/zoom/circuit_solve.c fude/zoom/circuit_step.c fude/zoom/circuit_play.c fude/zoom/limits.c fude/zoom/display.c fude/zoom/trim.c fude/zoom/nest.c fude/zoom/map.c fude/zoom/piece.c fude/zoom/sheet.c fude/zoom/stl.c fude/zoom/snap.c fude/zoom/cut.c fude/zoom/select.c fude/zoom/bucket.c fude/zoom/connect.c fude/zoom/instrument.c fude/zoom/instrument_hands.c fude/zoom/instrument_pen.c fude/zoom/instrument_draw.c fude/zoom/units.c fude/zoom/pdf.c fude/zoom/dxf.c fude/zoom/zoom.c fude/zoom/shape.c fude/zoom/smooth.c fude/zoom/nav.c fude/zoom/fill.c fude/zoom/export.c fude/zoom/export_formats.c fude/zoom/rich.c fude/zoom/formula.c fude/zoom/texcalc.c fude/zoom/tex.c fude/zoom/mathfont.c fude/drawing/base/theme.c fude/zoom/codec.c fude/zoom/index.c fude/zoom/scene.c fude/zoom/scene_history.c fude/zoom/scene_records.c fude/zoom/erase.c fude/zoom/zfile.c fude/zoom/video.c fude/zoom/handfind.c fude/zoom/symbol.c fude/zoom/symbol_diagrams.c fude/zoom/symbol_shapes.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c fude/drawing/base/redraw.c
mech|-std=c17 -I@RDE/external/include/Box/Box2D/include -I@RDE/engine/src|tests/mech/test.c tests/support/physics.c tests/support/zoomdraw.c fude/zoom/mechrun.c fude/zoom/props.c fude/zoom/logic.c fude/sim/sparse.c fude/sim/body.c fude/sim/def.c fude/sim/elab.c fude/sim/digital.c fude/sim/logic.c fude/sim/chips.c fude/zoom/calc.c fude/zoom/plot.c fude/zoom/chart.c fude/zoom/plan.c fude/zoom/mech.c fude/zoom/examples.c fude/zoom/examples_electronics.c fude/zoom/examples_mechanisms.c fude/zoom/examples_both.c fude/zoom/placer.c fude/zoom/custom.c fude/zoom/coupling.c fude/zoom/circuit.c fude/zoom/circuit_catalogue.c fude/zoom/circuit_draw.c fude/zoom/circuit_read.c fude/zoom/circuit_wires.c fude/zoom/circuit_build.c fude/zoom/circuit_blocks.c fude/zoom/circuit_stamp.c fude/zoom/circuit_solve.c fude/zoom/circuit_step.c fude/zoom/circuit_play.c fude/zoom/limits.c fude/zoom/display.c fude/zoom/trim.c fude/zoom/nest.c fude/zoom/map.c fude/zoom/piece.c fude/zoom/sheet.c fude/zoom/stl.c fude/zoom/snap.c fude/zoom/cut.c fude/zoom/select.c fude/zoom/bucket.c fude/zoom/connect.c fude/zoom/instrument.c fude/zoom/instrument_hands.c fude/zoom/instrument_pen.c fude/zoom/instrument_draw.c fude/zoom/units.c fude/zoom/pdf.c fude/zoom/dxf.c fude/zoom/zoom.c fude/zoom/shape.c fude/zoom/smooth.c fude/zoom/nav.c fude/zoom/fill.c fude/zoom/export.c fude/zoom/export_formats.c fude/zoom/rich.c fude/zoom/formula.c fude/zoom/texcalc.c fude/zoom/tex.c fude/zoom/mathfont.c fude/drawing/base/theme.c fude/zoom/codec.c fude/zoom/index.c fude/zoom/scene.c fude/zoom/scene_history.c fude/zoom/scene_records.c fude/zoom/erase.c fude/zoom/zfile.c fude/zoom/video.c fude/zoom/handfind.c fude/zoom/symbol.c fude/zoom/symbol_diagrams.c fude/zoom/symbol_shapes.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c @BOX2D fude/drawing/base/redraw.c
units||tests/units/test.c fude/zoom/units.c fude/drawing/base/utf8.c tests/support/engine.c
pdf||tests/pdf/test.c fude/zoom/pdf.c fude/zoom/mathfont.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c
dxf||tests/dxf/test.c fude/zoom/dxf.c fude/drawing/base/kfile.c fude/drawing/base/utf8.c tests/support/engine.c
sim||tests/sim/test.c fude/sim/sparse.c fude/sim/body.c fude/sim/def.c fude/sim/elab.c fude/sim/digital.c fude/sim/logic.c fude/sim/chips.c tests/support/engine.c
graph||tests/graph/test.c fude/zoom/graph.c fude/zoom/graph_read.c fude/zoom/graph_order.c fude/zoom/graph_place.c fude/zoom/graph_layout.c fude/drawing/base/utf8.c tests/support/engine.c
wordexam|-include unistd.h -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/wordexam/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/study/screens/wordexam.c fude/study/models/vocab.c fude/study/models/review.c fude/study/models/charnote.c fude/study/services/speech.c fude/study/models/marks.c fude/drawing/widgets/scroll.c fude/study/handwriting/recognize.c fude/drawing/ink/mlkit.c fude/study/handwriting/score.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/study/chars/glyph.c fude/drawing/base/theme.c fude/drawing/base/draw.c tests/support/rich.c fude/drawing/base/text.c tests/support/text.c fude/lang/ja/romaji.c fude/lang/ja/lang.c tests/support/app.c fude/study/widgets/header.c fude/drawing/base/save.c fude/drawing/base/redraw.c
zh|@APP=hanzi|tests/zh/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/lang/zh/lang.c
ko|@APP=hangul -include drawing/ink/pagefile.h -include drawing/app/settings.h|tests/ko/test.c fude/drawing/base/utf8.c tests/support/engine.c fude/lang/ko/lang.c fude/lang/ko/hangul.c fude/drawing/base/text.c tests/support/text.c fude/study/handwriting/segment.c fude/study/handwriting/match.c fude/study/chars/catalog.c fude/study/chars/kanji.c fude/drawing/base/kfile.c fude/drawing/ink/ink.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/study/models/marks.c tests/support/rich.c fude/drawing/base/save.c
geometry||tests/geometry/test.c fude/sim/body.c tests/support/engine.c
docwork||tests/docwork/test.c fude/drawing/doc/worker.c fude/drawing/base/theme.c tests/support/engine.c fude/drawing/base/redraw.c
kit||tests/kit/test.c fude/drawing/widgets/kit.c fude/drawing/base/draw.c fude/drawing/base/theme.c fude/drawing/base/utf8.c fude/drawing/base/text.c tests/support/text.c tests/support/engine.c
redraw||tests/redraw/test.c fude/drawing/base/redraw.c fude/drawing/widgets/scroll.c fude/drawing/widgets/notice.c fude/drawing/base/clock.c fude/drawing/ink/canvas.c fude/drawing/base/theme.c fude/drawing/base/draw.c fude/drawing/base/utf8.c tests/support/engine.c tests/support/rich.c
'

echo "$SUITES" | while IFS='|' read -r name flags sources; do
    [ -z "$name" ] && continue
    if [ $# -gt 0 ]; then
        case " $* " in *" $name "*) ;; *) continue ;; esac
    fi
    dir=$OUT/$name
    mkdir -p "$dir"
    # The engine's set-up first: start-up code runs in link order (macOS ignores constructor priorities), and a
    # suite's own may already need the engine's allocator.
    files="$ROOT/tests/support/headless.c"
    for f in $sources; do
        case $f in
            @BOX2D) files="$files $RDE/external/include/Box/Box2D/src/*.c" ;;   # (Box2D, as RDE has it: the physics' suites)
            *) files="$files $ROOT/$f" ;;
        esac
    done
    flags=$(echo "$flags" | sed "s#@RDE#$RDE#g")
    # @CANVAS: a suite of the canvas layer finds a canvas app's strings (its text_ids.h), not Kana's: its folder goes
    # first on the include path, so its text_ids.h is the one found.
    first=""
    case "$flags" in *@CANVAS*) first="-I$ROOT/apps/sketching/src"; flags=$(echo "$flags" | sed "s#@CANVAS##g") ;; esac
    # @APP=<app>: a suite of another study app's language is built as that app. Its src/ goes first, as @CANVAS's
    # does; its strings are the ones read (FUDE_ROOT, tests/support/text.c); and it runs on its character data.
    app=kana
    case "$flags" in
        *@APP=*) app=$(echo "$flags" | sed 's#.*@APP=\([a-z]*\).*#\1#'); first="-I$ROOT/apps/$app/src"; flags=$(echo "$flags" | sed 's#@APP=[a-z]*##g') ;;
    esac
    root="-DFUDE_ROOT=\"$ROOT/apps/$app\""
    # $first, $CFLAGS, $root, $flags and $files split into words on purpose.
    if ! clang $first $CFLAGS $root $flags $files -o "$dir/$name" $LINK -lm > "$dir/build.log" 2>&1; then
        printf '%-10s BUILD FAILED (%s/build.log)\n' "$name" "$dir"
        grep -m5 "error" "$dir/build.log"
        echo "$name" >> "$OUT/failed"
        continue
    fi
    # Each suite in its own folder: they write their saves where they run.
    rm -rf "$dir/saves" "$dir/sheets"
    result=$(cd "$dir" && "./$name" "$ROOT/apps/$app/assets/data/characters.kana" 2>&1 | tail -1)
    printf '%-10s %s\n' "$name" "$result"
    case "$result" in *"ALL PASSED"*) ;; *) echo "$name" >> "$OUT/failed" ;; esac
done
if [ -f "$OUT/failed" ]; then
    echo "failed: $(tr '\n' ' ' < "$OUT/failed")"
    rm -f "$OUT/failed"
    exit 1
fi
echo "every suite passed"
