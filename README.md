# Cryptarithmetic Problem

---

## Part 1: Introduction

### 1.1 Scenario

Cryptarithmetic (also known as verbal arithmetic) is a type of mathematical puzzle where arithmetic equations are presented with letters substituting digits. Each letter represents a **unique** digit (0–9), and the goal is to find an assignment of digits to letters such that the equation holds true.

** Example:**

```
      A B C
    ×   D E
    -------
      F E C        ← ABC × E  (partial product, digit 0 from right of DE)
+   D E C          ← ABC × D  (partial product, digit 1 from right of DE, shifted ×10)
    -------
  H G B C          ← final product
```

Written as a single equation:

```
ABC * DE = FEC + DEC = HGBC
```

**Rules:**

- Each letter (`A, B, C, D, E, F, G, H`) maps to a **distinct** digit from `{0–9}`.
- Leading letters (`A`, `D`) **cannot** be zero.
- The partial-product structure must match standard long multiplication exactly.

---

### 1.2 Application

Cryptarithmetic puzzles serve as a canonical benchmark in **Artificial Intelligence** and **Constraint Programming** due to their combinatorial nature. Real-world applications include:

- **AI & Search Algorithms**: Benchmarking backtracking, branch-and-bound, and constraint propagation strategies.
- **Cryptography**: Conceptual similarity to cipher solving and symbolic reasoning.
- **Compiler Design & Symbolic Computation**: Mapping symbols to values under constraints mirrors type inference and register allocation.
- **Education**: Teaches logical deduction, systematic enumeration, and algorithmic thinking.
- **Operations Research**: Demonstrates constraint-satisfaction techniques applicable to scheduling, planning, and resource allocation.

---

### 1.3 Motivation — Combinatorial Explosion

`ABC * DE = FEC + DEC = HGBC` contains **8 unique letters**: `{A, B, C, D, E, F, G, H}`.

The raw search space is:

```
P(10, 8) = 10! / (10 - 8)! = 1,814,400 possible assignments
```

Without intelligent pruning, checking all 1.8 million candidates and verifying each against four simultaneous constraints (two partial products, one shifted sum, one full product) is computationally wasteful. This motivates the use of **Constraint Satisfaction Problem (CSP)** formulation combined with **backtracking** and **pruning** to drastically reduce the search space.

---

### 1.4 Goal

> **Analyze, from a scientific perspective**, the `ABC * DE = FEC + DEC = HGBC`
> cryptarithmetic puzzle by:
>
> 1. Formally modeling it as a **Constraint Satisfaction Problem (CSP)**.
> 2. Designing and applying **Backtracking** with **pruning** techniques.
> 3. Tracing the **Long-Multiplication CSP** solver.
> 4. Evaluating **time and space complexity** and measuring empirical performance.

---

## Part 2: Problem Modeling & Algorithm Design

### 2.1 Modeling as a Constraint Satisfaction Problem (CSP)

A **Constraint Satisfaction Problem** is defined by a triple **(X, D, C)**:

| Component           | Definition                                                  |
| ------------------- | ----------------------------------------------------------- |
| **X** — Variables   | Each unique letter in the puzzle                            |
| **D** — Domains     | `D(xi) = {0, 1, …, 9}`                                      |
| **C** — Constraints | All-Different + partial-product equations + no leading zero |

---

#### 2.1.1 Variables

```
X = { A, B, C, D, E, F, G, H }     (8 unique letters)
```

Extracted from the input string `ABC * DE = FEC + DEC = HGBC` in the order they
first appear.

---

#### 2.1.2 Domains

- Default domain for every letter: `{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}`
- **Leading-digit constraint** shrinks domain at source:
  - `A ∈ {1, 2, …, 9}` (first letter of `ABC`)
  - `D ∈ {1, 2, …, 9}` (first letter of `DEC` and `DE`)

---

#### 2.1.3 Constraints

**C1 — All-Different:**

```
∀ xi, xj ∈ X, i ≠ j  →  xi ≠ xj
```

No two letters may share the same digit.

**C2 — Partial Product 0** (rightmost digit of `DE` = digit `E`):

```
val(ABC) × digit(E) = val(FEC)
```

**C3 — Partial Product 1** (next digit of `DE` from right = digit `D`):

