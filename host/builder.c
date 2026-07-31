/*
 * USBForge Builder — Media Creation Tool (WebKitGTK + HTML/CSS/JS UI).
 */
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "usbforge.h"

typedef struct {
    GtkWidget *window;
    WebKitWebView *web;
    UsbDeviceList devices;
    int busy;
    int alive;           /* 0 after window destroy */
    unsigned job_gen;    /* ignore stale job callbacks */
    char ui_dir[USBFORGE_MAX_PATH];
} App;

static App app;

static char *json_escape(const char *s)
{
    GString *out = g_string_new(NULL);
    const unsigned char *p;
    if (!s) return g_strdup("");
    for (p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '\\': g_string_append(out, "\\\\"); break;
        case '"':  g_string_append(out, "\\\""); break;
        case '\n': g_string_append(out, "\\n"); break;
        case '\r': g_string_append(out, "\\r"); break;
        case '\t': g_string_append(out, "\\t"); break;
        default:
            if (*p < 0x20)
                g_string_append_printf(out, "\\u%04x", *p);
            else
                g_string_append_c(out, (char)*p);
            break;
        }
    }
    return g_string_free(out, FALSE);
}

static void ui_send_raw(const char *js_arg_json)
{
    char *script;
    if (!app.alive || !app.web || !js_arg_json)
        return;
    script = g_strdup_printf("window.UF && window.UF.receive(%s);", js_arg_json);
    webkit_web_view_evaluate_javascript(app.web, script, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(script);
}

static void ui_send_event(const char *json_object)
{
    ui_send_raw(json_object);
}

static void ui_log(const char *text)
{
    char *esc = json_escape(text);
    char *json = g_strdup_printf("{\"event\":\"log\",\"text\":\"%s\"}", esc);
    ui_send_event(json);
    g_free(esc);
    g_free(json);
}

static void ui_progress(int percent, const char *klass)
{
    char *json = g_strdup_printf(
        "{\"event\":\"progress\",\"percent\":%d,\"klass\":\"%s\"}",
        percent, klass ? klass : "");
    ui_send_event(json);
    g_free(json);
}

static int find_ui_dir(char *out, int outlen)
{
    const char *cands[] = {
        "ui/builder",
        "/usr/share/usbforge/ui/builder",
        "/usr/local/share/usbforge/ui/builder",
        "/usbforge/ui/builder",
        NULL
    };
    int i;
    char probe[USBFORGE_MAX_PATH];
    for (i = 0; cands[i]; i++) {
        snprintf(probe, sizeof(probe), "%s/index.html", cands[i]);
        if (uf_file_exists(probe)) {
            snprintf(out, outlen, "%s", cands[i]);
            return 0;
        }
    }
    /* Relative to executable */
    {
        char exe[USBFORGE_MAX_PATH];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n > 0) {
            char *slash;
            exe[n] = '\0';
            slash = strrchr(exe, '/');
            if (slash) {
                *slash = '\0';
                snprintf(probe, sizeof(probe), "%s/../share/usbforge/ui/builder/index.html", exe);
                if (uf_file_exists(probe)) {
                    snprintf(out, outlen, "%s/../share/usbforge/ui/builder", exe);
                    return 0;
                }
                snprintf(probe, sizeof(probe), "%s/../../ui/builder/index.html", exe);
                if (uf_file_exists(probe)) {
                    snprintf(out, outlen, "%s/../../ui/builder", exe);
                    return 0;
                }
            }
        }
    }
    return -1;
}

static void send_usb_list(void)
{
    GString *json;
    int i, n = 0;
    char *esc;

    uf_scan_usb_devices(&app.devices);
    json = g_string_new("{\"event\":\"usb_list\",\"devices\":[");
    for (i = 0; i < app.devices.count; i++) {
        UsbDevice *d = &app.devices.devices[i];
        char *p, *m, *s;
        if (!d->is_usb && !d->removable)
            continue;
        if (n++)
            g_string_append_c(json, ',');
        p = json_escape(d->path);
        m = json_escape(d->model[0] ? d->model : d->name);
        s = json_escape(d->size);
        g_string_append_printf(json,
            "{\"path\":\"%s\",\"model\":\"%s\",\"size\":\"%s\"}", p, m, s);
        g_free(p); g_free(m); g_free(s);
    }
    g_string_append(json, "]}");
    ui_send_event(json->str);
    g_string_free(json, TRUE);
    (void)esc;
}

