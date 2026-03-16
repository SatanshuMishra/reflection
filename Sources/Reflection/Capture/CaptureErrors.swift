import Foundation

public enum CaptureError: Error, Sendable, Equatable, LocalizedError {
    case deviceNotFound
    case permissionDenied
    case permissionNotDetermined
    case sessionConfigurationFailed(String)
    case captureInterrupted(reason: String)
    case deviceDisconnected
    case unknownError(String)

    public var errorDescription: String? {
        switch self {
        case .deviceNotFound:
            "No iPad found. Connect your iPad via USB cable."
        case .permissionDenied:
            "Camera access denied. Open System Settings > Privacy > Camera to allow Reflection."
        case .permissionNotDetermined:
            "Camera permission has not been granted yet."
        case .sessionConfigurationFailed(let detail):
            "Failed to configure capture: \(detail)"
        case .captureInterrupted(let reason):
            "Capture was interrupted: \(reason)"
        case .deviceDisconnected:
            "iPad was disconnected."
        case .unknownError(let detail):
            "An unexpected error occurred: \(detail)"
        }
    }
}
