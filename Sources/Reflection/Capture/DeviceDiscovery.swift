import AVFoundation
import Combine
import CoreMediaIO
import Foundation
import os

private func debugLog(_ message: String) {
    let logFile = "/tmp/reflection_debug.log"
    let timestamp = ISO8601DateFormatter().string(from: Date())
    let line = "[\(timestamp)] \(message)\n"
    if let data = line.data(using: .utf8) {
        if FileManager.default.fileExists(atPath: logFile) {
            if let handle = FileHandle(forWritingAtPath: logFile) {
                handle.seekToEndOfFile()
                handle.write(data)
                handle.closeFile()
            }
        } else {
            FileManager.default.createFile(atPath: logFile, contents: data)
        }
    }
}

/// iOS devices connected via USB report modelID "iOS Device" through CoreMediaIO.
private let iosDeviceModelID = "iOS Device"

private func isIOSDevice(_ device: AVCaptureDevice) -> Bool {
    device.modelID == iosDeviceModelID
}

@MainActor
public final class DeviceDiscovery: ObservableObject {
    @Published public private(set) var devices: [DeviceModel] = []
    @Published public private(set) var cameraAuthorized = false

    private var discoverySession: AVCaptureDevice.DiscoverySession?
    private var observation: NSKeyValueObservation?
    private let logger = Logger.discovery

    public init() {}

    public func startDiscovery() async {
        debugLog("[Discovery] Starting discovery...")
        enableIOSDeviceDiscovery()
        await requestCameraAccess()

        // CoreMediaIO needs time to register iOS devices after the property is set.
        // Wait before creating the discovery session.
        debugLog("[Discovery] Waiting 3s for CoreMediaIO to register iOS devices...")
        try? await Task.sleep(nanoseconds: 3_000_000_000) // 3 seconds

        logAllAvailableDevices()
        beginDeviceDiscovery()
        debugLog("[Discovery] Discovery complete. Found \(devices.count) device(s).")
        for device in devices {
            debugLog("[Discovery]   - \(device.name) (model=\(device.modelID), connected=\(device.isConnected))")
        }
    }

    public func refreshDevices() {
        guard cameraAuthorized else {
            logger.warning("Cannot refresh: camera not authorized.")
            return
        }
        observation?.invalidate()
        observation = nil
        discoverySession = nil
        beginDeviceDiscovery()
        logger.info("Manual device refresh completed. Found \(self.devices.count) device(s).")
    }

    public func stopDiscovery() {
        observation?.invalidate()
        observation = nil
        discoverySession = nil
        devices = []
        logger.info("Device discovery stopped.")
    }

    // MARK: - Camera Permission

    private func requestCameraAccess() async {
        let status = AVCaptureDevice.authorizationStatus(for: .video)
        switch status {
        case .authorized:
            cameraAuthorized = true
            logger.info("Camera already authorized.")
        case .notDetermined:
            logger.info("Requesting camera access...")
            let granted = await AVCaptureDevice.requestAccess(for: .video)
            cameraAuthorized = granted
            logger.info("Camera access \(granted ? "granted" : "denied").")
        case .denied, .restricted:
            cameraAuthorized = false
            logger.warning("Camera access denied/restricted.")
        @unknown default:
            cameraAuthorized = false
        }
    }

    // MARK: - Device Discovery

    private func beginDeviceDiscovery() {
        let deviceTypes: [AVCaptureDevice.DeviceType]
        if #available(macOS 14.0, *) {
            deviceTypes = [.external]
        } else {
            deviceTypes = [.externalUnknown]
        }

        // Use nil mediaType to catch all devices (iOS devices register as muxed)
        let primarySession = AVCaptureDevice.DiscoverySession(
            deviceTypes: deviceTypes,
            mediaType: nil,
            position: .unspecified
        )
        self.discoverySession = primarySession

        // Filter to only iOS devices — excludes virtual cameras, webcams, etc.
        let iosDevices = primarySession.devices.filter(isIOSDevice)

        debugLog("[Discovery] Total external devices: \(primarySession.devices.count), iOS devices: \(iosDevices.count)")
        for device in primarySession.devices {
            let included = isIOSDevice(device) ? "INCLUDED" : "FILTERED OUT"
            debugLog("[Discovery]   \(device.localizedName) | model=\(device.modelID) | \(included)")
        }

        devices = iosDevices.map { DeviceModel(from: $0) }

        observation = primarySession.observe(\.devices, options: [.new]) { [weak self] _, change in
            let models = (change.newValue ?? [])
                .filter(isIOSDevice)
                .map { DeviceModel(from: $0) }
            Task { @MainActor [weak self] in
                guard let self else { return }
                if models != self.devices {
                    self.devices = models
                    self.logger.info("Devices updated: \(models.count) iOS device(s) found.")
                }
            }
        }

        logger.info("Device discovery started. Found \(self.devices.count) iOS device(s).")
    }

    // MARK: - Diagnostics

    private func logAllAvailableDevices() {
        debugLog("[Discovery] === Device Diagnostic ===")

        // CoreMediaIO direct enumeration
        var deviceProperty = CMIOObjectPropertyAddress(
            mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyDevices),
            mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
            mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
        )
        var dataSize: UInt32 = 0
        CMIOObjectGetPropertyDataSize(
            CMIOObjectID(kCMIOObjectSystemObject),
            &deviceProperty, 0, nil, &dataSize
        )
        let cmioDeviceCount = Int(dataSize) / MemoryLayout<CMIOObjectID>.size
        debugLog("[Discovery] CoreMediaIO device count: \(cmioDeviceCount)")

        // AVFoundation enumeration with all device types
        var allTypes: [AVCaptureDevice.DeviceType] = [.builtInWideAngleCamera]
        if #available(macOS 14.0, *) {
            allTypes.append(.external)
        } else {
            allTypes.append(.externalUnknown)
        }

        let allSession = AVCaptureDevice.DiscoverySession(
            deviceTypes: allTypes,
            mediaType: nil,
            position: .unspecified
        )
        for device in allSession.devices {
            debugLog("[Discovery]   AVF Device: \(device.localizedName) | type=\(device.deviceType.rawValue) | model=\(device.modelID) | video=\(device.hasMediaType(.video)) | muxed=\(device.hasMediaType(.muxed))")
        }

        logger.info("=== End Diagnostic ===")
    }

    // MARK: - CoreMediaIO

    private func enableIOSDeviceDiscovery() {
        var property = CMIOObjectPropertyAddress(
            mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyAllowScreenCaptureDevices),
            mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
            mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
        )
        var allow: UInt32 = 1
        let size = UInt32(MemoryLayout<UInt32>.size)
        let status = CMIOObjectSetPropertyData(
            CMIOObjectID(kCMIOObjectSystemObject),
            &property,
            0, nil,
            size,
            &allow
        )
        if status == noErr {
            logger.info("CoreMediaIO screen capture devices enabled.")
        } else {
            logger.error("Failed to enable CoreMediaIO screen capture devices: \(status)")
        }
    }
}
