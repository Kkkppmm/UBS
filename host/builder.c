/*
 * USBForge Builder — Windows Media Creation Tool style wizard (GTK3).
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
#include "uf_theme.h"

typedef enum {
    MODE_CREATE_ISO = 0,
    MODE_WRITE_USB = 1
} WizardMode;

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *status;
    GtkWidget *progress;
    GtkWidget *progress_title;
    GtkWidget *log_view;
    GtkTextBuffer *log_buf;
    GtkWidget *iso_entry;
    GtkWidget *iso_label;
    GtkWidget *usb_combo;
    GtkWidget *help_view;
    GtkTextBuffer *help_buf;
    GtkListStore *usb_store;
    GtkWidget *radio_iso;
    GtkWidget *radio_usb;
    GtkWidget *tile_iso;
    GtkWidget *tile_usb;
    GtkWidget *media_hint;
    GtkWidget *usb_row;
    GtkWidget *usb_label;
    GtkWidget *page_title;
    GtkWidget *create_btn;
    UsbDeviceList devices;
    WizardMode mode;
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
    const char *docs = uf_docs_path(NULL);

    if (topic && *topic)
        snprintf(path, sizeof(path), "%s/%s", docs, topic);
    else
        snprintf(path, sizeof(path), "%s/index.md", docs);

    if (g_file_get_contents(path, &content, &len, NULL)) {
        gtk_text_buffer_set_text(app.help_buf, content, (gint)len);
        g_free(content);
    } else {
        gtk_text_buffer_set_text(app.help_buf,
            "USBForge Help\n\n"
            "Use this wizard like Windows Media Creation Tool:\n"
            "1. Choose Create ISO or Write to USB\n"
            "2. Select paths\n"
            "3. Click Create / Write\n", -1);
    }
}

static void refresh_usb_list(void)
{
    int i;
    GtkTreeIter iter;

    if (!app.usb_store)
        return;

    gtk_list_store_clear(app.usb_store);
    uf_scan_usb_devices(&app.devices);

    for (i = 0; i < app.devices.count; i++) {
        UsbDevice *d = &app.devices.devices[i];
        gchar *label;
        if (!d->is_usb && !d->removable)
            continue;
        label = g_strdup_printf("%s  —  %s (%s)", d->path, d->model, d->size);
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
    append_log("Working...");
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
        app.mode == MODE_CREATE_ISO ? "Choose ISO save location" : "Select ISO file",
        GTK_WINDOW(app.window),
        app.mode == MODE_CREATE_ISO ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        app.mode == MODE_CREATE_ISO ? "_Select" : "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    if (app.mode == MODE_CREATE_ISO) {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "usbforge.iso");
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    } else {
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

static void on_build_iso(void)
{
    const char *iso;
    const char *script;
    char cmd[USBFORGE_MAX_PATH * 3];

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    if (!iso || !*iso) {
        append_log("Please choose an output ISO path first (example: ~/usbforge.iso).");
        return;
    }

    /* Don't overwrite an existing Windows/other ISO by "building" into it */
    if (uf_file_exists(iso) && uf_iso_is_windows(iso)) {
        append_log("That file is an existing Windows ISO.");
        append_log("To make a bootable USB from it: go Back, choose "
                   "'Write an existing ISO to USB flash drive', then Create.");
        append_log("To build a NEW USBForge ISO, pick a different output path "
                   "like ~/Downloads/usbforge.iso");
        return;
    }

    script = uf_find_script("build-iso.sh");
    if (!script) {
        append_log("Could not find build-iso.sh.");
        append_log("Expected in /usr/share/usbforge/scripts/ or ./scripts/");
        return;
    }

    snprintf(cmd, sizeof(cmd), "bash '%s' '%s' 2>&1", script, iso);
    start_job(cmd, "USBForge ISO created successfully. You can write it to a USB drive next.",
              "ISO build failed. See log above (needs xorriso + grub-mkrescue).");
}

