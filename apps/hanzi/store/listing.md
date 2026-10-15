# Hanzi Learn!: App Store and Google Play texts

Hanzi Learn!’s texts for the App Store and Google Play, in its five languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 9,565 characters (simplified and traditional), 85,601 words and 11,845 example sentences). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/hanzi/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/hanzi/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/hanzi/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Chinese (Simplified) = Play `zh-CN`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Chinese (Simplified):** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.
- **China mainland:** Chinese (Simplified) is also read in Singapore, Malaysia and elsewhere. Distributing in mainland China needs an ICP filing number in App Store Connect, and Google’s model downloads (ML Kit) cannot be reached there. Decide whether China mainland is in the app’s availability.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Hanzi Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Hanzi, then any character: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few characters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. Tap the camera on the toolbar (or press and hold the page › Text from photo): the camera reads Chinese text live; Hold keeps the lines, Write puts them on the page, Translate with Google translates them. Choosing a photo works too.
4. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
5. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Chinese). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting, text recognition and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. The text recognition model is inside the app. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Text from a photo and Translate with Google are unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Text from a photo or Lectures › Scan pages is first used. The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Chinese, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | zh | fr |
|---|---|---|---|---|---|
| App Store name (30) | 27 | 27 | 28 | 18 | 30 |
| App Store subtitle (30) | 28 | 25 | 25 | 11 | 27 |
| App Store keywords (100) | 98 | 100 | 96 | 51 | 99 |
| App Store promotional text (170) | 157 | 152 | 148 | 60 | 159 |
| App Store description (4000) | 3289 | 3466 | 3499 | 1390 | 3773 |
| Google Play title (30) | 27 | 27 | 28 | 18 | 30 |
| Google Play short description (80) | 72 | 68 | 67 | 27 | 75 |
| Google Play full description (4000) | 3317 | 3507 | 3533 | 1382 | 3810 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (27/30): Hanzi Learn!: Write Chinese

**Subtitle** (28/30): Characters, stroke by stroke

**Keywords** (98/100): mandarin,pinyin,hsk,handwriting,simplified,traditional,vocabulary,translate,ocr,scanner,dictionary

**Promotional text** (157/170): Write Chinese by hand and Hanzi Learn! reads it. Point the camera at Chinese and translate it. Scan your textbook and write on it. No account, works offline.

**Description** (3289/4000):

```text
Write Chinese by hand, and Hanzi Learn! reads it. Point the camera at Chinese, and it translates it. Scan your textbook, and write on it.

Hanzi Learn! is a Chinese notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: Chinese characters, written with the Apple Pencil or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Chinese.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every character writing itself, simplified and traditional, with words and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

WRITE, AND HANZI LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Chinese text, and Hanzi Learn! writes it out by hand on your page.
• Five themes, from paper to night.

TRANSLATE, BOTH WAYS
• From the camera: Hanzi Learn! finds the Chinese, translates it line by line, and writes the lines you keep on your page.
• Into Chinese: type a word or a sentence in your language, get it in Chinese, then save its words, write it on the page or practice its characters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

EVERY CHARACTER, STROKE BY STROKE
• More than 9,500 characters, simplified and traditional, each one writing itself in the right order.
• Pinyin, meanings, parts, stroke counts and HSK levels.
• More than 85,000 words and 11,000 example sentences, read aloud by your iPad’s voices.
• Can’t type a character? Draw it, or pick its parts, and Hanzi Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by HSK level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: Wikivoyage’s Chinese phrasebook, with pinyin.
• Bring your own from Files, Photos or the document camera. Hanzi Learn! reads the Chinese in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hanzi Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Chinese and French.
```

### Google Play

**Title** (27/30): Hanzi Learn!: Write Chinese

**Short description** (72/80): Write Chinese by hand: characters stroke by stroke, read and translated.

**Full description** (3317/4000):

