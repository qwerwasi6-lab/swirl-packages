/* Clipboard History - the last 5 things you copied, newest on top.
 * Star an entry to keep it forever (saved to /home/user/Clipboard/starred.txt);
 * Copy puts an entry back on the system clipboard.
 *
 * SwirlOS has no "clipboard changed" event, so this app polls the kernel
 * clipboard (syscall 48) a few times a second. It only records while it is
 * running (minimised is fine). */
#include "game_common.h"

#define W_       460
#define H_       372
#define N        5            /* visible slots                              */
#define HEAD_H   44
#define ROW_P    64           /* row pitch                                  */
#define ROW_H    58
#define PAD      12
#define CB_W    64
#define CB_H    30
#define STAR_S   34
#define ENT_MAX  4096         /* bytes kept per entry (longer clips are cut) */
#define POLL_BUF 4096
#define DIR_     "/home/user/Clipboard"
#define FILE_    "/home/user/Clipboard/starred.txt"

typedef struct {
    int  used, starred;
    int  len;
    char text[ENT_MAX + 1];
} ent_t;

static ent_t ent[N];
static uint32_t last_hash;
static int  last_len = -1;
static int  mx = -1, my = -1;
static int  copied_slot = -1;
static uint32_t copied_until;
static uint32_t note_until;
static const char *note = "";
static char pbuf[POLL_BUF];

/* ---- tiny helpers (no libc assumptions) ---- */
static uint32_t fnv(const char *p, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= (uint8_t)p[i]; h *= 16777619u; }
    return h;
}
static void mcopy(void *d, const void *s, int n) {
    char *dd = d; const char *ss = s;
    for (int i = 0; i < n; i++) dd[i] = ss[i];
}
static void mmove_ent(int to, int from) { ent[to] = ent[from]; }

static void set_note(const char *s) { note = s; note_until = s_ticks_ms() + 2500; }

