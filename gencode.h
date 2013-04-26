/**
 * Generating the output C code.
 */

#ifndef _GENCODE_H
#define _GENCODE_H

#include <stdio.h>
#include "statement.h"

#define WRITESOURCE

extern void
write_op_simple(FILE *out, prog_oper *op);

extern void
write_op_linear(FILE *out, prog_oper *op);

extern void
gen_code(FILE *out, prog_tree *tree);

#endif
