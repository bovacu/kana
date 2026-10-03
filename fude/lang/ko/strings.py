# Korean's UI strings (lang.h): the study's ids that name the language, in
# its words, and the ids fude/lang/ko/lang.c and chart.c use. A study app's
# strings tool reads this after fude/study/strings.py (as fude/lang/ja/strings.py,
# whose study ids these are).
#
#   t('ID', English, Spanish, Portuguese (Brazil), Japanese, French)

# The kinds of character (lang.c's groups), their exam prompts and the readings' sort chips.
t('KO_JAMO', 'Jamo', 'Jamo', 'Jamo', '字母', 'Jamo')
t('KO_SYLLABLES', 'Syllables', 'Sílabas', 'Sílabas', '音節', 'Syllabes')
t('KO_HANJA', 'Hanja', 'Hanja', 'Hanja', '漢字', 'Hanja')
t('EXAM_WRITE_JAMO', 'WRITE THE LETTER', 'ESCRIBE LA LETRA', 'ESCREVA A LETRA', 'この字母を書く', 'ÉCRIVEZ LA LETTRE')
t('EXAM_WRITE_HANGUL', 'WRITE IN HANGUL', 'ESCRIBE EN HANGUL', 'ESCREVA EM HANGUL', 'ハングルで書く', 'ÉCRIVEZ EN HANGUL')
t('EXAM_WRITE_HANJA', 'WRITE THE HANJA FOR', 'ESCRIBE EL HANJA DE', 'ESCREVA O HANJA DE', 'この意味の漢字を書く', 'ÉCRIVEZ LE HANJA DE')
t('SORT_EUM', 'Eum', 'Eum', 'Eum', '音', 'Eum')
t('SORT_HUN', 'Hun', 'Hun', 'Hun', '訓', 'Hun')

# The levels (lang.c): the Basic Korean Dictionary's, after their Korean names.
t('KO_LEVEL_1', 'Beginner', 'Inicial', 'Iniciante', '初級', 'Débutant')
t('KO_LEVEL_2', 'Intermediate', 'Intermedio', 'Intermediário', '中級', 'Intermédiaire')
t('KO_LEVEL_3', 'Advanced', 'Avanzado', 'Avançado', '上級', 'Avancé')

# The Hangul chart (chart.c): its sections.
t('CHART_CONSONANTS', 'Consonants', 'Consonantes', 'Consoantes', '子音', 'Consonnes')
t('CHART_VOWELS', 'Vowels', 'Vocales', 'Vogais', '母音', 'Voyelles')
t('CHART_SYLLABLES', 'Syllables: a consonant and a vowel', 'Sílabas: una consonante y una vocal', 'Sílabas: uma consoante e uma vogal', '音節：子音と母音', 'Syllabes : une consonne et une voyelle')

# Text from a photo (scan.h).
t('SCAN_HINT', 'Point the camera at Korean text, or choose a photo: what is read is boxed, and the lines you keep are written on the page.', 'Apunta la cámara a un texto en coreano o elige una foto: lo que se lee aparece enmarcado y las líneas que elijas se escriben en la página.', 'Aponte a câmera para um texto em coreano ou escolha uma foto: o que é lido fica destacado, e as linhas que você escolher são escritas na página.', 'カメラを韓国語に向けるか、写真を選んでください。読み取った行に枠が付き、選んだ行をページに書きます。', 'Pointez l’appareil photo vers un texte coréen ou choisissez une photo : ce qui est lu est encadré, et les lignes gardées sont écrites sur la page.')
t('SCAN_LIVE_NONE', 'Point the camera at Korean text', 'Apunta la cámara a un texto en coreano', 'Aponte a câmera para um texto em coreano', 'カメラを韓国語に向けてください', 'Pointez l’appareil photo vers un texte coréen')
t('SCAN_NONE', 'No Korean text found in this photo.', 'No se encontró texto coreano en esta foto.', 'Nenhum texto em coreano encontrado nesta foto.', 'この写真に韓国語は見つかりませんでした。', 'Aucun texte coréen trouvé sur cette photo.')

