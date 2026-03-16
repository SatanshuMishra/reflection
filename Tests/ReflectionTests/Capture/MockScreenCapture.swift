import AVFoundation
@testable import Reflection

final class MockScreenCapture: ScreenCapture, @unchecked Sendable {
    var state: CaptureState = .idle
    let stateStream: AsyncStream<CaptureState>
    var captureSession: AVCaptureSession? = nil

    var isReceivingFrames: Bool = true
    let frameStatusStream: AsyncStream<Bool>
    private let frameStatusContinuation: AsyncStream<Bool>.Continuation

    var startCaptureCallCount = 0
    var stopCaptureCallCount = 0
    var shouldThrowOnStart: CaptureError?

    private let stateContinuation: AsyncStream<CaptureState>.Continuation

    init() {
        let (stream, continuation) = AsyncStream<CaptureState>.makeStream()
        self.stateStream = stream
        self.stateContinuation = continuation

        let (frameStream, frameContinuation) = AsyncStream<Bool>.makeStream()
        self.frameStatusStream = frameStream
        self.frameStatusContinuation = frameContinuation
    }

    func startCapture() async throws {
        startCaptureCallCount += 1
        if let error = shouldThrowOnStart {
            state = .failed(error)
            stateContinuation.yield(.failed(error))
            throw error
        }
        state = .running
        stateContinuation.yield(.running)
    }

    func stopCapture() async {
        stopCaptureCallCount += 1
        state = .stopped
        stateContinuation.yield(.stopped)
    }

    func simulateDisconnect() {
        state = .failed(.deviceDisconnected)
        stateContinuation.yield(.failed(.deviceDisconnected))
    }

    func simulateLock() {
        isReceivingFrames = false
        frameStatusContinuation.yield(false)
    }

    func simulateUnlock() {
        isReceivingFrames = true
        frameStatusContinuation.yield(true)
    }
}
