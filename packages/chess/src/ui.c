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

/* ------------------------------------------------------------ pieces */
#define BODY_W 0xFAF8F0
#define LINE_W 0x2D2D30
#define BODY_B 0x2E2E34
#define LINE_B 0x9C9CA4

static void draw_piece(int x, int y, int pc, int size, int alpha) {
    int t = (pc < 0 ? -pc : pc) - 1;
    if (t < 0 || t > 5) return;
    const unsigned char *fm = g_piece_fill + t * 4096;
    const unsigned char *lm = g_piece_line + t * 4096;
    u32 body = pc > 0 ? BODY_W : BODY_B, line = pc > 0 ? LINE_W : LINE_B;
    for (int dy = 0; dy < size; dy++) {
        int sy0 = dy * 64 / size, sy1 = (dy + 1) * 64 / size;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (int dx = 0; dx < size; dx++) {
            int sx0 = dx * 64 / size, sx1 = (dx + 1) * 64 / size;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            int sf = 0, sl = 0, cnt = 0;
            for (int sy = sy0; sy < sy1; sy++)
                for (int sx = sx0; sx < sx1; sx++) {
                    sf += fm[sy * 64 + sx];
                    sl += lm[sy * 64 + sx];
                    cnt++;
                }
            sf /= cnt;
            sl /= cnt;
            if (sf) put(x + dx, y + dy, body, sf * alpha / 255);
            if (sl) put(x + dx, y + dy, line, sl * alpha / 255);
        }
    }
}

/* ------------------------------------------------------------- state */
static Game G;
static TimeFn g_now;
static int mode;            /* 0 you=white vs CPU, 1 you=black vs CPU, 2 two players */
static int level = 3;
static int flipped;
static int sel = -1, dragging, mx, my;
static int promo_from = -1, promo_to = -1;
static Move legal[MAX_MOVES];
static int nlegal;
static int last_from = -1, last_to = -1;
static int hint_from = -1, hint_to = -1;
static char hint_san[12];
static int ai_shown;
static int dirty = 1;
static int hover = -1;
static int kcur = 12;          /* keyboard cursor square (e2) */
static int kb_active;
#ifndef DBG_DEFAULT
#define DBG_DEFAULT 1      /* the Windows build shows the input readout until D is pressed */
#endif
static int dbg_on = DBG_DEFAULT;
int ui_dbg[16];
static int eng_state, eng_a, eng_b;
static u32 t_start, hb_last;

static const char *LEVEL_NAME[5] = {"", "Beginner", "Casual", "Club", "Strong"};

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

static const Pos *cur(void) { return &G.pos[G.n]; }
static int human_turn(void) {
    if (G.result != RES_NONE) return 0;
    if (mode == 2) return 1;
    return cur()->side == (mode == 0 ? WHITE : BLACK);
}
static int ai_turn(void) { return G.result == RES_NONE && !human_turn(); }

static void refresh_legal(void) { nlegal = legal_moves(cur(), legal); }

static void clear_sel(void) { sel = -1; dragging = 0; promo_from = promo_to = -1; }

static void new_game(void) {
    game_new(&G);
    flipped = (mode == 1);
    clear_sel();
    last_from = last_to = hint_from = hint_to = -1;
    ai_shown = 0;
    refresh_legal();
    dirty = 1;
}

static void play(Move m) {
    if (!game_play(&G, m)) return;
    last_from = MV_FROM(m);
    last_to = MV_TO(m);
    hint_from = hint_to = -1;
    clear_sel();
    refresh_legal();
    dirty = 1;
}

static void do_undo(void) {
    int minp = (mode == 1) ? 1 : 0;
    if (G.n <= minp) return;
    if (mode == 2) game_undo(&G, 1);
    else do game_undo(&G, 1); while (G.n > minp && !human_turn());
    if (G.n > 0) { last_from = MV_FROM(G.mv[G.n - 1]); last_to = MV_TO(G.mv[G.n - 1]); }
    else last_from = last_to = -1;
    hint_from = hint_to = -1;
    clear_sel();
    refresh_legal();
    dirty = 1;
}

static void do_hint(void) {
    if (!human_turn()) return;
    Move m = ai_pick(cur(), 3, G.hash, G.n + 1, g_now);
    if (!m) return;
    hint_from = MV_FROM(m);
    hint_to = MV_TO(m);
    move_to_san(cur(), m, hint_san);
    dirty = 1;
}

