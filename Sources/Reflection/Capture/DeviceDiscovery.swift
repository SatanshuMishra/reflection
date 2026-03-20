import AVFoundation
import Combine
import CoreMediaIO
import Foundation
import os

/// iOS devices connected via USB report modelID "iOS Device" through CoreMediaIO.
private let iosDeviceModelID = "iOS Device"

private func isIOSDevice(_ device: AVCaptureDevice) -> Bool {
    device.modelID == iosDeviceModelID
}

@MainActor
public final class DeviceDiscovery: ObservableObject {
    @Published public private(set) var devices: [DeviceModel] = []
    @Published public private(set) var cameraAuthorized = false
    @Published public private(set) var isRefreshing = false

    private nonisolated(unsafe) var discoverySession: AVCaptureDevice.DiscoverySession?
    private nonisolated(unsafe) var observation: NSKeyValueObservation?
    private let logger = Logger.discovery

    public init() {}

    deinit {
        observation?.invalidate()
    }

    public func startDiscovery() async {
        enableIOSDeviceDiscovery()
        checkCameraAuthorizationStatus()

        guard cameraAuthorized else { return }

        // CoreMediaIO needs time to register iOS devices after the property is set.
        // Wait before creating the discovery session.
        try? await Task.sleep(nanoseconds: 3_000_000_000) // 3 seconds

        beginDeviceDiscovery()
    }

    public func refreshDevices() {
        guard cameraAuthorized else {
            logger.warning("Cannot refresh: camera not authorized.")
            return
        }
        isRefreshing = true
        observation?.invalidate()
        observation = nil
        discoverySession = nil
        beginDeviceDiscovery()
        logger.info("Manual device refresh completed. Found \(self.devices.count) device(s).")

        // Keep the spinner visible long enough for the user to notice
        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 600_000_000)
            self?.isRefreshing = false
        }
    }

    public func stopDiscovery() {
        observation?.invalidate()
        observation = nil
        discoverySession = nil
        devices = []
        logger.info("Device discovery stopped.")
    }

    // MARK: - Camera Permission

    /// Reads camera authorization status without triggering the system permission dialog.
    /// The dialog is only shown from PermissionPage when the user clicks "Grant Permission".
    private func checkCameraAuthorizationStatus() {
        let status = AVCaptureDevice.authorizationStatus(for: .video)
        switch status {
        case .authorized:
            cameraAuthorized = true
            logger.info("Camera already authorized.")
        case .notDetermined:
            cameraAuthorized = false
            logger.info("Camera permission not yet requested — skipping discovery.")
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
