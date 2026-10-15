#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Every released app's two public web pages: its help page (index.html: what the app is, how to start, the questions
# people ask, how to write to us) and its privacy policy (privacy.html), each in exactly the app's own languages.
#
#   python3 tools/store/site.py                     # every released app
#   python3 tools/store/site.py kana infinote       # some of them
#   python3 tools/store/site.py --pages DIR         # also: DIR/ ready to push to the GitHub Pages repository
#
# Writes apps/<app>/site/index.html and apps/<app>/site/privacy.html (the copies kept with each app), and checks every
# page it writes (the checks are at the end of this file: well-formed HTML, no ASCII apostrophe in the prose, no text
# cut with "..." or "…", every language there with its lang, Arabic right to left). Python 3's standard library only.
#
# Hosting: one GitHub repository for every app, one folder per app. --pages DIR writes DIR/<app>/index.html and
# DIR/<app>/privacy.html for every app named, an index.html at DIR's root listing them, and .nojekyll (GitHub Pages
# then serves the files as they are). Push DIR's contents to the repository, turn on Settings › Pages › Deploy from a
# branch (main, / (root)), and a minute later each app's pages are at
#   https://<user>.github.io/<repository>/<app>/              its help page    (the stores' Support URL)
#   https://<user>.github.io/<repository>/<app>/privacy.html  its privacy policy (the stores' Privacy Policy URL)
# Then the in-app link (the side panel's FAQ & Contact › Frequently asked questions, fude/drawing/app/side.c) is each
# app's own folder: set .faq_url = "https://<user>.github.io/<repository>/<app>/" in the app's fude_app_info
# (apps/<app>/<app>.c, every app's own file, the canvas apps' included). FUDE_APP_FAQ_URL (fude/drawing/app/info.h), the one every
# app falls back to, can be the repository's root, whose index lists every app. The page picks the reader's language
# itself (the browser's, or #es, #ja... at the end of the address), so one address serves every language.
#
# Maintaining it, top to bottom:
#   FACTS     what each app really does, with where in the code it is shown: change a fact when the code changes,
#             and the pages follow (an app's ML Kit switch, for one, is its mlkit_switch: the menu path, or None).
#   TEXT      the paragraphs, one dictionary a language, written whole in that language (not word for word); a
#             paragraph used only when a fact says so. The menu names in them are never typed: {m[ID]} is the app's own
#             label for that string id, read from the app's assets/text/strings.rdel (made by tools/strings/build.py),
#             so a menu renamed in the app is renamed here at the next run. Prose uses the typographic apostrophe ’.
#   UPDATED   the date the pages show: change it when what they say changes.
# A language an app gains needs its TEXT (the run says which paragraph is missing); a new app needs its FACTS row.
import html, os, re, shutil, sys
from html.parser import HTMLParser

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

CONTACT = 'rde.apps.support@gmail.com'   # the address for questions, on every page (FUDE_APP_CONTACT in info.h too)
UPDATED = (2026, 10, 13)                 # year, month, day: when the pages last changed what they say

# The apps with pages, in this order on the hosting index. (Sketching is the developer build and apps/draw is not
# released: neither has pages.)
RELEASED = ['kana', 'hanzi', 'hangul', 'thai', 'hindi', 'arabic', 'infinote', 'infiniworkshop']

GOOGLE_PRIVACY = 'https://policies.google.com/privacy'
MLKIT_TERMS    = 'https://developers.google.com/ml-kit/terms'
MLKIT_IOS_DATA = 'https://developers.google.com/ml-kit/ios-data-disclosure'
MLKIT_AND_DATA = 'https://developers.google.com/ml-kit/android-data-disclosure'
HF_PRIVACY     = 'https://huggingface.co/privacy'

# =====================================================================================================================
# FACTS: what each app does, verified in the code on 2026-10-13. file:line is where each is shown.
# =====================================================================================================================
#
# Every app
#   - No accounts, ads, analytics or tracking. RDE's analytics module is compiled in but never started: nothing in
#     fude/ or apps/ calls rde_analytics_init (~/RDE/engine/src/rde_analytics.c:41,197-239); RDE's push, ads and
#     in-app purchases are never started either (~/RDE/rde_build.c:13355-13387, rde_mobile.c:3791).
#   - Everything is kept in the app's private save folder, <id>/ (fude/drawing/base/save.c:26-46; iOS: Library/
#     Application Support, ~/RDE/engine/src/rde_mobile.c:1787-1794). It is in the device's backups: iCloud Backup
#     (save.h:31-34), Android's backup (android:allowBackup="true", ~/RDE/external/templates/android/app/src/main/
#     AndroidManifest.xml.in:199, no rules; Android keeps at most 25 MB an app).
#   - Settings › Your data › Export writes every file of the save folder into one backup file and hands it to the
#     share sheet; Import replaces everything (fude/drawing/base/backup.h:9-31, side.c:1649-1678, session.c:357-367).
#   - Crash reports: a crash or a death on screen leaves crash_report.txt in the save folder (the app, its version,
#     the device's model and system, the stack, the last notes like "screen home"; never what the person wrote:
#     fude/drawing/base/crash.h:8-23, crash.c:352-353). At the next launch a card asks: Send report opens a mailto:
#     draft to CONTACT with the report in it, for the person to read and send; either button deletes it
#     (fude/drawing/app/crashcard.c:41-62, crash.c:439-449). Nothing is uploaded. The system's own crash handling
#     still runs afterwards (crash.c:281-295; FudeAndroid.java:74-76): Apple's and Google's opt-in reports.
#   - Links: FAQ & Contact opens faq_url in the browser and a mailto: draft (side.c:300-351); Rate opens Apple's
#     rating sheet or Google Play (side.c:272-279, ~/RDE/engine/src/rde_mobile.c:2162-2177). Nothing else opens a link.
#   - The only network use in the apps' own code is InfiNote's speech model download (below); everything else on the
#     network is Google ML Kit's own, and Google's document scanner (Android, the study apps).
#   - ML Kit's diagnostics: what Google's iOS and Android data disclosures list (MLKIT_*_DATA), also declared in ML
#     Kit's privacy manifests shipped in the iOS app (third_party/mlkit/bundles/, apps/kana/docs/app_store.md).
#
# The study apps (kana, hanzi, hangul, thai, hindi, arabic)
#   - ML Kit: handwriting (MLKitDigitalInkRecognition), translation (MLKitTranslate), and text recognition where ML
#     Kit has the script ([profile ios] in its .rdeproj; Android: its platform/android/deps.lock). Text recognition:
#     ja, zh, ko, hi yes; th, ar no (fude/lang/<code>/lang.c fude_lang_text_readable). Its model ships inside the app
#     on both platforms, never downloaded (study/services/textscan.h:11, textscan_android.c:8). It reads Text from a
#     photo, and the pages of a document that have no text of their own (scans, photos) so that Search and the lasso
#     find their words (drawing/doc/doc.h:35-38, study/app/study.c:283-291).
#   - Handwriting reads Check, Copy as text, Save word, drawing to search, and exams (study/handwriting/recognize.c:30,
#     screens/browse.c:276, exam.c:520). Its model (about 20 MB) downloads when the app starts with ML Kit on, which it
#     is by default: at the first launch (study/app/shell.c:251-254; drawing/ink/mlkit.c:13-14; mlkit_ios.m:107,
#     mlkit_android.c:35-45).
#   - Translation models, about 30 MB a language, download the first time a translation is asked for
#     (study/services/translate.h:26; scan.c:349-354, pagetext.c:219-223, translator.c:95-99). The pair: the language
#     taught and the app's language, or English (translate.c:74-87).
#   - The switch: Settings › HANDWRITING › Read with Google ML Kit, every app's (drawing/app/handwriting.c, listed by
#     study/app/study.c:164), on by default; off, ML Kit is never called by the app: handwriting, text recognition and
#     translation all check it (mlkit.h:45-46; textscan_ios.m:225, textscan_android.c:34-39, translate.c:89-105). It
#     is a per-app fact below (mlkit_switch), a path of string ids.
#   - Camera: Text from a photo, live (RDE's camera: AVCaptureSession on iOS, Camera2 on Android), only while that
#     screen is open, asking permission the first time (study/screens/scan.c:196-205); the photo picker (PHPicker on
#     iOS, Android's photo picker) hands over one picture, read in memory, never kept (textscan_ios.m:19-21,232-235;
#     FudeText.java:118,141). Not in Thai and Arabic: no camera on the toolbar (study.c:158, toolbar.c:389-400), the
#     long-press item greyed (pagetext.c:299,328).
#   - Lectures › Scan pages: Apple's document camera, VisionKit (drawing/app/import_ios.m:122-199); on Android Google's
#     document scanner in Play services, which asks for the camera itself and is not under the ML Kit switch
#     (fude/android/java/com/rde/fude/FudeImport.java:111-131). From Photos and From Files: the system's pickers
#     (import_ios.m:173-182, FudeImport.java:94-109). What comes in becomes a PDF kept in the notes (drawing/app/import.h).
#   - iOS asks only for the camera (apps/<app>/platform/ios/Info.plist:31-32); Android: CAMERA (not Thai and Arabic,
#     which never open it themselves), INTERNET, ACCESS_NETWORK_STATE, WAKE_LOCK (apps/<app>/<app>.rdeproj, [android]
#     permissions), and FOREGROUND_SERVICE from androidx.work.
#   - Reading aloud: the device's voices, AVSpeechSynthesizer (study/services/speech_ios.m) or Android's TextToSpeech
#     with the engine the person chose (fude/android/java/com/rde/fude/FudeSpeech.java).
#   - Reviews: due the next day, then 3 days, then the last step times the ease; wrong, due tomorrow again; at most 10
#     new a day (study/models/review.h:9-41).
#
# The canvas apps (infinote, infiniworkshop): one shell, fude/zoom/shell.c, each described by apps/<app>/<app>.c and its src/version.h
#   - ML Kit: handwriting only, MLKitDigitalInkRecognition (.rdeproj [profile ios]; deps.lock): the lasso's To text
#     and Find's handwriting search (zoom/page_edit.c:1077, page_find.c:437-455,567-572). Its model (about 20 MB, as
#     the app's MLKIT_DOWNLOADING says), for the language the app is shown in (zoom/shell.c: fude_zoom_ink_model at launch),
#     downloads the first time either reads handwriting, or when Settings' Download is pressed
#     (drawing/app/handwriting.c:58-72); never at launch. The same switch as the study apps' since 2026-10-13
#     (zoom/page_settings.c:546 lists FUDE_HANDWRITING_SECTION): off, To text is hidden (page_find.c:393-395) and Find
#     reads no handwriting (page_find.c:438). An app without a switch would have mlkit_switch None: its pages then say
#     that ML Kit is used only when those features are (ml_noswitch_canvas).
#   - The camera: never opened. The only caller of the document camera is the study apps' Lectures
#     (drawing/app/library.c:341-351); no canvas menu reaches it, and Android asks for no camera
#     (their project files' [android] permissions). (Their iOS Info.plist still declares NSCameraUsageDescription:
#     unused.)
#   - Pictures: Insert › Photos and Insert › Files, the system's pickers (zoom/page_pictures.c:235-262); Insert › PDF
#     opens a copy as a canvas of its own (page_pictures.c:71-77). Exports go to the share sheet (page_export.c:331-336);
#     on iPad its Save Image and Save Video need NSPhotoLibraryAddUsageDescription (Info.plist:29-32), add-only.
#   - Links written in a text or note open in the browser when tapped (zoom/page_text.c:202-205, page_edit.c:1127-1135).
#   - No reading aloud (FudeSpeech and AVSpeechSynthesizer are the study layer's only: the project files).
#   - InfiNote records (product NOTES, audio true: zoom/product.c:36); InfiniWorkshop does not (product.c:49, no
#     microphone key in its Info.plist, none in its project file's [android] permissions). Recording asks for the
#     microphone at the first tap and stops when the canvas or the app is left (page_audio.c:96-99,165-178,
#     zoom/shell.c's events); recordings are <id>.rec<n> in the save folder, in the export (zoom/audio.h:19-28).
#   - Speech to text (InfiNote): whisper.cpp with OpenAI's Whisper base model, on Android only (FUDE_WHISPER:
#     their project files' [android] and [macos]; not built for iOS, where the buttons are hidden:
#     page_audio.c:384). The model, 59,707,625 bytes (the app shows 57 MB), comes from
#     https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin (zoom/listen.h:35-38) only after a
#     second tap on Transcribe or Live text (page_audio.c:588-604,686-698, page_speech.c:117-124), into the
#     downloads folder: the platform's cache, which no backup takes in and the system may empty, when it is downloaded
#     again (drawing/base/save.h: fude_save_downloads_dir; zoom/listen.c). Transcripts: notes/<id>.rec<n>.text
#     (page_speech.c:26-31).
#   - Removing a canvas deletes every file it had: each is notes/<id>.<something>, recordings and transcripts
#     included (drawing/ink/notes.c: fude_notes_delete_files).

# The language each study app teaches, in each language its pages are in. 'the' is the form after a verb where the
# language needs an article (French: écrire le japonais, l’arabe); elsewhere it is the name itself.
TAUGHT = {
    'ja': {'en': 'Japanese', 'es': 'japonés', 'pt': 'japonês', 'fr': ('japonais', 'le japonais'), 'ja': '日本語'},
    'zh': {'en': 'Chinese', 'es': 'chino', 'pt': 'chinês', 'fr': ('chinois', 'le chinois'), 'zh': '中文'},
    'ko': {'en': 'Korean', 'es': 'coreano', 'pt': 'coreano', 'fr': ('coréen', 'le coréen'), 'ko': '한국어'},
    'th': {'en': 'Thai', 'es': 'tailandés', 'pt': 'tailandês', 'fr': ('thaï', 'le thaï'), 'th': 'ภาษาไทย'},
    'hi': {'en': 'Hindi', 'es': 'hindi', 'pt': 'hindi', 'fr': ('hindi', 'le hindi'), 'hi': 'हिंदी'},
    'ar': {'en': 'Arabic', 'es': 'árabe', 'pt': 'árabe', 'fr': ('arabe', 'l’arabe'), 'ar': 'العربية'},
}

# Each app's facts (see above), and what it is, in each of its languages ({app} is its name).
#   kind          'study' or 'canvas': which paragraphs apply
#   taught        the language a study app teaches (fude/lang/<code>)
#   unit          what a study app's learner writes: 'char' (characters) or 'letter' (letters)
#   mark          the glyph on the pages' badge
#   ocr           ML Kit reads printed text in this language (Text from a photo, scanned pages)
#   mlkit_switch  the path of string ids to the switch that turns ML Kit off, or None when the app has none
#   microphone    records audio (InfiNote)
#   transcribes   the platforms where recordings become text ('android')
#   home_new      the home screen's button that starts a new canvas (canvas apps)
#   exports       the Export tool's choices (string ids, in its order: zoom/page_export.c:379-392), as the app's
#                 product gives them (zoom/product.c:33-34 InfiNote's, :47 InfiniWorkshop's: all of them)
MLKIT_SWITCH = ('SETTINGS', 'SETTINGS_HANDWRITING', 'SETTINGS_MLKIT')   # every app's: drawing/app/handwriting.c
NOTES_EXPORTS = ['ZOOM_EXPORT_PNG', 'ZOOM_EXPORT_SVG', 'ZOOM_EXPORT_PDF', 'ZOOM_EXPORT_TILES', 'ZOOM_EXPORT_TILES_LETTER',
                 'ZOOM_EXPORT_VIDEO', 'ZOOM_EXPORT_AREAS']
WORKSHOP_EXPORTS = ['ZOOM_EXPORT_PNG', 'ZOOM_EXPORT_SVG', 'ZOOM_EXPORT_PDF', 'ZOOM_EXPORT_DXF', 'ZOOM_EXPORT_TILES',
                    'ZOOM_EXPORT_TILES_LETTER', 'ZOOM_EXPORT_CUTLIST', 'ZOOM_EXPORT_VIDEO', 'ZOOM_EXPORT_AREAS',
                    'ZOOM_EXPORT_SHEETS', 'ZOOM_EXPORT_STL', 'ZOOM_EXPORT_PARTS']

APPS = {
    'kana': dict(kind='study', taught='ja', unit='char', mark='字', ocr=True, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Japanese by hand: kana and kanji stroke by stroke, words to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir japonés a mano: kana y kanji trazo a trazo, palabras para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever japonês à mão: kana e kanji traço a traço, palavras para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'ja': '{app}は、手で日本語を書くことを学ぶアプリです。かなと漢字を一画ずつ、読んで聞ける言葉、自分の単語帳、テストと復習。iPadとAndroidタブレットで、ペンでも指でも、オフラインで使えます。',
        'fr': '{app} vous apprend à écrire le japonais à la main : kana et kanji trait par trait, des mots à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'hanzi': dict(kind='study', taught='zh', unit='char', mark='汉', ocr=True, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Chinese by hand: the characters stroke by stroke, simplified and traditional, by HSK level; words to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir chino a mano: los caracteres trazo a trazo, simplificados y tradicionales, por nivel del HSK; palabras para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever chinês à mão: os caracteres traço a traço, simplificados e tradicionais, por nível do HSK; palavras para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'zh': '{app} 教你亲手书写中文：汉字按笔顺一笔一画地写，简体繁体都有，按 HSK 等级排列；还有可以阅读和收听的词语、你自己的生词本、测验和复习。适用于 iPad 和 Android 平板，用笔或手指都能写，无需联网。',
        'fr': '{app} vous apprend à écrire le chinois à la main : les caractères trait par trait, simplifiés et traditionnels, par niveau HSK ; des mots à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'hangul': dict(kind='study', taught='ko', unit='char', mark='한', ocr=True, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Korean by hand: the Hangul letters and syllables stroke by stroke, and the hanja; words by level to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir coreano a mano: las letras y sílabas del hangul trazo a trazo, y los hanja; palabras por nivel para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever coreano à mão: as letras e sílabas do hangul traço a traço, e os hanja; palavras por nível para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'ko': '{app}은 한국어를 손으로 쓰는 법을 가르쳐 줍니다. 한글 자모와 음절, 그리고 한자를 획순대로 한 획씩 익히고, 수준별 단어를 읽고 들으며, 나만의 단어장과 시험, 복습으로 다질 수 있습니다. iPad와 Android 태블릿에서 펜이나 손가락으로, 인터넷 없이 사용할 수 있습니다.',
        'fr': '{app} vous apprend à écrire le coréen à la main : les lettres et les syllabes du hangul trait par trait, et les hanja ; des mots par niveau à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'thai': dict(kind='study', taught='th', unit='letter', mark='ก', ocr=False, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Thai by hand: the Thai alphabet by consonant class, every letter stroke by stroke from the little loop where most of them start; words to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir tailandés a mano: el alfabeto tailandés por clase de consonante, cada letra trazo a trazo desde el pequeño bucle donde empiezan casi todas; palabras para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever tailandês à mão: o alfabeto tailandês por classe de consoante, cada letra traço a traço a partir do pequeno laço onde quase todas começam; palavras para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'th': '{app} สอนให้คุณเขียนภาษาไทยด้วยลายมือ ตั้งแต่ตัวอักษรไทยที่จัดตามหมู่พยัญชนะ แต่ละตัวเขียนทีละเส้นตามลำดับ เริ่มจากหัวกลมเล็ก ๆ ที่ตัวอักษรส่วนใหญ่ใช้ขึ้นต้น มีคำศัพท์ให้อ่านและฟัง คลังคำศัพท์ของคุณเอง แบบทดสอบ และการทบทวน ใช้ได้บน iPad และแท็บเล็ต Android ด้วยปากกาหรือนิ้ว โดยไม่ต้องต่ออินเทอร์เน็ต',
        'fr': '{app} vous apprend à écrire le thaï à la main : l’alphabet thaï par classe de consonne, chaque lettre trait par trait depuis la petite boucle par laquelle presque toutes commencent ; des mots à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'hindi': dict(kind='study', taught='hi', unit='letter', mark='अ', ocr=True, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Hindi by hand: the Devanagari alphabet, its vowels, consonants and vowel signs stroke by stroke, the line along the top last; words to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir hindi a mano: el alfabeto devanagari, sus vocales, consonantes y matras trazo a trazo, con la línea de arriba al final; palabras para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever hindi à mão: o alfabeto devanágari, suas vogais, consoantes e matras traço a traço, com a linha de cima por último; palavras para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'hi': '{app} आपको हाथ से हिंदी लिखना सिखाता है: देवनागरी वर्णमाला के स्वर, व्यंजन और मात्राएँ, हर स्ट्रोक सही क्रम में और ऊपर की रेखा सबसे आख़िर में; साथ ही पढ़ने और सुनने के लिए शब्द, आपकी अपनी शब्दावली, परीक्षाएँ और दोहराव। iPad और Android टैबलेट पर, पेन या उंगली से, बिना इंटरनेट के।',
        'fr': '{app} vous apprend à écrire le hindi à la main : l’alphabet devanagari, ses voyelles, consonnes et matras trait par trait, la ligne du haut en dernier ; des mots à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'arabic': dict(kind='study', taught='ar', unit='letter', mark='ع', ocr=False, mlkit_switch=MLKIT_SWITCH, about={
        'en': '{app} teaches you to write Arabic by hand: every letter stroke by stroke, from right to left, in each of the forms it takes in a word; words to read and listen to, your own vocabulary, exams and reviews — on iPad and Android tablets, with a pen or your finger, offline.',
        'es': '{app} te enseña a escribir árabe a mano: cada letra trazo a trazo, de derecha a izquierda, en cada una de las formas que toma en una palabra; palabras para leer y escuchar, tu propio vocabulario, exámenes y repasos — en iPad y tabletas Android, con lápiz o con el dedo, sin conexión.',
        'pt': 'O {app} ensina você a escrever árabe à mão: cada letra traço a traço, da direita para a esquerda, em cada uma das formas que ela toma numa palavra; palavras para ler e ouvir, seu próprio vocabulário, testes e revisões — no iPad e em tablets Android, com caneta ou com o dedo, offline.',
        'ar': 'يعلّمك {app} كتابة العربية بخط اليد: كل حرف ضربةً بعد ضربة، من اليمين إلى اليسار، في كل شكل يتخذه داخل الكلمة، مع كلمات تقرؤها وتستمع إليها، ومفرداتك الخاصة، والاختبارات والمراجعات — على iPad والأجهزة اللوحية العاملة بنظام Android، بالقلم أو بالإصبع، ودون اتصال بالإنترنت.',
        'fr': '{app} vous apprend à écrire l’arabe à la main : chaque lettre trait par trait, de droite à gauche, dans chacune des formes qu’elle prend dans un mot ; des mots à lire et à écouter, votre propre vocabulaire, examens et révisions — sur iPad et tablettes Android, au stylet ou du doigt, hors ligne.',
    }),
    'infinote': dict(kind='canvas', mark='∞', mlkit_switch=MLKIT_SWITCH, microphone=True, transcribes=('android',),
                     home_new='ZOOM_HOME_NEW_NOTEBOOK', exports=NOTES_EXPORTS, about={
        'en': '{app} is a notebook for iPad and Android tablets: write and draw on pages, or on a canvas you can zoom into without end, with your PDFs, maths and diagrams, and recordings linked to what you write.',
        'es': '{app} es un cuaderno para iPad y tabletas Android: escribe y dibuja en páginas, o en un lienzo en el que puedes acercarte sin fin, con tus PDF, matemáticas y diagramas, y grabaciones unidas a lo que escribes.',
        'pt': 'O {app} é um caderno para iPad e tablets Android: escreva e desenhe em páginas, ou numa tela que você pode aproximar sem fim, com seus PDFs, matemática e diagramas, e gravações ligadas ao que você escreve.',
        'fr': '{app} est un carnet pour iPad et tablettes Android : écrivez et dessinez sur des pages, ou sur une toile où zoomer sans fin, avec vos PDF, des maths et des diagrammes, et des enregistrements liés à ce que vous écrivez.',
    }),
    'infiniworkshop': dict(kind='canvas', mark='⚙︎', mlkit_switch=MLKIT_SWITCH, microphone=False, transcribes=(),
                           home_new='ZOOM_HOME_NEW_PROJECT',
                           exports=WORKSHOP_EXPORTS, about={
        'en': '{app} is a project notebook for iPad and Android tablets whose drawings work: circuits that run, mechanisms that move, plans at true size, woodworking and sewing patterns, all on a canvas you can zoom into without end.',
        'es': '{app} es un cuaderno de proyectos para iPad y tabletas Android cuyos dibujos funcionan: circuitos que se encienden, mecanismos que se mueven, planos a tamaño real, carpintería y patrones de costura, todo en un lienzo en el que puedes acercarte sin fin.',
        'pt': 'O {app} é um caderno de projetos para iPad e tablets Android cujos desenhos funcionam: circuitos que acendem, mecanismos que se movem, plantas em tamanho real, marcenaria e moldes de costura, tudo numa tela que você pode aproximar sem fim.',
        'fr': '{app} est un carnet de projets pour iPad et tablettes Android dont les dessins fonctionnent : des circuits qui s’allument, des mécanismes qui bougent, des plans à taille réelle, de la menuiserie et des patrons de couture, le tout sur une toile où zoomer sans fin.',
    }),
}

# The ids the app shows in capitals as a section's header (Settings' HANDWRITING): written in a sentence they read
# better as a word ("Settings › Handwriting"), as Kana's pages always had them.
SECTION_HEADERS = {'SETTINGS_HANDWRITING', 'SETTINGS_ABOUT', 'SETTINGS_THEME', 'SETTINGS_LANGUAGE', 'SIDE_STUDY'}

# =====================================================================================================================
# The page languages: html lang, direction, the name shown on its button, the app strings' block, and the date.
# =====================================================================================================================

LANGS = {
    'en': ('en', 'ltr', 'English', 'EN-US'),
    'es': ('es', 'ltr', 'Español', 'ES-ES'),
    'pt': ('pt-BR', 'ltr', 'Português', 'PT-BR'),
    'fr': ('fr', 'ltr', 'Français', 'FR-FR'),
    'ja': ('ja', 'ltr', '日本語', 'JA-JP'),
    'zh': ('zh-Hans', 'ltr', '简体中文', 'ZH-CN'),
    'ko': ('ko', 'ltr', '한국어', 'KO-KR'),
    'th': ('th', 'ltr', 'ไทย', 'TH-TH'),
    'hi': ('hi', 'ltr', 'हिन्दी', 'HI-IN'),
    'ar': ('ar', 'rtl', 'العربية', 'AR-SA'),
}
RDEL_LANG = {v[3]: k for k, v in LANGS.items()}

MONTHS = {
    'en': 'January February March April May June July August September October November December'.split(),
    'es': 'enero febrero marzo abril mayo junio julio agosto septiembre octubre noviembre diciembre'.split(),
    'pt': 'janeiro fevereiro março abril maio junho julho agosto setembro outubro novembro dezembro'.split(),
    'fr': 'janvier février mars avril mai juin juillet août septembre octobre novembre décembre'.split(),
    'th': 'มกราคม กุมภาพันธ์ มีนาคม เมษายน พฤษภาคม มิถุนายน กรกฎาคม สิงหาคม กันยายน ตุลาคม พฤศจิกายน ธันวาคม'.split(),
    'hi': 'जनवरी फ़रवरी मार्च अप्रैल मई जून जुलाई अगस्त सितंबर अक्तूबर नवंबर दिसंबर'.split(),
    'ar': 'يناير فبراير مارس أبريل مايو يونيو يوليو أغسطس سبتمبر أكتوبر نوفمبر ديسمبر'.split(),
}

