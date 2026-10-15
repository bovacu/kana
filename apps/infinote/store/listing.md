# InfiNote: App Store and Google Play texts

InfiNote’s texts for the App Store and Google Play, in its four languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language) and its product profile (`fude/zoom/product.c`: the topics, Insert entries, instruments and exports it shows); the examples are the 65 of `fude/zoom/page_custom.c`.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Productivity (primary), Education (secondary). Google Play: Productivity.
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). docs/product_split.md §9 says “paid once” (2026-10-11); docs/release_checklist.md §2 still lists the price as yours to set.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/infinote/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/infinote/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/infinote/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
InfiNote needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: InfiNote starts with the hand on the toolbar on, so one finger draws and two fingers move and zoom the canvas. The first Apple Pencil stroke switches to Pencil drawing (fingers then move the canvas); tapping the hand switches back.

To try the main features:
1. On the home screen, New notebook: write on the page, pinch to zoom in anywhere and keep writing. Pages, on the toolbar just before Paper, changes the page size or turns the notebook into one endless canvas.
2. Tap the microphone on the toolbar (Record) and write while someone speaks; stop, then tap the writing to hear what was said at that moment. Recordings lists them, to play, Replay ink or delete.
3. Lasso some handwriting and choose To text. Find searches typed text and handwriting, in this canvas and in the others.
4. Insert › PDF opens a PDF as a canvas to write on; Insert › Photos or Files adds pictures; Export makes an image, an SVG, a PDF, a 1:1 print, a zoom video or the named areas as a PDF, and hands it to the share sheet.
5. Topic, on the toolbar, switches the canvas’s tools: Notes, PDF study, Maths notebook (Insert › Graphs) and Diagrams (Insert › Diagram from text, Kanban board). Present shows the marked places and named areas one by one.

Network and downloads: nothing is downloaded when the app starts. Google ML Kit’s handwriting model (about 20 MB) downloads the first time To text or Find reads handwriting, or when Download is tapped in Settings › Handwriting, so that first use needs the internet. ML Kit’s switch is the menu › Settings › Handwriting › Read with Google ML Kit (on by default); off, To text is hidden, Find reads no handwriting, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: the microphone is asked for only at the first tap on Record, and recording stops when the canvas or the app is left. The camera is never used. Insert › Photos and Files use the system’s pickers, which need no permission; saving an exported image or video to Photos through the share sheet asks for add-only access. No location, contacts or tracking.

Speech to text (Transcribe, Live text) exists only in the Android app; the iPad app has none, and its buttons are hidden.

Links open only when tapped: a link written in a note opens in Safari; FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | fr |
|---|---|---|---|---|
| App Store name (30) | 27 | 24 | 25 | 28 |
| App Store subtitle (30) | 28 | 28 | 29 | 27 |
| App Store keywords (100) | 95 | 99 | 97 | 97 |
| App Store promotional text (170) | 163 | 157 | 157 | 162 |
| App Store description (4000) | 2050 | 2200 | 2201 | 2436 |
| Google Play title (30) | 27 | 24 | 25 | 28 |
| Google Play short description (80) | 68 | 67 | 62 | 73 |
| Google Play full description (4000) | 2181 | 2345 | 2344 | 2597 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (27/30): InfiNote: Handwritten Notes

**Subtitle** (28/30): Pages, PDFs and endless zoom

**Keywords** (95/100): notebook,annotate,pencil,lecture,recording,whiteboard,canvas,diagram,kanban,graph,journal,study

**Promotional text** (163/170): Write and draw on pages, or on a canvas you can zoom into without end. Record a lecture and tap your notes to hear it again. Write on your PDFs. No account needed.

**Description** (2050/4000):

