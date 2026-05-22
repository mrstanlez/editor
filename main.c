/*
Project: Editor 1.0
Author: Stanislav Petrek
Date: 21.may.2026

Compile: gcc main.c actions.c -o editor `pkg-config --cflags --libs gtk+-3.0 gtksourceview-3.0 vte-2.91` -w
Run: ./editor
*/

#include "editor.h"
#include <vte/vte.h>

GtkWidget *window;
GtkWidget *notebook;
GtkWidget *statusbar;
GtkWidget *vte_term;
GtkWidget *vte_debug;
GtkWidget *notebook_bottom;
const char *current_theme_id = "none";

AppConfig cfg = {
    .width = 1024,
    .height = 768,
    .pos_x = 100,
    .pos_y = 100,
    .font = "Monospace",
    .font_size = 10,
    .theme = "none",
    .last_file = "",
    .language = "C",
    .cmd_debug = "gdb ./program",
    .cmd_build = "gcc main.c actions.c -o program `pkg-config --cflags --libs gtk+-3.0 gtksourceview-3.0 vte-2.91` -w",
    .cmd_run = "./program"
};


static void on_switch_page(GtkNotebook *nb, GtkWidget *page, guint page_num, gpointer user_data) {
    TabData *tab = (TabData*)g_object_get_data(G_OBJECT(page), "tab_data");
    if (tab && tab->buffer) {
        update_statusbar(GTK_TEXT_BUFFER(tab->buffer), NULL);
    }
}

void init(void) {
    FILE *f = fopen("init.txt", "r");
    if (!f) {
        current_theme_id = g_strdup(cfg.theme);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char key[128], val[256];
        if (sscanf(line, "%127[^=]=%255[^\n]", key, val) == 2) {
            char *g_key = g_strstrip(key);
            char *g_val = g_strstrip(val);
            if (strcmp(g_key, "width") == 0) cfg.width = atoi(g_val);
            else if (strcmp(g_key, "height") == 0) cfg.height = atoi(g_val);
            else if (strcmp(g_key, "pos_x") == 0) cfg.pos_x = atoi(g_val);
            else if (strcmp(g_key, "pos_y") == 0) cfg.pos_y = atoi(g_val);
            else if (strcmp(g_key, "font_name") == 0) strcpy(cfg.font, g_val);
            else if (strcmp(g_key, "font_size") == 0) cfg.font_size = atoi(g_val);
            else if (strcmp(g_key, "theme") == 0) strcpy(cfg.theme, g_val);
            else if (strcmp(g_key, "last_file") == 0) strcpy(cfg.last_file, g_val);
			else if (strcmp(g_key, "language") == 0) strcpy(cfg.language, g_val);
			else if (strcmp(g_key, "cmd_debug") == 0) strcpy(cfg.cmd_debug, g_val);
			else if (strcmp(g_key, "cmd_build") == 0) strcpy(cfg.cmd_build, g_val);
			else if (strcmp(g_key, "cmd_run") == 0) strcpy(cfg.cmd_run, g_val);
        }
    }
    fclose(f);
    current_theme_id = g_strdup(cfg.theme);
}

void save_config(const char *filename) {
    strcpy(cfg.last_file, "");
    TabData *tab = get_current_tab_data();
    if (tab) {
        if (tab->font_family) strcpy(cfg.font, tab->font_family);
        cfg.font_size = tab->font_size;
        if (tab->file_path) strcpy(cfg.last_file, tab->file_path);
    }
    FILE *f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "width=%d\nheight=%d\npos_x=%d\npos_y=%d\n", cfg.width, cfg.height, cfg.pos_x, cfg.pos_y);
    fprintf(f, "font_name=%s\nfont_size=%d\ntheme=%s\nlast_file=%s\n", cfg.font, cfg.font_size, current_theme_id, cfg.last_file);
    fprintf(f, "language=%s\ncmd_debug=%s\ncmd_build=%s\ncmd_run=%s\n", cfg.language, cfg.cmd_debug, cfg.cmd_build, cfg.cmd_run);
    fclose(f);
}

