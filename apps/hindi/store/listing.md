# Hindi Learn!: App Store and Google Play texts

Hindi Learn!’s texts for the App Store and Google Play, in its five languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 90 characters: vowels, consonants, vowel signs, marks and the 10 Devanagari digits). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/hindi/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/hindi/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/hindi/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Hindi = Play `hi-IN`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Hindi:** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Hindi Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Alphabet, then any letter: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few letters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. Tap the camera on the toolbar (or press and hold the page › Text from photo): the camera reads Hindi text live; Hold keeps the lines, Write puts them on the page, Translate with Google translates them. Choosing a photo works too.
4. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
5. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Hindi). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting, text recognition and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. The text recognition model is inside the app. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Text from a photo and Translate with Google are unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Text from a photo or Lectures › Scan pages is first used. The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Hindi, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | hi | fr |
|---|---|---|---|---|---|
| App Store name (30) | 30 | 30 | 30 | 25 | 28 |
| App Store subtitle (30) | 30 | 25 | 25 | 22 | 30 |
| App Store keywords (100) | 97 | 97 | 91 | 87 | 95 |
| App Store promotional text (170) | 153 | 152 | 146 | 158 | 155 |
| App Store description (4000) | 3306 | 3482 | 3463 | 3533 | 3786 |
| Google Play title (30) | 30 | 30 | 30 | 25 | 28 |
| Google Play short description (80) | 70 | 66 | 64 | 60 | 74 |
| Google Play full description (4000) | 3334 | 3523 | 3497 | 3566 | 3823 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (30/30): Hindi Learn!: Write Devanagari

**Subtitle** (30/30): Every letter, stroke by stroke

**Keywords** (97/100): alphabet,handwriting,matra,vowels,consonants,vocabulary,translate,ocr,scanner,romanization,pencil

**Promotional text** (153/170): Write Hindi by hand and Hindi Learn! reads it. Point the camera at Hindi and translate it. Scan your textbook and write on it. No account, works offline.

**Description** (3306/4000):

```text
Write Hindi by hand, and Hindi Learn! reads it. Point the camera at Hindi, and it translates it. Scan your textbook, and write on it.

Hindi Learn! is a Hindi notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: Devanagari letters and words, written with the Apple Pencil or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Hindi.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

WRITE, AND HINDI LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Hindi text, and Hindi Learn! writes it out by hand on your page.
• Five themes, from paper to night.

TRANSLATE, BOTH WAYS
• From the camera: Hindi Learn! finds the Hindi, translates it line by line, and writes the lines you keep on your page.
• Into Hindi: type a word or a sentence in your language, get it in Hindi, then save its words, write it on the page or practice its letters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

DEVANAGARI, STROKE BY STROKE
• Every vowel, consonant and vowel sign (matra), and the Devanagari digits, each one writing itself in the right order, the line along the top last.
• Every letter with its name, from the core letters to the rare ones.
• Words to read and listen to, with their romanization and meanings, and example sentences, read aloud by your iPad’s voices.
• Can’t find a letter? Draw it, and Hindi Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: Wikivoyage’s Hindi phrasebook, with romanization.
• Bring your own from Files, Photos or the document camera. Hindi Learn! reads the Hindi in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hindi Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Hindi and French.
```

### Google Play

**Title** (30/30): Hindi Learn!: Write Devanagari

**Short description** (70/80): Write Hindi by hand: Devanagari stroke by stroke, read and translated.

**Full description** (3334/4000):

