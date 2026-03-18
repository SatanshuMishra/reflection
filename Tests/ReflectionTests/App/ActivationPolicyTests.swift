import XCTest
@testable import Reflection

@MainActor
final class ActivationPolicyTests: XCTestCase {

    private func makeSettings(runInBackground: Bool = false) -> AppSettings {
        let suite = UserDefaults(suiteName: "test.\(UUID().uuidString)")!
        let settings = AppSettings(defaults: suite, loginItemService: MockLoginItemService())
        settings.runInBackground = runInBackground
        return settings
    }

    // MARK: - shouldTerminateAfterLastWindowClosed

    func testShouldTerminateWhenRunInBackgroundDisabled() {
        let settings = makeSettings(runInBackground: false)
        let result = ActivationPolicyHelper.shouldTerminateAfterLastWindowClosed(settings: settings)
        XCTAssertTrue(result)
    }

    func testShouldNotTerminateWhenRunInBackgroundEnabled() {
        let settings = makeSettings(runInBackground: true)
        let result = ActivationPolicyHelper.shouldTerminateAfterLastWindowClosed(settings: settings)
        XCTAssertFalse(result)
    }

    // MARK: - Policy determination

    func testPolicyIsAccessoryWhenBackgroundModeAndNoWindows() {
        let policy = ActivationPolicyHelper.desiredPolicy(runInBackground: true, hasVisibleWindows: false)
        XCTAssertEqual(policy, .accessory)
    }

    func testPolicyIsRegularWhenBackgroundModeAndHasWindows() {
        let policy = ActivationPolicyHelper.desiredPolicy(runInBackground: true, hasVisibleWindows: true)
        XCTAssertEqual(policy, .regular)
    }

    func testPolicyIsRegularWhenNotInBackgroundMode() {
        let policy = ActivationPolicyHelper.desiredPolicy(runInBackground: false, hasVisibleWindows: false)
        XCTAssertEqual(policy, .regular)
    }

    func testPolicyIsRegularWhenNotInBackgroundModeWithWindows() {
        let policy = ActivationPolicyHelper.desiredPolicy(runInBackground: false, hasVisibleWindows: true)
        XCTAssertEqual(policy, .regular)
    }
}
