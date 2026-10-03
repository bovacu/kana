# Chinese's UI strings (lang.h): the study's ids that name the language, in
# its words, and the ids fude/lang/zh/lang.c returns. A study app's strings
# tool reads this after fude/study/strings.py (as fude/lang/ja/strings.py, whose
# study ids these are).
#
#   t('ID', English, Spanish, Portuguese (Brazil), Japanese, French)

# The kinds of character (lang.c's groups), the exam's prompt and the reading's sort chip.
t('ZH_SIMPLIFIED', 'Simplified', 'Simplificado', 'Simplificado', '簡体字', 'Simplifié')
t('ZH_TRADITIONAL', 'Traditional', 'Tradicional', 'Tradicional', '繁体字', 'Traditionnel')
t('EXAM_WRITE_HANZI', 'WRITE THE CHARACTER FOR', 'ESCRIBE EL CARÁCTER DE', 'ESCREVA O CARACTERE DE', 'この意味の漢字を書く', 'ÉCRIVEZ LE CARACTÈRE DE')
t('SORT_PINYIN', 'Pinyin', 'Pinyin', 'Pinyin', 'ピンイン', 'Pinyin')

# Text from a photo (scan.h).
t('SCAN_HINT', 'Point the camera at Chinese text, or choose a photo: what is read is boxed, and the lines you keep are written on the page.', 'Apunta la cámara a un texto en chino o elige una foto: lo que se lee aparece enmarcado y las líneas que elijas se escriben en la página.', 'Aponte a câmera para um texto em chinês ou escolha uma foto: o que é lido fica destacado, e as linhas que você escolher são escritas na página.', 'カメラを中国語に向けるか、写真を選んでください。読み取った行に枠が付き、選んだ行をページに書きます。', 'Pointez l’appareil photo vers un texte chinois ou choisissez une photo : ce qui est lu est encadré, et les lignes gardées sont écrites sur la page.')
t('SCAN_LIVE_NONE', 'Point the camera at Chinese text', 'Apunta la cámara a un texto en chino', 'Aponte a câmera para um texto em chinês', 'カメラを中国語に向けてください', 'Pointez l’appareil photo vers un texte chinois')
t('SCAN_NONE', 'No Chinese text found in this photo.', 'No se encontró texto chino en esta foto.', 'Nenhum texto em chinês encontrado nesta foto.', 'この写真に中国語は見つかりませんでした。', 'Aucun texte chinois trouvé sur cette photo.')

# Into Chinese (translator.h): the Vocabulary's Translate with Google, from the reader's language.
t('TRANSLATOR_TITLE', 'Into Chinese', 'Al chino', 'Para o chinês', '中国語に翻訳', 'Vers le chinois')
t('TRANSLATOR_EMPTY', 'Type a word or a sentence in your language in the field at the top right, then Return: Google translates it into Chinese, with the words in it — tap one to save it. Then save the whole of it, write it on the page or practise its characters.',
  'Escribe una palabra o una frase en tu idioma en el campo de arriba a la derecha y pulsa Intro: Google la traduce al chino, con las palabras que contiene; toca una para guardarla. Después guárdala entera, escríbela en la página o practica sus caracteres.',
  'Digite uma palavra ou uma frase no seu idioma no campo no canto superior direito e toque em Retorno: o Google a traduz para o chinês, com as palavras que ela contém — toque em uma para salvá-la. Depois salve-a inteira, escreva-a na página ou pratique seus caracteres.',
  '右上の欄に単語や文を入力して改行すると、Googleが中国語に翻訳し、含まれる単語も表示します（タップで保存）。そのまま保存したり、ページに書いたり、字を練習したりできます。',
  'Écrivez un mot ou une phrase dans votre langue dans le champ en haut à droite, puis Entrée : Google le traduit en chinois, avec les mots qu’il contient — touchez-en un pour l’enregistrer. Ensuite, enregistrez-le en entier, écrivez-le sur la page ou entraînez-vous à ses caractères.')

# The page: Paste text, and Check's field.
t('NOTICE_NOTHING_TO_WRITE', 'No Chinese in the clipboard to write', 'No hay chino en el portapapeles para escribir', 'Não há chinês na área de transferência para escrever', '書ける中国語がクリップボードにありません', 'Aucun chinois à écrire dans le presse-papiers')
t('CHECK_FIELD', 'I meant… (ni3hao3, or Chinese)', 'Quise decir… (ni3hao3, o chino)', 'Eu quis dizer… (ni3hao3, ou chinês)', '書きたかった言葉…（ni3hao3、または中国語）', 'Je voulais écrire… (ni3hao3, ou en chinois)')

