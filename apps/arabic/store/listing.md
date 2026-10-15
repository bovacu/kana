# Arabic Learn!: App Store and Google Play texts

Arabic Learn!’s texts for the App Store and Google Play, in its five languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 143 characters: the letters, hamza’s seats, the joined forms, the vowel marks and the 10 Arabic-Indic digits). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/arabic/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/arabic/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/arabic/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Arabic = Play `ar`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Arabic:** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.
- **Arabic texts:** wherever the name stands in Arabic text it is followed by an invisible left-to-right mark (U+200E), as in the app’s own Arabic strings, so that its “!” stays on the right of “Learn”. It is counted in the lengths below; copy the field whole and it comes along.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Arabic Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Alphabet, then any letter: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few letters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
4. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Arabic). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Translate with Google is unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Lectures › Scan pages is first used (this app has no Text from a photo: ML Kit cannot read printed Arabic). The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Arabic, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | ar | fr |
|---|---|---|---|---|---|
| App Store name (30) | 30 | 29 | 29 | 28 | 30 |
| App Store subtitle (30) | 26 | 26 | 25 | 26 | 24 |
| App Store keywords (100) | 98 | 95 | 86 | 76 | 96 |
| App Store promotional text (170) | 163 | 147 | 152 | 132 | 158 |
| App Store description (4000) | 3075 | 3267 | 3279 | 2897 | 3538 |
| Google Play title (30) | 30 | 29 | 29 | 28 | 30 |
| Google Play short description (80) | 70 | 68 | 71 | 60 | 64 |
| Google Play full description (4000) | 3103 | 3308 | 3313 | 2950 | 3575 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (30/30): Arabic Learn!: Arabic Alphabet

**Subtitle** (26/30): Write every letter by hand

**Keywords** (98/100): handwriting,harakat,hamza,vowels,script,vocabulary,translate,romanization,pencil,beginner,practice

**Promotional text** (163/170): Write Arabic by hand and Arabic Learn! reads it. Watch each letter write itself in all its joined forms, then trace it and write it from memory. No account needed.

**Description** (3075/4000):

```text
Write Arabic by hand, and Arabic Learn! reads it. Watch every letter write itself from right to left, in each of the forms it takes in a word. Scan a worksheet, and write on it.

Arabic Learn! is a Arabic notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: Arabic letters and words, written with the Apple Pencil or your finger.
• Translates with Google: what you write, and your own language into Arabic.
• Turns scans, PDFs and photos into lessons you can highlight and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

WRITE, AND ARABIC LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Arabic text, and Arabic Learn! writes it out by hand on your page.
• Five themes, from paper to night.

TRANSLATE, BOTH WAYS
• From your page: lasso what you wrote and read it in your language.
• Into Arabic: type a word or a sentence in your language, get it in Arabic, then save its words, write it on the page or practice its letters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

THE ARABIC ALPHABET, STROKE BY STROKE
• Every letter, alone and in its joined forms, hamza on its seats, the vowel marks and the Arabic digits, each one writing itself from right to left.
• Every letter with its name, from the alphabet itself to the extra letters and marks.
• Words with their vowels, romanization and meanings, and example sentences, read aloud by your iPad’s voices.
• Can’t find a letter? Draw it, and Arabic Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: Wikivoyage’s Arabic phrasebook, with romanization.
• Bring your own from Files, Photos or the document camera, and write on them.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting and translation use Google ML Kit on the device: the handwriting model downloads when Arabic Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Arabic and French.
```

### Google Play

**Title** (30/30): Arabic Learn!: Arabic Alphabet

**Short description** (70/80): Write Arabic by hand: every letter in all its forms, stroke by stroke.

**Full description** (3103/4000):

