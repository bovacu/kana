# InfiniWorkshop: App Store and Google Play texts

InfiniWorkshop’s texts for the App Store and Google Play, in its four languages.

Written 2026-10-13 from the facts in `tools/store/site.py` (FACTS), the privacy and help pages it made in `../site/`, the app’s own strings (`../assets/text/strings.rdel`: every menu name here is the app’s label in that language) and its product profile (`fude/zoom/product.c`: the topics, Insert entries, instruments and exports it shows); the examples are the 65 of `fude/zoom/page_custom.c`.

Every field is pasted as it is. In the descriptions, lines starting with “•” are bullets and lines in capitals are section headings (Google Play gets the same headings in sentence case, and Android wording: tablet, stylus, Google’s document scanner). Both stores’ texts are checked by a script for their length, the keywords rules (comma-separated, no space after a comma, no word of the name or subtitle repeated), no ASCII apostrophe, no ellipsis, and no other platform named. Lengths are in characters, as both stores count them.

## Shared (every language)

- **Category:** App Store: Productivity (primary), Education (secondary). Google Play: Productivity (or Education: your call).
- **Price: open.** None of these texts names a price or says “one purchase”, so they fit whatever you decide. Decide it per app (free, paid once, or in-app purchases). docs/product_split.md §9 says “paid once” (2026-10-11); docs/release_checklist.md §2 still lists the price as yours to set.
- **Age rating and the privacy answers:** `docs/store_privacy_answers.md` (App Privacy, Data safety, Apple’s age rating and IARC).
- **Copyright:** `2026 Borja Vazquez Cuesta` (or the seller name in App Store Connect).
- **Devices:** App Store: iPad only. Google Play: Android tablets (phones: the open decision in `docs/release_checklist.md` §2).
- **Support URL** (App Store) and **Website** (Google Play): `https://<user>.github.io/<repo>/infiniworkshop/`
- **Privacy Policy URL** (both stores): `https://<user>.github.io/<repo>/infiniworkshop/privacy.html`
  (placeholders until the pages are hosted: `python3 tools/store/site.py --pages DIR`; the pages are `apps/infiniworkshop/site/`.)
- **Contact e-mail** (Google Play, and App Review): `rde.apps.support@gmail.com`.
- **Google Play › App content:** Ads: no. App access: “All functionality is available without any access restrictions” (no account, no sign-in).
- **Localizations:** English (U.S.) = Play `en-US`, Spanish (Spain) = Play `es-ES`, Portuguese (Brazil) = Play `pt-BR`, French = Play `fr-FR`. Optionally also Spanish (Mexico) / Play `es-419` and French (Canada) / `fr-CA` with the same texts.

## App Review notes (English)

App Store Connect › App Review Information › Notes. Sign-in required: no.

```text
InfiniWorkshop needs no account and has no sign-in, ads or in-app purchases; every feature works on a fresh install.

Pencil or finger: InfiniWorkshop starts with the hand on the toolbar on, so one finger draws and two fingers move and zoom the canvas. The first Apple Pencil stroke switches to Pencil drawing (fingers then move the canvas); tapping the hand switches back.

To try the main features:
1. On the welcome card, See the examples (or, in any project, Insert › Examples): the examples are made as canvases in a folder. Open one and press Play: circuits light up, mechanisms move.
2. New project, then Topic › Electronics. The Circuit tool’s Circuit parts puts a battery, a switch and an LED on the canvas; Circuit › Wire joins their pins (drag from pin to pin); Circuit › Play runs it: tap the switch.
3. Topic › Mechanisms. Motion › Mechanism parts: put a motor and two gears whose teeth touch, one gear pinned on the motor (its hole over the motor’s), then Motion › Play.
4. Topic, on the toolbar, switches the canvas’s tools: Sketch, Technical plans, Woodworking, Electronics, Mechanisms, Floor plans, House wiring, Sewing patterns, Graphs, Diagrams and PDF study. Export makes an image, an SVG, a PDF, a DXF, a 1:1 print, a cut list, a parts list, a 3D model (STL) or a zoom video, and hands it to the share sheet.
5. Lasso some handwriting and choose To text. Find searches typed text and handwriting, in this canvas and in the others.

Network and downloads: nothing is downloaded when the app starts. Google ML Kit’s handwriting model (about 20 MB) downloads the first time To text or Find reads handwriting, or when Download is tapped in Settings › Handwriting, so that first use needs the internet. ML Kit’s switch is the menu › Settings › Handwriting › Read with Google ML Kit (on by default); off, To text is hidden, Find reads no handwriting, and nothing is downloaded or sent to Google through ML Kit. The app itself sends nothing anywhere.

Permissions: no microphone is used. The camera is never used. Insert › Photos and Files use the system’s pickers, which need no permission; saving an exported image or video to Photos through the share sheet asks for add-only access. No location, contacts or tracking.

Links open only when tapped: a link written in a note opens in Safari; FAQ & Contact opens the support page or an email to us; Rate opens the App Store’s rating sheet. After a crash, the next launch offers to open an email draft with a short report, which the person reads and sends themselves (or deletes); nothing is uploaded.

Contact: rde.apps.support@gmail.com
```