# The word card (wordcard.h), the characters' search and the Vocabulary's.
t('WORD_FIELD_READING', 'Reading: pinyin (ni3hao3 or nǐhǎo)', 'Lectura: pinyin (ni3hao3 o nǐhǎo)', 'Leitura: pinyin (ni3hao3 ou nǐhǎo)', '読み：ピンイン（ni3hao3、nǐhǎo）', 'Lecture : pinyin (ni3hao3 ou nǐhǎo)')
t('WORD_ERR_READING', 'Its reading, in pinyin', 'Falta su lectura, en pinyin', 'Falta a leitura, em pinyin', '読みを、ピンインで', 'Sa lecture, en pinyin')
t('WORD_ERR_ROMAJI', 'That reading is not pinyin {APP} knows', '{APP} no reconoce esa lectura en pinyin', 'O {APP} não reconhece essa leitura em pinyin', 'そのピンインは読み取れません', '{APP} ne reconnaît pas cette lecture en pinyin')
t('WORD_TRANSLATE_HINT', 'Your meaning, into Chinese', 'Tu significado, al chino', 'O seu significado, para o chinês', '意味（英語）を中国語に', 'Votre sens, en chinois')
o('SEARCH_HINT', 'A meaning or a reading: tree, mu', 'Un significado o una lectura: árbol, mu', 'Um significado ou uma leitura: árvore, mu', '意味（英語）か読み：tree、mu', 'Un sens ou une lecture : arbre, mu')
t('VOCAB_SEARCH_HINT', 'Find: hanzi, pinyin or meaning', 'Buscar: hanzi, pinyin o significado', 'Buscar: hanzi, pinyin ou significado', '検索：漢字、ピンイン、意味', 'Chercher : hanzi, pinyin ou sens')

# Google ML Kit's handwriting model (mlkit.h).
t('MLKIT_READY', 'Ready: the Chinese model is on this device.', 'Listo: el modelo chino está en este dispositivo.', 'Pronto: o modelo chinês está neste dispositivo.', '準備完了：中国語モデルはこの端末にあります。', 'Prêt : le modèle chinois est sur cet appareil.')
t('MLKIT_DOWNLOADING', 'Downloading the Chinese model (about 20 MB)...', 'Descargando el modelo chino (unos 20 MB)...', 'Baixando o modelo chinês (cerca de 20 MB)...', '中国語モデルをダウンロード中（約20 MB）…', 'Téléchargement du modèle chinois (environ 20 Mo)...')
t('MLKIT_FAILED', 'The Chinese model could not be downloaded. Is the device online?', 'No se pudo descargar el modelo chino. ¿Hay conexión?', 'Não foi possível baixar o modelo chinês. O dispositivo está conectado?', '中国語モデルをダウンロードできませんでした。インターネットに接続されていますか？', 'Impossible de télécharger le modèle chinois. L’appareil est-il connecté ?')
t('MLKIT_MISSING', 'The Chinese model (about 20 MB) is not downloaded yet.', 'El modelo chino (unos 20 MB) aún no está descargado.', 'O modelo chinês (cerca de 20 MB) ainda não foi baixado.', '中国語モデル（約20 MB）はまだダウンロードされていません。', 'Le modèle chinois (environ 20 Mo) n’est pas encore téléchargé.')

# Read aloud (speech.h): the device's basic voice spoke.
t('SPEECH_BETTER_VOICE', 'For a clearer voice, download a Chinese one in Settings › Accessibility › Spoken Content › Voices.', 'Para una voz más clara, descarga una china en Ajustes › Accesibilidad › Contenido leído › Voces.', 'Para uma voz mais clara, baixe uma chinesa em Ajustes › Acessibilidade › Conteúdo Falado › Vozes.', 'よりきれいな音声は、設定 › アクセシビリティ › 読み上げコンテンツ › 声 から中国語の声をダウンロードできます。', 'Pour une voix plus claire, téléchargez-en une chinoise dans Réglages › Accessibilité › Contenu énoncé › Voix.')

# Exams, the album, practice sheets.
t('EXAM_INTRO', 'Each character once, from memory: from its meaning and its pinyin.', 'Cada carácter una vez, de memoria: a partir de su significado y su pinyin.', 'Cada caractere uma vez, de memória: a partir do significado e do pinyin.', '各字を1回ずつ、何も見ずに：意味とピンインから書きます。', 'Chaque caractère une fois, de mémoire : d’après son sens et son pinyin.')
t('ALBUM_EMPTY', 'Nothing practised yet: open a character (Hanzi), then Practice, then Score.', 'Aún no hay nada practicado: abre un carácter (Hanzi), luego Practicar y luego Puntuar.', 'Ainda não há nada praticado: abra um caractere (Hanzi), depois Praticar e depois Avaliar.', 'まだ練習がありません。字を開いて（漢字）、「練習」、「採点」の順に。', 'Rien de pratiqué pour l’instant : ouvrez un caractère (Hanzi), puis S’entraîner, puis Noter.')
t('SHEET_CREDIT', 'Stroke order from Make Me a Hanzi (Arphic Public License). Made with {APP}.', 'Orden de trazos de Make Me a Hanzi (Arphic Public License). Hecho con {APP}.', 'Ordem dos traços do Make Me a Hanzi (Arphic Public License). Feito com o {APP}.', '書き順: Make Me a Hanzi (Arphic Public License)。{APP}で作成。', 'Ordre des traits : Make Me a Hanzi (Arphic Public License). Fait avec {APP}.')
