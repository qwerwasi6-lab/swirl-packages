/* Swirl Notes - native SwirlOS app (static-PIE ELF, SwirlWM client).
 *
 * Sidebar with search + note list, word-wrapped monospace editor, one .txt
 * file per note in /home/user/Notes/. Auto-saves ~0.7 s after you stop typing
 * (and on focus loss / close). Editing logic lives in core.c (host-tested);
 * this file is the SwirlOS glue + drawing. Build: see build.sh. */
#include "gclient_core.h"
#include "core.h"

#define NOTES_DIR  "/home/user/Notes"
#define MAX_NOTES  128
#define SB_W       232          /* sidebar width */
#define HDR_H      44
#define ROW_H      54
#define ED_SIZE    14
#define ED_LH      20
#define ED_PAD     18
#define SAVE_DELAY 700
#define DEL_CONFIRM_MS 3000

typedef struct {
    char id[28];                /* file name without .txt, sorts by creation time */
    char title[56];
    char prev[72];
    char snip[300];             /* first ~300 chars, for search */
} Meta;

static Meta N[MAX_NOTES];
static int nnotes, curn = -1;
static int vis[MAX_NOTES], nvis;           /* filtered view -> index into N */
static int list_off;                       /* first visible row of the list */

static Doc D;
static int LS[MAX_LINES];                  /* visual line starts */
static int nl, cols, row_off;
static int ed_x, ed_w, ed_h, cw;           /* editor geometry (content px) */

static char query[48];
static int qlen, focus_search;
static int dirty_doc;                      /* unsaved edits */
static uint32_t last_edit, del_armed, blink0;
static int last_kind;
static int dirty = 1;
static int hover = -1;                     /* 0 new, 1 delete, 2 search, 3+ list rows */
static int mxp, myp, left_prev;
static wclient_t *C;

/* ---------------------------------------------------------------- storage */

static void path_of(const char *id, char *out, int n) { snprintf(out, n, NOTES_DIR "/%s.txt", id); }

static int read_note(const char *id, char *buf, int max) {
    char p[96];
    path_of(id, p, sizeof p);
    int fd = s_open(p, O_READ);
    if (fd < 0) return 0;
    int tot = 0, r;
    while (tot < max && (r = s_read(fd, buf + tot, max - tot)) > 0) tot += r;
    s_close(fd);
    return tot;
}

static int write_note(const char *id, const char *buf, int n) {
    char p[96];
    path_of(id, p, sizeof p);
    int fd = s_open(p, O_WRITE | O_CREATE | O_TRUNC);
    if (fd < 0) return -1;
    int off = 0;
    while (off < n) {
        int w = s_write(fd, buf + off, n - off);
        if (w <= 0) { s_close(fd); return -1; }
        off += w;
    }
    s_close(fd);
    return 0;
}

static void meta_fill(Meta *m, const char *b, int len) {
    note_title(b, len, m->title, sizeof m->title);
    note_preview(b, len, m->prev, sizeof m->prev);
    int n = len < (int)sizeof m->snip - 1 ? len : (int)sizeof m->snip - 1;
    for (int i = 0; i < n; i++) m->snip[i] = b[i] == '\n' ? ' ' : b[i];
    m->snip[n] = 0;
}

static void sort_notes(void) {              /* newest (largest id) first */
    for (int i = 1; i < nnotes; i++) {
        Meta t = N[i];
        int j = i - 1;
        while (j >= 0 && strcmp(N[j].id, t.id) < 0) { N[j + 1] = N[j]; j--; }
        N[j + 1] = t;
    }
}

