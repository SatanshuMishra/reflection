import XCTest
@testable import Reflection

@MainActor
final class MirrorSessionManagerTests: XCTestCase {

    // MARK: - Starting mirroring

    func testStartMirroringCreatesActiveSession() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })

        await manager.startMirroring(deviceID: "device-001")

        XCTAssertNotNil(manager.activeSessions["device-001"])
    }

    func testStartMirroringCallsStartCapture() async {
        let mock = MockScreenCapture()
        let manager = MirrorSessionManager(captureFactory: { _ in mock })

        await manager.startMirroring(deviceID: "device-001")

        XCTAssertEqual(mock.startCaptureCallCount, 1)
    }

    // MARK: - Stopping mirroring

    func testStopMirroringRemovesSession() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })
        await manager.startMirroring(deviceID: "device-001")

        await manager.stopMirroring(deviceID: "device-001")

        XCTAssertNil(manager.activeSessions["device-001"])
    }

    func testStopMirroringCallsStopCapture() async {
        let mock = MockScreenCapture()
        let manager = MirrorSessionManager(captureFactory: { _ in mock })
        await manager.startMirroring(deviceID: "device-001")

        await manager.stopMirroring(deviceID: "device-001")

        XCTAssertEqual(mock.stopCaptureCallCount, 1)
    }

    // MARK: - Duplicate prevention

    func testStartMirroringTwiceDoesNotCreateDuplicate() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })

        await manager.startMirroring(deviceID: "device-001")
        await manager.startMirroring(deviceID: "device-001")

        // Only one session should exist — second call is a no-op
        XCTAssertEqual(manager.activeSessions.count, 1)
    }

    // MARK: - Error handling

    func testCaptureErrorSurfacesInCurrentError() async {
        let mock = MockScreenCapture()
        mock.shouldThrowOnStart = .permissionDenied
        let manager = MirrorSessionManager(captureFactory: { _ in mock })

        await manager.startMirroring(deviceID: "device-001")

        XCTAssertEqual(manager.currentError, .permissionDenied)
    }

    func testCaptureErrorCleansUpSession() async {
        let mock = MockScreenCapture()
        mock.shouldThrowOnStart = .deviceNotFound
        let manager = MirrorSessionManager(captureFactory: { _ in mock })

        await manager.startMirroring(deviceID: "device-001")

        XCTAssertNil(manager.activeSessions["device-001"])
    }

    func testDismissingErrorClearsCurrentError() async {
        let mock = MockScreenCapture()
        mock.shouldThrowOnStart = .permissionDenied
        let manager = MirrorSessionManager(captureFactory: { _ in mock })
        await manager.startMirroring(deviceID: "device-001")
        XCTAssertNotNil(manager.currentError)

        manager.currentError = nil

        XCTAssertNil(manager.currentError)
    }

    // MARK: - Stop nonexistent session

    func testStopMirroringNonexistentDeviceIsNoOp() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })

        await manager.stopMirroring(deviceID: "nonexistent")

        XCTAssertTrue(manager.activeSessions.isEmpty)
    }

    // MARK: - Multiple devices

    func testMultipleDevicesHaveIndependentSessions() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })

        await manager.startMirroring(deviceID: "device-001")
        await manager.startMirroring(deviceID: "device-002")

        XCTAssertEqual(manager.activeSessions.count, 2)
        XCTAssertNotNil(manager.activeSessions["device-001"])
        XCTAssertNotNil(manager.activeSessions["device-002"])
    }

    func testStoppingOneDeviceDoesNotAffectOther() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })
        await manager.startMirroring(deviceID: "device-001")
        await manager.startMirroring(deviceID: "device-002")

        await manager.stopMirroring(deviceID: "device-001")

        XCTAssertNil(manager.activeSessions["device-001"])
        XCTAssertNotNil(manager.activeSessions["device-002"])
    }

    // MARK: - Window close notification cleanup

    func testWindowCloseNotificationStopsSession() async {
        let mock = MockScreenCapture()
        let manager = MirrorSessionManager(captureFactory: { _ in mock })
        await manager.startMirroring(deviceID: "device-001")
        XCTAssertNotNil(manager.activeSessions["device-001"])

        NotificationCenter.default.post(
            name: .mirrorWindowClosed,
            object: nil,
            userInfo: ["deviceID": "device-001"]
        )

        // Allow the async Task in the notification handler to execute
        try? await Task.sleep(nanoseconds: 200_000_000)

        XCTAssertNil(manager.activeSessions["device-001"])
        XCTAssertEqual(mock.stopCaptureCallCount, 1)
    }

    func testWindowCloseForUnknownDeviceIsNoOp() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })
        await manager.startMirroring(deviceID: "device-001")

        NotificationCenter.default.post(
            name: .mirrorWindowClosed,
            object: nil,
            userInfo: ["deviceID": "unknown-device"]
        )

        try? await Task.sleep(nanoseconds: 200_000_000)

        XCTAssertNotNil(manager.activeSessions["device-001"])
        XCTAssertEqual(manager.activeSessions.count, 1)
    }

    func testWindowCloseDoesNotAffectOtherSessions() async {
        let manager = MirrorSessionManager(captureFactory: { _ in MockScreenCapture() })
        await manager.startMirroring(deviceID: "device-001")
        await manager.startMirroring(deviceID: "device-002")

        NotificationCenter.default.post(
            name: .mirrorWindowClosed,
            object: nil,
            userInfo: ["deviceID": "device-001"]
        )

        try? await Task.sleep(nanoseconds: 200_000_000)

        XCTAssertNil(manager.activeSessions["device-001"])
        XCTAssertNotNil(manager.activeSessions["device-002"])
    }
}
