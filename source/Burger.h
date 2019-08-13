#pragma once

#include <cstdint>

/* DC: header from the 3DO SDK - leaving here for reference for now.
 
#ifndef __TYPES_H
    #include <Types.h>
#endif
*/

struct CelImage;

// DC: this is in the 3DO SDK - define for now to fix compile errors
typedef int32_t Item;

// In Graphics 
extern uint8_t* gVideoPointer;
extern uint32_t gFramebufferWidth;
extern uint32_t gFramebufferHeight;

// TODO: move elsewhere
extern void DrawARect(const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height, const uint16_t color) noexcept;

// Input handlers 
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

extern uint32_t gLastJoyButtons[4];      // Save the previous joypad bits 
extern uint32_t ReadJoyButtons(uint32_t Which) noexcept;

// Misc routines 
extern void LongWordToAscii(uint32_t Input, char* AsciiPtr) noexcept;
extern uint32_t SaveAFile(const char* FileName, void* data, uint32_t Length) noexcept;

// Time and Events 
extern uint32_t gLastTick;
extern uint32_t ReadTick() noexcept;

// Misc 
extern Item gVideoItem;         // 3DO Specific! 
extern Item gVideoScreen;       // 3DO Specific! 
