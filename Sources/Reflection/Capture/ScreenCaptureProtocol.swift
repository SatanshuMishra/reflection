import AVFoundation

public enum CaptureState: Sendable, Equatable {
    case idle
    case starting
    case running
    case stopped
    case failed(CaptureError)
}

public protocol ScreenCapture: AnyObject, Sendable {
    var state: CaptureState { get }
    var stateStream: AsyncStream<CaptureState> { get }

    /// Whether the capture is currently receiving frames from the device.
    var isReceivingFrames: Bool { get }

    /// Async stream that emits when frame receiving status changes.
    var frameStatusStream: AsyncStream<Bool> { get }

    func startCapture() async throws
    func stopCapture() async

    /// The capture session, exposed for AVCaptureVideoPreviewLayer binding.
    /// Returns nil for capture backends that deliver frames differently (e.g. wireless).
    var captureSession: AVCaptureSession? { get }
}
