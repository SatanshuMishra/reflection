import SwiftUI

struct DeviceListView: View {
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var discovery: DeviceDiscovery
    let onMirror: (DeviceModel) -> Void

    var body: some View {
        NavigationStack {
            Group {
                if discovery.devices.isEmpty {
                    EmptyStateView()
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
                        .listRowBackground(Constants.appBackground)
                    }
                    .scrollContentBackground(.hidden)
                }
            }
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button {
                        discovery.refreshDevices()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                            .rotationEffect(.degrees(discovery.isRefreshing ? 360 : 0))
                            .animation(
                                discovery.isRefreshing
                                    ? .linear(duration: 0.6).repeatForever(autoreverses: false)
                                    : .default,
                                value: discovery.isRefreshing
                            )
                    }
                    .disabled(discovery.isRefreshing)
                    .help("Refresh device list")
                }
            }
        }
        .background(Constants.appBackground)
        .background(
            WindowAccessor { window in
                window.titlebarAppearsTransparent = true
                window.backgroundColor = Constants.appBackgroundNS
                window.isMovableByWindowBackground = true
                window.styleMask.insert(.fullSizeContentView)
                window.titlebarSeparatorStyle = .none
            }
        )
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