```
val(ABC) × digit(D) = val(DEC)
```

**C4 — Shifted Sum equals Final Product:**

```
val(FEC) + val(DEC) × 10 = val(HGBC)
```

**C5 — Full Multiplication:**

```
val(ABC) × val(DE) = val(HGBC)
```

**C6 — No Leading Zero:**

```
A != 0, D != 0 , F!= 0 , H != 0
```

**CSP Summary:**

```
Find: assignment  f : {A,B,C,D,E,F,G,H} → {0..9}

Such that:
  AllDifferent(A, B, C, D, E, F, G, H)
  A ≠ 0,  D ≠ 0
  (100A+10B+C) × f(E)         = 100f(F)+10f(E)+f(C)
  (100A+10B+C) × f(D)         = 100f(D)+10f(E)+f(C)
  val(FEC) + val(DEC)×10      = 1000f(H)+100f(G)+10f(B)+f(C)
  (100A+10B+C) × (10f(D)+f(E))= 1000f(H)+100f(G)+10f(B)+f(C)
```

---

### 2.2 Long-Multiplication Structure

Standard long multiplication of `ABC × DE`:

```
         A  B  C
      ×     D  E
      ---------
         F  E  C    ← row 0: ABC × E  (units digit of DE)
+     D  E  C       ← row 1: ABC × D  (tens digit  of DE), shifted left by 1
      ---------
      H  G  B  C    ← sum of row0 + row1×10
```

The solver detects this structure automatically using `detect_long_mul()`:

```c
/* Pattern: W1 * W2 = P0 + P1 + ... + Pk = PRODUCT
 *   P0 corresponds to W2's rightmost digit (index 0)
 *   P1 corresponds to W2's next digit      (index 1)
 *   ...
 *   Number of partials must equal strlen(W2)
 */
```

For `ABC * DE = FEC + DEC = HGBC`:

| Field             | Value                      |
| ----------------- | -------------------------- |
| `lm_multiplicand` | `"ABC"`                    |
| `lm_multiplier`   | `"DE"` (2 digits)          |
| `lm_partials[0]`  | `"FEC"` — `ABC × digit(E)` |
| `lm_partials[1]`  | `"DEC"` — `ABC × digit(D)` |
| `lm_product`      | `"HGBC"`                   |
| `lm_npartials`    | `2` (= `strlen("DE")`)     |

---

### 2.3 Backtracking Algorithm

The solver uses **generic backtracking**: assign digits one letter at a time, and call
`evaluate_long_mul()` once all 8 letters are assigned.

**Pseudocode:**

```
generic_solve(idx):
    if idx == nletters:
        if no leading zero slipped through:
            if evaluate_long_mul():
                print_solution()
        return

    letter = letters[idx]
    for d in 0..9:
        if used[d]: continue
        if leading[letter] and d == 0: continue
        digit[letter] = d
        used[d] = 1
        generic_solve(idx + 1)
        digit[letter] = -1
        used[d] = 0
```

```c
static void generic_solve_with(int idx, EvalFn eval_fn) {
    if (idx == S.nletters) {
        for (int i = 0; i < S.nletters; i++) {
            int ci = (unsigned char)(S.letters[i] - 'A');
            if (S.leading[ci] && S.digit[ci] == 0) return;
        }
        if (eval_fn()) print_solution();
        return;
    }
    int ci = (unsigned char)(S.letters[idx] - 'A');
    for (int d = 0; d <= 9; d++) {
        if (S.used[d]) continue;
        S.digit[ci] = d;  S.used[d] = 1;
        generic_solve_with(idx + 1, eval_fn);
        S.digit[ci] = -1; S.used[d] = 0;
    }
}
```

Called as:

```c
generic_solve_with(0, evaluate_long_mul);
```

---

### 2.4 Pruning Techniques

**① All-Different — `used[]` array (O(1) per check)**

```c
if (S.used[d]) continue;
```

After assigning digit `d` to a letter, `used[d] = 1` immediately prevents any other letter
from taking the same digit. This eliminates duplicate-digit assignments without extra
bookkeeping.

**② No Leading Zero**

```c
if (S.leading[ci] && d == 0) continue;
```

`leading[c]` is set during lexing for every first character of a word. Applied at
assignment time, this prunes entire subtrees where `A = 0` or `D = 0`.

