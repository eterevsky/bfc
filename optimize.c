#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "optimize.h"
#include "statement.h"

/**
 * @return 0 if no changes, 1 if changed.
 */
int
poper_unroll_loop(prog_oper *op)
{
  if (op->type != PROG_OPER_LINEAR) return 0;
  if (op->cycle_type != 1) return 0;
  if (op->forw != op || op->back != op) return 0;
  if (op->cycle_shift != 0) return 0;
  if (op->nin || op->nout)
    return 0;
 
  output_val *ov_ind = NULL;
  for (int i = 0; i < op->noutv; i++)
    if (op->outv[i].dtype == OUTPUT_VALD_TAPE && op->outv[i].dest == op->cond_offset)
    {
      ov_ind = &op->outv[i];
      break;
    }
    
  if (ov_ind == NULL)
  {
    op->type = PROG_OPER_INFLOOP_COND;
    op->inf_loop_cond_offset = op->cond_offset;
    return 1;
  }
    
  int in_ind = -1, in_const = -1;
  for (int i = 0; i < op->ninv; i++)
    if (op->inv[i].type == INPUT_VAL_CONST1)
    {
      assert(in_const < 0);
      in_const = i;
    }
    else if (op->inv[i].type == INPUT_VAL_TAPE && op->inv[i].offset == op->cond_offset)
    {
      assert(in_ind < 0);
      in_ind = i;
    }
    else if (ov_ind->coef[i] != 0)
      return 0;
      
  if (in_ind < 0 || ov_ind->coef[in_ind] == 0)
  {
    if (in_const < 0 || ov_ind->coef[in_const] == 0)
    {
      op->cycle_type = 2;
      return 1;
    }
    else
    {
      op->type = PROG_OPER_INFLOOP_COND;
      op->inf_loop_cond_offset = op->cond_offset;
      return 1;
    }
  }
      
  if (in_const < 0 || ov_ind->coef[in_const] == 0)
  {
    if (ov_ind->coef[in_ind] & 2)	// we presume that the modulo is the power of 2
    {
      op->type = PROG_OPER_INFLOOP_COND;
      op->inf_loop_cond_offset = op->cond_offset;
      return 1;
    }
    else return 0;    
  }
  
  if (ov_ind->coef[in_ind] != 1)
    return 0;
    
  if (ov_ind->coef[in_const] != 1 && ov_ind->coef[in_const] != -1)
    return 0;
    
  int sign = -ov_ind->coef[in_const];
  int own[op->noutv];
    
  for (int i = 0; i < op->noutv; i++)
  {
    assert(op->outv[i].dtype == OUTPUT_VALD_TAPE);
    assert(op->outv[i].type == OUTPUT_VAL_LIN);
    
    own[i] = 0;
  
    for (int j = 0; j < op->ninv; j++)
      if (op->inv[j].type == INPUT_VAL_TAPE)
      {
        if (op->inv[j].offset != op->outv[i].dest)
        {
          if (op->outv[i].coef[j] != 0) return 0;
        }
        else
          own[i] = op->outv[i].coef[j];
      }
      
    if (own[i] != 0 && own[i] != 1) return 0;
  }
  
  int has_const = 0;
   
  for (int i = 0; i < op->noutv; i++)
    if (own[i] == 1)
    {
      if (op->outv[i].dest == op->cond_offset)
        op->outv[i].coef[in_ind] = 0;
      else
        op->outv[i].coef[in_ind] = sign * op->outv[i].coef[in_const];
  
      op->outv[i].coef[in_const] = 0;
    }
    else if (own[i] == 0)
    {
      op->outv[i].coef[in_ind] = 0;
      has_const = 1;
    }
  
  if (has_const)
    op->cycle_type = 2;
  else
  {
    op->forw = NULL;
    op->back = NULL;
    op->cycle_type = 0;
  }
  
  return 1;
}