def digits(lang, n):
    """A number as the language writes it: Arabic with its own digits, as the Arabic app shows them (@digits)."""
    s = str(n)
    return s.translate(str.maketrans('0123456789', '٠١٢٣٤٥٦٧٨٩')) if lang == 'ar' else s

def date(lang):
    """UPDATED as each language writes a date (Thai counts the Buddhist era's years, as Thai readers expect)."""
    y, m, d = UPDATED
    return {
        'en': '%d %s %d' % (d, MONTHS['en'][m - 1], y),
        'es': '%d de %s de %d' % (d, MONTHS['es'][m - 1], y),
        'pt': '%d de %s de %d' % (d, MONTHS['pt'][m - 1], y),
        'fr': '%d %s %d' % (d, MONTHS['fr'][m - 1], y),
        'ja': '%d年%d月%d日' % (y, m, d),
        'zh': '%d年%d月%d日' % (y, m, d),
        'ko': '%d년 %d월 %d일' % (y, m, d),
        'th': '%d %s %d' % (d, MONTHS['th'][m - 1], y + 543),
        'hi': '%d %s %d' % (d, MONTHS['hi'][m - 1], y),
        'ar': '%s %s %s' % (digits('ar', d), MONTHS['ar'][m - 1], digits('ar', y)),
    }[lang]

# =====================================================================================================================
# TEXT: the paragraphs, a dictionary a language. Placeholders: {app} the app's name; {m[ID]} the app's label for a
# string id; {taught} / {taught_the} the language taught; {about} the app's description; {export_path} Settings › Your
# data › Export; {mlkit_path} the switch's path, {mlkit_where} its section and {mlkit_name} its name; {ink_mb},
# {tr_mb}, {speech_mb} the models' sizes; {mail} the contact link; {lang} the page's language; links' addresses by name.
# Keys starting with q_ are questions, a_ their answers. A study app's own language (ja, zh, ko, th, hi, ar) has only
# the paragraphs its one app uses.
# =====================================================================================================================

TEXT = {}

TEXT['en'] = dict(
    privacy='Privacy policy', help='Help and support', updated='Updated: {date}', langs='Languages', and_=' and ',
    units={'char': ('a character', 'characters'), 'letter': ('a letter', 'letters')},
    tts_engine='Speech Services by Google',
    # --- the privacy policy
    intro_study='{app} is an app for learning to write {taught}, on iPad and Android tablets. It is built to work offline: what you write and learn stays on your device.',
    intro_canvas='{about} It is built to work offline: what you write and draw stays on your device.',
    h_short='In short',
    s_none='{app} has no accounts, no ads and no tracking, and collects nothing about you for itself.',
    s_local='Everything you make in {app} is stored only on your device.',
    s_mlkit_switch='Some features use Google ML Kit, which works on your device but sends Google anonymous diagnostics while it is on. You can switch it off.',
    s_mlkit_canvas='Reading handwriting ({m[ZOOM_READ]}, {m[ZOOM_FIND]}) uses Google ML Kit, which works on your device but sends Google anonymous diagnostics when you use it.',
    s_speech='Your recordings stay on your device. On Android they can be turned into text on the device too, once you have agreed to download the speech model.',
    s_crash='If {app} closes unexpectedly, it offers to email us a short report about it: nothing goes unless you send it.',
    h_keeps='What {app} keeps, and where',
    k_study='{app} stores your pages and handwriting, your notes, your practice and exam history, your review schedule, your vocabulary and lists, your notes on characters, the documents you bring into {m[LIBRARY_TITLE]} and your settings — on your device only. {app} itself never sends them anywhere.',
    k_canvas_rec='{app} stores your notebooks and canvases with everything on them, the PDFs and pictures you bring in, your recordings and their transcripts, and your settings — on your device only. {app} itself never sends them anywhere.',
    k_canvas='{app} stores your projects and canvases with everything on them, the PDFs and pictures you bring in, and your settings — on your device only. {app} itself never sends them anywhere.',
    k_backup='Like any app’s data, they are part of your device’s own backups when you have those on: iCloud Backup on iPad, Google’s backup on Android (which keeps at most 25 MB of each app’s data). Apple and Google keep those under their own privacy policies.',
    k_export='{export_path} puts everything in one file. That file goes only where you choose to send or save it.',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app} uses Google ML Kit to read handwriting ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, exams, searching by drawing), to read printed text ({m[SCAN_TITLE]}, and the pages you scan or bring in as pictures, so that you can search them and lasso their words) and to translate ({m[SCAN_TRANSLATE]}). The reading and the translating happen on your device: your handwriting, your photos, your pages and the text you translate are not sent to Google.',
    ml_uses_noocr='{app} uses Google ML Kit to read handwriting ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, exams, searching by drawing) and to translate ({m[SCAN_TRANSLATE]}). The reading and the translating happen on your device: your handwriting and the text you translate are not sent to Google.',
    ml_dl_ocr='ML Kit downloads its models from Google: the handwriting model (about {ink_mb} MB) as soon as {app} first starts, and a translation model (about {tr_mb} MB per language) the first time you translate. The model that reads printed text comes inside the app.',
    ml_dl_noocr='ML Kit downloads its models from Google: the handwriting model (about {ink_mb} MB) as soon as {app} first starts, and a translation model (about {tr_mb} MB per language) the first time you translate.',
    ml_uses_canvas='{app} uses Google ML Kit for one thing: reading handwriting, when you lasso writing and choose {m[ZOOM_READ]}, and when {m[ZOOM_FIND]} looks for words in what you wrote by hand. The reading happens on your device: your handwriting is not sent to Google.',
    s_can_off='You can switch it off.',
    ml_dl_canvas='ML Kit downloads its handwriting model (about {ink_mb} MB) from Google, for the language the app is shown in: the first time handwriting is read, or when you tap {m[DOWNLOAD]} in {mlkit_where}; never on its own when {app} starts.',
    ml_dl_canvas_noswitch='The first time you use either, ML Kit downloads its handwriting model from Google, for the language the app is shown in.',
    ml_when_on='While ML Kit is on,', ml_when_used='Whenever ML Kit is used,',
    ml_diag='{when} Google receives usage and diagnostics data from it: device information (manufacturer, model, iPadOS or Android version), the app’s identifier and version, a per-installation identifier, performance figures, events (such as model downloads) and error codes, and the language set. Google uses them for diagnostics and usage analytics; they are not linked to your identity, and not used for advertising or tracking. Google lists them for <a href="{mlkit_ios_data}">iPadOS</a> and <a href="{mlkit_android_data}">Android</a>; see also <a href="{google_privacy}">Google’s privacy policy</a> and the <a href="{mlkit_terms}">ML Kit terms</a>.',
    ml_switch_ocr='To switch it all off: {mlkit_path}. {app} then never calls ML Kit: it reads handwriting on its own (a little less accurately), {m[SCAN_TITLE]} and {m[SCAN_TRANSLATE]} are unavailable, and nothing is downloaded or sent to Google through ML Kit.',
    ml_switch_noocr='To switch it all off: {mlkit_path}. {app} then never calls ML Kit: it reads handwriting on its own (a little less accurately), {m[SCAN_TRANSLATE]} is unavailable, and nothing is downloaded or sent to Google through ML Kit.',
    ml_switch_canvas='To switch it off: {mlkit_path}. {m[ZOOM_READ]} and the handwriting search in {m[ZOOM_FIND]} are then unavailable, and nothing is downloaded or sent to Google through ML Kit.',
    ml_noswitch_canvas='{app} has no setting to switch ML Kit off yet. It is used only when you use {m[ZOOM_READ]} or {m[ZOOM_FIND]}: until then, nothing is downloaded or sent to Google through it.',
    h_camera='Camera and photos',
    cam_ocr='{m[SCAN_TITLE]} uses the camera only while you use it, once you allow it. A photo you pick there is handed over by the system on its own: {app} cannot see the rest of your photos. What the camera sees and the photos you pick are read on your device, and are neither kept nor sent.',
    cam_scan='In {m[LIBRARY_TITLE]}, {m[LIBRARY_SCAN]} turns paper pages into a document to read and write on. On iPad it opens Apple’s document camera, once you allow the camera. On Android it opens Google’s document scanner, which runs in Google Play services under Google’s terms, asks for the camera itself and is not covered by the ML Kit switch. Either way, {app} receives only the pages you keep.',
    cam_picker='{m[LIBRARY_PHOTOS]} and {m[LIBRARY_FILES]} use the system’s own pickers: {app} receives only the pictures and files you choose, and never asks for access to your photo library. The pages you scan and the pictures and files you bring in are kept with your documents, on your device.',
    cam_none='{app} does not use the camera.',
    pic_canvas='{m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} and {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_FILES]} use the system’s own pickers: {app} receives only the pictures and files you choose, and never asks for access to your photo library. What you bring in is kept in your canvas, on your device.',
    photos_add='When you export a picture or a video, the system’s share sheet can save it to your photos: on iPad, iPadOS asks you first, and {app} can only add to your photo library, never look into it.',
    h_mic='Microphone and recordings',
    mic='{app} uses the microphone only while you record ({m[ZOOM_AUDIO_RECORD]}), once you allow it, and stops when you leave the canvas or the app. Recordings are kept on your device with the canvas they belong to, are included in {export_path} with everything else, and are never sent anywhere.',
    h_speech='Speech to text',
    speech_android='On Android, {m[ZOOM_LISTEN_TRANSCRIBE]} and {m[ZOOM_LISTEN_LIVE]} turn what a recording says into text, on your device, with Whisper, OpenAI’s open speech model, run by whisper.cpp. The model is not inside the app: the first time, {app} tells you its size (about {speech_mb} MB) and downloads it only if you tap again to agree. It comes from Hugging Face (huggingface.co), where it is published; like any download, it shows your device’s internet address to Hugging Face, under <a href="{hf_privacy}">Hugging Face’s privacy policy</a>. After that, transcription works offline, and neither your recordings nor their text leave the device.',
    speech_ios_none='On iPad, {app} does not turn speech into text yet.',
    h_voice='Reading aloud',
    voice='{app} reads {taught} aloud with your device’s own voices: iPadOS’s on iPad, and on Android the text-to-speech engine chosen in the system settings (such as {tts_engine}), which works under its own terms.',
    h_crash='Crash reports',
    crash='If {app} closes unexpectedly, it writes a short report on your device: the app’s version, the device’s model and system version, where in the app it stopped and the last few things it was doing (such as which screen was open) — never anything you wrote, drew or named. The next time you open {app}, it asks you about it: {m[CRASH_SEND]} opens an email to us with the report in it, for you to read before you send it; {m[CRASH_LATER]} deletes it. Nothing is sent unless you send that email.',
    h_links='Links and email',
    links='{app} opens a web page, an email or a store only when you tap something for it: {m[FAQ_CONTACT]} opens this page or an email to us, and {m[RATE]} takes you to the App Store or Google Play.',
    links_note='A link you write in a note opens in your browser when you tap it.',
    h_stores='The App Store and Google Play',
    stores='Downloading {app}, and any purchase, are handled by Apple (the App Store) or Google (Google Play) under their own privacy policies, as are the crash reports and usage figures your device shares with app developers when you have allowed it in its settings.',
    h_children='Children',
    children='{app} does not knowingly collect personal information from anyone, children included. Apart from the downloads described above, the ML Kit diagnostics are the only data that leave the device on their own, and they do not identify a person.',
    h_changes='Changes',
    changes='If this policy changes, the new version will be published on this page with its date.',
    h_contact='Contact',
    contact_privacy='Questions about privacy: {mail}.',
    # --- the help page
    q_start='Where do I start?',
    a_start_study='Open the menu at the top left: it has everything to study, and {m[REVIEWS]}, {m[VOCAB]}, {m[LIBRARY_TITLE]}, {m[EXAMS]} and {m[STATISTICS]}. Pick {unit} to see its strokes in order, then write it: {app} reads what you write and tells you how well it went. {m[TUTORIAL]}, in the same menu, shows the first steps again.',
    a_start_notes='On the home screen, {m[ZOOM_HOME_NEW_NOTEBOOK]} starts a notebook of pages. Write and draw with the tools on the bar, and zoom in anywhere for more room: there is no end to it. {m[ZOOM_PAGES]} changes the page size, or turns the notebook into one canvas without edges ({m[ZOOM_PAGES_ENDLESS]}).',
    a_start_workshop='On the home screen, {m[ZOOM_HOME_NEW_PROJECT]} starts a project. {m[ZOOM_WELCOME_WORKSHOP_EXAMPLES]} {m[ZOOM_WELCOME_WORKSHOP_BUILD]} {m[ZOOM_WELCOME_WORKSHOP_DRAW]}',
    q_pencil='Do I need an Apple Pencil or a stylus?',
    a_pencil_study='No. With the hand on the toolbar on, one finger writes and two move the page. With an Apple Pencil, or a stylus on Android, {app} switches to it by itself: the pen writes, fingers move the page, and your hand can rest on the screen.',
    a_pencil_canvas='No. With the hand on the toolbar on, one finger draws and two move and zoom the canvas; {app} starts that way. With an Apple Pencil, or a stylus on Android, it switches by itself: the pen draws and fingers move the canvas.',
    q_offline='Does {app} work offline?',
    a_offline_study='Yes. Google ML Kit’s handwriting model (about {ink_mb} MB) downloads once, when {app} first starts, and each language’s model for {m[SCAN_TRANSLATE]} the first time you translate; after that, everything works offline.',
    a_offline_canvas='Yes. It needs the internet only once: the first time you use {m[ZOOM_READ]} or the handwriting search in {m[ZOOM_FIND]}, to download Google ML Kit’s handwriting model.',
    a_offline_speech='On Android, transcribing needs it once too, to download the speech model (about {speech_mb} MB).',
    q_data='Where is my work kept? Can I move it to a new device?',
    a_data='On your device, and in its iCloud Backup or Google backup when those are on. To keep a copy yourself, or to move to another iPad or tablet: {export_path}, then {m[DATA_IMPORT]} on the other one (it replaces what is there). The file works on iPad and Android alike.',
    q_hand='My handwriting is not read well',
    a_hand='In {mlkit_where}, {mlkit_name} should be on and its model downloaded (it needs the internet once). With it off, {app} reads handwriting on its own, less accurately.',
    q_photo='Can {app} read text from a book or a sign?',
    a_photo_ocr='Yes: tap the camera on the toolbar, or press and hold the page and choose {m[CTX_SCAN]}, then point it at {taught} text, or pick a photo. {app} reads each line: {m[SCAN_HOLD]} keeps them, {m[SCAN_WRITE]} puts them on the page as handwriting to trace, and {m[SCAN_TRANSLATE]} shows them in your language.',
    a_photo_noocr='Not yet: Google ML Kit cannot read printed {taught} text, so {m[SCAN_TITLE]} is not available. You can still bring pages in to read and write on: {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} or {m[LIBRARY_PHOTOS]}.',
    q_translate='How do I translate?',
    a_translate='Lasso your writing and choose {m[SCAN_TRANSLATE]} to read it in your language. In {m[VOCAB]}, {m[SCAN_TRANSLATE]} goes the other way: type in your language and get it in {taught}, with its words to save or practise. It all happens on your device: each language’s model (about {tr_mb} MB) downloads once, then works offline.',
    q_voice='There is no voice, or it sounds robotic',
    a_voice='{app} reads aloud with your device’s own voices. On iPad: {voice_ios} On Android, install a {taught} voice for the text-to-speech engine ({tts_engine}, for example) in the system settings; when there is none, {m[VOICE_BUTTON]} in {app} opens them for you.',
    q_reviews='How do reviews work?',
    a_reviews='Mark {units} as {m[STUDYING]}: {m[REVIEWS]} brings each one back when it is due, the next day, then three days later, then longer each time you write it right, and sooner when you do not. New ones join at most ten a day, so marking many at once does not flood the next day. The words you save have their own reviews, in {m[VOCAB]}.',
    q_sheet='Can I print practice sheets?',
    a_sheet='Yes: {m[SHEET]}, in {unit}’s page, in {m[SELECT]} mode or in {m[VOCAB]}, makes a PDF to print or share.',
    q_record='How do I record a lecture with my notes?',
    a_record='Tap the microphone on the bar ({m[ZOOM_AUDIO_RECORD]}) and write: what you write is linked to what is being said at that moment. Later, tap your writing to hear what was said then. {m[ZOOM_AUDIO_RECORDINGS]} lists the canvas’s recordings, to play them, replay the ink as it was written, or delete them.',
    q_transcribe='Can {app} turn a recording into text?',
    a_transcribe='On Android, yes: in {m[ZOOM_AUDIO_RECORDINGS]}, {m[ZOOM_LISTEN_TRANSCRIBE]} writes out what a recording says, and {m[ZOOM_LISTEN_LIVE]} does it while you record; {m[ZOOM_LISTEN_AS_TEXT]} or {m[ZOOM_LISTEN_AS_HAND]} then puts it on the canvas. It all happens on the device, after a one-time download of the speech model (about {speech_mb} MB) that {app} asks you about first. On iPad, not yet.',
    q_totext='Can my handwriting become text?',
    a_totext='Yes: lasso it and choose {m[ZOOM_READ]} (one undo brings the handwriting back). {m[ZOOM_FIND]} finds words in your typed text and in your handwriting, in this canvas and in your others. Both read handwriting with Google ML Kit, on your device, in the language the app is shown in.',
    q_import='How do I bring in a PDF or a photo?',
    a_import='{m[ZOOM_INSERT]} › {m[ZOOM_INSERT_PDF]} opens a PDF as a canvas of its own, to write on. {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} or {m[ZOOM_PICTURE_FILES]} puts pictures on the canvas.',
    q_export='How do I share or print my work?',
    a_export='{m[ZOOM_EXPORT]} offers {exports}. Each is made on your device and handed to the system’s share sheet, to save, print or send.',
    q_privacy='What does {app} send, and to whom?',
    a_privacy_study='Your work stays on your device. Google ML Kit, which reads handwriting and text and translates on the device, downloads its models and sends Google anonymous diagnostics while it is on: {mlkit_path} switches it off.',
    a_privacy_study_noocr='Your work stays on your device. Google ML Kit, which reads handwriting and translates on the device, downloads its models and sends Google anonymous diagnostics while it is on: {mlkit_path} switches it off.',
    a_privacy_canvas='Your work stays on your device. Google ML Kit reads your handwriting on the device for {m[ZOOM_READ]} and {m[ZOOM_FIND]}: when you use them, it downloads its model and sends Google anonymous diagnostics.',
    a_privacy_speech='On Android, transcribing downloads the speech model once, from Hugging Face.',
    a_totext_off='If {m[ZOOM_READ]} is missing, {mlkit_name} is off in {mlkit_where}.',
    a_privacy_switch='To switch ML Kit off: {mlkit_path}.',
    a_privacy_more='Everything else is in the <a href="privacy.html#{lang}">privacy policy</a>.',
    contact_title='Contact',
    contact_help='Write to {mail}. If something goes wrong, tell us your device’s model and system version, and what you were doing.',
)

