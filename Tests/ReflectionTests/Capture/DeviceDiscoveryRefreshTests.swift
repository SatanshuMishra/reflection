import XCTest
@testable import Reflection

@MainActor
final class DeviceDiscoveryRefreshTests: XCTestCase {

    func testRefreshBeforeStartDiscoveryDoesNotCrash() {
        let discovery = DeviceDiscovery()

        // Should not crash or throw when called before startDiscovery
        discovery.refreshDevices()

        XCTAssertTrue(discovery.devices.isEmpty)
    }

    func testRefreshWhenNotAuthorizedIsNoOp() {
        let discovery = DeviceDiscovery()
        // cameraAuthorized defaults to false

        discovery.refreshDevices()

        XCTAssertFalse(discovery.cameraAuthorized)
        XCTAssertTrue(discovery.devices.isEmpty)
    }
}
