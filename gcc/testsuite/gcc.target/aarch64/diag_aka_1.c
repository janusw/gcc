#include <arm_neon.h>

typedef int16x4_t myvec;

void f (float x)
{
  __Int8x8_t y1 = x; /* { dg-error {incompatible types when initializing type '__Int8x8_t' using type 'float'} } */
  __Int8x8_t *ptr1 = &x; /* { dg-error {initialization of '__Int8x8_t \*' from incompatible pointer type 'float \*'} } */
  int8x8_t y2 = x; /* { dg-error {incompatible types when initializing type 'int8x8_t' using type 'float'} } */
  int8x8_t *ptr2 = &x; /* { dg-error {initialization of 'int8x8_t \*' from incompatible pointer type 'float \*'} } */
  /* ??? For these it would be better to print an aka for 'int16x4_t'.  */
  myvec y3 = x; /* { dg-error {incompatible types when initializing type 'myvec' using type 'float'} } */
  myvec *ptr3 = &x; /* { dg-error {initialization of 'myvec \*' from incompatible pointer type 'float \*'} } */
}
