#include "MainWindow.h"

#include "DarkMode.h"
#include "ResourceIds.h"
#include "Utils.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>

namespace {
constexpr wchar_t kMainWindowClass[] = L"ScreenshotSettingsWindow";
constexpr UINT kHotkeyId = 1;
constexpr UINT kTrayMessage = WM_APP + 10;
constexpr int kWindowWidth = 700;
constexpr int kWindowHeight = 420;
constexpr int kMargin = 24;
constexpr int kControlHeight = 34;
constexpr int kLabelWidth = 170;

BOOL CALLBACK ApplyFontToChild(HWND hwnd, LPARAM lParam) {
    SendMessageW(hwnd, WM_SETFONT, lParam, TRUE);
    return TRUE;
}

UINT GetModifiersFromKeyboardState() {
    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        modifiers |= MOD_CONTROL;
    }
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        modifiers |= MOD_SHIFT;
    }
    if (GetKeyState(VK_MENU) & 0x8000) {
        modifiers |= MOD_ALT;
    }
    if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) {
        modifiers |= MOD_WIN;
    }
    return modifiers;
}

bool IsModifierKey(WPARAM key) {
    return key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU || key == VK_LWIN || key == VK_RWIN;
}
}

MainWindow::MainWindow(HINSTANCE instance, ConfigStore* configStore)
    : instance_(instance)
    , configStore_(configStore)
    , overlay_(instance) {
    config_ = configStore_->Load();
}

MainWindow::~MainWindow() {
    UnregisterCurrentHotkey();
    RemoveTrayIcon();
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
}

bool MainWindow::Create() {
    HICON largeIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR | LR_SHARED));
    HICON smallIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED));

    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = largeIcon;
    wc.hIconSm = smallIcon;
    wc.hbrBackground = DarkMode::GetBackgroundBrush();
    wc.lpszClassName = kMainWindowClass;

    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kMainWindowClass,
        LoadStringResource(instance_, IDS_WINDOW_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance_,
        this);
    if (hwnd_ == nullptr) {
        return false;
    }

    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    DarkMode::ApplyImmersiveDarkMode(hwnd_);
    CenterWindowOnScreen(hwnd_, kWindowWidth, kWindowHeight);
    CreateControls();
    ApplyFonts();
    ApplyConfigToControls();
    AddTrayIcon();
    RegisterCurrentHotkey();

    ShowWindow(hwnd_, SW_HIDE);
    UpdateWindow(hwnd_);
    UpdateStatus(LoadStringResource(instance_, IDS_STATUS_HIDDEN));
    return true;
}

