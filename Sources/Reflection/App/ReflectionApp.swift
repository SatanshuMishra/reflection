import AppKit
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

@MainActor
private func focusWindowIfAvailable(windowID: String) -> Bool {
    guard let window = NSApp.windows.first(where: { $0.identifier?.rawValue == windowID }) else {
        return false
    }
    window.makeKeyAndOrderFront(nil)
    return true
}

@main
struct ReflectionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var sessionManager = MirrorSessionManager()
    @StateObject private var appSettings = AppSettings()

    init() {
        let shouldResetOnboarding = ProcessInfo.processInfo.arguments.contains("-resetOnboarding")

        if shouldResetOnboarding {
            UserDefaults.standard.set(false, forKey: Constants.onboardingCompletedKey)
        }
    }

    var body: some Scene {
        mainWindowScene
        onboardingWindowScene
    }

    @SceneBuilder
    private var mainWindowScene: some Scene {
        Window("Reflection", id: Constants.mainWindowID) {
            MainWindowRootView(
                appDelegate: appDelegate,
                sessionManager: sessionManager,
                appSettings: appSettings,
                onMirror: { device in openMirrorWindow(for: device) }
            )
        }
        .defaultSize(
            width: Constants.mainWindowSize.width,
            height: Constants.mainWindowSize.height
        )
    }

    @SceneBuilder
    private var onboardingWindowScene: some Scene {
        Window("Reflection Onboarding", id: Constants.onboardingWindowID) {
            OnboardingWindowRootView(
                appDelegate: appDelegate,
                sessionManager: sessionManager,
                appSettings: appSettings,
                onMirror: { device in openMirrorWindow(for: device) }
            )
        }
        .windowStyle(.hiddenTitleBar)
        .defaultSize(
            width: Constants.onboardingWelcomeSize.width,
            height: Constants.onboardingWelcomeSize.height
        )
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

// MARK: - Main Window Root

private struct MainWindowRootView: View {
    let appDelegate: AppDelegate
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var appSettings: AppSettings
    let onMirror: (DeviceModel) -> Void

    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss
    @AppStorage(Constants.onboardingCompletedKey) private var onboardingCompleted = false

    var body: some View {
        DeviceListView(
            sessionManager: sessionManager,
            discovery: sessionManager.discovery,
            appSettings: appSettings,
            onMirror: onMirror
        )
        .background(
            WindowAccessor { window in
                window.identifier = NSUserInterfaceItemIdentifier(Constants.mainWindowID)
            }
        )
        .task(id: onboardingCompleted) {
            guard onboardingCompleted else { return }
            await sessionManager.startDiscovery()
        }
        .onAppear {
            appDelegate.configure(
                appSettings: appSettings,
                sessionManager: sessionManager,
                onMirror: onMirror,
                openWindowAction: { [openWindow] windowID in
                    openWindow(id: windowID)
                }
            )

            guard !onboardingCompleted else { return }
            if !focusWindowIfAvailable(windowID: Constants.onboardingWindowID) {
                openWindow(id: Constants.onboardingWindowID)
            }
            dismiss()
        }
    }
}

// MARK: - Onboarding Window Root

private struct OnboardingWindowRootView: View {
    let appDelegate: AppDelegate
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var appSettings: AppSettings
    let onMirror: (DeviceModel) -> Void

    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss
    @AppStorage(Constants.onboardingCompletedKey) private var onboardingCompleted = false

    @ViewBuilder
    var body: some View {
        if #available(macOS 15.0, *) {
            baseView
                .toolbar(removing: .title)
                .toolbarBackgroundVisibility(.hidden, for: .windowToolbar)
                .toolbarVisibility(.hidden, for: .windowToolbar)
        } else {
            baseView
        }
    }

    private var baseView: some View {
        OnboardingView {
            withAnimation(.easeInOut(duration: 0.3)) {
                onboardingCompleted = true
            }
            DispatchQueue.main.async {
                if !focusWindowIfAvailable(windowID: Constants.mainWindowID) {
                    openWindow(id: Constants.mainWindowID)
                }
                dismiss()
            }
        }
        .background(OnboardingWindowStyle())
        .background(
            WindowAccessor { window in
                window.identifier = NSUserInterfaceItemIdentifier(Constants.onboardingWindowID)
            }
        )
        .onAppear {
            appDelegate.configure(
                appSettings: appSettings,
                sessionManager: sessionManager,
                onMirror: onMirror,
                openWindowAction: { [openWindow] windowID in
                    openWindow(id: windowID)
                }
            )

            guard onboardingCompleted else { return }
            if !focusWindowIfAvailable(windowID: Constants.mainWindowID) {
                openWindow(id: Constants.mainWindowID)
            }
            dismiss()
        }
    }
}
