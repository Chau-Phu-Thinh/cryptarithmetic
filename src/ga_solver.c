/*
 * ga_solver.c — Parallel Genetic Algorithm (Island Model) for Cryptarithmetic
 *
 * Solves:
 *   ABC * DE  = HGBC          (equation 1 — multiplication)
 *   FEC + DEC*10 = HGBC      (equation 2 — shifted partial-product sum)
 *
 * Letter-to-index mapping:
 *   0=A  1=B  2=C  3=D  4=E  5=F  6=G  7=H
 *
 * Leading-zero banned positions: A(0), D(3), F(5), H(7).
 *
 * Island Model:
 *   - GA_NUM_ISLANDS threads run independent GAs.
 *   - Every GA_MIGRATION_INTERVAL generations the best individuals
 *     migrate to the next island in a ring topology.
 *   - A shared atomic flag stops all islands as soon as one finds
 *     an exact solution (fitness == 0).
 *
 * Build (standalone):
 *   gcc -std=c17 -Wall -Wextra -O2 -pthread src/ga_solver.c src/ga_main.c -o
 * bin/ga -lm
 */

#include "ga_solver.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Thread-local PRNG (xorshift64) ────────────────────────────────── */
/*
 * Each island gets its own fast PRNG so we avoid contention on a
 * shared random state.  xorshift64 is simple and fast.
 */
typedef struct {
  uint64_t state;
} RNG;

static inline uint64_t rng_next(RNG *r) {
  uint64_t x = r->state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  r->state = x;
  return x;
}

/* Uniform int in [0, n) */
static inline int rng_int(RNG *r, int n) {
  return (int)(rng_next(r) % (uint64_t)n);
}

/* Uniform double in [0, 1) */
static inline double rng_double(RNG *r) {
  return (double)(rng_next(r) & 0xFFFFFFFFFFFFFULL) / (double)(1ULL << 52);
}

/* ── Leading-zero mask ─────────────────────────────────────────────── */
/*
 * leading_mask[i] == 1 iff letter i is a leading letter.
 * A(0), D(3), F(5), H(7) cannot be 0.
 */
static const int leading_mask[GA_NUM_LETTERS] = {1, 0, 0, 1, 0, 1, 0, 1};

/* ── Fitness evaluation ────────────────────────────────────────────── */
/*
 * Convert a letter-sequence to its decimal value under the current
 * chromosome.  e.g. word_val(chr, (int[]){0,1,2}, 3) → A*100+B*10+C
 */
static inline long word_val(const int *genes, const int *letters, int len) {
  long v = 0;
  for (int i = 0; i < len; i++)
    v = v * 10 + genes[letters[i]];
  return v;
}

/* Weight constants for the error terms */
#define W1 1.0 /* ABC * DE = HGBC          */
#define W2 1.0 /* ABC * E  = FEC (partial0) */
#define W3 1.0 /* ABC * D  = DEC (partial1) */
#define W4 1.0 /* FEC + DEC*10 = HGBC       */

/* Penalty for violating uniqueness or leading-zero constraints */
#define PENALTY_DUPLICATE 1000.0
#define PENALTY_LEADING 1000.0

/*
 * compute_fitness — long-multiplication constraints:
 *
 *   error1 = |ABC * DE  - HGBC|        (main product)
 *   error2 = |ABC * E   - FEC|         (partial product 0)
 *   error3 = |ABC * D   - DEC|         (partial product 1)
 *   error4 = |FEC + DEC*10 - HGBC|     (shifted sum of partials)
 *
 *   fitness = -(W1*error1 + W2*error2 + W3*error3 + W4*error4 + penalties)
 *
 * A fitness of 0.0 means an exact solution.
 */