TEXT['es'] = dict(
    privacy='Política de privacidad', help='Ayuda y soporte', updated='Actualizada: {date}', langs='Idiomas', and_=' y ',
    units={'char': ('un carácter', 'caracteres'), 'letter': ('una letra', 'letras')},
    tts_engine='Servicios de voz de Google',
    intro_study='{app} es una app para aprender a escribir {taught}, en iPad y tabletas Android. Está pensada para funcionar sin conexión: lo que escribes y aprendes se queda en tu dispositivo.',
    intro_canvas='{about} Está pensada para funcionar sin conexión: lo que escribes y dibujas se queda en tu dispositivo.',
    h_short='En resumen',
    s_none='{app} no tiene cuentas, ni anuncios, ni seguimiento, y no recopila nada sobre ti para sí.',
    s_local='Todo lo que creas en {app} se guarda solo en tu dispositivo.',
    s_mlkit_switch='Algunas funciones usan Google ML Kit, que funciona en tu dispositivo pero envía a Google datos de diagnóstico anónimos mientras está activado. Puedes desactivarlo.',
    s_mlkit_canvas='La lectura de la escritura a mano ({m[ZOOM_READ]}, {m[ZOOM_FIND]}) usa Google ML Kit, que funciona en tu dispositivo pero envía a Google datos de diagnóstico anónimos cuando lo usas.',
    s_speech='Tus grabaciones se quedan en tu dispositivo. En Android también se pueden pasar a texto en el propio dispositivo, después de que aceptes descargar el modelo de voz.',
    s_crash='Si {app} se cierra de forma inesperada, te ofrece enviarnos por correo un breve informe: no sale nada si tú no lo envías.',
    h_keeps='Qué guarda {app} y dónde',
    k_study='{app} guarda tus páginas y tu escritura, tus notas, tu historial de práctica y de exámenes, tu calendario de repasos, tu vocabulario y tus listas, tus notas sobre caracteres, los documentos que traes a {m[LIBRARY_TITLE]} y tus ajustes, solo en tu dispositivo. {app} nunca los envía a ningún sitio.',
    k_canvas_rec='{app} guarda tus cuadernos y lienzos con todo lo que hay en ellos, los PDF e imágenes que traes, tus grabaciones y sus transcripciones, y tus ajustes, solo en tu dispositivo. {app} nunca los envía a ningún sitio.',
    k_canvas='{app} guarda tus proyectos y lienzos con todo lo que hay en ellos, los PDF e imágenes que traes y tus ajustes, solo en tu dispositivo. {app} nunca los envía a ningún sitio.',
    k_backup='Como los datos de cualquier app, forman parte de las copias de seguridad de tu dispositivo si las tienes activadas: la copia en iCloud en el iPad y la copia de seguridad de Google en Android (que guarda como mucho 25 MB de datos de cada app). Apple y Google las guardan según sus propias políticas de privacidad.',
    k_export='{export_path} lo pone todo en un archivo. Ese archivo solo va adonde tú decidas enviarlo o guardarlo.',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app} usa Google ML Kit para leer la escritura a mano ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, los exámenes, la búsqueda dibujando), para leer texto impreso ({m[SCAN_TITLE]}, y las páginas que escaneas o traes como imágenes, para que puedas buscar en ellas y seleccionar sus palabras con el lazo) y para traducir ({m[SCAN_TRANSLATE]}). La lectura y la traducción se hacen en tu dispositivo: tu escritura, tus fotos, tus páginas y el texto que traduces no se envían a Google.',
    ml_uses_noocr='{app} usa Google ML Kit para leer la escritura a mano ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, los exámenes, la búsqueda dibujando) y para traducir ({m[SCAN_TRANSLATE]}). La lectura y la traducción se hacen en tu dispositivo: tu escritura y el texto que traduces no se envían a Google.',
    ml_dl_ocr='ML Kit descarga sus modelos de Google: el de escritura (unos {ink_mb} MB) en cuanto abres {app} por primera vez, y uno de traducción (unos {tr_mb} MB por idioma) la primera vez que traduces. El modelo que lee el texto impreso viene dentro de la app.',
    ml_dl_noocr='ML Kit descarga sus modelos de Google: el de escritura (unos {ink_mb} MB) en cuanto abres {app} por primera vez, y uno de traducción (unos {tr_mb} MB por idioma) la primera vez que traduces.',
    ml_uses_canvas='{app} usa Google ML Kit para una sola cosa: leer la escritura a mano, cuando rodeas algo escrito con el lazo y eliges {m[ZOOM_READ]}, y cuando {m[ZOOM_FIND]} busca palabras en lo que escribiste a mano. La lectura se hace en tu dispositivo: tu escritura no se envía a Google.',
    s_can_off='Puedes desactivarlo.',
    ml_dl_canvas='ML Kit descarga de Google su modelo de escritura (unos {ink_mb} MB), para el idioma en que ves la app: la primera vez que se lee escritura a mano, o cuando tocas {m[DOWNLOAD]} en {mlkit_where}; nunca por su cuenta al abrir {app}.',
    ml_dl_canvas_noswitch='La primera vez que usas cualquiera de las dos, ML Kit descarga de Google su modelo de escritura, para el idioma en que ves la app.',
    ml_when_on='Mientras ML Kit está activado,', ml_when_used='Cada vez que se usa ML Kit,',
    ml_diag='{when} Google recibe de él datos de uso y diagnóstico: información del dispositivo (fabricante, modelo, versión de iPadOS o de Android), el identificador y la versión de la app, un identificador por instalación, datos de rendimiento, eventos (como la descarga de modelos) y códigos de error, y el idioma configurado. Google los usa para diagnóstico y analítica de uso; no se vinculan a tu identidad ni se usan para publicidad o seguimiento. Google los detalla para <a href="{mlkit_ios_data}">iPadOS</a> y para <a href="{mlkit_android_data}">Android</a>; consulta también la <a href="{google_privacy}">política de privacidad de Google</a> y las <a href="{mlkit_terms}">condiciones de ML Kit</a>.',
    ml_switch_ocr='Para desactivarlo todo: {mlkit_path}. {app} deja entonces de usar ML Kit: lee la escritura por su cuenta (con algo menos de precisión), {m[SCAN_TITLE]} y {m[SCAN_TRANSLATE]} no están disponibles, y no se descarga ni se envía nada a Google a través de ML Kit.',
    ml_switch_noocr='Para desactivarlo todo: {mlkit_path}. {app} deja entonces de usar ML Kit: lee la escritura por su cuenta (con algo menos de precisión), {m[SCAN_TRANSLATE]} no está disponible, y no se descarga ni se envía nada a Google a través de ML Kit.',
    ml_switch_canvas='Para desactivarlo: {mlkit_path}. {m[ZOOM_READ]} y la búsqueda en la escritura a mano de {m[ZOOM_FIND]} dejan entonces de estar disponibles, y no se descarga ni se envía nada a Google a través de ML Kit.',
    ml_noswitch_canvas='{app} aún no tiene un ajuste para desactivar ML Kit. Solo se usa cuando usas {m[ZOOM_READ]} o {m[ZOOM_FIND]}: hasta entonces, no se descarga ni se envía nada a Google a través de él.',
    h_camera='Cámara y fotos',
    cam_ocr='{m[SCAN_TITLE]} usa la cámara solo mientras lo usas, y solo si lo permites. El sistema entrega por sí solo la foto que eliges ahí: {app} no puede ver el resto de tus fotos. Lo que ve la cámara y las fotos que eliges se leen en tu dispositivo, y no se guardan ni se envían.',
    cam_scan='En {m[LIBRARY_TITLE]}, {m[LIBRARY_SCAN]} convierte páginas de papel en un documento para leer y escribir encima. En el iPad abre la cámara de documentos de Apple, si permites usar la cámara. En Android abre el escáner de documentos de Google, que funciona dentro de los servicios de Google Play según las condiciones de Google, pide él mismo permiso para usar la cámara y no depende del interruptor de ML Kit. En los dos casos, {app} solo recibe las páginas que conservas.',
    cam_picker='{m[LIBRARY_PHOTOS]} y {m[LIBRARY_FILES]} usan los selectores del propio sistema: {app} solo recibe las imágenes y los archivos que eliges, y nunca pide acceso a tu fototeca. Las páginas que escaneas y las imágenes y archivos que traes se guardan con tus documentos, en tu dispositivo.',
    cam_none='{app} no usa la cámara.',
    pic_canvas='{m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} e {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_FILES]} usan los selectores del propio sistema: {app} solo recibe las imágenes y los archivos que eliges, y nunca pide acceso a tu fototeca. Lo que traes se guarda en tu lienzo, en tu dispositivo.',
    photos_add='Cuando exportas una imagen o un vídeo, el menú de compartir del sistema puede guardarlo en tus fotos: en el iPad, iPadOS te lo pregunta antes, y {app} solo puede añadir a tu fototeca, nunca ver lo que hay en ella.',
    h_mic='Micrófono y grabaciones',
    mic='{app} usa el micrófono solo mientras grabas ({m[ZOOM_AUDIO_RECORD]}), y solo si lo permites, y deja de usarlo cuando sales del lienzo o de la app. Las grabaciones se guardan en tu dispositivo con el lienzo al que pertenecen, entran en {export_path} con todo lo demás y nunca se envían a ningún sitio.',
    h_speech='De voz a texto',
    speech_android='En Android, {m[ZOOM_LISTEN_TRANSCRIBE]} y {m[ZOOM_LISTEN_LIVE]} convierten en texto lo que dice una grabación, en tu dispositivo, con Whisper, el modelo de voz abierto de OpenAI, que ejecuta whisper.cpp. El modelo no viene dentro de la app: la primera vez, {app} te dice cuánto ocupa (unos {speech_mb} MB) y solo lo descarga si vuelves a tocar para aceptar. Se descarga de Hugging Face (huggingface.co), donde está publicado; como cualquier descarga, le muestra a Hugging Face la dirección de internet de tu dispositivo, según la <a href="{hf_privacy}">política de privacidad de Hugging Face</a>. Después, la transcripción funciona sin conexión, y ni tus grabaciones ni su texto salen del dispositivo.',
    speech_ios_none='En el iPad, {app} todavía no convierte la voz en texto.',
    h_voice='Lectura en voz alta',
    voice='{app} lee el {taught} en voz alta con las voces de tu propio dispositivo: las de iPadOS en el iPad y, en Android, el motor de texto a voz elegido en los ajustes del sistema (por ejemplo, {tts_engine}), que funciona según sus propias condiciones.',
    h_crash='Informes de fallos',
    crash='Si {app} se cierra de forma inesperada, escribe un breve informe en tu dispositivo: la versión de la app, el modelo del dispositivo y la versión del sistema, dónde se detuvo la app y lo último que estaba haciendo (por ejemplo, qué pantalla estaba abierta); nunca nada de lo que escribiste, dibujaste o nombraste. La siguiente vez que abres {app}, te lo pregunta: {m[CRASH_SEND]} abre un correo para nosotros con el informe dentro, para que lo leas antes de enviarlo; {m[CRASH_LATER]} lo borra. No se envía nada si tú no envías ese correo.',
    h_links='Enlaces y correo',
    links='{app} solo abre una página web, un correo o una tienda cuando tocas algo para ello: {m[FAQ_CONTACT]} abre esta página o un correo para nosotros, y {m[RATE]} te lleva a la App Store o a Google Play.',
    links_note='Un enlace que escribes en una nota se abre en tu navegador cuando lo tocas.',
    h_stores='La App Store y Google Play',
    stores='La descarga de {app} y cualquier compra las gestionan Apple (la App Store) o Google (Google Play) según sus propias políticas de privacidad, igual que los informes de fallos y los datos de uso que tu dispositivo comparte con los desarrolladores si lo has permitido en sus ajustes.',
    h_children='Menores',
    children='{app} no recopila a sabiendas información personal de nadie, tampoco de menores. Aparte de las descargas descritas arriba, los diagnósticos de ML Kit son los únicos datos que salen del dispositivo por sí solos, y no identifican a ninguna persona.',
    h_changes='Cambios',
    changes='Si esta política cambia, la nueva versión se publicará en esta página con su fecha.',
    h_contact='Contacto',
    contact_privacy='Preguntas sobre privacidad: {mail}.',
    q_start='¿Por dónde empiezo?',
    a_start_study='Abre el menú de arriba a la izquierda: tiene todo para estudiar, además de {m[REVIEWS]}, {m[VOCAB]}, {m[LIBRARY_TITLE]}, {m[EXAMS]} y {m[STATISTICS]}. Elige {unit} para ver sus trazos en orden y luego escribe tú: {app} lee lo que escribes y te dice qué tal te ha salido. {m[TUTORIAL]}, en el mismo menú, vuelve a mostrarte los primeros pasos.',
    a_start_notes='En la pantalla de inicio, {m[ZOOM_HOME_NEW_NOTEBOOK]} crea un cuaderno de páginas. Escribe y dibuja con las herramientas de la barra, y acércate donde quieras para tener más sitio: no tiene fin. {m[ZOOM_PAGES]} cambia el tamaño de las páginas, o convierte el cuaderno en un lienzo sin bordes ({m[ZOOM_PAGES_ENDLESS]}).',
    a_start_workshop='En la pantalla de inicio, {m[ZOOM_HOME_NEW_PROJECT]} crea un proyecto. {m[ZOOM_WELCOME_WORKSHOP_EXAMPLES]} {m[ZOOM_WELCOME_WORKSHOP_BUILD]} {m[ZOOM_WELCOME_WORKSHOP_DRAW]}',
    q_pencil='¿Necesito un Apple Pencil o un lápiz óptico?',
    a_pencil_study='No. Con la mano de la barra activada, un dedo escribe y dos mueven la página. Si usas un Apple Pencil, o un lápiz óptico en Android, {app} cambia a él automáticamente: escribe el lápiz, los dedos mueven la página y puedes apoyar la mano en la pantalla.',
    a_pencil_canvas='No. Con la mano de la barra activada, un dedo dibuja y dos mueven el lienzo y hacen zoom; así empieza {app}. Si usas un Apple Pencil, o un lápiz óptico en Android, cambia a él automáticamente: dibuja el lápiz y los dedos mueven el lienzo.',
    q_offline='¿{app} funciona sin conexión?',
    a_offline_study='Sí. El modelo de escritura de Google ML Kit (unos {ink_mb} MB) se descarga una vez, la primera vez que abres {app}, y el modelo de cada idioma para {m[SCAN_TRANSLATE]}, la primera vez que traduces; después, todo funciona sin conexión.',
    a_offline_canvas='Sí. Solo hace falta internet una vez: la primera vez que usas {m[ZOOM_READ]} o la búsqueda en la escritura a mano de {m[ZOOM_FIND]}, para descargar el modelo de escritura de Google ML Kit.',
    a_offline_speech='En Android, transcribir también lo necesita una vez, para descargar el modelo de voz (unos {speech_mb} MB).',
    q_data='¿Dónde se guarda lo que hago? ¿Puedo pasarlo a otro dispositivo?',
    a_data='En tu dispositivo, y en su copia en iCloud o de Google si las tienes activadas. Para guardar una copia por tu cuenta, o para pasarlo a otro iPad o tableta: {export_path}, y luego {m[DATA_IMPORT]} en el otro (sustituye lo que haya en él). El archivo sirve igual en iPad que en Android.',
    q_hand='No reconoce bien mi escritura',
    a_hand='En {mlkit_where}, {mlkit_name} debe estar activado y su modelo descargado (necesita internet una vez). Desactivado, {app} lee la escritura por su cuenta, con menos precisión.',
    q_photo='¿{app} puede leer el texto de un libro o de un cartel?',
    a_photo_ocr='Sí: toca la cámara de la barra, o mantén pulsada la página y elige {m[CTX_SCAN]}, y apunta a un texto en {taught}, o elige una foto. {app} lee cada línea: {m[SCAN_HOLD]} las conserva, {m[SCAN_WRITE]} las pone en la página como escritura a mano para repasar, y {m[SCAN_TRANSLATE]} las muestra en tu idioma.',
    a_photo_noocr='Todavía no: Google ML Kit no sabe leer texto impreso en {taught}, así que {m[SCAN_TITLE]} no está disponible. Aun así, puedes traer páginas para leer y escribir encima: {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} o {m[LIBRARY_PHOTOS]}.',
    q_translate='¿Cómo traduzco?',
    a_translate='Rodea tu escritura con el lazo y elige {m[SCAN_TRANSLATE]} para leerla en tu idioma. En {m[VOCAB]}, {m[SCAN_TRANSLATE]} va al revés: escribe en tu idioma y obtenlo en {taught}, con sus palabras para guardar o practicar. Todo se hace en tu dispositivo: el modelo de cada idioma (unos {tr_mb} MB) se descarga una vez y después funciona sin conexión.',
    q_voice='No hay voz, o suena robótica',
    a_voice='{app} lee en voz alta con las voces de tu dispositivo. En el iPad: {voice_ios} En Android, instala una voz en {taught} para el motor de texto a voz (por ejemplo, {tts_engine}) en los ajustes del sistema; si no hay ninguna, {m[VOICE_BUTTON]}, en {app}, te los abre.',
    q_reviews='¿Cómo funcionan los repasos?',
    a_reviews='Marca {units} como {m[STUDYING]}: {m[REVIEWS]} te los trae cuando toca, al día siguiente, luego a los tres días, y cada vez más tarde si los escribes bien; antes si no. Los nuevos entran como mucho de diez en diez al día, para que marcar muchos a la vez no te llene el día siguiente. Las palabras que guardas tienen sus propios repasos, en {m[VOCAB]}.',
    q_sheet='¿Puedo imprimir hojas de práctica?',
    a_sheet='Sí: {m[SHEET]}, en la página de {unit}, en el modo {m[SELECT]} o en {m[VOCAB]}, crea un PDF para imprimir o compartir.',
    q_record='¿Cómo grabo una clase junto con mis notas?',
    a_record='Toca el micrófono de la barra ({m[ZOOM_AUDIO_RECORD]}) y escribe: lo que escribes queda unido a lo que se dice en ese momento. Después, toca lo que escribiste para oír lo que se decía entonces. {m[ZOOM_AUDIO_RECORDINGS]} muestra las grabaciones del lienzo, para escucharlas, ver cómo se escribió la tinta o borrarlas.',
    q_transcribe='¿{app} puede pasar una grabación a texto?',
    a_transcribe='En Android, sí: en {m[ZOOM_AUDIO_RECORDINGS]}, {m[ZOOM_LISTEN_TRANSCRIBE]} escribe lo que dice una grabación, y {m[ZOOM_LISTEN_LIVE]} lo hace mientras grabas; luego {m[ZOOM_LISTEN_AS_TEXT]} o {m[ZOOM_LISTEN_AS_HAND]} lo pone en el lienzo. Todo se hace en el dispositivo, tras descargar una sola vez el modelo de voz (unos {speech_mb} MB), sobre lo que {app} te pregunta antes. En el iPad, todavía no.',
    q_totext='¿Mi escritura a mano puede pasar a texto?',
    a_totext='Sí: rodéala con el lazo y elige {m[ZOOM_READ]} (con deshacer vuelve la escritura). {m[ZOOM_FIND]} encuentra palabras en tus textos y en tu escritura a mano, en este lienzo y en los demás. Los dos leen la escritura con Google ML Kit, en tu dispositivo, en el idioma en que ves la app.',
    q_import='¿Cómo traigo un PDF o una foto?',
    a_import='{m[ZOOM_INSERT]} › {m[ZOOM_INSERT_PDF]} abre un PDF como un lienzo propio, para escribir encima. {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} o {m[ZOOM_PICTURE_FILES]} pone imágenes en el lienzo.',
    q_export='¿Cómo comparto o imprimo lo que hago?',
    a_export='{m[ZOOM_EXPORT]} ofrece {exports}. Cada uno se crea en tu dispositivo y pasa al menú de compartir del sistema, para guardarlo, imprimirlo o enviarlo.',
    q_privacy='¿Qué envía {app}, y a quién?',
    a_privacy_study='Lo que haces se queda en tu dispositivo. Google ML Kit, que lee la escritura y el texto y traduce en el dispositivo, descarga sus modelos y envía a Google diagnósticos anónimos mientras está activado: {mlkit_path} lo desactiva.',
    a_privacy_study_noocr='Lo que haces se queda en tu dispositivo. Google ML Kit, que lee la escritura y traduce en el dispositivo, descarga sus modelos y envía a Google diagnósticos anónimos mientras está activado: {mlkit_path} lo desactiva.',
    a_privacy_canvas='Lo que haces se queda en tu dispositivo. Google ML Kit lee tu escritura a mano en el dispositivo para {m[ZOOM_READ]} y {m[ZOOM_FIND]}: cuando los usas, descarga su modelo y envía a Google diagnósticos anónimos.',
    a_privacy_speech='En Android, transcribir descarga una vez el modelo de voz, de Hugging Face.',
    a_totext_off='Si no aparece {m[ZOOM_READ]}, es que {mlkit_name} está desactivado en {mlkit_where}.',
    a_privacy_switch='Para desactivar ML Kit: {mlkit_path}.',
    a_privacy_more='Todo lo demás está en la <a href="privacy.html#{lang}">política de privacidad</a>.',
    contact_title='Contacto',
    contact_help='Escribe a {mail}. Si algo falla, cuéntanos el modelo de tu dispositivo, la versión del sistema y qué estabas haciendo.',
)

TEXT['pt'] = dict(
    privacy='Política de privacidade', help='Ajuda e suporte', updated='Atualizada em {date}', langs='Idiomas', and_=' e ',
    units={'char': ('um caractere', 'caracteres'), 'letter': ('uma letra', 'letras')},
    tts_engine='Serviços de fala do Google',
    intro_study='O {app} é um app para aprender a escrever {taught}, no iPad e em tablets Android. Ele foi feito para funcionar offline: o que você escreve e aprende fica no seu dispositivo.',
    intro_canvas='{about} Ele foi feito para funcionar offline: o que você escreve e desenha fica no seu dispositivo.',
    h_short='Em resumo',
    s_none='O {app} não tem contas, anúncios nem rastreamento, e não coleta nada sobre você para si.',
    s_local='Tudo o que você cria no {app} fica guardado só no seu dispositivo.',
    s_mlkit_switch='Alguns recursos usam o Google ML Kit, que funciona no seu dispositivo mas envia ao Google dados de diagnóstico anônimos enquanto está ligado. Você pode desligá-lo.',
    s_mlkit_canvas='A leitura da escrita à mão ({m[ZOOM_READ]}, {m[ZOOM_FIND]}) usa o Google ML Kit, que funciona no seu dispositivo mas envia ao Google dados de diagnóstico anônimos quando você o usa.',
    s_speech='Suas gravações ficam no seu dispositivo. No Android, elas também podem ser transcritas no próprio dispositivo, depois que você aceita baixar o modelo de voz.',
    s_crash='Se o {app} fechar inesperadamente, ele oferece mandar para nós um relatório curto por e-mail: nada sai se você não enviar.',
    h_keeps='O que o {app} guarda, e onde',
    k_study='O {app} guarda suas páginas e sua escrita, suas notas, seu histórico de prática e de testes, sua agenda de revisões, seu vocabulário e suas listas, suas notas sobre caracteres, os documentos que você traz para {m[LIBRARY_TITLE]} e suas configurações — só no seu dispositivo. O próprio {app} nunca os envia a lugar nenhum.',
    k_canvas_rec='O {app} guarda seus cadernos e telas com tudo o que há neles, os PDFs e imagens que você traz, suas gravações e as transcrições delas, e suas configurações — só no seu dispositivo. O próprio {app} nunca os envia a lugar nenhum.',
    k_canvas='O {app} guarda seus projetos e telas com tudo o que há neles, os PDFs e imagens que você traz e suas configurações — só no seu dispositivo. O próprio {app} nunca os envia a lugar nenhum.',
    k_backup='Como os dados de qualquer app, eles fazem parte dos backups do seu dispositivo quando estão ativados: o Backup do iCloud no iPad e o backup do Google no Android (que guarda no máximo 25 MB de dados de cada app). A Apple e o Google os guardam segundo as próprias políticas de privacidade.',
    k_export='{export_path} coloca tudo em um arquivo. Esse arquivo só vai para onde você escolher enviá-lo ou salvá-lo.',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='O {app} usa o Google ML Kit para ler a escrita à mão ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, os testes, a busca desenhando), para ler texto impresso ({m[SCAN_TITLE]}, e as páginas que você escaneia ou traz como imagens, para poder buscar nelas e selecionar suas palavras com o laço) e para traduzir ({m[SCAN_TRANSLATE]}). A leitura e a tradução acontecem no seu dispositivo: sua escrita, suas fotos, suas páginas e o texto que você traduz não são enviados ao Google.',
    ml_uses_noocr='O {app} usa o Google ML Kit para ler a escrita à mão ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, os testes, a busca desenhando) e para traduzir ({m[SCAN_TRANSLATE]}). A leitura e a tradução acontecem no seu dispositivo: sua escrita e o texto que você traduz não são enviados ao Google.',
    ml_dl_ocr='O ML Kit baixa seus modelos do Google: o de escrita (cerca de {ink_mb} MB) assim que o {app} é aberto pela primeira vez, e um de tradução (cerca de {tr_mb} MB por idioma) na primeira vez que você traduz. O modelo que lê texto impresso já vem dentro do app.',
    ml_dl_noocr='O ML Kit baixa seus modelos do Google: o de escrita (cerca de {ink_mb} MB) assim que o {app} é aberto pela primeira vez, e um de tradução (cerca de {tr_mb} MB por idioma) na primeira vez que você traduz.',
    ml_uses_canvas='O {app} usa o Google ML Kit para uma coisa só: ler a escrita à mão, quando você seleciona algo escrito com o laço e escolhe {m[ZOOM_READ]}, e quando {m[ZOOM_FIND]} procura palavras no que você escreveu à mão. A leitura acontece no seu dispositivo: sua escrita não é enviada ao Google.',
    s_can_off='Você pode desligá-lo.',
    ml_dl_canvas='O ML Kit baixa do Google seu modelo de escrita (cerca de {ink_mb} MB), para o idioma em que o app está: na primeira vez que uma escrita à mão é lida, ou quando você toca em {m[DOWNLOAD]} em {mlkit_where}; nunca por conta própria quando o {app} é aberto.',
    ml_dl_canvas_noswitch='Na primeira vez que você usa qualquer um dos dois, o ML Kit baixa do Google seu modelo de escrita, para o idioma em que o app está.',
    ml_when_on='Enquanto o ML Kit está ligado,', ml_when_used='Sempre que o ML Kit é usado,',
    ml_diag='{when} o Google recebe dele dados de uso e diagnóstico: informações do dispositivo (fabricante, modelo, versão do iPadOS ou do Android), o identificador e a versão do app, um identificador por instalação, dados de desempenho, eventos (como o download de modelos) e códigos de erro, e o idioma configurado. O Google os usa para diagnóstico e análise de uso; eles não são vinculados à sua identidade nem usados para publicidade ou rastreamento. O Google os detalha para <a href="{mlkit_ios_data}">iPadOS</a> e para <a href="{mlkit_android_data}">Android</a>; veja também a <a href="{google_privacy}">política de privacidade do Google</a> e os <a href="{mlkit_terms}">termos do ML Kit</a>.',
    ml_switch_ocr='Para desligar tudo: {mlkit_path}. O {app} deixa então de usar o ML Kit: lê a escrita por conta própria (com um pouco menos de precisão), {m[SCAN_TITLE]} e {m[SCAN_TRANSLATE]} ficam indisponíveis, e nada é baixado ou enviado ao Google pelo ML Kit.',
    ml_switch_noocr='Para desligar tudo: {mlkit_path}. O {app} deixa então de usar o ML Kit: lê a escrita por conta própria (com um pouco menos de precisão), {m[SCAN_TRANSLATE]} fica indisponível, e nada é baixado ou enviado ao Google pelo ML Kit.',
    ml_switch_canvas='Para desligá-lo: {mlkit_path}. {m[ZOOM_READ]} e a busca na escrita à mão de {m[ZOOM_FIND]} ficam então indisponíveis, e nada é baixado ou enviado ao Google pelo ML Kit.',
    ml_noswitch_canvas='O {app} ainda não tem uma configuração para desligar o ML Kit. Ele só é usado quando você usa {m[ZOOM_READ]} ou {m[ZOOM_FIND]}: até lá, nada é baixado ou enviado ao Google por ele.',
    h_camera='Câmera e fotos',
    cam_ocr='{m[SCAN_TITLE]} usa a câmera só enquanto você o usa, e só se você permitir. O sistema entrega apenas a foto que você escolhe ali: o {app} não vê o resto das suas fotos. O que a câmera vê e as fotos que você escolhe são lidos no seu dispositivo, e não são guardados nem enviados.',
    cam_scan='Em {m[LIBRARY_TITLE]}, {m[LIBRARY_SCAN]} transforma páginas de papel em um documento para ler e escrever por cima. No iPad, abre a câmera de documentos da Apple, se você permitir o uso da câmera. No Android, abre o scanner de documentos do Google, que funciona dentro do Google Play Services segundo os termos do Google, pede ele mesmo a permissão da câmera e não depende da chave do ML Kit. Nos dois casos, o {app} recebe só as páginas que você mantém.',
    cam_picker='{m[LIBRARY_PHOTOS]} e {m[LIBRARY_FILES]} usam os seletores do próprio sistema: o {app} recebe só as imagens e os arquivos que você escolhe, e nunca pede acesso à sua biblioteca de fotos. As páginas que você escaneia e as imagens e arquivos que traz ficam guardados com seus documentos, no seu dispositivo.',
    cam_none='O {app} não usa a câmera.',
    pic_canvas='{m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} e {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_FILES]} usam os seletores do próprio sistema: o {app} recebe só as imagens e os arquivos que você escolhe, e nunca pede acesso à sua biblioteca de fotos. O que você traz fica guardado na sua tela, no seu dispositivo.',
    photos_add='Quando você exporta uma imagem ou um vídeo, o menu de compartilhamento do sistema pode salvá-lo nas suas fotos: no iPad, o iPadOS pergunta antes, e o {app} só pode adicionar à sua biblioteca de fotos, nunca olhar o que há nela.',
    h_mic='Microfone e gravações',
    mic='O {app} usa o microfone só enquanto você grava ({m[ZOOM_AUDIO_RECORD]}), e só se você permitir, e para quando você sai da tela de desenho ou do app. As gravações ficam no seu dispositivo, com a tela a que pertencem, entram em {export_path} com todo o resto e nunca são enviadas a lugar nenhum.',
    h_speech='Voz para texto',
    speech_android='No Android, {m[ZOOM_LISTEN_TRANSCRIBE]} e {m[ZOOM_LISTEN_LIVE]} transformam em texto o que uma gravação diz, no seu dispositivo, com o Whisper, o modelo de voz aberto da OpenAI, executado pelo whisper.cpp. O modelo não vem dentro do app: na primeira vez, o {app} mostra o tamanho dele (cerca de {speech_mb} MB) e só o baixa se você tocar de novo para aceitar. Ele vem do Hugging Face (huggingface.co), onde é publicado; como qualquer download, mostra ao Hugging Face o endereço de internet do seu dispositivo, segundo a <a href="{hf_privacy}">política de privacidade do Hugging Face</a>. Depois disso, a transcrição funciona offline, e nem suas gravações nem o texto delas saem do dispositivo.',
    speech_ios_none='No iPad, o {app} ainda não transforma voz em texto.',
    h_voice='Leitura em voz alta',
    voice='O {app} lê {taught} em voz alta com as vozes do seu próprio dispositivo: as do iPadOS no iPad e, no Android, o mecanismo de conversão de texto em voz escolhido nas configurações do sistema (por exemplo, {tts_engine}), que funciona segundo os próprios termos.',
    h_crash='Relatórios de falhas',
    crash='Se o {app} fechar inesperadamente, ele grava um relatório curto no seu dispositivo: a versão do app, o modelo do dispositivo e a versão do sistema, onde o app parou e as últimas coisas que estava fazendo (por exemplo, qual tela estava aberta) — nunca nada do que você escreveu, desenhou ou nomeou. Na próxima vez que você abrir o {app}, ele pergunta: {m[CRASH_SEND]} abre um e-mail para nós com o relatório, para você ler antes de enviar; {m[CRASH_LATER]} o apaga. Nada é enviado se você não enviar esse e-mail.',
    h_links='Links e e-mail',
    links='O {app} só abre uma página da web, um e-mail ou uma loja quando você toca em algo para isso: {m[FAQ_CONTACT]} abre esta página ou um e-mail para nós, e {m[RATE]} leva você à App Store ou ao Google Play.',
    links_note='Um link que você escreve numa nota abre no navegador quando você toca nele.',
    h_stores='A App Store e o Google Play',
    stores='O download do {app} e qualquer compra são tratados pela Apple (App Store) ou pelo Google (Google Play), segundo as próprias políticas de privacidade, assim como os relatórios de falhas e os dados de uso que seu dispositivo compartilha com desenvolvedores quando você permite isso nas configurações.',
    h_children='Crianças',
    children='O {app} não coleta conscientemente informações pessoais de ninguém, incluindo crianças. Além dos downloads descritos acima, os diagnósticos do ML Kit são os únicos dados que saem do dispositivo por conta própria, e não identificam ninguém.',
    h_changes='Mudanças',
    changes='Se esta política mudar, a nova versão será publicada nesta página com a data.',
    h_contact='Contato',
    contact_privacy='Dúvidas sobre privacidade: {mail}.',
    q_start='Por onde eu começo?',
    a_start_study='Abra o menu no canto superior esquerdo: ele tem tudo para estudar, além de {m[REVIEWS]}, {m[VOCAB]}, {m[LIBRARY_TITLE]}, {m[EXAMS]} e {m[STATISTICS]}. Escolha {unit} para ver os traços na ordem e depois escreva você: o {app} lê o que você escreve e diz como foi. {m[TUTORIAL]}, no mesmo menu, mostra de novo os primeiros passos.',
    a_start_notes='Na tela inicial, {m[ZOOM_HOME_NEW_NOTEBOOK]} cria um caderno de páginas. Escreva e desenhe com as ferramentas da barra, e aproxime onde quiser para ter mais espaço: não tem fim. {m[ZOOM_PAGES]} muda o tamanho das páginas, ou transforma o caderno numa tela sem bordas ({m[ZOOM_PAGES_ENDLESS]}).',
    a_start_workshop='Na tela inicial, {m[ZOOM_HOME_NEW_PROJECT]} cria um projeto. {m[ZOOM_WELCOME_WORKSHOP_EXAMPLES]} {m[ZOOM_WELCOME_WORKSHOP_BUILD]} {m[ZOOM_WELCOME_WORKSHOP_DRAW]}',
    q_pencil='Preciso de um Apple Pencil ou de uma caneta?',
    a_pencil_study='Não. Com a mão da barra ligada, um dedo escreve e dois movem a página. Se você usar um Apple Pencil, ou uma caneta no Android, o {app} muda para ela sozinho: a caneta escreve, os dedos movem a página e você pode apoiar a mão na tela.',
    a_pencil_canvas='Não. Com a mão da barra ligada, um dedo desenha e dois movem e aproximam a tela; o {app} começa assim. Se você usar um Apple Pencil, ou uma caneta no Android, ele muda sozinho: a caneta desenha e os dedos movem a tela.',
    q_offline='O {app} funciona offline?',
    a_offline_study='Sim. O modelo de escrita do Google ML Kit (cerca de {ink_mb} MB) é baixado uma vez, na primeira vez que o {app} é aberto, e o modelo de cada idioma para {m[SCAN_TRANSLATE]}, na primeira vez que você traduz; depois disso, tudo funciona offline.',
    a_offline_canvas='Sim. A internet só é necessária uma vez: na primeira vez que você usa {m[ZOOM_READ]} ou a busca na escrita à mão de {m[ZOOM_FIND]}, para baixar o modelo de escrita do Google ML Kit.',
    a_offline_speech='No Android, transcrever também precisa dela uma vez, para baixar o modelo de voz (cerca de {speech_mb} MB).',
    q_data='Onde fica o que eu faço? Posso passar para outro dispositivo?',
    a_data='No seu dispositivo, e no backup do iCloud ou do Google quando estão ativados. Para guardar uma cópia você mesmo, ou para passar para outro iPad ou tablet: {export_path}, e depois {m[DATA_IMPORT]} no outro (substitui o que houver nele). O arquivo serve tanto no iPad quanto no Android.',
    q_hand='Minha escrita não é bem reconhecida',
    a_hand='Em {mlkit_where}, {mlkit_name} deve estar ligado e o modelo baixado (precisa de internet uma vez). Desligado, o {app} lê a escrita por conta própria, com menos precisão.',
    q_photo='O {app} consegue ler o texto de um livro ou de uma placa?',
    a_photo_ocr='Sim: toque na câmera da barra, ou toque e segure a página e escolha {m[CTX_SCAN]}, e aponte para um texto em {taught}, ou escolha uma foto. O {app} lê cada linha: {m[SCAN_HOLD]} as mantém, {m[SCAN_WRITE]} as coloca na página como escrita à mão para cobrir, e {m[SCAN_TRANSLATE]} as mostra no seu idioma.',
    a_photo_noocr='Ainda não: o Google ML Kit não sabe ler texto impresso em {taught}, então {m[SCAN_TITLE]} não está disponível. Mesmo assim, você pode trazer páginas para ler e escrever por cima: {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} ou {m[LIBRARY_PHOTOS]}.',
    q_translate='Como eu traduzo?',
    a_translate='Selecione sua escrita com o laço e escolha {m[SCAN_TRANSLATE]} para lê-la no seu idioma. Em {m[VOCAB]}, {m[SCAN_TRANSLATE]} faz o caminho inverso: digite no seu idioma e receba em {taught}, com as palavras para salvar ou praticar. Tudo acontece no seu dispositivo: o modelo de cada idioma (cerca de {tr_mb} MB) é baixado uma vez e depois funciona offline.',
    q_voice='Não tem voz, ou ela soa robótica',
    a_voice='O {app} lê em voz alta com as vozes do seu dispositivo. No iPad: {voice_ios} No Android, instale uma voz em {taught} para o mecanismo de conversão de texto em voz (por exemplo, {tts_engine}) nas configurações do sistema; se não houver nenhuma, {m[VOICE_BUTTON]}, no {app}, as abre para você.',
    q_reviews='Como funcionam as revisões?',
    a_reviews='Marque {units} como {m[STUDYING]}: {m[REVIEWS]} traz cada um de volta no momento certo, no dia seguinte, depois em três dias, e cada vez mais tarde quando você acerta; mais cedo quando erra. Os novos entram no máximo dez por dia, para que marcar muitos de uma vez não lote o dia seguinte. As palavras que você salva têm revisões próprias, em {m[VOCAB]}.',
    q_sheet='Posso imprimir folhas de prática?',
    a_sheet='Sim: {m[SHEET]}, na página de {unit}, no modo {m[SELECT]} ou em {m[VOCAB]}, cria um PDF para imprimir ou compartilhar.',
    q_record='Como gravo uma aula junto com minhas notas?',
    a_record='Toque no microfone da barra ({m[ZOOM_AUDIO_RECORD]}) e escreva: o que você escreve fica ligado ao que está sendo dito naquele momento. Depois, toque no que escreveu para ouvir o que era dito então. {m[ZOOM_AUDIO_RECORDINGS]} mostra as gravações da tela, para ouvi-las, rever a tinta sendo escrita ou excluí-las.',
    q_transcribe='O {app} consegue transformar uma gravação em texto?',
    a_transcribe='No Android, sim: em {m[ZOOM_AUDIO_RECORDINGS]}, {m[ZOOM_LISTEN_TRANSCRIBE]} escreve o que uma gravação diz, e {m[ZOOM_LISTEN_LIVE]} faz isso enquanto você grava; depois, {m[ZOOM_LISTEN_AS_TEXT]} ou {m[ZOOM_LISTEN_AS_HAND]} coloca o texto na tela. Tudo acontece no dispositivo, depois de baixar uma única vez o modelo de voz (cerca de {speech_mb} MB), o que o {app} pergunta antes. No iPad, ainda não.',
    q_totext='Minha escrita à mão pode virar texto?',
    a_totext='Sim: selecione-a com o laço e escolha {m[ZOOM_READ]} (desfazer traz a escrita de volta). {m[ZOOM_FIND]} encontra palavras nos seus textos e na sua escrita à mão, nesta tela e nas outras. Os dois leem a escrita com o Google ML Kit, no seu dispositivo, no idioma em que o app está.',
    q_import='Como trago um PDF ou uma foto?',
    a_import='{m[ZOOM_INSERT]} › {m[ZOOM_INSERT_PDF]} abre um PDF como uma tela própria, para escrever por cima. {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} ou {m[ZOOM_PICTURE_FILES]} coloca imagens na tela.',
    q_export='Como compartilho ou imprimo o que faço?',
    a_export='{m[ZOOM_EXPORT]} oferece {exports}. Cada um é feito no seu dispositivo e vai para o menu de compartilhamento do sistema, para salvar, imprimir ou enviar.',
    q_privacy='O que o {app} envia, e para quem?',
    a_privacy_study='O que você faz fica no seu dispositivo. O Google ML Kit, que lê a escrita e o texto e traduz no dispositivo, baixa seus modelos e envia ao Google diagnósticos anônimos enquanto está ligado: {mlkit_path} o desliga.',
    a_privacy_study_noocr='O que você faz fica no seu dispositivo. O Google ML Kit, que lê a escrita e traduz no dispositivo, baixa seus modelos e envia ao Google diagnósticos anônimos enquanto está ligado: {mlkit_path} o desliga.',
    a_privacy_canvas='O que você faz fica no seu dispositivo. O Google ML Kit lê sua escrita à mão no dispositivo para {m[ZOOM_READ]} e {m[ZOOM_FIND]}: quando você os usa, ele baixa seu modelo e envia ao Google diagnósticos anônimos.',
    a_privacy_speech='No Android, transcrever baixa o modelo de voz uma vez, do Hugging Face.',
    a_totext_off='Se {m[ZOOM_READ]} não aparecer, {mlkit_name} está desligado em {mlkit_where}.',
    a_privacy_switch='Para desligar o ML Kit: {mlkit_path}.',
    a_privacy_more='Todo o resto está na <a href="privacy.html#{lang}">política de privacidade</a>.',
    contact_title='Contato',
    contact_help='Escreva para {mail}. Se algo der errado, informe o modelo do seu dispositivo, a versão do sistema e o que você estava fazendo.',
)

