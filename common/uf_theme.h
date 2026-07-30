/* Shared Fluent / Windows Media Creation Tool design system for GTK apps */
#ifndef UF_THEME_H
#define UF_THEME_H

#include <gtk/gtk.h>
#include <string.h>

static const char *UF_FLUENT_CSS =
    "@keyframes uf-fade-in {"
    "  from { opacity: 0; }"
    "  to   { opacity: 1; }"
    "}"
    "@keyframes uf-progress-glow {"
    "  0%   { background-color: #0078d4; }"
    "  50%  { background-color: #2b88d8; }"
    "  100% { background-color: #0078d4; }"
    "}"
    "@keyframes uf-pulse-dot {"
    "  0%, 100% { opacity: 1; }"
    "  50%      { opacity: 0.45; }"
    "}"
    "window, .uf-root {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "  font-family: 'Segoe UI', 'Segoe UI Variable', 'Cantarell', 'Ubuntu', sans-serif;"
    "  font-size: 14px;"
    "}"
    /* Product chrome */
    ".uf-header {"
    "  background-color: #0078d4;"
    "  color: #ffffff;"
    "  padding: 0 20px;"
    "  min-height: 48px;"
    "  border-bottom: 3px solid #005a9e;"
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
    /* Step rail under header */
    ".uf-steps {"
    "  background-color: #f8f8f8;"
    "  border-bottom: 1px solid #e5e5e5;"
    "  padding: 10px 40px;"
    "}"
    ".uf-step-item {"
    "  color: #a19f9d;"
    "  font-size: 12px;"
    "  font-weight: 600;"
    "  letter-spacing: 0.2px;"
    "}"
    ".uf-step-item.uf-step-on {"
    "  color: #0078d4;"
    "}"
    ".uf-step-item.uf-step-done {"
    "  color: #107c10;"
    "}"
    ".uf-step-sep {"
    "  color: #c8c6c4;"
    "  font-size: 12px;"
    "  padding: 0 10px;"
    "}"
    ".uf-step-dot {"
    "  color: #c8c6c4;"
    "  font-size: 11px;"
    "  margin-right: 6px;"
    "}"
    ".uf-step-dot.uf-step-on {"
    "  color: #0078d4;"
    "  animation: uf-pulse-dot 1.6s ease-in-out infinite;"
    "}"
    ".uf-step-dot.uf-step-done {"
    "  color: #107c10;"
    "  animation: none;"
    "}"
    /* Content */
    ".uf-content {"
    "  background-color: #ffffff;"
    "  padding: 28px 40px 20px 40px;"
    "}"
    ".uf-reveal {"
    "  animation: uf-fade-in 260ms ease-out;"
    "}"
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
    ".uf-warn {"
    "  color: #8a6116;"
    "  background-color: #fff4ce;"
    "  border: 1px solid #f0e2a4;"
    "  border-radius: 2px;"
    "  padding: 10px 12px;"
    "  font-size: 13px;"
    "}"
    ".uf-success-banner {"
    "  color: #0b6a0b;"
    "  background-color: #dff6dd;"
    "  border: 1px solid #9fd89f;"
    "  border-radius: 2px;"
    "  padding: 12px 14px;"
    "  font-size: 14px;"
    "  font-weight: 600;"
    "}"
    ".uf-error-banner {"
    "  color: #a4262c;"
    "  background-color: #fde7e9;"
    "  border: 1px solid #f1aeb5;"
    "  border-radius: 2px;"
    "  padding: 12px 14px;"
    "  font-size: 14px;"
    "  font-weight: 600;"
    "}"
    ".uf-status { color: #605e5c; font-size: 13px; }"
    "label { color: #1b1b1b; }"
    /* Inputs */
    "entry, combobox, combobox button.combo {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "  border: 1px solid #8a8886;"
    "  border-radius: 2px;"
    "  min-height: 32px;"
    "  padding: 4px 10px;"
    "  transition: border-color 120ms ease, background-color 120ms ease;"
    "}"
    "entry:focus, combobox:focus {"
    "  border-color: #0078d4;"
    "  border-width: 2px;"
    "}"
    "entry:hover, combobox:hover {"
    "  border-color: #323130;"
    "}"
    /* Buttons */
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
    "  transition: background-color 140ms ease, border-color 140ms ease, color 140ms ease;"
    "}"
    "button:hover {"
    "  background-color: #f5f5f5;"
    "  border-color: #323130;"
    "}"
    "button:active {"
    "  background-color: #edebe9;"
    "}"
    "button:disabled {"
    "  background-color: #f3f3f3;"
    "  color: #a19f9d;"
    "  border-color: #e1dfdd;"
    "}"
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
    "button.uf-primary:active {"
    "  background-color: #005a9e;"
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
    /* Option tiles */
    ".uf-option {"
    "  background-color: #ffffff;"
    "  border: 1px solid #e1e1e1;"
    "  border-radius: 2px;"
    "  padding: 14px 16px;"
    "  margin: 6px 0;"
    "  transition: background-color 160ms ease, border-color 160ms ease;"
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
    "  transition: background-color 160ms ease, border-color 160ms ease;"
    "}"
    "button.uf-choice:hover {"
    "  background-color: #f3f9fd;"
    "  border-color: #0078d4;"
    "}"
    "button.uf-choice:active {"
    "  background-color: #deecf9;"
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
    "  transition: background-color 120ms ease, border-color 120ms ease;"
    "}"
    "radiobutton:checked indicator {"
    "  background-color: #0078d4;"
    "  border-color: #0078d4;"
    "  color: #ffffff;"
    "}"
    /* Surfaces */
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
    "  min-height: 6px;"
    "}"
    "progressbar progress {"
    "  background-color: #0078d4;"
    "  border-radius: 0;"
    "  min-height: 6px;"
    "  transition: all 180ms ease;"
    "}"
    "progressbar.uf-busy progress {"
    "  animation: uf-progress-glow 1.4s ease-in-out infinite;"
    "}"
    "progressbar.uf-done progress {"
    "  background-color: #107c10;"
    "  animation: none;"
    "}"
    "progressbar.uf-fail progress {"
    "  background-color: #d13438;"
    "  animation: none;"
    "}"
    "treeview {"
    "  background-color: #ffffff;"
    "  color: #1b1b1b;"
    "}"
    "treeview:selected { background-color: #c7e0f4; color: #1b1b1b; }"
    ".uf-progress-hero {"
    "  font-size: 16px;"
    "  color: #1b1b1b;"
    "  font-weight: 600;"
    "}"
    ".uf-spinner-label {"
    "  color: #0078d4;"
    "  font-size: 13px;"
    "  font-weight: 600;"
    "  animation: uf-pulse-dot 1.2s ease-in-out infinite;"
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

