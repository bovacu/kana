# Thai Learn!: App Store and Google Play texts

Thai Learn!’s texts for the App Store and Google Play, in its five languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language), and its data (`../assets/data/characters.kana`, counted 2026-10-13: 83 characters: the 44 consonants with the vowels, tone marks, signs and the 10 Thai digits). The texts round those figures down and say “more than”, so a rebake that adds a few does not make them wrong.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Education (primary), Reference (secondary). Google Play: Education.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). Note that the app itself already says “One purchase, with no ads and no subscription” in Settings › About › Why it works offline (the OFFLINE string, every study app): if the app is not paid once, that text must change too.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/thai/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/thai/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/thai/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, Thai = Play `th`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.
- **Keywords in Thai:** counted here in characters, as App Store Connect counts them; its own counter, shown as you type, is the judge (some scripts have been reported counted in bytes there). If it shows more than 100, drop keywords from the end.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
Thai Learn! needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: on a new install the hand on the toolbar is on, so one finger writes and two fingers move the page. The first Apple Pencil stroke switches to Pencil writing (fingers then move the page); tapping the hand switches back.

To try the main features:
1. The menu › Alphabet, then any letter: its stroke order plays, with words and an example sentence; Practice writes it with guidance, and Score grades it.
2. On the page, write a few letters, circle them with the lasso, and choose Check, Copy as text, Save word or Translate with Google.
3. The menu › Lectures: open the free lesson and write or highlight on it; From Files, From Photos and Scan pages bring in documents; Share as PDF exports one.
4. The menu › Vocabulary: saved words, lists, word exams, and Translate with Google (type English, get Thai). The menu also has Reviews, Exams, Statistics and the Tutorial.

Network and downloads: Handwriting and translation use Google ML Kit, on the device. It is on by default; the switch is the menu › Settings › Handwriting › Read with Google ML Kit. With it on, ML Kit’s handwriting model (about 20 MB) downloads when the app first starts, so the first launch needs the internet; a translation model (about 30 MB) downloads the first time a translation is asked for. Switched off, the app never calls ML Kit: it reads handwriting with its own matcher, Translate with Google is unavailable, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the camera is asked for only when Lectures › Scan pages is first used (this app has no Text from a photo: ML Kit cannot read printed Thai). The photo and file pickers are the system’s own and need no permission. No microphone, location, contacts or tracking.

Reading aloud uses the iPad’s own voices; when none speaks Thai, Set up the voice explains how to add one.

Links open only when tapped: FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | th | fr |
|---|---|---|---|---|---|
| App Store name (30) | 26 | 30 | 30 | 28 | 27 |
| App Store subtitle (30) | 26 | 26 | 25 | 19 | 24 |
| App Store keywords (100) | 94 | 96 | 91 | 87 | 94 |
| App Store promotional text (170) | 143 | 152 | 148 | 148 | 156 |
| App Store description (4000) | 3011 | 3205 | 3204 | 2892 | 3438 |
| Google Play title (30) | 26 | 30 | 30 | 28 | 27 |
| Google Play short description (80) | 78 | 76 | 78 | 73 | 74 |
| Google Play full description (4000) | 3039 | 3246 | 3238 | 2924 | 3475 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (26/30): Thai Learn!: Thai Alphabet

**Subtitle** (26/30): Write every letter by hand

**Keywords** (94/100): handwriting,consonants,vowels,tones,vocabulary,translate,romanization,pencil,beginner,practice

**Promotional text** (143/170): Write Thai by hand and Thai Learn! reads it. Watch each letter write itself, then trace it and write it from memory. No account, works offline.

**Description** (3011/4000):

```text
Write Thai by hand, and Thai Learn! reads it. Watch every letter write itself, from the little loop where most of them start. Scan a worksheet, and write on it.

Thai Learn! is a Thai notebook for your iPad that understands what you write.

WHAT IT DOES
• Reads your handwriting: Thai letters and words, written with the Apple Pencil or your finger.
• Translates with Google: what you write, and your own language into Thai.
• Turns scans, PDFs and photos into lessons you can highlight and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

WRITE, AND THAI LEARN! READS IT
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Thai text, and Thai Learn! writes it out by hand on your page.
• Five themes, from paper to night.

TRANSLATE, BOTH WAYS
• From your page: lasso what you wrote and read it in your language.
• Into Thai: type a word or a sentence in your language, get it in Thai, then save its words, write it on the page or practice its letters.
• It all runs on your iPad with Google ML Kit: each language downloads once, then translates offline.

THE THAI ALPHABET, STROKE BY STROKE
• All 44 consonants by class, the vowels, the tone marks and signs, and the Thai digits, each one writing itself in the right order.
• Every letter with its name, from the core letters to the rare ones.
• Words to read and listen to, with their romanization and meanings, and example sentences, read aloud by your iPad’s voices.
• Can’t find a letter? Draw it, and Thai Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

PRACTICE UNTIL IT STICKS
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

READ REAL LESSONS, AND WRITE ON THEM
• Free lessons included: Wikivoyage’s Thai phrasebook, with romanization.
• Bring your own from Files, Photos or the document camera, and write on them.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your pages, words and progress stay on your iPad; Settings › Your data exports them whenever you like. Handwriting and translation use Google ML Kit on the device: the handwriting model downloads when Thai Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Thai and French.
```

