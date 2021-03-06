#include "com_biguint.h"
#include "com_assert.h"
#include "com_imath.h"
#include "com_mem.h"

com_biguint com_biguint_create(com_allocator_Handle h) {
  return (com_biguint){._array = com_vec_create(h)};
}

void com_biguint_destroy(com_biguint *a) { com_vec_destroy(&a->_array); }

void com_biguint_set(com_biguint *dest, const com_biguint *src) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(src != NULL, "src is null");
  usize len = com_vec_length(&src->_array);
  com_vec_set_len(&dest->_array, len);
  com_mem_move(com_vec_get(&dest->_array, 0), com_vec_get(&src->_array, 0),
               len);
}

void com_biguint_set_u64(com_biguint *dest, u64 val) {
  com_assert_m(dest != NULL, "dest is null");
  // u64 means only 2 u32 s are needed in the vector
  com_vec *v = &dest->_array;
  if (val == 0) {
    com_vec_set_len_m(v, 0, u32);
  } else if (val <= u32_max_m) {
    com_vec_set_len_m(v, 1, u32);
    *com_vec_get_m(v, 0, u32) = val & 0x00000000FFFFFFFFu;
  } else {
    com_vec_set_len_m(v, 2, u32);
    // downcasting it will get rid of the upper 32 bits
    *com_vec_get_m(v, 0, u32) = val & 0x00000000FFFFFFFFu;
    // guaranteed to fit in 32 bits
    *com_vec_get_m(v, 1, u32) = val >> 32;
  }
}

u64 com_biguint_get_u64(const com_biguint *a) {
  com_assert_m(a != NULL, "a is null");

  switch (com_vec_len_m(&a->_array, u32)) {
  case 0: {
    return 0;
  }
  case 1: {
    return *com_vec_get_m(&a->_array, 0, u32);
  }
  case 2: {
    u32 *arr = com_vec_get_m(&a->_array, 0, u32);
    return ((u64)arr[1] << (u64)32) + (u64)arr[0];
  }
  default: {
    return u64_max_m;
  }
  }
}

f64 com_biguint_get_f64(const com_biguint *a) {
  com_assert_m(a != NULL, "a is null");
  f64 ret = 0;
  for (usize i = 0; i < com_vec_len_m(&a->_array, u32); i++) {
    // this operation may be lossy because floating points can really only get
    // so big
    ret = ret * u32_max_m + *com_vec_get_m(&a->_array, i, u32);
  }
  return ret;
}

bool com_biguint_fits_u64(const com_biguint *a) {
  return com_vec_len_m(&a->_array, u32) <= 2;
}

bool com_biguint_is_zero(const com_biguint *a) {
  return com_vec_length(&a->_array) == 0;
}

// bitwise functions

// sets dest to a & b
// REQUIRES: `dest` is a valid pointer to a valid com_biguint
// REQUIRES: `a` is a valid pointer to `alen` u32s
// REQUIRES: `b` is a valid pointer to `blen` u32s
// REQUIRES: alen >= blen
// GUARANTEES: `dest` will be set to the value of `a` & `b`
static void internal_value_and(com_biguint *dest, const u32 *a, usize alen,
                               const u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen isn't greater than blen");
  // algorithm is to loop through where both are valid and then trim off any
  // excess

  // for now, we'll set the dest len to blen, as this is the largest it could
  // possibly be length of dest
  usize dest_len = blen;

  com_vec_set_len_m(&dest->_array, dest_len, u32);
  u32 *dest_arr = com_vec_get(&dest->_array, 0);
  for (usize i = 0; i < dest_len; i++) {
    dest_arr[i] = a[i] & b[i];
  }

  // first start at the end of the array and then count the number of zeros
  // going backwards
  usize zeros_len = 0;
  for (usize i = dest_len; i > 0; i--) {
    // here I is the index + 1 (did this to prevent overflow)

    // note that i-1 can never overflow because i > 0 at all times in the loop
    if (dest_arr[i - 1] == 0) {
      zeros_len++;
    } else {
      break;
    }
  }

  // now actually remove zeros by truncating the vector
  // This is safe because zeros_len is always less than dest_len
  com_vec_set_len_m(&dest->_array, dest_len - zeros_len, u32);
}

