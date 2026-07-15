#define _POSIX_C_SOURCE 199309L

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_WORD_LEN 8
#define MAX_TOKENS 200
#define MAX_LETTERS 10
#define MAX_INPUTS 512
#define MAX_SEGMENTS 10
#define MAX_ADDENDS 32

typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } Op;
typedef enum { TK_WORD, TK_OP, TK_EQ, TK_END } TKind;

typedef struct {
  TKind kind;
  Op op;
  char word[MAX_WORD_LEN + 1];
} Token;

#define MAX_COL_LETTERS (MAX_ADDENDS + 1)

typedef struct {
  int letter_idx[MAX_COL_LETTERS];
  int coeff[MAX_COL_LETTERS];
  int n;

  int new_idx[MAX_LETTERS];
  int n_new;
} ColDesc;

typedef struct {
  Token tokens[MAX_TOKENS];
  int ntokens;
  int cur;
  char letters[MAX_LETTERS];
  int nletters;

  int digit[26];
  int used[10];
  int leading[26];

  int nsolutions;

  int is_addition;
  ColDesc cols[MAX_WORD_LEN + 2];
  int ncols;
  int result_len;

  char addends[MAX_ADDENDS][MAX_WORD_LEN + 1];
  int nadd;
  char result[MAX_WORD_LEN + 1];

  int is_longmul;
  char lm_multiplicand[MAX_WORD_LEN + 1];
  char lm_multiplier[MAX_WORD_LEN + 1];
  char lm_partials[MAX_WORD_LEN][MAX_WORD_LEN + 1];
  int lm_npartials;
  char lm_product[MAX_WORD_LEN + 1];
} Solver;

static Solver S;

static void ERR(const char *msg) {
  fprintf(stderr, "\n  [Error] %s\n", msg);
  exit(EXIT_FAILURE);
}

static int letter_index(char c) {
  for (int i = 0; i < S.nletters; i++)
    if (S.letters[i] == c)
      return i;
  return -1;
}

static void register_letter(char c) {
  if (letter_index(c) >= 0)
    return;
  if (S.nletters >= MAX_LETTERS)
    ERR("more than 10 distinct letters — cannot assign unique digits 0-9.");
  S.letters[S.nletters++] = c;
}

static void normalize_input(char *s) {
  int n = (int)strlen(s);
  for (int i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!isalpha(c))
      continue;

    if (c == 'x' || c == 'X') {
      int prev_is_alpha = (i > 0) && isalpha((unsigned char)s[i - 1]);
      int next_is_alpha = (i < n - 1) && isalpha((unsigned char)s[i + 1]);
      if (!prev_is_alpha && !next_is_alpha) {
        s[i] = 'x'; /* standalone -> multiply operator */
        continue;
      }
    }
    s[i] = (char)toupper(c);
  }
}

static void lex(const char *src) {
  S.ntokens = 0;
  S.nletters = 0;
  memset(S.digit, -1, sizeof S.digit);
  memset(S.used, 0, sizeof S.used);
  memset(S.leading, 0, sizeof S.leading);

  for (const char *p = src; *p;) {
    if (isspace((unsigned char)*p)) {
      p++;
      continue;
    }

    Token tk = {0};

    if (isupper((unsigned char)*p)) {
      tk.kind = TK_WORD;
      int len = 0;
      while (isupper((unsigned char)*p) && len < MAX_WORD_LEN)
        tk.word[len++] = *p++;
      if (isupper((unsigned char)*p)) {
        fprintf(stderr, "\n  [Error] word exceeds %d letters.\n", MAX_WORD_LEN);
        exit(EXIT_FAILURE);
      }
      tk.word[len] = '\0';
      S.leading[(unsigned char)(tk.word[0] - 'A')] = 1;
      for (int i = 0; i < len; i++)
        register_letter(tk.word[i]);
    } else {
      tk.kind = TK_OP;
      switch (*p) {
      case '+':
        tk.op = OP_ADD;
        break;
      case '-':
        tk.op = OP_SUB;
        break;
      case '*':
        tk.op = OP_MUL;
        break;
      case 'x':
        tk.op = OP_MUL;
        break;
      case '/':
        tk.op = OP_DIV;
        break;
      case '=':
        tk.kind = TK_EQ;
        break;
      default:
        fprintf(stderr, "\n  [Error] unexpected character '%c'.\n", *p);
        exit(EXIT_FAILURE);
      }
      p++;
    }

    if (S.ntokens >= MAX_TOKENS)
      ERR("too many tokens.");
    S.tokens[S.ntokens++] = tk;
  }
  S.tokens[S.ntokens++] = (Token){.kind = TK_END};
}

