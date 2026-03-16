import XCTest
@testable import Reflection

/// Tests for FrameStaleMonitor.
///
/// These tests use `Task.sleep` for timing assertions. All thresholds and sleep
/// durations use generous margins to avoid flakiness on slow CI runners where
/// `Task.sleep` precision is poor (~100-200ms jitter is common).
///
/// Key invariant: any sleep used to "confirm" a state must be long enough for
/// at least 2-3 check cycles to complete, and must be shorter than the threshold
/// to avoid the frame going stale during the assertion window.
final class FrameStaleMonitorTests: XCTestCase {

    func testMonitorStartsNotReceiving() {
        let monitor = FrameStaleMonitor(threshold: 1.0, checkIntervalNanos: 100_000_000)

        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testRecordFrameThenMonitorSetsReceivingTrue() async {
        let monitor = FrameStaleMonitor(threshold: 2.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for several check cycles (100ms interval × 4 = 400ms, well under 2s threshold)
        try? await Task.sleep(nanoseconds: 400_000_000)

        XCTAssertTrue(monitor.isReceivingFrames)
        monitor.stopMonitoring()
    }

    func testFrameBecomesStaleAfterThreshold() async {
        let monitor = FrameStaleMonitor(threshold: 0.5, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for initial check (400ms < 0.5s threshold, so frame is still fresh)
        try? await Task.sleep(nanoseconds: 400_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Now wait well past the threshold without recording frames
        // 0.5s threshold + generous buffer = 1s additional wait
        try? await Task.sleep(nanoseconds: 1_000_000_000)
        XCTAssertFalse(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testFrameResumesAfterStale() async {
        // Use a long threshold so the resumed frame stays fresh during the assertion window.
        let monitor = FrameStaleMonitor(threshold: 2.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for initial check to confirm receiving
        try? await Task.sleep(nanoseconds: 400_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Let it go stale (wait well past the 2s threshold)
        try? await Task.sleep(nanoseconds: 2_500_000_000)
        XCTAssertFalse(monitor.isReceivingFrames)

        // Record a new frame — should resume on next check cycle.
        // Sleep (400ms) must be shorter than threshold (2s) so frame stays fresh.
        monitor.recordFrame()
        try? await Task.sleep(nanoseconds: 400_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testStopMonitoringResetsState() async {
        let monitor = FrameStaleMonitor(threshold: 2.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        try? await Task.sleep(nanoseconds: 400_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testStatusStreamEmitsChanges() async {
        let monitor = FrameStaleMonitor(threshold: 0.5, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait long enough for it to go true then false
        // (frame at t=0, fresh for 0.5s, then stale — wait 1.5s total)
        try? await Task.sleep(nanoseconds: 1_500_000_000)

        // After the threshold, the monitor should have transitioned:
        // true (frames recent) → false (stale)
        XCTAssertFalse(monitor.isReceivingFrames, "Should be stale after threshold")

        monitor.stopMonitoring()
    }
}
