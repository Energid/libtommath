/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@iahu.ca, http://math.libtomcrypt.org
 */
#include <tommath.h>

/* multiply by a digit */
int
mp_mul_d (mp_int * a, mp_digit b, mp_int * c)
{
  int     res, pa, olduse;

  /* make sure c is big enough to hold a*b */
  pa = a->used;
  if (c->alloc < pa + 1) {
    if ((res = mp_grow (c, pa + 1)) != MP_OKAY) {
      return res;
    }
  }

  /* get the original destinations used count */
  olduse = c->used;

  /* set the new temporary used count */
  c->used = pa + 1;
  c->sign = a->sign;

  {
    register mp_digit u, *tmpa, *tmpc;
    register mp_word r;
    register int ix;

    /* alias for a->dp [source] */
    tmpa = a->dp;

    /* alias for c->dp [dest] */
    tmpc = c->dp;

    /* zero carry */
    u = 0;
    for (ix = 0; ix < pa; ix++) {
      /* compute product and carry sum for this term */
      r = ((mp_word) u) + ((mp_word)*tmpa++) * ((mp_word)b);

      /* mask off higher bits to get a single digit */
      *tmpc++ = (mp_digit) (r & ((mp_word) MP_MASK));

      /* send carry into next iteration */
      u = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
    }
    /* store final carry [if any] */
    *tmpc++ = u;

    /* now zero digits above the top */
    for (; pa < olduse; pa++) {
       *tmpc++ = 0;
    }
  }

  mp_clamp (c);
  return MP_OKAY;
}
