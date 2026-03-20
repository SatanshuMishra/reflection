#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace reflection {

/// First-run onboarding wizard.
/// 3-page flow: Welcome → Firewall Setup → Ready
///
/// TODO (Milestone 6): Full implementation
class OnboardingWindow {
public:
    OnboardingWindow();
    ~OnboardingWindow();

    /// Show the onboarding wizard. Returns true if completed successfully.
    bool show(HINSTANCE instance);

    /// Check if onboarding has been completed (registry flag).
    static bool is_completed();

    /// Mark onboarding as completed.
    static void mark_completed();

private:
    HWND hwnd_ = nullptr;
};

} // namespace reflection
