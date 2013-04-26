/**
 * Parsing the input into series of statements of PROG_OPER_SIMPLE type.
 */

#ifndef _PARSE_H
#define _PARSE_H

#include <stdio.h>
#include "statement.h"

extern prog_tree*
parse(FILE *in);

#endif
