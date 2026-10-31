#include "engine.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

static const i8 KN_DR[8] = {2, 2, 1, 1, -1, -1, -2, -2};
static const i8 KN_DF[8] = {1, -1, 2, -2, 2, -2, 1, -1};
static const i8 K_DR[8] = {1, 1, 1, 0, 0, -1, -1, -1};
static const i8 K_DF[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const i8 B_DR[4] = {1, 1, -1, -1};
static const i8 B_DF[4] = {1, -1, 1, -1};
static const i8 R_DR[4] = {1, -1, 0, 0};
static const i8 R_DF[4] = {0, 0, 1, -1};

/* ------------------------------------------------------------------ setup */

void pos_start(Pos *p) {
    static const i8 back[8] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int i = 0; i < 64; i++) p->b[i] = 0;
    for (int f = 0; f < 8; f++) {
        p->b[f] = back[f];
        p->b[8 + f] = PAWN;
        p->b[48 + f] = -PAWN;
        p->b[56 + f] = -back[f];
    }
    p->side = WHITE;
    p->castle = 15;
    p->ep = -1;
    p->ksq[0] = 4;
    p->ksq[1] = 60;
    p->half = 0;
    p->full = 1;
}

int pos_from_fen(Pos *p, const char *s) {
    for (int i = 0; i < 64; i++) p->b[i] = 0;
    p->castle = 0;
    p->ep = -1;
    p->half = 0;
    p->full = 1;
    p->ksq[0] = p->ksq[1] = 0;
    int r = 7, f = 0;
    while (*s && *s != ' ') {
        char c = *s++;
        if (c == '/') { r--; f = 0; continue; }
        if (c >= '1' && c <= '8') { f += c - '0'; continue; }
        int t = 0, col = 1;
        char lc = c;
        if (c >= 'A' && c <= 'Z') { lc = (char)(c + 32); col = 1; } else col = -1;
        switch (lc) {
            case 'p': t = PAWN; break;
            case 'n': t = KNIGHT; break;
            case 'b': t = BISHOP; break;
            case 'r': t = ROOK; break;
            case 'q': t = QUEEN; break;
            case 'k': t = KING; break;
            default: return 0;
        }
        if (r < 0 || f > 7) return 0;
        p->b[r * 8 + f] = (i8)(t * col);
        if (t == KING) p->ksq[col == 1 ? 0 : 1] = (u8)(r * 8 + f);
        f++;
    }
    while (*s == ' ') s++;
    p->side = (*s == 'b') ? BLACK : WHITE;
    if (*s) s++;
    while (*s == ' ') s++;
    while (*s && *s != ' ') {
        if (*s == 'K') p->castle |= 1;
        else if (*s == 'Q') p->castle |= 2;
        else if (*s == 'k') p->castle |= 4;
        else if (*s == 'q') p->castle |= 8;
        s++;
    }
    while (*s == ' ') s++;
    if (*s && *s != '-') {
        int ef = *s - 'a';
        int er = s[1] - '1';
        if (ef >= 0 && ef < 8 && er >= 0 && er < 8) p->ep = (i8)(er * 8 + ef);
        while (*s && *s != ' ') s++;
    } else if (*s) s++;
    while (*s == ' ') s++;
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    p->half = (u8)v;
    while (*s == ' ') s++;
    v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    p->full = (u16)(v ? v : 1);
    return 1;
}

/* ------------------------------------------------------------------- hash */

static u64 zk[12][64], z_side, z_castle[16], z_ep[8];
static int z_ready;