```text
Write Chinese by hand, and Hanzi Learn! reads it. Point the camera at Chinese, and it translates it. Scan your textbook, and write on it.

Hanzi Learn! is a Chinese notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: Chinese characters, written with a stylus or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Chinese.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every character writing itself, simplified and traditional, with words and example sentences.
• Brings characters and words back for review, a little later each time you get them right.

Write, and Hanzi Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Chinese text, and Hanzi Learn! writes it out by hand on your page.
• Five themes, from paper to night.

Translate, both ways
• From the camera: Hanzi Learn! finds the Chinese, translates it line by line, and writes the lines you keep on your page.
• Into Chinese: type a word or a sentence in your language, get it in Chinese, then save its words, write it on the page or practice its characters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

Every character, stroke by stroke
• More than 9,500 characters, simplified and traditional, each one writing itself in the right order.
• Pinyin, meanings, parts, stroke counts and HSK levels.
• More than 85,000 words and 11,000 example sentences, read aloud by your tablet’s voices.
• Can’t type a character? Draw it, or pick its parts, and Hanzi Learn! finds it.
• Look-alikes show the characters that get mixed up, yours first. Keep your own note on any character.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by HSK level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: Wikivoyage’s Chinese phrasebook, with pinyin.
• Bring your own from your files and photos, or with Google’s document scanner. Hanzi Learn! reads the Chinese in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hanzi Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Chinese and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (27/30): Hanzi Learn!: escribe chino

**Subtitle** (25/30): Caracteres, trazo a trazo

**Keywords** (100/100): mandarín,pinyin,hsk,escritura,simplificado,tradicional,vocabulario,traductor,ocr,escáner,diccionario

**Promotional text** (152/170): Escribe chino a mano y Hanzi Learn! lo lee. Apunta la cámara al chino y tradúcelo. Escanea tu libro y escribe encima. Sin cuenta, funciona sin conexión.

**Description** (3466/4000):

```text
Escribe chino a mano y Hanzi Learn! lo lee. Apunta la cámara a un texto en chino y te lo traduce. Escanea tu libro de texto y escribe encima.

Hanzi Learn! es un cuaderno de chino para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: caracteres chinos, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al chino.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada carácter, simplificado y tradicional, con palabras y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y HANZI LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en chino y Hanzi Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Con la cámara: Hanzi Learn! encuentra el chino, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al chino: escribe una palabra o una frase en tu idioma, obtenla en chino y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

CADA CARÁCTER, TRAZO A TRAZO
• Más de 9.500 caracteres, simplificados y tradicionales, cada uno escribiéndose en su orden.
• Pinyin, significados, partes, número de trazos y nivel del HSK.
• Más de 85.000 palabras y 11.000 frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No sabes teclear un carácter? Dibújalo, o elige sus partes, y Hanzi Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel del HSK, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: la guía de chino de Wikiviajes, con pinyin, en español.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos. Hanzi Learn! lee el chino que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hanzi Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, chino y francés.
```

### Google Play

**Title** (27/30): Hanzi Learn!: escribe chino

**Short description** (68/80): Escribe chino a mano: caracteres trazo a trazo, leídos y traducidos.

**Full description** (3507/4000):

```text
Escribe chino a mano y Hanzi Learn! lo lee. Apunta la cámara a un texto en chino y te lo traduce. Escanea tu libro de texto y escribe encima.

Hanzi Learn! es un cuaderno de chino para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: caracteres chinos, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al chino.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada carácter, simplificado y tradicional, con palabras y frases de ejemplo.
• Programa repasos de los caracteres y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Hanzi Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en chino y Hanzi Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

Traduce en los dos sentidos
• Con la cámara: Hanzi Learn! encuentra el chino, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al chino: escribe una palabra o una frase en tu idioma, obtenla en chino y guarda sus palabras, escríbela en la página o practica sus caracteres.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

Cada carácter, trazo a trazo
• Más de 9.500 caracteres, simplificados y tradicionales, cada uno escribiéndose en su orden.
• Pinyin, significados, partes, número de trazos y nivel del HSK.
• Más de 85.000 palabras y 11.000 frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No sabes teclear un carácter? Dibújalo, o elige sus partes, y Hanzi Learn! lo encuentra.
• Parecidos: los caracteres que se confunden, empezando por los tuyos. Y tu propia nota en cualquier carácter.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel del HSK, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: la guía de chino de Wikiviajes, con pinyin, en español.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google. Hanzi Learn! lee el chino que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hanzi Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, chino y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (28/30): Hanzi Learn!: escreva chinês

**Subtitle** (25/30): Caracteres, traço a traço

**Keywords** (96/100): mandarim,pinyin,hsk,escrita,simplificado,tradicional,vocabulário,tradutor,ocr,scanner,dicionário

**Promotional text** (148/170): Escreva chinês à mão e o Hanzi Learn! lê. Aponte a câmera para o chinês e traduza. Digitalize seu livro e escreva nele. Sem conta, funciona offline.

**Description** (3499/4000):

