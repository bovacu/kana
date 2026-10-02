#ifndef KANA_TOOLBAR
#define KANA_TOOLBAR

#include "rde.h"
#include "wordcard.h"
#include "sheet.h"

#define KANA_SHEET_RECORDS (KANA_SHEET_MAX + 1u)   // one more: too many to fit is told
#include "ink.h"
#include "canvas.h"
#include "lasso.h"
#include "viewer.h"
#include "browse.h"
#include "chart.h"
#include "practice.h"
#include "album.h"
#include "side.h"
#include "notes.h"
#include "check.h"
#include "select.h"
#include "exam.h"
#include "marks.h"
#include "stats.h"
#include "textink.h"
#include "scan.h"
#include "wordsplit.h"
#include "vocabview.h"
#include "wordexam.h"

// ===========================================================================
// The floating toolbar: a movable bar of tools that can sit anywhere on screen,
// vertical or horizontal. An RDE UI canvas of its own, text in Slug.
//
// Children are placed by hand (kana_toolbar_layout) rather than by an hbox/vbox:
// rotating the bar is then just laying the same children out along the other
// axis, and none of the container sizing rules come into it.
//
// INPUT: the RDE UI only ever sees MOUSE events — finger taps and pen touches
// reach it as SDL's synthetic mouse, AFTER the app has already had the pen/touch
// event. So the app must ask kana_toolbar_hit before starting ink, an erase or a
// pan, or writing on a button would also draw under it.
// ===========================================================================

typedef enum {
    KANA_TOOL_DRAW = 0,
    KANA_TOOL_ERASE,
    KANA_TOOL_LASSO
} KANA_TOOL_;

// The UI font: Roboto, rendered with Slug (curves, sharp at any size), with Noto
// Sans JP behind it for Japanese. Loaded at this size; everything that draws
// text with it scales from here. The rest of the app draws its text with
// toolbar.font too — see kana.c.
#define KANA_TOOLBAR_FONT_SIZE   32

#define KANA_TOOLBAR_PALETTE_COUNT 8

// Slider range for the brush half-width (see kana_ink.constant_radius).
#define KANA_TOOLBAR_SIZE_MIN  0.5f
#define KANA_TOOLBAR_SIZE_MAX 12.0f

typedef struct kana_toolbar kana_toolbar;

#define KANA_TOOLBAR_MENU_MAX 8
#define KANA_TOOLBAR_SEPARATORS 5

// Translate with Google on a selection: where it is.
typedef enum {
    KANA_TOOLBAR_TRANSLATION_NONE = 0,   // no card
    KANA_TOOLBAR_TRANSLATION_READING,    // the selection being read (text_reader)
    KANA_TOOLBAR_TRANSLATION_GETTING,    // its models asked for / on their way
    KANA_TOOLBAR_TRANSLATION_ASKED,      // the translator has it
    KANA_TOOLBAR_TRANSLATION_DONE,       // translation: in (empty: it could not be)
    KANA_TOOLBAR_TRANSLATION_FAILED      // the models could not be downloaded
} KANA_TOOLBAR_TRANSLATION_;

// A practice sheet asked for (sheet.h): of what.
typedef enum {
    KANA_TOOLBAR_SHEET_NONE = 0,
    KANA_TOOLBAR_SHEET_VIEWER,       // the viewer's character
    KANA_TOOLBAR_SHEET_SELECTION,    // Select mode's ticks
    KANA_TOOLBAR_SHEET_VOCAB         // the Vocabulary screen's words' characters
} KANA_TOOLBAR_SHEET_;

// A floating row of buttons: the menu over a lasso selection, the page's
// context menu.
RDE_STRUCT {
    rde_ui_image*  panel;
    rde_ui_button* buttons[KANA_TOOLBAR_MENU_MAX];
    u32            count;
    b8             open;
    rde_vec_2F     center;   // UI canvas units
    rde_vec_2F     size;
} kana_toolbar_menu;

// One swatch's callback context: which toolbar, which colour.
RDE_STRUCT {
    kana_toolbar* toolbar;
    u32           index;
} kana_toolbar_swatch_ref;