static double compute_fitness(const Chromosome *chr) {
  const int *g = chr->genes;

  /* Penalty: duplicate digits */
  int seen[10] = {0};
  double penalty = 0.0;
  for (int i = 0; i < GA_NUM_LETTERS; i++) {
    if (g[i] < 0 || g[i] > 9)
      return -1e18;
    if (seen[g[i]])
      penalty += PENALTY_DUPLICATE;
    seen[g[i]] = 1;
  }

  /* Penalty: leading zeros */
  for (int i = 0; i < GA_NUM_LETTERS; i++) {
    if (leading_mask[i] && g[i] == 0)
      penalty += PENALTY_LEADING;
  }

  /* Letter indices for each word */
  static const int ABC[] = {0, 1, 2};     /* A B C */
  static const int DE[] = {3, 4};         /* D E   */
  static const int FEC[] = {5, 4, 2};     /* F E C */
  static const int DEC[] = {3, 4, 2};     /* D E C */
  static const int HGBC[] = {7, 6, 1, 2}; /* H G B C */

  long abc = word_val(g, ABC, 3);
  long de = word_val(g, DE, 2);
  long fec = word_val(g, FEC, 3);
  long dec = word_val(g, DEC, 3);
  long hgbc = word_val(g, HGBC, 4);

  /* Individual digit values for partial products */
  int d_digit = g[3]; /* D = leftmost digit of DE  */
  int e_digit = g[4]; /* E = rightmost digit of DE */

  double error1 = fabs((double)(abc * de - hgbc));     /* main product        */
  double error2 = fabs((double)(abc * e_digit - fec)); /* partial: ABC × E    */
  double error3 = fabs((double)(abc * d_digit - dec)); /* partial: ABC × D    */
  double error4 = fabs((double)(fec + dec * 10 - hgbc)); /* shifted sum */

  return -(W1 * error1 + W2 * error2 + W3 * error3 + W4 * error4 + penalty);
}

/* ── Random valid chromosome ───────────────────────────────────────── */
/*
 * Generates a random permutation of 8 distinct digits chosen from 0-9,
 * respecting leading-zero constraints.
 */
static void random_chromosome(RNG *rng, Chromosome *chr) {
  int pool[10];
  for (int i = 0; i < 10; i++)
    pool[i] = i;

  /* Fisher-Yates shuffle on the full pool */
  for (int i = 9; i > 0; i--) {
    int j = rng_int(rng, i + 1);
    int tmp = pool[i];
    pool[i] = pool[j];
    pool[j] = tmp;
  }

  /* Pick the first 8 from the shuffled pool */
  memcpy(chr->genes, pool, GA_NUM_LETTERS * sizeof(int));

  /* Repair leading-zero violations:
   * If a leading letter got 0, swap with a non-leading letter that
   * has a non-zero digit.
   */
  for (int i = 0; i < GA_NUM_LETTERS; i++) {
    if (leading_mask[i] && chr->genes[i] == 0) {
      /* find a non-leading position with nonzero digit */
      for (int j = 0; j < GA_NUM_LETTERS; j++) {
        if (!leading_mask[j] && chr->genes[j] != 0) {
          int tmp = chr->genes[i];
          chr->genes[i] = chr->genes[j];
          chr->genes[j] = tmp;
          break;
        }
      }
    }
  }

  chr->fitness = compute_fitness(chr);
}

/* ── Tournament selection ──────────────────────────────────────────── */
static int tournament_select(RNG *rng, const Chromosome *pop, int pop_size) {
  int best = rng_int(rng, pop_size);
  for (int i = 1; i < GA_TOURNAMENT_K; i++) {
    int r = rng_int(rng, pop_size);
    if (pop[r].fitness > pop[best].fitness)
      best = r;
  }
  return best;
}

/* ── PMX Crossover ─────────────────────────────────────────────────── */
/*
 * Partially-Mapped Crossover (PMX) for permutation chromosomes.
 * Produces two children from two parents.
 */
