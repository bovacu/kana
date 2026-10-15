# Release checklist: every app, App Store and Google Play

Written 2026-10-12 from a full audit of the repository, the built APKs and the store requirements.
Apple's developer certificate is left out on purpose, as agreed. Tick items here as they are done.

The apps: **Kana, Hanzi, Hangul, Thai, Hindi, Arabic** (the study apps) and **InfiNote, InfiniWorkshop** (the canvas
apps; Sketching is the developer build and is not released).

---

## 1. Where each app stands

**How close each is to submission** (refreshed 2026-10-15). Two gates, both needed before any upload:
the code certification (`docs/quality_plan.md`: nothing ships before it) and the store side (this file). The
percentages are my estimate of the work done on each, by effort, not by count of items. The apps' features are not
in them: every app runs complete on the tablet today. Your accounts and Apple's certificate are not in them either.

| | Code certified | Store side | Overall |
|---|---|---|---|
| Kana | ~90% | ~70% | ~82% |
| Hanzi, Hangul | ~90% | ~50% | ~72% |
| Thai, Hindi, Arabic | ~90% | ~45% | ~70% |
| InfiNote, InfiniWorkshop | ~85% | ~40% | ~65% |

- The study apps' code (`drawing`, `study`, `lang`): every module documented, no C library, no compiler warning, no
  layer crossed, no app named in shared code, no function over 150 lines, `segment.c`'s reading model in the
  language (2.10), and every reported bug closed (`docs/bug_log.md`). Left: the performance pass (Phase 4) and the
  design notes (Phase 5).
- The canvas apps' own code (`zoom`, `sim`): every file documented; `page.c`, `circuit.c` and the other long files
  split by domain, each app's own small shell (3.3), the remaining containers and shared helpers (3.5, 3.6), and
  every reported bug closed. Left: the performance pass (Phase 4) and the design notes (Phase 5).
- The store side: pages and texts written for every app, and the App Bundles build; left are the pictures
  (screenshots, Play's graphics) and what only you can do (§2).

| | Kana | Hanzi | Hangul | Thai | Hindi | Arabic | InfiNote | InfiniWorkshop |
|---|---|---|---|---|---|---|---|---|
| FAQ + privacy pages | written, not hosted | written, not hosted | written, not hosted | written, not hosted | written, not hosted | written, not hosted | written, not hosted | written, not hosted |
| Store texts (App Store), `store/listing.md` | drafted | drafted | drafted | drafted | drafted | drafted | drafted | drafted |
| Store texts (Google Play) | drafted | drafted | drafted | drafted | drafted | drafted | drafted | drafted |
| Screenshots | iPad 13" only | – | – | – | – | – | – | – |
| Licences complete | yes | yes | yes | yes | yes | yes | yes | yes |
| iOS localizations right | yes | yes | yes | yes | yes | yes | yes | yes |
| iOS build exists | yes | yes | yes | yes | yes | yes | Simulator only | Simulator only |
| Android .aab (Play needs it) | builds, checked | builds | builds | builds | builds | builds | builds | builds |
| Signing key backed up | yes | yes | yes | yes* | yes* | yes* | yes* | yes* |

The App Bundle (2026-10-14): `zsh tools/build/app.sh <app> bundle` (its project file's `bundle` profile, since
2026-10-15) makes `build/<app>-android/game.aab`, its version code from the app's `version.h` (major × 10000 + minor × 100 +
patch: Kana 0.5.4 is 504, so each version raises it, as Play requires). Kana's was checked: bundletool validates it,
its manifest says 504, 0.5.4, not debuggable, target SDK 37, and it is signed with the upload key. "builds" for the
others: the same script and flags, not yet built one by one.

\* Backed up to `~/kana-android-keys/` on 2026-10-12 (they lived only in `build/`, which `COMMANDS.txt` says to delete).

Already compliant on Android (checked on the built APKs): target SDK 37, 16 KB page alignment of every native library
(including the 7 whisper CPU variants), 64-bit, signature schemes v1–v3, `debuggable=false`, exported flags, no
cleartext traffic, sizes 33–51 MB. Release builds ignore every launch argument and compile the look harness out.

---

## 2. What only you can do

