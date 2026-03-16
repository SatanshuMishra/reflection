import AppKit

/// Shows a native macOS alert informing the user that a newer version is available.
/// "Download" opens the GitHub release page in Safari. "Later" dismisses.
@MainActor
func showUpdateAlert(info: UpdateInfo) {
    let alert = NSAlert()
    alert.alertStyle = .informational
    alert.messageText = "Update Available"
    alert.informativeText = "Reflection v\(info.latestVersion) is available. You're currently on v\(info.currentVersion)."
    alert.addButton(withTitle: "Download")
    alert.addButton(withTitle: "Later")

    let response = alert.runModal()
    if response == .alertFirstButtonReturn {
        NSWorkspace.shared.open(info.releaseURL)
    }
}

/// Shows a native macOS alert with release notes for the current version.
/// Displayed on first launch after an update.
@MainActor
func showWhatsNewAlert(version: String, notes: String) {
    let alert = NSAlert()
    alert.alertStyle = .informational
    alert.messageText = "What's New in Reflection v\(version)"
    alert.informativeText = notes
    alert.addButton(withTitle: "Got It")

    alert.runModal()
}
