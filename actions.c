#include "editor.h"

static cairo_surface_t *surface = NULL;
static double current_red = 0.0, current_green = 0.0, current_blue = 0.0;

void free_tab_data(TabData *tab) {
    if (!tab) return;
    if (tab->file_path) g_free(tab->file_path);
    if (tab->font_family) g_free(tab->font_family);
    if (tab->css_provider) g_object_unref(tab->css_provider);
    g_free(tab);
}

TabData* get_current_tab_data(void) {
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    if (current_page == -1) return NULL;
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), current_page);
    return (TabData*)g_object_get_data(G_OBJECT(page), "tab_data");
}

void update_tab_label(TabData *tab) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(tab->tab_label_box));
    if (children) {
        GtkWidget *label = GTK_WIDGET(children->data);
        if (GTK_IS_LABEL(label)) {
            char *base_name = tab->file_path ? g_path_get_basename(tab->file_path) : g_strdup("Untitled Document");
            gtk_label_set_text(GTK_LABEL(label), base_name);
            g_free(base_name);
        }
        g_list_free(children);
    }
}

void apply_font_to_tab(TabData *tab) {
    if (!tab || !tab->css_provider) return;
    char *css = g_strdup_printf("textview { font-family: '%s'; font-size: %dpt; }", tab->font_family, tab->font_size);
    gtk_css_provider_load_from_data(tab->css_provider, css, -1, NULL);
    g_free(css);
}

void apply_theme_to_buffer(GtkSourceBuffer *buffer, const char *theme_id) {
    if (g_strcmp0(theme_id, "none") == 0) {
        gtk_source_buffer_set_style_scheme(buffer, NULL);
        return;
    }
    GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(manager, theme_id);
    if (scheme) {
        gtk_source_buffer_set_style_scheme(buffer, scheme);
    }
}

static void apply_language_highlighting(GtkSourceBuffer *buffer, const char *filename) {
    if (!filename) return;
    GtkSourceLanguageManager *lang_manager = gtk_source_language_manager_get_default();
    GtkSourceLanguage *lang = gtk_source_language_manager_guess_language(lang_manager, filename, NULL);
    if (lang) {
        gtk_source_buffer_set_language(buffer, lang);
        gtk_source_buffer_set_highlight_syntax(buffer, TRUE);
    }
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    if (event->state & GDK_CONTROL_MASK) {
        TabData *tab = (TabData *)user_data;
        gdouble delta_y = 0;
        if (event->direction == GDK_SCROLL_UP) delta_y = -1;
        else if (event->direction == GDK_SCROLL_DOWN) delta_y = 1;
        else if (event->direction == GDK_SCROLL_SMOOTH) gdk_event_get_scroll_deltas((GdkEvent *)event, NULL, &delta_y);

        if (delta_y < 0) tab->font_size += 1;
        else if (delta_y > 0) {
            tab->font_size -= 1;
            if (tab->font_size < 6) tab->font_size = 6;
        }
        if (delta_y != 0) apply_font_to_tab(tab);
        return TRUE;
    }
    return FALSE;
}

static void on_close_tab_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *scroll_win = GTK_WIDGET(user_data);
    int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), scroll_win);
    if (page_num != -1) gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_num);
}

static void on_buffer_changed_clear_error(GtkTextBuffer *buf, gpointer data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_iter_at_mark(buf, &start, gtk_text_buffer_get_insert(buf));
    end = start;
    gtk_text_iter_set_line_offset(&start, 0);
    gtk_text_iter_forward_to_line_end(&end);
    gtk_text_buffer_remove_tag_by_name(buf, "error_line", &start, &end);
}

