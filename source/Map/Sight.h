#pragma once

struct mobj_t;

// DC: Note - made use of the reject map optional, as it appears to be an unreliable check in some cases.
// I made the mistake of trying to use the reject LUT for shooting line of sight calculations, and boy was I sorry...
// I don't know why the reject is so unreliable on the 3DO maps, perhaps down to bugs in whatever node builder was used?
bool CheckSight(mobj_t& t1, mobj_t& t2, const bool bUseRejectMap) noexcept;
