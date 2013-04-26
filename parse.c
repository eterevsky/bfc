#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "tools.h"

/**
 * @param op current operation
 *
 * We presume, that op->orig has the operation substring, op->lo, op->hi, op->in, op->out are already calculated.
 */
static void
parse_do_arith(prog_oper *op)
{
  op->tape = malloc(sizeof(simple_expr) * (op->hi - op->lo + 1));

  for (int i = 0; i < op->hi - op->lo + 1; i++)
  {
    op->tape[i].type   = 1;
    op->tape[i].offset = i + op->lo;
    op->tape[i].delta  = 0;
  }

  if (op->nout > 0)
    op->out = malloc(sizeof(simple_expr) * op->nout);

  int off  = 0;
  int nin  = 0;
  int nout = 0;

  for (char *c = op->orig; *c != '\0'; c++)
    switch (*c)
    {
      case '+': op->tape[off - op->lo].delta++; break;
      case '-': op->tape[off - op->lo].delta--; break;
      case '>': off++; break;
      case '<': off--; break;
      case '.': op->out[nout++] = op->tape[off - op->lo]; break;

      case ',':
      {
        simple_expr *expr = &op->tape[off - op->lo];
        expr->type   = 2;
        expr->offset = nin++;
        expr->delta  = 0;
        break;
      }
    }
}

static void
parse_fill_jmps(prog_oper *op, arr_p *back_stack)
{
  if (op->forw != NULL)
    arr_p_push(back_stack, op);
  
  if (op->back != NULL)
  {
    if (back_stack->size == 0)
    {
      fprintf(stderr, "error: ] without [");
      abort();
    }
    
    op->back = arr_p_pop(back_stack);
    
    assert(op->back->forw != NULL);
    
    op->back->forw = op;
    op->back->cycle_type = 1;
  }
}

prog_tree*
parse(FILE *in)
{
  prog_tree *tree = malloc(sizeof(prog_tree));
  tree->error   = poper_create(-3, PROG_OPER_ERROR);
  tree->first   = NULL;
  tree->last    = NULL;

  arr_c *buffer     = arr_c_create();
  arr_p *back_stack = arr_p_create();

  int togo = 1;

  // skipping until first significient character
  while (1)
  {
    int c = getc(in);

    if (c == EOF)
    {
      togo = 0;
      break;
    }

    if (strchr("+-<>[].,", c) != NULL)
    {
      ungetc(c, in);
      break;
    }
  }

  int n = 0;

  while (togo)
  {
    // Here we know that there is at least one operation left.
    prog_oper *op = poper_create(++n, PROG_OPER_SIMPLE);

    int step   = 1;
    int offset = 0;		// current offset

    while (step < 5)
    {
      int c = getc(in);

      if (c == EOF)
      {
        togo = 0;
        break;
      }

      switch (c)
      {
        case '[':
          if (step > 1)
          {
            ungetc(c, in);
            step = 5;
            break;
          }

          step = 2;
          arr_c_push(buffer, c);
          op->forw = tree->error;
          break;

        case ']':
          step = 5;
          arr_c_push(buffer, c);
          op->back = tree->error;
          break;

        case '+': case '-': case '<': case '>':
          if (step > 3)
          {
            ungetc(c, in);
            step = 5;
            break;
          }
          
          if (step < 2)
            step = 2;

          arr_c_push(buffer, c);

          if (c == '<')
          {
            offset--;
            if (offset < op->lo)
              op->lo = offset;
          }
          else if (c == '>')
          {
            offset++;
            if (offset > op->hi)
              op->hi = offset;
          }
          break;

        case ',':
          if (step > 2)
          {
            ungetc(c, in);
            step = 5;
            break;
          }
          step = 2;
        
          arr_c_push(buffer, c);
          op->nin++;
          break;

        case '.':
          if (step > 3)
          {
            ungetc(c, in);
            step = 5;
            break;
          }
          step = 3;

          arr_c_push(buffer, c);

          op->nout++;
          break;
      }
    }

	  op->orig = malloc(sizeof(char) * (buffer->size + 1));
    assert(op->orig != NULL);
    memcpy(op->orig, buffer->val, buffer->size);
    op->orig[buffer->size] = '\0';
	  
    arr_c_clear(buffer);

	  op->shift  = offset;
	  op->rbound = op->hi; 
	  
// At this point we've filled op->orig, op->nstarts, op->lo, op->hi, op->nin, op->nout,
// op->shift

    ptree_add_poper(tree, op);
    parse_do_arith(op);
    parse_fill_jmps(op, back_stack);
  }

  if (back_stack->size > 0)
  {
    fprintf(stderr, "error: [ without ]\n");
    abort();
  }

  arr_c_delete(buffer);
  arr_p_delete(back_stack);

//  ptree_add_poper(tree, poper_create(-1, PROG_OPER_FINISH));

#ifdef DUMP
  {
    prog_oper *op = tree->first;
    while (op)
    {
      poper_dump(op);
      op = op->next;
    }
  }
#endif

  return tree;
}

