/*
 * USBForge Live - boot-session GUI (USB Lab).
 * More than an installer: USB testing, diagnostics, docs, and optional install.
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "usbforge.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *status;
    GtkWidget *device_list;
    GtkListStore *device_store;
    GtkWidget *result_view;
    GtkTextBuffer *result_buf;
    GtkWidget *help_view;
    GtkTextBuffer *help_buf;
    GtkWidget *install_target;
    GtkListStore *target_store;
    UsbDeviceList devices;
} LiveApp;

static LiveApp app;

static void set_status(const char *msg)
{
    if (app.status)
        gtk_label_set_text(GTK_LABEL(app.status), msg);
}

static void set_result(const char *text)
{
    if (app.result_buf)
        gtk_text_buffer_set_text(app.result_buf, text ? text : "", -1);
}

static void on_show_page(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), (const char *)user_data);
}

static void refresh_devices(void)
{
    int i;
    GtkTreeIter iter;

    gtk_list_store_clear(app.device_store);
    if (app.target_store)
        gtk_list_store_clear(app.target_store);

    uf_scan_usb_devices(&app.devices);

    for (i = 0; i < app.devices.count; i++) {
        UsbDevice *d = &app.devices.devices[i];
        gchar *row;

        row = g_strdup_printf("%s | %s | %s | %s%s",
                              d->path, d->model, d->size, d->transport,
                              d->is_usb ? " *" : "");
        gtk_list_store_append(app.device_store, &iter);
        gtk_list_store_set(app.device_store, &iter,
                           0, row,
                           1, d->path,
                           2, d->is_usb,
                           -1);
        g_free(row);

        if (app.target_store && (d->is_usb || d->removable)) {
            gchar *tlabel = g_strdup_printf("%s - %s (%s)", d->path, d->model, d->size);
            gtk_list_store_append(app.target_store, &iter);
            gtk_list_store_set(app.target_store, &iter, 0, tlabel, 1, d->path, -1);
            g_free(tlabel);
        }
    }

    if (app.install_target &&
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app.target_store), &iter))
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.install_target), 0);

    {
        gchar *msg = g_strdup_printf("Found %d block device(s).", app.devices.count);
        set_status(msg);
        g_free(msg);
    }
}

static void on_refresh(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    (void)user_data;
    refresh_devices();
    set_result("Device list refreshed.\nUSB devices are marked with *.");
}

static char *selected_device_path(GtkWidget *combo, GtkListStore *store)
{
    GtkTreeIter iter;
    gchar *path = NULL;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
        return NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &path, -1);
    return path;
}

static void on_usb_probe(GtkButton *btn, gpointer user_data)
{
    GString *out;
    int i;
    (void)btn;
    (void)user_data;

    refresh_devices();
    out = g_string_new("USB Lab - Device Probe\n");
    g_string_append(out, "========================\n\n");

    if (app.devices.count == 0) {
        g_string_append(out, "No block devices found.\n");
    }

    for (i = 0; i < app.devices.count; i++) {
        UsbDevice *d = &app.devices.devices[i];
        char syspath[USBFORGE_MAX_PATH];
        char buf[256];

        g_string_append_printf(out,
            "Device:  %s\n"
            "Model:   %s\n"
            "Size:    %s\n"
            "Type:    %s\n"
            "USB:     %s\n"
            "Removable: %s\n",
            d->path, d->model, d->size, d->transport,
            d->is_usb ? "yes" : "no",
            d->removable ? "yes" : "no");

        snprintf(syspath, sizeof(syspath), "/sys/block/%s/stat", d->name);
        if (uf_read_file(syspath, buf, sizeof(buf)) > 0)
            g_string_append_printf(out, "I/O stat: %s", buf);

        g_string_append(out, "\n------------------------\n\n");
    }

    set_result(out->str);
    set_status("USB probe complete.");
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "results");
    g_string_free(out, TRUE);
}

static void on_usb_read_test(GtkButton *btn, gpointer user_data)
{
    char *path = NULL;
    int fd;
    unsigned char *buf;
    const size_t chunk = 1024 * 1024; /* 1 MiB */
    const int rounds = 16;
    int i, n;
    size_t total = 0;
    clock_t t0, t1;
    double sec, mibs;
    GString *out;
    GtkWidget *dialog;
    GtkWidget *combo;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    gint res;
    int j;
    (void)btn;
    (void)user_data;

    dialog = gtk_dialog_new_with_buttons(
        "Select USB for read test",
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Test", GTK_RESPONSE_ACCEPT,
        NULL);

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    for (j = 0; j < app.devices.count; j++) {
        UsbDevice *d = &app.devices.devices[j];
        gchar *label;
        if (!d->is_usb && !d->removable)
            continue;
        label = g_strdup_printf("%s (%s)", d->path, d->model);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, label, 1, d->path, -1);
        g_free(label);
    }
    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer, "text", 0);
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       combo, TRUE, TRUE, 12);
    gtk_widget_show_all(dialog);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT)
        path = selected_device_path(combo, store);
    gtk_widget_destroy(dialog);
    g_object_unref(store);

    if (!path) {
        set_status("Read test cancelled.");
        return;
    }

    set_status("Running safe read-only speed test...");
    while (gtk_events_pending())
        gtk_main_iteration();

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        gchar *err = g_strdup_printf(
            "Could not open %s for reading:\n%s\n"
            "Try running as root if needed.",
            path, strerror(errno));
        set_result(err);
        g_free(err);
        set_status("Read test failed.");
        gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "results");
        g_free(path);
        return;
    }

    buf = g_malloc(chunk);
    t0 = clock();
    for (i = 0; i < rounds; i++) {
        n = (int)read(fd, buf, chunk);
        if (n <= 0)
            break;
        total += (size_t)n;
    }
    t1 = clock();
    close(fd);
    g_free(buf);

    sec = (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    if (sec < 0.0001)
        sec = 0.0001;
    mibs = ((double)total / (1024.0 * 1024.0)) / sec;

    out = g_string_new("USB Lab - Read Speed Test (safe, read-only)\n");
    g_string_append(out, "==========================================\n\n");
    g_string_append_printf(out,
        "Device:     %s\n"
        "Bytes read: %zu\n"
        "Time:       %.3f s\n"
        "Approx:     %.1f MiB/s\n\n"
        "Note: This is a simple sequential read sample.\n"
        "It does not write to the drive.\n",
        path, total, sec, mibs);

    set_result(out->str);
    set_status("Read test finished.");
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "results");
    g_string_free(out, TRUE);
    g_free(path);
}