/* ----------------------------------------------------------- geometry */
static int sq_x(int sq) { int f = sq & 7; if (flipped) f = 7 - f; return BX + f * SQ; }
static int sq_y(int sq) { int r = sq >> 3; if (!flipped) r = 7 - r; return BY + r * SQ; }
static int sq_at(int x, int y) {
    if (x < BX || y < BY) return -1;
    int f = (x - BX) / SQ, r = (y - BY) / SQ;
    if (f > 7 || r > 7) return -1;
    if (flipped) f = 7 - f; else r = 7 - r;
    return r * 8 + f;
}

static int own_piece(int sq) {
    int pc = cur()->b[sq];
    return cur()->side == WHITE ? pc > 0 : pc < 0;
}

/* returns 1 if the click was consumed (move played or promotion chooser opened) */
static int try_move(int from, int to) {
    int cnt = 0, promo = 0;
    for (int i = 0; i < nlegal; i++)
        if (MV_FROM(legal[i]) == from && MV_TO(legal[i]) == to) {
            cnt++;
            if (MV_PROMO(legal[i])) promo = 1;
        }
    if (!cnt) return 0;
    if (promo) {
        promo_from = from;
        promo_to = to;
        dragging = 0;
        dirty = 1;
        return 1;
    }
    play(game_find(&G, from, to, 0));
    return 1;
}

static int promo_card_y(int idx) {
    int ty = sq_y(promo_to);
    return ty == BY ? ty + idx * SQ : ty - idx * SQ;
}

static const int PROMO_ORDER[4] = {QUEEN, ROOK, BISHOP, KNIGHT};

static int over_banner_button(int x, int y) {
    int bx = BX + 106 + 75, by = BY + 186 + 86;
    return x >= bx && x < bx + 150 && y >= by && y < by + 40;
}

void ui_mouse(int type, int x, int y) {
    mx = x;
    my = y;
    if (type == 0) {
        int h = btn_at(x, y);
        if (G.result != RES_NONE && over_banner_button(x, y)) h = 100;
        if (h != hover) { hover = h; dirty = 1; }
        if (dragging) dirty = 1;
        return;
    }
    if (type == 1) {
        if (promo_from >= 0) {
            int f = sq_x(promo_to);
            for (int i = 0; i < 4; i++) {
                int cy = promo_card_y(i);
                if (x >= f && x < f + SQ && y >= cy && y < cy + SQ) {
                    play(game_find(&G, promo_from, promo_to, PROMO_ORDER[i]));
                    return;
                }
            }
            clear_sel();
            dirty = 1;
            return;
        }
        if (G.result != RES_NONE && over_banner_button(x, y)) { new_game(); return; }
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
        if (own_piece(sq)) {
            sel = sq;
            dragging = 1;
            hint_from = hint_to = -1;
        } else {
            clear_sel();
        }
        dirty = 1;
        return;
    }
    if (type == 2) {
        if (!dragging) return;
        dragging = 0;
        dirty = 1;
        int sq = sq_at(x, y);
        if (sq < 0) { sel = -1; return; }
        if (sq == sel) return;                /* click-click mode: stay selected */
        if (try_move(sel, sq)) return;
        if (own_piece(sq)) sel = sq; else sel = -1;
    }
}

/* move the keyboard cursor in screen space (arrows always mean up/down/left/right on screen) */
static void kmove(int dc, int dr) {
    int f = kcur & 7, r = kcur >> 3;
    int col = flipped ? 7 - f : f, row = flipped ? r : 7 - r;
    col += dc;
    row += dr;
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
    if (own_piece(kcur)) { sel = kcur; hint_from = hint_to = -1; }
    else clear_sel();
}

