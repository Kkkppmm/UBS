/*
 * USBForge Builder for Windows (Win32 GUI).
 * Cross-compile: x86_64-w64-mingw32-gcc
 */
#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

#include "usbforge.h"

#define ID_ISO_EDIT       1001
#define ID_BROWSE_ISO     1002
#define ID_USB_LIST       1003
#define ID_REFRESH        1004
#define ID_WRITE_USB      1005
#define ID_OPEN_DOCS      1006
#define ID_HELP_VIEW      1007
#define ID_STATUS         1008
#define ID_LOG            1009
#define ID_TOPIC_START    1010
#define ID_TOPIC_WRITE    1011
#define ID_TOPIC_SAFE     1012
#define ID_VERIFY         1013
#define ID_BUILD_HINT     1014
#define ID_CHECK_UPDATES  1015

static HWND g_hwnd;
static HWND g_iso;
static HWND g_usb;
static HWND g_log;
static HWND g_status;
static HWND g_help;
static UsbDeviceList g_devices;
static HFONT g_font;
static HFONT g_title_font;

static void set_status(const char *msg)
{
    if (g_status)
        SetWindowTextA(g_status, msg);
}

static void append_log(const char *msg)
{
    int len;
    if (!g_log || !msg)
        return;
    len = GetWindowTextLengthA(g_log);
    SendMessageA(g_log, EM_SETSEL, len, len);
    SendMessageA(g_log, EM_REPLACESEL, FALSE, (LPARAM)msg);
    SendMessageA(g_log, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
    set_status(msg);
}

static void load_help_file(const char *topic)
{
    char path[USBFORGE_MAX_PATH];
    char *buf;
    long sz;
    FILE *f;
    const char *docs = uf_docs_path(NULL);

    snprintf(path, sizeof(path), "%s\\%s", docs, topic);
    f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "docs\\%s", topic);
        f = fopen(path, "rb");
    }
    if (!f) {
        SetWindowTextA(g_help,
            "USBForge Help\r\n"
            "=============\r\n\r\n"
            "Use this Windows builder to write a USBForge ISO to a USB drive.\r\n"
            "Full ISO creation (grub-mkrescue) is supported on Linux packages;\r\n"
            "on Windows you can write a pre-built ISO, or build via WSL if installed.\r\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 512 * 1024) {
        fclose(f);
        return;
    }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    /* Normalize newlines for the edit control */
    {
        char *out = (char *)malloc((size_t)sz * 2 + 1);
        size_t i, j = 0;
        if (out) {
            for (i = 0; i < (size_t)sz; i++) {
                if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r')) {
                    out[j++] = '\r';
                    out[j++] = '\n';
                } else {
                    out[j++] = buf[i];
                }
            }
            out[j] = '\0';
            SetWindowTextA(g_help, out);
            free(out);
        } else {
            SetWindowTextA(g_help, buf);
        }
    }
    free(buf);
}

static void refresh_usb(void)
{
    int i;
    SendMessageA(g_usb, CB_RESETCONTENT, 0, 0);
    uf_scan_usb_devices(&g_devices);
    for (i = 0; i < g_devices.count; i++) {
        UsbDevice *d = &g_devices.devices[i];
        char label[256];
        snprintf(label, sizeof(label), "%s - %s (%s)", d->path, d->model, d->size);
        SendMessageA(g_usb, CB_ADDSTRING, 0, (LPARAM)label);
    }
    if (g_devices.count > 0)
        SendMessageA(g_usb, CB_SETCURSEL, 0, 0);
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %d removable drive(s).", g_devices.count);
        append_log(msg);
    }
}

static void browse_iso(void)
{
    OPENFILENAMEA ofn;
    char file[USBFORGE_MAX_PATH];

    memset(&ofn, 0, sizeof(ofn));
    file[0] = '\0';
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "ISO Images (*.iso)\0*.iso;*.ISO\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = "Select USBForge ISO";
    if (GetOpenFileNameA(&ofn))
        SetWindowTextA(g_iso, file);
}

static int selected_usb_index(void)
{
    return (int)SendMessageA(g_usb, CB_GETCURSEL, 0, 0);
}

