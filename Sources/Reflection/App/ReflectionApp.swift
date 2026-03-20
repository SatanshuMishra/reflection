import AppKit
import AVFoundation
import Foundation
import SwiftUI

@main
struct ReflectionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var sessionManager = MirrorSessionManager()
    @StateObject private var appSettings = AppSettings()

    init() {
        if ProcessInfo.processInfo.arguments.contains("-resetOnboarding") {
            UserDefaults.standard.set(false, forKey: Constants.onboardingCompletedKey)
        }
    }

    var body: some Scene {
        Window("Reflection", id: Constants.mainWindowID) {
            RootView(
                appDelegate: appDelegate,
                sessionManager: sessionManager,
                appSettings: appSettings,
                onMirror: { device in openMirrorWindow(for: device) }
            )
        }
        .defaultSize(
            width: Constants.onboardingWelcomeSize.width,
            height: Constants.onboardingWelcomeSize.height
        )
    }

    private func openMirrorWindow(for device: DeviceModel) {
        guard let capture = sessionManager.activeSessions[device.id] else { return }
        guard let session = capture.captureSession else { return }
        appDelegate.openMirrorWindow(
            session: session,
            deviceID: device.id,
            deviceName: device.name,
            frameStatusStream: capture.frameStatusStream
        )
    }
}

// MARK: - Root View (single window, conditional content)

private struct RootView: View {
    let appDelegate: AppDelegate
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var appSettings: AppSettings
    let onMirror: (DeviceModel) -> Void

    @Environment(\.openWindow) private var openWindow
    @AppStorage(Constants.onboardingCompletedKey) private var onboardingCompleted = false
    @State private var windowRef: NSWindow?

    var body: some View {
        Group {
            if onboardingCompleted {
                mainContent
            } else {
                onboardingContent
            }
        }
        .background(
            WindowAccessor { window in
                windowRef = window
                applyWindowChrome(window, isOnboarding: !onboardingCompleted)
            }
        )
        .onChange(of: onboardingCompleted) { completed in
            guard let window = windowRef else { return }
            applyWindowChrome(window, isOnboarding: !completed)
            if completed {
                resizeWindow(window, to: Constants.mainWindowSize)
            }
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

            // Gate on camera permission
            let cameraAuthorized = AVCaptureDevice.authorizationStatus(for: .video) == .authorized
            if !cameraAuthorized {
                onboardingCompleted = false
            }
        }
    }

    // MARK: - Main Content

    private var mainContent: some View {
        DeviceListView(
            sessionManager: sessionManager,
            discovery: sessionManager.discovery,
            appSettings: appSettings,
            onMirror: onMirror
        )
        .background(Constants.appBackground)
        .task {
            await sessionManager.startDiscovery()
        }
    }

    // MARK: - Onboarding Content

    private var onboardingContent: some View {
        onboardingView
            .background(Constants.appBackground.ignoresSafeArea(.container, edges: .top))
            .clipShape(RoundedRectangle(cornerRadius: Constants.onboardingCornerRadius))
    }

    @ViewBuilder
    private var onboardingView: some View {
        if #available(macOS 15.0, *) {
            OnboardingView {
                completeOnboarding()
            }
            .toolbar(removing: .title)
            .toolbarBackgroundVisibility(.hidden, for: .windowToolbar)
            .toolbarVisibility(.hidden, for: .windowToolbar)
        } else {
            OnboardingView {
                completeOnboarding()
            }
        }
    }

    private func completeOnboarding() {
        withAnimation(.easeInOut(duration: 0.3)) {
            onboardingCompleted = true
        }
    }

    // MARK: - Window Chrome

    private func applyWindowChrome(_ window: NSWindow, isOnboarding: Bool) {
        window.identifier = NSUserInterfaceItemIdentifier(Constants.mainWindowID)
        window.backgroundColor = Constants.appBackgroundNS
        window.titlebarAppearsTransparent = true
        window.titlebarSeparatorStyle = .none
        window.hasShadow = true

        if isOnboarding {
            window.styleMask.insert(.fullSizeContentView)
            window.styleMask.remove(.resizable)
            window.isMovableByWindowBackground = true
            window.toolbar = nil
            window.standardWindowButton(.closeButton)?.isHidden = true
            window.standardWindowButton(.miniaturizeButton)?.isHidden = true
            window.standardWindowButton(.zoomButton)?.isHidden = true
        } else {
            window.styleMask.insert(.resizable)
            window.styleMask.remove(.fullSizeContentView)
            window.isMovableByWindowBackground = false
            window.standardWindowButton(.closeButton)?.isHidden = false
            window.standardWindowButton(.miniaturizeButton)?.isHidden = false
            window.standardWindowButton(.zoomButton)?.isHidden = false
        }
    }

    private func resizeWindow(_ window: NSWindow, to size: CGSize) {
        let current = window.frame
        let origin = NSPoint(
            x: current.midX - size.width / 2,
            y: current.midY - size.height / 2
        )
        let newFrame = NSRect(origin: origin, size: size)
        window.setFrame(newFrame, display: true, animate: true)
    }
}
