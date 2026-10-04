# Thai's UI strings (lang.h): the study's ids that name the language, in
# its words, and the ids fude/lang/th/lang.c and chart.c use. A study app's
# strings tool reads this after fude/study/strings.py (as fude/lang/ja/strings.py,
# whose study ids these are).
#
#   t('ID', English, Spanish, Portuguese (Brazil), Japanese, French)

# The kinds of character (lang.c's groups) and their exam prompts.
t('TH_CONSONANTS', 'Consonants', 'Consonantes', 'Consoantes', '子音', 'Consonnes')
t('TH_VOWELS', 'Vowels', 'Vocales', 'Vogais', '母音', 'Voyelles')
t('TH_MARKS', 'Tone marks and signs', 'Tonos y signos', 'Tons e sinais', '声調記号・記号', 'Tons et signes')
t('TH_DIGITS', 'Digits', 'Cifras', 'Algarismos', '数字', 'Chiffres')
t('EXAM_WRITE_TH_CONSONANT', 'WRITE THE CONSONANT', 'ESCRIBE LA CONSONANTE', 'ESCREVA A CONSOANTE', 'この子音を書く', 'ÉCRIVEZ LA CONSONNE')
t('EXAM_WRITE_TH_VOWEL', 'WRITE THE VOWEL', 'ESCRIBE LA VOCAL', 'ESCREVA A VOGAL', 'この母音を書く', 'ÉCRIVEZ LA VOYELLE')
t('EXAM_WRITE_TH_MARK', 'WRITE THE SIGN', 'ESCRIBE EL SIGNO', 'ESCREVA O SINAL', 'この記号を書く', 'ÉCRIVEZ LE SIGNE')
t('EXAM_WRITE_TH_DIGIT', 'WRITE THE DIGIT', 'ESCRIBE LA CIFRA', 'ESCREVA O ALGARISMO', 'この数字を書く', 'ÉCRIVEZ LE CHIFFRE')

# The levels (lang.c): the letters' own — the ones learnt first, the rest, the rare.
t('TH_LEVEL_1', 'Core letters', 'Letras básicas', 'Letras básicas', '基本の文字', 'Lettres de base')
t('TH_LEVEL_1_SHORT', 'Core', 'Básicas', 'Básicas', '基本', 'Base')
t('TH_LEVEL_2', 'More letters', 'Más letras', 'Mais letras', 'その他の文字', 'Autres lettres')
t('TH_LEVEL_2_SHORT', 'More', 'Más', 'Mais', 'その他', 'Autres')
t('TH_LEVEL_3', 'Rare letters', 'Letras raras', 'Letras raras', 'まれな文字', 'Lettres rares')
t('TH_LEVEL_3_SHORT', 'Rare', 'Raras', 'Raras', 'まれ', 'Rares')

# Text from a photo (scan.h).
t('SCAN_HINT', 'Point the camera at Thai text, or choose a photo: what is read is boxed, and the lines you keep are written on the page.', 'Apunta la cámara a un texto en tailandés o elige una foto: lo que se lee aparece enmarcado y las líneas que elijas se escriben en la página.', 'Aponte a câmera para um texto em tailandês ou escolha uma foto: o que é lido fica destacado, e as linhas que você escolher são escritas na página.', 'カメラをタイ語の文字に向けるか、写真を選んでください。読み取った行は枠で囲まれ、選んだ行がページに書かれます。', 'Pointez l’appareil photo vers un texte thaï, ou choisissez une photo : ce qui est lu est encadré, et les lignes que vous gardez sont écrites sur la page.')
t('SCAN_LIVE_NONE', 'Point the camera at Thai text', 'Apunta la cámara a un texto en tailandés', 'Aponte a câmera para um texto em tailandês', 'カメラをタイ語に向けてください', 'Pointez l’appareil photo vers un texte thaï')
t('SCAN_NONE', 'No Thai text found in this photo.', 'No se encontró texto en tailandés en esta foto.', 'Nenhum texto em tailandês encontrado nesta foto.', 'この写真にタイ語は見つかりませんでした。', 'Aucun texte thaï trouvé sur cette photo.')

