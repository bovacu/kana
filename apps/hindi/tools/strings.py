# Hindi's UI strings: English and four translations. Every text the app shows
# is in the layers' files and here, and only there: the core's (fude/drawing/
# strings.py), the study's (fude/study/strings.py), Hindi's
# (fude/lang/hi/strings.py), then Hindi's own below. Run after changing one:
#
#   python3 apps/hindi/tools/strings.py
#
# It checks every language has every id with the same placeholders, then writes
#   apps/hindi/src/text_ids.h            the ids, in order (FUDE_TEXT_ID(X) lines)
#   apps/hindi/assets/text/strings.rdel  RDE's localization file, one block per language
# (both generated: edit the strings' files, not them). tools/strings/build.py has
# how a row is written.
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.dont_write_bytecode = True   # no __pycache__ left in tools/strings/
sys.path.insert(0, os.path.join(ROOT, 'tools', 'strings'))
from build import t, o, P, layer, write, fourth

layer(os.path.join(ROOT, 'fude', 'drawing', 'strings.py'))
layer(os.path.join(ROOT, 'fude', 'study', 'strings.py'))
layer(os.path.join(ROOT, 'fude', 'lang', 'hi', 'strings.py'))

# --- common words ---------------------------------------------------------------
t('HINDI',
  'Alphabet',
  'Alfabeto',
  'Alfabeto',
  '文字表',
  'Alphabet')

# --- the side panel and Settings ---------------------------------------------------------------
t('LICENCE_LECTURES',
  'Lectures',
  'Lecciones',
  'Lições',
  '教材',
  'Leçons')
t('CREDITS',
  'Hindi’s stroke order drawn for {APP}. Words, their romanization and their meanings from Wiktionary (CC BY-SA 4.0, via kaikki.org); how common they are from wordfreq (CC BY-SA 4.0); example sentences from Tatoeba (tatoeba.org, CC BY 2.0 FR). Handwriting and text recognition by Google ML Kit, which sends Google anonymous usage data while it is on. Every licence in full: Licences.',
  'Orden de trazos del hindi dibujado para {APP}. Palabras, su romanización y sus significados de Wiktionary (CC BY-SA 4.0, vía kaikki.org); su frecuencia de wordfreq (CC BY-SA 4.0); frases de ejemplo de Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconocimiento de escritura y de texto de Google ML Kit, que envía a Google datos de uso anónimos mientras está activado. Todas las licencias completas: Licencias.',
  'Ordem dos traços do híndi desenhada para o {APP}. Palavras, sua romanização e seus significados do Wiktionary (CC BY-SA 4.0, via kaikki.org); sua frequência do wordfreq (CC BY-SA 4.0); frases de exemplo do Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconhecimento de escrita e de texto do Google ML Kit, que envia ao Google dados de uso anônimos enquanto está ligado. Todas as licenças completas: Licenças.',
  'デーヴァナーガリーの書き順：{APP}のために作成。単語・ローマ字・意味：Wiktionary（CC BY-SA 4.0、kaikki.org経由）。頻度：wordfreq（CC BY-SA 4.0）。例文：Tatoeba（tatoeba.org、CC BY 2.0 FR）。手書き・文字認識：Google ML Kit（オンの間、匿名の利用データがGoogleに送信されます）。すべてのライセンスの全文：ライセンス。',
  'Ordre des traits du hindi dessiné pour {APP}. Mots, leur romanisation et leurs sens de Wiktionary (CC BY-SA 4.0, via kaikki.org) ; leur fréquence de wordfreq (CC BY-SA 4.0) ; phrases d’exemple de Tatoeba (tatoeba.org, CC BY 2.0 FR). Reconnaissance de l’écriture et du texte par Google ML Kit, qui envoie à Google des données d’utilisation anonymes tant qu’il est activé. Toutes les licences complètes : Licences.')

# The welcome (study/app/welcome.h): the first time Hindi opens, and from Settings.
t('WELCOME',
  'Welcome',
  'Bienvenida',
  'Boas-vindas',
  'ようこそ',
  'Bienvenue')
