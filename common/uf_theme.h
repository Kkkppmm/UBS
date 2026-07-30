/* Shared Fluent / Windows Media Creation Tool visual tokens for GTK apps */
#ifndef UF_THEME_H
#define UF_THEME_H

static const char *UF_FLUENT_CSS =
    "window, .uf-root {"
    "  background-color: #f3f3f3;"
    "  color: #1a1a1a;"
    "  font-family: 'Segoe UI', 'Cantarell', 'Ubuntu', sans-serif;"
    "  font-size: 14px;"
    "}"
    ".uf-header {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  padding: 14px 24px;"
    "  min-height: 52px;"
    "}"
    ".uf-header label {"
    "  color: #ffffff;"
    "  font-weight: 600;"
    "  font-size: 16px;"
    "}"
    ".uf-content {"
    "  background-color: #ffffff;"
    "  padding: 28px 36px;"
    "}"
    ".uf-footer {"
    "  background-color: #f9f9f9;"
    "  border-top: 1px solid #e1e1e1;"
    "  padding: 12px 24px;"
    "}"
    ".uf-title {"
    "  color: #1a1a1a;"
    "  font-size: 22px;"
    "  font-weight: 600;"
    "}"
    ".uf-subtitle {"
    "  color: #605e5c;"
    "  font-size: 13px;"
    "}"
    ".uf-step {"
    "  color: #0078d4;"
    "  font-size: 12px;"
    "  font-weight: 600;"
    "  letter-spacing: 0.4px;"
    "}"
    "label {"
    "  color: #1a1a1a;"
    "}"
    "entry, combobox, combobox button.combo {"
    "  background-color: #ffffff;"
    "  color: #1a1a1a;"
    "  border: 1px solid #8a8886;"
    "  border-radius: 2px;"
    "  min-height: 32px;"
    "  padding: 4px 8px;"
    "}"
    "entry:focus {"
    "  border-color: #0078d4;"
    "}"
    "button {"
    "  background-image: none;"
    "  background-color: #ffffff;"
    "  color: #1a1a1a;"
    "  border: 1px solid #8a8886;"
    "  border-radius: 2px;"
    "  padding: 8px 18px;"
    "  font-weight: 600;"
    "  min-height: 32px;"
    "}"
    "button:hover {"
    "  background-color: #f5f5f5;"
    "  border-color: #323130;"
    "}"
    "button.uf-primary {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  border-color: #0078d4;"
    "}"
    "button.uf-primary:hover {"
    "  background-color: #106ebe;"
    "  border-color: #106ebe;"
    "  color: #ffffff;"
    "}"
    "button.uf-accent {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  border: none;"
    "  padding: 12px 20px;"
    "}"
    "button.uf-accent:hover { background-color: #106ebe; color: #ffffff; }"
    "button.uf-danger {"
    "  background-color: #d13438;"
    "  color: #ffffff;"
    "  border-color: #d13438;"
    "}"
    "button.uf-danger:hover { background-color: #a4262c; color: #ffffff; }"
    "button.uf-choice {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 4px;"
    "  padding: 16px 18px;"
    "  min-height: 64px;"
    "}"
    "button.uf-choice:hover {"
    "  background-color: #f3f9fd;"
    "  border-color: #0078d4;"
    "}"
    "radiobutton {"
    "  padding: 10px 4px;"
    "  color: #1a1a1a;"
    "}"
    "radiobutton:checked label { color: #0078d4; font-weight: 600; }"
    "frame {"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "  background-color: #fafafa;"
    "}"
    "frame > label { color: #605e5c; }"
    "textview, textview text {"
    "  background-color: #fafafa;"
    "  color: #323130;"
    "  font-family: 'Consolas', 'Ubuntu Mono', monospace;"
    "  font-size: 12px;"
    "}"
    "scrolledwindow {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "}"
    "progressbar trough {"
    "  background-color: #edebe9;"
    "  border-radius: 2px;"
    "  min-height: 6px;"
    "}"
    "progressbar progress {"
    "  background-color: #0078d4;"
    "  border-radius: 2px;"
    "  min-height: 6px;"
    "}"
    "treeview {"
    "  background-color: #ffffff;"
    "  color: #1a1a1a;"
    "}"
    "treeview:selected { background-color: #c7e0f4; color: #1a1a1a; }"
    ".uf-status { color: #605e5c; font-size: 12px; }";

static void uf_apply_fluent_theme(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, UF_FLUENT_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GtkWidget *uf_primary_button(const char *label)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "uf-primary");
    return b;
}

static GtkWidget *uf_header_bar(const char *title)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "uf-header");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);
    return box;
}

#endif
