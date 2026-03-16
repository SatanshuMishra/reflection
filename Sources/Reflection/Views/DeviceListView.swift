import SwiftUI

struct DeviceListView: View {
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var discovery: DeviceDiscovery
    let onMirror: (DeviceModel) -> Void

    var body: some View {
        NavigationStack {
            Group {
                if discovery.devices.isEmpty {
                    EmptyStateView(onRefresh: { discovery.refreshDevices() })
                } else {
                    List(discovery.devices) { device in
                        DeviceRowView(
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
                    }
                }
            }
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button {
                        discovery.refreshDevices()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .help("Refresh device list")
                }
            }
        }
        .frame(minWidth: 350, minHeight: 200)
        .alert(
            "Error",
            isPresented: Binding(
                get: { sessionManager.currentError != nil },
                set: { if !$0 { sessionManager.currentError = nil } }
            ),
            presenting: sessionManager.currentError
        ) { _ in
            Button("OK") { sessionManager.currentError = nil }
        } message: { error in
            Text(error.localizedDescription)
        }
    }
}