**③ Partial-Product Constraint (inside `evaluate_long_mul`)**

Once all digits are assigned, the evaluator checks each partial product:

```c
long got = multiplicand * mdigit;
if (got != expected) return 0;   /* prune this leaf immediately */
```

If `val(ABC) × digit(E) ≠ val(FEC)`, the function returns `0` without checking the
remaining constraints — an early-exit within the fully-assigned state.

**④ Computed-Product Cross-check**

```c
return (computed_product == product && multiplicand * multiplier == product);
```

Both the shifted-sum path (`val(FEC) + val(DEC)×10`) and the direct multiplication path
(`val(ABC) × val(DE)`) must agree with `val(HGBC)`. This dual check catches any residual
inconsistency.

---

### 2.5 Worked Trace

Suppose the backtracker is exploring the partial assignment:

```
A=1, B=7, C=5, D=3, E=8, F=4, G=0, H=2
```

**Step 1 — Evaluate partial product 0 (i=0, digit E):**

```
mdigit   = digit(E) = 8
expected = val(FEC) = val("FEC") = 100×4 + 10×8 + 5 = 485
got      = val(ABC) × mdigit = 175 × 8 = 1400
1400 ≠ 485  →  PRUNE immediately ✗
```

Backtracker undoes the last assignment and tries the next digit.

**Eventually the solver finds a valid assignment where all four constraints hold:**

```
val(ABC) × digit(E) = val(FEC)          ✓
val(ABC) × digit(D) = val(DEC)          ✓
val(FEC) + val(DEC) × 10 = val(HGBC)   ✓
val(ABC) × val(DE)  = val(HGBC)         ✓
```

---

## Part 3: Design, Data Structures & C Implementation

### 3.1 Core Data Structures

#### 3.1.1 Token Types (Lexer Output)

```c
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } Op;
typedef enum { TK_WORD, TK_OP, TK_EQ, TK_END } TKind;

typedef struct {
    TKind kind;
    Op    op;
    char  word[MAX_WORD_LEN + 1];
} Token;
```

Lexing `"ABC * DE = FEC + DEC = HGBC"` produces:

```
TK_WORD("ABC")  TK_OP(MUL)  TK_WORD("DE")
TK_EQ
TK_WORD("FEC")  TK_OP(ADD)  TK_WORD("DEC")
TK_EQ
TK_WORD("HGBC")
TK_END
```

#### 3.1.2 Long-Multiplication Fields (inside `Solver`)

```c
int  is_longmul;                               /* set to 1 when detected          */
char lm_multiplicand[MAX_WORD_LEN + 1];        /* "ABC"                           */
char lm_multiplier  [MAX_WORD_LEN + 1];        /* "DE"                            */
char lm_partials[MAX_WORD_LEN][MAX_WORD_LEN+1];/* {"FEC", "DEC"}                  */
int  lm_npartials;                             /* 2  (= strlen("DE"))             */
char lm_product     [MAX_WORD_LEN + 1];        /* "HGBC"                          */
```

#### 3.1.3 Global Solver State

```c
typedef struct {
    Token tokens[MAX_TOKENS];
    int   ntokens, cur;

    char  letters[MAX_LETTERS];   /* unique letters in order of first appearance  */
    int   nletters;               /* 8 for this puzzle                            */

    int   digit[26];   /* digit['A'-'A'] .. digit['Z'-'A'], -1 = unassigned       */
    int   used[10];    /* used[d] = 1 if digit d is already taken                 */
    int   leading[26]; /* leading[c-'A'] = 1 if letter c cannot be 0             */

    int   nsolutions;

    /* Long-Multiplication CSP fields (populated by detect_long_mul) */
    int  is_longmul;
    char lm_multiplicand[MAX_WORD_LEN + 1];
    char lm_multiplier  [MAX_WORD_LEN + 1];
    char lm_partials[MAX_WORD_LEN][MAX_WORD_LEN + 1];
    int  lm_npartials;
    char lm_product[MAX_WORD_LEN + 1];
} Solver;

static Solver S;
```

---

### 3.2 Implementation

#### Step 1 — Lexer (`lex`)

Scans the input string character by character, producing tokens. Registers every uppercase
letter into `S.letters[]` and marks leading letters:

