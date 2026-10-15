# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Arabic's own UI strings, after the layers' (fude/drawing/, fude/study/ and
# fude/lang/ar/strings.py). Run after changing any of them:
#
#   python3 tools/strings/build.py arabic
#
# (tools/strings/build.py has how a row is written, and what it writes.)

# --- common words ---------------------------------------------------------------
t('ARABIC',
  'Alphabet',
  'Alfabeto',
  'Alfabeto',
  '文字表',
  'Alphabet')

# --- the side panel and Settings ---------------------------------------------------------------
t('CREDITS',
  'Arabic’s stroke order drawn for {APP}. Words, their vowels, romanization and meanings from Wiktionary (CC BY-SA 4.0, via kaikki.org); how common they are from wordfreq (CC BY-SA 4.0); example sentences from Tatoeba (tatoeba.org, CC BY 2.0 FR). Handwriting recognition by Google ML Kit, which sends Google anonymous usage data while it is on. Every licence in full: Licences.',
  'Orden de trazos del árabe dibujado para {APP}. Palabras, sus vocales, su romanización y sus significados de Wiktionary (CC BY-SA 4.0, vía kaikki.org); su frecuencia de wordfreq (CC BY-SA 4.0); frases de ejemplo de Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconocimiento de escritura de Google ML Kit, que envía a Google datos de uso anónimos mientras está activado. Todas las licencias completas: Licencias.',
  'Ordem dos traços do árabe desenhada para o {APP}. Palavras, suas vogais, sua romanização e seus significados do Wiktionary (CC BY-SA 4.0, via kaikki.org); sua frequência do wordfreq (CC BY-SA 4.0); frases de exemplo do Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconhecimento de escrita do Google ML Kit, que envia ao Google dados de uso anônimos enquanto está ligado. Todas as licenças completas: Licenças.',
  'アラビア文字の書き順：{APP}のために作成。単語・母音記号・ローマ字・意味：Wiktionary（CC BY-SA 4.0、kaikki.org経由）。頻度：wordfreq（CC BY-SA 4.0）。例文：Tatoeba（tatoeba.org、CC BY 2.0 FR）。手書き認識：Google ML Kit（オンの間、匿名の利用データがGoogleに送信されます）。すべてのライセンスの全文：ライセンス。',
  'Ordre des traits de l’arabe dessiné pour {APP}. Mots, leurs voyelles, leur romanisation et leurs sens de Wiktionary (CC BY-SA 4.0, via kaikki.org) ; leur fréquence de wordfreq (CC BY-SA 4.0) ; phrases d’exemple de Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconnaissance de l’écriture par Google ML Kit, qui envoie à Google des données d’utilisation anonymes tant qu’il est activé. Toutes les licences complètes : Licences.')

# The welcome (study/app/welcome.h): the first time Arabic opens, and from Settings.
t('WELCOME_1',
  'Write Arabic by hand, the way it is written: {APP} shows every letter’s strokes in order, from right to left, in each of the forms it takes in a word, reads what you write and tells you how well it went. It works offline, and everything stays on this device.',
  'Escribe árabe a mano, como se escribe: {APP} muestra los trazos de cada letra en orden, de derecha a izquierda, en cada una de las formas que toma en una palabra, lee lo que escribes y te dice qué tal ha salido. Funciona sin conexión y todo se queda en este dispositivo.',
  'Escreva árabe à mão, do jeito que se escreve: o {APP} mostra os traços de cada letra em ordem, da direita para a esquerda, em cada uma das formas que ela toma numa palavra, lê o que você escreve e diz como ficou. Funciona offline, e tudo fica neste dispositivo.',
  'アラビア語を、書かれるとおりに手で書きましょう。{APP}は、一文字ずつ、語の中でとるそれぞれの形で、右から左への筆順を示し、書いた字を読み取って、出来ばえを教えてくれます。オフラインで動き、すべてこの端末に残ります。',
  'Écrivez l’arabe à la main, comme il s’écrit : {APP} montre les traits de chaque lettre dans l’ordre, de droite à gauche, dans chacune des formes qu’elle prend dans un mot, lit ce que vous écrivez et vous dit comment c’était. Il fonctionne hors ligne, et tout reste sur cet appareil.')
# ML Kit cannot read Arabic in a photo: the camera scans pages into the Library instead.
o('WELCOME_3_TITLE',
  'Scan pages',
  'Escanea páginas',
  'Digitalize páginas',
  'ページをスキャン',
  'Numérisez des pages')
t('WELCOME_3',
  'In Lectures, Scan pages turns a book or a worksheet into pages to write on with the camera, and From Photos makes a PDF of pictures you already have. Write over them, mark them and keep them with your canvases.',
  'En Lecciones, Escanear páginas convierte un libro o una ficha en páginas para escribir encima con la cámara, y De Fotos hace un PDF con imágenes que ya tienes. Escribe sobre ellas, márcalas y guárdalas con tus lienzos.',
  'Em Lições, Digitalizar páginas transforma um livro ou uma ficha em páginas para escrever por cima com a câmera, e De Fotos faz um PDF com imagens que você já tem. Escreva sobre elas, marque-as e guarde-as com suas telas.',
  '教材の「ページをスキャン」で本やプリントをカメラで取り込んで書き込めるページにでき、「写真から」で手持ちの画像からPDFを作れます。上から書いたり印を付けたりして、キャンバスと一緒に残せます。',
  'Dans Leçons, Numériser des pages transforme un livre ou une fiche en pages sur lesquelles écrire, avec l’appareil photo, et Depuis Photos fait un PDF d’images que vous avez déjà. Écrivez dessus, annotez-les et gardez-les avec vos toiles.')
