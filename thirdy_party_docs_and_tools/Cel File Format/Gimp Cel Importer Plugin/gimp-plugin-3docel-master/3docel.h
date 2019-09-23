/*
 * 3docel.h:	Structure and bit definitions for 3DO cel files.
 *
 * 		Uses information obtained from the 3DO Portfolio programmers
 * 		toolkit, which may not be strictly kosher.
 *
 * Leo L. Schwab					2012.10.02
 */
#ifndef _3DOCEL_H
#define _3DOCEL_H


#ifndef _STDINT_H
#include <stdint.h>
#endif

#ifndef _ENDIAN_H
#include <endian.h>
#endif

#ifndef	__BITTWIDDLE_H
#include "bittwiddle.h"
#endif


/***************************************************************************
 * Chunk ID values.
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define MKID4(a,b,c,d)	((uint32_t) a | (b<<8) | (c<<16) | (d<<24))
#else
  #define MKID4(a,b,c,d)	((uint32_t) (a<<24) | (b<<16) | (c<<8) | d)
#endif


#define CHUNK_3DO	MKID4('3','D','O',' ')   /* '3DO ' wrapper chunk */
#define CHUNK_IMAG	MKID4('I','M','A','G')   /* 'IMAG' the image control chunk */
#define CHUNK_PDAT	MKID4('P','D','A','T')   /* 'PDAT' pixel data */
#define CHUNK_CCB	MKID4('C','C','B',' ')   /* 'CCB ' cel control */
#define CHUNK_ANIM	MKID4('A','N','I','M')   /* 'ANIM' ANIM chunk */
#define CHUNK_PLUT	MKID4('P','L','U','T')   /* 'PLUT' pixel lookup table */
#define CHUNK_VDL	MKID4('V','D','L',' ')   /* 'VDL ' VDL chunk */
#define CHUNK_CPYR	MKID4('C','P','Y','R')   /* 'CPYR' copyright text*/
#define CHUNK_DESC	MKID4('D','E','S','C')   /* 'DESC' description text */
#define CHUNK_KWRD	MKID4('K','W','R','D')   /* 'KWRD' keyword text */
#define CHUNK_CRDT	MKID4('C','R','D','T')   /* 'CRDT' credits text */
#define CHUNK_XTRA	MKID4('X','T','R','A')   /* 'XTRA' 3DO Animator creates these */


/***************************************************************************
 * On-disk representations.  All fields are stored in big-endian format.
 *
 * These data were typically loaded from disc directly into memory, patched up,
 * and fed directly to the HW, so they're not exactly what you'd call portable.
 */
/*
 * 3DO framebuffer pixels have this format.
 */
typedef	uint16_t	XRGB1555;


typedef struct chunk_header {
	uint32_t	ID;
	uint32_t	size;	/*  Includes size of this header.  */
} chunk_header;


/*
 * Cel Control Block Chunk
 */
typedef struct CCC_chunk {
	chunk_header	header;		/*  ID = CHUNK_CCB  */
	uint32_t	ccc_version;	/* version number of the scob data structure.  0 for now */

	uint32_t	ccc_Flags;	/* 32 bits of CCB flags       */
	uint32_t	ccc_NextPtr;	/* HW pointer to next CCB.    */
	uint32_t	ccc_CelData;	/* HW pointer to pixel data.  */
	uint32_t	ccc_PLUTPtr;	/* HW pointer to PLUT data.   */

	int32_t		ccc_X;
	int32_t		ccc_Y;
	int32_t		ccc_hdx;
	int32_t		ccc_hdy;
	int32_t		ccc_vdx;
	int32_t		ccc_vdy;
	int32_t		ccc_ddx;
	int32_t		ccc_ddy;
	uint16_t	ccc_PPMP1;
	uint16_t	ccc_PPMP0;
	uint32_t	ccc_PRE0;	/* Sprite Preamble Word 0 */
	uint32_t	ccc_PRE1;	/* Sprite Preamble Word 1 */

	int32_t		ccc_Width;	/* Saved by editor programs;  */
	int32_t		ccc_Height;	/*  not interpreted by HW     */
} CCC_chunk;

/*
 * CCB control word flags
 */
#define	CCB_SKIP			31:31	/*  Skip to next cel  */
#define	CCB_LAST			30:30	/*  Last cel in list  */

#define	CCB_NXTPTRTYPE			29:29
#define	CCB_NXTPTRTYPE_RELATIVE		0
#define	CCB_NXTPTRTYPE_ABSOLUTE		1

#define	CCB_SRCPTRTYPE			28:28
#define	CCB_SRCPTRTYPE_RELATIVE		0
#define	CCB_SRCPTRTYPE_ABSOLUTE		1

