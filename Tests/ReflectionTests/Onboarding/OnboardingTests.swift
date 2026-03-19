import XCTest
@testable import Reflection

@MainActor
final class OnboardingTests: XCTestCase {

    private func makeIsolatedDefaults() -> (UserDefaults, String) {
        let suiteName = "test.\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: suiteName)!
        return (defaults, suiteName)
    }

    // MARK: - Onboarding Completion Flag

    func testOnboardingKeyDefaultsToFalse() {
        let (defaults, suite) = makeIsolatedDefaults()
        defer { UserDefaults.standard.removeSuite(named: suite) }

        let completed = defaults.bool(forKey: Constants.onboardingCompletedKey)
        XCTAssertFalse(completed, "Onboarding should not be completed by default")
    }

    func testOnboardingKeyPersistsWhenSetTrue() {
        let (defaults, suite) = makeIsolatedDefaults()
        defer { UserDefaults.standard.removeSuite(named: suite) }

        defaults.set(true, forKey: Constants.onboardingCompletedKey)
        XCTAssertTrue(defaults.bool(forKey: Constants.onboardingCompletedKey))
    }

    // MARK: - Constants

    func testOnboardingCompletedKeyHasExpectedValue() {
        XCTAssertEqual(Constants.onboardingCompletedKey, "com.reflection.onboardingCompleted")
    }

    func testMainWindowIDHasExpectedValue() {
        XCTAssertEqual(Constants.mainWindowID, "main")
    }

    func testOnboardingWindowIDHasExpectedValue() {
        XCTAssertEqual(Constants.onboardingWindowID, "onboarding")
    }

    func testAppBackgroundColorIsCorrect() {
        // #121215 = RGB(18, 18, 21)
        let nsColor = Constants.appBackgroundNS
        XCTAssertEqual(nsColor.redComponent, 18 / 255, accuracy: 0.001)
        XCTAssertEqual(nsColor.greenComponent, 18 / 255, accuracy: 0.001)
        XCTAssertEqual(nsColor.blueComponent, 21 / 255, accuracy: 0.001)
    }

    // MARK: - Onboarding Window Size Constants

    func testOnboardingPageSizesHasTwoEntries() {
        XCTAssertEqual(Constants.onboardingPageSizes.count, 2)
    }

    func testOnboardingPageSizesHavePositiveDimensions() {
        for (index, size) in Constants.onboardingPageSizes.enumerated() {
            XCTAssertGreaterThan(size.width, 0, "Page \(index) width must be positive")
            XCTAssertGreaterThan(size.height, 0, "Page \(index) height must be positive")
        }
    }

    func testOnboardingCornerRadiusIsTen() {
        XCTAssertEqual(
            Constants.onboardingCornerRadius, 10,
            "Onboarding window corner radius must be 10"
        )
    }

    func testOnboardingVerticalPaddingIsTwentyFour() {
        XCTAssertEqual(
            Constants.onboardingVerticalPadding, 24,
            "Onboarding vertical padding must be 24"
        )
    }

    func testOnboardingPillWidthIsWiderThanDot() {
        XCTAssertGreaterThan(
            Constants.onboardingPillWidth,
            Constants.onboardingDotSize,
            "Pill must be wider than inactive dot"
        )
    }

}
