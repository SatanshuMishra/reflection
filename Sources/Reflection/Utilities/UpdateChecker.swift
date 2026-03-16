import Foundation
import os.log

// MARK: - Types

struct UpdateInfo: Sendable {
    let latestVersion: String
    let currentVersion: String
    let releaseURL: URL
    let releaseNotes: String
}

// MARK: - URLDataProvider protocol (testability)

protocol URLDataProvider: Sendable {
    func data(from url: URL) async throws -> (Data, URLResponse)
}

extension URLSession: URLDataProvider {}

// MARK: - UpdateChecker

@MainActor
final class UpdateChecker {
    private static let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "com.reflection.app",
        category: "UpdateChecker"
    )

    private static let repoOwner = "SatanshuMishra"
    private static let repoName = "reflection"
    private static let lastSeenVersionKey = "lastSeenVersion"

    private let dataProvider: URLDataProvider
    let currentVersion: String

    init(
        dataProvider: URLDataProvider = URLSession.shared,
        currentVersion: String? = nil
    ) {
        self.dataProvider = dataProvider
        self.currentVersion = currentVersion
            ?? Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String
            ?? "0.0.0"
    }

    // MARK: - Public API

    /// Checks if a newer version is available on GitHub Releases.
    /// Returns nil if current version is up-to-date, or on any error.
    func checkForUpdate() async -> UpdateInfo? {
        let urlString = "https://api.github.com/repos/\(Self.repoOwner)/\(Self.repoName)/releases/latest"
        guard let url = URL(string: urlString) else {
            Self.logger.error("Invalid GitHub API URL")
            return nil
        }

        do {
            let (data, _) = try await dataProvider.data(from: url)
            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let tagName = json["tag_name"] as? String,
                  let htmlURL = json["html_url"] as? String,
                  let releaseURL = URL(string: htmlURL) else {
                Self.logger.warning("Failed to parse GitHub release response")
                return nil
            }

            let remoteVersion = stripVersionPrefix(tagName)
            let releaseNotes = json["body"] as? String ?? ""

            guard isVersion(remoteVersion, newerThan: currentVersion) else {
                Self.logger.info("App is up to date (current: \(self.currentVersion), latest: \(remoteVersion))")
                return nil
            }

            Self.logger.info("Update available: \(remoteVersion) (current: \(self.currentVersion))")
            return UpdateInfo(
                latestVersion: remoteVersion,
                currentVersion: currentVersion,
                releaseURL: releaseURL,
                releaseNotes: releaseNotes
            )
        } catch {
            Self.logger.error("Failed to check for updates: \(error.localizedDescription)")
            return nil
        }
    }

    /// Fetches release notes for a specific version tag from GitHub.
    func fetchReleaseNotes(for version: String) async -> String? {
        let tag = version.hasPrefix("v") ? version : "v\(version)"
        let urlString = "https://api.github.com/repos/\(Self.repoOwner)/\(Self.repoName)/releases/tags/\(tag)"
        guard let url = URL(string: urlString) else { return nil }

        do {
            let (data, _) = try await dataProvider.data(from: url)
            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let body = json["body"] as? String,
                  !body.isEmpty else {
                return nil
            }
            return body
        } catch {
            Self.logger.error("Failed to fetch release notes for \(tag): \(error.localizedDescription)")
            return nil
        }
    }

    // MARK: - What's New tracking

    static var lastSeenVersion: String? {
        get { UserDefaults.standard.string(forKey: lastSeenVersionKey) }
        set { UserDefaults.standard.set(newValue, forKey: lastSeenVersionKey) }
    }

    var isFirstLaunchOfNewVersion: Bool {
        guard let lastSeen = Self.lastSeenVersion else {
            return true // First ever launch
        }
        return lastSeen != currentVersion
    }

    // MARK: - Version comparison

    /// Returns true if `version` is strictly newer than `other`.
    func isVersion(_ version: String, newerThan other: String) -> Bool {
        let lhs = parseVersion(version)
        let rhs = parseVersion(other)

        for i in 0..<max(lhs.count, rhs.count) {
            let l = i < lhs.count ? lhs[i] : 0
            let r = i < rhs.count ? rhs[i] : 0
            if l > r { return true }
            if l < r { return false }
        }
        return false // equal
    }

    // MARK: - Private helpers

    private func stripVersionPrefix(_ tag: String) -> String {
        tag.hasPrefix("v") ? String(tag.dropFirst()) : tag
    }

    private func parseVersion(_ version: String) -> [Int] {
        version.split(separator: ".").compactMap { Int($0) }
    }
}
