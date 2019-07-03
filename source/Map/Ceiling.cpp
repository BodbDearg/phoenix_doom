#include "Ceiling.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Floor.h"
#include "Game/Tick.h"
#include "MapData.h"
#include "Specials.h"

/**********************************

    Local structures for moving ceilings

**********************************/

#define CEILSPEED (2<<FRACBITS) // Speed of crushing ceilings 

typedef struct ceiling_t {
    sector_t *sector;       // Pointer to the sector structure 
    struct ceiling_t *next; // Next entry in the linked list 
    Fixed bottomheight; // Lowest point to move to 
    Fixed topheight;    // Highest point to move to 
    Fixed speed;        // Speed of motion 
    uint32_t tag;       // ID 
    int direction;      // 1 = up, 0 = waiting, -1 = down 
    int olddirection;   // Previous direction to restart with 
    ceiling_e type;     // Type of ceiling 
    bool crush;         // Can it crush? 
} ceiling_t;

static ceiling_t* gMainCeilingPtr;   // Pointer to the first ceiling in list 

/**********************************

    Insert a new ceiling record into the linked list,
    use a simple insert into the head to add.

**********************************/

static void AddActiveCeiling(ceiling_t *CeilingPtr)
{
    CeilingPtr->next = gMainCeilingPtr;  // Insert into the chain 
    gMainCeilingPtr = CeilingPtr;        // Set this one as the first 
}

/**********************************

    Remove a ceiling record from the linked list.
    I also call RemoveThinker to actually perform the
    memory disposal.

**********************************/

static void RemoveActiveCeiling(ceiling_t *CeilingPtrKill)
{
    ceiling_t *CeilingPtr;  // Temp pointer 
    ceiling_t *PrevPtr;     // Previous pointer (For linked list) 

    PrevPtr = 0;            // Init the previous pointer 
    CeilingPtr = gMainCeilingPtr;    // Get the main list entry 
    while (CeilingPtr) {        // Failsafe! 
        if (CeilingPtr==CeilingPtrKill) {   // Master pointer matches? 
            CeilingPtr = CeilingPtr->next;  // Get the next link 
            if (!PrevPtr) {             // First one in chain? 
                gMainCeilingPtr = CeilingPtr;    // Make the next one the new head 
            } else {
                PrevPtr->next = CeilingPtr; // Remove the link 
            }
            break;      // Get out of the loop 
        }
        PrevPtr = CeilingPtr;       // Make the current pointer the previous one 
        CeilingPtr = CeilingPtr->next;  // Get the next link 
    }
    CeilingPtrKill->sector->specialdata = 0;    // Unlink from the sector 
    RemoveThinker(CeilingPtrKill);  // Remove the thinker 
}

/**********************************

    Think logic.
    This code will move the ceiling up and down.

**********************************/

static void T_MoveCeiling(ceiling_t *ceiling)
{
    result_e res;
    
    if (ceiling->direction == 1) {        // Going up? 
        res = T_MovePlane(ceiling->sector,ceiling->speed,   // Move it 
                ceiling->topheight,false,true,ceiling->direction);
        
        if (gTick2) {   // Sound? 
            S_StartSound(&ceiling->sector->SoundX,sfx_stnmov);
        }
        
        if (res == pastdest) {      // Did it reach the top? 
            switch(ceiling->type) {
                case raiseToHighest:
                    RemoveActiveCeiling(ceiling);   // Remove the thinker 
                    break;
                case fastCrushAndRaise:
                case crushAndRaise:
                    ceiling->direction = -1;        // Go down now 
                case lowerToFloor:                  // DC: nothing to be done for these cases 
                case lowerAndCrush:
                    break;
            }
        }
    } else if (ceiling->direction == -1) {        // Going down? 
        res = T_MovePlane(ceiling->sector,ceiling->speed,   // Move it 
            ceiling->bottomheight,ceiling->crush,true,ceiling->direction);
        
        if (gTick2) {   // Time for sound? 
            S_StartSound(&ceiling->sector->SoundX,sfx_stnmov);
        }
        
        if (res == pastdest) {  // Reached the bottom? 
            switch (ceiling->type) {
                case crushAndRaise:
                    ceiling->speed = CEILSPEED;     // Reset the speed ALWAYS 
                case fastCrushAndRaise:
                    ceiling->direction = 1;         // Go up now 
                    break;
                case lowerAndCrush:
                case lowerToFloor:
                    RemoveActiveCeiling(ceiling);   // Remove it 
                case raiseToHighest:                // DC: nothing to be done for these cases 
                    break;
            }
        } else if (res == crushed) {    // Is it crushing something? 
            switch (ceiling->type) {
                case crushAndRaise:
                case lowerAndCrush:
                    ceiling->speed = (CEILSPEED/8);     // Slow down for more fun! 
                case lowerToFloor:                      // DC: nothing to be done for these cases 
                case raiseToHighest:
                case fastCrushAndRaise:
                    break;
            }
        }
    }
}

