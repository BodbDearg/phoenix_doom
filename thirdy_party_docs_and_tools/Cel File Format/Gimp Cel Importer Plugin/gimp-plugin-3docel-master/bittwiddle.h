/* :ts=8 bk=0
 *
 * bittwiddle.h:	Macros to twiddle bits.  Stolen from the XFree86
 *			sources.
 *
 * Leo L. Schwab					1998.07.13
 *  Further personal refinements			2000.01.12
 */
#ifndef	__BITTWIDDLE_H
#define __BITTWIDDLE_H

/*
 * These macros assume bitfields to be expressed as ternary expressions in the
 * form <highbit>:<lowbit>.  Bits are numbered starting from zero (least
 * significant).  highbit and lowbit are inclusive.  All macros assume 2's
 * complement binary math.
 */
/* Little macro to construct bitmask for contiguous ranges of bits */
#define	BITMASK(t,b)		\
	(((unsigned long) (1UL << (((t) - (b) + 1))) - 1) << (b))
#define	MASKEXPAND(mask)	BITMASK (1?mask, 0?mask)

/* Macro to set specific bitfields (mask has to be a macro x:y) ! */
#define	SetBF(mask,value)	((value) << (0?mask))
#define	GetBF(var,mask)		(((unsigned) ((var) & MASKEXPAND(mask))) >> \
				 (0?mask))

#define	MaskAndSetBF(var,mask,value)	(var) = (((var) & (~MASKEXPAND(mask)) \
						  | SetBF (mask, value)))

/*
 * From 'value', extract value from bitfield 'from' and format it for the
 * bitfield 'to'.  Use of mismatched bitfield sizes is recommended only for
 * the Talented.
 */
#define SetBitField(value,from,to)	SetBF (to, GetBF (value, from))
#define SetBit(n)			(1 << (n))
#define Set8Bits(value)			((value) & 0xff)


#define	MASKFIELD(reg,field)		MASKEXPAND (reg##_##field)
#define	DEF2FIELD(reg,field,def)	SetBF (reg##_##field, \
					       reg##_##field##_##def)
#define	VAL2FIELD(reg,field,val)	SetBF (reg##_##field, val)
#define	VAL2MASKD(reg,field,val)	(SetBF (reg##_##field, val) & \
					 MASKEXPAND (reg##_##field))
#define	FIELD2VAL(reg,field,val)	GetBF (val, reg##_##field)
#define	SETDEF2FIELD(var,reg,field,def)	(((var) & \
					  ~MASKEXPAND (reg##_##field)) | \
					 DEF2FIELD (##reg##, ##field##, ##def##))
#define	SETVAL2FIELD(var,reg,field,val)	(((var) & \
					  ~MASKEXPAND (reg##_##field)) | \
					 VAL2FIELD (##reg##, ##field##, ##val##))
#define	SETVAL2MASKD(var,reg,field,val)	(((var) & \
					  ~MASKEXPAND (reg##_##field)) | \
					 VAL2MASKD (##reg##, ##field##, ##val##))


#endif	/*  __BITTWIDDLE_H  */
