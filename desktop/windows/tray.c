#include "tray.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>

#define WM_TRAYICON     (WM_USER + 100)
#define IDM_STATUS      3001
#define IDM_RECONNECT   3002
#define IDM_QUIT        3003

static NOTIFYICONDATAA  g_nid;
static HWND             g_tray_hwnd = NULL;
static rc_tray_quit_cb  g_quit_cb = NULL;
static rc_tray_reconnect_cb g_reconnect_cb = NULL;

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING | MF_GRAYED, IDM_STATUS, g_nid.szTip);
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(menu, MF_STRING, IDM_RECONNECT, "Reconnect");
            AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(menu, MF_STRING, IDM_QUIT, "Quit");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                           pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(menu);
            PostMessage(hwnd, WM_NULL, 0, 0);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_RECONNECT:
            if (g_reconnect_cb != NULL) g_reconnect_cb();
            return 0;
        case IDM_QUIT:
            if (g_quit_cb != NULL) g_quit_cb();
            Shell_NotifyIconA(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

int rc_tray_init(HINSTANCE hInstance, rc_tray_quit_cb on_quit,
                 rc_tray_reconnect_cb on_reconnect)
{
    assert(on_quit != NULL);

    g_quit_cb = on_quit;
    g_reconnect_cb = on_reconnect;

    /* Register hidden window class */
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = tray_wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "RCTrayWindow";
    RegisterClassA(&wc);

    /* Create hidden message-only window */
    g_tray_hwnd = CreateWindowExA(0, "RCTrayWindow", "RemoteControl Tray",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (g_tray_hwnd == NULL) {
        fprintf(stderr, "[tray] Failed to create tray window\n");
        return -1;
    }

    /* Add tray icon */
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAA);
    g_nid.hWnd = g_tray_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nid.szTip, "RemoteControl - Connecting...");

    Shell_NotifyIconA(NIM_ADD, &g_nid);

    fprintf(stdout, "[tray] System tray initialized\n");
    return 0;
}

void rc_tray_set_status(const char *status)
{
    if (status != NULL) {
        snprintf(g_nid.szTip, sizeof(g_nid.szTip), "RemoteControl - %s", status);
        Shell_NotifyIconA(NIM_MODIFY, &g_nid);
    }
}

void rc_tray_run(void)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void rc_tray_shutdown(void)
{
    if (g_tray_hwnd != NULL) {
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        DestroyWindow(g_tray_hwnd);
        g_tray_hwnd = NULL;
    }
}