static gboolean on_window_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    cfg.pos_x = event->x;
    cfg.pos_y = event->y;
    cfg.width = event->width;
    cfg.height = event->height;
    return FALSE;
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    save_config("init.txt");
    gtk_main_quit();
}

static void build_menu_bar(GtkWidget *vbox, GtkAccelGroup *accel_group) {
    GtkWidget *menu_bar = gtk_menu_bar_new();
    
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_mi = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_mi), file_menu);

    GtkWidget *new_mi = create_menu_item_with_icon("_New", "document-new");
    g_signal_connect(new_mi, "activate", G_CALLBACK(on_new_clicked), NULL);
    gtk_widget_add_accelerator(new_mi, "activate", accel_group, GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_mi);

    GtkWidget *open_mi = create_menu_item_with_icon("_Open...", "document-open");
    g_signal_connect(open_mi, "activate", G_CALLBACK(on_open_clicked), NULL);
    gtk_widget_add_accelerator(open_mi, "activate", accel_group, GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_mi);

    GtkWidget *save_mi = create_menu_item_with_icon("_Save", "document-save");
    g_signal_connect(save_mi, "activate", G_CALLBACK(on_save_clicked), NULL);
    gtk_widget_add_accelerator(save_mi, "activate", accel_group, GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_mi);

    GtkWidget *save_as_mi = create_menu_item_with_icon("Save _As...", "document-save-as");
    g_signal_connect(save_as_mi, "activate", G_CALLBACK(on_save_as_clicked), NULL);
    gtk_widget_add_accelerator(save_as_mi, "activate", accel_group, GDK_KEY_S, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_as_mi);

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());

    GtkWidget *quit_mi = create_menu_item_with_icon("_Quit", "application-exit");
    g_signal_connect(quit_mi, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_add_accelerator(quit_mi, "activate", accel_group, GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_mi);

    GtkWidget *edit_menu = gtk_menu_new();
    GtkWidget *edit_mi = gtk_menu_item_new_with_mnemonic("_Edit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_mi), edit_menu);

    GtkWidget *undo_mi = create_menu_item_with_icon("_Undo", "edit-undo");
    g_signal_connect(undo_mi, "activate", G_CALLBACK(on_undo_clicked), NULL);
    gtk_widget_add_accelerator(undo_mi, "activate", accel_group, GDK_KEY_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), undo_mi);

    GtkWidget *redo_mi = create_menu_item_with_icon("_Redo", "edit-redo");
    g_signal_connect(redo_mi, "activate", G_CALLBACK(on_redo_clicked), NULL);
    gtk_widget_add_accelerator(redo_mi, "activate", accel_group, GDK_KEY_y, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), redo_mi);

    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), gtk_separator_menu_item_new());

    GtkWidget *cut_mi = create_menu_item_with_icon("Cu_t", "edit-cut");
    g_signal_connect(cut_mi, "activate", G_CALLBACK(on_cut_clicked), NULL);
    gtk_widget_add_accelerator(cut_mi, "activate", accel_group, GDK_KEY_x, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), cut_mi);

    GtkWidget *copy_mi = create_menu_item_with_icon("_Copy", "edit-copy");
    g_signal_connect(copy_mi, "activate", G_CALLBACK(on_copy_clicked), NULL);
    gtk_widget_add_accelerator(copy_mi, "activate", accel_group, GDK_KEY_c, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), copy_mi);

    GtkWidget *paste_mi = create_menu_item_with_icon("_Paste", "edit-paste");
    g_signal_connect(paste_mi, "activate", G_CALLBACK(on_paste_clicked), NULL);
    gtk_widget_add_accelerator(paste_mi, "activate", accel_group, GDK_KEY_v, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), paste_mi);

    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), gtk_separator_menu_item_new());

    GtkWidget *search_mi = create_menu_item_with_icon("_Find...", "edit-find");
    g_signal_connect(search_mi, "activate", G_CALLBACK(on_search_clicked), NULL);
    gtk_widget_add_accelerator(search_mi, "activate", accel_group, GDK_KEY_f, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), search_mi);

    GtkWidget *replace_mi = create_menu_item_with_icon("R_eplace...", "edit-find-replace");
    g_signal_connect(replace_mi, "activate", G_CALLBACK(on_replace_clicked), NULL);
    gtk_widget_add_accelerator(replace_mi, "activate", accel_group, GDK_KEY_h, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), replace_mi);

    GtkWidget *goto_mi = create_menu_item_with_icon("Go _To...", "go-jump");
    g_signal_connect(goto_mi, "activate", G_CALLBACK(on_goto_clicked), NULL);
    gtk_widget_add_accelerator(goto_mi, "activate", accel_group, GDK_KEY_g, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), goto_mi);

    GtkWidget *options_menu = gtk_menu_new();
    GtkWidget *options_mi = gtk_menu_item_new_with_mnemonic("_Options");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(options_mi), options_menu);

    GtkWidget *font_mi = create_menu_item_with_icon("Select _Font...", "preferences-desktop-font");
    g_signal_connect(font_mi, "activate", G_CALLBACK(on_font_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), font_mi);

    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());

    GtkWidget *themes_submenu = gtk_menu_new();
    GtkWidget *themes_mi = create_menu_item_with_icon("_Themes", "preferences-desktop-theme");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(themes_mi), themes_submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), themes_mi);

    ThemeInfo themes[] = {
        {"None", "none"},
        {"Classic (Light)", "classic"},
        {"Cobalt (Blue/Dark)", "cobalt"},
        {"Kate (Light)", "kate"},
        {"Oblivion (Dark)", "oblivion"},
        {"Solarized Dark", "solarized-dark"},
        {"Solarized Light", "solarized-light"},
        {"Tango (Light)", "tango"}
    };

    GSList *theme_group = NULL;
    for (int i = 0; i < 8; i++) {
        GtkWidget *t_item = gtk_radio_menu_item_new_with_label(theme_group, themes[i].name);
        theme_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(t_item));
        if (strcmp(themes[i].id, current_theme_id) == 0) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(t_item), TRUE);
        }
        g_signal_connect(t_item, "activate", G_CALLBACK(on_theme_changed), (gpointer)themes[i].id);
        gtk_menu_shell_append(GTK_MENU_SHELL(themes_submenu), t_item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());

    GtkWidget *lang_submenu = gtk_menu_new();
    GtkWidget *lang_mi = create_menu_item_with_icon("_Language", "preferences-desktop-locale");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(lang_mi), lang_submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), lang_mi);

    const char *languages[] = {"Bash", "C", "C++", "Java", "Python", "Normal Text"};
    GSList *lang_group = NULL;
    for (int i = 0; i < 6; i++) {
        GtkWidget *l_item = gtk_radio_menu_item_new_with_label(lang_group, languages[i]);
        lang_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(l_item));
        if (strcmp(languages[i], cfg.language) == 0) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(l_item), TRUE);
        }
        g_signal_connect(l_item, "activate", G_CALLBACK(on_language_changed), (gpointer)languages[i]);
        gtk_menu_shell_append(GTK_MENU_SHELL(lang_submenu), l_item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
    GtkWidget *setup_cmd_mi = create_menu_item_with_icon("Setup _Commands...", "preferences-system");
    g_signal_connect(setup_cmd_mi, "activate", G_CALLBACK(on_setup_commands_clicked), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), edit_mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), options_mi);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), setup_cmd_mi);
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
}