static void pmx_crossover(RNG *rng, const Chromosome *p1, const Chromosome *p2,
                          Chromosome *c1, Chromosome *c2) {
  int n = GA_NUM_LETTERS;

  /* Pick two crossover points */
  int pt1 = rng_int(rng, n);
  int pt2 = rng_int(rng, n);
  if (pt1 > pt2) {
    int tmp = pt1;
    pt1 = pt2;
    pt2 = tmp;
  }

  /* Initialise children to -1 (unmapped) */
  memset(c1->genes, -1, sizeof(c1->genes));
  memset(c2->genes, -1, sizeof(c2->genes));

  /* Copy segment [pt1, pt2] from parents */
  for (int i = pt1; i <= pt2; i++) {
    c1->genes[i] = p1->genes[i];
    c2->genes[i] = p2->genes[i];
  }

  /* Build mapping arrays for the copied segment */
  int map1[10], map2[10];
  memset(map1, -1, sizeof(map1));
  memset(map2, -1, sizeof(map2));
  for (int i = pt1; i <= pt2; i++) {
    map1[p1->genes[i]] = p2->genes[i];
    map2[p2->genes[i]] = p1->genes[i];
  }

  /* Fill remaining positions using PMX mapping */
  /* Build sets of values present in each child's copied segment */
  int in_seg1[10] = {0}, in_seg2[10] = {0};
  for (int i = pt1; i <= pt2; i++) {
    if (c1->genes[i] >= 0 && c1->genes[i] <= 9)
      in_seg1[c1->genes[i]] = 1;
    if (c2->genes[i] >= 0 && c2->genes[i] <= 9)
      in_seg2[c2->genes[i]] = 1;
  }

  for (int i = 0; i < n; i++) {
    if (i >= pt1 && i <= pt2)
      continue;

    /* Child 1: try to place p2's gene, follow mapping if conflicting */
    int val = p2->genes[i];
    while (in_seg1[val])
      val = map1[val];
    c1->genes[i] = val;

    /* Child 2: try to place p1's gene, follow mapping if conflicting */
    val = p1->genes[i];
    while (in_seg2[val])
      val = map2[val];
    c2->genes[i] = val;
  }

  /* Repair: fill any remaining -1 positions with unused digits */
  for (int child = 0; child < 2; child++) {
    int *g = (child == 0) ? c1->genes : c2->genes;
    int used[10] = {0};
    for (int i = 0; i < n; i++) {
      if (g[i] >= 0 && g[i] <= 9)
        used[g[i]] = 1;
    }
    int next_free = 0;
    for (int i = 0; i < n; i++) {
      if (g[i] < 0 || g[i] > 9) {
        while (next_free < 10 && used[next_free])
          next_free++;
        if (next_free < 10) {
          g[i] = next_free;
          used[next_free] = 1;
        }
      }
    }
  }

  c1->fitness = compute_fitness(c1);
  c2->fitness = compute_fitness(c2);
}

/* ── Swap mutation ─────────────────────────────────────────────────── */
/*
 * Swap two random gene positions.  Respects uniqueness (swaps preserve
 * the permutation property).  Re-checks leading-zero after swap.
 */
static void swap_mutation(RNG *rng, Chromosome *chr) {
  int i = rng_int(rng, GA_NUM_LETTERS);
  int j = rng_int(rng, GA_NUM_LETTERS);
  if (i == j)
    return;

  int tmp = chr->genes[i];
  chr->genes[i] = chr->genes[j];
  chr->genes[j] = tmp;

  /* If the swap created a leading-zero, undo it */
  if ((leading_mask[i] && chr->genes[i] == 0) ||
      (leading_mask[j] && chr->genes[j] == 0)) {
    chr->genes[j] = chr->genes[i];
    chr->genes[i] = tmp;
    return;
  }

  chr->fitness = compute_fitness(chr);
}

/* ── Comparison for qsort (descending fitness) ─────────────────────── */
static int cmp_fitness_desc(const void *a, const void *b) {
  double fa = ((const Chromosome *)a)->fitness;
  double fb = ((const Chromosome *)b)->fitness;
  if (fa > fb)
    return -1;
  if (fa < fb)
    return 1;
  return 0;
}

