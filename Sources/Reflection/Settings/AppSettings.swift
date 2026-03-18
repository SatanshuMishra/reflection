import AppKit
import Foundation
import os

/// User-selectable appearance theme.
enum AppearanceMode: String, CaseIterable, Sendable {
    case system
    case light
    case dark

    /// Maps to AppKit's `NSAppearance` for immediate window-level updates.
    /// Returns `nil` for `.system` (inherits from system setting).
    var nsAppearance: NSAppearance? {
        switch self {
        case .system: nil
        case .light: NSAppearance(named: .aqua)
        case .dark: NSAppearance(named: .darkAqua)
        }
    }
}

/// Observable preferences model backed by UserDefaults.
/// Injectable dependencies enable full test isolation.
@MainActor
final class AppSettings: ObservableObject {

    private let defaults: UserDefaults
    private let loginItemService: LoginItemService
    private var isSyncingLoginItem = false

    @Published var appearance: AppearanceMode {
        didSet {
            defaults.set(appearance.rawValue, forKey: Constants.appearanceKey)
            applyAppearance()
        }
    }

    @Published var runInBackground: Bool {
        didSet { defaults.set(runInBackground, forKey: Constants.runInBackgroundKey) }
    }

    /// Transient flag: when set to `true`, DeviceListView navigates
    /// to the Settings page. Reset to `false` after consumption.
    /// Not persisted — purely an in-memory navigation trigger.
    @Published var navigateToSettings = false

    @Published var launchAtLogin: Bool {
        didSet {
            guard !isSyncingLoginItem else { return }
            syncLoginItem(enabled: launchAtLogin)
        }
    }

    init(
        defaults: UserDefaults = .standard,
        loginItemService: LoginItemService = SMLoginItemService()
    ) {
        self.defaults = defaults
        self.loginItemService = loginItemService

        // Read stored values (or use defaults)
        let storedAppearance = defaults.string(forKey: Constants.appearanceKey)
        self.appearance = storedAppearance.flatMap(AppearanceMode.init(rawValue:)) ?? .system

        self.runInBackground = defaults.bool(forKey: Constants.runInBackgroundKey)

        // Sync launch-at-login with the actual system state
        let storedLaunchAtLogin = defaults.bool(forKey: Constants.launchAtLoginKey)
        self.launchAtLogin = storedLaunchAtLogin

        // Apply stored appearance immediately on launch
        applyAppearance()
    }

    /// Sets `NSApp.appearance` at the AppKit level for immediate,
    /// synchronous theme propagation to all windows.
    /// Guard for nil NSApp (test environment).
    private func applyAppearance() {
        NSApp?.appearance = appearance.nsAppearance
    }

    private func syncLoginItem(enabled: Bool) {
        do {
            if enabled {
                try loginItemService.enable()
            } else {
                try loginItemService.disable()
            }
            defaults.set(enabled, forKey: Constants.launchAtLoginKey)
        } catch {
            Logger.settings.error("Failed to \(enabled ? "enable" : "disable") login item: \(error.localizedDescription)")
            // Revert via re-entrancy guard to avoid infinite didSet recursion
            isSyncingLoginItem = true
            launchAtLogin = !enabled
            isSyncingLoginItem = false
            defaults.set(!enabled, forKey: Constants.launchAtLoginKey)
        }
    }
}
