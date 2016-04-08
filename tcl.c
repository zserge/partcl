#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#if 0
#define DBG printf
#else
#define DBG(...)
#endif

struct tcl;
int tcl_eval(struct tcl *tcl, const char *s, size_t len);

/* Token type and control flow constants */
enum { TCMD, TWORD, TPART, TERROR };
enum { FERROR, FNORMAL, FRETURN, FBREAK, FAGAIN };

static int tcl_is_special(char c, int q) {
  return (c == '$' || (!q && (c == '{' || c == '}' || c == ';' || c == '\r' ||
                              c == '\n')) ||
          c == '[' || c == ']' || c == '"' || c == '\0');
}

static int tcl_is_space(char c) { return (c == ' ' || c == '\t'); }

static int tcl_is_end(char c) {
  return (c == '\n' || c == '\r' || c == ';' || c == '\0');
}

int tcl_next(const char *s, size_t n, const char **from, const char **to,
             int *q) {
  int i = 0;
  int depth = 0;
  char open;
  char close;

  /* Skip leading spaces if not quoted */
  for (; !*q && n > 0 && tcl_is_space(*s); s++, n--)
    ;
  *from = s;
  /* Terminate command if not quoted */
  if (!*q && n > 0 && tcl_is_end(*s)) {
    *to = s + 1;
    return TCMD;
  }
  if (*s == '$') { /* Variable token, must not start with a space or quote */
    if (tcl_is_space(s[1]) || s[1] == '"') {
      return TERROR;
    }
    int mode = *q;
    *q = 0;
    int r = tcl_next(s + 1, n - 1, to, to, q);
    *q = mode;
    return ((r == TWORD && *q) ? TPART : r);
  } else if (*s == '[' || (!*q && *s == '{')) {
    /* Interleaving pairs are not welcome, but it simplifies the code */
    open = *s;
    close = (open == '[' ? ']' : '}');
    for (i = 0, depth = 1; i < n && depth != 0; i++) {
      if (i > 0 && s[i] == open) {
        depth++;
      } else if (s[i] == close) {
        depth--;
      }
    }
  } else if (*s == '"') {
    *q = !*q;
    *from = *to = s + 1;
    if (*q) {
      return TPART;
    } else if (n < 2 || (!tcl_is_space(s[1]) && !tcl_is_end(s[1]))) {
      return TERROR;
    } else {
      *from = *to = s + 1;
      return TWORD;
    }
  } else {
    while (i < n && (*q || !tcl_is_space(s[i])) && !tcl_is_special(s[i], *q))
      i++;
  }
  *to = s + i;
  if (i == n) {
    return TERROR;
  } else if (*q) {
    return TPART;
  } else {
    return (tcl_is_space(s[i]) || tcl_is_end(s[i])) ? TWORD : TPART;
  }
}

/* A helper parser struct and macro (requires C99) */
struct tcl_parser {
  const char *from;
  const char *to;
  const char *start;
  const char *end;
  int q;
  int token;
};
#define tcl_each(s, len, skiperr)                                              \
  for (struct tcl_parser p = {NULL, NULL, s, s + len, 0, TERROR};              \
       p.start < p.end &&                                                      \
           (((p.token = tcl_next(p.start, p.end - p.start, &p.from, &p.to,     \
                                 &p.q)) != TERROR) ||                          \
            skiperr);                                                          \
       p.start = p.to)

/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
typedef char tcl_value_t;

const char *tcl_string(tcl_value_t *v) { return (char *)v; }
int tcl_int(tcl_value_t *v) { return atoi((char *)v); }
int tcl_length(tcl_value_t *v) { return v == NULL ? 0 : strlen(v); }

void tcl_free(tcl_value_t *v) { free(v); }

tcl_value_t *tcl_append_string(tcl_value_t *v, const char *s, size_t len) {
  size_t n = tcl_length(v);
  v = realloc(v, n + len + 1);
  memset((char *)tcl_string(v) + n, 0, len + 1);
  strncpy((char *)tcl_string(v) + n, s, len);
  return v;
}

