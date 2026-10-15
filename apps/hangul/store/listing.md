# Hangul Learn!: App Store and Google Play texts

Hangul Learn!’s texts for the App Store and Google Play, in its five languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 51 jamo, all 11,172 syllables, 2,925 hanja, 46,651 words and 3,880 example sentences). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/hangul/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/hangul/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/hangul/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Korean = Play `ko-KR`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Korean:** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Hangul Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Characters (or Hangul, the chart), then any letter, syllable or hanja: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few characters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. Tap the camera on the toolbar (or press and hold the page › Text from photo): the camera reads Korean text live; Hold keeps the lines, Write puts them on the page, Translate with Google translates them. Choosing a photo works too.
4. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
5. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Korean). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting, text recognition and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. The text recognition model is inside the app. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Text from a photo and Translate with Google are unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Text from a photo or Lectures › Scan pages is first used. The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Korean, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | ko | fr |
|---|---|---|---|---|---|
| App Store name (30) | 27 | 30 | 30 | 24 | 30 |
| App Store subtitle (30) | 28 | 23 | 23 | 9 | 26 |
| App Store keywords (100) | 97 | 97 | 92 | 52 | 98 |
| App Store promotional text (170) | 156 | 157 | 151 | 93 | 158 |
| App Store description (4000) | 3333 | 3548 | 3537 | 1873 | 3850 |
| Google Play title (30) | 27 | 30 | 30 | 24 | 30 |
| Google Play short description (80) | 77 | 74 | 72 | 47 | 79 |
| Google Play full description (4000) | 3361 | 3589 | 3571 | 1882 | 3887 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (27/30): Hangul Learn!: Write Korean

**Subtitle** (28/30): Letters, syllables and hanja

**Keywords** (97/100): hangeul,jamo,handwriting,vocabulary,translate,ocr,scanner,dictionary,alphabet,romanization,pencil

**Promotional text** (156/170): Write Korean by hand and Hangul Learn! reads it. Point the camera at Korean and translate it. Scan your textbook and write on it. No account, works offline.

**Description** (3333/4000):

```text
Write Korean by hand, and Hangul Learn! reads it. Point the camera at Korean, and it translates it. Scan your textbook, and write on it.

Hangul Learn! is a Korean notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: Hangul and hanja, written with the Apple Pencil or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Korean.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every letter and syllable writing itself, with words by level and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

WRITE, AND HANGUL LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Korean text, and Hangul Learn! writes it out by hand on your page.
• Five themes, from paper to night.

TRANSLATE, BOTH WAYS
• From the camera: Hangul Learn! finds the Korean, translates it line by line, and writes the lines you keep on your page.
• Into Korean: type a word or a sentence in your language, get it in Korean, then save its words, write it on the page or practice its characters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

EVERY LETTER, STROKE BY STROKE
• The Hangul chart: every consonant and vowel, and all 11,172 syllables, each one writing itself in the right order.
• More than 2,900 hanja, with their meanings, sounds, parts and stroke counts.
• More than 46,000 words from beginner to advanced, and 3,800 example sentences, read aloud by your iPad’s voices.
• Can’t type a hanja? Draw it, or pick its parts, and Hangul Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: Wikivoyage’s Korean phrasebook, with romanization.
• Bring your own from Files, Photos or the document camera. Hangul Learn! reads the Korean in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hangul Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Korean and French.
```

### Google Play

**Title** (27/30): Hangul Learn!: Write Korean

**Short description** (77/80): Write Korean by hand: Hangul and hanja stroke by stroke, read and translated.

**Full description** (3361/4000):

