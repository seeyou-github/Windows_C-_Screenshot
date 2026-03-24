#include "Config.h"

#include "Utils.h"

#include <codecvt>
#include <fstream>
#include <locale>
#include <sstream>

ConfigStore::ConfigStore(const std::wstring& filePath)
    : filePath_(filePath) {
}

AppConfig ConfigStore::Load() const {
    AppConfig config;
    config.saveDirectory = JoinPath(GetModuleDirectory(), L"screenshots");

    std::wifstream file(filePath_.c_str());
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
    if (!file.is_open()) {
        return config;
    }

    std::wstringstream buffer;
    buffer << file.rdbuf();
    return Parse(buffer.str());
}

bool ConfigStore::SaveIfChanged(const AppConfig& config, bool* changed) const {
    const std::wstring next = Serialize(config);
    std::wstring current;

    // Read the current file first so we only rewrite when values actually differ.
    std::wifstream input(filePath_.c_str());
    input.imbue(std::locale(input.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
    if (input.is_open()) {
        std::wstringstream buffer;
        buffer << input.rdbuf();
        current = buffer.str();
    }

    if (current == next) {
        if (changed != nullptr) {
            *changed = false;
        }
        return true;
    }

    // The file stays UTF-8 so the config remains readable beside the executable.
    std::wofstream output(filePath_.c_str(), std::ios::trunc);
    output.imbue(std::locale(output.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
    if (!output.is_open()) {
        if (changed != nullptr) {
            *changed = false;
        }
        return false;
    }

    output << next;
    if (changed != nullptr) {
        *changed = true;
    }
    return true;
}

std::wstring ConfigStore::Serialize(const AppConfig& config) const {
    std::wstringstream stream;
    stream << L"hotkey_modifiers=" << config.hotkeyModifiers << L"\n";
    stream << L"hotkey_vk=" << config.hotkeyVirtualKey << L"\n";
    stream << L"copy_to_clipboard=" << (config.copyToClipboard ? 1 : 0) << L"\n";
    stream << L"save_to_file=" << (config.saveToFile ? 1 : 0) << L"\n";
    stream << L"save_directory=" << config.saveDirectory << L"\n";
    return stream.str();
}

AppConfig ConfigStore::Parse(const std::wstring& text) const {
    AppConfig config;
    config.saveDirectory = JoinPath(GetModuleDirectory(), L"screenshots");

    std::wstringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        const auto index = line.find(L'=');
        if (index == std::wstring::npos) {
            continue;
        }

        const std::wstring key = Trim(line.substr(0, index));
        const std::wstring value = Trim(line.substr(index + 1));
        if (key == L"hotkey_modifiers") {
            config.hotkeyModifiers = static_cast<UINT>(_wtoi(value.c_str()));
        } else if (key == L"hotkey_vk") {
            config.hotkeyVirtualKey = static_cast<UINT>(_wtoi(value.c_str()));
        } else if (key == L"copy_to_clipboard") {
            config.copyToClipboard = value == L"1";
        } else if (key == L"save_to_file") {
            config.saveToFile = value == L"1";
        } else if (key == L"save_directory") {
            config.saveDirectory = value;
        }
    }

    if (config.hotkeyVirtualKey == 0) {
        config.hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
        config.hotkeyVirtualKey = 'A';
    }
    if (config.saveDirectory.empty()) {
        config.saveDirectory = JoinPath(GetModuleDirectory(), L"screenshots");
    }
    return config;
}
