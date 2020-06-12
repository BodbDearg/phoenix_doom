#pragma once

#include "Base/Macros.h"

//------------------------------------------------------------------------------------------------------------------------------------------
// Functionality to load and save player progress and preferences.
// Saves to a simple .ini file in the same folder as the config file.
//------------------------------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Prefs)

void load() noexcept;
void save() noexcept;

END_NAMESPACE(Prefs)
