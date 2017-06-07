#ifndef TCL_TEST_LEXER_H
#define TCL_TEST_LEXER_H

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static void va_check_tokens(const char *s, size_t len, int count, va_list ap) {
  int j = 0;
  tcl_each(s, len, 1) {
    int type = va_arg(ap, int);
    char *token = va_arg(ap, char *);
    j++;
    if (p.token != type) {
      FAIL("Expected token #%d type %d, but found %d (%.*s)\n", j, type,
           p.token, (int)len, s);
    } else if (p.token == TERROR) {
      break;
    } else {
      if ((p.token == TPART || p.token == TWORD) &&
          (strlen(token) != p.to - p.from ||
           strncmp(p.from, token, p.to - p.from) != 0)) {
        FAIL("Expected %s, but found %.*s (%s)\n", token, (int)(p.to - p.from),
             p.from, s);
      }
    }
  }
  if (j != count) {
    FAIL("Expected %d tokens, but found %d (%s)\n", count, j, s);
  } else {
    printf("OK: %.*s\n", (int)len, s);
  }
}

static void check_tokens(const char *s, int count, ...) {
  va_list ap;
  va_start(ap, count);
  va_check_tokens(s, strlen(s) + 1, count, ap);
  va_end(ap);
}

static void check_tokens_len(const char *s, size_t len, int count, ...) {
  va_list ap;
  va_start(ap, count);
  va_check_tokens(s, len, count, ap);
  va_end(ap);
}