static long long eval_word(const char *w) {
  long long v = 0;
  for (int i = 0; w[i]; i++)
    v = v * 10 + S.digit[(unsigned char)(w[i] - 'A')];
  return v;
}

static int evaluate(void) {
  if (S.ntokens == 0)
    return 0;
  S.cur = 0;
  long long segs[MAX_SEGMENTS];
  int nsegs = 0;

  while (S.tokens[S.cur].kind != TK_END) {
    if (S.tokens[S.cur].kind != TK_WORD)
      return 0;
    long long current_val = eval_word(S.tokens[S.cur++].word);

    while (S.tokens[S.cur].kind == TK_OP) {
      Op op = S.tokens[S.cur++].op;
      if (S.tokens[S.cur].kind != TK_WORD)
        return 0;
      long long next_val = eval_word(S.tokens[S.cur++].word);

      switch (op) {
      case OP_ADD:
        current_val += next_val;
        break;
      case OP_SUB:
        current_val -= next_val;
        break;
      case OP_MUL:
        current_val *= next_val;
        break;
      case OP_DIV:
        if (next_val == 0 || current_val % next_val != 0)
          return 0;
        current_val /= next_val;
        break;
      }
    }

    if (nsegs >= MAX_SEGMENTS)
      return 0;
    segs[nsegs++] = current_val;

    if (S.tokens[S.cur].kind == TK_EQ)
      S.cur++;
  }

  if (nsegs < 2)
    return 0;
  for (int i = 1; i < nsegs; i++)
    if (segs[i] != segs[0])
      return 0;
  return 1;
}

static const char *op_sym[] = {"+", "-", "×", "÷"};

/* Set by brute_force_solve() when the raw input structurally matches
 * long-mul notation, so print_solution() knows to display it with the
 * correct column shift instead of the generic literal-sum format. */
static int G_bf_longmul_shape = 0;

static int evaluate_longmul_shape(void) {
  long long w1 = eval_word(S.lm_multiplicand);
  long long w2 = eval_word(S.lm_multiplier);
  long long product = eval_word(S.lm_product);

  long long shifted_sum = 0, shift = 1;
  for (int i = 0; i < S.lm_npartials; i++) {
    shifted_sum += eval_word(S.lm_partials[i]) * shift;
    shift *= 10;
  }

  return (w1 * w2 == product) && (shifted_sum == product);
}

static void print_solution(void) {
  S.nsolutions++;
  printf("\n  Solution [#%d]\n", S.nsolutions);
  printf("                  +---------+\n");
  for (int i = 0; i < S.nletters; i++)
    printf("                  | %c  →  %d |\n", S.letters[i],
           S.digit[(unsigned char)(S.letters[i] - 'A')]);
  printf("                  +---------+\n");
  printf("\n  Equation: ");
  if (S.is_longmul || G_bf_longmul_shape) {
    long long multiplicand = eval_word(S.lm_multiplicand);
    long long multiplier = eval_word(S.lm_multiplier);
    long long product = eval_word(S.lm_product);

    printf("%lld %s %lld = ", multiplicand, op_sym[OP_MUL], multiplier);
    long long shift = 1;
    for (int i = 0; i < S.lm_npartials; i++) {
      if (i > 0)
        printf(" + ");
      long long partial = eval_word(S.lm_partials[i]);
      printf("%lld", partial);
      if (i > 0)
        printf("*%lld", shift);
      shift *= 10;
    }
    printf(" = %lld", product);
    return;
  }

  for (int i = 0; i < S.ntokens - 1; i++) {
    Token *tk = &S.tokens[i];
    switch (tk->kind) {
    case TK_WORD:
      printf("%lld", eval_word(tk->word));
      break;
    case TK_EQ:
      printf(" = ");
      break;
    case TK_OP:
      printf(" %s ", op_sym[tk->op]);
      break;
    default:
      break;
    }
  }
}

