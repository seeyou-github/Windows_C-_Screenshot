#pragma once

#include <string>
#include <windows.h>

std::wstring LoadStringResource(HINSTANCE instance, UINT id);
std::wstring GetModuleDirectory();
std::wstring JoinPath(const std::wstring& left, const std::wstring& right);
std::wstring Trim(const std::wstring& value);
std::wstring FormatHotkeyText(UINT modifiers, UINT vk);
std::wstring GetTimestampForFileName();
bool EnsureDirectoryExists(const std::wstring& path);
void CenterWindowOnScreen(HWND hwnd, int width, int height);