### Google Play

**Title** (26/30): Thai Learn!: Thai Alphabet

**Short description** (78/80): Learn to write Thai by hand: every letter stroke by stroke, read as you write.

**Full description** (3039/4000):

```text
Write Thai by hand, and Thai Learn! reads it. Watch every letter write itself, from the little loop where most of them start. Scan a worksheet, and write on it.

Thai Learn! is a Thai notebook for your Android tablet that understands what you write.

What it does
• Reads your handwriting: Thai letters and words, written with a stylus or your finger.
• Translates with Google: what you write, and your own language into Thai.
• Turns scans, PDFs and photos into lessons you can highlight and write on.
• Shows every letter writing itself, with words and example sentences.
• Brings letters and words back for review, a little later each time you get them right.

Write, and Thai Learn! reads it
• An endless page for notes and practice. Circle what you wrote with the lasso to check it, copy it as text, save a word or translate it.
• Paste Thai text, and Thai Learn! writes it out by hand on your page.
• Five themes, from paper to night.

Translate, both ways
• From your page: lasso what you wrote and read it in your language.
• Into Thai: type a word or a sentence in your language, get it in Thai, then save its words, write it on the page or practice its letters.
• It all runs on your tablet with Google ML Kit: each language downloads once, then translates offline.

The Thai alphabet, stroke by stroke
• All 44 consonants by class, the vowels, the tone marks and signs, and the Thai digits, each one writing itself in the right order.
• Every letter with its name, from the core letters to the rare ones.
• Words to read and listen to, with their romanization and meanings, and example sentences, read aloud by your tablet’s voices.
• Can’t find a letter? Draw it, and Thai Learn! finds it.
• Look-alikes show the letters that get mixed up, yours first. Keep your own note on any letter.

Practice until it sticks
• Guided practice, from tracing to writing from memory, scored on shape and stroke order.
• Your own vocabulary in lists like “Lesson 3” or “Travel”, with word exams: write each word from its meaning, or by ear.
• Reviews, exams by level, printable practice sheets, statistics, and an album of everything you have written.

Read real lessons, and write on them
• Free lessons included: Wikivoyage’s Thai phrasebook, with romanization.
• Bring your own from your files and photos, or with Google’s document scanner, and write on them.
• Highlight, write in the margins, jump to a page, turn a scan upright, and share it all as a PDF with your notes.

Yours, and private
No account, no ads, no tracking. Your pages, words and progress stay on your tablet; Settings › Your data exports them whenever you like. Handwriting and translation use Google ML Kit on the device: the handwriting model downloads when Thai Learn! first starts, each translation model the first time you translate, and while ML Kit is on, Google receives anonymous diagnostics. To switch it off: Settings › Handwriting › Read with Google ML Kit. After those downloads, everything works offline.

In English, Spanish, Portuguese, Thai and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (30/30): Thai Learn!: escribe tailandés

**Subtitle** (26/30): Cada letra, escrita a mano

**Keywords** (96/100): escritura,alfabeto,consonantes,vocales,tonos,tailandia,vocabulario,traductor,romanización,pencil

**Promotional text** (152/170): Escribe tailandés a mano y Thai Learn! lo lee. Mira cómo se escribe cada letra, cálcala y luego escríbela de memoria. Sin cuenta, funciona sin conexión.

**Description** (3205/4000):

```text
Escribe tailandés a mano y Thai Learn! lo lee. Mira cómo se escribe cada letra, desde el pequeño bucle donde empiezan casi todas. Escanea una hoja de ejercicios y escribe encima.

Thai Learn! es un cuaderno de tailandés para tu iPad que entiende lo que escribes.

LO QUE HACE
• Lee tu letra: letras y palabras tailandesas, con el Apple Pencil o con el dedo.
• Traduce con Google: lo que escribes, y de tu idioma al tailandés.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

ESCRIBE, Y THAI LEARN! LO LEE
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en tailandés y Thai Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

TRADUCE EN LOS DOS SENTIDOS
• Desde tu página: rodea lo que has escrito con el lazo y léelo en tu idioma.
• Al tailandés: escribe una palabra o una frase en tu idioma, obtenla en tailandés y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu iPad con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

EL ALFABETO TAILANDÉS, TRAZO A TRAZO
• Las 44 consonantes por clase, las vocales, los tonos y signos, y las cifras tailandesas, cada una escribiéndose en su orden.
• Cada letra con su nombre, de las básicas a las más raras.
• Palabras para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu iPad.
• ¿No encuentras una letra? Dibújala y Thai Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

PRACTICA HASTA QUE SE TE QUEDE
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