static int detect_addition(void) {
  if (S.ntokens < 4)
    return 0;

  S.nadd = 0;
  int i = 0;
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  if (S.nadd >= MAX_ADDENDS)
    return 0;
  strcpy(S.addends[S.nadd++], S.tokens[i++].word);

  while (S.tokens[i].kind == TK_OP) {
    if (S.tokens[i].op != OP_ADD)
      return 0;
    i++;
    if (S.tokens[i].kind != TK_WORD)
      return 0;
    if (S.nadd >= MAX_ADDENDS)
      return 0;
    strcpy(S.addends[S.nadd++], S.tokens[i++].word);
  }

  if (S.tokens[i].kind != TK_EQ)
    return 0;
  i++;
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  strcpy(S.result, S.tokens[i++].word);
  if (S.tokens[i].kind != TK_END)
    return 0;

  return 1;
}
static int detect_long_mul(void) {
  int i = 0;

  /* W1 */
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  strcpy(S.lm_multiplicand, S.tokens[i++].word);

  /* * */
  if (S.tokens[i].kind != TK_OP || S.tokens[i].op != OP_MUL)
    return 0;
  i++;

  /* W2 */
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  strcpy(S.lm_multiplier, S.tokens[i++].word);
  int mlen = (int)strlen(S.lm_multiplier); /* number of digits in multiplier */

  /* = */
  if (S.tokens[i].kind != TK_EQ)
    return 0;
  i++;

  /* Collect partial products: P0 (+ Pi)* */
  S.lm_npartials = 0;
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  strcpy(S.lm_partials[S.lm_npartials++], S.tokens[i++].word);

  while (S.tokens[i].kind == TK_OP) {
    if (S.tokens[i].op != OP_ADD)
      return 0;
    i++;
    if (S.tokens[i].kind != TK_WORD)
      return 0;
    if (S.lm_npartials >= MAX_WORD_LEN)
      return 0;
    strcpy(S.lm_partials[S.lm_npartials++], S.tokens[i++].word);
  }

  if (S.tokens[i].kind != TK_EQ)
    return 0;
  i++;
  if (S.tokens[i].kind != TK_WORD)
    return 0;
  strcpy(S.lm_product, S.tokens[i++].word);

  if (S.tokens[i].kind != TK_END)
    return 0;

  /* Number of partials must equal length of multiplier */
  if (S.lm_npartials != mlen)
    return 0;

  return 1;
}

typedef enum { FA_FAIL, FA_ALREADY, FA_NEW } FAResult;

static int w1_len_g, mlen_g, product_len_g, prod_cols_g;
static int w1_letter_col[MAX_WORD_LEN];   /* col j -> letter idx    */
static int mult_letter_pos[MAX_WORD_LEN]; /* pos i -> letter idx    */
static int part_letter_col[MAX_WORD_LEN][MAX_WORD_LEN + 1];
static int part_len_g[MAX_WORD_LEN];
static int prod_letter_col[MAX_WORD_LEN * 2 + 2];
static int w1_distinct[MAX_LETTERS], w1_ndistinct;

static FAResult try_assign_forced(int li, int value) {
  int ci = (unsigned char)(S.letters[li] - 'A');
  if (S.digit[ci] != -1)
    return (S.digit[ci] == value) ? FA_ALREADY : FA_FAIL;
  if (value < 0 || value > 9 || S.used[value])
    return FA_FAIL;
  if (S.leading[ci] && value == 0)
    return FA_FAIL;
  S.digit[ci] = value;
  S.used[value] = 1;
  return FA_NEW;
}

static void undo_forced(int li, int value) {
  int ci = (unsigned char)(S.letters[li] - 'A');
  S.digit[ci] = -1;
  S.used[value] = 0;
}

