#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Kana's two web pages for the App Store: the privacy policy (privacy.html) and
# the help and support page (index.html), in Kana's five languages, written into
# site/. App Store Connect asks for both URLs: host site/ anywhere static (GitHub
# Pages, Netlify...) and give it the two addresses.
#
#   python3 tools/store/site.py
#
# Set CONTACT below (an address people can write to) before publishing: it
# appears on both pages. The texts follow the app's own names for its menus
# (assets/text/strings.rdel) and what it does (docs/app_store.md); change them
# together.

import html, os

CONTACT = "rde.apps.support@gmail.com"   # the address for questions (shown on both pages)
UPDATED = {"en": "2 October 2026", "es": "2 de octubre de 2026", "pt": "2 de outubro de 2026", "ja": "2026年10月2日", "fr": "2 octobre 2026"}
LANGS   = [("en", "English"), ("es", "Español"), ("pt", "Português"), ("ja", "日本語"), ("fr", "Français")]
GOOGLE_PRIVACY = "https://policies.google.com/privacy"
MLKIT_TERMS    = "https://developers.google.com/ml-kit/terms"

# --- the privacy policy ---------------------------------------------------------------
# Each language: title, updated label, intro, then (heading, [paragraphs or ("ul", [items])]).
PRIVACY = {
"en": dict(
    title="Kana — Privacy policy", updated="Updated", other="Help and support",
    intro="Kana is an app for learning to write Japanese. It is built to work offline: what you write and learn stays on your iPad.",
    sections=[
        ("In short", [("ul", [
            "Kana has no accounts, no ads and no tracking, and collects nothing about you for itself.",
            "Everything you make in Kana is stored only on your iPad.",
            "Some features use Google ML Kit, which works on your iPad but sends Google anonymous diagnostics while it is on. You can switch it off.",
        ])]),
        ("What Kana keeps, and where", [
            "Kana stores your pages and handwriting, your notes, your practice and exam history, your review schedule, your vocabulary and lists, your notes on characters and your settings — on your iPad only. Kana itself never sends them anywhere.",
            "Like any app's data, they are part of your iPad's backups (iCloud Backup, or a computer) when you have those on; Apple keeps those under its own privacy policy.",
            "Settings › Your data › Export puts everything in one file. That file goes only where you choose to send or save it.",
        ]),
        ("Google ML Kit", [
            "Kana uses Google ML Kit to read handwriting (Check, Copy as text, Save word, searching by drawing), to read the text in photos (Text from a photo) and to translate (Translate with Google). The reading and the translating happen on your iPad: your handwriting, your photos and the text you translate are not sent to Google.",
            "The first time each is used, ML Kit downloads its model from Google (handwriting: about 20 MB; translation: about 30 MB per language).",
            "While ML Kit is on, Google receives usage and diagnostics data from it: device information (manufacturer, model, iPadOS version), the app's identifier and version, a per-installation identifier, performance figures, events (such as model downloads) and error codes, and the language set. Google uses them for diagnostics and usage analytics; they are not linked to your identity, and not used for advertising or tracking. See <a href=\"%s\">Google's privacy policy</a> and the <a href=\"%s\">ML Kit terms</a>." % (GOOGLE_PRIVACY, MLKIT_TERMS),
            "To switch it all off: Settings › Handwriting › Read with Google ML Kit. Kana then reads handwriting on its own (a little less accurately), Text from a photo and Translate with Google are unavailable, and nothing is sent to Google.",
        ]),
        ("Camera and photos", [
            "Text from a photo uses the camera only while you use it, once you allow it. A photo you pick is handed over by iPadOS on its own: Kana cannot see the rest of your photos. Pictures are read on your iPad and are neither kept nor sent.",
        ]),
        ("Reading aloud", [
            "Kana reads Japanese aloud with the voices built into iPadOS, on your iPad.",
        ]),
        ("The App Store", [
            "Downloading Kana, purchases, and any crash reports you agree to share with developers are handled by Apple under Apple's privacy policy.",
        ]),
        ("Children", [
            "Kana does not knowingly collect personal information from anyone, children included. The ML Kit diagnostics above are the only data that leave the iPad, and they do not identify a person.",
        ]),
        ("Changes", [
            "If this policy changes, the new version will be published on this page with its date.",
        ]),
        ("Contact", [
            "Questions about privacy: <a href=\"mailto:{contact}\">{contact}</a>.",
        ]),
    ]),
"es": dict(
    title="Kana — Política de privacidad", updated="Actualizada", other="Ayuda y soporte",
    intro="Kana es una app para aprender a escribir japonés. Está pensada para funcionar sin conexión: lo que escribes y aprendes se queda en tu iPad.",
    sections=[
        ("En resumen", [("ul", [
            "Kana no tiene cuentas, ni anuncios, ni seguimiento, y no recopila nada sobre ti para sí.",
            "Todo lo que creas en Kana se guarda solo en tu iPad.",
            "Algunas funciones usan Google ML Kit, que funciona en tu iPad pero envía a Google datos de diagnóstico anónimos mientras está activado. Puedes desactivarlo.",
        ])]),
        ("Qué guarda Kana y dónde", [
            "Kana guarda tus páginas y tu escritura, tus notas, tu historial de práctica y de exámenes, tu calendario de repasos, tu vocabulario y tus listas, tus notas sobre caracteres y tus ajustes, solo en tu iPad. Kana nunca los envía a ningún sitio.",
            "Como los datos de cualquier app, forman parte de las copias de seguridad de tu iPad (Copia en iCloud, o un ordenador) si las tienes activadas; Apple las guarda según su propia política de privacidad.",
            "Ajustes › Tus datos › Exportar lo pone todo en un archivo. Ese archivo solo va adonde tú decidas enviarlo o guardarlo.",
        ]),
        ("Google ML Kit", [
            "Kana usa Google ML Kit para leer la escritura a mano (Revisar, Copiar como texto, Guardar palabra, buscar dibujando), para leer el texto de las fotos (Texto de una foto) y para traducir (Traducir con Google). La lectura y la traducción se hacen en tu iPad: tu escritura, tus fotos y el texto que traduces no se envían a Google.",
            "La primera vez que se usa cada una, ML Kit descarga su modelo de Google (escritura: unos 20 MB; traducción: unos 30 MB por idioma).",
            "Mientras ML Kit está activado, Google recibe de él datos de uso y diagnóstico: información del dispositivo (fabricante, modelo, versión de iPadOS), el identificador y la versión de la app, un identificador por instalación, datos de rendimiento, eventos (como la descarga de modelos) y códigos de error, y el idioma configurado. Google los usa para diagnóstico y analítica de uso; no se vinculan a tu identidad ni se usan para publicidad o seguimiento. Consulta la <a href=\"%s\">política de privacidad de Google</a> y las <a href=\"%s\">condiciones de ML Kit</a>." % (GOOGLE_PRIVACY, MLKIT_TERMS),
            "Para desactivarlo todo: Ajustes › Escritura › Leer con Google ML Kit. Kana lee entonces la escritura por su cuenta (con algo menos de precisión), Texto de una foto y Traducir con Google no están disponibles, y no se envía nada a Google.",
        ]),
        ("Cámara y fotos", [
            "Texto de una foto usa la cámara solo mientras lo usas, y solo si lo permites. iPadOS entrega la foto que eliges por sí sola: Kana no puede ver el resto de tus fotos. Las imágenes se leen en tu iPad y no se guardan ni se envían.",
        ]),
        ("Lectura en voz alta", [
            "Kana lee el japonés en voz alta con las voces de iPadOS, en tu iPad.",
        ]),
        ("La App Store", [
            "La descarga de Kana, las compras y los informes de fallos que aceptes compartir con los desarrolladores los gestiona Apple según su política de privacidad.",
        ]),
        ("Menores", [
            "Kana no recopila a sabiendas información personal de nadie, tampoco de menores. Los diagnósticos de ML Kit descritos arriba son los únicos datos que salen del iPad, y no identifican a ninguna persona.",
        ]),
        ("Cambios", [
            "Si esta política cambia, la nueva versión se publicará en esta página con su fecha.",
        ]),
        ("Contacto", [
            "Preguntas sobre privacidad: <a href=\"mailto:{contact}\">{contact}</a>.",
        ]),
    ]),
"pt": dict(
    title="Kana — Política de privacidade", updated="Atualizada em", other="Ajuda e suporte",
    intro="O Kana é um app para aprender a escrever japonês. Ele foi feito para funcionar offline: o que você escreve e aprende fica no seu iPad.",
    sections=[
        ("Em resumo", [("ul", [
            "O Kana não tem contas, anúncios nem rastreamento, e não coleta nada sobre você para si.",
            "Tudo o que você cria no Kana fica guardado só no seu iPad.",
            "Alguns recursos usam o Google ML Kit, que funciona no seu iPad mas envia ao Google dados de diagnóstico anônimos enquanto está ligado. Você pode desligá-lo.",
        ])]),
        ("O que o Kana guarda, e onde", [
            "O Kana guarda suas páginas e sua escrita, suas notas, seu histórico de prática e de provas, sua agenda de revisões, seu vocabulário e suas listas, suas notas sobre caracteres e suas configurações — só no seu iPad. O próprio Kana nunca os envia a lugar nenhum.",
            "Como os dados de qualquer app, eles fazem parte dos backups do seu iPad (Backup do iCloud, ou um computador) quando estão ativados; a Apple os guarda segundo a própria política de privacidade.",
            "Configurações › Seus dados › Exportar coloca tudo em um arquivo. Esse arquivo só vai para onde você escolher enviá-lo ou salvá-lo.",
        ]),
        ("Google ML Kit", [
            "O Kana usa o Google ML Kit para ler a escrita à mão (Verificar, Copiar como texto, Salvar palavra, buscar desenhando), para ler o texto de fotos (Texto de uma foto) e para traduzir (Traduzir com o Google). A leitura e a tradução acontecem no seu iPad: sua escrita, suas fotos e o texto que você traduz não são enviados ao Google.",
            "Na primeira vez que cada um é usado, o ML Kit baixa seu modelo do Google (escrita: cerca de 20 MB; tradução: cerca de 30 MB por idioma).",
            "Enquanto o ML Kit está ligado, o Google recebe dele dados de uso e diagnóstico: informações do dispositivo (fabricante, modelo, versão do iPadOS), o identificador e a versão do app, um identificador por instalação, dados de desempenho, eventos (como o download de modelos) e códigos de erro, e o idioma configurado. O Google os usa para diagnóstico e análise de uso; eles não são vinculados à sua identidade nem usados para publicidade ou rastreamento. Veja a <a href=\"%s\">política de privacidade do Google</a> e os <a href=\"%s\">termos do ML Kit</a>." % (GOOGLE_PRIVACY, MLKIT_TERMS),
            "Para desligar tudo: Configurações › Escrita › Ler com o Google ML Kit. O Kana passa a ler a escrita por conta própria (com um pouco menos de precisão), Texto de uma foto e Traduzir com o Google ficam indisponíveis, e nada é enviado ao Google.",
        ]),
        ("Câmera e fotos", [
            "Texto de uma foto usa a câmera só enquanto você o usa, e só se você permitir. O iPadOS entrega apenas a foto que você escolhe: o Kana não vê o resto das suas fotos. As imagens são lidas no seu iPad e não são guardadas nem enviadas.",
        ]),
        ("Leitura em voz alta", [
            "O Kana lê japonês em voz alta com as vozes do iPadOS, no seu iPad.",
        ]),
        ("A App Store", [
            "O download do Kana, as compras e os relatórios de falhas que você aceitar compartilhar com desenvolvedores são tratados pela Apple, segundo a política de privacidade da Apple.",
        ]),
        ("Crianças", [
            "O Kana não coleta conscientemente informações pessoais de ninguém, incluindo crianças. Os diagnósticos do ML Kit descritos acima são os únicos dados que saem do iPad, e não identificam ninguém.",
        ]),
        ("Mudanças", [
            "Se esta política mudar, a nova versão será publicada nesta página com a data.",
        ]),
        ("Contato", [
            "Dúvidas sobre privacidade: <a href=\"mailto:{contact}\">{contact}</a>.",
        ]),
    ]),
"ja": dict(
    title="Kana — プライバシーポリシー", updated="更新日", other="ヘルプとサポート",
    intro="Kanaは日本語を書いて学ぶためのアプリです。オフラインで使えるように作られており、書いたものや学んだことはお使いのiPadの中に残ります。",
    sections=[
        ("概要", [("ul", [
            "Kanaにはアカウントも広告もトラッキングもなく、Kana自身があなたについての情報を集めることはありません。",
            "Kanaで作ったものはすべて、お使いのiPadの中だけに保存されます。",
            "一部の機能はGoogle ML Kitを使います。ML KitはiPad上で動きますが、オンの間は匿名の診断データをGoogleに送信します。オフにすることもできます。",
        ])]),
        ("Kanaが保存するもの、保存する場所", [
            "Kanaは、ページと手書きの文字、メモ、練習とテストの履歴、復習の予定、単語帳とリスト、文字ごとのメモ、設定を、お使いのiPadの中だけに保存します。Kana自身がそれらをどこかへ送ることはありません。",
            "ほかのアプリのデータと同じく、iPadのバックアップ（iCloudバックアップやコンピュータ）がオンなら、その中に含まれます。それらはAppleがAppleのプライバシーポリシーのもとで扱います。",
            "設定 › データ › 書き出す で、すべてを1つのファイルにまとめられます。そのファイルは、あなたが送ったり保存したりする先にだけ渡ります。",
        ]),
        ("Google ML Kit", [
            "Kanaは、手書き文字の読み取り（チェック、テキストでコピー、単語を保存、描いて検索）、写真の文字の読み取り（写真の文字）、翻訳（Googleで翻訳）にGoogle ML Kitを使います。読み取りと翻訳はiPadの中で行われ、手書きの文字、写真、翻訳する文はGoogleに送られません。",
            "それぞれ初めて使うときに、ML KitがGoogleからモデルをダウンロードします（手書き：約20 MB、翻訳：言語ごとに約30 MB）。",
            "ML Kitがオンの間、Googleは利用状況と診断のデータを受け取ります：端末の情報（メーカー、機種、iPadOSのバージョン）、アプリの識別子とバージョン、インストールごとの識別子、パフォーマンスの数値、イベント（モデルのダウンロードなど）とエラーコード、設定されている言語。Googleはこれらを診断と利用状況の分析に使い、あなたの身元とは結び付けず、広告やトラッキングには使いません。<a href=\"%s\">Googleのプライバシーポリシー</a>と<a href=\"%s\">ML Kitの利用規約</a>をご覧ください。" % (GOOGLE_PRIVACY, MLKIT_TERMS),
            "すべてオフにするには：設定 › 手書き認識 › Google ML Kitで読み取る。オフの間、Kanaは自分で手書き文字を読み取り（精度は少し下がります）、写真の文字とGoogleで翻訳は使えなくなり、Googleには何も送られません。",
        ]),
        ("カメラと写真", [
            "写真の文字は、あなたが許可したうえで、使っている間だけカメラを使います。選んだ写真はiPadOSがその1枚だけを渡すので、Kanaがほかの写真を見ることはできません。画像はiPadの中で読み取られ、保存も送信もされません。",
        ]),
        ("読み上げ", [
            "Kanaは、iPadOSに内蔵された声を使ってiPadの中で日本語を読み上げます。",
        ]),
        ("App Store", [
            "Kanaのダウンロード、購入、開発者と共有することに同意したクラッシュレポートは、Appleのプライバシーポリシーのもとで、Appleが扱います。",
        ]),
        ("お子さまについて", [
            "Kanaは、お子さまを含め、誰の個人情報も意図して集めることはありません。iPadの外に出るデータは上記のML Kitの診断データだけで、それは個人を特定するものではありません。",
        ]),
        ("変更", [
            "このポリシーを変更する場合は、新しい版を日付とともにこのページに掲載します。",
        ]),
        ("お問い合わせ", [
            "プライバシーについてのご質問：<a href=\"mailto:{contact}\">{contact}</a>",
        ]),
    ]),
"fr": dict(
    title="Kana — Politique de confidentialité", updated="Mise à jour", other="Aide et assistance",
    intro="Kana est une app pour apprendre à écrire le japonais. Elle est conçue pour fonctionner hors ligne : ce que vous écrivez et apprenez reste sur votre iPad.",
    sections=[
        ("En bref", [("ul", [
            "Kana n’a ni compte, ni publicité, ni pistage, et ne recueille rien sur vous pour elle-même.",
            "Tout ce que vous créez dans Kana est enregistré uniquement sur votre iPad.",
            "Certaines fonctions utilisent Google ML Kit, qui fonctionne sur votre iPad mais envoie à Google des données de diagnostic anonymes tant qu’il est activé. Vous pouvez le désactiver.",
        ])]),
        ("Ce que Kana conserve, et où", [
            "Kana conserve vos pages et votre écriture, vos notes, votre historique d’entraînement et d’examens, votre calendrier de révisions, votre vocabulaire et vos listes, vos notes sur les caractères et vos réglages — uniquement sur votre iPad. Kana elle-même ne les envoie nulle part.",
            "Comme les données de toute app, elles font partie des sauvegardes de votre iPad (Sauvegarde iCloud, ou un ordinateur) si elles sont activées ; Apple les conserve selon sa propre politique de confidentialité.",
            "Réglages › Vos données › Exporter rassemble tout dans un fichier. Ce fichier ne va que là où vous choisissez de l’envoyer ou de l’enregistrer.",
        ]),
        ("Google ML Kit", [
            "Kana utilise Google ML Kit pour lire l’écriture manuscrite (Vérifier, Copier en texte, Enregistrer, rechercher en dessinant), pour lire le texte des photos (Texte d’une photo) et pour traduire (Traduire avec Google). La lecture et la traduction se font sur votre iPad : votre écriture, vos photos et le texte que vous traduisez ne sont pas envoyés à Google.",
            "À la première utilisation de chacune, ML Kit télécharge son modèle depuis Google (écriture : environ 20 Mo ; traduction : environ 30 Mo par langue).",
            "Tant que ML Kit est activé, Google en reçoit des données d’utilisation et de diagnostic : informations sur l’appareil (fabricant, modèle, version d’iPadOS), l’identifiant et la version de l’app, un identifiant propre à l’installation, des mesures de performance, des événements (comme les téléchargements de modèles) et des codes d’erreur, et la langue choisie. Google les utilise pour le diagnostic et l’analyse d’utilisation ; elles ne sont pas liées à votre identité et ne servent ni à la publicité ni au pistage. Voir la <a href=\"%s\">politique de confidentialité de Google</a> et les <a href=\"%s\">conditions de ML Kit</a>." % (GOOGLE_PRIVACY, MLKIT_TERMS),
            "Pour tout désactiver : Réglages › Écriture › Lire avec Google ML Kit. Kana lit alors l’écriture par elle-même (un peu moins précisément), Texte d’une photo et Traduire avec Google ne sont plus disponibles, et rien n’est envoyé à Google.",
        ]),
        ("Appareil photo et photos", [
            "Texte d’une photo n’utilise l’appareil photo que pendant que vous vous en servez, et seulement si vous l’autorisez. iPadOS ne transmet que la photo que vous choisissez : Kana ne voit pas le reste de vos photos. Les images sont lues sur votre iPad et ne sont ni conservées ni envoyées.",
        ]),
        ("Lecture à voix haute", [
            "Kana lit le japonais à voix haute avec les voix intégrées à iPadOS, sur votre iPad.",
        ]),
        ("L’App Store", [
            "Le téléchargement de Kana, les achats et les rapports de plantage que vous acceptez de partager avec les développeurs sont gérés par Apple, selon la politique de confidentialité d’Apple.",
        ]),
        ("Enfants", [
            "Kana ne recueille sciemment aucune information personnelle, y compris celles d’enfants. Les diagnostics de ML Kit décrits ci-dessus sont les seules données qui quittent l’iPad, et ils n’identifient personne.",
        ]),
        ("Modifications", [
            "Si cette politique change, la nouvelle version sera publiée sur cette page avec sa date.",
        ]),
        ("Contact", [
            "Questions sur la confidentialité : <a href=\"mailto:{contact}\">{contact}</a>.",
        ]),
    ]),
}