# Into Thai (translator.h): the Vocabulary's Translate with Google, from the reader's language.
t('TRANSLATOR_TITLE', 'Into Thai', 'Al tailandés', 'Para o tailandês', 'タイ語に翻訳', 'Vers le thaï')
t('TRANSLATOR_EMPTY', 'Type a word or a sentence in your language in the field at the top right, then Return: Google translates it into Thai, with the words in it — tap one to save it. Then save the whole of it, write it on the page or practise its letters.', 'Escribe una palabra o una frase en tu idioma en el campo de arriba a la derecha y pulsa Intro: Google la traduce al tailandés, con las palabras que contiene; toca una para guardarla. Después guárdala entera, escríbela en la página o practica sus letras.', 'Digite uma palavra ou uma frase no seu idioma no campo no canto superior direito e toque em Retorno: o Google a traduz para o tailandês, com as palavras que ela contém — toque em uma para salvá-la. Depois salve-a inteira, escreva-a na página ou pratique suas letras.', '右上の欄に単語や文を入力して改行すると、Googleがタイ語に翻訳し、含まれる単語も表示します（タップで保存）。そのまま保存したり、ページに書いたり、文字を練習したりできます。', 'Écrivez un mot ou une phrase dans votre langue dans le champ en haut à droite, puis Entrée : Google le traduit vers le thaï, avec les mots qu’il contient — touchez-en un pour l’enregistrer. Ensuite, enregistrez-le en entier, écrivez-le sur la page ou entraînez-vous à ses lettres.')

# The page: Paste text, and Check's field.
t('NOTICE_NOTHING_TO_WRITE', 'No Thai in the clipboard to write', 'No hay tailandés en el portapapeles para escribir', 'Não há tailandês na área de transferência para escrever', '書けるタイ語がクリップボードにありません', 'Aucun thaï à écrire dans le presse-papiers')
t('CHECK_FIELD', 'I meant… (sawatdi, or Thai)', 'Quise decir… (sawatdi, o tailandés)', 'Eu quis dizer… (sawatdi, ou tailandês)', '書きたかった言葉…（sawatdi、またはタイ語）', 'Je voulais écrire… (sawatdi, ou en thaï)')

# The word card (wordcard.h), the characters' search and the Vocabulary's.
t('WORD_FIELD_READING', 'Romanization (sawatdi): blank, from the letters', 'Romanización (sawatdi): vacía, la de las letras', 'Romanização (sawatdi): vazia, a das letras', 'ローマ字表記（sawatdi）：空欄なら文字から', 'Romanisation (sawatdi) : vide, celle des lettres')
t('WORD_ERR_READING', 'Its romanization', 'Falta su romanización', 'Falta a romanização', 'ローマ字表記を', 'Sa romanisation')
t('WORD_ERR_ROMAJI', 'That romanization is not one {APP} knows', '{APP} no reconoce esa romanización', 'O {APP} não reconhece essa romanização', 'そのローマ字表記は読み取れません', '{APP} ne reconnaît pas cette romanisation')
t('WORD_TRANSLATE_HINT', 'Your meaning, into Thai', 'Tu significado, al tailandés', 'O seu significado, para o tailandês', '意味（英語）をタイ語に', 'Votre sens, en thaï')
o('SEARCH_HINT', 'A letter’s name: ko kai', 'El nombre de una letra: ko kai', 'O nome de uma letra: ko kai', '文字の名前：ko kai', 'Le nom d’une lettre : ko kai')
t('VOCAB_SEARCH_HINT', 'Find: Thai, romanization or meaning', 'Buscar: tailandés, romanización o significado', 'Buscar: tailandês, romanização ou significado', '検索：タイ語、ローマ字、意味', 'Chercher : thaï, romanisation ou sens')