LEE LECCIONES DE VERDAD Y ESCRIBE ENCIMA
• Lecciones gratuitas incluidas: la guía de tailandés de Wikivoyage, con romanización, en inglés.
• Trae las tuyas desde Archivos, Fotos o la cámara de documentos y escribe encima.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu iPad; Ajustes › Tus datos los exporta cuando quieras. La escritura y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Thai Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, tailandés y francés.
```

### Google Play

**Title** (30/30): Thai Learn!: escribe tailandés

**Short description** (76/80): Escribe tailandés a mano: cada letra trazo a trazo, leída mientras escribes.

**Full description** (3246/4000):

```text
Escribe tailandés a mano y Thai Learn! lo lee. Mira cómo se escribe cada letra, desde el pequeño bucle donde empiezan casi todas. Escanea una hoja de ejercicios y escribe encima.

Thai Learn! es un cuaderno de tailandés para tu tableta Android que entiende lo que escribes.

Lo que hace
• Lee tu letra: letras y palabras tailandesas, con un lápiz óptico o con el dedo.
• Traduce con Google: lo que escribes, y de tu idioma al tailandés.
• Convierte escaneos, PDF y fotos en lecciones en las que subrayar y escribir.
• Muestra cómo se escribe cada letra, con palabras y frases de ejemplo.
• Programa repasos de las letras y de las palabras, cada vez más espaciados cuando aciertas.

Escribe, y Thai Learn! lo lee
• Una página infinita para apuntes y práctica. Rodea lo que has escrito con el lazo para revisarlo, copiarlo como texto, guardar una palabra o traducirlo.
• Pega un texto en tailandés y Thai Learn! lo escribe a mano en tu página.
• Cinco temas, del papel a la noche.

Traduce en los dos sentidos
• Desde tu página: rodea lo que has escrito con el lazo y léelo en tu idioma.
• Al tailandés: escribe una palabra o una frase en tu idioma, obtenla en tailandés y guarda sus palabras, escríbela en la página o practica sus letras.
• Todo funciona en tu tableta con Google ML Kit: cada idioma se descarga una vez y después traduce sin conexión.

El alfabeto tailandés, trazo a trazo
• Las 44 consonantes por clase, las vocales, los tonos y signos, y las cifras tailandesas, cada una escribiéndose en su orden.
• Cada letra con su nombre, de las básicas a las más raras.
• Palabras para leer y escuchar, con su romanización y su significado, y frases de ejemplo, leídas en voz alta con las voces de tu tableta.
• ¿No encuentras una letra? Dibújala y Thai Learn! la encuentra.
• Parecidos: las letras que se confunden, empezando por las tuyas. Y tu propia nota en cualquier letra.

Practica hasta que se te quede
• Práctica guiada, de calcar a escribir de memoria, con puntos por la forma y el orden de los trazos.
• Tu propio vocabulario en listas como «Lección 3» o «Viaje», con exámenes de palabras: escribe cada una a partir de su significado, o de oído.
• Repasos, exámenes por nivel, hojas de práctica para imprimir, estadísticas y un álbum de todo lo que has escrito.

Lee lecciones de verdad y escribe encima
• Lecciones gratuitas incluidas: la guía de tailandés de Wikivoyage, con romanización, en inglés.
• Trae las tuyas desde tus archivos y fotos, o con el escáner de documentos de Google y escribe encima.
• Subraya, escribe en los márgenes, salta a una página, endereza un escaneo y compártelo todo como PDF con tus notas.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus páginas, palabras y progreso se quedan en tu tableta; Ajustes › Tus datos los exporta cuando quieras. La escritura y la traducción usan Google ML Kit en el dispositivo: el modelo de escritura se descarga la primera vez que abres Thai Learn!, cada modelo de traducción la primera vez que traduces, y mientras ML Kit está activado, Google recibe datos de diagnóstico anónimos. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Tras esas descargas, todo funciona sin conexión.

En español, inglés, portugués, tailandés y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (30/30): Thai Learn!: escreva tailandês

**Subtitle** (25/30): Cada letra, escrita à mão

**Keywords** (91/100): alfabeto,consoantes,vogais,tons,tailândia,vocabulário,tradutor,romanização,pencil,iniciante

**Promotional text** (148/170): Escreva tailandês à mão e o Thai Learn! lê. Veja cada letra sendo escrita, cubra o traçado e depois escreva de memória. Sem conta, funciona offline.

**Description** (3204/4000):

