# Kana Learn!: App Store and Google Play texts

Kana Learn!’s texts for the App Store and Google Play, in its five languages. They replace the App Store-only listing of 2026-10-02: the name is the app’s, Kana Learn!, everywhere it is meant; the texts now say that the handwriting model downloads at the first launch while Google ML Kit is on, and how to switch ML Kit off; every language has a Google Play version; and no price is promised (see Price below).

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 6,430 CJK characters (6,416 with readings and meanings; the 2026-10-02 listing and the screenshots say 6,412: the texts now say “more than 6,400”), 90 hiragana and 94 katakana, 54,362 words and 14,595 example sentences). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases): the 2026-10-02 listing suggested a one-time purchase of USD 9.99, with the App Store Small Business Program (15% commission). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/kana/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/kana/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/kana/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Japanese = Play `ja-JP`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Japanese:** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.
- **Screenshots:** `README.md` and `framed/` (iPad 13-inch; Google Play’s are still to make). Kana’s permissions, privacy manifests and licences: `../docs/app_store.md`.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Kana Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Kanji (or Kana), then any character: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few characters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. Tap the camera on the toolbar (or press and hold the page › Text from photo): the camera reads Japanese text live; Hold keeps the lines, Write puts them on the page, Translate with Google translates them. Choosing a photo works too.
4. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
5. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Japanese). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting, text recognition and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. The text recognition model is inside the app. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Text from a photo and Translate with Google are unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Text from a photo or Lectures › Scan pages is first used. The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Japanese, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | ja | fr |
|---|---|---|---|---|---|
| App Store name (30) | 27 | 28 | 28 | 20 | 30 |
| App Store subtitle (30) | 30 | 27 | 27 | 10 | 30 |
| App Store keywords (100) | 99 | 99 | 95 | 53 | 94 |
| App Store promotional text (170) | 158 | 155 | 149 | 80 | 160 |
| App Store description (4000) | 3349 | 3545 | 3557 | 1631 | 3836 |
| Google Play title (30) | 27 | 28 | 28 | 20 | 30 |
| Google Play short description (80) | 77 | 72 | 70 | 40 | 79 |
| Google Play full description (4000) | 3377 | 3586 | 3591 | 1649 | 3873 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (27/30): Kana Learn!: Write Japanese

**Subtitle** (30/30): Kanji & kana, stroke by stroke

**Keywords** (99/100): hiragana,katakana,handwriting,jlpt,nihongo,vocabulary,translate,ocr,scanner,dictionary,pencil,study

**Promotional text** (158/170): Write Japanese by hand and Kana Learn! reads it. Point the camera at Japanese and translate it. Scan your textbook and write on it. No account, works offline.

**Description** (3349/4000):

```text
Write Japanese by hand, and Kana Learn! reads it. Point the camera at Japanese, and it translates it. Scan your textbook, and write on it.

Kana Learn! is a Japanese notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: kanji and kana, written with the Apple Pencil or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Japanese.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every kanji writing itself, with words and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

WRITE, AND KANA LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Japanese text, and Kana Learn! writes it out by hand on your page.
• Five themes, from washi paper to night.

TRANSLATE, BOTH WAYS
• From the camera: Kana Learn! finds the Japanese, translates it line by line, and writes the lines you keep on your page.
• Into Japanese: type a word or a sentence in your language, get it in Japanese, then save its words, write it on the page or practice its characters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

EVERY KANJI, STROKE BY STROKE
• More than 6,400 kanji and all the hiragana and katakana, each one writing itself in the right order.
• Readings, meanings, parts, stroke counts, school grades and JLPT levels from N5 to N1.
• More than 54,000 words and 14,500 example sentences, read aloud by your iPad’s voices.
• Can’t type a kanji? Draw it, or pick its parts, and Kana Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by JLPT level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: First Year Japanese I (handouts and worksheets from a college course) and Wikivoyage’s Japanese phrasebook.
• Bring your own from Files, Photos or the document camera. Kana Learn! reads the Japanese in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Kana Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Japanese and French.
```

