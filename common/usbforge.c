#include "usbforge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define mkdir(path, mode) _mkdir(path)
#else
#  include <unistd.h>
#  include <dirent.h>
#endif

int uf_file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

int uf_read_file(const char *path, char *buf, int buflen)
{
    FILE *f;
    size_t n;

    if (!path || !buf || buflen < 1)
        return -1;

    f = fopen(path, "rb");
    if (!f)
        return -1;

    n = fread(buf, 1, (size_t)buflen - 1, f);
    buf[n] = '\0';
    fclose(f);
    return (int)n;
}

int uf_ensure_dir(const char *path)
{
    char tmp[USBFORGE_MAX_PATH];
    char *p;
    size_t len;
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    if (!path || !*path)
        return -1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0)
        return -1;
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = sep;
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int uf_copy_file(const char *src, const char *dst)
{
    FILE *in, *out;
    char buf[8192];
    size_t n;

    in = fopen(src, "rb");
    if (!in)
        return -1;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

void uf_human_size(unsigned long long bytes, char *out, int outlen)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double)bytes;
    int i = 0;

    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        i++;
    }
    if (i == 0)
        snprintf(out, outlen, "%llu %s", bytes, units[i]);
    else
        snprintf(out, outlen, "%.1f %s", v, units[i]);
}

int uf_run_cmd(const char *cmd, char *out, int outlen)
{
#ifdef _WIN32
    FILE *fp;
    size_t n;
    if (!cmd)
        return -1;
    fp = _popen(cmd, "r");
    if (!fp)
        return -1;
    if (out && outlen > 0) {
        n = fread(out, 1, (size_t)outlen - 1, fp);
        out[n] = '\0';
    } else {
        char discard[256];
        while (fread(discard, 1, sizeof(discard), fp) > 0)
            ;
    }
    return _pclose(fp);
#else
    FILE *fp;
    size_t n;
    if (!cmd)
        return -1;
    fp = popen(cmd, "r");
    if (!fp)
        return -1;
    if (out && outlen > 0) {
        n = fread(out, 1, (size_t)outlen - 1, fp);
        out[n] = '\0';
    } else {
        char discard[256];
        while (fread(discard, 1, sizeof(discard), fp) > 0)
            ;
    }
    return pclose(fp);
#endif
}

#ifdef _WIN32

int uf_scan_usb_devices(UsbDeviceList *list)
{
    char drives[256];
    char *p;
    DWORD n;

    if (!list)
        return -1;
    list->count = 0;

    n = GetLogicalDriveStringsA(sizeof(drives) - 1, drives);
    if (n == 0 || n >= sizeof(drives))
        return 0;

    for (p = drives; *p && list->count < USBFORGE_MAX_DEVICES; p += strlen(p) + 1) {
        UINT type = GetDriveTypeA(p);
        UsbDevice *d;
        ULARGE_INTEGER free_bytes, total_bytes;
        char vol[MAX_PATH];
        char fs[MAX_PATH];
        DWORD serial = 0, maxcomp = 0, flags = 0;

        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED)
            continue;
        /* Prefer removable; still list fixed for completeness but mark USB only for removable */
        if (type != DRIVE_REMOVABLE)
            continue;

        d = &list->devices[list->count];
        memset(d, 0, sizeof(*d));
        snprintf(d->path, sizeof(d->path), "%s", p);
        snprintf(d->name, sizeof(d->name), "%c:", p[0]);
        d->removable = 1;
        d->is_usb = 1;
        d->physical_index = -1;
        snprintf(d->transport, sizeof(d->transport), "USB/Removable");

        vol[0] = '\0';
        if (GetVolumeInformationA(p, vol, sizeof(vol), &serial, &maxcomp, &flags, fs, sizeof(fs))) {
            if (vol[0])
                snprintf(d->model, sizeof(d->model), "%s", vol);
            else
                snprintf(d->model, sizeof(d->model), "Removable (%s)", fs[0] ? fs : "disk");
        } else {
            snprintf(d->model, sizeof(d->model), "Removable Drive");
        }

        if (GetDiskFreeSpaceExA(p, &free_bytes, &total_bytes, NULL))
            uf_human_size((unsigned long long)total_bytes.QuadPart, d->size, sizeof(d->size));
        else
            snprintf(d->size, sizeof(d->size), "unknown");

        list->count++;
    }
    return list->count;
}

#else /* Linux / Unix */