static u64 splitmix(u64 *x) {
    u64 z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void z_init(void) {
    u64 x = 0x5317C0DE5EEDULL;
    for (int i = 0; i < 12; i++)
        for (int j = 0; j < 64; j++) zk[i][j] = splitmix(&x);
    z_side = splitmix(&x);
    for (int i = 0; i < 16; i++) z_castle[i] = splitmix(&x);
    for (int i = 0; i < 8; i++) z_ep[i] = splitmix(&x);
    z_ready = 1;
}

u64 pos_hash(const Pos *p) {
    if (!z_ready) z_init();
    u64 h = 0;
    for (int i = 0; i < 64; i++) {
        int pc = p->b[i];
        if (pc) h ^= zk[(pc > 0 ? pc - 1 : 5 - pc)][i];
    }
    if (p->side) h ^= z_side;
    h ^= z_castle[p->castle & 15];
    if (p->ep >= 0) h ^= z_ep[p->ep & 7];
    return h;
}

/* ---------------------------------------------------------------- attacks */

int attacked(const Pos *p, int sq, int by) {
    int r = sq >> 3, f = sq & 7;
    int sgn = by == WHITE ? 1 : -1;
    int pr = by == WHITE ? r - 1 : r + 1;
    if ((unsigned)pr < 8) {
        if (f > 0 && p->b[pr * 8 + f - 1] == sgn * PAWN) return 1;
        if (f < 7 && p->b[pr * 8 + f + 1] == sgn * PAWN) return 1;
    }
    for (int i = 0; i < 8; i++) {
        int rr = r + KN_DR[i], ff = f + KN_DF[i];
        if ((unsigned)rr < 8 && (unsigned)ff < 8 && p->b[rr * 8 + ff] == sgn * KNIGHT) return 1;
        rr = r + K_DR[i];
        ff = f + K_DF[i];
        if ((unsigned)rr < 8 && (unsigned)ff < 8 && p->b[rr * 8 + ff] == sgn * KING) return 1;
    }
    for (int d = 0; d < 4; d++) {
        int rr = r + B_DR[d], ff = f + B_DF[d];
        while ((unsigned)rr < 8 && (unsigned)ff < 8) {
            int pc = p->b[rr * 8 + ff];
            if (pc) {
                if (pc == sgn * BISHOP || pc == sgn * QUEEN) return 1;
                break;
            }
            rr += B_DR[d];
            ff += B_DF[d];
        }
        rr = r + R_DR[d];
        ff = f + R_DF[d];
        while ((unsigned)rr < 8 && (unsigned)ff < 8) {
            int pc = p->b[rr * 8 + ff];
            if (pc) {
                if (pc == sgn * ROOK || pc == sgn * QUEEN) return 1;
                break;
            }
            rr += R_DR[d];
            ff += R_DF[d];
        }
    }
    return 0;
}

int in_check(const Pos *p, int side) { return attacked(p, p->ksq[side], side ^ 1); }

/* ------------------------------------------------------------- generation */

static inline void add(Move *out, int *n, int from, int to, int promo, int fl) {
    out[(*n)++] = MV_MAKE(from, to, promo, fl);
}

int gen_moves(const Pos *p, Move *out, int caps_only) {
    int n = 0;
    int side = p->side;
    int sgn = side ? -1 : 1;
    int dr = side ? -1 : 1;
    int start_rank = side ? 6 : 1;
    int promo_rank = side ? 0 : 7;
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->b[sq] * sgn;
        if (pc <= 0) continue;
        int r = sq >> 3, f = sq & 7;
        if (pc == PAWN) {
            int nr = r + dr;
            int to = nr * 8 + f;
            if (!p->b[to]) {
                if (nr == promo_rank) {
                    add(out, &n, sq, to, QUEEN, 0);
                    if (!caps_only) {
                        add(out, &n, sq, to, ROOK, 0);
                        add(out, &n, sq, to, BISHOP, 0);
                        add(out, &n, sq, to, KNIGHT, 0);
                    }
                } else if (!caps_only) {
                    add(out, &n, sq, to, 0, 0);
                    if (r == start_rank && !p->b[to + 8 * dr]) add(out, &n, sq, to + 8 * dr, 0, F_DBL);
                }
            }
            for (int df = -1; df <= 1; df += 2) {
                int ff = f + df;
                if ((unsigned)ff >= 8) continue;
                int t = nr * 8 + ff;
                int tp = p->b[t] * sgn;
                if (tp < 0) {
                    if (nr == promo_rank) {
                        add(out, &n, sq, t, QUEEN, F_CAP);
                        add(out, &n, sq, t, ROOK, F_CAP);
                        add(out, &n, sq, t, BISHOP, F_CAP);
                        add(out, &n, sq, t, KNIGHT, F_CAP);
                    } else add(out, &n, sq, t, 0, F_CAP);
                } else if (t == p->ep && tp == 0) {
                    add(out, &n, sq, t, 0, F_CAP | F_EP);
                }
            }
        } else if (pc == KNIGHT || pc == KING) {
            const i8 *DR = pc == KNIGHT ? KN_DR : K_DR;
            const i8 *DF = pc == KNIGHT ? KN_DF : K_DF;
            for (int i = 0; i < 8; i++) {
                int rr = r + DR[i], ff = f + DF[i];
                if ((unsigned)rr >= 8 || (unsigned)ff >= 8) continue;
                int t = rr * 8 + ff;
                int tp = p->b[t] * sgn;
                if (tp > 0) continue;
                if (tp < 0) add(out, &n, sq, t, 0, F_CAP);
                else if (!caps_only) add(out, &n, sq, t, 0, 0);
            }
        } else {
            int first = pc == ROOK ? 4 : 0;   /* unused marker */
            (void)first;
            for (int pass = 0; pass < 2; pass++) {
                if (pass == 0 && pc == ROOK) continue;
                if (pass == 1 && pc == BISHOP) continue;
                const i8 *DR = pass == 0 ? B_DR : R_DR;
                const i8 *DF = pass == 0 ? B_DF : R_DF;
                for (int d = 0; d < 4; d++) {
                    int rr = r + DR[d], ff = f + DF[d];
                    while ((unsigned)rr < 8 && (unsigned)ff < 8) {
                        int t = rr * 8 + ff;
                        int tp = p->b[t] * sgn;
                        if (tp > 0) break;
                        if (tp < 0) {
                            add(out, &n, sq, t, 0, F_CAP);
                            break;
                        }
                        if (!caps_only) add(out, &n, sq, t, 0, 0);
                        rr += DR[d];
                        ff += DF[d];
                    }
                }
            }
        }
    }
    if (!caps_only) {
        if (side == WHITE && p->b[4] == KING && !attacked(p, 4, BLACK)) {
            if ((p->castle & 1) && p->b[7] == ROOK && !p->b[5] && !p->b[6] && !attacked(p, 5, BLACK))
                add(out, &n, 4, 6, 0, F_CASTLE);
            if ((p->castle & 2) && p->b[0] == ROOK && !p->b[1] && !p->b[2] && !p->b[3] && !attacked(p, 3, BLACK))
                add(out, &n, 4, 2, 0, F_CASTLE);
        } else if (side == BLACK && p->b[60] == -KING && !attacked(p, 60, WHITE)) {
            if ((p->castle & 4) && p->b[63] == -ROOK && !p->b[61] && !p->b[62] && !attacked(p, 61, WHITE))
                add(out, &n, 60, 62, 0, F_CASTLE);
            if ((p->castle & 8) && p->b[56] == -ROOK && !p->b[57] && !p->b[58] && !p->b[59] && !attacked(p, 59, WHITE))
                add(out, &n, 60, 58, 0, F_CASTLE);
        }
    }
    return n;
}