int
poper_join(prog_oper *op)
{
  prog_oper *next = op->next;
  
  if (op->type != PROG_OPER_LINEAR) return 0;
  
  if (next == NULL) return 0;

  if (next->type == PROG_OPER_FINISH)
  {
    assert(next->next == NULL);
    assert(next->back == NULL && op->back == NULL);
    poper_delete(next);
    return 1;
  }
  
  if (op->back || next->forw) return 0;
  if (op->ns_start != next->ns_start) return 0;
  if (op->nout != 0 && next->nin != 0) return 0;
  
  //printf("nin: %d, %d\n", op->nin, next->nin);
  
  if (op->lo > next->lo)
    op->lo = next->lo;
  
  if (op->hi < next->hi)
    op->hi = next->hi;
  
  if (op->ns_end == next)
  {
    op->shift = next->shift;
    
    for (prog_oper *opi = op->ns_start; opi != next; opi = opi->next)
      opi->ns_end = op;
  }
  
  if (op->tape != NULL) { free(op->tape); op->tape = NULL; }
  if (op->out  != NULL) { free(op->out); op->out = NULL; }
  
  for (int i = 0; i < next->noutv; i++)
    if (next->outv[i].dtype == OUTPUT_VALD_OUT)
      next->outv[i].dest += op->nout;
  
  if (next->back != NULL)
  {
    op->back = next->back;
    op->back->forw = op;
  }
  
  op->next = next->next;
  if (op->next != NULL)
    op->next->prev = op;
  
  /* There are several types of inv in the second operation:
    - constant (should be changed to constant in first inv[])
    - io-input (will be left in the new inv[] array)
    - tape input, that was not touched during first operation (also will be left unchanged)
    - tape input, that appeares in the output of first operation (will be dropped, the value substituted)
    - tape input, that appeares only in the input of first operation (substituted)
   */
  
  int n_add_inv = 0;
  
  for (int i = 0; i < next->ninv; i++)
  {
    input_val *inv = &next->inv[i];

    if (inv->type == INPUT_VAL_TAPE)
    {
      int j;
      for (j = 0; j < op->noutv; j++)
        if (op->outv[j].dtype == OUTPUT_VALD_TAPE && op->outv[j].dest == inv->offset)
          break;

      if (j < op->noutv)
      {
        inv->type   = INPUT_VAL_OUTV_PREV;
        inv->offset = j;
        continue;
      }

      for (j = 0; j < op->ninv; j++)
        if (op->inv[j].type == INPUT_VAL_TAPE && op->inv[j].offset == inv->offset)
          break;
      
      if (j < op->ninv)
      {
        inv->type   = INPUT_VAL_INV_PREV;
        inv->offset = j;
        continue;
      }
      
      n_add_inv++;
    }
    else if (inv->type == INPUT_VAL_IO)
      n_add_inv++;
  }

  if (n_add_inv > 0)
  {
    // Enlarging the coef's in first operations because of the growth of inv[]
    for (int i = 0; i < op->noutv; i++)
    {
      output_val *outv = &op->outv[i];
      outv->coef = realloc(outv->coef, (op->ninv + n_add_inv)*sizeof(int));
      assert(outv->coef);
  
      for (int j = op->ninv; j < op->ninv + n_add_inv; j++)
        outv->coef[j] = 0;
    }
  
    // Enlarging inv[] in first operation
    assert(op->inv != NULL);
    op->inv = realloc(op->inv, (op->ninv + n_add_inv) * sizeof(input_val));
    assert(op->inv);
  }

  // Adding inv from second operation and setting the correspondence.
  for (int i = 0, nadded = 0; i < next->ninv; i++)
  {
    input_val *inv = &next->inv[i];
    
    if (inv->type == INPUT_VAL_CONST1)
    {
      int ind_const = -1;

      for (int i = 0; i < op->ninv; i++)
        if (op->inv[i].type == INPUT_VAL_CONST1)
        {
          ind_const = i;
          break;
        }

      assert(ind_const >= 0);

      inv->type   = INPUT_VAL_INV_PREV;
      inv->offset = ind_const;
    }
    else if (inv->type == INPUT_VAL_IO || inv->type == INPUT_VAL_TAPE)
    {
      input_val *new_inv = &op->inv[op->ninv + nadded];

      if (inv->type == INPUT_VAL_IO)
      {
        new_inv->type   = INPUT_VAL_IO;
        new_inv->offset = inv->offset + op->nin;
        //        printf("io offset: %d + %d = %d\n", inv->offset, op->nin, new_inv->offset);
      }
      else
      {
        new_inv->type   = INPUT_VAL_TAPE;
        new_inv->offset = inv->offset;
      }

      inv->type   = INPUT_VAL_INV_PREV;
      inv->offset = op->ninv + nadded;
      nadded++;
    }
  }

  op->ninv += n_add_inv;
  
  op->nin += next->nin;  // We can't do it before, because in the previous loop we renumber the inputs.
  op->nout += next->nout;
  
  int n_add_outv = 0;
  int outv_corr[next->noutv];   // The correspondence between next->noutv and op->noutv.

  // Renewing outv in second operation
  for (int i = 0; i < next->noutv; i++)
  {
    output_val *outv = &next->outv[i];

    outv_corr[i] = -1;
    
    if (outv->dtype == OUTPUT_VALD_OUT)
      n_add_outv++;
    else if (outv->dtype == OUTPUT_VALD_TAPE)
    {
      for (int j = 0; j < op->noutv; j++)
        if (op->outv[j].dtype == OUTPUT_VALD_TAPE && op->outv[j].dest == outv->dest)
        {
          outv_corr[i] = j;
          break;
        }

      if (outv_corr[i] < 0) n_add_outv++;
    }
    else assert(0);

    int *new_coef = calloc(sizeof(int), op->ninv);

    for (int j = 0; j < next->ninv; j++)
      if (outv->coef[j])
      {
        if (next->inv[j].type == INPUT_VAL_INV_PREV)
          new_coef[next->inv[j].offset] += outv->coef[j];
        else if (next->inv[j].type == INPUT_VAL_OUTV_PREV)
        {
          output_val *old_outv = &op->outv[next->inv[j].offset];
          
          for (int k = 0; k < op->ninv; k++)
            new_coef[k] += outv->coef[j] * old_outv->coef[k];
        }
        else assert(0);
      }

    free(outv->coef);
    outv->coef = new_coef;
  }

  op->outv = realloc(op->outv, sizeof(output_val) * (op->noutv + n_add_outv));

// Adding/replacing outv from second operation
  for (int i = 0, nadded = 0; i < next->noutv; i++)
  {
    if (outv_corr[i] >= 0)  // replacing
    {
      free(op->outv[outv_corr[i]].coef);
      op->outv[outv_corr[i]] = next->outv[i];
    }
    else
      op->outv[op->noutv + (nadded++)] = next->outv[i];
    
    next->outv[i].coef = NULL;  // So that new coef array whouldn't be deleted while deleting second op
  }

  op->noutv += n_add_outv;

  poper_delete(next);

  return 1;
}

