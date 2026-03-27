#include "CaptureOverlay.h"

#include "DarkMode.h"

#include <algorithm>
#include <vector>
#include <windowsx.h>

namespace {
constexpr wchar_t kOverlayClassName[] = L"ScreenshotOverlayWindow";

HBITMAP CaptureDesktopSnapshot(const RECT& area) {
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (memoryDc == nullptr || bitmap == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (memoryDc != nullptr) {
            DeleteDC(memoryDc);
        }
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
    // The overlay paints this snapshot back to the screen so users can select against the real desktop.
    BitBlt(memoryDc, 0, 0, width, height, screenDc, area.left, area.top, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, oldObject);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return bitmap;
}

void FillRectAlpha(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void ApplyDimOverlay(HDC dc, const RECT& rect, BYTE alpha) {
    BITMAPINFO info {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right - rect.left;
    info.bmiHeader.biHeight = -(rect.bottom - rect.top);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP overlayBitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (overlayBitmap == nullptr || bits == nullptr) {
        if (overlayBitmap != nullptr) {
            DeleteObject(overlayBitmap);
        }
        return;
    }

    const size_t pixelCount = static_cast<size_t>(rect.right - rect.left) * static_cast<size_t>(rect.bottom - rect.top);
    auto* pixels = reinterpret_cast<DWORD*>(bits);
    for (size_t i = 0; i < pixelCount; ++i) {
        pixels[i] = 0x00000000;
    }

    HDC overlayDc = CreateCompatibleDC(dc);
    HGDIOBJ oldBitmap = SelectObject(overlayDc, overlayBitmap);

    BLENDFUNCTION blend {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = alpha;
    AlphaBlend(dc,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        overlayDc,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        blend);

    SelectObject(overlayDc, oldBitmap);
    DeleteDC(overlayDc);
    DeleteObject(overlayBitmap);
}

HBITMAP CreateDimmedBitmap(HDC referenceDc, HBITMAP sourceBitmap, int width, int height, BYTE alpha) {
    if (sourceBitmap == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }

    HDC targetDc = CreateCompatibleDC(referenceDc);
    HDC sourceDc = CreateCompatibleDC(referenceDc);
    HBITMAP outputBitmap = CreateCompatibleBitmap(referenceDc, width, height);
    if (targetDc == nullptr || sourceDc == nullptr || outputBitmap == nullptr) {
        if (outputBitmap != nullptr) {
            DeleteObject(outputBitmap);
        }
        if (targetDc != nullptr) {
            DeleteDC(targetDc);
        }
        if (sourceDc != nullptr) {
            DeleteDC(sourceDc);
        }
        return nullptr;
    }

    HGDIOBJ oldTarget = SelectObject(targetDc, outputBitmap);
    HGDIOBJ oldSource = SelectObject(sourceDc, sourceBitmap);
    BitBlt(targetDc, 0, 0, width, height, sourceDc, 0, 0, SRCCOPY);
    RECT rect { 0, 0, width, height };
    ApplyDimOverlay(targetDc, rect, alpha);

    SelectObject(sourceDc, oldSource);
    SelectObject(targetDc, oldTarget);
    DeleteDC(sourceDc);
    DeleteDC(targetDc);
    return outputBitmap;
}
}

CaptureOverlay::CaptureOverlay(HINSTANCE instance)
    : instance_(instance) {
}

bool CaptureOverlay::SelectArea(HWND owner, RECT* selectedRect) {
    if (!EnsureClassRegistered()) {
        return false;
    }

    virtualScreen_.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    virtualScreen_.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    virtualScreen_.right = virtualScreen_.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virtualScreen_.bottom = virtualScreen_.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (backgroundBitmap_ != nullptr) {
        DeleteObject(backgroundBitmap_);
        backgroundBitmap_ = nullptr;
    }
    if (dimmedBitmap_ != nullptr) {
        DeleteObject(dimmedBitmap_);
        dimmedBitmap_ = nullptr;
    }
    if (paintBufferBitmap_ != nullptr) {
        DeleteObject(paintBufferBitmap_);
        paintBufferBitmap_ = nullptr;
    }
    backgroundBitmap_ = CaptureDesktopSnapshot(virtualScreen_);
    if (backgroundBitmap_ != nullptr) {
        HDC screenDc = GetDC(nullptr);
        dimmedBitmap_ = CreateDimmedBitmap(
            screenDc,
            backgroundBitmap_,
            virtualScreen_.right - virtualScreen_.left,
            virtualScreen_.bottom - virtualScreen_.top,
            70);
        ReleaseDC(nullptr, screenDc);
    }
    selection_ = {};
    dragging_ = false;
    accepted_ = false;

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kOverlayClassName, L"", WS_POPUP,
        virtualScreen_.left, virtualScreen_.top,
        virtualScreen_.right - virtualScreen_.left,
        virtualScreen_.bottom - virtualScreen_.top,
        owner, nullptr, instance_, this);
    if (hwnd_ == nullptr) {
        if (backgroundBitmap_ != nullptr) {
            DeleteObject(backgroundBitmap_);
            backgroundBitmap_ = nullptr;
        }
        if (dimmedBitmap_ != nullptr) {
            DeleteObject(dimmedBitmap_);
            dimmedBitmap_ = nullptr;
        }
        if (paintBufferBitmap_ != nullptr) {
            DeleteObject(paintBufferBitmap_);
            paintBufferBitmap_ = nullptr;
        }
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd_, HWND_TOPMOST,
        virtualScreen_.left,
        virtualScreen_.top,
        virtualScreen_.right - virtualScreen_.left,
        virtualScreen_.bottom - virtualScreen_.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetCursor(LoadCursorW(nullptr, IDC_CROSS));

    MSG msg {};
    while (IsWindow(hwnd_)) {
        if (PeekMessageW(&msg, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE)) {
            accepted_ = false;
            break;
        }

        const BOOL status = GetMessageW(&msg, hwnd_, 0, 0);
        if (status <= 0) {
            accepted_ = false;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(hwnd_)) {
            break;
        }
    }

    ReleaseCapture();
    if (accepted_ && selectedRect != nullptr) {
        *selectedRect = NormalizeRect(selection_);
    }
    return accepted_;
}

LRESULT CALLBACK CaptureOverlay::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<CaptureOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<CaptureOverlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CaptureOverlay::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;
    case WM_MOUSEACTIVATE:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        startPoint_.x = GET_X_LPARAM(lParam) + virtualScreen_.left;
        startPoint_.y = GET_Y_LPARAM(lParam) + virtualScreen_.top;
        selection_ = { startPoint_.x, startPoint_.y, startPoint_.x, startPoint_.y };
        dragging_ = true;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        if (dragging_) {
            POINT current { GET_X_LPARAM(lParam) + virtualScreen_.left, GET_Y_LPARAM(lParam) + virtualScreen_.top };
            UpdateSelection(current);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging_) {
            POINT current { GET_X_LPARAM(lParam) + virtualScreen_.left, GET_Y_LPARAM(lParam) + virtualScreen_.top };
            UpdateSelection(current);
            dragging_ = false;
            ReleaseCapture();
            accepted_ = selection_.left != selection_.right && selection_.top != selection_.bottom;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        accepted_ = false;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            accepted_ = false;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_DESTROY:
        if (backgroundBitmap_ != nullptr) {
            DeleteObject(backgroundBitmap_);
            backgroundBitmap_ = nullptr;
        }
        if (dimmedBitmap_ != nullptr) {
            DeleteObject(dimmedBitmap_);
            dimmedBitmap_ = nullptr;
        }
        if (paintBufferBitmap_ != nullptr) {
            DeleteObject(paintBufferBitmap_);
            paintBufferBitmap_ = nullptr;
        }
        hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

RECT CaptureOverlay::NormalizeRect(const RECT& rect) const {
    RECT result = rect;
    if (result.left > result.right) {
        std::swap(result.left, result.right);
    }
    if (result.top > result.bottom) {
        std::swap(result.top, result.bottom);
    }
    return result;
}

void CaptureOverlay::UpdateSelection(POINT point) {
    selection_.left = startPoint_.x;
    selection_.top = startPoint_.y;
    selection_.right = point.x;
    selection_.bottom = point.y;
}

void CaptureOverlay::Paint(HWND hwnd) {
    PAINTSTRUCT ps {};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client {};
    GetClientRect(hwnd, &client);
    HDC bufferDc = CreateCompatibleDC(dc);
    if (bufferDc == nullptr) {
        EndPaint(hwnd, &ps);
        return;
    }

    const int width = client.right;
    const int height = client.bottom;
    if (paintBufferBitmap_ == nullptr) {
        paintBufferBitmap_ = CreateCompatibleBitmap(dc, width, height);
    }
    if (paintBufferBitmap_ == nullptr) {
        DeleteDC(bufferDc);
        EndPaint(hwnd, &ps);
        return;
    }

    HGDIOBJ oldBufferBitmap = SelectObject(bufferDc, paintBufferBitmap_);

    if (dimmedBitmap_ != nullptr) {
        HDC dimmedDc = CreateCompatibleDC(dc);
        if (dimmedDc != nullptr) {
            HGDIOBJ oldDimmed = SelectObject(dimmedDc, dimmedBitmap_);
            BitBlt(bufferDc, 0, 0, width, height, dimmedDc, 0, 0, SRCCOPY);
            SelectObject(dimmedDc, oldDimmed);
            DeleteDC(dimmedDc);
        }
    } else if (backgroundBitmap_ != nullptr) {
        HDC snapshotDc = CreateCompatibleDC(dc);
        if (snapshotDc != nullptr) {
            HGDIOBJ oldSnapshot = SelectObject(snapshotDc, backgroundBitmap_);
            BitBlt(bufferDc, 0, 0, width, height, snapshotDc, 0, 0, SRCCOPY);
            SelectObject(snapshotDc, oldSnapshot);
            DeleteDC(snapshotDc);
            ApplyDimOverlay(bufferDc, client, 70);
        }
    } else {
        FillRectAlpha(bufferDc, client, RGB(20, 20, 20));
    }

    const RECT selected = NormalizeRect(selection_);
    RECT local {
        selected.left - virtualScreen_.left,
        selected.top - virtualScreen_.top,
        selected.right - virtualScreen_.left,
        selected.bottom - virtualScreen_.top
    };

    if (local.left != local.right && local.top != local.bottom) {
        if (backgroundBitmap_ != nullptr) {
            HDC snapshotDc = CreateCompatibleDC(dc);
            if (snapshotDc != nullptr) {
                HGDIOBJ oldSnapshot = SelectObject(snapshotDc, backgroundBitmap_);
                BitBlt(bufferDc,
                    local.left,
                    local.top,
                    local.right - local.left,
                    local.bottom - local.top,
                    snapshotDc,
                    local.left,
                    local.top,
                    SRCCOPY);
                SelectObject(snapshotDc, oldSnapshot);
                DeleteDC(snapshotDc);
            }
        }

        HPEN pen = CreatePen(PS_SOLID, 2, DarkMode::GetAccentColor());
        if (pen != nullptr) {
            HGDIOBJ oldPen = SelectObject(bufferDc, pen);
            HGDIOBJ oldBrush = SelectObject(bufferDc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(bufferDc, local.left, local.top, local.right, local.bottom);
            SelectObject(bufferDc, oldBrush);
            SelectObject(bufferDc, oldPen);
            DeleteObject(pen);
        }
    }

    BitBlt(dc, 0, 0, width, height, bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, oldBufferBitmap);
    DeleteDC(bufferDc);

    EndPaint(hwnd, &ps);
}

bool CaptureOverlay::EnsureClassRegistered() {
    if (classRegistered_) {
        return true;
    }

    WNDCLASSW wc {};
    wc.lpfnWndProc = CaptureOverlay::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kOverlayClassName;
    classRegistered_ = RegisterClassW(&wc) != 0;
    return classRegistered_;
}
