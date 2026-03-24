#include "DarkMode.h"

#include <dwmapi.h>

namespace {
constexpr COLORREF kBackgroundColor = RGB(28, 28, 30);
constexpr COLORREF kControlColor = RGB(42, 42, 46);
constexpr COLORREF kTextColor = RGB(230, 230, 230);
constexpr COLORREF kAccentColor = RGB(82, 135, 255);
}

namespace DarkMode {

void ApplyImmersiveDarkMode(HWND hwnd) {
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
}

HBRUSH GetBackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(kBackgroundColor);
    return brush;
}

COLORREF GetBackgroundColor() {
    return kBackgroundColor;
}

COLORREF GetControlColor() {
    return kControlColor;
}

COLORREF GetTextColor() {
    return kTextColor;
}

COLORREF GetAccentColor() {
    return kAccentColor;
}

}
