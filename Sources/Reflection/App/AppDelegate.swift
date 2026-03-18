import AppKit
import AVFoundation
import Combine
import SwiftUI
import os

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private var windowControllers: [String: MirrorWindowController] = [:]
    private let statusBarController = StatusBarController()
    private var cancellables = Set<AnyCancellable>()

    private weak var appSettings: AppSettings?
    private var onMirror: ((DeviceModel) -> Void)?
    private var openMainWindow: (() -> Void)?

    /// Device IDs with open mirror windows, for window identification.
    var mirrorWindowDeviceIDs: [String] { Array(windowControllers.keys) }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleMirrorWindowClosed(_:)),
            name: .mirrorWindowClosed,
            object: nil
        )

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleDeviceDisconnected(_:)),
            name: .deviceDisconnected,
            object: nil
        )

        Task {
            await performUpdateChecks()
        }
    }

    /// Called from ReflectionApp to share state. Sets up the menu bar
    /// item and observes background-mode preference.
    func configure(
        appSettings: AppSettings,
        sessionManager: MirrorSessionManager,
        onMirror: @escaping (DeviceModel) -> Void,
        openMainWindow: @escaping () -> Void
    ) {
        // Guard against duplicate configuration
        guard self.appSettings == nil else { return }
        self.appSettings = appSettings
        self.onMirror = onMirror
        self.openMainWindow = openMainWindow

        let menuBarContent = MenuBarView(
            discovery: sessionManager.discovery,
            sessionManager: sessionManager,
            onMirror: { [weak self] device in self?.onMirror?(device) },
            onShowMainWindow: { [weak self] in self?.showMainWindow() }
        )
        statusBarController.configure(
            appSettings: appSettings,
            contentView: menuBarContent
        )

        // When window closes and runInBackground is on, hide from dock
        NotificationCenter.default.publisher(for: NSWindow.willCloseNotification)
            .sink { [weak self] notification in
                guard let self,
                      let window = notification.object as? NSWindow,
                      window.level == .normal,
                      self.appSettings?.runInBackground == true else { return }
                // Check if this was the last normal window
                let remainingNormal = NSApp.windows.filter {
                    $0 != window && $0.level == .normal && $0.isVisible
                }
                if remainingNormal.isEmpty {
                    NSApp.setActivationPolicy(.accessory)
                }
            }
            .store(in: &cancellables)
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag {
            showMainWindow()
        }
        return true
    }

    // MARK: - Window Management

    func showMainWindow() {
        NSApp.setActivationPolicy(.regular)
        if #available(macOS 14.0, *) {
            NSApp.activate()
        } else {
            NSApp.activate(ignoringOtherApps: true)
        }

        // Try to bring an existing normal window to front
        for window in NSApp.windows {
            if window.level == .normal && window.canBecomeKey {
                window.makeKeyAndOrderFront(nil)
                return
            }
        }

        // No window found — SwiftUI destroyed it on close.
        // Use the captured openWindow(id:) action from SwiftUI
        // environment to create a new WindowGroup window.
        openMainWindow?()
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

    // MARK: - Update Checker

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

    // MARK: - Notification Handlers

    @objc nonisolated private func handleMirrorWindowClosed(_ notification: Notification) {
        guard let deviceID = notification.userInfo?["deviceID"] as? String else { return }
        Task { @MainActor in
            self.windowControllers.removeValue(forKey: deviceID)
        }
    }

    @objc nonisolated private func handleDeviceDisconnected(_ notification: Notification) {
        guard let deviceID = notification.userInfo?["deviceID"] as? String else { return }
        Task { @MainActor in
            self.showDisconnectPrompt(deviceID: deviceID)
        }
    }

    private func showDisconnectPrompt(deviceID: String) {
        let alert = NSAlert()
        alert.messageText = "iPad Disconnected"
        alert.informativeText = "The iPad was disconnected. Reconnect via USB to resume mirroring."
        alert.alertStyle = .informational
        alert.addButton(withTitle: "OK")
        alert.icon = NSImage(systemSymbolName: "eject.fill", accessibilityDescription: "Disconnected")
        alert.runModal()

        closeMirrorWindow(deviceID: deviceID)
    }
}
