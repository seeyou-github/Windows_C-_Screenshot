#pragma once

#include <string>
#include <windows.h>

struct ScreenshotResult {
    HBITMAP bitmap = nullptr;
    RECT captureRect {};
};

class ScreenshotService {
public:
    ScreenshotResult CaptureArea(const RECT& rect) const;
    bool CopyBitmapToClipboard(HWND owner, HBITMAP bitmap) const;
    bool SaveBitmapToJpeg(HBITMAP bitmap, const std::wstring& filePath) const;
};