tcl_value_t *tcl_append(tcl_value_t *v, tcl_value_t *tail) {
  v = tcl_append_string(v, tcl_string(tail), tcl_length(tail));
  tcl_free(tail);
  return v;
}

tcl_value_t *tcl_alloc(const char *s, size_t len) {
  return tcl_append_string(NULL, s, len);
}

tcl_value_t *tcl_dup(tcl_value_t *v) {
  return tcl_alloc(tcl_string(v), tcl_length(v));
}

tcl_value_t *tcl_list_alloc() { return tcl_alloc("", 0); }

int tcl_list_length(tcl_value_t *v) {
  int count = 0;
  tcl_each(tcl_string(v), tcl_length(v) + 1, 0) {
    if (p.token == TWORD) {
      count++;
    }
  }
  return count;
}

void tcl_list_free(tcl_value_t *v) { free(v); }

tcl_value_t *tcl_list_at(tcl_value_t *v, int index) {
  int i = 0;
  tcl_each(tcl_string(v), tcl_length(v) + 1, 0) {
    if (p.token == TWORD) {
      if (i == index) {
        if (p.from[0] == '{') {
          return tcl_alloc(p.from + 1, p.to - p.from - 2);
        } else {
          return tcl_alloc(p.from, p.to - p.from);
        }
      }
      i++;
    }
  }
  return NULL;
}

tcl_value_t *tcl_list_append(tcl_value_t *v, tcl_value_t *tail) {
  if (tcl_length(v) > 0) {
    v = tcl_append(v, tcl_alloc(" ", 2));
  }
  if (tcl_length(tail) > 0) {
    int q = 0;
    const char *p;
    for (p = tcl_string(tail); *p; p++) {
      if (tcl_is_space(*p) || tcl_is_special(*p, 0)) {
        q = 1;
        break;
      }
    }
    if (q) {
      v = tcl_append(v, tcl_alloc("{", 1));
    }
    v = tcl_append(v, tcl_dup(tail));
    if (q) {
      v = tcl_append(v, tcl_alloc("}", 1));
    }
  } else {
    v = tcl_append(v, tcl_alloc("{}", 2));
  }
  return v;
}

/* ----------------------------- */
/* ----------------------------- */
/* ----------------------------- */
/* ----------------------------- */

typedef int (*tcl_cmd_fn_t)(struct tcl *, tcl_value_t *, void *);

struct tcl_cmd {
  const char *name;
  int arity;
  tcl_cmd_fn_t fn;
  void *arg;
  struct tcl_cmd *next;
};

struct tcl_var {
  char *name;
  tcl_value_t *value;
  struct tcl_var *next;
};

struct tcl_env {
  struct tcl_var *vars;
  struct tcl_env *parent;
};

static struct tcl_env *tcl_env_alloc(struct tcl_env *parent) {
  struct tcl_env *env = malloc(sizeof(*env));
  env->vars = NULL;
  env->parent = parent;
  return env;
}

static struct tcl_env *tcl_env_free(struct tcl_env *env) {
  struct tcl_env *parent = env->parent;
  while (env->vars) {
    struct tcl_var *var = env->vars;
    env->vars = env->vars->next;
    free(var->name);
    tcl_free(var->value);
    free(var);
  }
  free(env);
  return parent;
}

struct tcl {
  struct tcl_env *env;
  struct tcl_cmd *cmds;
  tcl_value_t *result;
};

tcl_value_t *tcl_var(struct tcl *tcl, const char *name, tcl_value_t *v) {
  DBG("var(%s := %.*s)\n", name, tcl_length(v), tcl_string(v));
  struct tcl_var *var;
  for (var = tcl->env->vars; var != NULL; var = var->next) {
    if (strcmp(var->name, name) == 0) {
      break;
    }
  }
  if (var == NULL) {
    var = malloc(sizeof(struct tcl_var));
    var->name = (char *)strdup(name);
    var->next = tcl->env->vars;
    var->value = tcl_alloc("", 0);
    tcl->env->vars = var;
  }
  if (v != NULL) {
    tcl_free(var->value);
    var->value = tcl_dup(v);
    tcl_free(v);
  }
  return var->value;
}