### Google Play

**Title** (27/30): Kana Learn!: Write Japanese

**Short description** (77/80): Write Japanese by hand: kanji and kana stroke by stroke, read and translated.

**Full description** (3377/4000):

```text
Write Japanese by hand, and Kana Learn! reads it. Point the camera at Japanese, and it translates it. Scan your textbook, and write on it.

Kana Learn! is a Japanese notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: kanji and kana, written with a stylus or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Japanese.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every kanji writing itself, with words and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

Write, and Kana Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Japanese text, and Kana Learn! writes it out by hand on your page.
• Five themes, from washi paper to night.

Translate, both ways
• From the camera: Kana Learn! finds the Japanese, translates it line by line, and writes the lines you keep on your page.
• Into Japanese: type a word or a sentence in your language, get it in Japanese, then save its words, write it on the page or practice its characters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

Every kanji, stroke by stroke
• More than 6,400 kanji and all the hiragana and katakana, each one writing itself in the right order.
• Readings, meanings, parts, stroke counts, school grades and JLPT levels from N5 to N1.
• More than 54,000 words and 14,500 example sentences, read aloud by your tablet’s voices.
• Can’t type a kanji? Draw it, or pick its parts, and Kana Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by JLPT level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: First Year Japanese I (handouts and worksheets from a college course) and Wikivoyage’s Japanese phrasebook.
• Bring your own from your files and photos, or with Google’s document scanner. Kana Learn! reads the Japanese in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Kana Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Japanese and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (28/30): Kana Learn!: escribe japonés

**Subtitle** (27/30): Kanji y kana, trazo a trazo

**Keywords** (99/100): hiragana,katakana,escritura,jlpt,nihongo,vocabulario,traductor,ocr,escáner,diccionario,pencil,letra

**Promotional text** (155/170): Escribe japonés a mano y Kana Learn! lo lee. Apunta la cámara al japonés y tradúcelo. Escanea tu libro y escribe encima. Sin cuenta, funciona sin conexión.

**Description** (3545/4000):

```text
Escribe japonés a mano y Kana Learn! lo lee. Apunta la cámara a un texto en japonés y te lo traduce. Escanea tu libro de texto y escribe encima.

Kana Learn! es un cuaderno de japonés para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: kanji y kana, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al japonés.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada kanji, con palabras y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y KANA LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en japonés y Kana Learn! lo escribe a mano en tu página.
• Cinco temas, del papel washi a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Con la cámara: Kana Learn! encuentra el japonés, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al japonés: escribe una palabra o una frase en tu idioma, obtenla en japonés y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

CADA KANJI, TRAZO A TRAZO
• Más de 6.400 kanji y todos los hiragana y katakana, cada uno escribiéndose en su orden.
• Lecturas, significados, partes, número de trazos, curso escolar y nivel del JLPT, del N5 al N1.
• Más de 54.000 palabras y 14.500 frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No sabes teclear un kanji? Dibújalo, o elige sus partes, y Kana Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel del JLPT, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: First Year Japanese I (apuntes y ejercicios de un curso universitario, en inglés) y la guía de japonés de Wikiviajes, en español.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos. Kana Learn! lee el japonés que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Kana Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, japonés y francés.
```

### Google Play

**Title** (28/30): Kana Learn!: escribe japonés

**Short description** (72/80): Escribe japonés a mano: kanji y kana trazo a trazo, leídos y traducidos.

**Full description** (3586/4000):

```text
Escribe japonés a mano y Kana Learn! lo lee. Apunta la cámara a un texto en japonés y te lo traduce. Escanea tu libro de texto y escribe encima.

Kana Learn! es un cuaderno de japonés para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: kanji y kana, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al japonés.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada kanji, con palabras y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Kana Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en japonés y Kana Learn! lo escribe a mano en tu página.
• Cinco temas, del papel washi a la noche.