static void scan_notes(void) {
    static char tmp[NOTE_MAX + 1];
    nnotes = 0;
    int h = s_dir_open(NOTES_DIR);
    if (h < 0) return;
    struct { uint32_t type; char name[56]; } e;
    while (s_dir_read(h, &e) > 0 && nnotes < MAX_NOTES) {
        int l = (int)strlen(e.name);
        if (e.type == 2 || l < 5 || l - 4 >= (int)sizeof N[0].id || strcmp(e.name + l - 4, ".txt")) continue;
        Meta *m = &N[nnotes];
        memcpy(m->id, e.name, l - 4);
        m->id[l - 4] = 0;
        int n = read_note(m->id, tmp, NOTE_MAX);
        Doc *t = &D;                         /* borrow D for sanitising; reloaded after */
        doc_set(t, tmp, n);
        meta_fill(m, t->b, t->len);
        nnotes++;
    }
    s_close(h);
    sort_notes();
}

/* ---------------------------------------------------------------- view */

static void rebuild_vis(void) {
    nvis = 0;
    for (int i = 0; i < nnotes; i++)
        if (!qlen || ci_contains(N[i].title, query) || ci_contains(N[i].snip, query))
            vis[nvis++] = i;
}

static int vis_row_of(int idx) { for (int i = 0; i < nvis; i++) if (vis[i] == idx) return i; return -1; }

static void relayout(void) {
    ed_x = SB_W;
    ed_w = C->w - SB_W;
    ed_h = C->h - HDR_H;
    cw = (text_wm("0000000000", ED_SIZE) - 1) / 10;
    if (cw < 4) cw = 8;
    cols = (ed_w - 2 * ED_PAD - 10) / cw;
    if (cols < 8) cols = 8;
    nl = wrap_lines(D.b, D.len, cols, LS, MAX_LINES);
}

static int vis_rows(void) { int r = (ed_h - 2 * 6) / ED_LH; return r < 1 ? 1 : r; }

static void keep_cursor_visible(void) {
    int r, c;
    cur_rc(LS, nl, D.cur, &r, &c);
    int vr = vis_rows();
    if (r < row_off) row_off = r;
    if (r >= row_off + vr) row_off = r - vr + 1;
    if (row_off > nl - 1) row_off = nl - 1;
    if (row_off < 0) row_off = 0;
}

static void keep_list_visible(void) {
    int lr = (C->h - 98) / ROW_H;
    if (lr < 1) lr = 1;
    int r = vis_row_of(curn);
    if (r < 0) return;
    if (r < list_off) list_off = r;
    if (r >= list_off + lr) list_off = r - lr + 1;
    if (list_off < 0) list_off = 0;
}

static void touch(void) { dirty = 1; blink0 = s_ticks_ms(); }

/* ---------------------------------------------------------------- actions */

static void save_now(void) {
    if (!dirty_doc || curn < 0) return;
    if (write_note(N[curn].id, D.b, D.len) == 0) {
        meta_fill(&N[curn], D.b, D.len);
        dirty_doc = 0;
    }
}

static void load_note(int idx) {
    static char tmp[NOTE_MAX + 1];
    curn = idx;
    int n = read_note(N[idx].id, tmp, NOTE_MAX);
    doc_set(&D, tmp, n);
    D.cur = D.len;                           /* cursor at the end, ready to type */
    undo_reset();
    dirty_doc = 0;
    row_off = 0;
    last_kind = 0;
    relayout();
    keep_cursor_visible();
    keep_list_visible();
    del_armed = 0;
}

static void remove_meta(int idx) {
    for (int i = idx; i + 1 < nnotes; i++) N[i] = N[i + 1];
    nnotes--;
}

static void select_note(int idx) {
    if (idx == curn || idx < 0 || idx >= nnotes) return;
    save_now();
    if (curn >= 0 && D.len == 0) {           /* leaving a blank note: it is just clutter */
        char p[96];
        path_of(N[curn].id, p, sizeof p);
        s_unlink(p);
        remove_meta(curn);
        if (idx > curn) idx--;
        curn = -1;
    }
    load_note(idx);
    rebuild_vis();
    keep_list_visible();
    touch();
}

