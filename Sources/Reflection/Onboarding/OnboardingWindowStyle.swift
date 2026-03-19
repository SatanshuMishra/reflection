import AppKit
import SwiftUI

/// Applies onboarding-only window chrome to the hosting NSWindow.
///
/// This style is intentionally one-way: the onboarding scene has its own
/// window, so there is no runtime toggling between onboarding and main
/// window chrome in a single NSWindow.
///
/// SwiftUI scene-level hidden-titlebar style is the primary mechanism.
/// This representable is a compatibility hardening layer that reapplies
/// AppKit window chrome settings across key/main lifecycle transitions.
struct OnboardingWindowStyle: NSViewRepresentable {
    func makeNSView(context: Context) -> WindowObserverView {
        let view = WindowObserverView()
        let coordinator = context.coordinator
        view.onWindowAttached = { window in
            attachWindowAndApplyStyle(window, coordinator: coordinator)
        }
        return view
    }

    func updateNSView(_ nsView: WindowObserverView, context: Context) {
        if let window = context.coordinator.window ?? nsView.window {
            attachWindowAndApplyStyle(window, coordinator: context.coordinator)
        }
    }

    func makeCoordinator() -> Coordinator { Coordinator() }

    @MainActor
    final class Coordinator: NSObject {
        weak var window: NSWindow?
        private var onWindowLifecycleChange: (() -> Void)?

        deinit {
            NotificationCenter.default.removeObserver(self)
        }

        func observeWindowLifecycle(for window: NSWindow, onChange: @escaping () -> Void) {
            guard self.window !== window else {
                onWindowLifecycleChange = onChange
                return
            }
            clearWindowObservers()
            self.window = window
            onWindowLifecycleChange = onChange

            let names: [Notification.Name] = [
                NSWindow.didBecomeMainNotification,
                NSWindow.didResignMainNotification,
                NSWindow.didBecomeKeyNotification,
                NSWindow.didResignKeyNotification,
            ]

            for name in names {
                NotificationCenter.default.addObserver(
                    self,
                    selector: #selector(handleWindowLifecycleChange(_:)),
                    name: name,
                    object: window
                )
            }
        }

        @objc private func handleWindowLifecycleChange(_ notification: Notification) {
            onWindowLifecycleChange?()
        }

        private func clearWindowObservers() {
            NotificationCenter.default.removeObserver(self)
            onWindowLifecycleChange = nil
        }
    }

    /// Custom NSView that fires a callback when attached to a window.
    final class WindowObserverView: NSView {
        var onWindowAttached: ((NSWindow) -> Void)?

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            if let window {
                onWindowAttached?(window)
            }
        }
    }

    private func attachWindowAndApplyStyle(_ window: NSWindow, coordinator: Coordinator) {
        coordinator.observeWindowLifecycle(for: window) { [weak coordinator] in
            guard let trackedWindow = coordinator?.window else { return }
            applyOnboardingStyle(to: trackedWindow)
            DispatchQueue.main.async { [weak coordinator] in
                guard let latestWindow = coordinator?.window, latestWindow === trackedWindow else { return }
                applyOnboardingStyle(to: latestWindow)
            }
        }

        // Apply immediately and once more on the next run loop tick.
        // SwiftUI can update NSWindow chrome after first attachment.
        // Reapplying prevents titlebar regressions on first focus change.
        applyOnboardingStyle(to: window)
        DispatchQueue.main.async { [weak coordinator] in
            guard let trackedWindow = coordinator?.window, trackedWindow === window else { return }
            applyOnboardingStyle(to: trackedWindow)
        }
    }

    private func applyOnboardingStyle(to window: NSWindow) {
        // IMPORTANT: Do NOT remove .titled from styleMask.
        // On modern macOS builds, removing .titled from a SwiftUI-managed
        // window crashes in _layoutSubtreeWithOldSize: because AppKit's
        // internal hosting views expect titlebar structures to exist.
        window.titlebarAppearsTransparent = true
        window.titleVisibility = .hidden
        window.titlebarSeparatorStyle = .none
        window.styleMask.insert(.fullSizeContentView)
        window.styleMask.remove(.resizable)
        window.toolbar = nil
        window.isMovableByWindowBackground = true

        // Hide traffic light buttons.
        window.standardWindowButton(.closeButton)?.isHidden = true
        window.standardWindowButton(.miniaturizeButton)?.isHidden = true
        window.standardWindowButton(.zoomButton)?.isHidden = true

        // Match titlebar and body to avoid visible seams.
        window.isOpaque = true
        window.backgroundColor = Constants.appBackgroundNS
        window.hasShadow = true
    }
}
