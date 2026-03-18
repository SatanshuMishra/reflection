import SwiftUI

struct DeviceListView: View {
    @ObservedObject var sessionManager: MirrorSessionManager
    @ObservedObject var discovery: DeviceDiscovery
    @ObservedObject var appSettings: AppSettings
    let onMirror: (DeviceModel) -> Void

    @State private var showingSettings = false

    var body: some View {
        NavigationStack {
            Group {
                if showingSettings {
                    settingsPage
                } else {
                    devicePage
                }
            }
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    if showingSettings {
                        Button {
                            withAnimation(.easeInOut(duration: 0.2)) {
                                showingSettings = false
                            }
                        } label: {
                            Image(systemName: "chevron.left")
                        }
                        .help("Back to devices")
                    } else {
                        HStack(spacing: 8) {
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

                            Button {
                                withAnimation(.easeInOut(duration: 0.2)) {
                                    showingSettings = true
                                }
                            } label: {
                                Image(systemName: "gear")
                            }
                            .help("Settings")
                            .keyboardShortcut(",", modifiers: .command)
                        }
                    }
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

    private var devicePage: some View {
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
                }
                .scrollContentBackground(.hidden)
            }
        }
    }

    private var settingsPage: some View {
        SettingsView(settings: appSettings)
    }
}
