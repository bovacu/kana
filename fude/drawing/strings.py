# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# The drawing core's UI strings: a layer of every app's (tools/strings/build.py
# reads it, first). {APP} is the app's name (its src/version.h), filled in as the
# app's strings are written; a layer over this one, or the app, may give a
# string other words (o: the study's Your data says what a study app keeps).
#
#   t('ID', English, Spanish, Portuguese (Brazil), Japanese, French)

# --- common words ---------------------------------------------------------------
t('FINISH', 'Finish', 'Terminar', 'Concluir', '終了', 'Terminer')
t('CLOSE', 'Close', 'Cerrar', 'Fechar', '閉じる', 'Fermer')
t('MORE_MENU', 'More', 'Más', 'Mais', 'その他', 'Plus')   # a row's buttons that do not fit a phone (row.h)
t('READ_CLOSE_IN', 'Close ({0})', 'Cerrar ({0})', 'Fechar ({0})', '閉じる（{0}）', 'Fermer ({0})')   # a must-read card's Close, counting down (readcard.h)
t('CANCEL', 'Cancel', 'Cancelar', 'Cancelar', 'キャンセル', 'Annuler')
t('DELETE', 'Delete', 'Eliminar', 'Excluir', '削除', 'Supprimer')
t('SAVE', 'Save', 'Guardar', 'Salvar', '保存', 'Enregistrer')
t('RENAME', 'Rename', 'Renombrar', 'Renomear', '名前を変更', 'Renommer')
t('UNDO', 'Undo', 'Deshacer', 'Desfazer', '元に戻す', 'Défaire')
t('REDO', 'Redo', 'Rehacer', 'Refazer', 'やり直す', 'Refaire')
t('OF_N', '{0} of {1}', '{0} de {1}', '{0} de {1}', '{0} / {1}', '{0} sur {1}')

# --- the canvas's bar and panels (the tools' names are hidden behind icons) ---------------
t('TOOL_FINGER', 'Finger', 'Dedo', 'Dedo', '指', 'Doigt')
t('FINGER_ON', 'Your finger writes: two fingers move the page', 'Tu dedo escribe: dos dedos mueven la página', 'Seu dedo escreve: dois dedos movem a página', '指で書けます。ページは2本指で動かします', 'Votre doigt écrit : deux doigts déplacent la page')
t('FINGER_OFF', 'The Apple Pencil writes: your fingers move the page', 'Escribe el Apple Pencil: tus dedos mueven la página', 'O Apple Pencil escreve: seus dedos movem a página', 'Apple Pencilで書きます。ページは指で動かします', 'L’Apple Pencil écrit : vos doigts déplacent la page')
t('FINGER_OFF_ANDROID', 'The stylus writes: your fingers move the page', 'Escribe el lápiz: tus dedos mueven la página', 'A caneta escreve: seus dedos movem a página', 'ペンで書きます。ページは指で動かします', 'Le stylet écrit : vos doigts déplacent la page')
t('TOOL_DRAW', 'Draw', 'Dibujar', 'Desenhar', '書く', 'Dessiner')
t('TOOL_MARK', 'Marker', 'Marcador', 'Marca-texto', 'マーカー', 'Surligneur')
t('TOOL_ERASE', 'Erase', 'Borrar', 'Apagar', '消しゴム', 'Gomme')
t('TOOL_LASSO', 'Lasso', 'Lazo', 'Laço', '投げ縄', 'Lasso')
t('TOOL_CLEAR', 'Clear the page', 'Borrar la página', 'Limpar a página', 'ページを消去', 'Effacer la page')
t('TOOL_COLOR', 'Colour', 'Color', 'Cor', '色', 'Couleur')
t('TOOL_PAGE', 'Page', 'Página', 'Página', 'ページ', 'Page')
t('TOOL_SCREEN', 'Screen', 'Pantalla', 'Tela', '画面', 'Écran')
t('TOOL_PAPER', 'Paper', 'Papel', 'Papel', '用紙', 'Papier')
t('TOOL_ROTATE', 'Rotate', 'Girar', 'Girar', '回転', 'Pivoter')
t('TOOL_RESET', 'Reset the view', 'Restablecer la vista', 'Redefinir a visão', '表示をリセット', 'Réinitialiser la vue')
t('MENU', 'Menu', 'Menú', 'Menu', 'メニュー', 'Menu')
t('PAPER_DOTS', 'Dots', 'Puntos', 'Pontos', 'ドット', 'Points')
t('PAPER_SQUARES', 'Squares', 'Cuadros', 'Quadrados', 'マス目', 'Carreaux')
t('PAPER_LINES', 'Lines', 'Líneas', 'Linhas', '罫線', 'Lignes')
t('PAPER_NONE', 'Blank', 'En blanco', 'Em branco', '無地', 'Vierge')
t('SEL_CUT', 'Cut', 'Cortar', 'Recortar', '切り取り', 'Couper')
t('SEL_COPY', 'Copy', 'Copiar', 'Copiar', 'コピー', 'Copier')
t('SEL_COPIED', 'Copied', 'Copiado', 'Copiado', 'コピー済み', 'Copié')
t('SEL_DUPLICATE', 'Duplicate', 'Duplicar', 'Duplicar', '複製', 'Dupliquer')
t('CTX_PASTE', 'Paste', 'Pegar', 'Colar', '貼り付け', 'Coller')
t('CTX_SELECT_ALL', 'Select all', 'Seleccionar todo', 'Selecionar tudo', 'すべて選択', 'Tout sélectionner')