/* ── Shared state across islands ──────────────────────────────────── */
static atomic_int g_solution_found = 0;

/* Migration buffer: ring topology, island i → island (i+1) % N */
typedef struct {
  Chromosome buffer[GA_MIGRATION_COUNT];
  pthread_mutex_t mutex;
  int ready; /* 1 when buffer is filled */
} MigrationSlot;

static MigrationSlot g_migration[GA_NUM_ISLANDS];

/* Global best solution (protected by mutex) */
static Chromosome g_best;
static pthread_mutex_t g_best_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Per-island context ──────────────────────────────────────────── */
typedef struct {
  int island_id;
  RNG rng;
  int generations_run;
  int found_exact;
} IslandCtx;

/* ── Island thread function ──────────────────────────────────────── */
static void *island_thread(void *arg) {
  IslandCtx *ctx = (IslandCtx *)arg;
  RNG *rng = &ctx->rng;
  int id = ctx->island_id;

  /* Allocate populations */
  Chromosome *pop = malloc(GA_POP_SIZE * sizeof(Chromosome));
  Chromosome *next = malloc(GA_POP_SIZE * sizeof(Chromosome));
  if (!pop || !next) {
    fprintf(stderr, "  [Island %d] malloc failed\n", id);
    free(pop);
    free(next);
    return NULL;
  }

  /* Initialise population */
  for (int i = 0; i < GA_POP_SIZE; i++)
    random_chromosome(rng, &pop[i]);

  qsort(pop, GA_POP_SIZE, sizeof(Chromosome), cmp_fitness_desc);

  double best_fitness = pop[0].fitness;
  int stagnation = 0;

  for (int gen = 0; gen < GA_MAX_GENERATIONS; gen++) {
    /* Check if another island already found the solution */
    if (atomic_load(&g_solution_found)) {
      ctx->generations_run = gen;
      break;
    }

    /* ── Elitism: copy top individuals directly ── */
    for (int i = 0; i < GA_ELITE_COUNT; i++)
      next[i] = pop[i];

    /* ── Breed the rest of the population ── */
    int idx = GA_ELITE_COUNT;
    while (idx < GA_POP_SIZE) {
      int p1 = tournament_select(rng, pop, GA_POP_SIZE);
      int p2 = tournament_select(rng, pop, GA_POP_SIZE);

      if (rng_double(rng) < GA_CROSSOVER_RATE && idx + 1 < GA_POP_SIZE) {
        pmx_crossover(rng, &pop[p1], &pop[p2], &next[idx], &next[idx + 1]);
        idx += 2;
      } else {
        next[idx] = pop[p1];
        idx++;
      }
    }

    /* ── Mutation ── */
    for (int i = GA_ELITE_COUNT; i < GA_POP_SIZE; i++) {
      if (rng_double(rng) < GA_MUTATION_RATE)
        swap_mutation(rng, &next[i]);
    }

    /* Swap populations */
    Chromosome *tmp = pop;
    pop = next;
    next = tmp;

    /* Sort by fitness */
    qsort(pop, GA_POP_SIZE, sizeof(Chromosome), cmp_fitness_desc);

    /* ── Check for exact solution ── */
    if (pop[0].fitness >= -0.5) { /* fitness == 0 means exact */
      atomic_store(&g_solution_found, 1);
      ctx->found_exact = 1;
      ctx->generations_run = gen + 1;

      /* Update global best */
      pthread_mutex_lock(&g_best_mutex);
      if (pop[0].fitness > g_best.fitness)
        g_best = pop[0];
      pthread_mutex_unlock(&g_best_mutex);
      break;
    }

    /* ── Stagnation detection ── */
    if (pop[0].fitness > best_fitness + 0.5) {
      best_fitness = pop[0].fitness;
      stagnation = 0;
    } else {
      stagnation++;
    }

    if (stagnation >= GA_STAGNATION_LIMIT) {
      ctx->generations_run = gen + 1;
      /* Update global best before quitting */
      pthread_mutex_lock(&g_best_mutex);
      if (pop[0].fitness > g_best.fitness)
        g_best = pop[0];
      pthread_mutex_unlock(&g_best_mutex);
      break;
    }

    /* ── Migration (ring topology) ── */
    if ((gen + 1) % GA_MIGRATION_INTERVAL == 0) {
      /* Send best individuals to the next island */
      int dest = (id + 1) % GA_NUM_ISLANDS;
      pthread_mutex_lock(&g_migration[dest].mutex);
      for (int i = 0; i < GA_MIGRATION_COUNT; i++)
        g_migration[dest].buffer[i] = pop[i];
      g_migration[dest].ready = 1;
      pthread_mutex_unlock(&g_migration[dest].mutex);

      /* Receive immigrants from the previous island */
      pthread_mutex_lock(&g_migration[id].mutex);
      if (g_migration[id].ready) {
        /* Replace worst individuals with immigrants */
        for (int i = 0; i < GA_MIGRATION_COUNT; i++) {
          int slot = GA_POP_SIZE - 1 - i;
          pop[slot] = g_migration[id].buffer[i];
        }
        g_migration[id].ready = 0;

        /* Re-sort after immigration */
        qsort(pop, GA_POP_SIZE, sizeof(Chromosome), cmp_fitness_desc);

        /* Reset stagnation counter after immigration */
        if (pop[0].fitness > best_fitness + 0.5) {
          best_fitness = pop[0].fitness;
          stagnation = 0;
        }
      }
      pthread_mutex_unlock(&g_migration[id].mutex);
    }

    /* Update global best periodically */
    if ((gen + 1) % 500 == 0) {
      pthread_mutex_lock(&g_best_mutex);
      if (pop[0].fitness > g_best.fitness)
        g_best = pop[0];
      pthread_mutex_unlock(&g_best_mutex);
    }

    ctx->generations_run = gen + 1;
  }

  /* Final update of global best */
  pthread_mutex_lock(&g_best_mutex);
  if (pop[0].fitness > g_best.fitness)
    g_best = pop[0];
  pthread_mutex_unlock(&g_best_mutex);

  free(pop);
  free(next);
  return NULL;
}