```text
Write Arabic by hand, and Arabic Learn! reads it. Watch every letter write itself from right to left, in each of the forms it takes in a word. Scan a worksheet, and write on it.

Arabic Learn! is a Arabic notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: Arabic letters and words, written with a stylus or your finger.
• Translates with Google: what you write, and your own language into Arabic.
• Turns scans, PDFs and photos into lessons you can highlight and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

Write, and Arabic Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Arabic text, and Arabic Learn! writes it out by hand on your page.
• Five themes, from paper to night.

Translate, both ways
• From your page: lasso what you wrote and read it in your language.
• Into Arabic: type a word or a sentence in your language, get it in Arabic, then save its words, write it on the page or practice its letters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

The Arabic alphabet, stroke by stroke
• Every letter, alone and in its joined forms, hamza on its seats, the vowel marks and the Arabic digits, each one writing itself from right to left.
• Every letter with its name, from the alphabet itself to the extra letters and marks.
• Words with their vowels, romanization and meanings, and example sentences, read aloud by your tablet’s voices.
• Can’t find a letter? Draw it, and Arabic Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: Wikivoyage’s Arabic phrasebook, with romanization.
• Bring your own from your files and photos, or with Google’s document scanner, and write on them.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting and translation use Google ML Kit on the device: the handwriting model downloads when Arabic Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Arabic and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (29/30): Arabic Learn!: alfabeto árabe

**Subtitle** (26/30): Cada letra, escrita a mano

**Keywords** (95/100): escritura,harakat,hamza,vocales,vocabulario,traductor,romanización,pencil,principiante,práctica

**Promotional text** (147/170): Escribe árabe a mano y Arabic Learn! lo lee. Mira cómo se escribe cada letra en todas sus formas, cálcala y luego escríbela de memoria. Sin cuenta.

**Description** (3267/4000):

```text
Escribe árabe a mano y Arabic Learn! lo lee. Mira cómo se escribe cada letra, de derecha a izquierda, en cada una de las formas que toma en una palabra. Escanea una hoja de ejercicios y escribe encima.

Arabic Learn! es un cuaderno de árabe para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: letras y palabras árabes, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, y de tu idioma al árabe.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y ARABIC LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en árabe y Arabic Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Desde tu página: rodea lo que has escrito con el lazo y léelo en tu idioma.
• Al árabe: escribe una palabra o una frase en tu idioma, obtenla en árabe y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

EL ALFABETO ÁRABE, TRAZO A TRAZO
• Cada letra, sola y en sus formas unidas, la hamza y sus soportes, los signos vocálicos y las cifras del árabe, cada una escribiéndose de derecha a izquierda.
• Cada letra con su nombre, del alfabeto básico a las letras y signos añadidos.
• Palabras vocalizadas para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No encuentras una letra? Dibújala y Arabic Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: la guía de árabe de Wikiviajes, con romanización, en español.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos y escribe encima.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Arabic Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, árabe y francés.
```

### Google Play

**Title** (29/30): Arabic Learn!: alfabeto árabe

**Short description** (68/80): Escribe árabe a mano: cada letra en todas sus formas, trazo a trazo.

**Full description** (3308/4000):

```text
Escribe árabe a mano y Arabic Learn! lo lee. Mira cómo se escribe cada letra, de derecha a izquierda, en cada una de las formas que toma en una palabra. Escanea una hoja de ejercicios y escribe encima.

Arabic Learn! es un cuaderno de árabe para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: letras y palabras árabes, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, y de tu idioma al árabe.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Arabic Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en árabe y Arabic Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

Traduce en los dos sentidos
• Desde tu página: rodea lo que has escrito con el lazo y léelo en tu idioma.
• Al árabe: escribe una palabra o una frase en tu idioma, obtenla en árabe y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

El alfabeto árabe, trazo a trazo
• Cada letra, sola y en sus formas unidas, la hamza y sus soportes, los signos vocálicos y las cifras del árabe, cada una escribiéndose de derecha a izquierda.
• Cada letra con su nombre, del alfabeto básico a las letras y signos añadidos.
• Palabras vocalizadas para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No encuentras una letra? Dibújala y Arabic Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: la guía de árabe de Wikiviajes, con romanización, en español.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google y escribe encima.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Arabic Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, árabe y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (29/30): Arabic Learn!: alfabeto árabe

**Subtitle** (25/30): Cada letra, escrita à mão

**Keywords** (86/100): harakat,hamza,vogais,vocabulário,tradutor,romanização,pencil,iniciante,prática,leitura

**Promotional text** (152/170): Escreva árabe à mão e o Arabic Learn! lê. Veja cada letra sendo escrita em todas as suas formas, cubra o traçado e depois escreva de memória. Sem conta.

**Description** (3279/4000):