static void build_main_toolbar(GtkWidget *vbox) {
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    
    GtkToolItem *tb_new = gtk_tool_button_new(gtk_image_new_from_icon_name("document-new", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_new, "New File (Ctrl+N)");
    g_signal_connect(tb_new, "clicked", G_CALLBACK(on_new_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_new, -1);

    GtkToolItem *tb_open = gtk_tool_button_new(gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_open, "Open File (Ctrl+O)");
    g_signal_connect(tb_open, "clicked", G_CALLBACK(on_open_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_open, -1);

    GtkToolItem *tb_save = gtk_tool_button_new(gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_save, "Save File (Ctrl+S)");
    g_signal_connect(tb_save, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_save, -1);

    GtkToolItem *tb_save_as = gtk_tool_button_new(gtk_image_new_from_icon_name("document-save-as", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_save_as, "Save As... (Ctrl+Shift+S)");
    g_signal_connect(tb_save_as, "clicked", G_CALLBACK(on_save_as_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_save_as, -1);

    GtkToolItem *tb_quit = gtk_tool_button_new(gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_quit, "Quit (Ctrl+Q)");
    g_signal_connect(tb_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_quit, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    GtkToolItem *tb_undo = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-undo", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_undo, "Undo (Ctrl+Z)");
    g_signal_connect(tb_undo, "clicked", G_CALLBACK(on_undo_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_undo, -1);

    GtkToolItem *tb_redo = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-redo", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_redo, "Redo (Ctrl+Y)");
    g_signal_connect(tb_redo, "clicked", G_CALLBACK(on_redo_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_redo, -1);

    GtkToolItem *tb_cut = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-cut", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_cut, "Cut (Ctrl+X)");
    g_signal_connect(tb_cut, "clicked", G_CALLBACK(on_cut_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_cut, -1);

    GtkToolItem *tb_copy = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-copy", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_copy, "Copy (Ctrl+C)");
    g_signal_connect(tb_copy, "clicked", G_CALLBACK(on_copy_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_copy, -1);

    GtkToolItem *tb_paste = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-paste", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_paste, "Paste (Ctrl+V)");
    g_signal_connect(tb_paste, "clicked", G_CALLBACK(on_paste_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_paste, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    GtkToolItem *tb_find = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-find", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_find, "Find text (Ctrl+F)");
    g_signal_connect(tb_find, "clicked", G_CALLBACK(on_search_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_find, -1);

    GtkToolItem *tb_replace = gtk_tool_button_new(gtk_image_new_from_icon_name("edit-find-replace", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_replace, "Replace text (Ctrl+H)");
    g_signal_connect(tb_replace, "clicked", G_CALLBACK(on_replace_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_replace, -1);

    GtkToolItem *tb_goto = gtk_tool_button_new(gtk_image_new_from_icon_name("go-jump", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_goto, "Go to line (Ctrl+G)");
    g_signal_connect(tb_goto, "clicked", G_CALLBACK(on_goto_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_goto, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    GtkToolItem *tb_build = gtk_tool_button_new(gtk_image_new_from_icon_name("preferences-system", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_build, "Build Project");
    g_signal_connect(tb_build, "clicked", G_CALLBACK(on_build_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_build, -1);

    GtkToolItem *tb_run = gtk_tool_button_new(gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_run, "Run Project");
    g_signal_connect(tb_run, "clicked", G_CALLBACK(on_run_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_run, -1);

    GtkToolItem *tb_debug = gtk_tool_button_new(gtk_image_new_from_icon_name("system-run", GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_tool_item_set_tooltip_text(tb_debug, "Debug Project");
    g_signal_connect(tb_debug, "clicked", G_CALLBACK(on_debug_clicked), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tb_debug, -1);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
}

static void build_bottom_notebook(GtkWidget *paned) {
	notebook_bottom = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_bottom), TRUE);
    gtk_paned_pack2(GTK_PANED(paned), notebook_bottom, FALSE, TRUE);

    const char *user_shell = g_getenv("SHELL");
    if (!user_shell) {
        user_shell = "/bin/bash";
    }

    vte_term = vte_terminal_new();
    vte_terminal_spawn_async(VTE_TERMINAL(vte_term),
        VTE_PTY_DEFAULT, NULL,
        (char *[]){(char *)user_shell, "-l", NULL},
        NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL);

    GdkRGBA bg_color, fg_color;
    gdk_rgba_parse(&bg_color, "#141414");
    gdk_rgba_parse(&fg_color, "#FFFFFF");

    GdkRGBA palette[16];
    gdk_rgba_parse(&palette[0],  "#000000");
    gdk_rgba_parse(&palette[1],  "#EF2929");
    gdk_rgba_parse(&palette[2],  "#4E9A06");
    gdk_rgba_parse(&palette[3],  "#C4A000");
    gdk_rgba_parse(&palette[4],  "#3465A4");
    gdk_rgba_parse(&palette[5],  "#75507B");
    gdk_rgba_parse(&palette[6],  "#06989A");
    gdk_rgba_parse(&palette[7],  "#D3D7CF");
    gdk_rgba_parse(&palette[8],  "#555753");
    gdk_rgba_parse(&palette[9],  "#EF2929");
    gdk_rgba_parse(&palette[10], "#8AE234");
    gdk_rgba_parse(&palette[11], "#FCE94F");
    gdk_rgba_parse(&palette[12], "#729FCF");
    gdk_rgba_parse(&palette[13], "#AD7FA8");
    gdk_rgba_parse(&palette[14], "#34E2E2");
    gdk_rgba_parse(&palette[15], "#EEEEEC");

    vte_terminal_set_colors(VTE_TERMINAL(vte_term), &fg_color, &bg_color, palette, 16);


    vte_terminal_set_colors(VTE_TERMINAL(vte_term), &fg_color, &bg_color, palette, 16);
    PangoFontDescription *vte_font = pango_font_description_from_string("JetBrainsMono Nerd Font Regular 11");
    vte_terminal_set_font(VTE_TERMINAL(vte_term), vte_font);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_bottom), vte_term, gtk_label_new("Terminal"));

    vte_debug = vte_terminal_new();
    vte_terminal_spawn_async(VTE_TERMINAL(vte_debug),
        VTE_PTY_DEFAULT, NULL,
        (char *[]){(char *)user_shell, "-l", NULL},
        NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL);

    vte_terminal_set_colors(VTE_TERMINAL(vte_debug), &fg_color, &bg_color, palette, 16);
    vte_terminal_set_font(VTE_TERMINAL(vte_debug), vte_font);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_bottom), vte_debug, gtk_label_new("Debugger"));

    pango_font_description_free(vte_font);

    GtkWidget *notes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *notes_view = gtk_text_view_new();
    GtkWidget *notes_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(notes_scroll), notes_view);
    gtk_box_pack_start(GTK_BOX(notes_box), notes_scroll, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_bottom), notes_box, gtk_label_new("Notes"));

    GtkWidget *drawing_area_widget = create_drawing_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_bottom), drawing_area_widget, gtk_label_new("Drawing Area"));
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    init();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Editor 1.0");
    gtk_window_set_default_size(GTK_WINDOW(window), cfg.width, cfg.height);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_NONE);
    gtk_window_move(GTK_WINDOW(window), cfg.pos_x, cfg.pos_y);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    g_signal_connect(window, "configure-event", G_CALLBACK(on_window_configure), NULL);


    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    build_menu_bar(vbox, accel_group);
    build_main_toolbar(vbox);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    g_signal_connect(notebook, "switch-page", G_CALLBACK(on_switch_page), NULL);
    gtk_paned_pack1(GTK_PANED(paned), notebook, TRUE, FALSE);

    build_bottom_notebook(paned);

    gtk_paned_set_position(GTK_PANED(paned), (int)(cfg.height * 0.65));

    statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);

    if (strlen(cfg.last_file) > 0 && g_file_test(cfg.last_file, G_FILE_TEST_EXISTS)) {
        create_new_tab(cfg.last_file);
        TabData *tab = get_current_tab_data();
        if (tab) {
            if (tab->font_family) g_free(tab->font_family);
            tab->font_family = g_strdup(cfg.font);
            tab->font_size = cfg.font_size;
            apply_font_to_tab(tab);
        }
    }

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
