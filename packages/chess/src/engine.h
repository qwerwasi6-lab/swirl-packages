#ifndef ENGINE_H
#define ENGINE_H
/* Swirl Chess engine: rules, SAN, game state and a small alpha-beta AI.
 * Freestanding: no libc needed (the Windows build has no CRT). */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;

enum { EMPTY = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };
enum { WHITE = 0, BLACK = 1 };

/* Square = rank*8 + file, a1 = 0, h1 = 7, a8 = 56. board[sq] > 0 white, < 0 black. */
typedef struct {
    i8 b[64];
    u8 side;        /* side to move */
    u8 castle;      /* 1 = white O-O, 2 = white O-O-O, 4 = black O-O, 8 = black O-O-O */
    i8 ep;          /* en-passant target square or -1 */
    u8 ksq[2];      /* king squares */
    u8 half;        /* half-move clock (50-move rule) */
    u16 full;       /* full move number */
} Pos;

/* Move: from(6) | to(6)<<6 | promo(3)<<12 | flags<<15 */
typedef u32 Move;
#define MV_FROM(m) ((int)((m) & 63))
#define MV_TO(m) ((int)(((m) >> 6) & 63))
#define MV_PROMO(m) ((int)(((m) >> 12) & 7))
#define MV_FLAGS(m) ((int)(((m) >> 15) & 15))
#define MV_MAKE(f, t, p, fl) ((Move)((f) | ((t) << 6) | ((p) << 12) | ((fl) << 15)))
enum { F_CAP = 1, F_EP = 2, F_CASTLE = 4, F_DBL = 8 };

#define MAX_MOVES 256

void pos_start(Pos *p);
int pos_from_fen(Pos *p, const char *fen);       /* returns 1 on success */
u64 pos_hash(const Pos *p);
int attacked(const Pos *p, int sq, int by_side);
int in_check(const Pos *p, int side);
int gen_moves(const Pos *p, Move *out, int caps_only);   /* pseudo-legal */
int make_move(const Pos *p, Move m, Pos *out);           /* 1 if legal */
int legal_moves(const Pos *p, Move *out);
u64 perft(const Pos *p, int depth);
void move_to_san(const Pos *p, Move m, char *out);       /* out >= 12 bytes */
void move_to_uci(Move m, char *out);                     /* out >= 6 bytes */

/* ---- game ---- */
#define MAX_PLY 1024
enum {
    RES_NONE = 0, RES_WHITE_WINS, RES_BLACK_WINS,
    RES_STALEMATE, RES_FIFTY, RES_MATERIAL, RES_REPETITION
};

typedef struct {
    Pos pos[MAX_PLY + 1];
    u64 hash[MAX_PLY + 1];
    Move mv[MAX_PLY];
    char san[MAX_PLY][12];
    int n;          /* plies played; current position is pos[n] */
    int result;
} Game;

void game_new(Game *g);
int game_play(Game *g, Move m);          /* m must be legal; 1 on success */
void game_undo(Game *g, int plies);
Move game_find(const Game *g, int from, int to, int promo);  /* 0 if illegal */
int game_repetitions(const Game *g);     /* how often the current position occurred */

/* ---- AI ---- */
typedef u32 (*TimeFn)(void);
/* level 1..4. hist/nhist: position hashes so far (to avoid walking into threefold). */
Move ai_pick(const Pos *p, int level, const u64 *hist, int nhist, TimeFn now);
void ai_seed(u32 s);
int ai_last_score(void);

#endif