/* ---- persistence: one starred entry per line, \n and \\ escaped ---- */
static void save_starred(void) {
    static char out[N * (ENT_MAX * 2 + 4) + 16];
    int o = 0;
    for (int i = 0; i < N; i++) {
        if (!ent[i].used || !ent[i].starred) continue;
        for (int k = 0; k < ent[i].len; k++) {
            char c = ent[i].text[k];
            if (c == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
            else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
            else if (c == '\r') { /* drop */ }
            else out[o++] = c;
        }
        out[o++] = '\n';
    }
    s_mkdir("/home/user");
    s_mkdir(DIR_);
    int fd = s_open(FILE_, O_WRITE | O_CREATE | O_TRUNC);
    if (fd < 0) { set_note("Could not save stars"); return; }
    int done = 0;
    while (done < o) {
        int n = s_write(fd, out + done, o - done);
        if (n <= 0) break;
        done += n;
    }
    s_close(fd);
}

static void load_starred(void) {
    static char in[N * (ENT_MAX * 2 + 4) + 16];
    int fd = s_open(FILE_, O_READ);
    if (fd < 0) return;
    int n = 0;
    for (;;) {
        int r = s_read(fd, in + n, (int)sizeof in - 1 - n);
        if (r <= 0) break;
        n += r;
        if (n >= (int)sizeof in - 1) break;
    }
    s_close(fd);
    in[n] = 0;
    int slot = 0, p = 0;
    while (p < n && slot < N) {
        ent_t *e = &ent[slot];
        int l = 0;
        while (p < n && in[p] != '\n') {
            char c = in[p++];
            if (c == '\\' && p < n) {
                char d = in[p++];
                c = (d == 'n') ? '\n' : d;
            }
            if (l < ENT_MAX) e->text[l++] = c;
        }
        if (p < n) p++;                      /* skip the newline */
        if (l == 0) continue;
        e->text[l] = 0; e->len = l; e->used = 1; e->starred = 1;
        slot++;
    }
}

/* ---- history logic ---- */
static int find_same(const char *t, int len) {
    for (int i = 0; i < N; i++) {
        if (!ent[i].used || ent[i].len != len) continue;
        int same = 1;
        for (int k = 0; k < len; k++) if (ent[i].text[k] != t[k]) { same = 0; break; }
        if (same) return i;
    }
    return -1;
}

/* move slot i to the top, shifting everything above it down one */
static void bring_to_top(int i) {
    if (i == 0) return;
    static ent_t tmp;
    tmp = ent[i];
    for (int k = i; k > 0; k--) mmove_ent(k, k - 1);
    ent[0] = tmp;
}

static void push_clip(const char *t, int len) {
    int same = find_same(t, len);
    if (same >= 0) { bring_to_top(same); return; }
    /* need a free or evictable slot: empty first, else the OLDEST unstarred */
    int victim = -1;
    for (int i = 0; i < N; i++) if (!ent[i].used) { victim = i; break; }
    if (victim < 0) for (int i = N - 1; i >= 0; i--) if (!ent[i].starred) { victim = i; break; }
    if (victim < 0) { set_note("All 5 are starred - unstar one to record more"); return; }
    for (int k = victim; k > 0; k--) mmove_ent(k, k - 1);
    ent_t *e = &ent[0];
    e->used = 1; e->starred = 0;
    e->len = len > ENT_MAX ? ENT_MAX : len;
    mcopy(e->text, t, e->len);
    e->text[e->len] = 0;
}

static void poll_clip(void) {
    int real = s_clip_get(pbuf, POLL_BUF);
    if (real <= 0) return;
    int n = real > POLL_BUF ? POLL_BUF : real;
    uint32_t h = fnv(pbuf, n) ^ (uint32_t)real * 31u;
    if (real == last_len && h == last_hash) return;
    last_len = real; last_hash = h;
    /* ignore all-whitespace clips */
    int has = 0;
    for (int i = 0; i < n; i++) if (pbuf[i] != ' ' && pbuf[i] != '\n' && pbuf[i] != '\t' && pbuf[i] != '\r') { has = 1; break; }
    if (!has) return;
    push_clip(pbuf, n);
    /* a pushed duplicate of a starred entry changes starred ORDER only; keep file in sync */
    save_starred();
}

static void do_copy(int i) {
    if (!ent[i].used) return;
    s_clip_set(ent[i].text, ent[i].len);
    /* remember it so the poller does not record our own copy as new */
    last_len = ent[i].len;
    last_hash = fnv(ent[i].text, ent[i].len) ^ (uint32_t)ent[i].len * 31u;
    copied_slot = i;
    copied_until = s_ticks_ms() + 1100;
}

static void toggle_star(int i) {
    if (!ent[i].used) return;
    ent[i].starred = !ent[i].starred;
    save_starred();
    set_note(ent[i].starred ? "Starred - saved permanently" : "Unstarred - will expire");
}

/* ---- layout ---- */
static int star_x(void)  { return W_ - PAD - CB_W - 8 - STAR_S; }
static int copy_x(void)  { return W_ - PAD - CB_W; }
static int box_w(void)   { return star_x() - 10 - PAD; }
static int row_y(int i)  { return HEAD_H + i * ROW_P; }

/* ---- star drawing: filled with even-odd scanlines, or an outline ---- */
static void star_pts(int cx, int cy, int R, int *px, int *py) {
    /* 10 vertices, point up. Outer k at angle 72k, inner k at 36+72k
     * (clockwise from the top; x = sin, y = -cos), inner radius 0.42 R. */
    static const int osn[5] = {    0,  951,  588, -588, -951 };
    static const int ocs[5] = { 1000,  309, -809, -809,  309 };
    static const int isn[5] = {  588,  951,    0, -951, -588 };
    static const int ics[5] = {  809, -309,-1000, -309,  809 };
    for (int k = 0; k < 5; k++) {
        px[2 * k]     = cx + osn[k] * R / 1000;
        py[2 * k]     = cy - ocs[k] * R / 1000;
        px[2 * k + 1] = cx + isn[k] * R * 42 / 100000;
        py[2 * k + 1] = cy - ics[k] * R * 42 / 100000;
    }
}

static void draw_star(int cx, int cy, int R, uint32_t col, int filled) {
    int px[10], py[10];
    star_pts(cx, cy, R, px, py);
    if (!filled) {
        for (int k = 0; k < 10; k++) {
            int a = (k + 1) % 10;
            gfx_line_aa(px[k], py[k], px[a], py[a], col, 255);
        }
        return;
    }
    for (int y = cy - R; y <= cy + R; y++) {
        int xs[10], nx = 0;
        for (int k = 0; k < 10; k++) {
            int a = (k + 1) % 10;
            int y0 = py[k], y1 = py[a];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int x = px[k] + (y - y0) * (px[a] - px[k]) / (y1 - y0);
                int j = nx++;
                while (j > 0 && xs[j - 1] > x) { xs[j] = xs[j - 1]; j--; }
                xs[j] = x;
            }
        }
        for (int k = 0; k + 1 < nx; k += 2) gfx_rect(xs[k], y, xs[k + 1] - xs[k] + 1, 1, col);
    }
    for (int k = 0; k < 10; k++) {
        int a = (k + 1) % 10;
        gfx_line_aa(px[k], py[k], px[a], py[a], col, 255);
    }
}

