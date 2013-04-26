#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "statement.h"

void
sexp_dump(simple_expr *expr)
{
  if (expr->type == 1)
    printf("tape[%d] + %d", expr->offset, expr->delta);
  else
    printf("in[%d] + %d", expr->offset, expr->delta);
}

prog_oper*
poper_create(int id, prog_oper_type type)
{
  prog_oper *op   = malloc(sizeof(prog_oper));
  assert(op);

  op->type        = type;
  op->id          = id;
  op->orig        = NULL;
  op->next        = NULL;
  op->prev        = NULL;
  
  op->lo          = 0;
  op->hi          = 0;
  op->nin         = 0;
  op->nout        = 0;
  op->tape        = NULL;
  op->out         = NULL;

  op->ns_start    = op;
  op->ns_end      = op;
  op->shift       = 0;
  op->rbound	  = 0;
  
  op->ninv        = 0;
  op->noutv       = 0;
  op->inv         = NULL;
  op->outv        = NULL;
  
  op->forw	  = NULL;
  op->back        = NULL;
  op->cond_offset = 0;
  op->cycle_type  = 0;
  op->cycle_shift = 1;		// a priori we can't garantee that the cycle is not shifting

  op->inf_loop_cond_offset = 0;

  return op;
}

/**
 * Deleting the operation. The string of the operation is appended to the end of the string of the previous operation.
 */
void
poper_delete(prog_oper *op)
{
  if (op->orig != NULL)
  {
    if (op->prev != NULL)
    {
      prog_oper *prev = op->prev;
      
      if (prev->orig == NULL)
        prev->orig = op->orig;
      else
      {
        prev->orig = realloc(prev->orig, (strlen(prev->orig) + strlen(op->orig) + 1)*sizeof(char));
        assert(prev->orig != NULL);
        strcat(prev->orig, op->orig);
      }
    }
    free(op->orig);
  }
  
  if (op->tape != NULL) free(op->tape);
  if (op->out  != NULL) free(op->out);
  if (op->inv  != NULL) free(op->inv);
  
  if (op->outv != NULL)
  {
    for (int i = 0; i < op->noutv; i++)
      free(op->outv[i].coef);
    
    free(op->outv);
  }
  
  if (op->ns_start != op && op->ns_end == op)
  {
    prog_oper *old_end = op->ns_end;
    
    for (prog_oper *opi = op->ns_start; opi != NULL && opi->ns_end == old_end; opi = opi->next)
      opi->ns_end = op->prev;
  }
  else if (op->ns_start == op && op->ns_end != op)
  {
    for (prog_oper *opi = op->next; opi != NULL && opi->ns_end == op->ns_end; opi = opi->next)
      opi->ns_start = op->next;
      
    op->next->rbound = op->rbound;
  }
      
  if (op->next != NULL && op->next->prev == op)
    op->next->prev = op->prev;
  
  if (op->prev != NULL && op->prev->next == op)
    op->prev->next = op->next;
  
  if (op->forw != NULL && op->forw != op && op->forw->back == op)
    assert(0);
  
  if (op->back != NULL && op->back != op && op->back->forw == op)
    assert(0);

  assert(op->prev == NULL || op->prev->ns_end != op);
  assert(op->next == NULL || op->next->ns_start != op);

  free(op);
}

static const char *prog_oper_name[] = {
  "PROG_OPER_VOID",
  "PROG_OPER_SIMPLE",
  "PROG_OPER_LINEAR",
  "PROG_OPER_FINISH",
  "PROG_OPER_INFLOOP_COND",
  "PROG_OPER_ERROR"
};

