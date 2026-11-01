/* Swirl Checkers: English draughts. Rules, AI and software-rendered UI in one portable file. */
#include "ui.h"
#include "assets.h"

u32 ui_fb[UI_W * UI_H];

/* ------------------------------------------------------------- palette */
#define C_BG 0x302E2B
#define C_PANEL 0x262421
#define C_CARD 0x211F1C
#define C_TEXT 0xE8E6E3
#define C_MUTED 0x9B9894
#define C_GREEN 0x81B64C
#define C_GREEN_HOV 0x95C85E
#define C_BTN 0x3D3A37
#define C_BTN_HOV 0x4C4946
#define C_LIGHT 0xEEEED2
#define C_DARK 0x769656

#define BX 24
#define BY 44
#define SQ 64
#define PX 568          /* panel x */
#define PW 248          /* panel width */

/* ----------------------------------------------------------- primitives */
#define DIV255(v) ((((v) + 128) * 257) >> 16)

static inline u32 blend_px(u32 d, u32 s, int a) {
    if (a <= 0) return d;
    if (a >= 255) return s;
    int ia = 255 - a;
    int r = DIV255(((d >> 16) & 255) * ia + ((s >> 16) & 255) * a);
    int g = DIV255(((d >> 8) & 255) * ia + ((s >> 8) & 255) * a);
    int b = DIV255((d & 255) * ia + (s & 255) * a);
    return ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static inline void put(int x, int y, u32 col, int a) {
    if ((unsigned)x >= UI_W || (unsigned)y >= UI_H) return;
    u32 *p = &ui_fb[y * UI_W + x];
    *p = blend_px(*p, col, a);
}

static void fill_rect(int x, int y, int w, int h, u32 col) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > UI_W ? UI_W : x + w, y1 = y + h > UI_H ? UI_H : y + h;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = &ui_fb[yy * UI_W + x0];
        for (int xx = x0; xx < x1; xx++) *p++ = col;
    }
}

static void blend_rect(int x, int y, int w, int h, u32 col, int a) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) put(xx, yy, col, a);
}

/* coverage of a disc / ring by 4x4 supersampling; centre and radii in quarter pixels */
static int cover(int px, int py, int cx4, int cy4, int ro4, int ri4) {
    int cnt = 0;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            int dx = px * 4 + i - cx4, dy = py * 4 + j - cy4;
            int d2 = dx * dx + dy * dy;
            if (d2 <= ro4 * ro4 && d2 >= ri4 * ri4) cnt++;
        }
    return cnt * 255 / 16;
}

static void disc(int cx, int cy, int r, int rin, u32 col, int alpha) {
    int cx4 = cx * 4 + 2, cy4 = cy * 4 + 2;
    for (int y = cy - r - 1; y <= cy + r + 1; y++)
        for (int x = cx - r - 1; x <= cx + r + 1; x++) {
            int c = cover(x, y, cx4, cy4, r * 4 + 2, rin > 0 ? rin * 4 : 0);
            if (c) put(x, y, col, c * alpha / 255);
        }
}

static void fill_rrect(int x, int y, int w, int h, int r, u32 col) {
    fill_rect(x + r, y, w - 2 * r, h, col);
    fill_rect(x, y + r, r, h - 2 * r, col);
    fill_rect(x + w - r, y + r, r, h - 2 * r, col);
    int cs[4][2] = {{x + r, y + r}, {x + w - r, y + r}, {x + r, y + h - r}, {x + w - r, y + h - r}};
    int ox[4] = {x, x + w - r, x, x + w - r};
    int oy[4] = {y, y, y + h - r, y + h - r};
    for (int k = 0; k < 4; k++)
        for (int yy = oy[k]; yy < oy[k] + r; yy++)
            for (int xx = ox[k]; xx < ox[k] + r; xx++) {
                int c = cover(xx, yy, cs[k][0] * 4, cs[k][1] * 4, r * 4, 0);
                if (c) put(xx, yy, col, c);
            }
}

/* -------------------------------------------------------------- text */
static int text_w(int font, const char *s) {
    const FontInfo *F = &g_fonts[font];
    int w8 = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 126) continue;
        w8 += F->g[c - 32].adv8;
    }
    return (w8 + 4) / 8;
}

