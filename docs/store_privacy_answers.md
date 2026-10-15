# Store privacy and rating answers: every app

The answers App Store Connect and the Google Play Console ask for, for the eight released apps: **Kana Learn!, Hanzi
Learn!, Hangul Learn!, Thai Learn!, Hindi Learn!, Arabic Learn!** (the study apps) and **InfiNote, InfiniWorkshop** (the
canvas apps). Drafted 2026-10-13. The listings themselves are `apps/<app>/store/listing.md`.

Sources: the FACTS section of `tools/store/site.py` (every fact with its file and line) and the privacy pages it wrote
(`apps/<app>/site/privacy.html`), which these answers agree with; ML Kit’s privacy manifests as shipped in the iOS
apps (`third_party/mlkit/bundles/*_Privacy.bundle/PrivacyInfo.xcprivacy`, read 2026-10-13); the Android dependency
locks (`apps/<app>/platform/android/deps.lock`); and Google’s two data disclosure pages, read on 2026-10-13:
[iOS](https://developers.google.com/ml-kit/ios-data-disclosure) and
[Android](https://developers.google.com/ml-kit/android-data-disclosure). Neither Google page maps its data to Apple’s
or Google Play’s form categories, so the mapping below is ours: the items marked **verify on the page** are the ones
to read again on Google’s page or in the store’s own help before you submit.

---

## 1. The facts the answers rest on

| | Study apps (all six) | InfiNote | InfiniWorkshop |
|---|---|---|---|
| Account, ads, analytics, tracking | none | none | none |
| What the app itself sends | nothing | nothing | nothing |
| Google ML Kit in the app | handwriting, translation, and text recognition where ML Kit has the script (Japanese, Chinese, Korean, Devanagari: not Thai or Arabic) | handwriting only | handwriting only |
| When ML Kit first reaches Google | at the first launch (handwriting model download, ML Kit on by default) | the first time To text or Find reads handwriting (or Download in Settings) | same as InfiNote |
| ML Kit switch | Settings › Handwriting › Read with Google ML Kit: off, ML Kit is never called | the same switch: off, To text is hidden and Find reads no handwriting | the same |
| Other network use | Android only: Lectures › Scan pages is Google’s document scanner in Play services (not under the switch) | Android only: the speech model, downloaded from Hugging Face after the person agrees | none |
| Camera | Text from a photo (not Thai, Arabic) and Lectures › Scan pages, only while used | never | never |
| Microphone | none | while recording only | none |
| Photos | the system pickers (no library access) | pickers; add-only when saving an export | the same as InfiNote |
| Crash reports | a card at the next launch opens an email draft to rde.apps.support@gmail.com for the person to read and send; nothing is uploaded | the same | the same |
| Links | FAQ & Contact, Rate | FAQ & Contact, Rate, links written in notes | the same as InfiNote |

What ML Kit sends, in Google’s words (both pages): device information (manufacturer, model, OS version and build,
available ML accelerators); application information (bundle id or package name, app version); a per-installation
identifier, “not intended to uniquely identify a user or physical device” (on Android, features that run in Play
services, the “thin” ones, add a device identifier: here, the study apps’ document scanner); performance metrics
(latency); API configuration; event types (initialisation, model downloads, detection, release) and error codes; for
handwriting and translation, the configured languages. Purpose: “diagnostics and usage analytics”. Android page:
encrypted in transit (HTTPS); “ML Kit does not transfer this data to third parties”. Neither page offers deletion or
an opt-out of its own: the apps’ switch is the opt-out.

**Every app includes ML Kit** (the study apps since the start, the canvas apps for To text and Find), so every app
answers “yes, data is collected” below, and every answer is the same for the eight apps unless a line says otherwise.

---

## 2. App Store Connect › App Privacy

**Do you or your third-party partners collect data from this app?** Yes. (The third party is Google, through ML Kit.)

**Tick these data types, for every app** (the six that ML Kit’s privacy manifests in the app declare: `MLKitCommon`,
`MLKitDigitalInkRecognition`, `MLKitTranslate`, `MLKitNaturalLanguage` and `MLKitTextRecognitionCommon` each declare
these six, `GoogleDataTransport` Other Diagnostic Data alone; the canvas apps ship `MLKitCommon` and
`MLKitDigitalInkRecognition`, which declare the same six):

| Apple category › data type | What of ML Kit’s it is | Purposes | Linked to the user | Used for tracking |
|---|---|---|---|---|
| Identifiers › Device ID | the per-installation identifier | Analytics, App Functionality | No | No |
| Usage Data › Product Interaction | event types (initialisation, model download, recognition) | Analytics, App Functionality | No | No |
| Diagnostics › Performance Data | latency and other performance metrics | Analytics, App Functionality | No | No |
| Diagnostics › Other Diagnostic Data | error codes, device information, API configuration | Analytics | No | No |
| User Content › Other User Content | declared by Google’s manifests (no item of the web page matches it; the closest is the API configuration) | Analytics, App Functionality | No | No |
| Other Data › Other Data Types | the app’s bundle id and version, the configured language | Analytics, App Functionality | No | No |

Why all six, though Google’s web page names fewer: Apple builds the app’s privacy report from the manifests inside the
app, and App Review compares it with these answers; answering exactly what Google’s own manifests declare keeps the two
the same. (A narrower set, without User Content, would match the web page but not the manifests.) This is also what
`apps/kana/docs/app_store.md` answers for Kana. If a newer ML Kit changes its manifests (after `tools/mlkit/setup.py`),
read them again and follow them.

**What the product page then shows:**
- *Data Used to Track You:* none.
- *Data Linked to You:* none.
- *Data Not Linked to You:* Identifiers, Usage Data, Diagnostics, User Content, Other Data.

**Tracking:** No. No App Tracking Transparency prompt: every manifest says `NSPrivacyTracking` false, and the apps
have no tracking domains.

**Not declared, and why:**
- **What people write, draw, photograph, scan, record or translate** (pages, ink, photos, documents, InfiNote’s
  recordings and transcripts): read and kept on the device only; neither the app nor ML Kit sends it (site.py FACTS;
  the privacy pages say so). Data processed only on the device is not “collected” in Apple’s sense.
- **Crash reports.** Apple counts as collected the data transmitted off the device in a way that lets you or a partner
  keep it. The apps transmit nothing: they write the report on the device and, at the next launch, offer an email
  draft that the person reads in their own mail app and sends, or not (`fude/drawing/base/crash.c`,
  `fude/drawing/app/crashcard.c`); either button deletes the file. That is a person writing to support, which no app
  declares; it would also meet Apple’s “optional disclosure” conditions (infrequent, not the app’s main purpose,
  optional, the person sees what is sent and from which address, and chooses each time). **Verify** on Apple’s
  [App privacy details](https://developer.apple.com/app-store/app-privacy-details/) page that you agree. The
  cautious alternative is to declare Diagnostics › Crash Data and Contact Info › Email Address, App Functionality,
  *linked* to the person (an email has their address): it would add a “Data Linked to You” section to every app.
- **Apple’s own crash reports and analytics** (the opt-in ones in iPadOS settings) are Apple’s, not the app’s.
- **The document camera** on iPad is Apple’s VisionKit (no Google involved), and reading aloud uses iPadOS’s voices,
  on the device.
- **Links** (FAQ, Rate, links in notes) open Safari or the App Store at the person’s tap: no data from the app.

**Per app:** no differences in the answers. The privacy policy URL for each app is
`https://<user>.github.io/<repo>/<app>/privacy.html` (in each listing).

---

## 3. Google Play Console › App content › Data safety

### Data collection and security

| Question | Answer (every app) |
|---|---|
| Does your app collect or share any of the required user data types? | **Yes** (Google ML Kit) |
| Is all of the user data collected by your app encrypted in transit? | **Yes** (Google’s Android page: HTTPS) — verify on the page |
| Which of the following methods of account creation does your app support? | **My app does not allow users to create an account** (no account, no sign-in) |
| Do you provide a way for users to request that their data is deleted? | **No**: there is no account, the developer holds no one’s data, and ML Kit’s diagnostics are not linked to a person. (Uninstalling removes everything the app keeps on the device.) |
| Independent security review | No (optional badge) |

### Data types

Declare these, for every app:

| Play category › data type | Collected | Shared | Processed ephemerally | Required or optional | Purpose |
|---|---|---|---|---|---|
| Device or other IDs › Device or other IDs | Yes | No | No | Optional | Analytics |
| App info and performance › Diagnostics | Yes | No | No | Optional | Analytics |
| App info and performance › Other app performance data | Yes | No | No | Optional | Analytics |
| App activity › App interactions | Yes | No | No | Optional | Analytics |

- **Device or other IDs:** the per-installation identifier, and on the study apps the device identifier of the
  document scanner (a Play services feature).
- **Diagnostics:** latency, error codes. **Other app performance data:** device information, package name and version,
  API configuration, feature version, configured language.
- **App interactions:** the event types (feature started, model downloaded, recognition run). This one is the
  cautious choice, matching Usage Data on the App Store; it can be argued that SDK events are diagnostics only.
  Verify on the page.
- **Shared: No.** Google’s Android page says ML Kit does not transfer the data to third parties; Google receives it as
  the SDK’s provider. Verify on the page.
- **Optional:** the person can switch ML Kit off (Settings › Handwriting › Read with Google ML Kit), and then the app
  never calls it. In the study apps it is on by default and downloads its model at the first launch, so if you read
  Google Play’s definition of “optional” as “the person chooses before anything is collected”, answer **Required**:
  it is the safe answer. Verify in Play’s help (“Data safety section” › “optional”).
- **Purpose: Analytics** only: Google states one purpose, “diagnostics and usage analytics”, which is what Play’s
  Analytics covers (app health, diagnosing problems). It is not App functionality: no feature of the apps depends on
  it.

**Not declared, and why:**
- **Photos and videos, Audio, Files and docs, Messages, Personal info, Location:** processed on the device only, never
  transmitted (pictures and scans, InfiNote’s recordings and transcripts, documents). The apps ask for no location.
- **Crash logs:** not collected by the app: it writes the report on the device and opens an email draft that the
  person sends with their own email app, or not. Verify that you agree, as for the App Store; the cautious
  alternative is App info and performance › Crash logs plus Personal info › Email address, Collected, purpose App
  functionality.
- **InfiNote’s speech model (Android only):** an HTTPS download of a public file
  (`https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin`, `fude/zoom/listen.h`), after the
  person taps twice to agree. It sends nothing but the request itself: no identifier, no recording, no text; Hugging
  Face sees the device’s IP address, as any web server would. Nothing to declare for it. (The privacy page names it.)
- **Google’s document scanner** (study apps, Android): it runs in Google Play services under Google’s terms; what it
  sends is ML Kit’s same data types, already declared above.

**Thai and Arabic on Android:** their dependency locks include ML Kit’s Latin text recognition, though the apps never
read printed text (`fude/lang/th/lang.c`, `fude/lang/ar/lang.c`: `fude_lang_text_readable` false). Never called, it
sends nothing, so the answers stay the same; it only makes the packages bigger.

---

## 4. Content ratings

### Apple: Age Rating (App Store Connect › App Information)

**Study apps (all six): answer as for Kana (chosen 2026-10-02), giving 13+.** Their dictionaries and example
sentences are real language: among their meanings and translations are words about violence, alcohol and drugs, sex
and crude language. Counted in the shipped data on 2026-10-13 (whole-word matches in English, in
`apps/<app>/assets/data/characters.kana`): Kana: “kill” 26, “drug” 45, “sex” 41, “shit” 5; Hanzi: “fuck” 19, “kill”
112, “sex” 81; Hangul: “fuck” 8, “kill” 25; Thai, Hindi and Arabic: a few each (“fuck” 2 to 3, “sex” 8 to 14, “kill”
4 to 23). Among tens of thousands of entries, as reference meanings:

- **Infrequent/Mild:** Profanity or Crude Humor; Mature or Suggestive Themes; Alcohol, Tobacco, or Drug Use or
  References; Realistic Violence; Cartoon or Fantasy Violence (Kana’s 2026-10-02 answers, kept for all six). If the
  questionnaire asks about Guns or Other Weapons, also Infrequent/Mild (dictionary words such as gun and bomb).
- **None:** everything else (Sexual Content or Nudity, Graphic Sexual Content, Horror/Fear Themes, prolonged violence,
  Medical or Treatment Information, Gambling, Simulated Gambling, Contests).
- **Capabilities:** Unrestricted Web Access: No (no browser in the app; FAQ & Contact and Rate open Safari or the
  App Store at a tap). User-Generated Content: No (what people make stays on their device; nothing is shared with other
  users). Messaging and Chat: No. Advertising: No. Parental Controls and Age Assurance: No.
- **Result:** 13+ (Apple’s tiers since 2025: 4+, 9+, 13+, 16+, 18+). A 4+ or 9+ rating would need those words and
  sentences filtered out of each app’s data first: a choice, not a requirement.

**InfiNote and InfiniWorkshop: None to every content question, 4+.** The apps bring no content of their own beyond
the UI, InfiniWorkshop’s examples (circuits, gears, a workbench) and the paper templates. Unrestricted Web Access: No:
a link a person writes in a note opens in Safari when they tap it, which is not browsing inside the app (verify the
current wording of the question). User-Generated Content: No (notes stay on the device; exporting goes through the
system’s share sheet at the person’s choice, not to other users of the app). Messaging, Advertising, Gambling: No.

**In-app purchases:** none today. If the pricing decision adds them, the App Store shows “In-App Purchases” on its
own; the rating answers do not change.

Verify the questionnaire’s current wording in App Store Connect: Apple revised it in 2025, and its questions may
differ from the names above.

### Google Play: content rating (the IARC questionnaire)

- **Category to choose:** study apps: *Reference, News, or Educational*. Canvas apps: *Utility, Productivity,
  Communication, or Other*. Verify the names in the Console.
- **Content:** canvas apps: no violence, sexuality, language, controlled substances, crude humour or gambling. Study
  apps: answer that the app contains, as dictionary and example-sentence text, references to violence, drugs and
  alcohol, sexual terms and crude language (the counts above), none of it depicted, illustrated or promoted.
- **Interactive elements, every app:** Users can interact or exchange content: **No**. Shares the user’s location:
  **No**. Digital purchases: **No** (until a pricing decision adds in-app purchases: then Yes). Unrestricted internet
  access: **No** (no browser or search in the app; it opens only the links the person taps: FAQ, Rate, and in the
  canvas apps links written in notes).
- **Ratings to expect** (IARC computes them; verify on the certificate): canvas apps: ESRB Everyone, PEGI 3, USK 0,
  and their equivalents. Study apps: if the dictionary’s crude language and drug and sex references are declared as
  above, around ESRB Teen, PEGI 12, USK 12; the reference framing may bring them lower. Either is honest; the answers
  must be.

### Google Play: other App content answers

- **Target audience and content:** choose 13 and over (13–15, 16–17, 18 and over) for every app. Choosing an under-13
  group brings in the Families policy for the app and every SDK in it, ML Kit included; the study apps’ data is not
  meant for young children either (13+ on the App Store).
- **Ads:** No. **App access:** all functionality available without special access. **News app, financial features,
  health, government app:** No.

---

## 5. What you must check or decide

- [ ] **Pricing** (every app): free, paid once or in-app purchases. Nothing here or in the listings assumes one;
      the study apps’ own “Why it works offline” text already says “one purchase”, so it must change if they are not.
- [ ] **Apple App Privacy:** the six types above (the manifests’), or the narrower five without User Content.
- [ ] **Crash reports:** not declared (as written) or declared the cautious way, in both stores.
- [ ] **Play “optional” vs “required”** for ML Kit’s data (on by default in the study apps).
- [ ] **Play App interactions:** declared (cautious) or not.
- [ ] **Ratings:** Apple 13+ for the study apps (or filter their data for a lower one); IARC’s results.
- [ ] **Verify on the page:** Google’s [iOS](https://developers.google.com/ml-kit/ios-data-disclosure) and
      [Android](https://developers.google.com/ml-kit/android-data-disclosure) disclosures (encryption, no transfer
      to third parties, the data list), read 2026-10-13: Translation’s line there also mentions Firebase Remote
      Config and Installations, which these apps do not ship (no Firebase Installations or Remote Config in
      `third_party/mlkit/frameworks/` or the Android locks), so those Firebase sections do not apply; check again if
      ML Kit is updated.