# --- the side panel and Settings ---------------------------------------------------------------
t('SIDE_NOTES', 'NOTES', 'NOTAS', 'NOTAS', 'ノート', 'NOTES')
t('FOLDER', 'Folder', 'Carpeta', 'Pasta', 'フォルダ', 'Dossier')
t('CANVAS', 'Canvas', 'Lienzo', 'Tela', 'キャンバス', 'Toile')
t('SETTINGS', 'Settings', 'Ajustes', 'Configurações', '設定', 'Réglages')
t('SETTINGS_THEME', 'THEME', 'TEMA', 'TEMA', 'テーマ', 'THÈME')
t('SETTINGS_LANGUAGE', 'LANGUAGE', 'IDIOMA', 'IDIOMA', '言語', 'LANGUE')
t('SETTINGS_PEN_WIDTH', 'Pen width', 'Grosor del trazo', 'Espessura do traço', '線の太さ', 'Épaisseur du trait')
t('SETTINGS_EVEN', 'Even', 'Uniforme', 'Uniforme', '均一', 'Uniforme')
t('SETTINGS_PRESSURE', 'Pressure', 'Presión', 'Pressão', '筆圧', 'Pression')
t('SETTINGS_PAPER', 'Lines & squares', 'Líneas y cuadros', 'Linhas e quadrados', '罫線とマス目', 'Lignes et carreaux')
t('SETTINGS_UI_SIZE', 'Interface size', 'Tamaño de la interfaz', 'Tamanho da interface', '表示サイズ', 'Taille de l’interface')   # everything on screen bigger or smaller (Small on a phone at first)
t('SIZE_SMALL', 'Small', 'Pequeño', 'Pequeno', '小', 'Petit')
t('SIZE_MEDIUM', 'Medium', 'Mediano', 'Médio', '中', 'Moyen')
t('SIZE_LARGE', 'Large', 'Grande', 'Grande', '大', 'Grand')
t('SETTINGS_ABOUT', 'ABOUT', 'ACERCA DE', 'SOBRE', '情報', 'À PROPOS')
t('ABOUT_BUILT', '{APP} {0}, built {1}.', '{APP} {0}, compilado el {1}.', '{APP} {0}, compilado em {1}.', '{APP} {0}（{1} ビルド）', '{APP} {0}, compilé le {1}.')
t('RATE', 'Rate {APP}', 'Valorar {APP}', 'Avaliar o {APP}', '{APP}を評価', 'Noter {APP}')
t('TUTORIAL', 'Tutorial', 'Tutorial', 'Tutorial', 'チュートリアル', 'Tutoriel')
# FAQ & Contact (the side panel's lowest row, and its card): the FAQ's page, and where to write.
t('FAQ_CONTACT', 'FAQ & Contact', 'Preguntas y contacto', 'Perguntas e contato', 'よくある質問・お問い合わせ', 'FAQ et contact')
t('FAQ_OPEN', 'Frequently asked questions', 'Preguntas frecuentes', 'Perguntas frequentes', 'よくある質問', 'Questions fréquentes')
t('CONTACT_WRITE', 'Questions, ideas or a problem? Write to us:', '¿Dudas, ideas o algún problema? Escríbenos:', 'Dúvidas, ideias ou algum problema? Escreva para nós:', 'ご質問・ご意見・不具合のご報告はこちらへ：', 'Une question, une idée ou un problème ? Écrivez-nous :')