```text
Escreva árabe à mão e o Arabic Learn! lê. Veja cada letra sendo escrita, da direita para a esquerda, em cada uma das formas que ela toma numa palavra. Digitalize uma folha de exercícios e escreva nela.

O Arabic Learn! é um caderno de árabe para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: letras e palavras árabes, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, e do seu idioma para o árabe.
• Transforma digitalizações, PDFs e fotos em lições para marcar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O ARABIC LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em árabe e o Arabic Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

TRADUZA NOS DOIS SENTIDOS
• Da sua página: circule o que você escreveu com o laço e leia no seu idioma.
• Para o árabe: digite uma palavra ou uma frase no seu idioma, veja em árabe e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O ALFABETO ÁRABE, TRAÇO A TRAÇO
• Cada letra, sozinha e em suas formas ligadas, a hamza e seus suportes, os sinais vocálicos e os algarismos do árabe, cada um se escrevendo da direita para a esquerda.
• Cada letra com seu nome, do alfabeto básico às letras e sinais adicionais.
• Palavras vocalizadas para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não acha uma letra? Desenhe, e o Arabic Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: o guia de conversação árabe do Wikivoyage, com romanização, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos e escreva nelas.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Arabic Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, árabe e francês.
```

### Google Play

**Title** (29/30): Arabic Learn!: alfabeto árabe

**Short description** (71/80): Escreva árabe à mão: cada letra em todas as suas formas, traço a traço.

**Full description** (3313/4000):

```text
Escreva árabe à mão e o Arabic Learn! lê. Veja cada letra sendo escrita, da direita para a esquerda, em cada uma das formas que ela toma numa palavra. Digitalize uma folha de exercícios e escreva nela.

O Arabic Learn! é um caderno de árabe para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: letras e palavras árabes, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, e do seu idioma para o árabe.
• Transforma digitalizações, PDFs e fotos em lições para marcar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Arabic Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em árabe e o Arabic Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

Traduza nos dois sentidos
• Da sua página: circule o que você escreveu com o laço e leia no seu idioma.
• Para o árabe: digite uma palavra ou uma frase no seu idioma, veja em árabe e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O alfabeto árabe, traço a traço
• Cada letra, sozinha e em suas formas ligadas, a hamza e seus suportes, os sinais vocálicos e os algarismos do árabe, cada um se escrevendo da direita para a esquerda.
• Cada letra com seu nome, do alfabeto básico às letras e sinais adicionais.
• Palavras vocalizadas para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não acha uma letra? Desenhe, e o Arabic Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: o guia de conversação árabe do Wikivoyage, com romanização, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google e escreva nelas.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Arabic Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, árabe e francês.
```

## Arabic (Google Play: ar)

### App Store

**Name** (28/30): Arabic Learn!‎: اكتب العربية

**Subtitle** (26/30): كل حرف بأشكاله، ضربةً ضربة

**Keywords** (76/100): الحروف,الأبجدية,خط اليد,الحركات,الهمزة,المفردات,ترجمة,تعلم,مبتدئ,إملاء,تدريب

**Promotional text** (132/170): اكتب بالعربية بخط يدك، ويقرؤها Arabic Learn!‎. شاهد كل حرف يُكتب بكل أشكاله، ثم تتبّعه واكتبه من الذاكرة. بلا حساب، ويعمل دون اتصال.

**Description** (2897/4000):

