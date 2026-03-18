import AppKit

/// Pure logic for activation policy decisions, extracted for testability.
/// No side effects — callers are responsible for applying the policy.
enum ActivationPolicyHelper {

    /// Whether the app should terminate when the last window closes.
    /// Returns `false` when "Run in Background" is enabled (app stays in menu bar).
    @MainActor
    static func shouldTerminateAfterLastWindowClosed(settings: AppSettings) -> Bool {
        !settings.runInBackground
    }

    /// Determines the desired activation policy based on current state.
    /// - `.regular`: dock icon visible, standard app behavior
    /// - `.accessory`: no dock icon, menu bar only
    static func desiredPolicy(
        runInBackground: Bool,
        hasVisibleWindows: Bool
    ) -> NSApplication.ActivationPolicy {
        if runInBackground && !hasVisibleWindows {
            return .accessory
        }
        return .regular
    }
}
