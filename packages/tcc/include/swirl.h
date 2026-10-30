/* swirl.h — the SwirlOS userspace runtime (libswirl) */
#ifndef SWIRL_H
#define SWIRL_H

#include <stdint.h>
#include <stddef.h>

/* ---- syscall ABI (int 0x80): rax=num, rdi/rsi/rdx=args ---- */
static inline int64_t syscall3(int64_t n, int64_t a, int64_t b, int64_t c) {
    int64_t r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "memory", "rcx", "r11");
    return r;
}

#define SYS_OPEN       0
#define SYS_READ       1
#define SYS_WRITE      2
#define SYS_CLOSE      3
#define SYS_SEEK       4
#define SYS_DIR_OPEN   5
#define SYS_DIR_READ   6
#define SYS_SPAWN      7
#define SYS_WAIT       8
#define SYS_EXIT       9
#define SYS_GETPID    10
#define SYS_PPID      11
#define SYS_SYSINFO   12
#define SYS_FB_INFO   13
#define SYS_FB_RECT   14
#define SYS_FB_TEXT   15
#define SYS_KEY_READ  16
#define SYS_MKDIR     17
#define SYS_UNLINK    18
#define SYS_PROC_LIST 19
#define SYS_GETRING   20
#define SYS_WM_CREATE 21
#define SYS_WM_DESTROY 22
#define SYS_WM_PRESENT 23
#define SYS_WM_POLL   24
#define SYS_WM_SET_TITLE 25
#define SYS_WM_SET_ICON 26
#define SYS_WM_BOUNDS 27
#define SYS_KILL      28
#define SYS_RENAME    29
#define SYS_YIELD     30
#define SYS_RTC       31
#define SYS_UNLINK_REC 32
#define SYS_WM_TEXT   33
#define SYS_FONT_MET  34
/* --- ring-3 desktop (SwirlWM) extensions --- */
#define SYS_RTC6          35   /* out: {sec,min,hour,day,month,year}      */
#define SYS_MOUSE_POLL    36   /* out: {mx,my,buttons,cs}; ret 1=changed  */
#define SYS_KEYFLUSH      37   /* drop queued key input                   */
#define SYS_FB_SETMODE    38   /* VBE mode switch (kernel fb_setmode)     */
#define SYS_POWER         39   /* rdi=1 reboot, rdi=0 power off           */
#define SYS_FB_MAP        40   /* map fb rw at 0x16000000; become gui root*/
#define SYS_FB_TEXT_TARGET 41  /* render fb_text into {pix,stride}        */
#define SYS_CONSOLE_SILENT 42  /* console logs to serial only             */
#define SYS_WM_QUERY      43   /* list kernel windows not owned by caller */
#define SYS_WM_MAPCANVAS  44   /* map a window canvas into this process   */
#define SYS_WM_SETKFOCUS  45   /* kernel-side keyboard focus (wid<=0 blur)*/
#define SYS_WAIT_NB       46   /* reap a zombie child if any, never block */
#define SYS_CLIP_SET      47   /* kernel clipboard write (ctrl+c)         */
#define SYS_CLIP_GET      48   /* kernel clipboard read  (ctrl+v)         */
#define SYS_SLEEP         49   /* REAL sleep: park until timer (ms)       */
#define SYS_SPAWN_A       50   /* spawn with argument string (argv[1])    */
#define SYS_GETARGS       51   /* copy this process's spawn argument      */
#define SYS_WM_SETGEOM    52   /* desktop-driven window move/resize       */
#define SYS_WM_MOUSE      53   /* desktop forwards content mouse event    */
/* --- daily-usability extensions (0.2) --- */
#define SYS_WM_KEYGRAB    88   /* WM only: register key combos the kernel parks for the WM */
#define SYS_SYSKEY_POLL   57   /* next SHELL hotkey {down,code,ascii,mods}*/
#define SYS_NOTIFY_SEND   58   /* post a notification (title, body, kind) */
#define SYS_NOTIF_DRAIN   59   /* desktop: drain pending notifications    */
#define SYS_BEEP          60   /* PC speaker: rdi=freq Hz, rsi=ms         */
#define SYS_SETVOL        61   /* master beep volume 0..100 (0 = mute)    */

#define O_READ    1
#define O_WRITE   2
#define O_CREATE  4
#define O_TRUNC   8

