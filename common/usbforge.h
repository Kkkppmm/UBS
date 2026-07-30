#ifndef USBFORGE_H
#define USBFORGE_H

#define USBFORGE_NAME        "USBForge"
#define USBFORGE_VERSION     "1.0.0"
#define USBFORGE_TAGLINE     "Create, test, and install bootable USB media"
#define USBFORGE_DOCS_DIR    "docs"
#define USBFORGE_ISO_LABEL   "USBFORGE"

#define USBFORGE_MAX_PATH    1024
#define USBFORGE_MAX_LINE    512
#define USBFORGE_MAX_DEVICES 64
#define USBFORGE_MAX_LOG     8192

typedef struct {
    char path[64];
    char name[128];
    char model[128];
    char size[64];
    char transport[32];
    int  removable;
    int  is_usb;
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

#endif
