import AVFoundation
import Foundation
import os

private func debugLog(_ message: String) {
    let logFile = "/tmp/reflection_debug.log"
    let timestamp = ISO8601DateFormatter().string(from: Date())
    let line = "[\(timestamp)] \(message)\n"
    if let data = line.data(using: .utf8) {
        if FileManager.default.fileExists(atPath: logFile) {
            if let handle = FileHandle(forWritingAtPath: logFile) {
                handle.seekToEndOfFile()
                handle.write(data)
                handle.closeFile()
            }
        } else {
            FileManager.default.createFile(atPath: logFile, contents: data)
        }
    }
}

public final class USBCapture: ScreenCapture, @unchecked Sendable {

    // MARK: - ScreenCapture Protocol

    public private(set) var state: CaptureState = .idle
    public let stateStream: AsyncStream<CaptureState>
    public private(set) var captureSession: AVCaptureSession?

    public var isReceivingFrames: Bool { frameStaleMonitor.isReceivingFrames }
    public var frameStatusStream: AsyncStream<Bool> { frameStaleMonitor.statusStream }

    // MARK: - Private

    private let deviceID: String
    private let stateContinuation: AsyncStream<CaptureState>.Continuation
    private var interruptionObservers: [NSObjectProtocol] = []
    private let logger = Logger.capture
    private let frameStaleMonitor = FrameStaleMonitor()
    private let frameDelegate = FrameTimestampDelegate()
    private let frameDelegateQueue = DispatchQueue(label: "com.reflection.frameMonitor", qos: .userInitiated)

    // MARK: - Init

    public init(deviceID: String) {
        self.deviceID = deviceID
        let (stream, continuation) = AsyncStream<CaptureState>.makeStream()
        self.stateStream = stream
        self.stateContinuation = continuation
    }

    deinit {
        stateContinuation.finish()
        removeInterruptionObservers()
        frameStaleMonitor.stopMonitoring()
    }

    // MARK: - ScreenCapture

    public func startCapture() async throws {
        debugLog("[USBCapture] startCapture for device \(deviceID)")
        updateState(.starting)

        try checkPermission()
        debugLog("[USBCapture] Permission OK")

        let session = try configureSession()
        self.captureSession = session
        debugLog("[USBCapture] Session configured, inputs=\(session.inputs.count)")

        registerInterruptionObservers(for: session)

        // startRunning() blocks — run on background thread
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            DispatchQueue.global(qos: .userInitiated).async {
                session.startRunning()
                continuation.resume()
            }
        }