int tcl_result(struct tcl *tcl, int flow, tcl_value_t *result) {
  DBG("tcl_result %.*s, flow=%d\n", tcl_length(result), tcl_string(result),
      flow);
  tcl_free(tcl->result);
  tcl->result = result;
  return flow;
}

int tcl_subst(struct tcl *tcl, const char *s, size_t len) {
  DBG("subst(%.*s)\n", (int)len, s);
  if (len == 0) {
    return tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
  }
  if (s[0] == '{') {
    return tcl_result(tcl, FNORMAL, tcl_alloc(s + 1, len - 2));
  } else if (s[0] == '$') {
    char buf[256];
    sprintf(buf, "set %.*s", (int)(len - 1), s + 1);
    return tcl_eval(tcl, buf, strlen(buf) + 1);
  } else if (s[0] == '[') {
    tcl_value_t *expr = tcl_alloc(s + 1, len - 2);
    int r = tcl_eval(tcl, tcl_string(expr), tcl_length(expr) + 1);
    tcl_free(expr);
    return r;
  } else {
    return tcl_result(tcl, FNORMAL, tcl_alloc(s, len));
  }
}

int tcl_eval(struct tcl *tcl, const char *s, size_t len) {
  DBG("eval(%.*s)->\n", (int)len, s);
  tcl_value_t *list = tcl_list_alloc();
  tcl_value_t *cur = NULL;
  tcl_each(s, len, 1) {
    /*DBG("tcl_next %d %.*s\n", p.token, (int)(p.to - p.from), p.from);*/
    switch (p.token) {
    case TERROR:
      DBG("eval: FERROR, lexer error\n");
      return tcl_result(tcl, FERROR, tcl_alloc("", 0));
    case TWORD:
      DBG("token %.*s, length=%d, cur=%p (3.1.1)\n", (int)(p.to - p.from),
          p.from, (int)(p.to - p.from), cur);
      if (cur != NULL) {
        tcl_subst(tcl, p.from, p.to - p.from);
        tcl_value_t *part = tcl_dup(tcl->result);
        cur = tcl_append(cur, part);
      } else {
        tcl_subst(tcl, p.from, p.to - p.from);
        cur = tcl_dup(tcl->result);
      }
      list = tcl_list_append(list, cur);
      tcl_free(cur);
      cur = NULL;
      break;
    case TPART:
      tcl_subst(tcl, p.from, p.to - p.from);
      tcl_value_t *part = tcl_dup(tcl->result);
      cur = tcl_append(cur, part);
      break;
    case TCMD:
      if (tcl_list_length(list) == 0) {
        tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
      } else {
        tcl_value_t *cmdname  = tcl_list_at(list, 0);
        struct tcl_cmd *cmd = NULL;
        int r = FERROR;
        for (cmd = tcl->cmds; cmd != NULL; cmd = cmd->next) {
          if (strcmp(tcl_string(cmdname), cmd->name) == 0) {
            if (cmd->arity == 0 || cmd->arity == tcl_list_length(list)) {
              r = cmd->fn(tcl, list, cmd->arg);
              break;
            }
          }
        }
        tcl_free(cmdname);
        if (cmd == NULL || r != FNORMAL) {
          return r;
        }
      }
      tcl_list_free(list);
      list = tcl_list_alloc();
      break;
    }
  }
  tcl_list_free(list);
  return FNORMAL;
}

/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
void tcl_register(struct tcl *tcl, const char *name, tcl_cmd_fn_t fn, int arity,
                  void *arg) {
  struct tcl_cmd *cmd = malloc(sizeof(struct tcl_cmd));
  cmd->name = name;
  cmd->fn = fn;
  cmd->arg = arg;
  cmd->arity = arity;
  cmd->next = tcl->cmds;
  tcl->cmds = cmd;
}