t('WELCOME_SKIP',
  'Skip',
  'Saltar',
  'Pular',
  'スキップ',
  'Passer')
t('WELCOME_START',
  'Start writing',
  'Empezar a escribir',
  'Começar a escrever',
  '書きはじめる',
  'Commencer à écrire')
t('WELCOME_1_TITLE',
  'Welcome to {APP}',
  'Te damos la bienvenida a {APP}',
  'Boas-vindas ao {APP}',
  '{APP}へようこそ',
  'Bienvenue dans {APP}')
t('WELCOME_1',
  'Write Hindi by hand, the way it is written: {APP} shows every Devanagari letter’s strokes in order, the line along the top last, reads what you write and tells you how well it went. It works offline, and everything stays on this device.',
  'Escribe hindi a mano, como se escribe: {APP} muestra los trazos de cada letra devanagari en orden, la línea de arriba al final, lee lo que escribes y te dice qué tal ha salido. Funciona sin conexión y todo se queda en este dispositivo.',
  'Escreva hindi à mão, do jeito que se escreve: o {APP} mostra os traços de cada letra devanágari em ordem, a linha de cima por último, lê o que você escreve e diz como ficou. Funciona offline, e tudo fica neste dispositivo.',
  'ヒンディー語を、書かれるとおりに手で書きましょう。{APP}は、デーヴァナーガリー文字の筆順を一文字ずつ（上の横線は最後に）示し、書いた字を読み取って、出来ばえを教えてくれます。オフラインで動き、すべてこの端末に残ります。',
  'Écrivez le hindi à la main, comme il s’écrit : {APP} montre les traits de chaque lettre devanagari dans l’ordre, la ligne du haut en dernier, lit ce que vous écrivez et vous dit comment c’était. Il fonctionne hors ligne, et tout reste sur cet appareil.')
t('WELCOME_PHONE',
  '{APP} is designed for tablets. It works on phones too, with less room to write.',
  '{APP} está pensado para tabletas. También funciona en el móvil, con menos espacio para escribir.',
  'O {APP} foi pensado para tablets. Também funciona no celular, com menos espaço para escrever.',
  '{APP}はタブレット向けに作られています。スマートフォンでも使えますが、書くスペースは小さくなります。',
  '{APP} est conçu pour les tablettes. Il fonctionne aussi sur téléphone, avec moins de place pour écrire.')
t('WELCOME_2_TITLE',
  'Pencil or finger',
  'Pencil o dedo',
  'Pencil ou dedo',
  'Pencilでも指でも',
  'Pencil ou doigt')
t('WELCOME_2_TITLE_ANDROID',
  'Stylus or finger',
  'Lápiz o dedo',
  'Caneta ou dedo',
  'ペンでも指でも',
  'Stylet ou doigt')
t('WELCOME_2',
  'Write with the Apple Pencil — fingers then move the page — or, with the hand on the bar, with one finger, and move the page with two. {APP} switches by itself when the Pencil writes. The tools are on a bar you can drag anywhere; a double tap on its handle folds it away.',
  'Escribe con el Apple Pencil —los dedos mueven entonces la página— o, con la mano de la barra, con un dedo, y mueve la página con dos. {APP} cambia solo cuando escribe el Pencil. Las herramientas están en una barra que puedes arrastrar adonde quieras; un doble toque en su asa la pliega.',
  'Escreva com o Apple Pencil — os dedos então movem a página — ou, com a mão da barra, com um dedo, e mova a página com dois. O {APP} muda sozinho quando o Pencil escreve. As ferramentas ficam numa barra que você arrasta para onde quiser; um toque duplo na alça a recolhe.',
  'Apple Pencilで書く（ページは指で動かします）か、バーの手のボタンで指1本で書き、ページは2本指で動かします。Pencilで書くと自動で切り替わります。道具はどこへでもドラッグできるバーにあり、取っ手をダブルタップすると畳めます。',
  'Écrivez à l’Apple Pencil — les doigts déplacent alors la page — ou, avec la main de la barre, d’un doigt, et déplacez la page à deux doigts. {APP} bascule de lui-même quand le Pencil écrit. Les outils sont sur une barre à déplacer où vous voulez ; un double toucher sur sa poignée la replie.')
