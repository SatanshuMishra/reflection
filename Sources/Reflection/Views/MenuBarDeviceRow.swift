import SwiftUI

/// Compact device row for the menu bar dropdown.
/// Shows status dot, device name, and Mirror/Stop button.
struct MenuBarDeviceRow: View {
    let device: DeviceModel
    let isMirroring: Bool
    let onMirror: () -> Void
    let onStop: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            StatusIndicatorView(isConnected: device.isConnected)

            Text(device.name)
                .lineLimit(1)

            Spacer()

            Button(isMirroring ? "Stop" : "Mirror") {
                if isMirroring {
                    onStop()
                } else {
                    onMirror()
                }
            }
            .controlSize(.small)
            .buttonStyle(.borderedProminent)
            .tint(isMirroring ? .red : .accentColor)
        }
        .padding(.vertical, 2)
    }
}
