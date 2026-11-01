#ifndef SWIRLCHECKERS_UI_H
#define SWIRLCHECKERS_UI_H
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u32 (*TimeFn)(void);

#define UI_W 840
#define UI_H 600

extern u32 ui_fb[UI_W * UI_H];          /* 0x00RRGGBB */

void ui_init(TimeFn now, u32 seed);
void ui_render(void);
void ui_mouse(int type, int x, int y);  /* 0 = move, 1 = left down, 2 = left up */
void ui_key(int vk);
int ui_step(void);                      /* 1 = screen changed; runs the computer's move when due */
#endif
