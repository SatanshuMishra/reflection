import XCTest
import SwiftUI
@testable import Reflection

@MainActor
final class AppSettingsTests: XCTestCase {

    private func makeSettings(
        defaults: UserDefaults? = nil,
        loginItemService: MockLoginItemService? = nil
    ) -> (AppSettings, UserDefaults, MockLoginItemService) {
        let suite = defaults ?? UserDefaults(suiteName: "test.\(UUID().uuidString)")!
        let mock = loginItemService ?? MockLoginItemService()
        let settings = AppSettings(defaults: suite, loginItemService: mock)
        return (settings, suite, mock)
    }

    // MARK: - Defaults

    func testDefaultAppearanceIsSystem() {
        let (settings, _, _) = makeSettings()
        XCTAssertEqual(settings.appearance, .system)
    }

    func testDefaultRunInBackgroundIsFalse() {
        let (settings, _, _) = makeSettings()
        XCTAssertFalse(settings.runInBackground)
    }

    func testDefaultLaunchAtLoginIsFalse() {
        let (settings, _, _) = makeSettings()
        XCTAssertFalse(settings.launchAtLogin)
    }

    // MARK: - Persistence Roundtrips

    func testAppearanceRoundtrips() {
        let suite = UserDefaults(suiteName: "test.\(UUID().uuidString)")!
        let (settings, _, _) = makeSettings(defaults: suite)
        settings.appearance = .dark

        // Create a new instance reading from the same suite
        let (settings2, _, _) = makeSettings(defaults: suite)
        XCTAssertEqual(settings2.appearance, .dark)
    }

    func testRunInBackgroundRoundtrips() {
        let suite = UserDefaults(suiteName: "test.\(UUID().uuidString)")!
        let (settings, _, _) = makeSettings(defaults: suite)
        settings.runInBackground = true

        let (settings2, _, _) = makeSettings(defaults: suite)
        XCTAssertTrue(settings2.runInBackground)
    }

    func testLaunchAtLoginRoundtrips() {
        let suite = UserDefaults(suiteName: "test.\(UUID().uuidString)")!
        let mock = MockLoginItemService()
        let (settings, _, _) = makeSettings(defaults: suite, loginItemService: mock)
        settings.launchAtLogin = true

        let mock2 = MockLoginItemService()
        mock2.isEnabled = true
        let (settings2, _, _) = makeSettings(defaults: suite, loginItemService: mock2)
        XCTAssertTrue(settings2.launchAtLogin)
    }

    // MARK: - LoginItemService Integration

    func testLaunchAtLoginCallsServiceEnable() {
        let (settings, _, mock) = makeSettings()
        settings.launchAtLogin = true
        XCTAssertEqual(mock.enableCallCount, 1)
        XCTAssertEqual(mock.disableCallCount, 0)
    }

    func testLaunchAtLoginCallsServiceDisable() {
        let (settings, _, mock) = makeSettings()
        settings.launchAtLogin = true
        settings.launchAtLogin = false
        XCTAssertEqual(mock.disableCallCount, 1)
    }

    func testServiceErrorRevertsLaunchAtLogin() {
        let (settings, _, mock) = makeSettings()
        mock.shouldThrow = true
        settings.launchAtLogin = true
        // Should revert to false after error
        XCTAssertFalse(settings.launchAtLogin)
    }

    // MARK: - AppearanceMode.nsAppearance

    func testNsAppearanceNilForSystem() {
        XCTAssertNil(AppearanceMode.system.nsAppearance)
    }

    func testNsAppearanceDarkForDark() {
        XCTAssertEqual(AppearanceMode.dark.nsAppearance?.name, .darkAqua)
    }

    func testNsAppearanceLightForLight() {
        XCTAssertEqual(AppearanceMode.light.nsAppearance?.name, .aqua)
    }

    // MARK: - AppearanceMode Conformances

    func testAppearanceModeIsCaseIterable() {
        XCTAssertEqual(AppearanceMode.allCases.count, 3)
    }

    func testAppearanceModeRawValues() {
        XCTAssertEqual(AppearanceMode.system.rawValue, "system")
        XCTAssertEqual(AppearanceMode.light.rawValue, "light")
        XCTAssertEqual(AppearanceMode.dark.rawValue, "dark")
    }
}
