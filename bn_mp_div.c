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

/* integer signed division. 
 * c*b + d == a [e.g. a/b, c=quotient, d=remainder]
 * HAC pp.598 Algorithm 14.20
 *
 * Note that the description in HAC is horribly 
 * incomplete.  For example, it doesn't consider 
 * the case where digits are removed from 'x' in 
 * the inner loop.  It also doesn't consider the 
 * case that y has fewer than three digits, etc..
 *
 * The overall algorithm is as described as 
 * 14.20 from HAC but fixed to treat these cases.
*/
int
mp_div (mp_int * a, mp_int * b, mp_int * c, mp_int * d)
{
  mp_int  q, x, y, t1, t2;
  int     res, n, t, i, norm, neg;

  /* is divisor zero ? */
  if (mp_iszero (b) == 1) {
    return MP_VAL;
  }

  /* if a < b then q=0, r = a */
  if (mp_cmp_mag (a, b) == MP_LT) {
    if (d != NULL) {
      res = mp_copy (a, d);
    } else {
      res = MP_OKAY;
    }
    if (c != NULL) {
      mp_zero (c);
    }
    return res;
  }

  if ((res = mp_init_size (&q, a->used + 2)) != MP_OKAY) {
    return res;
  }
  q.used = a->used + 2;

  if ((res = mp_init (&t1)) != MP_OKAY) {
    goto __Q;
  }

  if ((res = mp_init (&t2)) != MP_OKAY) {
    goto __T1;
  }

  if ((res = mp_init_copy (&x, a)) != MP_OKAY) {
    goto __T2;
  }

  if ((res = mp_init_copy (&y, b)) != MP_OKAY) {
    goto __X;
  }

  /* fix the sign */
  neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
  x.sign = y.sign = MP_ZPOS;

  /* normalize both x and y, ensure that y >= b/2, [b == 2**DIGIT_BIT] */
  norm = mp_count_bits(&y) % DIGIT_BIT;
  if (norm < (int)(DIGIT_BIT-1)) {
     norm = (DIGIT_BIT-1) - norm;
     if ((res = mp_mul_2d (&x, norm, &x)) != MP_OKAY) {
       goto __Y;
     }
     if ((res = mp_mul_2d (&y, norm, &y)) != MP_OKAY) {
       goto __Y;
     }
  } else {
     norm = 0;
  }

  /* note hac does 0 based, so if used==5 then its 0,1,2,3,4, e.g. use 4 */
  n = x.used - 1;
  t = y.used - 1;

  /* while (x >= y*b**n-t) do { q[n-t] += 1; x -= y*b**{n-t} } */
  if ((res = mp_lshd (&y, n - t)) != MP_OKAY) { /* y = y*b**{n-t} */
    goto __Y;
  }

  while (mp_cmp (&x, &y) != MP_LT) {
    ++(q.dp[n - t]);
    if ((res = mp_sub (&x, &y, &x)) != MP_OKAY) {
      goto __Y;
    }
  }

  /* reset y by shifting it back down */
  mp_rshd (&y, n - t);

  /* step 3. for i from n down to (t + 1) */
  for (i = n; i >= (t + 1); i--) {
    if (i > x.used)
      continue;

    /* step 3.1 if xi == yt then set q{i-t-1} to b-1, 
     * otherwise set q{i-t-1} to (xi*b + x{i-1})/yt */
    if (x.dp[i] == y.dp[t]) {
      q.dp[i - t - 1] = ((((mp_digit)1) << DIGIT_BIT) - 1);
    } else {
      mp_word tmp;
      tmp = ((mp_word) x.dp[i]) << ((mp_word) DIGIT_BIT);
      tmp |= ((mp_word) x.dp[i - 1]);
      tmp /= ((mp_word) y.dp[t]);
      if (tmp > (mp_word) MP_MASK)
        tmp = MP_MASK;
      q.dp[i - t - 1] = (mp_digit) (tmp & (mp_word) (MP_MASK));
    }

    /* while (q{i-t-1} * (yt * b + y{t-1})) > 
             xi * b**2 + xi-1 * b + xi-2 
     
       do q{i-t-1} -= 1; 
    */
    q.dp[i - t - 1] = (q.dp[i - t - 1] + 1) & MP_MASK;
    do {
      q.dp[i - t - 1] = (q.dp[i - t - 1] - 1) & MP_MASK;

      /* find left hand */
      mp_zero (&t1);
      t1.dp[0] = (t - 1 < 0) ? 0 : y.dp[t - 1];
      t1.dp[1] = y.dp[t];
      t1.used = 2;
      if ((res = mp_mul_d (&t1, q.dp[i - t - 1], &t1)) != MP_OKAY) {
        goto __Y;
      }

      /* find right hand */
      t2.dp[0] = (i - 2 < 0) ? 0 : x.dp[i - 2];
      t2.dp[1] = (i - 1 < 0) ? 0 : x.dp[i - 1];
      t2.dp[2] = x.dp[i];
      t2.used = 3;
    } while (mp_cmp_mag(&t1, &t2) == MP_GT);

    /* step 3.3 x = x - q{i-t-1} * y * b**{i-t-1} */
    if ((res = mp_mul_d (&y, q.dp[i - t - 1], &t1)) != MP_OKAY) {
      goto __Y;
    }

    if ((res = mp_lshd (&t1, i - t - 1)) != MP_OKAY) {
      goto __Y;
    }

    if ((res = mp_sub (&x, &t1, &x)) != MP_OKAY) {
      goto __Y;
    }

    /* if x < 0 then { x = x + y*b**{i-t-1}; q{i-t-1} -= 1; } */
    if (x.sign == MP_NEG) {
      if ((res = mp_copy (&y, &t1)) != MP_OKAY) {
        goto __Y;
      }
      if ((res = mp_lshd (&t1, i - t - 1)) != MP_OKAY) {
        goto __Y;
      }
      if ((res = mp_add (&x, &t1, &x)) != MP_OKAY) {
        goto __Y;
      }

      q.dp[i - t - 1] = (q.dp[i - t - 1] - 1UL) & MP_MASK;
    }
  }

  /* now q is the quotient and x is the remainder 
   * [which we have to normalize] 
   */
  
  /* get sign before writing to c */
  x.sign = a->sign;

  if (c != NULL) {
    mp_clamp (&q);
    mp_exch (&q, c);
    c->sign = neg;
  }

  if (d != NULL) {
    mp_div_2d (&x, norm, &x, NULL);
    mp_exch (&x, d);
  }

  res = MP_OKAY;

__Y:mp_clear (&y);
__X:mp_clear (&x);
__T2:mp_clear (&t2);
__T1:mp_clear (&t1);
__Q:mp_clear (&q);
  return res;
}
