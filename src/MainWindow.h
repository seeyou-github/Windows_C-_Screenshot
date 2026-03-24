#pragma once

#include "CaptureOverlay.h"
#include "Config.h"
#include "Screenshot.h"

#include <windows.h>

class MainWindow {
public:
    MainWindow(HINSTANCE instance, ConfigStore* configStore);
    ~MainWindow();

    bool Create();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void ApplyFonts();
    void ApplyConfigToControls();
    AppConfig ReadConfigFromControls() const;
    void UpdateStatus(const std::wstring& text);
    void ShowTrayMenu();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowSettingsWindow();
    void HideToTray();
    bool RegisterCurrentHotkey();
    void UnregisterCurrentHotkey();
    void HandleSave();
    void BrowseForFolder();
    void StartCapture();
    void ApplyDarkControlTheme(HDC dc, HWND control);
    void ToggleCheckbox(HWND control, bool value);
    bool GetCheckbox(HWND control) const;
    void SetEditText(HWND control, const std::wstring& value);
    std::wstring GetEditText(HWND control) const;

    HINSTANCE instance_;
    HWND hwnd_ = nullptr;
    HWND hotkeyEdit_ = nullptr;
    HWND clipboardCheck_ = nullptr;
    HWND fileCheck_ = nullptr;
    HWND pathEdit_ = nullptr;
    HWND browseButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    WNDPROC hotkeyEditBaseProc_ = nullptr;
    UINT registeredModifiers_ = 0;
    UINT registeredVirtualKey_ = 0;
    bool trayIconAdded_ = false;
    NOTIFYICONDATAW trayIcon_ {};
    AppConfig config_;
    ConfigStore* configStore_ = nullptr;
    ScreenshotService screenshotService_;
    CaptureOverlay overlay_;
};