```text
Write Hindi by hand, and Hindi Learn! reads it. Point the camera at Hindi, and it translates it. Scan your textbook, and write on it.

Hindi Learn! is a Hindi notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: Devanagari letters and words, written with a stylus or your finger.
• Translates with Google: what you write, what you photograph, any line of a PDF, and your own language into Hindi.
• Reads text through the camera: point it at a sign, a menu or a book, live or from a photo.
• Turns scans, PDFs and photos into lessons you can highlight, search and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

Write, and Hindi Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Hindi text, and Hindi Learn! writes it out by hand on your page.
• Five themes, from paper to night.

Translate, both ways
• From the camera: Hindi Learn! finds the Hindi, translates it line by line, and writes the lines you keep on your page.
• Into Hindi: type a word or a sentence in your language, get it in Hindi, then save its words, write it on the page or practice its letters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

Devanagari, stroke by stroke
• Every vowel, consonant and vowel sign (matra), and the Devanagari digits, each one writing itself in the right order, the line along the top last.
• Every letter with its name, from the core letters to the rare ones.
• Words to read and listen to, with their romanization and meanings, and example sentences, read aloud by your tablet’s voices.
• Can’t find a letter? Draw it, and Hindi Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: Wikivoyage’s Hindi phrasebook, with romanization.
• Bring your own from your files and photos, or with Google’s document scanner. Hindi Learn! reads the Hindi in them, even in scans, so you can search it, select it and translate it.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting, text recognition and translation use Google ML Kit on the device: the handwriting model downloads when Hindi Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Hindi and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (30/30): Hindi Learn!: escribe en hindi

**Subtitle** (25/30): Devanagari, trazo a trazo

**Keywords** (97/100): alfabeto,escritura,matra,vocales,consonantes,vocabulario,traductor,ocr,escáner,romanización,india

**Promotional text** (152/170): Escribe hindi a mano y Hindi Learn! lo lee. Apunta la cámara al hindi y tradúcelo. Escanea tu libro y escribe encima. Sin cuenta, funciona sin conexión.

**Description** (3482/4000):

```text
Escribe hindi a mano y Hindi Learn! lo lee. Apunta la cámara a un texto en hindi y te lo traduce. Escanea tu libro de texto y escribe encima.

Hindi Learn! es un cuaderno de hindi para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: letras y palabras en devanagari, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al hindi.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y HINDI LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en hindi y Hindi Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Con la cámara: Hindi Learn! encuentra el hindi, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al hindi: escribe una palabra o una frase en tu idioma, obtenla en hindi y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

EL DEVANAGARI, TRAZO A TRAZO
• Todas las vocales, consonantes y matras, y las cifras devanagari, cada una escribiéndose en su orden, con la línea de arriba al final.
• Cada letra con su nombre, de las básicas a las más raras.
• Palabras para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No encuentras una letra? Dibújala y Hindi Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: la guía de hindi de Wikiviajes, con romanización, en español.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos. Hindi Learn! lee el hindi que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hindi Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, hindi y francés.
```

### Google Play

**Title** (30/30): Hindi Learn!: escribe en hindi

**Short description** (66/80): Escribe hindi a mano: devanagari trazo a trazo, leído y traducido.

**Full description** (3523/4000):

```text
Escribe hindi a mano y Hindi Learn! lo lee. Apunta la cámara a un texto en hindi y te lo traduce. Escanea tu libro de texto y escribe encima.

Hindi Learn! es un cuaderno de hindi para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: letras y palabras en devanagari, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, lo que fotografías, cualquier línea de un PDF, y de tu idioma al hindi.
• Lee texto con la cámara: apúntala a un cartel, una carta o un libro, en directo o desde una foto.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar, buscar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Hindi Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en hindi y Hindi Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

Traduce en los dos sentidos
• Con la cámara: Hindi Learn! encuentra el hindi, lo traduce línea a línea y escribe en tu página las líneas que te quedes.
• Al hindi: escribe una palabra o una frase en tu idioma, obtenla en hindi y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

El devanagari, trazo a trazo
• Todas las vocales, consonantes y matras, y las cifras devanagari, cada una escribiéndose en su orden, con la línea de arriba al final.
• Cada letra con su nombre, de las básicas a las más raras.
• Palabras para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No encuentras una letra? Dibújala y Hindi Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: la guía de hindi de Wikiviajes, con romanización, en español.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google. Hindi Learn! lee el hindi que contienen, incluso en los escaneos, para que puedas buscarlo, seleccionarlo y traducirlo.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura, el reconocimiento de texto y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Hindi Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, hindi y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (30/30): Hindi Learn!: escreva em hindi

