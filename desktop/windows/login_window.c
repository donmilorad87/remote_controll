#include "login_window.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define IDC_EMAIL       2001
#define IDC_PASSWORD    2002
#define IDC_LOGIN       2003
#define IDC_REGISTER    2004
#define IDC_STATUS      2005

static rc_login_result_t *g_result_ptr = NULL;
static HWND g_email_edit = NULL;
static HWND g_pass_edit = NULL;
static HWND g_status_label = NULL;

static LRESULT CALLBACK login_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND: {
        WORD cmd = LOWORD(wp);
        if (cmd == IDC_LOGIN || cmd == IDC_REGISTER) {
            assert(g_result_ptr != NULL);

            GetWindowTextA(g_email_edit, g_result_ptr->email,
                          sizeof(g_result_ptr->email));
            GetWindowTextA(g_pass_edit, g_result_ptr->password,
                          sizeof(g_result_ptr->password));

            if (strlen(g_result_ptr->email) < 5 ||
                strlen(g_result_ptr->password) < 8) {
                SetWindowTextA(g_status_label,
                              "Email and password (min 8 chars) required");
                return 0;
            }

            g_result_ptr->action = (cmd == IDC_LOGIN) ? 1 : 2;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (g_result_ptr != NULL) {
            g_result_ptr->action = 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

int rc_login_window_show(rc_login_result_t *result)
{
    assert(result != NULL);
    memset(result, 0, sizeof(*result));
    g_result_ptr = result;

    HINSTANCE hInst = GetModuleHandle(NULL);

    /* Register window class */
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = login_wnd_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = "RCLoginWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    /* Create main window */
    HWND hwnd = CreateWindowExA(
        0, "RCLoginWindow", "RemoteControl - Login",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 280,
        NULL, NULL, hInst, NULL);

    if (hwnd == NULL) return -1;

    /* Title label */
    HWND title = CreateWindowExA(0, "STATIC", "RemoteControl",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 12, 340, 24, hwnd, NULL, hInst, NULL);
    HFONT title_font = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    SendMessage(title, WM_SETFONT, (WPARAM)title_font, TRUE);

    /* Email */
    CreateWindowExA(0, "STATIC", "Email:",
        WS_CHILD | WS_VISIBLE, 20, 48, 60, 20, hwnd, NULL, hInst, NULL);
    g_email_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 68, 340, 24, hwnd, (HMENU)IDC_EMAIL, hInst, NULL);

    /* Password */
    CreateWindowExA(0, "STATIC", "Password:",
        WS_CHILD | WS_VISIBLE, 20, 100, 80, 20, hwnd, NULL, hInst, NULL);
    g_pass_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
        20, 120, 340, 24, hwnd, (HMENU)IDC_PASSWORD, hInst, NULL);

    /* Buttons */
    CreateWindowExA(0, "BUTTON", "Login",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        20, 160, 160, 36, hwnd, (HMENU)IDC_LOGIN, hInst, NULL);
    CreateWindowExA(0, "BUTTON", "Register",
        WS_CHILD | WS_VISIBLE,
        200, 160, 160, 36, hwnd, (HMENU)IDC_REGISTER, hInst, NULL);

    /* Status label */
    g_status_label = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 208, 340, 20, hwnd, (HMENU)IDC_STATUS, hInst, NULL);

    /* Set font for all controls */
    HFONT font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, 0, 0, "Segoe UI");
    SendMessage(g_email_edit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_pass_edit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_status_label, WM_SETFONT, (WPARAM)font, TRUE);

    /* Show and run message loop */
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(font);
    DeleteObject(title_font);
    g_result_ptr = NULL;

    return (result->action != 0) ? 0 : -1;
}