TEXT['fr'] = dict(
    privacy='Politique de confidentialité', help='Aide et assistance', updated='Mise à jour : {date}', langs='Langues', and_=' et ',
    units={'char': ('un caractère', 'des caractères'), 'letter': ('une lettre', 'des lettres')},
    tts_engine='Services vocaux de Google',
    intro_study='{app} est une app pour apprendre à écrire {taught_the}, sur iPad et tablettes Android. Elle est conçue pour fonctionner hors ligne : ce que vous écrivez et apprenez reste sur votre appareil.',
    intro_canvas='{about} L’app est conçue pour fonctionner hors ligne : ce que vous écrivez et dessinez reste sur votre appareil.',
    h_short='En bref',
    s_none='{app} n’a ni compte, ni publicité, ni pistage, et ne recueille rien sur vous pour son propre compte.',
    s_local='Tout ce que vous créez dans {app} est enregistré uniquement sur votre appareil.',
    s_mlkit_switch='Certaines fonctions utilisent Google ML Kit, qui fonctionne sur votre appareil mais envoie à Google des données de diagnostic anonymes tant qu’il est activé. Vous pouvez le désactiver.',
    s_mlkit_canvas='La lecture de l’écriture manuscrite ({m[ZOOM_READ]}, {m[ZOOM_FIND]}) utilise Google ML Kit, qui fonctionne sur votre appareil mais envoie à Google des données de diagnostic anonymes quand vous vous en servez.',
    s_speech='Vos enregistrements restent sur votre appareil. Sur Android, ils peuvent aussi être transcrits sur l’appareil même, une fois que vous avez accepté de télécharger le modèle vocal.',
    s_crash='Si {app} se ferme de façon inattendue, l’app vous propose de nous envoyer un court rapport par e-mail : rien ne part si vous ne l’envoyez pas.',
    h_keeps='Ce {que_app} conserve, et où',
    k_study='{app} conserve vos pages et votre écriture, vos notes, votre historique d’entraînement et d’examens, votre calendrier de révisions, votre vocabulaire et vos listes, vos notes sur les caractères, les documents que vous ajoutez dans {m[LIBRARY_TITLE]} et vos réglages — uniquement sur votre appareil. L’app elle-même ne les envoie nulle part.',
    k_canvas_rec='{app} conserve vos cahiers et vos toiles avec tout ce qu’ils contiennent, les PDF et images que vous importez, vos enregistrements et leurs transcriptions, et vos réglages — uniquement sur votre appareil. L’app elle-même ne les envoie nulle part.',
    k_canvas='{app} conserve vos projets et vos toiles avec tout ce qu’ils contiennent, les PDF et images que vous importez, et vos réglages — uniquement sur votre appareil. L’app elle-même ne les envoie nulle part.',
    k_backup='Comme les données de toute app, elles font partie des sauvegardes de votre appareil si elles sont activées : la sauvegarde iCloud sur iPad, la sauvegarde Google sur Android (qui garde au plus 25 Mo de données par app). Apple et Google les conservent selon leur propre politique de confidentialité.',
    k_export='{export_path} rassemble tout dans un fichier. Ce fichier ne va que là où vous choisissez de l’envoyer ou de l’enregistrer.',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app} utilise Google ML Kit pour lire l’écriture manuscrite ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, les examens, la recherche en dessinant), pour lire le texte imprimé ({m[SCAN_TITLE]}, et les pages que vous numérisez ou importez en images, pour pouvoir y chercher et en sélectionner les mots au lasso) et pour traduire ({m[SCAN_TRANSLATE]}). La lecture et la traduction se font sur votre appareil : votre écriture, vos photos, vos pages et le texte que vous traduisez ne sont pas envoyés à Google.',
    ml_uses_noocr='{app} utilise Google ML Kit pour lire l’écriture manuscrite ({m[SEL_CHECK]}, {m[SEL_COPY_TEXT]}, {m[SEL_SAVE_WORD]}, les examens, la recherche en dessinant) et pour traduire ({m[SCAN_TRANSLATE]}). La lecture et la traduction se font sur votre appareil : votre écriture et le texte que vous traduisez ne sont pas envoyés à Google.',
    ml_dl_ocr='ML Kit télécharge ses modèles depuis Google : celui de l’écriture (environ {ink_mb} Mo) dès le premier lancement {de_app}, et un modèle de traduction (environ {tr_mb} Mo par langue) la première fois que vous traduisez. Le modèle qui lit le texte imprimé est fourni avec l’app.',
    ml_dl_noocr='ML Kit télécharge ses modèles depuis Google : celui de l’écriture (environ {ink_mb} Mo) dès le premier lancement {de_app}, et un modèle de traduction (environ {tr_mb} Mo par langue) la première fois que vous traduisez.',
    ml_uses_canvas='{app} utilise Google ML Kit pour une seule chose : lire l’écriture manuscrite, quand vous entourez de l’écrit au lasso et choisissez {m[ZOOM_READ]}, et quand {m[ZOOM_FIND]} cherche des mots dans ce que vous avez écrit à la main. La lecture se fait sur votre appareil : votre écriture n’est pas envoyée à Google.',
    s_can_off='Vous pouvez le désactiver.',
    ml_dl_canvas='ML Kit télécharge depuis Google son modèle d’écriture (environ {ink_mb} Mo), pour la langue de l’app : la première fois qu’une écriture manuscrite est lue, ou quand vous touchez {m[DOWNLOAD]} dans {mlkit_where} ; jamais de lui-même au lancement {de_app}.',
    ml_dl_canvas_noswitch='La première fois que vous vous servez de l’une ou l’autre, ML Kit télécharge depuis Google son modèle d’écriture, pour la langue de l’app.',
    ml_when_on='Tant que ML Kit est activé,', ml_when_used='Chaque fois que ML Kit est utilisé,',
    ml_diag='{when} Google en reçoit des données d’utilisation et de diagnostic : informations sur l’appareil (fabricant, modèle, version d’iPadOS ou d’Android), l’identifiant et la version de l’app, un identifiant propre à l’installation, des mesures de performance, des événements (comme les téléchargements de modèles) et des codes d’erreur, et la langue choisie. Google les utilise pour le diagnostic et l’analyse d’utilisation ; elles ne sont pas liées à votre identité et ne servent ni à la publicité ni au pistage. Google les détaille pour <a href="{mlkit_ios_data}">iPadOS</a> et pour <a href="{mlkit_android_data}">Android</a> ; voir aussi la <a href="{google_privacy}">politique de confidentialité de Google</a> et les <a href="{mlkit_terms}">conditions de ML Kit</a>.',
    ml_switch_ocr='Pour tout désactiver : {mlkit_path}. L’app n’appelle alors plus jamais ML Kit : elle lit l’écriture par elle-même (un peu moins précisément), {m[SCAN_TITLE]} et {m[SCAN_TRANSLATE]} ne sont plus disponibles, et rien n’est téléchargé ni envoyé à Google par ML Kit.',
    ml_switch_noocr='Pour tout désactiver : {mlkit_path}. L’app n’appelle alors plus jamais ML Kit : elle lit l’écriture par elle-même (un peu moins précisément), {m[SCAN_TRANSLATE]} n’est plus disponible, et rien n’est téléchargé ni envoyé à Google par ML Kit.',
    ml_switch_canvas='Pour le désactiver : {mlkit_path}. {m[ZOOM_READ]} et la recherche dans l’écriture manuscrite de {m[ZOOM_FIND]} ne sont alors plus disponibles, et rien n’est téléchargé ni envoyé à Google par ML Kit.',
    ml_noswitch_canvas='{app} n’a pas encore de réglage pour désactiver ML Kit. Il ne sert que lorsque vous utilisez {m[ZOOM_READ]} ou {m[ZOOM_FIND]} : d’ici là, rien n’est téléchargé ni envoyé à Google par son intermédiaire.',
    h_camera='Appareil photo et photos',
    cam_ocr='{m[SCAN_TITLE]} n’utilise l’appareil photo que pendant que vous vous en servez, et seulement si vous l’autorisez. Le système ne transmet que la photo que vous y choisissez : {app} ne voit pas le reste de vos photos. Ce que voit l’appareil photo et les photos choisies sont lus sur votre appareil, et ne sont ni conservés ni envoyés.',
    cam_scan='Dans {m[LIBRARY_TITLE]}, {m[LIBRARY_SCAN]} transforme des pages papier en un document à lire et à annoter. Sur iPad, cela ouvre l’appareil photo de documents d’Apple, si vous autorisez l’appareil photo. Sur Android, cela ouvre le scanner de documents de Google, qui fonctionne dans les services Google Play selon les conditions de Google, demande lui-même l’accès à l’appareil photo et ne dépend pas du réglage de ML Kit. Dans les deux cas, {app} ne reçoit que les pages que vous gardez.',
    cam_picker='{m[LIBRARY_PHOTOS]} et {m[LIBRARY_FILES]} utilisent les sélecteurs du système : {app} ne reçoit que les images et les fichiers que vous choisissez, et ne demande jamais l’accès à votre photothèque. Les pages numérisées et les images et fichiers importés sont conservés avec vos documents, sur votre appareil.',
    cam_none='{app} n’utilise pas l’appareil photo.',
    pic_canvas='{m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} et {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_FILES]} utilisent les sélecteurs du système : {app} ne reçoit que les images et les fichiers que vous choisissez, et ne demande jamais l’accès à votre photothèque. Ce que vous importez est conservé dans votre toile, sur votre appareil.',
    photos_add='Quand vous exportez une image ou une vidéo, la feuille de partage du système peut l’enregistrer dans vos photos : sur iPad, iPadOS vous le demande d’abord, et {app} peut seulement ajouter des éléments à votre photothèque, jamais la consulter.',
    h_mic='Micro et enregistrements',
    mic='{app} n’utilise le micro que pendant que vous enregistrez ({m[ZOOM_AUDIO_RECORD]}), et seulement si vous l’autorisez ; l’enregistrement s’arrête quand vous quittez la toile ou l’app. Les enregistrements sont conservés sur votre appareil avec la toile à laquelle ils appartiennent, sont inclus dans {export_path} avec tout le reste, et ne sont jamais envoyés nulle part.',
    h_speech='De la parole au texte',
    speech_android='Sur Android, {m[ZOOM_LISTEN_TRANSCRIBE]} et {m[ZOOM_LISTEN_LIVE]} transforment en texte ce que dit un enregistrement, sur votre appareil, avec Whisper, le modèle vocal ouvert d’OpenAI, exécuté par whisper.cpp. Le modèle n’est pas fourni avec l’app : la première fois, {app} vous indique sa taille (environ {speech_mb} Mo) et ne le télécharge que si vous touchez à nouveau pour accepter. Il provient de Hugging Face (huggingface.co), où il est publié ; comme tout téléchargement, il montre à Hugging Face l’adresse internet de votre appareil, selon la <a href="{hf_privacy}">politique de confidentialité de Hugging Face</a>. Ensuite, la transcription fonctionne hors ligne, et ni vos enregistrements ni leur texte ne quittent l’appareil.',
    speech_ios_none='Sur iPad, {app} ne transcrit pas encore la parole.',
    h_voice='Lecture à voix haute',
    voice='{app} lit {taught_the} à voix haute avec les voix de votre appareil : celles d’iPadOS sur iPad et, sur Android, le moteur de synthèse vocale choisi dans les réglages du système ({tts_engine}, par exemple), qui fonctionne selon ses propres conditions.',
    h_crash='Rapports de plantage',
    crash='Si {app} se ferme de façon inattendue, l’app écrit un court rapport sur votre appareil : sa version, le modèle de l’appareil et la version du système, l’endroit où elle s’est arrêtée et les dernières choses qu’elle faisait (par exemple, quel écran était ouvert) — jamais rien de ce que vous avez écrit, dessiné ou nommé. À l’ouverture suivante, elle vous le propose : {m[CRASH_SEND]} ouvre un e-mail à notre adresse avec le rapport, à lire avant de l’envoyer ; {m[CRASH_LATER]} le supprime. Rien n’est envoyé si vous n’envoyez pas cet e-mail.',
    h_links='Liens et e-mail',
    links='{app} n’ouvre une page web, un e-mail ou une boutique que lorsque vous touchez quelque chose prévu pour cela : {m[FAQ_CONTACT]} ouvre cette page ou un e-mail à notre adresse, et {m[RATE]} vous mène à l’App Store ou à Google Play.',
    links_note='Un lien que vous écrivez dans une note s’ouvre dans votre navigateur quand vous le touchez.',
    h_stores='L’App Store et Google Play',
    stores='Le téléchargement {de_app} et tout achat sont gérés par Apple (l’App Store) ou Google (Google Play), selon leur propre politique de confidentialité, tout comme les rapports de plantage et les données d’utilisation que votre appareil partage avec les développeurs si vous l’avez autorisé dans ses réglages.',
    h_children='Enfants',
    children='{app} ne recueille sciemment aucune information personnelle, y compris celles d’enfants. En dehors des téléchargements décrits plus haut, les diagnostics de ML Kit sont les seules données qui quittent l’appareil d’elles-mêmes, et ils n’identifient personne.',
    h_changes='Modifications',
    changes='Si cette politique change, la nouvelle version sera publiée sur cette page avec sa date.',
    h_contact='Contact',
    contact_privacy='Questions sur la confidentialité : {mail}.',
    q_start='Par où commencer ?',
    a_start_study='Ouvrez le menu en haut à gauche : il a tout pour étudier, plus {m[REVIEWS]}, {m[VOCAB]}, {m[LIBRARY_TITLE]}, {m[EXAMS]} et {m[STATISTICS]}. Choisissez {unit} pour voir ses traits dans l’ordre, puis écrivez à votre tour : l’app lit ce que vous écrivez et vous dit comment c’était. {m[TUTORIAL]}, dans le même menu, remontre les premiers pas.',
    a_start_notes='Sur l’écran d’accueil, {m[ZOOM_HOME_NEW_NOTEBOOK]} crée un cahier de pages. Écrivez et dessinez avec les outils de la barre, et zoomez n’importe où pour avoir plus de place : c’est sans fin. {m[ZOOM_PAGES]} change la taille des pages, ou fait du cahier une seule toile sans bords ({m[ZOOM_PAGES_ENDLESS]}).',
    a_start_workshop='Sur l’écran d’accueil, {m[ZOOM_HOME_NEW_PROJECT]} crée un projet. {m[ZOOM_WELCOME_WORKSHOP_EXAMPLES]} {m[ZOOM_WELCOME_WORKSHOP_BUILD]} {m[ZOOM_WELCOME_WORKSHOP_DRAW]}',
    q_pencil='Faut-il un Apple Pencil ou un stylet ?',
    a_pencil_study='Non. Avec la main de la barre activée, un doigt écrit et deux déplacent la page. Avec un Apple Pencil, ou un stylet sur Android, l’app bascule d’elle-même : le stylet écrit, les doigts déplacent la page, et vous pouvez poser la main sur l’écran.',
    a_pencil_canvas='Non. Avec la main de la barre activée, un doigt dessine et deux déplacent la toile et zooment ; c’est ainsi que l’app démarre. Avec un Apple Pencil, ou un stylet sur Android, elle bascule d’elle-même : le stylet dessine et les doigts déplacent la toile.',
    q_offline='{app} fonctionne-t-il hors ligne ?',
    a_offline_study='Oui. Le modèle d’écriture de Google ML Kit (environ {ink_mb} Mo) se télécharge une fois, au premier lancement de l’app, et le modèle de chaque langue pour {m[SCAN_TRANSLATE]}, la première fois que vous traduisez ; ensuite, tout fonctionne hors ligne.',
    a_offline_canvas='Oui. Il ne faut internet qu’une fois : la première fois que vous utilisez {m[ZOOM_READ]} ou la recherche dans l’écriture manuscrite de {m[ZOOM_FIND]}, pour télécharger le modèle d’écriture de Google ML Kit.',
    a_offline_speech='Sur Android, la transcription en a besoin une fois aussi, pour télécharger le modèle vocal (environ {speech_mb} Mo).',
    q_data='Où est enregistré mon travail ? Puis-je le passer sur un autre appareil ?',
    a_data='Sur votre appareil, et dans sa sauvegarde iCloud ou Google si elle est activée. Pour en garder une copie vous-même, ou pour passer à un autre iPad ou à une autre tablette : {export_path}, puis {m[DATA_IMPORT]} sur l’autre (cela remplace ce qui s’y trouve). Le fichier sert aussi bien sur iPad que sur Android.',
    q_hand='Mon écriture est mal reconnue',
    a_hand='Dans {mlkit_where}, {mlkit_name} doit être activé et son modèle téléchargé (il faut internet une fois). Désactivé, l’app lit l’écriture par elle-même, moins précisément.',
    q_photo='{app} peut-il lire le texte d’un livre ou d’un panneau ?',
    a_photo_ocr='Oui : touchez l’appareil photo de la barre, ou appuyez longuement sur la page et choisissez {m[CTX_SCAN]}, puis visez un texte en {taught}, ou choisissez une photo. L’app lit chaque ligne : {m[SCAN_HOLD]} les garde, {m[SCAN_WRITE]} les pose sur la page en écriture à la main à repasser, et {m[SCAN_TRANSLATE]} les affiche dans votre langue.',
    a_photo_noocr='Pas encore : Google ML Kit ne sait pas lire le texte imprimé en {taught}, donc {m[SCAN_TITLE]} n’est pas disponible. Vous pouvez quand même importer des pages à lire et à annoter : {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} ou {m[LIBRARY_PHOTOS]}.',
    q_translate='Comment traduire ?',
    a_translate='Entourez votre écriture au lasso et choisissez {m[SCAN_TRANSLATE]} pour la lire dans votre langue. Dans {m[VOCAB]}, {m[SCAN_TRANSLATE]} va dans l’autre sens : tapez dans votre langue et obtenez le résultat en {taught}, avec ses mots à enregistrer ou à travailler. Tout se fait sur votre appareil : le modèle de chaque langue (environ {tr_mb} Mo) se télécharge une fois, puis fonctionne hors ligne.',
    q_voice='Il n’y a pas de voix, ou elle semble robotique',
    a_voice='L’app lit à voix haute avec les voix de votre appareil. Sur iPad : {voice_ios} Sur Android, installez une voix pour {taught_the} dans le moteur de synthèse vocale ({tts_engine}, par exemple), dans les réglages du système ; s’il n’y en a aucune, {m[VOICE_BUTTON]}, dans {app}, vous les ouvre.',
    q_reviews='Comment fonctionnent les révisions ?',
    a_reviews='Marquez {units} {m[STUDYING]} : {m[REVIEWS]} vous les ramène au bon moment, le lendemain, puis trois jours plus tard, et de plus en plus tard à chaque bonne réponse ; plus tôt sinon. Les nouveaux arrivent au plus dix par jour, pour qu’en marquer beaucoup d’un coup ne surcharge pas le lendemain. Les mots que vous enregistrez ont leurs propres révisions, dans {m[VOCAB]}.',
    q_sheet='Puis-je imprimer des feuilles d’entraînement ?',
    a_sheet='Oui : {m[SHEET]}, sur la page d’{unit}, en mode {m[SELECT]} ou dans {m[VOCAB]}, crée un PDF à imprimer ou à partager.',
    q_record='Comment enregistrer un cours avec mes notes ?',
    a_record='Touchez le micro de la barre ({m[ZOOM_AUDIO_RECORD]}) et écrivez : ce que vous écrivez est lié à ce qui se dit à ce moment-là. Plus tard, touchez ce que vous avez écrit pour entendre ce qui se disait alors. {m[ZOOM_AUDIO_RECORDINGS]} liste les enregistrements de la toile, pour les écouter, revoir l’encre s’écrire ou les supprimer.',
    q_transcribe='{app} peut-il transformer un enregistrement en texte ?',
    a_transcribe='Sur Android, oui : dans {m[ZOOM_AUDIO_RECORDINGS]}, {m[ZOOM_LISTEN_TRANSCRIBE]} écrit ce que dit un enregistrement, et {m[ZOOM_LISTEN_LIVE]} le fait pendant que vous enregistrez ; {m[ZOOM_LISTEN_AS_TEXT]} ou {m[ZOOM_LISTEN_AS_HAND]} le pose ensuite sur la toile. Tout se fait sur l’appareil, après un téléchargement unique du modèle vocal (environ {speech_mb} Mo), que l’app vous demande d’abord d’accepter. Sur iPad, pas encore.',
    q_totext='Mon écriture manuscrite peut-elle devenir du texte ?',
    a_totext='Oui : entourez-la au lasso et choisissez {m[ZOOM_READ]} (une annulation rend l’écriture). {m[ZOOM_FIND]} trouve des mots dans vos textes et dans votre écriture manuscrite, dans cette toile et dans les autres. Tous deux lisent l’écriture avec Google ML Kit, sur votre appareil, dans la langue de l’app.',
    q_import='Comment importer un PDF ou une photo ?',
    a_import='{m[ZOOM_INSERT]} › {m[ZOOM_INSERT_PDF]} ouvre un PDF dans une toile à part, pour écrire dessus. {m[ZOOM_INSERT]} › {m[ZOOM_PICTURE_PHOTOS]} ou {m[ZOOM_PICTURE_FILES]} pose des images sur la toile.',
    q_export='Comment partager ou imprimer mon travail ?',
    a_export='{m[ZOOM_EXPORT]} propose {exports}. Chacun est créé sur votre appareil et passe par la feuille de partage du système, pour l’enregistrer, l’imprimer ou l’envoyer.',
    q_privacy='Qu’envoie {app}, et à qui ?',
    a_privacy_study='Votre travail reste sur votre appareil. Google ML Kit, qui lit l’écriture et le texte et traduit sur l’appareil, télécharge ses modèles et envoie à Google des diagnostics anonymes tant qu’il est activé : {mlkit_path} le désactive.',
    a_privacy_study_noocr='Votre travail reste sur votre appareil. Google ML Kit, qui lit l’écriture et traduit sur l’appareil, télécharge ses modèles et envoie à Google des diagnostics anonymes tant qu’il est activé : {mlkit_path} le désactive.',
    a_privacy_canvas='Votre travail reste sur votre appareil. Google ML Kit lit votre écriture manuscrite sur l’appareil pour {m[ZOOM_READ]} et {m[ZOOM_FIND]} : quand vous vous en servez, il télécharge son modèle et envoie à Google des diagnostics anonymes.',
    a_privacy_speech='Sur Android, la transcription télécharge une fois le modèle vocal, depuis Hugging Face.',
    a_totext_off='Si {m[ZOOM_READ]} n’apparaît pas, c’est que {mlkit_name} est désactivé dans {mlkit_where}.',
    a_privacy_switch='Pour désactiver ML Kit : {mlkit_path}.',
    a_privacy_more='Tout le reste se trouve dans la <a href="privacy.html#{lang}">politique de confidentialité</a>.',
    contact_title='Contact',
    contact_help='Écrivez à {mail}. En cas de problème, indiquez le modèle de votre appareil, la version du système et ce que vous faisiez.',
)