static void on_write_usb(void)
{
    const char *iso;
    char *usb;
    const char *script;
    char cmd[USBFORGE_MAX_PATH * 4];
    GtkWidget *confirm;
    gint res;
    int is_win;

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    usb = selected_usb_path();

    if (!iso || !*iso || !uf_file_exists(iso)) {
        append_log("Choose a valid ISO file before writing.");
        g_free(usb);
        return;
    }
    if (!usb) {
        append_log("Select a USB flash drive first.");
        return;
    }

    is_win = uf_iso_is_windows(iso);
    confirm = gtk_message_dialog_new(
        GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
        "Everything on %s will be deleted.\n\n"
        "%s bootable media will be created from:\n%s",
        usb,
        is_win ? "Windows" : "ISO",
        iso);
    res = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (res != GTK_RESPONSE_OK) {
        append_log("Write cancelled.");
        g_free(usb);
        return;
    }

    script = uf_find_script("write-media.sh");
    if (script) {
        snprintf(cmd, sizeof(cmd),
                 "pkexec bash '%s' '%s' '%s' 2>&1", script, iso, usb);
        start_job(cmd,
                  is_win
                      ? "Windows USB media is ready. You can boot from this drive."
                      : "USB media is ready. You can boot from this drive.",
                  "USB write failed. See log (needs pkexec; Windows ISOs also need parted, dosfstools, rsync; large WIMs need wimtools).");
    } else {
        append_log("write-media.sh not found — falling back to raw dd.");
        snprintf(cmd, sizeof(cmd),
                 "pkexec dd if='%s' of='%s' bs=4M status=progress conv=fsync oflag=direct 2>&1",
                 iso, usb);
        start_job(cmd, "USB media is ready (raw dd).",
                  "USB write failed. You may need administrator rights.");
    }
    g_free(usb);
}

static void on_verify(void)
{
    const char *iso;
    char cmd[USBFORGE_MAX_PATH * 2];

    iso = gtk_entry_get_text(GTK_ENTRY(app.iso_entry));
    if (!iso || !*iso || !uf_file_exists(iso)) {
        append_log("Choose an existing ISO to verify.");
        return;
    }
    snprintf(cmd, sizeof(cmd),
             "xorriso -indev \"%s\" -find / -exec report_lba -- 2>&1 | head -n 40", iso);
    start_job(cmd, "ISO verification finished.", "Could not verify ISO.");
}

static void on_refresh_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    refresh_usb_list();
}

static void on_verify_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    on_verify();
}

static void on_show_page(const char *name)
{
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), name);
}

static void open_releases_url(const char *url)
{
    char cmd[640];
    const char *u = (url && url[0]) ? url : USBFORGE_RELEASES_URL;
    GError *err = NULL;

    if (gtk_show_uri_on_window(GTK_WINDOW(app.window), u, GDK_CURRENT_TIME, &err))
        return;
    if (err)
        g_error_free(err);
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", u);
    if (system(cmd) != 0)
        append_log("Could not open browser. Visit https://github.com/Kkkppmm/UBS/releases");
}

static void on_check_updates(GtkButton *btn, gpointer user_data)
{
    UfUpdateInfo info;
    GtkWidget *dialog;
    gint response;
    (void)btn;
    (void)user_data;

    append_log("Checking for updates...");
    while (gtk_events_pending())
        gtk_main_iteration();
    uf_check_for_updates(&info);
    append_log(info.message);

    if (!info.ok) {
        dialog = gtk_message_dialog_new(
            GTK_WINDOW(app.window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s", info.message);
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (response == GTK_RESPONSE_YES)
            open_releases_url(info.html_url);
        return;
    }

    dialog = gtk_message_dialog_new(
        GTK_WINDOW(app.window), GTK_DIALOG_MODAL,
        info.update_available ? GTK_MESSAGE_QUESTION : GTK_MESSAGE_INFO,
        info.update_available ? GTK_BUTTONS_YES_NO : GTK_BUTTONS_OK,
        "%s", info.message);
    if (info.update_available)
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "Install the update now with usbforge-update?");

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES && info.update_available) {
        if (uf_file_exists("/usr/bin/usbforge-update"))
            start_job("x-terminal-emulator -e usbforge-update 2>/dev/null || "
                      "gnome-terminal -- usbforge-update 2>/dev/null || usbforge-update",
                      "Updater finished.", "Could not launch updater.");
        else if (uf_file_exists("scripts/usbforge-update.sh"))
            start_job("bash scripts/usbforge-update.sh", "Updater finished.", "Update failed.");
        else
            open_releases_url(info.html_url);
    }
}