typedef struct {
    uint32_t uptime_cs;
    uint32_t mem_total, mem_used;
    uint32_t nprocs;
    uint32_t font_size;
    char     hostname[24];
    uint32_t cpu_pct;        /* busy % over the last second */
} swirl_sysinfo_t;

typedef struct { int32_t s, mi, h, d, mo, y; } swirl_rtc_t;
typedef struct { int32_t mx, my, buttons, cs; } swirl_mouse_t;

static inline int  s_open(const char *p, int fl)        { return (int)syscall3(SYS_OPEN, (int64_t)p, fl, 0); }
static inline int  s_read(int fd, void *b, int n)       { return (int)syscall3(SYS_READ, fd, (int64_t)b, n); }
static inline int  s_write(int fd, const void *b, int n){ return (int)syscall3(SYS_WRITE, fd, (int64_t)b, n); }
static inline int  s_close(int fd)                      { return (int)syscall3(SYS_CLOSE, fd, 0, 0); }
static inline int  s_seek(int fd, int off, int w)       { return (int)syscall3(SYS_SEEK, fd, off, w); }
static inline int  s_dir_open(const char *p)            { return (int)syscall3(SYS_DIR_OPEN, (int64_t)p, 0, 0); }
static inline int  s_dir_read(int h, void *buf)         { return (int)syscall3(SYS_DIR_READ, h, (int64_t)buf, 0); }
static inline int  s_spawn(const char *p)               { return (int)syscall3(SYS_SPAWN, (int64_t)p, 0, 0); }
#define SYS_LIB_LOAD 76
#define SYS_KBD_MODS 78
static inline int s_kbd_mods(void) { return (int)syscall3(SYS_KBD_MODS, 0, 0, 0); }
static inline long s_lib_load(const char *path) { return (long)syscall3(SYS_LIB_LOAD, (int64_t)path, 0, 0); }
/* generic TLS facility (r18): anchor the calling task's ring-3 GS base at
 * `base`. Language runtimes (and the SwirlRun PE loader's TEB) use this
 * the way Linux programs use arch_prctl(ARCH_SET_GS). */
/* theme engine (r35): swap the UI font (compositor only) and read the
 * fixed-pitch cell advance of every pixel size 0..64 */
#define SYS_FONT_SET   81
#define SYS_FONT_CELLS 82
#define SYS_FONT_ADV   87   /* r43: advance of every byte at (size | 0x100 UI | 0x200 bold) */
static inline int s_font_adv(int sizeflags, uint8_t out256[256]) { return (int)syscall3(SYS_FONT_ADV, sizeflags, (int64_t)out256, 0); }
static inline int s_font_set(const char *path)   { return (int)syscall3(SYS_FONT_SET, (int64_t)path, 0, 0); }
static inline void s_font_cells(uint8_t out65[65]) { syscall3(SYS_FONT_CELLS, (int64_t)out65, 0, 0); }
#define SYS_TLS_SET 79
static inline int s_set_tls_base(uint64_t base) {
    return (int)syscall3(SYS_TLS_SET, (int64_t)base, 0, 0);
}
static inline int  s_spawn_a(const char *p, const char *arg) { return (int)syscall3(SYS_SPAWN_A, (int64_t)p, (int64_t)arg, 0); }
static inline int  s_getargs(char *buf, int max)        { return (int)syscall3(SYS_GETARGS, (int64_t)buf, max, 0); }
static inline int  s_wait(int pid)                      { return (int)syscall3(SYS_WAIT, pid, 0, 0); }
static inline void s_exit(int c)                        { syscall3(SYS_EXIT, c, 0, 0); __builtin_unreachable(); }
static inline int  s_getpid(void)                       { return (int)syscall3(SYS_GETPID, 0, 0, 0); }
static inline int  s_ppid(void)                         { return (int)syscall3(SYS_PPID, 0, 0, 0); }
static inline void s_sysinfo(swirl_sysinfo_t *si)       { syscall3(SYS_SYSINFO, (int64_t)si, 0, 0); }
static inline int  s_ring(void)                         { return (int)syscall3(SYS_GETRING, 0, 0, 0); }
static inline int  s_mkdir(const char *p)               { return (int)syscall3(SYS_MKDIR, (int64_t)p, 0, 0); }
static inline int  s_unlink(const char *p)              { return (int)syscall3(SYS_UNLINK, (int64_t)p, 0, 0); }
static inline int  s_key(void)                          { return (int)syscall3(SYS_KEY_READ, 0, 0, 0); }

