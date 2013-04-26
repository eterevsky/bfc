/**
 * General data structures.
 */

#ifndef _TOOLS_H
#define _TOOLS_H

/**
 * Self-expanding array of characters.
 */
typedef struct arr_c
{
  long			size;
  long			alloc;
  char			*val;
} arr_c;

/// Starting size (number of elements) of arr_c
#define ARR_C_ST_ALLOC 8

extern arr_c*
arr_c_create();

extern void
arr_c_delete(arr_c *arr);

extern void
arr_c_push(arr_c *arr, char v);

extern void
arr_c_clear(arr_c *arr);

/**
 * Self-expanding array of pointers.
 */
typedef struct arr_p
{
  long			size;
  long			alloc;
  void			**val;
} arr_p;

/// Starting size (number of elements) of arr_p
#define ARR_P_ST_ALLOC 2

extern arr_p*
arr_p_create();

extern void
arr_p_delete(arr_p *arr);

extern void
arr_p_push(arr_p *arr, void* v);

extern void*
arr_p_pop(arr_p *arr);

#endif