/* ---- text wrapping into two lines inside the box ---- */
static int utf8_start(char c) { return ((uint8_t)c & 0xC0) != 0x80; }

/* copy up to 'cap' chars of the entry into out with whitespace runs squashed */
static int flatten(const ent_t *e, char *out, int cap) {
    int o = 0, sp = 1;
    for (int i = 0; i < e->len && o < cap - 1; i++) {
        char c = e->text[i];
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (!sp) { out[o++] = ' '; sp = 1; }
        } else { out[o++] = c; sp = 0; }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
    return o;
}

/* byte length of the longest prefix of s (up to max bytes) that fits in maxw */
static int fit_prefix(const char *s, int max, int maxw, int size) {
    char tmp[200];
    int best = 0;
    if (max > (int)sizeof tmp - 1) max = (int)sizeof tmp - 1;
    for (int i = 1; i <= max; i++) {
        if (i < max && !utf8_start(s[i])) continue;   /* only cut on a char start */
        mcopy(tmp, s, i); tmp[i] = 0;
        if (text_w(tmp, size) > maxw) break;
        best = i;
    }
    return best;
}

static void draw_entry_text(int i, int x, int y, int w, int h) {
    uint32_t ink = app_ink, dim = app_ink_faint;
    if (!ent[i].used) {
        ltext(x + 12, y + h / 2, "empty", 13, dim, 0);
        return;
    }
    char flat[220];
    int n = flatten(&ent[i], flat, sizeof flat);
    int inner = w - 24;
    int l1 = fit_prefix(flat, n, inner, 13);
    if (l1 <= 0) l1 = n > 0 ? 1 : 0;
    char a[210], b[210];
    mcopy(a, flat, l1); a[l1] = 0;
    /* break at the last space when we are mid-word and have a second line */
    int cut = l1;
    if (cut < n) {
        int sp = -1;
        for (int k = cut; k > cut / 2; k--) if (flat[k] == ' ') { sp = k; break; }
        if (sp > 0) { cut = sp; a[cut] = 0; }
    }
    int rest = n - cut;
    const char *r = flat + cut;
    while (*r == ' ') { r++; rest--; }
    int has2 = rest > 0;
    if (!has2) {
        ltext(x + 12, y + h / 2, a, 13, ink, 0);
        return;
    }
    mcopy(b, r, rest); b[rest] = 0;
    int l2 = fit_prefix(b, rest, inner, 13);
    if (l2 < rest) {                               /* too long: end with ... */
        int l3 = fit_prefix(b, rest, inner - text_w("...", 13), 13);
        b[l3] = '.'; b[l3 + 1] = '.'; b[l3 + 2] = '.'; b[l3 + 3] = 0;
    }
    ltext(x + 12, y + h / 2 - 10, a, 13, ink, 0);
    ltext(x + 12, y + h / 2 + 10, b, 13, ink, 0);
}

