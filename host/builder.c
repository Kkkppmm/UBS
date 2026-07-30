/*
 * USBForge Builder - host GUI to create bootable ISOs and write them to USB.
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>

#include "usbforge.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *status;
    GtkWidget *progress;
    GtkWidget *log_view;
    GtkTextBuffer *log_buf;
    GtkWidget *iso_entry;
    GtkWidget *usb_combo;
    GtkWidget *help_view;
    GtkTextBuffer *help_buf;
    GtkListStore *usb_store;
    UsbDeviceList devices;
    int busy;
} App;

static App app;

static void append_log(const char *msg)
{
    GtkTextIter end;
    gchar *line;

    if (!app.log_buf || !msg)
        return;

    line = g_strdup_printf("%s\n", msg);
    gtk_text_buffer_get_end_iter(app.log_buf, &end);
    gtk_text_buffer_insert(app.log_buf, &end, line, -1);
    g_free(line);

    if (app.status)
        gtk_label_set_text(GTK_LABEL(app.status), msg);
}

static void set_busy(int busy)
{
    app.busy = busy;
    if (app.progress)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress), busy ? 0.05 : 0.0);
}

static void load_help_topic(const char *topic)
{
    char path[USBFORGE_MAX_PATH];
    char *content = NULL;
    gsize len = 0;
    const char *docs;

    docs = uf_docs_path(NULL);
    if (topic && *topic)
        snprintf(path, sizeof(path), "%s/%s", docs, topic);
    else
        snprintf(path, sizeof(path), "%s/index.md", docs);

    if (g_file_get_contents(path, &content, &len, NULL)) {
        gtk_text_buffer_set_text(app.help_buf, content, (gint)len);
        g_free(content);
    } else {
        const char *fallback =
            "USBForge Help\n"
            "=============\n\n"
            "Welcome! Use the Builder tab to create a bootable ISO,\n"
            "or write an ISO to a USB drive.\n\n"
            "Topics:\n"
            "  * Getting Started\n"
            "  * Creating an ISO\n"
            "  * Writing to USB\n"
            "  * Live Session (USB Lab)\n"
            "  * Safety tips\n\n"
            "Place docs in the docs/ folder next to the binary for full help.";
        gtk_text_buffer_set_text(app.help_buf, fallback, -1);
    }
}

static void refresh_usb_list(void)
{
    int i;
    GtkTreeIter iter;

    gtk_list_store_clear(app.usb_store);
    uf_scan_usb_devices(&app.devices);

    for (i = 0; i < app.devices.count; i++) {
        UsbDevice *d = &app.devices.devices[i];
        gchar *label;

        if (!d->is_usb && !d->removable)
            continue;

        label = g_strdup_printf("%s - %s (%s) [%s]",
                                d->path, d->model, d->size, d->transport);
        gtk_list_store_append(app.usb_store, &iter);
        gtk_list_store_set(app.usb_store, &iter, 0, label, 1, d->path, -1);
        g_free(label);
    }

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app.usb_store), &iter))
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.usb_combo), 0);

    append_log("USB device list refreshed.");
}

static gboolean pulse_progress(gpointer data)
{
    (void)data;
    if (!app.busy)
        return G_SOURCE_REMOVE;
    if (app.progress)
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(app.progress));
    return G_SOURCE_CONTINUE;
}

typedef struct {
    char cmd[USBFORGE_MAX_PATH * 2];
    char success_msg[256];
    char fail_msg[256];
} Job;

typedef struct {
    Job *job;
    int rc;
    char *output;
} JobResult;

static gboolean on_job_done(gpointer data)
{
    JobResult *r = data;
    if (r->output && r->output[0]) {
        append_log("--- command output ---");
        append_log(r->output);
        append_log("--- end output ---");
    }
    if (r->rc == 0) {
        append_log(r->job->success_msg);
        if (app.progress)
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress), 1.0);
    } else {
        append_log(r->job->fail_msg);
        if (app.progress)
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress), 0.0);
    }
    set_busy(0);
    g_free(r->output);
    g_free(r->job);
    g_free(r);
    return G_SOURCE_REMOVE;
}

static gpointer job_thread_portable(gpointer data)
{
    Job *job = data;
    JobResult *r = g_new0(JobResult, 1);
    char out[USBFORGE_MAX_LOG];
    int status;

    out[0] = '\0';
    status = uf_run_cmd(job->cmd, out, sizeof(out));
    r->job = job;
    r->rc = (status == 0) ? 0 : 1;
    r->output = g_strdup(out);
    g_idle_add(on_job_done, r);
    return NULL;
}

static void start_job(const char *cmd, const char *ok, const char *fail)
{
    Job *job;
    pthread_t thr;

    if (app.busy) {
        append_log("A job is already running. Please wait.");
        return;
    }

    job = g_new0(Job, 1);
    snprintf(job->cmd, sizeof(job->cmd), "%s", cmd);
    snprintf(job->success_msg, sizeof(job->success_msg), "%s", ok);
    snprintf(job->fail_msg, sizeof(job->fail_msg), "%s", fail);

    set_busy(1);
    append_log("Starting...");
    append_log(cmd);
    g_timeout_add(120, pulse_progress, NULL);
    pthread_create(&thr, NULL, job_thread_portable, job);
    pthread_detach(thr);
}

static void on_browse_iso(GtkButton *btn, gpointer user_data)
{
    GtkWidget *dialog;
    gint res;
    (void)btn;
    (void)user_data;

    dialog = gtk_file_chooser_dialog_new(
        "Choose ISO output location",
        GTK_WINDOW(app.window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Select", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "usbforge.iso");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(app.iso_entry), file);
        g_free(file);
    }
    gtk_widget_destroy(dialog);
}

static void on_browse_existing_iso(GtkButton *btn, gpointer user_data)
{
    GtkWidget *dialog;
    gint res;
    (void)btn;
    (void)user_data;

    dialog = gtk_file_chooser_dialog_new(
        "Open existing ISO",
        GTK_WINDOW(app.window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    {
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "ISO images");
        gtk_file_filter_add_pattern(filter, "*.iso");
        gtk_file_filter_add_pattern(filter, "*.ISO");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(app.iso_entry), file);
        g_free(file);
    }
    gtk_widget_destroy(dialog);
}

static char *selected_usb_path(void)
{
    GtkTreeIter iter;
    gchar *path = NULL;

    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(app.usb_combo), &iter))
        return NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(app.usb_store), &iter, 1, &path, -1);
    return path;
}

static void on_build_iso(GtkButton *btn, gpointer user_data)
{
    const char *iso;
    char cmd[USBFORGE_MAX_PATH * 3];
    (void)btn;
    (void)user_data;

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    if (!iso || !*iso) {
        append_log("Please choose an output ISO path first.");
        return;
    }

    /* Prefer project script; fall back to packaged location */
    if (uf_file_exists("scripts/build-iso.sh"))
        snprintf(cmd, sizeof(cmd), "bash scripts/build-iso.sh \"%s\"", iso);
    else if (uf_file_exists("/usbforge/scripts/build-iso.sh"))
        snprintf(cmd, sizeof(cmd), "bash /usbforge/scripts/build-iso.sh \"%s\"", iso);
    else
        snprintf(cmd, sizeof(cmd), "bash scripts/build-iso.sh \"%s\"", iso);

    start_job(cmd, "ISO built successfully.", "ISO build failed. Check the log and dependencies (xorriso, grub).");
}