static void new_note(void) {
    if (curn >= 0 && D.len == 0) { focus_search = 0; touch(); return; }   /* already a blank one */
    save_now();
    swirl_rtc_t t;
    s_rtc6(&t);
    char id[28];
    snprintf(id, sizeof id, "%04d%02d%02d-%02d%02d%02d", (int)t.y, (int)t.mo, (int)t.d, (int)t.h, (int)t.mi, (int)t.s);
    for (int k = 1; k < 100; k++) {          /* never clobber an existing id */
        int clash = 0;
        for (int i = 0; i < nnotes; i++) if (!strcmp(N[i].id, id)) clash = 1;
        if (!clash) break;
        snprintf(id, sizeof id, "%04d%02d%02d-%02d%02d%02d-%d", (int)t.y, (int)t.mo, (int)t.d, (int)t.h, (int)t.mi, (int)t.s, k);
    }
    if (nnotes >= MAX_NOTES) return;
    if (write_note(id, "", 0) != 0) return;
    for (int i = nnotes; i > 0; i--) N[i] = N[i - 1];
    memset(&N[0], 0, sizeof N[0]);
    snprintf(N[0].id, sizeof N[0].id, "%s", id);
    snprintf(N[0].title, sizeof N[0].title, "New note");
    nnotes++;
    qlen = 0; query[0] = 0; focus_search = 0;
    curn = -1;
    load_note(0);
    rebuild_vis();
    list_off = 0;
    touch();
}

static void delete_note(void) {
    if (curn < 0) return;
    char p[96];
    path_of(N[curn].id, p, sizeof p);
    s_unlink(p);
    int gone = curn;
    remove_meta(gone);
    dirty_doc = 0;
    curn = -1;
    if (nnotes == 0) { doc_clear(&D); new_note(); return; }
    load_note(gone < nnotes ? gone : nnotes - 1);
    rebuild_vis();
    keep_list_visible();
    touch();
}

/* ---------------------------------------------------------------- editing */

static void edit_begin(int kind) {           /* snapshot for undo at natural boundaries */
    uint32_t now = s_ticks_ms();
    if (kind != last_kind || now - last_edit > 1000) undo_push(&D);
    last_kind = kind;
    last_edit = now;
    dirty_doc = 1;
}

static void after_edit(void) {
    relayout();
    keep_cursor_visible();
    D.want = -1;
    touch();
}

static void insert_text(const char *s, int n, int kind) {
    if (D.len >= NOTE_MAX) return;
    edit_begin(kind);
    doc_insert(&D, s, n);
    after_edit();
}

static void paste(void) {
    char buf[2048];
    int n = s_clip_get(buf, sizeof buf - 1);
    if (n <= 0) return;
    static Doc scratch;                      /* sanitise like a file load */
    doc_set(&scratch, buf, n);
    insert_text(scratch.b, scratch.len, 3);
}

static void move_cursor(int dir) {
    int page = vis_rows() - 1;
    doc_move(&D, LS, nl, dir, page < 1 ? 1 : page);
    last_kind = 0;
    keep_cursor_visible();
    touch();
}

static void do_undo(int redo) {
    save_now();
    if (redo ? redo_do(&D) : undo_do(&D)) {
        dirty_doc = 1;
        last_edit = s_ticks_ms();
        last_kind = 0;
        after_edit();
    }
}

static void key_search(int k) {
    if (k == 27) { qlen = 0; query[0] = 0; focus_search = 0; rebuild_vis(); touch(); return; }
    if (k == '\n' || k == '\r') { focus_search = 0; touch(); return; }
    if (k == '\b' || k == 127) { if (qlen) query[--qlen] = 0; }
    else if (k >= 32 && k < 127 && qlen < (int)sizeof query - 1) { query[qlen++] = (char)k; query[qlen] = 0; }
    else if (k == 0x82 || k == 0x81) {       /* up/down walk the filtered list */
        int r = vis_row_of(curn);
        int nr = r + (k == 0x82 ? 1 : -1);
        if (nr >= 0 && nr < nvis) select_note(vis[nr]);
        return;
    } else return;
    rebuild_vis();
    list_off = 0;
    if (nvis && vis_row_of(curn) < 0) select_note(vis[0]);
    touch();
}