/* ---- draw ---- */
static void draw(int w, int h) {
    gfx_rect(0, 0, w, h, app_bg);

    /* header */
    gfx_rect(0, 0, w, HEAD_H - 4, app_bg2);
    ltext(PAD, 20, "Clipboard History", 16, app_ink, 1);
    int stars = 0;
    for (int i = 0; i < N; i++) stars += ent[i].used && ent[i].starred;
    char info[48];
    snprintf(info, sizeof info, "%d starred", stars);
    rtext(w - PAD, 20, info, 12, app_ink_dim, 0);

    for (int i = 0; i < N; i++) {
        int y = row_y(i) + 3;
        int bw = box_w();
        /* text box */
        gfx_round_fill(PAD, y, bw, ROW_H, 10, app_card);
        gfx_round_outline(PAD, y, bw, ROW_H, 10, ent[i].starred ? UI_ACCENT : app_line);
        gfx_clip_save();
        gfx_clip(PAD + 2, y + 2, bw - 4, ROW_H - 4);
        draw_entry_text(i, PAD, y, bw, ROW_H);
        gfx_clip_restore();

        int used = ent[i].used;

        /* star button */
        int sx = star_x(), sy = y + (ROW_H - STAR_S) / 2;
        int hs = used && in_rect(mx, my, sx, sy, STAR_S, STAR_S);
        gfx_round_fill(sx, sy, STAR_S, STAR_S, STAR_S / 2, hs ? app_field : app_card);
        gfx_round_outline(sx, sy, STAR_S, STAR_S, STAR_S / 2, app_line);
        uint32_t scol = !used ? app_ink_faint : (ent[i].starred ? 0xF5B301 : app_ink_dim);
        draw_star(sx + STAR_S / 2, sy + STAR_S / 2 + 1, 10, scol, used && ent[i].starred);

        /* copy button */
        int cx = copy_x(), cy = y + (ROW_H - CB_H) / 2;
        int hc = used && in_rect(mx, my, cx, cy, CB_W, CB_H);
        int just = (copied_slot == i && s_ticks_ms() < copied_until);
        if (!used) {
            gfx_round_fill(cx, cy, CB_W, CB_H, CB_H / 2, app_card);
            gfx_round_outline(cx, cy, CB_W, CB_H, CB_H / 2, app_line);
            ctext(cx + CB_W / 2, cy + CB_H / 2, "Copy", 13, app_ink_faint, 1);
        } else if (just) {
            gfx_round_fill(cx, cy, CB_W, CB_H, CB_H / 2, 0x2FA866);
            ctext(cx + CB_W / 2, cy + CB_H / 2, "Copied", 13, 0xFFFFFF, 1);
        } else {
            gbutton(cx, cy, CB_W, CB_H, "Copy", 1, hc);
        }
    }

    /* footer note */
    if (s_ticks_ms() < note_until)
        ctext(w / 2, h - 12, note, 12, app_ink_dim, 0);
    else
        ctext(w / 2, h - 12, "Star an entry to keep it. Unstarred ones expire after 5 newer copies.", 11, app_ink_faint, 0);
}

static void tick(void) { poll_clip(); }

static void mouse(int x, int y, int buttons, int press) {
    (void)buttons;
    mx = x; my = y;
    if (!press) return;
    for (int i = 0; i < N; i++) {
        int yy = row_y(i) + 3;
        int sx = star_x(), sy = yy + (ROW_H - STAR_S) / 2;
        int cx = copy_x(), cy = yy + (ROW_H - CB_H) / 2;
        if (in_rect(x, y, sx, sy, STAR_S, STAR_S)) { toggle_star(i); return; }
        if (in_rect(x, y, cx, cy, CB_W, CB_H))   { do_copy(i);     return; }
    }
}

static void init(void) {
    load_starred();
    poll_clip();
}

int main(void) {
    static const game_t g = {
        "Clipboard History", W_, H_, 150,
        init, tick, draw, 0, mouse
    };
    game_run(&g);
    return 0;
}