/* ── Pretty-print a solution ──────────────────────────────────────── */
static void print_ga_solution(const Chromosome *chr) {
  static const char letter_names[GA_NUM_LETTERS] = {'A', 'B', 'C', 'D',
                                                    'E', 'F', 'G', 'H'};
  const int *g = chr->genes;

  printf("\n  +---------+\n");
  for (int i = 0; i < GA_NUM_LETTERS; i++)
    printf("  | %c  →  %d |\n", letter_names[i], g[i]);
  printf("  +---------+\n");

  static const int ABC[] = {0, 1, 2};
  static const int DE[] = {3, 4};
  static const int FEC[] = {5, 4, 2};
  static const int DEC[] = {3, 4, 2};
  static const int HGBC[] = {7, 6, 1, 2};

  long abc = word_val(g, ABC, 3);
  long de = word_val(g, DE, 2);
  long fec = word_val(g, FEC, 3);
  long dec = word_val(g, DEC, 3);
  long hgbc = word_val(g, HGBC, 4);

  int d_digit = g[3];
  int e_digit = g[4];

  printf("\n  Verification:\n");
  printf("    ABC * DE       = %ld * %ld = %ld", abc, de, abc * de);
  printf("   (HGBC = %ld)  %s\n", hgbc, (abc * de == hgbc) ? "✓" : "✗");
  printf("    ABC * E        = %ld * %d = %ld", abc, e_digit, abc * e_digit);
  printf("   (FEC  = %ld)  %s\n", fec, (abc * e_digit == fec) ? "✓" : "✗");
  printf("    ABC * D        = %ld * %d = %ld", abc, d_digit, abc * d_digit);
  printf("   (DEC  = %ld)  %s\n", dec, (abc * d_digit == dec) ? "✓" : "✗");
  printf("    FEC + DEC*10   = %ld + %ld = %ld", fec, dec * 10, fec + dec * 10);
  printf("   (HGBC = %ld)  %s\n", hgbc, (fec + dec * 10 == hgbc) ? "✓" : "✗");

  printf("\n  Fitness: %.1f\n", chr->fitness);
}

