/*
 * USBForge Builder for Windows — Media Creation Tool style (Win32).
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
#define ID_BUILD_HINT     1014
#define ID_CHECK_UPDATES  1015
#define ID_RADIO_ISO      1020
#define ID_RADIO_USB      1021
#define ID_CREATE         1022

#define COL_BG       RGB(243, 243, 243)
#define COL_WHITE    RGB(255, 255, 255)
#define COL_HEADER   RGB(0, 120, 212)
#define COL_TEXT     RGB(26, 26, 26)
#define COL_MUTED    RGB(96, 94, 92)
#define COL_BORDER   RGB(225, 225, 225)
#define COL_FOOTER   RGB(249, 249, 249)

static HWND g_hwnd;
static HWND g_iso;
static HWND g_usb;
static HWND g_log;
static HWND g_status;
static HWND g_help;
static HWND g_radio_iso;
static HWND g_radio_usb;
static UsbDeviceList g_devices;
static HFONT g_font;
static HFONT g_title_font;
static HFONT g_header_font;
static HBRUSH g_br_bg;
static HBRUSH g_br_white;
static HBRUSH g_br_header;
static HBRUSH g_br_footer;
static int g_mode_usb; /* 0=iso hint only, 1=write usb */

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
            "USBForge Media Creation Tool\r\n\r\n"
            "1. Choose Create ISO or Write to USB\r\n"
            "2. Browse for the ISO path\r\n"
            "3. Click Create / Write\r\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 512 * 1024) { fclose(f); return; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    {
        char *out = (char *)malloc((size_t)sz * 2 + 1);
        size_t i, j = 0;
        if (out) {
            for (i = 0; i < (size_t)sz; i++) {
                if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r')) {
                    out[j++] = '\r'; out[j++] = '\n';
                } else out[j++] = buf[i];
            }
            out[j] = '\0';
            SetWindowTextA(g_help, out);
            free(out);
        } else SetWindowTextA(g_help, buf);
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
        snprintf(label, sizeof(label), "%s  -  %s (%s)", d->path, d->model, d->size);
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
    char letter[8];
    int idx, res;
    UsbDevice *d;
    SHELLEXECUTEINFOA sei;
    static char params[USBFORGE_MAX_PATH * 3];

    GetWindowTextA(g_iso, iso, sizeof(iso));
    if (!iso[0] || !uf_file_exists(iso)) {
        MessageBoxA(g_hwnd, "Choose a valid ISO file first.", "USBForge", MB_ICONWARNING);
        return;
    }
    idx = selected_usb_index();
    if (idx < 0 || idx >= g_devices.count) {
        MessageBoxA(g_hwnd, "Select a USB flash drive.", "USBForge", MB_ICONWARNING);
        return;
    }
    d = &g_devices.devices[idx];
    letter[0] = d->path[0];
    letter[1] = '\0';

    res = MessageBoxA(g_hwnd,
        "WARNING: Everything on the selected USB drive will be deleted.\n\n"
        "Create bootable USBForge media now?",
        "USBForge Media Creation", MB_ICONWARNING | MB_OKCANCEL);
    if (res != IDOK) {
        append_log("Write cancelled.");
        return;
    }

    if (uf_file_exists("scripts\\write-iso.ps1"))
        snprintf(script, sizeof(script), "scripts\\write-iso.ps1");
    else if (uf_file_exists("write-iso.ps1"))
        snprintf(script, sizeof(script), "write-iso.ps1");
    else {
        char exe[USBFORGE_MAX_PATH];
        char *slash;
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        slash = strrchr(exe, '\\');
        if (slash) { *slash = '\0'; snprintf(script, sizeof(script), "%s\\write-iso.ps1", exe); }
        else snprintf(script, sizeof(script), "write-iso.ps1");
    }
    if (!uf_file_exists(script)) {
        MessageBoxA(g_hwnd, "write-iso.ps1 was not found. Reinstall USBForge.", "USBForge", MB_ICONERROR);
        return;
    }

    append_log("Starting elevated ISO write (UAC may appear)...");
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = g_hwnd;
    sei.lpVerb = "runas";
    sei.lpFile = "powershell.exe";
    snprintf(params, sizeof(params),
             "-NoProfile -ExecutionPolicy Bypass -File \"%s\" -IsoPath \"%s\" -DriveLetter %s",
             script, iso, letter);
    sei.lpParameters = params;
    sei.nShow = SW_SHOW;
    if (!ShellExecuteExA(&sei)) {
        append_log("Failed to launch writer (UAC cancelled?).");
        return;
    }
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
    append_log("Write helper finished.");
    MessageBoxA(g_hwnd, "If the write succeeded, you can reboot and boot from USB.",
                "USBForge", MB_ICONINFORMATION);
}