# Google ML Kit's handwriting model (mlkit.h).
t('MLKIT_READY', 'Ready: the Thai model is on this device.', 'Listo: el modelo tailandés está en este dispositivo.', 'Pronto: o modelo tailandês está neste dispositivo.', '準備完了：タイ語モデルはこの端末にあります。', 'Prêt : le modèle thaï est sur cet appareil.')
t('MLKIT_DOWNLOADING', 'Downloading the Thai model (about 20 MB)...', 'Descargando el modelo tailandés (unos 20 MB)...', 'Baixando o modelo tailandês (cerca de 20 MB)...', 'タイ語モデルをダウンロード中（約20 MB）…', 'Téléchargement du modèle thaï (environ 20 Mo)...')
t('MLKIT_FAILED', 'The Thai model could not be downloaded. Is the device online?', 'No se pudo descargar el modelo tailandés. ¿Hay conexión?', 'Não foi possível baixar o modelo tailandês. O dispositivo está conectado?', 'タイ語モデルをダウンロードできませんでした。インターネットに接続されていますか？', 'Impossible de télécharger le modèle thaï. L’appareil est-il connecté ?')
t('MLKIT_MISSING', 'The Thai model (about 20 MB) is not downloaded yet.', 'El modelo tailandés (unos 20 MB) aún no está descargado.', 'O modelo tailandês (cerca de 20 MB) ainda não foi baixado.', 'タイ語モデル（約20 MB）はまだダウンロードされていません。', 'Le modèle thaï (environ 20 Mo) n’est pas encore téléchargé.')

