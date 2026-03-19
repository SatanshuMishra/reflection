import AppKit
import SwiftUI
import AVFoundation

extension Notification.Name {
    static let mirrorWindowClosed = Notification.Name("com.reflection.mirrorWindowClosed")
    static let mirrorSessionStopped = Notification.Name("com.reflection.mirrorSessionStopped")
    static let deviceDisconnected = Notification.Name("com.reflection.deviceDisconnected")
}

final class MirrorWindowController: NSWindowController, NSWindowDelegate {
    private let deviceID: String
    private let windowState: MirrorWindowState

    init(session: AVCaptureSession, deviceID: String, deviceName: String, frameStatusStream: AsyncStream<Bool>) {
        self.deviceID = deviceID
        let state = MirrorWindowState()
        self.windowState = state

        let contentView = NSHostingView(
            rootView: MirrorWindowView(session: session, windowState: state)
        )

        let window = NSWindow(
            contentRect: NSRect(
                x: 0, y: 0,
                width: Constants.defaultWindowWidth,
                height: Constants.defaultWindowHeight
            ),
            styleMask: [.titled, .closable, .resizable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.contentView = contentView
        window.title = deviceName
        window.contentAspectRatio = Constants.defaultAspectRatio
        window.minSize = NSSize(
            width: Constants.windowMinWidth,
            height: Constants.windowMinHeight
        )
        window.center()
        window.setFrameAutosaveName("MirrorWindow-\(deviceID)")
        window.backgroundColor = Constants.appBackgroundNS

        super.init(window: window)
        window.delegate = self
        Task { @MainActor in
            state.observeFrameStatus(frameStatusStream)
        }
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) not implemented")
    }

    func windowWillClose(_ notification: Notification) {
        NotificationCenter.default.post(
            name: .mirrorWindowClosed,
            object: nil,
            userInfo: ["deviceID": deviceID]
        )
    }
}