void create_new_tab(const char *filename) {
    TabData *tab = g_new0(TabData, 1);
    tab->font_size = 10;
    tab->font_family = g_strdup("Monospace");
    tab->buffer = gtk_source_buffer_new(NULL);
    tab->source_view = gtk_source_view_new_with_buffer(tab->buffer);
    
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->source_view), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(tab->source_view), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->source_view), TRUE);
    gtk_source_buffer_set_highlight_matching_brackets(GTK_SOURCE_BUFFER(tab->buffer), TRUE);
    
    apply_theme_to_buffer(tab->buffer, current_theme_id);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(tab->buffer), "error_line", "background", "#FFCCCC", "foreground", "#990000", NULL);
    tab->css_provider = gtk_css_provider_new();
    apply_font_to_tab(tab);
    
    gtk_style_context_add_provider(gtk_widget_get_style_context(tab->source_view),
                                   GTK_STYLE_PROVIDER(tab->css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(tab->source_view, "scroll-event", G_CALLBACK(on_scroll_event), tab);

    if (filename) {
        tab->file_path = g_strdup(filename);
        apply_language_highlighting(tab->buffer, filename);
        GFile *file = g_file_new_for_path(filename);
        GtkSourceFile *source_file = gtk_source_file_new();
        gtk_source_file_set_location(source_file, file);
        GtkSourceFileLoader *loader = gtk_source_file_loader_new(tab->buffer, source_file);
        gtk_source_file_loader_load_async(loader, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, NULL, NULL);
        g_object_unref(loader); g_object_unref(source_file); g_object_unref(file);
    }

    tab->scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(tab->scroll_win), tab->source_view);
    
    tab->tab_label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    char *base_name = tab->file_path ? g_path_get_basename(tab->file_path) : g_strdup("Untitled Document");
    GtkWidget *label = gtk_label_new(base_name);
    g_free(base_name);

    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);

    GtkWidget *close_btn = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(close_btn), gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU));
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    GtkCssProvider *btn_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(btn_provider, "button { padding: 2px; border: none; background: none; } button:hover { background: #e0e0e0; border-radius: 3px; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(close_btn), GTK_STYLE_PROVIDER(btn_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(btn_provider);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkRequisition req;
    gtk_widget_get_preferred_size(close_btn, NULL, &req);
    gtk_widget_set_size_request(spacer, req.width, -1);

    gtk_box_pack_start(GTK_BOX(tab->tab_label_box), spacer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab->tab_label_box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tab->tab_label_box), close_btn, FALSE, FALSE, 0);
    gtk_widget_show_all(tab->tab_label_box);

    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab_clicked), tab->scroll_win);
    g_signal_connect(tab->buffer, "changed", G_CALLBACK(update_statusbar), NULL);
	g_signal_connect(tab->buffer, "changed", G_CALLBACK(on_buffer_changed_clear_error), NULL);

    g_signal_connect_after(tab->buffer, "mark-set", G_CALLBACK(update_statusbar), NULL);
    int index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab->scroll_win, tab->tab_label_box);
    gtk_container_child_set(GTK_CONTAINER(notebook), tab->scroll_win, "tab-expand", TRUE, NULL);
    g_object_set_data_full(G_OBJECT(tab->scroll_win), "tab_data", tab, (GDestroyNotify)free_tab_data);
    
    gtk_widget_show_all(tab->scroll_win);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), index);
    update_statusbar(GTK_TEXT_BUFFER(tab->buffer), NULL);
}

void on_new_clicked(void) { create_new_tab(NULL); }
void on_open_clicked(void) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        create_new_tab(filename); g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void execute_save(TabData *tab) {
    GFile *file = g_file_new_for_path(tab->file_path);
    GtkSourceFile *source_file = gtk_source_file_new();
    gtk_source_file_set_location(source_file, file);
    GtkSourceFileSaver *saver = gtk_source_file_saver_new(tab->buffer, source_file);
    gtk_source_file_saver_save_async(saver, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, NULL, NULL);
    g_object_unref(saver); g_object_unref(source_file); g_object_unref(file);
    apply_language_highlighting(tab->buffer, tab->file_path);
    update_tab_label(tab);
}

void on_save_clicked(void) {
    TabData *tab = get_current_tab_data(); if (!tab) return;
    if (tab->file_path == NULL) {
        GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            tab->file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)); execute_save(tab);
        }
        gtk_widget_destroy(dialog);
    } else execute_save(tab);
}

void on_save_as_clicked(void) {
    TabData *tab = get_current_tab_data(); if (!tab) return;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File As...", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    if (tab->file_path) gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), tab->file_path);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        g_free(tab->file_path); tab->file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)); execute_save(tab);
    }
    gtk_widget_destroy(dialog);
}

void on_font_clicked(void) {
    TabData *tab = get_current_tab_data(); if (!tab) return;
    GtkWidget *dialog = gtk_font_chooser_dialog_new("Select Font", GTK_WINDOW(window));
    char *initial_font = g_strdup_printf("%s %d", tab->font_family, tab->font_size);
    gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dialog), initial_font); g_free(initial_font);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        PangoFontDescription *font_desc = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(dialog));
        if (font_desc) {
            g_free(tab->font_family); tab->font_family = g_strdup(pango_font_description_get_family(font_desc));
            tab->font_size = pango_font_description_get_size(font_desc) / PANGO_SCALE;
            apply_font_to_tab(tab); pango_font_description_free(font_desc);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_theme_changed(GtkMenuItem *menu_item, gpointer user_data) {
    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item))) return;
    current_theme_id = (const char *)user_data;
    int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    for (int i = 0; i < n_pages; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
        TabData *tab = (TabData*)g_object_get_data(G_OBJECT(page), "tab_data");
        if (tab && tab->buffer) apply_theme_to_buffer(tab->buffer, current_theme_id);
    }
}