static void on_diagnostics(GtkButton *btn, gpointer user_data)
{
    GString *out;
    char buf[4096];
    struct statvfs vfs;
    (void)btn;
    (void)user_data;

    out = g_string_new("USBForge Live - System Diagnostics\n");
    g_string_append(out, "==================================\n\n");

    if (uf_read_file("/proc/version", buf, sizeof(buf)) > 0)
        g_string_append_printf(out, "Kernel:\n%s\n", buf);

    if (uf_read_file("/proc/meminfo", buf, sizeof(buf)) > 0) {
        char *line = strtok(buf, "\n");
        int lines = 0;
        g_string_append(out, "Memory:\n");
        while (line && lines < 5) {
            g_string_append_printf(out, "  %s\n", line);
            line = strtok(NULL, "\n");
            lines++;
        }
        g_string_append(out, "\n");
    }

    if (uf_read_file("/proc/cpuinfo", buf, sizeof(buf)) > 0) {
        char *p = strstr(buf, "model name");
        if (p) {
            char *nl = strchr(p, '\n');
            if (nl)
                *nl = '\0';
            g_string_append_printf(out, "CPU: %s\n\n", p);
        }
    }

    if (statvfs("/", &vfs) == 0) {
        unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
        unsigned long long freeb = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
        char t[64], f[64];
        uf_human_size(total, t, sizeof(t));
        uf_human_size(freeb, f, sizeof(f));
        g_string_append_printf(out, "Root filesystem: %s total, %s available\n\n", t, f);
    }

    if (uf_read_file("/proc/uptime", buf, sizeof(buf)) > 0)
        g_string_append_printf(out, "Uptime (seconds): %s\n", buf);

    g_string_append_printf(out, "\nUSBForge Live %s\n", USBFORGE_VERSION);

    set_result(out->str);
    set_status("Diagnostics collected.");
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "results");
    g_string_free(out, TRUE);
}