static void text(int font, int x, int base, u32 col, const char *s) {
    const FontInfo *F = &g_fonts[font];
    int pen8 = x * 8;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 126) continue;
        const Glyph *g = &F->g[c - 32];
        int gx = (pen8 + 4) / 8 + g->xoff, gy = base + g->yoff;
        const unsigned char *d = F->data + g->off;
        for (int yy = 0; yy < g->h; yy++)
            for (int xx = 0; xx < g->w; xx++) {
                int a = d[yy * g->w + xx];
                if (a) put(gx + xx, gy + yy, col, a);
            }
        pen8 += g->adv8;
    }
}

static void text_c(int font, int cx, int base, u32 col, const char *s) {
    text(font, cx - text_w(font, s) / 2, base, col, s);
}

static int put_int(char *o, int v) {
    char t[12];
    int n = 0, k = 0;
    if (v < 0) { o[k++] = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) o[k++] = t[--n];
    o[k] = 0;
    return k;
}

/* =================================================================== rules */
#define WHITE 0
#define BLACK 1
#define MAXM 64
#define NOMV 0xFF
#define MAXH 600
#define DRAW_PLIES 80           /* 40 moves each without a capture or a man move */

typedef struct { i8 b[64]; u8 side; } Pos;      /* b: 1 man, 2 king (white +, black -) */
typedef struct { u8 n; u8 sq[13]; u8 cap[12]; } Move;   /* n hops: sq[0] -> sq[n]; cap[i] taken on hop i */

static const i8 DF[4] = {-1, 1, -1, 1};
static const i8 DR[4] = {1, 1, -1, -1};

static int dir_ok(int v, int d) { if (v == 2 || v == -2) return 1; return v > 0 ? d < 2 : d >= 2; }
static int own(const Pos *p, int sq) { int v = p->b[sq]; return p->side == WHITE ? v > 0 : v < 0; }
static void emit(Move *out, int *cnt, const Move *m) { if (*cnt < MAXM) out[(*cnt)++] = *m; }

static void dfs(const Pos *p, u8 *taken, int cur, int v, Move *m, Move *out, int *cnt) {
    int found = 0, r = cur >> 3, f = cur & 7;
    for (int d = 0; d < 4; d++) {
        if (!dir_ok(v, d)) continue;
        int r2 = r + 2 * DR[d], f2 = f + 2 * DF[d];
        if (r2 < 0 || r2 > 7 || f2 < 0 || f2 > 7) continue;
        int s1 = (r + DR[d]) * 8 + f + DF[d], s2 = r2 * 8 + f2;
        int ov = p->b[s1];
        if (taken[s1] || !ov || (ov > 0) == (v > 0)) continue;
        if (p->b[s2] && s2 != m->sq[0]) continue;
        found = 1;
        taken[s1] = 1;
        m->cap[m->n] = (u8)s1;
        m->n++;
        m->sq[m->n] = (u8)s2;
        if ((v == 1 && r2 == 7) || (v == -1 && r2 == 0)) emit(out, cnt, m);   /* crowned: turn ends */
        else dfs(p, taken, s2, v, m, out, cnt);
        m->n--;
        taken[s1] = 0;
    }
    if (!found && m->n > 0) emit(out, cnt, m);
}

/* all legal moves; captures are mandatory, so if any jump exists only jumps are returned */
static int gen_moves(const Pos *p, Move *out) {
    int cnt = 0;
    u8 taken[64];
    Move m;
    for (int i = 0; i < 64; i++) taken[i] = 0;
    m.n = 0;
    for (int sq = 0; sq < 64; sq++) {
        if (!own(p, sq)) continue;
        m.n = 0;
        m.sq[0] = (u8)sq;
        dfs(p, taken, sq, p->b[sq], &m, out, &cnt);
    }
    if (cnt) return cnt;
    for (int sq = 0; sq < 64; sq++) {
        if (!own(p, sq)) continue;
        int v = p->b[sq], r = sq >> 3, f = sq & 7;
        for (int d = 0; d < 4; d++) {
            if (!dir_ok(v, d)) continue;
            int r1 = r + DR[d], f1 = f + DF[d];
            if (r1 < 0 || r1 > 7 || f1 < 0 || f1 > 7 || p->b[r1 * 8 + f1]) continue;
            m.n = 1;
            m.sq[0] = (u8)sq;
            m.sq[1] = (u8)(r1 * 8 + f1);
            m.cap[0] = NOMV;
            emit(out, &cnt, &m);
        }
    }
    return cnt;
}