static void trim_newline(char *s)
{
    size_t n;
    if (!s)
        return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

static int read_sysfs_str(const char *path, char *out, int outlen)
{
    if (uf_read_file(path, out, outlen) < 0) {
        if (outlen > 0)
            out[0] = '\0';
        return -1;
    }
    trim_newline(out);
    return 0;
}

static int is_usb_device(const char *name)
{
    char path[USBFORGE_MAX_PATH];
    char link[USBFORGE_MAX_PATH];
    ssize_t n;

    snprintf(path, sizeof(path), "/sys/block/%s", name);
    n = readlink(path, link, sizeof(link) - 1);
    if (n < 0)
        return 0;
    link[n] = '\0';
    return strstr(link, "usb") != NULL;
}

int uf_scan_usb_devices(UsbDeviceList *list)
{
    DIR *dir;
    struct dirent *ent;
    char path[USBFORGE_MAX_PATH];
    char buf[128];
    unsigned long long sectors;
    unsigned long long sect_size;

    if (!list)
        return -1;

    list->count = 0;
    dir = opendir("/sys/block");
    if (!dir)
        return -1;

    while ((ent = readdir(dir)) != NULL && list->count < USBFORGE_MAX_DEVICES) {
        UsbDevice *d;
        int removable = 0;

        if (ent->d_name[0] == '.')
            continue;
        if (!(strncmp(ent->d_name, "sd", 2) == 0 ||
              strncmp(ent->d_name, "vd", 2) == 0 ||
              strncmp(ent->d_name, "nvme", 4) == 0 ||
              strncmp(ent->d_name, "mmcblk", 6) == 0))
            continue;
        if (strchr(ent->d_name, 'p') && isdigit((unsigned char)ent->d_name[strlen(ent->d_name) - 1]))
            continue;
        if (strncmp(ent->d_name, "sd", 2) == 0 && isdigit((unsigned char)ent->d_name[strlen(ent->d_name) - 1]))
            continue;

        snprintf(path, sizeof(path), "/sys/block/%s/removable", ent->d_name);
        if (read_sysfs_str(path, buf, sizeof(buf)) == 0)
            removable = atoi(buf);

        d = &list->devices[list->count];
        memset(d, 0, sizeof(*d));
        snprintf(d->path, sizeof(d->path), "/dev/%s", ent->d_name);
        snprintf(d->name, sizeof(d->name), "%s", ent->d_name);
        d->removable = removable;
        d->is_usb = is_usb_device(ent->d_name);
        d->physical_index = -1;

        snprintf(path, sizeof(path), "/sys/block/%s/device/model", ent->d_name);
        if (read_sysfs_str(path, d->model, sizeof(d->model)) != 0)
            snprintf(d->model, sizeof(d->model), "Unknown");

        snprintf(path, sizeof(path), "/sys/block/%s/device/vendor", ent->d_name);
        if (read_sysfs_str(path, buf, sizeof(buf)) == 0 && buf[0]) {
            char model[256];
            snprintf(model, sizeof(model), "%s %s", buf, d->model);
            snprintf(d->model, sizeof(d->model), "%s", model);
        }

        snprintf(path, sizeof(path), "/sys/block/%s/size", ent->d_name);
        sectors = 0;
        if (read_sysfs_str(path, buf, sizeof(buf)) == 0)
            sectors = strtoull(buf, NULL, 10);

        sect_size = 512;
        snprintf(path, sizeof(path), "/sys/block/%s/queue/logical_block_size", ent->d_name);
        if (read_sysfs_str(path, buf, sizeof(buf)) == 0)
            sect_size = strtoull(buf, NULL, 10);

        uf_human_size(sectors * sect_size, d->size, sizeof(d->size));

        if (d->is_usb)
            snprintf(d->transport, sizeof(d->transport), "USB");
        else if (removable)
            snprintf(d->transport, sizeof(d->transport), "Removable");
        else
            snprintf(d->transport, sizeof(d->transport), "Disk");

        list->count++;
    }

    closedir(dir);
    return list->count;
}

#endif /* !_WIN32 */

const char *uf_docs_path(const char *relative)
{
    static char path[USBFORGE_MAX_PATH];
    const char *candidates[] = {
        "docs",
#ifdef _WIN32
        ".\\docs",
        "C:\\Program Files\\USBForge\\docs",
#else
        "iso/usbforge/docs",
        "/usbforge/docs",
        "/usr/share/usbforge/docs",
#endif
        NULL
    };
    int i;

    for (i = 0; candidates[i]; i++) {
        if (relative && relative[0])
#ifdef _WIN32
            snprintf(path, sizeof(path), "%s\\%s", candidates[i], relative);
#else
            snprintf(path, sizeof(path), "%s/%s", candidates[i], relative);
#endif
        else
            snprintf(path, sizeof(path), "%s", candidates[i]);
        if (uf_file_exists(path))
            return path;
    }

    if (relative && relative[0])
        snprintf(path, sizeof(path), "docs/%s", relative);
    else
        snprintf(path, sizeof(path), "docs");
    return path;
}

int uf_version_cmp(const char *a, const char *b)
{
    int ai[4] = {0, 0, 0, 0};
    int bi[4] = {0, 0, 0, 0};
    int i;

    if (!a) a = "0";
    if (!b) b = "0";
    sscanf(a, "%d.%d.%d.%d", &ai[0], &ai[1], &ai[2], &ai[3]);
    sscanf(b, "%d.%d.%d.%d", &bi[0], &bi[1], &bi[2], &bi[3]);
    for (i = 0; i < 4; i++) {
        if (ai[i] < bi[i]) return -1;
        if (ai[i] > bi[i]) return 1;
    }
    return 0;
}

static void json_extract_string(const char *json, const char *key, char *out, int outlen)
{
    char pattern[128];
    const char *p, *q;

    if (!json || !key || !out || outlen < 1) {
        if (out && outlen > 0) out[0] = '\0';
        return;
    }
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) return;
    p = strchr(p + strlen(pattern), '"');
    if (!p) return;
    p++;
    q = strchr(p, '"');
    if (!q) return;
    if ((int)(q - p) >= outlen)
        q = p + outlen - 1;
    memcpy(out, p, (size_t)(q - p));
    out[q - p] = '\0';
}