static int
cycle_test_shift(prog_oper *start)
{
  if (!start->cycle_type) return 0;
  
  start->cycle_shift = 0;
  
  int shift = 0;
  prog_oper *finish = start->forw;
  prog_oper *op = start;
  
  while (op != finish->next)
    if (op != start && op->cycle_type)
    {
      if (cycle_test_shift(op))
        start->cycle_shift = 1;		// We do not do break here beacause we should 
        				// go recursively through all the cycles and 
        				// set the flags for them.
        
      op = op->forw->next;
    }
    else
    {
      if (op->ns_end == op)
        shift += op->shift;
        
      op = op->next;
    }
    
  if (shift)
    start->cycle_shift = 1;
    
  return start->cycle_shift;
}

static void
finish_ns(prog_oper *op, int *cur_shift)
{
  op->shift += *cur_shift;
  *cur_shift = 0;
  
  int rbound = 0;
  
  for (prog_oper *opi = op->ns_start; opi != op->next; opi = opi->next)
    if (opi->hi > rbound)
      rbound = opi->hi;
      
  if (op->next != NULL && op->next->cycle_type && rbound < op->next->cond_offset)
    rbound = op->next->cond_offset;
    
  op->ns_start->rbound = rbound;
//  printf("op = %d, ns_start = %d, rbound = %d\n", op->id, op->ns_start->id, rbound);
}

static void
poper_apply_shift(prog_oper *op, int shift)
{
  op->lo += shift;
  op->hi += shift;
  
  for (int i = 0; i < op->hi - op->lo + 1; i++)
    if (op->tape[i].type == INPUT_VAL_TAPE)
      op->tape[i].offset += shift;
      
  for (int i = 0; i < op->nout; i++)
    if (op->out[i].type == INPUT_VAL_TAPE)
      op->out[i].offset += shift;
      
  for (int i = 0; i < op->ninv; i++)
    if (op->inv[i].type == INPUT_VAL_TAPE)
      op->inv[i].offset += shift;
      
  for (int i = 0; i < op->noutv; i++)
    if (op->outv[i].dtype == OUTPUT_VALD_TAPE)
      op->outv[i].dest += shift;
      
  op->cond_offset += shift;
  op->inf_loop_cond_offset += shift;
}

