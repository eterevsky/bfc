/**
 * Optimize the program.
 */

#ifndef _OPTIMIZE_H
#define _OPTIMIZE_H

#include "statement.h"

extern int
poper_unroll_loop(prog_oper *op);

extern int
poper_move_shift(prog_oper *op);

extern int
poper_join(prog_oper *op);

extern void
ptree_optimize(prog_tree *tree);


#endif