void ui_key(int vk) {
    if (promo_from >= 0) {            /* promotion chooser: Q R B N, Esc cancels */
        int pr = vk == 'Q' ? QUEEN : vk == 'R' ? ROOK : vk == 'B' ? BISHOP : vk == 'N' ? KNIGHT : 0;
        if (pr) play(game_find(&G, promo_from, promo_to, pr));
        else if (vk == 27) { clear_sel(); dirty = 1; }
        return;
    }
    switch (vk) {
        case 37: kmove(-1, 0); return;      /* arrows */
        case 39: kmove(1, 0); return;
        case 38: kmove(0, -1); return;
        case 40: kmove(0, 1); return;
        case 13: case 32: kselect(); return;
        case 'D': dbg_on ^= 1; dirty = 1; return;
    }
    switch (vk) {
        case 'N': new_game(); break;
        case 'U': case 8: do_undo(); break;
        case 'F': flipped ^= 1; dirty = 1; break;
        case 'H': do_hint(); break;
        case 27: clear_sel(); dirty = 1; break;
        case 'L': level = level % 4 + 1; dirty = 1; break;
    }
}

int ui_step(void) {
    if (ai_turn()) {
        if (!ai_shown) { ai_shown = 1; dirty = 0; return 1; }   /* show "thinking" first */
        ai_shown = 0;
        Move m = ai_pick(cur(), level, G.hash, G.n + 1, g_now);
        if (m) play(m);
        dirty = 0;
        return 1;
    }
    ai_shown = 0;
    if (dbg_on && g_now) {            /* heartbeat: redraw once a second so a frozen app is obvious */
        u32 t = g_now();
        if (t - hb_last >= 1000) { hb_last = t; dirty = 1; }
    }
    int d = dirty;
    dirty = 0;
    return d;
}

void ui_engine_status(int state, int a, int b) {
    eng_state = state;
    eng_a = a;
    eng_b = b;
    dirty = 1;
}

void ui_init(TimeFn now, u32 seed) {
    g_now = now;
    t_start = hb_last = now ? now() : 0;
    ai_seed(seed);
    mode = 0;
    level = 3;
    new_game();
}

void ui_test_load(const char *fen, int fl, int sel_sq) {
    game_new(&G);
    pos_from_fen(&G.pos[0], fen);
    G.hash[0] = pos_hash(&G.pos[0]);
    flipped = fl;
    sel = sel_sq;
    refresh_legal();
}

/* ------------------------------------------------------------ render */
static const char *name_of(int side, char *buf) {
    char *o = buf;
    const char *s;
    int cpu = (mode == 0 && side == BLACK) || (mode == 1 && side == WHITE);
    if (mode == 2) s = side == WHITE ? "White" : "Black";
    else if (cpu) s = "Computer";
    else s = "You";
    while (*s) *o++ = *s++;
    if (cpu) {
        *o++ = ' ';
        *o++ = '(';
        s = LEVEL_NAME[level];
        while (*s) *o++ = *s++;
        *o++ = ')';
    }
    *o = 0;
    return buf;
}

static const int PVAL[7] = {0, 1, 3, 3, 5, 9, 0};

