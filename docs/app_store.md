# Kana on the App Store: privacy, licences and permissions

What the app ships and declares since Google ML Kit reads handwriting in
Check, and what still has to be filled in outside the app. Written
2026-09-30, for ML Kit 8.0 (MLKitDigitalInkRecognition), GoogleMLKit 9.0.

---

## What leaves the device

**Kana itself sends nothing.** Pages, notes, practice history and settings are
saved on the device only.

**Google ML Kit** (handwriting recognition in Check) runs on the device, and
the writing is recognised there. While it is **on** (Settings › Handwriting, on
by default):

- The first time, it **downloads the Japanese model** (about 20 MB) from Google.
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
sent, and Kana reads handwriting on its own (less accurately: 52/60 characters
against ML Kit's 60/60 on `data/samples/`).

## Permissions: none

No camera, microphone, photos, location or contacts, so there are no usage
strings in Info.plist and no prompts. There is no tracking, so there is no
App Tracking Transparency prompt either (`NSPrivacyTracking` is false
everywhere).

## Privacy manifests (in the app)

- **The app's own** `PrivacyInfo.xcprivacy` (the engine's template): file
  timestamps, system boot time, user defaults. Kana adds nothing to it.
- **One per SDK**, in a bundle beside the executable, named as CocoaPods names
  them. The code is linked statically, so there is no framework to carry them.
  Apple's privacy report reads every manifest in the app:
  `MLKitCommon_Privacy`, `MLKitDigitalInkRecognition_Privacy`,
  `GoogleDataTransport_Privacy`, `GoogleUtilities_Privacy`,
  `GTMSessionFetcher_Core_Privacy`, `FBLPromises_Privacy`, `nanopb_Privacy`,
  `GoogleToolboxForMac_Privacy`, `GoogleToolboxForMac_Logger_Privacy`,
  `SSZipArchive`.
  `tools/mlkit/setup.py` builds them into `third_party/mlkit/bundles/`, and
  the builder copies them in (`--ios_bundle`).

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

This is exactly what ML Kit's own manifests declare. If a newer ML Kit
changes them, re-read `MLKit*.framework/PrivacyInfo.xcprivacy` after
`setup.py`.

## Privacy policy (a URL is required)

Still to write and host. It must say:
- Kana keeps everything on the device.
- Handwriting recognition uses Google ML Kit, which sends Google the
  diagnostics data above. Link Google's
  [privacy policy](https://policies.google.com/privacy) and the
  [ML Kit terms](https://developers.google.com/ml-kit/terms).
- It can be switched off in Settings › Handwriting.
- A contact address.

## Licences and attribution (in the app)

**Settings › About** credits KanjiVG, KANJIDIC2, JMdict and the JLPT lists and Google
ML Kit in a few lines, with the note on what ML Kit sends. It points to
Licences for the rest: the full attribution KanjiVG and EDRDG require is its
Data document, and Google's terms head the ML Kit one.

**Settings › Licences** shows every licence in full, one document at a time:

| Document | Contents | File |
|---|---|---|
| Data | KanjiVG, KANJIDIC2, JMdict, JLPT | `assets/data/LICENSE-data.txt` |
| Fonts | Roboto (Apache 2.0), Noto Sans JP (SIL OFL 1.1), Phosphor icons (MIT) | `assets/fonts/LICENSE-*.txt` |
| Libraries | GTMSessionFetcher, GoogleDataTransport, GoogleUtilities, GoogleToolboxForMac, Promises (Apache 2.0); nanopb, minizip-ng (zlib); SSZipArchive (MIT) | `assets/licenses/libraries.txt` |
| ML Kit | Google's NOTICES for the software inside ML Kit (~25,000 lines) | `assets/licenses/ml-kit-notices.txt` |

`setup.py` regenerates both files in `assets/licenses/`. Commit them; they
ship with the app.

## Still open before submitting

- [ ] The privacy policy page (above), and its URL in App Store Connect.
- [ ] Export compliance: the only encryption is HTTPS through Apple's
      networking (ML Kit downloads with NSURLSession), which is normally
      exempt. Confirm, then set `ITSAppUsesNonExemptEncryption` to false (a
      builder Info.plist key).
- [ ] A release build: optimised and stripped. The debug IPA is 26 MB, most of
      it ML Kit and debug symbols.
- [ ] Android: ML Kit's .aar through the builder's `--android_dep`, separately.