## Lengths

Characters per field, as pasted (checked 2026-10-13 by script; every one within its limit).

| Field (limit) | en | es | pt | fr |
|---|---|---|---|---|
| App Store name (30) | 30 | 30 | 29 | 29 |
| App Store subtitle (30) | 27 | 29 | 30 | 27 |
| App Store keywords (100) | 97 | 99 | 93 | 100 |
| App Store promotional text (170) | 156 | 165 | 170 | 165 |
| App Store description (4000) | 2196 | 2544 | 2493 | 2677 |
| Google Play title (30) | 30 | 30 | 29 | 29 |
| Google Play short description (80) | 78 | 73 | 75 | 69 |
| Google Play full description (4000) | 2208 | 2558 | 2505 | 2693 |

## English (U.S.) (Google Play: en-US)

### App Store

**Name** (30/30): InfiniWorkshop: Maker Notebook

**Subtitle** (27/30): Circuits, mechanisms, plans

**Keywords** (97/100): electronics,simulator,breadboard,gears,woodworking,cutlist,dxf,stl,floorplan,blueprint,sewing,diy

**Promotional text** (156/170): Wire a circuit and press Play. Pin gears to a motor and watch them turn. Draw plans at true size and print them 1:1. All on a canvas that zooms without end.

**Description** (2196/4000):

```text
InfiniWorkshop is a project notebook for your iPad whose drawings work: circuits that run, mechanisms that move and plans at true size, all on a canvas you can zoom into without end.

START FROM AN EXAMPLE
• More than 60 working examples, from a flashlight and logic gates to a gearbox, a four-stroke engine and a walking linkage. Press Play in any of them, then change it.

CIRCUITS THAT RUN
• Wire batteries, resistors, LEDs, switches, transistors, logic chips, motors, sensors, displays and microcontroller boards, then press Play: tap switches and buttons and watch it work.
• Measure with ammeters, voltmeters and an oscilloscope. Push a part past its limits and it heats up, then burns.
• Turn a piece of logic you built into a part of your own, and use it again.

MECHANISMS THAT MOVE
• Gears, racks, cams, cranks, pulleys, belts, springs, ropes and motors: pin parts together and press Play.
• Draw your own bodies and set them moving. Overloaded parts break, and a blocked motor jams.

PLANS AT TRUE SIZE
• Technical plans with dimensions, sheets to scale, floor plans and house wiring.
• Woodworking: boards, finger joints, dovetails and 32 mm hole rows; a saw that cuts boards; parts fitted into a board with the saw’s kerf; a cut list.
• Sewing patterns with their marks.
• Print at 1:1 on A4 or Letter, or export DXF for cutting, a 3D model (STL), a cut list or a parts list (CSV).

DRAW AND PLAN FREELY
• Ruler, set squares, protractor, compass, templates, French curve, tape measure and guides.
• Graphs, diagrams, Kanban boards, PDFs to write on, layers, and themes such as Blueprint, Kraft and Cutting mat.
• Lasso your handwriting into text, and find words in your typed text and handwriting across all your projects.

YOURS, AND PRIVATE
No account, no ads, no tracking. Your projects stay on your iPad; Settings › Your data exports everything into one file. Reading handwriting (To text, Find) uses Google ML Kit on the device: its model downloads the first time you use it, and Google receives anonymous diagnostics when it is used. To switch it off: Settings › Handwriting › Read with Google ML Kit. Everything else works offline.

In English, Spanish, Portuguese and French.
```

### Google Play

**Title** (30/30): InfiniWorkshop: Maker Notebook

