#ifndef USBFORGE_H
#define USBFORGE_H

#define USBFORGE_NAME        "USBForge"
#define USBFORGE_VERSION     "1.3.0"
#define USBFORGE_TAGLINE     "Create, test, and install bootable USB media"
#define USBFORGE_DOCS_DIR    "docs"
#define USBFORGE_ISO_LABEL   "USBFORGE"
#define USBFORGE_RELEASES_API "https://api.github.com/repos/Kkkppmm/UBS/releases/latest"
#define USBFORGE_RELEASES_URL "https://github.com/Kkkppmm/UBS/releases"

#define USBFORGE_MAX_PATH    1024
#define USBFORGE_MAX_LINE    512
#define USBFORGE_MAX_DEVICES 64
#define USBFORGE_MAX_LOG     8192

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#endif

typedef struct {
    char path[64];       /* /dev/sdX or E:\ or \\.\PhysicalDrive1 */
    char name[128];
    char model[128];
    char size[64];
    char transport[32];
    int  removable;
    int  is_usb;
    int  physical_index; /* Windows PhysicalDriveN, -1 if unknown */
} UsbDevice;

typedef struct {
    UsbDevice devices[USBFORGE_MAX_DEVICES];
    int count;
} UsbDeviceList;

int  uf_scan_usb_devices(UsbDeviceList *list);
int  uf_read_file(const char *path, char *buf, int buflen);
int  uf_file_exists(const char *path);
int  uf_copy_file(const char *src, const char *dst);
int  uf_ensure_dir(const char *path);
void uf_human_size(unsigned long long bytes, char *out, int outlen);
int  uf_run_cmd(const char *cmd, char *out, int outlen);
const char *uf_docs_path(const char *relative);

typedef struct {
    char latest_tag[64];
    char html_url[512];
    int  update_available; /* 1 if latest > installed */
    int  ok;               /* 1 if check succeeded */
    char message[320];
} UfUpdateInfo;

/* Compare dotted versions like 1.1.0 vs 1.1.1; returns <0, 0, >0 */
int  uf_version_cmp(const char *a, const char *b);
/* Query GitHub Releases (curl, wget, or python3). Fills info. */
int  uf_check_for_updates(UfUpdateInfo *info);

/* Locate a packaged script (build-iso.sh, write-media.sh, ...). */
const char *uf_find_script(const char *name);
/* Return 1 if ISO looks like Microsoft Windows install media. */
int  uf_iso_is_windows(const char *iso_path);

#endif