int MainWindow::Run() {
    MSG msg {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::HotkeyEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        if (IsModifierKey(wParam)) {
            return 0;
        }
        // The edit box acts like a recorder: once focused, the next key combination becomes the hotkey.
        self->config_.hotkeyModifiers = GetModifiersFromKeyboardState();
        self->config_.hotkeyVirtualKey = static_cast<UINT>(wParam);
        SetWindowTextW(hwnd, FormatHotkeyText(self->config_.hotkeyModifiers, self->config_.hotkeyVirtualKey).c_str());
        return 0;
    }
    if (message == WM_CHAR || message == WM_SYSCHAR) {
        return 0;
    }
    return CallWindowProcW(self->hotkeyEditBaseProc_, hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BROWSE_PATH:
            BrowseForFolder();
            return 0;
        case ID_TRAY_CAPTURE:
            StartCapture();
            return 0;
        case ID_TRAY_SHOW_SETTINGS:
            ShowSettingsWindow();
            return 0;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd_);
            return 0;
        default:
            if (reinterpret_cast<HWND>(lParam) == saveButton_) {
                HandleSave();
                return 0;
            }
            return 0;
        }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
        ApplyDarkControlTheme(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
        return reinterpret_cast<LRESULT>(DarkMode::GetBackgroundBrush());
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            HideToTray();
        }
        return 0;
    case WM_CLOSE:
        HideToTray();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            StartCapture();
        }
        return 0;
    default:
        if (message == kTrayMessage) {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowTrayMenu();
            } else if (lParam == WM_LBUTTONUP) {
                StartCapture();
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void MainWindow::CreateControls() {
    const int fullWidth = kWindowWidth - (kMargin * 2) - 16;
    int top = kMargin;
    const int editWidth = 320;
    const int browseWidth = 100;

    CreateWindowW(L"STATIC", LoadStringResource(instance_, IDS_LABEL_HOTKEY).c_str(), WS_CHILD | WS_VISIBLE, kMargin, top, kLabelWidth, kControlHeight, hwnd_, nullptr, instance_, nullptr);
    hotkeyEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, kMargin + kLabelWidth, top, editWidth, kControlHeight, hwnd_, nullptr, instance_, nullptr);
    top += kControlHeight + 8;

    CreateWindowW(L"STATIC", LoadStringResource(instance_, IDS_LABEL_HOTKEY_HINT).c_str(), WS_CHILD | WS_VISIBLE, kMargin + kLabelWidth, top, fullWidth - kLabelWidth, 44, hwnd_, nullptr, instance_, nullptr);
    top += 54;

    clipboardCheck_ = CreateWindowW(L"BUTTON", LoadStringResource(instance_, IDS_LABEL_CLIPBOARD).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, kMargin, top, fullWidth, 36, hwnd_, nullptr, instance_, nullptr);
    top += 44;

    fileCheck_ = CreateWindowW(L"BUTTON", LoadStringResource(instance_, IDS_LABEL_SAVE_FILE).c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, kMargin, top, fullWidth, 36, hwnd_, nullptr, instance_, nullptr);
    top += 52;

    CreateWindowW(L"STATIC", LoadStringResource(instance_, IDS_LABEL_SAVE_PATH).c_str(), WS_CHILD | WS_VISIBLE, kMargin, top, kLabelWidth, kControlHeight, hwnd_, nullptr, instance_, nullptr);
    pathEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, kMargin + kLabelWidth, top, editWidth, kControlHeight, hwnd_, nullptr, instance_, nullptr);
    browseButton_ = CreateWindowW(L"BUTTON", LoadStringResource(instance_, IDS_BUTTON_BROWSE).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, kMargin + kLabelWidth + editWidth + 12, top, browseWidth, kControlHeight, hwnd_, reinterpret_cast<HMENU>(ID_BROWSE_PATH), instance_, nullptr);
    top += 60;

    saveButton_ = CreateWindowW(L"BUTTON", LoadStringResource(instance_, IDS_BUTTON_SAVE).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, kWindowWidth - kMargin - 140 - 16, top, 140, 38, hwnd_, nullptr, instance_, nullptr);
    top += 58;

    statusLabel_ = CreateWindowW(L"STATIC", LoadStringResource(instance_, IDS_STATUS_READY).c_str(), WS_CHILD | WS_VISIBLE, kMargin, top, fullWidth, 56, hwnd_, nullptr, instance_, nullptr);

    SetWindowLongPtrW(hotkeyEdit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    hotkeyEditBaseProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hotkeyEdit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindow::HotkeyEditProc)));
}

void MainWindow::ApplyFonts() {
    NONCLIENTMETRICSW metrics {};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    metrics.lfMessageFont.lfHeight = -20;
    metrics.lfMessageFont.lfWeight = FW_NORMAL;
    font_ = CreateFontIndirectW(&metrics.lfMessageFont);
    EnumChildWindows(hwnd_, ApplyFontToChild, reinterpret_cast<LPARAM>(font_));
}

void MainWindow::ApplyConfigToControls() {
    SetWindowTextW(hotkeyEdit_, FormatHotkeyText(config_.hotkeyModifiers, config_.hotkeyVirtualKey).c_str());
    ToggleCheckbox(clipboardCheck_, config_.copyToClipboard);
    ToggleCheckbox(fileCheck_, config_.saveToFile);
    SetEditText(pathEdit_, config_.saveDirectory);
}

AppConfig MainWindow::ReadConfigFromControls() const {
    AppConfig result = config_;
    result.copyToClipboard = GetCheckbox(clipboardCheck_);
    result.saveToFile = GetCheckbox(fileCheck_);
    result.saveDirectory = Trim(GetEditText(pathEdit_));
    return result;
}

void MainWindow::UpdateStatus(const std::wstring& text) {
    SetWindowTextW(statusLabel_, text.c_str());
}

void MainWindow::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_CAPTURE, LoadStringResource(instance_, IDS_TRAY_CAPTURE).c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW_SETTINGS, LoadStringResource(instance_, IDS_TRAY_SETTINGS).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, LoadStringResource(instance_, IDS_TRAY_EXIT).c_str());

    POINT pt {};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::AddTrayIcon() {
    HICON trayIconHandle = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED));

    trayIcon_ = {};
    trayIcon_.cbSize = sizeof(trayIcon_);
    trayIcon_.hWnd = hwnd_;
    trayIcon_.uID = 1;
    trayIcon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayIcon_.uCallbackMessage = kTrayMessage;
    trayIcon_.hIcon = trayIconHandle;
    wcsncpy_s(trayIcon_.szTip, LoadStringResource(instance_, IDS_TRAY_TIP).c_str(), _TRUNCATE);
    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &trayIcon_) == TRUE;
}

void MainWindow::RemoveTrayIcon() {
    if (trayIconAdded_) {
        Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
        trayIconAdded_ = false;
    }
}

void MainWindow::ShowSettingsWindow() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
    UpdateStatus(LoadStringResource(instance_, IDS_STATUS_READY));
}