static void apply(Pos *p, const Move *m) {
    int v = p->b[m->sq[0]], to = m->sq[m->n];
    p->b[m->sq[0]] = 0;
    for (int i = 0; i < m->n; i++) if (m->cap[i] != NOMV) p->b[m->cap[i]] = 0;
    if (v == 1 && (to >> 3) == 7) v = 2;
    if (v == -1 && (to >> 3) == 0) v = -2;
    p->b[to] = (i8)v;
    p->side ^= 1;
}

static void setup(Pos *p) {
    for (int i = 0; i < 64; i++) p->b[i] = 0;
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++) {
            if (((r + f) & 1) != 0) continue;
            if (r < 3) p->b[r * 8 + f] = 1;
            else if (r > 4) p->b[r * 8 + f] = -1;
        }
    p->side = WHITE;
}

/* ===================================================================== AI */
static int eval(const Pos *p) {
    int s = 0;
    for (int sq = 0; sq < 64; sq++) {
        int v = p->b[sq];
        if (!v) continue;
        int r = sq >> 3, f = sq & 7, a = v > 0 ? v : -v, val;
        int cen = (f >= 2 && f <= 5 && r >= 2 && r <= 5) ? 3 : 0;
        if (a == 2) val = 170 + cen;
        else {
            int adv = v > 0 ? r : 7 - r;
            val = 100 + adv * 4 + cen + (adv == 0 ? 9 : 0) + ((f == 0 || f == 7) ? -2 : 0);
        }
        s += v > 0 ? val : -val;
    }
    return p->side == WHITE ? s : -s;
}