# No voice for the language yet (speech.h): how to add one, Android's and iOS's (study's VOICE_TITLE over it).
t('VOICE_STEPS_ANDROID', '{APP} reads words and readings aloud with this device’s text-to-speech, which has no Thai voice yet. To add one:\\n\\n1. Tap Open settings below: it opens Text-to-speech output.\\n2. As the preferred engine, choose Speech Services by Google.\\n3. Tap the gear next to it, then Install voice data › Thai, and download it.\\n4. Come back to {APP}: the speaker buttons appear by themselves.', '{APP} lee en voz alta las palabras y sus lecturas con la síntesis de voz del dispositivo, que aún no tiene voz en tailandés. Para añadirla:\\n\\n1. Toca Abrir ajustes, abajo: se abre la salida de texto a voz.\\n2. Como motor preferido, elige Servicios de voz de Google.\\n3. Toca el engranaje de al lado y luego Instalar datos de voz › Tailandés, y descárgala.\\n4. Vuelve a {APP}: los botones de altavoz aparecen solos.', 'O {APP} lê em voz alta as palavras e as leituras com a conversão de texto em voz do aparelho, que ainda não tem voz em tailandês. Para adicionar uma:\\n\\n1. Toque em Abrir configurações, abaixo: abre a saída de texto em voz.\\n2. Como mecanismo preferido, escolha Serviços de fala do Google.\\n3. Toque na engrenagem ao lado e depois em Instalar dados de voz › Tailandês, e baixe.\\n4. Volte ao {APP}: os botões de alto-falante aparecem sozinhos.', '{APP}は端末のテキスト読み上げで単語や読みを声に出しますが、まだタイ語の音声がありません。追加するには：\\n\\n1. 下の「設定を開く」をタップすると、テキスト読み上げの設定が開きます。\\n2. 優先するエンジンに「Google 音声サービス」を選びます。\\n3. 横の歯車をタップし、「音声データをインストール」›「タイ語」を選んでダウンロードします。\\n4. {APP}に戻ると、スピーカーのボタンが自動で表示されます。', '{APP} lit à voix haute les mots et leurs lectures avec la synthèse vocale de l’appareil, qui n’a pas encore de voix en thaï. Pour en ajouter une :\\n\\n1. Touchez Ouvrir les réglages, ci-dessous : la synthèse vocale s’ouvre.\\n2. Comme moteur préféré, choisissez Services vocaux de Google.\\n3. Touchez la roue dentée à côté, puis Installer les données vocales › Thaï, et téléchargez-la.\\n4. Revenez dans {APP} : les boutons de haut-parleur apparaissent d’eux-mêmes.')
t('VOICE_STEPS_IOS', '{APP} reads words and readings aloud with this device’s voices, and none of them speaks Thai yet. To add one, open Settings › Accessibility › Spoken Content › Voices › Thai and download a voice. Then come back to {APP}: the speaker buttons appear by themselves.', '{APP} lee en voz alta las palabras y sus lecturas con las voces del dispositivo, y ninguna habla tailandés todavía. Para añadir una, abre Ajustes › Accesibilidad › Contenido leído › Voces › Tailandés y descarga una voz. Después vuelve a {APP}: los botones de altavoz aparecen solos.', 'O {APP} lê em voz alta as palavras e as leituras com as vozes do aparelho, e nenhuma fala tailandês ainda. Para adicionar uma, abra Ajustes › Acessibilidade › Conteúdo Falado › Vozes › Tailandês e baixe uma voz. Depois volte ao {APP}: os botões de alto-falante aparecem sozinhos.', '{APP}は端末の音声で単語や読みを声に出しますが、まだタイ語を話す音声がありません。設定 › アクセシビリティ › 読み上げコンテンツ › 声 › タイ語 から音声をダウンロードしてください。{APP}に戻ると、スピーカーのボタンが自動で表示されます。', '{APP} lit à voix haute les mots et leurs lectures avec les voix de l’appareil, et aucune ne parle encore thaï. Pour en ajouter une, ouvrez Réglages › Accessibilité › Contenu énoncé › Voix › Thaï et téléchargez une voix. Revenez ensuite dans {APP} : les boutons de haut-parleur apparaissent d’eux-mêmes.')
t('SPEECH_BETTER_VOICE', 'For a clearer voice, download a Thai one in Settings › Accessibility › Spoken Content › Voices.', 'Para una voz más clara, descarga una en tailandés en Ajustes › Accesibilidad › Contenido leído › Voces.', 'Para uma voz mais clara, baixe uma em tailandês em Ajustes › Acessibilidade › Conteúdo Falado › Vozes.', 'よりきれいな音声は、設定 › アクセシビリティ › 読み上げコンテンツ › 声 からタイ語の声をダウンロードできます。', 'Pour une voix plus claire, téléchargez-en une en thaï dans Réglages › Accessibilité › Contenu énoncé › Voix.')

# Exams, the album, practice sheets.
t('EXAM_INTRO', 'Each letter once, from memory, from its name.', 'Cada letra una vez, de memoria, a partir de su nombre.', 'Cada letra uma vez, de memória, a partir do nome.', '各文字を1回ずつ、名前から何も見ずに書きます。', 'Chaque lettre une fois, de mémoire, à partir de son nom.')
t('ALBUM_EMPTY', 'Nothing practised yet: open a letter in Alphabet, then Practice, then Score.', 'Aún no hay nada practicado: abre una letra en Alfabeto, luego Practicar y luego Puntuar.', 'Ainda não há nada praticado: abra uma letra em Alfabeto, depois Praticar e depois Avaliar.', 'まだ練習がありません。文字表で文字を開いて、練習、採点の順に進んでください。', 'Rien d’entraîné pour l’instant : ouvrez une lettre dans Alphabet, puis S’entraîner, puis Noter.')
t('SHEET_CREDIT', 'Thai stroke order by {APP}. Made with {APP}.', 'Orden de trazos del tailandés de {APP}. Hecho con {APP}.', 'Ordem dos traços do tailandês do {APP}. Feito com o {APP}.', 'タイ文字の書き順：{APP}。{APP}で作成。', 'Ordre des traits du thaï par {APP}. Fait avec {APP}.')