static void on_key(int k) {
    if (k == 0xAD) { new_note(); return; }                         /* ctrl+n */
    if (k == 0xA5) { focus_search = 1; touch(); return; }          /* ctrl+f */
    if (k == 0xA3) {                                               /* ctrl+d, press twice */
        uint32_t now = s_ticks_ms();
        if (del_armed && now - del_armed < DEL_CONFIRM_MS) { del_armed = 0; delete_note(); }
        else { del_armed = now ? now : 1; touch(); }
        return;
    }
    if (focus_search) { key_search(k); return; }
    del_armed = 0;
    switch (k) {
        case 0xB9: do_undo(0); return;                             /* ctrl+z */
        case 0xB8: do_undo(1); return;                             /* ctrl+y */
        case 0xB5: paste(); return;                                /* ctrl+v */
        case 0x81: move_cursor(2); return;
        case 0x82: move_cursor(3); return;
        case 0x83: move_cursor(0); return;
        case 0x84: move_cursor(1); return;
        case 0x89: move_cursor(4); return;
        case 0x8A: move_cursor(5); return;
        case 0x8B: move_cursor(6); return;
        case 0x8C: move_cursor(7); return;
        case 0x85: edit_begin(2); doc_delete(&D); after_edit(); return;
        case '\b': case 127: edit_begin(2); doc_backspace(&D); after_edit(); return;
        case '\n': case '\r': insert_text("\n", 1, 3); return;
        case '\t': insert_text("  ", 2, 1); return;
        case 27: return;
    }
    if (k >= 32 && k < 127) { char ch = (char)k; insert_text(&ch, 1, 1); }
}

/* ---------------------------------------------------------------- mouse */

static int in_rect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

#define NEW_X  (SB_W - 46)
static int hit_test(int x, int y) {
    if (in_rect(x, y, NEW_X, 8, 34, 28)) return 0;
    if (in_rect(x, y, C->w - 100, 8, 88, 28)) return 1;
    if (in_rect(x, y, 12, 50, SB_W - 24, 30)) return 2;
    if (x < SB_W && y >= 92) {
        int r = list_off + (y - 92) / ROW_H;
        if (r >= 0 && r < nvis) return 3 + r;
    }
    return -1;
}

static void click(int x, int y) {
    int h = hit_test(x, y);
    if (h == 0) { new_note(); return; }
    if (h == 1) { uint32_t now = s_ticks_ms(); if (del_armed && now - del_armed < DEL_CONFIRM_MS) { del_armed = 0; delete_note(); } else { del_armed = now ? now : 1; touch(); } return; }
    if (h == 2) { focus_search = 1; touch(); return; }
    if (h >= 3) { select_note(vis[h - 3]); focus_search = 0; touch(); return; }
    if (x >= ed_x && y >= HDR_H) {
        focus_search = 0;
        int rowi = row_off + (y - HDR_H - 6) / ED_LH;
        int col = (x - ed_x - ED_PAD + cw / 2) / cw;
        if (rowi < 0) rowi = 0;
        if (col < 0) col = 0;
        D.cur = rc_cur(D.b, D.len, LS, nl, rowi, col);
        D.want = -1;
        last_kind = 0;
        touch();
    }
}

static void on_mouse(const wm_event_t *ev) {
    int x = (int)ev->a - 1, y = (int)ev->b;
    int left = (int)(ev->c & 1);
    if (x != mxp || y != myp) {
        mxp = x; myp = y;
        int h = hit_test(x, y);
        if (h != hover) { hover = h; dirty = 1; }
    }
    if (left && !left_prev) click(x, y);
    left_prev = left;
}