/* ---- window manager ---- */
typedef struct {
    uint32_t type;           /* WM_EVT_KEY / WM_EVT_MOUSE */
    uint32_t a, b, c;
    uint32_t sid;            /* r56: kernel surface (window) the event came from */
} wm_event_t;

/* event types delivered to windows (mirror of kernel/surface.h) */
#define WM_EVT_KEY     1
#define WM_EVT_MOUSE   2
#define WM_EVT_FOCUS   3     /* a = 1 focused, 0 blurred                */
#define WM_EVT_RESIZE  4     /* a = new w, b = new h (logical)          */
#define WM_EVT_CLOSE   5
#define WM_EVT_REFRESH 6     /* desktop ping: re-list the directory      */

typedef struct {
    const char *title;
    int w, h;
    uint32_t **out_canvas;
} wm_create_t;

static inline int  s_wm_create(const char *title, int w, int h, uint32_t **canvas) {
    wm_create_t req = { title, w, h, canvas };
    return (int)syscall3(SYS_WM_CREATE, (int64_t)&req, 0, 0);
}
static inline void s_wm_destroy(int wid)                { syscall3(SYS_WM_DESTROY, wid, 0, 0); }
static inline void s_wm_present(int wid)                { syscall3(SYS_WM_PRESENT, wid, 0, 0); }
static inline int  s_wm_poll(wm_event_t *ev)            { return (int)syscall3(SYS_WM_POLL, (int64_t)ev, 0, 0); }
static inline int  s_wm_poll_nb(wm_event_t *ev)         { return (int)syscall3(SYS_WM_POLL, (int64_t)ev, 1, 0); }
static inline void s_wm_set_title(int wid, const char *t){ syscall3(SYS_WM_SET_TITLE, wid, (int64_t)t, 0); }
static inline int  s_wm_set_icon(int wid, const uint32_t *rgba, int w, int h) {
    return (int)syscall3(SYS_WM_SET_ICON, wid, (int64_t)rgba, (int64_t)(((int64_t)h << 16) | (w & 0xFFFF)));
}
static inline int  s_wm_bounds(int wid, int out[4])     { return (int)syscall3(SYS_WM_BOUNDS, wid, (int64_t)out, 0); }
#define SYS_WM_CLOSE      94   /* r56: WM-side: ask surface owner to shut down */
static inline int  s_wm_close(int wid)                 { return (int)syscall3(SYS_WM_CLOSE, wid, 0, 0); }
static inline int  s_kill(int pid)                      { return (int)syscall3(SYS_KILL, pid, 0, 0); }
static inline int  s_rename(const char *o, const char *n){ return (int)syscall3(SYS_RENAME, (int64_t)o, (int64_t)n, 0); }
static inline void s_yield(void)                        { syscall3(SYS_YIELD, 0, 0, 0); }
static inline int  s_rtc(void)                          { return (int)syscall3(SYS_RTC, 0, 0, 0); }
static inline int  s_unlink_rec(const char *p)          { return (int)syscall3(SYS_UNLINK_REC, (int64_t)p, 0, 0); }
static inline void s_wm_text(int wid, int x, int y, int size, uint32_t color, const char *s) {
    struct { int32_t wid, x, y, size; uint32_t color; const char *str;
             uint32_t *pix; } req = { wid, x, y, size, color, s, 0 };
    syscall3(SYS_WM_TEXT, (int64_t)&req, 0, 0);
}
/* client-mode variant: render text into a SPECIFIC draw buffer (multi-
 * buffer clients draw into a canvas the compositor cannot be reading) */