static int search(const Pos *p, int depth, int alpha, int beta, int ply) {
    Move ms[MAXM];
    int n = gen_moves(p, ms);
    if (!n) return -20000 + ply;
    if (depth <= 0 && !(ms[0].cap[0] != NOMV && ply < 24)) return eval(p);   /* play out captures */
    int best = -30000;
    for (int i = 0; i < n; i++) {
        Pos q = *p;
        apply(&q, &ms[i]);
        int s = -search(&q, depth - 1, -beta, -alpha, ply + 1);
        if (s > best) best = s;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

static u32 rs = 2463534242u;
static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static int ai_choose(const Pos *p, const Move *ms, int n, int level) {
    static const int DEPTH[5] = {0, 1, 3, 5, 7};
    static const int NOISE[5] = {0, 70, 18, 3, 0};
    if (n <= 1) return 0;
    int best = -30000, bi = 0;
    for (int i = 0; i < n; i++) {
        Pos q = *p;
        apply(&q, &ms[i]);
        int s = NOISE[level] > 3 ? -search(&q, DEPTH[level] - 1, -30000, 30000, 1)
                                 : -search(&q, DEPTH[level] - 1, -30000, -best, 1);
        if (NOISE[level]) s += (int)(rnd() % (u32)NOISE[level]);
        if (s > best) { best = s; bi = i; }
    }
    return bi;
}

/* ==================================================================== game */
static Pos hist[MAXH + 1];
static Move mvs[MAXH];
static char san[MAXH][30];
static int noprog[MAXH + 1];
static int hn;                  /* moves played; hist[hn] is the current position */
static int result;              /* 0 playing, 1 white wins, 2 black wins, 3 draw */

static TimeFn g_now;
static int mode;                /* 0 you=white vs CPU, 1 you=black vs CPU, 2 two players */
static int level = 3;
static int flipped;
static int sel = -1, dragging, mx, my;
static Move legal[MAXM];
static int nlegal;
static int last_from = -1, last_to = -1;
static int hint_from = -1, hint_to = -1;
static char hint_san[30];
static int ai_shown, dirty = 1, hover = -1;
static int kcur = 17, kb_active;

#define SEG_MS 170
static struct { int on; u32 t0; Pos pre; Move m; } anim;
static int skip_anim;

static const char *LEVEL_NAME[5] = {"", "Beginner", "Casual", "Club", "Strong"};

static const Pos *cur(void) { return &hist[hn]; }

static void refresh_legal(void) {
    nlegal = gen_moves(cur(), legal);
    if (hn >= MAXH - 2 || noprog[hn] >= DRAW_PLIES) result = 3;
    else if (!nlegal) result = cur()->side == WHITE ? 2 : 1;
    else result = 0;
}

static int human_turn(void) {
    if (result) return 0;
    if (mode == 2) return 1;
    return cur()->side == (mode == 0 ? WHITE : BLACK);
}
static int ai_turn(void) { return !result && !human_turn(); }

static void clear_sel(void) { sel = -1; dragging = 0; skip_anim = 0; }

static void new_game(void) {
    hn = 0;
    setup(&hist[0]);
    noprog[0] = 0;
    anim.on = 0;
    flipped = (mode == 1);
    clear_sel();
    last_from = last_to = hint_from = hint_to = -1;
    ai_shown = 0;
    refresh_legal();
    dirty = 1;
}

static char *put_sq(char *o, int sq) { *o++ = (char)('a' + (sq & 7)); *o++ = (char)('1' + (sq >> 3)); return o; }

static void move_text(const Move *m, char *o) {
    o = put_sq(o, m->sq[0]);
    for (int i = 1; i <= m->n; i++) { *o++ = m->cap[0] != NOMV ? 'x' : '-'; o = put_sq(o, m->sq[i]); }
    *o = 0;
}

static void play(int i) {
    if (hn >= MAXH - 1) return;
    Move m = legal[i];
    Pos pre = hist[hn], q = pre;
    int pv = pre.b[m.sq[0]];
    apply(&q, &m);
    mvs[hn] = m;
    move_text(&m, san[hn]);
    noprog[hn + 1] = (m.cap[0] != NOMV || pv == 1 || pv == -1) ? 0 : noprog[hn] + 1;
    hist[hn + 1] = q;
    hn++;
    anim.on = 0;
    if (g_now && !skip_anim) { anim.pre = pre; anim.m = m; anim.t0 = g_now(); anim.on = 1; }
    last_from = m.sq[0];
    last_to = m.sq[m.n];
    hint_from = hint_to = -1;
    clear_sel();
    refresh_legal();
    dirty = 1;
}

static void do_undo(void) {
    int minp = (mode == 1) ? 1 : 0;
    if (hn <= minp) return;
    anim.on = 0;
    if (mode == 2) hn--;
    else { int hs = mode == 0 ? WHITE : BLACK; do hn--; while (hn > minp && hist[hn].side != hs); }
    if (hn > 0) { last_from = mvs[hn - 1].sq[0]; last_to = mvs[hn - 1].sq[mvs[hn - 1].n]; }
    else last_from = last_to = -1;
    hint_from = hint_to = -1;
    clear_sel();
    refresh_legal();
    dirty = 1;
}

static void do_hint(void) {
    if (!human_turn()) return;
    int i = ai_choose(cur(), legal, nlegal, 3);
    hint_from = legal[i].sq[0];
    hint_to = legal[i].sq[legal[i].n];
    move_text(&legal[i], hint_san);
    dirty = 1;
}

/* ================================================================ geometry */
enum { B_NEW, B_UNDO, B_FLIP, B_HINT, B_MODE, B_LEVEL, B_COUNT };
static void btn_rect(int id, int *x, int *y, int *w, int *h) {
    static const short R[B_COUNT][4] = {
        {PX, 148, 118, 38}, {PX + 130, 148, 118, 38},
        {PX, 196, 118, 38}, {PX + 130, 196, 118, 38},
        {PX, 244, 248, 38}, {PX, 292, 248, 38}};
    *x = R[id][0]; *y = R[id][1]; *w = R[id][2]; *h = R[id][3];
}
static int btn_at(int x, int y) {
    for (int i = 0; i < B_COUNT; i++) {
        int bx, by, bw, bh;
        btn_rect(i, &bx, &by, &bw, &bh);
        if (x >= bx && y >= by && x < bx + bw && y < by + bh) return i;
    }
    return -1;
}
static int sq_x(int sq) { int f = sq & 7; if (flipped) f = 7 - f; return BX + f * SQ; }
static int sq_y(int sq) { int r = sq >> 3; if (!flipped) r = 7 - r; return BY + r * SQ; }
static int sq_at(int x, int y) {
    if (x < BX || y < BY) return -1;
    int f = (x - BX) / SQ, r = (y - BY) / SQ;
    if (f > 7 || r > 7) return -1;
    if (flipped) f = 7 - f; else r = 7 - r;
    return r * 8 + f;
}
static int movable(int sq) {
    for (int i = 0; i < nlegal; i++) if (legal[i].sq[0] == sq) return 1;
    return 0;
}
static int try_move(int from, int to) {
    for (int i = 0; i < nlegal; i++)
        if (legal[i].sq[0] == from && legal[i].sq[legal[i].n] == to) { play(i); return 1; }
    return 0;
}
static int over_banner_button(int x, int y) {
    int bx = BX + 106 + 75, by = BY + 186 + 86;
    return x >= bx && x < bx + 150 && y >= by && y < by + 40;
}

/* ================================================================== input */
void ui_mouse(int type, int x, int y) {
    mx = x;
    my = y;
    if (type == 0) {
        int h = btn_at(x, y);
        if (result && over_banner_button(x, y)) h = 100;
        if (h != hover) { hover = h; dirty = 1; }
        if (dragging) dirty = 1;
        return;
    }
    if (type == 1) {
        if (result && over_banner_button(x, y)) { new_game(); return; }
        int b = btn_at(x, y);
        if (b >= 0) {
            switch (b) {
                case B_NEW: new_game(); break;
                case B_UNDO: do_undo(); break;
                case B_FLIP: flipped ^= 1; dirty = 1; break;
                case B_HINT: do_hint(); break;
                case B_MODE: mode = (mode + 1) % 3; new_game(); break;
                case B_LEVEL: level = level % 4 + 1; dirty = 1; break;
            }
            return;
        }
        if (!human_turn()) return;
        int sq = sq_at(x, y);
        if (sq < 0) { clear_sel(); dirty = 1; return; }
        if (sel >= 0 && sel != sq && try_move(sel, sq)) return;
        if (movable(sq)) { sel = sq; dragging = 1; hint_from = hint_to = -1; }
        else clear_sel();
        dirty = 1;
        return;
    }
    if (type == 2) {
        if (!dragging) return;
        dragging = 0;
        dirty = 1;
        int sq = sq_at(x, y);
        if (sq < 0) { sel = -1; return; }
        if (sq == sel) return;
        skip_anim = 1;
        if (try_move(sel, sq)) return;
        skip_anim = 0;
        sel = movable(sq) ? sq : -1;
    }
}

static void kmove(int dc, int dr) {
    int f = kcur & 7, r = kcur >> 3;
    int col = flipped ? 7 - f : f, row = flipped ? r : 7 - r;
    col += dc; row += dr;
    if (col < 0) col = 0;
    if (col > 7) col = 7;
    if (row < 0) row = 0;
    if (row > 7) row = 7;
    f = flipped ? 7 - col : col;
    r = flipped ? row : 7 - row;
    kcur = r * 8 + f;
    kb_active = 1;
    dirty = 1;
}

static void kselect(void) {
    kb_active = 1;
    dirty = 1;
    if (!human_turn()) return;
    if (sel >= 0 && sel != kcur && try_move(sel, kcur)) return;
    if (sel == kcur) { clear_sel(); return; }
    dragging = 0;
    if (movable(kcur)) { sel = kcur; hint_from = hint_to = -1; }
    else clear_sel();
}

void ui_key(int vk) {
    switch (vk) {
        case 37: kmove(-1, 0); return;
        case 39: kmove(1, 0); return;
        case 38: kmove(0, -1); return;
        case 40: kmove(0, 1); return;
        case 13: case 32: kselect(); return;
        case 'N': new_game(); break;
        case 'U': case 8: do_undo(); break;
        case 'F': flipped ^= 1; dirty = 1; break;
        case 'H': do_hint(); break;
        case 27: clear_sel(); dirty = 1; break;
        case 'L': level = level % 4 + 1; dirty = 1; break;
    }
}

static int seg_now(int *seg) {      /* 0..1024 inside the current hop; returns 0 when finished */
    u32 t = g_now ? g_now() - anim.t0 : 0xFFFFFF;
    int s = (int)(t / SEG_MS);
    if (s >= anim.m.n) return 0;
    int q = 1024 - (int)((t % SEG_MS) * 1024 / SEG_MS);
    *seg = s;
    return 1024 - ((q * q >> 10) * q >> 10) + 1;     /* +1 so "running" is never 0 */
}

int ui_step(void) {
    if (anim.on) {
        int s;
        if (!seg_now(&s)) anim.on = 0;
        dirty = 0;
        return 1;
    }
    if (ai_turn()) {
        if (!ai_shown) { ai_shown = 1; dirty = 0; return 1; }
        ai_shown = 0;
        play(ai_choose(cur(), legal, nlegal, level));
        dirty = 0;
        return 1;
    }
    ai_shown = 0;
    int d = dirty;
    dirty = 0;
    return d;
}

void ui_init(TimeFn now, u32 seed) {
    g_now = now;
    rs = seed ? seed : 2463534242u;
    mode = 0;
    level = 3;
    new_game();
}

/* ================================================================== render */
static void draw_man(int cx, int cy, int R, int v, int detail) {
    int a = v > 0 ? v : -v;
    u32 body = v > 0 ? 0xF0EDE0 : 0x37373D, rim = v > 0 ? 0xBDB8A2 : 0x141418, groove = v > 0 ? 0xD3CDB4 : 0x55555D;
    disc(cx + 1, cy + (detail ? 3 : 1), R, 0, 0x000000, 70);
    disc(cx, cy, R, 0, rim, 255);
    disc(cx, cy, R - (detail ? 3 : 2), 0, body, 255);
    if (detail) disc(cx, cy, R - 9, R - 11, groove, 255);
    if (a == 2 && detail) {
        disc(cx, cy, R / 2, R / 2 - 3, 0xE3B341, 255);
        disc(cx, cy, R / 6 + 1, 0, 0xE3B341, 255);
    }
}

static const char *name_of(int side, char *buf) {
    char *o = buf;
    const char *s;
    int cpu = (mode == 0 && side == BLACK) || (mode == 1 && side == WHITE);
    if (mode == 2) s = side == WHITE ? "White" : "Black";
    else s = cpu ? "Computer" : "You";
    while (*s) *o++ = *s++;
    if (cpu) {
        *o++ = ' '; *o++ = '(';
        s = LEVEL_NAME[level];
        while (*s) *o++ = *s++;
        *o++ = ')';
    }
    *o = 0;
    return buf;
}

/* player name + the enemy pieces this side has captured, in a strip at y */
static void draw_strip(int side, int y) {
    const Pos *p = cur();
    int have[2] = {0, 0}, kings[2] = {0, 0};
    for (int i = 0; i < 64; i++) {
        int v = p->b[i];
        if (!v) continue;
        int c = v > 0 ? 0 : 1;
        have[c]++;
        if (v == 2 || v == -2) kings[c]++;
    }
    char nb[48];
    name_of(side, nb);
    disc(BX + 5, y + 21, 5, 0, side == WHITE ? 0xF0F0F0 : 0x1A1A1A, 255);
    if (side == BLACK) disc(BX + 5, y + 21, 5, 4, 0x8A8A8A, 255);
    text(1, BX + 18, y + 27, C_TEXT, nb);
    int x = BX + 18 + text_w(1, nb) + 18;
    int enemy = side ^ 1;
    int taken = 12 - have[enemy];
    for (int k = 0; k < taken; k++) { draw_man(x + 8, y + 21, 8, enemy == WHITE ? 1 : -1, 0); x += 13; }
    int lead = have[side] - have[enemy];
    if (lead > 0) {
        char b[12] = "+";
        put_int(b + 1, lead);
        text(0, x + 6, y + 27, C_MUTED, b);
    }
    (void)kings;
}

static void draw_button(int id, const char *label, int primary) {
    int x, y, w, h;
    btn_rect(id, &x, &y, &w, &h);
    int hov = hover == id;
    u32 col = primary ? (hov ? C_GREEN_HOV : C_GREEN) : (hov ? C_BTN_HOV : C_BTN);
    fill_rrect(x, y, w, h, 7, col);
    text_c(1, x + w / 2, y + 25, primary ? 0x1B2410 : C_TEXT, label);
}

static void status_text(const char **l1, const char **l2) {
    static char s2[48];
    *l2 = "";
    switch (result) {
        case 1: *l1 = "White wins"; *l2 = "Black has no moves"; return;
        case 2: *l1 = "Black wins"; *l2 = "White has no moves"; return;
        case 3: *l1 = "Draw"; *l2 = "40 moves without a capture"; return;
    }
    if (ai_turn()) *l1 = "Computer is thinking...";
    else if (mode == 2) *l1 = cur()->side == WHITE ? "White to move" : "Black to move";
    else *l1 = "Your move";
    if (hint_from >= 0) {
        char *o = s2;
        const char *h = "Hint: ";
        while (*h) *o++ = *h++;
        for (const char *q = hint_san; *q; q++) *o++ = *q;
        *o = 0;
        *l2 = s2;
    } else if (nlegal && legal[0].cap[0] != NOMV) *l2 = "Capture is required";
    else if (mode == 2) *l2 = "Two players";
    else *l2 = LEVEL_NAME[level];
}

void ui_render(void) {
    fill_rect(0, 0, UI_W, UI_H, C_BG);
    int top_side = flipped ? WHITE : BLACK, bot_side = flipped ? BLACK : WHITE;
    draw_strip(top_side, 0);
    draw_strip(bot_side, BY + 8 * SQ);

    const Pos *p = anim.on ? &anim.pre : cur();
    int aseg = 0, e = 1024;
    if (anim.on) { e = seg_now(&aseg); if (!e) { e = 1024; aseg = anim.m.n - 1; } }

    for (int sq = 0; sq < 64; sq++) {
        int x = sq_x(sq), y = sq_y(sq), r = sq >> 3, f = sq & 7;
        int dark = ((r + f) & 1) == 0;
        fill_rect(x, y, SQ, SQ, dark ? C_DARK : C_LIGHT);
        if (sq == last_from || sq == last_to) blend_rect(x, y, SQ, SQ, 0xFFFF33, 105);
        if (sq == sel) blend_rect(x, y, SQ, SQ, 0xFFFF33, 135);
        if (sq == hint_from || sq == hint_to) blend_rect(x, y, SQ, SQ, 0x4F8CFF, 120);
        int fx = flipped ? 7 - f : f, rx = flipped ? r : 7 - r;
        u32 tc = dark ? C_LIGHT : C_DARK;
        if (rx == 7) { char c[2] = {(char)('a' + f), 0}; text(0, x + SQ - 11, y + SQ - 5, tc, c); }
        if (fx == 0) { char c[2] = {(char)('1' + r), 0}; text(0, x + 4, y + 14, tc, c); }
    }
    for (int sq = 0; sq < 64; sq++) {
        int v = p->b[sq];
        if (!v) continue;
        if (dragging && sq == sel) continue;
        if (anim.on) {
            if (sq == anim.m.sq[0]) continue;
            int gone = 0;
            for (int i = 0; i < aseg; i++) if (anim.m.cap[i] == sq) gone = 1;
            if (gone) continue;
        }
        draw_man(sq_x(sq) + 32, sq_y(sq) + 32, 25, v, 1);
    }
    if (anim.on) {
        int a = sq_x(anim.m.sq[aseg]), b = sq_x(anim.m.sq[aseg + 1]);
        int c = sq_y(anim.m.sq[aseg]), d = sq_y(anim.m.sq[aseg + 1]);
        draw_man(a + (b - a) * e / 1024 + 32, c + (d - c) * e / 1024 + 32, 25, anim.pre.b[anim.m.sq[0]], 1);
    }
    if (kb_active) {
        int x = sq_x(kcur), y = sq_y(kcur);
        for (int i = 0; i < 4; i++) {
            fill_rect(x + i, y, 1, SQ, 0x3C8CFF);
            fill_rect(x + SQ - 1 - i, y, 1, SQ, 0x3C8CFF);
            fill_rect(x, y + i, SQ, 1, 0x3C8CFF);
            fill_rect(x, y + SQ - 1 - i, SQ, 1, 0x3C8CFF);
        }
    }
    if (sel >= 0 && !anim.on) {
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].sq[0] != sel) continue;
            int to = legal[i].sq[legal[i].n];
            int cx = sq_x(to) + 32, cy = sq_y(to) + 32;
            if (legal[i].cap[0] != NOMV) disc(cx, cy, 14, 10, 0x000000, 60);
            else disc(cx, cy, 11, 0, 0x000000, 46);
        }
    }
    if (dragging && sel >= 0) draw_man(mx, my, 25, cur()->b[sel], 1);

    /* panel */
    text(2, PX, 68, C_TEXT, "Swirl Checkers");
    fill_rrect(PX, 82, PW, 54, 8, C_CARD);
    const char *l1, *l2;
    status_text(&l1, &l2);
    text(1, PX + 14, 105, result ? C_GREEN : C_TEXT, l1);
    text(0, PX + 14, 125, C_MUTED, l2);
    draw_button(B_NEW, "New game", 1);
    draw_button(B_UNDO, "Undo", 0);
    draw_button(B_FLIP, "Flip board", 0);
    draw_button(B_HINT, "Hint", 0);
    {
        char b[40] = "Play: ";
        const char *m = mode == 0 ? "White vs Computer" : mode == 1 ? "Black vs Computer" : "Two players";
        char *o = b + 6;
        while (*m) *o++ = *m++;
        *o = 0;
        draw_button(B_MODE, b, 0);
        char b2[40] = "Level: ";
        o = b2 + 7;
        char nb[4];
        put_int(nb, level);
        const char *s = LEVEL_NAME[level];
        while (*s) *o++ = *s++;
        *o++ = ' '; *o++ = '('; *o++ = nb[0]; *o++ = '/'; *o++ = '4'; *o++ = ')';
        *o = 0;
        draw_button(B_LEVEL, b2, 0);
    }
    text(0, PX + 2, 352, C_MUTED, "MOVES");
    fill_rrect(PX, 360, PW, 184, 8, C_CARD);
    {
        int rows = (hn + 1) / 2, first = rows > 8 ? rows - 8 : 0;
        for (int r = first; r < rows; r++) {
            int y = 360 + 12 + (r - first) * 22 + 15;
            char nb[10];
            int k = put_int(nb, r + 1);
            nb[k] = '.'; nb[k + 1] = 0;
            text(0, PX + 12, y, C_MUTED, nb);
            int wi = r * 2, bi = r * 2 + 1, l;
            for (l = 0; san[wi][l]; l++) {}
            text(l > 8 ? 0 : 1, PX + 52, y, wi == hn - 1 ? C_GREEN : C_TEXT, san[wi]);
            if (bi < hn) {
                for (l = 0; san[bi][l]; l++) {}
                text(l > 8 ? 0 : 1, PX + 146, y, bi == hn - 1 ? C_GREEN : C_TEXT, san[bi]);
            }
        }
        if (hn == 0) text(0, PX + 14, 384, C_MUTED, "Click, drag, or arrows + Enter.");
    }
    if (result) {
        int bx = BX + 106, by = BY + 186;
        fill_rrect(bx, by, 300, 140, 12, C_PANEL);
        text_c(2, bx + 150, by + 44, C_TEXT, l1);
        text_c(1, bx + 150, by + 70, C_MUTED, l2);
        int x = bx + 75, y = by + 86;
        fill_rrect(x, y, 150, 40, 8, hover == 100 ? C_GREEN_HOV : C_GREEN);
        text_c(1, x + 75, y + 26, 0x1B2410, "New game");
    }
}
