import Foundation
@testable import Reflection

final class MockLoginItemService: LoginItemService, @unchecked Sendable {
    private let lock = NSLock()

    private var _isEnabled: Bool = false
    private var _enableCallCount = 0
    private var _disableCallCount = 0
    private var _shouldThrow = false

    var isEnabled: Bool {
        get {
            lock.lock()
            defer { lock.unlock() }
            return _isEnabled
        }
        set {
            lock.lock()
            _isEnabled = newValue
            lock.unlock()
        }
    }

    var enableCallCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return _enableCallCount
    }

    var disableCallCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return _disableCallCount
    }

    var shouldThrow: Bool {
        get {
            lock.lock()
            defer { lock.unlock() }
            return _shouldThrow
        }
        set {
            lock.lock()
            _shouldThrow = newValue
            lock.unlock()
        }
    }

    struct MockError: Error, LocalizedError {
        var errorDescription: String? { "Mock login item service error" }
    }

    func enable() throws {
        lock.lock()
        _enableCallCount += 1
        let shouldFail = _shouldThrow
        if !shouldFail { _isEnabled = true }
        lock.unlock()
        if shouldFail { throw MockError() }
    }

    func disable() throws {
        lock.lock()
        _disableCallCount += 1
        let shouldFail = _shouldThrow
        if !shouldFail { _isEnabled = false }
        lock.unlock()
        if shouldFail { throw MockError() }
    }
}
