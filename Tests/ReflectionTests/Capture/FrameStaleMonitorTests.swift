import XCTest
@testable import Reflection

/// Tests for FrameStaleMonitor.
///
/// These tests use the monitor's `statusStream` to await real state transitions
/// instead of relying on `Task.sleep` timing. This eliminates flakiness from
/// CI runner jitter — tests wait for actual events, not estimated durations.
final class FrameStaleMonitorTests: XCTestCase {

    /// Awaits the next value from a FrameStaleMonitor's statusStream that matches
    /// the expected value. Fails the test if the timeout expires first.
    private func awaitStatus(
        _ expected: Bool,
        from monitor: FrameStaleMonitor,
        timeout: TimeInterval = 10.0,
        file: StaticString = #filePath,
        line: UInt = #line
    ) async {
        let deadline = Date().addingTimeInterval(timeout)

        // If the monitor already has the expected value, check if we need to wait
        // for a stream emission or if we can return immediately
        for await value in monitor.statusStream {
            if value == expected {
                return
            }
            if Date() > deadline {
                XCTFail(
                    "Timed out waiting for isReceivingFrames == \(expected)",
                    file: file,
                    line: line
                )
                return
            }
        }
        // Stream ended without seeing expected value
        XCTFail(
            "statusStream ended without emitting \(expected)",
            file: file,
            line: line
        )
    }

    func testMonitorStartsNotReceiving() {
        let monitor = FrameStaleMonitor(threshold: 1.0, checkIntervalNanos: 50_000_000)

        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testRecordFrameThenMonitorSetsReceivingTrue() async {
        let monitor = FrameStaleMonitor(threshold: 10.0, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        await awaitStatus(true, from: monitor)

        XCTAssertTrue(monitor.isReceivingFrames)
        monitor.stopMonitoring()
    }

    func testFrameBecomesStaleAfterThreshold() async {
        // Short threshold (0.3s) so it goes stale quickly, but check interval is fast enough
        // to catch the transition reliably.
        let monitor = FrameStaleMonitor(threshold: 0.3, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for receiving = true
        await awaitStatus(true, from: monitor)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Don't record any more frames — wait for stale transition
        await awaitStatus(false, from: monitor)
        XCTAssertFalse(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testFrameResumesAfterStale() async {
        let monitor = FrameStaleMonitor(threshold: 0.3, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for receiving = true
        await awaitStatus(true, from: monitor)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Let it go stale
        await awaitStatus(false, from: monitor)
        XCTAssertFalse(monitor.isReceivingFrames)

        // Record a new frame — should resume
        monitor.recordFrame()
        await awaitStatus(true, from: monitor)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testStopMonitoringResetsState() async {
        let monitor = FrameStaleMonitor(threshold: 10.0, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for the monitor to confirm receiving — deterministic, no guessing
        await awaitStatus(true, from: monitor)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testStatusStreamEmitsChanges() async {
        let monitor = FrameStaleMonitor(threshold: 0.3, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Should transition: true (frames recent) → false (stale)
        await awaitStatus(true, from: monitor)
        await awaitStatus(false, from: monitor)

        XCTAssertFalse(monitor.isReceivingFrames, "Should be stale after threshold")

        monitor.stopMonitoring()
    }
}
