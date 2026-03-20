import AVFoundation
import Combine
import Foundation
import os

public typealias CaptureFactory = @Sendable (String) -> any ScreenCapture

@MainActor
public final class MirrorSessionManager: ObservableObject {
    @Published public private(set) var activeSessions: [String: any ScreenCapture] = [:]
    @Published public var currentError: CaptureError?

    public let discovery = DeviceDiscovery()

    private let captureFactory: CaptureFactory
    private var stateObservationTasks: [String: Task<Void, Never>] = [:]
    private nonisolated(unsafe) var windowCloseObserver: NSObjectProtocol?
    private let logger = Logger.session

    public init(captureFactory: @escaping CaptureFactory = { deviceID in USBCapture(deviceID: deviceID) }) {
        self.captureFactory = captureFactory
        registerWindowCloseObserver()
    }

    deinit {
        if let observer = windowCloseObserver {
            NotificationCenter.default.removeObserver(observer)
        }
    }

    private func registerWindowCloseObserver() {
        windowCloseObserver = NotificationCenter.default.addObserver(
            forName: .mirrorWindowClosed,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            guard let deviceID = notification.userInfo?["deviceID"] as? String else { return }
            Task { @MainActor [weak self] in
                await self?.stopMirroring(deviceID: deviceID)
            }
        }
    }

    public func startDiscovery() async {
        await discovery.startDiscovery()
    }

    public func startMirroring(deviceID: String) async {
        guard activeSessions[deviceID] == nil else { return }

        let capture = captureFactory(deviceID)
        activeSessions[deviceID] = capture

        do {
            try await capture.startCapture()
            observeState(of: capture, deviceID: deviceID)
        } catch let error as CaptureError {
            currentError = error
            activeSessions.removeValue(forKey: deviceID)
        } catch {
            currentError = .unknownError(error.localizedDescription)
            activeSessions.removeValue(forKey: deviceID)
        }
    }

    public func stopAllMirroring() async {
        let deviceIDs = Array(activeSessions.keys)
        for deviceID in deviceIDs {
            await stopMirroring(deviceID: deviceID)
        }
    }

    public func stopMirroring(deviceID: String) async {
        guard let capture = activeSessions[deviceID] else { return }

        stateObservationTasks[deviceID]?.cancel()
        stateObservationTasks.removeValue(forKey: deviceID)

        await capture.stopCapture()
        activeSessions.removeValue(forKey: deviceID)
        logger.info("Mirroring stopped for device \(deviceID)")

        NotificationCenter.default.post(
            name: .mirrorSessionStopped,
            object: nil,
            userInfo: ["deviceID": deviceID]
        )
    }

    private func observeState(of capture: any ScreenCapture, deviceID: String) {
        let task = Task { [weak self] in
            for await state in capture.stateStream {
                guard let self else { return }
                if case .failed(let error) = state {
                    self.activeSessions.removeValue(forKey: deviceID)
                    self.stateObservationTasks.removeValue(forKey: deviceID)
                    self.logger.error("Capture failed for \(deviceID): \(error.localizedDescription)")

                    if case .deviceDisconnected = error {
                        // Post disconnect notification — AppDelegate handles prompt + window close
                        NotificationCenter.default.post(
                            name: .deviceDisconnected,
                            object: nil,
                            userInfo: ["deviceID": deviceID]
                        )
                    } else {
                        self.currentError = error
                    }
                }
            }
        }
        stateObservationTasks[deviceID] = task
    }
}