// One Browse chip's callback context: which toolbar, which filter or sort.
typedef kana_toolbar_swatch_ref kana_toolbar_chip_ref;

struct kana_toolbar {
    rde_ui_canvas* ui;
    rde_font*      font;
    rde_font*      font_jp;         // Noto Sans JP, font's fallback: Japanese typed or shown as text
    rde_font*      font_icons;      // Phosphor Regular, font's last fallback: icons as text (icons.h)
    rde_font*      font_icons_fill; // Phosphor Fill, a font of its own: an icon showing something on
    rde_window*    window;

    // What the toolbar drives.
    kana_ink*      ink;
    kana_canvas*   view;
    kana_lasso*    lasso;
    kana_viewer*   viewer;
    kana_browse*   browse;
    kana_chart*    chart;
    kana_practice* practice;
    kana_album*    album;
    kana_notes*    notes;
    kana_check*    check;
    kana_selection* selection;      // Browse's and the chart's ticks (select.h); set by the owner after init
    kana_exam*     exam;            // exams (exam.h); set by the owner after init
    kana_stats*    stats;           // statistics (stats.h); set by the owner after init
    kana_scan*     scan;            // text from a photo (scan.h); set by the owner after init
    kana_vocabview* vocab;          // the Vocabulary screen (vocabview.h); set by the owner after init
    kana_wordexam* wordexam;        // word exams (wordexam.h); set by the owner after init
    b8*            show_hud;
    u8             _sheet_asked;    // KANA_TOOLBAR_SHEET_: for kana_toolbar_take_sheet
    u32            _sheet_one;      // ...the viewer's character's record

    KANA_TOOL_     tool;
    KANA_TOOL_     tool_before;   // the one before it (a pen's double tap goes back to it)
    b8             covered;       // one of Kana's own screens is over everything (the welcome): the bar and the menu hide
    b8             vertical;
    rde_vec_2F     center;          // panel centre, UI canvas units (bottom-left origin, Y up)
    rde_vec_2F     panel_size;

    rde_ui_image*  panel;
    // The grip's end of the bar: what drags it, and a double tap there folds the
    // bar down to just this (minimized) and opens it again. The handle drawn in it.
    rde_ui_image*  grip_area;
    rde_ui_label*  grip;             // six dots (icons.h), across the bar
    b8             minimized;
    // The tools, in a strip that scrolls along the bar when they do not all fit
    // (and more can be added without the bar outgrowing the screen).
    rde_ui_scroll_area* strip;
    rde_ui_button* undo;
    rde_ui_button* redo;
    rde_ui_button* draw;
    rde_ui_button* erase;
    rde_ui_button* lasso_tool;
    rde_ui_button* clear;
    rde_ui_slider* size;
    rde_ui_button* color;
    rde_ui_image*  color_dot;       // in Color: the colour writing now
    rde_ui_button* brush_scale;
    rde_ui_button* paper;           // opens the paper panel: the page's dots, lines, squares or nothing (canvas.h)
    rde_ui_button* rotate;
    rde_ui_button* reset_view;
    rde_ui_button* camera;          // Text from a photo, the camera live (scan.h); only where it can be read
    rde_ui_image*  separators[KANA_TOOLBAR_SEPARATORS];   // between the groups of tools

    rde_ui_image*            palette;
    rde_ui_button*           swatches[KANA_TOOLBAR_PALETTE_COUNT];
    kana_toolbar_swatch_ref  swatch_refs[KANA_TOOLBAR_PALETTE_COUNT];
    b8                       palette_open;
    rde_vec_2F               palette_center;   // UI canvas units, set whenever it is placed
    rde_vec_2F               palette_size;

