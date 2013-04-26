/**
 * This file define the structures handling the high-level statements, 
 * of which the program is composed.
 */

#ifndef _STATEMENT_H
#define _STATEMENT_H

/**
 * The type of input variables for the statement.
 */
typedef enum input_val_type
{
  INPUT_VAL_VOID      = 0,
  INPUT_VAL_TAPE      = 1,
  INPUT_VAL_IO        = 2,
  INPUT_VAL_CONST1    = 3,
  INPUT_VAL_OUTV      = 4,
  INPUT_VAL_INV_PREV  = 5,  // used during poper_join
  INPUT_VAL_OUTV_PREV = 6,  // used during poper_join
} input_val_type;

typedef struct simple_expr
{
  int			offset;
  int			delta;
  input_val_type	type;		// can take values 0, 1, 2, 4.
} simple_expr;

extern void
sexp_dump(simple_expr *expr);

typedef struct input_val
{
  input_val_type	type;		// can take value 1, 2, 3
  short			expl;		// if we should explicitly write int ix = ...
  int			offset;		// offset in the tape or the index of input char
} input_val;

typedef enum output_val_dest
{
  OUTPUT_VALD_OUT,
  OUTPUT_VALD_TAPE
} output_val_dest;

typedef enum output_val_type
{
  OUTPUT_VAL_LIN
} output_val_type;

typedef struct output_val
{
  output_val_dest	dtype;
  int			dest;
  
  output_val_type	type;
  
  int			*coef;
} output_val;

typedef enum prog_oper_type
{
  PROG_OPER_VOID = 0,
  PROG_OPER_SIMPLE = 1,			// the expressions are in tape and i/o
  PROG_OPER_LINEAR = 2,			// inv and outv are filled
  PROG_OPER_FINISH = 3,
  PROG_OPER_INFLOOP_COND = 4,		// conditional infinite loop
  PROG_OPER_ERROR = 5
} prog_oper_type;

/**
 * One statement. Initialy parsed to PROG_OPER_SIMPLE form. Then during optimizations 
 * usualy is converted to PROG_OPER_LINEAR type.
 */
typedef struct prog_oper
{
  prog_oper_type	type;
  int			id;

  char			*orig;		// original brainfuck string

  struct prog_oper	*next;		// Next operation
  struct prog_oper	*prev;		// Previous operation (in the code)
  
// input & output in the form of simple_expr
  int			lo, hi;		// the leftmost and rightmost positions
  					// that we reach during the operation
  
  int			nin, nout;	// number of input and output characters
  
  simple_expr		*tape;		// New values in the tape. Has (-lo + hi + 1)
					// elements, that define the new values of the tape
					// cells from p-lo to p+lo, where p is the initial
					// position of the pointer.
  simple_expr		*out;		// the sequence of the input and output operations.
  					// for input operations we write type=3 here, for output --
  					// the actual value

// shifts
  struct prog_oper      *ns_start;      // start of the non-shifting block
  struct prog_oper      *ns_end;        // end of the non-shifting block
  int                   shift;          // shift (for the last operation in the non-shifting block)
  int			rbound;
  
// input & output in the linear form
  int			ninv;
  input_val		*inv;
  
  int			noutv;
  output_val		*outv;

// cycles
  struct prog_oper	*forw;		// ']' matching current '[' or NULL if no '[' in the beginning
  struct prog_oper	*back;		// '[' matching current ']' or NULL if no ']'
  
  int			cond_offset;
  int			cycle_type;	// type of cycle
                                        // 0 is equivalent to forw == NULL (no starting cycle)
                                        // 1: while (...) {
                                        // 2: if (...) {
  int			cycle_shift;	// Is sensible only when forw != NULL. If 0 the loop has 0 shift,
  					// otherwise we do not know the exact shift during the loop. 
	
// in case type == PROG_OPER_INFLOOP_COND
  int			inf_loop_cond_offset;
} prog_oper;

extern prog_oper*
poper_create(int id, prog_oper_type type);

/**
 * Deleting the statement. The string with the source is appended to the end of the source 
 * of the previous statement.
 */
extern void
poper_delete(prog_oper *op);

extern void
poper_dump(prog_oper *op);

typedef struct prog_tree
{
  prog_oper		*first, *last;
  prog_oper		*error;
} prog_tree;

extern void
ptree_add_poper(prog_tree *tree, prog_oper *op);

extern void
poper_simple2linear(prog_oper *op);

extern void
ptree_simple2linear(prog_tree *tree);

#endif