t('WELCOME_5',
  'Lasso your writing and choose Translate with Google to read it in your language. In Vocabulary, Translate with Google goes the other way: type in your language and get it in Arabic, with its words to save, practise or write. The translation models download once, then work offline.',
  'Rodea lo que escribiste con el lazo y elige Traducir con Google para leerlo en tu idioma. En Vocabulario, Traducir con Google va al revés: escribe en tu idioma y obtén la traducción al árabe, con sus palabras para guardar, practicar o escribir. Los modelos de traducción se descargan una vez y luego funcionan sin conexión.',
  'Contorne a sua escrita com o laço e escolha Traduzir com o Google para lê-la no seu idioma. Em Vocabulário, Traduzir com o Google faz o caminho inverso: digite no seu idioma e receba a tradução para o árabe, com as palavras para salvar, praticar ou escrever. Os modelos de tradução são baixados uma vez e depois funcionam offline.',
  '書いた字を投げなわで囲み「Googleで翻訳」を選ぶと、自分の言語で読めます。単語帳の「Googleで翻訳」はその逆で、自分の言語で入力するとアラビア語に訳し、含まれる単語を保存・練習・ページに書けます。翻訳モデルは一度ダウンロードすれば、オフラインで使えます。',
  'Entourez votre écriture au lasso et choisissez Traduire avec Google pour la lire dans votre langue. Dans Vocabulaire, Traduire avec Google fait l’inverse : écrivez dans votre langue et obtenez la traduction en arabe, avec ses mots à enregistrer, pratiquer ou écrire. Les modèles de traduction se téléchargent une fois, puis marchent hors ligne.')
t('WELCOME_6',
  'The menu at the top left has everything to study: the Arabic alphabet with each letter’s joined forms, the letters with their stroke order, words to read and listen to, practice, reviews and exams. Lectures has free lessons, and the PDFs, photos and pages you scan, to read and write on.',
  'El menú de arriba a la izquierda tiene todo para estudiar: el alfabeto árabe con las formas unidas de cada letra, las letras con su orden de trazos, palabras para leer y escuchar, práctica, repasos y exámenes. Lecciones tiene clases gratuitas, y los PDF, fotos y páginas que escanees, para leer y escribir encima.',
  'O menu no canto superior esquerdo tem tudo para estudar: o alfabeto árabe com as formas ligadas de cada letra, as letras com a ordem dos traços, palavras para ler e ouvir, prática, revisões e provas. Lições tem aulas gratuitas, e os PDFs, fotos e páginas que você digitalizar, para ler e escrever por cima.',
  '左上のメニューに学習のすべてがあります。各文字の連結形つきのアラビア文字表、筆順つきの文字、読んで聞ける単語、練習、復習、試験。教材には無料のレッスンと、自分のPDF・写真・スキャンしたページがあり、読んだり書き込んだりできます。',
  'Le menu en haut à gauche a tout pour étudier : l’alphabet arabe avec les formes liées de chaque lettre, les lettres avec leur ordre des traits, des mots à lire et à écouter, l’entraînement, les révisions et les examens. Leçons a des cours gratuits, et les PDF, photos et pages que vous numérisez, à lire et sur lesquels écrire.')

# The lectures (the Library's books: doc.h).
t('LECTURE_PHRASEBOOK',
  'Arabic phrasebook',
  'Guía de árabe',
  'Guia de conversação árabe',
  'アラビア語会話集',
  'Guide linguistique arabe')
t('LECTURE_PHRASEBOOK_ABOUT',
  'Arabic, its alphabet and pronunciation, a little grammar, and the phrases a trip needs (greetings, numbers, time, getting around, eating, shopping), with their romanization. From Wikivoyage.',
  'El árabe, su alfabeto y su pronunciación, un poco de gramática y las frases de un viaje (saludos, números, horas, transporte, comida, compras), con su romanización. De Wikiviajes.',
  'O árabe, o alfabeto e a pronúncia, um pouco de gramática e as frases de que uma viagem precisa (cumprimentos, números, horas, transporte, comida, compras), com a romanização. Do Wikivoyage.',
  'アラビア語、その文字と発音、少しの文法、旅に必要なフレーズ（あいさつ、数字、時間、交通、食事、買い物）をローマ字つきで。Wikivoyageより。',
  'L’arabe, son alphabet et sa prononciation, un peu de grammaire et les phrases d’un voyage (salutations, nombres, heures, transports, nourriture, achats), avec leur romanisation. De Wikivoyage.')