static void
try_merge_ns(prog_oper *op, int *cur_shift)
{
  if (op->ns_start == op->next->ns_end) return;
  
// The no-shift section is finished at the beginning or end
// of the cycle with non-zero shift.
  if (op->next->cycle_type && op->next->cycle_shift)
  {
    if (op->back != NULL) // In this case we should insert shift as a separate statement
    { 
      prog_oper *ops = poper_create(-op->id, PROG_OPER_LINEAR);
      ops->prev       = op;
      ops->next       = op->next;
      ops->next->prev = ops;
      op->next        = ops;
      ops->ns_start   = op->ns_start;
      
      for (prog_oper *opi = op->ns_start; opi != ops; opi = opi->next)
        opi->ns_end = ops;
       
      return; 	// The shift will be applyed to this new operation on the next step of outer loop
    }
    
    finish_ns(op, cur_shift);
  }
  else if (op->back != NULL && op->back->cycle_shift) 	
  {
    finish_ns(op, cur_shift);
  }
  else
  {
    *cur_shift += op->shift;
    op->shift = 0;
    
    for (prog_oper *opi = op->next; opi != op->next->ns_end->next; opi = opi->next)
    {
      if (cur_shift)
	poper_apply_shift(opi, *cur_shift);
	
      opi->ns_start = op->ns_start;
    }
    
    for (prog_oper *opi = op->ns_start; opi != op->next; opi = opi->next)
      opi->ns_end = op->next->ns_end;
  }
}

void
opt_gen_ns(prog_tree *tree)
{
  prog_oper *op = tree->first;
  
  while (op != NULL)
    if (op->cycle_type)
    {
      cycle_test_shift(op);
      op = op->forw->next;
    }
    else
      op = op->next;
      
  int cur_shift = 0;
      
  op = tree->first;
  
  while (op != NULL && op->next != NULL)
  {
    try_merge_ns(op, &cur_shift);
    op = op->next;
  }
      
  if (op != NULL && op->next == NULL)
    finish_ns(op, &cur_shift);
}

int
poper_empty(prog_oper *op)
{
  if (op->type == PROG_OPER_VOID) return 1;
  if (op->type != PROG_OPER_LINEAR) return 0;
  if (op->ns_end == op && op->shift) return 0;
  if (op->ns_start == op && op->rbound) return 0;
  if (op->nin || op->nout) return 0;
  if (op->cycle_type) return 0;
  if (op->back && op->cycle_shift) return 0;
  
  for (int i = 0; i < op->noutv; i++)
  {
    output_val *ov = &op->outv[i];
    for (int j = 0; j < op->ninv; j++)
    {
      input_val *iv = &op->inv[j];
      if ((iv->type != INPUT_VAL_TAPE && ov->coef[i] != 0) ||
          (iv->offset != ov->dest && ov->coef[i] != 0) ||
          (iv->offset == ov->dest && ov->coef[i] != 1))
        return 0;
    }
  }
  
  return 1;
}

void
poper_find_exp_inv(prog_oper *op)
{
  for (int i = 0; i < op->ninv; i++)
  {
    input_val *iv = &op->inv[i];

    iv->expl = 0;
    
    switch (iv->type)
    {
      case INPUT_VAL_CONST1:
        break;

      case INPUT_VAL_IO:
      {
        if (iv->offset != op->nin - 1)
        {
          iv->expl = 1;
          break;
        }

        int nappear = 0;
        for (int j = 0; j < op->noutv; j++)
          if (op->outv[j].coef[i])
            nappear++;

        if (nappear != 1)
          iv->expl = 1;
        break;
      }

      case INPUT_VAL_TAPE:
      {
        int changed = 0;
    
        for (int j = 0; j < op->noutv; j++)
        {
          output_val *ov = &op->outv[j];
    
          if (changed && ov->coef[i])
            iv->expl = 1;
    
          if (ov->dtype == OUTPUT_VALD_TAPE && iv->offset == ov->dest)
            changed = 1;
        }

        break;
      }

      default:
        assert(0);
    }
  }
}

void
ptree_optimize(prog_tree *tree)
{
  int changed;
  
  opt_gen_ns(tree);
    
  do {
    changed = 0;
  
    for (prog_oper *op = tree->first; op != NULL; op = op->next)
      if (poper_unroll_loop(op))
        changed++;
        
    printf("Unrolled %d loops.\n", changed);
    
    int old_ch = changed;

    old_ch = changed;

    for (prog_oper *op = tree->first; op != NULL; op = op->next)
      while (poper_join(op))
	changed++;
    
    printf("Join %d instructions.\n", changed - old_ch);
    
    int simp = 0, del = 0;
    
    prog_oper *op = tree->first;
    while (op != NULL)
    {
//      if (poper_simplify(op))
//        simp++;
        
      if (poper_empty(op))
      {
//        printf("Deleting statement:\n");
//        poper_dump(op);

        prog_oper *del_op = op;
        op = op->next;
        poper_delete(del_op);
        del++;
      }
      else
      {
        poper_find_exp_inv(op);
        op = op->next;
      }
    }
    
    printf("%d instructions simplified, %d instructions deleted.\n", simp, del);
  } while (changed);
  
}

