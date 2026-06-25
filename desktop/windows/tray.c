#include "tray.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>

#define WM_TRAYICON     (WM_USER + 100)
#define IDM_RECONNECT   3002
#define IDM_QUIT        3003

static NOTIFYICONDATAA g_nid;
static HWND g_tray_hwnd = NULL;
static rc_tray_quit_cb g_quit_cb = NULL;
static rc_tray_reconnect_cb g_reconnect_cb = NULL;

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING | MF_GRAYED, 0, g_nid.szTip);
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(menu, MF_STRING, IDM_RECONNECT, "New Pairing");
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(menu, MF_STRING, IDM_QUIT, "Quit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(menu);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDM_QUIT) { if (g_quit_cb) g_quit_cb(); Shell_NotifyIconA(NIM_DELETE, &g_nid); PostQuitMessage(0); }
        else if (LOWORD(wp) == IDM_RECONNECT) { if (g_reconnect_cb) g_reconnect_cb(); }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &g_nid); PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int rc_tray_init(HINSTANCE hInstance, rc_tray_quit_cb on_quit, rc_tray_reconnect_cb on_reconnect)
{
    g_quit_cb = on_quit;
    g_reconnect_cb = on_reconnect;
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = tray_wnd_proc; wc.hInstance = hInstance; wc.lpszClassName = "RCTrayWnd";
    RegisterClassA(&wc);
    g_tray_hwnd = CreateWindowExA(0, "RCTrayWnd", "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!g_tray_hwnd) return -1;
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAA); g_nid.hWnd = g_tray_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nid.szTip, "RemoteControl");
    Shell_NotifyIconA(NIM_ADD, &g_nid);
    return 0;
}

void rc_tray_set_status(const char *status)
{
    if (status) { snprintf(g_nid.szTip, sizeof(g_nid.szTip), "RC - %s", status); Shell_NotifyIconA(NIM_MODIFY, &g_nid); }
}

void rc_tray_run(void) { MSG msg; while (GetMessage(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessage(&msg); } }

void rc_tray_shutdown(void)
{
    if (g_tray_hwnd) { Shell_NotifyIconA(NIM_DELETE, &g_nid); DestroyWindow(g_tray_hwnd); g_tray_hwnd = NULL; }
}
