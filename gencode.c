#include <assert.h>
#include "gencode.h"

static const char prologue[] = "#include <stdio.h>\n#include <string.h>\n\n";

static const char tape[] = 
"#include <stdlib.h>\n#include <assert.h>\n"
"\n"
"char *tape;\n"
"int size;\n"
"char *end;\n"
"\n"
"static void enlarge(char *p, int r)\n"
"{\n"
"  while (p+r >= end)\n"
"  {\n"
"    int offset = p - tape;\n"
"    int old_size = size;\n"
"    int i;\n"
"    size += size/2;\n"
"    tape  = realloc(tape, size * sizeof(char));\n"
"    assert(tape);\n"
"    end   = tape + size;\n"
"    p     = tape + offset;\n"
"    for (i = old_size; i < size; i++) tape[i] = 0;\n"
"  }\n"
"}\n\n"
"int main()\n"
"{\n"
"  char *p;\n"
"  tape = calloc(%d, sizeof(char));\n"
"  assert(tape);\n"
"  size = %d;\n"
"  end = tape + size;\n"
"  p = tape;\n\n";

static const char notape[] = "int main()\n{\n  char p[%d];\n  memset(p, 0, %d);\n\n";

static const char epilogue[] = "\n  return 0;\n}\n";

static void
pos_print(FILE *out, int offset)
{
  if (offset != 0)
    fprintf(out, "p[%d]", offset);
  else
    fprintf(out, "(*p)");
}

static void
sexp_print(FILE *out, simple_expr *ex)
{
  if (ex->type == 1)
    pos_print(out, ex->offset);
  else
    fprintf(out, "i%d", ex->offset);

  if (ex->delta != 0)
    fprintf(out, "%+d", ex->delta);
}

void
write_op_simple(FILE *out, prog_oper *op)
{
#ifdef WRITESOURCE
  fprintf(out, "/* %d: %s */\n", op->id, op->orig);
#endif

  if (op->cycle_type == 1)
    fprintf(out, "  while (p[%d]) {\n", op->cond_offset);
  else if (op->cycle_type == 2)
    fprintf(out, "  if (p[%d]) {\n", op->cond_offset);

  if (op->nin)
    fprintf(out, "  {\n");

  for (int i = 0; i < op->nin; i++)
    fprintf(out, "  char i%d = getchar();\n", i);

  for (int i = 0; i < op->nout; i++)
  {
    fprintf(out, "  putchar(");
    sexp_print(out, &op->out[i]);
    fprintf(out, ");\n");
  }

  if (op->hi > 0)
    fprintf(out, "  enlarge(p, %d);\n", op->hi);

  for (int i = op->lo; i <= op->hi; i++)
  {
    simple_expr *ex = &op->tape[i - op->lo];

    if (ex->type == 0 || (ex->type == 1 && ex->offset == i && ex->delta == 0))
      continue;

    fprintf(out, "  ");
    pos_print(out, i);

    if (ex->type == 2 || (ex->type == 1 && ex->offset != i))
    {
      fprintf(out, " = ");
      sexp_print(out, ex);

    }
    else
    {
      if (ex->delta < -1)
        fprintf(out, " -= %d", -ex->delta);
      else if (ex->delta == -1)
        fprintf(out, "--");
      else if (ex->delta == 1)
        fprintf(out, "++");
      else
        fprintf(out, " += %d", ex->delta);
    }

    fprintf(out, ";\n");
  }

  if (op->shift < -1)
    fprintf(out, "  p -= %d;\n", -op->shift);
  else if (op->shift == -1)
    fprintf(out, "  p--;\n");
  else if (op->shift == 1)
    fprintf(out, "  p++;\n");
  else if (op->shift > 1)
    fprintf(out, "  p += %d;\n", op->shift);

  if (op->nin)
    fprintf(out, "  }\n");
    
  if (op->back != NULL)
    fprintf(out, "  }\n");

  fprintf(out, "\n");
}

static int
op_need_braces(prog_oper *op)
{
  if (op->back)
    return 0;

  for (int i = 0; i < op->ninv; i++)
    if (op->inv[i].expl)
      return 1;

  return 0;
}

static void
write_inv(FILE *out, input_val *inv, int idx, int *nin)
{
  if (inv->type == INPUT_VAL_TAPE && inv->expl)
    fprintf(out, "  int i%d = p[%d];\n", idx, inv->offset);
  else if (inv->type == INPUT_VAL_IO)
  {
    assert(inv->offset >= *nin);

    while (*nin < inv->offset)
    {
      fprintf(out, "  getchar();\n");
      (*nin)++;
    }

    if (inv->expl)
      fprintf(out, "  int i%d = getchar();\n", idx);
    (*nin)++;
  }
}

/**
 * @return 0: =, 1: ++, 2: +=, -1: --, -2: -=
 */
static int
find_inc(prog_oper *op, output_val *outv)
{
  if (outv->dtype == OUTPUT_VALD_OUT)
    return 0;

  int self   = 0;
  int cconst = 0;
  int neg    = 0;
  int pos    = 0;

  for (int i = 0; i < op->ninv; i++)
  {
    input_val *inv = &op->inv[i];

    if (inv->type == INPUT_VAL_TAPE && inv->offset == outv->dest)
      self   = outv->coef[i];
    else
    {
      if (inv->type == INPUT_VAL_CONST1)
        cconst = outv->coef[i];

      if (outv->coef[i] > 0)
        pos++;
      else if (outv->coef[i] < 0)
        neg++;
    }
  }

  if (self != 1)
    return 0;

  if (neg > pos)
  {
    if (neg == 1 && cconst == -1)
      return -1;
    else
      return -2;
  }
  else
  {
    if (neg == 0 && pos == 1 && cconst == 1)
      return 1;
    else
      return 2;
  }
}