static void on_write_usb(GtkButton *btn, gpointer user_data)
{
    const char *iso;
    char *usb;
    char cmd[USBFORGE_MAX_PATH * 3];
    GtkWidget *confirm;
    gint res;
    (void)btn;
    (void)user_data;

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    usb = selected_usb_path();

    if (!iso || !*iso || !uf_file_exists(iso)) {
        append_log("Choose a valid ISO file before writing.");
        g_free(usb);
        return;
    }
    if (!usb) {
        append_log("Select a USB device first.");
        return;
    }

    confirm = gtk_message_dialog_new(
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK_CANCEL,
        "This will ERASE all data on %s.\n\nWrite %s to that drive?",
        usb, iso);
    res = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);

    if (res != GTK_RESPONSE_OK) {
        append_log("Write cancelled.");
        g_free(usb);
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "pkexec dd if=\"%s\" of=\"%s\" bs=4M status=progress conv=fsync oflag=direct",
             iso, usb);
    start_job(cmd, "USB write completed. You can now boot from this drive.",
              "USB write failed. You may need administrator rights (pkexec/sudo).");
    g_free(usb);
}

static void on_verify(GtkButton *btn, gpointer user_data)
{
    const char *iso;
    char cmd[USBFORGE_MAX_PATH * 2];
    (void)btn;
    (void)user_data;

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    if (!iso || !*iso || !uf_file_exists(iso)) {
        append_log("Choose an existing ISO to verify.");
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "xorriso -indev \"%s\" -find / -exec report_lba -- 2>&1 | head -n 40",
             iso);
    start_job(cmd, "ISO verification finished (table of contents listed in log above).",
              "Could not verify ISO. Is xorriso installed?");
}

static void on_refresh(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    refresh_usb_list();
}

