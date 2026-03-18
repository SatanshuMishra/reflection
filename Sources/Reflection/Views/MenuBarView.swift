import SwiftUI

/// Content for the NSPopover menu bar dropdown.
/// Shows discovered devices with Mirror/Stop controls, plus app actions.
struct MenuBarView: View {
    @ObservedObject var discovery: DeviceDiscovery
    @ObservedObject var sessionManager: MirrorSessionManager
    let onMirror: (DeviceModel) -> Void
    let onShowMainWindow: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Device list
            if discovery.devices.isEmpty {
                emptyState
            } else {
                deviceList
            }

            Divider()
                .padding(.vertical, 4)

            // Refresh
            Button {
                discovery.refreshDevices()
            } label: {
                Label("Refresh Devices", systemImage: "arrow.clockwise")
            }
            .buttonStyle(.plain)
            .disabled(discovery.isRefreshing)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)

            Divider()
                .padding(.vertical, 4)

            // App actions
            Button {
                onShowMainWindow()
            } label: {
                Label("Show Reflection", systemImage: "macwindow")
            }
            .buttonStyle(.plain)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)

            Divider()
                .padding(.vertical, 4)

            Button {
                NSApplication.shared.terminate(nil)
            } label: {
                Label("Quit Reflection", systemImage: "power")
            }
            .buttonStyle(.plain)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
        }
        .padding(8)
        .frame(width: 280)
    }

    private var emptyState: some View {
        VStack(spacing: 4) {
            Image(systemName: "ipad")
                .font(.title2)
                .foregroundStyle(.secondary)
            Text("No iPads Detected")
                .font(.headline)
                .foregroundStyle(.secondary)
            Text("Connect an iPad via USB")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 12)
    }

    private var deviceList: some View {
        VStack(spacing: 4) {
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
    }
}