**Short description** (78/80): Circuits that run, mechanisms that move and plans at true size, on one canvas.

**Full description** (2208/4000):

```text
InfiniWorkshop is a project notebook for your Android tablet whose drawings work: circuits that run, mechanisms that move and plans at true size, all on a canvas you can zoom into without end.

Start from an example
• More than 60 working examples, from a flashlight and logic gates to a gearbox, a four-stroke engine and a walking linkage. Press Play in any of them, then change it.

Circuits that run
• Wire batteries, resistors, LEDs, switches, transistors, logic chips, motors, sensors, displays and microcontroller boards, then press Play: tap switches and buttons and watch it work.
• Measure with ammeters, voltmeters and an oscilloscope. Push a part past its limits and it heats up, then burns.
• Turn a piece of logic you built into a part of your own, and use it again.

Mechanisms that move
• Gears, racks, cams, cranks, pulleys, belts, springs, ropes and motors: pin parts together and press Play.
• Draw your own bodies and set them moving. Overloaded parts break, and a blocked motor jams.

Plans at true size
• Technical plans with dimensions, sheets to scale, floor plans and house wiring.
• Woodworking: boards, finger joints, dovetails and 32 mm hole rows; a saw that cuts boards; parts fitted into a board with the saw’s kerf; a cut list.
• Sewing patterns with their marks.
• Print at 1:1 on A4 or Letter, or export DXF for cutting, a 3D model (STL), a cut list or a parts list (CSV).

Draw and plan freely
• Ruler, set squares, protractor, compass, templates, French curve, tape measure and guides.
• Graphs, diagrams, Kanban boards, PDFs to write on, layers, and themes such as Blueprint, Kraft and Cutting mat.
• Lasso your handwriting into text, and find words in your typed text and handwriting across all your projects.

Yours, and private
No account, no ads, no tracking. Your projects stay on your tablet; Settings › Your data exports everything into one file. Reading handwriting (To text, Find) uses Google ML Kit on the device: its model downloads the first time you use it, and Google receives anonymous diagnostics when it is used. To switch it off: Settings › Handwriting › Read with Google ML Kit. Everything else works offline.

In English, Spanish, Portuguese and French.
```

## Spanish (Spain) (Google Play: es-ES)

### App Store

**Name** (30/30): InfiniWorkshop: cuaderno maker

**Subtitle** (29/30): Circuitos, mecanismos, planos

**Keywords** (99/100): electrónica,simulador,protoboard,lógica,engranajes,carpintería,dxf,stl,costura,bricolaje,led,taller

**Promotional text** (165/170): Conecta un circuito y pulsa Reproducir. Fija engranajes a un motor y míralos girar. Dibuja planos a tamaño real e imprímelos 1:1. Todo en un lienzo con zoom sin fin.

**Description** (2544/4000):

```text
InfiniWorkshop es un cuaderno de proyectos para tu iPad cuyos dibujos funcionan: circuitos que se encienden, mecanismos que se mueven y planos a tamaño real, todo en un lienzo en el que puedes acercarte sin fin.

EMPIEZA POR UN EJEMPLO
• Más de 60 ejemplos que funcionan, de una linterna y unas puertas lógicas a una caja de engranajes, un motor de cuatro tiempos o un mecanismo andante de Jansen. Pulsa Reproducir en cualquiera y cámbialo a tu gusto.

CIRCUITOS QUE FUNCIONAN
• Conecta pilas, resistencias, LED, interruptores, transistores, chips lógicos, motores, sensores, pantallas y placas de microcontrolador, y pulsa Reproducir: toca interruptores y botones y míralo funcionar.
• Mide con amperímetros, voltímetros y un osciloscopio. Lleva un componente más allá de sus límites y se calienta, y luego se quema.
• Convierte la lógica que has montado en un componente propio y úsalo otra vez.

MECANISMOS QUE SE MUEVEN
• Engranajes, cremalleras, levas, manivelas, poleas, correas, muelles, cuerdas y motores: une las piezas con pasadores y pulsa Reproducir.
• Dibuja tus propios cuerpos y ponlos en movimiento. Las piezas sobrecargadas se rompen, y un motor bloqueado se atasca.

PLANOS A TAMAÑO REAL
• Planos técnicos con cotas, hojas a escala, planos de planta e instalación eléctrica.
• Carpintería: tableros, uniones de dedos, colas de milano y filas de agujeros de 32 mm; una sierra que corta tableros; piezas encajadas en un tablero teniendo en cuenta el grosor del corte; una lista de cortes.
• Patrones de costura con sus marcas.
• Imprime a 1:1 en A4 o carta, o exporta DXF para cortar, un modelo 3D (STL), una lista de cortes o una lista de piezas (CSV).

DIBUJA Y PLANIFICA A TU AIRE
• Regla, escuadra y cartabón, transportador, compás, plantillas, plantilla de curvas, cinta métrica y guías.
• Gráficas, diagramas, tableros Kanban, PDF para escribir encima, capas, y temas como Plano, Kraft y Tapete de corte.
• Convierte tu escritura a mano en texto con el lazo, y busca palabras en tus textos y en tu escritura en todos tus proyectos.

TUYO, Y PRIVADO
Sin cuenta, sin anuncios, sin seguimiento. Tus proyectos se quedan en tu iPad; Ajustes › Tus datos lo exporta todo en un archivo. La lectura de la escritura a mano (A texto, Buscar) usa Google ML Kit en el dispositivo: su modelo se descarga la primera vez que la usas, y Google recibe datos de diagnóstico anónimos cuando se usa. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Todo lo demás funciona sin conexión.

En español, inglés, portugués y francés.
```