# A study app's own language: only the paragraphs its one app uses (Kana's Japanese, Hanzi's Chinese, Hangul's Korean,
# Thai's Thai, Hindi's Hindi, Arabic's Arabic). In these the app is shown in the language it teaches, so translation
# goes to and from English (translate.c:74-87), and the paragraphs say so.

TEXT['ja'] = dict(
    sp='', privacy='プライバシーポリシー', help='ヘルプとサポート', updated='更新日：{date}', langs='言語', and_='、',
    units={'char': ('文字', '文字'), 'letter': ('文字', '文字')},
    tts_engine='Google 音声サービス',
    intro_study='{app}は{taught}を書いて学ぶためのアプリで、iPadとAndroidタブレットで使えます。オフラインで使えるように作られており、書いたものや学んだことはお使いの端末の中に残ります。',
    h_short='概要',
    s_none='{app}にはアカウントも広告もトラッキングもなく、{app}自身があなたについての情報を集めることはありません。',
    s_local='{app}で作ったものはすべて、お使いの端末の中だけに保存されます。',
    s_mlkit_switch='一部の機能はGoogle ML Kitを使います。ML Kitは端末の中で動きますが、オンの間は匿名の診断データをGoogleに送信します。オフにすることもできます。',
    s_crash='{app}が予期せず終了したときは、短いレポートをメールで送るかどうかを尋ねます。あなたが送らない限り、何も送られません。',
    h_keeps='{app}が保存するもの、保存する場所',
    k_study='{app}は、ページと手書きの文字、メモ、練習とテストの履歴、復習の予定、単語帳とリスト、文字ごとのメモ、{m[LIBRARY_TITLE]}に取り込んだ資料、設定を、お使いの端末の中だけに保存します。{app}自身がそれらをどこかへ送ることはありません。',
    k_backup='ほかのアプリのデータと同じく、端末のバックアップがオンなら、その中に含まれます（iPadではiCloudバックアップ、AndroidではGoogleのバックアップで、後者はアプリごとに最大25 MBまでです）。それらはAppleとGoogleがそれぞれのプライバシーポリシーのもとで扱います。',
    k_export='{export_path} で、すべてを1つのファイルにまとめられます。そのファイルは、あなたが送ったり保存したりする先にだけ渡ります。',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app}は、手書き文字の読み取り（{m[SEL_CHECK]}、{m[SEL_COPY_TEXT]}、{m[SEL_SAVE_WORD]}、テスト、描いて検索）、印刷された文字の読み取り（{m[SCAN_TITLE]}と、スキャンしたページや画像として取り込んだページ。検索したり投げ縄で言葉を選んだりできるようにするためです）、翻訳（{m[SCAN_TRANSLATE]}）にGoogle ML Kitを使います。読み取りと翻訳は端末の中で行われ、手書きの文字、写真、ページ、翻訳する文はGoogleに送られません。',
    ml_dl_ocr='ML KitはGoogleからモデルをダウンロードします。手書き認識のモデル（約{ink_mb} MB）は{app}を初めて起動したとき、翻訳のモデル（言語ごとに約{tr_mb} MB）は初めて翻訳するときです。印刷された文字を読むモデルはアプリに含まれています。',
    ml_when_on='ML Kitがオンの間、',
    ml_diag='{when}Googleは利用状況と診断のデータを受け取ります：端末の情報（メーカー、機種、iPadOSまたはAndroidのバージョン）、アプリの識別子とバージョン、インストールごとの識別子、パフォーマンスの数値、イベント（モデルのダウンロードなど）とエラーコード、設定されている言語。Googleはこれらを診断と利用状況の分析に使い、あなたの身元とは結び付けず、広告やトラッキングには使いません。詳しくはGoogleによる説明（<a href="{mlkit_ios_data}">iPadOS</a>、<a href="{mlkit_android_data}">Android</a>）、<a href="{google_privacy}">Googleのプライバシーポリシー</a>、<a href="{mlkit_terms}">ML Kitの利用規約</a>をご覧ください。',
    ml_switch_ocr='すべてオフにするには：{mlkit_path}。オフの間、{app}はML Kitを一切使いません。手書き文字は自分で読み取り（精度は少し下がります）、{m[SCAN_TITLE]}と{m[SCAN_TRANSLATE]}は使えなくなり、ML Kitを通じて何かがダウンロードされたり、Googleに送られたりすることはありません。',
    h_camera='カメラと写真',
    cam_ocr='{m[SCAN_TITLE]}は、あなたが許可したうえで、使っている間だけカメラを使います。そこで選んだ写真はシステムがその1枚だけを渡すので、{app}がほかの写真を見ることはできません。カメラに映ったものと選んだ写真は端末の中で読み取られ、保存も送信もされません。',
    cam_scan='{m[LIBRARY_TITLE]}の「{m[LIBRARY_SCAN]}」は、紙のページを、読んだり書き込んだりできる資料にします。iPadでは、カメラの使用を許可したうえで、Appleの書類カメラが開きます。Androidでは、Google Playサービスの中でGoogleの規約のもとに動く、Googleのドキュメントスキャナが開きます。カメラの許可はスキャナ自身が求め、ML Kitのスイッチの対象にはなりません。どちらの場合も、{app}が受け取るのは、あなたが残したページだけです。',
    cam_picker='「{m[LIBRARY_PHOTOS]}」と「{m[LIBRARY_FILES]}」はシステム自身の選択画面を使うので、{app}が受け取るのはあなたが選んだ画像とファイルだけで、写真ライブラリへのアクセスを求めることはありません。スキャンしたページや取り込んだ画像とファイルは、資料として端末の中に保存されます。',
    h_voice='読み上げ',
    voice='{app}は、端末そのものの音声で{taught}を読み上げます。iPadではiPadOSの音声、Androidではシステムの設定で選ばれている音声合成エンジン（{tts_engine}など）を使い、そのエンジンは独自の規約のもとで動きます。',
    h_crash='クラッシュレポート',
    crash='{app}が予期せず終了すると、端末の中に短いレポートを書きます。内容はアプリのバージョン、端末の機種とシステムのバージョン、止まった場所、直前にしていたこと（どの画面が開いていたかなど）で、あなたが書いたもの、描いたもの、名前を付けたものは一切含みません。次に{app}を開いたときに確認があり、「{m[CRASH_SEND]}」を選ぶとレポートの入ったメールが私たち宛てに開くので、送る前に読めます。「{m[CRASH_LATER]}」を選ぶとレポートは削除されます。そのメールをあなたが送らない限り、何も送られません。',
    h_links='リンクとメール',
    links='{app}がWebページやメール、ストアを開くのは、そのためのボタンをタップしたときだけです。「{m[FAQ_CONTACT]}」はこのページか私たち宛てのメールを開き、「{m[RATE]}」はApp StoreかGoogle Playに移ります。',
    h_stores='App StoreとGoogle Play',
    stores='{app}のダウンロードと購入は、Apple（App Store）またはGoogle（Google Play）がそれぞれのプライバシーポリシーのもとで扱います。端末の設定で許可している場合に開発者と共有されるクラッシュレポートや利用状況のデータも同様です。',
    h_children='お子さまについて',
    children='{app}は、お子さまを含め、誰の個人情報も意図して集めることはありません。上に書いたダウンロードを除けば、端末の外にひとりでに出るデータはML Kitの診断データだけで、それは個人を特定するものではありません。',
    h_changes='変更',
    changes='このポリシーを変更する場合は、新しい版を日付とともにこのページに掲載します。',
    h_contact='お問い合わせ',
    contact_privacy='プライバシーについてのご質問：{mail}',
    q_start='何から始めればいいですか？',
    a_start_study='左上のメニューを開いてください。学習に必要なものがすべてあり、{m[REVIEWS]}、{m[VOCAB]}、{m[LIBRARY_TITLE]}、{m[EXAMS]}、{m[STATISTICS]}もここにあります。{unit}を選ぶと書き順が見られるので、続けて自分で書いてみましょう。{app}が書いた文字を読み取り、どのくらい上手に書けたかを教えてくれます。同じメニューの「{m[TUTORIAL]}」で、最初の手順をもう一度見られます。',
    q_pencil='Apple Pencilやスタイラスは必要ですか？',
    a_pencil_study='いいえ。バーの手のボタンがオンなら、指1本で書き、2本指でページを動かします。Apple Pencil（Androidではスタイラス）を使うと自動で切り替わり、ペンで書いて指でページを動かすので、手を画面に置いたまま書けます。',
    q_offline='{app}はオフラインで使えますか？',
    a_offline_study='はい。Google ML Kitの手書きモデル（約{ink_mb} MB）は{app}を初めて起動したときに、「{m[SCAN_TRANSLATE]}」で使う各言語のモデルは初めて翻訳するときに、一度だけダウンロードされます。そのあとは、すべてオフラインで使えます。',
    q_data='作ったものはどこに保存されますか？新しい端末に移せますか？',
    a_data='お使いの端末の中と、オンになっていればそのiCloudバックアップまたはGoogleのバックアップに保存されます。自分でコピーを取ったり、別のiPadやタブレットに移したりするには、{export_path} を使い、移す先で「{m[DATA_IMPORT]}」を選んでください（そこにあるものは置き換えられます）。このファイルはiPadでもAndroidでも使えます。',
    q_hand='手書きの文字がうまく読み取られません',
    a_hand='{mlkit_where} で「{mlkit_name}」がオンになっていて、モデルがダウンロード済みか確認してください（一度だけインターネットが必要です）。オフの場合、{app}は自分で読み取りますが、精度は下がります。',
    q_photo='本や看板の文字を読み取れますか？',
    a_photo_ocr='はい。バーのカメラをタップするか、ページを長押しして「{m[CTX_SCAN]}」を選び、{taught}の文字に向けるか、写真を選んでください。{app}が行ごとに読み取ります。「{m[SCAN_HOLD]}」で残し、「{m[SCAN_WRITE]}」でなぞれる手書きとしてページに書き、「{m[SCAN_TRANSLATE]}」で訳を表示できます。',
    q_translate='翻訳するには？',
    a_translate='書いた文字を投げ縄で囲んで「{m[SCAN_TRANSLATE]}」を選ぶと、英語に訳せます（アプリを日本語で使っている間は、日本語と英語の間で翻訳します）。{m[VOCAB]}の「{m[SCAN_TRANSLATE]}」は逆向きで、英語を入力すると日本語になり、その中の言葉を保存したり練習したりできます。どれも端末の中で行われ、各言語のモデル（約{tr_mb} MB）は一度ダウンロードすれば、あとはオフラインで使えます。',
    q_voice='音声がない、または機械的に聞こえます',
    a_voice='{app}は端末の音声で読み上げます。iPadでは：{voice_ios}Androidでは、システムの設定で音声合成エンジン（{tts_engine}など）に{taught}の音声をインストールしてください。音声がないときは、{app}の「{m[VOICE_BUTTON]}」からその設定を開けます。',
    q_reviews='復習はどのような仕組みですか？',
    a_reviews='{units}を「{m[STUDYING]}」にすると、{m[REVIEWS]}がその時期が来たものを出題します。翌日、次に3日後、正しく書けるたびに間隔が長くなり、書けなければ早めに戻ってきます。新しいものは1日に最大10個までなので、一度にたくさん印を付けても翌日があふれることはありません。保存した言葉には、{m[VOCAB]}に専用の復習があります。',
    q_sheet='練習シートを印刷できますか？',
    a_sheet='はい。文字のページ、{m[SELECT]}モード、{m[VOCAB]}の「{m[SHEET]}」で、印刷や共有のできるPDFを作れます。',
    q_privacy='{app}は何を、誰に送りますか？',
    a_privacy_study='作ったものは端末の中に残ります。端末の中で手書きや印刷された文字を読み取り、翻訳するGoogle ML Kitは、オンの間、モデルをダウンロードし、匿名の診断データをGoogleに送ります。{mlkit_path} でオフにできます。',
    a_privacy_more='そのほかのことは、すべて<a href="privacy.html#{lang}">プライバシーポリシー</a>に書いてあります。',
    contact_title='お問い合わせ',
    contact_help='{mail} までご連絡ください。不具合の場合は、端末の機種、システムのバージョン、何をしていたかをお知らせください。',
)

TEXT['zh'] = dict(
    sp='', privacy='隐私政策', help='帮助与支持', updated='更新日期：{date}', langs='语言', and_='和',
    units={'char': ('一个字', '字'), 'letter': ('一个字母', '字母')},
    tts_engine='Google 语音服务',
    intro_study='{app} 是一款学习书写{taught}的应用，适用于 iPad 和 Android 平板。它专为离线使用而设计：你写下的和学到的内容都留在你的设备上。',
    h_short='简要说明',
    s_none='{app} 没有账户、没有广告、没有跟踪，也不会为自己收集任何关于你的信息。',
    s_local='你在 {app} 中创建的一切都只保存在你的设备上。',
    s_mlkit_switch='部分功能使用 Google ML Kit。它在你的设备上运行，但在开启期间会向 Google 发送匿名诊断数据。你可以将其关闭。',
    s_crash='如果 {app} 意外关闭，它会询问你是否要通过电子邮件把一份简短的报告发给我们：只要你不发送，就什么都不会发出。',
    h_keeps='{app} 保存什么，保存在哪里',
    k_study='{app} 只在你的设备上保存你的页面和手写内容、笔记、练习和测验记录、复习计划、生词本和列表、你为每个字写的笔记、你导入“{m[LIBRARY_TITLE]}”的文档以及设置。{app} 本身从不把它们发送到任何地方。',
    k_backup='与任何应用的数据一样，如果你开启了设备备份，这些数据也会包含在其中：iPad 上是 iCloud 云备份，Android 上是 Google 备份（每个应用最多保存 25 MB 数据）。Apple 和 Google 按照各自的隐私政策保管这些备份。',
    k_export='“{export_path}”会把所有内容放进一个文件。这个文件只会去往你选择发送或保存的地方。',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app} 使用 Google ML Kit 识别手写（“{m[SEL_CHECK]}”“{m[SEL_COPY_TEXT]}”“{m[SEL_SAVE_WORD]}”、测验、手写查字），识别印刷文字（“{m[SCAN_TITLE]}”，以及你扫描或以图片导入的页面，以便在其中搜索、用套索选取其中的词语），并进行翻译（“{m[SCAN_TRANSLATE]}”）。识别和翻译都在你的设备上完成：你的手写、照片、页面和要翻译的文字都不会发送给 Google。',
    ml_dl_ocr='ML Kit 会从 Google 下载模型：手写识别模型（约 {ink_mb} MB）在首次启动 {app} 时下载，翻译模型（每种语言约 {tr_mb} MB）在你第一次翻译时下载。识别印刷文字的模型已内置在应用中。',
    ml_when_on='ML Kit 开启期间，',
    ml_diag='{when}Google 会从中收到使用情况和诊断数据：设备信息（制造商、型号、iPadOS 或 Android 版本）、应用的标识符和版本、每次安装的标识符、性能数据、事件（如模型下载）和错误代码，以及所设置的语言。Google 将其用于诊断和使用情况分析；这些数据不会与你的身份关联，也不会用于广告或跟踪。Google 对此有详细说明（<a href="{mlkit_ios_data}">iPadOS</a>、<a href="{mlkit_android_data}">Android</a>），另请参阅 <a href="{google_privacy}">Google 隐私政策</a>和 <a href="{mlkit_terms}">ML Kit 条款</a>。',
    ml_switch_ocr='要全部关闭：{mlkit_path}。之后 {app} 不再调用 ML Kit：它会自行识别手写（准确度略低），“{m[SCAN_TITLE]}”和“{m[SCAN_TRANSLATE]}”将无法使用，也不会通过 ML Kit 下载任何内容或向 Google 发送任何数据。',
    h_camera='相机和照片',
    cam_ocr='“{m[SCAN_TITLE]}”只在你使用时、并在你允许后才使用相机。你在其中选择的照片由系统单独交给应用：{app} 看不到你的其他照片。相机拍到的画面和你选择的照片都在设备上识别，既不保存也不发送。',
    cam_scan='在“{m[LIBRARY_TITLE]}”中，“{m[LIBRARY_SCAN]}”可以把纸质页面变成可以阅读和书写的文档。在 iPad 上，它会在你允许使用相机后打开 Apple 的文档相机。在 Android 上，它会打开 Google 的文档扫描器；扫描器在 Google Play 服务中按照 Google 的条款运行，自行请求相机权限，不受 ML Kit 开关控制。无论哪种情况，{app} 都只会收到你保留的页面。',
    cam_picker='“{m[LIBRARY_PHOTOS]}”和“{m[LIBRARY_FILES]}”使用系统自带的选择器：{app} 只会收到你选择的图片和文件，也从不请求访问你的照片图库。你扫描的页面以及导入的图片和文件会和你的文档一起保存在你的设备上。',
    h_voice='朗读',
    voice='{app} 使用设备自带的语音朗读{taught}：在 iPad 上使用 iPadOS 的语音，在 Android 上使用系统设置中选定的文字转语音引擎（例如 {tts_engine}），该引擎按照其自身的条款运行。',
    h_crash='崩溃报告',
    crash='如果 {app} 意外关闭，它会在你的设备上写下一份简短的报告：应用版本、设备型号和系统版本、应用在哪里停止，以及它最后在做的几件事（例如当时打开的是哪个界面），绝不包含你写的、画的或命名的任何内容。下次打开 {app} 时，它会询问你：“{m[CRASH_SEND]}”会打开一封发给我们的电子邮件，报告就在其中，你可以在发送前阅读；“{m[CRASH_LATER]}”会删除报告。只要你不发送这封邮件，就什么都不会发出。',
    h_links='链接和电子邮件',
    links='只有当你点按专门的按钮时，{app} 才会打开网页、电子邮件或应用商店：“{m[FAQ_CONTACT]}”会打开本页或一封发给我们的电子邮件，“{m[RATE]}”会带你前往 App Store 或 Google Play。',
    h_stores='App Store 和 Google Play',
    stores='{app} 的下载和任何购买由 Apple（App Store）或 Google（Google Play）按照各自的隐私政策处理；如果你在设备设置中允许，设备与开发者共享的崩溃报告和使用数据也是如此。',
    h_children='儿童',
    children='{app} 不会有意收集任何人的个人信息，包括儿童。除上文所述的下载外，ML Kit 的诊断数据是唯一会自行离开设备的数据，而且它们无法识别个人身份。',
    h_changes='变更',
    changes='如果本政策有所变更，新版本将连同日期发布在本页。',
    h_contact='联系我们',
    contact_privacy='有关隐私的问题：{mail}',
    q_start='我该从哪里开始？',
    a_start_study='打开左上角的菜单：这里有学习所需的一切，还有“{m[REVIEWS]}”“{m[VOCAB]}”“{m[LIBRARY_TITLE]}”“{m[EXAMS]}”和“{m[STATISTICS]}”。选{unit}就能看到它的笔顺，然后自己写一写：{app} 会识别你写的字，并告诉你写得怎么样。同一菜单里的“{m[TUTORIAL]}”可以再次展示入门步骤。',
    q_pencil='需要 Apple Pencil 或触控笔吗？',
    a_pencil_study='不需要。打开工具栏上的手形按钮后，用一根手指书写，两根手指移动页面。使用 Apple Pencil（在 Android 上是触控笔）时，{app} 会自动切换：用笔书写，用手指移动页面，手还可以放在屏幕上。',
    q_offline='{app} 可以离线使用吗？',
    a_offline_study='可以。Google ML Kit 的手写识别模型（约 {ink_mb} MB）会在首次启动 {app} 时下载一次；“{m[SCAN_TRANSLATE]}”所用的每种语言的模型会在你第一次翻译时下载。之后，一切都能离线使用。',
    q_data='我的内容保存在哪里？可以转移到新设备吗？',
    a_data='保存在你的设备上；如果开启了 iCloud 云备份或 Google 备份，也会包含在其中。要自己保留一份副本，或者转移到另一台 iPad 或平板，请使用“{export_path}”，然后在另一台设备上使用“{m[DATA_IMPORT]}”（会替换那里已有的内容）。这个文件在 iPad 和 Android 上通用。',
    q_hand='我的手写识别得不好',
    a_hand='请在“{mlkit_where}”中确认“{mlkit_name}”已开启，并且模型已下载（需要联网一次）。关闭时，{app} 会自行识别手写，但准确度较低。',
    q_photo='{app} 能识别书本或招牌上的文字吗？',
    a_photo_ocr='可以：点按工具栏上的相机，或长按页面并选择“{m[CTX_SCAN]}”，然后对准{taught}文字，或选择一张照片。{app} 会逐行识别：“{m[SCAN_HOLD]}”保留这些行，“{m[SCAN_WRITE]}”把它们作为可以描写的手写字放到页面上，“{m[SCAN_TRANSLATE]}”则显示译文。',
    q_translate='如何翻译？',
    a_translate='用套索圈出你写的字，选择“{m[SCAN_TRANSLATE]}”，即可译成英文（以中文使用本应用时，翻译在中文和英文之间进行）。在“{m[VOCAB]}”中，“{m[SCAN_TRANSLATE]}”方向相反：输入英文，得到中文，还可以保存或练习其中的词语。一切都在设备上完成：每种语言的模型（约 {tr_mb} MB）只需下载一次，之后即可离线使用。',
    q_voice='没有语音，或者听起来很生硬',
    a_voice='{app} 使用设备自带的语音朗读。在 iPad 上：{voice_ios}在 Android 上，请在系统设置中为文字转语音引擎（例如 {tts_engine}）安装{taught}语音；没有语音时，{app} 中的“{m[VOICE_BUTTON]}”会为你打开相应设置。',
    q_reviews='复习是怎么安排的？',
    a_reviews='把{units}标记为“{m[STUDYING]}”后，“{m[REVIEWS]}”会在到期时让它们重新出现：第二天，然后是三天后，之后每写对一次间隔就更长，写错则会更早出现。新字每天最多加入十个，所以一次标记很多也不会挤满第二天。你保存的词语在“{m[VOCAB]}”中有自己的复习。',
    q_sheet='可以打印练习纸吗？',
    a_sheet='可以：在字的页面、“{m[SELECT]}”模式或“{m[VOCAB]}”中使用“{m[SHEET]}”，即可生成可打印或分享的 PDF。',
    q_privacy='{app} 会发送什么？发给谁？',
    a_privacy_study='你的内容留在你的设备上。在设备上识别手写和印刷文字并进行翻译的 Google ML Kit，在开启期间会下载模型，并向 Google 发送匿名诊断数据：在“{mlkit_path}”中可以将其关闭。',
    a_privacy_more='其他所有内容都在<a href="privacy.html#{lang}">隐私政策</a>中。',
    contact_title='联系我们',
    contact_help='请发邮件至 {mail}。如果出现问题，请告诉我们你的设备型号、系统版本以及当时在做什么。',
)

