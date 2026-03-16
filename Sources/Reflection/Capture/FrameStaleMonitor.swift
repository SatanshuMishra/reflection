import Foundation

/// Monitors frame delivery timestamps to detect when a capture feed has gone stale
/// (e.g., when an iPad is locked or sleeping). This class is independently testable
/// without AVFoundation dependencies.
public final class FrameStaleMonitor: @unchecked Sendable {

    /// Async stream that emits `true` when frames are being received,
    /// `false` when the feed has gone stale.
    public let statusStream: AsyncStream<Bool>

    /// Whether frames are currently being received within the threshold.
    public private(set) var isReceivingFrames: Bool = false

    private let continuation: AsyncStream<Bool>.Continuation
    private var lastFrameTime: Date = .distantPast
    private let lock = NSLock()
    private var monitorTask: Task<Void, Never>?
    private let threshold: TimeInterval
    private let checkIntervalNanos: UInt64

    public init(
        threshold: TimeInterval = 1.0,
        checkIntervalNanos: UInt64 = 500_000_000
    ) {
        self.threshold = threshold
        self.checkIntervalNanos = checkIntervalNanos
        let (stream, continuation) = AsyncStream<Bool>.makeStream()
        self.statusStream = stream
        self.continuation = continuation
    }

    deinit {
        monitorTask?.cancel()
        continuation.finish()
    }

    /// Called by the video data output delegate each time a frame arrives.
    /// Thread-safe — may be called from any queue.
    public func recordFrame() {
        lock.lock()
        lastFrameTime = Date()
        lock.unlock()
    }

    /// Returns the time elapsed since the last frame, protected by the lock.
    private func elapsedSinceLastFrame() -> TimeInterval {
        lock.lock()
        let elapsed = Date().timeIntervalSince(lastFrameTime)
        lock.unlock()
        return elapsed
    }

    /// Starts periodic monitoring of frame delivery.
    /// Emits changes to `statusStream` when receiving status changes.
    public func startMonitoring() {
        monitorTask?.cancel()
        monitorTask = Task { [weak self] in
            guard let self else { return }
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: self.checkIntervalNanos)
                guard !Task.isCancelled else { break }

                let elapsed = self.elapsedSinceLastFrame()
                let receiving = elapsed <= self.threshold
                if receiving != self.isReceivingFrames {
                    self.isReceivingFrames = receiving
                    self.continuation.yield(receiving)
                }
            }
        }
    }

    /// Stops the monitoring task and resets state.
    public func stopMonitoring() {
        monitorTask?.cancel()
        monitorTask = nil
        if isReceivingFrames {
            isReceivingFrames = false
            continuation.yield(false)
        }
    }
}
