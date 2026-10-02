# Kana's architecture

How the code is put together, how to add to it, and which parts are Japanese —
the parts a Mandarin or Korean app built from this one would replace. Written
2026-10-02, when the app was restructured into these layers (every screen
looked the same before and after: see the checking below).

---

## The layers, and their folders

```
  kana.c                 the shell: what RDE calls (init, events, update, render, end)
  src/app/               the app: app (the screens' order, the pointer, the verbs) ·
                         ui (the retained UI) · page (writing on the page) · session
                         (saves, files out) · look (a developer's launch flags)
  src/screens/           the screens: viewer browse chart practice album check exam
                         stats scan vocabview wordexam welcome · screen.h
  src/widgets/           the widgets: kit row filterbar header notice toolbar
                         pagemenu side wordcard glyph draw scroll · icons.h
  ─ models: what is kept and worked on, knowing nothing of the screen ─
  src/ink/               the page's strokes, the lasso, the view, the canvases
  src/handwriting/       reading and scoring writing: match score segment recognize guide textink
  src/study/             what the learner keeps: vocab review marks examlog history charnote select sheet
  src/chars/             the character data and its index: kanji catalog
  src/base/              files, words, colours: kfile save backup text text_ids theme
  src/services/          the device's: ML Kit, text in photos, translation, speech (each a .c and an _ios.m)
  src/lang/ja/           Japanese's own: romaji wordsplit bake
  tests/                 a suite per module group (tests/README.md)
  tools/                 text/strings.py (every UI string), icons/bearings.py, mlkit/, store/
```

Each header is beside its source and is included by its folder (`#include
"widgets/row.h"`), so a file's includes show which layers it uses. The only
include path is `-I<project>/src`.

Each layer uses the ones under it, never the ones over it:

- **Models** keep what is learned and written, and know nothing of the screen.
  Most are tested on their own, without a window.
- **Widgets** are pieces of UI that know no screen. Retained ones are built on
  RDE's UI canvas with the kit: the toolbar, a row of buttons, the side panel,
  the word card. Immediate ones are drawn each frame: a glyph, a header, a card,
  text.
- **Screens** fill the screen over the page. Each draws itself, takes the
  pointer, and declares its rows of buttons: what they say, how they look now,
  and what they do.
- **The app** owns everything. It keeps the screens in order and hands each
  event and frame to the one on top. One screen reaches another only through
  the app's verbs (`kana_app_view`, `kana_app_practice`...), never directly.

## Where things are

