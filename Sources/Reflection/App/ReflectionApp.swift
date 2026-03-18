import Foundation
import SwiftUI

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
        WindowGroup {
            DeviceListView(
                sessionManager: sessionManager,
                discovery: sessionManager.discovery,
                appSettings: appSettings,
                onMirror: { device in
                    openMirrorWindow(for: device)
                }
            )
            .task {
                await sessionManager.startDiscovery()
            }
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