static int castle_mask(int sq) {
    switch (sq) {
        case 0: return ~2;
        case 7: return ~1;
        case 4: return ~3;
        case 56: return ~8;
        case 63: return ~4;
        case 60: return ~12;
        default: return ~0;
    }
}

int make_move(const Pos *p, Move m, Pos *o) {
    *o = *p;
    int from = MV_FROM(m), to = MV_TO(m), fl = MV_FLAGS(m), promo = MV_PROMO(m);
    int side = p->side;
    int pc = o->b[from];
    o->b[from] = 0;
    if (fl & F_EP) o->b[side ? to + 8 : to - 8] = 0;
    o->b[to] = (i8)(promo ? (side ? -promo : promo) : pc);
    if (ABS(pc) == KING) {
        o->ksq[side] = (u8)to;
        if (fl & F_CASTLE) {
            if (to == 6) { o->b[5] = o->b[7]; o->b[7] = 0; }
            else if (to == 2) { o->b[3] = o->b[0]; o->b[0] = 0; }
            else if (to == 62) { o->b[61] = o->b[63]; o->b[63] = 0; }
            else if (to == 58) { o->b[59] = o->b[56]; o->b[56] = 0; }
        }
    }
    o->castle = (u8)(o->castle & castle_mask(from) & castle_mask(to));
    o->ep = (fl & F_DBL) ? (i8)((from + to) / 2) : -1;
    if (ABS(pc) == PAWN || (fl & F_CAP)) o->half = 0;
    else o->half++;
    if (side == BLACK) o->full++;
    o->side = (u8)(side ^ 1);
    return !attacked(o, o->ksq[side], side ^ 1);
}

int legal_moves(const Pos *p, Move *out) {
    Move tmp[MAX_MOVES];
    Pos q;
    int n = gen_moves(p, tmp, 0), k = 0;
    for (int i = 0; i < n; i++)
        if (make_move(p, tmp[i], &q)) out[k++] = tmp[i];
    return k;
}