static inline void s_wm_text_buf(int wid, uint32_t *pix, int x, int y, int size, uint32_t color, const char *s) {
    struct { int32_t wid, x, y, size; uint32_t color; const char *str;
             uint32_t *pix; } req = { wid, x, y, size, color, s, pix };
    syscall3(SYS_WM_TEXT, (int64_t)&req, 0, 0);
}
#define SYS_WM_TEXT2 92
#define SYS_SET_EXC_STUB 93   /* r56: 32-bit SEH fault trampoline (rdi=guest addr) */
/* r56: render TTF text into an arbitrary caller buffer (Win32 memory DCs) */
static inline int s_wm_text_any(uint32_t *pix, int cw, int ch, int x, int y, int size, uint32_t color, const char *s) {
    struct { uint32_t *pix; int32_t cw, ch, x, y, size; uint32_t color; const char *str; }
        req = { pix, cw, ch, x, y, size, color, s };
    return (int)syscall3(SYS_WM_TEXT2, (int64_t)&req, 0, 0);
}
static inline void s_font_metrics(int size, int out[3]) { syscall3(SYS_FONT_MET, size, (int64_t)out, 0); }
static inline uint32_t s_ticks_ms(void) {
    swirl_sysinfo_t si;
    s_sysinfo(&si);
    return si.uptime_cs * 10;
}
static inline void s_fb_rect(int x, int y, int w, int h, uint32_t color) {
    __asm__ volatile("int $0x80"
                     :: "a"(SYS_FB_RECT), "D"(x), "S"(y), "d"((h << 16) | (w & 0xFFFF)),
                        "c"((int64_t)color)
                     : "memory", "r11");
}
static inline void s_fb_text(int x, int y, const char *s, int size, uint32_t color) {
    register int64_t r8v __asm__("r8") = (int64_t)color;
    __asm__ volatile("int $0x80"
                     :: "a"(SYS_FB_TEXT), "D"(x), "S"(y), "d"((int64_t)s), "c"((int64_t)size),
                        "r"(r8v)
                     : "memory", "r11");
}

/* ---- ring-3 desktop (SwirlWM) wrappers ---- */
static inline int  s_rtc6(swirl_rtc_t *rt)          { return (int)syscall3(SYS_RTC6, (int64_t)rt, 0, 0); }
static inline int  s_mouse_poll(swirl_mouse_t *m)   { return (int)syscall3(SYS_MOUSE_POLL, (int64_t)m, 0, 0); }
static inline int  s_key_poll(void)                 { return (int)syscall3(SYS_KEY_READ, 0, 1, 0); } /* non-blocking */
static inline void s_keyflush(void)                 { syscall3(SYS_KEYFLUSH, 0, 0, 0); }
/* returns only when power off failed (-1): reboot never returns */
static inline int  s_power(int reboot)              { return (int)syscall3(SYS_POWER, reboot, 0, 0); }
/* ---- driver roster (kernel/drvreg.h): every non-NIC driver, live ---- */
#define SYS_DRV_LIST      85
#define SYS_FB_BACKBUF    86   /* zeroed ring-3 buffer (bytes) for big back buffers */
enum { DRVC_SYSTEM = 0, DRVC_STORAGE, DRVC_USB, DRVC_INPUT, DRVC_DISPLAY,
       DRVC_NETWORK, DRVC_SOUND, DRVC_PORTS, DRVC_OTHER, DRVC_COUNT };
#define DRVS_NO_DEVICE 0    /* driver loaded, hardware absent           */
#define DRVS_STANDBY   1    /* driver loaded, hardware present but idle */
#define DRVS_FAILED    2
#define DRVS_ACTIVE    3
#define DRVS_NO_DRIVER 4    /* hardware present, no SwirlOS driver      */
typedef struct {
    char    name[48];
    char    summary[72];
    char    info[320];      /* "Key: value\n" lines                      */
    uint8_t cls, state, id, pad;
} drv_rec_t;                /* 448 bytes - mirrors kernel/drvreg.h      */
static inline int s_drv_list(drv_rec_t *buf, int max) {
    return (int)syscall3(SYS_DRV_LIST, (int64_t)buf, max, 0);
}
/* ---- Setup / installer (r62) ---- */
#define SYS_SETUP_CTL     99
typedef struct {
    uint32_t live, disk_present, disk_blank, disk_ptable;  /* disk_ptable: r67, the disk has partitions (never erased) */
    uint64_t disk_sectors;                 /* 512 B sectors, 0 = unknown     */
    uint32_t vol_total_kb, vol_free_kb, inodes_total, inodes_free;
    char     disk_backend[8], vol_backend[8], disk_model[48];
} swirl_setup_info_t;                      /* mirrors kernel/syscall.c case 99 */
static inline int s_setup_info(swirl_setup_info_t *si)  { return (int)syscall3(SYS_SETUP_CTL, 0, (int64_t)si, 0); }
static inline int s_setup_begin(int force)              { return (int)syscall3(SYS_SETUP_CTL, 1, force, 0); }
static inline int s_setup_step(void)                    { return (int)syscall3(SYS_SETUP_CTL, 2, 0, 0); }
static inline int s_setup_finish(void)                  { return (int)syscall3(SYS_SETUP_CTL, 3, 0, 0); }
static inline int s_setup_handoff(void)                 { return (int)syscall3(SYS_SETUP_CTL, 4, 0, 0); }
static inline int s_kernel_abi(void)                    { return (int)syscall3(SYS_SETUP_CTL, 5, 0, 0); }
#define SYS_ACPI_EVENTS   83
#define SYS_ACPI_INFO     84
#define ACPI_EV_POWER_BUTTON 1
static inline int  s_acpi_events(int clear)         { return (int)syscall3(SYS_ACPI_EVENTS, clear, 0, 0); }
static inline int  s_acpi_info(char *buf, int max)  { return (int)syscall3(SYS_ACPI_INFO, (int64_t)buf, max, 0); }
static inline uint32_t *s_fb_map(void)              { return (uint32_t *)syscall3(SYS_FB_MAP, 0, 0, 0); }
static inline void s_fb_text_target(uint32_t *pix, uint32_t stride) { syscall3(SYS_FB_TEXT_TARGET, (int64_t)pix, stride, 0); }
static inline uint32_t *s_fb_backbuf(uint64_t bytes)  { return (uint32_t *)syscall3(SYS_FB_BACKBUF, (int64_t)bytes, 0, 0); }
static inline void s_console_silent(int on)         { if (on) syscall3(SYS_CONSOLE_SILENT, 1, 0, 0); }

