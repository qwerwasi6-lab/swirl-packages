#include "selftest.h"
#include "assets.h"

/* Startup self-test of the chess engine and baked-in assets.
 * Every stage returns 0 when it passes, otherwise its error code (see ERRORS.txt). */

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static Move find_sq(const Pos *p, int from, int to, int promo) {
    Move l[MAX_MOVES];
    int n = legal_moves(p, l);
    for (int i = 0; i < n; i++)
        if (MV_FROM(l[i]) == from && MV_TO(l[i]) == to && MV_PROMO(l[i]) == promo) return l[i];
    return 0;
}

#define SQ(f, r) (((r) - 1) * 8 + ((f) - 'a'))

static Game g_test;   /* static: keeps the stack small */

int selftest_run(int stage, TimeFn now) {
    Pos p;
    char s[16];
    switch (stage) {
        case 1: {                                   /* move generator basics */
            Move l[MAX_MOVES];
            pos_start(&p);
            return legal_moves(&p, l) == 20 ? 0 : 101;
        }
        case 2:
            pos_start(&p);
            return perft(&p, 2) == 400 ? 0 : 102;
        case 3:
            pos_start(&p);
            return perft(&p, 3) == 8902 ? 0 : 103;
        case 4:                                     /* FEN parser */
            return pos_from_fen(&p, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1") ? 0 : 104;
        case 5:                                     /* castling, pins, captures */
            pos_from_fen(&p, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
            return perft(&p, 2) == 2039 ? 0 : 105;
        case 6:                                     /* en passant, discovered checks */
            pos_from_fen(&p, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
            return perft(&p, 3) == 2812 ? 0 : 106;
        case 7: {                                   /* move notation */
            Move m;
            pos_start(&p);
            m = find_sq(&p, SQ('e', 2), SQ('e', 4), 0);
            if (!m) return 201;
            move_to_san(&p, m, s);
            if (!streq(s, "e4")) return 201;
            m = find_sq(&p, SQ('g', 1), SQ('f', 3), 0);
            if (!m) return 201;
            move_to_san(&p, m, s);
            return streq(s, "Nf3") ? 0 : 201;
        }
        case 8: {                                   /* game flow + checkmate detection */
            static const char mv[7][2][3] = {{"e2", "e4"}, {"e7", "e5"}, {"d1", "h5"}, {"b8", "c6"},
                                             {"f1", "c4"}, {"g8", "f6"}, {"h5", "f7"}};
            game_new(&g_test);
            for (int i = 0; i < 7; i++) {
                int f = (mv[i][0][1] - '1') * 8 + (mv[i][0][0] - 'a');
                int t = (mv[i][1][1] - '1') * 8 + (mv[i][1][0] - 'a');
                Move m = game_find(&g_test, f, t, 0);
                if (!m || !game_play(&g_test, m)) return 202;
            }
            if (g_test.result != RES_WHITE_WINS) return 202;
            return streq(g_test.san[6], "Qxf7#") ? 0 : 202;
        }
        case 9: {                                   /* computer opponent returns a legal move */
            Move l[MAX_MOVES], m;
            pos_start(&p);
            m = ai_pick(&p, 1, 0, 0, now);
            if (!m) return 301;
            int n = legal_moves(&p, l);
            for (int i = 0; i < n; i++)
                if (l[i] == m) return 0;
            return 301;
        }
        case 10: {                                  /* computer opponent finds a mate in one */
            Move m;
            pos_from_fen(&p, "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");
            m = ai_pick(&p, 3, 0, 0, now);
            return (MV_FROM(m) == SQ('a', 1) && MV_TO(m) == SQ('a', 8)) ? 0 : 302;
        }
        case 11: {                                  /* position hashing (repetition detection) */
            Pos q;
            Move m;
            pos_start(&p);
            m = find_sq(&p, SQ('e', 2), SQ('e', 4), 0);
            if (!m || !make_move(&p, m, &q)) return 401;
            return (pos_hash(&p) != 0 && pos_hash(&p) != pos_hash(&q)) ? 0 : 401;
        }
        case 12: {                                  /* baked-in pieces and fonts */
            for (int t = 0; t < 6; t++) {
                unsigned sum = 0;
                for (int i = 0; i < 4096; i++) sum += g_piece_fill[t * 4096 + i];
                if (sum == 0) return 501;
            }
            for (int f = 0; f < 3; f++)
                if (g_fonts[f].g['A' - 32].w == 0 || g_fonts[f].g['A' - 32].h == 0) return 502;
            return 0;
        }
    }
    return 0;
}