static int tcl_cmd_set(struct tcl *tcl, tcl_value_t *args, void *arg) {
  tcl_value_t *var = tcl_list_at(args, 1);
  tcl_value_t *val = tcl_list_at(args, 2);
  return tcl_result(tcl, FNORMAL, tcl_dup(tcl_var(tcl, tcl_string(var), val)));
}

static int tcl_cmd_subst(struct tcl *tcl, tcl_value_t *args, void *arg) {
  tcl_value_t *s = tcl_list_at(args, 1);
  return tcl_subst(tcl, tcl_string(s), tcl_length(s));
}

static int tcl_cmd_puts(struct tcl *tcl, tcl_value_t *args, void *arg) {
  tcl_value_t *text = tcl_list_at(args, 1);
  printf("%s\n", tcl_string(text));
  return tcl_result(tcl, FNORMAL, text);
}

static int tcl_user_proc(struct tcl *tcl, tcl_value_t *args, void *arg) {
  tcl_value_t *code = (tcl_value_t *)arg;
  tcl_value_t *params = tcl_list_at(code, 2);
  tcl_value_t *body = tcl_list_at(code, 3);
  int i;
  tcl->env = tcl_env_alloc(tcl->env);
  for (i = 0; i < tcl_list_length(params); i++) {
    tcl_value_t *param = tcl_list_at(params, i);
    tcl_value_t *v = tcl_list_at(args, i + 1);
    tcl_var(tcl, tcl_string(param), v);
  }
  i = tcl_eval(tcl, tcl_string(body), tcl_length(body) + 1);
  tcl->env = tcl_env_free(tcl->env);
  tcl_free(params);
  tcl_free(body);
  return FNORMAL;
}

static int tcl_cmd_proc(struct tcl *tcl, tcl_value_t *args, void *arg) {
  tcl_register(tcl, tcl_string(tcl_list_at(args, 1)), tcl_user_proc, 0,
      tcl_dup(args));
  return tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
}

static int tcl_cmd_if(struct tcl *tcl, tcl_value_t *args, void *arg) {
  int i = 1;
  int n = tcl_list_length(args);
  int r = FNORMAL;
  while (i < n) {
    tcl_value_t *cond = tcl_list_at(args, i);
    tcl_value_t *branch = NULL;
    if (i + 1 < n) {
      branch = tcl_list_at(args, i + 1);
    }
    r = tcl_eval(tcl, tcl_string(cond), tcl_length(cond) + 1);
    tcl_free(cond);
    if (r != FNORMAL) {
      tcl_free(branch);
      break;
    }
    if (tcl_int(tcl->result)) {
      r = tcl_eval(tcl, tcl_string(branch), tcl_length(branch) + 1);
      tcl_free(branch);
      break;
    }
    i = i + 2;
    tcl_free(branch);
  }
  return r;
}

static int tcl_cmd_flow(struct tcl *tcl, tcl_value_t *args, void *arg) {
  const char *flow = tcl_string(tcl_list_at(args, 0));
  if (strcmp(flow, "break") == 0) {
    return FBREAK;
  } else if (strcmp(flow, "continue") == 0) {
    return FAGAIN;
  } else if (strcmp(flow, "return") == 0) {
    return tcl_result(tcl, FRETURN, tcl_list_at(args, 1));
  }
  return FERROR;
}

static int tcl_cmd_while(struct tcl *tcl, tcl_value_t *args, void *arg) {
  const char *cond = tcl_string(tcl_list_at(args, 1));
  const char *loop = tcl_string(tcl_list_at(args, 2));
  int r;
  for (;;) {
    r = tcl_eval(tcl, cond, strlen(cond) + 1);
    if (r != FNORMAL) {
      return r;
    }
    if (!tcl_int(tcl->result)) {
      return FNORMAL;
    }
    int r = tcl_eval(tcl, loop, strlen(loop) + 1);
    switch (r) {
    case FBREAK:
      return FNORMAL;
    case FRETURN:
      return FRETURN;
    case FAGAIN:
      continue;
    case FERROR:
      return FERROR;
    }
  }
}

