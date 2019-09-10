#pragma once

#include "Things/Player.h"

// Faces for our hero
enum spclface_e {
    f_none,         // Normal state
    f_faceleft,     // turn face left
    f_faceright,    // turn face right
    f_hurtbad,      // surprised look when slammed hard
    f_gotgat,       // picked up a weapon smile
    f_mowdown,      // grimace while continuous firing
    NUMSPCLFACES
};

// Describe data on the status bar
struct stbar_t {
    spclface_e  specialFace;            // Which type of special face to make
    bool        gotgibbed;              // Got gibbed
    bool        tryopen[NUMCARDS];      // Tried to open a card or skull door
};

extern stbar_t gStBar;   // Pass messages to the status bar

void ST_Start() noexcept;
void ST_Stop() noexcept;
void ST_Ticker() noexcept;
void ST_Drawer() noexcept;
