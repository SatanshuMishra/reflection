import Foundation

enum Constants {
    static let defaultFrameRate: Double = 60.0
    static let windowMinWidth: CGFloat = 320
    static let windowMinHeight: CGFloat = 240
    static let defaultWindowWidth: CGFloat = 1024
    static let defaultWindowHeight: CGFloat = 768
    static let defaultAspectRatio = CGSize(width: 4, height: 3)
    static let animationDuration: TimeInterval = 0.3
    static let frameStaleThreshold: TimeInterval = 1.0
    static let frameCheckIntervalNanos: UInt64 = 500_000_000
}