# Your data (backup.h): offline; the platform's own backup; Export / Import.
t('DATA', 'Your data', 'Tus datos', 'Seus dados', 'データ', 'Vos données')
t('DATA_OFFLINE', '{APP} works offline. Your pages stay on this device: {APP} sends them nowhere.', '{APP} funciona sin conexión. Tus páginas se quedan en este dispositivo: {APP} no las envía a ninguna parte.', 'O {APP} funciona offline. Suas páginas ficam neste dispositivo: o {APP} não as envia para lugar nenhum.', '{APP}はオフラインで動作します。ページはこの端末に保存され、{APP}がどこかへ送ることはありません。', '{APP} fonctionne hors ligne. Vos pages restent sur cet appareil : {APP} ne les envoie nulle part.')
t('DATA_BACKUP_IOS', 'They are also part of this device’s iCloud Backup when it is on (Settings › your name › iCloud › iCloud Backup): restoring the device, or setting up a new one, from that backup brings them back.', 'Tus datos también forman parte de la copia de seguridad en iCloud de este dispositivo si está activada (Ajustes › tu nombre › iCloud › Copia en iCloud): al restaurar el dispositivo, o al configurar uno nuevo, desde esa copia, vuelven.', 'Eles também fazem parte do Backup do iCloud deste dispositivo quando ativado (Ajustes › seu nome › iCloud › Backup do iCloud): restaurar o dispositivo, ou configurar um novo, a partir desse backup os traz de volta.', 'iCloudバックアップがオンなら（設定 › ユーザ名 › iCloud › iCloudバックアップ）、データはこの端末のバックアップにも含まれます。そのバックアップから端末を復元したり、新しい端末を設定したりすると元に戻ります。', 'Elles font aussi partie de la sauvegarde iCloud de cet appareil si elle est activée (Réglages › votre nom › iCloud › Sauvegarde iCloud) : restaurer l’appareil, ou en configurer un nouveau, à partir de cette sauvegarde les ramène.')
t('DATA_BACKUP_ANDROID', 'They are also part of this device’s Google backup when it is on (Settings › Google › Backup): a new or reset device that restores from it gets them back.', 'Tus datos también forman parte de la copia de seguridad de Google de este dispositivo si está activada (Ajustes › Google › Copia de seguridad): un dispositivo nuevo o restablecido que se restaure desde ella los recupera.', 'Eles também fazem parte do backup do Google deste dispositivo quando ativado (Configurações › Google › Backup): um dispositivo novo ou redefinido que restaure a partir dele os recebe de volta.', 'Googleバックアップがオンなら（設定 › Google › バックアップ）、データはこの端末のバックアップにも含まれ、そこから復元した新しい端末や初期化した端末に戻ります。', 'Elles font aussi partie de la sauvegarde Google de cet appareil si elle est activée (Paramètres › Google › Sauvegarde) : un appareil neuf ou réinitialisé qui la restaure les récupère.')
t('DATA_BACKUP_DESKTOP', 'They are kept in {APP}’s folder on this computer.', 'Se guardan en la carpeta de {APP} en este ordenador.', 'Ficam na pasta do {APP} neste computador.', 'このコンピュータの{APP}のフォルダに保存されています。', 'Elles sont gardées dans le dossier de {APP} sur cet ordinateur.')
t('DATA_HOW', 'To keep a copy of your own, or to move to another device: Export puts everything in one file, to keep where you like (a cloud drive, the device, a message); Import, on any device, puts it back — replacing what is there.', 'Para guardar tu propia copia o pasarte a otro dispositivo: Exportar pone todo en un solo archivo, para guardarlo donde quieras (una nube, el dispositivo, un mensaje); Importar, en cualquier dispositivo, lo devuelve, sustituyendo lo que haya.', 'Para ter sua própria cópia ou mudar de dispositivo: Exportar coloca tudo em um único arquivo, para guardar onde quiser (uma nuvem, o dispositivo, uma mensagem); Importar, em qualquer dispositivo, traz tudo de volta, substituindo o que houver.', '自分でコピーを残したり、別の端末に移したりするには：「書き出す」ですべてを1つのファイルにまとめ、好きな場所（クラウド、端末、メッセージなど）に保存できます。「読み込む」で、どの端末でも元に戻せます（今あるデータは置き換えられます）。', 'Pour garder votre propre copie ou changer d’appareil : Exporter met tout dans un seul fichier, à garder où vous voulez (un cloud, l’appareil, un message) ; Importer, sur n’importe quel appareil, remet tout en place, en remplaçant ce qui s’y trouve.')
t('DATA_EXPORT', 'Export', 'Exportar', 'Exportar', '書き出す', 'Exporter')
t('DATA_IMPORT', 'Import', 'Importar', 'Importar', '読み込む', 'Importer')
t('DATA_EXPORT_READY', 'Your backup is ready ({0} files): choose where to keep it.', 'Tu copia está lista ({0} archivos): elige dónde guardarla.', 'Seu backup está pronto ({0} arquivos): escolha onde guardá-lo.', 'バックアップの準備ができました（{0}ファイル）。保存先を選んでください。', 'Votre sauvegarde est prête ({0} fichiers) : choisissez où la garder.')
t('DATA_EXPORT_SAVED', 'Saved ({0} files): {1}', 'Guardada ({0} archivos): {1}', 'Salvo ({0} arquivos): {1}', '保存しました（{0}ファイル）：{1}', 'Enregistrée ({0} fichiers) : {1}')
t('DATA_EXPORT_FAILED', 'The backup could not be made.', 'No se pudo hacer la copia.', 'Não foi possível fazer o backup.', 'バックアップを作成できませんでした。', 'La sauvegarde n’a pas pu être faite.')
t('DATA_NOT_BACKUP', 'That file is not a {APP} backup, or it is damaged.', 'Ese archivo no es una copia de {APP}, o está dañado.', 'Esse arquivo não é um backup do {APP}, ou está danificado.', 'このファイルは{APP}のバックアップではないか、壊れています。', 'Ce fichier n’est pas une sauvegarde de {APP}, ou il est endommagé.')
t('DATA_CANT_OPEN', 'The file could not be opened.', 'No se pudo abrir el archivo.', 'Não foi possível abrir o arquivo.', 'ファイルを開けませんでした。', 'Le fichier n’a pas pu être ouvert.')
t('DATA_CONFIRM', 'Replace everything in {APP} on this device with this backup from {0} ({1,plural, one{# page} other{# pages}})? What is here now will be gone: export it first if you want to keep it.', '¿Sustituir todo lo de {APP} en este dispositivo por esta copia del {0} ({1,plural, one{# página} other{# páginas}})? Lo que hay ahora se perderá: expórtalo antes si quieres conservarlo.', 'Substituir tudo do {APP} neste dispositivo por este backup de {0} ({1,plural, one{# página} other{# páginas}})? O que há agora será perdido: exporte antes se quiser guardá-lo.', 'この端末の{APP}のデータを、{0}のバックアップ（{1}ページ）で置き換えますか？今あるデータは消えます。残したい場合は先に書き出してください。', 'Remplacer tout {APP} sur cet appareil par cette sauvegarde du {0} ({1,plural, one{# page} other{# pages}}) ? Ce qui s’y trouve maintenant disparaîtra : exportez-le d’abord si vous voulez le garder.')
t('DATA_REPLACE', 'Replace', 'Sustituir', 'Substituir', '置き換える', 'Remplacer')
t('DATA_IMPORTED', 'Imported: everything is as it was on {0}.', 'Importado: todo está como el {0}.', 'Importado: tudo está como em {0}.', '読み込みました。{0}の状態に戻りました。', 'Importé : tout est comme le {0}.')
t('DATA_IMPORT_FAILED', 'The backup could not be imported: nothing was changed.', 'No se pudo importar la copia: no se cambió nada.', 'Não foi possível importar o backup: nada foi alterado.', 'バックアップを読み込めませんでした。何も変更されていません。', 'La sauvegarde n’a pas pu être importée : rien n’a été modifié.')
t('LICENCES', 'Licences', 'Licencias', 'Licenças', 'ライセンス', 'Licences')
t('LICENCE_FONTS', 'Fonts', 'Fuentes', 'Fontes', 'フォント', 'Polices')
t('LICENCE_LIBRARIES', 'Libraries', 'Bibliotecas', 'Bibliotecas', 'ライブラリ', 'Bibliothèques')
t('LICENCE_MISSING', 'This licence is missing from the app.', 'Esta licencia no está en la aplicación.', 'Esta licença não está no aplicativo.', 'このライセンスはアプリに含まれていません。', 'Cette licence est absente de l’application.')
t('THEME_PAPER', 'Paper', 'Papel', 'Papel', '紙', 'Papier')
t('THEME_WASHI', 'Washi', 'Washi', 'Washi', '和紙', 'Washi')
t('THEME_NIGHT', 'Night', 'Noche', 'Noite', '夜', 'Nuit')
t('THEME_MATCHA', 'Matcha', 'Matcha', 'Matcha', '抹茶', 'Matcha')
t('THEME_SAKURA', 'Sakura', 'Sakura', 'Sakura', '桜', 'Sakura')
t('NOTE_FOLDER_BODY', 'Folder, ' + P('# canvas', '# canvases'), 'Carpeta, ' + P('# lienzo', '# lienzos'), 'Pasta, ' + P('# tela', '# telas'), 'フォルダ・キャンバス{0}枚', 'Dossier, ' + P('# toile', '# toiles'))
t('NOTE_RENAME_FOLDER', 'Rename folder', 'Renombrar carpeta', 'Renomear pasta', 'フォルダの名前を変更', 'Renommer le dossier')
t('NOTE_RENAME_CANVAS', 'Rename canvas', 'Renombrar lienzo', 'Renomear tela', 'キャンバスの名前を変更', 'Renommer la toile')
t('NOTE_DELETE_TITLE', 'Delete “{0}”?', '¿Eliminar «{0}»?', 'Excluir “{0}”?', '「{0}」を削除しますか？', 'Supprimer « {0} » ?')
t('NOTE_DELETE_FOLDER', 'The ' + P('# canvas', '# canvases') + ' in it (and any folders) go too. This cannot be undone.',
  P('También se eliminan el lienzo que contiene', 'También se eliminan los # lienzos que contiene') + ' (y sus carpetas). No se puede deshacer.',
  P('A tela que está nela também será excluída', 'As # telas que estão nela também serão excluídas') + ' (e as pastas). Não é possível desfazer.',
  '中のキャンバス{0}枚（とフォルダ）も削除されます。元に戻せません。',
  P('La toile qu’il contient', 'Les # toiles qu’il contient') + ' (et ses dossiers) seront aussi supprimées. Action irréversible.')