static void on_help_topic(GtkButton *btn, gpointer user_data)
{
    const char *topic = user_data;
    (void)btn;
    load_help_topic(topic);
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "help");
}

static void on_show_page(GtkButton *btn, gpointer user_data)
{
    const char *name = user_data;
    (void)btn;
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), name);
}

static GtkWidget *make_nav_button(const char *label, const char *page, GCallback cb)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_widget_set_margin_top(b, 4);
    gtk_widget_set_margin_bottom(b, 4);
    gtk_widget_set_margin_start(b, 8);
    gtk_widget_set_margin_end(b, 8);
    g_signal_connect(b, "clicked", cb, (gpointer)page);
    return b;
}

static GtkWidget *build_welcome_page(void)
{
    GtkWidget *box, *title, *sub, *hint, *start;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 48);
    gtk_widget_set_margin_bottom(box, 48);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>USBForge</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);

    sub = gtk_label_new(USBFORGE_TAGLINE);
    gtk_widget_set_halign(sub, GTK_ALIGN_CENTER);

    hint = gtk_label_new(
        "Create a bootable ISO, write it to USB, then boot into the USB Lab:\n"
        "test devices, read docs, run diagnostics - not just install.");
    gtk_label_set_justify(GTK_LABEL(hint), GTK_JUSTIFY_CENTER);
    gtk_widget_set_halign(hint, GTK_ALIGN_CENTER);

    start = gtk_button_new_with_label("Open Builder");
    gtk_widget_set_halign(start, GTK_ALIGN_CENTER);
    g_signal_connect(start, "clicked", G_CALLBACK(on_show_page), (gpointer)"builder");

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box), start, FALSE, FALSE, 16);

    return box;
}

static GtkWidget *build_builder_page(void)
{
    GtkWidget *box, *grid, *lbl, *browse, *open_iso, *row;
    GtkWidget *build_btn, *write_btn, *verify_btn, *refresh_btn;
    GtkWidget *scrolled, *frame;
    GtkCellRenderer *renderer;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);

    lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), "<span size='large' weight='bold'>Build &amp; Write</span>");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ISO path:"), 0, 0, 1, 1);
    app.iso_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.iso_entry), "usbforge.iso");
    gtk_entry_set_text(GTK_ENTRY(app.iso_entry), "build/usbforge.iso");
    gtk_widget_set_hexpand(app.iso_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), app.iso_entry, 1, 0, 1, 1);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    browse = gtk_button_new_with_label("Save as...");
    open_iso = gtk_button_new_with_label("Open ISO...");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_iso), NULL);
    g_signal_connect(open_iso, "clicked", G_CALLBACK(on_browse_existing_iso), NULL);
    gtk_box_pack_start(GTK_BOX(row), browse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), open_iso, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), row, 2, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("USB drive:"), 0, 1, 1, 1);
    app.usb_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    app.usb_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(app.usb_store));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app.usb_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(app.usb_combo), renderer, "text", 0);
    gtk_widget_set_hexpand(app.usb_combo, TRUE);
    gtk_grid_attach(GTK_GRID(grid), app.usb_combo, 1, 1, 1, 1);

    refresh_btn = gtk_button_new_with_label("Refresh");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh), NULL);
    gtk_grid_attach(GTK_GRID(grid), refresh_btn, 2, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    build_btn = gtk_button_new_with_label("Build Bootable ISO");
    write_btn = gtk_button_new_with_label("Write ISO -> USB");
    verify_btn = gtk_button_new_with_label("Verify ISO");
    g_signal_connect(build_btn, "clicked", G_CALLBACK(on_build_iso), NULL);
    g_signal_connect(write_btn, "clicked", G_CALLBACK(on_write_usb), NULL);
    g_signal_connect(verify_btn, "clicked", G_CALLBACK(on_verify), NULL);
    gtk_box_pack_start(GTK_BOX(row), build_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), write_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), verify_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);

    app.progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app.progress), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress), "Ready");
    gtk_box_pack_start(GTK_BOX(box), app.progress, FALSE, FALSE, 0);

    frame = gtk_frame_new("Feedback / Log");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    app.log_view = gtk_text_view_new();
    app.log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.log_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.log_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), app.log_view);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);
    gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);

    return box;
}

