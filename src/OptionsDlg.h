#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "GameSettings.h"

// Shows the options dialog modally.
// Returns true if the user pressed OK (settings already saved to registry).
bool ShowOptionsDialog(HWND hParent, GameSettings& settings);