    // The page's paper (canvas.h), a panel beside the bar like the palette,
    // opened by Paper; one of the two open at a time. In KANA_PAPER_ order.
    rde_ui_image*            paper_panel;
    rde_ui_button*           paper_choices[KANA_PAPER_COUNT];
    kana_toolbar_swatch_ref  paper_refs[KANA_PAPER_COUNT];
    b8                       paper_open;
    rde_vec_2F               paper_center;   // UI canvas units, set whenever it is placed
    rde_vec_2F               paper_size;
    // Over a lasso selection: Cut, Copy, Copy as text, Duplicate, Check, Delete.
    kana_toolbar_menu        selection_menu;
    f64                      copied_until;     // Copy (or Copy as text) reads "Copied" until then (engine clock)
    kana_textink_reader      text_reader;      // Copy as text: the selection being read (textink.h)
    b8                       _text_reader_ready;
    b8                       text_for_translate;   // the reading is for Translate with Google, not the clipboard
    b8                       text_for_vocab;       // ...for the word card (Save word), not the clipboard
    // Translate with Google on a selection: what was read, into the translator
    // (translate.h), shown in a card by the selection until it goes.
    KANA_TOOLBAR_TRANSLATION_ translation_state;
    u32                      translation_ticket;
    u32                      translation_of;      // the selection it is for (kana_toolbar_selection_key)
    b8                       translation_prepared;
    c8                       translation_from[256];
    c8                       translation[KANA_TRANSLATE_TEXT];
    rde_vec_2F               translation_min;     // the card on screen, Kana's screen space (last frame)
    rde_vec_2F               translation_max;
    rde_vec_2F               translation_speak_min;   // its speaker (none: min = max)
    rde_vec_2F               translation_speak_max;
    // What was read, as words (wordsplit.h): each a row on the card, + to add it
    // to the learner's words.
    u32                      translation_words[KANA_WORDSPLIT_MAX];
    u32                      translation_word_count;
    f32                      translation_words_top;   // the rows on screen (last frame)
    f32                      translation_words_left;
    f32                      translation_words_right;
    // The page's context menu, opened by a long press: Paste, Paste text, Select all.
    kana_toolbar_menu        context_menu;
    rde_vec_2F               context_canvas;   // where it was opened, on the page — where Paste lands
    kana_clip                _text_clip;       // Paste text: the system clipboard's text, written as strokes
    // A line for the page a moment (kana.c draws it): what Copy as text copied,
    // what Paste text left out.
    c8                       notice[192];
    f64                      notice_at;        // when it was set (engine clock); 0: none
    // Under the character viewer: Prev, Replay, Next, Close. While the viewer is
    // open the bar itself and every other menu hide.
    kana_toolbar_menu        viewer_menu;
    b8                       _viewer_shown;     // a full-screen scene is up: the floating bar is hidden
    // The kana chart's row: Hiragana, Katakana (jumps), Close.
    kana_toolbar_menu        chart_menu;
    // Browse's row, at the bottom like the chart's and the album's: Select,
    // Practice (its list as a set), Close.
    kana_toolbar_menu        browse_menu;
    // Browse's and the chart's row in Select mode: All (Browse's list, or the
    // chart's section in view), None, Practice (the ticked), Done.
    kana_toolbar_menu        select_menu;
    u32                      _selected_shown;   // the count "Practice n" shows
    // The viewer's Study: the character's mark (marks.h) as shown.
    u32                      _mark_shown_for;   // the code point it shows (0: none yet)
    KANA_MARK_               _mark_shown;
    u32                      _marks_seen;       // kana_marks_revision when shown
    // The exam's rows (exam.h), one per stage: Close, Next / Back, All, None,
    // Start n / Quit, Undo, Clear, Next (Finish) / Done, Retry wrong, Practice wrong.
    kana_toolbar_menu        exam_setup_menu;
    kana_toolbar_menu        exam_preview_menu;
    kana_toolbar_menu        exam_menu;
    kana_toolbar_menu        exam_results_menu;
    u32                      _exam_shown;       // what the rows' labels were set for (a hash of stage and counts)
    // Statistics' row: Close.
    kana_toolbar_menu        stats_menu;
    // The Vocabulary screen's row: Back, + Word, Review n, Exam, Sheet, Practice;
    // and a word exam's: its setup's (Close, Start n), writing's (Quit, Undo,
    // Clear, Next / Finish), results' (Done, Retry the wrong ones).
    kana_toolbar_menu        vocab_menu;
    kana_toolbar_menu        wordexam_setup_menu;
    kana_toolbar_menu        wordexam_menu;
    kana_toolbar_menu        wordexam_results_menu;
    u32                      _vocab_shown;      // what the vocabulary row's labels were set for (UINT32_MAX: not yet)
    u32                      _wordexam_shown;   // ...a word exam's rows'
    u32                      _sheet_records[KANA_SHEET_RECORDS];   // a sheet of the vocabulary's characters
    // Text from a photo's row: Back, Camera, Photos, Write n.
    kana_toolbar_menu        scan_menu;
    u32                      _scan_kept_shown;   // the count "Write n" shows (UINT32_MAX: not yet)
    u8                       _scan_translate_shown;   // Translate with Google's look: 0 off, 1 on (UINT8_MAX: not yet)
    // The viewer's Add (viewer.h): its row — Done, Type your own.
    kana_toolbar_menu        viewer_add_menu;
    // The word card (wordcard.h): a word into the vocabulary, or a saved one.
    kana_wordcard            word;
    // Practice's row: Back, Undo, Clear, Score, fewer / more squares.
    kana_toolbar_menu        practice_menu;
    // Check's row: Back, Stroke order, Practice; and its "I meant…" field, at
    // the top right of its screen.
    kana_toolbar_menu        check_menu;
    rde_ui_text_editor*      check_field;
    b8                       _check_field_shown;
    rde_vec_4F               _check_field_for;   // the screen size and insets it was placed for
    // The album's rows: the overview's (the sorts, Close) and a character page's
    // (Back, Practice).
    kana_toolbar_menu        album_menu;
    kana_toolbar_menu        album_page_menu;
    kana_toolbar_chip_ref    album_sort_refs[KANA_ALBUM_SORT_COUNT];
    u32                      _album_practice_shown;   // the count "Practice n" shows
    u32                      _album_view_shown;       // the album view its menu shows selected (KANA_ALBUM_VIEW_)
    // Practice over a set: Back, Undo, Clear, Score, Next (Finish on the last);
    // and the set's summary: Weakest again, Done.
    kana_toolbar_menu        practice_set_menu;
    kana_toolbar_menu        practice_summary_menu;
    b8                       _finish_shown;
    b8                       _guided_shown;      // what the Guided buttons show
    KANA_PAPER_              _paper_shown;       // the paper the panel shows chosen (the page's, which changes with the canvas)
    b8                       _can_score_shown;   // Score pressable (not in guided steps 1 and 2)