**Subtitle** (25/30): Devanágari, traço a traço

**Keywords** (91/100): alfabeto,escrita,matra,vogais,consoantes,vocabulário,tradutor,ocr,scanner,romanização,índia

**Promotional text** (146/170): Escreva hindi à mão e o Hindi Learn! lê. Aponte a câmera para o hindi e traduza. Digitalize seu livro e escreva nele. Sem conta, funciona offline.

**Description** (3463/4000):

```text
Escreva hindi à mão e o Hindi Learn! lê. Aponte a câmera para um texto em hindi e ele traduz. Digitalize seu livro e escreva nele.

O Hindi Learn! é um caderno de hindi para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: letras e palavras em devanágari, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o hindi.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O HINDI LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em hindi e o Hindi Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

TRADUZA NOS DOIS SENTIDOS
• Pela câmera: o Hindi Learn! encontra o hindi, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o hindi: digite uma palavra ou uma frase no seu idioma, veja em hindi e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O DEVANÁGARI, TRAÇO A TRAÇO
• Todas as vogais, consoantes e matras, e os algarismos devanágari, cada um se escrevendo na ordem certa, com a linha de cima por último.
• Cada letra com seu nome, das básicas às mais raras.
• Palavras para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não acha uma letra? Desenhe, e o Hindi Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: o guia de conversação hindi do Wikivoyage, com romanização, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos. O Hindi Learn! lê o hindi nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hindi Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, hindi e francês.
```

### Google Play

**Title** (30/30): Hindi Learn!: escreva em hindi

**Short description** (64/80): Escreva hindi à mão: devanágari traço a traço, lido e traduzido.

**Full description** (3497/4000):

```text
Escreva hindi à mão e o Hindi Learn! lê. Aponte a câmera para um texto em hindi e ele traduz. Digitalize seu livro e escreva nele.

O Hindi Learn! é um caderno de hindi para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: letras e palavras em devanágari, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, o que você fotografa, qualquer linha de um PDF, e do seu idioma para o hindi.
• Lê texto com a câmera: aponte para uma placa, um cardápio ou um livro, ao vivo ou a partir de uma foto.
• Transforma digitalizações, PDFs e fotos em lições para marcar, pesquisar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Hindi Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em hindi e o Hindi Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

Traduza nos dois sentidos
• Pela câmera: o Hindi Learn! encontra o hindi, traduz linha por linha e escreve na sua página as linhas que você escolher.
• Para o hindi: digite uma palavra ou uma frase no seu idioma, veja em hindi e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O devanágari, traço a traço
• Todas as vogais, consoantes e matras, e os algarismos devanágari, cada um se escrevendo na ordem certa, com a linha de cima por último.
• Cada letra com seu nome, das básicas às mais raras.
• Palavras para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não acha uma letra? Desenhe, e o Hindi Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: o guia de conversação hindi do Wikivoyage, com romanização, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google. O Hindi Learn! lê o hindi nelas, até nas digitalizações, para você pesquisar, selecionar e traduzir.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita, o reconhecimento de texto e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Hindi Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, hindi e francês.
```

## Hindi (Google Play: hi-IN)

### App Store

**Name** (25/30): Hindi Learn!: हिंदी लिखें

**Subtitle** (22/30): हर अक्षर, सही क्रम में

**Keywords** (87/100): देवनागरी,वर्णमाला,लिखावट,स्वर,व्यंजन,मात्रा,शब्दावली,अनुवाद,अभ्यास,दोहराव,स्कैन,उच्चारण

**Promotional text** (158/170): हाथ से हिंदी लिखें, Hindi Learn! उसे पढ़ लेता है। कैमरा हिंदी की ओर करें और अंग्रेज़ी अनुवाद पाएँ। अपनी किताब स्कैन करें और उसी पर लिखें। कोई खाता नहीं चाहिए।

**Description** (3533/4000):