# Korean: a particle after a menu name would depend on its last syllable, so the names are followed by a noun
# (‘…’ 버튼, 메뉴, 기능) or by a particle that does not change (에, 의, 에서).
TEXT['ko'] = dict(
    privacy='개인정보 처리방침', help='도움말 및 지원', updated='업데이트: {date}', langs='언어', and_=', ',
    units={'char': ('글자 하나', '글자'), 'letter': ('글자 하나', '글자')},
    tts_engine='Google 음성 서비스',
    intro_study='{app}은 {taught}를 손으로 쓰며 배우는 앱으로, iPad와 Android 태블릿에서 사용할 수 있습니다. 오프라인에서 작동하도록 만들어져, 쓰고 배운 내용은 모두 사용자의 기기에 남습니다.',
    h_short='요약',
    s_none='{app}에는 계정도, 광고도, 추적도 없으며, {app}이 스스로 사용자에 관한 정보를 수집하는 일도 없습니다.',
    s_local='{app}에서 만든 모든 것은 사용자의 기기에만 저장됩니다.',
    s_mlkit_switch='일부 기능은 Google ML Kit을 사용합니다. ML Kit은 기기에서 작동하지만, 켜져 있는 동안 익명의 진단 데이터를 Google에 보냅니다. 끌 수도 있습니다.',
    s_crash='{app}이 예기치 않게 종료되면 짧은 보고서를 이메일로 보낼지 묻습니다. 사용자가 직접 보내지 않으면 아무것도 전송되지 않습니다.',
    h_keeps='{app}이 저장하는 것과 저장 위치',
    k_study='{app}은 페이지와 손글씨, 노트, 연습 및 시험 기록, 복습 일정, 단어장과 목록, 글자별 메모, ‘{m[LIBRARY_TITLE]}’에 가져온 문서, 설정을 사용자의 기기에만 저장합니다. {app} 자체는 이를 어디에도 보내지 않습니다.',
    k_backup='다른 앱의 데이터와 마찬가지로, 기기 백업이 켜져 있으면 그 백업에 포함됩니다. iPad에서는 iCloud 백업, Android에서는 Google 백업(앱당 최대 25MB까지 보관)입니다. Apple과 Google은 각자의 개인정보 처리방침에 따라 이를 보관합니다.',
    k_export='‘{export_path}’ 버튼을 누르면 모든 것이 하나의 파일로 묶입니다. 이 파일은 사용자가 보내거나 저장하기로 한 곳으로만 갑니다.',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app}은 Google ML Kit을 사용해 손글씨를 읽고(‘{m[SEL_CHECK]}’, ‘{m[SEL_COPY_TEXT]}’, ‘{m[SEL_SAVE_WORD]}’, 시험, 손글씨로 찾기), 인쇄된 글자를 읽으며(‘{m[SCAN_TITLE]}’ 기능, 그리고 스캔하거나 이미지로 가져온 페이지: 그 안에서 검색하고 올가미로 단어를 고를 수 있도록), 번역합니다(‘{m[SCAN_TRANSLATE]}’ 기능). 읽기와 번역은 기기에서 이루어지며, 손글씨, 사진, 페이지, 번역하는 텍스트는 Google에 전송되지 않습니다.',
    ml_dl_ocr='ML Kit은 Google에서 모델을 다운로드합니다. 손글씨 모델(약 {ink_mb}MB)은 {app}을 처음 실행할 때, 번역 모델(언어당 약 {tr_mb}MB)은 처음 번역할 때 받습니다. 인쇄된 글자를 읽는 모델은 앱에 포함되어 있습니다.',
    ml_when_on='ML Kit이 켜져 있는 동안',
    ml_diag='{when} Google은 ML Kit으로부터 사용 및 진단 데이터를 받습니다. 기기 정보(제조사, 모델, iPadOS 또는 Android 버전), 앱의 식별자와 버전, 설치별 식별자, 성능 수치, 이벤트(모델 다운로드 등)와 오류 코드, 설정된 언어입니다. Google은 이를 진단과 사용 분석에 쓰며, 사용자의 신원과 연결하지 않고 광고나 추적에 사용하지 않습니다. 자세한 내용은 Google의 안내(<a href="{mlkit_ios_data}">iPadOS</a>, <a href="{mlkit_android_data}">Android</a>)와 <a href="{google_privacy}">Google 개인정보처리방침</a>, <a href="{mlkit_terms}">ML Kit 약관</a>을 참고하세요.',
    ml_switch_ocr='모두 끄려면: {mlkit_path}. 그러면 {app}은 ML Kit을 전혀 호출하지 않습니다. 손글씨는 앱이 직접 읽고(정확도는 조금 낮아집니다), ‘{m[SCAN_TITLE]}’ 기능과 ‘{m[SCAN_TRANSLATE]}’ 기능은 사용할 수 없으며, ML Kit을 통해 아무것도 다운로드되거나 Google에 전송되지 않습니다.',
    h_camera='카메라와 사진',
    cam_ocr='‘{m[SCAN_TITLE]}’ 기능은 사용자가 허용한 뒤, 사용하는 동안에만 카메라를 씁니다. 여기서 고른 사진은 시스템이 그 한 장만 건네주므로 {app}은 다른 사진을 볼 수 없습니다. 카메라에 비친 것과 고른 사진은 기기에서 읽히며, 저장되거나 전송되지 않습니다.',
    cam_scan='‘{m[LIBRARY_TITLE]}’의 ‘{m[LIBRARY_SCAN]}’ 기능은 종이 페이지를 읽고 쓸 수 있는 문서로 만듭니다. iPad에서는 카메라 사용을 허용하면 Apple의 문서 카메라가 열립니다. Android에서는 Google Play 서비스 안에서 Google의 약관에 따라 작동하는 Google 문서 스캐너가 열립니다. 이 스캐너는 카메라 권한을 직접 요청하며, ML Kit 스위치의 적용을 받지 않습니다. 어느 경우든 {app}은 사용자가 남긴 페이지만 받습니다.',
    cam_picker='‘{m[LIBRARY_PHOTOS]}’ 및 ‘{m[LIBRARY_FILES]}’ 기능은 시스템 자체의 선택 화면을 사용합니다. {app}은 사용자가 고른 이미지와 파일만 받으며, 사진 보관함에 대한 접근을 요청하지 않습니다. 스캔한 페이지와 가져온 이미지 및 파일은 문서와 함께 기기에 저장됩니다.',
    h_voice='소리 내어 읽기',
    voice='{app}은 기기 자체의 음성으로 {taught}를 소리 내어 읽습니다. iPad에서는 iPadOS의 음성을, Android에서는 시스템 설정에서 선택한 텍스트 음성 변환 엔진({tts_engine} 등)을 쓰며, 이 엔진은 자체 약관에 따라 작동합니다.',
    h_crash='오류 보고서',
    crash='{app}이 예기치 않게 종료되면 기기에 짧은 보고서를 남깁니다. 앱 버전, 기기 모델과 시스템 버전, 앱이 멈춘 위치, 직전에 하던 일 몇 가지(예: 열려 있던 화면)가 담기며, 사용자가 쓰거나 그리거나 이름 붙인 것은 절대 포함되지 않습니다. 다음에 {app}을 열면 확인을 요청합니다. ‘{m[CRASH_SEND]}’ 버튼을 누르면 보고서가 담긴 이메일이 저희 앞으로 열려, 보내기 전에 읽어 볼 수 있습니다. ‘{m[CRASH_LATER]}’ 버튼을 누르면 보고서가 삭제됩니다. 사용자가 그 이메일을 보내지 않으면 아무것도 전송되지 않습니다.',
    h_links='링크와 이메일',
    links='{app}은 해당 버튼을 탭할 때만 웹 페이지, 이메일 또는 스토어를 엽니다. ‘{m[FAQ_CONTACT]}’ 메뉴는 이 페이지나 저희에게 보내는 이메일을 열고, ‘{m[RATE]}’ 버튼은 App Store나 Google Play로 안내합니다.',
    h_stores='App Store와 Google Play',
    stores='{app}의 다운로드와 모든 구매는 Apple(App Store) 또는 Google(Google Play)이 각자의 개인정보 처리방침에 따라 처리합니다. 사용자가 기기 설정에서 허용한 경우 기기가 개발자와 공유하는 오류 보고서와 사용 데이터도 마찬가지입니다.',
    h_children='아동',
    children='{app}은 아동을 포함해 누구의 개인정보도 의도적으로 수집하지 않습니다. 위에서 설명한 다운로드를 제외하면, 기기 밖으로 저절로 나가는 데이터는 ML Kit의 진단 데이터뿐이며, 이는 개인을 식별하지 않습니다.',
    h_changes='변경 사항',
    changes='이 방침이 바뀌면 새 버전을 날짜와 함께 이 페이지에 게시합니다.',
    h_contact='문의',
    contact_privacy='개인정보에 관한 문의: {mail}',
    q_start='무엇부터 시작하면 되나요?',
    a_start_study='왼쪽 위의 메뉴를 여세요. 공부에 필요한 모든 것이 있고, {m[REVIEWS]}, {m[VOCAB]}, {m[LIBRARY_TITLE]}, {m[EXAMS]}, {m[STATISTICS]}도 여기에 있습니다. {unit}를 고르면 획순을 볼 수 있으니, 이어서 직접 써 보세요. {app}이 사용자가 쓴 글자를 읽고 얼마나 잘 썼는지 알려 줍니다. 같은 메뉴의 ‘{m[TUTORIAL]}’에서 처음 단계를 다시 볼 수 있습니다.',
    q_pencil='Apple Pencil이나 스타일러스가 필요한가요?',
    a_pencil_study='아니요. 툴바의 손 버튼을 켜면 손가락 하나로 쓰고, 두 손가락으로 페이지를 움직입니다. Apple Pencil(Android에서는 스타일러스)을 쓰면 {app}이 알아서 전환합니다. 펜으로 쓰고 손가락으로 페이지를 움직이며, 화면에 손을 올려놓아도 됩니다.',
    q_offline='{app}은 오프라인에서도 작동하나요?',
    a_offline_study='네. Google ML Kit의 손글씨 모델(약 {ink_mb}MB)은 {app}을 처음 실행할 때, ‘{m[SCAN_TRANSLATE]}’ 기능에 쓰이는 언어별 모델은 처음 번역할 때 한 번만 다운로드됩니다. 그다음부터는 모두 오프라인으로 작동합니다.',
    q_data='작업은 어디에 저장되나요? 새 기기로 옮길 수 있나요?',
    a_data='사용자의 기기에 저장되며, iCloud 백업이나 Google 백업이 켜져 있으면 그 백업에도 포함됩니다. 직접 사본을 보관하거나 다른 iPad나 태블릿으로 옮기려면 ‘{export_path}’ 버튼을 누른 다음, 다른 기기에서 ‘{m[DATA_IMPORT]}’ 버튼을 누르세요(그 기기에 있던 내용은 대체됩니다). 이 파일은 iPad와 Android 어디에서나 쓸 수 있습니다.',
    q_hand='손글씨가 잘 인식되지 않아요',
    a_hand='{mlkit_where}에서 ‘{mlkit_name}’ 옵션이 켜져 있고 모델이 다운로드되었는지 확인하세요(한 번은 인터넷이 필요합니다). 꺼져 있으면 {app}이 직접 읽지만 정확도가 낮아집니다.',
    q_photo='책이나 간판의 글자를 읽을 수 있나요?',
    a_photo_ocr='네. 툴바의 카메라를 탭하거나 페이지를 길게 눌러 ‘{m[CTX_SCAN]}’ 메뉴를 고른 다음, {taught} 글자에 카메라를 비추거나 사진을 고르세요. {app}이 한 줄씩 읽습니다. ‘{m[SCAN_HOLD]}’ 버튼은 줄을 남기고, ‘{m[SCAN_WRITE]}’ 버튼은 따라 쓸 수 있는 손글씨로 페이지에 옮기며, ‘{m[SCAN_TRANSLATE]}’ 버튼은 번역을 보여 줍니다.',
    q_translate='번역은 어떻게 하나요?',
    a_translate='쓴 글자를 올가미로 감싸고 ‘{m[SCAN_TRANSLATE]}’ 메뉴를 고르면 영어로 번역됩니다(앱을 한국어로 쓰는 동안에는 한국어와 영어 사이에서 번역합니다). ‘{m[VOCAB]}’의 ‘{m[SCAN_TRANSLATE]}’ 버튼은 반대 방향으로, 영어로 입력하면 한국어로 바꿔 주고, 그 안의 단어를 저장하거나 연습할 수 있습니다. 모두 기기에서 이루어지며, 언어별 모델(약 {tr_mb}MB)은 한 번만 받으면 그다음부터 오프라인으로 작동합니다.',
    q_voice='음성이 없거나 기계음처럼 들려요',
    a_voice='{app}은 기기의 음성으로 읽습니다. iPad에서는: {voice_ios} Android에서는 시스템 설정에서 텍스트 음성 변환 엔진({tts_engine} 등)에 {taught} 음성을 설치하세요. 음성이 없을 때는 {app}의 ‘{m[VOICE_BUTTON]}’ 버튼으로 그 설정을 열 수 있습니다.',
    q_reviews='복습은 어떻게 이루어지나요?',
    a_reviews='{units}를 ‘{m[STUDYING]}’ 상태로 표시하면, ‘{m[REVIEWS]}’에서 때가 된 글자를 다시 내 줍니다. 다음 날, 그다음엔 사흘 뒤, 맞게 쓸 때마다 간격이 길어지고, 틀리면 더 일찍 돌아옵니다. 새 글자는 하루에 최대 열 개까지만 들어오므로, 한꺼번에 많이 표시해도 다음 날이 넘치지 않습니다. 저장한 단어는 ‘{m[VOCAB]}’에서 따로 복습합니다.',
    q_sheet='연습지를 인쇄할 수 있나요?',
    a_sheet='네. 글자 페이지, ‘{m[SELECT]}’ 모드, ‘{m[VOCAB]}’에 있는 ‘{m[SHEET]}’ 버튼으로 인쇄하거나 공유할 수 있는 PDF를 만들 수 있습니다.',
    q_privacy='{app}은 무엇을 누구에게 보내나요?',
    a_privacy_study='작업은 사용자의 기기에 남습니다. 기기에서 손글씨와 인쇄된 글자를 읽고 번역하는 Google ML Kit은, 켜져 있는 동안 모델을 다운로드하고 익명의 진단 데이터를 Google에 보냅니다. {mlkit_path}에서 끌 수 있습니다.',
    a_privacy_more='그 밖의 모든 내용은 <a href="privacy.html#{lang}">개인정보 처리방침</a>에 있습니다.',
    contact_title='문의',
    contact_help='{mail} 주소로 연락해 주세요. 문제가 생겼다면 기기 모델, 시스템 버전, 그리고 무엇을 하고 있었는지 알려 주세요.',
)

TEXT['th'] = dict(
    privacy='นโยบายความเป็นส่วนตัว', help='ความช่วยเหลือและการสนับสนุน', updated='ปรับปรุงล่าสุด: {date}', langs='ภาษา', and_=' และ ',
    units={'char': ('ตัวอักษรสักตัว', 'ตัวอักษร'), 'letter': ('ตัวอักษรสักตัว', 'ตัวอักษร')},
    tts_engine='บริการเสียงพูดของ Google',
    intro_study='{app} เป็นแอปสำหรับฝึกเขียน{taught} ใช้ได้บน iPad และแท็บเล็ต Android แอปนี้ออกแบบมาให้ทำงานได้โดยไม่ต้องต่ออินเทอร์เน็ต สิ่งที่คุณเขียนและเรียนรู้จะอยู่บนอุปกรณ์ของคุณ',
    h_short='โดยสรุป',
    s_none='{app} ไม่มีบัญชีผู้ใช้ ไม่มีโฆษณา ไม่มีการติดตาม และไม่เก็บข้อมูลใด ๆ เกี่ยวกับคุณไว้ใช้เอง',
    s_local='ทุกสิ่งที่คุณสร้างใน {app} จะถูกเก็บไว้บนอุปกรณ์ของคุณเท่านั้น',
    s_mlkit_switch='บางฟีเจอร์ใช้ Google ML Kit ซึ่งทำงานบนอุปกรณ์ของคุณ แต่จะส่งข้อมูลการวินิจฉัยแบบไม่ระบุตัวตนไปยัง Google ขณะที่เปิดใช้อยู่ และคุณปิดได้',
    s_crash='หาก {app} ปิดตัวลงโดยไม่คาดคิด แอปจะเสนอให้ส่งรายงานสั้น ๆ ทางอีเมลถึงเรา จะไม่มีอะไรถูกส่งไปหากคุณไม่ส่งเอง',
    h_keeps='สิ่งที่ {app} เก็บไว้ และเก็บไว้ที่ไหน',
    k_study='{app} เก็บหน้าเขียนและลายมือของคุณ บันทึก ประวัติการฝึกและแบบทดสอบ กำหนดการทบทวน คำศัพท์และรายการคำ บันทึกเกี่ยวกับตัวอักษร เอกสารที่คุณนำเข้ามาใน “{m[LIBRARY_TITLE]}” และการตั้งค่าของคุณ ไว้บนอุปกรณ์ของคุณเท่านั้น ตัว {app} เองไม่เคยส่งข้อมูลเหล่านี้ไปที่ใด',
    k_backup='เช่นเดียวกับข้อมูลของแอปอื่น ๆ ข้อมูลเหล่านี้จะรวมอยู่ในข้อมูลสำรองของอุปกรณ์เมื่อคุณเปิดใช้ไว้ ได้แก่ การสำรองข้อมูล iCloud บน iPad และการสำรองข้อมูลของ Google บน Android (ซึ่งเก็บข้อมูลได้สูงสุด 25 MB ต่อแอป) โดย Apple และ Google ดูแลข้อมูลสำรองเหล่านั้นตามนโยบายความเป็นส่วนตัวของตนเอง',
    k_export='{export_path} จะรวมทุกอย่างไว้ในไฟล์เดียว ไฟล์นั้นจะไปเฉพาะที่ที่คุณเลือกส่งหรือบันทึกเท่านั้น',
    h_mlkit='Google ML Kit',
    ml_uses_noocr='{app} ใช้ Google ML Kit เพื่ออ่านลายมือ (“{m[SEL_CHECK]}” “{m[SEL_COPY_TEXT]}” “{m[SEL_SAVE_WORD]}” แบบทดสอบ และการค้นหาด้วยการเขียน) และเพื่อแปลภาษา (“{m[SCAN_TRANSLATE]}”) การอ่านและการแปลเกิดขึ้นบนอุปกรณ์ของคุณ ลายมือของคุณและข้อความที่คุณแปลจะไม่ถูกส่งไปยัง Google',
    ml_dl_noocr='ML Kit ดาวน์โหลดโมเดลจาก Google ได้แก่ โมเดลลายมือ (ประมาณ {ink_mb} MB) ทันทีที่เปิด {app} ครั้งแรก และโมเดลการแปล (ประมาณ {tr_mb} MB ต่อภาษา) ในครั้งแรกที่คุณแปล',
    ml_when_on='ขณะที่เปิดใช้ ML Kit',
    ml_diag='{when} Google จะได้รับข้อมูลการใช้งานและการวินิจฉัยจาก ML Kit ได้แก่ ข้อมูลอุปกรณ์ (ผู้ผลิต รุ่น เวอร์ชันของ iPadOS หรือ Android) ตัวระบุและเวอร์ชันของแอป ตัวระบุของการติดตั้งแต่ละครั้ง ตัวเลขประสิทธิภาพ เหตุการณ์ (เช่น การดาวน์โหลดโมเดล) และรหัสข้อผิดพลาด รวมถึงภาษาที่ตั้งไว้ Google ใช้ข้อมูลเหล่านี้เพื่อการวินิจฉัยและการวิเคราะห์การใช้งาน โดยไม่เชื่อมโยงกับตัวตนของคุณ และไม่ใช้เพื่อการโฆษณาหรือการติดตาม ดูรายละเอียดจาก Google สำหรับ <a href="{mlkit_ios_data}">iPadOS</a> และ <a href="{mlkit_android_data}">Android</a> รวมถึง <a href="{google_privacy}">นโยบายความเป็นส่วนตัวของ Google</a> และ <a href="{mlkit_terms}">ข้อกำหนดของ ML Kit</a>',
    ml_switch_noocr='หากต้องการปิดทั้งหมด ให้ไปที่ {mlkit_path} จากนั้น {app} จะไม่เรียกใช้ ML Kit อีกเลย แอปจะอ่านลายมือเอง (แม่นยำน้อยลงเล็กน้อย) “{m[SCAN_TRANSLATE]}” จะใช้ไม่ได้ และจะไม่มีการดาวน์โหลดหรือส่งสิ่งใดไปยัง Google ผ่าน ML Kit',
    h_camera='กล้องและรูปภาพ',
    cam_scan='ใน “{m[LIBRARY_TITLE]}” ปุ่ม “{m[LIBRARY_SCAN]}” จะเปลี่ยนหน้ากระดาษให้เป็นเอกสารที่อ่านและเขียนลงไปได้ บน iPad จะเปิดกล้องสแกนเอกสารของ Apple เมื่อคุณอนุญาตให้ใช้กล้อง บน Android จะเปิดเครื่องสแกนเอกสารของ Google ซึ่งทำงานอยู่ในบริการ Google Play ตามข้อกำหนดของ Google ขออนุญาตใช้กล้องเอง และไม่อยู่ภายใต้สวิตช์ ML Kit ไม่ว่ากรณีใด {app} จะได้รับเฉพาะหน้าที่คุณเก็บไว้เท่านั้น',
    cam_picker='“{m[LIBRARY_PHOTOS]}” และ “{m[LIBRARY_FILES]}” ใช้หน้าต่างเลือกของระบบเอง {app} จะได้รับเฉพาะรูปภาพและไฟล์ที่คุณเลือก และไม่เคยขอสิทธิ์เข้าถึงคลังรูปภาพของคุณ หน้าที่คุณสแกน รวมถึงรูปภาพและไฟล์ที่นำเข้ามา จะถูกเก็บไว้กับเอกสารของคุณบนอุปกรณ์',
    h_voice='การอ่านออกเสียง',
    voice='{app} อ่าน{taught}ออกเสียงด้วยเสียงของอุปกรณ์คุณเอง บน iPad ใช้เสียงของ iPadOS และบน Android ใช้ระบบอ่านออกเสียงข้อความที่เลือกไว้ในการตั้งค่าระบบ (เช่น {tts_engine}) ซึ่งทำงานตามข้อกำหนดของตนเอง',
    h_crash='รายงานข้อขัดข้อง',
    crash='หาก {app} ปิดตัวลงโดยไม่คาดคิด แอปจะเขียนรายงานสั้น ๆ ไว้บนอุปกรณ์ของคุณ ได้แก่ เวอร์ชันของแอป รุ่นของอุปกรณ์และเวอร์ชันของระบบ จุดที่แอปหยุดทำงาน และสิ่งสุดท้ายที่แอปกำลังทำอยู่ (เช่น หน้าจอที่เปิดอยู่) โดยไม่มีสิ่งที่คุณเขียน วาด หรือตั้งชื่อเลย เมื่อคุณเปิด {app} ครั้งถัดไป แอปจะถามคุณ ปุ่ม “{m[CRASH_SEND]}” จะเปิดอีเมลถึงเราที่มีรายงานอยู่ข้างใน ให้คุณอ่านก่อนส่ง ส่วนปุ่ม “{m[CRASH_LATER]}” จะลบรายงานนั้น จะไม่มีอะไรถูกส่งไปหากคุณไม่ส่งอีเมลนั้นเอง',
    h_links='ลิงก์และอีเมล',
    links='{app} จะเปิดหน้าเว็บ อีเมล หรือร้านค้าแอป เฉพาะเมื่อคุณแตะปุ่มสำหรับสิ่งนั้นเท่านั้น “{m[FAQ_CONTACT]}” จะเปิดหน้านี้หรืออีเมลถึงเรา และ “{m[RATE]}” จะพาคุณไปที่ App Store หรือ Google Play',
    h_stores='App Store และ Google Play',
    stores='การดาวน์โหลด {app} และการซื้อใด ๆ ดำเนินการโดย Apple (App Store) หรือ Google (Google Play) ตามนโยบายความเป็นส่วนตัวของแต่ละราย รวมถึงรายงานข้อขัดข้องและข้อมูลการใช้งานที่อุปกรณ์ของคุณแชร์กับนักพัฒนาเมื่อคุณอนุญาตไว้ในการตั้งค่า',
    h_children='เด็ก',
    children='{app} ไม่เก็บข้อมูลส่วนบุคคลของผู้ใดโดยเจตนา รวมถึงเด็ก นอกจากการดาวน์โหลดที่อธิบายไว้ข้างต้น ข้อมูลการวินิจฉัยของ ML Kit เป็นข้อมูลเดียวที่ออกจากอุปกรณ์ไปเอง และข้อมูลนี้ไม่สามารถระบุตัวบุคคลได้',
    h_changes='การเปลี่ยนแปลง',
    changes='หากนโยบายนี้เปลี่ยนแปลง เราจะเผยแพร่ฉบับใหม่พร้อมวันที่ไว้ในหน้านี้',
    h_contact='ติดต่อ',
    contact_privacy='คำถามเกี่ยวกับความเป็นส่วนตัว: {mail}',
    q_start='ควรเริ่มจากตรงไหน',
    a_start_study='เปิดเมนูที่มุมซ้ายบน ในนั้นมีทุกอย่างสำหรับการเรียน รวมถึง “{m[REVIEWS]}” “{m[VOCAB]}” “{m[LIBRARY_TITLE]}” “{m[EXAMS]}” และ “{m[STATISTICS]}” เลือก{unit}เพื่อดูลำดับการเขียนเส้น แล้วลองเขียนเอง {app} จะอ่านสิ่งที่คุณเขียนและบอกว่าเขียนได้ดีแค่ไหน “{m[TUTORIAL]}” ในเมนูเดียวกันจะแสดงขั้นตอนแรกให้ดูอีกครั้ง',
    q_pencil='ต้องใช้ Apple Pencil หรือปากกาสไตลัสไหม',
    a_pencil_study='ไม่ต้อง เมื่อเปิดปุ่มรูปมือบนแถบเครื่องมือ นิ้วเดียวจะใช้เขียน และสองนิ้วใช้เลื่อนหน้า เมื่อใช้ Apple Pencil (หรือปากกาสไตลัสบน Android) {app} จะสลับให้เอง ปากกาใช้เขียน นิ้วใช้เลื่อนหน้า และวางมือบนหน้าจอได้',
    q_offline='{app} ใช้งานแบบออฟไลน์ได้ไหม',
    a_offline_study='ได้ โมเดลลายมือของ Google ML Kit (ประมาณ {ink_mb} MB) จะดาวน์โหลดครั้งเดียวเมื่อเปิด {app} ครั้งแรก และโมเดลของแต่ละภาษาสำหรับ “{m[SCAN_TRANSLATE]}” จะดาวน์โหลดในครั้งแรกที่คุณแปล หลังจากนั้นทุกอย่างใช้งานแบบออฟไลน์ได้',
    q_data='งานของฉันเก็บไว้ที่ไหน ย้ายไปอุปกรณ์ใหม่ได้ไหม',
    a_data='เก็บไว้บนอุปกรณ์ของคุณ และอยู่ในข้อมูลสำรอง iCloud หรือข้อมูลสำรองของ Google เมื่อเปิดใช้ไว้ หากต้องการเก็บสำเนาเอง หรือย้ายไปยัง iPad หรือแท็บเล็ตเครื่องอื่น ให้ใช้ {export_path} แล้วใช้ “{m[DATA_IMPORT]}” บนเครื่องใหม่ (จะแทนที่สิ่งที่มีอยู่ในเครื่องนั้น) ไฟล์นี้ใช้ได้ทั้งบน iPad และ Android',
    q_hand='แอปอ่านลายมือของฉันได้ไม่ดี',
    a_hand='ใน {mlkit_where} ตรวจดูว่า “{mlkit_name}” เปิดอยู่และดาวน์โหลดโมเดลแล้ว (ต้องใช้อินเทอร์เน็ตหนึ่งครั้ง) ถ้าปิดไว้ {app} จะอ่านลายมือเองแต่แม่นยำน้อยกว่า',
    q_photo='{app} อ่านข้อความจากหนังสือหรือป้ายได้ไหม',
    a_photo_noocr='ยังไม่ได้ เพราะ Google ML Kit ยังอ่านตัวพิมพ์{taught}ไม่ได้ จึงยังไม่มี “{m[SCAN_TITLE]}” แต่คุณยังนำหน้ากระดาษเข้ามาอ่านและเขียนลงไปได้ด้วย {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} หรือ {m[LIBRARY_PHOTOS]}',
    q_translate='แปลภาษาได้อย่างไร',
    a_translate='ใช้บ่วงบาศล้อมสิ่งที่คุณเขียน แล้วเลือก “{m[SCAN_TRANSLATE]}” เพื่ออ่านเป็นภาษาอังกฤษ (เมื่อใช้แอปเป็นภาษาไทย การแปลจะเป็นระหว่างภาษาไทยกับภาษาอังกฤษ) ใน “{m[VOCAB]}” ปุ่ม “{m[SCAN_TRANSLATE]}” จะแปลในทางกลับกัน พิมพ์ภาษาอังกฤษแล้วได้ภาษาไทย พร้อมคำศัพท์ให้บันทึกหรือฝึก ทุกอย่างเกิดขึ้นบนอุปกรณ์ของคุณ โมเดลของแต่ละภาษา (ประมาณ {tr_mb} MB) ดาวน์โหลดเพียงครั้งเดียว แล้วใช้แบบออฟไลน์ได้',
    q_voice='ไม่มีเสียง หรือเสียงฟังดูเหมือนหุ่นยนต์',
    a_voice='{app} อ่านออกเสียงด้วยเสียงของอุปกรณ์ บน iPad: {voice_ios} บน Android ให้ติดตั้งเสียง{taught}สำหรับระบบอ่านออกเสียงข้อความ (เช่น {tts_engine}) ในการตั้งค่าระบบ หากยังไม่มีเสียง ปุ่ม “{m[VOICE_BUTTON]}” ใน {app} จะเปิดการตั้งค่านั้นให้',
    q_reviews='การทบทวนทำงานอย่างไร',
    a_reviews='ทำเครื่องหมาย{units}ว่า “{m[STUDYING]}” แล้ว “{m[REVIEWS]}” จะนำแต่ละตัวกลับมาเมื่อถึงเวลา คือวันถัดไป จากนั้นอีกสามวัน แล้วนานขึ้นทุกครั้งที่คุณเขียนถูก และเร็วขึ้นเมื่อเขียนผิด ตัวใหม่จะเพิ่มเข้ามาไม่เกินวันละสิบตัว การทำเครื่องหมายหลายตัวพร้อมกันจึงไม่ทำให้วันถัดไปล้น ส่วนคำที่คุณบันทึกไว้มีการทบทวนของตัวเองใน “{m[VOCAB]}”',
    q_sheet='พิมพ์ใบฝึกได้ไหม',
    a_sheet='ได้ “{m[SHEET]}” ในหน้าของตัวอักษร ในโหมด “{m[SELECT]}” หรือใน “{m[VOCAB]}” จะสร้าง PDF ให้พิมพ์หรือแชร์',
    q_privacy='{app} ส่งอะไร และส่งให้ใคร',
    a_privacy_study_noocr='งานของคุณอยู่บนอุปกรณ์ของคุณ Google ML Kit ซึ่งอ่านลายมือและแปลภาษาบนอุปกรณ์ จะดาวน์โหลดโมเดลและส่งข้อมูลการวินิจฉัยแบบไม่ระบุตัวตนไปยัง Google ขณะที่เปิดใช้อยู่ ปิดได้ที่ {mlkit_path}',
    a_privacy_more='ส่วนเรื่องอื่น ๆ ทั้งหมดอยู่ใน <a href="privacy.html#{lang}">นโยบายความเป็นส่วนตัว</a>',
    contact_title='ติดต่อ',
    contact_help='เขียนถึงเราได้ที่ {mail} หากเกิดปัญหา โปรดบอกรุ่นของอุปกรณ์ เวอร์ชันของระบบ และสิ่งที่คุณกำลังทำอยู่',
)