# --- help and support ------------------------------------------------------------------
SUPPORT = {
"en": dict(
    title="Kana — Help and support", other="Privacy policy",
    intro="Kana teaches you to write Japanese by hand: kana and kanji stroke by stroke, your own vocabulary, exams and reviews — on iPad, with the Apple Pencil or your finger, offline.",
    faq=[
        ("Do I need an Apple Pencil?", "No. With the hand on the toolbar on, one finger writes and two move the page. With an Apple Pencil, Kana switches to it by itself: the Pencil writes, fingers move the page, and your hand can rest on the screen."),
        ("Does Kana work offline?", "Yes. Everything works offline once the handwriting model (about 20 MB) — and, for Translate with Google, each language's model — has been downloaded the first time it is used."),
        ("Where is my work kept? Can I move it to a new iPad?", "On your iPad, and in its iCloud Backup when that is on. To keep a copy yourself or move it: Settings › Your data › Export, then Import on the other iPad."),
        ("Handwriting is not read well", "In Settings › Handwriting, Read with Google ML Kit should be on and its model downloaded (it needs the internet once). With it off, Kana reads handwriting on its own, less accurately."),
        ("The Japanese voice sounds robotic", "Download a better Japanese voice in iPadOS: Settings › Accessibility › Spoken Content › Voices › Japanese."),
        ("How do reviews work?", "Mark characters as Studying, or save words to your Vocabulary. Reviews brings each back when it is due: the next day, then after three days, then longer each time you write it right — sooner when you do not."),
        ("Can I print practice sheets?", "Yes: Sheet, in a character's page, in Select mode or in Vocabulary, makes a PDF to print or share."),
    ],
    contact_title="Contact",
    contact="Write to <a href=\"mailto:{contact}\">{contact}</a>. If something goes wrong, tell us your iPad model and iPadOS version, and what you were doing."),
"es": dict(
    title="Kana — Ayuda y soporte", other="Política de privacidad",
    intro="Kana te enseña a escribir japonés a mano: kana y kanji trazo a trazo, tu propio vocabulario, exámenes y repasos — en iPad, con el Apple Pencil o con el dedo, sin conexión.",
    faq=[
        ("¿Necesito un Apple Pencil?", "No. Con la mano de la barra activada, un dedo escribe y dos mueven la página. Si usas un Apple Pencil, Kana cambia a él sola: escribe el Pencil, los dedos mueven la página y puedes apoyar la mano en la pantalla."),
        ("¿Kana funciona sin conexión?", "Sí. Todo funciona sin conexión una vez descargado el modelo de escritura (unos 20 MB) —y, para Traducir con Google, el modelo de cada idioma— la primera vez que se usa."),
        ("¿Dónde se guarda lo que hago? ¿Puedo pasarlo a otro iPad?", "En tu iPad, y en su Copia en iCloud si está activada. Para guardar una copia tú mismo o pasarlo: Ajustes › Tus datos › Exportar, y luego Importar en el otro iPad."),
        ("No reconoce bien mi escritura", "En Ajustes › Escritura, Leer con Google ML Kit debe estar activado y su modelo descargado (necesita internet una vez). Desactivado, Kana lee la escritura por su cuenta, con menos precisión."),
        ("La voz japonesa suena robótica", "Descarga una voz japonesa mejor en iPadOS: Ajustes › Accesibilidad › Contenido leído › Voces › Japonés."),
        ("¿Cómo funcionan los repasos?", "Marca caracteres como Estudiando, o guarda palabras en tu Vocabulario. Repasos te los trae cuando toca: al día siguiente, luego a los tres días, y cada vez más tarde si lo escribes bien; antes si no."),
        ("¿Puedo imprimir hojas de práctica?", "Sí: Hoja, en la página de un carácter, en el modo Seleccionar o en Vocabulario, crea un PDF para imprimir o compartir."),
    ],
    contact_title="Contacto",
    contact="Escribe a <a href=\"mailto:{contact}\">{contact}</a>. Si algo falla, indica el modelo de tu iPad, la versión de iPadOS y qué estabas haciendo."),
"pt": dict(
    title="Kana — Ajuda e suporte", other="Política de privacidade",
    intro="O Kana ensina você a escrever japonês à mão: kana e kanji traço a traço, seu próprio vocabulário, provas e revisões — no iPad, com o Apple Pencil ou com o dedo, offline.",
    faq=[
        ("Preciso de um Apple Pencil?", "Não. Com a mão da barra ligada, um dedo escreve e dois movem a página. Se você usar um Apple Pencil, o Kana muda para ele sozinho: o Pencil escreve, os dedos movem a página e você pode apoiar a mão na tela."),
        ("O Kana funciona offline?", "Sim. Tudo funciona offline depois que o modelo de escrita (cerca de 20 MB) — e, para Traduzir com o Google, o modelo de cada idioma — for baixado na primeira vez em que é usado."),
        ("Onde fica o que eu faço? Posso passar para outro iPad?", "No seu iPad, e no Backup do iCloud dele quando ativado. Para guardar uma cópia você mesmo ou transferir: Configurações › Seus dados › Exportar, e depois Importar no outro iPad."),
        ("Minha escrita não é bem reconhecida", "Em Configurações › Escrita, Ler com o Google ML Kit deve estar ligado e o modelo baixado (precisa de internet uma vez). Desligado, o Kana lê a escrita por conta própria, com menos precisão."),
        ("A voz em japonês soa robótica", "Baixe uma voz japonesa melhor no iPadOS: Ajustes › Acessibilidade › Conteúdo Falado › Vozes › Japonês."),
        ("Como funcionam as revisões?", "Marque caracteres como Estudando, ou salve palavras no seu Vocabulário. Revisões traz cada um de volta no momento certo: no dia seguinte, depois em três dias, e cada vez mais tarde quando você acerta; mais cedo quando erra."),
        ("Posso imprimir folhas de prática?", "Sim: Folha, na página de um caractere, no modo Selecionar ou em Vocabulário, cria um PDF para imprimir ou compartilhar."),
    ],
    contact_title="Contato",
    contact="Escreva para <a href=\"mailto:{contact}\">{contact}</a>. Se algo der errado, informe o modelo do seu iPad, a versão do iPadOS e o que você estava fazendo."),
"ja": dict(
    title="Kana — ヘルプとサポート", other="プライバシーポリシー",
    intro="Kanaは、手で日本語を書くことを学ぶアプリです。かなと漢字を一画ずつ、自分の単語帳、テストと復習を、iPadで、Apple Pencilでも指でも、オフラインで。",
    faq=[
        ("Apple Pencilは必要ですか？", "いいえ。バーの手のボタンがオンなら、指1本で書き、2本指でページを動かします。Apple Pencilを使うと自動で切り替わり、Pencilで書いて指でページを動かすので、手を画面に置いたまま書けます。"),
        ("オフラインで使えますか？", "はい。手書きのモデル（約20 MB）と、Googleで翻訳を使う場合は各言語のモデルを、初めて使うときにダウンロードすれば、あとはすべてオフラインで使えます。"),
        ("作ったものはどこに保存されますか？新しいiPadに移せますか？", "お使いのiPadの中と、オンになっていればそのiCloudバックアップに保存されます。自分でコピーを取ったり移したりするには：設定 › データ › 書き出す、そして新しいiPadで 読み込む。"),
        ("手書きの文字がうまく読み取られません", "設定 › 手書き認識 で「Google ML Kitで読み取る」がオンで、モデルがダウンロード済みか確認してください（一度だけインターネットが必要です）。オフの場合、Kanaは自分で読み取りますが、精度は下がります。"),
        ("日本語の声が機械的に聞こえます", "iPadOSでより良い日本語の声をダウンロードしてください：設定 › アクセシビリティ › 読み上げコンテンツ › 声 › 日本語。"),
        ("復習はどのような仕組みですか？", "文字を「学習中」にするか、言葉を単語帳に保存してください。復習は、その時期が来たものを出題します。翌日、次に3日後、正しく書けるたびに間隔が長くなり、書けなければ早めに戻ってきます。"),
        ("練習シートを印刷できますか？", "はい。文字のページ、選択モード、単語帳の「練習シート」で、印刷や共有のできるPDFを作れます。"),
    ],
    contact_title="お問い合わせ",
    contact="<a href=\"mailto:{contact}\">{contact}</a> までご連絡ください。不具合の場合は、iPadの機種、iPadOSのバージョン、何をしていたかをお知らせください。"),
"fr": dict(
    title="Kana — Aide et assistance", other="Politique de confidentialité",
    intro="Kana vous apprend à écrire le japonais à la main : kana et kanji trait par trait, votre propre vocabulaire, examens et révisions — sur iPad, avec l’Apple Pencil ou du doigt, hors ligne.",
    faq=[
        ("Faut-il un Apple Pencil ?", "Non. Avec la main de la barre activée, un doigt écrit et deux déplacent la page. Avec un Apple Pencil, Kana bascule d’elle-même : le Pencil écrit, les doigts déplacent la page, et vous pouvez poser la main sur l’écran."),
        ("Kana fonctionne-t-elle hors ligne ?", "Oui. Tout fonctionne hors ligne une fois téléchargés, à la première utilisation, le modèle d’écriture (environ 20 Mo) et, pour Traduire avec Google, le modèle de chaque langue."),
        ("Où est enregistré mon travail ? Puis-je le passer sur un autre iPad ?", "Sur votre iPad, et dans sa Sauvegarde iCloud si elle est activée. Pour en garder une copie vous-même ou le transférer : Réglages › Vos données › Exporter, puis Importer sur l’autre iPad."),
        ("Mon écriture est mal reconnue", "Dans Réglages › Écriture, Lire avec Google ML Kit doit être activé et son modèle téléchargé (il faut internet une fois). Désactivé, Kana lit l’écriture par elle-même, moins précisément."),
        ("La voix japonaise semble robotique", "Téléchargez une meilleure voix japonaise dans iPadOS : Réglages › Accessibilité › Contenu énoncé › Voix › Japonais."),
        ("Comment fonctionnent les révisions ?", "Marquez des caractères En cours, ou enregistrez des mots dans votre Vocabulaire. Révisions vous les ramène au bon moment : le lendemain, puis trois jours plus tard, et de plus en plus tard à chaque bonne réponse — plus tôt sinon."),
        ("Puis-je imprimer des feuilles d’entraînement ?", "Oui : Feuille, sur la page d’un caractère, en mode Sélectionner ou dans Vocabulaire, crée un PDF à imprimer ou partager."),
    ],
    contact_title="Contact",
    contact="Écrivez à <a href=\"mailto:{contact}\">{contact}</a>. En cas de problème, indiquez le modèle de votre iPad, la version d’iPadOS et ce que vous faisiez."),
}