void
poper_dump(prog_oper *op)
{
  printf("========dump=========\n");
  printf("%d ==> %d (%s): '%s' ==> %d\n",
         op->prev == NULL ? 0 : op->prev->id,
         op->id, prog_oper_name[op->type], op->orig,
         op->next == NULL ? 0 : op->next->id);

  if (op->type == PROG_OPER_INFLOOP_COND)
  {
    printf("inf_loop_cond_offset = %d\n", op->inf_loop_cond_offset);
    return;
  }
  else if (op->type != PROG_OPER_SIMPLE && op->type != PROG_OPER_LINEAR)
    return;

  printf("lo, hi = [%d, %d], nin = %d, nout = %d\n", op->lo, op->hi, op->nin, op->nout);

  assert(op->ns_start != NULL);
  assert(op->ns_end != NULL);
  printf("ns: [%d, %d]", op->ns_start->id, op->ns_end->id);

  if (op->ns_start == op)
    printf(", rbound = %d", op->rbound);

  if (op->ns_end == op)
    printf(", shift =  %d", op->shift);

  printf("\n");

  if (op->cycle_type)
    printf("cycle_type: %d, by p[%d], to %d, ifshifting %d\n",
           op->cycle_type, op->cond_offset, op->forw->id, op->cycle_shift);

  if (op->back)
    printf("cycle end: %d\n", op->back->id);

  if (op->type == PROG_OPER_SIMPLE)
  {
    for (int i = 0; i < op->nout; i++)
    {
      printf("out[%d] = ", i);
      sexp_dump(&op->out[i]);
      printf("\n");
    }

    for (int i = op->lo; i <= op->hi; i++)
    {
      printf("tape[%d] = ", i);
      sexp_dump(&op->tape[i-op->lo]);
      printf("\n");
    }
  }
  else
  {
    printf("\t");
    for (int i = 0; i < op->ninv; i++)
    {
      input_val *inv = &op->inv[i];
      switch (inv->type)
      {
        case INPUT_VAL_TAPE:
          printf("t[%d]%c\t", inv->offset, inv->expl ? '*' : ' ');
          break;

        case INPUT_VAL_IO:
          printf("in[%d]%c\t", inv->offset, inv->expl ? '*' : ' ');
          break;

        case INPUT_VAL_CONST1:
          printf("const\t");
          break;

        default:
          assert(0);
      }
    }
    printf("\n");

    for (int i = 0; i < op->noutv; i++)
    {
      output_val *outv = &op->outv[i];

      assert(outv->type == OUTPUT_VAL_LIN);

      if (outv->dtype == OUTPUT_VALD_OUT)
        printf("out[%d]\t", outv->dest);
      else if (outv->dtype == OUTPUT_VALD_TAPE)
        printf("t[%d]\t", outv->dest);

      for (int j = 0; j < op->ninv; j++)
        printf("%d\t", outv->coef[j]);

      printf("\n");
    }
  }

  printf("======end dump=======\n");
}

void
ptree_add_poper(prog_tree *tree, prog_oper *op)
{
  if (tree->first != NULL && tree->last != NULL)
  {
    op->prev = tree->last;
    tree->last->next = op;
    tree->last = op;
  }
  else
  {
    assert(tree->first == NULL);
    assert(tree->last  == NULL);

    tree->first = op;
    tree->last  = op;
  }
}

/**
 * By this function we not only fill the op->inv array, but also calculate the op->noutv.
 */
