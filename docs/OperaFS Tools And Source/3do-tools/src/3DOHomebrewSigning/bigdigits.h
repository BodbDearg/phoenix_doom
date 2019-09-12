/* $Id: bigdigits.h $ */

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

/* Interface to BigDigits "mp" functions */

#ifndef BIGDIGITS_H_
#define BIGDIGITS_H_ 1

#include "bigdtypes.h"

/**** USER CONFIGURABLE SECTION ****/

/* Define type and size of DIGIT */

/* [v2.1] Changed to use C99 exact-width types so it will compile with 64-bit compilers. */
/* [v2.2] Put macros for exact-width types in separate file "bigdtypes.h" */

typedef uint32_t DIGIT_T;
typedef uint16_t HALF_DIGIT_T;

/* Sizes to match */
#define MAX_DIGIT 0xffffffffUL
#define MAX_HALF_DIGIT 0xffffUL	/* NB 'L' */
#define BITS_PER_DIGIT 32
#define HIBITMASK 0x80000000UL

/*	[v2.2] added option to avoid allocating temp storage
	and use fixed automatic arrays instead.
	Define NO_ALLOCS to invoke this.
	Only applicable to mp functions. Do not use with bd.
*/
/* Specifiy the maximum number of digits allowed in a temp mp array
   -- ignored unless NO_ALLOCS is defined */ 
#ifdef NO_ALLOCS
#define MAX_FIXED_DIGITS (8192 / BITS_PER_DIGIT)
#endif

/**** END OF USER CONFIGURABLE SECTION ****/

#define BITS_PER_HALF_DIGIT (BITS_PER_DIGIT / 2)
#define BYTES_PER_DIGIT (BITS_PER_DIGIT / 8)

/* Useful macros */
#define LOHALF(x) ((DIGIT_T)((x) & MAX_HALF_DIGIT))
#define HIHALF(x) ((DIGIT_T)((x) >> BITS_PER_HALF_DIGIT & MAX_HALF_DIGIT))
#define TOHIGH(x) ((DIGIT_T)((x) << BITS_PER_HALF_DIGIT))

#define ISODD(x) ((x) & 0x1)
#define ISEVEN(x) (!ISODD(x))

#define mpISODD(x, n) (x[0] & 0x1)
#define mpISEVEN(x, n) (!(x[0] & 0x1))

#define mpNEXTBITMASK(mask, n) do{if(mask==1){mask=HIBITMASK;n--;}else{mask>>=1;}}while(0)