    // Browse's bar, across the top of the screen: filters, sorts, the search
    // field, Draw/Clear, Close.
    rde_ui_image*            browse_bar;
    rde_ui_button*           filter_chips[KANA_FILTER_COUNT];
    kana_toolbar_chip_ref    filter_refs[KANA_FILTER_COUNT];
    rde_ui_button*           sort_chips[KANA_SORT_COUNT];
    kana_toolbar_chip_ref    sort_refs[KANA_SORT_COUNT];
    rde_ui_text_editor*      search_field;
    rde_ui_button*           draw_toggle;
    rde_ui_button*           parts_toggle;        // the parts panel (search by component)
    rde_ui_button*           pad_clear;
    b8                       _browse_shown;
    rde_vec_2F               _browse_laid_out;   // the screen size the bar was laid out for
    rde_vec_4I               _insets_seen;       // the safe area the layouts are for (see kana_toolbar_update_viewer)
    f32                      browse_bar_height;  // UI units, including the top safe inset

    // Navigation, the themes, Settings: the side panel (side.h).
    kana_side                side;

    // Grip drag, and the grip's last tap (for a double tap).
    rde_vec_2F     drag_start_center;
    rde_vec_2F     drag_press;
    f64            grip_tapped;        // engine clock (0: no tap waiting for its second)
    rde_vec_2F     grip_tapped_at;

    // What Undo/Redo currently show, so kana_toolbar_update only touches them on
    // a change.
    b8             _history_shown;
    b8             _can_undo_shown;
    b8             _can_redo_shown;
    u32            _text_revision;     // the language the widgets were built in (kana_text_revision)
};

void       kana_toolbar_init(kana_toolbar* _toolbar, rde_window* _window, kana_ink* _ink, kana_canvas* _view, kana_lasso* _lasso,
                             kana_viewer* _viewer, kana_browse* _browse, kana_chart* _chart, kana_practice* _practice, kana_album* _album, kana_notes* _notes, kana_check* _check, b8* _show_hud);