void on_undo_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (tab && gtk_source_buffer_can_undo(tab->buffer)) gtk_source_buffer_undo(tab->buffer);
}
void on_redo_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (tab && gtk_source_buffer_can_redo(tab->buffer)) gtk_source_buffer_redo(tab->buffer);
}
void on_cut_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (tab) g_signal_emit_by_name(tab->source_view, "cut-clipboard");
}
void on_copy_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (tab) g_signal_emit_by_name(tab->source_view, "copy-clipboard");
}
void on_paste_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (tab) g_signal_emit_by_name(tab->source_view, "paste-clipboard");
}

void on_search_clicked(void) {
    TabData *tab = get_current_tab_data(); if (!tab) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Search", GTK_WINDOW(window), GTK_DIALOG_MODAL, "_Find", GTK_RESPONSE_ACCEPT, "_Close", GTK_RESPONSE_REJECT, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Text to find...");
    gtk_container_add(GTK_CONTAINER(content_area), entry); gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
        GtkTextIter start, match_start, match_end;
        gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(tab->buffer), &start);
        if (gtk_text_iter_forward_search(&start, text, GTK_TEXT_SEARCH_VISIBLE_ONLY, &match_start, &match_end, NULL)) {
            gtk_text_buffer_select_range(GTK_TEXT_BUFFER(tab->buffer), &match_start, &match_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->source_view), &match_start, 0.0, FALSE, 0.0, 0.0);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_replace_clicked(void) {
    TabData *tab = get_current_tab_data(); if (!tab) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Replace", GTK_WINDOW(window), GTK_DIALOG_MODAL, "_Replace", GTK_RESPONSE_ACCEPT, "_Close", GTK_RESPONSE_REJECT, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *e_find = gtk_entry_new(); GtkWidget *e_rep = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Find:"), 0, 0, 1, 1); gtk_grid_attach(GTK_GRID(grid), e_find, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Replace:"), 0, 1, 1, 1); gtk_grid_attach(GTK_GRID(grid), e_rep, 1, 1, 1, 1);
    gtk_container_add(GTK_CONTAINER(content_area), grid); gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *find_txt = gtk_entry_get_text(GTK_ENTRY(e_find));
        const char *rep_txt = gtk_entry_get_text(GTK_ENTRY(e_rep));
        GtkTextIter start, match_start, match_end;
        gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(tab->buffer), &start);
        if (gtk_text_iter_forward_search(&start, find_txt, GTK_TEXT_SEARCH_VISIBLE_ONLY, &match_start, &match_end, NULL)) {
            gtk_text_buffer_begin_user_action(GTK_TEXT_BUFFER(tab->buffer));
            gtk_text_buffer_delete(GTK_TEXT_BUFFER(tab->buffer), &match_start, &match_end);
            gtk_text_buffer_insert(GTK_TEXT_BUFFER(tab->buffer), &match_start, rep_txt, -1);
            gtk_text_buffer_end_user_action(GTK_TEXT_BUFFER(tab->buffer));
        }
    }
    gtk_widget_destroy(dialog);
}

void on_goto_clicked(void) {
    TabData *tab = get_current_tab_data();
    if (!tab) return;

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Go To", GTK_WINDOW(window), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_Jump to", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int total_lines = gtk_text_buffer_get_line_count(GTK_TEXT_BUFFER(tab->buffer));
    
    GtkAdjustment *adj_line = gtk_adjustment_new(1, 1, total_lines, 1, 10, 0);
    GtkWidget *spin_line = gtk_spin_button_new(adj_line, 1, 0);
    
    GtkAdjustment *adj_col = gtk_adjustment_new(1, 1, 1000, 1, 10, 0);
    GtkWidget *spin_col = gtk_spin_button_new(adj_col, 1, 0);

    GtkTextIter current_iter;
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(tab->buffer), &current_iter, gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(tab->buffer)));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_line), gtk_text_iter_get_line(&current_iter) + 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_col), gtk_text_iter_get_line_offset(&current_iter) + 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Line number:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spin_line, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Column number:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spin_col, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        int target_line = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_line)) - 1;
        int target_col = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_col)) - 1;

        GtkTextIter target_iter;
        gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(tab->buffer), &target_iter, target_line);
        
        while (target_col > 0 && !gtk_text_iter_ends_line(&target_iter)) {
            gtk_text_iter_forward_char(&target_iter);
            target_col--;
        }

        gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(tab->buffer), &target_iter);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->source_view), &target_iter, 0.0, FALSE, 0.0, 0.0);
    }
    gtk_widget_destroy(dialog);
}

