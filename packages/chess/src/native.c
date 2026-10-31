/* Swirl Chess - native SwirlOS front end (static-PIE ELF, SwirlWM client).
 *
 * The game itself (engine.c rules + AI, ui.c software-rendered UI, assets.c
 * pieces and font, selftest.c) is platform independent and unchanged from
 * the Windows build.  This file is the only SwirlOS-specific part:
 *
 *   - opens ONE SwirlWM window (wclient) of UI_W x UI_H content pixels,
 *   - turns WM events into ui_mouse() / ui_key() calls,
 *   - copies the ui_fb software framebuffer into the window canvas
 *     (1:1, or letterboxed + nearest-neighbour scaled if the window is
 *     resized, aspect ratio kept),
 *   - runs the 12-stage engine self-test once at start (see ERRORS.txt).
 *
 * Build: see build.sh (needs a SwirlOS source tree, r58 or newer). */
#include "gclient_core.h"

/* The game modules are freestanding and share no headers with the OS
 * (engine.h defines its own u8/u32/Pos/Move...), so only their entry
 * points are declared here. */
#define UI_W 840
#define UI_H 600
#define SELFTEST_STAGES 12
#define LETTERBOX 0x302E2B             /* matches the board background */

extern uint32_t ui_fb[UI_W * UI_H];    /* 0x00RRGGBB */
extern int ui_dbg[16];
void ui_engine_status(int state, int a, int b);
void ui_init(uint32_t (*now)(void), uint32_t seed);
void ui_render(void);
void ui_mouse(int type, int x, int y);     /* 0 move, 1 left down, 2 left up */
void ui_key(int vk);                       /* Windows-style virtual key code */
int ui_step(void);                         /* 1 = redraw; runs the AI when due */
int selftest_run(int stage, uint32_t (*now)(void));

static uint32_t now_ms(void) { return s_ticks_ms(); }

static int st_state, st_code;       /* self-test result, re-applied to mark the UI dirty */

/* ---- presenting -------------------------------------------------------- */

/* destination rectangle of the UI inside the client area (keeps aspect) */
static void layout(const wclient_t *c, int *dx, int *dy, int *dw, int *dh) {
    int cw = c->w, ch = c->h;
    int w = cw, h = cw * UI_H / UI_W;
    if (h > ch) { h = ch; w = ch * UI_W / UI_H; }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    *dw = w;
    *dh = h;
    *dx = (cw - w) / 2;
    *dy = (ch - h) / 2;
}

#define XMAP_MAX 4096
static int xmap[XMAP_MAX];

static void present(wclient_t *c) {
    uint32_t *dst = c->bufs[c->draw];
    int stride = c->cw;
    int dx, dy, dw, dh;
    layout(c, &dx, &dy, &dw, &dh);
    ui_dbg[8] = c->w;
    ui_dbg[9] = c->h;

    if (dw == UI_W && dh == UI_H && c->w == UI_W && c->h == UI_H) {
        for (int y = 0; y < UI_H; y++)
            memcpy(dst + (size_t)y * stride, ui_fb + (size_t)y * UI_W, UI_W * sizeof(uint32_t));
    } else {
        if (dw > XMAP_MAX) dw = XMAP_MAX;
        for (int x = 0; x < dw; x++) xmap[x] = x * UI_W / dw;
        for (int y = 0; y < c->h; y++) {
            uint32_t *row = dst + (size_t)y * stride;
            int sy = y - dy;
            if (sy < 0 || sy >= dh) {
                for (int x = 0; x < c->w; x++) row[x] = LETTERBOX;
                continue;
            }
            const uint32_t *src = ui_fb + (size_t)(sy * UI_H / dh) * UI_W;
            for (int x = 0; x < dx; x++) row[x] = LETTERBOX;
            for (int x = 0; x < dw; x++) row[dx + x] = src[xmap[x]];
            for (int x = dx + dw; x < c->w; x++) row[x] = LETTERBOX;
        }
    }
    wclient_present(c);
}

static void redraw(wclient_t *c) {
    uint32_t t0 = now_ms();
    ui_render();
    present(c);
    ui_dbg[11] = (int)(now_ms() - t0);
    ui_dbg[10]++;
}

/* ---- input ------------------------------------------------------------- */

static int left_prev;

static void on_mouse(wclient_t *c, const wm_event_t *ev) {
    int rx = (int)ev->a - 1, ry = (int)ev->b;      /* content-relative */
    int left = (int)(ev->c & 1);
    int dx, dy, dw, dh;
    layout(c, &dx, &dy, &dw, &dh);
    int x = (rx - dx) * UI_W / dw;
    int y = (ry - dy) * UI_H / dh;
    int type = 0;
    if (left && !left_prev) type = 1;
    else if (!left && left_prev) type = 2;
    left_prev = left;
    ui_dbg[type == 1 ? 1 : type == 2 ? 2 : 0]++;
    ui_dbg[4] = rx; ui_dbg[5] = ry;
    ui_dbg[6] = x;  ui_dbg[7] = y;
    ui_mouse(type, x, y);
}

/* SwirlOS key convention (ascii byte, or private 0x81..0x84 arrows) ->
 * the Windows virtual-key codes ui.c expects */
static int map_key(int k) {
    if (k >= 'a' && k <= 'z') return k - 'a' + 'A';
    switch (k) {
    case 0x81: return 38;                 /* up    */
    case 0x82: return 40;                 /* down  */
    case 0x83: return 37;                 /* left  */
    case 0x84: return 39;                 /* right */
    case '\n': case '\r': return 13;      /* enter */
    case '\b': case 127: return 8;        /* backspace */
    default: return k;                    /* Esc 27, Space 32, A-Z, digits */
    }
}

/* ---- main -------------------------------------------------------------- */

int main(void) {
    wclient_t c;
    if (wclient_open(&c, "Chess", UI_W, UI_H)) s_exit(1);
    c.win.state = 0;
    c.win.focused = 1;

    ui_init(now_ms, now_ms() * 2654435761u + 12345u);
    redraw(&c);

    /* engine self-test: one stage per frame, so a hang shows which stage */
    {
        int code = 0;
        for (int s = 1; s <= SELFTEST_STAGES; s++) {
            ui_engine_status(0, s, SELFTEST_STAGES);
            redraw(&c);
            code = selftest_run(s, now_ms);
            if (code) break;
        }
        st_state = code ? 2 : 1;
        st_code = code;
        ui_engine_status(st_state, st_code, 0);
        redraw(&c);
    }

    for (;;) {
        ui_dbg[13]++;
        wm_event_t ev;
        while (s_wm_poll_nb(&ev)) {
            switch (ev.type) {
            case WM_EVT_KEY: {
                int k = gkey(&ev);
                if (k) { ui_dbg[3]++; ui_key(map_key(k)); }
                break;
            }
            case WM_EVT_MOUSE:
                on_mouse(&c, &ev);
                break;
            case WM_EVT_FOCUS:
                c.win.focused = (int)ev.a;
                break;
            case WM_EVT_RESIZE:
                wclient_resize(&c, (int)ev.a - 2, (int)ev.b - (TITLE_H + 1));
                ui_engine_status(st_state, st_code, 0);     /* repaint, keep the status */
                break;
            case WM_EVT_CLOSE:
                s_exit(0);
                break;
            default: ;
            }
            if (!c.win.used) s_exit(0);
        }
        if (ui_step()) redraw(&c);
        else s_sleep(8);
    }
    return 0;
}
