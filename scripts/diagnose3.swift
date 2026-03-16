#!/usr/bin/env swift

import AVFoundation
import CoreMediaIO
import Foundation

print("=== Reflection Device Diagnostic ===")
print("macOS: \(ProcessInfo.processInfo.operatingSystemVersionString)")
print()

// Step 1: Enable CoreMediaIO screen capture devices
print("[1] Setting kCMIOHardwarePropertyAllowScreenCaptureDevices...")
var property = CMIOObjectPropertyAddress(
    mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyAllowScreenCaptureDevices),
    mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
    mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
)
var allow: UInt32 = 1
let status = CMIOObjectSetPropertyData(
    CMIOObjectID(kCMIOObjectSystemObject),
    &property,
    0, nil,
    UInt32(MemoryLayout<UInt32>.size),
    &allow
)
print("    Status: \(status == noErr ? "SUCCESS" : "FAILED (\(status))")")

// Step 2: Check camera authorization
print()
print("[2] Camera authorization status...")
let authStatus = AVCaptureDevice.authorizationStatus(for: .video)
switch authStatus {
case .authorized: print("    Status: AUTHORIZED")
case .notDetermined: print("    Status: NOT DETERMINED")
case .denied: print("    Status: DENIED")
case .restricted: print("    Status: RESTRICTED")
@unknown default: print("    Status: UNKNOWN")
}

// Step 3: Wait for CoreMediaIO to register devices
print()
print("[3] Waiting 3 seconds for CoreMediaIO device registration...")
Thread.sleep(forTimeInterval: 3.0)

// Step 4: Enumerate CoreMediaIO devices directly
print()
print("[4] CoreMediaIO direct enumeration...")
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
print("    Device count: \(cmioDeviceCount)")

if cmioDeviceCount > 0 {
    var devices = [CMIOObjectID](repeating: 0, count: cmioDeviceCount)
    var actualSize: UInt32 = 0
    CMIOObjectGetPropertyData(
        CMIOObjectID(kCMIOObjectSystemObject),
        &deviceProperty, 0, nil,
        dataSize, &actualSize,
        &devices
    )

    for (i, deviceID) in devices.enumerated() {
        // Get device name
        var nameProperty = CMIOObjectPropertyAddress(
            mSelector: CMIOObjectPropertySelector(kCMIOObjectPropertyName),
            mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
            mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
        )
        var nameRef: CFString = "" as CFString
        var nameSize: UInt32 = UInt32(MemoryLayout<CFString>.size)
        CMIOObjectGetPropertyData(deviceID, &nameProperty, 0, nil, nameSize, &nameSize, &nameRef)
        print("    [\(i)] ID=\(deviceID) Name=\(nameRef)")
    }
}

// Step 5: AVFoundation discovery with various configurations
print()
print("[5] AVFoundation device discovery...")

let deviceTypes: [AVCaptureDevice.DeviceType]
if #available(macOS 14.0, *) {
    deviceTypes = [.external, .builtInWideAngleCamera]
} else {
    deviceTypes = [.externalUnknown, .builtInWideAngleCamera]
}

let mediaTypes: [AVMediaType?] = [.video, .muxed, nil]
for mediaType in mediaTypes {
    let session = AVCaptureDevice.DiscoverySession(
        deviceTypes: deviceTypes,
        mediaType: mediaType,
        position: .unspecified
    )
    print("    mediaType=\(mediaType?.rawValue ?? "nil"): \(session.devices.count) device(s)")
    for device in session.devices {
        print("      - \(device.localizedName) | uniqueID=\(device.uniqueID) | model=\(device.modelID) | type=\(device.deviceType.rawValue) | video=\(device.hasMediaType(.video)) | muxed=\(device.hasMediaType(.muxed))")
    }
}

// Step 6: Try all AVCaptureDevice default devices
print()
print("[6] Default devices...")
if let defaultVideo = AVCaptureDevice.default(for: .video) {
    print("    Default video: \(defaultVideo.localizedName)")
} else {
    print("    Default video: NONE")
}
if let defaultMuxed = AVCaptureDevice.default(for: .muxed) {
    print("    Default muxed: \(defaultMuxed.localizedName)")
} else {
    print("    Default muxed: NONE")
}

// Step 7: List ALL devices via class method
print()
print("[7] AVCaptureDevice.devices() (deprecated but comprehensive)...")
let allDevices = AVCaptureDevice.devices()
print("    Total: \(allDevices.count)")
for device in allDevices {
    print("      - \(device.localizedName) | model=\(device.modelID) | type=\(device.deviceType.rawValue)")
}

print()
print("=== Diagnostic Complete ===")