Traduce en los dos sentidos
• Con la cámara: Kana Learn! encuentra el japonés, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al japonés: escribe una palabra o una frase en tu idioma, obtenla en japonés y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

Cada kanji, trazo a trazo
• Más de 6.400 kanji y todos los hiragana y katakana, cada uno escribiéndose en su orden.
• Lecturas, significados, partes, número de trazos, curso escolar y nivel del JLPT, del N5 al N1.
• Más de 54.000 palabras y 14.500 frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No sabes teclear un kanji? Dibújalo, o elige sus partes, y Kana Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel del JLPT, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: First Year Japanese I (apuntes y ejercicios de un curso universitario, en inglés) y la guía de japonés de Wikiviajes, en español.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google. Kana Learn! lee el japonés que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Kana Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, japonés y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (28/30): Kana Learn!: escreva japonês

**Subtitle** (27/30): Kanji e kana, traço a traço

**Keywords** (95/100): hiragana,katakana,escrita,jlpt,nihongo,vocabulário,tradutor,ocr,scanner,dicionário,pencil,letra

**Promotional text** (149/170): Escreva japonês à mão e o Kana Learn! lê. Aponte a câmera para o japonês e traduza. Digitalize seu livro e escreva nele. Sem conta, funciona offline.

**Description** (3557/4000):

```text
Escreva japonês à mão e o Kana Learn! lê. Aponte a câmera para um texto em japonês e ele traduz. Digitalize seu livro e escreva nele.

O Kana Learn! é um caderno de japonês para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: kanji e kana, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o japonês.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada kanji sendo escrito, com palavras e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O KANA LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em japonês e o Kana Learn! o escreve à mão na sua página.
• Cinco temas, do papel washi à noite.

TRADUZA NOS DOIS SENTIDOS
• Pela câmera: o Kana Learn! encontra o japonês, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o japonês: digite uma palavra ou uma frase no seu idioma, veja em japonês e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

CADA KANJI, TRAÇO A TRAÇO
• Mais de 6.400 kanji e todos os hiragana e katakana, cada um se escrevendo na ordem certa.
• Leituras, significados, partes, número de traços, série escolar e nível do JLPT, do N5 ao N1.
• Mais de 54.000 palavras e 14.500 frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não sabe digitar um kanji? Desenhe, ou escolha as partes dele, e o Kana Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível do JLPT, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: First Year Japanese I (material e exercícios de um curso universitário, em inglês) e o guia de conversação japonês do Wikivoyage, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos. O Kana Learn! lê o japonês nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Kana Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, japonês e francês.
```

### Google Play

**Title** (28/30): Kana Learn!: escreva japonês

**Short description** (70/80): Escreva japonês à mão: kanji e kana traço a traço, lidos e traduzidos.

**Full description** (3591/4000):

```text
Escreva japonês à mão e o Kana Learn! lê. Aponte a câmera para um texto em japonês e ele traduz. Digitalize seu livro e escreva nele.

O Kana Learn! é um caderno de japonês para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: kanji e kana, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o japonês.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada kanji sendo escrito, com palavras e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Kana Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em japonês e o Kana Learn! o escreve à mão na sua página.
• Cinco temas, do papel washi à noite.

Traduza nos dois sentidos
• Pela câmera: o Kana Learn! encontra o japonês, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o japonês: digite uma palavra ou uma frase no seu idioma, veja em japonês e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

Cada kanji, traço a traço
• Mais de 6.400 kanji e todos os hiragana e katakana, cada um se escrevendo na ordem certa.
• Leituras, significados, partes, número de traços, série escolar e nível do JLPT, do N5 ao N1.
• Mais de 54.000 palavras e 14.500 frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não sabe digitar um kanji? Desenhe, ou escolha as partes dele, e o Kana Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível do JLPT, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: First Year Japanese I (material e exercícios de um curso universitário, em inglês) e o guia de conversação japonês do Wikivoyage, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google. O Kana Learn! lê o japonês nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Kana Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, japonês e francês.
```

