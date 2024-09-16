#include "lil.h"

#include "dom.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RT_DUMMY                                                               \
  fprintf(stderr, "FATAL ERROR: runtime dummy %s invoked\n",                   \
          __PRETTY_FUNCTION__);                                                \
  abort();

void field_notify(lv *field) {
  (void)field;
  RT_DUMMY
}
void go_notify(lv *deck, lv *args, int dest) {
  (void)deck, (void)args, (void)dest;
  RT_DUMMY
}
lv *interface_app(lv *self, lv *i, lv *x) {
  (void)self, (void)i, (void)x;
  RT_DUMMY
}
lv *n_alert(lv *self, lv *z) {
  (void)self, (void)z;
  RT_DUMMY
}
lv *n_open(lv *self, lv *z) {
  (void)self, (void)z;
  RT_DUMMY
}
lv *n_panic(lv *self, lv *z) {
  (void)self, (void)z;
  RT_DUMMY
}
lv *n_play(lv *self, lv *z) {
  (void)self, (void)z;
  RT_DUMMY
}
lv *n_print(lv *self, lv *a) {
  (void)self, (void)a;
  RT_DUMMY
}
lv *n_readfile(lv *self, lv *a) {
  (void)self, (void)a;
  RT_DUMMY
}
lv *n_save(lv *self, lv *z) {
  (void)self, (void)z;
  RT_DUMMY
}
lv *n_show(lv *self, lv *a) {
  (void)self, (void)a;
  RT_DUMMY
}

static const char *EXE;

void usage(void) { fprintf(stderr, "usage: %s [-e EXPR | FILE]\n", EXE); }

int read_file(FILE *in, str *s) {
  int ch;
  while ((ch = fgetc(in)) != EOF) {
    str_addraw(s, ch);
  }
  return feof(in);
}

#define INBOUNDS(a, n) (0 <= n && (size_t)n < (sizeof a / sizeof *a))

#define C_RST ""         // reset
#define C_OPC "1;7"      // bold
#define C_ARG "38;5;251" // white
#define C_LIT "32"       // green
#define C_OPN "35"       // magenta

static int DO_COLORS;
#define C(name) (DO_COLORS ? "\033[" C_##name "m" : "")

const char *fmt_imm(int op, lv *p, int imm) {
#define PREP_BUFFER                                                            \
  do {                                                                         \
    if (!buf.sv)                                                               \
      buf = str_new();                                                         \
    else                                                                       \
      buf.c = 0;                                                               \
  } while (0)
#define BUF_ADDRAW(s)                                                          \
  do {                                                                         \
    for (const char *p = (s); p && *p; ++p) {                                  \
      str_addraw(&buf, *p);                                                    \
    }                                                                          \
  } while (0)
#define PRETTY_OP(n, opdefs)                                                   \
  {                                                                            \
    if (!INBOUNDS(opdefs, imm))                                                \
      return "??";                                                             \
    PREP_BUFFER;                                                               \
    BUF_ADDRAW(C(OPN));                                                        \
    str_addc(&buf, '`');                                                       \
    str_addz(&buf, opdefs[imm].name);                                          \
    str_addc(&buf, '`');                                                       \
    BUF_ADDRAW(C(RST));                                                        \
    break;                                                                     \
  }

  static str buf;
  switch (op) {
  case LIT: {
    PREP_BUFFER;
    BUF_ADDRAW(C(LIT));
    show(&buf, blk_getimm(p, imm), 1);
    BUF_ADDRAW(C(RST));
    break;
  }

  case OP1:
    PRETTY_OP(1, monads);
  case OP2:
    PRETTY_OP(2, dyads);
  case OP3:
    PRETTY_OP(3, triads);

  case BUND:
  case GET:
  case SET:
    return 0;

  default:
    return "??";
  }

  str_term(&buf);
  return buf.sv;

#undef PREP_BUFFER
#undef PRETTY_OP
}

int main(int argc, char **argv) {
  DO_COLORS = isatty(STDOUT_FILENO);
  EXE = argc ? argv[0] : "<exe>";

  int ch;
  char *expr = 0;

  while ((ch = getopt(argc, argv, "he:")) != -1) {
    switch (ch) {
    case 'e':
      expr = optarg;
      break;
    case 'h':
    default:
      return ch != 'h';
    }
  }

  argc -= optind;
  argv += optind;

  if (expr && argc) {
    fprintf(stderr, "%s: expected `-e EXPR` OR `FILE` argument\n", EXE);
    return 1;
  }

  if (argc > 1) {
    fprintf(stderr, "%s: expected only one `FILE` argument\n", EXE);
    return 1;
  }

  init_interns();
  str src_str = {0};

  if (argc) {
    const char *filepath = argv[0];
    FILE *file = fopen(filepath, "r");
    if (!file) {
      fprintf(stderr, "%s: failed to open '%s' (%s)\n", EXE, filepath,
              strerror(errno));
      return 1;
    }
    src_str = str_new();
    if (!read_file(file, &src_str)) {
      fprintf(stderr, "%s: failed to read from '%s' (%s)\n", EXE, filepath,
              strerror(errno));
      return 1;
    }
    fclose(file);
  } else if (!expr) {
    src_str = str_new();
    if (!read_file(stdin, &src_str)) {
      fprintf(stderr, "%s: failed to read from stdin (%s)\n", EXE,
              strerror(errno));
      return 1;
    }
  }

  if (src_str.sv) {
    str_term(&src_str);
  }

  lv *prog = parse(expr ? expr : src_str.sv);
  if (perr()) {
    fprintf(stderr, "error:%s:%d:%d: %s\n", argc ? argv[0] : "stdin", par.r + 1,
            par.c + 1, par.error);
    return 1;
  }

  static const char *const opnames[] = {
      "JUMP", "JUMPF", "LIT",  "DUP", "DROP", "SWAP",  "OVER", "BUND", "OP1",
      "OP2",  "OP3",   "GET",  "SET", "LOC",  "AMEND", "TAIL", "CALL", "BIND",
      "ITER", "EACH",  "NEXT", "COL", "IPRE", "IPOST", "FIDX", "FMAP",
  };
  const int opcount = (int)(sizeof opnames / sizeof *opnames);

  for (int width, pc = 0; pc < prog->n; pc += width) {
    // Sanity check that it is valid to index `opnames` & `oplens`.
    int op = prog->sv[pc];
    if (op < 0 || opcount <= op) {
      fprintf(stderr, "invalid opcode byte %x at offset %d\n", op, pc);
      return 1;
    }

    // Sanity check that we will not read outside the program buffer when
    // decoding this instruction.
    width = oplens[op];
    int missing = pc + width - prog->n;
    if (missing > 0) {
      fprintf(stderr, "bad opcode: %s missing %d byte%s\n", opnames[op],
              missing, missing == 1 ? "" : "s");
      return 1;
    }

    int imm = width == 3 ? blk_gets(prog, pc + 1) : -1;
    printf("%s[%0.4x] %s", C(OPC), pc, opnames[op]);
    if (imm != -1) {
      const char *pretty = fmt_imm(op, prog, imm);
      printf(" (#%d)%s%s%s", imm, C(RST), pretty ? "  " : "",
             pretty ? pretty : "");
    }
    printf("%s\n", C(RST));

    for (int i = 1; i < width; ++i) {
      printf("%s[%0.4x]   0x%0.2x%s\n", C(ARG), pc + i, (int)prog->sv[pc + i],
             C(RST));
    }
  }
}
