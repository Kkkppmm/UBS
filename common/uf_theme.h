/* Shared Fluent / Windows Media Creation Tool visual tokens for GTK apps */
#ifndef UF_THEME_H
#define UF_THEME_H

#include <gtk/gtk.h>

static const char *UF_FLUENT_CSS =
    "window, .uf-root {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "  font-family: 'Segoe UI', 'Segoe UI Variable', 'Cantarell', 'Ubuntu', sans-serif;"
    "  font-size: 14px;"
    "}"
    /* Top product chrome — solid Windows blue like MCT / Setup */
    ".uf-header {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  padding: 0 20px;"
    "  min-height: 48px;"
    "}"
    ".uf-header label {"
    "  color: #ffffff;"
    "  font-weight: 600;"
    "  font-size: 15px;"
    "  letter-spacing: 0.1px;"
    "}"
    ".uf-header-version {"
    "  color: rgba(255,255,255,0.85);"
    "  font-size: 12px;"
    "  font-weight: 400;"
    "}"
    /* Main content plane */
    ".uf-content {"
    "  background-color: #ffffff;"
    "  padding: 28px 40px 20px 40px;"
    "}"
    /* Footer chrome — light grey strip with hairline */
    ".uf-footer {"
    "  background-color: #f3f3f3;"
    "  border-top: 1px solid #e5e5e5;"
    "  padding: 14px 20px;"
    "  min-height: 60px;"
    "}"
    ".uf-eyebrow {"
    "  color: #0078d4;"
    "  font-size: 12px;"
    "  font-weight: 600;"
    "  letter-spacing: 0.6px;"
    "}"
    ".uf-title {"
    "  color: #1b1b1b;"
    "  font-size: 28px;"
    "  font-weight: 600;"
    "  letter-spacing: -0.2px;"
    "}"
    ".uf-subtitle {"
    "  color: #605e5c;"
    "  font-size: 14px;"
    "}"
    ".uf-field-label {"
    "  color: #1b1b1b;"
    "  font-size: 13px;"
    "  font-weight: 600;"
    "}"
    ".uf-link {"
    "  color: #0078d4;"
    "  font-size: 13px;"
    "}"
    ".uf-status { color: #605e5c; font-size: 13px; }"
    "label { color: #1b1b1b; }"
    /* Inputs — Fluent outline */
    "entry, combobox, combobox button.combo {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "  border: 1px solid #8a8886;"
    "  border-radius: 2px;"
    "  min-height: 32px;"
    "  padding: 4px 10px;"
    "  box-shadow: none;"
    "}"
    "entry:focus, combobox:focus {"
    "  border-color: #0078d4;"
    "  border-width: 2px;"
    "}"
    /* Default / secondary buttons */
    "button {"
    "  background-image: none;"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "  border: 1px solid #8a8886;"
    "  border-radius: 2px;"
    "  padding: 6px 20px;"
    "  font-weight: 600;"
    "  font-size: 14px;"
    "  min-height: 32px;"
    "  min-width: 88px;"
    "  box-shadow: none;"
    "}"
    "button:hover {"
    "  background-color: #f5f5f5;"
    "  border-color: #323130;"
    "}"
    "button:disabled {"
    "  background-color: #f3f3f3;"
    "  color: #a19f9d;"
    "  border-color: #e1dfdd;"
    "}"
    /* Primary action (Next / Accept / Create) */
    "button.uf-primary {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  border-color: #0078d4;"
    "  min-width: 96px;"
    "}"
    "button.uf-primary:hover {"
    "  background-color: #106ebe;"
    "  border-color: #106ebe;"
    "  color: #ffffff;"
    "}"
    "button.uf-primary:disabled {"
    "  background-color: #f3f3f3;"
    "  color: #a19f9d;"
    "  border-color: #e1dfdd;"
    "}"
    "button.uf-danger {"
    "  background-color: #d13438;"
    "  color: #ffffff;"
    "  border-color: #d13438;"
    "}"
    "button.uf-danger:hover { background-color: #a4262c; color: #ffffff; }"
    /* Quiet text-style actions in content */
    "button.uf-linkbtn {"
    "  background-color: transparent;"
    "  border: none;"
    "  color: #0078d4;"
    "  font-weight: 400;"
    "  min-width: 0;"
    "  padding: 4px 2px;"
    "}"
    "button.uf-linkbtn:hover {"
    "  background-color: transparent;"
    "  color: #004578;"
    "  text-decoration: underline;"
    "}"
    /* MCT-style selectable option tiles */
    ".uf-option {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "  padding: 14px 16px;"
    "  margin: 6px 0;"
    "}"
    ".uf-option:hover {"
    "  background-color: #f3f9fd;"
    "  border-color: #c7e0f4;"
    "}"
    ".uf-option.uf-option-on {"
    "  background-color: #f3f9fd;"
    "  border-color: #0078d4;"
    "  border-width: 2px;"
    "  padding: 13px 15px;"
    "}"
    ".uf-option-title {"
    "  color: #1b1b1b;"
    "  font-size: 15px;"
    "  font-weight: 600;"
    "}"
    ".uf-option-desc {"
    "  color: #605e5c;"
    "  font-size: 13px;"
    "}"
    "button.uf-choice {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "  padding: 16px 18px;"
    "  min-height: 64px;"
    "  font-weight: 600;"
    "}"
    "button.uf-choice:hover {"
    "  background-color: #f3f9fd;"
    "  border-color: #0078d4;"
    "}"
    "radiobutton {"
    "  padding: 2px 0;"
    "  color: #1b1b1b;"
    "}"
    "radiobutton indicator {"
    "  border-color: #605e5c;"
    "  background-color: #ffffff;"
    "  min-width: 18px;"
    "  min-height: 18px;"
    "}"
    "radiobutton:checked indicator {"
    "  background-color: #0078d4;"
    "  border-color: #0078d4;"
    "  color: #ffffff;"
    "}"
    /* License / notice box */
    ".uf-license {"
    "  background-color: #faf9f8;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "}"
    ".uf-license textview, .uf-license textview text {"
    "  background-color: #faf9f8;"
    "  color: #323130;"
    "  font-family: 'Segoe UI', 'Cantarell', sans-serif;"
    "  font-size: 13px;"
    "  padding: 10px;"
    "}"
    "frame {"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "  background-color: #faf9f8;"
    "}"
    "frame > label { color: #605e5c; font-size: 12px; }"
    "textview, textview text {"
    "  background-color: #faf9f8;"
    "  color: #323130;"
    "  font-family: 'Consolas', 'Cascadia Mono', 'Ubuntu Mono', monospace;"
    "  font-size: 12px;"
    "}"
    "scrolledwindow {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "}"
    "progressbar trough {"
    "  background-color: #edebe9;"
    "  border: none;"
    "  border-radius: 0;"
    "  min-height: 4px;"
    "}"
    "progressbar progress {"
    "  background-color: #0078d4;"
    "  border-radius: 0;"
    "  min-height: 4px;"
    "}"
    "treeview {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "}"
    "treeview:selected { background-color: #c7e0f4; color: #1b1b1b; }"
    ".uf-progress-hero {"
    "  font-size: 16px;"
    "  color: #1b1b1b;"
    "}";

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
    gtk_widget_set_size_request(b, 96, 32);
    return b;
}