```text
InfiNote is a notebook for your iPad where every page zooms in without end. Write and draw with the Apple Pencil or your finger, mark up your PDFs, and record what is said while you write.

ROOM FOR EVERYTHING
• Notebooks of A4, A5, Letter or square pages, with a new page whenever you need one, or one endless canvas without edges.
• Zoom into any corner for more room, then back out to see the whole.
• Paper to suit the work: lines, squares, dots, graph, isometric, hexagons, polar, music, and Cornell and to-do pages.
• Pens with pressure, a marker, shapes, fill, layers, text and sticky notes.

RECORD AND WRITE
• Record a lecture or a meeting as you write: later, tap any line to hear what was being said when you wrote it.
• Replay the ink as it was written.

HANDWRITING YOU CAN SEARCH
• Lasso your handwriting and turn it into text.
• Find looks for words in your typed text and in your handwriting, in this notebook and in all the others.

PDFS, MATH AND DIAGRAMS
• Open a PDF as a canvas of its own and write all over it. Bring in photos and files too.
• Graphs of equations and inequalities, with roots, turning points and crossings marked.
• Ruler, set squares, protractor, compass, circle and ellipse templates, French curve and tape measure.
• Diagrams from shapes and connectors, or written as text, and Kanban boards.
• Present: mark places and named areas, then show them one after another, with the pen as a laser pointer.

SHARE AND PRINT
• Export an image, an SVG drawing, a PDF, a 1:1 print on A4 or Letter, a zoom video, or your named areas as a PDF.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your notebooks and recordings stay on your iPad; Settings › Your data exports everything into one file. Reading handwriting (To text, Find) uses Google ML Kit on the device: its model downloads the first time you use it, and Google receives anonymous diagnostics when it is used. To switch it off: Settings › Handwriting › Read with Google ML Kit. Everything else works offline.

In English, Spanish, Portuguese and French.
```

### Google Play

**Title** (27/30): InfiNote: Handwritten Notes

**Short description** (68/80): A notebook that zooms without end: handwriting, PDFs and recordings.

**Full description** (2181/4000):

```text
InfiNote is a notebook for your Android tablet where every page zooms in without end. Write and draw with a stylus or your finger, mark up your PDFs, and record what is said while you write.

Room for everything
• Notebooks of A4, A5, Letter or square pages, with a new page whenever you need one, or one endless canvas without edges.
• Zoom into any corner for more room, then back out to see the whole.
• Paper to suit the work: lines, squares, dots, graph, isometric, hexagons, polar, music, and Cornell and to-do pages.
• Pens with pressure, a marker, shapes, fill, layers, text and sticky notes.

Record and write
• Record a lecture or a meeting as you write: later, tap any line to hear what was being said when you wrote it.
• Replay the ink as it was written.
• Turn a recording into text on your tablet, after a one-time download of the speech model that InfiNote asks you about first.

Handwriting you can search
• Lasso your handwriting and turn it into text.
• Find looks for words in your typed text and in your handwriting, in this notebook and in all the others.

PDFs, math and diagrams
• Open a PDF as a canvas of its own and write all over it. Bring in photos and files too.
• Graphs of equations and inequalities, with roots, turning points and crossings marked.
• Ruler, set squares, protractor, compass, circle and ellipse templates, French curve and tape measure.
• Diagrams from shapes and connectors, or written as text, and Kanban boards.
• Present: mark places and named areas, then show them one after another, with the pen as a laser pointer.

Share and print
• Export an image, an SVG drawing, a PDF, a 1:1 print on A4 or Letter, a zoom video, or your named areas as a PDF.

Yours, and private
No account, no ads, no tracking. Your notebooks and recordings stay on your tablet; Settings › Your data exports everything into one file. Reading handwriting (To text, Find) uses Google ML Kit on the device: its model downloads the first time you use it, and Google receives anonymous diagnostics when it is used. To switch it off: Settings › Handwriting › Read with Google ML Kit. Everything else works offline.

In English, Spanish, Portuguese and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (24/30): InfiNote: apuntes a mano

**Subtitle** (28/30): Páginas, PDF y zoom infinito

**Keywords** (99/100): cuaderno,libreta,notas,anotar,pencil,clase,grabación,pizarra,lienzo,diagrama,kanban,gráficas,agenda

**Promotional text** (157/170): Escribe y dibuja en páginas o en un lienzo en el que puedes acercarte sin fin. Graba una clase y toca tus apuntes para volver a oírla. Escribe sobre tus PDF.

**Description** (2200/4000):

```text
InfiNote es un cuaderno para tu iPad en el que cada página se amplía sin fin. Escribe y dibuja con el Apple Pencil o con el dedo, anota tus PDF y graba lo que se dice mientras escribes.

