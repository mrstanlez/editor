#ifndef EDITOR_H
#define EDITOR_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <vte/vte.h>

typedef struct {
    GtkWidget *scroll_win;
    GtkWidget *source_view;
    GtkSourceBuffer *buffer;
    char *file_path;
    GtkCssProvider *css_provider;
    int font_size;
    char *font_family;
    GtkWidget *tab_label_box;
} TabData;

typedef struct {
    const char *name;
    const char *id;
} ThemeInfo;

typedef struct {
    int line_num;
    char file_name[256];
    char description[512];
    char solution[512];
} analyze_problem;

static const char *terminal_words[] = {"function", "Function",  "error", "Error", "warning", "Warning", NULL};

typedef struct {
    int width;
    int height;
    int pos_x;
    int pos_y;
    char font[128];
    int font_size;
    char theme[64];
    char last_file[256];
    char language[64];
    char cmd_debug[512];
    char cmd_build[512];
    char cmd_run[512];
} AppConfig;

extern AppConfig cfg;
extern GtkWidget *window;
extern GtkWidget *notebook;
extern GtkWidget *notebook_bottom;
extern GtkWidget *statusbar;
extern const char *current_theme_id;
extern GtkWidget *vte_term;
extern GtkWidget *vte_debug;

void init(void);
void save_config(const char *filename);
void on_setup_commands_clicked(void);
void on_build_clicked(void);
void on_run_clicked(void);
void on_language_changed(GtkMenuItem *menu_item, gpointer user_data);
void free_tab_data(TabData *tab);
TabData* get_current_tab_data(void);
void update_tab_label(TabData *tab);
void apply_font_to_tab(TabData *tab);
void apply_theme_to_buffer(GtkSourceBuffer *buffer, const char *theme_id);
void create_new_tab(const char *filename);

void on_new_clicked(void);
void on_open_clicked(void);
void on_save_clicked(void);
void on_save_as_clicked(void);
void on_font_clicked(void);
void on_theme_changed(GtkMenuItem *menu_item, gpointer user_data);

void on_undo_clicked(void);
void on_redo_clicked(void);
void on_cut_clicked(void);
void on_copy_clicked(void);
void on_paste_clicked(void);
void on_search_clicked(void);
void on_replace_clicked(void);
void on_goto_clicked(void);

void update_statusbar(GtkTextBuffer *buffer, gpointer user_data);
GtkWidget* create_menu_item_with_icon(const char *label_text, const char *icon_name);
GtkWidget* create_drawing_tab(void);

void on_compile_clicked(void);
void on_debug_clicked(void);
void on_setup_commands_clicked(void);
void on_compile_finished(GtkWidget *vte, int status, gpointer user_data);
static gboolean is_comment_or_empty(const char *text);

void analyze(const char *error_line, analyze_problem *prob);
void analyze_row(const char *clean_line, const char *word, analyze_problem *prob);

static gboolean is_comment_or_empty(const char *text);
void parse_buffer_errors(GtkWidget *vte);

#endif