```text
हाथ से हिंदी लिखें, Hindi Learn! उसे पढ़ लेता है। कैमरा हिंदी की ओर करें, वह उसका अंग्रेज़ी में अनुवाद कर देता है। अपनी किताब स्कैन करें, और उसी पर लिखें।

Hindi Learn! आपके iPad के लिए हिंदी की नोटबुक है, जो आपका लिखा समझती है।

■ यह क्या करता है
• लिखावट पढ़ता है: Apple Pencil या उंगली से लिखे देवनागरी अक्षर और शब्द।
• Google से अनुवाद: आपका लिखा, आपकी खींची फ़ोटो, PDF की कोई भी पंक्ति अंग्रेज़ी में, और अंग्रेज़ी से हिंदी में।
• कैमरे से टेक्स्ट पढ़ता है: किसी साइनबोर्ड, मेन्यू या किताब की ओर करें, लाइव या किसी फ़ोटो से।
• स्कैन किए पन्नों, PDF और फ़ोटो को ऐसे पाठ बनाता है जिन पर आप हाइलाइट कर सकें, खोज सकें और लिख सकें।
• हर अक्षर को सही स्ट्रोक क्रम में लिखकर दिखाता है, शब्दों और उदाहरण वाक्यों के साथ।
• अक्षरों और शब्दों को दोहराव के लिए वापस लाता है, और हर बार सही लिखने पर अंतराल बढ़ाता है।

■ लिखें, और Hindi Learn! पढ़ेगा
• नोट और अभ्यास के लिए अंतहीन पन्ना। अपने लिखे को लैसो से घेरें, फिर उसे जाँचें, टेक्स्ट के रूप में कॉपी करें, शब्द सेव करें या अनुवाद करें।
• हिंदी टेक्स्ट चिपकाएँ, और Hindi Learn! उसे आपके पन्ने पर हाथ से लिख देता है।
• पाँच थीम, काग़ज़ से लेकर रात तक।

■ अनुवाद, दोनों तरफ़
• कैमरे से: Hindi Learn! हिंदी ढूँढता है, हर पंक्ति का अंग्रेज़ी अनुवाद करता है, और जो पंक्तियाँ आप रखें उन्हें पन्ने पर लिख देता है।
• हिंदी में: अंग्रेज़ी में कोई शब्द या वाक्य टाइप करें, उसे हिंदी में पाएँ, फिर उसके शब्द सेव करें, उसे पन्ने पर लिखें या उसके अक्षरों का अभ्यास करें।
• सब कुछ Google ML Kit के साथ आपके iPad पर ही चलता है: हर भाषा एक बार डाउनलोड होती है, फिर अनुवाद बिना इंटरनेट के होता है।
• ऐप को अंग्रेज़ी, स्पेनिश, पुर्तगाली या फ़्रेंच में इस्तेमाल करने पर अनुवाद उस भाषा और हिंदी के बीच होता है।

■ देवनागरी, हर स्ट्रोक के साथ
• हर स्वर, व्यंजन और मात्रा, और देवनागरी अंक, हर एक सही क्रम में लिखकर दिखाया जाता है, ऊपर की रेखा सबसे आख़िर में।
• हर अक्षर अपने नाम के साथ, मुख्य अक्षरों से लेकर कम इस्तेमाल होने वालों तक।
• पढ़ने और सुनने के लिए शब्द, रोमन लिपि में उच्चारण और अंग्रेज़ी अर्थ के साथ, और उदाहरण वाक्य, आपके iPad की आवाज़ में।
• कोई अक्षर नहीं मिल रहा? उसे लिखें, और Hindi Learn! उसे ढूँढ देगा।
• मिलते-जुलते अक्षर: जिन अक्षरों में अक्सर गड़बड़ होती है, पहले वे जिनमें आपसे हुई। किसी भी अक्षर पर अपना नोट रखें।

■ याद होने तक अभ्यास
• निर्देशित अभ्यास, ऊपर से लिखने से लेकर याद से लिखने तक, आकार और स्ट्रोक क्रम पर अंकों के साथ।
• “पाठ 3” या “यात्रा” जैसी सूचियों में आपकी अपनी शब्दावली, शब्द-परीक्षाओं के साथ: हर शब्द उसके अर्थ से, या सुनकर लिखें।
• दोहराव, स्तर के अनुसार परीक्षाएँ, प्रिंट करने लायक अभ्यास शीट, आँकड़े, और आपके लिखे हर अक्षर का एल्बम।

■ असली पाठ पढ़ें, और उन पर लिखें
• मुफ़्त पाठ शामिल: Wikivoyage की हिंदी बोलचाल गाइड (अंग्रेज़ी में, रोमन लिपि में उच्चारण के साथ)।
• अपने पाठ भी लाएँ: फ़ाइलों, फ़ोटो या डॉक्यूमेंट कैमरे से। Hindi Learn! उनमें लिखी हिंदी पढ़ लेता है, स्कैन में भी, ताकि आप उसे खोज सकें, चुन सकें और उसका अनुवाद कर सकें।
• हाइलाइट करें, हाशिये में लिखें, किसी भी पन्ने पर जाएँ, स्कैन को सीधा करें, और सब कुछ अपने नोट के साथ PDF के रूप में शेयर करें।

■ आपका, और निजी
न कोई खाता, न विज्ञापन, न ट्रैकिंग। आपके पन्ने, शब्द और प्रगति आपके iPad पर ही रहते हैं; “सेटिंग्स › आपका डेटा” से उन्हें कभी भी एक्सपोर्ट करें। लिखावट और टेक्स्ट पढ़ने और अनुवाद के लिए डिवाइस पर चलने वाला Google ML Kit इस्तेमाल होता है: लिखावट का मॉडल Hindi Learn! के पहली बार खुलने पर, और अनुवाद का मॉडल पहली बार अनुवाद करने पर डाउनलोड होता है; ML Kit चालू रहने तक Google को गुमनाम डायग्नोस्टिक डेटा मिलता है। बंद करने के लिए: “सेटिंग्स › लिखावट › Google ML Kit से पढ़ें”। इन डाउनलोड के बाद सब कुछ बिना इंटरनेट के चलता है।

हिंदी, अंग्रेज़ी, स्पेनिश, पुर्तगाली और फ़्रेंच में।
```