t('NOTE_UNDONE', 'This cannot be undone.', 'No se puede deshacer.', 'Não é possível desfazer.', '元に戻せません。', 'Action irréversible.')
t('NOTE_PAGE', 'Page', 'Página', 'Página', 'ページ', 'Page')

# --- documents: the Library, a canvas's PDF (doc/) ------------------------------------------------
t('BACK', 'Back', 'Atrás', 'Voltar', '戻る', 'Retour')
t('LIBRARY_TITLE', 'Lectures', 'Lecciones', 'Lições', '教材', 'Leçons')
t('LIBRARY_CAPTION', 'Read them, zoom in, write on them: your notes stay on the page.', 'Léelas, amplía y escribe encima: tus notas se quedan en la página.',
  'Leia, amplie e escreva por cima: suas anotações ficam na página.', '読んで、拡大して、書き込めます。メモはページに残ります。', 'Lisez-les, zoomez, écrivez dessus : vos notes restent sur la page.')
t('LIBRARY_BOOKS', 'Free lessons', 'Lecciones gratuitas', 'Lições gratuitas', '無料の教材', 'Leçons gratuites')
t('LIBRARY_YOURS', 'Your documents', 'Tus documentos', 'Seus documentos', 'あなたの資料', 'Vos documents')
t('LIBRARY_YOURS_EMPTY', 'PDFs and pictures you bring in, and pages you scan, show here and in your notes, to read and write on.',
  'Los PDF e imágenes que traigas y las páginas que escanees aparecen aquí y en tus notas, para leer y escribir encima.',
  'Os PDFs e imagens que você trouxer e as páginas que escanear aparecem aqui e nas suas notas, para ler e escrever por cima.',
  '取り込んだPDFや画像、スキャンしたページは、こことノートに表示され、読んだり書き込んだりできます。',
  'Les PDF et images que vous importez, et les pages que vous numérisez, apparaissent ici et dans vos notes, pour les lire et écrire dessus.')
