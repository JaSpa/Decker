#include "lil.h"

#include "dom.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_va_list.h>
#include <sys/cdefs.h>
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

int read_file(FILE *in, str *s) {
  int ch;
  while ((ch = fgetc(in)) != EOF) {
    str_addraw(s, ch);
  }
  return feof(in);
}

#define INBOUNDS(a, n) (0 <= n && (size_t)n < (sizeof a / sizeof *a))

static struct {
  uint8_t verbose : 1;
  uint8_t colorized : 1;
} OUT_FMT;

#define C_RST ""         // reset
#define C_OPC "1;7"      // bold, reverse
#define C_ARG "38;5;251" // white
#define C_LIT "32"       // green
#define C_OPN "35"       // magenta
#define C_ERR "1;31"     // bold, red

#define C(name) (OUT_FMT.colorized ? "\033[" C_##name "m" : "")

__dead2 __printflike(1, 2) void err_exit(const char *fmt, ...) {
  fprintf(stderr, "%serror:%s ", C(ERR), C(RST));
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);
  exit(1);
}

const char *fmt_imm(int op, lv *p, int imm, int *len) {
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
    if (len)                                                                   \
      *len += strlen(opdefs[imm].name) + 2;                                    \
    break;                                                                     \
  }

  static str buf;
  switch (op) {
  case LIT: {
    PREP_BUFFER;
    BUF_ADDRAW(C(LIT));
    int c_pre = buf.c;
    show(&buf, blk_getimm(p, imm), 1);
    int c_post = buf.c;
    BUF_ADDRAW(C(RST));
    if (len)
      *len += c_post - c_pre;
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

  default: {
    const char *s = "??";
    if (len)
      *len += strlen(s);
    return s;
  }
  }

  str_term(&buf);
  return buf.sv;

#undef PREP_BUFFER
#undef PRETTY_OP
}

void pretty_block(lv *b, lv *d_fns) {
  const char *const opnames[] = {
      "JUMP", "JUMPF", "LIT",  "DUP", "DROP", "SWAP",  "OVER", "BUND", "OP1",
      "OP2",  "OP3",   "GET",  "SET", "LOC",  "AMEND", "TAIL", "CALL", "BIND",
      "ITER", "EACH",  "NEXT", "COL", "IPRE", "IPOST", "FIDX", "FMAP",
  };
  const int opcount = (int)(sizeof opnames / sizeof *opnames);

  for (int width, pc = 0; pc < b->n; pc += width) {
    // Sanity check that it is valid to index `opnames` & `oplens`.
    int op = b->sv[pc];
    if (op < 0 || opcount <= op) {
      err_exit("invalid opcode byte %x at offset %d\n", op, pc);
    }

    // Sanity check that we will not read outside the program buffer when
    // decoding this instruction.
    width = oplens[op];
    int missing = pc + width - b->n;
    if (missing > 0) {
      err_exit("bad opcode: %s missing %d byte%s\n", opnames[op], missing,
               missing == 1 ? "" : "s");
    }

    int imm = width == 3 ? blk_gets(b, pc + 1) : -1;
    if (op == LIT) {
      lv *lit = blk_getimm(b, imm);
      if (lion(lit)) {
        dset(d_fns, lit, ONE);
      }
    }

    int fmtlen = 0;
    const char *c_opc = OUT_FMT.verbose ? C(OPC) : "";
    printf("%s", c_opc);
    fmtlen += printf("[%0.4x] %s", pc, opnames[op]);
    if (imm != -1) {
      const char *pretty = fmt_imm(op, b, imm, &fmtlen);
      fmtlen += printf(" (#%d)", imm);
      // printf("%s", C(RST));
      printf("%s%s", pretty ? "  " : "", pretty ? pretty : "");
      fmtlen += pretty ? 2 : 0;
    }
    if (fmtlen < 80 && OUT_FMT.colorized && OUT_FMT.verbose)
      printf("%s%*s", c_opc, 80 - fmtlen, "");
    printf("%s\n", C(RST));

    if (OUT_FMT.verbose) {
      for (int i = 1; i < width; ++i) {
        printf("%s[%0.4x]   0x%0.2x%s\n", C(ARG), pc + i, (int)b->sv[pc + i],
               C(RST));
      }
    }
  }
}

static const char *EXE;

void usage(void) { fprintf(stderr, "usage: %s [-pv] [-e EXPR | FILE]\n", EXE); }

int main(int argc, char **argv) {
  EXE = argc ? argv[0] : "<exe>";

  OUT_FMT.colorized = isatty(STDOUT_FILENO);

  int ch;
  char *expr = 0;

  while ((ch = getopt(argc, argv, "e:hpv")) != -1) {
    switch (ch) {
    case 'e':
      expr = optarg;
      break;
    case 'p':
      OUT_FMT.colorized = 0;
      break;
    case 'v':
      OUT_FMT.verbose = 1;
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
    err_exit("%s:%d:%d: %s", argc ? argv[0] : "«stdin»", par.r + 1, par.c + 1,
             par.error);
  }

  lv *ref_fns = lmd();
  pretty_block(prog, ref_fns);

  EACH(i, ref_fns) {
    lv *fn = ref_fns->kv[i];
    printf("\n«%s»\n", fn->sv);
    pretty_block(fn->b, ref_fns);
  }
}