void com_biguint_and(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");

  u32 *a_arr = com_vec_get(&a->_array, 0);
  u32 *b_arr = com_vec_get(&b->_array, 0);

  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  if (blen > alen) {
    internal_value_and(dest, b_arr, blen, a_arr, alen);
  } else {
    internal_value_and(dest, a_arr, alen, b_arr, blen);
  }
}

static void internal_value_or(com_biguint *dest, const u32 *a, usize alen,
                              const u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen isn't greater than blen");

  // the result is guaranteed to be exactly alen long
  usize dest_len = alen;
  com_vec_set_len_m(&dest->_array, dest_len, u32);
  u32 *dest_arr = com_vec_get(&dest->_array, 0);

  // for this part of the array, both a and b are defined
  for (usize i = 0; i < blen; i++) {
    dest_arr[i] = a[i] | b[i];
  }

  // for this part only a is defined
  for (usize i = blen; i < dest_len; i++) {
    dest_arr[i] = a[i];
  }
}

void com_biguint_or(com_biguint *dest, const com_biguint *a,
                    const com_biguint *b) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");

  u32 *a_arr = com_vec_get(&a->_array, 0);
  u32 *b_arr = com_vec_get(&b->_array, 0);

  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  if (blen > alen) {
    internal_value_or(dest, b_arr, blen, a_arr, alen);
  } else {
    internal_value_or(dest, a_arr, alen, b_arr, blen);
  }
}

static void internal_value_xor(com_biguint *dest, const u32 *a, usize alen,
                               const u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen isn't greater than blen");
  // algorithm is to loop through where both are valid and then trim off any
  // excess

  // for now, we'll set the dest len to blen, as this is the largest it could
  // possibly be length of dest
  usize dest_len = blen;

  com_vec_set_len_m(&dest->_array, dest_len, u32);
  u32 *dest_arr = com_vec_get(&dest->_array, 0);
  for (usize i = 0; i < dest_len; i++) {
    dest_arr[i] = a[i] ^ b[i];
  }

  // first start at the end of the array and then count the number of zeros
  // going backwards
  usize zeros_len = 0;
  for (usize i = dest_len; i > 0; i--) {
    // here I is the index + 1 (did this to prevent overflow)

    // note that i-1 can never overflow because i > 0 at all times in the loop
    if (dest_arr[i - 1] == 0) {
      zeros_len++;
    } else {
      break;
    }
  }

  // now actually remove zeros by truncating the vector
  // This is safe because zeros_len is always less than dest_len
  com_vec_set_len_m(&dest->_array, dest_len - zeros_len, u32);
}

void com_biguint_xor(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");

  u32 *a_arr = com_vec_get(&a->_array, 0);
  u32 *b_arr = com_vec_get(&b->_array, 0);

  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  if (blen > alen) {
    internal_value_xor(dest, b_arr, blen, a_arr, alen);
  } else {
    internal_value_xor(dest, a_arr, alen, b_arr, blen);
  }
}