#define	CCB_PLUTPTRTYPE			27:27
#define	CCB_PLUTPTRTYPE_RELATIVE	0
#define	CCB_PLUTPTRTYPE_ABSOLUTE	1

#define	CCB_LDSIZE			26:26	/*  Load ccc_[hv]d[xy] fields  */
#define	CCB_LDPRS			25:25	/*  Load ccc_dd[xy] fields  */
#define	CCB_LDPPMP			24:24	/*  Load ccc_PPMPC field  */
#define	CCB_LDPLUT			23:23	/*  Load ccc_PLUTPtr field  */

#define	CCB_PRELOC			22:22	/*  Location of PRE[01]  */
#define	CCB_PRELOC_CELDATA		0
#define	CCB_PRELOC_CCB			1

#define	CCB_YOXY			21:21	/*  Use ccc_[XY] values  */
#define	CCB_ACSC			20:20	/*  Enable cel super-clipping  */
#define	CCB_ALSC			19:19	/*  Enable line super-clipping  */
#define	CCB_ACW				18:18	/*  Enable clockwise rendering  */
#define	CCB_ACCW			17:17	/*  Enable counterclockwise rendering*/
#define	CCB_TWD				16:16	/*  Terminate Wrong Direction  */
#define	CCB_LCE				15:15	/*  Sync corner engines  */

#define	CCB_ACE				14:14	/*  Corner engines to use  */
#define	CCB_ACE_ONE			0
#define	CCB_ACE_BOTH			1

#define	CCB_MARIA			12:12	/*  Handling of magnified pixels  */
#define	CCB_MARIA_FILL			0
#define	CCB_MARIA_NOFILL		1

#define	CCB_PXOR			11:11	/*  PPMP final stage op.  */
#define	CCB_PXOR_NORMAL			0
#define	CCB_PXOR_USEXOR			1

#define	CCB_USEAV			10:10	/*  Use PIXC AV bits for math funcs */
#define	CCB_PACKED			9:9	/*  1 == Source data is packed  */

#define	CCB_POVER			8:7
#define	CCB_POVER_SRCPIX		0
#define	CCB_POVER_ZERO			2
#define	CCB_POVER_ONE			3

#define	CCB_PLUTPOS			6:6	/*  0 == Get VH value from XY  */
						/*  1 == Get VH value from PLUT  */
#define	CCB_BGND			5:5	/*  Treatment of 000 pixels  */
#define	CCB_BGND_TRANSPARENT		0
#define	CCB_BGND_BLACK			1

#define	CCB_NOBLK			4:4	/*  How to write 000 pixels  */
#define	CCB_NOBLK_100			0
#define	CCB_NOBLK_000			1

#define	CCB_PLUTA			3:0

/* === Cel first preamble word flags === */
#define PRE0_LITERAL			31:31
#define PRE0_BGND			30:30
#define PRE0_reservedA			29:28
#define PRE0_SKIPX			27:24
#define PRE0_reservedB			16:23
#define PRE0_VCNT			15:6
#define PRE0_reservedC			5:5
#define PRE0_LINEAR			4:4
#define PRE0_REP8			3:3

#define PRE0_BPP			2:0
#define PRE0_BPP_1			1
#define PRE0_BPP_2			2
#define PRE0_BPP_4			3
#define PRE0_BPP_6			4
#define PRE0_BPP_8			5
#define PRE0_BPP_16			6


/* Subtract this value from the actual vertical source line count */
#define PRE0_VCNT_PREFETCH		1


/* === Cel second preamble word flags === */
#define PRE1_WOFFSET8			31:24
#define PRE1_WOFFSET10			25:16
#define PRE1_NOSWAP			14:14

#define PRE1_TLLSB			13:12
#define PRE1_TLLSB_ZERO			0
#define PRE1_TLLSB_PDC0			1	/* Normal */
#define PRE1_TLLSB_PDC4			2
#define PRE1_TLLSB_PDC5			3

#define PRE1_LRFORM			11:11
#define PRE1_TLHPCNT			10:0


/* Subtract this value from the actual word offset */
#define PRE1_WOFFSET_PREFETCH		2
/* Subtract this value from the actual pixel count */
#define PRE1_TLHPCNT_PREFETCH		1


/*
 * === PPMPC control word flags ===
 */
#define	PPMPC_1S			15:15	//  0x00008000
#define	PPMPC_1S_PDC			0	//   0x00000000
#define	PPMPC_1S_CFBD			1	//  0x00008000

