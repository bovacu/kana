# Japanese's UI strings (lang.h): the study's ids that name the language, in
# its words, and the ids fude/lang/ja/lang.c returns. A study app's strings
# tool reads this after fude/study/strings.py; another language's strings.py
# has the same study ids, in its own words.
#
#   t('ID', English, Spanish, Portuguese (Brazil), Japanese, French)

# The kinds of character (lang.c's groups), their exam prompts and the readings' sort chips.
t('HIRAGANA', 'Hiragana', 'Hiragana', 'Hiragana', 'ひらがな', 'Hiragana')
t('KATAKANA', 'Katakana', 'Katakana', 'Katakana', 'カタカナ', 'Katakana')
t('KANJI', 'Kanji', 'Kanji', 'Kanji', '漢字', 'Kanji')
t('EXAM_WRITE_HIRAGANA', 'WRITE IN HIRAGANA', 'ESCRIBE EN HIRAGANA', 'ESCREVA EM HIRAGANA', 'ひらがなで書く', 'ÉCRIVEZ EN HIRAGANA')
t('EXAM_WRITE_KATAKANA', 'WRITE IN KATAKANA', 'ESCRIBE EN KATAKANA', 'ESCREVA EM KATAKANA', 'カタカナで書く', 'ÉCRIVEZ EN KATAKANA')
t('EXAM_WRITE_KANJI', 'WRITE THE KANJI FOR', 'ESCRIBE EL KANJI DE', 'ESCREVA O KANJI DE', 'この意味の漢字を書く', 'ÉCRIVEZ LE KANJI DE')
t('SORT_ON', 'On', 'On', 'On', '音読み', 'On')
t('SORT_KUN', 'Kun', 'Kun', 'Kun', '訓読み', 'Kun')

# The kana chart (chart.c).
t('CHART_VOICED', 'Voiced (dakuten, handakuten) and small kana', 'Sonoras (dakuten, handakuten) y kana pequeños', 'Sonoras (dakuten, handakuten) e kana pequenos', '濁音・半濁音と小書き文字', 'Sonores (dakuten, handakuten) et petits kana')

# Text from a photo (scan.h).
t('SCAN_HINT', 'Point the camera at Japanese text, or choose a photo: what is read is boxed, and the lines you keep are written on the page.', 'Apunta la cámara a un texto en japonés o elige una foto: lo que se lee aparece enmarcado y las líneas que elijas se escriben en la página.', 'Aponte a câmera para um texto em japonês ou escolha uma foto: o que é lido fica destacado, e as linhas que você escolher são escritas na página.', 'カメラを日本語に向けるか、写真を選んでください。読み取った行に枠が付き、選んだ行をページに書きます。', 'Pointez l’appareil photo vers un texte japonais ou choisissez une photo : ce qui est lu est encadré, et les lignes gardées sont écrites sur la page.')
t('SCAN_LIVE_NONE', 'Point the camera at Japanese text', 'Apunta la cámara a un texto en japonés', 'Aponte a câmera para um texto em japonês', 'カメラを日本語に向けてください', 'Pointez l’appareil photo vers un texte japonais')
t('SCAN_NONE', 'No Japanese text found in this photo.', 'No se encontró texto japonés en esta foto.', 'Nenhum texto em japonês encontrado nesta foto.', 'この写真に日本語は見つかりませんでした。', 'Aucun texte japonais trouvé sur cette photo.')

# Into Japanese (translator.h): the Vocabulary's Translate with Google, from the reader's language.
t('TRANSLATOR_TITLE', 'Into Japanese', 'Al japonés', 'Para o japonês', '日本語に翻訳', 'Vers le japonais')
t('TRANSLATOR_EMPTY', 'Type a word or a sentence in your language in the field at the top right, then Return: Google translates it into Japanese, with the words in it — tap one to save it. Then save the whole of it, write it on the page or practise its characters.',
  'Escribe una palabra o una frase en tu idioma en el campo de arriba a la derecha y pulsa Intro: Google la traduce al japonés, con las palabras que contiene; toca una para guardarla. Después guárdala entera, escríbela en la página o practica sus caracteres.',
  'Digite uma palavra ou uma frase no seu idioma no campo no canto superior direito e toque em Retorno: o Google a traduz para o japonês, com as palavras que ela contém — toque em uma para salvá-la. Depois salve-a inteira, escreva-a na página ou pratique seus caracteres.',
  '右上の欄に単語や文を入力して改行すると、Googleが日本語に翻訳し、含まれる単語も表示します（タップで保存）。そのまま保存したり、ページに書いたり、字を練習したりできます。',
  'Écrivez un mot ou une phrase dans votre langue dans le champ en haut à droite, puis Entrée : Google le traduit en japonais, avec les mots qu’il contient — touchez-en un pour l’enregistrer. Ensuite, enregistrez-le en entier, écrivez-le sur la page ou entraînez-vous à ses caractères.')

# The page: Paste text, and Check's field.
t('NOTICE_NOTHING_TO_WRITE', 'No Japanese in the clipboard to write', 'No hay japonés en el portapapeles para escribir', 'Não há japonês na área de transferência para escrever', '書ける日本語がクリップボードにありません', 'Aucun japonais à écrire dans le presse-papiers')
t('CHECK_FIELD', 'I meant… (kyou wa, or Japanese)', 'Quise decir… (kyou wa, o japonés)', 'Eu quis dizer… (kyou wa, ou japonês)', '書きたかった言葉…（kyou wa、または日本語）', 'Je voulais écrire… (kyou wa, ou en japonais)')