static void test_lexer() {
  printf("\n");
  printf("###################\n");
  printf("### LEXER TESTS ###\n");
  printf("###################\n");
  printf("\n");

  /* Empty */
  check_tokens("", 1, TCMD, "");
  check_tokens(";", 2, TCMD, ";", TCMD, "");
  check_tokens(";;;  ;", 5, TCMD, ";", TCMD, ";", TCMD, ";", TCMD, ";", TCMD,
               "");
  /* Regular words */
  check_tokens("foo", 2, TWORD, "foo", TCMD, "");
  check_tokens("foo bar", 3, TWORD, "foo", TWORD, "bar", TCMD, "");
  check_tokens("foo bar baz", 4, TWORD, "foo", TWORD, "bar", TWORD, "baz", TCMD,
               "");
  /* Grouping */
  check_tokens("foo {bar baz}", 3, TWORD, "foo", TWORD, "{bar baz}", TCMD, "");
  check_tokens("foo {bar {baz} {q u x}}", 3, TWORD, "foo", TWORD,
               "{bar {baz} {q u x}}", TCMD, "");
  check_tokens("foo {bar {baz} [q u x]}", 3, TWORD, "foo", TWORD,
               "{bar {baz} [q u x]}", TCMD, "");
  check_tokens("foo {bar $baz [q u x]}", 3, TWORD, "foo", TWORD,
               "{bar $baz [q u x]}", TCMD, "");
  check_tokens("foo {bar \" baz}", 3, TWORD, "foo", TWORD, "{bar \" baz}", TCMD,
               "");
  check_tokens("foo {\n\tbar\n}", 3, TWORD, "foo", TWORD, "{\n\tbar\n}", TCMD,
               "");
  /* Substitution */
  check_tokens("foo [bar baz]", 3, TWORD, "foo", TWORD, "[bar baz]", TCMD, "");
  check_tokens("foo [bar {baz}]", 3, TWORD, "foo", TWORD, "[bar {baz}]", TCMD,
               "");
  check_tokens("foo $bar $baz", 4, TWORD, "foo", TWORD, "$bar", TWORD, "$baz",
               TCMD, "");
  check_tokens("foo $bar$baz", 4, TWORD, "foo", TPART, "$bar", TWORD, "$baz",
               TCMD, "");
  check_tokens("foo ${bar baz}", 3, TWORD, "foo", TWORD, "${bar baz}", TCMD,
               "");
  check_tokens("puts hello[\n]world", 5, TWORD, "puts", TPART, "hello", TPART,
               "[\n]", TWORD, "world", TCMD, "");
  /* Quotes */
  check_tokens("\"\"", 3, TPART, "", TWORD, "", TCMD, "");
  check_tokens("\"\"\"\"", 2, TPART, "", TERROR, "");
  check_tokens("foo \"bar baz\"", 5, TWORD, "foo", TPART, "", TPART, "bar baz",
               TWORD, "", TCMD, "");
  check_tokens("foo \"bar $b[a z]\" qux", 8, TWORD, "foo", TPART, "", TPART,
               "bar ", TPART, "$b", TPART, "[a z]", TWORD, "", TWORD, "qux",
               TCMD, "");
  check_tokens("foo \"bar baz\" \"qux quz\"", 8, TWORD, "foo", TPART, "", TPART,
               "bar baz", TWORD, "", TPART, "", TPART, "qux quz", TWORD, "",
               TCMD, "");
  check_tokens("\"{\" \"$a$b\"", 8, TPART, "", TPART, "{", TWORD, "", TPART, "",
               TPART, "$a", TPART, "$b", TWORD, "", TCMD, "");

  check_tokens("\"{\" \"$a\"$b", 6, TPART, "", TPART, "{", TWORD, "", TPART, "",
               TPART, "$a", TERROR, "");
  check_tokens("\"$a + $a = ?\"", 7, TPART, "", TPART, "$a", TPART, " + ",
               TPART, "$a", TPART, " = ?", TWORD, "", TCMD, "");
  /* Variables */
  check_tokens("puts $ a", 2, TWORD, "puts", TERROR, "");
  check_tokens("puts $\"a b\"", 2, TWORD, "puts", TERROR, "");
  check_tokens("puts $$foo", 3, TWORD, "puts", TWORD, "$$foo", TCMD, "");
  check_tokens("puts ${a b}", 3, TWORD, "puts", TWORD, "${a b}", TCMD, "");
  check_tokens("puts $[a b]", 3, TWORD, "puts", TWORD, "$[a b]", TCMD, "");
  check_tokens("puts { ", 2, TWORD, "puts", TERROR, "");
  check_tokens("set a {\n", 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens("puts {[}", 3, TWORD, "puts", TWORD, "{[}", TCMD, "");
  check_tokens("puts [{]", 3, TWORD, "puts", TWORD, "[{]", TCMD, "");
  check_tokens("puts {[}{]} ", 4, TWORD, "puts", TPART, "{[}", TWORD, "{]}",
               TCMD, "");

  /* Strings without trailing zero */
  check_tokens_len("abc foo", 1, 1, TERROR, "a");
  check_tokens_len("abc foo", 2, 1, TERROR, "a");
  check_tokens_len("abc foo", 3, 1, TERROR, "a");
  check_tokens_len("abc foo", 4, 2, TWORD, "abc", TERROR, "");
  check_tokens_len("abc foo", 7, 2, TWORD, "abc", TERROR, "");
  check_tokens_len("abc foo", 8, 3, TWORD, "abc", TWORD, "foo", TCMD, "");
  check_tokens_len("s", 1, 1, TERROR, "s");
  check_tokens_len("se", 2, 1, TERROR, "s");
  check_tokens_len("set", 3, 1, TERROR, "s");
  check_tokens_len("set ", 4, 2, TWORD, "set", TERROR, "");
  check_tokens_len("set a", 5, 2, TWORD, "set", TERROR, "");
  check_tokens_len("set a ", 6, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {", 7, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {\n", 8, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {\nh", 9, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {\nhe", 10, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {\nhel", 11, 3, TWORD, "set", TWORD, "a", TERROR, "");
  check_tokens_len("set a {\nhell", 12, 3, TWORD, "set", TWORD, "a", TERROR,
                   "");
  check_tokens_len("set a {\nhello", 13, 3, TWORD, "set", TWORD, "a", TERROR,
                   "");
  check_tokens_len("set a {\nhello\n", 14, 3, TWORD, "set", TWORD, "a", TERROR,
                   "");
  check_tokens_len("set a {\nhello\n}", 15, 3, TWORD, "set", TWORD, "a", TERROR,
                   "");
  check_tokens_len("set a {\nhello\n}\n", 16, 4, TWORD, "set", TWORD, "a",
                   TWORD, "{\nhello\n}", TCMD, "");
}

#endif /* TCL_TEST_LEXER_H */
