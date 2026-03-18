import ServiceManagement

/// Abstraction over login item registration for testability.
protocol LoginItemService: Sendable {
    var isEnabled: Bool { get }
    func enable() throws
    func disable() throws
}

/// Concrete implementation using Apple's SMAppService (macOS 13+).
/// No helper app or entitlements required. The login item appears
/// in System Settings > General > Login Items.
struct SMLoginItemService: LoginItemService {
    var isEnabled: Bool {
        SMAppService.mainApp.status == .enabled
    }

    func enable() throws {
        try SMAppService.mainApp.register()
    }

    func disable() throws {
        try SMAppService.mainApp.unregister()
    }
}