**Accounts and money**
- [ ] Google Play Console account (one-time fee) and identity verification.
      **New personal accounts must run a closed test with at least 12 testers for 14 days before they can publish
      to production.** This is the longest lead time on the list. Start it as early as possible.
- [ ] Payments profile, if any app is paid or has in-app purchases.
- [ ] Price per app (free, paid or in-app purchase), and the launch order (`docs/product_split.md` §9 leaves it
      open for InfiNote and InfiniWorkshop).
- [ ] Later, for Apple: the developer program, one App Store Connect record per app (bundle ids), agreements,
      tax and banking. Then the store id of each app goes in its `version.h` (`<APP>_APP_STORE_ID`), so Rate opens the
      review page.

**Hosting**
- [ ] A public GitHub repository with GitHub Pages turned on, for the FAQ and privacy pages of every app
      (I generate the files: `python3 tools/store/site.py --pages DIR`; you create the repository and push DIR's
      contents). Each app's folder, `https://<user>.github.io/<repository>/<app>/`, then goes in its `faq_url`
      (`fude/drawing/app/info.h`; where each app sets it: the header of `tools/store/site.py`), and the in-app FAQ
      link appears.
- [ ] Public repositories for the converted ShareAlike data the licences require us to publish: Hanzi's
      (`docs/chinese_korean_data.md`), Hangul's (krdict, CC BY-SA) and Thai's (Volubilis, Wiktionary). I can prepare
      their contents. Hanzi's stroke data repository (`bovacu/hazi_custom_stroke_data`) already exists.
- [ ] Where the speech model is downloaded from. It is fetched from Hugging Face's `main` branch today, which can
      change under us. Either I pin it to a fixed revision with a checksum (no action from you), or you host the file
      in a GitHub release of your own (the most robust).

**Machines and devices**
- [ ] macOS and Xcode upgrade on this Mac: uploads must be built with a current Xcode (15.3 is too old).
- [ ] Kana's development profile expires today; Hanzi's and Hangul's expired on 2026-10-10.
- [ ] On the iPad, what Kana's old list (`apps/kana/docs/TODO.md`) left to see working: the word card (typing,
      lists), Save word on handwriting, word exams by ear; every screen's row, the side panel, a language change; the
      camera on the toolbar, Copy as text and Translate on a selection, Paste text, Text from a photo, Rate, Your
      data; Translate with Google in the word card; a scan read (lasso, Search), the phrasebook, Share as PDF, Turn
      page; the icon and the launch screen.
- [ ] Kana's meanings that JMdict and KANJIDIC2 have only in English (Spanish, Portuguese, French), translated by AI
      as you said yes to (`apps/kana/docs/TODO.md` §10): not run yet. Its translations should be read by a native
      speaker before release.

**Decisions**
- [ ] **Phones on Android.** iOS is iPad-only, but on Android every app installs on phones too, so Play will ask for
      phone screenshots. Either the apps are limited to tablets (I add a builder option that declares large
      screens only), or the layouts are checked on a phone first.
- [ ] **Keystore passwords.** All keystores use the builder's default password `android`. With Play App Signing
      the key you hold is only the upload key, so this is not fatal. Still, decide whether to use real passwords
      before the first upload.
