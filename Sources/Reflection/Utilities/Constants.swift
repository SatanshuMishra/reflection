import Foundation
import SwiftUI

enum Constants {
    // MARK: - Window Identifiers

    static let mainWindowID = "main"
    static let onboardingWindowID = "onboarding"

    /// App background — near-black (#121215)
    static let appBackground = Color(red: 18 / 255, green: 18 / 255, blue: 21 / 255)
    static let appBackgroundNS = NSColor(red: 18 / 255, green: 18 / 255, blue: 21 / 255, alpha: 1)
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
    static let onboardingCompletedKey = "com.reflection.onboardingCompleted"

    // MARK: - Onboarding Window Sizes

    static let onboardingWelcomeSize = CGSize(width: 800, height: 600)
    static let onboardingPermissionSize = CGSize(width: 800, height: 600)
    static let onboardingPageSizes: [CGSize] = [
        onboardingWelcomeSize, onboardingPermissionSize,
    ]

    /// Corner radius for the chromeless onboarding window.
    static let onboardingCornerRadius: CGFloat = 10

    /// Vertical padding inside the onboarding window (top and bottom).
    static let onboardingVerticalPadding: CGFloat = 24

    /// Width of the active pill in the step indicator.
    static let onboardingPillWidth: CGFloat = 36

    /// Size of inactive dots in the step indicator.
    static let onboardingDotSize: CGFloat = 8

    /// Main app default window size (post-onboarding).
    static let mainWindowSize = CGSize(width: 400, height: 300)
}
