# Kana's UI strings: English and four translations. Every text the app shows is
# in the layers' files and here, and only there: the core's (fude/drawing/
# strings.py), the study's (fude/study/strings.py), then Kana's own below. Run
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
t('WELCOME_1_TITLE', 'Welcome to Kana', 'Te damos la bienvenida a Kana', 'Boas-vindas ao Kana', 'Kanaへようこそ', 'Bienvenue dans Kana')
t('WELCOME_1', 'Write Japanese by hand, the way it is written: Kana shows every character’s strokes in order, reads what you write and tells you how well it went. It works offline, and everything stays on this device.', 'Escribe japonés a mano, como se escribe: Kana muestra los trazos de cada carácter en orden, lee lo que escribes y te dice qué tal ha salido. Funciona sin conexión y todo se queda en este dispositivo.', 'Escreva japonês à mão, do jeito que se escreve: o Kana mostra os traços de cada caractere em ordem, lê o que você escreve e diz como ficou. Funciona offline, e tudo fica neste dispositivo.', '日本語を、書かれるとおりに手で書きましょう。Kanaは一文字ずつ筆順を示し、書いた字を読み取って、出来ばえを教えてくれます。オフラインで動き、すべてこの端末に残ります。', 'Écrivez le japonais à la main, comme il s’écrit : Kana montre les traits de chaque caractère dans l’ordre, lit ce que vous écrivez et vous dit comment c’était. Il fonctionne hors ligne, et tout reste sur cet appareil.')
t('WELCOME_2_TITLE', 'Pencil or finger', 'Pencil o dedo', 'Pencil ou dedo', 'Pencilでも指でも', 'Pencil ou doigt')
t('WELCOME_2', 'Write with the Apple Pencil — fingers then move the page — or, with the hand on the bar, with one finger, and move the page with two. Kana switches by itself when the Pencil writes. The tools are on a bar you can drag anywhere; a double tap on its handle folds it away.',
  'Escribe con el Apple Pencil —los dedos mueven entonces la página— o, con la mano de la barra, con un dedo, y mueve la página con dos. Kana cambia sola cuando escribe el Pencil. Las herramientas están en una barra que puedes arrastrar adonde quieras; un doble toque en su asa la pliega.',
  'Escreva com o Apple Pencil — os dedos então movem a página — ou, com a mão da barra, com um dedo, e mova a página com dois. O Kana muda sozinho quando o Pencil escreve. As ferramentas ficam numa barra que você arrasta para onde quiser; um toque duplo na alça a recolhe.',
  'Apple Pencilで書く（ページは指で動かします）か、バーの手のボタンで指1本で書き、ページは2本指で動かします。Pencilで書くと自動で切り替わります。道具はどこへでもドラッグできるバーにあり、取っ手をダブルタップすると畳めます。',
  'Écrivez à l’Apple Pencil — les doigts déplacent alors la page — ou, avec la main de la barre, d’un doigt, et déplacez la page à deux doigts. Kana bascule d’elle-même quand le Pencil écrit. Les outils sont sur une barre à déplacer où vous voulez ; un double toucher sur sa poignée la replie.')
t('WELCOME_3_TITLE', 'Long press, and the lasso', 'Pulsación larga y el lazo', 'Toque longo e o laço', '長押しと投げなわ', 'L’appui long et le lasso')
t('WELCOME_3', 'Press and hold on the page for its menu: paste, paste text written as handwriting, text from a photo or the camera. Draw around writing with the lasso to copy it, copy it as text, translate it or check it.', 'Mantén pulsada la página para ver su menú: pegar, pegar texto escrito a mano, texto de una foto o de la cámara. Rodea lo escrito con el lazo para copiarlo, copiarlo como texto, traducirlo o revisarlo.', 'Toque e segure a página para ver o menu: colar, colar texto escrito à mão, texto de uma foto ou da câmera. Contorne a escrita com o laço para copiá-la, copiá-la como texto, traduzi-la ou conferi-la.', 'ページを長押しするとメニューが出ます：貼り付け、テキストを手書きで貼り付け、写真やカメラの文字。書いた字を投げなわで囲むと、コピー、テキストとしてコピー、翻訳、チェックができます。', 'Appuyez longuement sur la page pour son menu : coller, coller du texte écrit à la main, le texte d’une photo ou de l’appareil photo. Entourez l’écriture au lasso pour la copier, la copier en texte, la traduire ou la vérifier.')
t('WELCOME_4_TITLE', 'Learn', 'Aprende', 'Aprenda', '学ぶ', 'Apprendre')
t('WELCOME_4', 'The menu at the top left has everything to study: kanji and kana with their stroke order, words to read and listen to, practice, reviews, exams, your album and statistics — and Settings, where your data is.', 'El menú de arriba a la izquierda tiene todo para estudiar: kanji y kana con su orden de trazos, palabras para leer y escuchar, práctica, repasos, exámenes, tu álbum y estadísticas, y los Ajustes, donde están tus datos.', 'O menu no canto superior esquerdo tem tudo para estudar: kanji e kana com a ordem dos traços, palavras para ler e ouvir, prática, revisões, provas, seu álbum e estatísticas, e os Ajustes, onde ficam seus dados.', '左上のメニューに学習のすべてがあります：筆順つきの漢字とかな、読んで聞ける単語、練習、復習、テスト、アルバム、統計。そしてデータのある設定も。', 'Le menu en haut à gauche a tout pour étudier : kanji et kana avec leur ordre des traits, des mots à lire et à écouter, l’entraînement, les révisions, les examens, votre album et les statistiques, et les Réglages, où sont vos données.')

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
