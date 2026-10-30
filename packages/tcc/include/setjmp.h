/* setjmp.h — freestanding setjmp/longjmp for the SwirlOS port (used by
 * freetype's error recovery). x86_64 callee-saved registers + rsp + rip. */
#ifndef _SETJMP_H_SWIRL
#define _SETJMP_H_SWIRL

typedef long jmp_buf_swirl[8];
typedef jmp_buf_swirl jmp_buf;

extern int setjmp(jmp_buf env) __attribute__((returns_twice));
extern void longjmp(jmp_buf env, int val) __attribute__((noreturn));

/* internal: same buffer, different symbol that setjmp writes */
typedef jmp_buf_swirl swirl_jmp_buf;

#endif