static void sync_mode_ui(void)
{
    gboolean write = (app.mode == MODE_WRITE_USB);

    uf_option_set_active(app.tile_usb, write);
    uf_option_set_active(app.tile_iso, !write);

    if (app.usb_row)
        gtk_widget_set_sensitive(app.usb_row, write);
    if (app.usb_label)
        gtk_widget_set_sensitive(app.usb_label, write);
    if (app.iso_label) {
        gtk_label_set_text(GTK_LABEL(app.iso_label),
            write ? "ISO file" : "Save ISO as");
    }
    if (app.media_hint) {
        gtk_label_set_text(GTK_LABEL(app.media_hint),
            write
                ? "Select the ISO image and the USB flash drive you want to use."
                : "Choose where to save the new bootable ISO image.");
    }
    if (app.page_title) {
        gtk_label_set_text(GTK_LABEL(app.page_title),
            write ? "Choose which media to use" : "Select an ISO file");
    }
    if (app.create_btn)
        gtk_button_set_label(GTK_BUTTON(app.create_btn), "Create");
}

static void on_mode_toggled(GtkToggleButton *btn, gpointer user_data)
{
    (void)user_data;
    if (!gtk_toggle_button_get_active(btn))
        return;
    if (btn == GTK_TOGGLE_BUTTON(app.radio_iso))
        app.mode = MODE_CREATE_ISO;
    else
        app.mode = MODE_WRITE_USB;
    sync_mode_ui();
}

static gboolean on_tile_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkWidget *radio = user_data;
    (void)widget;
    (void)event;
    if (radio)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
    return TRUE;
}

static void on_accept(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    on_show_page("mode");
}

static void on_back_to_welcome(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    on_show_page("welcome");
}

static void on_back_to_mode(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    on_show_page("mode");
}

static void on_next_media(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    sync_mode_ui();
    refresh_usb_list();
    on_show_page("media");
}

static void on_create(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    on_show_page("progress");
    if (app.progress_title) {
        gtk_label_set_text(GTK_LABEL(app.progress_title),
            app.mode == MODE_WRITE_USB
                ? "Creating your USB flash drive"
                : "Creating your ISO file");
    }
    if (app.progress)
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress), "Working…");
    if (app.mode == MODE_CREATE_ISO)
        on_build_iso();
    else
        on_write_usb();
}

static void on_cancel_quit(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    if (app.window)
        gtk_window_close(GTK_WINDOW(app.window));
}

static void on_open_help(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    load_help_topic("getting-started.md");
    on_show_page("help");
}

static void on_help_topic(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    load_help_topic((const char *)user_data);
}

static GtkWidget *footer_bar(GtkWidget *left, GtkWidget *right1, GtkWidget *right2)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "uf-footer");
    if (left)
        gtk_box_pack_start(GTK_BOX(bar), left, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), gtk_label_new(NULL), TRUE, TRUE, 0);
    if (right1)
        gtk_box_pack_start(GTK_BOX(bar), right1, FALSE, FALSE, 0);
    if (right2)
        gtk_box_pack_start(GTK_BOX(bar), right2, FALSE, FALSE, 0);
    return bar;
}

static GtkWidget *page_shell(GtkWidget *body, GtkWidget *footer)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(content), "uf-content");
    gtk_box_pack_start(GTK_BOX(content), body, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), content, TRUE, TRUE, 0);
    if (footer)
        gtk_box_pack_start(GTK_BOX(box), footer, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *build_welcome(void)
{
    GtkWidget *body, *title, *sub, *license_box, *scrolled, *view;
    GtkWidget *accept, *cancel, *help, *updates, *footer, *left;
    GtkTextBuffer *buf;
    const char *notices =
        "USBForge Media Creation Tool\n"
        "Copyright (c) USBForge contributors\n\n"
        "Applicable notices\n"
        "------------------\n"
        "This tool creates bootable installation media (ISO or USB flash drive),\n"
        "similar to the Windows Media Creation Tool.\n\n"
        "• Writing to a USB flash drive will erase all data on that drive.\n"
        "• Always double-check the selected drive before continuing.\n"
        "• Windows ISOs are prepared for FAT32 (large install.wim may be split).\n"
        "• After booting USBForge media you get a USB Lab for testing and docs;\n"
        "  install is optional.\n\n"
        "By selecting Accept you acknowledge these notices and agree to use this\n"
        "software at your own risk. See LICENSE and docs/safety.md for details.\n\n"
        "Privacy: Check for updates contacts GitHub Releases over the network.\n";

    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    title = uf_title_label("Applicable notices and license terms");
    sub = uf_subtitle_label(
        "Review the following notices. Select Accept to continue.");

    license_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(license_box), "uf-license");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_size_request(scrolled, -1, 240);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    view = gtk_text_view_new();
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buf, notices, -1);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 10);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    gtk_box_pack_start(GTK_BOX(license_box), scrolled, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(body), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), sub, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), license_box, TRUE, TRUE, 8);

    accept = uf_primary_button("Accept");
    cancel = uf_secondary_button("Cancel");
    help = uf_link_button("Help");
    updates = uf_link_button("Check for updates");
    g_signal_connect(accept, "clicked", G_CALLBACK(on_accept), NULL);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_quit), NULL);
    g_signal_connect(help, "clicked", G_CALLBACK(on_open_help), NULL);
    g_signal_connect(updates, "clicked", G_CALLBACK(on_check_updates), NULL);

    left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_pack_start(GTK_BOX(left), cancel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), help, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), updates, FALSE, FALSE, 0);

    footer = footer_bar(left, NULL, accept);
    return page_shell(body, footer);
}