SITIO PARA TODO
• Cuadernos de páginas A4, A5, carta o cuadradas, con una página nueva cada vez que la necesitas, o un único lienzo sin bordes.
• Acércate a cualquier rincón para tener más sitio y aléjate para verlo todo.
• Papel para cada tarea: rayado, cuadriculado, de puntos, milimetrado, isométrico, hexagonal, polar, pentagrama, y páginas Cornell y de tareas.
• Plumas con presión, marcador, formas, relleno, capas, textos y notas adhesivas.

GRABA Y ESCRIBE
• Graba una clase o una reunión mientras escribes: después, toca cualquier línea para oír lo que se decía cuando la escribiste.
• Vuelve a ver la tinta tal como se escribió.

ESCRITURA A MANO EN LA QUE BUSCAR
• Rodea tu escritura con el lazo y conviértela en texto.
• Buscar encuentra palabras en tus textos y en tu escritura a mano, en este cuaderno y en todos los demás.

PDF, MATEMÁTICAS Y DIAGRAMAS
• Abre un PDF como un lienzo propio y escribe por todas partes. Trae también fotos y archivos.
• Gráficas de ecuaciones e inecuaciones, con raíces, máximos, mínimos y cortes marcados.
• Regla, escuadra y cartabón, transportador, compás, plantillas de círculos y elipses, plantilla de curvas y cinta métrica.
• Diagramas con formas y conectores, o escritos como texto, y tableros Kanban.
• Presentar: marca lugares y zonas con nombre y muéstralos uno tras otro, con el lápiz como puntero láser.

COMPARTE E IMPRIME
• Exporta una imagen, un dibujo SVG, un PDF, una impresión 1:1 en A4 o carta, un vídeo del zoom o tus zonas con nombre en PDF.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus cuadernos y grabaciones se quedan en tu iPad; Ajustes › Tus datos lo exporta todo en un archivo. La lectura de la escritura a mano (A texto, Buscar) usa Google ML Kit en el dispositivo: su modelo se descarga la primera vez que la usas, y Google recibe datos de diagnóstico anónimos cuando se usa. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Todo lo demás funciona sin conexión.

En español, inglés, portugués y francés.
```

### Google Play

**Title** (24/30): InfiNote: apuntes a mano

**Short description** (67/80): Un cuaderno con zoom infinito: escritura a mano, PDF y grabaciones.

**Full description** (2345/4000):

```text
InfiNote es un cuaderno para tu tableta Android en el que cada página se amplía sin fin. Escribe y dibuja con un lápiz óptico o con el dedo, anota tus PDF y graba lo que se dice mientras escribes.

Sitio para todo
• Cuadernos de páginas A4, A5, carta o cuadradas, con una página nueva cada vez que la necesitas, o un único lienzo sin bordes.
• Acércate a cualquier rincón para tener más sitio y aléjate para verlo todo.
• Papel para cada tarea: rayado, cuadriculado, de puntos, milimetrado, isométrico, hexagonal, polar, pentagrama, y páginas Cornell y de tareas.
• Plumas con presión, marcador, formas, relleno, capas, textos y notas adhesivas.

Graba y escribe
• Graba una clase o una reunión mientras escribes: después, toca cualquier línea para oír lo que se decía cuando la escribiste.
• Vuelve a ver la tinta tal como se escribió.
• Pasa una grabación a texto en tu tableta, tras descargar una sola vez el modelo de voz, sobre lo que InfiNote te pregunta antes.

Escritura a mano en la que buscar
• Rodea tu escritura con el lazo y conviértela en texto.
• Buscar encuentra palabras en tus textos y en tu escritura a mano, en este cuaderno y en todos los demás.

PDF, matemáticas y diagramas
• Abre un PDF como un lienzo propio y escribe por todas partes. Trae también fotos y archivos.
• Gráficas de ecuaciones e inecuaciones, con raíces, máximos, mínimos y cortes marcados.
• Regla, escuadra y cartabón, transportador, compás, plantillas de círculos y elipses, plantilla de curvas y cinta métrica.
• Diagramas con formas y conectores, o escritos como texto, y tableros Kanban.
• Presentar: marca lugares y zonas con nombre y muéstralos uno tras otro, con el lápiz como puntero láser.