TEXT['hi'] = dict(
    privacy='निजता नीति', help='मदद और सहायता', updated='अपडेट किया गया: {date}', langs='भाषाएँ', and_=' और ',
    units={'char': ('कोई अक्षर', 'अक्षरों'), 'letter': ('कोई अक्षर', 'अक्षरों')},
    tts_engine='Google की स्पीच सेवाएँ',
    intro_study='{app} {taught} लिखना सीखने का ऐप है, जो iPad और Android टैबलेट पर चलता है। इसे बिना इंटरनेट के चलने के लिए बनाया गया है: आप जो लिखते और सीखते हैं, वह आपके डिवाइस पर ही रहता है।',
    h_short='संक्षेप में',
    s_none='{app} में न कोई खाता है, न विज्ञापन, न ट्रैकिंग, और यह अपने लिए आपके बारे में कुछ भी इकट्ठा नहीं करता।',
    s_local='{app} में आप जो कुछ बनाते हैं, वह सब सिर्फ़ आपके डिवाइस पर सहेजा जाता है।',
    s_mlkit_switch='कुछ सुविधाएँ Google ML Kit का इस्तेमाल करती हैं। यह आपके डिवाइस पर ही काम करता है, लेकिन चालू रहने तक Google को गुमनाम डायग्नोस्टिक डेटा भेजता है। आप इसे बंद कर सकते हैं।',
    s_crash='अगर {app} अचानक बंद हो जाए, तो वह हमें ईमेल से एक छोटी रिपोर्ट भेजने का विकल्प देता है: जब तक आप खुद न भेजें, कुछ नहीं जाता।',
    h_keeps='{app} क्या रखता है, और कहाँ',
    k_study='{app} आपके पन्ने और लिखावट, आपके नोट, अभ्यास और परीक्षाओं का इतिहास, दोहराव की योजना, आपकी शब्दावली और सूचियाँ, अक्षरों पर आपके नोट, “{m[LIBRARY_TITLE]}” में लाए गए दस्तावेज़ और आपकी सेटिंग्स — सिर्फ़ आपके डिवाइस पर रखता है। {app} खुद इन्हें कहीं नहीं भेजता।',
    k_backup='किसी भी ऐप के डेटा की तरह, अगर आपके डिवाइस का बैकअप चालू है तो ये उसमें शामिल रहते हैं: iPad पर iCloud बैकअप, और Android पर Google बैकअप (जो हर ऐप का ज़्यादा से ज़्यादा 25 MB डेटा रखता है)। Apple और Google इन्हें अपनी-अपनी निजता नीतियों के तहत रखते हैं।',
    k_export='{export_path} सब कुछ एक फ़ाइल में रख देता है। वह फ़ाइल सिर्फ़ वहीं जाती है जहाँ आप उसे भेजना या सहेजना चुनते हैं।',
    h_mlkit='Google ML Kit',
    ml_uses_ocr='{app} Google ML Kit का इस्तेमाल लिखावट पढ़ने (“{m[SEL_CHECK]}”, “{m[SEL_COPY_TEXT]}”, “{m[SEL_SAVE_WORD]}”, परीक्षाएँ, लिखकर खोजना), छपा हुआ टेक्स्ट पढ़ने (“{m[SCAN_TITLE]}”, और वे पन्ने जिन्हें आप स्कैन करते हैं या तस्वीरों के रूप में लाते हैं, ताकि आप उनमें खोज सकें और लैसो से उनके शब्द चुन सकें) और अनुवाद करने (“{m[SCAN_TRANSLATE]}”) के लिए करता है। पढ़ना और अनुवाद करना आपके डिवाइस पर ही होता है: आपकी लिखावट, फ़ोटो, पन्ने और जिस टेक्स्ट का आप अनुवाद करते हैं, वह Google को नहीं भेजा जाता।',
    ml_dl_ocr='ML Kit अपने मॉडल Google से डाउनलोड करता है: लिखावट का मॉडल (लगभग {ink_mb} MB) {app} के पहली बार खुलते ही, और अनुवाद का मॉडल (हर भाषा के लिए लगभग {tr_mb} MB) पहली बार अनुवाद करने पर। छपा हुआ टेक्स्ट पढ़ने वाला मॉडल ऐप के अंदर ही आता है।',
    ml_when_on='ML Kit चालू रहने तक',
    ml_diag='{when} Google को उससे इस्तेमाल और डायग्नोस्टिक डेटा मिलता है: डिवाइस की जानकारी (निर्माता, मॉडल, iPadOS या Android का वर्शन), ऐप का पहचानकर्ता और वर्शन, हर इंस्टॉलेशन का एक पहचानकर्ता, परफ़ॉर्मेंस के आँकड़े, इवेंट (जैसे मॉडल डाउनलोड) और एरर कोड, और सेट की गई भाषा। Google इनका इस्तेमाल डायग्नोस्टिक और इस्तेमाल के विश्लेषण के लिए करता है; ये आपकी पहचान से नहीं जोड़े जाते, और विज्ञापन या ट्रैकिंग के लिए इस्तेमाल नहीं होते। Google इनका ब्योरा <a href="{mlkit_ios_data}">iPadOS</a> और <a href="{mlkit_android_data}">Android</a> के लिए देता है; <a href="{google_privacy}">Google की निजता नीति</a> और <a href="{mlkit_terms}">ML Kit की शर्तें</a> भी देखें।',
    ml_switch_ocr='सब कुछ बंद करने के लिए: {mlkit_path}। तब {app} ML Kit को कभी इस्तेमाल नहीं करता: वह लिखावट खुद पढ़ता है (थोड़ी कम सटीकता से), “{m[SCAN_TITLE]}” और “{m[SCAN_TRANSLATE]}” उपलब्ध नहीं रहते, और ML Kit के ज़रिए न कुछ डाउनलोड होता है, न Google को भेजा जाता है।',
    h_camera='कैमरा और फ़ोटो',
    cam_ocr='“{m[SCAN_TITLE]}” कैमरे का इस्तेमाल सिर्फ़ तभी करता है जब आप उसे इस्तेमाल कर रहे हों, और आपकी अनुमति के बाद। वहाँ आप जो फ़ोटो चुनते हैं, उसे सिस्टम अकेले ही सौंपता है: {app} आपकी बाकी फ़ोटो नहीं देख सकता। कैमरा जो देखता है और जो फ़ोटो आप चुनते हैं, वे आपके डिवाइस पर ही पढ़े जाते हैं, और न रखे जाते हैं, न भेजे जाते हैं।',
    cam_scan='“{m[LIBRARY_TITLE]}” में, “{m[LIBRARY_SCAN]}” काग़ज़ के पन्नों को पढ़ने और उन पर लिखने लायक दस्तावेज़ में बदल देता है। iPad पर यह कैमरे की अनुमति मिलने के बाद Apple का डॉक्यूमेंट कैमरा खोलता है। Android पर यह Google का डॉक्यूमेंट स्कैनर खोलता है, जो Google Play सेवाओं के अंदर Google की शर्तों के तहत चलता है, कैमरे की अनुमति खुद माँगता है और ML Kit के स्विच के दायरे में नहीं आता। दोनों ही मामलों में {app} को सिर्फ़ वही पन्ने मिलते हैं जिन्हें आप रखते हैं।',
    cam_picker='“{m[LIBRARY_PHOTOS]}” और “{m[LIBRARY_FILES]}” सिस्टम की अपनी चुनने वाली स्क्रीन का इस्तेमाल करते हैं: {app} को सिर्फ़ वही तस्वीरें और फ़ाइलें मिलती हैं जो आप चुनते हैं, और यह कभी आपकी फ़ोटो लाइब्रेरी तक पहुँच नहीं माँगता। स्कैन किए गए पन्ने और लाई गई तस्वीरें और फ़ाइलें आपके दस्तावेज़ों के साथ आपके डिवाइस पर रखी जाती हैं।',
    h_voice='पढ़कर सुनाना',
    voice='{app} आपके डिवाइस की अपनी आवाज़ों से {taught} पढ़कर सुनाता है: iPad पर iPadOS की आवाज़ों से, और Android पर सिस्टम सेटिंग्स में चुने गए टेक्स्ट-टू-स्पीच इंजन (जैसे {tts_engine}) से, जो अपनी शर्तों के तहत काम करता है।',
    h_crash='क्रैश रिपोर्ट',
    crash='अगर {app} अचानक बंद हो जाए, तो वह आपके डिवाइस पर एक छोटी रिपोर्ट लिखता है: ऐप का वर्शन, डिवाइस का मॉडल और सिस्टम का वर्शन, ऐप कहाँ रुका, और वह आख़िर में क्या कर रहा था (जैसे कौन-सी स्क्रीन खुली थी) — आपने जो लिखा, बनाया या नाम दिया, उसमें से कुछ भी कभी नहीं। अगली बार {app} खोलने पर वह आपसे पूछता है: “{m[CRASH_SEND]}” हमारे नाम एक ईमेल खोलता है जिसमें रिपोर्ट होती है, ताकि आप भेजने से पहले उसे पढ़ सकें; “{m[CRASH_LATER]}” उसे मिटा देता है। जब तक आप वह ईमेल न भेजें, कुछ नहीं भेजा जाता।',
    h_links='लिंक और ईमेल',
    links='{app} कोई वेब पेज, ईमेल या स्टोर तभी खोलता है जब आप उसके लिए बने बटन पर टैप करते हैं: “{m[FAQ_CONTACT]}” यह पेज या हमें ईमेल खोलता है, और “{m[RATE]}” आपको App Store या Google Play पर ले जाता है।',
    h_stores='App Store और Google Play',
    stores='{app} का डाउनलोड और कोई भी ख़रीदारी Apple (App Store) या Google (Google Play) अपनी-अपनी निजता नीतियों के तहत सँभालते हैं; वह क्रैश रिपोर्ट और इस्तेमाल का डेटा भी, जो आपका डिवाइस सेटिंग्स में आपकी अनुमति होने पर डेवलपरों से साझा करता है।',
    h_children='बच्चे',
    children='{app} जान-बूझकर किसी की भी निजी जानकारी इकट्ठा नहीं करता, बच्चों की भी नहीं। ऊपर बताए गए डाउनलोड के अलावा, ML Kit का डायग्नोस्टिक डेटा ही वह अकेला डेटा है जो अपने आप डिवाइस से बाहर जाता है, और उससे किसी व्यक्ति की पहचान नहीं होती।',
    h_changes='बदलाव',
    changes='अगर यह नीति बदलती है, तो नया वर्शन उसकी तारीख़ के साथ इसी पेज पर प्रकाशित किया जाएगा।',
    h_contact='संपर्क',
    contact_privacy='निजता के बारे में सवाल: {mail}',
    q_start='शुरुआत कहाँ से करें?',
    a_start_study='ऊपर बाईं ओर का मेन्यू खोलें: उसमें सीखने की हर चीज़ है, और “{m[REVIEWS]}”, “{m[VOCAB]}”, “{m[LIBRARY_TITLE]}”, “{m[EXAMS]}” और “{m[STATISTICS]}” भी। {unit} चुनें, उसके स्ट्रोक क्रम से देखें, फिर खुद लिखें: {app} आपका लिखा पढ़ता है और बताता है कि वह कितना अच्छा बना। उसी मेन्यू में “{m[TUTORIAL]}” शुरुआती कदम फिर से दिखाता है।',
    q_pencil='क्या Apple Pencil या स्टाइलस ज़रूरी है?',
    a_pencil_study='नहीं। टूलबार पर हाथ वाला बटन चालू हो तो एक उंगली से लिखा जाता है और दो उंगलियों से पन्ना खिसकता है। Apple Pencil (या Android पर स्टाइलस) इस्तेमाल करने पर {app} अपने आप उस पर आ जाता है: पेन लिखता है, उंगलियाँ पन्ना खिसकाती हैं, और आप हाथ स्क्रीन पर टिका सकते हैं।',
    q_offline='क्या {app} बिना इंटरनेट के चलता है?',
    a_offline_study='हाँ। Google ML Kit का लिखावट वाला मॉडल (लगभग {ink_mb} MB) {app} के पहली बार खुलने पर एक बार डाउनलोड होता है, और “{m[SCAN_TRANSLATE]}” के लिए हर भाषा का मॉडल पहली बार अनुवाद करने पर। उसके बाद सब कुछ बिना इंटरनेट के चलता है।',
    q_data='मेरा काम कहाँ रहता है? क्या उसे नए डिवाइस पर ले जाया जा सकता है?',
    a_data='आपके डिवाइस पर, और अगर iCloud बैकअप या Google बैकअप चालू हो तो उसमें भी। खुद एक कॉपी रखने के लिए, या दूसरे iPad या टैबलेट पर ले जाने के लिए {export_path} इस्तेमाल करें, फिर दूसरे डिवाइस पर “{m[DATA_IMPORT]}” (यह वहाँ पहले से मौजूद चीज़ों की जगह ले लेता है)। यह फ़ाइल iPad और Android दोनों पर चलती है।',
    q_hand='मेरी लिखावट ठीक से नहीं पढ़ी जाती',
    a_hand='{mlkit_where} में “{mlkit_name}” चालू होना चाहिए और उसका मॉडल डाउनलोड होना चाहिए (इसके लिए एक बार इंटरनेट चाहिए)। बंद होने पर {app} लिखावट खुद पढ़ता है, पर कम सटीकता से।',
    q_photo='क्या {app} किताब या साइनबोर्ड का टेक्स्ट पढ़ सकता है?',
    a_photo_ocr='हाँ: टूलबार पर कैमरे पर टैप करें, या पन्ने को दबाकर रखें और “{m[CTX_SCAN]}” चुनें, फिर कैमरे को {taught} टेक्स्ट की ओर करें, या कोई फ़ोटो चुनें। {app} हर पंक्ति पढ़ता है: “{m[SCAN_HOLD]}” उन्हें रोककर रखता है, “{m[SCAN_WRITE]}” उन्हें पन्ने पर हाथ की लिखावट के रूप में रखता है ताकि आप उन पर लिखकर अभ्यास कर सकें, और “{m[SCAN_TRANSLATE]}” उनका अनुवाद दिखाता है।',
    q_translate='अनुवाद कैसे करें?',
    a_translate='अपनी लिखावट को लैसो से घेरें और “{m[SCAN_TRANSLATE]}” चुनें, तो उसका अंग्रेज़ी अनुवाद दिखता है (ऐप हिंदी में इस्तेमाल करते समय अनुवाद हिंदी और अंग्रेज़ी के बीच होता है)। “{m[VOCAB]}” में “{m[SCAN_TRANSLATE]}” उल्टी दिशा में काम करता है: अंग्रेज़ी में टाइप करें और हिंदी पाएँ, साथ में उसके शब्द, जिन्हें आप सहेज सकते हैं या जिनका अभ्यास कर सकते हैं। सब कुछ आपके डिवाइस पर होता है: हर भाषा का मॉडल (लगभग {tr_mb} MB) एक बार डाउनलोड होता है, फिर बिना इंटरनेट के चलता है।',
    q_voice='आवाज़ नहीं है, या रोबोट जैसी लगती है',
    a_voice='{app} आपके डिवाइस की आवाज़ों से पढ़कर सुनाता है। iPad पर: {voice_ios} Android पर, सिस्टम सेटिंग्स में टेक्स्ट-टू-स्पीच इंजन (जैसे {tts_engine}) के लिए {taught} की आवाज़ इंस्टॉल करें; अगर कोई आवाज़ न हो, तो {app} में “{m[VOICE_BUTTON]}” आपके लिए वे सेटिंग्स खोल देता है।',
    q_reviews='दोहराव कैसे काम करता है?',
    a_reviews='{units} को “{m[STUDYING]}” के रूप में चिह्नित करें: “{m[REVIEWS]}” हर एक को उसका समय आने पर वापस लाता है — अगले दिन, फिर तीन दिन बाद, और हर बार सही लिखने पर और देर से; ग़लत लिखने पर जल्दी। नए अक्षर दिन में ज़्यादा से ज़्यादा दस जुड़ते हैं, ताकि एक साथ बहुत सारे चिह्नित करने पर अगला दिन भर न जाए। आपके सहेजे शब्दों का दोहराव अलग से “{m[VOCAB]}” में होता है।',
    q_sheet='क्या अभ्यास शीट प्रिंट की जा सकती हैं?',
    a_sheet='हाँ: किसी अक्षर के पेज पर, “{m[SELECT]}” मोड में या “{m[VOCAB]}” में “{m[SHEET]}” प्रिंट करने या साझा करने के लिए एक PDF बनाता है।',
    q_privacy='{app} क्या भेजता है, और किसे?',
    a_privacy_study='आपका काम आपके डिवाइस पर रहता है। Google ML Kit, जो डिवाइस पर ही लिखावट और छपा हुआ टेक्स्ट पढ़ता है और अनुवाद करता है, चालू रहने तक अपने मॉडल डाउनलोड करता है और Google को गुमनाम डायग्नोस्टिक डेटा भेजता है: इसे {mlkit_path} में जाकर बंद किया जा सकता है।',
    a_privacy_more='बाकी सब कुछ <a href="privacy.html#{lang}">निजता नीति</a> में है।',
    contact_title='संपर्क',
    contact_help='{mail} पर लिखें। अगर कुछ गड़बड़ हो, तो हमें अपने डिवाइस का मॉडल, सिस्टम का वर्शन और आप उस समय क्या कर रहे थे, यह बताएँ।',
)

TEXT['ar'] = dict(
    privacy='سياسة الخصوصية', help='المساعدة والدعم', updated='آخر تحديث: {date}', langs='اللغات', and_=' و',
    units={'char': ('حرفًا', 'الحروف'), 'letter': ('حرفًا', 'الحروف')},
    tts_engine='خدمات الكلام من Google',
    intro_study='{app} تطبيق لتعلّم كتابة {taught}، على iPad والأجهزة اللوحية العاملة بنظام Android. صُمّم ليعمل دون اتصال بالإنترنت: ما تكتبه وتتعلمه يبقى على جهازك.',
    h_short='باختصار',
    s_none='لا يحتوي {app} على حسابات ولا إعلانات ولا تتبّع، ولا يجمع أي معلومات عنك لنفسه.',
    s_local='كل ما تنشئه في {app} يُحفظ على جهازك فقط.',
    s_mlkit_switch='تستخدم بعض الميزات Google ML Kit، الذي يعمل على جهازك لكنه يرسل إلى Google بيانات تشخيص مجهولة الهوية ما دام مفعّلًا. ويمكنك إيقافه.',
    s_crash='إذا أُغلق {app} على نحو غير متوقع، فإنه يعرض عليك أن ترسل إلينا تقريرًا قصيرًا بالبريد الإلكتروني: لا يُرسل شيء ما لم ترسله أنت.',
    h_keeps='ما يحفظه {app}، وأين',
    k_study='يحفظ {app} صفحاتك وخط يدك، وملاحظاتك، وسجل تدريباتك واختباراتك، وجدول مراجعاتك، ومفرداتك وقوائمك، وملاحظاتك على الحروف، والمستندات التي تضيفها إلى «{m[LIBRARY_TITLE]}»، وإعداداتك — على جهازك فقط. ولا يرسلها {app} نفسه إلى أي مكان.',
    k_backup='مثل بيانات أي تطبيق، تدخل هذه البيانات في النسخ الاحتياطية لجهازك إذا كانت مفعّلة: نسخ iCloud الاحتياطي على iPad، والنسخ الاحتياطي من Google على Android (الذي يحتفظ بما لا يزيد على ٢٥ ميغابايت من بيانات كل تطبيق). وتحفظها Apple وGoogle وفق سياسة الخصوصية الخاصة بكل منهما.',
    k_export='يجمع الأمر «{export_path}» كل شيء في ملف واحد. ولا يذهب هذا الملف إلا إلى حيث تختار أن ترسله أو تحفظه.',
    h_mlkit='Google ML Kit',
    ml_uses_noocr='يستخدم {app} خدمة Google ML Kit لقراءة خط اليد («{m[SEL_CHECK]}»، «{m[SEL_COPY_TEXT]}»، «{m[SEL_SAVE_WORD]}»، الاختبارات، البحث بالكتابة) وللترجمة («{m[SCAN_TRANSLATE]}»). تتم القراءة والترجمة على جهازك: لا يُرسل خط يدك ولا النص الذي تترجمه إلى Google.',
    ml_dl_noocr='ينزّل ML Kit نماذجه من Google: نموذج خط اليد (نحو {ink_mb} ميغابايت) بمجرد تشغيل {app} أول مرة، ونموذج ترجمة (نحو {tr_mb} ميغابايت لكل لغة) أول مرة تترجم فيها.',
    ml_when_on='ما دام ML Kit مفعّلًا،',
    ml_diag='{when} تتلقى Google منه بيانات الاستخدام والتشخيص: معلومات الجهاز (الشركة المصنّعة، والطراز، وإصدار iPadOS أو Android)، ومعرّف التطبيق وإصداره، ومعرّفًا خاصًا بكل تثبيت، وأرقام الأداء، والأحداث (مثل تنزيل النماذج) ورموز الأخطاء، واللغة المضبوطة. تستخدمها Google للتشخيص وتحليل الاستخدام؛ ولا تُربط بهويتك، ولا تُستخدم للإعلانات أو التتبّع. تفصّلها Google لنظامَي <a href="{mlkit_ios_data}">iPadOS</a> و<a href="{mlkit_android_data}">Android</a>؛ وراجع أيضًا <a href="{google_privacy}">سياسة خصوصية Google</a> و<a href="{mlkit_terms}">شروط ML Kit</a>.',
    ml_switch_noocr='لإيقاف كل ذلك: {mlkit_path}. عندها لا يستدعي {app} خدمة ML Kit أبدًا: يقرأ خط اليد بنفسه (بدقة أقل قليلًا)، ولا تتوفر ميزة «{m[SCAN_TRANSLATE]}»، ولا يُنزَّل شيء ولا يُرسل شيء إلى Google عبر ML Kit.',
    h_camera='الكاميرا والصور',
    cam_scan='في «{m[LIBRARY_TITLE]}»، يحوّل زر «{m[LIBRARY_SCAN]}» الصفحات الورقية إلى مستند تقرؤه وتكتب عليه. على iPad يفتح كاميرا المستندات من Apple بعد أن تسمح باستخدام الكاميرا. وعلى Android يفتح ماسح المستندات من Google، الذي يعمل داخل خدمات Google Play وفق شروط Google، ويطلب إذن الكاميرا بنفسه، ولا يخضع لمفتاح ML Kit. وفي الحالتين لا يتلقى {app} إلا الصفحات التي تحتفظ بها.',
    cam_picker='يستخدم الخياران «{m[LIBRARY_PHOTOS]}» و«{m[LIBRARY_FILES]}» أدوات الاختيار الخاصة بالنظام: لا يتلقى {app} إلا الصور والملفات التي تختارها، ولا يطلب أبدًا الوصول إلى مكتبة صورك. وتُحفظ الصفحات التي تمسحها والصور والملفات التي تضيفها مع مستنداتك، على جهازك.',
    h_voice='القراءة بصوت عالٍ',
    voice='يقرأ {app} {taught} بصوت عالٍ بأصوات جهازك نفسه: أصوات iPadOS على iPad، وعلى Android محرك تحويل النص إلى كلام المختار في إعدادات النظام (مثل {tts_engine})، الذي يعمل وفق شروطه الخاصة.',
    h_crash='تقارير الأعطال',
    crash='إذا أُغلق {app} على نحو غير متوقع، يكتب تقريرًا قصيرًا على جهازك: إصدار التطبيق، وطراز الجهاز وإصدار النظام، والموضع الذي توقف فيه التطبيق، وآخر ما كان يفعله (مثل الشاشة التي كانت مفتوحة) — ولا شيء أبدًا مما كتبته أو رسمته أو سمّيته. وعندما تفتح {app} في المرة التالية يسألك: يفتح زر «{m[CRASH_SEND]}» رسالة بريد إلكتروني إلينا تتضمن التقرير لتقرأه قبل إرساله، ويحذفه زر «{m[CRASH_LATER]}». لا يُرسل شيء ما لم ترسل أنت تلك الرسالة.',
    h_links='الروابط والبريد الإلكتروني',
    links='لا يفتح {app} صفحة ويب أو رسالة بريد إلكتروني أو متجرًا إلا عندما تضغط على ما خُصّص لذلك: يفتح خيار «{m[FAQ_CONTACT]}» هذه الصفحة أو رسالة بريد إلكتروني إلينا، وينقلك زر «{m[RATE]}» إلى App Store أو Google Play.',
    h_stores='App Store وGoogle Play',
    stores='تتولى Apple (App Store) أو Google (Google Play) تنزيل {app} وأي عملية شراء وفق سياسة الخصوصية الخاصة بكل منهما، وكذلك تقارير الأعطال وبيانات الاستخدام التي يشاركها جهازك مع المطوّرين إذا سمحت بذلك في إعداداته.',
    h_children='الأطفال',
    children='لا يجمع {app} عن قصد أي معلومات شخصية عن أي أحد، بمن في ذلك الأطفال. وباستثناء التنزيلات الموصوفة أعلاه، فإن بيانات التشخيص الخاصة بـ ML Kit هي البيانات الوحيدة التي تغادر الجهاز من تلقاء نفسها، ولا تحدّد هوية أي شخص.',
    h_changes='التغييرات',
    changes='إذا تغيّرت هذه السياسة، فسيُنشر الإصدار الجديد في هذه الصفحة مع تاريخه.',
    h_contact='التواصل',
    contact_privacy='للأسئلة المتعلقة بالخصوصية: {mail}',
    q_start='من أين أبدأ؟',
    a_start_study='افتح القائمة في أعلى الشاشة: فيها كل ما تحتاجه للدراسة، ومعها «{m[REVIEWS]}» و«{m[VOCAB]}» و«{m[LIBRARY_TITLE]}» و«{m[EXAMS]}» و«{m[STATISTICS]}». اختر {unit} لترى ضرباته بالترتيب، ثم اكتبه بنفسك: يقرأ {app} ما تكتبه ويخبرك بمدى إتقانك. ويعرض «{m[TUTORIAL]}» في القائمة نفسها الخطوات الأولى من جديد.',
    q_pencil='هل أحتاج إلى Apple Pencil أو قلم إلكتروني؟',
    a_pencil_study='لا. عند تفعيل زر اليد في شريط الأدوات، تكتب بإصبع واحد وتحرّك الصفحة بإصبعين. ومع Apple Pencil، أو قلم إلكتروني على Android، يتحول {app} إليه تلقائيًا: يكتب القلم، وتحرّك الأصابع الصفحة، ويمكنك إسناد يدك على الشاشة.',
    q_offline='هل يعمل {app} دون اتصال بالإنترنت؟',
    a_offline_study='نعم. يُنزَّل نموذج خط اليد من Google ML Kit (نحو {ink_mb} ميغابايت) مرة واحدة عند تشغيل {app} أول مرة، ونموذج كل لغة لـ«{m[SCAN_TRANSLATE]}» أول مرة تترجم فيها؛ وبعد ذلك يعمل كل شيء دون اتصال.',
    q_data='أين يُحفظ عملي؟ وهل يمكنني نقله إلى جهاز جديد؟',
    a_data='على جهازك، وفي نسخ iCloud الاحتياطي أو النسخ الاحتياطي من Google إذا كان مفعّلًا. للاحتفاظ بنسخة بنفسك، أو للانتقال إلى iPad أو جهاز لوحي آخر، استخدم «{export_path}»، ثم «{m[DATA_IMPORT]}» على الجهاز الآخر (يستبدل ما عليه). ويصلح الملف على iPad وAndroid على حد سواء.',
    q_hand='لا يُقرأ خط يدي جيدًا',
    a_hand='في «{mlkit_where}»، يجب أن يكون خيار «{mlkit_name}» مفعّلًا ونموذجه منزّلًا (يحتاج إلى الإنترنت مرة واحدة). وإذا كان متوقفًا، يقرأ {app} خط اليد بنفسه، بدقة أقل.',
    q_photo='هل يستطيع {app} قراءة نص من كتاب أو لافتة؟',
    a_photo_noocr='ليس بعد: لا يستطيع Google ML Kit حتى الآن قراءة النص العربي المطبوع، لذا لا تتوفر ميزة «{m[SCAN_TITLE]}». لكن يمكنك إضافة صفحات لتقرأها وتكتب عليها: {m[LIBRARY_TITLE]} › {m[LIBRARY_SCAN]} أو {m[LIBRARY_PHOTOS]}.',
    q_translate='كيف أترجم؟',
    a_translate='أحِط ما كتبته بـ«{m[TOOL_LASSO]}» واختر «{m[SCAN_TRANSLATE]}» لتقرأه بالإنجليزية (عند استخدام التطبيق بالعربية تكون الترجمة بين العربية والإنجليزية). وفي «{m[VOCAB]}» يعمل زر «{m[SCAN_TRANSLATE]}» في الاتجاه المعاكس: اكتب بالإنجليزية واحصل على العربية، مع كلماتها لتحفظها أو تتدرب عليها. يحدث كل ذلك على جهازك: يُنزَّل نموذج كل لغة (نحو {tr_mb} ميغابايت) مرة واحدة، ثم يعمل دون اتصال.',
    q_voice='لا يوجد صوت، أو يبدو الصوت آليًا',
    a_voice='يقرأ {app} بصوت عالٍ بأصوات جهازك. على iPad: {voice_ios} وعلى Android، ثبّت صوتًا عربيًا لمحرك تحويل النص إلى كلام (مثل {tts_engine}) في إعدادات النظام؛ وإن لم يوجد صوت، يفتح لك زر «{m[VOICE_BUTTON]}» في {app} تلك الإعدادات.',
    q_reviews='كيف تعمل المراجعات؟',
    a_reviews='ضع علامة «{m[STUDYING]}» على {units}: تعيد شاشة «{m[REVIEWS]}» كل حرف عندما يحين موعده، في اليوم التالي، ثم بعد ثلاثة أيام، ثم بعد مدة أطول في كل مرة تكتبه فيها صحيحًا، وأقرب إن أخطأت. ولا تدخل الحروف الجديدة إلا عشرةً في اليوم على الأكثر، حتى لا يزدحم اليوم التالي إذا وضعت علامات على كثير منها دفعة واحدة. وللكلمات التي تحفظها مراجعاتها الخاصة في «{m[VOCAB]}».',
    q_sheet='هل يمكنني طباعة أوراق تدريب؟',
    a_sheet='نعم: ينشئ زر «{m[SHEET]}»، في صفحة أي حرف، أو في وضع «{m[SELECT]}»، أو في «{m[VOCAB]}»، ملف PDF للطباعة أو المشاركة.',
    q_privacy='ماذا يرسل {app}، وإلى من؟',
    a_privacy_study_noocr='يبقى عملك على جهازك. أما Google ML Kit، الذي يقرأ خط اليد ويترجم على الجهاز، فينزّل نماذجه ويرسل إلى Google بيانات تشخيص مجهولة الهوية ما دام مفعّلًا: يمكنك إيقافه من {mlkit_path}.',
    a_privacy_more='وكل ما عدا ذلك تجده في <a href="privacy.html#{lang}">سياسة الخصوصية</a>.',
    contact_title='التواصل',
    contact_help='راسلنا على {mail}. وإذا حدثت مشكلة، فأخبرنا بطراز جهازك وإصدار النظام وما كنت تفعله.',
)