void com_biguint_lshift(com_biguint *dest, const com_biguint *a,
                        const usize bits) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(a != NULL, "a is null");

  com_vec *dvec = &dest->_array;
  const com_vec *avec = &a->_array;

  usize alen = com_vec_len_m(avec, u32);

  // if a is zero exit fast
  if (alen == 0) {
    com_biguint_set_u64(dest, 0);
    return;
  }

  // in general, the plan is to go in reverse
  // This enables in place mutation, should the user provide the same
  // destination and operator

  usize words = bits / 32;
  u32 rbits = bits % 32;

  // set dest length to a's length + words shifted
  // (we'll add an extra one if it overflows later)
  // since this is left shift we are increasing the size of the number
  com_vec_set_len_m(dvec, com_vec_len_m(avec, u32) + words, u32);

  // get the overflow from the last digit
  u32 overflow = ((u64)*com_vec_get_m(avec, alen - 1, u32) << (u64)rbits) >> 32;
  // handle final overflow if it exists
  if (overflow != 0) {
    *com_vec_push_m(dvec, u32) = overflow;
  }

  const u32 *a_arr = com_vec_get(avec, 0);

  // get array
  u32 *dest_arr = com_vec_get(dvec, 0);

  // get normal
  for (usize i = alen - 1; i > 0; i--) {
    // must account for lower bytes being shifted into upper bytes
    dest_arr[i + words] = (a_arr[i] << rbits) | (a_arr[i - 1] >> (32 - rbits));
  }

  dest_arr[0] <<= rbits;

  // set first `word` bytes to zero
  com_mem_zero_arr_m(dest_arr, words, u32);
}

void com_biguint_rshift(com_biguint *dest, const com_biguint *a,
                        const usize bits) {
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(a != NULL, "a is null");

  com_vec *dvec = &dest->_array;
  const com_vec *avec = &a->_array;

  usize alen = com_vec_len_m(avec, u32);

  // have to set it exactly large to avoid issues
  // if aliasing
  com_vec_set_len_m(dvec, alen, u32);

  usize words = bits / 32;
  u32 rbits = bits % 32;

  if (words > alen) {
    com_biguint_set_u64(dest, 0);
    return;
  }

  const u32 *a_arr = com_vec_get(avec, 0);
  u32 *dest_arr = com_vec_get(dvec, 0);

  for (usize i = words; i < alen - 1; i++) {
    // Handle bits from the next bytes shifting into place
    dest_arr[i - words] = (a_arr[i] >> rbits) | (a_arr[i + 1] << (32 - rbits));
  }

  u32 last = a_arr[alen - 1] >> rbits;
  if (last != 0) {
    dest_arr[alen - words - 1] = last;
    com_vec_set_len_m(dvec, alen - words, u32);
  } else {
    com_vec_set_len_m(dvec, alen - words - 1, u32);
  }
}

// Adds together a and b into DEST
// REQUIRES: `dest` is a pointer to a valid com_biguint
// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
// REQUIRES: alen >= blen
// GUARANTEES: `dest` will contain the sum of a and b
static void internal_value_add_u32_arr(com_biguint *dest, const u32 *a,
                                       usize alen, const u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen is less than blen");
  // first we have to find the size of the result
  // NOTE: Thanks to carrying, the result size could be 1 larger than this
  // We guarantee that `alen` is the larger len
  usize dest_len = alen;

  // extend dest arr to new size
  com_vec_set_len_m(&dest->_array, dest_len, u32);

  // get destination array
  u32 *dest_arr = com_vec_get_m(&dest->_array, 0, u32);

  // represents the amount to be carried after adding one word
  u8 carry = 0;

  // procedure is:
  // add together each word, if it overflows then we carry 1 over to the next
  // word

  // in this for loop, a[i] and b[i] are both valid
  for (usize i = 0; i < blen; i++) {
    u64 aval = a[i];
    u64 bval = b[i];
    u64 tmp = aval + bval + carry;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      carry = 1;
      dest_arr[i] = (u32)(tmp - u32_max_m);
    } else {
      // if the value can fit in a u32
      carry = 0;
      dest_arr[i] = (u32)tmp;
    }
  }

  // this is essentially the loop where we are finished adding,
  // and are just handling any times where carrying the 1 forward will cause
  // overflow and require another carry
  // this also handles the part of a unaffected by the addition

  // in this for loop, only a[i] is valid
  for (usize i = blen; i < alen; i++) {
    u32 aval = a[i];
    u64 tmp = aval + carry;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      dest_arr[i] = (u32)(tmp - u32_max_m);
      carry = 1;
    } else {
      // if the value can fit in a u32
      dest_arr[i] = (u32)tmp;
      carry = 0;
    }
  }

  // in the last part, if we are still carrying, we need to extend the length
  // of dest to handle that
  if (carry != 0) {
    *com_vec_push_m(&dest->_array, u32) = carry;
  }
}

