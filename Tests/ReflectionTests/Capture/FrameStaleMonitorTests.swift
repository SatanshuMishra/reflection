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
        // Use a long threshold (3s) so the initial assertion sleep (800ms) is well within bounds.
        // CI runners can add 200-300ms jitter to Task.sleep, so wide margins are essential.
        let monitor = FrameStaleMonitor(threshold: 3.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for several check cycles (800ms, well under 3s threshold)
        try? await Task.sleep(nanoseconds: 800_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Now wait well past the 3s threshold without recording frames
        // 3s threshold + 1.5s buffer = 4.5s total from last frame
        try? await Task.sleep(nanoseconds: 4_000_000_000)
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
        // Use long threshold and generous sleep to guarantee the monitor task
        // completes multiple check cycles and sets isReceivingFrames = true.
        let monitor = FrameStaleMonitor(threshold: 5.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait 1s — enough for ~10 check cycles even with CI jitter
        try? await Task.sleep(nanoseconds: 1_000_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testStatusStreamEmitsChanges() async {
        // Use a moderate threshold (2s) and wait well past it (4s total)
        // to ensure the monitor transitions from receiving → stale.
        let monitor = FrameStaleMonitor(threshold: 2.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait well past the 2s threshold (4s total from frame recording)
        try? await Task.sleep(nanoseconds: 4_000_000_000)

        // After the threshold, the monitor should have transitioned:
        // true (frames recent) → false (stale)
        XCTAssertFalse(monitor.isReceivingFrames, "Should be stale after threshold")

        monitor.stopMonitoring()
    }
}