| Folder | File | What it is |
|---|---|---|
| (root) | `kana.c` | RDE's callbacks; owns every instance; the HUD |
| `app/` | `app.h/.c` | `kana_app` (what the app holds), the screens' order and pointer, the verbs, Select mode's shared row |
| | `ui.h/.c` | the one UI canvas and the fonts; builds every widget and each screen's rows, bar and field; rebuilt when the language changes |
| | `page.h/.c` | the page under the screens: pen, finger (the hand), mouse, pinch, long press, its keys, its drawing |
| | `session.h/.c` | the autosave, the canvases, the study files, Your data's export and import, practice sheets |
| | `look.h/.c` | a developer's launch flags (`--shot`, `--press`...), `--perf`, the ML Kit probes |
| `screens/` | `screen.h` | the screen interface (`kana_screen`) and its adapter macros |
| `widgets/` | `kit.h/.c` | button looks, icons, fields, placement; `kana_kit_modal` (a card over a dimmed backdrop) |
| | `row.h/.c` | a row of buttons declared as data, with faces (the bottom bar of every screen, the page's menus) |
| | `filterbar.h/.c` | chips to filter and sort, a search field, toggles (Browse's bar) |
| | `header.h/.c` | a screen's header (badge, title, caption, count), its large title, the badge labels |
| | `notice.h/.c` | the line that says what an action did, a moment |
| | `toolbar.h/.c` | the floating bar of the page's tools, its palette and paper panel |
| | `pagemenu.h/.c` | the lasso selection's menu, the translation card, the long-press menu |
| | `side.h/.c` | the side panel, Settings, Licences, Your data, the canvases list |
| | `wordcard.h/.c` | a word into the vocabulary (and a list's name, a character's note) |
| | `glyph`, `draw`, `scroll` | characters drawn from their strokes (and animated); text, cards, chips; a scroller with inertia |

## A screen

A screen module keeps its state, draws itself and takes the pointer. At the end
of its `.c`, it fills a `kana_screen` (`KANA_<NAME>_SCREEN`). Use the adapters
in `screen.h` for the usual function shapes.

```c
const kana_screen KANA_STATS_SCREEN = {
    .name = "stats", .input = KANA_SCREEN_INPUT_POINT,
    .is_open = ..., .close = ...,            // Escape and Back
    .update = kana_stats_screen_update,      // its frame, and what it hands on
    .render = ..., .pointer_down/moved/up = ...,
    .rows = KANA_STATS_BUTTON_ROWS, .row_count = 1u, .row = ...,   // its rows of buttons
    .faces = ...,                            // how each button shows now (counts, greyed out, chosen)
    .field_hint = KANA_TEXT_COUNT,           // no field of its own
};
```

- **Its rows are data.** Each button is `{ text, icon, press, arg, look,
  counted, present }`. `press` gets the app and the screen. When a button only
  calls a function of the screen, use `KANA_ROW_CALL(name, type, fn)`. Each
  frame, `faces` says how the buttons show now: "Start 12", greyed out, chosen.
  The widget changes only what differs, so a screen keeps no "what is shown"
  state of its own.
- **What it hands on**, such as a character tapped or a word to practise, it
  either keeps for its update to pass to a verb, or does from a button with a
  verb. It never opens another screen directly.
- **Extras, declared too:**
  - `bar`: a filter bar across its top (Browse's)
  - `field_*`: a text field at the top right (Check's "I meant…")
  - `in_view`: the characters in view (what its Practice and Select's All take)
  - `resume`: what to do when it is on top again (the viewer replays)
  - `overlay`: drawn over what is under it (the welcome)

**Adding a screen:**
1. Write the module in `src/screens/`.
2. Fill its `kana_screen`.
3. Add it to `KANA_SCREEN_` in `app/app.h`, in its place in the stack.
4. Add it to the table in `kana.c` (`kana_screens_place`).
5. Add its `.c` to the four source lists in `COMMANDS.txt`.

The UI builds its rows, the app routes to it, and its row shows when it is on
top. Nothing else changes.

**The stack.** `KANA_SCREEN_` is the order, top first. The first open screen is
on top: it alone gets the pointer, the frame, Escape and its row. Under every
screen is the page. When a screen closes, the one under it comes back on top and
its `resume` runs.

**Input.** One pointer at a time for the screen on top (pen, finger or desktop
mouse), in `kana_app_screen_event`.
- A writing screen (`KANA_SCREEN_INPUT_WRITE`, Practice) takes a finger only
  with the hand on, and a pen takes over from it.
- Presses on the UI (`kana_ui_hit`) are the UI's.
- The page has its own input, with the finger-writing state machine
  (`page.c`).

## The UI, and rebuilding it

All retained widgets live on one canvas (`ui.c`) and are built from the app's
state. When the language changes, `kana_ui_follow_language` destroys the canvas
and builds everything again. The only state kept across a rebuild is what the
widgets do not own: the toolbar's tool and place, the side panel being open,
and the page menu's reading and card. Before, the rebuild copied a long list of
fields by hand. Two it missed (the Vocabulary screen and the word exam) lost
their rows after a language change. That class of bug is gone, because widgets
no longer hold pointers to screens: they reach them through the app.

## Checking a change without a device

A developer's build reads launch flags (`look.h`, `COMMANDS.txt`):

- `--size=744x1133` and a screen flag (`--browse`, `--viewer=6728`, `--exam`,
  `--vocab`...) open a screen in a given state.
- `--shot=FILE` writes a screenshot and quits.
- `--press=I,J,...` presses the buttons of the screen on top in turn, as taps
  would.
- `--language=N` switches the language mid-run.

`tests/run.sh` builds and runs the test suites (tests/README.md).

The restructure was checked this way. 39 screens were shot before and after and
compared pixel by pixel. 9 of them animate or pick at random; of the other 30,
29 came out identical, and one changed on purpose (Translate with Google's
chosen icon now fills, as every other toggle's does). 13 pressed sequences were
checked as well.

## The language layer: what is Japanese

Everything above is language-neutral except the parts below. A Mandarin or
Korean app keeps the engine and the layers, and replaces these.

| What | Where | Japanese now | Mandarin | Korean |
|---|---|---|---|---|
| Character data | `lang/ja/bake.c` → `assets/data/characters.kana` (format: `chars/kanji.h`) | KanjiVG strokes, KANJIDIC2, JMdict words, Tatoeba sentences, JLPT lists | Make Me a Hanzi (strokes), CC-CEDICT (words), Tatoeba, HSK lists | jamo strokes (a small set, drawable by hand), a Korean dictionary, Tatoeba, TOPIK lists |
| Readings | `chars/kanji.h` (on and kun), `screens/viewer.c` (音 訓 lines), `chars/catalog.c` (search by reading) | on / kun | pinyin with tones | Revised Romanization; hanja readings optional |
| Script tables | `lang/ja/romaji.c` (gojūon, romaji ↔ hiragana), `screens/chart.c` (the kana chart screen) | kana | none (pinyin input instead) | jamo and syllable composition: a jamo chart screen |
| Levels and groups | `screens/browse.c` (filter chips), `screens/exam.c` (sources), `screens/stats.c` (groups) | JLPT N5–N1, hiragana, katakana, kanji | HSK 1–6, simplified / traditional | TOPIK, jamo, syllables |
| Words in text | `lang/ja/wordsplit.c` (longest match + conjugation), `study/vocab.c` (a character's first reading) | Japanese conjugation | longest match, no conjugation | particles and conjugation (different rules) |
| On the device | `services/`: `mlkit_ios.m` (ink `ja`), `textscan_ios.m` (Japanese text model), `translate` (from `ja`), `speech_ios.m` (ja-JP voice) | `ja` | `zh-Hani-CN` / `zh-Hani-TW`, the Chinese text model, zh-CN | `ko`, the Korean text model, ko-KR |
| Fonts | `app/ui.c` | Noto Sans JP | Noto Sans SC / TC | Noto Sans KR |
| Words in the UI | `tools/text/strings.py`, the badges in `widgets/header.c`'s callers (試 語 音 訓 部 似 記) | | | |

**The next step, when the second app starts.** Gather these behind one
interface, `lang.h`, with a Japanese implementation:
- the language codes and voice
- the font paths
- the reading kinds and their labels
- the levels
- the script chart (or none)
- the word splitter
- romanization to script

Then the shared folders (everything but `lang/`) are what both apps build
from. Each app keeps its `lang/xx/`, its data bake, its strings and its store
texts. Do the language step only then: with one language, an interface is
guesswork. The tables above are the list of what goes behind it.
