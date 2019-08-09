#pragma once

#include <stdint.h>

//----------------------------------------------------------------------------------------------------------------------
// Utility stuff relating to 3DO CEL files.
// Needed because some resources in 3DO Doom are in the native 3DO 'CEL' format.
// 
// Notes:
//  (1) All data is ASSUMED to be in BIG ENDIAN format - these functions will NOT work otherwise.
//      BE was the native endian format of the 3DO and it's development environment (68K Mac), 
//      hence if you are dealing with CELs or CCBs it is assumed the data is big endian.
//  (2) Most of these functions make tons of simplifying assumptions and shortcuts surrounding CEL
//      files that hold true for 3DO Doom, but which won't work on all 3DO CEL files in general.
//      They also make modifications for 3DO Doom's own specific way of way storing CEL data.
//      If you want a more complete and robust reference for how to decode/encode 3DO CEL files,
//      check out various resources available on the net, including the GIMP CEL plugin:
//          https://github.com/ewhac/gimp-plugin-3docel
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Definition for a 3DO Cel Control Block (CCB).
// This is used for draw operations with the 3DO hardware, and within CEL files on disk.
// Obtained this from the 3DO SDK headers.
//
// More info about it here:
//  https://github.com/trapexit/3DO-information/blob/master/software/sdk/3DO%20Online%20Developer%20Documentation/ppgfldr/smmfldr/cdmfldr/08CDM001.html
//----------------------------------------------------------------------------------------------------------------------
typedef struct CelControlBlock {
	uint32_t    flags;
    uint32_t    nextPtr;        // N.B: can't use CelControlBlock* since that introduces 64-bit incompatibilities!
	uint32_t    sourcePtr;      // N.B: can't use void* since that introduces 64-bit incompatibilities!
	uint32_t    plutPtr;        // N.B: can't use void* since that introduces 64-bit incompatibilities!
	int32_t     xPos;
	int32_t     yPos;
	int32_t     hdx;
	int32_t     hdy;
	int32_t     vdx;
	int32_t     vdy;
	int32_t     hddx;
	int32_t     hddy;
	uint32_t    pixc;
	uint32_t    pre0;
	uint32_t    pre1;
	int32_t     width;
	int32_t     height;
} CelControlBlock;

// Determine the width and height from the given Cel Control Block
extern uint16_t getCCBWidth(const CelControlBlock* const pCCB);
extern uint16_t getCCBHeight(const CelControlBlock* const pCCB);

//----------------------------------------------------------------------------------------------------------------------
// Decodes the CEL image data for a Doom sprite and saves it to the given output.
// The input image data is assumed to follow the pointer to the cel control block.
// The output image data is saved in ARGB1555 little endian format.
//----------------------------------------------------------------------------------------------------------------------
void decodeDoomCelSprite(
    const CelControlBlock* const pCCB,
    uint16_t** pImageOut,
    uint16_t* pImageWidthOut,
    uint16_t* pImageHeightOut
);