GtkWidget* create_menu_item_with_icon(const char *label_text, const char *icon_name) {
    GtkWidget *mi = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    GtkWidget *label = gtk_label_new_with_mnemonic(label_text);
    gtk_box_pack_start(GTK_BOX(mi), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mi), label, FALSE, FALSE, 0);
    GtkWidget *menu_item = gtk_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu_item), mi);
    return menu_item;
}

void update_statusbar(GtkTextBuffer *buffer, gpointer user_data) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    int row = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;
    int total_lines = gtk_text_buffer_get_line_count(buffer);

    const char *lang_name = "Plain Text";
    GtkSourceLanguage *lang = gtk_source_buffer_get_language(GTK_SOURCE_BUFFER(buffer));
    if (lang) {
        lang_name = gtk_source_language_get_name(lang);
    }

    char *status = g_strdup_printf("Ln %d, Col %d  |  Total Lines: %d  |  Language: %s", row, col, total_lines, lang_name);
    gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0);
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, status);
    g_free(status);
}

static int current_width = 2;

typedef enum { TOOL_POINTHAND, TOOL_LINE, TOOL_RECT } DrawTool;
static DrawTool current_tool = TOOL_POINTHAND;
static double start_x = -1.0, start_y = -1.0;
static double last_x = -1.0, last_y = -1.0;

static GtkWidget *drawing_scroll = NULL;
static double hand_start_hadj = 0.0, hand_start_vadj = 0.0;
static double hand_start_x = 0.0, hand_start_y = 0.0;
static gboolean is_panning = FALSE;

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
    if (surface) {
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
    }
    if ((current_tool == TOOL_LINE || current_tool == TOOL_RECT) && start_x >= 0 && last_x >= 0) {
        cairo_set_source_rgb(cr, current_red, current_green, current_blue);
        cairo_set_line_width(cr, current_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        if (current_tool == TOOL_LINE) {
            cairo_move_to(cr, start_x, start_y);
            cairo_line_to(cr, last_x, last_y);
        } else {
            cairo_rectangle(cr, start_x, start_y, last_x - start_x, last_y - start_y);
        }
        cairo_stroke(cr);
    }
    return FALSE;
}

static gboolean configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    if (!surface) {
        surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget), CAIRO_CONTENT_COLOR, 2000, 2000);
        cairo_t *cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
    return TRUE;
}

static gboolean motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (is_panning) {
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(drawing_scroll));
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(drawing_scroll));
        double dx = event->x_root - hand_start_x;
        double dy = event->y_root - hand_start_y;
        gtk_adjustment_set_value(hadj, hand_start_hadj - dx);
        gtk_adjustment_set_value(vadj, hand_start_vadj - dy);
    } else if (event->state & GDK_BUTTON1_MASK) {
        if (current_tool == TOOL_POINTHAND) {
            cairo_t *cr = cairo_create(surface);
            cairo_set_source_rgb(cr, current_red, current_green, current_blue);
            cairo_set_line_width(cr, current_width);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            if (last_x < 0) { last_x = event->x; last_y = event->y; }
            cairo_move_to(cr, last_x, last_y);
            cairo_line_to(cr, event->x, event->y);
            cairo_stroke(cr);
            cairo_destroy(cr);
            last_x = event->x; last_y = event->y;
            gtk_widget_queue_draw(widget);
        } else if (current_tool == TOOL_LINE || current_tool == TOOL_RECT) {
            last_x = event->x; last_y = event->y;
            gtk_widget_queue_draw(widget);
        }
    }
    return TRUE;
}

static gboolean button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == GDK_BUTTON_SECONDARY || event->button == GDK_BUTTON_MIDDLE) {
        is_panning = TRUE;
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(drawing_scroll));
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(drawing_scroll));
        hand_start_hadj = gtk_adjustment_get_value(hadj);
        hand_start_vadj = gtk_adjustment_get_value(vadj);
        hand_start_x = event->x_root;
        hand_start_y = event->y_root;
        return TRUE;
    }
    if (event->button == GDK_BUTTON_PRIMARY) {
        start_x = event->x; start_y = event->y;
        last_x = event->x; last_y = event->y;
        if (current_tool == TOOL_POINTHAND) {
            cairo_t *cr = cairo_create(surface);
            cairo_set_source_rgb(cr, current_red, current_green, current_blue);
            cairo_set_line_width(cr, current_width);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, event->x, event->y);
            cairo_line_to(cr, event->x, event->y);
            cairo_stroke(cr);
            cairo_destroy(cr);
            gtk_widget_queue_draw(widget);
        }
    }
    return TRUE;
}