static void open_docs_folder(void)
{
    ShellExecuteA(g_hwnd, "open", uf_docs_path(NULL), NULL, NULL, SW_SHOWNORMAL);
}

static void build_hint(void)
{
    MessageBoxA(g_hwnd,
        "Creating a GRUB ISO needs Linux tools (xorriso / grub-mkrescue).\n\n"
        "1. Download a release ISO from GitHub Releases\n"
        "2. Or build on Linux / WSL: make iso\n"
        "3. Then use Create here to write it to USB",
        "USBForge - Create ISO", MB_ICONINFORMATION);
}

static void check_updates(void)
{
    UfUpdateInfo info;
    char msg[640];
    append_log("Checking for updates...");
    uf_check_for_updates(&info);
    append_log(info.message);
    if (!info.ok) {
        snprintf(msg, sizeof(msg), "%s", info.message);
        if (MessageBoxA(g_hwnd, msg, "USBForge Update", MB_ICONWARNING | MB_YESNO) == IDYES)
            ShellExecuteA(g_hwnd, "open",
                          info.html_url[0] ? info.html_url : USBFORGE_RELEASES_URL,
                          NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    if (info.update_available) {
        snprintf(msg, sizeof(msg), "%s\n\nOpen downloads page now?", info.message);
        if (MessageBoxA(g_hwnd, msg, "USBForge Update", MB_ICONQUESTION | MB_YESNO) == IDYES)
            ShellExecuteA(g_hwnd, "open",
                          info.html_url[0] ? info.html_url : USBFORGE_RELEASES_URL,
                          NULL, NULL, SW_SHOWNORMAL);
    } else {
        MessageBoxA(g_hwnd, info.message, "USBForge Update", MB_ICONINFORMATION);
    }
}

static HWND mk_btn(HWND parent, const char *text, int x, int y, int w, int h, int id)
{
    HWND b = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(intptr_t)id, NULL, NULL);
    SendMessageA(b, WM_SETFONT, (WPARAM)g_font, TRUE);
    return b;
}

static HWND mk_label(HWND parent, const char *text, int x, int y, int w, int h, HFONT font)
{
    HWND l = CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, NULL, NULL, NULL);
    SendMessageA(l, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
    return l;
}

static void create_ui(HWND hwnd)
{
    int y;

    g_br_bg = CreateSolidBrush(COL_BG);
    g_br_white = CreateSolidBrush(COL_WHITE);
    g_br_header = CreateSolidBrush(COL_HEADER);
    g_br_footer = CreateSolidBrush(COL_FOOTER);

    g_header_font = CreateFontA(18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_title_font = CreateFontA(24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                         DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");

    /* Header bar drawn in WM_PAINT; label on top */
    mk_label(hwnd, "USBForge Media Creation Tool", 24, 14, 500, 28, g_header_font);

    y = 64;
    mk_label(hwnd, "Create USBForge installation media", 24, y, 640, 32, g_title_font);
    y += 36;
    mk_label(hwnd, "Choose what to do, then select an ISO and USB drive — like Windows Media Creation Tool.",
             24, y, 660, 22, g_font);
    y += 36;

    g_radio_iso = CreateWindowA("BUTTON",
        "Create / download a bootable ISO file (build on Linux or use a release ISO)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        24, y, 660, 24, hwnd, (HMENU)ID_RADIO_ISO, NULL, NULL);
    SendMessageA(g_radio_iso, WM_SETFONT, (WPARAM)g_font, TRUE);
    y += 28;
    g_radio_usb = CreateWindowA("BUTTON",
        "Create bootable USB flash drive (recommended)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        24, y, 660, 24, hwnd, (HMENU)ID_RADIO_USB, NULL, NULL);
    SendMessageA(g_radio_usb, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageA(g_radio_usb, BM_SETCHECK, BST_CHECKED, 0);
    g_mode_usb = 1;
    y += 40;

    mk_label(hwnd, "ISO file", 24, y, 100, 20, g_font);
    y += 22;
    g_iso = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            24, y, 520, 28, hwnd, (HMENU)ID_ISO_EDIT, NULL, NULL);
    SendMessageA(g_iso, WM_SETFONT, (WPARAM)g_font, TRUE);
    mk_btn(hwnd, "Browse", 554, y, 100, 28, ID_BROWSE_ISO);
    y += 40;

    mk_label(hwnd, "USB flash drive", 24, y, 200, 20, g_font);
    y += 22;
    g_usb = CreateWindowA("COMBOBOX", "",
                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                          24, y, 520, 200, hwnd, (HMENU)ID_USB_LIST, NULL, NULL);
    SendMessageA(g_usb, WM_SETFONT, (WPARAM)g_font, TRUE);
    mk_btn(hwnd, "Refresh", 554, y, 100, 28, ID_REFRESH);
    y += 44;

    mk_btn(hwnd, "Create", 24, y, 120, 34, ID_CREATE);
    mk_btn(hwnd, "Build ISO hint", 156, y, 130, 34, ID_BUILD_HINT);
    mk_btn(hwnd, "Check updates", 298, y, 130, 34, ID_CHECK_UPDATES);
    mk_btn(hwnd, "Help docs", 440, y, 100, 34, ID_OPEN_DOCS);
    y += 48;

    mk_label(hwnd, "Help", 24, y, 100, 20, g_font);
    y += 22;
    mk_btn(hwnd, "Getting started", 24, y, 120, 28, ID_TOPIC_START);
    mk_btn(hwnd, "Write USB", 152, y, 100, 28, ID_TOPIC_WRITE);
    mk_btn(hwnd, "Safety", 260, y, 90, 28, ID_TOPIC_SAFE);
    y += 36;

    g_help = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                             24, y, 630, 110, hwnd, (HMENU)ID_HELP_VIEW, NULL, NULL);
    SendMessageA(g_help, WM_SETFONT, (WPARAM)g_font, TRUE);
    y += 120;

    mk_label(hwnd, "Status", 24, y, 100, 20, g_font);
    y += 22;
    g_log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                            24, y, 630, 90, hwnd, (HMENU)ID_LOG, NULL, NULL);
    SendMessageA(g_log, WM_SETFONT, (WPARAM)g_font, TRUE);
    y += 100;

    g_status = CreateWindowA("STATIC", "Ready — select an ISO and USB drive, then Create.",
                             WS_CHILD | WS_VISIBLE, 24, y, 630, 22, hwnd, (HMENU)ID_STATUS, NULL, NULL);
    SendMessageA(g_status, WM_SETFONT, (WPARAM)g_font, TRUE);

    load_help_file("getting-started.md");
    refresh_usb();
    append_log("USBForge Media Creation Tool " USBFORGE_VERSION " ready.");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        create_ui(hwnd);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc, header;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_br_bg);
        header = rc;
        header.bottom = 52;
        FillRect(hdc, &header, g_br_header);
        SetBkMode(hdc, TRANSPARENT);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        /* Header labels (~y < 52) rendered white */
        SetTextColor(hdc, COL_TEXT);
        return (LRESULT)g_br_bg;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BROWSE_ISO: browse_iso(); break;
        case ID_REFRESH: refresh_usb(); break;
        case ID_WRITE_USB:
        case ID_CREATE: write_iso_to_usb(); break;
        case ID_OPEN_DOCS: open_docs_folder(); break;
        case ID_BUILD_HINT: build_hint(); break;
        case ID_CHECK_UPDATES: check_updates(); break;
        case ID_TOPIC_START: load_help_file("getting-started.md"); break;
        case ID_TOPIC_WRITE: load_help_file("write-usb.md"); break;
        case ID_TOPIC_SAFE: load_help_file("safety.md"); break;
        case ID_RADIO_ISO:
            g_mode_usb = 0;
            append_log("Mode: create/download ISO (use Build ISO hint or a release ISO).");
            break;
        case ID_RADIO_USB:
            g_mode_usb = 1;
            append_log("Mode: write bootable USB flash drive.");
            break;
        }
        return 0;
    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        if (g_title_font) DeleteObject(g_title_font);
        if (g_header_font) DeleteObject(g_header_font);
        if (g_br_bg) DeleteObject(g_br_bg);
        if (g_br_white) DeleteObject(g_br_white);
        if (g_br_header) DeleteObject(g_br_header);
        if (g_br_footer) DeleteObject(g_br_footer);
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
    wc.hbrBackground = CreateSolidBrush(COL_BG);
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "USBForgeWinBuilder",
                           "USBForge Media Creation Tool",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 700, 720,
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