static void load_help(const char *topic)
{
    char path[USBFORGE_MAX_PATH];
    char *content = NULL;
    gsize len = 0;
    const char *docs = uf_docs_path(NULL);

    snprintf(path, sizeof(path), "%s/%s", docs, topic ? topic : "index.md");
    if (g_file_get_contents(path, &content, &len, NULL)) {
        gtk_text_buffer_set_text(app.help_buf, content, (gint)len);
        g_free(content);
    } else {
        gtk_text_buffer_set_text(app.help_buf,
            "USBForge Live Help\n\n"
            "This live session is a USB Lab - not only an installer.\n\n"
            "Use the home menu to:\n"
            "  * Probe and test USB devices\n"
            "  * Run system diagnostics\n"
            "  * Read documentation\n"
            "  * Optionally install USBForge files to a USB drive\n",
            -1);
    }
}

static void on_help_topic(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    load_help((const char *)user_data);
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "help");
}

static void on_install(GtkButton *btn, gpointer user_data)
{
    char *target;
    GtkWidget *confirm;
    gint res;
    GString *out;
    char dest_dir[USBFORGE_MAX_PATH];
    char cmd[8192];
    int rc;
    (void)btn;
    (void)user_data;

    target = selected_device_path(app.install_target, app.target_store);
    if (!target) {
        set_status("Select a USB target first.");
        return;
    }

    confirm = gtk_message_dialog_new(
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK_CANCEL,
        "Install USBForge payload onto %s?\n\n"
        "This copies the live tools and docs onto the drive\n"
        "(does not re-partition unless you choose a full ISO write from the Builder).",
        target);
    res = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (res != GTK_RESPONSE_OK) {
        set_status("Install cancelled.");
        g_free(target);
        return;
    }

    /* Mountable install: copy payload to mount point if possible */
    snprintf(dest_dir, sizeof(dest_dir), "/mnt/usbforge-install");
    uf_ensure_dir(dest_dir);
    snprintf(cmd, sizeof(cmd),
             "mount '%s1' '%s' 2>/dev/null || mount '%s' '%s' 2>/dev/null; "
             "mkdir -p '%s/usbforge' && "
             "(cp -a /usbforge/. '%s/usbforge/' 2>/dev/null || "
             " cp -a iso/usbforge/. '%s/usbforge/' 2>/dev/null || "
             " (mkdir -p '%s/usbforge/bin' '%s/usbforge/docs' && "
             "  cp -a docs/. '%s/usbforge/docs/' && "
             "  cp -a build/usbforge-live '%s/usbforge/bin/')); "
             "sync",
             target, dest_dir, target, dest_dir,
             dest_dir, dest_dir, dest_dir, dest_dir, dest_dir, dest_dir, dest_dir);

    set_status("Installing payload...");
    while (gtk_events_pending())
        gtk_main_iteration();

    rc = system(cmd);

    out = g_string_new("Install USBForge - Results\n");
    g_string_append(out, "==========================\n\n");
    if (rc == 0) {
        g_string_append_printf(out,
            "Payload copy attempted to %s via mount point %s.\n\n"
            "For a full bootable drive, use USBForge Builder on a host PC:\n"
            "  1. Build the ISO\n"
            "  2. Write ISO -> USB\n"
            "  3. Boot this Live session from firmware (USB boot)\n",
            target, dest_dir);
        set_status("Install finished (see results).");
    } else {
        g_string_append(out,
            "Could not complete automatic install.\n"
            "Tip: mount the USB partition yourself, then copy /usbforge onto it,\n"
            "or use the host Builder to write the full bootable ISO.\n");
        set_status("Install had issues - see results.");
    }

    set_result(out->str);
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "results");
    g_string_free(out, TRUE);
    g_free(target);
}