void       kana_toolbar_destroy(kana_toolbar* _toolbar);
// A line on the page, a moment (kana.c draws it): what an action did.
void       kana_toolbar_notice(kana_toolbar* _toolbar, const c8* _text);
// A practice sheet asked for (the viewer's Sheet, or Select mode's): its records
// (valid until the selection changes), how many; 0 when none was. Once per ask.
u32        kana_toolbar_take_sheet(kana_toolbar* _toolbar, const u32** _records);

// Apple Pencil's double tap (RDE_EVENT_TYPE_PEN_DOUBLE_TAP), as the learner set
// it in the system's settings (_action, RDE_PEN_TAP_ACTION_): the eraser and back,
// the tool before, or the colour palette; nothing when they turned it off.
void       kana_toolbar_pen_double_tap(kana_toolbar* _toolbar, u8 _action);

// Translate with Google on the selection (its menu's button, and a look flag):
// read, translated, and shown in a card by it (kana_toolbar_render_translation).
void       kana_toolbar_translate_selection(kana_toolbar* _toolbar);
// Save word on the selection (its menu's button, and a look flag): read, then
// the word card (wordcard.h) with what it says.
void       kana_toolbar_save_selection(kana_toolbar* _toolbar);
// The card, once a frame over the page (Kana's screen space).
void       kana_toolbar_render_translation(kana_toolbar* _toolbar, rde_window* _window);
// A press at _screen (Kana's screen space): on the card's speaker, what was read
// is said aloud (speech.h). True when it was the speaker.
b8         kana_toolbar_card_press(kana_toolbar* _toolbar, rde_vec_2F _screen);

// The app went to the background or the OS is short of memory: the fonts give
// back the GPU memory their glyphs grew into (rde_font_trim). Before drawing.
void       kana_toolbar_trim_fonts(kana_toolbar* _toolbar);

// Is this point on the toolbar, its open palette or paper panel, or an open menu? _screen is
// Kana's screen space (centre-origin, Y up) — what pen positions convert to.
b8         kana_toolbar_hit(const kana_toolbar* _toolbar, rde_vec_2F _screen);

// Re-reads ink state into the widgets (after a keyboard shortcut changed it), and
// restyles every widget from the current theme (after kana_theme_set).
void       kana_toolbar_sync(kana_toolbar* _toolbar);

// The page's context menu, for a long press at _screen (Kana screen space).
// _canvas is the same point on the page: where Paste will land. It floats just
// above the finger so the finger doesn't cover it. Closed by choosing an item or
// by kana_toolbar_close_context_menu (the caller closes it on any other press).
void       kana_toolbar_open_context_menu(kana_toolbar* _toolbar, rde_vec_2F _screen, rde_vec_2F _canvas);
void       kana_toolbar_close_context_menu(kana_toolbar* _toolbar);

// Puts the bar back where a save left it (orientation, centre in UI canvas units,
// folded to its grip or not). Clamped on screen, so a centre from a bigger or
// rotated screen is fine.
void       kana_toolbar_set_placement(kana_toolbar* _toolbar, b8 _vertical, rde_vec_2F _center, b8 _minimized);

// Once a frame: greys Undo/Redo out when there is nothing to undo/redo, and
// shows the selection menu over a lasso selection. Cheap — it only touches the
// widgets when something changed.
void       kana_toolbar_update(kana_toolbar* _toolbar);

// The paper panel, open (as Paper does).
void       kana_toolbar_open_paper(kana_toolbar* _toolbar);
// _text (UTF-8) written in the characters' own strokes with the brush
// (textink.h), centred at _canvas, selected (Paste text's work; the clipboard is
// its caller's to read). What could not be written is said in the notice.
void       kana_toolbar_paste_text(kana_toolbar* _toolbar, const c8* _text, rde_vec_2F _canvas);

// Once a frame, outside the UI's own events (a language chosen in Settings
// rebuilds the whole UI, the button that chose it included): every widget built
// again when the language changed (text.h).
void       kana_toolbar_follow_language(kana_toolbar* _toolbar);

#endif