## Japanese (Google Play: ja-JP)

### App Store

**Name** (20/30): Kana Learn!: 手で書く日本語

**Subtitle** (10/30): 漢字とかなを一画ずつ

**Keywords** (53/100): ひらがな,カタカナ,書き順,手書き,練習,JLPT,単語帳,復習,翻訳,文字認識,スキャン,辞書,書き取り

**Promotional text** (80/170): Kana Learn!が手書きの日本語を読み取ります。カメラを向ければ文字を読んで英語に翻訳、教科書をスキャンすればそのまま書き込めます。アカウントは不要です。

**Description** (1631/4000):

```text
手で書けば、Kana Learn!が読む。カメラを向ければ、英語に訳す。教科書をスキャンすれば、そのまま書き込める。

Kana Learn!は、書いた字を理解するiPadの日本語ノートです。

【できること】
• 手書きを読み取る：漢字もかなも、Apple Pencilでも指でも。
• Googleで翻訳：書いたもの、撮ったもの、PDFのどの行も英語に。英語から日本語へも。
• カメラで文字認識：看板やメニュー、本にかざすだけ。撮った写真からも。
• スキャンしたページ、PDF、写真が、マーカーを引けて、検索できて、書き込める教材に。
• どの漢字も書き順どおりに書いて見せ、言葉と例文も一緒に。
• 文字と言葉を復習に出し、正しく書けるたびに間隔を延ばします。

【書けば、Kana Learn!が読む】
• メモと練習のための無限のページ。書いたものを投げ縄で囲めば、チェック、テキストでコピー、単語の保存、翻訳ができます。
• 日本語の文章を貼り付けると、Kana Learn!が手書きでページに書いてくれます。
• 和紙から夜まで、5つのテーマ。

【翻訳は、どちら向きにも】
• カメラから：日本語を見つけて1行ずつ英語に訳し、残したい行をページに書きます。
• 日本語へ：英語で単語や文を入力すれば日本語に。含まれる単語を保存したり、ページに書いたり、その字を練習したりできます。
• Google ML KitでiPadの中で動きます。言語ごとに一度ダウンロードすれば、あとはオフラインで翻訳できます。
• アプリを英語・スペイン語・ポルトガル語・フランス語で使うと、その言語と日本語のあいだで翻訳します。

【どの漢字も、一画ずつ】
• 6,400字を超える漢字と、ひらがな・カタカナのすべて。どの字も書き順どおりに書いて見せます。
• 読み、意味（英語）、部品、画数、学年、JLPTレベル（N5〜N1）。
• 54,000を超える言葉と14,500を超える例文を、iPadの音声で読み上げます。
• 入力できない漢字は、手で書くか部品を選べば見つかります。
• 似ている字：まぎらわしい字を、自分が間違えた字から順に。どの字にも自分だけのメモを。

【身につくまで練習】
• なぞり書きから何も見ずに書くまで導く練習。形と書き順に点数が付きます。
• 「第3課」「旅行」など自分のリストで単語帳を。言葉のテストでは、意味と読みから、または聞いて書きます。
• 復習、JLPTレベル別のテスト、練習シートの印刷、統計、そして書いたすべての字のアルバム。

【本物の教材を読んで、書き込む】
• 無料の教材付き：大学の日本語コースの補助教材とワークシート「First Year Japanese I」と、Wikivoyageの日本語フレーズ集（どちらも英語）。
• 自分の教材も：ファイル、写真、書類のスキャンから取り込めます。スキャンした中の日本語もKana Learn!が読み取るので、検索も選択も翻訳もできます。
• マーカーを引き、余白に書き込み、ページへ移動し、スキャンの向きを直して、メモごとPDFで共有。

【あなたのものを、あなたの手元に】
アカウントも広告もトラッキングもありません。ページも単語も記録もiPadの中に残り、「設定 › データ」からいつでも書き出せます。手書き認識・文字認識・翻訳には、端末の中で動くGoogle ML Kitを使います。手書きのモデルはKana Learn!の初回起動時に、翻訳のモデルは初めて翻訳するときにダウンロードされ、ML Kitがオンの間は匿名の診断データがGoogleに送られます。オフにするには「設定 › 手書き認識 › Google ML Kitで読み取る」。ダウンロードのあとは、すべてオフラインで使えます。

日本語、英語、スペイン語、ポルトガル語、フランス語に対応。
```