```text
اكتب بالعربية بخط يدك، ويقرؤها Arabic Learn!‎. شاهد كل حرف يُكتب أمامك من اليمين إلى اليسار، في كل شكل يتخذه داخل الكلمة. امسح ورقة عمل ضوئيًا، واكتب عليها.

يُعدّ Arabic Learn!‎ دفترًا للعربية على iPad يفهم ما تكتبه.

■ ماذا يفعل
• يقرأ خط يدك: الحروف والكلمات العربية المكتوبة بقلم Apple Pencil أو بإصبعك.
• يترجم عبر Google: ما تكتبه إلى الإنجليزية، والإنجليزية إلى العربية.
• يحوّل الصفحات الممسوحة ضوئيًا وملفات PDF والصور إلى دروس تظلّلها وتكتب عليها.
• يعرض كل حرف وهو يُكتب بترتيب ضرباته، مع كلمات وجمل أمثلة.
• يعيد إليك الحروف والكلمات للمراجعة، ويباعد بين المراجعات كلما كتبتها صحيحة.

■ اكتب، ويقرأ Arabic Learn!‎
• صفحة لا نهاية لها للملاحظات والتدريب. أحِط ما كتبته بأداة التحديد لتتحقق منه، أو تنسخه كنص، أو تحفظ كلمة، أو تترجمه.
• الصق نصًا عربيًا، فيكتبه Arabic Learn!‎ بخط اليد على صفحتك.
• خمس سمات، من الورق إلى الليل.

■ ترجمة في الاتجاهين
• من صفحتك: أحِط ما كتبته بأداة التحديد واقرأه بالإنجليزية.
• إلى العربية: اكتب كلمة أو جملة بالإنجليزية واحصل عليها بالعربية، ثم احفظ كلماتها أو اكتبها على الصفحة أو تدرّب على حروفها.
• يعمل كل ذلك على iPad عبر Google ML Kit: تُنزَّل كل لغة مرة واحدة، ثم تعمل الترجمة دون اتصال.
• عند استخدام التطبيق بالإنجليزية أو الإسبانية أو البرتغالية أو الفرنسية، تكون الترجمة بين تلك اللغة والعربية.

■ الأبجدية العربية، ضربةً بعد ضربة
• كل حرف منفردًا وفي أشكال اتصاله، والهمزة على كراسيها، والحركات، والأرقام العربية، وكلها تُكتب أمامك من اليمين إلى اليسار.
• لكل حرف اسمه، من حروف الأبجدية الأساسية إلى الحروف والعلامات الإضافية.
• كلمات مشكولة لتقرأها وتستمع إليها، مع نطقها بالحروف اللاتينية ومعانيها بالإنجليزية، وجمل أمثلة، تقرؤها أصوات iPad.
• لا تجد حرفًا؟ اكتبه، وسيجده Arabic Learn!‎.
• الحروف المتشابهة: ما يكثر الخلط بينها، بدءًا بما خلطت أنت بينه. واكتب ملاحظتك الخاصة على أي حرف.

■ تدرّب حتى تُتقن
• تدريب موجَّه، من تتبّع الخطوط إلى الكتابة من الذاكرة، مع نقاط على الشكل وترتيب الضربات.
• مفرداتك الخاصة في قوائم مثل «الدرس ٣» أو «السفر»، مع اختبارات للكلمات: اكتب كل كلمة من معناها، أو بالسمع.
• مراجعات، واختبارات حسب المستوى، وأوراق تدريب للطباعة، وإحصاءات، وألبوم لكل ما كتبته.

■ اقرأ دروسًا حقيقية، واكتب عليها
• دروس مجانية مضمّنة: دليل المحادثة العربية من Wikivoyage (بالإنجليزية، مع النطق بالحروف اللاتينية).
• أضف دروسك من «الملفات» و«الصور» أو كاميرا المستندات، واكتب عليها.
• ظلّل، واكتب في الهوامش، وانتقل إلى أي صفحة، واعدل الصفحة الممسوحة المائلة، وشارك الكل ملف PDF مع ملاحظاتك.

■ ملكك، وخاص بك
لا حساب، ولا إعلانات، ولا تتبّع. تبقى صفحاتك وكلماتك وتقدّمك على iPad، ويمكنك تصديرها متى شئت من «الإعدادات › بياناتك». تستخدم قراءة خط اليد والترجمة خدمة Google ML Kit التي تعمل على الجهاز: يُنزَّل نموذج خط اليد عند تشغيل Arabic Learn!‎ أول مرة، ونموذج الترجمة أول مرة تترجم فيها، وما دام ML Kit مفعّلًا تتلقى Google بيانات تشخيص مجهولة الهوية. لإيقافه: «الإعدادات › الكتابة اليدوية › القراءة باستخدام Google ML Kit». وبعد هذه التنزيلات يعمل كل شيء دون اتصال.

بالعربية والإنجليزية والإسبانية والبرتغالية والفرنسية.
```

### Google Play

**Title** (28/30): Arabic Learn!‎: اكتب العربية

