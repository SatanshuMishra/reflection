#include "views/OnboardingWindow.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

namespace reflection {

OnboardingWindow::OnboardingWindow() = default;
OnboardingWindow::~OnboardingWindow() = default;

bool OnboardingWindow::show(HINSTANCE /*instance*/) {
    Logger::info("Showing onboarding wizard");
    // TODO (Milestone 6): Implement 3-page wizard
    // Page 1: Welcome
    // Page 2: Firewall configuration
    // Page 3: Ready / instructions
    return true;
}

bool OnboardingWindow::is_completed() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    const bool completed = (RegQueryValueEx(
        key, constants::kRegKeyOnboardingCompleted.data(),
        nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size
    ) == ERROR_SUCCESS) && value != 0;

    RegCloseKey(key);
    return completed;
}

void OnboardingWindow::mark_completed() {
    HKEY key;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                       0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) == ERROR_SUCCESS) {
        DWORD value = 1;
        RegSetValueEx(key, constants::kRegKeyOnboardingCompleted.data(),
                      0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                      sizeof(value));
        RegCloseKey(key);
    }
}

} // namespace reflection
