#pragma once

#include <windows.h>

class CaptureOverlay {
public:
    explicit CaptureOverlay(HINSTANCE instance);
    bool SelectArea(HWND owner, RECT* selectedRect);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    RECT NormalizeRect(const RECT& rect) const;
    void UpdateSelection(POINT point);
    void Paint(HWND hwnd);
    bool EnsureClassRegistered();

    HINSTANCE instance_;
    HWND hwnd_ = nullptr;
    RECT virtualScreen_ {};
    RECT selection_ {};
    POINT startPoint_ {};
    bool dragging_ = false;
    bool accepted_ = false;
    bool classRegistered_ = false;
    HBITMAP backgroundBitmap_ = nullptr;
    HBITMAP dimmedBitmap_ = nullptr;
};