/* ---- external-window proxy (desktop composites kernel windows) ---- */
typedef struct {
    int32_t wid, owner;
    int32_t x, y;                 /* screen position of the window frame   */
    int32_t w, h;                 /* logical client size                   */
    int32_t cw, ch;               /* canvas dims (allocated size)          */
    uint32_t pseq, focused;
    char title[40];
    uint64_t canvas;              /* VISIBLE (front) canvas, identity phys */
    uint64_t canvas0;             /* draw buffer 0 (see kernel/surface.h)       */
} wm_qentry_t;

static inline int  s_wm_query(uint32_t self_pid, wm_qentry_t *buf, int max) {
    return (int)syscall3(SYS_WM_QUERY, self_pid, (int64_t)buf, max);
}
static inline int  s_wm_mapcanvas(int wid, uint64_t vma) {
    return (int)syscall3(SYS_WM_MAPCANVAS, wid, (int64_t)vma, 0);
}
static inline void s_wm_setkfocus(int wid)  { syscall3(SYS_WM_SETKFOCUS, wid, 0, 0); }
static inline int  s_wm_setgeom(int wid, int x, int y, int w, int h) {
    struct { int32_t x, y, w, h; } g = { x, y, w, h };
    return (int)syscall3(SYS_WM_SETGEOM, wid, (int64_t)&g, 0);
}
static inline int  s_wm_mouse(int wid, int rx, int ry, int buttons) {
    struct { int32_t rx, ry; } m = { rx, ry };
    return (int)syscall3(SYS_WM_MOUSE, wid, (int64_t)&m, buttons);
}
static inline int  s_waitnb(int pid)        { return (int)syscall3(SYS_WAIT_NB, pid, 0, 0); }
/* r56: also fetch the reaped child's exit code (0 ptr = don't care) */
static inline int  s_waitnb_code(int pid, int *code) { return (int)syscall3(SYS_WAIT_NB, pid, (int64_t)code, 0); }
/* ---- drag & drop (files <-> desktop) ----
 * The dragging side publishes the source path; the desktop polls it every
 * frame, draws the drop feedback and performs the move on release. */
static inline void s_wm_drag_set(int wid, const char *path) {
    syscall3(62, wid, (int64_t)path, 0);
}
typedef struct { int32_t wid; char path[96]; } swirl_drag_t;
static inline int  s_wm_drag_info(swirl_drag_t *d) {
    return (int)syscall3(63, (int64_t)d, 0, 0);
}
/* ask the kernel to push WM_EVT_REFRESH into a window's event queue */
static inline void s_wm_ping(int wid) { syscall3(64, wid, 0, 0); }
static inline int  s_clip_set(const void *buf, int n) { return (int)syscall3(SYS_CLIP_SET, (int64_t)buf, n, 0); }
static inline int  s_clip_get(void *buf, int max)     { return (int)syscall3(SYS_CLIP_GET, (int64_t)buf, max, 0); }
/* fixed VA window where the desktop maps external canvases: the private
 * ring-3 region the kernel builds under PML4[508] (see vmm.c). Canonical
 * form of slot 508 = 0xFFFF800000000000 + (508-256)<<39 = 0xFFFFFE0000000000.
 * MUST NOT be PML4[509]: that entry is the kernel's supervisor-only window
 * table (ramdisk/AHCI/SRC) and is not ring-3 accessible. The low VA space
 * is fully identity-mapped and off limits. 16 MiB stride, 16 slots - wide
 * enough for ALL draw canvases of one window (3 x 1440x900 x 4 = 15.6 MB). */
