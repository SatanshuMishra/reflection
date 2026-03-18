import Foundation
import SwiftUI

/// Identifier for the main device-list window.
/// Used with `openWindow(id:)` to recreate it after background-mode close.
private let mainWindowID = "main"

private func debugLog(_ message: String) {
    let logFile = "/tmp/reflection_debug.log"
    let timestamp = ISO8601DateFormatter().string(from: Date())
    let line = "[\(timestamp)] \(message)\n"
    if let data = line.data(using: .utf8) {
        if FileManager.default.fileExists(atPath: logFile) {
            if let handle = FileHandle(forWritingAtPath: logFile) {
                handle.seekToEndOfFile()
                handle.write(data)
                handle.closeFile()
            }
        } else {
            FileManager.default.createFile(atPath: logFile, contents: data)
        }
    }
}

@main
struct ReflectionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var sessionManager = MirrorSessionManager()
    @StateObject private var appSettings = AppSettings()

    var body: some Scene {
        WindowGroup(id: mainWindowID) {
            MainContentView(
                appDelegate: appDelegate,
                sessionManager: sessionManager,
                appSettings: appSettings,
                onMirror: { device in openMirrorWindow(for: device) }
            )
        }
        .defaultSize(width: 400, height: 300)
    }

    private func openMirrorWindow(for device: DeviceModel) {
        debugLog("[App] openMirrorWindow for \(device.name) (id=\(device.id))")
        guard let capture = sessionManager.activeSessions[device.id] else {
            debugLog("[App] No active session for device \(device.id)")
            return
        }
        guard let session = capture.captureSession else {
            debugLog("[App] captureSession is nil for device \(device.id), state=\(capture.state)")
            return
        }
        debugLog("[App] Opening mirror window, session.isRunning=\(session.isRunning)")
        appDelegate.openMirrorWindow(
            session: session,
            deviceID: device.id,
            deviceName: device.name,
            frameStatusStream: capture.frameStatusStream
        )
    }
}

// MARK: - Main Content View

/// Wraps DeviceListView to capture `@Environment(\.openWindow)`,
/// which is only available inside a View (not in an App struct).
/// Passes the captured action to AppDelegate so it can recreate
/// the window from AppKit code (menu bar "Show Reflection").
private struct MainContentView: View {
    let appDelegate: AppDelegate
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var appSettings: AppSettings
    let onMirror: (DeviceModel) -> Void

    @Environment(\.openWindow) private var openWindow

    var body: some View {
        DeviceListView(
            sessionManager: sessionManager,
            discovery: sessionManager.discovery,
            appSettings: appSettings,
            onMirror: onMirror
        )
        .task {
            await sessionManager.startDiscovery()
        }
        .onAppear {
            // Capture the SwiftUI openWindow action for AppDelegate.
            // This is the only reliable way to create a new WindowGroup
            // window from AppKit code (e.g., NSStatusItem popover).
            appDelegate.configure(
                appSettings: appSettings,
                sessionManager: sessionManager,
                onMirror: onMirror,
                openMainWindow: { [openWindow] in
                    openWindow(id: mainWindowID)
                }
            )
        }
    }
}