t('LIBRARY_FILES', 'From Files', 'De Archivos', 'De Arquivos', 'ファイルから', 'Depuis Fichiers')
t('LIBRARY_PHOTOS', 'From Photos', 'De Fotos', 'De Fotos', '写真から', 'Depuis Photos')
t('LIBRARY_SCAN', 'Scan pages', 'Escanear páginas', 'Escanear páginas', 'ページをスキャン', 'Numériser des pages')
t('LIBRARY_FOLDER', 'Lectures', 'Lecciones', 'Lições', '教材', 'Leçons')
t('DOC_CANT_PICK', 'The files could not be shown.', 'No se pudieron mostrar los archivos.', 'Não foi possível mostrar os arquivos.', 'ファイルを表示できませんでした。', 'Impossible d’afficher les fichiers.')
t('DOC_CANT_OPEN', 'That could not be opened as a PDF or a picture.', 'No se pudo abrir como PDF ni como imagen.', 'Não foi possível abrir como PDF nem como imagem.',
  'PDFや画像として開けませんでした。', 'Impossible de l’ouvrir comme PDF ou image.')
t('DOC_NOT_HERE', 'PDFs open on iPad and Mac for now.', 'Por ahora, los PDF se abren en iPad y Mac.', 'Por enquanto, os PDFs abrem no iPad e no Mac.',
  'PDFは今のところiPadとMacで開けます。', 'Pour l’instant, les PDF s’ouvrent sur iPad et Mac.')
