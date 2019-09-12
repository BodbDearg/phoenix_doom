/* $Id: bigd.h $ */

/******************** SHORT COPYRIGHT NOTICE**************************
This source code is part of the BigDigits multiple-precision
arithmetic library Version 2.2 originally written by David Ireland,
copyright (c) 2001-8 D.I. Management Services Pty Limited, all rights
reserved. It is provided "as is" with no warranties. You may use
this software under the terms of the full copyright notice
"bigdigitsCopyright.txt" that should have been included with this
library or can be obtained from <www.di-mgt.com.au/bigdigits.html>.
This notice must always be retained in any copy.
******************* END OF COPYRIGHT NOTICE***************************/
/*
	Last updated:
	$Date: 2008-07-31 12:54:00 $
	$Revision: 2.2.0 $
	$Author: dai $
*/

/* 
Interface to the BIGD library (BigDigits "bd" functions)
Multiple-precision arithmetic for non-negative numbers
using memory management and opaque pointers. 
*/

#ifndef BIGD_H_
#define BIGD_H_ 1

#include "bigdtypes.h"

/**** USER CONFIGURABLE SECTION ****/

/* [v2.1] Changed to use exact width integer types.
   [v2.2] Moved macros for exact-width types to "bigdtypes.h"
*/

/* DIGIT_T type is not exposed by this library so we expose 
   a synonym `bdigit_t' for a single digit */
typedef uint32_t bdigit_t;

/**** END OF USER CONFIGURABLE SECTION ****/

#define OCTETS_PER_DIGIT (sizeof(bdigit_t))
/* Options for bdPrint */
#define BD_PRINT_NL   0x1	/* append a newline after printing */
#define BD_PRINT_TRIM 0x2	/* trim leading zero digits */

#ifdef NO_ALLOCS 
#error NO_ALLOCS is not permitted with bd functions
#endif

/*
This interface uses opaque pointers of type BIGD using
the conventions in "C Interfaces and Implementions" by
David R. Hanson, Addison-Wesley, 1996, pp21-4.
Thanks to Ian Tree for the C++ fudge.
*/
#define T BIGD
#ifdef __cplusplus
	typedef struct T2 *T;
#else
	typedef struct T *T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* PROGRAMMER'S NOTES
