import XCTest
@testable import Reflection

final class DeviceModelTests: XCTestCase {

    // MARK: - Initialization

    func testInitWithProperties() {
        let device = DeviceModel(
            id: "device-001",
            name: "iPad Pro",
            modelID: "iPad13,4",
            isConnected: true
        )
        XCTAssertEqual(device.id, "device-001")
        XCTAssertEqual(device.name, "iPad Pro")
        XCTAssertEqual(device.modelID, "iPad13,4")
        XCTAssertTrue(device.isConnected)
    }

    // MARK: - Identifiable

    func testIdentifiableUsesUniqueID() {
        let device1 = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        let device2 = DeviceModel(id: "xyz", name: "iPad", modelID: "M1", isConnected: true)
        XCTAssertNotEqual(device1.id, device2.id)
    }

    // MARK: - Equatable

    func testEqualDevicesAreEqual() {
        let device1 = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        let device2 = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        XCTAssertEqual(device1, device2)
    }

    func testDifferentIDMakesNotEqual() {
        let device1 = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        let device2 = DeviceModel(id: "xyz", name: "iPad", modelID: "M1", isConnected: true)
        XCTAssertNotEqual(device1, device2)
    }

    func testDifferentConnectionStateMakesNotEqual() {
        let connected = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        let disconnected = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: false)
        XCTAssertNotEqual(connected, disconnected)
    }

    // MARK: - Sendable (compile-time check)

    func testIsSendable() async {
        let device: any Sendable = DeviceModel(
            id: "abc", name: "iPad", modelID: "M1", isConnected: true
        )
        XCTAssertTrue(device is DeviceModel)
    }

    // MARK: - Immutability

    func testIsValueType() {
        let device1 = DeviceModel(id: "abc", name: "iPad", modelID: "M1", isConnected: true)
        var device2 = device1
        // DeviceModel is a struct — mutating device2 should not affect device1
        // Since all fields are let, we verify identity via equality
        XCTAssertEqual(device1, device2)
        // Reassigning device2 entirely should not affect device1
        device2 = DeviceModel(id: "xyz", name: "iPad Air", modelID: "M2", isConnected: false)
        XCTAssertNotEqual(device1, device2)
        XCTAssertEqual(device1.id, "abc") // device1 unchanged
    }
}