static void
write_item(FILE *out, input_val *inv, int idx, int c, int first, int inc)
{
  fprintf(out, " ");

  if (inc < 0)
    c = -c;
  
  if (c < 0)
  {
    fprintf(out, "- ");  c = -c;
  }
  else if (!first)
    fprintf(out, "+ ");

  if (inv->type == INPUT_VAL_CONST1)
  {
    fprintf(out, "%d", c);
    return;
  }
    
  if (c != 1)
    fprintf(out, "%d*", c);

  if (inv->expl)
    fprintf(out, "i%d", idx);
  else
    if (inv->type == INPUT_VAL_TAPE)
      fprintf(out, "p[%d]", inv->offset);
    else
      fprintf(out, "getchar()");
}

static void
write_outv(FILE *out, prog_oper *op, output_val *outv)
{
  int inc = find_inc(op, outv);

  if (outv->dtype == OUTPUT_VALD_TAPE)
  {
    fprintf(out, "  p[%d]", outv->dest);

    switch (inc)
    {
      case -2: fprintf(out, " -="); break;
      case -1: fprintf(out, "--;\n"); return;
      case  0: fprintf(out, "  ="); break;
      case  1: fprintf(out, "++;\n"); return;
      case  2: fprintf(out, " +="); break;
    }
  }
  else if (outv->dtype == OUTPUT_VALD_OUT)
    fprintf(out, "  putchar(");

  int first = 1;
  int iconst = -1;

  for (int iv = 0; iv < op->ninv; iv++)
    if (outv->coef[iv])
    {
      int c = outv->coef[iv];
      input_val *inv = &op->inv[iv];
    
      if (inv->type == INPUT_VAL_CONST1)
      {
        iconst = iv;
        continue;
      }

      if (inv->type == INPUT_VAL_TAPE && inv->offset == outv->dest && inc)
        continue;
    
      write_item(out, inv, iv, c, first, inc);
   
      first = 0;
    }

  if (iconst >= 0 && (first || outv->coef[iconst]))
    write_item(out, &op->inv[iconst], iconst, outv->coef[iconst], first, inc);
  else if (first)
    fprintf(out, " 0");

  if (outv->dtype == OUTPUT_VALD_TAPE)
    fprintf(out, ";\n");
  else if (outv->dtype == OUTPUT_VALD_OUT)
    fprintf(out, " );\n");
}


void
write_op_linear(FILE *out, prog_oper *op)
{
#ifdef WRITESOURCE
  fprintf(out, "/* %d: %s */\n", op->id, op->orig);
#endif

  if (op->cycle_type == 1)
    fprintf(out, "  while (p[%d]) {\n", op->cond_offset);
  else if (op->cycle_type == 2)
    fprintf(out, "  if (p[%d]) {\n", op->cond_offset);
  
  if (op->ns_start == op && op->rbound > 0 && op->prev != NULL)
    fprintf(out, "  enlarge(p, %d);\n", op->rbound);
	
  if (op_need_braces(op))
    fprintf(out, "  {\n");

  for (int i = 0, nin = 0; i < op->ninv; i++)
    write_inv(out, &op->inv[i], i, &nin);
    
  for (int i = 0; i < op->noutv; i++)
    write_outv(out, op, &op->outv[i]);
  
  if (op->ns_end == op && op->shift && (op->back || op->next))
  {
    if (op->shift < -1)
      fprintf(out, "  p -= %d;\n", -op->shift);
    else if (op->shift == -1)
      fprintf(out, "  p--;\n");
    else if (op->shift == 1)
      fprintf(out, "  p++;\n");
    else if (op->shift > 1)
      fprintf(out, "  p += %d;\n", op->shift);
  }
	
  if (op_need_braces(op))
    fprintf(out, "  }\n");
    
  if (op->back != NULL)
    fprintf(out, "  }\n");
    
  fprintf(out, "\n");
}

void
gen_code(FILE *out, prog_tree *tree)
{
  fprintf(out, prologue);
  
  int start_len = tree->first->rbound + 1;
  if (start_len == 1) start_len = 2;  
  
  if (tree->first->ns_end->next != NULL)
    fprintf(out, tape, start_len, start_len);
  else
    fprintf(out, notape, start_len, start_len);

  for (prog_oper *op = tree->first; op != NULL; op = op->next)
    if (op->type == PROG_OPER_SIMPLE)
      write_op_simple(out, op);
    else if (op->type == PROG_OPER_FINISH)
      fprintf(out, "/* FINISH */\n");
    else if (op->type == PROG_OPER_LINEAR)
      write_op_linear(out, op);
    else if (op->type == PROG_OPER_INFLOOP_COND)
      fprintf(out, "  /* %d: %s */\n  if (p[%d]) { fprintf(stderr, \"\\nInfinite loop!\\n\"); abort(); }\n\n",
              op->id, op->orig, op->inf_loop_cond_offset);

  fprintf(out, epilogue);
}