static void send_help(const char *topic)
{
    char path[USBFORGE_MAX_PATH];
    char *content = NULL;
    gsize len = 0;
    const char *docs = uf_docs_path(NULL);
    char *esc;
    char *json;

    if (!topic || !*topic)
        topic = "getting-started.md";
    snprintf(path, sizeof(path), "%s/%s", docs, topic);
    if (!g_file_get_contents(path, &content, &len, NULL)) {
        content = g_strdup("Help topic not found.\nSee docs/ in the USBForge package.");
    }
    esc = json_escape(content);
    json = g_strdup_printf("{\"event\":\"help\",\"text\":\"%s\"}", esc);
    ui_send_event(json);
    g_free(content);
    g_free(esc);
    g_free(json);
}

/* ---- background jobs ---- */

typedef struct {
    char cmd[USBFORGE_MAX_PATH * 3];
    char ok_msg[256];
    char fail_msg[256];
    unsigned gen;
} Job;

typedef struct {
    Job *job;
    int rc;
    char *output;
} JobResult;

static gboolean on_job_done(gpointer data)
{
    JobResult *r = data;
    char *esc;
    char *json;

    if (!app.alive || r->job->gen != app.job_gen) {
        g_free(r->output);
        g_free(r->job);
        g_free(r);
        return G_SOURCE_REMOVE;
    }

    if (r->output && r->output[0]) {
        ui_log("--- command output ---");
        ui_log(r->output);
        ui_log("--- end output ---");
    }

    app.busy = 0;
    esc = json_escape(r->rc == 0 ? r->job->ok_msg : r->job->fail_msg);
    json = g_strdup_printf(
        "{\"event\":\"job_done\",\"ok\":%s,\"message\":\"%s\"}",
        r->rc == 0 ? "true" : "false", esc);
    ui_send_event(json);
    g_free(esc);
    g_free(json);

    g_free(r->output);
    g_free(r->job);
    g_free(r);
    return G_SOURCE_REMOVE;
}

static gpointer job_thread(gpointer data)
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

static int start_job(const char *cmd, const char *ok, const char *fail)
{
    Job *job;
    pthread_t thr;
    int prc;

    if (app.busy) {
        ui_log("A job is already running. Please wait.");
        return -1;
    }
    job = g_new0(Job, 1);
    snprintf(job->cmd, sizeof(job->cmd), "%s", cmd);
    snprintf(job->ok_msg, sizeof(job->ok_msg), "%s", ok);
    snprintf(job->fail_msg, sizeof(job->fail_msg), "%s", fail);
    job->gen = ++app.job_gen;
    app.busy = 1;
    ui_log("Working…");
    ui_log(cmd);
    ui_progress(10, "busy");
    prc = pthread_create(&thr, NULL, job_thread, job);
    if (prc != 0) {
        app.busy = 0;
        ui_log("Could not start background worker.");
        g_free(job);
        return -1;
    }
    pthread_detach(thr);
    return 0;
}

static char *shell_single_quote(const char *s)
{
    GString *out = g_string_new("'");
    for (; s && *s; s++) {
        if (*s == '\'')
            g_string_append(out, "'\\''");
        else
            g_string_append_c(out, *s);
    }
    g_string_append_c(out, '\'');
    return g_string_free(out, FALSE);
}

static void do_browse(const char *mode)
{
    GtkWidget *dialog;
    gint res;
    int save = mode && !strcmp(mode, "save");

    dialog = gtk_file_chooser_dialog_new(
        save ? "Choose ISO save location" : "Select ISO file",
        GTK_WINDOW(app.window),
        save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        save ? "_Select" : "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    if (save) {
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
        char *esc = json_escape(file);
        char *json = g_strdup_printf("{\"event\":\"iso_chosen\",\"path\":\"%s\"}", esc);
        ui_send_event(json);
        g_free(file); g_free(esc); g_free(json);
    }
    gtk_widget_destroy(dialog);
}

