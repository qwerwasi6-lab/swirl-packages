#ifndef ASSETS_H
#define ASSETS_H
/* Piece masks: 64x64 8-bit coverage, index 0..5 = P N B R Q K. */
extern const unsigned char g_piece_fill[6 * 64 * 64];
extern const unsigned char g_piece_line[6 * 64 * 64];

typedef struct {
    unsigned short off;      /* offset into font data */
    unsigned char w, h;      /* bitmap size */
    signed char xoff, yoff;  /* bitmap top-left relative to pen/baseline */
    unsigned short adv8;     /* advance in 1/8 px */
} Glyph;

typedef struct {
    const Glyph *g;          /* 95 glyphs, ASCII 32..126 */
    const unsigned char *data;
    int ascent;
} FontInfo;

extern const FontInfo g_fonts[3];   /* 0 small, 1 medium (semibold), 2 large (semibold) */
#endif