u64 perft(const Pos *p, int depth) {
    Move mv[MAX_MOVES];
    Pos q;
    int n = gen_moves(p, mv, 0);
    u64 total = 0;
    for (int i = 0; i < n; i++) {
        if (!make_move(p, mv[i], &q)) continue;
        total += depth <= 1 ? 1 : perft(&q, depth - 1);
    }
    return total;
}

/* -------------------------------------------------------------- notation */

static const char PIECE_LETTER[7] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};

void move_to_uci(Move m, char *o) {
    int from = MV_FROM(m), to = MV_TO(m), pr = MV_PROMO(m);
    o[0] = (char)('a' + (from & 7));
    o[1] = (char)('1' + (from >> 3));
    o[2] = (char)('a' + (to & 7));
    o[3] = (char)('1' + (to >> 3));
    int k = 4;
    if (pr) o[k++] = "  nbrq"[pr];
    o[k] = 0;
}

void move_to_san(const Pos *p, Move m, char *o) {
    char *s = o;
    int from = MV_FROM(m), to = MV_TO(m), fl = MV_FLAGS(m), promo = MV_PROMO(m);
    int pc = ABS(p->b[from]);
    if (fl & F_CASTLE) {
        const char *t = (to & 7) == 6 ? "O-O" : "O-O-O";
        while (*t) *s++ = *t++;
    } else {
        if (pc != PAWN) {
            *s++ = PIECE_LETTER[pc];
            Move lm[MAX_MOVES];
            int n = legal_moves(p, lm);
            int amb = 0, same_f = 0, same_r = 0;
            for (int i = 0; i < n; i++) {
                int ofrom = MV_FROM(lm[i]);
                if (lm[i] == m || MV_TO(lm[i]) != to || ofrom == from) continue;
                if (ABS(p->b[ofrom]) != pc) continue;
                amb = 1;
                if ((ofrom & 7) == (from & 7)) same_f = 1;
                if ((ofrom >> 3) == (from >> 3)) same_r = 1;
            }
            if (amb) {
                if (!same_f) *s++ = (char)('a' + (from & 7));
                else if (!same_r) *s++ = (char)('1' + (from >> 3));
                else {
                    *s++ = (char)('a' + (from & 7));
                    *s++ = (char)('1' + (from >> 3));
                }
            }
        } else if (fl & F_CAP) {
            *s++ = (char)('a' + (from & 7));
        }
        if (fl & F_CAP) *s++ = 'x';
        *s++ = (char)('a' + (to & 7));
        *s++ = (char)('1' + (to >> 3));
        if (promo) {
            *s++ = '=';
            *s++ = PIECE_LETTER[promo];
        }
    }
    Pos q;
    make_move(p, m, &q);
    if (in_check(&q, q.side)) {
        Move l2[MAX_MOVES];
        *s++ = legal_moves(&q, l2) == 0 ? '#' : '+';
    }
    *s = 0;
}

/* ------------------------------------------------------------------ game */

static int insufficient(const Pos *p) {
    int minors = 0, bishops_dark = 0, bishops_light = 0, knights = 0;
    for (int i = 0; i < 64; i++) {
        int pc = ABS(p->b[i]);
        if (!pc || pc == KING) continue;
        if (pc == PAWN || pc == ROOK || pc == QUEEN) return 0;
        minors++;
        if (pc == KNIGHT) knights++;
        else if ((((i >> 3) + (i & 7)) & 1) == 0) bishops_dark++;
        else bishops_light++;
    }
    if (minors <= 1) return 1;
    if (knights == 0 && (bishops_dark == 0 || bishops_light == 0)) return 1;
    return 0;
}

int game_repetitions(const Game *g) {
    int c = 1;
    int lim = g->n - g->pos[g->n].half;
    if (lim < 0) lim = 0;
    for (int i = g->n - 2; i >= lim; i -= 2)
        if (g->hash[i] == g->hash[g->n]) c++;
    return c;
}

static void game_eval_result(Game *g) {
    const Pos *p = &g->pos[g->n];
    Move l[MAX_MOVES];
    int n = legal_moves(p, l);
    g->result = RES_NONE;
    if (n == 0) {
        if (in_check(p, p->side)) g->result = p->side == WHITE ? RES_BLACK_WINS : RES_WHITE_WINS;
        else g->result = RES_STALEMATE;
    } else if (insufficient(p)) g->result = RES_MATERIAL;
    else if (p->half >= 100) g->result = RES_FIFTY;
    else if (game_repetitions(g) >= 3) g->result = RES_REPETITION;
}