Comparte e imprime
• Exporta una imagen, un dibujo SVG, un PDF, una impresión 1:1 en A4 o carta, un vídeo del zoom o tus zonas con nombre en PDF.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus cuadernos y grabaciones se quedan en tu tableta; Ajustes › Tus datos lo exporta todo en un archivo. La lectura de la escritura a mano (A texto, Buscar) usa Google ML Kit en el dispositivo: su modelo se descarga la primera vez que la usas, y Google recibe datos de diagnóstico anónimos cuando se usa. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Todo lo demás funciona sin conexión.

En español, inglés, portugués y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (25/30): InfiNote: anotações à mão

**Subtitle** (29/30): Páginas, PDFs e zoom infinito

**Keywords** (97/100): caderno,notas,anotar,pencil,aula,gravação,áudio,quadro,tela,diagrama,kanban,gráfico,agenda,estudo

**Promotional text** (157/170): Escreva e desenhe em páginas ou numa tela que você pode aproximar sem fim. Grave uma aula e toque nas suas notas para ouvi-la de novo. Escreva nos seus PDFs.

**Description** (2201/4000):

```text
O InfiNote é um caderno para o seu iPad em que cada página se aproxima sem fim. Escreva e desenhe com o Apple Pencil ou com o dedo, faça anotações nos seus PDFs e grave o que é dito enquanto você escreve.

ESPAÇO PARA TUDO
• Cadernos de páginas A4, A5, Carta ou quadradas, com uma página nova sempre que você precisar, ou uma única tela sem bordas.
• Aproxime qualquer canto para ter mais espaço e afaste para ver o todo.
• Papel para cada trabalho: pautado, quadriculado, pontilhado, milimetrado, isométrico, hexagonal, polar, pauta musical, e páginas Cornell e de tarefas.
• Canetas com pressão, marca-texto, formas, preenchimento, camadas, textos e notas adesivas.

GRAVE E ESCREVA
• Grave uma aula ou uma reunião enquanto escreve: depois, toque em qualquer linha para ouvir o que era dito quando você a escreveu.
• Reveja a tinta sendo escrita.

ESCRITA À MÃO QUE DÁ PARA BUSCAR
• Selecione sua escrita com o laço e transforme-a em texto.
• Buscar encontra palavras nos seus textos e na sua escrita à mão, neste caderno e em todos os outros.

PDFS, MATEMÁTICA E DIAGRAMAS
• Abra um PDF como uma tela própria e escreva por toda parte. Traga também fotos e arquivos.
• Gráficos de equações e inequações, com raízes, máximos, mínimos e interseções marcados.
• Régua, esquadros, transferidor, compasso, gabaritos de círculos e elipses, curva francesa e trena.
• Diagramas com formas e conectores, ou escritos como texto, e quadros Kanban.
• Apresentar: marque lugares e áreas com nome e mostre um depois do outro, com a caneta como apontador laser.

COMPARTILHE E IMPRIMA
• Exporte uma imagem, um desenho SVG, um PDF, uma impressão 1:1 em A4 ou Carta, um vídeo de zoom ou suas áreas com nome em PDF.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Seus cadernos e gravações ficam no seu iPad; Configurações › Seus dados exporta tudo num arquivo. A leitura da escrita à mão (Para texto, Buscar) usa o Google ML Kit no dispositivo: o modelo é baixado na primeira vez que você a usa, e o Google recebe dados de diagnóstico anônimos quando ela é usada. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Todo o resto funciona offline.

Em português, inglês, espanhol e francês.
```

### Google Play

**Title** (25/30): InfiNote: anotações à mão

**Short description** (62/80): Um caderno com zoom infinito: escrita à mão, PDFs e gravações.

**Full description** (2344/4000):