static gboolean button_release_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == GDK_BUTTON_SECONDARY || event->button == GDK_BUTTON_MIDDLE) {
        is_panning = FALSE;
        return TRUE;
    }
    if (event->button == GDK_BUTTON_PRIMARY) {
        if (current_tool == TOOL_LINE || current_tool == TOOL_RECT) {
            cairo_t *cr = cairo_create(surface);
            cairo_set_source_rgb(cr, current_red, current_green, current_blue);
            cairo_set_line_width(cr, current_width);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            if (current_tool == TOOL_LINE) {
                cairo_move_to(cr, start_x, start_y);
                cairo_line_to(cr, event->x, event->y);
            } else {
                cairo_rectangle(cr, start_x, start_y, event->x - start_x, event->y - start_y);
            }
            cairo_stroke(cr);
            cairo_destroy(cr);
        }
        start_x = -1.0; start_y = -1.0;
        last_x = -1.0; last_y = -1.0;
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

static void color_changed_cb(GtkColorButton *button, gpointer data) {
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    current_red = color.red; current_green = color.green; current_blue = color.blue;
}

static void width_changed_cb(GtkSpinButton *spin, gpointer data) {
    current_width = gtk_spin_button_get_value_as_int(spin);
}

static void tool_changed_cb(GtkComboBox *combo, gpointer data) {
    current_tool = (DrawTool)gtk_combo_box_get_active(combo);
}

static void da_new_cb(GtkWidget *btn, gpointer da) {
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_destroy(cr);
    gtk_widget_queue_draw(GTK_WIDGET(da));
}

static void da_open_cb(GtkWidget *btn, gpointer da) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Image", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_set_name(filter, "Images (*.png, *.jpg, *.jpeg)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        GError *error = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
        
        if (pixbuf) {
            cairo_t *cr = cairo_create(surface);
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_paint(cr);
            gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
            cairo_paint(cr);
            cairo_destroy(cr);
            g_object_unref(pixbuf);
            gtk_widget_queue_draw(GTK_WIDGET(da));
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void da_save_cb(GtkWidget *btn, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Image", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "drawing.jpg");
    
    GtkFileFilter *f_jpg = gtk_file_filter_new();
    gtk_file_filter_add_pattern(f_jpg, "*.jpg");
    gtk_file_filter_set_name(f_jpg, "JPEG Image (*.jpg)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), f_jpg);

    GtkFileFilter *f_png = gtk_file_filter_new();
    gtk_file_filter_add_pattern(f_png, "*.png");
    gtk_file_filter_set_name(f_png, "PNG Image (*.png)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), f_png);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, 2000, 2000);
        if (pixbuf) {
            const char *type = "jpeg";
            if (g_str_has_suffix(filename, ".png") || g_str_has_suffix(filename, ".PNG")) {
                type = "png";
            }
            gdk_pixbuf_save(pixbuf, filename, type, NULL, NULL);
            g_object_unref(pixbuf);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

GtkWidget* create_drawing_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *color_btn = gtk_color_button_new();
    GtkAdjustment *adj = gtk_adjustment_new(2, 1, 10, 1, 2, 0);
    GtkWidget *spin = gtk_spin_button_new(adj, 1, 0);
    GtkWidget *da = gtk_drawing_area_new();

    GtkWidget *btn_new = gtk_button_new_from_icon_name("document-new", GTK_ICON_SIZE_MENU);
    GtkWidget *btn_open = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
    GtkWidget *btn_save = gtk_button_new_from_icon_name("document-save", GTK_ICON_SIZE_MENU);
    GtkWidget *btn_saveas = gtk_button_new_from_icon_name("document-save-as", GTK_ICON_SIZE_MENU);

    gtk_widget_set_tooltip_text(btn_new, "Clear Canvas");
    gtk_widget_set_tooltip_text(btn_open, "Load PNG Image");
    gtk_widget_set_tooltip_text(btn_save, "Save Image");
    gtk_widget_set_tooltip_text(btn_saveas, "Save Image As...");

    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Point & Hand");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Line");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Rectangle");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

    gtk_box_pack_start(GTK_BOX(hbox), btn_new, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_open, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_save, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), btn_saveas, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 2);
    
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Tool:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Color:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), color_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Width:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *frame = gtk_frame_new(NULL);
    drawing_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(da, 2000, 2000);
    
    gtk_container_add(GTK_CONTAINER(drawing_scroll), da);
    gtk_container_add(GTK_CONTAINER(frame), drawing_scroll);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

    g_signal_connect(da, "draw", G_CALLBACK(draw_cb), NULL);
    g_signal_connect(da, "configure-event", G_CALLBACK(configure_event_cb), NULL);
    g_signal_connect(da, "motion-notify-event", G_CALLBACK(motion_notify_event_cb), NULL);
    g_signal_connect(da, "button-press-event", G_CALLBACK(button_press_event_cb), NULL);
    g_signal_connect(da, "button-release-event", G_CALLBACK(button_release_event_cb), NULL);
    
    g_signal_connect(btn_new, "clicked", G_CALLBACK(da_new_cb), da);
    g_signal_connect(btn_open, "clicked", G_CALLBACK(da_open_cb), da);
    g_signal_connect(btn_save, "clicked", G_CALLBACK(da_save_cb), NULL);
    g_signal_connect(btn_saveas, "clicked", G_CALLBACK(da_save_cb), NULL);

    gtk_widget_set_events(da, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(color_btn, "color-set", G_CALLBACK(color_changed_cb), NULL);
    g_signal_connect(spin, "value-changed", G_CALLBACK(width_changed_cb), NULL);
    g_signal_connect(combo, "changed", G_CALLBACK(tool_changed_cb), NULL);

    return vbox;
}

void on_language_changed(GtkMenuItem *menu_item, gpointer user_data) {
    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item))) return;
    const char *selected_lang = (const char *)user_data;
    if (selected_lang) {
        strncpy(cfg.language, selected_lang, sizeof(cfg.language) - 1);
        cfg.language[sizeof(cfg.language) - 1] = '\0';
        
        TabData *tab = get_current_tab_data();
        if (tab && tab->buffer) {
            GtkSourceLanguageManager *lang_manager = gtk_source_language_manager_get_default();
            GtkSourceLanguage *lang = NULL;
            
            if (strcmp(selected_lang, "C") == 0) lang = gtk_source_language_manager_get_language(lang_manager, "c");
            else if (strcmp(selected_lang, "C++") == 0) lang = gtk_source_language_manager_get_language(lang_manager, "cpp");
            else if (strcmp(selected_lang, "Python") == 0) lang = gtk_source_language_manager_get_language(lang_manager, "python");
            else if (strcmp(selected_lang, "Java") == 0) lang = gtk_source_language_manager_get_language(lang_manager, "java");
            else if (strcmp(selected_lang, "Bash") == 0) lang = gtk_source_language_manager_get_language(lang_manager, "sh");
            
            gtk_source_buffer_set_language(tab->buffer, lang);
            gtk_source_buffer_set_highlight_syntax(tab->buffer, lang != NULL);
            
            update_statusbar(GTK_TEXT_BUFFER(tab->buffer), NULL);
        }
    }
}

