import XCTest
@testable import Reflection

final class CaptureStateTests: XCTestCase {

    // MARK: - Equatable

    func testIdleEqualsIdle() {
        XCTAssertEqual(CaptureState.idle, CaptureState.idle)
    }

    func testRunningEqualsRunning() {
        XCTAssertEqual(CaptureState.running, CaptureState.running)
    }

    func testDifferentStatesAreNotEqual() {
        XCTAssertNotEqual(CaptureState.idle, CaptureState.running)
        XCTAssertNotEqual(CaptureState.starting, CaptureState.stopped)
    }

    func testFailedStatesWithSameErrorAreEqual() {
        XCTAssertEqual(
            CaptureState.failed(.deviceNotFound),
            CaptureState.failed(.deviceNotFound)
        )
    }

    func testFailedStatesWithDifferentErrorsAreNotEqual() {
        XCTAssertNotEqual(
            CaptureState.failed(.deviceNotFound),
            CaptureState.failed(.permissionDenied)
        )
    }

    // MARK: - Sendable (compile-time check)

    func testIsSendable() async {
        let state: any Sendable = CaptureState.running
        XCTAssertTrue(state is CaptureState)
    }

    // MARK: - All cases exist

    func testAllCasesExist() {
        let states: [CaptureState] = [
            .idle,
            .starting,
            .running,
            .stopped,
            .failed(.deviceNotFound)
        ]
        XCTAssertEqual(states.count, 5)
    }
}