### Google Play

**Title** (30/30): InfiniWorkshop: cuaderno maker

**Short description** (73/80): Circuitos que funcionan, mecanismos que se mueven y planos a tamaño real.

**Full description** (2558/4000):

```text
InfiniWorkshop es un cuaderno de proyectos para tu tableta Android cuyos dibujos funcionan: circuitos que se encienden, mecanismos que se mueven y planos a tamaño real, todo en un lienzo en el que puedes acercarte sin fin.

Empieza por un ejemplo
• Más de 60 ejemplos que funcionan, de una linterna y unas puertas lógicas a una caja de engranajes, un motor de cuatro tiempos o un mecanismo andante de Jansen. Pulsa Reproducir en cualquiera y cámbialo a tu gusto.

Circuitos que funcionan
• Conecta pilas, resistencias, LED, interruptores, transistores, chips lógicos, motores, sensores, pantallas y placas de microcontrolador, y pulsa Reproducir: toca interruptores y botones y míralo funcionar.
• Mide con amperímetros, voltímetros y un osciloscopio. Lleva un componente más allá de sus límites y se calienta, y luego se quema.
• Convierte la lógica que has montado en un componente propio y úsalo otra vez.

Mecanismos que se mueven
• Engranajes, cremalleras, levas, manivelas, poleas, correas, muelles, cuerdas y motores: une las piezas con pasadores y pulsa Reproducir.
• Dibuja tus propios cuerpos y ponlos en movimiento. Las piezas sobrecargadas se rompen, y un motor bloqueado se atasca.

Planos a tamaño real
• Planos técnicos con cotas, hojas a escala, planos de planta e instalación eléctrica.
• Carpintería: tableros, uniones de dedos, colas de milano y filas de agujeros de 32 mm; una sierra que corta tableros; piezas encajadas en un tablero teniendo en cuenta el grosor del corte; una lista de cortes.
• Patrones de costura con sus marcas.
• Imprime a 1:1 en A4 o carta, o exporta DXF para cortar, un modelo 3D (STL), una lista de cortes o una lista de piezas (CSV).

Dibuja y planifica a tu aire
• Regla, escuadra y cartabón, transportador, compás, plantillas, plantilla de curvas, cinta métrica y guías.
• Gráficas, diagramas, tableros Kanban, PDF para escribir encima, capas, y temas como Plano, Kraft y Tapete de corte.
• Convierte tu escritura a mano en texto con el lazo, y busca palabras en tus textos y en tu escritura en todos tus proyectos.

Tuyo, y privado
Sin cuenta, sin anuncios, sin seguimiento. Tus proyectos se quedan en tu tableta; Ajustes › Tus datos lo exporta todo en un archivo. La lectura de la escritura a mano (A texto, Buscar) usa Google ML Kit en el dispositivo: su modelo se descarga la primera vez que la usas, y Google recibe datos de diagnóstico anónimos cuando se usa. Para desactivarlo: Ajustes › Escritura › Leer con Google ML Kit. Todo lo demás funciona sin conexión.

En español, inglés, portugués y francés.
```

