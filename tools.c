#include <stdio.h>
#include <stdlib.h>
#include "tools.h"

arr_c*
arr_c_create()
{
  arr_c *arr  = malloc(sizeof(arr_c));
  arr->val   = malloc(sizeof(char) * ARR_C_ST_ALLOC);
  arr->size  = 0;
  arr->alloc = ARR_C_ST_ALLOC;

  return arr;
}

void
arr_c_delete(arr_c *arr)
{
  free(arr->val);
  free(arr);
}

static void
arr_c_enlarge(arr_c *arr)
{
  if (arr->size == arr->alloc)
  {
    arr->alloc += arr->alloc/2;
    arr->val = realloc(arr->val, sizeof(char) * arr->alloc);
    if (arr->val == NULL)
    {
      fprintf(stderr, "Not enougn memory.");
      abort();
    }
  }
}

void
arr_c_push(arr_c *arr, char v)
{
  arr_c_enlarge(arr);
  arr->val[arr->size++] = v;
}

void
arr_c_clear(arr_c *arr)
{
  arr->size = 0;
}


arr_p*
arr_p_create()
{
  arr_p *arr  = malloc(sizeof(arr_p));
  arr->val   = malloc(sizeof(void*) * ARR_P_ST_ALLOC);
  arr->size  = 0;
  arr->alloc = ARR_P_ST_ALLOC;

  return arr;
}

void
arr_p_delete(arr_p *arr)
{
  free(arr->val);
  free(arr);
}

static void
arr_p_enlarge(arr_p *arr)
{
  if (arr->size == arr->alloc)
  {
    arr->alloc += arr->alloc/2;
    arr->val = realloc(arr->val, sizeof(void*) * arr->alloc);
    if (arr->val == NULL)
    {
      fprintf(stderr, "Not enougn memory.");
      abort();
    }
  }
}

void
arr_p_push(arr_p *arr, void* v)
{
  arr_p_enlarge(arr);
  arr->val[arr->size++] = v;
}

void*
arr_p_pop(arr_p *arr)
{
  return arr->val[--arr->size];
}