#ifdef __cplusplus
extern "C" {
#endif

volatile char *copyright_notice(void);
	/* Forces linker to include copyright notice in executable */

/*	
 * Multiple precision calculations	
 * Using known, equal ndigits
 * except where noted
*/

/*************************/
/* ARITHMETIC OPERATIONS */
/*************************/

DIGIT_T mpAdd(DIGIT_T w[], const DIGIT_T u[], const DIGIT_T v[], size_t ndigits);
	/* Computes w = u + v, returns carry */

DIGIT_T mpSubtract(DIGIT_T w[], const DIGIT_T u[], const DIGIT_T v[], size_t ndigits);
	/* Computes w = u - v, returns borrow */

int mpMultiply(DIGIT_T w[], const DIGIT_T u[], const DIGIT_T v[], size_t ndigits);
	/* Computes product w = u * v 
	   u, v = ndigits long; w = 2 * ndigits long */

int mpDivide(DIGIT_T q[], DIGIT_T r[], const DIGIT_T u[], 
	size_t udigits, DIGIT_T v[], size_t vdigits);
	/* Computes quotient q = u / v and remainder r = u mod v 
	   q, r, u = udigits long; v = vdigits long
	   Warning: Trashes q and r first */

int mpModulo(DIGIT_T r[], const DIGIT_T u[], size_t udigits, DIGIT_T v[], size_t vdigits);
	/* Computes r = u mod v 
	   u = udigits long; r, v = vdigits long */

int mpSquare(DIGIT_T w[], const DIGIT_T x[], size_t ndigits);
	/* Computes square w = x^2
	   x = ndigits long; w = 2 * ndigits long */

int mpSqrt(DIGIT_T s[], const DIGIT_T x[], size_t ndigits);
	/* Computes integer square root s = floor(sqrt(x)) */

/*************************/
/* COMPARISON OPERATIONS */
/*************************/

int mpEqual(const DIGIT_T a[], const DIGIT_T b[], size_t ndigits);
	/* Returns true if a == b, else false */

int mpCompare(const DIGIT_T a[], const DIGIT_T b[], size_t ndigits);
	/* Returns sign of (a - b) */

int mpIsZero(const DIGIT_T a[], size_t ndigits);
	/* Returns true if a == 0, else false */

/****************************/
/* NUMBER THEORY OPERATIONS */
/****************************/

/* [v2.2] removed `const' restriction on m[] for mpModMult and mpModExp */

int mpModMult(DIGIT_T a[], const DIGIT_T x[], const DIGIT_T y[], DIGIT_T m[], size_t ndigits);
	/* Computes a = (x * y) mod m */

int mpModExp(DIGIT_T y[], const DIGIT_T x[], const DIGIT_T e[], DIGIT_T m[], size_t ndigits);
	/* Computes y = x^e mod m */

int mpModInv(DIGIT_T inv[], const DIGIT_T u[], const DIGIT_T v[], size_t ndigits);
	/* Computes inv = u^-1 mod v */

int mpGcd(DIGIT_T g[], const DIGIT_T x[], const DIGIT_T y[], size_t ndigits);
	/* Computes g = gcd(x, y) */

/* [v2.2] Added Jacobi (and Legendre) symbol fn */
int mpJacobi(const DIGIT_T a[], const DIGIT_T n[], size_t ndigits);
	/* Returns Jacobi(a, n) = {0, +1, -1} */

/**********************/
/* BITWISE OPERATIONS */
/**********************/

size_t mpBitLength(const DIGIT_T a[], size_t ndigits);
	/* Returns number of significant bits in a */

DIGIT_T mpShiftLeft(DIGIT_T a[], const DIGIT_T b[], size_t x, size_t ndigits);
	/* Computes a = b << x */

DIGIT_T mpShiftRight(DIGIT_T a[], const DIGIT_T b[], size_t x, size_t ndigits);
	/* Computes a = b >> x */

void mpXorBits(DIGIT_T a[], const DIGIT_T b[], const DIGIT_T c[], size_t ndigits);
	/* Computes bitwise a = b XOR c */

void mpOrBits(DIGIT_T a[], const DIGIT_T b[], const DIGIT_T c[], size_t ndigits);
	/* Computes bitwise a = b OR c */

void mpAndBits(DIGIT_T a[], const DIGIT_T b[], const DIGIT_T c[], size_t ndigits);
	/* Computes bitwise a = b AND c */

void mpNotBits(DIGIT_T a[], const DIGIT_T b[], size_t ndigits);
	/* Computes bitwise a = NOT b */

void mpModPowerOf2(DIGIT_T a[], size_t ndigits, size_t L);
	/* Computes a = a mod 2^L */

int mpSetBit(DIGIT_T a[], size_t ndigits, size_t n, int value);
	/* Set bit n (0..nbits-1) with value 1 or 0 */

int mpGetBit(DIGIT_T a[], size_t ndigits, size_t n);
	/* Returns value 1 or 0 of bit n (0..nbits-1) */

/*************************/
/* ASSIGNMENT OPERATIONS */
/*************************/

volatile DIGIT_T mpSetZero(volatile DIGIT_T a[], size_t ndigits);
	/* Sets a = 0 */

void mpSetDigit(DIGIT_T a[], DIGIT_T d, size_t ndigits);
	/* Sets a = d where d is a single digit */

void mpSetEqual(DIGIT_T a[], const DIGIT_T b[], size_t ndigits);
	/* Sets a = b */

/****************************/
/* SIGNED INTEGER FUNCTIONS */
/****************************/

int mpIsNegative(const DIGIT_T x[], size_t ndigits);
	/* Returns TRUE (1) if x < 0, else FALSE (0) */

int mpChs(DIGIT_T x[], const DIGIT_T y[], size_t ndigits);
	/* Sets x = -y */

int mpAbs(DIGIT_T x[], const DIGIT_T y[], size_t ndigits);
	/* Sets x = |y| */


/**********************/
/* OTHER MP UTILITIES */
/**********************/

size_t mpSizeof(const DIGIT_T a[], size_t ndigits);
	/* Returns size i.e. number of significant non-zero digits in a */

int mpIsPrime(DIGIT_T w[], size_t ndigits, size_t t);
	/* Returns true if w > 2 is a probable prime 
	   t tests using FIPS-186-2/Rabin-Miller */

int mpRabinMiller(DIGIT_T w[], size_t ndigits, size_t t);
	/* Just the FIPS-186-2/Rabin-Miller test 
	   without trial division by small primes */

/**********************************************/
/* FUNCTIONS THAT OPERATE WITH A SINGLE DIGIT */
/**********************************************/

DIGIT_T mpShortAdd(DIGIT_T w[], const DIGIT_T u[], DIGIT_T d, size_t ndigits);
	/* Computes w = u + d, returns carry */

DIGIT_T mpShortSub(DIGIT_T w[], const DIGIT_T u[], DIGIT_T d, size_t ndigits);
	/* Computes w = u - d, returns borrow */

DIGIT_T mpShortMult(DIGIT_T p[], const DIGIT_T x[], DIGIT_T d, size_t ndigits);
	/* Computes product p = x * d */

DIGIT_T mpShortDiv(DIGIT_T q[], const DIGIT_T u[], DIGIT_T d, size_t ndigits);
	/* Computes q = u / d, returns remainder */

DIGIT_T mpShortMod(const DIGIT_T a[], DIGIT_T d, size_t ndigits);
	/* Returns r = a mod d */

int mpShortCmp(const DIGIT_T a[], DIGIT_T d, size_t ndigits);
	/* Returns sign of (a - d) where d is a single digit */

/**************************************/
/* CORE SINGLE PRECISION CALCULATIONS */
/* (double where necessary)           */
/**************************************/

/* NOTE spMultiply and spDivide are used by almost all mp functions. 
   Using the Intel MASM alternatives gives significant speed improvements
   -- to use, define USE_SPASM as a preprocessor directive.
   [v2.2] Removed references to spasm* versions.
*/

int spMultiply(DIGIT_T p[2], DIGIT_T x, DIGIT_T y);
	/* Computes p = x * y */

DIGIT_T spDivide(DIGIT_T *q, DIGIT_T *r, const DIGIT_T u[2], DIGIT_T v);
	/* Computes quotient q = u / v, remainder r = u mod v */

/****************************/
/* RANDOM NUMBER FUNCTIONS  */
/* CAUTION: NOT thread-safe */
/****************************/

DIGIT_T spSimpleRand(DIGIT_T lower, DIGIT_T upper);
	/* Returns a simple pseudo-random digit between lower and upper */

/* [Version 2.1: spBetterRand moved to spRandom.h] */

/*******************/
/* PRINT UTILITIES */
/*******************/

void mpPrint(const DIGIT_T *p, size_t len);
	/* Print all digits incl leading zero digits */
void mpPrintNL(const DIGIT_T *p, size_t len);
	/* Print all digits with newlines */
void mpPrintTrim(const DIGIT_T *p, size_t len);
	/* Print but trim leading zero digits */
void mpPrintTrimNL(const DIGIT_T *p, size_t len);
	/* Print, trim leading zeroes, add newlines */

/************************/
/* CONVERSION UTILITIES */
/************************/

size_t mpConvFromOctets(DIGIT_T a[], size_t ndigits, const unsigned char *c, size_t nbytes);
	/* Converts nbytes octets into big digit a of max size ndigits
	   Returns actual number of digits set */
size_t mpConvToOctets(const DIGIT_T a[], size_t ndigits, unsigned char *c, size_t nbytes);
	/* Convert big digit a into string of octets, in big-endian order,
	   padding to nbytes or truncating if necessary.
	   Return number of non-zero octets required. */
size_t mpConvFromDecimal(DIGIT_T a[], size_t ndigits, const char *s);
	/* Convert a string in decimal format to a big digit.
	   Return actual number of (possibly zero) digits set. */
size_t mpConvToDecimal(const DIGIT_T a[], size_t ndigits, char *s, size_t smax);
	/* Convert big digit a into a string in decimal format, 
	   where s has size smax including the terminating zero.
	   Return number of chars required excluding leading zeroes. */
size_t mpConvFromHex(DIGIT_T a[], size_t ndigits, const char *s);
	/* Convert a string in hexadecimal format to a big digit.
	   Return actual number of (possibly zero) digits set. */
size_t mpConvToHex(const DIGIT_T a[], size_t ndigits, char *s, size_t smax);
	/* Convert big digit a into a string in hexadecimal format, 
	   where s has size smax including the terminating zero.
	   Return number of chars required excluding leading zeroes. */

/****************/
/* VERSION INFO */
/****************/
int mpVersion(void);
	/* Returns version number = major*1000+minor*100+release*10+uses_asm(0|1)+uses_64(0|2)+uses_noalloc(0|5) */

/*************************************************/
/* MEMORY ALLOCATION FUNCTIONS - USED INTERNALLY */
/*************************************************/
/* [v2.2] added option to avoid memory allocation if NO_ALLOCS is defined */
#ifndef NO_ALLOCS
DIGIT_T *mpAlloc(size_t ndigits);
void mpFree(DIGIT_T **p);
#endif
void mpFail(char *msg);

/* Clean up by zeroising and freeing allocated memory */
#ifdef NO_ALLOCS
#define mpDESTROY(b, n) do{if(b)mpSetZero(b,n);}while(0)
#else
#define mpDESTROY(b, n) do{if(b)mpSetZero(b,n);mpFree(&b);}while(0)
#endif


#ifdef __cplusplus
}
#endif

#endif	/* BIGDIGITS_H_ */