### Google Play

**Title** (20/30): Kana Learn!: 手で書く日本語

**Short description** (40/80): 手で書く日本語ノート。漢字とかなを書き順どおりに練習し、書いた字を読み取ります。

**Full description** (1649/4000):

```text
手で書けば、Kana Learn!が読む。カメラを向ければ、英語に訳す。教科書をスキャンすれば、そのまま書き込める。

Kana Learn!は、書いた字を理解するAndroidタブレットの日本語ノートです。

【できること】
• 手書きを読み取る：漢字もかなも、タッチペンでも指でも。
• Googleで翻訳：書いたもの、撮ったもの、PDFのどの行も英語に。英語から日本語へも。
• カメラで文字認識：看板やメニュー、本にかざすだけ。撮った写真からも。
• スキャンしたページ、PDF、写真が、マーカーを引けて、検索できて、書き込める教材に。
• どの漢字も書き順どおりに書いて見せ、言葉と例文も一緒に。
• 文字と言葉を復習に出し、正しく書けるたびに間隔を延ばします。

【書けば、Kana Learn!が読む】
• メモと練習のための無限のページ。書いたものを投げ縄で囲めば、チェック、テキストでコピー、単語の保存、翻訳ができます。
• 日本語の文章を貼り付けると、Kana Learn!が手書きでページに書いてくれます。
• 和紙から夜まで、5つのテーマ。

【翻訳は、どちら向きにも】
• カメラから：日本語を見つけて1行ずつ英語に訳し、残したい行をページに書きます。
• 日本語へ：英語で単語や文を入力すれば日本語に。含まれる単語を保存したり、ページに書いたり、その字を練習したりできます。
• Google ML Kitでタブレットの中で動きます。言語ごとに一度ダウンロードすれば、あとはオフラインで翻訳できます。
• アプリを英語・スペイン語・ポルトガル語・フランス語で使うと、その言語と日本語のあいだで翻訳します。

【どの漢字も、一画ずつ】
• 6,400字を超える漢字と、ひらがな・カタカナのすべて。どの字も書き順どおりに書いて見せます。
• 読み、意味（英語）、部品、画数、学年、JLPTレベル（N5〜N1）。
• 54,000を超える言葉と14,500を超える例文を、タブレットの音声で読み上げます。
• 入力できない漢字は、手で書くか部品を選べば見つかります。
• 似ている字：まぎらわしい字を、自分が間違えた字から順に。どの字にも自分だけのメモを。

【身につくまで練習】
• なぞり書きから何も見ずに書くまで導く練習。形と書き順に点数が付きます。
• 「第3課」「旅行」など自分のリストで単語帳を。言葉のテストでは、意味と読みから、または聞いて書きます。
• 復習、JLPTレベル別のテスト、練習シートの印刷、統計、そして書いたすべての字のアルバム。

【本物の教材を読んで、書き込む】
• 無料の教材付き：大学の日本語コースの補助教材とワークシート「First Year Japanese I」と、Wikivoyageの日本語フレーズ集（どちらも英語）。
• 自分の教材も：ファイルや写真から、またはGoogleのドキュメントスキャナで取り込めます。スキャンした中の日本語もKana Learn!が読み取るので、検索も選択も翻訳もできます。
• マーカーを引き、余白に書き込み、ページへ移動し、スキャンの向きを直して、メモごとPDFで共有。

【あなたのものを、あなたの手元に】
アカウントも広告もトラッキングもありません。ページも単語も記録もタブレットの中に残り、「設定 › データ」からいつでも書き出せます。手書き認識・文字認識・翻訳には、端末の中で動くGoogle ML Kitを使います。手書きのモデルはKana Learn!の初回起動時に、翻訳のモデルは初めて翻訳するときにダウンロードされ、ML Kitがオンの間は匿名の診断データがGoogleに送られます。オフにするには「設定 › 手書き認識 › Google ML Kitで読み取る」。ダウンロードのあとは、すべてオフラインで使えます。

日本語、英語、スペイン語、ポルトガル語、フランス語に対応。
```

