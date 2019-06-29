#pragma once

#include <cstdint>

/* DC: header from the 3DO SDK - leaving here for reference for now.
 
#ifndef __TYPES_H
    #include <Types.h>
#endif
*/

typedef uint32_t    Word;       // DC: was 'unsigned int' in original 3DO source
typedef uint8_t     Byte;       // DC: was 'unsigned char' in original 3DO source
typedef uint32_t    LongWord;   // DC: was 'unsigned long' in original 3DO source
typedef uint16_t    Short;      // DC: was 'unsigned short' in original 3DO source
typedef int32_t     Frac;       // DC: was 'long' in original 3DO source
typedef int32_t     Fixed;      // DC: was 'long' in original 3DO source
typedef double      extended;

// DC: this is in the 3DO SDK - define for now to fix compile errors
typedef int32_t Item;

#define TICKSPERSEC 60

#define BLACK 0x0000
#define DARKGREY 0x39CE
#define BROWN 0x4102
#define PURPLE 0x3898
#define BLUE 0x001E
#define DARKGREEN 0x0200
#define ORANGE 0x79C0
#define RED 0x6800
#define BEIGE 0x7A92
#define YELLOW 0x7BC0
#define GREEN 0x0380
#define LIGHTBLUE 0x235E
#define LILAC 0x6A9E
#define PERIWINKLE 0x3A1E
#define LIGHTGREY 0x6318
#define WHITE 0x7FFF
#define SfxActive 1
#define MusicActive 2

/* In Graphics */
extern Byte *VideoPointer;
extern Word FramebufferWidth;
extern Word FramebufferHeight;
extern void DrawShape(const uint32_t x1, const uint32_t y1, const struct CelControlBlock* const pShape) noexcept;
extern void DrawMShape(const uint32_t x1, const uint32_t y1, const struct CelControlBlock* const pShape) noexcept;
extern void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept;
extern const struct CelControlBlock* GetShapeIndexPtr(const void* ShapeArrayPtr, Word Index) noexcept;
extern void DrawRezShape(Word x,Word y,Word RezNum) noexcept;

/* Input handlers */
#define PadDown         0x80000000
#define PadUp           0x40000000
#define PadRight        0x20000000
#define PadLeft         0x10000000
#define PadA            0x08000000
#define PadB            0x04000000
#define PadC            0x02000000
#define PadD            0x00000000
#define PadStart        0x01000000
#define PadX            0x00800000
#define PadRightShift   0x00400000
#define PadLeftShift    0x00200000
#define PadXLeft        0x00100000
#define PadXRight       0x00080000

extern Word LastJoyButtons[4];      /* Save the previous joypad bits */
extern Word ReadJoyButtons(Word Which) noexcept;

/* Misc routines */
extern void LongWordToAscii(LongWord Input,Byte *AsciiPtr) noexcept;
extern Word SaveAFile(Byte *FileName,void *data,LongWord Length) noexcept;

/* Time and Events */
extern LongWord LastTick;
extern LongWord ReadTick() noexcept;

/* Misc */
extern Word SystemState;
extern Item VideoItem;          /* 3DO Specific! */
extern Item VideoScreen;        /* 3DO Specific! */
