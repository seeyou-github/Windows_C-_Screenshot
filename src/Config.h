#pragma once

#include <string>
#include <windows.h>

struct AppConfig {
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVirtualKey = 'A';
    bool copyToClipboard = true;
    bool saveToFile = true;
    std::wstring saveDirectory;
};

class ConfigStore {
public:
    explicit ConfigStore(const std::wstring& filePath);

    AppConfig Load() const;
    bool SaveIfChanged(const AppConfig& config, bool* changed) const;

private:
    std::wstring Serialize(const AppConfig& config) const;
    AppConfig Parse(const std::wstring& text) const;

    std::wstring filePath_;
};
