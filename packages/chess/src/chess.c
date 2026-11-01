/* Swirl Chess: full chess rules, AI, move ratings and software-rendered UI in one portable file.
 * Same ui.h / assets.h / native.c front end as Swirl Checkers. */
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

/* thick line drawn as a row of small discs */
static void thick_line(int x0, int y0, int x1, int y1, int r, u32 col) {
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int n = adx > ady ? adx : ady;
    if (n < 1) n = 1;
    for (int i = 0; i <= n; i++) disc(x0 + dx * i / n, y0 + dy * i / n, r, 0, col, 255);
}

/* filled polygon, even-odd, 8.8 fixed-point vertices relative to (cx,cy) */
static void fill_poly(int cx, int cy, const short *v, int n, u32 col) {
    for (int y = -12; y <= 12; y++)
        for (int x = -12; x <= 12; x++) {
            int hit = 0;
            for (int s = 0; s < 4; s++) {                      /* 2x2 supersampling */
                int px = x * 256 + (s & 1) * 128 + 64, py = y * 256 + (s >> 1) * 128 + 64, in = 0;
                for (int i = 0, j = n - 1; i < n; j = i++) {
                    int xi = v[i * 2], yi = v[i * 2 + 1], xj = v[j * 2], yj = v[j * 2 + 1];
                    if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) in ^= 1;
                }
                hit += in;
            }
            if (hit) put(cx + x, cy + y, col, hit * 255 / 4);
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
enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
#define MAXM 192
#define MAXQ 64
#define MAXH 1024
#define INF 30000
#define MATE 29000

typedef struct { i8 b[64]; u8 side, castle, half; i8 ep; u8 ksq[2]; } Pos;   /* b: +white / -black, 1..6 = P N B R Q K */
typedef struct { u8 from, to, promo, flags; } Move;
#define F_CAP 1
#define F_EP 2
#define F_CASTLE 4
#define F_DBL 8

static const i8 KN_R[8] = {1, 2, 2, 1, -1, -2, -2, -1};
static const i8 KN_F[8] = {2, 1, -1, -2, -2, -1, 1, 2};
static const i8 DR8[8] = {1, 1, 1, 0, -1, -1, -1, 0};      /* even = diagonal, odd = straight */
static const i8 DF8[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
static const short VAL[7] = {0, 100, 320, 330, 500, 900, 0};

static int iabs(int v) { return v < 0 ? -v : v; }

static int attacked(const Pos *p, int sq, int by) {
    int r = sq >> 3, f = sq & 7, s = by == WHITE ? 1 : -1, pr = r - s;
    if (pr >= 0 && pr < 8) {
        if (f > 0 && p->b[pr * 8 + f - 1] == s * PAWN) return 1;
        if (f < 7 && p->b[pr * 8 + f + 1] == s * PAWN) return 1;
    }
    for (int k = 0; k < 8; k++) {
        int r2 = r + KN_R[k], f2 = f + KN_F[k];
        if (r2 >= 0 && r2 < 8 && f2 >= 0 && f2 < 8 && p->b[r2 * 8 + f2] == s * KNIGHT) return 1;
    }
    for (int d = 0; d < 8; d++) {
        int r2 = r + DR8[d], f2 = f + DF8[d], first = 1;
        while (r2 >= 0 && r2 < 8 && f2 >= 0 && f2 < 8) {
            int v = p->b[r2 * 8 + f2];
            if (v) {
                if (v == s * QUEEN || v == s * ((d & 1) ? ROOK : BISHOP)) return 1;
                if (first && v == s * KING) return 1;
                break;
            }
            first = 0;
            r2 += DR8[d];
            f2 += DF8[d];
        }
    }
    return 0;
}

static int in_check(const Pos *p) { return attacked(p, p->ksq[p->side], p->side ^ 1); }

static void emit(Move *o, int *n, int max, int from, int to, int promo, int flags) {
    if (*n >= max) return;
    o[*n].from = (u8)from; o[*n].to = (u8)to; o[*n].promo = (u8)promo; o[*n].flags = (u8)flags;
    (*n)++;
}

/* pseudo-legal moves (king safety not checked); caponly = captures and promotions */
static int gen_pseudo(const Pos *p, Move *o, int caponly, int max) {
    int n = 0, s = p->side == WHITE ? 1 : -1;
    for (int sq = 0; sq < 64; sq++) {
        int v = p->b[sq];
        if (!v || (v > 0) != (s > 0)) continue;
        int t = iabs(v), r = sq >> 3, f = sq & 7;
        if (t == PAWN) {
            int r1 = r + s, prom = (r1 == 0 || r1 == 7), start = (s > 0 ? r == 1 : r == 6);
            if (r1 < 0 || r1 > 7) continue;
            for (int df = -1; df <= 1; df += 2) {
                int f1 = f + df;
                if (f1 < 0 || f1 > 7) continue;
                int to = r1 * 8 + f1, tv = p->b[to];
                if (tv && (tv > 0) != (s > 0)) {
                    if (prom) { for (int pt = QUEEN; pt >= KNIGHT; pt--) emit(o, &n, max, sq, to, pt, F_CAP); }
                    else emit(o, &n, max, sq, to, 0, F_CAP);
                } else if (!tv && to == p->ep) emit(o, &n, max, sq, to, 0, F_CAP | F_EP);
            }
            int to = r1 * 8 + f;
            if (!p->b[to]) {
                if (prom) {
                    if (caponly) emit(o, &n, max, sq, to, QUEEN, 0);
                    else for (int pt = QUEEN; pt >= KNIGHT; pt--) emit(o, &n, max, sq, to, pt, 0);
                } else if (!caponly) {
                    emit(o, &n, max, sq, to, 0, 0);
                    if (start && !p->b[to + s * 8]) emit(o, &n, max, sq, to + s * 8, 0, F_DBL);
                }
            }
        } else if (t == KNIGHT || t == KING) {
            for (int k = 0; k < 8; k++) {
                int r2 = r + (t == KNIGHT ? KN_R[k] : DR8[k]), f2 = f + (t == KNIGHT ? KN_F[k] : DF8[k]);
                if (r2 < 0 || r2 > 7 || f2 < 0 || f2 > 7) continue;
                int to = r2 * 8 + f2, tv = p->b[to];
                if (!tv) { if (!caponly) emit(o, &n, max, sq, to, 0, 0); }
                else if ((tv > 0) != (s > 0)) emit(o, &n, max, sq, to, 0, F_CAP);
            }
        } else {
            for (int d = 0; d < 8; d++) {
                if (t == BISHOP && (d & 1)) continue;
                if (t == ROOK && !(d & 1)) continue;
                int r2 = r + DR8[d], f2 = f + DF8[d];
                while (r2 >= 0 && r2 < 8 && f2 >= 0 && f2 < 8) {
                    int to = r2 * 8 + f2, tv = p->b[to];
                    if (!tv) { if (!caponly) emit(o, &n, max, sq, to, 0, 0); }
                    else { if ((tv > 0) != (s > 0)) emit(o, &n, max, sq, to, 0, F_CAP); break; }
                    r2 += DR8[d];
                    f2 += DF8[d];
                }
            }
        }
    }
    if (!caponly) {                                   /* castling */
        int base = s > 0 ? 0 : 56, ks = s > 0 ? 1 : 4, qs = s > 0 ? 2 : 8, opp = p->side ^ 1;
        if (p->b[base + 4] == s * KING && !attacked(p, base + 4, opp)) {
            if ((p->castle & ks) && p->b[base + 7] == s * ROOK && !p->b[base + 5] && !p->b[base + 6] &&
                !attacked(p, base + 5, opp) && !attacked(p, base + 6, opp))
                emit(o, &n, max, base + 4, base + 6, 0, F_CASTLE);
            if ((p->castle & qs) && p->b[base] == s * ROOK && !p->b[base + 1] && !p->b[base + 2] && !p->b[base + 3] &&
                !attacked(p, base + 3, opp) && !attacked(p, base + 2, opp))
                emit(o, &n, max, base + 4, base + 2, 0, F_CASTLE);
        }
    }
    return n;
}

static void apply(Pos *p, const Move *m) {
    int pc = p->b[m->from], s = pc > 0 ? 1 : -1, t = iabs(pc), from = m->from, to = m->to;
    p->half = (t == PAWN || p->b[to]) ? 0 : (u8)(p->half + 1);
    p->ep = -1;
    if (m->flags & F_EP) p->b[to - s * 8] = 0;
    p->b[from] = 0;
    p->b[to] = (i8)(m->promo ? s * m->promo : pc);
    if (m->flags & F_CASTLE) {
        int base = to & ~7;
        if (to > from) { p->b[base + 5] = p->b[base + 7]; p->b[base + 7] = 0; }
        else { p->b[base + 3] = p->b[base]; p->b[base] = 0; }
    }
    if (t == KING) p->ksq[s > 0 ? 0 : 1] = (u8)to;
    if (from == 4 || to == 4) p->castle &= (u8)~3;
    if (from == 60 || to == 60) p->castle &= (u8)~12;
    if (from == 0 || to == 0) p->castle &= (u8)~2;
    if (from == 7 || to == 7) p->castle &= (u8)~1;
    if (from == 56 || to == 56) p->castle &= (u8)~8;
    if (m->flags & F_DBL) {
        int f = to & 7;
        if ((f > 0 && p->b[to - 1] == -s * PAWN) || (f < 7 && p->b[to + 1] == -s * PAWN)) p->ep = (i8)((from + to) / 2);
    }
    p->side ^= 1;
}

static int gen_legal(const Pos *p, Move *o) {
    Move t[MAXM];
    int n = gen_pseudo(p, t, 0, MAXM), c = 0;
    for (int i = 0; i < n; i++) {
        Pos q = *p;
        apply(&q, &t[i]);
        if (!attacked(&q, q.ksq[p->side], q.side)) o[c++] = t[i];
    }
    return c;
}

static void setup(Pos *p) {
    static const i8 back[8] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int i = 0; i < 64; i++) p->b[i] = 0;
    for (int f = 0; f < 8; f++) {
        p->b[f] = back[f]; p->b[8 + f] = PAWN;
        p->b[48 + f] = -PAWN; p->b[56 + f] = (i8)-back[f];
    }
    p->side = WHITE; p->castle = 15; p->half = 0; p->ep = -1; p->ksq[0] = 4; p->ksq[1] = 60;
}

static int same_pos(const Pos *a, const Pos *b) {
    if (a->side != b->side || a->castle != b->castle || a->ep != b->ep) return 0;
    for (int i = 0; i < 64; i++) if (a->b[i] != b->b[i]) return 0;
    return 1;
}

static int insufficient(const Pos *p) {
    int minors = 0, nb = 0;
    for (int i = 0; i < 64; i++) {
        int t = iabs(p->b[i]);
        if (t == PAWN || t == ROOK || t == QUEEN) return 0;
        if (t == KNIGHT) minors++;
        if (t == BISHOP) { minors++; nb++; }
    }
    if (minors <= 1) return 1;
    if (minors == nb && nb >= 2) { for (int i = 0, c = -1; i < 64; i++) if (iabs(p->b[i]) == BISHOP) { int col = ((i >> 3) + (i & 7)) & 1; if (c < 0) c = col; else if (c != col) return 0; } return 1; }
    return 0;
}

/* ============================================================ evaluation */
static int eval_mat(const Pos *p) {
    int s = 0;
    for (int i = 0; i < 64; i++) { int v = p->b[i]; if (v) s += v > 0 ? VAL[v] : -VAL[-v]; }
    return p->side == WHITE ? s : -s;
}

static int eval(const Pos *p) {
    int s = 0, npm = 0, bish[2] = {0, 0};
    for (int sq = 0; sq < 64; sq++) {
        int v = p->b[sq];
        if (!v) continue;
        int t = iabs(v), r = sq >> 3, f = sq & 7, rr = v > 0 ? r : 7 - r;
        int cd = iabs(2 * r - 7) > iabs(2 * f - 7) ? iabs(2 * r - 7) : iabs(2 * f - 7);
        int val = VAL[t];
        if (t == KING) continue;
        if (t != PAWN) npm += VAL[t];
        switch (t) {
            case PAWN: val += rr * 7 + ((f == 3 || f == 4) ? rr * 3 : 0) + (rr >= 5 ? rr * 8 : 0) - ((f == 0 || f == 7) ? 6 : 0); break;
            case KNIGHT: val += (7 - cd) * 7 - (rr == 0 ? 10 : 0); break;
            case BISHOP: val += (7 - cd) * 3; bish[v > 0 ? 0 : 1]++; break;
            case ROOK: val += rr == 6 ? 18 : 0; break;
            case QUEEN: val += 7 - cd; break;
        }
        s += v > 0 ? val : -val;
    }
    if (bish[0] >= 2) s += 30;
    if (bish[1] >= 2) s -= 30;
    for (int c = 0; c < 2; c++) {
        int sq = p->ksq[c], r = sq >> 3, f = sq & 7, rr = c == 0 ? r : 7 - r, val;
        int cd = iabs(2 * r - 7) > iabs(2 * f - 7) ? iabs(2 * r - 7) : iabs(2 * f - 7);
        if (npm < 2400) val = (7 - cd) * 8;
        else val = rr == 0 ? ((f <= 2 || f >= 6) ? 28 : -10) : -30 * rr;
        s += c == 0 ? val : -val;
    }
    return p->side == WHITE ? s : -s;
}

/* ================================================================== search */
static TimeFn g_now;
static int nodes, abort_s, q_mat;
static u32 t_start, t_limit;
static Move killer[64][2];

static int mscore(const Pos *p, const Move *m, int ply) {
    int s = 0;
    if (m->flags & F_CAP) s = 20000 + ((m->flags & F_EP) ? PAWN : iabs(p->b[m->to])) * 16 - iabs(p->b[m->from]);
    if (m->promo) s += 15000 + m->promo;
    if (!s && ply < 64 &&
        ((killer[ply][0].from == m->from && killer[ply][0].to == m->to) ||
         (killer[ply][1].from == m->from && killer[ply][1].to == m->to))) s = 10000;
    return s;
}

static int time_up(void) {
    if ((++nodes & 2047) == 0 && t_limit && g_now && g_now() - t_start > t_limit) abort_s = 1;
    return abort_s;
}

static int qsearch(const Pos *p, int alpha, int beta, int ply) {
    if (time_up()) return 0;
    int stand = q_mat ? eval_mat(p) : eval(p), best = stand;
    if (ply >= 40 || stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    Move ms[MAXQ];
    short sc[MAXQ];
    int n = gen_pseudo(p, ms, 1, MAXQ);
    for (int i = 0; i < n; i++) sc[i] = (short)(mscore(p, &ms[i], 99) / 8);
    for (int k = 0; k < n; k++) {
        int bi = k;
        for (int j = k + 1; j < n; j++) if (sc[j] > sc[bi]) bi = j;
        Move m = ms[bi]; short ts = sc[bi];
        ms[bi] = ms[k]; sc[bi] = sc[k]; ms[k] = m; sc[k] = ts;
        Pos q = *p;
        apply(&q, &m);
        if (attacked(&q, q.ksq[p->side], q.side)) continue;
        int s = -qsearch(&q, -beta, -alpha, ply + 1);
        if (abort_s) return 0;
        if (s > best) { best = s; if (s > alpha) alpha = s; }
        if (alpha >= beta) break;
    }
    return best;
}

static int negamax(const Pos *p, int depth, int alpha, int beta, int ply) {
    if (time_up()) return 0;
    int chk = in_check(p);
    if (chk && ply < 40) depth++;
    if (depth <= 0) return qsearch(p, alpha, beta, 0);
    if (p->half >= 100) return 0;
    Move ms[MAXM];
    short sc[MAXM];
    int n = gen_pseudo(p, ms, 0, MAXM), best = -INF, legal = 0;
    for (int i = 0; i < n; i++) sc[i] = (short)(mscore(p, &ms[i], ply) / 8);
    for (int k = 0; k < n; k++) {
        int bi = k;
        for (int j = k + 1; j < n; j++) if (sc[j] > sc[bi]) bi = j;
        Move m = ms[bi]; short ts = sc[bi];
        ms[bi] = ms[k]; sc[bi] = sc[k]; ms[k] = m; sc[k] = ts;
        Pos q = *p;
        apply(&q, &m);
        if (attacked(&q, q.ksq[p->side], q.side)) continue;
        legal++;
        int s = -negamax(&q, depth - 1, -beta, -alpha, ply + 1);
        if (abort_s) return 0;
        if (s > best) { best = s; if (s > alpha) alpha = s; }
        if (alpha >= beta) {
            if (!(m.flags & F_CAP) && ply < 64) { killer[ply][1] = killer[ply][0]; killer[ply][0] = m; }
            break;
        }
    }
    if (!legal) return chk ? -MATE + ply : 0;
    return best;
}

static u32 rs = 2463534242u;
static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static int ai_choose(const Pos *p, const Move *ms, int n, int lvl) {
    static const int DEPTH[5] = {0, 1, 2, 4, 7}, TMS[5] = {0, 0, 0, 1200, 2500}, NOISE[5] = {0, 160, 50, 0, 0};
    if (n <= 1) return 0;
    int order[MAXM];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = n - 1; i > 0; i--) { int j = (int)(rnd() % (u32)(i + 1)), t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < 64; i++) killer[i][0].from = killer[i][0].to = killer[i][1].from = killer[i][1].to = 0;
    t_start = g_now ? g_now() : 0;
    t_limit = (u32)TMS[lvl];
    abort_s = 0;
    nodes = 0;
    int best_i = order[0];
    int d0 = NOISE[lvl] ? DEPTH[lvl] : 1;
    for (int d = d0; d <= DEPTH[lvl]; d++) {
        int cur_best = -INF, cur_i = -1;
        for (int k = 0; k < n; k++) {
            int i = order[k];
            Pos q = *p;
            apply(&q, &ms[i]);
            int s;
            if (NOISE[lvl]) s = -negamax(&q, d - 1, -INF, INF, 1) + (int)(rnd() % (u32)NOISE[lvl]);
            else s = -negamax(&q, d - 1, -INF, -cur_best, 1);
            if (abort_s) break;
            if (s > cur_best || cur_i < 0) { cur_best = s; cur_i = k; }
        }
        if (abort_s) break;
        best_i = order[cur_i];
        { int t = order[cur_i]; for (int k = cur_i; k > 0; k--) order[k] = order[k - 1]; order[0] = t; }
        if (cur_best > MATE - 60 || cur_best < -MATE + 60) break;
    }
    abort_s = 0;
    return best_i;
}

/* ============================================================ move rating */
enum { R_NONE, R_BRILLIANT, R_GREAT, R_BEST, R_EXCELLENT, R_GOOD, R_BOOK, R_INACC, R_MISTAKE, R_MISS, R_BLUNDER, R_COUNT };
static const char *R_NAME[R_COUNT] = {"", "Brilliant", "Great", "Best", "Excellent", "Good", "Book", "Inaccuracy", "Mistake", "Miss", "Blunder"};
static const char *R_SYM[R_COUNT] = {"", "!!", "!", "*", "+", "ok", "book", "?!", "?", "x", "??"};
static const u32 R_COL[R_COUNT] = {0, 0x26C2A3, 0x5C8BB0, 0x96BC4B, 0x96BC4B, 0x7FA650, 0xA88865, 0xF7C631, 0xE58F2A, 0xFA412D, 0xCA3431};

/* expected score (per mille) from centipawns, logistic curve */
static int wp(int cp) {
    static const short WT[21] = {500, 546, 591, 635, 676, 715, 751, 784, 813, 840, 863, 883, 901, 916, 929, 941, 950, 958, 965, 971, 975};
    if (cp >= 20000) return 1000;
    if (cp <= -20000) return 0;
    int a = cp < 0 ? -cp : cp, w;
    if (a >= 1000) { w = 975 + (a - 1000) / 80; if (w > 998) w = 998; }
    else { int i = a / 50, r = a % 50; w = WT[i] + (WT[i + 1] - WT[i]) * r / 50; }
    return cp < 0 ? 1000 - w : w;
}

static const char *BOOK[] = {
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8",
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 c3d4 c5b4",
    "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 f3g5 d7d5 e4d5 c6a5",
    "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 d4c6 b7c6 e4e5 d8e7",
    "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3 f8e7 e1g1 b8c6",
    "e2e4 e7e5 b1c3 g8f6 g1f3 b8c6 f1b5 f8b4 e1g1 e8g8 d2d3 d7d6 c1g5 b4c3",
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6",
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 d7d6 c1g5 e7e6",
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5 d7d6 c1g5 a7a6 b5a3 b7b5",
    "e2e4 c7c5 c2c3 g8f6 e4e5 f6d5 d2d4 c5d4 g1f3 b8c6 c3d4 d7d6",
    "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3 g8e7",
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6 c1g5 f8e7 e4e5 f6d7 g5e7 d8e7",
    "e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6 a2a3 c5c4",
    "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 c6c5 e1g1 b8c6",
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7",
    "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5 d2d4 g8f6 g1f3 c7c6 f1c4 c8f5",
    "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 g1f3 f8g7 f1e2 e8g8 e1g1 c7c6",
    "e2e4 g8f6 e4e5 f6d5 d2d4 d7d6 g1f3 c8g4 f1e2 e7e6 e1g1 f8e7",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 b8d7 a1c1 c7c6",
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5 e2e3 e7e6 f1c4 f8b4",
    "d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5 e1g1 a7a6",
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5 e1g1 b8c6",
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3 c7c5 e1g1 b8c6",
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7 g1f3 c7c5",
    "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7 f1g2 f8e7 e1g1 e8g8 b1c3 f6e4",
    "d2d4 d7d5 g1f3 g8f6 c1f4 e7e6 e2e3 c7c5 c2c3 b8c6 b1d2 f8d6 f4g3 e8g8",
    "d2d4 f7f5 g2g3 g8f6 f1g2 e7e6 g1f3 f8e7 e1g1 e8g8 c2c4 d7d6",
    "d2d4 g8f6 c2c4 e7e6 g2g3 d7d5 f1g2 f8e7 g1f3 e8g8 e1g1 d5c4",
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6 e1g1 f8e7",
    "c2c4 c7c5 g1f3 g8f6 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7 e1g1 e8g8",
    "g1f3 d7d5 c2c4 e7e6 g2g3 g8f6 f1g2 f8e7 e1g1 e8g8 b2b3 c7c5",
    0};

static void coord(const Move *m, char *o) {
    o[0] = (char)('a' + (m->from & 7)); o[1] = (char)('1' + (m->from >> 3));
    o[2] = (char)('a' + (m->to & 7)); o[3] = (char)('1' + (m->to >> 3));
}

/* ==================================================================== game */
static Pos hist[MAXH + 1];
static Move mvs[MAXH];
static char san[MAXH][12];
static u8 rate[MAXH], revalid[MAXH];
static i8 rate_pc[MAXH];
static short rloss[MAXH];
static int rev[MAXH];            /* eval after the move, white's point of view (when revalid) */
static int hn;                   /* plies played; hist[hn] is the current position */
static int result;               /* 0 playing, 1 white wins, 2 black wins, 3 draw */
static const char *res_reason = "";

static int mode;                 /* 0 you=white vs CPU, 1 you=black vs CPU, 2 two players */
static int level = 3;
static int flipped, rating_on;
static int sel = -1, dragging, mx, my;
static Move legal[MAXM];
static int nlegal;
static int last_from = -1, last_to = -1, check_sq = -1;
static int hint_from = -1, hint_to = -1;
static char hint_san[12];
static int ai_shown, dirty = 1, hover = -1;
static int kcur = 12, kb_active;
static int promo_on, promo_from, promo_to;

#define ANIM_MS 220
static struct { int on; u32 t0; Move m; int pc; } anim;
static int skip_anim;

static const char *LEVEL_NAME[5] = {"", "Beginner", "Casual", "Club", "Strong"};

static const Pos *cur(void) { return &hist[hn]; }

static void refresh_legal(void) {
    const Pos *p = cur();
    nlegal = gen_legal(p, legal);
    check_sq = in_check(p) ? p->ksq[p->side] : -1;
    result = 0;
    res_reason = "";
    if (!nlegal) {
        if (check_sq >= 0) { result = p->side == WHITE ? 2 : 1; res_reason = "Checkmate"; }
        else { result = 3; res_reason = "Stalemate"; }
    } else if (p->half >= 100) { result = 3; res_reason = "50 moves without a capture"; }
    else if (insufficient(p)) { result = 3; res_reason = "Not enough material"; }
    else if (hn >= MAXH - 2) { result = 3; res_reason = "Move limit"; }
    else {
        int reps = 0;
        for (int i = hn; i >= 0; i--) if (same_pos(&hist[i], p)) reps++;
        if (reps >= 3) { result = 3; res_reason = "Threefold repetition"; }
    }
}

static int human_turn(void) {
    if (result) return 0;
    if (mode == 2) return 1;
    return cur()->side == (mode == 0 ? WHITE : BLACK);
}
static int ai_turn(void) { return !result && !human_turn(); }

static void clear_sel(void) { sel = -1; dragging = 0; skip_anim = 0; promo_on = 0; }

static void new_game(void) {
    hn = 0;
    setup(&hist[0]);
    anim.on = 0;
    flipped = (mode == 1);
    clear_sel();
    last_from = last_to = hint_from = hint_to = -1;
    ai_shown = 0;
    refresh_legal();
    dirty = 1;
}

/* ------------------------------------------------------------ notation */
static void move_san(const Pos *pre, const Move *m, const Move *lg, int nlg, const Pos *post, char *o) {
    char *s = o;
    int pc = iabs(pre->b[m->from]);
    if (m->flags & F_CASTLE) {
        const char *c = m->to > m->from ? "O-O" : "O-O-O";
        while (*c) *s++ = *c++;
    } else {
        if (pc == PAWN) {
            if (m->flags & F_CAP) *s++ = (char)('a' + (m->from & 7));
        } else {
            *s++ = "  NBRQK"[pc];
            int same = 0, sf = 0, sr = 0;
            for (int i = 0; i < nlg; i++)
                if (lg[i].to == m->to && lg[i].from != m->from && iabs(pre->b[lg[i].from]) == pc) {
                    same = 1;
                    if ((lg[i].from & 7) == (m->from & 7)) sf = 1;
                    if ((lg[i].from >> 3) == (m->from >> 3)) sr = 1;
                }
            if (same && !sf) *s++ = (char)('a' + (m->from & 7));
            else if (same && !sr) *s++ = (char)('1' + (m->from >> 3));
            else if (same) { *s++ = (char)('a' + (m->from & 7)); *s++ = (char)('1' + (m->from >> 3)); }
        }
        if (m->flags & F_CAP) *s++ = 'x';
        *s++ = (char)('a' + (m->to & 7));
        *s++ = (char)('1' + (m->to >> 3));
        if (m->promo) { *s++ = '='; *s++ = "  NBRQ"[m->promo]; }
    }
    if (in_check(post)) {
        Move t[MAXM];
        *s++ = gen_legal(post, t) ? '+' : '#';
    }
    *s = 0;
}

static int in_book(const Move *m) {
    if (hn > 30) return 0;
    char c[4];
    coord(m, c);
    for (int l = 0; BOOK[l]; l++) {
        const char *ln = BOOK[l];
        int len = 0, ok = 1;
        while (ln[len]) len++;
        if (len < hn * 5 + 4) continue;
        for (int k = 0; k < hn && ok; k++) {
            char mc[4];
            coord(&mvs[k], mc);
            for (int j = 0; j < 4; j++) if (ln[k * 5 + j] != mc[j]) ok = 0;
        }
        for (int j = 0; j < 4 && ok; j++) if (ln[hn * 5 + j] != c[j]) ok = 0;
        if (ok) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ rating */
static int mat_diff(const Pos *p) {           /* material from the side to move's view */
    return eval_mat(p);
}

static int is_brilliant(const Pos *pre, const Move *m, int played, int best) {
    int pc = iabs(pre->b[m->from]);
    if (pc < KNIGHT || pc > QUEEN) return 0;
    if (best >= 500 || played < -100) return 0;           /* already winning big, or just losing */
    Pos q = *pre;
    apply(&q, m);
    q_mat = 1;
    abort_s = 0;
    t_limit = 0;
    int v = -qsearch(&q, -INF, INF, 0);
    q_mat = 0;
    return v <= mat_diff(pre) - 200;                       /* piece(s) given up for nothing material */
}

/* rate the move legal[pi] about to be played from hist[hn]; fills rate/rloss/rev for ply hn */
static void rate_move(int pi) {
    const Pos *pre = &hist[hn];
    int n = nlegal, sc[MAXM];
    int d = n > 40 ? 2 : 3;
    t_limit = 0;
    abort_s = 0;
    for (int i = 0; i < n; i++) {
        Pos q = *pre;
        apply(&q, &legal[i]);
        sc[i] = -negamax(&q, d - 1, -INF, INF, 1);
    }
    if (g_now) {            /* second pass: search the top candidates and the played move deeper */
        int S[5], ns = 0, deep[5], ok = 1;
        for (int k = 0; k < 4 && k < n; k++) {
            int bj = -1;
            for (int i = 0; i < n; i++) {
                int used = 0;
                for (int j = 0; j < ns; j++) if (S[j] == i) used = 1;
                if (!used && (bj < 0 || sc[i] > sc[bj])) bj = i;
            }
            S[ns++] = bj;
        }
        { int has = 0; for (int j = 0; j < ns; j++) if (S[j] == pi) has = 1; if (!has) S[ns++] = pi; }
        for (int j = 0; j < ns && ok; j++) {
            Pos q = *pre;
            apply(&q, &legal[S[j]]);
            t_start = g_now(); t_limit = 450; abort_s = 0; nodes = 0;
            int v = -negamax(&q, 4, -INF, INF, 1);
            if (abort_s) ok = 0; else deep[j] = v;
        }
        abort_s = 0; t_limit = 0;
        if (ok) {
            for (int i = 0; i < n; i++) sc[i] = -INF;
            for (int j = 0; j < ns; j++) sc[S[j]] = deep[j];
        }
    }
    int bi = 0;
    for (int i = 1; i < n; i++) if (sc[i] > sc[bi]) bi = i;
    int best = sc[bi], sec = best;
    if (n > 1) { sec = -INF; for (int i = 0; i < n; i++) if (i != bi && sc[i] > sec) sec = sc[i]; }
    int wb = wp(best), loss = wb - wp(sc[pi]);
    if (loss < 0) loss = 0;

    /* did the opponent just hand us something? (then a weak reply is a "Miss") */
    int opp_err = 0;
    if (hn >= 1 && revalid[hn - 1]) opp_err = rloss[hn - 1];
    else if (hn >= 2 && revalid[hn - 2]) {
        int ref = pre->side == WHITE ? rev[hn - 2] : -rev[hn - 2];
        opp_err = wb - wp(ref);
        if (opp_err < 0) opp_err = 0;
    }

    const Move *m = &legal[pi];
    int r;
    if (in_book(m)) r = R_BOOK;
    else if (loss <= 20) {
        r = loss <= 2 ? R_BEST : R_EXCELLENT;
        Pos q = *pre;
        apply(&q, m);
        int recap = hn >= 1 && (m->flags & F_CAP) && m->to == mvs[hn - 1].to;
        int free_grab = (m->flags & F_CAP) && !(m->flags & F_EP) && iabs(pre->b[m->to]) >= KNIGHT && !attacked(&q, m->to, q.side);
        if (is_brilliant(pre, m, sc[pi], best)) r = R_BRILLIANT;
        else if (loss <= 2 && n >= 2 && wb - wp(sec) >= 150 && !recap && !free_grab) r = R_GREAT;
    }
    else if (loss <= 50) r = R_GOOD;
    else if (loss <= 100) r = R_INACC;
    else if (loss <= 200) r = R_MISTAKE;
    else r = R_BLUNDER;
    if ((r == R_INACC || r == R_MISTAKE) && opp_err >= 150) r = R_MISS;

    rate[hn] = (u8)r;
    rloss[hn] = (short)loss;
    revalid[hn] = 1;
    rev[hn] = pre->side == WHITE ? sc[pi] : -sc[pi];
    Pos q = *pre;
    apply(&q, m);
    rate_pc[hn] = q.b[m->to];
}

/* ------------------------------------------------------------ playing */
static void play(int i) {
    if (hn >= MAXH - 1) return;
    Move m = legal[i];
    Pos pre = hist[hn], q = pre;
    int mover = pre.side;
    rate[hn] = 0; rloss[hn] = 0; revalid[hn] = 0; rate_pc[hn] = 0;
    if (rating_on && (mode == 2 || mover == (mode == 0 ? WHITE : BLACK))) rate_move(i);
    apply(&q, &m);
    move_san(&pre, &m, legal, nlegal, &q, san[hn]);
    mvs[hn] = m;
    hist[hn + 1] = q;
    hn++;
    anim.on = 0;
    if (g_now && !skip_anim) { anim.m = m; anim.pc = pre.b[m.from]; anim.t0 = g_now(); anim.on = 1; }
    last_from = m.from;
    last_to = m.to;
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
    if (hn > 0) { last_from = mvs[hn - 1].from; last_to = mvs[hn - 1].to; }
    else last_from = last_to = -1;
    hint_from = hint_to = -1;
    clear_sel();
    refresh_legal();
    dirty = 1;
}

static void do_hint(void) {
    if (!human_turn()) return;
    int i = ai_choose(cur(), legal, nlegal, 3);
    hint_from = legal[i].from;
    hint_to = legal[i].to;
    move_san(cur(), &legal[i], legal, nlegal, cur(), hint_san);
    {   /* san() looked at the pre position for check marks; strip them */
        int l = 0;
        while (hint_san[l]) l++;
        if (l && (hint_san[l - 1] == '+' || hint_san[l - 1] == '#')) hint_san[l - 1] = 0;
    }
    dirty = 1;
}

/* ================================================================ geometry */
enum { B_NEW, B_UNDO, B_FLIP, B_HINT, B_MODE, B_LEVEL, B_RATE, B_COUNT };
static void btn_rect(int id, int *x, int *y, int *w, int *h) {
    static const short R[B_COUNT][4] = {
        {PX, 148, 118, 38}, {PX + 130, 148, 118, 38},
        {PX, 196, 118, 38}, {PX + 130, 196, 118, 38},
        {PX, 244, 248, 38}, {PX, 292, 248, 38}, {PX, 340, 248, 38}};
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
    for (int i = 0; i < nlegal; i++) if (legal[i].from == sq) return 1;
    return 0;
}
static int try_move(int from, int to) {
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) {
            if (legal[i].promo) { promo_on = 1; promo_from = from; promo_to = to; dragging = 0; dirty = 1; return 1; }
            play(i);
            return 1;
        }
    return 0;
}
static void finish_promo(int pt) {
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == promo_from && legal[i].to == promo_to && legal[i].promo == pt) { skip_anim = 0; play(i); return; }
}
static int over_banner_button(int x, int y) {
    int bx = BX + 106 + 75, by = BY + 186 + 86;
    return x >= bx && x < bx + 150 && y >= by && y < by + 40;
}
static int promo_cell(int x, int y) {      /* 0..3 = Q R B N, -1 none */
    int x0 = BX + 256 - 140 + 12, y0 = BY + 256 - 52 + 30;
    if (y < y0 || y >= y0 + SQ || x < x0 || x >= x0 + 4 * 68) return -1;
    int k = (x - x0) / 68;
    return (x - x0) % 68 < SQ ? k : -1;
}

/* ================================================================== input */
void ui_mouse(int type, int x, int y) {
    mx = x;
    my = y;
    if (promo_on) {
        if (type == 1) {
            int k = promo_cell(x, y);
            if (k >= 0) { static const u8 PT[4] = {QUEEN, ROOK, BISHOP, KNIGHT}; finish_promo(PT[k]); }
            else { promo_on = 0; sel = -1; }
        }
        dirty = 1;
        return;
    }
    if (type == 0) {
        int h = btn_at(x, y);
        if (result && !anim.on && over_banner_button(x, y)) h = 100;
        if (h != hover) { hover = h; dirty = 1; }
        if (dragging) dirty = 1;
        return;
    }
    if (type == 1) {
        if (result && !anim.on && over_banner_button(x, y)) { new_game(); return; }
        int b = btn_at(x, y);
        if (b >= 0) {
            switch (b) {
                case B_NEW: new_game(); break;
                case B_UNDO: do_undo(); break;
                case B_FLIP: flipped ^= 1; dirty = 1; break;
                case B_HINT: do_hint(); break;
                case B_MODE: mode = (mode + 1) % 3; new_game(); break;
                case B_LEVEL: level = level % 4 + 1; dirty = 1; break;
                case B_RATE: rating_on ^= 1; dirty = 1; break;
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
    if (promo_on) {
        dirty = 1;
        if (vk == 'Q') finish_promo(QUEEN);
        else if (vk == 'R') finish_promo(ROOK);
        else if (vk == 'B') finish_promo(BISHOP);
        else if (vk == 'N') finish_promo(KNIGHT);
        else if (vk == 27) { promo_on = 0; sel = -1; }
        return;
    }
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
        case 'R': rating_on ^= 1; dirty = 1; break;
        case 27: clear_sel(); dirty = 1; break;
        case 'L': level = level % 4 + 1; dirty = 1; break;
    }
}

static int anim_e(void) {           /* 0 = finished, else 1..1025 eased progress */
    u32 t = g_now ? g_now() - anim.t0 : 0xFFFFFF;
    if (t >= ANIM_MS) return 0;
    int q = 1024 - (int)(t * 1024 / ANIM_MS);
    return 1024 - ((q * q >> 10) * q >> 10) + 1;
}

int ui_step(void) {
    if (anim.on) {
        if (!anim_e()) anim.on = 0;
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
    rating_on = 0;
    new_game();
}

/* ================================================================== render */
#define PW_FILL 0xFAFAF5
#define PW_LINE 0x3B3631
#define PB_FILL 0x4A4743
#define PB_LINE 0x0F0E0D

static void draw_piece(int v, int x, int y) {
    int t = (v > 0 ? v : -v) - 1;
    const unsigned char *F = g_piece_fill + t * 4096, *L = g_piece_line + t * 4096;
    u32 fc = v > 0 ? PW_FILL : PB_FILL, lc = v > 0 ? PW_LINE : PB_LINE;
    for (int yy = 0; yy < 64; yy++)
        for (int xx = 0; xx < 64; xx++) {
            int a = F[yy * 64 + xx];
            if (a) put(x + xx, y + yy, fc, a);
            a = L[yy * 64 + xx];
            if (a) put(x + xx, y + yy, lc, a);
        }
}

/* small piece for the captured strips: 64x64 mask box-filtered down to 22x22 */
static void draw_mini(int v, int x, int y) {
    int t = (v > 0 ? v : -v) - 1;
    const unsigned char *F = g_piece_fill + t * 4096, *L = g_piece_line + t * 4096;
    u32 fc = v > 0 ? PW_FILL : PB_FILL, lc = v > 0 ? PW_LINE : PB_LINE;
    for (int dy = 0; dy < 22; dy++)
        for (int dx = 0; dx < 22; dx++) {
            int sf = 0, sl = 0, cnt = 0;
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 3; i++) {
                    int sx = dx * 64 / 22 + i, sy = dy * 64 / 22 + j;
                    if (sx > 63 || sy > 63) continue;
                    sf += F[sy * 64 + sx]; sl += L[sy * 64 + sx]; cnt++;
                }
            if (!cnt) continue;
            if (sf) put(x + dx, y + dy, fc, sf / cnt);
            if (sl) put(x + dx, y + dy, lc, sl / cnt);
        }
}

static const short STAR[20] = {0, -1843, 450, -911, 1752, -569, 728, 177, 1083, 1475, 0, 768, -1083, 1475, -728, 177, -1752, -569, -450, -911};

/* rating badge, centred on (cx,cy), radius 11 */
static void draw_badge(int cx, int cy, int r) {
    u32 col = R_COL[r];
    disc(cx, cy + 1, 12, 0, 0x000000, 70);
    disc(cx, cy, 12, 0, 0xFFFFFF, 255);
    disc(cx, cy, 11, 0, col, 255);
    switch (r) {
        case R_BEST: fill_poly(cx, cy, STAR, 10, 0xFFFFFF); break;
        case R_EXCELLENT: thick_line(cx - 6, cy, cx + 6, cy, 1, 0xFFFFFF); thick_line(cx, cy - 6, cx, cy + 6, 1, 0xFFFFFF); break;
        case R_GOOD: thick_line(cx - 6, cy, cx - 2, cy + 5, 1, 0xFFFFFF); thick_line(cx - 2, cy + 5, cx + 6, cy - 5, 1, 0xFFFFFF); break;
        case R_MISS: thick_line(cx - 5, cy - 5, cx + 5, cy + 5, 1, 0xFFFFFF); thick_line(cx - 5, cy + 5, cx + 5, cy - 5, 1, 0xFFFFFF); break;
        case R_BOOK: text_c(0, cx, cy + 4, 0xFFFFFF, "B"); break;
        case R_INACC: text_c(1, cx, cy + 5, 0x3A2E00, "?!"); break;
        default: text_c(1, cx, cy + 5, 0xFFFFFF, R_SYM[r]); break;
    }
}

/* the last rated move whose piece is still standing on its target square */
static int badge_ply(void) {
    if (!rating_on) return -1;
    for (int k = hn - 1; k >= 0 && k >= hn - 2; k--)
        if (rate[k]) return cur()->b[mvs[k].to] == rate_pc[k] ? k : -1;
    return -1;
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
    int have[2][7] = {{0}}, mat[2] = {0, 0};
    for (int i = 0; i < 64; i++) {
        int v = p->b[i];
        if (!v) continue;
        int c = v > 0 ? 0 : 1, t = v > 0 ? v : -v;
        have[c][t]++;
        mat[c] += VAL[t] / 10 > 0 ? (t == PAWN ? 1 : t == QUEEN ? 9 : t == ROOK ? 5 : 3) : 0;
    }
    char nb[48];
    name_of(side, nb);
    disc(BX + 5, y + 21, 5, 0, side == WHITE ? 0xF0F0F0 : 0x1A1A1A, 255);
    if (side == BLACK) disc(BX + 5, y + 21, 5, 4, 0x8A8A8A, 255);
    text(1, BX + 18, y + 27, C_TEXT, nb);
    int x = BX + 18 + text_w(1, nb) + 14;
    int enemy = side ^ 1;
    static const u8 INIT[7] = {0, 8, 2, 2, 2, 1, 0};
    for (int t = QUEEN; t >= PAWN; t--) {
        int taken = INIT[t] - have[enemy][t];
        for (int k = 0; k < taken; k++) { draw_mini(enemy == WHITE ? t : -t, x, y + 10); x += t == PAWN ? 9 : 14; }
        if (taken > 0) x += 4;
    }
    int lead = mat[side] - mat[enemy];
    if (lead > 0) {
        char b[12] = "+";
        put_int(b + 1, lead);
        text(0, x + 4, y + 27, C_MUTED, b);
    }
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
        case 1: *l1 = "White wins"; *l2 = res_reason; return;
        case 2: *l1 = "Black wins"; *l2 = res_reason; return;
        case 3: *l1 = "Draw"; *l2 = res_reason; return;
    }
    if (ai_turn()) *l1 = "Computer is thinking...";
    else if (mode == 2) *l1 = cur()->side == WHITE ? "White to move" : "Black to move";
    else *l1 = "Your move";
    int bp = badge_ply();
    if (hint_from >= 0) {
        char *o = s2;
        const char *h = "Hint: ";
        while (*h) *o++ = *h++;
        for (const char *q = hint_san; *q; q++) *o++ = *q;
        *o = 0;
        *l2 = s2;
    } else if (bp >= 0) {
        char *o = s2;
        const char *h = "Last move: ";
        while (*h) *o++ = *h++;
        for (const char *q = R_NAME[rate[bp]]; *q; q++) *o++ = *q;
        *o++ = ' '; *o++ = '(';
        for (const char *q = R_SYM[rate[bp]]; *q; q++) *o++ = *q;
        *o++ = ')'; *o = 0;
        *l2 = s2;
    } else if (check_sq >= 0) *l2 = "Check";
    else if (mode == 2) *l2 = "Two players";
    else *l2 = LEVEL_NAME[level];
}

void ui_render(void) {
    fill_rect(0, 0, UI_W, UI_H, C_BG);
    int top_side = flipped ? WHITE : BLACK, bot_side = flipped ? BLACK : WHITE;
    draw_strip(top_side, 0);
    draw_strip(bot_side, BY + 8 * SQ);

    const Pos *p = cur();
    int e = 1024;
    if (anim.on) { e = anim_e(); if (!e) e = 1024; }

    for (int sq = 0; sq < 64; sq++) {
        int x = sq_x(sq), y = sq_y(sq), r = sq >> 3, f = sq & 7;
        int dark = ((r + f) & 1) == 0;
        fill_rect(x, y, SQ, SQ, dark ? C_DARK : C_LIGHT);
        if (sq == last_from || sq == last_to) blend_rect(x, y, SQ, SQ, 0xFFFF33, 105);
        if (sq == sel) blend_rect(x, y, SQ, SQ, 0xFFFF33, 135);
        if (sq == hint_from || sq == hint_to) blend_rect(x, y, SQ, SQ, 0x4F8CFF, 120);
        if (sq == check_sq) blend_rect(x, y, SQ, SQ, 0xE5322D, 150);
        int fx = flipped ? 7 - f : f, rx = flipped ? r : 7 - r;
        u32 tc = dark ? C_LIGHT : C_DARK;
        if (rx == 7) { char c[2] = {(char)('a' + f), 0}; text(0, x + SQ - 11, y + SQ - 5, tc, c); }
        if (fx == 0) { char c[2] = {(char)('1' + r), 0}; text(0, x + 4, y + 14, tc, c); }
    }
    for (int sq = 0; sq < 64; sq++) {
        int v = p->b[sq];
        if (!v) continue;
        if (dragging && sq == sel) continue;
        if (anim.on && sq == anim.m.to) continue;
        draw_piece(v, sq_x(sq), sq_y(sq));
    }
    if (anim.on) {
        int a = sq_x(anim.m.from), b = sq_x(anim.m.to), c = sq_y(anim.m.from), d = sq_y(anim.m.to);
        draw_piece(anim.pc, a + (b - a) * e / 1024, c + (d - c) * e / 1024);
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
    if (sel >= 0 && !anim.on && !promo_on) {
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel || (legal[i].promo && legal[i].promo != QUEEN)) continue;
            int to = legal[i].to;
            int cx = sq_x(to) + 32, cy = sq_y(to) + 32;
            if (legal[i].flags & F_CAP) disc(cx, cy, 30, 25, 0x000000, 46);
            else disc(cx, cy, 11, 0, 0x000000, 46);
        }
    }
    if (dragging && sel >= 0) draw_piece(cur()->b[sel], mx - 32, my - 32);

    {   /* move rating badge: above and to the right of the piece */
        int bp = anim.on ? -1 : badge_ply();
        if (bp >= 0) draw_badge(sq_x(mvs[bp].to) + SQ - 13, sq_y(mvs[bp].to) + 13, rate[bp]);
    }

    /* panel */
    text(2, PX, 68, C_TEXT, "Swirl Chess");
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
        draw_button(B_RATE, rating_on ? "Rating: On" : "Rating: Off", rating_on);
    }
    text(0, PX + 2, 398, C_MUTED, "MOVES");
    fill_rrect(PX, 406, PW, 150, 8, C_CARD);
    {
        int rows = (hn + 1) / 2, first = rows > 6 ? rows - 6 : 0;
        for (int r = first; r < rows; r++) {
            int y = 406 + 10 + (r - first) * 22 + 15;
            char nb[10];
            int k = put_int(nb, r + 1);
            nb[k] = '.'; nb[k + 1] = 0;
            text(0, PX + 12, y, C_MUTED, nb);
            for (int c = 0; c < 2; c++) {
                int idx = r * 2 + c, l;
                if (idx >= hn) break;
                for (l = 0; san[idx][l]; l++) {}
                u32 col = idx == hn - 1 ? C_GREEN : C_TEXT;
                if (rating_on && rate[idx] && rate[idx] != R_BEST && rate[idx] != R_EXCELLENT && rate[idx] != R_GOOD && rate[idx] != R_BOOK) col = R_COL[rate[idx]];
                text(l > 7 ? 0 : 1, PX + (c ? 148 : 52), y, col, san[idx]);
            }
        }
        if (hn == 0) text(0, PX + 14, 430, C_MUTED, "Click, drag, or arrows + Enter.");
    }
    if (result && !anim.on) {
        int bx = BX + 106, by = BY + 186;
        fill_rrect(bx, by, 300, 140, 12, C_PANEL);
        text_c(2, bx + 150, by + 44, C_TEXT, l1);
        text_c(1, bx + 150, by + 70, C_MUTED, l2);
        int x = bx + 75, y = by + 86;
        fill_rrect(x, y, 150, 40, 8, hover == 100 ? C_GREEN_HOV : C_GREEN);
        text_c(1, x + 75, y + 26, 0x1B2410, "New game");
    }
    if (promo_on) {
        int cx0 = BX + 256 - 140, cy0 = BY + 256 - 52;
        blend_rect(BX, BY, 8 * SQ, 8 * SQ, 0x000000, 130);
        fill_rrect(cx0, cy0, 280, 104, 12, C_PANEL);
        text(1, cx0 + 14, cy0 + 22, C_TEXT, "Promote to");
        static const u8 PT[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
        int side = cur()->side == WHITE ? 1 : -1, hv = promo_cell(mx, my);
        for (int k = 0; k < 4; k++) {
            int x = cx0 + 12 + k * 68, y = cy0 + 30;
            fill_rrect(x, y, SQ, SQ, 8, hv == k ? C_BTN_HOV : C_BTN);
            draw_piece(side * PT[k], x, y);
        }
    }
}