/* ── Public entry point ──────────────────────────────────────────── */
int ga_solve(void) {
  printf("\n");
  printf("  +-----------------------------------------------+\n");
  printf("  |   Parallel Genetic Algorithm — Island Model   |\n");
  printf("  +-----------------------------------------------+\n");
  printf("  |  Puzzle : ABC * DE = HGBC                     |\n");
  printf("  |           FEC + DEC*10 = HGBC                 |\n");
  printf("  |  Islands: %-4d  Population: %-4d              |\n",
         GA_NUM_ISLANDS, GA_POP_SIZE);
  printf("  |  Migration every %d generations               |\n",
         GA_MIGRATION_INTERVAL);
  printf("  +-----------------------------------------------+\n");
  printf("\n  Launching %d islands...\n", GA_NUM_ISLANDS);

  /* Reset shared state */
  atomic_store(&g_solution_found, 0);
  g_best.fitness = -1e18;

  for (int i = 0; i < GA_NUM_ISLANDS; i++) {
    pthread_mutex_init(&g_migration[i].mutex, NULL);
    g_migration[i].ready = 0;
  }

  /* Seed island RNGs from system time + island id */
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t base_seed =
      (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

  IslandCtx contexts[GA_NUM_ISLANDS];
  pthread_t threads[GA_NUM_ISLANDS];

  for (int i = 0; i < GA_NUM_ISLANDS; i++) {
    contexts[i].island_id = i;
    contexts[i].rng.state =
        base_seed ^ ((uint64_t)(i + 1) * 6364136223846793005ULL);
    if (contexts[i].rng.state == 0)
      contexts[i].rng.state = 1;
    contexts[i].generations_run = 0;
    contexts[i].found_exact = 0;
  }

  /* Launch threads */
  struct timespec t_start, t_end;
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  for (int i = 0; i < GA_NUM_ISLANDS; i++)
    pthread_create(&threads[i], NULL, island_thread, &contexts[i]);

  /* Wait for all islands */
  for (int i = 0; i < GA_NUM_ISLANDS; i++)
    pthread_join(threads[i], NULL);

  clock_gettime(CLOCK_MONOTONIC, &t_end);
  double elapsed = (double)(t_end.tv_sec - t_start.tv_sec) +
                   (double)(t_end.tv_nsec - t_start.tv_nsec) / 1e9;

  /* ── Results ── */
  printf("\n  +---------------- Island Stats -----------------+\n");
  for (int i = 0; i < GA_NUM_ISLANDS; i++) {
    printf("  |  Island %d: %6d generations  %s  |\n", i,
           contexts[i].generations_run,
           contexts[i].found_exact ? " ★ EXACT ★ " : "           ");
  }
  printf("  +-----------------------------------------------+\n");
  printf("  |  Elapsed: %.4f seconds                       |\n", elapsed);
  printf("  +-----------------------------------------------+\n");

  /* Print best solution */
  int exact = (g_best.fitness >= -0.5);
  if (exact) {
    printf("\n  ✓ Exact solution found!");
  } else {
    printf("\n  ✗ No exact solution found (best approximation):");
  }
  print_ga_solution(&g_best);

  /* Cleanup */
  for (int i = 0; i < GA_NUM_ISLANDS; i++)
    pthread_mutex_destroy(&g_migration[i].mutex);

  return exact ? 0 : 1;
}