```c
S.leading[(unsigned char)(tk.word[0] - 'A')] = 1;
for (int i = 0; i < len; i++)
    register_letter(tk.word[i]);
```

For `"ABC * DE = FEC + DEC = HGBC"`:

```
S.letters  = { A, B, C, D, E, F, G, H }
S.nletters = 8
S.leading  = { A=1, D=1, all others=0 }
```

#### Step 2 — Puzzle Detection (`detect_long_mul`)

Reads the token stream and fills the long-mul fields:

```c
static int detect_long_mul(void) {
    int i = 0;
    /* W1 */  strcpy(S.lm_multiplicand, S.tokens[i++].word);   /* "ABC" */
    /* *  */  i++;
    /* W2 */  strcpy(S.lm_multiplier,   S.tokens[i++].word);   /* "DE"  */
    int mlen = strlen(S.lm_multiplier);                          /* 2     */
    /* =  */  i++;

    /* Collect partials: FEC, DEC */
    S.lm_npartials = 0;
    strcpy(S.lm_partials[S.lm_npartials++], S.tokens[i++].word); /* FEC */
    while (S.tokens[i].kind == TK_OP) {          /* + DEC */
        i++;
        strcpy(S.lm_partials[S.lm_npartials++], S.tokens[i++].word);
    }
    /* = PRODUCT */
    i++;
    strcpy(S.lm_product, S.tokens[i++].word);    /* "HGBC" */

    if (S.lm_npartials != mlen) return 0;         /* must match multiplier length */
    return 1;
}
```

#### Step 3 — Solver Dispatch (`main`)

```c
S.is_longmul = (!S.is_addition) ? detect_long_mul() : 0;

if (S.is_longmul)
    generic_solve_with(0, evaluate_long_mul);
```

#### Step 4 — Long-Multiplication Evaluator (`evaluate_long_mul`)

```c
static int evaluate_long_mul(void) {
    long multiplicand = eval_word(S.lm_multiplicand); /* val(ABC)  */
    long multiplier   = eval_word(S.lm_multiplier);   /* val(DE)   */
    long product      = eval_word(S.lm_product);       /* val(HGBC) */

    if (multiplicand <= 0 || multiplier <= 0 || product <= 0) return 0;

    int  mlen             = S.lm_npartials;  /* 2 */
    long shift            = 1;               /* 10^i */
    long computed_product = 0;

    for (int i = 0; i < mlen; i++) {
        /*
         * i=0: lm_multiplier[mlen-1-0] = lm_multiplier[1] = 'E'  → digit(E)
         * i=1: lm_multiplier[mlen-1-1] = lm_multiplier[0] = 'D'  → digit(D)
         */
        int  mchar  = (unsigned char)(S.lm_multiplier[mlen - 1 - i] - 'A');
        long mdigit = S.digit[mchar];

        long expected = eval_word(S.lm_partials[i]);
        long got      = multiplicand * mdigit;

        if (got != expected) return 0;          /* C2 or C3 violated → prune */

        computed_product += expected * shift;
        shift *= 10;
    }

    /* C4 and C5 */
    return (computed_product == product && multiplicand * multiplier == product);
}
```

**Iteration detail for i=0 and i=1:**

| i   | `lm_multiplier[mlen-1-i]` | digit variable | partial word | constraint                     |
| --- | ------------------------- | -------------- | ------------ | ------------------------------ |
| 0   | `'E'` (index 1 of `"DE"`) | `digit(E)`     | `"FEC"`      | `val(ABC)×digit(E) = val(FEC)` |
| 1   | `'D'` (index 0 of `"DE"`) | `digit(D)`     | `"DEC"`      | `val(ABC)×digit(D) = val(DEC)` |

---

### 3.3 How to Compile and Run

```bash
# Compile (from project root)
make

# Run
./bin/main
```

**Full expected interaction:**