```text
Escreva tailandês à mão e o Thai Learn! lê. Veja cada letra sendo escrita, a partir do pequeno laço onde quase todas começam. Digitalize uma folha de exercícios e escreva nela.

O Thai Learn! é um caderno de tailandês para o seu iPad que entende o que você escreve.

O QUE ELE FAZ
• Lê a sua letra: letras e palavras tailandesas, com o Apple Pencil ou com o dedo.
• Traduz com o Google: o que você escreve, e do seu idioma para o tailandês.
• Transforma digitalizações, PDFs e fotos em lições para marcar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

ESCREVA, E O THAI LEARN! LÊ
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em tailandês e o Thai Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

TRADUZA NOS DOIS SENTIDOS
• Da sua página: circule o que você escreveu com o laço e leia no seu idioma.
• Para o tailandês: digite uma palavra ou uma frase no seu idioma, veja em tailandês e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu iPad com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O ALFABETO TAILANDÊS, TRAÇO A TRAÇO
• As 44 consoantes por classe, as vogais, os tons e sinais, e os algarismos tailandeses, cada um se escrevendo na ordem certa.
• Cada letra com seu nome, das básicas às mais raras.
• Palavras para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu iPad.
• Não acha uma letra? Desenhe, e o Thai Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

PRATIQUE ATÉ FIXAR
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

LEIA LIÇÕES DE VERDADE, E ESCREVA NELAS
• Lições grátis incluídas: o guia de conversação tailandês do Wikivoyage, com romanização, em português.
• Traga as suas do app Arquivos, de Fotos ou da câmera de documentos e escreva nelas.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu iPad; Configurações › Seus dados os exporta quando você quiser. A escrita e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Thai Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, tailandês e francês.
```

### Google Play

**Title** (30/30): Thai Learn!: escreva tailandês

**Short description** (78/80): Escreva tailandês à mão: cada letra traço a traço, lida enquanto você escreve.

**Full description** (3238/4000):

```text
Escreva tailandês à mão e o Thai Learn! lê. Veja cada letra sendo escrita, a partir do pequeno laço onde quase todas começam. Digitalize uma folha de exercícios e escreva nela.

O Thai Learn! é um caderno de tailandês para o seu tablet Android que entende o que você escreve.

O que ele faz
• Lê a sua letra: letras e palavras tailandesas, com uma caneta stylus ou com o dedo.
• Traduz com o Google: o que você escreve, e do seu idioma para o tailandês.
• Transforma digitalizações, PDFs e fotos em lições para marcar e escrever por cima.
• Mostra cada letra sendo escrita, com palavras e frases de exemplo.
• Agenda revisões das letras e das palavras, cada vez mais espaçadas quando você acerta.

Escreva, e o Thai Learn! lê
• Uma página infinita para notas e prática. Circule o que você escreveu com o laço para verificar, copiar como texto, salvar uma palavra ou traduzir.
• Cole um texto em tailandês e o Thai Learn! o escreve à mão na sua página.
• Cinco temas, do papel à noite.

Traduza nos dois sentidos
• Da sua página: circule o que você escreveu com o laço e leia no seu idioma.
• Para o tailandês: digite uma palavra ou uma frase no seu idioma, veja em tailandês e depois salve as palavras, escreva na página ou pratique seus letras.
• Tudo roda no seu tablet com o Google ML Kit: cada idioma é baixado uma vez e depois traduz offline.

O alfabeto tailandês, traço a traço
• As 44 consoantes por classe, as vogais, os tons e sinais, e os algarismos tailandeses, cada um se escrevendo na ordem certa.
• Cada letra com seu nome, das básicas às mais raras.
• Palavras para ler e ouvir, com romanização e significado, e frases de exemplo, lidas em voz alta pelas vozes do seu tablet.
• Não acha uma letra? Desenhe, e o Thai Learn! encontra.
• Parecidos: as letras que se confundem, começando pelas suas. E sua própria nota em qualquer letra.

Pratique até fixar
• Prática guiada, de decalcar a escrever de memória, com pontos pela forma e pela ordem dos traços.
• Seu próprio vocabulário em listas como “Lição 3” ou “Viagem”, com testes de palavras: escreva cada uma a partir do significado, ou de ouvido.
• Revisões, testes por nível, folhas de prática para imprimir, estatísticas e um álbum de tudo o que você escreveu.

Leia lições de verdade, e escreva nelas
• Lições grátis incluídas: o guia de conversação tailandês do Wikivoyage, com romanização, em português.
• Traga as suas dos seus arquivos e fotos, ou com o scanner de documentos do Google e escreva nelas.
• Destaque com o marca-texto, escreva nas margens, vá direto a uma página, endireite uma digitalização e compartilhe tudo como PDF com suas notas.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Suas páginas, palavras e progresso ficam no seu tablet; Configurações › Seus dados os exporta quando você quiser. A escrita e a tradução usam o Google ML Kit no dispositivo: o modelo de escrita é baixado na primeira vez que você abre o Thai Learn!, cada modelo de tradução na primeira vez que você traduz, e enquanto o ML Kit está ligado, o Google recebe dados de diagnóstico anônimos. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Depois desses downloads, tudo funciona offline.

Em português, inglês, espanhol, tailandês e francês.
```

## Thai (Google Play: th)

### App Store

**Name** (28/30): Thai Learn!: ฝึกเขียนภาษาไทย

**Subtitle** (19/30): ตัวอักษรไทยทีละเส้น

