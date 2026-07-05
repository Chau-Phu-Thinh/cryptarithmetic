/*
 * ga_solver.h — Parallel Genetic Algorithm (Island Model) for Cryptarithmetic
 *
 *   ABC * DE = HGBC   AND   FEC + DEC*10 = HGBC
 *
 * Public API:
 *   ga_solve()  — launch the island-model GA and print the best solution.
 */

#ifndef GA_SOLVER_H
#define GA_SOLVER_H

/* ── Tunable parameters ─────────────────────────────────────────────── */
#define GA_NUM_ISLANDS      4       /* number of parallel islands        */
#define GA_POP_SIZE         200     /* population per island             */
#define GA_MAX_GENERATIONS  50000   /* per-island generation cap         */
#define GA_TOURNAMENT_K     5       /* tournament selection size         */
#define GA_CROSSOVER_RATE   0.85    /* probability of crossover          */
#define GA_MUTATION_RATE    0.15    /* probability of swap-mutation      */
#define GA_ELITE_COUNT      2       /* elitism: keep top-N each gen      */
#define GA_MIGRATION_INTERVAL 100   /* migrate every K generations       */
#define GA_MIGRATION_COUNT  3       /* individuals migrated per event    */
#define GA_STAGNATION_LIMIT 5000    /* stop island if no improvement     */

#define GA_NUM_LETTERS      8       /* A B C D E F G H                   */

/* ── Chromosome ──────────────────────────────────────────────────────── */
/*
 * genes[i] = digit assigned to the i-th letter.
 *   Index mapping:  0=A  1=B  2=C  3=D  4=E  5=F  6=G  7=H
 *
 * All 8 genes must be distinct digits in 0-9.
 * Leading letters (A, D, F, H → indices 0, 3, 5, 7) cannot be 0.
 */
typedef struct {
    int genes[GA_NUM_LETTERS];   /* digit per letter                    */
    double fitness;              /* higher is better (≤ 0)              */
} Chromosome;

/* ── Entry point ─────────────────────────────────────────────────────── */
/*
 * Run the island-model GA.
 * Returns 0 if an exact solution was found, 1 otherwise.
 */
int ga_solve(void);

#endif /* GA_SOLVER_H */