t('DOC_IMPORTED', '“{0}” is in your notes', '«{0}» está en tus notas', '“{0}” está nas suas notas', '「{0}」をノートに追加しました', '« {0} » est dans vos notes')
t('DOC_SCAN_NAME', 'Scan {0}', 'Escaneo {0}', 'Digitalização {0}', 'スキャン {0}', 'Numérisation {0}')
t('DOC_PHOTOS_NAME', 'Photos {0}', 'Fotos {0}', 'Fotos {0}', '写真 {0}', 'Photos {0}')
t('DOC_SEARCH', 'Search', 'Buscar', 'Buscar', '検索', 'Rechercher')
t('DOC_SEARCH_HINT', 'Words to find', 'Palabras a buscar', 'Palavras a buscar', '探す言葉', 'Mots à chercher')
t('DOC_SEARCHING', 'Searching…', 'Buscando…', 'Buscando…', '検索中…', 'Recherche…')
t('DOC_NO_MATCHES', 'Not found', 'Sin resultados', 'Nada encontrado', '見つかりません', 'Introuvable')
t('DOC_BEFORE', 'Previous', 'Anterior', 'Anterior', '前へ', 'Précédent')
t('DOC_NEXT', 'Next', 'Siguiente', 'Próximo', '次へ', 'Suivant')
t('DOC_PAGE', 'Page', 'Página', 'Página', 'ページ', 'Page')
t('DOC_TURN', 'Turn page', 'Girar página', 'Girar página', 'ページを回転', 'Faire pivoter la page')
t('DOC_TURN_FULL', 'No more pages can be turned in this document.', 'No se pueden girar más páginas en este documento.', 'Não é possível girar mais páginas neste documento.', 'この資料ではこれ以上ページを回転できません。', 'Impossible de faire pivoter d’autres pages dans ce document.')
t('DOC_EXPORT', 'Share as PDF', 'Compartir como PDF', 'Compartilhar como PDF', 'PDFで共有', 'Partager en PDF')
t('DOC_EXPORT_FAILED', 'The PDF could not be made.', 'No se pudo crear el PDF.', 'Não foi possível criar o PDF.', 'PDFを作成できませんでした。', 'Impossible de créer le PDF.')
t('DOC_EXPORTED', 'Saved: {0}', 'Guardado: {0}', 'Salvo: {0}', '保存しました：{0}', 'Enregistré : {0}')

