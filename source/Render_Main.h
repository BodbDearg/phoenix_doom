#include "Doom.h"

extern viswall_t        viswalls[MAXWALLCMDS];          // Visible wall array
extern viswall_t*       lastwallcmd;                    // Pointer to free wall entry
extern visplane_t       visplanes[MAXVISPLANES];        // Visible floor array
extern visplane_t*      lastvisplane;                   // Pointer to free floor entry
extern vissprite_t      vissprites[MAXVISSPRITES];      // Visible sprite array
extern vissprite_t*     vissprite_p;                    // Pointer to free sprite entry
extern Byte             openings[MAXOPENINGS];
extern Byte*            lastopening;
extern Fixed            viewx;                          // Camera x,y,z
extern Fixed            viewy;
extern Fixed            viewz;
extern angle_t          viewangle;                      // Camera angle
extern Fixed            viewcos;                        // Camera sine, cosine from angle
extern Fixed            viewsin;
extern uint32_t         extralight;                     // Bumped light from gun blasts
extern angle_t          clipangle;                      // Leftmost clipping angle
extern angle_t          doubleclipangle;                // Doubled leftmost clipping angle

void R_Init() noexcept;
void R_Setup() noexcept;
void R_RenderPlayerView() noexcept;
