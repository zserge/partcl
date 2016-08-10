# Partcl - a minimal Tcl interpreter

[![Build Status](https://travis-ci.org/zserge/partcl.svg?branch=master)](https://travis-ci.org/zserge/partcl)

## Features

* ~600 lines of "pedantic" C99 code
* No external dependencies
* Good test coverage
* Can be extended with custom Tcl commands
* Runs well on bare metal embedded MCUs (~10k of flash is required)

Built-in commands:

* `subst arg`
* `set var ?val?`
* `while cond loop`
* `if cond branch ?cond? ?branch? ?other?`
* `proc name args body`
* `return`
* `break`
* `continue`
* arithmetic operations: `+, -, *, /, <, >, <=, >=, ==, !=`

## Usage

```c
struct tcl tcl;
const char *s = "set x 4; puts [+ [* $x 10] 2]";

tcl_init(&tcl);
if (tcl_eval(&tcl, s, strlen(s)) != FERROR) {
    printf("%.*s\n", tcl_length(tcl.result), tcl_string(tcl.result));
}
tcl_destroy(&tcl);
```

## License

Code is distributed under MIT license, feel free to use it in your proprietary
projects as well.