static int tcl_cmd_math(struct tcl *tcl, tcl_value_t *args, void *arg) {
  char buf[64];
  tcl_value_t *opval = tcl_list_at(args, 0);
  tcl_value_t *aval = tcl_list_at(args, 1);
  tcl_value_t *bval = tcl_list_at(args, 2);
  const char *op = tcl_string(opval);
  int a = tcl_int(aval);
  int b = tcl_int(bval);
  int c = 0;
  if (op[0] == '+')
    c = a + b;
  else if (op[0] == '-')
    c = a - b;
  else if (op[0] == '*')
    c = a * b;
  else if (op[0] == '/')
    c = a / b;
  else if (op[0] == '>' && op[1] == '\0')
    c = a > b;
  else if (op[0] == '>' && op[1] == '=')
    c = a >= b;
  else if (op[0] == '<' && op[1] == '\0')
    c = a < b;
  else if (op[0] == '<' && op[1] == '=')
    c = a <= b;
  else if (op[0] == '=' && op[1] == '=')
    c = a == b;
  else if (op[0] == '!' && op[1] == '=')
    c = a != b;
  snprintf(buf, 64, "%d", c);
  tcl_free(opval);
  tcl_free(aval);
  tcl_free(bval);
  return tcl_result(tcl, FNORMAL, tcl_alloc(buf, strlen(buf)));
}

struct tcl *tcl_create() {
  int i;
  struct tcl *tcl = (struct tcl *)malloc(sizeof(struct tcl));
  tcl->env = tcl_env_alloc(NULL);
  tcl->result = tcl_alloc("", 0);
  tcl->cmds = NULL;
  tcl_register(tcl, "set", tcl_cmd_set, 0, NULL);
  tcl_register(tcl, "subst", tcl_cmd_subst, 2, NULL);
  tcl_register(tcl, "puts", tcl_cmd_puts, 2, NULL);
  tcl_register(tcl, "proc", tcl_cmd_proc, 4, NULL);
  tcl_register(tcl, "if", tcl_cmd_if, 0, NULL);
  tcl_register(tcl, "while", tcl_cmd_while, 3, NULL);
  tcl_register(tcl, "return", tcl_cmd_flow, 0, NULL);
  tcl_register(tcl, "break", tcl_cmd_flow, 1, NULL);
  tcl_register(tcl, "continue", tcl_cmd_flow, 1, NULL);
  char *math[] = {"+", "-", "*", "/", ">", ">=", "<", "<=", "==", "!="};
  for (i = 0; i < (sizeof(math) / sizeof(math[0])); i++) {
    tcl_register(tcl, math[i], tcl_cmd_math, 3, NULL);
  }
  return tcl;
}

void tcl_destroy(struct tcl *tcl) {
  while (tcl->env) {
    tcl->env = tcl_env_free(tcl->env);
  }
  while (tcl->cmds) {
    struct tcl_cmd *cmd = tcl->cmds;
    tcl->cmds = tcl->cmds->next;
    free(cmd->arg);
    free(cmd);
  }
  tcl_free(tcl->result);
  free(tcl);
}

#ifndef TEST
int main() {
  char buf[1024] = {0};
  int i = 0;
  struct tcl *tcl = tcl_create();
  while (1) {
  next:
    buf[i++] = fgetc(stdin);
    tcl_each(buf, i, 1) {
      if (p.token == TERROR && (p.to - buf) != i) {
        memset(buf, 0, sizeof(buf));
        i = 0;
      } else if (p.token == TCMD && *(p.from) != '\0') {
        int r = tcl_eval(tcl, buf, strlen(buf));
        if (r != FERROR) {
          printf("result> %.*s\n", tcl_length(tcl->result),
                 tcl_string(tcl->result));
        } else {
          printf("?!\n");
        }

        memset(buf, 0, sizeof(buf));
        i = 0;
        goto next;
      }
    }
  }
  return 0;
}
#endif