```text
Write Korean by hand, and Hangul Learn! reads it. Point the camera at Korean, and it translates it. Scan your textbook, and write on it.

Hangul Learn! is a Korean notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: Hangul and hanja, written with a stylus or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Korean.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every letter and syllable writing itself, with words by level and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

Write, and Hangul Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Korean text, and Hangul Learn! writes it out by hand on your page.
• Five themes, from paper to night.

Translate, both ways
• From the camera: Hangul Learn! finds the Korean, translates it line by line, and writes the lines you keep on your page.
• Into Korean: type a word or a sentence in your language, get it in Korean, then save its words, write it on the page or practice its characters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

Every letter, stroke by stroke
• The Hangul chart: every consonant and vowel, and all 11,172 syllables, each one writing itself in the right order.
• More than 2,900 hanja, with their meanings, sounds, parts and stroke counts.
• More than 46,000 words from beginner to advanced, and 3,800 example sentences, read aloud by your tablet’s voices.
• Can’t type a hanja? Draw it, or pick its parts, and Hangul Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: Wikivoyage’s Korean phrasebook, with romanization.
• Bring your own from your files and photos, or with Google’s document scanner. Hangul Learn! reads the Korean in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hangul Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Korean and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (30/30): Hangul Learn!: escribe coreano

**Subtitle** (23/30): Letras, sílabas y hanja

**Keywords** (97/100): hangeul,jamo,escritura,vocabulario,traductor,ocr,escáner,diccionario,alfabeto,romanización,pencil

**Promotional text** (157/170): Escribe coreano a mano y Hangul Learn! lo lee. Apunta la cámara al coreano y tradúcelo. Escanea tu libro y escribe encima. Sin cuenta, funciona sin conexión.

**Description** (3548/4000):

```text
Escribe coreano a mano y Hangul Learn! lo lee. Apunta la cámara a un texto en coreano y te lo traduce. Escanea tu libro de texto y escribe encima.

Hangul Learn! es un cuaderno de coreano para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: hangul y hanja, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al coreano.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada letra y cada sílaba, con palabras por nivel y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y HANGUL LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en coreano y Hangul Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Con la cámara: Hangul Learn! encuentra el coreano, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al coreano: escribe una palabra o una frase en tu idioma, obtenla en coreano y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

CADA LETRA, TRAZO A TRAZO
• La tabla del hangul: todas las consonantes y vocales, y las 11.172 sílabas, cada una escribiéndose en su orden.
• Más de 2.900 hanja, con su significado, su sonido, sus partes y su número de trazos.
• Más de 46.000 palabras, de nivel inicial a avanzado, y 3.800 frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No sabes teclear un hanja? Dibújalo, o elige sus partes, y Hangul Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: la guía de coreano de Wikiviajes, con romanización, en español.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos. Hangul Learn! lee el coreano que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hangul Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, coreano y francés.
```

### Google Play

**Title** (30/30): Hangul Learn!: escribe coreano

**Short description** (74/80): Escribe coreano a mano: hangul y hanja trazo a trazo, leídos y traducidos.

**Full description** (3589/4000):

```text
Escribe coreano a mano y Hangul Learn! lo lee. Apunta la cámara a un texto en coreano y te lo traduce. Escanea tu libro de texto y escribe encima.

Hangul Learn! es un cuaderno de coreano para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: hangul y hanja, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al coreano.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada letra y cada sílaba, con palabras por nivel y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Hangul Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en coreano y Hangul Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

Traduce en los dos sentidos
• Con la cámara: Hangul Learn! encuentra el coreano, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al coreano: escribe una palabra o una frase en tu idioma, obtenla en coreano y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

Cada letra, trazo a trazo
• La tabla del hangul: todas las consonantes y vocales, y las 11.172 sílabas, cada una escribiéndose en su orden.
• Más de 2.900 hanja, con su significado, su sonido, sus partes y su número de trazos.
• Más de 46.000 palabras, de nivel inicial a avanzado, y 3.800 frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No sabes teclear un hanja? Dibújalo, o elige sus partes, y Hangul Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: la guía de coreano de Wikiviajes, con romanización, en español.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google. Hangul Learn! lee el coreano que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hangul Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, coreano y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (30/30): Hangul Learn!: escreva coreano

**Subtitle** (23/30): Letras, sílabas e hanja

**Keywords** (92/100): hangeul,jamo,escrita,vocabulário,tradutor,ocr,scanner,dicionário,alfabeto,romanização,pencil

**Promotional text** (151/170): Escreva coreano à mão e o Hangul Learn! lê. Aponte a câmera para o coreano e traduza. Digitalize seu livro e escreva nele. Sem conta, funciona offline.

