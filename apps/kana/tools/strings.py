# Kana's UI strings: English and four translations. Every text the app shows is
# in the layers' files and here, and only there: the core's (fude/drawing/
# strings.py), the study's (fude/study/strings.py), Japanese's
# (fude/lang/ja/strings.py), then Kana's own below. Run
# after changing one:
#
#   python3 apps/kana/tools/strings.py
#
# It checks every language has every id with the same placeholders, then writes
#   apps/kana/src/text_ids.h          the ids, in order (FUDE_TEXT_ID(X) lines)
#   apps/kana/assets/text/strings.rdel   RDE's localization file, one block per language
# (both generated: edit the strings' files, not them). tools/strings/build.py has
# how a row is written.
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.dont_write_bytecode = True   # no __pycache__ left in tools/strings/
sys.path.insert(0, os.path.join(ROOT, 'tools', 'strings'))
from build import t, o, P, layer, write

layer(os.path.join(ROOT, 'fude', 'drawing', 'strings.py'))
layer(os.path.join(ROOT, 'fude', 'study', 'strings.py'))
layer(os.path.join(ROOT, 'fude', 'lang', 'ja', 'strings.py'))

# --- common words ---------------------------------------------------------------
t('KANA', 'Kana', 'Kana', 'Kana', 'かな', 'Kana')

# --- the side panel and Settings ---------------------------------------------------------------
t('LICENCE_LECTURES', 'Lectures', 'Lecciones', 'Lições', '教材', 'Leçons')
t('CREDITS', 'Stroke order from KanjiVG (Ulrich Apel); readings, meanings and example words from KANJIDIC2 and JMdict (EDRDG); example sentences from Tatoeba (tatoeba.org, CC BY 2.0 FR); JLPT levels from Jonathan Waller’s lists. Handwriting and text recognition by Google ML Kit, which sends Google anonymous usage data while it is on. Every licence in full: Licences.',
  'Orden de trazos de KanjiVG (Ulrich Apel); lecturas, significados y palabras de ejemplo de KANJIDIC2 y JMdict (EDRDG); frases de ejemplo de Tatoeba (tatoeba.org, CC BY 2.0 FR); niveles JLPT de las listas de Jonathan Waller. Reconocimiento de escritura y de texto de Google ML Kit, que envía a Google datos de uso anónimos mientras está activado. Todas las licencias completas: Licencias.',
  'Ordem dos traços do KanjiVG (Ulrich Apel); leituras, significados e palavras de exemplo do KANJIDIC2 e do JMdict (EDRDG); frases de exemplo do Tatoeba (tatoeba.org, CC BY 2.0 FR); níveis do JLPT das listas de Jonathan Waller. Reconhecimento de escrita e de texto do Google ML Kit, que envia ao Google dados de uso anônimos enquanto está ligado. Todas as licenças completas: Licenças.',
  '書き順：KanjiVG（Ulrich Apel）。読み・意味・例語：KANJIDIC2とJMdict（EDRDG）。例文：Tatoeba（tatoeba.org、CC BY 2.0 FR）。JLPTレベル：Jonathan Wallerのリスト。手書き・文字認識：Google ML Kit（オンの間、匿名の利用データがGoogleに送信されます）。すべてのライセンスの全文：ライセンス。',
  'Ordre des traits : KanjiVG (Ulrich Apel) ; lectures, sens et mots d’exemple : KANJIDIC2 et JMdict (EDRDG) ; phrases d’exemple : Tatoeba (tatoeba.org, CC BY 2.0 FR) ; niveaux JLPT : listes de Jonathan Waller. Reconnaissance de l’écriture et du texte : Google ML Kit, qui envoie à Google des données d’utilisation anonymes tant qu’il est activé. Toutes les licences complètes : Licences.')