/**********************************

    Given a tag ID number, activate a ceiling that is currently
    inactive.

**********************************/

static void ActivateInStasisCeiling(uint32_t tag)
{
    ceiling_t *CeilingPtr;      // Temp pointer 

    CeilingPtr = gMainCeilingPtr;    // Get the main list entry 
    if (CeilingPtr) {       // Scan all entries in the thinker list 
        do {
            if (CeilingPtr->tag == tag && !CeilingPtr->direction) { // Match? 
                CeilingPtr->direction = CeilingPtr->olddirection;   // Restart the platform 
                ChangeThinkCode(CeilingPtr,(ThinkerFunc) T_MoveCeiling);  // Reset code 
            }
            CeilingPtr = CeilingPtr->next;  // Get the next link 
        } while (CeilingPtr);
    }
}

/**********************************

    Move a ceiling up/down and all around!

**********************************/

bool EV_DoCeiling(line_t *line, ceiling_e  type)
{
    bool rtn;       // Return value 
    uint32_t secnum;        // Sector being scanned 
    sector_t *sec;
    ceiling_t *ceiling;

    secnum = -1;
    rtn = false;

    // Reactivate in-stasis ceilings...for certain types. 
    switch(type) {
        case fastCrushAndRaise:
        case crushAndRaise:
            ActivateInStasisCeiling(line->tag);
        // DC: TODO: replace this with if() instead
        case lowerToFloor:
        case raiseToHighest:
        case lowerAndCrush:
            break;
    }

    while ((secnum = P_FindSectorFromLineTag(line,secnum)) != -1) {
        sec = &gpSectors[secnum];   // Get the sector pointer 
        if (sec->specialdata) {     // Already something is here? 
            continue;
        }

        // New ceiling thinker 
        rtn = true;
        ceiling = (ceiling_t *)AddThinker((ThinkerFunc) T_MoveCeiling,sizeof(ceiling_t));
        sec->specialdata = ceiling;     // Pass the pointer 
        ceiling->sector = sec;          // Save the sector ptr 
        ceiling->tag = sec->tag;        // Set the tag number 
        ceiling->crush = false;         // Assume it can't crush 
        ceiling->type = type;           // Set the ceiling type 
        switch(type) {
        case fastCrushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = sec->ceilingheight;
            ceiling->bottomheight = sec->floorheight;
            ceiling->direction = -1;            // Down 
            ceiling->speed = CEILSPEED*2;       // Go down fast! 
            break;
        case crushAndRaise:
            ceiling->crush = true;
            ceiling->topheight = sec->ceilingheight;    // Floor and ceiling 
        case lowerAndCrush:
        case lowerToFloor:
            ceiling->bottomheight = sec->floorheight;   // To the floor! 
            ceiling->direction = -1;        // Down 
            ceiling->speed = CEILSPEED;
            break;
        case raiseToHighest:
            ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
            ceiling->direction = 1;         // Go up 
            ceiling->speed = CEILSPEED;
            break;
        }
        AddActiveCeiling(ceiling);      // Add the ceiling to the list 
    }
    return rtn;
}

/**********************************

    Deactivate a ceiling
    Only affect ceilings that have the same ID tag
    as the line segment and also are active.

**********************************/

bool EV_CeilingCrushStop(line_t *line)
{
    uint32_t tag;       // ID Tag to scan for 
    bool rtn;   // Return value 
    ceiling_t *CeilingPtr;      // Temp pointer 

    rtn = false;
    tag = line->tag;            // Get the tag to look for 
    CeilingPtr = gMainCeilingPtr;    // Get the main list entry 
    while (CeilingPtr) {        // Scan all entries in the thinker list 
        if (CeilingPtr->tag == tag && CeilingPtr->direction) {  // Match? 
            CeilingPtr->olddirection = CeilingPtr->direction;   // Save the platform's state 
            ChangeThinkCode(CeilingPtr,0);  // Shut down 
            CeilingPtr->direction = 0;      // In statis 
            rtn = true;
        }
        CeilingPtr = CeilingPtr->next;  // Get the next link 
    }
    return rtn;
}

/**********************************

    Reset the master ceiling pointer
    Called from InitThinkers

**********************************/

void ResetCeilings(void)
{
    gMainCeilingPtr = 0;     // Discard the links 
}