static void longmul_setup(void) {
  w1_len_g = (int)strlen(S.lm_multiplicand);
  mlen_g = S.lm_npartials;
  product_len_g = (int)strlen(S.lm_product);

  for (int j = 0; j < w1_len_g; j++)
    w1_letter_col[j] = letter_index(S.lm_multiplicand[w1_len_g - 1 - j]);

  for (int i = 0; i < mlen_g; i++)
    mult_letter_pos[i] = letter_index(S.lm_multiplier[mlen_g - 1 - i]);

  int max_relevant = product_len_g - 1;
  for (int i = 0; i < mlen_g; i++) {
    int plen = (int)strlen(S.lm_partials[i]);
    part_len_g[i] = plen;
    for (int j = 0; j < plen; j++)
      part_letter_col[i][j] = letter_index(S.lm_partials[i][plen - 1 - j]);
    if (i + plen - 1 > max_relevant)
      max_relevant = i + plen - 1;
  }
  prod_cols_g = max_relevant + 2; /* +1 real, +1 overflow-must-be-zero */

  for (int k = 0; k < product_len_g; k++)
    prod_letter_col[k] = letter_index(S.lm_product[product_len_g - 1 - k]);

  int seen[MAX_LETTERS] = {0};
  w1_ndistinct = 0;
  for (int j = 0; j < w1_len_g; j++) {
    int li = w1_letter_col[j];
    if (!seen[li]) {
      seen[li] = 1;
      w1_distinct[w1_ndistinct++] = li;
    }
  }
}

static void longmul_solve_partials(int i);

static void longmul_solve_product(void) {
  int carry = 0, ok = 1;
  int forced_li[MAX_WORD_LEN * 2 + 2], forced_val[MAX_WORD_LEN * 2 + 2], nf = 0;

  for (int k = 0; k < prod_cols_g && ok; k++) {
    long sum = carry;
    for (int i = 0; i < mlen_g; i++) {
      int jj = k - i;
      if (jj >= 0 && jj < part_len_g[i])
        sum +=
            S.digit[(unsigned char)(S.letters[part_letter_col[i][jj]] - 'A')];
    }
    int digit_needed = sum % 10;
    carry = sum / 10;
    int pli = (k < product_len_g) ? prod_letter_col[k] : -1;
    if (pli >= 0) {
      FAResult r = try_assign_forced(pli, digit_needed);
      if (r == FA_FAIL) {
        ok = 0;
        break;
      }
      if (r == FA_NEW) {
        forced_li[nf] = pli;
        forced_val[nf] = digit_needed;
        nf++;
      }
    } else if (digit_needed != 0) {
      ok = 0;
    }
  }
  if (ok && carry != 0)
    ok = 0;
  if (ok)
    print_solution();
  for (int t = nf - 1; t >= 0; t--)
    undo_forced(forced_li[t], forced_val[t]);
}

static void longmul_do_partial(int i, int mdigit) {
  int maxcols = part_len_g[i] > w1_len_g ? part_len_g[i] : w1_len_g;
  int carry = 0, ok = 1;
  int forced_li[MAX_WORD_LEN + 1], forced_val[MAX_WORD_LEN + 1], nf = 0;

  for (int j = 0; j < maxcols && ok; j++) {
    int w1_digit =
        (j < w1_len_g)
            ? S.digit[(unsigned char)(S.letters[w1_letter_col[j]] - 'A')]
            : 0;
    long total = (long)w1_digit * mdigit + carry;
    int digit_needed = total % 10;
    carry = total / 10;
    int pli = (j < part_len_g[i]) ? part_letter_col[i][j] : -1;
    if (pli >= 0) {
      FAResult r = try_assign_forced(pli, digit_needed);
      if (r == FA_FAIL) {
        ok = 0;
        break;
      }
      if (r == FA_NEW) {
        forced_li[nf] = pli;
        forced_val[nf] = digit_needed;
        nf++;
      }
    } else if (digit_needed != 0) {
      ok = 0;
    }
  }
  if (ok && carry != 0)
    ok = 0;

  int next_i = i + 1;
  if (ok) {
    if (next_i == mlen_g)
      longmul_solve_product();
    else
      longmul_solve_partials(next_i);
  }
  for (int t = nf - 1; t >= 0; t--)
    undo_forced(forced_li[t], forced_val[t]);
}