void MainWindow::HideToTray() {
    ShowWindow(hwnd_, SW_HIDE);
    UpdateStatus(LoadStringResource(instance_, IDS_STATUS_HIDDEN));
}

bool MainWindow::RegisterCurrentHotkey() {
    UnregisterCurrentHotkey();
    if (!RegisterHotKey(hwnd_, kHotkeyId, config_.hotkeyModifiers, config_.hotkeyVirtualKey)) {
        UpdateStatus(LoadStringResource(instance_, IDS_MSG_HOTKEY_REGISTER_FAILED));
        return false;
    }
    registeredModifiers_ = config_.hotkeyModifiers;
    registeredVirtualKey_ = config_.hotkeyVirtualKey;
    return true;
}

void MainWindow::UnregisterCurrentHotkey() {
    if (registeredVirtualKey_ != 0) {
        UnregisterHotKey(hwnd_, kHotkeyId);
        registeredVirtualKey_ = 0;
        registeredModifiers_ = 0;
    }
}

void MainWindow::HandleSave() {
    AppConfig updated = ReadConfigFromControls();
    if (!updated.copyToClipboard && !updated.saveToFile) {
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_NOTHING_ENABLED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    if (updated.saveToFile && updated.saveDirectory.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_PATH_REQUIRED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    const AppConfig original = config_;
    config_ = updated;
    if (!RegisterCurrentHotkey()) {
        config_ = original;
        RegisterCurrentHotkey();
        ApplyConfigToControls();
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_HOTKEY_CONFLICT).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    bool changed = false;
    if (!configStore_->SaveIfChanged(config_, &changed)) {
        config_ = original;
        RegisterCurrentHotkey();
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_SAVE_FAILED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    UpdateStatus(LoadStringResource(instance_, changed ? IDS_MSG_SAVE_SUCCESS : IDS_MSG_SAVE_UNCHANGED));
}

void MainWindow::BrowseForFolder() {
    BROWSEINFOW info {};
    info.hwndOwner = hwnd_;
    const std::wstring title = LoadStringResource(instance_, IDS_MSG_FOLDER_DIALOG);
    info.lpszTitle = title.c_str();
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    PIDLIST_ABSOLUTE list = SHBrowseForFolderW(&info);
    if (list == nullptr) {
        return;
    }

    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(list, path)) {
        SetEditText(pathEdit_, path);
    }
    CoTaskMemFree(list);
}

void MainWindow::StartCapture() {
    if (!config_.copyToClipboard && !config_.saveToFile) {
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_NOTHING_ENABLED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    UpdateStatus(LoadStringResource(instance_, IDS_STATUS_CAPTURE_HINT));
    ShowWindow(hwnd_, SW_HIDE);
    // A short delay prevents this window from being captured in the desktop snapshot.
    Sleep(120);

    RECT area {};
    if (!overlay_.SelectArea(hwnd_, &area)) {
        return;
    }

    ScreenshotResult result = screenshotService_.CaptureArea(area);
    if (result.bitmap == nullptr) {
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_CAPTURE_FAILED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    if (config_.copyToClipboard && !screenshotService_.CopyBitmapToClipboard(hwnd_, result.bitmap)) {
        MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_CLIPBOARD_FAILED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
    }

    if (config_.saveToFile) {
        if (!EnsureDirectoryExists(config_.saveDirectory)) {
            MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_SAVE_DIR_FAILED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
        } else {
            const std::wstring fileName = LoadStringResource(instance_, IDS_DEFAULT_FILENAME_PREFIX) + L"_" + GetTimestampForFileName() + L".jpg";
            const std::wstring filePath = JoinPath(config_.saveDirectory, fileName);
            if (!screenshotService_.SaveBitmapToJpeg(result.bitmap, filePath)) {
                MessageBoxW(hwnd_, LoadStringResource(instance_, IDS_MSG_SAVE_FAILED).c_str(), LoadStringResource(instance_, IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
            }
        }
    }

    DeleteObject(result.bitmap);
}

void MainWindow::ApplyDarkControlTheme(HDC dc, HWND control) {
    wchar_t className[32] = {};
    GetClassNameW(control, className, static_cast<int>(std::size(className)));
    SetTextColor(dc, DarkMode::GetTextColor());
    SetBkMode(dc, OPAQUE);
    if (wcscmp(className, L"Edit") == 0 || wcscmp(className, L"Button") == 0) {
        SetBkColor(dc, DarkMode::GetControlColor());
    } else {
        SetBkColor(dc, DarkMode::GetBackgroundColor());
    }
}

void MainWindow::ToggleCheckbox(HWND control, bool value) {
    SendMessageW(control, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool MainWindow::GetCheckbox(HWND control) const {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void MainWindow::SetEditText(HWND control, const std::wstring& value) {
    SetWindowTextW(control, value.c_str());
}

std::wstring MainWindow::GetEditText(HWND control) const {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}