t('WELCOME_2_ANDROID',
  'Write with a stylus — fingers then move the page — or, with the hand on the bar, with one finger, and move the page with two. {APP} switches by itself when the stylus writes. The tools are on a bar you can drag anywhere; a double tap on its handle folds it away.',
  'Escribe con un lápiz óptico —los dedos mueven entonces la página— o, con la mano de la barra, con un dedo, y mueve la página con dos. {APP} cambia solo cuando escribe el lápiz. Las herramientas están en una barra que puedes arrastrar adonde quieras; un doble toque en su asa la pliega.',
  'Escreva com uma caneta — os dedos então movem a página — ou, com a mão da barra, com um dedo, e mova a página com dois. O {APP} muda sozinho quando a caneta escreve. As ferramentas ficam numa barra que você arrasta para onde quiser; um toque duplo na alça a recolhe.',
  'タッチペンで書く（ページは指で動かします）か、バーの手のボタンで指1本で書き、ページは2本指で動かします。ペンで書くと自動で切り替わります。道具はどこへでもドラッグできるバーにあり、取っ手をダブルタップすると畳めます。',
  'Écrivez au stylet — les doigts déplacent alors la page — ou, avec la main de la barre, d’un doigt, et déplacez la page à deux doigts. {APP} bascule de lui-même quand le stylet écrit. Les outils sont sur une barre à déplacer où vous voulez ; un double toucher sur sa poignée la replie.')
t('WELCOME_3_TITLE',
  'Read with the camera',
  'Lee con la cámara',
  'Leia com a câmera',
  'カメラで読む',
  'Lire avec l’appareil photo')
t('WELCOME_3',
  'Tap the camera on the bar, or press and hold the page for Text from photo, and point it at Hindi text: a book, a sign, a menu. {APP} reads it live and boxes each line: Hold keeps them, Write puts them on the page as handwriting to trace, and Translate with Google shows them in your language. A photo works too.',
  'Toca la cámara de la barra, o mantén pulsada la página y elige Texto de foto, y apunta a un texto en hindi: un libro, un cartel, una carta. {APP} lo lee en directo y enmarca cada línea: Congelar las conserva, Escribir las pone en la página como escritura a mano para repasar, y Traducir con Google las muestra en tu idioma. También sirve una foto.',
  'Toque na câmera da barra, ou toque e segure a página e escolha Texto de foto, e aponte para um texto em hindi: um livro, uma placa, um cardápio. O {APP} lê ao vivo e destaca cada linha: Congelar as mantém, Escrever as coloca na página como escrita à mão para cobrir, e Traduzir com o Google as mostra no seu idioma. Uma foto também serve.',
  'バーのカメラをタップするか、ページを長押しして「写真の文字」を選び、ヒンディー語の本や看板、メニューに向けます。{APP}がその場で読み取り、行ごとに枠を付けます。「固定」で残し、「書く」でなぞれる手書きとしてページに書き、「Googleで翻訳」で自分の言語に訳せます。写真からもできます。',
  'Touchez l’appareil photo de la barre, ou appuyez longuement sur la page et choisissez Texte d’une photo, puis visez un texte en hindi : un livre, un panneau, un menu. {APP} le lit en direct et encadre chaque ligne : Figer les garde, Écrire les pose sur la page en écriture à la main à repasser, et Traduire avec Google les affiche dans votre langue. Une photo marche aussi.')
t('WELCOME_4_TITLE',
  'Copy, paste, check',
  'Copia, pega, revisa',
  'Copie, cole, confira',
  'コピー・貼り付け・チェック',
  'Copier, coller, vérifier')
