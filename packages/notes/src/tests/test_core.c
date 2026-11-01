#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core.h"
static Doc D; static int S[MAX_LINES];
#define T(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
int main(void) {
    int fails = 0;
    doc_clear(&D);
    doc_insert(&D, "hello world", 11);
    T(D.len == 11 && D.cur == 11 && !strcmp(D.b, "hello world"));
    D.cur = 5; doc_insert(&D, ",", 1); T(!strcmp(D.b, "hello, world"));
    doc_backspace(&D); T(!strcmp(D.b, "hello world") && D.cur == 5);
    doc_delete(&D); T(!strcmp(D.b, "helloworld"));
    doc_clear(&D); doc_backspace(&D); doc_delete(&D); T(D.len == 0);

    /* wrap */
    doc_set(&D, "aaa bbb ccc\nddd\n\neee", 20);
    int nl = wrap_lines(D.b, D.len, 8, S, MAX_LINES);
    /* "aaa bbb " | "ccc" | "ddd" | "" | "eee" */
    T(nl == 5); T(S[0]==0 && S[1]==8 && S[2]==12 && S[3]==16 && S[4]==17);
    int r, c; cur_rc(S, nl, 8, &r, &c); T(r == 1 && c == 0);
    cur_rc(S, nl, 11, &r, &c); T(r == 1 && c == 3);
    T(line_len(D.b, D.len, S, nl, 0) == 8); T(line_len(D.b, D.len, S, nl, 2) == 3); T(line_len(D.b, D.len, S, nl, 3) == 0);
    /* trailing newline -> empty last line */
    doc_set(&D, "ab\n", 3); nl = wrap_lines(D.b, D.len, 10, S, MAX_LINES); T(nl == 2 && S[1] == 3);
    doc_set(&D, "", 0); nl = wrap_lines(D.b, D.len, 10, S, MAX_LINES); T(nl == 1);
    /* long word hard split */
    doc_set(&D, "abcdefghijkl", 12); nl = wrap_lines(D.b, D.len, 5, S, MAX_LINES); T(nl == 3 && S[1] == 5 && S[2] == 10);

    /* movement */
    doc_set(&D, "abcdef\nxy\nlonger line", 20); nl = wrap_lines(D.b, D.len, 40, S, MAX_LINES);
    D.cur = 5; D.want = -1; doc_move(&D, S, nl, 3, 5); T(D.cur == 7 + 2);    /* clamps to end of "xy" */
    doc_move(&D, S, nl, 3, 5); T(D.cur == 10 + 5);                           /* want col 5 restored */
    doc_move(&D, S, nl, 2, 5); doc_move(&D, S, nl, 2, 5); T(D.cur == 5);
    doc_move(&D, S, nl, 2, 5); T(D.cur == 0);
    doc_move(&D, S, nl, 5, 5); T(D.cur == 6); doc_move(&D, S, nl, 4, 5); T(D.cur == 0);
    doc_move(&D, S, nl, 3, 5); doc_move(&D, S, nl, 3, 5); doc_move(&D, S, nl, 3, 5); T(D.cur == D.len);

    /* sanitise */
    doc_set(&D, "a\r\nb\rc\td\x01" "e\xC3\xA4", 12);
    T(!strcmp(D.b, "a\nb\nc d?e??"));

    /* undo / redo */
    undo_reset(); doc_set(&D, "one", 3); D.cur = 3;
    undo_push(&D); doc_insert(&D, " two", 4);
    undo_push(&D); doc_insert(&D, " three", 6);
    T(!strcmp(D.b, "one two three"));
    T(undo_do(&D) && !strcmp(D.b, "one two"));
    T(undo_do(&D) && !strcmp(D.b, "one"));
    T(!undo_do(&D));
    T(redo_do(&D) && !strcmp(D.b, "one two"));
    undo_push(&D); doc_insert(&D, "!", 1); T(!redo_do(&D));
    for (int i = 0; i < 40; i++) { undo_push(&D); doc_insert(&D, "x", 1); }
    int k = 0; while (undo_do(&D)) k++; T(k == 16);

    /* limits */
    doc_clear(&D); char big[100]; memset(big, 'a', 100);
    for (int i = 0; i < 200; i++) doc_insert(&D, big, 100);
    T(D.len == NOTE_MAX && D.b[NOTE_MAX] == 0);

    /* list helpers */
    char t[48], p[48];
    note_title("\n  \n  Shopping list \nmilk\neggs", 29, t, 48); T(!strcmp(t, "Shopping list"));
    note_preview("\n  \n  Shopping list \nmilk\neggs", 29, p, 48); T(!strcmp(p, "milk"));
    note_title("", 0, t, 48); T(!strcmp(t, "New note"));
    note_preview("only one", 8, p, 48); T(!p[0]);
    T(ci_contains("Shopping List", "list") && ci_contains("x", "") && !ci_contains("abc", "abd"));
    printf(fails ? "%d FAILED\n" : "all core tests passed\n", fails);
    return fails != 0;
}
