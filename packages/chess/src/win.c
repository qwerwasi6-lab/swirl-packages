/* Swirl Chess - Win32 front end.
 * Built freestanding (no CRT) so it only imports kernel32 / user32 / gdi32,
 * all of which SwirlRun implements.  The whole UI is software-rendered into
 * ui_fb and shown with StretchDIBits. */
#include <windows.h>
#include "ui.h"

static HWND g_hwnd;
static BITMAPINFO g_bmi;
static u32 g_present[UI_W * UI_H];     /* bottom-up copy: positive biHeight is the most basic DIB form */

static u32 tick(void) { return (u32)GetTickCount(); }

/* destination rectangle inside the client area (keeps the aspect ratio) */
static void layout(int *dx, int *dy, int *dw, int *dh) {
    RECT rc;
    int cw = 0, ch = 0;
    if (GetClientRect(g_hwnd, &rc)) { cw = rc.right - rc.left; ch = rc.bottom - rc.top; }
    if (cw <= 0 || ch <= 0) { cw = UI_W; ch = UI_H; }
    int w = cw, h = cw * UI_H / UI_W;
    if (h > ch) { h = ch; w = ch * UI_W / UI_H; }
    ui_dbg[8] = cw;
    ui_dbg[9] = ch;
    *dw = w; *dh = h;
    *dx = (cw - w) / 2;
    *dy = (ch - h) / 2;
}

static void present(HDC dc) {
    for (int y = 0; y < UI_H; y++) {
        const u32 *src = &ui_fb[y * UI_W];
        u32 *dst = &g_present[(UI_H - 1 - y) * UI_W];
        for (int x = 0; x < UI_W; x++) dst[x] = src[x];
    }
    int dx, dy, dw, dh;
    layout(&dx, &dy, &dw, &dh);
    StretchDIBits(dc, dx, dy, dw, dh, 0, 0, UI_W, UI_H, g_present, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
}

static void redraw(void) {
    ui_render();
    HDC dc = GetDC(g_hwnd);
    if (dc) {
        present(dc);
        ReleaseDC(g_hwnd, dc);
    }
}

static void mouse(int type, LPARAM l) {
    int x = (short)(l & 0xFFFF), y = (short)((l >> 16) & 0xFFFF);
    int dx, dy, dw, dh;
    layout(&dx, &dy, &dw, &dh);
    if (dw > 0 && dh > 0) {
        x = (x - dx) * UI_W / dw;
        y = (y - dy) * UI_H / dh;
    }
    ui_dbg[4] = (short)(l & 0xFFFF);
    ui_dbg[5] = (short)((l >> 16) & 0xFFFF);
    ui_dbg[6] = x;
    ui_dbg[7] = y;
    ui_mouse(type, x, y);
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_LBUTTONDOWN: ui_dbg[1]++; mouse(1, l); return 0;
        case WM_LBUTTONUP: ui_dbg[2]++; mouse(2, l); return 0;
        case WM_MOUSEMOVE: ui_dbg[0]++; mouse(0, l); return 0;
        case WM_KEYDOWN: ui_dbg[3]++; ui_key((int)w); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            ui_render();
            present(dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

void start(void) {
    HINSTANCE inst = GetModuleHandleA(0);

    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = UI_W;
    g_bmi.bmiHeader.biHeight = UI_H;          /* bottom-up */
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    WNDCLASSEXA wc;
    char *z = (char *)&wc;
    for (unsigned i = 0; i < sizeof wc; i++) z[i] = 0;
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.lpszClassName = "SwirlChess";
    RegisterClassExA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, UI_W, UI_H};
    AdjustWindowRect(&r, style, FALSE);
    g_hwnd = CreateWindowExA(0, "SwirlChess", "Swirl Chess", style, 40, 10,
                             r.right - r.left, r.bottom - r.top, 0, 0, inst, 0);
    if (!g_hwnd) ExitProcess(1);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    ui_init(tick, (u32)GetTickCount() * 2654435761u + 12345u);
    redraw();

    MSG msg;
    int quit = 0;
    while (!quit) {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { quit = 1; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (quit) break;
        if (ui_step()) redraw();
        else Sleep(10);
    }
    ExitProcess(0);
}