### Google Play

**Title** (25/30): Hindi Learn!: हिंदी लिखें

**Short description** (60/80): हाथ से हिंदी लिखना सीखें: देवनागरी का हर अक्षर सही क्रम में।

**Full description** (3566/4000):

```text
हाथ से हिंदी लिखें, Hindi Learn! उसे पढ़ लेता है। कैमरा हिंदी की ओर करें, वह उसका अंग्रेज़ी में अनुवाद कर देता है। अपनी किताब स्कैन करें, और उसी पर लिखें।

Hindi Learn! आपके Android टैबलेट के लिए हिंदी की नोटबुक है, जो आपका लिखा समझती है।

■ यह क्या करता है
• लिखावट पढ़ता है: स्टाइलस या उंगली से लिखे देवनागरी अक्षर और शब्द।
• Google से अनुवाद: आपका लिखा, आपकी खींची फ़ोटो, PDF की कोई भी पंक्ति अंग्रेज़ी में, और अंग्रेज़ी से हिंदी में।
• कैमरे से टेक्स्ट पढ़ता है: किसी साइनबोर्ड, मेन्यू या किताब की ओर करें, लाइव या किसी फ़ोटो से।
• स्कैन किए पन्नों, PDF और फ़ोटो को ऐसे पाठ बनाता है जिन पर आप हाइलाइट कर सकें, खोज सकें और लिख सकें।
• हर अक्षर को सही स्ट्रोक क्रम में लिखकर दिखाता है, शब्दों और उदाहरण वाक्यों के साथ।
• अक्षरों और शब्दों को दोहराव के लिए वापस लाता है, और हर बार सही लिखने पर अंतराल बढ़ाता है।

■ लिखें, और Hindi Learn! पढ़ेगा
• नोट और अभ्यास के लिए अंतहीन पन्ना। अपने लिखे को लैसो से घेरें, फिर उसे जाँचें, टेक्स्ट के रूप में कॉपी करें, शब्द सेव करें या अनुवाद करें।
• हिंदी टेक्स्ट चिपकाएँ, और Hindi Learn! उसे आपके पन्ने पर हाथ से लिख देता है।
• पाँच थीम, काग़ज़ से लेकर रात तक।

■ अनुवाद, दोनों तरफ़
• कैमरे से: Hindi Learn! हिंदी ढूँढता है, हर पंक्ति का अंग्रेज़ी अनुवाद करता है, और जो पंक्तियाँ आप रखें उन्हें पन्ने पर लिख देता है।
• हिंदी में: अंग्रेज़ी में कोई शब्द या वाक्य टाइप करें, उसे हिंदी में पाएँ, फिर उसके शब्द सेव करें, उसे पन्ने पर लिखें या उसके अक्षरों का अभ्यास करें।
• सब कुछ Google ML Kit के साथ आपके टैबलेट पर ही चलता है: हर भाषा एक बार डाउनलोड होती है, फिर अनुवाद बिना इंटरनेट के होता है।
• ऐप को अंग्रेज़ी, स्पेनिश, पुर्तगाली या फ़्रेंच में इस्तेमाल करने पर अनुवाद उस भाषा और हिंदी के बीच होता है।

■ देवनागरी, हर स्ट्रोक के साथ
• हर स्वर, व्यंजन और मात्रा, और देवनागरी अंक, हर एक सही क्रम में लिखकर दिखाया जाता है, ऊपर की रेखा सबसे आख़िर में।
• हर अक्षर अपने नाम के साथ, मुख्य अक्षरों से लेकर कम इस्तेमाल होने वालों तक।
• पढ़ने और सुनने के लिए शब्द, रोमन लिपि में उच्चारण और अंग्रेज़ी अर्थ के साथ, और उदाहरण वाक्य, आपके टैबलेट की आवाज़ में।
• कोई अक्षर नहीं मिल रहा? उसे लिखें, और Hindi Learn! उसे ढूँढ देगा।
• मिलते-जुलते अक्षर: जिन अक्षरों में अक्सर गड़बड़ होती है, पहले वे जिनमें आपसे हुई। किसी भी अक्षर पर अपना नोट रखें।

■ याद होने तक अभ्यास
• निर्देशित अभ्यास, ऊपर से लिखने से लेकर याद से लिखने तक, आकार और स्ट्रोक क्रम पर अंकों के साथ।
• “पाठ 3” या “यात्रा” जैसी सूचियों में आपकी अपनी शब्दावली, शब्द-परीक्षाओं के साथ: हर शब्द उसके अर्थ से, या सुनकर लिखें।
• दोहराव, स्तर के अनुसार परीक्षाएँ, प्रिंट करने लायक अभ्यास शीट, आँकड़े, और आपके लिखे हर अक्षर का एल्बम।

■ असली पाठ पढ़ें, और उन पर लिखें
• मुफ़्त पाठ शामिल: Wikivoyage की हिंदी बोलचाल गाइड (अंग्रेज़ी में, रोमन लिपि में उच्चारण के साथ)।
• अपने पाठ भी लाएँ: अपनी फ़ाइलों और फ़ोटो से, या Google के डॉक्यूमेंट स्कैनर से। Hindi Learn! उनमें लिखी हिंदी पढ़ लेता है, स्कैन में भी, ताकि आप उसे खोज सकें, चुन सकें और उसका अनुवाद कर सकें।
• हाइलाइट करें, हाशिये में लिखें, किसी भी पन्ने पर जाएँ, स्कैन को सीधा करें, और सब कुछ अपने नोट के साथ PDF के रूप में शेयर करें।

■ आपका, और निजी
न कोई खाता, न विज्ञापन, न ट्रैकिंग। आपके पन्ने, शब्द और प्रगति आपके टैबलेट पर ही रहते हैं; “सेटिंग्स › आपका डेटा” से उन्हें कभी भी एक्सपोर्ट करें। लिखावट और टेक्स्ट पढ़ने और अनुवाद के लिए डिवाइस पर चलने वाला Google ML Kit इस्तेमाल होता है: लिखावट का मॉडल Hindi Learn! के पहली बार खुलने पर, और अनुवाद का मॉडल पहली बार अनुवाद करने पर डाउनलोड होता है; ML Kit चालू रहने तक Google को गुमनाम डायग्नोस्टिक डेटा मिलता है। बंद करने के लिए: “सेटिंग्स › लिखावट › Google ML Kit से पढ़ें”। इन डाउनलोड के बाद सब कुछ बिना इंटरनेट के चलता है।

हिंदी, अंग्रेज़ी, स्पेनिश, पुर्तगाली और फ़्रेंच में।
```

