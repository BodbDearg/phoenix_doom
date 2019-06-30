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

// DC: this is in the 3DO SDK - define for now to fix compile errors
typedef int32_t Item;

#define TICKSPERSEC 60

/* In Graphics */
extern uint8_t *VideoPointer;
extern uint32_t FramebufferWidth;
extern uint32_t FramebufferHeight;
extern void DrawShape(const uint32_t x1, const uint32_t y1, const struct CelControlBlock* const pShape) noexcept;
extern void DrawMShape(const uint32_t x1, const uint32_t y1, const struct CelControlBlock* const pShape) noexcept;
extern void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept;
extern const struct CelControlBlock* GetShapeIndexPtr(const void* ShapeArrayPtr, uint32_t Index) noexcept;
extern void DrawRezShape(uint32_t x, uint32_t y, uint32_t RezNum) noexcept;

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

extern uint32_t LastJoyButtons[4];      /* Save the previous joypad bits */
extern uint32_t ReadJoyButtons(uint32_t Which) noexcept;

/* Misc routines */
extern void LongWordToAscii(uint32_t Input, char* AsciiPtr) noexcept;
extern uint32_t SaveAFile(uint8_t* FileName, void* data, uint32_t Length) noexcept;

/* Time and Events */
extern uint32_t LastTick;
extern uint32_t ReadTick() noexcept;

/* Misc */
extern Item VideoItem;          /* 3DO Specific! */
extern Item VideoScreen;        /* 3DO Specific! */