// compares the magnitude of b with reference to a
/// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
/// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
static com_math_cmptype internal_value_cmp_u32_arr(const u32 *a, usize alen,
                                                   const u32 *b, usize blen) {
  // eliminate the easy cases where one is obviously less or greater than the
  // other
  if (blen > alen) {
    return com_math_GREATER;
  } else if (blen < alen) {
    return com_math_LESS;
  }

  // if control flow reaches here, alen and blen are the same
  usize len = alen;

  // compare backwards where both a and b are defined
  for (usize i = len; i != 0; i--) {
    // note that in this form, i will always be 1 greater than what we want
    u32 aval = a[i - 1];
    u32 bval = b[i - 1];

    if (aval < bval) {
      return com_math_GREATER;
    } else if (aval > bval) {
      return com_math_LESS;
    }
  }

  return com_math_EQUAL;
}

com_math_cmptype com_biguint_cmp(const com_biguint *a, const com_biguint *b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  u32 *b_arr = com_vec_get_m(&b->_array, 0, u32);

  return internal_value_cmp_u32_arr(a_arr, alen, b_arr, blen);
}

com_math_cmptype com_biguint_cmp_u64(const com_biguint *a, u64 b) {
  com_assert_m(a != NULL, "a is null");
  if (com_biguint_fits_u64(a)) {
    u64 aval = com_biguint_get_u64(a);
    if (b == aval) {
      return com_math_EQUAL;
    } else if (b < aval) {
      return com_math_LESS;
    } else {
      return com_math_GREATER;
    }
  } else {
    return com_math_LESS;
  }
}

// sets DEST to a - b
// REQUIRES: `dest` is a pointer to a valid com_biguint
// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
// REQUIRES: alen >= blen
// REQUIRES: the value held by a is greater than the value held by b
// GUARANTEES: `dest` will contain the difference of a and b
static void internal_value_sub_u32_arr(com_biguint *dest, const u32 *a,
                                       usize alen, const u32 *b, usize blen) {
  com_assert_m(alen >= blen, "alen is less than blen");
  com_assert_m(internal_value_cmp_u32_arr(a, alen, b, blen) != com_math_LESS,
               "a < b");

  // in general, it is critical to make sure that we account for the fact that
  // dest could be aliased to a or b

  // first we have to find the size of the result
  // alen is the MAX width that our result could have, because
  // We've already guaranteed that `alen` is the larger len,
  // and also that a - b will always be less than or equal to a

  // resize dest_vec to the new size, we will trim later if necessary
  com_vec_set_len_m(&dest->_array, alen, u32);

  u32 *dest_arr = com_vec_get_m(&dest->_array, 0, u32);

  // represents the amount to be borrowed if necessary
  u8 borrow = 0;

  // procedure is:
  // sub each word: a + next_word - b + borrow
  // If it's greater than max length, then we didn't need the borrow, and
  // subtract it before setting the word in the dest array
  // Else, we did need the borrow, and set borrow to -1 so we can
  // subtract it from the next word

  // in this for loop, a[i] and b[i] are both valid
  for (usize i = 0; i < blen; i++) {
    u32 aval = a[i];
    u32 bval = b[i];
    u64 tmp = u32_max_m + aval - bval - borrow;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      // Means we didn't need the borrow
      borrow = 0;
      dest_arr[i] = (u32)(tmp - u32_max_m);
    } else {
      // if the value can fit in a u32
      // Means we needed the borrow
      borrow = 1;
      dest_arr[i] = (u32)tmp;
    }
  }

  // this is essentially the loop where we are finished subtracting,
  // and are just handling any times where borrowing the forward will cause
  // problems and require another carry
  // It also handles the portion of `a` that is unaffected by the subtraction

  // in this for loop, only a[i] is valid
  for (usize i = blen; i < alen; i++) {
    u32 aval = a[i];
    u64 tmp = u32_max_m + aval - borrow;

    if (tmp > u32_max_m) {
      // if the value overflows the capacity of a u32
      dest_arr[i] = (u32)(tmp - u32_max_m);
      // means we didn't need to borrow
      borrow = 0;
    } else {
      // if the value can fit in a u32
      // Means we needed the borrow
      dest_arr[i] = (u32)tmp;
      borrow = 1;
    }
  }

  com_assert_m(borrow == 0,
               "even after subtraction, we still need a borrow, means a < b");

  // length of dest
  usize dest_len = alen;

  // first start at the end of the array and then count the number of zeros
  // going backwards
  usize zeros_len = 0;
  for (usize i = dest_len; i > 0; i--) {
    // here I is the index + 1 (did this to prevent overflow)

    // note that i-1 can never overflow because i > 0 at all times in the loop
    if (dest_arr[i - 1] == 0) {
      zeros_len++;
    } else {
      break;
    }
  }

  // now actually remove zeros by truncating the vector
  // This is safe because zeros_len is always less than dest_len
  com_vec_set_len_m(&dest->_array, dest_len - zeros_len, u32);
}

