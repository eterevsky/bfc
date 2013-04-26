/* BrainFuck compiler v. 2.3
   (c) 2006, Oleg Eterevsky */

#include <stdio.h>
#include <stdlib.h>

#include "statement.h"
#include "parse.h"
#include "gencode.h"
#include "optimize.h"

int
main(int argn, const char **argv)
{
  if (argn < 3)
  {
    printf("Usage: bfc <input bf-file> <output c-file>\n");
    exit(0);
  }

  FILE *in = fopen(argv[1], "r");
  prog_tree *tree = parse(in);
  fclose(in);
  
  ptree_simple2linear(tree);
  
  ptree_optimize(tree);

  FILE *out = fopen(argv[2], "w");
  gen_code(out, tree);
  fclose(out);

  return 0;
}
