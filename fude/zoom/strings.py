# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# The deep-zoom canvas's UI strings (fude/zoom), read by the apps built on it
# (apps/sketching/tools/strings.py) after the drawing core's. A row:
# t('ID', English, Spanish, Portuguese (Brazil), Japanese, French).

# The eraser's mode, said as it changes (Erase pressed again: page.h).
t('ZOOM_ERASER_PARTIAL', 'Eraser: rubs out what it touches', 'Borrador: borra lo que toca', 'Borracha: apaga o que toca', '消しゴム：触れた部分を消す', 'Gomme : efface ce qu’elle touche')
t('ZOOM_ERASER_STROKE', 'Eraser: whole strokes', 'Borrador: trazos enteros', 'Borracha: traços inteiros', '消しゴム：線ごと消す', 'Gomme : traits entiers')
t('ZOOM_ERASER_TRIM', 'Eraser: up to the crossings', 'Borrador: hasta los cruces', 'Borracha: até os cruzamentos', '消しゴム：交点まで消す', 'Gomme : jusqu’aux croisements')

# The shapes tool (page.h) and its choices.
t('ZOOM_SHAPES', 'Shapes', 'Formas', 'Formas', '図形', 'Formes')
t('ZOOM_SHAPE_LINE', 'Line', 'Línea', 'Linha', '線', 'Ligne')
t('ZOOM_SHAPE_RECT', 'Rectangle', 'Rectángulo', 'Retângulo', '長方形', 'Rectangle')
t('ZOOM_SHAPE_ELLIPSE', 'Ellipse', 'Elipse', 'Elipse', '楕円', 'Ellipse')
t('ZOOM_SHAPE_TRIANGLE', 'Triangle', 'Triángulo', 'Triângulo', '三角形', 'Triangle')
t('ZOOM_SHAPE_FILLED', 'Filled', 'Relleno', 'Preenchido', '塗りつぶし', 'Rempli')

# The Picture tool (page.h): where a picture comes from.
t('ZOOM_PICTURE', 'Picture', 'Imagen', 'Imagem', '画像', 'Image')
t('ZOOM_PICTURE_PHOTOS', 'Photos', 'Fotos', 'Fotos', '写真', 'Photos')
t('ZOOM_PICTURE_FILES', 'Files', 'Archivos', 'Arquivos', 'ファイル', 'Fichiers')
t('ZOOM_PICTURE_UNREADABLE', 'That picture could not be read', 'No se pudo leer esa imagen', 'Não foi possível ler essa imagem', 'その画像を読み込めませんでした', 'Impossible de lire cette image')

# The Smoothing tool (page.h, smooth.h) and its levels.
t('ZOOM_SMOOTHING', 'Smoothing', 'Suavizado', 'Suavização', '手ブレ補正', 'Lissage')
t('ZOOM_SMOOTH_OFF', 'Off', 'Desactivado', 'Desligado', 'オフ', 'Désactivé')
t('ZOOM_SMOOTH_LOW', 'Low', 'Bajo', 'Baixo', '弱', 'Faible')
t('ZOOM_SMOOTH_MEDIUM', 'Medium', 'Medio', 'Médio', '中', 'Moyen')
t('ZOOM_SMOOTH_HIGH', 'High', 'Alto', 'Alto', '強', 'Fort')
t('ZOOM_SMOOTH_ROPE', 'Rope', 'Cuerda', 'Corda', 'ロープ', 'Corde')

# Getting around (nav.h): the button back to the view a flight left.
t('ZOOM_GO_BACK', 'Back to where you were', 'Volver a donde estabas', 'Voltar para onde você estava', '前の場所に戻る', 'Revenir où vous étiez')

# The Fill tool (page.h, fill.h): a tap fills what is closed under it.
t('ZOOM_FILL', 'Fill', 'Rellenar', 'Preencher', '塗りつぶし', 'Remplir')
t('ZOOM_FILL_NOTHING', 'Tap inside a closed shape or a closed line to fill it', 'Toca dentro de una forma o una línea cerrada para rellenarla', 'Toque dentro de uma forma ou de uma linha fechada para preenchê-la', '閉じた図形や線の内側をタップすると塗りつぶせます', 'Touchez l’intérieur d’une forme ou d’un trait fermé pour le remplir')

# Export (page.h, export.h): the view out of the app.
t('ZOOM_EXPORT', 'Export', 'Exportar', 'Exportar', '書き出し', 'Exporter')
t('ZOOM_EXPORT_PNG', 'Image', 'Imagen', 'Imagem', '画像', 'Image')
t('ZOOM_EXPORT_SVG', 'Drawing (SVG)', 'Dibujo (SVG)', 'Desenho (SVG)', '図 (SVG)', 'Dessin (SVG)')
t('ZOOM_EXPORT_SAVED', 'Saved to {0}', 'Guardado en {0}', 'Salvo em {0}', '{0} に保存しました', 'Enregistré dans {0}')
t('ZOOM_EXPORT_FAILED', 'The view could not be exported', 'No se pudo exportar la vista', 'Não foi possível exportar a vista', '表示を書き出せませんでした', 'Impossible d’exporter la vue')

# Places (page.h): the bookmarks.
t('ZOOM_PLACES', 'Places', 'Lugares', 'Lugares', '場所', 'Lieux')
t('ZOOM_PLACE_MARK', 'Mark this view', 'Guardar esta vista', 'Marcar esta vista', 'この表示を保存', 'Marquer cette vue')
t('ZOOM_PLACE_UNMARK', 'Unmark this place', 'Quitar este lugar', 'Desmarcar este lugar', 'この場所を削除', 'Retirer ce lieu')
t('ZOOM_PLACE_N', 'Place {0}', 'Lugar {0}', 'Lugar {0}', '場所 {0}', 'Lieu {0}')
t('ZOOM_PLACE_MARKED', '{0} marked', '{0} guardado', '{0} marcado', '{0} を保存しました', '{0} marqué')
t('ZOOM_PLACE_UNMARKED', 'Place unmarked', 'Lugar quitado', 'Lugar desmarcado', '場所を削除しました', 'Lieu retiré')
t('ZOOM_PLACE_DROP', 'Remove this place', 'Quitar este lugar', 'Remover este lugar', 'この場所を削除', 'Retirer ce lieu')