void com_biguint_add_u32(com_biguint *dest, const com_biguint *a, u32 b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(dest != NULL, "dest is null");
  if (b == 0) {
    return;
  }
  usize a_len = com_vec_len_m(&a->_array, u32);
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  if (a_len == 0) {
    // if a_len is 0, then the value of a is zero, so we can just set it
    com_biguint_set_u64(dest, b);
  } else {
    // we treat the `b` like a 1 length array
    internal_value_add_u32_arr(dest, a_arr, a_len, &b, 1u);
  }
}

void com_biguint_sub_u32(com_biguint *dest, const com_biguint *a, u32 b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(dest != NULL, "dest is null");
  if (b == 0) {
    return;
  }
  usize a_len = com_vec_len_m(&a->_array, u32);
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  // we treat the `b` like a 1 length array
  if (a_len == 0) {
    // if a_len == 0, then it means a is zero.
    // negative numbers are invalid because this is a uint
    com_assert_m(b == 0,
                 "trying to subtract a nonzero number from a zero biguint");
    return;
  } else {
    internal_value_sub_u32_arr(dest, a_arr, a_len, &b, 1u);
  }
}

void com_biguint_add(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(dest != NULL, "dest is null");
  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  // get a pointer to the beginning of the array
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  u32 *b_arr = com_vec_get_m(&b->_array, 0, u32);

  // if alen >= blen, add normally
  // else flip order before calling interally
  if (alen >= blen) {
    internal_value_add_u32_arr(dest, a_arr, alen, b_arr, blen);
  } else {
    internal_value_add_u32_arr(dest, b_arr, blen, a_arr, alen);
  }
}

void com_biguint_sub(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(com_biguint_cmp(a, b) != com_math_GREATER,
               "b > a, subtraction would be invalid");

  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  // get a pointer to the beginning of the array
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  u32 *b_arr = com_vec_get_m(&b->_array, 0, u32);

  // this is safe because we know that if alen > blen, then a > b, because we
  // prohibit leading zeros
  internal_value_add_u32_arr(dest, a_arr, alen, b_arr, blen);
}

// Sets dest to a * b
// REQUIRES: `dest` is a valid pointer to a valid com_biguint
// REQUIRES: `a` is a valid pointer at least `alen` u32s
// GUARANTEES: dest will be set to a*b
static void internal_value_mul_u32_arr_u32(com_biguint *dest, const u32 *a,
                                           usize alen, u32 b) {
  com_vec *dest_vec = &dest->_array;
  // extend array to the size of alen
  com_vec_set_len_m(dest_vec, alen, u32);

  u32 *dest_arr = com_vec_get_m(dest_vec, 0, u32);

  u32 carry = 0;
  for (usize i = 0; i < alen; i++) {
    u64 aval = a[i];
    // this will never overflow since aval * b is promoted to u64 before adding
    // carry
    u64 tmp = aval * b + carry;
    // the upper half of u32
    // Represents the value that can't be fit into the current value
    carry = tmp >> 32;
    // push the lower half of tmp
    dest_arr[i] = tmp & 0x00000000FFFFFFFFu;
  }

  // if we still have something to carry add it in
  if (carry != 0) {
    *com_vec_push_m(dest_vec, u32) = carry;
  }
}