static void apply_css(void)
{
    GtkCssProvider *provider;
    const char *css =
        "window {"
        "  background: linear-gradient(145deg, #102a1f 0%, #0d3d32 40%, #145c4c 100%);"
        "}"
        "label, textview, textview text {"
        "  color: #e7f7f1;"
        "  font-family: 'IBM Plex Sans', 'Source Sans 3', 'Segoe UI', sans-serif;"
        "}"
        "button {"
        "  background-image: none;"
        "  background-color: #1f8a6e;"
        "  color: #f3fffb;"
        "  border-radius: 8px;"
        "  padding: 14px 18px;"
        "  border: none;"
        "  font-weight: 600;"
        "  font-size: 14px;"
        "}"
        "button:hover { background-color: #27a884; }"
        "button.danger { background-color: #b3543c; }"
        "button.danger:hover { background-color: #c8664c; }"
        "entry, combobox, frame, scrolledwindow, treeview {"
        "  background-color: rgba(6, 32, 26, 0.75);"
        "  color: #e7f7f1;"
        "  border-radius: 6px;"
        "}";

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GtkWidget *big_button(const char *label, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(b, 280, 48);
    g_signal_connect(b, "clicked", cb, data);
    return b;
}

static GtkWidget *build_home(void)
{
    GtkWidget *box, *title, *sub, *grid;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(box, 36);
    gtk_widget_set_margin_bottom(box, 36);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>USBForge Live</span>");
    sub = gtk_label_new("USB Lab - test, diagnose, learn, then install if you want");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(sub, GTK_ALIGN_CENTER);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    gtk_grid_attach(GTK_GRID(grid), big_button("USB Device Probe", G_CALLBACK(on_usb_probe), NULL), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), big_button("USB Read Speed Test", G_CALLBACK(on_usb_read_test), NULL), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), big_button("System Diagnostics", G_CALLBACK(on_diagnostics), NULL), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), big_button("Help & Docs", G_CALLBACK(on_show_page), (gpointer)"help"), 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), big_button("Install / Copy Tools", G_CALLBACK(on_show_page), (gpointer)"install"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), big_button("Device List", G_CALLBACK(on_show_page), (gpointer)"devices"), 1, 2, 1, 1);

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 12);
    return box;
}

static GtkWidget *build_devices(void)
{
    GtkWidget *box, *scrolled, *view, *row, *refresh, *back;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_label_new("Block devices (USB marked with *)"), FALSE, FALSE, 0);

    app.device_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app.device_store));
    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Devices", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    app.device_list = view;

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    refresh = gtk_button_new_with_label("Refresh");
    back = gtk_button_new_with_label("Back to Home");
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh), NULL);
    g_signal_connect(back, "clicked", G_CALLBACK(on_show_page), (gpointer)"home");
    gtk_box_pack_start(GTK_BOX(row), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), back, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *build_results(void)
{
    GtkWidget *box, *scrolled, *back;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 16);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_label_new("Results / Feedback"), FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    app.result_view = gtk_text_view_new();
    app.result_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.result_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.result_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.result_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), app.result_view);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

    back = gtk_button_new_with_label("Back to Home");
    g_signal_connect(back, "clicked", G_CALLBACK(on_show_page), (gpointer)"home");
    gtk_box_pack_start(GTK_BOX(box), back, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *build_help(void)
{
    GtkWidget *box, *topics, *scrolled, *back;
    GtkWidget *b1, *b2, *b3, *b4;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 16);

    gtk_box_pack_start(GTK_BOX(box),
        gtk_label_new("Help & Documentation"), FALSE, FALSE, 0);

    topics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    b1 = gtk_button_new_with_label("Overview");
    b2 = gtk_button_new_with_label("USB Testing");
    b3 = gtk_button_new_with_label("Live Lab");
    b4 = gtk_button_new_with_label("Safety");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_help_topic), (gpointer)"index.md");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_help_topic), (gpointer)"usb-testing.md");
    g_signal_connect(b3, "clicked", G_CALLBACK(on_help_topic), (gpointer)"live-lab.md");
    g_signal_connect(b4, "clicked", G_CALLBACK(on_help_topic), (gpointer)"safety.md");
    gtk_box_pack_start(GTK_BOX(topics), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topics), b4, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), topics, FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    app.help_view = gtk_text_view_new();
    app.help_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.help_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app.help_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.help_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), app.help_view);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

    back = gtk_button_new_with_label("Back to Home");
    g_signal_connect(back, "clicked", G_CALLBACK(on_show_page), (gpointer)"home");
    gtk_box_pack_start(GTK_BOX(box), back, FALSE, FALSE, 0);

    load_help("index.md");
    return box;
}