# --- days of the week (a date's; Statistics' too) ------------------------------------------------
t('DAY_MON', 'Mo', 'Lu', 'Seg', '月', 'Lu')
t('DAY_TUE', 'Tu', 'Ma', 'Ter', '火', 'Ma')
t('DAY_WED', 'We', 'Mi', 'Qua', '水', 'Me')
t('DAY_THU', 'Th', 'Ju', 'Qui', '木', 'Je')
t('DAY_FRI', 'Fr', 'Vi', 'Sex', '金', 'Ve')
t('DAY_SAT', 'Sa', 'Sá', 'Sáb', '土', 'Sa')
t('DAY_SUN', 'Su', 'Do', 'Dom', '日', 'Di')

# --- dates --------------------------------------------------------------------------------------
t('DATE', '{0} {1} {2}', '{0} {1} {2}', '{0} de {1} de {2}', '{2}年{1}{0}日', '{0} {1} {2}')
t('DATE_TIME', '{0}, {1}', '{0}, {1}', '{0}, {1}', '{0} {1}', '{0}, {1}')
for i, (en, es, pt, fr) in enumerate([('Jan','ene','jan','janv.'),('Feb','feb','fev','févr.'),('Mar','mar','mar','mars'),('Apr','abr','abr','avr.'),
                                      ('May','may','mai','mai'),('Jun','jun','jun','juin'),('Jul','jul','jul','juil.'),('Aug','ago','ago','août'),
                                      ('Sep','sept','set','sept.'),('Oct','oct','out','oct.'),('Nov','nov','nov','nov.'),('Dec','dic','dez','déc.')]):
    t('MONTH_%d' % (i + 1), en, es, pt, '%d月' % (i + 1), fr)