void com_biguint_mul_u32(com_biguint *dest, const com_biguint *a, u32 b) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(dest != NULL, "dest is null");

  internal_value_mul_u32_arr_u32(dest, com_vec_get_m(&a->_array, 0, u32),
                                 com_vec_len_m(&a->_array, u32), b);
}

// sets dest to a * b
// REQUIRES: `dest` is a pointer to a valid com_biguint
// REQUIRES: `a` is a pointer to an array of at least `alen` u32s
// REQUIRES: `b` is a pointer to an array of at least `blen` u32s
// REQUIRES: `tmp` is a temporary biguint that will be overwritten
// REQUIRES: dest->_array._data MUST NOT overlap with a or b, as dest will be
// modified before the end
// GUARANTEES: dest will contain the product of a and b
// GUARANTEES: tmp will be undefined
static void internal_value_mul_u32_arr(com_biguint *dest, const u32 *a,
                                       usize alen, const u32 *b, usize blen,
                                       com_biguint *tmp) {
  // algorithm is the same as normal multiplication
  // Start with 0
  // For each digit in b, multiply a by that digit of b, shift by the index and
  // add it to the running total

  // zero initialize dest, we will later add the result of each
  // submultiplication to it
  com_biguint_set_u64(dest, 0);

  for (usize i = 0; i < blen; i++) {

    // we know ahead of time that the maximum size that this temp thing can be
    // is alen + blen this variable represents the product of a with a single
    // digit of b

    // multiply
    internal_value_mul_u32_arr_u32(tmp, a, alen, b[i]);

    // shift right by the current place, zero fill
    usize len = sizeof(u32) * i;
    com_mem_zero(com_vec_insert(&tmp->_array, 0, len), len);

    // add result to dest
    com_biguint_add(dest, dest, tmp);
  }
}

void com_biguint_mul(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b, com_allocator *allocator) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(dest != NULL, "dest is null");
  com_assert_m(allocator != NULL, "allocator is null");

  usize alen = com_vec_len_m(&a->_array, u32);
  usize blen = com_vec_len_m(&b->_array, u32);

  // get a pointer to the beginning of the array
  u32 *a_arr = com_vec_get_m(&a->_array, 0, u32);
  u32 *b_arr = com_vec_get_m(&b->_array, 0, u32);

  // allocate a tmp biguint
  com_biguint tmp = com_biguint_create(com_allocator_alloc(
      allocator, (com_allocator_HandleData){
                     // technically could overflow, but won't because if alen
                     // and blen are really that long,
                     // then all the address space is already used up by those
                     .len = alen + blen + 1,
                     // we don't need to REALLOCABLE since the max len tmp could
                     // go through is already covered by our len
                     // we'll clean up at the end so we don't need NOLEAK
                     .flags = com_allocator_defaults(allocator)}));

  // if any of the arguments are aliased with dest, then we need to create a new
  // dest
  if (dest == b || dest == a) {
    com_biguint newdest = com_biguint_create(com_allocator_alloc(
        allocator, (com_allocator_HandleData){
                       .len = alen + blen + 1,
                       .flags = com_allocator_defaults(allocator)}));
    internal_value_mul_u32_arr(&newdest, a_arr, alen, b_arr, blen, &tmp);
    com_biguint_set(dest, &newdest);
    // free newdest
    com_biguint_destroy(&newdest);
  } else {
    // means that none of the arguments are aliased with each other
    internal_value_mul_u32_arr(dest, a_arr, alen, b_arr, blen, &tmp);
  }

  // free tmp
  com_biguint_destroy(&tmp);
}