#define	PPMPC_MS			14:13	//  0x00006000
#define	PPMPC_MS_CCB			0	//   0x00000000
#define	PPMPC_MS_PIN			1	//   0x00002000
#define	PPMPC_MS_PDC			2	//   0x00004000
#define	PPMPC_MS_PDC_MFONLY		3	//   0x00006000

#define	PPMPC_MF			12:10	//  0x00001C00
#define	PPMPC_MF_1			0	//   0x00000000
#define	PPMPC_MF_2			1	//   0x00000400
#define	PPMPC_MF_3			2	//   0x00000800
#define	PPMPC_MF_4			3	//   0x00000C00
#define	PPMPC_MF_5			4	//   0x00001000
#define	PPMPC_MF_6			5	//   0x00001400
#define	PPMPC_MF_7			6	//   0x00001800
#define	PPMPC_MF_8			7	//   0x00001C00

#define	PPMPC_SF			9:8	//  0x00000300
#define	PPMPC_SF_16			0	//   0x00000000
#define	PPMPC_SF_2			1	//   0x00000100
#define	PPMPC_SF_4			2	//   0x00000200
#define	PPMPC_SF_8			3	//   0x00000300

#define	PPMPC_2S			7:6	//  0x000000C0
#define	PPMPC_2S_ZERO			0	//   0x00000000
#define	PPMPC_2S_CCB			1	//   0x00000040
#define	PPMPC_2S_CFBD			2	//   0x00000080
#define	PPMPC_2S_PDC			3	//   0x000000C0

#define	PPMPC_AVSDV			5:4	//  0x0000003E
#define	PPMPC_AVSDV_1			0	//   0x00000000
#define	PPMPC_AVSDV_2			1	//   0x00000010
#define	PPMPC_AVSDV_4			2	//   0x00000020
#define	PPMPC_AVSDV_PDC			3	//   0x00000030

#define	PPMPC_AVWRAP			3:3	//  0x00000008
#define	PPMPC_AVWRAP_CLIP		0
#define	PPMPC_AVWRAP_WRAP		1

#define	PPMPC_AV2SSEX			2:2	//  0x00000004	/*  Sign-EXtend  */
#define	PPMPC_AV2SINVERT		1:1	//  0x00000002

#define	PPMPC_2D			0:0	//  0x00000001
#define	PPMPC_2D_1			0	//   0x00000000
#define	PPMPC_2D_2			1	//   0x00000001

#define	PPMPC_MS_SHIFT  13
#define	PPMPC_MF_SHIFT  10
#define	PPMPC_SF_SHIFT  8
#define	PPMPC_2S_SHIFT  6
#define	PPMPC_AV_SHIFT  1

/* PPMPC_1S_MASK definitions */

/* PPMPC_MS_MASK definitions */

/* PPMPC_MF_MASK definitions */

/* PPMPC_SF_MASK definitions */

/* PPMPC_2S_MASK definitions */

/* PPMPC_AV_MASK definitions (only valid if CCB_USEAV set in ccb_Flags) */

#define	PPMPC_AV_SF2_MASK	0x00000030


/* PPMPC_2D_MASK definitions */


#define	PACK_EOL			0
#define	PACK_LITERAL			1
#define	PACK_TRANSPARENT		2
#define	PACK_PACKED			3



typedef struct PLUT_chunk {
	chunk_header	header;		/*  ID = CHUNK_PLUT  */
	int32_t		numentries;	/*  number of entries in PLUT Table */
	XRGB1555	PLUT[0];	/*  PLUT entries  */
} PLUT_chunk;

typedef struct loop_rec {
	int32_t		loopStart;	/*  start frame for a loop in the animation */
	int32_t		loopEnd;	/*  end frame for a loop in the animation */
	int32_t		repeatCount;	/*  number of times to repeat the looped portion */
	int32_t		repeatDelay;	/*  number of 1/60s of a sec to delay each time thru loop */
} loop_rec;

typedef struct ANIM_chunk {
	chunk_header	header;		/*  ID = CHUNK_ANIM  */
	int32_t		version;	/*  current version = 0 */
	int32_t		animType;	/*  0 = multi-CCB ; 1 = single CCB	*/
	int32_t		numFrames;	/*  number of frames for this animation */
	int32_t		frameRate;	/*  number of 1/60s of a sec to display each frame */
	int32_t		startFrame; 	/*  the first frame in the anim. Can be non zero */
	int32_t		numLoops;	/*  number of loops in loop array. Loops are executed serially */
	loop_rec	loop[0];	/*  array of loop info. see numLoops */
} ANIM_chunk;


typedef struct PDAT_chunk {
	chunk_header	header;
	uint8_t		data[0];
} PDAT_chunk;


#endif	/*  _3DOCEL_H  */
