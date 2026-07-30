#include "usbforge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

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

    f = fopen(path, "r");
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

    if (!path || !*path)
        return -1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0)
        return -1;
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
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
}

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
        /* whole disks only: sdX, nvmeXnY, vdX, mmcblkN */
        if (!(strncmp(ent->d_name, "sd", 2) == 0 ||
              strncmp(ent->d_name, "vd", 2) == 0 ||
              strncmp(ent->d_name, "nvme", 4) == 0 ||
              strncmp(ent->d_name, "mmcblk", 6) == 0))
            continue;
        /* skip partitions */
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

        snprintf(path, sizeof(path), "/sys/block/%s/device/uevent", ent->d_name);
        if (d->is_usb)
            snprintf(d->transport, sizeof(d->transport), "USB");
        else if (removable)
            snprintf(d->transport, sizeof(d->transport), "Removable");
        else
            snprintf(d->transport, sizeof(d->transport), "Disk");

        /* Prefer USB / removable for the list; still include others for install targets */
        list->count++;
    }

    closedir(dir);
    return list->count;
}

const char *uf_docs_path(const char *relative)
{
    static char path[USBFORGE_MAX_PATH];
    const char *candidates[] = {
        "docs",
        "iso/usbforge/docs",
        "/usbforge/docs",
        "/usr/share/usbforge/docs",
        NULL
    };
    int i;

    for (i = 0; candidates[i]; i++) {
        if (relative && relative[0])
            snprintf(path, sizeof(path), "%s/%s", candidates[i], relative);
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