static GtkWidget *uf_secondary_button(const char *label)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(b, 88, 32);
    return b;
}

static GtkWidget *uf_link_button(const char *label)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "uf-linkbtn");
    return b;
}

static GtkWidget *uf_header_bar(const char *title)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "uf-header");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);
    return box;
}

static GtkWidget *uf_header_bar_versioned(const char *title, const char *version)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *lbl = gtk_label_new(title);
    GtkWidget *ver = gtk_label_new(version);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "uf-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(ver), "uf-header-version");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(ver, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(box), ver, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *uf_title_label(const char *text)
{
    GtkWidget *lbl = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "uf-title");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
    return lbl;
}

static GtkWidget *uf_subtitle_label(const char *text)
{
    GtkWidget *lbl = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "uf-subtitle");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
    return lbl;
}

static GtkWidget *uf_eyebrow_label(const char *text)
{
    GtkWidget *lbl = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "uf-eyebrow");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    return lbl;
}

/* MCT-style option row: radio + bold title + muted description inside a tile */
static GtkWidget *uf_option_tile(GtkRadioButton *group,
                                 const char *title,
                                 const char *desc,
                                 GtkWidget **radio_out)
{
    GtkWidget *event = gtk_event_box_new();
    GtkWidget *tile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *radio;
    GtkWidget *t, *d;

    gtk_style_context_add_class(gtk_widget_get_style_context(event), "uf-option");
    gtk_widget_set_hexpand(event, TRUE);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event), TRUE);

    if (group)
        radio = gtk_radio_button_new_from_widget(group);
    else
        radio = gtk_radio_button_new(NULL);
    gtk_widget_set_valign(radio, GTK_ALIGN_START);
    gtk_widget_set_margin_top(radio, 2);

    t = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(t), "uf-option-title");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_label_set_line_wrap(GTK_LABEL(t), TRUE);

    d = gtk_label_new(desc);
    gtk_style_context_add_class(gtk_widget_get_style_context(d), "uf-option-desc");
    gtk_label_set_xalign(GTK_LABEL(d), 0);
    gtk_label_set_line_wrap(GTK_LABEL(d), TRUE);

    gtk_box_pack_start(GTK_BOX(texts), t, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(texts), d, FALSE, FALSE, 0);
    gtk_widget_set_hexpand(texts, TRUE);

    gtk_box_pack_start(GTK_BOX(tile), radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tile), texts, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(event), tile);

    if (radio_out)
        *radio_out = radio;
    return event;
}

static void uf_option_set_active(GtkWidget *tile, gboolean on)
{
    GtkStyleContext *ctx;
    if (!tile)
        return;
    ctx = gtk_widget_get_style_context(tile);
    if (on)
        gtk_style_context_add_class(ctx, "uf-option-on");
    else
        gtk_style_context_remove_class(ctx, "uf-option-on");
}

#endif