void game_new(Game *g) {
    pos_start(&g->pos[0]);
    g->hash[0] = pos_hash(&g->pos[0]);
    g->n = 0;
    g->result = RES_NONE;
}

int game_play(Game *g, Move m) {
    if (g->n >= MAX_PLY || g->result != RES_NONE) return 0;
    move_to_san(&g->pos[g->n], m, g->san[g->n]);
    if (!make_move(&g->pos[g->n], m, &g->pos[g->n + 1])) return 0;
    g->mv[g->n] = m;
    g->n++;
    g->hash[g->n] = pos_hash(&g->pos[g->n]);
    game_eval_result(g);
    return 1;
}

void game_undo(Game *g, int plies) {
    g->n -= plies;
    if (g->n < 0) g->n = 0;
    g->result = RES_NONE;
}

Move game_find(const Game *g, int from, int to, int promo) {
    Move l[MAX_MOVES];
    int n = legal_moves(&g->pos[g->n], l);
    for (int i = 0; i < n; i++)
        if (MV_FROM(l[i]) == from && MV_TO(l[i]) == to && MV_PROMO(l[i]) == promo) return l[i];
    return 0;
}

/* -------------------------------------------------------------------- AI */

#define INF 32000
#define MATE 30000

static const int VAL[7] = {0, 100, 320, 330, 500, 900, 0};

/* Piece-square tables, white's view, first row = rank 8. */
static const i8 PST[7][64] = {
    {0},
    {0, 0, 0, 0, 0, 0, 0, 0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
     5, 5, 10, 25, 25, 10, 5, 5,
     0, 0, 0, 20, 20, 0, 0, 0,
     5, -5, -10, 0, 0, -10, -5, 5,
     5, 10, 10, -20, -20, 10, 10, 5,
     0, 0, 0, 0, 0, 0, 0, 0},
    {-50, -40, -30, -30, -30, -30, -40, -50,
     -40, -20, 0, 0, 0, 0, -20, -40,
     -30, 0, 10, 15, 15, 10, 0, -30,
     -30, 5, 15, 20, 20, 15, 5, -30,
     -30, 0, 15, 20, 20, 15, 0, -30,
     -30, 5, 10, 15, 15, 10, 5, -30,
     -40, -20, 0, 5, 5, 0, -20, -40,
     -50, -40, -30, -30, -30, -30, -40, -50},
    {-20, -10, -10, -10, -10, -10, -10, -20,
     -10, 0, 0, 0, 0, 0, 0, -10,
     -10, 0, 5, 10, 10, 5, 0, -10,
     -10, 5, 5, 10, 10, 5, 5, -10,
     -10, 0, 10, 10, 10, 10, 0, -10,
     -10, 10, 10, 10, 10, 10, 10, -10,
     -10, 5, 0, 0, 0, 0, 5, -10,
     -20, -10, -10, -10, -10, -10, -10, -20},
    {0, 0, 0, 0, 0, 0, 0, 0,
     5, 10, 10, 10, 10, 10, 10, 5,
     -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0, 0, 0, 0, 0, 0, -5,
     0, 0, 0, 5, 5, 0, 0, 0},
    {-20, -10, -10, -5, -5, -10, -10, -20,
     -10, 0, 0, 0, 0, 0, 0, -10,
     -10, 0, 5, 5, 5, 5, 0, -10,
     -5, 0, 5, 5, 5, 5, 0, -5,
     0, 0, 5, 5, 5, 5, 0, -5,
     -10, 5, 5, 5, 5, 5, 0, -10,
     -10, 0, 5, 0, 0, 0, 0, -10,
     -20, -10, -10, -5, -5, -10, -10, -20},
    {0},
};
static const i8 KING_MG[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    20, 20, 0, 0, 0, 0, 20, 20,
    20, 30, 10, 0, 0, 10, 30, 20};
static const i8 KING_EG[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10, 0, 0, -10, -20, -30,
    -30, -10, 20, 30, 30, 20, -10, -30,
    -30, -10, 30, 40, 40, 30, -10, -30,
    -30, -10, 30, 40, 40, 30, -10, -30,
    -30, -10, 20, 30, 30, 20, -10, -30,
    -30, -30, 0, 0, 0, 0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50};
static const int PASSED[8] = {0, 0, 10, 15, 25, 45, 75, 0};