```text
Escreva chinês à mão e o Hanzi Learn! lê. Aponte a câmera para um texto em chinês e ele traduz. Digitalize seu livro e escreva nele.

O Hanzi Learn! é um caderno de chinês para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: caracteres chineses, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o chinês.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada caractere sendo escrito, simplificado e tradicional, com palavras e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O HANZI LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em chinês e o Hanzi Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

TRADUZA NOS DOIS SENTIDOS
• Pela câmera: o Hanzi Learn! encontra o chinês, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o chinês: digite uma palavra ou uma frase no seu idioma, veja em chinês e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

CADA CARACTERE, TRAÇO A TRAÇO
• Mais de 9.500 caracteres, simplificados e tradicionais, cada um se escrevendo na ordem certa.
• Pinyin, significados, partes, número de traços e nível do HSK.
• Mais de 85.000 palavras e 11.000 frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não sabe digitar um caractere? Desenhe, ou escolha as partes dele, e o Hanzi Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível do HSK, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: o guia de conversação de mandarim do Wikivoyage, com pinyin, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos. O Hanzi Learn! lê o chinês nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hanzi Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, chinês e francês.
```

### Google Play

**Title** (28/30): Hanzi Learn!: escreva chinês

**Short description** (67/80): Escreva chinês à mão: caracteres traço a traço, lidos e traduzidos.

**Full description** (3533/4000):

```text
Escreva chinês à mão e o Hanzi Learn! lê. Aponte a câmera para um texto em chinês e ele traduz. Digitalize seu livro e escreva nele.

O Hanzi Learn! é um caderno de chinês para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: caracteres chineses, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o chinês.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada caractere sendo escrito, simplificado e tradicional, com palavras e frases de exemplo.
• Agenda revisões dos caracteres e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Hanzi Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em chinês e o Hanzi Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

Traduza nos dois sentidos
• Pela câmera: o Hanzi Learn! encontra o chinês, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o chinês: digite uma palavra ou uma frase no seu idioma, veja em chinês e depois salve as palavras, escreva na página ou pratique seus caracteres.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

Cada caractere, traço a traço
• Mais de 9.500 caracteres, simplificados e tradicionais, cada um se escrevendo na ordem certa.
• Pinyin, significados, partes, número de traços e nível do HSK.
• Mais de 85.000 palavras e 11.000 frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não sabe digitar um caractere? Desenhe, ou escolha as partes dele, e o Hanzi Learn! encontra.
• Parecidos: os caracteres que se confundem, começando pelos seus. E sua própria nota em qualquer caractere.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível do HSK, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: o guia de conversação de mandarim do Wikivoyage, com pinyin, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google. O Hanzi Learn! lê o chinês nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hanzi Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, chinês e francês.
```

## Chinese (Simplified) (Google Play: zh-CN)

### App Store

**Name** (18/30): Hanzi Learn!：亲手写中文

**Subtitle** (11/30): 汉字一笔一画，简繁都有

**Keywords** (51/100): 写字,笔顺,练字,拼音,HSK,生词本,复习,翻译,文字识别,扫描,字典,繁体字,简体字,普通话,字帖

**Promotional text** (60/170): 亲手写中文，Hanzi Learn! 就能读懂。用相机对准中文，即可识别并译成英文；扫描课本，直接在上面书写。无需账户。

**Description** (1390/4000):