static void do_verify(const char *iso)
{
    char *q;
    char cmd[USBFORGE_MAX_PATH * 3];
    if (!iso || !*iso || !uf_file_exists(iso)) {
        ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Choose a valid ISO file first.\"}");
        return;
    }
    q = shell_single_quote(iso);
    /* Avoid "| head" so pipeline status reflects xorriso, not head */
    snprintf(cmd, sizeof(cmd),
             "bash -c 'set -o pipefail; xorriso -indev %s -find / -exec report_lba -- 2>&1 | sed -n \"1,40p\"'",
             q);
    g_free(q);
    start_job(cmd, "ISO verification finished.", "Could not verify ISO (is xorriso installed?).");
}

static void do_create(const char *mode, const char *iso, const char *usb)
{
    char *qi;
    char cmd[USBFORGE_MAX_PATH * 4];
    const char *script;
    int is_win;
    GtkWidget *confirm;
    gint res;

    if (!iso || !*iso) {
        ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Choose an ISO path first.\"}");
        return;
    }

    if (mode && !strcmp(mode, "iso")) {
        char *qs;
        if (uf_file_exists(iso) && uf_iso_is_windows(iso)) {
            ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"That file is an existing Windows ISO. Choose Write to USB, or pick a different output path.\"}");
            return;
        }
        script = uf_find_script("build-iso.sh");
        if (!script) {
            ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Could not find build-iso.sh.\"}");
            return;
        }
        qi = shell_single_quote(iso);
        qs = shell_single_quote(script);
        snprintf(cmd, sizeof(cmd), "bash %s %s 2>&1", qs, qi);
        g_free(qs);
        g_free(qi);
        start_job(cmd,
                  "USBForge ISO created successfully. You can write it to a USB drive next.",
                  "ISO build failed. See log (needs xorriso + grub-mkrescue).");
        return;
    }

    /* Write USB */
    if (!usb || !*usb) {
        ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Select a USB flash drive first.\"}");
        return;
    }
    if (!uf_file_exists(iso)) {
        ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Choose a valid ISO file before writing.\"}");
        return;
    }

    is_win = uf_iso_is_windows(iso);

    /* Preflight: allow auto-install when elevation is available */
    {
        char check_cmd[USBFORGE_MAX_PATH * 4];
        char check_out[USBFORGE_MAX_LOG];
        const char *writer = uf_find_script("usbforge-write.sh");
        const char *wmedia = uf_find_script("write-media.sh");
        char *qscript, *qiso;
        int rc;
        int exit_code;

        if (!wmedia && !writer) {
            ui_send_event("{\"event\":\"job_done\",\"ok\":false,\"message\":\"Could not find write-media.sh. Reinstall USBForge or run from the source tree.\"}");
            return;
        }
        qiso = shell_single_quote(iso);
        qscript = shell_single_quote(writer ? writer : wmedia);
        snprintf(check_cmd, sizeof(check_cmd), "bash %s --check-deps %s 2>&1", qscript, qiso);
        g_free(qscript);
        g_free(qiso);
        check_out[0] = '\0';
        rc = uf_run_cmd(check_cmd, check_out, sizeof(check_out));
#ifndef _WIN32
        if (WIFEXITED(rc))
            exit_code = WEXITSTATUS(rc);
        else
            exit_code = (rc != 0) ? 1 : 0;
#else
        exit_code = rc;
#endif
        /* 0 = ok, 2 = missing but auto-install available, 1 = hard fail */
        if (exit_code == 1) {
            char *esc = json_escape(check_out[0] ? check_out :
                "Missing tools and cannot auto-install. Install pkexec or sudo, then retry.");
            char *json = g_strdup_printf(
                "{\"event\":\"job_done\",\"ok\":false,\"message\":\"USB write preflight failed.\\n\\n%s\"}",
                esc);
            ui_send_event(json);
            g_free(esc);
            g_free(json);
            return;
        }
        if (check_out[0])
            ui_log(check_out);
        if (exit_code == 2)
            ui_log("Missing packages will be installed automatically when you confirm (admin password may be required).");
    }

    confirm = gtk_message_dialog_new(
        GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
        "Everything on %s will be deleted.\n\n"
        "%s bootable media will be created from:\n%s\n\n"
        "If tools are missing, USBForge will install them automatically.",
        usb, is_win ? "Windows" : "ISO", iso);
    res = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (res != GTK_RESPONSE_OK) {
        ui_send_event("{\"event\":\"job_cancelled\"}");
        ui_log("Write cancelled.");
        return;
    }

    {
        const char *writer = uf_find_script("usbforge-write.sh");
        const char *wmedia = uf_find_script("write-media.sh");
        char *qs, *qi2, *qu2;

        qi2 = shell_single_quote(iso);
        qu2 = shell_single_quote(usb);
        if (writer) {
            qs = shell_single_quote(writer);
            snprintf(cmd, sizeof(cmd), "bash %s %s %s 2>&1", qs, qi2, qu2);
            g_free(qs);
        } else {
            qs = shell_single_quote(wmedia);
            snprintf(cmd, sizeof(cmd),
                     "bash -c 'S=%s; I=%s; D=%s; "
                     "if command -v pkexec >/dev/null; then pkexec /bin/bash \"$S\" \"$I\" \"$D\"; "
                     "elif command -v sudo >/dev/null; then sudo /bin/bash \"$S\" \"$I\" \"$D\"; "
                     "else echo \"[usbforge] ERROR: need pkexec or sudo\" >&2; exit 1; fi' 2>&1",
                     qs, qi2, qu2);
            g_free(qs);
        }
        g_free(qi2);
        g_free(qu2);
        start_job(cmd,
                  is_win
                      ? "Windows USB media is ready. You can boot from this drive."
                      : "USB media is ready. You can boot from this drive.",
                  "USB write failed. See log above. USBForge auto-installs parted/dosfstools/"
                  "rsync/wimtools when possible — approve the admin prompt, ensure the USB "
                  "is plugged in, and retry.");
    }
}

