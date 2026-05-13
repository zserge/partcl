#include <stdio.h>

#define TEST
#include "tcl.c"

int status = 0;
#define FAIL(...)                   \
  do {                              \
    printf("FAILED: " __VA_ARGS__); \
    status = 1;                     \
  } while (0)

// clang-format off
#include "tcl_test_lexer.h"
#include "tcl_test_subst.h"
#include "tcl_test_flow.h"
#include "tcl_test_math.h"
// clang-format on

int main(void) {
  test_lexer();
  test_subst();
  test_flow();
  test_math();
  return status;
}
