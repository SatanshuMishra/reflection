import AppKit
import AVFoundation

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private var windowControllers: [String: MirrorWindowController] = [:]

    func applicationDidFinishLaunching(_ notification: Notification) {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleMirrorWindowClosed(_:)),
            name: .mirrorWindowClosed,
            object: nil
        )

        Task {
            await performUpdateChecks()
        }
    }

    private func performUpdateChecks() async {
        let checker = UpdateChecker()

        // Show What's New if this is the first launch of a new version
        if checker.isFirstLaunchOfNewVersion {
            if let notes = await checker.fetchReleaseNotes(for: checker.currentVersion) {
                showWhatsNewAlert(version: checker.currentVersion, notes: notes)
            }
            UpdateChecker.lastSeenVersion = checker.currentVersion
        }

        // Check if a newer version is available
        if let update = await checker.checkForUpdate() {
            showUpdateAlert(info: update)
        }
    }

    func openMirrorWindow(session: AVCaptureSession, deviceID: String, deviceName: String, frameStatusStream: AsyncStream<Bool>) {
        guard windowControllers[deviceID] == nil else { return }

        let controller = MirrorWindowController(
            session: session,
            deviceID: deviceID,
            deviceName: deviceName,
            frameStatusStream: frameStatusStream
        )
        windowControllers[deviceID] = controller
        controller.showWindow(nil)
        controller.window?.makeKeyAndOrderFront(nil)
    }

    func closeMirrorWindow(deviceID: String) {
        windowControllers[deviceID]?.close()
        windowControllers.removeValue(forKey: deviceID)
    }

    @objc nonisolated private func handleMirrorWindowClosed(_ notification: Notification) {
        guard let deviceID = notification.userInfo?["deviceID"] as? String else { return }
        Task { @MainActor in
            self.windowControllers.removeValue(forKey: deviceID)
        }
    }
}
