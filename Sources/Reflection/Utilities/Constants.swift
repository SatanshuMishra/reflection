import Foundation
import SwiftUI

enum Constants {
    /// App background — near-black (#0d0f11)
    static let appBackground = Color(red: 13 / 255, green: 15 / 255, blue: 17 / 255)
    static let appBackgroundNS = NSColor(red: 13 / 255, green: 15 / 255, blue: 17 / 255, alpha: 1)
    static let defaultFrameRate: Double = 60.0
    static let windowMinWidth: CGFloat = 320
    static let windowMinHeight: CGFloat = 240
    static let defaultWindowWidth: CGFloat = 1024
    static let defaultWindowHeight: CGFloat = 768
    static let defaultAspectRatio = CGSize(width: 4, height: 3)
    static let animationDuration: TimeInterval = 0.3
    static let frameStaleThreshold: TimeInterval = 1.0
    static let frameCheckIntervalNanos: UInt64 = 500_000_000

    // MARK: - Settings Keys

    static let appearanceKey = "com.reflection.appearance"
    static let runInBackgroundKey = "com.reflection.runInBackground"
    static let launchAtLoginKey = "com.reflection.launchAtLogin"
}