/* ---------------------------------------------------------------- drawing */

static void draw_button(int x, int y, int w, int h, const char *label, int hot, uint32_t fill, uint32_t ink) {
    gfx_round_fill(x, y, w, h, 7, hot ? fill : app_bg2);
    gfx_round_outline(x, y, w, h, 7, app_line);
    int tw = text_wb(label, 13);
    text_clip_b(x + (w - tw) / 2, y + h / 2 + 5, label, w, 13, hot ? ink : app_ink);
}

static void draw(void) {
    int W = C->w, H = C->h;
    gfx_clip_reset();
    gfx_rect(0, 0, W, H, app_bg);

    /* ---- sidebar ---- */
    gfx_rect(0, 0, SB_W, H, app_bg2);
    gfx_rect(SB_W - 1, 0, 1, H, app_line);
    text_clip_b(16, 29, "Notes", SB_W - 70, 17, app_ink);
    draw_button(NEW_X, 8, 34, 28, "+", hover == 0, ui_accent, 0xFFFFFF);

    int sf = focus_search;
    gfx_round_fill(12, 50, SB_W - 24, 30, 7, app_bg);
    gfx_round_outline(12, 50, SB_W - 24, 30, 7, sf ? ui_accent : app_line);
    if (qlen) text_clip(22, 70, query, SB_W - 56, 13, app_ink);
    else text_clip(22, 70, "Search (Ctrl+F)", SB_W - 56, 13, app_ink_faint);
    if (sf && ((s_ticks_ms() - blink0) / 530) % 2 == 0)
        gfx_rect(22 + (qlen ? text_w(query, 13) : 0) + 1, 56, 2, 18, app_ink);

    int lr = (H - 98) / ROW_H;
    gfx_clip(0, 92, SB_W - 1, H - 92);
    for (int i = 0; i < lr + 1; i++) {
        int r = list_off + i;
        if (r >= nvis) break;
        int idx = vis[r], y = 92 + i * ROW_H;
        int sel = idx == curn;
        if (sel) gfx_round_fill_a(8, y + 2, SB_W - 17, ROW_H - 4, 8, ui_accent, 70);
        else if (hover == 3 + r) gfx_round_fill_a(8, y + 2, SB_W - 17, ROW_H - 4, 8, app_ink, 18);
        text_clip_b(20, y + 24, N[idx].title, SB_W - 40, 14, app_ink);
        if (N[idx].prev[0]) text_clip(20, y + 43, N[idx].prev, SB_W - 40, 12, app_ink_dim);
    }
    gfx_clip_reset();
    if (nvis == 0) text_clip(20, 120, qlen ? "No matching notes" : "No notes yet", SB_W - 40, 13, app_ink_faint);
    if (nvis > lr + 1 || list_off > 0) {          /* thin scroll indicator */
        int tot = nvis, trackh = H - 98;
        int bh = trackh * lr / (tot ? tot : 1); if (bh < 24) bh = 24; if (bh > trackh) bh = trackh;
        int by = 92 + (trackh - bh) * list_off / (tot - lr > 0 ? tot - lr : 1);
        gfx_round_fill_a(SB_W - 7, by, 3, bh, 1, app_ink, 60);
    }

    /* ---- editor header ---- */
    gfx_rect(ed_x, HDR_H - 1, ed_w, 1, app_line);
    char info[40];
    snprintf(info, sizeof info, "%d chars", D.len);
    text_clip(ed_x + ED_PAD, 28, curn >= 0 ? (dirty_doc ? "Saving..." : "Saved") : "", 140, 12, app_ink_faint);
    text_clip(ed_x + ED_PAD + 80, 28, info, 120, 12, app_ink_faint);
    int armed = del_armed && s_ticks_ms() - del_armed < DEL_CONFIRM_MS;
    draw_button(W - 100, 8, 88, 28, armed ? "Sure?" : "Delete", hover == 1 || armed, app_danger, 0xFFFFFF);

    /* ---- editor ---- */
    gfx_clip(ed_x, HDR_H, ed_w, ed_h);
    int vr = vis_rows();
    int crow, ccol;
    cur_rc(LS, nl, D.cur, &crow, &ccol);
    if (D.len == 0 && !focus_search) text_clip_m(ed_x + ED_PAD, HDR_H + 6 + 15, "Start typing...", ed_w - 2 * ED_PAD, ED_SIZE, app_ink_faint);
    char line[512];
    for (int i = 0; i < vr + 1; i++) {
        int r = row_off + i;
        if (r >= nl) break;
        int n = line_len(D.b, D.len, LS, nl, r);
        if (n > (int)sizeof line - 1) n = (int)sizeof line - 1;
        memcpy(line, D.b + LS[r], n);
        line[n] = 0;
        if (n) textm(ed_x + ED_PAD, HDR_H + 6 + i * ED_LH + 15, line, ED_SIZE, app_ink);
    }
    if (!focus_search && ((s_ticks_ms() - blink0) / 530) % 2 == 0 && crow >= row_off && crow < row_off + vr + 1)
        gfx_rect(ed_x + ED_PAD + ccol * cw, HDR_H + 6 + (crow - row_off) * ED_LH + 2, 2, ED_LH - 4, ui_accent);
    gfx_clip_reset();
    if (nl > vr) {                                 /* scroll thumb */
        int trackh = ed_h - 12;
        int bh = trackh * vr / nl; if (bh < 24) bh = 24;
        int by = HDR_H + 6 + (trackh - bh) * row_off / (nl - vr);
        gfx_round_fill_a(W - 8, by, 4, bh, 2, app_ink, 55);
    }
}