/* draw the pieces `side` has captured, plus the material lead, in a strip at y */
static void draw_strip(int side, int y) {
    const Pos *p = cur();
    int have[2][7] = {{0}};
    int mat[2] = {0, 0};
    for (int i = 0; i < 64; i++) {
        int pc = p->b[i];
        if (!pc) continue;
        int c = pc > 0 ? 0 : 1, t = pc > 0 ? pc : -pc;
        have[c][t]++;
        mat[c] += PVAL[t];
    }
    static const int START[7] = {0, 8, 2, 2, 2, 1, 1};
    char nb[48];
    name_of(side, nb);
    disc(BX + 5, y + 21, 5, 0, side == WHITE ? 0xF0F0F0 : 0x1A1A1A, 255);
    if (side == BLACK) disc(BX + 5, y + 21, 5, 4, 0x8A8A8A, 255);
    text(1, BX + 18, y + 27, C_TEXT, nb);
    int x = BX + 18 + text_w(1, nb) + 14;
    int enemy = side ^ 1;
    static const int ORD[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    for (int k = 0; k < 5; k++) {
        int t = ORD[k];
        int miss = START[t] - have[enemy][t];
        for (int j = 0; j < miss; j++) {
            draw_piece(x, y + 8, enemy == WHITE ? t : -t, 26, 255);
            x += (t == PAWN) ? 11 : 15;
        }
        if (miss > 0) x += 6;
    }
    int lead = mat[side] - mat[enemy];
    if (lead > 0) {
        char b[12] = "+";
        put_int(b + 1, lead);
        text(0, x + 2, y + 27, C_MUTED, b);
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
    switch (G.result) {
        case RES_WHITE_WINS: *l1 = "Checkmate"; *l2 = "White wins"; return;
        case RES_BLACK_WINS: *l1 = "Checkmate"; *l2 = "Black wins"; return;
        case RES_STALEMATE: *l1 = "Draw"; *l2 = "Stalemate"; return;
        case RES_FIFTY: *l1 = "Draw"; *l2 = "50-move rule"; return;
        case RES_MATERIAL: *l1 = "Draw"; *l2 = "Insufficient material"; return;
        case RES_REPETITION: *l1 = "Draw"; *l2 = "Threefold repetition"; return;
    }
    int chk = in_check(cur(), cur()->side);
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
    } else if (chk) *l2 = "Check!";
    else if (mode == 2) *l2 = "Two players";
    else *l2 = LEVEL_NAME[level];
}

void ui_render(void) {
    fill_rect(0, 0, UI_W, UI_H, C_BG);

    /* player strips */
    int top_side = flipped ? WHITE : BLACK, bot_side = flipped ? BLACK : WHITE;
    draw_strip(top_side, 0);
    draw_strip(bot_side, BY + 8 * SQ);

    /* board */
    const Pos *p = cur();
    int ksq_chk = in_check(p, p->side) ? p->ksq[p->side] : -1;
    for (int sq = 0; sq < 64; sq++) {
        int x = sq_x(sq), y = sq_y(sq);
        int r = sq >> 3, f = sq & 7;
        int dark = ((r + f) & 1) == 0;
        fill_rect(x, y, SQ, SQ, dark ? C_DARK : C_LIGHT);
        if (sq == last_from || sq == last_to) blend_rect(x, y, SQ, SQ, 0xFFFF33, 105);
        if (sq == sel) blend_rect(x, y, SQ, SQ, 0xFFFF33, 135);
        if (sq == hint_from || sq == hint_to) blend_rect(x, y, SQ, SQ, 0x4F8CFF, 120);
        if (sq == ksq_chk) {
            int R = 31, R2 = R * R;
            for (int yy = 0; yy < SQ; yy++)
                for (int xx = 0; xx < SQ; xx++) {
                    int dx = xx - 32, dy = yy - 32, d2 = dx * dx + dy * dy;
                    if (d2 < R2) put(x + xx, y + yy, 0xE0201A, 215 * (R2 - d2) / R2);
                }
        }
        /* coordinates */
        int fx = flipped ? 7 - f : f, rx = flipped ? r : 7 - r;
        u32 tc = dark ? C_LIGHT : C_DARK;
        if (rx == 7) { char c[2] = {(char)('a' + f), 0}; text(0, x + SQ - 11, y + SQ - 5, tc, c); }
        if (fx == 0) { char c[2] = {(char)('1' + r), 0}; text(0, x + 4, y + 14, tc, c); }
    }
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->b[sq];
        if (!pc) continue;
        if (dragging && sq == sel) continue;
        draw_piece(sq_x(sq), sq_y(sq), pc, SQ, 255);
    }
    if (kb_active && promo_from < 0) {
        int x = sq_x(kcur), y = sq_y(kcur);
        for (int i = 0; i < 4; i++) {
            fill_rect(x + i, y, 1, SQ, 0x3C8CFF);
            fill_rect(x + SQ - 1 - i, y, 1, SQ, 0x3C8CFF);
            fill_rect(x, y + i, SQ, 1, 0x3C8CFF);
            fill_rect(x, y + SQ - 1 - i, SQ, 1, 0x3C8CFF);
        }
    }
    /* legal move hints */
    if (sel >= 0 && promo_from < 0) {
        for (int i = 0; i < nlegal; i++) {
            if (MV_FROM(legal[i]) != sel) continue;
            int to = MV_TO(legal[i]);
            int cx = sq_x(to) + 32, cy = sq_y(to) + 32;
            if (p->b[to] || (MV_FLAGS(legal[i]) & F_EP)) disc(cx, cy, 31, 25, 0x000000, 46);
            else disc(cx, cy, 11, 0, 0x000000, 46);
        }
    }
    if (dragging && sel >= 0) draw_piece(mx - 32, my - 32, p->b[sel], SQ, 255);

    /* panel */
    text(2, PX, 68, C_TEXT, "Swirl Chess");
    {
        char st[32], *o = st;
        const char *t;
        u32 col;
        if (eng_state == 1) { t = "Engine loaded"; col = C_GREEN; while (*t) *o++ = *t++; }
        else if (eng_state == 2) {
            t = "FAILED E"; col = 0xE5484D; while (*t) *o++ = *t++;
            o += put_int(o, eng_a);
        } else {
            t = "Testing "; col = C_MUTED; while (*t) *o++ = *t++;
            o += put_int(o, eng_a);
            *o++ = '/';
            o += put_int(o, eng_b);
        }
        *o = 0;
        text(0, PX + PW - text_w(0, st), 66, col, st);
    }
    fill_rrect(PX, 82, PW, 54, 8, C_CARD);
    const char *l1, *l2;
    status_text(&l1, &l2);
    text(1, PX + 14, 105, G.result != RES_NONE ? C_GREEN : C_TEXT, l1);
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
        int rows = (G.n + 1) / 2, first = rows > 8 ? rows - 8 : 0;
        for (int r = first; r < rows; r++) {
            int y = 360 + 12 + (r - first) * 22 + 15;
            char nb[10];
            int k = put_int(nb, r + 1);
            nb[k] = '.'; nb[k + 1] = 0;
            text(0, PX + 12, y, C_MUTED, nb);
            int wi = r * 2, bi = r * 2 + 1;
            text(1, PX + 56, y, wi == G.n - 1 ? C_GREEN : C_TEXT, G.san[wi]);
            if (bi < G.n) text(1, PX + 146, y, bi == G.n - 1 ? C_GREEN : C_TEXT, G.san[bi]);
        }
        if (G.n == 0) text(0, PX + 14, 384, C_MUTED, "Click, drag, or arrows + Enter.");
    }

    /* promotion chooser */
    if (promo_from >= 0) {
        blend_rect(BX, BY, 8 * SQ, 8 * SQ, 0x000000, 110);
        int x = sq_x(promo_to);
        int side = p->side;
        for (int i = 0; i < 4; i++) {
            int y = promo_card_y(i);
            fill_rect(x, y, SQ, SQ, 0xF2F2F2);
            fill_rect(x + 1, y + 1, SQ - 2, SQ - 2, (mx >= x && mx < x + SQ && my >= y && my < y + SQ) ? 0xCFE6A8 : 0xFFFFFF);
            draw_piece(x, y, side == WHITE ? PROMO_ORDER[i] : -PROMO_ORDER[i], SQ, 255);
        }
    }

    /* game over banner */
    if (G.result != RES_NONE) {
        int bx = BX + 106, by = BY + 186;
        fill_rrect(bx, by, 300, 140, 12, C_PANEL);
        text_c(2, bx + 150, by + 44, C_TEXT, l1);
        text_c(1, bx + 150, by + 70, C_MUTED, l2);
        int x = bx + 75, y = by + 86;
        fill_rrect(x, y, 150, 40, 8, hover == 100 ? C_GREEN_HOV : C_GREEN);
        text_c(1, x + 75, y + 26, 0x1B2410, "New game");
    }

    if (dbg_on) {
        char a[64], b[64], c[64], *o;
        const char *t;
        ui_dbg[10]++;
        if (g_now) ui_dbg[12] = (int)((g_now() - t_start) / 1000);
        o = a;
        t = "mv "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[0]);
        t = "  dn "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[1]);
        t = "  up "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[2]);
        t = "  key "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[3]);
        *o = 0;
        o = b;
        t = "raw "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[4]);
        *o++ = ',';
        o += put_int(o, ui_dbg[5]);
        t = " > "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[6]);
        *o++ = ',';
        o += put_int(o, ui_dbg[7]);
        t = "  win "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[8]);
        *o++ = 'x';
        o += put_int(o, ui_dbg[9]);
        *o = 0;
        o = c;
        t = "up "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[12]);
        t = "s  draw "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[11]);
        t = "ms  loop "; while (*t) *o++ = *t++;
        o += put_int(o, ui_dbg[13]);
        *o = 0;
        text(0, PX, 560, C_MUTED, a);
        text(0, PX, 574, C_MUTED, b);
        text(0, PX, 588, C_MUTED, c);
    }
}
