/*
 * ga_main.c — Entry point for the Parallel GA cryptarithmetic solver.
 *
 * Build:
 *   make ga
 *
 * Run:
 *   ./bin/ga
 */

#include "ga_solver.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("+-----------------------------------------+\n");
    printf("|      Cryptarithmetic — GA Solver         |\n");
    printf("+-----------------------------------------+\n");
    printf("|  Puzzle:                                 |\n");
    printf("|    ABC * DE  = HGBC                      |\n");
    printf("|    FEC + DEC*10 = HGBC                   |\n");
    printf("|                                          |\n");
    printf("|  Letters: A B C D E F G H  (8 unique)    |\n");
    printf("|  Leading (no zero): A, D, F, H           |\n");
    printf("+-----------------------------------------+\n");

    int rc = ga_solve();
    return rc;
}