static void write_iso_to_usb(void)
{
    char iso[USBFORGE_MAX_PATH];
    char script[USBFORGE_MAX_PATH];
    char cmd[USBFORGE_MAX_PATH * 3];
    char letter[8];
    int idx;
    int res;
    UsbDevice *d;

    GetWindowTextA(g_iso, iso, sizeof(iso));
    if (!iso[0] || !uf_file_exists(iso)) {
        MessageBoxA(g_hwnd, "Choose a valid ISO file first.", "USBForge", MB_ICONWARNING);
        return;
    }
    idx = selected_usb_index();
    if (idx < 0 || idx >= g_devices.count) {
        MessageBoxA(g_hwnd, "Select a removable USB drive.", "USBForge", MB_ICONWARNING);
        return;
    }
    d = &g_devices.devices[idx];
    letter[0] = d->path[0];
    letter[1] = '\0';

    res = MessageBoxA(g_hwnd,
        "WARNING: This will ERASE all data on the selected USB drive.\n\n"
        "Continue writing the ISO?",
        "USBForge - Confirm Write",
        MB_ICONWARNING | MB_OKCANCEL);
    if (res != IDOK) {
        append_log("Write cancelled.");
        return;
    }

    /* Prefer packaged PowerShell helper next to the EXE / install dir */
    if (uf_file_exists("scripts\\write-iso.ps1"))
        snprintf(script, sizeof(script), "scripts\\write-iso.ps1");
    else if (uf_file_exists("write-iso.ps1"))
        snprintf(script, sizeof(script), "write-iso.ps1");
    else {
        char exe[USBFORGE_MAX_PATH];
        char *slash;
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        slash = strrchr(exe, '\\');
        if (slash) {
            *slash = '\0';
            snprintf(script, sizeof(script), "%s\\write-iso.ps1", exe);
        } else {
            snprintf(script, sizeof(script), "write-iso.ps1");
        }
    }

    if (!uf_file_exists(script)) {
        MessageBoxA(g_hwnd,
            "write-iso.ps1 was not found.\n"
            "Reinstall USBForge or place write-iso.ps1 next to the EXE.",
            "USBForge", MB_ICONERROR);
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" -IsoPath \"%s\" -DriveLetter %s",
             script, iso, letter);

    append_log("Starting elevated ISO write (UAC prompt may appear)...");
    append_log(cmd);

    {
        SHELLEXECUTEINFOA sei;
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = g_hwnd;
        sei.lpVerb = "runas";
        sei.lpFile = "powershell.exe";
        {
            static char params[USBFORGE_MAX_PATH * 3];
            snprintf(params, sizeof(params),
                     "-NoProfile -ExecutionPolicy Bypass -File \"%s\" -IsoPath \"%s\" -DriveLetter %s",
                     script, iso, letter);
            sei.lpParameters = params;
        }
        sei.nShow = SW_SHOW;
        if (!ShellExecuteExA(&sei)) {
            append_log("Failed to launch writer (UAC cancelled or PowerShell missing).");
            return;
        }
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
        append_log("Write helper finished. Check the PowerShell window for results.");
        MessageBoxA(g_hwnd,
            "Write helper finished.\n\n"
            "If it succeeded, you can reboot and boot from the USB drive.",
            "USBForge", MB_ICONINFORMATION);
    }
}

static void open_docs_folder(void)
{
    const char *docs = uf_docs_path(NULL);
    ShellExecuteA(g_hwnd, "open", docs, NULL, NULL, SW_SHOWNORMAL);
}

static void build_hint(void)
{
    MessageBoxA(g_hwnd,
        "Building a GRUB-bootable USBForge ISO needs Linux tools\n"
        "(xorriso / grub-mkrescue).\n\n"
        "Options:\n"
        "  1. Download a release ISO from GitHub Releases\n"
        "  2. Build on Linux: make iso\n"
        "  3. Use WSL: wsl make iso\n\n"
        "Then use this Windows app to write the ISO to USB.",
        "USBForge - Build ISO", MB_ICONINFORMATION);
}

static void check_updates(void)
{
    UfUpdateInfo info;
    char msg[512];

    append_log("Checking for updates...");
    uf_check_for_updates(&info);
    append_log(info.message);

    if (info.update_available) {
        snprintf(msg, sizeof(msg),
                 "%s\n\nOpen the downloads page now?", info.message);
        if (MessageBoxA(g_hwnd, msg, "USBForge Update",
                        MB_ICONQUESTION | MB_YESNO) == IDYES) {
            ShellExecuteA(g_hwnd, "open",
                          info.html_url[0] ? info.html_url : USBFORGE_RELEASES_URL,
                          NULL, NULL, SW_SHOWNORMAL);
        }
    } else {
        MessageBoxA(g_hwnd, info.message, "USBForge Update", MB_ICONINFORMATION);
    }
}