void on_setup_commands_clicked(void) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Setup Commands", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
    gtk_widget_destroy(dialog);

    dialog = gtk_dialog_new_with_buttons("Setup Commands", GTK_WINDOW(window), GTK_DIALOG_MODAL, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 250);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);

    GtkWidget *build_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(build_view), GTK_WRAP_WORD);
    GtkWidget *build_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(build_scroll, -1, 60);
    gtk_container_add(GTK_CONTAINER(build_scroll), build_view);
    gtk_widget_set_hexpand(build_scroll, TRUE);

    GtkTextBuffer *build_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(build_view));
    gtk_text_buffer_set_text(build_buf, cfg.cmd_build, -1);

    GtkWidget *e_run = gtk_entry_new();
    GtkWidget *e_debug = gtk_entry_new();

    gtk_widget_set_hexpand(e_run, TRUE);
    gtk_widget_set_hexpand(e_debug, TRUE);

    gtk_entry_set_text(GTK_ENTRY(e_run), cfg.cmd_run);
    gtk_entry_set_text(GTK_ENTRY(e_debug), cfg.cmd_debug);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Build command:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), build_scroll, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Run command:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e_run, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Debug command:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e_debug, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkTextIter start, end;
        gtk_text_buffer_get_start_iter(build_buf, &start);
        gtk_text_buffer_get_end_iter(build_buf, &end);
        char *build_text = gtk_text_buffer_get_text(build_buf, &start, &end, FALSE);
        char *cleaned_build = g_strstrip(g_strdup(build_text));
        g_free(build_text);

        strncpy(cfg.cmd_build, cleaned_build, sizeof(cfg.cmd_build) - 1);
        cfg.cmd_build[sizeof(cfg.cmd_build) - 1] = '\0';
        g_free(cleaned_build);

        strncpy(cfg.cmd_run, gtk_entry_get_text(GTK_ENTRY(e_run)), sizeof(cfg.cmd_run) - 1);
        cfg.cmd_run[sizeof(cfg.cmd_run) - 1] = '\0';

        strncpy(cfg.cmd_debug, gtk_entry_get_text(GTK_ENTRY(e_debug)), sizeof(cfg.cmd_debug) - 1);
        cfg.cmd_debug[sizeof(cfg.cmd_debug) - 1] = '\0';
    }
    gtk_widget_destroy(dialog);
}