        if session.isRunning {
            frameStaleMonitor.startMonitoring()
            updateState(.running)
            debugLog("[USBCapture] Capture RUNNING for device \(deviceID)")
        } else {
            debugLog("[USBCapture] Session FAILED to start running for device \(deviceID)")
            throw CaptureError.sessionConfigurationFailed("Session failed to start running")
        }
    }

    public func stopCapture() async {
        guard let session = captureSession else { return }

        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            DispatchQueue.global(qos: .userInitiated).async {
                session.stopRunning()
                continuation.resume()
            }
        }

        frameStaleMonitor.stopMonitoring()
        removeInterruptionObservers()
        captureSession = nil
        updateState(.stopped)
        logger.info("Capture stopped for device \(self.deviceID)")
    }

    // MARK: - Permission

    /// Checks camera authorization status without triggering the system permission dialog.
    /// Permission must be granted through PermissionPage before capture can start.
    private func checkPermission() throws {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            return
        case .notDetermined:
            throw CaptureError.permissionNotDetermined
        case .denied, .restricted:
            throw CaptureError.permissionDenied
        @unknown default:
            throw CaptureError.unknownError("Unknown camera authorization status")
        }
    }

    // MARK: - Session Configuration

    private func configureSession() throws -> AVCaptureSession {
        guard let device = AVCaptureDevice(uniqueID: deviceID) else {
            debugLog("[USBCapture] Device NOT FOUND for ID: \(deviceID)")
            throw CaptureError.deviceNotFound
        }
        debugLog("[USBCapture] Found device: \(device.localizedName) | model=\(device.modelID) | video=\(device.hasMediaType(.video)) | muxed=\(device.hasMediaType(.muxed))")

        let session = AVCaptureSession()
        session.beginConfiguration()

        // Don't override the device's native format. For external iOS devices,
        // forcing a preset like .high can cause unnecessary transcoding overhead.
        // On macOS .inputPriority is unavailable, so we omit setting a preset
        // (defaults to .high) and configure the device format directly instead.

        let input: AVCaptureDeviceInput
        do {
            input = try AVCaptureDeviceInput(device: device)
        } catch {
            session.commitConfiguration()
            debugLog("[USBCapture] Failed to create input: \(error.localizedDescription)")
            throw CaptureError.sessionConfigurationFailed(error.localizedDescription)
        }

        guard session.canAddInput(input) else {
            session.commitConfiguration()
            debugLog("[USBCapture] Cannot add input to session")
            throw CaptureError.sessionConfigurationFailed("Cannot add device input to session")
        }
        session.addInput(input)

        // Optimize device for low latency: maximize frame rate, disable unneeded features
        configureDeviceForLowLatency(device)

        // Disable audio ports on muxed connections to eliminate A/V sync buffering.
        // iPad streams as muxed (audio+video); if AVFoundation tries to synchronize
        // audio it adds 10-30ms of buffering we don't need for a video-only mirror.
        for connection in session.connections {
            for port in connection.inputPorts where port.mediaType == .audio {
                port.isEnabled = false
                debugLog("[USBCapture] Disabled audio port to reduce sync latency")
            }
            if connection.isVideoMirroringSupported {
                connection.automaticallyAdjustsVideoMirroring = false
                connection.isVideoMirrored = false
            }
        }

        // Add video data output for frame timestamp monitoring (lock/sleep detection).
        // This coexists with PreviewLayer — we only track timestamps, not pixel data.
        let videoOutput = AVCaptureVideoDataOutput()
        videoOutput.alwaysDiscardsLateVideoFrames = true
        frameDelegate.monitor = frameStaleMonitor
        videoOutput.setSampleBufferDelegate(frameDelegate, queue: frameDelegateQueue)
        if session.canAddOutput(videoOutput) {
            session.addOutput(videoOutput)
            debugLog("[USBCapture] Added video data output for frame monitoring")
        }

        session.commitConfiguration()
        debugLog("[USBCapture] Session configured (preset=inputPriority)")

        return session
    }

    private func configureDeviceForLowLatency(_ device: AVCaptureDevice) {
        do {
            try device.lockForConfiguration()

            let supportedRanges = device.activeFormat.videoSupportedFrameRateRanges
            debugLog("[USBCapture] Active format frame rates: \(supportedRanges.map { "\($0.minFrameRate)-\($0.maxFrameRate)fps" })")

            // Lock to the highest supported frame rate (up to 60fps).
            // Higher FPS = shorter per-frame interval = lower perceived latency.
            if let maxRange = supportedRanges.max(by: { $0.maxFrameRate < $1.maxFrameRate }) {
                let targetFPS = min(maxRange.maxFrameRate, 60.0)
                let frameDuration = CMTime(value: 1, timescale: CMTimeScale(targetFPS))
                device.activeVideoMinFrameDuration = frameDuration
                device.activeVideoMaxFrameDuration = frameDuration
                debugLog("[USBCapture] Locked frame rate to \(targetFPS)fps")
            }

            device.unlockForConfiguration()
        } catch {
            debugLog("[USBCapture] Could not lock device for config: \(error.localizedDescription)")
        }
    }

    // MARK: - Interruption Handling

    private func registerInterruptionObservers(for session: AVCaptureSession) {
        let runtimeError = NotificationCenter.default.addObserver(
            forName: .AVCaptureSessionRuntimeError,
            object: session,
            queue: .main
        ) { [weak self] notification in
            self?.handleRuntimeError(notification)
        }

        let interrupted = NotificationCenter.default.addObserver(
            forName: .AVCaptureSessionWasInterrupted,
            object: session,
            queue: .main
        ) { [weak self] notification in
            self?.handleInterruption(notification)
        }

        let resumed = NotificationCenter.default.addObserver(
            forName: .AVCaptureSessionInterruptionEnded,
            object: session,
            queue: .main
        ) { [weak self] _ in
            self?.logger.info("Capture session interruption ended.")
            self?.updateState(.running)
        }

        interruptionObservers = [runtimeError, interrupted, resumed]
    }

    private func removeInterruptionObservers() {
        for observer in interruptionObservers {
            NotificationCenter.default.removeObserver(observer)
        }
        interruptionObservers = []
    }

    private func handleRuntimeError(_ notification: Notification) {
        guard let error = notification.userInfo?[AVCaptureSessionErrorKey] as? AVError else {
            updateState(.failed(.unknownError("Unknown runtime error")))
            return
        }
        logger.error("Capture runtime error: \(error.localizedDescription)")

        // Check if the device is still physically connected.
        // If AVCaptureDevice can no longer find it, this is a USB disconnect.
        if AVCaptureDevice(uniqueID: deviceID) == nil {
            logger.info("Device \(self.deviceID) no longer found — treating as disconnect")
            frameStaleMonitor.stopMonitoring()
            updateState(.failed(.deviceDisconnected))
        } else {
            updateState(.failed(.captureInterrupted(reason: error.localizedDescription)))
        }
    }

    private func handleInterruption(_ notification: Notification) {
        // On macOS, AVCaptureSession interruptions are less common than iOS.
        // The interruption reason enum is iOS-only, so we handle generically.
        logger.warning("Capture session was interrupted.")
        updateState(.failed(.captureInterrupted(reason: "Capture session was interrupted")))
    }

    // MARK: - State Management

    private func updateState(_ newState: CaptureState) {
        state = newState
        stateContinuation.yield(newState)
    }
}

// MARK: - Frame Timestamp Delegate

/// Lightweight delegate that records frame arrival timestamps for stale-feed detection.
/// Does not process pixel data — only notifies the monitor that a frame arrived.
private final class FrameTimestampDelegate: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, @unchecked Sendable {
    var monitor: FrameStaleMonitor?

    func captureOutput(
        _ output: AVCaptureOutput,
        didOutput sampleBuffer: CMSampleBuffer,
        from connection: AVCaptureConnection
    ) {
        monitor?.recordFrame()
    }
}
