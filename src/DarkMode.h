#pragma once

#include <windows.h>

namespace DarkMode {
void ApplyImmersiveDarkMode(HWND hwnd);
HBRUSH GetBackgroundBrush();
COLORREF GetBackgroundColor();
COLORREF GetControlColor();
COLORREF GetTextColor();
COLORREF GetAccentColor();
}