**Keywords** (87/100): ลายมือ,คัดลายมือ,พยัญชนะ,สระ,วรรณยุกต์,ก-ฮ,คำศัพท์,แปลภาษา,ทบทวน,ใบงาน,แบบฝึกหัด,เลขไทย

**Promotional text** (148/170): เขียนภาษาไทยด้วยลายมือ แล้ว Thai Learn! จะอ่านให้ ดูตัวอักษรแต่ละตัวเขียนทีละเส้น ลากตาม แล้วเขียนเองจากความจำ ไม่ต้องมีบัญชี และใช้งานแบบออฟไลน์ได้

**Description** (2892/4000):

```text
เขียนภาษาไทยด้วยลายมือ แล้ว Thai Learn! จะอ่านให้ ดูตัวอักษรทุกตัวเขียนทีละเส้น เริ่มจากหัวกลมเล็ก ๆ ที่ตัวอักษรส่วนใหญ่ใช้ขึ้นต้น สแกนใบงาน แล้วเขียนลงไปได้เลย

Thai Learn! คือสมุดภาษาไทยบน iPad ที่เข้าใจสิ่งที่คุณเขียน

■ ทำอะไรได้บ้าง
• อ่านลายมือ: ตัวอักษรและคำภาษาไทยที่เขียนด้วย Apple Pencil หรือนิ้ว
• แปลด้วย Google: แปลสิ่งที่คุณเขียนเป็นภาษาอังกฤษ และแปลภาษาอังกฤษเป็นภาษาไทย
• เปลี่ยนหน้าที่สแกน PDF และรูปภาพให้เป็นบทเรียนที่ไฮไลต์และเขียนลงไปได้
• แสดงตัวอักษรทุกตัวเขียนตามลำดับเส้น พร้อมคำและประโยคตัวอย่าง
• นำตัวอักษรและคำกลับมาให้ทบทวน โดยเว้นระยะนานขึ้นทุกครั้งที่เขียนถูก

■ เขียนแล้ว Thai Learn! อ่านให้
• หน้ากระดาษไม่มีที่สิ้นสุดสำหรับจดบันทึกและฝึกเขียน ใช้บ่วงบาศล้อมสิ่งที่เขียนเพื่อตรวจ คัดลอกเป็นข้อความ บันทึกคำ หรือแปล
• วางข้อความภาษาไทย แล้ว Thai Learn! จะเขียนเป็นลายมือลงบนหน้าให้
• ธีม 5 แบบ ตั้งแต่กระดาษไปจนถึงกลางคืน

■ แปลได้ทั้งสองทาง
• จากหน้าของคุณ: ใช้บ่วงบาศล้อมสิ่งที่เขียน แล้วอ่านเป็นภาษาอังกฤษ
• เป็นภาษาไทย: พิมพ์คำหรือประโยคภาษาอังกฤษ ได้ผลเป็นภาษาไทย แล้วบันทึกคำ เขียนลงหน้า หรือฝึกเขียนตัวอักษรในนั้น
• ทำงานบน iPad ด้วย Google ML Kit ดาวน์โหลดแต่ละภาษาเพียงครั้งเดียว แล้วแปลแบบออฟไลน์ได้
• เมื่อใช้แอปเป็นภาษาอังกฤษ สเปน โปรตุเกส หรือฝรั่งเศส การแปลจะเป็นระหว่างภาษานั้นกับภาษาไทย

■ ตัวอักษรไทย ทีละเส้น
• พยัญชนะครบ 44 ตัวแยกตามหมู่อักษร สระ วรรณยุกต์และเครื่องหมาย และเลขไทย ทุกตัวเขียนให้ดูตามลำดับเส้นที่ถูกต้อง
• ตัวอักษรทุกตัวมีชื่อเรียก ตั้งแต่ตัวหลักไปจนถึงตัวที่ใช้น้อย
• คำให้อ่านและฟัง พร้อมคำอ่านอักษรโรมันและความหมาย (ภาษาอังกฤษ) และประโยคตัวอย่าง อ่านออกเสียงด้วยเสียงของ iPad
• หาตัวอักษรไม่เจอ? เขียนลงไป แล้ว Thai Learn! จะหาให้
• ตัวที่คล้ายกัน: แสดงตัวอักษรที่มักสับสน โดยเริ่มจากตัวที่คุณเคยเขียนสลับ และจดบันทึกของคุณเองไว้กับตัวอักษรใดก็ได้

■ ฝึกจนจำได้
• การฝึกแบบมีคำแนะนำ ตั้งแต่ลากตามเส้นไปจนถึงเขียนเองจากความจำ ให้คะแนนทั้งรูปร่างและลำดับเส้น
• คลังคำศัพท์ของคุณเองในรายการอย่าง “บทที่ 3” หรือ “เที่ยว” พร้อมแบบทดสอบคำ: เขียนแต่ละคำจากความหมาย หรือจากการฟัง
• การทบทวน แบบทดสอบตามระดับ ใบฝึกสำหรับพิมพ์ สถิติ และอัลบั้มของทุกตัวที่คุณเคยเขียน

■ อ่านบทเรียนจริง แล้วเขียนลงไป
• มีบทเรียนฟรี: คู่มือสนทนาภาษาไทยจาก Wikivoyage (ฉบับภาษาอังกฤษ พร้อมคำอ่านอักษรโรมัน)
• นำบทเรียนของคุณเองเข้ามาจาก “ไฟล์” “รูปภาพ” หรือกล้องสแกนเอกสาร แล้วเขียนลงไป
• ไฮไลต์ เขียนที่ขอบกระดาษ ข้ามไปยังหน้าที่ต้องการ หมุนหน้าที่สแกนให้ตรง แล้วแชร์ทั้งหมดพร้อมบันทึกเป็น PDF

■ ของคุณ และเป็นส่วนตัว
ไม่มีบัญชีผู้ใช้ ไม่มีโฆษณา ไม่มีการติดตาม หน้า คำศัพท์ และความก้าวหน้าของคุณอยู่บน iPad เท่านั้น และส่งออกได้ทุกเมื่อที่ “การตั้งค่า › ข้อมูลของคุณ” การอ่านลายมือและการแปลใช้ Google ML Kit ที่ทำงานบนอุปกรณ์ โมเดลลายมือจะดาวน์โหลดเมื่อเปิด Thai Learn! ครั้งแรก และโมเดลการแปลจะดาวน์โหลดในครั้งแรกที่คุณแปล ขณะที่เปิดใช้ ML Kit อยู่ Google จะได้รับข้อมูลการวินิจฉัยแบบไม่ระบุตัวตน ปิดได้ที่ “การตั้งค่า › ลายมือ › อ่านด้วย Google ML Kit” หลังดาวน์โหลดแล้ว ทุกอย่างใช้งานแบบออฟไลน์ได้

รองรับภาษาไทย อังกฤษ สเปน โปรตุเกส และฝรั่งเศส
```