CSS = """
:root { --page:#f6f3ec; --ink:#1d1d22; --soft:#5f5f6b; --line:#ddd8cc; --card:#fffdf8; --accent:#3b6fe0; }
@media (prefers-color-scheme: dark) { :root { --page:#17181c; --ink:#ececf0; --soft:#a3a3ae; --line:#2e3038; --card:#1f2026; --accent:#7ea2ff; } }
* { box-sizing: border-box; }
body { margin:0; background:var(--page); color:var(--ink); font:16px/1.6 -apple-system, BlinkMacSystemFont, "Hiragino Sans", "Noto Sans JP", "Segoe UI", sans-serif; }
main { max-width:760px; margin:0 auto; padding:32px 16px 64px; }
header { display:flex; align-items:center; gap:14px; margin-bottom:8px; }
.mark { width:44px; height:44px; border-radius:11px; background:var(--accent); color:#fff; display:grid; place-items:center; font-size:24px; flex:none; }
h1 { font-size:26px; line-height:1.25; margin:0; }
h2 { font-size:19px; margin:32px 0 6px; }
h3 { font-size:16px; margin:22px 0 4px; }
p, li { color:var(--ink); }
.soft { color:var(--soft); font-size:14px; }
nav.langs { display:flex; flex-wrap:wrap; gap:8px; margin:18px 0 22px; }
nav.langs a { padding:6px 12px; border-radius:999px; border:1px solid var(--line); color:var(--ink); text-decoration:none; font-size:14px; }
nav.langs a.on { background:var(--accent); border-color:var(--accent); color:#fff; }
a { color:var(--accent); }
section.lang { display:none; }
section.lang.on { display:block; }
.card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:4px 18px 14px; margin-top:18px; }
footer { margin-top:40px; border-top:1px solid var(--line); padding-top:14px; }
"""

