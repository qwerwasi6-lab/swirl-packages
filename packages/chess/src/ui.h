#ifndef SWIRLCHESS_UI_H
#define SWIRLCHESS_UI_H
#include "engine.h"

#define UI_W 840
#define UI_H 600

extern u32 ui_fb[UI_W * UI_H];          /* 0x00RRGGBB */

/* input diagnostics shown in the panel (toggle with D):
 * 0 mouse-move msgs, 1 button-downs, 2 button-ups, 3 key-downs,
 * 4,5 raw x,y   6,7 mapped x,y   8,9 client w,h   10 frames drawn */
extern int ui_dbg[12];

void ui_init(TimeFn now, u32 seed);
void ui_render(void);
void ui_mouse(int type, int x, int y);  /* 0 = move, 1 = left down, 2 = left up */
void ui_key(int vk);
int ui_step(void);                      /* 1 = screen changed; runs the computer's move when due */

/* test hooks */
void ui_test_load(const char *fen, int flipped_view, int sel_sq);
#endif
