# Kana on the App Store: privacy, licences and permissions

What the app ships and declares, and what has to be filled in outside the app.
Written 2026-09-30 for ML Kit's handwriting; brought up to date 2026-10-02 for
everything since (text from a photo, translation, the camera, reading aloud,
vocabulary). The store texts are in `docs/store/listing.md`; the two web pages
App Store Connect asks for are made by `tools/store/site.py` into `site/`.

---

## What leaves the device

**Kana itself sends nothing.** Pages, notes, practice and exam history, reviews,
vocabulary and lists, character notes and settings are saved on the device
only (and are part of the device's own backups, which Apple handles). Export
(Settings › Your data) writes one file and hands it to the share sheet: it goes
only where the learner sends it.

**Google ML Kit** runs on the device for three things — handwriting
(MLKitDigitalInkRecognition: Check, Copy as text, Save word, drawing to search),
text in photos (MLKitTextRecognitionJapanese: Text from a photo) and translation
(MLKitTranslate: Translate with Google). The writing, the photos and the text
translated are read on the device; none of them is sent. While ML Kit is **on**
(Settings › Handwriting › Read with Google ML Kit, on by default):

- The first time each is used, it **downloads its model** from Google:
  handwriting about 20 MB; translation about 30 MB per target language (and
  Japanese's); text recognition's Japanese model ships inside the app.
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
against ML Kit's 60/60 on `data/samples/`), and Text from a photo and
Translate with Google say they need it.

**Reading aloud** uses iPadOS's own voices (AVSpeechSynthesizer), on the device.

## Permissions

- **Camera** (`NSCameraUsageDescription`): Text from a photo, live. Asked the
  first time it is used; the reason in each of Kana's five languages
  (`platform/ios/localized/*.lproj/InfoPlist.strings`).
- **Photos: none.** The system's photo picker (PHPicker) hands over only the
  photo chosen; no library access, no prompt.
- No microphone, location, contacts or tracking (`NSPrivacyTracking` is false
  everywhere, so no App Tracking Transparency prompt).

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

This is exactly what ML Kit's manifests declare — checked again on 2026-10-02:
`MLKitCommon`, `MLKitDigitalInkRecognition`, `MLKitTextRecognitionCommon`,
`MLKitTranslate` and `MLKitNaturalLanguage` declare the same six, and
`GoogleDataTransport` Other Diagnostic Data alone. If a newer ML Kit changes
them, re-read the bundles' `PrivacyInfo.xcprivacy` after `setup.py`.

## Privacy policy and support pages (URLs required)

Written: `python3 tools/store/site.py` makes `site/privacy.html` (the policy)
and `site/index.html` (help and support), each in the five languages (the
reader's, or a choice at the top). `CONTACT` at the top of the script is the
address both show (rde.apps.support@gmail.com); run the script again after
changing it. Hosted on GitHub Pages: a public repository whose root holds the
two files (or a `docs/` folder with them), Settings › Pages › Deploy from a
branch, `main`, `/ (root)` or `/docs`; a minute later they are at
`https://<user>.github.io/<repository>/`. Then give App Store Connect:
- **Privacy Policy URL**: `…/privacy.html`
- **Support URL**: `…/` (index.html)

The policy says what this file says: everything stays on the device; ML Kit's
features, its downloads and its diagnostics (Google's privacy policy and the ML
Kit terms linked); the switch that stops it all; the camera and the photo
picker; reading aloud; the App Store's own data; children; changes; contact.

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
| Libraries | GTMSessionFetcher, GoogleDataTransport, GoogleUtilities, GoogleToolboxForMac, Promises (Apache 2.0); nanopb, minizip-ng (zlib); SSZipArchive (MIT) | `assets/licenses/libraries.txt` |
| ML Kit | Google's NOTICES for the software inside ML Kit, the translation disclaimer first | `assets/licenses/ml-kit-notices.txt` |

`setup.py` regenerates both files in `assets/licenses/`. Commit them; they
ship with the app.

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
      (include/version.h) so Rate opens the write-review page; then the texts
      (`docs/store/listing.md`), the screenshots (iPad 13-inch:
      `tools/store/screenshots.py`), the age rating, the price.
- [x] Export compliance: `ITSAppUsesNonExemptEncryption` false (HTTPS through
      Apple's networking only).
- [x] A release build: optimised (-O3), the HUD and launch arguments ignored.
- [ ] Android: ML Kit's .aar through the builder's `--android_dep`; the
      translation, speech and document-import sides still to write.