# The welcome (welcome.h): the first time Kana opens, and from Settings.
t('WELCOME', 'Welcome', 'Bienvenida', 'Boas-vindas', 'ようこそ', 'Bienvenue')
t('WELCOME_SKIP', 'Skip', 'Saltar', 'Pular', 'スキップ', 'Passer')
t('WELCOME_START', 'Start writing', 'Empezar a escribir', 'Começar a escrever', '書きはじめる', 'Commencer à écrire')
t('WELCOME_1_TITLE', 'Welcome to {APP}', 'Te damos la bienvenida a {APP}', 'Boas-vindas ao {APP}', 'Kanaへようこそ', 'Bienvenue dans {APP}')
t('WELCOME_1', 'Write Japanese by hand, the way it is written: {APP} shows every character’s strokes in order, reads what you write and tells you how well it went. It works offline, and everything stays on this device.', 'Escribe japonés a mano, como se escribe: {APP} muestra los trazos de cada carácter en orden, lee lo que escribes y te dice qué tal ha salido. Funciona sin conexión y todo se queda en este dispositivo.', 'Escreva japonês à mão, do jeito que se escreve: o {APP} mostra os traços de cada caractere em ordem, lê o que você escreve e diz como ficou. Funciona offline, e tudo fica neste dispositivo.', '日本語を、書かれるとおりに手で書きましょう。Kanaは一文字ずつ筆順を示し、書いた字を読み取って、出来ばえを教えてくれます。オフラインで動き、すべてこの端末に残ります。', 'Écrivez le japonais à la main, comme il s’écrit : {APP} montre les traits de chaque caractère dans l’ordre, lit ce que vous écrivez et vous dit comment c’était. Il fonctionne hors ligne, et tout reste sur cet appareil.')
t('WELCOME_2_TITLE', 'Pencil or finger', 'Pencil o dedo', 'Pencil ou dedo', 'Pencilでも指でも', 'Pencil ou doigt')
t('WELCOME_2', 'Write with the Apple Pencil — fingers then move the page — or, with the hand on the bar, with one finger, and move the page with two. {APP} switches by itself when the Pencil writes. The tools are on a bar you can drag anywhere; a double tap on its handle folds it away.',
  'Escribe con el Apple Pencil —los dedos mueven entonces la página— o, con la mano de la barra, con un dedo, y mueve la página con dos. {APP} cambia sola cuando escribe el Pencil. Las herramientas están en una barra que puedes arrastrar adonde quieras; un doble toque en su asa la pliega.',
  'Escreva com o Apple Pencil — os dedos então movem a página — ou, com a mão da barra, com um dedo, e mova a página com dois. O {APP} muda sozinho quando o Pencil escreve. As ferramentas ficam numa barra que você arrasta para onde quiser; um toque duplo na alça a recolhe.',
  'Apple Pencilで書く（ページは指で動かします）か、バーの手のボタンで指1本で書き、ページは2本指で動かします。Pencilで書くと自動で切り替わります。道具はどこへでもドラッグできるバーにあり、取っ手をダブルタップすると畳めます。',
  'Écrivez à l’Apple Pencil — les doigts déplacent alors la page — ou, avec la main de la barre, d’un doigt, et déplacez la page à deux doigts. {APP} bascule d’elle-même quand le Pencil écrit. Les outils sont sur une barre à déplacer où vous voulez ; un double toucher sur sa poignée la replie.')
t('WELCOME_3_TITLE', 'Read with the camera', 'Lee con la cámara', 'Leia com a câmera', 'カメラで読む', 'Lire avec l’appareil photo')
t('WELCOME_3', 'Tap the camera on the bar, or press and hold the page for Text from photo, and point it at Japanese text: a book, a sign, a menu. {APP} reads it live and boxes each line: Hold keeps them, Write puts them on the page as handwriting to trace, and Translate with Google shows them in your language. A photo works too.',
  'Toca la cámara de la barra, o mantén pulsada la página y elige Texto de foto, y apunta a un texto en japonés: un libro, un cartel, una carta. {APP} lo lee en directo y enmarca cada línea: Congelar las conserva, Escribir las pone en la página como escritura a mano para repasar, y Traducir con Google las muestra en tu idioma. También sirve una foto.',
  'Toque na câmera da barra, ou toque e segure a página e escolha Texto de foto, e aponte para um texto em japonês: um livro, uma placa, um cardápio. O {APP} lê ao vivo e destaca cada linha: Congelar as mantém, Escrever as coloca na página como escrita à mão para cobrir, e Traduzir com o Google as mostra no seu idioma. Uma foto também serve.',
  'バーのカメラをタップするか、ページを長押しして「写真の文字」を選び、日本語の本や看板、メニューに向けます。{APP}がその場で読み取り、行ごとに枠を付けます。「固定」で残し、「書く」でなぞれる手書きとしてページに書き、「Googleで翻訳」で自分の言語に訳せます。写真からもできます。',
  'Touchez l’appareil photo de la barre, ou appuyez longuement sur la page et choisissez Texte d’une photo, puis visez un texte japonais : un livre, un panneau, un menu. {APP} le lit en direct et encadre chaque ligne : Figer les garde, Écrire les pose sur la page en écriture à la main à repasser, et Traduire avec Google les affiche dans votre langue. Une photo marche aussi.')
