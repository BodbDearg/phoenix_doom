#include "Teleport.h"

#include "Audio/Sound.h"
#include "Audio/Sounds.h"
#include "Base/Tables.h"
#include "Game/Tick.h"
#include "Info.h"
#include "Interactions.h"
#include "Map/Map.h"
#include "Map/MapData.h"
#include "Map/MapUtil.h"
#include "MapObj.h"

//----------------------------------------------------------------------------------------------------------------------
// Kill all monsters around the given spot
//----------------------------------------------------------------------------------------------------------------------
static void P_Telefrag(mobj_t& thing, const Fixed x, const Fixed y) noexcept {
    for (mobj_t* pMObj = gMObjHead.next; pMObj != &gMObjHead; pMObj = pMObj->next) {
        mobj_t& mObj = *pMObj;

        if ((mObj.flags & MF_SHOOTABLE) != 0) {     // Can I kill it?
            const Fixed size = mObj.radius + thing.radius + (4 << FRACBITS);
            Fixed delta = mObj.x - x;   // Differance to object

            if ((delta >= -size) && (delta < size)) {           // In x range?
                delta = mObj.y - y;                             // Y differance
                if ((delta >= -size) && (delta < size)) {       // In y range?
                    DamageMObj(mObj, &thing, &thing, 10000);    // Blast it
                    mObj.flags &= ~(MF_SOLID|MF_SHOOTABLE);     // Gone!
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Teleport a player or monster
//----------------------------------------------------------------------------------------------------------------------
bool EV_Teleport(line_t& line, mobj_t& thing) noexcept {
    // Don't teleport missiles
    if ((thing.flags & MF_MISSILE) != 0) {
        return false;
    }

    // Check which side of the line we are on so you can get out of the teleporter
    vector_t divLine;
    MakeVector(line, divLine);

    if (!PointOnVectorSide(thing.x, thing.y, divLine)) {
        return false;
    }

    // Run through all sectors matching the line tag
    const uint32_t tag = line.tag;

    const sector_t* const pBegSector = gpSectors;
    const sector_t* const pEndSector = pBegSector + gNumSectors;
    
    for (const sector_t* pCurSector = pBegSector; pCurSector < pEndSector; ++pCurSector) {
        if (pCurSector->tag != tag) {
            continue;
        }

        for (mobj_t* pMObj = gMObjHead.next; pMObj != &gMObjHead; pMObj = pMObj->next) {
            mobj_t& mObj = *pMObj;

            if (mObj.InfoPtr != &gMObjInfo[MT_TELEPORTMAN]) {
                continue;   // Not a teleportman
            }

            if (mObj.subsector->sector != pCurSector) {
                continue;   // Wrong sector
            }
            
            const Fixed oldx = thing.x;     // Previous x,y,z
            const Fixed oldy = thing.y;
            const Fixed oldz = thing.z;

            thing.flags |= MF_TELEPORT;                             // Mark as a teleport
            P_Telefrag(thing, mObj.x, mObj.y);                      // Frag everything at dest
            const bool flag = P_TryMove(thing, mObj.x, mObj.y);     // Put it there
            thing.flags &= ~MF_TELEPORT;                            // Clear the flag

            if (!flag) {
                return false;   // (Can't teleport) move is blocked
            }

            thing.z = thing.floorz;     // Set the proper z

            // Spawn teleport fog at source and destination (in front of player)
            mobj_t& fog1 = SpawnMObj(
                oldx,
                oldy,
                oldz,
                gMObjInfo[MT_TFOG]
            );

            S_StartSound(&fog1.x, sfx_telept);

            const angle_t an = mObj.angle >> ANGLETOFINESHIFT;            
            mobj_t& fog2 = SpawnMObj(
                mObj.x + 20 * gFineCosine[an],
                mObj.y + 20 * gFineSine[an],
                thing.z,
                gMObjInfo[MT_TFOG]
            );

            S_StartSound(&fog2.x, sfx_telept);

            if (thing.player) {                                             // Add a timing delay
                thing.reactiontime = TICKSPERSEC + (TICKSPERSEC / 5);       // Don't move for a bit
            }

            thing.angle = mObj.angle;                   // Set the angle
            thing.momx = thing.momy = thing.momz = 0;   // No sliding
            return true;                                // I did it
        }
    }

    return false;   // Didn't teleport
}