```text
O InfiNote é um caderno para o seu tablet Android em que cada página se aproxima sem fim. Escreva e desenhe com uma caneta ou com o dedo, faça anotações nos seus PDFs e grave o que é dito enquanto você escreve.

Espaço para tudo
• Cadernos de páginas A4, A5, Carta ou quadradas, com uma página nova sempre que você precisar, ou uma única tela sem bordas.
• Aproxime qualquer canto para ter mais espaço e afaste para ver o todo.
• Papel para cada trabalho: pautado, quadriculado, pontilhado, milimetrado, isométrico, hexagonal, polar, pauta musical, e páginas Cornell e de tarefas.
• Canetas com pressão, marca-texto, formas, preenchimento, camadas, textos e notas adesivas.

Grave e escreva
• Grave uma aula ou uma reunião enquanto escreve: depois, toque em qualquer linha para ouvir o que era dito quando você a escreveu.
• Reveja a tinta sendo escrita.
• Transforme uma gravação em texto no próprio tablet, depois de baixar uma única vez o modelo de voz, o que o InfiNote pergunta antes.

Escrita à mão que dá para buscar
• Selecione sua escrita com o laço e transforme-a em texto.
• Buscar encontra palavras nos seus textos e na sua escrita à mão, neste caderno e em todos os outros.

PDFs, matemática e diagramas
• Abra um PDF como uma tela própria e escreva por toda parte. Traga também fotos e arquivos.
• Gráficos de equações e inequações, com raízes, máximos, mínimos e interseções marcados.
• Régua, esquadros, transferidor, compasso, gabaritos de círculos e elipses, curva francesa e trena.
• Diagramas com formas e conectores, ou escritos como texto, e quadros Kanban.
• Apresentar: marque lugares e áreas com nome e mostre um depois do outro, com a caneta como apontador laser.

Compartilhe e imprima
• Exporte uma imagem, um desenho SVG, um PDF, uma impressão 1:1 em A4 ou Carta, um vídeo de zoom ou suas áreas com nome em PDF.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Seus cadernos e gravações ficam no seu tablet; Configurações › Seus dados exporta tudo num arquivo. A leitura da escrita à mão (Para texto, Buscar) usa o Google ML Kit no dispositivo: o modelo é baixado na primeira vez que você a usa, e o Google recebe dados de diagnóstico anônimos quando ela é usada. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Todo o resto funciona offline.

Em português, inglês, espanhol e francês.
```

## French (Google Play: fr-FR)

### App Store

**Name** (28/30): InfiNote : notes manuscrites

**Subtitle** (27/30): Pages, PDF et zoom sans fin

**Keywords** (97/100): cahier,carnet,annoter,stylet,pencil,cours,enregistrement,tableau,toile,diagramme,kanban,graphique

**Promotional text** (162/170): Écrivez et dessinez sur des pages ou sur une toile où zoomer sans fin. Enregistrez un cours et touchez vos notes pour le réentendre. Annotez vos PDF. Sans compte.

**Description** (2436/4000):

```text
InfiNote est un carnet pour votre iPad où chaque page se zoome sans fin. Écrivez et dessinez à l’Apple Pencil ou du doigt, annotez vos PDF et enregistrez ce qui se dit pendant que vous écrivez.

DE LA PLACE POUR TOUT
• Des cahiers de pages A4, A5, Letter ou carrées, avec une nouvelle page dès que vous en avez besoin, ou une seule toile sans bords.
• Zoomez dans n’importe quel coin pour avoir plus de place, puis dézoomez pour voir l’ensemble.
• Un papier pour chaque usage : lignes, carreaux, points, millimétré, isométrique, hexagones, polaire, portées, et pages Cornell et de tâches.
• Stylos sensibles à la pression, surligneur, formes, remplissage, calques, textes et notes adhésives.

ENREGISTREZ ET ÉCRIVEZ
• Enregistrez un cours ou une réunion pendant que vous écrivez : plus tard, touchez n’importe quelle ligne pour entendre ce qui se disait quand vous l’avez écrite.
• Rejouez l’encre telle qu’elle a été écrite.

UNE ÉCRITURE MANUSCRITE OÙ CHERCHER
• Entourez votre écriture au lasso et transformez-la en texte.
• Rechercher trouve des mots dans vos textes et dans votre écriture manuscrite, dans ce cahier et dans tous les autres.

PDF, MATHS ET DIAGRAMMES
• Ouvrez un PDF comme une toile à part et écrivez partout dessus. Importez aussi des photos et des fichiers.
• Des graphiques d’équations et d’inéquations, avec racines, extremums et intersections marqués.
• Règle, équerres, rapporteur, compas, gabarits de cercles et d’ellipses, pistolet et mètre ruban.
• Des diagrammes faits de formes et de connecteurs, ou écrits sous forme de texte, et des tableaux Kanban.
• Présenter : marquez des lieux et des zones nommées, puis montrez-les l’un après l’autre, le stylet en guise de pointeur laser.

PARTAGEZ ET IMPRIMEZ
• Exportez une image, un dessin SVG, un PDF, une impression 1:1 en A4 ou Letter, une vidéo de zoom ou vos zones nommées en PDF.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos cahiers et enregistrements restent sur votre iPad ; Réglages › Vos données exporte tout dans un seul fichier. La lecture de l’écriture manuscrite (En texte, Rechercher) utilise Google ML Kit sur l’appareil : son modèle se télécharge la première fois que vous vous en servez, et Google reçoit des données de diagnostic anonymes quand elle est utilisée. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Tout le reste fonctionne hors ligne.

En français, anglais, espagnol et portugais.
```