static void uf_reveal(GtkWidget *w)
{
    GtkStyleContext *ctx;
    if (!w)
        return;
    ctx = gtk_widget_get_style_context(w);
    gtk_style_context_remove_class(ctx, "uf-reveal");
    /* Force restart of CSS animation */
    gtk_widget_hide(w);
    gtk_style_context_add_class(ctx, "uf-reveal");
    gtk_widget_show(w);
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

static GtkWidget *__attribute__((unused)) uf_header_bar(const char *title)
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

static GtkWidget *__attribute__((unused)) uf_header_bar_versioned(const char *title, const char *version)
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

static GtkWidget *__attribute__((unused)) uf_eyebrow_label(const char *text)
{
    GtkWidget *lbl = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "uf-eyebrow");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    return lbl;
}

typedef struct {
    GtkWidget *bar;
    GtkWidget *items[8];
    GtkWidget *dots[8];
    int count;
    int active;
} UfStepRail;

static GtkWidget *uf_step_rail_new(UfStepRail *rail, const char **labels, int count)
{
    int i;
    memset(rail, 0, sizeof(*rail));
    rail->bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(rail->bar), "uf-steps");
    rail->count = count > 8 ? 8 : count;

    for (i = 0; i < rail->count; i++) {
        GtkWidget *cell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *dot = gtk_label_new("●");
        GtkWidget *lbl = gtk_label_new(labels[i]);

        gtk_style_context_add_class(gtk_widget_get_style_context(dot), "uf-step-dot");
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "uf-step-item");
        gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);

        gtk_box_pack_start(GTK_BOX(cell), dot, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(cell), lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(rail->bar), cell, FALSE, FALSE, 0);

        if (i + 1 < rail->count) {
            GtkWidget *sep = gtk_label_new("——");
            gtk_style_context_add_class(gtk_widget_get_style_context(sep), "uf-step-sep");
            gtk_box_pack_start(GTK_BOX(rail->bar), sep, FALSE, FALSE, 0);
        }

        rail->items[i] = lbl;
        rail->dots[i] = dot;
    }
    rail->active = -1;
    return rail->bar;
}

static void uf_step_rail_set(UfStepRail *rail, int active)
{
    int i;
    if (!rail || !rail->bar)
        return;
    rail->active = active;
    for (i = 0; i < rail->count; i++) {
        GtkStyleContext *lc = gtk_widget_get_style_context(rail->items[i]);
        GtkStyleContext *dc = gtk_widget_get_style_context(rail->dots[i]);
        gtk_style_context_remove_class(lc, "uf-step-on");
        gtk_style_context_remove_class(lc, "uf-step-done");
        gtk_style_context_remove_class(dc, "uf-step-on");
        gtk_style_context_remove_class(dc, "uf-step-done");
        if (i < active) {
            gtk_style_context_add_class(lc, "uf-step-done");
            gtk_style_context_add_class(dc, "uf-step-done");
        } else if (i == active) {
            gtk_style_context_add_class(lc, "uf-step-on");
            gtk_style_context_add_class(dc, "uf-step-on");
        }
    }
}

/* MCT-style option row */
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