**Short description** (60/80): تعلّم كتابة العربية بخط يدك: كل حرف بأشكاله، ضربةً بعد ضربة.

**Full description** (2950/4000):

```text
اكتب بالعربية بخط يدك، ويقرؤها Arabic Learn!‎. شاهد كل حرف يُكتب أمامك من اليمين إلى اليسار، في كل شكل يتخذه داخل الكلمة. امسح ورقة عمل ضوئيًا، واكتب عليها.

يُعدّ Arabic Learn!‎ دفترًا للعربية على جهازك اللوحي العامل بنظام Android يفهم ما تكتبه.

■ ماذا يفعل
• يقرأ خط يدك: الحروف والكلمات العربية المكتوبة بقلم إلكتروني أو بإصبعك.
• يترجم عبر Google: ما تكتبه إلى الإنجليزية، والإنجليزية إلى العربية.
• يحوّل الصفحات الممسوحة ضوئيًا وملفات PDF والصور إلى دروس تظلّلها وتكتب عليها.
• يعرض كل حرف وهو يُكتب بترتيب ضرباته، مع كلمات وجمل أمثلة.
• يعيد إليك الحروف والكلمات للمراجعة، ويباعد بين المراجعات كلما كتبتها صحيحة.

■ اكتب، ويقرأ Arabic Learn!‎
• صفحة لا نهاية لها للملاحظات والتدريب. أحِط ما كتبته بأداة التحديد لتتحقق منه، أو تنسخه كنص، أو تحفظ كلمة، أو تترجمه.
• الصق نصًا عربيًا، فيكتبه Arabic Learn!‎ بخط اليد على صفحتك.
• خمس سمات، من الورق إلى الليل.

■ ترجمة في الاتجاهين
• من صفحتك: أحِط ما كتبته بأداة التحديد واقرأه بالإنجليزية.
• إلى العربية: اكتب كلمة أو جملة بالإنجليزية واحصل عليها بالعربية، ثم احفظ كلماتها أو اكتبها على الصفحة أو تدرّب على حروفها.
• يعمل كل ذلك على جهازك اللوحي عبر Google ML Kit: تُنزَّل كل لغة مرة واحدة، ثم تعمل الترجمة دون اتصال.
• عند استخدام التطبيق بالإنجليزية أو الإسبانية أو البرتغالية أو الفرنسية، تكون الترجمة بين تلك اللغة والعربية.

■ الأبجدية العربية، ضربةً بعد ضربة
• كل حرف منفردًا وفي أشكال اتصاله، والهمزة على كراسيها، والحركات، والأرقام العربية، وكلها تُكتب أمامك من اليمين إلى اليسار.
• لكل حرف اسمه، من حروف الأبجدية الأساسية إلى الحروف والعلامات الإضافية.
• كلمات مشكولة لتقرأها وتستمع إليها، مع نطقها بالحروف اللاتينية ومعانيها بالإنجليزية، وجمل أمثلة، تقرؤها أصوات جهازك اللوحي.
• لا تجد حرفًا؟ اكتبه، وسيجده Arabic Learn!‎.
• الحروف المتشابهة: ما يكثر الخلط بينها، بدءًا بما خلطت أنت بينه. واكتب ملاحظتك الخاصة على أي حرف.

■ تدرّب حتى تُتقن
• تدريب موجَّه، من تتبّع الخطوط إلى الكتابة من الذاكرة، مع نقاط على الشكل وترتيب الضربات.
• مفرداتك الخاصة في قوائم مثل «الدرس ٣» أو «السفر»، مع اختبارات للكلمات: اكتب كل كلمة من معناها، أو بالسمع.
• مراجعات، واختبارات حسب المستوى، وأوراق تدريب للطباعة، وإحصاءات، وألبوم لكل ما كتبته.

■ اقرأ دروسًا حقيقية، واكتب عليها
• دروس مجانية مضمّنة: دليل المحادثة العربية من Wikivoyage (بالإنجليزية، مع النطق بالحروف اللاتينية).
• أضف دروسك من ملفاتك وصورك، أو بماسح المستندات من Google، واكتب عليها.
• ظلّل، واكتب في الهوامش، وانتقل إلى أي صفحة، واعدل الصفحة الممسوحة المائلة، وشارك الكل ملف PDF مع ملاحظاتك.

■ ملكك، وخاص بك
لا حساب، ولا إعلانات، ولا تتبّع. تبقى صفحاتك وكلماتك وتقدّمك على جهازك اللوحي، ويمكنك تصديرها متى شئت من «الإعدادات › بياناتك». تستخدم قراءة خط اليد والترجمة خدمة Google ML Kit التي تعمل على الجهاز: يُنزَّل نموذج خط اليد عند تشغيل Arabic Learn!‎ أول مرة، ونموذج الترجمة أول مرة تترجم فيها، وما دام ML Kit مفعّلًا تتلقى Google بيانات تشخيص مجهولة الهوية. لإيقافه: «الإعدادات › الكتابة اليدوية › القراءة باستخدام Google ML Kit». وبعد هذه التنزيلات يعمل كل شيء دون اتصال.

بالعربية والإنجليزية والإسبانية والبرتغالية والفرنسية.
```