```text
亲手写中文，Hanzi Learn! 就能读懂。用相机对准中文，就能译成英文。扫描课本，直接在上面写。

Hanzi Learn! 是一本 iPad 上的中文笔记本，能读懂你写的字。

【主要功能】
• 识别手写：用 Apple Pencil 或手指写下的汉字。
• 用 Google 翻译：你写的字、拍的照片、PDF 中的任意一行都能译成英文，英文也能译成中文。
• 用相机识别文字：对准招牌、菜单或书本即可实时识别，也可以识别照片。
• 把扫描件、PDF 和照片变成可以划重点、搜索和书写的教材。
• 每个汉字都按笔顺写给你看，简体繁体都有，并附词语和例句。
• 安排汉字和词语的复习，每写对一次，间隔就更长一些。

【写下来，Hanzi Learn! 就能读懂】
• 一张无限大的纸，用来记笔记和练字。用套索圈出写好的字，就能检查、复制为文本、保存词语或翻译。
• 粘贴一段中文，Hanzi Learn! 会把它一笔一画地手写到页面上。
• 五种主题，从纸张到夜间。

【双向翻译】
• 用相机：Hanzi Learn! 找出其中的中文，逐行译成英文，并把你保留的行写到页面上。
• 译成中文：输入英文单词或句子，得到中文，再保存其中的词语、写到页面上或练习其中的字。
• 借助 Google ML Kit 在 iPad 上运行：每种语言只需下载一次，之后即可离线翻译。
• 以英语、西班牙语、葡萄牙语或法语使用本应用时，则在该语言和中文之间翻译。

【每个汉字，一笔一画】
• 9,500 多个汉字，简体繁体都有，每个字都按正确的笔顺写给你看。
• 拼音、英文释义、部件、笔画数和 HSK 等级。
• 85,000 多个词语和 11,000 多个例句，用 iPad 的语音朗读。
• 打不出来的字？手写出来，或选择它的部件，Hanzi Learn! 就能找到。
• 形近字：列出容易混淆的字，先列出你自己写混过的。还可以给任何一个字写下自己的笔记。

【练到记住为止】
• 从描红到默写的引导练习，按字形和笔顺打分。
• 用“第三课”“旅行”这样的列表整理自己的生词本，还有词语测验：根据释义和拼音写出词语，或听写。
• 复习、按 HSK 等级的测验、可打印的字帖、统计，以及收录你写过的每个字的作品集。

【阅读真实的教材，并在上面书写】
• 附带免费教材：Wikivoyage 的中文会话手册（英文版，附拼音）。
• 也可以导入自己的教材：从“文件”“照片”或文档扫描导入。即使是扫描件，Hanzi Learn! 也能识别其中的中文，方便你搜索、选取和翻译。
• 用荧光笔划重点、在页边书写、跳到指定页、把扫描页转正，并把内容连同笔记一起以 PDF 分享。

【你的内容，只属于你】
没有账户，没有广告，没有跟踪。你的页面、词语和学习进度都留在 iPad 上，可以随时在“设置 › 你的数据”中导出。手写识别、文字识别和翻译使用在设备上运行的 Google ML Kit：手写识别模型在首次启动 Hanzi Learn! 时下载，翻译模型在第一次翻译时下载；ML Kit 开启期间，Google 会收到匿名诊断数据。如需关闭：“设置 › 手写识别 › 用 Google ML Kit 识别”。下载完成后，一切都能离线使用。

支持中文、英语、西班牙语、葡萄牙语和法语。
```

### Google Play

**Title** (18/30): Hanzi Learn!：亲手写中文

**Short description** (27/80): 亲手写中文的笔记本：按笔顺练习汉字，写下的字即时识别。

**Full description** (1382/4000):

```text
亲手写中文，Hanzi Learn! 就能读懂。用相机对准中文，就能译成英文。扫描课本，直接在上面写。

Hanzi Learn! 是一本 Android 平板上的中文笔记本，能读懂你写的字。

【主要功能】
• 识别手写：用触控笔或手指写下的汉字。
• 用 Google 翻译：你写的字、拍的照片、PDF 中的任意一行都能译成英文，英文也能译成中文。
• 用相机识别文字：对准招牌、菜单或书本即可实时识别，也可以识别照片。
• 把扫描件、PDF 和照片变成可以划重点、搜索和书写的教材。
• 每个汉字都按笔顺写给你看，简体繁体都有，并附词语和例句。
• 安排汉字和词语的复习，每写对一次，间隔就更长一些。

【写下来，Hanzi Learn! 就能读懂】
• 一张无限大的纸，用来记笔记和练字。用套索圈出写好的字，就能检查、复制为文本、保存词语或翻译。
• 粘贴一段中文，Hanzi Learn! 会把它一笔一画地手写到页面上。
• 五种主题，从纸张到夜间。

【双向翻译】
• 用相机：Hanzi Learn! 找出其中的中文，逐行译成英文，并把你保留的行写到页面上。
• 译成中文：输入英文单词或句子，得到中文，再保存其中的词语、写到页面上或练习其中的字。
• 借助 Google ML Kit 在平板上运行：每种语言只需下载一次，之后即可离线翻译。
• 以英语、西班牙语、葡萄牙语或法语使用本应用时，则在该语言和中文之间翻译。

【每个汉字，一笔一画】
• 9,500 多个汉字，简体繁体都有，每个字都按正确的笔顺写给你看。
• 拼音、英文释义、部件、笔画数和 HSK 等级。
• 85,000 多个词语和 11,000 多个例句，用平板的语音朗读。
• 打不出来的字？手写出来，或选择它的部件，Hanzi Learn! 就能找到。
• 形近字：列出容易混淆的字，先列出你自己写混过的。还可以给任何一个字写下自己的笔记。

【练到记住为止】
• 从描红到默写的引导练习，按字形和笔顺打分。
• 用“第三课”“旅行”这样的列表整理自己的生词本，还有词语测验：根据释义和拼音写出词语，或听写。
• 复习、按 HSK 等级的测验、可打印的字帖、统计，以及收录你写过的每个字的作品集。

【阅读真实的教材，并在上面书写】
• 附带免费教材：Wikivoyage 的中文会话手册（英文版，附拼音）。
• 也可以导入自己的教材：从文件和照片导入，或用 Google 文档扫描器扫描。即使是扫描件，Hanzi Learn! 也能识别其中的中文，方便你搜索、选取和翻译。
• 用荧光笔划重点、在页边书写、跳到指定页、把扫描页转正，并把内容连同笔记一起以 PDF 分享。

【你的内容，只属于你】
没有账户，没有广告，没有跟踪。你的页面、词语和学习进度都留在平板上，可以随时在“设置 › 你的数据”中导出。手写识别、文字识别和翻译使用在设备上运行的 Google ML Kit：手写识别模型在首次启动 Hanzi Learn! 时下载，翻译模型在第一次翻译时下载；ML Kit 开启期间，Google 会收到匿名诊断数据。如需关闭：“设置 › 手写识别 › 用 Google ML Kit 识别”。下载完成后，一切都能离线使用。

支持中文、英语、西班牙语、葡萄牙语和法语。
```