## Portuguese (Brazil) (Google Play: pt-BR)

### App Store

**Name** (29/30): InfiniWorkshop: caderno maker

**Subtitle** (30/30): Circuitos, mecanismos, plantas

**Keywords** (93/100): eletrônica,simulador,protoboard,lógica,engrenagens,marcenaria,dxf,stl,costura,diy,led,oficina

**Promotional text** (170/170): Monte um circuito e toque em Reproduzir. Prenda engrenagens a um motor e veja-as girar. Desenhe plantas em tamanho real e imprima em 1:1. Tudo numa tela com zoom sem fim.

**Description** (2493/4000):

```text
O InfiniWorkshop é um caderno de projetos para o seu iPad cujos desenhos funcionam: circuitos que acendem, mecanismos que se movem e plantas em tamanho real, tudo numa tela que você pode aproximar sem fim.

COMECE POR UM EXEMPLO
• Mais de 60 exemplos que funcionam, de uma lanterna e portas lógicas a uma caixa de engrenagens, um motor de quatro tempos e um mecanismo de Jansen que anda. Toque em Reproduzir em qualquer um e mude o que quiser.

CIRCUITOS QUE FUNCIONAM
• Ligue pilhas, resistores, LEDs, interruptores, transistores, chips lógicos, motores, sensores, displays e placas de microcontrolador, e toque em Reproduzir: acione interruptores e botões e veja funcionar.
• Meça com amperímetros, voltímetros e um osciloscópio. Leve um componente além dos seus limites e ele esquenta, depois queima.
• Transforme uma lógica que você montou num componente seu e use de novo.

MECANISMOS QUE SE MOVEM
• Engrenagens, cremalheiras, cames, manivelas, polias, correias, molas, cordas e motores: prenda as peças com pinos e toque em Reproduzir.
• Desenhe seus próprios corpos e coloque-os em movimento. Peças sobrecarregadas quebram, e um motor bloqueado trava.

PLANTAS EM TAMANHO REAL
• Planos técnicos com cotas, folhas em escala, plantas baixas e instalação elétrica.
• Marcenaria: tábuas, juntas de dedos, rabos de andorinha e filas de furos de 32 mm; uma serra que corta tábuas; peças encaixadas numa tábua levando em conta a espessura do corte; uma lista de cortes.
• Moldes de costura com suas marcações.
• Imprima em 1:1 em A4 ou Carta, ou exporte DXF para corte, um modelo 3D (STL), uma lista de cortes ou uma lista de peças (CSV).

DESENHE E PLANEJE À VONTADE
• Régua, esquadros, transferidor, compasso, gabaritos, curva francesa, trena e guias.
• Gráficos, diagramas, quadros Kanban, PDFs para escrever por cima, camadas e temas como Planta, Kraft e Base de corte.
• Transforme sua escrita à mão em texto com o laço, e busque palavras nos seus textos e na sua escrita em todos os seus projetos.

SEU, E PRIVADO
Sem conta, sem anúncios, sem rastreamento. Seus projetos ficam no seu iPad; Configurações › Seus dados exporta tudo num arquivo. A leitura da escrita à mão (Para texto, Buscar) usa o Google ML Kit no dispositivo: o modelo é baixado na primeira vez que você a usa, e o Google recebe dados de diagnóstico anônimos quando ela é usada. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Todo o resto funciona offline.

Em português, inglês, espanhol e francês.
```

### Google Play

**Title** (29/30): InfiniWorkshop: caderno maker

**Short description** (75/80): Circuitos que funcionam, mecanismos que se movem e plantas em tamanho real.

**Full description** (2505/4000):