# Into Korean (translator.h): the Vocabulary's Translate with Google, from the reader's language.
t('TRANSLATOR_TITLE', 'Into Korean', 'Al coreano', 'Para o coreano', '韓国語に翻訳', 'Vers le coréen')
t('TRANSLATOR_EMPTY', 'Type a word or a sentence in your language in the field at the top right, then Return: Google translates it into Korean, with the words in it — tap one to save it. Then save the whole of it, write it on the page or practise its letters.',
  'Escribe una palabra o una frase en tu idioma en el campo de arriba a la derecha y pulsa Intro: Google la traduce al coreano, con las palabras que contiene; toca una para guardarla. Después guárdala entera, escríbela en la página o practica sus letras.',
  'Digite uma palavra ou uma frase no seu idioma no campo no canto superior direito e toque em Retorno: o Google a traduz para o coreano, com as palavras que ela contém — toque em uma para salvá-la. Depois salve-a inteira, escreva-a na página ou pratique suas letras.',
  '右上の欄に単語や文を入力して改行すると、Googleが韓国語に翻訳し、含まれる単語も表示します（タップで保存）。そのまま保存したり、ページに書いたり、文字を練習したりできます。',
  'Écrivez un mot ou une phrase dans votre langue dans le champ en haut à droite, puis Entrée : Google le traduit en coréen, avec les mots qu’il contient — touchez-en un pour l’enregistrer. Ensuite, enregistrez-le en entier, écrivez-le sur la page ou entraînez-vous à ses lettres.')

# The page: Paste text, and Check's field.
t('NOTICE_NOTHING_TO_WRITE', 'No Korean in the clipboard to write', 'No hay coreano en el portapapeles para escribir', 'Não há coreano na área de transferência para escrever', '書ける韓国語がクリップボードにありません', 'Aucun coréen à écrire dans le presse-papiers')
t('CHECK_FIELD', 'I meant… (hakgyo, or Korean)', 'Quise decir… (hakgyo, o coreano)', 'Eu quis dizer… (hakgyo, ou coreano)', '書きたかった言葉…（hakgyo、または韓国語）', 'Je voulais écrire… (hakgyo, ou en coréen)')

# The word card (wordcard.h), the characters' search and the Vocabulary's.
t('WORD_FIELD_READING', 'Romanization (hakgyo): blank, from the Hangul', 'Romanización (hakgyo): vacía, la del hangul', 'Romanização (hakgyo): vazia, a do hangul', 'ローマ字表記（hakgyo）：空欄ならハングルから', 'Romanisation (hakgyo) : vide, celle du hangul')
t('WORD_ERR_READING', 'Its romanization', 'Falta su romanización', 'Falta a romanização', 'ローマ字表記を', 'Sa romanisation')
t('WORD_ERR_ROMAJI', 'That romanization is not one {APP} knows', '{APP} no reconoce esa romanización', 'O {APP} não reconhece essa romanização', 'そのローマ字表記は読み取れません', '{APP} ne reconnaît pas cette romanisation')
t('WORD_TRANSLATE_HINT', 'Your meaning, into Korean', 'Tu significado, al coreano', 'O seu significado, para o coreano', '意味（英語）を韓国語に', 'Votre sens, en coréen')
o('SEARCH_HINT', 'A meaning or a sound: sky, cheon', 'Un significado o un sonido: cielo, cheon', 'Um significado ou um som: céu, cheon', '意味（英語）か音：sky、cheon', 'Un sens ou un son : ciel, cheon')
t('VOCAB_SEARCH_HINT', 'Find: Hangul, romanization or meaning', 'Buscar: hangul, romanización o significado', 'Buscar: hangul, romanização ou significado', '検索：ハングル、ローマ字、意味', 'Chercher : hangul, romanisation ou sens')