t('WELCOME_4_TITLE', 'Copy, paste, check', 'Copia, pega, revisa', 'Copie, cole, confira', 'コピー・貼り付け・チェック', 'Copier, coller, vérifier')
t('WELCOME_4', 'Draw around writing with the lasso: copy it, Copy as text to use it in any app, Check what it says, or Save word. Press and hold the page to paste: Paste text writes text copied anywhere as handwriting, stroke by stroke.',
  'Rodea lo escrito con el lazo: cópialo, usa Copiar como texto para llevarlo a cualquier app, Revisar lo que dice, o Guardar palabra. Mantén pulsada la página para pegar: Pegar texto escribe a mano, trazo a trazo, un texto copiado en cualquier sitio.',
  'Contorne a escrita com o laço: copie, use Copiar como texto para levá-la a qualquer app, Verificar o que diz, ou Salvar palavra. Toque e segure a página para colar: Colar texto escreve à mão, traço a traço, um texto copiado em qualquer lugar.',
  '書いた字を投げなわで囲むと、コピー、ほかのアプリで使える「テキストでコピー」、読み取りを確かめる「チェック」、「単語を保存」ができます。ページを長押しすると貼り付け：「テキストを貼り付け」は、どこかでコピーした文を一画ずつ手書きで書きます。',
  'Entourez l’écriture au lasso : copiez-la, Copier en texte pour l’utiliser dans n’importe quelle app, Vérifier ce qu’elle dit, ou Enregistrer un mot. Appuyez longuement sur la page pour coller : Coller le texte écrit à la main, trait par trait, un texte copié n’importe où.')
t('WELCOME_5_TITLE', 'Translate', 'Traduce', 'Traduza', '翻訳', 'Traduire')
t('WELCOME_5', 'Lasso your writing and choose Translate with Google to read it in your language. In Vocabulary, Translate with Google goes the other way: type in your language and get it in Japanese, with its words to save, practise or write. The translation models download once, then work offline.',
  'Rodea lo que escribiste con el lazo y elige Traducir con Google para leerlo en tu idioma. En Vocabulario, Traducir con Google va al revés: escribe en tu idioma y obtén la traducción al japonés, con sus palabras para guardar, practicar o escribir. Los modelos de traducción se descargan una vez y luego funcionan sin conexión.',
  'Contorne a sua escrita com o laço e escolha Traduzir com o Google para lê-la no seu idioma. Em Vocabulário, Traduzir com o Google faz o caminho inverso: digite no seu idioma e receba a tradução para o japonês, com as palavras para salvar, praticar ou escrever. Os modelos de tradução são baixados uma vez e depois funcionam offline.',
  '書いた字を投げなわで囲み「Googleで翻訳」を選ぶと、自分の言語で読めます。単語帳の「Googleで翻訳」はその逆で、自分の言語で入力すると日本語に訳し、含まれる単語を保存・練習・ページに書けます。翻訳モデルは一度ダウンロードすれば、オフラインで使えます。',
  'Entourez votre écriture au lasso et choisissez Traduire avec Google pour la lire dans votre langue. Dans Vocabulaire, Traduire avec Google fait l’inverse : écrivez dans votre langue et obtenez la traduction en japonais, avec ses mots à enregistrer, pratiquer ou écrire. Les modèles de traduction se téléchargent une fois, puis marchent hors ligne.')
t('WELCOME_6_TITLE', 'Learn', 'Aprende', 'Aprenda', '学ぶ', 'Apprendre')
t('WELCOME_6', 'The menu at the top left has everything to study: kanji and kana with their stroke order, words to read and listen to, practice, reviews and exams. Lectures has free lessons, and the PDFs, photos and pages you scan, to read and write on.',
  'El menú de arriba a la izquierda tiene todo para estudiar: kanji y kana con su orden de trazos, palabras para leer y escuchar, práctica, repasos y exámenes. Lecciones tiene clases gratuitas, y los PDF, fotos y páginas que escanees, para leer y escribir encima.',
  'O menu no canto superior esquerdo tem tudo para estudar: kanji e kana com a ordem dos traços, palavras para ler e ouvir, prática, revisões e testes. Lições tem aulas gratuitas, e os PDFs, fotos e páginas que você escanear, para ler e escrever por cima.',
  '左上のメニューに学習のすべてがあります：筆順つきの漢字とかな、読んで聞ける単語、練習、復習、テスト。教材には無料の教材と、取り込んだPDF・写真・スキャンしたページがあり、読みながら書き込めます。',
  'Le menu en haut à gauche a tout pour étudier : kanji et kana avec leur ordre des traits, des mots à lire et à écouter, l’entraînement, les révisions et les examens. Leçons a des cours gratuits, et les PDF, photos et pages que vous numérisez, à lire et sur lesquels écrire.')
