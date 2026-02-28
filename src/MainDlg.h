#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "GameSettings.h"

// Shows the main launcher dialog.
// Returns when the user closes the dialog.
void ShowMainDialog(HINSTANCE hInst, GameSettings& settings);
