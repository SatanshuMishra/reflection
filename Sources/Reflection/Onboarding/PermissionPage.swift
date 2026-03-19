import AVFoundation
import SwiftUI

/// Onboarding page 2: Camera permission grant.
///
/// Shows a permission card with icon on the LEFT side (HStack layout).
/// After authorization, a "Start Reflecting" button appears at the bottom.
/// This is the final onboarding page — no separate "Ready" page.
struct PermissionPage: View {
    let onComplete: () -> Void

    @State private var permissionStatus: AVAuthorizationStatus = .notDetermined

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            VStack(alignment: .leading, spacing: 4) {
                Text("Grant Permissions")
                    .font(.system(size: 40, weight: .bold))
                    .foregroundStyle(.primary)

                Text("Reflection needs access to be able to access and share your iPad screen.")
                    .font(.system(size: 20))
                    .foregroundStyle(.white)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.top, 8)

            // Permission card — stretches to fill remaining vertical space
            permissionCard
                .frame(maxHeight: .infinity, alignment: .top)
                .padding(.top, 32)
                .animation(.spring(response: 0.4, dampingFraction: 0.8), value: permissionStatus)

            // "Start Reflecting" — appears only after authorization
            if permissionStatus == .authorized {
                Button(action: onComplete) {
                    Text("Start Reflecting")
                        .font(.system(size: 18, weight: .medium))
                        .foregroundStyle(.white)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 12)
                        .background(
                            RoundedRectangle(cornerRadius: 8)
                                .fill(Color.accentColor)
                        )
                }
                .buttonStyle(.plain)
                .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .padding(.horizontal, 64)
        .padding(.vertical, 24)
        .onAppear {
            permissionStatus = AVCaptureDevice.authorizationStatus(for: .video)
        }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in
            let currentStatus = AVCaptureDevice.authorizationStatus(for: .video)
            if currentStatus != permissionStatus {
                withAnimation {
                    permissionStatus = currentStatus
                }
            }
        }
    }

    // MARK: - Permission Card

    private var permissionCard: some View {
        VStack(alignment: .leading, spacing: 24) {
            // Top: icon + title/description in horizontal layout
            HStack(alignment: .top, spacing: 20) {
                // Camera icon
                Image(systemName: "camera.fill")
                    .font(.system(size: 44))
                    .foregroundStyle(.secondary)
                    .frame(width: 140, height: 140)
                    .background(
                        RoundedRectangle(cornerRadius: 18)
                            .fill(Color.white.opacity(0.10))
                    )

                // Title + description + bullets
                VStack(alignment: .leading, spacing: 6) {
                    Text("Camera Access")
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundStyle(.primary)

                    Text("Required to receive your iPad's video feed over USB")
                        .font(.system(size: 15))
                        .foregroundStyle(.secondary)

                    VStack(alignment: .leading, spacing: 6) {
                        bulletPoint("See connected iPads")
                        bulletPoint("Display your iPad screen")
                        bulletPoint("Monitor connection status")
                    }
                    .padding(.top, 12)
                }
            }

            // Action / status button
            statusButton
        }
        .padding(28)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color.white.opacity(0.05))
                .overlay(
                    RoundedRectangle(cornerRadius: 16)
                        .strokeBorder(Color.white.opacity(0.08), lineWidth: 1)
                )
        )
    }

    private func bulletPoint(_ text: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Text("•")
                .font(.system(size: 15))
                .foregroundStyle(.tertiary)
            Text(text)
                .font(.system(size: 15))
                .foregroundStyle(.secondary)
        }
    }

    // MARK: - Status Button

    @ViewBuilder
    private var statusButton: some View {
        switch permissionStatus {
        case .authorized:
            Label("Authorized", systemImage: "checkmark.circle.fill")
                .font(.system(size: 18, weight: .medium))
                .foregroundStyle(.green)
                .padding(.horizontal, 20)
                .padding(.vertical, 8)
                .frame(maxWidth: .infinity)
                .background(
                    RoundedRectangle(cornerRadius: 8)
                        .fill(Color.green.opacity(0.15))
                )
                .transition(.scale.combined(with: .opacity))

        case .notDetermined:
            Button {
                Task {
                    let granted = await AVCaptureDevice.requestAccess(for: .video)
                    withAnimation {
                        permissionStatus = granted ? .authorized : .denied
                    }
                }
            } label: {
                Text("Grant Permission")
                    .font(.system(size: 18, weight: .medium))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
            }
            .buttonStyle(.borderedProminent)

        case .denied, .restricted:
            Button {
                if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Camera") {
                    NSWorkspace.shared.open(url)
                }
            } label: {
                Text("Open System Settings")
                    .font(.system(size: 18, weight: .medium))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
            }
            .buttonStyle(.bordered)
            .tint(.orange)

        @unknown default:
            EmptyView()
        }
    }
}

// MARK: - Preview

struct PermissionPage_Previews: PreviewProvider {
    static var previews: some View {
        PermissionPage(onComplete: {})
            .frame(width: 800, height: 600)
            .background(Constants.appBackground)
            .preferredColorScheme(.dark)
    }
}