t('WELCOME_7_TITLE', 'Your progress', 'Tu progreso', 'Seu progresso', '学習の記録', 'Vos progrès')
t('WELCOME_7', 'Statistics shows your streak, the days you wrote, your scores week by week, your exams and the characters that need more work; the Album keeps every character you practised, with each try. It all stays on this device: Settings › Your data makes a backup.',
  'Las Estadísticas muestran tu racha, los días que escribiste, tus puntuaciones semana a semana, tus exámenes y los caracteres que necesitan más trabajo; el Álbum guarda cada carácter que practicaste, con cada intento. Todo se queda en este dispositivo: Ajustes › Tus datos hace una copia de seguridad.',
  'As Estatísticas mostram sua sequência, os dias em que você escreveu, suas pontuações semana a semana, seus testes e os caracteres que precisam de mais prática; o Álbum guarda cada caractere praticado, com cada tentativa. Tudo fica neste dispositivo: Configurações › Seus dados faz um backup.',
  '統計では、連続日数、書いた日、週ごとの点数、テスト、もっと練習が必要な字がわかります。アルバムには練習したすべての字が、毎回の記録とともに残ります。すべてこの端末に保存され、設定 › データ でバックアップできます。',
  'Les Statistiques montrent votre série, les jours où vous avez écrit, vos notes semaine par semaine, vos examens et les caractères à retravailler ; l’Album garde chaque caractère pratiqué, avec chaque essai. Tout reste sur cet appareil : Réglages › Vos données fait une sauvegarde.')

# --- the lectures (kana_app.c: the Library's books) ----------------------------------------------
t('LECTURE_JPN101', 'First Year Japanese I', 'First Year Japanese I', 'First Year Japanese I', 'First Year Japanese I', 'First Year Japanese I')
t('LECTURE_JPN101_ABOUT', 'Kana charts, how each stroke ends, writing hiragana and katakana, greetings, particles and verb forms, with worksheets to write on. From a first-year college course, in English.',
  'Tablas de kana, cómo termina cada trazo, escritura de hiragana y katakana, saludos, partículas y formas verbales, con hojas de ejercicios para escribir encima. De un curso universitario de primer año, en inglés.',
  'Tabelas de kana, como termina cada traço, escrita de hiragana e katakana, cumprimentos, partículas e formas verbais, com folhas de exercícios para escrever por cima. De um curso universitário de primeiro ano, em inglês.',
  'かなの表、とめ・はね・はらい、ひらがなとカタカナの書き方、あいさつ、助詞、動詞の形。書き込める練習シート付き。大学1年生の講座の教材です（英語）。',
  'Tableaux des kana, la fin de chaque trait, écriture des hiragana et katakana, salutations, particules et formes verbales, avec des fiches d’exercices sur lesquelles écrire. D’un cours universitaire de première année, en anglais.')

t('LECTURE_PHRASEBOOK', 'Japanese phrasebook', 'Guía de japonés', 'Guia de conversação japonês', '日本語フレーズ集（英語）', 'Guide linguistique japonais')
t('LECTURE_PHRASEBOOK_ABOUT', 'Pronunciation, a little grammar, and the phrases a trip needs (greetings, numbers, time, getting around, eating, shopping) in Japanese, with their readings. From Wikivoyage.',
  'Pronunciación, algo de gramática y las frases de un viaje (saludos, números, horas, transporte, comida) en japonés, con su lectura. De Wikiviajes.',
  'Pronúncia, um pouco de gramática e as frases de uma viagem (cumprimentos, números, horas, transporte, comida, compras) em japonês, com a leitura. Do Wikivoyage.',
  '発音、少しの文法、旅に必要なフレーズ（あいさつ、数字、時間、交通、食事、買い物）を読み方つきで。Wikivoyageより（英語）。',
  'La prononciation et les phrases d’un voyage (salutations, nombres, heures, transports, nourriture, achats) en japonais, avec leur lecture. De Wikivoyage.')

if __name__ == '__main__':
    write(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..'), 'apps/kana/tools/strings.py')