static void
poper_gen_inv(prog_oper *op)
{
  char in_used[op->nin], tape_used[op->hi - op->lo + 1];
  
  for (int i = 0; i < op->nin; i++)
    in_used[i] = 0;
    
  for (int i = 0; i < op->hi - op->lo + 1; i++)
    tape_used[i] = 0;
    
  op->ninv = 1;
    
  for (int i = 0; i < op->nout; i++)
    if (op->out[i].type == 1)
    {
      int t = op->out[i].offset - op->lo;
      
      if (!tape_used[t])
      {
        tape_used[t] = 1;
        op->ninv++;
      }
    }
    else if (op->out[i].type == 2)
    {
      int t = op->out[i].offset;
      
      if (!in_used[t])
      {     
        in_used[t] = 1;
        op->ninv++;
      }
    }

  op->noutv = op->nout;

  for (int i = 0; i < op->hi - op->lo + 1; i++)
    if (op->tape[i].type == 1 && (op->tape[i].offset != i+op->lo || op->tape[i].delta != 0))
    {
      int t = op->tape[i].offset - op->lo;
      
      if (!tape_used[t])
      {
        tape_used[t] = 1;
        op->ninv++;
      }
      
      op->noutv++;
    }
    else if (op->tape[i].type == 2)
    {
      int t = op->tape[i].offset;
      
      if (!in_used[t])
      {     
        in_used[t] = 1;
        op->ninv++;
      }

      op->noutv++;
    }
    
  op->inv  = malloc(sizeof(input_val)  * op->ninv);

  op->inv[0].type = INPUT_VAL_CONST1;
 
  int ind = 1;

  for (int i = 0; i < op->nin; i++)
    if (in_used[i])
    {
      op->inv[ind].type   = INPUT_VAL_IO;
      op->inv[ind].offset = i;
      ind++;
    }
    
  for (int i = 0; i < op->hi - op->lo + 1; i++)
    if (tape_used[i])
    {
      op->inv[ind].type   = INPUT_VAL_TAPE;
      op->inv[ind].offset = i + op->lo;
      ind++;
    }
    
  assert(ind == op->ninv);
}

static void
sexp2out_val(simple_expr *sexp, prog_oper *op, int ind, int dtype, int dest)
{
  output_val *oval = &op->outv[ind];
      
  oval->dtype = dtype;
  oval->dest  = dest;
  oval->type  = OUTPUT_VAL_LIN;
  oval->coef  = calloc(op->ninv, sizeof(int));
      
  int iv;

  for (iv = 0; iv < op->ninv; iv++)
    if (sexp->type == op->inv[iv].type  && sexp->offset == op->inv[iv].offset)
      break;
           
  assert(iv < op->ninv);
        
  oval->coef[iv]       = 1;

  assert(op->inv[0].type == INPUT_VAL_CONST1);
  oval->coef[0] = sexp->delta;

  sexp->type   = INPUT_VAL_OUTV;
  sexp->offset = ind;
}

/**
 * Before calling this function op->ninv, op->inv, op->noutv should be filled. The function creates op->noutv 
 * and changes the types of all elements of op->out and op->tape to either 0, or 4.
 */
static void
poper_gen_outv(prog_oper *op)
{
  op->outv = malloc(sizeof(output_val) * op->noutv);
  
  int ind = 0;
  
  for (int i = 0; i < op->nout; i++)
  {
    simple_expr *sexp = &op->out[i];
  
    if (sexp->type == INPUT_VAL_TAPE || sexp->type == INPUT_VAL_IO)
      sexp2out_val(sexp, op, ind++, OUTPUT_VALD_OUT, i);
  }
  
  for (int i = 0; i < op->hi - op->lo + 1; i++)
  {
    simple_expr *sexp = &op->tape[i];
    
    if (sexp->type == INPUT_VAL_TAPE && sexp->offset == op->lo + i && sexp->delta == 0)
      sexp->type = INPUT_VAL_VOID;
  
    if (sexp->type == INPUT_VAL_TAPE || sexp->type == INPUT_VAL_IO)
      sexp2out_val(sexp, op, ind++, OUTPUT_VALD_TAPE, op->lo + i);
  }
  
  assert(ind == op->noutv);  
}

void
poper_simple2linear(prog_oper *op)
{
  if (op->type != PROG_OPER_SIMPLE) return;

  op->type = PROG_OPER_LINEAR;

  poper_gen_inv(op);
  poper_gen_outv(op);
}

void
ptree_simple2linear(prog_tree *tree)
{
  for (prog_oper *op = tree->first; op != NULL; op = op->next)
    poper_simple2linear(op);
}

