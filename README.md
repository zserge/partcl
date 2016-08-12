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

## Language syntax

Tcl script is made up of _commands_ separated by semicolons or newline
symbols. Commnads in their turn are made up of _words_ separated by whitespace.
To make whitespace a part of the word one may use double quotes or braces.

An important part of the language is _command substitution_, when the result of
a command inside square braces is returned as a part of the outer command, e.g.
`puts [+ 1 2]`.

The only data type of the language is a string. Although it may complicate
mathematical operations, it opens a broad way for building your own DSLs to
enhance the language.

## Lexer

Any symbol can be part of the word, except for the following special symbols:

* whitespace, tab - used to delimit words
* `\r`, `\n`, semicolon or EOF - used to delimit commands
* Braces, square brackets, dollar sign - used for substitution and grouping

Partcl has special helper functions for these char classes:

```
static int tcl_is_space(char c);
static int tcl_is_end(char c);
static int tcl_is_special(char c, int q);
```

`tcl_is_special` behaves differently depending on the quoting mode (`q`
parameter). Inside a quoted string braces, semicolon and end-of-line symbols
lose their special meaning and become regular printable characters.

Partcl lexer is implemented in one function:

```
int tcl_next(const char *s, size_t n, const char **from, const char **to, int *q);
```

`tcl_next` function finds the next token in the string `s`. `from` and `to` are
set to point to the token start/end, `q` denotes the quoting mode and is
changed if `"` is met.

A special macro `tcl_each(s, len, skip_error)` can used to iterate over all the
tokens in the string. If `skip_error` is false - loop ends when string ends,
otherwise loop can end earlier if a syntax error is found. It allows to
"validate" input string without evaluating it and detect when a full command
has been read.

## Data types

Tcl uses strings as a primary data type. When Tcl script is evaluated, many of
the strings are created, disposed or modified. In embedded systems memory
management can be complex, so all operations with Tcl values are moved into
isolated functions that can be easily rewritten to optimize certain parts (e.g.
to use a pool of strings, a custom memory allocator, cache numerical or list
values to increase performance etc).

```
/* Raw string values */
tcl_value_t *tcl_alloc(const char *s, size_t len);
tcl_value_t *tcl_dup(tcl_value_t *v);
tcl_value_t *tcl_append(tcl_value_t *v, tcl_value_t *tail);
int tcl_length(tcl_value_t *v);
void tcl_free(tcl_value_t *v);

/* Helpers to access raw string or numeric value */
int tcl_int(tcl_value_t *v);
const char *tcl_string(tcl_value_t *v);

/* List values */
tcl_value_t *tcl_list_alloc();
tcl_value_t *tcl_list_append(tcl_value_t *v, tcl_value_t *tail);
tcl_value_t *tcl_list_at(tcl_value_t *v, int index);
int tcl_list_length(tcl_value_t *v);
void tcl_list_free(tcl_value_t *v);
```

Keep in mind, that `..._append()` functions must free the tail argument.
Also, the string returned by `tcl_string()` it not meant to be mutated or
cached.

In the default implementation lists are implemented as raw strings that add
some escaping (braces) around each iterm. It's a simple solution that also
reduces the code, but in some exotic cases the escaping can become wrong and
invalid results will be returned.

## Environments

A special type, `struct tcl_env` is used to keep the evaluation environment (a
set of functions). The interpreter creates a new environment for each
user-defined procedure, also there is one global environment per interpreter.

There are only 3 functions related to the environment. One creates a new environment, another seeks for a variable (or creates a new one), the last one destroys the environment and all its variables.

These functions use malloc/free, but can easily be rewritten to use memory pools instead.

```
static struct tcl_env *tcl_env_alloc(struct tcl_env *parent);
static struct tcl_var *tcl_env_var(struct tcl_env *env, tcl_value_t *name);
static struct tcl_env *tcl_env_free(struct tcl_env *env);
```

Variables are implemented as a single-linked list, each variable is a pair of
values (name + value) and a pointer to the next variable.

## Interpreter

Partcl interpreter is a simple structure `struct tcl` which keeps the current
environment, array of available commands and a last result value.

Interpreter logic is wrapped around two functions - evaluation and
substitution.

Substitution:

- If argument starts with `$` - create a temporary command `[set name]` and
  evaluate it. In Tcl `$foo` is just a shortcut to `[set foo]`, which returns
  the value of "foo" variable in the current environment.
- If argument starts with `[` - evaluate what's inside the square brackets and
  return the result.
- If argument is a quoted string (e.g. `{foo bar}`) - return it as is, just
  without braces.
- Otherwise return the argument as is.

Evaluation:

- Iterates over each token in a list
- Appends words into a list
- If the command end is met (semicolor, or newline, or end-of-file - our lexer
  has a special token type `TCMD` for them) - then find a suitable command (the
  first word in the list) and call it.

Where the commands are taken from? Initially, a Partcl interpeter starts with
no commands, but one may add the commands by calling `tcl_register()`.

Each command has a name, arity (how many arguments is shall take - interpreter
checks it before calling the command, use zero arity for varargs) and a C
function pointer that actually implements the command.

## Builtin commands

"set" - `tcl_cmd_set`, assigns value to the variable (if any) and returns the
current variable value.

"subst" - `tcl_cmd_subst`, does command substitution in the argument string.

"puts" - `tcl_cmd_puts`, prints argument to the stdout, followed by a newline.
This command can be disabled using `#define TCL_DISABLE_PUTS`, which is handy
for embedded systems that don't have "stdout".

"proc" - `tcl_cmd_proc`, creates a new command appending it to the list of
current interpreter commands. That's how user-defined commands are built.

"if" - `tcl_cmd_if`, does a simple `if {cond} {then} {cond2} {then2} {else}`.

"while" - `tcl_cmd_while`, runs a while loop `while {cond} {body}`. One may use
"break", "continue" or "return" inside the loop to contol the flow.

Various math operations are implemented as `tcl_cmd_math`, but can be disabled,
too if your script doesn't need them (if you want to use Partcl as a command
shell, not as a programming language).

## Building and testing

All sources are in one file, `tcl.c`. It can be used as a standalone
interpreter, or included as a single-file library (you may want to rename it
into tcl.h then).

Tests are run with clang and coverage is calculated. Just run "make test" and
you're done.

Code is formatted using clang-format to keep the clean and readable coding
style. Please run it for pull requests, too.

## License

Code is distributed under MIT license, feel free to use it in your proprietary
projects as well.