**Description** (3537/4000):

```text
Escreva coreano à mão e o Hangul Learn! lê. Aponte a câmera para um texto em coreano e ele traduz. Digitalize seu livro e escreva nele.

O Hangul Learn! é um caderno de coreano para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: hangul e hanja, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o coreano.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada letra e cada sílaba sendo escritas, com palavras por nível e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O HANGUL LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em coreano e o Hangul Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

TRADUZA NOS DOIS SENTIDOS
• Pela câmera: o Hangul Learn! encontra o coreano, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o coreano: digite uma palavra ou uma frase no seu idioma, veja em coreano e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

CADA LETRA, TRAÇO A TRAÇO
• A tabela do hangul: todas as consoantes e vogais, e as 11.172 sílabas, cada uma se escrevendo na ordem certa.
• Mais de 2.900 hanja, com significado, som, partes e número de traços.
• Mais de 46.000 palavras, do básico ao avançado, e 3.800 frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não sabe digitar um hanja? Desenhe, ou escolha as partes dele, e o Hangul Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: o guia de conversação coreano do Wikivoyage, com romanização, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos. O Hangul Learn! lê o coreano nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hangul Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, coreano e francês.
```

### Google Play

**Title** (30/30): Hangul Learn!: escreva coreano

**Short description** (72/80): Escreva coreano à mão: hangul e hanja traço a traço, lidos e traduzidos.

**Full description** (3571/4000):

```text
Escreva coreano à mão e o Hangul Learn! lê. Aponte a câmera para um texto em coreano e ele traduz. Digitalize seu livro e escreva nele.

O Hangul Learn! é um caderno de coreano para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: hangul e hanja, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o coreano.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada letra e cada sílaba sendo escritas, com palavras por nível e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Hangul Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em coreano e o Hangul Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

Traduza nos dois sentidos
• Pela câmera: o Hangul Learn! encontra o coreano, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o coreano: digite uma palavra ou uma frase no seu idioma, veja em coreano e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

Cada letra, traço a traço
• A tabela do hangul: todas as consoantes e vogais, e as 11.172 sílabas, cada uma se escrevendo na ordem certa.
• Mais de 2.900 hanja, com significado, som, partes e número de traços.
• Mais de 46.000 palavras, do básico ao avançado, e 3.800 frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não sabe digitar um hanja? Desenhe, ou escolha as partes dele, e o Hangul Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: o guia de conversação coreano do Wikivoyage, com romanização, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google. O Hangul Learn! lê o coreano nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hangul Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, coreano e francês.
```

## Korean (Google Play: ko-KR)

### App Store

**Name** (24/30): Hangul Learn!: 한글과 한자 쓰기

**Subtitle** (9/30): 획순대로 한 획씩

**Keywords** (52/100): 손글씨,필기,글씨연습,자모,음절,단어장,복습,번역,문자인식,스캔,사전,한국어,받아쓰기,따라쓰기

**Promotional text** (93/170): 손으로 쓰면 Hangul Learn!이 읽습니다. 카메라를 비추면 글자를 읽어 영어로 번역하고, 교재를 스캔하면 그 위에 바로 쓸 수 있습니다. 계정은 필요 없습니다.

**Description** (1873/4000):