static void longmul_solve_partials(int i) {
  int li = mult_letter_pos[i];
  int ci = (unsigned char)(S.letters[li] - 'A');
  if (S.digit[ci] != -1) {
    longmul_do_partial(i, S.digit[ci]);
    return;
  }
  int is_leading = S.leading[ci];
  for (int d = 0; d <= 9; d++) {
    if (S.used[d] || (is_leading && d == 0))
      continue;
    S.digit[ci] = d;
    S.used[d] = 1;
    longmul_do_partial(i, d);
    S.digit[ci] = -1;
    S.used[d] = 0;
  }
}

static void longmul_assign_w1(int idx) {
  if (idx == w1_ndistinct) {
    longmul_solve_partials(0);
    return;
  }
  int li = w1_distinct[idx];
  int ci = (unsigned char)(S.letters[li] - 'A');
  int is_leading = S.leading[ci];
  for (int d = 0; d <= 9; d++) {
    if (S.used[d] || (is_leading && d == 0))
      continue;
    S.digit[ci] = d;
    S.used[d] = 1;
    longmul_assign_w1(idx + 1);
    S.digit[ci] = -1;
    S.used[d] = 0;
  }
}

static void longmul_csp_solve(void) {
  longmul_setup();
  longmul_assign_w1(0);
}

static void build_columns(void) {
  int maxlen = (int)strlen(S.result);
  for (int a = 0; a < S.nadd; a++) {
    int l = (int)strlen(S.addends[a]);
    if (l > maxlen)
      maxlen = l;
  }

  S.result_len = (int)strlen(S.result);
  S.ncols = maxlen + 1;

  memset(S.cols, 0, sizeof S.cols);

  for (int k = 0; k < S.ncols; k++) {
    ColDesc *cd = &S.cols[k];

    /* addend letters */
    for (int a = 0; a < S.nadd; a++) {
      int wlen = (int)strlen(S.addends[a]);
      int pos = wlen - 1 - k;
      if (pos < 0)
        continue;
      int li = letter_index(S.addends[a][pos]);
      if (li < 0)
        continue;
      if (cd->n >= MAX_COL_LETTERS)
        ERR("too many letters in column.");
      cd->letter_idx[cd->n] = li;
      cd->coeff[cd->n] = +1;
      cd->n++;
    }

    /* result letter */
    int pos = S.result_len - 1 - k;
    if (pos >= 0) {
      int li = letter_index(S.result[pos]);
      if (li >= 0) {
        if (cd->n >= MAX_COL_LETTERS)
          ERR("too many letters in column.");
        cd->letter_idx[cd->n] = li;
        cd->coeff[cd->n] = -1;
        cd->n++;
      }
    }
  }

  /* Mark which letters appear for the first time in each column */
  int seen[MAX_LETTERS] = {0};
  for (int k = 0; k < S.ncols; k++) {
    ColDesc *cd = &S.cols[k];
    cd->n_new = 0;
    for (int j = 0; j < cd->n; j++) {
      int li = cd->letter_idx[j];
      if (!seen[li]) {
        seen[li] = 1;
        cd->new_idx[cd->n_new++] = li;
      }
    }
  }
}

static void carry_solve(int col, int carry_in);

