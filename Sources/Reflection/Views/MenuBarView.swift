import SwiftUI

/// Content for the NSPopover menu bar dropdown.
///
/// Layout:
///   ┌──────────────────────────┐
///   │  Reflection    ⟳  ⚙     │  ← header with icon buttons
///   ├──────────────────────────┤
///   │  (device list or empty)  │  ← content area
///   ├──────────────────────────┤
///   │  ▫ Show Reflection       │
///   │  ▫ Quit Reflection       │  ← footer actions
///   └──────────────────────────┘
struct MenuBarView: View {
    @ObservedObject var discovery: DeviceDiscovery
    @ObservedObject var sessionManager: MirrorSessionManager
    let onMirror: (DeviceModel) -> Void
    let onShowMainWindow: () -> Void
    let onShowSettings: () -> Void

    @State private var refreshRotation: Double = 0

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header
            Divider()
            content
            Divider()
            footer
        }
        .frame(width: 280)
    }

    // MARK: - Header

    private var header: some View {
        HStack {
            Text("Reflection")
                .font(.headline)
                .foregroundStyle(.primary)

            Spacer()

            // Refresh button with rotation animation
            Button {
                guard !discovery.isRefreshing else { return }
                withAnimation(.linear(duration: 0.6)) {
                    refreshRotation += 360
                }
                discovery.refreshDevices()
            } label: {
                Image(systemName: "arrow.clockwise")
                    .font(.body)
                    .foregroundStyle(.secondary)
                    .rotationEffect(.degrees(refreshRotation))
            }
            .buttonStyle(.plain)
            .disabled(discovery.isRefreshing)
            .help("Refresh Devices")

            // Settings button — opens main window directly to Settings page
            Button {
                onShowSettings()
            } label: {
                Image(systemName: "gear")
                    .font(.body)
                    .foregroundStyle(.secondary)
            }
            .buttonStyle(.plain)
            .help("Settings")
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
    }

    // MARK: - Content

    private var content: some View {
        Group {
            if discovery.devices.isEmpty {
                emptyState
            } else {
                deviceList
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 6) {
            Image(systemName: "ipad")
                .font(.title2)
                .foregroundStyle(.secondary)
            Text("No iPads Detected")
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(.secondary)
            Text("Connect an iPad via USB")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 20)
    }

    private var deviceList: some View {
        VStack(spacing: 2) {
            ForEach(discovery.devices) { device in
                MenuBarDeviceRow(
                    device: device,
                    isMirroring: sessionManager.activeSessions[device.id] != nil,
                    onMirror: {
                        Task {
                            await sessionManager.startMirroring(deviceID: device.id)
                            onMirror(device)
                        }
                    },
                    onStop: {
                        Task { await sessionManager.stopMirroring(deviceID: device.id) }
                    }
                )
                .padding(.horizontal, 8)
            }
        }
        .padding(.vertical, 4)
    }

    // MARK: - Footer

    private var footer: some View {
        VStack(alignment: .leading, spacing: 0) {
            Button {
                onShowMainWindow()
            } label: {
                Label("Show Reflection", systemImage: "macwindow")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
            }
            .buttonStyle(MenuBarButtonStyle())

            Button {
                NSApplication.shared.terminate(nil)
            } label: {
                Label("Quit Reflection", systemImage: "power")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
            }
            .buttonStyle(MenuBarButtonStyle())
        }
        .padding(.vertical, 4)
    }
}

// MARK: - Menu Bar Button Style

/// Hover-highlighted button style matching native macOS menu items.
private struct MenuBarButtonStyle: ButtonStyle {
    @State private var isHovered = false

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.body)
            .foregroundStyle(.primary)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(
                RoundedRectangle(cornerRadius: 4)
                    .fill(isHovered ? Color.white.opacity(0.1) : Color.clear)
            )
            .onHover { hovering in
                isHovered = hovering
            }
    }
}
