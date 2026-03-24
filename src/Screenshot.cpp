#include "Screenshot.h"

#include <gdiplus.h>
#include <memory>
#include <vector>

namespace {
int GetEncoderClsid(const wchar_t* mimeType, CLSID* clsid) {
    UINT count = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (size == 0) {
        return -1;
    }

    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok) {
        return -1;
    }

    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *clsid = codecs[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}
}

ScreenshotResult ScreenshotService::CaptureArea(const RECT& rect) const {
    ScreenshotResult result {};
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return result;
    }

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
        return result;
    }

    HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
    // CAPTUREBLT helps include many layered and topmost windows in the final bitmap.
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, oldObject);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied) {
        DeleteObject(bitmap);
        return result;
    }

    result.bitmap = bitmap;
    result.captureRect = rect;
    return result;
}

bool ScreenshotService::CopyBitmapToClipboard(HWND owner, HBITMAP bitmap) const {
    if (bitmap == nullptr) {
        return false;
    }

    BITMAP bm {};
    if (GetObjectW(bitmap, sizeof(bm), &bm) == 0) {
        return false;
    }

    BITMAPINFOHEADER info {};
    info.biSize = sizeof(info);
    info.biWidth = bm.bmWidth;
    info.biHeight = bm.bmHeight;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;

    const DWORD imageSize = static_cast<DWORD>(bm.bmWidth * bm.bmHeight * 4);
    std::vector<BYTE> pixels(imageSize);

    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, bitmap, 0, static_cast<UINT>(bm.bmHeight), pixels.data(), reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines == 0) {
        return false;
    }

    HGLOBAL globalHandle = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + imageSize);
    if (globalHandle == nullptr) {
        return false;
    }

    void* globalData = GlobalLock(globalHandle);
    if (globalData == nullptr) {
        GlobalFree(globalHandle);
        return false;
    }

    memcpy(globalData, &info, sizeof(info));
    memcpy(static_cast<BYTE*>(globalData) + sizeof(info), pixels.data(), imageSize);
    GlobalUnlock(globalHandle);

    bool clipboardOpened = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (OpenClipboard(owner)) {
            clipboardOpened = true;
            break;
        }
        if (OpenClipboard(nullptr)) {
            clipboardOpened = true;
            break;
        }
        Sleep(20);
    }

    if (!clipboardOpened) {
        GlobalFree(globalHandle);
        return false;
    }

    EmptyClipboard();
    const bool ok = SetClipboardData(CF_DIB, globalHandle) != nullptr;
    CloseClipboard();
    if (!ok) {
        GlobalFree(globalHandle);
    }
    return ok;
}

bool ScreenshotService::SaveBitmapToJpeg(HBITMAP bitmap, const std::wstring& filePath) const {
    if (bitmap == nullptr) {
        return false;
    }

    CLSID encoderClsid {};
    if (GetEncoderClsid(L"image/jpeg", &encoderClsid) < 0) {
        return false;
    }

    Gdiplus::Bitmap image(bitmap, nullptr);
    Gdiplus::EncoderParameters parameters {};
    parameters.Count = 1;
    parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
    parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    parameters.Parameter[0].NumberOfValues = 1;
    ULONG quality = 92;
    parameters.Parameter[0].Value = &quality;

    if (image.GetLastStatus() != Gdiplus::Ok) {
        return false;
    }
    return image.Save(filePath.c_str(), &encoderClsid, &parameters) == Gdiplus::Ok;
}
