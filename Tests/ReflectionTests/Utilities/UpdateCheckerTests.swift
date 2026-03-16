import XCTest
@testable import Reflection

// MARK: - Mock URLDataProvider

private final class MockURLDataProvider: URLDataProvider, @unchecked Sendable {
    var mockData: Data?
    var mockError: Error?

    func data(from url: URL) async throws -> (Data, URLResponse) {
        if let error = mockError {
            throw error
        }
        let response = HTTPURLResponse(
            url: url,
            statusCode: 200,
            httpVersion: nil,
            headerFields: nil
        )!
        return (mockData ?? Data(), response)
    }
}

// MARK: - Tests

@MainActor
final class UpdateCheckerTests: XCTestCase {

    // MARK: - Version comparison

    func testNewerVersionDetected() {
        let checker = UpdateChecker(currentVersion: "1.0.0")
        XCTAssertTrue(checker.isVersion("1.1.0", newerThan: "1.0.0"))
    }

    func testSameVersionNotNewer() {
        let checker = UpdateChecker(currentVersion: "1.0.0")
        XCTAssertFalse(checker.isVersion("1.0.0", newerThan: "1.0.0"))
    }

    func testOlderVersionNotNewer() {
        let checker = UpdateChecker(currentVersion: "1.0.0")
        XCTAssertFalse(checker.isVersion("0.9.0", newerThan: "1.0.0"))
    }

    func testMajorVersionBump() {
        let checker = UpdateChecker(currentVersion: "1.0.0")
        XCTAssertTrue(checker.isVersion("2.0.0", newerThan: "1.0.0"))
    }

    func testPatchVersionBump() {
        let checker = UpdateChecker(currentVersion: "1.0.0")
        XCTAssertTrue(checker.isVersion("1.0.1", newerThan: "1.0.0"))
    }

    func testDifferentLengthVersions() {
        let checker = UpdateChecker(currentVersion: "1.0")
        XCTAssertTrue(checker.isVersion("1.0.1", newerThan: "1.0"))
    }

    // MARK: - checkForUpdate

    func testCheckForUpdateReturnsUpdateInfo() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v1.1.0", url: "https://github.com/test/releases/v1.1.0", body: "Bug fixes")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertNotNil(result)
        XCTAssertEqual(result?.latestVersion, "1.1.0")
        XCTAssertEqual(result?.currentVersion, "1.0.0")
        XCTAssertEqual(result?.releaseNotes, "Bug fixes")
    }

    func testCheckForUpdateReturnsNilWhenUpToDate() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v1.0.0", url: "https://github.com/test/releases/v1.0.0", body: "")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertNil(result)
    }

    func testCheckForUpdateReturnsNilWhenRemoteOlder() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v0.9.0", url: "https://github.com/test/releases/v0.9.0", body: "")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertNil(result)
    }

    func testCheckForUpdateReturnsNilOnNetworkError() async {
        let mock = MockURLDataProvider()
        mock.mockError = URLError(.notConnectedToInternet)
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertNil(result)
    }

    func testCheckForUpdateReturnsNilOnMalformedJSON() async {
        let mock = MockURLDataProvider()
        mock.mockData = "not json".data(using: .utf8)
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertNil(result)
    }

    func testCheckForUpdateStripsVersionPrefix() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v2.0.0", url: "https://github.com/test/releases/v2.0.0", body: "Major update")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let result = await checker.checkForUpdate()

        XCTAssertEqual(result?.latestVersion, "2.0.0")
    }

    // MARK: - What's New tracking

    func testIsFirstLaunchWhenNoLastSeenVersion() {
        UserDefaults.standard.removeObject(forKey: "lastSeenVersion")
        let checker = UpdateChecker(currentVersion: "1.0.0")

        XCTAssertTrue(checker.isFirstLaunchOfNewVersion)
    }

    func testIsNotFirstLaunchWhenVersionMatches() {
        UserDefaults.standard.set("1.0.0", forKey: "lastSeenVersion")
        let checker = UpdateChecker(currentVersion: "1.0.0")

        XCTAssertFalse(checker.isFirstLaunchOfNewVersion)
    }

    func testIsFirstLaunchWhenVersionDiffers() {
        UserDefaults.standard.set("0.9.0", forKey: "lastSeenVersion")
        let checker = UpdateChecker(currentVersion: "1.0.0")

        XCTAssertTrue(checker.isFirstLaunchOfNewVersion)
    }

    // MARK: - fetchReleaseNotes

    func testFetchReleaseNotesReturnsBody() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v1.0.0", url: "https://github.com/test", body: "## Changes\n- Added feature X")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let notes = await checker.fetchReleaseNotes(for: "1.0.0")

        XCTAssertEqual(notes, "## Changes\n- Added feature X")
    }

    func testFetchReleaseNotesReturnsNilOnError() async {
        let mock = MockURLDataProvider()
        mock.mockError = URLError(.timedOut)
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let notes = await checker.fetchReleaseNotes(for: "1.0.0")

        XCTAssertNil(notes)
    }

    func testFetchReleaseNotesReturnsNilForEmptyBody() async {
        let mock = MockURLDataProvider()
        mock.mockData = makeReleaseJSON(tag: "v1.0.0", url: "https://github.com/test", body: "")
        let checker = UpdateChecker(dataProvider: mock, currentVersion: "1.0.0")

        let notes = await checker.fetchReleaseNotes(for: "1.0.0")

        XCTAssertNil(notes)
    }

    // MARK: - Cleanup

    override func tearDown() {
        super.tearDown()
        UserDefaults.standard.removeObject(forKey: "lastSeenVersion")
    }

    // MARK: - Helpers

    private func makeReleaseJSON(tag: String, url: String, body: String) -> Data {
        let json: [String: Any] = [
            "tag_name": tag,
            "html_url": url,
            "body": body
        ]
        return try! JSONSerialization.data(withJSONObject: json)
    }
}