### Google Play

**Title** (28/30): Thai Learn!: ฝึกเขียนภาษาไทย

**Short description** (73/80): ฝึกเขียนภาษาไทยด้วยลายมือ ตัวอักษรทุกตัวทีละเส้น พร้อมอ่านสิ่งที่คุณเขียน

**Full description** (2924/4000):

```text
เขียนภาษาไทยด้วยลายมือ แล้ว Thai Learn! จะอ่านให้ ดูตัวอักษรทุกตัวเขียนทีละเส้น เริ่มจากหัวกลมเล็ก ๆ ที่ตัวอักษรส่วนใหญ่ใช้ขึ้นต้น สแกนใบงาน แล้วเขียนลงไปได้เลย

Thai Learn! คือสมุดภาษาไทยบนแท็บเล็ต Android ที่เข้าใจสิ่งที่คุณเขียน

■ ทำอะไรได้บ้าง
• อ่านลายมือ: ตัวอักษรและคำภาษาไทยที่เขียนด้วยปากกาสไตลัสหรือนิ้ว
• แปลด้วย Google: แปลสิ่งที่คุณเขียนเป็นภาษาอังกฤษ และแปลภาษาอังกฤษเป็นภาษาไทย
• เปลี่ยนหน้าที่สแกน PDF และรูปภาพให้เป็นบทเรียนที่ไฮไลต์และเขียนลงไปได้
• แสดงตัวอักษรทุกตัวเขียนตามลำดับเส้น พร้อมคำและประโยคตัวอย่าง
• นำตัวอักษรและคำกลับมาให้ทบทวน โดยเว้นระยะนานขึ้นทุกครั้งที่เขียนถูก

■ เขียนแล้ว Thai Learn! อ่านให้
• หน้ากระดาษไม่มีที่สิ้นสุดสำหรับจดบันทึกและฝึกเขียน ใช้บ่วงบาศล้อมสิ่งที่เขียนเพื่อตรวจ คัดลอกเป็นข้อความ บันทึกคำ หรือแปล
• วางข้อความภาษาไทย แล้ว Thai Learn! จะเขียนเป็นลายมือลงบนหน้าให้
• ธีม 5 แบบ ตั้งแต่กระดาษไปจนถึงกลางคืน

■ แปลได้ทั้งสองทาง
• จากหน้าของคุณ: ใช้บ่วงบาศล้อมสิ่งที่เขียน แล้วอ่านเป็นภาษาอังกฤษ
• เป็นภาษาไทย: พิมพ์คำหรือประโยคภาษาอังกฤษ ได้ผลเป็นภาษาไทย แล้วบันทึกคำ เขียนลงหน้า หรือฝึกเขียนตัวอักษรในนั้น
• ทำงานบนแท็บเล็ตด้วย Google ML Kit ดาวน์โหลดแต่ละภาษาเพียงครั้งเดียว แล้วแปลแบบออฟไลน์ได้
• เมื่อใช้แอปเป็นภาษาอังกฤษ สเปน โปรตุเกส หรือฝรั่งเศส การแปลจะเป็นระหว่างภาษานั้นกับภาษาไทย

■ ตัวอักษรไทย ทีละเส้น
• พยัญชนะครบ 44 ตัวแยกตามหมู่อักษร สระ วรรณยุกต์และเครื่องหมาย และเลขไทย ทุกตัวเขียนให้ดูตามลำดับเส้นที่ถูกต้อง
• ตัวอักษรทุกตัวมีชื่อเรียก ตั้งแต่ตัวหลักไปจนถึงตัวที่ใช้น้อย
• คำให้อ่านและฟัง พร้อมคำอ่านอักษรโรมันและความหมาย (ภาษาอังกฤษ) และประโยคตัวอย่าง อ่านออกเสียงด้วยเสียงของแท็บเล็ต
• หาตัวอักษรไม่เจอ? เขียนลงไป แล้ว Thai Learn! จะหาให้
• ตัวที่คล้ายกัน: แสดงตัวอักษรที่มักสับสน โดยเริ่มจากตัวที่คุณเคยเขียนสลับ และจดบันทึกของคุณเองไว้กับตัวอักษรใดก็ได้

■ ฝึกจนจำได้
• การฝึกแบบมีคำแนะนำ ตั้งแต่ลากตามเส้นไปจนถึงเขียนเองจากความจำ ให้คะแนนทั้งรูปร่างและลำดับเส้น
• คลังคำศัพท์ของคุณเองในรายการอย่าง “บทที่ 3” หรือ “เที่ยว” พร้อมแบบทดสอบคำ: เขียนแต่ละคำจากความหมาย หรือจากการฟัง
• การทบทวน แบบทดสอบตามระดับ ใบฝึกสำหรับพิมพ์ สถิติ และอัลบั้มของทุกตัวที่คุณเคยเขียน

■ อ่านบทเรียนจริง แล้วเขียนลงไป
• มีบทเรียนฟรี: คู่มือสนทนาภาษาไทยจาก Wikivoyage (ฉบับภาษาอังกฤษ พร้อมคำอ่านอักษรโรมัน)
• นำบทเรียนของคุณเองเข้ามาจากไฟล์และรูปภาพ หรือสแกนด้วยเครื่องสแกนเอกสารของ Google แล้วเขียนลงไป
• ไฮไลต์ เขียนที่ขอบกระดาษ ข้ามไปยังหน้าที่ต้องการ หมุนหน้าที่สแกนให้ตรง แล้วแชร์ทั้งหมดพร้อมบันทึกเป็น PDF

■ ของคุณ และเป็นส่วนตัว
ไม่มีบัญชีผู้ใช้ ไม่มีโฆษณา ไม่มีการติดตาม หน้า คำศัพท์ และความก้าวหน้าของคุณอยู่บนแท็บเล็ตเท่านั้น และส่งออกได้ทุกเมื่อที่ “การตั้งค่า › ข้อมูลของคุณ” การอ่านลายมือและการแปลใช้ Google ML Kit ที่ทำงานบนอุปกรณ์ โมเดลลายมือจะดาวน์โหลดเมื่อเปิด Thai Learn! ครั้งแรก และโมเดลการแปลจะดาวน์โหลดในครั้งแรกที่คุณแปล ขณะที่เปิดใช้ ML Kit อยู่ Google จะได้รับข้อมูลการวินิจฉัยแบบไม่ระบุตัวตน ปิดได้ที่ “การตั้งค่า › ลายมือ › อ่านด้วย Google ML Kit” หลังดาวน์โหลดแล้ว ทุกอย่างใช้งานแบบออฟไลน์ได้

รองรับภาษาไทย อังกฤษ สเปน โปรตุเกส และฝรั่งเศส
```