static GtkWidget *build_mode(void)
{
    GtkWidget *body, *title, *sub, *back, *next, *footer;

    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    title = uf_title_label("What do you want to do?");
    sub = uf_subtitle_label(
        "Select an option below, then choose Next to continue.");

    app.tile_usb = uf_option_tile(
        NULL,
        "Create installation media (USB flash drive)",
        "Write a Windows or Linux ISO you already have to a USB flash drive. "
        "Recommended for most people.",
        &app.radio_usb);
    app.tile_iso = uf_option_tile(
        GTK_RADIO_BUTTON(app.radio_usb),
        "Create an ISO file",
        "Build a new USBForge bootable ISO image to save on this PC "
        "(advanced — for making USBForge Live media).",
        &app.radio_iso);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.radio_usb), TRUE);
    app.mode = MODE_WRITE_USB;
    g_signal_connect(app.radio_iso, "toggled", G_CALLBACK(on_mode_toggled), NULL);
    g_signal_connect(app.radio_usb, "toggled", G_CALLBACK(on_mode_toggled), NULL);
    g_signal_connect(app.tile_usb, "button-press-event", G_CALLBACK(on_tile_press), app.radio_usb);
    g_signal_connect(app.tile_iso, "button-press-event", G_CALLBACK(on_tile_press), app.radio_iso);

    back = uf_secondary_button("Back");
    next = uf_primary_button("Next");
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_to_welcome), NULL);
    g_signal_connect(next, "clicked", G_CALLBACK(on_next_media), NULL);

    gtk_box_pack_start(GTK_BOX(body), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), sub, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(body), app.tile_usb, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), app.tile_iso, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), gtk_label_new(NULL), TRUE, TRUE, 0);

    footer = footer_bar(NULL, back, next);
    return page_shell(body, footer);
}

static GtkWidget *build_media(void)
{
    GtkWidget *body, *browse, *refresh, *iso_row, *back, *create, *verify, *footer;
    GtkWidget *lbl;
    GtkCellRenderer *renderer;

    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    app.page_title = uf_title_label("Choose which media to use");
    app.media_hint = uf_subtitle_label(
        "Select the ISO image and the USB flash drive you want to use.");

    gtk_box_pack_start(GTK_BOX(body), app.page_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), app.media_hint, FALSE, FALSE, 6);

    app.iso_label = gtk_label_new("ISO file");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.iso_label), "uf-field-label");
    gtk_widget_set_halign(app.iso_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(body), app.iso_label, FALSE, FALSE, 8);

    iso_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.iso_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.iso_entry),
                                   "Select an ISO file…");
    gtk_entry_set_text(GTK_ENTRY(app.iso_entry), "");
    gtk_widget_set_hexpand(app.iso_entry, TRUE);
    browse = uf_secondary_button("Browse");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_iso), NULL);
    gtk_box_pack_start(GTK_BOX(iso_row), app.iso_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(iso_row), browse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), iso_row, FALSE, FALSE, 0);

    app.usb_label = gtk_label_new("Removable drive that will be used");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.usb_label), "uf-field-label");
    gtk_widget_set_halign(app.usb_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(body), app.usb_label, FALSE, FALSE, 14);

    app.usb_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.usb_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    app.usb_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(app.usb_store));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app.usb_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(app.usb_combo), renderer, "text", 0);
    gtk_widget_set_hexpand(app.usb_combo, TRUE);
    refresh = uf_secondary_button("Refresh");
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(app.usb_row), app.usb_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app.usb_row), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), app.usb_row, FALSE, FALSE, 0);

    verify = uf_link_button("Verify ISO file");
    g_signal_connect(verify, "clicked", G_CALLBACK(on_verify_clicked), NULL);
    gtk_widget_set_halign(verify, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(body), verify, FALSE, FALSE, 12);

    lbl = uf_subtitle_label(
        "Warning: Everything on the selected USB flash drive will be deleted.");
    gtk_box_pack_start(GTK_BOX(body), lbl, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), gtk_label_new(NULL), TRUE, TRUE, 0);

    back = uf_secondary_button("Back");
    create = uf_primary_button("Create");
    app.create_btn = create;
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_to_mode), NULL);
    g_signal_connect(create, "clicked", G_CALLBACK(on_create), NULL);

    footer = footer_bar(NULL, back, create);
    return page_shell(body, footer);
}