#define EXT_CANVAS_VA(wid)  (0xFFFFFE0000000000ULL + (uint64_t)((wid) & 15) * 0x1000000ULL)
static inline void s_sleep(int ms) {                  /* REAL park: timer wake */
    syscall3(SYS_SLEEP, ms, 0, 0);
}
/* compositor wait: park until a client presents or the timeout expires.
 * Returns 1 when a fresh frame exists (wake came from a present). */
static inline int s_wm_wait(int timeout_ms) {
    return (int)syscall3(54, timeout_ms, 0, 0);
}
/* lock-free client rendering: allocate the extra draw canvases. out[0]/
 * out[1] get the new canvas addresses (0 when the arena had no room).
 * Returns the buffer count (1..3). */
static inline int s_wm_backbuf(int wid, uint64_t out[2]) {
    return (int)syscall3(55, wid, (int64_t)out, 0);
}
/* publish a finished draw buffer: it becomes the compositor-visible one
 * atomically. The client never draws into the visible canvas, so the
 * compositor can read it at ANY moment without catching a torn frame. */
static inline void s_wm_present_buf(int wid, uint32_t *buf) {
    syscall3(56, wid, (int64_t)(unsigned long)buf, 0);
}

/* ---- notifications (kernel queue; the desktop renders toasts) ---- */
typedef struct {
    uint32_t kind;           /* 0 info 1 ok 2 warn 3 error               */
    uint32_t tick;           /* uptime (cs) when posted                  */
    char title[40];
    char body[128];
} swirl_notif_t;

static inline void s_notify(const char *title, const char *body, int kind) {
    syscall3(SYS_NOTIFY_SEND, (int64_t)title, (int64_t)body, kind);
}
static inline int  s_notif_drain(void *buf, int max) {
    return (int)syscall3(SYS_NOTIF_DRAIN, (int64_t)buf, max, 0);
}
static inline void s_beep(int freq, int ms) {
    syscall3(SYS_BEEP, freq, ms, 0);
}
static inline void s_setvol(int v)  { syscall3(SYS_SETVOL, v, 0, 0); }

/* ---- r57: Settings daily-driver controls ---- */
#define SYS_PTRCFG 95   /* a=speed %, b=enhance precision, c=swap buttons */
#define SYS_RTC_SET 96   /* rdi = int[6]{s,m,h,d,mo,y} local wall time    */
#define SYS_TZ_SET 97    /* a = minutes east of UTC, b = EU DST          */
#define SYS_SNDBAL 98    /* a = balance -100..100 (0 = centre)           */
static inline void s_ptrcfg(int speed_pct, int accel, int swap) {
    syscall3(SYS_PTRCFG, speed_pct, accel, swap);
}
static inline int  s_rtc_set(const int t[6]) {    /* 0 ok, -1 bad, -2 busy */
    return (int)syscall3(SYS_RTC_SET, (int64_t)t, 0, 0);
}
static inline void s_tz_set(int offset_min, int dst_eu) {
    syscall3(SYS_TZ_SET, offset_min, dst_eu, 0);
}
static inline void s_set_balance(int bal) {
    syscall3(SYS_SNDBAL, bal, 0, 0);
}

/* ---- sound (kernel AC'97 mixer, syscalls 90/91) ----
 * The machine format: 48 kHz stereo S16LE, interleaved (L,R). Any
 * process can append frames; the kernel mixes every stream, applies
 * the tray's master volume and feeds the sound card. Non-blocking:
 * returns the bytes accepted (0 = ring full; retry next frame). */
#define SYS_SND_WRITE 90
typedef struct {
    uint32_t rate, channels;   /* 48000, 2 (S16LE interleaved)      */
    uint32_t ring_bytes;       /* per-write ring capacity           */
    uint32_t present;          /* 1 = AC'97 hardware live            */
    uint32_t volume;           /* master volume 0..100              */
    uint32_t clients;          /* streams with data right now       */
    uint32_t hw_irqs, hw_fills;/* live engine counters              */
} swirl_sndinfo_t;
static inline int s_snd_write(const void *buf, int len) {
    return (int)syscall3(SYS_SND_WRITE, (int64_t)buf, len, 0);
}
static inline int s_snd_info(swirl_sndinfo_t *i) {
    return (int)syscall3(91, (int64_t)i, 0, 0);
}