## French (Google Play: fr-FR)

### App Store

**Name** (28/30): Hindi Learn! Écrire le hindi

**Subtitle** (30/30): Le devanagari, trait par trait

**Keywords** (95/100): alphabet,écriture,matra,voyelles,consonnes,vocabulaire,traduction,ocr,scanner,romanisation,inde

**Promotional text** (155/170): Écrivez le hindi à la main, Hindi Learn! le lit. Visez du hindi avec l’appareil photo et traduisez-le. Numérisez votre manuel, écrivez dessus. Sans compte.

**Description** (3786/4000):

```text
Écrivez le hindi à la main, Hindi Learn! le lit. Visez du hindi avec l’appareil photo, Hindi Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hindi Learn! est un cahier de hindi pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT HINDI LEARN!
• Lit votre écriture : lettres et mots en devanagari, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le hindi.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, HINDI LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en hindi et Hindi Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Avec l’appareil photo : Hindi Learn! trouve le hindi, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le hindi : tapez un mot ou une phrase dans votre langue, obtenez-le en hindi, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

LE DEVANAGARI, TRAIT PAR TRAIT
• Toutes les voyelles, consonnes et matras, et les chiffres devanagari, chacun s’écrivant dans l’ordre, la ligne du haut en dernier.
• Chaque lettre avec son nom, des lettres de base aux plus rares.
• Des mots à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Hindi Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : le guide de conversation hindi de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents. Hindi Learn! lit le hindi qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hindi Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et hindi.
```

