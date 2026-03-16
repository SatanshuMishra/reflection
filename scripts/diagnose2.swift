import AVFoundation
import CoreMediaIO
import Foundation

// STEP 1: Enable iOS device discovery via CoreMediaIO
print("=== Step 1: Enable CoreMediaIO screen capture devices ===")
var property = CMIOObjectPropertyAddress(
    mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyAllowScreenCaptureDevices),
    mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
    mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
)
var allow: UInt32 = 1
let size = UInt32(MemoryLayout<UInt32>.size)
let status = CMIOObjectSetPropertyData(
    CMIOObjectID(kCMIOObjectSystemObject),
    &property, 0, nil, size, &allow
)
print("  CMIOObjectSetPropertyData result: \(status) (\(status == noErr ? "OK" : "FAILED"))")

// STEP 2: Request camera access
print("\n=== Step 2: Camera authorization ===")
let authStatus = AVCaptureDevice.authorizationStatus(for: .video)
print("  Current status: \(authStatus.rawValue) (0=notDetermined, 1=restricted, 2=denied, 3=authorized)")

if authStatus == .notDetermined {
    print("  Requesting access...")
    let semaphore = DispatchSemaphore(value: 0)
    AVCaptureDevice.requestAccess(for: .video) { granted in
        print("  Access granted: \(granted)")
        semaphore.signal()
    }
    semaphore.wait()
} else if authStatus == .authorized {
    print("  Already authorized.")
} else {
    print("  Access denied/restricted!")
}

// STEP 3: Wait for CoreMediaIO to register devices
print("\n=== Step 3: Waiting 3 seconds for device registration ===")
Thread.sleep(forTimeInterval: 3.0)

// STEP 4: Discover devices with multiple device types
print("\n=== Step 4: Device discovery ===")

print("\n  --- .external devices (video) ---")
if #available(macOS 14.0, *) {
    let session = AVCaptureDevice.DiscoverySession(
        deviceTypes: [.external],
        mediaType: .video,
        position: .unspecified
    )
    for d in session.devices {
        print("    \(d.localizedName) | model=\(d.modelID) | id=\(d.uniqueID) | connected=\(d.isConnected)")
    }
    if session.devices.isEmpty { print("    (none)") }
}

print("\n  --- .external devices (muxed/nil media type) ---")
if #available(macOS 14.0, *) {
    let session = AVCaptureDevice.DiscoverySession(
        deviceTypes: [.external],
        mediaType: .muxed,
        position: .unspecified
    )
    for d in session.devices {
        print("    \(d.localizedName) | model=\(d.modelID) | id=\(d.uniqueID) | hasVideo=\(d.hasMediaType(.video)) | hasMuxed=\(d.hasMediaType(.muxed))")
    }
    if session.devices.isEmpty { print("    (none)") }
}

print("\n  --- .external devices (no media filter) ---")
if #available(macOS 14.0, *) {
    let session = AVCaptureDevice.DiscoverySession(
        deviceTypes: [.external],
        mediaType: nil,
        position: .unspecified
    )
    for d in session.devices {
        print("    \(d.localizedName) | model=\(d.modelID) | type=\(d.deviceType) | video=\(d.hasMediaType(.video)) | muxed=\(d.hasMediaType(.muxed))")
    }
    if session.devices.isEmpty { print("    (none)") }
}

// STEP 5: Try CoreMediaIO direct device enumeration
print("\n=== Step 5: CoreMediaIO direct enumeration ===")
var deviceProperty = CMIOObjectPropertyAddress(
    mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyDevices),
    mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
    mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
)
var dataSize: UInt32 = 0
var dataUsed: UInt32 = 0
CMIOObjectGetPropertyDataSize(CMIOObjectID(kCMIOObjectSystemObject), &deviceProperty, 0, nil, &dataSize)
let deviceCount = Int(dataSize) / MemoryLayout<CMIOObjectID>.size
print("  CoreMediaIO reports \(deviceCount) device(s)")

if deviceCount > 0 {
    var devices = [CMIOObjectID](repeating: 0, count: deviceCount)
    CMIOObjectGetPropertyData(CMIOObjectID(kCMIOObjectSystemObject), &deviceProperty, 0, nil, dataSize, &dataUsed, &devices)

    for deviceID in devices {
        // Get device name
        var nameProperty = CMIOObjectPropertyAddress(
            mSelector: CMIOObjectPropertySelector(kCMIOObjectPropertyName),
            mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
            mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
        )
        var nameSize: UInt32 = UInt32(MemoryLayout<CFString>.size)
        var name: CFString = "" as CFString
        let nameStatus = CMIOObjectGetPropertyData(deviceID, &nameProperty, 0, nil, nameSize, &dataUsed, &name)
        if nameStatus == noErr {
            print("    Device \(deviceID): \(name as String)")
        } else {
            print("    Device \(deviceID): (name unavailable, status=\(nameStatus))")
        }
    }
}

print("\n=== Step 6: Check AVCaptureDevice.default ===")
if let defaultDevice = AVCaptureDevice.default(for: .video) {
    print("  Default video device: \(defaultDevice.localizedName)")
} else {
    print("  No default video device")
}

print("\nDone.")