# Google ML Kit's handwriting model (mlkit.h).
t('MLKIT_READY', 'Ready: the Korean model is on this device.', 'Listo: el modelo coreano está en este dispositivo.', 'Pronto: o modelo coreano está neste dispositivo.', '準備完了：韓国語モデルはこの端末にあります。', 'Prêt : le modèle coréen est sur cet appareil.')
t('MLKIT_DOWNLOADING', 'Downloading the Korean model (about 20 MB)...', 'Descargando el modelo coreano (unos 20 MB)...', 'Baixando o modelo coreano (cerca de 20 MB)...', '韓国語モデルをダウンロード中（約20 MB）…', 'Téléchargement du modèle coréen (environ 20 Mo)...')
t('MLKIT_FAILED', 'The Korean model could not be downloaded. Is the device online?', 'No se pudo descargar el modelo coreano. ¿Hay conexión?', 'Não foi possível baixar o modelo coreano. O dispositivo está conectado?', '韓国語モデルをダウンロードできませんでした。インターネットに接続されていますか？', 'Impossible de télécharger le modèle coréen. L’appareil est-il connecté ?')
t('MLKIT_MISSING', 'The Korean model (about 20 MB) is not downloaded yet.', 'El modelo coreano (unos 20 MB) aún no está descargado.', 'O modelo coreano (cerca de 20 MB) ainda não foi baixado.', '韓国語モデル（約20 MB）はまだダウンロードされていません。', 'Le modèle coréen (environ 20 Mo) n’est pas encore téléchargé.')

# Read aloud (speech.h): the device's basic voice spoke.
t('SPEECH_BETTER_VOICE', 'For a clearer voice, download a Korean one in Settings › Accessibility › Spoken Content › Voices.', 'Para una voz más clara, descarga una coreana en Ajustes › Accesibilidad › Contenido leído › Voces.', 'Para uma voz mais clara, baixe uma coreana em Ajustes › Acessibilidade › Conteúdo Falado › Vozes.', 'よりきれいな音声は、設定 › アクセシビリティ › 読み上げコンテンツ › 声 から韓国語の声をダウンロードできます。', 'Pour une voix plus claire, téléchargez-en une coréenne dans Réglages › Accessibilité › Contenu énoncé › Voix.')

# Exams, the album, practice sheets.
t('EXAM_INTRO', 'Each character once, from memory: a letter from its romanization, a hanja from its meaning and its sound.', 'Cada carácter una vez, de memoria: una letra a partir de su romanización, un hanja a partir de su significado y su sonido.', 'Cada caractere uma vez, de memória: uma letra a partir da romanização, um hanja a partir do significado e do som.', '各字を1回ずつ、何も見ずに：字母はローマ字から、漢字は意味と音から書きます。', 'Chaque caractère une fois, de mémoire : une lettre d’après sa romanisation, un hanja d’après son sens et son son.')
t('ALBUM_EMPTY', 'Nothing practised yet: open a letter (Characters, or the Hangul chart), then Practice, then Score.', 'Aún no hay nada practicado: abre una letra (Caracteres o la tabla de hangul), luego Practicar y luego Puntuar.', 'Ainda não há nada praticado: abra uma letra (Caracteres ou a tabela de hangul), depois Praticar e depois Avaliar.', 'まだ練習がありません。文字を開いて（文字、またはハングル表）、「練習」、「採点」の順に。', 'Rien de pratiqué pour l’instant : ouvrez une lettre (Caractères, ou le tableau du hangul), puis S’entraîner, puis Noter.')
t('SHEET_CREDIT', 'Hangul stroke order by {APP}; hanja from KanjiVG (CC BY-SA 3.0). Made with {APP}.', 'Orden de trazos del hangul de {APP}; hanja de KanjiVG (CC BY-SA 3.0). Hecho con {APP}.', 'Ordem dos traços do hangul do {APP}; hanja do KanjiVG (CC BY-SA 3.0). Feito com o {APP}.', 'ハングルの書き順：{APP}。漢字：KanjiVG（CC BY-SA 3.0）。{APP}で作成。', 'Ordre des traits du hangul : {APP} ; hanja : KanjiVG (CC BY-SA 3.0). Fait avec {APP}.')