## French (Google Play: fr-FR)

### App Store

**Name** (27/30): Thai Learn! L’alphabet thaï

**Subtitle** (24/30): Chaque lettre, à la main

**Keywords** (94/100): écriture,consonnes,voyelles,tons,thaïlande,vocabulaire,traduction,romanisation,pencil,débutant

**Promotional text** (156/170): Écrivez le thaï à la main, Thai Learn! le lit. Regardez chaque lettre s’écrire, repassez-la, puis écrivez-la de mémoire. Sans compte, fonctionne hors ligne.

**Description** (3438/4000):

```text
Écrivez le thaï à la main, Thai Learn! le lit. Regardez chaque lettre s’écrire, depuis la petite boucle par laquelle presque toutes commencent. Numérisez une fiche d’exercices, et écrivez dessus.

Thai Learn! est un cahier de thaï pour votre iPad qui comprend ce que vous écrivez.

CE QUE FAIT THAI LEARN!
• Lit votre écriture : lettres et mots thaïs, à l’Apple Pencil ou du doigt.
• Traduit avec Google : ce que vous écrivez, et de votre langue vers le thaï.
• Fait de vos numérisations, PDF et photos des leçons à surligner et à annoter.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

ÉCRIVEZ, THAI LEARN! LIT
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en thaï et Thai Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

TRADUISEZ, DANS LES DEUX SENS
• Depuis votre page : entourez ce que vous avez écrit au lasso et lisez-le dans votre langue.
• Vers le thaï : tapez un mot ou une phrase dans votre langue, obtenez-le en thaï, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre iPad avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

L’ALPHABET THAÏ, TRAIT PAR TRAIT
• Les 44 consonnes par classe, les voyelles, les tons et les signes, et les chiffres thaïs, chacun s’écrivant dans l’ordre.
• Chaque lettre avec son nom, des lettres de base aux plus rares.
• Des mots à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre iPad.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Thai Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

ENTRAÎNEZ-VOUS JUSQU’À RETENIR
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

LISEZ DE VRAIES LEÇONS, ET ÉCRIVEZ DESSUS
• Leçons gratuites incluses : le guide de conversation thaï de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis Fichiers, Photos ou l’appareil photo de documents, et écrivez dessus.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre iPad ; Réglages › Vos données les exporte quand vous voulez. L’écriture et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Thai Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et thaï.
```