# The word card (wordcard.h) and the Vocabulary's search.
t('WORD_FIELD_READING', 'Reading: kana, or romaji (megusuri)', 'Lectura: kana o rōmaji (megusuri)', 'Leitura: kana ou romaji (megusuri)', '読み：かな、またはローマ字（megusuri）', 'Lecture : kana ou rōmaji (megusuri)')
t('WORD_ERR_READING', 'Its reading, in kana or romaji', 'Falta su lectura, en kana o rōmaji', 'Falta a leitura, em kana ou romaji', '読みを、かなかローマ字で', 'Sa lecture, en kana ou en rōmaji')
t('WORD_ERR_ROMAJI', 'That reading is not romaji {APP} knows', '{APP} no reconoce esa lectura en rōmaji', 'O {APP} não reconhece essa leitura em romaji', 'そのローマ字は読み取れません', '{APP} ne reconnaît pas cette lecture en rōmaji')
t('WORD_TRANSLATE_HINT', 'Your meaning, into Japanese', 'Tu significado, al japonés', 'O seu significado, para o japonês', '意味（英語）を日本語に', 'Votre sens, en japonais')
t('VOCAB_SEARCH_HINT', 'Find: kanji, kana, romaji or meaning', 'Buscar: kanji, kana, romaji o significado', 'Buscar: kanji, kana, romaji ou significado', '検索：漢字、かな、ローマ字、意味', 'Chercher : kanji, kana, romaji ou sens')

# Google ML Kit's handwriting model (mlkit.h).
t('MLKIT_READY', 'Ready: the Japanese model is on this device.', 'Listo: el modelo japonés está en este dispositivo.', 'Pronto: o modelo japonês está neste dispositivo.', '準備完了：日本語モデルはこの端末にあります。', 'Prêt : le modèle japonais est sur cet appareil.')
t('MLKIT_DOWNLOADING', 'Downloading the Japanese model (about 20 MB)...', 'Descargando el modelo japonés (unos 20 MB)...', 'Baixando o modelo japonês (cerca de 20 MB)...', '日本語モデルをダウンロード中（約20 MB）…', 'Téléchargement du modèle japonais (environ 20 Mo)...')
t('MLKIT_FAILED', 'The Japanese model could not be downloaded. Is the device online?', 'No se pudo descargar el modelo japonés. ¿Hay conexión?', 'Não foi possível baixar o modelo japonês. O dispositivo está conectado?', '日本語モデルをダウンロードできませんでした。インターネットに接続されていますか？', 'Impossible de télécharger le modèle japonais. L’appareil est-il connecté ?')
t('MLKIT_MISSING', 'The Japanese model (about 20 MB) is not downloaded yet.', 'El modelo japonés (unos 20 MB) aún no está descargado.', 'O modelo japonês (cerca de 20 MB) ainda não foi baixado.', '日本語モデル（約20 MB）はまだダウンロードされていません。', 'Le modèle japonais (environ 20 Mo) n’est pas encore téléchargé.')

# Read aloud (speech.h): the device's basic voice spoke.
t('SPEECH_BETTER_VOICE', 'For a clearer voice, download a Japanese one in Settings › Accessibility › Spoken Content › Voices.', 'Para una voz más clara, descarga una japonesa en Ajustes › Accesibilidad › Contenido leído › Voces.', 'Para uma voz mais clara, baixe uma japonesa em Ajustes › Acessibilidade › Conteúdo Falado › Vozes.', 'よりきれいな音声は、設定 › アクセシビリティ › 読み上げコンテンツ › 声 から日本語の声をダウンロードできます。', 'Pour une voix plus claire, téléchargez-en une japonaise dans Réglages › Accessibilité › Contenu énoncé › Voix.')

# Exams, the album, practice sheets.
t('EXAM_INTRO', 'Each character once, from memory: a kanji from its meaning and readings, a kana from its romaji.', 'Cada carácter una vez, de memoria: un kanji a partir de su significado y lecturas; un kana, de su rōmaji.', 'Cada caractere uma vez, de memória: um kanji a partir do significado e das leituras; um kana, do romaji.', '各字を1回ずつ、何も見ずに：漢字は意味と読みから、かなはローマ字から書きます。', 'Chaque caractère une fois, de mémoire : un kanji d’après son sens et ses lectures, un kana d’après son rōmaji.')
t('ALBUM_EMPTY', 'Nothing practised yet: open a character (Kanji or Kana), then Practice, then Score.', 'Aún no hay nada practicado: abre un carácter (Kanji o Kana), luego Practicar y luego Puntuar.', 'Ainda não há nada praticado: abra um caractere (Kanji ou Kana), depois Praticar e depois Avaliar.', 'まだ練習がありません。字を開いて（漢字かかな）、「練習」、「採点」の順に。', 'Rien de pratiqué pour l’instant : ouvrez un caractère (Kanji ou Kana), puis S’entraîner, puis Noter.')
t('SHEET_CREDIT', 'Stroke order from KanjiVG (Ulrich Apel), CC BY-SA 3.0. Made with {APP}.', 'Orden de trazos de KanjiVG (Ulrich Apel), CC BY-SA 3.0. Hecho con {APP}.', 'Ordem dos traços do KanjiVG (Ulrich Apel), CC BY-SA 3.0. Feito com o {APP}.', '書き順: KanjiVG (Ulrich Apel), CC BY-SA 3.0。{APP}で作成。', 'Ordre des traits : KanjiVG (Ulrich Apel), CC BY-SA 3.0. Fait avec {APP}.')