static void create_ui(HWND hwnd)
{
    int y = 16;
    HWND title, lbl;

    g_title_font = CreateFontA(28, 0, 0, 0, FW_BOLD, 0, 0, 0,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                         DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");

    title = CreateWindowA("STATIC", "USBForge for Windows",
                          WS_CHILD | WS_VISIBLE, 20, y, 500, 36, hwnd, NULL, NULL, NULL);
    SendMessageA(title, WM_SETFONT, (WPARAM)g_title_font, TRUE);
    y += 40;

    lbl = CreateWindowA("STATIC", "Write a bootable USBForge ISO to USB - with help and feedback.",
                        WS_CHILD | WS_VISIBLE, 20, y, 640, 22, hwnd, NULL, NULL, NULL);
    SendMessageA(lbl, WM_SETFONT, (WPARAM)g_font, TRUE);
    y += 32;

    CreateWindowA("STATIC", "ISO file:", WS_CHILD | WS_VISIBLE, 20, y + 4, 70, 20, hwnd, NULL, NULL, NULL);
    g_iso = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            100, y, 480, 26, hwnd, (HMENU)ID_ISO_EDIT, NULL, NULL);
    CreateWindowA("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE,
                  590, y, 100, 28, hwnd, (HMENU)ID_BROWSE_ISO, NULL, NULL);
    y += 40;

    CreateWindowA("STATIC", "USB drive:", WS_CHILD | WS_VISIBLE, 20, y + 4, 70, 20, hwnd, NULL, NULL, NULL);
    g_usb = CreateWindowA("COMBOBOX", "",
                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                          100, y, 480, 200, hwnd, (HMENU)ID_USB_LIST, NULL, NULL);
    CreateWindowA("BUTTON", "Refresh", WS_CHILD | WS_VISIBLE,
                  590, y, 100, 28, hwnd, (HMENU)ID_REFRESH, NULL, NULL);
    y += 44;

    CreateWindowA("BUTTON", "Write ISO -> USB", WS_CHILD | WS_VISIBLE,
                  100, y, 150, 34, hwnd, (HMENU)ID_WRITE_USB, NULL, NULL);
    CreateWindowA("BUTTON", "Build ISO (hint)", WS_CHILD | WS_VISIBLE,
                  260, y, 140, 34, hwnd, (HMENU)ID_BUILD_HINT, NULL, NULL);
    CreateWindowA("BUTTON", "Open Docs Folder", WS_CHILD | WS_VISIBLE,
                  410, y, 140, 34, hwnd, (HMENU)ID_OPEN_DOCS, NULL, NULL);
    CreateWindowA("BUTTON", "Check Updates", WS_CHILD | WS_VISIBLE,
                  560, y, 130, 34, hwnd, (HMENU)ID_CHECK_UPDATES, NULL, NULL);
    y += 48;

    CreateWindowA("STATIC", "Help topics:", WS_CHILD | WS_VISIBLE, 20, y, 100, 20, hwnd, NULL, NULL, NULL);
    CreateWindowA("BUTTON", "Getting Started", WS_CHILD | WS_VISIBLE,
                  120, y - 4, 120, 28, hwnd, (HMENU)ID_TOPIC_START, NULL, NULL);
    CreateWindowA("BUTTON", "Write USB", WS_CHILD | WS_VISIBLE,
                  250, y - 4, 100, 28, hwnd, (HMENU)ID_TOPIC_WRITE, NULL, NULL);
    CreateWindowA("BUTTON", "Safety", WS_CHILD | WS_VISIBLE,
                  360, y - 4, 90, 28, hwnd, (HMENU)ID_TOPIC_SAFE, NULL, NULL);
    y += 36;

    g_help = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                             20, y, 670, 160, hwnd, (HMENU)ID_HELP_VIEW, NULL, NULL);
    y += 172;

    CreateWindowA("STATIC", "Feedback / Log", WS_CHILD | WS_VISIBLE, 20, y, 200, 20, hwnd, NULL, NULL, NULL);
    y += 22;
    g_log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                            20, y, 670, 110, hwnd, (HMENU)ID_LOG, NULL, NULL);
    y += 120;

    g_status = CreateWindowA("STATIC", "Ready.",
                             WS_CHILD | WS_VISIBLE, 20, y, 670, 22, hwnd, (HMENU)ID_STATUS, NULL, NULL);

    SendMessageA(g_iso, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageA(g_usb, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageA(g_help, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageA(g_log, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageA(g_status, WM_SETFONT, (WPARAM)g_font, TRUE);

    load_help_file("getting-started.md");
    refresh_usb();
    append_log("USBForge Windows Builder " USBFORGE_VERSION " ready.");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        create_ui(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BROWSE_ISO: browse_iso(); break;
        case ID_REFRESH: refresh_usb(); break;
        case ID_WRITE_USB: write_iso_to_usb(); break;
        case ID_OPEN_DOCS: open_docs_folder(); break;
        case ID_BUILD_HINT: build_hint(); break;
        case ID_CHECK_UPDATES: check_updates(); break;
        case ID_TOPIC_START: load_help_file("getting-started.md"); break;
        case ID_TOPIC_WRITE: load_help_file("write-usb.md"); break;
        case ID_TOPIC_SAFE: load_help_file("safety.md"); break;
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(230, 240, 248));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_ERASEBKGND: {
        RECT rc;
        HDC hdc = (HDC)wParam;
        HBRUSH br = CreateSolidBrush(RGB(15, 39, 68));
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return 1;
    }
    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        if (g_title_font) DeleteObject(g_title_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    (void)hPrev;
    (void)lpCmd;

    /* Console smoke mode */
    if (lpCmd && strstr(lpCmd, "--smoke")) {
        UsbDeviceList list;
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        printf("USBForge Windows %s smoke test\n", USBFORGE_VERSION);
        uf_scan_usb_devices(&list);
        printf("removable drives: %d\n", list.count);
        return 0;
    }

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "USBForgeWinBuilder";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "USBForgeWinBuilder",
                           "USBForge - Bootable ISO Writer (Windows)",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 730, 700,
                           NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

#endif /* _WIN32 */
