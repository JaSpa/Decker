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

int main(int argc, char **argv) {
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

  int i = 0;
  while (i < prog->n) {
    int c = prog->sv[i];
    if (c < 0 || opcount <= c) {
      fprintf(stderr, "invalid opcode byte %x at offset %d\n", c, i);
      return 1;
    }

    int n = oplens[c];
    int missing = i + n - prog->n;
    if (missing > 0) {
      fprintf(stderr, "bad opcode: %s missing %d byte%s\n", opnames[c], missing,
              missing == 1 ? "" : "s");
      return 1;
    }

    printf("[%0.4x] %s", i++, opnames[c]);
    if (n == 3) {
      printf(" #%d", blk_gets(prog, i));
    }
    printf("\n");

    while (--n) {
      printf("[%0.4x]   0x%0.2x\n", i, (int)prog->sv[i]);
      ++i;
    }
  }
}