```text
손으로 쓰면 Hangul Learn!이 읽습니다. 카메라를 비추면 영어로 번역합니다. 교재를 스캔하면 그 위에 바로 쓸 수 있습니다.

Hangul Learn!은 쓴 글자를 알아보는 iPad용 한국어 노트입니다.

■ 주요 기능
• 손글씨 인식: Apple Pencil이나 손가락으로 쓴 한글과 한자를 읽습니다.
• Google 번역: 쓴 글, 찍은 사진, PDF의 어느 줄이든 영어로, 영어는 한국어로 번역합니다.
• 카메라 문자 인식: 간판, 메뉴, 책에 비추기만 하면 실시간으로 읽고, 사진에서도 읽습니다.
• 스캔한 페이지, PDF, 사진을 표시하고, 검색하고, 그 위에 쓸 수 있는 교재로 만듭니다.
• 모든 자모와 음절, 한자를 획순대로 써 보이고, 단어와 예문을 함께 보여 줍니다.
• 글자와 단어를 복습에 내며, 맞게 쓸 때마다 간격을 늘립니다.

■ 쓰면 Hangul Learn!이 읽습니다
• 노트와 연습을 위한 끝없는 페이지. 쓴 글자를 올가미로 감싸면 검사, 텍스트로 복사, 단어 저장, 번역을 할 수 있습니다.
• 한국어 문장을 붙여 넣으면 Hangul Learn!이 페이지에 손글씨로 써 줍니다.
• 종이부터 밤까지 다섯 가지 테마.

■ 양방향 번역
• 카메라로: 한국어를 찾아 한 줄씩 영어로 번역하고, 남길 줄을 페이지에 씁니다.
• 한국어로: 영어 단어나 문장을 입력하면 한국어로 바꿔 주고, 그 안의 단어를 저장하거나 페이지에 쓰거나 글자를 연습할 수 있습니다.
• Google ML Kit으로 iPad 안에서 작동합니다. 언어마다 한 번만 내려받으면 그다음부터 오프라인으로 번역합니다.
• 앱을 영어, 스페인어, 포르투갈어, 프랑스어로 쓰면 그 언어와 한국어 사이에서 번역합니다.

■ 모든 글자를 한 획씩
• 한글 표: 모든 자음과 모음, 그리고 11,172개 음절 전부를 바른 획순으로 써 보입니다.
• 2,900자가 넘는 한자와 그 훈·음, 부분, 획수.
• 초급부터 고급까지 46,000개가 넘는 단어와 3,800개가 넘는 예문을 iPad의 음성으로 읽어 줍니다.
• 입력하기 어려운 한자는 손으로 쓰거나 부분을 고르면 찾아 줍니다.
• 비슷한 글자: 헷갈리기 쉬운 글자를, 내가 헷갈린 것부터 보여 줍니다. 어느 글자에나 나만의 메모를 남길 수 있습니다.

■ 몸에 밸 때까지 연습
• 따라 쓰기부터 보지 않고 쓰기까지 이끌어 주는 연습. 모양과 획순에 점수를 매깁니다.
• ‘3과’, ‘여행’처럼 나만의 목록으로 단어장을 만들고, 단어 시험에서는 뜻과 발음을 보고, 또는 듣고 씁니다.
• 복습, 수준별 시험, 인쇄용 연습지, 통계, 그리고 내가 쓴 모든 글자를 모은 앨범.

■ 진짜 교재를 읽고, 그 위에 쓰기
• 무료 교재 포함: Wikivoyage의 한국어 회화집(영어판, 로마자 표기 포함).
• 내 교재도 파일, 사진, 문서 스캔으로 가져올 수 있습니다. 스캔한 페이지 속 한국어도 Hangul Learn!이 읽어서 검색, 선택, 번역할 수 있습니다.
• 마커로 표시하고, 여백에 쓰고, 원하는 페이지로 이동하고, 스캔을 바로 세우고, 메모와 함께 PDF로 공유하세요.

■ 내 것은 내 기기에
계정도, 광고도, 추적도 없습니다. 페이지와 단어, 학습 기록은 iPad에만 남고, ‘설정 › 내 데이터’에서 언제든 내보낼 수 있습니다. 손글씨 인식, 문자 인식, 번역에는 기기에서 작동하는 Google ML Kit을 사용합니다. 손글씨 모델은 Hangul Learn!을 처음 실행할 때, 번역 모델은 처음 번역할 때 내려받으며, ML Kit이 켜져 있는 동안 익명의 진단 데이터가 Google에 전송됩니다. 끄려면 ‘설정 › 필기 인식 › Google ML Kit으로 인식’에서 끄세요. 내려받은 뒤에는 모두 오프라인으로 작동합니다.

한국어, 영어, 스페인어, 포르투갈어, 프랑스어 지원.
```

### Google Play

**Title** (24/30): Hangul Learn!: 한글과 한자 쓰기