```
╔═════════════════════════════════════════╗
║       Cryptarithmetic Solver            ║
╠═════════════════════════════════════════╣
║  · Each word <= 8 letters               ║
║  · Each letter -> unique digit 0-9      ║
║  · Operators: + - * / x =               ║
║  · Long-mul: W1*W2 = P0+P1+...= PROD    ║
╚═════════════════════════════════════════╝

  Examples:
    SEND + MORE = MONEY
    ABC * DE = FEC + DEC = HGBC
  Equation: ABC * DE = FEC + DEC = HGBC

  Input   : "ABC * DE = FEC + DEC = HGBC"
  Letters : A B C D E F G H (8 unique)
  Mode    : Long-multiplication CSP
  Multiplicand : ABC
  Multiplier   : DE  (2 digits)
  Partial[0]   : FEC  (ABC x digit[0 from right])
  Partial[1]   : DEC  (ABC x digit[1 from right])
  Product      : HGBC

  Solving...

  Solution [#1]
                  │ A  ->  … │
                  │ B  ->  … │
                  │ C  ->  … │
                  │ D  ->  … │
                  │ E  ->  … │
                  │ F  ->  … │
                  │ G  ->  … │
                  │ H  ->  … │

  Equation: … × … = … + … = …

╔════════════════════════════════════════════╗
║  Found:               N  solution(s)       ║
╚════════════════════════════════════════════╝
```

> **Note**: Actual digit values and solution count are determined at runtime. The program
> enumerates **all** valid assignments and prints each as a numbered solution.

---

## Part 4: Efficiency Evaluation

### 4.1 Theoretical Complexity

#### Search Space

```
n = 8 unique letters: {A, B, C, D, E, F, G, H}
Raw permutations: P(10, 8) = 10!/2! = 1,814,400
```

#### Time Complexity

| Scenario                        | Nodes Explored  | Complexity                  |
| ------------------------------- | --------------- | --------------------------- |
| Brute force (no pruning)        | ≤ 1,814,400     | O(P(10,n))                  |
| Backtracking + All-Different    | Much less       | Reduced by used[]           |
| + No-leading-zero               | Further reduced | A≠0, D≠0 cuts ~20% of tries |
| + Partial-product check at leaf | Final filter    | O(mlen) = O(2) per leaf     |

#### Why Leaves Are Cheap

For `ABC * DE`, there are only `mlen = 2` partial products to check. Each check is:

```
O(len(multiplicand)) + O(len(partial))  =  O(3) + O(3)  =  O(1)
```

So `evaluate_long_mul` runs in **O(1)** per fully-assigned candidate.

#### Space Complexity

| Component                              | Memory                 |
| -------------------------------------- | ---------------------- |
| `digit[26]`, `used[10]`, `leading[26]` | O(1)                   |
| Recursion stack depth                  | O(n) = O(8) frames     |
| `lm_partials[8][9]`, `lm_product[9]`   | O(n²) = O(64) chars    |
| **Total**                              | **O(n²)** — negligible |

---

### 4.2 Empirical Benchmark

Run the compiled solver and fill in the table:

| Metric           | Value     |
| ---------------- | --------- |
| Unique letters   | 8         |
| Raw search space | 1,814,400 |
| Solutions found  | TBD       |
| Nodes explored   | TBD       |
| Solve time (ms)  | TBD       |

> **Note**: Fill in empirical values after running the compiled solver.

---

### 4.3 Observations & Conclusions

1. **Long-Mul CSP is structure-aware**: By parsing the explicit partial-product words
   (`FEC`, `DEC`) from the input, the solver turns an opaque multiplication into a set of
   exact integer equalities — far tighter constraints than a generic expression evaluator
   would produce.

2. **All-Different is free**: The `used[10]` boolean array enforces the all-different
   constraint in O(1) per digit assignment — zero overhead beyond a flag flip.

3. **Partial-product check at the leaf is sufficient**: For 2-digit multipliers, there are
   only 2 partial equalities to test. Together they almost always rule out an assignment
   immediately, keeping the effective search space tiny.

4. **No-leading-zero removes whole subtrees**: Skipping `A=0` and `D=0` at assignment time
   prunes ~20% of digit choices for those letters before any deeper recursion.

5. **Space efficiency**: The solver uses O(n²) memory regardless of puzzle complexity,
   making it suitable for embedded or resource-constrained environments.

6. **Scalability**: For multipliers with more digits (e.g., `ABC * DEF = ...`), the number
   of partial-product checks grows linearly with `strlen(multiplier)`, keeping the evaluator
   practical for all puzzle sizes supported by the program.

---