void on_compile_finished(GtkWidget *vte, int status, gpointer user_data) {
    printf("\n[DEBUG] --- FUNKCIA on_compile_finished SPUSTENA ---\n");
    
    // Použijeme priame a bezpečné vytiahnutie aktívnej záložky
    TabData *tab = get_current_tab_data();
    if (!tab || !tab->buffer) {
        printf("[DEBUG] Chyba: Aktualna zalozka alebo jej buffer je NULL! Skener konci.\n");
        return;
    }

    printf("[DEBUG] Zalozka najdena úspesne. Cistim stare zvraznenia...\n");
    GtkTextIter s_start, s_end;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(tab->buffer), &s_start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(tab->buffer), &s_end);
    gtk_text_buffer_remove_tag_by_name(GTK_TEXT_BUFFER(tab->buffer), "error_line", &s_start, &s_end);

    long row_count = vte_terminal_get_row_count(VTE_TERMINAL(vte));
    printf("[DEBUG] Pocet riadkov vo VTE terminali na kontrolu: %ld\n", row_count);
    gboolean first_scroll = TRUE;

    for (long r = 0; r < row_count; r++) {
        char *line = vte_terminal_get_text_range(VTE_TERMINAL(vte), r, 0, r, 511, NULL, NULL, NULL);
        if (!line || strlen(g_strstrip(g_strdup(line))) == 0) {
            if (line) g_free(line);
            continue;
        }

        printf("[DEBUG] Citam riadok z VTE %ld: %s", r, line);

        analyze_problem prob;
        analyze(line, &prob);
        g_free(line);

        if (prob.line_num != -1) {
            printf("[DEBUG] Nasiel sa platny problem na riadku %d v subore %s\n", prob.line_num, prob.file_name);
            int target_line = prob.line_num - 1;
            
            if (g_strrstr(prob.description, "expected ';'") || g_strrstr(prob.description, "expected ','")) {
                printf("[DEBUG] Detegovana chyba bodkočiarky. Hladam predchadzajuci neohraniceny riadok...\n");
                while (target_line > 0) {
                    GtkTextIter check_start, check_end;
                    gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(tab->buffer), &check_start, target_line - 1);
                    check_end = check_start;
                    gtk_text_iter_forward_to_line_end(&check_end);
                    char *line_text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(tab->buffer), &check_start, &check_end, FALSE);
                    
                    if (!is_comment_or_empty(line_text)) {
                        target_line--;
                        printf("[DEBUG] Nasiel sa stred chyby, posuvam zvraznenie na riadok %d\n", target_line + 1);
                        g_free(line_text);
                        break;
                    }
                    g_free(line_text);
                    target_line--;
                }
            }

            GtkTextIter start, end;
            gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(tab->buffer), &start, target_line);
            end = start;
            gtk_text_iter_forward_to_line_end(&end);

            GtkTextTag *err_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(GTK_TEXT_BUFFER(tab->buffer)), "error_line");
            gtk_text_buffer_apply_tag(GTK_TEXT_BUFFER(tab->buffer), err_tag, &start, &end);
            printf("[DEBUG] Tag error_line uspesne aplikovany na riadok %d v editore\n", target_line + 1);
            
            gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0);
            gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, g_strdup_printf("❌ Riadok %d: %s -> Riešenie: %s", target_line + 1, prob.description, prob.solution));

            if (first_scroll) {
                gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->source_view), &start, 0.0, FALSE, 0.0, 0.0);
                first_scroll = FALSE;
            }
        }
    }
    printf("[DEBUG] --- KONEC FUNKCIE on_compile_finished ---\n\n");
}

void on_build_clicked(void) {
    char *sys_cmd = g_strdup_printf("(%s) > project.log 2>&1", cfg.cmd_build);
    system(sys_cmd);
    g_free(sys_cmd);

    if (vte_term) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_bottom), 0);
        parse_buffer_errors(vte_term);
    }
}

void on_run_clicked(void) {
    char *cmd = g_strdup_printf("%s\n", cfg.cmd_run);
    if (vte_term) {
        vte_terminal_feed_child(VTE_TERMINAL(vte_term), cmd, strlen(cmd));
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_bottom), 0);
    }
    g_free(cmd);
}