## French (Google Play: fr-FR)

### App Store

**Name** (30/30): Kana Learn! Écrire le japonais

**Subtitle** (30/30): Kanji et kana, trait par trait

**Keywords** (94/100): hiragana,katakana,écriture,jlpt,nihongo,vocabulaire,traduction,ocr,scanner,dictionnaire,pencil

**Promotional text** (160/170): Écrivez le japonais à la main, Kana Learn! le lit. Visez du japonais avec l’appareil photo et traduisez-le. Numérisez votre manuel, écrivez dessus. Sans compte.

**Description** (3836/4000):

```text
Écrivez le japonais à la main, Kana Learn! le lit. Visez du japonais avec l’appareil photo, Kana Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Kana Learn! est un cahier de japonais pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT KANA LEARN!
• Lit votre écriture : kanji et kana, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le japonais.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque kanji s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, KANA LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en japonais et Kana Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier washi à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Avec l’appareil photo : Kana Learn! trouve le japonais, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le japonais : tapez un mot ou une phrase dans votre langue, obtenez-le en japonais, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

CHAQUE KANJI, TRAIT PAR TRAIT
• Plus de 6 400 kanji et tous les hiragana et katakana, chacun s’écrivant dans l’ordre.
• Lectures, sens, éléments, nombre de traits, niveau scolaire et niveau JLPT, du N5 au N1.
• Plus de 54 000 mots et 14 500 phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Impossible de taper un kanji ? Dessinez-le, ou choisissez ses éléments, et Kana Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau JLPT, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : First Year Japanese I (fiches et exercices d’un cours universitaire, en anglais) et le guide de conversation japonais de Wikivoyage, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents. Kana Learn! lit le japonais qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Kana Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et japonais.
```

### Google Play

**Title** (30/30): Kana Learn! Écrire le japonais

**Short description** (79/80): Écrivez le japonais à la main : kanji et kana trait par trait, lus et traduits.

**Full description** (3873/4000):

```text
Écrivez le japonais à la main, Kana Learn! le lit. Visez du japonais avec l’appareil photo, Kana Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Kana Learn! est un cahier de japonais pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Kana Learn!
• Lit votre écriture : kanji et kana, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le japonais.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque kanji s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Kana Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en japonais et Kana Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier washi à la nuit.

Traduisez, dans les deux sens
• Avec l’appareil photo : Kana Learn! trouve le japonais, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le japonais : tapez un mot ou une phrase dans votre langue, obtenez-le en japonais, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

Chaque kanji, trait par trait
• Plus de 6 400 kanji et tous les hiragana et katakana, chacun s’écrivant dans l’ordre.
• Lectures, sens, éléments, nombre de traits, niveau scolaire et niveau JLPT, du N5 au N1.
• Plus de 54 000 mots et 14 500 phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Impossible de taper un kanji ? Dessinez-le, ou choisissez ses éléments, et Kana Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau JLPT, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : First Year Japanese I (fiches et exercices d’un cours universitaire, en anglais) et le guide de conversation japonais de Wikivoyage, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google. Kana Learn! lit le japonais qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Kana Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et japonais.
```