static GtkWidget *build_install(void)
{
    GtkWidget *box, *lbl, *hint, *go, *back, *row;
    GtkCellRenderer *renderer;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_margin_top(box, 24);

    lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl),
        "<span size='large' weight='bold'>Install / Copy Tools</span>");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);

    hint = gtk_label_new(
        "USBForge Live is more than an installer. Use USB testing and docs first.\n"
        "When you are ready, copy the USBForge tools onto a USB drive here.\n"
        "For a fully bootable stick, prefer Builder -> Write ISO -> USB on a host PC.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0);

    app.target_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    app.install_target = gtk_combo_box_new_with_model(GTK_TREE_MODEL(app.target_store));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app.install_target), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(app.install_target), renderer, "text", 0);

    go = gtk_button_new_with_label("Copy USBForge to selected USB");
    gtk_style_context_add_class(gtk_widget_get_style_context(go), "danger");
    g_signal_connect(go, "clicked", G_CALLBACK(on_install), NULL);

    back = gtk_button_new_with_label("Back to Home");
    g_signal_connect(back, "clicked", G_CALLBACK(on_show_page), (gpointer)"home");

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), go, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), back, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("Target USB:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.install_target, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 8);
    return box;
}

static void activate(GtkApplication *gtk_app, gpointer user_data)
{
    GtkWidget *outer;
    (void)user_data;

    memset(&app, 0, sizeof(app));
    app.window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app.window), "USBForge Live - USB Lab");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 920, 640);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);

    apply_css();

    outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_add_named(GTK_STACK(app.stack), build_home(), "home");
    gtk_stack_add_named(GTK_STACK(app.stack), build_devices(), "devices");
    gtk_stack_add_named(GTK_STACK(app.stack), build_results(), "results");
    gtk_stack_add_named(GTK_STACK(app.stack), build_help(), "help");
    gtk_stack_add_named(GTK_STACK(app.stack), build_install(), "install");

    app.status = gtk_label_new("Welcome to USBForge Live. Pick a tool to begin.");
    gtk_widget_set_margin_bottom(app.status, 10);
    gtk_widget_set_margin_start(app.status, 16);
    gtk_widget_set_halign(app.status, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(outer), app.stack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), app.status, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(app.window), outer);

    refresh_devices();
    gtk_widget_show_all(app.window);
}

int main(int argc, char **argv)
{
    GtkApplication *gtk_app;
    int status;

    /* Allow running without a display for smoke tests */
    if (argc > 1 && strcmp(argv[1], "--smoke") == 0) {
        UsbDeviceList list;
        printf("USBForge Live %s smoke test\n", USBFORGE_VERSION);
        uf_scan_usb_devices(&list);
        printf("devices: %d\n", list.count);
        return 0;
    }

    gtk_app = gtk_application_new("org.usbforge.live", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}