static void assign_new(int col, int carry_in, int new_pos) {
  ColDesc *cd = &S.cols[col];

  if (new_pos == cd->n_new) {

#define LDIGIT(li) S.digit[(unsigned char)(S.letters[(li)] - 'A')]

    int addend_sum = 0;
    int result_digit = -1; /* -1 means no result letter in this col */

    for (int j = 0; j < cd->n; j++) {
      int li = cd->letter_idx[j];
      if (cd->coeff[j] == +1) {
        addend_sum += LDIGIT(li);
      } else {
        /* coeff == -1: this is the result letter */
        result_digit = LDIGIT(li);
      }
    }

    int total = addend_sum + carry_in;
    int digit_needed = total % 10; /* always non-negative (total >= 0) */
    int carry_out = total / 10;

    if (result_digit >= 0) {
      /* Result letter exists in this column — its digit must match */
      if (result_digit != digit_needed)
        return; /* prune */
    } else {
      /*
       * No result letter in this column.
       * This happens when this column is beyond the result word length
       * (overflow carry column).  The residue must be 0.
       */
      if (digit_needed != 0)
        return; /* prune */
    }

#undef LDIGIT

    /* Passed constraint — recurse to next column */
    carry_solve(col + 1, carry_out);
    return;
  }

  /* ── Assign digit to new_idx[new_pos] ── */
  int li = cd->new_idx[new_pos];
  char letter = S.letters[li];
  int ci = (unsigned char)(letter - 'A'); /* index into S.digit */
  int is_leading = S.leading[ci];

  /* Guard: if already assigned (appears in an earlier column too), skip */
  if (S.digit[ci] != -1) {
    assign_new(col, carry_in, new_pos + 1);
    return;
  }

  for (int d = 0; d <= 9; d++) {
    if (S.used[d])
      continue;
    if (is_leading && d == 0)
      continue; /* no leading zero */

    S.digit[ci] = d;
    S.used[d] = 1;
    assign_new(col, carry_in, new_pos + 1);
    S.digit[ci] = -1;
    S.used[d] = 0;
  }
}

static void carry_solve(int col, int carry_in) {
  if (col == S.ncols) {
    /*
     * All columns processed.
     * Carry must be 0: the addition must not overflow beyond the result.
     */
    if (carry_in != 0)
      return;

    /* Sanity: verify no leading zeros slipped through */
    for (int i = 0; i < S.nletters; i++) {
      int lci = (unsigned char)(S.letters[i] - 'A');
      if (S.leading[lci] && S.digit[lci] == 0)
        return;
    }
    print_solution();
    return;
  }

  assign_new(col, carry_in, 0);
}

typedef int (*EvalFn)(void);

static void brute_force_solve_with(int idx, EvalFn eval_fn) {
  if (idx == S.nletters) {
    for (int i = 0; i < S.nletters; i++) {
      int ci = (unsigned char)(S.letters[i] - 'A');
      if (S.leading[ci] && S.digit[ci] == 0)
        return;
    }
    if (eval_fn())
      print_solution();
    return;
  }

  int ci = (unsigned char)(S.letters[idx] - 'A');
  for (int d = 0; d <= 9; d++) {
    if (S.used[d])
      continue;
    S.digit[ci] = d;
    S.used[d] = 1;
    brute_force_solve_with(idx + 1, eval_fn);
    S.digit[ci] = -1;
    S.used[d] = 0;
  }
}

static void brute_force_solve(int idx) {
  G_bf_longmul_shape = detect_long_mul();
  EvalFn fn = G_bf_longmul_shape ? evaluate_longmul_shape : evaluate;
  brute_force_solve_with(idx, fn);
}