**Short description** (47/80): 손으로 쓰는 한국어 노트. 한글과 한자를 획순대로 익히고, 쓴 글자를 바로 읽습니다.

**Full description** (1882/4000):

```text
손으로 쓰면 Hangul Learn!이 읽습니다. 카메라를 비추면 영어로 번역합니다. 교재를 스캔하면 그 위에 바로 쓸 수 있습니다.

Hangul Learn!은 쓴 글자를 알아보는 Android 태블릿용 한국어 노트입니다.

■ 주요 기능
• 손글씨 인식: 스타일러스나 손가락으로 쓴 한글과 한자를 읽습니다.
• Google 번역: 쓴 글, 찍은 사진, PDF의 어느 줄이든 영어로, 영어는 한국어로 번역합니다.
• 카메라 문자 인식: 간판, 메뉴, 책에 비추기만 하면 실시간으로 읽고, 사진에서도 읽습니다.
• 스캔한 페이지, PDF, 사진을 표시하고, 검색하고, 그 위에 쓸 수 있는 교재로 만듭니다.
• 모든 자모와 음절, 한자를 획순대로 써 보이고, 단어와 예문을 함께 보여 줍니다.
• 글자와 단어를 복습에 내며, 맞게 쓸 때마다 간격을 늘립니다.

■ 쓰면 Hangul Learn!이 읽습니다
• 노트와 연습을 위한 끝없는 페이지. 쓴 글자를 올가미로 감싸면 검사, 텍스트로 복사, 단어 저장, 번역을 할 수 있습니다.
• 한국어 문장을 붙여 넣으면 Hangul Learn!이 페이지에 손글씨로 써 줍니다.
• 종이부터 밤까지 다섯 가지 테마.

■ 양방향 번역
• 카메라로: 한국어를 찾아 한 줄씩 영어로 번역하고, 남길 줄을 페이지에 씁니다.
• 한국어로: 영어 단어나 문장을 입력하면 한국어로 바꿔 주고, 그 안의 단어를 저장하거나 페이지에 쓰거나 글자를 연습할 수 있습니다.
• Google ML Kit으로 태블릿 안에서 작동합니다. 언어마다 한 번만 내려받으면 그다음부터 오프라인으로 번역합니다.
• 앱을 영어, 스페인어, 포르투갈어, 프랑스어로 쓰면 그 언어와 한국어 사이에서 번역합니다.

■ 모든 글자를 한 획씩
• 한글 표: 모든 자음과 모음, 그리고 11,172개 음절 전부를 바른 획순으로 써 보입니다.
• 2,900자가 넘는 한자와 그 훈·음, 부분, 획수.
• 초급부터 고급까지 46,000개가 넘는 단어와 3,800개가 넘는 예문을 태블릿의 음성으로 읽어 줍니다.
• 입력하기 어려운 한자는 손으로 쓰거나 부분을 고르면 찾아 줍니다.
• 비슷한 글자: 헷갈리기 쉬운 글자를, 내가 헷갈린 것부터 보여 줍니다. 어느 글자에나 나만의 메모를 남길 수 있습니다.

■ 몸에 밸 때까지 연습
• 따라 쓰기부터 보지 않고 쓰기까지 이끌어 주는 연습. 모양과 획순에 점수를 매깁니다.
• ‘3과’, ‘여행’처럼 나만의 목록으로 단어장을 만들고, 단어 시험에서는 뜻과 발음을 보고, 또는 듣고 씁니다.
• 복습, 수준별 시험, 인쇄용 연습지, 통계, 그리고 내가 쓴 모든 글자를 모은 앨범.

■ 진짜 교재를 읽고, 그 위에 쓰기
• 무료 교재 포함: Wikivoyage의 한국어 회화집(영어판, 로마자 표기 포함).
• 내 교재도 파일이나 사진에서, 또는 Google 문서 스캐너로 가져올 수 있습니다. 스캔한 페이지 속 한국어도 Hangul Learn!이 읽어서 검색, 선택, 번역할 수 있습니다.
• 마커로 표시하고, 여백에 쓰고, 원하는 페이지로 이동하고, 스캔을 바로 세우고, 메모와 함께 PDF로 공유하세요.

■ 내 것은 내 기기에
계정도, 광고도, 추적도 없습니다. 페이지와 단어, 학습 기록은 태블릿에만 남고, ‘설정 › 내 데이터’에서 언제든 내보낼 수 있습니다. 손글씨 인식, 문자 인식, 번역에는 기기에서 작동하는 Google ML Kit을 사용합니다. 손글씨 모델은 Hangul Learn!을 처음 실행할 때, 번역 모델은 처음 번역할 때 내려받으며, ML Kit이 켜져 있는 동안 익명의 진단 데이터가 Google에 전송됩니다. 끄려면 ‘설정 › 필기 인식 › Google ML Kit으로 인식’에서 끄세요. 내려받은 뒤에는 모두 오프라인으로 작동합니다.

한국어, 영어, 스페인어, 포르투갈어, 프랑스어 지원.
```