### Google Play

**Title** (28/30): InfiNote : notes manuscrites

**Short description** (73/80): Un carnet au zoom sans fin : écriture manuscrite, PDF et enregistrements.

**Full description** (2597/4000):

```text
InfiNote est un carnet pour votre tablette Android où chaque page se zoome sans fin. Écrivez et dessinez au stylet ou du doigt, annotez vos PDF et enregistrez ce qui se dit pendant que vous écrivez.

De la place pour tout
• Des cahiers de pages A4, A5, Letter ou carrées, avec une nouvelle page dès que vous en avez besoin, ou une seule toile sans bords.
• Zoomez dans n’importe quel coin pour avoir plus de place, puis dézoomez pour voir l’ensemble.
• Un papier pour chaque usage : lignes, carreaux, points, millimétré, isométrique, hexagones, polaire, portées, et pages Cornell et de tâches.
• Stylos sensibles à la pression, surligneur, formes, remplissage, calques, textes et notes adhésives.

Enregistrez et écrivez
• Enregistrez un cours ou une réunion pendant que vous écrivez : plus tard, touchez n’importe quelle ligne pour entendre ce qui se disait quand vous l’avez écrite.
• Rejouez l’encre telle qu’elle a été écrite.
• Transformez un enregistrement en texte sur votre tablette, après un téléchargement unique du modèle vocal, que l’app vous demande d’abord d’accepter.

Une écriture manuscrite où chercher
• Entourez votre écriture au lasso et transformez-la en texte.
• Rechercher trouve des mots dans vos textes et dans votre écriture manuscrite, dans ce cahier et dans tous les autres.

PDF, maths et diagrammes
• Ouvrez un PDF comme une toile à part et écrivez partout dessus. Importez aussi des photos et des fichiers.
• Des graphiques d’équations et d’inéquations, avec racines, extremums et intersections marqués.
• Règle, équerres, rapporteur, compas, gabarits de cercles et d’ellipses, pistolet et mètre ruban.
• Des diagrammes faits de formes et de connecteurs, ou écrits sous forme de texte, et des tableaux Kanban.
• Présenter : marquez des lieux et des zones nommées, puis montrez-les l’un après l’autre, le stylet en guise de pointeur laser.

Partagez et imprimez
• Exportez une image, un dessin SVG, un PDF, une impression 1:1 en A4 ou Letter, une vidéo de zoom ou vos zones nommées en PDF.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos cahiers et enregistrements restent sur votre tablette ; Réglages › Vos données exporte tout dans un seul fichier. La lecture de l’écriture manuscrite (En texte, Rechercher) utilise Google ML Kit sur l’appareil : son modèle se télécharge la première fois que vous vous en servez, et Google reçoit des données de diagnostic anonymes quand elle est utilisée. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Tout le reste fonctionne hors ligne.

En français, anglais, espagnol et portugais.
```