JS = """
(function(){
  var codes = %s, pick = (location.hash || '').slice(1);
  if(codes.indexOf(pick) < 0) {
    var langs = navigator.languages || [navigator.language || 'en'];
    for(var i = 0; i < langs.length && codes.indexOf(pick) < 0; i++) { pick = (langs[i] || '').slice(0, 2).toLowerCase(); }
  }
  if(codes.indexOf(pick) < 0) { pick = 'en'; }
  function show(c){
    codes.forEach(function(x){
      document.getElementById('s-' + x).classList.toggle('on', x === c);
      document.getElementById('l-' + x).classList.toggle('on', x === c);
    });
    document.documentElement.lang = c === 'pt' ? 'pt-BR' : c;
  }
  codes.forEach(function(c){ document.getElementById('l-' + c).addEventListener('click', function(e){ e.preventDefault(); history.replaceState(null, '', '#' + c); show(c); }); });
  show(pick);
})();
"""

def fill(s):
    return s.replace("{contact}", html.escape(CONTACT))

def page(title, sections_html, other_href, other_label_by_lang):
    nav = "".join('<a id="l-%s" href="#%s">%s</a>' % (c, c, n) for c, n in LANGS)
    codes = "[" + ",".join("'%s'" % c for c, _ in LANGS) + "]"
    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%s</title>