## French (Google Play: fr-FR)

### App Store

**Name** (30/30): Hangul Learn! Écrire le coréen

**Subtitle** (26/30): Lettres, syllabes et hanja

**Keywords** (98/100): hangeul,jamo,écriture,vocabulaire,traduction,ocr,scanner,dictionnaire,alphabet,romanisation,pencil

**Promotional text** (158/170): Écrivez le coréen à la main, Hangul Learn! le lit. Visez du coréen avec l’appareil photo et traduisez-le. Numérisez votre manuel, écrivez dessus. Sans compte.

**Description** (3850/4000):

```text
Écrivez le coréen à la main, Hangul Learn! le lit. Visez du coréen avec l’appareil photo, Hangul Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hangul Learn! est un cahier de coréen pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT HANGUL LEARN!
• Lit votre écriture : hangul et hanja, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le coréen.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque lettre et chaque syllabe s’écrire, avec des mots par niveau et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, HANGUL LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en coréen et Hangul Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Avec l’appareil photo : Hangul Learn! trouve le coréen, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le coréen : tapez un mot ou une phrase dans votre langue, obtenez-le en coréen, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

CHAQUE LETTRE, TRAIT PAR TRAIT
• Le tableau du hangul : toutes les consonnes et voyelles, et les 11 172 syllabes, chacune s’écrivant dans l’ordre.
• Plus de 2 900 hanja, avec leur sens, leur son, leurs éléments et leur nombre de traits.
• Plus de 46 000 mots, du niveau débutant à avancé, et 3 800 phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Impossible de taper un hanja ? Dessinez-le, ou choisissez ses éléments, et Hangul Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : le guide de conversation coréen de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents. Hangul Learn! lit le coréen qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hangul Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et coréen.
```

### Google Play

**Title** (30/30): Hangul Learn! Écrire le coréen

**Short description** (79/80): Écrivez le coréen à la main : hangul et hanja trait par trait, lus et traduits.

**Full description** (3887/4000):

```text
Écrivez le coréen à la main, Hangul Learn! le lit. Visez du coréen avec l’appareil photo, Hangul Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hangul Learn! est un cahier de coréen pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Hangul Learn!
• Lit votre écriture : hangul et hanja, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le coréen.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque lettre et chaque syllabe s’écrire, avec des mots par niveau et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Hangul Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en coréen et Hangul Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

Traduisez, dans les deux sens
• Avec l’appareil photo : Hangul Learn! trouve le coréen, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le coréen : tapez un mot ou une phrase dans votre langue, obtenez-le en coréen, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

Chaque lettre, trait par trait
• Le tableau du hangul : toutes les consonnes et voyelles, et les 11 172 syllabes, chacune s’écrivant dans l’ordre.
• Plus de 2 900 hanja, avec leur sens, leur son, leurs éléments et leur nombre de traits.
• Plus de 46 000 mots, du niveau débutant à avancé, et 3 800 phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Impossible de taper un hanja ? Dessinez-le, ou choisissez ses éléments, et Hangul Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : le guide de conversation coréen de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google. Hangul Learn! lit le coréen qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hangul Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et coréen.
```