int uf_check_for_updates(UfUpdateInfo *info)
{
    char cmd[512];
    char buf[USBFORGE_MAX_LOG];
    char tag[64];
    const char *ver;
    int rc;

    if (!info)
        return -1;
    memset(info, 0, sizeof(*info));

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
             "curl.exe -fsSL -A USBForge/%s \"%s\" 2>nul",
             USBFORGE_VERSION, USBFORGE_RELEASES_API);
#else
    snprintf(cmd, sizeof(cmd),
             "curl -fsSL -A 'USBForge/%s' '%s' 2>/dev/null || "
             "wget -qO- --user-agent='USBForge/%s' '%s' 2>/dev/null",
             USBFORGE_VERSION, USBFORGE_RELEASES_API,
             USBFORGE_VERSION, USBFORGE_RELEASES_API);
#endif

    buf[0] = '\0';
    rc = uf_run_cmd(cmd, buf, sizeof(buf));
    if (rc != 0 || !buf[0] || buf[0] != '{') {
        info->ok = 0;
        snprintf(info->message, sizeof(info->message),
                 "Could not reach update server. Check your network (needs curl/wget).");
        snprintf(info->html_url, sizeof(info->html_url), "%s", USBFORGE_RELEASES_URL);
        return -1;
    }

    json_extract_string(buf, "tag_name", tag, sizeof(tag));
    json_extract_string(buf, "html_url", info->html_url, sizeof(info->html_url));
    if (!tag[0]) {
        info->ok = 0;
        snprintf(info->message, sizeof(info->message), "Unexpected response from update server.");
        return -1;
    }

    /* strip leading v */
    ver = tag;
    if (ver[0] == 'v' || ver[0] == 'V')
        ver++;
    snprintf(info->latest_tag, sizeof(info->latest_tag), "%s", tag);
    if (!info->html_url[0])
        snprintf(info->html_url, sizeof(info->html_url), "%s", USBFORGE_RELEASES_URL);

    info->ok = 1;
    if (uf_version_cmp(USBFORGE_VERSION, ver) < 0) {
        info->update_available = 1;
        snprintf(info->message, sizeof(info->message),
                 "Update available: %s (you have %s)", tag, USBFORGE_VERSION);
    } else {
        info->update_available = 0;
        snprintf(info->message, sizeof(info->message),
                 "You are up to date (v%s).", USBFORGE_VERSION);
    }
    return 0;
}

const char *uf_find_script(const char *name)
{
    static char path[USBFORGE_MAX_PATH];
    const char *dirs[] = {
        "scripts",
        "/usr/share/usbforge/scripts",
        "/usbforge/scripts",
        "/usr/local/share/usbforge/scripts",
        NULL
    };
    int i;

    if (!name || !*name)
        return NULL;

    for (i = 0; dirs[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dirs[i], name);
        if (uf_file_exists(path))
            return path;
    }
    return NULL;
}

int uf_iso_is_windows(const char *iso_path)
{
    char cmd[USBFORGE_MAX_PATH * 2];
    char out[USBFORGE_MAX_LOG];

    if (!iso_path || !uf_file_exists(iso_path))
        return 0;

    /* Look for classic Windows layout markers inside the ISO */
    snprintf(cmd, sizeof(cmd),
             "xorriso -indev '%s' -find /bootmgr -exec report_lba -- 2>/dev/null | head -n 3; "
             "xorriso -indev '%s' -find /sources -exec report_lba -- 2>/dev/null | head -n 5",
             iso_path, iso_path);
    out[0] = '\0';
    uf_run_cmd(cmd, out, sizeof(out));
    if (strstr(out, "/bootmgr") || strstr(out, "/sources/") || strstr(out, "bootmgr"))
        return 1;

    /* Fallback: filename heuristics */
    {
        const char *base = strrchr(iso_path, '/');
        base = base ? base + 1 : iso_path;
        if (strncmp(base, "Win", 3) == 0 || strstr(base, "windows") || strstr(base, "Windows") ||
            strstr(base, "WIN10") || strstr(base, "WIN11") || strstr(base, "Win10") ||
            strstr(base, "Win11"))
            return 1;
    }
    return 0;
}

