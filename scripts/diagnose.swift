import AVFoundation
import CoreMediaIO

// Enable access to iOS devices via CoreMediaIO
// This property must be set BEFORE discovery for iPads to appear
var prop = CMIOObjectPropertyAddress(
    mSelector: CMIOObjectPropertySelector(kCMIOHardwarePropertyAllowScreenCaptureDevices),
    mScope: CMIOObjectPropertyScope(kCMIOObjectPropertyScopeGlobal),
    mElement: CMIOObjectPropertyElement(kCMIOObjectPropertyElementMain)
)
var allow: UInt32 = 1
let sizeOfAllow = UInt32(MemoryLayout<UInt32>.size)
CMIOObjectSetPropertyData(
    CMIOObjectID(kCMIOObjectSystemObject),
    &prop,
    0, nil,
    sizeOfAllow,
    &allow
)

print("=== Camera Authorization Status ===")
let status = AVCaptureDevice.authorizationStatus(for: .video)
switch status {
case .authorized: print("Status: AUTHORIZED")
case .denied: print("Status: DENIED")
case .restricted: print("Status: RESTRICTED")
case .notDetermined: print("Status: NOT DETERMINED")
@unknown default: print("Status: UNKNOWN")
}

print("\n=== All Video Devices ===")
let allDevices = AVCaptureDevice.DiscoverySession(
    deviceTypes: [.builtInWideAngleCamera, .externalUnknown],
    mediaType: .video,
    position: .unspecified
).devices
for device in allDevices {
    print("  Name: \(device.localizedName)")
    print("  ID:   \(device.uniqueID)")
    print("  Model: \(device.modelID)")
    print("  Connected: \(device.isConnected)")
    print("  Type: \(device.deviceType)")
    print("  ---")
}

if #available(macOS 14.0, *) {
    print("\n=== External Devices (macOS 14+ .external) ===")
    let externalDevices = AVCaptureDevice.DiscoverySession(
        deviceTypes: [.external],
        mediaType: .video,
        position: .unspecified
    ).devices
    for device in externalDevices {
        print("  Name: \(device.localizedName)")
        print("  ID:   \(device.uniqueID)")
        print("  Model: \(device.modelID)")
        print("  Connected: \(device.isConnected)")
        print("  ---")
    }
    if externalDevices.isEmpty {
        print("  (none found)")
    }
}

print("\n=== All Device Types (broad search) ===")
let broadDevices = AVCaptureDevice.DiscoverySession(
    deviceTypes: [.builtInWideAngleCamera, .externalUnknown, .builtInMicrophone],
    mediaType: nil,  // any media type
    position: .unspecified
).devices
for device in broadDevices {
    print("  Name: \(device.localizedName) | Type: \(device.deviceType) | Media: \(device.hasMediaType(.video) ? "video" : "other")")
}
if broadDevices.isEmpty {
    print("  (none found)")
}

print("\nTotal devices found: \(allDevices.count) (video), \(broadDevices.count) (broad)")
