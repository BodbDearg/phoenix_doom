#pragma once

struct mobj_t;

void A_Look(mobj_t& actor) noexcept;
void A_Chase(mobj_t& actor) noexcept;
void A_FaceTarget(mobj_t& actor) noexcept;
void A_PosAttack(mobj_t& actor) noexcept;
void A_SPosAttack(mobj_t& actor) noexcept;
void A_SpidRefire(mobj_t& actor) noexcept;
void A_TroopAttack(mobj_t& actor) noexcept;
void A_SargAttack(mobj_t& actor) noexcept;
void A_HeadAttack(mobj_t& actor) noexcept;
void A_CyberAttack(mobj_t& actor) noexcept;
void A_BruisAttack(mobj_t& actor) noexcept;
void A_SkullAttack(mobj_t& actor) noexcept;
void A_Scream(mobj_t& actor) noexcept;
void A_XScream(mobj_t& actor) noexcept;
void A_Pain(mobj_t& actor) noexcept;
void A_Fall(mobj_t& actor) noexcept;
void A_Explode(mobj_t& actor) noexcept;
void A_BossDeath(mobj_t& actor) noexcept;
void A_Hoof(mobj_t& actor) noexcept;
void A_Metal(mobj_t& actor) noexcept;
void L_MissileHit(mobj_t& mapObj, mobj_t* const pMissile) noexcept;
void L_SkullBash(mobj_t& mapObj, mobj_t* const pSkull) noexcept;
