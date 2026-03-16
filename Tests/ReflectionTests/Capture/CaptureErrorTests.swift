import XCTest
@testable import Reflection

final class CaptureErrorTests: XCTestCase {

    // MARK: - All cases produce non-empty localized descriptions

    func testDeviceNotFoundHasActionableDescription() {
        let error = CaptureError.deviceNotFound
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.localizedStandardContains("iPad"))
        XCTAssertTrue(description.localizedStandardContains("USB"))
    }

    func testPermissionDeniedHasActionableDescription() {
        let error = CaptureError.permissionDenied
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(
            description.localizedStandardContains("denied") ||
            description.localizedStandardContains("permission")
        )
        XCTAssertTrue(
            description.localizedStandardContains("Settings") ||
            description.localizedStandardContains("settings")
        )
    }

    func testPermissionNotDeterminedHasDescription() {
        let error = CaptureError.permissionNotDetermined
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.localizedStandardContains("permission"))
    }

    func testSessionConfigurationFailedIncludesDetail() {
        let detail = "Cannot add device input"
        let error = CaptureError.sessionConfigurationFailed(detail)
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.contains(detail))
    }

    func testCaptureInterruptedIncludesReason() {
        let reason = "Video device not available"
        let error = CaptureError.captureInterrupted(reason: reason)
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.contains(reason))
    }

    func testDeviceDisconnectedHasActionableDescription() {
        let error = CaptureError.deviceDisconnected
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.localizedStandardContains("disconnected"))
    }

    func testUnknownErrorIncludesDetail() {
        let detail = "Something went wrong"
        let error = CaptureError.unknownError(detail)
        let description = error.errorDescription ?? ""
        XCTAssertFalse(description.isEmpty)
        XCTAssertTrue(description.contains(detail))
    }

    // MARK: - Equatable conformance

    func testSameCasesAreEqual() {
        XCTAssertEqual(CaptureError.deviceNotFound, CaptureError.deviceNotFound)
        XCTAssertEqual(CaptureError.permissionDenied, CaptureError.permissionDenied)
        XCTAssertEqual(CaptureError.deviceDisconnected, CaptureError.deviceDisconnected)
        XCTAssertEqual(
            CaptureError.sessionConfigurationFailed("x"),
            CaptureError.sessionConfigurationFailed("x")
        )
    }

    func testDifferentCasesAreNotEqual() {
        XCTAssertNotEqual(CaptureError.deviceNotFound, CaptureError.permissionDenied)
        XCTAssertNotEqual(
            CaptureError.sessionConfigurationFailed("a"),
            CaptureError.sessionConfigurationFailed("b")
        )
    }

    // MARK: - Error conformance

    func testConformsToError() {
        let error: any Error = CaptureError.deviceNotFound
        XCTAssertTrue(error is CaptureError)
    }

    func testConformsToSendable() async {
        let error: any Sendable = CaptureError.deviceNotFound
        XCTAssertTrue(error is CaptureError)
    }
}