<style>%s</style>
</head>
<body>
<main>
<header><div class="mark">字</div><h1>Kana</h1></header>
<nav class="langs">%s</nav>
<noscript><style>section.lang { display:block; border-top:1px solid var(--line); }</style></noscript>
%s
</main>
<script>%s</script>
</body>
</html>
""" % (html.escape(title), CSS, nav, sections_html, JS % codes)

def privacy_html():
    out = []
    for c, _ in LANGS:
        d = PRIVACY[c]
        body = ['<h2>%s</h2>' % html.escape(d["title"]), '<p class="soft">%s: %s</p>' % (d["updated"], UPDATED[c]), "<p>%s</p>" % d["intro"]]
        for heading, items in d["sections"]:
            body.append("<h3>%s</h3>" % html.escape(heading))
            for it in items:
                if isinstance(it, tuple):
                    body.append("<ul>" + "".join("<li>%s</li>" % fill(x) for x in it[1]) + "</ul>")
                else:
                    body.append("<p>%s</p>" % fill(it))
        body.append('<footer><a href="index.html#%s">%s</a></footer>' % (c, html.escape(d["other"])))
        out.append('<section class="lang" id="s-%s" lang="%s">%s</section>' % (c, "pt-BR" if c == "pt" else c, "\n".join(body)))
    return page("Kana — Privacy policy", "\n".join(out), "index.html", None)

def support_html():
    out = []
    for c, _ in LANGS:
        d = SUPPORT[c]
        body = ['<h2>%s</h2>' % html.escape(d["title"]), "<p>%s</p>" % d["intro"], '<div class="card">']
        for q, a in d["faq"]:
            body.append("<h3>%s</h3><p>%s</p>" % (html.escape(q), fill(a)))
        body.append("</div>")
        body.append("<h3>%s</h3><p>%s</p>" % (html.escape(d["contact_title"]), fill(d["contact"])))
        body.append('<footer><a href="privacy.html#%s">%s</a></footer>' % (c, html.escape(d["other"])))
        out.append('<section class="lang" id="s-%s" lang="%s">%s</section>' % (c, "pt-BR" if c == "pt" else c, "\n".join(body)))
    return page("Kana — Help and support", "\n".join(out), "privacy.html", None)

if __name__ == "__main__":
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "site")
    os.makedirs(root, exist_ok=True)
    for name, text in (("privacy.html", privacy_html()), ("index.html", support_html())):
        with open(os.path.join(root, name), "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote", os.path.normpath(os.path.join(root, name)))
    if CONTACT == "CONTACT_EMAIL":
        print("NOTE: set CONTACT at the top of this file before publishing.")