/* score from the side to move's point of view */
static int eval(const Pos *p) {
    int score = 0, npm = 0;
    int wcnt[8] = {0}, bcnt[8] = {0};
    int bmax[8], wmin[8];
    int wb = 0, bb = 0;
    for (int i = 0; i < 8; i++) { bmax[i] = -1; wmin[i] = 8; }
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->b[sq];
        if (!pc) continue;
        int r = sq >> 3, f = sq & 7;
        int t = ABS(pc);
        if (pc > 0) {
            if (t == KING) continue;
            score += VAL[t] + PST[t][(7 - r) * 8 + f];
            if (t == PAWN) { wcnt[f]++; if (r < wmin[f]) wmin[f] = r; }
            else { npm += VAL[t]; if (t == BISHOP) wb++; }
        } else {
            if (t == KING) continue;
            score -= VAL[t] + PST[t][r * 8 + f];
            if (t == PAWN) { bcnt[f]++; if (r > bmax[f]) bmax[f] = r; }
            else { npm += VAL[t]; if (t == BISHOP) bb++; }
        }
    }
    if (wb >= 2) score += 30;
    if (bb >= 2) score -= 30;
    for (int sq = 0; sq < 64; sq++) {
        int pc = p->b[sq];
        if (!pc) continue;
        int r = sq >> 3, f = sq & 7;
        if (pc == PAWN) {
            int passed = 1;
            for (int g = f - 1; g <= f + 1; g++)
                if ((unsigned)g < 8 && bmax[g] > r) passed = 0;
            if (passed) score += PASSED[r];
            if (wcnt[f] > 1) score -= 8;
        } else if (pc == -PAWN) {
            int passed = 1;
            for (int g = f - 1; g <= f + 1; g++)
                if ((unsigned)g < 8 && wmin[g] < r) passed = 0;
            if (passed) score -= PASSED[7 - r];
            if (bcnt[f] > 1) score += 8;
        } else if (pc == ROOK) {
            if (!wcnt[f]) score += bcnt[f] ? 8 : 18;
        } else if (pc == -ROOK) {
            if (!bcnt[f]) score -= wcnt[f] ? 8 : 18;
        }
    }
    int phase = npm > 4000 ? 4000 : npm;
    int wk = p->ksq[0], bk = p->ksq[1];
    int wmg = KING_MG[(7 - (wk >> 3)) * 8 + (wk & 7)], weg = KING_EG[(7 - (wk >> 3)) * 8 + (wk & 7)];
    int bmg = KING_MG[(bk >> 3) * 8 + (bk & 7)], beg = KING_EG[(bk >> 3) * 8 + (bk & 7)];
    score += ((wmg - bmg) * phase + (weg - beg) * (4000 - phase)) / 4000;
    return (p->side == WHITE ? score : -score) + 8;
}

typedef struct {
    u32 nodes, max_nodes, t0, tlim;
    TimeFn now;
    int stop;
    Move killer[64][2];
    int hist[2][64][64];
    u32 rng;
    int last_score;
} Srch;
static Srch S;

void ai_seed(u32 s) { S.rng = s ? s : 0x1234567u; }

static u32 rnd(void) {
    u32 x = S.rng ? S.rng : 0x1234567u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    S.rng = x;
    return x;
}

int ai_last_score(void) { return S.last_score; }

static void check_limits(void) {
    if (S.nodes >= S.max_nodes) S.stop = 1;
    else if (S.now && S.tlim && (u32)(S.now() - S.t0) > S.tlim) S.stop = 1;
}

static int order_score(const Pos *p, Move m, int ply, Move first) {
    if (m == first) return 1000000;
    int fl = MV_FLAGS(m), from = MV_FROM(m), to = MV_TO(m);
    int s = 0;
    if (fl & F_CAP) {
        int victim = (fl & F_EP) ? PAWN : ABS(p->b[to]);
        s = 100000 + VAL[victim] * 10 - VAL[ABS(p->b[from])] / 10;
    } else if (MV_PROMO(m)) {
        s = 90000 + VAL[MV_PROMO(m)];
    } else if (ply < 64 && m == S.killer[ply][0]) s = 80000;
    else if (ply < 64 && m == S.killer[ply][1]) s = 70000;
    else s = S.hist[p->side][from][to];
    if (MV_PROMO(m) && (fl & F_CAP)) s += 5000;
    return s;
}