int main(int argc, char *argv[]) {
  char input[MAX_INPUTS];
  int force_bruteforce = 0;
  int force_longmul = 0;
  int force_columncarry = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--brute-force") == 0 || strcmp(argv[i], "-b") == 0) {
      force_bruteforce = 1;
    } else if (strcmp(argv[i], "--long-mul") == 0 ||
               strcmp(argv[i], "-l") == 0) {
      force_longmul = 1;
    } else if (strcmp(argv[i], "--column-carry") == 0 ||
               strcmp(argv[i], "-c") == 0) {
      force_columncarry = 1;
    }
  }

  printf("+-----------------------------------------+\n");
  printf("|         Cryptarithmetic Solver          |\n");
  printf("+-----------------------------------------+\n");
  printf("|  · Each word ≤ %d letters                |\n", MAX_WORD_LEN);
  printf("|  · Each letter → unique digit 0–9       |\n");
  printf("|  · Operators: + - * / x =               |\n");
  printf("|  · Column-Carry: W1+W2+...+Wn = PROD    |\n");
  printf("|  · Long-mul: W1*W2 = P0+P1+...= PROD    |\n");
  printf("|  · Flags: --brute-force --long-mul      |\n");
  printf("|           --column-carry                |\n");
  printf("+-----------------------------------------+\n");
  printf("\n  Examples:\n");
  printf("    SEND + MORE = MONEY\n");
  printf("    ABC * DE = FEC + DEC = HGBC  (long-mul partials)\n");
  printf("  Equation: ");
  fflush(stdout);

  if (!fgets(input, sizeof input, stdin))
    ERR("failed to read input.");
  input[strcspn(input, "\n")] = '\0';

  normalize_input(input);

  printf("\n  Input   : \"%s\"\n", input);
  lex(input);

  printf("  Letters : ");
  for (int i = 0; i < S.nletters; i++)
    printf("%c ", S.letters[i]);
  printf("(%d unique)\n", S.nletters);

  /* ── Detect puzzle type (in priority order) ── */
  if (force_bruteforce) {
    S.is_addition = 0;
    S.is_longmul = 0;
  } else if (force_longmul) {
    S.is_addition = 0;
    S.is_longmul = detect_long_mul();
    if (!S.is_longmul)
      ERR("input does not match long-multiplication pattern "
          "(W1 * W2 = P0 + P1 + ... = PRODUCT).");
  } else if (force_columncarry) {
    S.is_addition = detect_addition();
    S.is_longmul = 0;
    if (!S.is_addition)
      ERR("input does not match column-carry addition pattern "
          "(W1 + W2 + ... = RESULT).");
  } else {
    S.is_addition = detect_addition();
    S.is_longmul = (!S.is_addition) ? detect_long_mul() : 0;
  }

  if (S.is_addition) {
    printf("  Mode    : Column-Carry CSP\n");
    build_columns();

    printf("\n  +------------------------ Column layout ------------"
           "-------------+\n");
    for (int k = 0; k < S.ncols; k++) {
      ColDesc *cd = &S.cols[k];
      if (cd->n == 0)
        continue;
      printf("   col[%d]: addends[ ", k);
      for (int j = 0; j < cd->n; j++)
        if (cd->coeff[j] == +1)
          printf("%c ", S.letters[cd->letter_idx[j]]);
      printf("]  result[ ");
      for (int j = 0; j < cd->n; j++)
        if (cd->coeff[j] == -1)
          printf("%c ", S.letters[cd->letter_idx[j]]);
      int rpos = S.result_len - 1 - k;
      if (rpos < 0)
        printf("] (overflow carry col)");
      else
        printf("]");
      if (cd->n_new > 0) {
        printf("  ← first seen here: ");
        for (int j = 0; j < cd->n_new; j++)
          printf("%c ", S.letters[cd->new_idx[j]]);
      }
      printf("\n");
    }
    printf("  "
           "+----------------------------------------------------------------+"
           "\n");

  } else if (S.is_longmul) {
    int mlen = (int)strlen(S.lm_multiplier);
    printf("  Mode    : Long-multiplication CSP\n");
    printf("  Multiplicand : %s\n", S.lm_multiplicand);
    printf("  Multiplier   : %s  (%d digits)\n", S.lm_multiplier, mlen);
    for (int i = 0; i < S.lm_npartials; i++) {
      printf("  Partial[%d]   : %s  (%s × digit[%d from right])\n", i,
             S.lm_partials[i], S.lm_multiplicand, i);
    }
    printf("  Product      : %s\n", S.lm_product);
    printf("\n");
  } else {
    printf("  Mode    : Brute-Force + eval\n");
  }

  printf("\n  Solving...\n");
  S.nsolutions = 0;

  struct timespec t_start, t_end;
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  if (S.is_addition)
    carry_solve(0, 0);
  else if (S.is_longmul)
    longmul_csp_solve();
  else
    brute_force_solve(0);

  clock_gettime(CLOCK_MONOTONIC, &t_end);
  double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e6;

  printf("\n  +--------------------------------------------+\n");
  if (S.nsolutions == 0)
    printf("  |          No solutions found.               |\n");
  else
    printf("  |  Found:                  %-2d    solution(s) |\n",
           S.nsolutions);
  printf("  +--------------------------------------------+\n");
  printf("  Execution time: %.3f ms\n", elapsed_ms);

  return EXIT_SUCCESS;
}
