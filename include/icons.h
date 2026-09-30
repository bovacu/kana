#ifndef KANA_ICONS
#define KANA_ICONS

// ===========================================================================
// The icons: Phosphor (MIT, assets/fonts/LICENSE-Phosphor.txt), as text. Its
// Regular weight is the UI font's last fallback (after Noto Sans JP), so an icon
// is a string: a label can hold one, or one beside words ("<icon> Practice"),
// in the label's colour and at any size (Slug). The Fill weight has the same
// code points; it is a font of its own (kana_toolbar.font_icons_fill), for an
// icon that shows something ON: a chosen tick, a studied star.
//
// Code points from Phosphor 2.1's stylesheet (Fonts/regular/style.css), one
// per icon the app uses; add more from there (data/raw/phosphor-icons/, not
// shipped — only the two fonts in assets/fonts/ are).
// ===========================================================================

#define KANA_ICON_UNDO          "\xEE\x80\xB8"   // ph-arrow-counter-clockwise U+E038
#define KANA_ICON_REDO          "\xEE\x80\xB6"   // ph-arrow-clockwise U+E036
#define KANA_ICON_DRAW          "\xEE\x8E\xB4"   // ph-pencil-simple U+E3B4
#define KANA_ICON_ERASE         "\xEE\x88\x9E"   // ph-eraser U+E21E
#define KANA_ICON_LASSO         "\xEE\xB7\x86"   // ph-lasso U+EDC6
#define KANA_ICON_TRASH         "\xEE\x92\xA6"   // ph-trash U+E4A6
#define KANA_ICON_PALETTE       "\xEE\x9B\x88"   // ph-palette U+E6C8
#define KANA_ICON_PAPER_DOTS    "\xEE\x87\xBC"   // ph-dots-nine U+E1FC
#define KANA_ICON_PAPER_LINES   "\xEE\x96\xA2"   // ph-rows U+E5A2
#define KANA_ICON_PAPER_SQUARES "\xEE\x8A\x96"   // ph-grid-four U+E296
#define KANA_ICON_PAPER_NONE    "\xEE\x91\x9E"   // ph-square U+E45E
#define KANA_ICON_RESET_VIEW    "\xEE\x87\x96"   // ph-crosshair U+E1D6
#define KANA_ICON_ROTATE        "\xEE\xB7\xB2"   // ph-device-rotate U+EDF2
#define KANA_ICON_BACK          "\xEE\x81\x98"   // ph-arrow-left U+E058
#define KANA_ICON_PREV          "\xEE\x84\xB8"   // ph-caret-left U+E138
#define KANA_ICON_NEXT          "\xEE\x84\xBA"   // ph-caret-right U+E13A
#define KANA_ICON_CLOSE         "\xEE\x93\xB6"   // ph-x U+E4F6
#define KANA_ICON_MENU          "\xEE\x8B\xB0"   // ph-list U+E2F0
#define KANA_ICON_SETTINGS      "\xEE\x89\xB2"   // ph-gear-six U+E272
#define KANA_ICON_PLAY          "\xEE\x8F\x90"   // ph-play U+E3D0
#define KANA_ICON_REPLAY        "\xEE\x8F\xB6"   // ph-repeat U+E3F6
#define KANA_ICON_CHECK         "\xEE\x86\x82"   // ph-check U+E182
#define KANA_ICON_CHECK_CIRCLE  "\xEE\x86\x84"   // ph-check-circle U+E184
#define KANA_ICON_CIRCLE        "\xEE\x86\x8A"   // ph-circle U+E18A
#define KANA_ICON_CHECKS        "\xEE\x94\xBA"   // ph-checks U+E53A
#define KANA_ICON_SELECT        "\xEE\x9A\x9A"   // ph-selection U+E69A
#define KANA_ICON_SELECT_ALL    "\xEE\x9D\x86"   // ph-selection-all U+E746
#define KANA_ICON_SELECT_NONE   "\xEE\x9A\x9E"   // ph-selection-slash U+E69E
#define KANA_ICON_LIST_CHECKS   "\xEE\xAB\x9C"   // ph-list-checks U+EADC
#define KANA_ICON_STAR          "\xEE\x91\xAA"   // ph-star U+E46A
#define KANA_ICON_KNOWN         "\xEE\x98\x86"   // ph-seal-check U+E606
#define KANA_ICON_GRADUATION    "\xEE\x98\xAC"   // ph-graduation-cap U+E62C
#define KANA_ICON_EXAM          "\xEE\x9D\x82"   // ph-exam U+E742
#define KANA_ICON_CHART_LINE    "\xEE\x85\x96"   // ph-chart-line-up U+E156
#define KANA_ICON_CHART_BAR     "\xEE\x85\x90"   // ph-chart-bar U+E150
#define KANA_ICON_CALENDAR      "\xEE\x84\x8A"   // ph-calendar-blank U+E10A
#define KANA_ICON_STREAK        "\xEE\x89\x82"   // ph-fire U+E242
#define KANA_ICON_TROPHY        "\xEE\x99\xBE"   // ph-trophy U+E67E
#define KANA_ICON_TARGET        "\xEE\x91\xBC"   // ph-target U+E47C
#define KANA_ICON_CLOCK         "\xEE\x86\x9A"   // ph-clock U+E19A
#define KANA_ICON_BOOKS         "\xEE\x9D\x98"   // ph-books U+E758
#define KANA_ICON_BOOK          "\xEE\x83\xA6"   // ph-book-open U+E0E6
#define KANA_ICON_SEARCH        "\xEE\x8C\x8C"   // ph-magnifying-glass U+E30C
#define KANA_ICON_FILTER        "\xEE\x89\xA6"   // ph-funnel U+E266
#define KANA_ICON_SORT          "\xEE\x91\x84"   // ph-sort-ascending U+E444
#define KANA_ICON_FOLDER        "\xEE\x89\x8A"   // ph-folder U+E24A
#define KANA_ICON_FOLDER_ADD    "\xEE\x89\x98"   // ph-folder-plus U+E258
#define KANA_ICON_FILE_ADD      "\xEE\x88\xB6"   // ph-file-plus U+E236
#define KANA_ICON_NOTE          "\xEE\x8D\x8C"   // ph-note-pencil U+E34C
#define KANA_ICON_TRANSLATE     "\xEE\x92\xA2"   // ph-translate U+E4A2
#define KANA_ICON_INFO          "\xEE\x8B\x8E"   // ph-info U+E2CE
#define KANA_ICON_HINT          "\xEE\x8B\x9C"   // ph-lightbulb U+E2DC
#define KANA_ICON_SHOW          "\xEE\x88\xA0"   // ph-eye U+E220
#define KANA_ICON_HIDE          "\xEE\x88\xA4"   // ph-eye-slash U+E224
#define KANA_ICON_PLUS          "\xEE\x8F\x94"   // ph-plus U+E3D4
#define KANA_ICON_MINUS         "\xEE\x8C\xAA"   // ph-minus U+E32A
#define KANA_ICON_FULLSCREEN    "\xEE\x98\xA6"   // ph-frame-corners U+E626
#define KANA_ICON_PRACTICE      "\xEE\x9D\x8E"   // ph-brain U+E74E
#define KANA_ICON_SHUFFLE       "\xEE\x90\xA2"   // ph-shuffle U+E422
#define KANA_ICON_STACK         "\xEE\x91\xA6"   // ph-stack U+E466
#define KANA_ICON_FLAG          "\xEE\x89\x84"   // ph-flag U+E244
#define KANA_ICON_SLIDERS       "\xEE\x90\xB2"   // ph-sliders U+E432
#define KANA_ICON_PEN           "\xEE\x8E\xAC"   // ph-pen-nib U+E3AC
#define KANA_ICON_CARDS         "\xEE\x83\xB8"   // ph-cards U+E0F8

#endif
