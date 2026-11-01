/* Swirl Notes - portable editing core (no OS headers, host-testable). */
#ifndef NOTES_CORE_H
#define NOTES_CORE_H

#define NOTE_MAX   16384          /* bytes per note */
#define MAX_LINES  (NOTE_MAX + 2) /* visual lines */

typedef struct {
    char b[NOTE_MAX + 1];
    int len, cur;
    int want;                     /* remembered column for up/down */
} Doc;

void doc_clear(Doc *d);
void doc_set(Doc *d, const char *src, int n);          /* loads + sanitises (CRLF, tabs, odd bytes) */
int  doc_insert(Doc *d, const char *s, int n);         /* returns bytes inserted */
void doc_backspace(Doc *d);
void doc_delete(Doc *d);

/* word-wrapped layout: starts[i] = byte offset where visual line i begins.
 * returns the number of lines (>= 1). */
int  wrap_lines(const char *b, int len, int cols, int *starts, int max);
void cur_rc(const int *starts, int nl, int cur, int *row, int *col);
int  rc_cur(const char *b, int len, const int *starts, int nl, int row, int col);
int  line_len(const char *b, int len, const int *starts, int nl, int row);   /* visible chars */

/* cursor movement over a layout. dir: 0 left 1 right 2 up 3 down 4 home 5 end 6 pgup 7 pgdn */
void doc_move(Doc *d, const int *starts, int nl, int dir, int page);

/* undo / redo (snapshots of the whole note) */
void undo_reset(void);
void undo_push(const Doc *d);       /* call BEFORE an edit */
int  undo_do(Doc *d);               /* 1 if something was undone */
int  redo_do(Doc *d);

/* list helpers */
void note_title(const char *b, int len, char *out, int n);     /* first non-empty line */
void note_preview(const char *b, int len, char *out, int n);   /* following non-empty line */
int  ci_contains(const char *hay, const char *needle);         /* case-insensitive substring */

#endif