static void do_check_updates(void)
{
    UfUpdateInfo info;
    char *esc;
    char *json;

    uf_check_for_updates(&info);
    esc = json_escape(info.message);
    if (!info.ok) {
        json = g_strdup_printf(
            "{\"event\":\"update_result\",\"message\":\"%s\",\"open_url\":\"%s\"}",
            esc, USBFORGE_RELEASES_URL);
    } else if (info.update_available) {
        char *url = json_escape(info.html_url[0] ? info.html_url : USBFORGE_RELEASES_URL);
        json = g_strdup_printf(
            "{\"event\":\"update_result\",\"message\":\"%s\\n\\nOpen downloads page?\",\"open_url\":\"%s\"}",
            esc, url);
        g_free(url);
    } else {
        json = g_strdup_printf(
            "{\"event\":\"update_result\",\"message\":\"%s\"}", esc);
    }
    ui_send_event(json);
    g_free(esc);
    g_free(json);
}

static char *json_get_string(const char *json, const char *key)
{
    char pattern[128];
    const char *p, *q;
    char *out;
    size_t n;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) return NULL;
    p = strchr(p + strlen(pattern), '"');
    if (!p) return NULL;
    p++;
    q = p;
    while (*q && *q != '"') {
        if (*q == '\\' && q[1]) q += 2;
        else q++;
    }
    if (*q != '"') return NULL;
    n = (size_t)(q - p);
    out = g_malloc(n + 1);
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static void handle_message(const char *payload)
{
    char *action;

    if (!payload || !*payload)
        return;
    action = json_get_string(payload, "action");
    if (!action)
        return;

    if (!strcmp(action, "ready")) {
        char *json = g_strdup_printf(
            "{\"event\":\"ready\",\"version\":\"%s\"}", USBFORGE_VERSION);
        ui_send_event(json);
        g_free(json);
        send_usb_list();
    } else if (!strcmp(action, "scan_usb")) {
        send_usb_list();
    } else if (!strcmp(action, "browse_iso")) {
        char *mode = json_get_string(payload, "mode");
        do_browse(mode ? mode : "open");
        g_free(mode);
    } else if (!strcmp(action, "verify_iso")) {
        char *iso = json_get_string(payload, "iso");
        do_verify(iso);
        g_free(iso);
    } else if (!strcmp(action, "create")) {
        char *mode = json_get_string(payload, "mode");
        char *iso = json_get_string(payload, "iso");
        char *usb = json_get_string(payload, "usb");
        do_create(mode, iso, usb);
        g_free(mode); g_free(iso); g_free(usb);
    } else if (!strcmp(action, "check_updates")) {
        do_check_updates();
    } else if (!strcmp(action, "load_help")) {
        char *topic = json_get_string(payload, "topic");
        send_help(topic);
        g_free(topic);
    } else if (!strcmp(action, "open_url")) {
        char *url = json_get_string(payload, "url");
        if (url && *url) {
            char cmd[640];
            snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", url);
            if (system(cmd) != 0)
                ui_log("Could not open browser.");
        }
        g_free(url);
    } else if (!strcmp(action, "quit")) {
        if (app.window)
            gtk_window_close(GTK_WINDOW(app.window));
    }
    g_free(action);
}