### Google Play

**Title** (28/30): Hindi Learn! Écrire le hindi

**Short description** (74/80): Écrivez le hindi à la main : le devanagari trait par trait, lu et traduit.

**Full description** (3823/4000):

```text
Écrivez le hindi à la main, Hindi Learn! le lit. Visez du hindi avec l’appareil photo, Hindi Learn! le traduit. Numérisez votre manuel, et écrivez dessus.

Hindi Learn! est un cahier de hindi pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Hindi Learn!
• Lit votre écriture : lettres et mots en devanagari, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, ce que vous photographiez, n’importe quelle ligne d’un PDF, et de votre langue vers le hindi.
• Lit le texte avec l’appareil photo : visez un panneau, un menu ou un livre, en direct ou depuis une photo.
• Fait de vos numérisations, PDF et photos des leçons à surligner, à annoter et où chercher.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Hindi Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en hindi et Hindi Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

Traduisez, dans les deux sens
• Avec l’appareil photo : Hindi Learn! trouve le hindi, le traduit ligne par ligne et écrit sur votre page les lignes que vous gardez.
• Vers le hindi : tapez un mot ou une phrase dans votre langue, obtenez-le en hindi, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

Le devanagari, trait par trait
• Toutes les voyelles, consonnes et matras, et les chiffres devanagari, chacun s’écrivant dans l’ordre, la ligne du haut en dernier.
• Chaque lettre avec son nom, des lettres de base aux plus rares.
• Des mots à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Hindi Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : le guide de conversation hindi de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google. Hindi Learn! lit le hindi qu’ils contiennent, même numérisé, pour que vous puissiez le chercher, le sélectionner et le traduire.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture, la reconnaissance du texte et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Hindi Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et hindi.
```