t('WELCOME_4',
  'Draw around writing with the lasso: copy it, Copy as text to use it in any app, Check what it says, or Save word. Press and hold the page to paste: Paste text writes text copied anywhere as handwriting, stroke by stroke.',
  'Rodea lo escrito con el lazo: cópialo, usa Copiar como texto para llevarlo a cualquier app, Revisar lo que dice, o Guardar palabra. Mantén pulsada la página para pegar: Pegar texto escribe a mano, trazo a trazo, un texto copiado en cualquier sitio.',
  'Contorne a escrita com o laço: copie, use Copiar como texto para levá-la a qualquer app, Verificar o que diz, ou Salvar palavra. Toque e segure a página para colar: Colar texto escreve à mão, traço a traço, um texto copiado em qualquer lugar.',
  '書いた字を投げなわで囲むと、コピー、ほかのアプリで使える「テキストでコピー」、読み取りを確かめる「チェック」、「単語を保存」ができます。ページを長押しすると貼り付け：「テキストを貼り付け」は、どこかでコピーした文を一画ずつ手書きで書きます。',
  'Entourez l’écriture au lasso : copiez-la, Copier en texte pour l’utiliser dans n’importe quelle app, Vérifier ce qu’elle dit, ou Enregistrer un mot. Appuyez longuement sur la page pour coller : Coller le texte écrit à la main, trait par trait, un texte copié n’importe où.')
t('WELCOME_5_TITLE',
  'Translate',
  'Traduce',
  'Traduza',
  '翻訳',
  'Traduire')
t('WELCOME_5',
  'Lasso your writing and choose Translate with Google to read it in your language. In Vocabulary, Translate with Google goes the other way: type in your language and get it in Hindi, with its words to save, practise or write. The translation models download once, then work offline.',
  'Rodea lo que escribiste con el lazo y elige Traducir con Google para leerlo en tu idioma. En Vocabulario, Traducir con Google va al revés: escribe en tu idioma y obtén la traducción al hindi, con sus palabras para guardar, practicar o escribir. Los modelos de traducción se descargan una vez y luego funcionan sin conexión.',
  'Contorne a sua escrita com o laço e escolha Traduzir com o Google para lê-la no seu idioma. Em Vocabulário, Traduzir com o Google faz o caminho inverso: digite no seu idioma e receba a tradução para o hindi, com as palavras para salvar, praticar ou escrever. Os modelos de tradução são baixados uma vez e depois funcionam offline.',
  '書いた字を投げなわで囲み「Googleで翻訳」を選ぶと、自分の言語で読めます。単語帳の「Googleで翻訳」はその逆で、自分の言語で入力するとヒンディー語に訳し、含まれる単語を保存・練習・ページに書けます。翻訳モデルは一度ダウンロードすれば、オフラインで使えます。',
  'Entourez votre écriture au lasso et choisissez Traduire avec Google pour la lire dans votre langue. Dans Vocabulaire, Traduire avec Google fait l’inverse : écrivez dans votre langue et obtenez la traduction en hindi, avec ses mots à enregistrer, pratiquer ou écrire. Les modèles de traduction se téléchargent une fois, puis marchent hors ligne.')
t('WELCOME_6_TITLE',
  'Learn',
  'Aprende',
  'Aprenda',
  '学ぶ',
  'Apprendre')
t('WELCOME_6',
  'The menu at the top left has everything to study: the Devanagari alphabet, its vowels, consonants and vowel signs with their stroke order, words to read and listen to, practice, reviews and exams. Lectures has free lessons, and the PDFs, photos and pages you scan, to read and write on.',
  'El menú de arriba a la izquierda tiene todo para estudiar: el alfabeto devanagari, sus vocales, consonantes y matras con su orden de trazos, palabras para leer y escuchar, práctica, repasos y exámenes. Lecciones tiene clases gratuitas, y los PDF, fotos y páginas que escanees, para leer y escribir encima.',
  'O menu no canto superior esquerdo tem tudo para estudar: o alfabeto devanágari, suas vogais, consoantes e matras com a ordem dos traços, palavras para ler e ouvir, prática, revisões e provas. Lições tem aulas gratuitas, e os PDFs, fotos e páginas que você digitalizar, para ler e escrever por cima.',
  '左上のメニューに学習のすべてがあります。デーヴァナーガリー文字表、筆順つきの母音・子音・母音記号、読んで聞ける単語、練習、復習、試験。教材には無料のレッスンと、自分のPDF・写真・スキャンしたページがあり、読んだり書き込んだりできます。',
  'Le menu en haut à gauche a tout pour étudier : l’alphabet devanagari, ses voyelles, consonnes et matras avec leur ordre des traits, des mots à lire et à écouter, l’entraînement, les révisions et les examens. Leçons a des cours gratuits, et les PDF, photos et pages que vous numérisez, à lire et sur lesquels écrire.')