/* shell hotkeys (never delivered to apps): alt+tab, alt+arrows,
 * printscreen, ctrl+space, alt up/down */
typedef struct { uint8_t down, code, ascii, mods; } swirl_syskey_t;
/* key grabs (r46): the window manager tells the kernel which combos are
 * ITS (never delivered to a client). A key-down matches when
 * (mods & need) == need, code == 0 or equal, ascii == 0 or equal; `send`
 * (0 = the real code) is what s_syskey_poll reports. mods: 1 shift 2 ctrl 4 alt */
typedef struct { uint8_t need, code, ascii, send; } swirl_keygrab_t;
static inline int s_wm_keygrab(const swirl_keygrab_t *g, int n) {
    return (int)syscall3(SYS_WM_KEYGRAB, (int64_t)g, n, 0);
}
static inline int s_syskey_poll(swirl_syskey_t *sk) {
    return (int)syscall3(SYS_SYSKEY_POLL, (int64_t)sk, 0, 0);
}

/* ---- networking (kernel stack: ether/arp/ipv4/icmp + 6 NIC drivers) --
 * Mirror of kernel/net.h net_dev_rec_t - keep both sides in sync. */
#define SYS_NET_LIST        65
#define SYS_NET_CTRL        66
#define SYS_NET_PING_START  67
#define SYS_NET_PING_POLL   68
#define SYS_NET_WIFIDEBUG   69

/* driver lifecycle states */
#define NDS_NO_DEVICE 0     /* driver ready, hardware not present       */
#define NDS_UNBOUND   1     /* hardware present, driver uninstalled     */
#define NDS_DISABLED  2     /* bound + initialised, interface down      */
#define NDS_ACTIVE    3     /* bound, up, traffic flows                 */

/* driver self-test states */
#define NTST_NONE 0
#define NTST_RUNNING 1
#define NTST_PASS 2
#define NTST_FAIL 3

typedef struct {
    char     name[40];
    char     chip[20];
    char     bus_note[32];
    uint8_t  state;
    uint8_t  test_state;
    uint8_t  link_up;
    uint8_t  drv_index;   /* roster index: pass to s_net_ctrl */
    char     test_note[96];
    uint32_t ip, gw, mask;
    uint8_t  mac[6];
    uint16_t irq;
    uint8_t  pci_loc[4];
    uint32_t speed;
    uint32_t rx_pkts, tx_pkts, rx_err, tx_err;
    char     ifname[8];
    uint32_t rx_bytes, tx_bytes;
} net_dev_rec_t;

/* net_ctrl commands */
#define NETC_ENABLE    1
#define NETC_DISABLE   2
#define NETC_TEST      3
#define NETC_UNINSTALL 4

static inline int s_net_list(net_dev_rec_t *buf, int max) {
    return (int)syscall3(SYS_NET_LIST, (int64_t)buf, max, 0);
}
static inline int s_net_ctrl(int idx, int cmd, char *note, int note_max) {
    (void)note_max;                    /* note is NUL-terminated by the kernel */
    return (int)syscall3(SYS_NET_CTRL, idx, cmd, (int64_t)note);
}
static inline int s_net_ping_start(const char *ip, int count) {
    return (int)syscall3(SYS_NET_PING_START, (int64_t)ip, count, 0);
}
static inline int s_net_ping_poll(char *line, int max) {
    return (int)syscall3(SYS_NET_PING_POLL, (int64_t)line, max, 0);
}
static inline int s_net_wifidebug(char *buf, int max) {
    return (int)syscall3(SYS_NET_WIFIDEBUG, (int64_t)buf, max, 0);
}

/* ---- Wi-Fi (kernel/wifi/wifi.h, syscall 89). Layouts mirror the kernel's. ---- */
#define SYS_WIFI_CTL 89
enum { WS_NONE = 0, WS_IDLE, WS_SCAN, WS_AUTH, WS_ASSOC, WS_KEYS, WS_UP };
enum { WSEC_OPEN = 0, WSEC_WPA2, WSEC_WEP, WSEC_WPA1, WSEC_EAP, WSEC_SAE, WSEC_MFPR };
enum { WR_NONE = 0, WR_NO_AP, WR_AUTH, WR_ASSOC, WR_KEYS, WR_LOST, WR_KICKED,
       WR_UNSUPPORTED, WR_BADPASS };