static void on_script_message(WebKitUserContentManager *mgr,
                              WebKitJavascriptResult *result,
                              gpointer user_data)
{
    JSCValue *value;
    gchar *payload = NULL;
    (void)mgr;
    (void)user_data;

    value = webkit_javascript_result_get_js_value(result);
    if (jsc_value_is_string(value))
        payload = jsc_value_to_string(value);
    else {
        /* Some WebKit builds deliver objects; stringify via JSON */
        JSCValue *s = jsc_value_object_invoke_method(value, "toString", G_TYPE_NONE);
        if (s && jsc_value_is_string(s))
            payload = jsc_value_to_string(s);
    }
    if (payload) {
        handle_message(payload);
        g_free(payload);
    }
}

static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer data)
{
    (void)w; (void)e; (void)data;
    if (app.busy) {
        GtkWidget *d = gtk_message_dialog_new(
            GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
            "A job is still running.\n\nClose anyway? The write may continue in the background.");
        gint res = gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        if (res != GTK_RESPONSE_YES)
            return TRUE;
    }
    app.alive = 0;
    app.job_gen++;
    return FALSE;
}

static void activate(GtkApplication *gtk_app, gpointer user_data)
{
    WebKitUserContentManager *ucm;
    WebKitSettings *settings;
    char index_uri[USBFORGE_MAX_PATH * 2];
    char index_path[USBFORGE_MAX_PATH];
    (void)user_data;

    memset(&app, 0, sizeof(app));
    app.alive = 1;

    if (find_ui_dir(app.ui_dir, sizeof(app.ui_dir)) != 0) {
        g_printerr("USBForge: UI not found (ui/builder/index.html).\n");
        return;
    }
    snprintf(index_path, sizeof(index_path), "%s/index.html", app.ui_dir);

    app.window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app.window), "USBForge Media Creation Tool");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 720, 580);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
    g_signal_connect(app.window, "delete-event", G_CALLBACK(on_delete), NULL);

    ucm = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(ucm, "usbforge");
    g_signal_connect(ucm, "script-message-received::usbforge",
                     G_CALLBACK(on_script_message), NULL);

    app.web = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(ucm));
    settings = webkit_web_view_get_settings(app.web);
    webkit_settings_set_enable_developer_extras(settings, FALSE);
    webkit_settings_set_javascript_can_access_clipboard(settings, FALSE);

    {
        char *abs = realpath(index_path, NULL);
        if (abs) {
            snprintf(index_uri, sizeof(index_uri), "file://%s", abs);
            free(abs);
        } else {
            snprintf(index_uri, sizeof(index_uri), "file://%s", index_path);
        }
    }
    webkit_web_view_load_uri(app.web, index_uri);

    gtk_container_add(GTK_CONTAINER(app.window), GTK_WIDGET(app.web));
    gtk_widget_show_all(app.window);
}

int main(int argc, char **argv)
{
    GtkApplication *gtk_app;
    int status;

    if (argc > 1 && !strcmp(argv[1], "--smoke")) {
        printf("USBForge Builder %s smoke test (webkit ui)\n", USBFORGE_VERSION);
        return 0;
    }

    gtk_app = gtk_application_new("org.usbforge.builder",
#if GLIB_CHECK_VERSION(2,74,0)
                                  G_APPLICATION_DEFAULT_FLAGS
#else
                                  G_APPLICATION_FLAGS_NONE
#endif
    );
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}
