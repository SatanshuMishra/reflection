import AppKit
import Combine
import SwiftUI

/// Manages the NSStatusItem (menu bar icon) and its popover.
/// Uses AppKit directly instead of SwiftUI's MenuBarExtra for
/// reliable behavior across all macOS versions.
///
/// Inherits from NSObject for target-action pattern (NSStatusBarButton).
@MainActor
final class StatusBarController: NSObject {
    private var statusItem: NSStatusItem?
    private let popover = NSPopover()
    private var eventMonitor: Any?
    private var cancellables = Set<AnyCancellable>()

    private weak var appSettings: AppSettings?

    /// Installs or removes the status bar item based on `runInBackground`.
    func configure(
        appSettings: AppSettings,
        contentView: some View
    ) {
        self.appSettings = appSettings

        // Host the SwiftUI MenuBarView inside the popover
        popover.contentSize = NSSize(width: 280, height: 320)
        popover.behavior = .transient
        popover.animates = true
        popover.contentViewController = NSHostingController(rootView: contentView)

        // React to runInBackground changes
        appSettings.$runInBackground
            .removeDuplicates()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] enabled in
                guard let self else { return }
                if enabled {
                    self.installStatusItem()
                } else {
                    self.removeStatusItem()
                }
            }
            .store(in: &cancellables)
    }

    // MARK: - Status Item Lifecycle

    private func installStatusItem() {
        guard statusItem == nil else { return }
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = item.button {
            button.image = NSImage(
                systemSymbolName: "ipad.and.arrow.forward",
                accessibilityDescription: "Reflection"
            )
            button.action = #selector(togglePopover)
            button.target = self
        }
        statusItem = item
    }

    private func removeStatusItem() {
        if let item = statusItem {
            NSStatusBar.system.removeStatusItem(item)
        }
        statusItem = nil
        popover.performClose(nil)
        removeEventMonitor()
    }

    // MARK: - Popover

    @objc private func togglePopover() {
        if popover.isShown {
            closePopover()
        } else {
            openPopover()
        }
    }

    private func openPopover() {
        guard let button = statusItem?.button else { return }

        // Sync appearance — NSPopover windows don't inherit NSApp.appearance.
        popover.appearance = NSApp.effectiveAppearance

        popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)

        // Activate the app and make the popover's window key.
        // Without this, background-mode apps (.accessory policy) render
        // the popover in the "inactive" window style (lighter/washed out).
        // Clicking inside would activate it, but users see the flash.
        if #available(macOS 14.0, *) {
            NSApp.activate()
        } else {
            NSApp.activate(ignoringOtherApps: true)
        }
        popover.contentViewController?.view.window?.makeKeyAndOrderFront(nil)

        installEventMonitor()
    }

    private func closePopover() {
        popover.performClose(nil)
        removeEventMonitor()
    }

    // MARK: - Click-Outside Dismissal

    private func installEventMonitor() {
        eventMonitor = NSEvent.addGlobalMonitorForEvents(
            matching: [.leftMouseDown, .rightMouseDown]
        ) { [weak self] _ in
            self?.closePopover()
        }
    }

    private func removeEventMonitor() {
        if let monitor = eventMonitor {
            NSEvent.removeMonitor(monitor)
            eventMonitor = nil
        }
    }

    /// Tears down the event monitor, status item, and subscriptions.
    func tearDown() {
        removeEventMonitor()
        removeStatusItem()
        cancellables.removeAll()
    }
}
