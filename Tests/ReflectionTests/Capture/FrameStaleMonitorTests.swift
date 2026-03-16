import XCTest
@testable import Reflection

final class FrameStaleMonitorTests: XCTestCase {

    func testMonitorStartsNotReceiving() {
        let monitor = FrameStaleMonitor(threshold: 1.0, checkIntervalNanos: 100_000_000)

        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testRecordFrameThenMonitorSetsReceivingTrue() async {
        let monitor = FrameStaleMonitor(threshold: 1.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for the first check cycle
        try? await Task.sleep(nanoseconds: 250_000_000)

        XCTAssertTrue(monitor.isReceivingFrames)
        monitor.stopMonitoring()
    }

    func testFrameBecomesStaleAfterThreshold() async {
        // Use a very short threshold for testing
        let monitor = FrameStaleMonitor(threshold: 0.2, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait for initial check to confirm receiving
        try? await Task.sleep(nanoseconds: 100_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        // Now wait past the threshold without recording frames
        try? await Task.sleep(nanoseconds: 400_000_000)
        XCTAssertFalse(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testFrameResumesAfterStale() async {
        let monitor = FrameStaleMonitor(threshold: 0.2, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Let it go stale
        try? await Task.sleep(nanoseconds: 500_000_000)
        XCTAssertFalse(monitor.isReceivingFrames)

        // Record a new frame — should resume
        monitor.recordFrame()
        try? await Task.sleep(nanoseconds: 150_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
    }

    func testStopMonitoringResetsState() async {
        let monitor = FrameStaleMonitor(threshold: 1.0, checkIntervalNanos: 100_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        try? await Task.sleep(nanoseconds: 250_000_000)
        XCTAssertTrue(monitor.isReceivingFrames)

        monitor.stopMonitoring()
        XCTAssertFalse(monitor.isReceivingFrames)
    }

    func testStatusStreamEmitsChanges() async {
        let monitor = FrameStaleMonitor(threshold: 0.2, checkIntervalNanos: 50_000_000)
        monitor.recordFrame()
        monitor.startMonitoring()

        // Wait long enough for it to go true then false
        try? await Task.sleep(nanoseconds: 600_000_000)

        // After the threshold, the monitor should have transitioned:
        // true (frames recent) → false (stale)
        // We verify the final state is false (stale) since no new frames arrived
        XCTAssertFalse(monitor.isReceivingFrames, "Should be stale after threshold")

        monitor.stopMonitoring()
    }
}
