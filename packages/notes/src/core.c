#include "core.h"

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

void doc_clear(Doc *d) { d->len = d->cur = d->want = 0; d->b[0] = 0; }

void doc_set(Doc *d, const char *src, int n) {
    int o = 0;
    for (int i = 0; i < n && o < NOTE_MAX; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\r') { if (i + 1 < n && src[i + 1] == '\n') continue; c = '\n'; }
        else if (c == '\t') c = ' ';
        else if (c != '\n' && (c < 32 || c > 126)) c = '?';
        d->b[o++] = (char)c;
    }
    d->b[o] = 0;
    d->len = o;
    d->cur = 0;
    d->want = 0;
}

int doc_insert(Doc *d, const char *s, int n) {
    if (n > NOTE_MAX - d->len) n = NOTE_MAX - d->len;
    if (n <= 0) return 0;
    for (int i = d->len; i >= d->cur; i--) d->b[i + n] = d->b[i];   /* includes the NUL */
    for (int i = 0; i < n; i++) d->b[d->cur + i] = s[i];
    d->len += n;
    d->cur += n;
    return n;
}

void doc_backspace(Doc *d) {
    if (d->cur <= 0) return;
    for (int i = d->cur; i <= d->len; i++) d->b[i - 1] = d->b[i];
    d->cur--;
    d->len--;
}

void doc_delete(Doc *d) {
    if (d->cur >= d->len) return;
    for (int i = d->cur + 1; i <= d->len; i++) d->b[i - 1] = d->b[i];
    d->len--;
}

int wrap_lines(const char *b, int len, int cols, int *starts, int max) {
    if (cols < 4) cols = 4;
    int nl = 0, pos = 0;
    for (;;) {
        if (nl >= max) break;
        starts[nl++] = pos;
        int p = pos, c = 0, lastsp = -1;
        while (p < len && b[p] != '\n') {
            if (c == cols) break;
            if (b[p] == ' ') lastsp = p;
            p++;
            c++;
        }
        if (p >= len) break;                         /* last line, no trailing newline */
        if (b[p] == '\n') { pos = p + 1; continue; } /* hard break (may start an empty last line) */
        if (b[p] == ' ') pos = p + 1;                /* soft break on a space */
        else if (lastsp > pos) pos = lastsp + 1;     /* soft break at the last space */
        else pos = p;                                /* a word longer than the line: hard split */
    }
    return nl;
}

void cur_rc(const int *starts, int nl, int cur, int *row, int *col) {
    int r = 0;
    for (int i = 0; i < nl; i++) { if (starts[i] <= cur) r = i; else break; }
    *row = r;
    *col = cur - starts[r];
}

int line_len(const char *b, int len, const int *starts, int nl, int row) {
    int s = starts[row];
    int e = (row + 1 < nl) ? starts[row + 1] : len;
    int l = e - s;
    if (row + 1 < nl && l > 0 && b[e - 1] == '\n') l--;
    return l;
}

int rc_cur(const char *b, int len, const int *starts, int nl, int row, int col) {
    if (row < 0) row = 0;
    if (row >= nl) row = nl - 1;
    int ll = line_len(b, len, starts, nl, row);
    int hard = (row + 1 >= nl) || (starts[row + 1] > 0 && b[starts[row + 1] - 1] == '\n');
    int maxc = hard ? ll : (ll > 0 ? ll - 1 : 0);   /* a soft-wrapped line's end belongs to the next line */
    if (col > maxc) col = maxc;
    if (col < 0) col = 0;
    return starts[row] + col;
}

void doc_move(Doc *d, const int *starts, int nl, int dir, int page) {
    int row, col;
    cur_rc(starts, nl, d->cur, &row, &col);
    switch (dir) {
        case 0: if (d->cur > 0) d->cur--; d->want = -1; return;
        case 1: if (d->cur < d->len) d->cur++; d->want = -1; return;
        case 4: d->cur = starts[row]; d->want = -1; return;
        case 5: d->cur = rc_cur(d->b, d->len, starts, nl, row, 1 << 20); d->want = -1; return;
    }
    if (d->want < 0) d->want = col;
    int want = d->want;
    int nr = row + (dir == 2 ? -1 : dir == 3 ? 1 : dir == 6 ? -page : page);
    if (nr < 0) { d->cur = 0; return; }
    if (nr >= nl) { d->cur = d->len; return; }
    d->cur = rc_cur(d->b, d->len, starts, nl, nr, want);
    d->want = want;
}

/* ---- undo / redo ---- */
#define UNDO_N 16
typedef struct { char b[NOTE_MAX + 1]; int len, cur; } Snap;
static Snap ustack[UNDO_N], rstack[UNDO_N];
static int un, rn;                  /* counts */

static void snap_take(Snap *s, const Doc *d) {
    for (int i = 0; i <= d->len; i++) s->b[i] = d->b[i];
    s->len = d->len;
    s->cur = d->cur;
}
static void snap_put(const Snap *s, Doc *d) {
    for (int i = 0; i <= s->len; i++) d->b[i] = s->b[i];
    d->len = s->len;
    d->cur = s->cur;
    d->want = -1;
}
static void stack_push(Snap *st, int *n, const Doc *d) {
    if (*n == UNDO_N) {
        for (int i = 1; i < UNDO_N; i++) st[i - 1] = st[i];
        (*n)--;
    }
    snap_take(&st[(*n)++], d);
}

void undo_reset(void) { un = rn = 0; }
void undo_push(const Doc *d) { stack_push(ustack, &un, d); rn = 0; }

int undo_do(Doc *d) {
    if (!un) return 0;
    stack_push(rstack, &rn, d);
    snap_put(&ustack[--un], d);
    return 1;
}
int redo_do(Doc *d) {
    if (!rn) return 0;
    stack_push(ustack, &un, d);
    snap_put(&rstack[--rn], d);
    return 1;
}

/* ---- list helpers ---- */
static int next_line(const char *b, int len, int *pos, int *s, int *e) {
    while (*pos < len) {
        int a = *pos;
        while (a < len && (b[a] == ' ' || b[a] == '\n')) a++;
        if (a >= len) { *pos = len; return 0; }
        int z = a;
        while (z < len && b[z] != '\n') z++;
        *s = a; *e = z; *pos = z;
        return 1;
    }
    return 0;
}

static void copy_span(const char *b, int s, int e, char *out, int n) {
    int o = 0;
    while (s < e && o < n - 1) out[o++] = b[s++];
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
}

void note_title(const char *b, int len, char *out, int n) {
    int pos = 0, s, e;
    if (next_line(b, len, &pos, &s, &e)) copy_span(b, s, e, out, n);
    else { const char *t = "New note"; int i = 0; while (t[i] && i < n - 1) { out[i] = t[i]; i++; } out[i] = 0; }
}

void note_preview(const char *b, int len, char *out, int n) {
    int pos = 0, s, e;
    out[0] = 0;
    if (!next_line(b, len, &pos, &s, &e)) return;
    if (next_line(b, len, &pos, &s, &e)) copy_span(b, s, e, out, n);
}

int ci_contains(const char *hay, const char *needle) {
    if (!*needle) return 1;
    for (; *hay; hay++) {
        int i = 0;
        while (needle[i] && hay[i] && lower((unsigned char)hay[i]) == lower((unsigned char)needle[i])) i++;
        if (!needle[i]) return 1;
    }
    return 0;
}