void on_debug_clicked(void) {
    char *cmd = g_strdup_printf("%s\n", cfg.cmd_debug);
    if (vte_debug) {
        vte_terminal_feed_child(VTE_TERMINAL(vte_debug), cmd, strlen(cmd));
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_bottom), 1);
    }
    g_free(cmd);
}

static gboolean is_comment_or_empty(const char *text) {
    char *trimmed = g_strstrip(g_strdup(text));
    if (strlen(trimmed) == 0) { g_free(trimmed); return TRUE; }
    if (g_str_has_prefix(trimmed, "//") || g_str_has_prefix(trimmed, "/*") || g_str_has_prefix(trimmed, "*/")) { g_free(trimmed); return TRUE; }
    g_free(trimmed);
    return FALSE;
}

void analyze_row(const char *clean_line, const char *word, analyze_problem *prob) {
    char **parts = g_strsplit(clean_line, ":", -1);
    if (parts && parts[0] != NULL && parts[1] != NULL) {
        strcpy(prob->file_name, g_strstrip(parts[0]));
        prob->line_num = atoi(parts[1]);

        char *pos = g_strrstr(clean_line, word);
        if (pos) {
            char *desc = strstr(pos, " ");
            if (desc) {
                strcpy(prob->description, g_strstrip(g_strdup(desc)));
            } else {
                strcpy(prob->description, g_strstrip(g_strdup(pos)));
            }

            if (g_strrstr(prob->description, "expected") || g_strrstr(prob->description, "expected")) {
                strcpy(prob->solution, "Chýba znak na konci predchádzajúceho príkazu.");
            } else if (g_strrstr(prob->description, "undeclared")) {
                strcpy(prob->solution, "Premenná nie je deklarovaná. Skontrolujte preklepy alebo typ.");
            } else if (g_strrstr(prob->description, "expected ')'")) {
                strcpy(prob->solution, "Skontrolujte uzatváranie okrúhlych zátvoriek.");
            }
        }
    }
    g_strfreev(parts);
}

void analyze(const char *error_line, analyze_problem *prob) {
    prob->line_num = -1;
    strcpy(prob->file_name, "");
    strcpy(prob->description, "Chyba kompilácie");
    strcpy(prob->solution, "Skontrolujte syntax.");

    if (g_strrstr(error_line, "error:") == NULL && g_strrstr(error_line, "warning:") == NULL) {
        return;
    }

    char **parts = g_strsplit(error_line, ":", -1);
    if (parts && parts[0] != NULL && parts[1] != NULL) {
        strcpy(prob->file_name, g_strstrip(parts[0]));
        prob->line_num = atoi(parts[1]);
        
        if (parts[2] != NULL && parts[3] != NULL) {
            if (atoi(parts[2]) > 0) {
                strcpy(prob->description, g_strstrip(parts[4] ? parts[4] : parts[3]));
            } else {
                strcpy(prob->description, g_strstrip(parts[3]));
            }
        }
    }
    g_strfreev(parts);
}

void parse_buffer_errors(GtkWidget *vte) {
    TabData *tab = get_current_tab_data();
    if (!tab || !tab->buffer) return;

    GtkTextIter s_start, s_end;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(tab->buffer), &s_start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(tab->buffer), &s_end);
    gtk_text_buffer_remove_tag_by_name(GTK_TEXT_BUFFER(tab->buffer), "error_line", &s_start, &s_end);

    char *full_text = NULL;
    GError *error = NULL;
    if (!g_file_get_contents("project.log", &full_text, NULL, &error)) {
        if (error) g_error_free(error);
        return;
    }

    char **lines = g_strsplit(full_text, "\n", -1);
    g_free(full_text);

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        if (!line || strlen(line) == 0) continue;

        analyze_problem prob;
        analyze(line, &prob);

        if (prob.line_num > 0) {
            int target_line = prob.line_num - 1;

            GtkTextIter start, end;
            gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(tab->buffer), &start, target_line);
            end = start;
            gtk_text_iter_forward_to_line_end(&end);

            GtkTextTag *err_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(GTK_TEXT_BUFFER(tab->buffer)), "error_line");
            gtk_text_buffer_apply_tag(GTK_TEXT_BUFFER(tab->buffer), err_tag, &start, &end);
            
            gtk_statusbar_pop(GTK_STATUSBAR(statusbar), 0);
            gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, g_strdup_printf("❌ Riadok %d: %s", target_line + 1, prob.description));
        }
    }
    g_strfreev(lines);
}
