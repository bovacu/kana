// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ICONS
#define FUDE_ICONS

// ===========================================================================
// The icons: Phosphor (MIT, assets/fonts/LICENSE-Phosphor.txt), as text. Its
// Regular weight is the UI font's last fallback (after Noto Sans JP), so an icon
// is a string: a label can hold one, or one beside words ("<icon> Practice"),
// in the label's colour and at any size (Slug). The Fill weight has the same
// code points; it is a font of its own (fude_ui.font_icons_fill), for an
// icon that shows something ON: a chosen tick, a studied star.
//
// Code points from Phosphor 2.1's stylesheet (Fonts/regular/style.css), one
// per icon the app uses; add more from there (data/raw/phosphor-icons/, not
// shipped — only the two fonts in assets/fonts/ are).
// ===========================================================================

#define FUDE_ICON_UNDO          "\xEE\x80\xB8"   // ph-arrow-counter-clockwise U+E038
#define FUDE_ICON_REDO          "\xEE\x80\xB6"   // ph-arrow-clockwise U+E036
#define FUDE_ICON_DRAW          "\xEE\x8E\xB4"   // ph-pencil-simple U+E3B4
#define FUDE_ICON_ERASE         "\xEE\x88\x9E"   // ph-eraser U+E21E
#define FUDE_ICON_LASSO         "\xEE\xB7\x86"   // ph-lasso U+EDC6
#define FUDE_ICON_TRASH         "\xEE\x92\xA6"   // ph-trash U+E4A6
#define FUDE_ICON_PALETTE       "\xEE\x9B\x88"   // ph-palette U+E6C8
#define FUDE_ICON_PAPER_DOTS    "\xEE\x87\xBC"   // ph-dots-nine U+E1FC
#define FUDE_ICON_PAPER_LINES   "\xEE\x96\xA2"   // ph-rows U+E5A2
#define FUDE_ICON_PAPER_SQUARES "\xEE\x8A\x96"   // ph-grid-four U+E296
#define FUDE_ICON_PAPER_NONE    "\xEE\x91\x9E"   // ph-square U+E45E
#define FUDE_ICON_RESET_VIEW    "\xEE\x87\x96"   // ph-crosshair U+E1D6
#define FUDE_ICON_ROTATE        "\xEE\xB7\xB2"   // ph-device-rotate U+EDF2
#define FUDE_ICON_BACK          "\xEE\x81\x98"   // ph-arrow-left U+E058
#define FUDE_ICON_PREV          "\xEE\x84\xB8"   // ph-caret-left U+E138
#define FUDE_ICON_NEXT          "\xEE\x84\xBA"   // ph-caret-right U+E13A
#define FUDE_ICON_CLOSE         "\xEE\x93\xB6"   // ph-x U+E4F6
#define FUDE_ICON_MENU          "\xEE\x8B\xB0"   // ph-list U+E2F0
#define FUDE_ICON_SETTINGS      "\xEE\x89\xB2"   // ph-gear-six U+E272
#define FUDE_ICON_PLAY          "\xEE\x8F\x90"   // ph-play U+E3D0
#define FUDE_ICON_REPLAY        "\xEE\x8F\xB6"   // ph-repeat U+E3F6
#define FUDE_ICON_CHECK         "\xEE\x86\x82"   // ph-check U+E182
#define FUDE_ICON_CHECK_CIRCLE  "\xEE\x86\x84"   // ph-check-circle U+E184
#define FUDE_ICON_CIRCLE        "\xEE\x86\x8A"   // ph-circle U+E18A
#define FUDE_ICON_CHECKS        "\xEE\x94\xBA"   // ph-checks U+E53A
#define FUDE_ICON_SELECT        "\xEE\x9A\x9A"   // ph-selection U+E69A
#define FUDE_ICON_SELECT_ALL    "\xEE\x9D\x86"   // ph-selection-all U+E746
#define FUDE_ICON_SELECT_NONE   "\xEE\x9A\x9E"   // ph-selection-slash U+E69E
#define FUDE_ICON_LIST_CHECKS   "\xEE\xAB\x9C"   // ph-list-checks U+EADC
#define FUDE_ICON_STAR          "\xEE\x91\xAA"   // ph-star U+E46A
#define FUDE_ICON_KNOWN         "\xEE\x98\x86"   // ph-seal-check U+E606
#define FUDE_ICON_GRADUATION    "\xEE\x98\xAC"   // ph-graduation-cap U+E62C
#define FUDE_ICON_EXAM          "\xEE\x9D\x82"   // ph-exam U+E742
#define FUDE_ICON_CHART_LINE    "\xEE\x85\x96"   // ph-chart-line-up U+E156
#define FUDE_ICON_CHART_BAR     "\xEE\x85\x90"   // ph-chart-bar U+E150
#define FUDE_ICON_CALENDAR      "\xEE\x84\x8A"   // ph-calendar-blank U+E10A
#define FUDE_ICON_STREAK        "\xEE\x89\x82"   // ph-fire U+E242
#define FUDE_ICON_TROPHY        "\xEE\x99\xBE"   // ph-trophy U+E67E
#define FUDE_ICON_TARGET        "\xEE\x91\xBC"   // ph-target U+E47C
#define FUDE_ICON_CLOCK         "\xEE\x86\x9A"   // ph-clock U+E19A
#define FUDE_ICON_BOOKS         "\xEE\x9D\x98"   // ph-books U+E758
#define FUDE_ICON_BOOK          "\xEE\x83\xA6"   // ph-book-open U+E0E6
#define FUDE_ICON_SEARCH        "\xEE\x8C\x8C"   // ph-magnifying-glass U+E30C
#define FUDE_ICON_MARKER        "\xEE\xB1\xB6"   // ph-highlighter U+EC76 (the marker tool)
#define FUDE_ICON_UP            "\xEE\x84\xBC"   // ph-caret-up U+E13C (a search's previous match)
#define FUDE_ICON_DOWN          "\xEE\x84\xB6"   // ph-caret-down U+E136 (...its next)
#define FUDE_ICON_PAGE_NUMBER   "\xEE\x8A\xA2"   // ph-hash U+E2A2 (go to a page)
#define FUDE_ICON_TURN_PAGE     "\xEE\x80\x96"   // ph-arrow-arc-right U+E016 (a document's page turned)
#define FUDE_ICON_FILTER        "\xEE\x89\xA6"   // ph-funnel U+E266
#define FUDE_ICON_SORT          "\xEE\x91\x84"   // ph-sort-ascending U+E444
#define FUDE_ICON_FOLDER        "\xEE\x89\x8A"   // ph-folder U+E24A
#define FUDE_ICON_FOLDER_ADD    "\xEE\x89\x98"   // ph-folder-plus U+E258
#define FUDE_ICON_FILE_ADD      "\xEE\x88\xB6"   // ph-file-plus U+E236
#define FUDE_ICON_NOTE          "\xEE\x8D\x8C"   // ph-note-pencil U+E34C
#define FUDE_ICON_TRANSLATE     "\xEE\x92\xA2"   // ph-translate U+E4A2
#define FUDE_ICON_INFO          "\xEE\x8B\x8E"   // ph-info U+E2CE
#define FUDE_ICON_HINT          "\xEE\x8B\x9C"   // ph-lightbulb U+E2DC
#define FUDE_ICON_SHOW          "\xEE\x88\xA0"   // ph-eye U+E220
#define FUDE_ICON_HIDE          "\xEE\x88\xA4"   // ph-eye-slash U+E224
#define FUDE_ICON_PLUS          "\xEE\x8F\x94"   // ph-plus U+E3D4
#define FUDE_ICON_MINUS         "\xEE\x8C\xAA"   // ph-minus U+E32A
#define FUDE_ICON_FULLSCREEN    "\xEE\x98\xA6"   // ph-frame-corners U+E626
#define FUDE_ICON_PRACTICE      "\xEE\x9D\x8E"   // ph-brain U+E74E
#define FUDE_ICON_SHUFFLE       "\xEE\x90\xA2"   // ph-shuffle U+E422
#define FUDE_ICON_STACK         "\xEE\x91\xA6"   // ph-stack U+E466
#define FUDE_ICON_FLAG          "\xEE\x89\x84"   // ph-flag U+E244
#define FUDE_ICON_SLIDERS       "\xEE\x90\xB2"   // ph-sliders U+E432
#define FUDE_ICON_PEN           "\xEE\x8E\xAC"   // ph-pen-nib U+E3AC
#define FUDE_ICON_CARDS         "\xEE\x83\xB8"   // ph-cards U+E0F8
#define FUDE_ICON_GRIP_H        "\xEE\x9E\x94"   // ph-dots-six U+E794
#define FUDE_ICON_GRIP_V        "\xEE\xAB\xA2"   // ph-dots-six-vertical U+EAE2
#define FUDE_ICON_MORE          "\xEE\x87\xBE"   // ph-dots-three U+E1FE
#define FUDE_ICON_PASTE         "\xEE\x86\x96"   // ph-clipboard U+E196
#define FUDE_ICON_KEYBOARD      "\xEE\x8B\x98"   // ph-keyboard U+E2D8
#define FUDE_ICON_PAGE          "\xEE\x88\xB0"   // ph-file U+E230
#define FUDE_ICON_TABLET        "\xEE\x87\xA6"   // ph-device-tablet U+E1E6
#define FUDE_ICON_GUIDED        "\xEE\x8E\x9C"   // ph-path U+E39C
#define FUDE_ICON_WEAKEST       "\xEE\x92\xAC"   // ph-trend-down U+E4AC
#define FUDE_ICON_STROKE_ORDER  "\xEE\x8B\xB6"   // ph-list-numbers U+E2F6
#define FUDE_ICON_SCRIBBLE      "\xEE\xA0\x86"   // ph-scribble U+E806
#define FUDE_ICON_ARROW_RIGHT   "\xEE\x81\xAC"   // ph-arrow-right U+E06C
#define FUDE_ICON_RETRY         "\xEE\x82\x94"   // ph-arrows-clockwise U+E094
#define FUDE_ICON_FINISH        "\xEE\xA8\xB8"   // ph-flag-checkered U+EA38
#define FUDE_ICON_CUT           "\xEE\xAB\xA0"   // ph-scissors U+EAE0
#define FUDE_ICON_COPY          "\xEE\x87\x8A"   // ph-copy U+E1CA
#define FUDE_ICON_DUPLICATE     "\xEE\x87\x8C"   // ph-copy-simple U+E1CC
#define FUDE_ICON_FOLDER_OPEN   "\xEE\x89\x96"   // ph-folder-open U+E256
#define FUDE_ICON_TEXT_COPY     "\xEE\x9B\xAE"   // ph-text-aa U+E6EE
#define FUDE_ICON_TEXT_PASTE    "\xEE\x86\x98"   // ph-clipboard-text U+E198
#define FUDE_ICON_CAMERA        "\xEE\x84\x8E"   // ph-camera U+E10E
#define FUDE_ICON_EXPORT        "\xEE\xAB\xB0"   // ph-export U+EAF0 (Your data: Export)
#define FUDE_ICON_IMPORT        "\xEE\x80\x90"   // ph-tray-arrow-down U+E010 (Your data: Import)
#define FUDE_ICON_SPEAK         "\xEE\x91\x8A"   // ph-speaker-high U+E44A (read aloud: speech.h)
#define FUDE_ICON_PRINT         "\xEE\x8F\x9C"   // ph-printer U+E3DC (a practice sheet: sheet.h)
#define FUDE_ICON_BOOKMARK      "\xEE\x83\xAA"   // ph-bookmark-simple U+E0EA (a word: save it / saved, Fill)
#define FUDE_ICON_VOCAB         "\xEE\x83\xA4"   // ph-book-bookmark U+E0E4 (the vocabulary)
#define FUDE_ICON_LISTS         "\xEE\x8B\xB2"   // ph-list-bullets U+E2F2 (the vocabulary's lists)
#define FUDE_ICON_LISTEN        "\xEE\x8A\xA6"   // ph-headphones U+E2A6 (an exam by ear)
#define FUDE_ICON_NOTE_EDIT     "\xEE\x8D\x8C"   // ph-note-pencil U+E34C (a character's own note)
#define FUDE_ICON_FINGER        "\xEE\x8A\x9A"   // ph-hand-pointing U+E29A (one finger writes: the toolbar's hand)
#define FUDE_ICON_IMAGE         "\xEE\x8B\x8A"   // ph-image U+E2CA
#define FUDE_ICON_SCAN          "\xEE\xAE\xB6"   // ph-scan U+EBB6
#define FUDE_ICON_PAUSE         "\xEE\x8E\x9E"   // ph-pause U+E39E
#define FUDE_ICON_SHAPES          "\xEE\xB1\x9E"   // ph-shapes U+EC5E (the shapes tool (Sketching))
#define FUDE_ICON_LINE            "\xEE\x9B\x92"   // ph-line-segment U+E6D2 (a line)
#define FUDE_ICON_RECTANGLE       "\xEE\x8F\xB0"   // ph-rectangle U+E3F0 (a rectangle)
#define FUDE_ICON_CIRCLE          "\xEE\x86\x8A"   // ph-circle U+E18A (an ellipse)
#define FUDE_ICON_TRIANGLE        "\xEE\x92\xB0"   // ph-triangle U+E4B0 (a triangle)
#define FUDE_ICON_FILL            "\xEE\x8E\x92"   // ph-paint-bucket U+E392 (filled shapes)
#define FUDE_ICON_SMOOTHING       "\xEE\xAA\x9A"   // ph-wave-sine U+EA9A (the smoothing tool (Sketching))
#define FUDE_ICON_SIGNAL_LOW      "\xEE\x85\x86"   // ph-cell-signal-low U+E146 (a little)
#define FUDE_ICON_SIGNAL_MEDIUM   "\xEE\x85\x88"   // ph-cell-signal-medium U+E148 (more)
#define FUDE_ICON_SIGNAL_HIGH     "\xEE\x85\x84"   // ph-cell-signal-high U+E144 (a lot)
#define FUDE_ICON_RULER           "\xEE\x9A\xB8"   // ph-ruler U+E6B8 (the ruler (Sketching's instruments))
#define FUDE_ICON_SQUARE_30       "\xEE\x92\xB2"   // ph-triangle-dashed U+E4B2 (the 30/60 set square)
#define FUDE_ICON_PROTRACTOR      "\xEE\x86\x8C"   // ph-circle-half U+E18C (the protractor)
#define FUDE_ICON_COMPASS         "\xEE\xA8\x8E"   // ph-compass-tool U+EA0E (the compass)
#define FUDE_ICON_ANGLE           "\xEE\x9E\xBC"   // ph-angle U+E7BC (the instruments tool)
#define FUDE_ICON_MEASURE         "\xEE\xAC\x86"   // ph-arrows-horizontal U+EB06 (measuring, dimensions)
#define FUDE_ICON_TEXT            "\xEE\x92\x8A"   // ph-text-t U+E48A (a text box)
#define FUDE_ICON_STICKY          "\xEE\x96\xAC"   // ph-sticker U+E5AC (a sticky note)
#define FUDE_ICON_CONNECTOR       "\xEE\x9B\xAC"   // ph-flow-arrow U+E6EC (a connector)
#define FUDE_ICON_PEN_NIB         "\xEE\x8E\xAC"   // ph-pen-nib U+E3AC (the curve pen)
#define FUDE_ICON_LAYERS          "\xEE\x91\xA8"   // ph-stack-simple U+E468 (layers)
#define FUDE_ICON_PDF             "\xEE\x9C\x82"   // ph-file-pdf U+E702 (a PDF)
#define FUDE_ICON_CALCULATOR      "\xEE\x94\xB8"   // ph-calculator U+E538 (typing a number)
#define FUDE_ICON_MAGNET          "\xEE\x9A\x82"   // ph-magnet-straight U+E682 (snapping)
#define FUDE_ICON_VIDEO           "\xEE\x9E\x92"   // ph-film-strip U+E792 (a zoom video)
#define FUDE_ICON_GRAPH           "\xEE\xAD\x98"   // ph-graph U+EB58 (a diagram from text)
#define FUDE_ICON_LOCK            "\xEE\x8C\x88"   // ph-lock-simple U+E308 (locked)
#define FUDE_ICON_TREE            "\xEE\x99\xBC"   // ph-tree-structure U+E67C (tidy, lay out)
#define FUDE_ICON_ALIGN           "\xEE\x94\x8E"   // ph-align-left U+E50E (align)
#define FUDE_ICON_ALIGN_CENTER    "\xEE\x94\x8A"   // ph-align-center-horizontal U+E50A (align their middles across)
#define FUDE_ICON_ALIGN_RIGHT     "\xEE\x94\x90"   // ph-align-right U+E510 (align right)
#define FUDE_ICON_ALIGN_TOP       "\xEE\x94\x92"   // ph-align-top U+E512 (align top)
#define FUDE_ICON_ALIGN_MIDDLE    "\xEE\x94\x8C"   // ph-align-center-vertical U+E50C (align their middles up and down)
#define FUDE_ICON_ALIGN_BOTTOM    "\xEE\x94\x86"   // ph-align-bottom U+E506 (align bottom)
#define FUDE_ICON_COLUMNS         "\xEE\x95\x86"   // ph-columns U+E546 (spread across)
#define FUDE_ICON_ARRANGE         "\xEE\x91\xA6"   // ph-stack U+E466 (the Arrange tool)
#define FUDE_ICON_MAP             "\xEE\x8C\x9A"   // ph-map-trifold U+E31A (the canvas's map)
#define FUDE_ICON_AREA            "\xEE\x9B\x8E"   // ph-bounding-box U+E6CE (a named area)
#define FUDE_ICON_PRESENT         "\xEE\x99\x94"   // ph-presentation U+E654 (present: the areas in turn)

#endif