## French (Google Play: fr-FR)

### App Store

**Name** (30/30): Hanzi Learn! Écrire le chinois

**Subtitle** (27/30): Caractères, trait par trait

**Keywords** (99/100): mandarin,pinyin,hsk,écriture,simplifié,traditionnel,vocabulaire,traduction,ocr,scanner,dictionnaire

**Promotional text** (159/170): Écrivez le chinois à la main, Hanzi Learn! le lit. Visez du chinois avec l’appareil photo et traduisez-le. Numérisez votre manuel, écrivez dessus. Sans compte.

**Description** (3773/4000):

```text
Écrivez le chinois à la main, Hanzi Learn! le lit. Visez du chinois avec l’appareil photo, Hanzi Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hanzi Learn! est un cahier de chinois pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT HANZI LEARN!
• Lit votre écriture : caractères chinois, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le chinois.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque caractère s’écrire, simplifié et traditionnel, avec des mots et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, HANZI LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en chinois et Hanzi Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Avec l’appareil photo : Hanzi Learn! trouve le chinois, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le chinois : tapez un mot ou une phrase dans votre langue, obtenez-le en chinois, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

CHAQUE CARACTÈRE, TRAIT PAR TRAIT
• Plus de 9 500 caractères, simplifiés et traditionnels, chacun s’écrivant dans l’ordre.
• Pinyin, sens, éléments, nombre de traits et niveau HSK.
• Plus de 85 000 mots et 11 000 phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Impossible de taper un caractère ? Dessinez-le, ou choisissez ses éléments, et Hanzi Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau HSK, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : le guide de conversation mandarin de Wikivoyage, avec le pinyin, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents. Hanzi Learn! lit le chinois qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hanzi Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et chinois.
```

### Google Play

**Title** (30/30): Hanzi Learn! Écrire le chinois

**Short description** (75/80): Écrivez le chinois à la main : caractères trait par trait, lus et traduits.

**Full description** (3810/4000):

```text
Écrivez le chinois à la main, Hanzi Learn! le lit. Visez du chinois avec l’appareil photo, Hanzi Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hanzi Learn! est un cahier de chinois pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Hanzi Learn!
• Lit votre écriture : caractères chinois, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le chinois.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque caractère s’écrire, simplifié et traditionnel, avec des mots et des phrases d’exemple.
• Vous ramène les caractères et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Hanzi Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en chinois et Hanzi Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

Traduisez, dans les deux sens
• Avec l’appareil photo : Hanzi Learn! trouve le chinois, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le chinois : tapez un mot ou une phrase dans votre langue, obtenez-le en chinois, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses caractères.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

Chaque caractère, trait par trait
• Plus de 9 500 caractères, simplifiés et traditionnels, chacun s’écrivant dans l’ordre.
• Pinyin, sens, éléments, nombre de traits et niveau HSK.
• Plus de 85 000 mots et 11 000 phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Impossible de taper un caractère ? Dessinez-le, ou choisissez ses éléments, et Hanzi Learn! le trouve.
• Ressemblances : les caractères que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quel caractère.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau HSK, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : le guide de conversation mandarin de Wikivoyage, avec le pinyin, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google. Hanzi Learn! lit le chinois qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hanzi Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et chinois.
```