/* ---------------------------------------------------------------- main */

int main(void) {
    wclient_t c;
    if (wclient_open(&c, "Notes", 760, 520)) s_exit(1);
    C = &c;
    c.win.state = 0;
    c.win.focused = 1;
    settings_load();
    s_mkdir("/home/user");
    s_mkdir(NOTES_DIR);

    doc_clear(&D);
    scan_notes();
    relayout();
    if (nnotes == 0) new_note();
    else { load_note(0); rebuild_vis(); }
    blink0 = s_ticks_ms();
    draw();
    wclient_present(&c);

    uint32_t last_blink = 0;
    for (;;) {
        wm_event_t ev;
        while (s_wm_poll_nb(&ev)) {
            switch (ev.type) {
            case WM_EVT_KEY: { int k = gkey(&ev); if (k) on_key(k); break; }
            case WM_EVT_MOUSE: on_mouse(&ev); break;
            case WM_EVT_FOCUS:
                c.win.focused = (int)ev.a;
                if (!ev.a) save_now();
                dirty = 1;
                break;
            case WM_EVT_RESIZE:
                wclient_resize(&c, (int)ev.a - 2, (int)ev.b - (TITLE_H + 1));
                relayout();
                keep_cursor_visible();
                keep_list_visible();
                dirty = 1;
                break;
            case WM_EVT_REFRESH: settings_load(); dirty = 1; break;
            case WM_EVT_CLOSE:
                save_now();
                if (curn >= 0 && D.len == 0) { char p[96]; path_of(N[curn].id, p, sizeof p); s_unlink(p); }
                s_exit(0);
                break;
            default: ;
            }
            if (!c.win.used) { save_now(); s_exit(0); }
        }
        uint32_t now = s_ticks_ms();
        if (dirty_doc && now - last_edit > SAVE_DELAY) { save_now(); dirty = 1; }
        if (del_armed && now - del_armed >= DEL_CONFIRM_MS) { del_armed = 0; dirty = 1; }
        if (now - last_blink >= 530) { last_blink = now; dirty = 1; }   /* caret blink */
        if (dirty) { dirty = 0; draw(); wclient_present(&c); }
        else s_sleep(16);
    }
    return 0;
}
