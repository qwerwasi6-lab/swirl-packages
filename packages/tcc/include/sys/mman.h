/* SwirlOS: no mmap; user memory is already RWX. mprotect is a no-op. */
#ifndef SWIRL_TCC_MMAN_H
#define SWIRL_TCC_MMAN_H
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
static inline int mprotect(void *p, unsigned long n, int prot) { (void)p; (void)n; (void)prot; return 0; }
#endif