void com_biguint_div_u32(attr_UNUSED com_biguint *dest, attr_UNUSED const com_biguint *a, attr_UNUSED u32 b){
  // TODO
  // Need to figure out way without memory allocation
}

#define new_bigint(a)                                                          \
  com_biguint_create(com_allocator_alloc(                                      \
      a, (com_allocator_HandleData){.len = 10,                                 \
                                    .flags = com_allocator_defaults(a) |       \
                                             com_allocator_REALLOCABLE}))
// ALGORITHM FROM HERE
// https://github.com/kokke/tiny-bignum-c/blob/master/bn.c
void com_biguint_div(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b, com_allocator *allocator) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(dest != NULL, "remainder is null");
  com_assert_m(allocator != NULL, "allocator is null");
  com_assert_m(!com_biguint_is_zero(b), "division by zero error");

  com_biguint current = new_bigint(allocator);
  com_biguint denom = new_bigint(allocator);
  com_biguint tmp = new_bigint(allocator);

  com_biguint_set_u64(&current, 1); // int current = 1;
  com_biguint_set(&denom, b);       // denom = b
  com_biguint_set(&tmp, a);         // tmp   = a

  while (com_biguint_cmp(a, &denom) != com_math_GREATER) // while (denom <= a)
  {
    com_biguint_lshift(&current, &current, 1); //   current <<= 1;
    com_biguint_lshift(&denom, &denom, 1);     //   denom <<= 1;
  }

  com_biguint_rshift(&current, &current, 1); //   current >>= 1;
  com_biguint_rshift(&denom, &denom, 1);     //   denom >>= 1;

  com_biguint_set_u64(dest, 0); // int answer = 0;

  while (!com_biguint_is_zero(&current)) // while (current != 0)
  {
    if (com_biguint_cmp(&denom, &tmp) !=
        com_math_LESS) //   if (dividend >= denom)
    {
      com_biguint_sub(&tmp, &tmp, &denom);  //     dividend -= denom;
      com_biguint_or(dest, &current, dest); //     answer |= current;
    }
    com_biguint_rshift(&current, &current, 1); //   current >>= 1;
    com_biguint_rshift(&denom, &denom, 1);     //   denom >>= 1;
  }                                            // return answer;

  com_biguint_destroy(&current);
  com_biguint_destroy(&denom);
  com_biguint_destroy(&tmp);
}

void com_biguint_div_rem(com_biguint *quotient, com_biguint *remainder,
                         const com_biguint *a, const com_biguint *b,
                         com_allocator *allocator) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(quotient != NULL, "quotient is null");
  com_assert_m(remainder != NULL, "remainder is null");
  com_assert_m(allocator != NULL, "allocator is null");
  com_assert_m(!com_biguint_is_zero(b), "division by zero error");

  com_biguint_div(quotient, a, b, allocator);
  com_biguint_sub(remainder, a, quotient);
}

void com_biguint_rem(com_biguint *dest, const com_biguint *a,
                     const com_biguint *b, com_allocator *allocator) {
  com_assert_m(a != NULL, "a is null");
  com_assert_m(b != NULL, "b is null");
  com_assert_m(dest != NULL, "remainder is null");
  com_assert_m(allocator != NULL, "allocator is null");
  com_assert_m(!com_biguint_is_zero(b), "division by zero error");

  // initially start with zero length but chan realloc later
  com_biguint quotient = new_bigint(allocator);

  com_biguint_div_rem(&quotient, dest, a, b, allocator);

  com_biguint_destroy(&quotient);
}

usize com_biguint_len(const com_biguint *a) {
  return com_vec_len_m(&a->_array, u32);
}

u32 com_biguint_get_at(const com_biguint *a, usize i) {
  return *com_vec_get_m(&a->_array, i, u32);
}

void com_biguint_set_at(com_biguint *a, usize i, u32 val) {
  *com_vec_get_m(&a->_array, i, u32) = val;
}
