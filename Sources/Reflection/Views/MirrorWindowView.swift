import SwiftUI
import AVFoundation

struct MirrorWindowView: View {
    let session: AVCaptureSession
    @ObservedObject var windowState: MirrorWindowState

    private var isDeviceLocked: Bool {
        !windowState.isReceivingFrames && windowState.captureState == .running
    }

    var body: some View {
        ZStack {
            Color.black

            PreviewLayerView(session: session)

            if case .starting = windowState.captureState {
                ProgressView("Connecting...")
                    .padding()
                    .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
            }

            if isDeviceLocked {
                // Full-screen blur over the frozen preview frame
                BlurOverlayView()
                    .ignoresSafeArea()
                    .transition(.opacity)

                VStack(spacing: 8) {
                    Image(systemName: "lock.ipad")
                        .font(.system(size: 32))

                    Text("Device may be locked or sleeping")
                        .font(.headline)

                    Text("Unlock your iPad to resume mirroring")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
                .foregroundStyle(.white)
                .padding(20)
                .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
                .transition(.opacity)
            }
        }
        .animation(.easeInOut(duration: Constants.animationDuration), value: isDeviceLocked)
    }
}

// MARK: - Blur Overlay

/// Full-screen blur using NSVisualEffectView for native macOS frosted glass effect.
private struct BlurOverlayView: NSViewRepresentable {
    func makeNSView(context: Context) -> NSVisualEffectView {
        let view = NSVisualEffectView()
        view.material = .hudWindow
        view.blendingMode = .withinWindow
        view.state = .active
        return view
    }

    func updateNSView(_ nsView: NSVisualEffectView, context: Context) {}
}

/// Observable state for a mirror window, updated from the capture's frame status stream.
@MainActor
final class MirrorWindowState: ObservableObject {
    @Published var captureState: CaptureState
    @Published var isReceivingFrames: Bool = true

    private var observeTask: Task<Void, Never>?

    init(captureState: CaptureState = .running) {
        self.captureState = captureState
    }

    func observeFrameStatus(_ stream: AsyncStream<Bool>) {
        observeTask?.cancel()
        observeTask = Task { [weak self] in
            for await receiving in stream {
                guard let self, !Task.isCancelled else { break }
                self.isReceivingFrames = receiving
            }
        }
    }

    deinit {
        observeTask?.cancel()
    }
}