Where the function computes a new BIGD value,
the result is returned in the first argument.
Some functions do not allow variables to overlap (noted by w#v).
Functions of type int generally return 0 to denote success 
but some return True/False (1/0) or borrow (+1) or error (-1).
Functions of type size_t (an unsigned int) return a length.

Memory is allocated if the function might need it. 
It is only released when freed with bdFree(), e.g.:

	BIGD b;
	b = bdNew();
	...
	bdFree(&b);
*/

/******************************/
/* CONSTRUCTOR AND DESTRUCTOR */
/******************************/

BIGD bdNew(void);
	/* Return handle to new BIGD object */

void bdFree(BIGD *bd);	
	/* Zeroise and free BIGD memory
	   - NB pass a pointer to the handle */

/*************************/
/* ARITHMETIC OPERATIONS */
/*************************/

int bdAdd(BIGD w, BIGD u, BIGD v);
	/* Compute w = u + v, w#v */

int bdSubtract(BIGD w, BIGD u, BIGD v);
	/* Compute w = u - v, return borrow, w#v */

int bdMultiply(BIGD w, BIGD u, BIGD v);
	/* Compute w = u * v, w#u */

int bdDivide(BIGD q, BIGD r, BIGD u, BIGD v);
	/* Computes quotient q = u / v and remainder r = u mod v 
	   Trashes q and r first. q#(u,v), r#(u,v) */

int bdModulo(BIGD r, BIGD u, BIGD v);
	/* Computes r = u mod v, r#u */

int bdSquare(BIGD w, BIGD x);
	/* Computes w = x^2, w#x */

int bdSqrt(BIGD s, BIGD x);
	/* Computes integer square root s = floor(sqrt(x)) */

int bdIncrement(BIGD a);
	/* Sets a = a + 1, returns carry */

int bdDecrement(BIGD a);
	/* Sets a = a - 1, returns borrow */

/* 'Safe' versions with no restrictions on overlap */
/* [v2.2] Changed name from ~Ex to ~_s */
int bdAdd_s(BIGD w, BIGD u, BIGD v);
int bdSubtract_s(BIGD w, BIGD u, BIGD v);
int bdMultiply_s(BIGD w, BIGD u, BIGD v);
int bdDivide_s(BIGD q, BIGD r, BIGD u, BIGD v);
int bdModulo_s(BIGD r, BIGD u, BIGD v);
int bdSquare_s(BIGD s, BIGD x);
/* Keep the faith with the deprecated ~Ex version names */
#define bdAddEx(w, u, v) bdAdd_s((w), (u), (v))
#define bdSubtractEx(w, u, v) bdSubtract_s((w), (u), (v))
#define bdMultiplyEx(w, u, v) bdMultiply_s((w), (u), (v))
#define bdDivideEx(q, r, u, v) bdDivide_s((q), (r), (u), (v))
#define bdModuloEx(r, u, v) bdModulo_s((r), (u), (v))
#define bdSquareEx(s, x) bdSquare_s((s), (x))

/*************************/
/* COMPARISON OPERATIONS */
/*************************/

int bdIsEqual(BIGD a, BIGD b);
	/* Returns true if a == b, else false */

int bdCompare(BIGD a, BIGD b);
	/* Returns sign of (a-b) */

int bdIsZero(BIGD a);
	/* Returns true if a == 0, else false */

int bdIsEven(BIGD a);
int bdIsOdd(BIGD a);
	/* Return TRUE or FALSE */

/*************************/
/* ASSIGNMENT OPERATIONS */
/*************************/

int bdSetEqual(BIGD a, BIGD b);
	/* Sets a = b */

int bdSetZero(BIGD a);
	/* Sets a = 0 */

/****************************/
/* NUMBER THEORY OPERATIONS */
/****************************/

int bdModExp(BIGD y, BIGD x, BIGD e, BIGD m);
	/* Compute y = x^e mod m */

int bdModMult(BIGD a, BIGD x, BIGD y, BIGD m);
	/* Compute a = (x * y) mod m */

int bdModInv(BIGD x, BIGD a, BIGD m);
	/* Compute x = a^-1 mod m */

int bdGcd(BIGD g, BIGD x, BIGD y);
	/* Compute g = gcd(x, y) */

int bdJacobi(BIGD a, BIGD n);
	/* Returns Jacobi(a, n) = {0, +1, -1} */

int bdIsPrime(BIGD b, size_t ntests);
	/* Returns true if passes ntests x Miller-Rabin tests with trial division */

int bdRabinMiller(BIGD b, size_t ntests);
	/* Returns true if passes ntests x Miller-Rabin tests without trial division, b > 2 */

/**********************************************/
/* FUNCTIONS THAT OPERATE WITH A SINGLE DIGIT */
/**********************************************/

int bdSetShort(BIGD b, bdigit_t d);
	/* Converts single digit d into a BIGD b */

int bdShortAdd(BIGD w, BIGD u, bdigit_t d);
	/* Compute w = u + d */

int bdShortSub(BIGD w, BIGD u, bdigit_t d);
	/* Compute w = u - d, return borrow */

int bdShortMult(BIGD w, BIGD u, bdigit_t d);
	/* Compute w = u * d */

int bdShortDiv(BIGD q, BIGD r, BIGD u, bdigit_t d);
	/* Computes quotient q = u / d and remainder r = u mod d */

bdigit_t bdShortMod(BIGD r, BIGD u, bdigit_t d);
	/* Computes r = u mod d. Returns r. */

int bdShortCmp(BIGD a, bdigit_t d);
	/* Returns sign of (a-d) */

/***********************/
/* BIT-WISE OPERATIONS */
/***********************/
/* [v2.1.0] Added ModPowerOf2, Xor, Or and AndBits functions */
/* [v2.2.0] Added NotBits, GetBit, SetBit functions */

size_t bdBitLength(BIGD bd);
	/* Returns number of significant bits in bd */

int bdSetBit(BIGD a, size_t n, int value);
	/* Set bit n (0..nbits-1) with value 1 or 0 */

int bdGetBit(BIGD a, size_t ibit);
	/* Returns value 1 or 0 of bit n (0..nbits-1) */

/* [v2.1.0] Removed restriction on shift size */
void bdShiftLeft(BIGD a, BIGD b, size_t n);
	/* Computes a = b << n */

void bdShiftRight(BIGD a, BIGD b, size_t n);
	/* Computes a = b >> n */

void bdModPowerOf2(BIGD a, size_t L);
	/* Computes a = a mod 2^L */

void bdXorBits(BIGD a, BIGD b, BIGD c);
	/* Computes bitwise operation a = b XOR c */

void bdOrBits(BIGD a, BIGD b, BIGD c);
	/* Computes bitwise operation a = b OR c */

void bdAndBits(BIGD a, BIGD b, BIGD c);
	/* Computes bitwise operation a = b AND c */

void bdNotBits(BIGD a, BIGD b);
	/* Computes bitwise a = NOT b */

/*******************/
/* MISC OPERATIONS */
/*******************/

size_t bdSizeof(BIGD b);
	/* Returns number of significant digits in b */

void bdPrint(BIGD bd, size_t flags);
	/* Print bd to stdout (see flags above) */

/***************/
/* CONVERSIONS */
/***************/
/* [v2.1] Tightened up specs here */

/* All bdConv functions return 0 on error */

/* `Octet' = an 8-bit byte */

size_t bdConvFromOctets(BIGD b, const unsigned char *octets, size_t nbytes);
	/* Converts nbytes octets into big digit b, resizing b if necessary.
	   Returns actual number of digits set, which may be larger than mpSizeof(b). */

size_t bdConvToOctets(BIGD b, unsigned char *octets, size_t nbytes);
	/* Convert big digit b into array of octets, in big-endian order,
	   padding on the left with zeroes to nbytes, or truncating as necessary.
       It returns the exact number of significant bytes <nsig> in the result.
	   If octets is NULL or nbytes == 0 then just return required <nsig>;
       if nbytes== nsig, octets <-    nnnnnn;
       if nbytes > nsig, octets <- 000nnnnnn;
       if nbytes < nsig, octets <-       nnn. */

/* For Hex and Decimal conversions:
   The parameter `smax' in the bdConvFrom functions
   is the size INCLUDING the terminating zero (as in sizeof(s))
   but the bdConvTo functions return the number of characters required
   EXCLUDING the terminating zero.

   The bdConvToHex and bdConvToDecimal functions return the 
   total length of the string they tried to create even if 
   the output string was truncated at a shorter length, 
   so remember to check!.
*/

size_t bdConvFromHex(BIGD b, const char *s);
	/* Convert string s of hex chars into big digit b. 
	   Returns number of significant digits actually set, which could be 0. */

size_t bdConvToHex(BIGD b, char *s, size_t smax);
	/* Convert big digit b into string s of hex characters
	   where s has size smax including the terminating zero.
	   Returns number of chars required for s excluding leading zeroes. 
	   If s is NULL or smax == 0 then just return required size. */

size_t bdConvFromDecimal(BIGD b, const char *s);
	/* Convert string s of decimal chars into big digit b. 
	   Returns number of significant digits actually set, which could be 0. */

size_t bdConvToDecimal(BIGD b, char *s, size_t smax);
	/* Convert big digit b into string s of decimal characters
	   where s has size smax including the terminating zero.
	   Returns number of chars required for s excluding leading zeroes. 
	   If s is NULL or smax == 0 then just return required size. */


/****************************/
/* RANDOM NUMBER OPERATIONS */
/****************************/

/* [Version 2.1: bdRandDigit and bdRandomBits moved to bigdRand.h] */

size_t bdSetRandTest(BIGD a, size_t ndigits);
	/* Fill a with [1,ndigits] pseudo-random digits. Returns # digits set.
	   Randomly clears a random number of bits in the most significant digit.
	   For testing only. Not crypto secure. Not thread safe. */

/* TYPEDEF for user-defined random byte generator function */
typedef int (* BD_RANDFUNC)(unsigned char *buf, size_t nbytes, const unsigned char *seed, size_t seedlen);

int bdRandomSeeded(BIGD a, size_t nbits, const unsigned char *seed, 
	size_t seedlen, BD_RANDFUNC RandFunc);
	/* Generate a random number nbits long using RandFunc */

int bdGeneratePrime(BIGD b, size_t nbits, size_t ntests, const unsigned char *seed, 
	size_t seedlen, BD_RANDFUNC RandFunc);
	/* Generate a prime number nbits long; carry out ntests x primality tests */

/****************/
/* VERSION INFO */
/****************/
int bdVersion(void);
	/* Returns version number = major*1000+minor*100+release*10+uses_asm(0|1)+uses_64(0|2)+uses_noalloc(0|5) */


#undef T /* (for opaque BIGD pointer) */

#ifdef __cplusplus
}
#endif

#endif /* BIGD_H_ */