enum { WCTL_STATUS = 1, WCTL_SCAN, WCTL_LIST, WCTL_CONNECT, WCTL_DISCONNECT,
       WCTL_FORGET, WCTL_SAVED, WCTL_AUTO };
typedef struct {
    char     ssid[33];
    uint8_t  bssid[6];
    uint8_t  chan;
    int8_t   rssi;            /* dBm */
    uint8_t  sec;             /* WSEC_* */
    uint8_t  flags;           /* 1 = connected to this one, 2 = saved */
    uint8_t  age;
    uint8_t  pad[4];
} wifi_ap_rec_t;
typedef struct {
    uint8_t  state, present, chan;
    int8_t   rssi;
    uint8_t  sec, reason, scanning, saved_count;
    char     ssid[33];
    uint8_t  bssid[6];
    uint8_t  rate;            /* units of 500 kbit/s */
    uint8_t  pad;
    uint32_t ip;
    uint32_t rx_pkts, tx_pkts;
    char     chip[24];
    char     note[64];
} wifi_status_t;
static inline int s_wifi(int cmd, const void *a, int64_t b) {
    return (int)syscall3(SYS_WIFI_CTL, cmd, (int64_t)a, b);
}

/* ---- TCP/UDP sockets (kernel/tcpip.c; syscalls 70..77) ----
 * Non-blocking client sockets over the kernel stack. IP addresses are
 * u32 in "human order" (10.0.2.15 = 0x0A00020F, same as NET_LIST). */
#define SYS_SOCK_OPEN    70
#define SYS_SOCK_CONNECT 71
#define SYS_SOCK_SEND    72
#define SYS_SOCK_RECV    73
#define SYS_SOCK_CLOSE   74
#define SYS_SOCK_POLL    75
#define SYS_NET_GETINFO  77

#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* poll() status bits */
#define SCK_readable   0x1   /* recv() has data (or EOF: also set)      */
#define SCK_writable   0x2
#define SCK_estab      0x4   /* TCP established / UDP ready             */
#define SCK_eof        0x10  /* peer closed; recv -> -1 when drained    */
#define SCK_reset      0x8   /* RST or timeout: recv/send -> -4         */

/* send/recv results: n bytes; -2 would-block; -3 not connected;
 * -1 clean EOF (recv only); -4 reset */
static inline int  s_sock(int type)                    { return (int)syscall3(SYS_SOCK_OPEN, type, 0, 0); }
static inline int  s_sock_connect(int h, uint32_t ip, int port) { return (int)syscall3(SYS_SOCK_CONNECT, h, ip, port); }
static inline int  s_sock_send(int h, const void *b, int n)     { return (int)syscall3(SYS_SOCK_SEND, h, (int64_t)b, n); }
static inline int  s_sock_recv(int h, void *b, int max)         { return (int)syscall3(SYS_SOCK_RECV, h, (int64_t)b, max); }
static inline int  s_sock_close(int h)                 { return (int)syscall3(SYS_SOCK_CLOSE, h, 0, 0); }
static inline int  s_sock_poll(int h)                  { return (int)syscall3(SYS_SOCK_POLL, h, 0, 0); }
typedef struct { uint32_t ip, gw, mask, dns; } swirl_netinfo_t;
static inline int  s_net_info(swirl_netinfo_t *ni)     { return (int)syscall3(SYS_NET_GETINFO, (int64_t)ni, 0, 0); }
static inline uint32_t s_ip(const char a, const char b, const char c, const char d) {
    return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) |
           ((uint32_t)(uint8_t)c << 8) | (uint32_t)(uint8_t)d;
}

/* ---- libswirl std ---- */
int  printf(const char *fmt, ...);
int  vsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap);
int  snprintf(char *buf, size_t n, const char *fmt, ...);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
size_t strlen(const char *s);
int   strcmp(const char *a, const char *b);
int   strncmp(const char *a, const char *b, size_t n);
int   memcmp(const void *a, const void *b, size_t n);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
char *strrchr(const char *s, int c);

/* file helpers */
int  fcat(const char *path);                 /* print file to stdout */
int  fls(const char *path, int longform);    /* list directory */

#endif
