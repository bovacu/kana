# Kana on the App Store

Everything App Store Connect asks for, in one place:

- `listing.md`: the texts in the five languages (name, subtitle, promotional
  text, keywords, description), plus the shared answers: category, price, age
  rating and the App Review notes.
- `framed/<lang>/1.png … 10.png`: the ten iPad screenshots, captioned in
  English, Spanish, Portuguese, Japanese and French, at 2048 × 2732. They are
  ready to drag into each localization's **iPad 13-inch** slot, in order.
- `screenshots/`: the same ten without frames, and `spare/` with six more.
  #1 and #2 are from the iPad mini, the rest from the iPad Pro 12.9-inch
  Simulator.

Privacy, permissions and the App Privacy answers: `../docs/app_store.md`.

## The ten screenshots

| # | File | Shows | Caption (English) |
|---|---|---|---|
| 1 | `01-write-translate.png` | Handwritten sentences, 日本語 circled and translated, with the translation enlarged | Write Japanese by hand. Kana reads it and **translates it**. |
| 2 | `02-translate.png` | Text from a photo: a textbook cover, its 16 lines found and translated | Point the camera at Japanese. **Read it in your language.** |
| 3 | `03-kanji.png` | 用: stroke order playing, readings, words, an example | Every kanji writes itself, **stroke by stroke** |
| 4 | `04-lectures.png` | A lesson: marker highlights, printed text circled (Copy as text, Save word) | **Real Japanese lessons** to read, highlight and write on |
| 5 | `05-practice.png` | Guided practice of 冂 | Practice with guidance, until you write it **from memory** |
| 6 | `06-draw-search.png` | 冂 drawn, its 60 best matches | Can't type a kanji? **Draw it** |
| 7 | `07-vocabulary.png` | 36 words in lists, with their reviews | Your words come back **right before you forget them** |
| 8 | `08-kanji-list.png` | The kanji by JLPT level | **6,412 kanji** and all the kana, by JLPT level |
| 9 | `09-kana.png` | The hiragana and katakana charts | Hiragana and katakana, **from the very first stroke** |
| 10 | `10-statistics.png` | Statistics | Watch your Japanese **grow**, day after day |

The bold words are drawn in yellow. The first three are the ones the store
shows in search results.

Every shot has its status bar (clock, battery) cut off, so the iPad's and the
Simulator's look alike. #1 also shows its translation card enlarged over the
page (`CALLOUTS` in the script), so it reads even as a thumbnail.

Spares in `screenshots/spare/`, all with captions ready except `menu.png`:

- `page.png`: the sentences alone. Its caption: "Write Japanese by hand.
  Kana reads it."
- `select.png`: 毎朝 circled, without Translate.
- `translate-lesson.png`: こうえん in a lesson translated as "This". Google
  got it wrong (it means park or lecture), so leave it out.
- `album.png`, `library.png` and `menu.png`.
- A caption is also ready for an "into-japanese" shot: Vocabulary › Translate
  with Google, a sentence in English turned into Japanese.

To swap a shot, give it the place and caption in its name. For example, to
put the album at 9, move `09-kana.png` to `spare/`, copy `spare/album.png` to
`09-album.png`, and frame again (below).

The Vocabulary shot (#7) was taken before Vocabulary had its search field.

## Framing the screenshots again

After changing a shot, or a caption in `tools/store/screenshots.py`:

```
python3 -m pip install pillow      # once
python3 apps/kana/tools/store/screenshots.py apps/kana/store/screenshots apps/kana/store/framed
```

This rewrites `framed/` for all five languages. The captions are in the
script, keyed by the word in each file's name; the words inside `[[ ]]` are
the yellow ones. The screenshots show the
English app in every language. For a localized look, put shots of the app in
that language in `screenshots/es/`, `screenshots/ja/` and so on (same names);
each language then uses its own.

`framed/` is about 31 MB and can always be made again, so it doesn't have to
be committed.

## Before submitting

The open items are in `../docs/app_store.md` under "Still open before
submitting". They are: a current Xcode, the privacy and support pages hosted,
the App Store Connect record (then its Apple ID in `KANA_APP_STORE_ID`), and
the price.
