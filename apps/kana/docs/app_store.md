# Kana on the App Store: privacy, licences and permissions

What the app ships and declares, and what has to be filled in outside the app.
Written 2026-09-30 for ML Kit's handwriting; brought up to date 2026-10-02 for
everything since (text from a photo, translation, the camera, reading aloud,
vocabulary). The store texts and screenshots are in `store/`; the two web pages
App Store Connect asks for are made into `site/` by the repository's
`tools/store/site.py`, which writes every app's (2026-10-13). Checked against
the code again on 2026-10-15, for Kana 0.5.4 (`src/version.h`). The store
texts, the price question and the review notes are `store/listing.md`'s; the
privacy and rating answers for every app are `docs/store_privacy_answers.md`'s.
This file keeps what is Kana's alone: what it ships, declares and credits.

---

## What leaves the device

**Kana itself sends nothing.** Pages, notes, practice and exam history, reviews,
vocabulary and lists, character notes and settings are saved on the device
only (and are part of the device's own backups, which Apple handles). Export
(Settings › Your data) writes one file and hands it to the share sheet: it goes
only where the learner sends it.

**Google ML Kit** runs on the device for three things — handwriting
(MLKitDigitalInkRecognition: Check, Copy as text, Save word, drawing to search,
exams), printed text (MLKitTextRecognitionJapanese: Text from a photo, and the
pages of a scanned or photographed document, read once so that Search and the
lasso find their words) and translation (MLKitTranslate: Translate with Google).
The writing, the photos, the pages and the text translated are read on the
device; none of them is sent. While ML Kit is **on** (Settings › Handwriting ›
Read with Google ML Kit, on by default):

- It **downloads its models** from Google: the handwriting model (about 20 MB)
  as soon as Kana starts with ML Kit on, so at the first launch
  (`fude/study/app/shell.c`, on iOS and Android alike), not at its first use; a
  translation model (about 30 MB per language, Japanese's too) the first time a
  translation is asked for; text recognition's Japanese model ships inside the
  app and is never downloaded.
- It **sends Google usage and diagnostics data** — what Google's
  [iOS data disclosure](https://developers.google.com/ml-kit/ios-data-disclosure)
  and its privacy manifests list:
  - device information (manufacturer, model, OS version);
  - the app's bundle ID and version;
  - a per-installation identifier ("not intended to uniquely identify a user
    or physical device");
  - performance metrics (latency);
  - events (initialisations, model downloads) and error codes;
  - the configured language.
- It is used "for diagnostics and usage analytics": not linked to the user,
  not used for tracking.

Switched **off**, ML Kit is never called: nothing is downloaded, nothing is
sent, Kana reads handwriting on its own (less accurately: 52/60 characters
against ML Kit's 60/60, measured on 2026-09-30 on handwritten samples that
`data/samples/` no longer holds), and Text from a photo and Translate with
Google say they need it. (On Android, Lectures › Scan pages is
Google's document scanner, which runs in Play services: the switch does not
cover it, and the policy says so.)

**Reading aloud** uses iPadOS's own voices (AVSpeechSynthesizer), on the device.

**Crash reports** stay on the device too. After a crash, the next launch
offers an email draft to rde.apps.support@gmail.com with a short report in it
(`fude/drawing/base/crash.c`, `fude/drawing/app/crashcard.c`), which the
learner reads and sends from their own mail app, or not; either way the report
is deleted. Nothing is uploaded.

## Permissions

- **Camera** (`NSCameraUsageDescription`): Text from a photo, live; and
  Lectures › Scan pages (the system's document camera, VisionKit). Asked the
  first time it is used; the reason in each of Kana's five languages
  (`platform/ios/localized/*.lproj/InfoPlist.strings`).
- **Photos: none.** The system's photo picker (PHPicker) hands over only the
  photos chosen (Text from a photo: one; Lectures › From Photos: any number);
  no library access, no prompt.
- No microphone, location, contacts or tracking (`NSPrivacyTracking` is false
  everywhere, so no App Tracking Transparency prompt).

## Bundled content

- **Lectures:** First Year Japanese I – Supplemental material and worksheets
  (Yoko Sato, Mt Hood Community College, CC BY 4.0), Kana's edition: the stock
  pictures not under its licence left out (pages 1, 2, 21), a title page with
  the credit and the changes. CC BY 4.0 allows selling it inside the app; it
  forbids locking it (no DRM, no terms that restrict it). The credit is on its
  card and its first page, and in Settings › Licences › Lectures
  (`assets/lectures/LICENSE-lectures.txt`). How it was made: Kana's own
  `tools/lectures/make_jpn101.py`.
- **Lectures:** Wikivoyage's Japanese phrasebook in English, Spanish, French
  and Portuguese (CC BY-SA 4.0, by Wikivoyage's contributors), text only, laid
  out as PDFs; the Library shows the one in the app's language. ShareAlike
  covers these PDFs, not the app (they are bundled beside it). Their credits,
  revisions and changes: their first pages and `LICENSE-lectures.txt`. How they
  were made: the repository's `tools/lectures/phrasebook.py` (and
  `html2pdf.swift`), which Hanzi's and Hangul's phrasebooks are made with too.

## Privacy manifests (in the app)

- **The app's own** `PrivacyInfo.xcprivacy` (the engine's template): file
  timestamps (C617.1), system boot time (35F9.1), user defaults (CA92.1). Kana
  adds nothing to it.
- **One per SDK**, in a bundle beside the executable, named as CocoaPods names
  them (the code is linked statically, so there is no framework to carry them;
  Apple's privacy report reads every manifest in the app): `MLKitCommon`,
  `MLKitDigitalInkRecognition`, `MLKitTextRecognitionCommon`, `MLKitTranslate`,
  `MLKitNaturalLanguage`, `GoogleDataTransport`, `GoogleUtilities`,
  `GTMSessionFetcher_Core`, `FBLPromises`, `nanopb`, `GoogleToolboxForMac`,
  `GoogleToolboxForMac_Logger` (each `<name>_Privacy.bundle`).
  `tools/mlkit/setup.py` builds them into `third_party/mlkit/bundles/`, and the
  builder copies them in (`--ios_bundle`).

## App Store Connect › App Privacy: the answers

**Do you or your third-party partners collect data?** Yes (Google ML Kit).
**Tracking:** No.

For each of these types:
- "Linked to the user's identity": **No**.
- "Used for tracking": **No**.

| Category | Data type | Purposes |
|---|---|---|
| Identifiers | Device ID | Analytics, App Functionality |
| Usage Data | Product Interaction | Analytics, App Functionality |
| Diagnostics | Performance Data | Analytics, App Functionality |
| Diagnostics | Other Diagnostic Data | Analytics |
| User Content | Other User Content | Analytics, App Functionality |
| Other Data | Other Data Types | Analytics, App Functionality |

This is exactly what ML Kit's manifests declare — checked again on 2026-10-02,
and on 2026-10-13 for every app: `MLKitCommon`, `MLKitDigitalInkRecognition`,
`MLKitTextRecognitionCommon`, `MLKitTranslate` and `MLKitNaturalLanguage`
declare the same six, and `GoogleDataTransport` Other Diagnostic Data alone.
If a newer ML Kit changes them, re-read the bundles' `PrivacyInfo.xcprivacy`
after `setup.py`. Why these six and not fewer, what is left undeclared (crash
reports among it) and why, and Google Play's Data safety answers:
`docs/store_privacy_answers.md` §2 and §3.

## Privacy policy and support pages (URLs required)

Written: `python3 tools/store/site.py kana` (the repository's tool, every app's)
makes `site/privacy.html` (the policy) and `site/index.html` (help and
support), each in the five languages (the reader's, or a choice at the top),
with the menu names read from the app's own strings. `CONTACT` at the top of
the script is the address both show (rde.apps.support@gmail.com); run the
script again after changing it. Hosted on GitHub Pages, one repository for every
app: `python3 tools/store/site.py --pages DIR` writes `DIR/<app>/` for each app
and an index of them; push DIR's contents, Settings › Pages › Deploy from a
branch, `main`, `/ (root)`; a minute later Kana's are at
`https://<user>.github.io/<repository>/kana/`. Then App Store Connect gets the
Privacy Policy URL and the Support URL (both in `store/listing.md`, Shared), and
the support page's address goes in Kana's `faq_url` (`fude/drawing/app/info.h`),
so that the side panel's FAQ link appears: until then it has none.

The policy says what this file says, for iPad and Android: everything stays on
the device, and in the device's own backups; Export; ML Kit's features, its
downloads and its diagnostics (Google's data disclosures, privacy policy and ML
Kit terms linked); the switch that stops it all; the camera, the document
scanner (Apple's, or Google's on Android) and the photo pickers; reading aloud;
crash reports (only an email the learner sends); links; the stores' own data;
children; changes; contact.

## Licences and attribution (in the app)

**Settings › About** credits KanjiVG, KANJIDIC2, JMdict, Tatoeba, the JLPT lists
and Google ML Kit in a few lines, with the note on what ML Kit sends. It points
to Licences for the rest: the full attribution KanjiVG, EDRDG and Tatoeba
require is its Data document, and Google's terms head the ML Kit one. Printed
practice sheets carry KanjiVG's credit at the foot of every page.

**Settings › Licences** shows every licence in full, one document at a time:

| Document | Contents | File |
|---|---|---|
| Data | KanjiVG, KANJIDIC2, JMdict, JLPT, Tatoeba (CC BY 2.0 FR) | `assets/data/LICENSE-data.txt` |
| Fonts | Roboto (Apache 2.0), Noto Sans JP (SIL OFL 1.1), Phosphor icons (MIT) | `assets/fonts/LICENSE-*.txt` |
| Libraries | every library built into Kana, each with its licence, by platform: the engine's (SDL3, FreeType, HarfBuzz, FriBidi, Box2D, LuaJIT, miniaudio, stb and the rest), Android's (AndroidX, PdfBox-Android, libc++) and ML Kit's iOS pods (GTMSessionFetcher, GoogleDataTransport, GoogleUtilities, GoogleToolboxForMac, Promises, nanopb, SSZipArchive with minizip-ng) | `assets/licenses/libraries.txt` |
| ML Kit | Google's NOTICES for the software inside ML Kit, the translation disclaimer first | `assets/licenses/ml-kit-notices.txt` |
| Lectures | the lectures' licences, credits and what Kana changed (above) | `assets/lectures/LICENSE-lectures.txt` |

`tools/mlkit/setup.py` writes `ml-kit-notices.txt` and, at its end, runs
`tools/licences/build.py`, which writes `libraries.txt` (and every other app's)
from the licence texts in `tools/licences/texts/`; `build.py --check` says
whether a file is out of date. Commit them; they ship with the app.

## Still open before submitting

- [ ] **The toolchain.** Uploads must be built with a current Xcode and iOS SDK
      (this Mac had Xcode 15.3 / iOS 17.4 SDK on macOS 14.4: too old). macOS
      first, then Xcode, then the engine and Kana rebuilt and retried on the iPad
      — the Pencil's 17.5 double-tap code compiles in then.
- [x] **Writing without an Apple Pencil**: the toolbar's hand — one finger
      writes, two move the page; on for a new install, off by itself when a Pencil
      writes (and back on by hand).
- [x] CONTACT set (rde.apps.support@gmail.com). [ ] **The pages hosted** on GitHub
      Pages, both URLs in App Store Connect.
- [ ] **The App Store Connect record**: its Apple ID goes in `KANA_APP_STORE_ID`
      (apps/kana/src/version.h) so Rate opens the write-review page; then the texts
      (`store/listing.md`), the screenshots (iPad 13-inch: `store/framed/`,
      `store/README.md`), the age rating (the answers drafted in
      `docs/store_privacy_answers.md` §4), the price. The price is not decided
      (`store/listing.md`, Price); the app's own Why it works offline already
      says "One purchase", so it must change if Kana is not paid once.
- [x] Export compliance: `ITSAppUsesNonExemptEncryption` false (HTTPS through
      Apple's networking only).
- [x] A release build: optimised (-O3), the HUD and launch arguments ignored.
- [x] Android: ML Kit's .aar through the builder's `--android_dep`, and the
      Android sides of handwriting (drawing/ink/mlkit_android.c), text in photos,
      translation and speech (study/services/*_android.c) and document import
      (drawing/app/import_android.c), all written. What remains for the stores is
      in docs/release_checklist.md.