- [ ] **FriBidi** (right-to-left text, in RDE) is LGPL-2.1 and compiled into every app on every platform (its
      symbols are in the iOS binaries, Android's libSDL3.so and the Mac's libRDE), and RDE modified one of its files
      (`lib/common.h`). LGPL-2.1 §6, for a work linked with the library: a prominent notice and the licence (done, in
      Licences), terms that allow modification for the user's own use and reverse engineering to debug it, and one of
      (a) the library's source with our changes plus the rest of the binary as objects so the user can relink, (b) a
      shared library the user can replace, (c) a written offer of (a) for three years, (d) the same from where the app
      is downloaded; §2: modified files say so. On iOS, ML Kit is linked into the same executable, which makes relinking
      impractical. The options: (1) keep it static and comply through (a), (c) or (d), feasible on Android and the Mac;
      (2) make it a shared library (debatable on iOS, where a signed app's libraries cannot be swapped); (3) replace it
      with a permissively licensed bidi library (SheenBidi, Apache-2.0, for example); (4) compile it out of the apps
      with no right-to-left text (an RDE switch), which leaves it in Arabic only. An engine decision, and yours.

---

## 3. What I can do directly (no input needed)

**Store pages and texts**
- [x] One FAQ and privacy page generator for every app (2026-10-13): `python3 tools/store/site.py [app...]` writes
      `apps/<app>/site/index.html` (help) and `privacy.html` for the eight released apps, each in exactly its own
      languages (the canvas apps: en, es, pt, fr), Arabic right to left, menu names read from each app's strings, and
      checks every page it writes. Its FACTS section says, with file:line, what each app really does: ML Kit and its
      switch (every app's now), the camera and the document scanner, the microphone and recordings (InfiNote), the
      speech model download (InfiNote, Android), crash reports (only an email the user sends), links, Android as well
      as iPad. Kana's own `apps/kana/tools/store/site.py` is gone. `--pages DIR` writes the tree to push to the GitHub
      Pages repository (§2, Hosting). Written, not hosted.
- [x] App Store and Google Play texts for every app and language, drafted (2026-10-13): `apps/<app>/store/listing.md`
      for the eight apps (Kana’s rewritten: its name, the first-launch model download, Google Play, no price promised).
      Each has, in each of the app’s languages, the App Store name, subtitle, keywords, promotional text and
      description, and the Google Play title, short and full description; once per app the English App Review notes,
      the support and privacy URLs (placeholders until hosted) and a table of every field’s length, all checked by
      script and within the limits. Left for you: the price (no text assumes one), and reading them over.
- [x] Answers drafted for App Store App Privacy, Google Play Data safety and the content rating questionnaire
      (2026-10-13): `docs/store_privacy_answers.md`, every app. Its §5 lists what to decide and what to verify on
      Google’s ML Kit disclosure pages.

**Pictures** (see §4)
- [ ] Screenshots of every app, captured by the apps' own look harness at the exact store sizes, in each language.
      Then framed with captions by one shared script whose fonts cover every script (Kana's only has Japanese).
- [ ] Play's 1024×500 feature graphic and 512×512 icon for every app.

**Fixes found by the audit**
- [ ] Android App Bundles: every app's project file has a `bundle` profile (`aab = true`). Kana's was built and
      checked (above). I then check each bundle with bundletool, including the whisper libraries.
- [ ] versionCode: the builder derives it from the version (patch capped at 99). Every upload must have a higher
      code, so I add a build number to the scheme. The canvas apps are at patch 96, with three builds left before
      0.2.0.
- [x] iOS (2026-10-12):
      - Thai, Hindi and Arabic declare their own language (`th`, `hi`, `ar`) instead of Japanese, with their camera
        prompt in it; the Japanese folders no study app but Kana declares are gone (iOS could have shown their prompts).
      - The canvas apps say why they save to Photos (`NSPhotoLibraryAddUsageDescription`, in their four languages):
        their exported pictures and videos go through the share sheet's Save Image and Save Video.
      - Their icons have no alpha channel. The fix is in the tool (`tools/icons/app_icon.py`, now every app's: the
        store icon opaque, the launch icons rounded), so a new icon cannot bring it back.
- [x] iOS build lines for every app: `zsh tools/build/ios.sh <app> [--release] [--simulator]` (2026-10-12), from the
      same source lists as the Mac's and Android's builds, so they no longer go stale. Kana and InfiNote build for the
      Simulator with it. A device build signs with `FUDE_APPLE_TEAM` and `FUDE_IOS_PROFILE` (the script's header).
- [ ] Speech to text on iOS: whisper is not built for iOS yet, so InfiNote there has no transcription.
- [x] Licences (2026-10-13): every app's `assets/licenses/libraries.txt` is written by one tool,
      `tools/licences/build.py` (`--check` verifies), from verbatim licence texts in `tools/licences/texts/`: every
      library compiled into the app on each platform (the engine's SDL3, FreeType, HarfBuzz, FriBidi, Box2D, LuaJIT,
      miniaudio, stb and the rest, found from the engine's build and the built binaries' symbols), whisper.cpp and the
      Whisper model (Sketching, InfiNote), ML Kit's iOS pods, and on Android AndroidX and PdfBox-Android; the Unicode
      notice names Hindi's and Arabic's data. The canvas apps list it in Settings › Licences, with the maths font and ML
      Kit's notices (handwriting only). `tools/mlkit/setup.py` runs the tool at its end. Left for you: etcpak's and
      ufbx's licences are not in RDE's copies of them (`texts/etcpak.txt`, `texts/ufbx.txt` hold a TODO; the tool
      names them every run): add their upstream LICENSE files to RDE. Android's ML Kit packages carry their own
      third-party notices (about 1 MB), not yet reconciled with the iOS ones.
- [ ] The canvas apps carry Kana's Android dependencies (Japanese text recognition, translation), so they are about
      10 MB bigger than they need to be and declare more data. I cut them down to what they use.
      Done in the sources (2026-10-13): the study apps' Java (text in photos, translation) is in
      `fude/android/java_study/`, passed to the study apps' builds only; the picture decoder both use moved to
      `FudeAndroid`; the canvas apps' `deps.lock` is resolved from ML Kit's handwriting reader and the document
      scanner alone (75 libraries, 10 fewer: no text recognition, translation, vision-common). Kana builds for
      Android with it, and its package has both folders' classes. Left: build the canvas apps for Android (after
      the `page.c` split), and try one on the tablet (the scanner and To text).
- [x] An ML Kit off switch in the canvas apps' Settings, as the study apps have (2026-10-13). Settings' Handwriting
      is the drawing core's now (`drawing/app/handwriting.c`), listed by every app, its switch saved with the core's
      settings; off, the canvas apps hide To text and Find reads no handwriting, so nothing reaches Google. The
      privacy pages say so (`tools/store/site.py`).
- [ ] The canvas apps' iPad camera question. Both declare `NSCameraUsageDescription` ("scans pages") and link
      VisionKit, but no canvas menu opens the document camera (only the study apps' Library does), so their privacy
      pages say they do not use the camera. Either the canvas apps get a Scan pages entry (InfiNote could well use
      one), or the key, VisionKit and the scanner dependency go from their builds. A product decision: yours.
- [x] Android: CAMERA is declared but unused by the canvas apps, and `android.hardware.camera` is required on every
      app, which hides them from tablets without a camera. Both fixed (2026-10-13): the canvas apps no longer ask for
      the camera (their scanner is Google's, which runs in Play services with its own), and RDE's manifest template
      marks the camera `required="false"`, so the study apps install on tablets without one and offer Text from a
      photo where there is one. (An RDE change: the template, for you to commit.)
- [x] Release leftovers in the canvas apps (2026-10-15): the frame rate in Settings › Display stays, in every build,
      as you asked (a person can see how smoothly their device draws a big canvas); `flush_logs_immediately` stays, in
      every app: the apps log a few lines an hour, not a frame, so flushing costs nothing measurable, and the last
      lines are kept when an app crashes; the Mermaid tool's example is in the app's language (only Mermaid's own
      keywords, `flowchart LR`, are English: they are its syntax).
- [x] Stale notes: `apps/kana/docs/app_store.md` said the Android services were to be written; they are, and it says
      so now (2026-10-13). Kana's old working list, `apps/kana/docs/TODO.md`, is a record now (2026-10-15): its
      open items are below or marked there.

---

## 4. Store pictures: what each store asks for

**App Store** (the apps are iPad-only, so only iPad shots): up to 10 per language. The required size is the
13-inch iPad, 2048×2732 or 2064×2752, portrait.

**Google Play**
- The 512×512 icon and the 1024×500 feature graphic are required.
- Between 2 and 8 screenshots per device type: phone, 7-inch tablet and 10-inch tablet.
- Tablet screenshots are what get an app shown to tablet users.
- If phones stay allowed, phone screenshots are needed too.

**How they are made**
- Kana's were captured from the iPad and the Simulator and framed by `apps/kana/tools/store/screenshots.py`. Any
  app now builds for the Simulator with `zsh tools/build/ios.sh <app> --simulator` (COMMANDS.txt has the
  Simulator's steps: the 13-inch iPad gives 2048×2732).
- The other apps can be captured on the Mac by their own look harness, at any size (`--size=WxH --shot=FILE`), with
  real content opened by flags.
- The canvas apps can open an example canvas, a notebook, a PDF, the home screen or the audio panel this way.
- The study apps' demo content is Japanese only (`fude/study/app/demo.c`), so each language needs its own demo first.