static GtkWidget *build_help_page(void)
{
    GtkWidget *box, *topics, *scrolled, *lbl;
    GtkWidget *b1, *b2, *b3, *b4, *b5;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);

    lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), "<span size='large' weight='bold'>Help &amp; Docs</span>");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    topics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    b1 = gtk_button_new_with_label("Getting Started");
    b2 = gtk_button_new_with_label("Create ISO");
    b3 = gtk_button_new_with_label("Write USB");
    b4 = gtk_button_new_with_label("Live USB Lab");
    b5 = gtk_button_new_with_label("Safety");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_help_topic), (gpointer)"getting-started.md");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_help_topic), (gpointer)"create-iso.md");
    g_signal_connect(b3, "clicked", G_CALLBACK(on_help_topic), (gpointer)"write-usb.md");
    g_signal_connect(b4, "clicked", G_CALLBACK(on_help_topic), (gpointer)"live-lab.md");
    g_signal_connect(b5, "clicked", G_CALLBACK(on_help_topic), (gpointer)"safety.md");
    gtk_box_pack_start(GTK_BOX(topics), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b4, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b5, FALSE, FALSE, 0);
    {
        GtkWidget *b6 = gtk_button_new_with_label("Packages");
        g_signal_connect(b6, "clicked", G_CALLBACK(on_help_topic), (gpointer)"packages.md");
        gtk_box_pack_start(GTK_BOX(topics), b6, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(box), topics, FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    app.help_view = gtk_text_view_new();
    app.help_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.help_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.help_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.help_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), app.help_view);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

    load_help_topic("index.md");
    return box;
}

static void apply_css(void)
{
    GtkCssProvider *provider;
    const char *css =
        "window {"
        "  background: linear-gradient(160deg, #0f2744 0%, #163a5f 45%, #1a4a6e 100%);"
        "}"
        "label, textview, textview text {"
        "  color: #e8f1fa;"
        "  font-family: 'IBM Plex Sans', 'Source Sans 3', 'Segoe UI', sans-serif;"
        "}"
        "entry, combobox, frame, scrolledwindow {"
        "  background-color: rgba(8, 28, 48, 0.72);"
        "  color: #e8f1fa;"
        "  border-radius: 6px;"
        "}"
        "button {"
        "  background-image: none;"
        "  background-color: #2a6f97;"
        "  color: #f4faff;"
        "  border-radius: 6px;"
        "  padding: 8px 14px;"
        "  border: none;"
        "  font-weight: 600;"
        "}"
        "button:hover {"
        "  background-color: #3a8bb8;"
        "}"
        "frame > label {"
        "  color: #9ec9e6;"
        "}"
        "progressbar trough {"
        "  background-color: rgba(255,255,255,0.12);"
        "  border-radius: 4px;"
        "}"
        "progressbar progress {"
        "  background-color: #3dbb8f;"
        "  border-radius: 4px;"
        "}";

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void activate(GtkApplication *gtk_app, gpointer user_data)
{
    GtkWidget *outer, *nav, *content;
    GtkWidget *brand;
    (void)user_data;

    memset(&app, 0, sizeof(app));

    app.window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app.window), "USBForge - Bootable ISO Builder");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 960, 640);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);

    apply_css();

    outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    nav = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(nav, 180, -1);
    gtk_widget_set_margin_top(nav, 12);
    brand = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(brand),
        "<span size='large' weight='bold'>USBForge</span>\n"
        "<span size='small'>v" USBFORGE_VERSION "</span>");
    gtk_widget_set_margin_bottom(brand, 16);
    gtk_widget_set_margin_start(brand, 8);
    gtk_box_pack_start(GTK_BOX(nav), brand, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav),
        make_nav_button("Home", "welcome", G_CALLBACK(on_show_page)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav),
        make_nav_button("Builder", "builder", G_CALLBACK(on_show_page)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav),
        make_nav_button("Help & Docs", "help", G_CALLBACK(on_show_page)), FALSE, FALSE, 0);

    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_add_named(GTK_STACK(app.stack), build_welcome_page(), "welcome");
    gtk_stack_add_named(GTK_STACK(app.stack), build_builder_page(), "builder");
    gtk_stack_add_named(GTK_STACK(app.stack), build_help_page(), "help");
    gtk_box_pack_start(GTK_BOX(content), app.stack, TRUE, TRUE, 0);

    app.status = gtk_label_new("Ready - choose Builder to create your ISO.");
    gtk_widget_set_margin_top(app.status, 4);
    gtk_widget_set_margin_bottom(app.status, 8);
    gtk_widget_set_margin_start(app.status, 12);
    gtk_widget_set_halign(app.status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), app.status, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer), nav, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), content, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(app.window), outer);

    refresh_usb_list();
    append_log("USBForge Builder ready.");
    gtk_widget_show_all(app.window);
}

int main(int argc, char **argv)
{
    GtkApplication *gtk_app;
    int status;

    gtk_app = gtk_application_new("org.usbforge.builder", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}