## French (Google Play: fr-FR)

### App Store

**Name** (30/30): Arabic Learn! L’alphabet arabe

**Subtitle** (24/30): Chaque lettre, à la main

**Keywords** (96/100): écriture,harakat,hamza,voyelles,vocabulaire,traduction,romanisation,pencil,débutant,entraînement

**Promotional text** (158/170): Écrivez l’arabe à la main, Arabic Learn! le lit. Regardez chaque lettre s’écrire dans toutes ses formes, repassez-la, puis écrivez-la de mémoire. Sans compte.

**Description** (3538/4000):

```text
Écrivez l’arabe à la main, Arabic Learn! le lit. Regardez chaque lettre s’écrire de droite à gauche, dans chacune des formes qu’elle prend dans un mot. Numérisez une fiche d’exercices, et écrivez dessus.

Arabic Learn! est un cahier d’arabe pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT ARABIC LEARN!
• Lit votre écriture : lettres et mots arabes, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, et de votre langue vers l’arabe.
• Fait de vos numérisations, PDF et photos des leçons à surligner et à annoter.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, ARABIC LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en arabe et Arabic Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Depuis votre page : entourez ce que vous avez écrit au lasso et lisez-le dans votre langue.
• Vers l’arabe : tapez un mot ou une phrase dans votre langue, obtenez-le en arabe, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

L’ALPHABET ARABE, TRAIT PAR TRAIT
• Chaque lettre, seule et dans ses formes liées, la hamza et ses supports, les signes de voyelles et les chiffres de l’arabe, chacun s’écrivant de droite à gauche.
• Chaque lettre avec son nom, de l’alphabet de base aux lettres et signes supplémentaires.
• Des mots vocalisés à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Arabic Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : le guide de conversation arabe de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents, et écrivez dessus.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement d’Arabic Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et arabe.
```

### Google Play

**Title** (30/30): Arabic Learn! L’alphabet arabe

**Short description** (64/80): Écrivez l’arabe à la main, chaque lettre dans toutes ses formes.

**Full description** (3575/4000):

```text
Écrivez l’arabe à la main, Arabic Learn! le lit. Regardez chaque lettre s’écrire de droite à gauche, dans chacune des formes qu’elle prend dans un mot. Numérisez une fiche d’exercices, et écrivez dessus.

Arabic Learn! est un cahier d’arabe pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Arabic Learn!
• Lit votre écriture : lettres et mots arabes, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, et de votre langue vers l’arabe.
• Fait de vos numérisations, PDF et photos des leçons à surligner et à annoter.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Arabic Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en arabe et Arabic Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

Traduisez, dans les deux sens
• Depuis votre page : entourez ce que vous avez écrit au lasso et lisez-le dans votre langue.
• Vers l’arabe : tapez un mot ou une phrase dans votre langue, obtenez-le en arabe, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

L’alphabet arabe, trait par trait
• Chaque lettre, seule et dans ses formes liées, la hamza et ses supports, les signes de voyelles et les chiffres de l’arabe, chacun s’écrivant de droite à gauche.
• Chaque lettre avec son nom, de l’alphabet de base aux lettres et signes supplémentaires.
• Des mots vocalisés à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Arabic Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : le guide de conversation arabe de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google, et écrivez dessus.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement d’Arabic Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et arabe.
```
