#include "Base/Macros.h"

//----------------------------------------------------------------------------------------------------------------------
// UI logic and drawing for the title screen and title credits
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(TitleScreens)

// Run the title screen page, returns 'true' if credits should show after this call
bool runTitleScreen() noexcept;
void runCreditScreens() noexcept;

END_NAMESPACE(TitleScreens)