### Google Play

**Title** (27/30): Thai Learn! L’alphabet thaï

**Short description** (74/80): Apprenez à écrire le thaï à la main, lettre par lettre et trait par trait.

**Full description** (3475/4000):

```text
Écrivez le thaï à la main, Thai Learn! le lit. Regardez chaque lettre s’écrire, depuis la petite boucle par laquelle presque toutes commencent. Numérisez une fiche d’exercices, et écrivez dessus.

Thai Learn! est un cahier de thaï pour votre tablette Android qui comprend ce que vous écrivez.

Ce que fait Thai Learn!
• Lit votre écriture : lettres et mots thaïs, au stylet ou du doigt.
• Traduit avec Google : ce que vous écrivez, et de votre langue vers le thaï.
• Fait de vos numérisations, PDF et photos des leçons à surligner et à annoter.
• Montre chaque lettre s’écrire, avec des mots et des phrases d’exemple.
• Vous ramène les lettres et les mots en révision, à des intervalles de plus en plus longs quand vous les réussissez.

Écrivez, Thai Learn! lit
• Une page infinie pour vos notes et votre entraînement. Entourez ce que vous avez écrit au lasso pour le vérifier, le copier en texte, enregistrer un mot ou le traduire.
• Collez un texte en thaï et Thai Learn! l’écrit à la main sur votre page.
• Cinq thèmes, du papier à la nuit.

Traduisez, dans les deux sens
• Depuis votre page : entourez ce que vous avez écrit au lasso et lisez-le dans votre langue.
• Vers le thaï : tapez un mot ou une phrase dans votre langue, obtenez-le en thaï, puis enregistrez ses mots, écrivez-le sur la page ou entraînez-vous sur ses lettres.
• Tout fonctionne sur votre tablette avec Google ML Kit : chaque langue se télécharge une fois, puis traduit hors ligne.

L’alphabet thaï, trait par trait
• Les 44 consonnes par classe, les voyelles, les tons et les signes, et les chiffres thaïs, chacun s’écrivant dans l’ordre.
• Chaque lettre avec son nom, des lettres de base aux plus rares.
• Des mots à lire et à écouter, avec leur romanisation et leur sens, et des phrases d’exemple, lus à voix haute par les voix de votre tablette.
• Vous ne trouvez pas une lettre ? Dessinez-la, et Thai Learn! la trouve.
• Ressemblances : les lettres que l’on confond, les vôtres d’abord. Et votre propre note sur n’importe quelle lettre.

Entraînez-vous jusqu’à retenir
• Un entraînement guidé, du calque à l’écriture de mémoire, noté sur la forme et l’ordre des traits.
• Votre vocabulaire dans des listes comme « Leçon 3 » ou « Voyage », avec des examens de mots : écrivez chaque mot à partir de son sens, ou à l’oreille.
• Révisions, examens par niveau, feuilles d’entraînement à imprimer, statistiques et un album de tout ce que vous avez écrit.

Lisez de vraies leçons, et écrivez dessus
• Leçons gratuites incluses : le guide de conversation thaï de Wikivoyage, avec la romanisation, en français.
• Ajoutez les vôtres depuis vos fichiers et vos photos, ou avec le scanner de documents de Google, et écrivez dessus.
• Surlignez, écrivez dans les marges, allez à une page, redressez une page numérisée et partagez le tout en PDF avec vos notes.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos pages, vos mots et vos progrès restent sur votre tablette ; Réglages › Vos données les exporte quand vous voulez. L’écriture et la traduction utilisent Google ML Kit sur l’appareil : le modèle d’écriture se télécharge au premier lancement de Thai Learn!, chaque modèle de traduction la première fois que vous traduisez, et tant que ML Kit est activé, Google reçoit des données de diagnostic anonymes. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Après ces téléchargements, tout fonctionne hors ligne.

En français, anglais, espagnol, portugais et thaï.
```
