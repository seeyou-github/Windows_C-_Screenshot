#include "Utils.h"

#include <algorithm>
#include <cwctype>
#include <shlobj.h>
#include <shlwapi.h>

std::wstring LoadStringResource(HINSTANCE instance, UINT id) {
    wchar_t buffer[512] = {};
    const int length = LoadStringW(instance, id, buffer, static_cast<int>(std::size(buffer)));
    return length > 0 ? std::wstring(buffer, length) : L"";
}

std::wstring GetModuleDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == L'\\') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring Trim(const std::wstring& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) { return std::iswspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) { return std::iswspace(ch) != 0; }).base();
    return begin < end ? std::wstring(begin, end) : L"";
}

std::wstring FormatHotkeyText(UINT modifiers, UINT vk) {
    std::wstring result;
    if (modifiers & MOD_CONTROL) {
        result += L"Ctrl + ";
    }
    if (modifiers & MOD_SHIFT) {
        result += L"Shift + ";
    }
    if (modifiers & MOD_ALT) {
        result += L"Alt + ";
    }
    if (modifiers & MOD_WIN) {
        result += L"Win + ";
    }

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
    wchar_t keyName[128] = {};
    if (GetKeyNameTextW(static_cast<LONG>(scanCode), keyName, static_cast<int>(std::size(keyName))) > 0) {
        result += keyName;
    } else {
        result += L"?";
    }
    return result;
}

std::wstring GetTimestampForFileName() {
    SYSTEMTIME time {};
    GetLocalTime(&time);
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u",
        time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond);
    return std::wstring(buffer);
}

bool EnsureDirectoryExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    if (PathFileExistsW(path.c_str())) {
        return true;
    }
    const int result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return result == ERROR_SUCCESS || result == ERROR_FILE_EXISTS || result == ERROR_ALREADY_EXISTS;
}

void CenterWindowOnScreen(HWND hwnd, int width, int height) {
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}
