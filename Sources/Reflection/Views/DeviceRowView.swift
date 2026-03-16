import SwiftUI

struct DeviceRowView: View {
    let device: DeviceModel
    let isMirroring: Bool
    let onMirror: () -> Void
    let onStop: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            StatusIndicatorView(isConnected: device.isConnected)

            VStack(alignment: .leading, spacing: 2) {
                Text(device.name)
                    .font(.headline)
                    .foregroundStyle(.white)
                Text(device.modelID)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Button(isMirroring ? "Stop" : "Mirror") {
                if isMirroring {
                    onStop()
                } else {
                    onMirror()
                }
            }
            .buttonStyle(FlatButtonStyle(color: isMirroring ? .red : .accentColor))
            .animation(.easeInOut(duration: Constants.animationDuration), value: isMirroring)
        }
        .padding(.vertical, 4)
    }
}
