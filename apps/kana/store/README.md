# Kana on the App Store

Everything App Store Connect asks for, in one place:

- `listing.md`: the texts in the five languages (name, subtitle, promotional
  text, keywords, description), plus the shared answers: category, price, age
  rating and the App Review notes.
- `framed/<lang>/1.png … 10.png`: the ten iPad screenshots, captioned in
  English, Spanish, Portuguese, Japanese and French, at 2048 × 2732. They are
  ready to drag into each localization's **iPad 13-inch** slot, in order.
- `screenshots/`: the same ten without frames (from the iPad Pro 12.9-inch
  Simulator), and `spare/` with three more.

Privacy, permissions and the App Privacy answers: `../docs/app_store.md`.

## The ten screenshots

| # | File | Shows | Caption (English) |
|---|---|---|---|
| 1 | `01-page.png` | Three handwritten sentences on the page | Write Japanese by hand. Kana reads it. |
| 2 | `02-kanji.png` | 用: stroke order playing, readings, words, an example | Every kanji writes itself, stroke by stroke |
| 3 | `03-lectures.png` | A lesson: marker highlights, printed text circled (Copy as text, Save word) | Real Japanese lessons to read, highlight and write on |
| 4 | `04-select.png` | 毎朝 circled: Cut, Copy, Copy as text, Save word, Check | Circle a word: check it, copy it as text, save it |
| 5 | `05-practice.png` | Guided practice of 冂 | Practice with guidance, until you write it from memory |
| 6 | `06-draw-search.png` | 冂 drawn, its 60 best matches | Can't type a kanji? Draw it |
| 7 | `07-vocabulary.png` | 36 words in lists, with their reviews | Your words come back right before you forget them |
| 8 | `08-kanji-list.png` | The kanji by JLPT level | 6,412 kanji and all the kana, by JLPT level |
| 9 | `09-kana.png` | The hiragana and katakana charts | Hiragana and katakana, from the very first stroke |
| 10 | `10-statistics.png` | Statistics | Watch your Japanese grow, day after day |

The first three are the ones the store shows in search results.

Spares in `screenshots/spare/`: `album.png`, `library.png` (captions ready:
"album", "library") and `menu.png` (no caption).

## Showing the translation

The Simulator has no ML Kit, so it can't show Text from a photo or Translate
with Google. Your iPad mini can. Its screenshots work here: the framing tool
scales any iPad's screenshot into the 13-inch frame. Captions are ready for
two of them:

- **translate**: Text from a photo with its lines translated ("Point the
  camera at Japanese. Read it in your language.")
- **into-japanese**: Vocabulary › Translate with Google, a sentence in
  English turned into Japanese ("Say it in your language, write it in
  Japanese").

To put one in, for example in place of `09-kana.png`:

1. Take the screenshot on the iPad (top button + volume up) and AirDrop it to
   the Mac.
2. Move `09-kana.png` to `spare/`.
3. Save the new shot as `screenshots/09-translate.png`.
4. Frame the shots again (below).

Its name sets both its place (09) and its caption (translate). A good set
would put **translate** at 3 and move the lessons to 4. The first three show
in search, and translation is the one thing the description has to explain
in words today.

## Framing the screenshots again

After changing a shot, or a caption in `tools/store/screenshots.py`:

```
python3 -m pip install pillow      # once
python3 apps/kana/tools/store/screenshots.py apps/kana/store/screenshots apps/kana/store/framed
```

This rewrites `framed/` for all five languages. The captions are in the
script, keyed by the word in each file's name. The screenshots show the
English app in every language. For a localized look, put shots of the app in
that language in `screenshots/es/`, `screenshots/ja/` and so on (same names);
each language then uses its own.

`framed/` is about 28 MB and can always be made again, so it doesn't have to
be committed.

## Before submitting

The open items are in `../docs/app_store.md` under "Still open before
submitting". They are: a current Xcode, the privacy and support pages hosted,
the App Store Connect record (then its Apple ID in `KANA_APP_STORE_ID`), and
the price.