# =====================================================================================================================
# The app's own words: its strings
# =====================================================================================================================

def read_strings(app):
    """The app's shipped strings: {page language: {id: text}}, in the order the app has its languages."""
    path = os.path.join(ROOT, 'apps', app, 'assets', 'text', 'strings.rdel')
    blocks, cur = {}, None
    for line in open(path, encoding='utf-8'):
        line = line.rstrip('\n')
        if re.match(r'^[A-Z]{2}-[A-Z]{2}:$', line):
            if line[:-1] not in RDEL_LANG:
                sys.exit('%s: a language this tool has no texts for: %s (add it to LANGS and TEXT)' % (path, line[:-1]))
            cur = blocks.setdefault(RDEL_LANG[line[:-1]], {})
        elif cur is not None and '=' in line and not line.startswith(('@', '//')):
            k, v = line.split('=', 1)
            cur[k] = v   # (a right-to-left language's Latin names keep the strings tool's left-to-right mark after them)
    return blocks

def app_name(app):
    """The app's name, from its src/version.h, as the strings tool and the C code take it."""
    text = open(os.path.join(ROOT, 'apps', app, 'src', 'version.h'), encoding='utf-8').read()
    return re.search(r'#define\s+\w+_NAME\s+"([^"]+)"', text).group(1)

class Labels:
    """{m[ID]} in a paragraph: the app's label for that id in the page's language, ready for HTML."""
    def __init__(self, app, lang, strings):
        self.app, self.lang, self.strings = app, lang, strings

    def __getitem__(self, key):
        if key not in self.strings:
            sys.exit('%s (%s): no string %s in apps/%s/assets/text/strings.rdel' % (self.app, self.lang, key, self.app))
        s = self.strings[key]
        if key in SECTION_HEADERS and s.isupper():
            s = s[0] + s[1:].lower()
        return html.escape(s, quote=False)

# =====================================================================================================================
# Composing the pages: which paragraphs each app's facts call for
# =====================================================================================================================

class Page:
    """One app's page in one language: its paragraphs filled from the app's facts and words."""
    def __init__(self, app, lang, strings):
        self.app, self.lang, self.facts = app, lang, APPS[app]
        self.text = TEXT.get(lang)
        if self.text is None:
            sys.exit('%s is in %s, which has no TEXT here: write TEXT[%r]' % (app, lang, lang))
        a, m = self.facts, Labels(app, lang, strings[lang])
        rtl = LANGS[lang][1] == 'rtl'
        name = html.escape(app_name(app))
        # (a Latin name in a right-to-left sentence is isolated, or its "!" would land on the wrong side)
        mail = '<a href="mailto:%s">%s</a>' % (CONTACT, CONTACT)
        self.ctx = dict(m=m, lang=lang, app='<bdi>%s</bdi>' % name if rtl else name, mail='<bdi>%s</bdi>' % mail if rtl else mail,
                        google_privacy=GOOGLE_PRIVACY, mlkit_terms=MLKIT_TERMS, mlkit_ios_data=MLKIT_IOS_DATA,
                        mlkit_android_data=MLKIT_AND_DATA, hf_privacy=HF_PRIVACY,
                        ink_mb=digits(lang, 20), tr_mb=digits(lang, 30), speech_mb=digits(lang, 57),
                        export_path=' › '.join(m[i] for i in ('SETTINGS', 'DATA', 'DATA_EXPORT')))
        if 'tts_engine' in self.text:
            self.ctx['tts_engine'] = self.text['tts_engine']
        if a['kind'] == 'study':
            taught = TAUGHT[a['taught']][lang]
            self.ctx['taught'], self.ctx['taught_the'] = taught if isinstance(taught, tuple) else (taught, taught)
            unit_one, units = self.text['units'][a['unit']]
            # (the app's own sentence on getting a better voice on iPad, which names iPadOS's menus in this language; in a
            # language with capitals it follows a colon, so it starts in lower case)
            voice = strings[lang]['SPEECH_BETTER_VOICE']
            if lang in ('en', 'es', 'pt', 'fr'):
                voice = voice[0].lower() + voice[1:]
            self.ctx.update(unit=unit_one, units=units, voice_ios=html.escape(voice, quote=False))
        if a['mlkit_switch']:
            self.ctx['mlkit_path'] = ' › '.join(m[i] for i in a['mlkit_switch'])
            self.ctx['mlkit_where'] = ' › '.join(m[i] for i in a['mlkit_switch'][:-1])
            self.ctx['mlkit_name'] = m[a['mlkit_switch'][-1]]
        if a['kind'] == 'canvas':
            labels = [m[i] for i in a['exports']]
            self.ctx['exports'] = ', '.join(labels[:-1]) + self.text['and_'] + labels[-1]
        # (a study app's ML Kit works from the start, its handwriting model downloading at launch; a canvas app's only
        # when handwriting is read)
        self.ctx['when'] = self.text['ml_when_on' if a['kind'] == 'study' else 'ml_when_used']
        # (French elides before a vowel: d’InfiNote, qu’Arabic Learn!, but de Kana Learn!)
        vowel = app_name(app)[0] in 'AEIOUaeiou'
        self.ctx['de_app'] = ('d’' if vowel else 'de ') + self.ctx['app']
        self.ctx['que_app'] = ('qu’' if vowel else 'que ') + self.ctx['app']
        self.ctx['about'] = a['about'][lang].format(**self.ctx)

    def join(self, *parts):
        """Sentences one after another: with a space, except in the languages that write none between them."""
        return self.text.get('sp', ' ').join(p for p in parts if p)

    def t(self, key):
        """A paragraph of this page's language, filled in."""
        if key not in self.text:
            sys.exit('%s: TEXT[%r] has no %r, which %s needs: write it' % (self.app, self.lang, key, self.app))
        return self.text[key].format(**self.ctx)

    def privacy(self):
        """The privacy policy's sections: [(heading, [paragraph, or ('ul', [items])])]."""
        a, t = self.facts, self.t
        study, canvas = a['kind'] == 'study', a['kind'] == 'canvas'
        rec = canvas and a['microphone']
        if study:
            mlkit = t('s_mlkit_switch')
        else:
            mlkit = self.join(t('s_mlkit_canvas'), a['mlkit_switch'] and t('s_can_off'))
        short = [t('s_none'), t('s_local'), mlkit]
        if rec and a['transcribes']:
            short.append(t('s_speech'))
        short.append(t('s_crash'))
        s = [(t('h_short'), [('ul', short)])]
        s.append((t('h_keeps'), [t('k_study') if study else t('k_canvas_rec') if rec else t('k_canvas'), t('k_backup'), t('k_export')]))
        if study:
            ml = [t('ml_uses_ocr' if a['ocr'] else 'ml_uses_noocr'), t('ml_dl_ocr' if a['ocr'] else 'ml_dl_noocr')]
        else:
            ml = [t('ml_uses_canvas'), t('ml_dl_canvas' if a['mlkit_switch'] else 'ml_dl_canvas_noswitch')]
        ml.append(t('ml_diag'))
        if a['mlkit_switch']:
            ml.append(t(('ml_switch_ocr' if a['ocr'] else 'ml_switch_noocr') if study else 'ml_switch_canvas'))
        else:
            ml.append(t('ml_noswitch_canvas'))
        s.append((t('h_mlkit'), ml))
        if study:
            s.append((t('h_camera'), ([t('cam_ocr')] if a['ocr'] else []) + [t('cam_scan'), t('cam_picker')]))
        else:
            s.append((t('h_camera'), [t('cam_none'), t('pic_canvas'), t('photos_add')]))
        if rec:
            s.append((t('h_mic'), [t('mic')]))
            if a['transcribes'] == ('android',):
                s.append((t('h_speech'), [t('speech_android'), t('speech_ios_none')]))
        if study:
            s.append((t('h_voice'), [t('voice')]))
        s.append((t('h_crash'), [t('crash')]))
        s.append((t('h_links'), [t('links')] + ([t('links_note')] if canvas else [])))
        s.append((t('h_stores'), [t('stores')]))
        s.append((t('h_children'), [t('children')]))
        s.append((t('h_changes'), [t('changes')]))
        s.append((t('h_contact'), [t('contact_privacy')]))
        return s

    def faq(self):
        """The help page's questions and answers: [(question, answer)]."""
        a, t = self.facts, self.t
        if a['kind'] == 'study':
            q = [('q_start', t('a_start_study')), ('q_pencil', t('a_pencil_study')), ('q_offline', t('a_offline_study')),
                 ('q_data', t('a_data')), ('q_hand', t('a_hand')),
                 ('q_photo', t('a_photo_ocr' if a['ocr'] else 'a_photo_noocr')), ('q_translate', t('a_translate')),
                 ('q_voice', t('a_voice')), ('q_reviews', t('a_reviews')), ('q_sheet', t('a_sheet')),
                 ('q_privacy', self.join(t('a_privacy_study' if a['ocr'] else 'a_privacy_study_noocr'), t('a_privacy_more')))]
        else:
            rec = a['microphone']
            speech = rec and a['transcribes']
            offline = self.join(t('a_offline_canvas'), speech and t('a_offline_speech'))
            privacy = self.join(t('a_privacy_canvas'), a['mlkit_switch'] and t('a_privacy_switch'), speech and t('a_privacy_speech'),
                                t('a_privacy_more'))
            q = [('q_start', t('a_start_notes' if a['home_new'] == 'ZOOM_HOME_NEW_NOTEBOOK' else 'a_start_workshop')),
                 ('q_pencil', t('a_pencil_canvas')), ('q_offline', offline)]
            if rec:
                q.append(('q_record', t('a_record')))
                if a['transcribes']:
                    q.append(('q_transcribe', t('a_transcribe')))
            q += [('q_totext', self.join(t('a_totext'), a['mlkit_switch'] and t('a_totext_off'))), ('q_import', t('a_import')), ('q_export', t('a_export')),
                  ('q_data', t('a_data')), ('q_privacy', privacy)]
        return [(t(k), v) for k, v in q]

# =====================================================================================================================
# The HTML: one stylesheet, one script, every language in one file
# =====================================================================================================================
# One file a page with all the app's languages in it, shown one at a time: the in-app link is one address for every
# language (side.c opens faq_url as it is), so the page picks the reader's language itself: the #hash first (a
# language button, or a link from the other page), then the browser's languages, then English. Without JavaScript
# every language shows, one after another, each with its own lang and dir. The stylesheet uses no left or right, only
# directions that follow the text (padding-inline, flex), so a right-to-left section mirrors as a whole.

CSS = """
:root { --page:#f6f3ec; --ink:#1d1d22; --soft:#5f5f6b; --line:#ddd8cc; --card:#fffdf8; --accent:#3b6fe0; --on-accent:#fff; }
@media (prefers-color-scheme: dark) { :root { --page:#17181c; --ink:#ececf0; --soft:#a3a3ae; --line:#2e3038; --card:#1f2026; --accent:#7ea2ff; --on-accent:#10131a; } }
* { box-sizing:border-box; }
html { -webkit-text-size-adjust:100%; text-size-adjust:100%; }
body { margin:0; background:var(--page); color:var(--ink); font:16px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Noto Sans", sans-serif; overflow-wrap:break-word; }
:lang(ja) { font-family:-apple-system, "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Noto Sans JP", "Yu Gothic", sans-serif; }
:lang(zh) { font-family:-apple-system, "PingFang SC", "Noto Sans SC", "Microsoft YaHei", sans-serif; }
:lang(ko) { font-family:-apple-system, "Apple SD Gothic Neo", "Noto Sans KR", "Malgun Gothic", sans-serif; word-break:keep-all; }
:lang(th) { font-family:-apple-system, "Thonburi", "Noto Sans Thai", "Leelawadee UI", sans-serif; line-height:1.8; }
:lang(hi) { font-family:-apple-system, "Kohinoor Devanagari", "Noto Sans Devanagari", "Nirmala UI", sans-serif; line-height:1.8; }
:lang(ar) { font-family:-apple-system, "Geeza Pro", "Noto Naskh Arabic", "Noto Sans Arabic", "Segoe UI", sans-serif; line-height:1.9; }
main { max-width:760px; margin:0 auto; padding:32px 16px 64px; }
header { display:flex; align-items:center; gap:14px; margin-bottom:8px; }
.mark { width:44px; height:44px; border-radius:11px; background:var(--accent); color:var(--on-accent); display:grid; place-items:center; font-size:24px; line-height:1; flex:none; }
h1 { font-size:26px; line-height:1.25; margin:0; }
h2 { font-size:21px; line-height:1.3; margin:8px 0 4px; }
h3 { font-size:17px; line-height:1.35; margin:28px 0 4px; }
.card h3 { margin-top:20px; }
ul { padding-inline-start:22px; }
li { margin:6px 0; }
.soft { color:var(--soft); font-size:14px; }
nav.langs { display:flex; flex-wrap:wrap; gap:8px; margin:18px 0 22px; }
nav.langs a { padding:6px 12px; border-radius:999px; border:1px solid var(--line); color:var(--ink); text-decoration:none; font-size:14px; }
nav.langs a.on { background:var(--accent); border-color:var(--accent); color:var(--on-accent); }
a { color:var(--accent); }
a:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
section.lang { display:none; }
section.lang.on { display:block; }
.card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:2px 18px 14px; margin-top:18px; }
footer { margin-top:40px; border-top:1px solid var(--line); padding-top:14px; }
"""

JS = """
(function(){
  var codes = %s, pick = (location.hash || '').slice(1);
  if(codes.indexOf(pick) < 0) {
    var langs = navigator.languages || [navigator.language || 'en'];
    for(var i = 0; i < langs.length && codes.indexOf(pick) < 0; i++) { pick = (langs[i] || '').slice(0, 2).toLowerCase(); }
  }
  if(codes.indexOf(pick) < 0) { pick = 'en'; }
  function show(c){
    codes.forEach(function(x){
      var s = document.getElementById('s-' + x), l = document.getElementById('l-' + x), on = x === c;
      s.classList.toggle('on', on);
      l.classList.toggle('on', on);
      if(on) {
        l.setAttribute('aria-current', 'true');
        document.documentElement.lang = s.getAttribute('lang');
        document.documentElement.dir = s.getAttribute('dir');
        document.title = s.getAttribute('data-title');
      } else {
        l.removeAttribute('aria-current');
      }
    });
  }
  codes.forEach(function(c){ document.getElementById('l-' + c).addEventListener('click', function(e){ e.preventDefault(); history.replaceState(null, '', '#' + c); show(c); }); });
  show(pick);
})();
"""

def title(app, lang, page):
    # (a plain-text title cannot isolate a Latin name in a right-to-left one: a left-to-right mark after it does)
    return '%s%s — %s' % (app_name(app), '‎' if LANGS[lang][1] == 'rtl' else '', TEXT[lang][page])

def document(app, langs, page, sections):
    """The whole page: _sections is {lang: inner HTML}."""
    first = langs[0]
    nav = ''.join('<a id="l-%s" href="#%s" hreflang="%s" lang="%s">%s</a>' % (c, c, LANGS[c][0], LANGS[c][0], LANGS[c][2]) for c in langs)
    body = '\n'.join('<section class="lang" id="s-%s" lang="%s" dir="%s" data-title="%s">\n%s\n</section>' % (
        c, LANGS[c][0], LANGS[c][1], html.escape(title(app, c, page)), sections[c]) for c in langs)
    return """<!doctype html>
<html lang="%s" dir="%s">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>%s</title>
<style>%s</style>
</head>
<body>
<main>
<header><div class="mark" aria-hidden="true">%s</div><h1><bdi>%s</bdi></h1></header>
<nav class="langs" aria-label="%s">%s</nav>
<noscript><style>section.lang { display:block; border-top:1px solid var(--line); margin-top:24px; }</style></noscript>
%s
</main>
<script>%s</script>
</body>
</html>
""" % (LANGS[first][0], LANGS[first][1], html.escape(title(app, first, page)), CSS, APPS[app]['mark'],
       html.escape(app_name(app)), html.escape(TEXT[first]['langs']), nav, body,
       JS % ('[' + ','.join("'%s'" % c for c in langs) + ']'))

def privacy_html(app, strings):
    langs, out = list(strings), {}
    for c in langs:
        p = Page(app, c, strings)
        body = ['<h2>%s</h2>' % html.escape(TEXT[c]['privacy']),
                '<p class="soft">%s</p>' % html.escape(TEXT[c]['updated'].format(date=date(c))),
                '<p>%s</p>' % p.t('intro_study' if p.facts['kind'] == 'study' else 'intro_canvas')]
        for heading, items in p.privacy():
            body.append('<h3>%s</h3>' % heading)
            for it in items:
                if isinstance(it, tuple):
                    body.append('<ul>' + ''.join('<li>%s</li>' % x for x in it[1]) + '</ul>')
                else:
                    body.append('<p>%s</p>' % it)
        body.append('<footer><a href="index.html#%s">%s</a></footer>' % (c, html.escape(TEXT[c]['help'])))
        out[c] = '\n'.join(body)
    return document(app, langs, 'privacy', out)

def help_html(app, strings):
    langs, out = list(strings), {}
    for c in langs:
        p = Page(app, c, strings)
        body = ['<h2>%s</h2>' % html.escape(TEXT[c]['help']), '<p>%s</p>' % p.ctx['about'], '<div class="card">']
        for q, a in p.faq():
            body.append('<h3>%s</h3>\n<p>%s</p>' % (q, a))
        body.append('</div>')
        body.append('<h3>%s</h3>\n<p>%s</p>' % (html.escape(TEXT[c]['contact_title']), p.t('contact_help')))
        body.append('<footer><a href="privacy.html#%s">%s</a></footer>' % (c, html.escape(TEXT[c]['privacy'])))
        out[c] = '\n'.join(body)
    return document(app, langs, 'help', out)

def hosting_index(apps):
    """The root of the hosting repository: every app, its help page and its privacy policy (English: a list of names)."""
    rows = ''.join('<li><a href="%s/"><bdi>%s</bdi></a> · <a href="%s/privacy.html">%s</a></li>' % (
        a, html.escape(app_name(a)), a, html.escape(TEXT['en']['privacy'])) for a in apps)
    return """<!doctype html>
<html lang="en" dir="ltr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>Apps — Help and privacy</title>
<style>%s</style>
</head>
<body>
<main>
<h1>Help and privacy</h1>
<p>Each app’s help page and privacy policy, in the app’s own languages.</p>
<ul>%s</ul>
<p class="soft">Contact: <a href="mailto:%s">%s</a></p>
</main>
</body>
</html>
""" % (CSS, rows, CONTACT, CONTACT)

# =====================================================================================================================
# The checks: run on every page written
# =====================================================================================================================
# The rules every page keeps, checked on the HTML itself rather than on the paragraphs, so that a menu name read from
# the strings is checked too: the HTML is well formed (every element closed, in order); the prose has no ASCII
# apostrophe (the typographic one, U+2019, as the app writes it) and no text cut short with "..." or "…"; no
# placeholder is left unfilled; every language the app has is there with its lang, and the right-to-left one with
# dir="rtl"; the headings go down one level at a time.

VOID = {'meta', 'br', 'img', 'link', 'input', 'hr', 'area', 'base', 'col', 'embed', 'source', 'track', 'wbr'}

class Checker(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack, self.problems, self.sections, self.level, self.raw = [], [], {}, 0, 0

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag in ('script', 'style'):
            self.raw += 1
        if tag == 'section':
            self.sections[a.get('id', '')[2:]] = a
            self.level = 1   # (each section starts under the page's h1)
        if re.match(r'^h[1-6]$', tag):
            n = int(tag[1])
            if n > self.level + 1:
                self.problems.append('a heading skips a level: <%s> after <h%d>' % (tag, self.level))
            self.level = n
        if tag not in VOID:
            self.stack.append(tag)

    def handle_endtag(self, tag):
        if tag in ('script', 'style'):
            self.raw -= 1
        if tag in VOID:
            return
        if not self.stack or self.stack[-1] != tag:
            self.problems.append('</%s> does not close <%s>' % (tag, self.stack[-1] if self.stack else 'nothing'))
            return
        self.stack.pop()

    def handle_data(self, data):
        if self.raw:
            return
        for bad, why in (("'", 'an ASCII apostrophe (write ’)'), ('...', 'three dots'), ('…', 'an ellipsis'),
                         ('{', 'an unfilled placeholder'), ('}', 'an unfilled placeholder')):
            if bad in data:
                self.problems.append('%s in: %s' % (why, data.strip()[:100]))

def check(path, text, langs):
    c = Checker()
    c.feed(text)
    c.close()
    problems = list(c.problems)
    if c.stack:
        problems.append('left open: ' + ' '.join(c.stack))
    for code in langs:
        s = c.sections.get(code)
        if s is None:
            problems.append('no section for ' + code)
        elif s.get('lang') != LANGS[code][0] or s.get('dir') != LANGS[code][1]:
            problems.append('%s: lang=%r dir=%r' % (code, s.get('lang'), s.get('dir')))
    problems += ['a section for a language the app does not have: ' + code for code in c.sections if code not in langs]
    return ['%s: %s' % (os.path.relpath(path, ROOT), p) for p in problems]

# =====================================================================================================================

def main(argv):
    pages_dir = None
    if '--pages' in argv:
        i = argv.index('--pages')
        if i + 1 >= len(argv):
            sys.exit('--pages needs a folder')
        pages_dir = os.path.abspath(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    apps = argv or RELEASED
    for a in apps:
        if a not in APPS:
            sys.exit('no pages for %s (one of: %s)' % (a, ', '.join(RELEASED)))
    problems = []
    for a in apps:
        strings = read_strings(a)
        out = os.path.join(ROOT, 'apps', a, 'site')
        os.makedirs(out, exist_ok=True)
        for name, text in (('index.html', help_html(a, strings)), ('privacy.html', privacy_html(a, strings))):
            path = os.path.join(out, name)
            problems += check(path, text, list(strings))
            with open(path, 'w', encoding='utf-8') as f:
                f.write(text)
            if pages_dir:
                os.makedirs(os.path.join(pages_dir, a), exist_ok=True)
                shutil.copyfile(path, os.path.join(pages_dir, a, name))
        print('%-15s %s  (%s)' % (a, os.path.relpath(out, ROOT), ' '.join(strings)))
    if pages_dir:
        text = hosting_index([a for a in RELEASED if a in apps])
        problems += check(os.path.join(pages_dir, 'index.html'), text, [])
        with open(os.path.join(pages_dir, 'index.html'), 'w', encoding='utf-8') as f:
            f.write(text)
        open(os.path.join(pages_dir, '.nojekyll'), 'w').close()
        print('%-15s %s' % ('(hosting)', pages_dir))
    if problems:
        print('\n'.join(problems))
        sys.exit(1)

if __name__ == '__main__':
    sys.dont_write_bytecode = True
    main(sys.argv[1:])