```text
O InfiniWorkshop é um caderno de projetos para o seu tablet Android cujos desenhos funcionam: circuitos que acendem, mecanismos que se movem e plantas em tamanho real, tudo numa tela que você pode aproximar sem fim.

Comece por um exemplo
• Mais de 60 exemplos que funcionam, de uma lanterna e portas lógicas a uma caixa de engrenagens, um motor de quatro tempos e um mecanismo de Jansen que anda. Toque em Reproduzir em qualquer um e mude o que quiser.

Circuitos que funcionam
• Ligue pilhas, resistores, LEDs, interruptores, transistores, chips lógicos, motores, sensores, displays e placas de microcontrolador, e toque em Reproduzir: acione interruptores e botões e veja funcionar.
• Meça com amperímetros, voltímetros e um osciloscópio. Leve um componente além dos seus limites e ele esquenta, depois queima.
• Transforme uma lógica que você montou num componente seu e use de novo.

Mecanismos que se movem
• Engrenagens, cremalheiras, cames, manivelas, polias, correias, molas, cordas e motores: prenda as peças com pinos e toque em Reproduzir.
• Desenhe seus próprios corpos e coloque-os em movimento. Peças sobrecarregadas quebram, e um motor bloqueado trava.

Plantas em tamanho real
• Planos técnicos com cotas, folhas em escala, plantas baixas e instalação elétrica.
• Marcenaria: tábuas, juntas de dedos, rabos de andorinha e filas de furos de 32 mm; uma serra que corta tábuas; peças encaixadas numa tábua levando em conta a espessura do corte; uma lista de cortes.
• Moldes de costura com suas marcações.
• Imprima em 1:1 em A4 ou Carta, ou exporte DXF para corte, um modelo 3D (STL), uma lista de cortes ou uma lista de peças (CSV).

Desenhe e planeje à vontade
• Régua, esquadros, transferidor, compasso, gabaritos, curva francesa, trena e guias.
• Gráficos, diagramas, quadros Kanban, PDFs para escrever por cima, camadas e temas como Planta, Kraft e Base de corte.
• Transforme sua escrita à mão em texto com o laço, e busque palavras nos seus textos e na sua escrita em todos os seus projetos.

Seu, e privado
Sem conta, sem anúncios, sem rastreamento. Seus projetos ficam no seu tablet; Configurações › Seus dados exporta tudo num arquivo. A leitura da escrita à mão (Para texto, Buscar) usa o Google ML Kit no dispositivo: o modelo é baixado na primeira vez que você a usa, e o Google recebe dados de diagnóstico anônimos quando ela é usada. Para desligá-lo: Configurações › Escrita › Ler com o Google ML Kit. Todo o resto funciona offline.

Em português, inglês, espanhol e francês.
```

## French (Google Play: fr-FR)

### App Store

**Name** (29/30): InfiniWorkshop : carnet maker

**Subtitle** (27/30): Circuits, mécanismes, plans

**Keywords** (100/100): électronique,simulateur,breadboard,logique,engrenages,menuiserie,débit,dxf,stl,couture,bricolage,led

**Promotional text** (165/170): Câblez un circuit et touchez Lancer. Fixez des engrenages à un moteur et regardez-les tourner. Dessinez des plans à taille réelle, imprimez-les à 1:1. Zoom sans fin.

**Description** (2677/4000):

```text
InfiniWorkshop est un carnet de projets pour votre iPad dont les dessins fonctionnent : des circuits qui s’allument, des mécanismes qui bougent et des plans à taille réelle, le tout sur une toile où zoomer sans fin.

PARTEZ D’UN EXEMPLE
• Plus de 60 exemples qui fonctionnent, d’une lampe de poche et de portes logiques à une boîte d’engrenages, un moteur à quatre temps et un mécanisme marcheur de Jansen. Touchez Lancer dans n’importe lequel, puis modifiez-le.

DES CIRCUITS QUI MARCHENT
• Câblez piles, résistances, LED, interrupteurs, transistors, puces logiques, moteurs, capteurs, afficheurs et cartes à microcontrôleur, puis touchez Lancer : actionnez interrupteurs et boutons, et regardez le circuit fonctionner.
• Mesurez avec des ampèremètres, des voltmètres et un oscilloscope. Poussez un composant au-delà de ses limites : il chauffe, puis grille.
• Faites de la logique que vous avez montée un composant à vous, et réutilisez-le.

DES MÉCANISMES QUI BOUGENT
• Engrenages, crémaillères, cames, manivelles, poulies, courroies, ressorts, cordes et moteurs : assemblez les pièces avec des axes et touchez Lancer.
• Dessinez vos propres corps et mettez-les en mouvement. Les pièces surchargées cassent, et un moteur bloqué cale.

DES PLANS À TAILLE RÉELLE
• Plans techniques cotés, feuilles à l’échelle, plans d’étage et installation électrique.
• Menuiserie : planches, entailles droites, queues d’aronde et rangées de trous de 32 mm ; une scie qui coupe les planches ; des pièces placées dans une planche en tenant compte du trait de scie ; une liste de débit.
• Patrons de couture avec leurs repères.
• Imprimez à 1:1 en A4 ou Letter, ou exportez un DXF pour la découpe, un modèle 3D (STL), une liste de débit ou une liste des pièces (CSV).

DESSINEZ ET PLANIFIEZ LIBREMENT
• Règle, équerres, rapporteur, compas, gabarits, pistolet, mètre ruban et repères.
• Graphiques, diagrammes, tableaux Kanban, PDF à annoter, calques, et des thèmes comme Plan, Kraft et Tapis de coupe.
• Transformez votre écriture manuscrite en texte au lasso, et cherchez des mots dans vos textes et votre écriture dans tous vos projets.

À VOUS, ET PRIVÉ
Pas de compte, pas de publicité, pas de pistage. Vos projets restent sur votre iPad ; Réglages › Vos données exporte tout dans un seul fichier. La lecture de l’écriture manuscrite (En texte, Rechercher) utilise Google ML Kit sur l’appareil : son modèle se télécharge la première fois que vous vous en servez, et Google reçoit des données de diagnostic anonymes quand elle est utilisée. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Tout le reste fonctionne hors ligne.

En français, anglais, espagnol et portugais.
```