static int qsearch(const Pos *p, int alpha, int beta, int ply) {
    if ((++S.nodes & 1023) == 0) check_limits();
    if (S.stop) return 0;
    int stand = eval(p);
    if (ply >= 60) return stand;
    if (stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    Move mv[MAX_MOVES];
    int sc[MAX_MOVES];
    int n = gen_moves(p, mv, 1);
    for (int i = 0; i < n; i++) sc[i] = order_score(p, mv[i], 99, 0);
    Pos q;
    for (int i = 0; i < n; i++) {
        int bi = i;
        for (int j = i + 1; j < n; j++) if (sc[j] > sc[bi]) bi = j;
        if (bi != i) { int ts = sc[i]; sc[i] = sc[bi]; sc[bi] = ts; Move tm = mv[i]; mv[i] = mv[bi]; mv[bi] = tm; }
        /* delta pruning: skip captures that cannot raise alpha even with a big margin */
        if (!MV_PROMO(mv[i])) {
            int victim = (MV_FLAGS(mv[i]) & F_EP) ? PAWN : ABS(p->b[MV_TO(mv[i])]);
            if (stand + VAL[victim] + 200 < alpha) continue;
        }
        if (!make_move(p, mv[i], &q)) continue;
        int s = -qsearch(&q, -beta, -alpha, ply + 1);
        if (S.stop) return 0;
        if (s >= beta) return s;
        if (s > alpha) alpha = s;
    }
    return alpha;
}

static int has_pieces(const Pos *p, int side) {
    int sgn = side ? -1 : 1;
    for (int i = 0; i < 64; i++) {
        int pc = p->b[i] * sgn;
        if (pc >= KNIGHT && pc <= QUEEN) return 1;
    }
    return 0;
}

static int negamax(const Pos *p, int depth, int alpha, int beta, int ply, int allow_null) {
    if ((++S.nodes & 1023) == 0) check_limits();
    if (S.stop) return 0;
    int check = in_check(p, p->side);
    if (check && ply < 60) depth++;
    if (depth <= 0) return qsearch(p, alpha, beta, ply);
    if (ply >= 62) return eval(p);
    if (p->half >= 100) return 0;

    if (allow_null && !check && depth >= 3 && has_pieces(p, p->side)) {
        Pos np = *p;
        np.side ^= 1;
        np.ep = -1;
        int s = -negamax(&np, depth - 3, -beta, -beta + 1, ply + 1, 0);
        if (S.stop) return 0;
        if (s >= beta) return s;
    }

    Move mv[MAX_MOVES];
    int sc[MAX_MOVES];
    int n = gen_moves(p, mv, 0);
    for (int i = 0; i < n; i++) sc[i] = order_score(p, mv[i], ply, 0);
    int legal = 0, best = -INF;
    Pos q;
    for (int i = 0; i < n; i++) {
        int bi = i;
        for (int j = i + 1; j < n; j++) if (sc[j] > sc[bi]) bi = j;
        if (bi != i) { int ts = sc[i]; sc[i] = sc[bi]; sc[bi] = ts; Move tm = mv[i]; mv[i] = mv[bi]; mv[bi] = tm; }
        Move m = mv[i];
        if (!make_move(p, m, &q)) continue;
        legal++;
        int s;
        int red = 0;
        if (depth >= 3 && legal > 4 && !check && !(MV_FLAGS(m) & F_CAP) && !MV_PROMO(m)) red = 1;
        s = -negamax(&q, depth - 1 - red, -beta, -alpha, ply + 1, 1);
        if (red && s > alpha && !S.stop) s = -negamax(&q, depth - 1, -beta, -alpha, ply + 1, 1);
        if (S.stop) return 0;
        if (s > best) best = s;
        if (s > alpha) {
            alpha = s;
            if (alpha >= beta) {
                if (!(MV_FLAGS(m) & F_CAP) && ply < 64) {
                    if (S.killer[ply][0] != m) { S.killer[ply][1] = S.killer[ply][0]; S.killer[ply][0] = m; }
                    int *h = &S.hist[p->side][MV_FROM(m)][MV_TO(m)];
                    *h += depth * depth;
                    if (*h > 60000) *h = 60000;
                }
                break;
            }
        }
    }
    if (legal == 0) return check ? -MATE + ply : 0;
    return best;
}

/* root data is static: the root frame stays small (no stack-probe helper without a CRT) */
static Move r_mv[MAX_MOVES];
static Pos r_child[MAX_MOVES];
static int r_score[MAX_MOVES];
static int r_rep[MAX_MOVES];

Move ai_pick(const Pos *p, int level, const u64 *hist, int nhist, TimeFn now) {
    int n = legal_moves(p, r_mv);
    if (n == 0) return 0;
    if (n == 1) return r_mv[0];
    int maxdepth, jitter = 0;
    u32 maxnodes, tlim = 0;
    switch (level) {
        case 1: maxdepth = 1; maxnodes = 20000; jitter = 140; break;
        case 2: maxdepth = 2; maxnodes = 60000; jitter = 45; break;
        case 3: maxdepth = 5; maxnodes = 250000; tlim = 2500; break;
        default: maxdepth = 9; maxnodes = 900000; tlim = 4000; break;
    }
    S.nodes = 0;
    S.max_nodes = maxnodes;
    S.stop = 0;
    S.now = now;
    S.t0 = now ? now() : 0;
    S.tlim = tlim;
    for (int i = 0; i < 64; i++) S.killer[i][0] = S.killer[i][1] = 0;
    for (int c = 0; c < 2; c++)
        for (int a = 0; a < 64; a++)
            for (int b = 0; b < 64; b++) S.hist[c][a][b] = 0;

    for (int i = 0; i < n; i++) {
        make_move(p, r_mv[i], &r_child[i]);
        u64 h = pos_hash(&r_child[i]);
        int c = 0;
        for (int k = 0; k < nhist; k++) if (hist[k] == h) c++;
        r_rep[i] = c;
    }
    /* initial order: captures / promotions first */
    for (int i = 0; i < n; i++) {
        r_score[i] = order_score(p, r_mv[i], 0, 0);
        if (r_score[i] < 50000) r_score[i] = (int)(rnd() % 4096);   /* random order among quiet moves = variety */
    }
    for (int i = 0; i < n; i++) {
        int bi = i;
        for (int j = i + 1; j < n; j++) if (r_score[j] > r_score[bi]) bi = j;
        if (bi != i) {
            int ts = r_score[i]; r_score[i] = r_score[bi]; r_score[bi] = ts;
            Move tm = r_mv[i]; r_mv[i] = r_mv[bi]; r_mv[bi] = tm;
            Pos tp = r_child[i]; r_child[i] = r_child[bi]; r_child[bi] = tp;
            int tr = r_rep[i]; r_rep[i] = r_rep[bi]; r_rep[bi] = tr;
        }
    }

    Move best = r_mv[0];
    int best_score = -INF;
    static int scores[MAX_MOVES];
    int have_scores = 0;
    for (int depth = 1; depth <= maxdepth; depth++) {
        int alpha = -INF, beta = INF;
        int it_best = 0, it_score = -INF;
        int complete = 1;
        for (int i = 0; i < n; i++) {
            int s;
            if (r_rep[i] >= 2) s = 0;
            else if (jitter) s = -negamax(&r_child[i], depth - 1, -INF, INF, 1, 1);
            else s = -negamax(&r_child[i], depth - 1, -beta, -alpha, 1, 1);
            if (S.stop) { complete = 0; break; }
            scores[i] = s;
            if (s > it_score) { it_score = s; it_best = i; }
            if (s > alpha) alpha = s;
        }
        if (!complete) {
            if (depth == 1) have_scores = 0;
            break;
        }
        have_scores = 1;
        best = r_mv[it_best];
        best_score = it_score;
        if (it_best != 0) {   /* best move first for the next iteration */
            Move tm = r_mv[it_best]; Pos tp = r_child[it_best]; int tr = r_rep[it_best];
            for (int k = it_best; k > 0; k--) { r_mv[k] = r_mv[k - 1]; r_child[k] = r_child[k - 1]; r_rep[k] = r_rep[k - 1]; }
            r_mv[0] = tm; r_child[0] = tp; r_rep[0] = tr;
            for (int k = it_best; k > 0; k--) scores[k] = scores[k - 1];
            scores[0] = it_score;
        }
        if (it_score > MATE - 100 || it_score < -MATE + 100) break;
    }
    S.last_score = best_score;
    if (jitter && have_scores) {
        int bi = 0, bs = -INF * 2;
        for (int i = 0; i < n; i++) {
            int s = scores[i] + (int)(rnd() % (u32)(2 * jitter + 1)) - jitter;
            if (s > bs) { bs = s; bi = i; }
        }
        return r_mv[bi];
    }
    return best;
}