static GtkWidget *build_progress(void)
{
    GtkWidget *body, *sub, *scrolled, *frame, *back, *again, *footer;

    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    app.progress_title = uf_title_label("Creating your media");
    sub = uf_subtitle_label(
        "This might take a while — keep this window open until the process finishes.");

    app.progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app.progress), FALSE);
    gtk_widget_set_margin_top(app.progress, 12);
    gtk_widget_set_margin_bottom(app.progress, 8);

    app.status = gtk_label_new("Getting things ready…");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.status), "uf-progress-hero");
    gtk_widget_set_halign(app.status, GTK_ALIGN_START);

    frame = gtk_frame_new("Details");
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_size_request(scrolled, -1, 180);
    app.log_view = gtk_text_view_new();
    app.log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.log_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.log_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app.log_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app.log_view), 6);
    gtk_container_add(GTK_CONTAINER(scrolled), app.log_view);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);

    gtk_box_pack_start(GTK_BOX(body), app.progress_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), sub, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), app.progress, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), app.status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), frame, TRUE, TRUE, 12);

    back = uf_secondary_button("Back");
    again = uf_primary_button("Finish");
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_to_mode), NULL);
    g_signal_connect(again, "clicked", G_CALLBACK(on_back_to_welcome), NULL);
    footer = footer_bar(NULL, back, again);
    return page_shell(body, footer);
}

static GtkWidget *build_help(void)
{
    GtkWidget *body, *title, *topics, *scrolled, *back, *footer;
    GtkWidget *b1, *b2, *b3, *b4;

    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    title = uf_title_label("Help");

    topics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    b1 = uf_secondary_button("Getting started");
    b2 = uf_secondary_button("Create ISO");
    b3 = uf_secondary_button("Write USB");
    b4 = uf_secondary_button("Safety");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_help_topic), (gpointer)"getting-started.md");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_help_topic), (gpointer)"create-iso.md");
    g_signal_connect(b3, "clicked", G_CALLBACK(on_help_topic), (gpointer)"write-usb.md");
    g_signal_connect(b4, "clicked", G_CALLBACK(on_help_topic), (gpointer)"safety.md");
    gtk_box_pack_start(GTK_BOX(topics), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b4, FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    app.help_view = gtk_text_view_new();
    app.help_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.help_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.help_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.help_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app.help_view), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app.help_view), 8);
    gtk_container_add(GTK_CONTAINER(scrolled), app.help_view);

    gtk_box_pack_start(GTK_BOX(body), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), topics, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body), scrolled, TRUE, TRUE, 0);

    back = uf_primary_button("Back");
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_to_welcome), NULL);
    footer = footer_bar(NULL, NULL, back);
    load_help_topic("getting-started.md");
    return page_shell(body, footer);
}

static void activate(GtkApplication *gtk_app, gpointer user_data)
{
    GtkWidget *outer, *header;
    (void)user_data;

    memset(&app, 0, sizeof(app));
    app.mode = MODE_WRITE_USB;

    app.window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app.window), "USBForge Media Creation Tool");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 640, 520);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);

    uf_apply_fluent_theme();

    outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(outer), "uf-root");
    header = uf_header_bar_versioned("USBForge Setup", "v" USBFORGE_VERSION);

    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(app.stack), 180);
    gtk_stack_add_named(GTK_STACK(app.stack), build_welcome(), "welcome");
    gtk_stack_add_named(GTK_STACK(app.stack), build_mode(), "mode");
    gtk_stack_add_named(GTK_STACK(app.stack), build_media(), "media");
    gtk_stack_add_named(GTK_STACK(app.stack), build_progress(), "progress");
    gtk_stack_add_named(GTK_STACK(app.stack), build_help(), "help");

    gtk_box_pack_start(GTK_BOX(outer), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), app.stack, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(app.window), outer);

    refresh_usb_list();
    append_log("USBForge Media Creation Tool ready.");
    sync_mode_ui();
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