### Google Play

**Title** (29/30): InfiniWorkshop : carnet maker

**Short description** (69/80): Circuits qui marchent, mécanismes qui bougent, plans à taille réelle.

**Full description** (2693/4000):

```text
InfiniWorkshop est un carnet de projets pour votre tablette Android dont les dessins fonctionnent : des circuits qui s’allument, des mécanismes qui bougent et des plans à taille réelle, le tout sur une toile où zoomer sans fin.

Partez d’un exemple
• Plus de 60 exemples qui fonctionnent, d’une lampe de poche et de portes logiques à une boîte d’engrenages, un moteur à quatre temps et un mécanisme marcheur de Jansen. Touchez Lancer dans n’importe lequel, puis modifiez-le.

Des circuits qui marchent
• Câblez piles, résistances, LED, interrupteurs, transistors, puces logiques, moteurs, capteurs, afficheurs et cartes à microcontrôleur, puis touchez Lancer : actionnez interrupteurs et boutons, et regardez le circuit fonctionner.
• Mesurez avec des ampèremètres, des voltmètres et un oscilloscope. Poussez un composant au-delà de ses limites : il chauffe, puis grille.
• Faites de la logique que vous avez montée un composant à vous, et réutilisez-le.

Des mécanismes qui bougent
• Engrenages, crémaillères, cames, manivelles, poulies, courroies, ressorts, cordes et moteurs : assemblez les pièces avec des axes et touchez Lancer.
• Dessinez vos propres corps et mettez-les en mouvement. Les pièces surchargées cassent, et un moteur bloqué cale.

Des plans à taille réelle
• Plans techniques cotés, feuilles à l’échelle, plans d’étage et installation électrique.
• Menuiserie : planches, entailles droites, queues d’aronde et rangées de trous de 32 mm ; une scie qui coupe les planches ; des pièces placées dans une planche en tenant compte du trait de scie ; une liste de débit.
• Patrons de couture avec leurs repères.
• Imprimez à 1:1 en A4 ou Letter, ou exportez un DXF pour la découpe, un modèle 3D (STL), une liste de débit ou une liste des pièces (CSV).

Dessinez et planifiez librement
• Règle, équerres, rapporteur, compas, gabarits, pistolet, mètre ruban et repères.
• Graphiques, diagrammes, tableaux Kanban, PDF à annoter, calques, et des thèmes comme Plan, Kraft et Tapis de coupe.
• Transformez votre écriture manuscrite en texte au lasso, et cherchez des mots dans vos textes et votre écriture dans tous vos projets.

À vous, et privé
Pas de compte, pas de publicité, pas de pistage. Vos projets restent sur votre tablette ; Réglages › Vos données exporte tout dans un seul fichier. La lecture de l’écriture manuscrite (En texte, Rechercher) utilise Google ML Kit sur l’appareil : son modèle se télécharge la première fois que vous vous en servez, et Google reçoit des données de diagnostic anonymes quand elle est utilisée. Pour le désactiver : Réglages › Écriture › Lire avec Google ML Kit. Tout le reste fonctionne hors ligne.

En français, anglais, espagnol et portugais.
```