t('WELCOME_7_TITLE',
  'Your progress',
  'Tu progreso',
  'Seu progresso',
  '学習の記録',
  'Vos progrès')
t('WELCOME_7',
  'Statistics shows your streak, the days you wrote, your scores week by week, your exams and the characters that need more work; the Album keeps every character you practised, with each try. It all stays on this device: Settings › Your data makes a backup.',
  'Las Estadísticas muestran tu racha, los días que escribiste, tus puntuaciones semana a semana, tus exámenes y los caracteres que necesitan más trabajo; el Álbum guarda cada carácter que practicaste, con cada intento. Todo se queda en este dispositivo: Ajustes › Tus datos hace una copia de seguridad.',
  'As Estatísticas mostram sua sequência, os dias em que você escreveu, suas pontuações semana a semana, seus testes e os caracteres que precisam de mais prática; o Álbum guarda cada caractere praticado, com cada tentativa. Tudo fica neste dispositivo: Configurações › Seus dados faz um backup.',
  '統計では、連続日数、書いた日、週ごとの点数、テスト、もっと練習が必要な字がわかります。アルバムには練習したすべての字が、毎回の記録とともに残ります。すべてこの端末に保存され、設定 › データ でバックアップできます。',
  'Les Statistiques montrent votre série, les jours où vous avez écrit, vos notes semaine par semaine, vos examens et les caractères à retravailler ; l’Album garde chaque caractère pratiqué, avec chaque essai. Tout reste sur cet appareil : Réglages › Vos données fait une sauvegarde.')

# The lectures (the Library's books: doc.h).
t('LECTURE_PHRASEBOOK',
  'Hindi phrasebook',
  'Guía de hindi',
  'Guia de conversação hindi',
  'ヒンディー語会話集',
  'Guide linguistique hindi')
t('LECTURE_PHRASEBOOK_ABOUT',
  'Hindi, Devanagari and pronunciation, a little grammar, and the phrases a trip needs (greetings, numbers, time, getting around, eating, shopping), with their romanization. From Wikivoyage.',
  'El hindi, el devanagari y la pronunciación, un poco de gramática y las frases de un viaje (saludos, números, horas, transporte, comida, compras), con su romanización. De Wikiviajes.',
  'O hindi, o devanágari e a pronúncia, um pouco de gramática e as frases de que uma viagem precisa (cumprimentos, números, horas, transporte, comida, compras), com a romanização. Do Wikivoyage.',
  'ヒンディー語、デーヴァナーガリー文字と発音、少しの文法、旅に必要なフレーズ（あいさつ、数字、時間、交通、食事、買い物）をローマ字つきで。Wikivoyageより。',
  'Le hindi, le devanagari et la prononciation, un peu de grammaire et les phrases d’un voyage (salutations, nombres, heures, transports, nourriture, achats), avec leur romanisation. De Wikivoyage.')

# The fourth language: Hindi (the one Hindi teaches) in place of Japanese, once
# every string has one: each layer's strings_hi.py beside its strings.py, and this
# tool's own (tools/strings/build.py: fourth).
fourth('HI-IN', 'Hindi', ['@plural = 1'], [
    os.path.join(ROOT, 'fude', 'drawing', 'strings_hi.py'),
    os.path.join(ROOT, 'fude', 'study', 'strings_hi.py'),
    os.path.join(ROOT, 'fude', 'lang', 'hi', 'strings_hi.py'),
    os.path.join(HERE, 'strings_hi.py'),
])

if __name__ == '__main__':
    write(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..'), 'apps/hindi/tools/strings.py')
